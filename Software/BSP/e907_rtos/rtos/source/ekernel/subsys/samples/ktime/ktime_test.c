/*
 * ===========================================================================================
 *
 *       Filename:  ktime_test.c
 *
 *    Description:  ktime get.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-11 15:49:18
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-03-11 16:02:25
 *
 * ===========================================================================================
 */

#include <rtthread.h>
#include <dfs_posix.h>
#include <stdio.h>
#include <stdlib.h>
#include <ktimer.h>
#include <debug.h>
#include <finsh_api.h>
#include <finsh.h>


static int ktime_test(void)
{
    uint64_t new, old;

    new = old = 0;

    old = ktime_get();

    while (1)
    {
        rt_thread_delay(10);
        new = ktime_get();
        if (new < old)
        {
            printf("%s line %d, error, new %lld, old %lld\n", __func__, __LINE__, new, old);
            software_break();
        }
        else
        {
            printf("%s line %d, new %lld, old %lld\n", __func__, __LINE__, new, old);
            old = new;
        }
    }

    return 0;
}

static int cmd_ktimeget(int argc, const char **argv)
{
    ktime_test();
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_ktimeget, __cmd_ktimeget, ktimetest);
