#ifndef _LINUX_BITREV_H
#define _LINUX_BITREV_H

#include <stdint.h>

extern uint8_t const byte_rev_table[256];

static inline uint8_t __bitrev8(uint8_t byte)
{
    return byte_rev_table[byte];
}

static inline uint16_t __bitrev16(uint16_t x)
{
    return (__bitrev8(x & 0xff) << 8) | __bitrev8(x >> 8);
}

static inline uint32_t __bitrev32(uint32_t x)
{
    return (__bitrev16(x & 0xffff) << 16) | __bitrev16(x >> 16);
}

#define __constant_bitrev32(x)  \
    ({                  \
        uint32_t ___x = x;          \
        ___x = (___x >> 16) | (___x << 16); \
        ___x = ((___x & (uint32_t)0xFF00FF00UL) >> 8) | ((___x & (uint32_t)0x00FF00FFUL) << 8); \
        ___x = ((___x & (uint32_t)0xF0F0F0F0UL) >> 4) | ((___x & (uint32_t)0x0F0F0F0FUL) << 4); \
        ___x = ((___x & (uint32_t)0xCCCCCCCCUL) >> 2) | ((___x & (uint32_t)0x33333333UL) << 2); \
        ___x = ((___x & (uint32_t)0xAAAAAAAAUL) >> 1) | ((___x & (uint32_t)0x55555555UL) << 1); \
        ___x;                               \
    })

#define __constant_bitrev16(x)  \
    ({                  \
        uint16_t ___x = x;          \
        ___x = (___x >> 8) | (___x << 8);   \
        ___x = ((___x & (uint16_t)0xF0F0U) >> 4) | ((___x & (uint16_t)0x0F0FU) << 4);   \
        ___x = ((___x & (uint16_t)0xCCCCU) >> 2) | ((___x & (uint16_t)0x3333U) << 2);   \
        ___x = ((___x & (uint16_t)0xAAAAU) >> 1) | ((___x & (uint16_t)0x5555U) << 1);   \
        ___x;                               \
    })

#define __constant_bitrev8(x)   \
    ({                  \
        uint8_t ___x = x;           \
        ___x = (___x >> 4) | (___x << 4);   \
        ___x = ((___x & (uint8_t)0xCCU) >> 2) | ((___x & (uint8_t)0x33U) << 2); \
        ___x = ((___x & (uint8_t)0xAAU) >> 1) | ((___x & (uint8_t)0x55U) << 1); \
        ___x;                               \
    })

#define bitrev32(x) \
    ({          \
        uint32_t __x = x;   \
        __builtin_constant_p(__x) ? \
        __constant_bitrev32(__x) :          \
        __bitrev32(__x);                \
    })

#define bitrev16(x) \
    ({          \
        uint16_t __x = x;   \
        __builtin_constant_p(__x) ? \
        __constant_bitrev16(__x) :          \
        __bitrev16(__x);                \
    })

#define bitrev8(x) \
    ({          \
        uint8_t __x = x;    \
        __builtin_constant_p(__x) ? \
        __constant_bitrev8(__x) :           \
        __bitrev8(__x)  ;           \
    })
#endif /* _LINUX_BITREV_H */
