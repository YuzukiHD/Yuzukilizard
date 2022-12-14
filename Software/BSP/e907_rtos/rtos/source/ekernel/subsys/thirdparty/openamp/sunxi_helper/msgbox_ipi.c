#include <errno.h>

#include <metal/alloc.h>
#include <openamp/open_amp.h>
#include <hal_msgbox.h>

#include <openamp/sunxi_helper/msgbox_ipi.h>
#include <openamp/sunxi_helper/openamp_log.h>

#ifdef CONFIG_RPMSG_SPEEDTEST
static msgbox_sta_callback full_func = NULL;
static msgbox_sta_callback empty_func = NULL;
#endif
/**
 * msgbox recive data callback
 * it will wakeup recv task
 * */
static void msgbox_recv_callback(uint32_t data, void *priv)
{
	int ret = 0;
	struct msgbox_ipi *ipi = (struct msgbox_ipi *)priv;

	ret = hal_queue_send(ipi->recv_queue, &data);
	if (ret != 0) {
#ifdef CONFIG_RPMSG_SPEEDTEST
		if (full_func)
			full_func();
#else
		openamp_errFromISR("%s:recv_queue is full\r\n", ipi->info.name);
#endif
	}
}

static void msgbox_recv_handler_task(void *params)
{
	struct msgbox_ipi *ipi = (struct msgbox_ipi *)params;
	uint32_t data = 0;
	uint32_t ret;

	openamp_dbg("%s: recv task start\r\n",ipi->info.name);
	while(1) {
#ifdef CONFIG_RPMSG_SPEEDTEST
		ret = hal_is_queue_empty(ipi->recv_queue);
		if (ret && empty_func)
			empty_func();
#endif
		/* wait forever */
		ret = hal_queue_recv(ipi->recv_queue, &data, -1);
		if(ret != 0)
			continue;
		if(ipi->callback)
			ipi->callback(ipi->priv, data);
	}
}

struct msgbox_ipi *msgbox_ipi_create(struct msgbox_ipi_info *info,
				msgbox_ipi_callback callback, void *priv)
{
	int ret = 0;
	struct msgbox_ipi *ipi = NULL;
	struct msg_endpoint *ept = NULL;

	ipi = metal_allocate_memory(sizeof(*ipi));
	if(!ipi) {
		openamp_err("%s: Failed to allocate memory for ipi handle\n", info->name);
		ret = -ENOMEM;
		goto out;
	}
	memcpy(&ipi->info, info, sizeof(*info));
	ipi->callback = callback;
	ipi->priv = priv;

	ipi->recv_queue = hal_queue_create(info->name, sizeof(uint32_t), info->queue_size);
	if(!ipi->recv_queue) {
		openamp_err("%s: Failed to allocate memory for queue\n", info->name);
		ret  = -ENOMEM;
		goto free_ipi;
	}
	/* hal_msgbox init */
	ept = metal_allocate_memory(sizeof(*ept));
	if(!ept) {
		openamp_err("%s: Failed to allocate memory for mgs endpoint\n", info->name);
		ret  = -ENOMEM;
		goto free_queue;
	}
	/* config ept recv callback */
	ept->rec = msgbox_recv_callback;
	ept->private = (void *)ipi;
	ipi->ept = ept;
	/* alloc msgbox channel */
	ret = hal_msgbox_alloc_channel(ept, info->remote_id, info->read_ch, info->write_ch);
	if(ret != 0) {
		openamp_err("Failed to allocate msgbox channel\n");
		goto free_ept;
	}

	/* create recv task */
	ipi->msgbox_task_handler = kthread_create(msgbox_recv_handler_task, (void *)ipi, info->name,
			HAL_THREAD_STACK_SIZE, HAL_THREAD_PRIORITY);
	if(ipi->msgbox_task_handler == NULL)
		goto free_channel;
	kthread_start(ipi->msgbox_task_handler);

	return ipi;

free_channel:
	hal_msgbox_free_channel(ept);
free_ept:
	metal_free_memory(ept);
free_queue:
	hal_queue_delete(ipi->recv_queue);
free_ipi:
	metal_free_memory(ipi);
out:
	return NULL;
}

void msgbox_ipi_release(struct msgbox_ipi *ipi)
{
	if(!ipi) {
		openamp_err("Invalid msgbox_ini input\r\n");
		return;
	}

	kthread_stop(ipi->msgbox_task_handler);
	hal_msgbox_free_channel(ipi->ept);
	metal_free_memory(ipi->ept);
	ipi->ept = NULL;
	hal_queue_delete(ipi->recv_queue);
	ipi->recv_queue = NULL;
	metal_free_memory(ipi);
}

/* it will be called when remote connected */
int msgbox_ipi_notify(struct msgbox_ipi *ipi, uint32_t data)
{
	int ret;

	ret = hal_msgbox_channel_send(ipi->ept, (uint8_t *)&data, sizeof(data));
	if(ret != 0) {
		openamp_err("%s: Failed to send message to remote\r\n",ipi->info.name);
		return -1;
	}
	return 0;
}

#ifdef CONFIG_RPMSG_SPEEDTEST
void msgbox_set_full_callback(msgbox_sta_callback call)
{
	full_func = call;
}

void msgbox_set_empty_callback(msgbox_sta_callback call)
{
	empty_func = call;
}

void msgbox_reset_full_callback(void)
{
	full_func = NULL;
}

void msgbox_reset_empty_callback(void)
{
	empty_func = NULL;
}


#endif

