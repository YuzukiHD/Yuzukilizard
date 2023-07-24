/*
 * ===========================================================================================
 *
 *       Filename:  armeabi.c
 *
 *    Description:  armeabi test.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-12 16:31:38
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-05-19 09:22:17
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

/*
 * ARMEABI:
 * push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
 * add     fp, sp, #32
 * nop     {0}
 * pop     {r4, r5, r6, r7, r8, r9, sl, fp, pc}
 *
 * r0-r3 : caller saved register.
 * r4-r11: callee saved register.
 * r12:    ip, caller saved register.
 * lr:     caller saved register.
 * sp, pc: callee saved register.
 */

#if 1
OPTIMILIZE_O0_LEVEL \
noinline void foo(void)
{
    __asm__ volatile("nop\n" "mov ip, #2": : : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", \
                     /* "r11", */ "r12", "r14");
}
#endif

OPTIMILIZE_O0_LEVEL void foo_bar0(void)
{
    asm volatile("mov ip, #1\n" : : : "ip");
    foo();
    asm volatile("mov r0, ip\n" : : : "ip", "r0");
}

OPTIMILIZE_O0_LEVEL long foo_bar1(void)
{
    long temp;
    register long ip __asm__("ip");
    ip = 1;
    foo();
    temp = ip;
    return temp;
}

OPTIMILIZE_O0_LEVEL void foo_bar2(void)
{
    long temp;
    register long r4 __asm__("r4");
    r4 = 1;
    foo();
    temp = r4;
}

OPTIMILIZE_O0_LEVEL long foo_bar3(void)
{
    long temp, temp1;
    register long ip __asm__("ip");
    temp1 = ip;
    foo();
    temp = ip;
    return temp;
}

OPTIMILIZE_O0_LEVEL long foo_bar4(void)
{
    long temp, temp1;
    register long r4 __asm__("r4");
    temp1 = r4;
    foo();
    temp = r4;
    return temp;
}

OPTIMILIZE_O0_LEVEL float floateabi(int a, int b, float c, float d)
{
    return a + b + c + d;
}

OPTIMILIZE_O0_LEVEL float floateabi_exceed(int a, int b, float c, float d, int x, int y, float m, float n)
{
    return a + b + c + d + x + y + m + n;
}

OPTIMILIZE_O0_LEVEL static int main_armeabi(void)
{
    int a = 1;
    int b = 2;
    float c = 3.14159;
    float d = 2.71828;

    floateabi(a, b, c, d);
    floateabi_exceed(a, b, c, d, a, b, c, d);
    return 0;
}

OPTIMILIZE_O0_LEVEL int foobar_nofloat(int a, int b)
{
    __log("a = %d, b = %d.", a, b);
    return 0;
}

OPTIMILIZE_O0_LEVEL int foobar_float(float a, float b, int c, int d)
{
    printf("%s line %d. a = %f, b = %f, c = %d, d = %d\n", __func__, __LINE__, a, b, c, d);
    return 0;
}

OPTIMILIZE_O0_LEVEL static int cmd_armeabi(int argc, const char **argv)
{
    int a = 6;
    int b = 8;
    float c = 3.141592;
    float d = 2.718282;

    foobar_nofloat(a, b);
    foobar_float(c, d, a, b);
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_armeabi, __cmd_armeabi, armeabi test);
