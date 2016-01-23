#include "ib-sock.h"
#include "ib-sock-int.h"

static struct IB_SOCK *__ib_socket_create(struct rdma_cm_id *cm_id);

static void qp_event_callback(struct ib_event *cause, void *context)
{
	printk("got qp event %d\n",cause->event);
}

static void ib_cq_event_callback(struct ib_event *cause, void *context)
{
	printk("got cq event %d \n", cause->event);
}

static void ib_sock_handle_rx(struct IB_SOCK *sock, struct ib_wc *wc)
{
	struct ib_sock_ctl *msg = (struct ib_sock_ctl *)(uintptr_t)wc->wr_id;
	struct ib_device *device = sock->is_id->device;
	int ret;

	/* CPU is owner*/
	ib_dma_sync_single_for_cpu(device, 
				   msg->iscm_sge.addr, msg->iscm_sge.length,
				   DMA_FROM_DEVICE);

	if (wc->status != IB_WC_SUCCESS) {
		printk("rx status error %d\n", wc->status);
		goto repost;
	}


	if (msg->iscm_msg.sww_magic == IB_CTL_MSG_MAGIC)
		printk("recv maic ok!\n");
	else
		printk("recv magic bad %x\n", msg->iscm_msg.sww_magic);

repost:
	/* trasnfer ownership to the device */
	ib_dma_sync_single_for_device(device, 
				   msg->iscm_sge.addr, msg->iscm_sge.length,
				   DMA_FROM_DEVICE);
	/* repost to processing */
	ret  = ib_sock_ctl_post(sock, msg);
	if (ret != 0)
		printk("Error with summit to rx queue\n");
}

static void ib_sock_handle_tx(struct IB_SOCK *sock, struct ib_wc *wc)
{
	int event = POLLOUT;
	/* TX event hit when TX done or error hit */

	if (wc->status != IB_WC_SUCCESS)
		event |= POLLERR;
	sock_event_set(sock, event);
}

/* based on ip over ib code  */
static void ib_sock_cq_work(struct work_struct *work)
{
	struct IB_SOCK *sock;
	int n, i;
	struct ib_sock_ctl *msg;

	sock = container_of(work, struct IB_SOCK, is_cq_work);

poll_more:
	n = ib_poll_cq(sock->is_cq, IB_CQ_EVENTS_BATCH, sock->is_cq_wc);
	for (i = 0; i < n; i++) {
		msg = (struct ib_sock_ctl *)(uintptr_t)sock->is_cq_wc[i].wr_id;
		if (msg->iscm_flags & CTL_MSG_RX)
			ib_sock_handle_rx(sock, &sock->is_cq_wc[i]);
		else
			ib_sock_handle_tx(sock, &sock->is_cq_wc[i]);

	}

	/* abstract limit */
	if (n < (IB_CQ_EVENTS_BATCH / 2)) {
		if (unlikely(ib_req_notify_cq(sock->is_cq,
					      IB_CQ_NEXT_COMP |
					      IB_CQ_REPORT_MISSED_EVENTS)))
			goto poll_more;
	}

}

/* have some change states  */
/* DID we really needs it ? */
static void ib_cq_callback(struct ib_cq *cq, void *cq_context)
{
	struct IB_SOCK *sock = cq_context;

	printk("cq event %p\n", sock);
	schedule_work(&sock->is_cq_work);
}

/* creation of CQ/QP isn't needs to create on route event,
 * but it must done before rdma_connect */
