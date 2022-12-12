/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxHlsParser.c
* Description :
* History :
*   Author  : Kewei Han
*   Date    : 2014/10/08
*   Comment : Http live streaming implementation.
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxHlsParser"
#include "M3UParser.h"
#include "CdxHlsParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#include <CdxTime.h>
#include <ctype.h>
#include <stdlib.h>
#include <openssl/md5.h>
//#include <openssl/aes.h>
#include <time.h>
#include <inttypes.h>

//#define REFRESH_PLAYLIST_TIMEOUT 60*1000*1000LL
#if defined(CONF_CMCC)
#define STREAM_OPEN_TIMEOUT_LIVE    INT64_MAX
#define CONFIG_HLS_TRY_NEXT_TS    1
#else
#define STREAM_OPEN_TIMEOUT_LIVE    (60*1000*1000LL)
#define CONFIG_HLS_TRY_NEXT_TS    1
#endif
#ifndef CONFIG_HLS_TRY_NEXT_TS
#define CONFIG_HLS_TRY_NEXT_TS    0
#endif

// option for hls seek.
#define OPTION_HLS_SEEK_IN_SEGMENT  1
#define OPTION_HLS_NOT_SEEK_IN_SEGMENT 0

// <CONFIG_HLS_SEEK=1>: seek to the right request point, it may be slower a little.
// <CONFIG_HLS_SEEK=0>: seek to the beginning of this segment,
///                     it is faster a little, but not much accurat.
#define CONFIG_HLS_SEEK    OPTION_HLS_NOT_SEEK_IN_SEGMENT

//for cmcc: X, 1, 1, 1, 2, 5, 10, 10, 10...
#define REFRESHPLAYLISTUG (1)

#define CDX_MIN(x,y) (x)<(y)?(x):(y)

static int CallbackProcess(void* pUserData, int eMessageId, void* param);

#define rdLockPlaylist(hlsParser) \
    do { \
        struct timespec t = {0}; \
        clock_gettime(CLOCK_REALTIME, &t); \
        t.tv_sec += 1; \
        if (pthread_rwlock_timedrdlock(&hlsParser->rwlockPlaylist, &t)) \
            logd("pthread_rwlock_timedrdlock error: %s", strerror(errno)); \
    } while (0)

#define wrLockPlaylist(hlsParser) \
    do { \
        struct timespec t = {0}; \
        clock_gettime(CLOCK_REALTIME, &t); \
        t.tv_sec += 1; \
        if (pthread_rwlock_timedwrlock(&hlsParser->rwlockPlaylist, &t)) \
            logd("pthread_rwlock_timedwrlock error %s", strerror(errno)); \
    } while (0)

#define rdUnlockPlaylist(hlsParser) \
    do { \
        pthread_rwlock_unlock(&hlsParser->rwlockPlaylist); \
    } while (0)

#define wrUnlockPlaylist(hlsParser) rdUnlockPlaylist(hlsParser)

static int CdxStreamOpen_Retry(CdxDataSourceT *source, pthread_mutex_t *mutex, cdx_bool *exit,
                               CdxStreamT **stream, ContorlTask *streamTasks, int64_t timeout)
{
    int ret;
    int64_t firstConnectTimeUs = CdxGetNowUs();
    int count = -1;
_reTry:
    count++;
    ret = CdxStreamOpen(source, mutex, exit, stream, streamTasks);
    if (ret < 0)
    {
        CDX_LOGE("open stream fail, uri(%s)", source->uri);
        if(*exit || CdxGetNowUs() - firstConnectTimeUs > timeout)
        {
            CDX_LOGE("forceStop or STREAM_OPEN_TIMEOUT, forceStop=(%d)", *exit);
            return ret;
        }
        else if(!*stream)
        {
            CDX_LOGE("Maybe it is a bug.");
            return ret;
        }
        CDX_LOGE("CdxStreamOpen fail");
        pthread_mutex_lock(mutex);
        CdxStreamClose(*stream);
        *stream = NULL;
        pthread_mutex_unlock(mutex);
        usleep(50*1000);
        goto _reTry;
    }
    return count;
}

static int getGroupItemUriFromGroup(MediaGroup *group, int itemIndex, AString **mURI)
{
    int j;
    if(itemIndex >= group->mNumMediaItem)
    {
        CDX_LOGE("itemIndex invalid");
        return 0;
    }
    MediaItem *item = group->mMediaItems;
    for (j=0; j<itemIndex; j++)
    {
        item = item->next;
    }
    if(!item)
    {
        CDX_LOGE("ERROR_MALFORMED");
        return 0;
    }
    *mURI = item->mURI;
    return 1;
}

static int getGroupItemUriFromPlaylist(Playlist *playlist, SelectTrace *selectTrace,AString **mURI)
{
    *mURI = NULL;
    if(!playlist->mIsVariantPlaylist)
    {
        CDX_LOGE("media playlist has not media group");
        return 0;
    }
    MediaGroup *group = playlist->u.masterPlaylistPrivate.mMediaGroups;
    while (group)
    {
        if(group->mType == selectTrace->mType &&
           !strcmp(group->groupID->mData, selectTrace->groupID->mData))
        {
            break;
        }
        group = group->next;
    }
    if(!group)
    {
        return 0;
    }
    return getGroupItemUriFromGroup(group, selectTrace->mNum, mURI);
}

static void ClearHlsExtraDataContainer(CdxHlsParser *hlsParser)
{
    ExtraDataContainerT *extraDataContainer = &hlsParser->extraDataContainer;
    if(extraDataContainer->extraData)
    {
        if(extraDataContainer->extraDataType == EXTRA_DATA_HTTP_HEADER)
        {
            CdxHttpHeaderFieldsT *hdr = (CdxHttpHeaderFieldsT *)(extraDataContainer->extraData);
            if(hdr->pHttpHeader)
            {
                int i;
                for(i = 0; i < hdr->num; i++)
                {
                    free((void*)(hdr->pHttpHeader + i)->key);
                    free((void*)(hdr->pHttpHeader + i)->val);
                }
                free(hdr->pHttpHeader);
                hdr->pHttpHeader = NULL;
            }
        }
        free(extraDataContainer->extraData);
        extraDataContainer->extraDataType = EXTRA_DATA_UNKNOWN;
        extraDataContainer->extraData = NULL;
    }
    return ;
}

static void GetExtraDataContainer(CdxHlsParser *hlsParser, CdxStreamT *stream, CdxParserT *parser)
{
    CDX_LOGD("*********extraDataContainer");
    ClearHlsExtraDataContainer(hlsParser);
    ExtraDataContainerT *extraDataContainer = NULL;
    if(stream)
    {
        CdxStreamGetMetaData(stream, "extra-data", (void **)&extraDataContainer);
    }
    else if(parser)
    {
        CdxParserControl(parser, CDX_PSR_CMD_GET_STREAM_EXTRADATA, (void *)&extraDataContainer);
    }
    if(extraDataContainer)
    {
        CDX_LOGD("extraDataContainer");
        hlsParser->extraDataContainer.extraDataType = extraDataContainer->extraDataType;
        if(extraDataContainer->extraDataType == EXTRA_DATA_HTTP_HEADER)
        {
            hlsParser->extraDataContainer.extraData =
                                      (CdxHttpHeaderFieldsT *)malloc(sizeof(CdxHttpHeaderFieldsT));

            CdxHttpHeaderFieldsT *hdr = (CdxHttpHeaderFieldsT *)(extraDataContainer->extraData);

            CdxHttpHeaderFieldsT *hdr1 =
                                 (CdxHttpHeaderFieldsT *)(hlsParser->extraDataContainer.extraData);
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
            CDX_LOGW("it is not supported now. extraDataType(%d)",
                      extraDataContainer->extraDataType);
        }
    }
    return ;
}

static void FreeExtraDataOfDataSource(CdxDataSourceT *cdxDataSource)
{
    if(!cdxDataSource->extraData)
    {
        cdxDataSource->extraDataType = EXTRA_DATA_UNKNOWN;
        return ;
    }
    enum DSExtraDataTypeE extraDataType = cdxDataSource->extraDataType;
    void *extraData = cdxDataSource->extraData;
    if(extraDataType == EXTRA_DATA_AES)
    {
        AesStreamExtraDataT *aesCipherInf = (AesStreamExtraDataT *)extraData;
        extraDataType = aesCipherInf->extraDataType;
        extraData = aesCipherInf->extraData;
    }
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
    cdxDataSource->extraData = NULL;
    cdxDataSource->extraDataType = EXTRA_DATA_UNKNOWN;
    return;
}

static int SetDataSourceByExtraDataContainer(CdxDataSourceT *dataSource,
                                            ExtraDataContainerT *extraDataContainer)
{
    FreeExtraDataOfDataSource(dataSource);
    dataSource->extraDataType = extraDataContainer->extraDataType;
    if(extraDataContainer->extraData)
    {
        if(extraDataContainer->extraDataType == EXTRA_DATA_HTTP_HEADER)
        {
            dataSource->extraData = (CdxHttpHeaderFieldsT *)malloc(sizeof(CdxHttpHeaderFieldsT));
            CdxHttpHeaderFieldsT *hdr = (CdxHttpHeaderFieldsT *)(extraDataContainer->extraData);
            CdxHttpHeaderFieldsT *hdr1 = (CdxHttpHeaderFieldsT *)(dataSource->extraData);
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
            CDX_LOGW("it is not supported now. extraDataType(%d)",
                      extraDataContainer->extraDataType);
        }
    }
    return 0;
}

