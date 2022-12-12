/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : AwRawStreamParser.c
* Description :parse raw video stream
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment : parse raw video stream
*
*
*/

#include "AwRawStreamParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#include <stdlib.h>
#include <errno.h>

#define MKBETAG(a,b,c,d) ((d) | ((c) << 8) | ((b) << 16) | ((unsigned)(a) << 24))
#define Get8Bits(p)   (*p++)
#define GetBE16Bits(p)  ({\
    cdx_uint32 myTmp = Get8Bits(p);\
    myTmp << 8 | Get8Bits(p);})
#define GetBE32Bits(p)  ({\
    cdx_uint32 myTmp = GetBE16Bits(p);\
    myTmp << 16 | GetBE16Bits(p);})
#define GetBE64Bits(p) ({\
    cdx_uint64 myTmp = GetBE32Bits(p);\
    myTmp << 32 | GetBE32Bits(p);})
#define GetLE16Bits(p)  ({\
    cdx_uint32 myTmp = Get8Bits(p);\
    myTmp | Get8Bits(p) << 8;})
#define GetLE32Bits(p)  ({\
    cdx_uint32 myTmp = GetLE16Bits(p);\
    myTmp | GetLE16Bits(p) << 16;})
#define GetLE64Bits(p) ({\
    cdx_uint64 myTmp = GetLE32Bits(p);\
    myTmp | (cdx_uint64)GetLE32Bits(p) << 32;})

#define AW_RAW_STREAM_PROB_SIZE (512*1024)

static int eStreamType;

static int  AwRawStreamFindStartCode (unsigned char *Buf, int ZerosInStartCode)
{
    int info;
    int i;
    info = 1;
    for (i = 0; i < ZerosInStartCode; i++)
        if(Buf[i] != 0)
            info = 0;

        if(Buf[i] != 1)
            info = 0;
        return info;
}

static int AwRawStreamGetDataTrunkSize(RawStreamParser *rawStreamParser)
{
    unsigned char *Buf = rawStreamParser->pRawStreamBuf;
    int pos             = 0;
    int StartCodeFound  = 0;
    int info2           = 0;
    int info3           = 0;
    int ret             = 0;

    do
    {
        ret = CdxStreamRead(rawStreamParser->file, &(Buf[pos++]), sizeof(char));
        if(ret != 1)
        {
            rawStreamParser->nRawStreamBufSize = 0;
            return ret;
        }
    }while(Buf[pos-1]==0);

    while (!StartCodeFound)
    {
        ret = CdxStreamRead(rawStreamParser->file, &(Buf[pos++]), sizeof(char));

        if(ret != 1)
        {
            if(CdxStreamEos(rawStreamParser->file))
                rawStreamParser->nRawStreamBufSize = pos - 1;
            else
                rawStreamParser->nRawStreamBufSize = pos - 4 + info2;
            return ret;
        }

        info3 = AwRawStreamFindStartCode(&Buf[pos-4], 3);
        if(info3 != 1)
            info2 = AwRawStreamFindStartCode(&Buf[pos-3], 2);

        StartCodeFound = (info2 == 1 || info3 == 1);
    }
    CdxStreamSeek(rawStreamParser->file,- 4 + info2, SEEK_CUR);
    rawStreamParser->nRawStreamBufSize = pos - 4 + info2;

    return pos - 4 + info2;
}

static cdx_int32 AwRawStreamParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    RawStreamParser *rawStreamParser = (RawStreamParser*)parser;
    (void)rawStreamParser;
    memset(pMediaInfo, 0, sizeof(CdxMediaInfoT));
    pMediaInfo->fileSize = -1;
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pMediaInfo->program[0].videoNum = 1;
    pMediaInfo->program[0].videoIndex = 0;
    if(eStreamType == h265_raw_stream)
        pMediaInfo->program[0].video[0].eCodecFormat = VIDEO_CODEC_FORMAT_H265;
    else
        pMediaInfo->program[0].video[0].eCodecFormat = VIDEO_CODEC_FORMAT_H264;
    pMediaInfo->program[0].video[0].pCodecSpecificData =
        rawStreamParser->tempVideoInfo.pCodecSpecificData;
    pMediaInfo->program[0].video[0].nCodecSpecificDataLen =
        rawStreamParser->tempVideoInfo.nCodecSpecificDataLen;
    return 0;
}

