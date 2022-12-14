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
#ifndef _ASM_RISCV_CMPXCHG_H
#define _ASM_RISCV_CMPXCHG_H

#define RISCV_ACQUIRE_BARRIER           "\tfence r , rw\n"
#define RISCV_RELEASE_BARRIER           "\tfence rw,  w\n"
#define BUILD_BUG(...)                  do {  } while(0)
#define BUILD_BUG_ON(...)               do {  } while(0)

#define __xchg_relaxed(ptr, new, size)                     \
    ({                                                     \
        __typeof__(ptr) __ptr = (ptr);                     \
        __typeof__(new) __new = (new);                     \
        __typeof__(*(ptr)) __ret;                          \
        switch (size) {                                    \
            case 4:                                        \
                __asm__ __volatile__ (                     \
                        "amoswap.w %0, %2, %1\n"           \
                        : "=r" (__ret), "+A" (*__ptr)      \
                        : "r" (__new)                      \
                        : "memory");                       \
                break;                                     \
            case 8:                                        \
                __asm__ __volatile__ (                     \
                        "amoswap.d %0, %2, %1\n"           \
                        : "=r" (__ret), "+A" (*__ptr)      \
                        : "r" (__new)                      \
                        : "memory");                       \
                break;                                     \
            default:                                       \
                BUILD_BUG();                               \
        }                                                  \
        __ret;                                             \
    })

#define xchg_relaxed(ptr, x)                               \
    ({                                                     \
        __typeof__(*(ptr)) _x_ = (x);                      \
        (__typeof__(*(ptr))) __xchg_relaxed((ptr),         \
                                            _x_, sizeof(*(ptr))); \
    })

#define __xchg_acquire(ptr, new, size)                     \
    ({                                                     \
        __typeof__(ptr) __ptr = (ptr);                     \
        __typeof__(new) __new = (new);                     \
        __typeof__(*(ptr)) __ret;                          \
        switch (size) {                                    \
            case 4:                                        \
                __asm__ __volatile__ (                     \
                        "amoswap.w %0, %2, %1\n"           \
                        RISCV_ACQUIRE_BARRIER              \
                        : "=r" (__ret), "+A" (*__ptr)      \
                        : "r" (__new)                      \
                        : "memory");                       \
                break;                                     \
            case 8:                                        \
                __asm__ __volatile__ (                     \
                        "amoswap.d %0, %2, %1\n"           \
                        RISCV_ACQUIRE_BARRIER              \
                        : "=r" (__ret), "+A" (*__ptr)      \
                        : "r" (__new)                      \
                        : "memory");                       \
                break;                                     \
            default:                                       \
                BUILD_BUG();                               \
        }                                                  \
        __ret;                                             \
    })

#define xchg_acquire(ptr, x)                               \
    ({                                                     \
        __typeof__(*(ptr)) _x_ = (x);                      \
        (__typeof__(*(ptr))) __xchg_acquire((ptr),         \
                                            _x_, sizeof(*(ptr)));   \
    })

#define __xchg_release(ptr, new, size)                     \
    ({                                                     \
        __typeof__(ptr) __ptr = (ptr);                     \
        __typeof__(new) __new = (new);                     \
        __typeof__(*(ptr)) __ret;                          \
        switch (size) {                                    \
            case 4:                                        \
                __asm__ __volatile__ (                     \
                        RISCV_RELEASE_BARRIER              \
                        "amoswap.w %0, %2, %1\n"           \
                        : "=r" (__ret), "+A" (*__ptr)      \
                        : "r" (__new)                      \
                        : "memory");                       \
                break;                                     \
            case 8:                                        \
                __asm__ __volatile__ (                     \
                        RISCV_RELEASE_BARRIER              \
                        "amoswap.d %0, %2, %1\n"           \
                        : "=r" (__ret), "+A" (*__ptr)      \
                        : "r" (__new)                      \
                        : "memory");                       \
                break;                                     \
            default:                                       \
                BUILD_BUG();                               \
        }                                                  \
        __ret;                                             \
    })

#define xchg_release(ptr, x)                               \
    ({                                                     \
        __typeof__(*(ptr)) _x_ = (x);                      \
        (__typeof__(*(ptr))) __xchg_release((ptr),         \
                                            _x_, sizeof(*(ptr)));   \
    })

#define __xchg(ptr, new, size)                             \
    ({                                                     \
        __typeof__(ptr) __ptr = (ptr);                     \
        __typeof__(new) __new = (new);                     \
        __typeof__(*(ptr)) __ret;                          \
        switch (size) {                                    \
            case 4:                                        \
                __asm__ __volatile__ (                     \
                        "amoswap.w.aqrl %0, %2, %1\n"      \
                        : "=r" (__ret), "+A" (*__ptr)      \
                        : "r" (__new)                      \
                        : "memory");                       \
                break;                                     \
            case 8:                                        \
                __asm__ __volatile__ (                     \
                        "amoswap.d.aqrl %0, %2, %1\n"      \
                        : "=r" (__ret), "+A" (*__ptr)      \
                        : "r" (__new)                      \
                        : "memory");                       \
                break;                                     \
            default:                                       \
                BUILD_BUG();                               \
        }                                                  \
        __ret;                                             \
    })