static int SetDataSourceForSegment(CdxHlsParser *hlsParser, PlaylistItem *item)
{
    FreeExtraDataOfDataSource(&hlsParser->cdxDataSource);
    int cipher = 0;
    int haveRange = 0;
    int num = 0;
    int ret = 0;

    rdLockPlaylist(hlsParser);
    Playlist *playlist = hlsParser->mPlaylist;
    if(playlist->u.mediaPlaylistPrivate.hasEncrypte)
    {
        PlaylistItem *cipherReference = item->u.mediaItemAttribute.cipherReference;
        if(!cipherReference)
        {
            CDX_LOGW("cipherReference == NULL");
            ret = -2;
            goto out;
        }
        if(cipherReference != hlsParser->u.media.cipherReference)
        {
            AString *cipherMethod;
            AString *cipherUri;
            AString *cipherIv;
            if(findString(&cipherReference->itemMeta, "cipher-method", &cipherMethod) == 0)
            {
                CDX_LOGE("failed to fetch cipher-method");
                ret = -1;
                goto out;
            }
            if(!strcasecmp(cipherMethod->mData, "AES-128"))
            {
                if(findString(&cipherReference->itemMeta, "cipher-uri", &cipherUri) == 0)
                {
                    CDX_LOGE("failed to fetch key uri ");
                    ret = -1;
                    goto out;
                }
                CdxDataSourceT dataSource;
                memset(&dataSource, 0x00, sizeof(CdxDataSourceT));
                dataSource.uri = cipherUri->mData;
                CdxStreamT *stream = NULL;
                // Todo: There is no way to force stop this stream if it's blocking in connect().
                ret = CdxStreamOpen_Retry(&dataSource, &hlsParser->statusLock,
                                          &hlsParser->forceStop, &stream, NULL,
                                          hlsParser->streamOpenTimeout);
                if(ret < 0)
                {
                    CDX_LOGE("CdxStreamOpen fail");
                    if (stream)
                        CdxStreamClose(stream);
                    ret = -1;
                    goto out;
                }
                ret = CdxStreamRead(stream, hlsParser->aesCipherInf.key, 16);
                if(ret != 16)
                {
                    CDX_LOGE("CdxStreamRead fail");
                    if (stream)
                        CdxStreamClose(stream);
                    ret = -1;
                    goto out;
                }
                if(!CdxStreamEos(stream))
                {
                    CDX_LOGW("should not be here.");
                }
                CdxStreamClose(stream);
                if(findString(&cipherReference->itemMeta, "cipher-iv", &cipherIv))
                {
                    hlsParser->ivIsAppointed = 1;
                    if((!startsWith(cipherIv->mData, "0x") && !startsWith(cipherIv->mData, "0X"))
                        || cipherIv->mSize != 16 * 2 + 2)
                    {
                        CDX_LOGE("malformed cipher IV (%s)", cipherIv->mData);
                        ret = -1;
                        goto out;
                    }
                    memset(hlsParser->aesCipherInf.iv, 0, sizeof(hlsParser->aesCipherInf.iv));
                    int i;
                    for (i = 0; i < 16; ++i)
                    {
                        char c_1 = tolower(cipherIv->mData[2 + 2 * i]);
                        char c_2 = tolower(cipherIv->mData[3 + 2 * i]);
                        if (!isxdigit(c_1) || !isxdigit(c_2))
                        {
                            CDX_LOGE("malformed cipher IV (%s)", cipherIv->mData);
                            ret = -1;
                            goto out;
                        }
                        uint8_t nb1 = isdigit(c_1) ? c_1 - '0' : c_1 - 'a' + 10;
                        uint8_t nb2 = isdigit(c_2) ? c_2 - '0' : c_2 - 'a' + 10;

                        hlsParser->aesCipherInf.iv[i] = nb1 << 4 | nb2;
                    }
                }
                else
                {
                    memset(hlsParser->aesCipherInf.iv, 0, sizeof(hlsParser->aesCipherInf.iv));
                    hlsParser->aesCipherInf.iv[15] = hlsParser->u.media.curSeqNum & 0xff;
                    hlsParser->aesCipherInf.iv[14] = (hlsParser->u.media.curSeqNum >> 8) & 0xff;
                    hlsParser->aesCipherInf.iv[13] = (hlsParser->u.media.curSeqNum >> 16) & 0xff;
                    hlsParser->aesCipherInf.iv[12] = (hlsParser->u.media.curSeqNum >> 24) & 0xff;
                }
            }
            else
            {
                CDX_LOGE("cipher-method:(%s), is not supported.", cipherMethod->mData);
                ret = -1;
                goto out;
            }
            hlsParser->u.media.cipherReference = cipherReference;
        }
        else
        {
            if(!hlsParser->ivIsAppointed)
            {
                memset(hlsParser->aesCipherInf.iv, 0, sizeof(hlsParser->aesCipherInf.iv));
                hlsParser->aesCipherInf.iv[15] = hlsParser->u.media.curSeqNum & 0xff;
                hlsParser->aesCipherInf.iv[14] = (hlsParser->u.media.curSeqNum >> 8) & 0xff;
                hlsParser->aesCipherInf.iv[13] = (hlsParser->u.media.curSeqNum >> 16) & 0xff;
                hlsParser->aesCipherInf.iv[12] = (hlsParser->u.media.curSeqNum >> 24) & 0xff;
            }
        }
        if(hlsParser->aesUri)
        {
            free(hlsParser->aesUri);
        }

        int size = item->mURI->mSize + 7;
        hlsParser->aesUri = (char *)malloc(size);
        if(!hlsParser->aesUri)
        {
            CDX_LOGE("malloc fail");
            ret = -1;
            goto out;
        }
        sprintf(hlsParser->aesUri, "aes://%s", item->mURI->mData);

        free(hlsParser->cdxDataSource.uri);
        hlsParser->cdxDataSource.uri = strdup(hlsParser->aesUri);
        hlsParser->cdxDataSource.extraData = &hlsParser->aesCipherInf;
        hlsParser->cdxDataSource.extraDataType = EXTRA_DATA_AES;
        cipher = 1;
    }

    cdx_int64 rangeOffset, rangeLength;
    /*offset,length在代码中是一定同时存在或者不存在，除非item->itemMeta的空间已满*/
    if(findInt64(&item->itemMeta, "range-offset", &rangeOffset)
        && findInt64(&item->itemMeta, "range-length", &rangeLength))
    {
        haveRange = 1;
    }
    if(hlsParser->extraDataContainer.extraData &&
       hlsParser->extraDataContainer.extraDataType == EXTRA_DATA_HTTP_HEADER)
    {
        num = ((CdxHttpHeaderFieldsT *)(hlsParser->extraDataContainer.extraData))->num;
        int i;
        CdxHttpHeaderFieldsT *hdr =
                                 (CdxHttpHeaderFieldsT *)(hlsParser->extraDataContainer.extraData);
        for(i = 0; i < hdr->num; i++)
        {
            if(!strcasecmp((hdr->pHttpHeader + i)->key, "EagleEye-TraceId"))
            {
                logd("have XXX yet.");
                num--;
            }
        }
    }
    num += haveRange;
    if(hlsParser->mYunOSUUID[0])
    {
        logd("have XXX");
        num++;
    }
    if(num)
    {
        CdxHttpHeaderFieldsT *hdr = CdxMalloc(sizeof(CdxHttpHeaderFieldsT));
        CDX_FORCE_CHECK(hdr);
        hdr->num = num;
        hdr->pHttpHeader = CdxMalloc(hdr->num * sizeof(CdxHttpHeaderFieldT));
        CDX_FORCE_CHECK(hdr->pHttpHeader);
        cdx_int32 i = 0;
        if(haveRange)
        {
            int size = 200;
            char *val = CdxMalloc(size);
            CDX_FORCE_CHECK(val);
            snprintf(val, size, "bytes=%lld-%lld", rangeOffset, rangeOffset + rangeLength - 1);
            (hdr->pHttpHeader + i)->key = strdup("Range");
            (hdr->pHttpHeader + i)->val = val;
            i++;
        }
        if(hlsParser->extraDataContainer.extraData)
        {
            CdxHttpHeaderFieldT *pHttpHeader =
                  ((CdxHttpHeaderFieldsT *)(hlsParser->extraDataContainer.extraData))->pHttpHeader;
            for(; i < hdr->num; i++)
            {
                (hdr->pHttpHeader + i)->key = strdup(pHttpHeader[i].key);
                if(!strcasecmp((hdr->pHttpHeader + i)->key, "EagleEye-TraceId"))
                {
                    int size = 200;
                    char *val = CdxMalloc(size);
                    CDX_FORCE_CHECK(val);
                    snprintf(val, size, "%s%13lld%4d", hlsParser->mYunOSUUID, CdxGetNowUs()/1000,
                             1000 + hlsParser->curDownloadObject);
                    (hdr->pHttpHeader + i)->val = val;
                }
                else
                {
                    (hdr->pHttpHeader + i)->val = strdup(pHttpHeader[i].val);
                }
            }
        }
        else
        {
            int size = 200;
            (hdr->pHttpHeader + i)->key = strdup("EagleEye-TraceId");
            char *val = CdxMalloc(size);
            CDX_FORCE_CHECK(val);
            snprintf(val, size, "%s%lld%d", hlsParser->mYunOSUUID, CdxGetNowUs()/1000,
                     1000 + hlsParser->curDownloadObject);
            (hdr->pHttpHeader + i)->val = val;
            i++;
        }

        if(cipher)
        {
            hlsParser->aesCipherInf.extraData = (void *)hdr;
            hlsParser->aesCipherInf.extraDataType = EXTRA_DATA_HTTP_HEADER;
        }
        else
        {
            hlsParser->cdxDataSource.extraData = (void *)hdr;
            hlsParser->cdxDataSource.extraDataType = EXTRA_DATA_HTTP_HEADER;
        }
    }

    if(!cipher)
    {
        free(hlsParser->cdxDataSource.uri);
        hlsParser->cdxDataSource.uri = strdup(item->mURI->mData);
    }

    ret = 0;
out:
    rdUnlockPlaylist(hlsParser);
    return ret;
}

/*return -1:error*/
/*return 0:playlist 未改变*/
/*return 1:playlist 是新生成的*/
static int DownloadParseM3u8(CdxHlsParser *hlsParser)
{
    int ret, m3u8ReadSize = 0;
    int num = 1024;
    int incSize = 1024*1024;
    int64_t beforeFetchTimeUs = CdxGetNowUs();

    while(1)
    {
        if(hlsParser->forceStop == 1)
        {
            return -1;
        }
        ret = CdxStreamRead(hlsParser->cdxStream, hlsParser->m3u8Buf + m3u8ReadSize, num);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamRead fail, hlsParser->forceStop=%d", hlsParser->forceStop);
            return -1;
        }
        if(ret == 0)
        {
            break;
        }
        m3u8ReadSize += ret;

        if(m3u8ReadSize >= hlsParser->m3u8BufSize - num)
        {
            char *tmpBuf = NULL;
            tmpBuf = (char *)realloc(hlsParser->m3u8Buf, hlsParser->m3u8BufSize + incSize);
            if(!tmpBuf)
            {
                loge("realloc failed.");
                return -1;
            }
            hlsParser->m3u8Buf = tmpBuf;
            hlsParser->m3u8BufSize += incSize;
        }
    }

    if(m3u8ReadSize == 0)
    {
        CDX_LOGE("download m3u8 mPlaylist fail");
        return -1;
    }

    MD5_CTX m;
    MD5_Init(&m);
    MD5_Update(&m, hlsParser->m3u8Buf, m3u8ReadSize);

    Playlist *playlist = NULL;
    status_t err;
    if(hlsParser->isMasterParser == -1)
    {
        if ((err = M3u8Parse(hlsParser->m3u8Buf, m3u8ReadSize, &playlist, hlsParser->m3u8Url)) !=
             OK)
        {
            CDX_LOGE("creation playlist fail, err=%d", err);
            return -1;
        }

        wrLockPlaylist(hlsParser);
        hlsParser->mPlaylist = playlist;
        wrUnlockPlaylist(hlsParser);
        if(playlist->mIsVariantPlaylist)
        {
            hlsParser->isMasterParser = 1;
        }
        else
        {
            hlsParser->isMasterParser = 0;
            MD5_Final(hlsParser->u.media.mMD5, &m);
            hlsParser->u.media.refreshState = INITIAL_MINIMUM_RELOAD_DELAY;
        }
        return 1;
    }
    else if(hlsParser->isMasterParser == 0)
    {
        uint8_t key[16];
        MD5_Final(key, &m);

        /*update==1时，文件虽然可能一样，但hlsParser->m3u8Url却不同*/
        if(!hlsParser->update && memcmp(key, hlsParser->u.media.mMD5, 16) == 0)
        {
            logd("refreshState(%d)", hlsParser->u.media.refreshState);
#if REFRESHPLAYLISTUG
            if(hlsParser->u.media.refreshState != SIXTH_UNCHANGED_RELOAD_ATTEMPT)
            {
                hlsParser->u.media.refreshState++;
            }
#else
            hlsParser->u.media.refreshState = FIRST_UNCHANGED_RELOAD_ATTEMPT;
#endif
            return 0;
        }
        else
        {
            if ((err = M3u8Parse(hlsParser->m3u8Buf, m3u8ReadSize, &playlist, hlsParser->m3u8Url))
                != OK)
            {
                CDX_LOGE("creation playlist fail, err=%d", err);
                return -1;
            }

            wrLockPlaylist(hlsParser);
            if (hlsParser->mPlaylist)
            {
                destroyPlaylist(hlsParser->mPlaylist);
            }
            hlsParser->mPlaylist = playlist;
            wrUnlockPlaylist(hlsParser);

            hlsParser->u.media.refreshState = INITIAL_MINIMUM_RELOAD_DELAY;
            memcpy(hlsParser->u.media.mMD5, key, 16);
            return 1;
        }
    }
    else
    {
        CDX_LOGE("Should not be here.");
        return -1;
    }
}

static int64_t RefreshPlaylistWaitTime(CdxHlsParser *hlsParser)
{
    rdLockPlaylist(hlsParser);
    Playlist *playlist = hlsParser->mPlaylist;
    int32_t targetSegDurationSecs;
    if(!findInt32(&playlist->mMeta,"target-duration", &targetSegDurationSecs))
    {
        rdUnlockPlaylist(hlsParser);
        CDX_LOGE("target-duration is not found, should not be here");
        return 1;
    }

    int64_t targetSegDurationUs = targetSegDurationSecs * 1000000ll;
    int64_t minAgeUsPlaylist = targetSegDurationUs / 2;
    switch (hlsParser->u.media.refreshState)
    {
        case INITIAL_MINIMUM_RELOAD_DELAY:
        {
            PlaylistItem *item = findItemByIndex(playlist, playlist->mNumItems - 1);
            if (item == NULL)
            {
                CDX_LOGE("findItemByIndex fail");
                break;
            }
            minAgeUsPlaylist = CDX_MIN(CDX_MIN(item->u.mediaItemAttribute.durationUs, 9000000),
                                               targetSegDurationUs);
            break;
        }
        case FIRST_UNCHANGED_RELOAD_ATTEMPT:
        {
#if REFRESHPLAYLISTUG
            minAgeUsPlaylist = 1000000;
#else
            minAgeUsPlaylist = targetSegDurationUs / 2;
#endif
            break;
        }
        case SECOND_UNCHANGED_RELOAD_ATTEMPT:
        {
            minAgeUsPlaylist = 1000000;
            break;
        }
        case THIRD_UNCHANGED_RELOAD_ATTEMPT:
        {
            minAgeUsPlaylist = 1000000;
            break;
        }
        case FOURTH_UNCHANGED_RELOAD_ATTEMPT:
        {
            minAgeUsPlaylist = 2000000;
            break;
        }
        case FIFTH_UNCHANGED_RELOAD_ATTEMPT:
        {
            minAgeUsPlaylist = 5000000;
            break;
        }
        case SIXTH_UNCHANGED_RELOAD_ATTEMPT:
        {
            minAgeUsPlaylist = 10000000;
            break;
        }
        default:
        {
            CDX_LOGD("never be here.");
            break;
        }
    }
    rdUnlockPlaylist(hlsParser);
    return hlsParser->u.media.mLastPlaylistFetchTimeUs + minAgeUsPlaylist - CdxGetNowUs();
}

