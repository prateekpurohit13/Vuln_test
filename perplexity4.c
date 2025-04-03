#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define URL_SIZE 64

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char url[URL_SIZE];

    // Read request from client
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        perror("recv");
        return;
    }

    // Extract URL from request
    char* start = strstr(buffer, "GET ");
    if (start) {
        start += 5; // Skip "GET "
        char* end = strstr(start, " ");
        if (end) {
            strncpy(url, start, end - start);
            url[end - start] = '\0'; // Null-terminate
            printf("URL: %s\n", url);
        }
    }

    // Send response back to client
    char* response = "HTTP/1.1 200 OK\r\n\r\nHello, World!";
    send(client_socket, response, strlen(response), 0);

    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_length = sizeof(client_address);

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(1);
    }

    // Set server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    // Bind server socket
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("bind");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(server_socket, 3) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        // Accept client connection
        client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_length);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        // Handle client in a separate thread for concurrency
        pthread_t thread;
        pthread_create(&thread, NULL, (void* (*)(void*))handle_client, (void*)client_socket);
        pthread_detach(thread);
    }

    return 0;
}
