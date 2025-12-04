// brandon - refer to readme.md for explanation
// on threading, reader writer locking

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include "udp.h"

// initialise a reader writer lock for the shared client list (more in readme.md)
pthread_rwlock_t client_list_lock = PTHREAD_RWLOCK_INITIALIZER;
client_node *client_list_head = NULL;

// ===== HELPER FUNCTIONS =====
void add_client(struct sockaddr_in addr, char *name) {  
    pthread_rwlock_wrlock(&client_list_lock);   
    client_node *new_node = malloc(sizeof(client_node));   
    new_node->client_address = addr;
    new_node->client_name = strdup(name);
    new_node->mute_list = NULL;
    new_node->next = client_list_head;
    new_node->previous = NULL;
    if (client_list_head)
    {
        client_list_head->previous = new_node;
        client_list_head = new_node;
    }
    pthread_rwlock_unlock(&client_list_lock);
}

client_node* find_client_by_name(char *name) {
    pthread_rwlock_rdlock(&client_list_lock);  
    client_node *curr = client_list_head;
    while (curr) {  
        if (strcmp(curr->client_name, name) == 0) 
        {
            pthread_rwlock_unlock(&client_list_lock);
            return curr;
        }
        curr = curr->next;
    }
    
    pthread_rwlock_unlock(&client_list_lock);
    return NULL;  
}

client_node* find_client_by_addr(struct sockaddr_in addr) {
    pthread_rwlock_rdlock(&client_list_lock);   
    client_node *curr = client_list_head;
    while (curr) {
        if (curr->client_address.sin_addr.s_addr == addr.sin_addr.s_addr &&
            curr->client_address.sin_port == addr.sin_port) 
        {
            pthread_rwlock_unlock(&client_list_lock);
            return curr;
        }
        curr = curr->next;
    }
    pthread_rwlock_unlock(&client_list_lock);
    return NULL;
}

void remove_client_by_name(char *name) {  
    pthread_rwlock_wrlock(&client_list_lock);   
    client_node *curr = client_list_head;
    while (curr) {
        if (strcmp(curr->client_name, name) == 0) {
            if (curr->previous) { 
                curr->previous->next = curr->next;
            }
            else { 
                client_list_head = curr->next;
            }
            if (curr->next) {
                curr->next->previous = curr->previous;
            }
            
            mute_node *mute_curr = curr->mute_list;
            while (mute_curr) {
                mute_node *temp = mute_curr;
                mute_curr = mute_curr->next;
                free(temp);
            }
            free(curr->client_name);
            free(curr);
            pthread_rwlock_unlock(&client_list_lock);
            return;
        }
        curr = curr->next;
    }
    pthread_rwlock_unlock(&client_list_lock);
}

void broadcast_message(int sd, char *message, struct sockaddr_in *exclude_addr) {  
    pthread_rwlock_rdlock(&client_list_lock);   
    client_node *curr = client_list_head;
    while (curr) {
        if (exclude_addr == NULL || 
            (curr->client_address.sin_port != exclude_addr->sin_port ||
             curr->client_address.sin_addr.s_addr != exclude_addr->sin_addr.s_addr)) {
            udp_socket_write(sd, &curr->client_address, message, strlen(message) + 1);
        }
        curr = curr->next;
    }
    pthread_rwlock_unlock(&client_list_lock);
}

