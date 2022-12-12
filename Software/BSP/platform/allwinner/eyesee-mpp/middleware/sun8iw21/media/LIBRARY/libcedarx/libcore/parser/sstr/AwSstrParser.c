/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : AwSstrParser.c
* Description :
* History :
*   Author  : gvc-al3 <gvc-al3@allwinnertech.com>
*   Date    :
*   Comment : smooth streaming implementation.
*/

//Smooth Stream Transfer Protocol
//#define CONFIG_LOG_LEVEL 4
#define LOG_TAG "sstrParser"
#include <AwSstrUtils.h>
#include <AwSstrParser.h>
#include <stdlib.h>
#include <string.h>

#define ISMC_BUFF_SIZE (1024*1024) //* for store manifest

#define kUseSecureInputBuffers 256 //copy from demuxComponent.cpp

//chunk qualitylevel sms will use list...

enum AwSstrStatusE
{
    AW_SSTR_INITIALIZED,
    AW_SSTR_IDLE,
    AW_SSTR_PREFETCHING,
    AW_SSTR_PREFETCHED,
    AW_SSTR_READING,
    AW_SSTR_SEEKING,
};

#define STRA_SIZE 334
#define SMOO_SIZE (STRA_SIZE * 3 + 24) /* 1026 */

static int BuildSmooBox(CdxStreamT *stream, cdx_uint8 *smoo_box) //* todo
{
    CDX_UNUSE(stream);
    CDX_UNUSE(smoo_box);
    return 0;
}

static ChunkT *BuildInitChunk(CdxStreamT *stream)
{
    ChunkT *ret = malloc(sizeof(ChunkT));

    if(!ret)
    {
        CDX_LOGE("malloc failed.");
        return NULL;
    }

    ret->size = SMOO_SIZE;
    ret->data = malloc(SMOO_SIZE);
    if(!ret->data)
    {
        CDX_LOGE("malloc failed.");
        free(ret);
        return NULL;
    }
    if(BuildSmooBox(stream, ret->data) == 0)
        return ret;
    else
    {
        free(ret->data);
        free(ret);
        return NULL;
    }
}

static AwSstrParserImplT *SstrInit(void)
{
    AwSstrParserImplT *p;

    p = (AwSstrParserImplT *)malloc(sizeof(AwSstrParserImplT));
    if(!p)
    {
        CDX_LOGE("malloc failed.");
        return NULL;
    }
    memset(p, 0x00, sizeof(AwSstrParserImplT));

    p->ismc = (AwIsmcT *)malloc(sizeof(AwIsmcT));
    if(!p->ismc)
    {
        CDX_LOGE("malloc ismc failed.");
        free(p);
        return NULL;
    }
    memset(p->ismc, 0x00, sizeof(AwIsmcT));

    p->mErrno = PSR_INVALID;
    return p;
}

static cdx_int32 InitMediaStreams(AwSstrParserImplT *impl)
{
    cdx_int32 i;

    if(!impl)
    {
        CDX_LOGE("param is null.");
        return -1;
    }

    for(i=0; i<3; i++)
    {
        impl->mediaStream[i].datasource = malloc(sizeof(CdxDataSourceT));
        if(!impl->mediaStream[i].datasource)
        {
            CDX_LOGE("malloc failed.");
            return -1;
        }
        memset(impl->mediaStream[i].datasource, 0x00, sizeof(CdxDataSourceT));
        impl->mediaStream[i].flag = 0;
        impl->mediaStream[i].eos = 0;
        impl->mediaStream[i].baseTime = 0;
        impl->mediaStream[i].parser = NULL;
    }
    return 0;
}
static void FreeMediaStreams(AwSstrParserImplT *impl)
{
    cdx_int32 i;

    if(impl)
    {
        for(i=0; i< 3; i++)
        {
            if(impl->mediaStream[i].datasource)
            {
                free(impl->mediaStream[i].datasource);
                impl->mediaStream[i].datasource = NULL;
            }
            if(impl->mediaStream[i].parser)
            {
                CdxParserClose(impl->mediaStream[i].parser);
                impl->mediaStream[i].parser = NULL;
            }
        }
    }

    return;
}

static cdx_int32 __AwSstrParserClose(CdxParserT *parser)
{
    AwSstrParserImplT *impl;

    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    if(impl)
    {
        impl->exitFlag = 1;
        //pthread_join(impl->nOpenPid, NULL);
        if(impl->stream)
        {
            CdxStreamClose(impl->stream);
            impl->stream = NULL;
        }

        //* free sms streams
        SmsStreamT *sms;
        int i = 0;
        for(i = 0; i < SmsArrayCount(impl->ismc->smsStreams); i++)
        {
            sms = SmsArrayItemAtIndex(impl->ismc->smsStreams, i);
            if(sms)
                SmsFree(sms);
        }

        //* free init chunks
        ChunkT *chunk;
        for(i = 0; i < SmsArrayCount(impl->ismc->initChunks); i++)
        {
            chunk = SmsArrayItemAtIndex(impl->ismc->initChunks, i);
            ChunkFree(chunk);
        }

        //* free bws queue             need? todo.
        SmsQueueFree(impl->ismc->bws);

        //* destroy
        SmsArrayDestroy(impl->ismc->smsStreams);
        SmsArrayDestroy(impl->ismc->selectedSt);
        SmsArrayDestroy(impl->ismc->initChunks);
        SmsArrayDestroy(impl->ismc->download.chunks);

        if(impl->ismc)
        {
            free(impl->ismc);
            impl->ismc = NULL;
        }
        if(impl->pIsmcBuffer)
        {
            free(impl->pIsmcBuffer);
            impl->pIsmcBuffer = NULL;
        }
        if(impl->ismcUrl)
        {
            free(impl->ismcUrl);
            impl->ismcUrl = NULL;
        }
        if(impl->codecSpecDataA)
        {
            free(impl->codecSpecDataA);
            impl->codecSpecDataA = NULL;
        }
        if(impl->codecSpecDataV)
        {
            free(impl->codecSpecDataV);
            impl->codecSpecDataV = NULL;
        }
#if SAVE_VIDEO_STREAM
    if(impl->fpVideoStream)
    {
        fclose(impl->fpVideoStream);
    }
#endif
        FreeMediaStreams(impl);

        pthread_mutex_destroy(&impl->statusMutex);
        pthread_cond_destroy(&impl->cond);
    }
    free(impl);
    return CDX_SUCCESS;
}

//*  template: QualityLevels({bitrate})/Fragments(video={start time})
static char *SmsConstructUrl(const char *template, const char *baseUrl, const cdx_uint64 bandWidth,
                           const cdx_uint64 startTime)
{
    char *frag, *end, *qual;
    char *urlTemplate = strdup(template);
    char *saveptr = NULL;

    qual = CdxStrToKr(urlTemplate, "{", &saveptr);
    CdxStrToKr(NULL, "}", &saveptr);
    frag = CdxStrToKr(NULL, "{", &saveptr); //* frag=bitrate
    CdxStrToKr(NULL, "}", &saveptr);
    end = CdxStrToKr(NULL, "", &saveptr);
    char *url = NULL;

    if(asprintf(&url, "%s/%s%llu%s%llu%s", baseUrl, qual,
                bandWidth, frag, startTime, end) < 0)
    {
        free(urlTemplate);
        return NULL;
    }

    free(urlTemplate);
    return url;
}

cdx_int32 SmsOpenSegment(AwSstrParserImplT *impl, SmsStreamT *sms)
{
    cdx_int32 index;
    cdx_int32 ret;
    cdx_int64 startTime;

    index = EsCatToIndex(sms->type);
    startTime = impl->ismc->download.lead[index];

    //* sms->downloadQlvl should change by bandwidth... todo
    QualityLevelT *qlevel = GetQlevel(sms, sms->downloadQlvl);
    if(!qlevel)
    {
        CDX_LOGE("can not get quality level id=%u", sms->downloadQlvl);
        return -1;
    }

    ChunkT *chunk;
    chunk = ChunkGet(sms, startTime);
    if(!chunk)
    {
        CDX_LOGE("get chunk failed.");
        return -1;
    }

    chunk->type = sms->type;
    char *url = SmsConstructUrl(sms->urlTemplate, impl->ismc->baseUrl,
                                qlevel->Bitrate, chunk->startTime);
    if(!url)
    {
        CDX_LOGE("construct url failed.");
        return -1;
    }
    //CDX_LOGD("xxx url: %s", url);

    impl->mediaStream[EsCatToIndex(impl->prefetchType)].datasource->uri = url;

    CdxStreamT *stream;
    ret = CdxStreamOpen(impl->mediaStream[EsCatToIndex(impl->prefetchType)].datasource,
                        &impl->statusMutex, &impl->exitFlag, &stream, NULL);
    if(ret < 0)
    {
        CDX_LOGE("open stream failed. uri:%s", url);
        return -1;
    }

    pthread_mutex_lock(&impl->statusMutex);
    ret = CdxParserControl(impl->mediaStream[EsCatToIndex(impl->prefetchType)].parser,
                           CDX_PSR_CMD_REPLACE_STREAM, (void *)stream);
    pthread_mutex_unlock(&impl->statusMutex);

    if(ret < 0)
    {
        CDX_LOGE("sms replace stream failed.");
        return -1;
    }

    return 0;
}

