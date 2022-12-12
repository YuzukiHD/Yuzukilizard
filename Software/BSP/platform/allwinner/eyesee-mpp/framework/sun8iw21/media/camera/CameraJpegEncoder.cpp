/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CameraJpegEncoder.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/15
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "CameraJpegEncoder"
#include <utils/plat_log.h>

#include <cstdlib>
#include <cstring>
#include <mpi_venc.h>

#include "CameraJpegEncoder.h"


using namespace std;
namespace EyeseeLinux {

CameraJpegEncoder::CameraJpegEncoder()
{
    mChn = MM_INVALID_CHN;
    mCurFrameId = -1;
    VENC_PACK_S *pEncPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S));
    memset(pEncPack, 0, sizeof(VENC_PACK_S));
    mOutStream.mpPack = pEncPack;
    mOutStream.mPackCount = 1;
    cdx_sem_init(&mSemFrameBack, 0);
}

CameraJpegEncoder::~CameraJpegEncoder()
{
    cdx_sem_deinit(&mSemFrameBack);
    free(mOutStream.mpPack);
    mOutStream.mpPack = NULL;
    mOutStream.mPackCount = 0;
}

ERRORTYPE CameraJpegEncoder::VEncCallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    CameraJpegEncoder *pThiz = (CameraJpegEncoder*)cookie;
    if(MOD_ID_VENC == pChn->mModId)
    {
        if(pChn->mChnId != pThiz->mChn)
        {
            aloge("fatal error! VO chnId[%d]!=[%d]", pChn->mChnId, pThiz->mChn);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                if(pThiz->mCurFrameId != pVideoFrameInfo->mId)
                {
                    aloge("fatal error! frameId is not match[%d]!=[%d]!", pThiz->mCurFrameId, pVideoFrameInfo->mId);
                }
                cdx_sem_up(&pThiz->mSemFrameBack);
                break;
            }
            case MPP_EVENT_VENC_BUFFER_FULL:
            {
                alogd("jpeg encoder chn[%d] vbvBuffer full", pChn->mChnId);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VENC_ILLEGAL_PARAM;
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

status_t CameraJpegEncoder::initialize(CameraJpegEncConfig * pConfig)
{
    status_t result = NO_ERROR;
    int venc_ret = 0;
    if(mChn >= 0)
    {
        aloge("fatal error! already init jpeg encoder!");
    }

    memset(&mChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    mChnAttr.VeAttr.Type = PT_JPEG;
    mChnAttr.VeAttr.AttrJpeg.MaxPicWidth = 0;
    mChnAttr.VeAttr.AttrJpeg.MaxPicHeight = 0;
    mChnAttr.VeAttr.AttrJpeg.BufSize = pConfig->nVbvBufferSize;
    mChnAttr.VeAttr.AttrJpeg.mThreshSize = pConfig->nVbvThreshSize;
    mChnAttr.VeAttr.AttrJpeg.bByFrame = TRUE;
    mChnAttr.VeAttr.AttrJpeg.PicWidth = pConfig->mPicWidth;
    mChnAttr.VeAttr.AttrJpeg.PicHeight = pConfig->mPicHeight;
    mChnAttr.VeAttr.AttrJpeg.bSupportDCF = FALSE;
    mChnAttr.VeAttr.MaxKeyInterval = 1;
    mChnAttr.VeAttr.SrcPicWidth = pConfig->mSourceWidth;
    mChnAttr.VeAttr.SrcPicHeight = pConfig->mSourceHeight;
    mChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    mChnAttr.VeAttr.PixelFormat = pConfig->mPixelFormat;

    ERRORTYPE ret;
    bool bSuccessFlag = false;
    mChn = 0;
    while(mChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(mChn, &mChnAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("create venc channel[%d] success!", mChn);
            break;
        }
        else if(ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", mChn);
            mChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", mChn, ret);
            mChn++;
        }
    }
    if(false == bSuccessFlag)
    {
        mChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        result = UNKNOWN_ERROR;
        goto _err0;
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)this;
    cbInfo.callback = (MPPCallbackFuncType)&VEncCallback;
    AW_MPI_VENC_RegisterCallback(mChn, &cbInfo);

    memset(&mJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
    mJpegParam.Qfactor = pConfig->mQuality;
    AW_MPI_VENC_SetJpegParam(mChn, &mJpegParam);
    AW_MPI_VENC_ForbidDiscardingFrame(mChn, TRUE);

    venc_ret = AW_MPI_VENC_StartRecvPic(mChn);
    if(SUCCESS != venc_ret)
    {
        aloge("fatal error:%x jpegEnc AW_MPI_VENC_StartRecvPic",venc_ret);
        if(ERR_VENC_NOMEM == venc_ret)
        {
            result = NO_MEMORY;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    return result;
_err0:
    return result;
}

status_t CameraJpegEncoder::destroy()
{
    AW_MPI_VENC_StopRecvPic(mChn);
    //can't reset channel now, because in non-tunnel mode, mpi_venc will force release out frames, but we don't want
    //those out frames to be released before we return them.
    //AW_MPI_VENC_ResetChn(mChn); 
    AW_MPI_VENC_DestroyChn(mChn);
    mChn = MM_INVALID_CHN;
    return NO_ERROR;
}

status_t CameraJpegEncoder::encode(VIDEO_FRAME_INFO_S *pFrameInfo, VENC_EXIFINFO_S *pExifInfo)
{
    mCurFrameId = pFrameInfo->mId;
    AW_MPI_VENC_SetJpegExifInfo(mChn, pExifInfo);
    AW_MPI_VENC_SendFrame(mChn, pFrameInfo, 0);
    //cdx_sem_down(&mSemFrameBack);
    int ret = cdx_sem_down_timedwait(&mSemFrameBack, 10*1000);
    if(0 == ret)
    {
        return NO_ERROR;
    }
    else
    {
        alogd("Be careful! jpeg encode error[0x%x]", ret);
        return TIMED_OUT;
    }
}

int CameraJpegEncoder::getFrame()
{
    ERRORTYPE ret = AW_MPI_VENC_GetStream(mChn, &mOutStream, 1000);
    if(ret != SUCCESS)
    {
        aloge("fatal error! why get stream fail?");
        return -1;
    }
    else
    {
        AW_MPI_VENC_GetJpegThumbBuffer(mChn, &mJpegThumbBuf);
        return 0;
    }
}

int CameraJpegEncoder::returnFrame(VENC_STREAM_S *pVencStream)
{
    ERRORTYPE ret = AW_MPI_VENC_ReleaseStream(mChn, pVencStream);
    if(ret != SUCCESS)
    {
        aloge("fatal error! why release stream fail?");
        return -1;
    }
    else
    {
        return 0;
    }
}

unsigned int CameraJpegEncoder::getDataSize()
{
    return mOutStream.mpPack[0].mLen0 + mOutStream.mpPack[0].mLen1 + mOutStream.mpPack[0].mLen2;
}
/*
status_t CameraJpegEncoder::getDataToBuffer(void *buffer)
{
    char *p = (char *)buffer;
    memcpy(p, mOutStream.mpPack[0].mpAddr0, mOutStream.mpPack[0].mLen0);
    p += mOutStream.mpPack[0].mLen0;
    if(mOutStream.mpPack[0].mLen1 > 0)
    {
        memcpy(p, mOutStream.mpPack[0].mpAddr1, mOutStream.mpPack[0].mLen1);
    }
    return NO_ERROR;
}
*/
status_t CameraJpegEncoder::getThumbOffset(off_t &offset, size_t &len)
{
    status_t ret = NO_ERROR;
    if(mJpegThumbBuf.ThumbLen > 0)
    {
        len = mJpegThumbBuf.ThumbLen;
        //deduce thumb picture buffer in mOutStream
        if(NULL==mOutStream.mpPack[0].mpAddr0 || NULL==mOutStream.mpPack[0].mpAddr1 || 0==mOutStream.mpPack[0].mLen0 || 0==mOutStream.mpPack[0].mLen1)
        {
            aloge("fatal error! check code!");
        }
        unsigned char *pExifBufStart = mOutStream.mpPack[0].mpAddr0;
        if(mJpegThumbBuf.ThumbAddrVir >= mOutStream.mpPack[0].mpAddr0)
        {
            if(mJpegThumbBuf.ThumbAddrVir >= mOutStream.mpPack[0].mpAddr0+mOutStream.mpPack[0].mLen0)
            {
                aloge("fatal error! check code![%p][%p][%d]", mJpegThumbBuf.ThumbAddrVir, mOutStream.mpPack[0].mpAddr0, mOutStream.mpPack[0].mLen0);
            }
            offset = mJpegThumbBuf.ThumbAddrVir - mOutStream.mpPack[0].mpAddr0;
        }
        else if(mJpegThumbBuf.ThumbAddrVir>=mOutStream.mpPack[0].mpAddr1)
        {
            if(mJpegThumbBuf.ThumbAddrVir >= mOutStream.mpPack[0].mpAddr1+mOutStream.mpPack[0].mLen1)
            {
                aloge("fatal error! check code![%p][%p][%d]", mJpegThumbBuf.ThumbAddrVir, mOutStream.mpPack[0].mpAddr1, mOutStream.mpPack[0].mLen1);
            }
            offset = mOutStream.mpPack[0].mLen0 + (mJpegThumbBuf.ThumbAddrVir - mOutStream.mpPack[0].mpAddr1);
        }
        else
        {
            aloge("fatal error! check code![%p][%p][%d][%p][%d]", mJpegThumbBuf.ThumbAddrVir, 
                mOutStream.mpPack[0].mpAddr0, mOutStream.mpPack[0].mLen0, mOutStream.mpPack[0].mpAddr1, mOutStream.mpPack[0].mLen1);
            ret = UNKNOWN_ERROR;
        }
    }
    else
    {
        alogd("jpeg has no thumb picture");
        offset = 0;
        len = 0;
    }
    //alogd("size of off_t[%d], size_t[%d]", sizeof(off_t), sizeof(size_t));
    //alogd("thumbBuf [%p][%d], [%p][%d][%p][%d][%p][%d]", mJpegThumbBuf.ThumbAddrVir, mJpegThumbBuf.ThumbLen, 
    //    mOutStream.mpPack[0].mpAddr0, mOutStream.mpPack[0].mLen0, 
    //    mOutStream.mpPack[0].mpAddr1, mOutStream.mpPack[0].mLen1, 
    //    mOutStream.mpPack[0].mpAddr2, mOutStream.mpPack[0].mLen2);
    //alogd("offset[%jd], size[%d]", offset, len);
    return ret;
}

}; /* namespace EyeseeLinux */
