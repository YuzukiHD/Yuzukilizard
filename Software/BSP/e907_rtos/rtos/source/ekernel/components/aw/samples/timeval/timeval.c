/*
 * =====================================================================================
 *
 *       Filename:  timeval.h
 *
 *    Description:  test struct of timeval
 *
 *        Version:  1.0
 *        Created:  2020年03月11日 11时24分11秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */

#include <rtthread.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <time.h>
#include <finsh.h>
#include <debug.h>
#include <pipe.h>
#include <log.h>

struct timeval_another
{
    long arg1;
    long arg2;
};

static int cmd_timeval(int argc, const char **argv)
{
    __log("timeval %d, long %d, timievall %d", sizeof(struct timeval), sizeof(long), sizeof(struct timeval_another));
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_timeval, __cmd_timeval, timeval test);