cdx_int32 SmsOpenFirstSegment(AwSstrParserImplT *impl, cdx_int32 index)
{
    SmsStreamT *sms;
    ChunkT * chunk;
    cdx_int64 startTime;
    cdx_int32 ret;

    sms = SmsGetStreamByCat(impl->ismc->selectedSt, IndexToEsCat(index));
    if(!sms)
    {
        CDX_LOGE("no found stream. index=%d", index);
        return -1;
    }

    chunk = SmsArrayItemAtIndex(sms->chunks, 0);
    impl->ismc->download.lead[index] = chunk->startTime + impl->ismc->timeScale/1000;
    //* 下载完片时 需要更新download.lead, 作为下一片的start time
    startTime = impl->ismc->download.lead[index];

    QualityLevelT *qlevel = GetQlevel(sms, sms->downloadQlvl);
    if(!qlevel)
    {
        CDX_LOGE("can not get quality level id=%u", sms->downloadQlvl);
        return -1;
    }

    //* todo   ChunkGet again...

    chunk->type = sms->type;

    //* construct url with template, baseUrl, bitrate and starttime.
    char *url = SmsConstructUrl(sms->urlTemplate, impl->ismc->baseUrl, qlevel->Bitrate,
                                chunk->startTime);
    if(!url)
    {
        CDX_LOGE("construct url failed.");
        return -1;
    }
    CDX_LOGD("xxx url: %s", url);

    impl->mediaStream[index].flag |= SEGMENT_SMS;
    if (impl->ismc->protectSystem == SSTR_PROTECT_PLAYREADY)
        impl->mediaStream[index].flag |= SEGMENT_PLAYREADY;
    impl->mediaStream[index].datasource->uri = url;
    ret = CdxParserPrepare(impl->mediaStream[index].datasource,
                           impl->mediaStream[index].flag,
                           &impl->statusMutex,
                           &impl->exitFlag,
                           &impl->mediaStream[index].parser,
                           &impl->mediaStream[index].stream,
                           NULL,
                           NULL);
    if(ret < 0)
    {
        CDX_LOGE("prepare failed.");
        return -1;
    }

    return 0;
}

#if 0
static int SmsCallbackProcess(void* pUserData, int eMessageId, void* param)
{
    AwSstrParserImplT *impl = (CdxHlsParser*)pUserData;
    int ret;

    switch(eMessageId)
    {
        case PARSER_NOTIFY_VIDEO_STREAM_CHANGE:
        //case PARSER_NOTIFY_AUDIO_STREAM_CHANGE:
        {
            CDX_LOGD("PARSER_NOTIFY_VIDEO_STREAM_CHANGE");
            ret = SetMediaInfo(impl);///SmsSetMediaInfo getmediainfo
            if(ret < 0)
            {
                CDX_LOGE("SetMediaInfo fail, ret(%d)", ret);
            }
            break;
        }
        default:
            logw("ignore callback message, eMessageId = 0x%x.", eMessageId);
            return -1;
    }
    impl->callback(impl->pUserData, eMessageId, param);
    return 0;
}
#endif

//* h264, construct codec specific data. 000000016742C00D96640988DFF8C800C458800001F480005DC0078A15
//* 500000000168CE32C8 =>
//* [0]: 0x01 [1][2][3]: 0x00 00 00
//* [4]: 0x03 [5]: 0x01 [6]: spsLen high 8 bits [7]:spsLen low 8 bits [8, 7+spsLen]:sps data
//* [8+spsLen]:0x01 [9+spsLen]: ppsLen high 8 bits [10+spsLen]: ppsLen low 8 bits [11+spsLen, spsLe
//* n+ppsLen+10]: pps data
static cdx_int32 ConstructCodecSpecificDataForH264(AwSstrParserImplT *impl)
{
    //* 000000016742C00D96640988DFF8C800C458800001F480005DC0078A15500000000168CE32C8
    cdx_int32 nAnalyseSize = impl->codecSpecDataVLen;
    cdx_uint8 tmpBuf0[1024], tmpBuf1[1024];
    cdx_int32 index0 = 0, index1 = 0, len0 = 0, len1 = 0;
    memcpy(tmpBuf0, impl->codecSpecDataV, nAnalyseSize);
    int i, startCodeNum0 = 0, startCodeNum1 = 0;

    for(i=0; (i+4)<=nAnalyseSize; i++)
    {
        if(tmpBuf0[i] == 0x00 && tmpBuf0[i+1] == 0x00 && tmpBuf0[i+2] == 0x01)
        {
            startCodeNum0 = 3;
            index0 = i;
            break;
        }
        else if(tmpBuf0[i] == 0x00 && tmpBuf0[i+1] == 0x00 && tmpBuf0[i+2] == 0x00 &&
                tmpBuf0[i+3] == 0x01)
        {
            startCodeNum0 = 4;
            index0 = i;
            break;
        }

    }
    if(startCodeNum0 == 0)
    {
        CDX_LOGE("not found startCode0");
        return -1;
    }
    for(i=startCodeNum0; (i+4)<=nAnalyseSize; i++)
    {
        if(tmpBuf0[i] == 0x00 && tmpBuf0[i+1] == 0x00 && tmpBuf0[i+2] == 0x01)
        {
            startCodeNum1 = 3;
            index1 = i;
            break;
        }
        else if(tmpBuf0[i] == 0x00 && tmpBuf0[i+1] == 0x00 && tmpBuf0[i+2] == 0x00 &&
                tmpBuf0[i+3] == 0x01)
        {
            startCodeNum1 = 4;
            index1 = i;
            break;
        }
    }
    //CDX_LOGD("index0=%d, index1=%d", index0, index1);

    if(startCodeNum1 == 0)
    {
        CDX_LOGE("not found startCode1");
        return -1;
    }

    len0 = index1 - startCodeNum0;
    len1 = nAnalyseSize - startCodeNum0 - len0 - startCodeNum1;

    tmpBuf1[0] = 0x01;
    tmpBuf1[1] = 0x00;
    tmpBuf1[2] = 0x00;
    tmpBuf1[3] = 0x00;
    tmpBuf1[4] = 0x03;
    tmpBuf1[5] = 0x01;
    tmpBuf1[6] = (len0 >> 8) & 0xff;
    tmpBuf1[7] = len0 & 0xff;
    memcpy(tmpBuf1+8, tmpBuf0+startCodeNum0, len0);
    tmpBuf1[8+len0] = 0x01;
    tmpBuf1[9+len0] = (len1 >> 8) & 0xff;
    tmpBuf1[10+len0] = len1 & 0xff;
    memcpy(tmpBuf1+len0+11, tmpBuf0+index1+startCodeNum1, len1);

    impl->nExtraDataLen = 11 + len0 + len1;
    memcpy(impl->pExtraData, tmpBuf1, impl->nExtraDataLen);

    return 0;
}

