/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxUdpStream.c
 * Description : UdpStream
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxUdpStream"
#include "CdxUdpStream.h"
#include <cdx_log.h>
#include <CdxMemory.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>

#include <CdxSocketUtil.h>
#include <CdxTime.h>

#define ProbeDataLen (2*1024)
#define SizeToReadEverytime (64*1024)
#define closesocket close

#define SAVE_FILE    (0)

int UdpSocketCreate(CdxUdpStream *udpStream, CdxUrlT *url);
void *UdpDownloadThread(void* parameter);

#if SAVE_FILE
    FILE    *file = NULL;
#endif

CDX_INTERFACE int GetUdpBandwidthKbps(CdxUdpStream *udpStream, cdx_int32 *kbps)
{
    if(udpStream->mBWCount < 2 || !udpStream->mBWTotalTimeCost)
    {
        return -1;
    }
    *kbps = udpStream->mBWTotalSize*8*1000/udpStream->mBWTotalTimeCost;
    return 0;
}

static cdx_int32 GetCacheState(CdxUdpStream *udpStream, struct StreamCacheStateS *cacheState)
{
    cdx_int32 kbps;
    cacheState->nCacheCapacity = udpStream->bufSize;
    cacheState->nCacheSize = udpStream->validDataSize;

    if(!GetUdpBandwidthKbps(udpStream, &kbps))
    {
        cacheState->nBandwidthKbps = kbps;
    }
    else
    {
        CDX_LOGW("udpStream->mBWCount < 2 || !udpStream->mBWTotalTimeCost");
        cacheState->nBandwidthKbps = 200;
    }
    return 0;
}

void UdpTransferMeasurement(CdxUdpStream *udpStream, cdx_uint32 numBytes, cdx_int64 delayUs)
{
    if(udpStream->mBWWritePos >= BANDWIDTH_ARRAY_SIZE)
    {
        udpStream->mBWWritePos = 0;
    }
    BandwidthEntryT begin = udpStream->mBWArray[udpStream->mBWWritePos];

    udpStream->mBWArray[udpStream->mBWWritePos].downloadTimeCost = delayUs;
    udpStream->mBWArray[udpStream->mBWWritePos].downloadSize = numBytes;
    udpStream->mBWTotalSize += numBytes;
    udpStream->mBWTotalTimeCost += delayUs;
    if(++udpStream->mBWCount > BANDWIDTH_ARRAY_SIZE)
    {
        udpStream->mBWTotalSize -= begin.downloadSize;
        udpStream->mBWTotalTimeCost -= begin.downloadTimeCost;
        --udpStream->mBWCount;
    }
    udpStream->mBWWritePos++;
}

static CdxStreamProbeDataT *UdpStreamGetProbeData(CdxStreamT *stream)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    return &udpStream->probeData;
}

static cdx_int32 UdpStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    pthread_mutex_lock(&udpStream->lock);
    if(udpStream->forceStop)
    {
        pthread_mutex_unlock(&udpStream->lock);
        return -1;
    }
    udpStream->status = STREAM_READING;
    pthread_mutex_unlock(&udpStream->lock);

    cdx_uint8 *data = (cdx_uint8 *)buf;
    cdx_uint32 size = udpStream->bufSize - 2 * SizeToReadEverytime;
    if(len > size)
    {
        CDX_LOGW("UdpStreamRead len is too big!");
        len = size;
    }

    while(udpStream->validDataSize < len && !udpStream->downloadThreadExit)
    {
        if(udpStream->forceStop)
        {
            len = -1;
            goto _exit;
        }
        usleep(10*1000);
    }
    if(!udpStream->validDataSize)
    {
        udpStream->ioState = CDX_IO_STATE_EOS;
        len = -1;
        goto _exit;
    }
    if(udpStream->validDataSize < len)
    {
        len = udpStream->validDataSize;
    }

    pthread_mutex_lock(&udpStream->bufferMutex);
    if(udpStream->endPos > 0 && (size = udpStream->endPos - udpStream->readPos) < len)
    {
        memcpy(data, udpStream->bigBuf + udpStream->readPos, size);
        memcpy(data + size, udpStream->bigBuf, len - size);
        udpStream->readPos = len - size;
        udpStream->endPos = 0;
    }
    else
    {
        memcpy(data, udpStream->bigBuf + udpStream->readPos, len);
        udpStream->readPos += len;
        if(udpStream->endPos > 0 && udpStream->readPos >= udpStream->endPos)
        {
            udpStream->endPos = 0;
            udpStream->readPos = 0;
        }
    }
    udpStream->validDataSize -= len;
    pthread_mutex_unlock(&udpStream->bufferMutex);

