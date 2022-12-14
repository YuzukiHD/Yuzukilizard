/*
 * =====================================================================================
 *
 *       Filename:  arch.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2020年06月08日 17时45分40秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zeng Zhijin
 *   Organization:
 *
 * =====================================================================================
 */

#ifndef ARCH_H
#define ARCH_H

#include <stdint.h>

int irqs_disabled(void);
/* ----------------------------------------------------------------------------*/
/**
 * @brief  __get_cpsr  Get cpsr register.
 *
 * @return  value of cpsr register.
 */
/* ----------------------------------------------------------------------------*/
static inline unsigned long __get_cpsr(void)
{
    return 0;
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_lock_irq disable the interrupt response capacity of processor.
 *
 * @return
 *   - old value of processor status register.
 */
/* ----------------------------------------------------------------------------*/
unsigned long  awos_arch_lock_irq(void);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_unlock_irq restore the former value of status cpu register.
 *
 * @param[in] cpu_sr, former status register value needed to restore.
 */
/* ----------------------------------------------------------------------------*/

void awos_arch_unlock_irq(unsigned long cpu_sr);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_set_vector_base <set new vector address>
 *
 * @param[in] newvector: new vector table address.
 * @attention: only ARM11 and cortex-a can modify the vector address.
 *             ARM7,ARM9,ARM10 only fixed at 0x0 or 0xffff0000.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_set_vector_base(uint32_t newvector);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_first_task_start start the operation system scheduler, and
 *         switch to the first prepare ready task, usually, the bringup thread.
 *
 * @param[in] to, the stack pointer of the next task switching into.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_first_task_start(unsigned long to);


/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_task_switch  switch thread from thread from(sp) to to(sp).
 *         thread "to" context would be restored and to deploy and run.
 *
 * @param[in] from, the stack pointer of the task going to be swtch out.
 * @param[in] to, the stack pointer of the task going to be switch into.
 * return:
 *   0: return from task context switch
 *   1: return from interrupt context switch
 */
/* ----------------------------------------------------------------------------*/
unsigned long awos_arch_task_switch(unsigned long from, unsigned long to);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_task_switch_inirq try to switch thread on interrupt context.
 *
 * @param[in] from, the stack pointer of the task going to be swtch out.
 * @param[in] to, the stack pointer of the task going to be switch into.
 *
 * @attention This function did not do the actual switch operation on irq context
 *            just setup flags, and swith when going out of the irq context.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_task_switch_inirq(uint32_t from, uint32_t to);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_dbg_task_switch_to  for debug only, used to investigate speci-
 *         fy task status.
 *
 * @param[in] to, the stack pointer of task you want to switch into.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_dbg_task_switch_to(uint32_t to);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_test_clean_flush_cache, test, clean, and flush cache op.
 *  @attention  refer arm926ej-s processor arch spec for detail.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_test_clean_flush_cache(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_clean_dcache <clean dcache op.>
 *  @attention refer arm926ej=s processor arch spec for detail.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_clean_dcache(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_clean_flush_dcache <flush dcache op.>
 *  @attention refer arm926ej=s processor arch spec for detail.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_clean_flush_dcache(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_clean_flush_cache <clean flush cache>
 *  @attention refer arm926ej=s processor arch spec for detail.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_clean_flush_cache(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_flush_icache, flush i cache op.
 *  @attention refer arm926ej-s processor arch spec for detail.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_flush_icache(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_flush_dcache <flush_dcache op>
 *  @attention refer arm926ej-s processor arch spec for detail.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_flush_dcache(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_flush_cache <flush cache region>
 *  @attention refer arm926ej-s processor arch spec for detail.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_flush_cache(void);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_mems_flush_icache_region  flush icache data to physical
 * memory.
 *
 * @param[in] addr, start address of memory block.
 * @param[in] bytes, size of memory block need to be flushed, by BYTE.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_mems_flush_icache_region(void *addr, uint32_t bytes);

/* ----------------------------------------------------------------------------*/
/**
 * @brief awos_arch_mems_flush_dcache_region flush dcache data to physical memory.
 *
 * @param[in] addr, start address of memory block.
 * @param[in] bytes size of memory block need to be flushed, by BYTE.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_mems_flush_dcache_region(unsigned long addr, unsigned long bytes);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_mems_flush_cache_region flush cache data to physical memory.
 *
 * @param[in] addr, start address of memory block.
 * @param[in] bytes, sz of memory block, by BYTE.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_mems_flush_cache_region(void *addr, uint32_t bytes);

/* ----------------------------------------------------------------------------*/
/**
 * @brief awos_arch_mems_clean_dcache_region clean dcache region. refer arm926ej-s
 *         arch spec.
 *
 * @param[in] addr, start address of memory block.
 * @param[in] bytes, size of memory block, by BYTE.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_mems_clean_dcache_region(unsigned long vaddr_addr, unsigned long bytes);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_mems_clean_flush_dcache_region clean, flush dcache region.
 *
 * @param[in] addr, start address of memory block need to be operated.
 * @param[in] bytes, size of memory block, by BYTE.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_mems_clean_flush_dcache_region(unsigned long addr, unsigned long bytes);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_mems_clean_flush_cache_region  clean, flush cache region.
 *
 * @param[in] addr, start address of memory block need to be operated.
 * @param[in] bytes, size of memory block.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_mems_clean_flush_cache_region(void *addr, uint32_t bytes);
void cpu_arm926_cache_clean_invalidate_all(void);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_lock_tlb_zone lock the page table items in tle, not swap out.
 *
 * @param[in] addr, virtual address which you want to lock its page table entry.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_lock_tlb_zone(uint32_t addr);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_chksched_start, get the status of operation system scheduler.
 *
 * @return
 *   -0: the scheduler has not been started.
 *   -1: the scheduler has been started.
 */
/* ----------------------------------------------------------------------------*/
uint8_t awos_arch_chksched_start(void);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_bus_to_virt  ioremap operation, Map io Registers to Virtual.
 *
 * @param[in] phyaddr, IO register address want mapped.
 *
 * @return the virtual address that can be accessed by software of IO registers.
 */
/* ----------------------------------------------------------------------------*/
uint32_t awos_arch_bus_to_virt(uint32_t phyaddr);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_phys_to_virt map physical memory address to virtual address.
 *
 * @param[in] phyaddr, physcal memory address.
 *
 * @return  the virtual address of specify physical address.
 */
/* ----------------------------------------------------------------------------*/
unsigned long awos_arch_phys_to_virt(unsigned long phyaddr);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_vmem_create  create virtual memory area through MMU.
 *
 * @param[in] virtaddr, the area start virtual address want create.
 * @param[in] npage, page number of the area, 4K.
 * @param[in] domain, previlege level of the area.
 *
 * @return
 *   -0: success.
 *   -1: failure.
 */
/* ----------------------------------------------------------------------------*/
int32_t awos_arch_vmem_create(uint8_t *virtaddr, uint32_t npage, uint8_t domain);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_vmem_delete destory the virtual area created.
 *
 * @param[in] virtaddr, start address of the virtual zone.
 * @param[in] npage, page number want to delete successive.
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_vmem_delete(uint8_t *virtaddr, uint32_t npage);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_vaddr_map2_phys  virtual to physical address convert, especi-
 *         ally for the DMA Controller and VE, GE etc.
 *
 * @param[in] vaddr, virtual address you want to convert.
 *
 * @return  the physical address of specify virtual address after mmu transition.
 */
/* ----------------------------------------------------------------------------*/
unsigned long awos_arch_vaddr_map2_phys(unsigned long vaddr);

/* ----------------------------------------------------------------------------*/
/**
 * @brief awos_arch_set_fsce_id set the fast context extension id. refer arm926ej-s
 *         arch spec for detail.
 *
 * @param[in] fsceid id you want to set to CP15 id you want to set to CP15
 *
 * @return  the fsceid has been set.
 */
/* ----------------------------------------------------------------------------*/
uint8_t awos_arch_set_fsce_id(uint8_t fsceid);

/* ----------------------------------------------------------------------------*/
/** @brief  aw_hal_enable_vfp enable vfp(vector floating point, fpu) function.
 *  @attention not support SUN3IW2 based on arm926ej-s processor.
 */
/* ----------------------------------------------------------------------------*/
void aw_hal_enable_vfp(void);

/* ----------------------------------------------------------------------------*/
/** @brief  jtag_enable_op enable jtag pin into jtag mode.
 */
/* ----------------------------------------------------------------------------*/
void jtag_enable_op(void);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_stack_init This function will initialize thread stack.
 *
 * @param[in] tentry  the entry of thread
 * @param[in] parameter the parameter of entry
 * @param[in] stack_addr the beginning of the thread stack.
 * @param[in] texit the function will be called when thread entry return.
 *
 * @return  the stack pointer with the full context.
 */
/* ----------------------------------------------------------------------------*/
uint8_t *awos_arch_stack_init(void *tentry, void *parameter, uint8_t *stack_addr, \
                              void *texit);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_ffs This function finds the first bit set(beginning with the least
 *         significant bit) in value and reeturn the index of the bit.
 *
 * @param[in] value the value you want to operat __ffs on .
 *
 * @return
 *   -0: the argument was zero.
 *   -x: the first bit position starting from 1.
 */
/* ----------------------------------------------------------------------------*/
int32_t awos_arch_ffs(int32_t value);

/**
 * @brief  awos_arch_swi_func_error, wrong syscall report.
 *
 * @param[in] spsr
 * @param[in] cpsr
 * @param[in] swino
 * @param[in] swihander
 * @param[in] caller_lr
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_swi_func_error(uint32_t spsr, uint32_t cpsr, uint32_t swino, \
                              uint32_t swihander, uint32_t caller_lr);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_swi_mode_error <wrong cpu mode happed>
 *
 * @param spsr
 * @param cpsr
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_swi_mode_error(uint32_t spsr, uint32_t cpsr);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_fiq_trap_entry <FIQ not support.> */
/* ----------------------------------------------------------------------------*/
void awos_arch_fiq_trap_entry(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_rst_trap_entry <reset mode> */
/* ----------------------------------------------------------------------------*/
void awos_arch_rst_trap_entry(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_rsv_trap_entry <reserved exception, should never happend> */
/* ----------------------------------------------------------------------------*/
void awos_arch_rsv_trap_entry(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_irq_trap_enter < implementation by OS vendor.> */
/* ----------------------------------------------------------------------------*/
void awos_arch_irq_trap_enter(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_irq_trap_leave <implementation by OS vendor> */
/* ----------------------------------------------------------------------------*/
void awos_arch_irq_trap_leave(void);

/* ----------------------------------------------------------------------------*/
/** @brief  awos_arch_tick_increase <++> */
/* ----------------------------------------------------------------------------*/
void awos_arch_tick_increase(void);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_isin_irq <++>
 *
 * @return
 */
/* ----------------------------------------------------------------------------*/
uint8_t awos_arch_isin_irq(void);

/* ----------------------------------------------------------------------------*/
/**
 * @brief  awos_arch_hal_uart_send <++>
 *
 * @param str
 */
/* ----------------------------------------------------------------------------*/
void awos_arch_hal_uart_send(const char *str);

void cpu_do_idle(void);
//the two interface are same, running on differenct address.
void awos_arch_dcache_clean_flush(void);
void awos_arch_dcache_clean_flush_all(void);

//the two interface are same, running on differenct address.
void awos_arch_icache_flush(void);
void awos_arch_dcache_disable(void);
void awos_arch_icache_disable(void);
void awos_arch_tlb_set(uint32_t tlb_base);
void awos_arch_mmu_enable(void);
void awos_arch_mmu_disable(void);
void awos_arch_icache_enable(void);
void awos_arch_dcache_enable(void);
void awos_arch_tlb_invalidate_all(void);
void awos_arch_dcache_clean_area(void *start, uint32_t size);
void awos_arch_flush_kern_cache_all(void);
void awos_arch_flush_icache_all(void);
void awos_arch_flush_kern_tlb_range(uint32_t start, uint32_t end);
void awos_arch_fpu_init(void);
void awos_arch_mems_dma_inv_range(void *start, void *end);

#define arch_break(...) do {                    \
        asm volatile ("ebreak": : :"memory");   \
    } while(0)

#define CPSR_ALLOC() unsigned long __cpsr
#define MELIS_CPU_CRITICAL_ENTER() { __cpsr = awos_arch_lock_irq(); }
#define MELIS_CPU_CRITICAL_LEAVE() { awos_arch_unlock_irq(__cpsr);  }

#endif  /*ARCH_H*/
