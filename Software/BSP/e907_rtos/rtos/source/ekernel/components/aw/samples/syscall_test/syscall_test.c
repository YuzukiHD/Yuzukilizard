/*
 * ===========================================================================================
 *
 *       Filename:  syscall_test.c
 *
 *    Description:  syscall test for multi parameters.
 *
 *        Version:  Melis3.0
 *         Create:  2018-11-30 11:06:08
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-05-22 14:03:02
 *
 * ===========================================================================================
 */

#include <syscall.h>
#include <typedef.h>

__attribute__((target("arm"))) uint32_t foolbar(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t result;

    result =  __syscall4(5, a, b, c, d);
    return result;
}

__attribute__((target("arm"))) uint32_t foolbar5(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t result;

    result =  __syscall5(1, a, b, c, d, e);
    return result;
}

__attribute__((target("arm"))) uint32_t foolbar6(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f)
{
    uint32_t result;

    result =  __syscall6(5, a, b, c, d, e, f);
    return result;
}
