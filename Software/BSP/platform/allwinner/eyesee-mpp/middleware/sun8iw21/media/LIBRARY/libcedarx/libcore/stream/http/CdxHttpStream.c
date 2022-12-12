/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxHttpStream.c
 * Description : Http stream implementation.
 * History : 1. add cacheManager 2017-04.
 *
 */

//#define CONFIG_LOG_LEVEL 3
#define LOG_TAG "httpStream"
#include <stdlib.h>
#include <stdio.h>
#include <CdxStream.h>
#include <CdxHttpStream.h>
#include <CdxMemory.h>
#include <CdxTypes.h>
#include <sys/time.h>
#include <stdint.h>
#include <CdxTime.h>
#include <errno.h>

#define MAX_STR_LEN (4096)
#define EACH_CACHE_SIZE (512*1024)//Bytes
#define MAX_STREAM_BUF_SIZE (10*1024*1024)
#define MAX_CACHE_NUM  (MAX_STREAM_BUF_SIZE/EACH_CACHE_SIZE)
#define CACHE_NODE_SIZE (32*1024)
#define CACHE_NODE_NUM (EACH_CACHE_SIZE / CACHE_NODE_SIZE)
#define PROBE_DATA_LEN_DEFAULT (2*1024)
#define PROBE_DATA_LEN_MAX (128*1024)
#define RE_CONNECT_TIME (3600) //* unit: second
#define MAX_THREAD_NUM (5)

#if defined(CONF_PRODUCT_STB) && (!defined(CONF_CMCC) && !defined(CONF_YUNOS))
#define USE_MULTI_THREAD
#endif


struct AwMessage {
    AWMESSAGE_COMMON_MEMBERS
    uintptr_t params[4];
};

typedef struct HttpInfoS
{
    cdx_int64 nSize;
    cdx_int32 bSeekable;
}HttpInfoT;

//http command id.
static const int HTTP_COMMAND_PREPARE    = 0x101;
static const int HTTP_COMMAND_RECV       = 0x102;
static const int HTTP_COMMAND_PAUSE      = 0x103;
static const int HTTP_COMMAND_QUIT       = 0x104;

enum CdxHttpStreamE
{
    HTTP_CONTENT_INFO   = 0x110,
    HTTP_EXTRA_DATA     = 0x111,
    HTTP_DL_PART_FIN    = 0x112,
    HTTP_PREPARED       = 0x113,
    HTTP_RESPONSE_HDR   = 0x114,
    HTTP_REDIRECT_URL   = 0x115,
    HTTP_LAST_PART_FIN  = 0x116,
};

enum HttpDownLoadStateE
{
    DL_STATE_OK = 0,  //* nothing wrong of the data accession.
    DL_STATE_INVALID,
    DL_STATE_ERROR,   //* download fail.
    DL_STATE_EOS,     //* download eos.
};

static cdx_int64 GetNowUs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_sec * 1000000ll + tv.tv_usec;
}

static int httpSetCallback(void *dl, HttpCallback callback, void *pUserData)
{
    DownloadCtxT *d = (DownloadCtxT *)dl;
    d->callback  = callback;
    d->pUserData = pUserData;
    return 0;
}

static int streamDLStateIsEos(CdxHttpStreamImplT *impl)
{
    int ret = 1;
    int i = 0;

    while(i < impl->threadNum)
    {
        if(impl->dlCtx[i].dlState != DL_STATE_EOS)
        {
            ret = 0;
            break;
        }
        i++;
    }

    return ret;
}

static int CallbackProcess(void* pUserData, int eMessageId, void* param)
{
    CdxHttpStreamImplT *impl = (CdxHttpStreamImplT *)pUserData;

    switch(eMessageId)
    {
        case STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR:
        {
            if(param != NULL)
            {
                impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR, param);
            }
            break;
        }
        default:
            logw("ignore demux callback message, eMessageId = 0x%x.", eMessageId);
            break;
    }
    return 0;
}

static int httpCallbackProcess(void* pUserData, int eMessageId, void* param, int bLocal)
{
    CdxHttpStreamImplT *impl = (CdxHttpStreamImplT *)pUserData;

    if(bLocal == 1)
    {
        switch(eMessageId)
        {
            case HTTP_CONTENT_INFO:
            {
                pthread_mutex_lock(&impl->lock);
                HttpInfoT info = *(HttpInfoT*)param;
                if(impl->totalSize <= 0 /*|| (impl->totalSize > 0 && impl->totalSize != info.nSize)*/)
                    impl->totalSize = info.nSize;
                impl->seekAble = info.bSeekable;
                pthread_mutex_unlock(&impl->lock);
                break;
            }
            case HTTP_EXTRA_DATA:
            {
                pthread_mutex_lock(&impl->lock);
                ExtraDataContainerT *hfs = (ExtraDataContainerT *)param;
                if(impl->hfsContainer.extraData == NULL)
                {
                    impl->hfsContainer.extraData =
                             (CdxHttpHeaderFieldsT *)malloc(sizeof(CdxHttpHeaderFieldsT));
                    impl->hfsContainer.extraDataType = hfs->extraDataType;
                    CdxHttpHeaderFieldsT *hdr = (CdxHttpHeaderFieldsT *)(hfs->extraData);
                    CdxHttpHeaderFieldsT *hdr1 =
                                         (CdxHttpHeaderFieldsT *)(impl->hfsContainer.extraData);
                    CDX_FORCE_CHECK(hdr && hdr1);
                    hdr1->num = hdr->num;
                    hdr1->pHttpHeader = malloc(hdr1->num * sizeof(CdxHttpHeaderFieldT));
                    int i;
                    for(i = 0; i < hdr1->num; i++)
                    {
                        (hdr1->pHttpHeader + i)->key = strdup((hdr->pHttpHeader + i)->key);
                        (hdr1->pHttpHeader + i)->val = strdup((hdr->pHttpHeader + i)->val);

                        logd("hfs %s %s",
                                  (hdr1->pHttpHeader + i)->key,(hdr1->pHttpHeader + i)->val);
                    }
                }
                pthread_mutex_unlock(&impl->lock);
                break;
            }
            case HTTP_RESPONSE_HDR:
            {
                pthread_mutex_lock(&impl->lock);
                if(impl->resHdr == NULL)
                {
                    CdxHttpHeaderT *hdr = (CdxHttpHeaderT *)param;
                    CdxHttpHeaderT *resHdr;
                    resHdr = CdxHttpNewHeader();
                    if(resHdr == NULL)
                    {
                        loge("new header fail.");
                        pthread_mutex_unlock(&impl->lock);
                        return -1;
                    }
                    impl->resHdr = resHdr;
                    resHdr->protocol = strdup(hdr->protocol);
                    resHdr->statusCode = hdr->statusCode;
                    resHdr->reasonPhrase = strdup(hdr->reasonPhrase);
                    CDX_FORCE_CHECK(resHdr->protocol && resHdr->reasonPhrase);
                }
                pthread_mutex_unlock(&impl->lock);
                break;
            }
            case HTTP_LAST_PART_FIN:
            {
                pthread_mutex_lock(&impl->lock);
                impl->bLastPartFin = *(int *)param;
                if(impl->threadNum == 1)
                    impl->bDownloadFin = impl->bLastPartFin;
                else
                {
                    impl->bDownloadFin = 0;
                    if(streamDLStateIsEos(impl) == 1)
                        impl->bDownloadFin = 1;
                }
                pthread_mutex_unlock(&impl->lock);
                break;
            }
            case HTTP_DL_PART_FIN:
            {
                pthread_mutex_lock(&impl->lock);
                if(impl->threadNum > 1)
                {
                    if(impl->bLastPartFin == 1)
                    {
                        if(streamDLStateIsEos(impl) == 1)//the last part maybe download finish later.
                            impl->bDownloadFin = 1;
                        else
                            impl->bDownloadFin = 0;
                    }
                }
                pthread_mutex_unlock(&impl->lock);
                break;
            }
            case HTTP_PREPARED:
            {
                pthread_mutex_lock(&impl->lock);
                if(impl->bPrepared == 0)
                {
                    cdx_int64 ret = *(int *)param;
                    impl->bPrepared = 1;
                    impl->retPrepared = ret;
                }
                pthread_mutex_unlock(&impl->lock);
                pthread_cond_signal(&impl->condPrepared);
                break;
            }
            case HTTP_REDIRECT_URL:
            {
                pthread_mutex_lock(&impl->lock);
                Pfree(impl->pool, impl->sourceUri);
                impl->sourceUri = Pstrdup(impl->pool, (cdx_char *)param);
                if(impl->sourceUri == NULL)
                {
                    loge("strdup fail.");
                    pthread_mutex_unlock(&impl->lock);
                    return -1;
                }
                pthread_mutex_unlock(&impl->lock);
            }
            default:
                logw("ignore demux callback message, eMessageId = 0x%x.", eMessageId);
                return -1;
        }
    }
    else
    {
        switch(eMessageId)
        {
            case STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR:
            case STREAM_EVT_DOWNLOAD_ERROR:
            case STREAM_EVT_DOWNLOAD_END:
            case STREAM_EVT_DOWNLOAD_END_TIME:
            case STREAM_EVT_CMCC_LOG_RECORD:
            case STREAM_EVT_NET_DISCONNECT:
            case STREAM_EVT_DOWNLOAD_FIRST_TIME:
            case STREAM_EVT_DOWNLOAD_START_TIME:
            case STREAM_EVT_DOWNLOAD_START:
            case STREAM_EVT_DOWNLOAD_GET_TCP_IP:
            case STREAM_EVT_DOWNLOAD_RESPONSE_HEADER:
                break;
            default:
            {
                logw("ignore demux callback message, eMessageId = 0x%x.", eMessageId);
                return -1;
            }
        }
        if(impl->callback != NULL)
        {
            impl->callback(impl->pUserData, eMessageId, param);
        }
    }
    return 0;
}

static int httpStreamPrepare(DownloadCtxT *d)
{
    AwMessage msg;

    //* send a prepare message.
    memset(&msg, 0, sizeof(AwMessage));
    msg.messageId = HTTP_COMMAND_PREPARE;
    AwMessageQueuePostMessage(d->mq, &msg);
    return 0;
}

