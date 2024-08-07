#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include "record.h"

#define MAX_NAME_LEN 30
#define MAX_RESPONSE_LEN 11
#define BACKLOG 10

// Helper function to find a record by name
int get_sunspots(FILE *f, const char *name, unsigned short *psunspots) {
    record r;

    unsigned n = strlen(name);
    fseek(f, 0, SEEK_SET);

    while (fread(&r, sizeof(record), 1, f) == 1) {
        if (n == r.name_len && strncmp(name, r.name, n) == 0) {
            *psunspots = r.sunspots;
            return 1;
        }
    }

    return 0;
}

// Handle a single client's request
void handle_client(int client_socket, FILE *customer_file) {
    char name[MAX_NAME_LEN];
    ssize_t length;

    while ((length = recv(client_socket, name, MAX_NAME_LEN - 1, 0)) > 0) {
        name[length] = '\0'; // Null-terminate the received string

        if (name[length - 1] == '\n') {
            name[length - 1] = '\0';  // Remove the newline character
        }

        unsigned short sunspots;
        if (get_sunspots(customer_file, name, &sunspots)) {
            char response[MAX_RESPONSE_LEN];
            snprintf(response, MAX_RESPONSE_LEN, "%u\n", sunspots);
            send(client_socket, response, strlen(response), 0);
        } else {
            send(client_socket, "none\n", 5, 0);
        }
    }

    close(client_socket);
    exit(EXIT_SUCCESS);
}

// Handle SIGCHLD signals to prevent zombie processes
void sigchld_handler(int s) {
    (void)s; // Silence unused variable warning
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <customer file>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    const char *customer_file_name = argv[2];

    // Open customer file
    FILE *customer_file = fopen(customer_file_name, "rb");
    if (!customer_file) {
        perror("Error opening customer file");
        exit(EXIT_FAILURE);
    }

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error creating socket");
        fclose(customer_file);
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the specified port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        close(sockfd);
        fclose(customer_file);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(sockfd, BACKLOG) == -1) {
        perror("Error listening on socket");
        close(sockfd);
        fclose(customer_file);
        exit(EXIT_FAILURE);
    }

    // Set up the SIGCHLD signal handler
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        close(sockfd);
        fclose(customer_file);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    // Main server loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int client_socket = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        // Fork a new process to handle the client
        if (!fork()) {
            close(sockfd); // Child process doesn't need the listener socket
            handle_client(client_socket, customer_file);
        }

        close(client_socket); // Parent process doesn't need this client socket
    }

    close(sockfd);
    fclose(customer_file);
    return 0;
}
