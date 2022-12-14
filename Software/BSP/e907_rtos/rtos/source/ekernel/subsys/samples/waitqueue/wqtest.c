/*
 * =====================================================================================
 *
 *       Filename:  wqtest.c
 *
 *    Description:  waitqueue test.
 *
 *        Version:  1.0
 *        Created:  2019年10月18日 14时08分13秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#include <typedef.h>
#include <rtthread.h>
#include <waitqueue.h>
#include <rthw.h>
#include <arch.h>
#include <log.h>

static rt_wqueue_t wqueue_test;

static void wqtest_task1(void *ARG_UNUSED(para))
{
    while (1)
    {
        __log("comm = %s, before enter waitlist.", rt_thread_self()->name);
        rt_wqueue_wait(&wqueue_test, 0, RT_WAITING_FOREVER);
        __log("comm = %s, resume from waitlist.", rt_thread_self()->name);
    }
}

static void wqtest_task2(void *ARG_UNUSED(para))
{
    while (1)
    {
        rt_thread_delay(1000);
        __log("comm = %s, wakeup waitlist.", rt_thread_self()->name);
        rt_wqueue_wakeup(&wqueue_test, NULL);
    }
}

/* ----------------------------------------------------------------------------*/
/*
 *  @brief  schedule_in_irqlock <test pattern, Thread swith out when iterrupt
 *          disable>
 */
/* ----------------------------------------------------------------------------*/
void wqtest_entry(void)
{
    rt_thread_t thread;

    rt_wqueue_init(&wqueue_test);

    thread = rt_thread_create("wq1", wqtest_task1, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("wq2", wqtest_task1, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("wq3", wqtest_task2, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);
}
