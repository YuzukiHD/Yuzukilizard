
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : fbm.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef FBM_H
#define FBM_H

#include "vdecoder.h"
#include <pthread.h>
#include "veInterface.h"

enum BUFFER_TYPE
{
    BUF_TYPE_REFERENCE_DISP = 0,
    BUF_TYPE_ONLY_REFERENCE,
    BUF_TYPE_ONLY_DISP,
};

typedef struct FRAMENODEFLAG FrameNodeFlag;
struct FRAMENODEFLAG
{
    int          bUsedByDecoder;
    int          bUsedByRender;
    int          bInValidPictureQueue;
    int          bAlreadyDisplayed;
    int          bNeedRelease;
};

typedef struct FRAMENODE FrameNode;
struct FRAMENODE
{
    FrameNodeFlag Flag;
    VideoPicture  vpicture;
    FrameNode*    pNext;
};

typedef struct FRAMEBUFFERMANAGER
{
    pthread_mutex_t mutex;
    int             nMaxFrameNum;
    int             nEmptyBufferNum;
    int             nValidPictureNum;
    int             nReleaseBufferNum;
    FrameNode*      pEmptyBufferQueue;
    FrameNode*      pValidPictureQueue;
    FrameNode*      pReleaseBufferQueue;
    int             bThumbnailMode;
    FrameNode*      pFrames;

    int             bUseGpuBuf;
    int             nAlignValue;
    void*           pFbmInfo;
    int             nDecoderHoldingNum;
    int             nRenderHoldingNum;
    int             nWaitForDispNum;
    struct ScMemOpsS *memops;
    VeOpsS*           sVeops;
    void*             pVeOpsSelf;
}Fbm;

typedef struct FBMCREATEINFO
{
    int nFrameNum;
    int nDecoderNeededMiniFrameNum;
    int nWidth;
    int nHeight;
    int ePixelFormat;
    int bThumbnailMode;

    int bGpuBufValid;
    int nAlignStride;
    int nBufferType;
    int bProgressiveFlag;
    int bIsSoftDecoderFlag;
    struct ScMemOpsS *memops;
    int b10BitStreamFlag;
    VeOpsS*           veOpsS;
    void*             pVeOpsSelf;
}FbmCreateInfo;

Fbm* FbmCreate(FbmCreateInfo* pFbmCreateInfo, VideoFbmInfo* pFbmInfo);

void FbmDestroy(Fbm* pFbm);

void FbmFlush(Fbm* pFbm);

int FbmGetBufferInfo(Fbm* pFbm, VideoPicture* pVPicture);

int FbmTotalBufferNum(Fbm* pFbm);

int FbmEmptyBufferNum(Fbm* pFbm);

int FbmValidPictureNum(Fbm* pFbm);

VideoPicture* FbmRequestBuffer(Fbm* pFbm);

void FbmReturnBuffer(Fbm* pFbm, VideoPicture* pVPicture, int bValidPicture);

void FbmShareBuffer(Fbm* pFbm, VideoPicture* pVPicture);

VideoPicture* FbmRequestPicture(Fbm* pFbm);

int FbmReturnPicture(Fbm* pFbm, VideoPicture* pPicture);

VideoPicture* FbmNextPictureInfo(Fbm* pFbm);

int FbmAllocatePictureBuffer(Fbm* pFbm,
    VideoPicture* pPicture,
    int* nAlignValue,
    int nWidth,
    int nHeight);

int FbmFreePictureBuffer(Fbm* pFbm, VideoPicture* pPicture);
int FbmGetAlignValue(Fbm* pFbm);

FbmBufInfo* FbmGetVideoBufferInfo(VideoFbmInfo* pFbmInf);
VideoPicture* FbmSetFbmBufAddress(VideoFbmInfo* pFbmInfo,
    VideoPicture* pPicture,
    int bForbidUseFlag);
int FbmNewDispRelease(Fbm* pFbm);
int FbmSetNewDispRelease(VideoFbmInfo* pFbmInfo);
VideoPicture* FbmRequestReleasePicture(VideoFbmInfo* pFbmInfo);
VideoPicture* FbmReturnReleasePicture(VideoFbmInfo* pFbmInfo,
    VideoPicture* pVpicture,
    int bForbidUseFlag);
unsigned int FbmGetBufferOffset(Fbm* pFbm, int isYBufFlag);

int FbmGetDisplayBufferNum(Fbm* pFbm);
void FbmDebugPrintStatus(Fbm* pFbm);
#endif

