/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxBitReader.c
 * Description : BitReader
 * History :
 *
 */

#include <CdxBitReader.h>
#include <CdxMemory.h>
#include <cdx_log.h>

struct CdxBitReaderImplS
{
    struct CdxBitReaderS base;
    const cdx_uint8 *mData;
    cdx_uint32 mSize;
    cdx_uint32 mReservoir;  // left-aligned bits
    cdx_uint32 mNumBitsLeft;
};

void CdxBitReaderDestroy(struct CdxBitReaderS *br)
{
    CDX_CHECK(br);
    struct CdxBitReaderImplS *impl = NULL;
    impl = CdxContainerOf(br, struct CdxBitReaderImplS, base);
    free(impl);
    return ;
}

static void onFillReservoir(struct CdxBitReaderImplS *impl)
{
    cdx_uint32 i;

    CDX_CHECK(impl->mSize > 0u);

    impl->mReservoir = 0;
    for (i = 0; impl->mSize > 0 && i < 4; ++i) {
        impl->mReservoir = (impl->mReservoir << 8) | *(impl->mData);

        ++(impl->mData);
        --(impl->mSize);
    }

    impl->mNumBitsLeft = 8 * i;
    impl->mReservoir <<= 32 - impl->mNumBitsLeft;
    return ;
}

cdx_uint32 CdxBitReaderGetBits(struct CdxBitReaderS *br, cdx_uint32 n)
{
    CDX_CHECK(br);
    struct CdxBitReaderImplS *impl = NULL;
    impl = CdxContainerOf(br, struct CdxBitReaderImplS, base);

    cdx_uint32 result = 0;
    CDX_CHECK(n <= 32u);

    while (n > 0) {
        cdx_uint32 m;
        if (impl->mNumBitsLeft == 0) {
            onFillReservoir(impl);
        }

        m = n;
        if (m > impl->mNumBitsLeft) {
            m = impl->mNumBitsLeft;
        }

        result = (result << m) | (impl->mReservoir >> (32 - m));
        impl->mReservoir <<= m;
        impl->mNumBitsLeft -= m;

        n -= m;
    }

    return result;
}

void CdxBitReaderSkipBits(struct CdxBitReaderS *br, cdx_uint32 n)
{
    CDX_CHECK(br);
    while (n > 32)
    {
        CdxBitReaderGetBits(br, 32);
        n -= 32;
    }

    if (n > 0)
    {
        CdxBitReaderGetBits(br, n);
    }
}

void CdxBitReaderPutBits(struct CdxBitReaderS *br, cdx_uint32 x, cdx_uint32 n)
{
    CDX_CHECK(br);
    struct CdxBitReaderImplS *impl = NULL;
    impl = CdxContainerOf(br, struct CdxBitReaderImplS, base);

    CDX_CHECK(n < 32u);

    while (impl->mNumBitsLeft + n > 32) {
        impl->mNumBitsLeft -= 8;
        --impl->mData;
        ++impl->mSize;
    }

    impl->mReservoir = (impl->mReservoir >> n) | (x << (32 - n));
    impl->mNumBitsLeft += n;
}

cdx_uint32 CdxBitReaderNumBitsLeft(struct CdxBitReaderS *br)
{
    CDX_CHECK(br);
    struct CdxBitReaderImplS *impl = NULL;
    impl = CdxContainerOf(br, struct CdxBitReaderImplS, base);

    return impl->mSize * 8 + impl->mNumBitsLeft;
}

const cdx_uint8 *CdxBitReaderData(struct CdxBitReaderS *br)
{
    CDX_CHECK(br);
    struct CdxBitReaderImplS *impl = NULL;
    impl = CdxContainerOf(br, struct CdxBitReaderImplS, base);

    return impl->mData - (impl->mNumBitsLeft + 7) / 8;
}

static struct CdxBitReaderOpsS gBitReaderOps =
{
    .destroy = CdxBitReaderDestroy,
    .getBits = CdxBitReaderGetBits,
    .skipBits = CdxBitReaderSkipBits,
    .putBits = CdxBitReaderPutBits,
    .numBitsLeft = CdxBitReaderNumBitsLeft,
    .data = CdxBitReaderData
};

CdxBitReaderT *CdxBitReaderCreate(const cdx_uint8 *data, cdx_uint32 size)
{
    struct CdxBitReaderImplS *impl;
    impl = malloc(sizeof(*impl));
    CDX_FORCE_CHECK(impl);
    impl->mData = data;
    impl->mSize = size;
    impl->mReservoir = 0;
    impl->mNumBitsLeft = 0;
    impl->base.ops = &gBitReaderOps;
    return &impl->base;
}

