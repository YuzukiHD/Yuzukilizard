/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : DecoderList.h
* Description :
* Cedarx framework.
* Copyright (c) 2008-2015 Allwinner Technology Co. Ltd.
* Copyright (c) 2014 BZ Chen <bzchen@allwinnertech.com>
*
* This file is part of Cedarx.
*
* Cedarx is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.

* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef CDX_LIST_H
#define CDX_LIST_H
#include "DecoderTypes.h"

#define CDX_LIST_POISON1  ((void *) 0x00700700)
#define CDX_LIST_POISON2  ((void *) 0x00900900)

static inline void DecoderPrefetchIon(const void *x)
{
    (void)x;
}

struct DecoderListNodeS
{
    struct DecoderListNodeS *next;
    struct DecoderListNodeS *prev;
};

struct DecoderListS
{
    struct DecoderListNodeS *head;
    struct DecoderListNodeS *tail;
};

#define DecoderListInit(list) do { \
    (list)->head = (list)->tail = (struct DecoderListNodeS *)(list);\
    }while (0)

#define DecoderListNodeInit(node) do { \
    (node)->next = (node)->prev = (node);\
    }while (0)

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef AWP_DEBUG

void DecoderListAdd(struct DecoderListNodeS *newList, struct DecoderListS *list);

void DecoderListAddBefore(struct DecoderListNodeS *newList, struct DecoderListNodeS *pos);

void DecoderListAddAfter(struct DecoderListNodeS *newList, struct DecoderListNodeS *pos);

void DecoderListAddTail(struct DecoderListNodeS *newList, struct DecoderListS *list);

void DecoderListDel(struct DecoderListNodeS *node);

int DecoderListEmpty(const struct DecoderListS *list);

#else
#include <DecoderListFunc.h>
#endif

#ifdef __cplusplus
}
#endif

#define DecoderListEntry(ptr, type, member) \
    DecoderContainerOf(ptr, type, member)

#define DecoderListFirstEntry(ptr, type, member) \
    DecoderListEntry((ptr)->head, type, member)

#define DecoderListForEach(pos, list) \
    for (pos = (list)->head; \
            pos != (struct DecoderListNodeS *)(list);\
            pos = pos->next)

#define DecoderListForEachPrev(pos, list) \
    for (pos = (list)->tail; \
        pos != (struct DecoderListNodeS *)(list); \
        pos = pos->prev)

#define DecoderListForEachSafe(pos, n, list) \
    for (pos = (list)->head, n = pos->next; \
        pos != (struct DecoderListNodeS *)(list); \
        pos = n, n = pos->next)

#define DecoderListForEachSafeIon(pos, n, list) \
        for (pos = (list)->next, n = pos->next; pos != (list); \
        pos = n, n = pos->next)

#define DecoderListForEachPrevSafe(pos, n, list) \
    for (pos = (list)->tail, n = pos->prev; \
         pos != (struct DecoderListNodeS *)(list); \
         pos = n, n = pos->prev)

#define DecoderListForEachEntry(pos, list, member)                \
    for (pos = DecoderListEntry((list)->head, typeof(*pos), member);    \
         &pos->member != (struct DecoderListNodeS *)(list);     \
         pos = DecoderListEntry(pos->member.next, typeof(*pos), member))

#define DecoderListForEachEntryIon(pos, list, member) \
for (pos = DecoderListEntry((list)->next, typeof(*pos), member); \
     DecoderPrefetchIon(pos->member.next), &pos->member != (list);  \
     pos = DecoderListEntry(pos->member.next, typeof(*pos), member))

#define DecoderListForEachEntryReverse(pos, list, member)            \
    for (pos = DecoderListEntry((list)->tail, typeof(*pos), member);    \
         &pos->member != (struct DecoderListNodeS *)(list);     \
         pos = DecoderListEntry(pos->member.prev, typeof(*pos), member))

#define DecoderListForEachEntrySafe(pos, n, list, member)            \
    for (pos = DecoderListEntry((list)->head, typeof(*pos), member),    \
        n = DecoderListEntry(pos->member.next, typeof(*pos), member);    \
         &pos->member != (struct DecoderListNodeS *)(list);                     \
         pos = n, n = DecoderListEntry(n->member.next, typeof(*n), member))

#define DecoderListForEachEntrySafeReverse(pos, n, list, member)        \
    for (pos = DecoderListEntry((list)->prev, typeof(*pos), member),    \
        n = DecoderListEntry(pos->member.prev, typeof(*pos), member);    \
         &pos->member != (struct DecoderListNodeS *)(list);                     \
         pos = n, n = DecoderListEntry(n->member.prev, typeof(*n), member))

#endif
