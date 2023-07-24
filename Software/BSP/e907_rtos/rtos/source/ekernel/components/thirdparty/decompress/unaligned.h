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
#ifndef __COMPRESS_UNALIGNED_H__
#define __COMPRESS_UNALIGNED_H__

#include <typedef.h>

#undef  __force
#define __force
#undef  __typeof__
#define __typeof__  typeof

extern int backtrace(void);
/*
 * The ARM can do unaligned accesses itself.
 *
 * The strange macros are there to make sure these can't
 * be misused in a way that makes them not work on other
 * architectures where unaligned accesses aren't as simple.
 */
static inline void bad_unaligned_access_length(void)
{
	backtrace();
}

static inline void __bug_unaligned_x(void *ptr)
{
	backtrace();
}

/*
 * What is the most efficient way of loading/storing an unaligned value?
 *
 * That is the subject of this file.  Efficiency here is defined as
 * minimum code size with minimum register usage for the common cases.
 * It is currently not believed that long longs are common, so we
 * trade efficiency for the chars, shorts and longs against the long
 * longs.
 *
 * Current stats with gcc 2.7.2.2 for these functions:
 *
 *  ptrsize get:    code    regs    put:    code    regs
 *  1       1   1       1   2
 *  2       3   2       3   2
 *  4       7   3       7   3
 *  8       20  6       16  6
 *
 * gcc 2.95.1 seems to code differently:
 *
 *  ptrsize get:    code    regs    put:    code    regs
 *  1       1   1       1   2
 *  2       3   2       3   2
 *  4       7   4       7   4
 *  8       19  8       15  6
 *
 * which may or may not be more efficient (depending upon whether
 * you can afford the extra registers).  Hopefully the gcc 2.95
 * is inteligent enough to decide if it is better to use the
 * extra register, but evidence so far seems to suggest otherwise.
 *
 * Unfortunately, gcc is not able to optimise the high word
 * out of long long >> 32, or the low word from long long << 32
 */

#define __get_unaligned_2_le(__p)                   \
    (__p[0] | __p[1] << 8)

#define __get_unaligned_2_be(__p)                   \
    (__p[0] << 8 | __p[1])

#define __get_unaligned_4_le(__p)                   \
    (__p[0] | __p[1] << 8 | __p[2] << 16 | __p[3] << 24)

#define __get_unaligned_4_be(__p)                   \
    (__p[0] << 24 | __p[1] << 16 | __p[2] << 8 | __p[3])

#define __get_unaligned_8_le(__p)                   \
    ((unsigned long long)__get_unaligned_4_le((__p+4)) << 32 |  \
     __get_unaligned_4_le(__p))

#define __get_unaligned_8_be(__p)                   \
    ((unsigned long long)__get_unaligned_4_be(__p) << 32 |      \
     __get_unaligned_4_be((__p+4)))

#define __get_unaligned_le16(ptr) ({              \
        const char *__p = (const char *)ptr;      \
        __u64 __val = __get_unaligned_2_le(__p);  \
        (__force __typeof__(*(ptr)))__val;        \
    })

#define __get_unaligned_le32(ptr) ({              \
        const char *__p = (const char *)ptr;      \
        __u64 __val = __get_unaligned_4_le(__p);  \
        (__force __typeof__(*(ptr)))__val;        \
    })

#define __get_unaligned_le64(ptr) ({              \
        const char *__p = (const char *)ptr;      \
        __u64 __val = __get_unaligned_8_le(__p);  \
        (__force __typeof__(*(ptr)))__val;        \
    })

#define __get_unaligned_le(ptr) ({          \
        const char *__p = (const char *)ptr;    \
        __u64 __val;                            \
        switch (sizeof(*ptr)) {                 \
            case 1:                                 \
                __val = *(const __u8 *)__p;         \
                break;                              \
            case 2:                                 \
                __val = __get_unaligned_2_le(__p);  \
                break;                              \
            case 4:                                 \
                __val = __get_unaligned_4_le(__p);  \
                break;                              \
            case 8:                                 \
                __val = __get_unaligned_8_le(__p);  \
                break;                              \
            default:                                \
                bad_unaligned_access_length();      \
        };                                      \
        (__force __typeof__(*(ptr)))__val;      \
    })

#define __get_unaligned_be16(ptr) ({              \
        const char *__p = (const char *)ptr;      \
        __u64 __val = __get_unaligned_2_be(__p);  \
        (__force __typeof__(*(ptr)))__val;        \
    })

#define __get_unaligned_be32(ptr) ({              \
        const char *__p = (const char *)ptr;      \
        __u64 __val = __get_unaligned_4_be(__p);  \
        (__force __typeof__(*(ptr)))__val;        \
    })

#define __get_unaligned_be64(ptr) ({              \
        const char *__p = (const char *)ptr;      \
        __u64 __val = __get_unaligned_8_be(__p);  \
        (__force __typeof__(*(ptr)))__val;        \
    })

