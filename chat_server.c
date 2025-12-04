#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include "udp.h"

pthread_rwlock_t client_list_lock = PTHREAD_RWLOCK_INITIALIZER;
client_node *client_list_head = NULL;

// ===== HELPER FUNCTIONS =====
void add_client(struct sockaddr_in addr, char *name) {  
    pthread_rwlock_wrlock(&client_list_lock);   // writer lock 
    client_node *new_node = malloc(sizeof(client_node));    // allocate memory for a new client node
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
    pthread_rwlock_rdlock(&client_list_lock);  // reader lock - multiple threads can read simultaneously
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
    return NULL;  // if not found
}

client_node* find_client_by_addr(struct sockaddr_in addr) {
    pthread_rwlock_rdlock(&client_list_lock);   // reader lock
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
    pthread_rwlock_wrlock(&client_list_lock);   // writer lock
    client_node *curr = client_list_head;
    while (curr) {
        if (strcmp(curr->client_name, name) == 0) {
            if (curr->previous) curr->previous->next = curr->next;
            else client_list_head = curr->next;
            if (curr->next) curr->next->previous = curr->previous;
            
            // free mute list
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

void* handle_disconn_request(void *arg)
{
    request_args *args = (request_args *)arg;
    int sd = args->sd;
    struct sockaddr_in client_addr = args->client_addr;
    char *client_name = args->request_content;
    char response[BUFFER_SIZE];

    remove_client_by_name(client_name);
    snprintf(response, BUFFER_SIZE, "Disconnected. Bye %s!", client_name);
    udp_socket_write(sd, &client_addr, response, strlen(response) + 1);

    free(args);
    return NULL;
}

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

            if (strcmp(request_type, "conn") == 0){
                pthread_create(&handler_tid, NULL, handle_conn_request, args);
                pthread_detach(handler_tid);
            }
            if (strcmp(request_type, "disconn") == 0){
                pthread_create(&handler_tid, NULL, handle_disconn_request, args);
                pthread_detach(handler_tid);
            }
        }
    }

    return 0;
}