#include <errno.h>
#include <aw_list.h>
#include <openamp/rpmsg.h>
#include <hal_thread.h>
#include <init.h>
#include "rpbuf_internal.h"

#define RPMSG_RPBUF_SERVICE_NAME "rpbuf-service"
#define RPMSG_RPBUF_SERVICE_SRC_ADDR RPMSG_ADDR_ANY
#define RPMSG_RPBUF_SERVICE_DST_ADDR RPMSG_ADDR_ANY

struct rpmsg_rpbuf_service_instance {
	struct rpmsg_endpoint ept;
};

LIST_HEAD(__rpbuf_services);
static rpbuf_mutex_t __rpbuf_services_lock;

static int rpbuf_services_lock_create(void)
{
	__rpbuf_services_lock = rpbuf_mutex_create();
	return 0;
}
core_initcall(rpbuf_services_lock_create);

/* will nerver execute */
static void rpbuf_services_lock_delete(void)
{
	rpbuf_mutex_delete(__rpbuf_services_lock);
}

static int rpmsg_rpbuf_service_ept_cb(struct rpmsg_endpoint *ept, void *data,
				      size_t len, uint32_t src, void *priv)
{
	struct rpbuf_service *service = priv;
	int ret;

	ret = rpbuf_service_get_notification(service, data, len);
	if (ret < 0) {
		rpbuf_err("rpbuf_service_get_notification failed\n");
	}

	/*
	 * Return 0 anyway because openamp checks the returned value of endpoint
	 * callback. We cannot return a negative value otherwise it will trigger
	 * the assertion failure.
	 */
	return 0;
}

static int rpmsg_rpbuf_service_notify(void *msg, int msg_len, void *priv)
{
	struct rpmsg_rpbuf_service_instance *inst = priv;
	struct rpmsg_endpoint *ept = &inst->ept;

	if (!is_rpmsg_ept_ready(ept)) {
		rpbuf_err("endpoint not ready\n");
		return -EACCES;
	}

	return rpmsg_trysend(ept, msg, msg_len);
}

static const struct rpbuf_service_ops rpmsg_rpbuf_service_ops = {
	.notify = rpmsg_rpbuf_service_notify,
};

/* 'token' must be a rpmsg_device */
struct rpbuf_service *rpbuf_init_service(int id, void *token)
{
	int ret;
	struct rpmsg_device *rpmsg_dev = token;
	struct rpmsg_rpbuf_service_instance *inst;
	struct rpbuf_service *service;

	if (!token) {
		rpbuf_err("invalid token\n");
		goto err_out;
	}

	inst = rpbuf_zalloc(sizeof(struct rpmsg_rpbuf_service_instance));
	if (!inst) {
		rpbuf_err("rpbuf_zalloc failed\n");
		goto err_out;
	}

	ret = rpmsg_create_ept(&inst->ept, rpmsg_dev, RPMSG_RPBUF_SERVICE_NAME,
			       RPMSG_RPBUF_SERVICE_SRC_ADDR, RPMSG_RPBUF_SERVICE_DST_ADDR,
			       rpmsg_rpbuf_service_ept_cb, NULL);
	if (ret != 0) {
		rpbuf_err("rpmsg_create_ept failed: %d\n", ret);
		goto err_free_inst;
	}

	service = rpbuf_create_service(id, &rpmsg_rpbuf_service_ops, (void *)inst);
	if (!service) {
		rpbuf_err("rpbuf_create_service failed\n");
		goto err_destroy_ept;
	}

	inst->ept.priv = (void *)service;

	ret = rpbuf_register_service(service, (void *)rpmsg_dev);
	if (ret < 0) {
		rpbuf_err("rpbuf_register_service failed\n");
		goto err_destroy_service;
	}

	rpbuf_dbg("Waiting for rpbuf ept ready.\r\n");

	while (!is_rpmsg_ept_ready(&inst->ept))
		kthread_msleep(1);

	rpbuf_dbg("%s ready: src(0x%x) dst(0x%x)\r\n", RPMSG_RPBUF_SERVICE_NAME,
					inst->ept.addr, inst->ept.dest_addr);

	rpbuf_mutex_lock(__rpbuf_services_lock);
	list_add_tail(&service->list, &__rpbuf_services);
	rpbuf_mutex_unlock(__rpbuf_services_lock);

	return service;

err_destroy_service:
	rpbuf_destroy_service(service);
err_destroy_ept:
	rpmsg_destroy_ept(&inst->ept);
err_free_inst:
	rpbuf_free(inst);
err_out:
	return NULL;
}

void rpbuf_deinit_service(struct rpbuf_service *service)
{
	struct rpmsg_rpbuf_service_instance *inst = service->priv;

	if (!service) {
		rpbuf_err("invalid rpbuf_service ptr\n");
		return;
	}

	rpbuf_mutex_lock(__rpbuf_services_lock);
	list_del(&service->list);
	rpbuf_mutex_unlock(__rpbuf_services_lock);

	rpbuf_unregister_service(service);
	rpbuf_destroy_service(service);

	rpmsg_destroy_ept(&inst->ept);
	rpbuf_free(inst);
}

struct rpbuf_service *rpbuf_get_service_by_id(int id)
{
	struct rpbuf_service *service;

	rpbuf_mutex_lock(__rpbuf_services_lock);
	list_for_each_entry(service, &__rpbuf_services, list) {
		if (service->id == id) {
			rpbuf_mutex_unlock(__rpbuf_services_lock);
			return service;
		}
	}
	rpbuf_mutex_unlock(__rpbuf_services_lock);

	return NULL;
}

