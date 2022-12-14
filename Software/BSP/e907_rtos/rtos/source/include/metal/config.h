/*
 * Copyright (c) 2015, Xilinx Inc. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file	config.h
 * @brief	Generated configuration settings for libmetal.
 */

#ifndef __METAL_CONFIG__H__
#define __METAL_CONFIG__H__

#ifdef __cplusplus
extern "C" {
#endif

/** Library major version number. */
#define METAL_VER_MAJOR		1

/** Library minor version number. */
#define METAL_VER_MINOR		0

/** Library patch level. */
#define METAL_VER_PATCH		0

/** Library version string. */
#define METAL_VER		"1.0.0"

/** System type (linux, generic, ...). */
#define METAL_SYSTEM		"generic"
#define METAL_SYSTEM_GENERIC

/** Processor type (arm, x86_64, ...). */
#define METAL_PROCESSOR		"riscv"
#define METAL_PROCESSOR_RISCV

/** Machine type (zynq, zynqmp, ...). */
#define METAL_MACHINE		"sunxi"
#define METAL_MACHINE_SUNXI

/* #undef HAVE_STDATOMIC_H */
/* #undef HAVE_FUTEX_H */

#ifdef __cplusplus
}
#endif

#endif /* __METAL_CONFIG__H__ */
