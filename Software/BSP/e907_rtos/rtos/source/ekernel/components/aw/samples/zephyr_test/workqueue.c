/*
 * =====================================================================================
 *
 *       Filename:  ekernel/components/samples/zephyr_test/workqueue.c
 *
 *    Description:  test workqueue for zephyr kernel.
 *
 *        Version:  2.0
 *         Create:  2017-11-22 17:24:17
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2017-12-01 20:21:04
 *
 * =====================================================================================
 */

#include "workqueue.h"
#include <zephyr.h>
#include <kernel_structs.h>
#include <arch/arm/cortex_m/exc.h>
#include <arch/cpu.h>
#include <ksched.h>
#include <log.h>
#include <stdbool.h>
#include <zephyr.h>
#include <misc/util.h>
#include <misc/nano_work.h>
#include <debug.h>

#define NUM_TEST_ITEMS          6
#define SUBMIT_WAIT             50
#define STACK_SIZE              2048
#define WORK_ITEM_WAIT          100

struct test_item
{
    int key;
    struct k_delayed_work work;

};
static K_THREAD_STACK_DEFINE(co_op_stack, STACK_SIZE);
static struct test_item tests[NUM_TEST_ITEMS];
static int results[NUM_TEST_ITEMS];
static int num_results;
static struct k_thread co_op_data;

static void work_handler(struct k_work *work)
{
    struct test_item *ti = CONTAINER_OF(work, struct test_item, work);

    __log(" - Running test item %d", ti->key);
    k_sleep(WORK_ITEM_WAIT);

    results[num_results++] = ti->key;
}
static void test_items_init(void)
{
    int i;

    for (i = 0; i < NUM_TEST_ITEMS; i++)
    {
        tests[i].key = i + 1;
        k_work_init(&tests[i].work.work, work_handler);
    }
}


static void coop_work_main(int arg1, int arg2)
{
    int i;

    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);

    /*  Let the preempt thread submit the first work item. */
    k_sleep(SUBMIT_WAIT / 2);

    for (i = 1; i < NUM_TEST_ITEMS; i += 2)
    {
        k_work_submit(&tests[i].work.work);
        __log("Submitting work %d.", i + 1);
        k_sleep(SUBMIT_WAIT);
    }
}

static void test_items_submit(void)
{
    int i;

    k_thread_create(&co_op_data, co_op_stack, STACK_SIZE,
                    (k_thread_entry_t)coop_work_main,
                    NULL, NULL, NULL, K_PRIO_COOP(10), 0, 0);

    for (i = 0; i < NUM_TEST_ITEMS; i += 2)
    {
        k_work_submit(&tests[i].work.work);
        __log("Submitting work %d.", i + 1);
        k_sleep(SUBMIT_WAIT);
    }
}

static int check_results(int num_tests)
{
    int i;

    if (num_results != num_tests)
    {
        __log("*** work items finished: %d (expected: %d)", num_results, num_tests);
        return -1;
    }

    for (i = 0; i < num_tests; i ++)
    {
        if (results[i] != i + 1)
        {
            __log("*** got result %d in position %d (expected %d)", results[i], i, i + 1);
            return -1;
        }
    }

    return 0;
}

static int test_sequence(void)
{
    __log("Starting sequence test");
    __log(" - Initializing test items");
    test_items_init();
    __log(" - Submitting test items");
    test_items_submit();
    __log(" - Waiting for work to finish");
    k_sleep((NUM_TEST_ITEMS + 1) * WORK_ITEM_WAIT);
    __log(" - Checking results");
    return check_results(NUM_TEST_ITEMS);
}

static void resubmit_work_handler(struct k_work *work)
{
    struct test_item *ti = CONTAINER_OF(work, struct test_item, work);

    k_sleep(WORK_ITEM_WAIT);

    results[num_results++] = ti->key;

    if (ti->key < NUM_TEST_ITEMS)
    {
        ti->key++;
        __log(" - Resubmitting work");
        k_work_submit(work);
    }
}

static int test_resubmit(void)
{
    __log("Starting resubmit test");

    tests[0].key = 1;
    tests[0].work.work.handler = resubmit_work_handler;

    __log(" - Submitting work");
    k_work_submit(&tests[0].work.work);

    __log(" - Waiting for work to finish");
    k_sleep((NUM_TEST_ITEMS + 1) * WORK_ITEM_WAIT);

    __log(" - Checking results");
    return check_results(NUM_TEST_ITEMS);
}

static void delayed_resubmit_work_handler(struct k_work *work)
{
    struct test_item *ti = CONTAINER_OF(work, struct test_item, work);

    results[num_results++] = ti->key;

    if (ti->key < NUM_TEST_ITEMS)
    {
        ti->key++;
        __log(" - Resubmitting delayed work");
        k_delayed_work_submit(&ti->work, WORK_ITEM_WAIT);
    }
}

static int test_delayed_resubmit(void)
{
    __log("Starting delayed resubmit test");

    tests[0].key = 1;
    k_delayed_work_init(&tests[0].work, delayed_resubmit_work_handler);

    __log(" - Submitting delayed work");
    k_delayed_work_submit(&tests[0].work, WORK_ITEM_WAIT);

    __log(" - Waiting for work to finish");
    k_sleep((NUM_TEST_ITEMS + 1) * WORK_ITEM_WAIT);

    __log(" - Checking results");
    return check_results(NUM_TEST_ITEMS);
}

static void delayed_work_handler(struct k_work *work)
{
    struct test_item *ti = CONTAINER_OF(work, struct test_item, work);

    __log(" - Running delayed test item %d", ti->key);

    results[num_results++] = ti->key;
}

