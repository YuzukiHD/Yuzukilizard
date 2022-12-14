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
 *  Last Modified:  2020-03-09 11:06:26
 *
 * =====================================================================================
 */

#include <typedef.h>
#include <rtthread.h>
#include <preempt.h>
#include <completion.h>
#include <rthw.h>
#include <arch.h>
#include <log.h>

static struct rt_completion completion;
/* ----------------------------------------------------------------------------*/
/**
 * @brief  irq_lock_thread
 *
 * @param para, not used.
 */
/* ----------------------------------------------------------------------------*/
static void complete_wait_task(void *ARG_UNUSED(para))
{
    while (1)
    {
        __log("before wait.");
        rt_completion_wait(&completion, RT_WAITING_FOREVER);
        __log("after wait.");
    }
}

static void complete_signal_task(void *ARG_UNUSED(para))
{
    while (1)
    {
        __log("before signal.");
        rt_completion_done(&completion);
        __log("after signal.");

        rt_thread_delay(100);
    }
}

/* ----------------------------------------------------------------------------*/
/*
 *  @brief  schedule_in_irqlock <test pattern, Thread swith out when iterrupt
 *          disable>
 */
/* ----------------------------------------------------------------------------*/
void complete_test(void)
{
    rt_thread_t thread;

    rt_completion_init(&completion);

    thread = rt_thread_create("comp_1", complete_wait_task, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("comp_2", complete_signal_task, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);
}
