/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxBdParser.c
 * Description : BdParser
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxBdParser"
#include "CdxBdParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#include <stdlib.h>
#include <errno.h>
#define Skip8Bits(p)   (p++)
#define Skip16Bits(p)   (p += 2)
#define Skip32Bits(p)   (p += 4)
#define GetBE8Bits(p)   (*p++)
#define MKBETAG(a,b,c,d) ((d) | ((c) << 8) | ((b) << 16) | ((unsigned)(a) << 24))
#if 0
#define GetBE16Bits(p)  (((*(p++)) << 8) | (*(p++)))
//#define GetBE24Bits(p)  (((*(p)++) << 16) | ((*(p)++) << 8) | (*(p)++))
#define GetBE32Bits(p)  (((*(p++)) << 24) | ((*(p++)) << 16) | ((*(p++)) << 8) | (*(p++)))
#endif
#define GetBE16Bits(p)  ({\
    cdx_uint32 myTmp = GetBE8Bits(p);\
    myTmp << 8 | GetBE8Bits(p);})
#define GetBE32Bits(p)  ({\
    cdx_uint32 myTmp = GetBE16Bits(p);\
    myTmp << 16 | GetBE16Bits(p);})

//(((unsigned dat = *(p++)) << 8) | (*(p++)))
//#define GetBE24Bits(p)  (((*(p)++) << 16) | ((*(p)++) << 8) | (*(p)++))
//#define GetBE32Bits(p)  (((*(p++)) << 24) | ((*(p++)) << 16) | ((*(p++)) << 8) | (*(p++)))
#if 0

CDX_INTERFACE cdx_uint32 GetBE16Bits(cdx_uint8 *p)
{
    cdx_uint32 data = GetBE8Bits(p);
    return data << 8 | GetBE8Bits(p);

}
CDX_INTERFACE cdx_uint32 GetBE32Bits(cdx_uint8 *p)
{
    cdx_uint32 data = GetBE16Bits(p);
    return data << 16 | GetBE16Bits(p);

}
#endif

static void *ExtOpendir(CdxStreamT *stream, char *dirUrl)
{
    struct ExtIoctlParamS param;
    param.cmd = EXT_IO_OPERATION_OPENDIR;
    param.inPath = dirUrl;

    CdxStreamControl(stream, STREAM_CMD_EXT_IO_OPERATION, &param);
    if (param.outRet == -1)
    {
        CDX_LOGE("io operation opendir failure...");
        return NULL;
    }
    return param.outHdr;
}

static int ExtReaddir(CdxStreamT *stream, void *dirHdr, char *buf, int bufLen)
{
    struct ExtIoctlParamS param;
    param.cmd = EXT_IO_OPERATION_READDIR;
    param.inHdr = dirHdr;
    param.inBuf = buf;
    param.inBufLen = bufLen;

    CdxStreamControl(stream, STREAM_CMD_EXT_IO_OPERATION, &param);
    return param.outRet;
}

static int ExtClosedir(CdxStreamT *stream, void *dirHdr)
{
    struct ExtIoctlParamS param;
    param.cmd = EXT_IO_OPERATION_CLOSEDIR;
    param.inHdr = dirHdr;

    CdxStreamControl(stream, STREAM_CMD_EXT_IO_OPERATION, &param);
    return param.outRet;
}

static int ExtOpen(CdxStreamT *stream, char *fileUrl)
{
    struct ExtIoctlParamS param;
    param.cmd = EXT_IO_OPERATION_OPENFILE;
    param.inPath = fileUrl;

    CdxStreamControl(stream, STREAM_CMD_EXT_IO_OPERATION, &param);

    if (param.outRet == -1)
    {
        CDX_LOGE("open file failure");
        return -1;
    }

    int fd = (int)(intptr_t)param.outHdr;
    CDX_LOGV("open file success, fd(%d)", fd);
    return fd;
}

static int ExtAccess(CdxStreamT *stream, char *fileUrl)
{
    struct ExtIoctlParamS param;
    param.cmd = EXT_IO_OPERATION_ACCESS;
    param.inPath = fileUrl;

    CdxStreamControl(stream, STREAM_CMD_EXT_IO_OPERATION, &param);
    return param.outRet;
}

static int OpenDir(CdxBdParser *bdParser, char *dirUrl)
{
    void *dirHdr = ExtOpendir(bdParser->cdxStream, dirUrl);
    if ((intptr_t)dirHdr < 0)
    {
        CDX_LOGE("ExtOpendir failure...");
        return -1;
    }

    bdParser->dir = dirHdr;
    return 0;
}

static int ReadDir(CdxBdParser *bdParser)
{
    return ExtReaddir(bdParser->cdxStream, bdParser->dir, bdParser->filename, 512);
}

static int CloseDir(CdxBdParser *bdParser)
{
    return ExtClosedir(bdParser->cdxStream, bdParser->dir);
}

static int OpenFile(CdxBdParser *bdParser, char *fileUrl)
{
    int fd = ExtOpen(bdParser->cdxStream, fileUrl);
    if (fd == -1)
    {
        CDX_LOGE("open file failure...(%s)", fileUrl);
        return -1;
    }
    bdParser->fd = fd;
    CDX_LOGV("dup a new fd(%d)", bdParser->fd);
    return 0;
}

static int ReadFile(CdxBdParser *bdParser)
{
    memset(bdParser->fileBuf, 0x00, bdParser->fileBufSize);
    int fileLen = lseek(bdParser->fd, 0, SEEK_END);
    if (fileLen <= 0 || fileLen > bdParser->fileBufSize)
    {
        CDX_LOGW("read file fail, fd(%d) fileLen(%d), BufSize(%d)",
            bdParser->fd, fileLen, bdParser->fileBufSize);
        close(bdParser->fd);
        bdParser->fd = -1;
        return -1;
    }

    lseek(bdParser->fd, 0, SEEK_SET);
    bdParser->fileValidDataSize = read(bdParser->fd, bdParser->fileBuf, fileLen);

    close(bdParser->fd);
    bdParser->fd = -1;

    return 0;
}

static int CheckAccess(CdxBdParser *bdParser, char *fileUrl)
{
    return ExtAccess(bdParser->cdxStream, fileUrl);
}

static cdx_int32 BdParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    CdxBdParser *bdParser = (CdxBdParser*)parser;
    if(bdParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGE("status < CDX_PSR_IDLE, BdParserGetMediaInfo invaild");
        bdParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    memcpy(pMediaInfo, &bdParser->mediaInfo, sizeof(CdxMediaInfoT));
    return 0;
}

