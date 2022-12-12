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

#include "AwSpecialStreamParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#include <stdlib.h>
#include <errno.h>

#define AW_RAW_STREAM_PROB_SIZE (512*1024)


static cdx_int32 AwSpecialStreamParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    SpecialStreamParser *specialStreamParser = (SpecialStreamParser*)parser;
    int width, height, ret;
    int nSpecialDataLen;
    int bIs3DStream = 0;
    unsigned int codec = 0;
    char *pSpecilData = NULL;
    (void)specialStreamParser;
    memset(pMediaInfo, 0, sizeof(CdxMediaInfoT));
    ret = 0;
    /* width, height, codec, special data len, special data */
    if(1)
    {
         char pp[16];
        ret |= CdxStreamRead(specialStreamParser->file, pp, 14);
        logd("special parser test: %x,%x,%x,%x,%x,%x,%x,%x", \
            pp[0],pp[1],pp[2],pp[3],pp[4],pp[5],pp[6],pp[7]);

    }
    ret |= CdxStreamRead(specialStreamParser->file, &codec, sizeof(int));
    ret |= CdxStreamRead(specialStreamParser->file, &width, sizeof(int));
    ret |= CdxStreamRead(specialStreamParser->file, &height, sizeof(int));
    ret |= CdxStreamRead(specialStreamParser->file, &bIs3DStream, sizeof(int));
    ret |= CdxStreamRead(specialStreamParser->file, &nSpecialDataLen, sizeof(int));
    logd(" special parser get special data len: \
        %d, width: %d, height: %d, codex: %x , bIs3DStream: %d",  \
        nSpecialDataLen, width, height, (unsigned int)codec, bIs3DStream);
    if(ret < 0)
        return -1;
    if(nSpecialDataLen > 0)
    {
        int size = (nSpecialDataLen + 7) & ~7;
        pSpecilData = CdxMalloc(size);
        if(pSpecilData == NULL)
        {
            loge(" aw special stream parser malloc error, size: %d ", size);
            return -1;
        }
        ret = CdxStreamRead(specialStreamParser->file, pSpecilData, nSpecialDataLen);
        specialStreamParser->tempVideoInfo.pCodecSpecificData = pSpecilData;
        specialStreamParser->tempVideoInfo.nCodecSpecificDataLen = nSpecialDataLen;
    }
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pMediaInfo->program[0].videoNum = 1;
    pMediaInfo->program[0].videoIndex = 0;
    pMediaInfo->program[0].video[0].eCodecFormat = codec;
    pMediaInfo->program[0].video[0].pCodecSpecificData =
        specialStreamParser->tempVideoInfo.pCodecSpecificData;
    pMediaInfo->program[0].video[0].nCodecSpecificDataLen =
        specialStreamParser->tempVideoInfo.nCodecSpecificDataLen;
    pMediaInfo->program[0].video[0].bIs3DStream = bIs3DStream;
    return 0;
}

