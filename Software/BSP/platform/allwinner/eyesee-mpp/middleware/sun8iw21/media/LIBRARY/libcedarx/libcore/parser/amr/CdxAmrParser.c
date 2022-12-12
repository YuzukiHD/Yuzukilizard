/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxAmrParser.c
* Description : Amr Parser
* History :
*
*/
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxAmrParser.h>

#define AMR_UNEXPECTED_ERROR (-1)
#define AMR_LACK_OF_DATA (0)
#define AMR_ERROR_NONE (1)

cdx_int32 AmrGetFrameSizeByOffset(AmrParserImplS *impl, cdx_int64 offset,
                                  cdx_int32 isWide, cdx_int32 *frameSize)
{
    cdx_int32 ret = 0;
    unsigned char header[9];
    unsigned char *d=header;

    if( CdxStreamSeek(impl->stream,offset, SEEK_SET) < 0 )
    {
        CDX_LOGE("Seek fail...");
        return AMR_UNEXPECTED_ERROR;
    }

    ret = CdxStreamRead(impl->stream,(void*)d,1);
    if (ret < 1) {
        CDX_LOGD("Sync head data no enough! offset : %lld", offset);
        return AMR_LACK_OF_DATA;
    }

    if( CdxStreamSeek(impl->stream,impl->file_offset, SEEK_SET) < 0 )
    {
        CDX_LOGE("Seek fail...");
        return AMR_UNEXPECTED_ERROR;
    }

    cdx_uint32 FT = (d[0] >> 3) & 0x0f;
    if (0)//FT > 15 || (isWide && FT > 10 && FT < 14) || (!isWide && FT > 8 && FT < 16))
    {
        CDX_LOGE("illegal AMR frame type %d,offset:%lld", FT, offset);
        *frameSize = 0;
        return AMR_UNEXPECTED_ERROR;
    }
    *frameSize = isWide ? block_size_WB[FT] : (block_size_NB[FT]+1);
    return AMR_ERROR_NONE;

}

static int AmrInit(CdxParserT* parameter)
{
    cdx_int32 ret = 0;
    cdx_int32 framesize,NumFrames;
    cdx_int64 offset=0;
    cdx_int32 rc    =AMR_ERROR_NONE;
    unsigned char readBuf[9];
    unsigned char *d=readBuf;
    struct AmrParserImplS *impl          = NULL;

    impl = (AmrParserImplS *)parameter;
    impl->fileSize = CdxStreamSize(impl->stream);
    ret = CdxStreamRead(impl->stream, (void *)d, 6);
    if(ret!=6){
        CDX_LOGD("First 6 byte no enough!");
        goto OPENFAILURE;
    }
    impl->file_offset+=6;
    offset+=6;
    if(memcmp(d,AMR_header,6))
    {
        ret = CdxStreamRead(impl->stream, (void *)(d+6), 3);
        if(ret!=3){
            CDX_LOGD("Extern 3 byte no enough!");
            goto OPENFAILURE;
        }
        impl->file_offset+=3;
        offset+=3;
        if(memcmp(d,AMRWB_header,9)){
            CDX_LOGE("Invaild header!");
            goto OPENFAILURE;
        }
        impl->is_wide = 1;
        impl->sample_rate = 16000;
        impl->framepcms = 320;
    }
    else
    {
        impl->is_wide = 0;
        impl->sample_rate = 8000;
        impl->framepcms = 160;
    }
    impl->chans=1;
    NumFrames=impl->seek_table_len=0;
    while (offset < impl->fileSize) {
        rc = AmrGetFrameSizeByOffset(impl, offset, impl->is_wide, &framesize);
        if (rc == AMR_UNEXPECTED_ERROR)
            goto OPENFAILURE;
        else if(rc == AMR_LACK_OF_DATA)
            break;

        if ((NumFrames % 50 == 0) && (NumFrames / 50 < SEEK_TABLE_LEN)) {
            impl->seek_table[impl->seek_table_len] = offset - (impl->is_wide ? 9: 6);
            impl->seek_table_len ++;
        }
        offset += framesize;
        impl->ulDuration += 20;  // Each frame is 20ms
        NumFrames++;
    }
    impl->head_len = impl->is_wide ? 9:6;
    impl->mErrno = PSR_OK;
    pthread_cond_signal(&impl->cond);
    return 0;
OPENFAILURE:
    CDX_LOGE("AmrOpenThread fail!!!");
    impl->mErrno = PSR_OPEN_FAIL;
    pthread_cond_signal(&impl->cond);
    return -1;
}

