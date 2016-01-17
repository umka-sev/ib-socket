#include "ib-sock.h"
#include "ib-sock-int.h"

struct IB_SOCK *__ib_socket_create(struct rdma_cm_id *cm_id);


static int
cm_client_handler(struct rdma_cm_id *cmid, struct rdma_cm_event *event)
{
	struct IB_SOCK *sock = cmid->context;
	int ret = 0;

	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		ret = rdma_resolve_route(cmid, IB_ROUTE_TIMEOUT);
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED: {
		/* route resolved - need to send an HELLO message */
		struct rdma_conn_param	conn_param;
		struct ib_hello	hello;

		memset(&conn_param, 0, sizeof conn_param);
		conn_param.responder_resources = 4;
		conn_param.initiator_depth     = 1;
		conn_param.retry_count	       = 7;
		conn_param.rnr_retry_count     = 6;

		/* fill a hello message */
		hello.magic = IB_HELLO_MAGIC;
		conn_param.private_data		= (void *)&hello;
		conn_param.private_data_len	= sizeof(struct ib_hello);

		/* we may use a strict IB verbs API to send a hello message
		 * instead of rdma_ API */
		ret = rdma_connect(sock->is_id, &conn_param);
		if (ret)
			printk("failure connecting: %d\n", ret);

		break;
	}
#if 0
	case RDMA_CM_EVENT_ESTABLISHED:
		break;
#endif
	case RDMA_CM_EVENT_CONNECT_RESPONSE: {
		/* This event Generated  on  the active side to notify the user of a
		 * successful response to a connection  request.   It  is
		 * only  generated  on rdma_cm_id's that do not have a QP
		 * associated with them.
		*/
		/* we delay to create a connection resources until we have
		 * check a server connect response, it save lots resources
		 * in case server have a wrong version or capabilites */
		const struct ib_hello *hello = event->param.conn.private_data;

		sock->is_flags |= SOCK_CONNECTED;
		if (hello->magic != IB_HELLO_MAGIC) {
			ret = -EPROTO;
			break;
		}

		sock_event_set(sock, POLLOUT);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	printk("client CM event ret %d\n", ret);
	return ret;
}
/**************************************************************************************/
static int
cm_server_handler(struct rdma_cm_id *cmid, struct rdma_cm_event *event)
{
	int ret = 0;

	switch (event->event) {
	case RDMA_CM_EVENT_CONNECT_REQUEST: {
		/* incomming request - lets allocate a resources for a new connect */
		const struct ib_hello *hello = event->param.conn.private_data;
		struct ib_hello hello_ack;
		struct rdma_conn_param	conn_param;
		struct IB_SOCK *parent = cmid->context;
		struct IB_SOCK *sock;

		if (hello->magic != IB_HELLO_MAGIC) {
			printk("Incomming error: len %u magic %x\n",
				event->param.conn.private_data_len,
				hello->magic);
			ret = -EPROTO;
			break;
		}

		sock = __ib_socket_create(cmid);
		if (sock != NULL) {
			printk("error accept \n");
			ret = -ENOMEM;
			break;
		}

		memset(&hello_ack, 0, sizeof hello_ack);
		hello_ack.magic = IB_HELLO_MAGIC;

		memset(&conn_param, 0, sizeof conn_param);
		conn_param.private_data_len = sizeof hello_ack;
		conn_param.private_data = &hello_ack;
		conn_param.responder_resources = 0 /* no atomic */;
		conn_param.initiator_depth = 0;
		conn_param.retry_count = 10;

		ret = rdma_accept(sock->is_id, &conn_param);
		if (ret < 0) {
			/* will destroy after exit from event cb */
			sock->is_id == NULL;
			ib_socket_destroy(sock);
			return ret;
		}

		spin_lock(&parent->is_child_lock);
		list_add(&sock->is_child, &parent->is_child);
		spin_unlock(&parent->is_child_lock);

		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	printk("server CM event ret %d\n", ret);

	return ret;
}


static int cm_handler(struct rdma_cm_id *cmid, struct rdma_cm_event *event)
{
	struct IB_SOCK *sock = cmid->context;
	int ret = 0;

	printk("CM event %s status %d conn %p id %p\n",
		cm_event_type_str(event->event), event->status, cmid->context,
		cmid);

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
		ret = cm_server_handler(cmid, event);
		/* if we have any errors (including protocol violation 
		 * during connect handshake, we need a destroy new allocated
		 * cm_id  */
		if (ret < 0)
			return ret;
		break;
	/* some common errors */
	case RDMA_CM_EVENT_ADDR_ERROR:
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_UNREACHABLE:
		ret = -EHOSTUNREACH;
		break;
	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_REJECTED:
		/* to decode a reject event, see srp_cm_rej_handler */
		/* reject event hit in two cases - none bind ports, or disconnected on 
		 * other side */
		ret = -ECONNREFUSED;
		break;
	/* some hard errors to abort connection */
	case RDMA_CM_EVENT_DISCONNECTED:
	case RDMA_CM_EVENT_DEVICE_REMOVAL:
	case RDMA_CM_EVENT_ADDR_CHANGE:
		break;
	default:
		printk("Unexpected RDMA CM event (%d)\n", event->event);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		sock->is_flags |= SOCK_ERROR;
		sock_event_set(sock, POLLERR);
	}
	/* if we return an error, OFED treat it as critical error and kill an ID,
	 * dobule destroy id will result a panic. LNet uses it way
	 * to save a resources. */
	return 0;
}

struct IB_SOCK *__ib_socket_create(struct rdma_cm_id *cm_id)
{
	struct IB_SOCK *sock;

	sock = kmalloc(sizeof(*sock), GFP_KERNEL);
	if (sock == NULL)
		return NULL;

	sock->is_id = cm_id;
	cm_id->context = sock;

	sock->is_flags = 0;

	INIT_LIST_HEAD(&sock->is_child);
	spin_lock_init(&sock->is_child_lock);

	sock->is_events = 0;
	init_waitqueue_head(&sock->is_events_wait);
	
	printk("IB socket create %p\n", sock);
	return sock;
}

struct IB_SOCK *ib_socket_create()
{
	struct rdma_cm_id *cm_id;
	struct IB_SOCK *ret;
	
	cm_id = rdma_create_id(cm_handler, NULL, RDMA_PS_TCP, IB_QPT_RC);
	if (IS_ERR(cm_id)) {
		printk("error create cm_id %ld\n", PTR_ERR(cm_id));
		return NULL;
	}

	ret = __ib_socket_create(cm_id);
	if (ret == NULL)
		rdma_destroy_id(cm_id);

	return ret;
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

	if ((sock->is_flags & SOCK_CONNECTED) == 0)
		return;

	err = rdma_disconnect(sock->is_id);
	if (err)
		printk("Failed to disconnect, conn: 0x%p err %d\n",
			 sock,err);
}
/*****************************************************************************************/
static unsigned long __take_event(struct IB_SOCK *sock, unsigned long *e)
{
	unsigned long events;
	
	events = xchg(&sock->is_events, 0);
	if (events != 0)
		*e = events;

	return events;
}

unsigned long ib_socket_poll(struct IB_SOCK *sock)
{
	unsigned long mask = 0;

	wait_event(sock->is_events_wait, __take_event(sock, &mask));

	if (sock->is_flags & SOCK_ERROR)
		mask |= POLLERR;

	return mask;
}

/*****************************************************************************************/
int ib_socket_bind(struct IB_SOCK *sock, uint32_t addr, unsigned port)
{
	struct sockaddr_in  sin;
	int ret;

	sin.sin_family = AF_INET,
	sin.sin_addr.s_addr = (__force u32)htonl(addr);
	sin.sin_port = (__force u16)htons(port);

	ret = rdma_bind_addr(sock->is_id, (struct sockaddr *)&sin);
	if (ret) {
		printk(KERN_ERR "RDMA: failed to setup listener, "
		       "rdma_bind_addr() returned %d\n", ret);
		goto out;
	}

	ret = rdma_listen(sock->is_id, IB_LISTEN_QUEUE);
	if (ret) {
		printk(KERN_ERR "RDMA: failed to setup listener, "
		       "rdma_listen() returned %d\n", ret);
		goto out;
	}
	/* HELLO will done via CM (mad) packets */
out:
	return ret;

}

struct IB_SOCK *ib_socket_accept(struct IB_SOCK *parent)
{
	struct IB_SOCK *sock;

	spin_lock(&parent->is_child_lock);
	sock = list_first_entry_or_null(&parent->is_child, struct IB_SOCK, is_child);
	if (sock) {
		/* none can touch new socket until it accepted */
		list_del_init(&sock->is_child);
	}
	spin_unlock(&parent->is_child_lock);
	return sock;
}
