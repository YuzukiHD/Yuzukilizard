/*
 * =====================================================================================
 *
 *       Filename:  sched_priority.c
 *
 *    Description:  test pattern
 *
 *        Version:  2.0
 *         Create:  2017-12-01 16:53:37
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2017-12-01 17:01:17
 *
 * =====================================================================================
 */

#include "sched_priority.h"
#include "test.h"
#include <zephyr.h>
#include <kernel_structs.h>
#include <arch/cpu.h>
#include <ksched.h>
#include <log.h>
#include <debug.h>

static K_THREAD_STACK_DEFINE(tstack, STACK_SIZE);
static struct k_thread tdata;
static int last_prio;

static void thread_entry(void *p1, void *p2, void *p3)
{
    last_prio = k_thread_priority_get(k_current_get());
}

/*test cases*/
void test_priority_cooperative(void)
{
    int old_prio = k_thread_priority_get(k_current_get());

    /* set current thread to a negative priority */
    last_prio = -1;
    k_thread_priority_set(k_current_get(), last_prio);

    /* spawn thread with higher priority */
    int spawn_prio = last_prio - 1;

    k_tid_t tid = k_thread_create(&tdata, tstack, STACK_SIZE,
                                  thread_entry, NULL, NULL, NULL,
                                  spawn_prio, 0, 0);
    /* checkpoint: current thread shouldn't preempted by higher thread */
    zassert_true(last_prio == k_thread_priority_get(k_current_get()), NULL);
    k_sleep(100);
    /* checkpoint: spawned thread get executed */
    zassert_true(last_prio == spawn_prio, NULL);
    k_thread_abort(tid);

    /* restore environment */
    k_thread_priority_set(k_current_get(), old_prio);
}

void test_priority_preemptible(void)
{
    int old_prio = k_thread_priority_get(k_current_get());

    /* set current thread to a non-negative priority */
    last_prio = 2;
    k_thread_priority_set(k_current_get(), last_prio);

    int spawn_prio = last_prio - 1;

    k_tid_t tid = k_thread_create(&tdata, tstack, STACK_SIZE,
                                  thread_entry, NULL, NULL, NULL,
                                  spawn_prio, 0, 0);
    /* checkpoint: thread is preempted by higher thread */
    zassert_true(last_prio == spawn_prio, NULL);

    k_sleep(100);
    k_thread_abort(tid);

    spawn_prio = last_prio + 1;
    tid = k_thread_create(&tdata, tstack, STACK_SIZE,
                          thread_entry, NULL, NULL, NULL,
                          spawn_prio, 0, 0);
    /* checkpoint: thread is not preempted by lower thread */
    zassert_false(last_prio == spawn_prio, NULL);
    k_thread_abort(tid);

    /* restore environment */
    k_thread_priority_set(k_current_get(), old_prio);
}

