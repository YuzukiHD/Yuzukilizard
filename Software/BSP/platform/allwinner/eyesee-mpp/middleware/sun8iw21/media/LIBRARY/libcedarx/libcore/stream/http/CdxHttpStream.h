/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxHttpStream.h
 * Description : Part of http stream.
 * History :
 *
 */

#ifndef CDX_HTTP_STREAM_H
#define CDX_HTTP_STREAM_H

#include <CdxStream.h>
#include <CdxAtomic.h>
#include <CdxUrl.h>
#include <AwPool.h>
#include <zlib.h>
#include <assert.h>
#include <AwMessageQueue.h>

#define __SAVE_BITSTREAMS (0)

#define closesocket close

#define __CONFIG_ZLIB (1)

enum HttpStreamStateE
{
    HTTP_STREAM_IDLE       = 0x00,
    HTTP_STREAM_PREPARING,
    HTTP_STREAM_READING,
    HTTP_STREAM_SEEKING,
};

typedef struct HttpCacheManagerS HttpCacheManagerT;
typedef struct HttpCacheS HttpCacheT;
typedef struct HttpCacheNodeS HttpCacheNodeT;
typedef int (*HttpCallback)(void* pUserData, int eMessageId, void* param, int bLocal);

typedef struct HttpBandwidthS HttpBandwidthT;

struct HttpCacheNodeS
{
    unsigned char  *pData;
    int             nLength;
    HttpCacheNodeT *pNext;
};
//0-19, 512K*20=10M
struct HttpCacheS
{
    pthread_mutex_t  mutex;
    cdx_int64        nStartPos;
    cdx_int64        nEndPos;
    int              nCacheSize; //max cache size
    int              nNodeNum;

    int              nDataSize;  //valid size
    int              nPassedSize;//be requested data size
    HttpCacheNodeT  *pHead;
    HttpCacheNodeT  *pTail;
    HttpCacheNodeT  *pProbeHead;
    HttpCacheNodeT  *pOriHead;

    HttpCacheNodeT  *tmpNode;
    int              nNodeReadPos; //bytes
    int              nNodeValidSize;
    int              nCurNodeNum;

    cdx_int32        nWriteSize;
};

struct HttpHeaderField
{
    const char *key;
    const char *val;
};

typedef struct AW_HTTP_FIELD_TYPE
{
    char *fieldName;
    struct AW_HTTP_FIELD_TYPE *next;
}CdxHttpFieldT;

typedef struct AW_HTTP_HEADER
{
     char *protocol;
     char *method;
     char *uri;
     unsigned int statusCode;
     char *reasonPhrase;
     unsigned int httpMinorVersion;
     // Field variables
     CdxHttpFieldT *firstField;
     CdxHttpFieldT *lastField;
     unsigned int fieldNb;
     char *fieldSearch;
     CdxHttpFieldT *fieldSearchPos;
     // Body variables
     char *body;
     size_t bodySize;
     char *buffer;
     size_t bufferSize;
     unsigned int isParsed;
     //
     char *cookies;
     int posHdrSep;
 } CdxHttpHeaderT;

typedef struct DownloadCtx
{
    int id;
    int bufSize;
    int bWorking;
    pthread_t threadId;
    AwMessageQueue* mq;
    pthread_mutex_t lock;                      //for break blocking operation
    pthread_cond_t cond;
    const cdx_char *ua;
    //int forceStop;
    CdxUrlT *urlT;
    cdx_char *sourceUri;
    HttpCallback callback;
    void *pUserData;
    HttpCacheManagerT *cacheManager;
    int oneThread;

    //need clr
    //enum HttpStreamStateE state;
    cdx_int64 startPos;
    cdx_int64 endPos;
    CdxStreamT *pStream;

    int nHttpHeaderSize;
    CdxHttpHeaderFieldT *pHttpHeader;
    int isAuth;
    ExtraDataContainerT hfsContainer;
    cdx_int64 baseOffset;
    cdx_int64 totalSize;
    cdx_int64 bufPos;
    int chunkedFlag;
    int compressed;
    z_stream inflateStream;
    cdx_uint8 *inflateBuffer;
    int keepAlive;
    CdxHttpHeaderT *resHdr;
    cdx_int32 dlState;
    AwPoolT *pool;

    sem_t semQuit;
    HttpCacheT *httpCache;
    int  nQuitReply;

    cdx_int32 exit;
    cdx_int32 seekAble;

    cdx_int32 bLastPartFin;          //download last part finish
    cdx_int32 bDlfcc;                //mark chunked & compressed download finish.
    cdx_int64 downloadStart;
    cdx_int64 downloadEnd;
    cdx_int64 downloadTimeMs;
#if defined(CONF_YUNOS)
    char tcpIp[128];
#endif

    char *httpDataBufferChunked;                // store parsed data from chunked data;
    int  httpDataSizeChunked;                   // valid data size of httpDataBufferChunked
    int  httpDataBufferPosChunked;              // read pos of httpDataBufferChunked,
                                                // 0~httpDataSizeChunked-1
    int  restChunkSize;                         // when chunked, during CdxStreamOpen,
                                                // httpDataBuffer may not store the whole chunk,
                                                // this variable store rest size of the chunk.
    int dataCRLF;                               // data\r\n, next chunk need to get rid of \r\n.
    cdx_char tmpChunkedLen[10];                 // store temp chunked len while force stop.
                                                // eg: abcd\r\n, ab
    int tmpChunkedSize;                         // size of temp chunked len while force stop.
                                                // eg: ab is 2 bytes
    int lenCRLF;                                // len\r\n, next chunk need to get rid of \r\n.
    int chunkedLen;                             // while force stop, store current chunked len.
                                                // len\r\n
    HttpBandwidthT *bandWidthEst;
    int tempSize;
#if __SAVE_BITSTREAMS
    FILE *fp_download;
    cdx_int32 streamIndx;
    cdx_char location[256];
#endif
}DownloadCtxT;


