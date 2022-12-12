/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : AsfParserItf.c
 * Description : AsfParser
 * Author  : xuqi <xuqi@allwinnertech.com>
 * Comment : 创建初始版本
 *
 */

#include <AsfParser.h>
#include <CdxMemory.h>
/*0/false for not same, 1/true for the same*/
#define GuidCmp(g1, g2) \
    (!memcmp(g1, g2, sizeof(AsfGuid)))

#define BUF_STORE_SIZE (4 * 1024 * 1024)
#define STREAM_NUM 2

struct BufStoreS
{
    cdx_char *buf;
    cdx_int32 len;
    cdx_bool intact;
};
enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
};
struct AsfParserImplS
{
    CdxParserT base;
    CdxStreamT *stream;
    cdx_uint32 flags;

    pthread_mutex_t mutex;

    unsigned int audioHeartIndex;
    unsigned int videoHeartIndex;

    cdx_bool initial;

    AsfContextT *ctx;
    enum CdxParserStatus status;
    CdxPacketT pkt;

    int mErrno;
    struct BufStoreS videoStore[2];
    struct BufStoreS audioStore[7];
};

static cdx_int32 __AsfPsrClose(CdxParserT *psr)
{
    struct AsfParserImplS *impl;
    impl = CdxContainerOf(psr, struct AsfParserImplS, base);

    int i;
    for (i = 0; i < 7; i++)
    {
        if (impl->audioStore[i].buf)
        {
            free(impl->audioStore[i].buf);
            impl->audioStore[i].buf = NULL;
        }
    }

    for (i = 0; i < 2; i++)
    {
        if (impl->videoStore[i].buf)
        {
            free(impl->videoStore[i].buf);
            impl->videoStore[i].buf = NULL;
        }
    }

    if(!(impl->flags & NO_NEED_CLOSE_STREAM))
    {
        CdxStreamClose(impl->stream);
    }
    AsfPsrCoreClose(impl->ctx);
    impl->ctx = NULL;

    pthread_mutex_destroy(&impl->mutex);
    free(impl);
    return 0;
}

static int asfOnPrefetch(struct AsfParserImplS *impl, CdxPacketT *pkt)
{
    cdx_int32 result;
    struct BufStoreS *store;

    memset(pkt, 0x00, sizeof(*pkt));

    result = AsfPsrCoreRead(impl->ctx);

    if (result != 0)
    {
        if (result == PSR_EOS)
        {
            impl->mErrno = PSR_EOS;
            CDX_LOGW("Try to read sample failed! EOS!");
            return -1;
        }
        else
        {
            impl->mErrno = PSR_IO_ERR;
            CDX_LOGW("Try to read sample failed! IO_ERROR!");
            return -1;
        }
    }

    pkt->type = impl->ctx->asf_st->codec.codec_type;

    pkt->pts = (impl->ctx->packet_frag_timestamp - impl->ctx->hdr.preroll) * 1000;
    pkt->streamIndex = impl->ctx->asf_st->typeIndex;

    if (pkt->type == CDX_MEDIA_VIDEO)
    {
        store = &impl->videoStore[pkt->streamIndex];
    }
    else
    {
        store = &impl->audioStore[pkt->streamIndex];
    }

    if (!store->buf)
    {
        store->buf = malloc(BUF_STORE_SIZE);
    }
    CDX_LOG_CHECK(store->len + impl->ctx->packet_frag_size <= BUF_STORE_SIZE,
      "store->len(%d), impl->ctx->packet_frag_size(%d), impl->ctx->packet_timestamp(%d)",
      store->len, impl->ctx->packet_frag_size, impl->ctx->packet_timestamp);

    if (pkt->type == CDX_MEDIA_VIDEO)
    {
        if (impl->ctx->frame_ctrl & 2)
        {
            CdxStreamRead(impl->stream, store->buf + store->len, impl->ctx->packet_frag_size);
            store->intact = CDX_TRUE;
            store->len += impl->ctx->packet_frag_size;
            pkt->length = store->len;
        }
        else if (impl->ctx->frame_ctrl & 1)
        {
            CdxStreamRead(impl->stream, store->buf + store->len, impl->ctx->packet_frag_size);
            store->intact = CDX_FALSE;
            store->len += impl->ctx->packet_frag_size;
            pkt->length = 0;
        }
        else
        {
            if (store->len == 0)
            {
                CdxStreamRead(impl->stream, store->buf, impl->ctx->packet_frag_size);
                store->intact = CDX_TRUE;
                store->len = impl->ctx->packet_frag_size;
                pkt->length = store->len;
            }
            else
            {
                CdxStreamRead(impl->stream, store->buf + store->len, impl->ctx->packet_frag_size);
                store->intact = CDX_FALSE;
                store->len += impl->ctx->packet_frag_size;
                pkt->length = 0;
            }
        }
    }
    else
    {
        CdxStreamRead(impl->stream, store->buf, impl->ctx->packet_frag_size);
        store->intact = CDX_TRUE;
        store->len = impl->ctx->packet_frag_size;
        pkt->length = store->len;
    }

    return 0;
}

