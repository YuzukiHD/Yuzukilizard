/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : PreviewWindow.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/03
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "PreviewWindow"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys_linux_ioctl.h>
#include <type_camera.h>
#include <hwdisplay.h>
#include <mpi_vo.h>
#include <mpi_clock.h>
#include <mpi_sys.h>
#include <Clock_Component.h>
#include <VIDEO_FRAME_INFO_S.h>
#include <linux/g2d_driver.h>
#include <PIXEL_FORMAT_E_g2d_format_convert.h>

#include "PreviewWindow.h"
#include "VIChannel.h"
#include "PreviewRotate.h"

#define PREVIEW_PREFIX_VALUE  (0x0DEF)
#define PREVIEW_BUFID_PREFIX  (PREVIEW_PREFIX_VALUE<<16)

#define USE_MP_CONVERT

#define PREVIEW_ROTATION_MASK  (0x0000FFFF)
#define PREVIEW_MIRROR_MASK    (0xFFFF0000)

//#define VO_DISP_BUF_NUM (0)

using namespace std;
namespace EyeseeLinux {

PreviewWindow::PreviewWindow(CameraBufferReference *pcbr)
    : mpCameraBufRef(pcbr)
    , mbPreviewEnabled(false)
    , mHlay(-1)
    //, mbPreviewNeedSetSrc(false)
    //, mbWaitFirstFrame(false)
{
    mDispBufNum = 0;
    mDisplayFrameRate = 0;
    mPrevFramePts = -1;
    mPreviewRotation = 0;
    mFrameCnt = 0;
    mG2DHandle = -1;
    mVOLayer = MM_INVALID_DEV;
    mVOChn = MM_INVALID_CHN;
    memset(&mUserRegion_Ro,0,sizeof(mUserRegion_Ro));
    memset(&mB4G2dUserRegion_Ro,0,sizeof(mB4G2dUserRegion_Ro));
    mUserRegionUpdateFlag = false;
    mB4G2dUserRegionFlag = false;
    mbPreviewPause = false;
    mRotationBufIdCounter = 0;
#ifdef DEBUG_PREVWINDOW_SAVE_PICTURE
    mDebugSaveData = 0
#endif
    //mClockChn = MM_INVALID_CHN;
    //memset(&mClockChnAttr, 0, sizeof(CLOCK_CHN_ATTR_S));
}

PreviewWindow::~PreviewWindow()
{
    Mutex::Autolock lock(mBufferLock);
    if(mDisplayFrameBufferList.size() > 0)
    {
        aloge("fatal error! why display frame count[%d] exist?", mDisplayFrameBufferList.size());
        mIdleFrameBufferList.splice(mIdleFrameBufferList.end(), mDisplayFrameBufferList);
    }
    if(mReadyFrameBufferList.size() > 0)
    {
        aloge("fatal error! why ready frame count[%d] exist?", mReadyFrameBufferList.size());
        mIdleFrameBufferList.splice(mIdleFrameBufferList.end(), mReadyFrameBufferList);
    }
    if(mUsingFrameBufferList.size() > 0)
    {
        aloge("fatal error! why using frame count[%d] exist?", mUsingFrameBufferList.size());
        mIdleFrameBufferList.splice(mIdleFrameBufferList.end(), mUsingFrameBufferList);
    }
    //release frame buffer memory.
    for(VIDEO_FRAME_BUFFER_S& FrmBuf : mIdleFrameBufferList)
    {
        for(int i=0; i<3; i++)
        {
            if(FrmBuf.mFrameBuf.VFrame.mpVirAddr[i] != NULL)
            {
                AW_MPI_SYS_MmzFree(FrmBuf.mFrameBuf.VFrame.mPhyAddr[i], FrmBuf.mFrameBuf.VFrame.mpVirAddr[i]);
            }
        }
    }
}

status_t PreviewWindow::setPreviewWindow(int hlay)
{
    AutoMutex lock(mLock);
    mHlay = hlay;
    return NO_ERROR;
}

status_t PreviewWindow::changePreviewWindow(int hlay)
{
    AutoMutex lock(mLock);
    mHlay = hlay;
    mVOLayer = mHlay;
    mVOChn = 0;

    ERRORTYPE ret;
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)this;
    cbInfo.callback = (MPPCallbackFuncType)&VOCallbackWrapper;
    AW_MPI_VO_RegisterCallback(mVOLayer, mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(mVOLayer, mVOChn, mDispBufNum);
    
    unsigned int nDispBufNum = -1;
    ret = AW_MPI_VO_GetChnDispBufNum(mVOLayer, mVOChn, &nDispBufNum);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo[%d,%d] get DispBufNum fail[0x%x]", mVOLayer, mVOChn, ret);
    }
    SIZE_S stDisplaySize = {0, 0};
    ret = AW_MPI_VO_GetDisplaySize (mVOLayer, mVOChn, &stDisplaySize);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo[%d,%d] get DisplaySize fail[0x%x]", mVOLayer, mVOChn, ret);
    }
    int nFrmRate = -1;
    ret = AW_MPI_VO_GetChnFrameRate(mVOLayer, mVOChn, &nFrmRate);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo[%d,%d] get FrameRate fail[0x%x]", mVOLayer, mVOChn, ret);
    }
    alogd("change to mpi_vo of videoLayer[%d], dispBufNum:%d, DisplaySize:%dx%d, FrameRate:%d", mVOLayer, nDispBufNum, 
        stDisplaySize.Width, stDisplaySize.Height, nFrmRate);
    return NO_ERROR;
}

status_t PreviewWindow::setDispBufferNum(int nDispBufNum)
{
    AutoMutex lock(mLock);
    mDispBufNum = nDispBufNum;
    return NO_ERROR;
}

