/*
 * ===========================================================================================
 *
 *       Filename:  barrier.h
 *
 *    Description:  barrier impl. of riscv
 *
 *        Version:  Melis3.0
 *         Create:  2020-07-08 14:35:53
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-09-28 11:12:43
 *
 * ===========================================================================================
 */
#ifndef _ASM_RISCV_BARRIER_H
#define _ASM_RISCV_BARRIER_H

#ifndef __ASSEMBLY__

#define nop()           __asm__ __volatile__ ("nop")

#define RISCV_FENCE(p, s) \
    __asm__ __volatile__ ("fence " #p "," #s : : : "memory")

/* These barriers need to enforce ordering on both devices or memory. */
#define mb()            RISCV_FENCE(iorw,iorw)
#define rmb()           RISCV_FENCE(ir,ir)
#define wmb()           RISCV_FENCE(ow,ow)

/* These barriers do not need to enforce ordering on devices, just memory. */
#define __smp_mb()      RISCV_FENCE(rw,rw)
#define __smp_rmb()     RISCV_FENCE(r,r)
#define __smp_wmb()     RISCV_FENCE(w,w)

#define dmb(...)        mb();
#define dsb(...)        mb();
#define isb(...)        do { __asm__ __volatile__ ("fence.i \n": : : "memory"); } while(0)

#define barrier()             __asm__ __volatile__("": : :"memory")

#endif

#endif
