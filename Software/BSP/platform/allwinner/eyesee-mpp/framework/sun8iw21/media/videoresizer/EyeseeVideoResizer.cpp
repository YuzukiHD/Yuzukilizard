/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : EyeseeVideoResizer.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-XIAN Team
  Created       : 2017/12/29
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "EyeseeVideoResizer"
#include <plat_log.h>
#include <plat_type.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

#include "EyeseeVideoResizer.h"
#include "mpi_sys.h"
#include "mpi_vdec.h"
#include "mpi_clock.h"
#include "mpi_demux.h"
#include "mpi_venc.h"
#include "mpi_mux.h"
#include "record_writer.h"

static void ShowMediaInfo(DEMUX_MEDIA_INFO_S* pMediaInfo)
{
    alogd("======== MediaInfo ===========");
    alogd("FileSize = %d Duration = %d ms", pMediaInfo->mFileSize, pMediaInfo->mDuration);
    alogd("TypeMap H264(%d) H265(%d) MJPEG(%d) JPEG(%d) AAC(%d) MP3(%d)", PT_H264, PT_H265, PT_MJPEG, PT_JPEG, PT_AAC, PT_MP3);
    // Video Or Jpeg Info
    for (int i = 0; i < pMediaInfo->mVideoNum; i++)
    {//VideoType: PT_JPEG, PT_H264, PT_H265, PT_MJPEG
        alogd("Video: CodecType = %d wWidth = %d wHeight = %d nFrameRate = %d AvgBitsRate = %d MaxBitsRate = %d"
            ,pMediaInfo->mVideoStreamInfo[i].mCodecType
            ,pMediaInfo->mVideoStreamInfo[i].mWidth
            ,pMediaInfo->mVideoStreamInfo[i].mHeight
            ,pMediaInfo->mVideoStreamInfo[i].mFrameRate
            ,pMediaInfo->mVideoStreamInfo[i].mAvgBitsRate
            ,pMediaInfo->mVideoStreamInfo[i].mMaxBitsRate
            );
    }

    // Audio Info
    for (int i = 0; i < pMediaInfo->mAudioNum; i++)
    {
        alogd("Audio: CodecType = %d nChaninelNum = %d nBitsPerSample = %d nSampleRate = %d AvgBitsRate = %d MaxBitsRate = %d"
            ,pMediaInfo->mAudioStreamInfo[i].mCodecType
            ,pMediaInfo->mAudioStreamInfo[i].mChannelNum
            ,pMediaInfo->mAudioStreamInfo[i].mBitsPerSample
            ,pMediaInfo->mAudioStreamInfo[i].mSampleRate
            ,pMediaInfo->mAudioStreamInfo[i].mAvgBitsRate
            ,pMediaInfo->mAudioStreamInfo[i].mMaxBitsRate
            );
    }

    return;
}


using namespace std;
namespace EyeseeLinux {

ERRORTYPE EyeseeVideoResizer::MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    EyeseeVideoResizer* pVideoResizer = (EyeseeVideoResizer*)cookie;
    pVideoResizer->notify(pChn, event, pEventData);

    if (NULL != pVideoResizer->mOnMessageListener)
    {
        pVideoResizer->mOnMessageListener->onMessage(pVideoResizer, pChn, event, pEventData);
    }

    return SUCCESS;
}

EyeseeVideoResizer::EyeseeVideoResizer()
{
    ALOGV("EyeseeVideoResizer construct");
    mOnErrorListener = NULL;
    mOnMessageListener = NULL;

    memset(&mOutDest, 0, sizeof(ResizerOutputDest));
    mOutDest.fd           = -1;
    mOutDest.videoEncFmt  = PT_H264;
    mOutDest.mediaFileFmt = MEDIA_FILE_FORMAT_DEFAULT;
    mDstVideoEncProfile   = 2;
    mDstRcMode            = VENC_RC_MODE_BUTT;
    mDstMediaDuration     = 0;

    mDmxChn   = MM_INVALID_CHN;
    mClockChn = MM_INVALID_CHN;
    mVdecChn  = MM_INVALID_CHN;
    mVencChn  = MM_INVALID_CHN;
    mMuxGrp   = MM_INVALID_CHN;
    mbSeekStart = false;

    mBufList.resize(ENC_BACKUP_BUFFER_NUM);
    for (std::list<std::shared_ptr<VREncBuffer>>::iterator it = mBufList.begin(); it != mBufList.end(); ++it)
    {
        memset(&*it, 0, sizeof(std::list<std::shared_ptr<VREncBuffer>>));
    }

    mpDoVRThread = new DoVRThread(this);
    mpDoVRThread->startThread();

    mCurrentState = VIDEO_RESIZER_IDLE;
}

