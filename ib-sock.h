#ifndef _IB_SOCK_H_
#define _IB_SOCK_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/types.h>
#include <linux/net.h>

#include <linux/socket.h>
#include <linux/in.h>

#include <linux/poll.h>

struct IB_SOCK;

struct IB_SOCK *ib_socket_create(void);
void ib_socket_destroy(struct IB_SOCK *sock);

/* async. return POLLOUT or POLLERR events */
int ib_socket_connect(struct IB_SOCK *sock, struct sockaddr_in  *dstaddr);
void ib_socket_disconnect(struct IB_SOCK *sock);

/* sleep to wait poll event from socket */
unsigned long ib_socket_poll(struct IB_SOCK *sock);

#endif