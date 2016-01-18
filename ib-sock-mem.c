#include "ib-sock-int.h"

/* it may done for device only (not per connection base) */
static int ib_sock_mem_init_common(struct IB_SOCK *sock)
{
	int 	ret;
	int	access_flags = IB_ACCESS_LOCAL_WRITE |
				IB_ACCESS_REMOTE_WRITE;

	/* create a protection domain. */
	sock->is_mem.ism_pd = ib_alloc_pd(sock->is_id->device);
	if (IS_ERR(sock->is_mem.ism_pd)) {
		printk("can't create a protection domain\n");
		ret = PTR_ERR(sock->is_mem.ism_pd);
		sock->is_mem.ism_pd = NULL;
		return ret;
	}

	sock->is_mem.ism_mr = ib_get_dma_mr(sock->is_mem.ism_pd, access_flags);
	if (IS_ERR(sock->is_mem.ism_mr)) {
		ret = PTR_ERR(sock->is_mem.ism_mr);
		sock->is_mem.ism_mr = NULL;
		printk("Failed ib_get_dma_mr : %d\n", ret);
		return ret;
	}

	return 0;
}

static void ib_sock_mem_fini_common(struct IB_SOCK *sock)
{
	if (sock->is_mem.ism_mr)
		ib_dereg_mr(sock->is_mem.ism_mr);

	if (sock->is_mem.ism_pd)
		ib_dealloc_pd(sock->is_mem.ism_pd);
}

int ib_sock_mem_init(struct IB_SOCK *sock)
{
	int ret;

	ret = ib_sock_mem_init_common(sock);
	if (ret < 0)
		return ret;

	/* different memory managment models may need different init */

	return 0;
}

void ib_sock_mem_fini(struct IB_SOCK *sock)
{
	ib_sock_mem_fini_common(sock);
}