static cdx_int32 AwRawStreamParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    RawStreamParser *rawStreamParser = (RawStreamParser*)parser;
    int nLen, ret;
    if(rawStreamParser->status != CDX_PSR_IDLE &&
        rawStreamParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGW("status != CDX_PSR_IDLE && status != \
            CDX_PSR_PREFETCHED, BdParserPrefetch invaild");
        rawStreamParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(rawStreamParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if(rawStreamParser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(pkt, &rawStreamParser->pkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&rawStreamParser->statusLock);
    if(rawStreamParser->forceStop)
    {
        pthread_mutex_unlock(&rawStreamParser->statusLock);
        return -1;
    }
    rawStreamParser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&rawStreamParser->statusLock);

    memset(pkt, 0, sizeof(CdxPacketT));
    nLen = AwRawStreamGetDataTrunkSize(rawStreamParser);
    if(nLen < 0)
    {
        /* end of file */
        ret = -1;
        goto _exit;
    }
    else if(nLen == 0)
    {
        rawStreamParser->bEndofStream = 1;
    }
    pkt->type = CDX_MEDIA_VIDEO;
    pkt->length = rawStreamParser->nRawStreamBufSize;
    pkt->pts = -1;
    pkt->pcr = -1;
    pkt->flags |= (FIRST_PART|LAST_PART);
    pkt->streamIndex = 0;

    memcpy(&rawStreamParser->pkt, pkt, sizeof(CdxPacketT));
    rawStreamParser->status = CDX_PSR_PREFETCHED;
    return 0;
_exit:
    rawStreamParser->status = CDX_PSR_IDLE;
    return ret;
}

static cdx_int32 AwRawStreamParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    RawStreamParser *rawStreamParser = (RawStreamParser*)parser;
    if(rawStreamParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_PREFETCHED, we can not read!");
        rawStreamParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    pthread_mutex_lock(&rawStreamParser->statusLock);
    if(rawStreamParser->forceStop)
    {
        pthread_mutex_unlock(&rawStreamParser->statusLock);
        return -1;
    }
    rawStreamParser->status = CDX_PSR_READING;
    pthread_mutex_unlock(&rawStreamParser->statusLock);

    if(pkt->length <= pkt->buflen)
    {
        memcpy(pkt->buf, rawStreamParser->pRawStreamBuf, pkt->length);
    }
    else
    {
        memcpy(pkt->buf, rawStreamParser->pRawStreamBuf, pkt->buflen);
        memcpy(pkt->ringBuf, rawStreamParser->pRawStreamBuf + pkt->buflen,
            pkt->length - pkt->buflen);
    }
    if(rawStreamParser->bEndofStream)
    {
        rawStreamParser->bEndofStream = 0;
        rawStreamParser->mErrno = PSR_EOS;
    }
    rawStreamParser->status = CDX_PSR_IDLE;
    return 0;
}

cdx_int32 AwRawStreamParserForceStop(CdxParserT *parser)
{
    CDX_LOGI("AwRawStreamParserForceStop start");
    RawStreamParser *rawStreamParser = (RawStreamParser*)parser;
    if(rawStreamParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGW("rawStreamParser->status < CDX_PSR_IDLE, can not forceStop");
        rawStreamParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    pthread_mutex_lock(&rawStreamParser->statusLock);
    rawStreamParser->forceStop = 1;
    rawStreamParser->mErrno = PSR_USER_CANCEL;
    pthread_mutex_unlock(&rawStreamParser->statusLock);

    int ret = CdxStreamForceStop(rawStreamParser->file);
    if(ret < 0)
    {
        CDX_LOGW("CdxStreamForceStop fail");
    }
    while(rawStreamParser->status != CDX_PSR_IDLE && rawStreamParser->status != CDX_PSR_PREFETCHED)
    {
        usleep(10000);
    }
    rawStreamParser->status = CDX_PSR_IDLE;
    CDX_LOGI("AwRawStreamParserForceStop end");
    return 0;
}

cdx_int32 AwRawStreamParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGI("AwRawStreamParserClrForceStop start");
    RawStreamParser *rawStreamParser = (RawStreamParser*)parser;
    if(rawStreamParser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("rawStreamParser->status != CDX_PSR_IDLE");
        rawStreamParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    rawStreamParser->forceStop = 0;
    int ret = CdxStreamClrForceStop(rawStreamParser->file);
    if(ret < 0)
    {
        CDX_LOGW("CdxStreamClrForceStop fail");
    }
    CDX_LOGI("AwRawStreamParserClrForceStop end");
    return 0;
}


static int AwRawStreamParserControl(CdxParserT *parser, int cmd, void *param)
{
    RawStreamParser *rawStreamParser = (RawStreamParser*)parser;
    (void)rawStreamParser;
    (void)param;
    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI(" awts parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            return 0;
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return 0;
        default:
            break;
    }
    return 0;
}

cdx_int32 AwRawStreamParserGetStatus(CdxParserT *parser)
{
    RawStreamParser *rawStreamParser = (RawStreamParser *)parser;
    return rawStreamParser->mErrno;
}
/*
cdx_int32 AwRawStreamParserSeekTo(CdxParserT *parser, cdx_int64  timeUs)
{
    //CDX_LOGD("AwRawStreamParserSeekTo start, timeUs = %lld", timeUs);
    CDX_LOGE("it is not supported");
    return -1;
}*/
static cdx_int32 AwRawStreamParserClose(CdxParserT *parser)
{
    RawStreamParser *rawStreamParser = (RawStreamParser *)parser;
    int ret;
    pthread_mutex_lock(&rawStreamParser->refLock);
    rawStreamParser->forceStop = 1;
    pthread_mutex_unlock(&rawStreamParser->refLock);

    if(rawStreamParser->tempVideoInfo.pCodecSpecificData != NULL)
        free(rawStreamParser->tempVideoInfo.pCodecSpecificData);
    rawStreamParser->tempVideoInfo.pCodecSpecificData = NULL;

    CdxAtomicDec(&rawStreamParser->ref);
    while (CdxAtomicRead(&rawStreamParser->ref) != 0)
    {
        usleep(10000);
    }
    pthread_join(rawStreamParser->openTid, NULL);
    ret = CdxStreamClose(rawStreamParser->file);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamClose fail, ret(%d)", ret);
    }

    pthread_mutex_destroy(&rawStreamParser->statusLock);
    pthread_mutex_destroy(&rawStreamParser->refLock);
    if(rawStreamParser->pRawStreamBuf)
        CdxFree(rawStreamParser->pRawStreamBuf);
    free(rawStreamParser);
    return 0;
}

cdx_uint32 AwRawStreamParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    RawStreamParser *rawStreamParser = (RawStreamParser *)parser;
    return rawStreamParser->flags;
}
int AwRawStreamParserInit(CdxParserT *parser)
{
    CDX_UNUSE(parser);
    return 0;
}
static struct CdxParserOpsS awRawStreamParserOps =
{
    .control         = AwRawStreamParserControl,
    .prefetch         = AwRawStreamParserPrefetch,
    .read             = AwRawStreamParserRead,
    .getMediaInfo     = AwRawStreamParserGetMediaInfo,
    .close             = AwRawStreamParserClose,
    .attribute        = AwRawStreamParserAttribute,
    .getStatus        = AwRawStreamParserGetStatus,
    .init            = AwRawStreamParserInit,
};

