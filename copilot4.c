#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    char buffer[BUFFER_SIZE], url[64];

    // Create and set up socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(server_fd, F_SETFL, O_NONBLOCK);  // Non-blocking mode
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, MAX_CLIENTS);

    struct pollfd fds[MAX_CLIENTS];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    int nfds = 1;

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        poll(fds, nfds, -1);  // Wait for activity

        if (fds[0].revents & POLLIN) {
            client_fd = accept(server_fd, (struct sockaddr*)&address, &addr_len);
            fcntl(client_fd, F_SETFL, O_NONBLOCK);
            fds[nfds].fd = client_fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                int bytes = read(fds[i].fd, buffer, BUFFER_SIZE - 1);
                if (bytes <= 0) {
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    continue;
                }
                buffer[bytes] = '\0';

                // Extract URL from GET request
                if (strncmp(buffer, "GET ", 4) == 0) {
                    char *start = buffer + 4;
                    char *end = strchr(start, ' ');
                    if (end) {
                        int len = (end - start < 64) ? (end - start) : 63;
                        strncpy(url, start, len);
                        url[len] = '\0';
                        printf("Requested URL: %s\n", url);
                    }
                }

                // Respond to client
                const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
                write(fds[i].fd, response, strlen(response));
                close(fds[i].fd);
                fds[i] = fds[nfds - 1];
                nfds--;
            }
        }
    }

    return 0;
}
