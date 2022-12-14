#ifndef SUNXI_HAL_OSAL_H
#define SUNXI_HAL_OSAL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#include <hal_status.h>
#include <hal_atomic.h>
#include <hal_cache.h>
#include <hal_interrupt.h>
#include <hal_mem.h>
#include <hal_mutex.h>
#include <hal_queue.h>
#include <hal_sem.h>
#include <hal_thread.h>
#include <hal_time.h>
#include <hal_timer.h>
#include <hal_log.h>
#include <hal_cmd.h>
#include <hal_debug.h>
#include <hal_time.h>
#include <hal_timer.h>

#define likely(x)             __builtin_expect((long)!!(x), 1L)
#define unlikely(x)           __builtin_expect((long)!!(x), 0L)

#ifdef CONFIG_KERNEL_FREERTOS
#include <aw_types.h>
#endif

#ifdef __cplusplus
}
#endif

#endif
