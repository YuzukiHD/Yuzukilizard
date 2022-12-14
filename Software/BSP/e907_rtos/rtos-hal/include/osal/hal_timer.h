#ifndef SUNXI_HAL_TIMER_H
#define SUNXI_HAL_TIMER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>
#include <hal_status.h>
#include <hal_time.h>

#ifdef CONFIG_KERNEL_FREERTOS

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

typedef TimerHandle_t osal_timer_t;
typedef void (*timeout_func)(void *parameter);

#define OSAL_TIMER_FLAG_PERIODIC     (1 << 0)
#define OSAL_TIMER_FLAG_ONE_SHOT     (1 << 1)
#define OSAL_TIMER_FLAG_HARD_TIMER   (1 << 2)
#define OSAL_TIMER_FLAG_SOFT_TIMER   (1 << 3)

#define OSAL_TIMER_CTRL_SET_TIME     (1 << 1)

#elif defined(CONFIG_RTTKERNEL)

#include <rtthread.h>

typedef rt_timer_t osal_timer_t;
typedef void (*timeout_func)(void *parameter);

#define OSAL_TIMER_FLAG_DEACTIVATED  RT_TIMER_FLAG_DEACTIVATED
#define OSAL_TIMER_FLAG_ACTIVATED    RT_TIMER_FLAG_ACTIVATED
#define OSAL_TIMER_FLAG_ONE_SHOT     RT_TIMER_FLAG_ONE_SHOT
#define OSAL_TIMER_FLAG_PERIODIC     RT_TIMER_FLAG_PERIODIC

#define OSAL_TIMER_FLAG_HARD_TIMER   RT_TIMER_FLAG_HARD_TIMER
#define OSAL_TIMER_FLAG_SOFT_TIMER   RT_TIMER_FLAG_SOFT_TIMER

#define OSAL_TIMER_CTRL_SET_TIME     RT_TIMER_CTRL_SET_TIME
#define OSAL_TIMER_CTRL_GET_TIME     RT_TIMER_CTRL_GET_TIME
#define OSAL_TIMER_CTRL_SET_ONESHOT  RT_TIMER_CTRL_SET_ONESHOT
#define OSAL_TIMER_CTRL_SET_PERIODIC RT_TIMER_CTRL_SET_PERIODIC
#define OSAL_TIMER_CTRL_GET_STATE    RT_TIMER_CTRL_GET_STATE

#else
#error "can not support unknown platform"
#endif

osal_timer_t osal_timer_create(const char *name,
                               timeout_func timeout,
                               void *parameter,
                               unsigned int time,
                               unsigned char flag);

hal_status_t osal_timer_delete(osal_timer_t timer);
hal_status_t osal_timer_start(osal_timer_t timer);
hal_status_t osal_timer_stop(osal_timer_t timer);
hal_status_t osal_timer_control(osal_timer_t timer, int cmd, void *arg);

int hal_sleep(unsigned int secs);
int hal_usleep(unsigned int usecs);
int hal_msleep(unsigned int msecs);
void hal_udelay(unsigned int us);
uint64_t hal_gettime_ns(void);
uint64_t hal_gettime_us(void);

#ifdef __cplusplus
}
#endif
#endif
