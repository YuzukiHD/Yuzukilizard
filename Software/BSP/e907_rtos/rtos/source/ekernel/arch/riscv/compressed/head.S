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

#include <compiler.h>

#define HEAP_SIZE			(0x10000)

		.section ".start", "ax", @progbits
		.align

		.type	start, %function
.option norvc
start:
		j		1f

		.word	_magic_sig		# 魔数
		.word	_magic_comp
		.word	_magic_start	# zImage 绝对运行地址
		.word	_magic_end		# zImage 结束地址
		.word	input_data_end - 4	# decompress image size
		.word	0x04030201		# 结束标志位

1:
		#  mask all interrupts
		csrw mie, 0
		csrw mip, 0

		#  Enable thead extens
		li t0, (1 << 22) | (1 << 15)
		csrs mxstatus, t0

		rdtime s8
		/*
		 * t0 = 当前pc
		 * t4 = 解压Image 起始地址
		 * if t0 < t4 :
		 *      t0 = LC0[8] = _end - restart + = 当前镜像大小
		 *      t0 = t0 + pc = 当镜像结束地址
		 *      if t4 < t0 :(这种情况会发生自覆盖)
		 *          t4 |= 0x1
		 *      else :
		 *          bl cache_on
		 * else :
		 *          bl cache_on
		 */
#ifdef CONFIG_KERNEL_COMPRESS_ELF
		la	t4, zelfaddr
		bnez t4, 1f

		la		t0, LC0
		lw		s0, 0 * 4(t0)	#  LC0
		lw		t4, 7 * 4(t0)	#  .L_user_stack_end
		sub		t0, t0, s0 		#  calculate the delta offset

		add		t4, t4, t0
		li		t0, HEAP_SIZE
		add		t4, t4, t0		# 解压到 当前镜像 结束地址
#else
		la	t4, zreladdr
#endif
1:
		auipc	t0, 0
		bgeu	t0, t4, 1f

		la 		t1, LC0
		lw		t2, 8 * 4(t1)
		auipc	t0, 0
		add		t0, t0, t2

		bltu	t4, t0, 2f
1:
		jal		cache_on
		j       restart
2:
		ori		t4, t4, 1		#  remember we skipped cache_on
restart:
		la	t0, LC0
		lw	s0, 0 * 4(t0)		#  LC0
		lw	s1, 1 * 4(t0)		#  __bss_start
		lw	s2, 2 * 4(t0)		#  _end
		lw	s3, 3 * 4(t0)		#  _edata
		lw	s4, 4 * 4(t0)		#  input+data_end - 4
		lw	s5, 5 * 4(t0)		#  _got_start
		lw	s6, 6 * 4(t0)		#  _got_end
		lw	sp, 7 * 4(t0)		#  .L_user_stack_end

		/*
		*	r0 -= r1 	计算偏移, 编译地址与运行地址的差值，以此来修正其他符号的地址
		*	r6 += r0 	重新计算_edata, 获得_edata的实际运行地址
		*	r10 += r0 	重新获得压缩后的内核大小数据位置
		*/
		sub	t0, t0, s0 		#  calculate the delta offset
		add	s3, s3, t0		#  _edata
		add	s4, s4, t0		#  inflated kernel size location

		/*
		* 将解压后的内核大小数据正确地放入t1中（避免了大小端问题）
		*/
		lbu	t1, 0(s4)

		lbu	t2, 1(s4)
		slli t2, t2, 8
		or t1, t1, t2

		lbu	t2, 2(s4)
		slli t2, t2, 16
		or t1, t1, t2

		lbu	t2, 3(s4)
		slli t2, t2, 24
		or t1, t1, t2

		/*
		* sp += r0
		* s4 = sp + HEAP_SIZE (预留空间出来给作为后续C环境的堆空间)
		*/
		add		sp, sp, t0
		li		t5, HEAP_SIZE
		add		s4, sp, t5

		/*
		 * 检查是否发生自覆盖.
		 *   t4  = 解压后镜像的 起始地址
		 *   t1  = 解压后镜像的 大小
		 *   s4 = 当前镜像的 结束地址, 包含 bss/stack/malloc 空间
		 * 不发生自覆盖的条件: 16K是MMU的页表大小
		 *   t4 >= s4   -> OK
		 *   t4 + t1 <= 当前PC -> OK
		 */
		#  addi  s4, s4, 0x4000
		bgeu t4, s4, wont_overwrite
		add	s4, t4, t1
		auipc t1, 0
		bgeu t1, s4, wont_overwrite

		/*
		 * 将自己拷贝到 解压后内核的后面.
		 *   s3  = _edata 本镜像结束地址 (text+data+bss段,已修正)
		 *   s4 = 解压后镜像的结束地址
		 * Because we always copy ahead, we need to do it from the end and go
		 * backward in case the source and destination overlap.
		 */
		/*
		 * 复制到解压后的镜像后
		 * 对齐到 256B边界
		 * s4 = ALIGN_UP(s4, 0xff)
		 */
		# la		t5, ((reloc_code_end - restart + 256) & ~255)
		la		t5, reloc_code_end
		la 		t3, restart
		sub		t5, t5, t3
		addi	t5, t5, 256
		li		t3, ~0xff
		and		t5, t5, t3

		add		s4, s4, t5
		andi	s4, s4, ~0xff
		/*
		 * 对齐到32B
		 */
		la		t1, restart
		andi	t1, t1, ~0x1f
		/*
		 * 自复制
		 * NOTE:
		 *	t1 当前 restart 地址
		 *  s3 当前镜像结束地址
		 */
		sub		t2, s3, t1			#  size to copy
		addi	t2, t2, 0x1f		#  rounded up to a multiple
		andi	t2, t2, ~0x1f		#  ... of 32 bytes
		add		t3, t2, t1			#  src t3
		add		t2, t2, s4			#  dst t2

		/*
		 * 逐个byte复制,直到 t3 <= t1
		 * 从后往前复制
		 */
