#include "ib-sock.h"



static int __init
srv_init(void)
{
	struct IB_SOCK *sock, *sock_child = NULL;
	unsigned long event = 0;
	bool read_st;
	void *data;
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
		printk("srv got event %lx\n", event);
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
		if (!read_st && (event & POLLIN)) {
			size_t d_size;

			d_size = ib_socket_read_size(sock);
			data = kmalloc(d_size, GFP_KERNEL);
			if (data == NULL)
				break;
			read_st = true;
		}
		if (read_st && (event & POLLIN)) {
			/* read done */
			printk("%s\n", data);
			kfree(data);
			read_st = false;
		}
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