/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxRemuxParser.c
 * Description : deliver es stream from rtsp, can be extended.
 * History :
 *
 */

//#define CONFIG_LOG_LEVEL 4

#include <CdxTypes.h>
#include <cdx_log.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxParser.h>
#include <CdxRtspSpec.h>
#include "mpeg4Vol.h"

#define RTSP_CACHE_TIME (10*1000000) //10s
#define MAX_PKT_NUM 50

//#define SAVE_STREAM

#ifdef SAVE_STREAM
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static FILE *fp_bs_rtsp = NULL;

static void CdxStreamSave(void *buf, cdx_uint32 len)
{
    if (fp_bs_rtsp == NULL)
    {
        fp_bs_rtsp = fopen("/data/camera/rtsp_stream", "wb+");
        if (fp_bs_rtsp == NULL)
        {
            CDX_LOGE("fopen failure, errno(%d)", errno);
        }
    }
    fwrite(buf, 1, len, fp_bs_rtsp);
    sync();

    CDX_LOGV("==== CdxStreamSave %s,%u", __FUNCTION__, len);
    return;
}
#endif
#if 0
static void GetMediaInfo(CdxMediaInfoT *info, struct CedarXRtspMetaS *meta)
{
    memset(info, 0x00, sizeof(*info));

    info->fileSize = 0;
    info->bSeekable = CDX_FAILURE;

    info->programNum = 0;
    info->programIndex = 0;

    info->program[0].id = 0;
    info->program[0].flags = 0;
    info->program[0].duration = 0;

    info->program[0].subtitleNum = 0;
    info->program[0].subtitleIndex = -1;

    if (meta->header.hasAudio)
    {
        info->program[0].audioNum = 1;
        info->program[0].audioIndex = 0;

        info->program[0].audio[0].eCodecFormat = meta->audioRegion.codingType;
        info->program[0].audio[0].eSubCodecFormat = meta->audioRegion.subCodingType;

        info->program[0].audio[0].nChannelNum = meta->audioRegion.channels;
        info->program[0].audio[0].nSampleRate = meta->audioRegion.samplePerSecond;

        if (meta->audioRegion.extraDataLen)
        {
            info->program[0].audio[0].nFlags = 0;
            info->program[0].audio[0].nCodecSpecificDataLen = meta->audioRegion.extraDataLen;
            info->program[0].audio[0].pCodecSpecificData = meta->audioExtraData;
        }
        else
        {
            info->program[0].audio[0].nFlags = 1;
        }
    }

    if (meta->header.hasVideo)
    {
        info->program[0].videoNum = 1;
        info->program[0].videoIndex = 0;

        info->program[0].video[0].eCodecFormat = meta->videoRegion.codingType;
        info->program[0].video[0].nWidth = meta->videoRegion.width;
        info->program[0].video[0].nHeight = meta->videoRegion.height;

        info->program[0].video[0].nFrameRate = 0;
        info->program[0].video[0].nFrameDuration = 0;
        info->program[0].video[0].nAspectRatio = 0;

        info->program[0].video[0].bIs3DStream = 0;
        info->program[0].video[0].nCodecSpecificDataLen = meta->videoRegion.extraDataLen;
        info->program[0].video[0].pCodecSpecificData = meta->videoExtraData;
    }

    return ;
}
#endif

struct CdxRemuxParserImplS
{
    CdxParserT base;
    CdxStreamT *stream;
    CdxMediaInfoT   pMediaInfo;
    ExtraDataContainerT extraDataContainer;
    cdx_int32       exitFlag;
    cdx_int32       mErrno;
    cdx_int32       mStatus;
    pthread_mutex_t statusMutex;
    //CdxRtspPacketT      curPkt;

    cdx_int64   preAudiopts;
    cdx_int64   preVideopts;

    pthread_t    thread_id;
    cdx_int32        thread_exit;

    cdx_int32     pause_fetch;
    cdx_int32     fetch_is_pause;

    cdx_uint32        hasAudio;
    cdx_uint32        hasVideo;

    cdx_int64        fetch_packet_time;
    cdx_int64        read_packet_time;

