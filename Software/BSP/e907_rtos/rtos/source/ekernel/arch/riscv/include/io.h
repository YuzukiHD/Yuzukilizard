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
#ifndef _ASM_RISCV_IO_H
#define _ASM_RISCV_IO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Generic IO read/write.  These perform native-endian accesses. */
#define __raw_writeb __raw_writeb
static inline void __raw_writeb(uint8_t val, volatile void *addr)
{
    asm volatile("sb %0, 0(%1)" : : "r"(val), "r"(addr));
}

#define __raw_writew __raw_writew
static inline void __raw_writew(uint16_t val, volatile void *addr)
{
    asm volatile("sh %0, 0(%1)" : : "r"(val), "r"(addr));
}

#define __raw_writel __raw_writel
static inline void __raw_writel(uint32_t val, volatile void *addr)
{
    asm volatile("sw %0, 0(%1)" : : "r"(val), "r"(addr));
}

#ifdef CONFIG_64BIT
#define __raw_writeq __raw_writeq
static inline void __raw_writeq(uint64_t val, volatile void *addr)
{
    asm volatile("sd %0, 0(%1)" : : "r"(val), "r"(addr));
}
#endif

#define __raw_readb __raw_readb
static inline uint8_t __raw_readb(const volatile void *addr)
{
    uint8_t val;

    asm volatile("lb %0, 0(%1)" : "=r"(val) : "r"(addr));
    return val;
}

#define __raw_readw __raw_readw
static inline uint16_t __raw_readw(const volatile void *addr)
{
    uint16_t val;

    asm volatile("lh %0, 0(%1)" : "=r"(val) : "r"(addr));
    return val;
}

#define __raw_readl __raw_readl
static inline uint32_t __raw_readl(const volatile void *addr)
{
    uint32_t val;

    asm volatile("lw %0, 0(%1)" : "=r"(val) : "r"(addr));
    return val;
}

#ifdef CONFIG_64BIT
#define __raw_readq __raw_readq
static inline uint64_t __raw_readq(const volatile void *addr)
{
    uint64_t val;

    asm volatile("ld %0, 0(%1)" : "=r"(val) : "r"(addr));
    return val;
}
#endif

#define mmiowb_set_pending()            do {  } while (0)
#define cpu_to_le16(val) (val)
#define cpu_to_le32(val) (val)
#define cpu_to_le64(val) (val)
#define le32_to_cpu(val) (val)
/*
 * Unordered I/O memory access primitives.  These are even more relaxed than
 * the relaxed versions, as they don't even order accesses between successive
 * operations to the I/O regions.
 */
#define readb_cpu(c)        ({ uint8_t  __r = __raw_readb(c); __r; })
#define readw_cpu(c)        ({ uint16_t __r = le16_to_cpu(__raw_readw(c)); __r; })
#define readl_cpu(c)        ({ uint32_t __r = le32_to_cpu(__raw_readl(c)); __r; })

#define writeb_cpu(v,c)     ((void)__raw_writeb((v),(c)))
#define writew_cpu(v,c)     ((void)__raw_writew((uint16_t)cpu_to_le16(v),(c)))
#define writel_cpu(v,c)     ((void)__raw_writel((uint32_t)cpu_to_le32(v),(c)))

#ifdef CONFIG_64BIT
#define readq_cpu(c)        ({ uint64_t __r = le64_to_cpu((__le64)__raw_readq(c)); __r; })
#define writeq_cpu(v,c)     ((void)__raw_writeq((uint64_t)cpu_to_le64(v),(c)))
#endif

/*
 * Relaxed I/O memory access primitives. These follow the Device memory
 * ordering rules but do not guarantee any ordering relative to Normal memory
 * accesses.  These are defined to order the indicated access (either a read or
 * write) with all other I/O memory accesses. Since the platform specification
 * defines that all I/O regions are strongly ordered on channel 2, no explicit
 * fences are required to enforce this ordering.
 */
/* FIXME: These are now the same as asm-generic */
#define __io_rbr()      do {} while (0)
#define __io_rar()      do {} while (0)
#define __io_rbw()      do {} while (0)
#define __io_raw()      do {} while (0)

#define readb_relaxed(c)    ({ uint8_t  __v; __io_rbr(); __v = readb_cpu(c); __io_rar(); __v; })
#define readw_relaxed(c)    ({ uint16_t __v; __io_rbr(); __v = readw_cpu(c); __io_rar(); __v; })
#define readl_relaxed(c)    ({ uint32_t __v; __io_rbr(); __v = readl_cpu(c); __io_rar(); __v; })