status_t PreviewWindow::startPreview()
{
    status_t result = NO_ERROR;
    AutoMutex lock(mLock);
    //mbPreviewEnabled = true;
    //mbPreviewNeedSetSrc = true;
    //mbWaitFirstFrame = true;

    if(true == mbPreviewEnabled)
    {
        alogv("already enable preview");
        return result;
    }
    if(mHlay < 0)
    {
        alogv("cannot enable preview using invalid hlay[%d]", mHlay);
        return UNKNOWN_ERROR;
    }
    ERRORTYPE ret;
    bool bSuccessFlag = false;
    mVOLayer = mHlay;
    mVOChn = 0;
    while(mVOChn < VO_MAX_CHN_NUM)
    {
        ret = AW_MPI_VO_CreateChn(mVOLayer, mVOChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("create vo channel[%d] success!", mVOChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == ret)
        {
            alogv("vo channel[%d] is exist, find next!", mVOChn);
            mVOChn++;
        }
        else
        {
            alogd("create vo channel[%d] ret[0x%x]!", mVOChn, ret);
            break;
        }
    }

    if(false == bSuccessFlag)
    {
        mVOChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        return UNKNOWN_ERROR;
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)this;
    cbInfo.callback = (MPPCallbackFuncType)&VOCallbackWrapper;
    AW_MPI_VO_RegisterCallback(mVOLayer, mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(mVOLayer, mVOChn, mDispBufNum);
/*
    bSuccessFlag = false;
    mClockChnAttr.nWaitMask = 0;
    mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_VIDEO;
    mClockChn = 0;
    while(mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(mClockChn, &mClockChnAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("create clock channel[%d] success!", mClockChn);
            break;
        }
        else if(ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", mClockChn);
            mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", mClockChn, ret);
            break;
        }
    }
    if(false == bSuccessFlag)
    {
        mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
        result = UNKNOWN_ERROR;
        goto clock_fail;
    }
    cbInfo.cookie = (void*)this;
    cbInfo.callback = (MPPCallbackFuncType)&CLOCKCallbackWrapper;
    AW_MPI_CLOCK_RegisterCallback(mClockChn, &cbInfo);
    //bind component
    {
    MPP_CHN_S ClockChn{MOD_ID_CLOCK, 0, mClockChn};
    MPP_CHN_S VOChn{MOD_ID_VOU, mVOLayer, mVOChn};
    AW_MPI_SYS_Bind(&ClockChn, &VOChn);
    }
*/
    //start vo and clock.
    //AW_MPI_CLOCK_Start(mClockChn);
    AW_MPI_VO_StartChn(mVOLayer, mVOChn);
    mFrameCnt = 0;
#ifdef USE_MP_CONVERT
    // open MP driver
    if(mG2DHandle < 0)
    {
        mG2DHandle = open("/dev/g2d", O_RDWR, 0);
        if (mG2DHandle < 0)
        {
            aloge("fatal error! open /dev/g2d failed");
        }
        alogv("open /dev/g2d OK");
    }
    else
    {
        aloge("fatal error! why g2dDriver[%d] is open?", mG2DHandle);
    }
#endif
    mBufferLock.lock();
    if(mDisplayFrameBufferList.size() > 0)
    {
        aloge("fatal error! display frame num[%d]>0", mDisplayFrameBufferList.size());
    }
    if(mReadyFrameBufferList.size() > 0)
    {
        aloge("fatal error! ready frame num[%d]>0", mReadyFrameBufferList.size());
    }
    if(mUsingFrameBufferList.size() > 0)
    {
        aloge("fatal error! using frame num[%d]>0", mUsingFrameBufferList.size());
    }
    alogd("left [%d] idle frames at start preview", mIdleFrameBufferList.size());
    mBufferLock.unlock();
    mRotationBufIdCounter = 0;
    mPrevFramePts = -1;
    mbPreviewEnabled = true;
#ifdef DEBUG_PREVWINDOW_SAVE_PICTURE
    mDebugSaveData = 0;
#endif
    return result;

clock_fail:
    AW_MPI_VO_DestroyChn(mVOLayer, mVOChn);
    mVOLayer = MM_INVALID_DEV;
    mVOChn = MM_INVALID_CHN;
    return result;
}

status_t PreviewWindow::stopPreview()
{
    status_t result = NO_ERROR;
    AutoMutex lock(mLock);
    //mbPreviewEnabled = false;
    //mbPreviewNeedSetSrc = false;
    //mbWaitFirstFrame = false;
    //if(mHlay>=0)
    //{
    //    hwd_layer_close(mHlay);
    //}
    if(false == mbPreviewEnabled)
    {
        alogv("already disable preview");
        return result;
    }
    AW_MPI_VO_StopChn(mVOLayer, mVOChn);
    //AW_MPI_CLOCK_Stop(mClockChn);
    AW_MPI_VO_DestroyChn(mVOLayer, mVOChn);
    mVOLayer = MM_INVALID_DEV;
    mVOChn = MM_INVALID_CHN;
    //AW_MPI_CLOCK_DestroyChn(mClockChn);
    //mClockChn = MM_INVALID_CHN;
    if(mFrameCnt != 0)
    {
        aloge("fatal error! preview window still occupy [%d]frames", mFrameCnt);
    }
#ifdef USE_MP_CONVERT
    if(mG2DHandle >= 0)
    {
        close(mG2DHandle);
        mG2DHandle = -1;
    }
#endif
    mPrevFramePts = -1;
    mbPreviewEnabled = false;
    mBufferLock.lock();
    if(mDisplayFrameBufferList.size() > 0)
    {
        aloge("fatal error! display frame num[%d]>0", mDisplayFrameBufferList.size());
    }
    if(mReadyFrameBufferList.size() > 0)
    {
        aloge("fatal error! ready frame num[%d]>0", mReadyFrameBufferList.size());
    }
    if(mUsingFrameBufferList.size() > 0)
    {
        aloge("fatal error! using frame num[%d]>0", mUsingFrameBufferList.size());
    }
    if(mIdleFrameBufferList.size() > 0)
    {
        alogd("left [%d] idle frames at stop preview", mIdleFrameBufferList.size());
    }
    mRotationBufIdCounter = 0;
    mBufferLock.unlock();
    return result;
}
status_t PreviewWindow::pausePreview()
{
    status_t result = NO_ERROR;
    AutoMutex lock(mLock);
    if(false == mbPreviewEnabled)
    {
        alogv("already disable preview");
        return result;
    }
    if(true == mbPreviewPause)
    {
        alogv("already pause preview");
        return result;
    }
    AW_MPI_VO_StopChn(mVOLayer, mVOChn);
    if(mFrameCnt != 0)
    {
        aloge("fatal error! preview window layer[%d] still occupy [%d]frames when pause done.", mVOLayer, mFrameCnt);
    }
    mbPreviewPause = true;
    return result;
}

status_t PreviewWindow::resumePreview()
{
    status_t result = NO_ERROR;
    AutoMutex lock(mLock);
    if(false == mbPreviewEnabled)
    {
        alogv("already disable preview");
        return result;
    }
    if(false == mbPreviewPause)
    {
        alogv("already resume preview");
        return result;
    }
    AW_MPI_VO_StartChn(mVOLayer, mVOChn);
    if(mFrameCnt != 0)
    {
        aloge("fatal error! preview window layer[%d] still occupy [%d]frames when resume done.", mVOLayer, mFrameCnt);
    }
    mbPreviewPause = false;
    return result;
}

status_t PreviewWindow::storeDisplayFrame(uint64_t framePts)
{
    AutoMutex lock(mLock);
    if(false == mbPreviewEnabled)
    {
        alogd("Be careful! preview disable, return");
        return INVALID_OPERATION;
    }
    AW_MPI_VO_Debug_StoreFrame(mVOLayer, mVOChn, framePts);
    return NO_ERROR;
}

void PreviewWindow::setDisplayFrameRate(int fps)
{
    mDisplayFrameRate = fps;
}

void PreviewWindow::setPreviewRotation(int rotation)
{
    mPreviewRotation = rotation;
}

int PreviewWindow::getPreviewRotation()
{
    return mPreviewRotation;
}

status_t PreviewWindow::setPreviewRegion(RECT_S ParamRegion)
{
    status_t ret = NO_ERROR;
    AutoMutex locker(mLock);
    mUserRegion_Ro.X = ParamRegion.X;
    mUserRegion_Ro.Y = ParamRegion.Y;
    mUserRegion_Ro.Width = ParamRegion.Width;
    mUserRegion_Ro.Height = ParamRegion.Height;
    mUserRegionUpdateFlag = true;
    return ret;
}

status_t PreviewWindow::getPreviewRegion(RECT_S * pParamRegion)
{
    status_t ret = NO_ERROR;
    AutoMutex locker(mLock);
    pParamRegion->X = mUserRegion_Ro.X;
    pParamRegion->Y = mUserRegion_Ro.Y;
    pParamRegion->Width = mUserRegion_Ro.Width;
    pParamRegion->Height = mUserRegion_Ro.Height;
    return ret;
}

status_t PreviewWindow::setB4G2dPreviewRegion(RECT_S ParamRegion)
{
    status_t ret = NO_ERROR;
    AutoMutex locker(mLock);
    if(mB4G2dUserRegion_Ro.Width > 0 && mB4G2dUserRegion_Ro.Height > 0)
    {
        if(mB4G2dUserRegion_Ro.Width != ParamRegion.Width || mB4G2dUserRegion_Ro.Height != ParamRegion.Height)
        {
            alogw("Be careful! very very careful, B4G2dRegion shape changes![%dx%d]->[%dx%d]", 
                mB4G2dUserRegion_Ro.Width, mB4G2dUserRegion_Ro.Height, ParamRegion.Width, ParamRegion.Height);
        }
    }
    mB4G2dUserRegion_Ro.X = ParamRegion.X;
    mB4G2dUserRegion_Ro.Y = ParamRegion.Y;
    mB4G2dUserRegion_Ro.Width = ParamRegion.Width;
    mB4G2dUserRegion_Ro.Height = ParamRegion.Height;
    if(mB4G2dUserRegion_Ro.Width > 0 && mB4G2dUserRegion_Ro.Height > 0)
    {
        mB4G2dUserRegionFlag = true;
    }
    else
    {
        alogd("Be careful! user cancel B4G2dRegion!");
        mB4G2dUserRegionFlag = false;
    }
    return ret;
}

status_t PreviewWindow::getB4G2dPreviewRegion(RECT_S * pParamRegion)
{
    status_t ret = NO_ERROR;
    AutoMutex locker(mLock);
    pParamRegion->X = mB4G2dUserRegion_Ro.X;
    pParamRegion->Y = mB4G2dUserRegion_Ro.Y;
    pParamRegion->Width = mB4G2dUserRegion_Ro.Width;
    pParamRegion->Height = mB4G2dUserRegion_Ro.Height;
    return ret;
}

/**
 * nRotation: anti-close wise. 90, 180, 270
 */
status_t PreviewWindow::rotateFrame(const VIDEO_FRAME_INFO_S *pSrc, VIDEO_FRAME_INFO_S *pDes, int nRotation)
{
#ifdef USE_MP_CONVERT
  #if 0
    g2d_blt     blit_para;
    int         err;
    status_t    ret = NO_ERROR;
    if (nRotation != 90 && nRotation != 180 && nRotation != 270)
    {
        aloge("fatal error! rotation[%d] is invalid!", nRotation);
        return UNKNOWN_ERROR;
    }
    if(mG2DHandle < 0)
    {
        aloge("fatal error! g2d driver[%d] is not valid, can't rotate!", mG2DHandle);
        return UNKNOWN_ERROR;
    }
    g2d_data_fmt    eSrcFormat, eDstFormat;
    g2d_pixel_seq   eSrcPixelSeq, eDstPixelSeq;
    ERRORTYPE eError;
    eError = convert_PIXEL_FORMAT_E_to_G2dFormat(pSrc->VFrame.mPixelFormat, &eSrcFormat, &eSrcPixelSeq);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pSrc->VFrame.mPixelFormat);
        return UNKNOWN_ERROR;
    }
    eError = convert_PIXEL_FORMAT_E_to_G2dFormat(pDes->VFrame.mPixelFormat, &eDstFormat, &eDstPixelSeq);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pDes->VFrame.mPixelFormat);
        return UNKNOWN_ERROR;
    }
    memset(&blit_para, 0, sizeof(g2d_blt));
    blit_para.src_image.addr[0]      = pSrc->VFrame.mPhyAddr[0];
    blit_para.src_image.addr[1]      = pSrc->VFrame.mPhyAddr[1];
    blit_para.src_image.w            = pSrc->VFrame.mWidth;         /* src buffer width in pixel units */
    blit_para.src_image.h            = pSrc->VFrame.mHeight;        /* src buffer height in pixel units */
    blit_para.src_image.format       = eSrcFormat;
    blit_para.src_image.pixel_seq    = eSrcPixelSeq;//G2D_SEQ_VUVU;          /*  */
    blit_para.src_rect.x             = 0;                       /* src rect->x in pixel */
    blit_para.src_rect.y             = 0;                       /* src rect->y in pixel */
    blit_para.src_rect.w             = pSrc->VFrame.mWidth;           /* src rect->w in pixel */
    blit_para.src_rect.h             = pSrc->VFrame.mHeight;          /* src rect->h in pixel */

    blit_para.dst_image.addr[0]      = pDes->VFrame.mPhyAddr[0];
    blit_para.dst_image.addr[1]      = pDes->VFrame.mPhyAddr[1];
    blit_para.dst_image.w            = pDes->VFrame.mWidth;         /* dst buffer width in pixel units */
    blit_para.dst_image.h            = pDes->VFrame.mHeight;        /* dst buffer height in pixel units */
    blit_para.dst_image.format       = eDstFormat;
    blit_para.dst_image.pixel_seq    = eDstPixelSeq;
    blit_para.dst_x                  = 0;                   /* dst rect->x in pixel */
    blit_para.dst_y                  = 0;                   /* dst rect->y in pixel */
    blit_para.color                  = 0xff;                /* fix me*/
    blit_para.alpha                  = 0xff;                /* globe alpha */

    switch(nRotation)
    {
        case 90:
            blit_para.flag = G2D_BLT_ROTATE90;
            break;
        case 180:
            blit_para.flag = G2D_BLT_ROTATE180;
            break;
        case 270:
            blit_para.flag = G2D_BLT_ROTATE270;
            break;
        default:
            aloge("fatal error! rotation[%d] is invalid!", nRotation);
            blit_para.flag = G2D_BLT_NONE;
            break;
    }

    err = ioctl(mG2DHandle, G2D_CMD_BITBLT, (unsigned long)&blit_para);
    if(err < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed");
        ((VIChannel *)mpCameraBufRef)->PreviewG2dTimeoutCb();
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
        ret = UNKNOWN_ERROR;
    }
    return ret;

  #else
    g2d_blt_h   blit;
    int         err;
    status_t    ret = NO_ERROR;
    //set Rotation value
    PreviewRote nRotationValue = static_cast<PreviewRote>(nRotation & PREVIEW_ROTATION_MASK);
    if (nRotationValue != PreviewRote::PreviewRoteROT_0 
        && nRotationValue != PreviewRote::PreviewRoteROT_90 
        && nRotationValue != PreviewRote::PreviewRoteROT_180 
        && nRotationValue != PreviewRote::PreviewRoteROT_270 
        && nRotationValue != PreviewRote::PreviewRoteROT_360)
    {
        aloge("fatal error! rotation[%d] is invalid!", static_cast<int>(nRotationValue));
        return UNKNOWN_ERROR;
    }
    if(mG2DHandle < 0)
    {
        aloge("fatal error! g2d driver[%d] is not valid, can't rotate!", mG2DHandle);
        return UNKNOWN_ERROR;
    }
    g2d_fmt_enh eSrcFormat, eDstFormat;
    ERRORTYPE eError;
    eError = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pSrc->VFrame.mPixelFormat, &eSrcFormat);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pSrc->VFrame.mPixelFormat);
        return UNKNOWN_ERROR;
    }
    eError = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pDes->VFrame.mPixelFormat, &eDstFormat);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pDes->VFrame.mPixelFormat);
        return UNKNOWN_ERROR;
    }

    //config blit
    memset(&blit, 0, sizeof(g2d_blt_h));
    switch(nRotationValue)
    {
        case PreviewRote::PreviewRoteROT_90:
            blit.flag_h = G2D_ROT_90;
            break;
        case PreviewRote::PreviewRoteROT_180:
            blit.flag_h = G2D_ROT_180;
            break;
        case PreviewRote::PreviewRoteROT_270:
            blit.flag_h = G2D_ROT_270;
            break;
        default:
            //aloge("fatal error! rotation[%d] is invalid!", nRotation);
            //blit.flag_h = G2D_BLT_NONE_H;
            blit.flag_h = G2D_ROT_0;
            break;
    }

    //set mirror roration    
    int nMirrorFlag = (nRotation & PREVIEW_MIRROR_MASK) >> 16;
    switch(static_cast<PreviewRote>(nMirrorFlag))
    {
        case PreviewRote::PreviewRoteMIR_N:
            blit.flag_h = blit.flag_h;
            break;
        case PreviewRote::PreviewRoteMIR_H:
            blit.flag_h = g2d_blt_flags_h(blit.flag_h | G2D_ROT_H);
            break;
        case PreviewRote::PreviewRoteMIR_V:
            blit.flag_h = g2d_blt_flags_h(blit.flag_h | G2D_ROT_V);
            break;
        default:
            blit.flag_h = blit.flag_h;
            break;
    }
    blit.src_image_h.bbuff = 1;
    blit.src_image_h.use_phy_addr = 1;
    blit.src_image_h.color = 0xff;
    blit.src_image_h.format = eSrcFormat;
    blit.src_image_h.laddr[0] = pSrc->VFrame.mPhyAddr[0];
    blit.src_image_h.laddr[1] = pSrc->VFrame.mPhyAddr[1];
    blit.src_image_h.width = pSrc->VFrame.mWidth;
    blit.src_image_h.height = pSrc->VFrame.mHeight;
    blit.src_image_h.align[0] = 0;
    blit.src_image_h.align[1] = 0;
    blit.src_image_h.align[2] = 0;
    if(mB4G2dUserRegionFlag == true)
    {
        blit.src_image_h.clip_rect.x = mB4G2dUserRegion_Ro.X;
        blit.src_image_h.clip_rect.y = mB4G2dUserRegion_Ro.Y;
        blit.src_image_h.clip_rect.w = mB4G2dUserRegion_Ro.Width;
        blit.src_image_h.clip_rect.h = mB4G2dUserRegion_Ro.Height;
    }
    else
    {
        blit.src_image_h.clip_rect.x = pSrc->VFrame.mOffsetLeft;
        blit.src_image_h.clip_rect.y = pSrc->VFrame.mOffsetTop;
        blit.src_image_h.clip_rect.w = pSrc->VFrame.mOffsetRight - pSrc->VFrame.mOffsetLeft;
        blit.src_image_h.clip_rect.h = pSrc->VFrame.mOffsetBottom - pSrc->VFrame.mOffsetTop;
    }
    blit.src_image_h.gamut = G2D_BT601;
    blit.src_image_h.bpremul = 0;
    blit.src_image_h.alpha = 0xff;
    blit.src_image_h.mode = G2D_GLOBAL_ALPHA;

    blit.dst_image_h.bbuff = 1;
    blit.dst_image_h.use_phy_addr = 1;
    blit.dst_image_h.color = 0xff;
    blit.dst_image_h.format = eDstFormat;
    blit.dst_image_h.laddr[0] = pDes->VFrame.mPhyAddr[0];
    blit.dst_image_h.laddr[1] = pDes->VFrame.mPhyAddr[1];
    blit.dst_image_h.width = pDes->VFrame.mWidth;
    blit.dst_image_h.height = pDes->VFrame.mHeight;
    blit.dst_image_h.align[0] = 0;
    blit.dst_image_h.align[1] = 0;
    blit.dst_image_h.align[2] = 0;

    if(mB4G2dUserRegionFlag == true)
    {
        blit.dst_image_h.clip_rect.x = 0;
        blit.dst_image_h.clip_rect.y = 0;
        blit.dst_image_h.clip_rect.w = pDes->VFrame.mWidth;
        blit.dst_image_h.clip_rect.h = pDes->VFrame.mHeight;
    }
    else
    {
        blit.dst_image_h.clip_rect.x = pDes->VFrame.mOffsetLeft;
        blit.dst_image_h.clip_rect.y = pDes->VFrame.mOffsetTop;
        blit.dst_image_h.clip_rect.w = pDes->VFrame.mOffsetRight - pDes->VFrame.mOffsetLeft;
        blit.dst_image_h.clip_rect.h = pDes->VFrame.mOffsetBottom - pDes->VFrame.mOffsetTop;
    }
    blit.dst_image_h.gamut = G2D_BT601;
    blit.dst_image_h.bpremul = 0;
    blit.dst_image_h.alpha = 0xff;
    blit.dst_image_h.mode = G2D_GLOBAL_ALPHA;

    err = ioctl(mG2DHandle, G2D_CMD_BITBLT_H, (unsigned long)&blit);
    if(err < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed");
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
        ret = UNKNOWN_ERROR;
    }

    //alogd("debug g2d[0x%x]: virAddr[0x%x][0x%x], phyAddr[0x%x][0x%x], size[%dx%d]", mG2DHandle,
    //    pDes->VFrame.mpVirAddr[0], pDes->VFrame.mpVirAddr[1], pDes->VFrame.mPhyAddr[0], pDes->VFrame.mPhyAddr[1],
    //    pDes->VFrame.mWidth, pDes->VFrame.mHeight);
    //memset(pDes->VFrame.mpVirAddr[0], 0xff, pDes->VFrame.mWidth * pDes->VFrame.mHeight);
    //memset(pDes->VFrame.mpVirAddr[1], 0xff, pDes->VFrame.mWidth * pDes->VFrame.mHeight/2);
    //memcpy(pDes->VFrame.mpVirAddr[0], pSrc->VFrame.mpVirAddr[0], pDes->VFrame.mWidth * pDes->VFrame.mHeight);
    //memcpy(pDes->VFrame.mpVirAddr[1], pSrc->VFrame.mpVirAddr[1], pDes->VFrame.mWidth * pDes->VFrame.mHeight/2);
    return ret;
  #endif
