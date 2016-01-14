#ifndef _IB_SOCK_INT_H_
#define _IB_SOCK_INT_H_

#include <linux/types.h>
#include <linux/net.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

struct IB_SOCK {
	/* primary OFED stack ID */
	struct rdma_cm_id	*is_id;
};

#endif
