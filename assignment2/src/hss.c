// author: Jia
// date: 2025-05-19
// hss.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LISTEN_PORT 5002
#define S_CSCF_PORT 5003
#define S_CSCF_IP "127.0.0.1"

// simulate registered users in the database
const char *valid_users[] = {
    "JohnMiller@university.edu",
    "AliceSmith@university.edu"
};

int user_exists(const char *user_id) {
    for (int i = 0; i < sizeof(valid_users)/sizeof(valid_users[0]); i++) {
        if (strcmp(valid_users[i], user_id) == 0) {
            return 1;
        }
    }
    return 0;
}

void *handle_icscf(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    recv(sock, buffer, sizeof(buffer)-1, 0);
    printf("HSS received from I-CSCF:\n%s\n", buffer);

    // parse user_id
    char user_id[100] = {0}, ip[50] = {0}, expires[20] = {0};
    sscanf(buffer, "USER:%[^|]|IP:%[^|]|EXPIRES:%s", user_id, ip, expires);

    if (!user_exists(user_id)) {
        printf("User %s not found in HSS database.\n", user_id);

        // const char *unauth_msg =
        //     "SIP/2.0 401 Unauthorized\r\n"
        //     "Content-Length: 0\r\n\r\n";
        char unauth_msg[1024];
        snprintf(unauth_msg, sizeof(unauth_msg),
            "SIP/2.0 401 Unauthorized\r\n"
            "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=elelmloperg\r\n"
            "From: <sip:%s>;tag=4445556\r\n"
            "To: <sip:%s@yahoo.com>\r\n"
            "Call-ID: 23456678901223\r\n"
            "CSeq: 1 REGISTER\r\n"
            "Contact: <sip:127.0.0.1>\r\n"
            "Expires: 3600\r\n"
            "Content-Length: 0\r\n\r\n",
            user_id, user_id);
        send(sock, unauth_msg, strlen(unauth_msg), 0);
        close(sock);
        return NULL;
    }

    // if user exists, connect to S-CSCF
    int scscf_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in scscf_addr;
    scscf_addr.sin_family = AF_INET;
    scscf_addr.sin_port = htons(S_CSCF_PORT);
    scscf_addr.sin_addr.s_addr = inet_addr(S_CSCF_IP);

    if (connect(scscf_sock, (struct sockaddr*)&scscf_addr, sizeof(scscf_addr)) < 0) {
        perror("Failed to connect to S-CSCF");
        close(sock);
        return NULL;
    }

    // re-send original message to S-CSCF
    send(scscf_sock, buffer, strlen(buffer), 0);
    printf("Forwarded to S-CSCF: %s\n", buffer);

    // wait for S-CSCF response
    memset(buffer, 0, sizeof(buffer));
    recv(scscf_sock, buffer, sizeof(buffer)-1, 0);
    printf("Received from S-CSCF:\n%s\n", buffer);

    // return to I-CSCF
    send(sock, buffer, strlen(buffer), 0);
    close(scscf_sock);
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
    printf("HSS listening on port %d...\n", LISTEN_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        int *sock_ptr = malloc(sizeof(int));
        *sock_ptr = client_sock;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_icscf, sock_ptr);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
