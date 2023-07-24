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
.section .bss
	.align 2
	.globl g_sleep_context
	.globl g_sleep_flag

g_sleep_context:
	.space 768

g_sleep_flag:
.long 0

.macro save_csr name index
	csrr	t1, \name
	sw		t1, \index * 4(t0)
.endm

.macro restore_csr name index
	lw		t1, \index * 4(t0)
	csrw	\name, t1
.endm

.macro general_reg option base
	la		t0, \base
	\option		x0,  0*4(t0)
	\option		ra,  1*4(t0)
	\option		sp,  2*4(t0)
	\option		gp,  3*4(t0)
	\option		tp,  4*4(t0)
	# \option		t0,  5*4(t0)
	\option		t1,  6*4(t0)
	\option		x7,  7*4(t0)
	\option		x8,  8*4(t0)
	\option		x9,  9*4(t0)
	\option		x10, 10*4(t0)
	\option		x11, 11*4(t0)
	\option		x12, 12*4(t0)
	\option		x13, 13*4(t0)
	\option		x14, 14*4(t0)
	\option		x15, 15*4(t0)
	\option		x16, 16*4(t0)
	\option		x17, 17*4(t0)
	\option		x18, 18*4(t0)
	\option		x19, 19*4(t0)
	\option		x20, 20*4(t0)
	\option		x21, 21*4(t0)
	\option		x22, 22*4(t0)
	\option		x23, 23*4(t0)
	\option		x24, 24*4(t0)
	\option		x25, 25*4(t0)
	\option		x26, 26*4(t0)
	\option		x27, 27*4(t0)
	\option		x28, 28*4(t0)
	\option		x29, 29*4(t0)
	\option		x30, 30*4(t0)
	\option		x31, 31*4(t0)
	addi		t0, t0, 32 * 4
.endm

.macro float_reg option base
	\option		ft0,  0*8(\base)
	\option		ft1,  1*8(\base)
	\option		ft2,  2*8(\base)
	\option		ft3,  3*8(\base)
	\option		ft4,  4*8(\base)
	\option		ft5,  5*8(\base)
	\option		ft6,  6*8(\base)
	\option		ft7,  7*8(\base)
	\option		fs0,  8*8(\base)
	\option		fs1,  9*8(\base)
	\option		fa0,  10*8(\base)
	\option		fa1,  11*8(\base)
	\option		fa2,  12*8(\base)
	\option		fa3,  13*8(\base)
	\option		fa4,  14*8(\base)
	\option		fa5,  15*8(\base)
	\option		fa6,  16*8(\base)
	\option		fa7,  17*8(\base)
	\option		fs2,  18*8(\base)
	\option		fs3,  19*8(\base)
	\option		fs4,  20*8(\base)
	\option		fs5,  21*8(\base)
	\option		fs6,  22*8(\base)
	\option		fs7,  23*8(\base)
	\option		fs8,  24*8(\base)
	\option		fs9,  25*8(\base)
	\option		fs10, 26*8(\base)
	\option		fs11, 27*8(\base)
	\option		ft8,  28*8(\base)
	\option		ft9,  29*8(\base)
	\option		ft10, 30*8(\base)
	\option		ft11, 31*8(\base)
	addi	\base, \base, 8*32
.endm

.text
	.align 2
	.globl cpu_suspend
	.type  cpu_suspend, %function
cpu_suspend:
	# save t0, t1 value to stack
	sw		t0, -4(sp)
	sw		t1, -8(sp)
	# save t0
	la		t0, g_sleep_context
	lw		t1, -4(sp)
	sw		t1, 5 * 4(t0)
	# restore t1
	lw		t1, -8(sp)

	# save register excep t0
	general_reg  sw g_sleep_context

	/* Save M-mode Exception Setting Register */
	save_csr mstatus		0
	save_csr mie			1
	save_csr mtvec			2
	save_csr mtvt			3
	addi	t0, t0, 4*4
	/* Save  M-mode Exception Handling Register */
	save_csr mscratch		0
	save_csr mepc			1
	save_csr mcause			2
	save_csr mtval			3
	save_csr mip			4
	save_csr mnxti			5
	save_csr mscratchcsw	6
	save_csr mscratchcswl	7
	addi	t0, t0, 4*8