static cdx_int32 __AsfPsrPrefetch(CdxParserT *psr, CdxPacketT *pkt)
{
    cdx_int32 ret;
    struct AsfParserImplS *impl;
    impl = CdxContainerOf(psr, struct AsfParserImplS, base);
    if (impl->mErrno == PSR_EOS)
    {
        CDX_LOGW("eos...");
        return -1;
    }
    if(impl->status == CDX_PSR_PREFETCHED)
    {
        memcpy(pkt, &impl->pkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&impl->mutex);
doPrefetch:
    do
    {
        ret = asfOnPrefetch(impl, pkt);
        if (ret != 0)
        {
            CDX_LOGE("prefetch failure.");
            break;
        }
    } while (pkt->length == 0);

    if (ret == 0 && (!impl->ctx->asf_st->active))
    {
        CdxStreamSkip(impl->stream, pkt->length);
        goto doPrefetch;
    }

    pkt->flags |= (FIRST_PART|LAST_PART);

    if (pkt->type == CDX_MEDIA_VIDEO)
    {
        if (impl->ctx->packet_key_frame)
        {
            pkt->flags |= KEY_FRAME;
        }
    }
    if(ret == 0)
    {
        memcpy(&impl->pkt, pkt, sizeof(CdxPacketT));
        impl->status = CDX_PSR_PREFETCHED;
    }
//    CDX_LOGD("pts(%.3f) type(%d) index(%d) length(%d)",
//            (double)pkt->pts/1000000.0, pkt->type, pkt->streamIndex, pkt->length);

    pthread_mutex_unlock(&impl->mutex);
   return ret;
}

static cdx_int32 __AsfPsrRead(CdxParserT *psr, CdxPacketT *pkt)
{
    struct AsfParserImplS *impl;
    impl = CdxContainerOf(psr, struct AsfParserImplS, base);

    struct BufStoreS *store; // = &impl->store[impl->ctx->asf_st->index];
    if (pkt->type == CDX_MEDIA_VIDEO)
    {
        store = &impl->videoStore[pkt->streamIndex];
    }
    else
    {
        store = &impl->audioStore[pkt->streamIndex];
    }

    CDX_CHECK(store->intact == CDX_TRUE);

   if (pkt->length <= pkt->buflen)
   {
       memcpy(pkt->buf, store->buf, pkt->length);
   }
   else
   {
       memcpy(pkt->buf, store->buf, pkt->buflen);
       memcpy(pkt->ringBuf, store->buf + pkt->buflen, pkt->length - pkt->buflen);
   }
    store->intact = CDX_FALSE;
    store->len = 0;

    if (pkt->type == CDX_MEDIA_VIDEO)
    {
        if ((impl->videoHeartIndex++ & 0x3fU) == 0x0U)
        {
           // CDX_LOGD("[video]: pts(%.3f) length(%d)", (double)pkt->pts/1000000.0, pkt->length);
        }

    }
    else if (pkt->type == CDX_MEDIA_AUDIO)
    {
        if ((impl->audioHeartIndex++ & 0x3fU) == 0x0U)
        {
           // CDX_LOGD("[audio]: pts(%.3f) length(%d)", (double)pkt->pts/1000000.0, pkt->length);
        }
    }
    impl->status = CDX_PSR_IDLE;
    return 0;
}

static cdx_int32 __AsfPsrGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    struct AsfParserImplS *impl;
    int streamIndex, typeIndex;
    impl = CdxContainerOf(parser, struct AsfParserImplS, base);

    //set if the media file contain audio, video or subtitle
    mediaInfo->fileSize = CdxStreamSize(impl->stream);
    mediaInfo->programNum = 1;
    mediaInfo->programIndex = 0;
    mediaInfo->bSeekable = CdxStreamSeekAble(impl->stream);
    /*
    if((!impl->ctx->index_ptr) && (impl->ctx->nb_packets == 0
    || impl->ctx->nb_packets == 0xffffffff) && (impl->ctx->totalBitRate == 0))
    {
       CDX_LOGD("cannot seek");
       mediaInfo->bSeekable = 0;
    }*/

    mediaInfo->program[0].audioNum = impl->ctx->audioNum;
    typeIndex = 0;
    for (streamIndex = 0; streamIndex < impl->ctx->nb_streams; streamIndex++)
    {
        if (impl->ctx->streams[streamIndex]->codec.codec_type == CDX_MEDIA_AUDIO)
        {
            struct AsfCodecS *codec = &impl->ctx->streams[streamIndex]->codec;
            AudioStreamInfo *audioSI = &mediaInfo->program[0].audio[typeIndex];

            audioSI->eCodecFormat = codec->codec_id;
            audioSI->eSubCodecFormat = codec->codec_tag;//codec->subCodecId;
            audioSI->nChannelNum = codec->channels;
            audioSI->nBitsPerSample = codec->bits_per_sample;
            audioSI->nSampleRate = codec->sample_rate;
            audioSI->nAvgBitrate = codec->bit_rate;
            audioSI->nBlockAlign = codec->block_align;

            audioSI->pCodecSpecificData = (char *)codec->extradata;
            audioSI->nCodecSpecificDataLen = codec->extradata_size;
            strcpy((char *)audioSI->strLang, "Unknown language");

            typeIndex++;
        }
    }

    mediaInfo->program[0].videoNum = impl->ctx->videoNum;
    typeIndex = 0;
    for (streamIndex = 0; streamIndex < impl->ctx->nb_streams; streamIndex++)
    {
        if (impl->ctx->streams[streamIndex]->codec.codec_type == CDX_MEDIA_VIDEO)
        {
            struct AsfCodecS *codec = &impl->ctx->streams[streamIndex]->codec;
            VideoStreamInfo *videoSI = &mediaInfo->program[0].video[typeIndex];

            videoSI->eCodecFormat = codec->codec_id;
            videoSI->nWidth = codec->width;
            videoSI->nHeight = codec->height;
            videoSI->nFrameRate = 25000;
            videoSI->pCodecSpecificData = (char *)codec->extradata;
            videoSI->nCodecSpecificDataLen = codec->extradata_size;

            typeIndex++;
        }
    }

    mediaInfo->program[0].subtitleNum = 0; /*container not support subtitle*/

    /*select audio stream*/
    CDX_LOGI("open flags(0x%x)", impl->flags);
    if (MutilAudioStream(impl->flags))
    {
        for (streamIndex = 0; streamIndex < impl->ctx->nb_streams; streamIndex++)
        {
            if (impl->ctx->streams[streamIndex]->codec.codec_type == CDX_MEDIA_AUDIO)
            {
                impl->ctx->streams[streamIndex]->active = 1;
            }
        }
        CDX_LOGI("mutil audio...");
        mediaInfo->program[0].audioIndex = 0;
    }
    else
    {
        for (streamIndex = 0; streamIndex < impl->ctx->nb_streams; streamIndex++)
        {
            if (impl->ctx->streams[streamIndex]->codec.codec_type == CDX_MEDIA_AUDIO)
            {
                impl->ctx->streams[streamIndex]->active = 1;
                mediaInfo->program[0].audioIndex = impl->ctx->streams[streamIndex]->typeIndex;
                break;
            }
        }
    }

    for (streamIndex = 0; streamIndex < impl->ctx->nb_streams; streamIndex++)
    {
        if (impl->ctx->streams[streamIndex]->codec.codec_type == CDX_MEDIA_VIDEO)
        {
            impl->ctx->streams[streamIndex]->active = 1;
         CDX_CHECK(impl->ctx->streams[streamIndex]->typeIndex == 0);
            mediaInfo->program[0].videoIndex = 0;
            break;
        }
    }

   if(impl->ctx->durationMs <= 0 && impl->ctx->fileSize != 0 && impl->ctx->totalBitRate)
   {
      impl->ctx->durationMs = impl->ctx->fileSize*8 / impl->ctx->totalBitRate * 1000;
   }

    //set total time
    mediaInfo->program[0].duration = impl->ctx->durationMs;

    return 0;
}

