/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxHttpUtils.c
 * Description : Part of http stream.
 * History :
 *
 */
#define  LOG_TAG "httpUtils"
#include <string.h>
#include <stdlib.h>
#include <CdxHttpStream.h>

#define PRESERVE_RATIO 0.1

typedef struct HttpCacheFifoS
{
    HttpCacheT *pCaches;
    int         nCacheNum; //max cache num in fifo.
    int         nCacheSize; //max cache size of each cache.
    int         nReadPos; //cache num
    int         nWritePos;
}HttpCacheFifoT;

struct HttpCacheManagerS
{
    pthread_mutex_t mutex;
    HttpCacheFifoT  cacheFifo;

    cdx_int64       nTotalSize;
    int             nNodeSize;
    int             nCacheSize;
    cdx_int64       nStartCachePos; //set after seek.

    cdx_int64       nReqDataPos; //CacheManagerRequestData to this pos.

    int             bUseup;
    int             probeCacheIdx;
    int             probeNodeIdx;
    HttpCacheNodeT *probeNode;
    int             probeNodePos;
};

typedef struct BandwidthEntry
{
    cdx_int64 mDelayUs;
    cdx_int32 mNumBytes;
}BandwidthEntryT;

struct HttpBandwidthS
{
    pthread_mutex_t mutex;
    cdx_int64 mTotalMeasureTimeUs;             //for bandwidth measurement.
    cdx_int32 mTotalMeasureBytes;
    cdx_int32 mBandwidthNum;
    cdx_int32 mMaxBandwidthNum;
    BandwidthEntryT *bandWidthHistory;
    cdx_int32 mBandwidthIndex;
};

//set User-Agent field.
const char *GetUA(int n, CdxHttpHeaderFieldT *pHttpHeader)
{
    int i;
    /*const char *defaultUA = "AppleCoreMedia/1.0.0.9A405
     (iPad; U; CPU OS 5_0_1 like Mac OS X; zh_cn)";*/
    //const char *defaultUA = "stagefright/1.2 (Linux;Android 4.2.2)";
    const char *defaultUA = "Allwinner/CedarX 2.8";

    if(pHttpHeader)
    {
        for(i = 0; i < n; i++)
        {
            if(strcasecmp("User-Agent", pHttpHeader[i].key) == 0)
            {
                //return strdup(pHttpHeader[i].val);
                return pHttpHeader[i].val;
            }
        }
    }
    return defaultUA;
}
//get "len" in "len\r\n". tmpLen: store "len", size: num
//return -2: force stop while read len or \r
//       -3: force stop while read \n
cdx_int32 ReadChunkedSize(CdxStreamT *stream, cdx_char tmpLen[], cdx_int32 *size)
{
    cdx_int32 ret;
    cdx_char len[11] = {0};
    cdx_int32 pos = 0;

    while(1)
    {
        cdx_char byte;

        ret = CdxStreamRead(stream, &byte, 1);
        if(ret <= 0)
        {
            if(ret == -2)
            {
                CDX_LOGW("force stop ReadChunkedSize while get len.");
                strcpy(tmpLen, len);
                *size = pos;
                return -2;
            }
            CDX_LOGE("Read failed.");
            return -1;
        }

        CDX_CHECK(ret == 1);
        if((byte >= '0' && byte <='9') || (byte >= 'a' && byte <= 'f') ||
            (byte >= 'A' && byte <= 'F'))
        {
            len[pos++] = byte;
            if(pos > 10)
            {
                CDX_LOGE("chunked len is too big...");
                return -1;
            }
            continue;
        }
        else if(byte == '\r')
        {
            ret = CdxStreamRead(stream, &byte, 1);
            if(ret == -2)
            {
                CDX_LOGW("force stop ReadChunkedSize while get LF.");
                strcpy(tmpLen, len);
                *size = pos;
                return -3;
            }
            CDX_CHECK(ret == 1);
            CDX_CHECK(byte == '\n');
            break;
        }
        else
        {
            CDX_LOGE("Something error happen, %d lencrlf.", byte);
            return -1;
        }
    }

    return strtol(len, NULL, 16);
}
//return > 0 : read data bytes
//      == -1: read failed.
//      == -2: force stop while read data, no data has read.
//      == -3: force stop while read \r\n in data\r\n, all data has been read.
//      == -4: force stop while read \n in data\r\n, all data has been read.
cdx_int32 ReadChunkedData(CdxStreamT *stream, void *ptr, cdx_int32 size)
{
    cdx_int32 ret;
    cdx_int32 pos = 0;
    cdx_char dummy[2];

    while(1)
    {
        ret = CdxStreamRead(stream, (char *)ptr + pos, size - pos);
        if(ret <= 0)
        {
            if(ret == -2)
            {
                CDX_LOGW("force stop ReadChunkedDate.");
                return pos>0 ? pos : -2;
            }
            CDX_LOGE("Read failed.");
            return ret;
        }
        pos += ret;
        if(pos == size)
        {
            ret = CdxStreamRead(stream, dummy, 2);
            if(ret <= 0)
            {
                if(ret == -2)
                {
                    CDX_LOGW("force stop ReadChunkedData.");
                    return -3; // force stop while read \r\n in data\r\n.
                }
                CDX_LOGE("read failed.");
                return -1;
            }
            else if(ret == 1)
            {
                CDX_LOGW("force stop ReadChunkedData.");
                return -4; // force stop while read \n in data\r\n.
            }

            if(memcmp(dummy, "\r\n", 2) != 0)
            {
                CDX_LOGE("Not end with crlf, check the content.");
                return -1;
            }
            break;
        }
    }
    return pos;
}
//remove spaces at head and tail of string
char *RmSpace(char *str)
{
    char *it = NULL;

    if(!str || *str=='\0')
        return str;

    while(*str == ' ')
    {
        ++str;
    }
    it = str;

    while(*str)//point to the str end
    {
        str++;
    }

    while(*(--str)==' ')//remove the space of str end
    {
        ;
    }

    *(++str)='\0';

    return it;
}