static cdx_int32 BdParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxBdParser *bdParser = (CdxBdParser*)parser;
    if(bdParser->status != CDX_PSR_IDLE && bdParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGW("BdParserPrefetch invaild");
        bdParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(bdParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if(bdParser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(pkt, &bdParser->pkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&bdParser->statusLock);
    if(bdParser->forceStop)
    {
        pthread_mutex_unlock(&bdParser->statusLock);
        return -1;
    }
    bdParser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&bdParser->statusLock);

    bdParser->prefetchType = 0;
    if(bdParser->streamPts[1] < bdParser->streamPts[0])
    {
        bdParser->prefetchType = 1;
    }
    if((bdParser->streamPts[2] < bdParser->streamPts[1])
        &&(bdParser->streamPts[2] < bdParser->streamPts[0]))
    {
        bdParser->prefetchType = 2;
    }
//    CDX_LOGD("bdParser->prefetchType = %d", bdParser->prefetchType);

    PlayItem *playItem;
    SubPlayItem *subPlayItem;
    Clip *clip;
    CdxStreamT *cdxStream = NULL;

    int mErrno, m = 0, ret = 0;
next_child:
    if(m == 3)
    {
        CDX_LOGD("bdParser->status = PSR_EOS");
        bdParser->mErrno = PSR_EOS;
        ret = -1;
        goto _exit;
    }
    if(!bdParser->child[bdParser->prefetchType])
    {
        CDX_LOGD("child[%d]== NULL", bdParser->prefetchType);
        m++;
        bdParser->prefetchType = (bdParser->prefetchType + 1)%3;
        goto next_child;
    }
_prefetch:
    ret = CdxParserPrefetch(bdParser->child[bdParser->prefetchType], pkt);
    if(ret < 0)
    {
        mErrno = CdxParserGetStatus(bdParser->child[bdParser->prefetchType]);
        if(mErrno == PSR_EOS)
        {
            CDX_LOGD("child EOS, bdParser->prefetchType = %d", bdParser->prefetchType);
            if(bdParser->prefetchType == 2)
            {
                m++;
                bdParser->prefetchType = (bdParser->prefetchType + 1)%3;
                goto next_child;
            }
            else if(bdParser->prefetchType == 0)
            {
                bdParser->curPlayItemIndex = bdParser->curClip->finalItemIndex + 1;
                //bdParser->curPlayItemIndex++;
                if(bdParser->curPlayItemIndex >= bdParser->curPlaylist->numer_of_PlayItems)
                {
                    m++;
                    bdParser->prefetchType = (bdParser->prefetchType + 1)%3;
                    goto next_child;
                }
                CDX_LOGD("curSeqNum = %d", bdParser->curPlayItemIndex);

                playItem = bdParser->curPlaylist->playItems + bdParser->curPlayItemIndex;
                clip = &playItem->clip;
                bdParser->curClip = clip;

                int fd;
                sprintf(bdParser->fileUrl, "STREAM/%s.m2ts", clip->ClipName);
                CDX_LOGD("child[0](%s)", bdParser->fileUrl);
                fd = ExtOpen(bdParser->cdxStream, bdParser->fileUrl);
                if (fd == -1)
                {
                    CDX_LOGE("ExtOpen failure");
                    goto _exit;
                }
                sprintf(bdParser->fileUrl, "fd://%d?offset=0&length=0", fd);

                bdParser->cdxDataSource.uri = bdParser->fileUrl;
                ret = CdxStreamOpen(&bdParser->cdxDataSource,
                    &bdParser->statusLock, &bdParser->forceStop, &cdxStream, NULL);
                close(fd);
                if(ret < 0)
                {
                    if(!bdParser->forceStop)
                    {
                        CDX_LOGE(" CdxStreamOpen error!");
                        bdParser->mErrno = PSR_UNKNOWN_ERR;
                    }
                    ret = -1;
                    goto _exit;
                }
                pthread_mutex_lock(&bdParser->statusLock);
                ret = CdxParserControl(bdParser->child[bdParser->prefetchType],
                    CDX_PSR_CMD_REPLACE_STREAM, (void *)cdxStream);
                pthread_mutex_unlock(&bdParser->statusLock);
                if(ret < 0)
                {
                    bdParser->mErrno = PSR_UNKNOWN_ERR;
                    goto _exit;
                }
                int i;
                for(i = 0; i < 3; i++)
                {
                    if(bdParser->child[i])
                    {
                        bdParser->streamPts[i] = 0;
                    }
                }
                goto _prefetch;
                /*
                ret = CdxParserPrefetch(bdParser->child[bdParser->prefetchType], pkt);
                if(ret < 0)
                {
                    CDX_LOGE(" prefetch error!");
                    bdParser->mErrno = PSR_UNKNOWN_ERR;
                    goto _exit;
                }
                */

            }
            else
            {
                //bdParser->curSubPlayItemIndex++;
                bdParser->curSubPlayItemIndex = bdParser->curSubClip->finalItemIndex + 1;
                if(bdParser->curSubPlayItemIndex >=
                    bdParser->curPlaylist->subPathE->number_of_SubPlayItems)
                {
                    m++;
                    bdParser->prefetchType = (bdParser->prefetchType + 1)%3;
                    goto next_child;
                }
                CDX_LOGD("curSeqNum = %d", bdParser->curSubPlayItemIndex);
                subPlayItem = bdParser->curPlaylist->subPathE->subPlayItems +
                    bdParser->curSubPlayItemIndex;
                clip = &subPlayItem->clip;
                bdParser->curSubClip = clip;

                int fd;
                sprintf(bdParser->fileUrl, "STREAM/%s.m2ts", clip->ClipName);
                CDX_LOGD("child[1](%s)", bdParser->fileUrl);
                fd = ExtOpen(bdParser->cdxStream, bdParser->fileUrl);
                if (fd == -1)
                {
                    CDX_LOGE("ExtOpen failure");
                    goto _exit;
                }
                sprintf(bdParser->fileUrl, "fd://%d?offset=0&length=0", fd);

                bdParser->cdxDataSource.uri = bdParser->fileUrl;
                ret = CdxStreamOpen(&bdParser->cdxDataSource,
                    &bdParser->statusLock, &bdParser->forceStop, &cdxStream, NULL);
                close(fd);
                if(ret < 0)
                {
                    if(!bdParser->forceStop)
                    {
                        CDX_LOGE(" CdxStreamOpen error!");
                        bdParser->mErrno = PSR_UNKNOWN_ERR;
                    }
                    ret = -1;
                    goto _exit;
                }
                pthread_mutex_lock(&bdParser->statusLock);
                ret = CdxParserControl(bdParser->child[bdParser->prefetchType],
                    CDX_PSR_CMD_REPLACE_STREAM, (void *)cdxStream);
                pthread_mutex_unlock(&bdParser->statusLock);
                if(ret < 0)
                {
                    CDX_LOGE("CDX_PSR_CMD_REPLACE_STREAM fail, ret(%d)", ret);
                    bdParser->mErrno = PSR_UNKNOWN_ERR;
                    goto _exit;
                }
                int i;
                for(i = 0; i < 3; i++)
                {
                    if(bdParser->child[i])
                    {
                        bdParser->streamPts[i] = 0;
                    }
                }
                goto _prefetch;
                /*
                ret = CdxParserPrefetch(bdParser->child[bdParser->prefetchType], pkt);
                if(ret < 0)
                {
                    if(!bdParser->forceStop)
                    {
                        CDX_LOGE(" prefetch error! ret(%d)", ret);
                        bdParser->mErrno = PSR_UNKNOWN_ERR;
                    }
                    goto _exit;
                }
                */
            }
        }
        else
        {
            bdParser->mErrno = mErrno;
            CDX_LOGE("CdxParserPrefetch mErrno = %d", mErrno);
            goto _exit;
        }
    }
//    CDX_LOGD("bdParser->prefetchType = %d", bdParser->prefetchType);
    if(!bdParser->prefetchType)
    {
        if(pkt->type == CDX_MEDIA_VIDEO)
        {
            //CDX_LOGD("pkt->length = %d", pkt->length);
            bdParser->streamPts[0] = pkt->pts;
        }
        else if(pkt->type == CDX_MEDIA_AUDIO)
        {
            bdParser->streamPts[3] = pkt->pts;
        }
        /*
        else if(pkt->type == CDX_MEDIA_SUBTITLE)//此时text subtitle不存在
        {
            bdParser->streamPts[2] = pkt->pts;
        }
        */
    }
    else
    {

        //CDX_LOGD("pkt->length = %d", pkt->length);
        bdParser->streamPts[bdParser->prefetchType] = pkt->pts;
    }

    if(bdParser->child[1]
        && bdParser->streamPts[0] + 2000000LL < bdParser->streamPts[1])
    {
        CDX_LOGW("maybe BD_BASE pts jump");
        bdParser->streamPts[1] = 0;
    }
    memcpy(&bdParser->pkt, pkt, sizeof(CdxPacketT));
    pthread_mutex_lock(&bdParser->statusLock);
    bdParser->status = CDX_PSR_PREFETCHED;
    pthread_mutex_unlock(&bdParser->statusLock);
    pthread_cond_signal(&bdParser->cond);
    return 0;
_exit:
    pthread_mutex_lock(&bdParser->statusLock);
    bdParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&bdParser->statusLock);
    pthread_cond_signal(&bdParser->cond);
    return ret;
}

static cdx_int32 BdParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxBdParser *bdParser = (CdxBdParser*)parser;
    if(bdParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_PREFETCHED, we can not read!");
        bdParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
   /*
    pthread_mutex_lock(&bdParser->statusLock);
    if(bdParser->forceStop)
    {
        pthread_mutex_unlock(&bdParser->statusLock);
        return -1;
    }
    bdParser->status = CDX_PSR_READING;
    pthread_mutex_unlock(&bdParser->statusLock);*/

    int ret = CdxParserRead(bdParser->child[bdParser->prefetchType], pkt);
    bdParser->status = CDX_PSR_IDLE;
    return ret;
}

cdx_int32 BdParserForceStop(CdxParserT *parser)
{
    CDX_LOGI("BdParserForceStop start");
    CdxBdParser *bdParser = (CdxBdParser*)parser;

    int ret;
    pthread_mutex_lock(&bdParser->statusLock);
    bdParser->forceStop = 1;
    bdParser->mErrno = PSR_USER_CANCEL;
    if(bdParser->cdxStream)
    {
        ret = CdxStreamForceStop(bdParser->cdxStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamForceStop fail");
        }
    }
    int i;
    for(i = 0; i < 3; i++)
    {
        if(bdParser->child[i])
        {
            ret = CdxParserForceStop(bdParser->child[i]);
            if(ret < 0)
            {
                CDX_LOGE("CdxParserForceStop fail");
                //hlsParser->mErrno = CdxParserGetStatus(hlsParser->child[i]);
                //return -1;
            }
        }
        else if(bdParser->childStream[i])
        {
            ret = CdxStreamForceStop(bdParser->childStream[i]);
            if(ret < 0)
            {
                CDX_LOGW("CdxStreamForceStop fail");
                //hlsParser->mErrno = PSR_UNKNOWN_ERR;
            }
        }
    }

    while(bdParser->status != CDX_PSR_IDLE && bdParser->status != CDX_PSR_PREFETCHED)
    {
        pthread_cond_wait(&bdParser->cond, &bdParser->statusLock);
    }
    pthread_mutex_unlock(&bdParser->statusLock);
    bdParser->mErrno = PSR_USER_CANCEL;
    bdParser->status = CDX_PSR_IDLE;
    CDX_LOGI("BdParserForceStop end");
    return 0;
}

cdx_int32 BdParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGI("BdParserClrForceStop start");
    int i, ret;
    CdxBdParser *bdParser = (CdxBdParser*)parser;
    if(bdParser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("bdParser->status != CDX_PSR_IDLE");
        bdParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    bdParser->forceStop = 0;
    if(bdParser->cdxStream)
    {
        ret = CdxStreamClrForceStop(bdParser->cdxStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamClrForceStop fail");
            //hlsParser->mErrno = PSR_UNKNOWN_ERR;
            //return -1;
        }
    }
    for(i = 0; i < 3; i++)
    {
        if(bdParser->child[i])
        {
            ret = CdxParserClrForceStop(bdParser->child[i]);
            if(ret < 0)
            {
                CDX_LOGE("CdxParserClrForceStop fail");
                //hlsParser->mErrno = CdxParserGetStatus(hlsParser->child[i]);
                //return -1;
            }
        }
        else if(bdParser->childStream[i])
        {
            ret = CdxStreamClrForceStop(bdParser->childStream[i]);
            if(ret < 0)
            {
                CDX_LOGW("CdxStreamClrForceStop fail");
                //hlsParser->mErrno = PSR_UNKNOWN_ERR;
            }
        }
    }
    CDX_LOGI("BdParserClrForceStop end");
    return 0;
}