#ifdef CONFIG_ARCH_RISCV_FPU
	float_reg fsd t0

	/* save float status register */
	save_csr fflags			0
	save_csr frm			1
	save_csr fcsr			2
	save_csr fxcr			3
	addi	t0, t0, 4*4
#endif
	/* Save M-mode Memory Protect Register */
	save_csr pmpcfg0		0
	save_csr pmpcfg1		1
	save_csr pmpcfg2		2
	save_csr pmpcfg3		3
	save_csr pmpaddr0		4
	save_csr pmpaddr1		5
	save_csr pmpaddr2		6
	save_csr pmpaddr3		7
	save_csr pmpaddr4		8
	save_csr pmpaddr5		9
	save_csr pmpaddr6		10
	save_csr pmpaddr7		11
	save_csr pmpaddr8		12
	save_csr pmpaddr9		13
	save_csr pmpaddr10		14
	save_csr pmpaddr11		15
	save_csr pmpaddr12		16
	save_csr pmpaddr13		17
	save_csr pmpaddr14		18
	save_csr pmpaddr15		19
	addi	t0, t0, 4*20

	/* Save M-mode Perf and Counter Register */
	save_csr mcounteren		0
	save_csr mcountinhibit	1
	save_csr mhpmevent3		2
	save_csr mhpmevent4		3
	save_csr mhpmevent5		4
	save_csr mhpmevent6		5
	save_csr mhpmevent7		6
	save_csr mhpmevent8		7
	save_csr mhpmevent9		8
	save_csr mhpmevent10	9
	save_csr mhpmevent11	10
	save_csr mhpmevent12	11
	save_csr mhpmevent13	12
	save_csr mhpmevent14	13
	save_csr mhpmevent15	14
	save_csr mhpmevent16	15
	save_csr mhpmevent17	16
	save_csr mcycle			17
	save_csr minstret		18
	save_csr mcycleh		19
	save_csr minstreth		20
	save_csr mhpmcounter3	21
	save_csr mhpmcounter4	22
	save_csr mhpmcounter5	23
	save_csr mhpmcounter6	24
	save_csr mhpmcounter7	25
	save_csr mhpmcounter8	26
	save_csr mhpmcounter9	27
	save_csr mhpmcounter10	28
	save_csr mhpmcounter11	29
	save_csr mhpmcounter12	30
	save_csr mhpmcounter13	31
	save_csr mhpmcounter14	32
	save_csr mhpmcounter15	33
	save_csr mhpmcounter16	34
	save_csr mhpmcounter17	35
	save_csr mhpmcounter3h	36
	save_csr mhpmcounter4h	37
	save_csr mhpmcounter5h	38
	save_csr mhpmcounter6h	39
	save_csr mhpmcounter7h	40
	save_csr mhpmcounter8h	41
	save_csr mhpmcounter9h	42
	save_csr mhpmcounter10h	43
	save_csr mhpmcounter11h	44
	save_csr mhpmcounter12h	45
	save_csr mhpmcounter13h	46
	save_csr mhpmcounter14h	47
	save_csr mhpmcounter15h	48
	save_csr mhpmcounter16h	49
	save_csr mhpmcounter17h	50
	addi	t0, t0, 4*51

	/* Save M-mode Control vs Status Register */
	save_csr mxstatus		0
	save_csr mhcr			1
	save_csr mhint			2
	save_csr mraddr			3
	save_csr mexstatus		4
	save_csr mnmicause		5
	save_csr mnmipc			6
	addi	t0, t0, 4*7
	/* Save Debug Register */
	# save_csr dcsr			0
	# save_csr dpc			1

	/* Updata sleep flag */
	la		t0, g_sleep_flag
	li		t1, 1
	sw		t1, 0(t0)

	/* restore the scene */
	general_reg  lw g_sleep_context
	/* restore t0 value */
	lw		t0, -4(sp)

	ret

	.align 2
	.globl cpu_resume
	.type  cpu_resume, %function
cpu_resume:
	/* restore general register later */
	la		t0, g_sleep_context
	addi	t0, t0, 4*32
	/* Save M-mode Exception Setting Register */
	restore_csr mstatus			0
	restore_csr mie				1
	restore_csr mtvec			2
	restore_csr mtvt			3
	addi	t0, t0, 4*4

	/* Save  M-mode Exception Handling Register */
	restore_csr mscratch		0
	restore_csr mepc			1
	restore_csr mcause			2
	restore_csr mtval			3
	restore_csr mip				4
	# restore_csr mnxti			5
	restore_csr mscratchcsw		6
	restore_csr mscratchcswl	7
	addi	t0, t0, 4*8

