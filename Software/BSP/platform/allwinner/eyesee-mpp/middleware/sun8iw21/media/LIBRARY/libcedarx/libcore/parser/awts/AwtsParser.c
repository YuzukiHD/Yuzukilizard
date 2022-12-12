/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : AwtsParser.c
 * Description : Allwinner TS Parser Definition
 * History :
 *
 */

#define LOG_TAG "AwtsParser"
#include "AwtsParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#include <stdlib.h>
#include <errno.h>

#define MKBETAG(a,b,c,d) ((d) | ((c) << 8) | ((b) << 16) | ((unsigned)(a) << 24))
#define Skip8Bits(p)   (p++)
#define Skip16Bits(p)   (p += 2)
#define Skip32Bits(p)   (p += 4)
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

static int AwtsSearchSyncHeader(AwtsParser *awtsParser)
{
    uint8_t sync[3];
    int sp = 0;

    int ret = CdxStreamRead(awtsParser->file, sync, 3);
    if (ret < 3)
    {
        CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
        if(ret < 0)
        {
            return ret;
        }
        else
        {
            awtsParser->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return -1;
        }
    }

    while(1)
    {
        if (sync[sp & 2] == 0x00 &&
            sync[(sp + 1) & 2] == 0x00 &&
            sync[(sp + 2) & 2] == 0x5a)
            break;
        ret = CdxStreamRead(awtsParser->file, &sync[sp++ & 2], 1);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            return ret;
        }
        else if(ret == 0)
        {
            awtsParser->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return -1;
        }
    }
    return 0;
}

static int AwtsNewTrack(AwtsParser *awtsParser)
{
    uint8_t tmp[29];
    uint8_t *data = tmp;

    int ret = CdxStreamRead(awtsParser->file, tmp, 29);
    if (ret < 29)
    {
        CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
        if(ret < 0)
        {
            return ret;
        }
        else
        {
            awtsParser->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return -1;
        }
    }
    Skip32Bits(data);//len
    unsigned tag = GetBE32Bits(data);
    enum CdxMediaTypeE type = CDX_MEDIA_UNKNOWN;
    
    switch (tag)
    {
        case MKBETAG('v','i','d','e'):
            type = CDX_MEDIA_VIDEO;
            break;
        case MKBETAG('s','o','u','n'):
            type = CDX_MEDIA_AUDIO;
            break;
        default:
            CDX_LOGW("please check this code, when error happen");
            return -1;
    }
    
    AwtsTrace *track;
    track = awtsParser->tracks[awtsParser->trackCount] = (AwtsTrace *)malloc(sizeof(AwtsTrace));
    if(!track)
    {
        CDX_LOGE("malloc fail.");
        return -1;
    }
    memset(awtsParser->tracks[awtsParser->trackCount], 0, sizeof(AwtsTrace));

    track->version = Get8Bits(data);
    Skip8Bits(data); //track->traceIndex = Get8Bits(data);
    track->duration = GetLE32Bits(data);
    unsigned mime = GetBE32Bits(data);

    switch (mime)
    {
        case MKBETAG('a','v','c','1'):
            track->eCodecFormat = VIDEO_CODEC_FORMAT_H264;
            break;
        case MKBETAG('m','p','4','a'):
            track->eCodecFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
            break;
        case MKBETAG('m','p','4','v'):
            track->eCodecFormat = VIDEO_CODEC_FORMAT_MPEG4;
            break;
        default:
            CDX_LOGW("it is not suppoted, mime = %u", mime);
            track->eCodecFormat = 0;//VIDEO_CODEC_FORMAT_UNKNOWN//AUDIO_CODEC_FORMAT_UNKNOWN
            break;
    }

    track->nBitrate = GetBE16Bits(data) * 1024; //kbps
    track->nBitsPerSample = Get8Bits(data); /* depth */
    track->eType = type;
    if(type)
    {
        track->nWidth = GetBE16Bits(data); /* width */
        track->nHeight = GetBE16Bits(data); /* height */
    }
    else
    {
        track->nSampleRate = GetBE16Bits(data);
        track->nChannelNum = GetBE16Bits(data);
    }
    
    GetBE16Bits(data); /* reserved */

    track->nCodecSpecificDataLen = GetBE16Bits(data);
    if (track->nCodecSpecificDataLen > 0)
    {
        track->pCodecSpecificData = (char *)malloc(track->nCodecSpecificDataLen);
        if(!track->pCodecSpecificData)
        {
            CDX_LOGE("malloc fail.");
            return -1;
        }
        ret = CdxStreamRead(awtsParser->file, track->pCodecSpecificData,
                            track->nCodecSpecificDataLen);
        if (ret < track->nCodecSpecificDataLen)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                return ret;
            }
            else
            {
                awtsParser->mErrno = PSR_EOS;
                CDX_LOGI("CdxStreamRead eos");
                return -1;
            }
        }
    }

    awtsParser->trackCount++;
    return 0;
}

