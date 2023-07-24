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
#include <cpuport.h>

/* Enable interrupts when returning from the handler */
#define MSTATUS_PRV1 0x1880

.section .bss
    .align  2
    .globl  g_trapstackalloc
    .global g_trapstackbase
    .global g_top_trapstack
g_trapstackalloc:
g_trapstackbase:
    .space 768
g_top_trapstack:

    .align 2
    .globl g_trap_sp
    .type  g_trap_sp, object
g_trap_sp:
    .long 0
    .size g_trap_sp, .-g_trap_sp

    .globl  g_irq_stack_base
    .global g_irq_stack_top
g_irq_stack_base:
    .space 16 * 1024
g_irq_stack_top:
    .type  irq_ori_sp, object
irq_ori_sp:
.long 0
    .size irq_ori_sp, .-irq_ori_sp

.text

    .align  2
    .global Default_IRQHandler
    .weak   Default_IRQHandler
    .type   Default_IRQHandler, %function
Default_IRQHandler:
    ipush

    li    a0, 0x8
    csrc  mstatus, a0

#ifdef CONFIG_ARCH_RISCV_FPU
    csrr  a0, mstatus
    call  fpu_save_inirq
#endif
    /* switch to IRQ stack */
    la      t1, irq_ori_sp
    sw      sp, 0(t1)
    la      sp, g_irq_stack_top

    csrr    t1, mcause
    andi    t1, t1, 0x3FF
    slli    t1, t1, 2
    la      t0, g_irqvector
    add     t0, t0, t1
    lw      t2, (t0)
    jalr    t2

    li      t0, MSTATUS_PRV1
    csrs    mstatus, t0

#    li   a0, 0x1880
#    csrw mstatus, a0

    /* switch to origin stack */
    la      t1, irq_ori_sp
    lw      sp, 0(t1)

#ifdef CONFIG_ARCH_RISCV_FPU
    csrr a0, mstatus
    call fpu_restore_inirq
#endif

    jal  schedule_preempt_inirq

    li    a0, 0x8
    csrs  mstatus, a0

    ipop

/******************************************************************************
 * Functions:
 *     void trap(void);
 * default exception handler
 ******************************************************************************/
    .align  2
    .global trap
    .type   trap, %function
trap:
    /* Check for interrupt */
    addi    sp, sp, -4
    sw      t0, 0x0(sp)
    csrr    t0, mcause
    blt     t0, x0, .Lirq
    addi    sp, sp, 4
    la      t0, g_trap_sp

    addi    t0, t0, -132

    sw      x1,    0(t0)
    sw      x2,    4(t0)
    sw      x3,    8(t0)
    sw      x4,   12(t0)
    sw      x6,   20(t0)
    sw      x7,   24(t0)
    sw      x8,   28(t0)
    sw      x9,   32(t0)
    sw      x10,  36(t0)
    sw      x11,  40(t0)
    sw      x12,  44(t0)
    sw      x13,  48(t0)
    sw      x14,  52(t0)
    sw      x15,  56(t0)
    sw      x16,  60(t0)
    sw      x17,  64(t0)
    sw      x18,  68(t0)
    sw      x19,  72(t0)
    sw      x20,  76(t0)
    sw      x21,  80(t0)
    sw      x22,  84(t0)
    sw      x23,  88(t0)
    sw      x24,  92(t0)
    sw      x25,  96(t0)
    sw      x26, 100(t0)
    sw      x27, 104(t0)
    sw      x28, 108(t0)
    sw      x29, 112(t0)
    sw      x30, 116(t0)
    sw      x31, 120(t0)

    csrr    a0, mepc
    sw      a0, 124(t0)
    csrr    a0, mstatus
    sw      a0, 128(t0)

    mv      a0, t0
    lw      t0, -4(sp)
    mv      sp, a0
    sw      t0, 16(sp)

    jal     trap_c


.Lirq:
    lw      t0, 0x0(sp)
    addi    sp, sp, 4
    j       Default_IRQHandler

    .align  6
    .weak   Default_Handler
    .global Default_Handler
    .type   Default_Handler, %function
Default_Handler:
    /* Check for nmi */
    addi    sp, sp, -8
    sw      t0, 0x0(sp)
    sw      t1, 0x4(sp)
    csrr    t0, mcause
    andi    t0, t0, 0x3FF
    li      t1, 24
    beq     t0, t1, .NMI_Handler
    lw      t0, 0x0(sp)
    lw      t1, 0x4(sp)
    addi    sp, sp, 8
    j       trap

.NMI_Handler:
    lw      t0, 0x0(sp)
    lw      t1, 0x4(sp)
    addi    sp, sp, 8

    addi    sp, sp, -64

    sw      ra,  0(sp)
    sw      t0,  4(sp)
    sw      t1,  8(sp)
    sw      t2, 12(sp)
    sw      a0, 16(sp)
    sw      a1, 20(sp)
    sw      a2, 24(sp)
    sw      a3, 28(sp)
    sw      a4, 32(sp)
    sw      a5, 36(sp)
    sw      a6, 40(sp)
    sw      a7, 44(sp)
    sw      t3, 48(sp)
    sw      t4, 52(sp)
    sw      t5, 56(sp)
    sw      t6, 60(sp)

