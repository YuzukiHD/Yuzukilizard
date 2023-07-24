#ifndef _WAIT_QUEUE_H
#define _WAIT_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hal_thread.h>
#include <hal_atomic.h>
#include <hal_time.h>
#include <rtdef.h>
#include <rtthread.h>

#define WQ_FLAG_EXCLUSIVE		0x01
#define WQ_FLAG_WOKEN			0x02
#define WQ_FLAG_BOOKMARK		0x04

/*
 *	NOTE:
 *		waitqueue will use FreeRTOS Notify function, if you task wants
 *		to use waitqueue functions, you should not use notify functions
 *		and waitqueue function at the same time.
 *
 *		FreeRTOS Notify function:
 *			xTaskNotifyStateClear, xTaskNotifyWait, xTaskNotifyTake,xTaskNotify
 *
 *		Useage:
 *			init:
 *				wait_queue_head_t wq;
 *				init_waitqueue_head(&wq);
 *			deinit:
 *				deinit_waitqueue_head(&wq)
 *
 *			task1:
 *				...
 *				if (wait_event_timeout(wq, condition, wait_ms) <= 0) {
 *					...
 *					return -ETIMEOU
 *				}
 *				...
 *			task2:
 *				condition = xxxxx
 *				wake_up(&wq)
 *				...
 */
struct __wait_queue;
typedef struct __wait_queue wait_queue_t;
typedef int (*wait_queue_func_t)(wait_queue_t *wait, void *key);

struct wait_queue_head {
	hal_spinlock_t lock;
	struct list_head head;
};
typedef struct wait_queue_head wait_queue_head_t;

struct __wait_queue {
	unsigned int flags;
	void *priv;
	wait_queue_func_t func;
	struct list_head task_list;
	struct rt_semaphore sem;
};

long prepare_to_wait_event(wait_queue_head_t *q, wait_queue_t *wait);
void init_wait_entry(wait_queue_t *wait, unsigned int flags);
void finish_wait(wait_queue_head_t *q, wait_queue_t *wait);
void __wake_up(wait_queue_head_t *q, int nr_exclusive, void *key);

static inline long __wait_yeild_cpu(wait_queue_t *wait, int ms)
{
#ifndef MS_TO_OSTICK
#define MS_TO_OSTICK(x)			((x) / (1000 / configTICK_RATE_HZ))
#endif
#ifndef OSTICK_TO_MS
#define OSTICK_TO_MS(x)			((x) * (1000 / configTICK_RATE_HZ))
#endif
	rt_tick_t tick = ms < 0 ? RT_WAITING_FOREVER : MS_TO_OSTICK(ms);
	rt_tick_t old, cur;

	old = rt_tick_get();
	rt_sem_take(&wait->sem, tick);
	cur = rt_tick_get();
	return OSTICK_TO_MS(tick - (cur - old));
}

static inline void __add_wait_queue(struct wait_queue_head *wq_head, wait_queue_t *wq_entry)
{
	list_add(&wq_entry->task_list, &wq_head->head);
}

static inline void __add_wait_queue_tail(struct wait_queue_head *wq_head, wait_queue_t *wq_entry)
{
	list_add_tail(&wq_entry->task_list, &wq_head->head);
}

#define init_waitqueue_head(q) \
    do {                                     \
        hal_spin_lock_init(&(q)->lock);  \
        INIT_LIST_HEAD(&(q)->head);      \
    } while (0)

#define deinit_waitqueue_head(q) \
    do {                                     \
        hal_spin_lock_deinit(&(q)->lock);  \
    } while (0)

#define ___wait_event(wq, condition, exclusive, ret, cmd)    \
({                                     \
	wait_queue_t __wait;               \
	long __ret = ret;                  \
                                       \
	init_wait_entry(&__wait, exclusive ? WQ_FLAG_EXCLUSIVE : 0); \
	for (;;) {                         \
		long __int = prepare_to_wait_event(&wq, &__wait); \
                                       \
		if ((condition) || __int)      \
			break;                     \
		cmd;                           \
	}                                  \
	finish_wait(&wq, &__wait);         \
	__ret;                             \
})

#define wait_event(wq_head, condition)        \
  do {                                        \
      if (condition)                          \
          break;                              \
    (void)___wait_event(wq_head, condition, 0, 0, \
             __ret = __wait_yeild_cpu(&__wait, -1)); \
  } while (0)

/*
 *	return 1, if condition == true || remain_main == 0
 *	else return 0
 */
#define ___wait_cond_timeout(condition)                 \
({                                  \
    int __cond = (condition);                  \
    if (__cond && !__ret)                       \
        __ret = 1;                      \
    __cond || !__ret;                       \
})

/*
 * publish function: wait_event_timeout(wq, condition, ms);
 *
 * return:
 * 0 if the @condition evaluated to %false after the @timeout elapsed,
 * 1 if the @condition evaluated to %true after the @timeout elapsed,
 * or the remaining ms (at least 1) if the @condition evaluated,
 * to %true before the @timeout elapsed.
 */
#define wait_event_timeout(wq_head, condition, timeout) \
({                                        \
	int __ret = timeout;                    \
	if (!___wait_cond_timeout(condition))  \
		__ret = ___wait_event(wq_head, \
			___wait_cond_timeout(condition), 0, timeout, \
				__ret = __wait_yeild_cpu(&__wait, __ret)); \
	__ret; \
})

#define wait_event_timeout_exclusive(wq_head, condition, timeout) \
({                                        \
	int __ret = timeout;                    \
	if (!___wait_cond_timeout(condition))  \
		__ret = ___wait_event(wq_head, \
			___wait_cond_timeout(condition), 1, timeout, \
				__ret = __wait_yeild_cpu(&__wait, __ret)) \
	__ret; \
})

/*
 * wake_up function
 * wake_up(wait_queue_head_t *q);
 * wake_up_nr(wait_queue_head_t *q, int nr_exclusive);
 * wake_up_all(wait_queue_head_t *q);
 */
#define wake_up(x)              __wake_up(x, 1, NULL)
#define wake_up_nr(x, nr)       __wake_up(x, nr, NULL)
#define wake_up_all(x)          __wake_up(x, 0, NULL)

#ifdef __cplusplus
}
#endif
#endif
