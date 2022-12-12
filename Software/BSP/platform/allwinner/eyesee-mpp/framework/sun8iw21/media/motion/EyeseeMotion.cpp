/*
********************************************************************************
*                           eyesee framework module
*
*          (c) Copyright 2010-2018, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : EyeseeMotion.cpp
* Version: V1.0
* By     : zing huang
* Date   : 2018-06-28
* Description:
********************************************************************************
*/

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


#include <plat_log.h>
#include <mpi_sys.h>
#include <mpi_demux.h>
#include <mpi_venc.h>
#include <mpi_vdec.h>
#include <mpi_clock.h>
#include <ion_memmanager.h>
#include <cvt_color.h>
#include <aw_ai_mt.h>

#include <Clock_Component.h>
#include <EyeseeMotion.h>

namespace EyeseeLinux {

EyeseeMotion::EyeseeMotion():
	mMotionTrailRet(-1)
{
    mMotionHandle = NULL;
    memset(&mImageYUV, 0, sizeof mImageYUV);
    memset(&mImageBGR, 0, sizeof mImageBGR);
    
    ClearConfig();
}

EyeseeMotion::~EyeseeMotion()
{
    reset(); 
}

status_t EyeseeMotion::setVideoFileSource(const char * url)
{
    status_t opStatus;
    if(0 == access(url, F_OK))
    {
        int fd = open(url, O_RDONLY);
        if(fd < 0)
        {
            aloge("Failed to open file %s(%s)", url, strerror(errno));
            return UNKNOWN_ERROR;
        }
        opStatus = setVideoFileSource(fd, 0, 0x7ffffffffffffffL);
        close(fd);
    }
    else
    {
        aloge("fatal error! open file path[%s] fail!", url);
        opStatus = INVALID_OPERATION;
    }
    return opStatus;
}

status_t EyeseeMotion::setVideoFileSource(int fd, int64_t offset, int64_t length)
{
    Mutex::Autolock _l(mLock);
    if(!(mCurrentState & MEDIA_MOTION_IDLE || mCurrentState == MEDIA_MOTION_STATE_ERROR))
    {
        aloge("called in wrong state %d", mCurrentState);
        return INVALID_OPERATION;
    }

    ERRORTYPE ret;
    bool nSuccessFlag = false;
    
    mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    mDmxChnAttr.mSourceUrl = NULL;
    mDmxChnAttr.mFd = dup(fd);
    mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_AUDIO_TRACK;
    mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_SUBTITLE_TRACK;
    mDmxChn = 0;

    while(mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(mDmxChn, &mDmxChnAttr);
        if(SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create demux channel[%d] success!", mDmxChn);
            break;
        }
        else if(ERR_DEMUX_EXIST == ret)
        {
            alogd("demux channel[%d] is exist, find next!", mDmxChn);
            mDmxChn++;
        }
        else if(ERR_DEMUX_FILE_EXCEPTION == ret)
        {
            aloge("demux detect media file exception!");
            return BAD_FILE;
        }
        else
        {
            alogd("create demux channel[%d] ret[0x%x]!", mDmxChn, ret);
            break;
        }
    }
    if(false == nSuccessFlag)
    {
        mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        return UNKNOWN_ERROR;
    }

    ret = AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &mMediaInfo);
    if(SUCCESS == ret)
    {
        if(  (mMediaInfo.mVideoNum > 0 && mMediaInfo.mVideoIndex >= mMediaInfo.mVideoNum)
          || (mMediaInfo.mAudioNum > 0 && mMediaInfo.mAudioIndex >= mMediaInfo.mAudioNum)
          || (mMediaInfo.mSubtitleNum > 0 && mMediaInfo.mSubtitleIndex >= mMediaInfo.mSubtitleNum)
          )
        {
            alogd("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]",
                mMediaInfo.mVideoNum, mMediaInfo.mVideoIndex, mMediaInfo.mAudioNum, mMediaInfo.mAudioIndex, mMediaInfo.mSubtitleNum, mMediaInfo.mSubtitleIndex);
            return UNKNOWN_ERROR;
        }
    }
    else
    {
        return UNKNOWN_ERROR;
    }

