#ifndef HAL_POLL_H
#define HAL_POLL_H

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(CONFIG_OS_MELIS)
#include <dfs_poll.h>
#elif defined(CONFIG_KERNEL_FREERTOS)
struct hal_pollreq;
#else
#error "can not support unknown platform"
#endif

#ifdef __cplusplus
}
#endif

#endif  /*HAL_POLL_H*/
