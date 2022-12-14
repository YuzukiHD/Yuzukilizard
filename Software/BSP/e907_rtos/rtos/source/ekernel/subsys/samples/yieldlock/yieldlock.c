/*
 * =====================================================================================
 *
 *       Filename:  sched_irq.c
 *
 *    Description:  schedule during irq lock.
 *
 *        Version:  2.0
 *         Create:  2017-11-09 15:37:55
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-03-02 16:25:32
 *
 * =====================================================================================
 */

#include <typedef.h>
#include <rtthread.h>
#include <preempt.h>
#include <rthw.h>
#include <arch.h>
#include <log.h>

/* ----------------------------------------------------------------------------*/
/**
 * @brief  irq_lock_thread
 *
 * @param para, not used.
 */
/* ----------------------------------------------------------------------------*/
static void lock_thread_1(void *ARG_UNUSED(para))
{
    //each task has its own cpsr context, so irq & fiq environt not shared.
    while (1)
    {
        rt_enter_critical();
        __log("comm = %s, level %d.", rt_thread_self()->name, preempt_level());
        rt_enter_critical();
        __log("comm = %s, level %d.", rt_thread_self()->name, preempt_level());
        rt_thread_delay(100);
        __log("comm = %s, level %d.", rt_thread_self()->name, preempt_level());
        rt_exit_critical();
        __log("comm = %s, level %d.", rt_thread_self()->name, preempt_level());
        rt_exit_critical();
        __log("comm = %s, level %d.", rt_thread_self()->name, preempt_level());
    }
}

static void lock_thread_2(void *ARG_UNUSED(para))
{
    while (1)
    {
        rt_enter_critical();
        __log("comm = %s, level %d.", rt_thread_self()->name, preempt_level());
        rt_thread_delay(100);
        __log("comm = %s, level %d.", rt_thread_self()->name, preempt_level());
        rt_exit_critical();
        __log("comm = %s, level %d.", rt_thread_self()->name, preempt_level());
    }

}

/* ----------------------------------------------------------------------------*/
/*
 *  @brief  schedule_in_irqlock <test pattern, Thread swith out when iterrupt
 *          disable>
 */
/* ----------------------------------------------------------------------------*/
void lock_yield(void)
{
    rt_thread_t thread;
    thread = rt_thread_create("lock_1", lock_thread_1, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("lock_2", lock_thread_2, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);
}