static int BdParserControl(CdxParserT *parser, int cmd, void *param)
{
    CdxBdParser *bdParser = (CdxBdParser*)parser;
    (void)param;
    (void)bdParser;

    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI(" bd parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            return BdParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return BdParserClrForceStop(parser);
        default:
            break;
    }
    return 0;
}

cdx_int32 BdParserGetStatus(CdxParserT *parser)
{
    CdxBdParser *bdParser = (CdxBdParser *)parser;
    return bdParser->mErrno;
}
cdx_int32 BdParserSeekTo(CdxParserT *parser, cdx_int64  timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    CDX_LOGD("BdParserSeekTo start, timeUs = %lld", timeUs);
    CdxBdParser *bdParser = (CdxBdParser *)parser;
    bdParser->mErrno = PSR_OK;
    Playlist *playlist = bdParser->curPlaylist;

    if(timeUs < 0)
    {
        CDX_LOGE("timeUs invalid");
        bdParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    else if(timeUs >= playlist->durationUs)
    {
        CDX_LOGI("PSR_EOS");
        bdParser->mErrno = PSR_EOS;
        return 0;
    }

    pthread_mutex_lock(&bdParser->statusLock);
    if(bdParser->forceStop)
    {
        bdParser->mErrno = PSR_USER_CANCEL;
        CDX_LOGE("PSR_USER_CANCEL");
        pthread_mutex_unlock(&bdParser->statusLock);
        return -1;
    }
    bdParser->status = CDX_PSR_SEEKING;
    pthread_mutex_unlock(&bdParser->statusLock);

    int ret = 0, i;
    int64_t mDurationUs = 0;
    PlayItem *item = NULL;
    SubPlayItem *subItem;
    Clip *clip;
    CdxStreamT *cdxStream = NULL;
    for(i = 0; i < playlist->numer_of_PlayItems; i++)
    {
        item = playlist->playItems + i;
        mDurationUs += item->durationUs;
        if (mDurationUs > timeUs)
            break;
    }
    mDurationUs -= item->durationUs;
    int64_t diff = timeUs - mDurationUs;
    CDX_LOGD("timeUs=%lld, mDurationUs=%lld, diff=%lld", timeUs, mDurationUs, diff);

    bdParser->curSubPlayItemIndex = bdParser->curPlayItemIndex = i;

    clip = &item->clip;
    bdParser->curClip = clip;

    int fd;
    sprintf(bdParser->fileUrl, "STREAM/%s.m2ts", clip->ClipName);
    CDX_LOGD("child[0](%s)", bdParser->fileUrl);
    fd = ExtOpen(bdParser->cdxStream, bdParser->fileUrl);
    if (fd == -1)
    {
        CDX_LOGE("ExtOpen failure");
        goto _exit;
    }
    sprintf(bdParser->fileUrl, "fd://%d?offset=0&length=0", fd);

    bdParser->cdxDataSource.uri = bdParser->fileUrl;
    ret = CdxStreamOpen(&bdParser->cdxDataSource,
        &bdParser->statusLock, &bdParser->forceStop, &cdxStream, NULL);
    close(fd);
    if(ret < 0)
    {
        if(!bdParser->forceStop)
        {
            CDX_LOGE(" CdxStreamOpen error!");
            bdParser->mErrno = PSR_UNKNOWN_ERR;
        }
        ret = -1;
        goto _exit;
    }
    pthread_mutex_lock(&bdParser->statusLock);
    ret = CdxParserControl(bdParser->child[0],
        CDX_PSR_CMD_REPLACE_STREAM, (void *)cdxStream);
    pthread_mutex_unlock(&bdParser->statusLock);
    if(ret < 0)
    {
        CDX_LOGE("CDX_PSR_CMD_REPLACE_STREAM fail");
        bdParser->mErrno = PSR_UNKNOWN_ERR;
        goto _exit;
    }

    if(bdParser->child[1])
    {

        clip = &((playlist->subPathE->subPlayItems + bdParser->curSubPlayItemIndex)->clip);
        bdParser->curSubClip = clip;

        int fd;
        sprintf(bdParser->fileUrl, "STREAM/%s.m2ts", clip->ClipName);
        CDX_LOGD("child[0](%s)", bdParser->fileUrl);
        fd = ExtOpen(bdParser->cdxStream, bdParser->fileUrl);
        if (fd == -1)
        {
            CDX_LOGE("ExtOpen failure");
            goto _exit;
        }
        sprintf(bdParser->fileUrl, "fd://%d?offset=0&length=0", fd);

        bdParser->cdxDataSource.uri = bdParser->fileUrl;
        ret = CdxStreamOpen(&bdParser->cdxDataSource,
            &bdParser->statusLock, &bdParser->forceStop, &cdxStream, NULL);
        close(fd);
        if(ret < 0)
        {
            if(!bdParser->forceStop)
            {
                CDX_LOGE(" CdxStreamOpen error!");
                bdParser->mErrno = PSR_UNKNOWN_ERR;
            }
            ret = -1;
            goto _exit;
        }
        pthread_mutex_lock(&bdParser->statusLock);
        ret = CdxParserControl(bdParser->child[1],
            CDX_PSR_CMD_REPLACE_STREAM, (void *)cdxStream);
        pthread_mutex_unlock(&bdParser->statusLock);
        if(ret < 0)
        {
            CDX_LOGE("CDX_PSR_CMD_REPLACE_STREAM fail");
            bdParser->mErrno = PSR_UNKNOWN_ERR;
            goto _exit;
        }
    }

    if(item->ditto)
    {
        clip = item->ditto;
    }
    else
    {
        clip = &item->clip;
    }
    int STC_id = item->ref_to_STC_id;
    STC *stc = clip->atc->stc + STC_id;
    cdx_uint32 aimPts = item->IN_time + diff * 45/1000;

    //CDX_LOGD("IN_time=%u, aimPts=%u", item->IN_time, aimPts);
    StreamInfo *stream = NULL;
    for(i = 0; i < clip->ps->number_of_streams_in_ps; i++)
    {
        stream = clip->ps->streams + i;
        if(stream->mediaType == TYPE_VIDEO)
        {
            break;
        }
    }
    EPMapForOneStream *epMapForOneStream = stream->epMapForOneStream;
    EPMapFine *epfine = NULL, *epfineNext;
    for(i = 0; i < epMapForOneStream->number_of_EP_fine_entries; i++)
    {
        epfine = epMapForOneStream->fine + i;
        if(epfine->SPN_EP_fine < stc->SPN_STC_start)
        {
            continue;
        }
        if(i == epMapForOneStream->number_of_EP_fine_entries - 1)
        {
            break;
        }
        epfineNext = epMapForOneStream->fine + i + 1;
        if(epfineNext->PTS_EP_fine > aimPts )
        {
            break;
        }
    }
    bdParser->streamSeekPos.pos = (cdx_int64)epfine->SPN_EP_fine * 192;
//    bdParser->streamCtl.cmd = STREAM_CTL_SEEK;
//    bdParser->streamCtl.param = &bdParser->streamSeekPos;
    ret = CdxParserControl(bdParser->child[0],
    CDX_PSR_CMD_STREAM_SEEK, &bdParser->streamSeekPos);
    if(ret < 0)
    {
        bdParser->mErrno = PSR_UNKNOWN_ERR;
        goto _exit;
    }
    bdParser->streamPts[0] = 0;
    bdParser->streamPts[3] = 0;

    if(bdParser->child[1])
    {
        subItem = playlist->subPathE->subPlayItems + bdParser->curSubPlayItemIndex;
        if(subItem->ditto)
        {
            clip = subItem->ditto;
        }
        else
        {
            clip = &subItem->clip;
        }
        STC_id = subItem->ref_to_STC_id;
        stc = clip->atc->stc + STC_id;
        //cdx_uint32 aimPts = item->IN_time + diff * 45/1000;
        //StreamInfo *stream;
        for(i = 0; i < clip->ps->number_of_streams_in_ps; i++)
        {
            stream = clip->ps->streams + i;
            CDX_LOGD("clip=%p, clip->ps=%p, clip->ps->streams=%p, stream = %p",
                clip, clip->ps, clip->ps->streams, stream);
            if(stream->mediaType == TYPE_VIDEO)
            {
                break;
            }
        }
        epMapForOneStream = stream->epMapForOneStream;
        for(i = 0; i < epMapForOneStream->number_of_EP_fine_entries; i++)
        {
            epfine = epMapForOneStream->fine + i;
            if(epfine->SPN_EP_fine < stc->SPN_STC_start)
            {
                continue;
            }
            if(i == epMapForOneStream->number_of_EP_fine_entries - 1)
            {
                break;
            }
            epfineNext = epMapForOneStream->fine + i + 1;
            if(epfineNext->PTS_EP_fine > aimPts )
            {
                break;
            }
        }
        bdParser->streamSeekPos.pos = (cdx_int64)epfine->SPN_EP_fine * 192;
//        bdParser->streamCtl.cmd = STREAM_CTL_SEEK;
//        bdParser->streamCtl.param = &bdParser->streamSeekPos;
        ret = CdxParserControl(bdParser->child[1], CDX_PSR_CMD_STREAM_SEEK,
        &bdParser->streamSeekPos);
        if(ret < 0)
        {
            bdParser->mErrno = PSR_UNKNOWN_ERR;
            goto _exit;
        }
        bdParser->streamPts[1] = 0;
    }

    if(bdParser->child[2])
    {
        //To do
        bdParser->streamPts[2] = 0;
    }

_exit:
    pthread_mutex_lock(&bdParser->statusLock);
    bdParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&bdParser->statusLock);
    pthread_cond_signal(&bdParser->cond);
    CDX_LOGV("BdParserSeekTo end, ret = %d", ret);
    return ret;
}
CDX_INTERFACE void ClearProgramInfo(Clip *clip)
{
    if(clip->ps)
    {
        if(clip->ps->streams)
        {
            int i;
            for(i = 0; i < clip->ps->number_of_streams_in_ps; i++)
            {
                if((clip->ps->streams + i)->metadata)
                {
                    free((clip->ps->streams + i)->metadata);
                }
            }
            free(clip->ps->streams);
        }
        free(clip->ps);
    }
}
CDX_INTERFACE void ClearCPI(Clip *clip)
{
    if(clip->epMapForOneStream)
    {
        if(clip->epMapForOneStream->coarse)
        {
            free(clip->epMapForOneStream->coarse);
        }
        if(clip->epMapForOneStream->fine)
        {
            free(clip->epMapForOneStream->fine);
        }
        free(clip->epMapForOneStream);
    }
}
CDX_INTERFACE void DestroyClip(Clip *clip)
{
    if(clip->atc)
    {
        if(clip->atc->stc)
        {
            free(clip->atc->stc);
        }
        free(clip->atc);
    }
    ClearProgramInfo(clip);
    ClearCPI(clip);
}
CDX_INTERFACE void DestroyPlaylist(Playlist *playlist)
{
    int i;
    if(playlist->playItems)
    {
        for(i = 0; i < playlist->numer_of_PlayItems; i++)
        {
            DestroyClip(&(playlist->playItems + i)->clip);
        }
        free(playlist->playItems);
    }
    if(playlist->subPath)
    {
        if(playlist->subPath->subPlayItems)
        {
            for(i = 0; i < playlist->subPath->number_of_SubPlayItems; i++)
            {
                DestroyClip(&(playlist->subPath->subPlayItems + i)->clip);
            }
            free(playlist->subPath->subPlayItems);
        }
        free(playlist->subPath);
    }
    if(playlist->subPathE)
    {
        if(playlist->subPathE->subPlayItems)
        {
            for(i = 0; i < playlist->subPathE->number_of_SubPlayItems; i++)
            {
                DestroyClip(&(playlist->subPathE->subPlayItems + i)->clip);
            }
            free(playlist->subPathE->subPlayItems);
        }
        free(playlist->subPathE);
    }
    free(playlist);
}
CDX_INTERFACE void DestroyPlaylists(CdxBdParser *bdParser)
{
    Playlist *posPlaylist, *nextPlaylist;

    CdxListForEachEntrySafe(posPlaylist, nextPlaylist, &bdParser->playlists, node)
    {
        CdxListDel(&posPlaylist->node);
        DestroyPlaylist(posPlaylist);
    }
}
static cdx_int32 BdParserClose(CdxParserT *parser)
{
    CdxBdParser *bdParser = (CdxBdParser *)parser;
    int ret = BdParserForceStop(parser);
    if(ret < 0)
    {
        CDX_LOGW("BdParserForceStop fail");
    }
    if(bdParser->cdxStream)
    {
        ret = CdxStreamClose(bdParser->cdxStream);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamClose fail, ret(%d)", ret);
        }
    }
    int i;
    for(i = 0; i < 3; i++)
    {
        if(bdParser->child[i])
        {
            ret = CdxParserClose(bdParser->child[i]);
            if(ret < 0)
            {
                CDX_LOGE("CdxParserClose fail");
            }
        }
        else if(bdParser->childStream[i])
        {
            ret = CdxStreamClose(bdParser->childStream[i]);
            if(ret < 0)
            {
                CDX_LOGW("CdxStreamForceStop fail");
                //hlsParser->mErrno = PSR_UNKNOWN_ERR;
            }
        }
    }
    DestroyPlaylists(bdParser);

    if(bdParser->fileUrl)
    {
        free(bdParser->fileUrl);
    }
    if(bdParser->fileBuf)
    {
        free(bdParser->fileBuf);
    }
    pthread_mutex_destroy(&bdParser->statusLock);
    pthread_cond_destroy(&bdParser->cond);
    free(bdParser);
    return 0;
}