    VideoStreamInfo vStreamInfo;

    CdxPacketT *pkts[MAX_PKT_NUM];
    cdx_uint32 pktNum;
    cdx_uint32 pktPos;
    cdx_uint8 *vdata;
    cdx_int32 vdataLen;
};

cdx_int32 __RemuxPsrSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    (void)parser;
    (void)timeUs;
    CDX_LOGE("not support...");
    return -1;
}

cdx_uint32 __RemuxPsrAttribute(CdxParserT *parser) /*return falgs define as open's flags*/
{
    (void)parser;
    return 0;
}

cdx_int32 __RemuxPsrGetStatus(CdxParserT *parser) /*return enum CdxPrserStatusE*/
{
    (void)parser;
    return PSR_OK;
}

cdx_int32 __RemuxPsrControl(CdxParserT *parser, int cmd, void *param)
{
    struct CdxRemuxParserImplS *impl;

    impl = CdxContainerOf(parser, struct CdxRemuxParserImplS, base);

    switch(cmd)
    {
    case CDX_PSR_CMD_GET_CACHESTATE:
    {
        int ret;
        struct ParserCacheStateS *psrCS = param;
        struct StreamCacheStateS stCS;
        ret = CdxStreamControl(impl->stream, STREAM_CMD_GET_CACHESTATE, &stCS);
        if (ret != CDX_SUCCESS)
        {
            CDX_LOGE("get cache state failure, err(%d)", ret);
            return -1;
        }
        psrCS->nCacheCapacity = stCS.nCacheCapacity;
        psrCS->nBandwidthKbps = stCS.nBandwidthKbps;
        psrCS->nCacheSize = stCS.nCacheSize;
        psrCS->nCacheCapacity = stCS.nCacheCapacity;
        psrCS->nPercentage = stCS.nPercentage;
        return 0;
    }
    default:
        CDX_LOGW("cmd(%d) not implment now...", cmd);
        break;
    }

    return CDX_SUCCESS;
}

cdx_int32 __RemuxPsrPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    struct CdxRemuxParserImplS *impl;
    cdx_int32 ret;

    CdxRtspPktHeaderT ptkHead;

    impl = CdxContainerOf(parser, struct CdxRemuxParserImplS, base);

    memset(pkt, 0x00, sizeof(*pkt));

    if(impl->pktPos < impl->pktNum)
    {
        memcpy(pkt, impl->pkts[impl->pktPos], sizeof(*pkt));
        return 0;
    }

    ret = CdxStreamRead(impl->stream, &ptkHead, sizeof(ptkHead));
    if (ret != 0)
    {
        CDX_LOGE("read failure..., req(%d), ret(%d)", (int)sizeof(ptkHead), ret);
        return -1;
    }

    pkt->type = ptkHead.type;
    pkt->length = ptkHead.length;
    pkt->pts = ptkHead.pts;
    pkt->streamIndex = 0;
#if 1
    if(pkt->type == CDX_MEDIA_AUDIO)
    {
        pkt->flags |= (FIRST_PART|LAST_PART);
    }
    if(pkt->type == CDX_MEDIA_VIDEO)
    {
        logv("impl->preVideopts = %lld, ptkHead.pts = %lld",impl->preVideopts,ptkHead.pts);
        //* set the FIRST_PART when the pts is not the same
        if(impl->preVideopts != ptkHead.pts || impl->preVideopts == 0)
        {
            pkt->flags |= FIRST_PART;
        }
        pkt->pts = ptkHead.pts;
        impl->preVideopts = ptkHead.pts;
    }
#endif

    logv("==========prefetch pkt->type(%d),pkt->length(%d), pkt->pts(%lld), pkt->flags(%d)",
    pkt->type, pkt->length, pkt->pts, pkt->flags);
    if (pkt->type != CDX_MEDIA_AUDIO && pkt->type != CDX_MEDIA_VIDEO)
    {
        CDX_LOGD("pkt type(%d), length(%d)", pkt->type, pkt->length);

        memset(pkt, 0x00, sizeof(*pkt));
        return 0;
    }

    return 0;
}

