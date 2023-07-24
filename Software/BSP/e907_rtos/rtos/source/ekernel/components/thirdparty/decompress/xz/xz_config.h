#ifndef __XZ_CONFIG_H__
#define __XZ_CONFIG_H__

/*
 * most of this is copied from lib/xz/xz_private.h, we can't use their defines
 * since the boot wrapper is not built in the same environment as the rest of
 * the kernel.
 */

#include <stdint.h>
#include <stdbool.h>
#include <typedef.h>
#include <string.h>
#include "swab.h"

/*
 * min()/max() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#ifndef min
#define min(x,y) ({ \
        typeof(x) _x = (x); \
        typeof(y) _y = (y); \
        (void) (&_x == &_y);        \
        _x < _y ? _x : _y; })
#endif

#ifndef max
#define max(x,y) ({ \
        typeof(x) _x = (x); \
        typeof(y) _y = (y); \
        (void) (&_x == &_y);        \
        _x > _y ? _x : _y; })
#endif

#ifndef array_size
#define array_size(array)	(sizeof(array) / sizeof((array)[0]))
#endif

#ifndef min
#define min(a, b) ({				\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	__a < __b ? __a : __b;			\
})
#endif

#ifndef min_t
#define min_t(type, a, b) ({			\
	type __a = (a);				\
	type __b = (b);				\
	__a < __b ? __a : __b;			\
})
#endif

#ifndef max
#define max(a, b) ({				\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	__a > __b ? __a : __b;			\
})
#endif

#ifndef max_t
#define max_t(type, a, b) ({			\
	type __a = (a);				\
	type __b = (b);				\
	__a > __b ? __a : __b;			\
})
#endif

static inline uint32_t swab32p(void *p)
{
	uint32_t *q = p;

	return swab32(*q);
}

#define get_le32(p) (*((uint32_t *) (p)))
#define cpu_to_be32(x) swab32(x)
static inline u32 be32_to_cpup(const u32 *p)
{
	return swab32p((u32 *)p);
}

static inline uint32_t get_unaligned_be32(const void *p)
{
	return be32_to_cpup(p);
}

static inline void put_unaligned_be32(u32 val, void *p)
{
	*((u32 *)p) = cpu_to_be32(val);
}

#define memeq(a, b, size) (memcmp(a, b, size) == 0)
#define memzero(buf, size) memset(buf, 0, size)

/* prevent the inclusion of the xz-preboot MM headers */
#define DECOMPR_MM_H
#define memmove memmove
#define XZ_EXTERN static

/* xz.h needs to be included directly since we need enum xz_mode */
#include "xz.h"

#endif
