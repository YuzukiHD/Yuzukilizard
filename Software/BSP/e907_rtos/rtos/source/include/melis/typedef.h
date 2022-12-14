#ifndef __MELIS_TYPEDEF__
#define __MELIS_TYPEDEF__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>

#ifndef EBASE_TRUE
#define EBASE_TRUE      0
#endif

#ifndef EBASE_FALSE
#define EBASE_FALSE     (-1)
#endif

#ifndef EBSP_TRUE
#define EBSP_TRUE       EBASE_TRUE
#endif

#ifndef EBSP_FALSE
#define EBSP_FALSE      EBASE_FALSE
#endif

#ifndef EPDK_OK
#define EPDK_OK         0
#endif

#ifndef EPDK_FAIL
#define EPDK_FAIL       (-1)
#endif

#ifndef EPDK_TRUE
#define EPDK_TRUE       1
#endif

#ifndef EPDK_FALSE
#define EPDK_FALSE      0
#endif

#ifndef EPDK_DISABLED
#define EPDK_DISABLED   0
#endif

#ifndef EPDK_ENABLED
#define EPDK_ENABLED    1
#endif

#ifndef EPDK_NO
#define EPDK_NO         0
#endif

#ifndef EPDK_YES
#define EPDK_YES        1
#endif

#ifndef EPDK_OFF
#define EPDK_OFF        0
#endif

#ifndef EPDK_ON
#define EPDK_ON         1
#endif

#ifndef EPDK_CLR
#define EPDK_CLR        0
#endif

#ifndef EPDK_SET
#define EPDK_SET        1
#endif

#ifndef NULL
#define NULL            (void*)(0)
#endif

#ifndef DATA_TYPE_X___u64
#define DATA_TYPE_X___u64
typedef uint64_t    __u64;
#endif

#ifndef DATA_TYPE_X___u32
#define DATA_TYPE_X___u32
typedef uint32_t    __u32;
#endif

#ifndef DATA_TYPE_X___u16
#define DATA_TYPE_X___u16
typedef uint16_t    __u16;
#endif

#ifndef DATA_TYPE_X___u8
#define DATA_TYPE_X___u8
typedef uint8_t     __u8;
#endif

#ifndef DATA_TYPE_X___s64
#define DATA_TYPE_X___s64
typedef int64_t     __s64;
#endif

#ifndef DATA_TYPE_X___s32
#define DATA_TYPE_X___s32
typedef int32_t     __s32;
#endif

#ifndef DATA_TYPE_X___s16
#define DATA_TYPE_X___s16
typedef int16_t     __s16;
#endif

#ifndef DATA_TYPE_X___s8
#define DATA_TYPE_X___s8
typedef int8_t      __s8;
#endif

#ifndef DATA_TYPE_X_u64
#define DATA_TYPE_X_u64
typedef uint64_t    u64;
#endif

#ifndef DATA_TYPE_X_u32
#define DATA_TYPE_X_u32
typedef uint32_t    u32;
#endif

#ifndef DATA_TYPE_X_u16
#define DATA_TYPE_X_u16
typedef uint16_t    u16;
#endif

#ifndef DATA_TYPE_X_u8
#define DATA_TYPE_X_u8
typedef uint8_t     u8;
#endif

#ifndef DATA_TYPE_X_s64
#define DATA_TYPE_X_s64
typedef int64_t     s64;
#endif

#ifndef DATA_TYPE_X_s32
#define DATA_TYPE_X_s32
typedef int32_t     s32;
#endif

#ifndef DATA_TYPE_X_s16
#define DATA_TYPE_X_s16
typedef int16_t     s16;
#endif

#ifndef DATA_TYPE_X_s8
#define DATA_TYPE_X_s8
typedef int8_t      s8;
#endif


#ifndef DATA_TYPE_X___hdle
#define DATA_TYPE_X___hdle
typedef void       *__hdle;
#endif

#ifndef DATA_TYPE_X___bool
#define DATA_TYPE_X___bool
typedef signed char __bool;
#endif

#ifndef DATA_TYPE_X___size
#define DATA_TYPE_X___size
typedef unsigned int __size;
#endif

#ifndef DATA_TYPE_X___fp32
#define DATA_TYPE_X___fp32
typedef float        __fp32;
#endif

#ifndef DATA_TYPE_X___wchar_t
#define DATA_TYPE_X___wchar_t
typedef uint16_t   __wchar_t;
#endif

#ifndef DATA_TYPE_X_uint
#define DATA_TYPE_X_uint
typedef unsigned int uint;
#endif

#ifdef __cplusplus
}
#endif

#endif

