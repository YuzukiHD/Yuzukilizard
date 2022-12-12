/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxPlsParser.c
 * Description : Play list parser implementation.
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxPlsParser"
#include <cdx_log.h>
#include <string.h>
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <ctype.h>

enum CdxParserStatus
{
    CDX_PLS_INITIALIZED,
    CDX_PLS_IDLE,
    CDX_PLS_PREFETCHING,
    CDX_PLS_PREFETCHED,
    CDX_PLS_SEEKING,
    CDX_PLS_READING,
};

typedef struct PlsItemS
{
    char *mURI;//file
    char *pTitle;//title
    cdx_int64 durationUs;//length
    cdx_int32 seqNum;
    cdx_int64 baseTimeUs;
    struct PlsItemS *next;
}PlsItemT;

typedef struct PlaylistS
{
    char *mBaseURI;
    PlsItemT *mItems;
    cdx_uint32 mNumItems;
    cdx_int32 lastSeqNum;
    cdx_int64 durationUs;
}PlaylistT;

typedef struct CdxPlayPlsParser
{
    CdxParserT base;
    enum CdxParserStatus status;
    int mErrno;
    cdx_uint32 flags;

    int forceStop;          /* for forceStop()*/
    pthread_mutex_t statusLock;
    pthread_cond_t cond;

    CdxStreamT *cdxStream;
    CdxDataSourceT cdxDataSource;
    char *plsUrl;
    char *plsBuf;//for pls file download
    cdx_int64 plsBufSize;

    PlaylistT* mPls;
    int curSeqNum;
    cdx_int64 baseTimeUs;

    CdxParserT *child;//child parser
    CdxStreamT *childStream;
    CdxParserT *tmpChild;
    CdxStreamT *tmpChildStream;

    CdxMediaInfoT mediaInfo;
    CdxPacketT cdxPkt;
}CdxPlsParser;

static inline void SetDataSouceForPlsSegment(PlsItemT *item, CdxDataSourceT *cdxDataSource)
{
    cdxDataSource->uri = item->mURI;
}

static char *plsMakeURL(char *baseURL, const char *url)
{
    char *out = NULL;
    if(strncasecmp("http://", baseURL, 7)
             && strncasecmp("https://", baseURL, 8)
             && strncasecmp("file://", baseURL, 7))
    {
        /*Base URL must be absolute*/
        logw("url:%s", baseURL);
        //return -1;
    }

    if (!strncasecmp("http://", url, 7) || !strncasecmp("https://", url, 8))
    {
        /*"url" is already an absolute URL, ignore base URL.*/
        out = calloc(1, strlen(url) + 1);
        if(!out)
        {
            CDX_LOGE("err_no_memory");
            return NULL;
        }
        memcpy(out, url, strlen(url) + 1);
        return out;
    }

    cdx_uint32 memSize = 0;
    char *temp;
    char *protocolEnd = strstr(baseURL, "//") + 2;/*为了屏蔽http://，https://之间的差异*/

    if (url[0] == '/')
    {
         /*URL is an absolute path.*/
        char *pathStart = strchr(protocolEnd, '/');

        if (pathStart != NULL)
        {
            memSize = pathStart - baseURL + strlen(url) + 1;
            temp = (char *)calloc(1, memSize);
            if (temp == NULL)
            {
                CDX_LOGE("err_no_memory");
                return NULL;
            }
            memcpy(temp, baseURL, pathStart - baseURL);
            memcpy(temp + (pathStart - baseURL), url, strlen(url) + 1);/*url是以'\0'结尾的*/
        }
        else
        {
            memSize = strlen(baseURL) + strlen(url) + 1;
            temp = (char *)calloc(1, memSize);
            if (temp == NULL)
            {
                CDX_LOGE("err_no_memory");
                return NULL;
            }
            memcpy(temp, baseURL, strlen(baseURL));
            memcpy(temp + strlen(baseURL), url, strlen(url)+1);
        }
    }
    else
    {
         /* URL is a relative path*/
        cdx_uint32 n = strlen(baseURL);
        char *slashPos = strstr(protocolEnd, ".m3u");
        if(slashPos != NULL)
        {
            while(slashPos >= protocolEnd)
            {
                slashPos--;
                if(*slashPos == '/')
                    break;
            }
            if (slashPos >= protocolEnd)
            {
                memSize = slashPos - baseURL + strlen(url) + 2;
                temp = (char *)calloc(1, memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return NULL;
                }
                memcpy(temp, baseURL, slashPos - baseURL);
                *(temp+(slashPos - baseURL))='/';
                memcpy(temp+(slashPos - baseURL)+1, url, strlen(url) + 1);
            }
            else
            {
                memSize= n + strlen(url) + 2;
                temp = (char *)calloc(1, memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return NULL;
                }
                memcpy(temp, baseURL, n);
                *(temp + n)='/';
                memcpy(temp + n + 1, url, strlen(url) + 1);
            }

        }
        else if (baseURL[n - 1] == '/')
        {
            memSize = n + strlen(url) + 1;
            temp = (char *)calloc(1, memSize);
            if (temp == NULL)
            {
                CDX_LOGE("err_no_memory");
                return NULL;
            }
            memcpy(temp, baseURL, n);
            memcpy(temp + n, url, strlen(url) + 1);
        }
        else
        {
            slashPos = strrchr(protocolEnd, '/');

            if (slashPos != NULL)
            {
                memSize = slashPos - baseURL + strlen(url) + 2;
                temp = (char *)calloc(1, memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return NULL;
                }
                memcpy(temp, baseURL, slashPos - baseURL);
                *(temp+(slashPos - baseURL))='/';
                memcpy(temp+(slashPos - baseURL)+1, url, strlen(url) + 1);
            }
            else
            {
                memSize= n + strlen(url) + 2;
                temp = (char *)calloc(1, memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return NULL;
                }
                memcpy(temp, baseURL, n);
                *(temp + n)='/';
                memcpy(temp + n + 1, url, strlen(url) + 1);
            }

        }
    }
    out = temp;
    return out;
}
static void destroyPlsList(PlaylistT *playList)
{
    if(playList)
    {
        if(playList->mBaseURI)
        {
            free(playList->mBaseURI);
        }

        PlsItemT *e, *p;
        p = playList->mItems;
        while (p)
        {
            e = p->next;

            if (p->mURI)
            {
                free(p->mURI);
            }
            if (p->pTitle)
            {
                free(p->pTitle);
            }
            free(p);
            p = e;
        }

        free(playList);
    }
    return;
}

