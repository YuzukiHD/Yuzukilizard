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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <excep.h>
#include <rtthread.h>
#include <irqflags.h>
#include <debug.h>
#include <csr.h>
#include <log.h>

#ifdef CONFIG_DEBUG_BACKTRACE
#include <backtrace.h>
#endif

extern uint8_t melis_kernel_running;
void awos_arch_save_fpu_status(fpu_context_t *);
void awos_arch_restore_fpu_status(fpu_context_t *);
extern unsigned long esSYSCALL_function(irq_regs_t *regs, void *reserved);
extern void handle_exception(void);

void panic_goto_cli(void);

typedef unsigned long (* exception_callback)(
    char *str,
    unsigned long mcause,
	unsigned long mepc,
	unsigned long mtval,
	irq_regs_t *regs);

typedef struct _exception_handle_table_entry
{
    int cause;
    exception_callback callback;
    char *error_str;
} exception_handle_table_entry;

exception_handle_table_entry exception_table[] =
{
    {
        .cause = EXC_INST_MISALIGNED,
        .error_str = "EXC_INST_MISALIGNED",
    },
    {
        .cause = EXC_INST_ACCESS,
        .error_str = "EXC_INST_ACCESS",
    },
    {
        .cause = EXC_INST_ILLEGAL,
        .error_str = "EXC_INST_ILLEGAL",
    },
    {
        .cause = EXC_BREAKPOINT,
        .error_str = "EXC_BREAKPOINT",
    },
    {
        .cause = EXC_LOAD_MISALIGN,
        .error_str = "EXC_LOAD_MISALIGN",
    },
    {
        .cause = EXC_LOAD_ACCESS,
        .error_str = "EXC_LOAD_ACCESS",
    },
    {
        .cause = EXC_STORE_MISALIGN,
        .error_str = "EXC_STORE_MISALIGN",
    },
    {
        .cause = EXC_STORE_ACCESS,
        .error_str = "EXC_STORE_ACCESS",
    },
    {
        .cause = EXC_INST_PAGE_FAULT,
        .error_str = "EXC_INST_PAGE_FAULT",
    },
    {
        .cause = EXC_LOAD_PAGE_FAULT,
        .error_str = "EXC_LOAD_PAGE_FAULT",
    },
    {
        .cause = EXC_STORE_PAGE_FAULT,
        .error_str = "EXC_STORE_PAGE_FAULT",
    },
    {
        .cause = EXC_SYSCALL_FRM_U,
        .error_str = "EXC_SYSCALL_FRM_U",
    },
    {
        .cause = EXC_SYSCALL_FRM_M,
        .error_str = "EXC_SYSCALL_FRM_M",
    },
};

static int check_fpu_status_clean(void)
{
    unsigned long sstatus;

    sstatus = arch_local_save_flags();
    if ((sstatus & SR_FS) != SR_FS_CLEAN)
    {
        return 0;
    }
    return 1;
}

void fpu_save_inirq(unsigned long sstatus)
{
    rt_thread_t self = rt_thread_self();

    if (!melis_kernel_running)
    {
        return;
    }

    if (self == RT_NULL)
    {
        software_break();
    }

    if ((sstatus & SR_FS) == SR_FS_DIRTY)
    {
        awos_arch_save_fpu_status(&self->fdext_ctx);
    }

    if (!check_fpu_status_clean())
    {
        software_break();
    }
}

void fpu_restore_inirq(unsigned long sstatus)
{
    rt_thread_t self = rt_thread_self();

    if (!melis_kernel_running)
    {
        return;
    }

    if (self == RT_NULL)
    {
        software_break();
    }

    // interrupt handler vector operations is integrity.
    // and has finished, so need not save its context.
    if ((sstatus & SR_FS) == SR_FS_DIRTY)
    {
        awos_arch_restore_fpu_status(&self->fdext_ctx);
    }

    if (!check_fpu_status_clean())
    {
        software_break();
    }
}

