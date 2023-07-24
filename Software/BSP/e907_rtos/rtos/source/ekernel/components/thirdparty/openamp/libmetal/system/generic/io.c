/*
 * Copyright (c) 2017, Xilinx Inc. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file	generic/io.c
 * @brief	Generic libmetal io operations
 */

#include <metal/io.h>
#include <openamp/sunxi_helper/openamp_log.h>

void metal_sys_io_mem_map(struct metal_io_region *io)
{
	unsigned long p;
	size_t psize;
	size_t *va;

	va = io->virt;
	psize = (size_t)io->size;

	openamp_dbg("Start map io region.\r\n");
	openamp_dbg("io->ops addr:0x%p\r\n", io->ops.block_write);

	if (psize) {
		if (psize >> io->page_shift)
			psize = (size_t)1 << io->page_shift;
		for (p = 0; p <= (io->size >> io->page_shift); p++) {
			openamp_dbg("\tmap 0x%08lx to 0x%08lx; size 0x%lx\r\n", io->physmap[p], va, psize);
			metal_machine_io_mem_map(va, io->physmap[p],
						 psize, io->mem_flags);
			va += psize;
		}
	}
	openamp_dbg("End map io region.\r\n");
}