static int PlsParse(char *data, cdx_uint32 size, PlaylistT **P, const char *baseURI)
{
    cdx_uint32 offset = 0;
    int seqNum = 0;
    int err = 0;
    char *line = NULL;
    char *lineKey;
    char *lineValue;
    int   iItem = -1;
    char *pUrl = NULL;
    char *pOriUrl = NULL;
    char *title = NULL;
    cdx_int64 iDuration = -1;

    PlaylistT *playList = calloc(1, sizeof(PlaylistT));
    if(!playList)
    {
        CDX_LOGE("err_no_memory");
        return -1;
    }

    CDX_LOGD("baseURI=%s", baseURI);
    int baseLen = strlen(baseURI);
    playList->mBaseURI = calloc(1, baseLen+1);
    if(!playList->mBaseURI)
    {
        CDX_LOGE("err_no_memory");
        err = -1;
        goto _err;
    }

    memcpy(playList->mBaseURI, baseURI, baseLen+1);

    while(offset < size)
    {
        while(offset < size && isspace(data[offset]))
        {
            offset++;
        }
        if(offset >= size)
        {
            break;
        }
        cdx_uint32 offsetLF = offset;
        while (offsetLF < size && data[offsetLF] != '\n')
        {
            ++offsetLF;
        }

        cdx_uint32 offsetData = offsetLF;

        while(isspace(data[offsetData-1]))
        {
            --offsetData;
        }

        if ((offsetData - offset)<=0)
        {
            offset = offsetLF + 1;
            continue;
        }
        else
        {
            line = (char *)malloc(offsetData - offset + 1);
            if(!line)
            {
                CDX_LOGE("err_no_memory");
                err = -1;
                goto _err;
            }
            memcpy(line, &data[offset], offsetData - offset);
            line[offsetData - offset] = '\0';
            CDX_LOGI("%s", line);
        }

        //now we have one line.
        if( !strncasecmp(line, "[playlist]", sizeof("[playlist]")-1 ) ||
            !strncasecmp(line, "[Reference]", sizeof("[Reference]")-1 ) )
        {
            free(line);
            line = NULL;
            offset = offsetLF + 1;
            continue;
        }

        lineKey = line;
        lineValue = strchr(line, '=');
        if(lineValue)
        {
            *lineValue='\0';
            lineValue++;
        }
        else
        {
            free(line);
            line = NULL;
            offset = offsetLF + 1;
            continue;
        }

        if(!strcasecmp(lineKey, "version"))
        {
            CDX_LOGD("pls file version: %s", lineKey);
            free(line);
            line = NULL;
            offset = offsetLF + 1;
            continue;
        }
        if(!strcasecmp(lineKey, "numberofentries"))
        {
            CDX_LOGD("pls should have %d entries", atoi(lineValue));
            free(line);
            line = NULL;
            offset = offsetLF + 1;
            continue;
        }

        //find the number part of of file1, title1 or length1 etc.
        int iNewItem;
        if(sscanf(lineKey, "%*[^0-9]%d", &iNewItem) != 1)
        {
            CDX_LOGD("couldn't find number of items");
            free(line);
            line = NULL;
            offset = offsetLF + 1;
            continue;
        }

        if(iItem == -1)
            iItem = iNewItem;
        else if(iItem != iNewItem)
        {
            //we found a new item, insert the previous.
            if(pUrl)
            {
                PlsItemT *item= (PlsItemT *)calloc(1, sizeof(PlsItemT));
                if (!item)
                {
                    CDX_LOGE("err_no_memory");
                    err = -1;
                    goto _err;
                }

                item->mURI = strdup(pUrl);
                if(item->mURI == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    err = -1;
                    goto _err;
                }
                if(title != NULL)
                {
                    item->pTitle = strdup(title);
                    if(item->pTitle == NULL)
                    {
                        CDX_LOGE("err_no_memory");
                        err = -1;
                        goto _err;
                    }
                }
                free(pOriUrl);
                pOriUrl = pUrl = NULL;

                item->durationUs = iDuration;
                item->seqNum = seqNum;

                playList->lastSeqNum = seqNum;
                if (playList->mItems == NULL)
                {
                    playList->mItems = item;
                }
                else
                {
                    PlsItemT *item2 = playList->mItems;
                    while(item2->next != NULL)
                        item2 = item2->next;
                    item2->next = item;
                }
                (playList->mNumItems)++;
                seqNum++;
            }
            else
            {
                CDX_LOGW("no file= part found for item %d", iItem);
            }
            free(title);
            title = NULL;
            iDuration = -1;
            iItem = iNewItem;
        }

        if(!strncasecmp(lineKey, "file", sizeof("file") - 1) ||
            !strncasecmp(lineKey, "Ref", sizeof("Ref") - 1))
        {
            free(pOriUrl);
            pOriUrl = pUrl = plsMakeURL(playList->mBaseURI, lineValue);

            if(!strncasecmp(lineKey, "Ref", sizeof("Ref") - 1))
            {
                if(!strncasecmp(pUrl, "http://", sizeof("http://") - 1))
                    memcpy(pUrl, "mmsh", 4);
            }
        }
        else if(!strncasecmp(lineKey, "title", sizeof("title") - 1))
        {
            free(title);
            title = strdup(lineValue);
        }
        else if(!strncasecmp(lineKey, "length", sizeof("length") - 1))
        {
            iDuration = atoll(lineValue);
            if(iDuration != -1)
            {
                iDuration *= 1000000;//us
            }
        }
        else
        {
            CDX_LOGW("unknown key found in pls file: %s", lineKey);
        }
        free(line);
        line = NULL;
        offset = offsetLF + 1;
    }

    //Add last object. //todo reuse.
    if(pUrl)
    {
        PlsItemT *item = (PlsItemT *)calloc(1, sizeof(PlsItemT));
        if (!item)
        {
            CDX_LOGE("err_no_memory");
            err = -1;
            goto _err;
        }

        item->mURI = strdup(pUrl);
        if(item->mURI == NULL)
        {
            CDX_LOGE("err_no_memory");
            err = -1;
            goto _err;
        }
        if(title != NULL)
        {
            item->pTitle = strdup(title);
            if(item->pTitle == NULL)
            {
                CDX_LOGE("err_no_memory");
                err = -1;
                goto _err;
            }
        }
        free(pOriUrl);
        pOriUrl = pUrl = NULL;

        item->durationUs = iDuration;
        item->seqNum = seqNum;

        playList->lastSeqNum = seqNum;
        if (playList->mItems == NULL)
        {
            playList->mItems = item;
        }
        else
        {
            PlsItemT *item2 = playList->mItems;
            while(item2->next != NULL)
                item2 = item2->next;
            item2->next = item;
        }
        (playList->mNumItems)++;
        seqNum++;
    }
    else
    {
        CDX_LOGW("no file= part found for item %d", iItem);
    }
    free(title);
    title = NULL;

    if(playList->mNumItems <= 0)
    {
        CDX_LOGE("playList->mNumItems <= 0");
        goto _err;
    }
    *P = playList;
    return 0;

_err:
    if(line != NULL)
    {
        free(line);
    }
    destroyPlsList(playList);
    return err;
}


