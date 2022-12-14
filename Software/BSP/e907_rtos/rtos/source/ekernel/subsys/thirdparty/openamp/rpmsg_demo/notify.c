#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <init.h>
#include <openamp/sunxi_helper/openamp.h>
#include <openamp/sunxi_helper/msgbox_ipi.h>

#define RPMSG_SERVICE_NAME "sunxi,notify"
#define RPMSG_NOTIFY_MAX_LEN			32

#if 1
#define debug(fmt, args...)		printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif

static struct rpmsg_endpoint *srm_ept = NULL;

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	return 0;
}

static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	debug("%s is destroyed\n", ept->name);
	srm_ept = NULL;
}

int rpmsg_notify(char *name, void *data, int len)
{
	uint8_t buf[RPMSG_MAX_LEN];

	if (!srm_ept) {
		debug("Please call srm_init.\n");
		return -ENXIO;
	}

	memcpy(buf, name, strlen(name));
	memset(buf + strlen(name), 0, RPMSG_NOTIFY_MAX_LEN - strlen(name));
	if (data)
		memcpy(buf + RPMSG_NOTIFY_MAX_LEN, data, len);
	else
		len = 0;

	openamp_rpmsg_send(srm_ept, buf, RPMSG_NOTIFY_MAX_LEN + len);

	return 0;
}

static void cmd_rpmsg_demo_thread(void *arg)
{
	struct rpmsg_endpoint *ept;

	if (openamp_init() != 0) {
		printf("Failed to init openamp framework\r\n");
		return;
	}

	ept = openamp_ept_open(RPMSG_SERVICE_NAME, 0, RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
					NULL, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (ept == NULL)
		printf("Failed to Create Endpoint\r\n");
	srm_ept = ept;

	return;
}

static int rpmsg_notify_init(void)
{
	void *thread;

	thread = kthread_create(cmd_rpmsg_demo_thread, NULL, "rpmsg_srm", 2048, 8);
	if(thread != NULL)
		kthread_start(thread);
	else
		printf("fail to create rpmsg hearbeat thread\r\n");

	return 0;
}
device_initcall(rpmsg_notify_init);