static int RefreshPlaylist(CdxHlsParser *hlsParser)
{
    pthread_mutex_lock(&hlsParser->statusLock);
    if(hlsParser->forceStop)
    {
        pthread_mutex_unlock(&hlsParser->statusLock);
        return -1;
    }

    if(hlsParser->cdxStream)
    {
        if(hlsParser->callback == CallbackProcess && hlsParser->pUserData)
        {
            CdxHlsParser *father = hlsParser->pUserData;
            pthread_mutex_lock(&father->statusLock);
            if(CdxStreamClose(hlsParser->cdxStream))
            {
                CDX_LOGE("CdxStreamClose fail");
            }
            hlsParser->cdxStream = NULL;
            father->childStream[0] = NULL;//TODO
            pthread_mutex_unlock(&father->statusLock);
        }
        else
        {
            if(CdxStreamClose(hlsParser->cdxStream))
            {
                CDX_LOGE("CdxStreamClose fail");
            }
            hlsParser->cdxStream = NULL;
        }
    }
    pthread_mutex_unlock(&hlsParser->statusLock);

    hlsParser->u.media.mLastPlaylistFetchTimeUs = CdxGetNowUs();

    CdxDataSourceT dataSource;
    memset(&dataSource, 0, sizeof(CdxDataSourceT));
    dataSource.uri = hlsParser->m3u8Url;
    SetDataSourceByExtraDataContainer(&dataSource, &hlsParser->extraDataContainer);
/*
    if(hlsParser->extraDataContainer.extraData)
    {
        dataSource.extraData = hlsParser->extraDataContainer.extraData;
        dataSource.extraDataType = hlsParser->extraDataContainer.extraDataType;
    }
*/
    hlsParser->curDownloadObject = -1;
    hlsParser->curDownloadUri = dataSource.uri;
    struct CallBack cb;
    cb.callback = CallbackProcess;
    cb.pUserData = (void *)hlsParser;

    ContorlTask streamContorlTask, streamContorlTask1;
    streamContorlTask.cmd = CDX_PSR_CMD_SET_CALLBACK;
    streamContorlTask.param = (void *)&cb;
    streamContorlTask.next = &streamContorlTask1;
    streamContorlTask1.cmd = STREAM_CMD_SET_ISHLS;
    streamContorlTask1.param = NULL;
    streamContorlTask1.next = NULL;
    int ret = CdxStreamOpen_Retry(&dataSource, &hlsParser->statusLock, &hlsParser->forceStop,
                                  &hlsParser->cdxStream, &streamContorlTask,
                                  hlsParser->streamOpenTimeout);
    FreeExtraDataOfDataSource(&dataSource);
    if(ret < 0)
    {
        if(!hlsParser->forceStop)
        {
            CDX_LOGE("CdxStreamOpen error");
            hlsParser->mErrno = PSR_IO_ERR;
        }
        return -1;
    }
    char *tmpUrl;
    ret = CdxStreamGetMetaData(hlsParser->cdxStream, "uri", (void **)&tmpUrl);
    if(ret < 0)
    {
        CDX_LOGE("get the uri of the stream error!");
        return -1;
    }
    if(strcmp(tmpUrl, hlsParser->m3u8Url))
    {
        free(hlsParser->m3u8Url);
        hlsParser->m3u8Url = strdup(tmpUrl);
    }
    if(DownloadParseM3u8(hlsParser) < 0)
    {
        CDX_LOGE("downloadParseM3u8 fail");
        if(!hlsParser->forceStop)
        {
            hlsParser->mErrno = PSR_IO_ERR;
        }
        return -1;
    }

    return 0;
}

/* Call this function only if curSeq is the last segment.
 * Demux Component takes timeShiftLastSeqNum as the last segment you have
 * played. If you call notifyTimeShiftInfo() in init(), and then apk call
 * seekTo one min after, it will seek to timeShiftLastSeqNum.
 * timeShiftLastSeqNum can be twenty mins after, instead of one.
 */
static void notifyTimeShiftInfo(CdxHlsParser *hlsParser)
{
    if (!hlsParser->callback || !hlsParser->pUserData)
    {
        CDX_LOGE("no callback for notify time shift info");
        return;
    }

    TimeShiftEndInfoT info = {
        .timeShiftDuration = hlsParser->mPlaylist->u.mediaPlaylistPrivate.mDurationUs / 1000,
        .timeShiftLastSeqNum = hlsParser->mPlaylist->u.mediaPlaylistPrivate.lastSeqNum,
    };

    if (hlsParser->callback(hlsParser->pUserData,
                PARSER_NOTIFY_TIMESHIFT_END_INFO, &info) < 0)
        CDX_LOGE("hlsParser->callback fail.");
}

static void *PeriodicTaskThread(void *arg)
{
    CdxHlsParser *hlsParser = (CdxHlsParser *)arg;
    while (1)
    {
        int64_t waitTime = 0;
        if (hlsParser->refreshFaild == 0)
            waitTime = RefreshPlaylistWaitTime(hlsParser);
        CDX_LOGD("wait %lld ms to refresh playlist", (long long int)(waitTime / 1000));
        if (waitTime > 10000)
        {
            struct timespec time;
            clock_gettime(CLOCK_REALTIME, &time);
            waitTime = waitTime * 1000L + time.tv_nsec;
            time.tv_sec += waitTime / (1000L * 1000L * 1000L);
            time.tv_nsec = waitTime % (1000L * 1000L * 1000L);
            if (sem_timedwait(&hlsParser->PeriodicTask.semStop, &time) == 0)
                break;
        }

        CDX_LOGD("RefreshPlaylist start");
        if (RefreshPlaylist(hlsParser) < 0)
        {
            CDX_LOGE("RefreshPlaylist fail");
            hlsParser->refreshFaild = 1;
#if 0
            /* For live media playlist, force stop means a close request, not
             * seek, right???
             */
            pthread_mutex_lock(&hlsParser->statusLock);
            int forceStop = hlsParser->forceStop;
            pthread_mutex_unlock(&hlsParser->statusLock);
            if (forceStop)
                break;
#endif
            /* wait 10ms to refresh again */
            struct timespec time;
            clock_gettime(CLOCK_REALTIME, &time);
            waitTime = 10 * 1000L * 1000L + time.tv_nsec;
            time.tv_sec += waitTime / (1000L * 1000L * 1000L);
            time.tv_nsec = waitTime % (1000L * 1000L * 1000L);
            if (sem_timedwait(&hlsParser->PeriodicTask.semStop, &time) == 0)
                break;
        }
        else
        {
            hlsParser->refreshFaild = 0;
        }
        CDX_LOGD("RefreshPlaylist finish");

        if (hlsParser->mPlaylist->u.mediaPlaylistPrivate.mIsComplete ||
            (hlsParser->mIsTimeShift == 1 &&
            hlsParser->mPlaylist->u.mediaPlaylistPrivate.discontinueTimeshift == 1))
        {
            logd("mIsComplete=%d, mIsTimeShift=%d",
                hlsParser->mPlaylist->u.mediaPlaylistPrivate.mIsComplete,
                hlsParser->mIsTimeShift);
            //we not break here, just wait.
            sem_wait(&hlsParser->PeriodicTask.semTimeshift);
        }
    }

    CDX_LOGD("exit");
    return NULL;
}

static int UpdateHlsParser(CdxHlsParser *hlsParser, const char *url)
{
    if (hlsParser->isMasterParser)
    {
        CDX_LOGE("should not be here.");
        return -1;
    }
    if(hlsParser->m3u8Url)
    {
        free(hlsParser->m3u8Url);
    }
    hlsParser->m3u8Url = strdup(url);
    if(!hlsParser->m3u8Url)
    {
        CDX_LOGE("strdup failed.");
        return -1;
    }
    hlsParser->u.media.cipherReference = NULL;
    hlsParser->ivIsAppointed = 0;
    return RefreshPlaylist(hlsParser);
}

static int GetCacheState(CdxHlsParser *hlsParser, struct ParserCacheStateS *cacheState)
{
    struct ParserCacheStateS parserCS;
    if(hlsParser->child[0])
    {
        if (CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_GET_CACHESTATE, cacheState) < 0)
        {
            CDX_LOGE("CDX_PSR_CMD_STREAM_CONTROL fail");
            return -1;
        }
    }

    if (hlsParser->isMasterParser)
    {
        if (hlsParser->child[1])
        {
            if (CdxParserControl(hlsParser->child[1], CDX_PSR_CMD_GET_CACHESTATE, &parserCS) < 0)
            {
                CDX_LOGE("CDX_PSR_CMD_STREAM_CONTROL fail");
                return -1;
            }
            cacheState->nCacheCapacity += parserCS.nCacheCapacity;
            cacheState->nCacheSize += parserCS.nCacheSize;
            cacheState->nBandwidthKbps += parserCS.nBandwidthKbps;
        }
        if (hlsParser->child[2])
        {
            if (CdxParserControl(hlsParser->child[2], CDX_PSR_CMD_GET_CACHESTATE, &parserCS) < 0)
            {
                CDX_LOGE("CDX_PSR_CMD_STREAM_CONTROL fail");
                return -1;
            }
            cacheState->nCacheCapacity += parserCS.nCacheCapacity;
            cacheState->nCacheSize += parserCS.nCacheSize;
            cacheState->nBandwidthKbps += parserCS.nBandwidthKbps;
        }
    }
    else
    {
        rdLockPlaylist(hlsParser);
        Playlist *playlist = hlsParser->mPlaylist;
        if(playlist->u.mediaPlaylistPrivate.mIsComplete)
        {
            cdx_int64 durationUs = playlist->u.mediaPlaylistPrivate.mDurationUs;
            PlaylistItem *item = hlsParser->u.media.curItemInf.item;

            cdx_int64 cacheTime = hlsParser->u.media.baseTimeUs +
                             item->u.mediaItemAttribute.durationUs * cacheState->nPercentage / 100;
            if (durationUs != 0)
                cacheState->nPercentage = (cacheTime * 100) / durationUs;
        }
        else
        {
            cacheState->nPercentage = 0;
        }
        rdUnlockPlaylist(hlsParser);
    }
    return 0;
}

