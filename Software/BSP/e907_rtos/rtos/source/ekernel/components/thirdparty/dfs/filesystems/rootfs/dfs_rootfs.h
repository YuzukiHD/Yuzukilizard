/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-04-15     Bernard      the first version
 * 2013-05-05     Bernard      remove CRC for rootfs persistence
 */

#ifndef __DFS_RAMFS_H__
#define __DFS_RAMFS_H__

#include <rtthread.h>
#include <rtservice.h>

int dfs_rootfs_init(void);

#endif

