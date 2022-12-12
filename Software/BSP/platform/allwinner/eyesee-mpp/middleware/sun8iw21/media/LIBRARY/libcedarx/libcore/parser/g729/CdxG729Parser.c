/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxG729Parser.c
 * Description : G729Parser
 * Author  : chengkan<chengkan@allwinnertech.com>
 * Date    : 2016/04/13
 * Comment : 创建初始版本
 *
 */

#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxG729Parser.h>

static int G729Init(CdxParserT *g729_impl)
{
   cdx_int32  ret = 0, i = 0,skiplen = 0;
   cdx_int32  dataSize = 0;
   cdx_uint16 tembuf[SERIAL_SIZE]={0};
   cdx_bool   done = CDX_FALSE;
   cdx_int64  duration = 0;
   //CafDescription  theInputFormat;
   CdxStreamT *cdxStream = NULL;
   G729ParserImplS *impl= (G729ParserImplS*)g729_impl;

   impl->fileSize = CdxStreamSize(impl->stream);
   cdxStream = impl->stream;

   if(impl->fileSize<0)
   {
      goto OPENFAILURE;
   }
   if(0)//!GetCAFFdescFormat(cdxStream,&theInputFormat))
   {
      goto OPENFAILURE;
   }

   CdxStreamSeek(cdxStream,ret,SEEK_SET);

   impl->ulBitsSample = 8000;
   impl->ulChannels   = 1;
   impl->ulSampleRate = 8000;
   while( CdxStreamRead(cdxStream, tembuf, sizeof(short)*2) == sizeof(short)*2)
   {
      if(tembuf[1] != 0){
         CdxStreamRead(cdxStream, &tembuf[2], sizeof(short)*tembuf[1]);
         duration += (cdx_int64)L_FRAME * 1000ll/impl->ulSampleRate;
      }
   }
    impl->ulDuration   = duration;
   CDX_LOGV("ulDuration:%lld",impl->ulDuration);

   CdxStreamSeek(cdxStream,0,SEEK_SET);
   impl->mErrno = PSR_OK;
   pthread_cond_signal(&impl->cond);
   return 0;

OPENFAILURE:
    CDX_LOGE("G729OpenThread fail!!!");
    impl->mErrno = PSR_OPEN_FAIL;
    pthread_cond_signal(&impl->cond);
   return -1;
}

static cdx_int32 __G729ParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    struct G729ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct G729ParserImplS, base);
    (void)param;

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

static cdx_int32 __G729ParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
   struct G729ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct G729ParserImplS, base);

   pkt->type = CDX_MEDIA_AUDIO;
   pkt->length = PKTSIZE;
   pkt->pts = impl->seektime;//one frame pts;
    pkt->flags |= (FIRST_PART|LAST_PART);

   return CDX_SUCCESS;
}

static cdx_int32 __G729ParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 read_length;
    struct G729ParserImplS *impl = NULL;
    CdxStreamT *cdxStream = NULL;

   impl = CdxContainerOf(parser, struct G729ParserImplS, base);
   cdxStream = impl->stream;
   //READ ACTION

   if(pkt->length <= pkt->buflen)
   {
       read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->length);
   }
   else
   {
       read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->buflen);
       read_length += CdxStreamRead(cdxStream, pkt->ringBuf,   pkt->length - pkt->buflen);
   }

   if(read_length < 0)
   {
       CDX_LOGE("CdxStreamRead fail");
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
   //impl->nowsamples += impl->sampleperpkt;
   impl->framecount++;

   // TODO: fill pkt
   return CDX_SUCCESS;
}

static cdx_int32 __G729ParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    struct G729ParserImplS *impl = NULL;
    struct CdxProgramS *cdxProgram = NULL;

    impl = CdxContainerOf(parser, struct G729ParserImplS, base);
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
   cdxProgram->audio[0].eCodecFormat    = AUDIO_CODEC_FORMAT_G729;
   cdxProgram->audio[0].eSubCodecFormat = 0;
   cdxProgram->audio[0].nChannelNum     = impl->ulChannels;
   cdxProgram->audio[0].nBitsPerSample  = impl->ulBitsSample;
   cdxProgram->audio[0].nSampleRate     = impl->ulSampleRate;
   cdxProgram->audio[0].nCodecSpecificDataLen = impl->extradata_size;
   cdxProgram->audio[0].pCodecSpecificData = (char *)impl->extradata;
   return CDX_SUCCESS;
}