static cdx_int32 CdxHttpSendRequest(DownloadCtxT *d, cdx_int64 pos)
{
    cdx_char str[MAX_STR_LEN]={0};
    cdx_int32 ret = 0;
    CdxHttpHeaderT *httpHdr;
    CdxUrlT *serverUrl = d->urlT;
    cdx_int32 setRangeFlag = 0;
    CdxDataSourceT tcpSource;
    CdxHttpSendBufferT sendBuf;
    cdx_int32 i;

    httpHdr = CdxHttpNewHeader();
    if(httpHdr == NULL)
    {
        loge("httpHdr is NULL.");
        return -1;
    }

    CdxHttpSetUri(httpHdr, serverUrl->file);

    if (serverUrl->port && (serverUrl->port != 80))
    {
        snprintf(str, sizeof(str), "Host: %s:%d", serverUrl->hostname, serverUrl->port);
    }
    else
    {
        snprintf(str, sizeof(str), "Host: %s", serverUrl->hostname);
    }

    //set Host field
    CdxHttpSetField(httpHdr, str);

    //set extended header field
    if (d->pHttpHeader)
    {
        for (i=0; i < d->nHttpHeaderSize; i++)
        {
            if (strcasecmp("User-Agent", d->pHttpHeader[i].key) == 0)//UA max length?
            {
                continue;
            }
            if(strcasecmp("Range", d->pHttpHeader[i].key) == 0)//Range: bytes=%lld-%lld
            {
                logd("attention!");
                setRangeFlag = 1;
            }
            snprintf(str, sizeof(str), "%s: %s",d->pHttpHeader[i].key,
                                                d->pHttpHeader[i].val);
            CdxHttpSetField(httpHdr, str);
            logv("xxx http header key: %s, val:%s",
            d->pHttpHeader[i].key, d->pHttpHeader[i].val);
        }
    }

    //set User-Agent
    snprintf(str, sizeof(str), "User-Agent: %s",d->ua);
    CdxHttpSetField(httpHdr, str);

    //set Range
    if(setRangeFlag == 0 && d->oneThread == 0)
    {
        snprintf(str, sizeof(str), "Range: bytes=%lld-%lld", d->startPos, d->endPos);
        CdxHttpSetField(httpHdr, str);
    }
    else if(d->oneThread == 1)
    {
        if(pos == 0)
        {
            snprintf(str, sizeof(str), "Range: bytes=%lld-", d->startPos);
            CdxHttpSetField(httpHdr, str);
        }
        else
        {
            snprintf(str, sizeof(str), "Range: bytes=%lld-", pos);
            CdxHttpSetField(httpHdr, str);
        }
    }
    else if(setRangeFlag == 1)
    {
        logd("set range?");
    }

#ifndef HTTP_KEEP_ALIVE
    CdxHttpSetField(httpHdr, "Connection: close");
#else
    CdxHttpSetField(httpHdr, "Connection: Keep-Alive");
#endif

    if (d->isAuth == 1)
    {
        CdxHttpAddBasicAuthentication(httpHdr, d->urlT->username, d->urlT->password);
        d->isAuth = 0;
    }

    httpHdr->httpMinorVersion = 1;
    if(CdxHttpBuildRequest(httpHdr) == NULL)
    {
        loge("CdxHttpBuildRequest fail.");
        goto err_out;
    }

    if(!strcasecmp(serverUrl->protocol, "https"))
    {
        if(serverUrl->port == 0)
        {
            serverUrl->port = 443;
        }
    }
    if (serverUrl->port == 0)
    {
        serverUrl->port = 80;  // Default port for the web server
    }

    //build "tcp://host:port"
    sendBuf.buf = (void *)serverUrl->hostname;
    sendBuf.size = (void *)&serverUrl->port;
    tcpSource.extraData = (void *)&sendBuf;
    if(!strcasecmp(serverUrl->protocol, "http"))
    {
        snprintf(str, sizeof(str), "tcp://%s:%d", serverUrl->hostname,serverUrl->port);
    }
    else if(!strcasecmp(serverUrl->protocol, "https"))
    {
        snprintf(str, sizeof(str), "ssl://%s:%d", serverUrl->hostname,serverUrl->port);
    }

    tcpSource.uri = str;

    cdx_int64 startConnect, endConnect, connectTimeMs;
    startConnect = GetNowUs();

    if(d->callback)
    {
        //* Add Callback for CdxTcpStream
        struct CallBack cb;
        cb.callback = CallbackProcess;
        cb.pUserData = (void *)d->pUserData;
        ContorlTask streamContorlTask;
        streamContorlTask.cmd = STREAM_CMD_SET_CALLBACK;
        streamContorlTask.param = (void *)&cb;
        streamContorlTask.next = NULL;

        ret = CdxStreamOpen(&tcpSource, &d->lock, &d->exit, &d->pStream,
            &streamContorlTask);//__CdxTcpStreamOpen
    }
    else
    {
        ret = CdxStreamOpen(&tcpSource, &d->lock, &d->exit, &d->pStream, NULL);
    }

    if(ret < 0)
    {
        loge("CdxStreamOpen failed. '%s'", tcpSource.uri);
#if defined(CONF_CMCC)
        if(ret == -2)
        {
            loge("network disconnect! ");
            int flag = 1;
            if(d->callback)
            {
                d->callback(d->pUserData, STREAM_EVT_NET_DISCONNECT, &flag, 0);
            }
        }
#endif
        goto err_out;
    }

    //int flag = 0;
    endConnect = GetNowUs();
    connectTimeMs = (endConnect - startConnect) / 1000;

#if defined(CONF_CMCC)
    if(d->callback)
    {
        d->callback(d->pUserData, STREAM_EVT_NET_DISCONNECT, &flag, 0);

        //*cmcc 2.1.7.12-m3
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]Connect server OK! spend time: %lldms",
            LOG_TAG, __FUNCTION__, __LINE__, connectTimeMs);
        d->callback(d->pUserData, STREAM_EVT_CMCC_LOG_RECORD, (void*)cmccLog, 0);
    }
#endif

    ret = CdxStreamWrite(d->pStream, httpHdr->buffer, httpHdr->bufferSize);
    if (ret < 0)
    {
        loge("send error.");
        goto err_out;
    }

    CdxHttpFree(httpHdr);
    return 0;

err_out:

    CdxHttpFree(httpHdr);
    return -1;
}

static CdxHttpHeaderT *CdxHttpReadResponse(DownloadCtxT *d)
{
    CdxHttpHeaderT *httpHdr;
    CdxStreamT *pStream;
    int i = 0;
    int bufTmpSize = 0;
    char *buf = NULL;

    pStream = d->pStream;

    httpHdr = CdxHttpNewHeader();

    if (httpHdr == NULL)
    {
        loge("CdxHttpNewHeader fail.");
        return NULL;
    }
    cdx_int64 start, end;
    start = GetNowUs();
    while(1)
    {
        if(d->exit == 1)
        {
            logw("force stop CdxHttpReadResponse.");
            goto err_out;
        }

        if((int)httpHdr->bufferSize == bufTmpSize)
        {
            if(bufTmpSize >= (1 << 16))
            {
                loge("size too big...");
                goto err_out;
            }
            buf = realloc(httpHdr->buffer, bufTmpSize+1024+1); //* attention, end with '\0'
            if(!buf)
            {
                loge("realloc failed.");
                goto err_out;
            }
            httpHdr->buffer = buf;
            bufTmpSize += 1024;
        }

        i = CdxStreamRead(pStream, httpHdr->buffer+httpHdr->bufferSize, 1);
        if(i != 1)
        {
            loge("read failed.");
            goto err_out;
        }
        httpHdr->bufferSize++;
        httpHdr->buffer[httpHdr->bufferSize] = 0;
        if(CdxHttpIsHeaderEntire(httpHdr) > 0)
        {
            break;
        }
    }

#if defined(CONF_YUNOS)
    cdx_int64 downloadFirstTime = GetNowUs();//Ali YUNOS invoke info
    if(d->callback)
    {
        d->callback(d->pUserData, STREAM_EVT_DOWNLOAD_FIRST_TIME, &downloadFirstTime, 0);
    }
#endif

    end = GetNowUs();
    logv("xxx get response header cost time: %lld", end-start);
    if (CdxHttpResponseParse(httpHdr) < 0)
    {
        CdxHttpFree(httpHdr);
        return NULL;
    }

#if defined(CONF_YUNOS)
    //Ali YUNOS invoke info http respond header
    if(httpHdr->posHdrSep > 0)
    {
        char *tmpBuf = calloc(1, httpHdr->posHdrSep);
        memcpy(tmpBuf, httpHdr->buffer, httpHdr->posHdrSep);

        if(d->callback)
            d->callback(d->pUserData, STREAM_EVT_DOWNLOAD_RESPONSE_HEADER, tmpBuf, 0);
        free(tmpBuf);
    }
#endif

    return httpHdr;

err_out:
    CdxHttpFree(httpHdr);
    return NULL;
}

static int AnalyseHttpHeader(DownloadCtxT *d, int *haveCookie)
{
    int ret = 0, i;
    *haveCookie = 0;
    if(d->pHttpHeader)
    {
        for(i = 0; i < d->nHttpHeaderSize; i++)
        {
            if(strcasecmp("Cookie", d->pHttpHeader[i].key) == 0)
            {
                ret = 1;
                *haveCookie = 1;
                break;
            }
        #if 0
            else if(strcasecmp("Set-Cookie", impl->pHttpHeader[i].key) == 0)
            {
                ret++;
                *haveCookie = 1;
            }
        #endif
        }
    }
    return ret;
}

static void clearHttpExtraDataContainer(ExtraDataContainerT *hfs)
{
    if(hfs->extraData)
    {
        if(hfs->extraDataType == EXTRA_DATA_HTTP_HEADER)
        {
            CdxHttpHeaderFieldsT *hdr = (CdxHttpHeaderFieldsT *)(hfs->extraData);
            if(hdr->pHttpHeader)
            {
                int i;
                for(i = 0; i < hdr->num; i++)
                {
                    free((void*)(hdr->pHttpHeader + i)->key);
                    free((void*)(hdr->pHttpHeader + i)->val);
                }
                free(hdr->pHttpHeader);
            }
        }
        free(hfs->extraData);
        hfs->extraData = NULL;
    }
    hfs->extraDataType = EXTRA_DATA_UNKNOWN;
    return;
}

static void MakeExtraDataContainer(DownloadCtxT *d, CdxHttpHeaderT* httpHdr)
{
    int haveCookie = 0, num = 0, i = 0, j = 0, flag = 0;
    clearHttpExtraDataContainer(&d->hfsContainer);
    AnalyseHttpHeader(d, &haveCookie);

    if(!haveCookie && httpHdr->cookies)
    {
        num = 1;
    }
    if(num + d->nHttpHeaderSize > 0)
    {
        CdxHttpHeaderFieldsT *extraData =
            (CdxHttpHeaderFieldsT *)malloc(sizeof(CdxHttpHeaderFieldsT));
        CdxHttpHeaderFieldT *pHttpHeader = (CdxHttpHeaderFieldT *)malloc(
            (num + d->nHttpHeaderSize) * sizeof(CdxHttpHeaderFieldT));
        if(!extraData || !pHttpHeader)
        {
            loge("malloc fail.");
            if (extraData)
                free(extraData);
            if (pHttpHeader)
                free(pHttpHeader);
            return;
        }

        for(i = 0; i < d->nHttpHeaderSize; i++)
        {
            if(strcasecmp("Cookie", d->pHttpHeader[i].key) == 0)
            {
                if(!httpHdr->cookies || strstr(d->pHttpHeader[i].val, httpHdr->cookies))
                {
                    (pHttpHeader + j)->key = strdup(d->pHttpHeader[i].key);
                    (pHttpHeader + j)->val = strdup(d->pHttpHeader[i].val);
                }
                else
                {
                    (pHttpHeader + j)->key = strdup(d->pHttpHeader[i].key);
                    (pHttpHeader + j)->val = strdup(httpHdr->cookies);
                }
                flag = 1;
                j++;
                logd("test Cookie: %s", d->pHttpHeader[i].val);
            }
            else
            {
                (pHttpHeader + j)->key = strdup(d->pHttpHeader[i].key);
                (pHttpHeader + j)->val = strdup(d->pHttpHeader[i].val);
                j++;
            }
        }

        if(flag == 0 && httpHdr->cookies)
        {
            (pHttpHeader + j)->key = strdup("Cookie");
            (pHttpHeader + j)->val = strdup(httpHdr->cookies);
            j++;
        }

        extraData->num = d->nHttpHeaderSize + num;
        extraData->pHttpHeader = pHttpHeader;
        d->hfsContainer.extraDataType = EXTRA_DATA_HTTP_HEADER;
        d->hfsContainer.extraData = extraData;
    }

    return ;
}

static cdx_int32 ReSetHeaderFields(CdxHttpHeaderFieldsT *pHdrs, DownloadCtxT *d)
{
    int i;

    if(pHdrs == NULL || d == NULL)
    {
        loge("check para");
        return -1;
    }

    if(d->pHttpHeader)
    {
        for(i = 0; i < d->nHttpHeaderSize; i++)
        {
            if(d->pHttpHeader[i].key)
            {
                Pfree(d->pool, (void *)d->pHttpHeader[i].key);
            }
            if(d->pHttpHeader[i].val)
            {
                Pfree(d->pool, (void *)d->pHttpHeader[i].val);
            }
        }
        Pfree(d->pool, d->pHttpHeader);
        d->pHttpHeader = NULL;
    }

    d->nHttpHeaderSize = pHdrs->num;
    d->pHttpHeader = (CdxHttpHeaderFieldT *)Palloc(d->pool, pHdrs->num *
        sizeof(CdxHttpHeaderFieldT));
    if(d->pHttpHeader == NULL)
    {
        loge("malloc failed.");
        return -1;
    }

    for(i = 0; i < d->nHttpHeaderSize; i++)
    {
        (d->pHttpHeader + i)->key = (const char *)Pstrdup(d->pool,
            (pHdrs->pHttpHeader + i)->key);
        (d->pHttpHeader + i)->val = (const char *)Pstrdup(d->pool,
            (pHdrs->pHttpHeader + i)->val);

        logv("extraDataContainer %s %s", (d->pHttpHeader + i)->key,
            (d->pHttpHeader + i)->val);
    }

    return 0;
}

static void clearHttpHeader(CdxHttpHeaderFieldT *p, cdx_int32 num, AwPoolT *pool)
{
    if(p != NULL && num > 0 && pool != NULL)
    {
        cdx_int32 i;
        for(i = 0; i < num; i++)
        {
            if(p[i].key)
            {
                Pfree(pool, (void *)p[i].key);
            }
            if(p[i].val)
            {
                Pfree(pool, (void *)p[i].val);
            }
        }
        Pfree(pool, p);
    }
    return;
}

