/*
 * =====================================================================================
 *
 *       Filename:  sched_irq.c
 *
 *    Description:  schedule during irq lock.
 *
 *        Version:  2.0
 *         Create:  2017-11-09 15:37:55
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2019-01-30 20:32:26
 *
 * =====================================================================================
 */

#include <typedef.h>
#include <rtthread.h>
#include <coroutine.h>
#include <rthw.h>
#include <arch.h>
#include <log.h>

static void func1(struct context_from_t from)
{
    // check
    /*void **contexts = (void **)from.priv;*/

    // 先保存下主函数入口context，方便之后切换回去
    /*contexts[0] = from.fctx;*/

    // 初始化切换到func2
    /*from.fctx = contexts[2];*/

    // loop
    size_t count = 1;
    while (count ++)
    {
        rt_thread_delay(10);
        // trace
        __log("func1: %lu", count);

        // 切换到func2，返回后更新from中的context地址
        from = jump_fcontext(from.fctx, NULL);
    }

    __log("finished by func1: %lu", count);
    // 切换回主入口函数
    jump_fcontext(from.fctx, NULL);
}

static void func2(struct context_from_t from)
{
    // check
    /*void **contexts = (void **)from.priv;*/

    // loop
    size_t count = 1;
    while (count ++)
    {
        // trace
        rt_thread_delay(10);
        __log("func2: %lu", count);

        // 切换到func1，返回后更新from中的context地址
        from = jump_fcontext(from.fctx, NULL);
    }

    // 切换回主入口函数
    __log("finished by func2: %lu", count);
    jump_fcontext(from.fctx, NULL);
}


void coroutine_main(void)
{
    void            *contexts[3] = {0};
    unsigned char    stacks1[1024] = {0};
    unsigned char    stacks2[1024] = {0};
    struct context_from_t ctx1, ctx2;

    // 生成两个context上下文，绑定对应函数和堆栈
    contexts[1] = make_fcontext(stacks1 + sizeof(stacks1), sizeof(stacks1), func1);
    contexts[2] = make_fcontext(stacks2 + sizeof(stacks2), sizeof(stacks2), func2);

    ctx1.fctx = contexts[1];
    ctx2.fctx = contexts[2];

    // 切换到func1并传递私有参数：context数组
    while (1)
    {
        ctx1 = jump_fcontext(ctx1.fctx, NULL);
        __log("");
        ctx2 = jump_fcontext(ctx2.fctx, NULL);
    }
}
