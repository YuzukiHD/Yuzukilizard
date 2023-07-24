/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2017/11/30     Bernard      The first version.
 */

#include <stdint.h>
#include <stdio.h>

#include <rtthread.h>
#include <dfs_posix.h>

#include "mman.h"

void *mmap(void *addr, size_t length, int prot, int flags,
    int fd, off_t offset)
{
    int result;
    struct dfs_fd *d;
    struct map_struct mmp;

    /* get the fd */
    d = fd_get(fd);
    if (d == NULL) {
        rt_set_errno(-EBADF);
        return MAP_FAILED;
    }

    if (d->fops->mmap == NULL) { 
	    rt_set_errno(-ENOSYS);
	    goto out;
    }

    /*
     * Here, if we support to map address dynmaic, we need to check
     * the addr is be mapped or not. But if the addr is NULL, we just
     * smpile get the address from filesystem.
     *
     */
    mmp.start = (unsigned long)addr;
    mmp.end   = (unsigned long)addr + length;
    mmp.flags = flags;
    mmp.prot  = prot;
#define PAGE_SHIFT  12
    mmp.pgoff = offset >> PAGE_SHIFT;

    result = d->fops->mmap(d, &mmp);
    if (result < 0) {
        rt_set_errno(result);
        goto out;
    }

    fd_put(d);
    return (void *)mmp.start;

out:
    /* release the ref-count of fd */
    fd_put(d);

    return MAP_FAILED;
}

int munmap(void *addr, size_t length)
{
    return 0;
}