static cdx_int32 SstrSetMediaInfo(AwSstrParserImplT *impl)
{
    CdxMediaInfoT *pMediaInfo;
    //cdx_int32 i;
    cdx_uint32 fourCC;
    cdx_int32 ret;

    pMediaInfo = &impl->mediaInfo;

    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pMediaInfo->bSeekable = 1;

    if(impl->ismc->hasVideo)
    {
        pMediaInfo->program[0].videoNum = impl->ismc->hasVideo;
        pMediaInfo->program[0].videoIndex = 0;

#if BOARD_PLAYREADY_USE_SECUREOS
        if (impl->ismc->protectSystem == SSTR_PROTECT_PLAYREADY) {
            CDX_LOGV("sstr use secure input buffers!");
            pMediaInfo->program[0].flags |= kUseSecureInputBuffers;
        }
#endif

        SmsStreamT *sms = SmsGetStreamByCat(impl->ismc->smsStreams, SSTR_VIDEO);
        QualityLevelT *qlevel = GetQlevel(sms, sms->downloadQlvl);

        pMediaInfo->bitrate = qlevel->Bitrate;

        fourCC = qlevel->FourCC ? qlevel->FourCC : sms->defaultFourCC;
        switch(fourCC)
        {
            case SSTR_FOURCC('W','V','C','1'):
            {
                pMediaInfo->program[0].video[0].eCodecFormat = VIDEO_CODEC_FORMAT_WMV3;
                CDX_LOGD("video codec:[%x] VIDEO_CODEC_FORMAT_WMV3", VIDEO_CODEC_FORMAT_WMV3);
                break;
            }
            case SSTR_FOURCC('A','V','C','1'):
                CDX_LOGD("AVC1");
            case SSTR_FOURCC('H','2','6','4'):
            {
                pMediaInfo->program[0].video[0].eCodecFormat = VIDEO_CODEC_FORMAT_H264;
                //pMediaInfo->program[0].video[0].bIsFramePackage = 1;
                CDX_LOGD("video codec:[%x] VIDEO_CODEC_FORMAT_H264", VIDEO_CODEC_FORMAT_H264);
                break;
            }
            default:
            {
                CDX_LOGW("sms not support fourcc:%d", fourCC);
                return -1;
            }
        }
        pMediaInfo->program[0].video[0].nHeight = qlevel->MaxHeight;
        pMediaInfo->program[0].video[0].nWidth  = qlevel->MaxWidth;
        //impl->nExtraDataLen = strlen(qlevel->CodecPrivateData) / 2;

        if(impl->codecSpecDataV)
        {
            free(impl->codecSpecDataV);
            impl->codecSpecDataV = NULL;
        }
        impl->codecSpecDataV = DecodeStringHexToBinary(qlevel->CodecPrivateData);
        impl->codecSpecDataVLen = strlen(qlevel->CodecPrivateData) / 2;

        //* construct codec specific data for h264
        if(pMediaInfo->program[0].video[0].eCodecFormat == VIDEO_CODEC_FORMAT_H264)
        {
            ret = ConstructCodecSpecificDataForH264(impl);
            if(ret < 0)
            {
                CDX_LOGE("ConstructCodecSpecificDataForH264 failed.");
                return -1;
            }
            CDX_LOGD("nExtraDataLen:%d", impl->nExtraDataLen);
            pMediaInfo->program[0].video[0].nCodecSpecificDataLen = impl->nExtraDataLen;
            pMediaInfo->program[0].video[0].pCodecSpecificData = (char *)impl->pExtraData;
        }
        else
        {
            pMediaInfo->program[0].video[0].nCodecSpecificDataLen = impl->codecSpecDataVLen;
            pMediaInfo->program[0].video[0].pCodecSpecificData = (char *)impl->codecSpecDataV;
        }

        CDX_LOGV("video, height=%d, width=%d, codecdatalen=%d", qlevel->MaxHeight,qlevel->MaxWidth,
                  pMediaInfo->program[0].video[0].nCodecSpecificDataLen);
        CDX_LOGV("qlevel->id=%d, qlevel->CodecPrivateData=[%s]", qlevel->id,
                  qlevel->CodecPrivateData);

        impl->vInfo.videoNum = 1;
        memcpy(&impl->vInfo.video[0], &pMediaInfo->program[0].video[0], sizeof(VideoStreamInfo));
    }
    if(impl->ismc->hasAudio)
    {
        pMediaInfo->program[0].audioNum = impl->ismc->hasAudio;
        pMediaInfo->program[0].audioIndex = 0;

        SmsStreamT *sms = SmsGetStreamByCat(impl->ismc->smsStreams, SSTR_AUDIO);
        QualityLevelT *qlevel = GetQlevel(sms, sms->downloadQlvl);

        fourCC = qlevel->FourCC ? qlevel->FourCC : sms->defaultFourCC;
        switch(fourCC)
        {
            case SSTR_FOURCC('A','A','C','L'):
            {
                pMediaInfo->program[0].audio[0].eCodecFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
                CDX_LOGD("audio codec:[%d] AUDIO_CODEC_FORMAT_MPEG_AAC_LC",
                         AUDIO_CODEC_FORMAT_MPEG_AAC_LC);
                break;
            }
            //case SSTR_FOURCC('W','m','a','P'):
            case SSTR_FOURCC('W','M','A','2'):
            case SSTR_FOURCC('W','M','A','P'):
            {
                //*AUDIO_CODEC_FORMAT_WMA_STANDARD or AUDIO_CODEC_FORMAT_WMA_PRO ?
                pMediaInfo->program[0].audio[0].eCodecFormat = AUDIO_CODEC_FORMAT_WMA_STANDARD;
                CDX_LOGD("audio codec:[%d] AUDIO_CODEC_FORMAT_WMA_STANDARD",
                          AUDIO_CODEC_FORMAT_WMA_STANDARD);
                break;
            }
            default:
            {
                CDX_LOGW("sms not support fourcc:%d", fourCC);
                return -1;
            }
        }
        pMediaInfo->program[0].audio[0].nChannelNum = qlevel->Channels;
        pMediaInfo->program[0].audio[0].nBitsPerSample = qlevel->BitsPerSample;
        pMediaInfo->program[0].audio[0].nSampleRate = qlevel->SamplingRate;
        pMediaInfo->program[0].audio[0].nAvgBitrate = qlevel->Bitrate;
        pMediaInfo->program[0].audio[0].nBlockAlign = qlevel->nBlockAlign;
        pMediaInfo->program[0].audio[0].eSubCodecFormat = qlevel->WfTag;

        if(impl->codecSpecDataA)
        {
            free(impl->codecSpecDataA);
            impl->codecSpecDataA = NULL;
        }
        impl->codecSpecDataA = DecodeStringHexToBinary(qlevel->CodecPrivateData);
        //if(qlevel->WfTag == 0x162) //* why ?
        //{
        //    *(impl->codecSpecDataA + 22) = *(impl->codecSpecDataA + 32);
        //    *(impl->codecSpecDataA + 23) = *(impl->codecSpecDataA + 33);
        //}
        if(pMediaInfo->program[0].audio[0].eCodecFormat == AUDIO_CODEC_FORMAT_WMA_STANDARD)
        {
            pMediaInfo->program[0].audio[0].nCodecSpecificDataLen = qlevel->cbSize;
            pMediaInfo->program[0].audio[0].pCodecSpecificData = (char *)impl->codecSpecDataA + 18;
            CDX_LOGD("codec specific data size=%d", qlevel->cbSize);
        }
        else
        {
            pMediaInfo->program[0].audio[0].nCodecSpecificDataLen= strlen(qlevel->CodecPrivateData)
                                                                   / 2;
            pMediaInfo->program[0].audio[0].pCodecSpecificData = (char *)impl->codecSpecDataA;
        }

        CDX_LOGV("audio nChannelNum =%u, nBitsPerSample=%d, nSampleRate=%d, codecdatalen=%d",
                  qlevel->Channels, qlevel->BitsPerSample,
            qlevel->SamplingRate, pMediaInfo->program[0].audio[0].nCodecSpecificDataLen);
        CDX_LOGV("audio codecdata[0][1]-[last]:%x %x %x", impl->codecSpecDataA[0],
                  impl->codecSpecDataA[1],
                  impl->codecSpecDataA[strlen(qlevel->CodecPrivateData) / 2 -1]);
    }
    if(impl->ismc->hasSubtitle)
    {
        pMediaInfo->program[0].subtitleNum= impl->ismc->hasSubtitle;
        pMediaInfo->program[0].subtitleIndex = 0;

        SmsStreamT *sms = SmsGetStreamByCat(impl->ismc->smsStreams, SSTR_TEXT);
        QualityLevelT *qlevel = GetQlevel(sms, sms->downloadQlvl);

        fourCC = qlevel->FourCC ? qlevel->FourCC : sms->defaultFourCC;
        switch(fourCC)
        {
            default:
            {
                CDX_LOGW("sms not support fourcc:%d", fourCC);
                return -1;
            }
        }
        CDX_LOGD("has subtitle...");
    }

    CDX_LOGD("SMS videoNum(%d), audioNum(%d), subtitleNum(%d)", pMediaInfo->program[0].videoNum,
              pMediaInfo->program[0].audioNum,pMediaInfo->program[0].subtitleNum);

    if(impl->ismc->vodDuration)
    {
        pMediaInfo->bSeekable = 1;
        pMediaInfo->program[0].duration = impl->ismc->vodDuration / impl->ismc->timeScale * 1000;
    }
    else
    {
        CDX_LOGV("not vod.");
    }

    return 0;
}

