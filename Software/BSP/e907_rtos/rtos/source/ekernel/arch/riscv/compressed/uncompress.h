/*
 *  arch/arm/mach-sun6i/include/mach/uncompress.h
 *
 * Copyright(c) 2012-2016 Allwinnertech Co., Ltd.
 * Author: Benn Huang <benn@allwinnertech.com>
 *	   Sugar <shuge@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 */
#ifndef __MACH_UNCOMPRESS_H
#define __MACH_UNCOMPRESS_H

#include <barrier.h>

#ifdef CONFIG_KERNEL_COMPRESS_DEBUG
#define SUNXI_UART_PBASE	(0x02500000)
#define UART_TF		(*(volatile unsigned long*)(SUNXI_UART_PBASE + 0x00))
#define UART_SR		(*(volatile unsigned long*)(SUNXI_UART_PBASE + 0x7C))
#define TX_BUSY		(0x02)
/*
 * put the character through uart
 */
static inline void putc(int c)
{
	while (!(UART_SR & TX_BUSY));
		UART_TF = c;
}
#else
static inline void putc(int c) {}
#endif
static inline void flush(void) {}
static inline void arch_decomp_setup(void) {}

#define arch_decomp_setup()
#define arch_decomp_wdog()

#endif /* __MACH_UNCOMPRESS_H */
