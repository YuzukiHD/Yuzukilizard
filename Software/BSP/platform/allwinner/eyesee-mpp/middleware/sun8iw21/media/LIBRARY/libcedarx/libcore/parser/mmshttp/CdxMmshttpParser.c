/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmshttpParser.c
 * Description : parser for mms stream whose url is started with "http"
 * History :
 *
 */

#include <CdxMmshttpParser.h>

enum CdxMmshttpStatusE
{
    CDX_MMSH_INITIALIZED, /*control , getMediaInfo, not prefetch or read, seekTo,.*/
    CDX_MMSH_IDLE,
    CDX_MMSH_PREFETCHING,
    CDX_MMSH_PREFETCHED,
    CDX_MMSH_READING,
    CDX_MMSH_SEEKING,
    CDX_MMSH_EOS,
};

static void MmshParserClose(struct CdxMmshttpParser* impl)
{
    if (impl)
    {
        impl->exitFlag = 1;
        if(impl->datasource)
        {
            if(impl->datasource->uri)
            {
                free(impl->datasource->uri);
                impl->datasource->uri = NULL;
            }

            free(impl->datasource);
            impl->datasource = NULL;
        }
        if(impl->stream) CdxStreamClose(impl->stream); // close the Ref http stream
        if(impl->parserinfoNext) CdxParserClose(impl->parserinfoNext);
        if(impl->buffer) free(impl->buffer);
        pthread_mutex_destroy(&impl->mutex);
        free(impl);
    }
}

static cdx_int32 __CdxMmshttpParserClose(CdxParserT *parser)
{
    struct CdxMmshttpParser* impl = (struct CdxMmshttpParser*)parser;
    impl->exitFlag = 1;
    //pthread_join(impl->thread, NULL);
    MmshParserClose(impl);
    return 0;
}

