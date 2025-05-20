// author: Jia
// date: 2025-05-20
// sip_user.c - handle INVITE, ACK, BYE as SIP User (Server-4)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LISTEN_PORT 5004

void *handle_session(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    recv(sock, buffer, sizeof(buffer) - 1, 0);
    printf("SIP USER received:\n%s\n", buffer);

    if (strncmp(buffer, "INVITE", 6) == 0) {
        const char *sdp =
            "v=0\r\n"
            "o=1073055600 1073055600 IN IP4 127.0.0.1\r\n"
            "s=Session SDP\r\n"
            "c=IN IP4 127.0.0.1\r\n"
            "t=0 0\r\n"
            "m=video 8382 RTP/AVP 98 99\r\n"
            "a=rtpmap:0 PCMU/8000\r\n";
        int len = strlen(sdp);

        char response[1024];
        snprintf(response, sizeof(response),
            "SIP/2.0 200 OK\r\n"
            "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=elelmloperg; received=127.0.0.1\r\n"
            "From: <sip:bill@home3.net>; tag=aspyui\r\n"
            "To: <sip:JohnMiller@university.edu>;tag=4445556\r\n"
            "Call-ID: 23456678901223\r\n"
            "CSeq: 1 INVITE\r\n"
            "Contact: <sip:200.0200.10.33>\r\n"
            "Content-Type: application/sdp\r\n"
            "Content-Length: %d\r\n\r\n%s", len, sdp);

        send(sock, response, strlen(response), 0);
        printf("SIP USER sent 200 OK with SDP\n");
    } else if (strncmp(buffer, "ACK", 3) == 0) {
        printf("SIP USER received ACK.\n");
    } else if (strncmp(buffer, "BYE", 3) == 0) {
        const char *ok = "SIP/2.0 200 OK\r\nContent-Length: 0\r\n\r\n";
        send(sock, ok, strlen(ok), 0);
        printf("SIP USER received BYE and responded with 200 OK\n");
    } else {
        printf("SIP USER received unknown message.\n");
    }

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
    printf("SIP USER listening on port %d...\n", LISTEN_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        int *sock_ptr = malloc(sizeof(int));
        *sock_ptr = client_sock;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_session, sock_ptr);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
