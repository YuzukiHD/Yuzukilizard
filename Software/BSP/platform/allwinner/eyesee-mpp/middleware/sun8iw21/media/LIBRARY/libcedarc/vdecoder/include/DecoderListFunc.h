/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : DecoderListFunc.h
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

CDX_INTERFACE void __DecoderListAdd(struct DecoderListNodeS *newList,
                        struct DecoderListNodeS *prev, struct DecoderListNodeS *next)
{
    next->prev = newList;
    newList->next = next;
    newList->prev = prev;
    prev->next = newList;
}

CDX_INTERFACE void DecoderListAdd(struct DecoderListNodeS *newList, struct DecoderListS *list)
{
    __DecoderListAdd(newList, (struct DecoderListNodeS *)list, list->head);
}

CDX_INTERFACE void DecoderListAddBefore(struct DecoderListNodeS *newList,
                                    struct DecoderListNodeS *pos)
{
    __DecoderListAdd(newList, pos->prev, pos);
}

CDX_INTERFACE void DecoderListAddAfter(struct DecoderListNodeS *newList,
                                    struct DecoderListNodeS *pos)
{
    __DecoderListAdd(newList, pos, pos->next);
}

CDX_INTERFACE void DecoderListAddTail(struct DecoderListNodeS *newList, struct DecoderListS *list)
{
    __DecoderListAdd(newList, list->tail, (struct DecoderListNodeS *)list);
}

CDX_INTERFACE void __DecoderListDel(struct DecoderListNodeS *prev, struct DecoderListNodeS *next)
{
    next->prev = prev;
    prev->next = next;
}

CDX_INTERFACE void DecoderListDel(struct DecoderListNodeS *node)
{
    __DecoderListDel(node->prev, node->next);
    node->next = CDX_LIST_POISON1;
    node->prev = CDX_LIST_POISON2;
}

CDX_INTERFACE int DecoderListEmpty(const struct DecoderListS *list)
{
    return (list->head == (struct DecoderListNodeS *)list)
           && (list->tail == (struct DecoderListNodeS *)list);
}