/*
* download the manifest response of iis smooth streaming, parse it.
*/
static int __AwSstrParserInit(CdxParserT *parser)
{
    AwSstrParserImplT *impl;
    cdx_int32 ret = 0;
    cdx_int32 readSize = 0;

    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    impl->ismc->smsStreams = SmsArrayNew();
    impl->ismc->selectedSt = SmsArrayNew();
    impl->ismc->download.chunks = SmsArrayNew();
    impl->ismc->initChunks = SmsArrayNew();
    if(!impl->ismc->smsStreams || !impl->ismc->selectedSt || !impl->ismc->download.chunks ||
       !impl->ismc->initChunks)
    {
        CDX_LOGE("new array failed.");
        goto err_out;
    }

#if 0
    //* set callback, for switch bandwidth, update codec information etc.
    struct CallBack cb;
    cb.callback = SmsCallbackProcess;
    cb.pUserData = (void *)impl;
    ret = CdxParserControl(impl->mediaStream[0].parser, CDX_PSR_CMD_SET_CALLBACK, &cb);
    if(ret < 0)
    {
        CDX_LOGE("CDX_PSR_CMD_SET_CB fail");
        goto err_out;
    }
#endif
    //* download manifest
    while(1)
    {
        if(impl->exitFlag == 1)
        {
            goto err_out;
        }
        ret = CdxStreamRead(impl->stream, impl->pIsmcBuffer+readSize, 1024);
        if(ret < 0)
        {
            CDX_LOGE("read failed.");
            goto err_out;
        }
        else if(ret == 0)
        {
            break;
        }
        readSize += ret;
    }

    //* close stream or not ? todo

    impl->ismcSize = readSize;
    if(impl->ismcSize == 0)
    {
        CDX_LOGE("manifest size is 0? check...");
        goto err_out;
    }

    impl->ismc = ParseIsmc(impl->pIsmcBuffer, impl->ismcSize, impl->ismc, NULL, 0);
    if(!impl->ismc)
    {
        CDX_LOGE("parse ismc file failed.");
        goto err_out;
    }
    CDX_LOGD("parse ismc ok.");

    if(!impl->ismc->vodDuration)
    {
        impl->ismc->bLive = 1;
    }
    impl->ismc->iTracks = SmsArrayCount(impl->ismc->smsStreams);
    if (impl->ismc->protectSystem == SSTR_PROTECT_PLAYREADY)
    {
        parser->type = CDX_PARSER_SSTR_PLAYREADY;
    }

    //* choose first v/a/s stream available. ?
    SmsStreamT *tmp = NULL, *selected = NULL;
    int i;
    for(i = 0; i < impl->ismc->iTracks; i++)
    {
        tmp = (SmsStreamT*)SmsArrayItemAtIndex(impl->ismc->smsStreams, i);
        selected = SmsGetStreamByCat(impl->ismc->selectedSt, tmp->type);
        if(!selected)
            SmsArrayAppend(impl->ismc->selectedSt, tmp);
    }
    //* choose lowest quality for the first chunks
    QualityLevelT *wanted = NULL, *qlvl = NULL;
    SmsStreamT *sms;
#if 0
    for(i = 0; i < impl->ismc->iTracks; i++)
    {
        sms = (SmsStreamT *)SmsArrayItemAtIndex(impl->ismc->smsStreams, i);
        wanted = SmsArrayItemAtIndex(sms->qlevels, 0);
        int j;
        for(j = 1; j < (int)sms->qlevelNb; j++)
        {
            qlvl = SmsArrayItemAtIndex(sms->qlevels, j);
            if(qlvl->Bitrate < wanted->Bitrate) //* min: <   max: >
                wanted = qlvl;
        }
        CDX_LOGD("wanted->id=%u", wanted->id);
        sms->downloadQlvl = wanted->id;
    }
#endif
    //* choose middle quality for the first chunks
    //* choose highest quality for the first chunks
    for(i = 0; i < impl->ismc->iTracks; i++)
    {
        sms = (SmsStreamT *)SmsArrayItemAtIndex(impl->ismc->smsStreams, i);
        wanted = SmsArrayItemAtIndex(sms->qlevels, 0);
        int j;
        for(j = 1; j < (int)sms->qlevelNb; j++)
        {
            qlvl = SmsArrayItemAtIndex(sms->qlevels, j);
            if(qlvl->Bitrate > wanted->Bitrate) //* min: <   max: >
                wanted = qlvl;
        }
        //CDX_LOGD("wanted->id=%u", wanted->id);
        sms->downloadQlvl = wanted->id;

        //sms->downloadQlvl = (sms->qlevelNb > 1) ? (sms->qlevelNb/2+1) : (sms->qlevelNb);
        CDX_LOGD("first video chunk id sms->downloadQlvl=%u", sms->downloadQlvl);
    }

//------------------------------------------------------------------------------------------------>
    //* compute average bandwidth of last num download chunks
    impl->ismc->bws = SmsQueueInit(2);
    if(!impl->ismc->bws)
    {
        CDX_LOGE("band width init failed.");
        goto err_out;
    }

    ChunkT *initChunk = BuildInitChunk(impl->stream);
    if(!initChunk)
    {
        CDX_LOGE("build init chunk failed.");
        goto err_out;
    }

    SmsArrayAppend(impl->ismc->download.chunks, initChunk); //*
    SmsArrayAppend(impl->ismc->initChunks, initChunk);

    impl->ismc->download.nextChunkOffset = initChunk->size;

    cdx_int64 startTime = 0;
    ChunkT *chunk;
    for(i = 0; i < 3; i++)
    {
        sms = SmsGetStreamByCat(impl->ismc->selectedSt, IndexToEsCat(i));
        if(sms)
        {
            chunk = SmsArrayItemAtIndex(sms->chunks, 0);
            impl->ismc->download.lead[i] = chunk->startTime + impl->ismc->timeScale / 1000;
            if(!startTime)
                startTime = chunk->startTime;
        }
    }
//<------------------------------------------------------------------------------------------------

    //* each stream per handle
    if(impl->ismc->hasVideo)
    {
        ret = SmsOpenFirstSegment(impl, 0);
        if(ret < 0)
        {
            CDX_LOGE("open first video segment failed.");
            goto err_out;
        }
        impl->streamPts[0] = 0;
        impl->mediaStream[0].eos = 0;
    }
    if(impl->ismc->hasAudio)
    {
        ret = SmsOpenFirstSegment(impl, 1);
        if(ret < 0)
        {
            CDX_LOGE("open first audio segment failed.");
            goto err_out;
        }
        impl->streamPts[1] = 0;
        impl->mediaStream[1].eos = 0;
    }
    if(impl->ismc->hasSubtitle)
    {
        ret = SmsOpenFirstSegment(impl, 2);
        if(ret < 0)
        {
            CDX_LOGE("open first subtitle segment failed.");
            goto err_out;
        }
        impl->streamPts[2] = 0;
        impl->mediaStream[2].eos = 0;
    }

    ret = SstrSetMediaInfo(impl);
    if(ret < 0)
    {
        CDX_LOGE("setMediaInfo failed.");
        goto err_out;
    }
    impl->mErrno = PSR_OK;
    ret = 0;
    goto __exit;

err_out:
    if(impl->pIsmcBuffer)
    {
        free(impl->pIsmcBuffer);
        impl->pIsmcBuffer = NULL;
    }

    impl->mErrno = PSR_OPEN_FAIL;
    ret = -1;

__exit:
    pthread_mutex_lock(&impl->statusMutex);
    impl->mStatus = AW_SSTR_IDLE;
    pthread_mutex_unlock(&impl->statusMutex);
    pthread_cond_signal(&impl->cond);
    return ret;
}