static int ib_sock_cq_qp_create(struct IB_SOCK *sock)
{
	struct rdma_cm_id	*cmid = sock->is_id;
	struct ib_qp_init_attr	init_attr;
	int    ret;

	/* XXX need special refactoring to extract per device data */
	INIT_WORK(&sock->is_cq_work, ib_sock_cq_work);

	/* event queue, may per CPU and per device, not per socket */
	sock->is_cq = ib_create_cq(cmid->device,
				   ib_cq_callback,
				   ib_cq_event_callback,
				   (void *)&sock, /* context */
				   IB_CQ_EVENTS_MAX, /* max events in queue 
						     * typically max parallel ops */
				   0);
	if (IS_ERR(sock->is_cq)) {
		ret = PTR_ERR(sock->is_cq);
		sock->is_cq = NULL;
		printk("error create cq %d\n", ret);
		return ret;
	}

	/* XXX need check a ret code ? */
	ib_req_notify_cq(sock->is_cq, IB_CQ_NEXT_COMP);

	/* data queue */
	memset(&init_attr, 0, sizeof(init_attr));
	init_attr.event_handler = qp_event_callback;
	init_attr.qp_context	= (void *)&sock;
	init_attr.send_cq	= sock->is_cq;
	init_attr.recv_cq	= sock->is_cq;
	init_attr.sq_sig_type	= IB_SIGNAL_REQ_WR;
	init_attr.qp_type	= IB_QPT_RC;

	init_attr.cap.max_send_wr = sock->is_mem.ism_wr_count;
	init_attr.cap.max_recv_wr = sock->is_mem.ism_wr_count;

	init_attr.cap.max_send_sge = sock->is_mem.ism_sge_count;
	init_attr.cap.max_recv_sge = sock->is_mem.ism_sge_count;

	ret = rdma_create_qp(cmid, sock->is_mem.ism_pd, &init_attr);
	if (ret != 0) {
		printk("error create qp %d\n", ret);
		return ret;
	}

	return 0;
}

static void ib_sock_cq_qp_destroy(struct IB_SOCK *sock)
{
	struct rdma_cm_id *cmid = sock->is_id;

	flush_scheduled_work();

	/* XXX is it needs ? */
	if (cmid != NULL && cmid->qp != NULL)
		rdma_destroy_qp(cmid);

	if (sock->is_cq != NULL)
		ib_destroy_cq(sock->is_cq);
}

/* allocate a sort of resources for socket, such as 
 * 1) memory region to map a requests
 * 2) CQ/QP pair to handle events about transmssion data
 */
static int ib_sock_resource_alloc(struct IB_SOCK *sock)
{
	int ret;

	ret = ib_sock_mem_init(sock);
	if (ret < 0)
		return ret;

	ret = ib_sock_cq_qp_create(sock);
	if (ret < 0)
		return ret;

	ret = ib_sock_ctl_init(sock);
	if (ret < 0)
		return ret;

	return 0;
}

static void ib_sock_resource_free(struct IB_SOCK *sock)
{
	ib_sock_cq_qp_destroy(sock);
	ib_sock_mem_fini(sock);
	ib_sock_ctl_fini(sock);
}

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

		/* connect response done, hello verified - allocate resources and go */
		ret = ib_sock_resource_alloc(sock);
		if (ret < 0)
			break;

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
static void server_error_accept(struct IB_SOCK *sock)
{
	/* will destroy after exit from event cb */
	sock->is_id = NULL;
	ib_socket_destroy(sock);
}

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
		if (sock == NULL) {
			printk("error accept \n");
			ret = -ENOMEM;
			break;
		}

		ret = ib_sock_resource_alloc(sock);
		if (ret < 0) {
			server_error_accept(sock);
			return ret;
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
			server_error_accept(sock);
			return ret;
		}

		spin_lock(&parent->is_child_lock);
		list_add(&sock->is_child, &parent->is_child);
		spin_unlock(&parent->is_child_lock);

		sock_event_set(parent, POLLIN);

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

	sock->is_cq = NULL;

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
	struct IB_SOCK *pos, *next;

	printk("IB socket destroy %p\n", sock);

	sock->is_flags |= SOCK_ERROR;

	list_for_each_entry_safe(pos, next, &sock->is_child, is_child) {
		list_del(&pos->is_child);
		ib_socket_destroy(pos);
	}

	ib_sock_resource_free(sock);

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
	printk("Accept returned %p\n", sock);
	return sock;
}
