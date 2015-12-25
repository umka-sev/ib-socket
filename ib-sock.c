#include "ib-sock.h"
#include "ib-sock-int.h"

#define IB_HELLO_MAGIC 0x9012

#define IB_LISTEN_QUEUE 128
#define IB_CQ_EVENTS_MAX 128

#define IB_ADDR_TIMEOUT 100
#define IB_ROUTE_TIMEOUT 100


struct ib_hello {
	__u32	magic;
};

static void connect_error(struct rdma_cm_id *cmid)
{
	struct IB_SOCK *sock = cmid->context;

	sock->is_state = IBS_DISCONNECTED;
	wake_up(&sock->is_state_wait);
}

static void connected_handler(struct rdma_cm_id *cmid)
{
	struct IB_SOCK *sock = cmid->context;

	sock->is_state = IBS_CONNECTED;
	wake_up(&sock->is_state_wait);
}

static int disconnected_handler(struct rdma_cm_id *cmid)
{
//	struct IB_SOCK *sock = cmid->context;

	/* XXX */

	connect_error(cmid);
	return 0;
}


static void qp_event_callback(struct ib_event *cause, void *context)
{
	printk("got qp event %d\n",cause->event);
}

#if 0
static void ib_event_handler(struct ib_event_handler *handler,
				struct ib_event *event)
{
	printk("async event %d on device %s port %d\n", event->event,
		event->device->name, event->element.port_num);
}
#endif

static void cq_event_callback(struct ib_event *cause, void *context)
{
	printk("got cq event %d \n", cause->event);
}


/* have some change states  */
/* DID we really needs it ? */
static void cq_callback(struct ib_cq *cq, void *cq_context)
{
	struct IB_SOCK *conn = cq_context;

	printk("cq event %p\n", cq
	/* completion event */
	conn->ic_state = DATA_READY;
	wake_up(&conn->wait);
}

/* creation of CQ/QP isn't needs to create on route event,
 * but it must done before rdma_connect */
static int client_ib_alloc(struct rdma_cm_id *cmid)
{
	struct IB_SOCK *sock = cmid->context;
	struct ib_qp_init_attr	init_attr;
	struct rdma_conn_param	conn_param;
	struct ib_hello		*hello = page_address(sock->is_login);
	int    ret;

	/* event queue */
	sock->is_cq = ib_create_cq(cmid->device,
				   ib_cq_callback,
				   ib_cq_event_callback,
				   (void *)&sock, /* context */
				   IB_CQ_EVENTS_MAX, /* max events in queue 
						     * typically max parallel ops */
				   0);
	if (IS_ERR(sock->is_cq))
		return PTR_ERR(sock->is_cq);

	/* data queue */
	memset(&init_attr, 0, sizeof(init_attr));
	init_attr.event_handler = qp_event_callback;
	init_attr.qp_context	= (void *)&sock;
	init_attr.send_cq	= sock->is_cq;
	init_attr.recv_cq	= sock->is_cq;
	init_attr.cap.max_recv_wr  = 2;
	init_attr.cap.max_send_sge = 2;
	init_attr.cap.max_recv_sge = 1;
	init_attr.sq_sig_type	= IB_SIGNAL_REQ_WR;
	init_attr.qp_type	= IB_QPT_RC;

	ret = rdma_create_qp(cmid, sock->is_pd, &init_attr);
	if (ret != 0)
		return ret;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 4;
	conn_param.initiator_depth     = 1;
	conn_param.retry_count	       = 7;
	conn_param.rnr_retry_count     = 6;

	/* fill a hello message */
	memset(hello, 0, PAGE_SIZE);
	hello->magic = IB_HELLO_MAGIC;
	
	conn_param.private_data		= (void *)hello;
	conn_param.private_data_len	= sizeof(struct ib_hello);

	ret = rdma_connect(cmid, &conn_param);
	if (ret)
		printk("failure connecting: %d\n", ret);

	return ret;
}

static int
client_handler(struct rdma_cm_id *cmid, struct rdma_cm_event *event)
{
	int ret = 0;

	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		ret = rdma_resolve_route(cmid, IB_ROUTE_TIMEOUT);
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		ret = client_ib_alloc(cmid);
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		break;
#if 0
	/* we are save resources, so don't allocate an QP when route 
	 * resolved - do it's now */
	case RDMA_CM_EVENT_CONNECT_RESPONSE:
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}
	if (ret != 0)
		connect_error(cmid);

	return ret;
}

static int server_ib_alloc(struct rdma_cm_id *cmid)
{
	struct IB_SOCK *parent = cmid->context;
	struct IB_SOCK *sock;

	sock = ib_socket_create();
	if (sock == NULL)
		return -ENOMEM;

	list_add(&sock->is_child, &parent->is_child);
	return 0;
}

