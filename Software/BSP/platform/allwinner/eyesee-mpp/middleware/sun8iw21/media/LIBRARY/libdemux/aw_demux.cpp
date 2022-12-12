/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "aw_demux"
#include <utils/plat_log.h>
//#define CONFIG_LOG_LEVEL 3
#include <dlfcn.h>

//#include <utils/KeyedVector.h>
//#include <utils/String8.h>

#include "CdxParser.h"          //* parser library in "LIBRARY/DEMUX/PARSER/include/"
#include "CdxStream.h"          //* parser library in "LIBRARY/DEMUX/STREAM/include/"

//#include <CDX_Debug.h>
#include <CDX_ErrorType.h>
#include <CDX_fileformat.h>
#include "cedarx_demux.h"
#include "plat_math.h"
#include "aw_demux.h"

#define AWPLAYER_CONFIG_DISABLE_VIDEO       0
#define AWPLAYER_CONFIG_DISABLE_AUDIO       0
#define AWPLAYER_CONFIG_DISABLE_SUBTITLE    0
#define AWPLAYER_CONFIG_DISALBE_MULTI_AUDIO 0
using namespace std;

typedef struct AwDemuxerAPI {
    CdxParserT*         pParser;
    CdxStreamT*         pStream;

    pthread_mutex_t mMutex;
    char*   mpSkipChunk;
    int     mSkipSize;
    int (*mpEventHandler)(void* cookie, int event, void *data);
    void* mpCookie;
} AwDemuxerAPI;

void clearCdxMediaInfoT(CdxMediaInfoT *pMediaInfo, int nParserType)
{
    //&pDemuxData->cdx_mediainfo
    int i;
    enum CdxParserTypeE eParserType = (enum CdxParserTypeE)nParserType;

    if(CDX_PARSER_AVI == eParserType)
    {
        alogd("avi need not free some members now!");
        /*for(i=0; i<pMediaInfo->program[0].audioNum; i++)
        {
            if(pMediaInfo->program[0].audio[i].pCodecSpecificData)
            {
                free(pMediaInfo->program[0].audio[i].pCodecSpecificData);
                pMediaInfo->program[0].audio[i].pCodecSpecificData = NULL;
                pMediaInfo->program[0].audio[i].nCodecSpecificDataLen = 0;
            }
        }
        for(i=0; i<pMediaInfo->program[0].videoNum; i++)
        {
            if(pMediaInfo->program[0].video[i].pCodecSpecificData)
            {
                free(pMediaInfo->program[0].video[i].pCodecSpecificData);
                pMediaInfo->program[0].video[i].pCodecSpecificData = NULL;
                pMediaInfo->program[0].video[i].nCodecSpecificDataLen = 0;
            }
        }
        for(i=0; i<pMediaInfo->program[0].subtitleNum; i++)
        {
            if(pMediaInfo->program[0].subtitle[i].pCodecSpecificData)
            {
                free(pMediaInfo->program[0].subtitle[i].pCodecSpecificData);
                pMediaInfo->program[0].subtitle[i].pCodecSpecificData = NULL;
                pMediaInfo->program[0].subtitle[i].nCodecSpecificDataLen = 0;
            }
        }*/
    }
    else
    {
        alogd("parserType[%d] free nothing", eParserType);
    }
}
void clearDataSourceFields(CdxDataSourceT* source)
{
    CdxHttpHeaderFieldsT* pHttpHeaders;
    int                   i;
    int                   nHeaderSize;

    if(source->uri != NULL)
    {
        free(source->uri);
        source->uri = NULL;
    }

    if(source->extraDataType == EXTRA_DATA_HTTP_HEADER &&
       source->extraData != NULL)
    {
        pHttpHeaders = (CdxHttpHeaderFieldsT*)source->extraData;
        nHeaderSize  = pHttpHeaders->num;

        for(i=0; i<nHeaderSize; i++)
        {
            if(pHttpHeaders->pHttpHeader[i].key != NULL)
                free((void*)pHttpHeaders->pHttpHeader[i].key);
            if(pHttpHeaders->pHttpHeader[i].val != NULL)
                free((void*)pHttpHeaders->pHttpHeader[i].val);
        }

        free(pHttpHeaders->pHttpHeader);
        free(pHttpHeaders);
        source->extraData = NULL;
        source->extraDataType = EXTRA_DATA_UNKNOWN;
    }

    return;
}