cdx_uint32 BdParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    CdxBdParser *bdParser = (CdxBdParser *)parser;
    return bdParser->flags;
}

CDX_INTERFACE Playlist *CreatPlaylist(void)
{
    Playlist *playlist = CdxMalloc(sizeof(Playlist));
    memset(playlist, 0x00, sizeof(Playlist));
    return playlist;
}
cdx_int64 parsePlayItem(PlayItem *playItem, cdx_uint8 *data)
{
    memcpy(playItem->clip.ClipName, data, 5);
    playItem->clip.ClipName[5] = 0;
    data += 11;
    playItem->ref_to_STC_id = GetBE8Bits(data);
    playItem->IN_time = GetBE32Bits(data);
    playItem->OUT_time = GetBE32Bits(data);
    playItem->durationUs = (cdx_int64)(playItem->OUT_time - playItem->IN_time)*1000LL/45;
    return playItem->durationUs;
}
int parseSubPlayItem(SubPlayItem *subPlayItem, cdx_uint8 *data)
{
    memcpy(subPlayItem->clip.ClipName, data, 5);
    subPlayItem->clip.ClipName[5] = 0;
    data += 13;
    subPlayItem->ref_to_STC_id = GetBE8Bits(data);
    subPlayItem->IN_time = GetBE32Bits(data);
    subPlayItem->OUT_time = GetBE32Bits(data);
    subPlayItem->sync_PlayItem_id = GetBE16Bits(data);
    subPlayItem->sync_start_PTS_of_PlayItem = GetBE32Bits(data);

    return 0;
}
int CheckPlayItemForPlaylist(CdxBdParser *bdParser, Playlist *playlist)
{
    PlayItem *playItem = playlist->playItems;
    Clip *clip = &playItem->clip;
    sprintf(bdParser->fileUrl, "STREAM/%s.m2ts", clip->ClipName);
    if(CheckAccess(bdParser, bdParser->fileUrl) < 0)
    {
        CDX_LOGW("CheckAccess fail, url(%s)", bdParser->fileUrl);
        return -1;
    }
    return 0;
}
int CheckSubPlayItemForPlaylist(CdxBdParser *bdParser, Playlist *playlist)
{
    if(playlist->subPathE)
    {
        SubPlayItem *subPlayItem = playlist->subPathE->subPlayItems;
        Clip *clip = &subPlayItem->clip;
        sprintf(bdParser->fileUrl, "STREAM/%s.m2ts", clip->ClipName);
        if(CheckAccess(bdParser, bdParser->fileUrl) < 0)
        {
            CDX_LOGW("Check SubPlayItem Access fail, url(%s)", bdParser->fileUrl);
            if(playlist->subPathE->subPlayItems)
            {
                int i;
                for(i = 0; i < playlist->subPathE->number_of_SubPlayItems; i++)
                {
                    DestroyClip(&(playlist->subPathE->subPlayItems + i)->clip);
                }
                free(playlist->subPathE->subPlayItems);
            }
            free(playlist->subPathE);
            playlist->subPathE = NULL;
            return 0;
        }
        return 1;
    }
    return 0;
}

cdx_int64 parseMplsRoughly(CdxBdParser *bdParser,
    Playlist *playlist, cdx_uint8 *data)
{
    cdx_uint8 *cur = data;
    if(GetBE32Bits(cur) != MKBETAG('M', 'P', 'L', 'S'))
    {
        CDX_LOGE("may not be playlist file");
        return -1;
    }
    cur += 4;
    playlist->Playlist_start_address = GetBE32Bits(cur);
    cur += 4;
    playlist->ExtensionData_start_address = GetBE32Bits(cur);
    cur += 160>>3;

    //AppInfoPlaylist
    cur += 5;
    playlist->Playlist_playback_type = GetBE8Bits(cur);
    if(playlist->Playlist_playback_type != 1)
    {
        return -1;
    }
    cur += 10;
    playlist->MVC_Base_view_R_flag = (GetBE8Bits(cur) & 0x10) >> 4;

    //
    cur = data + playlist->Playlist_start_address;
    cur += 6;
    playlist->numer_of_PlayItems = GetBE16Bits(cur);
    playlist->numer_of_SubPaths = GetBE16Bits(cur);
    playlist->playItems =
        (PlayItem *)CdxMalloc(playlist->numer_of_PlayItems * sizeof(PlayItem));
    memset(playlist->playItems, 0x00, playlist->numer_of_PlayItems * sizeof(PlayItem));
    PlayItem *playItem, *prePlayItem = NULL;
    int i, length;
    cdx_int64 durationUs;
    //cdx_uint8 *tmp;
    for(i = 0; i < playlist->numer_of_PlayItems; i++)
    {
        playItem = playlist->playItems + i;
        length = GetBE16Bits(cur);
        durationUs = parsePlayItem(playItem, cur);
        if(durationUs < 0)
        {
            CDX_LOGE("parsePlayItem fail");
            return -1;
        }
        if(!prePlayItem || memcmp(prePlayItem, playItem, sizeof(PlayItem)))
        {
            playlist->durationUs += durationUs;
            prePlayItem = playItem;
        }
        cur += length;
    }

    if(CheckPlayItemForPlaylist(bdParser, playlist) == -1)
    {
        CDX_LOGW("CheckPlayItemForPlaylist fail");
        return -1;
    }
    playlist->curPosition = cur;
    return playlist->durationUs;
}

