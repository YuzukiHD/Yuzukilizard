/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : sunxiAllocList.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef SUNXI_ALLOC_LIST_H
#define SUNXI_ALLOC_LIST_H

#define aw_sunxi_offsetof(AWTYPE, AWMEMBER) ((size_t) &((AWTYPE *)0)->AWMEMBER)

#define container_of(aw_ptr, type, member) ( { \
const typeof( ((type *)0)->member ) *__mptr = (aw_ptr); \
(type *)( (char *)__mptr - aw_sunxi_offsetof(type,member) ); } )

static inline void aw_prefetch(const void *x) {;}
//static inline void prefetchw(const void *x) {;}

#define AW_LIST_LOCATION1  ((void *) 0x00100100)
#define AW_LIST_LOCATION2  ((void *) 0x00200200)

struct aw_mem_list_head {
struct aw_mem_list_head *aw_next, *aw_prev;
};

#define AW_MEM_LIST_HEAD_INIT(aw_name) { &(aw_name), &(aw_name) }

#define LIST_HEAD(aw_name) \
struct aw_mem_list_head aw_name = AW_MEM_LIST_HEAD_INIT(aw_name)

#define AW_MEM_INIT_LIST_HEAD(aw_ptr) do { \
(aw_ptr)->aw_next = (aw_ptr); (aw_ptr)->aw_prev = (aw_ptr); \
} while (0)

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the aw_prev/aw_next entries already!
 */