static cdx_int32 __AmrParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    (void)param;
    struct AmrParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct AmrParserImplS, base);
    switch (cmd)
    {
    case CDX_PSR_CMD_DISABLE_AUDIO:
    case CDX_PSR_CMD_DISABLE_VIDEO:
    case CDX_PSR_CMD_SWITCH_AUDIO:
        break;
    case CDX_PSR_CMD_SET_FORCESTOP:
        CdxStreamForceStop(impl->stream);
      break;
    case CDX_PSR_CMD_CLR_FORCESTOP:
        CdxStreamClrForceStop(impl->stream);
        break;
    default :
        CDX_LOGW("not implement...(%d)", cmd);
        break;
    }
    impl->flags = cmd;
    return CDX_SUCCESS;
}

static cdx_int32 __AmrParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    struct AmrParserImplS *impl = NULL;
    cdx_int32 rc = AMR_ERROR_NONE;
    impl = CdxContainerOf(parser, struct AmrParserImplS, base);
    pkt->type = CDX_MEDIA_AUDIO;
    pkt->length = 128;//see table block_size_WB, max frame size no exceed than 128

    rc = AmrGetFrameSizeByOffset(impl, impl->file_offset, impl->is_wide, &pkt->length);
    if(rc == AMR_LACK_OF_DATA){
        CDX_LOGD("AMR_LACK_OF_DATA Prefetch while lack of data...");
    }
    else if(rc == AMR_UNEXPECTED_ERROR){
        return CDX_FAILURE;
    }

    pkt->pts = (cdx_int64)impl->framecounts*20000;//one frame 20ms;
    pkt->flags |= (FIRST_PART|LAST_PART);
    CDX_LOGV("pkt->length:%d,pkt->pts:%lld",pkt->length,pkt->pts);
    return CDX_SUCCESS;
}

static cdx_int32 __AmrParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 read_length;
    struct AmrParserImplS *impl = NULL;
    CdxStreamT *cdxStream = NULL;
    
    impl = CdxContainerOf(parser, struct AmrParserImplS, base);
    cdxStream = impl->stream;
    //READ ACTION
    if(pkt->length <= pkt->buflen)
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->length);
    }
    else
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->buflen);
        read_length += CdxStreamRead(cdxStream, pkt->ringBuf,    pkt->length - pkt->buflen);
    }

    if(read_length < 0)
    {
        CDX_LOGD("CdxStreamRead fail");
        impl->mErrno = PSR_IO_ERR;
        return CDX_FAILURE;
    }
    else if(read_length == 0)
    {
       CDX_LOGD("CdxStream EOS");
       impl->mErrno = PSR_EOS;
       return CDX_FAILURE;
    }
    pkt->length = read_length;
    impl->file_offset += read_length;
    impl->framecounts++;
    // TODO: fill pkt
    return CDX_SUCCESS;
}

static cdx_int32 __AmrParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    struct AmrParserImplS *impl = NULL;
    struct CdxProgramS *cdxProgram = NULL;

    impl = CdxContainerOf(parser, struct AmrParserImplS, base);
    memset(mediaInfo, 0x00, sizeof(*mediaInfo));

    if(impl->mErrno != PSR_OK)
    {
        CDX_LOGE("audio parse status no PSR_OK");
        return CDX_FAILURE;
    }

    mediaInfo->programNum = 1;
    mediaInfo->programIndex = 0;
    cdxProgram = &mediaInfo->program[0];
    memset(cdxProgram, 0, sizeof(struct CdxProgramS));
    cdxProgram->id = 0;
    cdxProgram->audioNum = 1;
    cdxProgram->videoNum = 0;//
    cdxProgram->subtitleNum = 0;
    cdxProgram->audioIndex = 0;
    cdxProgram->videoIndex = 0;
    cdxProgram->subtitleIndex = 0;

    mediaInfo->bSeekable = CdxStreamSeekAble(impl->stream);
    mediaInfo->fileSize = impl->fileSize;
    
    cdxProgram->duration = impl->ulDuration;
    cdxProgram->audio[0].eCodecFormat    = AUDIO_CODEC_FORMAT_AMR;
    cdxProgram->audio[0].eSubCodecFormat = impl->is_wide ? CODEC_ID_AMR_WB : CODEC_ID_AMR_NB;
    cdxProgram->audio[0].nChannelNum     = impl->chans;
    cdxProgram->audio[0].nBitsPerSample  = 16;
    cdxProgram->audio[0].nSampleRate     = impl->sample_rate;
    return CDX_SUCCESS;
}

