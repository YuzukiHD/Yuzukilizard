#ifndef __RECRENDER_CACHE_H__
#define __RECRENDER_CACHE_H__

//ref platform headers
#include "plat_type.h"

//media internal common headers.
#include "mm_component.h"

typedef struct RsPacketCacheManager {
    struct list_head    mDataBufList;   //DynamicBuffer
    char*             mpWritePos;
    char*             mpReadPos;
    int             mUsedBufSize;
    int             mBufSize;
    int             mPacketCount;
    int64_t             mCacheTime; //unit:ms
    int64_t             mFirstPts;  //unit:us, the first packet's pts, video, audio and others are all possible.
    volatile BOOL   mIsFull;    //if cache is full, when it is set to TRUE, it is TRUE forever.
    BOOL            mWaitReleaseUsingPacketFlag;    //indicate begin t wait to release packet.
    int             mWaitReleaseUsingPacketId;  //which packet is waiting to release.
    pthread_cond_t      mCondReleaseUsingPacket;
    pthread_mutex_t     mPacketListLock;
    //struct list_head    mRSPacketBufList;   //DynamicBuffer, sizeof(RecSinkPacket)*PACKET_NUM_IN_RSPACKETBUF
    struct list_head    mIdlePacketList; //RecSinkPacket
    struct list_head    mReadyPacketList;
    struct list_head    mUsingPacketList;
    int             mPacketIdCounter;

    int mRefStreamId;   //indicate stream which is used to calculate time duration.

    ERRORTYPE (*SetRefStream)(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int nRefStreamId); //refStream is used to calculate time duration.
    ERRORTYPE (*PushPacket)(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN RecSinkPacket *pRSPacket);
    ERRORTYPE (*GetPacket)(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT RecSinkPacket **ppRSPacket);
    ERRORTYPE (*RefPacket)(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int nId);
    ERRORTYPE (*ReleasePacket)(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int nId);
    BOOL (*IsReady)(
        PARAM_IN COMP_HANDLETYPE hComponent);
    int (*GetReadyPacketCount)(
        PARAM_IN COMP_HANDLETYPE hComponent);
    ERRORTYPE (*ControlCacheLevel)(
        PARAM_IN COMP_HANDLETYPE hComponent);
    ERRORTYPE (*QueryCacheState)(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT CacheState *pState);
    ERRORTYPE (*LockPacketList)(
        PARAM_IN COMP_HANDLETYPE hComponent);
    ERRORTYPE (*UnlockPacketList)(
        PARAM_IN COMP_HANDLETYPE hComponent);
    ERRORTYPE (*GetUsingPacketList)(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT struct list_head **ppUsingPacketList);
}RsPacketCacheManager;

ERRORTYPE RsPacketCacheManagerInit(RsPacketCacheManager *pThiz, int64_t nCacheTime);//unit:ms
ERRORTYPE RsPacketCacheManagerDestroy(RsPacketCacheManager *pThiz);
RsPacketCacheManager *RsPacketCacheManagerConstruct(int64_t nCacheTime);//unit:ms
void RsPacketCacheManagerDestruct(RsPacketCacheManager *manager);

#endif