static inline void __aw_list_add(struct aw_mem_list_head *newList,
      struct aw_mem_list_head *aw_prev,
      struct aw_mem_list_head *aw_next)
{
    aw_next->aw_prev = newList;
    newList->aw_next = aw_next;
    newList->aw_prev = aw_prev;
    aw_prev->aw_next = newList;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void aw_mem_list_add(struct aw_mem_list_head *newList, struct aw_mem_list_head *head)
{
    __aw_list_add(newList, head, head->aw_next);
}

/**
 * aw_mem_list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void aw_mem_list_add_tail(struct aw_mem_list_head *newList,
        struct aw_mem_list_head *head)
{
    __aw_list_add(newList, head->aw_prev, head);
}

static inline void __aw_mem_list_del(struct aw_mem_list_head * aw_prev,
    struct aw_mem_list_head * aw_next)
{
    aw_next->aw_prev = aw_prev;
    aw_prev->aw_next = aw_next;
}

static inline void aw_mem_list_del(struct aw_mem_list_head *entry)
{
    __aw_mem_list_del(entry->aw_prev, entry->aw_next);
    entry->aw_next = AW_LIST_LOCATION1;
    entry->aw_prev = AW_LIST_LOCATION2;
}

#define aw_mem_list_entry(aw_ptr, type, member) container_of(aw_ptr, type, member)

#define aw_mem_list_for_each_safe(aw_pos, aw_n, aw_head) \
for (aw_pos = (aw_head)->aw_next, aw_n = aw_pos->aw_next; aw_pos != (aw_head); \
aw_pos = aw_n, aw_n = aw_pos->aw_next)


#define aw_mem_list_for_each_entry(aw_pos, aw_head, aw_member) \
for (aw_pos = aw_mem_list_entry((aw_head)->aw_next, typeof(*aw_pos), aw_member); \
     aw_prefetch(aw_pos->member.aw_next), &aw_pos->member != (aw_head);  \
     aw_pos = aw_mem_list_entry(aw_pos->member.aw_next, typeof(*aw_pos), aw_member))


/*
static inline void aw_mem_list_del_init(struct aw_mem_list_head *entry)
{
__aw_mem_list_del(entry->aw_prev, entry->aw_next);
AW_MEM_INIT_LIST_HEAD(entry);
}

static inline void list_move(struct aw_mem_list_head *list, struct aw_mem_list_head *head)
{
        __aw_mem_list_del(list->aw_prev, list->aw_next);
        aw_mem_list_add(list, head);
}

static inline void list_move_tail(struct aw_mem_list_head *list,
  struct aw_mem_list_head *head)
{
        __aw_mem_list_del(list->aw_prev, list->aw_next);
        aw_mem_list_add_tail(list, head);
}

static inline int list_empty(const struct aw_mem_list_head *head)
{
return head->aw_next == head;
}

static inline int list_empty_careful(const struct aw_mem_list_head *head)
{
struct aw_mem_list_head *aw_next = head->aw_next;
return (aw_next == head) && (aw_next == head->aw_prev);
}

static inline void __list_splice(struct aw_mem_list_head *list,
 struct aw_mem_list_head *head)
{
struct aw_mem_list_head *first = list->aw_next;
struct aw_mem_list_head *last = list->aw_prev;
struct aw_mem_list_head *at = head->aw_next;

first->aw_prev = head;
head->aw_next = first;

last->aw_next = at;
at->aw_prev = last;
}
*/

/**
 * list_splice - join two lists
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */

/*
static inline void list_splice(struct aw_mem_list_head *list, struct aw_mem_list_head *head)
{
if (!list_empty(list))
__list_splice(list, head);
}
*/

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised

static inline void list_splice_init(struct aw_mem_list_head *list,
    struct aw_mem_list_head *head)
{
if (!list_empty(list)) {
__list_splice(list, head);
AW_MEM_INIT_LIST_HEAD(list);
}
}
*/

//not use
/*
#define list_for_each(pos, head) \
for (pos = (head)->aw_next; prefetch(pos->aw_next), pos != (head); \
         pos = pos->aw_next)

#define __list_for_each(pos, head) \
for (pos = (head)->aw_next; pos != (head); pos = pos->aw_next)

#define list_for_each_prev(pos, head) \
for (pos = (head)->aw_prev; prefetch(pos->aw_prev), pos != (head); \
         pos = pos->aw_prev)

#define list_for_each_safe(pos, n, head) \
for (pos = (head)->aw_next, n = pos->aw_next; pos != (head); \
pos = n, n = pos->aw_next)

#define list_for_each_entry(pos, head, member) \
for (pos = list_entry((head)->aw_next, typeof(*pos), member); \
     prefetch(pos->member.aw_next), &pos->member != (head);  \
     pos = list_entry(pos->member.aw_next, typeof(*pos), member))

#define list_for_each_entry_reverse(pos, head, member) \
for (pos = list_entry((head)->aw_prev, typeof(*pos), member); \
     prefetch(pos->member.aw_prev), &pos->member != (head);  \
     pos = list_entry(pos->member.aw_prev, typeof(*pos), member))

#define list_prepare_entry(pos, head, member) \
((pos) ? : list_entry(head, typeof(*pos), member))

#define list_for_each_entry_continue(pos, head, member)  \
for (pos = list_entry(pos->member.aw_next, typeof(*pos), member); \
     prefetch(pos->member.aw_next), &pos->member != (head); \
     pos = list_entry(pos->member.aw_next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member) \
for (pos = list_entry((head)->aw_next, typeof(*pos), member), \
n = list_entry(pos->member.aw_next, typeof(*pos), member); \
     &pos->member != (head);  \
     pos = n, n = list_entry(n->member.aw_next, typeof(*n), member))

//HASH LIST
struct hlist_head {
struct hlist_node *first;
};

struct hlist_node {
struct hlist_node *aw_next, **pprev;
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = {  .first = NULL }
#define INIT_HLIST_HEAD(aw_ptr) ((aw_ptr)->first = NULL)
#define INIT_HLIST_NODE(aw_ptr) ((aw_ptr)->aw_next = NULL, (aw_ptr)->pprev = NULL)

static inline int hlist_unhashed(const struct hlist_node *h)
{
return !h->pprev;
}

static inline int hlist_empty(const struct hlist_head *h)
{
return !h->first;
}

static inline void __hlist_del(struct hlist_node *n)
{
struct hlist_node *aw_next = n->aw_next;
struct hlist_node **pprev = n->pprev;
*pprev = aw_next;
if (aw_next)
aw_next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node *n)
{
__hlist_del(n);
n->aw_next = AW_LIST_LOCATION1;
n->pprev = AW_LIST_LOCATION2;
}

static inline void hlist_del_init(struct hlist_node *n)
{
if (n->pprev)  {
__hlist_del(n);
INIT_HLIST_NODE(n);
}
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
struct hlist_node *first = h->first;
n->aw_next = first;
if (first)
first->pprev = &n->aw_next;
h->first = n;
n->pprev = &h->first;
}
*/

/* aw_next must be != NULL */
/*
static inline void hlist_add_before(struct hlist_node *n,
struct hlist_node *aw_next)
{
n->pprev = aw_next->pprev;
n->aw_next = aw_next;
aw_next->pprev = &n->aw_next;
*(n->pprev) = n;
}

static inline void hlist_add_after(struct hlist_node *n,
struct hlist_node *aw_next)
{
aw_next->aw_next = n->aw_next;
n->aw_next = aw_next;
aw_next->pprev = &n->aw_next;

if(aw_next->aw_next)
aw_next->aw_next->pprev  = &aw_next->aw_next;
}

#define hlist_entry(aw_ptr, type, member) container_of(aw_ptr,type,member)

#define hlist_for_each(pos, head) \
for (pos = (head)->first; pos && ({ prefetch(pos->aw_next); 1; }); \
     pos = pos->aw_next)

#define hlist_for_each_safe(pos, n, head) \
for (pos = (head)->first; pos && ({ n = pos->aw_next; 1; }); \
     pos = n)

#define hlist_for_each_entry(tpos, pos, head, member)  \
for (pos = (head)->first;  \
     pos && ({ prefetch(pos->aw_next); 1;}) &&  \
({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
     pos = pos->aw_next)

#define hlist_for_each_entry_continue(tpos, pos, member)  \
for (pos = (pos)->aw_next;  \
     pos && ({ prefetch(pos->aw_next); 1;}) &&  \
({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
     pos = pos->aw_next)

#define hlist_for_each_entry_from(tpos, pos, member)  \
for (; pos && ({ prefetch(pos->aw_next); 1;}) &&  \
({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
     pos = pos->aw_next)

#define hlist_for_each_entry_safe(tpos, pos, n, head, member)   \
for (pos = (head)->first;  \
     pos && ({ n = pos->aw_next; 1; }) &&   \
({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
     pos = n)
*/
#endif
