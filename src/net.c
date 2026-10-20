#include "net.h"

#include "cJSON.h"

#include <stdio.h>
#include <signal.h>
#include <string.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#define NET_CLOSE closesocket
#define NET_ERRNO WSAGetLastError()
#define NET_WOULDBLOCK WSAEWOULDBLOCK
#define NET_IN_PROGRESS WSAEINPROGRESS
#define NET_INTERRUPTED WSAEINTR
#define NET_SEND_FLAGS 0
static int net_socket_error(void) { return WSAGetLastError(); }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#define NET_CLOSE close
#define NET_ERRNO errno
#define NET_WOULDBLOCK EWOULDBLOCK
#define NET_IN_PROGRESS EINPROGRESS
#define NET_INTERRUPTED EINTR
#define NET_SEND_FLAGS MSG_NOSIGNAL
static int net_socket_error(void) { return errno; }
#endif

static net_socket_t invalid_socket(void)
{
#ifdef _WIN32
    return INVALID_SOCKET;
#else
    return -1;
#endif
}

static int socket_is_valid(net_socket_t socket_value)
{
    return socket_value != invalid_socket();
}

static int set_nonblocking(net_socket_t socket_value)
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(socket_value, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(socket_value, F_GETFL, 0);
    return flags < 0 ? -1 : fcntl(socket_value, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int wait_writable(net_socket_t socket_value, int timeout_ms)
{
    fd_set write_set;
    struct timeval timeout;
    FD_ZERO(&write_set);
    FD_SET(socket_value, &write_set);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    return select((int)socket_value + 1, NULL, &write_set, NULL, &timeout) > 0 ? 0 : -1;
}

static int send_all(net_socket_t socket_value, const char *data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        int result = send(socket_value, data + sent, (int)(length - sent), NET_SEND_FLAGS);
        if (result > 0) {
            sent += (size_t)result;
            continue;
        }
        if (result < 0 && net_socket_error() == NET_INTERRUPTED) continue;
        if (result < 0 && net_socket_error() == NET_WOULDBLOCK && wait_writable(socket_value, 2000) == 0) continue;
        return -1;
    }
    return 0;
}

static int valid_move_coordinates(int from_x, int from_y, int to_x, int to_y)
{
    return from_x >= 0 && from_x < 9 && to_x >= 0 && to_x < 9 &&
           from_y >= 0 && from_y < 10 && to_y >= 0 && to_y < 10;
}

int net_init(void)
{
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0 ? 0 : -1;
#else
    signal(SIGPIPE, SIG_IGN);
    return 0;
#endif
}

void net_context_init(NetContext *context)
{
    if (!context) return;
    memset(context, 0, sizeof(*context));
    context->listen_socket = invalid_socket();
    context->socket = invalid_socket();
}

int net_host_start(NetContext *context, unsigned short port)
{
    struct sockaddr_in address;
    int reuse = 1;
    if (!context) return -1;
    net_context_init(context);
    context->listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (!socket_is_valid(context->listen_socket)) return -1;
    setsockopt(context->listen_socket, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&reuse, (socklen_t)sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(context->listen_socket, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(context->listen_socket, 1) != 0 || set_nonblocking(context->listen_socket) != 0) {
        net_close(context);
        return -1;
    }
    context->role = NET_ROLE_HOST;
    return 0;
}

int net_host_accept(NetContext *context)
{
    struct sockaddr_in address;
    socklen_t length = (socklen_t)sizeof(address);
    net_socket_t client;
    if (!context || !socket_is_valid(context->listen_socket)) return -1;
    client = accept(context->listen_socket, (struct sockaddr *)&address, &length);
    if (!socket_is_valid(client)) {
        int error = net_socket_error();
        if (error == NET_WOULDBLOCK || error == NET_INTERRUPTED) return 0;
        return -1;
    }
    if (set_nonblocking(client) != 0) {
        NET_CLOSE(client);
        return -1;
    }
    if (socket_is_valid(context->socket)) NET_CLOSE(context->socket);
    context->socket = client;
    context->rx_length = 0;
    context->connected = 1;
    return 1;
}

int net_client_connect(NetContext *context, const char *ip, unsigned short port)
{
    struct sockaddr_in address;
    if (!context || !ip) return -1;
    net_context_init(context);
    context->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (!socket_is_valid(context->socket)) return -1;
    if (set_nonblocking(context->socket) != 0) {
        net_close(context);
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &address.sin_addr) != 1) {
        net_close(context);
        return -1;
    }
    if (connect(context->socket, (struct sockaddr *)&address, sizeof(address)) != 0) {
        int error = net_socket_error();
        if (error == NET_IN_PROGRESS || error == NET_WOULDBLOCK) {
            int socket_error = 0;
            socklen_t socket_error_length = (socklen_t)sizeof(socket_error);
            if (wait_writable(context->socket, 5000) != 0 ||
                getsockopt(context->socket, SOL_SOCKET, SO_ERROR,
                           (char *)&socket_error, &socket_error_length) != 0 || socket_error != 0) {
                net_close(context);
                return -1;
            }
        } else {
            net_close(context);
            return -1;
        }
    }
    context->role = NET_ROLE_CLIENT;
    context->connected = 1;
    return 0;
}

static int send_json(NetContext *context, cJSON *root)
{
    char *text;
    int result;
    if (!context || !context->connected || !root) return -1;
    text = cJSON_PrintUnformatted(root);
    if (!text) return -1;
    result = send_all(context->socket, text, strlen(text));
    if (result == 0) result = send_all(context->socket, "\n", 1);
    cJSON_free(text);
    if (result != 0) context->connected = 0;
    return result;
}

int net_send_hello(NetContext *context, int color)
{
    cJSON *root = cJSON_CreateObject();
    int result;
    if (!root || (color != 1 && color != -1)) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddStringToObject(root, "color", color == 1 ? "red" : "black");
    result = send_json(context, root);
    cJSON_Delete(root);
    return result;
}

int net_send_join(NetContext *context, const char *room)
{
    cJSON *root = cJSON_CreateObject();
    int result;
    if (!root || !room || room[0] == '\0' || strlen(room) >= 32) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_AddStringToObject(root, "type", "join");
    cJSON_AddStringToObject(root, "room", room);
    result = send_json(context, root);
    cJSON_Delete(root);
    return result;
}

int net_send_move(NetContext *context, int from_x, int from_y, int to_x, int to_y)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *from = cJSON_CreateArray();
    cJSON *to = cJSON_CreateArray();
    int result;
    if (!root || !from || !to || !valid_move_coordinates(from_x, from_y, to_x, to_y)) {
        cJSON_Delete(root);
        cJSON_Delete(from);
        cJSON_Delete(to);
        return -1;
    }
    cJSON_AddStringToObject(root, "type", "move");
    cJSON_AddItemToArray(from, cJSON_CreateNumber(from_x));
    cJSON_AddItemToArray(from, cJSON_CreateNumber(from_y));
    cJSON_AddItemToArray(to, cJSON_CreateNumber(to_x));
    cJSON_AddItemToArray(to, cJSON_CreateNumber(to_y));
    cJSON_AddItemToObject(root, "from", from);
    cJSON_AddItemToObject(root, "to", to);
    result = send_json(context, root);
    cJSON_Delete(root);
    return result;
}

