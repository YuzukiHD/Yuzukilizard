/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VIDevice.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/06
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "VIDevice"
#include <utils/plat_defines.h>
#include <utils/plat_log.h>

#include <stdlib.h>
#include <memory.h>
#include <stdbool.h>
#include <mpi_vi.h>
#include <mpi_awb.h>
#include <mpi_isp.h>
#include <mpi_region.h>
#include <type_camera.h>

#include "VIChannel.h"
#include "VIDevice.h"


#define SUPPORT_ISP
#define ILLEGAL_ISP_PARAM (-1024)

using namespace std;
namespace EyeseeLinux {

VIDevice::VIDevice(int cameraId, CameraInfo *pCameraInfo)
    : mVIDeviceState(VI_STATE_CONSTRUCTED)
    , mCameraId(cameraId)
{
    alogd("Construct");
    mCameraInfo = *pCameraInfo;
    vector<ISPGeometry>::iterator it = mCameraInfo.mMPPGeometry.mISPGeometrys.begin();
    mIspDevId = it->mISPDev;
    //memset(&mDevAttr, 0, sizeof(mDevAttr));
    mbIspRun = false;
    mbRefChannelPrepared = false;
}

VIDevice::~VIDevice()
{
    alogd("Destruct");
    ERRORTYPE ret;
    size_t num = mRgnHandleList.size();
    if(num > 0)
    {
        alogw("Be careful! There are [%d]regions need to destroy!", num);
        for(RGN_HANDLE& i : mRgnHandleList)
        {
            ret = AW_MPI_RGN_Destroy(i);
            if(SUCCESS != ret)
            {
                aloge("fatal error! destroy region[%d] fail!", i);
            }
        }
    }
}

VIChannel *VIDevice::searchVIChannel(int chnId)
{
    AutoMutex lock(mVIChannelVectorLock);
    for (vector<VIChannelInfo>::iterator it = mVIChannelVector.begin(); it != mVIChannelVector.end(); ++it) {
        if (it->mChnId == chnId) {
            return it->mpChannel;
        }
    }
    return NULL;
}

status_t VIDevice::setChannelDisplay(int chnId, int hlay)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setPreviewDisplay(hlay);
}

status_t VIDevice::changeChannelDisplay(int chnId, int hlay)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->changePreviewDisplay(hlay);
}

bool VIDevice::previewEnabled(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return false;
    }
    return pChannel->isPreviewEnabled();
}

status_t VIDevice::startRender(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startRender();
}

status_t VIDevice::stopRender(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopRender();
}

status_t VIDevice::pauseRender(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->pauseRender();
}

status_t VIDevice::resumeRender(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->resumeRender();
}

status_t VIDevice::storeDisplayFrame(int chnId, uint64_t framePts)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->storeDisplayFrame(framePts);
}

/*
status_t VIDevice::setDeviceAttr(VI_DEV_ATTR_S *devAttr)
{
    mDevAttr = *devAttr;
    return NO_ERROR;
}

status_t VIDevice::getDeviceAttr(VI_DEV_ATTR_S *devAttr)
{
    *devAttr = mDevAttr;
    return NO_ERROR;
}
*/

status_t VIDevice::setParameters(int chnId, CameraParameters &param)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        alogd("channel %d is not exist!", chnId);
        return NO_INIT;
    }

    return pChannel->setParameters(param);
}

void VIDevice::increaseBufRef(int chnId, VIDEO_FRAME_BUFFER_S *pBuf)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL){
        alogd("channel %d is not exist!", chnId);
        return ;
    }

    return pChannel->increaseBufRef(pBuf);
}

status_t VIDevice::setPicCapMode(int chnId, uint32_t cap_mode_en)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        alogd("channel %d is not exist!", chnId);
        return NO_INIT;
    }

    pChannel->setPicCapMode(cap_mode_en);
    return NO_ERROR;
}

status_t VIDevice::setFrmDrpThrForPicCapMode(int chnId,uint32_t frm_cnt)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        alogd("channel %d is not exist!", chnId);
        return NO_INIT;
    }

    pChannel->setFrmDrpThrForPicCapMode(frm_cnt);
    return NO_ERROR;
} 


status_t VIDevice::getParameters(int chnId, CameraParameters &param)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        alogd("channel %d is not exist!", chnId);
        return NO_INIT;
    }

    return pChannel->getParameters(param);
}

