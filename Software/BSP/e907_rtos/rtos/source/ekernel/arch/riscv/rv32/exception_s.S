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
.section .text.entry, "ax", @progbits
.align 8
.option norvc
.global handle_exception
handle_exception:
    # save interrupted context to current thread stack
    addi sp, sp, -34 * 4

    sw   x1,   1 * 4(sp)
    sw   x3,   3 * 4(sp)
    sw   x4,   4 * 4(sp)
    sw   x5,   5 * 4(sp)
    sw   x6,   6 * 4(sp)
    sw   x7,   7 * 4(sp)
    sw   x8,   8 * 4(sp)
    sw   x9,   9 * 4(sp)
    sw   x10, 10 * 4(sp)
    sw   x11, 11 * 4(sp)
    sw   x12, 12 * 4(sp)
    sw   x13, 13 * 4(sp)
    sw   x14, 14 * 4(sp)
    sw   x15, 15 * 4(sp)
    sw   x16, 16 * 4(sp)
    sw   x17, 17 * 4(sp)
    sw   x18, 18 * 4(sp)
    sw   x19, 19 * 4(sp)
    sw   x20, 20 * 4(sp)
    sw   x21, 21 * 4(sp)
    sw   x22, 22 * 4(sp)
    sw   x23, 23 * 4(sp)
    sw   x24, 24 * 4(sp)
    sw   x25, 25 * 4(sp)
    sw   x26, 26 * 4(sp)
    sw   x27, 27 * 4(sp)
    sw   x28, 28 * 4(sp)
    sw   x29, 29 * 4(sp)
    sw   x30, 30 * 4(sp)
    sw   x31, 31 * 4(sp)

    csrr a0, mepc
    sw   a0, 0 * 4(sp)
    csrr a1, mstatus
    sw   a1, 32 * 4(sp)
    csrr a2, mscratch
    sw   a2, 33 * 4(sp)

    # backup stack pointer for later check.
    move  s0, sp
    move  a0, sp
    addi  a0, a0, 34 * 4
    sw    a0, 2 * 4(sp)

    csrr  a0, mstatus
    call  fpu_save_inirq

 #re-backup the sstatus in chance clean the fpustatus.
   csrr s1, mstatus
   sw   s1, 32 * 4(s0)

    # check bit[63], 0:interrupt, 1:exceptions
    csrr  a0, mcause
    bge   a0, zero, .exception

.interrupt:
    # handle interrupts.
    call  hal_interrupt_enter
    csrr  a0, mcause
    csrr  a1, mepc
    csrr  a2, mtval
    move  a3, s0
    call  riscv_cpu_handle_interrupt

    call  hal_interrupt_leave
    j     .restore_all

.exception:
    # handle syscall and exceptions.
    csrr  a0, mcause
    csrr  a1, mepc
    csrr  a2, mtval
    move  a3, s0
    call  riscv_cpu_handle_exception
    nop

.restore_all:
    csrr a0, sstatus
    call fpu_restore_inirq

    # guanatee SPP bit was set, means we are from supervisor mode.
    csrr a0, sstatus
    ori  a0, a0, 0x100
    csrw sstatus, a0
    # keep sstatus not change inirq.
0:
    bne  a0, s1, 0b

    # keep stack balance.
1:
    bne  sp, s0, 1b

#.option push
#.option norelax
#    la    a0, __global_pointer$
#.option pop

.gplwr:
    auipc a0, %pcrel_hi(__global_pointer$)
    addi  a0, a0,%pcrel_lo(.gplwr)

2:  # x3 is gp in abi
    bne   gp, a0,2b

    jal  schedule_preempt_inirq

    # resotre interrupted context
    lw   a0,   0 * 4(sp)
    csrw sepc, a0

    lw   x1,   1 * 4(sp)

    lw   a0,   32 * 4(sp)
    csrw sstatus, a0

    lw   a0,   33 * 4(sp)
    csrw sscratch, a0

    lw   x3,   3 * 4(sp)
    lw   x4,   4 * 4(sp)
    lw   x5,   5 * 4(sp)
    lw   x6,   6 * 4(sp)
    lw   x7,   7 * 4(sp)
    lw   x8,   8 * 4(sp)
    lw   x9,   9 * 4(sp)
    lw   x10, 10 * 4(sp)
    lw   x11, 11 * 4(sp)
    lw   x12, 12 * 4(sp)
    lw   x13, 13 * 4(sp)
    lw   x14, 14 * 4(sp)
    lw   x15, 15 * 4(sp)
    lw   x16, 16 * 4(sp)
    lw   x17, 17 * 4(sp)
    lw   x18, 18 * 4(sp)
    lw   x19, 19 * 4(sp)
    lw   x20, 20 * 4(sp)
    lw   x21, 21 * 4(sp)
    lw   x22, 22 * 4(sp)
    lw   x23, 23 * 4(sp)
    lw   x24, 24 * 4(sp)
    lw   x25, 25 * 4(sp)
    lw   x26, 26 * 4(sp)
    lw   x27, 27 * 4(sp)
    lw   x28, 28 * 4(sp)
    lw   x29, 29 * 4(sp)
    lw   x30, 30 * 4(sp)
    lw   x31, 31 * 4(sp)

    addi sp,  sp, 34 * 4
    sret

.end
