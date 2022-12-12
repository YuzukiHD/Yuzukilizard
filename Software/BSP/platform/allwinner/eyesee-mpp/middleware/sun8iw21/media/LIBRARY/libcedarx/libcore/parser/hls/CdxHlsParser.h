/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxHlsParser.h
* Description :
* History :
*   Author  : Kewei Han
*   Date    : 2014/10/08
*/

#ifndef HLS_PARSER_H
#define HLS_PARSER_H
#include <stdint.h>
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>
#include <semaphore.h>

#define MaxNumBandwidthItems (16)
enum CdxParserStatus {
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    CDX_PSR_READING,
};

enum RefreshState {
    INITIAL_MINIMUM_RELOAD_DELAY,
    FIRST_UNCHANGED_RELOAD_ATTEMPT,
    SECOND_UNCHANGED_RELOAD_ATTEMPT,
    THIRD_UNCHANGED_RELOAD_ATTEMPT,
    FOURTH_UNCHANGED_RELOAD_ATTEMPT,
    FIFTH_UNCHANGED_RELOAD_ATTEMPT,
    SIXTH_UNCHANGED_RELOAD_ATTEMPT
};

typedef struct SelectTrace SelectTrace;
struct SelectTrace {
    MediaType        mType;
    AString          *groupID;/*表明该Trace属于哪一个MediaGroup*/
    unsigned char    mNum;/*表明该Trace属于MediaGroup中的哪一个MediaItem*/
};

typedef struct MediaPlaylistItemInf MediaPlaylistItemInf;
struct MediaPlaylistItemInf {
    PlaylistItem  *item;
    cdx_int64     size;
    int           givenPcr;
};

struct PeriodicTask {
    sem_t        semStop;
    pthread_t    tid;
    int          enable;
    sem_t        semTimeshift;
};

typedef struct {
    CdxParserT base;
    enum CdxParserStatus status;
    int mErrno;
    cdx_uint32 flags;/*使能标志*/

    int forceStop;          /* for forceStop()*/
    pthread_mutex_t statusLock;
    pthread_cond_t cond;

    CdxStreamT *cdxStream;/*对应于生成该CdxHlsParser的m3u8文件*/
    CdxDataSourceT cdxDataSource;/*用途1:playlist的更新，用途2:child打开*/
    char *m3u8Url;/*用于playlist的更新*/
    char *m3u8Buf;/*用于m3u8文件的下载*/
    cdx_int64 m3u8BufSize;

    Playlist* mPlaylist;
    pthread_rwlock_t rwlockPlaylist;
    int isMasterParser;

    ParserCallback callback;
    void *pUserData;

    //void *father;
    //int (*cbFunc)(void *);
    int update;
    union {
        struct {
            int64_t mLastPlaylistFetchTimeUs;
            enum RefreshState refreshState;
            uint8_t mMD5[16];
            int curSeqNum;
            cdx_int64 baseTimeUs;
            MediaPlaylistItemInf curItemInf;
            PlaylistItem *cipherReference;

            //int64_t durationUs;
        }media;
        struct {
            int bandwidthSortIndex[MaxNumBandwidthItems];
            int bandwidthCount;
            int curBandwidthIndex;
            int preBandwidthIndex;
            SelectTrace selectTrace[3];/*记录当前所选的轨道，用于轨道的选择和切换*/
            int curSeqNum[3];/*选择轨道时，各轨道当前的SeqNum*/
        }master;
    } u;

    CdxParserT *child[3];/*为子parser，下一级的hls parser或者媒体文件parser，如TS parser*/
    CdxStreamT *childStream[3];
    cdx_uint64 streamPts[3];
    int prefetchType;

    CdxMediaInfoT mediaInfo;
    CdxPacketT cdxPkt;
//    struct StreamCacheStateS cacheState;
    AesStreamExtraDataT aesCipherInf;
    int ivIsAppointed;
    char *aesUri;

    struct StreamSeekPos streamSeekPos;
    ExtraDataContainerT extraDataContainer;

    int mPlayQuality;
    int curDownloadObject;
    char *curDownloadUri;
    int mIsTimeShift;

    //* YUNOS UUID
    char mYunOSUUID[64];

    int refreshFaild;
    struct PeriodicTask PeriodicTask;

    int timeShiftLastSeqNum;
    int64_t ptsShift;

    int64_t streamOpenTimeout;

    int64_t firstPts;
}CdxHlsParser;

#endif
