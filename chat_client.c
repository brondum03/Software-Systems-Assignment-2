#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "udp.h"

#define BUFFER_SIZE 1024

typedef struct thread_args {
    int sd;
    struct sockaddr_in server_addr;
} thread_args;

void* sender_thread_function(void *arg);
void* listener_thread_function(void *arg);

int main(int argc, char *argv[])
{
    pthread_t sender_tid, listener_tid;

    int sd = udp_socket_open(0);  // bind client to any available UDP port
    struct sockaddr_in server_addr;
    set_socket_addr(&server_addr, "127.0.0.1", SERVER_PORT);

    thread_args *args = malloc(sizeof(thread_args));
    args->sd = sd;
    args->server_addr = server_addr;

    pthread_create(&sender_tid, NULL, sender_thread_function, args);    // sender thread
    pthread_create(&listener_tid, NULL, listener_thread_function, args); // listener thread

    pthread_join(sender_tid, NULL);
    pthread_join(listener_tid, NULL);

    free(args);
    return 0;
}

void* sender_thread_function(void *arg)
{
    thread_args *args = (thread_args *)arg;
    int sd = args->sd;
    struct sockaddr_in server_addr = args->server_addr;
    char input[BUFFER_SIZE];

    while (1)
    {
        if (fgets(input, BUFFER_SIZE, stdin) == NULL)   // retrieves newline terminated string from stdin 
        {
            perror("fgets failed");
            break;
        }

        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n')  // trim \n at the end
        {    
            input[len - 1] = '\0';
            len--;
        }

        int rc = udp_socket_write(sd, &server_addr, input, (int)(len + 1));
        if (rc < 0) 
        {
            perror("udp_socket_write failed");
        }

        if (strncmp(input, "disconn$", 8) == 0)
        {
            break;
        }
    }
    return NULL;
}

void* listener_thread_function(void *arg)
{
    thread_args *args = (thread_args *)arg;
    int sd = args->sd;
    struct sockaddr_in server_addr = args->server_addr;
    char server_response[BUFFER_SIZE];
    struct sockaddr_in responder_addr;

    while (1)
    {
        int rc = udp_socket_read(sd, &responder_addr, server_response, BUFFER_SIZE);
        if (rc > 0) {
            if (rc < BUFFER_SIZE) server_response[rc] = '\0';
            printf("%s", server_response);
            fflush(stdout);
        } else if (rc < 0) {
            perror("udp_socket_read failed");
        }
    }

    return NULL;
}

//chat_client request sender
void send_request(char *input)
{
    int sd = udp_socket_open(0); //open UDP socket on any available port
    struct sockaddr_in server_addr, responder_addr;
    set_socket_addr(&server_addr, "127.0.0.1", SERVER_PORT); //format server address with local IP and server port
    char server_response[BUFFER_SIZE];
    int rc = udp_socket_write(sd, &server_addr, input, BUFFER_SIZE); //send connection request to server from socket sd
    if (rc > 0)
    {
        int rc = udp_socket_read(sd, &responder_addr, server_response, BUFFER_SIZE); //waits for server response
        printf("server_response: %s", server_response); 
    }
}   