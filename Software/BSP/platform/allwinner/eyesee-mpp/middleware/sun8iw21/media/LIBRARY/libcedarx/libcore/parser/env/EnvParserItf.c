/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : EnvParserItf.c
 * Description : Envelope
 * Author  : xuqi <xuqi@allwinnertech.com>
 * Comment : 创建初始版本
 *
 */

#include <AsfParser.h>

#if BOARD_USE_PLAYREADY
#include <Envelope.h>
#include <CdxMemory.h>

#include <drmcrt.h>
#include <drmcontextsizes.h>
#include <drmmanager.h>
#include <drmrevocation.h>
#include <drmcmdlnpars.h>
#include <drmtoolsutils.h>
#include <drmtoolsinit.h>
#include <drmconstants.h>

#define BUF_STORE_SIZE (4 * 1024 * 1024)
#define STREAM_NUM 2
#define ENVELOPE_SIGNATURE (0x07455250)

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
struct EnvParserImplS
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

////////////////////////////////////////////////////////////////////////////////

struct CdxEnvStreamImplS
{
    CdxStreamT base;
    CdxStreamT *innerstream;
    DRM_APP_CONTEXT *drm_app_context;
    DRM_BYTE *drm_opaque_buffer;
    DRM_BYTE *drm_revocation_buffer;
    DRM_ENVELOPED_FILE_CONTEXT env_file_context;
    DRM_DECRYPT_CONTEXT drm_decrypt_context;
    DRM_BOOL is_decrypt_init;
    DRM_BOOL is_env_open;
};

static CdxStreamProbeDataT *__EnvStreamGetProbeData(CdxStreamT *stream)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    return CdxStreamGetProbeData(impl->innerstream);
}

static cdx_int32 __EnvStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    struct CdxEnvStreamImplS *impl;
   cdx_int64 nHadReadLen;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    DRM_DWORD read;
    Env_Envelope_Read(&impl->env_file_context, buf, len, &read);
    //CDX_LOGD("%s:%d, buf=%p, len=%d, read=%d", __func__, __LINE__, buf, len, read);

    return read;
}

static cdx_int32 __EnvStreamClose(CdxStreamT *stream)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    CDX_LOGD("do env cleaning.");

    if (impl->is_decrypt_init)
    {
        Drm_Reader_Close(&impl->drm_decrypt_context);
    }

    if (impl->is_env_open)
    {
        Env_Envelope_Close(&impl->env_file_context);
    }

    DRMTOOLS_DrmUninitializeWithOpaqueBuffer(&impl->drm_opaque_buffer, &impl->drm_app_context);

    SAFE_OEM_FREE(impl->drm_revocation_buffer);

    CdxStreamClose(impl->innerstream);
    CdxFree(impl);
    return CDX_SUCCESS;
}

static cdx_int32 __EnvStreamGetIoState(CdxStreamT *stream)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    return CdxStreamGetIoState(impl->innerstream);
}

static cdx_uint32 __EnvStreamAttribute(CdxStreamT *stream)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    return CdxStreamAttribute(impl->innerstream);
}

static cdx_int32 __EnvStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    return CdxStreamControl(impl->innerstream, cmd, param);
}

static cdx_int32 __EnvStreamSeek(CdxStreamT *stream, cdx_int64 offset, cdx_int32 whence)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    DRM_DWORD method = whence; // 0:set, 1:cur, 2:end
    DRM_DWORD new_pos;
    DRM_RESULT ret = Env_Envelope_Seek(&impl->env_file_context, offset, method, &new_pos);
    //CDX_LOGD("%s:%d, off=%lld, whence=%d, new=%d, ret=%d",
    //__func__, __LINE__, offset, whence, new_pos, ret);
    return ret == 0 ? 0 : -1;
}