static void show_register(irq_regs_t *regs, unsigned long stval, unsigned long scause)
{
#ifdef CONFIG_RTTKERNEL
    printk("thread: %s, entry: 0x%p, stack_base: 0x%p,stack_size: 0x%08x.\r\n", \
               rt_thread_self()->name, \
               rt_thread_self()->entry, \
               rt_thread_self()->stack_addr, \
               rt_thread_self()->stack_size);
#endif

#ifdef CONFIG_RV64
    printk("\r\ngprs:\r\n");
    printk(" x0:0x%016x   ra:0x%016x   sp:0x%016x   gp:0x%016x\r\n",
               0, regs->x1, regs->x2, regs->x3);
    printk(" tp:0x%016x   t0:0x%016x   t1:0x%016x   t2:0x%016x\r\n",
               regs->x4, regs->x5, regs->x6, regs->x7);
    printk(" s0:0x%016x   s1:0x%016x   a0:0x%016x   a1:0x%016x\r\n",
               regs->x8, regs->x9, regs->x10, regs->x11);
    printk(" a2:0x%016x   a3:0x%016x   a4:0x%016x   a5:0x%016x\r\n",
               regs->x12, regs->x13, regs->x14, regs->x15);
    printk(" a6:0x%016x   a7:0x%016x   s2:0x%016x   s3:0x%016x\r\n",
               regs->x16, regs->x17, regs->x18, regs->x19);
    printk(" s4:0x%016x   s5:0x%016x   s6:0x%016x   s7:0x%016x\r\n",
               regs->x20, regs->x21, regs->x22, regs->x23);
    printk(" s8:0x%016x   s9:0x%016x  s10:0x%016x  s11:0x%016x\r\n",
               regs->x24, regs->x25, regs->x26, regs->x27);
    printk(" t3:0x%016x   t4:0x%016x   t5:0x%016x   t6:0x%016x\r\n",
               regs->x28, regs->x29, regs->x30, regs->x31);

    printk("\r\nother:\r\n");
    printk("sepc    :0x%016x\r\n" \
               "sstatus :0x%016x\r\n" \
               "sscratch:0x%016x\r\n" \
               "stval   :0x%016x\r\n" \
               "scause  :0x%016x\r\n" \
               ,regs->epc, regs->status, regs->scratch, stval, scause);
#else
    rt_kprintf("\r\ngprs:\r\n");
    rt_kprintf(" x0:0x%08x   ra:0x%08x   sp:0x%08x   gp:0x%08x\r\n",
               0, regs->x1, regs->x2, regs->x3);
    rt_kprintf(" tp:0x%08x   t0:0x%08x   t1:0x%08x   t2:0x%08x\r\n",
               regs->x4, regs->x5, regs->x6, regs->x7);
    rt_kprintf(" s0:0x%08x   s1:0x%08x   a0:0x%08x   a1:0x%08x\r\n",
               regs->x8, regs->x9, regs->x10, regs->x11);
    rt_kprintf(" a2:0x%08x   a3:0x%08x   a4:0x%08x   a5:0x%08x\r\n",
               regs->x12, regs->x13, regs->x14, regs->x15);
    rt_kprintf(" a6:0x%08x   a7:0x%08x   s2:0x%08x   s3:0x%08x\r\n",
               regs->x16, regs->x17, regs->x18, regs->x19);
    rt_kprintf(" s5:0x%08x   s5:0x%08x   s6:0x%08x   s7:0x%08x\r\n",
               regs->x20, regs->x21, regs->x22, regs->x23);
    rt_kprintf(" s8:0x%08x   s9:0x%08x  s10:0x%08x  s11:0x%08x\r\n",
               regs->x24, regs->x25, regs->x26, regs->x27);
    rt_kprintf(" t3:0x%08x   t4:0x%08x   t5:0x%08x   t6:0x%08x\r\n",
               regs->x28, regs->x29, regs->x30, regs->x31);

    rt_kprintf("\r\nother:\r\n");
    rt_kprintf("mepc    :0x%08x\r\n" \
               "mstatus :0x%08x\r\n" \
               "mcratch:0x%08x\r\n" \
               "mtval   :0x%08x\r\n" \
               "mcause  :0x%08x\r\n" \
               ,regs->epc, regs->status, regs->scratch, stval, scause);
#endif
}