int SelectPlaylists(CdxBdParser *bdParser)
{
    int ret = OpenDir(bdParser, "PLAYLIST");
    if(ret < 0)
    {
        CDX_LOGE("open dir fail:'PLAYLIST'");
        return -1;
    }
    char *extension;
    cdx_int64 maxPlayTime = 0, minPlayTime = 0, durationUs;
    int count = 0;
    Playlist *playlist, *playlist1;
    while(1)
    {
        ret = ReadDir(bdParser);
        if(ret < 0)
        {
            break;
        }
        extension = strstr(bdParser->filename, ".mpls");
        if(!extension || strcmp(extension, ".mpls"))
        {
            continue;
        }
        sprintf(bdParser->fileUrl, "PLAYLIST/%s", bdParser->filename);
        ret = OpenFile(bdParser, bdParser->fileUrl);
        if(ret < 0)
        {
            CDX_LOGW("open file fail:%s", bdParser->fileUrl);
            continue;
        }
        ret = ReadFile(bdParser);
        if(ret < 0)
        {
            CDX_LOGW("read file fail:%s", bdParser->fileUrl);
            continue;
        }
        playlist = CreatPlaylist();
        memcpy(playlist->mplsFileName, bdParser->filename, 11);
        durationUs = parseMplsRoughly(bdParser, playlist, bdParser->fileBuf);
        if(durationUs < 0 || (count >= 6 && durationUs <= minPlayTime))
        {
            if(durationUs < 0)
            {
                CDX_LOGW("parseMplsRoughly fail, return(%lld)", durationUs);
            }
            DestroyPlaylist(playlist);
            continue;
        }

        if (durationUs > maxPlayTime)
        {
            CdxListAdd(&playlist->node, &bdParser->playlists);
            if(!count)
            {
                minPlayTime = durationUs;
            }
            count++;
            maxPlayTime = durationUs;
        }
        else if (durationUs <= minPlayTime)
        {
            CdxListAddTail(&playlist->node, &bdParser->playlists);
            count++;
            minPlayTime = durationUs;
        }
        else
        {
            CdxListForEachEntry(playlist1, &bdParser->playlists, node)
            {
                if(playlist1->durationUs < durationUs)
                {
                    CdxListAddBefore(&playlist->node, &playlist1->node);
                    count++;
                    break;
                }
            }
        }

        if(count > 6)
        {
            playlist1 = CdxListEntry(bdParser->playlists.tail, typeof(*playlist1), node);
            Playlist *tmp = CdxListEntry(playlist1->node.prev, typeof(*tmp), node);
            minPlayTime = tmp->durationUs;
            CdxListDel(&playlist1->node);
            DestroyPlaylist(playlist1);
            count--;
        }

    }
    if(CloseDir(bdParser) < 0)
    {
        CDX_LOGE("close dir fail");
        return -1;
    }
    if(maxPlayTime > 30*60*1000*1000)
    {
        CdxListForEachEntrySafe(playlist, playlist1, &bdParser->playlists, node)
        {

            CDX_LOGD("SelectPlaylists playlist->durationUs = %lld", playlist->durationUs);
            if(playlist->durationUs < maxPlayTime - 5*60*1000*1000)//hkw
            {
                CdxListDel(&playlist->node);
                DestroyPlaylist(playlist);
                count--;
            }
        }
    }
    bdParser->playlistCount = count;
    return 0;
}

int parseClipInfo(Clip *clip, cdx_uint8 *data)
{
    cdx_uint8 *cur = data + 7;
    clip->application_type = GetBE8Bits(cur);
    CDX_LOGV("clip->application_type = %d", clip->application_type);
    cur += 8;
    clip->number_of_source_packets = GetBE32Bits(cur);
    return 0;
}
int parseSequenceInfo(Clip *clip, cdx_uint8 *data)
{
    cdx_uint8 *cur = data + 5;
    clip->number_of_ATC_sequences = GetBE8Bits(cur);
    CDX_CHECK(clip->number_of_ATC_sequences == 1);
    clip->atc = CdxMalloc(clip->number_of_ATC_sequences * sizeof(ATC));
    int i, j;
    ATC *atc;
    STC *stc;
    for(i = 0; i < clip->number_of_ATC_sequences; i++)
    {
        atc = clip->atc + i;
        atc->SPN_ATC_start = GetBE32Bits(cur);
        CDX_CHECK(atc->SPN_ATC_start == 0);
        atc->number_of_STC_sequences = GetBE8Bits(cur);
        atc->offset_STC_id = GetBE8Bits(cur);
        CDX_CHECK(atc->offset_STC_id == 0);
        atc->stc = CdxMalloc(atc->number_of_STC_sequences * sizeof(STC));
        for(j = 0; j < atc->number_of_STC_sequences; j++)
        {
            stc = atc->stc + j;
            stc->PCR_PID = GetBE16Bits(cur);
            stc->SPN_STC_start = GetBE32Bits(cur);
            stc->presentation_start_time = GetBE32Bits(cur);
            stc->presentation_end_time = GetBE32Bits(cur);
        }
    }
    return 0;
}

int parseCPI(Clip *clip, cdx_uint8 *data)
{
    cdx_uint8 *cur = data, *cur1;
    int length = GetBE32Bits(cur);
    if(!length)
    {
        return 0;
    }
    data += 6;
    cur += 3;
    clip->number_of_stream_PID_entries = GetBE8Bits(cur);
    ClearCPI(clip);
    clip->epMapForOneStream =
        CdxMalloc(clip->number_of_stream_PID_entries * sizeof(EPMapForOneStream));
    int i, j, tmp, tmp1, start_address, EP_fine_table_start_address;
    EPMapForOneStream *epMapForOneStream;
    EPMapCoarse *epMapCoarse;
    EPMapFine *epMapFine;
    StreamInfo *stream;
    for(i = 0; i < clip->number_of_stream_PID_entries; i++)
    {
        epMapForOneStream = clip->epMapForOneStream + i;
        epMapForOneStream->stream_PID = GetBE16Bits(cur);
        tmp = GetBE32Bits(cur);
        epMapForOneStream->EP_stream_type = (tmp>>18)&0x000f;
        epMapForOneStream->number_of_EP_coarse_entries = (tmp>>2)&0xffff;
        tmp1 = GetBE16Bits(cur);
        epMapForOneStream->number_of_EP_fine_entries = (tmp&0x3)<<16 | tmp1;
        start_address = GetBE32Bits(cur);

        cur1 = data + start_address;
        EP_fine_table_start_address = GetBE32Bits(cur1);
        epMapForOneStream->coarse =
            CdxMalloc(epMapForOneStream->number_of_EP_coarse_entries * sizeof(EPMapCoarse));
        epMapForOneStream->fine =
            CdxMalloc(epMapForOneStream->number_of_EP_fine_entries * sizeof(EPMapFine));
        for(j = 0; j < epMapForOneStream->number_of_EP_coarse_entries; j++)
        {
            epMapCoarse = epMapForOneStream->coarse + j;
            tmp = GetBE32Bits(cur1);
            epMapCoarse->ref_to_EP_fine_id = tmp>>14;
            epMapCoarse->PTS_EP_coarse = tmp&0x3fff;
            epMapCoarse->SPN_EP_coarse = GetBE32Bits(cur1);
        }

        cur1 = data + start_address + EP_fine_table_start_address;
        int k = 1;
        for(j = 0; j < epMapForOneStream->number_of_EP_fine_entries; j++)
        {
            epMapFine = epMapForOneStream->fine + j;
            tmp = GetBE32Bits(cur1);
            epMapFine->PTS_EP_fine = (tmp>>17) & 0x7ff;
            epMapFine->SPN_EP_fine = tmp & 0x1ffff;

            if(k >= epMapForOneStream->number_of_EP_coarse_entries
                || j < epMapForOneStream->coarse[k].ref_to_EP_fine_id)
            {
                epMapCoarse = &(epMapForOneStream->coarse[k-1]);
            }
            else
            {
                epMapCoarse = &(epMapForOneStream->coarse[k]);
                k++;
            }
            epMapFine->PTS_EP_fine = (epMapCoarse->PTS_EP_coarse << 18)
                | (epMapFine->PTS_EP_fine << 8);
            epMapFine->SPN_EP_fine |= epMapCoarse->SPN_EP_coarse & 0xfffe0000;

            CDX_LOGV("epMapFine->PTS_EP_fine(%u), epMapFine->SPN_EP_fine(%u)",
                epMapFine->PTS_EP_fine, epMapFine->SPN_EP_fine);
        }

        for(j = 0; j < clip->ps[0].number_of_streams_in_ps; j++)
        {
            stream = clip->ps[0].streams + j;
            if(stream->stream_PID == epMapForOneStream->stream_PID)
            {
                stream->epMapForOneStream = epMapForOneStream;
            }
        }
    }
    return 0;
}

