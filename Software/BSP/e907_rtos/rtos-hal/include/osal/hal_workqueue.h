#ifndef SUNXI_HAL_WAITQUEUE_H
#define SUNXI_HAL_WAITQUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_KERNEL_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include <aw_list.h>

#include <hal_sem.h>
#include <hal_time.h>
#include <hal_timer.h>
#include <hal_status.h>

enum
{
    HAL_WORK_STATE_PENDING    = 0x0001,     /* Work item pending state */
    HAL_WORK_STATE_SUBMITTING = 0x0002,     /* Work item submitting state */
};

enum
{
    HAL_WORK_TYPE_DELAYED     = 0x0001,
};

typedef struct _hal_work hal_work;

typedef struct _hal_workqueue
{
    struct list_head work_list;
    hal_work *work_current;

    hal_sem_t sem;
    void *work_thread;
} hal_workqueue;

typedef struct _hal_work
{
    struct list_head list;

    void (*work_func)(hal_work *work, void *work_data);
    void *work_data;
    uint16_t flags;
    uint16_t type;
    osal_timer_t timer;
    hal_workqueue *workqueue;
} hal_work;

typedef struct _hal_delayed_work
{
    hal_work work;
} hal_delayed_work;

hal_workqueue *hal_workqueue_create(const char *name, uint16_t stack_size, uint8_t priority);
int hal_workqueue_destroy(hal_workqueue *queue);
int hal_workqueue_dowork(hal_workqueue *queue, hal_work *work);
int hal_workqueue_submit_work(hal_workqueue *queue, hal_work *work, hal_tick_t time);
int hal_workqueue_cancel_work(hal_workqueue *queue, hal_work *work);
int hal_workqueue_cancel_work_sync(hal_workqueue *queue, hal_work *work);

int hal_work_submit(hal_work *work, hal_tick_t time);
int hal_work_cancel(hal_work *work);

void hal_work_init(hal_work *work, void (*work_func)(hal_work *work, void *work_data), void *work_data);

void hal_delayed_work_init(hal_delayed_work *work, void (*work_func)(hal_work *work, void *work_data), void *work_data);

int hal_work_sys_workqueue_init(void);

#elif defined(CONFIG_RTTKERNEL)

#include <rtthread.h>
#include <workqueue.h>
#include <hal_time.h>

typedef struct rt_workqueue hal_workqueue;
typedef struct rt_work hal_work;

#define hal_work_init rt_work_init
hal_workqueue *hal_workqueue_create(const char *name, unsigned short stack_size, unsigned char priority);
int hal_workqueue_destroy(hal_workqueue *queue);
int hal_workqueue_dowork(hal_workqueue *queue, hal_work *work);
int hal_workqueue_submit_work(hal_workqueue *queue, hal_work *work, hal_tick_t time);
int hal_workqueue_cancel_work(hal_workqueue *queue, hal_work *work);
int hal_workqueue_cancel_work_sync(hal_workqueue *queue, hal_work *work);

#else
#error "can not support the RTOS!!"
#endif

#endif