static cdx_int32 __G729ParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
      struct G729ParserImplS *impl = NULL;
    cdx_int32 samples = 0,i = 0,frames = 0;
   cdx_int64 offset = 0;
   cdx_uint16 tembuf[SERIAL_SIZE]={0};
   impl = CdxContainerOf(parser, struct G729ParserImplS, base);
   samples=(timeUs*impl->ulSampleRate)/1000000ll;
   frames = samples/L_FRAME;
   //offset = (cdx_int64)frames*sizeof(short)*SERIAL_SIZE;
   if(CdxStreamSeek(impl->stream,0,SEEK_SET))
   {
      CDX_LOGE("CdxStreamSeek to 0 fail");
      return CDX_FAILURE;
   }
   while(frames)
   {
      if(CdxStreamRead(impl->stream, tembuf, sizeof(short)*2) != sizeof(short)*2)
      {
         CDX_LOGE("CdxStreamSeek read 2 bytes fail");
         return CDX_FAILURE;
      }
      if(tembuf[1] != 0)
      {
         if(CdxStreamSeek(impl->stream,sizeof(short)*tembuf[1],SEEK_CUR))
         {
            CDX_LOGE("CdxStreamSeek to frames fail");
            return CDX_FAILURE;
         }
         frames--;
      }

   }
   impl->seektime = timeUs;
   CDX_LOGV("Seek to time:%lld,to position:%lld,fliesize:%lld",
              timeUs,impl->file_offset,impl->fileSize);
   return CDX_SUCCESS;
}

static cdx_uint32 __G729ParserAttribute(CdxParserT *parser)
{
    struct G729ParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct G729ParserImplS, base);
    return 0;
}

static cdx_int32 __G729ParserGetStatus(CdxParserT *parser)
{
    struct G729ParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct G729ParserImplS, base);
#if 0
    if (CdxStreamEos(impl->stream))
    {
        return PSR_EOS;
    }
#endif
    return impl->mErrno;
}

static cdx_int32 __G729ParserClose(CdxParserT *parser)
{
    struct G729ParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct G729ParserImplS, base);
    CdxStreamClose(impl->stream);

    if(impl->extradata)
   {
      CdxFree(impl->extradata);
   }
   pthread_cond_destroy(&impl->cond);
   CdxFree(impl);
    return CDX_SUCCESS;
}

static struct CdxParserOpsS g729ParserOps =
{
    .control = __G729ParserControl,
    .prefetch = __G729ParserPrefetch,
    .read = __G729ParserRead,
    .getMediaInfo = __G729ParserGetMediaInfo,
    .seekTo = __G729ParserSeekTo,
    .attribute = __G729ParserAttribute,
    .getStatus = __G729ParserGetStatus,
    .close = __G729ParserClose,
    .init = G729Init
};

static cdx_uint32 __G729ParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_CHECK(probeData);
   short *buf = NULL;
   buf = (short*)probeData->buf;
    if(probeData->len < 4)
    {
        CDX_LOGE("Probe G729_header data is not enough.");
        return 0;
    }

    if(buf[0] != SYNC_WORD)
    {
        CDX_LOGE("g729 SYNC_WORD probe failed.");
        return 0;
    }
   else
   {
      if(0){//buf[1] != SIZE_WORD){
         CDX_LOGE("g729 SIZE_WORD probe failed.");
         return 0;
      }
   }
    return 100;
}

static CdxParserT *__G729ParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    //cdx_int32 ret = 0;
    struct G729ParserImplS *impl;
    impl = CdxMalloc(sizeof(*impl));
    if (impl == NULL) {
        CDX_LOGE("G729ParserOpen Failed");
      CdxStreamClose(stream);
        return NULL;
    }
    memset(impl, 0x00, sizeof(*impl));
    (void)flags;

    impl->stream = stream;
    impl->base.ops = &g729ParserOps;
   pthread_cond_init(&impl->cond, NULL);
    //ret = pthread_create(&impl->openTid, NULL, CafOpenThread, (void*)impl);
    //CDX_FORCE_CHECK(!ret);
    impl->mErrno = PSR_INVALID;

    return &impl->base;
}

struct CdxParserCreatorS g729ParserCtor =
{
    .probe = __G729ParserProbe,
    .create = __G729ParserOpen
};

