#include "ib-sock.h"
#include "ib-sock-int.h"

static int cm_handler(struct rdma_cm_id *cmid, struct rdma_cm_event *event)
{
	int ret = 0;

	printk("CM event %d status %d conn %p id %p\n",
		event->event, event->status, cmid->context, cmid);

	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
	case RDMA_CM_EVENT_ESTABLISHED:
	case RDMA_CM_EVENT_CONNECT_RESPONSE:
		break;
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		break;
	/* some common errors */
	case RDMA_CM_EVENT_ADDR_ERROR:
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_UNREACHABLE:
	case RDMA_CM_EVENT_REJECTED:
		break;
	/* some hard errors to abort connection */
	case RDMA_CM_EVENT_DISCONNECTED:
	case RDMA_CM_EVENT_DEVICE_REMOVAL:
	case RDMA_CM_EVENT_ADDR_CHANGE:
		break;
	default:
		printk("Unexpected RDMA CM event (%d)\n", event->event);
		break;
	}
	return ret;
}

struct IB_SOCK *ib_socket_create()
{
	struct IB_SOCK *sock;

	sock = kmalloc(sizeof(*sock), GFP_KERNEL);
	if (sock == NULL)
		return NULL;

	sock->is_id = rdma_create_id(cm_handler, sock,
					     RDMA_PS_TCP, IB_QPT_RC);
	if (IS_ERR(sock->is_id)) {
		printk("error create cm_id %ld\n", PTR_ERR(sock->is_id));
		goto out_free;
	}

	return sock;
out_free:
	ib_socket_destroy(sock);
	return NULL;
}

void ib_socket_destroy(struct IB_SOCK *sock)
{

	if (sock->is_id)
		rdma_destroy_id(sock->is_id);
	kfree(sock);
}