EyeseeVideoResizer::~EyeseeVideoResizer()
{
    ALOGV("~EyeseeVideoResizer");

    mDmxChn   = MM_INVALID_CHN;
    mClockChn = MM_INVALID_CHN;
    mVdecChn  = MM_INVALID_CHN;
    mVencChn  = MM_INVALID_CHN;
    mMuxGrp   = MM_INVALID_CHN;

    if (mpDoVRThread != NULL)
    {
        mpDoVRThread->stopThread();
        delete mpDoVRThread;
    }
    mCurrentState = VIDEO_RESIZER_IDLE;
}

status_t EyeseeVideoResizer::setDataSource(const char *url)
{
    alogv("setDataSource, url=%s", url);
    status_t opStatus;
    if (access(url, F_OK) == 0)
    {
        int fd = open(url, O_RDONLY);
        if (fd < 0)
        {
            aloge("Failed to open file %s(%s)", url, strerror(errno));
            return UNKNOWN_ERROR;
        }
        opStatus = setDataSource(fd);
        close(fd);
    }
    else
    {
        aloge("fatal error! open file path[%s] fail!", url);
        opStatus = INVALID_OPERATION;
    }

    if (NO_ERROR != opStatus)
    {
        if (NULL != mOnErrorListener)
            mOnErrorListener->onError(this, 0, 0);
    }

    return opStatus;
}

status_t EyeseeVideoResizer::setDataSource(int fd)
{
    return setDataSource(fd, 0, 0x7ffffffffffffffL);
}