status_t VIDevice::setISPParameters(CameraParameters &param)
{
//    if(mVIDeviceState!=VI_STATE_PREPARED && mVIDeviceState!=VI_STATE_STARTED)
//    {
//        alogw("set isp parameters in wrong state[0x%x]", mVIDeviceState);
//        return INVALID_OPERATION;
//    }
    Mutex::Autolock lock(mVIChannelVectorLock);
    bool bCanSet = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(VIChannel::VI_CHN_STATE_STARTED == i.mpChannel->getState())
        {
            bCanSet = true;
            break;
        }
    }
    if(bCanSet)
    {
      #if (defined(SUPPORT_ISP))

        printf("[FUN]:%s mIspDevId:%d \n", __FUNCTION__,mIspDevId);

        //detect param change, and update.
        //ModuleOnOff
        ISP_MODULE_ONOFF& curModuleOnOff = mISPParameters.getModuleOnOff();
        ISP_MODULE_ONOFF& newModuleOnOff = param.getModuleOnOff();
        if(memcmp(&curModuleOnOff, &newModuleOnOff, sizeof(ISP_MODULE_ONOFF)))
        {
            alogd("change ISP_ModuleOnOff");
            AW_MPI_ISP_SetModuleOnOff(mIspDevId, &newModuleOnOff);
        }
        //AE mode
        int curAEMode = mISPParameters.ChnIspAe_GetMode();
        int newAEMode = param.ChnIspAe_GetMode();
        if(curAEMode!= newAEMode)
        {
            alogd("change AE mode");
            AW_MPI_ISP_AE_SetMode(mIspDevId, newAEMode);
        }
        //AE exposure bias
        int curAEExposureBias = mISPParameters.ChnIspAe_GetExposureBias();
        int newAEExposureBias = param.ChnIspAe_GetExposureBias();
        if(curAEExposureBias != newAEExposureBias)
        {
            alogd("change AE Exposure Bias");
            AW_MPI_ISP_AE_SetExposureBias(mIspDevId, newAEExposureBias);
        }
        //AE exposure
        int curAEExposure = mISPParameters.ChnIspAe_GetExposure();
        int newAEExposure = param.ChnIspAe_GetExposure();
        if(curAEExposure != newAEExposure)
        {
            alogd("change AE exposure");
            AW_MPI_ISP_AE_SetExposure(mIspDevId, newAEExposure);
        }
        //AE gain
        int curAEGain = mISPParameters.ChnIspAe_GetGain();
        int newAEGain = param.ChnIspAe_GetGain();
        if(curAEGain != newAEGain)
        {
            alogd("change AE Gain");
            AW_MPI_ISP_AE_SetGain(mIspDevId, newAEGain);
        }
        //AE ISOSensitive
        int curAEISOSensitive = mISPParameters.ChnIspAe_GetISOSensitive();
        int newAEISOSensitive = param.ChnIspAe_GetISOSensitive();
        if(curAEISOSensitive != newAEISOSensitive)
        {
            alogd("change AE ISOSensitive[%d]->[%d]", curAEISOSensitive, newAEISOSensitive);
            AW_MPI_ISP_AE_SetISOSensitive(mIspDevId, newAEISOSensitive);
        }
        //AE Metering
        int curMetering = mISPParameters.ChnIspAe_GetMetering();
        int newMetering = param.ChnIspAe_GetMetering();
        if(curMetering != newMetering)
        {
            alogd("change AE curMetering[%d]->[%d]", curMetering, newMetering);
            AW_MPI_ISP_AE_SetMetering(mIspDevId, newMetering);
        }

        //AWB Mode(0-1,2-7)
        int curAwbMode = mISPParameters.ChnIspAwb_GetMode();
        int newAwbMode = param.ChnIspAwb_GetMode();
        if(curAwbMode != newAwbMode)
        {
            if (newAwbMode==0 || newAwbMode==1)
            {
                AW_MPI_ISP_AWB_SetMode(mIspDevId, newAwbMode);
            }
            else
            {
                AW_MPI_ISP_AWB_SetColorTemp(mIspDevId, newAwbMode);
            }
        }
        //AWB RGain
        int curAwbRGain= mISPParameters.ChnIspAwb_GetRGain();
        int newAwbRGain = param.ChnIspAwb_GetRGain();
        if(curAwbRGain != newAwbRGain)
        {
            alogd("change AWB RGain");
            AW_MPI_ISP_AWB_SetRGain(mIspDevId, newAwbRGain);
        }
        //AWB GrGain
        int curAwbGrGain= mISPParameters.ChnIspAwb_GetGrGain();
        int newAwbGrGain = param.ChnIspAwb_GetGrGain();
        if(curAwbGrGain != newAwbGrGain)
        {
            alogd("change AWB GrGain");
            AW_MPI_ISP_AWB_SetGrGain(mIspDevId, newAwbGrGain);
        }
    //AWB GbGain
        int curAwbGbGain= mISPParameters.ChnIspAwb_GetGbGain();
        int newAwbGbGain = param.ChnIspAwb_GetGbGain();
        if(curAwbGbGain != newAwbGbGain)
        {
            alogd("change AWB GbGain");
            AW_MPI_ISP_AWB_SetGbGain(mIspDevId, newAwbGbGain);
        }
        //AWB BGain
        int curAwbBGain= mISPParameters.ChnIspAwb_GetBGain();
        int newAwbBGain = param.ChnIspAwb_GetBGain();
        if(curAwbBGain != newAwbBGain)
        {
            alogd("change AWB BGain");
            AW_MPI_ISP_AWB_SetBGain(mIspDevId, newAwbBGain);
        }
        //Flicker
        int curFlicker = mISPParameters.ChnIsp_GetFlicker();
        int newFlicker = param.ChnIsp_GetFlicker();
        if(curFlicker != newFlicker)
        {
            alogd("change flicker");
            AW_MPI_ISP_SetFlicker(mIspDevId, newFlicker);
        }
        //brightness
        int curBrightness = mISPParameters.ChnIsp_GetBrightness();
        int newBrightness = param.ChnIsp_GetBrightness();
        if(curBrightness != newBrightness)
        {
            alogd("change brightness");
            AW_MPI_ISP_SetBrightness(mIspDevId, newBrightness);
        }
        //contrast
        int curContrast = mISPParameters.ChnIsp_GetContrast();
        int newContrast = param.ChnIsp_GetContrast();
        if(curContrast != newContrast)
        {
            alogd("change contrast");
            AW_MPI_ISP_SetContrast(mIspDevId, newContrast);
        }
        //saturation
        int curSaturation = mISPParameters.ChnIsp_GetSaturation();
        int newSaturation = param.ChnIsp_GetSaturation();
        if(curSaturation != newSaturation)
        {
            alogd("change saturation");
            AW_MPI_ISP_SetSaturation(mIspDevId, newSaturation);
        }
        //sharpness
        int curSharpness = mISPParameters.ChnIsp_GetSharpness();
        int newSharpness = param.ChnIsp_GetSharpness();
        if(curSharpness != newSharpness)
        {
            alogd("change sharpness");
            AW_MPI_ISP_SetSharpness(mIspDevId, newSharpness);
        }
        //hue
        /*int curHue = mISPParameters.ChnIsp_GetHue();
        int newHue = param.ChnIsp_GetHue();
        if(curHue != newHue)
        {
            alogd("change hue");
            AW_MPI_ISP_SetHue(mIspDevId, newHue);
        }*/

        //INI ISP config
        int curNRAttr = mISPParameters.getNRAttrValue();
        int newNRAttr = param.getNRAttrValue();
        if(curNRAttr != newNRAttr)
        {
            alogd("change ISP_NR_ATTR");
            AW_MPI_ISP_SetNRAttr(mIspDevId, newNRAttr);
        }
        int cur3NRAttr = mISPParameters.get3NRAttrValue();
        int new3NRAttr = param.get3NRAttrValue();
        if(cur3NRAttr != new3NRAttr)
        {
            alogd("change ISP_3NR_ATTR");
            AW_MPI_ISP_Set3NRAttr(mIspDevId, new3NRAttr);
        }

        //WDR
        int curPltmWDR = mISPParameters.getPltmWDR();
        int newPltmWDR = param.getPltmWDR();
        if(curPltmWDR != newPltmWDR)
        {
            alogd("change ISP_PltmWDR");
            AW_MPI_ISP_SetPltmWDR(mIspDevId, newPltmWDR);
        }

      #endif
    }
    mISPParameters = param;
    return NO_ERROR;
}

