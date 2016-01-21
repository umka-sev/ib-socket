#include "ib-sock.h"

#include <linux/delay.h>

static unsigned int port = 1998;
const char *srv_addr = "172.18.56.132";
static __u32 addr  = 0;


static int __init
cli_init(void)
{
	struct IB_SOCK *sock;
	unsigned a,b,c,d;
	struct sockaddr_in  dstaddr;
	unsigned long event;
	int err;

	/* numeric IP? */
	if (sscanf(srv_addr, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
		return -EINVAL;

	addr = ((a<<24)|(b<<16)|(c<<8)|d);

	sock = ib_socket_create();
	if (sock == NULL)
		return -ENOMEM;

	memset(&dstaddr, 0, sizeof(dstaddr));
	dstaddr.sin_family = AF_INET;
	dstaddr.sin_addr.s_addr = htonl(addr);
	dstaddr.sin_port = htons(port);

	err = ib_socket_connect(sock, &dstaddr);
	if (err) {
		printk("error connect \n");
		goto exit;
	}
	while ( 1 ) {
		event = ib_socket_poll(sock);
		printk("Event hit %lx\n", event);
		if (event & POLLERR)
			break;
		if (event & POLLOUT) {
			err = ib_socket_write(sock, srv_addr, strlen(srv_addr));
			printk("sends status %d\n", err);
		}
	}

	ib_socket_disconnect(sock);
exit:
	ib_socket_destroy(sock);
	return 0;
}

static void __exit
cli_cleanup(void)
{
}

module_init(cli_init);
module_exit(cli_cleanup);

MODULE_LICENSE("GPL");
