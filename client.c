#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_NAME_LEN 30
#define MAX_RESPONSE_LEN 11

int connectToServer(const char *server, int port) {
    int cfd;
    struct sockaddr_in server_address;
    cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd == -1) {
        perror("Error: cannot create socket");
        return -1;
    }
    memset(&server_address, 0, sizeof(struct sockaddr_in));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (inet_pton(AF_INET, server, &server_address.sin_addr) <= 0) {
        perror("Error: invalid address");
        close(cfd);
        return -1;
    }
    if (connect(cfd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error: cannot connect");
        close(cfd);
        return -1;
    }
    return cfd;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server address> <port>\n", argv[0]);
        return 1;
    }

    const char *server_addr = argv[1];
    int port = atoi(argv[2]);

    int cfd = connectToServer(server_addr, port);
    if (cfd == -1) {
        return 1;
    }

    printf("Ready\n");

    char name[MAX_NAME_LEN];
    while (fgets(name, MAX_NAME_LEN, stdin)) {
        if (name[0] == '\n') {
            break;
        }
        if (send(cfd, name, strlen(name), 0) < 0) {
            perror("Error: failed to send to server");
            close(cfd);
            return 1;
        }

        char response[MAX_RESPONSE_LEN];
        int length = recv(cfd, response, MAX_RESPONSE_LEN - 1, 0);
        if (length < 0) {
            perror("Error: failed to receive data");
            close(cfd);
            return 1;
        }
        response[length] = '\0';
        printf("%s", response);
    }

    close(cfd);
    return 0;
}