    mCurrentState = MEDIA_MOTION_INITIALIZED;
    return NO_ERROR;
}

std::shared_ptr<CMediaMemory> EyeseeMotion::getMotionJpeg(const MotionPictureParam &jpegParam)
{
    Mutex::Autolock _l(mLock);
    if(mCurrentState != MEDIA_MOTION_INITIALIZED)
    {
        aloge("called in error state 0x%x", mCurrentState);
        return NULL;
    }

    mPictureWidth = jpegParam.mPicWidth;
    mPictureHeight = jpegParam.mPicHeight;
    mStartTimeMs = jpegParam.mStartTimeUs / 1000;
    mSourceFrameNums = jpegParam.mSourceFrameNums;
    mSourceFrameStep = jpegParam.mSourceFrameStep;
    mObjectNums = jpegParam.mObjectNums;

    if(!(mPictureWidth > 0 && mPictureHeight > 0))
    {
        aloge("fatal error! picturesize[%d, %d]", mPictureWidth, mPictureHeight);
        return NULL;
    }

    if(mStartTimeMs > static_cast<int>(mMediaInfo.mDuration) || mStartTimeMs < 0)
    {
        aloge("fatal error, starttime[%d] ms!", mStartTimeMs);
        return NULL;
    }

    if(mSourceFrameNums < 1 || mSourceFrameStep < 0)
    {
        aloge("fatal error, SourceFrameNums[%d], SourceFrameStep[%d]", mSourceFrameNums, mSourceFrameStep);
        return NULL;
    }
    if(jpegParam.mTempBMPFileBasePath.empty())
    {
        mTempBMPFileBasePath = "./tmp_picture/";
    }
    else
    {
        mTempBMPFileBasePath = jpegParam.mTempBMPFileBasePath;
        if(jpegParam.mTempBMPFileBasePath[jpegParam.mTempBMPFileBasePath.size() - 1] != '/')
        {
            mTempBMPFileBasePath += "/";    
        }
    }
    
    prepare();

    seekTo();

    start();

    getMotionTrail();

    stop();

    cvtBGR2YUV(&mImageBGR, &mImageYUV, PIXEL_FORMAT_YUV_PLANAR_420);

    return PictureAsJpeg(mImageYUV);
    
}

