/*
 * ===========================================================================================
 *
 *       Filename:  excep.h
 *
 *    Description:  contexts definition of irq, task switch and exception.
 *
 *        Version:  Melis3.0
 *         Create:  2020-07-26 16:48:32
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-08-05 17:56:47
 *
 * ===========================================================================================
 */
#ifndef  __EXPRISCV_INC__
#define  __EXPRISCV_INC__

#ifndef __ASSEMBLY__

#include <stdint.h>
typedef struct
{
    uint32_t f[32];
    uint32_t fcsr;
} riscv_f_ext_state_t;

typedef struct
{
    uint64_t f[32];
    uint32_t fcsr;
} riscv_d_ext_state_t;

typedef struct
{
    uint64_t f[64] __attribute__((aligned(16)));
    uint32_t fcsr;
    /*
    ¦* Reserved for expansion of sigcontext structure.  Currently zeroed
    ¦* upon signal, and must be zero upon sigreturn.
    ¦*/
    uint32_t reserved[3];
} riscv_q_ext_state_t;

typedef struct
{
#ifdef CONFIG_FPU_DOUBLE
    riscv_d_ext_state_t fpustatus;
#else
    riscv_f_ext_state_t fpustatus;
#endif
} fpu_context_t;

/* CPU-specific state of a task */
typedef struct
{
    /* Callee-saved registers */
    unsigned long ra;
    unsigned long sstatus;       /* sr register the thread born */

    /*
     *array  isa  abi
     *s[ 0]: x8   s0/fp
     *s[ 1]: x9   s1
     *s[ 2]: x18  s2
     *s[ 3]: x19  s3
     *s[ 4]: x20  s4
     *s[ 5]: x21  s5
     *s[ 6]: x22  s6
     *s[ 7]: x23  s7
     *s[ 8]: x24  s8
     *s[ 9]: x25  s9
     *s[10]: x26  s10
     *s[11]: x27  s11
    */
    unsigned long s[12];    /* s[0]: frame pointer */
} __attribute__((packed)) switch_ctx_regs_t;

// zero, x2(sp), x3(tp) need not backup.
typedef struct
{
    unsigned long epc;            // 0 * __SIZEOF_LONG__
    unsigned long x1;              // 1 * __SIZEOF_LONG__
    unsigned long x2;              // 2 * __SIZEOF_LONG__
    unsigned long x3;              // 3 * __SIZEOF_LONG__
    unsigned long x4;              // 4 * __SIZEOF_LONG__
    unsigned long x5;              // 5 * __SIZEOF_LONG__
    unsigned long x6;              // 6 * __SIZEOF_LONG__
    unsigned long x7;              // 7 * __SIZEOF_LONG__
    unsigned long x8;              // 8 * __SIZEOF_LONG__
    unsigned long x9;              // 9 * __SIZEOF_LONG__
    unsigned long x10;             //10 * __SIZEOF_LONG__
    unsigned long x11;             //11 * __SIZEOF_LONG__
    unsigned long x12;             //12 * __SIZEOF_LONG__
    unsigned long x13;             //13 * __SIZEOF_LONG__
    unsigned long x14;             //14 * __SIZEOF_LONG__
    unsigned long x15;             //15 * __SIZEOF_LONG__
    unsigned long x16;             //16 * __SIZEOF_LONG__
    unsigned long x17;             //17 * __SIZEOF_LONG__
    unsigned long x18;             //18 * __SIZEOF_LONG__
    unsigned long x19;             //19 * __SIZEOF_LONG__
    unsigned long x20;             //20 * __SIZEOF_LONG__
    unsigned long x21;             //21 * __SIZEOF_LONG__
    unsigned long x22;             //22 * __SIZEOF_LONG__
    unsigned long x23;             //23 * __SIZEOF_LONG__
    unsigned long x24;             //24 * __SIZEOF_LONG__
    unsigned long x25;             //25 * __SIZEOF_LONG__
    unsigned long x26;             //26 * __SIZEOF_LONG__
    unsigned long x27;             //27 * __SIZEOF_LONG__
    unsigned long x28;             //28 * __SIZEOF_LONG__
    unsigned long x29;             //29 * __SIZEOF_LONG__
    unsigned long x30;             //30 * __SIZEOF_LONG__
    unsigned long x31;             //31 * __SIZEOF_LONG__
    unsigned long status;         //32 * __SIZEOF_LONG__
    unsigned long scratch;        //33 * __SIZEOF_LONG__
} irq_regs_t;

#endif

