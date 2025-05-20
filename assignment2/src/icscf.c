// author: Jia
// date: 2025-05-20
// icscf.c - supports REGISTER processing + INVITE/ACK/BYE forwarding
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LISTEN_PORT 5001
#define HSS_PORT 5002
#define HSS_IP "127.0.0.1"
#define S_CSCF_PORT 5003
#define S_CSCF_IP "127.0.0.1"

void extract_fields(const char *msg, char *user_id, char *ip_addr, char *expires) {
    char *from_line = strstr(msg, "From:");
    if (from_line) {
        sscanf(from_line, "From: <sip:%[^@]", user_id);
    }

    char *contact_line = strstr(msg, "Contact:");
    if (contact_line) {
        sscanf(contact_line, "Contact: <sip:%[^>]", ip_addr);
    }

    char *expires_line = strstr(msg, "Expires:");
    if (expires_line) {
        sscanf(expires_line, "Expires: %s", expires);
    }
}

void *handle_pcscf(void *arg) {
    int pcscf_sock = *(int *)arg;
    free(arg);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int bytes = recv(pcscf_sock, buffer, sizeof(buffer)-1, 0);
    if (bytes <= 0) {
        close(pcscf_sock);
        return NULL;
    }

    printf("I-CSCF received message from P-CSCF:\n%s\n", buffer);

    // 检查是否是 REGISTER
    if (strncmp(buffer, "REGISTER", 8) == 0) {
        // 提取字段
        char user_id[100] = {0}, ip_addr[50] = {0}, expires[20] = {0};
        extract_fields(buffer, user_id, ip_addr, expires);

        // 构造消息发给 HSS
        char message[256];
        snprintf(message, sizeof(message),
                 "USER:%s@university.edu|IP:%s|EXPIRES:%s",
                 user_id, ip_addr, expires);

        // 连接 HSS
        int hss_sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in hss_addr;
        hss_addr.sin_family = AF_INET;
        hss_addr.sin_port = htons(HSS_PORT);
        hss_addr.sin_addr.s_addr = inet_addr(HSS_IP);

        if (connect(hss_sock, (struct sockaddr*)&hss_addr, sizeof(hss_addr)) < 0) {
            perror("Failed to connect to HSS");
            close(pcscf_sock);
            return NULL;
        }

        send(hss_sock, message, strlen(message), 0);
        printf("Sent to HSS: %s\n", message);

        // 接收 HSS 回复
        memset(buffer, 0, sizeof(buffer));
        int r = recv(hss_sock, buffer, sizeof(buffer)-1, 0);
        if (r > 0) {
            printf("Received from HSS:\n%s\n", buffer);
            send(pcscf_sock, buffer, strlen(buffer), 0);
        }

        close(hss_sock);
        close(pcscf_sock);
        return NULL;
    }

    // 不是 REGISTER：直接转发给 S-CSCF
    int scscf_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in scscf_addr;
    scscf_addr.sin_family = AF_INET;
    scscf_addr.sin_port = htons(S_CSCF_PORT);
    scscf_addr.sin_addr.s_addr = inet_addr(S_CSCF_IP);

    if (connect(scscf_sock, (struct sockaddr*)&scscf_addr, sizeof(scscf_addr)) < 0) {
        perror("Failed to connect to S-CSCF");
        close(pcscf_sock);
        return NULL;
    }

    send(scscf_sock, buffer, strlen(buffer), 0);
    printf("Forwarded message to S-CSCF.\n");

    // 等待 S-CSCF 响应
    memset(buffer, 0, sizeof(buffer));
    int res = recv(scscf_sock, buffer, sizeof(buffer)-1, 0);
    if (res > 0) {
        printf("Received response from S-CSCF:\n%s\n", buffer);
        send(pcscf_sock, buffer, strlen(buffer), 0);
    }

    close(scscf_sock);
    close(pcscf_sock);
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
    printf("I-CSCF listening on port %d...\n", LISTEN_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        int *sock_ptr = malloc(sizeof(int));
        *sock_ptr = client_sock;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_pcscf, sock_ptr);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