#else
    aloge("fatal error! where is g2d driver?");
    return UNKNOWN_ERROR;
#endif
}

/**
 * malloc rotation frame buffers at one time, to avoid ion memory fragments.
 */
status_t PreviewWindow::PrePrepareRotationFrame(VIDEO_FRAME_BUFFER_S *pFrmbuf)
{
    Mutex::Autolock lock(mBufferLock);
    
    int nFrameNum = mIdleFrameBufferList.size() + mUsingFrameBufferList.size() + mReadyFrameBufferList.size() + mDisplayFrameBufferList.size();
    if(nFrameNum > 0)
    {
        return NO_ERROR;
    }
    
    int nRotation = mPreviewRotation;
    if(0 == nRotation)
    {
        aloge("fatal error! rotation is 0, why rotate?");
        return BAD_VALUE;
    }
    PreviewRote nRotationValue = static_cast<PreviewRote>(nRotation & PREVIEW_ROTATION_MASK);
    
    if((pFrmbuf->mFrameBuf.mId & 0xFFFF0000) != 0)
    {
        aloge("fatal error! vi frame buf id [0x%x] is conflict with my wish, check code!", pFrmbuf->mFrameBuf.mId);
    }
    VIDEO_FRAME_BUFFER_S rFrmBuf;
    memset(&rFrmBuf, 0, sizeof(VIDEO_FRAME_BUFFER_S));
    VIDEO_FRAME_INFO_S alignFrameBuf = pFrmbuf->mFrameBuf;
    if(mB4G2dUserRegionFlag == true)
    {
        alignFrameBuf.VFrame.mWidth = ALIGN(mB4G2dUserRegion_Ro.Width, 8);
        alignFrameBuf.VFrame.mHeight = ALIGN(mB4G2dUserRegion_Ro.Height, 8);
    }
    else
    {
        alignFrameBuf.VFrame.mWidth = ALIGN(pFrmbuf->mFrameBuf.VFrame.mWidth, 8);
        alignFrameBuf.VFrame.mHeight = ALIGN(pFrmbuf->mFrameBuf.VFrame.mHeight, 8);
    }
    if(PreviewRote::PreviewRoteROT_90 == nRotationValue || PreviewRote::PreviewRoteROT_270 == nRotationValue)
    {
        rFrmBuf.mFrameBuf.VFrame.mWidth = alignFrameBuf.VFrame.mHeight;
        rFrmBuf.mFrameBuf.VFrame.mHeight = alignFrameBuf.VFrame.mWidth;
    }
    else if((static_cast<int>(nRotationValue)%static_cast<int>(PreviewRote::PreviewRoteROT_180)) == 0)
    {
        rFrmBuf.mFrameBuf.VFrame.mWidth = alignFrameBuf.VFrame.mWidth;
        rFrmBuf.mFrameBuf.VFrame.mHeight = alignFrameBuf.VFrame.mHeight;
    }
    else
    {
        aloge("fatal error! rotation[%d] is not support!", nRotation);
    }
    rFrmBuf.mFrameBuf.VFrame.mField = pFrmbuf->mFrameBuf.VFrame.mField;
    rFrmBuf.mFrameBuf.VFrame.mPixelFormat = pFrmbuf->mFrameBuf.VFrame.mPixelFormat;
    rFrmBuf.mFrameBuf.VFrame.mVideoFormat = pFrmbuf->mFrameBuf.VFrame.mVideoFormat;
    rFrmBuf.mFrameBuf.VFrame.mCompressMode = pFrmbuf->mFrameBuf.VFrame.mCompressMode;
    rFrmBuf.mFrameBuf.mId = PREVIEW_BUFID_PREFIX | mRotationBufIdCounter++;
    alogv("rotate buf id:0x%x", rFrmBuf.mFrameBuf.mId);
    rFrmBuf.mColorSpace = pFrmbuf->mColorSpace;
    rFrmBuf.mRotation = nRotation;

    VideoFrameBufferSizeInfo SizeInfo;
    memset(&SizeInfo, 0, sizeof(VideoFrameBufferSizeInfo));
    getVideoFrameBufferSizeInfo(&alignFrameBuf, &SizeInfo);
    //alogd("MmzAlloc:[mYSize:%d] [mUSize:%d] [mVSize:%d]",SizeInfo.mYSize,SizeInfo.mUSize,SizeInfo.mVSize);
    ERRORTYPE mallocRet = SUCCESS;
    int i = 0;
    for(i=0; i<4; i++)
    {
        for(int j=0; j<3;j++)
        {
            rFrmBuf.mFrameBuf.VFrame.mPhyAddr[j] = 0;
            rFrmBuf.mFrameBuf.VFrame.mpVirAddr[j] = NULL;
            rFrmBuf.mFrameBuf.VFrame.mStride[j] = 0;
        }
        if(SizeInfo.mYSize > 0)
        {
            unsigned int nPhyAddr = 0;
            void* pVirtAddr = NULL;
            if(SUCCESS == AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &pVirtAddr, SizeInfo.mYSize))
            {
                rFrmBuf.mFrameBuf.VFrame.mPhyAddr[0] = nPhyAddr;
                rFrmBuf.mFrameBuf.VFrame.mpVirAddr[0] = pVirtAddr;
                rFrmBuf.mFrameBuf.VFrame.mStride[0] = alignFrameBuf.VFrame.mWidth;
            }
            else
            {
                mallocRet = FAILURE;
            }
        }
        if(SizeInfo.mUSize > 0)
        {
            unsigned int nPhyAddr = 0;
            void* pVirtAddr = NULL;
            if(SUCCESS == AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &pVirtAddr, SizeInfo.mUSize))
            {
                rFrmBuf.mFrameBuf.VFrame.mPhyAddr[1] = nPhyAddr;
                rFrmBuf.mFrameBuf.VFrame.mpVirAddr[1] = pVirtAddr;
                rFrmBuf.mFrameBuf.VFrame.mStride[1] = 0;
            }
            else
            {
                mallocRet = FAILURE;
            }
        }
        if(SizeInfo.mVSize > 0)
        {
            unsigned int nPhyAddr = 0;
            void* pVirtAddr = NULL;
            if(SUCCESS == AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &pVirtAddr, SizeInfo.mVSize))
            {
                rFrmBuf.mFrameBuf.VFrame.mPhyAddr[2] = nPhyAddr;
                rFrmBuf.mFrameBuf.VFrame.mpVirAddr[2] = pVirtAddr;
                rFrmBuf.mFrameBuf.VFrame.mStride[2] = 0;
            }
            else
            {
                mallocRet = FAILURE;
            }
        }
        //judge if malloc success
        if(SUCCESS == mallocRet)
        {
            mIdleFrameBufferList.push_back(rFrmBuf);
        }
        else
        {
            aloge("fatal error! ion malloc fail!");
            for(int i=0;i<2;i++)
            {
                if(rFrmBuf.mFrameBuf.VFrame.mpVirAddr[i] != NULL)
                {
                    AW_MPI_SYS_MmzFree(rFrmBuf.mFrameBuf.VFrame.mPhyAddr[i], rFrmBuf.mFrameBuf.VFrame.mpVirAddr[i]);
                }
            }
            break;
        }
    }
    alogw("previewWindow layer[%d] pre prepare [%d] rotate video frames!", mVOLayer, i);
    return NO_ERROR;
}

