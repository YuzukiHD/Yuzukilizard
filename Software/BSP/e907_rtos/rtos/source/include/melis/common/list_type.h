#ifndef _LIST_TYPE_H_
#define _LIST_TYPE_H_

#ifdef __cplusplus
extern "C" {
#endif

struct list_head
{
    struct list_head *next, *prev;
};

struct hlist_head
{
    struct hlist_node *first;
};

struct hlist_node
{
    struct hlist_node *next, * *pprev;
};

#ifdef __cplusplus
}
#endif

#endif  /* _LIST_TYPE_H_ */

