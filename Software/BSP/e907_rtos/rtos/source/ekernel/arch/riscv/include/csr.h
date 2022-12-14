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
#ifndef _ASM_RISCV_CSR_H
#define _ASM_RISCV_CSR_H

#include "asm.h"
#include "consts.h"

/* Status register flags */
#define SR_SIE      _AC(0x00000002, UL) /* Supervisor Interrupt Enable */
#define SR_SPIE     _AC(0x00000020, UL) /* Previous Supervisor IE */
#define SR_SPP      _AC(0x00000100, UL) /* Previously Supervisor */
#define SR_SUM      _AC(0x00040000, UL) /* Supervisor User Memory Access */

#define SR_FS       _AC(0x00006000, UL) /* Floating-point Status */
#define SR_FS_OFF   _AC(0x00000000, UL)
#define SR_FS_INITIAL   _AC(0x00002000, UL)
#define SR_FS_CLEAN _AC(0x00004000, UL)
#define SR_FS_DIRTY _AC(0x00006000, UL)

#define SR_XS       _AC(0x00018000, UL) /* Extension Status */
#define SR_XS_OFF   _AC(0x00000000, UL)
#define SR_XS_INITIAL   _AC(0x00008000, UL)
#define SR_XS_CLEAN _AC(0x00010000, UL)
#define SR_XS_DIRTY _AC(0x00018000, UL)

#define MR_MPP      _AC(0x00001800, UL) /* Previously Machine */
#define MR_MPIE     _AC(0x00000080, UL) /* Previously Machine */
#define MR_MIE      _AC(0x00000008, UL) /* Previously Machine */

#ifndef CONFIG_64BIT
#define SR_SD       _AC(0x80000000, UL) /* FS/XS dirty */
#else
#define SR_SD       _AC(0x8000000000000000, UL) /* FS/XS dirty */
#endif

/* SATP flags */
#ifndef CONFIG_64BIT
#define SATP_PPN    _AC(0x003FFFFF, UL)
#define SATP_MODE_32    _AC(0x80000000, UL)
#define SATP_MODE   SATP_MODE_32
#else
#define SATP_PPN    _AC(0x00000FFFFFFFFFFF, UL)
#define SATP_MODE_39    _AC(0x8000000000000000, UL)
#define SATP_MODE   SATP_MODE_39
#define SATP_ASID_BITS  16
#define SATP_ASID_SHIFT 44
#define SATP_ASID_MASK  _AC(0xFFFF, UL)
#endif

/* SCAUSE */
#define SCAUSE_IRQ_FLAG     (_AC(1, UL) << (__riscv_xlen - 1))

#define IRQ_U_SOFT      0
#define IRQ_S_SOFT      1
#define IRQ_M_SOFT      3
#define IRQ_U_TIMER     4
#define IRQ_S_TIMER     5
#define IRQ_M_TIMER     7
#define IRQ_U_EXT       8
#define IRQ_S_EXT       9
#define IRQ_M_EXT       11
#define IRQ_S_PMU       17

#define EXC_INST_MISALIGNED 0
#define EXC_INST_ACCESS     1
#define EXC_INST_ILLEGAL    2
#define EXC_BREAKPOINT      3
#define EXC_LOAD_MISALIGN   4
#define EXC_LOAD_ACCESS     5
#define EXC_STORE_MISALIGN  6
#define EXC_STORE_ACCESS    7
#define EXC_SYSCALL_FRM_U   8
#define EXC_SYSCALL_FRM_S   9
#define EXC_SYSCALL_FRM_M   11
#define EXC_INST_PAGE_FAULT 12
#define EXC_LOAD_PAGE_FAULT 13
#define EXC_STORE_PAGE_FAULT    15

/* SIE (Interrupt Enable) and SIP (Interrupt Pending) flags */
#define SIE_SSIE        (_AC(0x1, UL) << IRQ_S_SOFT)
#define SIE_STIE        (_AC(0x1, UL) << IRQ_S_TIMER)
#define SIE_SEIE        (_AC(0x1, UL) << IRQ_S_EXT)
#define SIE_SMIE        (_AC(0x1, UL) << IRQ_S_PMU)

/* MIE (Interrupt Enable) and MIP (Interrupt Pending) flags */
#define MIE_MSIE        (_AC(0x1, UL) << IRQ_M_SOFT)
#define MIE_MTIE        (_AC(0x1, UL) << IRQ_M_TIMER)
#define MIE_MEIE        (_AC(0x1, UL) << IRQ_M_EXT)