/////////////////////////////////////////////////////////////////////////
/////////////////////////Cache Manager for http ////////////////////////////
/////////////////////////////////////////////////////////////////////////
HttpCacheManagerT *CacheManagerCreate(int nCacheNum, int nCacheSize,
                                            cdx_int64 nTotal, int nNodeSize, cdx_int64 pos)
{
    HttpCacheManagerT *p;

    if(nCacheNum * PRESERVE_RATIO <= 0)
    {
        logw("preserve ratio too small.");
        return NULL;
    }

    p = (HttpCacheManagerT *)calloc(1, sizeof(*p));
    if(NULL == p)
    {
        loge("calloc fail, size=%d.", (int)sizeof(*p));
        return NULL;
    }

    p->cacheFifo.pCaches = (HttpCacheT *)calloc(nCacheNum, sizeof(HttpCacheT));
    if(NULL == p->cacheFifo.pCaches)
    {
        loge("calloc fail, nCacheNum=%d", nCacheNum);
        free(p);
        return NULL;
    }
    pthread_mutex_init(&p->mutex, NULL);
    p->cacheFifo.nCacheSize = nCacheSize;
    p->cacheFifo.nCacheNum = nCacheNum;
    p->cacheFifo.nReadPos = 0;
    p->cacheFifo.nWritePos = 0;
    p->nCacheSize = nCacheSize;
    p->nTotalSize = nTotal;
    p->nNodeSize = nNodeSize;
    p->nStartCachePos = pos;

    return p;
}

void CacheManagerDestroy(HttpCacheManagerT *p)
{
    if(NULL == p)
    {
        loge("check param.");
        return;
    }
    pthread_mutex_destroy(&p->mutex);

    HttpCacheT *pCache;
    int nCacheNum = p->cacheFifo.nCacheNum;
    HttpCacheNodeT *node;
    int i = 0;

    for(i = 0; i < nCacheNum; i++)
    {
        pCache = &p->cacheFifo.pCaches[i];
        node = pCache->pOriHead;
        while(node != NULL)
        {
            pCache->pOriHead = node->pNext;
            free(node->pData);
            node->pData = NULL;
            free(node);
            node = pCache->pOriHead;
        }
    }
    free(p->cacheFifo.pCaches);
    free(p);
    return;
}

