/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmsStream.c
 * Description : MmsStream
 * History :
 *
 */

#include "CdxMmsStream.h"
#include "stdlib.h"

#define MMS_PROTECT_DATA_SIZE 0

extern void CdxUrlFree(CdxUrlT* url);

/************************************/
extern void* CdxReadAsfStream(void* p_arg);

extern int CdxNopStreamingSeek( aw_mms_inf_t* mmsStreamInf);
extern int CdxMmshStreamingSeek(aw_mms_inf_t* mmsStreamInf);

static int MmsGetCacheState(struct StreamCacheStateS *cacheState, CdxStreamT *stream)//0424
{
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;
    if(!mmsStreamInf)
    {
        return -1;
    }

    cdx_int64 totSize = mmsStreamInf->fileSize;
    cdx_int64 bufPos = mmsStreamInf->buf_pos;
    //cdx_int64 readPos = mmsStreamInf->dmx_read_pos;
    double percentage = 0;
    cdx_int32 kbps = 0;//4000;//512KB/s

    memset(cacheState, 0, sizeof(struct StreamCacheStateS));

    if(totSize > 0)
    {
        percentage = 100.0 * (double)(bufPos)/totSize;
    }
    else
    {
        percentage = 0.0;
    }

    if (percentage > 100)
    {
        percentage = 100;
    }

    cacheState->nBandwidthKbps = kbps;
    cacheState->nCacheCapacity = mmsStreamInf->stream_buf_size;
    cacheState->nCacheSize = mmsStreamInf->validDataSize;

    return 0;
}

//***********************************************************************//
//**********************************************************************//

int CdxMmsClose(aw_mms_inf_t* mmsStreamInf)
{
    if(!mmsStreamInf)
    {
        return 0;
    }
    int ret;
    mmsStreamInf->exitFlag = 1;
    mmsStreamInf->closeFlag = 1;

    CdxAtomicDec(&mmsStreamInf->ref);
    while((ret = CdxAtomicRead(&mmsStreamInf->ref)) != 0)
    {
        CDX_LOGD("---- ret = %d", ret);
        usleep(10000);
    }

    //if(mmsStreamInf != NULL)
    {
        while(mmsStreamInf->eof == 0)
        {
        CDX_LOGD("usleep");
            usleep(10000);
        }

        if(mmsStreamInf->sockFd > 0)
        {
            closesocket(mmsStreamInf->sockFd);
        }
        if(mmsStreamInf->awUrl != NULL)
        {
            CdxUrlFree(mmsStreamInf->awUrl);
            mmsStreamInf->awUrl = NULL;
        }
        if(mmsStreamInf->firstChunk!=NULL)
        {
            free(mmsStreamInf->firstChunk);
            mmsStreamInf->firstChunk = NULL;
        }
        if(mmsStreamInf->buffer)
        {
            free(mmsStreamInf->buffer);
            mmsStreamInf->buffer = NULL;
        }
        if(mmsStreamInf->asfHttpDataBuffer != NULL)
        {
            free(mmsStreamInf->asfHttpDataBuffer);
            mmsStreamInf->asfHttpDataBuffer = NULL;
        }
        if(mmsStreamInf->data != NULL)
        {
            free(mmsStreamInf->data);
            mmsStreamInf->data = NULL;
        }

        if(mmsStreamInf->asfChunkDataBuffer != NULL)
        {
            free(mmsStreamInf->asfChunkDataBuffer);
            mmsStreamInf->asfChunkDataBuffer = NULL;
        }

        if(mmsStreamInf->probeData.buf)
        {
            free(mmsStreamInf->probeData.buf);
            mmsStreamInf->probeData.buf = NULL;
        }

        if (mmsStreamInf->uri)
        {
            free(mmsStreamInf->uri);
            mmsStreamInf->uri = NULL;
        }
        pthread_mutex_destroy(&mmsStreamInf->bufferMutex);
        pthread_mutex_destroy(&mmsStreamInf->lock);
        pthread_cond_destroy(&mmsStreamInf->cond);
    }
    free(mmsStreamInf);

    return 0;
}