int AwDemuxerSetDataSource(/*out*/CdxDataSourceT *pDataSource, /*in*/CedarXDataSourceDesc *datasrc_desc)
{
    alogv("DemuxSetDataSource");
    if(datasrc_desc->source_type == CEDARX_SOURCE_FILEPATH)
    {
        //* data source of url path.
        aloge("error! no support current!");
    }
    else if(datasrc_desc->source_type == CEDARX_SOURCE_FD)
    {
        //* data source is a file descriptor.
        int     fd;
        int64_t nOffset;
        int64_t nLength;
        char    str[128];

        clearDataSourceFields(pDataSource);

        fd = datasrc_desc->ext_fd_desc.fd;
        nOffset = datasrc_desc->ext_fd_desc.offset;
        nLength = datasrc_desc->ext_fd_desc.length;

        memset(pDataSource, 0x00, sizeof(CdxDataSourceT));
        sprintf(str, "fd://%d?offset=%lld&length=%lld", fd, nOffset, nLength);
        pDataSource->uri = strdup(str);
        if(pDataSource->uri == NULL)
        {
            return CDX_ERROR;
        }
    }
    return CDX_OK;
}

static int ParserCallbackProcess(void* pUserData, int eMessageId, void* param)
{
    AwDemuxerAPI *demux = (AwDemuxerAPI *)pUserData;
    switch(eMessageId) {
    case PARSER_NOTIFY_VIDEO_STREAM_CHANGE:
        aloge("fatal error! not process PARSER_NOTIFY_VIDEO_STREAM_CHANGE, write code now!");
        if(demux->mpEventHandler)
        {
            demux->mpEventHandler(demux->mpCookie, eMessageId, param);
        }
        break;
    case PARSER_NOTIFY_AUDIO_STREAM_CHANGE:
        aloge("fatal error! not process PARSER_NOTIFY_AUDIO_STREAM_CHANGE, write code now!");
        if(demux->mpEventHandler)
        {
            demux->mpEventHandler(demux->mpCookie, eMessageId, param);
        }
        break;
    default:
        alogw("ignore demux callback message, eMessageId = 0x%x.", eMessageId);
        return -1;
    }

    return 0;
}

int aw_demux_open(struct CedarXDemuxerAPI *handle, /*out*/CdxMediaInfoT *pMediaInfo, /*in*/CdxDataSourceT *datasrc_desc)
{
    AwDemuxerAPI *aw_dmx;
    int flags = 0;
    int ret = CDX_OK;
    int err;
    AwDemuxInfo *pPrivInfo = (AwDemuxInfo*)handle->reserved_usr_0;
    alogd("==== aw_demux_open url:%s====", datasrc_desc->uri);

    aw_dmx = (AwDemuxerAPI *)calloc(1, sizeof(AwDemuxerAPI));
    if (aw_dmx == NULL) {
        return CDX_ERROR;
    }
    aw_dmx->mpEventHandler = pPrivInfo->mpEventHandler;
    aw_dmx->mpCookie = pPrivInfo->mpCookie;
    err = pthread_mutex_init(&aw_dmx->mMutex, NULL);
    if(err!=0)
    {
        aloge("pthread mutex init fail!");
        ret = CDX_ERROR;
        goto ERROR_RET;
    }

#if AWPLAYER_CONFIG_DISABLE_SUBTITLE
    flags |= DISABLE_SUBTITLE;
#endif
#if AWPLAYER_CONFIG_DISABLE_AUDIO
    flags |= DISABLE_AUDIO;
#endif
#if AWPLAYER_CONFIG_DISABLE_VIDEO
    flags |= DISABLE_VIDEO;
#endif
#if !AWPLAYER_CONFIG_DISALBE_MULTI_AUDIO
    flags |= MUTIL_AUDIO;
#endif
    struct CallBack cb;
    cb.callback = ParserCallbackProcess;
    cb.pUserData = (void *)aw_dmx;
    ContorlTask parserContorlTask, *contorlTask;
    parserContorlTask.cmd = CDX_PSR_CMD_SET_CALLBACK;
    parserContorlTask.param = (void *)&cb;
    parserContorlTask.next = NULL;
    contorlTask = &parserContorlTask;
    if(flags & MIRACST)
    {
        alogw("flags[0x%x] not support MIRACST tmp", flags);
    }
    ret = CdxParserPrepare(datasrc_desc, flags, &aw_dmx->mMutex, NULL,
                &aw_dmx->pParser, &aw_dmx->pStream, &parserContorlTask, NULL);
    if(ret < 0)
    {
        alogw("Cdx ParserPrepare return error[%d], pParser[%p], pStream[%p]", ret, aw_dmx->pParser, aw_dmx->pStream);
        if (aw_dmx->pParser != NULL)
        {
            //aw_dmx->pParser->ops->close(aw_dmx->pParser);
            CdxParserClose(aw_dmx->pParser);
            aw_dmx->pParser = NULL;
            aw_dmx->pStream = NULL;
        }
        if(aw_dmx->pStream)
        {
            CdxStreamClose(aw_dmx->pStream);
            aw_dmx->pStream = NULL;
        }
        ret = CDX_ERROR;
        goto ERROR_RET1;
    }
    alogd("aw_demux_open: aw_dmx->pParser(%p), parserType[%d], is it forbiden:)?", aw_dmx->pParser, aw_dmx->pParser->type);

    CdxParserGetMediaInfo(aw_dmx->pParser, pMediaInfo);

    handle->reserved_0 = (void*)aw_dmx;

    alogd("aw_demux_open success.");
    return CDX_OK;

ERROR_RET1:
    pthread_mutex_destroy(&aw_dmx->mMutex);
ERROR_RET:
    free(aw_dmx);
    return ret;
}