static int AwtsReadStreamInfo(AwtsParser *awtsParser)
{
    int Length;
    char Version;
    char Flags;
    char TrackCount;

    uint8_t tmp[7];
    uint8_t *data = tmp;

    int ret = CdxStreamRead(awtsParser->file, tmp, 7);
    if (ret < 7)
    {
        CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
        if(ret < 0)
        {
            return ret;
        }
        else
        {
            awtsParser->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return -1;
        }
    }
    CDX_LOGD("Length = 0x%x %x %x %x %x %x %x",
             tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6]);
    Length  = GetBE32Bits(data);
    CDX_LOGD("Length = 0x%x", Length);
    Version = Get8Bits(data);
    Flags   = Get8Bits(data);
    TrackCount = Get8Bits(data);
    CDX_LOGD("TrackCount = 0x%x", TrackCount);
    while(TrackCount--)
    {
        ret = AwtsNewTrack(awtsParser);
        if(ret < 0)
        {
            CDX_LOGE("AwtsNewTrack fail.");
            return ret;
        }
    }
    return 0;
}

static int AwtsReadHeader(AwtsParser *awtsParser)
{
    if (AwtsSearchSyncHeader(awtsParser) < 0)
    {
        CDX_LOGE("AwtsSearchSyncHeader fail");
        return -1;
    }
    uint8_t tmp[8];
    uint8_t *data;

    int ret = CdxStreamRead(awtsParser->file, tmp, 7);
    if (ret < 7)
    {
        CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
        if(ret < 0)
        {
            return ret;
        }
        else
        {
            awtsParser->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return -1;
        }
    }
    data = tmp;
    CDX_BUF_DUMP(data, 7);
    awtsParser->packetNumInStream = Get8Bits(data);
    awtsParser->curPacketLength = GetBE32Bits(data);
    awtsParser->curTraceIndex = Get8Bits(data);
    awtsParser->curPacketFlags = Get8Bits(data);
    awtsParser->isPacketHeader = !!(awtsParser->curPacketFlags & (1<<7));
    awtsParser->isKeyFrame = !!(awtsParser->curPacketFlags & (1<<6));
    awtsParser->sequenceNumIncr = !!(awtsParser->curPacketFlags & (1<<5));

    if (awtsParser->isPacketHeader)
    {
        ret = CdxStreamRead(awtsParser->file, tmp, 8);
        if (ret < 8)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                return ret;
            }
            else
            {
                awtsParser->mErrno = PSR_EOS;
                CDX_LOGI("CdxStreamRead eos");
                return -1;
            }
        }
        if (memcmp(tmp, "awts", 4))
        {
            CDX_LOGE("it is not awts");
            return -1;
        }
        /*
        data = tmp + 4;
        GetBE32Bits(data);// CRC
        */
    }
    else
    {
        if (awtsParser->sequenceNumIncr)
        {
            ret = CdxStreamRead(awtsParser->file, tmp, 2);
            if (ret < 2)
            {
                CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
                if(ret < 0)
                {
                    return ret;
                }
                else
                {
                    awtsParser->mErrno = PSR_EOS;
                    CDX_LOGI("CdxStreamRead eos");
                    return -1;
                }
            }
            /*
            data = tmp;
            awtsParser->SequenceNumInPacket  = GetBE16Bits(data);
            */
        }
        ret = CdxStreamRead(awtsParser->file, tmp, 8);
        if (ret < 8)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                return ret;
            }
            else
            {
                awtsParser->mErrno = PSR_EOS;
                CDX_LOGI("CdxStreamRead eos");
                return -1;
            }
        }
        data = tmp;
        awtsParser->timeUs = (cdx_int64)GetLE64Bits(data) * 1000LL;
    }

    return awtsParser->isPacketHeader;
}

static cdx_int32 AwtsParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    AwtsParser *awtsParser = (AwtsParser*)parser;
    if(awtsParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGE("status < CDX_PSR_IDLE, BdParserGetMediaInfo invaild");
        awtsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    memcpy(pMediaInfo, &awtsParser->mediaInfo, sizeof(CdxMediaInfoT));
    return 0;
}