cdx_int32 __RemuxPsrRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 ret;
    struct CdxRemuxParserImplS *impl;

    impl = CdxContainerOf(parser, struct CdxRemuxParserImplS, base);
    //* read from impl->pkts first
    if(impl->pktPos < impl->pktNum)
    {
        cdx_uint8 *data = (cdx_uint8*)impl->pkts[impl->pktPos]->buf;
        if(pkt->buflen >= pkt->length)
        {
            memcpy(pkt->buf, data, pkt->length);
        }
        else
        {
            memcpy(pkt->buf, data, pkt->buflen);
            memcpy(pkt->ringBuf, data + pkt->buflen, pkt->length - pkt->buflen);
        }
        if(impl->pkts[impl->pktPos])
        {
            if(impl->pkts[impl->pktPos]->buf)
            {
                free(impl->pkts[impl->pktPos]->buf);
            }
            free(impl->pkts[impl->pktPos]);
            impl->pkts[impl->pktPos] = NULL;
        }
        impl->pktPos++;
        return 0;
    }

    if (pkt->buflen >= pkt->length)
    {
        ret = CdxStreamRead(impl->stream, pkt->buf, pkt->length);
        if (ret != 0)
        {
            CDX_LOGE("read stream error.");
            return -1;
        }

#ifdef SAVE_STREAM
        if (pkt->type == CDX_MEDIA_VIDEO)
        {
            CdxStreamSave(pkt->buf, pkt->length);
        }
#endif

    }
    else
    {
        CDX_LOG_CHECK(pkt->buflen + pkt->ringBufLen >= pkt->length,
            "buffer not enough (%d,%d,%d)", pkt->buflen, pkt->ringBufLen, pkt->length);

        logv("============ pkt->buflen(%d), pkt->ringBufLen(%d), pkt->length(%d)",
            pkt->buflen, pkt->ringBufLen, pkt->length);

        ret = CdxStreamRead(impl->stream, pkt->buf, pkt->buflen);
        if (ret != 0)
        {
            CDX_LOGE("read stream error.");
            return -1;
        }

#ifdef SAVE_STREAM
        if (pkt->type == CDX_MEDIA_VIDEO)
        {
            CdxStreamSave(pkt->buf, pkt->buflen);
        }
#endif

        ret = CdxStreamRead(impl->stream, pkt->ringBuf, pkt->length - pkt->buflen);
        if (ret != 0)
        {
            CDX_LOGE("read stream error.");
            return -1;
        }

#ifdef SAVE_STREAM
        if (pkt->type == CDX_MEDIA_VIDEO)
        {
            CdxStreamSave(pkt->ringBuf, pkt->length - pkt->buflen);
        }
#endif
    }

    return 0;
}

cdx_int32 __RemuxPsrGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    struct CdxRemuxParserImplS *impl;

    impl = CdxContainerOf(parser, struct CdxRemuxParserImplS, base);

    memset(pMediaInfo, 0x00, sizeof(CdxMediaInfoT));
    memcpy(pMediaInfo, impl->extraDataContainer.extraData, sizeof(CdxMediaInfoT));

    pMediaInfo->programIndex = 0;
    pMediaInfo->programNum = 1;
    pMediaInfo->program[0].audioIndex = 0;
    pMediaInfo->program[0].videoIndex = 0;

    if(pMediaInfo->program[0].video[0].pCodecSpecificData &&
        pMediaInfo->program[0].video[0].eCodecFormat == VIDEO_CODEC_FORMAT_XVID)
    {
        int width, height;
        mpeg4getvolhdr((cdx_uint8*)pMediaInfo->program[0].video[0].pCodecSpecificData,
                    pMediaInfo->program[0].video[0].nCodecSpecificDataLen, &width, &height);
        if(width > 0 && height > 0)
        {
            pMediaInfo->program[0].video[0].nHeight = height;
            pMediaInfo->program[0].video[0].nWidth = width;
        }
        logv("width(%d), nHeight(%d)", width, height);
    }
    else if(impl->vdata && pMediaInfo->program[0].video[0].eCodecFormat == VIDEO_CODEC_FORMAT_H263)
    {
        int width, height;
        //char a[32]={00, 0x00, 0x80, 0x02, 0x08, 0x04, 0x26, 0x20, 0x20, 0x20, 0x21,
        //0xFF, 0xFF, 0x31, 0x01, 0x01, 0x01, 0x0F, 0xFF, 0xF9, 0x88, 0x08, 0x08, 0x08,
        //0x7F, 0xFF, 0xCC, 0x40, 0x40, 0x40, 0x43, 0xFF};
        //h263GetPicHdr((cdx_uint8*)pMediaInfo->program[0].video[0].pCodecSpecificData,
        //            pMediaInfo->program[0].video[0].nCodecSpecificDataLen, &width, &height);
        h263GetPicHdr(impl->vdata, impl->vdataLen, &width, &height);
        if(width > 0 && height > 0)
        {
            pMediaInfo->program[0].video[0].nHeight = height;
            pMediaInfo->program[0].video[0].nWidth = width;
        }

        logv("width(%d), nHeight(%d)", width, height);
    }

    return 0;
}