//request, read response header
static int handlePrepare(DownloadCtxT *d)
{
    int redirect = 0;
    const char *acceptRanges;
    int seekable = 1;
    int res = -1;
    int authRetry = 0;
    char* contentLength;
    char* transferEncoding;
    char* contentRange;
    char* contentEncoding;
    char* nextUrl = NULL;
    CdxUrlT* url = NULL;
    CdxHttpHeaderT* httpHdr = NULL;

    url = d->urlT;

    do
    {
        if (httpHdr)
        {
            CdxHttpFree(httpHdr);
            httpHdr = NULL;
        }

        if(d->exit)
        {
            logd("forcestop");
            goto err_out;
        }
        if (redirect == 1)
        {
            CdxStreamClose(d->pStream);
            d->pStream = NULL;
            redirect = 0;
        }
        cdx_int64 t0=CdxGetNowUs();
        res = CdxHttpSendRequest(d, d->baseOffset);
        if(res < 0)
        {
            loge("CdxHttpSendRequest failed.");
            return -1;
        }

#if defined(CONF_YUNOS)
        d->downloadStart = GetNowUs();
        if(d->callback)
        {
            //Ali YUNOS invoke info
            d->callback(d->pUserData, STREAM_EVT_DOWNLOAD_START_TIME, &(d->downloadStart), 0);
        }
#endif
#if defined(CONF_CMCC)
        d->downloadStart = GetNowUs();
        if(d->callback)
        {
            d->callback(d->pUserData, STREAM_EVT_DOWNLOAD_START, NULL, 0);
        }
#endif
        httpHdr = CdxHttpReadResponse(d);
        if (httpHdr == NULL)
        {
            loge("Read http response failed.");
            return -1;
        }

        MakeExtraDataContainer(d, httpHdr);
        logv("lbh request+read response: %lld ms", (CdxGetNowUs()-t0)/1000);

        if(httpHdr->httpMinorVersion == 0)
            //http/1.0 not support range, but some http/1.0 server may not really http/1.0...
        {
            //seekable = 0;
            logd("Http server version: HTTP/1.%u", httpHdr->httpMinorVersion);
        }

#if defined(CONF_CMCC)
        if(d->callback)
        {
            char cmccLog[4096] = "";
            sprintf(cmccLog, "[info][%s %s %d]http status code: %d",
                LOG_TAG, __FUNCTION__, __LINE__, httpHdr->statusCode);
            d->callback(d->pUserData, STREAM_EVT_CMCC_LOG_RECORD, (void*)cmccLog, 0);
        }
#endif
        logd("statusCode = %d", httpHdr->statusCode);
        switch(httpHdr->statusCode)
        {
            case 200:
            case 201:
            case 202:
            case 203:
            case 204:
            case 205:
                seekable = 0; //has Range field && response code==200.
            case 206:
            {
                contentLength = CdxHttpGetField(httpHdr, "Content-Length");
                if (contentLength)
                {
                    logd("contentLength = %s",contentLength);
                    d->totalSize = atoll(contentLength);
                    d->chunkedFlag = 0;
                }
                else
                {
                    if((contentRange = CdxHttpGetField(httpHdr, "Content-Range")))
                    {
                        //sscanf(contentRange,"bytes %lld-%lld/%lld,&s,&e,&size);
                        char *p = strchr(contentRange,'/');
                        if(p != NULL)
                        {
                            d->totalSize = atoll(p + 1);
                            logv("Content-Range: %s, totalSize(%lld)",
                                contentRange, d->totalSize);
                        }
                        else
                        {
                            logd("wrong Content-Range str->[%s]", p);
                            d->totalSize = -1;
                        }
                    }
                    else
                    {
                        d->totalSize = -1;
                    }

                    if ((transferEncoding = CdxHttpGetField(httpHdr, "Transfer-Encoding")))
                    {
                        CDX_LOGI("transferEncoding = %s", transferEncoding);
                        d->chunkedFlag = 1;
                    }
                    else
                    {
                        d->chunkedFlag = 0;
                    }
                }

                //check if we can make partial content requests and thus seek in http-streams
                if(httpHdr->statusCode >= 200 && httpHdr->statusCode <= 206)
                {
                    acceptRanges = CdxHttpGetField(httpHdr, "Accept-Ranges");
                    if (acceptRanges)
                    {
                        seekable = strncmp(acceptRanges,"none",4) == 0 ? 0 : 1;
                        logv("xxx Accept-Ranges: bytes, seekable=%d", seekable);
                    }
                }
                d->seekAble = seekable;

                contentEncoding = CdxHttpGetField(httpHdr, "Content-Encoding");
                if(contentEncoding)
                {
                    if(!strcasecmp(contentEncoding, "gzip") ||
                        !strcasecmp(contentEncoding, "deflate"))
                    {
#if __CONFIG_ZLIB
                        d->compressed = 1;
                        if(inflateInit2(&d->inflateStream, 32+15) != Z_OK)
                        {
                            loge("inflateInit2 failed.");
                            goto err_out;
                        }
                        if(zlibCompileFlags() & (1 << 17))
                        {
                            loge("not support, check.");
                            goto err_out;
                        }
                        d->inflateBuffer = NULL;
#else
                        logw("(%s) need zlib support.", contentEncoding);
                        goto err_out;
#endif
                    }
                }

                char *conn = CdxHttpGetField(httpHdr, "Connection");
                if (conn != NULL)
                {
                    if (strcasecmp(conn, "Keep-Alive") == 0)
                    {
                        logd("http keep alive");
                        d->keepAlive = 1;
                    }
                }

                //if one thread, should callback totSize.
                if(d->oneThread == 1 && d->totalSize > 0 && d->pUserData != NULL)
                {
                    HttpInfoT info;
                    info.nSize = d->totalSize;
                    info.bSeekable = d->seekAble;
                    d->callback(d->pUserData, HTTP_CONTENT_INFO, &info, 1);
                }
                goto out;
            }
            // Redirect
            case 301: // Permanently
            case 302: // Temporarily
            case 303: // See Other
            case 307: // Temporarily (since HTTP/1.1)
            {
                //RFC 2616, recommand to detect infinite redirection loops
                nextUrl = CdxHttpGetField(httpHdr, "Location");
                logv("xxx nextUrl:(%s)", nextUrl);
#if defined(CONF_CMCC)
                if(d->callback)
                {
                    char cmccLog[4096] = "";
                    sprintf(cmccLog, "[info][%s %s %d]Redirect url: %s",
                        LOG_TAG, __FUNCTION__, __LINE__, nextUrl);
                    d->callback(d->pUserData, STREAM_EVT_CMCC_LOG_RECORD, (void*)cmccLog, 0);
                }
#endif
                if(nextUrl != NULL)
                {
                    nextUrl = RmSpace(nextUrl);
                    d->urlT = CdxUrlRedirect(&url, nextUrl);//free old url and build new.
                    if(strcasecmp(url->protocol, "http") && strcasecmp(url->protocol, "https"))
                    {
                        loge("Unsupported http %d redirect to %s protocol.",
                                      httpHdr->statusCode, url->protocol);
                        goto err_out;
                    }
                    if(d->sourceUri)
                    {
                        Pfree(d->pool, d->sourceUri);
                        d->sourceUri = NULL;
                    }
                    d->sourceUri = (cdx_char *)Palloc(d->pool, strlen(url->url)+1);
                    CDX_CHECK(d->sourceUri);
                    memset(d->sourceUri, 0x00, strlen(url->url)+1);
                    memcpy(d->sourceUri, url->url, strlen(url->url));  //for ParserTypeGuess
                    if(d->oneThread == 1 && d->pUserData != NULL)
                    {
                        d->callback(d->pUserData, HTTP_REDIRECT_URL, d->sourceUri, 1);
                    }
                    redirect = 1;//resend request.
                }
                else
                {
                    logw("No redirect uri?");
                    goto err_out;
                }

                if(httpHdr->cookies)
                {
                    ReSetHeaderFields(d->hfsContainer.extraData, d);
                }
                break;
            }
            case 401: // Authentication required
            {
#if defined(CONF_YUNOS)
                if(d->callback)
                {
                    int mYunOSstatusCode = 3401; //Ali YUNOS invoke info
                    d->callback(d->pUserData, STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR,
                        &mYunOSstatusCode, 0);
                }
#endif
                if(CdxHttpAuthenticate(httpHdr, url, &authRetry)<0)
                {
                    loge("CdxHttpAuthenticate < 0.");
                    goto err_out;
                }
                redirect = 1;
                d->isAuth = 1;
                break;
            }
            case 404:
            case 500:
            default:
            {
                loge("shoud not be here. statusCode(%d)", httpHdr->statusCode);
                if(d->callback)
                {
#if defined(CONF_CMCC)
                    d->callback(d->pUserData, STREAM_EVT_DOWNLOAD_ERROR,
                        &httpHdr->statusCode, 0);
#endif
#if defined(CONF_YUNOS)
                    int mYunOSstatusCode = 3500;//Ali YUNOS invoke info
                    d->callback(d->pUserData, STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR,
                        &mYunOSstatusCode, 0);
#endif
                }
                goto err_out;
            }
        }
    }while(redirect);
out:
    if(httpHdr->cookies && d->pUserData != NULL)
    {
        logv("xxxx test httpHdr->cookies=%s", httpHdr->cookies);
        d->callback(d->pUserData, HTTP_EXTRA_DATA, &d->hfsContainer, 1);
    }

#if defined(CONF_YUNOS)
    d->callback(d->pUserData, HTTP_RESPONSE_HDR, httpHdr, 1);

    if(d->pStream)
    {
        //Ali YUNOS invoke info
        CdxStreamControl(d->pStream, STREAM_CMD_GET_IP, (void*)(d->tcpIp));
        if(d->callback)
        {
            d->callback(d->pUserData, STREAM_EVT_DOWNLOAD_GET_TCP_IP, d->tcpIp, 0);
        }
    }
#endif

    CdxHttpFree(httpHdr);
    return 0;

err_out:
    CdxHttpFree(httpHdr);
    clearHttpExtraDataContainer(&d->hfsContainer);
    return -1;
}

static void httpStreamRecv(AwMessageQueue *mq)
{
    if(AwMessageQueueGetCount(mq) <= 0)
    {
        AwMessage msg;
        memset(&msg, 0, sizeof(AwMessage));
        msg.messageId = HTTP_COMMAND_RECV;
        AwMessageQueuePostMessage(mq, &msg);
    }

    return;
}

