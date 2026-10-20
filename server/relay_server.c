#define _DEFAULT_SOURCE

#include "../src/cJSON.h"

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

#define RELAY_PORT 6818
#define MAX_CLIENTS 64
#define RX_BUFFER 2048
#define ROOM_LENGTH 32

typedef struct Client Client;
struct Client {
    int fd;
    int joined;
    char room[ROOM_LENGTH];
    char rx[RX_BUFFER];
    size_t rx_length;
    Client *peer;
};

static Client clients[MAX_CLIENTS];
static volatile sig_atomic_t running = 1;

static void stop_server(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void reset_client(Client *client)
{
    if (client->fd >= 0) close(client->fd);
    memset(client, 0, sizeof(*client));
    client->fd = -1;
}

static int send_all(int fd, const char *text, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t count = send(fd, text + sent, length - sent, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        sent += (size_t)count;
    }
    return 0;
}

static int send_json(int fd, cJSON *root)
{
    char *text = cJSON_PrintUnformatted(root);
    int result;
    if (!text) return -1;
    result = send_all(fd, text, strlen(text));
    if (result == 0) result = send_all(fd, "\n", 1);
    cJSON_free(text);
    return result;
}

static int send_disconnect_message(int fd)
{
    cJSON *root = cJSON_CreateObject();
    int result;
    if (!root) return -1;
    cJSON_AddStringToObject(root, "type", "disconnect");
    result = send_json(fd, root);
    cJSON_Delete(root);
    return result;
}

static int send_hello(Client *client, int color)
{
    cJSON *root = cJSON_CreateObject();
    int result;
    if (!root) return -1;
    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddStringToObject(root, "color", color == 1 ? "red" : "black");
    result = send_json(client->fd, root);
    cJSON_Delete(root);
    return result;
}

static Client *find_waiting_client(const char *room, const Client *exclude)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (&clients[i] != exclude && clients[i].fd >= 0 && clients[i].joined &&
            clients[i].peer == NULL && strcmp(clients[i].room, room) == 0) return &clients[i];
    }
    return NULL;
}

static void pair_client(Client *client, const char *room)
{
    Client *waiting;
    snprintf(client->room, sizeof(client->room), "%s", room);
    client->joined = 1;
    waiting = find_waiting_client(room, client);
    if (!waiting) return;
    client->peer = waiting;
    waiting->peer = client;
    if (send_hello(waiting, 1) != 0 || send_hello(client, -1) != 0) {
        waiting->peer = NULL;
        client->peer = NULL;
        reset_client(waiting);
        reset_client(client);
        fprintf(stderr, "room=%s pairing failed while sending hello\n", room);
        return;
    }
    fprintf(stdout, "room=%s paired fd=%d(red) fd=%d(black)\n", room, waiting->fd, client->fd);
    fflush(stdout);
}

static void relay_line(Client *client, const char *line, size_t length)
{
    Client *peer = client->peer;
    if (!peer || peer->fd < 0) return;
    if (send_all(peer->fd, line, length) != 0 || send_all(peer->fd, "\n", 1) != 0) {
        client->peer = NULL;
        peer->peer = NULL;
        reset_client(peer);
        if (send_disconnect_message(client->fd) != 0) reset_client(client);
    }
}

static void handle_line(Client *client, const char *line, size_t length)
{
    cJSON *root = cJSON_ParseWithLength(line, length);
    cJSON *type;
    if (!root) return;
    type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (cJSON_IsString(type) && strcmp(type->valuestring, "join") == 0 && client->peer == NULL) {
        cJSON *room = cJSON_GetObjectItemCaseSensitive(root, "room");
        if (cJSON_IsString(room) && room->valuestring[0] != '\0' && strlen(room->valuestring) < ROOM_LENGTH) {
            pair_client(client, room->valuestring);
        }
    } else if (client->joined && client->peer && cJSON_IsString(type)) {
        const char *name = type->valuestring;
        if (strcmp(name, "move") == 0 || strcmp(name, "result") == 0 ||
            strcmp(name, "resign") == 0 || strcmp(name, "draw") == 0 ||
            strcmp(name, "disconnect") == 0) {
            relay_line(client, line, length);
        }
    }
    cJSON_Delete(root);
}

static int read_client(Client *client)
{
    char buffer[512];
    ssize_t received = recv(client->fd, buffer, sizeof(buffer), 0);
    if (received < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    if (received <= 0) return -1;
    if (client->rx_length + (size_t)received > sizeof(client->rx)) return -1;
    memcpy(client->rx + client->rx_length, buffer, (size_t)received);
    client->rx_length += (size_t)received;
    for (;;) {
        char *newline = memchr(client->rx, '\n', client->rx_length);
        size_t line_length;
        if (!newline) return 0;
        line_length = (size_t)(newline - client->rx);
        handle_line(client, client->rx, line_length);
        memmove(client->rx, newline + 1, client->rx_length - line_length - 1);
        client->rx_length -= line_length + 1;
    }
}

static void disconnect_client(Client *client)
{
    Client *peer = client->peer;
    if (peer) {
        send_disconnect_message(peer->fd);
        peer->peer = NULL;
    }
    reset_client(client);
}

int main(int argc, char **argv)
{
    int listen_fd;
    long requested_port = RELAY_PORT;
    int port;
    struct sockaddr_in address;
    int reuse = 1;
    int i;
    if (argc > 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        return 2;
    }
    if (argc > 1) {
        char *end = NULL;
        requested_port = strtol(argv[1], &end, 10);
        if (!end || *end != '\0' || requested_port < 1 || requested_port > 65535) {
            fprintf(stderr, "invalid port: %s\n", argv[1]);
            return 2;
        }
    }
    port = (int)requested_port;
    signal(SIGINT, stop_server);
    signal(SIGTERM, stop_server);
    for (i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) != 0 || listen(listen_fd, 16) != 0) {
        close(listen_fd);
        return 1;
    }
    printf("xiangqi relay listening on 0.0.0.0:%d\n", port);
    while (running) {
        fd_set read_set;
        int max_fd = listen_fd;
        FD_ZERO(&read_set);
        FD_SET(listen_fd, &read_set);
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0) {
                FD_SET(clients[i].fd, &read_set);
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
            }
        }
        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (FD_ISSET(listen_fd, &read_set)) {
            int fd = accept(listen_fd, NULL, NULL);
            if (fd >= 0) {
                int added = 0;
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd < 0) {
                        clients[i].fd = fd;
                        added = 1;
                        break;
                    }
                }
                if (!added) close(fd);
            }
        }
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0 && FD_ISSET(clients[i].fd, &read_set) && read_client(&clients[i]) != 0) {
                disconnect_client(&clients[i]);
            }
        }
    }
    for (i = 0; i < MAX_CLIENTS; i++) if (clients[i].fd >= 0) disconnect_client(&clients[i]);
    close(listen_fd);
    return 0;
}
