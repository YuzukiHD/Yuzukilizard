/*
 * =====================================================================================
 *
 *       Filename:  scheduler.c
 *
 *    Description:
 *
 *        Version:  2.0
 *         Create:  2017-12-02 11:09:07
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2018-08-15 13:45:00
 *
 * =====================================================================================
 */

#include "scheduler.h"
#include <zephyr.h>
#include <kernel_structs.h>
#include <arch/cpu.h>
#include <ksched.h>
#include <log.h>
#include <debug.h>

extern u32_t _tick_get_32(void);
static struct k_thread rr_thread_1;
static struct k_thread rr_thread_2;
static struct k_thread rr_thread_3;
static struct k_thread rr_thread_4;
static struct k_thread _thread_A;
static struct k_thread _thread_B;
static struct k_thread _thread_INT;

static k_tid_t const rr_thread_p1 = (k_tid_t) &rr_thread_1;
static k_tid_t const rr_thread_p2 = (k_tid_t) &rr_thread_2;
static k_tid_t const rr_thread_p3 = (k_tid_t) &rr_thread_3;
static k_tid_t const rr_thread_p4 = (k_tid_t) &rr_thread_4;
static k_tid_t const _thread_pA   = (k_tid_t) &_thread_A;
static k_tid_t const _thread_pB   = (k_tid_t) &_thread_B;
static k_tid_t const _thread_pINT   = (k_tid_t) &_thread_INT;
K_THREAD_STACK_DEFINE(rr_stack_1,  1024);
K_THREAD_STACK_DEFINE(rr_stack_2,  1024);
K_THREAD_STACK_DEFINE(rr_stack_3,  1024);
K_THREAD_STACK_DEFINE(rr_stack_4,  1024);
K_THREAD_STACK_DEFINE(_stack_A,  1024);
K_THREAD_STACK_DEFINE(_stack_B,  1024);
K_THREAD_STACK_DEFINE(_stack_INT,  1024);

struct k_sem threadA_sem;
struct k_sem threadB_sem;
struct k_sem log_sem;

#define RR_PRIORITY       10
#define PR_PRIORITY       9
#define SM_PRIORITY       8
#define HI_PRIORITY       6

static void rr_thread_entry_1(void *unused1, void *unused2, void *unused3)
{
    while (1)
    {
        k_sem_take(&log_sem, K_FOREVER);
        __log("pre");
        k_sem_give(&log_sem);
        k_sleep(1);
        k_sem_take(&log_sem, K_FOREVER);
        __log("post");
        k_sem_give(&log_sem);
    }
}

static void rr_thread_entry_2(void *unused1, void *unused2, void *unused3)
{
    while (1)
    {
        k_sem_take(&log_sem, K_FOREVER);
        __log("pre");
        k_sem_give(&log_sem);
        k_sleep(1);
        k_sem_take(&log_sem, K_FOREVER);
        __log("post");
        k_sem_give(&log_sem);
    }
}

static void rr_thread_entry_3(void *unused1, void *unused2, void *unused3)
{
    while (1)
    {
        k_sem_take(&log_sem, K_FOREVER);
        __log("pre-----------------------------------------");
        k_sem_give(&log_sem);
        k_sched_lock();
        k_sleep(1);
        k_sched_unlock();
        k_sem_take(&log_sem, K_FOREVER);
        __log("post----------------------------------------");
        k_sem_give(&log_sem);
    }
}

static void rr_thread_entry_4(void *unused1, void *unused2, void *unused3)
{
    uint32_t counter;

    while (1)
    {
        counter = 2000;
        while (counter --)
        {
            k_sem_take(&log_sem, K_FOREVER);
            __log("---------------------%lld------------------", k_uptime_get());
            k_sem_give(&log_sem);
            k_busy_wait(1);
        }
        k_sleep(5000);
    }
}

/*
 * @param my_name      thread identification string
 * @param my_sem       thread's own semaphore
 * @param other_sem    other thread's semaphore
 */
void helloLoop(const char *my_name,
               struct k_sem *my_sem, struct k_sem *other_sem)
{
    while (1)
    {
        /* take my semaphore */
        k_sem_take(my_sem, K_FOREVER);

        /* say "hello" */
        k_sem_take(&log_sem, K_FOREVER);
        printk("%s: Hello World from %s!\n", my_name, "arm");
        k_sem_give(&log_sem);

        /* wait a while, then let other thread have a turn */
        k_sleep(500);
        k_sem_give(other_sem);
    }
}

static void _thread_entry_A(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);

    /* invoke routine to ping-pong hello messages with threadA */
    helloLoop(__func__, &threadA_sem, &threadB_sem);
}

