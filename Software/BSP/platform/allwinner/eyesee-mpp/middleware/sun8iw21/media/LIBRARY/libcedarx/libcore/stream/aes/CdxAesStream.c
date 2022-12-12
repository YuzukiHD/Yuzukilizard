/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAesStream.c
 * Description : AesStream
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxAesStream"
#include "CdxAesStream.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#include <errno.h>
#include <stdlib.h>

#define SAVE_FILE    (0)

#if SAVE_FILE
static int  start_tag = 0x01000010;
static int  index_aes = 0x00000000;
static FILE *file = NULL;
static void DebugStoreFile(void *buf, int len)
{
    CDX_LOGD("aes stream DebugStoreFile %d",len);
    if (file == NULL)
    {
        CDX_LOGD("open file");
        file = fopen("/data/camera/file.aes", "wb");
        if (file == NULL)
            CDX_LOGD("open file failed");
    }
    fseek(file, 0, SEEK_END);
    fwrite(&start_tag, 1, 4, file);
    fwrite(&index_aes, 1, 4, file);
    fwrite(buf, 1, len, file);

    return;
}

static void DebugSync(void)
{
    CDX_LOGD("DebugSync");
    if (file != NULL)
    {
        fclose(file);
        file = NULL;
        sync();
    }
}

#endif

static inline CdxStreamProbeDataT *AesStreamGetProbeData(CdxStreamT *stream)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    return &aesStream->probeData;
}

int AesStreamDecrypt(CdxAesStream *aesStream, cdx_uint8 *in, cdx_uint8 *out,
               cdx_uint32 size, int needRemove)
{
    CDX_CHECK(size);
    if((aesStream->downloadComplete || CdxStreamEos(aesStream->mediaFile))
      && !size%16 && needRemove)
    {
        int size1 = size - 16;
        if(size1)
        {
            AES_cbc_encrypt(in, out, size1, &aesStream->aesKey, aesStream->iv, AES_DECRYPT);
        }
        cdx_uint8 tmp[16] = {0};
        AES_cbc_encrypt(in + size1, tmp, 16, &aesStream->aesKey, aesStream->iv, AES_DECRYPT);

        CDX_CHECK(aesStream->paddingType == CDX_PKCS7);

        int removeSize = tmp[15];
        CDX_CHECK(removeSize > 0 && removeSize <= 16);
        if(removeSize != 16)
        {
            memcpy(out + size1, tmp, 16 - removeSize);
        }
        return removeSize;
    }
    else
    {
        AES_cbc_encrypt(in, out, size, &aesStream->aesKey, aesStream->iv, AES_DECRYPT);
        return 0;
    }
}

