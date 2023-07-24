/*
 * ===========================================================================================
 *
 *       Filename:  mmuprotect.c
 *
 *    Description:  mmuprotect test case.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-11 09:26:14
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-06-11 14:55:09
 *
 * ===========================================================================================
 */
#include <rtthread.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <finsh.h>
#include <debug.h>
#include <log.h>

static int cmd_mmuprotect(int argc, const char **argv)
{
    void armv_vector_table(void);
    printf("protect addr is %p.", armv_vector_table);
    memset(armv_vector_table, 0x00, 32);
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_mmuprotect, __cmd_mmuprotect, mmuprotect test);