static cdx_int32 AwtsParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    AwtsParser *awtsParser = (AwtsParser*)parser;
    if(awtsParser->status != CDX_PSR_IDLE && awtsParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGW("status != CDX_PSR_IDLE && status != CDX_PSR_PREFETCHED, \
                 BdParserPrefetch invaild");
        awtsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(awtsParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if(awtsParser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(pkt, &awtsParser->pkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&awtsParser->statusLock);
    if(awtsParser->forceStop)
    {
        pthread_mutex_unlock(&awtsParser->statusLock);
        return -1;
    }
    awtsParser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&awtsParser->statusLock);

    int ret = 0;
    while(1)
    {
        if(awtsParser->forceStop)
        {
            CDX_LOGI("forceStop");
            awtsParser->mErrno = PSR_USER_CANCEL;
            ret = -1;
            goto _exit;
        }
        ret = AwtsReadHeader(awtsParser);
        if (ret < 0)
        {
            CDX_LOGE("AwtsReadHeader fail.");
            goto _exit;
        }
        else if(ret == 0)
        {
            break;
        }
        else
        {
            ret = CdxStreamSkip(awtsParser->file, awtsParser->curPacketLength - 14);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamSkip fail");
                goto _exit;
            }
        }
    }
    AwtsTrace *track = awtsParser->tracks[awtsParser->curTraceIndex];
    pkt->type = track->eType;
    pkt->length = awtsParser->curPacketLength;
    pkt->pts = awtsParser->timeUs;
    pkt->pcr = -1;
    pkt->flags |= (FIRST_PART|LAST_PART);
    pkt->streamIndex = track->streamIndex;
    
    memcpy(&awtsParser->pkt, pkt, sizeof(CdxPacketT));
    pthread_mutex_lock(&awtsParser->statusLock);
    awtsParser->status = CDX_PSR_PREFETCHED;
    pthread_mutex_unlock(&awtsParser->statusLock);
    pthread_cond_signal(&awtsParser->cond);
    return 0;
_exit:
    pthread_mutex_lock(&awtsParser->statusLock);
    awtsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&awtsParser->statusLock);
    pthread_cond_signal(&awtsParser->cond);
    return ret;
}

static cdx_int32 AwtsParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    AwtsParser *awtsParser = (AwtsParser*)parser;
    if(awtsParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_PREFETCHED, we can not read!");
        awtsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    pthread_mutex_lock(&awtsParser->statusLock);
    if(awtsParser->forceStop)
    {
        pthread_mutex_unlock(&awtsParser->statusLock);
        return -1;
    }
    awtsParser->status = CDX_PSR_READING;
    pthread_mutex_unlock(&awtsParser->statusLock);
    
    int ret;
    if(pkt->length <= pkt->buflen)
    {
        ret = CdxStreamRead(awtsParser->file, pkt->buf, pkt->length);
        if (ret < pkt->length)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                goto _exit;
            }
            else
            {
                awtsParser->mErrno = PSR_EOS;
                CDX_LOGI("CdxStreamRead eos");
                ret = -1;
                goto _exit;
            }
        }
    }
    else
    {
        ret = CdxStreamRead(awtsParser->file, pkt->buf, pkt->buflen);
        if (ret < pkt->buflen)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                goto _exit;
            }
            else
            {
                awtsParser->mErrno = PSR_EOS;
                CDX_LOGI("CdxStreamRead eos");
                ret = -1;
                goto _exit;
            }
        }
        ret = CdxStreamRead(awtsParser->file, pkt->ringBuf, pkt->length - pkt->buflen);
        if (ret < pkt->length - pkt->buflen)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                goto _exit;
            }
            else
            {
                awtsParser->mErrno = PSR_EOS;
                CDX_LOGI("CdxStreamRead eos");
                ret = -1;
                goto _exit;
            }
        }
    }
    pthread_mutex_lock(&awtsParser->statusLock);
    awtsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&awtsParser->statusLock);
    pthread_cond_signal(&awtsParser->cond);
    return 0;
_exit:
    pthread_mutex_lock(&awtsParser->statusLock);
    awtsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&awtsParser->statusLock);
    pthread_cond_signal(&awtsParser->cond);
    return ret;
}

cdx_int32 AwtsParserForceStop(CdxParserT *parser)
{
    CDX_LOGI("AwtsParserForceStop start");
    AwtsParser *awtsParser = (AwtsParser*)parser;

    pthread_mutex_lock(&awtsParser->statusLock);
    awtsParser->forceStop = 1;
    awtsParser->mErrno = PSR_USER_CANCEL;

    int ret = CdxStreamForceStop(awtsParser->file);
    if(ret < 0)
    {
        CDX_LOGW("CdxStreamForceStop fail");
    }
    while(awtsParser->status != CDX_PSR_IDLE && awtsParser->status != CDX_PSR_PREFETCHED)
    {
        pthread_cond_wait(&awtsParser->cond, &awtsParser->statusLock);
    }
    pthread_mutex_unlock(&awtsParser->statusLock);
    
    awtsParser->mErrno = PSR_USER_CANCEL;
    awtsParser->status = CDX_PSR_IDLE;
    CDX_LOGI("AwtsParserForceStop end");
    return 0;
}