int net_send_result(NetContext *context, int winner)
{
    cJSON *root = cJSON_CreateObject();
    int result;
    if (!root || (winner != 1 && winner != -1 && winner != 0)) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_AddStringToObject(root, "type", "result");
    cJSON_AddStringToObject(root, "winner", winner == 1 ? "red" : (winner == -1 ? "black" : "draw"));
    result = send_json(context, root);
    cJSON_Delete(root);
    return result;
}

int net_send_simple(NetContext *context, NetMessageType type, int color)
{
    const char *name = type == NET_MSG_RESIGN ? "resign" : (type == NET_MSG_DRAW ? "draw" : "disconnect");
    cJSON *root;
    int result;
    if ((type != NET_MSG_RESIGN && type != NET_MSG_DRAW && type != NET_MSG_DISCONNECT) ||
        ((type == NET_MSG_RESIGN || type == NET_MSG_DRAW) && color != 1 && color != -1)) return -1;
    root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "type", name);
    if (type != NET_MSG_DISCONNECT) cJSON_AddStringToObject(root, "color", color == 1 ? "red" : "black");
    result = send_json(context, root);
    cJSON_Delete(root);
    return result;
}

static int parse_line(const char *line, size_t length, NetMessage *message)
{
    cJSON *root;
    cJSON *item;
    const char *type;
    if (!line || !message) return -1;
    root = cJSON_ParseWithLength(line, length);
    if (!root) return -1;
    memset(message, 0, sizeof(*message));
    item = cJSON_GetObjectItemCaseSensitive(root, "type");
    type = cJSON_IsString(item) ? item->valuestring : "";
    if (strcmp(type, "hello") == 0) {
        cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
        cJSON *color = cJSON_GetObjectItemCaseSensitive(root, "color");
        if (!cJSON_IsNumber(version) || version->valuedouble != version->valueint || version->valueint != 1 ||
            !cJSON_IsString(color) || (strcmp(color->valuestring, "red") != 0 &&
                                       strcmp(color->valuestring, "black") != 0)) {
            cJSON_Delete(root);
            return -1;
        }
        message->type = NET_MSG_HELLO;
        message->color = strcmp(color->valuestring, "red") == 0 ? 1 : -1;
    } else if (strcmp(type, "move") == 0) {
        cJSON *from = cJSON_GetObjectItemCaseSensitive(root, "from");
        cJSON *to = cJSON_GetObjectItemCaseSensitive(root, "to");
        cJSON *from_x;
        cJSON *from_y;
        cJSON *to_x;
        cJSON *to_y;
        if (!cJSON_IsArray(from) || !cJSON_IsArray(to) || cJSON_GetArraySize(from) != 2 || cJSON_GetArraySize(to) != 2) {
            cJSON_Delete(root);
            return -1;
        }
        from_x = cJSON_GetArrayItem(from, 0);
        from_y = cJSON_GetArrayItem(from, 1);
        to_x = cJSON_GetArrayItem(to, 0);
        to_y = cJSON_GetArrayItem(to, 1);
        message->type = NET_MSG_MOVE;
        if (!cJSON_IsNumber(from_x) || !cJSON_IsNumber(from_y) ||
            !cJSON_IsNumber(to_x) || !cJSON_IsNumber(to_y) ||
            from_x->valuedouble != from_x->valueint || from_y->valuedouble != from_y->valueint ||
            to_x->valuedouble != to_x->valueint || to_y->valuedouble != to_y->valueint ||
            !valid_move_coordinates(from_x->valueint, from_y->valueint, to_x->valueint, to_y->valueint)) {
            cJSON_Delete(root);
            return -1;
        }
        message->from_x = from_x->valueint;
        message->from_y = from_y->valueint;
        message->to_x = to_x->valueint;
        message->to_y = to_y->valueint;
    } else if (strcmp(type, "result") == 0) {
        cJSON *winner = cJSON_GetObjectItemCaseSensitive(root, "winner");
        if (!cJSON_IsString(winner) || (strcmp(winner->valuestring, "red") != 0 &&
                                        strcmp(winner->valuestring, "black") != 0 &&
                                        strcmp(winner->valuestring, "draw") != 0)) {
            cJSON_Delete(root);
            return -1;
        }
        message->type = NET_MSG_RESULT;
        message->winner = strcmp(winner->valuestring, "red") == 0
                              ? 1 : (strcmp(winner->valuestring, "black") == 0 ? -1 : 0);
    } else if (strcmp(type, "resign") == 0) {
        cJSON *color = cJSON_GetObjectItemCaseSensitive(root, "color");
        if (!cJSON_IsString(color) || (strcmp(color->valuestring, "red") != 0 &&
                                       strcmp(color->valuestring, "black") != 0)) {
            cJSON_Delete(root);
            return -1;
        }
        message->type = NET_MSG_RESIGN;
        message->color = strcmp(color->valuestring, "red") == 0 ? 1 : -1;
    } else if (strcmp(type, "draw") == 0) {
        cJSON *color = cJSON_GetObjectItemCaseSensitive(root, "color");
        if (!cJSON_IsString(color) || (strcmp(color->valuestring, "red") != 0 &&
                                       strcmp(color->valuestring, "black") != 0)) {
            cJSON_Delete(root);
            return -1;
        }
        message->type = NET_MSG_DRAW;
        message->color = strcmp(color->valuestring, "red") == 0 ? 1 : -1;
    } else if (strcmp(type, "disconnect") == 0) {
        message->type = NET_MSG_DISCONNECT;
    } else {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_Delete(root);
    return 0;
}

int net_poll(NetContext *context, NetMessage *message)
{
    char buffer[512];
    int received;
    if (!context || !message || !context->connected) return -1;
    for (;;) {
        char *newline = (char *)memchr(context->rx_buffer, '\n', context->rx_length);
        size_t line_length;
        if (newline) {
            line_length = (size_t)(newline - context->rx_buffer);
            if (parse_line(context->rx_buffer, line_length, message) == 0) {
                size_t consumed = line_length + 1;
                memmove(context->rx_buffer, context->rx_buffer + consumed, context->rx_length - consumed);
                context->rx_length -= consumed;
                return 1;
            }
            memmove(context->rx_buffer, newline + 1, context->rx_length - line_length - 1);
            context->rx_length -= line_length + 1;
            continue;
        }
        received = recv(context->socket, buffer, (int)sizeof(buffer), 0);
        if (received == 0) {
            context->connected = 0;
            message->type = NET_MSG_DISCONNECT;
            return 1;
        }
        if (received < 0) {
            int error = net_socket_error();
            if (error == NET_INTERRUPTED) continue;
            if (error == NET_WOULDBLOCK) return 0;
            context->connected = 0;
            message->type = NET_MSG_DISCONNECT;
            return 1;
        }
        if (context->rx_length + (size_t)received > sizeof(context->rx_buffer)) {
            context->rx_length = 0;
            context->connected = 0;
            message->type = NET_MSG_DISCONNECT;
            return 1;
        }
        memcpy(context->rx_buffer + context->rx_length, buffer, (size_t)received);
        context->rx_length += (size_t)received;
    }
}

void net_close(NetContext *context)
{
    if (!context) return;
    if (socket_is_valid(context->socket)) NET_CLOSE(context->socket);
    if (socket_is_valid(context->listen_socket)) NET_CLOSE(context->listen_socket);
    context->socket = invalid_socket();
    context->listen_socket = invalid_socket();
    context->role = NET_ROLE_NONE;
    context->connected = 0;
    context->rx_length = 0;
}

const char *net_last_error(void)
{
    static char text[64];
    snprintf(text, sizeof(text), "socket error %d", NET_ERRNO);
    return text;
}
