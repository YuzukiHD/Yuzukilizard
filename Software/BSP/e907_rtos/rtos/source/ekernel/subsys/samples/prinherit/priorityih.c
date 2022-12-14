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

void udelay(unsigned int us);
static rt_mutex_t mutex_s1, mutex_s2;
static void pri_task_a(void *ARG_UNUSED(para))
{
    while (1)
    {
        rt_thread_delay(2000);
        __log("before got s1");
        rt_mutex_take(mutex_s1, RT_WAITING_FOREVER);
        __log("got s1");
        udelay(100 * 1000 * 1000);
        __log("after delay 100s");
        rt_mutex_release(mutex_s1);
        __log("release s1");
    }
}

static void pri_task_b(void *ARG_UNUSED(para))
{
    rt_thread_delay(5000);

    while (1)
    {
        udelay(1);
    }
}

static void pri_task_c(void *ARG_UNUSED(para))
{
    while (1)
    {
        rt_thread_delay(1000);
        __log("before got s1");
        rt_mutex_take(mutex_s1, RT_WAITING_FOREVER);
        __log("got s1");
        rt_mutex_take(mutex_s2, RT_WAITING_FOREVER);
        __log("got s2");
        udelay(100 * 1000 * 1000);
        __log("delay 100s");
        rt_mutex_release(mutex_s2);
        __log("release s2");
        rt_mutex_release(mutex_s1);
        __log("release s1");
    }
}

static void pri_task_d(void *ARG_UNUSED(para))
{
    while (1)
    {
        __log("before got s2");
        rt_mutex_take(mutex_s2, RT_WAITING_FOREVER);
        __log("got s2");
        udelay(100 * 1000 * 1000);
        __log("after delay 100s.");
        rt_mutex_release(mutex_s2);
        __log("release s2.");
    }
}

/* ----------------------------------------------------------------------------*/
/*
 *  @brief  schedule_in_irqlock <test pattern, Thread swith out when iterrupt
 *          disable>
 */
/* ----------------------------------------------------------------------------*/
void priority_entry(void)
{
    rt_thread_t thread;

    mutex_s1 = rt_mutex_create("mutexs1", RT_IPC_FLAG_PRIO /*RT_IPC_FLAG_FIFO*/);
    mutex_s2 = rt_mutex_create("mutexs2", RT_IPC_FLAG_PRIO /*RT_IPC_FLAG_FIFO*/);
    if (mutex_s1 == RT_NULL || mutex_s2 == RT_NULL)
    {
        __err("fatal error");
        return;
    }

    thread = rt_thread_create("pri_a", pri_task_a, RT_NULL, 0x1000, 0, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("pri_b", pri_task_b, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("pri_c", pri_task_c, RT_NULL, 0x1000, 2, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("pri_d", pri_task_d, RT_NULL, 0x1000, 3, 10);
    rt_thread_startup(thread);

    return;
}