static int SwitchBandwidth(void* parameter)
{
    CdxHlsParser *hlsParser = (CdxHlsParser*)parameter;
    if (!hlsParser->isMasterParser)
    {
        CDX_LOGE("should not be here.");
        return -1;
    }
    if(hlsParser->u.master.bandwidthCount <= 1)
    {
        return 0;
    }
    int i = 0, ret;
    PlaylistItem *item = NULL, *item1, *item2;
    if(hlsParser->mPlayQuality == 2)
    {
        struct ParserCacheStateS cacheState;
        ret = GetCacheState(hlsParser, &cacheState);
        if(ret < 0)
        {
            CDX_LOGE("GetCacheState fail.");
            return -1;
        }
        cdx_int64 bandwidthBps = (cdx_int64)cacheState.nBandwidthKbps * 1000;
        bandwidthBps = bandwidthBps * 8 / 10;
        while(i < hlsParser->u.master.bandwidthCount)
        {
            item= findItemByIndex(hlsParser->mPlaylist, hlsParser->u.master.bandwidthSortIndex[i]);
            if(item->u.masterItemAttribute.bandwidth <= bandwidthBps)
            {
                break;
            }
            i++;
            CDX_LOGV("bandwidth = %lld", item->u.masterItemAttribute.bandwidth);
        }
        if(i == hlsParser->u.master.bandwidthCount)
        {
            i--;
        }
        while(i > 0)
        {
            /*出现优先*//*避免纯音频*/
            item1 = findItemByIndex(hlsParser->mPlaylist,
                                    hlsParser->u.master.bandwidthSortIndex[i]);
            item2 = findItemByIndex(hlsParser->mPlaylist,
                                    hlsParser->u.master.bandwidthSortIndex[i-1]);
            if(item1->u.masterItemAttribute.bandwidth == item2->u.masterItemAttribute.bandwidth
                || (i == hlsParser->u.master.bandwidthCount - 1
                && item2->u.masterItemAttribute.bandwidth <
                   item1->u.masterItemAttribute.bandwidth*2/5))
            {
                i--;
                item = item2;
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        if(hlsParser->mPlayQuality == 0)
        {
            i = hlsParser->u.master.bandwidthCount - 1;
        }
        else if(hlsParser->mPlayQuality == 1)
        {
            i = 0;
        }
        item = findItemByIndex(hlsParser->mPlaylist, hlsParser->u.master.bandwidthSortIndex[i]);

    }

    hlsParser->u.master.curBandwidthIndex = i;
    if(hlsParser->u.master.curBandwidthIndex != hlsParser->u.master.preBandwidthIndex)
    {
        ret = CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_UPDATE_PARSER,
                               (void *)item->mURI->mData);
        if(ret < 0)
        {
            CDX_LOGE("CDX_PSR_CMD_UPDATE_PARSER fail.");
            return -1;
        }
        char *tmpUrl = ((CdxHlsParser *)hlsParser->child[0])->m3u8Url;
        //* 有多种带宽的master playlist，非第一个带宽url若重定向，记录重定向后的url。
        if(strcmp(tmpUrl, item->mURI->mData))
        {
            free(item->mURI->mData);
            int len = strlen(tmpUrl);
            item->mURI->mData = strdup(tmpUrl);
            item->mURI->mSize = len;
            item->mURI->mAllocSize = len + 1;
        }

        Playlist *playlist = hlsParser->mPlaylist;
        AString *groupIDA = NULL, *groupIDS = NULL;
        SelectTrace *selectTrace;
        AString *mURI;
        int foundA = findString(&item->itemMeta, "audio", &groupIDA);
        if(foundA)
        {
            selectTrace = &hlsParser->u.master.selectTrace[1];
            selectTrace->mType = TYPE_AUDIO;
            selectTrace->groupID = groupIDA;
            selectTrace->mNum = 0;

            if(getGroupItemUriFromPlaylist(playlist, selectTrace, &mURI))
            {
                if(strcmp(mURI->mData, item->mURI->mData))
                {
                    if(hlsParser->child[1])
                    {
                        ret = CdxParserControl(hlsParser->child[1], CDX_PSR_CMD_UPDATE_PARSER,
                                               (void *)mURI->mData);
                        if(ret < 0)
                        {
                            CDX_LOGE("CDX_PSR_CMD_UPDATE_PARSER fail.");
                            return -1;
                        }
                    }
                    else
                    {
                        free(hlsParser->cdxDataSource.uri);
                        hlsParser->cdxDataSource.uri = strdup(mURI->mData);
                        /*可以省掉，因为master没有extraData，也不用free*/
                        hlsParser->cdxDataSource.extraData = NULL;
                        ret = CdxParserPrepare(&hlsParser->cdxDataSource, 0,&hlsParser->statusLock,
                                               &hlsParser->forceStop,
                            &hlsParser->child[1], &hlsParser->childStream[1], NULL, NULL);
                        if(ret < 0)
                        {
                            CDX_LOGW("CreatAudioParser fail");
                            return -1;
                        }
                    }
                }
                else
                {
                    if(hlsParser->child[1])
                    {
                        ret = CdxParserClose(hlsParser->child[1]);
                        if(ret < 0)
                        {
                            CDX_LOGE("CdxParserClose fail");
                            return -1;
                        }
                        hlsParser->child[1] = NULL;
                    }
                }
            }
            else
            {
                if(hlsParser->child[1])
                {
                    ret = CdxParserClose(hlsParser->child[1]);
                    if(ret < 0)
                    {
                        CDX_LOGE("CdxParserClose fail");
                        return -1;
                    }
                    hlsParser->child[1] = NULL;
                }
            }
        }
        else
        {
            if(hlsParser->child[1])
            {
                ret = CdxParserClose(hlsParser->child[1]);
                if(ret < 0)
                {
                    CDX_LOGE("CdxParserClose fail");
                    return -1;
                }
                hlsParser->child[1] = NULL;
            }

        }

        int foundS = findString(&item->itemMeta, "subtitles", &groupIDS);
        if(foundS)
        {
            selectTrace = &hlsParser->u.master.selectTrace[2];
            selectTrace->mType = TYPE_SUBS;
            selectTrace->groupID = groupIDS;
            selectTrace->mNum = 0;

            if(getGroupItemUriFromPlaylist(playlist, selectTrace, &mURI))
            {
                if(strcmp(mURI->mData, item->mURI->mData))
                {
                    if(hlsParser->child[2])
                    {
                        ret = CdxParserControl(hlsParser->child[2], CDX_PSR_CMD_UPDATE_PARSER,
                                               (void *)mURI->mData);
                        if(ret < 0)
                        {
                            CDX_LOGE("CDX_PSR_CMD_UPDATE_PARSER fail.");
                            return -1;
                        }
                    }
                    else
                    {
                        free(hlsParser->cdxDataSource.uri);
                        hlsParser->cdxDataSource.uri = strdup(mURI->mData);
                        hlsParser->cdxDataSource.extraData = NULL;
                        ret = CdxParserPrepare(&hlsParser->cdxDataSource, 0,&hlsParser->statusLock,
                                               &hlsParser->forceStop,
                            &hlsParser->child[2], &hlsParser->childStream[2], NULL, NULL);
                        if(ret < 0)
                        {
                            CDX_LOGW("CreatAudioParser fail");
                            return -1;
                        }
                    }
                }
                else
                {
                    if(hlsParser->child[2])
                    {
                        ret = CdxParserClose(hlsParser->child[2]);
                        if(ret < 0)
                        {
                            CDX_LOGE("CdxParserClose fail");
                            return -1;
                        }
                        hlsParser->child[2] = NULL;
                    }
                }
            }
            else
            {
                if(hlsParser->child[2])
                {
                    ret = CdxParserClose(hlsParser->child[2]);
                    if(ret < 0)
                    {
                        CDX_LOGE("CdxParserClose fail");
                        return -1;
                    }
                    hlsParser->child[2] = NULL;
                }
            }
        }
        else
        {
            if(hlsParser->child[2])
            {
                ret = CdxParserClose(hlsParser->child[2]);
                if(ret < 0)
                {
                    CDX_LOGE("CdxParserClose fail");
                    return -1;
                }
                hlsParser->child[2] = NULL;
            }
        }
        hlsParser->u.master.preBandwidthIndex = hlsParser->u.master.curBandwidthIndex;
    }
    return 0;
}

static int refreshStartTime(char *url, time_t duraSecond)
{
    const char needle[] = "starttime=";
    char *p = strstr(url, needle);
    if (p == NULL)
        return -1;

    p += sizeof(needle) - 1;

    const char *const format = "%Y%m%dT%H%M%S.00Z";
    char strTime[sizeof("20160804T145839.00Z")] = {0};
    if (strlen(p) < sizeof(strTime) - 1)
        return -1;

    struct tm tm = {0};
    if (strptime(p, format, &tm) == NULL)
        return -1;

    time_t t = mktime(&tm);
    t += duraSecond;

    if (localtime_r(&t, &tm) == NULL)
        return -1;

    if (strftime(strTime, sizeof(strTime), format, &tm) != sizeof(strTime) - 1)
        return -1;

    memcpy(p, strTime, sizeof(strTime) - 1);
    return 0;
}

/*return -1:error*/
/*return 0:hlsParser PSR_EOS*/
/*return 1:选择到了新的Segment*/
static int SelectNextSegment(CdxHlsParser *hlsParser)
{
    int ret = 1;
    Playlist *playlist = NULL;
_RefreshPlaylist:
#if 0
    if (!playlist->u.mediaPlaylistPrivate.mIsComplete &&
            RefreshPlaylistWaitTime(hlsParser) <= 0)
    {
        if(RefreshPlaylist(hlsParser) < 0)
        {
            CDX_LOGE("RefreshPlaylist fail");
            return -1;
        }
        return -1;
    }
#else
    rdLockPlaylist(hlsParser);
    playlist = hlsParser->mPlaylist;
    if (!playlist->u.mediaPlaylistPrivate.mIsComplete &&
            hlsParser->refreshFaild == 1)
    {
        //* should we continue ??? */
        CDX_LOGE("RefreshPlaylist fail");
        ret = -1;
        goto out;
    }
#endif
    int32_t firstSeqNumberInPlaylist = playlist->mItems->u.mediaItemAttribute.seqNum;
    int32_t lastSeqNumberInPlaylist = playlist->u.mediaPlaylistPrivate.lastSeqNum;
_next:
    if(hlsParser->u.media.curSeqNum > lastSeqNumberInPlaylist)
    {
        logv("curSeqNum>lastSeqNum, mIsComplete=%d, mIsTimeShift=%d, discontinue=%d",
            playlist->u.mediaPlaylistPrivate.mIsComplete,
            hlsParser->mIsTimeShift,
            playlist->u.mediaPlaylistPrivate.discontinueTimeshift);

        if(playlist->u.mediaPlaylistPrivate.mIsComplete ||
            (hlsParser->mIsTimeShift == 1 &&
            playlist->u.mediaPlaylistPrivate.discontinueTimeshift == 1))
        {
            if (hlsParser->mIsTimeShift)
            {
                time_t duraSecond = hlsParser->mPlaylist->u.mediaPlaylistPrivate.mDurationUs /
                                    1000000;
                duraSecond -= 30;

                rdUnlockPlaylist(hlsParser);
                if (refreshStartTime(hlsParser->m3u8Url, duraSecond) ||
                        RefreshPlaylist(hlsParser))
                {
                    loge("refresh time shift m3u8 failed");
                    return 0; // use seekTo to do time shift
                }
                else
                {
                    sem_post(&hlsParser->PeriodicTask.semTimeshift);
                    goto _RefreshPlaylist;
                }
            }
            ret = 0;
            goto out;
        }
        else
        {
            if(hlsParser->forceStop == 1)
            {
                CDX_LOGD("forceStop");
                ret = -1;
                goto out;
            }
            if (hlsParser->u.media.curSeqNum > lastSeqNumberInPlaylist + 1)
            {
                /* Should we reset curSeqNum? */
                CDX_LOGW("sequence num wrapped? "
                    "curSeqNum %d, lastSeqNumberInPlaylist %d",
                    hlsParser->u.media.curSeqNum, lastSeqNumberInPlaylist);
                hlsParser->u.media.curSeqNum = firstSeqNumberInPlaylist;
            }
            else
            {
                rdUnlockPlaylist(hlsParser);
                usleep(20000);
                goto _RefreshPlaylist;
            }
        }
    }
    if(hlsParser->u.media.curSeqNum < firstSeqNumberInPlaylist)
    {
        CDX_LOGW("curSeqNum < firstSeqNumberInPlaylist, "
                "curSeqNum %d, firstSeqNumberInPlaylist %d",
                hlsParser->u.media.curSeqNum, firstSeqNumberInPlaylist);
        hlsParser->u.media.curSeqNum = firstSeqNumberInPlaylist;
    }

    PlaylistItem *item = findItemBySeqNum(playlist, hlsParser->u.media.curSeqNum);
    if(!item)
    {
        CDX_LOGE("findItemBySeqNum fail");
        ret = -1;
        goto out;
    }
    ret = SetDataSourceForSegment(hlsParser, item);
    if(ret == -1)
    {
        CDX_LOGE("SetDataSourceForSegment fail");
        ret = -1;
        goto out;
    }
    else if(ret == -2)
    {
        CDX_LOGW("please check this code, when error happen");
        hlsParser->u.media.curSeqNum++;
        goto _next;
    }
    hlsParser->u.media.baseTimeUs = item->u.mediaItemAttribute.baseTimeUs;
    hlsParser->u.media.curItemInf.item = item;
    hlsParser->u.media.curItemInf.givenPcr = 0;
    ret = 1;
out:
    rdUnlockPlaylist(hlsParser);
    return ret;
}

static cdx_int32 SetMediaInfo(CdxHlsParser *hlsParser)
{
    if(!hlsParser->child[0])
    {
        CDX_LOGE("hlsParser->child[0] == NULL");
        return -1;
    }
    CdxMediaInfoT* pMediaInfo = &hlsParser->mediaInfo;
    int ret = CdxParserGetMediaInfo(hlsParser->child[0], pMediaInfo);
    if(ret < 0)
    {
        CDX_LOGE("hlsParser->child[0] getMediaInfo error!");
        return -1;
    }
    if(hlsParser->child[1])
    {
        CdxMediaInfoT audioMediaInfo;
        memset(&audioMediaInfo, 0, sizeof(CdxMediaInfoT));

        ret = CdxParserGetMediaInfo(hlsParser->child[1], &audioMediaInfo);
        if(ret < 0)
        {
            CDX_LOGE("audio stream getMediaInfo error!");
            return -1;
        }
        pMediaInfo->program[0].audioNum = audioMediaInfo.program[0].audioNum;
        pMediaInfo->program[0].audioIndex = audioMediaInfo.program[0].audioIndex;
        memcpy(pMediaInfo->program[0].audio, audioMediaInfo.program[0].audio,
               sizeof(pMediaInfo->program[0].audio));
    }
    if(hlsParser->child[2])
    {
        CdxMediaInfoT subtitleMediaInfo;
        memset(&subtitleMediaInfo, 0, sizeof(CdxMediaInfoT));

        ret = CdxParserGetMediaInfo(hlsParser->child[2], &subtitleMediaInfo);
        if(ret < 0)
        {
            CDX_LOGE("Subtitle stream getMediaInfo error!");
            return -1;
        }
        pMediaInfo->program[0].subtitleNum = subtitleMediaInfo.program[0].subtitleNum;
        pMediaInfo->program[0].subtitleIndex = subtitleMediaInfo.program[0].subtitleIndex;
        memcpy(pMediaInfo->program[0].subtitle, subtitleMediaInfo.program[0].subtitle,
               sizeof(pMediaInfo->program[0].subtitle));
    }

    if(!hlsParser->isMasterParser)
    {
        if(hlsParser->mPlaylist->u.mediaPlaylistPrivate.mIsComplete)
        {
            pMediaInfo->bSeekable = CDX_TRUE;
            pMediaInfo->program[0].duration =
                     hlsParser->mPlaylist->u.mediaPlaylistPrivate.mDurationUs / 1000; //duration ms
        }
        else
        {
            pMediaInfo->bSeekable = CDX_FALSE;
            pMediaInfo->program[0].duration = 0;
        }
        hlsParser->u.media.curItemInf.size = pMediaInfo->fileSize;
    }
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pMediaInfo->fileSize = -1;
    return 0;
}

enum {
    PARSER_ATTEMPT_SWITCH_BANDWIDTH,
};

static int CallbackProcess(void* pUserData, int eMessageId, void* param)
{
    CdxHlsParser *hlsParser = (CdxHlsParser*)pUserData;
    int ret;
    DownloadObject cur;
    switch(eMessageId)
    {
        case PARSER_NOTIFY_VIDEO_STREAM_CHANGE:
        case PARSER_NOTIFY_AUDIO_STREAM_CHANGE:
        {
            CDX_LOGD("PARSER_NOTIFY_VIDEO_STREAM_CHANGE");
            ret = SetMediaInfo(hlsParser);
            if(ret < 0)
            {
                CDX_LOGE("SetMediaInfo fail, ret(%d)", ret);
            }
            break;
        }
        case PARSER_ATTEMPT_SWITCH_BANDWIDTH:
        {
             return SwitchBandwidth(pUserData);
        }
        case STREAM_EVT_DOWNLOAD_START:
        {
            /* only ts segment download start need callback, m3u8 not callback. keep param NULL.*/
            if(hlsParser->childStream[0] && !hlsParser->isMasterParser)
            {
                if(hlsParser->curDownloadObject != -1)
                {
                    cur.seqNum = hlsParser->curDownloadObject;
                    PlaylistItem *item = hlsParser->u.media.curItemInf.item;
                    cur.seqDuration = item->u.mediaItemAttribute.durationUs;
                    //cur.uri = hlsParser->curDownloadUri;
                    hlsParser->u.media.curItemInf.size = CdxStreamSize(hlsParser->childStream[0]);
                    cur.seqSize = hlsParser->u.media.curItemInf.size;
                    param = (void *)&cur;
                }
            }
            break;
#if 0
            if(!param)
            {
                int ret;
                //cur.seqNum = hlsParser->curDownloadObject;
                cur.uri = hlsParser->curDownloadUri;
                ret = CdxParserControl((CdxParserT*)hlsParser, CDX_PSR_CMD_GET_TS_SEQ_NUM,
                                       (void*)&cur.seqNum);
                if(ret < 0)
                {
                    logd("CDX_PSR_CMD_GET_TS_SEQ_NUM");
                    return -1;
                }
                ret = CdxParserControl((CdxParserT*)hlsParser, CDX_PSR_CMD_GET_TS_DURATION,
                                       (void*)&cur.seqDuration);
                if(ret < 0)
                {
                    logd("CDX_PSR_CMD_GET_TS_DURATION");
                    return -1;
                }
                ret = CdxParserControl((CdxParserT*)hlsParser, CDX_PSR_CMD_GET_TS_LENGTH,
                                       (void*)&cur.seqSize);
                if(ret < 0)
                {
                    logd("CDX_PSR_CMD_GET_TS_LENGTH");
                    return -1;
                }
                param = (void *)&cur;
            }
            break;
#endif
        }
        case STREAM_EVT_DOWNLOAD_END:
        {
            ExtraDataContainerT *extradata = (ExtraDataContainerT *)param;
            if(EXTRA_DATA_HTTP == extradata->extraDataType)
            {
                //* only ts segment download end need callback, m3u8 not callback.
                if(hlsParser->curDownloadObject != -1)
                {
                    cur.spendTime = *(cdx_int64*)extradata->extraData;
                    cur.seqSize = hlsParser->u.media.curItemInf.size;

                    struct ParserCacheStateS cacheState;
                    ret = GetCacheState(hlsParser, &cacheState);
                    if(ret < 0)
                    {
                        CDX_LOGE("GetCacheState fail.");
                        return -1;
                    }
                    cur.rate = (cdx_int64)cacheState.nBandwidthKbps * 1000;
                    extradata->extraDataType = EXTRA_DATA_HLS;
                    extradata->extraData = (void *)&cur;
                    param = extradata;
                }

            }
            break;
        }
        case STREAM_EVT_DOWNLOAD_ERROR:
            break;
        case STREAM_EVT_DOWNLOAD_GET_TCP_IP:
            break;
        case STREAM_EVT_DOWNLOAD_START_TIME:
            break;
        case STREAM_EVT_DOWNLOAD_FIRST_TIME:
            break;
        case STREAM_EVT_DOWNLOAD_END_TIME:
            break;
        case STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR:
            break;
        case STREAM_EVT_DOWNLOAD_RESPONSE_HEADER:
            break;
        case STREAM_EVT_NET_DISCONNECT:
        case STREAM_EVT_CMCC_LOG_RECORD:
        {
            break;
        }
        default:
            logw("ignore child callback message, eMessageId = 0x%x.", eMessageId);
            return -1;
    }
    if(hlsParser->callback)
    {
        hlsParser->callback(hlsParser->pUserData, eMessageId, param);
    }
    return 0;
}

static cdx_int32 HlsParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    CdxHlsParser *hlsParser = (CdxHlsParser*)parser;
    if(hlsParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGW("hlsParser->status < CDX_PSR_IDLE, can not GetMediaInfo");
        hlsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    memcpy(pMediaInfo, &hlsParser->mediaInfo, sizeof(CdxMediaInfoT));
    return 0;
}

static cdx_int32 HlsParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxHlsParser *hlsParser = (CdxHlsParser*)parser;
    if(hlsParser->status != CDX_PSR_IDLE && hlsParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGW("status != CDX_PSR_IDLE && status != CDX_PSR_PREFETCHED,\
                  HlsParserPrefetch invaild");
        hlsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(hlsParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if(hlsParser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(pkt, &hlsParser->cdxPkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&hlsParser->statusLock);
    if(hlsParser->forceStop)
    {
        CDX_LOGD("forceStop");
        pthread_mutex_unlock(&hlsParser->statusLock);
        return -1;
    }
    hlsParser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&hlsParser->statusLock);

    hlsParser->prefetchType = 0;
    if(hlsParser->streamPts[1] < hlsParser->streamPts[0])
    {
        hlsParser->prefetchType = 1;
    }
    if((hlsParser->streamPts[2] < hlsParser->streamPts[1])&&
       (hlsParser->streamPts[2] < hlsParser->streamPts[0]))
    {
        hlsParser->prefetchType = 2;
    }

    int ret, i = 0, mErrno;
    int resetPtsShift = 0;
    next_child:
    ret = CdxParserPrefetch(hlsParser->child[hlsParser->prefetchType], pkt);
    if(ret < 0)
    {
        mErrno = CdxParserGetStatus(hlsParser->child[hlsParser->prefetchType]);
#if CONFIG_HLS_TRY_NEXT_TS
        if(mErrno == PSR_EOS || mErrno == PSR_IO_ERR)
#else
        if(mErrno == PSR_EOS)
#endif
        {
            if(hlsParser->child[hlsParser->prefetchType]->type == CDX_PARSER_HLS)
            {
                next_child1:
                i++;
                if(i == 3)
                {
                    CDX_LOGD("hlsParser->status = PSR_EOS");
                    hlsParser->mErrno = PSR_EOS;
                    goto _exit;
                }
                hlsParser->prefetchType = (hlsParser->prefetchType + 1)%3;
                if(!hlsParser->child[hlsParser->prefetchType])
                {
                    goto next_child1;
                }
                goto next_child;
            }
            else
            {
_NextSegment:
                //CdxParserClose(hlsParser->child[hlsParser->prefetchType]);
                //hlsParser->child[hlsParser->prefetchType] = NULL;
                if(hlsParser->callback == CallbackProcess && hlsParser->pUserData)
                {
                    ret= hlsParser->callback(hlsParser->pUserData, PARSER_ATTEMPT_SWITCH_BANDWIDTH,
                                             NULL);
                    if(ret < 0)
                    {
                        CDX_LOGE("hlsParser->callback fail.");
                    }
                }
                hlsParser->u.media.curSeqNum++;

                CDX_LOGD("curSeqNum = %d, lastSeqNum=%d", hlsParser->u.media.curSeqNum,
                    hlsParser->mPlaylist->u.mediaPlaylistPrivate.lastSeqNum);
                ret = SelectNextSegment(hlsParser);
                if(ret < 0)
                {
                    CDX_LOGD("SelectNextSegment fail");
                    goto _exit;
                }
                else if(ret == 0)
                {
                    CDX_LOGD("hlsParser->status = PSR_EOS");
                    hlsParser->mErrno = PSR_EOS;
                    ret = -1;
                    goto _exit;
                }
                hlsParser->curDownloadObject = hlsParser->u.media.curSeqNum;
                hlsParser->curDownloadUri = hlsParser->cdxDataSource.uri;
                //CdxStreamT *cdxStream = NULL;
                struct CallBack cb;
                cb.callback = CallbackProcess;
                cb.pUserData = (void *)hlsParser;

                ContorlTask streamContorlTask, streamContorlTask1;
                streamContorlTask.cmd = CDX_PSR_CMD_SET_CALLBACK;
                streamContorlTask.param = (void *)&cb;
                streamContorlTask.next = &streamContorlTask1;
                streamContorlTask1.cmd = STREAM_CMD_SET_ISHLS;
                streamContorlTask1.param = NULL;
                streamContorlTask1.next = NULL;
                pthread_mutex_lock(&hlsParser->statusLock);
                hlsParser->childStream[0] = NULL;
                pthread_mutex_unlock(&hlsParser->statusLock);
                ret = CdxStreamOpen_Retry(&hlsParser->cdxDataSource, &hlsParser->statusLock,
                                          &hlsParser->forceStop, &hlsParser->childStream[0],
                                          &streamContorlTask, hlsParser->streamOpenTimeout);
                if(ret < 0)
                {
                    CDX_LOGD("CdxStreamOpen fail");

                    /* Whether hlsParser->forceStop is true or false, if childStream[0]
                     * is not NULL, it leads to memleak definitely. We can pass the stream
                     * to TS parser and let the parser do close. However, call TS parser
                     * control function is heavy. And by close it immediately, we can avoid
                     * call force stop on it and waste time.
                     */
                    pthread_mutex_lock(&hlsParser->statusLock);
                    if (hlsParser->childStream[0] != NULL)
                    {
                        CdxStreamClose(hlsParser->childStream[0]);
                        hlsParser->childStream[0] = NULL;
                    }
                    pthread_mutex_unlock(&hlsParser->statusLock);

                    if(!hlsParser->forceStop)
                    {
                        CDX_LOGE(" CdxStreamOpen error!");
                        hlsParser->mErrno = PSR_IO_ERR;
                    }
                    else
                    {
                        ret = -1;
                        goto _exit;
                    }

                    rdLockPlaylist(hlsParser);
                    ret = hlsParser->mPlaylist->u.mediaPlaylistPrivate.mIsComplete;
                    rdUnlockPlaylist(hlsParser);
                    if (ret)
                    {
                        ret = -1;
                        goto _exit;
                    }
                    else
                    {
                        CDX_LOGD("*************_NextSegment");
                        goto _NextSegment;
                    }
                }
                else if (ret > 20) // network is offline?
                {
                    rdLockPlaylist(hlsParser);
                    int complete = hlsParser->mPlaylist->u.mediaPlaylistPrivate.mIsComplete;
                    rdUnlockPlaylist(hlsParser);
                    if (!complete && !hlsParser->mIsTimeShift)
                    {
                        pthread_mutex_lock(&hlsParser->statusLock);
                        CdxStreamClose(hlsParser->childStream[0]);
                        hlsParser->childStream[0] = NULL;
                        pthread_mutex_unlock(&hlsParser->statusLock);
                        usleep(10000); // wait m3u8 to update
                        goto _NextSegment;
                    }
                }

                GetExtraDataContainer(hlsParser, hlsParser->childStream[0], NULL);
                pthread_mutex_lock(&hlsParser->statusLock);
                if (hlsParser->u.media.curItemInf.item->discontinue)
                {
                    CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_CLR_INFO, NULL);
                    resetPtsShift = 1;
                    logd("resetPtsShift");
                }
                ret = CdxParserControl(hlsParser->child[hlsParser->prefetchType],
                                    CDX_PSR_CMD_REPLACE_STREAM, (void *)hlsParser->childStream[0]);
                pthread_mutex_unlock(&hlsParser->statusLock);

                if(ret < 0)
                {
                    CDX_LOGE("CDX_PSR_CMD_REPLACE_STREAM fail, ret(%d)", ret);
                    hlsParser->mErrno = PSR_UNKNOWN_ERR;
                    goto _exit;
                }

                rdLockPlaylist(hlsParser);
                if(hlsParser->mPlaylist->u.mediaPlaylistPrivate.mIsComplete &&
                   (hlsParser->u.media.curSeqNum ==
                    hlsParser->mPlaylist->u.mediaPlaylistPrivate.lastSeqNum))
                {
                    if (hlsParser->mIsTimeShift)
                        notifyTimeShiftInfo(hlsParser);
                    ret = CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_SET_LASTSEGMENT_FLAG,
                                           NULL);

                }
                rdUnlockPlaylist(hlsParser);
                pthread_mutex_lock(&hlsParser->statusLock);
                if (hlsParser->u.media.curItemInf.item->discontinue)
                {
                    CDX_LOGD("set hls discontinuity");
                    CdxParserControl(hlsParser->child[0],
                        CDX_PSR_CMD_SET_HLS_DISCONTINUITY, NULL);
                    hlsParser->callback(hlsParser->pUserData,
                        PARSER_NOTIFY_HLS_DISCONTINUITY, NULL);
                }
                pthread_mutex_unlock(&hlsParser->statusLock);
                hlsParser->u.media.curItemInf.size = CdxStreamSize(hlsParser->childStream[0]);
                ret = CdxParserPrefetch(hlsParser->child[hlsParser->prefetchType], pkt);
                if(ret < 0)
                {
                    CDX_LOGD("CdxParserPrefetch fail");
                    if(hlsParser->forceStop)
                    {
                        CDX_LOGD("forceStop");
                        goto _exit;
                    }
                    mErrno = CdxParserGetStatus(hlsParser->child[hlsParser->prefetchType]);
#if CONFIG_HLS_TRY_NEXT_TS
                    if(mErrno == PSR_EOS || mErrno == PSR_IO_ERR)
#else
                    if(mErrno == PSR_EOS)
#endif
                    {
                        goto _NextSegment;
                    }
                    CDX_LOGE(" prefetch error! ret(%d)", ret);
                    hlsParser->mErrno = PSR_UNKNOWN_ERR;
                    goto _exit;
                }
            }
        }
        else
        {
            hlsParser->mErrno = mErrno;
            CDX_LOGE("CdxParserPrefetch mErrno = %d", mErrno);
            goto _exit;
        }
    }
    if(!hlsParser->isMasterParser)
    {
        if(!hlsParser->u.media.curItemInf.givenPcr)
        {
            pkt->pcr = hlsParser->u.media.baseTimeUs;
            hlsParser->u.media.curItemInf.givenPcr = 1;
        }
        else
        {
            pkt->pcr = -1;
        }

    }

    if (resetPtsShift)
        hlsParser->ptsShift = pkt->pts - hlsParser->streamPts[hlsParser->prefetchType] - 100000;

    pkt->pts -= hlsParser->ptsShift;
    hlsParser->streamPts[hlsParser->prefetchType] = pkt->pts;
    memcpy(&hlsParser->cdxPkt, pkt, sizeof(CdxPacketT));
    if(hlsParser->firstPts < 0)
    {
        hlsParser->firstPts = pkt->pts;
        logd("hlsParser->firstPts=%lld", hlsParser->firstPts);
    }

    // CDX_LOGV("CdxParserPrefetch pkt->pts=%lld, pkt->type=%d, pkt->length=%d",
    //          pkt->pts, pkt->type, pkt->length);
    pthread_mutex_lock(&hlsParser->statusLock);
    hlsParser->status = CDX_PSR_PREFETCHED;
    pthread_mutex_unlock(&hlsParser->statusLock);
    pthread_cond_signal(&hlsParser->cond);
    return 0;
_exit:
    pthread_mutex_lock(&hlsParser->statusLock);
    hlsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&hlsParser->statusLock);
    pthread_cond_signal(&hlsParser->cond);
    return ret;
}