std::shared_ptr<CMediaMemory> EyeseeMotion::getMotionBMP(const MotionPictureParam &jpegParam)
{
#if 0
    Mutex::Autolock _l(mLock);
    if(mCurrentState != MEDIA_MOTION_INITIALIZED)
    {
        aloge("called in error state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    mPictureWidth = picWidth;
    mPictureHeight = picHeight;
    mStartTimeMs = startTimeUs / 1000;
    mSourceFrameNums = sourceFrameNums;
    mSourceFrameStep = sourceFrameStep;

    if(!(mPictureWidth > 0 && mPictureHeight > 0))
    {
        aloge("fatal error! picturesize[%d, %d]", mPictureWidth, mPictureHeight);
        return NULL;
    }

    if(mStartTimeMs > mMediaInfo.mDuration || mStartTimeMs < 0)
    {
        aloge("fatal error, starttime[%d] ms!", mStartTimeMs);
        return NULL;
    }

    if(mSourceFrameNums < 1 || mSourceFrameStep < 0)
    {
        aloge("fatal error, SourceFrameNums[%d], SourceFrameStep[%d]", mSourceFrameNums, mSourceFrameStep);
        return NULL;
    }

    prepare();

    seekTo();

    start();

    getMotionTrail();

    stop();

    return PictureAsBMP(mImageBGR);
#else
    aloge("sorry, this version no realize func!");
    return NULL;
#endif    
}

status_t EyeseeMotion::reset()
{
    Mutex::Autolock _l(mLock);

    status_t result = NO_ERROR;
    if(mCurrentState == MEDIA_MOTION_INITIALIZED)
    {
        if(mDmxChn >= 0)
        {
            AW_MPI_DEMUX_DestroyChn(mDmxChn);
            mDmxChn = MM_INVALID_CHN;
        }
    }

    ClearConfig();
    
    //mCurrentState = MEDIA_MOTION_IDLE;

    return result;
}

void EyeseeMotion::ClearConfig()
{
    memset(&mDmxChnAttr, 0, sizeof mDmxChnAttr);
    memset(&mMediaInfo, 0, sizeof mMediaInfo);
    memset(&mVdecChnAttr, 0, sizeof mVdecChnAttr);
    memset(&mClockChnAttr, 0, sizeof mClockChnAttr);

    mDmxChn = MM_INVALID_CHN;
    mVdecChn = MM_INVALID_CHN;
    mClockChn = MM_INVALID_CHN;

    if(mMotionHandle)
    {
		AW_AI_MT_Reset(mMotionHandle);
        AW_AI_MT_UnInit(mMotionHandle);
        mMotionHandle = NULL;
    }
    
    DestroyImage(&mImageYUV);
    DestroyImage(&mImageBGR);
    
    mPictureWidth = 0;
    mPictureHeight = 0;
    mStartTimeMs = 0;
    mSourceFrameNums = 0;
    mSourceFrameStep = 0;
    mObjectNums = 0;

    mCurrentState = MEDIA_MOTION_IDLE;
}
status_t EyeseeMotion::prepare()
{
    status_t result = NO_ERROR;
    
    ERRORTYPE ret;

    //basepath
    if(access(mTempBMPFileBasePath.c_str(), 0) != 0)
    {
        if(mkdir(mTempBMPFileBasePath.c_str(), 0) != 0)
        {
            aloge("fatal error, can not creat directory[%s], %s", mTempBMPFileBasePath.c_str(), strerror(errno));
            return UNKNOWN_ERROR;
        }
    }

    if(mMediaInfo.mVideoNum > 0 && !(mDmxChnAttr.mDemuxDisableTrack & DEMUX_DISABLE_VIDEO_TRACK))
    {
        DEMUX_VIDEO_STREAM_INFO_S *pStreamInfo = &mMediaInfo.mVideoStreamInfo[mMediaInfo.mVideoIndex];
        
        mVdecChnAttr.mType = pStreamInfo->mCodecType;
        mVdecChnAttr.mPicWidth = mPictureWidth;
        mVdecChnAttr.mPicHeight = mPictureHeight;
        mVdecChnAttr.mInitRotation = (ROTATE_E)0;
        mVdecChnAttr.mOutputPixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
        mVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
        mVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 1;

        bool bSuccessFlag = false;
        mVdecChn = 0;
        while(mVdecChn < VDEC_MAX_CHN_NUM)
        {
            ret = AW_MPI_VDEC_CreateChn(mVdecChn, &mVdecChnAttr);
            if(SUCCESS == ret)
            {
                bSuccessFlag = true;
                alogd("create vdec channel[%d] success!", mVdecChn);
                break;
            }
            else if(ERR_VDEC_EXIST == ret)
            {
                alogd("vdec channel[%d] is exist, find next!", mVdecChn);
                mVdecChn++;
            }
            else
            {
                alogd("error, create vdec channel[%d] ret[0x%x]!", mVdecChn, ret);
                break;
            }
            
        }

        if(!bSuccessFlag)
        {
            mVdecChn = MM_INVALID_CHN;
            aloge("fatal error! create vdec channel failed");
            result = UNKNOWN_ERROR;
            goto _err0;
        }

        MPP_CHN_S DmxChn{MOD_ID_DEMUX, 0, mDmxChn};
        MPP_CHN_S VdecChn{MOD_ID_VDEC, 0, mVdecChn};
        AW_MPI_SYS_Bind(&DmxChn, &VdecChn);
        mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_VIDEO;
    }

    if(mDmxChn >= 0)
    {
        bool bSuccessFlag = false;
        mClockChn = 0;
        while(mClockChn < CLOCK_MAX_CHN_NUM)
        {
            ret = AW_MPI_CLOCK_CreateChn(mClockChn, &mClockChnAttr);
            if(SUCCESS == ret)
            {
                bSuccessFlag = true;
                alogd("creat clock channel[%d] success!", mClockChn);
                break;
            }
            else if(ERR_CLOCK_EXIST == ret)
            {
                alogd("clock channel[%d] is exist, find next!", mClockChn);
                ++mClockChn;
            }
            else
            {
                alogd("creat clock channel[%d] ret[0x%x]", mClockChn, ret);
                break;
            }
        }

        if(!bSuccessFlag)
        {
            mClockChn = MM_INVALID_CHN;
            aloge("fatal error! create clock channel fail!");
            result = UNKNOWN_ERROR;
            goto _err0;
        }

        MPP_CHN_S ClockChn{MOD_ID_CLOCK, 0, mClockChn};
        MPP_CHN_S DmxChn{MOD_ID_DEMUX, 0, mDmxChn};
        AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
        
    }

    mLpMtParam.Width = mPictureWidth;
    mLpMtParam.Height = mPictureHeight;
	mLpMtParam.bAntishakeEnable = true;
    mMotionHandle = AW_AI_MT_Init(&mLpMtParam);
    if(!mMotionHandle)
    {
        aloge("init mt lib fail");
        result = UNKNOWN_ERROR;
        goto _err0;
    }

    CreateImage(&mImageYUV, mLpMtParam.Width, mLpMtParam.Height, 3, PIXEL_FORMAT_YVU_PLANAR_420);
    CreateImage(&mImageBGR, mLpMtParam.Width, mLpMtParam.Height, 3, PIXEL_FORMAT_BGR_888);

    mCurrentState = MEDIA_MOTION_PREPARED;

_err0:
    return result;
}

status_t EyeseeMotion::start()
{
    status_t opStatus = NO_ERROR;

    if(mVdecChn >= 0)
    {
        //VDEC_DECODE_FRAME_PARAM_S DecodeParam;
        //DecodeParam.mDecodeFrameNum = (mSourceFrameNums -1) * mSourceFrameStep + mSourceFrameNums;
        //AW_MPI_VDEC_StartRecvStreamEx(mVdecChn, &DecodeParam);
        AW_MPI_VDEC_StartRecvStream(mVdecChn);
    }

    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Start(mDmxChn);
    }

    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Start(mClockChn);
    }

    mCurrentState = MEDIA_MOTION_STARTED;
    
    return opStatus;
}

