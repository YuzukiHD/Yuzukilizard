/*
 * ===========================================================================================
 *
 *       Filename:  pipe_test.c
 *
 *    Description:  pipe test case.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-11 09:26:14
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-06-15 14:17:22
 *
 * ===========================================================================================
 */
#include <rtthread.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <finsh.h>
#include <debug.h>
#include <pipe.h>
#include <log.h>

static rt_mutex_t lock_egg_buffer;
static rt_sem_t consumer, producer;
static int egg[10] = {0};
static int eggnum = 0;
static int index_wr = 0;
static int index_rd = 0;

static void put_egg(void)
{
    egg[index_wr] = eggnum ++;
    index_wr ++;
    index_wr %= 10;
    printf("%s line %d, put egg %d, in slot %d size %d.\n", __func__, __LINE__, egg[index_wr], index_wr, (index_wr-index_rd + 10) % 10);
}

static void get_egg(void)
{
    int ret =  egg[index_rd];
    index_rd ++;
    index_rd %= 10;
    printf("%s line %d, get egg %d, in slot %d size %d.\n", __func__, __LINE__, egg[index_rd], index_rd, (index_wr-index_rd + 10) % 10);
}

static void producer_entry(void *para)
{
    while (1)
    {
        rt_sem_take(producer, RT_WAITING_FOREVER);
        rt_mutex_take(lock_egg_buffer, RT_WAITING_FOREVER);
        put_egg();
        rt_mutex_release(lock_egg_buffer);
        rt_sem_release(consumer);
        //rt_thread_delay(100);
    }
}

static void consumer_entry(void *para)
{
    while (1)
    {
        rt_sem_take(consumer, RT_WAITING_FOREVER);
        rt_mutex_take(lock_egg_buffer, RT_WAITING_FOREVER);
        get_egg();
        rt_mutex_release(lock_egg_buffer);
        rt_sem_release(producer);
    }
}

static void test_entry(void)
{
    rt_thread_t producer_thread, consumer_thread;

    producer = rt_sem_create("producer", 10, RT_IPC_FLAG_FIFO);
    consumer = rt_sem_create("consumer", 0, RT_IPC_FLAG_FIFO);
    lock_egg_buffer = rt_mutex_create("dmntex", RT_IPC_FLAG_FIFO);

    if(!lock_egg_buffer || !producer || !consumer)
    {
        __err("fatal error, create sem failure.");
        software_break();
    }

    producer_thread = rt_thread_create("producer", producer_entry, RT_NULL, 0x1000, 1, 10);
    RT_ASSERT(producer_thread != RT_NULL);
    rt_thread_startup(producer_thread);

    consumer_thread = rt_thread_create("consumer", consumer_entry, RT_NULL, 0x1000, 1, 10);
    RT_ASSERT(consumer_thread != RT_NULL);
    rt_thread_startup(consumer_thread);

    return;
}

static int cmd_producer(int argc, const char **argv)
{
    test_entry();
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_producer, __cmd_producer, producer test);
