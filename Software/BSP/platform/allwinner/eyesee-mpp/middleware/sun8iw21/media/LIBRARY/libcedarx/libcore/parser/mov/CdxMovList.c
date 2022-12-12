/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMovList.c
 * Description :
 * History :
 *
 */

//#define LOG_TAG "CdxMovList"
#include "CdxMovList.h"

typedef struct cdxtagIS
{
    struct cdxtagIS     *pNext;
    void*             data;
}ItemSlot;

struct cdx_tag_array
{
    struct cdxtagIS     *pHead;
    struct cdxtagIS     *pTail;
    unsigned int     mEntryCount;       // number of ItemSlot in the list
    int             mFoundEntryNumber;
    struct cdxtagIS     *pFoundEntry;
};

void struct_list_free(AW_List* list, void(*_free)(void*))
{
    if(!list)    return;

    while(aw_list_count(list))
    {
        void* item = aw_list_last(list);
        aw_list_rem_last(list);
        if(item && _free) _free(item);
    }

    aw_list_del(list);
}

AW_List* aw_list_new()
{
    AW_List* mlist = (AW_List*)malloc(sizeof(AW_List));
    if(!mlist) return NULL;

    mlist->pHead = mlist->pFoundEntry = NULL;
    mlist->pTail = NULL;
    mlist->mFoundEntryNumber = -1;
    mlist->mEntryCount = 0;
    return mlist;
}

int aw_list_rem(AW_List* ptr, unsigned int itemNumber)
{
    ItemSlot *tmp, *tmp2;
    if(!ptr || (!ptr->mEntryCount) || (!ptr->pHead) || (itemNumber >= ptr->mEntryCount))
        return -1;

    //delete head
    if(!itemNumber)
    {
        tmp = ptr->pHead;
        ptr->pHead = ptr->pHead->pNext;
        ptr->mEntryCount--;
        ptr->pFoundEntry = ptr->pHead;
        ptr->mFoundEntryNumber = 0;
        free(tmp);

        // that was the last entry, reset the tail
        if(!ptr->mEntryCount)
        {
            ptr->pTail = ptr->pHead = ptr->pFoundEntry = NULL;
            ptr->mFoundEntryNumber = -1;
        }
        return 0;
    }

    tmp = ptr->pHead;
    unsigned int i;
    for(i = 0; i < itemNumber-1; i++)
    {
        tmp = tmp->pNext;
    }
    tmp2 = tmp->pNext;
    tmp->pNext = tmp2->pNext;
    /*if we deleted the last entry, update the tail !!!*/
    if (! tmp->pNext || (ptr->pTail == tmp2) ) {
        ptr->pTail = tmp;
        tmp->pNext = NULL;
    }

    free(tmp2);
    ptr->mEntryCount -- ;
    ptr->pFoundEntry = ptr->pHead;
    ptr->mFoundEntryNumber = 0;

    return 0;
}

void aw_list_del(AW_List *ptr)
{
    if (!ptr) return;
    while (ptr->mEntryCount) aw_list_rem(ptr, 0);
    free(ptr);
}

void aw_list_reset(AW_List *ptr)
{
    while (ptr && ptr->mEntryCount) aw_list_rem(ptr, 0);
}

int aw_list_add(AW_List* ptr, void* item)
{
    ItemSlot *entry;
    if(!ptr)
    {
        printf(" parameter error. \n");
        return -1;
    }
    entry = (ItemSlot*)malloc(sizeof(ItemSlot));
    if(!entry) return -1;

    entry->data = item;
    entry->pNext = NULL;

    if(!ptr->pHead)
    {
        ptr->pHead = entry;
        ptr->mEntryCount = 1;
    }
    else
    {
        ptr->mEntryCount += 1;
        ptr->pTail->pNext = entry;
    }
    ptr->pTail = entry;
    ptr->mFoundEntryNumber = ptr->mEntryCount - 1;
    ptr->pFoundEntry = entry;
    return 0;
}

unsigned int aw_list_count(AW_List* ptr)
{
    if(!ptr) return -1;
    return ptr->mEntryCount;
}

void* aw_list_get(AW_List *ptr, unsigned int itemNumber)
{
    ItemSlot* entry;
    unsigned int i;
    if(!ptr || (itemNumber >= ptr->mEntryCount)) return NULL;

    //if it is the first time to get item, get the first item
    if(!ptr->pFoundEntry || (itemNumber<(unsigned int)ptr->mFoundEntryNumber))
    {
        ptr->mFoundEntryNumber = 0;
        ptr->pFoundEntry = ptr->pHead;
    }
    entry = ptr->pFoundEntry;
    for(i = ptr->mFoundEntryNumber; i< itemNumber; ++i)
    {
        entry = entry->pNext;
    }
    ptr->mFoundEntryNumber = itemNumber;
    ptr->pFoundEntry = entry;

    return (void*) entry->data;

}

void* aw_list_last(AW_List* ptr)
{
    ItemSlot* entry;

    if(!ptr || !ptr->mEntryCount) return NULL;
    entry = ptr->pHead;

    while(entry->pNext)
    {
        entry = entry->pNext;
    }

    return entry->data;
}

int aw_list_rem_last(AW_List *ptr)
{
    return aw_list_rem(ptr, ptr->mEntryCount-1);
}

int aw_list_insert(AW_List *ptr, void* item, unsigned int position)
{
    if(!ptr || !item) return -1;

    //add the item to the end of list
    if(position >= ptr->mEntryCount)
        return aw_list_add(ptr, item);

    ItemSlot *entry;
    entry = (ItemSlot*)malloc(sizeof(ItemSlot));
    entry->data = item;
    entry->pNext = NULL;

    // insert in head of list
    if (position == 0)
    {
        entry->pNext = ptr->pHead;
        ptr->pHead = entry;
        ptr->mEntryCount++;
        ptr->pFoundEntry = entry;
        ptr->mFoundEntryNumber = 0;
        return 0;
    }

    unsigned int i;
    ItemSlot* tmp = ptr->pHead;
    for(i=0; i<position-1; i++)
    {
        tmp = tmp->pNext;
    }
    entry->pNext = tmp->pNext;
    tmp->pNext = entry;
    ptr->mEntryCount++;
    ptr->pFoundEntry = entry;
    ptr->mFoundEntryNumber = i;
    return 0;
}

