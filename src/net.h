#ifndef CHINESE_NET_H
#define CHINESE_NET_H

#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET net_socket_t;
#else
typedef int net_socket_t;
#endif

#define NET_DEFAULT_PORT 8888
#define NET_RX_BUFFER 2048

typedef enum {
    NET_ROLE_NONE = 0,
    NET_ROLE_HOST,
    NET_ROLE_CLIENT
} NetRole;

typedef enum {
    NET_MSG_NONE = 0,
    NET_MSG_HELLO,
    NET_MSG_MOVE,
    NET_MSG_RESULT,
    NET_MSG_RESIGN,
    NET_MSG_DRAW,
    NET_MSG_DISCONNECT
} NetMessageType;

typedef struct {
    NetMessageType type;
    int from_x;
    int from_y;
    int to_x;
    int to_y;
    int color;
    int winner;
} NetMessage;

typedef struct {
    net_socket_t listen_socket;
    net_socket_t socket;
    NetRole role;
    int connected;
    char rx_buffer[NET_RX_BUFFER];
    size_t rx_length;
} NetContext;

int net_init(void);
void net_context_init(NetContext *context);
int net_host_start(NetContext *context, unsigned short port);
int net_host_accept(NetContext *context);
int net_client_connect(NetContext *context, const char *ip, unsigned short port);
int net_send_join(NetContext *context, const char *room);
int net_send_hello(NetContext *context, int color);
int net_send_move(NetContext *context, int from_x, int from_y, int to_x, int to_y);
int net_send_result(NetContext *context, int winner);
int net_send_simple(NetContext *context, NetMessageType type, int color);
int net_poll(NetContext *context, NetMessage *message);
void net_close(NetContext *context);
const char *net_last_error(void);

#endif