void aw_demux_close(struct CedarXDemuxerAPI *handle)
{
    AwDemuxerAPI *aw_dmx = (AwDemuxerAPI *)handle->reserved_0;

    if(aw_dmx) {
        if (aw_dmx->pParser != NULL) {
            CdxParserClose(aw_dmx->pParser);
            aw_dmx->pParser = NULL;
            aw_dmx->pStream = NULL;
        }
        if(aw_dmx->pStream)
        {
            CdxStreamClose(aw_dmx->pStream);
            aw_dmx->pStream = NULL;
        }
        if(aw_dmx->mpSkipChunk)
        {
            free(aw_dmx->mpSkipChunk);
            aw_dmx->mpSkipChunk = NULL;
            aw_dmx->mSkipSize = 0;
        }
        pthread_mutex_destroy(&aw_dmx->mMutex);
        free(aw_dmx);
        handle->reserved_0 = NULL;
    }
}

int  aw_demux_prefetch(struct CedarXDemuxerAPI *handle, CdxPacketT *cdx_pkt)
{
    AwDemuxerAPI *aw_dmx = (AwDemuxerAPI *)handle->reserved_0;

    return CdxParserPrefetch(aw_dmx->pParser, cdx_pkt);
}

int aw_demux_read(struct CedarXDemuxerAPI *handle, CdxPacketT *cdx_pkt)
{
    AwDemuxerAPI *aw_dmx = (AwDemuxerAPI *)handle->reserved_0;

    if(cdx_pkt != NULL)
        return CdxParserRead(aw_dmx->pParser, cdx_pkt);
    else
        return CDX_ERROR;
}

int aw_demux_seek(struct CedarXDemuxerAPI *handle, int64_t abs_seek_secs, int flags)
{
    int ret = CDX_OK;
    AwDemuxerAPI *aw_dmx = (AwDemuxerAPI *)handle->reserved_0;
    int seekRet = CdxParserSeekTo(aw_dmx->pParser, ((int64_t)abs_seek_secs*1000)*1000, AW_SEEK_PREVIOUS_SYNC);
    if(seekRet!=0)
    {
        aloge("seek fail[%d]!", seekRet);
        ret = CDX_ERROR;
    }
    return ret;
}

