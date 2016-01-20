#include "ib-sock-int.h"

static bool __take_free_ctl(struct IB_SOCK *sock, struct ib_sock_ctl **msg)
{
	struct ib_sock_ctl *_msg;

	spin_lock(&sock->is_ctl_lock);
	_msg = list_first_entry_or_null(&sock->is_ctl_idle_list,
					struct ib_sock_ctl, iscm_link);
	if (_msg != NULL) {
		list_move(&_msg->iscm_link, &sock->is_ctl_active_list);

		*msg = _msg;
	}

	return _msg != NULL;
}

/* take from idle list or wait until free exist */
struct ib_sock_ctl *
ib_sock_ctl_take(struct IB_SOCK *sock)
{
	struct ib_sock_ctl *msg;

       /* take an IDLE TX, if none available - try to allocated one
        * with new WR index  */
       wait_event(sock->is_ctl_waitq, __take_free_ctl(sock, &msg));
       if (msg == NULL)
               return NULL;

	return msg;

}

void ib_sock_ctl_put(struct IB_SOCK *sock, struct ib_sock_ctl *msg)
{
	list_add(&msg->iscm_link, &sock->is_ctl_idle_list);
	wake_up(&sock->is_ctl_waitq);
}

struct kmem_cache *ib_sock_ctl;

int ib_sock_ctl_init(struct IB_SOCK *sock)
{
	struct ib_sock_ctl *msg;
	unsigned int i;
	unsigned count = 0;


	ib_sock_ctl = kmem_cache_create("ib_sock_ctl_msg",
					  sizeof(struct ib_sock_ctl),
					  0, SLAB_HWCACHE_ALIGN,
					  NULL);
	if (ib_sock_ctl == NULL)
		return -ENOMEM;

	init_waitqueue_head(&sock->is_ctl_waitq);
	INIT_LIST_HEAD(&sock->is_ctl_active_list);
	INIT_LIST_HEAD(&sock->is_ctl_idle_list);
	
	/* preallocate until limit.
	 * we can allocate it by request, but simplify a code 
	 */
	for (i = 0; i < IB_MAX_CTL_MSG; i ++) {
		msg = kmem_cache_alloc(ib_sock_ctl, GFP_KERNEL);
		if (msg == NULL)
			continue;
		count ++;
		/* pre init and put to idle */
		ib_sock_ctl_put(msg);
	}
	return 0;
}

void ib_sock_ctl_fini(struct IB_SOCK *sock)
{
	struct ib_sock_ctl *pos, *next;

	/* i don't know how an abort active controls for now */
	BUG_ON(!list_empty(&sock->is_ctl_active_list));

	list_for_each_entry_safe(pos, next, &sock->is_ctl_active_list,
				 iscm_link) {
		list_del(&pos->iscm_link);

		kmem_cache_free(ib_sock_ctl, pos);
	}

}