static cdx_int32 AwSpecialStreamParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    SpecialStreamParser *specialStreamParser = (SpecialStreamParser*)parser;
    int nLen = 0;
    int ret  = -1;
    int64_t nPts = 0;
    int nStreamIndex = 0;
    if(specialStreamParser->status != CDX_PSR_IDLE &&
        specialStreamParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGW("status != CDX_PSR_IDLE && status != \
            CDX_PSR_PREFETCHED, BdParserPrefetch invaild");
        specialStreamParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(specialStreamParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if(specialStreamParser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(pkt, &specialStreamParser->pkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&specialStreamParser->statusLock);
    if(specialStreamParser->forceStop)
    {
        pthread_mutex_unlock(&specialStreamParser->statusLock);
        return -1;
    }
    specialStreamParser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&specialStreamParser->statusLock);

    memset(pkt, 0, sizeof(CdxPacketT));
    #if 1
    ret |= CdxStreamRead(specialStreamParser->file, &nStreamIndex, 4);
    if(nStreamIndex == 0x70737761)
    {
        CDX_LOGI("data contain the awsp tab\n");
        ret |= CdxStreamRead(specialStreamParser->file, &nStreamIndex, sizeof(int));
    }
    #else
        ret |= CdxStreamRead(specialStreamParser->file, &nStreamIndex, sizeof(int));
    #endif
    ret = CdxStreamRead(specialStreamParser->file, &nLen, sizeof(int));
    ret = CdxStreamRead(specialStreamParser->file, &nPts, sizeof(int64_t));
    logv("  special parser get data stream index: %d, size: %d, ret: %d", \
        nStreamIndex, nLen, ret);
    if(nLen <= 0)
    {
        /* end of file */
        ret = -1;
        specialStreamParser->bEndofStream = 1;
        goto _exit;
    }

    pkt->type = CDX_MEDIA_VIDEO;
    pkt->length = nLen;
    pkt->pts = nPts;
    pkt->pcr = -1;
    pkt->flags |= (FIRST_PART|LAST_PART);
    pkt->streamIndex = nStreamIndex;
    pkt->flags |= (pkt->streamIndex == 1);

    memcpy(&specialStreamParser->pkt, pkt, sizeof(CdxPacketT));
    specialStreamParser->status = CDX_PSR_PREFETCHED;
    return 0;
_exit:
    specialStreamParser->status = CDX_PSR_IDLE;
    return ret;
}

static cdx_int32 AwSpecialStreamParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    int ret = 0;
    SpecialStreamParser *specialStreamParser = (SpecialStreamParser*)parser;
    if(specialStreamParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_PREFETCHED, we can not read!");
        specialStreamParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    pthread_mutex_lock(&specialStreamParser->statusLock);
    if(specialStreamParser->forceStop)
    {
        pthread_mutex_unlock(&specialStreamParser->statusLock);
        return -1;
    }
    specialStreamParser->status = CDX_PSR_READING;
    pthread_mutex_unlock(&specialStreamParser->statusLock);

    if(pkt->length <= pkt->buflen)
    {
        ret |= CdxStreamRead(specialStreamParser->file, pkt->buf, pkt->length);
    }
    else
    {
        ret |= CdxStreamRead(specialStreamParser->file, pkt->buf, pkt->buflen);
        ret |= CdxStreamRead(specialStreamParser->file, pkt->ringBuf,
                             pkt->length - pkt->buflen);
    }
    if(ret <= 0)
    {
         return -1;
    }

    if(specialStreamParser->bEndofStream)
    {
        specialStreamParser->bEndofStream = 0;
        specialStreamParser->mErrno = PSR_EOS;
    }
    specialStreamParser->status = CDX_PSR_IDLE;
    return 0;
}

cdx_int32 AwSpecialStreamParserForceStop(CdxParserT *parser)
{
    CDX_LOGI("AwSpecialStreamParserForceStop start");
    SpecialStreamParser *specialStreamParser = (SpecialStreamParser*)parser;
    if(specialStreamParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGW("specialStreamParser->status < CDX_PSR_IDLE, can not forceStop");
        specialStreamParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    pthread_mutex_lock(&specialStreamParser->statusLock);
    specialStreamParser->forceStop = 1;
    specialStreamParser->mErrno = PSR_USER_CANCEL;
    pthread_mutex_unlock(&specialStreamParser->statusLock);

    int ret = CdxStreamForceStop(specialStreamParser->file);
    if(ret < 0)
    {
        CDX_LOGW("CdxStreamForceStop fail");
    }
    while(specialStreamParser->status != CDX_PSR_IDLE &&
        specialStreamParser->status != CDX_PSR_PREFETCHED)
    {
        usleep(10000);
    }
    specialStreamParser->status = CDX_PSR_IDLE;
    CDX_LOGI("AwSpecialStreamParserForceStop end");
    return 0;
}

cdx_int32 AwSpecialStreamParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGI("AwSpecialStreamParserClrForceStop start");
    SpecialStreamParser *specialStreamParser = (SpecialStreamParser*)parser;
    if(specialStreamParser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("specialStreamParser->status != CDX_PSR_IDLE");
        specialStreamParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    specialStreamParser->forceStop = 0;
    int ret = CdxStreamClrForceStop(specialStreamParser->file);
    if(ret < 0)
    {
        CDX_LOGW("CdxStreamClrForceStop fail");
    }
    CDX_LOGI("AwSpecialStreamParserClrForceStop end");
    return 0;
}


static int AwSpecialStreamParserControl(CdxParserT *parser, int cmd, void *param)
{
    SpecialStreamParser *specialStreamParser = (SpecialStreamParser*)parser;
    (void)specialStreamParser;
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

cdx_int32 AwSpecialStreamParserGetStatus(CdxParserT *parser)
{
    SpecialStreamParser *specialStreamParser = (SpecialStreamParser *)parser;
    return specialStreamParser->mErrno;
}
/*
cdx_int32 AwSpecialStreamParserSeekTo(CdxParserT *parser, cdx_int64  timeUs)
{
    //CDX_LOGD("AwSpecialStreamParserSeekTo start, timeUs = %lld", timeUs);
    CDX_LOGE("it is not supported");
    return -1;
}*/
static cdx_int32 AwSpecialStreamParserClose(CdxParserT *parser)
{
    SpecialStreamParser *specialStreamParser = (SpecialStreamParser *)parser;
    int ret;
    pthread_mutex_lock(&specialStreamParser->refLock);
    specialStreamParser->forceStop = 1;
    pthread_mutex_unlock(&specialStreamParser->refLock);

    if(specialStreamParser->tempVideoInfo.pCodecSpecificData != NULL)
        free(specialStreamParser->tempVideoInfo.pCodecSpecificData);
    specialStreamParser->tempVideoInfo.pCodecSpecificData = NULL;

    CdxAtomicDec(&specialStreamParser->ref);
    while (CdxAtomicRead(&specialStreamParser->ref) != 0)
    {
        usleep(10000);
    }
    pthread_join(specialStreamParser->openTid, NULL);
    ret = CdxStreamClose(specialStreamParser->file);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamClose fail, ret(%d)", ret);
    }

    pthread_mutex_destroy(&specialStreamParser->statusLock);
    pthread_mutex_destroy(&specialStreamParser->refLock);
    if(specialStreamParser->pSpecialStreamBuf)
        CdxFree(specialStreamParser->pSpecialStreamBuf);
    free(specialStreamParser);
    return 0;
}

cdx_uint32 AwSpecialStreamParserAttribute(CdxParserT *parser)
    /*return falgs define as open's falgs*/
{
    SpecialStreamParser *specialStreamParser = (SpecialStreamParser *)parser;
    return specialStreamParser->flags;
}
int AwSpecialStreamParserInit(CdxParserT *parser)
{
    CDX_UNUSE(parser);
    return 0;
}
static struct CdxParserOpsS awSpecialStreamParserOps =
{
    .control         = AwSpecialStreamParserControl,
    .prefetch         = AwSpecialStreamParserPrefetch,
    .read             = AwSpecialStreamParserRead,
    .getMediaInfo     = AwSpecialStreamParserGetMediaInfo,
    .close             = AwSpecialStreamParserClose,
    .attribute        = AwSpecialStreamParserAttribute,
    .getStatus        = AwSpecialStreamParserGetStatus,
    .init            = AwSpecialStreamParserInit,
};

CdxParserT *AwSpecialStreamParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    SpecialStreamParser *specialStreamParser = CdxMalloc(sizeof(SpecialStreamParser));
    if(!specialStreamParser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(specialStreamParser, 0x00, sizeof(SpecialStreamParser));
    CdxAtomicSet(&specialStreamParser->ref, 1);

    int ret = pthread_mutex_init(&specialStreamParser->refLock, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_mutex_init(&specialStreamParser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);

    specialStreamParser->pSpecialStreamBuf = CdxMalloc(AW_RAW_STREAM_BUF_SIZE);
    CDX_FORCE_CHECK(specialStreamParser->pSpecialStreamBuf);
    specialStreamParser->file = stream;
    specialStreamParser->flags = flags & MUTIL_AUDIO;
    specialStreamParser->base.ops = &awSpecialStreamParserOps;
    specialStreamParser->base.type = CDX_PARSER_AWRAWSTREAM;
    specialStreamParser->mErrno = PSR_OK;
    specialStreamParser->status = CDX_PSR_IDLE;

    return &specialStreamParser->base;
}


cdx_uint32 AwSpecialStreamParserProbe(CdxStreamProbeDataT *probeData)
{
    cdx_char *data = probeData->buf;
    cdx_char mask[] = "awspcialstream"; //14 bytes
    int i, ret = 70;
    logd("  parser probe aw special parser");
    for(i = 0; i < 14; i++)
    {
        if(data[i] != mask[i])
        {
            ret = 0;
            loge(" probe special parser fail ");
            break;
        }
    }
    return ret;
}

CdxParserCreatorT specialStreamParserCtor =
{
    .create    = AwSpecialStreamParserOpen,
    .probe     = AwSpecialStreamParserProbe
};

