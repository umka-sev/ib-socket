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
#define IB_MAX_CTL_MSG		(IB_MAX_PARALLEL * 2)
#define IB_CQ_EVENTS_MAX	(IB_MAX_PARALLEL * 2)
/* abstract number, just related to CPU time spent in one event process loop
 */
#define IB_CQ_EVENTS_BATCH	(5)

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

	/******* transfer related parts ***************/
	/* number CQ events in batch poll */
	struct ib_wc		is_cq_wc[IB_CQ_EVENTS_BATCH];
	/* we can't process an cq events in callback as 
	 * it may executed in interrupt context, so create 
	 * work queue for it. Latter it should be per 
	 * IB device data */
	struct work_struct	is_cq_work;
	
	/* completion events */
	struct ib_cq		*is_cq;
	/* queue pair to communicate between nodes */
	/* we may create a per CPU QP to reduce a contention
	 * on single QP */
	struct ib_qp		*is_qp;

	/* control messages */
	/* IDLE <> active <> rd protection  */
	spinlock_t		is_ctl_lock;
	struct list_head	is_ctl_idle_list;
	struct list_head	is_ctl_active_list;
	struct list_head	is_ctl_rd_list;
	wait_queue_head_t	is_ctl_waitq;
	/******* transfer related parts end ************/

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

/* IB card operation. it's chain of WR's with own
 * sge */
/* differences between read and write operation 
 * just an IB wr opcode, and one WR at begin to transfer 
 * a transfer descriptor.
 * it message may replaced with MAD packet, but i don't find how
 * to do it.
 */
struct ib_sock_wr {
	struct list_head	isw_link;

	/* possition in WR array to use */
	unsigned		isw_wr_pos;
	/* preallocated send work items.. in ring.
	 * did we need a static allocation ?
	 */
	struct ib_send_wr	*isw_wrq;

	/* ...and their memory */
	unsigned		isw_sge_pos;
	struct ib_sge		*isw_sge;
};


/**************************** messages on wire ********************/
#define WIRE_ATTR	__attribute__((packed))

#define IB_HELLO_MAGIC 0x9012

struct ib_hello {
	uint32_t	magic;
} WIRE_ATTR;

#define IB_CTL_MSG_MAGIC	0x87154

struct ib_sock_wire_msg {
	uint32_t	sww_magic;
	uint32_t	sww_size;
} WIRE_ATTR;

/**************************** messages on wire ********************/

/************* ib sock control protocol ***************************/

#define CTL_MSG_RX	0x1

struct ib_sock_ctl {
	struct list_head	iscm_link;

	/* per MAD code we don't need sge at memory, but
	 * we need a dma address instead. Lets do it optimization
	 * later */
	struct ib_sge		iscm_sge;

	unsigned long		iscm_flags;

	/* used to describe an incomming rdma transfer,
	 * must be first WR in sending chain */
	struct ib_sock_wire_msg	iscm_msg;
};
/************* ib sock control protocol end ************************/


/* wr.c */
struct ib_sock_wr *wr_idle_get(struct IB_SOCK *sock);
void wr_put(struct IB_SOCK *sock, struct ib_sock_wr *wr);
int wr_init(struct IB_SOCK *sock);
void wr_fini(struct IB_SOCK *sock);

/* ctl-msg.c */
/* init queue and post sort of rx buffer to wait incomming data */
int ib_sock_ctl_init(struct IB_SOCK *sock);
void ib_sock_ctl_fini(struct IB_SOCK *sock);
/* take control message to send an outgoning buffer */
struct ib_sock_ctl *ib_sock_ctl_take(struct IB_SOCK *sock);
void ib_sock_ctl_put(struct IB_SOCK *sock, struct ib_sock_ctl *msg);
int ib_sock_ctl_post(struct IB_SOCK *sock, struct ib_sock_ctl *msg);


/* mem.c */
/* init function responsible to fill an number WR / SGE per socket*/
int ib_sock_mem_init(struct IB_SOCK *sock);
void ib_sock_mem_fini(struct IB_SOCK *sock);


/* util.c */
const char *ib_event_type_str(enum ib_event_type ev_type);
const char *wr_status_str(enum ib_wc_status status);
char *cm_event_type_str(enum rdma_cm_event_type ev_type);

#endif
