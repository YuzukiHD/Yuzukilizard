/*
 * ===========================================================================================
 *
 *       Filename:  perf.c
 *
 *    Description:  this file used for tast performance monitor.
 *
 *        Version:  Melis3.0
 *         Create:  2018-04-20 15:49:40
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-07-02 19:27:15
 *
 * ===========================================================================================
 */

#include <stdio.h>

#include <rtthread.h>
#include <rthw.h>

#include <excep.h>

extern int kthread_get_fpustat(void);
void dump_memory(rt_uint32_t *buf, rt_int32_t len);
static  rt_thread_t rt_perf_task = RT_NULL;
static  rt_tick_t   mon_start_time;
extern  struct rt_object_information rt_object_container[];

/* ----------------------------------------------------------------------------*/
/** @brief  monitor_start  trigger the monitor event. */
/* ----------------------------------------------------------------------------*/
void monitor_start(void)
{
    struct rt_object_information *information;
    struct rt_object *object;
    struct rt_list_node *node;
    rt_thread_t temp;

    rt_enter_critical();

    information = rt_object_get_information(RT_Object_Class_Thread);
    RT_ASSERT(information != RT_NULL);
    for (node = information->object_list.next; node != &(information->object_list); node = node->next)
    {
        object = rt_list_entry(node, struct rt_object, list);
        temp = (rt_thread_t)object;

        temp->montime = temp->cputime;
        temp->sched_i = temp->sched_o = 0;
    }

    rt_exit_critical();

    mon_start_time = rt_tick_get();

    return;
}

/* ----------------------------------------------------------------------------*/
/** @brief  monitor_end stop the current monitor session. */
/* ----------------------------------------------------------------------------*/
void monitor_end(void)
{
    struct rt_object_information *information;
    struct rt_object *object;
    struct rt_list_node *node;
    rt_thread_t temp;
    rt_uint32_t total, used, max_used;
    rt_uint8_t *ptr;
    int fpu_status = 0;
    char *stat;
    float stk_usage, cpu_usage;

    printf("\r\n");
    printf("   ---------------------------------------------TSK Usage Report(tick:%08ld)-----------------------------------------------------\n", \
           rt_tick_get());
    printf("        name   errno    entry    cputime     stat   prio     tcb     slice stacksize  stkusg  lt     pc      si   so  born   fpu\n");

    rt_enter_critical();

    information = rt_object_get_information(RT_Object_Class_Thread);
    RT_ASSERT(information != RT_NULL);
    for (node = information->object_list.next; node != &(information->object_list); node = node->next)
    {
        rt_uint8_t status;
        rt_ubase_t pc = 0xdeadbeef;
        switch_ctx_regs_t *regs_ctx;

        object = rt_list_entry(node, struct rt_object, list);
        temp = (rt_thread_t)object;

        if (temp != rt_thread_self())
        {
            regs_ctx = (switch_ctx_regs_t *)temp->sp;
        }
        else
        {
            regs_ctx = RT_NULL;
        }
#ifdef CONFIG_RISCV
        fpu_status = 1;
#else
        fpu_status = regs_ctx ? regs_ctx->use_fpu : kthread_get_fpustat();
#endif

#ifdef CONFIG_RISCV
        if (regs_ctx)
        {
            pc = regs_ctx->ra;
        }
#else
        if (regs_ctx && (regs_ctx->use_fpu == 1))
        {
            pc = regs_ctx->context.have_neon.pc;
        }
        else if (regs_ctx && (regs_ctx->use_fpu == 0))
        {
            pc = regs_ctx->context.no_neon.pc;
        }
#endif
        else if (regs_ctx == RT_NULL)
        {
            void rt_perf_init(void);
            pc = (rt_ubase_t)rt_perf_init;
        }

        status = (temp->stat & RT_THREAD_STAT_MASK);
        if (status == RT_THREAD_READY)
        {
            stat = "running";
        }
        else if (status == RT_THREAD_SUSPEND)
        {
            stat = "suspend";
        }
        else if (status == RT_THREAD_INIT)
        {
            stat = "initing";
        }
        else if (status == RT_THREAD_CLOSE)
        {
            stat = "closing";
        }
        else
        {
            stat = "unknown";
        }

        ptr = (rt_uint8_t *)temp->stack_addr;
        while (*ptr == '#')
        {
            ptr ++;
        }

        stk_usage = (float)(temp->stack_size - ((rt_ubase_t) ptr - (rt_ubase_t)temp->stack_addr)) * 100 / (float)temp->stack_size;
        cpu_usage = ((float)temp->cputime - (float)temp->montime) / 2;
        if (cpu_usage == 100)
        {
            cpu_usage = 99.95;
        }
        printf("%12.12s%6ld   0x%08lx  %06.2f%% %9s %4d   0x%08lx  %3ld %8d    %05.2f%%  %02ld 0x%lx %04d %04d %04ld    %d\n", \
               temp->name,
               temp->error,
               (rt_ubase_t)temp->entry,
               (temp == rt_thread_self()) ? 0.05 : cpu_usage,
               stat,
               temp->current_priority,
               (rt_ubase_t)temp,
               temp->init_tick,
               temp->stack_size,
               stk_usage,
               temp->remaining_tick, pc, \
               temp->sched_i, temp->sched_o, temp->born, fpu_status);
    }

    printf("   ---------------------------------------------------------------------------------------------------------------------------------\n");
    rt_memory_info(&total, &used, &max_used);
    printf("    memory info:\n\r");
    printf("\tTotal  0x%08x\n\r" \
           "\tUsed   0x%08x\n\r" \
           "\tMax    0x%08x\n\r", \
           total, used, max_used);

    rt_exit_critical();

    return;
}

static void rt_perf_thread(void *arg)
{
    printf("\e[1;1H\e[2J");
	(void)arg;

    while (1)
    {
        monitor_start();

        //delay 10 sectionds.
        rt_thread_delay(200);
        printf("\e[1;1H\e[2J");
        monitor_end();
    }
}

static void rt_sched_hook(struct rt_thread *from, struct rt_thread *to)
{
    if (from)
    {
        from->sched_o ++;
    }

    if (to)
    {
        to->sched_i ++;
    }
}

void rt_perf_init(void)
{
    rt_ubase_t level;

    if (rt_perf_task)
    {
        printf("performance task already exist.");
        return;
    }

    level = rt_hw_interrupt_disable();
    rt_scheduler_sethook(rt_sched_hook);
    rt_hw_interrupt_enable(level);

    /* set the priority of performance task to highest prio of others. */
    rt_perf_task = rt_thread_create("perf", rt_perf_thread, RT_NULL, 0x2000, 1, 30);
    if (!rt_perf_task)
    {
        printf("fatal error, create task failure.");
        return;
    }
    rt_thread_startup(rt_perf_task);

    return;
}

void rt_perf_exit(void)
{
    rt_ubase_t level;

    if (rt_perf_task == RT_NULL)
    {
        return;
    }

    rt_thread_delete(rt_perf_task);
    rt_perf_task = RT_NULL;
    mon_start_time = 0;

    level = rt_hw_interrupt_disable();
    rt_scheduler_sethook(RT_NULL);
    rt_hw_interrupt_enable(level);
    return;
}

static int sched_statistic_init(void)
{
    rt_scheduler_sethook(rt_sched_hook);
    return 0;
}
