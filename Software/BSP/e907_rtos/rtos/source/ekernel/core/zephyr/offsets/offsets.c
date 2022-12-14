/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ARM kernel structure member offset definition file
 *
 * This module is responsible for the generation of the absolute symbols whose
 * value represents the member offsets for various ARM kernel structures.
 *
 * All of the absolute symbols defined by this module will be present in the
 * final kernel ELF image (due to the linker's reference to the _OffsetAbsSyms
 * symbol).
 *
 * INTERNAL
 * It is NOT necessary to define the offset for every member of a structure.
 * Typically, only those members that are accessed by assembly language routines
 * are defined; however, it doesn't hurt to define all fields for the sake of
 * completeness.
 */

#include <gen_offset.h>
#include <kernel_structs.h>
#include <kernel_offsets.h>

GEN_OFFSET_SYM(_thread_arch_t, swap_return_value);
#ifdef CONFIG_FLOAT
GEN_OFFSET_SYM(_thread_arch_t, preempt_float);
#endif

GEN_ABSOLUTE_SYM(_thread_arch_t_SIZEOF, sizeof(_thread_arch_t));

GEN_OFFSET_SYM(_thread_t, base);
GEN_OFFSET_SYM(_thread_t, caller_saved);
GEN_OFFSET_SYM(_thread_t, callee_saved);
GEN_OFFSET_SYM(_thread_t, init_data);
GEN_OFFSET_SYM(_thread_t, fn_abort);
GEN_OFFSET_SYM(_thread_t, arch);

GEN_ABSOLUTE_SYM(_thread_t_SIZEOF, sizeof(_thread_t));

GEN_OFFSET_SYM(_esf_t, ctxid);
GEN_OFFSET_SYM(_esf_t, spsr);
GEN_OFFSET_SYM(_esf_t, r00);
GEN_OFFSET_SYM(_esf_t, r01);
GEN_OFFSET_SYM(_esf_t, r02);
GEN_OFFSET_SYM(_esf_t, r03);
GEN_OFFSET_SYM(_esf_t, r04);
GEN_OFFSET_SYM(_esf_t, r05);
GEN_OFFSET_SYM(_esf_t, r06);
GEN_OFFSET_SYM(_esf_t, r07);
GEN_OFFSET_SYM(_esf_t, r08);
GEN_OFFSET_SYM(_esf_t, r09);
GEN_OFFSET_SYM(_esf_t, r10);
GEN_OFFSET_SYM(_esf_t, r11);
GEN_OFFSET_SYM(_esf_t, ip);
GEN_OFFSET_SYM(_esf_t, pc);

GEN_ABSOLUTE_SYM(___esf_t_SIZEOF, sizeof(_esf_t));

GEN_OFFSET_SYM(_callee_saved_t, r04);
GEN_OFFSET_SYM(_callee_saved_t, r05);
GEN_OFFSET_SYM(_callee_saved_t, r06);
GEN_OFFSET_SYM(_callee_saved_t, r07);
GEN_OFFSET_SYM(_callee_saved_t, r08);
GEN_OFFSET_SYM(_callee_saved_t, r09);
GEN_OFFSET_SYM(_callee_saved_t, r10);
GEN_OFFSET_SYM(_callee_saved_t, r11);
GEN_OFFSET_SYM(_callee_saved_t, sp);

/* size of the entire preempt registers structure */

GEN_ABSOLUTE_SYM(___callee_saved_t_SIZEOF, sizeof(struct _callee_saved));

GEN_OFFSET_SYM(_caller_saved_t, r00);
GEN_OFFSET_SYM(_caller_saved_t, r01);
GEN_OFFSET_SYM(_caller_saved_t, r02);
GEN_OFFSET_SYM(_caller_saved_t, r03);
GEN_OFFSET_SYM(_caller_saved_t, ip);
GEN_OFFSET_SYM(_caller_saved_t, lr);
GEN_OFFSET_SYM(_caller_saved_t, pc);
GEN_OFFSET_SYM(_caller_saved_t, spsr);

GEN_ABSOLUTE_SYM(___caller_saved_t_SIZEOF, sizeof(struct _caller_saved));

/*
 * size of the struct k_thread structure sans save area for floating
 * point registers.
 */

#ifdef CONFIG_FLOAT
GEN_ABSOLUTE_SYM(_K_THREAD_NO_FLOAT_SIZEOF, sizeof(struct k_thread) -
                 sizeof(struct _preempt_float));
#else
GEN_ABSOLUTE_SYM(_K_THREAD_NO_FLOAT_SIZEOF, sizeof(struct k_thread));
#endif

GEN_ABS_SYM_END