status_t VIDevice::getISPParameters(CameraParameters &param)
{
    param = mISPParameters;
    return NO_ERROR;
}
/*
status_t VIDevice::setOSDRects(int chnId, std::list<OSDRectInfo> &rects)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("fatal error! VIPP channel[%d] is not exist!", chnId);
        return UNKNOWN_ERROR;
    }
    return pChannel->setOSDRects(rects);
}

status_t VIDevice::getOSDRects(int chnId, std::list<OSDRectInfo> **ppRects)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("fatal error! VIPP channel[%d] is not exist!", chnId);
        return UNKNOWN_ERROR;
    }
    return pChannel->getOSDRects(ppRects);
}

status_t VIDevice::OSDOnOff(int chnId, bool bOnOff)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("fatal error! VIPP channel[%d] is not exist!", chnId);
        return UNKNOWN_ERROR;
    }
    return pChannel->OSDOnOff(bOnOff);
}
*/
RGN_HANDLE VIDevice::createRegion(const RGN_ATTR_S *pstRegion)
{
    Mutex::Autolock lock(mRgnLock);
    ERRORTYPE ret;
    bool bSuccess = false;
    RGN_HANDLE handle = 0;
    while(handle < RGN_HANDLE_MAX)
    {
        ret = AW_MPI_RGN_Create(handle, pstRegion);
        if(SUCCESS == ret)
        {
            bSuccess = true;
            alogd("create region[%d] success!", handle);
            break;
        }
        else if(ERR_RGN_EXIST == ret)
        {
            alogv("region[%d] is exist, find next!", handle);
            handle++;
        }
        else
        {
            aloge("fatal error! create region[%d] ret[0x%x]!", handle, ret);
            break;
        }
    }
    if(bSuccess)
    {
        mRgnHandleList.push_back(handle);
        return handle;
    }
    else
    {
        alogd("create region fail");
        return MM_INVALID_HANDLE;
    }
}

status_t VIDevice::getRegionAttr(RGN_HANDLE Handle, RGN_ATTR_S *pstRgnAttr)
{
    Mutex::Autolock lock(mRgnLock);
    status_t result = NO_ERROR;
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        ERRORTYPE ret = AW_MPI_RGN_GetAttr(Handle, pstRgnAttr);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        result = UNKNOWN_ERROR;
    }
    return result;
}

status_t VIDevice::setRegionBitmap(RGN_HANDLE Handle, const BITMAP_S *pBitmap)
{
    Mutex::Autolock lock(mRgnLock);
    status_t result = NO_ERROR;
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        ERRORTYPE ret = AW_MPI_RGN_SetBitMap(Handle, pBitmap);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        result = UNKNOWN_ERROR;
    }
    return result;
}

status_t VIDevice::attachRegionToChannel(RGN_HANDLE Handle, int chnId, const RGN_CHN_ATTR_S *pstChnAttr)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mVIChannelVectorLock);
    VIChannel::VIChannelState chnState;
    bool bChannelExist = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(i.mChnId == chnId)
        {
            chnState = i.mpChannel->getState();
            bChannelExist = true;
            break;
        }
    }
    if(false == bChannelExist)
    {
        aloge("fatal error! wrong camera vipp[%d]", chnId);
        return UNKNOWN_ERROR;
    }
    if(chnState != VIChannel::VI_CHN_STATE_STARTED)
    {
        aloge("fatal error! vipp state[%d] is not started, can't attach region!", chnState);
        return UNKNOWN_ERROR;
    }
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        MPP_CHN_S stChn = {MOD_ID_VIU, chnId, 0};
        ERRORTYPE ret = AW_MPI_RGN_AttachToChn(Handle, &stChn, pstChnAttr);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}
