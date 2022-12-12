/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxStream.c
 * Description : Stream base
 * History :
 *
 */

#include <CdxTypes.h>
#include <cdx_log.h>
#include <CdxMemory.h>

#include <CdxStream.h>
#include "cdx_config.h"
#include <CdxList.h>

/*
#define STREAM_DECLARE(scheme) \
    extern CdxStreamCreatorT cdx_##scheme##_stream_ctor

#define STREAM_REGISTER(scheme) \
    {#scheme, &cdx_##scheme##_stream_ctor}
*/

struct CdxStreamListS
{
    CdxListT list;
    int size;
};

static int streamInit = 0;

struct CdxStreamListS streamList;

extern CdxStreamCreatorT fileStreamCtor;

#ifdef __ANDROID__
extern CdxStreamCreatorT rtspStreamCtor;
#endif

extern CdxStreamCreatorT httpStreamCtor;
extern CdxStreamCreatorT tcpStreamCtor;
#ifdef __ANDROID__
extern CdxStreamCreatorT rtmpStreamCtor;
extern CdxStreamCreatorT mmsStreamCtor;
#endif
extern CdxStreamCreatorT udpStreamCtor;
#ifdef __ANDROID__
extern CdxStreamCreatorT rtpStreamCreator;
extern CdxStreamCreatorT customerStreamCtor;
extern CdxStreamCreatorT sslStreamCtor;
extern CdxStreamCreatorT aesStreamCtor;
extern CdxStreamCreatorT bdmvStreamCtor;
extern CdxStreamCreatorT widevineStreamCtor;
extern CdxStreamCreatorT videoResizeStreamCtor;
extern CdxStreamCreatorT DTMBStreamCtor;
extern CdxStreamCreatorT dataSourceStreamCtor;
#endif

struct CdxStreamNodeS
{
    CdxListNodeT node;
    const cdx_char *scheme;
    CdxStreamCreatorT *creator;
};

int AwStreamRegister(CdxStreamCreatorT *creator, cdx_char *type)
{
    struct CdxStreamNodeS *streamNode;

    streamNode = malloc(sizeof(*streamNode));
    streamNode->creator = creator;
    streamNode->scheme = type;

    if(streamInit == 0)
    {
        CdxListInit(&streamList.list);
        streamList.size = 0;
        streamInit = 1;
    }

    CdxListAddTail(&streamNode->node, &streamList.list);
    streamList.size++;
    return 0;
}

static void AwStreamInit(void) __attribute__((constructor));
void AwStreamInit(void)
{
    CDX_LOGD("aw stream init...");

    AwStreamRegister(&fileStreamCtor,"fd");
    AwStreamRegister(&fileStreamCtor,"file");
#ifdef __ANDROID__
    AwStreamRegister(&rtspStreamCtor,"rtsp");
#endif
    //AwStreamRegister(&httpStreamCtor,"http");
#ifdef __ANDROID__
    AwStreamRegister(&httpStreamCtor,"https");
#endif
    //AwStreamRegister(&tcpStreamCtor,"tcp");
#ifdef __ANDROID__
    AwStreamRegister(&rtmpStreamCtor,"rtmp");
    AwStreamRegister(&mmsStreamCtor,"mms");
    AwStreamRegister(&mmsStreamCtor,"mmsh");
    AwStreamRegister(&mmsStreamCtor,"mmst");
    AwStreamRegister(&mmsStreamCtor,"mmshttp");
#endif
    //AwStreamRegister(&udpStreamCtor,"udp");
#ifdef __ANDROID__
    AwStreamRegister(&customerStreamCtor,"customer");
    AwStreamRegister(&sslStreamCtor,"ssl");
    AwStreamRegister(&aesStreamCtor,"aes");
    AwStreamRegister(&bdmvStreamCtor,"bdmv");
    AwStreamRegister(&DTMBStreamCtor,"dtmb");
#endif
#ifdef __ANDROID__
    AwStreamRegister(&widevineStreamCtor,"widevine");
    AwStreamRegister(&videoResizeStreamCtor,"videoResize");
    AwStreamRegister(&dataSourceStreamCtor,"datasource");
#endif
    CDX_LOGD("stream list size:%d",streamList.size);
    return ;
}

CdxStreamT *CdxStreamCreate(CdxDataSourceT *source)
{
    cdx_char scheme[24] = {0};
    cdx_char *colon;
    CdxStreamCreatorT *ctor = NULL;
    struct CdxStreamNodeS *streamNode;

    colon = strchr(source->uri, ':');
    CDX_CHECK(colon && (colon - source->uri < 24));
    memcpy(scheme, source->uri, colon - source->uri);

    CdxListForEachEntry(streamNode, &streamList.list, node)
    {
        CDX_CHECK(streamNode->creator);
        if (strcasecmp(streamNode->scheme, scheme) == 0)
        {
            ctor = streamNode->creator;
            break;
        }
    }

    if (NULL == ctor)
    {
        CDX_LOGE("unsupport stream. scheme(%s)", scheme);
        return NULL;
    }

    CDX_CHECK(ctor->create);
    CdxStreamT *stream = ctor->create(source);
    if (!stream)
    {
        CDX_LOGE("open stream fail, uri(%s)", source->uri);
        return NULL;
    }

    return stream;
}

int CdxStreamOpen(CdxDataSourceT *source, pthread_mutex_t *mutex, cdx_bool *exit,
        CdxStreamT **stream, ContorlTask *streamTasks)
{
    if(mutex)
        pthread_mutex_lock(mutex);
    if(exit && *exit)
    {
        CDX_LOGW("open stream user cancel.");
        if(mutex) pthread_mutex_unlock(mutex);
        return -1;
    }
    *stream = CdxStreamCreate(source);
    if(mutex)
        pthread_mutex_unlock(mutex);
    if (!*stream)
    {
        CDX_LOGW("open stream failure.");
        return -1;
    }
    int ret;
    while(streamTasks)
    {
        ret = CdxStreamControl(*stream, streamTasks->cmd, streamTasks->param);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamControl fail, cmd=%d", streamTasks->cmd);
        }
        streamTasks = streamTasks->next;
    }
    return CdxStreamConnect(*stream);
}