static cdx_int32 __AmrParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    struct AmrParserImplS *impl = NULL;
    cdx_int32 frames = 0, framesize=0;
    cdx_int32 index,i;
    cdx_int64 offset = 0;
    cdx_int32 rc = AMR_ERROR_NONE;

    impl = CdxContainerOf(parser, struct AmrParserImplS, base);
    frames = (timeUs/20000ll);
    index = frames / 50;
    if (index >= impl->seek_table_len) {
        index = impl->seek_table_len - 1;
    }

    offset = impl->seek_table[index] + (impl->is_wide ? 9 : 6);

    CDX_LOGD("offset : %lld,  fileSize : %lld", offset, impl->fileSize);
    for (i = 0; i< frames - index * 50; i++) {
        rc = AmrGetFrameSizeByOffset(impl, offset,impl->is_wide, &framesize);
        if (rc == AMR_LACK_OF_DATA) {
            CDX_LOGD("AMR_LACK_OF_DATA seek frame but lack of data!!");
            break;
        }
        else if(rc == AMR_UNEXPECTED_ERROR) {
            CDX_LOGE("AMR_UNEXPECTED_ERROR when seeking, fail...");
            return CDX_FAILURE;
        }

        offset += framesize;
    }
    if(offset!= impl->file_offset)
    {
        if(CdxStreamSeek(impl->stream,offset,SEEK_SET))
        {
            CDX_LOGE("CdxStreamSeek fail");
            return CDX_FAILURE;
        }
    }
    impl->framecounts = frames;
    impl->file_offset = offset;
    return CDX_SUCCESS;
}

static cdx_uint32 __AmrParserAttribute(CdxParserT *parser)
{
    struct AmrParserImplS *impl = NULL;
    
    impl = CdxContainerOf(parser, struct AmrParserImplS, base);
    return 0;
}
/*
static cdx_int32 __AmrParserForceStop(CdxParserT *parser)
{
    cdx_int32 ret = CDX_SUCCESS;
    struct AmrParserImplS *impl = NULL;
    
    impl = CdxContainerOf(parser, struct AmrParserImplS, base);
    
    ret = CdxStreamForceStop(impl->stream);
    if (ret != CDX_SUCCESS)
    {
        CDX_LOGE("stop stream failure... err(%d)", ret);
    }
    return ret;
}
*/
static cdx_int32 __AmrParserGetStatus(CdxParserT *parser)
{
    struct AmrParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct AmrParserImplS, base);
#if 0
    if (CdxStreamEos(impl->stream))
    {
        return PSR_EOS;
    }
#endif
    return impl->mErrno;
}

static cdx_int32 __AmrParserClose(CdxParserT *parser)
{
   struct AmrParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct AmrParserImplS, base);
    //pthread_join(impl->openTid, NULL);
    CdxStreamClose(impl->stream);
    pthread_cond_destroy(&impl->cond);
    CdxFree(impl);
    return CDX_SUCCESS;
}

static struct CdxParserOpsS amrParserOps =
{
    .control = __AmrParserControl,
    .prefetch = __AmrParserPrefetch,
    .read = __AmrParserRead,
    .getMediaInfo = __AmrParserGetMediaInfo,
    .seekTo = __AmrParserSeekTo,
    .attribute = __AmrParserAttribute,
    .getStatus = __AmrParserGetStatus,
    .close = __AmrParserClose,
    .init = AmrInit
};

static cdx_int32 AmrProbe(CdxStreamProbeDataT *p)
{
    cdx_char *d;
    d = p->buf;
    if(memcmp(d,AMR_header,6))
    {
        if(p->len<9)
        {
            CDX_LOGD("AMRWB_header data is not enough.");
            return CDX_FALSE;
        }
        if(memcmp(d,AMRWB_header,9))
            return CDX_FALSE;
        return CDX_TRUE;
    }
    else
        return CDX_TRUE;
}

static cdx_uint32 __AmrParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_CHECK(probeData);
    if(probeData->len < 6)
    {
        CDX_LOGD("Probe AMR_header data is not enough.");
        return 0;
    }

    if(!AmrProbe(probeData))
    {
        CDX_LOGD("amr probe failed.");
        return 0;
    }
    return 100;
}

static CdxParserT *__AmrParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    (void)flags;
    //cdx_int32 ret = 0;
    struct AmrParserImplS *impl;
    impl = CdxMalloc(sizeof(*impl));

    memset(impl, 0x00, sizeof(*impl));
    impl->stream = stream;
    impl->base.ops = &amrParserOps;
    pthread_cond_init(&impl->cond, NULL);
    //ret = pthread_create(&impl->openTid, NULL, AmrOpenThread, (void*)impl);
    //CDX_FORCE_CHECK(!ret);
    impl->mErrno = PSR_INVALID;
    
    return &impl->base;
}

struct CdxParserCreatorS amrParserCtor =
{
    .probe = __AmrParserProbe,
    .create = __AmrParserOpen
};