void CacheManagerReset(HttpCacheManagerT *p, cdx_int64 nStartCachePos)
{
    if(NULL == p || nStartCachePos < 0)
    {
        loge("check param.");
        return;
    }

    pthread_mutex_lock(&p->mutex);
    HttpCacheT *pCache;
    int nCacheNum = p->cacheFifo.nCacheNum;
    HttpCacheNodeT *node;
    int i = 0;
    //HttpCacheNodeT *tmpNode;

    for(i = 0; i < nCacheNum; i++)
    {
        pCache = &p->cacheFifo.pCaches[i];
        node = pCache->pOriHead;
        while(node != NULL)
        {
            pCache->pOriHead = node->pNext;
            free(node->pData);
            node->pData = NULL;
            free(node);
            node = pCache->pOriHead;
        }
        pCache->nStartPos = pCache->nEndPos = 0;
        pCache->nCacheSize = 0;
        pCache->nNodeNum = 0;
        pCache->nDataSize = 0;
        pCache->nPassedSize = 0;
        pCache->pHead = pCache->pTail = pCache->pOriHead = NULL;
        pCache->tmpNode = NULL;
        pCache->nNodeReadPos = 0;
        pCache->nNodeValidSize = 0;
        pCache->nCurNodeNum = 0;
        pCache->nWriteSize = 0;
    }

    p->nStartCachePos = nStartCachePos;
    p->nReqDataPos = nStartCachePos;
    p->bUseup = 0;
    p->cacheFifo.nReadPos = 0;
    p->cacheFifo.nWritePos = 0;
    pthread_mutex_unlock(&p->mutex);
    return;
}

static void CacheFlushNode(HttpCacheT *c)
{
    HttpCacheNodeT *node;
    node = c->pHead;
    if(node == NULL)
    {
        loge("something wrong, call CacheGetNode first.");
        return;
    }

    c->pHead = node->pNext;
    //c->nDataSize -= node->nLength;//to del
    //c->nPassedSize += node->nLength;//todo del
    return;
}

static inline int validCacheNum(HttpCacheFifoT *p)
{
    return (p->nWritePos - p->nReadPos + p->nCacheNum + 1) % p->nCacheNum;
}

static inline int cacheFifoTooFull(HttpCacheFifoT *p)
{
    int invalidCacheNum = p->nCacheNum - validCacheNum(p);
    int preserveCacheNum = (p->nCacheNum * PRESERVE_RATIO <= 1) ?
        1 : (p->nCacheNum * PRESERVE_RATIO);
    logv("invalidCacheNum=%d, preserveCacheNum=%d, p->nWritePos=%d, p->nReadPos=%d, %d",
        invalidCacheNum, preserveCacheNum, p->nWritePos, p->nReadPos,
        (int)(p->nCacheNum * PRESERVE_RATIO));
    if (invalidCacheNum <= preserveCacheNum)
        return 1;

    return 0;
}

int CacheManagerCheckCacheUseup(HttpCacheManagerT *p)
{
    int ret;
    pthread_mutex_lock(&p->mutex);
    ret = p->bUseup;
    pthread_mutex_unlock(&p->mutex);
    return ret;
}

cdx_int64 CacheManagerGetReqDataPos(HttpCacheManagerT *p)
{
    cdx_int64 ret;
    pthread_mutex_lock(&p->mutex);
    ret = p->nReqDataPos;
    pthread_mutex_unlock(&p->mutex);
    return ret;
}