static unsigned long default_handle(char *str, unsigned long mcause, unsigned long mepc, unsigned long mtval, irq_regs_t *regs)
{
    printk("=====================================================================================================\r\n");
    printk("                                         %s              \r\n", str);
    printk("=====================================================================================================\r\n");
    show_register(regs, mtval, mcause);

    printk("\r\n");

#ifdef CONFIG_DEBUG_BACKTRACE
    printf("-------backtrace-----------\r\n");
    backtrace_exception(printk, regs->status, regs->x2, regs->epc, regs->x1);
    printf("---------------------------\r\n");
#endif

    void dump_system_information(void);
    dump_system_information();

    void dump_register_memory(char *name, unsigned long addr, int32_t len);

    dump_register_memory("stack",regs->x2, 128);
    dump_register_memory("epc", regs->epc, 128);

    panic_goto_cli();

    while(1);
    return 0;
}

static unsigned long error_handle(unsigned long mcause, unsigned long mepc, unsigned long mtval, irq_regs_t *regs)
{
    int i;
    int cause = mcause & ~SCAUSE_IRQ_FLAG;

    for(i = 0; i < sizeof(exception_table) / sizeof(exception_table[0]); i++)
    {
        if (exception_table[i].cause == cause && exception_table[i].error_str)
        {
            if (exception_table[i].callback)
            {
                return exception_table[i].callback(exception_table[i].error_str, mcause, mepc, mtval, regs);
            }
            else
            {
                return default_handle(exception_table[i].error_str, mcause, mepc, mtval, regs);
            }
        }
    }
    return default_handle("UNKNOWN ERROR", mcause, mepc, mtval, regs);
}

unsigned long riscv_cpu_handle_exception(unsigned long mcause, unsigned long mepc, unsigned long mtval, irq_regs_t *regs)
{
    unsigned long ret = 0;

    if (mcause & SCAUSE_IRQ_FLAG)
    {
        return default_handle("UNKNOWN ERROR", mcause, mepc, mtval, regs);
    }

    switch (mcause & ~SCAUSE_IRQ_FLAG)
    {
        case EXC_INST_MISALIGNED:
        case EXC_INST_ACCESS:
        case EXC_INST_ILLEGAL:
        case EXC_BREAKPOINT:
        case EXC_LOAD_MISALIGN:
        case EXC_LOAD_ACCESS:
        case EXC_STORE_MISALIGN:
        case EXC_STORE_ACCESS:
        case EXC_SYSCALL_FRM_U:
        case EXC_SYSCALL_FRM_M:
        case EXC_INST_PAGE_FAULT:
        case EXC_LOAD_PAGE_FAULT:
        case EXC_STORE_PAGE_FAULT:
            return error_handle(mcause, mepc, mtval, regs);
        case EXC_SYSCALL_FRM_S:
#ifdef CONFIG_SYSCALL
            __inf("ecall SYSCALL_FROM_S");
            return esSYSCALL_function(regs, NULL);
#endif
        default:
            return default_handle("UNKNOWN ERROR", mcause, mepc, mtval, regs);
    }

    return 0;
}

void trap_init(void)
{
#ifdef CONFIG_RV_MACHINE_MODE
    /*
     * Set sup0 scratch register to 0, indicating to exception vector
     * that we are presently executing in the kernel
     */
    csr_write(CSR_MSCRATCH, 0);

    /* Set the exception vector address */
    csr_write(CSR_MTVEC, &handle_exception);

    /* Enable all interrupts */
    csr_write(CSR_MIE, -1);
#else
    /*
     * Set sup0 scratch register to 0, indicating to exception vector
     * that we are presently executing in the kernel
     */
    csr_write(CSR_SSCRATCH, 0);

    /* Set the exception vector address */
    csr_write(CSR_STVEC, &handle_exception);

    /* Enable all interrupts */
    csr_write(CSR_SIE, -1);
#endif
}