/*return -1:error*/
static int DownloadParsePls(CdxPlsParser *plsParser)
{
    int ret, readSize = 0;
    while(1)
    {
        if(plsParser->forceStop == 1)
        {
            return -1;
        }
        ret = CdxStreamRead(plsParser->cdxStream,
            plsParser->plsBuf + readSize, 1024);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            return -1;
        }
        if(ret == 0)
        {
            break;
        }
        readSize += ret;
    }

    if(readSize == 0)
    {
        CDX_LOGE("download pls fail");
        return -1;
    }

    PlaylistT *pls = NULL;
    int err;
    if ((err = PlsParse(plsParser->plsBuf, readSize, &pls,
        plsParser->plsUrl)) < 0)
    {
        CDX_LOGE("create pls fail, err=%d", err);
        return -1;
    }
    plsParser->mPls = pls;
    return 0;
}

static PlsItemT *findPlsItemBySeqNum(PlaylistT *playlist, int seqNum)
{
    PlsItemT *item = playlist->mItems;
    while(item->seqNum != seqNum)
    {
        item = item->next;
    }
    return item;
}

/*return -1:error*/
/*return 0:plsParser PSR_EOS*/
/*return 1:选择到了新的Segment*/
static int plsSelectNextSegment(CdxPlsParser *plsParser)
{
    PlaylistT *pls = plsParser->mPls;
    if(plsParser->curSeqNum > pls->lastSeqNum)
    {
        return 0;
    }
    PlsItemT *item = findPlsItemBySeqNum(pls, plsParser->curSeqNum);
    if(!item)
    {
        CDX_LOGE("findPlsItemBySeqNum fail");
        return -1;
    }
    SetDataSouceForPlsSegment(item, &plsParser->cdxDataSource);
    plsParser->baseTimeUs = item->baseTimeUs;
    return 1;
}