static cdx_int32 SmsSwitchBandwidth(SmsStreamT *sms, cdx_int64 bandwidth)
{
    if(sms->type != SSTR_VIDEO)
        return sms->downloadQlvl;

    cdx_int64 bwCandidate = 0;
    cdx_int64 bwMin = 0;
    cdx_int64 bwMax = 0;
    cdx_uint32 bwMinId;
    cdx_uint32 bwMaxId;
    QualityLevelT *qlevel;
    cdx_uint32 ret = sms->downloadQlvl;

    qlevel = SmsArrayItemAtIndex(sms->qlevels, 0);
    bwMin = qlevel->Bitrate;
    bwMax = qlevel->Bitrate;
    bwMinId = qlevel->id;
    bwMaxId = qlevel->id;

    int i;
    for(i = 0; i < (int)sms->qlevelNb; i++)
    {
        qlevel = SmsArrayItemAtIndex(sms->qlevels, i);
        if(!qlevel)
        {
            CDX_LOGE("Could no get %uth quality level", i);
            return 0;
        }
        //logv("id=%u, bitrate=%u", qlevel->id, qlevel->Bitrate);
        if(qlevel->Bitrate < (bandwidth - bandwidth / 3) &&
                qlevel->Bitrate > bwCandidate )
        {
            bwCandidate = qlevel->Bitrate;
            ret = qlevel->id;
        }

        if(qlevel->Bitrate < bwMin)
        {
            bwMin = qlevel->Bitrate;
            bwMinId = qlevel->id;
        }
        if(qlevel->Bitrate > bwMax)
        {
            bwMax = qlevel->Bitrate;
            bwMaxId = qlevel->id;
        }

    }

    if(bwCandidate < bwMin)
    {
        bwCandidate = bwMin;
        ret = bwMinId;
    }
    else if(bwCandidate > bwMax)
    {
        bwCandidate = bwMax;
        ret = bwMaxId;
    }

    return ret;
}

