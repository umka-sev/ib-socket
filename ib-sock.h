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

int ib_socket_bind(struct IB_SOCK *sock, uint32_t addr, unsigned port);
struct IB_SOCK *ib_socket_accept(struct IB_SOCK *parent);

/* return a size of transfer */
size_t ib_socket_read_size(struct IB_SOCK *sock);
/* submit to read.
 * read protocol
 * first POLLIN event have result to call ib_socket_read_try to get a buffer size
 *  and call of ib_socket_read will send request to read and attach a buffers
 * but return a EAGAIN to wait a transfer done
 * next POLLIN event will inducate a transfer done  */
int ib_socket_read(struct IB_SOCK *sock, void *buf, size_t size);


/* send some amount data over wire */
int ib_socket_write(struct IB_SOCK *sock, void *buf, size_t size);

#endif