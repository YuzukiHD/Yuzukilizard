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
 *  Last Modified:  2020-03-11 14:31:57
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

static int pipefd[2];
static char dev_name[32] = {0};

static void read_pipeline_entry(void *para)
{
    char *buf[256];

    while (1)
    {
        rt_memset(buf, 0x00, 256);
        int ret = read(pipefd[0], buf, 256);
        if (ret < 0)
        {
            __err("fatal error, read pipe failure.ret %d.", ret);
            software_break();
        }
        else
        {
            __log("fd %d, receive success, len: %d, str: %s", pipefd[0], ret, buf);
        }
    }
}

static void write_pipeline_entry(void *para)
{
    char *buf = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (1)
    {
        rt_thread_delay(100);
        int ret = write(pipefd[1], buf, strlen(buf));
        if (ret < 0)
        {
            __err("fatal error, write pipe failure.ret %d.", ret);
            software_break();
        }
        else
        {
            __log("write %d bytes success.", ret);
        }
    }
}

int mkfifo(const char *path, mode_t mode);
static void mkfifo_entry(void)
{
    rt_thread_t read_pipe_thread, write_pipe_thread;
    char dname[12] = {0};
    static int mkfifono = 0;

    rt_snprintf(dname, sizeof(dname), "mkfifo%d", mkfifono ++);

    int ret = mkfifo(dname, 0666);
    if (ret != 0)
    {
        __err("create pipe failure!");
        software_break();
    }

    rt_snprintf(dev_name, sizeof(dev_name), "/dev/%s", dname);

    pipefd[0] = open(dev_name, O_RDONLY, 0);
    if (pipefd[0] < 0)
    {
        __err("fatal error, open named pipe failure.");
        software_break();
    }

    pipefd[1] = open(dev_name, O_WRONLY, 0);
    if (pipefd[1] < 0)
    {
        __err("fatal error, open named pipe failure.");
        software_break();
    }

    read_pipe_thread = rt_thread_create("readpipe", read_pipeline_entry, RT_NULL, 0x1000, 1, 10);
    RT_ASSERT(read_pipe_thread != RT_NULL);
    rt_thread_startup(read_pipe_thread);

    write_pipe_thread = rt_thread_create("writepipe", write_pipeline_entry, RT_NULL, 0x1000, 1, 10);
    RT_ASSERT(write_pipe_thread != RT_NULL);
    rt_thread_startup(write_pipe_thread);

    return;
}

static int cmd_mkfifo(int argc, const char **argv)
{
    mkfifo_entry();
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_mkfifo, __cmd_mkfifo, mkfifo test);
