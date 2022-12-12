/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : WidevineStream.c
* Description : just for google widevine stream
* History :
*/

#define LOG_TAG "WidevineStream"
#include <cdx_log.h>
#include <CdxStream.h>
#include <CdxMemory.h>

typedef struct WidevineStream
{
    CdxStreamT base;
    CdxStreamT *child;
    CdxDataSourceT dataSource;//记录原始的datasource
    //char *uri;//访问的uri
}WidevineStream;

void ClrExtraDataOfDataSouce(CdxDataSourceT *cdxDataSource)
{
    if(!cdxDataSource->extraData)
    {
        cdxDataSource->extraDataType = EXTRA_DATA_UNKNOWN;
        return ;
    }
    enum DSExtraDataTypeE extraDataType = cdxDataSource->extraDataType;
    void *extraData = cdxDataSource->extraData;
    if(extraDataType == EXTRA_DATA_HTTP_HEADER)
    {
        CdxHttpHeaderFieldsT *hdr = (CdxHttpHeaderFieldsT *)extraData;
        if(hdr->pHttpHeader)
        {
            cdx_int32 i = 0;
            for(; i < hdr->num; i++)
            {
                if((hdr->pHttpHeader + i)->key)
                {
                    free((char *)((hdr->pHttpHeader + i)->key));
                }
                if((hdr->pHttpHeader + i)->val)
                {
                    free((char *)((hdr->pHttpHeader + i)->val));
                }
            }
            free(hdr->pHttpHeader);
            hdr->pHttpHeader = NULL;
        }
        free(hdr);
    }
    else
    {
        CDX_LOGE("extraDataType=%d, it is not supported.", extraDataType);
    }
    cdxDataSource->extraData = NULL;
    cdxDataSource->extraDataType = EXTRA_DATA_UNKNOWN;
    return;
}
int ClrDataSource(CdxDataSourceT *source)
{
    if(source->uri)
    {
        free(source->uri);
        source->uri = NULL;
    }
    ClrExtraDataOfDataSouce(source);
    return 0;
}
int DupExtraDataOfDataSouce(CdxDataSourceT *dest, CdxDataSourceT *src)
{
    if(src->extraData)
    {
        enum DSExtraDataTypeE extraDataType = src->extraDataType;
        void *extraData = src->extraData;
        if(extraDataType == EXTRA_DATA_HTTP_HEADER)
        {
            dest->extraData = (CdxHttpHeaderFieldsT *)malloc(sizeof(CdxHttpHeaderFieldsT));
            CdxHttpHeaderFieldsT *hdr = (CdxHttpHeaderFieldsT *)(extraData);
            CdxHttpHeaderFieldsT *hdr1 = (CdxHttpHeaderFieldsT *)(dest->extraData);
            CDX_FORCE_CHECK(hdr && hdr1);
            hdr1->num = hdr->num;
            hdr1->pHttpHeader = CdxMalloc(hdr1->num * sizeof(CdxHttpHeaderFieldT));
            int i;
            for(i = 0; i < hdr1->num; i++)
            {
                (hdr1->pHttpHeader + i)->key = strdup((hdr->pHttpHeader + i)->key);
                (hdr1->pHttpHeader + i)->val = strdup((hdr->pHttpHeader + i)->val);

                CDX_LOGD("extraDataContainer %s %s",
                         (hdr1->pHttpHeader + i)->key,(hdr1->pHttpHeader + i)->val);
            }
        }
        else
        {
            CDX_LOGE("extraDataType=%d, it is not supported.", extraDataType);
        }
    }
    return 0;
}
int DupDataSouce(CdxDataSourceT *dest, CdxDataSourceT *src)
{
    ClrDataSource(dest);
    dest->uri = strdup(src->uri);
    return DupExtraDataOfDataSouce(dest, src);
}

inline static CdxStreamProbeDataT *WidevineStreamGetProbeData(CdxStreamT *stream)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamGetProbeData(widevine->child);
}

