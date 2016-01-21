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
	spin_unlock(&sock->is_ctl_lock);

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

/* write related control messages should be return to idle list,
 * but read related messages need post to addapter to wait incomming data 
 */
 
/*
 * modern IB cards have a suggestion to use a SRQ (shared receiver queue)
 * it will adds latter, so extract base code to own function.
 */
int ib_sock_ctl_post(struct IB_SOCK *sock, struct ib_sock_ctl *msg)
{
	/* base on ib_mad_post_receive_mads() - that wr isn't need to live any time 
	 * so - save memory and declare on stack.
	 */
	struct ib_recv_wr wr;
	struct ib_recv_wr *bad_wr;
	int ret;

	ret = ib_post_recv(sock->is_qp, &wr, &bad_wr);

	return 0;
}
 
void ib_sock_ctl_put(struct IB_SOCK *sock, struct ib_sock_ctl *msg)
{
	spin_lock(&sock->is_ctl_lock);
	list_add(&msg->iscm_link, &sock->is_ctl_idle_list);
	spin_unlock(&sock->is_ctl_lock);

	wake_up(&sock->is_ctl_waitq);
}

int ib_sock_ctl_init(struct IB_SOCK *sock)
{
	struct ib_sock_ctl *msg;
	unsigned int i;
	unsigned count = 0;

	init_waitqueue_head(&sock->is_ctl_waitq);
	INIT_LIST_HEAD(&sock->is_ctl_active_list);
	INIT_LIST_HEAD(&sock->is_ctl_idle_list);

	/* preallocate until limit.
	 * we can allocate it by request, but simplify a code
	 */
	for (i = 0; i < IB_MAX_CTL_MSG; i ++) {
		msg = kmalloc(sizeof(*msg), GFP_KERNEL);
		if (msg == NULL)
			continue;
		count ++;
		/* pre init */
		ib_sock_ctl_put(sock, msg);
	}
	/* half of mgs uses for incomming, half outgoning */
	count /= 2;
	for(i = 0; i < count; i++) { 
		msg = ib_sock_ctl_take(sock);
		ib_sock_ctl_post(sock, msg);
	}

	return 0;
}

void ib_sock_ctl_fini(struct IB_SOCK *sock)
{
	struct ib_sock_ctl *pos, *next;

	/* XXX flush active */
	list_for_each_entry_safe(pos, next, &sock->is_ctl_active_list, iscm_link) {
		list_del(&pos->iscm_link);
		list_add(&pos->iscm_link, &sock->is_ctl_idle_list);
	}

	list_for_each_entry_safe(pos, next, &sock->is_ctl_idle_list, iscm_link) {
		list_del(&pos->iscm_link);

		kfree(pos);
	}
}
