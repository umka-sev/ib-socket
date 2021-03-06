#include "ib-sock.h"
#include "ib-sock-int.h"

const char *ib_event_type_str(enum ib_event_type ev_type)
{
	switch (ev_type) {
	case IB_EVENT_COMM_EST:
		return "COMM_EST";
	case IB_EVENT_QP_FATAL:
		return "QP_FATAL";
	case IB_EVENT_QP_REQ_ERR:
		return "QP_REQ_ERR";
	case IB_EVENT_QP_ACCESS_ERR:
		return "QP_ACCESS_ERR";
	case IB_EVENT_SQ_DRAINED:
		return "SQ_DRAINED";
	case IB_EVENT_PATH_MIG:
		return "PATH_MIG";
	case IB_EVENT_PATH_MIG_ERR:
		return "PATH_MIG_ERR";
	case IB_EVENT_QP_LAST_WQE_REACHED:
		return "QP_LAST_WQE_REACHED";
	case IB_EVENT_CQ_ERR:
		return "CQ_ERR";
	case IB_EVENT_SRQ_ERR:
		return "SRQ_ERR";
	case IB_EVENT_SRQ_LIMIT_REACHED:
		return "SRQ_LIMIT_REACHED";
	case IB_EVENT_PORT_ACTIVE:
		return "PORT_ACTIVE";
	case IB_EVENT_PORT_ERR:
		return "PORT_ERR";
	case IB_EVENT_LID_CHANGE:
		return "LID_CHANGE";
	case IB_EVENT_PKEY_CHANGE:
		return "PKEY_CHANGE";
	case IB_EVENT_SM_CHANGE:
		return "SM_CHANGE";
	case IB_EVENT_CLIENT_REREGISTER:
		return "CLIENT_REREGISTER";
	case IB_EVENT_DEVICE_FATAL:
		return "DEVICE_FATAL";
	default:
		return "UNKNOWN";
	}
}

const char *wr_status_str(enum ib_wc_status status)
{
	switch (status) {
	case IB_WC_SUCCESS:
		return "WC_SUCCESS";

	case IB_WC_LOC_LEN_ERR:
		return "WC_LOC_LEN_ERR";

	case IB_WC_LOC_QP_OP_ERR:
		return "WC_LOC_QP_OP_ERR";

	case IB_WC_LOC_EEC_OP_ERR:
		return "WC_LOC_EEC_OP_ERR";

	case IB_WC_LOC_PROT_ERR:
		return "WC_LOC_PROT_ERR";

	case IB_WC_WR_FLUSH_ERR:
		return "WC_WR_FLUSH_ERR";

	case IB_WC_MW_BIND_ERR:
		return "WC_MW_BIND_ERR";

	case IB_WC_BAD_RESP_ERR:
		return "WC_BAD_RESP_ERR";

	case IB_WC_LOC_ACCESS_ERR:
		return "WC_LOC_ACCESS_ERR";

	case IB_WC_REM_INV_REQ_ERR:
		return "WC_REM_INV_REQ_ERR";

	case IB_WC_REM_ACCESS_ERR:
		return "WC_REM_ACCESS_ERR";

	case IB_WC_REM_OP_ERR:
		return "WC_REM_OP_ERR";

	case IB_WC_RETRY_EXC_ERR:
		return "WC_RETRY_EXC_ERR";

	case IB_WC_RNR_RETRY_EXC_ERR:
		return "WC_RNR_RETRY_EXC_ERR";

	case IB_WC_LOC_RDD_VIOL_ERR:
		return "WC_LOC_RDD_VIOL_ERR";

	case IB_WC_REM_INV_RD_REQ_ERR:
		return "WC_REM_INV_RD_REQ_ERR";

	case IB_WC_REM_ABORT_ERR:
		return "WC_REM_ABORT_ERR";

	case IB_WC_INV_EECN_ERR:
		return "WC_INV_EECN_ERR";

	case IB_WC_INV_EEC_STATE_ERR:
		return "WC_INV_EEC_STATE_ERR";

	case IB_WC_FATAL_ERR:
		return "WC_FATAL_ERR";

	case IB_WC_RESP_TIMEOUT_ERR:
		return "WC_RESP_TIMEOUT_ERR";

	case IB_WC_GENERAL_ERR:
		return "WC_GENERAL_ERR";

	default:
		return "UNKNOWN";
	}
}

char *cm_event_type_str(enum rdma_cm_event_type ev_type)
{
	switch (ev_type) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		return "ADDRESS_RESOLVED";
	case RDMA_CM_EVENT_ADDR_ERROR:
		return "ADDESS_ERROR";
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		return "ROUTE_RESOLVED";
	case RDMA_CM_EVENT_ROUTE_ERROR:
		return "ROUTE_ERROR";
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		return "CONNECT_REQUEST";
	case RDMA_CM_EVENT_CONNECT_RESPONSE:
		return "CONNECT_RESPONSE";
	case RDMA_CM_EVENT_CONNECT_ERROR:
		return "CONNECT_ERROR";
	case RDMA_CM_EVENT_UNREACHABLE:
		return "UNREACHABLE";
	case RDMA_CM_EVENT_REJECTED:
		return "REJECTED";
	case RDMA_CM_EVENT_ESTABLISHED:
		return "ESTABLISHED";
	case RDMA_CM_EVENT_DISCONNECTED:
		return "DISCONNECTED";
	case RDMA_CM_EVENT_DEVICE_REMOVAL:
		return "DEVICE_REMOVAL";
	case RDMA_CM_EVENT_MULTICAST_JOIN:
		return "MULTICAST_JOIN";
	case RDMA_CM_EVENT_MULTICAST_ERROR:
		return "MULTICAST_ERROR";
	case RDMA_CM_EVENT_ADDR_CHANGE:
		return "ADDR_CHANGE";
	case RDMA_CM_EVENT_TIMEWAIT_EXIT:
		return "TIMEWAIT_EXIT";
	default:
		return "UNKNOWN";
	}
}