static cdx_int64 __EnvStreamTell(CdxStreamT *stream)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    DRM_DWORD new_pos;
    DRM_RESULT ret = Env_Envelope_Seek(&impl->env_file_context, 0, OEM_FILE_CURRENT, &new_pos);
    cdx_int64 pos = (ret == 0 ? (cdx_int64)new_pos : -1);
    //CDX_LOGD("pos=%lld", pos);
    return pos;
}

static cdx_bool __EnvStreamEos(CdxStreamT *stream)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    // same as stream, due to encrypt-info saved in the begining of file
    return CdxStreamEos(impl->innerstream);
}

static cdx_int64 __EnvStreamSize(CdxStreamT *stream)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    DRM_DWORD filesize = -1;
    DRM_RESULT ret = Env_Envelope_GetSize(&impl->env_file_context, &filesize);
    //CDX_LOGD("filesize=%d", filesize);
    return ret == 0 ? (cdx_int64)filesize : -1;
}

static cdx_int32 __EnvStreamGetMetaData(CdxStreamT *stream, const cdx_char *key, void **pVal)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    return CdxStreamGetMetaData(impl->innerstream, key, pVal);
}

cdx_int32 __EnvStreamConnect(CdxStreamT *stream)
{
    struct CdxEnvStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxEnvStreamImplS, base);
    CDX_CHECK(impl->innerstream);

    return CdxStreamConnect(impl->innerstream);
}

static struct CdxStreamOpsS envStreamOps =
{
   .connect = __EnvStreamConnect,
    .getProbeData = __EnvStreamGetProbeData,
    .read = __EnvStreamRead,
    .write = NULL,
    .close = __EnvStreamClose,
    .getIOState = __EnvStreamGetIoState,
    .attribute = __EnvStreamAttribute,
    .control = __EnvStreamControl,
    .getMetaData = __EnvStreamGetMetaData,
    .seek = __EnvStreamSeek,
    .seekToTime = NULL,
    .eos = __EnvStreamEos,
    .tell = __EnvStreamTell,
    .size = __EnvStreamSize,
};

static DRM_RESULT DRM_CALL BindCallback(
    __in     const DRM_VOID                 *f_pvPolicyCallbackData,
    __in           DRM_POLICY_CALLBACK_TYPE  f_dwCallbackType,
    __in_opt const DRM_KID                  *f_pKID,
    __in_opt const DRM_LID                  *f_pLID,
    __in     const DRM_VOID                 *f_pv )
{
    CDX_UNUSE(f_pKID);
    CDX_UNUSE(f_pLID);
    CDX_UNUSE(f_pv);
    return DRMTOOLS_PrintPolicyCallback( f_pvPolicyCallbackData, f_dwCallbackType );
}

void _pfDebugAnalyzeDR(unsigned long dr, const char* file, unsigned long line, const char* expr)
{
    if (dr != 0)
        CDX_LOGD("%s:%ld 0x%x(%s)\n", strrchr(file, '/') + 1, line, (int)dr, expr);
}

static inline void stringToWString(DRM_WCHAR *dst, char *src)
{
    while ((*dst++ = *src++));
}