status_t EyeseeMotion::stop()
{
    status_t opStatus = NO_ERROR;

    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Stop(mClockChn);
    }

    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Stop(mDmxChn);
    }

    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_StopRecvStream(mVdecChn);
    }

    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_DestroyChn(mVdecChn);
        mVdecChn = MM_INVALID_CHN;
    }
    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_DestroyChn(mDmxChn);
        mDmxChn = MM_INVALID_CHN;
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(mClockChn);
        mClockChn = MM_INVALID_CHN;
    }

    mCurrentState = MEDIA_MOTION_STOPPED;

    return opStatus;
}

status_t EyeseeMotion::seekTo()
{
    status_t opStatus = NO_ERROR;

    if(mDmxChn >= 0 && mStartTimeMs > 0)
    {
        AW_MPI_DEMUX_Seek(mDmxChn, mStartTimeMs);
        alogd("the demux had set %d ms!", mStartTimeMs);
    }
    return opStatus;
}

status_t EyeseeMotion::getMotionTrail()
{
    mMotionTrailRet = -1;
    VIDEO_FRAME_INFO_S tVideoFrameInfo;
    const int nFramSum = (mSourceFrameNums -1) * mSourceFrameStep + mSourceFrameNums;
    int nSendFrameCounts = 0;
	if(nFramSum <= 20){
		aloge("The voide is too short,video frame number nFramSum %d <= 20,",nFramSum);
		mMotionTrailRet = -1;
		return NO_ERROR;
	}
    for(int i = 0; i < nFramSum; i++)
    {
        ERRORTYPE ret = AW_MPI_VDEC_GetImage(mVdecChn, &tVideoFrameInfo, 500);
        if(SUCCESS == ret)
        {
        	if(i > nFramSum / 10 && i < (nFramSum - nFramSum / 10) )
        	{
	            if(0 == mSourceFrameStep || 0 == i % mSourceFrameStep)
	            {
	                assert(MM_PIXEL_FORMAT_YVU_PLANAR_420 == tVideoFrameInfo.VFrame.mPixelFormat);
	                const int size = mLpMtParam.Width * mLpMtParam.Height;
	                memcpy(mImageYUV.mpData, tVideoFrameInfo.VFrame.mpVirAddr[0], size);
	                memcpy(mImageYUV.mpData + size, tVideoFrameInfo.VFrame.mpVirAddr[1], size / 4);
	                memcpy(mImageYUV.mpData + size + size / 4, tVideoFrameInfo.VFrame.mpVirAddr[2], size / 4);

	                cvtYUV2BGR(&mImageYUV, &mImageBGR);

	                //AW_AI_MT_SendFrame(mMotionHandle, &mImageBGR);
	                char tmpFileName[32] = {0};
	                sprintf(tmpFileName, "%d.bmp", nSendFrameCounts++);
	                std::string TempFilePathName = mTempBMPFileBasePath + tmpFileName;
	                SaveBMP(&mImageBGR, const_cast<char*>(TempFilePathName.c_str()));
	                AW_AI_MT_SetFrame(mMotionHandle, TempFilePathName.c_str());
	            }
        	}
            AW_MPI_VDEC_ReleaseImage(mVdecChn, &tVideoFrameInfo);
        }
        else
        {
            aloge("why not get frame from vdec, maybe EOF [0x%x]", ret);
        }
    }

    alogd("__send frame to MT, count[%d]", nSendFrameCounts);

	mMotionTrailRet = AW_AI_MT_GenMotionTrail(mMotionHandle, &mImageBGR);
    return NO_ERROR;
}


