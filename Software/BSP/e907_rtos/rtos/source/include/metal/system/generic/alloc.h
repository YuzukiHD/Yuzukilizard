/*
 * Copyright (c) 2016, Xilinx Inc. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file	generic/alloc.c
 * @brief	generic libmetal memory allocattion definitions.
 */

#ifndef __METAL_ALLOC__H__
#error "Include metal/alloc.h instead of metal/generic/alloc.h"
#endif

#ifndef __METAL_GENERIC_ALLOC__H__
#define __METAL_GENERIC_ALLOC__H__

#include <stdlib.h>
#include <hal_mem.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void *metal_allocate_memory(unsigned int size)
{
	return hal_malloc(size);
}

static inline void metal_free_memory(void *ptr)
{
	hal_free(ptr);
}

#ifdef __cplusplus
}
#endif

#endif /* __METAL_GENERIC_ALLOC__H__ */
