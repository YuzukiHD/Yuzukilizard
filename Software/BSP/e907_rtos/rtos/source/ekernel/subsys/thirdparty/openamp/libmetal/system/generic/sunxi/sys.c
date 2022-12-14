/*
 * Copyright (c) 2018, Linaro Inc. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file	generic/template/sys.c
 * @brief	machine specific system primitives implementation.
 */

#include <metal/io.h>
#include <metal/sys.h>
#include <metal/utilities.h>
#include <stdint.h>

#include <hal_interrupt.h>
#include <hal_cache.h>
#include <hal_timer.h>

#include <openamp/sunxi_helper/openamp_log.h>

void sys_irq_restore_enable(unsigned int flags)
{
	hal_interrupt_restore((uint32_t)flags);
}

unsigned int sys_irq_save_disable(void)
{
	return (unsigned int)hal_interrupt_save();
}

void sys_irq_enable(unsigned int vector)
{
	hal_enable_irq(vector);
}

void sys_irq_disable(unsigned int vector)
{
	hal_disable_irq(vector);
}

void metal_machine_cache_flush(void *addr, unsigned int len)
{
	if(!addr && !len) {
		openamp_dbg("flush all\r\n");
		int backtrace(char *taskname, void *output[], int size, int offset, void *print);
		backtrace(NULL, NULL, 0, 0, printk);
		return;
	} else {
		hal_dcache_clean((unsigned long)addr, (unsigned long)len);
	}
}

void metal_machine_cache_invalidate(void *addr, unsigned int len)
{
	if(!addr && !len) {
		openamp_dbg("invalid all\r\n");
		int backtrace(char *taskname, void *output[], int size, int offset, void *print);
		backtrace(NULL, NULL, 0, 0, printk);
		while(1);
		return;
	} else {
		hal_dcache_invalidate((unsigned long)addr, (unsigned long)len);
	}
}

void metal_generic_default_poll(void)
{
	hal_msleep(1);
}

void *metal_machine_io_mem_map(void *va, metal_phys_addr_t pa,
			       size_t size, unsigned int flags)
{
	metal_unused(pa);
	metal_unused(size);
	metal_unused(flags);

	/* Add implementation here */

	return va;
}