static cdx_int32 __PlsParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    CdxPlsParser *plsParser = (CdxPlsParser*)parser;
    if(plsParser->status < CDX_PLS_IDLE)
    {
        CDX_LOGE("status < CDX_PLS_IDLE, PlsParserGetMediaInfo invaild");
        plsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    memcpy(pMediaInfo, &plsParser->mediaInfo, sizeof(CdxMediaInfoT));
    return 0;
}

static cdx_int32 __PlsParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxPlsParser *plsParser = (CdxPlsParser*)parser;
    if(plsParser->status != CDX_PLS_IDLE && plsParser->status != CDX_PLS_PREFETCHED)
    {
        CDX_LOGW("status != CDX_PLS_IDLE && status != CDX_PLS_PREFETCHED,"
            " PlsParserPrefetch invaild");
        plsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(plsParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if(plsParser->status == CDX_PLS_PREFETCHED)
    {
        memcpy(pkt, &plsParser->cdxPkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&plsParser->statusLock);
    if(plsParser->forceStop)
    {
        pthread_mutex_unlock(&plsParser->statusLock);
        return -1;
    }
    plsParser->status = CDX_PLS_PREFETCHING;
    pthread_mutex_unlock(&plsParser->statusLock);

    memset(pkt, 0, sizeof(CdxPacketT));
    int ret, mErrno;
    ret = CdxParserPrefetch(plsParser->child, pkt);
    if(ret < 0)
    {
        mErrno = CdxParserGetStatus(plsParser->child);
        if(mErrno == PSR_EOS)
        {
            pthread_mutex_lock(&plsParser->statusLock);
            if(plsParser->forceStop)
            {
                plsParser->mErrno = PSR_USER_CANCEL;
                ret = -1;
                pthread_mutex_unlock(&plsParser->statusLock);
                goto _exit;
            }
            ret = CdxParserClose(plsParser->child);
            CDX_CHECK(ret == 0);
            plsParser->child = NULL;
            plsParser->childStream = NULL;
            pthread_mutex_unlock(&plsParser->statusLock);

            plsParser->curSeqNum++;
            CDX_LOGI("curSeqNum = %d", plsParser->curSeqNum);
            ret = plsSelectNextSegment(plsParser);
            if(ret < 0)
            {
                CDX_LOGE("Select Next Segment fail.");
                plsParser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }
            else if(ret == 0)
            {
                CDX_LOGD("plsParser->status = PSR_EOS");
                plsParser->mErrno = PSR_EOS;
                ret = -1;
                goto _exit;
            }
            CDX_LOGD("replace parser");
            ret = CdxParserPrepare(&plsParser->cdxDataSource, NO_NEED_DURATION,
                &plsParser->statusLock, &plsParser->forceStop,
                &plsParser->child, &plsParser->childStream, NULL, NULL);
            if(ret < 0)
            {
                plsParser->curSeqNum = -1;
                if(!plsParser->forceStop)
                {
                    CDX_LOGE("CdxParserPrepare fail");
                    plsParser->mErrno = PSR_UNKNOWN_ERR;
                }
                goto _exit;
            }

            CdxMediaInfoT mediaInfo;
            ret = CdxParserGetMediaInfo(plsParser->child, &mediaInfo);
            if(ret < 0)
            {
                CDX_LOGE(" CdxParserGetMediaInfo fail. ret(%d)", ret);
                plsParser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }

            ret = CdxParserPrefetch(plsParser->child, pkt);
            if(ret < 0)
            {
                if(!plsParser->forceStop)
                {
                    CDX_LOGE(" prefetch error! ret(%d)", ret);
                    plsParser->mErrno = PSR_UNKNOWN_ERR;
                }
                goto _exit;
            }
        }
        else
        {
            plsParser->mErrno = mErrno;
            CDX_LOGE("CdxParserPrefetch mErrno = %d", mErrno);
            goto _exit;
        }
    }

    pkt->pts += plsParser->baseTimeUs;
    memcpy(&plsParser->cdxPkt, pkt, sizeof(CdxPacketT));
    pthread_mutex_lock(&plsParser->statusLock);
    plsParser->status = CDX_PLS_PREFETCHED;
    pthread_mutex_unlock(&plsParser->statusLock);
    pthread_cond_signal(&plsParser->cond);
    CDX_LOGV("CdxParserPrefetch pkt->pts=%lld, pkt->type=%d, pkt->length=%d",
        pkt->pts, pkt->type, pkt->length);
    return 0;
_exit:
    pthread_mutex_lock(&plsParser->statusLock);
    plsParser->status = CDX_PLS_IDLE;
    pthread_mutex_unlock(&plsParser->statusLock);
    pthread_cond_signal(&plsParser->cond);
    return ret;
}

static cdx_int32 __PlsParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxPlsParser *plsParser = (CdxPlsParser*)parser;
    if(plsParser->status != CDX_PLS_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PLS_PREFETCHED, we can not read!");
        plsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    pthread_mutex_lock(&plsParser->statusLock);
    if(plsParser->forceStop)
    {
        pthread_mutex_unlock(&plsParser->statusLock);
        return -1;
    }
    plsParser->status = CDX_PLS_READING;
    pthread_mutex_unlock(&plsParser->statusLock);

    int ret = CdxParserRead(plsParser->child, pkt);
    pthread_mutex_lock(&plsParser->statusLock);
    plsParser->status = CDX_PLS_IDLE;
    pthread_mutex_unlock(&plsParser->statusLock);
    pthread_cond_signal(&plsParser->cond);
    return ret;
}

cdx_int32 PlsParserForceStop(CdxParserT *parser)
{
    CDX_LOGI("PlsParserForceStop start");
    CdxPlsParser *plsParser = (CdxPlsParser*)parser;
    int ret;
    pthread_mutex_lock(&plsParser->statusLock);
    plsParser->forceStop = 1;
    if(plsParser->child)
    {
        ret = CdxParserForceStop(plsParser->child);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserForceStop fail, ret(%d)", ret);
        }
    }
    else if(plsParser->childStream)
    {
        ret = CdxStreamForceStop(plsParser->childStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamForceStop fail");
        }
    }

    if(plsParser->tmpChild)
    {
        ret = CdxParserForceStop(plsParser->tmpChild);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserForceStop fail, ret(%d)", ret);
            //plsParser->mErrno = CdxParserGetStatus(plsParser->child);
            //return -1;
        }
    }
    else if(plsParser->tmpChildStream)
    {
        ret = CdxStreamForceStop(plsParser->tmpChildStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamForceStop fail");
            //hlsParser->mErrno = PSR_UNKNOWN_ERR;
        }
    }
    while(plsParser->status != CDX_PLS_IDLE && plsParser->status != CDX_PLS_PREFETCHED)
    {
        pthread_cond_wait(&plsParser->cond, &plsParser->statusLock);
    }
    plsParser->mErrno = PSR_USER_CANCEL;
    plsParser->status = CDX_PLS_IDLE;
    pthread_mutex_unlock(&plsParser->statusLock);
    CDX_LOGD("PlsParserForceStop end");
    return 0;

}