static int
server_handler(struct rdma_cm_id *cmid, struct rdma_cm_event *event)
{
	int ret = 0;

	switch (event->event) {
	case RDMA_CM_EVENT_CONNECT_REQUEST:
	/* incomming request - lets allocate a resources for a new connect */
		ret = server_ib_alloc(cmid);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
cma_handler(struct rdma_cm_id *cmid, struct rdma_cm_event *event)
{
	int ret = 0;

	printk("event %d status %d conn %p id %p\n",
		event->event, event->status, cmid->context, cmid);

	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
	case RDMA_CM_EVENT_ESTABLISHED:
		ret = client_handler(cmid, event);
		break;
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		ret = server_handler(cmid, event);
		break;
	/* some common errors */
	case RDMA_CM_EVENT_ADDR_ERROR:
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_UNREACHABLE:
	case RDMA_CM_EVENT_REJECTED:
		connect_error(cmid);
		break;
	case RDMA_CM_EVENT_DISCONNECTED:
	case RDMA_CM_EVENT_DEVICE_REMOVAL:
	case RDMA_CM_EVENT_ADDR_CHANGE:
		ret = disconnected_handler(cmid);
		break;
	default:
		printk("Unexpected RDMA CM event (%d)\n", event->event);
		break;
	}
	return ret;
}


struct IB_SOCK *ib_socket_create(void)
{
	struct IB_SOCK *sock;

	sock = kmalloc(sizeof(*sock), GFP_KERNEL);
	if (sock == NULL)
		return NULL;

	sock->is_id = rdma_create_id(cma_handler, NULL, RDMA_PS_TCP, IB_QPT_RC);
	if (IS_ERR(sock->is_id)) {
		printk("error create cm_id %ld\n", PTR_ERR(sock->is_id));
		goto out_free;
	}
	/* create a protection domain. 
	 * XXX - may per device */
	sock->is_pd = ib_alloc_pd(sock->is_id->device);
	if (IS_ERR(sock->is_pd)) {
		printk("can't create a protection domain\n");
		goto out_free;
	}

	init_waitqueue_head(&sock->is_state_wait);
	sock->is_state = IBS_UNKNOW;
	sock->is_state_old = IBS_UNKNOW;
	INIT_LIST_HEAD(&sock->is_child);

	return sock;
out_free:
	ib_socket_destroy(sock);
	return NULL;
}

void ib_socket_destroy(struct IB_SOCK *sock)
{
	BUG_ON(!list_empty(&sock->is_child));

	if (sock->is_pd)
		ib_dealloc_pd(sock->is_pd);
	if (sock->is_id)
		rdma_destroy_id(sock->is_id);
	kfree(sock);
}

int ib_socket_connect(struct IB_SOCK *sock, struct sockaddr_in  *dstaddr)
{
	struct sockaddr_in  srcaddr;
	struct rdma_conn_param	conn_param;
	struct ib_hello	hello;
	int err;

        memset(&srcaddr, 0, sizeof(srcaddr));
        srcaddr.sin_family      = AF_INET;

	/* IB needs to start from resolve addr / route first */
	err = rdma_resolve_addr(sock->is_id, (struct sockaddr *)&srcaddr, 
			        (struct sockaddr *)dstaddr, 
			        IB_ADDR_TIMEOUT /* timeout ms */
			        );
	if (err) {
		printk("can't resolve dst address %d\n", err);
		goto exit;
	}

	/* wait until network operation done */
	if (ib_socket_pool(sock) != IBS_CONNECTED) {
		err = -ENOTCONN;
		goto exit;
	}

	/* ready to send a hello message */
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
	err = rdma_connect(sock->is_id, &conn_param);
	if (err)
		printk("failure connecting: %d\n", err);

exit:
	return err;
}


void ib_socket_disconnect(struct IB_SOCK *sock)
{
	int err = 0;

	/* change the ib conn state only if the conn is UP, however always call
	 * rdma_disconnect since this is the only way to cause the CMA to change
	 * the QP state to ERROR
	 */

	err = rdma_disconnect(sock->is_id);
	if (err)
		printk("Failed to disconnect, conn: 0x%p err %d\n",
			 sock,err);

//	wait_event(ib_conn.wait,
//				 ib_conn.ic_state == DISCONNECTED);

//	return err;
}


enum sock_state ib_socket_pool(struct IB_SOCK *sock)
{
	wait_event(sock->is_state_wait, sock->is_state != sock->is_state_old);
	sock->is_state_old = sock->is_state;

	return sock->is_state_old;
}

int ib_socket_bind(struct IB_SOCK *sock, unsigned port)
{
	struct sockaddr_in  sin;
	int ret;

	sin.sin_family = AF_INET,
	sin.sin_addr.s_addr = (__force u32)htonl(INADDR_ANY);
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
	/* allocate a memory window + premapped page for HELLO data */
out:
	return ret;

}

struct IB_SOCK *ib_socket_accept(struct IB_SOCK *parent)
{
	struct IB_SOCK *sock;

	sock = list_first_entry_or_null(&parent->is_child, struct IB_SOCK, is_child);
	if (sock) {
		list_del_init(&sock->is_child);
	}
	return sock;
}

int ib_socket_send(struct IB_SOCK *sock, void *data, size_t d_size)
{
	return 0;
}

int ib_socket_recv(struct IB_SOCK *sock, void *data, size_t d_size)
{
	return 0;
}
