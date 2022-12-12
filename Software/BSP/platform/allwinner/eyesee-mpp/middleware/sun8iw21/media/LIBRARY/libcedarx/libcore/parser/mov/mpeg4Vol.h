/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : mpeg4Vol.h
 * Description :
 * History :
 *
 */

#ifndef __MPEG4_VOL_H
#define __MPEG4_VOL_H

#include "CdxMovParser.h"

int mov_getvolhdr(cdx_uint8 *start_addr,cdx_int32 length,cdx_int32 *width,cdx_int32 *height);

#endif
