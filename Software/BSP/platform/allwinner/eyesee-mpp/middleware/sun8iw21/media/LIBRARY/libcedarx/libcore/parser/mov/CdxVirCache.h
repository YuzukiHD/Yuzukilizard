/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxVirCache.h
 * Description :
 * History :
 *
 */

#ifndef CDX__VIRCACHE_H_
#define CDX__VIRCACHE_H_

#include "CdxMovParser.h"

#define MOV_VIR_CACHE_SIZE      (1024 * 13)

typedef struct VirCacheContext
{
    CdxStreamT *fp;
    cdx_uint32  fpos_last;

    cdx_uint32  V_STSC_OST;
    cdx_uint32  V_STCO_OST;
    cdx_uint32  V_STSZ_OST;
    cdx_uint32  V_STSS_OST;
    cdx_uint32  V_STTS_OST;

    cdx_uint32  A_STSC_OST;
    cdx_uint32  A_STCO_OST;
    cdx_uint32  A_STSZ_OST;
    cdx_uint32  A_STTS_OST;

    cdx_uint32  S_STSC_OST;
    cdx_uint32  S_STCO_OST;
    cdx_uint32  S_STSZ_OST;
    cdx_uint32  S_STTS_OST;
} VirCacheContext;

#define SWAP(X) (((X)<<24) | ((X)<<8 &0XFF0000) | ((X)>>8 &0XFF00) | ((X)>>24 &0XFF))

//#define VirCache_debug

#ifdef VirCache_debug
    int tmp;
#endif

#endif
