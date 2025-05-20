// author: Jia
// date: 2025-05-19
// client.c - supports REGISTER, INVITE, ACK, BYE with retry timer
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

void send_with_retry(int sock, const char *msg) {
    char buffer[4096];
    int retry = 0;

    while (retry < MAX_RETRIES) {
        printf("Sending (attempt %d)...\n", retry + 1);
        send(sock, msg, strlen(msg), 0);

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
                return;
            }
        }

        printf("Timeout: no response, retrying...\n");
        retry++;
    }

    printf("IMS node does not respond\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <user_id>\n", argv[0]);
        return 1;
    }

    const char *username = argv[1];  // e.g., JohnMiller

    int sock, connected = 0;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

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

    // REGISTER
    char register_msg[1024];
    snprintf(register_msg, sizeof(register_msg),
        "REGISTER sip:yahoo.com SIP/2.0\r\n"
        "Via: SIP/2.0/UDP 127.0.0.1:5060;branch=elelmloperg\r\n"
        "Max-Forwards: 70\r\n"
        "To: <sip:%s@yahoo.com>\r\n"
        "From: <sip:%s@university.edu>;tag=4445556\r\n"
        "Call-ID: 23456678901223\r\n"
        "Cseq: 1 REGISTER\r\n"
        "Contact: <sip:127.0.0.1>\r\n"
        "Expires: 20\r\n"
        "Content-Length: 0\r\n\r\n",
        username, username);

    printf("REGISTER message being sent:\n%s\n", register_msg);
    send_with_retry(sock, register_msg);
    close(sock);

    sleep(1);  // Wait before starting INVITE

    // INVITE
    sock = socket(AF_INET, SOCK_STREAM, 0);
    connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

    const char *sdp =
        "v=0\r\n"
        "o=1073055600 1073055600 IN IP4 127.0.0.1\r\n"
        "s=Session SDP\r\n"
        "c=IN IP4 127.0.0.1\r\n"
        "t=0 0\r\n"
        "m=video 8382 RTP/AVP 98 99\r\n"
        "a=rtpmap:0 PCMU/8000\r\n";
    int content_len = strlen(sdp);

    char invite_msg[2048];
    snprintf(invite_msg, sizeof(invite_msg),
        "INVITE sip:bill@home3.net SIP/2.0\r\n"
        "Via: SIP/2.0/UDP 127.0.0.1:5059\r\n"
        "From: <sip:%s@university.edu>;tag=4445556\r\n"
        "To: <sip:bill@home3.net>;tag=aspyui\r\n"
        "Call-ID: 23456678901223\r\n"
        "CSeq: 1 INVITE\r\n"
        "Contact: <sip:127.0.0.1>\r\n"
        "Content-Type: application/sdp\r\n"
        "Content-Length: %d\r\n\r\n%s",
        username, content_len, sdp);

    printf("INVITE message being sent:\n%s\n", invite_msg);
    send_with_retry(sock, invite_msg);

    // ACK
    char ack_msg[1024];
    snprintf(ack_msg, sizeof(ack_msg),
        "ACK sip:127.0.0.1 SIP/2.0\r\n"
        "Via: SIP/2.0/UDP 127.0.0.1; rport=5060;branch=elelmloperg\r\n"
        "To: <sip:bill@home3.net>;tag=aspyui\r\n"
        "From: <sip:%s@university.edu>;tag=4445556\r\n"
        "Contact: <sip:bill@200.0200.10.33>\r\n"
        "Call-ID: 23456678901223\r\n"
        "CSeq: 1 ACK\r\n"
        "Max-Forwards: 70\r\n"
        "Content-Length: 0\r\n\r\n",
        username);

    printf("ACK message being sent:\n%s\n", ack_msg);
    send(sock, ack_msg, strlen(ack_msg), 0);

    sleep(2);

    // BYE
    char bye_msg[1024];
    snprintf(bye_msg, sizeof(bye_msg),
        "BYE sip:127.0.0.1 SIP/2.0\r\n"
        "Via: SIP/2.0/UDP 127.0.0.1; rport=5060;branch=elelmloperg\r\n"
        "To: <sip:bill@home3.net>;tag=aspyui\r\n"
        "From: <sip:%s@university.edu>;tag=4445556\r\n"
        "Contact: <sip:bill@200.0200.10.33>\r\n"
        "Call-ID: 23456678901223\r\n"
        "CSeq: 1 BYE\r\n"
        "Content-Length: 0\r\n\r\n",
        username);

    printf("BYE message being sent:\n%s\n", bye_msg);
    send(sock, bye_msg, strlen(bye_msg), 0);

    close(sock);
    return 0;
}
