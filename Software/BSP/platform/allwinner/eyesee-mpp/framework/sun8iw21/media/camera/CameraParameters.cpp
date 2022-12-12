/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CameraParameters.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/06
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "CameraParameters"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>
#include <linux/videodev2.h>

#include <CameraParameters.h>

using namespace std;
namespace EyeseeLinux {

CameraParameters::SensorParamSet::SensorParamSet(int width, int height, int fps)
{
    mWidth = width;
    mHeight = height;
    mFps = fps;
}

bool CameraParameters::SensorParamSet::operator== (const SensorParamSet& rhs)
{
    if(mWidth == rhs.mWidth && mHeight == rhs.mHeight && mFps == rhs.mFps)
    {
        return true;
    }
    else
    {
        return false;
    }
}

CameraParameters::CameraParameters()
{
    mCaptureMode = V4L2_MODE_VIDEO;
    mVideoSize = {1280, 720};
    mFrameRate = 25;
    mDisplayFrameRate = 0;
    mPreviewRotation = 0;
    mPixelFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    mColorSpace = V4L2_COLORSPACE_JPEG;
    mVideoBufferNum = 5;
    mPictureSize = {1280, 720};
    mJpegThumbnailSize = {320, 240};
    mJpegThumbnailQuality = 60;
    mJpegQuality = 90;
    mJpegRotation = 0;
    mPictureFromat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    mTakePictureMode = TAKE_PICTURE_MODE_NULL;
    mGpsLatitude = 0.0;
    mGpsLongitude = 0.0;
    mGpsAltitude = 0.0;
    mGpsTimestamp = 0;
    mpGpsProcessingMethod = NULL;
    mContinuousPicNum = 5;
    mContinuousPicIntervalMs = 200;
    mLostFrameNumber = 0;

    memset(&mBrightness, 0, sizeof(ISP_SINGLE_S));
//    memset(&mContrast, 0, sizeof(ISP_CONTRAST_S));
//    memset(&mSaturation, 0, sizeof(ISP_SATURATION_S));
    memset(&mHue, 0, sizeof(ISP_HUE_S));
    memset(&mSharpness, 0, sizeof(ISP_SHARPEN_ATTR_S));
    memset(&mWBAttr, 0, sizeof(ISP_WB_ATTR_S));
    memset(&mColorMatrixAttr, 0, sizeof(ISP_COLORMATRIX_ATTR_S));
    memset(&mAWBSpeed, 0, sizeof(ISP_AWB_SPEED_S));
    memset(&mAWBTempRange, 0, sizeof(ISP_AWB_TEMP_RANGE_S));
    memset(&mAWBFavor, 0, sizeof(ISP_AWB_FAVOR_S));
    memset(&mFlicker, 0, sizeof(ISP_FLICKER_S));
    memset(&mB4G2dUserRegion, 0, sizeof(RECT_S));
    memset(&mUserRegion, 0, sizeof(RECT_S));

    mNRAttr           =  -1;
    m3NRAttr          =  -1;
    mPltmWDR          =  -1;
    mModuleOnOff.pltm = -1;
    mModuleOnOff.tdf =  -1;

    mChnIspAe_Mode = -1;
    mChnIspAe_ExposureBias = -1;
    mChnIspAe_Exposure = -1;
    mChnIspAe_Gain = -1;
    mChnIspAe_ISOSensitive = -1;
    mChnIspAe_Metering = -1;
    mChnIspAwb_Mode = -1;
    mChnIspAwb_ColorTemp = -1;
    mChnIspAwb_RGain = -1;
    mChnIspAwb_GrGain = -1;
    mChnIspAwb_GbGain = -1;
    mChnIspAwb_BGain = -1;
    mChnIsp_Flicker = -1;
    mChnIsp_Brightness = -1;
    mChnIsp_Contrast = -1;
    mChnIsp_Saturation = -1;
    mChnIsp_Sharpness = -1;
    //mChnIsp_Hue = -1;

    mZoom = 0;
    mMaxZoom = 10;
    mbZoomSupported = true;
    mMirror = 0;
    mFlip = 0;
    memset(&mShutTime, 0, sizeof mShutTime);

    mOnlineEnable = false;
    mOnlineShareBufNum = 0;
}

CameraParameters::~CameraParameters()
{
    if (mpGpsProcessingMethod != NULL) {
        free(mpGpsProcessingMethod);
    }
}

void CameraParameters::set(const char *key, const char *value)
{
    // XXX i think i can do this with strspn()
    if (strchr(key, '=') || strchr(key, ';')) {
        //XXX ALOGE("Key \"%s\"contains invalid character (= or ;)", key);
        return;
    }

    if (strchr(value, '=') || strchr(value, ';')) {
        //XXX ALOGE("Value \"%s\"contains invalid character (= or ;)", value);
        return;
    }

    mMap[std::string(key)] = value;
}

void CameraParameters::set(const char *key, int value)
{
    char str[16];
    sprintf(str, "%d", value);
    set(key, str);
}

void CameraParameters::setFloat(const char *key, float value)
{
    char str[16];  // 14 should be enough. We overestimate to be safe.
    snprintf(str, sizeof(str), "%g", value);
    set(key, str);
}

const char *CameraParameters::get(const char *key) const
{
    std::map<std::string, std::string>::const_iterator it = mMap.find(std::string(key));
    if (it == mMap.end())
    {
        return NULL;
    }
    if(it->second.length() == 0)
    {
        return NULL;
    }
    return it->second.c_str();
}

int CameraParameters::getInt(const char *key) const
{
    const char *v = get(key);
    if (v == 0)
        return -1;
    return strtol(v, 0, 0);
}

float CameraParameters::getFloat(const char *key) const
{
    const char *v = get(key);
    if (v == 0) return -1;
    return strtof(v, 0);
}

void CameraParameters::setBrightnessValue(ISP_SINGLE_S& value)
{
    mBrightness = value;
}

ISP_SINGLE_S& CameraParameters::getBrightnessValue()
{
    return mBrightness;
}
/*
void CameraParameters::setContrastValue(ISP_CONTRAST_S& value)
{
    mContrast = value;
}

ISP_CONTRAST_S& CameraParameters::getContrastValue()
{
    return mContrast;
}

void CameraParameters::setSaturationValue(ISP_SATURATION_S& value)
{
    mSaturation = value;
}

ISP_SATURATION_S& CameraParameters::getSaturationValue()
{
    return mSaturation;
}
*/
void CameraParameters::setHueValue(ISP_HUE_S& value)
{
    mHue = value;
}

ISP_HUE_S& CameraParameters::getHueValue()
{
    return mHue;
}

void CameraParameters::setSharpnessValue(ISP_SHARPEN_ATTR_S& value)
{
    mSharpness = value;
}

ISP_SHARPEN_ATTR_S& CameraParameters::getSharpnessValue()
{
    return mSharpness;
}

void CameraParameters::setAWB_WBAttrValue(ISP_WB_ATTR_S& value)
{
    mWBAttr = value;
}

ISP_WB_ATTR_S& CameraParameters::getAWB_WBAttrValue()
{
    return mWBAttr;
}

void CameraParameters::setAWB_CCMAttrValue(ISP_COLORMATRIX_ATTR_S& value)
{
    mColorMatrixAttr = value;
}

ISP_COLORMATRIX_ATTR_S& CameraParameters::getAWB_CCMAttrValue()
{
    return mColorMatrixAttr;
}

void CameraParameters::setAWB_SpeedValue(ISP_AWB_SPEED_S& value)
{
    mAWBSpeed = value;
}

ISP_AWB_SPEED_S& CameraParameters::getAWB_SpeedValue()
{
    return mAWBSpeed;
}

void CameraParameters::setAWB_TempRangeValue(ISP_AWB_TEMP_RANGE_S& value)
{
    mAWBTempRange = value;
}

ISP_AWB_TEMP_RANGE_S& CameraParameters::getAWB_TempRangeValue()
{
    return mAWBTempRange;
}

void CameraParameters::setAWB_LightValue(int mode, ISP_AWB_TEMP_INFO_S& value)
{
    if(mode<0x01 || mode>0x04)
    {
        aloge("fatal error! AWB light mode[0x%x] is invalid!", mode);
        return;
    }
    mAWBLight[mode] = value;
}

ISP_AWB_TEMP_INFO_S* CameraParameters::getAWB_LightValue(int mode)
{
    std::map<int, ISP_AWB_TEMP_INFO_S>::iterator it = mAWBLight.find(mode);
    if(it != mAWBLight.end())
    {
        return &it->second;
    }
    return NULL;
}

void CameraParameters::setAWB_LightValues(std::map<int, ISP_AWB_TEMP_INFO_S>& value)
{
    mAWBLight = value;
}

std::map<int, ISP_AWB_TEMP_INFO_S>& CameraParameters::getAWB_LightValues()
{
    return mAWBLight;
}

void CameraParameters::setAWB_FavorValue(ISP_AWB_FAVOR_S& value)
{
    mAWBFavor = value;
}

ISP_AWB_FAVOR_S& CameraParameters::getAWB_FavorValue()
{
    return mAWBFavor;
}

void CameraParameters::setFlickerValue(ISP_FLICKER_S& value)
{
    mFlicker = value;
}

ISP_FLICKER_S& CameraParameters::getFlickerValue()
{
    return mFlicker;
}

void CameraParameters::setNRAttrValue(int value)
{
    mNRAttr = value;
}

int CameraParameters::getNRAttrValue()
{
    return mNRAttr;
}

void CameraParameters::set3NRAttrValue(int value)
{
    m3NRAttr = value;
}

int CameraParameters::get3NRAttrValue()
{
    return m3NRAttr;
}

void CameraParameters::setPltmWDR(int value)
{
    mPltmWDR = value;
}

int CameraParameters::getPltmWDR()
{
    return mPltmWDR;
}

void CameraParameters::setModuleOnOff(ISP_MODULE_ONOFF& value)
{
    mModuleOnOff = value;
}

ISP_MODULE_ONOFF& CameraParameters::getModuleOnOff()
{
    return mModuleOnOff;
}


void CameraParameters::ChnIspAe_SetMode(int value)
{
    mChnIspAe_Mode = value;
}
void CameraParameters::ChnIspAe_SetExposureBias(int value)
{
    mChnIspAe_ExposureBias = value;
}
void CameraParameters::ChnIspAe_SetExposure(int value)
{
    mChnIspAe_Exposure = value;
}
void CameraParameters::ChnIspAe_SetGain(int value)
{
    mChnIspAe_Gain = value;
}
void CameraParameters::ChnIspAe_SetMetering(int value)
{
    mChnIspAe_Metering = value;
}
void CameraParameters::ChnIspAe_SetISOSensitive(int value)
{
    mChnIspAe_ISOSensitive = value;
}
void CameraParameters::ChnIspAwb_SetMode(int value)
{
    mChnIspAwb_Mode = value;
}
void CameraParameters::ChnIspAwb_SetColorTemp(int value)
{
    mChnIspAwb_ColorTemp = value;
}

void CameraParameters::ChnIspAwb_SetRGain(int value)
{
    mChnIspAwb_RGain = value;
}

void CameraParameters::ChnIspAwb_SetGrGain(int value)
{
    mChnIspAwb_GrGain = value;
}

void CameraParameters::ChnIspAwb_SetGbGain(int value)
{
    mChnIspAwb_GbGain = value;
}

void CameraParameters::ChnIspAwb_SetBGain(int value)
{
    mChnIspAwb_BGain = value;
}

void CameraParameters::ChnIsp_SetFlicker(int value)
{
    mChnIsp_Flicker = value;
}

void CameraParameters::ChnIsp_SetBrightness(int value)
{
    mChnIsp_Brightness = value;
}
void CameraParameters::ChnIsp_SetContrast(int value)
{
    mChnIsp_Contrast = value;
}
void CameraParameters::ChnIsp_SetSaturation(int value)
{
    mChnIsp_Saturation = value;
}
void CameraParameters::ChnIsp_SetSharpness(int value)
{
    mChnIsp_Sharpness = value;
}
//void CameraParameters::ChnIsp_SetHue(int value)
//{
//    mChnIsp_Hue = value;
//}
int CameraParameters::ChnIspAe_GetMode()
{
    return mChnIspAe_Mode;
}
int CameraParameters::ChnIspAe_GetExposureBias()
{
    return mChnIspAe_ExposureBias;
}
int CameraParameters::ChnIspAe_GetExposure()
{
    return mChnIspAe_Exposure;
}
int CameraParameters::ChnIspAe_GetGain()
{
    return mChnIspAe_Gain;
}
int CameraParameters::ChnIspAe_GetMetering()
{
    return mChnIspAe_Metering;
}
int CameraParameters::ChnIspAe_GetISOSensitive()
{
    return mChnIspAe_ISOSensitive;
}
int CameraParameters::ChnIspAwb_GetMode()
{
    return mChnIspAwb_Mode;
}
int CameraParameters::ChnIspAwb_GetColorTemp()
{
    return mChnIspAwb_ColorTemp;
}

int CameraParameters::ChnIspAwb_GetRGain()
{
    return mChnIspAwb_RGain;
}

int CameraParameters::ChnIspAwb_GetGrGain()
{
    return mChnIspAwb_GrGain;
}

int CameraParameters::ChnIspAwb_GetGbGain()
{
    return mChnIspAwb_GbGain;
}

int CameraParameters::ChnIspAwb_GetBGain()
{
    return mChnIspAwb_BGain;
}

int CameraParameters::ChnIsp_GetFlicker()
{
    return mChnIsp_Flicker;
}

int CameraParameters::ChnIsp_GetBrightness()
{
    return mChnIsp_Brightness;
}
int CameraParameters::ChnIsp_GetContrast()
{
    return mChnIsp_Contrast;
}
int CameraParameters::ChnIsp_GetSaturation()
{
    return mChnIsp_Saturation;
}
int CameraParameters::ChnIsp_GetSharpness()
{
    return mChnIsp_Sharpness;
}
//int CameraParameters::ChnIsp_GetHue()
//{
//    return mChnIsp_Hue;
//}

void CameraParameters::setZoom(int value)
{
    mZoom = value;
}

int CameraParameters::getZoom()
{
    return mZoom;
}

void CameraParameters::setZoomSupported(bool value)
{
    mbZoomSupported = value;
}

bool CameraParameters::isZoomSupported()
{
    return mbZoomSupported;
}
void CameraParameters::setMaxZoom(int value)
{
    mMaxZoom = value;
}

int CameraParameters::getMaxZoom()
{
    return mMaxZoom;
}

void CameraParameters::setCaptureMode(int mode)
{
    mCaptureMode = mode;
}

int CameraParameters::getCaptureMode()
{
    return mCaptureMode;
}

void CameraParameters::setVideoSize(SIZE_S &size)
{
    mVideoSize = size;
    mVideoSizeOut = mVideoSize;
    mVideoBufSizeOut = mVideoSize;
}
void CameraParameters::getVideoSizeOut(SIZE_S &size) const
{
    size = mVideoSizeOut;
}
void CameraParameters::setVideoBufSizeOut(SIZE_S &size)
{
    mVideoBufSizeOut = size;
}

void CameraParameters::getVideoBufSizeOut(SIZE_S &size) const
{
    size = mVideoBufSizeOut;
}

void CameraParameters::SetMirror(int value)
{
    mMirror = value;
}

int CameraParameters::GetMirror()
{
    return mMirror;
}

void CameraParameters::SetFlip(int value)
{
    mFlip = value;
}

int CameraParameters::GetFlip()
{
    return mFlip;
}

void CameraParameters::setShutTime(VI_SHUTTIME_CFG_S &value)
{
    mShutTime = value;
}
void CameraParameters::getShutTime(VI_SHUTTIME_CFG_S &value) const
{
    value = mShutTime;
}
void CameraParameters::setSupportedSensorParamSets(std::vector<SensorParamSet> &SensorParamSets)
{
    mSupportedSensorParamSets = SensorParamSets;
}
void CameraParameters::getSupportedSensorParamSets(std::vector<SensorParamSet> &SensorParamSets) const
{
    SensorParamSets = mSupportedSensorParamSets;
}

void CameraParameters::setGpsProcessingMethod(const char *processing_method)
{
    if (mpGpsProcessingMethod != NULL) {
        free(mpGpsProcessingMethod);
    }
    if (processing_method == NULL) {
        mpGpsProcessingMethod = NULL;
    } else {
        mpGpsProcessingMethod = strdup(processing_method);
    }
}

void CameraParameters::removeGpsData()
{
    if (mpGpsProcessingMethod != NULL) {
        free(mpGpsProcessingMethod);
        mpGpsProcessingMethod = NULL;
    }
}

}; /* namespace EyeseeLinux */