inline static cdx_int32 WidevineStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamRead(widevine->child, buf, len);
}

inline static cdx_int32 WidevineStreamConnect(CdxStreamT *stream)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamConnect(widevine->child);
}

inline static cdx_int32 WidevineStreamClose(CdxStreamT *stream)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    CdxStreamClose(widevine->child);
    ClrDataSource(&widevine->dataSource);
    free(widevine);
    return 0;
}

inline static cdx_int32 WidevineStreamGetIOState(CdxStreamT *stream)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamGetIoState(widevine->child);
}

inline static cdx_uint32 WidevineStreamAttribute(CdxStreamT *stream)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamAttribute(widevine->child);
}

inline static cdx_int32 WidevineStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamControl(widevine->child, cmd, param);
}
inline static cdx_int32 WidevineStreamSeek(CdxStreamT *stream, cdx_int64 offset, cdx_int32 whence)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamSeek(widevine->child, offset, whence);
}
inline static cdx_int32 WidevineStreamEos(CdxStreamT *stream)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamEos(widevine->child);
}
inline static cdx_int64 WidevineStreamSize(CdxStreamT *stream)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamSize(widevine->child);
}

inline static cdx_int64 WidevineStreamTell(CdxStreamT *stream)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    return CdxStreamTell(widevine->child);
}

inline static cdx_int32 WidevineStreamGetMetaData(CdxStreamT *stream, const cdx_char *key,
                                        void **pVal)
{
    WidevineStream *widevine = (WidevineStream *)stream;
    if(strcmp(key, "uri") == 0
        || strcmp(key, STREAM_METADATA_REDIRECT_URI) == 0
        || strcmp(key, STREAM_METADATA_ACCESSIBLE_URI) == 0)
    {
        *pVal = widevine->dataSource.uri;
        return 0;
    }
    else
    {
        CDX_LOGD("key = %s", key);
        return CdxStreamGetMetaData(widevine->child, key, pVal);
    }
}


static struct CdxStreamOpsS widevineStreamOps =
{
    .connect = WidevineStreamConnect,
    .getProbeData = WidevineStreamGetProbeData,
    .read = WidevineStreamRead,
    .write = NULL,
    .close = WidevineStreamClose,
    .getIOState = WidevineStreamGetIOState,
    .attribute = WidevineStreamAttribute,
    .control = WidevineStreamControl,
    .getMetaData = WidevineStreamGetMetaData,
    .seek = WidevineStreamSeek,
    .seekToTime = NULL,
    .eos = WidevineStreamEos,
    .tell = WidevineStreamTell,
    .size = WidevineStreamSize,
};

CdxStreamT *WidevineStreamCreate(CdxDataSourceT *dataSource)
{
    WidevineStream *widevine = malloc(sizeof(WidevineStream));
    if(!widevine)
    {
        CDX_LOGE("malloc fail!");
        return NULL;
    }
    memset(widevine, 0x00, sizeof(WidevineStream));
    DupDataSouce(&widevine->dataSource, dataSource);
    CdxDataSourceT source;
    memset(&source, 0x00, sizeof(CdxDataSourceT));
    DupExtraDataOfDataSouce(&source, dataSource);
    source.uri = malloc(1024);
    CDX_CHECK(source.uri);
    sprintf(source.uri, "http://%s", dataSource->uri + 11);
    widevine->child = CdxStreamCreate(&source);
    if(!widevine->child)
    {
        CDX_LOGE("CdxStreamCreate fail! (%s)", source.uri);
        goto _error;
    }
    ClrDataSource(&source);
    widevine->base.ops = &widevineStreamOps;
    return &widevine->base;
_error:
    ClrDataSource(&source);
    ClrDataSource(&widevine->dataSource);
    free(widevine);
    return NULL;
}

CdxStreamCreatorT widevineStreamCtor =
{
    .create = WidevineStreamCreate
};