static cdx_int32 HlsParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxHlsParser *hlsParser = (CdxHlsParser*)parser;
    if(hlsParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_PREFETCHED, we can not read!");
        hlsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    pthread_mutex_lock(&hlsParser->statusLock);
    if(hlsParser->forceStop)
    {
        pthread_mutex_unlock(&hlsParser->statusLock);
        return -1;
    }
    hlsParser->status = CDX_PSR_READING;
    pthread_mutex_unlock(&hlsParser->statusLock);

    int ret = CdxParserRead(hlsParser->child[hlsParser->prefetchType], pkt);
    if(pkt->type == CDX_MEDIA_METADATA)
    {
        pkt->pts -= hlsParser->firstPts;
    }
    pthread_mutex_lock(&hlsParser->statusLock);
    hlsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&hlsParser->statusLock);
    pthread_cond_signal(&hlsParser->cond);
    return ret;
}

static cdx_int32 HlsParserForceStop(CdxParserT *parser)
{
    CDX_LOGD("HlsParserForceStop start");
    CdxHlsParser *hlsParser = (CdxHlsParser*)parser;

    int ret;
    pthread_mutex_lock(&hlsParser->statusLock);
    hlsParser->forceStop = 1;
    hlsParser->mErrno = PSR_USER_CANCEL;
    if(hlsParser->cdxStream)
    {
        ret = CdxStreamForceStop(hlsParser->cdxStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamForceStop fail");
            //hlsParser->mErrno = PSR_UNKNOWN_ERR;
        }
    }
    int i;
    for(i = 0; i < 3; i++)
    {
        if(hlsParser->child[i])
        {
            ret = CdxParserForceStop(hlsParser->child[i]);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamForceStop fail");
                //hlsParser->mErrno = CdxParserGetStatus(hlsParser->child[i]);
                //return -1;
            }
        }
        if(hlsParser->childStream[i])
        {
            /*
            CDX_LOGV("hlsParser->childStream[i](%p)", hlsParser->childStream[i]);
            if(hlsParser->child[i]->type == CDX_PARSER_HLS)
            {
                CDX_LOGD("hlsParser->cdxStream[i](%p)",
                          ((CdxHlsParser *)hlsParser->child[i])->cdxStream);
            }*/
            ret = CdxStreamForceStop(hlsParser->childStream[i]);
            if(ret < 0)
            {
                CDX_LOGW("CdxStreamForceStop fail");
                //hlsParser->mErrno = PSR_UNKNOWN_ERR;
            }
        }
    }

    while(hlsParser->status != CDX_PSR_IDLE && hlsParser->status != CDX_PSR_PREFETCHED)
    {
        pthread_cond_wait(&hlsParser->cond, &hlsParser->statusLock);
    }
    pthread_mutex_unlock(&hlsParser->statusLock);
    hlsParser->mErrno = PSR_USER_CANCEL;
    hlsParser->status = CDX_PSR_IDLE;
    CDX_LOGV("HlsParserForceStop end");
    return 0;
}

