#ifndef _IB_SOCK_INT_H_
#define _IB_SOCK_INT_H_

#include <linux/types.h>
#include <linux/net.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#define IB_CQ_EVENTS_MAX 128

#define IB_ADDR_TIMEOUT 100
#define IB_ROUTE_TIMEOUT 100

struct IB_SOCK {
	/* primary OFED stack ID */
	struct rdma_cm_id	*is_id;

	/* transfer related parts */
	/* completion events */
	struct ib_cq		*is_cq;
	/* queue pair to communicate between nodes */
	struct ib_qp		*is_qp;

	/* event mask */
	unsigned long		is_events;
};

/* messages on wire */
#define WIRE_ATTR	__attribute__((packed))

#define IB_HELLO_MAGIC 0x9012

struct ib_hello {
	__u32	magic;
} WIRE_ATTR;

#endif
