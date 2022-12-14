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

rt_thread_t switch1, switch2;
static void switch_task1(void *ARG_UNUSED(para))
{
    while (1)
    {
        rt_thread_delay(10) ;
        rt_thread_resume(switch2);
    }
}

static void switch_task2(void *ARG_UNUSED(para))
{
    while (1)
    {
        rt_thread_suspend(rt_thread_self());
    }
}

/* ----------------------------------------------------------------------------*/
/*
 *  @brief  schedule_in_irqlock <test pattern, Thread swith out when iterrupt
 *          disable>
 */
/* ----------------------------------------------------------------------------*/
void switchtest_entry(void)
{
    switch1 = rt_thread_create("switch1", switch_task1, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(switch1);

    switch2 = rt_thread_create("switch2", switch_task2, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(switch2);
}
