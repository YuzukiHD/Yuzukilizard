/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxSSLStream.c
 * Description : SSLStream
 * History :
 *
 */

#define LOG_NDEBUG 0
#define LOG_TAG "sslStream"

#include <stdlib.h>
#include <CdxTypes.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <CdxSocketUtil.h>
#include <netdb.h>

#include <CdxStream.h>
#include <CdxAtomic.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <SmartDnsService.h>

#define SOCKRECVBUF_LEN 512*1024// 262142 (5*1024*1024)
#define closesocket close

typedef struct CdxSSLStreamImpl
{
    CdxStreamT base;
    cdx_int32 ioState;
    cdx_int32 sockRecvBufLen;
    cdx_int32 notBlockFlag;
    cdx_int32 exitFlag;                  //when close, exit
    cdx_int32 forceStopFlag;
    cdx_int32 sockFd;                    //socket fd
    //int eof;                           //all stream data is read from network
    cdx_int32 port;
    cdx_char *hostname;
    cdx_atomic_t ref;                    //reference count, for free resource while still blocking.
    cdx_atomic_t state;
    pthread_mutex_t stateLock;
    pthread_cond_t  stateCond;

    SSL *ssl;
    SSL_CTX *ctx;

    pthread_cond_t dnsCond;
    pthread_mutex_t* dnsMutex;
    int dnsRet;
    struct addrinfo *dnsAI;

    //add more
}CdxSSLStreamImplT;

static void CdxSSLStreamDecRef(CdxStreamT *stream);

typedef struct CdxHttpSendBuffer
{
    void *size;
    void *buf;
}CdxHttpSendBufferT;

enum HttpStreamStateE
{
    SSL_STREAM_IDLE    = 0x00L,
    SSL_STREAM_CONNECTING = 0x01L,
    SSL_STREAM_READING = 0x02L,
    SSL_STREAM_WRITING = 0x03L,
    SSL_STREAM_FORCESTOPPED = 0x04L,
    //TCP_STREAM_CLOSING
};
#define SSL_FREE(impl) do{      \
    if(impl->ssl)               \
    {                           \
        SSL_free(impl->ssl);    \
        impl->ssl = NULL;       \
    }                           \
    if(impl->ctx)               \
    {                           \
        SSL_CTX_free(impl->ctx);\
        impl->ctx = NULL;       \
    }                           \
}while(0)

static int64_t GetNowUs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_sec * 1000000ll + tv.tv_usec;
}

