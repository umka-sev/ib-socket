#include "ib-sock-int.h"

/* TX is main IB object. It hold an IB card operations (WR) and 
 * memory attached to it.
 * for operations read/write from network WR have a single differences
 * just an WR opcode. Different memory managment models may have different
 * list of WR opcodes in signle chain.
 */

static bool __take_free_wr(struct IB_SOCK *sock, struct ib_sock_wr **wr)
{
	struct ib_sock_wr *_wr;

	/* XXX protect */
	_wr = list_first_entry_or_null(&sock->is_wr_idle_list,
					struct ib_sock_wr, isw_link);
	if (_wr != NULL) {
		list_del(&_wr->isw_link);
		list_add(&_wr->isw_link, &sock->is_wr_active_list);

		_wr->isw_wr_pos = 0;
		_wr->isw_sge_pos = 0;

		*wr = _wr;
	}

	return _wr != NULL;
}

struct ib_sock_wr *tx_idle_get(struct IB_SOCK *sock)
{
	struct ib_sock_wr *wr;

       /* take an IDLE TX, if none available - try to allocated one
        * with new WR index  */
       wait_event(sock->is_wr_waitq, __take_free_wr(sock, &wr));
       if (wr == NULL)
               return NULL;

	return wr;
}

/* return to idle */
void wr_put(struct IB_SOCK *sock, struct ib_sock_wr *wr)
{
	list_add(&wr->isw_link, &sock->is_wr_idle_list);
	wake_up(&sock->is_wr_waitq);
}

static void wr_destroy_one(struct ib_sock_wr *wr)
{
	if (wr->isw_wrq)
		kfree(wr->isw_wrq);

	if (wr->isw_sge)
		kfree(wr->isw_sge);
}

static int wr_init_one(struct IB_SOCK *sock, struct ib_sock_wr *wr)
{
	size_t sz;

	sz = sizeof(*wr->isw_wrq) * sock->is_mem.ism_wr_count;
	wr->isw_wrq = kmalloc(sz, GFP_KERNEL);
	if (wr->isw_wrq == NULL)
		goto out_err;

	sz = sizeof(*wr->isw_sge) * sock->is_mem.ism_sge_count;
	wr->isw_sge = kmalloc(sz, GFP_KERNEL);
	if (wr->isw_sge == NULL)
		goto out_err;

	wr->isw_wr_pos = 0;
	wr->isw_sge_pos = 0;

	return 0;
out_err:
	wr_destroy_one(wr);
	return -ENOMEM;
}

/*
 * allocate an number TX based on number paralel operations.
 * as TX uses for both (transmit and recerve) operations,
 * we need twice more than parallel transfers.
 * each transfer hold a own WR list + own memory
 */
int wr_init(struct IB_SOCK *sock)
{
	struct ib_sock_wr *wr;
	size_t num_wr = sock->is_parallel * 2;
	int i;

	INIT_LIST_HEAD(&sock->is_wr_idle_list);
	INIT_LIST_HEAD(&sock->is_wr_active_list);

	/* may on demand */
	for (i = 0; i < num_wr; i++) {
		wr = kmalloc(sizeof(*wr), GFP_KERNEL);
		if (wr == NULL)
			break;
		if (wr_init_one(sock, wr) < 0) {
			kfree(wr);
			/* it is not a fatal, try next one */
		}

		wr_put(sock, wr);
	}

	/* have one or more TX ready */
	return i > 0;
}

void wr_fini(struct IB_SOCK *sock)
{
	struct ib_sock_wr *wr, *next;

	/* XXX protect */
	list_for_each_entry_safe(wr, next, &sock->is_wr_idle_list, isw_link) {
		list_del(&wr->isw_link);
		wr_destroy_one(wr);
	}
	
	BUG_ON(!list_empty(&sock->is_wr_active_list));
}
