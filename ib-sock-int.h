#ifndef _IB_SOCK_INT_H_
#define _IB_SOCK_INT_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/types.h>
#include <linux/net.h>

#include <linux/wait.h>
#include <linux/dma-mapping.h>

#include <linux/socket.h>
#include <linux/in.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_fmr_pool.h>
#include <rdma/rdma_cm.h>


struct ib_mem_ops {
    int (*map)(struct IB_SOCK, struct kvec *pages, size_t count);
    int (*unmap)(struct IB_SOCK, struct kvec *pages, size_t count);
};

struct IB_SOCK {
	struct rdma_cm_id	*is_id;
	/* protection domain */
	struct ib_pd		*is_pd;

	/* completion events */
	struct ib_cq		*is_cq;
	/* queue pair to communicate between nodes */
	struct ib_qp		*is_qp;

	/* buffer for login request / response */
	struct page		*is_login;
	struct ib_mem_ops	is_mem;

	spinlock_t		is_state_lock;
	enum sock_state		is_state;
	enum sock_state		is_state_old;
	wait_queue_head_t	is_state_wait;

	/* pre-accepted sockets */
	struct list_head	is_child;
};

const char *ib_event_type_str(enum ib_event_type ev_type);
const char *wr_status_str(enum ib_wc_status status);
char *cm_event_type_str(enum rdma_cm_event_type ev_type);

#endif