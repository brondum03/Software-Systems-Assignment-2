#include <stdio.h>
#include <stdlib.h>
#include "udp.h"

int main(int argc, char *argv[])
{

    // This function opens a UDP socket,
    // binding it to all IP interfaces of this machine,
    // and port number SERVER_PORT
    // (See details of the function in udp.h)
    int sd = udp_socket_open(SERVER_PORT);

    assert(sd > -1);

    // Server main loop
    while (1) 
    {
        // Storage for request and response messages
        // Added storage for client's port (xxxxx\0) and IP address (max ip addr length)
        char client_request[BUFFER_SIZE], server_response[BUFFER_SIZE], client_port[6], client_ip[INET_ADDRSTRLEN];

        // Demo code (remove later)
        printf("Server is listening on port %d\n", SERVER_PORT);

        // Variable to store incoming client's IP address and port
        struct sockaddr_in client_address;
    
        // This function reads incoming client request from
        // the socket at sd.
        // (See details of the function in udp.h)
        int rc = udp_socket_read(sd, &client_address, client_request, BUFFER_SIZE);

        // Successfully received an incoming request
        if (rc > 0)
        {
            // Demo code (remove later)
            int client_port_int = ntohs(client_address.sin_port);
            const char* client_ip_addr = inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
            snprintf(client_port, sizeof(client_port), "%d", client_port_int);
            strcpy(server_response, "Hi, the server has received: ");
            strcat(server_response, client_request);
            strcat(server_response, " from port ");
            strcat(server_response, client_port);
            strcat(server_response, " from IP address ");
            strcat(server_response, client_ip_addr);
            strcat(server_response, "\n");

            // This function writes back to the incoming client,
            // whose address is now available in client_address, 
            // through the socket at sd.
            // (See details of the function in udp.h)
            rc = udp_socket_write(sd, &client_address, server_response, BUFFER_SIZE);

            // Demo code (remove later)
            printf("Request served...\n");
        }
    }

    return 0;
}