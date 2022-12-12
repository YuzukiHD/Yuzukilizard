
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : videoengine.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef VIDEOENGINE_H
#define VIDEOENGINE_H

#include "vdecoder.h"
#include "sbm.h"
#include "fbm.h"

/******************* decoder const fbm buffer start *********************/
/* each decoder's mini number
AVS_DECODER_NEEDED_FRAME_NUM (2)
MJPEG_DECODER_NEEDED_FRAME_NUM (2)
MPEG2_DECODER_NEEDED_FRAME_NUM (2)
MPEG4_DECODER_NEEDED_FRAME_NUM (2)
VP8_DECODER_NEEDED_FRAME_NUM (4)
VC1_DECODER_NEEDED_FRAME_NUM (2)
RV_DECODER_NEEDED_FRAME_NUM (2)
 h264 h265 decoder's mini number need to add reference picture number;
H264_DECODER_NEEDED_FRAME_NUM_MINUS_REF (1)
H264_DISP_NEEDED_FRAME_NUM_SCALE_DOWN (8)
H265_DECODER_NEEDED_FRAME_NUM_MINUS_REF (2)
 * */
#define AVS_DECODER_NEEDED_FRAME_NUM (2+0)
#define MJPEG_DECODER_NEEDED_FRAME_NUM (2+0)
#define MPEG2_DECODER_NEEDED_FRAME_NUM (2+0)
#define MPEG4_DECODER_NEEDED_FRAME_NUM (2+0)
#define VP8_DECODER_NEEDED_FRAME_NUM (4+0)
#define VC1_DECODER_NEEDED_FRAME_NUM (2+0)
#define RV_DECODER_NEEDED_FRAME_NUM (2+0)
#define H264_DECODER_NEEDED_FRAME_NUM_MINUS_REF (1+0)
#define H264_DISP_NEEDED_FRAME_NUM_SCALE_DOWN (8+0)
#define H265_DECODER_NEEDED_FRAME_NUM_MINUS_REF (3+0)
/******************* decoder const fbm buffer end  *********************/

typedef struct DECODERINTERFACE DecoderInterface;
struct DECODERINTERFACE
{
    int  (*Init)(DecoderInterface* pSelf,
                 VConfig* pConfig,
                 VideoStreamInfo* pVideoInfo,
                 VideoFbmInfo* pFbmInfo);
    void (*Reset)(DecoderInterface* pSelf);
    int  (*SetSbm)(DecoderInterface* pSelf, SbmInterface* pSbm, int nIndex);
    int  (*GetFbmNum)(DecoderInterface* pSelf);
    Fbm* (*GetFbm)(DecoderInterface* pSelf, int nIndex);
    int  (*Decode)(DecoderInterface* pSelf,
                   int               bEndOfStream,
                   int               bDecodeKeyFrameOnly,
                   int               bSkipBFrameIfDelay,
                   int64_t           nCurrentTimeUs);
    void (*Destroy)(DecoderInterface* pSelf);
    int (*SetSpecialData)(DecoderInterface* pSelf, void *pArg);
    int (*SetExtraScaleInfo)(DecoderInterface* pSelf,
                             int nWidthTh,
                             int nHeightTh,
                             int nHorizonScaleRatio,
                             int nVerticalScaleRatio);
    int(*SetPerformCmd)(DecoderInterface* pSelf,
                        enum EVDECODERSETPERFORMCMD performCmd);
    int(*GetPerformInfo)(DecoderInterface* pSelf,
                         enum EVDECODERGETPERFORMCMD performCmd,
                         VDecodePerformaceInfo** performInfo);
};

typedef struct VIDEOENGINE
{
    VConfig           vconfig;
    VideoStreamInfo   videoStreamInfo;
    void*             pLibHandle;
    DecoderInterface* pDecoderInterface;
    int               bIsSoftwareDecoder;
    VideoFbmInfo      fbmInfo;
    u8                nResetVeMode;
    VeOpsS*           veOpsS;
    void*             pVeOpsSelf;
    uint64_t          nIcVeVersion;
    uint32_t          ndecIpVersion;

    SbmInterface*       pSbm[2];
    s32               bEnableGoogleVp9Flag;

    int nTrhandle;
    int nTrChannel;
    int bInitTrFlag;

}VideoEngine;

typedef DecoderInterface *VDecoderCreator(VideoEngine *);

int VDecoderRegister(enum EVIDEOCODECFORMAT format,
                  char *desc,
                  VDecoderCreator *creator,
                  int bIsSoft);

VideoEngine* VideoEngineCreate(VConfig* pVConfig,
                            VideoStreamInfo* pVideoInfo);

void VideoEngineDestroy(VideoEngine* pVideoEngine);

void VideoEngineReset(VideoEngine* pVideoEngine);

int VideoEngineSetSbm(VideoEngine* pVideoEngine, SbmInterface* pSbm, int nIndex);

int VideoEngineGetFbmNum(VideoEngine* pVideoEngine);

Fbm* VideoEngineGetFbm(VideoEngine* pVideoEngine, int nIndex);

int VideoEngineDecode(VideoEngine* pVideoEngine,
                      int          bEndOfStream,
                      int          bDecodeKeyFrameOnly,
                      int          bDropBFrameIfDelay,
                      int64_t      nCurrentTimeUs);

int VideoEngineRotatePicture(VideoEngine* pVideoEngine,
                                   VideoPicture* pPictureIn,
                                   VideoPicture* pPictureOut,
                                   int           nRotateDegree);

int GetBufferSize(int ePixelFormat,
               int nWidth,
               int nHeight,
               int* nYBufferSize,
               int *nCBufferSize,
               int* nYLineStride,
               int* nCLineStride,
               int nAlignValue);

int VideoEngineReopen(VideoEngine* pVideoEngine,
                            VConfig* pVConfig, VideoStreamInfo* pVideoInfo);

#endif