CdxStreamProbeDataT* __MmsGetProbeData(CdxStreamT * stream)
{
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;

    return &(mmsStreamInf->probeData);
}

int __MmsRead(CdxStreamT* stream, void* buf, unsigned int size)
{
    int sendSize = 0;
    int remainSize = 0;
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;

    CdxAtomicInc(&mmsStreamInf->ref);
    CdxAtomicSet(&mmsStreamInf->mState, MMS_STREAM_READING);
    sendSize = size;
    while(mmsStreamInf->validDataSize < (int)size)
    {
        if(mmsStreamInf->eof == 1)
        {
            sendSize = (sendSize >= mmsStreamInf->validDataSize)?
                    mmsStreamInf->validDataSize: sendSize;
            break;
        }
        usleep(40*1000);
    }

    remainSize = mmsStreamInf->bufEndPtr - mmsStreamInf->bufReadPtr +1;
    if(remainSize >= sendSize)
    {
        memcpy((char*)buf, mmsStreamInf->bufReadPtr, sendSize);
    }
    else
    {
        memcpy((char*)buf, mmsStreamInf->bufReadPtr, remainSize);
        memcpy((char*)buf+remainSize, mmsStreamInf->buffer, sendSize-remainSize);
    }

    mmsStreamInf->bufReadPtr += sendSize;
    if(mmsStreamInf->bufReadPtr > mmsStreamInf->bufEndPtr)
    {
        mmsStreamInf->bufReadPtr -= mmsStreamInf->stream_buf_size;
    }
    mmsStreamInf->bufReleasePtr = mmsStreamInf->bufReadPtr;
    pthread_mutex_lock(&mmsStreamInf->bufferMutex);
    mmsStreamInf->dmx_read_pos += sendSize;
    mmsStreamInf->validDataSize -= sendSize;
    pthread_mutex_unlock(&mmsStreamInf->bufferMutex);

    CdxAtomicSet(&mmsStreamInf->mState, MMS_STREAM_IDLE);
    CdxAtomicDec(&mmsStreamInf->ref);
    return sendSize;
}

int __MmsClose(CdxStreamT* stream)
{
    if(stream == NULL)
    {
        return 0;
    }

    aw_mms_inf_t* mmsStreamInf = NULL;
    mmsStreamInf = (aw_mms_inf_t*)stream;

    #ifdef SAVE_MMS
    fclose(mmsStreamInf->fp);
    #endif
       CdxMmsClose(mmsStreamInf);

    return 0;
}

int __MmsGetIoState(CdxStreamT * stream)
{
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;
    return mmsStreamInf->iostate;
}

cdx_uint32 __MmsAttribute(CdxStreamT * stream)
{
    CDX_UNUSE(stream);
    //CDX_LOGV(" how to get the attribute of mmsh or mmst ????? ---- It is not sure");
    return CDX_STREAM_FLAG_NET;
}

int __MmsEos(CdxStreamT* stream)
{
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;
    return mmsStreamInf->eof;
}

int __MmsForceStop(CdxStreamT *stream)
{
    aw_mms_inf_t* impl = (aw_mms_inf_t*)stream;
    impl->exitFlag = 1;
    int ref;

    if(CdxAtomicRead(&impl->mState) == MMS_STREAM_FORCESTOPPED)
    {
        return 0;
    }
    while((ref = CdxAtomicRead(&impl->mState)) != MMS_STREAM_IDLE)
    {
        usleep(10000);
    }
    CdxAtomicSet(&impl->mState, MMS_STREAM_FORCESTOPPED);
    return 0;
}

int __MmsClrForceStop(CdxStreamT *stream)
{
    aw_mms_inf_t* impl = (aw_mms_inf_t*)stream;
    impl->exitFlag = 0;
    CdxAtomicSet(&impl->mState, MMS_STREAM_IDLE);
    return 0;
}

