#ifndef LIST_H
#define LIST_H

#ifdef __cplusplus
extern "C" {
#endif
typedef struct node
{
    void *val;
    struct node *next;
    struct node *prev;
} node;

typedef struct list
{
    int size;
    node *front;
    node *back;
} list_;

list_ *get_paths(const char *filename);
list_ *make_list();
void list_insert(list_ *, void *);
void free_list(list_ *l);
//void free_list_contents(list_ *l);
void **list_to_array(list_ *l);
#ifdef __cplusplus
}
#endif

#endif