cdx_int32  CdxSSLConnect(cdx_int32 sockfd, SSL *ssl,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
{
    cdx_int32 ret, ioErr;
    fd_set ws;
    struct timeval tv;
    cdx_long loopTimes = 0, i = 0;

    //blocking
    if (CdxSockIsBlocking(sockfd))
    {
        while(1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop.", __FUNCTION__, __LINE__);
                return -2;
            }

            ret = SSL_connect(ssl);
            if (ret > 0)
            {
                //success
                return 0;
            }
            else if(ret == 0)
                /*The TLS/SSL handshake was not successful but was shut down
                  controlled and by the specifications of the TLS/SSL protocol.
                  Call SSL_get_error() with the return value ret to find out the reason.*/
            {
                CDX_LOGE("xxx ret(%d), %s", ret, ERR_error_string(ERR_get_error(), NULL));
                return -1;
            }
            else
            {
                ret = SSL_get_error(ssl, ret);
                if(ret == SSL_ERROR_WANT_WRITE)
                {
                    usleep(20000);
                    continue;
                }
                else
                {
                    CDX_LOGE("write error, %s", ERR_error_string(ERR_get_error(), NULL));
                    return -1;
                }
            }
        }
    }

    //non-blocking
    if (timeoutUs == 0)
    {
        loopTimes = ((cdx_ulong)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }

    while(1)
    {
        ret = SSL_connect(ssl);
        if (ret > 0)
        {
            //success
            return 0;
        }
        else if(ret == 0)
            /*The TLS/SSL handshake was not successful but was shut down
              controlled and by the specifications of the TLS/SSL protocol.
              Call SSL_get_error() with the return value ret to find out the reason.*/
        {
            CDX_LOGE("xxx ret(%d), %s", ret, ERR_error_string(ERR_get_error(), NULL));
            return -1;
        }
        else
        {
            ret = SSL_get_error(ssl, ret);
            if(ret == SSL_ERROR_WANT_WRITE)
            {
                for (i = 0; i < loopTimes; i++)
                {
                    if (pForceStop && *pForceStop)
                    {
                        CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
                        return -2;
                    }
                    FD_ZERO(&ws);
                    FD_SET(sockfd, &ws);
                    tv.tv_sec = 0;
                    tv.tv_usec = CDX_SELECT_TIMEOUT;
                    ret = select(sockfd + 1, NULL, &ws, NULL, &tv);
                    if (ret > 0)
                    {
                        break;//return 0;
                    }
                    else if (ret == 0)
                    {
                        continue;
                    }
                    else
                    {
                        ioErr = errno;
                        if (EINTR == ioErr)
                        {
                            continue;
                        }
                        CDX_LOGE("<%s,%d>select err(%d)", __FUNCTION__, __LINE__, errno);
                        return -1;
                    }
                }
            }
            else
            {
                CDX_LOGE("xxx ret(%d), %s", ret, ERR_error_string(ERR_get_error(), NULL));
                return -1;
            }
        }
    }

    return -1;
}
cdx_ssize CdxSSLNoblockRecv(SSL *ssl, void *buf, cdx_size len)
{
    return SSL_read(ssl, buf, len);
}
cdx_ssize CdxSSLRecv(cdx_int32 sockfd, SSL *ssl, void *buf, cdx_size len,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
{
    fd_set rs;
    fd_set errs;
    struct timeval tv;
    cdx_ssize ret = 0, recvSize = 0;
    cdx_long loopTimes = 0, i = 0;
    cdx_int32 ioErr;

    //blocking read
    if (CdxSockIsBlocking(sockfd))
    {
        while(1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop.recvSize(%ld)", __FUNCTION__, __LINE__, recvSize);
                return recvSize>0 ? recvSize : -2;
            }
            ret = SSL_read(ssl, ((char *)buf) + recvSize, len - recvSize);
            if (ret < 0)
            {
                ret = SSL_get_error(ssl, ret);
                if(ret == SSL_ERROR_WANT_READ)
                {
                    usleep(20000);
                    continue;
                }
                else
                {
                    CDX_LOGE("read error, %s", ERR_error_string(ERR_get_error(), NULL));
                    return -1;
                }
            }
            else if(ret == 0)
            {
                CDX_LOGD("xxx recvSize(%ld),sockfd(%d), want to read(%lu), errno(%d),"
                    " shut down by peer?", recvSize, sockfd, len, errno);
                return recvSize;
            }
            else
            {
                recvSize += ret;
                if ((cdx_size)recvSize == len)
                {
                    return recvSize;
                }
            }
        }
    }

    // non-blocking read
    if (timeoutUs == 0)
    {
        loopTimes = ((unsigned long)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }

    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
            return recvSize>0 ? recvSize : -2;
        }

        FD_ZERO(&rs);
        FD_SET(sockfd, &rs);
        FD_ZERO(&errs);
        FD_SET(sockfd, &errs);
        tv.tv_sec = 0;
        tv.tv_usec = CDX_SELECT_TIMEOUT;
        ret = select(sockfd + 1, &rs, NULL, &errs, &tv);
        if (ret < 0)
        {
            ioErr = errno;
            if (EINTR == ioErr)
            {
                continue;
            }
            CDX_LOGE("<%s,%d>select err(%d)", __FUNCTION__, __LINE__, ioErr);
            return -1;
        }
        else if (ret == 0)
        {
            //("timeout\n");
            //CDX_LOGV("xxx timeout, select again...");
            continue;
        }

        while (1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop.recvSize(%ld)", __FUNCTION__, __LINE__, recvSize);
                return recvSize>0 ? recvSize : -2;
            }
            if(FD_ISSET(sockfd,&errs))
            {
                CDX_LOGE("<%s,%d>errs ", __FUNCTION__, __LINE__);
                break;
            }
            if(!FD_ISSET(sockfd, &rs))
            {
                CDX_LOGV("select > 0, but sockfd is not ready?");
                break;
            }

            ret = SSL_read(ssl, ((char *)buf) + recvSize, len - recvSize);
            if (ret < 0)
            {
                ret = SSL_get_error(ssl, ret);
                if(ret == SSL_ERROR_WANT_READ)
                {
                    break;
                }
                else
                {
                    CDX_LOGE("ret(%ld), read error, %s", ret,
                        ERR_error_string(ERR_get_error(), NULL));
                    ERR_print_errors_fp(stderr);
                    return -1;
                }
            }
            else if (ret == 0)//socket is close by peer?
            {
                CDX_LOGD("xxx recvSize(%ld),sockfd(%d), want to read(%lu), errno(%d),"
                    " shut down by peer?", recvSize, sockfd, len, errno);
                return recvSize;
            }
            else // ret > 0
            {
                recvSize += ret;
                if ((cdx_size)recvSize == len)
                {
                    return recvSize;
                }
            }
        }

    }

    return recvSize;
}
cdx_ssize CdxSSLSend(cdx_int32 sockfd, SSL *ssl, const void *buf, cdx_size len,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
{
    fd_set ws;
    struct timeval tv;
    cdx_ssize ret = 0, sendSize = 0;
    cdx_long loopTimes = 0, i = 0;
    cdx_int32 ioErr;

    //blocking send
    if (CdxSockIsBlocking(sockfd))
    {
        while(1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop. sendSize(%ld)", __FUNCTION__, __LINE__, sendSize);
                return sendSize>0 ? sendSize : -2;
            }
            ret = SSL_write(ssl, ((char *)buf) + sendSize, len - sendSize);
            if (ret < 0)
            {
                ret = SSL_get_error(ssl, ret);
                if(ret == SSL_ERROR_WANT_WRITE)
                {
                    usleep(20000);
                    continue;
                }
                else
                {
                    CDX_LOGE("ret(%ld), write error, %s", ret,
                        ERR_error_string(ERR_get_error(), NULL));
                    return -1;
                }
            }
            else if(ret == 0)
            {
                CDX_LOGD("xxx sendSize(%ld),sockfd(%d), want to read(%lu), errno(%d),"
                    " shut down by peer?", sendSize, sockfd, len, errno);
                return sendSize;
            }
            else
            {
                sendSize += ret;
                if ((cdx_size)sendSize == len)
                {
                    return sendSize;
                }
            }
        }
    }

    //non-blocking send
    if (timeoutUs == 0)
    {
        loopTimes = ((unsigned long)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }

    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
            return sendSize>0 ? sendSize : -2;
        }

        FD_ZERO(&ws);
        FD_SET(sockfd, &ws);
        tv.tv_sec = 0;
        tv.tv_usec = CDX_SELECT_TIMEOUT;
        ret = select(sockfd + 1, NULL, &ws, NULL, &tv);
        if (ret < 0)
        {
            ioErr = errno;
            if (EINTR == ioErr)
            {
                continue;
            }
            CDX_LOGE("<%s,%d>select err(%d)", __FUNCTION__, __LINE__, errno);
            return -1;
        }
        else if (ret == 0)
        {
            //("timeout\n");
            continue;
        }

        while (1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
                return sendSize>0 ? sendSize : -2;
            }

            ret = SSL_write(ssl, ((char *)buf) + sendSize, len - sendSize);
            if (ret < 0)
            {
                ret = SSL_get_error(ssl, ret);
                if(ret == SSL_ERROR_WANT_WRITE)
                {
                    break;
                }
                else
                {
                    CDX_LOGE("xxx ret(%ld)", ret);
                    ERR_print_errors_fp(stderr);
                    return -1;
                }
            }
            else if (ret == 0)
            {
                //buffer is full?
                return sendSize;
            }
            else // ret > 0
            {
                sendSize += ret;
                if ((cdx_size)sendSize == len)
                {
                    return sendSize;
                }
            }
        }

    }

    return sendSize;
}

