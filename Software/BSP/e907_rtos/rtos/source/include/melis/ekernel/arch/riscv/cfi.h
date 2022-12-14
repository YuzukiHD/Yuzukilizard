/*
 * =====================================================================================
 *
 *       Filename:  cfi.h
 *
 *    Description:  Debug indicator definition for GNU assemble.
 *
 *        Version:  Melis3.0
 *         Create:  2017-11-02 10:14:39
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-05-18 16:23:49
 *
 * =====================================================================================
 */

#ifndef __GCC_CFI_DEF__
#define __GCC_CFI_DEF__

.macro cfi_debug_info_begin symbol
.balign 8
.global \symbol
.section .text.\symbol, "ax"
.cfi_sections .eh_frame, .debug_frame
.type  \symbol, % function
.cfi_startproc
\symbol:
.endm

.macro cfi_debug_info_end symbol
.cfi_endproc
.size  \symbol, . - \symbol
.ident "gcc version 7.2.1 20170904 (release)"
.endm

.macro cfi_tlb_info_begin symbol
.balign 8
.global \symbol
.section .tlb.code, "ax"
.cfi_sections .eh_frame, .debug_frame
.type  \symbol, % function
.cfi_startproc
\symbol:
.endm

.macro cfi_tlb_info_end symbol
.cfi_endproc
.size  \symbol, . - \symbol
.ident "gcc version 7.2.1 20170904 (release)"
.endm

#define GTEXT(sym) .global sym; .type sym, %function
#define GDATA(sym) .global sym; .type sym, %object
#define WTEXT(sym) .weak sym; .type sym, %function
#define WDATA(sym) .weak sym; .type sym, %object

#endif  //__GCC_CFI_DEF__
