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
 *  Last Modified:  2020-06-03 14:04:07
 *
 * =====================================================================================
 */

#include <typedef.h>
#include <rtthread.h>
#include <rthw.h>
#include <arch.h>
#include <log.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <finsh.h>
#include <debug.h>

#define THREAD_STACK_SIZE 4096
#define THREAD_TIMESLICE  20
/*
 * 创建7个进程，tid20、tid21、tid22、tid23、tid24、tid25、tid26，
 * 各任务优先级分别为20、21、22、23、24、25、26、27、28、29
 * 其中tid22不需要使用到互斥锁，其他所有进程均需要使用互斥锁，
 * 通过各个进程的打印信息，观察各个进程获取到互斥锁后，优先级变化情况
 */
static rt_thread_t tid20 = RT_NULL;
static rt_thread_t tid21 = RT_NULL;
static rt_thread_t tid22 = RT_NULL;
static rt_thread_t tid23 = RT_NULL;
static rt_thread_t tid24 = RT_NULL;
static rt_thread_t tid25 = RT_NULL;
static rt_thread_t tid26 = RT_NULL;

static rt_mutex_t mutex = RT_NULL;

static void thread20_entry(void *parameter)
{
    rt_err_t result;
    rt_tick_t tick;

    rt_kprintf("[%d] thread20 run!\n", rt_tick_get());
    rt_thread_delay(25);
    rt_kprintf("[%d] thread20 wake!\n", rt_tick_get());

    result = rt_mutex_take(mutex, RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        __err("failure.");
        while (1);
    }

    tick = rt_tick_get();
    while (rt_tick_get() - tick < 20)
    {
        rt_kprintf("[%d] thread20, init_priority=%d, current_priority=%d\n",
                   rt_tick_get(), tid20->init_priority, tid20->current_priority);
    }
    rt_mutex_release(mutex);
    rt_kprintf("[%d] thread20 exit!\n", rt_tick_get());
}

static void thread21_entry(void *parameter)
{
    rt_err_t result;
    rt_tick_t tick;
    rt_kprintf("[%d] thread21 run!\n", rt_tick_get());
    rt_thread_delay(20);
    rt_kprintf("[%d] thread21 wake!\n", rt_tick_get());

    result = rt_mutex_take(mutex, RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        __err("failure.");
        while (1);
    }

    tick = rt_tick_get();
    while (rt_tick_get() - tick < 20)
    {
        rt_kprintf("[%d] thread21, init_priority=%d, current_priority=%d\n",
                   rt_tick_get(), tid21->init_priority, tid21->current_priority);
    }
    rt_mutex_release(mutex);
    rt_kprintf("[%d] thread21 exit!\n", rt_tick_get());

}

static void thread22_entry(void *parameter)
{
    rt_tick_t tick;

    rt_kprintf("[%d] thread22 run!\n", rt_tick_get());
    rt_thread_delay(70);
    rt_kprintf("[%d] thread22 wake!\n", rt_tick_get());

    tick = rt_tick_get();
    while (rt_tick_get() - tick < 200)
    {
        rt_kprintf("[%d] thread22 running..., init_priority=%d, current_priority=%d\n",
                   rt_tick_get(), tid22->init_priority, tid22->current_priority);
        rt_thread_delay(2);
    }
    rt_kprintf("[%d] thread22 exit!\n", rt_tick_get());
}

static void thread23_entry(void *parameter)
{
    rt_err_t result;
    rt_tick_t tick;
    rt_kprintf("[%d] thread23 run!\n", rt_tick_get());

    rt_thread_delay(15);
    rt_kprintf("[%d] thread23 wake!\n", rt_tick_get());

    result = rt_mutex_take(mutex, RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        __err("failure.");
        while (1);
    }

    tick = rt_tick_get();
    while (rt_tick_get() - tick < 20)
    {
        rt_kprintf("[%d] thread23, init_priority=%d, current_priority=%d\n",
                   rt_tick_get(), tid23->init_priority, tid23->current_priority);
    }
    rt_mutex_release(mutex);
    rt_kprintf("[%d] thread23 exit!\n", rt_tick_get());

}

static void thread24_entry(void *parameter)
{
    rt_err_t result;
    rt_tick_t tick;

    rt_kprintf("[%d] thread24 run!\n", rt_tick_get());
    rt_thread_delay(10);
    rt_kprintf("[%d] thread24 wake!\n", rt_tick_get());

    result = rt_mutex_take(mutex, RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        __err("failure.");
        while (1);
    }

    tick = rt_tick_get();
    while (rt_tick_get() - tick < 20)
    {
        rt_kprintf("[%d] thread24, init_priority=%d, current_priority=%d\n",
                   rt_tick_get(), tid24->init_priority, tid24->current_priority);
    }
    rt_mutex_release(mutex);
    rt_kprintf("[%d] thread24 exit!\n", rt_tick_get());
}

