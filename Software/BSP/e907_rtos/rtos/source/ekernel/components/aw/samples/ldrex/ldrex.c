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
 *  Last Modified:  2020-03-20 17:29:42
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

typedef unsigned long spin_lock_t;

static spin_lock_t  spinlck = 0;
void __init_key(spin_lock_t *lck);
void __lock_key_arm(spin_lock_t *lck);
int __unlock_key_arm(spin_lock_t *lck);

static void ldrex1_entry(void *para)
{
    while (1)
    {
        __lock_key_arm(&spinlck);
        __unlock_key_arm(&spinlck);
        printf("[DBG]: %s line %d.\n", __func__, __LINE__);
    }
}

static void ldrex2_entry(void *para)
{
    while (1)
    {
        __lock_key_arm(&spinlck);
        __unlock_key_arm(&spinlck);
        printf("[DBG]: %s line %d.\n", __func__, __LINE__);
    }
}

static int cmd_ldrex(int argc, const char **argv)
{
    __init_key(&spinlck);

    rt_thread_t ldrex_thread1, ldrex_thread2;

    rt_enter_critical();
    ldrex_thread1 = rt_thread_create("ldrex-1", ldrex1_entry, RT_NULL, 0x1000, 1, 10);
    RT_ASSERT(ldrex_thread1 != RT_NULL);
    rt_thread_startup(ldrex_thread1);

    ldrex_thread2 = rt_thread_create("ldrex-2", ldrex2_entry, RT_NULL, 0x1000, 1, 10);
    RT_ASSERT(ldrex_thread2 != RT_NULL);
    rt_thread_startup(ldrex_thread2);
    rt_exit_critical();
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_ldrex, __cmd_ldrex, ldrex test);