cdx_int32 __RemuxPsrClose(CdxParserT *parser)
{
    struct CdxRemuxParserImplS *impl;
    cdx_uint32 i;

    impl = CdxContainerOf(parser, struct CdxRemuxParserImplS, base);

    CdxStreamClose(impl->stream);

#ifdef SAVE_STREAM
    if (fp_bs_rtsp != NULL)
    {
        fclose(fp_bs_rtsp);
        fp_bs_rtsp = NULL;
    }
#endif

    for(i = 0; i < impl->pktNum; i++)
    {
        if(impl->pkts[i])
        {
            if(impl->pkts[i]->buf)
            {
                free(impl->pkts[i]->buf);
            }
            free(impl->pkts[i]);
            impl->pkts[i] = NULL;
        }
    }
    if(impl->vdata)
    {
        free(impl->vdata);
    }

    if(impl->extraDataContainer.extraData)
    {
        free(impl->extraDataContainer.extraData);
        impl->extraDataContainer.extraData = NULL;
    }
    free(impl);

    return 0;
}

static int GetVideoPkt(struct CdxRemuxParserImplS *impl)
{
    CdxPacketT *pkt;
    int ret;
    CdxRtspPktHeaderT ptkHead;

    while(1)
    {
        pkt = (CdxPacketT *)malloc(sizeof(CdxPacketT));
        if(!pkt)
        {
            loge("malloc failed.");
            return -1;
        }
        else
        {
            memset(pkt, 0x00, sizeof(CdxPacketT));
        }

        //* prefetch
        ptkHead.length = 0;
        while (ptkHead.length == 0)
        {
            ret = CdxStreamRead(impl->stream, &ptkHead, sizeof(ptkHead));
            if (ret != 0)
            {
                CDX_LOGE("read failure..., req(%d), ret(%d)", (int)sizeof(ptkHead), ret);
                if(pkt!=NULL)
                {
                    free(pkt);
                    pkt=NULL;
                }
                return -1;
            }

        }
        pkt->type = ptkHead.type;
        pkt->length = ptkHead.length;
        pkt->pts = ptkHead.pts;

        pkt->streamIndex = 0;
        if(pkt->type == CDX_MEDIA_AUDIO || pkt->type == CDX_MEDIA_VIDEO)
        {
            if(pkt->type == CDX_MEDIA_AUDIO)
            {
                pkt->flags |= (FIRST_PART|LAST_PART);
            }
            if(pkt->type == CDX_MEDIA_VIDEO)
            {
                //* set the FIRST_PART when the pts is not the same
                if(impl->preVideopts != ptkHead.pts || impl->preVideopts == 0)
                {
                    pkt->flags |= FIRST_PART;
                }
                pkt->pts = ptkHead.pts;
                impl->preVideopts = ptkHead.pts;
            }
        }

        //* read
        pkt->buf = (char *)malloc(pkt->length);
        if(!pkt->buf)
        {
            loge("malloc failed.");
            return -1;
        }
        pkt->buflen = pkt->length;
        ret = CdxStreamRead(impl->stream, pkt->buf, pkt->length);
        if(ret != 0)
        {
            loge("read failed.");
            return -1;
        }
        if(impl->pktNum >= MAX_PKT_NUM)
        {
            loge("packet num >= MAX_PKT_NUM");
            return -1;
        }

        impl->pkts[impl->pktNum++] = pkt;
        if(pkt->type == CDX_MEDIA_VIDEO)
        {
            logd("pkt length=%d", pkt->length);
            impl->vdataLen = pkt->length;
            impl->vdata = (cdx_uint8 *)malloc(impl->vdataLen);
            if(!impl->vdata)
            {
                loge("malloc failed.");
                return -1;
            }
            memcpy(impl->vdata, pkt->buf, impl->vdataLen);
            break;
        }
    }

    return 0;
}