static cdx_int32 AesStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 size)
{
    CDX_CHECK(stream && buf && size);
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    cdx_uint8 *data = (cdx_uint8 *)buf;
    cdx_uint32 len = size;
    int offset = 0, ret, removeSize = 0;

    if(aesStream->downloadComplete && !aesStream->remainingSize)
    {
        aesStream->ioState = CDX_IO_STATE_EOS;
    }
    if(aesStream->ioState == CDX_IO_STATE_EOS)
    {
        CDX_LOGD("aesStream eos");
        return 0;
    }
    pthread_mutex_lock(&aesStream->lock);
    if(aesStream->forceStop)
    {
        CDX_LOGD("forceStop");
        pthread_mutex_unlock(&aesStream->lock);
        return -1;
    }
    aesStream->status = STREAM_READING;
    pthread_mutex_unlock(&aesStream->lock);

    if(aesStream->remainingSize)
    {
        offset = len > aesStream->remainingSize ? aesStream->remainingSize : len;
        memcpy(data, aesStream->remaining + aesStream->remainingOffset, offset);
        aesStream->remainingOffset += offset;
        //memcpy(data, aesStream->remaining + (16 - aesStream->remainingSize), offset);
        len -= offset;
        aesStream->remainingSize -= offset;
    }

    cdx_uint32 aesReadLen = (len + 15) / 16 * 16;
    while(aesReadLen >= 16)
    {
        if(aesStream->forceStop)
        {
            CDX_LOGD("aesStream->forceStop");
            ret = -1;
            goto _exit;
        }
        if(aesStream->downloadComplete)
        {
            CDX_LOGD("downloadComplete");
            break;
        }
        cdx_uint32 readLen = aesReadLen > aesStream->bufSize ? aesStream->bufSize : aesReadLen;
        ret = CdxStreamRead(aesStream->mediaFile, aesStream->bigBuf, readLen);
        if (ret < (int)readLen)
        {
            CDX_LOGW("CdxStreamRead fail, readLen(%d), ret(%d)", readLen, ret);
            if(ret < 0)
            {
                goto _exit;
            }
            else if(ret == 0)
            {
                aesStream->downloadComplete = 1;
                break;
            }
            else
            {
                aesStream->downloadComplete = 1;
                readLen = ret;//readLen = ret / 16 * 16;//readLen = ret;
            }
        }

        if(len >= readLen)
        {
            removeSize = AesStreamDecrypt(aesStream, aesStream->bigBuf, data + offset, readLen, 1);

            aesReadLen -= readLen;
            readLen -= removeSize;
            offset += readLen;
            len -= readLen;
        }
        else
        {
        //此时len2一定非0，否则len不会小于readLen
            int len2 = len % 16;
            int len1 = len - len2;
            if(len1)
            {
                ret = AesStreamDecrypt(aesStream, aesStream->bigBuf, data + offset, len1, 0);
                offset += len1;
                len -= len1;
            }

            //removeSize = AesStreamDecrypt(aesStream, aesStream->bigBuf + len1,
            //aesStream->remaining + 16 - (readLen - len1), readLen - len1, 1);
            removeSize = AesStreamDecrypt(aesStream, aesStream->bigBuf + len1,
                                   aesStream->remaining, readLen - len1, 1);
            //aesStream->remainingOffset = 0;

            int remainingSize = readLen - len1 - removeSize;
            if(len2 > remainingSize)
            {
                len2 = remainingSize;
            }
            memcpy(data + offset, aesStream->remaining, len2);
            aesStream->remainingOffset = len2;
            aesStream->remainingSize = remainingSize - len2;
            offset += len2;
            len -= len2;
            aesReadLen -= readLen;
            //CDX_LOGD("aesReadLen(%)", aesReadLen);
        }
        if(readLen >= 16)
        {
            memcpy(aesStream->iv, aesStream->bigBuf + readLen - 16, 16);
        }
    }

    pthread_mutex_lock(&aesStream->lock);
    aesStream->status = STREAM_IDLE;
    pthread_mutex_unlock(&aesStream->lock);
    pthread_cond_signal(&aesStream->cond);
#if SAVE_FILE
    DebugStoreFile(buf,offset);
#endif
    return offset;
_exit:
    aesStream->ioState = CDX_IO_STATE_ERROR;
    pthread_mutex_lock(&aesStream->lock);
    aesStream->status = STREAM_IDLE;
    pthread_mutex_unlock(&aesStream->lock);
    pthread_cond_signal(&aesStream->cond);
    return ret;
}

static cdx_int32 AesStreamForceStop(CdxStreamT *stream)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    pthread_mutex_lock(&aesStream->lock);
    aesStream->forceStop = 1;
    int ret = CdxStreamForceStop(aesStream->mediaFile);
    if(ret < 0)
    {
        CDX_LOGW("CdxStreamForceStop fail");
    }
    while(aesStream->status != STREAM_IDLE)
    {
        pthread_cond_wait(&aesStream->cond, &aesStream->lock);
    }
    pthread_mutex_unlock(&aesStream->lock);
    return 0;
}
static cdx_int32 AesStreamClrForceStop(CdxStreamT *stream)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    if(aesStream->status != STREAM_IDLE)
    {
        CDX_LOGW("aesStream->status != STREAM_IDLE");
        aesStream->ioState = CDX_IO_STATE_INVALID;
        return -1;
    }
    CdxStreamClrForceStop(aesStream->mediaFile);
    aesStream->forceStop = 0;
    return 0;
}

static cdx_int32 AesStreamClose(CdxStreamT *stream)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    AesStreamForceStop(stream);

    if(aesStream->mediaFile)
    {
        CdxStreamClose(aesStream->mediaFile);
    }

    if(aesStream->bigBuf)
    {
        free(aesStream->bigBuf);
    }
    if(aesStream->probeData.buf)
    {
        free(aesStream->probeData.buf);
    }

    if(aesStream->mediaUri)
    {
        free(aesStream->mediaUri);
    }
    pthread_mutex_destroy(&aesStream->lock);
    pthread_cond_destroy(&aesStream->cond);

#if SAVE_FILE
    DebugSync();
#endif
    free(aesStream);
    return 0;
}

static inline cdx_int32 AesStreamGetIOState(CdxStreamT *stream)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    return aesStream->ioState;
}

static inline cdx_uint32 AesStreamAttribute(CdxStreamT *stream)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    return CdxStreamAttribute(aesStream->mediaFile);
}

static cdx_int32 AesStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    switch(cmd)
    {
        case STREAM_CMD_SET_FORCESTOP:
            return AesStreamForceStop(stream);
        case STREAM_CMD_CLR_FORCESTOP:
            return AesStreamClrForceStop(stream);
        default:
            return CdxStreamControl(aesStream->mediaFile, cmd, param);
    }
}

