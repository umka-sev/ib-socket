#include "ib-sock.h"

static unsigned int port = 10000;
const char *srv_addr = "192.168.1.1";
static __u32 addr  = 0;


static int __init
cli_init(void)
{
	struct IB_SOCK *sock;
	enum sock_state event;
        struct sockaddr_in  dstaddr;
        bool ping = true;
	unsigned a,b,c,d;
	int err;

        /* numeric IP? */
        if (sscanf(srv_addr, "%u.%u.%u.%u", &a, &b, &c, &d)) {
                addr = ((a<<24)|(b<<16)|(c<<8)|d);
                return 1;
        }

        memset(&dstaddr, 0, sizeof(dstaddr));
        dstaddr.sin_family = AF_INET;
        dstaddr.sin_addr.s_addr = (__force u32)addr;
        dstaddr.sin_port = htons(port);

	sock = ib_socket_create();
	if (sock == NULL)
		return -ENOMEM;

	err = ib_socket_connect(sock, &dstaddr);
	if (err) {
		printk("error connect \n");
		goto exit;
	}

	while (1) {
		event = ib_socket_pool(sock);
		if (event == IBS_ERROR) {
			printk("error event !\n");
			err = -EINVAL;
			break;
		}
//		if (event == DATA_READY) {
//		    err == 
//		}
	}

exit:
	ib_socket_destroy(sock);
	return err;
}

static void __exit
cli_cleanup(void)
{
}

module_init(cli_init);
module_exit(cli_cleanup);

MODULE_LICENSE("GPL");