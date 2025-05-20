// author: Jia
// date: 2025-05-19
// client.c - supports user input, ack_timer, connection retry, REGISTER print
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6000
#define MAX_RETRIES 3
#define TIMEOUT_SEC 3

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <user_id>\n", argv[0]);
        return 1;
    }

    const char *user_id = argv[1];  // e.g., JohnMiller@university.edu

    int sock, connected = 0;
    struct sockaddr_in server_addr;

    // set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // try to create socket
    // ack_timer
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Socket creation failed");
            return 1;
        }

        printf("Connecting to P-CSCF (attempt %d)...\n", attempt);
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            connected = 1;
            break;
        } else {
            perror("Connection failed");
            close(sock);
            if (attempt < MAX_RETRIES) sleep(TIMEOUT_SEC);
        }
    }

    if (!connected) {
        printf("IMS node is unreachable after %d attempts.\n", MAX_RETRIES);
        return 1;
    }

    printf("Connected to P-CSCF (Server-0)\n");

    // construct SIP REGISTER message
    char register_msg[1024];
    snprintf(register_msg, sizeof(register_msg),
        "REGISTER sip:yahoo.com SIP/2.0\r\n"
        "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=elelmloperg\r\n"
        "Max-Forwards: 70\r\n"
        "To: <sip:%s@yahoo.com>\r\n"
        "From: <sip:%s>;tag=4445556\r\n"
        "Call-ID: 23456678901223\r\n"
        "Cseq: 1 REGISTER\r\n"
        "Contact: <sip:127.0.0.1>\r\n"
        "Expires: 3600\r\n"
        "Content-Length: 0\r\n\r\n",
        user_id, user_id);

    // print register message (slide 3.1)
    printf("REGISTER message being sent:\n%s\n", register_msg);

    // register_retry
    char buffer[4096];
    int register_retry = 0;

    while (register_retry < MAX_RETRIES) {
        printf("Sending REGISTER (attempt %d)...\n", register_retry + 1);
        send(sock, register_msg, strlen(register_msg), 0);

        // wait for response with timeout
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        struct timeval timeout = {TIMEOUT_SEC, 0};

        int ready = select(sock + 1, &read_fds, NULL, NULL, &timeout);
        if (ready > 0 && FD_ISSET(sock, &read_fds)) {
            memset(buffer, 0, sizeof(buffer));
            int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (received > 0) {
                printf("Received response:\n%s\n", buffer);
                close(sock);
                return 0;
            }
        }

        printf("Timeout: no response, retrying...\n");
        register_retry++;
    }

    printf("IMS node does not respond\n");
    close(sock);
    return 1;
}