static void thread25_entry(void *parameter)
{
    rt_err_t result;
    rt_tick_t tick;
    rt_kprintf("[%d] thread25 run!\n", rt_tick_get());
    rt_thread_delay(5);
    rt_kprintf("[%d] thread25 wake!\n", rt_tick_get());

    result = rt_mutex_take(mutex, RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        __err("failure.");
        while (1);
    }

    tick = rt_tick_get();
    while (rt_tick_get() - tick < 20)
    {
        rt_kprintf("[%d] thread25, init_priority=%d, current_priority=%d\n",
                   rt_tick_get(), tid25->init_priority, tid25->current_priority);
    }
    rt_mutex_release(mutex);
    rt_kprintf("[%d] thread25 exit!\n", rt_tick_get());
}

static void thread26_entry(void *parameter)
{
    rt_tick_t tick;
    rt_err_t result;

    rt_kprintf("[%d] thread26 run!\n", rt_tick_get());

    result = rt_mutex_take(mutex, RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        __err("failure.");
        while (1);
    }

    tick = rt_tick_get();
    while (rt_tick_get() - tick < 90)
    {
        rt_kprintf("[%d] thread26, init_priority=%d, current_priority=%d\n",
                   rt_tick_get(), tid26->init_priority, tid26->current_priority);
    }
    rt_mutex_release(mutex);
    rt_kprintf("[%d] thread26 exit!\n", rt_tick_get());

}

static int mutex_simple_init(void)
{
    /* 创建互斥锁 */
    mutex = rt_mutex_create("mutex", RT_IPC_FLAG_FIFO);
    if (mutex == RT_NULL)
    {
        __err("failure.");
        return 0;
    }

    tid26 = rt_thread_create("t26",
                             thread26_entry, RT_NULL,
                             THREAD_STACK_SIZE, 26, THREAD_TIMESLICE);
    if (tid26 != RT_NULL)
    {
        rt_thread_startup(tid26);
    }
    else
    {
        __err("failure.");
        while (1);
    }

    tid25 = rt_thread_create("t25",
                             thread25_entry, RT_NULL,
                             THREAD_STACK_SIZE, 25, THREAD_TIMESLICE);
    if (tid25 != RT_NULL)
    {
        rt_thread_startup(tid25);
    }
    else
    {
        __err("failure.");
        while (1);
    }

    tid24 = rt_thread_create("t24",
                             thread24_entry, RT_NULL,
                             THREAD_STACK_SIZE, 24, THREAD_TIMESLICE);
    if (tid24 != RT_NULL)
    {
        rt_thread_startup(tid24);
    }
    else
    {
        __err("failure.");
        while (1);
    }

    tid23 = rt_thread_create("t23",
                             thread23_entry, RT_NULL,
                             THREAD_STACK_SIZE, 23, THREAD_TIMESLICE);
    if (tid23 != RT_NULL)
    {
        rt_thread_startup(tid23);
    }
    else
    {
        __err("failure.");
        while (1);
    }

    tid22 = rt_thread_create("t22",
                             thread22_entry, RT_NULL,
                             THREAD_STACK_SIZE, 22, THREAD_TIMESLICE);
    if (tid22 != RT_NULL)
    {
        rt_thread_startup(tid22);
    }
    else
    {
        __err("failure.");
        while (1);
    }

    tid21 = rt_thread_create("t21",
                             thread21_entry, RT_NULL,
                             THREAD_STACK_SIZE, 21, THREAD_TIMESLICE);
    if (tid21 != RT_NULL)
    {
        rt_thread_startup(tid21);
    }
    else
    {
        __err("failure.");
        while (1);
    }

    tid20 = rt_thread_create("t20",
                             thread20_entry, RT_NULL,
                             THREAD_STACK_SIZE, 20, THREAD_TIMESLICE);
    if (tid20 != RT_NULL)
    {
        rt_thread_startup(tid20);
    }
    else
    {
        __err("failure.");
        while (1);
    }

    return 0;
}

static void test_cleanup(void)
{
    /* 调度器上锁，上锁后，将不再切换到其他线程，仅响应中断 */
    rt_enter_critical();

    /* 删除线程 */
    if (tid20 != RT_NULL && tid20->stat != RT_THREAD_CLOSE)
    {
        rt_thread_delete(tid20);
    }
    if (tid21 != RT_NULL && tid21->stat != RT_THREAD_CLOSE)
    {
        rt_thread_delete(tid21);
    }
    if (tid22 != RT_NULL && tid22->stat != RT_THREAD_CLOSE)
    {
        rt_thread_delete(tid22);
    }
    if (tid23 != RT_NULL && tid23->stat != RT_THREAD_CLOSE)
    {
        rt_thread_delete(tid23);
    }
    if (tid24 != RT_NULL && tid24->stat != RT_THREAD_CLOSE)
    {
        rt_thread_delete(tid24);
    }
    if (tid25 != RT_NULL && tid25->stat != RT_THREAD_CLOSE)
    {
        rt_thread_delete(tid25);
    }
    if (tid26 != RT_NULL && tid26->stat != RT_THREAD_CLOSE)
    {
        rt_thread_delete(tid26);
    }

    if (mutex != RT_NULL)
    {
        rt_mutex_delete(mutex);
    }
    rt_kprintf("test_done!\n");

    /* 调度器解锁 */
    rt_exit_critical();
}

int cmd_sched_pri(int argc, char **argv)
{
    while (1)
    {
        mutex_simple_init();
        rt_thread_delay(1000);
    }
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_sched_pri, __cmd_sched_pri, group pririty inherient test);