static void test_delayed_init(void)
{
    int i;

    for (i = 0; i < NUM_TEST_ITEMS; i++)
    {
        tests[i].key = i + 1;
        k_delayed_work_init(&tests[i].work, delayed_work_handler);
    }
}

static void coop_delayed_work_main(int arg1, int arg2)
{
    int i;

    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);

    /* Let the preempt thread submit the first work item. */
    k_sleep(SUBMIT_WAIT / 2);

    for (i = 1; i < NUM_TEST_ITEMS; i += 2)
    {
        __log(" - Submitting delayed work %d from coop thread", i + 1);
        k_delayed_work_submit(&tests[i].work,
                              (i + 1) * WORK_ITEM_WAIT);
    }
}

static int test_delayed_submit(void)
{
    int i;

    k_thread_create(&co_op_data, co_op_stack, STACK_SIZE,
                    (k_thread_entry_t)coop_delayed_work_main,
                    NULL, NULL, NULL, K_PRIO_COOP(10), 0, 0);

    for (i = 0; i < NUM_TEST_ITEMS; i += 2)
    {
        __log(" - Submitting delayed work %d from preempt thread", i + 1);
        if (k_delayed_work_submit(&tests[i].work,
                                  (i + 1) * WORK_ITEM_WAIT))
        {
            return -1;
        }
    }

    return 0;
}


static int test_delayed(void)
{
    __log("Starting delayed test");

    __log(" - Initializing delayed test items");
    test_delayed_init();

    __log(" - Submitting delayed test items");
    if (test_delayed_submit() != 0)
    {
        return -1;
    }

    __log(" - Waiting for delayed work to finish");
    k_sleep((NUM_TEST_ITEMS + 2) * WORK_ITEM_WAIT);

    __log(" - Checking results");
    return check_results(NUM_TEST_ITEMS);
}

static void coop_delayed_work_resubmit(int arg1, int arg2)
{
    int i;

    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);

    for (i = 0; i < NUM_TEST_ITEMS; i++)
    {
        volatile u32_t uptime;

        __log(" - Resubmitting delayed work with 1 ms");
        k_delayed_work_submit(&tests[0].work, 1);

        /* Busy wait 1 ms to force a clash with workqueue */
        uptime = k_uptime_get_32();
        while (k_uptime_get_32() == uptime)
        {
        }
    }
}

static int test_delayed_resubmit_fiber(void)
{
    __log("Starting delayed resubmit from coop thread test");

    tests[0].key = 1;
    k_delayed_work_init(&tests[0].work, delayed_work_handler);

    k_thread_create(&co_op_data, co_op_stack, STACK_SIZE,
                    (k_thread_entry_t)coop_delayed_work_resubmit,
                    NULL, NULL, NULL, K_PRIO_COOP(10), 0, 0);

    __log(" - Waiting for work to finish");
    k_sleep(NUM_TEST_ITEMS + 1);

    __log(" - Checking results");
    return check_results(1);
}

static void coop_delayed_work_cancel_main(int arg1, int arg2)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);

    k_delayed_work_submit(&tests[1].work, WORK_ITEM_WAIT);

    __log(" - Cancel delayed work from coop thread");
    k_delayed_work_cancel(&tests[1].work);

#if defined(CONFIG_POLL)
    k_delayed_work_submit(&tests[2].work, 0 /* Submit immeditelly */);

    __log(" - Cancel pending delayed work from coop thread");
    k_delayed_work_cancel(&tests[2].work);
#endif
}

static int test_delayed_cancel(void)
{
    __log("Starting delayed cancel test");

    k_delayed_work_submit(&tests[0].work, WORK_ITEM_WAIT);

    __log(" - Cancel delayed work from preempt thread");
    k_delayed_work_cancel(&tests[0].work);

    k_thread_create(&co_op_data, co_op_stack, STACK_SIZE,
                    (k_thread_entry_t)coop_delayed_work_cancel_main,
                    NULL, NULL, NULL, K_HIGHEST_THREAD_PRIO, 0, 0);

    __log(" - Waiting for work to finish");
    k_sleep(2 * WORK_ITEM_WAIT);

    __log(" - Checking results");
    return check_results(0);
}

static void reset_results(void)
{
    int i;

    for (i = 0; i < NUM_TEST_ITEMS; i++)
    {
        results[i] = 0;
    }

    num_results = 0;
}

void test_workqueue(void)
{
    int status = -1;

    if (test_sequence() != 0)
    {
        __err("workqueue failure.");
        software_break();
        goto end;
    }
    else
    {
        __log("success");
    }

    reset_results();

    if (test_resubmit() != 0)
    {
        __err("workqueue failure.");
        software_break();
        goto end;
    }
    else
    {
        __log("success");
    }

    reset_results();

    if (test_delayed() != 0)
    {
        __err("workqueue failure.");
        software_break();
        goto end;
    }
    else
    {
        __log("success");
    }

    reset_results();

    if (test_delayed_resubmit() != 0)
    {
        __err("workqueue failure.");
        software_break();
        goto end;
    }
    else
    {
        __log("success");
    }

    reset_results();

    if (test_delayed_resubmit_fiber() != 0)
    {
        __err("workqueue failure.");
        software_break();
        goto end;
    }
    else
    {
        __log("success");
    }

    reset_results();

    if (test_delayed_cancel() != 0)
    {
        __err("workqueue failure.");
        software_break();
        goto end;
    }
    else
    {
        __log("success");
    }
end:
    reset_results();
    return;
}

