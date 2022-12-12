/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxBuffer.c
 * Description : Buffer
 * History :
 *
 */

#include <CdxTypes.h>
#include <CdxMemory.h>
#include <cdx_log.h>
#include <CdxAtomic.h>
//#include <CdxBaseErrno.h>

#include <CdxBuffer.h>
#include <CdxMeta.h>

struct CdxBufferImplS
{
    struct CdxBufferS base;

    AwPoolT *pool;
    cdx_bool selfPool;

    CdxMetaT *mMeta;
    cdx_uint8 *mData;
    cdx_uint32 mCapacity;
    cdx_uint32 mRangeOffset;
    cdx_uint32 mRangeLen;
    cdx_atomic_t mRef;
};

static cdx_uint32 Align2Power(cdx_uint32 val)
{
    cdx_uint32 ret = 1024;
    while (ret < val)
    {
       ret = ret << 2;
    }
    return ret;
}

static void onWrite(struct CdxBufferImplS *impl, cdx_uint32 offset,
                                const cdx_uint8 *data, cdx_uint32 len)
{
    if (impl->mCapacity < offset + len)
    {
        impl->mCapacity = Align2Power(offset + len);
        impl->mData = Prealloc(impl->pool, impl->mData, impl->mCapacity);
        CDX_FORCE_CHECK(impl->mData);
    }
    memcpy(impl->mData + offset, data, len);
    return ;
}

static void onSetRange(struct CdxBufferImplS *impl,
                            cdx_uint32 offset, cdx_uint32 len)
{
    CDX_CHECK(offset + len <= impl->mCapacity);
    impl->mRangeOffset = offset;
    impl->mRangeLen = len;
    return ;
}

static CdxBufferT *__CdxBufferIncRef(CdxBufferT *buf)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);
    CdxAtomicInc(&impl->mRef);
    return buf;
}

static void __CdxBufferDecRef(CdxBufferT *buf)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);
    if (CdxAtomicDec(&impl->mRef) == 0)
    {
        CdxMetaDestroy(impl->mMeta);
        Pfree(impl->pool, impl->mData);
        if (impl->selfPool)
        {
            AwPoolT *pool = impl->pool;
            Pfree(impl->pool, impl);
            AwPoolDestroy(pool);
        }
        else
        {
            Pfree(impl->pool, impl);
        }
    }
    return ;
}

static cdx_uint8 *__CdxBufferGetData(CdxBufferT *buf)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);
    return impl->mData + impl->mRangeOffset;
}

static cdx_uint8 *__CdxBufferGetBase(CdxBufferT *buf)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);
    return impl->mData;
}

static cdx_uint32 __CdxBufferGetSize(CdxBufferT *buf)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);
    return impl->mRangeLen;
}

void __CdxBufferSetRange(CdxBufferT *buf, cdx_uint32 offset, cdx_uint32 len)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);

    onSetRange(impl, offset, len);
    return ;
}

void __CdxBufferSeekRange(CdxBufferT *buf, cdx_int32 left, cdx_int32 right)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);
    impl->mRangeOffset = (cdx_uint32)((cdx_int32)impl->mRangeOffset + left);
    impl->mRangeLen = (cdx_uint32)((cdx_int32)impl->mRangeLen + (right - left));

    CDX_CHECK(impl->mRangeOffset + impl->mRangeLen <= impl->mCapacity);

    return ;
}

static void __CdxBufferAppend(CdxBufferT *buf, const cdx_uint8 *data,
                                    cdx_uint32 len)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);

    onWrite(impl, impl->mRangeOffset + impl->mRangeLen, data, len);
    onSetRange(impl, impl->mRangeOffset, impl->mRangeLen + len);
    return ;
}

