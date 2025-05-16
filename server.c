#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[1024];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
        if (bytes_received <= 0) break;
        printf("Server received: %s\n", buffer);
        send(client_socket, buffer, bytes_received, 0);
    }
    close(client_socket);
    return NULL;
}
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Bind failed");
        return 1;
    }
    listen(server_socket, 5);
    printf("Server listening on port %d\n", port);
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        int *client_ptr = malloc(sizeof(int));
        *client_ptr = client_socket;
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, client_ptr);
        pthread_detach(thread_id);
    }
    close(server_socket);
    return 0;
}
