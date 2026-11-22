#define _POSIX_C_SOURCE 200809L

#include "stream_buffer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static int send_all(int fd, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        ssize_t result = send(fd, data + sent, length - sent, 0);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) return -1;
        sent += (size_t)result;
    }
    return 0;
}

static int connect_to_server(const char *host, const char *port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *addresses = NULL;
    int error = getaddrinfo(host, port, &hints, &addresses);
    if (error != 0) {
        fprintf(stderr, "Cannot resolve %s: %s\n", host, gai_strerror(error));
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *address = addresses; address != NULL; address = address->ai_next) {
        fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd >= 0 && connect(fd, address->ai_addr, address->ai_addrlen) == 0) break;
        if (fd >= 0) close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    return fd;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int socket_fd = connect_to_server(argv[1], argv[2]);
    if (socket_fd < 0) {
        perror("connect");
        return EXIT_FAILURE;
    }
    printf("Connected to %s:%s. Type a message and press Enter; type /quit to exit.\n",
           argv[1], argv[2]);

    ClientSession session;
    memset(&session, 0, sizeof(session));
    session.socket_fd = socket_fd;
    stream_buffer_init(&session);
    int running = 1;

    while (running) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(socket_fd, &readable);
        FD_SET(STDIN_FILENO, &readable);
        int max_fd = socket_fd > STDIN_FILENO ? socket_fd : STDIN_FILENO;

        int ready = select(max_fd + 1, &readable, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(socket_fd, &readable)) {
            char bytes[1024];
            ssize_t received = recv(socket_fd, bytes, sizeof(bytes), 0);
            if (received <= 0) {
                printf("Server disconnected.\n");
                break;
            }
            if (stream_push(&session, bytes, (size_t)received) != STREAM_OK) {
                fprintf(stderr, "Server response exceeded receive buffer.\n");
                break;
            }
            char line[MAX_LINE_LENGTH + 1];
            StreamStatus status;
            while ((status = stream_pop_line(&session, line, sizeof(line))) == STREAM_OK) {
                printf("< %s\n", line);
            }
            if (status != STREAM_NEED_MORE_DATA) {
                fprintf(stderr, "Invalid or oversized server response.\n");
                break;
            }
            fflush(stdout);
        }

        if (FD_ISSET(STDIN_FILENO, &readable)) {
            char line[MAX_LINE_LENGTH + 1];
            if (fgets(line, sizeof(line), stdin) == NULL) break;
            size_t length = strlen(line);
            if (length > 0 && line[length - 1] == '\n') {
                line[--length] = '\0';
                if (length > 0 && line[length - 1] == '\r') line[--length] = '\0';
            } else if (!feof(stdin)) {
                int ch;
                while ((ch = getchar()) != '\n' && ch != EOF) {}
                fprintf(stderr, "Message too long (maximum %d bytes).\n", MAX_LINE_LENGTH - 2);
                continue;
            }
            if (length > MAX_LINE_LENGTH - 2) {
                fprintf(stderr, "Message too long (maximum %d bytes).\n", MAX_LINE_LENGTH - 2);
                continue;
            }
            if (strcmp(line, "/quit") == 0) break;
            line[length++] = '\r';
            line[length++] = '\n';
            if (send_all(socket_fd, line, length) != 0) {
                perror("send");
                running = 0;
            }
        }
    }

    close(socket_fd);
    return EXIT_SUCCESS;
}