_exit:

    pthread_mutex_lock(&udpStream->lock);
    udpStream->status = STREAM_IDLE;
    pthread_mutex_unlock(&udpStream->lock);
    pthread_cond_signal(&udpStream->cond);
    return len;
}

static cdx_int32 UdpStreamForceStop(CdxStreamT *stream)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    pthread_mutex_lock(&udpStream->lock);
    udpStream->forceStop = 1;
    pthread_cond_broadcast(&udpStream->cond);
    while(udpStream->status != STREAM_IDLE)
    {
        pthread_cond_wait(&udpStream->cond, &udpStream->lock);
    }
    pthread_mutex_unlock(&udpStream->lock);
    return 0;
}
static cdx_int32 UdpStreamConnect(CdxStreamT *stream)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    pthread_mutex_lock(&udpStream->lock);
    if(udpStream->forceStop)
    {
        pthread_mutex_unlock(&udpStream->lock);
        return -1;
    }
    udpStream->status = STREAM_CONNECTING;
    pthread_mutex_unlock(&udpStream->lock);

    udpStream->socketFd = UdpSocketCreate(udpStream, udpStream->url);
    if(udpStream->socketFd < 0)
    {
        CDX_LOGE("UdpSocketCreate fail!");
        udpStream->ioState = CDX_IO_STATE_ERROR;
        goto _exit;
    }
    int ret = pthread_create(&udpStream->threadId, NULL, UdpDownloadThread, (void *)udpStream);
    if(ret != 0)
    {
        CDX_LOGE("pthread_create fail, ret(%d)", ret);
        udpStream->ioState = CDX_IO_STATE_ERROR;
        goto _exit;
    }
    pthread_mutex_lock(&udpStream->lock);
    while(udpStream->ioState != CDX_IO_STATE_OK
        && udpStream->ioState != CDX_IO_STATE_EOS
        && udpStream->ioState != CDX_IO_STATE_ERROR
        && !udpStream->forceStop)
    {
        pthread_cond_wait(&udpStream->cond, &udpStream->lock);
    }
    pthread_mutex_unlock(&udpStream->lock);
_exit:
    pthread_mutex_lock(&udpStream->lock);
    udpStream->status = STREAM_IDLE;
    pthread_mutex_unlock(&udpStream->lock);
    pthread_cond_signal(&udpStream->cond);
    return udpStream->ioState == CDX_IO_STATE_ERROR ? -1 : 0;
}

static cdx_int32 UdpStreamClose(CdxStreamT *stream)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    udpStream->exitFlag = 1;
    UdpStreamForceStop(stream);
    /*不需要，因为close时，UdpStreamRead根本不会同时运行，close和UdpStreamRead是在同一线程的*/
    if(udpStream->threadId)
    {
        pthread_join(udpStream->threadId, NULL);
    }
    if(udpStream->bigBuf)
    {
        free(udpStream->bigBuf);
    }
    if(udpStream->probeData.buf)
    {
        free(udpStream->probeData.buf);
    }

    if(udpStream->url)
    {
        CdxUrlFree(udpStream->url);
    }

    if(udpStream->sourceUri)
    {
        free(udpStream->sourceUri);
    }

    if(udpStream->isMulticast)
    {
        setsockopt(udpStream->socketFd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
            &udpStream->multicast, sizeof(udpStream->multicast));
    }
    closesocket(udpStream->socketFd);
    pthread_mutex_destroy(&udpStream->bufferMutex);
    pthread_mutex_destroy(&udpStream->lock);
    pthread_cond_destroy(&udpStream->cond);
#if SAVE_FILE
    if (file)
    {
        fclose(file);
        file = NULL;
    }
#endif
    free(udpStream);
    return 0;
}

static cdx_int32 UdpStreamGetIOState(CdxStreamT *stream)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    return udpStream->ioState;
}

static cdx_uint32 UdpStreamAttribute(CdxStreamT *stream)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    return udpStream->attribute;
}

