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
#include <csr.h>
#include <excep.h>

#ifdef CONFIG_FPU_DOUBLE
#define FLOAD			fld
#define FSTORE			fsd
#else
#define FLOAD			flw
#define FSTORE			fsw
#endif
/*
 * unsigned long awos_arch_task_switch(unsigned long from, unsigned long to);
 */

.global awos_arch_task_switch
awos_arch_task_switch:
   addi  sp,  sp, -14 * 4
   sw    ra,  0*4(sp)

   csrr  t0,  mstatus
   li    t1,  MR_MPIE
   not   t1,  t1
   li    t2,  MR_MPP
   and   t0,  t0, t1
   or    t0,  t0, t2
   sw    t0,  1*4(sp)

   sw    s0,  2*4(sp)
   sw    s1,  3*4(sp)
   sw    s2,  4*4(sp)
   sw    s3,  5*4(sp)
   sw    s4,  6*4(sp)
   sw    s5,  7*4(sp)
   sw    s6,  8*4(sp)
   sw    s7,  9*4(sp)
   sw    s8,  10*4(sp)
   sw    s9,  11*4(sp)
   sw    s10, 12*4(sp)
   sw    s11, 13*4(sp)

   sw    sp,  0(a0)
   lw    sp,  0(a1)

   jal   check_gp_balance

   # load context switch registers.
   lw    ra,  0*4(sp)

   # load mstatus register.
   lw    t0,  1*4(sp)
   csrw  mstatus,t0
   csrw  mepc,ra

   lw    s0,  2*4(sp)
   lw    s1,  3*4(sp)
   lw    s2,  4*4(sp)
   lw    s3,  5*4(sp)
   lw    s4,  6*4(sp)
   lw    s5,  7*4(sp)
   lw    s6,  8*4(sp)
   lw    s7,  9*4(sp)
   lw    s8,  10*4(sp)
   lw    s9,  11*4(sp)
   lw    s10, 12*4(sp)
   lw    s11, 13*4(sp)
   addi  sp,  sp, 4*14

   move  tp,  a1
#if 1
   mret
#else
   #pseudo of 'jalr zero, 0(ra)'
   ret
#endif

/*
 * void awos_arch_first_task_start(unsigned long to);
 * r0 --> to
 */
.global awos_arch_first_task_start
awos_arch_first_task_start:
   la    t0,  melis_kernel_running
   li    t1,  1
   sb    t1,  (t0)

   lw    sp,  0(a0)

   # load context switch registers.
   lw    ra,  0*4(sp)

   #load mstatus register.
   lw    t0,  1*4(sp)
   csrw  mstatus,t0
   csrw  mepc,ra

   lw    s0,  2*4(sp)
   lw    s1,  3*4(sp)
   lw    s2,  4*4(sp)
   lw    s3,  5*4(sp)
   lw    s4,  6*4(sp)
   lw    s5,  7*4(sp)
   lw    s6,  8*4(sp)
   lw    s7,  9*4(sp)
   lw    s8,  10*4(sp)
   lw    s9,  11*4(sp)
   lw    s10, 12*4(sp)
   lw    s11, 13*4(sp)
   addi  sp,  sp, 4*14

   move  tp,  a0
   move  a1,  a0
   move  a0,  zero

   # task top exit entry to ra.
   move  ra,  s3

#if 1
   mret
#else
   #pseudo of 'jalr zero, 0(ra)'
   ret
#endif

/*
 * uint32_t awos_arch_chksched_start(void);
 */
.global awos_arch_chksched_start
awos_arch_chksched_start:
   la    t0,  melis_kernel_running
   lbu   a0, (t0)
   ret

/*
 * void awos_arch_save_fpu_status(fpu_context_t *);
 */