static void _thread_entry_B(void *unused1, void *unused2, void *unused3)
{
    ARG_UNUSED(unused1);
    ARG_UNUSED(unused2);
    ARG_UNUSED(unused3);

    /* invoke routine to ping-pong hello messages with threadA */
    helloLoop(__func__, &threadB_sem, &threadA_sem);
}

static void _thread_entry_INT(void *unused1, void *unused2, void *unused3)
{
    unsigned long long count = 0;
    unsigned long long i = 0;
    int tick;
    int tick2;
    int imask;

    while (1)
    {
        tick = _tick_get_32();
        while (_tick_get_32() == tick)
        {
        }

        tick ++;

        while (_tick_get_32() == tick)
        {
            count ++;
        }

        count <<= 4;

        imask =  irq_lock();
        tick = _tick_get_32();
        for (i = 0; i < count; i++)
        {
            _tick_get_32();
        }

        tick2 = _tick_get_32();
        irq_unlock(imask);

        if (tick2 != tick)
        {
            __log("failure.");
        }
        else
        {
            __log("success.");
        }

        for (i = 0; i < count; i++)
        {
            _tick_get_32();
        }

        tick2 = _tick_get_32();
        if (tick == tick2)
        {
            __log("failure");
        }
        else
        {
            __log("success");
        }

        k_sleep(2000);
    }
}

void test_main(void)
{

    void test_workqueue(void);
    void test_priority_scheduling(void);
    void test_mutex_lock_unlock(void);
    void test_mutex_reent_lock_forever(void);
    void test_mutex_reent_lock_no_wait(void);
    void test_mutex_reent_lock_timeout_fail(void);
    void test_mutex_reent_lock_timeout_pass(void);
    void test_priority_preemptible(void);
    void test_priority_cooperative(void);
    void test_yield_cooperative(void);
    void test_sleep_cooperative(void);
    void test_sleep_wakeup_preemptible(void);
    void test_time_slicing_preemptible(void);
    void test_time_slicing_disable_preemptible(void);
    void test_lock_preemptible(void);
    void test_unlock_preemptible(void);
    void test_sched_is_preempt_thread(void);
    while (1)
    {
        test_priority_scheduling();
        /*test_workqueue();*/
        test_mutex_lock_unlock();
        test_mutex_reent_lock_forever();
        test_mutex_reent_lock_no_wait();
        test_mutex_reent_lock_timeout_fail();
        test_mutex_reent_lock_timeout_pass();

        test_priority_preemptible();
        test_priority_cooperative();

        test_yield_cooperative();
        test_sleep_cooperative();
        test_sleep_wakeup_preemptible();
        test_time_slicing_preemptible();
        test_time_slicing_disable_preemptible();
        test_lock_preemptible();
        test_unlock_preemptible();
        test_sched_is_preempt_thread();
    }

#if 0
    k_sem_init(&threadA_sem, 1, 1); /* starts off "available" */
    k_sem_init(&threadB_sem, 0, 1); /* starts off "not available" */
    k_sem_init(&log_sem, 1, 1); /* starts off "available" */

    k_thread_create(rr_thread_p1, rr_stack_1,
                    1024, rr_thread_entry_1, NULL, NULL, NULL,
                    RR_PRIORITY, K_ESSENTIAL, 0);

    k_thread_create(rr_thread_p2, rr_stack_2,
                    1024, rr_thread_entry_2, NULL, NULL, NULL,
                    RR_PRIORITY, K_ESSENTIAL, 0);

    k_thread_create(rr_thread_p3, rr_stack_3,
                    1024, rr_thread_entry_3, NULL, NULL, NULL,
                    RR_PRIORITY, K_ESSENTIAL, 20000);

    k_thread_create(rr_thread_p4, rr_stack_4,
                    1024, rr_thread_entry_4, NULL, NULL, NULL,
                    PR_PRIORITY, K_ESSENTIAL, 2000);

    k_thread_create(_thread_pA, _stack_A,
                    1024, _thread_entry_A, NULL, NULL, NULL,
                    SM_PRIORITY, K_ESSENTIAL, 0);

    k_thread_create(_thread_pB, _stack_B,
                    1024, _thread_entry_B, NULL, NULL, NULL,
                    SM_PRIORITY, K_ESSENTIAL, 0);

    k_thread_create(_thread_pINT, _stack_INT,
                    1024, _thread_entry_INT, NULL, NULL, NULL,
                    HI_PRIORITY, K_ESSENTIAL, 0);
#endif
}

