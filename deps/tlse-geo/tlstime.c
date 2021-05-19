/**
 * tls_get_time takes 2RTT
 * threaded_tls_get_time takes 1RTT (Doesn't actually use threads)
 * 
 * Copied from eduardsui's tlsclienthello.c
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "tlse.c"

unsigned char client_message[0xFFFF];

void error(char *msg) {
    perror(msg);
    exit(0);
}

int send_pending(int client_sock, struct TLSContext *context) {
    unsigned int out_buffer_len = 0;
    const unsigned char *out_buffer = tls_get_write_buffer(context, &out_buffer_len);
    unsigned int out_buffer_index = 0;
    int send_res = 0;
    while ((out_buffer) && (out_buffer_len > 0)) {
        int res = send(client_sock, (char *)&out_buffer[out_buffer_index], out_buffer_len, 0);
        if (res <= 0) {
            send_res = res;
            break;
        }
        out_buffer_len -= res;
        out_buffer_index += res;
    }
    tls_buffer_clear(context);
    return send_res;
}

int open_socket(struct sockaddr_in serv_addr) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    return sockfd;
}

void establish_tls(int sockfd, struct TLSContext* context) {
    send_pending(sockfd, context); // Send CLIENT_HELLO
    int read_size;
    while ((read_size = recv(sockfd, client_message, sizeof(client_message) , 0)) > 0) {
        int st = tls_consume_stream(context, client_message, read_size, NULL);
        if (st == TLS_DROP)
            break;
        send_pending(sockfd, context);
    }
}

// @returns the transcript hash
unsigned char* tls_get_time(unsigned char client_random[32], char* anchor) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    signal(SIGPIPE, SIG_IGN);
    portno = 443;
    printf("[TLS] Connecting to %s\n", anchor);
    server = gethostbyname(anchor);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    sockfd = open_socket(serv_addr);

    struct TLSContext *context = tls_create_context(0, TLS_V12);
    my_tls_client_connect(context, client_random);
    establish_tls(sockfd, context);
    unsigned char *hash = malloc(32);
    memcpy(hash, context->transcript_hash, 32);
    DEBUG_DUMP_HEX_LABEL("orig", context->transcript_hash, 32);
    DEBUG_DUMP_HEX_LABEL("new", hash, 32);
    return hash;
}

#define MAX_TLS_CONNECTIONS 200

int sockets[MAX_TLS_CONNECTIONS];
int cur = 0;
int num_sockets;

void threaded_tls_init(char* anchor, int runs) {
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int portno = 443;
    printf("[TLS] Connecting to %s\n", anchor);
    server = gethostbyname(anchor);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    for (int i = 0; i < runs; i++) {
        sockets[i] = open_socket(serv_addr);
    }
    num_sockets = runs;
}

unsigned char* threaded_tls_get_time(unsigned char client_random[32], char* anchor) {
    if (cur >= num_sockets) {
        printf("Not enough sockets (%d %d)\n", cur, num_sockets);
        exit(0);
    }

    struct TLSContext *context = tls_create_context(0, TLS_V12);
    my_tls_client_connect(context, client_random);
    establish_tls(sockets[cur++], context);
    unsigned char *hash = malloc(32);
    memcpy(hash, context->transcript_hash, 32);
    DEBUG_DUMP_HEX_LABEL("orig", context->transcript_hash, 32);
    DEBUG_DUMP_HEX_LABEL("new", hash, 32);
    return hash;
}

void threaded_tls_close() {
    for (int i = 0; i < num_sockets; i++) {
        close(sockets[i]);
    }
}