status_t VIDevice::detachRegionFromChannel(RGN_HANDLE Handle, int chnId)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mVIChannelVectorLock);
    bool bChannelExist = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(i.mChnId == chnId)
        {
            bChannelExist = true;
            break;
        }
    }
    if(false == bChannelExist)
    {
        aloge("fatal error! wrong camera vipp[%d]", chnId);
        return UNKNOWN_ERROR;
    }
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        MPP_CHN_S stChn = {MOD_ID_VIU, chnId, 0};
        ERRORTYPE ret = AW_MPI_RGN_DetachFromChn(Handle, &stChn);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}
status_t VIDevice::setRegionDisplayAttr(RGN_HANDLE Handle, int chnId, const RGN_CHN_ATTR_S *pstChnAttr)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mVIChannelVectorLock);
    bool bChannelExist = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(i.mChnId == chnId)
        {
            bChannelExist = true;
            break;
        }
    }
    if(false == bChannelExist)
    {
        aloge("fatal error! wrong camera vipp[%d]", chnId);
        return UNKNOWN_ERROR;
    }
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        MPP_CHN_S stChn = {MOD_ID_VIU, chnId, 0};
        ERRORTYPE ret = AW_MPI_RGN_SetDisplayAttr(Handle, &stChn, pstChnAttr);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}
status_t VIDevice::getRegionDisplayAttr(RGN_HANDLE Handle, int chnId, RGN_CHN_ATTR_S *pstChnAttr)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mVIChannelVectorLock);
    bool bChannelExist = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(i.mChnId == chnId)
        {
            bChannelExist = true;
            break;
        }
    }
    if(false == bChannelExist)
    {
        aloge("fatal error! wrong camera vipp[%d]", chnId);
        return UNKNOWN_ERROR;
    }
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        MPP_CHN_S stChn = {MOD_ID_VIU, chnId, 0};
        ERRORTYPE ret = AW_MPI_RGN_GetDisplayAttr(Handle, &stChn, pstChnAttr);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}