.global awos_arch_save_fpu_status
awos_arch_save_fpu_status:
   li  t0, SR_FS
   csrs mstatus, t0
   frcsr    t1
   FSTORE f0,  FPU_CTX_F0_F0(a0)
   FSTORE f1,  FPU_CTX_F1_F0(a0)
   FSTORE f2,  FPU_CTX_F2_F0(a0)
   FSTORE f3,  FPU_CTX_F3_F0(a0)
   FSTORE f4,  FPU_CTX_F4_F0(a0)
   FSTORE f5,  FPU_CTX_F5_F0(a0)
   FSTORE f6,  FPU_CTX_F6_F0(a0)
   FSTORE f7,  FPU_CTX_F7_F0(a0)
   FSTORE f8,  FPU_CTX_F8_F0(a0)
   FSTORE f9,  FPU_CTX_F9_F0(a0)
   FSTORE f10, FPU_CTX_F10_F0(a0)
   FSTORE f11, FPU_CTX_F11_F0(a0)
   FSTORE f12, FPU_CTX_F12_F0(a0)
   FSTORE f13, FPU_CTX_F13_F0(a0)
   FSTORE f14, FPU_CTX_F14_F0(a0)
   FSTORE f15, FPU_CTX_F15_F0(a0)
   FSTORE f16, FPU_CTX_F16_F0(a0)
   FSTORE f17, FPU_CTX_F17_F0(a0)
   FSTORE f18, FPU_CTX_F18_F0(a0)
   FSTORE f19, FPU_CTX_F19_F0(a0)
   FSTORE f20, FPU_CTX_F20_F0(a0)
   FSTORE f21, FPU_CTX_F21_F0(a0)
   FSTORE f22, FPU_CTX_F22_F0(a0)
   FSTORE f23, FPU_CTX_F23_F0(a0)
   FSTORE f24, FPU_CTX_F24_F0(a0)
   FSTORE f25, FPU_CTX_F25_F0(a0)
   FSTORE f26, FPU_CTX_F26_F0(a0)
   FSTORE f27, FPU_CTX_F27_F0(a0)
   FSTORE f28, FPU_CTX_F28_F0(a0)
   FSTORE f29, FPU_CTX_F29_F0(a0)
   FSTORE f30, FPU_CTX_F30_F0(a0)
   FSTORE f31, FPU_CTX_F31_F0(a0)
   sw  t1,  FPU_CTX_FCSR_F0(a0)

   # clr FS domain
   csrc mstatus, t0

   # clean status woulw clr sr_sw;
   li t0, SR_FS_CLEAN
   csrs mstatus, t0
   ret

/*
 * void awos_arch_restore_fpu_status(fpu_context_t *);
 */

.global awos_arch_restore_fpu_status
awos_arch_restore_fpu_status:
   li  t0, SR_FS
   lw  t1, FPU_CTX_FCSR_F0(a0)
   csrs mstatus, t0
   FLOAD f0, FPU_CTX_F0_F0(a0)
   FLOAD f1, FPU_CTX_F1_F0(a0)
   FLOAD f2, FPU_CTX_F2_F0(a0)
   FLOAD f3, FPU_CTX_F3_F0(a0)
   FLOAD f4, FPU_CTX_F4_F0(a0)
   FLOAD f5, FPU_CTX_F5_F0(a0)
   FLOAD f6, FPU_CTX_F6_F0(a0)
   FLOAD f7, FPU_CTX_F7_F0(a0)
   FLOAD f8, FPU_CTX_F8_F0(a0)
   FLOAD f9, FPU_CTX_F9_F0(a0)
   FLOAD f10,FPU_CTX_F10_F0(a0)
   FLOAD f11,FPU_CTX_F11_F0(a0)
   FLOAD f12,FPU_CTX_F12_F0(a0)
   FLOAD f13,FPU_CTX_F13_F0(a0)
   FLOAD f14,FPU_CTX_F14_F0(a0)
   FLOAD f15,FPU_CTX_F15_F0(a0)
   FLOAD f16,FPU_CTX_F16_F0(a0)
   FLOAD f17,FPU_CTX_F17_F0(a0)
   FLOAD f18,FPU_CTX_F18_F0(a0)
   FLOAD f19,FPU_CTX_F19_F0(a0)
   FLOAD f20,FPU_CTX_F20_F0(a0)
   FLOAD f21,FPU_CTX_F21_F0(a0)
   FLOAD f22,FPU_CTX_F22_F0(a0)
   FLOAD f23,FPU_CTX_F23_F0(a0)
   FLOAD f24,FPU_CTX_F24_F0(a0)
   FLOAD f25,FPU_CTX_F25_F0(a0)
   FLOAD f26,FPU_CTX_F26_F0(a0)
   FLOAD f27,FPU_CTX_F27_F0(a0)
   FLOAD f28,FPU_CTX_F28_F0(a0)
   FLOAD f29,FPU_CTX_F29_F0(a0)
   FLOAD f30,FPU_CTX_F30_F0(a0)
   FLOAD f31,FPU_CTX_F31_F0(a0)
   fscsr t1

   # clr FS domain
   csrc mstatus, t0

   # clean status woulw clr sr_sw;
   li t0, SR_FS_CLEAN
   csrs mstatus, t0
   ret

/*
 * void check_gp_balance(void);
 */
.global check_gp_balance
check_gp_balance:
.gplwr:
    auipc a2, %pcrel_hi(__global_pointer$)
    addi  a2, a2,%pcrel_lo(.gplwr)

1:  # x3 is gp in abi
    bne   gp, a2,1b
    ret

/*
 * void delay_10insn(void);
 */
.global delay_10insn
delay_10insn:
   li     a0, 10
   beqz   a0, 2f
1:
   addi   a0, a0, -1
   .rept  5
   nop
   .endr
   bnez a0, 1b
2:
   ret

.end