int __MmsControl(CdxStreamT* stream, int cmd, void* param)
{
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;

    switch(cmd)
    {
        case STREAM_CMD_GET_CACHESTATE:
            return MmsGetCacheState((struct StreamCacheStateS *)param, stream);

        case STREAM_CMD_RESET_STREAM:
            CDX_LOGD("mms playlist reset stream");
            pthread_mutex_lock(&mmsStreamInf->bufferMutex);
            mmsStreamInf->bufReadPtr    = mmsStreamInf->tmpReadPtr;
            mmsStreamInf->bufReleasePtr = mmsStreamInf->tmpReleasePtr;
            mmsStreamInf->dmx_read_pos  = 0;
            mmsStreamInf->validDataSize = mmsStreamInf->buf_pos - mmsStreamInf->tmp_dmx_read_pos;
            pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
            mmsStreamInf->iostate = CDX_IO_STATE_OK;
            break;

        case STREAM_CMD_SET_FORCESTOP:
            //break;
            return __MmsForceStop(stream);

        case STREAM_CMD_CLR_FORCESTOP:
            return __MmsClrForceStop(stream);

        default:
            break;
    }

    return CDX_SUCCESS;

}

int __MmsSeek(CdxStreamT* stream, cdx_int64 offset, int whence)
{
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;
    int64_t seekPos = 0;

    CdxAtomicInc(&mmsStreamInf->ref);
    CdxAtomicSet(&mmsStreamInf->mState, MMS_STREAM_SEEKING);
   // CDX_LOGD("mms seek(offset = %lld, whence = %d)", offset, whence);

    switch(whence)
    {
        case SEEK_SET:
            seekPos = offset;
            break;

        case SEEK_CUR:
            seekPos = mmsStreamInf->dmx_read_pos + offset;
            break;
        case SEEK_END:
            CdxAtomicDec(&mmsStreamInf->ref);
            return -1;
    }

    if(seekPos < 0)
    {
        CDX_LOGW("we can not seek to this position!");
        CdxAtomicDec(&mmsStreamInf->ref);
        return -1;
    }

    while(seekPos > mmsStreamInf->buf_pos)
    {
        if(mmsStreamInf->exitFlag)
        {
            CdxAtomicDec(&mmsStreamInf->ref);
            CdxAtomicSet(&mmsStreamInf->mState, MMS_STREAM_IDLE);
            return -1;
        }
        usleep(100);
    }

    if(seekPos > (mmsStreamInf->dmx_read_pos -1024*1024))
    {
         pthread_mutex_lock(&mmsStreamInf->bufferMutex);
         mmsStreamInf->bufReadPtr += (seekPos - mmsStreamInf->dmx_read_pos);
         mmsStreamInf->validDataSize -= (seekPos - mmsStreamInf->dmx_read_pos);
         if(mmsStreamInf->bufReadPtr > mmsStreamInf->bufEndPtr)
         {
            mmsStreamInf->bufReadPtr -= mmsStreamInf->stream_buf_size;
         }
         else if(mmsStreamInf->bufReadPtr < mmsStreamInf->buffer)
         {
            mmsStreamInf->bufReadPtr += mmsStreamInf->stream_buf_size;
         }

         mmsStreamInf->dmx_read_pos = seekPos;
         pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
    }
    else
    {
        CDX_LOGW("maybe the buffer is overwride by the new data, \
               so we can not support to seek to this position");
        pthread_mutex_lock(&mmsStreamInf->bufferMutex);
        mmsStreamInf->bufReadPtr += (seekPos - mmsStreamInf->dmx_read_pos);
        mmsStreamInf->validDataSize -= (seekPos - mmsStreamInf->dmx_read_pos);
        if(mmsStreamInf->bufReadPtr > mmsStreamInf->bufEndPtr)
        {
            mmsStreamInf->bufReadPtr -= mmsStreamInf->stream_buf_size;
        }
        else if(mmsStreamInf->bufReadPtr < mmsStreamInf->buffer)
        {
            mmsStreamInf->bufReadPtr += mmsStreamInf->stream_buf_size;
        }

        mmsStreamInf->dmx_read_pos = seekPos;
        pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
    }

    CdxAtomicSet(&mmsStreamInf->mState, MMS_STREAM_IDLE);
    CdxAtomicDec(&mmsStreamInf->ref);
    return 0;
}

