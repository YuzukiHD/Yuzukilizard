/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VIDevice.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/01
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/
#ifndef __IPCLINUX_VI_DEVICE_H__
#define __IPCLINUX_VI_DEVICE_H__

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include <vector>

#include <mm_comm_vi.h>
#include <mm_comm_region.h>

#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>

#include <camera.h>
#include <CameraParameters.h>
#include <CameraListener.h>

#include "VIChannel.h"


namespace EyeseeLinux {

enum VIDeviceState {
    /* Object has been constructed. */
    VI_STATE_CONSTRUCTED = 0,
    /* Object was prepare to start. */
    VI_STATE_PREPARED,
    /* VI device has been started. */
    VI_STATE_STARTED,
};

typedef struct VIChannelInfo
{
    int mChnId;
    VIChannel *mpChannel;
} VIChannelInfo;

class VIDevice
{
public:
    VIDevice(int cameraId, CameraInfo *pCameraInfo);
    ~VIDevice();

    status_t openChannel(int chnId, bool bForceRef);
    status_t closeChannel(int chnId);

    status_t setChannelDisplay(int chnId, int hlay);
    status_t changeChannelDisplay(int chnId, int hlay);
    bool previewEnabled(int chnId);
    status_t startRender(int chnId);
    status_t stopRender(int chnId);
    status_t pauseRender(int chnId);
    status_t resumeRender(int chnId);
    status_t storeDisplayFrame(int chnId, uint64_t framePts);

    //status_t setDeviceAttr(VI_DEV_ATTR_S *devAttr);
    //status_t getDeviceAttr(VI_DEV_ATTR_S *devAttr);

    status_t prepareDevice();
    status_t releaseDevice();

    status_t prepareChannel(int chnId);
    status_t releaseChannel(int chnId);

    status_t startDevice();
    status_t stopDevice();

    status_t startChannel(int chnId);
    status_t stopChannel(int chnId);
    status_t getMODParams(int chnId, MOTION_DETECT_ATTR_S *pParamMD);
    status_t setMODParams(int chnId, MOTION_DETECT_ATTR_S pParamMD);
    status_t startMODDetect(int chnId);
    status_t stopMODDetect(int chnId);
    status_t getAdasParams(int chnId, AdasDetectParam *pParamADAS);
    status_t setAdasParams(int chnId, AdasDetectParam pParamADAS);
    status_t getAdasInParams(int chnId, AdasInParam * pParamADAS);
    status_t setAdasInParams(int chnId, AdasInParam pParamADAS);
    status_t startAdasDetect(int chnId);
    status_t stopAdasDetect(int chnId);
    status_t getAdasParams_v2(int chnId, AdasDetectParam_v2 *pParamADAS_v2);
    status_t setAdasParams_v2(int chnId, AdasDetectParam_v2 pParamADAS_v2);
    status_t getAdasInParams_v2(int chnId, AdasInParam_v2 * pParamADAS_v2);
    status_t setAdasInParams_v2(int chnId, AdasInParam_v2 pParamADAS_v2);
    status_t startAdasDetect_v2(int chnId);
    status_t stopAdasDetect_v2(int chnId);
    status_t setPicCapMode(int chnId, uint32_t cap_mode_en);
    status_t setFrmDrpThrForPicCapMode(int chnId,uint32_t frm_cnt);

    status_t setParameters(int chnId, CameraParameters &param);
    status_t getParameters(int chnId, CameraParameters &param);
    status_t setISPParameters(CameraParameters &param);
    status_t getISPParameters(CameraParameters &param);
    status_t getISPDMsg(int *exp, int *exp_line, int *gain, int *lv_idx, int *color_temp, int *rgain, int *bgain, int *grgain, int *gbgain);

    //status_t setOSDRects(int chnId, std::list<OSDRectInfo> &rects);
    //status_t getOSDRects(int chnId, std::list<OSDRectInfo> **ppRects);
    //status_t OSDOnOff(int chnId, bool bOnOff);

    RGN_HANDLE createRegion(const RGN_ATTR_S *pstRegion);
    status_t getRegionAttr(RGN_HANDLE Handle, RGN_ATTR_S *pstRgnAttr);
    status_t setRegionBitmap(RGN_HANDLE Handle, const BITMAP_S *pBitmap);
    status_t attachRegionToChannel(RGN_HANDLE Handle, int chnId, const RGN_CHN_ATTR_S *pstChnAttr);
    status_t detachRegionFromChannel(RGN_HANDLE Handle, int chnId);
    status_t setRegionDisplayAttr(RGN_HANDLE Handle, int chnId, const RGN_CHN_ATTR_S *pstChnAttr);
    status_t getRegionDisplayAttr(RGN_HANDLE Handle, int chnId, RGN_CHN_ATTR_S *pstChnAttr);
    status_t destroyRegion(RGN_HANDLE Handle);

    void releaseRecordingFrame(int chnId, uint32_t index);
    status_t startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId);
    status_t stopRecording(int chnId, int recorderId);
    void setDataListener(int chnId, DataListener *pCb);
    void setNotifyListener(int chnId, NotifyListener *pCb);

    status_t takePicture(int chnId, unsigned int msgType, PictureRegionCallback *pPicReg);
    status_t notifyPictureRelease(int chnId);
    status_t cancelContinuousPicture(int chnId);
    status_t KeepPictureEncoder(int chnId, bool bKeep);
    status_t releasePictureEncoder(int chnId);
    void increaseBufRef(int chnId, VIDEO_FRAME_BUFFER_S *pBuf);
    //void postDataCompleted(int chnId, const void *pData, int size);
    //void postVdaDataCompleted(int chnId, const VDA_DATA_S *pData);

private:
    VIChannel *searchVIChannel(int chnId);
    status_t initIspParameters();
    VIDeviceState mVIDeviceState;

    int mCameraId;  //same to cameraId
    CameraInfo mCameraInfo;
    
    std::vector<VIChannelInfo> mVIChannelVector;
    Mutex mVIChannelVectorLock;
    bool mbRefChannelPrepared;  //ref channel has prepared, it means ISP param is done!
    std::list<RGN_HANDLE> mRgnHandleList;
    Mutex mRgnLock;

    //csi
    ISP_DEV mIspDevId;
    bool mbIspRun;
    //VI_DEV_ATTR_S mDevAttr;
    //VI_CAP_MODE_E mVICapMode;
    CameraParameters mISPParameters;   //for record ISP parameters.

};

}; /* namespace EyeseeLinux */

#endif /* __IPCLINUX_VI_DEVICE_H__ */