CdxParserT *AwRawStreamParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    RawStreamParser *rawStreamParser = CdxMalloc(sizeof(RawStreamParser));
    if(!rawStreamParser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(rawStreamParser, 0x00, sizeof(RawStreamParser));
    CdxAtomicSet(&rawStreamParser->ref, 1);

    int ret = pthread_mutex_init(&rawStreamParser->refLock, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_mutex_init(&rawStreamParser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);

    rawStreamParser->pRawStreamBuf = CdxMalloc(AW_RAW_STREAM_BUF_SIZE);
    CDX_FORCE_CHECK(rawStreamParser->pRawStreamBuf);
    rawStreamParser->file = stream;
    rawStreamParser->flags = flags & MUTIL_AUDIO;
    rawStreamParser->base.ops = &awRawStreamParserOps;
    rawStreamParser->base.type = CDX_PARSER_AWRAWSTREAM;
    rawStreamParser->mErrno = PSR_OK;
    rawStreamParser->status = CDX_PSR_IDLE;

    CdxStreamRead(rawStreamParser->file,
             rawStreamParser->pRawStreamBuf,
             AW_RAW_STREAM_PROB_SIZE*sizeof(char));
    loge(" rawStreamParser->file: %p ", rawStreamParser->file);

    ProbeVideoSpecificData(&rawStreamParser->tempVideoInfo,
                            rawStreamParser->pRawStreamBuf,
                            AW_RAW_STREAM_PROB_SIZE, VIDEO_CODEC_FORMAT_H265,
                            CDX_PARSER_TS);
    CdxStreamSeek(rawStreamParser->file, 0, SEEK_SET);

    return &rawStreamParser->base;
}

static int AwRawStreamProbeH264(cdx_char *p)
{
    int code, type;
    code = p[0] & 0x80;
    if(code != 0)
    {
        logd(" Maybe this is not h264 stream. forbidden_zero_bit != 0  ");
        return 0;
    }
    type = p[0] & 0x1f;
    if(type <= 13 || type == 19)
    {
        logd(" Maybe this is h264 stream. nal unit type = %d", type);
        return 50;
    }
    else
    {
        logd(" This is not h264 stream. nal unit type = %d", type);
        return 0;
    }
    return 0;
}

static int AwRawStreamProbeH265(cdx_char *p)
{
    int code, type, nNuhLayerId;
    int ret = 0;
    code = p[0] & 0x80;
    if(code != 0)
    {
        logd(" Maybe this is not h265 stream. forbidden_zero_bit != 0  ");
        return ret;
    }
    type = (p[0] & 0x7e) >> 1;
    if(type > 63)
    {
        logd(" Maybe this is not h265 stream. nal unit type > 63");
        return ret;
    }
    switch(type)
    {
        case 0x20: /*VPS*/
        case 0x21: /*SPS*/
        case 0x22: /*PPS*/
        {
            nNuhLayerId = ((p[0] & 0x1) << 5) | ((p[1] & 0xf8) >> 3);
            logd(" Parser probe result: This is h265 raw stream:%d! ",
                nNuhLayerId);
            ret = 50;
            break;
        }
        default:
            break;
    }
    return ret;
}

cdx_uint32 AwRawStreamParserProbe(CdxStreamProbeDataT *probeData)
{
    cdx_char *data = probeData->buf;
    cdx_char *p = NULL;
    int mask = 0xffffff;
    int code = -1;
    int i, ret;

    eStreamType = h265_raw_stream;
    for(i = 0; i < 1024; i++)
    {
        code = (code << 8) | data[i];
        if((code & mask) == 0x000001) /* h265, h264 start code */
        {
            p = &data[i + 1];
            break;
        }
    }
    if(p != NULL)
    {
        eStreamType = h265_raw_stream;
        ret = AwRawStreamProbeH265(p);
        if(ret == 0)
        {
            ret = AwRawStreamProbeH264(p);
            if(ret != 0)
                eStreamType = h264_raw_stream;
        }
        return ret;
    }
    return 0;
}

CdxParserCreatorT rawStreamParserCtor =
{
    .create    = AwRawStreamParserOpen,
    .probe     = AwRawStreamParserProbe
};

