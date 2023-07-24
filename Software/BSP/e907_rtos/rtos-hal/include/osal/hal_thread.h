#ifndef SUNXI_HAL_THREAD_H
#define SUNXI_HAL_THREAD_H

#ifdef __cplusplus
extern "C"
{
#endif

#define HAL_THREAD_STACK_SIZE    (0x2000)
#define HAL_THREAD_PRIORITY      (    15)
#define HAL_THREAD_TIMESLICE     (    10)

#if defined(CONFIG_KERNEL_FREERTOS)
#include <FreeRTOS.h>
#include <task.h>
typedef TaskHandle_t hal_thread_t;
#else
#include <rtthread.h>
typedef rt_thread_t hal_thread_t;
typedef struct rt_thread hal_thread;
#endif


void *kthread_create(void (*threadfn)(void *data), void *data, const char *namefmt, int stacksize, int priority);
int kthread_stop(void *thread);
int kthread_start(void *thread);
void *kthread_self(void);
int kthread_wakeup(void *thread);
int kthread_suspend(void *thread);
int kthread_msleep(int ms);
int kthread_sleep(int tick);
int kthread_scheduler_is_running(void);
int kthread_in_critical_context(void);
void kthread_tick_increase(void);

#define kthread_run(threadfn, data, namefmt, ...)			   \
({									   \
	void *__k						   \
		= kthread_create(threadfn, data, namefmt, HAL_THREAD_STACK_SIZE, HAL_THREAD_PRIORITY); \
	if (__k)				   \
		kthread_start(__k);					   \
	__k;								   \
})

#ifdef __cplusplus
}
#endif
#endif