static CdxStreamT *__EnvStreamCreate(CdxStreamT *innerstream)
{
    struct CdxEnvStreamImplS *impl;

    impl = CdxMalloc(sizeof(*impl));
    CDX_FORCE_CHECK(impl);
    memset(impl, 0x00, sizeof(*impl));

    impl->base.ops = &envStreamOps;
    impl->innerstream = innerstream;

    CDX_LOGD("innerstream=%p", innerstream);

    //SetDbgAnalyzeFunction(_pfDebugAnalyzeDR);

    // /data/local/pr.hds
   char prhds[] = "/data/local/pr.hds";
    DRM_WCHAR wprhds[sizeof(prhds)] = {0};
    stringToWString(wprhds, prhds);
    const DRM_CONST_STRING pdstrDataStoreFile = DRM_CREATE_DRM_STRING(wprhds);

    DRM_RESULT dr = DRM_SUCCESS;
    const DRM_CONST_STRING *rights[1] = { &g_dstrWMDRM_RIGHT_PLAYBACK };
    impl->is_decrypt_init = FALSE;
    impl->is_env_open = FALSE;

    ChkDR( DRMTOOLS_DrmInitializeWithOpaqueBuffer(NULL,
                                                  &pdstrDataStoreFile,
                                                  TOOLS_DRM_BUFFER_SIZE,
                                                  &impl->drm_opaque_buffer,
                                                  &impl->drm_app_context) );
    if (DRM_REVOCATION_IsRevocationSupported())
    {
        ChkMem( impl->drm_revocation_buffer = (DRM_BYTE *)Oem_MemAlloc(REVOCATION_BUFFER_SIZE) );
        ChkDR( Drm_Revocation_SetBuffer(impl->drm_app_context, impl->drm_revocation_buffer,
                                     REVOCATION_BUFFER_SIZE) );
    }

    ChkDR( Env_Envelope_Open(impl->drm_app_context, NULL, innerstream, &impl->env_file_context) );
    impl->is_env_open = TRUE;

    ChkDR( Drm_Reader_Bind(impl->drm_app_context,
                          rights,
                          DRM_NO_OF(rights),
                          (DRMPFNPOLICYCALLBACK)BindCallback,
                          NULL,
                          &impl->drm_decrypt_context) );
    impl->is_decrypt_init = TRUE;

    ChkDR( Env_Envelope_InitializeRead(&impl->env_file_context, &impl->drm_decrypt_context) );
    ChkDR( Drm_Reader_Commit(impl->drm_app_context, NULL, NULL) );

   return &impl->base;

ErrorExit:

    CDX_LOGD("%s:%d, failed", __func__, __LINE__);
    if (impl->is_decrypt_init)
    {
        Drm_Reader_Close(&impl->drm_decrypt_context);
    }

    if (impl->is_env_open)
    {
        Env_Envelope_Close(&impl->env_file_context);
    }

    DRMTOOLS_DrmUninitializeWithOpaqueBuffer(&impl->drm_opaque_buffer, &impl->drm_app_context);
    SAFE_OEM_FREE(impl->drm_revocation_buffer);
    //CdxStreamClose(impl->innerstream);
    CdxFree(impl);

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
static int32_t __EnvPsrClose(CdxParserT *psr)
{
    struct EnvParserImplS *impl;
    impl = CdxContainerOf(psr, struct EnvParserImplS, base);

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

    pthread_mutex_destroy(&impl->mutex);
    free(impl);
    return 0;
}

static int envOnPrefetch(struct EnvParserImplS *impl, CdxPacketT *pkt)
{
    int32_t result;
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
            CDX_LOGW("Try to read sample failed! EOS!");
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

static cdx_int32 __EnvPsrPrefetch(CdxParserT *psr, CdxPacketT *pkt)
{
    cdx_int32 ret;
    struct EnvParserImplS *impl;
    impl = CdxContainerOf(psr, struct EnvParserImplS, base);
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
        ret = envOnPrefetch(impl, pkt);
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

static int32_t __EnvPsrRead(CdxParserT *psr, CdxPacketT *pkt)
{
    struct EnvParserImplS *impl;
    impl = CdxContainerOf(psr, struct EnvParserImplS, base);

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

static cdx_int32 __EnvPsrGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    struct EnvParserImplS *impl;
    int streamIndex, typeIndex;
    impl = CdxContainerOf(parser, struct EnvParserImplS, base);

    //set if the media file contain audio, video or subtitle
    mediaInfo->fileSize = CdxStreamSize(impl->stream);
    mediaInfo->programNum = 1;
    mediaInfo->programIndex = 0;
    mediaInfo->bSeekable = CdxStreamSeekAble(impl->stream);

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
            videoSI->nFrameRate = 25;
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

    //set total time
    mediaInfo->program[0].duration = impl->ctx->durationMs;

    return 0;
}

static cdx_int32 onForceStop(struct EnvParserImplS *impl)
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

static cdx_int32 onClrForceStop(struct EnvParserImplS *impl)
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

static int __EnvPsrControl(CdxParserT *psr, cdx_int32 cmd, void *param)
{
    (void)param;
    struct EnvParserImplS *impl;
    impl = CdxContainerOf(psr, struct EnvParserImplS, base);

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

static cdx_int32 __EnvPsrSeekTo(CdxParserT *psr, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    struct EnvParserImplS *impl = NULL;
    int ret = 0;
    int i;
    impl = CdxContainerOf(psr, struct EnvParserImplS, base);
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

static cdx_uint32 __EnvPsrAttribute(CdxParserT *psr)
{
    struct EnvParserImplS *impl = NULL;

    impl = CdxContainerOf(psr, struct EnvParserImplS, base);
    return impl->flags;
}

static cdx_int32 __EnvPsrGetStatus(CdxParserT *psr)
{
    struct EnvParserImplS *impl = NULL;

    impl = CdxContainerOf(psr, struct EnvParserImplS, base);

    return impl->mErrno;
}

static int __EnvPsrInit(CdxParserT *psr)
{
    struct EnvParserImplS *impl = NULL;
    int32_t result = 0;

    impl = CdxContainerOf(psr, struct EnvParserImplS, base);

    CDX_LOGI("env parser init begin...");

    pthread_mutex_init(&impl->mutex, NULL);
    //init env parser lib module
    impl->ctx = AsfPsrCoreInit();
    if (!impl->ctx)
    {
        CDX_LOGW("Initiate env file parser lib module failed!");
        goto failure;
    }

    result = AsfPsrCoreOpen(impl->ctx, impl->stream);

   if(result < 0)
   {
      CDX_LOGW("open env reader failed");
      goto failure;
   }

    impl->ctx->status = CDX_MEDIA_STATUS_IDLE;

    impl->mErrno = PSR_OK;
    impl->audioHeartIndex = 0;
    impl->videoHeartIndex = 0;
    impl->status = CDX_PSR_IDLE;
    CDX_LOGI("env parser init success...");

    AsfPsrCoreIoctrl(impl->ctx, CDX_MEDIA_STATUS_PLAY, 0);

    return 0;

failure:

    return -1;
}

struct CdxParserOpsS envPsrOps =
{
   .init = __EnvPsrInit,
    .control = __EnvPsrControl,
    .prefetch = __EnvPsrPrefetch,
    .read = __EnvPsrRead,
    .getMediaInfo = __EnvPsrGetMediaInfo,
    .seekTo = __EnvPsrSeekTo,
    .attribute = __EnvPsrAttribute,
    .getStatus = __EnvPsrGetStatus,
    .close = __EnvPsrClose
};

static cdx_uint32 __EnvPsrProbe(CdxStreamProbeDataT *probeData)
{
    cdx_uint32 signature = *(cdx_uint32 *)probeData->buf;
    CDX_LOGD("env probe, sig=0x%x", signature);
    return (signature == ENVELOPE_SIGNATURE) * 100;
}

static CdxParserT *__EnvPsrCreate(CdxStreamT *stream, cdx_uint32 flags)
{
    struct EnvParserImplS *impl = NULL;
    int32_t result = 0;

    impl = CdxMalloc(sizeof(*impl));

    memset(impl, 0x00, sizeof(*impl));
    impl->stream = __EnvStreamCreate(stream);
    impl->base.ops = &envPsrOps;
    impl->mErrno = PSR_INVALID;
    impl->flags = flags;

    if (impl->stream == NULL) {
        CdxFree(impl);
        return NULL;
    }

    return &impl->base;
}


#else

static cdx_uint32 __EnvPsrProbe(CdxStreamProbeDataT *probeData)
{
    return 0;
}

static CdxParserT *__EnvPsrCreate(CdxStreamT *stream, cdx_uint32 flags)
{
    return NULL;
}

#endif

struct CdxParserCreatorS envParserCtor =
{
    .probe = __EnvPsrProbe,
    .create = __EnvPsrCreate
};