static int __RemuxPsrInit(CdxParserT *parser)
{
    struct CdxRemuxParserImplS *impl;
    cdx_int32 ret, count;
    CdxMediaInfoT *pMediaInfo;

    impl = CdxContainerOf(parser, struct CdxRemuxParserImplS, base);

    ExtraDataContainerT *extraDataContainer = NULL;
    if(CdxStreamGetMetaData(impl->stream, "extra-data", (void **)&extraDataContainer) == 0
        && extraDataContainer)
    {
        impl->extraDataContainer.extraDataType = extraDataContainer->extraDataType;
        if(extraDataContainer->extraDataType == EXTRA_DATA_RTSP)
        {
            impl->extraDataContainer.extraData = (CdxMediaInfoT *)malloc(sizeof(CdxMediaInfoT));
            memcpy(impl->extraDataContainer.extraData, extraDataContainer->extraData,
                sizeof(CdxMediaInfoT));
        }
        CDX_LOGD("xxx get mediainfo.");
    }
    else
    {
        CDX_LOGE("get extra-data failed.");
        goto err_out;
    }

    pMediaInfo = impl->extraDataContainer.extraData;
    impl->hasAudio = pMediaInfo->program[0].audioNum;
    impl->hasVideo = pMediaInfo->program[0].videoNum;
    impl->read_packet_time = -1;

    if(!impl->hasAudio && !impl->hasVideo)
    {
        CDX_LOGE("no av found!");
        goto err_out;
    }

    //* pre-read some pkt save to impl->pkt, for parse video width/height..
    if(impl->hasVideo)
    {
        ret = GetVideoPkt(impl);
        if(ret < 0)
        {
            logw("get video pkt failed.");
            goto err_out;
        }
    }

    return 0;

err_out:
    if(impl->extraDataContainer.extraData)
    {
        free(impl->extraDataContainer.extraData);
        impl->extraDataContainer.extraData = NULL;
    }

    return -1;
}

struct CdxParserOpsS remuxParserOps =
{
    .control        = __RemuxPsrControl,
    .prefetch       = __RemuxPsrPrefetch,
    .read           = __RemuxPsrRead,
    .getMediaInfo   = __RemuxPsrGetMediaInfo,
    .seekTo         = __RemuxPsrSeekTo,
    .attribute      = __RemuxPsrAttribute,
    .getStatus      = __RemuxPsrGetStatus,
    .close          = __RemuxPsrClose,
    .init           = __RemuxPsrInit,
};

static CdxParserT *__RemuxPsrCreate(CdxStreamT *stream, cdx_uint32 flags)
{
    (void)flags;
    struct CdxRemuxParserImplS *impl = NULL;

    impl = CdxMalloc(sizeof(*impl));
    CDX_FORCE_CHECK(impl);
    memset(impl, 0x00, sizeof(*impl));

    impl->base.ops = &remuxParserOps;
    impl->stream = stream;

    return &impl->base;
}

static cdx_uint32 __RemuxPsrProbe(CdxStreamProbeDataT *probeData)
{
    if (!probeData || probeData->len < 9 || memcmp(probeData->buf, "remux", 5))
    {
        CDX_LOGW("not privite remux file...");
        return 0;
    }

    CDX_LOGI("privite remux file...");
    return 100;
}

CdxParserCreatorT remuxParserCtor =
{
    .create = __RemuxPsrCreate,
    .probe = __RemuxPsrProbe
};