static cdx_int32 onForceStop(struct AsfParserImplS *impl)
{
    cdx_int32 ret = CDX_SUCCESS;

    impl->ctx->forceStop = 1;
    impl->mErrno = PSR_USER_CANCEL;
    ret = CdxStreamForceStop(impl->stream);
    if (ret != CDX_SUCCESS)
    {
        CDX_LOGE("stop stream failure... err(%d)", ret);
    }
    return ret;
}

static cdx_int32 onClrForceStop(struct AsfParserImplS *impl)
{
    cdx_int32 ret = CDX_SUCCESS;

    ret = CdxStreamClrForceStop(impl->stream);
    if (ret != CDX_SUCCESS)
    {
        CDX_LOGE("stop stream failure... err(%d)", ret);
    }
    impl->ctx->forceStop = 0;
    impl->mErrno = PSR_OK;
    return ret;
}

static int __AsfPsrControl(CdxParserT *psr, cdx_int32 cmd, void *param)
{
    (void)param;
    struct AsfParserImplS *impl;
    impl = CdxContainerOf(psr, struct AsfParserImplS, base);

    switch(cmd)
    {
        case CDX_PSR_CMD_SET_FORCESTOP:
            return onForceStop(impl);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return onClrForceStop(impl);
        default:
            CDX_LOGW("not support cmd(%d)", cmd);
           break;
    }

   return 0;
}