static cdx_int32 __AwSstrParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 ret;
    AwSstrParserImplT *impl;
    //cdx_int32 nExtraDataLen = 0;

    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    if(impl->mStatus != AW_SSTR_IDLE && impl->mStatus != AW_SSTR_PREFETCHED)
    {
        CDX_LOGW("current status: %d can not prefetch.", impl->mStatus);
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    if(impl->mediaStream[0].eos && impl->mediaStream[1].eos && impl->mediaStream[2].eos)
    {
        CDX_LOGD("Sms parser end.");
        impl->mErrno = PSR_EOS;
        return -1;
    }

    if(impl->mStatus == AW_SSTR_PREFETCHED)
    {
        memcpy(pkt, &impl->pkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&impl->statusMutex);
    if(impl->exitFlag)
    {
        CDX_LOGD("exit prefetch.");
        pthread_mutex_unlock(&impl->statusMutex);
        return -1;
    }
    impl->mStatus = AW_SSTR_PREFETCHING;
    pthread_mutex_unlock(&impl->statusMutex);

    memset(pkt, 0x00, sizeof(CdxPacketT));

    if(impl->ismc->hasVideo)
        impl->prefetchType = SSTR_VIDEO;
    else if(impl->ismc->hasAudio)
        impl->prefetchType = SSTR_AUDIO;

    if(impl->ismc->hasVideo && impl->ismc->hasAudio && impl->streamPts[1] < impl->streamPts[0])
    {
        impl->prefetchType = SSTR_AUDIO;
    }
    if(impl->ismc->hasVideo && impl->ismc->hasAudio && impl->ismc->hasSubtitle &&
            (impl->streamPts[2] < impl->streamPts[1]) && (impl->streamPts[2] < impl->streamPts[0]))
    {
        impl->prefetchType = SSTR_TEXT;
    }
    //impl->prefetchType = SSTR_VIDEO;
    impl->bAddExtradata = 0;

    cdx_int32 mErrno;
    ret = CdxParserPrefetch(impl->mediaStream[EsCatToIndex(impl->prefetchType)].parser, pkt);
    if(ret < 0)
    {
        mErrno = CdxParserGetStatus(impl->mediaStream[EsCatToIndex(impl->prefetchType)].parser);
        if(mErrno == PSR_EOS)
        {
            SmsStreamT *sms = SmsGetStreamByCat(impl->ismc->selectedSt, impl->prefetchType);
            cdx_int32 index = EsCatToIndex(impl->prefetchType);
            cdx_int64 startTime = (cdx_int64)impl->ismc->download.lead[index];
            CDX_LOGD("index=%d, startTime=%lld", index, startTime);

            ChunkT *chunk;
            chunk = ChunkGet(sms, startTime);
            if(!chunk)
            {
                CDX_LOGE("get chunk failed.");
                goto err_out;
            }
            SmsArrayAppend(impl->ismc->download.chunks, chunk);
            impl->ismc->download.lead[index] += chunk->duration;
            //* last fragment eos.  open new one, bandwidth switch

NEXT_SEGMENT:
            //* decide which quality level will be opened.
            sms = SmsGetStreamByCat(impl->ismc->selectedSt, impl->prefetchType);

#if 1  //*open switch bandwidth or not.
            //* switch bandwidth
            struct ParserCacheStateS cacheState;
            if (CdxParserControl(impl->mediaStream[EsCatToIndex(impl->prefetchType)].parser,
                                 CDX_PSR_CMD_GET_CACHESTATE, &cacheState) < 0)
            {
                CDX_LOGE("CDX_PSR_CMD_GET_CACHESTATE failed.");
            }
            cdx_uint32 newQlevelId = SmsSwitchBandwidth(sms, cacheState.nBandwidthKbps*1000);
            QualityLevelT *newQlevel = GetQlevel(sms, newQlevelId);
            if(!newQlevel)
            {
                CDX_LOGE("Could not get quality level id %u", newQlevelId);
                goto err_out;
            }
            CDX_LOGV("newQlevelId=%u, newQlevel->Bitrate=%u, cacheState.nBandwidthKbps=%d kbps",
                      newQlevelId, newQlevel->Bitrate, cacheState.nBandwidthKbps);
            QualityLevelT *qlevel = GetQlevel(sms, sms->downloadQlvl);
            if(!qlevel)
            {
                CDX_LOGE("Could not get quality level id %u", sms->downloadQlvl);
                goto err_out;
            }
            if(newQlevel->Bitrate != qlevel->Bitrate)
            {
                CDX_LOGD("Now switch bandwidth to: %u", newQlevel->Bitrate);
                sms->downloadQlvl = newQlevelId;
                ret = SstrSetMediaInfo(impl);
                if(ret < 0)
                {
                    CDX_LOGE("set media info failed.");
                    goto err_out;
                }
                impl->infoVersionV++; //* switch bandwidth, notity
#if 0
                //if(impl->mediaInfo.program[0].video[0].eCodecFormat == VIDEO_CODEC_FORMAT_WMV3)
                {
                    impl->bAddExtradata = 1;
                    nExtraDataLen = impl->mediaInfo.program[0].video[0].nCodecSpecificDataLen;
                    CDX_LOGD("nExtraDataLen=%d", nExtraDataLen);
                    impl->nExtraDataLen = nExtraDataLen;
                }

                //* take extradata as one packet alone.
                pkt->type = CDX_MEDIA_VIDEO;
                //* for h264, startcode may 3 bytes(00 00 01), while length 4 bytes.
                pkt->length = nExtraDataLen + 10;
                pkt->pts = -1;
                pkt->flags |= (FIRST_PART|LAST_PART);
                impl->mStatus = AW_SSTR_PREFETCHED;

                //* open new segment.
                ret = SmsOpenSegment(impl, sms);
                if(ret < 0)
                {
                    CDX_LOGE("open segment failed. eos?");
                    //impl->mErrno = PSR_UNKNOWN_ERR;
                    impl->mErrno = PSR_EOS;
                    goto err_out;
                }
#endif
#if SAVE_VIDEO_STREAM
                if(sms->type == SSTR_VIDEO)
                {
                    if(impl->fpVideoStream)
                    {
                        fwrite("switchbegin", 1, 11, impl->fpVideoStream);// not exact
                        sync();
                    }
                    else
                    {
                        CDX_LOGE("demux->fpVideoStream == NULL");
                    }
                }
#endif
                CDX_LOGD("cur estimate bitrate: %d, new qlevel->bitrate: %u, qlevel->Bitrate: %u",
                          cacheState.nBandwidthKbps*1000, newQlevel->Bitrate, qlevel->Bitrate);
//                return 0;
            }
#endif

            //* open new segment.
            ret = SmsOpenSegment(impl, sms);
            if(ret < 0)
            {
                CDX_LOGE("open segment failed. eos?");
                //impl->mErrno = PSR_UNKNOWN_ERR;
                impl->mErrno = PSR_EOS;
                goto err_out;
            }

            ret = CdxParserPrefetch(impl->mediaStream[EsCatToIndex(impl->prefetchType)].parser,
                                    pkt);
            if(ret < 0)
            {
                if(impl->exitFlag)
                    goto err_out;

                mErrno = CdxParserGetStatus(impl->mediaStream[EsCatToIndex(
                                                              impl->prefetchType)].parser);
                if(mErrno == PSR_EOS)
                {
                    CDX_LOGV("eos, download next segment.");
                    impl->bAddExtradata = 0;
                    goto NEXT_SEGMENT;
                }
                CDX_LOGE("prefetch failed, ret(%d)", ret);
                impl->mErrno = PSR_UNKNOWN_ERR;
                goto err_out;
            }
            pkt->pts += impl->mediaStream[EsCatToIndex(impl->prefetchType)].baseTime;
        }
        else
        {
            impl->mErrno = mErrno;
            CDX_LOGE("prefetch failed, mErrno(%d)", mErrno);
            goto err_out;
        }
    }

    if(impl->prefetchType == SSTR_VIDEO)
    {
        pkt->infoVersion = impl->infoVersionV;
        pkt->info = &impl->vInfo;
        pkt->type = CDX_MEDIA_VIDEO;
    }
    else if(impl->prefetchType == SSTR_AUDIO)
    {
        pkt->infoVersion = impl->infoVersionA;
        pkt->info = &impl->aInfo;
        pkt->type = CDX_MEDIA_AUDIO;
    }
    else if(impl->prefetchType == SSTR_TEXT)
    {
        pkt->infoVersion = impl->infoVersionS;
        pkt->info = &impl->sInfo;
        pkt->type = CDX_MEDIA_SUBTITLE;
    }

    impl->streamPts[EsCatToIndex(impl->prefetchType)] = pkt->pts;

    pkt->flags |= (FIRST_PART|LAST_PART);
    memcpy(&impl->pkt, pkt, sizeof(CdxPacketT));

    pthread_mutex_lock(&impl->statusMutex);
    impl->mStatus = AW_SSTR_PREFETCHED;
    pthread_mutex_unlock(&impl->statusMutex);
    pthread_cond_signal(&impl->cond);
    return 0;

err_out:
    pthread_mutex_lock(&impl->statusMutex);
    impl->mStatus = AW_SSTR_IDLE;
    pthread_mutex_unlock(&impl->statusMutex);
    pthread_cond_signal(&impl->cond);
    return -1;
}
#if 0
//* to h264, while switch resolution should construct extradata.
//* 00 00 00 01 xxxxxx 00 00 00 01 yyyyyy => len0[4B] xxxx len1[4B] yyyyyy
static cdx_int32 ConstructExtraDataH264(AwSstrParserImplT *impl)
{
    //* 000000016742C00D96640988DFF8C800C458800001F480005DC0078A15500000000168CE32C8
    cdx_int32 nAnalyseSize = impl->mediaInfo.program[0].video[0].nCodecSpecificDataLen;
    cdx_uint8 tmpBuf0[1024], tmpBuf1[1024];
    cdx_int32 index0 = 0, index1 = 0, len0 = 0, len1 = 0;
    memcpy(tmpBuf0, impl->mediaInfo.program[0].video[0].pCodecSpecificData, nAnalyseSize);
    int i, startCodeNum0 = 0, startCodeNum1 = 0;

    for(i=0; (i+4)<=nAnalyseSize; i++)
    {
        if(tmpBuf0[i] == 0x00 && tmpBuf0[i+1] == 0x00 && tmpBuf0[i+2] == 0x01)
        {
            startCodeNum0 = 3;
            index0 = i;
            break;
        }
        else if(tmpBuf0[i] == 0x00 && tmpBuf0[i+1] == 0x00 && tmpBuf0[i+2] == 0x00 &&
                tmpBuf0[i+3] == 0x01)
        {
            startCodeNum0 = 4;
            index0 = i;
            break;
        }

    }
    if(startCodeNum0 == 0)
    {
        CDX_LOGE("not found startCode0");
        return -1;
    }
    for(i=startCodeNum0; (i+4)<=nAnalyseSize; i++)
    {
        if(tmpBuf0[i] == 0x00 && tmpBuf0[i+1] == 0x00 && tmpBuf0[i+2] == 0x01)
        {
            startCodeNum1 = 3;
            index1 = i;
            break;
        }
        else if(tmpBuf0[i] == 0x00 && tmpBuf0[i+1] == 0x00 && tmpBuf0[i+2] == 0x00 &&
                tmpBuf0[i+3] == 0x01)
        {
            startCodeNum1 = 4;
            index1 = i;
            break;
        }
    }
    //CDX_LOGD("index0=%d, index1=%d", index0, index1);

    if(startCodeNum1 == 0)
    {
        CDX_LOGE("not found startCode1");
        return -1;
    }

    len0 = index1 - startCodeNum0;
    len1 = nAnalyseSize - startCodeNum0 - len0 - startCodeNum1;

    tmpBuf1[0] = len0 << 24;
    tmpBuf1[1] = len0 << 16;
    tmpBuf1[2] = len0 << 8;
    tmpBuf1[3] = len0;
    memcpy(tmpBuf1+4, tmpBuf0+startCodeNum0, len0);
    tmpBuf1[index1]   = len1 << 24;
    tmpBuf1[index1+1] = len1 << 16;
    tmpBuf1[index1+2] = len1 << 8;
    tmpBuf1[index1+3] = len1;
    memcpy(tmpBuf1+len0+8, tmpBuf0+index1+startCodeNum1, len1);

    impl->nExtraDataLen = 4 + 4 + len0 + len1;
    memcpy(impl->pExtraData, tmpBuf1, impl->nExtraDataLen);

    return 0;
}
#endif
static cdx_int32 __AwSstrParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 ret = 0;
    AwSstrParserImplT *impl;
    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    if(impl->mStatus != AW_SSTR_PREFETCHED)
    {
        CDX_LOGW("status(%d) is not AW_SSTR_PREFETCHED", impl->mStatus);
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    pthread_mutex_lock(&impl->statusMutex);
    if(impl->exitFlag)
    {
        pthread_mutex_unlock(&impl->statusMutex);
        return -1;
    }
    impl->mStatus = AW_SSTR_READING;
    pthread_mutex_unlock(&impl->statusMutex);
#if 0
    if(impl->bAddExtradata)
    {
        pkt->length = impl->nExtraDataLen;

        //* for vc1, bandwidth switch, first packet should add extradata.
        if(impl->mediaInfo.program[0].video[0].eCodecFormat == VIDEO_CODEC_FORMAT_WMV3)
        {
            if(pkt->length <= pkt->buflen)
            {
                memcpy(pkt->buf,
                       impl->mediaInfo.program[0].video[0].pCodecSpecificData,
                       pkt->length);
            }
            else
            {
                memcpy(pkt->buf, impl->mediaInfo.program[0].video[0].pCodecSpecificData,
                       pkt->buflen);

                memcpy(pkt->ringBuf,
                       impl->mediaInfo.program[0].video[0].pCodecSpecificData+pkt->buflen,
                       pkt->length - pkt->buflen);
            }
        }
        else if(impl->mediaInfo.program[0].video[0].eCodecFormat == VIDEO_CODEC_FORMAT_H264)
        {
            ret = ConstructExtraDataH264(impl);
            if(ret < 0)
            {
                CDX_LOGE("ConstructExtraDataH264 failed.");
                goto out;
            }

            if(pkt->length <= pkt->buflen)
            {
                memcpy(pkt->buf, impl->pExtraData, pkt->length);
            }
            else
            {
                memcpy(pkt->buf, impl->pExtraData, pkt->buflen);
                memcpy(pkt->ringBuf, impl->pExtraData+pkt->buflen, pkt->length - pkt->buflen);
            }
        }
#if 0
        CdxPacketT packet;
        cdx_uint8 *tempBuf;
        tempBuf = malloc(pkt->length);
        if(!tempBuf)
        {
            CDX_LOGE("malloc failed.");
            impl->mStatus = AW_SSTR_IDLE;
            return -1;
        }

        memcpy(tempBuf,
               impl->mediaInfo.program[0].video[0].pCodecSpecificData,
               impl->mediaInfo.program[0].video[0].nCodecSpecificDataLen);

        packet.buf = tempBuf + impl->mediaInfo.program[0].video[0].nCodecSpecificDataLen;
        packet.length = pkt->length - impl->mediaInfo.program[0].video[0].nCodecSpecificDataLen;
        packet.buflen = packet.length;
        packet.ringBuf = NULL;
        packet.ringBufLen = 0;

        ret = CdxParserRead(impl->mediaStream[EsCatToIndex(impl->prefetchType)].parser, &packet);
        if(ret < 0)
        {
            CDX_LOGE("read failed.");
            impl->mStatus = AW_SSTR_IDLE;
            return -1;
        }

        if(pkt->length <= pkt->buflen)
        {
            memcpy(pkt->buf, tempBuf, pkt->length);
        }
        else
        {
            memcpy(pkt->buf, tempBuf, pkt->buflen);
            memcpy(pkt->ringBuf, tempBuf+pkt->buflen, pkt->length - pkt->buflen);
        }
#endif
    }
#endif
    if(0)
    {
        ;
    }
    else
    {
        ret = CdxParserRead(impl->mediaStream[EsCatToIndex(impl->prefetchType)].parser, pkt);
    }
#if SAVE_VIDEO_STREAM
    if(pkt->type == CDX_MEDIA_VIDEO)
    {
        if(impl->fpVideoStream)
        {
            fwrite(pkt->buf, 1, pkt->length, impl->fpVideoStream);// not exact
            sync();
        }
        else
        {
            CDX_LOGE("demux->fpVideoStream == NULL");
        }
    }
#endif
    //CDX_LOGD("sstr read,type=%d, pkt buflen=%d, pkt->duration=%lld, pkt->length=%d,
              //pkt->pts=%lld", pkt->type, pkt->buflen, pkt->duration, pkt->length, pkt->pts);
out:
    impl->mStatus = AW_SSTR_IDLE;
    return ret;
}

static cdx_int32 AwSstrParserForceStop(CdxParserT *parser)
{
    AwSstrParserImplT *impl;
    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    if(impl->mStatus < AW_SSTR_IDLE)
    {
        CDX_LOGW("status(%d) < AW_SSTR_IDLE", impl->mStatus);
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    pthread_mutex_lock(&impl->statusMutex);
    impl->exitFlag = 1;
    impl->mErrno = PSR_USER_CANCEL;
    if(impl->stream)
    {
        CdxStreamForceStop(impl->stream);
    }
    if(impl->ismc->hasVideo)
    {
        CdxParserForceStop(impl->mediaStream[0].parser);
    }
    if(impl->ismc->hasAudio)
    {
        CdxParserForceStop(impl->mediaStream[1].parser);
    }
    if(impl->ismc->hasSubtitle)
    {
        CdxParserForceStop(impl->mediaStream[2].parser);
    }
    pthread_mutex_unlock(&impl->statusMutex);

    pthread_mutex_lock(&impl->statusMutex);
    while(impl->mStatus != AW_SSTR_IDLE && impl->mStatus != AW_SSTR_PREFETCHED)
    {
        //usleep(10000);
        pthread_cond_wait(&impl->cond, &impl->statusMutex);
    }
    impl->mStatus = AW_SSTR_IDLE;
    pthread_mutex_unlock(&impl->statusMutex);

    return 0;
}
static cdx_int32 AwSstrParserClrForceStop(CdxParserT *parser)
{
    //cdx_int32 ret;
    AwSstrParserImplT *impl;
    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    if(impl->mStatus != AW_SSTR_IDLE)
    {
        CDX_LOGW("status(%d) != AW_SSTR_IDLE", impl->mStatus);
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    impl->exitFlag = 0;

    if(impl->stream)
    {
        CdxStreamClrForceStop(impl->stream);
    }
    if(impl->ismc->hasVideo)
    {
        CdxParserClrForceStop(impl->mediaStream[0].parser);
    }
    if(impl->ismc->hasAudio)
    {
        CdxParserClrForceStop(impl->mediaStream[1].parser);
    }
    if(impl->ismc->hasSubtitle)
    {
        CdxParserClrForceStop(impl->mediaStream[2].parser);
    }

    return 0;
}

static cdx_int32 __AwSstrParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    AwSstrParserImplT *impl;
    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
        {
            CDX_LOGW("sms is not support switch stream yet.");
            break;
        }
        case CDX_PSR_CMD_SET_FORCESTOP:
        {
            return AwSstrParserForceStop(parser);
        }
        case CDX_PSR_CMD_CLR_FORCESTOP:
        {
            return AwSstrParserClrForceStop(parser);
        }
        case CDX_PSR_CMD_GET_CACHESTATE:
        {
            //return GetCacheState(impl, param);
            break;
        }
        case CDX_PSR_CMD_SET_CALLBACK:
        {
            struct CallBack *cb = (struct CallBack *)param;
            impl->callback = cb->callback;
            impl->pUserData = cb->pUserData;
            return 0;
        }
        default:
        {
            CDX_LOGW("cmd(%d) not support.", cmd);
            break;
        }
    }

    return -1;
}

static cdx_int32 __AwSstrParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    AwSstrParserImplT *impl;

    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    if(impl->mStatus < AW_SSTR_IDLE)
    {
        CDX_LOGW("status(%d) < AW_SSTR_IDLE", impl->mStatus);
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    memset(pMediaInfo, 0x00, sizeof(CdxMediaInfoT));
    memcpy(pMediaInfo, &impl->mediaInfo, sizeof(CdxMediaInfoT));

    return 0;
}

static cdx_uint32 __AwSstrParserAttribute(CdxParserT *parser)
{
    CDX_UNUSE(parser);
    return -1;
}

static cdx_int32 __AwSstrParserGetStatus(CdxParserT *parser)
{
    AwSstrParserImplT *impl;
    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    return impl->mErrno;
}

//* not support seek in segment yet...
//* index: 0-video, 1-audio, 2-subtitle
cdx_int32 SstrParserSeekTo(AwSstrParserImplT *impl, cdx_int64 timeMs, cdx_int32 index,
                                   SeekModeType seekModeType)
{
    SmsStreamT *sms;
    ChunkT *chunk;
    cdx_int32 ret;

    sms = SmsGetStreamByCat(impl->ismc->selectedSt, IndexToEsCat(index));
    if(!sms)
    {
        CDX_LOGE("no found stream. index=%d", index);
        return -1;
    }

    chunk = ChunkGet(sms, timeMs /1000 * impl->ismc->timeScale);
    if(!chunk)
    {
        CDX_LOGE("get chunk failed.");
        return -1;
    }

    impl->ismc->download.lead[index] = chunk->startTime;
    impl->prefetchType = IndexToEsCat(index);
    ret = SmsOpenSegment(impl, sms);
    if(ret < 0)
    {
        CDX_LOGE("open segment failed.");
        return -1;
    }
    impl->streamPts[index] = chunk->startTime / impl->ismc->timeScale * 1000000;
    //CDX_LOGD("want to seek to timeMs=%lld, seek to %lld ms, index=%d",
              //timeMs, impl->streamPts[index] / 1000, index);

#if 1
    ret = CdxParserSeekTo(impl->mediaStream[index].parser,
                          chunk->startTime / impl->ismc->timeScale * 1000000,
                          seekModeType);
    if(ret < 0)
    {
        CDX_LOGE("xxx seekto failed.");
        return -1;
    }
#endif

    impl->mErrno = PSR_OK;
    return 0;
}

static cdx_int32 __AwSstrParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    cdx_int32 ret;
    AwSstrParserImplT *impl;
    cdx_int64 timeMs = timeUs / 1000;

    impl = CdxContainerOf(parser, AwSstrParserImplT, base);

    pthread_mutex_lock(&impl->statusMutex);
    if(impl->exitFlag)
    {
        impl->mErrno = PSR_USER_CANCEL;
        CDX_LOGW("PSR_USER_CANCEL");
        pthread_mutex_unlock(&impl->statusMutex);
        return -1;
    }
    impl->mStatus = AW_SSTR_SEEKING;
    pthread_mutex_unlock(&impl->statusMutex);

    //CDX_LOGV("xxx seekto %lld ms", timeMs);
    if(!impl->ismc->vodDuration || impl->ismc->bLive)
    {
        CDX_LOGW("live stream?");
        impl->mStatus = AW_SSTR_IDLE;
        return -1;
    }

    if(timeMs < 0)
    {
        CDX_LOGE("Bad timeMs. timeMs = %lld, totalTime = %llu", timeMs, impl->ismc->vodDuration);
        impl->mErrno = PSR_INVALID_OPERATION;
        ret = -1;
        goto __exit;
    }
    else if(timeMs >= (cdx_int64)(impl->ismc->vodDuration / impl->ismc->timeScale * 1000))
    {
        impl->mErrno = PSR_EOS;
        ret = 0;
        goto __exit;
    }

    //* compute which chunk will be downloaded.
    if(impl->ismc->hasVideo)
    {
        ret = SstrParserSeekTo(impl, timeMs, 0, seekModeType);
        if(ret < 0)
        {
            CDX_LOGE("video seekto timeMs(%lld) failed.", timeMs);
            impl->mErrno = PSR_UNKNOWN_ERR;
            ret = -1;
            goto __exit;
        }
    }
    if(impl->ismc->hasAudio)
    {
        ret = SstrParserSeekTo(impl, timeMs, 1, seekModeType);
        if(ret < 0)
        {
            CDX_LOGE("audio seekto timeMs(%lld) failed.", timeMs);
            impl->mErrno = PSR_UNKNOWN_ERR;
            ret = -1;
            goto __exit;
        }
    }
    if(impl->ismc->hasSubtitle)
    {
        ret = SstrParserSeekTo(impl, timeMs, 2, seekModeType);
        if(ret < 0)
        {
            CDX_LOGE("subtitle seekto timeMs(%lld) failed.", timeMs);
            impl->mErrno = PSR_UNKNOWN_ERR;
            ret = -1;
            goto __exit;
        }
    }
    ret = 0;

__exit:
    pthread_mutex_lock(&impl->statusMutex);
    impl->mStatus = AW_SSTR_IDLE;
    pthread_mutex_unlock(&impl->statusMutex);
    pthread_cond_signal(&impl->cond);
    return 0;
}