status_t VIDevice::destroyRegion(RGN_HANDLE Handle)
{
    status_t result = NO_ERROR;
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    std::list<RGN_HANDLE>::iterator it;
    for(it = mRgnHandleList.begin(); it != mRgnHandleList.end(); ++it)
    {
        if(*it == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        ERRORTYPE ret = AW_MPI_RGN_Destroy(Handle);
        if(SUCCESS == ret)
        {
            mRgnHandleList.erase(it);
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}

status_t VIDevice::initIspParameters()
{
#if 0
    //brightness
    int defaultBrightness;
    AW_MPI_ISP_GetBrightness(mIspDevId, &defaultBrightness);
    //contrast
    int defaultContrast;
    AW_MPI_ISP_GetContrast(mIspDevId, &defaultContrast);
    //saturation
    int defaultSaturation;
    AW_MPI_ISP_GetSaturation(mIspDevId, &defaultSaturation);
    //hue
//    int defaultHue;
//    AW_MPI_ISP_GetHue(mIspDevId, &defaultHue);
    //AE mode
    int defaultAEMode;
    AW_MPI_ISP_AE_GetMode(mIspDevId, &defaultAEMode);
    //AE exposure bias
    int defaultAEExposureBias;
    AW_MPI_ISP_AE_GetExposureBias(mIspDevId, &defaultAEExposureBias);
    //AE exposure
    int defaultAEExposure;
    AW_MPI_ISP_AE_GetExposure(mIspDevId, &defaultAEExposure);
    //AE gain
    int defaultAEGain;
    AW_MPI_ISP_AE_GetGain(mIspDevId, &defaultAEGain);
    //AE ISOSensitive
    int defaultAEISOSensitive = -1;
    AW_MPI_ISP_AE_GetISOSensitive(mIspDevId, &defaultAEISOSensitive);
    //AE Metering
    int defaultAEMetering = -1;
    AW_MPI_ISP_AE_GetMetering(mIspDevId, &defaultAEMetering);

    //AWB mode
    int defaultAwbMode;
    AW_MPI_ISP_AWB_GetMode(mIspDevId, &defaultAwbMode);
    //AWB color temp
    int defaultColorTemp;
    AW_MPI_ISP_AWB_GetColorTemp(mIspDevId, &defaultColorTemp);
    //AWB color RGain
    int defaultColorRGain = -1;
    AW_MPI_ISP_AWB_GetRGain(mIspDevId, &defaultColorRGain);
    //AWB color BGain
    int defaultColorBGain = -1;
    AW_MPI_ISP_AWB_GetBGain(mIspDevId, &defaultColorBGain);
    //flicker
    int defaultFlicker = -1;
    AW_MPI_ISP_GetFlicker(mIspDevId, &defaultFlicker);

    //sharpness
    int defaultSharpness = -1;
    AW_MPI_ISP_GetSharpness(mIspDevId, &defaultSharpness);
    //detailed AWB param setting
    //ISP_WB_ATTR_S defaultWBAttr;
    //AW_MPI_ISP_AWB_GetWBAttr(mIspDevId, &defaultWBAttr);
//    ISP_COLORMATRIX_ATTR_S defaultCMAttr;
//    AW_MPI_ISP_AWB_GetCCMAttr(mIspDevId, 0, &defaultCMAttr);
//    ISP_AWB_SPEED_S defaultWBSpeed;
//    AW_MPI_ISP_AWB_GetSpeed(mIspDevId, &defaultWBSpeed);
//    ISP_AWB_TEMP_RANGE_S defaultWBTempRange;
//    AW_MPI_ISP_AWB_GetTempRange(mIspDevId, &defaultWBTempRange);
//    std::map<int, ISP_AWB_TEMP_INFO_S> defaultWBLights;
//    int nLightMode;
//    ISP_AWB_TEMP_INFO_S defaultWBTempInfo;
//    nLightMode = 0x01;
//    AW_MPI_ISP_AWB_GetLight(mIspDevId, nLightMode, &defaultWBTempInfo);
//    defaultWBLights[nLightMode] = defaultWBTempInfo;
//    nLightMode = 0x02;
//    AW_MPI_ISP_AWB_GetLight(mIspDevId, nLightMode, &defaultWBTempInfo);
//    defaultWBLights[nLightMode] = defaultWBTempInfo;
//    nLightMode = 0x03;
//    AW_MPI_ISP_AWB_GetLight(mIspDevId, nLightMode, &defaultWBTempInfo);
//    defaultWBLights[nLightMode] = defaultWBTempInfo;
//    nLightMode = 0x04;
//    AW_MPI_ISP_AWB_GetLight(mIspDevId, nLightMode, &defaultWBTempInfo);
//    defaultWBLights[nLightMode] = defaultWBTempInfo;
//    ISP_AWB_FAVOR_S defaultWBFavor;
//    AW_MPI_ISP_AWB_GetFavor(mIspDevId, &defaultWBFavor);

    mISPParameters.ChnIspAe_SetMode(defaultAEMode);
    mISPParameters.ChnIspAe_SetExposureBias(defaultAEExposureBias);
    mISPParameters.ChnIspAe_SetExposure(defaultAEExposure);
    mISPParameters.ChnIspAe_SetGain(defaultAEGain);
    mISPParameters.ChnIspAe_SetISOSensitive(defaultAEISOSensitive);
    mISPParameters.ChnIspAe_SetMetering(defaultAEMetering);
    mISPParameters.ChnIspAwb_SetMode(defaultAwbMode);
    mISPParameters.ChnIspAwb_SetColorTemp(defaultColorTemp);
    mISPParameters.ChnIsp_SetFlicker(defaultFlicker);
    mISPParameters.ChnIsp_SetBrightness(defaultBrightness);
    mISPParameters.ChnIsp_SetContrast(defaultContrast);
    mISPParameters.ChnIsp_SetSaturation(defaultSaturation);
    mISPParameters.ChnIsp_SetSharpness(defaultSharpness);
    //mISPParameters.ChnIsp_SetHue(defaultHue);
#else
    mISPParameters.ChnIspAe_SetMode(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIspAe_SetExposureBias(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIspAe_SetExposure(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIspAe_SetGain(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIspAe_SetISOSensitive(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIspAe_SetMetering(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIspAwb_SetMode(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIspAwb_SetColorTemp(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIsp_SetFlicker(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIsp_SetBrightness(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIsp_SetContrast(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIsp_SetSaturation(ILLEGAL_ISP_PARAM);
    mISPParameters.ChnIsp_SetSharpness(ILLEGAL_ISP_PARAM);
    //mISPParameters.ChnIsp_SetHue(ILLEGAL_ISP_PARAM); 

    //INI ISP config
    ISP_MODULE_ONOFF defaultModuleOnOff;
    AW_MPI_ISP_GetModuleOnOff(mIspDevId, &defaultModuleOnOff);
    int defaultNRAttrValue;
    AW_MPI_ISP_GetNRAttr(mIspDevId, &defaultNRAttrValue);
    int default3NRAttrValue;
    AW_MPI_ISP_Get3NRAttr(mIspDevId, &default3NRAttrValue);
    int defaultPltmWDR;
    AW_MPI_ISP_GetPltmWDR(mIspDevId, &defaultPltmWDR);

    mISPParameters.setModuleOnOff(defaultModuleOnOff);
    mISPParameters.setNRAttrValue(defaultNRAttrValue);
    mISPParameters.set3NRAttrValue(default3NRAttrValue);
    mISPParameters.setPltmWDR(defaultPltmWDR);

//    mISPParameters.setAWB_WBAttrValue(defaultWBAttr);
//    mISPParameters.setAWB_CCMAttrValue(defaultCMAttr);
//    mISPParameters.setAWB_SpeedValue(defaultWBSpeed);
//    mISPParameters.setAWB_TempRangeValue(defaultWBTempRange);
//    mISPParameters.setAWB_LightValues(defaultWBLights);
//    mISPParameters.setAWB_FavorValue(defaultWBFavor);
//    mISPParameters.setFlickerValue(defaultFlicker);   
#endif
    return NO_ERROR;
}
status_t VIDevice::prepareDevice()
{
    int ret;

    if (mVIDeviceState != VI_STATE_CONSTRUCTED)
    {
        aloge("prepareDevice in error state %d", mVIDeviceState);
        return INVALID_OPERATION;
    }
    if(CameraInfo::CAMERA_CSI == mCameraInfo.mCameraDeviceType)
    {
    }
    else if(CameraInfo::CAMERA_USB == mCameraInfo.mCameraDeviceType)
    {
        aloge("need implement");
    }
    else
    {
        aloge("unsupported temporary");
    }
    mVIDeviceState = VI_STATE_PREPARED;
    return NO_ERROR;
}

status_t VIDevice::releaseDevice()
{
    if (mVIDeviceState != VI_STATE_PREPARED)
    {
        aloge("releaseDevice in error state %d", mVIDeviceState);
        return INVALID_OPERATION;
    }
    if(CameraInfo::CAMERA_CSI == mCameraInfo.mCameraDeviceType)
    {
    }
    else if(CameraInfo::CAMERA_USB == mCameraInfo.mCameraDeviceType)
    {
        aloge("need implement");
    }
    else
    {
        aloge("unsupported temporary");
    }
    mVIDeviceState = VI_STATE_CONSTRUCTED;
    return NO_ERROR;
}

status_t VIDevice::startDevice()
{
    if (mVIDeviceState != VI_STATE_PREPARED)
    {
        aloge("startDevice in error state %d", mVIDeviceState);
        return INVALID_OPERATION;
    }
    if(CameraInfo::CAMERA_CSI == mCameraInfo.mCameraDeviceType)
    {
    }
    else if(CameraInfo::CAMERA_USB == mCameraInfo.mCameraDeviceType)
    {
        aloge("need implement");
    }
    else
    {
        aloge("unsupported temporary");
    }

    mVIDeviceState = VI_STATE_STARTED;
    return NO_ERROR;
}

status_t VIDevice::stopDevice()
{
    status_t eError = NO_ERROR;
    if (mVIDeviceState != VI_STATE_STARTED)
    {
        aloge("stopDevice in error state %d", mVIDeviceState);
        return INVALID_OPERATION;
    }
    int nChnNum = 0;
    AutoMutex lock(mVIChannelVectorLock);
    for (vector<VIChannelInfo>::iterator it = mVIChannelVector.begin(); it != mVIChannelVector.end(); ++it)
    {
        aloge("fatal error! camera[%d] vipp[%d] is exist when stop device!", mCameraId, it->mChnId);
        nChnNum++;
    }
    if(0 == nChnNum)
    {
        mVIDeviceState = VI_STATE_PREPARED;
        eError = NO_ERROR;
    }
    else
    {
        eError = INVALID_OPERATION;
    }
    return eError;
}

status_t VIDevice::openChannel(int chnId, bool bForceRef)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel != NULL)
    {
        aloge("channel %d is opened!", chnId);
        return INVALID_OPERATION;
    }

    AutoMutex lock(mVIChannelVectorLock);
    if(bForceRef)
    {
        if(!mVIChannelVector.empty())
        {
            aloge("fatal error! Some channels are running. ISP sensor param has done. can't open current channel as reference channel");
            return BAD_VALUE;
        }
    }
    else
    {
        if(mVIChannelVector.empty())
        {
            aloge("fatal error! first channel must be ref channel! can't open current channel as sub channel!");
            return BAD_VALUE;
        }
    }
    VIChannelInfo chnInfo;
    chnInfo.mpChannel = new VIChannel(mIspDevId, chnId, bForceRef);
    chnInfo.mChnId = chnId;
    mVIChannelVector.push_back(chnInfo);
    return NO_ERROR;
}

status_t VIDevice::closeChannel(int chnId)
{
    vector<VIChannelInfo>::iterator it;
    bool found = false;

    {
        AutoMutex lock(mVIChannelVectorLock);
        for (it = mVIChannelVector.begin(); it != mVIChannelVector.end(); ++it)
        {
            if (it->mChnId == chnId) {
                delete it->mpChannel;
                mVIChannelVector.erase(it);
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        aloge("channel %d is not exist!", chnId);
        return INVALID_OPERATION;
    }

    //delete it->mpChannel;

    return NO_ERROR;
}
status_t VIDevice::prepareChannel(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    if(false == mbRefChannelPrepared)
    {
        if(false == pChannel->mbForceRef)
        {
            aloge("fatal error! must prepare reference channel first!");
            return INVALID_OPERATION;
        }
    }
    status_t ret = pChannel->prepare();
    if (ret != NO_ERROR)
    {
        aloge("prepare channel error!");
        return ret;
    }
    if(false == mbRefChannelPrepared)
    {
        mbRefChannelPrepared = true;
    }
    return NO_ERROR;
}

status_t VIDevice::releaseChannel(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    AutoMutex lock(mVIChannelVectorLock);
    status_t ret = pChannel->release();
    if(NO_ERROR == ret)
    {
        bool bAllRelease = true;
        for(VIChannelInfo& i : mVIChannelVector)
        {
            if(i.mpChannel->getState() != VIChannel::VI_CHN_STATE_CONSTRUCTED)
            {
                bAllRelease = false;
                break;
            }
        }
        if(bAllRelease)
        {
            mbRefChannelPrepared = false;
        }
    }
    return ret;
}

status_t VIDevice::startChannel(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
#if (defined(SUPPORT_ISP))
    if(!mbIspRun)
    {
        mbIspRun = true;
        AW_MPI_ISP_Run(mIspDevId);
        alogd("ISP[%d] run", mIspDevId);
        #if 0
        ISP_MODULE_ONOFF ModuleOnOff = mISPParameters.getModuleOnOff();
        if(ModuleOnOff.pltm != -1)
        {
            alogv("set ISP_ModuleOnOff");
            AW_MPI_ISP_SetModuleOnOff(mIspDevId, &ModuleOnOff);
        }
        //set dynamic isp parameters
        int curChnIspAe_Mode = mISPParameters.ChnIspAe_GetMode();
        if(curChnIspAe_Mode != -1)
        {
            alogv("set chnIspAe_Mode[%d]", curChnIspAe_Mode);
            AW_MPI_ISP_AE_SetMode(mIspDevId, curChnIspAe_Mode);
        }
        int curChnIspAe_ExposureBias = mISPParameters.ChnIspAe_GetExposureBias();
        if(curChnIspAe_ExposureBias != -1)
        {
            alogv("set chnIspAe_ExposureBias[%d]]", curChnIspAe_ExposureBias);
            AW_MPI_ISP_AE_SetExposureBias(mIspDevId, curChnIspAe_ExposureBias);
        }
        int curChnIspAe_Exposure = mISPParameters.ChnIspAe_GetExposure();
        if(curChnIspAe_Exposure != -1)
        {
            alogv("set chnIspAe_Exposure[%d]", curChnIspAe_Exposure);
            AW_MPI_ISP_AE_SetExposure(mIspDevId, curChnIspAe_Exposure);
        }
        int curChnIspAe_Gain = mISPParameters.ChnIspAe_GetGain();
        if(curChnIspAe_Gain != -1)
        {
            alogv("set chnIspAe_Gain[%d]", curChnIspAe_Gain);
            AW_MPI_ISP_AE_SetGain(mIspDevId, curChnIspAe_Gain);
        }
        int curChnIspAwb_Mode = mISPParameters.ChnIspAwb_GetMode();
        if(curChnIspAwb_Mode != -1)
        {
            alogv("set chnIspAwb_Mode[%d]", curChnIspAwb_Mode);
            AW_MPI_ISP_AWB_SetMode(mIspDevId, curChnIspAwb_Mode);
        }
        int curChnIspAwb_RGain = mISPParameters.ChnIspAwb_GetRGain();
        if(curChnIspAwb_RGain != -1)
        {
            alogv("set chnIspAwb_RGain[%d]", curChnIspAwb_RGain);
            AW_MPI_ISP_AWB_SetRGain(mIspDevId, curChnIspAwb_RGain);
        }
        int curChnIspAwb_BGain = mISPParameters.ChnIspAwb_GetBGain();
        if(curChnIspAwb_BGain != -1)
        {
            alogv("set chnIspAwb_BGain[%d]", curChnIspAwb_BGain);
            AW_MPI_ISP_AWB_SetBGain(mIspDevId, curChnIspAwb_BGain);
        }
        int curChnIsp_Flicker = mISPParameters.ChnIsp_GetFlicker();
        if(curChnIsp_Flicker != -1)
        {
            alogv("set chnIsp_Flicker[%d]", curChnIsp_Flicker);
            AW_MPI_ISP_SetFlicker(mIspDevId, curChnIsp_Flicker);
        }
        int curChnIsp_Brightness = mISPParameters.ChnIsp_GetBrightness();
        if(curChnIsp_Brightness != -1)
        {
            alogv("set chnIsp_Brightness[%d]", curChnIsp_Brightness);
            AW_MPI_ISP_SetBrightness(mIspDevId, curChnIsp_Brightness);
        }
        int curChnIsp_Contrast = mISPParameters.ChnIsp_GetContrast();
        if(curChnIsp_Contrast != -1)
        {
            alogv("set chnIsp_Contrast[%d]", curChnIsp_Contrast);
            AW_MPI_ISP_SetContrast(mIspDevId, curChnIsp_Contrast);
        }
        int curChnIsp_Saturation = mISPParameters.ChnIsp_GetSaturation();
        if(curChnIsp_Saturation != -1)
        {
            alogv("set chnIsp_Saturation[%d]", curChnIsp_Saturation);
            AW_MPI_ISP_SetSaturation(mIspDevId, curChnIsp_Saturation);
        }
        int curChnIsp_Sharpness = mISPParameters.ChnIsp_GetSharpness();
        if(curChnIsp_Sharpness != -1)
        {
            alogv("set chnIsp_Sharpness[%d]", curChnIsp_Sharpness);
            AW_MPI_ISP_SetSharpness(mIspDevId, curChnIsp_Sharpness);
        }
        /*int curChnIsp_Hue = mISPParameters.ChnIsp_GetHue();
        if(curChnIsp_Hue != -1)
        {
            alogd("set chnIsp_Hue[%d]", curChnIsp_Hue);
            AW_MPI_ISP_SetHue(mIspDevId, curChnIsp_Hue);
        }*/

        //INI ISP config
        int stNRAttrValue = mISPParameters.getNRAttrValue();
        if(stNRAttrValue != -1)
        {
            alogv("set ISP_NR_ATTR");
            AW_MPI_ISP_SetNRAttr(mIspDevId, stNRAttrValue);
        }
        int st3NRAttrValue = mISPParameters.get3NRAttrValue();
        if(st3NRAttrValue != -1)
        {
            alogv("set ISP_3NR_ATTR");
            AW_MPI_ISP_Set3NRAttr(mIspDevId, st3NRAttrValue);
        }

        int stPltmWDR = mISPParameters.getPltmWDR();
        if(stPltmWDR != -1)
        {
            alogv("set ISP_PltmWDR");
            AW_MPI_ISP_SetPltmWDR(mIspDevId, stPltmWDR);
        }
        #endif
        initIspParameters();
    }
#endif
    status_t ret = pChannel->startChannel();
    return ret;
}

status_t VIDevice::stopChannel(int chnId)
{
    status_t ret;
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    ret = pChannel->stopChannel();
#if (defined(SUPPORT_ISP))
    if(mbIspRun)
    {
        AutoMutex lock(mVIChannelVectorLock);
        int num = 0;
        for(VIChannelInfo& i : mVIChannelVector)
        {
            if(VIChannel::VI_CHN_STATE_STARTED == i.mpChannel->getState() && i.mChnId != chnId)
            {
                num++;
            }
        }
        if(0 == num)
        {
            //if(VIChannel::VI_CHN_STATE_STARTED == pChannel->getState())
            //{
                alogd("ISP[%d] stop", mIspDevId);
                AW_MPI_ISP_Stop(mIspDevId);
                mbIspRun = false;
            //}
        }
    }
#endif
    return ret;
}

status_t VIDevice::getMODParams(int chnId, MOTION_DETECT_ATTR_S *pParamMD)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getMODParams(pParamMD);
}

status_t VIDevice::setMODParams(int chnId, MOTION_DETECT_ATTR_S pParamMD)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setMODParams(pParamMD);
}

status_t VIDevice::startMODDetect(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startMODDetect();
}

status_t VIDevice::stopMODDetect(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopMODDetect();
}

status_t VIDevice::getAdasParams(int chnId, AdasDetectParam *pParamADAS)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getAdasParams(pParamADAS);
}
status_t VIDevice::setAdasParams(int chnId, AdasDetectParam pParamADAS)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setAdasParams(pParamADAS);
}
status_t VIDevice::getAdasInParams(int chnId, AdasInParam *pParamADAS)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getAdasInParams(pParamADAS);
}
status_t VIDevice::setAdasInParams(int chnId, AdasInParam pParamADAS)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setAdasInParams(pParamADAS);
}
status_t VIDevice::startAdasDetect(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startAdasDetect();
}
status_t VIDevice::stopAdasDetect(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopAdasDetect();
}
///============
status_t VIDevice::getAdasParams_v2(int chnId, AdasDetectParam_v2 *pParamADAS_v2)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getAdasParams_v2(pParamADAS_v2);
}
status_t VIDevice::setAdasParams_v2(int chnId, AdasDetectParam_v2 pParamADAS_v2)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setAdasParams_v2(pParamADAS_v2);
}
status_t VIDevice::getAdasInParams_v2(int chnId, AdasInParam_v2 *pParamADAS_v2)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getAdasInParams_v2(pParamADAS_v2);
}
status_t VIDevice::setAdasInParams_v2(int chnId, AdasInParam_v2 pParamADAS_v2)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setAdasInParams_v2(pParamADAS_v2);
}

status_t VIDevice::startAdasDetect_v2(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startAdasDetect_v2();
}
status_t VIDevice::stopAdasDetect_v2(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopAdasDetect_v2();
}

void VIDevice::releaseRecordingFrame(int chnId, uint32_t index)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->releaseFrame(index);
}

status_t VIDevice::startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startRecording(pCb, recorderId);
}

status_t VIDevice::stopRecording(int chnId, int recorderId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopRecording(recorderId);
}

void VIDevice::setDataListener(int chnId, DataListener *pCb)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->setDataListener(pCb);
}

void VIDevice::setNotifyListener(int chnId, NotifyListener *pCb)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->setNotifyListener(pCb);
}