int is_muted(client_node *client, char *muted_name) {
    mute_node *curr = client->mute_list;
    while (curr) {
        if (strcmp(curr->username, muted_name) == 0) {
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

void add_mute(client_node *client, char *muted_name) {
    if (is_muted(client, muted_name)) return;
    mute_node *new_mute = malloc(sizeof(mute_node));
    strcpy(new_mute->username, muted_name);
    new_mute->next = client->mute_list;
    client->mute_list = new_mute;
}

void remove_mute(client_node *client, char *muted_name) {
    mute_node *curr = client->mute_list;
    mute_node *prev = NULL;
    while (curr) {
        if (strcmp(curr->username, muted_name) == 0) {
            if (prev) prev->next = curr->next;
            else client->mute_list = curr->next;
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

// ===== HANDLE REQUESTS =====
typedef struct{
    int sd;
    struct sockaddr_in client_addr;
    char request_content[BUFFER_SIZE];
} request_args;

void* handle_conn_request(void *arg)
{
    request_args *args = (request_args *)arg;
    int sd = args->sd;
    struct sockaddr_in client_addr = args->client_addr;
    char *client_name = args->request_content;
    char response[BUFFER_SIZE];

    add_client(client_addr, client_name);
    snprintf(response, BUFFER_SIZE, "Hi %s, you have successfully connected to the chat\n", client_name);
    udp_socket_write(sd, &client_addr, response, strlen(response) + 1);

    free(args);
    return NULL;
}

void* handle_say_request(void *arg)
{
    request_args *args = (request_args *)arg;
    int sd = args->sd;
    struct sockaddr_in client_addr = args->client_addr;
    char *message = args->request_content;
    char broadcast_msg[BUFFER_SIZE];

    client_node *sender = find_client_by_addr(client_addr);
    if (sender) {
        snprintf(broadcast_msg, BUFFER_SIZE, "%s: %s\n", sender->client_name, message);
        broadcast_message(sd, broadcast_msg, NULL);
    }

    free(args);
    return NULL;
}

void* handle_sayto_request(void *arg) {
    request_args *args = (request_args *)arg;
    int sd = args->sd;
    struct sockaddr_in client_addr = args->client_addr;
    char *content = args->request_content;
    char recipient_name[50], message[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    sscanf(content, "%49s ", recipient_name);
    strcpy(message, content + strlen(recipient_name) + 1);

    client_node *sender = find_client_by_addr(client_addr);
    client_node *recipient = find_client_by_name(recipient_name);

    if (recipient && sender) {
        strncpy(response, sender->client_name, BUFFER_SIZE - 1);
        strncat(response, ": ", BUFFER_SIZE - strlen(response) - 1);
        strncat(response, message, BUFFER_SIZE - strlen(response) - 2);
        strncat(response, "\n", BUFFER_SIZE - strlen(response) - 1);
        udp_socket_write(sd, &recipient->client_address, response, strlen(response) + 1);
    }

    free(args);
    return NULL;
}

void* handle_mute_request(void *arg) {
    request_args *args = (request_args *)arg;
    struct sockaddr_in client_addr = args->client_addr;
    char *mute_name = args->request_content;

    pthread_rwlock_wrlock(&client_list_lock);
    client_node *client = find_client_by_addr(client_addr);
    if (client) {
        add_mute(client, mute_name);
    }
    pthread_rwlock_unlock(&client_list_lock);

    free(args);
    return NULL;
}

void* handle_unmute_request(void *arg) {
    request_args *args = (request_args *)arg;
    struct sockaddr_in client_addr = args->client_addr;
    char *unmute_name = args->request_content;

    pthread_rwlock_wrlock(&client_list_lock);
    client_node *client = find_client_by_addr(client_addr);
    if (client) {
        remove_mute(client, unmute_name);
    }
    pthread_rwlock_unlock(&client_list_lock);

    free(args);
    return NULL;
}

void* handle_rename_request(void *arg) {
    request_args *args = (request_args *)arg;
    int sd = args->sd;
    struct sockaddr_in client_addr = args->client_addr;
    char *new_name = args->request_content;
    char response[BUFFER_SIZE];

    pthread_rwlock_wrlock(&client_list_lock);
    client_node *client = find_client_by_addr(client_addr);
    if (client) {
        free(client->client_name);
        client->client_name = strdup(new_name);
        snprintf(response, BUFFER_SIZE, "You are now known as %s\n", new_name);
        udp_socket_write(sd, &client_addr, response, strlen(response) + 1);
    }
    pthread_rwlock_unlock(&client_list_lock);

    free(args);
    return NULL;
}

void* handle_disconn_request(void *arg) {
    request_args *args = (request_args *)arg;
    int sd = args->sd;
    struct sockaddr_in client_addr = args->client_addr;
    char response[BUFFER_SIZE];
    char *client_name = NULL;

    pthread_rwlock_rdlock(&client_list_lock);
    client_node *client = find_client_by_addr(client_addr);
    if (client) {
        client_name = strdup(client->client_name);
    }
    pthread_rwlock_unlock(&client_list_lock);

    if (client_name) {
        remove_client_by_name(client_name);
        strcpy(response, "Disconnected. Bye!\n");
        udp_socket_write(sd, &client_addr, response, strlen(response) + 1);
        free(client_name);
    }

    free(args);
    return NULL;
}

void* handle_kick_request(void *arg) {
    request_args *args = (request_args *)arg;
    int sd = args->sd;
    struct sockaddr_in client_addr = args->client_addr;
    char *kick_name = args->request_content;
    char response[BUFFER_SIZE];

    // Check if requester is admin (port 6666)
    if (ntohs(client_addr.sin_port) != 6666) {
        free(args);
        return NULL;
    }

    client_node *kickee = find_client_by_name(kick_name);
    if (kickee) {
        strcpy(response, "You have been removed from the chat\n");
        udp_socket_write(sd, &kickee->client_address, response, strlen(response) + 1);
        
        snprintf(response, BUFFER_SIZE, "%s has been removed from the chat\n", kick_name);
        broadcast_message(sd, response, NULL);
        
        remove_client_by_name(kick_name);
    }

    free(args);
    return NULL;
}

// ===== MAIN SERVER =====
int main(int argc, char *argv[])
{
    int sd = udp_socket_open(SERVER_PORT);
    assert(sd > -1);
    printf("Server is listening on port %d\n", SERVER_PORT);

    while (1) 
    {
        char client_request[BUFFER_SIZE], request_type[50], request_content[BUFFER_SIZE];
        struct sockaddr_in client_address;
        int rc = udp_socket_read(sd, &client_address, client_request, BUFFER_SIZE); // blocks until a UDP packet arrives

        if (rc > 0)
        {
            parse_input(client_request, request_type, request_content);   
            request_args *args = malloc(sizeof(request_args));
            args->sd = sd;
            args->client_addr = client_address;
            strcpy(args->request_content, request_content);
            
            pthread_t handler_tid;

            if (strcmp(request_type, "conn") == 0) {
                pthread_create(&handler_tid, NULL, handle_conn_request, args);
                pthread_detach(handler_tid);
            } else if (strcmp(request_type, "say") == 0) {
                pthread_create(&handler_tid, NULL, handle_say_request, args);
                pthread_detach(handler_tid);
            } else if (strcmp(request_type, "sayto") == 0) {
                pthread_create(&handler_tid, NULL, handle_sayto_request, args);
                pthread_detach(handler_tid);
            } else if (strcmp(request_type, "mute") == 0) {
                pthread_create(&handler_tid, NULL, handle_mute_request, args);
                pthread_detach(handler_tid);
            } else if (strcmp(request_type, "unmute") == 0) {
                pthread_create(&handler_tid, NULL, handle_unmute_request, args);
                pthread_detach(handler_tid);
            } else if (strcmp(request_type, "rename") == 0) {
                pthread_create(&handler_tid, NULL, handle_rename_request, args);
                pthread_detach(handler_tid);
            } else if (strcmp(request_type, "disconn") == 0) {
                pthread_create(&handler_tid, NULL, handle_disconn_request, args);
                pthread_detach(handler_tid);
            } else if (strcmp(request_type, "kick") == 0) {
                pthread_create(&handler_tid, NULL, handle_kick_request, args);
                pthread_detach(handler_tid);
            }
        }
    }

    return 0;
}