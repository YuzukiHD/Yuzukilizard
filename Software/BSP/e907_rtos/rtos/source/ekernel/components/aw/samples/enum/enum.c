/*
 * ===========================================================================================
 *
 *       Filename:  enum.c
 *
 *    Description:  enum.
 *
 *        Version:  Melis3.0 
 *         Create:  2020-03-26 18:14:47
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-03-26 18:41:09
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

enum{
    kKeyMIMEType          = 'mime',  // cstring
    kKeyWidth             = 'widt',  // int32_t, image pixel
    kKeyHeight            = 'heig',  // int32_t, image pixel
    kKeyDisplayWidth      = 'dWid',  // int32_t, display/presentation
    kKeyDisplayHeight     = 'dHgt'   // int32_t, display/presentation
};

enum{
    short_enum_0 = 0,
    short_enum_1,
    short_enum_2
};
static void enum_entry(void)
{
    unsigned int abc = kKeyMIMEType;

    printf("%s line %d, abc = 0x%08x, sizeof(kKeyMIMEType) = %d, sizeof(short_enum_0) = %d.\n", \
	    __func__, __LINE__, abc, sizeof(kKeyMIMEType), sizeof(short_enum_0));
    return;
}

static int cmd_enum(int argc, const char **argv)
{
    enum_entry();
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_enum, __cmd_enum, enum test);
