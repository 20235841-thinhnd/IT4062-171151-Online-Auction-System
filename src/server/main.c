#define _POSIX_C_SOURCE 200809L

#include "logger.h"
#include "stream_buffer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    ClientSession session;
    bool active;
} ConnectedClient;

static ConnectedClient clients[MAX_CLIENTS];
static volatile sig_atomic_t stopping;

static void handle_signal(int signal_number) {
    (void)signal_number;
    stopping = 1;
}

static int send_all(int fd, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
#ifdef MSG_NOSIGNAL
        ssize_t result = send(fd, data + sent, length - sent, MSG_NOSIGNAL);
#else
        ssize_t result = send(fd, data + sent, length - sent, 0);
#endif
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) return -1;
        sent += (size_t)result;
    }
    return 0;
}

static void disconnect_client(size_t index) {
    if (!clients[index].active) return;
    int fd = clients[index].session.socket_fd;
    logger_log(LOG_LEVEL_INFO, "Client disconnected (fd=%d)", fd);
    close(fd);
    memset(&clients[index], 0, sizeof(clients[index]));
    clients[index].session.socket_fd = -1;
}

static int process_client_data(size_t index, const char *data, size_t length) {
    ClientSession *session = &clients[index].session;
    StreamStatus status = stream_push(session, data, length);
    if (status != STREAM_OK) {
        const char *error = "ERR|4002|input buffer limit exceeded\r\n";
        (void)send_all(session->socket_fd, error, strlen(error));
        return -1;
    }

    char line[MAX_LINE_LENGTH + 1];
    while ((status = stream_pop_line(session, line, sizeof(line))) == STREAM_OK) {
        logger_log(LOG_LEVEL_INFO, "Received from fd=%d: %s", session->socket_fd, line);
        char response[MAX_LINE_LENGTH + 16];
        int response_length;
        if (strcmp(line, "PING") == 0) {
            response_length = snprintf(response, sizeof(response), "PONG\r\n");
        } else {
            response_length = snprintf(response, sizeof(response), "ECHO|%s\r\n", line);
        }
        if (response_length < 0 || (size_t)response_length >= sizeof(response) ||
            send_all(session->socket_fd, response, (size_t)response_length) != 0) {
            return -1;
        }
    }

    return status == STREAM_NEED_MORE_DATA ? 0 : -1;
}

static int create_listener(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int reuse = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 || listen(fd, MAX_CLIENTS) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void accept_client(int listener) {
    struct sockaddr_in address;
    socklen_t address_length = sizeof(address);
    int fd = accept(listener, (struct sockaddr *)&address, &address_length);
    if (fd < 0) {
        if (errno != EINTR) logger_log(LOG_LEVEL_WARN, "accept failed: %s", strerror(errno));
        return;
    }

    size_t index;
    for (index = 0; index < MAX_CLIENTS && clients[index].active; index++) {}
    if (index == MAX_CLIENTS) {
        const char *full = "ERR|5000|server full\r\n";
        (void)send_all(fd, full, strlen(full));
        close(fd);
        return;
    }

    memset(&clients[index], 0, sizeof(clients[index]));
    clients[index].active = true;
    clients[index].session.socket_fd = fd;
    clients[index].session.current_room_id = -1;
    (void)inet_ntop(AF_INET, &address.sin_addr, clients[index].session.ip_addr,
                    sizeof(clients[index].session.ip_addr));
    clients[index].session.port = ntohs(address.sin_port);
    stream_buffer_init(&clients[index].session);
    logger_log(LOG_LEVEL_INFO, "Client connected from %s:%d (fd=%d)",
               clients[index].session.ip_addr, clients[index].session.port, fd);
}

int main(int argc, char **argv) {
    char *end = NULL;
    long port = DEFAULT_PORT;
    if (argc > 2 || (argc == 2 && (port = strtol(argv[1], &end, 10),
                                    end == argv[1] || *end != '\0' || port < 1 || port > 65535))) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!logger_init("logs/server.log")) {
        fprintf(stderr, "Cannot open logs/server.log (create the logs directory first)\n");
    }
    int listener = create_listener((int)port);
    if (listener < 0) {
        logger_log(LOG_LEVEL_ERROR, "Could not listen on port %ld: %s", port, strerror(errno));
        logger_close();
        return EXIT_FAILURE;
    }
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    logger_log(LOG_LEVEL_INFO, "TCP echo server listening on port %ld", port);

    while (!stopping) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listener, &readable);
        int max_fd = listener;
        for (size_t i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                int fd = clients[i].session.socket_fd;
                FD_SET(fd, &readable);
                if (fd > max_fd) max_fd = fd;
            }
        }

        int ready = select(max_fd + 1, &readable, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            logger_log(LOG_LEVEL_ERROR, "select failed: %s", strerror(errno));
            break;
        }
        if (FD_ISSET(listener, &readable)) accept_client(listener);
        for (size_t i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) continue;
            int fd = clients[i].session.socket_fd;
            if (!FD_ISSET(fd, &readable)) continue;
            char buffer[1024];
            ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
            if (received <= 0 || process_client_data(i, buffer, (size_t)received) != 0) {
                disconnect_client(i);
            }
        }
    }

    for (size_t i = 0; i < MAX_CLIENTS; i++) disconnect_client(i);
    close(listener);
    logger_log(LOG_LEVEL_INFO, "Server stopped");
    logger_close();
    return EXIT_SUCCESS;
}