cdx_int32 PlsParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGI("PlsParserClrForceStop start");
    CdxPlsParser *plsParser = (CdxPlsParser*)parser;
    if(plsParser->status != CDX_PLS_IDLE)
    {
        CDX_LOGW("status != CDX_PLS_IDLE");
        plsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    plsParser->forceStop = 0;
    if(plsParser->child)
    {
        int ret = CdxParserClrForceStop(plsParser->child);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserClrForceStop fail, ret(%d)", ret);
            return ret;
        }
    }
    CDX_LOGI("PlsParserClrForceStop end");
    return 0;

}

static int __PlsParserControl(CdxParserT *parser, int cmd, void *param)
{
    CdxPlsParser *plsParser = (CdxPlsParser*)parser;
    (void)plsParser;
    (void)param;

    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI(" pls parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            return PlsParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return PlsParserClrForceStop(parser);
        default:
            break;
    }
    return 0;
}

cdx_int32 __PlsParserGetStatus(CdxParserT *parser)
{
    CdxPlsParser *plsParser = (CdxPlsParser *)parser;
    return plsParser->mErrno;
}

cdx_int32 __PlsParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_LOGI("PlsParserSeekTo start, timeUs = %lld", timeUs);
    CdxPlsParser *plsParser = (CdxPlsParser *)parser;
    plsParser->mErrno = PSR_OK;
    if(timeUs < 0)
    {
        CDX_LOGE("timeUs invalid");
        plsParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    else if(timeUs >= plsParser->mPls->durationUs)
    {
        CDX_LOGI("PSR_EOS");
        plsParser->mErrno = PSR_EOS;
        return 0;
    }

    pthread_mutex_lock(&plsParser->statusLock);
    if(plsParser->forceStop)
    {
        plsParser->mErrno = PSR_USER_CANCEL;
        CDX_LOGE("PSR_USER_CANCEL");
        pthread_mutex_unlock(&plsParser->statusLock);
        return -1;
    }
    plsParser->status = CDX_PLS_SEEKING;
    pthread_mutex_unlock(&plsParser->statusLock);

    int ret = 0;
    PlaylistT *pls = plsParser->mPls;
    cdx_int64 mDurationUs = 0;
    PlsItemT *item = pls->mItems;
    while(item)
    {
        mDurationUs += item->durationUs;
        if (mDurationUs > timeUs)
            break;
        item = item->next;
    }
    if(!item)
    {
        CDX_LOGE("unknown error");
        plsParser->mErrno = PSR_UNKNOWN_ERR;
        ret = -1;
        goto _exit;
    }
    mDurationUs -= item->durationUs;
    if(item->seqNum != plsParser->curSeqNum)
    {
        pthread_mutex_lock(&plsParser->statusLock);
        if(plsParser->forceStop)
        {
            plsParser->mErrno = PSR_USER_CANCEL;
            ret = -1;
            pthread_mutex_unlock(&plsParser->statusLock);
            goto _exit;
        }
        if(plsParser->child)
        {
            ret = CdxParserClose(plsParser->child);
            CDX_CHECK(ret == 0);
            plsParser->child = NULL;
            plsParser->childStream = NULL;
        }
        pthread_mutex_unlock(&plsParser->statusLock);

        SetDataSouceForPlsSegment(item, &plsParser->cdxDataSource);
        ret = CdxParserPrepare(&plsParser->cdxDataSource, NO_NEED_DURATION,
            &plsParser->statusLock, &plsParser->forceStop,
            &plsParser->child, &plsParser->childStream, NULL, NULL);
        if(ret < 0)
        {
            plsParser->curSeqNum = -1;
            if(!plsParser->forceStop)
            {
                CDX_LOGE("CdxParserPrepare fail");
                plsParser->mErrno = PSR_UNKNOWN_ERR;
            }
            ret = -1;
            goto _exit;
        }
        CdxMediaInfoT mediaInfo;
        ret = CdxParserGetMediaInfo(plsParser->child, &mediaInfo);
        if(ret < 0)
        {
            CDX_LOGE(" CdxParserGetMediaInfo fail. ret(%d)", ret);
            plsParser->mErrno = PSR_UNKNOWN_ERR;
            goto _exit;
        }
        plsParser->baseTimeUs = item->baseTimeUs;
        plsParser->curSeqNum = item->seqNum;
    }
    CDX_LOGI("mDurationUs = %lld", mDurationUs);

    ret = CdxParserSeekTo(plsParser->child, timeUs - mDurationUs, seekModeType);

_exit:

    pthread_mutex_lock(&plsParser->statusLock);
    plsParser->status = CDX_PLS_IDLE;
    pthread_mutex_unlock(&plsParser->statusLock);
    pthread_cond_signal(&plsParser->cond);
    CDX_LOGI("PlsParserSeekTo end, ret = %d", ret);
    return ret;
}

