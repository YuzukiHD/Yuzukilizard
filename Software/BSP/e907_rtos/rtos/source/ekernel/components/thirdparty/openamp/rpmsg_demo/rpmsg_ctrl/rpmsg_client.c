#include "rpmsg_master.h"

#define RPMSG_SERVICE_NAME "sunxi,rpmsg_client"

#ifdef CONFIG_RPMSG_CLIENT_DEBUG
#define log					printf
#else
#define log					hal_log_debug
#endif

static const char *ept_name = RPMSG_SERVICE_NAME;
static uint32_t src_addr = RPMSG_ADDR_ANY;
static uint32_t dst_addr = RPMSG_ADDR_ANY;

struct rpmsg_client_status {
	hal_sem_t sem;
	uint32_t status;
};

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	struct rpmsg_ctrl_msg_ack *ack = data;
	struct rpmsg_client_status *sta = priv;

	if (!priv) {
		log("Invalid Data.\r\n");
		return 0;
	}

	log("client: Rx %d Bytes\n", len);

	if (len != sizeof(*ack))
		return 0;

	sta->status = ack->ack;
	hal_sem_post(sta->sem);

	return 0;
}

struct rpmsg_endpoint *
rpmsg_eptdev_create_client(struct rpmsg_ctrl_msg *msg, rpmsg_ept_cb cb,
				rpmsg_ns_unbind_cb ubind, void *priv)
{
	struct rpmsg_endpoint *ept;
	struct rpmsg_client_status sta;
	hal_sem_t sem;
	int ret;

	sta.status = RPMSG_ACK_FAILED;
	sta.sem = hal_sem_create(0);
	if (!sta.sem) {
		hal_log_err("Failed to alloc sem\r\n");
		return NULL;
	}

	ept = openamp_ept_open(ept_name, 0, src_addr, dst_addr, &sta,
					rpmsg_ept_callback, ubind);
	if(ept == NULL) {
		hal_log_err("Failed to Create Endpoint\r\n");
		goto free_sem;
	}

	/* send info to master */
	rpmsg_send(ept, msg, sizeof(struct rpmsg_ctrl_msg));
	/* wait remote ack. try wait 5 secend */
	ret = hal_sem_timedwait(sta.sem, 1 * CONFIG_HZ);
	if (ret) {
		hal_log_err("Timeout wait master ack.\r\n");
		goto free_ept;
	}

	if (sta.status != RPMSG_ACK_OK) {
		hal_log_err("Master not ready. status=0x%lx\r\n", sta.status);
		goto free_ept;
	}

	hal_sem_delete(sta.sem);

	ept->priv = priv;
	ept->cb = cb;

	return ept;
free_ept:
	openamp_ept_close(ept);
free_sem:
	hal_sem_delete(sta.sem);

	return NULL;
}

void rpmsg_eptdev_release_client(struct rpmsg_endpoint *ept)
{
	openamp_ept_close(ept);
}