status_t EyeseeVideoResizer::setDataSource(int fd, int64_t offset, int64_t length)
{
    Mutex::Autolock _l(mLock);
    if (!(mCurrentState & (VIDEO_RESIZER_IDLE | VIDEO_RESIZER_STATE_ERROR)))
    {
        aloge("called in wrong state %d", mCurrentState);
        return INVALID_OPERATION;
    }

    ERRORTYPE ret;
    bool nSuccessFlag = false;
    mDmxChnAttr.mStreamType        = STREAMTYPE_LOCALFILE;
    mDmxChnAttr.mSourceType        = SOURCETYPE_FD;
    mDmxChnAttr.mSourceUrl         = NULL;
    mDmxChnAttr.mFd                = fd;
    mDmxChnAttr.mDemuxDisableTrack = DEMUX_DISABLE_SUBTITLE_TRACK;
    mDmxChn = 0;
    while (mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(mDmxChn, &mDmxChnAttr);
        if(SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogv("create demux channel[%d] success!", mDmxChn);
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
    if (false == nSuccessFlag)
    {
        mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        return UNKNOWN_ERROR;
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie   = (void*)this;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_DEMUX_RegisterCallback(mDmxChn, &cbInfo);

    mCurrentState = VIDEO_RESIZER_INITIALIZED;
    return NO_ERROR;
}

void EyeseeVideoResizer::setVideoSize(int32_t width, int32_t height)
{
    Mutex::Autolock _l(mLock);
    mOutDest.width  = width;
    mOutDest.height = height;
}

status_t EyeseeVideoResizer::setVideoEncFmt(PAYLOAD_TYPE_E format)
{
    Mutex::Autolock _l(mLock);
    mOutDest.videoEncFmt = format;
    return NO_ERROR;
}

status_t EyeseeVideoResizer::addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_E format, const char* url, int FallocateLen, bool callback_out_flag)
{
    if (NULL == url && callback_out_flag == false)
    {
        aloge("fatal error! setOutputPath is NULL while no callback!");
        return UNKNOWN_ERROR;
    }
    alogd("setOutputPath %s",url);
    strcpy(mDstFilePath, url);

    Mutex::Autolock _l(mLock);
    if (MEDIA_FILE_FORMAT_RAW == format)
    {
        mOutDest.mediaFileFmt = format;
        if (callback_out_flag == false)
        {
            alogw("set callback flag while output frame is MEDIA_FILE_FORMAT_RAW");
        }
    }
    else if (MEDIA_FILE_FORMAT_MP4 == format)
    {
        mOutDest.mediaFileFmt = format;
        if (callback_out_flag == true)
        {
            alogw("ignore callback flag while output frame is MEDIA_FILE_FORMAT_MP4");
        }
    }
    else
    {
        aloge("fatal error! set unsupport media format %d !",format);
        alogd("using default fmt mp4!");
        mOutDest.mediaFileFmt = MEDIA_FILE_FORMAT_DEFAULT;
    }

    return NO_ERROR;
}

status_t EyeseeVideoResizer::setFrameRate(int32_t framerate)
{
    mOutDest.frameRate = framerate;
    return NO_ERROR;
}

status_t EyeseeVideoResizer::setBitRate(int32_t bitrate)
{
    mOutDest.bitRate = bitrate;
    return NO_ERROR;
}

status_t EyeseeVideoResizer::prepare()
{
    alogv("prepare");
    Mutex::Autolock _l(mLock);
    status_t result = NO_ERROR;
    if (!(mCurrentState & (VIDEO_RESIZER_INITIALIZED | VIDEO_RESIZER_STOPPED)))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    DEMUX_MEDIA_INFO_S DemuxMediaInfo = {0};
    AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &DemuxMediaInfo);
    if (  (DemuxMediaInfo.mVideoNum >0 && DemuxMediaInfo.mVideoIndex>=DemuxMediaInfo.mVideoNum)
       || (DemuxMediaInfo.mAudioNum >0 && DemuxMediaInfo.mAudioIndex>=DemuxMediaInfo.mAudioNum)
       || (DemuxMediaInfo.mSubtitleNum >0 && DemuxMediaInfo.mSubtitleIndex>=DemuxMediaInfo.mSubtitleNum)
       )
    {
        aloge("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]"
              ,DemuxMediaInfo.mVideoNum
              ,DemuxMediaInfo.mVideoIndex
              ,DemuxMediaInfo.mAudioNum
              ,DemuxMediaInfo.mAudioIndex
              ,DemuxMediaInfo.mSubtitleNum
              ,DemuxMediaInfo.mSubtitleIndex);
        AW_MPI_DEMUX_DestroyChn(mDmxChn);
        AW_MPI_SYS_Exit();
        return UNKNOWN_ERROR;
    }
    ShowMediaInfo(&DemuxMediaInfo);
    memcpy(&mDemuxMediaInfo, &DemuxMediaInfo, sizeof(DEMUX_MEDIA_INFO_S));

    if (DemuxMediaInfo.mVideoNum <= 0)
    {
        aloge("fatal error! can't find video track, exit!");
        AW_MPI_DEMUX_DestroyChn(mDmxChn);
        AW_MPI_SYS_Exit();
        return UNKNOWN_ERROR;
    }

    // disable audio track if no audio in src file
    if (DemuxMediaInfo.mAudioNum < 0)
    {
        AW_MPI_DEMUX_GetChnAttr(mDmxChn, &mDmxChnAttr);
        mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_AUDIO_TRACK;
        AW_MPI_DEMUX_SetChnAttr(mDmxChn, &mDmxChnAttr);
    }

    // config src file video info
    mSrcVideoWidth  = DemuxMediaInfo.mVideoStreamInfo[0].mWidth;
    mSrcVideoHeight = DemuxMediaInfo.mVideoStreamInfo[0].mHeight;
    mSrcCodecType   = DemuxMediaInfo.mVideoStreamInfo[0].mCodecType;
    mSrcFrameRate   = DemuxMediaInfo.mVideoStreamInfo[0].mFrameRate;

    // CLOCK
    if(FALSE == create_CLOCK_CHN())
    {
        aloge("fatal error! create clock channel fail!");
        mClockChn = MM_INVALID_CHN;
        return UNKNOWN_ERROR;
    }
    MPP_CHN_S DmxChn   = {MOD_ID_DEMUX, 0, mDmxChn};
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, mClockChn};
    AW_MPI_SYS_Bind(&ClockChn, &DmxChn);

    // VDEC
    config_VDEC_CHN_ATTR_S();
    if (FALSE == create_VDEC_CHN())
    {
        aloge("fatal error! create vdec channel fail!");
        mVdecChn = MM_INVALID_CHN;
        return UNKNOWN_ERROR;
    }
    MPPCallbackInfo VdecCbInfo;
    VdecCbInfo.cookie   = (void*)this;
    VdecCbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VDEC_RegisterCallback(mVdecChn, &VdecCbInfo);
    MPP_CHN_S VdecChn   = {MOD_ID_VDEC,  0, mVdecChn};
    AW_MPI_SYS_Bind(&DmxChn, &VdecChn);

    // VENC
    config_VENC_CHN_ARRT_S();
    if(FALSE == create_VENC_CHN())
    {
        aloge("fatal error! create venc channel fail!");
        mVencChn = MM_INVALID_CHN;
        return UNKNOWN_ERROR;
    }
    MPP_CHN_S VencChn = {MOD_ID_VENC, 0, mVencChn};
    AW_MPI_SYS_Bind(&VdecChn, &VencChn);

    // MUX
    config_MUX_CHN_ARRT_S();
    if(FALSE == create_MUX_GRP_CHN())
    {
        aloge("fatal error! create mux grp channel fail!");
        mMuxGrp = MM_INVALID_CHN;
        return UNKNOWN_ERROR;
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie   = (void*)this;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_MUX_RegisterCallback(mMuxGrp, &cbInfo);
    VencHeaderData H264SpsPpsInfo;
    AW_MPI_VENC_GetH264SpsPpsInfo(mVencChn, &H264SpsPpsInfo);
    AW_MPI_MUX_SetH264SpsPpsInfo(mMuxGrp, &H264SpsPpsInfo);
    if (FALSE == create_MUX_CHN())
    {
        aloge("fatal error! create mux channel fail!");
        mMuxChn = MM_INVALID_CHN;
        return UNKNOWN_ERROR;
    }
    MPP_CHN_S MuxGrp = {MOD_ID_MUX,  0, mMuxGrp};
    ERRORTYPE eError = AW_MPI_SYS_Bind(&VencChn, &MuxGrp);
    if (eError != SUCCESS)
    {
        aloge("fatal error! bind Venc & Muxer failed!");
        return UNKNOWN_ERROR;
    }

    // Bind Demux and Muxer audio
    if (DemuxMediaInfo.mAudioNum > 0)
    {
        eError = AW_MPI_SYS_Bind(&DmxChn, &MuxGrp);
        if (eError != SUCCESS)
        {
            aloge("fatal error! bind Demux & Muxer failed!");
            return UNKNOWN_ERROR;
        }
    }

    mCurrentState = VIDEO_RESIZER_PREPARED;
    return NO_ERROR;
}

status_t EyeseeVideoResizer::start_l()
{
#if 1
    alogd("mSrcVideoWidth %d",mSrcVideoWidth);
    alogd("mSrcVideoHeight %d",mSrcVideoHeight);
    alogd("mSrcCodecType %d",mSrcCodecType);
    alogd("mSrcFrameRate %d",mSrcFrameRate);


    alogd("width %d",mOutDest.width);
    alogd("height %d",mOutDest.height);
    alogd("frameRate %d",mOutDest.frameRate);
    alogd("bitRate %d",mOutDest.bitRate);
    alogd("mDstVideoEncProfile %d",mDstVideoEncProfile);
    alogd("mDstRcMode %d",mDstRcMode);
    alogd("mDstMediaDuration %d",mDstMediaDuration);

    alogd("mDstMediaDuration %d",mDstMediaDuration);
    alogd("videoEncFmt %d",mOutDest.videoEncFmt);
    alogd("mMediaFileFmt %d",mOutDest.mediaFileFmt);
#endif

    if (!(mCurrentState & (VIDEO_RESIZER_PREPARED | VIDEO_RESIZER_PAUSED | VIDEO_RESIZER_PLAYBACK_COMPLETE)))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    if (mbSeekStart)
    {
        seekTo_l(mSeekStartPosition);
        mbSeekStart = false;
    }
    else
    {
        if(mCurrentState == VIDEO_RESIZER_PLAYBACK_COMPLETE)
        {
            seekTo_l(0);
        }
    }

    // Start
    if (mClockChn >= 0)
    {
        AW_MPI_CLOCK_Start(mClockChn);
    }
    if (mMuxGrp >= 0)
    {
        AW_MPI_MUX_StartGrp(mMuxGrp);
    }
    if (mVencChn >= 0)
    {
        AW_MPI_VENC_StartRecvPic(mVencChn);
    }
    if (mVdecChn >= 0)
    {
        AW_MPI_VDEC_StartRecvStream(mVdecChn);
    }
    if (mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Start(mDmxChn);
    }
    mCurrentState = VIDEO_RESIZER_STARTED;
    return NO_ERROR;
}
status_t EyeseeVideoResizer::start()
{
    alogd("start");
    Mutex::Autolock _l(mLock);
    return start_l();
}

status_t EyeseeVideoResizer::stop_l()
{

    if(!(mCurrentState & (VIDEO_RESIZER_PREPARED|VIDEO_RESIZER_STARTED|VIDEO_RESIZER_PAUSED|VIDEO_RESIZER_PLAYBACK_COMPLETE)))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    // stop
    if (mMuxGrp >= 0)
    {
        AW_MPI_MUX_StopGrp(mMuxGrp);
    }
    if (mVencChn >= 0)
    {
        AW_MPI_VENC_StopRecvPic(mVencChn);
    }
    if (mVdecChn >= 0)
    {
        AW_MPI_VDEC_StopRecvStream(mVdecChn);
    }
    if (mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Stop(mDmxChn);
    }
    if (mClockChn >= 0)
    {
        AW_MPI_CLOCK_Stop(mClockChn);
    }

    // Destroy
    if (mMuxGrp >= 0)
    {
        AW_MPI_MUX_DestroyGrp(mMuxGrp);
    }
    if (mVencChn >= 0)
    {
        AW_MPI_VENC_DestroyChn(mVencChn);
    }
    if (mVdecChn >= 0)
    {
        AW_MPI_VDEC_DestroyChn(mVdecChn);
    }
    if (mDmxChn >= 0)
    {
        AW_MPI_DEMUX_DestroyChn(mDmxChn);
    }
    if (mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(mClockChn);
    }



    mCurrentState = VIDEO_RESIZER_STOPPED;
    return NO_ERROR;
}
status_t EyeseeVideoResizer::stop()
{
    alogd("stop");
    Mutex::Autolock _l(mLock);
    return stop_l();
}

status_t EyeseeVideoResizer::pause_l()
{
    alogv("pause_l");
    status_t opStatus = NO_ERROR;
    if(!(mCurrentState & VIDEO_RESIZER_STARTED))
    {
        aloge("fatal error! called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Pause(mDmxChn);
    }
    if(mVdecChn>=0)
    {
        AW_MPI_VDEC_Pause(mVdecChn);
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Pause(mClockChn);
    }

    mCurrentState = VIDEO_RESIZER_PAUSED;
    return opStatus;
}

status_t EyeseeVideoResizer::pause()
{
    alogv("pause");
    Mutex::Autolock _l(mLock);
    return pause_l();
}

status_t EyeseeVideoResizer::seekTo_l(int msec)
{
    alogd("seekTo_l: %d(msec)", msec);
    status_t opStatus = NO_ERROR;
    if (mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Seek(mDmxChn, msec);
        if (mVdecChn >= 0)
        {
            AW_MPI_VDEC_SetStreamEof(mVdecChn, 0);
        }
    }
    if (mVdecChn >= 0)
    {
        AW_MPI_VDEC_Seek(mVdecChn);
    }
    if (mClockChn >= 0)
    {
        AW_MPI_CLOCK_Seek(mClockChn);
    }
    return opStatus;
}

status_t EyeseeVideoResizer::seekTo(int msec)
{
    alogv("seekTo %d msec",msec);
    if (!(mCurrentState & (VIDEO_RESIZER_PREPARED|VIDEO_RESIZER_STARTED|VIDEO_RESIZER_PAUSED|VIDEO_RESIZER_PLAYBACK_COMPLETE)))
    {
        alogw("can't seek in current state[0x%x]", mCurrentState);
        return INVALID_OPERATION;
    }

    EyeseeMessage msg;
    msg.mMsgType = DoVRThread::MsgType_SeekTo;
    msg.mPara0   = msec;
    mpDoVRThread->mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t EyeseeVideoResizer::DoSeekTo(int msec)
{
    alogv("DoSeekTo %d msec", msec);
    Mutex::Autolock _l(mLock);
    status_t opStatus = NO_ERROR;
    if (mCurrentState & (VIDEO_RESIZER_PREPARED|VIDEO_RESIZER_PLAYBACK_COMPLETE|VIDEO_RESIZER_PAUSED))
    {
        alogd("mCurrentState %d", mCurrentState);
        mSeekStartPosition = msec;
        mbSeekStart = true;
    }
    else if (mCurrentState == VIDEO_RESIZER_STARTED)
    {
        mbSeekStart = false;
        pause_l();
        seekTo_l(msec);
        start_l();
    }
    else
    {
        aloge("fatal error! illegal state[0x%x] can't come here!", mCurrentState);
        opStatus = UNKNOWN_ERROR;
    }
    return opStatus;
}

status_t EyeseeVideoResizer::reset()
{
    // TO DO
    return NO_ERROR;
}

status_t EyeseeVideoResizer::getDuration(int *msec)
{
    // TO DO
    return NO_ERROR;
}

status_t EyeseeVideoResizer::notify(MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    if (MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
        case MPP_EVENT_RECORD_DONE:
            {
                int muxerId = *(int*)pEventData;
                alogd("file done, mux_id=%d", muxerId);
            }
            break;

        case MPP_EVENT_NEED_NEXT_FD:
            {
                alogd("mux need next fd");
            }
            break;

        case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                pushOneBsFrame((CDXRecorderBsInfo*)pEventData);
                break;
            }
            break;

        default:
            break;
        }
    }
    else if (MOD_ID_DEMUX == pChn->mModId)
    {
        switch(event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("get the EOF flag in Callback");

            break;

        default:
            break;
        }
    }

    return 0;
}

status_t EyeseeVideoResizer::pushOneBsFrame(CDXRecorderBsInfo *frame)
{
    int total_bs_size=0;
    for (int i=0; i<frame->bs_count; i++)
    {
        total_bs_size+=frame->bs_size[i];
    }
    if(total_bs_size != frame->total_size)
    {
        aloge("fatal error! BsFrameSize[%d]!=[%d], check code!", total_bs_size, frame->total_size);
    }

    if (frame->mode == 0)
    {
        RawPacketHeader *pRawPacketHeader = (RawPacketHeader*)frame->bs_data[0];
        if(pRawPacketHeader->size != frame->total_size - frame->bs_size[0])
        {
            aloge("fatal error! BsFrameDataSize[%d]!=[%d], check code!", pRawPacketHeader->size, frame->total_size - frame->bs_size[0]);
        }
        int streamType = pRawPacketHeader->stream_type;
    }
    else if(frame->mode == 1)
    {
        TSPacketHeader *pTsPacketHeader = (TSPacketHeader*)frame->bs_data[0];
        if(pTsPacketHeader->size != frame->total_size - frame->bs_size[0])
        {
            aloge("fatal error! BsFrameDataSize[%d]!=[%d], check code!", pTsPacketHeader->size, frame->total_size - frame->bs_size[0]);
        }
    }

    Mutex::Autolock _l(mLock);

    std::shared_ptr<VREncBuffer> mVREncBuffer;

    if(0 == frame->mode)
    {
        RawPacketHeader *pRawPacketHeader = (RawPacketHeader*)frame->bs_data[0];
        if(frame->total_size > 0)
        {
            mVREncBuffer = std::make_shared<VREncBuffer>(frame->total_size);
            mVREncBuffer->total_size = frame->total_size;
            mVREncBuffer->stream_type = pRawPacketHeader->stream_type;
            mVREncBuffer->data_size = pRawPacketHeader->size;
            mVREncBuffer->pts = pRawPacketHeader->pts;
            mVREncBuffer->CurrQp = pRawPacketHeader->CurrQp;
            mVREncBuffer->avQp = pRawPacketHeader->avQp;
            mVREncBuffer->nGopIndex = pRawPacketHeader->nGopIndex;
            mVREncBuffer->nFrameIndex = pRawPacketHeader->nFrameIndex;
            mVREncBuffer->nTotalIndex = pRawPacketHeader->nTotalIndex;
            char *ptr = (char *)mVREncBuffer->getPointer();
            if(NULL == ptr)
            {
                aloge("fatal error! malloc fail!");
            }
            int nCopyLen = 0;
            for(int i=1; i<frame->bs_count; i++)
            {
                memcpy(ptr+nCopyLen, frame->bs_data[i], frame->bs_size[i]);
                nCopyLen += frame->bs_size[i];
            }
            mBufList.push_back(mVREncBuffer);
        }
        else
        {
            aloge("fatal error! BsFrameDataSize == 0!");
        }
    }
    else
    {
        aloge("fatal error! unsupport callback mode[%d]!", frame->mode);
    }
    return NO_ERROR;
}


std::shared_ptr<VREncBuffer> EyeseeVideoResizer::getPacket()
{
    if(mBufList.empty())
    {
        return NULL;
    }
    std::shared_ptr<VREncBuffer> tmpVREncBuffer = mBufList.front();
    mBufList.pop_front();
    return tmpVREncBuffer;
}

void EyeseeVideoResizer::setOnErrorListener(OnErrorListener *pListener)
{
    mOnErrorListener = pListener;
}

void EyeseeVideoResizer::setOnMessageListener(OnMessageListener *pListener)
{
    mOnMessageListener = pListener;
}


bool EyeseeVideoResizer::create_MUX_GRP_CHN()
{
    bool nSuccessFlag = false;
    ERRORTYPE ret;
    mMuxGrp = 0;
    while (mMuxGrp < MUX_MAX_GRP_NUM)
    {
        ret = AW_MPI_MUX_CreateGrp(mMuxGrp, &mMuxGrpAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = true;
            alogd("create mux grp channel[%d] success!", mMuxGrp);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogv("mux grp channel[%d] is exist, find next!", mMuxGrp);
            mMuxGrp++;
        }
        else
        {
            alogd("create mux grp channel[%d] ret[0x%x], find next!", mMuxGrp, ret);
            mMuxGrp++;
        }
    }
    return nSuccessFlag;
}

bool EyeseeVideoResizer::create_MUX_CHN()
{
    bool nSuccessFlag = false;
    ERRORTYPE ret;
    mMuxChn = 0;
    while (mMuxChn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(mMuxGrp, mMuxChn, &mMuxChnInfo.mMuxChnAttr, mMuxChnInfo.mSinkInfo.mOutputFd);
        if (SUCCESS == ret)
        {
            nSuccessFlag = true;
            alogd("create mux channel[%d] success!", mMuxChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogv("mux channel[%d] is exist, find next!", mMuxChn);
            mMuxChn++;
        }
        else
        {
            alogd("create mux channel[%d] ret[0x%x], find next!", mMuxChn, ret);
            mMuxChn++;
        }
    }
    return nSuccessFlag;
}

bool EyeseeVideoResizer::create_CLOCK_CHN()
{
    bool nSuccessFlag = false;
    ERRORTYPE ret;
    mClockChn = 0;
    while(mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(mClockChn, &mClockChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = true;
            alogd("create clock channel[%d] success!", mClockChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogv("clock channel[%d] is exist, find next!", mClockChn);
            mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x], find next!", mClockChn, ret);
            mClockChn++;
        }
    }
    return nSuccessFlag;
}

bool EyeseeVideoResizer::create_VDEC_CHN()
{
    bool nSuccessFlag = false;
    ERRORTYPE ret;
    mVdecChn = 0;
    while(mVdecChn < VDEC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VDEC_CreateChn(mVdecChn, &mVdecChnAttr);
        if(SUCCESS == ret)
        {
            nSuccessFlag = true;
            alogd("create vdec channel[%d] success!", mVdecChn);
            break;
        }
        else if(ERR_VENC_EXIST == ret)
        {
            alogv("vdec channel[%d] is exist, find next!", mVdecChn);
            mVdecChn++;
        }
        else
        {
            alogd("create vdec channel[%d] ret[0x%x], find next!", mVdecChn, ret);
            mVdecChn++;
        }
    }
    return nSuccessFlag;
}

bool EyeseeVideoResizer::create_VENC_CHN()
{
    bool nSuccessFlag = false;
    ERRORTYPE ret;
    mVencChn = 0;
    while(mVencChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(mVencChn, &mVencChnAttr);
        if(SUCCESS == ret)
        {
            nSuccessFlag = true;
            alogd("create venc channel[%d] success!", mVencChn);
            break;
        }
        else if(ERR_VENC_EXIST == ret)
        {
            alogv("venc channel[%d] is exist, find next!", mVencChn);
            mVencChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", mVencChn, ret);
            mVencChn++;
        }
    }
    return nSuccessFlag;
}


void EyeseeVideoResizer::config_VENC_CHN_ARRT_S()
{
    // config video enc chn
    mVencChnAttr.VeAttr.Type         = mOutDest.videoEncFmt;
    mVencChnAttr.VeAttr.SrcPicWidth  = mSrcVideoWidth;
    mVencChnAttr.VeAttr.SrcPicHeight = mSrcVideoHeight;
    mVencChnAttr.VeAttr.Field        = VIDEO_FIELD_FRAME;
    mVencChnAttr.VeAttr.PixelFormat  = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420; //NV21
    mVencChnAttr.VeAttr.Rotate       = ROTATE_NONE;
    if (PT_H264 == mOutDest.videoEncFmt)
    {
        mVencChnAttr.VeAttr.AttrH264e.bByFrame  = TRUE;
        mVencChnAttr.VeAttr.AttrH264e.Profile   = 2; // profile 0-low 1-mid 2-high
        mVencChnAttr.VeAttr.AttrH264e.PicWidth  = mOutDest.width;
        mVencChnAttr.VeAttr.AttrH264e.PicHeight = mOutDest.height;
        switch (mVencChnAttr.VeAttr.Rotate)
        {
            case 1:
                mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
                mVencChnAttr.RcAttr.mAttrH264Vbr.mMinQp = 10;
                mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxQp = 40;
                break;
            case 2:
                mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
                mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 35;
                mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 35;
                break;
            case 3:
                mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264QPMAP;
                break;
            case 0:
            default:
                mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate    = mOutDest.frameRate;
                mVencChnAttr.RcAttr.mAttrH264Cbr.fr32DstFrmRate = mOutDest.frameRate;
                mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate       = mOutDest.bitRate;
            break;
        }
    }
    else if (PT_H265 == mOutDest.videoEncFmt)
    {
        mVencChnAttr.VeAttr.AttrH265e.mbByFrame  = TRUE;
        mVencChnAttr.VeAttr.AttrH265e.mProfile   = mDstVideoEncProfile;
        mVencChnAttr.VeAttr.AttrH265e.mPicWidth  = mOutDest.width;
        mVencChnAttr.VeAttr.AttrH265e.mPicHeight = mOutDest.height;
        switch (mVencChnAttr.VeAttr.Rotate)
        {
            case 1:
                mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
                mVencChnAttr.RcAttr.mAttrH265Vbr.mMinQp = 10;
                mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxQp = 40;
                break;
            case 2:
                mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
                mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 35;
                mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 35;
                break;
            case 3:
                mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265QPMAP;
                break;
            case 0:
            default:
                mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate    = mOutDest.frameRate;
                mVencChnAttr.RcAttr.mAttrH265Cbr.fr32DstFrmRate = mOutDest.frameRate;
                mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate       = mOutDest.bitRate;
            break;
        }
    }
    else if (PT_MJPEG == mOutDest.videoEncFmt)
    {
        mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame     = TRUE;
        mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth     = mOutDest.width;
        mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight    = mOutDest.height;
        mVencChnAttr.RcAttr.mRcMode                 = VENC_RC_MODE_MJPEGCBR;
        mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = mOutDest.bitRate;
    }
    else if (PT_JPEG == mOutDest.videoEncFmt)
    {
        mVencChnAttr.VeAttr.AttrJpeg.bByFrame       = TRUE;
        mVencChnAttr.VeAttr.AttrJpeg.PicWidth       = mOutDest.width;
        mVencChnAttr.VeAttr.AttrJpeg.PicHeight      = mOutDest.height;
        mVencChnAttr.RcAttr.mRcMode                 = mDstRcMode;
        mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = mOutDest.bitRate;
    }
}

void EyeseeVideoResizer::config_MUX_CHN_ARRT_S()
{
    // config Mux chn
    OUTSINKINFO_S sinkInfo = {0};
    sinkInfo.mFallocateLen = 0;
    sinkInfo.mOutputFormat = mOutDest.mediaFileFmt;
    if (MEDIA_FILE_FORMAT_RAW == sinkInfo.mOutputFormat)
    {
        sinkInfo.mCallbackOutFlag = TRUE;
    }
    else if (MEDIA_FILE_FORMAT_MP4 == sinkInfo.mOutputFormat)
    {
        sinkInfo.mCallbackOutFlag = FALSE;
    }
    sinkInfo.mOutputFd = open(mDstFilePath, O_RDWR | O_CREAT, 0666);
    assert(sinkInfo.mOutputFd > 0);

    // config MuxChnInfo
    mMuxChnInfo.mSinkInfo.mMuxerId           = 0;
    mMuxChnInfo.mSinkInfo.mOutputFormat      = sinkInfo.mOutputFormat;
    mMuxChnInfo.mSinkInfo.mOutputFd          = dup(sinkInfo.mOutputFd);
    mMuxChnInfo.mSinkInfo.mFallocateLen      = sinkInfo.mFallocateLen;
    mMuxChnInfo.mSinkInfo.mCallbackOutFlag   = sinkInfo.mCallbackOutFlag;
    mMuxChnInfo.mMuxChnAttr.mMuxerId         = mMuxChnInfo.mSinkInfo.mMuxerId;
    mMuxChnInfo.mMuxChnAttr.mMediaFileFormat = mMuxChnInfo.mSinkInfo.mOutputFormat;
    mMuxChnInfo.mMuxChnAttr.mMaxFileDuration = mDstMediaDuration * 1000; //s -> ms
    mMuxChnInfo.mMuxChnAttr.mFallocateLen    = mMuxChnInfo.mSinkInfo.mFallocateLen;
    mMuxChnInfo.mMuxChnAttr.mCallbackOutFlag = mMuxChnInfo.mSinkInfo.mCallbackOutFlag;
    mMuxChnInfo.mMuxChnAttr.mFsWriteMode     = FSWRITEMODE_SIMPLECACHE;
    mMuxChnInfo.mMuxChnAttr.mSimpleCacheSize = 4 * 1024;
    mMuxChnInfo.mMuxChn = MM_INVALID_CHN;
    close(sinkInfo.mOutputFd);

    // config MuxGrpAttr
    mMuxGrpAttr.mVideoEncodeType = mOutDest.videoEncFmt;
    mMuxGrpAttr.mWidth           = mOutDest.width;
    mMuxGrpAttr.mHeight          = mOutDest.height;
    mMuxGrpAttr.mVideoFrmRate    = mOutDest.frameRate * 1000;

    if (mDemuxMediaInfo.mAudioNum > 0)
    {
        int AudioIndex = mDemuxMediaInfo.mAudioIndex;
        mMuxGrpAttr.mAudioEncodeType = mDemuxMediaInfo.mAudioStreamInfo[AudioIndex].mCodecType;
        mMuxGrpAttr.mChannels        = mDemuxMediaInfo.mAudioStreamInfo[AudioIndex].mChannelNum;
        mMuxGrpAttr.mBitsPerSample   = mDemuxMediaInfo.mAudioStreamInfo[AudioIndex].mBitsPerSample;
        mMuxGrpAttr.mSamplesPerFrame = 1024;
        mMuxGrpAttr.mSampleRate      = mDemuxMediaInfo.mAudioStreamInfo[AudioIndex].mSampleRate;
    }
    else
    {
        mMuxGrpAttr.mAudioEncodeType = PT_MAX;
    }
}

void EyeseeVideoResizer::config_VDEC_CHN_ATTR_S()
{
    // config video dec chn
    mVdecChnAttr.mType                = mSrcCodecType;
    mVdecChnAttr.mBufSize             = 2048*1024;  // TODO: how to decide size?
    mVdecChnAttr.mPriority            = 0;
    mVdecChnAttr.mPicWidth            = mSrcVideoWidth;
    mVdecChnAttr.mPicHeight           = mSrcVideoHeight;
    mVdecChnAttr.mInitRotation        = ROTATE_NONE;
    mVdecChnAttr.mOutputPixelFormat   = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420; //NV21
    mVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
    mVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 1;
}

bool EyeseeVideoResizer::VRThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    PROCESS_MESSAGE:
    getMsgRet = mpDoVRThread->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoVRThread::MsgType_SeekTo == msg.mMsgType)
        {
            DoSeekTo(msg.mPara0);
        }
        else if(DoVRThread::MsgType_Stop == msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else if(DoVRThread::MsgType_Exit == msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
        goto PROCESS_MESSAGE;
    }
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}


EyeseeVideoResizer::DoVRThread::DoVRThread(EyeseeVideoResizer *pVideoResizer)
                                                :mpVideoResizer(pVideoResizer)
{
}
status_t EyeseeVideoResizer::DoVRThread::startThread()
{
    status_t ret = run("DoVRThread");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void EyeseeVideoResizer::DoVRThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgType_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

bool EyeseeVideoResizer::DoVRThread::threadLoop()
{
    if(!exitPending())
    {
        return mpVideoResizer->VRThread();
    }
    else
    {
        return false;
    }
}

}