1:		lw		a0, 0 * 4(t3)
		lw		a0, 1 * 4(t3)
		lw		a0, 2 * 4(t3)
		lw		a0, 3 * 4(t3)
		addi	t3, t3, 4 * 4
		sw		a0, 0 * 4(t2)
		sw		a0, 1 * 4(t2)
		sw		a0, 2 * 4(t2)
		sw		a0, 3 * 4(t2)
		addi	t2, t2, 4 * 4
		bgeu	t3, t1, 1b

		/* 保留重定位代码的偏移 */
		sub		t6, t2, t3
		/* cache_clean_flush may use the stack, so relocate it */
		add		sp, sp, t6

		jal		cache_clean_flush

		la		t1, restart
		add		t1, t1, t6
		jr		t1

wont_overwrite:
		/*
		 *   t0 = delta(编译地址与运行地址的差值)
		 *   s0 = LC0
		 *   s1 = BSS start
		 *   s2 = BSS end
		 *   s3 = _edata
		 *   s4 = input+data_end - 4
		 *   t4  = 解压后镜像的 起始地址
		 *   s5 = _got_start
		 *   s6 = _got_end
		 *   sp = .L_user_stack_end
		 */
		beqz	t0, not_relocated

		/* 重定位 GOT */
		add		s5, s5, t0
		add		s6, s6, t0
		/* 重定位 BSS */
		add		s1, s1, t0
		add		s2, s2, t0

not_relocated:
1:		sw		t0, 0(s1)
		addi	s1, s1, 4
		bltu	s1, s2, 1b

		/*
		 * 如果前面跳过了 cache_on, t4的最低位=1
		 * 这里补上 开 cache
		 */
		andi	t0, t4, 0x1
		beqz	t0, 2f
		li		t5, 1
		sub		t4, t4, t5
		jal	cache_on
2:
		/*
		 * 准备跳到C执行代码
		 *   a0  = 内核代码的开始地址
		 */
		mv		x3, t4
		mv		a0, t4
		mv		a1 , sp				#  free_mem_start
		li		t5, HEAP_SIZE
		add		a2, sp, t5		#  free_mem_end
		mv		a3, x0				#  architecture ID
		jal		decompress_kernel
		jal		cache_clean_flush
#ifdef CONFIG_KERNEL_COMPRESS_ELF
		mv		a0, x3
		jal		load_elf_image
		mv		a0, x3
		jal		elf_get_entry_addr
		mv		x3,	a0
		jal		cache_clean_flush
#endif
		jal		cache_off

		mv      a0, s8
		jr		x3			#  __enter_kernel

		.align	5
		.type	LC0, %object
LC0:
		.word	LC0					#  s0
		.word	__bss_start			#  s1
		.word	_end				#  s2
		.word	_edata				#  s3
		.word	input_data_end - 4	#  s4 (inflated size location)
		.word	_got_start			#  s5
		.word	_got_end			#  s6
		.word	.L_user_stack_end	#  sp
		.word	_end - restart  	#  s7
		.size	LC0, . - LC0

		.align	5
cache_on:
		li t0, 0x11ff
		csrw mhcr, t0
		li t0, 0x638000
		csrw mxstatus, t0
		li t0, 0x11ff
		csrw mhint, t0
		fence
		sync
		ret
cache_off:
		li t0, 0x3
		csrc mhcr, x0
		fence
		sync
		ret
cache_clean_flush:
		dcache.call
		icache.iall
		fence
		sync
		ret

reloc_code_end:
	.align 6
	.section ".stack", "aw", %nobits
.L_user_stack:	.space	4096
.L_user_stack_end:
