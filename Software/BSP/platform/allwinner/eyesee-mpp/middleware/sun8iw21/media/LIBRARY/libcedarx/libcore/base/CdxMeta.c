/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMeta.c
 * Description : Meta
 * History :
 *
 */

#include <CdxMeta.h>

#include <CdxTypes.h>
#include <CdxMemory.h>
#include <CdxLock.h>
//#include <CdxBaseErrno.h>

#include <CdxAtomic.h>
#include <CdxList.h>

struct CdxMetaImplS
{
    struct CdxMetaS base;
    AwPoolT *pool;
    struct CdxListS mInt32List;
    struct CdxListS mInt64List;
    struct CdxListS mStringList;
    struct CdxListS mDataList;
    struct CdxListS mObjectList;
    cdx_atomic_t mRef;
};

struct CdxMetaItemInt32S
{
    cdx_char name[CDX_META_ITEM_NAMESIZE];
    struct CdxListNodeS node;
    cdx_int32 val;
};

struct CdxMetaItemInt64S
{
    cdx_char name[CDX_META_ITEM_NAMESIZE];
    struct CdxListNodeS node;
    cdx_int64 val;
};

struct CdxMetaItemStringS
{
    cdx_char name[CDX_META_ITEM_NAMESIZE];
    struct CdxListNodeS node;
    cdx_char *val;
};

struct CdxMetaItemDataS
{
    cdx_char name[CDX_META_ITEM_NAMESIZE];
    struct CdxListNodeS node;
    cdx_uint8 *val;
    cdx_uint32 size;
};

struct CdxMetaItemObjectS
{
    cdx_char name[CDX_META_ITEM_NAMESIZE];
    struct CdxListNodeS node;
    void *val;
};

static void __CdxMetaClear(struct CdxMetaS *meta)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemInt32S *posInt32, *nextInt32;
    struct CdxMetaItemInt64S *posInt64, *nextInt64;
    struct CdxMetaItemStringS *posString, *nextString;
    struct CdxMetaItemDataS *posData, *nextData;
    struct CdxMetaItemObjectS *posObject, *nextObject;

    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    /*int32*/
    CdxListForEachEntrySafe(posInt32, nextInt32, &impl->mInt32List, node)
    {
        CdxListDel(&posInt32->node);
        Pfree(impl->pool, posInt32);
    }
    /*int64*/
    CdxListForEachEntrySafe(posInt64, nextInt64, &impl->mInt64List, node)
    {
        CdxListDel(&posInt64->node);
        Pfree(impl->pool, posInt64);
    }
    /*string*/
    CdxListForEachEntrySafe(posString, nextString, &impl->mStringList, node)
    {
        CdxListDel(&posString->node);
        Pfree(impl->pool, posString->val);
        Pfree(impl->pool, posString);
    }
    /*data*/
    CdxListForEachEntrySafe(posData, nextData, &impl->mDataList, node)
    {
        CdxListDel(&posData->node);
        Pfree(impl->pool, posData->val);
        Pfree(impl->pool, posData);
    }
    /*object*/
    CdxListForEachEntrySafe(posObject, nextObject, &impl->mObjectList, node)
    {
        CdxListDel(&posObject->node);
        Pfree(impl->pool, posObject);
    }
    return ;
}

static struct CdxMetaS *__CdxMetaIncRef(struct CdxMetaS *meta)
{
    struct CdxMetaImplS *impl;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxAtomicInc(&impl->mRef);
    return meta;
}

static void __CdxMetaDecRef(struct CdxMetaS *meta)
{
    struct CdxMetaImplS *impl;

    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    if(CdxAtomicDec(&impl->mRef) == 0)
    {
        CdxMetaClear(meta);
        Pfree(impl->pool, impl);
    }
    return ;
}

static cdx_err __CdxMetaSetInt32(struct CdxMetaS *meta,
                                cdx_char * name, cdx_int32 val)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemInt32S *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    item = Palloc(impl->pool, sizeof(*item));
    CDX_FORCE_CHECK(item);

    item->val = val;
    strncpy(item->name, name, CDX_META_ITEM_NAMESIZE);
    CdxListAdd(&item->node, &impl->mInt32List);

    return CDX_SUCCESS;
}

