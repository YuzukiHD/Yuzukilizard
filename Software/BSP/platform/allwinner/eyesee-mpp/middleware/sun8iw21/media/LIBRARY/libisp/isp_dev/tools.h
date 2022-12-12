
/*
 ******************************************************************************
 *
 * tools.h
 *
 * Hawkview ISP - tools.h module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/05/11	VIDEO INPUT
 *
 *****************************************************************************
 */

#ifndef _TOOLS_H_
#define _TOOLS_H_

#define square(x) ((x) * (x))
#define clear(x) memset(&(x), 0, sizeof(x))

#define array_size(array) (sizeof(array) / sizeof((array)[0]))

#define min(a, b)              \
    ({                         \
        typeof(a) __a = (a);   \
        typeof(b) __b = (b);   \
        __a < __b ? __a : __b; \
    })

#define min_t(type, a, b)      \
    ({                         \
        type __a = (a);        \
        type __b = (b);        \
        __a < __b ? __a : __b; \
    })

#define max(a, b)              \
    ({                         \
        typeof(a) __a = (a);   \
        typeof(b) __b = (b);   \
        __a > __b ? __a : __b; \
    })

#define max_t(type, a, b)      \
    ({                         \
        type __a = (a);        \
        type __b = (b);        \
        __a > __b ? __a : __b; \
    })

#define clamp(val, min, max)                   \
    ({                                         \
        typeof(val) __val = (val);             \
        typeof(min) __min = (min);             \
        typeof(max) __max = (max);             \
        __val = __val < __min ? __min : __val; \
        __val > __max ? __max : __val;         \
    })

#define clamp_t(type, val, min, max)           \
    ({                                         \
        type __val = (val);                    \
        type __min = (min);                    \
        type __max = (max);                    \
        __val = __val < __min ? __min : __val; \
        __val > __max ? __max : __val;         \
    })

#define div_round_up(num, denom) (((num) + (denom)-1) / (denom))
#define div_round(num, denom) (((num) + ((denom) / 2)) / (denom))
/*
#define container_of(ptr, type, member) \
	(type *)((char *)(ptr) - offsetof(type, member))
*/
struct list_entry {
    struct list_entry *next, *prev;
};

struct hlist_entry {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *next, **pprev;
};

// #define LIST_POISON1 ((void *)0x00100100)
// #define LIST_POISON2 ((void *)0x00200200)

#endif /*_TOOLS_H_*/
