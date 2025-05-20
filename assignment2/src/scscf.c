// author: Jia
// date: 2025-05-20
// scscf.c - support REGISTER handling + INVITE/ACK/BYE processing
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LISTEN_PORT 5003
#define SIP_USER_IP "127.0.0.1"
#define SIP_USER_PORT 5004

typedef struct {
    char user_id[100];
    char ip_address[50];
    int expires;
} RegistrationEntry;

RegistrationEntry registry[100];
int reg_count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *expire_timer(void *arg) {
    RegistrationEntry *entry = (RegistrationEntry *)arg;
    while (entry->expires > 0) {
        sleep(1);
        pthread_mutex_lock(&lock);
        entry->expires--;
        pthread_mutex_unlock(&lock);
    }
    pthread_mutex_lock(&lock);
    printf("Registration expired for user: %s\n", entry->user_id);
    entry->user_id[0] = '\0';
    pthread_mutex_unlock(&lock);
    return NULL;
}

int is_registered(const char *user_id) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < reg_count; i++) {
        if (strcmp(registry[i].user_id, user_id) == 0 && registry[i].expires > 0) {
            pthread_mutex_unlock(&lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

void *handle_message(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    recv(sock, buffer, sizeof(buffer)-1, 0);
    printf("S-CSCF received message:\n%s\n", buffer);

    // REGISTER
    if (strncmp(buffer, "USER:", 5) == 0) {
        RegistrationEntry entry;
        sscanf(buffer, "USER:%[^|]|IP:%[^|]|EXPIRES:%d",
               entry.user_id, entry.ip_address, &entry.expires);

        pthread_mutex_lock(&lock);
        registry[reg_count++] = entry;
        pthread_mutex_unlock(&lock);

        RegistrationEntry *entry_copy = malloc(sizeof(RegistrationEntry));
        *entry_copy = entry;
        pthread_t timer_thread;
        pthread_create(&timer_thread, NULL, expire_timer, entry_copy);
        pthread_detach(timer_thread);

        char ok_msg[1024];
        snprintf(ok_msg, sizeof(ok_msg),
            "SIP/2.0 200 OK\r\n"
            "Via: SIP/2.0/UDP 182.1.0.2:5060;branch=elelmloperg; received=127.0.0.1\r\n"
            "To: <sip:%s>\r\n"
            "From: <sip:%s>;tag=4445556\r\n"
            // "To: <sip:%s@university.edu>\r\n"
            // "From: <sip:%s@university.edu>;tag=4445556\r\n"
            "Call-ID: 23456678901223\r\n"
            "CSeq: 1 REGISTER\r\n"
            "Contact: <sip:127.0.0.1>\r\n"
            "Expires: %d\r\n"
            "Content-Length: 0\r\n\r\n",
            entry.user_id, entry.user_id, entry.expires);
        send(sock, ok_msg, strlen(ok_msg), 0);
        close(sock);
        return NULL;
    }

    // INVITE
    if (strncmp(buffer, "INVITE", 6) == 0) {
        // extract user from To header
        char user_id[100] = {0};
        char *to_line = strstr(buffer, "From:");
        if (to_line) {
            // sscanf(to_line, "From: <sip:%[^@]", user_id);
            sscanf(to_line, "From: <sip:%[^>]", user_id);

        }

        if (!is_registered(user_id)) {
            const char *err_msg =
                "SIP/2.0 401 Unauthorized\r\nContent-Length: 0\r\n\r\n";
            send(sock, err_msg, strlen(err_msg), 0);
            close(sock);
            return NULL;
        }

        int sip_sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sip_addr;
        sip_addr.sin_family = AF_INET;
        sip_addr.sin_port = htons(SIP_USER_PORT);
        sip_addr.sin_addr.s_addr = inet_addr(SIP_USER_IP);

        if (connect(sip_sock, (struct sockaddr*)&sip_addr, sizeof(sip_addr)) < 0) {
            perror("Connect to SIP USER failed");
            close(sock);
            return NULL;
        }

        send(sip_sock, buffer, strlen(buffer), 0);

        memset(buffer, 0, sizeof(buffer));
        int resp = recv(sip_sock, buffer, sizeof(buffer)-1, 0);
        if (resp > 0) {
            send(sock, buffer, strlen(buffer), 0);
        }

        close(sip_sock);
        close(sock);
        return NULL;
    }

    // ACK / BYE
    if (strncmp(buffer, "ACK", 3) == 0 || strncmp(buffer, "BYE", 3) == 0) {
        int sip_sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sip_addr;
        sip_addr.sin_family = AF_INET;
        sip_addr.sin_port = htons(SIP_USER_PORT);
        sip_addr.sin_addr.s_addr = inet_addr(SIP_USER_IP);

        if (connect(sip_sock, (struct sockaddr*)&sip_addr, sizeof(sip_addr)) < 0) {
            perror("Connect to SIP USER failed");
            close(sock);
            return NULL;
        }

        send(sip_sock, buffer, strlen(buffer), 0);

        if (strncmp(buffer, "BYE", 3) == 0) {
            memset(buffer, 0, sizeof(buffer));
            int resp = recv(sip_sock, buffer, sizeof(buffer)-1, 0);
            if (resp > 0) {
                send(sock, buffer, strlen(buffer), 0);
            }
        }

        close(sip_sock);
        close(sock);
        return NULL;
    }

    printf("S-CSCF received unknown message.\n");
    close(sock);
    return NULL;
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_sock, 5);
    printf("S-CSCF listening on port %d...\n", LISTEN_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        int *sock_ptr = malloc(sizeof(int));
        *sock_ptr = client_sock;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_message, sock_ptr);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
