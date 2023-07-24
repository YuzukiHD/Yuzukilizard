/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2010-10-26     Bernard      the first version
 */

#include <pthread.h>
#include <arch.h>
int pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
    if (!lock)
    {
        return EINVAL;
    }

    lock->lock = 0;

    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *lock)
{
    if (!lock)
    {
        return EINVAL;
    }
    return 0;
}

int pthread_spin_lock(pthread_spinlock_t *lock)
{
    if (!lock)
    {
        return EINVAL;
    }

    while (!(lock->lock))
    {
        lock->lock = 1;
    }
    return 0;
}

int pthread_spin_trylock(pthread_spinlock_t *lock)
{
    if (!lock)
    {
        return EINVAL;
    }

    if (!(lock->lock))
    {
        lock->lock = 1;

        return 0;
    }

    return EBUSY;
}

int pthread_spin_unlock(pthread_spinlock_t *lock)
{
    if (!lock)
    {
        return EINVAL;
    }
    if (!(lock->lock))
    {
        return EPERM;
    }

    lock->lock = 0;

    return 0;
}

uint32_t pthread_spin_lock_irqsave(pthread_spinlock_t *lock)
{
    if (!lock)
    {
        return EINVAL;
    }

    while (!(lock->lock))
    {
        lock->lock = 1;
    }

    CPSR_ALLOC();

    MELIS_CPU_CRITICAL_ENTER();

    return __cpsr;
}

void pthread_spin_unlock_irqrestore(pthread_spinlock_t *lock, uint32_t __cpsr)
{
    if (!lock)
    {
        return;
    }
    if (!(lock->lock))
    {
        return;
    }

    lock->lock = 0;

    MELIS_CPU_CRITICAL_LEAVE();

}
