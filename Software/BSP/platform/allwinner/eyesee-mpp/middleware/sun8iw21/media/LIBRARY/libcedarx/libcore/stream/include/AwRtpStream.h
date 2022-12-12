/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : AwRtpStream.h
 * Description : Rtp Stream header
 * History :
 *
 */

#ifndef AW_RTP_STREAM_H
#define AW_RTP_STREAM_H
#include <CdxList.h>
#include <AwPool.h>

struct RtpStreamHdrS
{
    cdx_uint32 ssrcId;
    cdx_int32 type;
    cdx_uint32 rtpTime; /*time ms*/
    cdx_int32 length;
};

struct RtpBufListNodeS
{
    CdxListNodeT node;
    CdxBufferT *buf;
};

#ifdef __cplusplus
extern "C"
{
#endif

static inline void BufferListAdd(CdxListT *list, CdxBufferT *buf, AwPoolT *pool)
{
    struct RtpBufListNodeS *bufNode;
    bufNode = Palloc(pool, sizeof(*bufNode));
    bufNode->buf = buf;

    CdxListAddTail(&bufNode->node, list);

    return ;
}

RtpStreamItfT *RtpH264ItfCreate(AwPoolT *pool, cdx_char *desc, cdx_char *params);

RtpStreamItfT *RtpMpeg4ItfCreate(AwPoolT *pool, cdx_char *desc, cdx_char *params);

#ifdef __cplusplus
}
#endif

#endif