static cdx_int32 __CdxMmshttpParserPrefetch(CdxParserT *parser, CdxPacketT * pkt)
{
    struct CdxMmshttpParser* impl = (struct CdxMmshttpParser*)parser;
    int ret;

    if(impl->status != CDX_MMSH_IDLE && impl->status != CDX_MMSH_PREFETCHED)
    {
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    pthread_mutex_lock(&impl->mutex);
    impl->status = CDX_MMSH_PREFETCHING;
    ret = CdxParserPrefetch(impl->parserinfoNext, pkt);
    if(ret < 0)
    {
        //impl->mErrno = PSR_EOS;
        impl->status = CDX_MMSH_IDLE;
        pthread_mutex_unlock(&impl->mutex);
        return -1;
    }

    impl->status = CDX_MMSH_PREFETCHED;
    pthread_mutex_unlock(&impl->mutex);
    return ret;
}

static cdx_int32 __CdxMmshttpParserRead(CdxParserT *parser, CdxPacketT *cdx_pkt)
{
    int ret;
    struct CdxMmshttpParser* impl = (struct CdxMmshttpParser*)parser;
    if(impl->status != CDX_MMSH_PREFETCHED)
    {
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    ret = CdxParserRead(impl->parserinfoNext, cdx_pkt);
    impl->status = CDX_MMSH_IDLE;
    return ret;
}

static cdx_int32 __CdxMmshttpParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT * mediaInfo)
{
    struct CdxMmshttpParser* impl = (struct CdxMmshttpParser*)parser;
    if(impl->status != CDX_MMSH_IDLE)
    {
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    return CdxParserGetMediaInfo(impl->parserinfoNext, mediaInfo);
}

cdx_int32 __CdxMmshttpParserForceStop(CdxParserT *parser)
{
    struct CdxMmshttpParser* impl;
    int ret;

    impl = (struct CdxMmshttpParser*)parser;
    if(!impl)
    {
        CDX_LOGE("mms file parser has not been initiated!");
        return -1;
    }
    impl->exitFlag = 1;

    pthread_mutex_lock(&impl->mutex);
    ret = CdxParserForceStop(impl->parserinfoNext);
    if(ret < 0)
    {
        CDX_LOGE("mms parser force stop error!");
        pthread_mutex_unlock(&impl->mutex);
        return -1;
    }
    ret = CdxStreamForceStop(impl->stream);
    if(ret < 0)
    {
        CDX_LOGE("mms parser force stop error!");
        pthread_mutex_unlock(&impl->mutex);
        return -1;
    }
    pthread_mutex_unlock(&impl->mutex);

    while((impl->status != CDX_MMSH_IDLE) && (impl->status != CDX_MMSH_PREFETCHED))
    {
        usleep(2000);
    }

    impl->mErrno = PSR_USER_CANCEL;
    return 0;
}

cdx_int32 __CdxMmshttpParserClrForceStop(CdxParserT *parser)
{
    struct CdxMmshttpParser* impl;
    int ret;

    impl = (struct CdxMmshttpParser*)parser;
    if(!impl)
    {
        CDX_LOGE("mms file parser has not been initiated!");
        return -1;
    }
    impl->exitFlag = 0;

    pthread_mutex_lock(&impl->mutex);
    ret = CdxParserClrForceStop(impl->parserinfoNext);
    if(ret < 0)
    {
        CDX_LOGE("mms parser force stop error!");
        return -1;
    }
    ret = CdxStreamClrForceStop(impl->stream);
    if(ret < 0)
    {
        CDX_LOGE("mms parser force stop error!");
        return -1;
    }
    pthread_mutex_unlock(&impl->mutex);

    impl->mErrno = PSR_OK;
    return 0;
}

static int __CdxMmshttpParserControl(CdxParserT *parser, int cmd, void *param)
{
    struct CdxMmshttpParser* impl = (struct CdxMmshttpParser*)parser;

    switch(cmd)
    {
        case CDX_PSR_CMD_SET_FORCESTOP:
            return __CdxMmshttpParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return __CdxMmshttpParserClrForceStop(parser);
        default:
            return CdxParserControl(impl->parserinfoNext, cmd, param);
    }

    return 0;
}

cdx_int32 __CdxMmshttpParserSeekTo(CdxParserT *parser, cdx_int64  timeUs, SeekModeType seekModeType)
{
    int ret;
    struct CdxMmshttpParser* impl = (struct CdxMmshttpParser*)parser;
    if(impl->exitFlag == 1)
    {
        return -1;
    }

    pthread_mutex_lock(&impl->mutex);
    impl->status = CDX_MMSH_SEEKING;
    ret = CdxParserSeekTo(impl->parserinfoNext, timeUs, seekModeType);
    if(ret < 0)
    {
        impl->mErrno = PSR_UNKNOWN_ERR;
        impl->status = CDX_MMSH_IDLE;
    }

    impl->mErrno = PSR_OK;
    impl->status = CDX_MMSH_IDLE;
    pthread_mutex_unlock(&impl->mutex);
    return ret;
}

cdx_uint32 __CdxMmshttpParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    struct CdxMmshttpParser* tmpMmsPsr = (struct CdxMmshttpParser*)parser;
    (void)tmpMmsPsr;

    return -1;
}

cdx_int32 __CdxMmshttpParserGetStatus(CdxParserT *parser)
{
    struct CdxMmshttpParser* tmpMmsPsr = (struct CdxMmshttpParser*)parser;
    return tmpMmsPsr->mErrno;
}

static int __CdxMmshttpParserInit(CdxParserT *parser)
{
    struct CdxMmshttpParser* impl = (struct CdxMmshttpParser*)parser;
    int ret;

    impl->size = CdxStreamSize(impl->stream);
    impl->buffer = malloc(impl->size);
    if(!impl->buffer)
        return -1;
    int readSize = CdxStreamRead(impl->stream, impl->buffer, (int)impl->size);
    if(readSize < 0)
    {
        impl->mErrno = PSR_OPEN_FAIL;
        return -1;
    }
    CDX_LOGD("--- size = %lld, readSize = %d", impl->size, readSize);

    char* p;
    char* p1;
    const char ctrl[] = {0x0d, 0x0a};
    char url_ref1[1024];
    char url_mms[1024] = "mms";
    p = strstr(impl->buffer, "Ref1=");
    if(!p)
    {
        CDX_LOGE("cannot find Ref");
        impl->mErrno = PSR_OPEN_FAIL;
        return -1;
    }
    p += 5; //get the url

    p1 = strstr(p, ctrl); // the Ref1 url is end of  '0x0d0a'
    if(!p1)
    {
        CDX_LOGE("cannot find 0x0D0A");
        impl->mErrno = PSR_OPEN_FAIL;
        return -1;
    }
    int url_len = p1-p;
    if(url_len > 1024)
    {
        CDX_LOGE("the url length<%d> is large than 1024, we need malloc a big array!", url_len);
        impl->mErrno = PSR_OPEN_FAIL;
        return -1;
    }

    memcpy(url_ref1, p, url_len);
    url_ref1[url_len] = '\0';

    strcat(url_mms, url_ref1);
    CDX_LOGD("---- mms url: <%s>", url_mms);

    impl->datasource->uri = malloc(sizeof(url_mms));
    memcpy(impl->datasource->uri, url_mms, 1024);

    ret = CdxParserPrepare(impl->datasource, 0, &impl->mutex, &impl->exitFlag,
                                    &impl->parserinfoNext, &impl->streamNext, NULL, NULL);
    if(ret < 0)
    {
        CDX_LOGE("parser prepare failed");
        return -1;
    }

    impl->mErrno = PSR_OK;
    impl->status = CDX_MMSH_IDLE;
    return 0;
}

static struct CdxParserOpsS mmshttpParserOps =
{
    .init           = __CdxMmshttpParserInit,
    .control         = __CdxMmshttpParserControl,
    .prefetch         = __CdxMmshttpParserPrefetch,
    .read             = __CdxMmshttpParserRead,
    .getMediaInfo     = __CdxMmshttpParserGetMediaInfo,
    .close             = __CdxMmshttpParserClose,
    .seekTo         = __CdxMmshttpParserSeekTo,
    .attribute        = __CdxMmshttpParserAttribute,
    .getStatus        = __CdxMmshttpParserGetStatus
};

static CdxParserT *__CdxMmshttpParserOpen(CdxStreamT *stream, cdx_uint32 flag)
{
    cdx_int32            ret;
    struct CdxMmshttpParser *impl = NULL;

    impl = CdxMalloc(sizeof(struct CdxMmshttpParser));
    if(impl == NULL) goto error;
    memset(impl, 0x00, sizeof(*impl));
    (void)flag;

    impl->parserinfo.ops = &mmshttpParserOps;
    impl->stream = stream;
    impl->mErrno = PSR_INVALID;
    impl->status = CDX_MMSH_INITIALIZED;
    pthread_mutex_init(&impl->mutex, NULL);

    impl->datasource = malloc(sizeof(CdxDataSourceT));
    if(impl->datasource == NULL)
        goto error;
    memset(impl->datasource, 0, sizeof(CdxDataSourceT));

    //ret = pthread_create(&impl->thread, NULL, openThread, (void*)impl) ;
    //if(ret != 0)
    //{
    //    CDX_LOGE("cannot create probedata thread");
    //    impl->thread = (pthread_t)0;
    //    goto error;
    //}

    return &impl->parserinfo;

error:
    if(impl)
    {
        MmshParserClose(impl);
        impl = NULL;
    }
    return NULL;
}

static cdx_uint32 __CdxMmshttpParserProbe(CdxStreamProbeDataT *probeData)
{
    char* mmsHeader = "[Reference]";
    /* check file header */

    if(probeData->len < 11)
    {
        return 0;
    }

    if (memcmp(probeData->buf, mmsHeader, 11))
    {
        return 0;
    }
    CDX_LOGD(" --- mmshttp parser");
    return 100;
}

CdxParserCreatorT mmshttpParserCtor =
{
    .create = __CdxMmshttpParserOpen,
    .probe     = __CdxMmshttpParserProbe
};