int __MmsSeekToTime(CdxStreamT* stream, cdx_int64 timeUs)
{
    int64_t posOffset = 0;
    int ret = 0;

    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;
    aw_mmsh_ctrl_t* asfHttpCtrl = (aw_mmsh_ctrl_t*)mmsStreamInf->data;

    if(mmsStreamInf->mmsMode == PROTOCOL_MMS_T)
    {
        CDX_LOGW("--- mmst cannot seek");
        return -1;
    }
    if(mmsStreamInf->fileDuration <= 0)
    {
        return -1;
    }
    posOffset = (timeUs*mmsStreamInf->fileSize)/mmsStreamInf->fileDuration;
    if(posOffset>=mmsStreamInf->fileSize)
    {
        return -1;
    }
    else
    {
        pthread_mutex_lock(&mmsStreamInf->bufferMutex);

        mmsStreamInf->buf_pos = posOffset;
        mmsStreamInf->dmx_read_pos = posOffset;
        mmsStreamInf->bufWritePtr = mmsStreamInf->buffer;
        mmsStreamInf->bufReadPtr  = mmsStreamInf->buffer;
        mmsStreamInf->bufReleasePtr = mmsStreamInf->buffer;
        mmsStreamInf->validDataSize = 0;
        mmsStreamInf->bufferDataSize = 0;
        memset(mmsStreamInf->buffer, 0, MAX_STREAM_BUF_SIZE);
        if(mmsStreamInf->mmsMode == PROTOCOL_MMS_H)
        {
            if((asfHttpCtrl->streamingType==ASF_TYPE_PLAINTEXT)
            || (asfHttpCtrl->streamingType==ASF_TYPE_REDIRECTOR))
            {
                ret = CdxNopStreamingSeek(mmsStreamInf);
            }
            else
            {
                ret = CdxMmshStreamingSeek(mmsStreamInf);
            }
        }
        pthread_mutex_unlock(&mmsStreamInf->bufferMutex);

        if(ret < 0)
        {
            return -1;
        }
    }
    return 0;
}

cdx_int64 __MmsSize(CdxStreamT* stream)
{
    CDX_UNUSE(stream);
    //aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;
    //return mmsStreamInf->fileSize;
    return -1;
}

static cdx_int64 __MmsTell(CdxStreamT* stream)
{
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;
    pthread_mutex_lock(&mmsStreamInf->bufferMutex);
    int64_t ret = mmsStreamInf->dmx_read_pos;
    //CDX_LOGD("--- mms tell %lld", ret);
    pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
    return ret;
}

static int __MmsConnect(CdxStreamT* stream)
{
    aw_mms_inf_t* mmsStreamInf = (aw_mms_inf_t*)stream;
    int ret;

    ret = pthread_create(&mmsStreamInf->downloadTid, NULL, CdxReadAsfStream, (void*)mmsStreamInf);
    if(ret != 0)
    {
        CDX_LOGE("can not create parser send data thread.");
        mmsStreamInf->downloadTid = (pthread_t)0;
    }

    pthread_mutex_lock(&mmsStreamInf->lock);
    while(mmsStreamInf->iostate != CDX_IO_STATE_OK
        && mmsStreamInf->iostate != CDX_IO_STATE_EOS
        && mmsStreamInf->iostate != CDX_IO_STATE_ERROR)
    {
        pthread_cond_wait(&mmsStreamInf->cond, &mmsStreamInf->lock);
    }
    pthread_mutex_unlock(&mmsStreamInf->lock);

    return 0;
}

