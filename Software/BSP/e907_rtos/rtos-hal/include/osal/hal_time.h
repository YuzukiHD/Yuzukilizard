#ifndef SUNXI_HAL_TIME_H
#define SUNXI_HAL_TIME_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Parameters used to convert the timespec values: */
#define MSEC_PER_SEC    1000L
#define USEC_PER_MSEC   1000L
#define NSEC_PER_USEC   1000L
#define NSEC_PER_MSEC   1000000L
#define USEC_PER_SEC    1000000L
#define NSEC_PER_SEC    1000000000L
#define FSEC_PER_SEC    1000000000000000LL

#ifdef CONFIG_KERNEL_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include <portmacro.h>

#undef HAL_WAIT_FOREVER
#define HAL_WAIT_FOREVER portMAX_DELAY
#define HAL_WAIT_NO      (0)

#define OSTICK_TO_MS(x) (x * (MSEC_PER_SEC / CONFIG_HZ))
#define MS_TO_OSTICK(x) (x / (MSEC_PER_SEC / CONFIG_HZ))

#define hal_tick_get()  xTaskGetTickCount()

typedef TickType_t hal_tick_t;

#elif defined(CONFIG_RTTKERNEL)

#include <rtthread.h>

#undef HAL_WAIT_FOREVER
#define HAL_WAIT_FOREVER RT_WAITING_FOREVER
#define HAL_WAIT_NO      RT_WAITING_NO

#define OSTICK_TO_MS(x) (x * (MSEC_PER_SEC / CONFIG_HZ))
#define MS_TO_OSTICK(x) (x / (MSEC_PER_SEC / CONFIG_HZ))

#define hal_tick_get()  rt_tick_get()
typedef rt_tick_t hal_tick_t;

#else
#error "can not support the RTOS!!"
#endif

int hal_sleep(unsigned int secs);
int hal_usleep(unsigned int usecs);
int hal_msleep(unsigned int msecs);
void hal_udelay(unsigned int us);
void hal_mdelay(unsigned int ms);
void hal_sdelay(unsigned int s);

uint64_t hal_gettime_ns(void);

#ifdef __cplusplus
}
#endif

#endif
