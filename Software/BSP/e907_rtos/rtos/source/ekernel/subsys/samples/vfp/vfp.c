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

void kthread_enable_fpu(void);
extern int kthread_get_fpustat(void);
static void vfp_task1(void *ARG_UNUSED(para))
{
    kthread_enable_fpu();
    while (1)
    {
        rt_thread_delay(100);
        __log("use_fpu = %d.", kthread_get_fpustat());
    }
}

static void vfp_task2(void *ARG_UNUSED(para))
{
    kthread_enable_fpu();
    while (1)
    {
        rt_thread_delay(100);
        __log("use_fpu = %d.", kthread_get_fpustat());
    }
}

static void vfp_task3(void *ARG_UNUSED(para))
{
    while (1)
    {
        rt_thread_delay(100);
        __log("use_fpu = %d.", kthread_get_fpustat());
    }
}

/* ----------------------------------------------------------------------------*/
/*
 *  @brief  schedule_in_irqlock <test pattern, Thread swith out when iterrupt
 *          disable>
 */
/* ----------------------------------------------------------------------------*/
void vfptest_entry(void)
{
    rt_thread_t thread;

    thread = rt_thread_create("vfp1", vfp_task1, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("vfp2", vfp_task1, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("vfp3", vfp_task2, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);

    thread = rt_thread_create("vfp4", vfp_task3, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);
}