typedef struct CdxHttpStreamImpl
{
    CdxStreamT base;
    HttpCacheManagerT *cacheManager;
    CdxStreamT *pStream;
    CdxUrlT *url;

    CdxStreamProbeDataT probeData;
    cdx_uint32 probeSize;
    cdx_int32 ioState;                          //for interface use
    int seekAble;                               //seekable flag.
    cdx_int32 forceStopFlag;                    //forceStop, not exit GetNetworkData
    cdx_int64 totalSize;                        //content-length
    cdx_int64 readPos;                          //stream data is read to this pos.
    CdxHttpHeaderT *resHdr;
    cdx_char *sourceUri;                        //the source uri

    const cdx_char *ua;                         // UA
    int nHttpHeaderSize;                        //header number to be added
    CdxHttpHeaderFieldT *pHttpHeader;           //http header
    ExtraDataContainerT hfsContainer;
    int chunkedFlag;                            //set when "Transfer-Encoding: chunked"

    pthread_mutex_t lock;                       //for break blocking operation
    pthread_cond_t cond;

    enum HttpStreamStateE state;
    AwPoolT *pool;
    cdx_int32 bStreamReadEos;                   //CdxStreamRead eos.
    cdx_int32 bDownloadFin;
    cdx_int32 bLastPartFin;

    ParserCallback callback;
    void *pUserData;

#if defined(CONF_YUNOS)
    char tcpIp[128];
#endif

    int isHls;
    cdx_int64 baseOffset; //* for id3 parser

#if __CONFIG_ZLIB
    int compressed;
    z_stream inflateStream;
    cdx_uint8 *inflateBuffer;
#endif

    int keepAlive;
    DownloadCtxT *dlCtx;
    int threadNum;

    DownloadCtxT dlct;
    pthread_cond_t condPrepared;
    int bPrepared;
    int retPrepared;
    cdx_bool isDTMB;
}CdxHttpStreamImplT;

typedef struct CdxHttpSendBuffer
{
    void *size;
    void *buf;
}CdxHttpSendBufferT;

extern CdxHttpHeaderT *CdxHttpNewHeader(void);
extern char *CdxHttpGetField(CdxHttpHeaderT *httpHdr, const char *fieldName);
extern char* CdxHttpBuildRequest(CdxHttpHeaderT *httpHdr);
extern CdxUrlT *CdxUrlRedirect(CdxUrlT **url, char *redir);
extern const char *GetUA(int n, CdxHttpHeaderFieldT *pHttpHeader);
extern CdxUrlT* CdxUrlNew(char* url);
extern void CdxHttpSetUri(CdxHttpHeaderT *httpHdr, const char *uri);
extern void CdxHttpSetField(CdxHttpHeaderT *httpHdr, const char *fieldName);
extern void CdxHttpFree(CdxHttpHeaderT *httpHdr);
extern int CdxHttpResponseAppend(CdxHttpHeaderT *httpHdr, char *response, int length);
extern int CdxHttpIsHeaderEntire(CdxHttpHeaderT *httpHdr);
extern int CdxHttpResponseParse(CdxHttpHeaderT *httpHdr);
extern int CdxHttpAuthenticate(CdxHttpHeaderT *httpHdr, CdxUrlT *url, int *authRetry);
extern cdx_int32 ReadChunkedSize(CdxStreamT *stream, cdx_char tmpLen[], cdx_int32* size);
extern cdx_int32 ReadChunkedData(CdxStreamT *stream, void *ptr, cdx_int32 size);
extern char *RmSpace(char *str);
extern int CdxHttpAddBasicAuthentication(CdxHttpHeaderT *httpHdr, const char *username,
    const char *password);

//////////////////////////////////////////////////////////////////////////////////////////////////
HttpCacheManagerT *CacheManagerCreate(int nCacheNum, int nCacheSize,
                                      cdx_int64 nTotal, int nNodeSize, cdx_int64 pos);
void CacheManagerDestroy(HttpCacheManagerT *p);
cdx_int64 CacheManagerGetReqDataPos(HttpCacheManagerT *p);
int CacheManagerRequestCache(HttpCacheManagerT *p, HttpCacheT **ppCache);
int CacheManagerRequestData(HttpCacheManagerT *p, int nRequireSize, void *buf);
int CacheManagerSeekTo(HttpCacheManagerT *p, cdx_int64 pos);
int CacheManagerCheckCacheUseup(HttpCacheManagerT *p);
int CacheManagerAddData(HttpCacheManagerT *p, HttpCacheT *c, HttpCacheNodeT *node);
int CacheManagerGetProbeData(HttpCacheManagerT *p, cdx_uint32 nRequireSize, void *buf);
void CacheManagerGetCacheRange(HttpCacheManagerT *p, HttpCacheT *c,
                                        cdx_int64 *start, cdx_int64 *end);
int CacheManagerCacheIsFull(HttpCacheManagerT *p, HttpCacheT *c);
void CacheManagerReset(HttpCacheManagerT *p, cdx_int64 nStartCachePos);
void CacheManagerCacheNodeNumInc(HttpCacheManagerT *p, HttpCacheT *c);

//////////////////////////////////////////////////////////////////////////////////////////////////
HttpBandwidthT *CreateBandwidthEst(cdx_int32 mMaxIndex);
void BandwidthEst(HttpBandwidthT *b, cdx_int32 numBytes, cdx_int64 delayUs);
int GetEstBandwidthKbps(HttpBandwidthT *b, cdx_int32 *kbps);
void DestroyBandwidthEst(HttpBandwidthT *b);

#endif