static cdx_int32 __PlsParserClose(CdxParserT *parser)
{
    CDX_LOGI("PlsParserClose start");
    CdxPlsParser *plsParser = (CdxPlsParser *)parser;
    int ret = PlsParserForceStop(parser);
    if(ret < 0)
    {
        CDX_LOGW("HlsParserForceStop fail");
    }

    if(plsParser->child)
    {
        ret = CdxParserClose(plsParser->child);
        CDX_CHECK(ret >= 0);
    }
    else if(plsParser->childStream)
    {
        ret = CdxStreamClose(plsParser->childStream);
    }

    if(plsParser->tmpChild)
    {
        ret = CdxParserClose(plsParser->tmpChild);
        CDX_CHECK(ret >= 0);
    }
    else if(plsParser->tmpChildStream)
    {
        ret = CdxStreamClose(plsParser->tmpChildStream);
    }

    ret = CdxStreamClose(plsParser->cdxStream);
    CDX_CHECK(ret >= 0);
    if(plsParser->plsUrl)
    {
        free(plsParser->plsUrl);
    }
    if(plsParser->plsBuf)
    {
        free(plsParser->plsBuf);
    }
    if(plsParser->mPls)
    {
        destroyPlsList(plsParser->mPls);
    }
    pthread_mutex_destroy(&plsParser->statusLock);
    pthread_cond_destroy(&plsParser->cond);
    free(plsParser);
    CDX_LOGI("PlsParserClose end");
    return 0;
}