//note: bufId must distinguish with vi frame.
status_t PreviewWindow::prepareRotationFrame(VIDEO_FRAME_BUFFER_S *pFrmbuf)
{
    status_t ret = NO_ERROR;
    //Mutex::Autolock lock(mBufferLock);
    //get idle frmBuf node.
    int nRotation = mPreviewRotation;
    if(0 == nRotation)
    {
        aloge("fatal error! rotation is 0, why rotate?");
        return BAD_VALUE;
    }
    PreviewRote nRotationValue = static_cast<PreviewRote>(nRotation & PREVIEW_ROTATION_MASK);

    int nRotateFrameNum = 0;
    mBufferLock.lock();
    if(mIdleFrameBufferList.empty())
    {
        int n1 = mUsingFrameBufferList.size();
        int n2 = mReadyFrameBufferList.size();
        int n3 = mDisplayFrameBufferList.size();
        if(n1 + n2 + n3 >= 4)
        {
            alogv("previewWindow layer[%d] already has [%d] frames, perhaps vo not return frame? [%d][%d][%d]", mVOLayer, n1, n2, n3);
            mBufferLock.unlock();
            return UNKNOWN_ERROR;
        }
        nRotateFrameNum = n1+n2+n3;
        alogw("previewWindow layer[%d] rotate frame num increase to [%d]", mVOLayer, nRotateFrameNum+1);
        mIdleFrameBufferList.emplace_back();
        memset(&mIdleFrameBufferList.back(), 0, sizeof(VIDEO_FRAME_BUFFER_S));
    }
    mUsingFrameBufferList.splice(mUsingFrameBufferList.end(), mIdleFrameBufferList, mIdleFrameBufferList.begin());
    if(mUsingFrameBufferList.size() > 1)
    {
        aloge("fatal error! why using frame count[%d]>1? check code!", mUsingFrameBufferList.size());
    }
    mBufferLock.unlock();
    //member assign
    VIDEO_FRAME_BUFFER_S &rFrmBuf = mUsingFrameBufferList.front();
    if(0 == rFrmBuf.mRotation)
    {
        alogv("first use frame, continue set");
        VIDEO_FRAME_INFO_S alignFrameBuf = pFrmbuf->mFrameBuf;
        if(mB4G2dUserRegionFlag == true)
        {
            alignFrameBuf.VFrame.mWidth = ALIGN(mB4G2dUserRegion_Ro.Width, 8);
            alignFrameBuf.VFrame.mHeight = ALIGN(mB4G2dUserRegion_Ro.Height, 8);
        }
        else
        {
            alignFrameBuf.VFrame.mWidth = ALIGN(pFrmbuf->mFrameBuf.VFrame.mWidth, 8);
            alignFrameBuf.VFrame.mHeight = ALIGN(pFrmbuf->mFrameBuf.VFrame.mHeight, 8);
        }
        if(PreviewRote::PreviewRoteROT_90 == nRotationValue || PreviewRote::PreviewRoteROT_270 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mWidth = alignFrameBuf.VFrame.mHeight;
            rFrmBuf.mFrameBuf.VFrame.mHeight = alignFrameBuf.VFrame.mWidth;
        }
        else if((static_cast<int>(nRotationValue)%static_cast<int>(PreviewRote::PreviewRoteROT_180)) == 0)
        {
            rFrmBuf.mFrameBuf.VFrame.mWidth = alignFrameBuf.VFrame.mWidth;
            rFrmBuf.mFrameBuf.VFrame.mHeight = alignFrameBuf.VFrame.mHeight;
        }
        else
        {
            aloge("fatal error! rotation[%d] is not support!", nRotation);
        }
        rFrmBuf.mFrameBuf.VFrame.mField = pFrmbuf->mFrameBuf.VFrame.mField;
        rFrmBuf.mFrameBuf.VFrame.mPixelFormat = pFrmbuf->mFrameBuf.VFrame.mPixelFormat;
        rFrmBuf.mFrameBuf.VFrame.mVideoFormat = pFrmbuf->mFrameBuf.VFrame.mVideoFormat;
        rFrmBuf.mFrameBuf.VFrame.mCompressMode = pFrmbuf->mFrameBuf.VFrame.mCompressMode;
        if(rFrmBuf.mFrameBuf.VFrame.mPhyAddr[0] != 0)
        {
            aloge("fatal error! mPhyAddr[0]=[%u], check code!", rFrmBuf.mFrameBuf.VFrame.mPhyAddr[0]);
        }
        VideoFrameBufferSizeInfo SizeInfo;
        memset(&SizeInfo, 0, sizeof(VideoFrameBufferSizeInfo));
        getVideoFrameBufferSizeInfo(&alignFrameBuf, &SizeInfo);
        ERRORTYPE mallocRet = SUCCESS;
        if(SizeInfo.mYSize > 0)
        {
            unsigned int nPhyAddr = 0;
            void* pVirtAddr = NULL;
            if(SUCCESS == AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &pVirtAddr, SizeInfo.mYSize))
            {
                rFrmBuf.mFrameBuf.VFrame.mPhyAddr[0] = nPhyAddr;
                rFrmBuf.mFrameBuf.VFrame.mpVirAddr[0] = pVirtAddr;
                rFrmBuf.mFrameBuf.VFrame.mStride[0] = alignFrameBuf.VFrame.mWidth;
            }
            else
            {
                mallocRet = FAILURE;
            }
        }
        if(SizeInfo.mUSize > 0)
        {
            unsigned int nPhyAddr = 0;
            void* pVirtAddr = NULL;
            if(SUCCESS == AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &pVirtAddr, SizeInfo.mUSize))
            {
                rFrmBuf.mFrameBuf.VFrame.mPhyAddr[1] = nPhyAddr;
                rFrmBuf.mFrameBuf.VFrame.mpVirAddr[1] = pVirtAddr;
                rFrmBuf.mFrameBuf.VFrame.mStride[1] = 0;
            }
            else
            {
                mallocRet = FAILURE;
            }
        }
        if(SizeInfo.mVSize > 0)
        {
            unsigned int nPhyAddr = 0;
            void* pVirtAddr = NULL;
            if(SUCCESS == AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &pVirtAddr, SizeInfo.mVSize))
            {
                rFrmBuf.mFrameBuf.VFrame.mPhyAddr[2] = nPhyAddr;
                rFrmBuf.mFrameBuf.VFrame.mpVirAddr[2] = pVirtAddr;
                rFrmBuf.mFrameBuf.VFrame.mStride[2] = 0;
            }
            else
            {
                mallocRet = FAILURE;
            }
        }
        //judge if malloc success
        if(mallocRet!=SUCCESS)
        {
            aloge("fatal error! ion malloc fail!");
            for(int i=0;i<2;i++)
            {
                AW_MPI_SYS_MmzFree(rFrmBuf.mFrameBuf.VFrame.mPhyAddr[i], rFrmBuf.mFrameBuf.VFrame.mpVirAddr[i]);
            }
            mUsingFrameBufferList.pop_front();
            return UNKNOWN_ERROR;
        }
        if(PreviewRote::PreviewRoteROT_90 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft= pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
        }
        else if(PreviewRote::PreviewRoteROT_180 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
        }
        else if(PreviewRote::PreviewRoteROT_270 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
        }
        else
        {
            aloge("fatal error! rotation[%d] is invalid, check code!", nRotation);
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
        }

        rFrmBuf.mFrameBuf.VFrame.mpts = pFrmbuf->mFrameBuf.VFrame.mpts;
        if((pFrmbuf->mFrameBuf.mId & 0xFFFF0000) != 0)
        {
            aloge("fatal error! vi frame buf id [0x%x] is conflict with my wish, check code!", pFrmbuf->mFrameBuf.mId);
        }
        rFrmBuf.mFrameBuf.mId = PREVIEW_BUFID_PREFIX | mRotationBufIdCounter++;
        alogv("rotate buf id:0x%x", rFrmBuf.mFrameBuf.mId);

        rFrmBuf.mColorSpace = pFrmbuf->mColorSpace;
        if(rFrmBuf.mRefCnt != 0)
        {
            aloge("fatal error! refCnt[%d]!=0, check code!", rFrmBuf.mRefCnt);
            rFrmBuf.mRefCnt = 0;
        }
        rFrmBuf.mRotation = nRotation;
    }
    else if(rFrmBuf.mRotation == nRotation)
    {
        if(rFrmBuf.mFrameBuf.VFrame.mPhyAddr[0] == 0)
        {
            aloge("fatal error! mPhyAddr[0] = 0, check code!");
        }
        rFrmBuf.mFrameBuf.VFrame.mpts = pFrmbuf->mFrameBuf.VFrame.mpts;
        rFrmBuf.mColorSpace = pFrmbuf->mColorSpace;
        if(rFrmBuf.mRefCnt != 0)
        {
            aloge("fatal error! refCnt[%d]!=0, check code!", rFrmBuf.mRefCnt);
            rFrmBuf.mRefCnt = 0;
        }

        if(PreviewRote::PreviewRoteROT_90 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft= pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
        }
        else if(PreviewRote::PreviewRoteROT_180 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
        }
        else if(PreviewRote::PreviewRoteROT_270 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
        }
        else
        {
            //aloge("fatal error! rotation[%d] is invalid, check code!", nRotation);
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;    
        }
    }
    else
    {
        alogd("Be careful! rotation change [%d]->[%d], reconfig frame size", rFrmBuf.mRotation, nRotation);
        VIDEO_FRAME_INFO_S alignFrameBuf = pFrmbuf->mFrameBuf;
        alignFrameBuf.VFrame.mWidth = ALIGN(pFrmbuf->mFrameBuf.VFrame.mWidth, 8);
        alignFrameBuf.VFrame.mHeight = ALIGN(pFrmbuf->mFrameBuf.VFrame.mHeight, 8);
        if(PreviewRote::PreviewRoteROT_90 == nRotationValue || PreviewRote::PreviewRoteROT_270 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mWidth = alignFrameBuf.VFrame.mHeight;
            rFrmBuf.mFrameBuf.VFrame.mHeight = alignFrameBuf.VFrame.mWidth;
        }
        else if((static_cast<int>(nRotationValue)%static_cast<int>(PreviewRote::PreviewRoteROT_180)) == 0)
        {
            rFrmBuf.mFrameBuf.VFrame.mWidth = alignFrameBuf.VFrame.mWidth;
            rFrmBuf.mFrameBuf.VFrame.mHeight = alignFrameBuf.VFrame.mHeight;
        }
        else
        {
            aloge("fatal error! rotation[%d] is not support!", nRotation);
        }
        if(rFrmBuf.mFrameBuf.VFrame.mPhyAddr[0] == 0)
        {
            aloge("fatal error! mPhyAddr[0] = 0, check code!");
        }

        if(PreviewRote::PreviewRoteROT_90 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft= pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
        }
        else if(PreviewRote::PreviewRoteROT_180 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mHeight - pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
        }
        else if(PreviewRote::PreviewRoteROT_270 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mWidth - pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
        }
        else
        {
            aloge("fatal error! rotation[%d] is invalid, check code!", nRotation);
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = pFrmbuf->mFrameBuf.VFrame.mOffsetLeft;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = pFrmbuf->mFrameBuf.VFrame.mOffsetTop;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = pFrmbuf->mFrameBuf.VFrame.mOffsetRight;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = pFrmbuf->mFrameBuf.VFrame.mOffsetBottom;
        }

        rFrmBuf.mFrameBuf.VFrame.mpts = pFrmbuf->mFrameBuf.VFrame.mpts;
        rFrmBuf.mColorSpace = pFrmbuf->mColorSpace;
        if(rFrmBuf.mRefCnt != 0)
        {
            aloge("fatal error! refCnt[%d]!=0, check code!", rFrmBuf.mRefCnt);
            rFrmBuf.mRefCnt = 0;
        }
        rFrmBuf.mRotation = nRotation;
    }
    if(mB4G2dUserRegionFlag == true)
    {   
        if(PreviewRote::PreviewRoteROT_90 == nRotationValue || PreviewRote::PreviewRoteROT_270 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = 0;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = 0;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = mB4G2dUserRegion_Ro.Height;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = mB4G2dUserRegion_Ro.Width;
        }
        else if(PreviewRote::PreviewRoteROT_180 == nRotationValue || PreviewRote::PreviewRoteROT_0 == nRotationValue)
        {
            rFrmBuf.mFrameBuf.VFrame.mOffsetLeft = 0;
            rFrmBuf.mFrameBuf.VFrame.mOffsetTop = 0;
            rFrmBuf.mFrameBuf.VFrame.mOffsetRight = mB4G2dUserRegion_Ro.Width;
            rFrmBuf.mFrameBuf.VFrame.mOffsetBottom = mB4G2dUserRegion_Ro.Height;
        }
    }

    //use g2d to rotate vi_frame to preview_frame.
    status_t rotRet = rotateFrame(&pFrmbuf->mFrameBuf, &rFrmBuf.mFrameBuf, rFrmBuf.mRotation);
    if(rotRet == NO_ERROR)
    {
        //put to displayList
        Mutex::Autolock lock(mBufferLock);
        mReadyFrameBufferList.splice(mReadyFrameBufferList.end(), mUsingFrameBufferList, mUsingFrameBufferList.begin());
        ret = NO_ERROR;
    }
    else
    {
        //put to idle list
        Mutex::Autolock lock(mBufferLock);
        mIdleFrameBufferList.splice(mIdleFrameBufferList.end(), mUsingFrameBufferList, mUsingFrameBufferList.begin());
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

void PreviewWindow::onNextFrameAvailable(const void* frame)
{
    AutoMutex locker(mLock);
    VIDEO_FRAME_BUFFER_S *pPrevbuf = (VIDEO_FRAME_BUFFER_S *)frame;
    struct src_info src;
    libhwclayerpara_t pic;

    if (!mbPreviewEnabled || true==mbPreviewPause || mHlay < 0) 
    {
        //alogd("cannot start csi render!PrevewEnable[%d]mHlay[%d]", mbPreviewEnabled, mHlay);
        return;
    }

#ifdef DEBUG_PREVWINDOW_SAVE_PICTURE
    if (++mDebugSaveData == 50) {
        char name[256];
        snprintf(name, 256, "/home/yuvdata_%p.dat", this);
        int fd = open(name, O_RDWR | O_CREAT, 0666);
        if (fd < 0) {
            aloge("open file /home/yuvdata.dat failed(%s)!", strerror(errno));
        } else {
            alogd("u32Stride[0]=%d, u32Stride[1]=%d, u32Stride[2]=%d",
                pPrevbuf->mFrameBuf.VFrame.mStride[0],
                pPrevbuf->mFrameBuf.VFrame.mStride[1],
                pPrevbuf->mFrameBuf.VFrame.mStride[2]);
            alogd("pVirAddr[0]=%p, pVirAddr[1]=%p, pVirAddr[2]=%p",
                pPrevbuf->mFrameBuf.VFrame.mpVirAddr[0],
                pPrevbuf->mFrameBuf.VFrame.mpVirAddr[1],
                pPrevbuf->mFrameBuf.VFrame.mpVirAddr[2]);
            alogd("u32PhyAddr[0]=0x%08x, u32PhyAddr[1]=0x%08x, u32PhyAddr[2]=0x%08x",
                pPrevbuf->mFrameBuf.VFrame.mPhyAddr[0],
                pPrevbuf->mFrameBuf.VFrame.mPhyAddr[1],
                pPrevbuf->mFrameBuf.VFrame.mPhyAddr[2]);
            if (pPrevbuf->mFrameBuf.VFrame.mStride[0] > 0) {
                write(fd, pPrevbuf->mFrameBuf.VFrame.mpVirAddr[0], pPrevbuf->mFrameBuf.VFrame.mStride[0]);
            }
            if (pPrevbuf->mFrameBuf.VFrame.mStride[1] > 0) {
                write(fd, pPrevbuf->mFrameBuf.VFrame.mpVirAddr[1], pPrevbuf->mFrameBuf.VFrame.mStride[1]);
            }
            if (pPrevbuf->mFrameBuf.VFrame.mStride[2] > 0) {
                write(fd, pPrevbuf->mFrameBuf.VFrame.mpVirAddr[2], pPrevbuf->mFrameBuf.VFrame.mStride[2]);
            }
            alogd("crop[%d,%d,%d,%d], frame size[%d,%d]", pPrevbuf->mCrop.X, pPrevbuf->mCrop.Y,
                pPrevbuf->mCrop.Width, pPrevbuf->mCrop.Height, pPrevbuf->mFrameBuf.VFrame.mWidth, pPrevbuf->mFrameBuf.VFrame.mHeight);
        }
        close(fd);
    }
#endif
    /*
    memset(&src, 0, sizeof(struct src_info));

    src.w = pPrevbuf->mFrameBuf.VFrame.mWidth;
    src.h = pPrevbuf->mFrameBuf.VFrame.mHeight;
    src.crop_x = pPrevbuf->mFrameBuf.VFrame.mOffsetLeft;
    src.crop_y = pPrevbuf->mFrameBuf.VFrame.mOffsetTop;
    src.crop_w = pPrevbuf->mFrameBuf.VFrame.mOffsetRight - pPrevbuf->mFrameBuf.VFrame.mOffsetLeft;
    src.crop_h = pPrevbuf->mFrameBuf.VFrame.mOffsetBottom - pPrevbuf->mFrameBuf.VFrame.mOffsetTop;
    pic.top_y = (unsigned long)pPrevbuf->mFrameBuf.VFrame.mPhyAddr[0];
    pic.top_c = (unsigned long)pPrevbuf->mFrameBuf.VFrame.mPhyAddr[1];
    pic.bottom_y = (unsigned long)pPrevbuf->mFrameBuf.VFrame.mPhyAddr[2];

    setSrcFormat(&src, &pic, pPrevbuf, 0);
    hwd_layer_render(mHlay, &pic);
    if(mbWaitFirstFrame)
    {
        mbWaitFirstFrame = false;
        hwd_layer_open(mHlay);
    }
    */

    //decide if display
    if(mDisplayFrameRate > 0)
    {
        if(-1 == mPrevFramePts)
        {
            mPrevFramePts = pPrevbuf->mFrameBuf.VFrame.mpts;
        }
        else
        {
            if(pPrevbuf->mFrameBuf.VFrame.mpts >= (uint64_t)(mPrevFramePts + 1000*1000/mDisplayFrameRate))
            {
                //mPrevFramePts += 1000*1000/mDisplayFrameRate;
                uint64_t totalInterval = pPrevbuf->mFrameBuf.VFrame.mpts - (uint64_t)mPrevFramePts;
                uint64_t frameInterval = 1000*1000/mDisplayFrameRate;
                uint64_t frameNum = totalInterval/frameInterval;
                mPrevFramePts += (int64_t)frameNum*frameInterval;
                //alogd("show pts[%lld]us", pPrevbuf->mFrameBuf.VFrame.mpts);
            }
            else
            {
                //alogd("discard pts[%lld]us", pPrevbuf->mFrameBuf.VFrame.mpts);
                return;
            }
        }
    }
    else
    {
        //alogd("print pts[%lld]us", pPrevbuf->mFrameBuf.VFrame.mpts);
        mPrevFramePts = -1;
    }
    if(mPreviewRotation!=0)
    {
        PrePrepareRotationFrame(pPrevbuf);
        if(NO_ERROR != prepareRotationFrame(pPrevbuf))
        {
            return;
        }
        mBufferLock.lock();
        VIDEO_FRAME_BUFFER_S *pRotateBuf = &mReadyFrameBufferList.front();
        pRotateBuf->mRefCnt++;
        if(pRotateBuf->mRefCnt > 1)
        {
            aloge("fatal error! impossible refCnt[%d], id[0x%x]", pRotateBuf->mRefCnt, pRotateBuf->mFrameBuf.mId);
        }
        mDisplayFrameBufferList.splice(mDisplayFrameBufferList.end(), mReadyFrameBufferList, mReadyFrameBufferList.begin());
//        VIDEO_FRAME_BUFFER_S *pBackRotateBuf = &mDisplayFrameBufferList.back();
//        if(pBackRotateBuf != pRotateBuf)
//        {
//            aloge("fatal error! backBuf[%p] in DisplayList should equal to frontBuf[%p] in ReadList!", pBackRotateBuf, pRotateBuf);
//        }
        mBufferLock.unlock();
        /*set user display region value*/
        //AW_MPI_VO_GetFrameDisplayRegion(mVOLayer, mVOChn, RECT_S * pRect);
        //AW_MPI_VO_SetFrameDisplayRegion(mVOLayer, mVOChn, const RECT_S * pRect);
        PreviewRote nRotationValue = static_cast<PreviewRote>(mPreviewRotation & PREVIEW_ROTATION_MASK);
        if(mUserRegionUpdateFlag == true) //need set display area.
        {
            RECT_S stRotateRegion = mUserRegion_Ro;
            if(false == mB4G2dUserRegionFlag)   //rotate frame buffer is as same in area as source frame.
            {
                if(PreviewRote::PreviewRoteROT_90 == nRotationValue)
                {
                    stRotateRegion.Width = mUserRegion_Ro.Height;
                    stRotateRegion.Height = mUserRegion_Ro.Width;
                    stRotateRegion.X = pPrevbuf->mFrameBuf.VFrame.mHeight - mUserRegion_Ro.Height - mUserRegion_Ro.Y;
                    stRotateRegion.Y = mUserRegion_Ro.X;
                }
                else if(PreviewRote::PreviewRoteROT_180 == nRotationValue)
                {
                    stRotateRegion.Width = mUserRegion_Ro.Width;
                    stRotateRegion.Height = mUserRegion_Ro.Height;
                    stRotateRegion.X = pPrevbuf->mFrameBuf.VFrame.mWidth - mUserRegion_Ro.X - mUserRegion_Ro.Width;
                    stRotateRegion.Y = pPrevbuf->mFrameBuf.VFrame.mHeight - mUserRegion_Ro.Y - mUserRegion_Ro.Height;
                }
                else if(PreviewRote::PreviewRoteROT_270 == nRotationValue)
                {
                    stRotateRegion.Width = mUserRegion_Ro.Height;
                    stRotateRegion.Height = mUserRegion_Ro.Width;
                    stRotateRegion.X = mUserRegion_Ro.Y;
                    stRotateRegion.Y = pPrevbuf->mFrameBuf.VFrame.mWidth - mUserRegion_Ro.Width - mUserRegion_Ro.X;
                }
            }
            else
            {   
                //set display region in source frame buffer, and rotate frame buffer is part of source frame buffer,
                //so need recalculate display region coordinates in rotate frame buffer. e.g.:
                /*      |--------------------------|
                        |source frame              |
                        |  |------------------|    |
                        |  |B4G2dRegion       |    |  
                        |  |   |----------|   |    |
                        |  |   | display  |   |    |
                        |  |   |----------|   |    |
                        |  |                  |    |
                        |  |------------------|    |
                        |                          |
                        |--------------------------|
                */
                if(PreviewRote::PreviewRoteROT_90 == nRotationValue)
                {
                    stRotateRegion.Width = mUserRegion_Ro.Height;
                    stRotateRegion.Height = mUserRegion_Ro.Width;
                    stRotateRegion.X = mUserRegion_Ro.Y - mB4G2dUserRegion_Ro.Y;
                    stRotateRegion.Y = mUserRegion_Ro.X - mB4G2dUserRegion_Ro.X;
                }
                else if(PreviewRote::PreviewRoteROT_180 == nRotationValue)
                {
                    stRotateRegion.Width = mUserRegion_Ro.Width;
                    stRotateRegion.Height = mUserRegion_Ro.Height;
                    stRotateRegion.X = (mB4G2dUserRegion_Ro.X + mB4G2dUserRegion_Ro.Width) - (mUserRegion_Ro.X + mUserRegion_Ro.Width);
                    stRotateRegion.Y = (mB4G2dUserRegion_Ro.Y + mB4G2dUserRegion_Ro.Height) - (mUserRegion_Ro.Y + mUserRegion_Ro.Height);
                }
                else if(PreviewRote::PreviewRoteROT_270 == nRotationValue)
                {
                    stRotateRegion.Width = mUserRegion_Ro.Height;
                    stRotateRegion.Height = mUserRegion_Ro.Width;
                    stRotateRegion.X = mUserRegion_Ro.Y - mB4G2dUserRegion_Ro.Y;
                    stRotateRegion.Y = (mB4G2dUserRegion_Ro.X + mB4G2dUserRegion_Ro.Width) - (mUserRegion_Ro.X + mUserRegion_Ro.Width);
                }
            }
            AW_MPI_VO_SetFrameDisplayRegion(mVOLayer, mVOChn, &stRotateRegion);
            mUserRegionUpdateFlag = false;
        }
        if(SUCCESS == AW_MPI_VO_SendFrame(mVOLayer, mVOChn, &pRotateBuf->mFrameBuf, 0))
        {
            mFrameCnt++;
        }
        else
        {
            alogw("fatal error! videolayer[%d]: why send frame to vo fail? ", mVOLayer);
            mBufferLock.lock();
            VIDEO_FRAME_BUFFER_S *pBackRotateBuf = &mDisplayFrameBufferList.back();
            if(pBackRotateBuf != pRotateBuf)
            {
                aloge("fatal error! backBuf[%p] in DisplayList should equal to frontBuf[%p] in ReadList!", pBackRotateBuf, pRotateBuf);
            }
            pBackRotateBuf->mRefCnt--;
            if(pBackRotateBuf->mRefCnt!=0)
            {
                aloge("fatal error! refCnt[%d]!=0, check code!", pBackRotateBuf->mRefCnt);
            }
            mIdleFrameBufferList.splice(mIdleFrameBufferList.end(), mDisplayFrameBufferList, --mDisplayFrameBufferList.end());
            mBufferLock.unlock();
        }
    }
    else
    {
        mpCameraBufRef->increaseBufRef(pPrevbuf);
        if(mUserRegionUpdateFlag == true)
        {
            AW_MPI_VO_SetFrameDisplayRegion(mVOLayer, mVOChn, &mUserRegion_Ro);
            mUserRegionUpdateFlag = false;
        }
        if(SUCCESS == AW_MPI_VO_SendFrame(mVOLayer, mVOChn, &pPrevbuf->mFrameBuf, 0))
        {
            mFrameCnt++;
        }
        else
        {
            mpCameraBufRef->decreaseBufRef(pPrevbuf->mFrameBuf.mId);
        }
    }
    
}
/*
bool PreviewWindow::setSrcFormat(struct src_info *src, libhwclayerpara_t *pic, void *pbuf, int index)
{
	if (mbPreviewNeedSetSrc) {
        VIDEO_FRAME_BUFFER_S *pPrevbuf = (VIDEO_FRAME_BUFFER_S*)pbuf;

	    alogd("mPreviewNeedSetSrc");
		switch (pPrevbuf->mFrameBuf.VFrame.mPixelFormat)
		{
			case PIXEL_FORMAT_YVU_SEMIPLANAR_420:
				src->format = HWC_FORMAT_YUV420VUC;
				alogd("preview_format:HWC_FORMAT_YUV420VUC");
				break;
			case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
				src->format = HWC_FORMAT_YUV420UVC; 
				alogd("preview_format:HWC_FORMAT_YUV420UVC");
				break;
			case PIXEL_FORMAT_YUV_PLANAR_420:
				src->format = HWC_FORMAT_YUV420PLANAR;
				//pic->bottom_y = pPrevbuf->mFrameBuf.u32PhyAddr[2];
				alogd("preview_format:HWC_FORMAT_YUV420PLANAR");
				break;
			case PIXEL_FORMAT_YUYV_PACKAGE_422:
				src->format = HWC_FORMAT_YUV422PLANAR;
				//pic->bottom_y = pPrevbuf->mFrameBuf.u32PhyAddr[2];
				alogd("preview_format:HWC_FORMAT_YUV422PLANAR");
				break;
			
			default:
				alogd("preview unknown pixel format: %08x", pPrevbuf->mFrameBuf.VFrame.mPixelFormat);
				return false;
		}
        switch (pPrevbuf->mColorSpace)
        {
            case V4L2_COLORSPACE_SMPTE170M:
                src->color_space = DISP_BT601;
                break;
            case V4L2_COLORSPACE_JPEG:
                src->color_space = DISP_YCC;
                break;
            default:
                alogd("unknown color space: %08x", pPrevbuf->mColorSpace);
                src->color_space = DISP_YCC;
                break;
        }
		hwd_layer_set_src(mHlay, src);
		mbPreviewNeedSetSrc = false;
	}
	return true;
}
*/

ERRORTYPE PreviewWindow::VOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    PreviewWindow *pPreviewWindow = (PreviewWindow*)cookie;
    if(MOD_ID_VOU == pChn->mModId)
    {
        if(pChn->mChnId != pPreviewWindow->mVOChn)
        {
            aloge("fatal error! VO chnId[%d]!=[%d]", pChn->mChnId, pPreviewWindow->mVOChn);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                //distinguish internal rotation buffer and vi buffer.
                if((pVideoFrameInfo->mId&PREVIEW_BUFID_PREFIX) != 0)
                {
                    pPreviewWindow->mBufferLock.lock();
                    if(!pPreviewWindow->mDisplayFrameBufferList.empty())
                    {
                        VIDEO_FRAME_BUFFER_S *pRotateBuf = &pPreviewWindow->mDisplayFrameBufferList.front();
                        if(pVideoFrameInfo->mId != pRotateBuf->mFrameBuf.mId)
                        {
                            alogw("Be careful! vo return frameId[%d] != firstDisplayFrameId[%d]", pVideoFrameInfo->mId, pRotateBuf->mFrameBuf.mId);
                        }
                    }
                    else
                    {
                        alogw("fatal error! videoLayer[%d] display frame buffer list is empty.", pPreviewWindow->mVOLayer);
                    }
                    //find rotate frame
                    std::list<VIDEO_FRAME_BUFFER_S>::iterator it = pPreviewWindow->mDisplayFrameBufferList.begin();
                    for( ; it != pPreviewWindow->mDisplayFrameBufferList.end(); ++it)
                    {
                        if(pVideoFrameInfo->mId == it->mFrameBuf.mId)
                        {
                            break;
                        }
                        else
                        {
                            alogw("Be careful! frameId[0x%x] in list is not match vo return frameId[0x%x]", it->mFrameBuf.mId, pVideoFrameInfo->mId);
                        }
                    }
                    
                    if(it != pPreviewWindow->mDisplayFrameBufferList.end())
                    {
                        it->mRefCnt--;
                        if(0 == it->mRefCnt)
                        {
                            pPreviewWindow->mIdleFrameBufferList.splice(pPreviewWindow->mIdleFrameBufferList.end(), pPreviewWindow->mDisplayFrameBufferList, it);
                            pPreviewWindow->mFrameCnt--;
                        }
                        else
                        {
                            aloge("fatal error! frameId[0x%x] refCnt[%d]!=0, check code!", it->mFrameBuf.mId, it->mRefCnt);
                        }
                    }
                    else
                    {
                        aloge("fatal error! not find vo return frameId in display list!");
                    }
                    pPreviewWindow->mBufferLock.unlock();
                }
                else
                {
                    pPreviewWindow->mpCameraBufRef->decreaseBufRef(pVideoFrameInfo->mId);
                    pPreviewWindow->mFrameCnt--;
                }
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("vo report video display size[%dx%d]", pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                alogd("vo report rendering start");
                pPreviewWindow->mpCameraBufRef->NotifyRenderStart();
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! why modId[0x%x]?", pChn->mModId);
        ret = FAILURE;
    }
    return ret;
}

ERRORTYPE PreviewWindow::CLOCKCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    aloge("fatal error! why come here? do nothing...");
    return SUCCESS;
}

}; /* namespace EyeseeLinux */