#define __get_unaligned_be(ptr) ({          \
        const char *__p = (const char *)ptr;    \
        __u64 __val;                            \
        switch (sizeof(*ptr)) {                 \
            case 1:                                 \
                __val = *(const __u8 *)__p;         \
                break;                              \
            case 2:                                 \
                __val = __get_unaligned_2_be(__p);  \
                break;                              \
            case 4:                                 \
                __val = __get_unaligned_4_be(__p);  \
                break;                              \
            case 8:                                 \
                __val = __get_unaligned_8_be(__p);  \
                break;                              \
            default:                                \
                bad_unaligned_access_length();      \
        };                                      \
        (__force __typeof__(*(ptr)))__val;      \
    })

static inline void __put_unaligned_2_le(__u32 __v, register __u8 *__p)
{
    *__p++ = __v;
    *__p++ = __v >> 8;
}

static inline void __put_unaligned_2_be(__u32 __v, register __u8 *__p)
{
    *__p++ = __v >> 8;
    *__p++ = __v;
}

static inline void __put_unaligned_4_le(__u32 __v, register __u8 *__p)
{
    __put_unaligned_2_le(__v >> 16, __p + 2);
    __put_unaligned_2_le(__v, __p);
}

static inline void __put_unaligned_4_be(__u32 __v, register __u8 *__p)
{
    __put_unaligned_2_be(__v >> 16, __p);
    __put_unaligned_2_be(__v, __p + 2);
}

static inline void __put_unaligned_8_le(const unsigned long long __v, register __u8 *__p)
{
    /*
     * tradeoff: 8 bytes of stack for all unaligned puts (2
     * instructions), or an extra register in the long long
     * case - go for the extra register.
     */
    __put_unaligned_4_le(__v >> 32, __p + 4);
    __put_unaligned_4_le(__v, __p);
}

static inline void __put_unaligned_8_be(const unsigned long long __v, register __u8 *__p)
{
    /*
     * tradeoff: 8 bytes of stack for all unaligned puts (2
     * instructions), or an extra register in the long long
     * case - go for the extra register.
     */
    __put_unaligned_4_be(__v >> 32, __p);
    __put_unaligned_4_be(__v, __p + 4);
}

/*
 * Try to store an unaligned value as efficiently as possible.
 */
#define __put_unaligned_le(val,ptr)                 \
    ({                          \
        (void)sizeof(*(ptr) = (val));           \
        switch (sizeof(*(ptr))) {           \
            case 1:                     \
                *(ptr) = (val);             \
                break;                  \
            case 2: __put_unaligned_2_le((__force u16)(val),(__u8 *)(ptr)); \
                break;                  \
            case 4: __put_unaligned_4_le((__force u32)(val),(__u8 *)(ptr)); \
                break;                  \
            case 8: __put_unaligned_8_le((__force u64)(val),(__u8 *)(ptr)); \
                break;                  \
            default: __bug_unaligned_x(ptr);        \
                break;                  \
        }                       \
        (void) 0;                   \
    })

#define __put_unaligned_be(val,ptr)                 \
    ({                          \
        (void)sizeof(*(ptr) = (val));           \
        switch (sizeof(*(ptr))) {           \
            case 1:                     \
                *(ptr) = (val);             \
                break;                  \
            case 2: __put_unaligned_2_be((__force u16)(val),(__u8 *)(ptr)); \
                break;                  \
            case 4: __put_unaligned_4_be((__force u32)(val),(__u8 *)(ptr)); \
                break;                  \
            case 8: __put_unaligned_8_be((__force u64)(val),(__u8 *)(ptr)); \
                break;                  \
            default: __bug_unaligned_x(ptr);        \
                break;                  \
        }                       \
        (void) 0;                   \
    })

/*
 * Select endianness
 */
#ifndef __ARMEB__
#define get_unaligned   __get_unaligned_le
#define put_unaligned   __put_unaligned_le
#else
#define get_unaligned   __get_unaligned_be
#define put_unaligned   __put_unaligned_be
#endif

#define get_unaligned_le16(ptr) (__get_unaligned_le16((__force __u16 *)(ptr)))
#define get_unaligned_le32(ptr) (__get_unaligned_le32((__force __u32 *)(ptr)))
#define get_unaligned_le64(ptr) (__get_unaligned_le64((__force __u64 *)(ptr)))

#define get_unaligned_be16(ptr) (__get_unaligned_be16((__force __u16 *)(ptr)))
#define get_unaligned_be32(ptr) (__get_unaligned_be32((__force __u32 *)(ptr)))
#define get_unaligned_be64(ptr) (__get_unaligned_be64((__force __u64 *)(ptr)))

#endif /* __FS_UNALIGNED_H__ */