cdx_int32 AwtsParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGI("AwtsParserClrForceStop start");
    AwtsParser *awtsParser = (AwtsParser*)parser;
    if(awtsParser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("awtsParser->status != CDX_PSR_IDLE");
        awtsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    
    awtsParser->forceStop = 0;
    int ret = CdxStreamClrForceStop(awtsParser->file);
    if(ret < 0)
    {
        CDX_LOGW("CdxStreamClrForceStop fail");
    }
    CDX_LOGI("AwtsParserClrForceStop end");
    return 0;
}


static int AwtsParserControl(CdxParserT *parser, int cmd, void *param)
{
    (void)param;
    //AwtsParser *awtsParser = (AwtsParser*)parser;
    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI(" awts parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            return AwtsParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return AwtsParserClrForceStop(parser);
        default:
            break;
    }
    return 0;
}

cdx_int32 AwtsParserGetStatus(CdxParserT *parser)
{
    AwtsParser *awtsParser = (AwtsParser *)parser;
    return awtsParser->mErrno;
}
/*
cdx_int32 AwtsParserSeekTo(CdxParserT *parser, cdx_int64  timeUs)
{
    //CDX_LOGD("AwtsParserSeekTo start, timeUs = %lld", timeUs);
    CDX_LOGE("it is not supported");
    return -1;
}*/
static cdx_int32 AwtsParserClose(CdxParserT *parser)
{
    AwtsParser *awtsParser = (AwtsParser *)parser;
    int ret = AwtsParserForceStop(parser);
    if(ret < 0)
    {
        CDX_LOGW("AwtsParserForceStop fail");
    }

    ret = CdxStreamClose(awtsParser->file);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamClose fail, ret(%d)", ret);
    }
    int i;
    for(i = 0; i < awtsParser->trackCount; i++)
    {
        if(awtsParser->tracks[i])
        {
            if(awtsParser->tracks[i]->pCodecSpecificData)
            {
                free(awtsParser->tracks[i]->pCodecSpecificData);
            }
            free(awtsParser->tracks[i]);
        }
    }
    pthread_mutex_destroy(&awtsParser->statusLock);
    pthread_cond_destroy(&awtsParser->cond);
    free(awtsParser);
    return 0;
}

cdx_uint32 AwtsParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    AwtsParser *awtsParser = (AwtsParser *)parser;
    return awtsParser->flags;
}

static int SetMediaInfo(AwtsParser *awtsParser)
{
    CdxMediaInfoT *mediaInfo = &awtsParser->mediaInfo;
    //mediaInfo->fileSize = awtsParser->fileSize;
    mediaInfo->programNum = 1;
    struct CdxProgramS *program = &mediaInfo->program[0];
    //program->duration = awtsParser->durationUs / 1000;//ms
    
    int i, j = 0;
    AwtsTrace *track;
    for(i = 0; i < awtsParser->trackCount; i++)
    {
        track = awtsParser->tracks[i];

        if (track->eType == CDX_MEDIA_VIDEO && VIDEO_STREAM_LIMIT > program->videoNum++)
        {
            VideoStreamInfo *video = &program->video[0];
            video->eCodecFormat = track->eCodecFormat;
            video->nWidth = track->nWidth;
            video->nHeight = track->nHeight;
            /*
            //两帧画面之间的间隔时间，是帧率的倒数，单位为us
            video->nFrameDuration = os->stream->codec->nFrameDuration =
                os->stream->codec->time_base.num * 1000000LL/ os->stream->codec->time_base.den;
            //表示每1000秒有多少帧画面被播放，例如25000、29970、30000等
            video->nFrameRate = 1000 * 1000000/video->nFrameDuration;
            */
            if(track->pCodecSpecificData)
            {
                video->nCodecSpecificDataLen = track->nCodecSpecificDataLen;
                video->pCodecSpecificData = track->pCodecSpecificData;
            }
            
            track->streamIndex = 0;
        }
        else if(track->eType == CDX_MEDIA_AUDIO && AUDIO_STREAM_LIMIT > j)
        {
            program->audio[j].eCodecFormat = track->eCodecFormat;
            if(program->audio[j].eCodecFormat == AUDIO_CODEC_FORMAT_MPEG_AAC_LC)
                program->audio[j].nFlags |= 1;

            program->audio[j].nChannelNum = track->nChannelNum;
            program->audio[j].nSampleRate = track->nSampleRate;
            program->audio[j].nBitsPerSample = track->nBitsPerSample;
            program->audio[j].nAvgBitrate = track->nBitrate;
            if(track->pCodecSpecificData)
            {
                program->audio[j].nCodecSpecificDataLen = track->nCodecSpecificDataLen;
                program->audio[j].pCodecSpecificData = track->pCodecSpecificData;
            }
            
            track->streamIndex = j++;
        }

    }
    program->audioNum = j;
    return 0;
}