int EyeseeMotion::GetmMotionTrailRet()
{
	return mMotionTrailRet;
}

std::shared_ptr<CMediaMemory> EyeseeMotion::PictureAsJpeg(IMAGE_DATA_I &ImageYUV)
{
    VENC_CHN VecChn = 0;
    VENC_CHN_ATTR_S VecChnAttr;
    memset(&VecChnAttr, 0, sizeof VecChnAttr);
    VecChnAttr.VeAttr.Type = PT_JPEG;
    VecChnAttr.VeAttr.AttrJpeg.MaxPicWidth = 0;
    VecChnAttr.VeAttr.AttrJpeg.MaxPicHeight = 0;
    VecChnAttr.VeAttr.AttrJpeg.BufSize = ((((ImageYUV.mWidth * ImageYUV.mHeight * 3) >> 2) + 1023) >> 10) << 10;
    VecChnAttr.VeAttr.AttrJpeg.bByFrame = TRUE;
    VecChnAttr.VeAttr.AttrJpeg.PicWidth = ImageYUV.mWidth;
    VecChnAttr.VeAttr.AttrJpeg.PicHeight = ImageYUV.mHeight;
    VecChnAttr.VeAttr.AttrJpeg.bSupportDCF = FALSE;
    VecChnAttr.VeAttr.MaxKeyInterval = 1;
    VecChnAttr.VeAttr.SrcPicWidth = ImageYUV.mWidth;
    VecChnAttr.VeAttr.SrcPicHeight = ImageYUV.mHeight;
    VecChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    VecChnAttr.VeAttr.PixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    
    ERRORTYPE ret;
    bool bSuccessFlag = false;
    while(VecChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(VecChn, &VecChnAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("create venc channel[%d] success!", VecChn);
            break;
        }
        else if(ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", VecChn);
            VecChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!",  VecChn, ret);
            break;
        }      
    }
    if(!bSuccessFlag)
    {
        aloge("fatal error! create venc channel fail!");
        return NULL;
    }

    VIDEO_FRAME_INFO_S tVideoFrameInfo;
    memset(&tVideoFrameInfo, 0, sizeof tVideoFrameInfo);
    tVideoFrameInfo.mId = 0;
    tVideoFrameInfo.VFrame.mWidth = ImageYUV.mWidth;
    tVideoFrameInfo.VFrame.mHeight = ImageYUV.mHeight;
    tVideoFrameInfo.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    tVideoFrameInfo.VFrame.mOffsetLeft = 0;
    tVideoFrameInfo.VFrame.mOffsetRight = ImageYUV.mWidth;
    tVideoFrameInfo.VFrame.mOffsetTop = 0;
    tVideoFrameInfo.VFrame.mOffsetBottom = ImageYUV.mHeight;
    
    const int size = ImageYUV.mWidth * ImageYUV.mHeight;
    for(int i =0 ; i < 2; i++)
    {
        tVideoFrameInfo.VFrame.mpVirAddr[i] = ion_allocMem(size);
        tVideoFrameInfo.VFrame.mPhyAddr[i] = ion_getMemPhyAddr(tVideoFrameInfo.VFrame.mpVirAddr[i]);
    }
    memcpy(tVideoFrameInfo.VFrame.mpVirAddr[0], ImageYUV.mpData, size);
    ion_flushCache(tVideoFrameInfo.VFrame.mpVirAddr[0], size);
    
    unsigned char *ptr_uv = static_cast<unsigned char *>(tVideoFrameInfo.VFrame.mpVirAddr[1]);
    unsigned char *ptr_u = ImageYUV.mpData + size;
    unsigned char *ptr_v = ImageYUV.mpData + size + size / 4;
    for(int i = 0; i < size / 4; i++)
    {
        *ptr_uv++ = *ptr_v++;
        *ptr_uv++ = *ptr_u++;
    }
    ion_flushCache(tVideoFrameInfo.VFrame.mpVirAddr[1], size);

    VENC_RECV_PIC_PARAM_S RecvParam;
    RecvParam.mRecvPicNum = 1;
    AW_MPI_VENC_StartRecvPicEx(VecChn, &RecvParam);

    AW_MPI_VENC_SendFrame(VecChn, &tVideoFrameInfo, 500);

    std::shared_ptr<CMediaMemory> spJpegBuf;
    VENC_STREAM_S JpegStream;
    VENC_PACK_S Jpeg_pack;
    JpegStream.mPackCount = 1;
    JpegStream.mpPack = &Jpeg_pack;
    if(SUCCESS == AW_MPI_VENC_GetStream(VecChn, &JpegStream, 500))
    {
        const int JpegSize = JpegStream.mpPack[0].mLen0 + JpegStream.mpPack[0].mLen1;        
        spJpegBuf = std::make_shared<CMediaMemory>(JpegSize);
        char *ptr = (char *)spJpegBuf->getPointer();
        memcpy(ptr, JpegStream.mpPack[0].mpAddr0, JpegStream.mpPack[0].mLen0);
        ptr += JpegStream.mpPack[0].mLen0;
        memcpy(ptr, JpegStream.mpPack[0].mpAddr1, JpegStream.mpPack[0].mLen1);

        if(AW_MPI_VENC_ReleaseStream(VecChn, &JpegStream) != SUCCESS)
        {
            aloge("jpeg stream data return fail!");
        }
    }
    else
    {
        aloge("get jpeg stream data fail!"); 
    }

    AW_MPI_VENC_StopRecvPic(VecChn);

    for(int i = 0; i < 2; i++)
    {
        ion_freeMem(tVideoFrameInfo.VFrame.mpVirAddr[i]);
    }

    AW_MPI_VENC_DestroyChn(VecChn);
    
    return spJpegBuf;
}

std::shared_ptr<CMediaMemory> EyeseeMotion::PictureAsBMP(IMAGE_DATA_I &ImageBGR)
{
    aloge("sorry, this version no realize func!");
    return NULL;
}

};