static cdx_int32 __AsfPsrSeekTo(CdxParserT *psr, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    struct AsfParserImplS *impl = NULL;
    int ret = 0;
    int i;
    impl = CdxContainerOf(psr, struct AsfParserImplS, base);
    CDX_LOGI("-------seekTo (%.3f)--------", (double)timeUs/1000000.0);

    pthread_mutex_lock(&impl->mutex);
    for (i = 0; i < 2; i++)
    {
        impl->videoStore[i].len = 0;
        impl->videoStore[i].intact = CDX_FALSE;
    }
    for (i = 0; i < 7; i++)
    {
        impl->audioStore[i].len = 0;
        impl->audioStore[i].intact = CDX_FALSE;
    }
   impl->status = CDX_PSR_IDLE;
    //1.force stop
    //2.wait idle
    //3.seek
    if (timeUs >= impl->ctx->durationMs * 1000)
    {
        CDX_LOGW("seekTo (%lld/%lld) ms", timeUs/1000, impl->ctx->durationMs);
        impl->mErrno = PSR_EOS;
        pthread_mutex_unlock(&impl->mutex);
        return 0;
    }

    ret = AsfPsrCoreIoctrl(impl->ctx, CDX_MEDIA_STATUS_JUMP, (uintptr_t)&timeUs);
    CDX_LOGI("-------seekTo done--------");
    pthread_mutex_unlock(&impl->mutex);
    return ret;
}

static cdx_uint32 __AsfPsrAttribute(CdxParserT *psr)
{
    struct AsfParserImplS *impl = NULL;

    impl = CdxContainerOf(psr, struct AsfParserImplS, base);
    return impl->flags;
}

static cdx_int32 __AsfPsrGetStatus(CdxParserT *psr)
{
    struct AsfParserImplS *impl = NULL;

    impl = CdxContainerOf(psr, struct AsfParserImplS, base);

    return impl->mErrno;
}

static int __AsfPsrInit(CdxParserT *psr)
{
    struct AsfParserImplS *impl = NULL;
    cdx_int32 result = 0;

    impl = CdxContainerOf(psr, struct AsfParserImplS, base);

    CDX_LOGI("asf parser init begin...");

    pthread_mutex_init(&impl->mutex, NULL);
    //init asf parser lib module
    impl->ctx = AsfPsrCoreInit();
    if (!impl->ctx)
    {
        CDX_LOGW("Initiate asf file parser lib module failed!");
        goto failure;
    }

    result = AsfPsrCoreOpen(impl->ctx, impl->stream);

   if(result < 0)
   {
      CDX_LOGW("open asf/wmv reader failed");
      goto failure;
   }

    impl->ctx->status = CDX_MEDIA_STATUS_IDLE;

    impl->mErrno = PSR_OK;
    impl->audioHeartIndex = 0;
    impl->videoHeartIndex = 0;
    impl->status = CDX_PSR_IDLE;
    CDX_LOGI("asf parser init success...");

    AsfPsrCoreIoctrl(impl->ctx, CDX_MEDIA_STATUS_PLAY, 0);

    return 0;

failure:

    return -1;
}

struct CdxParserOpsS asfPsrOps =
{
   .init = __AsfPsrInit,
    .control = __AsfPsrControl,
    .prefetch = __AsfPsrPrefetch,
    .read = __AsfPsrRead,
    .getMediaInfo = __AsfPsrGetMediaInfo,
    .seekTo = __AsfPsrSeekTo,
    .attribute = __AsfPsrAttribute,
    .getStatus = __AsfPsrGetStatus,
    .close = __AsfPsrClose
};

static cdx_uint32 __AsfPsrProbe(CdxStreamProbeDataT *probeData)
{
    if (probeData->len < sizeof(asf_header))
    {
        return 0;
    }

    return GuidCmp(probeData->buf, &asf_header)*100;
}

static CdxParserT *__AsfPsrCreate(CdxStreamT *stream, cdx_uint32 flags)
{
    struct AsfParserImplS *impl = NULL;
    cdx_int32 result = 0;

    impl = CdxMalloc(sizeof(*impl));

    memset(impl, 0x00, sizeof(*impl));
    impl->stream = stream;
    impl->base.ops = &asfPsrOps;
    impl->mErrno = PSR_INVALID;
    impl->flags = flags;

    return &impl->base;
}

struct CdxParserCreatorS asfParserCtor =
{
    .probe = __AsfPsrProbe,
    .create = __AsfPsrCreate
};