static cdx_int32 __CdxSSLStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxSSLStreamImplT *impl;
    cdx_int32 ret;
    cdx_int32 recvSize = 0;
    cdx_int32 ioErr;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    if(stream == NULL || buf == NULL || len <= 0)
    {
        CDX_LOGW("check parameter.");
        return -1;
    }

    pthread_mutex_lock(&impl->stateLock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->stateLock);
        return -2;
    }
    CdxAtomicInc(&impl->ref);
    CdxAtomicSet(&impl->state, SSL_STREAM_READING);
    pthread_mutex_unlock(&impl->stateLock);

    while(impl->notBlockFlag)
    {
        if(impl->forceStopFlag)
        {
            //CDX_LOGV("__CdxTcpStreamRead forceStop.");
            ret = -2;
            goto __exit0;
        }
        ret = CdxSSLNoblockRecv(impl->ssl, buf, len);
        if (ret < 0) //TODO
        {
            ioErr = errno;
            if (EAGAIN == ioErr)
            {
                usleep(5000);
                continue;
            }
            else
            {
                CDX_LOGE("<%s,%d>recv err(%d), ret(%d)", __FUNCTION__, __LINE__, errno, ret);
                ret = SSL_get_error(impl->ssl, ret);
                if(ret == SSL_ERROR_WANT_READ)
                {
                    CDX_LOGD("SSL_ERROR_WANT_READ");
                }
                else if(ret == SSL_ERROR_WANT_WRITE)
                {
                    CDX_LOGD("SSL_ERROR_WANT_WRITE");
                }
                else
                {
                    CDX_LOGE("xxx %s", ERR_error_string(ERR_get_error(), NULL));
                }
                impl->ioState = CDX_IO_STATE_ERROR;
                impl->notBlockFlag = 0;
                ret = -1;
                goto __exit0;
            }
        }
        impl->notBlockFlag = 0;

__exit0:
        pthread_mutex_lock(&impl->stateLock);
        CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
        CdxSSLStreamDecRef(stream);
        pthread_mutex_unlock(&impl->stateLock);

        return ret;
    }

    while((cdx_uint32)recvSize < len)
    {
        if(impl->forceStopFlag)
        {
            CDX_LOGV("__CdxSSLStreamRead forceStop.");
            if(recvSize > 0)
                break;
            else
            {
                recvSize = -2;
                goto __exit1;
            }
        }
        ret = CdxSSLRecv(impl->sockFd, impl->ssl, (char *)buf + recvSize,
                                            len - recvSize, 0, &impl->forceStopFlag);
        if(ret < 0)
        {
            if(ret == -2)
            {
                recvSize = recvSize>0 ? recvSize : -2;
                goto __exit1;
            }
            impl->ioState = CDX_IO_STATE_ERROR;
            CDX_LOGE("__CdxSSLStreamRead error(%d).", errno);
            recvSize = -1;
            goto __exit1;
        }
        else if(ret == 0)
        {
            break;
        }
        else
        {
            recvSize += ret;
        }
    }

