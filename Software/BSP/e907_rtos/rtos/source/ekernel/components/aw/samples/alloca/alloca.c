/*
 * ===========================================================================================
 *
 *       Filename:  alloca.c
 *
 *    Description:  alloca test.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-31 11:05:01
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-04-04 15:21:18
 *
 * ===========================================================================================
 */
#include <rtthread.h>
#include <finsh_api.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <debug.h>
#include <log.h>

static int foobar(void)
{
    int array[100] = {0};
    int i = 0;

    for (i = 0; i < 100; i ++)
    {
        array[i] = i * i;
    }

    int *arr = alloca(50 * sizeof(int));

    for (i = 0; i < 50; i ++)
    {
        arr[i] = 0x5a5a5a5a;
    }

    return 0;
}

int cmd_alloca(int argc, char **argv)
{
    foobar();
    return 0;
}

static int page_alloc_test(int npage)
{
    int i = 0;
    void *ptr;

    for (i = 0; i < 100; i ++)
    {
        ptr = rt_page_alloc(npage);
        if (ptr == RT_NULL)
        {
            __err("fatal error, ptr = 0x%08lx.", (unsigned long)ptr);
            software_break();
        }

        rt_page_free(ptr, npage);
        __log("test alloc %d pages %d times, ptr = 0x%lx.", npage, i, (unsigned long)ptr);
        ptr = RT_NULL;
    }
    return 0;
}

int cmd_page_alloc(int argc, char **argv)
{
    page_alloc_test(1);
    rt_thread_delay(100);
    page_alloc_test(6);
    rt_thread_delay(100);
    page_alloc_test(10);

    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_alloca, __cmd_alloca, alloca test);
FINSH_FUNCTION_EXPORT_ALIAS(cmd_page_alloc, __cmd_page_alloc, page alloc test);