//Transfer-Encoding: chunked
//len1\r\n...\r\nlen2\r\n...\r\n......0\r\n\r\n.
//return -2: force stop
//copy data from httpDataBufferChunked to buf.
static int CdxReadChunkedData(DownloadCtxT *d,void* buf, int len)
{
    cdx_int32   bufferLen = 0;
    cdx_int32   readLen = 0;
    cdx_int32   sum = 0;
    cdx_int32   size = 0;
    //cdx_int32   ioState = 0;
    cdx_int32   tempLen = len;
    cdx_char    *tempBuf = buf;
    cdx_char    crlfBuf[4];
    cdx_int32   needReadLen = 0;
    cdx_int32   ret;
    cdx_char    chunkedLenChar[10] = {0};
    cdx_int32   chunkedSizeInt = 0;

    while(sum < len)
    {

        if(d->exit == 1)
        {
            if(sum > 0)
                break;
            else
            {
                logw("force stop CdxReadChunkedData.");
                return -2;
            }
        }

        //*******************************************
        //*copy data from httpDataBufferChunked.
        //*******************************************
        if(d->httpDataBufferChunked)
        {
            bufferLen = d->httpDataSizeChunked - d->httpDataBufferPosChunked;
            size = (tempLen < bufferLen) ? tempLen : bufferLen;
            memcpy(tempBuf, d->httpDataBufferChunked + d->httpDataBufferPosChunked, size);
            d->httpDataBufferPosChunked += size;
            sum += size;
            tempLen -= size;
            tempBuf += size;
            if(d->httpDataBufferPosChunked >= d->httpDataSizeChunked)
            {
                //Pfree(impl->pool, impl->httpDataBufferChunked);
                free(d->httpDataBufferChunked);
                d->httpDataBufferChunked = NULL;
                d->httpDataBufferPosChunked = 0;
                d->httpDataSizeChunked = 0;
            }
        }

        //*******************************************
        //*read data to httpDataBufferChunked.
        //*******************************************
        if(sum < len)
        {
            if(d->restChunkSize > 0)//last chunk not finished.
            {
                needReadLen = d->restChunkSize;
                logv("needRead len =%d", needReadLen);
            }
            else
            {
                if(d->dataCRLF != 0) //get rid of CRLF or LF.
                {
                    ret = CdxStreamRead(d->pStream, crlfBuf, d->dataCRLF);
                    if(ret <= 0)
                    {
                        if(ret == -2)
                        {
                            logw("force stop CdxReadChunkedData while get crlf.");
                            return -2;
                        }

                        loge("Io error.");
                        d->dlState = DL_STATE_ERROR;
                        goto err_out;
                    }
                    else if(ret < d->dataCRLF)
                    {
                        logw("force stop CdxReadChunkedData while get crlf.");
                        d->dataCRLF -= ret;
                        return -2;
                    }
                    else
                    {
                        logv("xxx crlfBuf %d %d", crlfBuf[0],crlfBuf[1]);
                        d->dataCRLF = 0;
                    }
                }
                else if(d->lenCRLF != 0) //just get rid of LF.
                {
                    ret = CdxStreamRead(d->pStream, crlfBuf, d->lenCRLF);
                    if(ret <= 0)
                    {
                        if(ret == -2)
                        {
                            logw("force stop CdxReadChunkedData while get crlf.");
                            return -2;
                        }

                        loge("Io error.");
                        d->dlState = DL_STATE_ERROR;
                        goto err_out;
                    }
                    else
                    {
                        //logv("xxx crlfBuf %d %d", crlfBuf[0],crlfBuf[1]);
                        d->lenCRLF = 0;
                    }
                }

                if(d->chunkedLen) // already has chunked size
                {
                    needReadLen = d->chunkedLen;
                    d->chunkedLen = 0;
                }
                else
                {   /*chunked size has been force stop last time, in order to
                    continue read the chunked data this time, need to combine "len".*/
                    if(d->tmpChunkedSize > 0)
                        /*last read chunked size been force stop,
                        combine "len" this time. should clear when not forcestop.*/
                    {
                        strcpy(chunkedLenChar, d->tmpChunkedLen);
                        chunkedSizeInt = d->tmpChunkedSize;
                        memset(d->tmpChunkedLen, 0, 10);
                        d->tmpChunkedSize = 0;
                        needReadLen = ReadChunkedSize(d->pStream, d->tmpChunkedLen,
                            &d->tmpChunkedSize);
                        logv("xxxxxxx chunkSize=%d", needReadLen);
                        if(needReadLen >= 0)
                        {
                            if(d->tmpChunkedSize > 0)
                            {
                                strcat(chunkedLenChar, d->tmpChunkedLen);
                            }
                            memset(d->tmpChunkedLen, 0, 10);
                            d->tmpChunkedSize = 0;
                            needReadLen = strtol(chunkedLenChar, NULL, 16);
                            if(needReadLen == 0)
                            {
                                logd("stream end.");
                                d->dlState = DL_STATE_EOS;
                                break;
                            }
                        }
                        else if(needReadLen < 0) // force stop again...
                        {
                            if(needReadLen == -2)
                            {
                                logw("force stop CdxReadChunkedData while get len.");
                                d->tmpChunkedSize += chunkedSizeInt;
                                strcat(chunkedLenChar, d->tmpChunkedLen);
                                strcpy(d->tmpChunkedLen, chunkedLenChar);
                                return -2;
                            }
                            else if(needReadLen == -3) // need to skip \n in len\r\n next time.
                            {
                                d->lenCRLF = 1;
                                strcat(chunkedLenChar, d->tmpChunkedLen);
                                d->chunkedLen = strtol(chunkedLenChar, NULL, 16);
                                if(d->chunkedLen == 0)
                                {
                                    logd("stream end.");
                                    d->dlState = DL_STATE_EOS;
                                    break;
                                }
                                logw("Forcestop, Next chunk will begin with LF, chunkedLen(%d)",
                                    d->chunkedLen);
                                return -2;
                            }
                            loge("Io error.");
                            d->dlState = DL_STATE_ERROR;
                            goto err_out;
                        }
                        else
                        {
                            d->tmpChunkedSize = 0;
                        }
                    }
                    else
                    {
                        memset(d->tmpChunkedLen, 0, 10);
                        d->tmpChunkedSize = 0;
                        needReadLen = ReadChunkedSize(d->pStream, d->tmpChunkedLen,
                            &d->tmpChunkedSize);
                        logv("xxxxxxx chunkSize=%d", needReadLen);
                        if(needReadLen == 0)
                        {
                            logd("stream end.");
                            d->dlState = DL_STATE_EOS;
                            break;
                        }
                        else if(needReadLen < 0)
                        {
                            if(needReadLen == -2)
                                // force stop while get xxx\r, next time need to combine...
                            {
                                logw("force stop CdxReadChunkedData while get len.");
                                return -2;
                            }
                            else if(needReadLen == -3) // need to skip \n in len\r\n.
                            {
                                d->lenCRLF = 1;
                                d->chunkedLen = strtol(d->tmpChunkedLen, NULL, 16);
                                if(d->chunkedLen == 0)
                                {
                                    logd("stream end.");
                                    d->dlState = DL_STATE_EOS;
                                    break;
                                }
                                logw("Forcestop, Next chunk will begin with LF, chunkedLen(%d)",
                                    d->chunkedLen);
                                return -2;
                            }
                            loge("Io error.");
                            d->dlState = DL_STATE_ERROR;
                            goto err_out;
                        }
                        else
                        {
                            d->tmpChunkedSize = 0;
                            //logv("xxx chunkedsize(0x%x)", needReadLen);
                        }

                    }
                }
            }

            d->httpDataBufferPosChunked = 0;
            d->restChunkSize = 0;
            d->httpDataSizeChunked = 0;
            d->httpDataBufferChunked = malloc(needReadLen);//Palloc(impl->pool, needReadLen);
            CDX_CHECK(d->httpDataBufferChunked);

            readLen = ReadChunkedData(d->pStream, d->httpDataBufferChunked, needReadLen);
            if(readLen <= 0)
            {
                if(readLen <= -2)
                {
                    logw("force stop CdxReadChunkedData.");
                    return -2;
                }
#if 0
                else if(readLen == -3)
                {
                    impl->restChunkSize = 0;
                    impl->httpDataSizeChunked += needReadLen;
                    impl->dataCRLF = 2; // next chunk need to skip \r\n in data\r\n.
                }
                else if(readLen == -4)
                {
                    impl->restChunkSize = 0;
                    impl->httpDataSizeChunked += needReadLen;
                    impl->dataCRLF = 1; // next chunk need to skip \n in data\r\n.
                }
#endif
                else if(readLen == 0)
                {
                    logd("EOS.");
                    d->dlState = DL_STATE_EOS;
                    return 0;
                }
                loge("Io error.");
                d->dlState = DL_STATE_ERROR;
                goto err_out;
            }
            else //--if readLen < needReadLen, last read break by forcestop, set restChunkSize
            {
                d->restChunkSize = needReadLen - readLen;
                d->httpDataSizeChunked += readLen;
            }
        }
    }

    return sum;

err_out:
    if(d->httpDataBufferChunked)
    {
        //Pfree(impl->pool, impl->httpDataBufferChunked);
        free(d->httpDataBufferChunked);
        d->httpDataBufferChunked= NULL;
    }

    return -1;
}

#if __CONFIG_ZLIB
#define DECOMPRESS_BUF_LEN (256*1024)
static cdx_int32 StreamReadCompressed(DownloadCtxT *d, cdx_uint8 *buf, int size)
{
    int ret;
    int readSize;

    if(d->inflateBuffer == NULL)
    {
        d->inflateBuffer = malloc(DECOMPRESS_BUF_LEN);
        if(d->inflateBuffer == NULL)
        {
            loge("malloc failed.");
            return -1;
        }
    }

    if(d->inflateStream.avail_in == 0)
    {
        if(d->chunkedFlag)
        {
            readSize = CdxReadChunkedData(d, d->inflateBuffer, DECOMPRESS_BUF_LEN);
        }
        else
        {
            readSize = CdxStreamRead(d->pStream, d->inflateBuffer, DECOMPRESS_BUF_LEN);
        }
        if(readSize <= 0)
        {
            logd("==== size(%lld), pos(%lld), readSize(%d)",
                d->totalSize, d->bufPos, readSize);
            return readSize;
        }

        d->inflateStream.next_in = d->inflateBuffer;
        d->inflateStream.avail_in = readSize;
    }

    d->inflateStream.avail_out = size;
    d->inflateStream.next_out = (cdx_uint8 *)buf;

    ret = inflate(&d->inflateStream, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END)
    {
        logd("inflate return: %d, %s; size: %d, %u", ret, d->inflateStream.msg,
            size, d->inflateStream.avail_out);
    }

    return size - d->inflateStream.avail_out;
}
#endif

static cdx_int32 StreamRead(DownloadCtxT *d, void *buf, cdx_int32 len)
{
    cdx_int32 size = 0;
    cdx_int32 wantToReadLen = 0;

    if(d->chunkedFlag)
    {
#if __CONFIG_ZLIB
        if(d->compressed)
        {
            size = StreamReadCompressed(d, buf, len);
            if(size <= 0)
            {
                logd("======== size = %d", size);
                d->dlState = DL_STATE_EOS;
                return 0;
            }

    #if __SAVE_BITSTREAMS
            fwrite((cdx_char *)buf, 1, size, d->fp_download);
            logd("xxx readLen(%d)",size);
            fsync(fileno(d->fp_download));
    #endif

            d->dlState = DL_STATE_OK;
            return size;
        }
#endif
        size = CdxReadChunkedData(d, buf, len);

        return size;
    }
    else
    {

#if __CONFIG_ZLIB
        if(d->compressed)
        {
            size = StreamReadCompressed(d, buf, len);
            if(size <= 0)
            {
                logd("======== size = %d", size);
                d->dlState = DL_STATE_EOS;
                return 0;
            }

#if __SAVE_BITSTREAMS
            fwrite((cdx_char *)buf, 1, size, d->fp_download);
            logd("xxx readLen(%d)",size);
            fsync(fileno(d->fp_download));
#endif
            d->dlState = DL_STATE_OK;
            return size;
        }
#endif

        if(d->totalSize > 0)
        {
            wantToReadLen = (len < d->totalSize - d->bufPos) ?
                            len : (d->totalSize - d->bufPos);
            logv("wantToReadLen (%u), totalSize(%lld), bufPos(%lld), len(%u)",
                                    wantToReadLen, d->totalSize, d->bufPos, len);
        }
        else
        {
            wantToReadLen = len;
        }
        logv("d->pStream=%p, wantToReadLen=%d, d->totalSize=%lld, d->bufPos=%lld",
            d->pStream, wantToReadLen, d->totalSize, d->bufPos);
        size = CdxStreamRead(d->pStream, buf, wantToReadLen);
        if(size < 0)
        {
            if(size == -2)
            {
                return size;
            }
            logw("xxx CdxStreamRead failed.");
            goto err_out;
        }
        else if(size == 0)
        {
            if(d->totalSize != -1)
            {
                loge("xxx readLen=0, totalSize(%lld), bufPos(%lld).",
                    d->totalSize, d->bufPos);
                goto err_out;
            }
            else
            {
                d->dlState = DL_STATE_EOS;
                return 0;
            }
        }
        else if(size <= wantToReadLen)
        {
            if((d->totalSize > 0) && (d->bufPos + size == d->totalSize))
            {
                logv("xxx EOS, impl->bufPos(%lld), readLen(%d), totsize(%lld)",
                    d->bufPos, size, d->totalSize);
                d->dlState = DL_STATE_EOS;
            }
            else
            {
                d->dlState = DL_STATE_OK;
            }
        }
#if __SAVE_BITSTREAMS
        logd("fp_download=%p", d->fp_download);
        fwrite((cdx_char *)buf, 1, size, d->fp_download);
        logd("xxx readLen(%d)",size);
        fsync(fileno(d->fp_download));
#endif
        return size;
    }

err_out:
    d->dlState = DL_STATE_ERROR;
    return -1;
}

static int handleRecv(DownloadCtxT *d)
{
    HttpCacheNodeT node;
    cdx_int32 recvSize = CACHE_NODE_SIZE;
    cdx_int32 ret;

    if(d->exit == 1)
    {
        logd("exit handleRecv.");
        return -1;
    }

    memset(&node, 0x00, sizeof(HttpCacheNodeT));
    recvSize = d->tempSize > 0 ? d->tempSize : recvSize;
    node.pData = (unsigned char *)calloc(1, recvSize);
    if(NULL == node.pData)
    {
        loge("malloc fail, size(%d)", recvSize);
        return -1;
    }

    ret = StreamRead(d, node.pData, recvSize);//read data from network.
    if((d->chunkedFlag || d->compressed) && d->dlState == DL_STATE_EOS)
    {
        d->bDlfcc = 1;
    }
    if(ret <= 0)
    {
        loge("stream read fail.");
        free(node.pData);
        node.pData = NULL;
        return ret;
    }

    node.nLength = ret;
    if(CacheManagerAddData(d->cacheManager, d->httpCache, &node) < 0)
    {
        loge("add data fail.");
        return -1;
    }

    d->bufPos += ret;
    logv("cM=%p, d->bufPos=%lld, d->httpCache(%lld,%lld), size=%d", d->cacheManager, d->bufPos,
        d->httpCache->nStartPos, d->httpCache->nEndPos, d->httpCache->nWriteSize);
    d->tempSize = recvSize - node.nLength;
    if(d->tempSize > 0)
    {
        CacheManagerCacheNodeNumInc(d->cacheManager, d->httpCache);
    }

    return ret;
}
static int httpStreamQuit(DownloadCtxT *d)
{
    AwMessage msg;

    pthread_mutex_lock(&d->lock);
    d->exit = 1;
    pthread_mutex_unlock(&d->lock);

    if(d->pStream != NULL)
    {
        CdxStreamForceStop(d->pStream);
    }

    //* send a quit message.
    memset(&msg, 0, sizeof(AwMessage));
    msg.messageId = HTTP_COMMAND_QUIT;
    msg.params[0] = (uintptr_t)&d->semQuit;
    msg.params[1] = (uintptr_t)&d->nQuitReply;
    AwMessageQueuePostMessage(d->mq, &msg);
    CdxSemTimedWait(&d->semQuit, -1);

    return d->nQuitReply;
}