#define xchg(ptr, x)                                       \
    ({                                                     \
        __typeof__(*(ptr)) _x_ = (x);                      \
        (__typeof__(*(ptr))) __xchg((ptr), _x_, sizeof(*(ptr)));  \
    })

#define xchg32(ptr, x)                                     \
    ({                                                     \
        BUILD_BUG_ON(sizeof(*(ptr)) != 4);                 \
        xchg((ptr), (x));                                  \
    })

#define xchg64(ptr, x)                                     \
    ({                                                     \
        BUILD_BUG_ON(sizeof(*(ptr)) != 8);                 \
        xchg((ptr), (x));                                  \
    })

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
#define __cmpxchg_relaxed(ptr, old, new, size)             \
    ({                                                     \
        __typeof__(ptr) __ptr = (ptr);                     \
        __typeof__(*(ptr)) __old = (old);                  \
        __typeof__(*(ptr)) __new = (new);                  \
        __typeof__(*(ptr)) __ret;                          \
        register unsigned int __rc;                        \
        switch (size) {                                    \
            case 4:                                        \
                __asm__ __volatile__ (                     \
                        "0:lr.w %0, %2\n"                  \
                        "bne  %0, %z3, 1f\n"               \
                        "sc.w %1, %z4, %2\n"               \
                        "bnez %1, 0b\n"                    \
                        "1:\n"                             \
                        : "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr) \
                        : "rJ" (__old), "rJ" (__new)                 \
                        : "memory");                                 \
                break;                                     \
            case 8:                                        \
                __asm__ __volatile__ (                     \
                        "0:lr.d %0, %2\n"                  \
                        "bne %0, %z3, 1f\n"                \
                        "sc.d %1, %z4, %2\n"               \
                        "bnez %1, 0b\n"                    \
                        "1:\n"                             \
                        : "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr) \
                        : "rJ" (__old), "rJ" (__new)       \
                        : "memory");                       \
                break;                                     \
            default:                                       \
                BUILD_BUG();                               \
        }                                                  \
        __ret;                                             \
    })

#define cmpxchg_relaxed(ptr, o, n)                         \
    ({                                                     \
        __typeof__(*(ptr)) _o_ = (o);                      \
        __typeof__(*(ptr)) _n_ = (n);                      \
        (__typeof__(*(ptr))) __cmpxchg_relaxed((ptr),      \
                                               _o_, _n_, sizeof(*(ptr)));  \
    })

#define __cmpxchg_acquire(ptr, old, new, size)             \
    ({                                                     \
        __typeof__(ptr) __ptr = (ptr);                     \
        __typeof__(*(ptr)) __old = (old);                  \
        __typeof__(*(ptr)) __new = (new);                  \
        __typeof__(*(ptr)) __ret;                          \
        register unsigned int __rc;                        \
        switch (size) {                                    \
            case 4:                                        \
                __asm__ __volatile__ (                     \
                        "0:lr.w %0, %2\n"                  \
                        "bne  %0, %z3, 1f\n"               \
                        "sc.w %1, %z4, %2\n"               \
                        "bnez %1, 0b\n"                    \
                        RISCV_ACQUIRE_BARRIER              \
                        "1:\n"                             \
                        : "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)    \
                        : "rJ" (__old), "rJ" (__new)       \
                        : "memory");                       \
                break;                                     \
            case 8:                                        \
                __asm__ __volatile__ (                     \
                        "0:	lr.d %0, %2\n"             \
                        "	bne %0, %z3, 1f\n"         \
                        "	sc.d %1, %z4, %2\n"        \
                        "	bnez %1, 0b\n"             \
                        RISCV_ACQUIRE_BARRIER              \
                        "1:\n"                             \
                        : "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)    \
                        : "rJ" (__old), "rJ" (__new)       \
                        : "memory");                       \
                break;                                     \
            default:                                       \
                BUILD_BUG();                               \
        }                                                  \
        __ret;                                             \
    })

#define cmpxchg_acquire(ptr, o, n)                         \
    ({                                                     \
        __typeof__(*(ptr)) _o_ = (o);                      \
        __typeof__(*(ptr)) _n_ = (n);                      \
        (__typeof__(*(ptr))) __cmpxchg_acquire((ptr),      \
                                               _o_, _n_, sizeof(*(ptr)));  \
    })