static cdx_int32 UdpStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    switch(cmd)
    {
        case STREAM_CMD_GET_CACHESTATE:
            return GetCacheState(udpStream, (struct StreamCacheStateS *)param);
        case STREAM_CMD_SET_FORCESTOP:
            return UdpStreamForceStop(stream);
        default:
            CDX_LOGW("not implement cmd(%d)", cmd);
            break;
    }
    return 0;
}

#if 0 // not use now
static cdx_int32 UdpStreamSeek(CdxStreamT *stream, cdx_int64 offset, cdx_int32 whence)
{
    CDX_UNUSE(stream);
    CDX_UNUSE(offset);
    CDX_UNUSE(whence);
    CDX_LOGI("not implement now...");
    return -1;
}
#endif

static cdx_int64 UdpStreamTell(CdxStreamT *stream)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    cdx_int64 tell;
    pthread_mutex_lock(&udpStream->bufferMutex);
    if(udpStream->endPos > 0)
    {
        tell = udpStream->accumulatedDownload - (udpStream->endPos - udpStream->readPos);
        tell -= udpStream->writePos;
    }
    else
    {
        tell = udpStream->accumulatedDownload - (udpStream->writePos - udpStream->readPos);
    }
    pthread_mutex_unlock(&udpStream->bufferMutex);
    return tell;
}

static cdx_int32 UdpStreamGetMetaData(CdxStreamT *stream, const cdx_char *key,
                                        void **pVal)
{
    CdxUdpStream *udpStream = (CdxUdpStream *)stream;
    if(strcmp(key, "uri") == 0)
    {
        *pVal = udpStream->sourceUri;
        return 0;
    }
    else
    {
        CDX_LOGD("key = %s, is not supported by UdpStreamGetMetaData.", key);
        return -1;
    }
}