static cdx_int32 HlsParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGI("HlsParserClrForceStop start");
    int i, ret;
    CdxHlsParser *hlsParser = (CdxHlsParser *)parser;
    if(hlsParser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("hlsParser->status != CDX_PSR_IDLE");
        hlsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    hlsParser->forceStop = 0;
    if(hlsParser->cdxStream)
    {
        ret = CdxStreamClrForceStop(hlsParser->cdxStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamClrForceStop fail");
            //hlsParser->mErrno = PSR_UNKNOWN_ERR;
            //return -1;
        }
    }
    for(i = 0; i < 3; i++)
    {
        if(hlsParser->child[i])
        {
            ret = CdxParserClrForceStop(hlsParser->child[i]);
            if(ret < 0)
            {
                CDX_LOGE("CdxParserClrForceStop fail");
                //hlsParser->mErrno = CdxParserGetStatus(hlsParser->child[i]);
                //return -1;
            }
        }
        if(hlsParser->childStream[i])
        {
            ret = CdxStreamClrForceStop(hlsParser->childStream[i]);
            if(ret < 0)
            {
                CDX_LOGW("CdxStreamClrForceStop fail");
                //hlsParser->mErrno = PSR_UNKNOWN_ERR;
            }
        }
    }
    CDX_LOGI("HlsParserClrForceStop end");
    return 0;
}

static int HlsParserControl(CdxParserT *parser, int cmd, void *param)
{
    CdxHlsParser *hlsParser = (CdxHlsParser*)parser;
    int ret;
    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI(" hls parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            return HlsParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return HlsParserClrForceStop(parser);
        case CDX_PSR_CMD_SET_CALLBACK:
        {
            struct CallBack *cb = (struct CallBack *)param;
            hlsParser->callback = cb->callback;
            hlsParser->pUserData = cb->pUserData;
            return 0;
        }
        case CDX_PSR_CMD_UPDATE_PARSER:
            hlsParser->update = 1;
            ret = UpdateHlsParser(hlsParser, (const char *)param);
            hlsParser->update = 0;
            return ret;

        case CDX_PSR_CMD_GET_CACHESTATE:
        {
            return GetCacheState(hlsParser, param);
        }
        case CDX_PSR_CMD_GET_REDIRECT_URL:
        {
            if(hlsParser->isMasterParser)
            {
                return CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_GET_REDIRECT_URL, param);
            }
            else
            {
                if(hlsParser->cdxStream)
                {
                    char* tmpUrl = NULL;
                    CdxStreamGetMetaData(hlsParser->cdxStream, "uri", (void**)&tmpUrl);
                    if(tmpUrl)
                    {
                        int urlLen = strlen(tmpUrl) + 1;
                        if(urlLen > 4096)
                        {
                            CDX_LOGE("url length is too long");
                            return -1;
                        }
                        memcpy((char*)param, tmpUrl, urlLen);
                        CDX_LOGW("CDX_PSR_CMD_GET_REDIRECT_URL");
                    }
                }
                else
                {
                    param = NULL;
                }
                return 0;
            }
        }
        case CDX_PSR_CMD_GET_STREAM_STATUSCODE:
        {
            if(hlsParser->isMasterParser)
            {
                return CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_GET_STREAM_STATUSCODE,
                                        param);
            }
            else
            {
                if(hlsParser->childStream[0])
                {
                    return CdxStreamGetMetaData(hlsParser->childStream[0], "statusCode",
                                                (void**)param);
                }
                else
                {
                    return 0;
                }
            }
        }
        case CDX_PSR_CMD_GET_STREAM_EXTRADATA:
        {
            if(hlsParser->child[0])
            {
                return CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_GET_STREAM_EXTRADATA,
                                        param);
            }
            return 0;
        }
        case CDX_PSR_CMD_GET_URL:
        {
            char* tmpUrl = NULL;
            CDX_LOGD("*********** CDX_PSR_CMD_GET_URL");
            if(hlsParser->cdxStream)
            {
                CdxStreamGetMetaData(hlsParser->cdxStream, "uri", (void**)&tmpUrl);
                if(tmpUrl)
                {
                    int urlLen = strlen(tmpUrl) + 1;
                    if(urlLen > 4096)
                    {
                        CDX_LOGE("url length is too long");
                        return -1;
                    }
                    CDX_LOGD("--- hls parser get url= %s", tmpUrl);
                    memcpy((char*)param, tmpUrl, urlLen);
                }
            }
            else
            {
                param = NULL;
            }
            return 0;
        }
        case CDX_PSR_CMD_GET_TS_M3U_BANDWIDTH:
        {
            if(hlsParser->isMasterParser)
            {
                rdLockPlaylist(hlsParser);
                int index = hlsParser->u.master.curBandwidthIndex;
                PlaylistItem *item = findItemByIndex(hlsParser->mPlaylist,
                                                    hlsParser->u.master.bandwidthSortIndex[index]);
                cdx_int64 mBandwidth = item->u.masterItemAttribute.bandwidth;
                *(cdx_int64*)param = mBandwidth;
                CDX_LOGV(" CdxHlsParser:mBandwidth[%lld]",mBandwidth);
                rdUnlockPlaylist(hlsParser);
                return 0;
            }
            else
            {
                *(cdx_int64*)param = 0;
                return 0;
            }
        }
        case CDX_PSR_CMD_GET_TS_SEQ_NUM:
        {
            if(hlsParser->isMasterParser)
            {
                if(hlsParser->child[0])
                    return CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_GET_TS_SEQ_NUM, param);
                else
                    return -1;
            }
            else
            {
                int seqNum = hlsParser->curDownloadObject;
                if(seqNum != -1)
                {
                    *(int*)param = seqNum;
                    return 0;
                }
                else
                    return -1;
            }
        }
        case CDX_PSR_CMD_GET_TS_DURATION:
        {
            if(hlsParser->isMasterParser)
            {
                if(hlsParser->child[0])
                    return CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_GET_TS_DURATION,
                                            param);
                else
                    return -1;
            }
            else
            {
                PlaylistItem *item = hlsParser->u.media.curItemInf.item;
                if(item)
                {
                    cdx_int64 durationUs = item->u.mediaItemAttribute.durationUs;
                    *(cdx_int64*)param = durationUs;
                    return 0;
                }
                else
                    return -1;
            }
        }
        case CDX_PSR_CMD_GET_TS_LENGTH:
        {
            if(hlsParser->isMasterParser)
            {
                if(hlsParser->child[0])
                    return CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_GET_TS_LENGTH, param);
                else
                    return -1;
            }
            else
            {
                cdx_int64 size = hlsParser->u.media.curItemInf.size;
                *(cdx_int64*)param = size;
                return 0;
            }
        }
        case CDX_PSR_CMD_SET_YUNOS_UUID:
        {
            if(param)
                memcpy(hlsParser->mYunOSUUID,(char*)param,32);
            CDX_LOGD(" HlsParserControl[%s]",hlsParser->mYunOSUUID);
            return 0;
        }
        case CDX_PSR_CMD_SET_TIMESHIFT_LAST_SEQNUM:
        {
            if(param)
            {
                int seqNum = *(int*)param;
                if(seqNum >= 0)
                {
                    hlsParser->timeShiftLastSeqNum = seqNum;
                }
            }
            return 0;
        }
        case CDX_PSR_CMD_SET_HLS_STREAM_FORMAT_CHANGE:
            if(hlsParser->child[0])
            {
                CdxParserControl(hlsParser->child[0],
                    CDX_PSR_CMD_SET_HLS_STREAM_FORMAT_CHANGE, NULL);
            }
            return 0;
        default:
            break;
    }
    return -1;
}

static cdx_int32 HlsParserGetStatus(CdxParserT *parser)
{
    CdxHlsParser *hlsParser = (CdxHlsParser *)parser;
    return hlsParser->mErrno;
}

