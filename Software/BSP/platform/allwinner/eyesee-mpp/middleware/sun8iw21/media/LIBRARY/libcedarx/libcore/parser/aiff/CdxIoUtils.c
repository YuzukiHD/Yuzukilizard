/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxIoUtils.c
* Description :
* History :
*   Author  : Khan <chengkan@allwinnertech.com>
*   Date    : 2014/07/07
*   Comment : 实现IO操作的CDX WRAPPED
*/

#include "CdxIoUtils.h"

int cdxio_r8(CdxStreamT *s)
{
    char c[4] = {0};
    CdxStreamRead(s, c, 1);
    return (c[0]&0xff);
}

uint32_t cdxio_rl16(CdxStreamT *s)
{
    unsigned int val;
    val = cdxio_r8(s);
    val |= cdxio_r8(s) << 8;
    return val;
}

uint32_t cdxio_rl32(CdxStreamT *s)
{
    unsigned int val;
    val = cdxio_rl16(s);
    val |= cdxio_rl16(s) << 16;
    return val;
}

uint64_t cdxio_rl64(CdxStreamT *s)
{
    uint64_t val = 0;
    val = (uint64_t)cdxio_rl32(s);
    val |= (uint64_t)cdxio_rl32(s) << 32;
    return val;
}

uint32_t cdxio_rb16(CdxStreamT *s)
{
    unsigned int val;
    val = cdxio_r8(s) << 8;
    val |= cdxio_r8(s);
    return val;
}

uint32_t cdxio_rb32(CdxStreamT *s)
{
    unsigned int val;
    val = cdxio_rb16(s) << 16;
    val |= cdxio_rb16(s);
    return val;
}

uint64_t cdxio_rb64(CdxStreamT *s)
{
    uint64_t val = 0;
    val = ((uint64_t)cdxio_rb32(s)) << 32 ;
    val |= (uint64_t)cdxio_rb32(s);
    return val;
}
