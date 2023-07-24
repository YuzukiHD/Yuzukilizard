/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <hal_queue.h>
#include <rtdef.h>
#include <log.h>
#include <waitqueue.h>
#include <hal_time.h>

hal_mailbox_t hal_mailbox_create(const char *name, unsigned int size)
{
    return rt_mb_create(name, size, RT_IPC_FLAG_FIFO);
}

int hal_mailbox_delete(hal_mailbox_t mailbox)
{
    if (mailbox == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    rt_mb_delete(mailbox);
    return 0;
}

int hal_mailbox_send(hal_mailbox_t mailbox, unsigned int value)
{
    rt_err_t ret;

    if (mailbox == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_mb_send(mailbox, value);
    if (ret != RT_EOK)
    {
        // timeout.
        return -2;
    }

    return 0;
}

int hal_mailbox_send_wait(hal_mailbox_t mailbox, unsigned int value, int timeout)
{
    rt_err_t ret;

    if (mailbox == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_mb_send_wait(mailbox, value, timeout);
    if (ret != RT_EOK)
    {
        // timeout.
        return -2;
    }

    return 0;
}

int hal_mailbox_recv(hal_mailbox_t mailbox, unsigned int *value, int timeout)
{
    rt_err_t ret;

    if (mailbox == NULL || value == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_mb_recv(mailbox, (rt_ubase_t *)value, timeout);
    if (ret != RT_EOK)
    {
        return -2;
    }

    return 0;
}

int hal_is_mailbox_empty(hal_mailbox_t mailbox)
{
    rt_err_t ret = -1;
    int arg;

    if (mailbox == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_mb_control(mailbox, RT_IPC_CMD_GET_STATE, &arg);
    if (ret != RT_EOK)
        return -2;

    return (arg == 0)? 1: 0;
}

hal_workqueue *hal_workqueue_create(const char *name, unsigned short stack_size, unsigned char priority)
{
	return rt_workqueue_create(name, stack_size, priority);
}

int hal_workqueue_destroy(hal_workqueue *queue)
{
	int ret = 0;
	ret = rt_workqueue_destroy(queue);
	return 0;
}

int hal_workqueue_dowork(hal_workqueue *queue, hal_work *work)
{
	int ret = 0;
	ret = rt_workqueue_dowork(queue, work);
	return ret;
}

int hal_workqueue_submit_work(hal_workqueue *queue, hal_work *work, hal_tick_t time)
{
	int ret = 0;
	ret = rt_workqueue_submit_work(queue, work, time);
	return ret;
}

int hal_workqueue_cancel_work(hal_workqueue *queue, hal_work *work)
{
	int ret = 0;
	ret = rt_workqueue_cancel_work(queue, work);
	return ret;
}

int hal_workqueue_cancel_work_sync(hal_workqueue *queue, hal_work *work)
{
	int ret = 0;
	ret = rt_workqueue_cancel_work_sync(queue, work);
	return ret;
}

hal_queue_t hal_queue_create(const char *name, unsigned int item_size, unsigned int queue_size)
{
    return rt_mq_create(name, item_size, queue_size, RT_IPC_FLAG_FIFO);
}

int hal_queue_delete(hal_queue_t queue)
{
    if (queue == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    rt_mq_delete(queue);
    return 0;

}

int hal_queue_send_wait(hal_queue_t queue, void *buffer, int timeout)
{
    rt_err_t ret;

    if (queue == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_mq_send_wait(queue, buffer, queue->msg_size, timeout);
    if (ret != RT_EOK)
    {
        __err("queue_send fatal error,ret = %d\n", ret);
        return -2;
    }

    return 0;
}

int hal_queue_send(hal_queue_t queue, void *buffer)
{
    return hal_queue_send_wait(queue, buffer, 0);
}

int hal_queue_recv(hal_queue_t queue, void *buffer, int timeout)
{
    rt_err_t ret;

    if (queue == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_mq_recv(queue, buffer, queue->msg_size, timeout);
    if (ret != RT_EOK)
    {
        return -2;
    }

    return 0;

}

int hal_is_queue_empty(hal_queue_t queue)
{
    rt_err_t ret = -1;
    int arg;

    if (queue == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    ret = rt_mq_control(queue, RT_IPC_CMD_GET_STATE, &arg);
    if (ret != RT_EOK)
        return -2;

    return (arg == 0)? 1: 0;
}

int hal_queue_len(hal_queue_t queue)
{
    rt_mq_t mq = queue;

    if (queue == NULL)
    {
        __err("fatal error, parameter is invalid.");
        return -1;
    }

    return mq->entry;
}

void hal_wqueue_init(hal_wqueue_t *queue)
{
	rt_wqueue_init(queue);

}

int hal_wqueue_wait(hal_wqueue_t *queue, int condition, int timeout)
{
	return rt_wqueue_wait(queue, condition, timeout);
}

void hal_wqueue_wakeup(hal_wqueue_t *queue, void *key)
{
	rt_wqueue_wakeup(queue, key);
}