//request cache from cacheManager in each thread.
//return -1: means no empty buf, should request later;
//return -2: something error happen.
int CacheManagerRequestCache(HttpCacheManagerT *p, HttpCacheT **ppCache)
{
    int nWritePos;
    HttpCacheT *pCache;
    //operate cache,
    if(NULL == p || NULL == ppCache)
    {
        loge("check param.");
        return -2;
    }

    *ppCache = NULL;

    pthread_mutex_lock(&p->mutex);

    if(p->bUseup == 1)
    {
        logv("cache used up.");
        pthread_mutex_unlock(&p->mutex);
        return -2;
    }

    if(cacheFifoTooFull(&p->cacheFifo))
    {
        logv("so many data, wait.");
        pthread_mutex_unlock(&p->mutex);
        return -1;
    }

    //free nodes
    nWritePos = p->cacheFifo.nWritePos;
    pCache = &p->cacheFifo.pCaches[nWritePos];
    HttpCacheNodeT *node = pCache->pOriHead;
    logv("pCaches=%p, nWritePos=%d, node=%p", pCache, nWritePos, node);
    while(node != NULL)
    {
        logv("will free node[%p]...", node);
        pCache->pOriHead = node->pNext;
        free(node->pData);
        node->pData = NULL;
        free(node);
        node = pCache->pOriHead;
    }

    pCache = &p->cacheFifo.pCaches[nWritePos];
    memset(pCache, 0x00, sizeof(*pCache));
    pCache->nStartPos = p->nStartCachePos;
    if(p->nTotalSize > 0 && pCache->nStartPos >= p->nTotalSize)
    {
        loge("check, pCache->nStartPos(%lld), p->nTotalSize(%lld).",
            pCache->nStartPos, p->nTotalSize);
        return -2;
    }

    pCache->nEndPos = pCache->nStartPos + p->nCacheSize - 1;
    if(p->nTotalSize > 0 && pCache->nEndPos >= p->nTotalSize)
    {
        pCache->nEndPos = p->nTotalSize - 1;
    }

    if(p->nTotalSize > 0 && pCache->nEndPos == p->nTotalSize - 1)
    {
        p->bUseup = 1;
        logv("cache useup 1.");
    }

    pCache->nNodeNum = (pCache->nEndPos - pCache->nStartPos) / p->nNodeSize + 1;
    pCache->nCacheSize = p->nCacheSize;

    *ppCache = pCache;
    nWritePos++;
    nWritePos %= p->cacheFifo.nCacheNum;
    p->cacheFifo.nWritePos = nWritePos;
    p->nStartCachePos = pCache->nEndPos + 1;
    if(p->nTotalSize > 0 && p->nStartCachePos == p->nTotalSize)
    {
        p->bUseup = 1;
        logv("cache useup 1.");
    }

    logv("request cache pCache(%p), p->cacheFifo.nWritePos=%d,"
         " pCache->nCacheSize=%d, pCache->nNodeNum=%d, nNodeSize=%d, s-e(%lld, %lld)",
        pCache, p->cacheFifo.nWritePos-1,
        pCache->nCacheSize, pCache->nNodeNum, p->nNodeSize, pCache->nStartPos, pCache->nEndPos);

    pthread_mutex_unlock(&p->mutex);

    return 0;
}

void CacheManagerGetCacheRange(HttpCacheManagerT *p, HttpCacheT *c,
                                        cdx_int64 *start, cdx_int64 *end)
{
    pthread_mutex_lock(&p->mutex);
    *start = c->nStartPos;
    *end = c->nEndPos;
    pthread_mutex_unlock(&p->mutex);

    return;
}

