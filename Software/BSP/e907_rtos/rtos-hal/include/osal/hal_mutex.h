#ifndef SUNXI_HAL_MUTEX_H
#define SUNXI_HAL_MUTEX_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef CONFIG_KERNEL_FREERTOS
#include <FreeRTOS.h>
#include <semphr.h>
typedef struct QueueDefinition hal_mutex;
typedef SemaphoreHandle_t hal_mutex_t;
#elif defined(CONFIG_RTTKERNEL)
#include <rtthread.h>
typedef struct rt_mutex hal_mutex;
typedef rt_mutex_t hal_mutex_t;
#else
#error "can not support the RTOS!!"
#endif

#include <stdint.h>
#include <stddef.h>

int hal_mutex_init(hal_mutex *mutex);
int hal_mutex_detach(hal_mutex *mutex);
hal_mutex_t hal_mutex_create(void);
int hal_mutex_delete(hal_mutex_t mutex);
int hal_mutex_lock(hal_mutex_t  mutex);
int hal_mutex_unlock(hal_mutex_t  mutex);
int hal_mutex_trylock(hal_mutex_t  mutex);
int hal_mutex_timedwait(hal_mutex_t mutex, int ticks);

#ifdef __cplusplus
}
#endif

#endif
