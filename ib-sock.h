#ifndef _IB_SOCK_H_
#define _IB_SOCK_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/types.h>
#include <linux/net.h>

#include <linux/socket.h>
#include <linux/in.h>

struct IB_SOCK;

struct IB_SOCK *ib_socket_create(void);
void ib_socket_destroy(struct IB_SOCK *sock);

#endif