static cdx_err __CdxMetaFindInt32(struct CdxMetaS *meta,
                                cdx_char * name, cdx_int32 *pVal)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemInt32S *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntry(item, &impl->mInt32List, node)
    {
        if (strcmp(item->name, name) == 0)
        {
            *pVal = item->val;
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaRemoveInt32(struct CdxMetaS *meta, cdx_char * name)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemInt32S *item, *tmpItem;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntrySafe(item, tmpItem, &impl->mInt32List, node)
    {
        if (strcmp(item->name, name) == 0)
        {
            CdxListDel(&item->node);
            Pfree(impl->pool, item);
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaSetInt64(struct CdxMetaS *meta,
                                cdx_char * name, cdx_int64 val)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemInt64S *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    item = Palloc(impl->pool, sizeof(*item));
    if (!item) {
        CDX_LOGE("malloc fail size:%u", (cdx_uint32)sizeof(*item));
        return -1;
    }

    item->val = val;
    strncpy(item->name, name, CDX_META_ITEM_NAMESIZE);
    CdxListAdd(&item->node, &impl->mInt64List);

    return CDX_SUCCESS;
}

static cdx_err __CdxMetaFindInt64(struct CdxMetaS *meta,
                                cdx_char * name, cdx_int64 *pVal)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemInt64S *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntry(item, &impl->mInt64List, node) {
        if (strcmp(item->name, name) == 0) {
            *pVal = item->val;
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaRemoveInt64(struct CdxMetaS *meta, cdx_char * name)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemInt64S *item, *tmpItem;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntrySafe(item, tmpItem, &impl->mInt64List, node) {
        if (strcmp(item->name, name) == 0) {
            CdxListDel(&item->node);
            Pfree(impl->pool, item);
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaSetString(struct CdxMetaS *meta,
                                cdx_char * name, cdx_char *val)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemStringS *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    item = Palloc(impl->pool, sizeof(*item));
    if (!item) {
        CDX_LOGE("malloc fail size:%u", (cdx_uint32)sizeof(*item));
        return -1;
    }

    item->val = Pstrdup(impl->pool, val);
    if (!item->val) {
        CDX_LOGE("malloc fail size:%u", (cdx_uint32)strlen(val));
        return -1;
    }
    strncpy(item->name, name, CDX_META_ITEM_NAMESIZE);
    CdxListAdd(&item->node, &impl->mStringList);

    return CDX_SUCCESS;
}

static cdx_err __CdxMetaFindString(struct CdxMetaS *meta,
                                cdx_char * name, cdx_char **pVal)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemStringS *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntry(item, &impl->mStringList, node) {
        if (strcmp(item->name, name) == 0) {
            *pVal = item->val;
//            *pVal = Pstrdup(item->val);
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaAppendString(struct CdxMetaS *meta,
                                cdx_char * name, cdx_char *val)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemStringS *item;

    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntry(item, &impl->mStringList, node) {
        if (strcmp(item->name, name) == 0) {
            cdx_int32 newStrLen = 0;
            newStrLen = strlen(item->val) + strlen(val);
            item->val = Prealloc(impl->pool, item->val, newStrLen + 1);
            strcat(item->val, val);
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaRemoveString(struct CdxMetaS *meta, cdx_char * name)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemStringS *item, *tmpItem;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntrySafe(item, tmpItem, &impl->mStringList, node) {
        if (strcmp(item->name, name) == 0) {
            CdxListDel(&item->node);
            Pfree(impl->pool, item->val);
            Pfree(impl->pool, item);
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaSetData(struct CdxMetaS *meta, cdx_char * name,
                                cdx_uint8 *val, cdx_uint32 size)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemDataS *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    item = Palloc(impl->pool, sizeof(*item));
    CDX_FORCE_CHECK(item);

    item->val = Palloc(impl->pool, size);
    CDX_FORCE_CHECK(item->val);

    memcpy(item->val, val, size);
    item->size = size;

    strncpy(item->name, name, CDX_META_ITEM_NAMESIZE);
    CdxListAdd(&item->node, &impl->mDataList);

    return CDX_SUCCESS;
}

static cdx_err __CdxMetaFindData(struct CdxMetaS *meta, cdx_char * name,
                                    cdx_uint8 **pVal, cdx_uint32 *pSize)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemDataS *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntry(item, &impl->mDataList, node)
    {
        if (strcmp(item->name, name) == 0)
        {
            *pVal = item->val;
            *pSize = item->size;
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaRemoveData(struct CdxMetaS *meta, cdx_char * name)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemDataS *item, *tmpItem;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntrySafe(item, tmpItem, &impl->mDataList, node)
    {
        if (strcmp(item->name, name) == 0)
        {
            CdxListDel(&item->node);
            Pfree(impl->pool, item->val);
            Pfree(impl->pool, item);
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaSetObject(struct CdxMetaS *meta, cdx_char *name, void *val)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemObjectS *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    item = Palloc(impl->pool, sizeof(*item));
    if (!item) {
        CDX_LOGE("malloc fail size:%u", (cdx_uint32)sizeof(*item));
        return -1;
    }

    item->val = val;
    strncpy(item->name, name, CDX_META_ITEM_NAMESIZE);
    CdxListAdd(&item->node, &impl->mObjectList);

    return CDX_SUCCESS;
}

static cdx_err __CdxMetaFindObject(struct CdxMetaS *meta,
                                cdx_char *name, void **pVal)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemObjectS *item;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntry(item, &impl->mObjectList, node) {
        if (strcmp(item->name, name) == 0) {
            *pVal = item->val;
            return CDX_SUCCESS;
        }
    }
    return -1;
}

static cdx_err __CdxMetaRemoveObject(struct CdxMetaS *meta, cdx_char *name)
{
    struct CdxMetaImplS *impl;
    struct CdxMetaItemObjectS *item, *tmpItem;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    CdxListForEachEntrySafe(item, tmpItem, &impl->mObjectList, node) {
        if (strcmp(item->name, name) == 0) {
            CdxListDel(&item->node);
            Pfree(impl->pool, item);
            return CDX_SUCCESS;
        }
    }
    return -1;
}

struct CdxMetaS *__CdxMetaDup(struct CdxMetaS *meta)
{
    struct CdxMetaS *newMeta;
    struct CdxMetaImplS *impl;
    struct CdxMetaItemInt32S *posInt32;
    struct CdxMetaItemInt64S *posInt64;
    struct CdxMetaItemStringS *posString;
    struct CdxMetaItemDataS *posData;
    struct CdxMetaItemObjectS *posObject;

    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);

    newMeta = CdxMetaCreate(impl->pool);

    /*int32*/
    CdxListForEachEntry(posInt32, &impl->mInt32List, node)
    {
        CdxMetaSetInt32(newMeta, posInt32->name, posInt32->val);
    }
    /*int64*/
    CdxListForEachEntry(posInt64, &impl->mInt64List, node)
    {
        CdxMetaSetInt64(newMeta, posInt64->name, posInt64->val);
    }
    /*string*/
    CdxListForEachEntry(posString, &impl->mStringList, node)
    {
        CdxMetaSetString(newMeta, posString->name, posString->val);
    }
    /*data*/
    CdxListForEachEntry(posData, &impl->mDataList, node)
    {
        CdxMetaSetData(newMeta, posData->name, posData->val, posData->size);
    }
    /*object*/
    CdxListForEachEntry(posObject, &impl->mObjectList, node)
    {
        CdxMetaSetObject(newMeta, posObject->name, posObject->val);
    }
    return newMeta;
}

struct CdxMetaOpsS gMetaOps = {
    .incRef = __CdxMetaIncRef,
    .decRef = __CdxMetaDecRef,

    .setInt32 = __CdxMetaSetInt32,
    .findInt32 = __CdxMetaFindInt32,
    .removeInt32 = __CdxMetaRemoveInt32,

    .setInt64 = __CdxMetaSetInt64,
    .findInt64 = __CdxMetaFindInt64,
    .removeInt64 = __CdxMetaRemoveInt64,

    .setString = __CdxMetaSetString,
    .findString = __CdxMetaFindString,
    .appendString = __CdxMetaAppendString,
    .removeString = __CdxMetaRemoveString,

    .setObject = __CdxMetaSetObject,
    .findObject = __CdxMetaFindObject,
    .removeObject = __CdxMetaRemoveObject,

    .setData = __CdxMetaSetData,
    .findData = __CdxMetaFindData,
    .removeData = __CdxMetaRemoveData,

    .dup = __CdxMetaDup,
    .clear = __CdxMetaClear
};

CdxMetaT *__CdxMetaCreate(AwPoolT *pool, char *file, int line)
{
    struct CdxMetaImplS *impl;
    char *callFrom = file;

#ifdef MEMORY_LEAK_CHK
    callFrom = malloc(512);
    sprintf(callFrom, "%s:%s", file, __FUNCTION__);
#endif

    impl = AwPalloc(pool, sizeof(struct CdxMetaImplS), callFrom, line);
    CDX_FORCE_CHECK(impl);

    memset(impl, 0x00, sizeof(struct CdxMetaImplS));
    impl->pool = pool;
    CdxListInit(&impl->mInt32List);
    CdxListInit(&impl->mInt64List);
    CdxListInit(&impl->mStringList);
    CdxListInit(&impl->mDataList);
    CdxListInit(&impl->mObjectList);
    CdxAtomicSet(&impl->mRef, 1);
    impl->base.ops = &gMetaOps;

    return &impl->base;
}

void CdxMetaDestroy(CdxMetaT  *meta)
{
    struct CdxMetaImplS *impl;
    CDX_CHECK(meta);
    impl = CdxContainerOf(meta, struct CdxMetaImplS, base);
    CDX_CHECK(CdxAtomicRead(&impl->mRef) == 1);
    __CdxMetaDecRef(meta);
    return ;
}