/*
void VIDevice::postDataCompleted(int chnId, const void *pData, int size)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->postDataCompleted(pData, size);
}

void VIDevice::postVdaDataCompleted(int chnId, const VDA_DATA_S *pData)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->postVdaDataCompleted(pData);
}
*/

status_t VIDevice::takePicture(int chnId, unsigned int msgType, PictureRegionCallback *pPicReg)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->takePicture(msgType, pPicReg);
}

status_t VIDevice::notifyPictureRelease(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->notifyPictureRelease();
}

status_t VIDevice::cancelContinuousPicture(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->cancelContinuousPicture();
}

status_t VIDevice::KeepPictureEncoder(int chnId, bool bKeep)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->KeepPictureEncoder(bKeep);
}

status_t VIDevice::releasePictureEncoder(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->releasePictureEncoder();
}

status_t VIDevice::getISPDMsg(int *exp, int *exp_line, int *gain, int *lv_idx, int *color_temp, int *rgain, int *bgain, int *grgain, int *gbgain)
{
      int tmp;
 
      AW_MPI_ISP_AE_GetExposure(mIspDevId, &tmp);
      *exp = tmp;
 
      AW_MPI_ISP_AE_GetExposureLine(mIspDevId, &tmp);
      *exp_line = tmp;
 
      AW_MPI_ISP_AE_GetGain(mIspDevId, &tmp);
      *gain = tmp;
 
      AW_MPI_ISP_AWB_GetRGain(mIspDevId, &tmp);
      *rgain = tmp;
 
      AW_MPI_ISP_AWB_GetBGain(mIspDevId, &tmp);
      *bgain = tmp;

      AW_MPI_ISP_AWB_GetGrGain(mIspDevId, &tmp);        //get gr_gain
      *grgain = tmp;
      
      AW_MPI_ISP_AWB_GetGbGain(mIspDevId, &tmp);        //get gb_gain
      *gbgain = tmp;

      AW_MPI_ISP_AWB_GetCurColorT(mIspDevId, &tmp);
      *color_temp = tmp;
 
      AW_MPI_ISP_AE_GetEvIdx(mIspDevId, &tmp);
      *lv_idx= tmp;
      
      return NO_ERROR;
}

  


}; /* namespace EyeseeLinux */