int AwtsParserInit(CdxParserT *parser)
{
    CDX_LOGD("AwtsOpenThread start");
       AwtsParser *awtsParser = (AwtsParser*)parser;

    int ret;
    while (1)
    {
        ret = AwtsReadHeader(awtsParser);
        if (ret < 0)
        {
            CDX_LOGE("should not be here.");
            goto _exit;
        }
        else if(ret > 0)
        {
            ret = AwtsReadStreamInfo(awtsParser);
            if(ret < 0)
            {
                CDX_LOGE("AwtsReadStreamInfo fail");
                goto _exit;
            }
            break;
        }
        else
        {
            ret = CdxStreamSkip(awtsParser->file, awtsParser->curPacketLength);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamSkip fail");
                goto _exit;
            }
        }
    }
    SetMediaInfo(awtsParser);
    awtsParser->mErrno = PSR_OK;
    pthread_mutex_lock(&awtsParser->statusLock);
    awtsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&awtsParser->statusLock);
    pthread_cond_signal(&awtsParser->cond);
    CDX_LOGI("AwtsOpenThread success");
    return 0;

_exit:
    CDX_LOGE("AwtsOpenThread fail.");
    awtsParser->mErrno = PSR_OPEN_FAIL;
    pthread_mutex_lock(&awtsParser->statusLock);
    awtsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&awtsParser->statusLock);
    pthread_cond_signal(&awtsParser->cond);
    return -1;
}
static struct CdxParserOpsS awtsParserOps =
{
    .control         = AwtsParserControl,
    .prefetch         = AwtsParserPrefetch,
    .read             = AwtsParserRead,
    .getMediaInfo     = AwtsParserGetMediaInfo,
    .close             = AwtsParserClose,
    .attribute        = AwtsParserAttribute,
    .getStatus        = AwtsParserGetStatus,
    .init            = AwtsParserInit
};

CdxParserT *AwtsParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    AwtsParser *awtsParser = CdxMalloc(sizeof(AwtsParser));
    if(!awtsParser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(awtsParser, 0x00, sizeof(AwtsParser));

    int ret = pthread_cond_init(&awtsParser->cond, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_mutex_init(&awtsParser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);
    
    awtsParser->file = stream;
    awtsParser->flags = flags & MUTIL_AUDIO;
    awtsParser->base.ops = &awtsParserOps;
    awtsParser->base.type = CDX_PARSER_AWTS;
    awtsParser->mErrno = PSR_INVALID;
    awtsParser->status = CDX_PSR_INITIALIZED;
    return &awtsParser->base;
}

cdx_uint32 AwtsParserProbe(CdxStreamProbeDataT *probeData)
{
    cdx_char *data = probeData->buf;
    cdx_uint64 sp = 0;//用uint64是为了防止(*)处可能的数据回头
    
_sync:
    while(sp + 2 < probeData->len)
    {
        if (data[sp] == 0x00 &&
            data[sp + 1] == 0x00 &&
            data[sp + 2] == 0x5a)
            break;
        sp++;
    }
    if(sp >= probeData->len || probeData->len - sp < 14)
    {
        CDX_LOGE("Maybe Probe data is not enough. len(%u)", probeData->len);
        return 0;
    }
    sp += 9;
    unsigned packetFlags = data[sp++];
    
    if(packetFlags & (1<<7))
    {
        if (!memcmp(data + sp, "awts", 4))
        {
            CDX_LOGD("AwtsParserProbe = 1");
            return (probeData->len - sp)*100/probeData->len;
        }
        else
        {
            CDX_LOGD("it is not awts.");
            return 0;
        }
    }
    else
    {
        cdx_char *tmp = data + sp - 6;
        unsigned len = GetBE32Bits(tmp);
        if(packetFlags & (1<<5))
        {
            sp += 2;
        }
        sp += 8 + len;//(*)
        goto _sync;
    }
}

CdxParserCreatorT awtsParserCtor =
{
    .create    = AwtsParserOpen,
    .probe     = AwtsParserProbe
};