#ifdef CONFIG_FPU_DOUBLE
#define FPU_CTX_F0_F0   0   /* offsetof(fpu_context_t, fpustatus.f[0])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F1_F0   8   /* offsetof(fpu_context_t, fpustatus.f[1])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F2_F0   16  /* offsetof(fpu_context_t, fpustatus.f[2])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F3_F0   24  /* offsetof(fpu_context_t, fpustatus.f[3])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F4_F0   32  /* offsetof(fpu_context_t, fpustatus.f[4])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F5_F0   40  /* offsetof(fpu_context_t, fpustatus.f[5])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F6_F0   48  /* offsetof(fpu_context_t, fpustatus.f[6])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F7_F0   56  /* offsetof(fpu_context_t, fpustatus.f[7])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F8_F0   64  /* offsetof(fpu_context_t, fpustatus.f[8])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F9_F0   72  /* offsetof(fpu_context_t, fpustatus.f[9])  - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F10_F0  80  /* offsetof(fpu_context_t, fpustatus.f[10]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F11_F0  88  /* offsetof(fpu_context_t, fpustatus.f[11]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F12_F0  96  /* offsetof(fpu_context_t, fpustatus.f[12]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F13_F0  104 /* offsetof(fpu_context_t, fpustatus.f[13]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F14_F0  112 /* offsetof(fpu_context_t, fpustatus.f[14]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F15_F0  120 /* offsetof(fpu_context_t, fpustatus.f[15]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F16_F0  128 /* offsetof(fpu_context_t, fpustatus.f[16]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F17_F0  136 /* offsetof(fpu_context_t, fpustatus.f[17]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F18_F0  144 /* offsetof(fpu_context_t, fpustatus.f[18]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F19_F0  152 /* offsetof(fpu_context_t, fpustatus.f[19]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F20_F0  160 /* offsetof(fpu_context_t, fpustatus.f[20]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F21_F0  168 /* offsetof(fpu_context_t, fpustatus.f[21]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F22_F0  176 /* offsetof(fpu_context_t, fpustatus.f[22]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F23_F0  184 /* offsetof(fpu_context_t, fpustatus.f[23]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F24_F0  192 /* offsetof(fpu_context_t, fpustatus.f[24]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F25_F0  200 /* offsetof(fpu_context_t, fpustatus.f[25]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F26_F0  208 /* offsetof(fpu_context_t, fpustatus.f[26]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F27_F0  216 /* offsetof(fpu_context_t, fpustatus.f[27]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F28_F0  224 /* offsetof(fpu_context_t, fpustatus.f[28]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F29_F0  232 /* offsetof(fpu_context_t, fpustatus.f[29]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F30_F0  240 /* offsetof(fpu_context_t, fpustatus.f[30]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_F31_F0  248 /* offsetof(fpu_context_t, fpustatus.f[31]) - offsetof(fpu_context_t, fpustatus.f[0]) */
#define FPU_CTX_FCSR_F0 256 /* offsetof(fpu_context_t, fpustatus.fcsr)  - offsetof(fpu_context_t, fpustatus.f[0]) */
#else
#define FPU_CTX_F0_F0   0
#define FPU_CTX_F1_F0   4
#define FPU_CTX_F2_F0   8
#define FPU_CTX_F3_F0   12
#define FPU_CTX_F4_F0   16
#define FPU_CTX_F5_F0   20
#define FPU_CTX_F6_F0   24
#define FPU_CTX_F7_F0   28
#define FPU_CTX_F8_F0   32
#define FPU_CTX_F9_F0   36
#define FPU_CTX_F10_F0  40
#define FPU_CTX_F11_F0  44
#define FPU_CTX_F12_F0  48
#define FPU_CTX_F13_F0  52
#define FPU_CTX_F14_F0  56
#define FPU_CTX_F15_F0  60
#define FPU_CTX_F16_F0  64
#define FPU_CTX_F17_F0  68
#define FPU_CTX_F18_F0  72
#define FPU_CTX_F19_F0  76
#define FPU_CTX_F20_F0  80
#define FPU_CTX_F21_F0  84
#define FPU_CTX_F22_F0  88
#define FPU_CTX_F23_F0  92
#define FPU_CTX_F24_F0  96
#define FPU_CTX_F25_F0  100
#define FPU_CTX_F26_F0  104
#define FPU_CTX_F27_F0  108
#define FPU_CTX_F28_F0  112
#define FPU_CTX_F29_F0  116
#define FPU_CTX_F30_F0  120
#define FPU_CTX_F31_F0  124
#define FPU_CTX_FCSR_F0 128
#endif

#endif