static void __CdxBufferInsert(CdxBufferT *buf, const cdx_uint8 *data, cdx_uint32 len)
{
    struct CdxBufferImplS *impl = NULL;
    cdx_uint8 *tmpData = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);

    if (len <= impl->mRangeOffset) /*lucky have enough buf before range*/
    {
        impl->mRangeOffset -= len;
        impl->mRangeLen += len;
        memcpy(impl->mData + impl->mRangeOffset, data, len);
        return ;
    }

    /*not enough buf before range*/
    if (impl->mCapacity >= impl->mRangeLen + len) /*capacity is enough*/
    {
        memmove(impl->mData + len, impl->mData + impl->mRangeOffset,
                impl->mRangeLen);
        memcpy(impl->mData, data, len);
        impl->mRangeOffset = 0;
        impl->mRangeLen += len;
        return ;
    }

    impl->mCapacity = Align2Power(impl->mRangeLen + len);
    tmpData = Palloc(impl->pool, impl->mCapacity);
    CDX_FORCE_CHECK(tmpData);
    memcpy(tmpData, data, len);
    memcpy(tmpData + len, impl->mData + impl->mRangeOffset, impl->mRangeLen);
    Pfree(impl->pool, impl->mData);
    impl->mData = tmpData;
    impl->mRangeOffset = 0;
    impl->mRangeLen += len;
    return ;
}

CdxMetaT *__CdxBufferGetMeta(CdxBufferT *buf)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);
    return impl->mMeta;
}

static struct CdxBufferOpsS gBufferOps =
{
    .incRef = __CdxBufferIncRef,
    .decRef = __CdxBufferDecRef,
    .getData = __CdxBufferGetData,
    .getBase = __CdxBufferGetBase,
    .getSize = __CdxBufferGetSize,
    .append = __CdxBufferAppend,
    .insert = __CdxBufferInsert,
    .setRange = __CdxBufferSetRange,
    .seekRange = __CdxBufferSeekRange,
    .getMeta = __CdxBufferGetMeta
};

CdxBufferT *__CdxBufferCreate(AwPoolT *pool, cdx_uint32 capacity, cdx_uint8 *initData,
                            cdx_uint32 len, char *file, int line)
{
    struct CdxBufferImplS *impl = NULL;
    char *callFrom = file;
    CDX_CHECK(capacity != 0);
    CDX_CHECK(capacity >= len);

#ifdef MEMORY_LEAK_CHK
    callFrom = malloc(512);
    sprintf(callFrom, "%s:%s", file, __FUNCTION__);
#endif

    if (pool)
    {
        impl = AwPalloc(pool, sizeof(struct CdxBufferImplS), callFrom, line);
        memset(impl, 0x00, sizeof(struct CdxBufferImplS));
        impl->pool = pool;
    }
    else
    {
        AwPoolT *privPool = __AwPoolCreate(NULL, callFrom, line);
        impl = Palloc(pool, sizeof(struct CdxBufferImplS));
        memset(impl, 0x00, sizeof(struct CdxBufferImplS));

        impl->pool = privPool;
        impl->selfPool = CDX_TRUE;
    }

    CDX_FORCE_CHECK(impl);

    impl->mMeta = CdxMetaCreate(impl->pool);

    impl->mCapacity = Align2Power(capacity);
    impl->mData = Palloc(impl->pool, impl->mCapacity);
    CDX_FORCE_CHECK(impl->mData);
    impl->mRangeOffset = 0;
    impl->mRangeLen = 0;
    if (initData && (len > 0))
    {
        memcpy(impl->mData, initData, len);
        impl->mRangeLen = len;
    }
    impl->base.ops = &gBufferOps;
    CdxAtomicSet(&impl->mRef, 1);

    return &impl->base;
}

void CdxBufferDestroy(CdxBufferT *buf)
{
    struct CdxBufferImplS *impl = NULL;

    CDX_CHECK(buf);
    impl = CdxContainerOf(buf, struct CdxBufferImplS, base);

    __CdxBufferDecRef(buf);
    return ;
}