static cdx_int32 HlsParserSeekTo(CdxParserT *parser, cdx_int64  timeUs, SeekModeType seekModeType)
{
    CDX_LOGI("HlsParserSeekTo start, timeUs = %lld", timeUs);
    CdxHlsParser *hlsParser = (CdxHlsParser *)parser;
    hlsParser->mErrno = PSR_OK;
    int ret = 0;
    if(hlsParser->isMasterParser)
    {
        pthread_mutex_lock(&hlsParser->statusLock);
        if(hlsParser->forceStop)
        {
            CDX_LOGE("PSR_USER_CANCEL");
            pthread_mutex_unlock(&hlsParser->statusLock);
            return -1;
        }
        hlsParser->status = CDX_PSR_SEEKING;
        pthread_mutex_unlock(&hlsParser->statusLock);

        int i;
        for(i = 0; i < 3; i++)
        {
            if(hlsParser->child[i])
            {
                ret = CdxParserSeekTo(hlsParser->child[i], timeUs, seekModeType);
                if(ret < 0)
                {
                    CDX_LOGE("CdxParserSeekTo fail, ret(%d)", ret);
                    hlsParser->mErrno = CdxParserGetStatus(hlsParser->child[i]);
                    goto _exit;
                }
                hlsParser->streamPts[i] = timeUs;
            }
        }
    }
    else
    {
        rdLockPlaylist(hlsParser);
        Playlist *playlist = hlsParser->mPlaylist;
        if(!playlist->u.mediaPlaylistPrivate.mIsComplete)
        {
            rdUnlockPlaylist(hlsParser);
            CDX_LOGE("live mode, seekTo is not support");
            hlsParser->mErrno = PSR_INVALID_OPERATION;
            return -1;
        }
        else if(timeUs < 0)
        {
            rdUnlockPlaylist(hlsParser);
            CDX_LOGE("timeUs invalid");
            hlsParser->mErrno = PSR_INVALID_OPERATION;
            return -1;
        }
        else if(timeUs >= playlist->u.mediaPlaylistPrivate.mDurationUs)
        {
            rdUnlockPlaylist(hlsParser);
            CDX_LOGI("PSR_EOS");
            hlsParser->mErrno = PSR_EOS;
            return 0;
        }

        pthread_mutex_lock(&hlsParser->statusLock);
        if(hlsParser->forceStop)
        {
            CDX_LOGE("PSR_USER_CANCEL");
            pthread_mutex_unlock(&hlsParser->statusLock);
            rdUnlockPlaylist(hlsParser);
            return -1;
        }
        hlsParser->status = CDX_PSR_SEEKING;
        pthread_mutex_unlock(&hlsParser->statusLock);

        int64_t mDurationUs = 0;
        PlaylistItem *item = playlist->mItems;
        while(item)
        {
            mDurationUs += item->u.mediaItemAttribute.durationUs;
            if (mDurationUs > timeUs)
                break;
            item = item->next;
        }
        if(!item)
        {
            CDX_LOGE("unknown error");
            hlsParser->mErrno = PSR_UNKNOWN_ERR;
            ret = -1;
            goto _exit;
        }
        mDurationUs -= item->u.mediaItemAttribute.durationUs;
        if(item->u.mediaItemAttribute.seqNum != hlsParser->u.media.curSeqNum)
        {
            //CdxParserClose(hlsParser->child[0]);
            ret = SetDataSourceForSegment(hlsParser, item);

            if(ret == -1)
            {
                CDX_LOGE("SetDataSourceForSegment fail");
                goto _exit;
            }
            else if(ret == -2)
            {
                CDX_LOGW("please check this code, when error happen");
                goto _exit;
            }
            hlsParser->curDownloadObject = item->u.mediaItemAttribute.seqNum;
            hlsParser->curDownloadUri = hlsParser->cdxDataSource.uri;
            //CdxStreamT *cdxStream = NULL;
            struct CallBack cb;
            cb.callback = CallbackProcess;
            cb.pUserData = (void *)hlsParser;
            ContorlTask streamContorlTask, streamContorlTask1;
            streamContorlTask.cmd = CDX_PSR_CMD_SET_CALLBACK;
            streamContorlTask.param = (void *)&cb;
            streamContorlTask.next = &streamContorlTask1;
            streamContorlTask1.cmd = STREAM_CMD_SET_ISHLS;
            streamContorlTask1.param = NULL;
            streamContorlTask1.next = NULL;


            hlsParser->u.media.curSeqNum = item->u.mediaItemAttribute.seqNum;
            hlsParser->u.media.baseTimeUs = item->u.mediaItemAttribute.baseTimeUs;
            hlsParser->u.media.curItemInf.item = item;

            pthread_mutex_lock(&hlsParser->statusLock);
            hlsParser->childStream[0] = NULL;
            pthread_mutex_unlock(&hlsParser->statusLock);
            ret = CdxStreamOpen_Retry(&hlsParser->cdxDataSource, &hlsParser->statusLock,
                                      &hlsParser->forceStop, &hlsParser->childStream[0],
                                      &streamContorlTask, hlsParser->streamOpenTimeout);
            if(ret < 0)
            {
                pthread_mutex_lock(&hlsParser->statusLock);
                if (hlsParser->childStream[0] != NULL)
                {
                    CdxStreamClose(hlsParser->childStream[0]);
                    hlsParser->childStream[0] = NULL;
                }
                pthread_mutex_unlock(&hlsParser->statusLock);

                if(!hlsParser->forceStop)
                {
                    CDX_LOGE(" CdxStreamOpen error!");
                    hlsParser->mErrno = PSR_IO_ERR;
                }
                ret = -1;
                goto _exit;
            }
            hlsParser->u.media.curItemInf.size = CdxStreamSize(hlsParser->childStream[0]);
            GetExtraDataContainer(hlsParser, hlsParser->childStream[0], NULL);
            pthread_mutex_lock(&hlsParser->statusLock);
            CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_CLR_INFO, NULL);
            ret = CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_REPLACE_STREAM,
                                   (void *)hlsParser->childStream[0]);
            pthread_mutex_unlock(&hlsParser->statusLock);
            if(ret < 0)
            {
                CDX_LOGE("CDX_PSR_CMD_REPLACE_STREAM fail, ret(%d)", ret);
                hlsParser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }
            if(hlsParser->mPlaylist->u.mediaPlaylistPrivate.mIsComplete &&
                (hlsParser->u.media.curSeqNum ==
                 hlsParser->mPlaylist->u.mediaPlaylistPrivate.lastSeqNum))
            {
                ret = CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_SET_LASTSEGMENT_FLAG, NULL);

            }
            //hlsParser->u.media.curItemInf.givenPcr = 0;
        }
        CDX_LOGI("mDurationUs = %lld", mDurationUs);
        #if CONFIG_HLS_SEEK
        if(hlsParser->u.media.curItemInf.size > 0)
        {
            if(timeUs - mDurationUs >= item->u.mediaItemAttribute.durationUs)
            {
                hlsParser->streamSeekPos.pos = hlsParser->u.media.curItemInf.size;
            }
            else
            {
                hlsParser->streamSeekPos.pos = (timeUs - mDurationUs)*
                                                hlsParser->u.media.curItemInf.size/
                                                item->u.mediaItemAttribute.durationUs;
            }
            hlsParser->streamSeekPos.time = timeUs;
            hlsParser->streamSeekPos.startTime = mDurationUs;
            int ret1 = CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_STREAM_SEEK,
                                        &hlsParser->streamSeekPos);
            if(ret1 < 0)
            {
                CDX_LOGW("CDX_PSR_CMD_STREAM_SEEK fail");
            }
        }
        //CdxParserSeekTo(hlsParser->child[0], timeUs - mDurationUs);
        hlsParser->u.media.baseTimeUs = timeUs;
        #endif
        hlsParser->u.media.curItemInf.givenPcr = 0;
    }
_exit:
    if (!hlsParser->isMasterParser)
        rdUnlockPlaylist(hlsParser);

    pthread_mutex_lock(&hlsParser->statusLock);
    hlsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&hlsParser->statusLock);
    pthread_cond_signal(&hlsParser->cond);
    CDX_LOGI("HlsParserSeekTo end, ret = %d", ret);
    return ret;
}

static cdx_int32 HlsParserClose(CdxParserT *parser)
{
    CdxHlsParser *hlsParser = (CdxHlsParser *)parser;
    int i, ret;
    ret = HlsParserForceStop(parser);
    if(ret < 0)
    {
        CDX_LOGW("HlsParserForceStop fail");
    }

    if (hlsParser->PeriodicTask.enable)
    {
        sem_post(&hlsParser->PeriodicTask.semStop);
        sem_post(&hlsParser->PeriodicTask.semTimeshift);
        pthread_join(hlsParser->PeriodicTask.tid, NULL);
        CDX_LOGV("pthread_join for PeriodicTask finish");
        sem_destroy(&hlsParser->PeriodicTask.semStop);
        sem_destroy(&hlsParser->PeriodicTask.semTimeshift);
    }

    for(i = 0; i < 3; i++)
    {
        if(hlsParser->child[i])
        {
            ret = CdxParserClose(hlsParser->child[i]);
            if(ret < 0)
            {
                CDX_LOGE("CdxParserClose fail");
            }
        }
        else if(hlsParser->childStream[i])
        {
            CDX_LOGV("hlsParser->cdxStream close, hlsParser->childStream[i](%p)",
                      hlsParser->childStream[i]);
            ret = CdxStreamClose(hlsParser->childStream[i]);
            if(ret < 0)
            {
                CDX_LOGW("CdxStreamForceStop fail");
                //hlsParser->mErrno = PSR_UNKNOWN_ERR;
            }
        }
    }
    if(hlsParser->cdxStream)
    {
        ret = CdxStreamClose(hlsParser->cdxStream);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamClose fail, ret(%d)", ret);
        }
    }

    ClearHlsExtraDataContainer(hlsParser);

    if(hlsParser->m3u8Url)
    {
        free(hlsParser->m3u8Url);
    }
    if(hlsParser->m3u8Buf)
    {
        free(hlsParser->m3u8Buf);
    }
    if(hlsParser->mPlaylist)
    {
        destroyPlaylist(hlsParser->mPlaylist);
    }
    if(hlsParser->aesUri)
    {
        free(hlsParser->aesUri);
    }

    free(hlsParser->cdxDataSource.uri);

    FreeExtraDataOfDataSource(&hlsParser->cdxDataSource);
    pthread_mutex_destroy(&hlsParser->statusLock);
    pthread_cond_destroy(&hlsParser->cond);
    pthread_rwlock_destroy(&hlsParser->rwlockPlaylist);
    free(hlsParser);
    return 0;
}

static cdx_uint32 HlsParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    CdxHlsParser *hlsParser = (CdxHlsParser *)parser;
    return hlsParser->flags;
}

