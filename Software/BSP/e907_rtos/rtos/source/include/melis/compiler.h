#ifndef _COMPILER_H
#define _COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

#define OPTIMILIZE_O0_LEVEL   __attribute__((optimize("O0")))
#define OPTIMILIZE_O1_LEVEL   __attribute__((optimize("O1")))
#define OPTIMILIZE_O2_LEVEL   __attribute__((optimize("O2")))
#define OPTIMILIZE_O3_LEVEL   __attribute__((optimize("O3")))
#define OPTIMILIZE_Os_LEVEL   __attribute__((optimize("Os")))

#define noinline              __attribute__((noinline))

#define __used                __attribute__((__used__))
#define __deprecated          __attribute__((deprecated))

#define likely(x)             __builtin_expect((long)!!(x), 1L)
#define unlikely(x)           __builtin_expect((long)!!(x), 0L)

#define CODE_UNREACHABLE      __builtin_unreachable()
#define FUNC_NORETURN         __attribute__((__noreturn__))

#ifndef __packed
#define __packed              __attribute__((__packed__))
#endif
#ifndef __aligned
#define __aligned(x)          __attribute__((__aligned__(x)))
#endif
#define __may_alias           __attribute__((__may_alias__))
#ifndef __printf_like
#define __printf_like(f, a)   __attribute__((format (printf, f, a)))
#endif

#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#define ARG_UNUSED(NAME) NAME ATTRIBUTE_UNUSED

#ifdef __cplusplus
}
#endif

#endif