static cdx_int32 AesStreamSeek(CdxStreamT *stream, cdx_int64 offset, cdx_int32 whence)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    pthread_mutex_lock(&aesStream->lock);
    if(aesStream->forceStop)
    {
        CDX_LOGD("forceStop");
        pthread_mutex_unlock(&aesStream->lock);
        return -1;
    }
    aesStream->status = STREAM_SEEKING;
    pthread_mutex_unlock(&aesStream->lock);
    aesStream->ioState = CDX_IO_STATE_OK;

    if(whence == STREAM_SEEK_END)
    {
        offset += aesStream->mediaFileSize;
    }
    else if(whence == STREAM_SEEK_CUR)
    {
        offset += CdxStreamTell(aesStream->mediaFile);
    }
    int remainder = offset % 16;
    int offset1 = offset - remainder - 16;
    aesStream->remainingSize=0;
    aesStream->remainingOffset=0;
    if(offset1 < 0)
    {
        offset1 = 0;
    }
    int ret = CdxStreamSeek(aesStream->mediaFile, offset1, STREAM_SEEK_SET);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamSeek fail");
        goto _exit;
    }
    if(offset < 16)
    {
        memcpy(aesStream->iv, aesStream->originalIv, 16);
    }
    else
    {
        ret = CdxStreamRead(aesStream->mediaFile, aesStream->iv, 16);
        if (ret < 16)
        {
            CDX_LOGW("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                goto _exit;
            }
            else
            {
                CDX_LOGE("should not be here, this is a bug !");/*说明offset这个入参有问题*/
                aesStream->downloadComplete = 1;
                goto _exit;
            }
        }
    }
    if(remainder)
    {
        ret = CdxStreamRead(aesStream->mediaFile, aesStream->bigBuf, 16);
        if (ret < 16)
        {
            CDX_LOGW("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                goto _exit;
            }
            else if(ret < remainder)/*说明offset这个入参有问题*/
            {
                CDX_LOGE("should not be here, this is a bug !");
                aesStream->downloadComplete = 1;
                goto _exit;
            }
            else
            {
                aesStream->downloadComplete = 1;
            }
        }
        int removeSize = AesStreamDecrypt(aesStream, aesStream->bigBuf,
                                aesStream->remaining, ret, 1);

        int remainingSize = ret - removeSize;
        if(remainder >= remainingSize)
        {
            aesStream->remainingSize = 0;
            aesStream->remainingOffset = 0;
        }
        else
        {
            aesStream->remainingSize = remainingSize - remainder;
            aesStream->remainingOffset = remainder;

        }
    }
    pthread_mutex_lock(&aesStream->lock);
    aesStream->status = STREAM_IDLE;
    pthread_mutex_unlock(&aesStream->lock);
    pthread_cond_signal(&aesStream->cond);
    return 0;
_exit:
    aesStream->ioState = CDX_IO_STATE_ERROR;
    pthread_mutex_lock(&aesStream->lock);
    aesStream->status = STREAM_IDLE;
    pthread_mutex_unlock(&aesStream->lock);
    pthread_cond_signal(&aesStream->cond);
    return -1;
}

static inline cdx_int64 AesStreamTell(CdxStreamT *stream)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    return CdxStreamTell(aesStream->mediaFile) - aesStream->remainingSize;
}

static inline cdx_int32 AesStreamGetMetaData(CdxStreamT *stream, const cdx_char *key,
                                        void **pVal)
{
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    return CdxStreamGetMetaData(aesStream->mediaFile, key, pVal);
}
static cdx_int32 AesStreamConnect(CdxStreamT *stream)
{
    CDX_LOGI("AesStreamConnect start");
    CdxAesStream *aesStream = (CdxAesStream *)stream;
    pthread_mutex_lock(&aesStream->lock);
    if(aesStream->forceStop)
    {
        pthread_mutex_unlock(&aesStream->lock);
        return -1;
    }
    aesStream->status = STREAM_CONNECTING;
    pthread_mutex_unlock(&aesStream->lock);
    int ret = CdxStreamConnect(aesStream->mediaFile);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamConnect fail, uri(%s)", aesStream->dataSource.uri);
        goto _exit;
    }
    CdxStreamProbeDataT *probeData = CdxStreamGetProbeData(aesStream->mediaFile);
    aesStream->probeData.len = probeData->len;
    aesStream->probeData.buf = CdxMalloc(probeData->len);
    if(!aesStream->probeData.buf)
    {
        CDX_LOGE("malloc fail!");
        goto _exit;
    }
    aesStream->mediaFileSize = CdxStreamSize(aesStream->mediaFile);
    memcpy(aesStream->iv, aesStream->originalIv, 16);
    AesStreamDecrypt(aesStream, (cdx_uint8 *)probeData->buf,
               (cdx_uint8 *)aesStream->probeData.buf, probeData->len, 0);
    memcpy(aesStream->iv, aesStream->originalIv, 16);
    aesStream->ioState = CDX_IO_STATE_OK;
    pthread_mutex_lock(&aesStream->lock);
    aesStream->status = STREAM_IDLE;
    pthread_mutex_unlock(&aesStream->lock);
    pthread_cond_signal(&aesStream->cond);
    CDX_LOGI("AesOpenThread succeed");
    return 0;

