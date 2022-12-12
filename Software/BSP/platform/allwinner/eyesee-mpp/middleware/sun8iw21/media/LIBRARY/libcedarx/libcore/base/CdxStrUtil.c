/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxStrUtil.c
 * Description : StrUtil
 * History :
 *
 */

#include <CdxStrUtil.h>

#include <string.h>
#include <ctype.h>

#include <CdxMemory.h>
#include <cdx_log.h>
#include <CdxList.h>

void CdxStrTrimTail(cdx_char *str)
{
    cdx_int32 len;
    if (!str)
    {
        return ;
    }
    len = strlen(str);
    while (len > 0 && isspace(str[--len])) {
        str[len] = '\0';
    }
    return ;
}

void CdxStrTrimHead(cdx_char *str)
{
    cdx_int32 size = 0, i = 0;
    if (!str)
    {
        return ;
    }
    size = strlen(str);

    while (i < size && isspace(str[i])) {
        i++;
    }

    if (i) {
        size -= i;
        memmove(str, str + i, size);
        str[size] = '\0';
    }
    return ;
}

void CdxStrTrim(cdx_char *str)
{
    CdxStrTrimHead(str);
    CdxStrTrimTail(str);
    return ;
}

void CdxStrTolower(cdx_char *str)
{
    cdx_int32 size = 0, i = 0;

    size = strlen(str);
    for (i = 0; i < size; i++) {
        str[i] = tolower(str[i]);
    }
    return ;
}

cdx_char *AttrbuteOfKeyInSingleParam(AwPoolT *pool, cdx_char *str, cdx_char *key)
{
    cdx_int32 strLen, keyLen;
    /* "key=val" */
    strLen = strlen(str);
    keyLen = strlen(key);
    if (strLen <= keyLen + 1)
    {
        return NULL;
    }

    if (str[keyLen] != '=')
    {
        return NULL;
    }

    if (strncasecmp(str, key, keyLen) != 0)//memcmp
    {
        return NULL;
    }
    return Pstrdup(pool, str + keyLen + 1);
}

cdx_char *CdxStrAttributeOfKey(AwPoolT *pool, const cdx_char *str, cdx_char *key, cdx_char sepa)
{
    cdx_char *value = NULL;
    cdx_char *_str, *pos;

    _str = Pstrdup(pool, str);
    CDX_FORCE_CHECK(_str);

    pos = _str;

    for (;;)
    {
        cdx_char *sepaPos;

        sepaPos = strchr(pos, sepa);

        if (sepaPos)
        {
            *sepaPos = '\0';
        }

        CdxStrTrim(pos);
        value = AttrbuteOfKeyInSingleParam(pool, pos, key);

        if (value || !sepaPos)
        {
            break;
        }
        pos = sepaPos + 1;
    }

    Pfree(pool, _str);
    return value;
}

cdx_uint32 CdxStrSplit(AwPoolT *pool, cdx_char *str, char sepa, CdxListT *itemList)
{
    cdx_char *startPos, *sepaPos;
//    cdx_uint32 strLen = strlen(str);
    cdx_uint32 itemNum = 0;
    struct CdxStrItemS *item;
    startPos = str;
    CdxListInit(itemList);

    while (1)
    {
        item = Palloc(pool, sizeof(*item));
        CDX_FORCE_CHECK(item);
        sepaPos = strchr(startPos, sepa);
        if (!sepaPos)
        {
            item->val = Pstrdup(pool, startPos);
            CdxStrTrim(item->val);
            CdxListAddTail(&item->node, itemList);
            itemNum++;
            break;
        }
        *sepaPos = '\0';
        item->val = Pstrdup(pool, startPos);
        *sepaPos = sepa;
        CdxStrTrim(item->val);
        CdxListAddTail(&item->node, itemList);
        itemNum++;
        startPos = sepaPos + 1;
    }
    return itemNum;
}

