/*
 * =====================================================================================
 *
 *       Filename:  test.h
 *
 *    Description:  header file
 *
 *        Version:  2.0
 *         Create:  2017-12-01 16:58:10
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2017-12-01 17:29:30
 *
 * =====================================================================================
 */

#ifndef __TEST_PATTERN_ZEPHYR__
#define __TEST_PATTERN_ZEPHYR__
#include <zephyr.h>
#include <kernel_structs.h>
#include <arch/cpu.h>
#include <ksched.h>
#include <log.h>
#include <debug.h>
#include <log.h>
#include <debug.h>

struct thread_data
{
    k_tid_t tid;
    int priority;
    int executed;
};

static inline void _zassert(int cond, const char *msg, const char *default_msg,
                            const char *file, int line, const char *func)
{
    if (!(cond))
    {
        __log("Assertion failed at %s:%d: %s: %s %s", file, line, func, msg, default_msg);
        software_break();
    }
    else
    {
        __log("Assertion succeeded at %s:%d (%s)", file, line, func);
    }
}

#define zassert(cond, msg, default_msg) \
    _zassert(cond, msg ? msg : "", msg ? ("(" default_msg ")") : (default_msg), \
             __FILE__, __LINE__, __func__)

#define zassert_true(cond, msg) zassert(cond, msg, #cond " is false")
#define zassert_false(cond, msg) zassert(!(cond), msg, #cond " is true")

#endif