_exit:
    aesStream->ioState = CDX_IO_STATE_ERROR;
    pthread_mutex_lock(&aesStream->lock);
    aesStream->status = STREAM_IDLE;
    pthread_mutex_unlock(&aesStream->lock);
    pthread_cond_signal(&aesStream->cond);
    CDX_LOGI("AesOpenThread fail");
    return -1;
}

static struct CdxStreamOpsS aesStreamOps =
{
    .connect = AesStreamConnect,
    .getProbeData = AesStreamGetProbeData,
    .read = AesStreamRead,
    .write = NULL,
    .close = AesStreamClose,
    .getIOState = AesStreamGetIOState,
    .attribute = AesStreamAttribute,
    .control = AesStreamControl,
    .getMetaData = AesStreamGetMetaData,
    .seek = AesStreamSeek,
    .seekToTime = NULL,
    .eos = NULL,
    .tell = AesStreamTell,
    //.size = AesStreamSize,
    //.forceStop = UdpStreamForceStop
};

CdxStreamT *AesStreamCreate(CdxDataSourceT *dataSource)
{
    CdxAesStream *aesStream = CdxMalloc(sizeof(CdxAesStream));
    if(!aesStream)
    {
        CDX_LOGE("malloc fail!");
        return NULL;
    }
    memset(aesStream, 0x00, sizeof(CdxAesStream));

    int ret = pthread_mutex_init(&aesStream->lock, NULL);
    CDX_CHECK(!ret);
    ret = pthread_cond_init(&aesStream->cond, NULL);
    CDX_CHECK(!ret);

    aesStream->mediaUri = strdup(dataSource->uri + 6);
    if(!aesStream->mediaUri)
    {
        CDX_LOGE("strdup failed.");
        goto _error;
    }
    CDX_LOGD("aesStream uri is %s",aesStream->mediaUri);

    if (strncmp("file://",aesStream->mediaUri,7)== 0)
    {
        getAes128Key(aesStream->key);
        getAes128Iv(aesStream->originalIv);
        aesStream->paddingType = CDX_PKCS7;
        aesStream->dataSource.extraData = NULL;
        aesStream->dataSource.extraDataType = EXTRA_DATA_UNKNOWN;

    }else
    {
        if(!dataSource->extraData
            || dataSource->extraDataType != EXTRA_DATA_AES)
        {
            CDX_LOGE("dataSource error");
            goto _error;
        }

        AesStreamExtraDataT *extraData = (AesStreamExtraDataT *)dataSource->extraData;

        //need to set key for diffrent stream
        memcpy(aesStream->originalIv, extraData->iv, 16);
        memcpy(aesStream->key, extraData->key, 16);
        aesStream->paddingType = extraData->paddingType;
        aesStream->dataSource.extraData = extraData->extraData;
        aesStream->dataSource.extraDataType = extraData->extraDataType;
    }

    aesStream->dataSource.uri = aesStream->mediaUri;
    if (AES_set_decrypt_key(aesStream->key, 128, &aesStream->aesKey) != 0)
    {
        CDX_LOGE("AES_set_decrypt_key fail!");
        goto _error;
    }
    CDX_LOGD("dataSource.uri is %s",aesStream->dataSource.uri);
    aesStream->mediaFile = CdxStreamCreate(&aesStream->dataSource);
    if(!aesStream->mediaFile)
    {
        CDX_LOGE("CdxStreamCreate fail. (%s)", aesStream->dataSource.uri);
        goto _error;
    }

    aesStream->bufSize = 1024*1024;
    aesStream->bigBuf = CdxMalloc(aesStream->bufSize);
    if(!aesStream->bigBuf)
    {
        CDX_LOGE("malloc fail!");
        goto _error;
    }

    aesStream->ioState = CDX_IO_STATE_INVALID;
    aesStream->status = STREAM_IDLE;
    aesStream->base.ops = &aesStreamOps;
    return &aesStream->base;
_error:
    if(aesStream->bigBuf)
    {
        free(aesStream->bigBuf);
    }
    if(aesStream->mediaUri)
    {
        free(aesStream->mediaUri);
    }
    pthread_mutex_destroy(&aesStream->lock);
    pthread_cond_destroy(&aesStream->cond);
    free(aesStream);
    return NULL;
}

CdxStreamCreatorT aesStreamCtor =
{
    .create = AesStreamCreate
};
