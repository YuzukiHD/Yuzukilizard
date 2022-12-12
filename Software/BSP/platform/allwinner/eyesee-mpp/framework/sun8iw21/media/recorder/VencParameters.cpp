/******************************************************************************
  Copyright (C), 2020-2022, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VencParameters.cpp
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2020/11/10
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

#include <string.h>

#include <utils/plat_log.h>
#include <VencParameters.h>

using namespace std;
namespace EyeseeLinux {

VencParameters::VencParameters()
{
    mFrameRate = 30;
    mVideoEncoder = PT_MAX;
    mVideoWidth = 1920;
    mVideoHeight = 1080;
    mVideoMaxKeyItl = 30;
    mIQpOffset = 0;
    mFastEncFlag = 0;
    //mVideoRCMode = VideoRCMode_CBR;
    mVideoPDMode = VideoEncodeProductMode::NORMAL_MODE;
    mSensorType = VENC_ST_DIS_WDR;
    mbPIntraEnable = true;
    mNullSkipEnable = false;
    mPSkipEnable = false;
    mbHorizonfilp = false;
    mbAdaptiveintrainp = false;
    mbColor2Grey = false;
    mVeChn = MM_INVALID_CHN;
    //mSensorType = VENC_ST_DEFAULT;
    mOnlineEnable = false;
    mOnlineShareBufNum = 0;

    memset(&mVenc3DnrParam, 0, sizeof(s3DfilterParam));
    memset(&mVEncAttr, 0, sizeof(VEncAttr));
    memset(&mVEncRcAttr, 0, sizeof(VEncBitRateControlAttr));
    memset(&mSmartPParam, 0, sizeof(VencSmartFun));
    memset(&mIntraRefreshParam, 0, sizeof(VENC_PARAM_INTRA_REFRESH_S));
    memset(&mVEncChnAttr, 0, sizeof(mVEncChnAttr));
    memset(&mVEncRcParam, 0, sizeof(mVEncRcParam));
    memset(&mVEncRefParam, 0, sizeof(VENC_PARAM_REF_S));
    memset(&mVEncRoiCfg, 0, sizeof(mVEncRoiCfg));
    memset(&mVEncSuperFrameCfg, 0, sizeof(VENC_SUPERFRAME_CFG_S));
    memset(&mSaveBSFileParam, 0, sizeof(VencSaveBSFile));
    memset(&mVeProcSet, 0, sizeof(VeProcSet));
}

VencParameters::~VencParameters()
{

}

void VencParameters::setVEncAttr(VencParameters::VEncAttr &nVEncAttr)
{
    mVEncAttr = nVEncAttr;
}

VencParameters::VEncAttr VencParameters::getVEncAttr()
{
    return mVEncAttr;
}

void  VencParameters::setVideoSize(const SIZE_S &nVideoSize)
{
    mVideoWidth =  nVideoSize.Width;
    mVideoHeight = nVideoSize.Height;
}

void VencParameters::getVideoSize(SIZE_S &nVideoSize)
{
    nVideoSize.Width = mVideoWidth;
    nVideoSize.Height = mVideoHeight;
}

status_t VencParameters::setIQpOffset(int nIQpOffset)
{
    if (PT_H264 == mVideoEncoder)
    {
        if (!(nIQpOffset>=0) && nIQpOffset<10)
        {
            aloge("IQpOffset value must be in [0, 10) for 264!");
            return BAD_VALUE;
        }
    }
    else if (PT_H265 == mVideoEncoder)
    {
        if (!(nIQpOffset>=-12 && nIQpOffset<=12))
        {
            aloge("IQpOffset value must be in [-12, 12] for 265");
        }
    }
    else
    {
        aloge("IQpOffset can not be set for other vencoder(%d)!", mVideoEncoder);
    }

    mIQpOffset = nIQpOffset;
    return NO_ERROR;
}

void VencParameters::setVEncBitRateControlAttr(VencParameters::VEncBitRateControlAttr &RcAttr)
{
    mVEncRcAttr = RcAttr;
}

VencParameters::VEncBitRateControlAttr VencParameters::getVEncBitRateControlAttr()
{
    return mVEncRcAttr;
}

//void VencParameters::setVideoEncodingRateControlMode(const VencParameters::VideoEncodeRateControlMode &rcMode)
//{
//    mVideoRCMode = rcMode;
//}

//VencParameters::VideoEncodeRateControlMode VencParameters::getVideoEncodingRateControlMode()
//{
//    return mVideoRCMode;
//}

void VencParameters::setVideoEncodingProductMode(VencParameters::VideoEncodeProductMode &ndMode)
{
    mVideoPDMode = ndMode;
}

VencParameters::VideoEncodeProductMode VencParameters::getVideoEncodingProductMode()
{
    return mVideoPDMode;
}

void VencParameters::set3DFilter(s3DfilterParam &n3DfilterParam)
{
    mVenc3DnrParam = n3DfilterParam;
}

s3DfilterParam VencParameters::get3DFilter()
{
    return mVenc3DnrParam;
}

void VencParameters::setVencSuperFrameConfig(VENC_SUPERFRAME_CFG_S &nSuperFrameConfig)
{
    mVEncSuperFrameCfg = nSuperFrameConfig;
}

VENC_SUPERFRAME_CFG_S VencParameters::getVencSuperFrameConfig()
{
    return mVEncSuperFrameCfg;
}

void VencParameters::enableSaveBSFile(VencSaveBSFile &nSavaParam)
{
    mSaveBSFileParam = nSavaParam;
}

VencSaveBSFile VencParameters::getenableSaveBSFile()
{
    return mSaveBSFileParam;
}

void VencParameters::setProcSet(VeProcSet &nVeProcSet)
{
    mVeProcSet = nVeProcSet;
}

VeProcSet VencParameters::getProcSet()
{
    return mVeProcSet;
}

void VencParameters::setVideoEncodingSmartP(VencSmartFun &nParam)
{
    mSmartPParam = nParam;
}

VencSmartFun VencParameters::getVideoEncodingSmartP()
{
    return mSmartPParam;
}

void VencParameters::setVideoEncodingIntraRefresh(VENC_PARAM_INTRA_REFRESH_S &nIntraRefresh)
{
    mIntraRefreshParam = nIntraRefresh;
}

VENC_PARAM_INTRA_REFRESH_S VencParameters::getVideoEncodingIntraRefresh()
{
    return mIntraRefreshParam;
}

void VencParameters::setGopAttr(const VENC_GOP_ATTR_S &nParam)
{
    mVEncChnAttr.GopAttr = nParam;
}

VENC_GOP_ATTR_S VencParameters::getGopAttr()
{
    return mVEncChnAttr.GopAttr;
}

void VencParameters::setVencChnAttr(VENC_CHN_ATTR_S &nVEncChnAttr)
{
    mVEncChnAttr = nVEncChnAttr;
}

VENC_CHN_ATTR_S VencParameters::getVencChnAttr()
{
    return mVEncChnAttr;
}

void VencParameters::setVencRcParam(VENC_RC_PARAM_S &stVEncRcParam)
{
    mVEncRcParam = stVEncRcParam;
}

VENC_RC_PARAM_S VencParameters::getVencRcParam()
{
    return mVEncRcParam;
}

void VencParameters::setRefParam(const VENC_PARAM_REF_S &nstRefParam)
{
    mVEncRefParam = nstRefParam;
}

VENC_PARAM_REF_S VencParameters::getRefParam()
{
    return mVEncRefParam;
}

void VencParameters::setRoiCfg(VENC_ROI_CFG_S &nVencRoiCfg)
{
    mVEncRoiCfg = nVencRoiCfg;
}

VENC_ROI_CFG_S VencParameters::getRoiCfg()
{
    return mVEncRoiCfg;
}

status_t VencParameters::enableIframeFilter(bool enable)
{
    alogd("need implement");
    return UNKNOWN_ERROR;
}

bool VencParameters::getIframeFilter()
{
    alogw("need implement");
    return false;
}

status_t VencParameters::setVideoEncodingMode(int Mode)
{
    alogd("need implement!");
    return UNKNOWN_ERROR;
}

};
