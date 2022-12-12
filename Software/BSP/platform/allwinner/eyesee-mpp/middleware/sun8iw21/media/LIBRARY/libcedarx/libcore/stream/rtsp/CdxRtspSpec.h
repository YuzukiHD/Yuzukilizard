/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxRtspSpec.h
 * Description :
 * History :
 *
 */

#ifndef CEDARX_RTSP_SPEC_H
#define CEDARX_RTSP_SPEC_H

typedef struct CdxRtspPktHeaderS
{
    cdx_int32  type;
    cdx_uint32 length;
    cdx_int64  pts;
}CdxRtspPktHeaderT;

#endif

