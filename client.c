// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define TIMEOUT_SEC 3
#define MAX_RETRIES 3
#define NUM_SERVERS 5

const int ports[NUM_SERVERS] = {5001, 5002, 5003, 5004, 5005};

typedef struct {
    int sock;
    int server_id;
} ThreadArg;

int send_with_ack(int sock, const char *message, int server_id) {
    int retries = 0;
    char buffer[1024];

    while (retries < MAX_RETRIES) {
        send(sock, message, strlen(message), 0);

        // set up select for timeout
        fd_set readfds;
        struct timeval timeout;

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        int activity = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (activity > 0 && FD_ISSET(sock, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(sock, buffer, sizeof(buffer)-1, 0);
            if (bytes_received > 0) {
                printf("[Server %d]: %s\n", server_id + 1, buffer);
                return 1; // success
            }
        } else {
            retries++;
            printf("Timeout waiting for Server %d response. Retrying (%d/%d)...\n", server_id + 1, retries, MAX_RETRIES);
        }
    }

    printf("IMS node (Server %d) does not respond\n", server_id + 1);
    return 0; // failure
}


void *receive_messages(void *arg) {
    ThreadArg *thread_arg = (ThreadArg *)arg;
    int sock = thread_arg->sock;
    int server_id = thread_arg->server_id;
    char buffer[1024];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (bytes_received <= 0) break;
        printf("\n[Server-%d]: %s\n", server_id, buffer);
    }

    free(arg);
    return NULL;
}

int main() {
    int sockets[NUM_SERVERS];
    pthread_t threads[NUM_SERVERS];

    // Connect to all servers
    for (int i = 0; i < NUM_SERVERS; i++) {
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sockets[i] < 0) {
            perror("Socket creation failed");
            continue;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(ports[i]);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sockets[i], (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            close(sockets[i]);
            continue;
        }

        printf("Connected to Server-%d (port %d)\n", i, ports[i]);

        ThreadArg *arg = malloc(sizeof(ThreadArg));
        arg->sock = sockets[i];
        arg->server_id = i;
        pthread_create(&threads[i], NULL, receive_messages, arg);
    }

    // Mesage sending loop
    int server_index = 0;
    char message[1024]
    while (1) {
        printf("Enter message: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;
        if (strcmp(message, "bye") == 0) break;
    
        send_with_ack(sockets[server_index], message, server_index);
        server_index = (server_index + 1) % NUM_SERVERS; // loop through servers
    }

    // Cleanup
    for (int i = 0; i < NUM_SERVERS; i++) {
        close(sockets[i]);
        pthread_join(threads[i], NULL);
    }

    return 0;
}