#ifdef CONFIG_ARCH_RISCV_FPU
    addi    sp,    sp, -(20*FREGBYTES)
    FSTORE  ft0,   0 * FREGBYTES(sp)
    FSTORE  ft1,   1 * FREGBYTES(sp)
    FSTORE  ft2,   2 * FREGBYTES(sp)
    FSTORE  ft3,   3 * FREGBYTES(sp)
    FSTORE  ft4,   4 * FREGBYTES(sp)
    FSTORE  ft5,   5 * FREGBYTES(sp)
    FSTORE  ft6,   6 * FREGBYTES(sp)
    FSTORE  ft7,   7 * FREGBYTES(sp)
    FSTORE  fa0,   8 * FREGBYTES(sp)
    FSTORE  fa1,   9 * FREGBYTES(sp)
    FSTORE  fa2,  10 * FREGBYTES(sp)
    FSTORE  fa3,  11 * FREGBYTES(sp)
    FSTORE  fa4,  12 * FREGBYTES(sp)
    FSTORE  fa5,  13 * FREGBYTES(sp)
    FSTORE  fa6,  14 * FREGBYTES(sp)
    FSTORE  fa7,  15 * FREGBYTES(sp)
    FSTORE  ft8,  16 * FREGBYTES(sp)
    FSTORE  ft9,  17 * FREGBYTES(sp)
    FSTORE  ft10, 18 * FREGBYTES(sp)
    FSTORE  ft11, 19 * FREGBYTES(sp)
#endif

    la      t0, g_nmivector
    lw      t0, (t0)
    jalr    t0

#ifdef CONFIG_ARCH_RISCV_FPU
    FLOAD   ft0,   0 * FREGBYTES(sp)
    FLOAD   ft1,   1 * FREGBYTES(sp)
    FLOAD   ft2,   2 * FREGBYTES(sp)
    FLOAD   ft3,   3 * FREGBYTES(sp)
    FLOAD   ft4,   4 * FREGBYTES(sp)
    FLOAD   ft5,   5 * FREGBYTES(sp)
    FLOAD   ft6,   6 * FREGBYTES(sp)
    FLOAD   ft7,   7 * FREGBYTES(sp)
    FLOAD   fa0,   8 * FREGBYTES(sp)
    FLOAD   fa1,   9 * FREGBYTES(sp)
    FLOAD   fa2,  10 * FREGBYTES(sp)
    FLOAD   fa3,  11 * FREGBYTES(sp)
    FLOAD   fa4,  12 * FREGBYTES(sp)
    FLOAD   fa5,  13 * FREGBYTES(sp)
    FLOAD   fa6,  14 * FREGBYTES(sp)
    FLOAD   fa7,  15 * FREGBYTES(sp)
    FLOAD   ft8,  16 * FREGBYTES(sp)
    FLOAD   ft9,  17 * FREGBYTES(sp)
    FLOAD   ft10, 18 * FREGBYTES(sp)
    FLOAD   ft11, 19 * FREGBYTES(sp)

    addi    sp, sp, (20 * FREGBYTES)
#endif

    lw      ra,  0(sp)
    lw      t0,  4(sp)
    lw      t1,  8(sp)
    lw      t2, 12(sp)
    lw      a0, 16(sp)
    lw      a1, 20(sp)
    lw      a2, 24(sp)
    lw      a3, 28(sp)
    lw      a4, 32(sp)
    lw      a5, 36(sp)
    lw      a6, 40(sp)
    lw      a7, 44(sp)
    lw      t3, 48(sp)
    lw      t4, 52(sp)
    lw      t5, 56(sp)
    lw      t6, 60(sp)

    addi    sp, sp, 64
    mret


    .size   Default_Handler, . - Default_Handler

/*    Macro to define default handlers. Default handler
 *    will be weak symbol and just dead loops. They can be
 *    overwritten by other handlers */
    .macro  def_irq_handler handler_name
    .weak   \handler_name
    .globl  \handler_name
    .set    \handler_name, Default_Handler
    .endm

    def_irq_handler PendSV_Handler
    def_irq_handler SysTick_Handler
    def_irq_handler STIM0_IRQHandler
    def_irq_handler STIM1_IRQHandler
    def_irq_handler STIM2_IRQHandler
    def_irq_handler STIM3_IRQHandler
    def_irq_handler TIM0_IRQHandler
    def_irq_handler TIM1_IRQHandler
    def_irq_handler TIM2_IRQHandler
    def_irq_handler TIM3_IRQHandler
    def_irq_handler USART_IRQHandler
    def_irq_handler GPIO0_IRQHandler
    def_irq_handler GPIO1_IRQHandler
    def_irq_handler GPIO2_IRQHandler
    def_irq_handler GPIO3_IRQHandler
    def_irq_handler GPIO4_IRQHandler
    def_irq_handler GPIO5_IRQHandler
    def_irq_handler GPIO6_IRQHandler
    def_irq_handler GPIO7_IRQHandler
    def_irq_handler PAD_IRQHandler
    def_irq_handler TIM6_IRQHandler
    def_irq_handler TIM7_IRQHandler
    def_irq_handler TIM8_IRQHandler
    def_irq_handler TIM9_IRQHandler
    def_irq_handler TIM10_IRQHandler
    def_irq_handler TIM11_IRQHandler