static int httpStreamPause(DownloadCtxT *d)
{
    AwMessage msg;

    pthread_mutex_lock(&d->lock);
    d->exit = 1;
    if(d->pStream != NULL)
    {
        CdxStreamForceStop(d->pStream);
    }

    //* send a quit message.
    memset(&msg, 0, sizeof(AwMessage));
    msg.messageId = HTTP_COMMAND_PAUSE;
    msg.params[0] = (uintptr_t)&d->semQuit;
    msg.params[1] = (uintptr_t)&d->nQuitReply;
    AwMessageQueuePostMessage(d->mq, &msg);
    CdxSemTimedWait(&d->semQuit, -1);
    pthread_mutex_unlock(&d->lock);

    return d->nQuitReply;
}

void clrDownloadInfo(DownloadCtxT *d)
{
    if(NULL == d)
        return;

    d->bufPos = 0;
    d->totalSize = 0;
    d->chunkedFlag = 0;

    if(d->pStream != NULL)
    {
        if (d->bLastPartFin == 1 && d->keepAlive == 1)//todo
            CdxStreamControl(d->pStream, STREAM_CMD_SET_EOF, NULL);

        CdxStreamClose(d->pStream);//close tcp first.
        d->pStream = NULL;
        d->dlState = DL_STATE_INVALID;
    }

    if (d->resHdr != NULL)
    {
        CdxHttpFree(d->resHdr);
        d->resHdr = NULL;
    }

#if __SAVE_BITSTREAMS
    if(d->fp_download != NULL)
    {
        fclose(d->fp_download);
        d->fp_download = NULL;
    }
#endif
    return;
}

//nCacheSize, nPercentage not accurate.
static int httpGetCacheState(struct StreamCacheStateS *cacheState, CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    DownloadCtxT *dl;
    cdx_int32 i, percent = 0;
    cdx_int32 kbps = 0, kbpsOne = 0;
    cdx_int64 posMax = 0, posMin = LLONG_MAX;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    for(i = 0; i < impl->threadNum; i++)
    {
        dl = &impl->dlCtx[i];
        //HttpCacheT *pHttpCache = dl->httpCache;
        if(dl->startPos + dl->bufPos > posMax)
            posMax = dl->startPos + dl->bufPos;

        if(dl->startPos + dl->bufPos <= posMin)
            posMin = dl->startPos + dl->bufPos;

        GetEstBandwidthKbps(dl->bandWidthEst, &kbpsOne);
        if(dl->bWorking == 1)
        {
            kbps += kbpsOne;
        }
    }

    memset(cacheState, 0, sizeof(struct StreamCacheStateS));

    if (impl->totalSize > 0)
    {
        percent = 100 * posMax/impl->totalSize;
    }
    else
    {
        percent = 0;
    }

    cacheState->nPercentage = percent;
    cacheState->nBandwidthKbps = kbps;
    cacheState->nCacheCapacity = MAX_STREAM_BUF_SIZE;
    if(CacheManagerCheckCacheUseup(impl->cacheManager) == 0)
        cacheState->nCacheSize = posMin - CacheManagerGetReqDataPos(impl->cacheManager);
    else
        cacheState->nCacheSize = posMax - CacheManagerGetReqDataPos(impl->cacheManager);

    logv("httpGetCacheState nCacheSize:%d, nCacheCapacity %d, nBandwidthKbps:%dkbps, percent:%d%%",
          cacheState->nCacheSize, cacheState->nCacheCapacity,
          cacheState->nBandwidthKbps, cacheState->nPercentage);
    return 0;
}

static int dlCacheState(struct StreamCacheStateS *cacheState, DownloadCtxT *d)
{
    cdx_int32 percent = 0;
    cdx_int32 kbps = 0;

    memset(cacheState, 0, sizeof(struct StreamCacheStateS));

    if (d->totalSize > 0)
    {
        percent = 100 * d->bufPos/d->totalSize;
    }
    else
    {
        percent = 0;
    }

    cacheState->nPercentage = percent;

    if(GetEstBandwidthKbps(d->bandWidthEst, &kbps) == 0)
    {
        cacheState->nBandwidthKbps = kbps;
    }
    else
    {
        cacheState->nBandwidthKbps = 0;
    }

    cacheState->nCacheCapacity = d->bufSize;
    cacheState->nCacheSize = d->bufPos;

    logv("dlCacheState[%d], nCacheSize:%d, nCacheCapacity %d, nBandwidthKbps:%dkbps, percent:%d%%",
             d->id, cacheState->nCacheSize, cacheState->nCacheCapacity,
            cacheState->nBandwidthKbps, cacheState->nPercentage);

    return 0;
}

//mSec: in unit of millisecond
void dlPrintCacheState(cdx_int32 mSec, cdx_int64 *lastNotifyTime, DownloadCtxT *d)
{
    struct StreamCacheStateS cacheState;

    if(GetNowUs() > *lastNotifyTime + mSec * 1000)
    {
        dlCacheState(&cacheState, d);
        logv("dlCacheState[%d], nCacheSize:%d, nCacheCapacity %d, nBandwidthKbps:%dkbps,"
             "percent:%d%%", d->id, cacheState.nCacheSize, cacheState.nCacheCapacity,
                 cacheState.nBandwidthKbps, cacheState.nPercentage);
        *lastNotifyTime = GetNowUs();
    }

    return;
}

static void *partDownload(void *pArg)
{
    DownloadCtxT *dlCtx;
    AwMessage msg;
    sem_t *pReplySem;
    int *pReplyValue;
    cdx_int64 startTimeUs;
    cdx_int64 delayTimeUs;
    cdx_int64 lastNotifyTime = 0;
    int ret;

    dlCtx = (DownloadCtxT *)pArg;

    while(1)
    {
        get_message:
        if(AwMessageQueueGetMessage(dlCtx->mq, &msg) < 0)
        {
            logw("get message fail.");
            continue;
        }
        process_message:
        startTimeUs = GetNowUs();
        pReplySem   = (sem_t*)msg.params[0];
        pReplyValue = (int*)msg.params[1];
        logv("msg.messageId=%x",msg.messageId);
        if(msg.messageId == HTTP_COMMAND_PREPARE)
        {
            logd("process message HTTP_COMMAND_PREPARE.");

            dlCtx->dlState = DL_STATE_INVALID;
            dlCtx->bLastPartFin = 0;
            dlCtx->callback(dlCtx->pUserData, HTTP_LAST_PART_FIN, &dlCtx->bLastPartFin, 1);

            while(1)
            {
                dlCtx->tempSize = 0;
                ret = CacheManagerRequestCache(dlCtx->cacheManager, &dlCtx->httpCache);
                if(ret == -1)
                {
                    logv("no cache now, wait...");
                    ret = AwMessageQueueTryGetMessage(dlCtx->mq, &msg, 50);
                    if(ret == 0)//new message come, quit loop to process.
                    {
                        delayTimeUs = GetNowUs() - startTimeUs;
                        BandwidthEst(dlCtx->bandWidthEst, 0, delayTimeUs);
                        goto process_message;
                    }
                }
                else if(ret == -2)
                {
                    delayTimeUs = GetNowUs() - startTimeUs;
                    BandwidthEst(dlCtx->bandWidthEst, 0, delayTimeUs);
                    goto get_message;
                }
                else
                    break;
            }
            dlCtx->bWorking = 1;
            CacheManagerGetCacheRange(dlCtx->cacheManager, dlCtx->httpCache,
                                        &dlCtx->startPos, &dlCtx->endPos);

            logv("dlCtx->startPos=%lld, dlCtx->endPos=%lld", dlCtx->startPos, dlCtx->endPos);
#if __SAVE_BITSTREAMS
            sprintf(dlCtx->location, "/data/camera/http_%lld-%lld.dat", dlCtx->startPos, dlCtx->endPos);
            dlCtx->fp_download = fopen(dlCtx->location,"wb");
            if(NULL == dlCtx->fp_download)
            {
                logw("fopen failed. errno=%d", errno);
            }
#endif

            ret = handlePrepare(dlCtx);
            //dlCtx->state = HTTP_STREAM_IDLE;
            int retPrepared = 0;
            if(ret < 0)
            {
                loge("handlePrepare fail.");
                retPrepared = -1;
                dlCtx->dlState = DL_STATE_ERROR;
                dlCtx->callback(dlCtx->pUserData, HTTP_PREPARED, &retPrepared, 1);
                continue;
            }
            dlCtx->dlState = DL_STATE_OK;
            dlCtx->callback(dlCtx->pUserData, HTTP_PREPARED, &retPrepared, 1);

            httpStreamRecv(dlCtx->mq);

            delayTimeUs = GetNowUs() - startTimeUs;
            BandwidthEst(dlCtx->bandWidthEst, 0, delayTimeUs);
            continue;
        }
        else if(msg.messageId == HTTP_COMMAND_RECV)
        {
            if(dlCtx->dlState != DL_STATE_OK)
                continue;

            dlPrintCacheState(1000, &lastNotifyTime, dlCtx);
            ret = handleRecv(dlCtx);
            if(ret < 0)
            {
                if(ret == -2)
                {
                    httpStreamRecv(dlCtx->mq);
                    continue;
                }
                if(dlCtx->dlState == DL_STATE_ERROR)
                {
                    //reconnect in HttpStreamRead.
                    logw("DL_STATE_ERROR");
                }
                delayTimeUs = GetNowUs() - startTimeUs;
                BandwidthEst(dlCtx->bandWidthEst, 0, delayTimeUs);
                continue;
            }
            //multi-thread case: each download thread has exact range and has eos state.
            //one-thread case: download in one thread, should check if cache is full or not.
            if(dlCtx->dlState == DL_STATE_EOS ||
                (dlCtx->oneThread == 1 &&
                    CacheManagerCacheIsFull(dlCtx->cacheManager, dlCtx->httpCache) == 1))
            {
                if(dlCtx->oneThread != 1)
                {
                    dlCtx->callback(dlCtx->pUserData, HTTP_DL_PART_FIN, NULL, 1);
                }

                if(CacheManagerCheckCacheUseup(dlCtx->cacheManager) == 1 ||
                    (dlCtx->oneThread == 1 && dlCtx->bufPos == dlCtx->totalSize
                    && dlCtx->chunkedFlag != 1 && dlCtx->compressed != 1) ||
                    dlCtx->bDlfcc == 1)
                {
                    logd("dl the last part finish.");
                    dlCtx->bLastPartFin = 1;
                    dlCtx->callback(dlCtx->pUserData, HTTP_LAST_PART_FIN,
                        &dlCtx->bLastPartFin, 1);

                    dlCtx->downloadEnd = GetNowUs();
                    dlCtx->downloadTimeMs = (dlCtx->downloadEnd - dlCtx->downloadStart) / 1000;
                    if(dlCtx->callback)
                    {
#if defined(CONF_CMCC)
                        logv("eos set STREAM_EVT_DOWNLOAD_END impl->downloadTimeMs(%lld ms)",
                            dlCtx->downloadTimeMs);
                        ExtraDataContainerT httpExtradata;
                        httpExtradata.extraData = &(dlCtx->downloadTimeMs);
                        httpExtradata.extraDataType = EXTRA_DATA_HTTP;
                        dlCtx->callback(dlCtx->pUserData, STREAM_EVT_DOWNLOAD_END,
                            &httpExtradata, 0);
#endif
#if defined(CONF_YUNOS)
                        cdx_int64 downloadLastTime = GetNowUs();//Ali YUNOS invoke info
                        dlCtx->callback(dlCtx->pUserData, STREAM_EVT_DOWNLOAD_END_TIME,
                            &downloadLastTime, 0);
#endif
                    }
                }
                else
                {
                    if(dlCtx->oneThread == 1)
                    {
                        logv("oneThread, go on request cache and download.");
                        if(dlCtx->dlState == DL_STATE_EOS)
                        {
                            CDX_LOGD("EOS...");
                            dlCtx->bLastPartFin = 1;
                            dlCtx->callback(dlCtx->pUserData, HTTP_LAST_PART_FIN,
                                &dlCtx->bLastPartFin, 1);
                            continue;
                        }
                        while(1)
                        {
                            dlCtx->tempSize = 0;
                            ret = CacheManagerRequestCache(dlCtx->cacheManager, &dlCtx->httpCache);
                            if(ret == -1)
                            {
                                logv("no cache now, wait...");
                                ret = AwMessageQueueTryGetMessage(dlCtx->mq, &msg, 50);
                                if(ret == 0)//new message come, quit loop to process.
                                {
                                    delayTimeUs = GetNowUs() - startTimeUs;
                                    BandwidthEst(dlCtx->bandWidthEst, 0, delayTimeUs);
                                    goto process_message;
                                }
                            }
                            else if(ret == -2)
                            {
                                delayTimeUs = GetNowUs() - startTimeUs;
                                BandwidthEst(dlCtx->bandWidthEst, 0, delayTimeUs);
                                goto get_message;
                            }
                            else
                                break;
                        }
                        CacheManagerGetCacheRange(dlCtx->cacheManager, dlCtx->httpCache,
                                                    &dlCtx->startPos, &dlCtx->endPos);
                        httpStreamRecv(dlCtx->mq);
                    }
                    else
                    {
                        logv("download next part.");
                        clrDownloadInfo(dlCtx);
                        dlCtx->bWorking = 0;
                        httpStreamPrepare(dlCtx);
                    }
                }
            }
            else if(dlCtx->dlState == DL_STATE_OK)
            {
                //dlCtx->oneThread
                httpStreamRecv(dlCtx->mq);
            }
            else
            {
                logd("dlCtx->dlState(%d)", dlCtx->dlState);
            }

            delayTimeUs = GetNowUs() - startTimeUs;
            BandwidthEst(dlCtx->bandWidthEst, ret, delayTimeUs);
            continue;
        }
        else if(msg.messageId == HTTP_COMMAND_PAUSE)
        {
            logv("HTTP_COMMAND_PAUSE");
            dlCtx->exit = 0;
            dlCtx->bWorking = 0;
            if(dlCtx->pStream != NULL)
            {
                if (dlCtx->bLastPartFin == 1 && dlCtx->keepAlive == 1)//todo
                    CdxStreamControl(dlCtx->pStream, STREAM_CMD_SET_EOF, NULL);
                CdxStreamClose(dlCtx->pStream); //* quit from reading.
                dlCtx->pStream = NULL;
                dlCtx->dlState = DL_STATE_INVALID;
            }
            if(pReplyValue != NULL)
                *pReplyValue = 0;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            continue;
        }
        else if(msg.messageId == HTTP_COMMAND_QUIT)
        {
            logd("HTTP_COMMAND_QUIT");
            dlCtx->exit = 0;
            dlCtx->bWorking = 0;
            if(dlCtx->pStream != NULL)
            {
                if (dlCtx->bLastPartFin == 1 && dlCtx->keepAlive == 1)//todo
                    CdxStreamControl(dlCtx->pStream, STREAM_CMD_SET_EOF, NULL);
                CdxStreamClose(dlCtx->pStream); //* quit from reading.
                dlCtx->pStream = NULL;
                dlCtx->dlState = DL_STATE_INVALID;
            }
            if(pReplyValue != NULL)
                *pReplyValue = 0;
            if(pReplySem != NULL)
                sem_post(pReplySem);
            break;
        }
    }

    return NULL;
}
size_t Hex2Oct(char* src)
{
    return strtol(src, NULL, 16);
}
cdx_int32 CopyChunkSize(cdx_char *srcBuf, cdx_int32 *pNum)//pNum: length of "len" in "len\r\n".
{
    cdx_char  result[10] = {0};
    cdx_int32 pos = 0;
    cdx_char *tmpSrcBuf = srcBuf;

    while (1)
    {
        cdx_char byte;

        byte = *tmpSrcBuf++;

        if((byte >= '0' && byte <= '9')
            || (byte >= 'a' && byte <= 'f')
            || (byte >= 'A' && byte <= 'F'))
        {
            result[pos++] = byte;
            *pNum = pos;
            continue;
        }
        else if(byte == '\r')
        {
            byte = *tmpSrcBuf++;
            if(byte != '\n')
            {
                logw("No lf after len flag.");
                return -1;
            }
            break;
        }
        else
        {
            logw("check the content.");
            return -2;
        }
    }
    return strtol(result, NULL, 16);
}

