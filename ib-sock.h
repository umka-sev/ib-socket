#ifndef _IB_SOCK_H_
#define _IB_SOCK_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/types.h>
#include <linux/net.h>

#include <linux/socket.h>
#include <linux/in.h>

enum sock_state {
	IBS_UNKNOW,
	IBS_ERROR,
	IBS_ADDR_RESOLVED,
	IBS_ROUTE_RESOLVED,
	IBS_CONNECTED,
	IBS_DATA_READY,
	IBS_DISCONNECTED,
	IBS_IDLE,
};

struct IB_SOCK *ib_socket_create(void);
void ib_socket_destroy(struct IB_SOCK *sock);

/* blocked until connect really started */
int ib_socket_connect(struct IB_SOCK *sock, struct sockaddr_in  *dstaddr);
void ib_socket_disconnect(struct IB_SOCK *sock);

int ib_socket_bind(struct IB_SOCK *sock, unsigned port);
struct IB_SOCK *ib_socket_accept(struct IB_SOCK *sock);

enum sock_state ib_socket_pool(struct IB_SOCK *sock);

int ib_socket_send(struct IB_SOCK *sock, void *data, size_t d_size);
int ib_socket_recv(struct IB_SOCK *sock, void *data, size_t d_size);

#endif