__exit1:
    pthread_mutex_lock(&impl->stateLock);
    CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
    CdxSSLStreamDecRef(stream);
    pthread_mutex_unlock(&impl->stateLock);

    return recvSize;
}

static cdx_int32 __CdxSSLStreamGetIOState(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    return impl->ioState;
}
static cdx_int32 __CdxSSLStreamWrite(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxSSLStreamImplT *impl;
    size_t size = 0;
    ssize_t ret = 0;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    pthread_mutex_lock(&impl->stateLock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->stateLock);
        return -1;
    }
    CdxAtomicInc(&impl->ref);
    CdxAtomicSet(&impl->state, SSL_STREAM_WRITING);
    pthread_mutex_unlock(&impl->stateLock);

    while(size < len)
    {
        ret = CdxSSLSend(impl->sockFd, impl->ssl, (char *)buf + size, len - size,
                                                        0, &impl->forceStopFlag);
        if(ret < 0)
        {
            CDX_LOGE("send failed.");
            break;
        }
        else if(ret == 0)
        {
            break;
        }
        size += ret;
    }
    pthread_mutex_lock(&impl->stateLock);
    CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
    CdxSSLStreamDecRef(stream);
    pthread_mutex_unlock(&impl->stateLock);

    return (size == len) ? 0 : -1;

}
static cdx_int32 CdxSSLStreamForceStop(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;
    long ref;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    CDX_LOGV("begin SSL force stop");
    pthread_mutex_lock(&impl->stateLock);
    CdxAtomicInc(&impl->ref);
    impl->forceStopFlag = 1;
    pthread_mutex_unlock(&impl->stateLock);

    while(((ref = CdxAtomicRead(&impl->state)) != SSL_STREAM_IDLE) &&
        ((ref = CdxAtomicRead(&impl->state)) != SSL_STREAM_CONNECTING))
    {
        CDX_LOGV("xxx state(%ld)", ref);
        usleep(10*1000);
    }

    pthread_mutex_lock(&impl->stateLock);
    pthread_mutex_unlock(&impl->stateLock);
    CdxSSLStreamDecRef(stream);
    CDX_LOGV("finish SSL force stop");
    return 0;
}
static cdx_int32 CdxSSLStreamClrForceStop(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    pthread_mutex_lock(&impl->stateLock);
    impl->forceStopFlag = 0;
    CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
    pthread_mutex_unlock(&impl->stateLock);

    return 0;
}