#define CSR_CYCLE       0xc00
#define CSR_TIME        0xc01
#define CSR_INSTRET     0xc02
#define CSR_SSTATUS     0x100
#define CSR_SIE         0x104
#define CSR_STVEC       0x105
#define CSR_SCOUNTEREN      0x106
#define CSR_SSCRATCH        0x140
#define CSR_SEPC        0x141
#define CSR_SCAUSE      0x142
#define CSR_STVAL       0x143
#define CSR_SIP         0x144
#define CSR_SATP        0x180
#define CSR_CYCLEH      0xc80
#define CSR_TIMEH       0xc81
#define CSR_INSTRETH        0xc82

#define CSR_MSTATUS     0x300U
#define CSR_MISA        0x301U
#define CSR_MEDELEG     0x302U
#define CSR_MIDELEG     0x303U
#define CSR_MIE         0x304U
#define CSR_MTVEC       0x305U
#define CSR_MSCRATCH    0x340U
#define CSR_MEPC        0x341U
#define CSR_MCAUSE      0x342U
#define CSR_MBADADDR    0x343U
#define CSR_MIP         0x344U

#define CSR_MCOR         0x7c2
#define CSR_MHCR         0x7c1
#define CSR_MCCR2        0x7c3
#define CSR_MHINT        0x7c5
#define CSR_MXSTATUS     0x7c0
#define CSR_PLIC_BASE    0xfc1
#define CSR_MRMR         0x7c6
#define CSR_MRVBR        0x7c7

#define MSTATUS_UIE         0x00000001U
#define MSTATUS_SIE         0x00000002U
#define MSTATUS_HIE         0x00000004U
#define MSTATUS_MIE         0x00000008U
#define MSTATUS_UPIE        0x00000010U
#define MSTATUS_SPIE        0x00000020U
#define MSTATUS_HPIE        0x00000040U
#define MSTATUS_MPIE        0x00000080U
#define MSTATUS_SPP         0x00000100U
#define MSTATUS_HPP         0x00000600U
#define MSTATUS_MPP         0x00001800U
#define MSTATUS_FS          0x00006000U
#define MSTATUS_XS          0x00018000U
#define MSTATUS_MPRV        0x00020000U
#define MSTATUS_PUM         0x00040000U
#define MSTATUS_MXR         0x00080000U
#define MSTATUS_VM          0x1F000000U
#define MSTATUS32_SD        0x80000000U
#define MSTATUS64_SD        0x8000000000000000U

#ifndef __ASSEMBLY__

#define csr_swap(csr, val)                  \
    ({                              \
        unsigned long __v = (unsigned long)(val);       \
        __asm__ __volatile__ ("csrrw %0, " __ASM_STR(csr) ", %1"\
                              : "=r" (__v) : "rK" (__v)     \
                              : "memory");          \
        __v;                            \
    })

#define csr_read(csr)                       \
    ({                              \
        register unsigned long __v;             \
        __asm__ __volatile__ ("csrr %0, " __ASM_STR(csr)    \
                              : "=r" (__v) :            \
                              : "memory");          \
        __v;                            \
    })

#define csr_write(csr, val)                 \
    ({                              \
        unsigned long __v = (unsigned long)(val);       \
        __asm__ __volatile__ ("csrw " __ASM_STR(csr) ", %0" \
                              : : "rK" (__v)            \
                              : "memory");          \
    })

#define csr_read_set(csr, val)                  \
    ({                              \
        unsigned long __v = (unsigned long)(val);       \
        __asm__ __volatile__ ("csrrs %0, " __ASM_STR(csr) ", %1"\
                              : "=r" (__v) : "rK" (__v)     \
                              : "memory");          \
        __v;                            \
    })

#define csr_set(csr, val)                   \
    ({                              \
        unsigned long __v = (unsigned long)(val);       \
        __asm__ __volatile__ ("csrs " __ASM_STR(csr) ", %0" \
                              : : "rK" (__v)            \
                              : "memory");          \
    })

#define csr_read_clear(csr, val)                \
    ({                              \
        unsigned long __v = (unsigned long)(val);       \
        __asm__ __volatile__ ("csrrc %0, " __ASM_STR(csr) ", %1"\
                              : "=r" (__v) : "rK" (__v)     \
                              : "memory");          \
        __v;                            \
    })

#define csr_clear(csr, val)                 \
    ({                              \
        unsigned long __v = (unsigned long)(val);       \
        __asm__ __volatile__ ("csrc " __ASM_STR(csr) ", %0" \
                              : : "rK" (__v)            \
                              : "memory");          \
    })

#endif /* __ASSEMBLY__ */

#endif /* _ASM_RISCV_CSR_H */
