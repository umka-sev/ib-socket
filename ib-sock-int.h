#ifndef _IB_SOCK_INT_H_
#define _IB_SOCK_INT_H_

#include <linux/types.h>
#include <linux/net.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#define IB_ADDR_TIMEOUT 100
#define IB_ROUTE_TIMEOUT 100

enum ib_sock_flags {
	SOCK_CONNECTED	= 1 << 0,
	SOCK_ERROR	= 1 << 1,
};

struct IB_SOCK {
	/* primary OFED stack ID */
	struct rdma_cm_id	*is_id;

	unsigned long		is_flags;

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


/* messages on wire */
#define WIRE_ATTR	__attribute__((packed))

#define IB_HELLO_MAGIC 0x9012

struct ib_hello {
	__u32	magic;
} WIRE_ATTR;



/* util.c */
const char *ib_event_type_str(enum ib_event_type ev_type);
const char *wr_status_str(enum ib_wc_status status);
char *cm_event_type_str(enum rdma_cm_event_type ev_type);

#endif