int parseProgramInfo(Clip *clip, cdx_uint8 *data)
{
    cdx_uint8 *cur = data + 5, *cur1, tmp;
    clip->number_of_program_sequences = GetBE8Bits(cur);
    CDX_CHECK(clip->number_of_program_sequences == 1);

    ClearProgramInfo(clip);
    clip->ps = CdxMalloc(clip->number_of_program_sequences * sizeof(ProgramSequence));
    int i, j, length;
    ProgramSequence *ps;
    StreamInfo *stream;
    for(i = 0; i < clip->number_of_program_sequences; i++)
    {
        ps = clip->ps + i;
        ps->SPN_program_sequence_start = GetBE32Bits(cur);
        CDX_CHECK(ps->SPN_program_sequence_start == 0);
        ps->program_map_PID = GetBE16Bits(cur);
        ps->number_of_streams_in_ps = GetBE8Bits(cur);
        cur++;
        ps->streams = CdxMalloc(ps->number_of_streams_in_ps * sizeof(StreamInfo));
        memset(ps->streams, 0x00, ps->number_of_streams_in_ps * sizeof(StreamInfo));
        for(j = 0; j < ps->number_of_streams_in_ps; j++)
        {
            stream = ps->streams + j;
            stream->stream_PID = GetBE16Bits(cur);
            length = GetBE8Bits(cur);
            cur1 = cur;
            stream->coding_type = GetBE8Bits(cur1);
            if(stream->coding_type == 0x02 || //MPEG2
               stream->coding_type == 0x1B || //AVC
               stream->coding_type == 0x24 || //HEVC
               stream->coding_type == 0xEA || //VC1
               stream->coding_type == 0x20)//从流 MVC
            {
                stream->mediaType = TYPE_VIDEO;
                clip->videoCount++;
                stream->metadata = (VideoMetaData *)CdxMalloc(sizeof(VideoMetaData));
                memset(stream->metadata, 0x00, sizeof(VideoMetaData));
                VideoMetaData *metadata = (VideoMetaData *)stream->metadata;
                tmp = GetBE8Bits(cur1);
                metadata->videoFormat = tmp >> 4;
                metadata->frameRate = tmp & 0x0f;
                tmp = GetBE8Bits(cur1);
                metadata->aspectRatio = tmp >> 4;
                metadata->ccFlag = (tmp & 0x02)>>1;
                if(stream->coding_type == 0x20)
                {
                    CDX_LOGD("MVC Dependent view video(%d)",
                        ps->number_of_streams_in_ps);
                    break;//只取了一个从流
                }
            }
            else if(stream->coding_type == 0x80 ||
                    stream->coding_type == 0x81 ||
                    stream->coding_type == 0x82 ||
                    stream->coding_type == 0x83 ||
                    stream->coding_type == 0x84 ||
                    stream->coding_type == 0x85 ||
                    stream->coding_type == 0x86 ||
                    stream->coding_type == 0x87 ||
                    stream->coding_type == 0x88 ||
                    stream->coding_type == 0xa1 ||
                    stream->coding_type == 0xa2)
            {
                CDX_LOGV("############stream->coding_type(%d)", stream->coding_type);
                stream->mediaType = TYPE_AUDIO;
                clip->audioCount++;
                stream->metadata = (AudioMetaData *)CdxMalloc(sizeof(AudioMetaData));
                memset(stream->metadata, 0x00, sizeof(AudioMetaData));
                AudioMetaData *metadata = (AudioMetaData *)stream->metadata;
                tmp = GetBE8Bits(cur1);
                metadata->audioPresentationType = tmp >> 4;
                metadata->samplingFrequency = tmp & 0x0f;
                memcpy(stream->lang, cur1, 3);
            }
            else if(stream->coding_type == 0x90)
            {
                stream->mediaType = TYPE_SUBS_PG;
                clip->subsCount++;
                memcpy(stream->lang, cur1, 3);
            }
            else if(stream->coding_type == 0x92)
            {
                stream->mediaType = TYPE_SUBS_TEXT;
                clip->subsCount++;
                stream->character_code = GetBE8Bits(cur1);
                memcpy(stream->lang, cur1, 3);
            }
            else
            {
                stream->mediaType = -1;
            }
            cur += length;

        }
    }
    return 0;
}
int parseExtensionData(void *father, cdx_uint8 *data)
{
    Playlist *playlist = NULL;
    Clip *clip = NULL;

    cdx_uint8 *cur = data;
    int length = GetBE32Bits(cur);
    if(!length)
    {
        return 0;
    }
    //int data_block_start_address =
    GetBE32Bits(cur);
    cur += 3;
    int number_of_ext_data_entries = GetBE8Bits(cur);
    int i, ID1, ID2, ext_data_start_address, ext_data_length;
    for(i = 0; i < number_of_ext_data_entries; i++)
    {
        ID1 = GetBE16Bits(cur);
        ID2 = GetBE16Bits(cur);
        ext_data_start_address = GetBE32Bits(cur);
        ext_data_length = GetBE32Bits(cur);
        if(ID1 == 2 && ID2 == 2)
        {
            playlist = (Playlist *)father;
            cdx_uint8 *tmp = data + ext_data_start_address + 4;
            cdx_uint8 *tmp1;
            int number_of_SubPath_extensions = GetBE16Bits(tmp);
            int j, SubPath_type;
            for(j = 0; j < number_of_SubPath_extensions; j++)
            {
                length = GetBE32Bits(tmp);
                tmp1 = tmp + 1;
                SubPath_type = GetBE8Bits(tmp1);
                if(SubPath_type == 8)
                {
                    tmp1 += 3;
                    playlist->subPathE = (SubPath *)CdxMalloc(sizeof(SubPath));
                    playlist->subPathE->number_of_SubPlayItems = GetBE8Bits(tmp1);
                    playlist->subPathE->subPlayItems =
                        (SubPlayItem *)CdxMalloc(playlist->subPathE->number_of_SubPlayItems *
                        sizeof(SubPlayItem));
                    memset(playlist->subPathE->subPlayItems, 0x00,
                        playlist->subPathE->number_of_SubPlayItems * sizeof(SubPlayItem));
                    int k;
                    SubPlayItem *subPlayItem;
                    for(k = 0; k < playlist->subPathE->number_of_SubPlayItems; k++)
                    {
                        length = GetBE16Bits(tmp1);
                        subPlayItem = playlist->subPathE->subPlayItems + k;
                        parseSubPlayItem(subPlayItem, tmp1);
                        tmp1 += length;
                    }
                    break;
                }
                else
                {
                    tmp += length;
                }
            }

        }
        else if(ID1 == 2 && ID2 == 5)
        {
            clip = (Clip *)father;
            cdx_uint8 *tmp = data + ext_data_start_address;
            parseProgramInfo(clip, tmp);

        }
        else if(ID1 == 2 && ID2 == 6)
        {
            clip = (Clip *)father;
            cdx_uint8 *tmp = data + ext_data_start_address;
            parseCPI(clip, tmp);
        }
    }
    return 0;
}
int parseMpls(CdxBdParser *bdParser, Playlist *playlist, cdx_uint8 *data)
{
    cdx_uint8 *cur = data;
    if(bdParser->playlistIsAppointed)
    {
        cdx_int64 durationUs = parseMplsRoughly(bdParser, playlist, cur);
        if(durationUs < 0)
        {
            CDX_LOGW("parseMplsRoughly fail");
            CdxListDel(&playlist->node);
            DestroyPlaylist(playlist);
            bdParser->playlistCount--;
            return -1;
        }
    }
    //SubPath *subPath;
    int i, length, SubPath_type;
    cdx_uint8 *tmp;
    cur = playlist->curPosition;
    for(i = 0; i < playlist->numer_of_SubPaths; i++)
    {
        length = GetBE32Bits(cur);
        tmp = cur + 1;
        SubPath_type = GetBE8Bits(tmp);
        if(SubPath_type != 4)
        {
            cur += length;
            continue;
        }
        else
        {
            tmp += 3;
            playlist->subPath = (SubPath *)CdxMalloc(sizeof(SubPath));
            playlist->subPath->number_of_SubPlayItems = GetBE8Bits(tmp);
            CDX_CHECK(playlist->subPath->number_of_SubPlayItems == 1);
            playlist->subPath->subPlayItems =
                (SubPlayItem *)CdxMalloc(playlist->subPath->number_of_SubPlayItems *
                sizeof(SubPlayItem));
            memset(playlist->subPath->subPlayItems, 0x00,
                playlist->subPath->number_of_SubPlayItems * sizeof(SubPlayItem));
            int j;
            SubPlayItem *subPlayItem;
            for(j = 0; j < playlist->subPath->number_of_SubPlayItems; j++)
            {
                length = GetBE16Bits(tmp);
                subPlayItem = playlist->subPath->subPlayItems + j;
                parseSubPlayItem(subPlayItem, tmp);
                tmp += length;
            }
            break;
        }
    }
    if(playlist->ExtensionData_start_address != 0)
    {
        cur = data + playlist->ExtensionData_start_address;
        parseExtensionData(playlist, cur);
    }

    return CheckSubPlayItemForPlaylist(bdParser, playlist);
}
int processMpls(CdxBdParser *bdParser, Playlist *playlist)
{
    int ret;
    if(bdParser->playlistIsAppointed)
    {
        cdx_int64 size = CdxStreamSize(bdParser->cdxStream);
        if(size > bdParser->fileBufSize)
        {
            CDX_LOGE("size > bdParser->fileBufSize");
            return -1;
        }
        int readSize = 0;
        memset(bdParser->fileBuf, 0x00, bdParser->fileBufSize);
        while(1)
        {
            ret = CdxStreamRead(bdParser->cdxStream,
                bdParser->fileBuf + readSize, (cdx_int32)size - readSize);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamRead fail");
                return -1;
            }
            else if(ret == 0)
            {
                break;
            }
            readSize += ret;
        }
        bdParser->fileValidDataSize = readSize;
    }
    else
    {
        sprintf(bdParser->fileUrl, "PLAYLIST/%s", playlist->mplsFileName);
        CDX_LOGD("bdParser->fileUrl(%s)", bdParser->fileUrl);
        ret = OpenFile(bdParser, bdParser->fileUrl);
        if(ret < 0)
        {
            CDX_LOGW("open file fail:%s", bdParser->fileUrl);
            return ret;
        }
        ret = ReadFile(bdParser);
        if(ret < 0)
        {
            CDX_LOGW("read file fail:%s", bdParser->fileUrl);
            return ret;
        }
    }
    return parseMpls(bdParser, playlist, bdParser->fileBuf);
}
int parseDependentClpi(CdxBdParser *bdParser, Clip *clip)
{
    sprintf(bdParser->fileUrl, "CLIPINF/%s.clpi", clip->ClipName);
    int ret = OpenFile(bdParser, bdParser->fileUrl);
    if(ret < 0)
    {
        CDX_LOGW("open file fail:%s", bdParser->fileUrl);
        return ret;
    }
    ret = ReadFile(bdParser);
    if(ret < 0)
    {
        CDX_LOGW("read file fail:%s", bdParser->fileUrl);
        return ret;
    }
    cdx_uint8 *data = bdParser->fileBuf;
    cdx_uint8 *cur = data + 24;
    int ExtensionData_start_address = GetBE32Bits(cur);
    if(ExtensionData_start_address)
    {
        cur = data + ExtensionData_start_address;
        parseExtensionData(clip, cur);
    }
    return 0;

}