static struct CdxParserOpsS sstrParserOps =
{
    .control        = __AwSstrParserControl,
    .prefetch       = __AwSstrParserPrefetch,
    .read           = __AwSstrParserRead,
    .getMediaInfo   = __AwSstrParserGetMediaInfo,
    .seekTo         = __AwSstrParserSeekTo,
    .attribute      = __AwSstrParserAttribute,
    .getStatus      = __AwSstrParserGetStatus,
    .close          = __AwSstrParserClose,
    .init           = __AwSstrParserInit
};

static CdxParserT *__AwSstrParserCreate(CdxStreamT *stream, cdx_uint32 flags)
{
    AwSstrParserImplT *impl;
    cdx_int32 result;
    CDX_UNUSE(flags);

    impl = SstrInit();
    CDX_FORCE_CHECK(impl);
    if(NULL == impl)
    {
        CDX_LOGE("Initiate sstr module error.");
        return NULL;
    }

    impl->pIsmcBuffer = (cdx_char *)malloc(ISMC_BUFF_SIZE);
    if(!impl->pIsmcBuffer)
    {
        CDX_LOGE("malloc ismcbuf failed.");
        goto err_out;
    }
    memset(impl->pIsmcBuffer, 0x00, ISMC_BUFF_SIZE);
    //* get the base url, should remove the last part of the url
    //* example: http://mediadl.microsoft.com/mediadl/iisnet/smoothmedia/Experience/
    //* BigBuckBunny_720p.ism/Manifest
    char *ismcUrl;
    char *pos;
    result = CdxStreamGetMetaData(stream, "uri", (void **)&ismcUrl);
    if(result < 0)
    {
        CDX_LOGE("Get url failed.");
        goto err_out;
    }
    impl->ismcUrl = strdup(ismcUrl);
    pos = strrchr(impl->ismcUrl, '/');
    *pos = '\0';
    impl->ismc->baseUrl = impl->ismcUrl;

    result = InitMediaStreams(impl);
    if(result < 0)
    {
        CDX_LOGE("init media streams failed.");
        goto err_out;
    }
#if SAVE_VIDEO_STREAM
    impl->fpVideoStream = fopen("/data/camera/sstr_videostream.es", "wb+");
    if (!impl->fpVideoStream)
    {
        CDX_LOGE("open video stream debug file failure errno(%d)", errno);
    }
#endif
    impl->base.ops = &sstrParserOps;
    impl->stream = stream;
    pthread_mutex_init(&impl->statusMutex, NULL);
    pthread_cond_init(&impl->cond, NULL);
    impl->mStatus = AW_SSTR_INITIALIZED;
    impl->mErrno = PSR_INVALID;
    return &impl->base;

err_out:
    if(impl->ismc)
    {
        free(impl->ismc);
        impl->ismc = NULL;
    }
    if(impl->pIsmcBuffer)
    {
        free(impl->pIsmcBuffer);
        impl->pIsmcBuffer = NULL;
    }
    if(impl->ismcUrl)
    {
        free(impl->ismcUrl);
        impl->ismcUrl = NULL;
    }
    FreeMediaStreams(impl);
    pthread_mutex_destroy(&impl->statusMutex);
    free(impl);

    return NULL;
}

