#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#define PORT 8080
#define MAX_CONNECTIONS 1000
#define BUFFER_SIZE 1024
#define URL_BUFFER_SIZE 64

typedef struct {
    int fd;
    char read_buffer[BUFFER_SIZE];
    size_t read_pos;
} client_t;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void extract_url(const char *request, char *url_buffer) {
    // Find GET request
    const char *get_pos = strstr(request, "GET ");
    if (get_pos) {
        // Move past "GET "
        get_pos += 4;
        
        // Find the end of the URL (space before HTTP version)
        const char *url_end = strchr(get_pos, ' ');
        if (url_end) {
            // Calculate URL length (limit to our buffer size - 1 for null termination)
            size_t url_len = url_end - get_pos;
            if (url_len >= URL_BUFFER_SIZE) {
                url_len = URL_BUFFER_SIZE - 1;
            }
            
            // Direct copy of URL into the buffer
            memcpy(url_buffer, get_pos, url_len);
            url_buffer[url_len] = '\0';
        } else {
            strcpy(url_buffer, "/");
        }
    } else {
        strcpy(url_buffer, "/");
    }
}

void handle_client_data(client_t *client) {
    char url_buffer[URL_BUFFER_SIZE];
    
    // Extract URL from the request
    extract_url(client->read_buffer, url_buffer);
    
    // Print the extracted URL
    printf("Received request for URL: %s\n", url_buffer);
    
    // Simple HTTP response
    char response[512];
    sprintf(response, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "URL Received: %s\r\n", url_buffer);
    
    // Send response
    write(client->fd, response, strlen(response));
    
    // Close connection
    close(client->fd);
    client->fd = -1;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    client_t clients[MAX_CONNECTIONS] = {0};
    struct pollfd fds[MAX_CONNECTIONS + 1];
    
    // Initialize all client FDs to -1 (unused)
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        clients[i].fd = -1;
    }
    
    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    // Set server socket to non-blocking
    set_nonblocking(server_fd);
    
    printf("Server started on port %d\n", PORT);
    
    while (1) {
        // Set up poll structures
        int poll_count = 0;
        
        // Add server socket to poll
        fds[poll_count].fd = server_fd;
        fds[poll_count].events = POLLIN;
        poll_count++;
        
        // Add active client sockets to poll
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (clients[i].fd != -1) {
                fds[poll_count].fd = clients[i].fd;
                fds[poll_count].events = POLLIN;
                poll_count++;
            }
        }
        
        // Wait for events with a timeout of 100ms
        int poll_result = poll(fds, poll_count, 100);
        
        if (poll_result > 0) {
            // Check for new connections on server socket
            if (fds[0].revents & POLLIN) {
                int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                if (new_socket >= 0) {
                    set_nonblocking(new_socket);
                    
                    // Find free client slot
                    int slot = -1;
                    for (int i = 0; i < MAX_CONNECTIONS; i++) {
                        if (clients[i].fd == -1) {
                            slot = i;
                            break;
                        }
                    }
                    
                    if (slot != -1) {
                        clients[slot].fd = new_socket;
                        clients[slot].read_pos = 0;
                        memset(clients[slot].read_buffer, 0, BUFFER_SIZE);
                    } else {
                        // No free slots, reject connection
                        close(new_socket);
                    }
                }
            }
            
            // Check client sockets
            for (int i = 1; i < poll_count; i++) {
                if (fds[i].revents & POLLIN) {
                    // Find matching client
                    for (int j = 0; j < MAX_CONNECTIONS; j++) {
                        if (clients[j].fd == fds[i].fd) {
                            // Read data from client
                            ssize_t bytes_read = read(clients[j].fd, 
                                                     clients[j].read_buffer + clients[j].read_pos,
                                                     BUFFER_SIZE - clients[j].read_pos - 1);
                            
                            if (bytes_read > 0) {
                                clients[j].read_pos += bytes_read;
                                clients[j].read_buffer[clients[j].read_pos] = '\0';
                                
                                // Check if we have a complete HTTP request
                                if (strstr(clients[j].read_buffer, "\r\n\r\n")) {
                                    handle_client_data(&clients[j]);
                                }
                            } else {
                                // Connection closed or error
                                close(clients[j].fd);
                                clients[j].fd = -1;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    
    return 0;
}