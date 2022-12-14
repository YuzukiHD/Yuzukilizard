/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-02-24     Bernard      first version
 * 2006-05-03     Bernard      add IRQ_DEBUG
 * 2016-08-09     ArdaFu       add interrupt enter and leave hook.
 */

#include <rthw.h>
#include <rtthread.h>
#include <preempt.h>

#ifdef RT_USING_HOOK

static void (*rt_interrupt_enter_hook)(void);
static void (*rt_interrupt_leave_hook)(void);

/**
 * @ingroup Hook
 * This function set a hook function when the system enter a interrupt
 *
 * @note the hook function must be simple and never be blocked or suspend.
 */
void rt_interrupt_enter_sethook(void (*hook)(void))
{
    rt_interrupt_enter_hook = hook;
}
/**
 * @ingroup Hook
 * This function set a hook function when the system exit a interrupt.
 *
 * @note the hook function must be simple and never be blocked or suspend.
 */
void rt_interrupt_leave_sethook(void (*hook)(void))
{
    rt_interrupt_leave_hook = hook;
}
#endif

/* #define IRQ_DEBUG */

/**
 * @addtogroup Kernel
 */

/**@{*/

/**
 * This function will be invoked by BSP, when enter interrupt service routine
 *
 * @note please don't invoke this routine in application
 *
 * @see rt_interrupt_leave
 */
void rt_interrupt_enter(void)
{
    rt_base_t level;

    RT_DEBUG_LOG(RT_DEBUG_IRQ, ("irq coming..., irq nest:%d\n", hardirq_count()));

    /* disable hardware interrupt */
    level = rt_hw_interrupt_disable();
    preempt_count_add(HARDIRQ_OFFSET);
    RT_OBJECT_HOOK_CALL(rt_interrupt_enter_hook, ());
    rt_hw_interrupt_enable(level);
}
RTM_EXPORT(rt_interrupt_enter);

/**
 * This function will be invoked by BSP, when leave interrupt service routine
 *
 * @note please don't invoke this routine in application
 *
 * @see rt_interrupt_enter
 */
void rt_interrupt_leave(void)
{
    rt_base_t level;

    RT_DEBUG_LOG(RT_DEBUG_IRQ, ("irq leave, irq nest:%d\n", hardirq_count()));

    /* disable hardware interrupt */
    level = rt_hw_interrupt_disable();
    preempt_count_sub(HARDIRQ_OFFSET);
    RT_OBJECT_HOOK_CALL(rt_interrupt_leave_hook, ());
    rt_hw_interrupt_enable(level);
}
RTM_EXPORT(rt_interrupt_leave);

/**
 * This function will return the nest of interrupt.
 *
 * User application can invoke this function to get whether current
 * context is interrupt context.
 *
 * @return the number of nested interrupts.
 */
RT_WEAK rt_uint8_t rt_interrupt_get_nest(void)
{
    rt_uint8_t ret;
    rt_base_t level;

    if (rt_thread_self() == RT_NULL)
    {
        return 0;
    }

    /* disable hardware interrupt */
    level = rt_hw_interrupt_disable();
    ret = hardirq_count();
    rt_hw_interrupt_enable(level);
    return ret;
}
RTM_EXPORT(rt_interrupt_get_nest);

/**
 * This function will lock the thread scheduler.
 */
void rt_enter_critical(void)
{
    register rt_base_t level;

    /* disable hardware interrupt */
    level = rt_hw_interrupt_disable();
    if (rt_thread_self() == RT_NULL)
    {
        rt_hw_interrupt_enable(level);
        return;
    }

    /*
     * the maximal number of nest is RT_UINT16_MAX, which is big
     * enough and does not check here
     */
    preempt_disable();

    /* enable interrupt */
    rt_hw_interrupt_enable(level);
}
RTM_EXPORT(rt_enter_critical);

/**
 * This function will unlock the thread scheduler.
 */
void rt_exit_critical(void)
{
    register rt_base_t level;

    /* disable hardware interrupt */
    level = rt_hw_interrupt_disable();
    if (rt_thread_self() == RT_NULL)
    {
        rt_hw_interrupt_enable(level);
        return;
    }

    preempt_enable();
    if (preempt_level() <= 0)
    {
        RT_ASSERT(preempt_level() == 0);

        /* enable interrupt */
        rt_hw_interrupt_enable(level);

        if (rt_thread_self())
        {
            /* if scheduler is started, do a schedule */
            rt_schedule();
        }
    }
    else
    {
        /* enable interrupt */
        rt_hw_interrupt_enable(level);
    }
}
RTM_EXPORT(rt_exit_critical);

/**
 * Get the scheduler lock level
 *
 * @return the level of the scheduler lock. 0 means unlocked.
 */
rt_uint16_t rt_critical_level(void)
{
    return preempt_level();
}
RTM_EXPORT(rt_critical_level);
/**@}*/

/**@}*/