#define __cmpxchg_release(ptr, old, new, size)             \
    ({                                                     \
        __typeof__(ptr) __ptr = (ptr);                     \
        __typeof__(*(ptr)) __old = (old);                  \
        __typeof__(*(ptr)) __new = (new);                  \
        __typeof__(*(ptr)) __ret;                          \
        register unsigned int __rc;                        \
        switch (size) {                                    \
            case 4:                                        \
                __asm__ __volatile__ (                     \
                        RISCV_RELEASE_BARRIER              \
                        "0:lr.w %0, %2\n"                  \
                        "bne  %0, %z3, 1f\n"               \
                        "sc.w %1, %z4, %2\n"               \
                        "bnez %1, 0b\n"                    \
                        "1:\n"                             \
                        : "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)    \
                        : "rJ" (__old), "rJ" (__new)       \
                        : "memory");                       \
                break;                                     \
            case 8:                                        \
                __asm__ __volatile__ (                     \
                        RISCV_RELEASE_BARRIER              \
                        "0:lr.d %0, %2\n"                  \
                        "bne %0, %z3, 1f\n"                \
                        "sc.d %1, %z4, %2\n"               \
                        "bnez %1, 0b\n"                    \
                        "1:\n"                             \
                        : "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)    \
                        : "rJ" (__old), "rJ" (__new)       \
                        : "memory");                       \
                break;                                     \
            default:                                       \
                BUILD_BUG();                               \
        }                                                  \
        __ret;                                             \
    })

#define cmpxchg_release(ptr, o, n)                         \
    ({                                                     \
        __typeof__(*(ptr)) _o_ = (o);                      \
        __typeof__(*(ptr)) _n_ = (n);                      \
        (__typeof__(*(ptr))) __cmpxchg_release((ptr),      \
                                               _o_, _n_, sizeof(*(ptr)));  \
    })

#define __cmpxchg(ptr, old, new, size)                     \
    ({                                                     \
        __typeof__(ptr) __ptr = (ptr);                     \
        __typeof__(*(ptr)) __old = (old);                  \
        __typeof__(*(ptr)) __new = (new);                  \
        __typeof__(*(ptr)) __ret;                          \
        register unsigned int __rc;                        \
        switch (size) {                                    \
            case 4:                                        \
                __asm__ __volatile__ (                     \
                        "0:lr.w %0, %2\n"                  \
                        "bne  %0, %z3, 1f\n"               \
                        "sc.w.rl %1, %z4, %2\n"            \
                        "bnez %1, 0b\n"                    \
                        "fence rw, rw\n"                   \
                        "1:\n"                             \
                        : "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)    \
                        : "rJ" (__old), "rJ" (__new)       \
                        : "memory");                       \
                break;                                     \
            case 8:                                        \
                __asm__ __volatile__ (                     \
                        "0:lr.d %0, %2\n"                  \
                        "bne %0, %z3, 1f\n"                \
                        "sc.d.rl %1, %z4, %2\n"            \
                        "bnez %1, 0b\n"                    \
                        "fence rw, rw\n"                   \
                        "1:\n"                             \
                        : "=&r" (__ret), "=&r" (__rc), "+A" (*__ptr)    \
                        : "rJ" (__old), "rJ" (__new)       \
                        : "memory");                       \
                break;                                     \
            default:                                       \
                BUILD_BUG();                               \
        }                                                  \
        __ret;                                             \
    })

#define cmpxchg(ptr, o, n)                                 \
    ({                                                     \
        __typeof__(*(ptr)) _o_ = (o);                      \
        __typeof__(*(ptr)) _n_ = (n);                      \
        (__typeof__(*(ptr))) __cmpxchg((ptr),              \
                                       _o_, _n_, sizeof(*(ptr))); \
    })

#define cmpxchg_local(ptr, o, n)                    \
    (__cmpxchg_relaxed((ptr), (o), (n), sizeof(*(ptr))))

#define cmpxchg32(ptr, o, n)                        \
    ({                                              \
        BUILD_BUG_ON(sizeof(*(ptr)) != 4);          \
        cmpxchg((ptr), (o), (n));                   \
    })

#define cmpxchg32_local(ptr, o, n)                  \
    ({                                              \
        BUILD_BUG_ON(sizeof(*(ptr)) != 4);          \
        cmpxchg_relaxed((ptr), (o), (n))            \
    })

#define cmpxchg64(ptr, o, n)                        \
    ({                                              \
        BUILD_BUG_ON(sizeof(*(ptr)) != 8);          \
        cmpxchg((ptr), (o), (n));                   \
    })

#define cmpxchg64_local(ptr, o, n)                  \
    ({                                              \
        BUILD_BUG_ON(sizeof(*(ptr)) != 8);          \
        cmpxchg_relaxed((ptr), (o), (n));           \
    })

#endif /* _ASM_RISCV_CMPXCHG_H */
