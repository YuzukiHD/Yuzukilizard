/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxIoUtils.h
* Description :
* History :
*   Author  : chengkan <chengkan@allwinnertech.com>
*   Date    : 2016/07/07  The Ghost Gate Welcome
*/
#ifndef CDXIOUTILS_H
#define CDXIOUTILS_H

#include <CdxStream.h>

int cdxio_r8(CdxStreamT *s);

uint32_t cdxio_rl16(CdxStreamT *s);

uint32_t cdxio_rl32(CdxStreamT *s);

uint64_t cdxio_rl64(CdxStreamT *s);

uint32_t cdxio_rb16(CdxStreamT *s);

uint32_t cdxio_rb32(CdxStreamT *s);

uint64_t cdxio_rb64(CdxStreamT *s);
#endif