static struct CdxStreamOpsS udpStreamOps =
{
    .connect = UdpStreamConnect,
    .getProbeData = UdpStreamGetProbeData,
    .read = UdpStreamRead,
    .write = NULL,
    .close = UdpStreamClose,
    .getIOState = UdpStreamGetIOState,
    .attribute = UdpStreamAttribute,
    .control = UdpStreamControl,
    .getMetaData = UdpStreamGetMetaData,
    .seek = NULL,
    .seekToTime = NULL,
    .eos = NULL,
    .tell = UdpStreamTell,
    .size = NULL,
    //.forceStop = UdpStreamForceStop
};
int UdpSocketCreate(CdxUdpStream *udpStream, CdxUrlT *url)
{
    int socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(socketFd == -1)
    {
        CDX_LOGE("create socket fail");
        return -1;
    }
    int ret = CdxSockSetBlocking(socketFd, 0);
    CDX_FORCE_CHECK(ret == 0);

    struct hostent *host = gethostbyname(url->hostname);
    if(!host)
    {
        CDX_LOGE("gethostbyname fail");
        goto _err;
    }

    struct sockaddr_in serverSockAddress;
    memset(&serverSockAddress, 0, sizeof(serverSockAddress));
    memcpy((cdx_uint8 *)&serverSockAddress.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
    serverSockAddress.sin_family = AF_INET;
    serverSockAddress.sin_port = htons(url->port);

    int reuse = 0;
    if(setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
    {
        CDX_LOGW("SO_REUSEADDR failed! ignore.");
    }

    /* Increase the socket rx buffer size to maximum -- this is UDP */
    int recvBufSize = 5*64*1024;
    if(setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, &recvBufSize, sizeof (recvBufSize)))
    {
        CDX_LOGW("SO_RCVBUF failed!");
    }
    if((ntohl(serverSockAddress.sin_addr.s_addr) >> 28) == 0xe)
    {
        struct ip_mreq *multicast = &udpStream->multicast;
        multicast->imr_multiaddr.s_addr = serverSockAddress.sin_addr.s_addr;
        multicast->imr_interface.s_addr = 0;//htonl(INADDR_ANY);

        if(setsockopt(socketFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, multicast, sizeof(*multicast)))
        {
            CDX_LOGE("IP_ADD_MEMBERSHIP fail, errno = %d, socketFd = %d, multicast = %x", errno,
                socketFd, (int)multicast->imr_multiaddr.s_addr);
            goto _err;
        }
        udpStream->isMulticast = 1;
    }

    if(udpStream->isMulticast)
    {
        if(bind(socketFd, (struct sockaddr *)&serverSockAddress, sizeof(serverSockAddress)) == -1)
        {
            CDX_LOGE("bind fail, errno = %d, Address = %x", errno,
                (int)serverSockAddress.sin_addr.s_addr);
            goto _err;
        }
    }
    else
    {
        if(connect(socketFd, (struct sockaddr *)&serverSockAddress,
            sizeof(serverSockAddress)) == -1)
        {
            CDX_LOGE("connect fail, errno = %d, Address = %x", errno,
                (int)serverSockAddress.sin_addr.s_addr);
            goto _err;
        }

        if(sendto(socketFd, "hello", 5, 0, (struct sockaddr *)&serverSockAddress,
            sizeof(serverSockAddress)) == -1)
        {
            CDX_LOGE("sendto fail, errno = %d, Address = %x", errno,
                (int)serverSockAddress.sin_addr.s_addr);
            goto _err;
        }
    }
    return socketFd;
_err:
    if(udpStream->isMulticast)
    {
        setsockopt(socketFd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &udpStream->multicast,
            sizeof(udpStream->multicast));
    }
    closesocket(socketFd);
    return -1;
}

/*downloadSize记录读到的数据量，return < 0出错，> 0正常，
可继续调用, ==0 socket is closed by peer?是否存在这种情况，未知*/
int UdpDownloadData(CdxUdpStream *udpStream, cdx_uint32 *downloadSize)
{
    int ret, endPos;
    *downloadSize = 0;
    pthread_mutex_lock(&udpStream->bufferMutex);
    while(1)
    {
        if(udpStream->exitFlag)
        {
            ret = -1;
            break;
        }
        if(udpStream->bufSize - udpStream->writePos < SizeToReadEverytime)
        {
            udpStream->endPos = udpStream->writePos;
            udpStream->writePos = 0;
        }

        if(udpStream->endPos > 0)
        {
            endPos = udpStream->endPos;
        }
        else
        {
            endPos = udpStream->bufSize;
        }

        if(endPos - udpStream->validDataSize < SizeToReadEverytime)
        {
            CDX_LOGW("Maybe Not all data is read.");
            ret = 1;
            break;
        }
        ret = recv(udpStream->socketFd, udpStream->bigBuf + udpStream->writePos,
            SizeToReadEverytime, 0);

        if(ret < 0)
        {
            if (EAGAIN == errno)
            {
                ret = 1;
                break;
            }
            else
            {
                CDX_LOGE("recv err(%d)", errno);
                break;
            }
        }
        else if(ret == 0)
        {
            CDX_LOGW("xxx errno(%d), socket is closed by peer?", errno);
            break;
        }
#if SAVE_FILE
        if (file)
        {
            fwrite(udpStream->bigBuf + udpStream->writePos, 1, ret, file);
            sync();
        }
        else
        {
            CDX_LOGW("save file = NULL");
        }
#endif
        udpStream->validDataSize += ret;
        udpStream->writePos += ret;
        udpStream->accumulatedDownload += ret;
        *downloadSize += ret;
    }
    pthread_mutex_unlock(&udpStream->bufferMutex);
    return ret;
}
void *UdpDownloadThread(void* parameter)
{
    CDX_LOGI("UdpDownloadThread start");
    CdxUdpStream *udpStream = (CdxUdpStream *)parameter;

    int ret, probe = 1;
    cdx_uint32 downloadSize;
    fd_set rs;
    fd_set errs;
    struct timeval tv;

    cdx_int64 startTime = 0;
    cdx_int64 endTime = 0;
    cdx_int64 totTime = 0;

    while(1)
    {
        if(udpStream->exitFlag)
        {
            break;
        }

        startTime = CdxGetNowUs();
        if(udpStream->bufSize - udpStream->validDataSize <= 2*SizeToReadEverytime)
        {
            usleep(50*1000);
            endTime = CdxGetNowUs();
            totTime = endTime - startTime;
            UdpTransferMeasurement(udpStream, 0, totTime);
            continue;
        }

        FD_ZERO(&rs);
        FD_SET(udpStream->socketFd, &rs);
        FD_ZERO(&errs);
        FD_SET(udpStream->socketFd, &errs);
        tv.tv_sec = 0;
        tv.tv_usec = 50*1000;
        ret = select(udpStream->socketFd + 1, &rs, NULL, &errs, &tv);
        if (ret < 0)
        {
            if (EINTR == errno)
            {
                endTime = CdxGetNowUs();
                totTime = endTime - startTime;
                UdpTransferMeasurement(udpStream, 0, totTime);
                continue;
            }
            CDX_LOGE("select err(%d)", errno);
            break;
        }
        else if (ret == 0)
        {
            endTime = CdxGetNowUs();
            totTime = endTime - startTime;
            UdpTransferMeasurement(udpStream, 0, totTime);
            continue;
        }

        if(FD_ISSET(udpStream->socketFd, &errs))
        {
            CDX_LOGE("select errs");
            break;
        }
        if(!FD_ISSET(udpStream->socketFd, &rs))
        {
            CDX_LOGE("select > 0, but sockfd is not ready?");
            break;
        }
        CDX_LOGV("1UdpDownloadData");
        ret = UdpDownloadData(udpStream, &downloadSize);

        if(ret < 0)
        {
            break;
        }

        endTime = CdxGetNowUs();
        totTime = endTime - startTime;
        UdpTransferMeasurement(udpStream, downloadSize, totTime);
        CDX_LOGV("2UdpDownloadData, downloadSize = %u", downloadSize);
        if(probe && udpStream->validDataSize >= ProbeDataLen)
        {
            memcpy(udpStream->probeData.buf, udpStream->bigBuf, ProbeDataLen);
            pthread_mutex_lock(&udpStream->lock);
            udpStream->ioState = CDX_IO_STATE_OK;
            pthread_mutex_unlock(&udpStream->lock);
            pthread_cond_signal(&udpStream->cond);
            probe = 0;
        }
    }
    udpStream->downloadThreadExit = 1;
    pthread_mutex_lock(&udpStream->lock);
    udpStream->ioState = CDX_IO_STATE_ERROR;
    pthread_mutex_unlock(&udpStream->lock);
    pthread_cond_signal(&udpStream->cond);
    CDX_LOGI("UdpDownloadThread end");
    return NULL;
}

CdxStreamT *UdpStreamCreate(CdxDataSourceT *dataSource)
{
    CdxUdpStream *udpStream = CdxMalloc(sizeof(CdxUdpStream));
    if(!udpStream)
    {
        CDX_LOGE("malloc fail!");
        return NULL;
    }
    memset(udpStream, 0x00, sizeof(CdxUdpStream));

    udpStream->sourceUri = strdup(dataSource->uri);
    if(!udpStream->sourceUri)
    {
        CDX_LOGE("strdup failed.");
        goto _error;
    }
    udpStream->url = CdxUrlNew(dataSource->uri);
    if(!udpStream->url)
    {
        CDX_LOGE("CdxUrlNew failed.");
        goto _error;
    }
    CdxUrlPrintf(udpStream->url);

    udpStream->bufSize = 20*1024*1024;
    udpStream->bigBuf = CdxMalloc(udpStream->bufSize);
    if(!udpStream->bigBuf)
    {
        CDX_LOGE("malloc fail!");
        goto _error;
    }

    udpStream->probeData.len = ProbeDataLen;
    udpStream->probeData.buf = CdxMalloc(ProbeDataLen);
    if(!udpStream->probeData.buf)
    {
        CDX_LOGE("malloc fail!");
        goto _error;
    }

    int ret = pthread_mutex_init(&udpStream->bufferMutex, NULL);
    CDX_CHECK(!ret);
    ret = pthread_mutex_init(&udpStream->lock, NULL);
    CDX_CHECK(!ret);
    ret = pthread_cond_init(&udpStream->cond, NULL);
    CDX_CHECK(!ret);

#if SAVE_FILE
    file = fopen("/data/camera/save.dat", "wb+");
    if (!file)
    {
        CDX_LOGE("open file failure errno(%d)", errno);
    }
#endif
    udpStream->ioState = CDX_IO_STATE_INVALID;
    udpStream->attribute = CDX_STREAM_FLAG_NET;
    udpStream->status = STREAM_IDLE;
    udpStream->base.ops = &udpStreamOps;
    return &udpStream->base;
_error:
    if(udpStream->probeData.buf)
    {
        free(udpStream->probeData.buf);
    }
    if(udpStream->bigBuf)
    {
        free(udpStream->bigBuf);
    }
    if(udpStream->url)
    {
        CdxUrlFree(udpStream->url);
    }
    if(udpStream->sourceUri)
    {
        free(udpStream->sourceUri);
    }
    free(udpStream);
    return NULL;
}

CdxStreamCreatorT udpStreamCtor =
{
    .create = UdpStreamCreate
};