static cdx_int32 __CdxSSLStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    CdxSSLStreamImplT *impl;

    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    switch(cmd)
    {
        case STREAM_CMD_READ_NOBLOCK:
        {
            impl->notBlockFlag = 1;
            break;
        }
        case STREAM_CMD_GET_SOCKRECVBUFLEN:
        {
            *(int *)param = impl->sockRecvBufLen;
            break;
        }
        case STREAM_CMD_SET_FORCESTOP:
        {
            return CdxSSLStreamForceStop(stream);
        }
        case STREAM_CMD_CLR_FORCESTOP:
        {
            return CdxSSLStreamClrForceStop(stream);
        }
        default:
        {
            CDX_LOGE("should not be here. CMD(%d)", cmd);
            break;
        }
    }
    return 0;
}

static cdx_int32 __CdxSSLStreamClose(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);
    CDX_LOGV("xxx SSL close begin.");
    CdxAtomicInc(&impl->ref);

    CdxSSLStreamForceStop(stream);

    CdxSSLStreamDecRef(stream);
    CdxSSLStreamDecRef(stream);
    CDX_LOGV("xxx SSL close finish.");

    return 0;
}
static void CdxSSLStreamDecRef(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    CdxAtomicDec(&impl->ref);
    if(CdxAtomicRead(&impl->ref) == 0)
    {
        shutdown(impl->sockFd, SHUT_RDWR);
        closesocket(impl->sockFd);
        if(impl->ssl)
        {
            SSL_shutdown(impl->ssl);
        }
        SSL_FREE(impl);
        ERR_free_strings();
        pthread_mutex_destroy(&impl->stateLock);
        pthread_cond_destroy(&impl->stateCond);
        pthread_mutex_destroy(impl->dnsMutex);
        pthread_cond_destroy(&impl->dnsCond);
        free(impl->dnsMutex);
        impl->dnsMutex = NULL;
        free(impl);
        impl = NULL;
    }
    return ;
}

