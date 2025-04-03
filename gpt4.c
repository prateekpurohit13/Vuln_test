#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define URL_SIZE 64

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE], url[URL_SIZE];
    read(client_socket, buffer, BUFFER_SIZE);
    
    char *start = strstr(buffer, "GET ");
    if (start) {
        start += 4;
        char *end = strchr(start, ' ');
        if (end) {
            size_t len = (end - start) < URL_SIZE ? (end - start) : (URL_SIZE - 1);
            strncpy(url, start, len);
            url[len] = '\0';
            printf("Requested URL: %s\n", url);
        }
    }
    
    const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
    write(client_socket, response, strlen(response));
    close(client_socket);
    free(arg);
    return NULL;
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_socket, 10);
    printf("Server listening on port %d\n", PORT);

    while (1) {
        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, NULL, NULL);
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_socket);
        pthread_detach(thread);
    }
    close(server_socket);
    return 0;
}