#ifdef CONFIG_ARCH_RISCV_FPU
	float_reg fld t0

	/* save float status register */
	restore_csr fflags			0
	restore_csr frm				1
	restore_csr fcsr			2
	restore_csr fxcr			3
	addi	t0, t0, 4*4
#endif

	/* Save M-mode Memory Protect Register */
	restore_csr pmpcfg0			0
	restore_csr pmpcfg1			1
	restore_csr pmpcfg2			2
	restore_csr pmpcfg3			3
	restore_csr pmpaddr0		4
	restore_csr pmpaddr1		5
	restore_csr pmpaddr2		6
	restore_csr pmpaddr3		7
	restore_csr pmpaddr4		8
	restore_csr pmpaddr5		9
	restore_csr pmpaddr6		10
	restore_csr pmpaddr7		11
	restore_csr pmpaddr8		12
	restore_csr pmpaddr9		13
	restore_csr pmpaddr10		14
	restore_csr pmpaddr11		15
	restore_csr pmpaddr12		16
	restore_csr pmpaddr13		17
	restore_csr pmpaddr14		18
	restore_csr pmpaddr15		19
	addi	t0, t0, 4*20

	/* Save M-mode Perf and Counter Register */
	restore_csr mcounteren		0
	restore_csr mcountinhibit	1
	restore_csr mhpmevent3		2
	restore_csr mhpmevent4		3
	restore_csr mhpmevent5		4
	restore_csr mhpmevent6		5
	restore_csr mhpmevent7		6
	restore_csr mhpmevent8		7
	restore_csr mhpmevent9		8
	restore_csr mhpmevent10	9
	restore_csr mhpmevent11	10
	restore_csr mhpmevent12	11
	restore_csr mhpmevent13	12
	restore_csr mhpmevent14	13
	restore_csr mhpmevent15	14
	restore_csr mhpmevent16	15
	restore_csr mhpmevent17	16
	restore_csr mcycle			17
	restore_csr minstret		18
	restore_csr mcycleh		19
	restore_csr minstreth		20
	restore_csr mhpmcounter3	21
	restore_csr mhpmcounter4	22
	restore_csr mhpmcounter5	23
	restore_csr mhpmcounter6	24
	restore_csr mhpmcounter7	25
	restore_csr mhpmcounter8	26
	restore_csr mhpmcounter9	27
	restore_csr mhpmcounter10	28
	restore_csr mhpmcounter11	29
	restore_csr mhpmcounter12	30
	restore_csr mhpmcounter13	31
	restore_csr mhpmcounter14	32
	restore_csr mhpmcounter15	33
	restore_csr mhpmcounter16	34
	restore_csr mhpmcounter17	35
	restore_csr mhpmcounter3h	36
	restore_csr mhpmcounter4h	37
	restore_csr mhpmcounter5h	38
	restore_csr mhpmcounter6h	39
	restore_csr mhpmcounter7h	40
	restore_csr mhpmcounter8h	41
	restore_csr mhpmcounter9h	42
	restore_csr mhpmcounter10h	43
	restore_csr mhpmcounter11h	44
	restore_csr mhpmcounter12h	45
	restore_csr mhpmcounter13h	46
	restore_csr mhpmcounter14h	47
	restore_csr mhpmcounter15h	48
	restore_csr mhpmcounter16h	49
	restore_csr mhpmcounter17h	50
	addi	t0, t0, 4*51

	/* Save M-mode Control vs Status Register */
	restore_csr mxstatus		0
	restore_csr mhcr			1
	restore_csr mhint			2
	restore_csr mraddr			3
	restore_csr mexstatus		4
	restore_csr mnmicause		5
	restore_csr mnmipc			6
	addi	t0, t0, 4*7

	/* Restore Debug Register */
	# restore_csr dcsr		0
	# restore_csr dpc			1

	/* Updata sleep flag */
	la		t0, g_sleep_flag
	sw		x0, 0(t0)

	/* restore general register */
	general_reg  lw g_sleep_context
	/* restore t0 register */
	sw		t1, -4(sp)
	la		t1, g_sleep_context
	lw		t0, 5 * 4(t1)
	lw		t1, -4(sp)

	ret