int CacheManagerSeekTo(HttpCacheManagerT *p, cdx_int64 pos)
{
    HttpCacheT *c;
    int i;

    pthread_mutex_lock(&p->mutex);
    for(i = 0; i < p->cacheFifo.nCacheNum; i++)
    {
        c = &p->cacheFifo.pCaches[i];
        if(pos >= c->nStartPos && pos < c->nStartPos + c->nDataSize + c->nPassedSize)
        {
            logv("test cache=%p, pos=%lld, nStartPos=%lld, nDataSize=%d, nPassedSize=%d",
                c, pos, c->nStartPos, c->nDataSize, c->nPassedSize);

            HttpCacheNodeT *pNode = c->pOriHead;
            cdx_int64 offset = c->nStartPos;
            int j = 0;
            for(; pNode != NULL; pNode = pNode->pNext)
            {
                offset += pNode->nLength;
                if(pos < offset) //pos is in pNode
                {
                    break;
                }
                j++;
            }

            if(pNode != NULL)
            {
                //update info.
                c->pHead = pNode;
                c->tmpNode = pNode;
                c->nNodeValidSize = offset - pos;
                c->nNodeReadPos = pNode->nLength - (offset - pos);//(pos - c->nStartPos)%pNode->nLength;//pNode->nLength - (offset - pos + 1);
                c->nCurNodeNum = j;
                p->nReqDataPos = pos;
                p->cacheFifo.nReadPos = i;

                int tmp = c->nPassedSize;
                c->nPassedSize = pos - c->nStartPos;
                c->nDataSize = tmp + c->nDataSize - c->nPassedSize ;

                logv("seek in cache=%p, pos=%lld, nStartPos=%lld, nDataSize=%d, nPassedSize=%d",
                    c, pos, c->nStartPos, c->nDataSize, c->nPassedSize);
                logv("cache(%p), c.pHead(%p), c.nNodeValidSize(%d), "
                     "c.nNodeReadPos(%d), p->nReqDataPos(%lld), c.tmpNode(%p)"
                     "c->nDataSize(%d), c->nPassedSize(%d)",
                    &c, c->pHead, c->nNodeValidSize,
                    c->nNodeReadPos, p->nReqDataPos, c->tmpNode,
                    c->nDataSize, c->nPassedSize);

                //need to update other cache behind current cache, because c->pHead is NULL after read.
                //in order to read afterwards, should reset c->pHead.
                for(i = 0; i < p->cacheFifo.nCacheNum; i++)
                {
                    c = &p->cacheFifo.pCaches[i];
                    if(c->pOriHead != NULL && c->pHead == NULL)
                    {
                        c->pHead = c->pOriHead;
                        if(i != p->cacheFifo.nReadPos)
                        {
                            if(c->nPassedSize != 0)
                            {
                                c->nDataSize += c->nPassedSize;
                                c->nPassedSize = 0;
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&p->mutex);
                return 0;
            }
            else
                break;
        }
    }
    pthread_mutex_unlock(&p->mutex);

    return -1;
}

//--------cache operate------------
int CacheManagerCacheIsFull(HttpCacheManagerT *p, HttpCacheT *c)
{
    int i = 0;
    pthread_mutex_lock(&p->mutex);
    i = (c->nWriteSize == c->nEndPos - c->nStartPos + 1) ? 1 : 0;
    pthread_mutex_unlock(&p->mutex);

    return i;
}

void CacheManagerCacheNodeNumInc(HttpCacheManagerT *p, HttpCacheT *c)
{
    pthread_mutex_lock(&p->mutex);
    c->nNodeNum++;
    pthread_mutex_unlock(&p->mutex);

    return;
}

//add data to cache, used by each download thread.
int CacheManagerAddData(HttpCacheManagerT *p, HttpCacheT *c, HttpCacheNodeT *node)
{
    HttpCacheNodeT *newNode;

    pthread_mutex_lock(&p->mutex);

    if(node->nLength + c->nDataSize + c->nPassedSize > c->nCacheSize)
    {
        logw("cache size(%d %d %d)(%d) > max cache size(%d), c=%p, r,w(%d, %d)", node->nLength, c->nDataSize,
            c->nPassedSize, node->nLength+c->nDataSize+c->nPassedSize, c->nCacheSize, c,
            p->cacheFifo.nReadPos, p->cacheFifo.nWritePos);
        pthread_mutex_unlock(&p->mutex);
        return -1;
    }

    newNode = (HttpCacheNodeT *)calloc(1, sizeof(*newNode));
    if(NULL == newNode)
    {
        loge("calloc fail.");
        pthread_mutex_unlock(&p->mutex);
        return -1;
    }
    logv("add data, node=%p, size=%d, cache=%p, c->nDataSize=%d, c->nPassedSize=%d,[%lld, %lld],"
        "p->cacheFifo.nReadPos=%d, p->cacheFifo.nWritePos=%d, c=%p",
        newNode, node->nLength, p, c->nDataSize, c->nPassedSize, c->nStartPos, c->nEndPos,
        p->cacheFifo.nReadPos, p->cacheFifo.nWritePos, c);

    *newNode = *node;
    newNode->pNext = NULL;

    if(c->pTail != NULL)
    {
        c->pTail->pNext = newNode;
        c->pTail = c->pTail->pNext;
        if(c->pHead == NULL)
            c->pHead = newNode;
        if(c->pProbeHead == NULL)
        {
            c->pProbeHead = newNode;
        }
    }
    else
    {
        c->pOriHead = c->pHead = c->pTail = c->pProbeHead = newNode;
    }

    c->nDataSize += newNode->nLength;
    c->nWriteSize += newNode->nLength;
    pthread_mutex_unlock(&p->mutex);

    return 0;
}

//get one node from httpcache, after use, should flush.
static HttpCacheNodeT *CacheGetNode(HttpCacheT *c)
{
    HttpCacheNodeT *node;
    node = c->pHead;
    return node;
}

//used by consumer, read data from cache. return actual size.
int CacheManagerRequestData(HttpCacheManagerT *p, int nRequireSize, void *buf)
{
    HttpCacheT *c;
    int readSize = 0;
    int needLen = 0;

    if(NULL == p || NULL == buf)
    {
        loge("check param.");
        return -1;
    }

    if(nRequireSize == 0)
        return 0;

    pthread_mutex_lock(&p->mutex);
    if(validCacheNum(&p->cacheFifo) == 1) //means this cache is writing && reading.
    {
        logv("writing && reading the same HttpCacheT.");
    }

    c = &p->cacheFifo.pCaches[p->cacheFifo.nReadPos];
    logv("nRequireSize=%d, c->nNodeValidSize=%d, c->cacheFifo.nReadPos,nWritePos=%d,%d, "
        "pCaches=%p, c->tmpNode(%p), curNodeNum=%d, nDatasize=%d, head=%p",
        nRequireSize, c->nNodeValidSize, p->cacheFifo.nReadPos, p->cacheFifo.nWritePos,
        c, c->tmpNode, c->nCurNodeNum, c->nDataSize, c->pHead);

    if(c->nNodeValidSize > 0 && c->tmpNode != NULL)
    {
        logv("needLen=%d, c->nNodeValidSize=%d, nodelen=%d, curNodeNum=%d", needLen,
            c->nNodeValidSize, c->tmpNode->nLength, c->nCurNodeNum);

        if(nRequireSize < c->nNodeValidSize)
        {
            memcpy(buf, c->tmpNode->pData + c->nNodeReadPos, nRequireSize);
            c->nNodeValidSize -= nRequireSize;
            c->nNodeReadPos += nRequireSize;
            p->nReqDataPos += nRequireSize;
            c->nDataSize -= nRequireSize;
            c->nPassedSize += nRequireSize;
            pthread_mutex_unlock(&p->mutex);
            return nRequireSize;
        }
        else
        {
            memcpy(buf, c->tmpNode->pData + c->nNodeReadPos, c->nNodeValidSize);
            c->nDataSize -= c->nNodeValidSize;
            c->nPassedSize += c->nNodeValidSize;
            readSize = c->nNodeValidSize;
            c->nNodeValidSize = 0;
            c->nNodeReadPos = 0;
            c->tmpNode = NULL;
            CacheFlushNode(c);
            c->nCurNodeNum++;
            if(c->nCurNodeNum == c->nNodeNum)
            {
                p->cacheFifo.nReadPos = (p->cacheFifo.nReadPos + 1) % p->cacheFifo.nCacheNum;
                c = &p->cacheFifo.pCaches[p->cacheFifo.nReadPos];
                c->nCurNodeNum = 0;
                logv("c=%p, c->pHead=%p, size=%d, passedsize=%d, s-e(%lld, %lld), nodeNum=%d",
                    c, c->pHead, c->nDataSize, c->nPassedSize, c->nStartPos,
                    c->nEndPos, c->nNodeNum);
            }
        }
    }

    needLen = nRequireSize - readSize;

    //get node
    while(1)
    {
        if(needLen <= 0)
            break;

        c->tmpNode = CacheGetNode(c);
        if(NULL == c->tmpNode) //means no cache.
        {
            break;
        }

        c->nNodeValidSize = c->tmpNode->nLength;
        c->nNodeReadPos = 0;
        logv("needLen=%d, c->nNodeValidSize=%d, nodelen=%d, curNodeNum=%d", needLen,
            c->nNodeValidSize, c->tmpNode->nLength, c->nCurNodeNum);

        if(c->tmpNode->nLength > needLen)
        {
            memcpy((unsigned char*)buf + readSize, c->tmpNode->pData, needLen);
            c->nNodeValidSize -= needLen;
            c->nNodeReadPos += needLen;
            c->nDataSize -= needLen;
            c->nPassedSize += needLen;
            readSize = nRequireSize;
            break;
        }
        else
        {
            memcpy((unsigned char*)buf + readSize, c->tmpNode->pData, c->tmpNode->nLength);
            c->nDataSize -= c->tmpNode->nLength;
            c->nPassedSize += c->tmpNode->nLength;
            c->nNodeValidSize = 0;
            c->nNodeReadPos = 0;
            readSize += c->tmpNode->nLength;
            needLen -= c->tmpNode->nLength;
            c->tmpNode = NULL;
            CacheFlushNode(c);
            c->nCurNodeNum++;
            if(c->nCurNodeNum == c->nNodeNum)
            {
                p->cacheFifo.nReadPos = (p->cacheFifo.nReadPos + 1) % p->cacheFifo.nCacheNum;
                c = &p->cacheFifo.pCaches[p->cacheFifo.nReadPos];
                c->nCurNodeNum = 0;
                logv("c=%p, c->pHead=%p, size=%d, passedsize=%d, s-e(%lld, %lld), nodeNum=%d",
                    c, c->pHead, c->nDataSize, c->nPassedSize, c->nStartPos,
                    c->nEndPos, c->nNodeNum);
            }
        }
    }

    p->nReqDataPos += readSize;
    pthread_mutex_unlock(&p->mutex);

    return readSize;
}

int CacheManagerGetProbeData(HttpCacheManagerT *p, cdx_uint32 nRequireSize, void *buf)
{
    HttpCacheT *c;
    HttpCacheNodeT *node;
    int cacheIdx = 0;
    int readSize = 0;
    int needSize = 0;

    if(NULL == p || NULL == buf)
    {
        loge("check param.");
        return -1;
    }

    if(nRequireSize == 0)
        return 0;

    pthread_mutex_lock(&p->mutex);
    c = &p->cacheFifo.pCaches[p->probeCacheIdx];
    needSize = nRequireSize;

    if(p->probeNode != NULL)
    {
        logv("node=%p, readSize=%d, node->nLength=%d, needSize=%d, p->probeNodeIdx=%d, cacheidx=%d",
            p->probeNode, readSize, p->probeNode->nLength, needSize,
            p->probeNodeIdx, p->probeCacheIdx);
        node = p->probeNode;
        if(node->nLength - p->probeNodePos >= needSize)
        {
            memcpy(buf, node->pData+p->probeNodePos, needSize);
            readSize += needSize;
            p->probeNodePos += needSize;
            pthread_mutex_unlock(&p->mutex);
            return readSize;
        }
        else
        {
            memcpy(buf, node->pData+p->probeNodePos, node->nLength - p->probeNodePos);
            readSize += (node->nLength - p->probeNodePos);
            needSize -= (node->nLength - p->probeNodePos);

            c->pProbeHead = node->pNext;
            p->probeNode = NULL;
            p->probeNodePos = 0;
            p->probeNodeIdx++;
            if(p->probeNodeIdx == c->nNodeNum)
            {
                cacheIdx = (p->probeCacheIdx + 1) % p->cacheFifo.nCacheNum;
                p->probeCacheIdx = cacheIdx;
                p->probeNodeIdx = 0;
                c = &p->cacheFifo.pCaches[cacheIdx];
            }
        }
    }

    logv("p->probeCacheIdx=%d, p->probeNodeIdx=%d, readSize=%d, needSize=%d, c->pProbeHead=%p",
        p->probeCacheIdx, p->probeNodeIdx, readSize, needSize, c->pProbeHead);
    while(1)
    {
        if(needSize <= 0)
            break;


        p->probeNode = c->pProbeHead;
        logv("readSize=%d, needSize=%d, p->probeNodeIdx=%d,cacheidx=%d, p->probeNode=%p",
            readSize, needSize, p->probeNodeIdx, p->probeCacheIdx, p->probeNode);
        if(NULL == p->probeNode)
            break;

        node = p->probeNode;
        if(node->nLength >= needSize)
        {
            memcpy((unsigned char*)buf + readSize, node->pData, needSize);
            readSize += needSize;
            p->probeNodePos += needSize;
            logv("readSize=%d, node->nLength=%d, needSize=%d", readSize, node->nLength, needSize);
            break;
        }
        else
        {
            memcpy((unsigned char*)buf + readSize, node->pData, node->nLength);
            readSize += node->nLength;
            needSize -= node->nLength;
            logv("readSize=%d, node->nLength=%d, needSize=%d", readSize, node->nLength, needSize);
            c->pProbeHead = node->pNext;
            p->probeNode = NULL;
            p->probeNodePos = 0;
            p->probeNodeIdx++;
            if(p->probeNodeIdx == c->nNodeNum)
            {
                cacheIdx = (p->probeCacheIdx + 1) % p->cacheFifo.nCacheNum;
                p->probeCacheIdx = cacheIdx;
                p->probeNodeIdx = 0;
                c = &p->cacheFifo.pCaches[cacheIdx];
            }
        }
    }
    pthread_mutex_unlock(&p->mutex);

    return readSize;
}

/////////////////////////////////////////////////////////////////////////
/////////////////////////Bandwidth estimated for http ///////////////////////
/////////////////////////////////////////////////////////////////////////
HttpBandwidthT *CreateBandwidthEst(cdx_int32 mMaxIndex)
{
    HttpBandwidthT *b;
    b = (HttpBandwidthT *)calloc(1, sizeof(*b));
    if(b == NULL)
    {
        loge("calloc failed, size=%d", (int)sizeof(*b));
        return NULL;
    }
    b->bandWidthHistory = (BandwidthEntryT *)calloc(mMaxIndex, sizeof(BandwidthEntryT));
    if(b->bandWidthHistory == NULL)
    {
        loge("calloc failed, mMaxIndex=%d", mMaxIndex);
        free(b);
        return NULL;
    }
    b->mMaxBandwidthNum = mMaxIndex;
    pthread_mutex_init(&b->mutex, NULL);
    return b;
}

void BandwidthEst(HttpBandwidthT *b, cdx_int32 numBytes, cdx_int64 delayUs)
{
    if(b == NULL)
        return;

    pthread_mutex_lock(&b->mutex);

    b->bandWidthHistory[b->mBandwidthIndex].mDelayUs = delayUs;
    b->bandWidthHistory[b->mBandwidthIndex].mNumBytes = numBytes;
    b->mTotalMeasureBytes += numBytes;
    b->mTotalMeasureTimeUs += delayUs;

    logv("mTotalMeasureBytes(%d), mTotalMeasureTimeUs(%lld), mBandwidthNum(%d)",
        b->mTotalMeasureBytes, b->mTotalMeasureTimeUs, b->mBandwidthNum);

    b->mBandwidthNum++;
    if(b->mBandwidthNum > b->mMaxBandwidthNum)
    {
        BandwidthEntryT begin;
        begin = b->bandWidthHistory[b->mBandwidthIndex];
        b->mTotalMeasureBytes -= begin.mNumBytes;
        b->mTotalMeasureTimeUs -= begin.mDelayUs;
        --b->mBandwidthNum;
    }

    b->mBandwidthIndex++;
    if(b->mBandwidthIndex >= b->mMaxBandwidthNum)
    {
        b->mBandwidthIndex = 0;
    }
    pthread_mutex_unlock(&b->mutex);

    return;
}

int GetEstBandwidthKbps(HttpBandwidthT *b, cdx_int32 *kbps)
{
    if(b == NULL)
        return -1;

    pthread_mutex_lock(&b->mutex);
    if(b->mBandwidthNum < 2)
    {
        pthread_mutex_unlock(&b->mutex);
        return -1;
    }

    *kbps = ((double)b->mTotalMeasureBytes * 8E3 / b->mTotalMeasureTimeUs);
    pthread_mutex_unlock(&b->mutex);

    return 0;
}

void DestroyBandwidthEst(HttpBandwidthT *b)
{
    if(b == NULL)
        return;

    pthread_mutex_destroy(&b->mutex);

    free(b->bandWidthHistory);
    free(b);
    return;
}
/////////////////////////////////////////////////////////////////////////
