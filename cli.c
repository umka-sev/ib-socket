#include "ib-sock.h"

static unsigned int port = 10000;
const char *srv_addr = "192.168.1.1";
static __u32 addr  = 0;


static int __init
cli_init(void)
{
	struct IB_SOCK *sock;

	sock = ib_socket_create();
	if (sock == NULL)
		return -ENOMEM;

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