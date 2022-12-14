/*
 * ===========================================================================================
 *
 *       Filename:  ktime_test.c
 *
 *    Description:  ldrex.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-11 15:49:18
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-06-15 15:51:20
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
#include <unistd.h>
#include <pthread.h>
#include <log.h>

static pthread_cond_t cond;
int cmd_cond(int argc, char **argv)
{
    int ret;

    ret = pthread_cond_init(&cond, NULL);
    if (ret != 0)
    {
        __err("ret is %d.", ret);
        return -1;
    }

    ret = pthread_cond_destroy(&cond);
    if (ret != 0)
    {
        __err("ret is %d.", ret);
        return -2;
    }

    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_cond, __cmd_cond, pthread_cond test);