static void ClearDataSourceFields(CdxHttpStreamImplT *impl)
{
    if(impl->sourceUri)
    {
        Pfree(impl->pool,impl->sourceUri);
        impl->sourceUri = NULL;
    }
    clearHttpHeader(impl->pHttpHeader, impl->nHttpHeaderSize, impl->pool);
    impl->pHttpHeader = NULL;

    return;
}
static cdx_int32 SetDataSourceFields(CdxDataSourceT * source, CdxHttpStreamImplT *impl)
{
    CdxHttpHeaderFieldsT* pHttpHeaders;
    cdx_int32             i;

    if(source->uri)
    {
        pHttpHeaders = (CdxHttpHeaderFieldsT *)source->extraData;
        impl->sourceUri = Pstrdup(impl->pool, source->uri);
        if(source->extraData)
        {
            impl->nHttpHeaderSize = pHttpHeaders->num;

            impl->pHttpHeader = (CdxHttpHeaderFieldT *)Palloc(impl->pool, impl->nHttpHeaderSize *
                sizeof(CdxHttpHeaderFieldT));
            if(impl->pHttpHeader == NULL)
            {
                loge("Palloc failed.");
                ClearDataSourceFields(impl);
                return -1;
            }
            memset(impl->pHttpHeader, 0x00, impl->nHttpHeaderSize * sizeof(CdxHttpHeaderFieldT));
            for(i = 0; i < impl->nHttpHeaderSize; i++)
            {
                if(0 == strcasecmp(pHttpHeaders->pHttpHeader[i].key, "Set-Cookie"))
                {
                    impl->pHttpHeader[i].key = (const char*)Pstrdup(impl->pool, "Cookie");
                }
                else
                {
                    impl->pHttpHeader[i].key = (const char*)Pstrdup(impl->pool,
                        pHttpHeaders->pHttpHeader[i].key);
                }
                if(impl->pHttpHeader[i].key == NULL)
                {
                    loge("dup key failed.");
                    ClearDataSourceFields(impl);
                    return -1;
                }
                impl->pHttpHeader[i].val = (const char*)Pstrdup(impl->pool,
                    pHttpHeaders->pHttpHeader[i].val);
                if(impl->pHttpHeader[i].val == NULL)
                {
                    loge("dup val failed.");
                    ClearDataSourceFields(impl);
                    return -1;
                }
                logv("============ impl->pHttpHeader[i].val(%s):%s",
                    impl->pHttpHeader[i].key, impl->pHttpHeader[i].val);
            }
        }

        if (source->probeSize > 0)
            impl->probeSize = source->probeSize;
    }
    return 0;
}

static CdxStreamProbeDataT *__CdxHttpStreamGetProbeData(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return &impl->probeData;
}

static int streamDLStateIsError(CdxHttpStreamImplT *impl)
{
    int ret = 0;
    int i = 0;

    while(i < impl->threadNum)
    {
        if(impl->dlCtx[i].dlState == DL_STATE_ERROR)
        {
            ret = 1;
            break;
        }
        i++;
    }

    return ret;
}

static int seekReconnect(CdxHttpStreamImplT *impl, cdx_int64 offset)
{
    if(!impl->seekAble)
    {
        logd("not seekable.");
        return -1;
    }

    //pause threads download
    int i;
    for(i = 0; i < impl->threadNum; i++)
    {
        httpStreamPause(&(impl->dlCtx[i]));
    }

    //flush cache of all threads
    CacheManagerReset(impl->cacheManager, offset);

    pthread_mutex_lock(&impl->lock);
    impl->bPrepared = 0;
    pthread_mutex_unlock(&impl->lock);

    //prepare
    for(i = 0; i < impl->threadNum; i++) //one thread, multi-threads
    {
        clrDownloadInfo(&(impl->dlCtx[i]));
        httpStreamPrepare(&(impl->dlCtx[i]));
    }
    impl->readPos = offset;
    return 0;
}

static cdx_int32 __CdxHttpStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxHttpStreamImplT *impl;
    cdx_int64 startTime = 0;
    cdx_int64 endTime = 0;
    cdx_int64 totTime = 0;
    int ret;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->state = HTTP_STREAM_READING;
    pthread_mutex_unlock(&impl->lock);

    cdx_uint32 readSize = 0;
    while(readSize < len)
    {
        if(impl->forceStopFlag)
        {
            readSize = -1;
            goto __exit;
        }
        ret = CacheManagerRequestData(impl->cacheManager, len - readSize,
                                        (cdx_uint8*)buf + readSize);
        if(ret < 0)
        {
            loge("request data fail.");
            readSize = -1;
            goto __exit;
        }
        else if(ret == 0)
        {
            pthread_mutex_lock(&impl->lock);
            if(impl->bDownloadFin == 1)
            {
                impl->bStreamReadEos = 1;
                pthread_mutex_unlock(&impl->lock);
                break;
            }
            pthread_mutex_unlock(&impl->lock);
            if(impl->totalSize > 0)
            {
                cdx_int64 pos = CacheManagerGetReqDataPos(impl->cacheManager);
                if(pos == impl->totalSize)
                {
                    impl->bStreamReadEos = 1;
                    break;
                }

                if(streamDLStateIsError(impl) == 1)
                {
                    if(impl->seekAble == 0)
                    {
                        break;
                    }
                    logd("reconnect at(%lld/%lld)", impl->readPos, impl->totalSize);
                    while(1)
                    {
                        startTime = GetNowUs();
                        ret = seekReconnect(impl, impl->readPos);
                        if(ret < 0)
                        {
                            readSize = -1;
                            goto __exit;
                        }
#if defined(CONF_CMCC)
                        if(impl->isHls)
                        {
                            readSize = -1;
                            goto __exit;
                        }
#endif
                        int tmpRet;
                        pthread_mutex_lock(&impl->lock);
                        while(impl->bPrepared != 1)
                        {
                            pthread_cond_wait(&impl->condPrepared, &impl->lock);
                        }
                        tmpRet = impl->retPrepared;
                        pthread_mutex_unlock(&impl->lock);

                        if(impl->forceStopFlag == 1)
                        {
                            readSize = -1;
                            goto __exit;
                        }
                        if(tmpRet == 0)
                        {
                            logd("reconnect ok, continue read.");
                            break;
                        }
                        else
                        {
                            logd("reconnect time: %lld s", totTime/1000000);
                            usleep(50000);
                        }

                        endTime = GetNowUs();
                        totTime += (endTime - startTime);
                        if(totTime >= (cdx_int64)RE_CONNECT_TIME * 1000000)
                        {
                            loge("reconnect failed, tried time:%d s, break.", RE_CONNECT_TIME);
                            readSize = -1;
#if defined(CONF_CMCC)
                            if(impl->callback)
                            {
                                cdx_int32 errCode = 2000;
                                impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_ERROR,
                                    &errCode);
                            }
#endif
                            goto __exit;
                        }
                    }
                }
                else
                {
                    logv("readSize=%u, pos=%lld, totSize=%lld.",
                        readSize, pos, impl->totalSize);
                    usleep(5000);
                }
            }
            else
            {
                usleep(5000);
                logv("impl->totalSize =%lld",impl->totalSize);
            }
        }
        readSize += ret;
    }

    impl->readPos += readSize;
    impl->state = HTTP_STREAM_IDLE;
#if __SAVE_BITSTREAMS
    FILE *fp = fopen("/data/camera/http_read.dat", "ab");
    fwrite(buf, 1, readSize, fp);
    fclose(fp);
#endif

__exit:
    pthread_mutex_lock(&impl->lock);
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return readSize;
}

static cdx_int32 CdxHttpStreamForceStop(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    int i;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    logv("xxx begin http force stop.");

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag == 1)
    {
        pthread_mutex_unlock(&impl->lock);
        return 0;
    }
    impl->forceStopFlag = 1;
    impl->dlct.exit = 1;
    pthread_cond_signal(&impl->condPrepared);

    while (impl->state != HTTP_STREAM_IDLE)
    {
        if(impl->dlct.pStream != NULL)
        {
            CdxStreamForceStop(impl->dlct.pStream);
        }
        for(i = 0; i < impl->threadNum; i++)
        {
            pthread_mutex_lock(&impl->dlCtx[i].lock);
            if(impl->dlCtx[i].pStream != NULL)
            {
                CdxStreamForceStop(impl->dlCtx[i].pStream);
            }
            pthread_mutex_unlock(&impl->dlCtx[i].lock);
        }
        pthread_cond_wait(&impl->cond, &impl->lock);
    }

    pthread_mutex_unlock(&impl->lock);

    logv("xxx finish http force stop");
    return 0;
}
static cdx_int32 CdxHttpStreamClrForceStop(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    int i;
    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    impl->forceStopFlag = 0;

    for(i = 0; i < impl->threadNum; i++)
    {
        if(impl->dlCtx[i].pStream != NULL)
        {
            CdxStreamClrForceStop(impl->dlCtx[i].pStream);
        }
    }
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);

    return 0;
}

