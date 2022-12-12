/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : PreviewWindow.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/03
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/
#ifndef __IPCLINUX_PREVIEW_WINDOW_H__
#define __IPCLINUX_PREVIEW_WINDOW_H__

#include <list>

#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"

#include "Errors.h"

#include <utils/Mutex.h>
#include <hwdisplay.h>
#include <mm_comm_clock.h>

#include "CameraBufferReference.h"
//#define DEBUG_PREVWINDOW_SAVE_PICTURE


namespace EyeseeLinux {

class PreviewWindow {
public:
    PreviewWindow(CameraBufferReference *pcbr);
    ~PreviewWindow();

    status_t setPreviewWindow(int hlay);
    status_t changePreviewWindow(int hlay);
    status_t setDispBufferNum(int nDispBufNum);
    status_t startPreview();
    status_t stopPreview();
    status_t pausePreview();
    status_t resumePreview();
    status_t storeDisplayFrame(uint64_t framePts);

    inline bool isPreviewEnabled()
    {
        return mbPreviewEnabled;
    }

//    inline void resetHwdLayerSrc()
//    {
//        mbPreviewNeedSetSrc = true;
//    }
    void setDisplayFrameRate(int fps);
    void setPreviewRotation(int rotation);
    status_t setPreviewRegion(RECT_S region);
    status_t getPreviewRegion(RECT_S * region);
    status_t setB4G2dPreviewRegion(RECT_S region);
    status_t getB4G2dPreviewRegion(RECT_S * region);
    int getPreviewRotation();
    void onNextFrameAvailable(const void* frame);

private:
    status_t rotateFrame(const VIDEO_FRAME_INFO_S *pSrc, VIDEO_FRAME_INFO_S *pDes, int nRotation);
    status_t PrePrepareRotationFrame(VIDEO_FRAME_BUFFER_S *pFrmbuf);
    status_t prepareRotationFrame(VIDEO_FRAME_BUFFER_S *pFrmbuf);
    //bool setSrcFormat(struct src_info *src, libhwclayerpara_t *pic, void *pbuf, int index);
    static ERRORTYPE VOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
    static ERRORTYPE CLOCKCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
    CameraBufferReference *mpCameraBufRef;
    bool mbPreviewEnabled;
    bool mbPreviewPause;    //true: pause. false: not pause, running.
    //bool mbPreviewNeedSetSrc;
    //bool mbWaitFirstFrame;
    int mDisplayFrameRate;  //0 means display every frame. unit: fps
    int64_t mPrevFramePts;  //unit:us
    int mPreviewRotation;   // anti-clock wise of picture, 90, 180, 270
    int mRotationBufIdCounter;
    Mutex mBufferLock;
    std::list<VIDEO_FRAME_BUFFER_S> mIdleFrameBufferList;   //use for rotation buffer.
    std::list<VIDEO_FRAME_BUFFER_S> mUsingFrameBufferList;  //one element commonly.
    std::list<VIDEO_FRAME_BUFFER_S> mReadyFrameBufferList;
    std::list<VIDEO_FRAME_BUFFER_S> mDisplayFrameBufferList;    //sent to vo
    Mutex mLock;
    volatile int mFrameCnt; //frame count which is occupied by mpi_vo. 
    int mG2DHandle;

    int mHlay;
    int mDispBufNum;
    VO_LAYER mVOLayer;
    VO_CHN mVOChn;
    RECT_S mUserRegion_Ro;  //display_area of original frame.
    RECT_S mB4G2dUserRegion_Ro;
    bool mUserRegionUpdateFlag; //used to indicate to update vo display param, after update mpi_vo, clear flag to false.
    bool mB4G2dUserRegionFlag; //used to indicate source frame buffer region, which will be processed by g2d.
    //bool mbFirstFrame;  //true: wait first frame to be shown. false: first frame has been shown.
    //CLOCK_CHN mClockChn;
    //CLOCK_CHN_ATTR_S mClockChnAttr;
#ifdef DEBUG_PREVWINDOW_SAVE_PICTURE
    int mDebugSaveData;
#endif
}; /* class PreviewWindow */

}; /* namespace EyeseeLinux */

#endif /* __IPCLINUX_PREVIEW_WINDOW_H__ */