cdx_uint32 __PlsParserAttribute(CdxParserT *parser)
{
    CdxPlsParser *plsParser = (CdxPlsParser *)parser;
    return plsParser->flags;
}

int __PlsParserInit(CdxParserT *parser)
{
    CDX_LOGI("CdxPlsOpenThread start");
    CdxPlsParser *plsParser = (CdxPlsParser*)parser;

    if(DownloadParsePls(plsParser) < 0)
    {
        CDX_LOGE("DownloadParsePls fail");
        goto _exit;
    }

    PlaylistT *pls = plsParser->mPls;
    PlsItemT *item = pls->mItems;
    int ret;
    cdx_uint32 i;
    CdxMediaInfoT *pMediaInfo;
    CdxMediaInfoT mediaInfo;
    for(i = 0; i < pls->mNumItems; i++)
    {
        SetDataSouceForPlsSegment(item, &plsParser->cdxDataSource);
        //plsParser->cdxDataSource->uri = item->mURI;

        if(!i)
        {
            ret = CdxParserPrepare(&plsParser->cdxDataSource, NO_NEED_DURATION,
                &plsParser->statusLock, &plsParser->forceStop,
                &plsParser->child, &plsParser->childStream, NULL, NULL);
            if(ret < 0)
            {
                CDX_LOGE("CreatMediaParser fail");
                goto _exit;
            }
            pMediaInfo = &plsParser->mediaInfo;
            ret = CdxParserGetMediaInfo(plsParser->child, pMediaInfo);
            if(ret < 0)
            {
                CDX_LOGE("plsParser->child getMediaInfo error!");
                goto _exit;
            }
            pls->durationUs += item->durationUs;
            break;
        }
#if 0
        else
        {
            ret = CdxParserPrepare(&plsParser->cdxDataSource, NO_NEED_DURATION,
                &plsParser->statusLock, &plsParser->forceStop,
                &plsParser->tmpChild, &plsParser->tmpChildStream, NULL, NULL);
            if(ret < 0)
            {
                CDX_LOGE("CreatMediaParser fail");
                goto _exit;
            }
            pMediaInfo = &mediaInfo;
            ret = CdxParserGetMediaInfo(plsParser->tmpChild, pMediaInfo);
            if(ret < 0)
            {
                CDX_LOGE("plsParser->child getMediaInfo error!");
            }
            ret = CdxParserClose(plsParser->tmpChild);
            plsParser->tmpChild = NULL;
            plsParser->tmpChildStream = NULL;
            if(ret < 0)
            {
                CDX_LOGE("CdxParserClose fail!");
                goto _exit;
            }
        }
        item->baseTimeUs = pls->durationUs;
        item->durationUs = (cdx_int64)(pMediaInfo->program[0].duration * 1000);
        pls->durationUs += item->durationUs;
        item = item->next;
#endif
    }

    pMediaInfo = &plsParser->mediaInfo;
    pMediaInfo->fileSize = -1;
    pMediaInfo->bSeekable = 1;
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pMediaInfo->program[0].duration = pls->durationUs / 1000; //duration ms
    CDX_LOGD("duration: %lld ms, %u ms", pls->durationUs / 1000, pMediaInfo->program[0].duration);
    //PrintMediaInfo(pMediaInfo);
    plsParser->mErrno = PSR_OK;
    pthread_mutex_lock(&plsParser->statusLock);
    plsParser->status = CDX_PLS_IDLE;
    pthread_mutex_unlock(&plsParser->statusLock);
    pthread_cond_signal(&plsParser->cond);
    CDX_LOGI("CdxPlsOpenThread success");
    return 0;

_exit:
    plsParser->mErrno = PSR_OPEN_FAIL;
    pthread_mutex_lock(&plsParser->statusLock);
    plsParser->status = CDX_PLS_IDLE;
    pthread_mutex_unlock(&plsParser->statusLock);
    pthread_cond_signal(&plsParser->cond);
    CDX_LOGE("CdxPlsOpenThread fail");
    return -1;
}
static struct CdxParserOpsS plsParserOps =
{
    .control         = __PlsParserControl,
    .prefetch         = __PlsParserPrefetch,
    .read             = __PlsParserRead,
    .getMediaInfo     = __PlsParserGetMediaInfo,
    .close             = __PlsParserClose,
    .seekTo            = __PlsParserSeekTo,
    .attribute        = __PlsParserAttribute,
    .getStatus        = __PlsParserGetStatus,
    .init            = __PlsParserInit
};

