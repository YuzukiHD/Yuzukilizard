/*
 * Copyright (C) 2008-2015 Allwinner Technology Co. Ltd.
 * Author: Ning Fang <fangning@allwinnertech.com>
 *         Caoyuan Yang <yangcaoyuan@allwinnertech.com>
 *
 * This software is confidential and proprietary and may be used
 * only as expressly authorized by a licensing agreement from
 * Softwinner Products.
 *
 * The entire notice above must be reproduced on all copies
 * and should not be removed.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef _BITSTREAM_MANAGER_H_
#define _BITSTREAM_MANAGER_H_

#include <pthread.h>

typedef struct StreamInfo
{
    int         nStreamOffset;
    int         nStreamLength;
    long long   nPts;
    int         nFlags;
    int         nID;

    int         CurrQp;
    int         avQp;
    int         nGopIndex;
    int         nFrameIndex;
    int         nTotalIndex;
    unsigned int nThumbLen;
    unsigned int nThumbPreExifDataLen;
}StreamInfo;

typedef struct BSListQ
{
    StreamInfo* pStreamInfos;
    int         nMaxFrameNum;
    int         nValidFrameNum;
    int         nUnReadFrameNum;
    int         nReadPos;
    int         nWritePos;
}BSListQ;

typedef struct BitStreamManager
{
    unsigned char  bEncH264Nalu;
    pthread_mutex_t mutex;
    char*           pStreamBuffer;
    char*           pStreamBufferPhyAddrEnd;
    char*           pStreamBufferPhyAddr;
    int             nStreamBufferSize;
    int             nWriteOffset;
    int             nValidDataSize;
    BSListQ         nBSListQ;
    struct ScMemOpsS *memops;
    void            *veOps;
    void            *pVeopsSelf;
}BitStreamManager;

BitStreamManager* BitStreamCreate(unsigned char bIsVbvNoCache, int nBufferSize, struct ScMemOpsS *memops,
                                      void *veOps, void *pVeopsSelf);
void BitStreamDestroy(BitStreamManager* handle);
void* BitStreamBaseAddress(BitStreamManager* handle);
void* BitStreamBasePhyAddress(BitStreamManager* handle);
void* BitStreamEndPhyAddress(BitStreamManager* handle);
int BitStreamBufferSize(BitStreamManager* handle);
int BitStreamFreeBufferSize(BitStreamManager* handle);
int BitStreamFrameNum(BitStreamManager* handle);
int BitStreamWriteOffset(BitStreamManager* handle);
int BitStreamAddOneBitstream(BitStreamManager* handle, StreamInfo* pStreamInfo);
StreamInfo* BitStreamGetOneBitstream(BitStreamManager* handle);
StreamInfo* BitStreamGetOneBitstreamInfo(BitStreamManager* handle);

int BitStreamReturnOneBitstream(BitStreamManager* handle, StreamInfo* pStreamInfo);
int BitStreamReset(BitStreamManager* handle, struct ScMemOpsS *memops);

#endif //_BITSTREAM_MANAGER_H_

#ifdef __cplusplus
}
#endif /* __cplusplus */