static cdx_int32 __CdxHttpStreamGetIOState(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return impl->ioState;

}

static cdx_int64 getContentSize(CdxHttpStreamImplT *impl)
{
    if(impl->threadNum == 1)
    {
        return impl->dlCtx[0].totalSize;
    }
    return 0;
}

static int CdxGetProbeData(CdxHttpStreamImplT *impl)
{
    cdx_uint32 size = 0;
    cdx_char *tmpBuf = NULL;
    int ret = 0;

    if(impl->probeData.buf != NULL)
    {
        size += impl->probeData.len;
        impl->probeData.len = impl->probeSize;
    }

    if (impl->probeSize <= 0)
    {
        impl->probeData.len = PROBE_DATA_LEN_DEFAULT;
    }
    else if(impl->probeSize > PROBE_DATA_LEN_MAX)
    {
        logw("probe size(%u) too big.", impl->probeSize);
        impl->probeData.len = PROBE_DATA_LEN_MAX;
    }
    else
    {
        impl->probeData.len = impl->probeSize;
    }

    tmpBuf = (cdx_char *)realloc(impl->probeData.buf, impl->probeData.len);
    if(tmpBuf == NULL)
    {
        logw("realloc fail.");
        goto err_out;
    }
    impl->probeData.buf = tmpBuf;

    while(size < impl->probeData.len)
    {
        if(impl->forceStopFlag == 1)
        {
            logd("forceStop.");
            goto err_out;
        }
        ret = CacheManagerGetProbeData(impl->cacheManager,
                    impl->probeData.len - size, impl->probeData.buf + size);
        if(ret < 0)
        {
            loge("get probe data fail.");
            goto err_out;
        }
        else if(ret == 0)
        {
            if(impl->totalSize > 0 && impl->totalSize == size)
            {
                impl->probeData.len = size;
                break;
            }
            if(impl->threadNum == 1)
            {
                cdx_int64 conSize = getContentSize(impl);
                pthread_mutex_lock(&impl->lock);
                if(impl->bDownloadFin == 1)
                {
                    if(size > 0)
                    {
                        logd("download finish.");
                        impl->probeData.len = size;
                        pthread_mutex_unlock(&impl->lock);
                        break;
                    }
                }
                pthread_mutex_unlock(&impl->lock);
                if(conSize > 0 && conSize == size)
                {
                    impl->probeData.len = size;
                    break;
                }
            }
            continue;
        }
        logv("get probe data need size=%d, size=%d, totSize=%lld",
            impl->probeData.len - size, ret, impl->totalSize);
        size += ret;
    }
    logd("probe data len %d", impl->probeData.len);
#if __SAVE_BITSTREAMS
    FILE *fp = fopen("/data/camera/probe.dat", "wb");
    fwrite(impl->probeData.buf, 1, impl->probeData.len, fp);
    fclose(fp);
#endif

    return 0;

err_out:
    if(impl->probeData.buf != NULL)
    {
        free(impl->probeData.buf);
        impl->probeData.buf = NULL;
    }
    return -1;
}

static cdx_int32 __CdxHttpStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    switch(cmd)
    {
        case STREAM_CMD_GET_CACHESTATE:
            return httpGetCacheState((struct StreamCacheStateS *)param, stream);

        case STREAM_CMD_SET_FORCESTOP:
            //logv("xxx STREAM_CMD_SET_FORCESTOP");
            return CdxHttpStreamForceStop(stream);

        case STREAM_CMD_CLR_FORCESTOP:
            return CdxHttpStreamClrForceStop(stream);
        case STREAM_CMD_SET_CALLBACK:
        {
            struct CallBack *cb = (struct CallBack *)param;
            impl->callback = cb->callback;
            impl->pUserData = cb->pUserData;
            return 0;
        }
        case STREAM_CMD_SET_ISHLS:
        {
            logd("======= set ishls");
            impl->isHls = 1;
            return 0;
        }
        case STREAM_CMD_SET_PROBE_SIZE:
        {
            impl->probeSize = *(cdx_uint32*)param;
            if(CdxGetProbeData(impl) < 0)
                return -1;
            else
                return 0;
        }
        default:
            break;
    }
    return 0;
}

static cdx_int32 __CdxHttpStreamSeek(CdxStreamT *stream, cdx_int64 offset, cdx_int32 whence)
{
    CdxHttpStreamImplT *impl;
    cdx_int64 fileLen = 0;
    cdx_int32 ret = 0;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->state = HTTP_STREAM_SEEKING;
    pthread_mutex_unlock(&impl->lock);

    fileLen = impl->totalSize;

    switch(whence)
    {
        case STREAM_SEEK_SET:
        {
            break;
        }
        case STREAM_SEEK_CUR:
        {
            offset += impl->readPos;
            break;
        }
        case STREAM_SEEK_END:
        {
            if(fileLen > 0)
            {
                offset += fileLen;
            }
            else
            {
                logw("bad fileLen, maybe live stream.");
                ret = -1;
                goto out;
            }

            break;
        }
        default:
        {
            loge("should not be here.");
            ret = -1;
            goto out;
        }
    }

    if((fileLen > 0 && offset > fileLen) || offset < 0)
    {
        loge("bad offset(%lld), fileLen(%lld), stream(%p)", offset, fileLen, stream);
        ret = -1;
        goto out;
    }

    if(offset == impl->readPos)
    {
        ret = 0;
        logv("offset == impl->readPos");
        goto out;
    }

    ret = CacheManagerSeekTo(impl->cacheManager, offset);
    if(ret < 0) //offset not in cache range, reconnect.
    {
        logv("seek reconnect, offset=%lld.", offset);
        while(1)
        {
            ret = seekReconnect(impl, offset);
            if(ret < 0)
            {
                logw("check.");
                goto out;
            }
            int tmpRet;
            pthread_mutex_lock(&impl->lock);
            while(impl->bPrepared != 1)
            {
                pthread_cond_wait(&impl->condPrepared, &impl->lock);
            }
            tmpRet = impl->retPrepared;
            pthread_mutex_unlock(&impl->lock);

            if(impl->forceStopFlag == 1)
            {
                ret = -1;
                goto out;
            }
            if(tmpRet == 0)
            {
                break;
            }
            else
            {
                usleep(50000);
            }
        }
    }
    else //offset is in cache range.
    {
        logv("seek in cache, offset=%lld", offset);
        impl->readPos = offset;
        ret = 0;
        goto out;
    }

out:
    pthread_mutex_lock(&impl->lock);
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return ret;
}

static cdx_int32 __CdxHttpStreamEos(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return (impl->bStreamReadEos == 1);
}

static cdx_int64 __CdxHttpStreamTell(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return impl->readPos;
}

static cdx_uint32 __CdxHttpStreamAttribute(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    CDX_CHECK(stream);
    cdx_uint32 flag = 0;

    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    if(impl->seekAble)
    {
        flag |= CDX_STREAM_FLAG_SEEK;
    }

    if (impl->isDTMB)
    {
        flag |= CDX_STREAM_FLAG_DTMB;
    }

    return flag|CDX_STREAM_FLAG_NET;
}
static cdx_int32 __CdxHttpStreamWrite(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CDX_UNUSE(stream);
    CDX_UNUSE(buf);
    CDX_UNUSE(len);

    return 0;
}

static cdx_int64 __CdxHttpStreamSize(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return impl->totalSize;
}
static cdx_int32 __CdxHttpStreamGetMetaData(CdxStreamT *stream, const cdx_char *key,
                                        void **pVal)
{
    CdxHttpStreamImplT *impl;
    CdxHttpHeaderT *httpHdr = NULL;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);
    httpHdr = (CdxHttpHeaderT *)impl->resHdr;

    if(strcmp(key, "uri") == 0)
    {
        *pVal = impl->sourceUri;
        logd("impl->sourceUri=%s", impl->sourceUri);
        return 0;
    }
    else if(strcmp(key, "extra-data") == 0)
    {
        if(impl->hfsContainer.extraData)
        {
            *pVal = &impl->hfsContainer;
            return 0;
        }
        else
            return -1;
    }
    else if(strcmp(key, "statusCode") == 0)
    {
        if(httpHdr)
        {
            logd("++++++ statusCode in http: %d", httpHdr->statusCode);
            *pVal = (void*)&httpHdr->statusCode;
            logd("++++++ *pVal = %p", *pVal);
        }
    }
    else
    {
        *pVal = CdxHttpGetField(httpHdr, key);
        if (!(*pVal))
        {
            return -1;
        }
    }
    return 0;
}

//decide how many threads to use.
static int threadNum(CdxHttpStreamImplT *impl)
{
    int ret = 0;

#if defined(USE_MULTI_THREAD)
    if(impl->chunkedFlag == 1)
        ret = 1;
    else if(impl->totalSize <= EACH_CACHE_SIZE || impl->seekAble == 0)
        ret = 1;
    else
    {
        ret = impl->totalSize/EACH_CACHE_SIZE +
            (impl->totalSize%EACH_CACHE_SIZE > 0 ? 1 : 0);
        if(ret > MAX_THREAD_NUM)
            ret = MAX_THREAD_NUM;
    }

#if __CONFIG_ZLIB
    if(impl->compressed == 1)
        ret = 1;
#endif

#else
    ret = 1;
    logd("ret = 1");
#endif

    if(impl->baseOffset > 0)
    {
        ret = 1;
    }

    logd("===== cFlag=%d, totalSize=%lld, sFlag=%d, comFlag=%d, tNum=%d =========",
        impl->chunkedFlag, impl->totalSize, impl->seekAble, impl->compressed, ret);
    return ret;
}
static int setHeaderToDlctx(DownloadCtxT *d, CdxHttpStreamImplT *impl)
{
    int i;
    CdxHttpHeaderFieldT *pHttpHeader = impl->pHttpHeader;
    if(impl->nHttpHeaderSize <= 0)
        return 0;

    d->nHttpHeaderSize = impl->nHttpHeaderSize;
    d->pHttpHeader = Palloc(impl->pool, d->nHttpHeaderSize * sizeof(CdxHttpHeaderFieldT));
    if(NULL == d->pHttpHeader)
    {
        loge("calloc failed.");
        return -1;
    }
    memset(d->pHttpHeader, 0x00, impl->nHttpHeaderSize * sizeof(CdxHttpHeaderFieldT));
    for(i = 0; i < d->nHttpHeaderSize; i++)
    {
        d->pHttpHeader[i].key = (const char*)Pstrdup(impl->pool, pHttpHeader[i].key);
        if(d->pHttpHeader[i].key == NULL)
        {
            loge("dup key failed.");
            clearHttpHeader(d->pHttpHeader, d->nHttpHeaderSize, d->pool);
            return -1;
        }
        d->pHttpHeader[i].val = (const char*)Pstrdup(impl->pool, pHttpHeader[i].val);
        if(d->pHttpHeader[i].val == NULL)
        {
            loge("dup val failed.");
            clearHttpHeader(d->pHttpHeader, d->nHttpHeaderSize, d->pool);
            return -1;
        }
        logv("============ impl->pHttpHeader[i].val(%s):%s",
            d->pHttpHeader[i].key, d->pHttpHeader[i].val);
    }
    return 0;
}

static int setUrlToDlctx(CdxUrlT *dst, CdxUrlT *src)
{
    if(src->url != NULL)
    {
        dst->url = strdup(src->url);
        CDX_CHECK(dst->url);
    }
    if(src->protocol != NULL)
    {
        dst->protocol = strdup(src->protocol);
        CDX_CHECK(dst->protocol);
    }
    if(src->hostname != NULL)
    {
        dst->hostname = strdup(src->hostname);
        CDX_CHECK(dst->hostname);
    }
    if(src->file != NULL)
    {
        dst->file = strdup(src->file);
        CDX_CHECK(dst->file);
    }
    dst->port = src->port;
    if(src->username != NULL)
    {
        dst->username = strdup(src->username);
        CDX_CHECK(dst->username);
    }
    if(src->password != NULL)
    {
        dst->password = strdup(src->password);
        CDX_CHECK(dst->password);
    }
    if(src->noauth_url != NULL)
    {
        dst->noauth_url = strdup(src->noauth_url);
        CDX_CHECK(dst->noauth_url);
    }

    return 0;
}