static void DnsResponeHook(void *userhdr, int ret, struct addrinfo *ai)
{
    CdxSSLStreamImplT *impl = (CdxSSLStreamImplT *)userhdr;

    if (impl == NULL)
      return;

    if (ret == SDS_OK)
    {
        impl->dnsAI = ai;
        /*CDX_LOGD("%x%x%x", ai->ai_addr->sa_data[0],
                                                  ai->ai_addr->sa_data[1],
                                                  ai->ai_addr->sa_data[2]);*/
    }

    impl->dnsRet = ret;
    if (impl->dnsMutex != NULL)
    {
        pthread_mutex_lock(impl->dnsMutex);
        pthread_cond_signal(&impl->dnsCond);
        pthread_mutex_unlock(impl->dnsMutex);
    }

    CdxSSLStreamDecRef(&impl->base);
    return ;
}

static int StartSSLStreamConnect(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;

    cdx_int32 ret;
    int64_t start, end;
    struct addrinfo *ai = NULL;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    start = GetNowUs();

    CDX_FORCE_CHECK(impl);

    CdxAtomicInc(&impl->ref);
    impl->dnsRet = SDSRequest(impl->hostname, impl->port, &ai, impl, DnsResponeHook);

    if (impl->dnsRet == SDS_OK)
    {
        CdxSSLStreamDecRef(&impl->base);
        CDX_FORCE_CHECK(ai);
    }
    else if (impl->dnsRet == SDS_PENDING)
    {
        while (1)
        {
            struct timespec abstime;

            abstime.tv_sec = time(0);
            abstime.tv_nsec = 100000000L;

            pthread_mutex_lock(impl->dnsMutex);
            pthread_cond_timedwait(&impl->dnsCond, impl->dnsMutex, &abstime); /* wait 100 ms */
            pthread_mutex_unlock(impl->dnsMutex);

            if (impl->forceStopFlag)
            {
                ai = NULL;
                break;
            }

            if (impl->dnsRet == SDS_OK)
            {
                ai = impl->dnsAI;
                break;
            }
            else if (impl->dnsRet != SDS_PENDING)
            {
                ai = NULL;
                break;
            }
        }
    }
    else
    {
        CdxSSLStreamDecRef(&impl->base);
    }

    if (ai == NULL)
    {
        goto err_out;
    }

    do
    {
        ret = CdxSockAsynConnect(impl->sockFd,
                ai->ai_addr, ai->ai_addrlen, 0,
                &impl->forceStopFlag);

        if(ret == 0)
        {
            break;
        }
        else if(ret < 0)
        {
            CDX_LOGE("connect failed. error(%d): %s.",
                     errno, strerror(errno));
            goto err_out;
        }

        if(impl->forceStopFlag == 1)
        {
            CDX_LOGV("force stop connect.");
            goto err_out;
        }
    } while ((ai = ai->ai_next) != NULL);

    if (ai == NULL)
    {
        CDX_LOGE("connect failed.");
        goto err_out;
    }

    end = GetNowUs();
    //CDX_LOGV("Start tcp time(%lld)", end-start);

    //SSL initial
    SSL_library_init();
    SSL_load_error_strings();
    impl->ctx = SSL_CTX_new(TLSv1_client_method());
    // downward compatibility? SSLv3_client_method()/SSLv23_client_method()
    if(impl->ctx == NULL)
    {
        CDX_LOGE("xxx %s", ERR_error_string(ERR_get_error(), NULL));
        //ERR_print_errors_fp(stderr);
        goto err_out;
    }
    impl->ssl = SSL_new(impl->ctx);
    if(impl->ssl == NULL)
    {
        CDX_LOGE("xxx %s", ERR_error_string(ERR_get_error(), NULL));
        //ERR_print_errors_fp(stderr);
        goto err_out;
    }

    //inject relevance into socket&SSL
    ret = SSL_set_fd(impl->ssl, impl->sockFd);
    if(ret == 0)
    {
        ERR_print_errors_fp(stderr);
        goto err_out;
    }
    CdxSockSetBlocking(impl->sockFd, 1);// set socket to blocking

    ret = CdxSSLConnect(impl->sockFd,impl->ssl, 0, &impl->forceStopFlag);
    if(ret < 0)
    {
        CDX_LOGE("ssl connect failed.");
        goto err_out;
    }
    return 0;

err_out:
    end = GetNowUs();
    //CDX_LOGV("Start tcp time(%lld)", end-start);

    return -1;
}

