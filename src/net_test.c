#include "net.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
static void test_sleep(void)
{
    Sleep(10);
}
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
static void test_sleep(void)
{
    struct timespec delay = {0, 10000000L};
    nanosleep(&delay, NULL);
}
#endif

static unsigned short listening_port(net_socket_t socket_value)
{
    struct sockaddr_in address;
#ifdef _WIN32
    int length = (int)sizeof(address);
#else
    socklen_t length = (socklen_t)sizeof(address);
#endif
    memset(&address, 0, sizeof(address));
    assert(getsockname(socket_value, (struct sockaddr *)&address, &length) == 0);
    return ntohs(address.sin_port);
}

static int send_raw(net_socket_t socket_value, const char *text)
{
    size_t length = strlen(text);
    size_t sent = 0;
    while (sent < length) {
        int count = send(socket_value, text + sent, (int)(length - sent), 0);
        if (count <= 0) return -1;
        sent += (size_t)count;
    }
    return 0;
}

static int poll_until(NetContext *context, NetMessage *message)
{
    int i;
    for (i = 0; i < 100; i++) {
        int result = net_poll(context, message);
        if (result != 0) return result;
        test_sleep();
    }
    return 0;
}

int main(void)
{
    NetContext host;
    NetContext client;
    NetMessage message;
    unsigned short port;

    assert(net_init() == 0);
    net_context_init(&host);
    net_context_init(&client);
    assert(net_host_start(&host, 0) == 0);
    port = listening_port(host.listen_socket);
    assert(net_client_connect(&client, "127.0.0.1", port) == 0);

    {
        NetContext invalid_client;
        net_context_init(&invalid_client);
        assert(net_client_connect(&invalid_client, "not-an-ip", port) != 0);
        net_close(&invalid_client);
    }

    while (net_host_accept(&host) == 0) test_sleep();
    assert(host.connected);

    assert(net_send_hello(&host, 1) == 0);
    assert(poll_until(&client, &message) == 1);
    assert(message.type == NET_MSG_HELLO && message.color == 1);

    assert(net_send_move(&client, 1, 9, 2, 7) == 0);
    assert(poll_until(&host, &message) == 1);
    assert(message.type == NET_MSG_MOVE);
    assert(message.from_x == 1 && message.from_y == 9 && message.to_x == 2 && message.to_y == 7);

    assert(send_raw(client.socket, "{\"type\":\"hello\",\"version\":2,\"color\":\"red\"}\n") == 0);
    assert(send_raw(client.socket, "{\"type\":\"hello\",\"version\":1,\"color\":\"purple\"}\n") == 0);
    assert(send_raw(client.socket, "{\"type\":\"move\",\"from\":[99,0],\"to\":[0,0]}\n") == 0);
    assert(net_send_result(&client, 0) == 0);
    assert(poll_until(&host, &message) == 1);
    assert(message.type == NET_MSG_RESULT && message.winner == 0);

    assert(net_send_simple(&client, NET_MSG_RESIGN, -1) == 0);
    assert(poll_until(&host, &message) == 1);
    assert(message.type == NET_MSG_RESIGN && message.color == -1);

    assert(net_send_simple(&client, NET_MSG_DRAW, -1) == 0);
    assert(poll_until(&host, &message) == 1);
    assert(message.type == NET_MSG_DRAW && message.color == -1);
    assert(net_send_simple(&client, NET_MSG_DRAW, 0) != 0);

    assert(net_send_join(&client, "room-with-32-characters-123456789") != 0);
    assert(net_send_move(&client, -1, 0, 0, 0) != 0);
    assert(net_send_simple(&client, NET_MSG_NONE, 1) != 0);

    net_close(&client);
    assert(poll_until(&host, &message) == 1);
    assert(message.type == NET_MSG_DISCONNECT);
    net_close(&host);
    puts("network tests passed");
    return 0;
}