static struct CdxStreamOpsS MmsStreamOps =
{
    .connect        = __MmsConnect,
    .getProbeData     = __MmsGetProbeData,
    .read             = __MmsRead,
    .close             = __MmsClose,
    .getIOState         = __MmsGetIoState,
    .attribute         = __MmsAttribute,
    .control         = __MmsControl,
    .seek             = __MmsSeek,
    .seekToTime         = __MmsSeekToTime,
    .eos             = __MmsEos,
    .tell             = __MmsTell,
    .size             = __MmsSize,
};

CdxStreamT* __MmsStreamOpen(CdxDataSourceT* dataSource)
{
    CDX_CHECK(dataSource);
    if(dataSource->uri == NULL)
    {
        return NULL;
    }

    aw_mms_inf_t* mmsStreamInf = NULL;
    int ret = 0;

    mmsStreamInf = (aw_mms_inf_t*)malloc(sizeof(aw_mms_inf_t));
    if(NULL == mmsStreamInf)
    {
        CDX_LOGE("--- malloc error");
        return NULL;
    }
    memset(mmsStreamInf, 0, sizeof(aw_mms_inf_t));

    if(mmsStreamInf->buffer != NULL)
    {
        free(mmsStreamInf->buffer);
        mmsStreamInf->buffer = NULL;
    }

    mmsStreamInf->dmx_read_pos  = 0;
    mmsStreamInf->buf_pos       = 0;
    mmsStreamInf->fd = 0;    /* server socket fd*/
    mmsStreamInf->fileSize = 0;
    mmsStreamInf->fileDuration = 0;
    mmsStreamInf->iostate = CDX_IO_STATE_INVALID;
    mmsStreamInf->probeData.buf = malloc(MMS_PROBE_DATA_LEN);
    mmsStreamInf->probeData.len = MMS_PROBE_DATA_LEN;

    mmsStreamInf->stream_buf_size = MAX_STREAM_BUF_SIZE;
    mmsStreamInf->buffer = (char*)malloc(sizeof(char)*mmsStreamInf->stream_buf_size);
   /* cliemt buffer, parser read data from it*/
    if(mmsStreamInf->buffer == NULL)
    {
         goto mms_open_error;
    }

    mmsStreamInf->bufEndPtr = mmsStreamInf->buffer+mmsStreamInf->stream_buf_size-1;
    mmsStreamInf->bufWritePtr = mmsStreamInf->buffer;
    mmsStreamInf->bufReadPtr = mmsStreamInf->buffer;
    mmsStreamInf->bufReleasePtr = mmsStreamInf->buffer;
    mmsStreamInf->bufferDataSize = 0;
    mmsStreamInf->validDataSize = 0;

    CdxAtomicSet(&mmsStreamInf->ref, 1);
    CdxAtomicSet(&mmsStreamInf->mState, MMS_STREAM_OPENING);
    mmsStreamInf->uri = strdup(dataSource->uri);

    #ifdef SAVE_MMS
    mmsStreamInf->fp = fopen("/mnt/sdcard/mms_stream.s", "wb");
    #endif

    mmsStreamInf->asfChunkDataBuffer = (char*)malloc(32*1024);
    if(mmsStreamInf->asfChunkDataBuffer == NULL)
    {
        return NULL;
    }
    pthread_mutex_init(&mmsStreamInf->bufferMutex, NULL);

    ret = pthread_mutex_init(&mmsStreamInf->lock, NULL);
    CDX_CHECK(!ret);
    ret = pthread_cond_init(&mmsStreamInf->cond, NULL);
    CDX_CHECK(!ret);

    mmsStreamInf->eof = 0;
    if(mmsStreamInf->fd == 0)
    {
        mmsStreamInf->fd = 1;
    }

    mmsStreamInf->streaminfo.ops                = &MmsStreamOps;
    return &(mmsStreamInf->streaminfo);

 mms_open_error:

    if(mmsStreamInf != NULL)
    {
        CdxMmsClose(mmsStreamInf);
    }
    return NULL;
}

CdxStreamCreatorT mmsStreamCtor =
{
    .create = __MmsStreamOpen
};