int parseClpi(CdxBdParser *bdParser, Clip *clip, int firstItemIndex)
{
    sprintf(bdParser->fileUrl, "CLIPINF/%s.clpi", clip->ClipName);
    int ret = OpenFile(bdParser, bdParser->fileUrl);
    if(ret < 0)
    {
        CDX_LOGW("open file fail:%s", bdParser->fileUrl);
        return ret;
    }
    ret = ReadFile(bdParser);
    if(ret < 0)
    {
        CDX_LOGW("read file fail:%s", bdParser->fileUrl);
        return ret;
    }
    cdx_uint8 *data = bdParser->fileBuf;
    cdx_uint8 *cur = data + 8;
    int SequenceInfo_start_address = GetBE32Bits(cur);
    int ProgramInfo_start_address = GetBE32Bits(cur);
    int CPI_start_address = GetBE32Bits(cur);
    cur += 4;
    int ExtensionData_start_address = GetBE32Bits(cur);
    cur += 96 >> 3;
    parseClipInfo(clip, cur);
    cur = data + SequenceInfo_start_address;
    parseSequenceInfo(clip, cur);
    cur = data + ProgramInfo_start_address;
    parseProgramInfo(clip, cur);
    cur = data + CPI_start_address;
    parseCPI(clip, cur);
    if(ExtensionData_start_address)
    {
        cur = data + ExtensionData_start_address;
        parseExtensionData(clip, cur);
    }
    clip->firstItemIndex = firstItemIndex;
    clip->finalItemIndex = firstItemIndex;
    return 0;
}
void PrintPlaylistInfo(Playlist *playlist)
{
    int i;
    PlayItem *playItem;
    Clip *clip;
    char name[1024] = {0};
    SubPlayItem *subPlayItem;
    for(i = 0; i < playlist->numer_of_PlayItems; i++)
    {
        playItem = playlist->playItems + i;
        clip = &playItem->clip;
        sprintf(name, "STREAM/%s.m2ts", clip->ClipName);
        CDX_LOGD("child[0](%s)", name);
    }
    if(playlist->subPathE)
    {
        for(i = 0; i < playlist->subPathE->number_of_SubPlayItems; i++)
        {
            subPlayItem = playlist->subPathE->subPlayItems + i;
            clip = &subPlayItem->clip;
            sprintf(name, "STREAM/%s.m2ts", clip->ClipName);
            CDX_LOGD("child[1](%s)", name);
        }
    }
}

int BdParserInit(CdxParserT *parser)
{
    CDX_LOGI("CdxBdParserInit start");
       CdxBdParser *bdParser = (CdxBdParser*)parser;

    int ret;
    Playlist *playlist, *playlist1;
//    char *tmp = strstr(bdParser->originalUrl, "BDMV/");

    if (0)  //!strncmp(bdParser->originalUrl + (bdParser->originalUrlLen - 5), ".mpls", 5))
    {
    // TODO: 上层指定mpls
    }
    else
    {
        ret = SelectPlaylists(bdParser);
        if(ret < 0)
        {
            CDX_LOGE("SelectPlaylists fail");
            goto _exit;
        }

        //check the 3D stream if exit
        /*
        memset(bdParser->fileUrl, 0x00, bdParser->originalUrlLen + 64);
        sprintf(bdParser->fileUrl, "%sSTREAM/SSIF/", bdParser->baseUrl);
        ret = CheckAccess(bdParser, bdParser->fileUrl);
        if(ret < 0)
        {
        }
        else
        {
            bdParser->hasMvcVideo = 1;
        }
        */
    }

    PlayItem *playItem, *prePlayItem = NULL;
    SubPlayItem *subPlayItem, *preSubPlayItem = NULL;

    //CdxListForEachEntry(playlist, &bdParser->playlists, node)
    CdxListForEachEntrySafe(playlist, playlist1, &bdParser->playlists, node)
    {
        CDX_LOGD("playlist->durationUs = %lld", playlist->durationUs);
        ret = processMpls(bdParser, playlist);
        if(ret < 0)
        {
            CDX_LOGE("parseMplsRoughly fail");
            goto _exit;
        }
        else if(ret == 1)
        {
            bdParser->hasMvcVideo = 1;
            bdParser->curPlaylist = playlist;
            break;
        }

    }
    if(!bdParser->playlistCount)
    {
        CDX_LOGE("bdParser->playlistCount==0 !!");
        goto _exit;
    }
    if(!bdParser->curPlaylist)
    {
        bdParser->curPlaylist =
            CdxListEntry(bdParser->playlists.head, typeof(*bdParser->curPlaylist), node);
    }

    playlist = bdParser->curPlaylist;
    CDX_LOGD("curPlaylist=%s", playlist->mplsFileName);
    int i, flag1 = 0, flag2 = 0, flag3 = 0;
    for(i = 0; i < playlist->numer_of_PlayItems; i++)
    {
        playItem = playlist->playItems + i;
        if(prePlayItem
            && !memcmp(prePlayItem->clip.ClipName, playItem->clip.ClipName, 5))
        {
            if(prePlayItem->ditto)
            {
                playItem->ditto = prePlayItem->ditto;
            }
            else
            {
                playItem->ditto = &prePlayItem->clip;
            }
            playItem->ditto->finalItemIndex = i;
            prePlayItem = playItem;
            continue;
        }
        ret = parseClpi(bdParser, &playItem->clip, i);
        if(ret < 0 || playItem->clip.application_type != 1)
        {
            CDX_LOGE("parseClpi fail");
            goto _exit;
        }
        prePlayItem = playItem;
        if(!flag1)
        {
            playlist->videoCount = playItem->clip.videoCount;
            playlist->audioCount = playItem->clip.audioCount;
            playlist->subsCount = playItem->clip.subsCount;
            flag1 = 1;
        }
        /*
        if(playlist->subPathE)
        {
            subPlayItem = &(playlist->subPathE->subPlayItems[i]);
            ret = parseDependentClpi(bdParser, &subPlayItem->clip);
            if(ret < 0)
            {
                CDX_LOGE("parseClpi fail");
                goto _exit;
            }
            if(!flag2)
            {
                playlist->videoCount += subPlayItem->clip.videoCount;
                flag2 = 1;
            }
        }
        */
    }
    if(playlist->subPathE)
    {
        for(i = 0; i < playlist->subPathE->number_of_SubPlayItems; i++)
        {
            subPlayItem = playlist->subPathE->subPlayItems + i;
            if(preSubPlayItem
                && !memcmp(preSubPlayItem->clip.ClipName, subPlayItem->clip.ClipName, 5))
            {
                if(preSubPlayItem->ditto)
                {
                    subPlayItem->ditto = preSubPlayItem->ditto;
                }
                else
                {
                    subPlayItem->ditto = &preSubPlayItem->clip;
                }

                subPlayItem->ditto->finalItemIndex = i;
                preSubPlayItem = subPlayItem;
                continue;
            }
            ret = parseClpi(bdParser, &subPlayItem->clip, i);
            if(ret < 0 || subPlayItem->clip.application_type != 8)
            {
                CDX_LOGE("parseClpi fail");
                goto _exit;
            }
            preSubPlayItem = subPlayItem;
            if(!flag2)
            {
                playlist->videoCount += subPlayItem->clip.videoCount;
                flag2 = 1;
            }
        }
    }

    if(playlist->subPath)
    {
        CDX_CHECK(playlist->subPath->number_of_SubPlayItems == 1);
        for(i = 0; i < playlist->subPath->number_of_SubPlayItems; i++)
        {
            subPlayItem = playlist->subPath->subPlayItems + i;
            ret = parseClpi(bdParser, &subPlayItem->clip, i);
            if(ret < 0 || subPlayItem->clip.application_type != 6)
            {
                CDX_LOGE("parseClpi fail");
                goto _exit;
            }
            if(!flag3)
            {
                playlist->subsCount += subPlayItem->clip.subsCount;
                flag3 = 1;
            }
        }
    }
    PrintPlaylistInfo(playlist);
    playItem = playlist->playItems + bdParser->curPlayItemIndex;
    Clip *clip = &playItem->clip;
    int fd = -1;
    bdParser->curClip = clip;

    sprintf(bdParser->fileUrl, "STREAM/%s.m2ts", clip->ClipName);
    CDX_LOGD("child[0](%s)", bdParser->fileUrl);
    fd = ExtOpen(bdParser->cdxStream, bdParser->fileUrl);
    if (fd == -1)
    {
        CDX_LOGE("ExtOpen failure");
        goto _exit;
    }
    sprintf(bdParser->fileUrl, "fd://%d?offset=0&length=0", fd);

    bdParser->cdxDataSource.uri = bdParser->fileUrl;
//    bdParser->cdxDataSource.extraDataType = EXTRA_DATA_APPOINTED_TS;
    int flags = bdParser->flags | BD_BASE;
    if(playlist->subPath)
    {
        flags = bdParser->flags | DISABLE_SUBTITLE;
    }

    ret = CdxParserPrepare(&bdParser->cdxDataSource, flags,
        &bdParser->statusLock, &bdParser->forceStop,
        &bdParser->child[0], &bdParser->childStream[0], NULL, NULL);
    close(fd);
    if(ret < 0)
    {
        CDX_LOGE("CdxParserPrepare fail");
        goto _exit;
    }

    if(playlist->subPathE)
    {
        subPlayItem = playlist->subPathE->subPlayItems + bdParser->curSubPlayItemIndex;
        clip = &subPlayItem->clip;
        bdParser->curSubClip = clip;

        sprintf(bdParser->fileUrl, "STREAM/%s.m2ts", clip->ClipName);
        CDX_LOGD("child[1](%s)", bdParser->fileUrl);
        fd = ExtOpen(bdParser->cdxStream, bdParser->fileUrl);
        if (fd == -1)
        {
            CDX_LOGE("ExtOpen failure");
            goto _exit;
        }
        sprintf(bdParser->fileUrl, "fd://%d?offset=0&length=0", fd);

        bdParser->cdxDataSource.uri = bdParser->fileUrl;

        flags = bdParser->flags | BD_DEPENDENT | DISABLE_SUBTITLE | DISABLE_AUDIO;
        ret = CdxParserPrepare(&bdParser->cdxDataSource, flags,
            &bdParser->statusLock, &bdParser->forceStop,
            &bdParser->child[1], &bdParser->childStream[1], NULL, NULL);
        close(fd);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserPrepare fail");
            goto _exit;
        }
    }
    if(playlist->subPath)
    {
        subPlayItem = playlist->subPath->subPlayItems;
        clip = &subPlayItem->clip;

        sprintf(bdParser->fileUrl, "STREAM/%s.m2ts", clip->ClipName);
        CDX_LOGD("child[2](%s)", bdParser->fileUrl);
        fd = ExtOpen(bdParser->cdxStream, bdParser->fileUrl);
        if (fd == -1)
        {
            CDX_LOGE("ExtOpen failure");
            goto _exit;
        }
        sprintf(bdParser->fileUrl, "fd://%d?offset=0&length=0", fd);

        bdParser->cdxDataSource.uri = bdParser->fileUrl;
        flags = bdParser->flags | BD_TXET | DISABLE_VIDEO | DISABLE_AUDIO;
        ret = CdxParserPrepare(&bdParser->cdxDataSource,
            flags, &bdParser->statusLock, &bdParser->forceStop,
            &bdParser->child[2], &bdParser->childStream[2], NULL, NULL);
        close(fd);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserPrepare fail");
            goto _exit;
        }
    }