static int HlsParserInit(CdxParserT *parser)
{
    CDX_LOGI("HlsParserInit start");
    CdxHlsParser *hlsParser = (CdxHlsParser*)parser;

    hlsParser->u.media.mLastPlaylistFetchTimeUs = CdxGetNowUs();

    if(DownloadParseM3u8(hlsParser) < 0)
    {
        CDX_LOGE("downloadParseM3u8 fail");
        goto _exit;
    }
    Playlist *playlist = hlsParser->mPlaylist;
    PlaylistItem *item;
    int ret;
    if(!hlsParser->isMasterParser)
    {
        if(playlist->u.mediaPlaylistPrivate.mIsComplete || hlsParser->mIsTimeShift)
        {
            hlsParser->u.media.curSeqNum = playlist->mItems->u.mediaItemAttribute.seqNum;
            CDX_LOGD("timeshift last seq num=%d,[%d, %d]", hlsParser->timeShiftLastSeqNum,
                            playlist->mItems->u.mediaItemAttribute.seqNum,
                            playlist->u.mediaPlaylistPrivate.lastSeqNum);

            if(hlsParser->mIsTimeShift)
            {
                if(hlsParser->timeShiftLastSeqNum> playlist->mItems->u.mediaItemAttribute.seqNum &&
                    hlsParser->timeShiftLastSeqNum < playlist->u.mediaPlaylistPrivate.lastSeqNum)
                {
                    CDX_LOGD("timeshift last seq num=%d,[%d, %d]", hlsParser->timeShiftLastSeqNum,
                              playlist->mItems->u.mediaItemAttribute.seqNum,
                              playlist->u.mediaPlaylistPrivate.lastSeqNum);
                    hlsParser->u.media.curSeqNum = hlsParser->timeShiftLastSeqNum + 1;
                }
            }
        }
        else
        {
#if 0
            // If this is a live session, start 3 segments from the end.
            hlsParser->u.media.curSeqNum = playlist->u.mediaPlaylistPrivate.lastSeqNum - 3;
            if (hlsParser->u.media.curSeqNum < playlist->mItems->u.mediaItemAttribute.seqNum)
            {
                hlsParser->u.media.curSeqNum = playlist->mItems->u.mediaItemAttribute.seqNum;
            }
#endif
            hlsParser->u.media.curSeqNum = playlist->mItems->u.mediaItemAttribute.seqNum;
        }

        if (!playlist->u.mediaPlaylistPrivate.mIsComplete)
        {
            hlsParser->streamOpenTimeout = STREAM_OPEN_TIMEOUT_LIVE;
            /* Create PeriodicTask to refresh m3u8 */
            if (sem_init(&hlsParser->PeriodicTask.semStop, 0, 0) != 0)
            {
                CDX_LOGE("sem_init failed");
                goto _exit;
            }
            if (sem_init(&hlsParser->PeriodicTask.semTimeshift, 0, 0) != 0)
            {
                CDX_LOGE("sem_init failed");
                goto _exit;
            }
            if (pthread_create(&hlsParser->PeriodicTask.tid, NULL,
                        PeriodicTaskThread, (void *)hlsParser) == 0)
                hlsParser->PeriodicTask.enable = 1;
            else
            {
                CDX_LOGE("pthread_create failed");
                goto _exit;
            }
        }

        //hlsParser->u.media.durationUs = playlist->u.mediaPlaylistPrivate.mDurationUs;
_next:
        item = findItemBySeqNum(playlist, hlsParser->u.media.curSeqNum);
        if(!item)
        {
            CDX_LOGE("findItemBySeqNum fail");
            goto _exit;
        }
        ret = SetDataSourceForSegment(hlsParser, item);
        if(ret == -1)
        {
            CDX_LOGE("SetDataSourceForSegment fail");
            goto _exit;
        }
        else if(ret == -2)
        {
            CDX_LOGW("please check this code, when error happen");
            hlsParser->u.media.curSeqNum++;
            goto _next;
        }
        hlsParser->curDownloadObject = hlsParser->u.media.curSeqNum;
        hlsParser->curDownloadUri = hlsParser->cdxDataSource.uri;
        struct CallBack cb;
        cb.callback = CallbackProcess;
        cb.pUserData = (void *)hlsParser;

        ContorlTask parserContorlTask;
        parserContorlTask.cmd = CDX_PSR_CMD_SET_CALLBACK;
        parserContorlTask.param = (void *)&cb;
        parserContorlTask.next = NULL;
        int flags = NO_NEED_DURATION;
        if(!hlsParser->mPlaylist->u.mediaPlaylistPrivate.mIsComplete ||
            hlsParser->u.media.curSeqNum < hlsParser->mPlaylist->u.mediaPlaylistPrivate.lastSeqNum)
        {
            flags |= NOT_LASTSEGMENT;
        }

        hlsParser->u.media.baseTimeUs = item->u.mediaItemAttribute.baseTimeUs;
        hlsParser->u.media.curItemInf.item = item;
        hlsParser->u.media.curItemInf.givenPcr = 0;

        ret = CdxParserPrepare(&hlsParser->cdxDataSource, flags, &hlsParser->statusLock,
                               &hlsParser->forceStop, &hlsParser->child[0],
                               &hlsParser->childStream[0], &parserContorlTask, &parserContorlTask);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserPrepare fail, hlsParser->childStream[0](%p)",
                      hlsParser->childStream[0]);

            goto _exit;
        }

        GetExtraDataContainer(hlsParser, NULL, hlsParser->child[0]);
    }
    else
    {
        int p = (playlist->mNumItems < MaxNumBandwidthItems)?
                 playlist->mNumItems:MaxNumBandwidthItems;
        hlsParser->u.master.bandwidthCount = p;

        int64_t temp[MaxNumBandwidthItems]={0};
        int i,j,k;

        item = playlist->mItems;
        for (i=0; i<p; i++)
        {
            temp[i] = item->u.masterItemAttribute.bandwidth;
            item = item->next;
        }
        for(i=0; i<p; i++)
        {
            k=0;
            for (j=1; j<p; j++)
            {
                if (temp[j]>temp[k])
                    k=j;
            }
            hlsParser->u.master.bandwidthSortIndex[i]=k;
            temp[k]=0;
        }
        //hlsParser->u.master.curBandwidthIndex = p/2 > 0 ? (p/2 -1) : p/2;
        if(hlsParser->mPlayQuality == 0)
        {
            hlsParser->u.master.curBandwidthIndex = hlsParser->u.master.bandwidthCount - 1;
        }
        else if(hlsParser->mPlayQuality == 1)
        {
            hlsParser->u.master.curBandwidthIndex = 0;
        }
        else if(hlsParser->mPlayQuality == 2)
        {
            hlsParser->u.master.curBandwidthIndex = (p/2==p-1)&&(p>1)?(p/2 -1) : p/2;
        }

        int m =0;
CreatChildParser:
        item = findItemByIndex(playlist,
                    hlsParser->u.master.bandwidthSortIndex[hlsParser->u.master.curBandwidthIndex]);
        CDX_LOGV("bandwidth = %lld", item->u.masterItemAttribute.bandwidth);
        free(hlsParser->cdxDataSource.uri);
        hlsParser->cdxDataSource.uri = strdup(item->mURI->mData);
        SetDataSourceByExtraDataContainer(&hlsParser->cdxDataSource,&hlsParser->extraDataContainer);
        hlsParser->curDownloadObject = -1;
        hlsParser->curDownloadUri = hlsParser->cdxDataSource.uri;

        struct CallBack cb;
        cb.callback = CallbackProcess;
        cb.pUserData = (void *)hlsParser;

        ContorlTask parserContorlTask;
        parserContorlTask.cmd = CDX_PSR_CMD_SET_CALLBACK;
        parserContorlTask.param = (void *)&cb;
        parserContorlTask.next = NULL;


        ret = CdxParserPrepare(&hlsParser->cdxDataSource, hlsParser->flags, &hlsParser->statusLock,
                               &hlsParser->forceStop, &hlsParser->child[0],
                               &hlsParser->childStream[0], &parserContorlTask, &parserContorlTask);
        if(ret < 0)
        {
            if(hlsParser->forceStop)
            {
                goto _exit;
            }

            CDX_LOGW("CreatChildParser fail, retry next");
            m++;
            if(m == hlsParser->u.master.bandwidthCount)
            {
                CDX_LOGE("CreatChildParser fail");
                goto _exit;
            }

            hlsParser->u.master.curBandwidthIndex ++;
            if (hlsParser->u.master.curBandwidthIndex >= hlsParser->u.master.bandwidthCount)
            {
                hlsParser->u.master.curBandwidthIndex -= hlsParser->u.master.bandwidthCount;
            }
            goto CreatChildParser;
        }

        char *tmpUrl = NULL;
        //* 有多种带宽的master playlist，第一个带宽url若重定向，记录重定向后的url.
        ret = CdxStreamGetMetaData(hlsParser->childStream[0], "uri", (void**)&tmpUrl);
        if(ret < 0)
        {
            CDX_LOGE("get the uri of the stream error!");
            return -1;
        }
        if(strcmp(tmpUrl, item->mURI->mData))
        {
            free(item->mURI->mData);
            int len = strlen(tmpUrl);
            item->mURI->mData = strdup(tmpUrl);
            item->mURI->mSize = len;
            item->mURI->mAllocSize = len + 1;
        }

        /*
        struct CallBack cb;
        cb.callback = CallbackProcess;
        cb.pUserData = (void *)hlsParser;
        ret = CdxParserControl(hlsParser->child[0], CDX_PSR_CMD_SET_CALLBACK, &cb);
        if(ret < 0)
        {
            CDX_LOGE("CDX_PSR_CMD_SET_CB fail");
            goto _exit;
        }
        */
        hlsParser->u.master.preBandwidthIndex = hlsParser->u.master.curBandwidthIndex;
        AString *groupIDA = NULL, *groupIDS = NULL;
        int foundA = findString(&item->itemMeta, "audio", &groupIDA);
        int foundS = findString(&item->itemMeta, "subtitles", &groupIDS);
        SelectTrace *selectTrace;
        AString *mURI;
        if(foundA)
        {
            selectTrace = &hlsParser->u.master.selectTrace[1];
            selectTrace->mType = TYPE_AUDIO;
            selectTrace->groupID = groupIDA;
            selectTrace->mNum = 0;

            if(getGroupItemUriFromPlaylist(playlist, selectTrace, &mURI))
            {
                if(strcmp(mURI->mData, item->mURI->mData))
                {
                    free(hlsParser->cdxDataSource.uri);
                    hlsParser->cdxDataSource.uri = strdup(mURI->mData);
                    hlsParser->cdxDataSource.extraData = NULL;
                    hlsParser->curDownloadUri = hlsParser->cdxDataSource.uri;
                    ret = CdxParserPrepare(&hlsParser->cdxDataSource, 0, &hlsParser->statusLock,
                                           &hlsParser->forceStop, &hlsParser->child[1],
                                           &hlsParser->childStream[1], &parserContorlTask,
                                           &parserContorlTask);
                    if(ret < 0)
                    {
                        CDX_LOGW("CreatAudioParser fail");
                    }
                }
            }
        }
        if(foundS)
        {
            selectTrace = &hlsParser->u.master.selectTrace[2];
            selectTrace->mType = TYPE_SUBS;
            selectTrace->groupID = groupIDS;
            selectTrace->mNum = 0;

            if(getGroupItemUriFromPlaylist(playlist, selectTrace, &mURI))
            {
                if(strcmp(mURI->mData, item->mURI->mData))
                {
                    free(hlsParser->cdxDataSource.uri);
                    hlsParser->cdxDataSource.uri = strdup(mURI->mData);
                    hlsParser->cdxDataSource.extraData = NULL;
                    hlsParser->curDownloadUri = hlsParser->cdxDataSource.uri;
                    ret = CdxParserPrepare(&hlsParser->cdxDataSource, 0, &hlsParser->statusLock,
                                           &hlsParser->forceStop, &hlsParser->child[2],
                                           &hlsParser->childStream[2], &parserContorlTask,
                                           &parserContorlTask);
                    if(ret < 0)
                    {
                        CDX_LOGW("CreatSubtitleParser fail");
                    }
                }
            }
        }
    }
    int i;
    for(i = 0; i < 3; i++)
    {
        if(hlsParser->child[i])
        {
            hlsParser->streamPts[i] = 0;
        }
    }
    ret = SetMediaInfo(hlsParser);
    if(ret < 0)
    {
        CDX_LOGE("SetMediaInfo fail");
        goto _exit;
    }

    hlsParser->mErrno = PSR_OK;
    pthread_mutex_lock(&hlsParser->statusLock);
    hlsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&hlsParser->statusLock);
    pthread_cond_signal(&hlsParser->cond);
    CDX_LOGI("HlsParserInit success");
    return 0;
_exit:
    hlsParser->mErrno = PSR_OPEN_FAIL;
    pthread_mutex_lock(&hlsParser->statusLock);
    hlsParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&hlsParser->statusLock);
    pthread_cond_signal(&hlsParser->cond);
    CDX_LOGE("HlsParserInit fail");
    return -1;
}

static struct CdxParserOpsS hlsParserOps =
{
    .control         = HlsParserControl,
    .prefetch         = HlsParserPrefetch,
    .read             = HlsParserRead,
    .getMediaInfo     = HlsParserGetMediaInfo,
    .close             = HlsParserClose,
    .seekTo            = HlsParserSeekTo,
    .attribute        = HlsParserAttribute,
    .getStatus        = HlsParserGetStatus,
    .init             = HlsParserInit
};

static CdxParserT *HlsParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    CdxHlsParser *hlsParser = CdxMalloc(sizeof(CdxHlsParser));
    if(!hlsParser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(hlsParser, 0x00, sizeof(CdxHlsParser));
    hlsParser->flags = flags;
    if(flags & CMCC_TIME_SHIFT)
    {
        CDX_LOGD("---- cmcc timeShift");
        hlsParser->mIsTimeShift = 1;
    }

    int ret = pthread_mutex_init(&hlsParser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_cond_init(&hlsParser->cond, NULL);
    CDX_FORCE_CHECK(ret == 0);
    char *tmpUrl;
    ret = CdxStreamGetMetaData(stream, "uri", (void **)&tmpUrl);
    if(ret < 0)
    {
        CDX_LOGE("get the uri of the stream error!");
        goto open_error;
    }
    hlsParser->cdxStream = stream;
    int urlLen = strlen(tmpUrl) + 1;
    hlsParser->m3u8Url = malloc(urlLen);
    if(!hlsParser->m3u8Url)
    {
        CDX_LOGE("malloc fail!");
        goto open_error;
    }
    memcpy(hlsParser->m3u8Url, tmpUrl, urlLen);

    GetExtraDataContainer(hlsParser, stream, NULL);

    hlsParser->m3u8BufSize = 512 * 1024;
    hlsParser->m3u8Buf = (char *)malloc(hlsParser->m3u8BufSize);
    if (!hlsParser->m3u8Buf)
    {
        CDX_LOGE("allocate memory fail for m3u8 file");
        goto open_error;
    }

    memset(hlsParser->streamPts, 0xff, sizeof(hlsParser->streamPts));
    hlsParser->isMasterParser = -1;/*尚未判明类型*/

    hlsParser->mPlayQuality = 2;

    hlsParser->base.ops = &hlsParserOps;
    hlsParser->base.type = CDX_PARSER_HLS;
    hlsParser->mErrno = PSR_INVALID;
    hlsParser->status = CDX_PSR_INITIALIZED;
    ret = pthread_rwlock_init(&hlsParser->rwlockPlaylist, NULL);
    CDX_FORCE_CHECK(ret == 0);

    hlsParser->streamOpenTimeout = INT64_MAX;
    hlsParser->firstPts = -1;

    return &hlsParser->base;
open_error:
    CdxStreamClose(stream);
    if(hlsParser->m3u8Url)
    {
        free(hlsParser->m3u8Url);
    }
    if(hlsParser->m3u8Buf)
    {
        free(hlsParser->m3u8Buf);
    }
    if(hlsParser->extraDataContainer.extraData)
    {
        if(hlsParser->extraDataContainer.extraDataType == EXTRA_DATA_HTTP_HEADER)
        {
            CdxHttpHeaderFieldsT *hdr =
                                 (CdxHttpHeaderFieldsT *)(hlsParser->extraDataContainer.extraData);
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
        free(hlsParser->extraDataContainer.extraData);
    }
    pthread_mutex_destroy(&hlsParser->statusLock);
    pthread_cond_destroy(&hlsParser->cond);
    free(hlsParser);
    return NULL;
}

static cdx_uint32 HlsParserProbe(CdxStreamProbeDataT *probeData)
{
    if(probeData->len < 7)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }
    int ret = M3uProbe((const char *)probeData->buf, probeData->len);
    CDX_LOGD("HlsParserProbe = %d", ret);
    return ret*100;

}

CdxParserCreatorT hlsParserCtor =
{
    .create    = HlsParserOpen,
    .probe     = HlsParserProbe
};
