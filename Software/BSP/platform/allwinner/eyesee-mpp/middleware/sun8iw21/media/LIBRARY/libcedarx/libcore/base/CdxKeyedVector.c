/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxKeyedVector.c
 * Description : KeyVector
 * History :
 *
 */

#include <stdlib.h>
#include <CdxTypes.h>
#include <cdx_log.h>
#include <AwPool.h>

#include "CdxKeyedVector.h"
CdxKeyedVectorT *CdxKeyedVectorCreate(int num)
{
    CdxKeyedVectorT *p;

    if (num <= 0)
        return NULL;

    p = calloc(1, sizeof(*p) + num * sizeof(KeyValuePairT));
    if (p == NULL)
        return NULL;

    p->size = num;
    p->index = -1;
    return p;
}

void CdxKeyedVectorDestroy(CdxKeyedVectorT *keyedVector)
{
    int i;

    if (keyedVector == NULL)
        return;

    for (i = 0; i <= keyedVector->index; i++)
    {
        free(keyedVector->item[i].key);
        free(keyedVector->item[i].val);
    }
    free(keyedVector);
}

int CdxKeyedVectorAdd(CdxKeyedVectorT *keyedVector,
        const char *key, const char *val)
{
    if (key == NULL && val == NULL)
        return 0;

    if (keyedVector->index >= keyedVector->size - 1)
    {
        CDX_LOGW("keyedVector is full");
        return -1;
    }

    int index = keyedVector->index + 1;
    if (key != NULL)
    {
        keyedVector->item[index].key = strdup(key);
        if (keyedVector->item[index].key == NULL)
            goto err;
    }
    if (val != NULL)
    {
        keyedVector->item[index].val = strdup(val);
        if (keyedVector->item[index].val == NULL)
            goto err;
    }

    keyedVector->index++;
    return 0;

err:
    free(keyedVector->item[index].key);
    free(keyedVector->item[index].val);
    logw("strdup error");
    return -1;
}

int CdxKeyedVectorGetSize(CdxKeyedVectorT *keyedVector)
{
    return keyedVector->size;
}

char *CdxKeyedVectorGetKey(CdxKeyedVectorT *keyedVector, int index)
{
    if (index >= 0 && index <= keyedVector->index)
        return keyedVector->item[index].key;

    return NULL;
}

char *CdxKeyedVectorGetValue(CdxKeyedVectorT *keyedVector, int index)
{
    if (index >= 0 && index <= keyedVector->index)
        return keyedVector->item[index].val;

    return NULL;
}