static int setInfoToDlctx(DownloadCtxT *d, CdxHttpStreamImplT *impl)
{
    int i;

    d->ua = impl->ua;
    d->pool = impl->pool;
    d->sourceUri = Pstrdup(impl->pool, impl->sourceUri); //should free
    if(d->sourceUri == NULL)
    {
        loge("dup failed.");
        return -1;
    }

    d->cacheManager = impl->cacheManager;
    d->oneThread = (impl->threadNum == 1) ? 1 : 0;
    i = setHeaderToDlctx(d, impl);
    if(i < 0)
    {
        loge("set header failed.");
        Pfree(d->pool, d->sourceUri);
        d->sourceUri = NULL;
        return -1;
    }

    d->urlT = calloc(1, sizeof(CdxUrlT));
    if(d->urlT == NULL)
    {
        loge("malloc size=%d", (int)sizeof(CdxUrlT));
        clearHttpHeader(d->pHttpHeader, d->nHttpHeaderSize, d->pool);
        d->pHttpHeader = NULL;
        Pfree(d->pool, d->sourceUri);
        d->sourceUri = NULL;
        return -1;
    }

    d->baseOffset = impl->baseOffset;
    setUrlToDlctx(d->urlT, impl->url);

    return 0;
}

static cdx_int32 __CdxHttpStreamConnect(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    cdx_int32 result = 0;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->state = HTTP_STREAM_PREPARING;
    pthread_mutex_unlock(&impl->lock);

//cmcc, aliyunos not need
#if defined(USE_MULTI_THREAD)
    memset(&impl->dlct, 0x00, sizeof(DownloadCtxT));
    if(setInfoToDlctx(&impl->dlct, impl) < 0)
    {
        loge("set info failed.");
        impl->ioState = CDX_IO_STATE_ERROR;
        goto __exit;
    }
    impl->dlct.oneThread = 1;
    result = handlePrepare(&impl->dlct);
    if(result < 0)
    {
        loge("handlePrepare failed.");
        impl->ioState = CDX_IO_STATE_ERROR;
        clrDownloadInfo(&impl->dlct);
        Pfree(impl->dlct.pool, impl->dlct.sourceUri);
        impl->dlct.sourceUri = NULL;
        clearHttpHeader(impl->dlct.pHttpHeader, impl->dlct.nHttpHeaderSize, impl->dlct.pool);
        impl->dlct.pHttpHeader = NULL;
        CdxUrlFree(impl->dlct.urlT);
        impl->dlct.urlT = NULL;
        goto __exit;
    }

    impl->totalSize = impl->dlct.totalSize;
    impl->seekAble = impl->dlct.seekAble;
#if __CONFIG_ZLIB
    impl->compressed = impl->dlct.compressed;
#endif
    impl->chunkedFlag = impl->dlct.chunkedFlag;

    Pfree(impl->dlct.pool, impl->dlct.sourceUri);
    impl->dlct.sourceUri = NULL;
    clearHttpHeader(impl->dlct.pHttpHeader, impl->dlct.nHttpHeaderSize, impl->dlct.pool);
    impl->dlct.pHttpHeader = NULL;
    clearHttpExtraDataContainer(&impl->dlct.hfsContainer);
    CdxUrlFree(impl->dlct.urlT);
    impl->dlct.urlT = NULL;
    clrDownloadInfo(&impl->dlct);
#endif

    logv("impl->baseOffset=%lld", impl->baseOffset);
    cdx_int64 totSize = impl->totalSize;
    if(impl->compressed || impl->chunkedFlag)
        totSize = -1;
    impl->cacheManager = CacheManagerCreate(MAX_CACHE_NUM, EACH_CACHE_SIZE,
                                            totSize, CACHE_NODE_SIZE, 0/*impl->baseOffset*/);
    if(impl->cacheManager == NULL)
    {
        loge("create cache manager fail.");
        impl->ioState = CDX_IO_STATE_ERROR;
        goto __exit;
    }

    //decide download thread number
    int parts, i;
    parts = threadNum(impl);

    impl->dlCtx = (DownloadCtxT *)calloc(parts, sizeof(DownloadCtxT));
    impl->threadNum = parts;
    for(i = 0; i < parts; i++)
    {
        impl->dlCtx[i].id = i;
        impl->dlCtx[i].bufSize = EACH_CACHE_SIZE;
        impl->dlCtx[i].mq = AwMessageQueueCreate(4, "httpDownload");
        if(impl->dlCtx[i].mq == NULL)
        {
            loge("create message queue failed.");
            impl->ioState = CDX_IO_STATE_ERROR;
            goto __exit;
        }
        sem_init(&impl->dlCtx[i].semQuit, 0, 0);

        impl->dlCtx[i].bandWidthEst = CreateBandwidthEst(100);
        if(impl->dlCtx[i].bandWidthEst == NULL)
        {
            loge("create bandwidthEst failed.");
            impl->ioState = CDX_IO_STATE_ERROR;
            goto __exit;
        }

        if(setInfoToDlctx(&(impl->dlCtx[i]), impl) < 0)
        {
            logd("set info failed.");
            impl->ioState = CDX_IO_STATE_ERROR;
            goto __exit;
        }

        result = pthread_create(&(impl->dlCtx[i].threadId), NULL, partDownload,
                    &(impl->dlCtx[i]));
        if(result != 0)
        {
            loge("create thread failed.");
            if(impl->dlCtx[i].mq != NULL)
            {
                AwMessageQueueDestroy(impl->dlCtx[i].mq);
                impl->dlCtx[i].mq = NULL;
            }
            impl->ioState = CDX_IO_STATE_ERROR;
            goto __exit;
        }
        httpSetCallback(&(impl->dlCtx[i]), httpCallbackProcess, impl);
        httpStreamPrepare(&(impl->dlCtx[i]));
    }

    int tmpRet = 0;
    pthread_mutex_lock(&impl->lock);
    while(impl->bPrepared != 1)
    {
        pthread_cond_wait(&impl->condPrepared, &impl->lock);
    }
    tmpRet = impl->retPrepared;
    pthread_mutex_unlock(&impl->lock);
    if(tmpRet != 0)
    {
        logw("partDownload failed.");
        impl->ioState = CDX_IO_STATE_ERROR;
        goto __exit;
    }

    cdx_int64 t0=CdxGetNowUs();
    result = CdxGetProbeData(impl);
    if(result < 0)
    {
        loge("get probe data failed.");
        impl->ioState = CDX_IO_STATE_ERROR;
        goto __exit;
    }
    logv("CdxGetProbeData time:%lld ms, %u Bytes", (CdxGetNowUs()-t0)/1000, impl->probeData.len);

__exit:
    pthread_mutex_lock(&impl->lock);
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return (impl->ioState == CDX_IO_STATE_ERROR || impl->forceStopFlag == 1) ? -1 : 0;
}

static cdx_int32 __CdxHttpStreamClose(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    int i;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    logv("xxxx http close begin. stream(%p)", stream);

    CdxHttpStreamForceStop(stream);

    for(i = 0; i < impl->threadNum; i++)
    {
        httpStreamQuit(&impl->dlCtx[i]);
        if(impl->dlCtx[i].mq != NULL)
        {
            AwMessageQueueDestroy(impl->dlCtx[i].mq);
            impl->dlCtx[i].mq = NULL;
        }
        sem_destroy(&impl->dlCtx[i].semQuit);
        logv("xxxx http close (%d)", i);
        pthread_join(impl->dlCtx[i].threadId, NULL);
        logv("xxxx http close end. (%d)", i);
        clearHttpHeader(impl->dlCtx[i].pHttpHeader, impl->dlCtx[i].nHttpHeaderSize,
            impl->dlCtx[i].pool);
        impl->dlCtx[i].pHttpHeader = NULL;
        CdxUrlFree(impl->dlCtx[i].urlT);
        impl->dlCtx[i].urlT = NULL;
        Pfree(impl->dlCtx[i].pool, impl->dlCtx[i].sourceUri);
        clearHttpExtraDataContainer(&impl->dlCtx[i].hfsContainer);
        if(impl->dlCtx[i].httpDataBufferChunked != NULL)
        {
            free(impl->dlCtx[i].httpDataBufferChunked);
            impl->dlCtx[i].httpDataBufferChunked = NULL;
        }
        DestroyBandwidthEst(impl->dlCtx[i].bandWidthEst);
        impl->dlCtx[i].bandWidthEst = NULL;

#if __SAVE_BITSTREAMS
        if(impl->dlCtx[i].fp_download != NULL)
            fclose(impl->dlCtx[i].fp_download);
#endif
    }

    CacheManagerDestroy(impl->cacheManager);
    impl->cacheManager = NULL;

    if(impl->dlCtx != NULL)
    {
        free(impl->dlCtx);
    }
    if(impl->url != NULL)
    {
        CdxUrlFree(impl->url);
        impl->url = NULL;
    }

    if(impl->resHdr != NULL)
    {
        CdxHttpFree(impl->resHdr);
        impl->resHdr = NULL;
    }
    ClearDataSourceFields(impl);
    clearHttpExtraDataContainer(&impl->hfsContainer);
    if(impl->probeData.buf)
    {
        free(impl->probeData.buf);
        impl->probeData.buf = NULL;
    }
#if __CONFIG_ZLIB
    inflateEnd(&impl->inflateStream);
    free(impl->inflateBuffer);
#endif
    pthread_mutex_destroy(&impl->lock);
    pthread_cond_destroy(&impl->cond);
    pthread_cond_destroy(&impl->condPrepared);

    if(impl->pool != NULL)
    {
        AwPoolDestroy(impl->pool);
    }
    free(impl);

    logv("xxxx http close finish.");
    return 0;
}

static struct CdxStreamOpsS CdxHttpStreamOps =
{
    .connect      = __CdxHttpStreamConnect,
    .getProbeData = __CdxHttpStreamGetProbeData,
    .read         = __CdxHttpStreamRead,
    .write        = __CdxHttpStreamWrite,
    .close        = __CdxHttpStreamClose,
    .getIOState   = __CdxHttpStreamGetIOState,
    .attribute    = __CdxHttpStreamAttribute,
    .control      = __CdxHttpStreamControl,

    .getMetaData  = __CdxHttpStreamGetMetaData,
    .seek         = __CdxHttpStreamSeek,
    .seekToTime   = NULL,
    .eos          = __CdxHttpStreamEos,
    .tell         = __CdxHttpStreamTell,
    .size         = __CdxHttpStreamSize,
};

static CdxHttpStreamImplT *CreateHttpStreamImpl(void)
{
    CdxHttpStreamImplT *impl = NULL;

    impl = (CdxHttpStreamImplT *)calloc(1, sizeof(*impl));
    if(!impl)
    {
        loge("malloc failed, size(%d)", (int)sizeof(*impl));
        return NULL;
    }

    impl->base.ops = &CdxHttpStreamOps;

    return impl;
}

CdxStreamT *__CdxHttpStreamCreate(CdxDataSourceT *source)
{
    CdxHttpStreamImplT *impl = NULL;
    CdxUrlT* url = NULL;
    cdx_int32 result;
    AwPoolT *pool;

    logi("source uri:(%s)", source->uri);

    impl = CreateHttpStreamImpl();
    if(NULL == impl)
    {
        loge("CreateHttpStreamImpl failed.");
        return NULL;
    }

    pool = AwPoolCreate(NULL);
    if(pool == NULL)
    {
        loge("pool is NULL.");
        free(impl);
        return NULL;
    }

    impl->pool = pool;

    url = CdxUrlNew(source->uri);
    if(url == NULL)
    {
        loge("CdxUrlNew failed.");
        goto err_out;
    }
    impl->url = url;

    result = SetDataSourceFields(source, impl);
    if(result < 0)
    {
        loge("Set datasource failed.");
        goto err_out;
    }

    impl->baseOffset = source->offset > 0 ? source->offset : 0;
    impl->ua = GetUA(impl->nHttpHeaderSize, impl->pHttpHeader);//for ua
    impl->ioState = CDX_IO_STATE_INVALID;
    impl->state = HTTP_STREAM_IDLE;
    impl->isDTMB = CDX_FALSE;

    if (strstr(url->file,"aw_dtmb_http.ts"))
    {
        impl->isDTMB = CDX_TRUE;
        CDX_LOGD("It is a dtmb stream!");
    }

    pthread_mutex_init(&impl->lock, NULL);
    pthread_cond_init(&impl->cond, NULL);
    pthread_cond_init(&impl->condPrepared, NULL);

    logd("http stream open.");
    return &impl->base;

err_out:
    if(impl != NULL)
    {
        if(impl->url)
        {
            CdxUrlFree(impl->url);
            impl->url = NULL;
        }
        ClearDataSourceFields(impl);
        AwPoolDestroy(impl->pool);
        free(impl);
    }
    return NULL;
}

CdxStreamCreatorT httpStreamCtor =
{
    .create = __CdxHttpStreamCreate
};

