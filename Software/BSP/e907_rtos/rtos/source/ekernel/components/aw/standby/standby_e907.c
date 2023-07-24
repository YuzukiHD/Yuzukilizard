#include <hal_osal.h>
#include <hal_log.h>
#include <hal_msgbox.h>
#include <errno.h>
#include <hal_uart.h>
#include <standby/standby.h>
#include "pm_internal.h"

// #define DEBUG

#define MSGBOX_RISCV 1
#define MSGBOX_ARM   0

/* msgbox config */
#define MSGBOX_REMOTE		MSGBOX_ARM
#define MSGBOX_R_CH			CONFIG_STANDBY_MSGBOX_CHANNEL
#define MSGBOX_W_CH			CONFIG_STANDBY_MSGBOX_CHANNEL
#define IRQ_MSGBOX			CONFIG_STANDBY_MSGBOX_IRQ

#define PM_POWER_SUSPEND	0xf3f30101
#define PM_POWER_ACK		0xf3f30102
#define PM_POWER_DEAD		0xf3f30103
#define PM_POWER_RESUME		0xf3f30204

#ifdef DEBUG
#define debug(fmt, args...)		hal_log_debug(fmt, ##args)
#else
#define debug(fmt, ...)
#endif

extern void irq_suspend(void);
extern void irq_resume(void);
extern void clic_resume(void);
extern void clic_suspend(void);

extern void cpu_suspend(void);
extern long g_sleep_flag;

struct sunxi_pm {
	struct msg_endpoint ept;
	void *thread;
	hal_queue_t recv_queue;
};

static void msgbox_recv_callback(uint32_t data, void *priv)
{
	int ret = 0;
	struct sunxi_pm *inst = priv;

#ifdef DEBUG
	int len;
	len = hal_queue_len(inst->recv_queue);
	debug("queue len:%d -> %d:data:%x\n", len, len + 1, data);
#endif
	ret = hal_queue_send(inst->recv_queue, &data);
	if (ret != 0)
		hal_log_warn("fail to send data to queue\n");
}

static void sunxi_standby_send_ack(struct sunxi_pm *inst)
{
	uint32_t data = PM_POWER_ACK;

	hal_msgbox_channel_send(&inst->ept, (uint8_t *)&data, sizeof(uint32_t));
}

static void sunxi_standby_send_will(struct sunxi_pm *inst)
{
	uint32_t data = PM_POWER_DEAD;

	debug("send will");
	hal_msgbox_channel_send(&inst->ept, (uint8_t *)&data, sizeof(uint32_t));
}

static void cpu_to_low_power(struct sunxi_pm *inst)
{
	clic_suspend();
	cpu_suspend();
	if (g_sleep_flag) {
		debug("enter standby");
		hal_dcache_clean_all();
		sunxi_standby_send_will(inst);
		__asm__ __volatile__("wfi");
	}
	debug("leave standby");
	clic_resume();
}

static void standby_suspend_prepare(struct sunxi_pm *inst)
{
	/* do something before suspend */
	dev_pm_notify_call(DEV_NOTIFY_SUSPEND);
	return;
}

static void standby_resume_prepare(struct sunxi_pm *inst)
{
	/* do something before resume */
	dev_pm_notify_call(DEV_NOTIFY_RESUME);
	return;
}

static void standby_enter(struct sunxi_pm *inst)
{
	hal_interrupt_disable();

	/* disable all interrupt */
	irq_suspend();
	/* only enable msgbox interrupt */
	hal_enable_irq(IRQ_MSGBOX);

	cpu_to_low_power(inst);

	irq_resume();
	standby_resume_prepare(inst);

	hal_interrupt_enable();
}

static void standby_thread(void *arg)
{
	int ret = 0;
#ifdef DEBUG
	int len;
#endif
	uint32_t data = 0;
	struct sunxi_pm *inst = arg;

	while (1) {
		data = 0;
		ret = hal_queue_recv(inst->recv_queue, &data, -1);
		if (ret != 0) {
			hal_log_warn("fail to read data from queue\r\n");
			continue;
		}

#ifdef DEBUG
		len = hal_queue_len(inst->recv_queue);
		debug("queue len:%d -> %d:data:%x\n", len + 1, len, data);
#endif

		switch (data) {
		case PM_POWER_SUSPEND:
			debug("send suspend ack");
			sunxi_standby_send_ack(inst);
			standby_suspend_prepare(inst);
			standby_enter(inst);
			debug("standby end");
			break;

		case PM_POWER_RESUME:
			debug("send resume ack");
			sunxi_standby_send_ack(inst);
			break;

		default:
			hal_log_warn("Undown data:0x%08x\r\n", data);
		}
	}
}

int pm_standby_init(void)
{
	int ret = 0;
	struct sunxi_pm *inst = NULL;

	inst = hal_malloc(sizeof(*inst));
	if (!inst) {
		hal_log_err("fail to alloc mem\r\n");
		ret = -ENOMEM;
		goto out;
	}

	inst->ept.rec = msgbox_recv_callback;
	inst->ept.private = inst;

	ret = hal_msgbox_alloc_channel(&inst->ept, MSGBOX_REMOTE, MSGBOX_R_CH, MSGBOX_W_CH);
	if (ret) {
		hal_log_err("fail to alloc msgbox channel(%d %d %d)\n", MSGBOX_REMOTE,
						MSGBOX_R_CH, MSGBOX_W_CH);
		ret = -EINVAL;
		goto free_inst;
	}

	inst->recv_queue = hal_queue_create("standby-queue", sizeof(uint32_t), 4);
	if (!inst->recv_queue) {
		hal_log_err("fail to create queue\r\n");
		ret = -ENOMEM;
		goto free_channel;
	}

	inst->thread = kthread_create(standby_thread, inst, "standby", 4 * 1024, 1);
	if (!inst->thread) {
		hal_log_err("fail to create standby thread\r\n");
		ret = -ENOMEM;
		goto free_queue;
	}
	kthread_start(inst->thread);

	return 0;

free_queue:
	hal_queue_delete(inst->recv_queue);
free_channel:
	hal_msgbox_free_channel(&inst->ept);
free_inst:
	hal_free(inst);
out:
	return ret;
}

