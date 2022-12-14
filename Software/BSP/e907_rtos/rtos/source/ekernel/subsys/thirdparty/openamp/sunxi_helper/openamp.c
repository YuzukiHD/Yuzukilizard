#include <hal_osal.h>
#include <openamp/sunxi_helper/openamp.h>
#include <hal_msgbox.h>
#include <melis/init.h>

static int is_init = 0;
static hal_mutex_t lock = NULL;

int openamp_init(void)
{
	int ret;
	struct remoteproc *rproc;
	struct rpmsg_device *rpmsg_dev;

	if (lock == NULL)
		lock = hal_mutex_create();

	hal_mutex_lock(lock);

	if (is_init) {
		hal_mutex_unlock(lock);
		return 0;
	}

	rproc = openamp_sunxi_create_rproc(0, 0);
	if (!rproc) {
		openamp_err("Failed to create remoteproc\n");
		ret = -1;
		goto out;
	}

	rpmsg_dev = openamp_sunxi_create_rpmsg_vdev(rproc,
					0, VIRTIO_DEV_SLAVE, NULL, NULL);
	if (!rpmsg_dev) {
		openamp_err("Failed to create rpmsg virtio device\n");
		ret = -2;
		goto release_rproc;
	}

	openamp_dbg("\n");

	is_init = 1;

	hal_mutex_unlock(lock);

	return 0;

release_rproc:
	openamp_sunxi_release_rproc(rproc);
out:
	hal_mutex_unlock(lock);
	return ret;
}

static void openamp_init_thread(void *arg)
{
	openamp_init();
}
static int openamp_init_async(void)
{
	void *thread;

	thread = kthread_create(openamp_init_thread, NULL,
					"openamp_init", 2 * 1024, 8);
	if (thread)
		kthread_start(thread);

	return 0;
}
subsys_initcall(openamp_init_async);

void openamp_deinit(void)
{
	struct remoteproc *rproc = openamp_sunxi_get_rproc(0);
	struct rpmsg_device *rpmsg_dev = openamp_sunxi_get_rpmsg_vdev(0);

	hal_mutex_lock(lock);
	openamp_sunxi_release_rpmsg_vdev(rproc, rpmsg_dev);
	openamp_sunxi_release_rproc(rproc);
	is_init = 0;
	hal_mutex_unlock(lock);
}

struct rpmsg_endpoint *openamp_ept_open(const char *name, int rpmsg_id,
				uint32_t src_addr, uint32_t dst_addr,void *priv,
				rpmsg_ept_cb cb, rpmsg_ns_unbind_cb unbind_cb)
{
	int ret;
	struct rpmsg_endpoint *ept = NULL;
	struct rpmsg_device *rpmsg_dev;

	if (!is_init) {
		openamp_err("Please call openamp_init to init openamp\r\n");
		return NULL;
	}

	rpmsg_dev = openamp_sunxi_get_rpmsg_vdev(rpmsg_id);
	if(!rpmsg_dev) {
		openamp_info("Can't find rpmsg device by id(%d)", rpmsg_id);
		goto out;
	}

	ept = hal_malloc(sizeof(*ept));
	if(!ept) {
		openamp_err("Failed to alloc %s rpmsg endpoint\n", name);
		goto out;
	}

	ret = rpmsg_create_ept(ept, rpmsg_dev, name,
			src_addr, dst_addr, cb, unbind_cb);

	if (ret != 0) {
		openamp_err("Failed to create rpmsg endpoint name=%s (ret: %d)\n", name, ret);
		goto free_ept;
	}

	ept->priv = priv;
	openamp_info("Waiting for rpmsg endpoint ready to send\r\n");
	while (!is_rpmsg_ept_ready(ept)) {
		kthread_mdelay(1);
	}
	openamp_dbg("endpoint ready: src(0x%x) dst(0x%x)\r\n",
					ept->addr, ept->dest_addr);

	return ept;
free_ept:
	hal_free(ept);
out:
	return NULL;
}

void openamp_ept_close(struct rpmsg_endpoint *ept)
{
	if(!ept) {
		openamp_err("Invalid arguent(ept=null)\r\n");
		return;
	}
	rpmsg_destroy_ept(ept);
	hal_free(ept);
}

int openamp_rpmsg_send(struct rpmsg_endpoint *ept, void *data, uint32_t len)
{
	int ret;
	if(ept == NULL) {
		openamp_err("Invalid endpoint arg(null)\r\n");
		return -1;
	}

	ret = rpmsg_send(ept, data, len);
	if (ret < 0)
		openamp_err("Failed to send data\n");
	return ret;
}