static CdxParserT *__PlsParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    CdxPlsParser *plsParser;
    (void)flags;

    plsParser = calloc(1, sizeof(CdxPlsParser));
    if(!plsParser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }

    int ret = pthread_cond_init(&plsParser->cond, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_mutex_init(&plsParser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);

    char *tmpUrl;
    ret = CdxStreamGetMetaData(stream, "uri", (void **)&tmpUrl);
    if(ret < 0)
    {
        CDX_LOGE("get uri of the stream fail!");
        goto open_error;
    }
    plsParser->cdxStream = stream;
    int urlLen = strlen(tmpUrl) + 1;
    plsParser->plsUrl = calloc(1, urlLen);
    if(!plsParser->plsUrl)
    {
        CDX_LOGE("malloc fail!");
        goto open_error;
    }

    memcpy(plsParser->plsUrl, tmpUrl, urlLen);

    plsParser->plsBufSize = 128*1024;
    plsParser->plsBuf = calloc(1, plsParser->plsBufSize);
    if (!plsParser->plsBuf)
    {
        CDX_LOGE("allocate memory fail for pls file");
        goto open_error;
    }

    plsParser->base.ops = &plsParserOps;
    plsParser->base.type = CDX_PARSER_PLS;
    plsParser->mErrno = PSR_INVALID;
    plsParser->status = CDX_PLS_INITIALIZED;
    return &plsParser->base;

open_error:
    CdxStreamClose(stream);
    if(plsParser->plsBuf)
    {
        free(plsParser->plsBuf);
    }
    if(plsParser->plsUrl)
    {
        free(plsParser->plsUrl);
    }
    pthread_mutex_destroy(&plsParser->statusLock);
    pthread_cond_destroy(&plsParser->cond);
    free(plsParser);
    return NULL;
}

static int PlsProbe(char *data, cdx_uint32 size)
{
    cdx_uint32 offset = 0;
    while (offset < size && isspace(data[offset]))
    {
        offset++;
    }
    if(offset + 10 >= size)
    {
        return 0;
    }

    if (strncasecmp(data + offset, "[playlist]", 10))
        return 0;

    return 1;
}

static cdx_uint32 __PlsParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_LOGD("PlsParserProbe");

    if(probeData->len < 10)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }
    int ret = PlsProbe(probeData->buf, probeData->len);

    return ret*100;

}

CdxParserCreatorT plsParserCtor =
{
    .create    = __PlsParserOpen,
    .probe     = __PlsParserProbe
};
