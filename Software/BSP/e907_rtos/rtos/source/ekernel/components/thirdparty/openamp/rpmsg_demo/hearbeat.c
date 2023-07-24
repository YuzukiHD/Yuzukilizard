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

#define RPMSG_SERVICE_NAME "sunxi,rpmsg_heartbeat"
#define TICK_INTERVAL		100

#if 1
#define debug(fmt, args...)		printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif

struct rpmsg_heartbeat {
	const char *name;
	uint32_t src;
	uint32_t dst;
	struct rpmsg_endpoint *ept;
	osal_timer_t timer;
	int unbound;
	uint32_t tick;
};

struct hearbeat_packet {
	char name[32];
	uint32_t tick;
};

static struct hearbeat_packet packet = {
	.name = CONFIG_RPMSG_REMOTE_NAME,
};

static void time_out_handler(void *arg)
{
	struct rpmsg_heartbeat *ddata = arg;

	if (!ddata->unbound) {
		packet.tick = ddata->tick;
		openamp_rpmsg_send(ddata->ept, &packet, sizeof(packet));
		ddata->tick++;
		ddata->tick %= TICK_INTERVAL;
	}
}

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	return 0;
}

static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	struct rpmsg_heartbeat *heart = ept->priv;

	printf("%s is destroyed\n", ept->name);
	heart->unbound = 1;
	osal_timer_stop(heart->timer);
	osal_timer_delete(heart->timer);
	printf("Stop Rpmsg Hearbeat Timer\r\n");
}

static void cmd_rpmsg_demo_thread(void *arg)
{

	struct rpmsg_heartbeat *heart = NULL;

	heart = hal_malloc(sizeof(*heart));
	if (!heart) {
		printf("Failed to malloc memory\r\n");
		goto out;
	}

	if (openamp_init() != 0) {
		printf("Failed to init openamp framework\r\n");
		goto out;
	}

	heart->name = RPMSG_SERVICE_NAME;
	heart->unbound = 0;
	heart->tick = 0;
	heart->src = RPMSG_ADDR_ANY;
	heart->dst = RPMSG_ADDR_ANY;

	/* Create Timer */
	heart->timer = osal_timer_create("rpmsg-heart", time_out_handler,
					heart, CONFIG_HZ - 10,
					OSAL_TIMER_FLAG_SOFT_TIMER |
					OSAL_TIMER_FLAG_PERIODIC);
	if (heart->timer == NULL) {
		printf("Failed to Create Timer\r\n");
		goto out;
	}

	heart->ept = openamp_ept_open(heart->name, 0, heart->src, heart->dst,
					heart, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (heart->ept == NULL) {
		printf("Failed to Create Endpoint\r\n");
		goto out;
	}

	/* we need to send a tick right away to tell the remote our rproc name */
	time_out_handler(heart);
	osal_timer_start(heart->timer);
	printf("Start Rpmsg Hearbeat Timer\r\n");

out:
	return ;
}

static int rpmsg_heart_init(void)
{
	void *thread;

	thread = kthread_create(cmd_rpmsg_demo_thread, NULL, "rpmsg_heart", 4096, 8);
	if(thread != NULL)
		kthread_start(thread);
	else
		printf("fail to create rpmsg hearbeat thread\r\n");

	return 0;
}
subsys_initcall(rpmsg_heart_init);
