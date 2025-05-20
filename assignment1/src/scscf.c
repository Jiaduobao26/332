// author: Jia
// date: 2025-05-19
// scscf.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LISTEN_PORT 5003

typedef struct {
    char user_id[100];
    char ip_address[50];
    int expires;
} RegistrationEntry;

RegistrationEntry registry[100];
int reg_count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// countdown thread
void *expire_timer(void *arg) {
    RegistrationEntry *entry = (RegistrationEntry *)arg;
    while (entry->expires > 0) {
        sleep(1);
        pthread_mutex_lock(&lock);
        entry->expires--;
        pthread_mutex_unlock(&lock);
    }

    // expire and delete user
    pthread_mutex_lock(&lock);
    printf("Registration expired for user: %s\n", entry->user_id);
    entry->user_id[0] = '\0';  // mark as empty
    pthread_mutex_unlock(&lock);
    return NULL;
}

void *handle_hss(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    recv(sock, buffer, sizeof(buffer)-1, 0);
    printf("S-CSCF received from HSS:\n%s\n", buffer);

    // parse message
    RegistrationEntry entry;
    sscanf(buffer, "USER:%[^|]|IP:%[^|]|EXPIRES:%d",
           entry.user_id, entry.ip_address, &entry.expires);

    pthread_mutex_lock(&lock);
    registry[reg_count++] = entry;
    pthread_mutex_unlock(&lock);

    // start countdown thread
    pthread_t timer_thread;
    RegistrationEntry *entry_copy = malloc(sizeof(RegistrationEntry));
    *entry_copy = entry;
    pthread_create(&timer_thread, NULL, expire_timer, entry_copy);
    pthread_detach(timer_thread);

    // send 200 OK response
    // const char *ok_msg =
    //     "SIP/2.0 200 OK\r\n"
    //     "Content-Length: 0\r\n\r\n";
    char ok_msg[1024];
    snprintf(ok_msg, sizeof(ok_msg),
        "SIP/2.0 200 OK\r\n"
        "Via: SIP/2.0/UDP 182.1.0.2:5060;branch=elelmloperg; received=127.0.0.1\r\n"
        "To: <sip:%s@yahoo.com>\r\n"
        "From: <sip:%s@university.edu>;tag=4445556\r\n"
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
        pthread_create(&tid, NULL, handle_hss, sock_ptr);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
