#include "ib-sock.h"

static int __init
srv_init(void)
{
	struct IB_SOCK *sock;

	sock = ib_socket_create();
	if (sock == NULL)
		return -ENOMEM;

	ib_socket_destroy(sock);
	return 0;
}

static void __exit
srv_cleanup(void)
{
}

module_init(srv_init);
module_exit(srv_cleanup);

MODULE_LICENSE("GPL");