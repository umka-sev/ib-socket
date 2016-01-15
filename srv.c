#include "ib-sock.h"



static int __init
srv_init(void)
{
	struct IB_SOCK *sock, *sock_child = NULL;
	unsigned long event;
	int ret;

	sock = ib_socket_create();
	if (sock == NULL)
		return -ENOMEM;

	ret = ib_socket_bind(sock, INADDR_ANY, 1998);
	if (ret < 0)
		goto out;

	/* wait incomming connect */
	while ((event & POLLIN) == 0) {
	event = ib_socket_poll(sock);
		/* wait a POLLIN to say new connection ready to */
		if ((event & POLLERR) != 0)
			goto out;
	}
	/* have incomming event, so socket is ready */
	sock_child = ib_socket_accept(sock);
	BUG_ON(sock_child == NULL);

	while ( 1 ) {
		event = ib_socket_poll(sock_child);
		if (event & POLLERR)
			break;
	}
	ib_socket_destroy(sock_child);
out:
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