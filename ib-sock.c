#include "ib-sock.h"
#include "ib-sock-int.h"

static int
cm_client_handler(struct rdma_cm_id *cmid, struct rdma_cm_event *event)
{
	struct IB_SOCK *sock = cmid->context;
	int ret = 0;

	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		ret = rdma_resolve_route(cmid, IB_ROUTE_TIMEOUT);
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	printk("client CM event ret %d\n", ret);
	return ret;
}


static int cm_handler(struct rdma_cm_id *cmid, struct rdma_cm_event *event)
{
	int ret = 0;

	printk("CM event %d status %d conn %p id %p\n",
		event->event, event->status, cmid->context, cmid);

	switch (event->event) {
	/* client related events */
	case RDMA_CM_EVENT_ADDR_RESOLVED:
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
	case RDMA_CM_EVENT_ESTABLISHED:
	case RDMA_CM_EVENT_CONNECT_RESPONSE:
		ret = cm_client_handler(cmid, event);
		break;
	/* server related events */
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

	printk("IB socket create %p\n", sock);
	return sock;
out_free:
	ib_socket_destroy(sock);
	return NULL;
}

void ib_socket_destroy(struct IB_SOCK *sock)
{
	printk("IB socket destroy %p\n", sock);

	if (sock->is_id)
		rdma_destroy_id(sock->is_id);
	kfree(sock);
}

/***************************************************************************************/
/* 
 * In IB terms - connect is resolving address + router + some connect mgs exchange.
 * all of it done via CM related events.
 *
 * IB uses an IP addresses as some ID to resolve in destination GID value.
 */

int ib_socket_connect(struct IB_SOCK *sock, struct sockaddr_in  *dstaddr)
{
	struct sockaddr_in  srcaddr;
	int err;

        memset(&srcaddr, 0, sizeof(srcaddr));
        srcaddr.sin_family      = AF_INET;

	/* IB needs to start from resolve addr / route first */
	err = rdma_resolve_addr(sock->is_id, (struct sockaddr *)&srcaddr, 
			        (struct sockaddr *)dstaddr, 
			        IB_ADDR_TIMEOUT /* timeout ms */
			        );
	printk("resolve dst address status %d\n", err);

	/* when connect will done - WRITE event will generated */
	return err;
}

void ib_socket_disconnect(struct IB_SOCK *sock)
{
	int err = 0;

	/* change the ib conn state only if the conn is UP, however always call
	 * rdma_disconnect since this is the only way to cause the CM to change
	 * the QP state to ERROR
	 */

	err = rdma_disconnect(sock->is_id);
	if (err)
		printk("Failed to disconnect, conn: 0x%p err %d\n",
			 sock,err);
}
