/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Cortex-M public exception handling
 *
 * ARM-specific kernel exception handling interface. Included by arm/arch.h.
 */

#ifndef _ARCH_ARM_CORTEXM_EXC_H_
#define _ARCH_ARM_CORTEXM_EXC_H_

#ifdef __cplusplus
extern "C" {
#endif

/* for assembler, only works with constants */
#define _EXC_PRIO(pri) (((pri) << (8 - CONFIG_NUM_IRQ_PRIO_BITS)) & 0xff)

#ifdef CONFIG_ZERO_LATENCY_IRQS
#define _ZERO_LATENCY_IRQS_RESERVED_PRIO 1
#else
#define _ZERO_LATENCY_IRQS_RESERVED_PRIO 0
#endif

#if defined(CONFIG_CPU_CORTEX_M_HAS_PROGRAMMABLE_FAULT_PRIOS) || \
    defined(CONFIG_CPU_CORTEX_M_HAS_BASEPRI)
#define _EXCEPTION_RESERVED_PRIO 1
#else
#define _EXCEPTION_RESERVED_PRIO 0
#endif

#define _IRQ_PRIO_OFFSET \
    (_ZERO_LATENCY_IRQS_RESERVED_PRIO + \
     _EXCEPTION_RESERVED_PRIO)

#define _EXC_IRQ_DEFAULT_PRIO _EXC_PRIO(_IRQ_PRIO_OFFSET)

#ifdef _ASMLANGUAGE
GTEXT(_ExcExit);
#else
#include <zephyr/types.h>

struct __esf
{
    /* exception frame registers */
    u32_t    ctxid;    /* fcseid */
    u32_t    spsr;     /* spsr */
    u32_t    r04;      /* r4 */
    u32_t    r05;      /* r5 */
    u32_t    r06;      /* r6 */
    u32_t    r07;      /* r7 */
    u32_t    r08;      /* r8 */
    u32_t    r09;      /* r9 */
    u32_t    r10;      /* r10 */
    u32_t    r11;      /* r11 */
    u32_t    ip;       /* r12 */
    u32_t    pc;       /* r15 */
    u32_t    r00;      /* r0, parameter0 */
    u32_t    r01;      /* r1, parameter1 */
    u32_t    r02;      /* r2, parameter2 */
    u32_t    r03;      /* r3, parameter3 */
};

typedef struct __esf NANO_ESF;

extern void _ExcExit(void);

/**
 * @brief display the contents of a exception stack frame
 *
 * @return N/A
 */

extern void sys_exc_esf_dump(NANO_ESF *esf);

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* _ARCH_ARM_CORTEXM_EXC_H_ */
