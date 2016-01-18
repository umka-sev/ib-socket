#ifndef _IB_SOCK_INT_H_
#define _IB_SOCK_INT_H_

#include <linux/types.h>
#include <linux/net.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#define IB_ADDR_TIMEOUT 100
#define IB_ROUTE_TIMEOUT 100

#define IB_LISTEN_QUEUE 128

/* just one control message in flight */
#define IB_MAX_PARALLEL	 1

/* 1 RX + 1 TX in flight */
#define IB_MAX_CTL_MSG	(IB_MAX_PARALLEL * 2)
#define IB_CQ_EVENTS_MAX (IB_MAX_PARALLEL * 2)

enum ib_sock_flags {
	SOCK_CONNECTED	= 1 << 0,
	SOCK_ERROR	= 1 << 1,
};

struct ib_sock_mem {
	/* protection domain */
	struct ib_pd		*ism_pd;
	/* memory window to map.
	 * all ? or most cards may work with single == global MR  */
	struct ib_mr		*ism_mr;

	/* # send work items */
	unsigned		ism_wr_count;

	/* ...and their memory */
	unsigned		ism_sge_count;

};

struct IB_SOCK {
	/* primary OFED stack ID */
	struct rdma_cm_id	*is_id;

	unsigned long		is_flags;

	struct ib_sock_mem	is_mem;

	/* transfer related parts */
	/* completion events */
	struct ib_cq		*is_cq;
	/* queue pair to communicate between nodes */
	struct ib_qp		*is_qp;

	/* control messages */
	/* IDLE <> active protection  */
	spinlock_t		is_ctl_lock;
	struct list_head	is_ctl_idle_list;
	struct list_head	is_ctl_active_list;
	wait_queue_head_t	is_ctl_waitq;

	/* pre-accepted sockets */
	spinlock_t		is_child_lock;
	struct list_head	is_child;

	/* event mask */
	unsigned long		is_events;
	wait_queue_head_t	is_events_wait;
};

static inline
void sock_event_set(struct IB_SOCK *sock, unsigned int event)
{
	sock->is_events |= event;
	wake_up(&sock->is_events_wait);
}

/**************************** messages on wire ********************/
#define WIRE_ATTR	__attribute__((packed))

#define IB_HELLO_MAGIC 0x9012

struct ib_hello {
	__u32	magic;
} WIRE_ATTR;

/************* ib sock control protocol ***************************/
#define IB_CTL_MSG_MAGIC	0x87154

struct ib_sock_wire_msg {
	uint32_t	sww_magic;
} WIRE_ATTR;

struct ib_sock_ctl {
	struct list_head	iscm_link;

	struct ib_sge		iscm_sge;

	/* used to describe an incomming rdma transfer,
	 * must be first WR in sending chain */
	struct ib_sock_wire_msg	iscm_msg;
};
/************* ib sock control protocol ***************************/
/* ctl-msg.c */
/* init queue and post sort of rx buffer to wait incomming data */
int ib_sock_ctl_msg_init(struct IB_SOCK *sock);
void ib_sock_ctl_msg_fini(struct IB_SOCK *sock);
/* take control message to send an outgoning buffer */
struct ib_sock_ctl *ib_sock_ctl_idle_take(struct IB_SOCK *sock);

/* mem.c */
/* init function responsible to fill an number WR / SGE per socket*/
int ib_sock_mem_init(struct IB_SOCK *sock);
void ib_sock_mem_fini(struct IB_SOCK *sock);


/* util.c */
const char *ib_event_type_str(enum ib_event_type ev_type);
const char *wr_status_str(enum ib_wc_status status);
char *cm_event_type_str(enum rdma_cm_event_type ev_type);

#endif