#define writeb_relaxed(v,c) ({ __io_rbw(); writeb_cpu((v),(c)); __io_raw(); })
#define writew_relaxed(v,c) ({ __io_rbw(); writew_cpu((v),(c)); __io_raw(); })
#define writel_relaxed(v,c) ({ __io_rbw(); writel_cpu((v),(c)); __io_raw(); })

#ifdef CONFIG_64BIT
#define readq_relaxed(c)    ({ uint64_t __v; __io_rbr(); __v = readq_cpu(c); __io_rar(); __v; })
#define writeq_relaxed(v,c) ({ __io_rbw(); writeq_cpu((v),(c)); __io_raw(); })
#endif

/*
 * I/O memory access primitives. Reads are ordered relative to any
 * following Normal memory access. Writes are ordered relative to any prior
 * Normal memory access.  The memory barriers here are necessary as RISC-V
 * doesn't define any ordering between the memory space and the I/O space.
 */
#define __io_br()   do {} while (0)
#define __io_ar(v)  __asm__ __volatile__ ("fence i,r" : : : "memory");
#define __io_bw()   __asm__ __volatile__ ("fence w,o" : : : "memory");
#define __io_aw()   mmiowb_set_pending()

#define readb(c)    ({ uint8_t  __v; __io_br(); __v = readb_cpu(c); __io_ar(__v); __v; })
#define readw(c)    ({ uint16_t __v; __io_br(); __v = readw_cpu(c); __io_ar(__v); __v; })
#define readl(c)    ({ uint32_t __v; __io_br(); __v = readl_cpu(c); __io_ar(__v); __v; })

#define writeb(v,c) ({ __io_bw(); writeb_cpu((v),(c)); __io_aw(); })
#define writew(v,c) ({ __io_bw(); writew_cpu((v),(c)); __io_aw(); })
#define writel(v,c) ({ __io_bw(); writel_cpu((v),(c)); __io_aw(); })

#ifdef CONFIG_64BIT
#define readq(c)    ({ uint64_t __v; __io_br(); __v = readq_cpu(c); __io_ar(__v); __v; })
#define writeq(v,c) ({ __io_bw(); writeq_cpu((v),(c)); __io_aw(); })
#endif

/*
 *  I/O port access constants.
 */
#define IO_SPACE_LIMIT      (PCI_IO_SIZE - 1)
#define PCI_IOBASE      ((void *)PCI_IO_START)

/*
 * Emulation routines for the port-mapped IO space used by some PCI drivers.
 * These are defined as being "fully synchronous", but also "not guaranteed to
 * be fully ordered with respect to other memory and I/O operations".  We're
 * going to be on the safe side here and just make them:
 *  - Fully ordered WRT each other, by bracketing them with two fences.  The
 *    outer set contains both I/O so inX is ordered with outX, while the inner just
 *    needs the type of the access (I for inX and O for outX).
 *  - Ordered in the same manner as readX/writeX WRT memory by subsuming their
 *    fences.
 *  - Ordered WRT timer reads, so udelay and friends don't get elided by the
 *    implementation.
 * Note that there is no way to actually enforce that outX is a non-posted
 * operation on RISC-V, but hopefully the timer ordering constraint is
 * sufficient to ensure this works sanely on controllers that support I/O
 * writes.
 */
#define __io_pbr()  __asm__ __volatile__ ("fence io,i"  : : : "memory");
#define __io_par(v) __asm__ __volatile__ ("fence i,ior" : : : "memory");
#define __io_pbw()  __asm__ __volatile__ ("fence iow,o" : : : "memory");
#define __io_paw()  __asm__ __volatile__ ("fence o,io"  : : : "memory");

#define inb(c)      ({ uint8_t  __v; __io_pbr(); __v = readb_cpu((void*)(PCI_IOBASE + (c))); __io_par(__v); __v; })
#define inw(c)      ({ uint16_t __v; __io_pbr(); __v = readw_cpu((void*)(PCI_IOBASE + (c))); __io_par(__v); __v; })
#define inl(c)      ({ uint32_t __v; __io_pbr(); __v = readl_cpu((void*)(PCI_IOBASE + (c))); __io_par(__v); __v; })

#define outb(v,c)   ({ __io_pbw(); writeb_cpu((v),(void*)(PCI_IOBASE + (c))); __io_paw(); })
#define outw(v,c)   ({ __io_pbw(); writew_cpu((v),(void*)(PCI_IOBASE + (c))); __io_paw(); })
#define outl(v,c)   ({ __io_pbw(); writel_cpu((v),(void*)(PCI_IOBASE + (c))); __io_paw(); })