//* UTF-16LE / UTF-16BE
static cdx_uint8 probeNeed[][42] =
{
    {0x3C, 0x00, 0x53, 0x00, 0x6D, 0x00, 0x6F, 0x00, 0x6F, 0x00, 0x74, 0x00, 0x68, 0x00,
     0x53, 0x00, 0x74, 0x00, 0x72, 0x00, 0x65, 0x00, 0x61, 0x00, 0x6D, 0x00, 0x69, 0x00,
     0x6E, 0x00, 0x67, 0x00, 0x4D, 0x00, 0x65, 0x00, 0x64, 0x00, 0x69, 0x00, 0x61, 0x00},

    {0x00, 0x3C, 0x00, 0x53, 0x00, 0x6D, 0x00, 0x6F, 0x00, 0x6F, 0x00, 0x74, 0x00, 0x68,
     0x00, 0x53, 0x00, 0x74, 0x00, 0x72, 0x00, 0x65, 0x00, 0x61, 0x00, 0x6D, 0x00, 0x69,
     0x00, 0x6E, 0x00, 0x67, 0x00, 0x4D, 0x00, 0x65, 0x00, 0x64, 0x00, 0x69, 0x00, 0x61},
    {0}
};

static cdx_uint32 __AwSstrParserProbe(CdxStreamProbeDataT *probeData)
{
    const cdx_char *need = "<SmoothStreamingMedia";
    //const cdx_char *encoding = NULL;
    cdx_char *probe;
    cdx_bool ret = CDX_FALSE;
    cdx_int32 i;

    const size_t len = 512;

    if(probeData->len < len)
    {
        CDX_LOGE("probe data is not enough.");
        return 0;
    }
    probe = malloc(len);
    if(probe == NULL)
    {
        CDX_LOGE("malloc failed.");
        return 0;
    }
    memcpy(probe, probeData->buf, len);
    probe[len - 1] = probe[len - 2] = '\0';

    if(strstr((const cdx_char *)probe, need) != NULL)
    {
        ret = CDX_TRUE;
    }

    for (i = 0; probeNeed[i][0]; i++)
    {
        if (memmem(probe, len, probeNeed[i], 42))
        {
            ret =  CDX_TRUE;
            CDX_LOGD("utf-16 encoding, probe ok.");
            break;
        }
    }
#if 0
    else // maybe it is utf-16 encoding, what about other encodings?
    {
        if(!memcmp(probe, "\xFF\xFE", 2))
            encoding = "UTF-16LE";
        else if(!memcmp(probe, "\xFE\xFF", 2))
            encoding = "UTF-16BE";
        else
        {
            CDX_LOGE("SSTR probe failed.");
            free(probe);
            return CDX_FALSE;
        }
        CDX_LOGD("encoding:%s", encoding);
        probe = AwFromCharset(encoding, probe, 512);
        if(probe != NULL && strstr(probe, need) != NULL)
            ret = CDX_TRUE;
    }
#endif
    free(probe);
    return ret*100;
}

CdxParserCreatorT sstrParserCtor =
{
    .create  = __AwSstrParserCreate,
    .probe = __AwSstrParserProbe
};
