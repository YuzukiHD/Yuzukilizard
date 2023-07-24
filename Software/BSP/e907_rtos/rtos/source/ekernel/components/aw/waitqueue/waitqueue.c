#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <aw_list.h>
#include <hal_interrupt.h>
#include <hal_thread.h>
#include "./include/aw_waitqueue.h"

static int default_wake_function(wait_queue_t *curr, void *key)
{
	(void)key;
	return rt_sem_release(&curr->sem);
}

int autoremove_wake_function(wait_queue_t *wait, void *key)
{
	int ret = default_wake_function(wait, key);

	if (!ret)
		list_del_init(&wait->task_list);
	/* return 0 if wake up successed */
	return ret;
}

void init_wait_entry(wait_queue_t *wait, unsigned int flags)
{
	wait->flags = flags;
	wait->priv = kthread_self();;
	wait->func = autoremove_wake_function;
	INIT_LIST_HEAD(&wait->task_list);
	rt_sem_init(&wait->sem, "wq", 0, RT_IPC_FLAG_FIFO);
}

long prepare_to_wait_event(wait_queue_head_t *q, wait_queue_t *wait)
{
	unsigned long flags;
	long ret = 0;

	flags = hal_spin_lock_irqsave(&q->lock);
	if (list_empty(&wait->task_list)) {
		if (wait->flags & WQ_FLAG_EXCLUSIVE)
			__add_wait_queue_tail(q, wait);
		else
			__add_wait_queue(q, wait);
	}
	else {
		ret = -EALREADY;
	}
	hal_spin_unlock_irqrestore(&q->lock, flags);

	return ret;
}

void finish_wait(wait_queue_head_t *q, wait_queue_t *wait)
{
	unsigned long flags;

	if (!list_empty_careful(&wait->task_list)) {
		flags = hal_spin_lock_irqsave(&q->lock);
		list_del_init(&wait->task_list);
		hal_spin_unlock_irqrestore(&q->lock, flags);
	}
	rt_sem_detach(&wait->sem);
}

static void __wake_up_common(wait_queue_head_t *q, int nr_exclusive,
				void *key)
{
	wait_queue_t *curr, *next;

	list_for_each_entry_safe(curr, next, &q->head, task_list) {
	unsigned flags = curr->flags;

	if (!curr->func(curr, key) &&
		(flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive)
		break;
	}
}

void __wake_up(wait_queue_head_t *q, int nr_exclusive, void *key)
{
	unsigned long flags;

	flags = hal_spin_lock_irqsave(&q->lock);
	__wake_up_common(q, nr_exclusive, key);
	hal_spin_unlock_irqrestore(&q->lock, flags);
}