#ifdef CONFIG_64BIT
#define inq(c)      ({ uint64_t __v; __io_pbr(); __v = readq_cpu((void*)(c)); __io_par(__v); __v; })
#define outq(v,c)   ({ __io_pbw(); writeq_cpu((v),(void*)(c)); __io_paw(); })
#endif

/*
 * Accesses from a single hart to a single I/O address must be ordered.  This
 * allows us to use the raw read macros, but we still need to fence before and
 * after the block to ensure ordering WRT other macros.  These are defined to
 * perform host-endian accesses so we use __raw instead of __cpu.
 */
#define __io_reads_ins(port, ctype, len, bfence, afence)            \
    static inline void __ ## port ## len(const volatile void *addr, \
                                         void *buffer,              \
                                         unsigned int count)        \
    {                                                               \
        bfence;                                                     \
        if (count) {                                                \
            ctype *buf = buffer;                                    \
            \
            do {                                                    \
                ctype x = __raw_read ## len(addr);                  \
                *buf++ = x;                                         \
            } while (--count);                                      \
        }                                                           \
        afence;                                                     \
    }

#define __io_writes_outs(port, ctype, len, bfence, afence)          \
    static inline void __ ## port ## len(volatile void *addr,       \
                                         const void *buffer,        \
                                         unsigned int count)        \
    {                                                               \
        bfence;                                                     \
        if (count) {                                                \
            const ctype *buf = buffer;                              \
            \
            do {                                                    \
                __raw_write ## len(*buf++, addr);                   \
            } while (--count);                                      \
        }                                                           \
        afence;                                                     \
    }

__io_reads_ins(reads,  uint8_t, b, __io_br(), __io_ar(addr))
__io_reads_ins(reads, uint16_t, w, __io_br(), __io_ar(addr))
__io_reads_ins(reads, uint32_t, l, __io_br(), __io_ar(addr))
#define readsb(addr, buffer, count) __readsb(addr, buffer, count)
#define readsw(addr, buffer, count) __readsw(addr, buffer, count)
#define readsl(addr, buffer, count) __readsl(addr, buffer, count)

__io_reads_ins(ins,  uint8_t, b, __io_pbr(), __io_par(addr))
__io_reads_ins(ins, uint16_t, w, __io_pbr(), __io_par(addr))
__io_reads_ins(ins, uint32_t, l, __io_pbr(), __io_par(addr))
#define insb(addr, buffer, count) __insb((void *)(long)addr, buffer, count)
#define insw(addr, buffer, count) __insw((void *)(long)addr, buffer, count)
#define insl(addr, buffer, count) __insl((void *)(long)addr, buffer, count)

__io_writes_outs(writes,  uint8_t, b, __io_bw(), __io_aw())
__io_writes_outs(writes, uint16_t, w, __io_bw(), __io_aw())
__io_writes_outs(writes, uint32_t, l, __io_bw(), __io_aw())
#define writesb(addr, buffer, count) __writesb(addr, buffer, count)
#define writesw(addr, buffer, count) __writesw(addr, buffer, count)
#define writesl(addr, buffer, count) __writesl(addr, buffer, count)

__io_writes_outs(outs,  uint8_t, b, __io_pbw(), __io_paw())
__io_writes_outs(outs, uint16_t, w, __io_pbw(), __io_paw())
__io_writes_outs(outs, uint32_t, l, __io_pbw(), __io_paw())
#define outsb(addr, buffer, count) __outsb((void *)(long)addr, buffer, count)
#define outsw(addr, buffer, count) __outsw((void *)(long)addr, buffer, count)
#define outsl(addr, buffer, count) __outsl((void *)(long)addr, buffer, count)

#ifdef CONFIG_64BIT
__io_reads_ins(reads, uint64_t, q, __io_br(), __io_ar(addr))
#define readsq(addr, buffer, count) __readsq(addr, buffer, count)

__io_reads_ins(ins, uint64_t, q, __io_pbr(), __io_par(addr))
#define insq(addr, buffer, count) __insq((void *)addr, buffer, count)

__io_writes_outs(writes, uint64_t, q, __io_bw(), __io_aw())
#define writesq(addr, buffer, count) __writesq(addr, buffer, count)

__io_writes_outs(outs, uint64_t, q, __io_pbr(), __io_paw())
#define outsq(addr, buffer, count) __outsq((void *)addr, buffer, count)
#endif

#endif /* _ASM_RISCV_IO_H */