int  aw_demux_control(struct CedarXDemuxerAPI *handle, int cmd, int cmd_sub, void *arg)
{
    int ret = CDX_OK;
    AwDemuxerAPI *aw_dmx = (AwDemuxerAPI *)handle->reserved_0;
    AwDemuxInfo *pPrivInfo = (AwDemuxInfo*)handle->reserved_usr_0;

    switch (cmd) {
    case CDX_DMX_CMD_GET_CACHE_STATE:
        return CDX_OK;
    case CDX_DMX_CMD_MEDIAMODE_CONTRL:
    {
        if(!aw_dmx)
        {
            aloge("fatal error! aw_demux == NULL, cmd[%d], cmd_sub[0x%x], arg[%p]", cmd, cmd_sub, arg);
            return CDX_ERROR;
        }
        switch (cmd_sub) {
            default:
                break;
        }
        break;
    }
    case CDX_DMX_CMD_SWITCH_VIDEO:
    {
        if(!aw_dmx)
        {
            aloge("fatal error! aw_demux == NULL, cmd[%d], cmd_sub[0x%x], arg[%p]", cmd, cmd_sub, arg);
            return CDX_ERROR;
        }
        int nVideoIndex = *(int*)arg;
        alogd("aw_demux_control: aw_dmx->pParser(%p), select video:%d", aw_dmx->pParser, nVideoIndex);
        ret = CdxParserControl(aw_dmx->pParser, CDX_PSR_CMD_SET_VIDEO_INDEX, arg);
        break;
    }
    case CDX_DMX_CMD_SWITCH_AUDIO:
    {
        //int nAudioStreamIndex = *(int*)arg;
        break;
    }
    case CDX_DMX_CMD_SWITCH_SUBTITLE:
    {
        //int nSubtitleStreamIndex = *(int*)arg;
        break;
    }
    case CDX_DMX_CMD_SET_DEMUX_EVENT_CALLBACK:
    {
        CdxDemuxEventCallbackInfo *pCbInfo = (CdxDemuxEventCallbackInfo*)arg;
        *(void**)&pPrivInfo->mpEventHandler = pCbInfo->mpCbEventHandler;
        pPrivInfo->mpCookie = pCbInfo->mpCookie;
        if(aw_dmx)
        {
            //aw_dmx->mpEventHandler = arg;
            *(void**)&aw_dmx->mpEventHandler = pCbInfo->mpCbEventHandler;
            aw_dmx->mpCookie = pCbInfo->mpCookie;
        }
        break;
    }
    case CDX_DMX_CMD_GET_STATUS:
        if(!aw_dmx)
        {
            aloge("fatal error! aw_demux == NULL, cmd[%d], cmd_sub[0x%x], arg[%p]", cmd, cmd_sub, arg);
            return CDX_ERROR;
        }
        alogd("aw_demux_control: aw_dmx->pParser(%p)", aw_dmx->pParser);
        ret = CdxParserGetStatus(aw_dmx->pParser);
        break;
    case CDX_DMX_CMD_SKIP_CHUNK_DATA:
    {
        if(!aw_dmx)
        {
            aloge("fatal error! aw_demux == NULL, cmd[%d], cmd_sub[0x%x], arg[%p]", cmd, cmd_sub, arg);
            return CDX_ERROR;
        }
        CdxPacketT *pPacket = (CdxPacketT*)arg;
        if(pPacket->length <= 0)
        {
            aloge("fatal error! pkt length[%d]<=0, skip nothing", pPacket->length);
            break;
        }
        if(aw_dmx->mSkipSize < pPacket->length)
        {
            if(aw_dmx->mpSkipChunk)
            {
                free(aw_dmx->mpSkipChunk);
                aw_dmx->mpSkipChunk = NULL;
                aw_dmx->mSkipSize = 0;
            }
            int nSkipSize = ALIGN(pPacket->length, 1024);
            aw_dmx->mpSkipChunk = (char*)malloc(nSkipSize);
            if(NULL == aw_dmx->mpSkipChunk)
            {
                aloge("fatal error! malloc fail[%s]", strerror(errno));
                ret = CDX_ERROR;
                break;
            }
            aw_dmx->mSkipSize = nSkipSize;
            alogd("malloc skip BufSize[%d]kB", aw_dmx->mSkipSize/1024);
        }
        pPacket->buf = aw_dmx->mpSkipChunk;
        pPacket->ringBuf = NULL;
        pPacket->buflen = aw_dmx->mSkipSize;
        pPacket->ringBufLen = 0;
        if(0!=CdxParserRead(aw_dmx->pParser, pPacket))
        {
            aloge("fatal error! cdx parser read fail");
            ret = CDX_ERROR;
        }
        break;
    }
    case CDX_DMX_CMD_GET_PARSER_TYPE:
    {
        if(!aw_dmx)
        {
            aloge("fatal error! aw_demux == NULL, cmd[%d], cmd_sub[0x%x], arg[%p]", cmd, cmd_sub, arg);
            return CDX_ERROR;
        }
        ret = (int)aw_dmx->pParser->type;
        break;
    }
    case CDX_DMX_CMD_GetMediaInfo:
    {
        CdxMediaInfoT *pCdxMediaInfo = (CdxMediaInfoT*)arg;
        if (pCdxMediaInfo)
        {
            ret = CdxParserGetMediaInfo(aw_dmx->pParser, pCdxMediaInfo);
        }
        else
        {
            ret = CDX_ERROR_OUT_OF_MEMORY;
        }
        break;
    }
    default:
        break;
    }

    return ret;
}

void aw_demux_stop(struct CedarXDemuxerAPI *handle, void *arg)
{
    aloge("fatal error! not support!");
}

extern "C" {

CedarXDemuxerAPI cdx_dmxs_aw = {
    name:                   "aw_dmx",
    subname:                "",
    shortdesc:              "aw demuxer packing",
    use_sft_demux_handle:   NULL,
    reserved_0:             NULL,
    reserved_1:             NULL,
    reserved_2:             NULL,
    dl_handle:              NULL,
    reserved_usr_0:         NULL,
    reserved_usr_1:         NULL,
    open:                   aw_demux_open,
    close:                  aw_demux_close,
    prefetch:               aw_demux_prefetch,
    read:                   aw_demux_read,
    seek:                   aw_demux_seek,
    control:                aw_demux_control,
    stop:                   aw_demux_stop,
};

}