static cdx_int32 __CdxSSLStreamConnect(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;
    cdx_int32 result;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    pthread_mutex_lock(&impl->stateLock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->stateLock);
        return -1;
    }
    CdxAtomicSet(&impl->state, SSL_STREAM_CONNECTING);
    CdxAtomicInc(&impl->ref);
    pthread_mutex_unlock(&impl->stateLock);

    result = StartSSLStreamConnect(stream);
    if (result < 0)
    {
        CDX_LOGE("StartTcpStreamConnect failed!");
        pthread_mutex_lock(&impl->stateLock);
        impl->ioState = CDX_IO_STATE_ERROR;
        pthread_mutex_unlock(&impl->stateLock);
    }
    else
    {
        pthread_mutex_lock(&impl->stateLock);
        impl->ioState = CDX_IO_STATE_OK;
        pthread_mutex_unlock(&impl->stateLock);
    }

    pthread_mutex_lock(&impl->stateLock);
    CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
    CdxSSLStreamDecRef(&impl->base);
    pthread_mutex_unlock(&impl->stateLock);
    return (impl->ioState == CDX_IO_STATE_ERROR) ? -1 : 0;

}

static struct CdxStreamOpsS CdxSSLStreamOps = {
    .connect    = __CdxSSLStreamConnect,
    .read       = __CdxSSLStreamRead,
    .close      = __CdxSSLStreamClose,
    .getIOState = __CdxSSLStreamGetIOState,
//    .forceStop  = __CdxTcpStreamForceStop,
    .write      = __CdxSSLStreamWrite,
    .control    = __CdxSSLStreamControl
};

static CdxStreamT *__CdxSSLStreamCreate(CdxDataSourceT *source)
{
    CdxSSLStreamImplT *impl = NULL;
    cdx_int32 sockRecvLen;
    cdx_int32 fd = -1;

    impl = (CdxSSLStreamImplT *)malloc(sizeof(CdxSSLStreamImplT));
    if(NULL == impl)
    {
        CDX_LOGE("malloc failed");
        return NULL;
    }
    memset(impl, 0x00, sizeof(CdxSSLStreamImplT));
    impl->base.ops = &CdxSSLStreamOps;
    impl->ioState = CDX_IO_STATE_INVALID;

    sockRecvLen = 0;
    fd = CdxAsynSocket(AF_INET, &sockRecvLen);
    if(fd < 0)
    {
        impl->ioState = CDX_IO_STATE_ERROR;
        CDX_LOGE("create socket failed.");
        goto err_out;
    }
    impl->sockFd = fd;
    impl->sockRecvBufLen = sockRecvLen;
    impl->port = *(cdx_int32 *)((CdxHttpSendBufferT *)source->extraData)->size;
    impl->hostname = (char *)((CdxHttpSendBufferT *)source->extraData)->buf;
    //CDX_LOGV("port (%d), hostname(%s)", impl->port, impl->hostname);
    CdxAtomicSet(&impl->ref, 1);
    pthread_mutex_init(&impl->stateLock, NULL);

    impl->dnsMutex = (pthread_mutex_t*)calloc(1,sizeof(pthread_mutex_t));
    if (impl->dnsMutex == NULL)
    {
        CDX_LOGE("malloc failed");
        return NULL;
    }

    pthread_mutex_init(impl->dnsMutex, NULL);
    pthread_cond_init(&impl->dnsCond, NULL);
    impl->dnsRet = -1;

    return &impl->base;

err_out:
    if(impl)
    {
        free(impl);
        impl = NULL;
    }
    return NULL;
}

CdxStreamCreatorT sslStreamCtor = {
    .create = __CdxSSLStreamCreate
};
