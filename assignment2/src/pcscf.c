// author: Jia
// date: 2025-05-20
// pcscf.c - support REGISTER, INVITE, ACK, BYE message forwarding
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LISTEN_PORT 6000
#define I_CSCF_PORT 5001
#define I_CSCF_IP "127.0.0.1"

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));

    // receive message from client
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_sock);
        return NULL;
    }

    printf("P-CSCF received message from client:\n%s\n", buffer);

    // check if REGISTER
    if (strncmp(buffer, "REGISTER", 8) == 0) {
        // validate content-length
        char *cl = strstr(buffer, "Content-Length:");
        if (!cl || strstr(cl, "0") == NULL) {
            printf("Register Error: Content-Length is not 0\n");
            close(client_sock);
            return NULL;
        }
    }

    // create socket to connect to I-CSCF
    int icscf_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in icscf_addr;
    icscf_addr.sin_family = AF_INET;
    icscf_addr.sin_port = htons(I_CSCF_PORT);
    icscf_addr.sin_addr.s_addr = inet_addr(I_CSCF_IP);

    if (connect(icscf_sock, (struct sockaddr*)&icscf_addr, sizeof(icscf_addr)) < 0) {
        perror("Failed to connect to I-CSCF");
        const char *fail_msg = "503 Service Unavailable\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, fail_msg, strlen(fail_msg), 0);
        close(client_sock);
        return NULL;
    }

    // forward message to I-CSCF
    send(icscf_sock, buffer, strlen(buffer), 0);
    printf("Forwarded message to I-CSCF.\n");

    // wait for response
    memset(buffer, 0, sizeof(buffer));
    int response_len = recv(icscf_sock, buffer, sizeof(buffer) - 1, 0);
    if (response_len > 0) {
        printf("Received response from I-CSCF:\n%s\n", buffer);
        send(client_sock, buffer, strlen(buffer), 0);
        printf("Sent response back to client.\n");
    }

    close(icscf_sock);
    close(client_sock);
    return NULL;
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(LISTEN_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_sock, 5);
    printf("P-CSCF listening on port %d...\n", LISTEN_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        int *client_ptr = malloc(sizeof(int));
        *client_ptr = client_sock;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_ptr);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
