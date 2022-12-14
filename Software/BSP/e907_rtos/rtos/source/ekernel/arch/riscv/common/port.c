/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdlib.h>
#include <rtthread.h>
#include <port.h>
#include <csr.h>
#include <irqflags.h>
#include <excep.h>
#include <cpuport.h>
#include <typedef.h>
#include <debug.h>
#include <rtdef.h>
#include <compiler.h>

unsigned long awos_arch_lock_irq(void)
{
    return arch_local_irq_save();
}

void awos_arch_unlock_irq(unsigned long cpu_sr)
{
    arch_local_irq_restore(cpu_sr);
}

rt_base_t rt_hw_interrupt_disable(void)
{
    return arch_local_irq_save();
}
RTM_EXPORT(rt_hw_interrupt_disable);

void rt_hw_interrupt_enable(rt_base_t level)
{
    arch_local_irq_restore(level);
}
RTM_EXPORT(rt_hw_interrupt_enable);

int irqs_disabled(void)
{
    return arch_irqs_disabled();
}

void local_irq_enable(void)
{
    arch_local_irq_enable();
}

void local_irq_disable(void)
{
    arch_local_irq_disable();
}

void finish_task_switch(rt_thread_t ARG_UNUSED(last));
void ret_from_create_c(unsigned long *frsp, unsigned long *tosp)
{
    rt_thread_t last =  RT_NULL;
    rt_thread_t curr =  RT_NULL;

    /* thread fn of entry. */
    register long s1 __asm__("s1");
    /* thread paramter of entry. */
    register long s2 __asm__("s2");
    /* thread exit entry. */
    register long s3 __asm__("s3");

    void (*entry)(void *) = (void (*)(void *))s1;
    void (*exity)(void)   = (void (*)(void))s3;
    void *parameter       = (void *)s2;

    if (frsp == RT_NULL)
    {
        rt_kprintf("scheduler startup\n");
    }
    else
    {
        last = rt_container_of(frsp, struct rt_thread, sp);
    }

    finish_task_switch(last);

    curr = rt_container_of(tosp, struct rt_thread, sp);
    if (last == rt_thread_self())
    {
        software_break();
    }
    else if (curr != rt_thread_self())
    {
        software_break();
    }

    // enable local irq.
    //arch_local_irq_enable();
    if (irqs_disabled())
    {
        software_break();
    }

    if (exity != rt_thread_exit)
    {
        software_break();
    }

    if (entry)
    {
        entry(parameter);
    }
    else
    {
        software_break();
    }

    exity();

    /* never come here */
    CODE_UNREACHABLE;
}

/**
 * This function will initialize thread stack
 *
 * @param tentry the entry of thread
 * @param parameter the parameter of entry
 * @param stack_addr the beginning stack address
 * @param texit the function will be called when thread exit
 *
 * @return stack address
 */
uint8_t *awos_arch_stack_init(void *tentry, void *parameter, uint8_t *stack_addr, void *texit)
{
    rt_uint8_t         *stk;
    switch_ctx_regs_t *frame;

    stk  = stack_addr + sizeof(rt_ubase_t);
    stk  = (rt_uint8_t *)RT_ALIGN_DOWN((rt_ubase_t)stk, REGBYTES);
    stk -= sizeof(switch_ctx_regs_t);

    frame = (switch_ctx_regs_t *)stk;

    frame->ra      = (rt_ubase_t)ret_from_create_c;

    // ABI take as FP, so cant pass parameter by S0.
    frame->s[0]    = (rt_ubase_t) -1;
    frame->s[1]    = (rt_ubase_t)tentry;
    frame->s[2]    = (rt_ubase_t)parameter;
    frame->s[3]    = (rt_ubase_t)texit;
    frame->s[4]    = (rt_ubase_t)0xdeadbeef;
    frame->s[5]    = (rt_ubase_t)0xdeadbeef;
    frame->s[6]    = (rt_ubase_t)0xdeadbeef;
    frame->s[7]    = (rt_ubase_t)0xdeadbeef;
    frame->s[8]    = (rt_ubase_t)0xdeadbeef;
    frame->s[9]    = (rt_ubase_t)0xdeadbeef;
    frame->s[10]   = (rt_ubase_t)0xdeadbeef;
    frame->s[11]   = (rt_ubase_t)0xdeadbeef;

    // Restore Previous Interrupt Enable and Previous Privledge level status.
#ifdef CONFIG_RV_MACHINE_MODE
    frame->sstatus = MR_MPP | MR_MPIE | SR_FS_CLEAN; /*  Supervisor, irqs on, Initial fpu for ready to use */
#else
    frame->sstatus = SR_SPP | SR_SPIE | SR_FS_CLEAN; /*  Supervisor, irqs on, Initial fpu for ready to use */
#endif

    return stk;
}
