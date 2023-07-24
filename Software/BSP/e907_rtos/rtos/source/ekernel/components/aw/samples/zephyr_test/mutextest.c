/*
 * =====================================================================================
 *
 *       Filename:  /home/caozilong/WorkSpace/melis-iot/melis-v2.0/melis-v2.0/source/ekernel/components/samples/zephyr_test/mutextest.c
 *
 *    Description:  mutex test
 *
 *        Version:  2.0
 *         Create:  2017-11-27 19:57:41
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2017-11-27 20:36:26
 *
 * =====================================================================================
 */

#include "mutextest.h"
#include <zephyr.h>
#include <kernel_structs.h>
#include <arch/arm/cortex_m/exc.h>
#include <arch/cpu.h>
#include <ksched.h>
#include <log.h>
#include <debug.h>

#define TIMEOUT 500
#define STACK_SIZE 512

/**TESTPOINT: init via K_MUTEX_DEFINE*/
K_MUTEX_DEFINE(kmutex);
static struct k_mutex mutex;

static K_THREAD_STACK_DEFINE(tstack, STACK_SIZE);
static struct k_thread tdata;

static void tThread_entry_lock_forever(void *p1, void *p2, void *p3)
{
    if ((k_mutex_lock((struct k_mutex *)p1, K_FOREVER) == 0) == 1)
    {
        software_break();
    }
    /* should not hit here */
}

static void tThread_entry_lock_no_wait(void *p1, void *p2, void *p3)
{
    if ((k_mutex_lock((struct k_mutex *)p1, K_NO_WAIT) != 0) == 0)
    {
        software_break();
    }
    __log("bypass locked resource from spawn thread");
}

static void tThread_entry_lock_timeout_fail(void *p1, void *p2, void *p3)
{
    if ((k_mutex_lock((struct k_mutex *)p1, TIMEOUT - 100) != 0) == 0)
    {
        software_break();
    }
    __log("bypass locked resource from spawn thread");
}

static void tThread_entry_lock_timeout_pass(void *p1, void *p2, void *p3)
{
    if ((k_mutex_lock((struct k_mutex *)p1, TIMEOUT + 100) == 0) == 0)
    {
        software_break();
    }
    __log("access resource from spawn thread");
    k_mutex_unlock((struct k_mutex *)p1);
}

static void tmutex_test_lock(struct k_mutex *pmutex,
                             void (*entry_fn)(void *, void *, void *))
{
    k_mutex_init(pmutex);
    k_tid_t tid = k_thread_create(&tdata, tstack, STACK_SIZE,
                                  entry_fn, pmutex, NULL, NULL,
                                  K_PRIO_PREEMPT(0), 0, 0);
    k_mutex_lock(pmutex, K_FOREVER);
    __log("access resource from main thread");

    /* wait for spawn thread to take action */
    k_sleep(TIMEOUT);

    /* teardown */
    k_thread_abort(tid);
}

static void tmutex_test_lock_timeout(struct k_mutex *pmutex,
                                     void (*entry_fn)(void *, void *, void *))
{
    /**TESTPOINT: test k_mutex_init mutex*/
    k_mutex_init(pmutex);
    k_tid_t tid = k_thread_create(&tdata, tstack, STACK_SIZE,
                                  entry_fn, pmutex, NULL, NULL,
                                  K_PRIO_PREEMPT(0), 0, 0);
    k_mutex_lock(pmutex, K_FOREVER);
    __log("access resource from main thread");

    /* wait for spawn thread to take action */
    k_sleep(TIMEOUT);
    k_mutex_unlock(pmutex);
    k_sleep(TIMEOUT);

    /* teardown */
    k_thread_abort(tid);
}

static void tmutex_test_lock_unlock(struct k_mutex *pmutex)
{
    k_mutex_init(pmutex);
    if ((k_mutex_lock(pmutex, K_FOREVER) == 0) == 0)
    {
        software_break();
    }
    k_mutex_unlock(pmutex);
    if ((k_mutex_lock(pmutex, K_NO_WAIT) == 0) == 0)
    {
        software_break();
    }
    k_mutex_unlock(pmutex);
    if ((k_mutex_lock(pmutex, TIMEOUT) == 0) == 0)
    {
        software_break();
    }
    k_mutex_unlock(pmutex);
}

/*test cases*/
void test_mutex_reent_lock_forever(void)
{
    /**TESTPOINT: test k_mutex_init mutex*/
    k_mutex_init(&mutex);
    tmutex_test_lock(&mutex, tThread_entry_lock_forever);

    /**TESTPOINT: test K_MUTEX_DEFINE mutex*/
    tmutex_test_lock(&kmutex, tThread_entry_lock_forever);
}

void test_mutex_reent_lock_no_wait(void)
{
    /**TESTPOINT: test k_mutex_init mutex*/
    tmutex_test_lock(&mutex, tThread_entry_lock_no_wait);

    /**TESTPOINT: test K_MUTEX_DEFINE mutex*/
    tmutex_test_lock(&kmutex, tThread_entry_lock_no_wait);
}

void test_mutex_reent_lock_timeout_fail(void)
{
    /**TESTPOINT: test k_mutex_init mutex*/
    tmutex_test_lock_timeout(&mutex, tThread_entry_lock_timeout_fail);

    /**TESTPOINT: test K_MUTEX_DEFINE mutex*/
    tmutex_test_lock_timeout(&kmutex, tThread_entry_lock_no_wait);
}

void test_mutex_reent_lock_timeout_pass(void)
{
    /**TESTPOINT: test k_mutex_init mutex*/
    tmutex_test_lock_timeout(&mutex, tThread_entry_lock_timeout_pass);

    /**TESTPOINT: test K_MUTEX_DEFINE mutex*/
    tmutex_test_lock_timeout(&kmutex, tThread_entry_lock_no_wait);
}

void test_mutex_lock_unlock(void)
{
    /**TESTPOINT: test k_mutex_init mutex*/
    tmutex_test_lock_unlock(&mutex);

    /**TESTPOINT: test K_MUTEX_DEFINE mutex*/
    tmutex_test_lock_unlock(&kmutex);
}