#if 1

    CdxMediaInfoT *pmediainfo = &bdParser->mediaInfo;
    struct CdxProgramS *cdxProgram = &pmediainfo->program[0];

    CdxMediaInfoT mediainfo;
    ret = CdxParserGetMediaInfo(bdParser->child[0], &mediainfo);
    if(ret < 0)
    {
        CDX_LOGE("CdxParserGetMediaInfo fail");
        goto _exit;
    }
    memcpy(pmediainfo, &mediainfo, sizeof(CdxMediaInfoT));

    struct CdxProgramS *cdxProgram1 = &mediainfo.program[0];
    memcpy(cdxProgram, cdxProgram1, sizeof(struct CdxProgramS));

    playItem = playlist->playItems + bdParser->curPlayItemIndex;
    clip = &playItem->clip;
    StreamInfo *stream;
    SubtitleStreamInfo *subtitle;
    AudioStreamInfo *audio;
    int j = 0, k = 0;
    for(i = 0; i < clip->ps->number_of_streams_in_ps; i++)
    {
        stream = clip->ps->streams + i;
        if(stream->mediaType == TYPE_SUBS_PG && j < SUBTITLE_STREAM_LIMIT)
        {
            subtitle = &cdxProgram->subtitle[j];
            memcpy(subtitle->strLang, stream->lang, 3);
            j++;
        }
#if 1
        else if(stream->mediaType == TYPE_AUDIO && k < AUDIO_STREAM_LIMIT)
        {
            if(stream->metadata)
            {
                audio = &cdxProgram->audio[k];
                memcpy(audio->strLang, stream->lang, 3);
                if(!audio->nChannelNum)
                {
                    switch(((AudioMetaData *)stream->metadata)->audioPresentationType)
                    {
                        case 1:
                            audio->nChannelNum = 1;
                            break;
                        case 3:
                            audio->nChannelNum = 2;
                            break;
                        case 6:
                            audio->nChannelNum = 6;
                            break;
                    }
                }
                if(!audio->nSampleRate)
                {
                    unsigned sampling_frequency =
                        ((AudioMetaData *)stream->metadata)->samplingFrequency;
                    if(sampling_frequency == 1)
                    {
                        audio->nSampleRate = 48000;
                    }
                    else if(sampling_frequency == 4)
                    {
                        audio->nSampleRate = 96000;
                    }
                    else if(sampling_frequency == 5)
                    {
                        audio->nSampleRate = 192000;
                    }
                }
            }
            k++;
        }
#endif
    }

    if(bdParser->child[1])
    {
        ret = CdxParserGetMediaInfo(bdParser->child[1], &mediainfo);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserGetMediaInfo fail");
            goto _exit;
        }
        cdxProgram1 = &mediainfo.program[0];
        CDX_CHECK(cdxProgram1->videoNum == 1);
    }
    if(bdParser->child[2])
    {
        ret = CdxParserGetMediaInfo(bdParser->child[2], &mediainfo);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserGetMediaInfo fail");
            goto _exit;
        }
        cdxProgram1 = &mediainfo.program[0];
        cdxProgram->subtitleNum += cdxProgram1->subtitleNum;
        CDX_CHECK(cdxProgram1->subtitleNum == 1);
    }
    if(cdxProgram->audioNum != playlist->audioCount)
    {
        CDX_LOGW("cdxProgram->audioNum(%d), playlist->audioCount(%d)",
            cdxProgram->audioNum, playlist->audioCount);
    }
    if(cdxProgram->videoNum != playlist->videoCount)
    {
        CDX_LOGW("cdxProgram->videoNum(%d), playlist->videoCount(%d)",
            cdxProgram->videoNum, playlist->videoCount);
    }
    if(cdxProgram->subtitleNum != playlist->subsCount)
    {
        CDX_LOGW("cdxProgram->subtitleNum(%d), playlist->subsCount(%d)",
            cdxProgram->subtitleNum, playlist->subsCount);
    }

    VideoStreamInfo *video = &cdxProgram->video[0];
    video->bIs3DStream = bdParser->hasMvcVideo;

    pmediainfo->bSeekable = 1;
    pmediainfo->programNum = 1;
    pmediainfo->programIndex = 0;
    cdxProgram->duration = playlist->durationUs/1000;
    CDX_LOGD("playlist->durationUs = %lld", playlist->durationUs);
#endif
    for(i = 0; i < 3; i++)
    {
        if(bdParser->child[i])
        {
            bdParser->streamPts[i] = 0;
        }
    }
    bdParser->streamPts[3] = 0;

    bdParser->mErrno = PSR_OK;
    pthread_mutex_lock(&bdParser->statusLock);
    bdParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&bdParser->statusLock);
    pthread_cond_signal(&bdParser->cond);
    CDX_LOGI("CdxBdParserInit success");

    return 0;

_exit:
    bdParser->mErrno = PSR_OPEN_FAIL;
    pthread_mutex_lock(&bdParser->statusLock);
    bdParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&bdParser->statusLock);
    pthread_cond_signal(&bdParser->cond);

    CDX_LOGI("CdxBdParserInit fail");
    return -1;
}
static struct CdxParserOpsS bdParserOps =
{
    .control         = BdParserControl,
    .prefetch         = BdParserPrefetch,
    .read             = BdParserRead,
    .getMediaInfo     = BdParserGetMediaInfo,
    .close             = BdParserClose,
    .seekTo            = BdParserSeekTo,
    .attribute        = BdParserAttribute,
    .getStatus        = BdParserGetStatus,
    .init            = BdParserInit
};

CdxParserT *BdParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    CdxBdParser *bdParser = CdxMalloc(sizeof(CdxBdParser));
    if(!bdParser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(bdParser, 0x00, sizeof(CdxBdParser));

    int ret = pthread_mutex_init(&bdParser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_cond_init(&bdParser->cond, NULL);
    CDX_FORCE_CHECK(ret == 0);

    bdParser->cdxStream = stream;

    bdParser->fileUrl = (char *)CdxMalloc(64);
    if(!bdParser->fileUrl)
    {
        CDX_LOGE("malloc fail!");
        goto open_error;
    }

    bdParser->fileBufSize = 240*1024;
    bdParser->fileBuf = (cdx_uint8 *)CdxMalloc(bdParser->fileBufSize);
    if (!bdParser->fileBuf)
    {
        CDX_LOGE("allocate memory fail");
        goto open_error;
    }
    memset(bdParser->streamPts, 0xff, sizeof(bdParser->streamPts));
    CdxListInit(&bdParser->playlists);
    bdParser->flags = flags
        & (DISABLE_VIDEO | DISABLE_AUDIO | DISABLE_SUBTITLE | MUTIL_AUDIO);
    bdParser->base.ops = &bdParserOps;
    bdParser->base.type = CDX_PARSER_BD;
    bdParser->mErrno = PSR_INVALID;
    bdParser->status = CDX_PSR_INITIALIZED;
    return &bdParser->base;

open_error:
    CdxStreamClose(stream);
    if(bdParser->fileUrl)
    {
        free(bdParser->fileUrl);
    }
    if(bdParser->fileBuf)
    {
        free(bdParser->fileBuf);
    }
    pthread_mutex_destroy(&bdParser->statusLock);
    pthread_cond_destroy(&bdParser->cond);
    free(bdParser);
    return NULL;
}

cdx_uint32 BdParserProbe(CdxStreamProbeDataT *probeData)
{
    if (probeData->len == 8 && memcmp(probeData->buf, "aw.bdmv.", 8) == 0)
    {
        CDX_LOGI("bdmv file");
        return 100;
    }

    return 0;
}

CdxParserCreatorT bdParserCtor =
{
    .create    = BdParserOpen,
    .probe     = BdParserProbe
};

