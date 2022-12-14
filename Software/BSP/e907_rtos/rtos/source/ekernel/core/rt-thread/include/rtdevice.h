/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-01-08     bernard      first version.
 * 2014-07-12     bernard      Add workqueue implementation.
 */

#ifndef __RT_DEVICE_H__
#define __RT_DEVICE_H__

#include <rtthread.h>

/* #include "ipc/ringbuffer.h" */
/* #include "ipc/completion.h" */
/* #include "ipc/dataqueue.h"　*/
/* #include "ipc/workqueue.h"  */
#include "waitqueue.h"
/* #include "ipc/pipe.h"       */
#include "ipc/poll.h"
/* #include "ipc/ringblk_buf.h"*/

#ifdef __cplusplus
extern "C" {
#endif

#define RT_DEVICE(device)            ((rt_device_t)device)

#ifdef __cplusplus
}
#endif

#endif /* __RT_DEVICE_H__ */
