/*
 * =====================================================================================
 *
 *       Filename:  prep_c.c
 *
 *    Description:  zephyr kernel bringup procedure.
 *
 *        Version:  2.0
 *         Create:  2017-11-13 11:35:27
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2017-12-01 19:04:07
 *
 * =====================================================================================
 */

#include <toolchain/gcc.h>
#include <nano_internal.h>
#include <drivers/system_timer.h>
#include <ksched.h>
#include <debug.h>
#include <exc.h>
#include <log.h>

/*****************************/
/*  CPU Mode                 */
/*****************************/
#define USERMODE        0x10
#define FIQMODE         0x11
#define IRQMODE         0x12
#define SVCMODE         0x13
#define SYSMODE         0x1f
#define ABORTMODE       0x17
#define UNDEFMODE       0x1b
#define MODEMASK        0x1f
#define NOINT           0xc0

unsigned int melis_syscall_table[256] = {0, };

void k_cpu_idle(void)
{
    /* TODO  */
}

void _IntLibInit(void)
{
    /* TODO  */
}

void k_thread_end(void)
{
    software_break();
}

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_irq_trap_enter <enter interrupt log.> */
/* ----------------------------------------------------------------------------*/
void awos_arch_irq_trap_enter(void)
{
    _kernel.nested ++;
}

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_irq_trap_leave <leave interrupt log.> */
/* ----------------------------------------------------------------------------*/
void awos_arch_irq_trap_leave(void)
{
    _kernel.nested --;
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief  _new_thread <zephyr thread initializ routine, specify for arm926ej-s>
 *
 * @param thread
 * @param stack_base
 * @param stack_size
 * @param entry
 * @param p1
 * @param p2
 * @param p3
 * @param prio
 * @param options
 */
/* ----------------------------------------------------------------------------*/
void _new_thread(struct k_thread *thread, k_thread_stack_t stack_base, \
                 size_t stack_size, void (*entry)(void *, void *, void *), \
                 void *p1, void *p2, void *p3, int prio, unsigned int options)
{
    struct __esf *init_ctx;
    char *stack_memory;
    char *stack_end;

    stack_memory = K_THREAD_STACK_BUFFER(stack_base);
    _ASSERT_VALID_PRIO(prio, entry);

    _new_thread_init(thread, stack_memory, stack_size, prio, options);

    stack_end = stack_memory + stack_size;

    init_ctx = (struct __esf *)(STACK_ROUND_DOWN(stack_end - sizeof(struct __esf)));

    memset(init_ctx, 0x00, sizeof(struct __esf));

    void _thread_entry_wrapper(_thread_entry_t, void *, void *, void *);
    init_ctx->pc  = (u32_t)_thread_entry_wrapper;
    init_ctx->ip  = 0xdeadbeef;
    init_ctx->r11 = 0xdeadbeef;
    init_ctx->r10 = 0xdeadbeef;
    init_ctx->r09 = 0xdeadbeef;
    init_ctx->r08 = 0xdeadbeef;
    init_ctx->r07 = 0xdeadbeef;
    init_ctx->r06 = 0xdeadbeef;
    init_ctx->r05 = 0xdeadbeef;
    init_ctx->r04 = 0xdeadbeef;
    init_ctx->r03 = (u32_t)p3;
    init_ctx->r02 = (u32_t)p2;
    init_ctx->r01 = (u32_t)p1;
    init_ctx->r00 = (u32_t)entry;
    init_ctx->spsr = SVCMODE;

    thread->callee_saved.sp = (u32_t)init_ctx;
    /* entry just for audit */
    thread->caller_saved.pc = (u32_t)entry;
    /* must return pEentry for the thread startup by '_Swap' */
    _set_thread_return_value(thread, (u32_t)0);

    thread_monitor_init(thread);
}

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_tick_increase <++> */
/* ----------------------------------------------------------------------------*/
void awos_arch_tick_increase(void)
{
    _sys_clock_final_tick_announce();
}

void _PrepC(void)
{
    _Cstart();
    CODE_UNREACHABLE;
}

