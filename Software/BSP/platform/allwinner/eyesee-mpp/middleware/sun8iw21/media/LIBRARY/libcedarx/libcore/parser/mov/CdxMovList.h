/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMovList.h
 * Description :
 * History :
 *
 */

#ifndef CDX_MOV_LIST_H
#define CDX_MOV_LIST_H

#ifdef __spluscplus
extern "C" {
#endif

#include "stdio.h"
#include "stdlib.h"

typedef struct cdx_tag_array AW_List;

/*!
 *    \brief list constructor
 *
 *    Constructs a new list object
 */
AW_List *aw_list_new();

/*!
 *    \brief list destructor
 *
 *    Destructs a list object
 */
void aw_list_del(AW_List *ptr);

/*!
 *    \brief get count
 *    Returns number of items in the list
 */
unsigned int aw_list_count(AW_List* ptr);

/*!
 *    \brief add item
 *
 *    Adds an item at the end of the list
 */
int aw_list_add(AW_List *ptr, void* item);

/*!
 *    Insert an item in the list
 *    \param ptr target list object
 *    \param item item to add
 *    \param position insertion position. It is expressed between 0 and gf_list_count-1,
        and any bigger value is equivalent to gf_list_add
 */
int aw_list_insert(AW_List *ptr, void *item, unsigned int position);

/*!
 *    Removes an item from the list given its position
 *    \param ptr target list object
 *    \param position position of the item to remove. It is expressed between 0 and gf_list_count-1.
 *    \note It is the caller responsability to destroy the content of the list if needed
 */
int aw_list_rem(AW_List *ptr, unsigned int position);

/*!
 *    \brief gets item
 *
 *    Gets an item from the list given its position
 *    \param ptr target list object
 *    \param position position of the item to get. It is expressed between 0 and gf_list_count-1.
 */
void *aw_list_get(AW_List *ptr, unsigned int position);

/*!
 *    \brief finds item
 *
 *    Finds an item in the list
 *    \param ptr target list object.
 *    \param item the item to find.
 *    \return 0-based item position in the list, or -1 if the item could not be found.
 */
int aw_list_find(AW_List *ptr, void *item);

/*!
 *    \brief deletes item
 *
 *    Deletes an item from the list
 *    \param ptr target list object.
 *    \param item the item to find.
 *    \return 0-based item position in the list before removal, or -1 if the item
        could not be found.
 */
int aw_list_del_item(AW_List *ptr, void *item);

/*!
 *    \brief resets list
 *
 *    Resets the content of the list
 *    \param ptr target list object.
 *    \note It is the caller responsability to destroy the content of the list if needed
 */
void aw_list_reset(AW_List *ptr);

/*!
 *    \brief gets last item
 *
 *    Gets last item o fthe list
 *    \param ptr target list object
 */
void *aw_list_last(AW_List *ptr);

/*!
 *    \brief reverses the order of the elements in the list container.
 *
 *    reverses the order of the elements in the list container.
 *    \param ptr target list object
 */
void aw_list_reverse(AW_List *ptr);

/*!
 *    \brief removes last item
 *
 *    Removes the last item of the list
 *    \param ptr target list object
 *    \note It is the caller responsability to destroy the content of the list if needed
 */
int aw_list_rem_last(AW_List *ptr);

/*!
 *    \brief list enumerator
 *
 *    Retrieves given list item and increment current position
 *    \param ptr target list object
 *    \param pos target item position. The position is automatically incremented regardless
        of the return value
 *    \note A typical enumeration will start with a value of 0 until NULL is returned.
 */
void *aw_list_enum(AW_List *ptr, unsigned int *pos);

/*!
 *    \brief list enumerator in reversed order
 *
 *    Retrieves the given list item in reversed order and update current position
 *    \param ptr target list object
 *    \param pos target item position. The position is automatically decrelented regardless
 *    \of the return value
 *    \note A typical enumeration in reversed order will start with a value of 0 until NULL
 *    \is returned.
 */
void *aw_list_rev_enum(AW_List *ptr, unsigned int *pos);

/*!
 *    \brief list swap
 *
 *    Swaps content of two lists
 *    \param l1 first list object
 *    \param l2 second list object
 */
int aw_list_swap(AW_List *l1, AW_List *l2);

/*!
 *    \brief list transfer
 *
 *    Transfer content between lists
 *    \param l1 destination list object
 *    \param l2 source list object
 */
int aw_list_transfer(AW_List *l1, AW_List *l2);

/*!
 *    \brief clone list
 *
 *    Returns a new list as an exact copy of the given list
 *    \param ptr the list to clone
 *    \return the cloned list
 */
AW_List* aw_list_clone(AW_List *ptr);

/*!
 *    \brief Pop the first element in the list
 *
 *    Removes the first element in the list container, effectively reducing its size by one
 *  and returns the popped element.
 *    \param ptr the list to pop
 *    \return the popped element
 */
void* aw_list_pop_front(AW_List *ptr);

/*!
 *    \brief Pop the last element in the list
 *
 *    Removes the last element in the list container, effectively reducing the container size by one
 *  and return the popped element.
 *    \param ptr the list to pop
 *    \return the popped element
 */
void* aw_list_pop_back(AW_List *ptr);

/*! @} */

// if the List contain a malloc structure, use the function to delete it
void struct_list_free(AW_List* list, void(*_free)(void*));

#ifdef __spluscplus
}
#endif

#endif
