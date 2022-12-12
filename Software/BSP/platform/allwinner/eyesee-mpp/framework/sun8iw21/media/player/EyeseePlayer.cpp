/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "EyeseePlayer"
#include <utils/plat_log.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
//#include "jni.h"
//#include "JNIHelp.h"
//#include "android_runtime/AndroidRuntime.h"

#include <cutils/properties.h>
#include <vector>

//#include <gui/SurfaceTexture.h>
//#include <gui/Surface.h>
//#include <camera/Camera.h>
//#include <binder/IMemory.h>
//#include <binder/IServiceManager.h>

#include <vdecoder.h>
#include <adecoder.h>
#include <mpi_demux.h>
#include <mpi_vdec.h>
#include <mpi_adec.h>
#include <mpi_vo.h>
#include <mpi_ao.h>
#include <mpi_clock.h>
#include <mpi_sys.h>
#include <Clock_Component.h>
#include <EyeseePlayer.h>
#include <CallbackDispatcher.h>

#define ENABLE_AUDIO_TRACK (1)

using namespace std;
namespace EyeseeLinux {

ERRORTYPE EyeseePlayer::MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ((EyeseePlayer*)cookie)->notify(pChn, event, pEventData);
    return SUCCESS;
}

void EyeseePlayer::EventHandler::handleMessage(const CallbackMessage &msg)
{
    switch (msg.what) {
        case MEDIA_PREPARED:
            alogv("MEDIA_PREPARED");
            if (mPlayer->mOnPreparedListener != NULL)
                mPlayer->mOnPreparedListener->onPrepared(mPlayer);
            return;

        case MEDIA_PLAYBACK_COMPLETE:
            alogv("MEDIA_PLAYBACK_COMPLETE");
            if (mPlayer->mOnCompletionListener != NULL)
                mPlayer->mOnCompletionListener->onCompletion(mPlayer);
            return;

        case MEDIA_SEEK_COMPLETE:
            alogv("MEDIA_SEEK_COMPLETE");
            if (mPlayer->mOnSeekCompleteListener != NULL)
                mPlayer->mOnSeekCompleteListener->onSeekComplete(mPlayer);
            return;

        case MEDIA_SET_VIDEO_SIZE:
            alogv("MEDIA_SET_VIDEO_SIZE");
            if (mPlayer->mOnVideoSizeChangedListener != NULL)
                mPlayer->mOnVideoSizeChangedListener->onVideoSizeChanged(mPlayer, msg.arg1, msg.arg2);
            return;

        case MEDIA_ERROR:
            {
                aloge("Error (%d,%d)",  msg.arg1, msg.arg2);
                bool error_was_handled = false;
                if (mPlayer->mOnErrorListener != NULL) {
                    error_was_handled = mPlayer->mOnErrorListener->onError(mPlayer, msg.arg1, msg.arg2);
                }
                if (mPlayer->mOnCompletionListener != NULL && !error_was_handled) {
                    mPlayer->mOnCompletionListener->onCompletion(mPlayer);
                }
                //stayAwake(false);
                return;
            }

        case MEDIA_INFO:
            alogv("MEDIA_INFO");
            if (msg.arg1 != MEDIA_INFO_VIDEO_TRACK_LAGGING) {
                alogv("Info (%d,%d)", msg.arg1, msg.arg2);
            }
            if (mPlayer->mOnInfoListener != NULL) {
                mPlayer->mOnInfoListener->onInfo(mPlayer, msg.arg1, msg.arg2);
            }
            // No real default action so far.
            return;

        case MEDIA_NOP: // interface test message - ignore
            break;
#if 0
        case MEDIA_TIMED_TEXT:
            if (mOnTimedTextListener == NULL)
                return;
            if (msg.obj == NULL) {
                mOnTimedTextListener.onTimedText(mPlayer, NULL);
            } else {
                if (msg.obj instanceof Parcel) {
                    Parcel parcel = (Parcel)msg.obj;
                    TimedText text = new TimedText(parcel);
                    parcel.recycle();
                    mOnTimedTextListener.onTimedText(mPlayer, text);
                }
            }
            return;

        /*Start by Bevis. Detect http data source from other application.*/
        case MEDIA_SOURCE_DETECTED:
            if (mDlnaSourceDetector != NULL && msg.obj != NULL){
                if(msg.obj instanceof Parcel){
                    Parcel parcel = (Parcel)msg.obj;
                    //parcel.setDataPosition(0);
                    String url = parcel.readString();
                    Log.d(TAG, "######MEDIA_SOURCE_DETECTED! url = " + url);
                    mDlnaSourceDetector.onSourceDetected(url);
                }
            }
            return;
            /*End by Bevis. Detect http data source from other application. */
#endif
        case MEDIA_NOTIFY_EOF:
        {
            MOD_ID_E eModId = (MOD_ID_E)msg.arg1;
            if(MOD_ID_DEMUX == eModId)
            {
                Mutex::Autolock autoLock(mPlayer->mNotifyEofLock);
                if(!mPlayer->mDmxNotifyEof)
                {
                    alogv("receive demuxer component finish flag.");
                    if(mPlayer->mVdecChn >= 0)
                    {
                        AW_MPI_VDEC_SetStreamEof(mPlayer->mVdecChn, 1);
                    }
                    if(mPlayer->mAdecChn >= 0)
                    {
                        AW_MPI_ADEC_SetStreamEof(mPlayer->mAdecChn, 1);
                    }
                    mPlayer->mDmxNotifyEof = true;
                }
                else
                {
                    aloge("fatal error! dmx already notify eof!");
                }
            }
            else if(MOD_ID_VDEC == eModId)
            {
                Mutex::Autolock autoLock(mPlayer->mNotifyEofLock);
                if(!mPlayer->mVdecNotifyEof)
                {
                    alogv("receive vdec component finish flag.");
                    if(mPlayer->mVOChn >= 0)
                    {
                        AW_MPI_VO_SetStreamEof(mPlayer->mVOLayer, mPlayer->mVOChn, 1);
                    }
                    mPlayer->mVdecNotifyEof = true;
                }
                else
                {
                    aloge("fatal error! vdec already notify eof!");
                }
            }
            else if(MOD_ID_ADEC == eModId)
            {
                Mutex::Autolock autoLock(mPlayer->mNotifyEofLock);
                if(!mPlayer->mAdecNotifyEof)
                {
                    alogv("receive adec component finish flag.");
                    if(mPlayer->mAOChn >= 0)
                    {
                        AW_MPI_AO_SetStreamEof(mPlayer->mAODev, mPlayer->mAOChn, 1, FALSE);
                    }
                    mPlayer->mAdecNotifyEof = true;
                }
                else
                {
                    aloge("fatal error! adec already notify eof!");
                }

            }
            else if(MOD_ID_VOU == eModId)
            {
                mPlayer->mNotifyEofLock.lock();
                if(!mPlayer->mVONotifyEof)
                {
                    alogv("receive vo component finish flag.");
                    mPlayer->mVONotifyEof = true;
                    mPlayer->mNotifyEofLock.unlock();
                    if(mPlayer->mAOChn < 0 || mPlayer->mAONotifyEof)
                    {
                        mPlayer->playbackComplete();

                        if(mPlayer->mLoop)
                        {
                            alogd("loop start!");
                            mPlayer->start();
                        }
                        else
                        {
                            if(mPlayer->mOnCompletionListener != NULL)
                            {
                                alogd("notify complete message.");
                                mPlayer->mOnCompletionListener->onCompletion(mPlayer);
                            }
                        }
                    }
                }
                else
                {
                    aloge("fatal error! vo already notify eof!");
                    mPlayer->mNotifyEofLock.unlock();
                }
            }
            else if(MOD_ID_AO == eModId)
            {
                mPlayer->mNotifyEofLock.lock();
                if(!mPlayer->mAONotifyEof)
                {
                    alogv("receive ao component finish flag.");
                    mPlayer->mAONotifyEof = true;
                    mPlayer->mNotifyEofLock.unlock();
                    if(mPlayer->mVOChn < 0 || mPlayer->mVONotifyEof)
                    {
                        mPlayer->playbackComplete();

                        if(mPlayer->mLoop)
                        {
                            alogd("loop start!");
                            mPlayer->start();
                        }
                        else
                        {
                            if(mPlayer->mOnCompletionListener != NULL)
                            {
                                alogd("notify complete message.");
                                mPlayer->mOnCompletionListener->onCompletion(mPlayer);
                            }
                        }
                    }
                }
                else
                {
                    aloge("fatal error! ao already notify eof!");
                    mPlayer->mNotifyEofLock.unlock();
                }

            }
            else
            {
                aloge("fatal error! other modId[0x%x]?", eModId);
            }
            break;
        }
        default:
            aloge("Unknown message type %d", msg.what);
            return;
    }
}

EyeseePlayer::EyeseePlayer()
{
    mOnErrorListener = NULL;
    mOnCompletionListener = NULL;
    mOnPreparedListener = NULL;
    mOnSeekCompleteListener = NULL;
    mOnVideoSizeChangedListener = NULL;
    mOnTimedTextListener = NULL;
    mOnInfoListener = NULL;
    mEventHandler = new EventHandler(this);
    mCurrentState = MEDIA_PLAYER_IDLE;
    mbGrantAudio = true;
    mbSeekStart = false;
    mLoop = false;
    mSurfaceHolder = MM_INVALID_LAYER;
    mCurSubtitleTrackIndex = -1;
    mTempPosition = 0;
    mDisplayWidth = 0;
    mDisplayHeight = 0;
    mMaxVdecOutputWidth = 0;
    mMaxVdecOutputHeight = 0;
    mAOCardId = PCM_CARD_TYPE_AUDIOCODEC;
    mUserSetPixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    mInitRotation = ROTATE_NONE;
    mbForceFramePackage = false;
    mDmxNotifyEof = false;
    mVdecNotifyEof = false;
    mAdecNotifyEof = false;
    mVONotifyEof = false;
    mAONotifyEof = false;
    mVdecInputBufferSize = 0;
    int err = pthread_create(&mCommandThreadId, NULL, EyeseeCommandThread, this);
	if (err || !mCommandThreadId)
	{
		aloge("fatal error! create thread fail!");
	}

    mDmxChn = MM_INVALID_CHN;
    mVdecChn = MM_INVALID_CHN;
    mVOLayer = MM_INVALID_DEV;
    mVOChn = MM_INVALID_CHN;
    mAdecChn = MM_INVALID_DEV;
    mAODev = MM_INVALID_DEV;
    mAOChn = MM_INVALID_CHN;
    mClockChn = MM_INVALID_CHN;
}

EyeseePlayer::~EyeseePlayer()
{
    reset();
    if(mCommandThreadId != 0)
    {
        EyeseeMessage cmdMsg;
        cmdMsg.mMsgType = PLAYER_COMMAND_STOP;
	mCommandQueue.queueMessage(&cmdMsg);
	// Wait for thread to exit so we can get the status into "error"
	void *retValue = NULL;
	pthread_join(mCommandThreadId, (void**)&retValue);
        mCommandThreadId = 0;
    }
    if(mEventHandler)
    {
        delete mEventHandler;
        mEventHandler = NULL;
    }
}

void EyeseePlayer::setDisplay(unsigned int hlay)
{
    Mutex::Autolock _l(mLock);
    alogv("setDisplay, hlay=%d", hlay);
    mSurfaceHolder = hlay;
}

status_t EyeseePlayer::setOutputPixelFormat(PIXEL_FORMAT_E ePixelFormat)
{
    mUserSetPixelFormat = ePixelFormat;
    return NO_ERROR;
}

status_t EyeseePlayer::setVideoScalingMode(int mode)
{
    Mutex::Autolock _l(mLock);
    alogv("setVideoScalingMode, mode=%d", mode);
    if (!isVideoScalingModeSupported(mode))
    {
        aloge("Scaling mode %d is not supported", mode);
        return INVALID_OPERATION;
    }
    if(mCurrentState==MEDIA_PLAYER_STATE_ERROR || mCurrentState&(MEDIA_PLAYER_IDLE|MEDIA_PLAYER_PREPARING))
    {
        aloge("can't called in state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(mVideoScalingMode == mode)
    {
        return NO_ERROR;
    }
    mVideoScalingMode = mode;
    if(mCurrentState&(MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_STARTED|MEDIA_PLAYER_PAUSED|MEDIA_PLAYER_PLAYBACK_COMPLETE))
    {
        if(mVOLayer!=MM_INVALID_DEV)
        {
            AW_MPI_VO_SetVideoScalingMode(mVOLayer, mVOChn, mode);
        }
        else
        {
            aloge("fatal error! video layer is not set!");
        }
    }
    return NO_ERROR;
}

status_t EyeseePlayer::setDataSource(string path)
{
    alogv("setDataSource, path=%s", path.c_str());
    status_t opStatus;

    if (access(path.c_str(), F_OK) == 0)
    {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0)
        {
            aloge("Failed to open file %s(%s)", path.c_str(), strerror(errno));
            return UNKNOWN_ERROR;
        }
        opStatus = setDataSource(fd);
        close(fd);
    }
    else
    {
        aloge("fatal error! open file path[%s] fail!", path.c_str());
        opStatus = INVALID_OPERATION;
    }

    if (NO_ERROR != opStatus)
    {
        if (NULL != mOnErrorListener)
            mOnErrorListener->onError(this, 0, 0);
    }

    return opStatus;
}

status_t EyeseePlayer::setDataSource(int fd)
{
    return setDataSource(fd, 0, 0x7ffffffffffffffL);
}

status_t EyeseePlayer::setDataSource(int fd, int64_t offset, int64_t length)
{
    Mutex::Autolock _l(mLock);
    if(!(mCurrentState&MEDIA_PLAYER_IDLE || mCurrentState==MEDIA_PLAYER_STATE_ERROR))
    {
        aloge("called in wrong state %d", mCurrentState);
        return INVALID_OPERATION;
    }
    int demux_disable_track = 0;    //DEMUX_DISABLE_AUDIO_TRACK
    if(false == mbGrantAudio)
    {
        demux_disable_track |= DEMUX_DISABLE_AUDIO_TRACK;
    }
#if (!ENABLE_AUDIO_TRACK)
        demux_disable_track |= DEMUX_DISABLE_AUDIO_TRACK;
#endif
    ERRORTYPE ret;
    bool nSuccessFlag = false;
    mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    mDmxChnAttr.mSourceUrl = NULL;
    mDmxChnAttr.mFd = fd;
    mDmxChnAttr.mDemuxDisableTrack = demux_disable_track;
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
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)this;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_DEMUX_RegisterCallback(mDmxChn, &cbInfo);

    mCurrentState = MEDIA_PLAYER_INITIALIZED;
    return NO_ERROR;
}

status_t EyeseePlayer::setVdecInputBufferSize(unsigned int nBufferSize)
{
    mVdecInputBufferSize = nBufferSize;
    return NO_ERROR;
}

unsigned int EyeseePlayer::getVdecInputBufferSize() const
{
    return mVdecInputBufferSize;
}

status_t EyeseePlayer::prepare()
{
    alogd("prepare");
    Mutex::Autolock _l(mLock);
    status_t result = NO_ERROR;
    if(!(mCurrentState & (MEDIA_PLAYER_INITIALIZED|MEDIA_PLAYER_STOPPED)))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    mClockChnAttr.nWaitMask = 0;
    DEMUX_MEDIA_INFO_S DemuxMediaInfo;
    if(AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &DemuxMediaInfo) != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
        return UNKNOWN_ERROR;
    }

    if((DemuxMediaInfo.mVideoNum >0 && DemuxMediaInfo.mVideoIndex>=DemuxMediaInfo.mVideoNum)
        || (DemuxMediaInfo.mAudioNum >0 && DemuxMediaInfo.mAudioIndex>=DemuxMediaInfo.mAudioNum)
        || (DemuxMediaInfo.mSubtitleNum >0 && DemuxMediaInfo.mSubtitleIndex>=DemuxMediaInfo.mSubtitleNum))
    {
        alogd("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]",
            DemuxMediaInfo.mVideoNum, DemuxMediaInfo.mVideoIndex, DemuxMediaInfo.mAudioNum, DemuxMediaInfo.mAudioIndex, DemuxMediaInfo.mSubtitleNum, DemuxMediaInfo.mSubtitleIndex);
        return UNKNOWN_ERROR;
    }
    if(DemuxMediaInfo.mSubtitleNum > 0)
    {
        //select subtitle index.
        int dmxSubtitleIndex = mCurSubtitleTrackIndex - (DemuxMediaInfo.mVideoNum + DemuxMediaInfo.mAudioNum);
        if(dmxSubtitleIndex < 0)
        {
            AW_MPI_DEMUX_GetChnAttr(mDmxChn, &mDmxChnAttr);
            mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_SUBTITLE_TRACK;
            AW_MPI_DEMUX_SetChnAttr(mDmxChn, &mDmxChnAttr);
        }
    }

    ERRORTYPE ret;
    if(DemuxMediaInfo.mVideoNum>0 && !(mDmxChnAttr.mDemuxDisableTrack&DEMUX_DISABLE_VIDEO_TRACK))
    {
        //create mpi_video decoder channel
        bool nSuccessFlag = false;
        config_VDEC_CHN_ATTR_S(&DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex]);
        mVdecChn = 0;
        while(mVdecChn < VDEC_MAX_CHN_NUM)
        {
            ret = AW_MPI_VDEC_CreateChn(mVdecChn, &mVdecAttr);
            if(SUCCESS == ret)
            {
                nSuccessFlag = true;
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
                alogd("create vdec channel[%d] ret[0x%x]!", mVdecChn, ret);
                break;
            }
        }
        if(false == nSuccessFlag)
        {
            mVdecChn = MM_INVALID_CHN;
            aloge("fatal error! create vdec channel fail!");
            result = UNKNOWN_ERROR;
            goto _err0;
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)this;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VDEC_RegisterCallback(mVdecChn, &cbInfo);
        AW_MPI_VDEC_ForceFramePackage(mVdecChn, (BOOL)mbForceFramePackage);
        //bind demux component
        MPP_CHN_S DmxChn{MOD_ID_DEMUX, 0, mDmxChn};
        MPP_CHN_S VdecChn{MOD_ID_VDEC, 0, mVdecChn};
        AW_MPI_SYS_Bind(&DmxChn, &VdecChn);
        mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_VIDEO;
    }
    if(mVdecChn >= 0)
    {
        //create vo channel
        bool bSuccessFlag = false;
        mVOLayer = mSurfaceHolder;
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
                alogd("vo channel[%d] is exist, find next!", mVOChn);
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
            result = UNKNOWN_ERROR;
            goto _err0;
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)this;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VO_RegisterCallback(mVOLayer, mVOChn, &cbInfo);
        AW_MPI_VO_SetChnDispBufNum(mVOLayer, mVOChn, 2);
        //bind component
        MPP_CHN_S VdecChn{MOD_ID_VDEC, 0, mVdecChn};
        MPP_CHN_S VOChn{MOD_ID_VOU, mVOLayer, mVOChn};
        AW_MPI_SYS_Bind(&VdecChn, &VOChn);
    }

    if(DemuxMediaInfo.mAudioNum>0 && !(mDmxChnAttr.mDemuxDisableTrack&DEMUX_DISABLE_AUDIO_TRACK))
    {
        //create mpi_audio decoder channel
        bool nSuccessFlag = false;
        config_ADEC_CHN_ATTR_S(&DemuxMediaInfo.mAudioStreamInfo[DemuxMediaInfo.mAudioIndex]);
        mAdecChn = 0;
        while(mAdecChn< ADEC_MAX_CHN_NUM)
        {
            ret = AW_MPI_ADEC_CreateChn(mAdecChn, &mAdecAttr);
            if(SUCCESS == ret)
            {
                nSuccessFlag = true;
                alogd("create adec channel[%d] success!", mAdecChn);
                break;
            }
            else if(ERR_ADEC_EXIST == ret)
            {
                alogd("adec channel[%d] is exist, find next!", mAdecChn);
                mAdecChn++;
            }
            else
            {
                alogd("create adec channel[%d] ret[0x%x]!", mAdecChn, ret);
                break;
            }
        }
        if(false == nSuccessFlag)
        {
            mAdecChn = MM_INVALID_CHN;
            aloge("fatal error! create adec channel fail!");
            result = UNKNOWN_ERROR;
            goto _err0;
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)this;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_ADEC_RegisterCallback(mAdecChn, &cbInfo);
        //bind demux component
        MPP_CHN_S DmxChn{MOD_ID_DEMUX, 0, mDmxChn};
        MPP_CHN_S AdecChn{MOD_ID_ADEC, 0, mAdecChn};
        AW_MPI_SYS_Bind(&DmxChn, &AdecChn);
        mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_AUDIO;
    }
    if(mAdecChn >= 0)
    {
        mAODev = 0;
        //config_AIO_ATTR_S(&DemuxMediaInfo.mAudioStreamInfo[DemuxMediaInfo.mAudioIndex]);
        //AW_MPI_AO_SetPubAttr(mAODev, &mAioAttr);

        //enable audio_hw_ao
        //embeded in AW_MPI_AO_CreateChn, not need call this api any more.
        //AW_MPI_AO_Enable(mAODev);

        //create ao channel
        bool bSuccessFlag = false;
        mAOChn = 0;
        while(mAOChn < AIO_MAX_CHN_NUM)
        {
            ret = AW_MPI_AO_CreateChn(mAODev, mAOChn);
            if(SUCCESS == ret)
            {
                bSuccessFlag = true;
                alogd("create ao channel[%d] success!", mAOChn);
                break;
            }
            else if (ERR_AO_EXIST == ret)
            {
                alogd("ao channel[%d] exist, find next!", mAOChn);
                mAOChn++;
            }
            else if(ERR_AO_NOT_ENABLED == ret)
            {
                aloge("audio_hw_ao not started!");
                break;
            }
            else
            {
                aloge("create ao channel[%d] fail! ret[0x%x]!", mAOChn, ret);
                break;
            }
        }
        if(false == bSuccessFlag)
        {
            mAOChn = MM_INVALID_CHN;
            aloge("fatal error! create ao channel fail!");
            result = UNKNOWN_ERROR;
            goto _err0;
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)this;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_AO_RegisterCallback(mAODev, mAOChn, &cbInfo);
        AW_MPI_AO_SetPcmCardType(mAODev, mAOChn, mAOCardId);
        //bind component
        MPP_CHN_S AdecChn{MOD_ID_ADEC, 0, mAdecChn};
        MPP_CHN_S AOChn{MOD_ID_AO, mAODev, mAOChn};
        AW_MPI_SYS_Bind(&AdecChn, &AOChn);
    }
    {
        //create clock channel
        if(mClockChn >= 0)
        {
            aloge("fatal error! clock channel should not exist now!");
        }
        bool bSuccessFlag = false;
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
            goto _err0;
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)this;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_CLOCK_RegisterCallback(mClockChn, &cbInfo);
        //bind component
        MPP_CHN_S ClockChn{MOD_ID_CLOCK, 0, mClockChn};
        MPP_CHN_S DmxChn{MOD_ID_DEMUX, 0, mDmxChn};
        AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
        if(mVOChn >= 0)
        {
            MPP_CHN_S VOChn{MOD_ID_VOU, mVOLayer, mVOChn};
            AW_MPI_SYS_Bind(&ClockChn, &VOChn);
        }
        if (mAOChn>=0)
        {
            MPP_CHN_S ClockChn{MOD_ID_CLOCK, 0, mClockChn};
            MPP_CHN_S AOChn{MOD_ID_AO, mAODev, mAOChn};
            AW_MPI_SYS_Bind(&ClockChn, &AOChn);
        }
    }
    if(DemuxMediaInfo.mVideoNum>0 && !(mDmxChnAttr.mDemuxDisableTrack&DEMUX_DISABLE_VIDEO_TRACK))
    {
        DEMUX_VIDEO_STREAM_INFO_S *pVideoStream = &DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex];
        mDisplayWidth = pVideoStream->mWidth;
        mDisplayHeight = pVideoStream->mHeight;
        postEventFromNative(this, MEDIA_SET_VIDEO_SIZE, pVideoStream->mWidth, pVideoStream->mHeight, NULL);

    }
    mCurrentState = MEDIA_PLAYER_PREPARED;
_err0:
    return result;
}

status_t EyeseePlayer::start_l()
{
    alogv("start_l");
    status_t opStatus = NO_ERROR;
    if(!(mCurrentState & (MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_PAUSED|MEDIA_PLAYER_PLAYBACK_COMPLETE)))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    {
        Mutex::Autolock autoLock(mNotifyEofLock);
        mDmxNotifyEof = false;
        mVdecNotifyEof = false;
        mAdecNotifyEof = false;
        mVONotifyEof = false;
        mAONotifyEof = false;
    }
    if(mbSeekStart)
    {
        seekTo_l(mSeekStartPosition);
        mbSeekStart = false;
    }
    else
    {
        if(mCurrentState & MEDIA_PLAYER_PLAYBACK_COMPLETE)
        {
            seekTo_l(0);
        }
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Start(mClockChn);
    }
    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_StartRecvStream(mVdecChn);
    }
    if(mVOChn>=0)
    {
        AW_MPI_VO_StartChn(mVOLayer, mVOChn);
    }
    if(mAdecChn >= 0)
    {
        AW_MPI_ADEC_StartRecvStream(mAdecChn);
    }
    if(mAODev>=0 && mAOChn>=0)
    {
        AW_MPI_AO_StartChn(mAODev, mAOChn);
    }
    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Start(mDmxChn);
    }
    mCurrentState = MEDIA_PLAYER_STARTED;
    return opStatus;
}

status_t EyeseePlayer::start()
{
    alogd("start");
    Mutex::Autolock _l(mLock);
    return start_l();
}

status_t EyeseePlayer::stop_l()
{
    alogv("stop_l");
    status_t opStatus = NO_ERROR;
    if(!(mCurrentState & (MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_STARTED|MEDIA_PLAYER_PAUSED|MEDIA_PLAYER_PLAYBACK_COMPLETE)))
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(mVOChn>=0)
    {
        AW_MPI_VO_HideChn(mVOLayer, mVOChn);
        AW_MPI_VO_StopChn(mVOLayer, mVOChn);
    }
    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_StopRecvStream(mVdecChn);
    }
    if(mAODev>=0 && mAOChn>=0)
    {
        AW_MPI_AO_StopChn(mAODev, mAOChn);
    }
    if(mAdecChn >= 0)
    {
        AW_MPI_ADEC_StopRecvStream(mAdecChn);
    }
    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Stop(mDmxChn);
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Stop(mClockChn);
    }

    //keep demux channel, destroy others.
    if(mVOChn>=0)
    {
        AW_MPI_VO_DestroyChn(mVOLayer, mVOChn);
        mVOChn = MM_INVALID_CHN;
    }
    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_DestroyChn(mVdecChn);
        mVdecChn = MM_INVALID_CHN;
    }
    if(mAODev>=0 && mAOChn>=0)
    {
        AW_MPI_AO_DestroyChn(mAODev, mAOChn);
        mAOChn = MM_INVALID_CHN;
    }
    if(mAdecChn >= 0)
    {
        AW_MPI_ADEC_DestroyChn(mAdecChn);
        mAdecChn = MM_INVALID_CHN;
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(mClockChn);
        mClockChn = MM_INVALID_CHN;
    }

    mCurrentState = MEDIA_PLAYER_STOPPED;
    return opStatus;
}
status_t EyeseePlayer::stop()
{
    alogv("stop");
    Mutex::Autolock _l(mLock);
    return stop_l();
}

status_t EyeseePlayer::pause_l()
{
    alogv("pause_l");
    status_t opStatus = NO_ERROR;
    if(!(mCurrentState & MEDIA_PLAYER_STARTED))
    {
        aloge("fatal error! called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Pause(mDmxChn);
    }
    if(mVOChn>=0)
    {
        AW_MPI_VO_PauseChn(mVOLayer, mVOChn);
    }
    if(mAODev>=0 && mAOChn>=0)
    {
        AW_MPI_AO_PauseChn(mAODev, mAOChn);
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Pause(mClockChn);
    }
    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_Pause(mVdecChn);
    }
    if(mAdecChn >= 0)
    {
        AW_MPI_ADEC_Pause(mAdecChn);
    }
    mTempPosition = getCurrentPosition_l();
    mCurrentState = MEDIA_PLAYER_PAUSED;
    return opStatus;
}
status_t EyeseePlayer::pause()
{
    alogv("pause");
    Mutex::Autolock _l(mLock);
    return pause_l();
}

status_t EyeseePlayer::playbackComplete()
{
    alogv("playbackComplete");
    Mutex::Autolock _l(mLock);
    if(!(mCurrentState & MEDIA_PLAYER_STARTED))
    {
        aloge("fatal error! called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(mVOChn>=0)
    {
        AW_MPI_VO_StopChn(mVOLayer, mVOChn);
    }
    if(mAODev>=0 && mAOChn>=0)
    {
        AW_MPI_AO_StopChn(mAODev, mAOChn);
    }
    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_StopRecvStream(mVdecChn);
    }
    if(mAdecChn >= 0)
    {
        AW_MPI_ADEC_StopRecvStream(mAdecChn);
    }
    if(mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Stop(mDmxChn);
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Stop(mClockChn);
    }
    mCurrentState = MEDIA_PLAYER_PLAYBACK_COMPLETE;
    return NO_ERROR;
}

status_t EyeseePlayer::getVideoWidth(int *width)
{
    Mutex::Autolock _l(mLock);
    *width = 0;
    if(mCurrentState&MEDIA_PLAYER_IDLE || mCurrentState==MEDIA_PLAYER_STATE_ERROR)
    {
        return NO_ERROR;
    }
    else if(mCurrentState & (MEDIA_PLAYER_INITIALIZED|MEDIA_PLAYER_PREPARING|MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_STOPPED|MEDIA_PLAYER_PLAYBACK_COMPLETE))
    {
        if(mDmxChn >= 0)
        {
            DEMUX_MEDIA_INFO_S DemuxMediaInfo;
            AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &DemuxMediaInfo);
            *width = DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex].mWidth;
        }
        else
        {
            aloge("fatal error! why demux channel not exist?");
        }
    }
    else if(mCurrentState & (MEDIA_PLAYER_STARTED|MEDIA_PLAYER_PAUSED))
    {
        if(mVOChn>=0)
        {
            SIZE_S displaySize;
            AW_MPI_VO_GetDisplaySize(mVOLayer, mVOChn, &displaySize);
            *width = displaySize.Width;
        }
        else
        {
            aloge("fatal error! why VO channel not exist?");
        }
    }
    else
    {
        aloge("fatal error! check code!");
    }
    return NO_ERROR;
}

status_t EyeseePlayer::getVideoHeight(int *height)
{
    Mutex::Autolock _l(mLock);
    *height = 0;
    if(mCurrentState&MEDIA_PLAYER_IDLE || mCurrentState==MEDIA_PLAYER_STATE_ERROR)
    {
        return NO_ERROR;
    }
    else if(mCurrentState & (MEDIA_PLAYER_INITIALIZED|MEDIA_PLAYER_PREPARING|MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_STOPPED|MEDIA_PLAYER_PLAYBACK_COMPLETE))
    {
        if(mDmxChn >= 0)
        {
            DEMUX_MEDIA_INFO_S DemuxMediaInfo;
            AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &DemuxMediaInfo);
            *height = DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex].mHeight;
        }
        else
        {
            aloge("fatal error! why demux channel not exist?");
        }
    }
    else if(mCurrentState & (MEDIA_PLAYER_STARTED|MEDIA_PLAYER_PAUSED))
    {
        if(mVOChn>=0)
        {
            SIZE_S displaySize;
            AW_MPI_VO_GetDisplaySize(mVOLayer, mVOChn, &displaySize);
            *height = displaySize.Height;
        }
        else
        {
            aloge("fatal error! why VO channel not exist?");
        }
    }
    else
    {
        aloge("fatal error! check code!");
    }
    return NO_ERROR;
}

bool EyeseePlayer::isPlaying()
{
    Mutex::Autolock _l(mLock);
    bool is_playing = false;
    if(mCurrentState & MEDIA_PLAYER_STARTED)
    {
        is_playing = true;
    }
    return is_playing;
}

status_t EyeseePlayer::seekTo_l(int msec)
{
    alogv("seekTo_l: %d(msec)", msec);
    Mutex::Autolock autoLock(mNotifyEofLock);
    status_t opStatus = NO_ERROR;
    if(mDmxChn >= 0)
    {
        opStatus = AW_MPI_DEMUX_Seek(mDmxChn, msec);
        if(opStatus != NO_ERROR)
        {
            return opStatus;
        }
        if(mAdecChn >= 0)
        {
            AW_MPI_ADEC_SetStreamEof(mAdecChn, 0);
        }
        if(mAOChn >= 0)
        {
            AW_MPI_AO_SetStreamEof(mAODev, mAOChn, 0, FALSE);
        }
        if(mVdecChn >= 0)
        {
            AW_MPI_VDEC_SetStreamEof(mVdecChn, 0);
        }
        if(mVOChn >= 0)
        {
            AW_MPI_VO_SetStreamEof(mVOLayer, mVOChn, 0);
        }
        mDmxNotifyEof = false;
        mVdecNotifyEof = false;
        mAdecNotifyEof = false;
        mVONotifyEof = false;
        mAONotifyEof = false;
    }
    if(mVOChn>=0)
    {
        AW_MPI_VO_Seek(mVOLayer, mVOChn);
    }
    if(mVdecChn >= 0)
    {
        AW_MPI_VDEC_Seek(mVdecChn);
    }
    if(mAODev>=0 && mAOChn>=0)
    {
        AW_MPI_AO_Seek(mAODev, mAOChn);
    }
    if(mAdecChn >= 0)
    {
        AW_MPI_ADEC_Seek(mAdecChn);
    }
    if(mClockChn >= 0)
    {
        AW_MPI_CLOCK_Seek(mClockChn);
    }
    return opStatus;
}

status_t EyeseePlayer::processSeekTo(int msec)
{
    Mutex::Autolock _l(mLock);
    alogv("processSeekTo: %d(msec)", msec);
    status_t opStatus = NO_ERROR;
    if(mCurrentState & (MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_PLAYBACK_COMPLETE|MEDIA_PLAYER_PAUSED))
    {
        mSeekStartPosition = msec;
        mbSeekStart = true;
    }
    else if(mCurrentState & MEDIA_PLAYER_STARTED)
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
    mTempPosition = msec;
    postEventFromNative(this, MEDIA_SEEK_COMPLETE, 0, 0, NULL);
    return opStatus;
}

status_t EyeseePlayer::seekTo(int msec)
{
    Mutex::Autolock _l(mLock);
    alogv("seekTo: %d(msec)", msec);
    status_t opStatus = NO_ERROR;
    if(!(mCurrentState & (MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_STARTED|MEDIA_PLAYER_PAUSED|MEDIA_PLAYER_PLAYBACK_COMPLETE)))
    {
        alogw("can't seek in current state[0x%x]", mCurrentState);
        return INVALID_OPERATION;
    }
    EyeseeMessage cmdMsg;
    cmdMsg.mMsgType = PLAYER_COMMAND_SEEK;
    cmdMsg.mPara0 = msec;
    mCommandQueue.queueMessage(&cmdMsg);
    return opStatus;
}

int EyeseePlayer::getCurrentPosition_l()
{
    if(mCurrentState&(MEDIA_PLAYER_IDLE|MEDIA_PLAYER_INITIALIZED|MEDIA_PLAYER_PREPARING|MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_STOPPED) || MEDIA_PLAYER_STATE_ERROR==mCurrentState)
    {
        return 0;
    }
    else if(mCurrentState & (MEDIA_PLAYER_STARTED|MEDIA_PLAYER_PAUSED))
    {
        if(mbSeekStart)
        {
            alogd("playerState[0x%x], during seeking ,use seekDstPos[%d]ms", mCurrentState, mTempPosition);
            return mTempPosition;
        }
        else
        {
            int mediaTime = -1;
            AW_MPI_CLOCK_GetCurrentMediaTime(mClockChn, &mediaTime);
            if(-1 == mediaTime)
            {
                mediaTime = mTempPosition;
            }
            return mediaTime;
        }
    }
    else if(mCurrentState & MEDIA_PLAYER_PLAYBACK_COMPLETE)
    {
        DEMUX_MEDIA_INFO_S DemuxMediaInfo;
        if(AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &DemuxMediaInfo) != SUCCESS)
        {
            aloge("fatal error! get media info fail in playback_complete state!");
        }
        return DemuxMediaInfo.mDuration;
    }
    else
    {
        aloge("fatal error! unknown state[0x%x]!", mCurrentState);
        return 0;
    }
}
int EyeseePlayer::getCurrentPosition()
{
    Mutex::Autolock _l(mLock);
    return getCurrentPosition_l();
}

int EyeseePlayer::getDuration()
{
    Mutex::Autolock _l(mLock);
    if(mCurrentState&MEDIA_PLAYER_IDLE || MEDIA_PLAYER_STATE_ERROR==mCurrentState)
    {
        aloge("fatal error! call in wrong state[0x%x]!", mCurrentState);
        return 0;
    }
    else if(mCurrentState & (MEDIA_PLAYER_INITIALIZED|MEDIA_PLAYER_PREPARING|MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_STARTED|MEDIA_PLAYER_PAUSED|MEDIA_PLAYER_STOPPED|MEDIA_PLAYER_PLAYBACK_COMPLETE))
    {
        DEMUX_MEDIA_INFO_S stMediaInfo;
        AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &stMediaInfo);
        return stMediaInfo.mDuration;
    }
    else
    {
        aloge("fatal error! unknown state[0x%x]!", mCurrentState);
        return 0;
    }
}

status_t EyeseePlayer::reset_l()
{
    alogv("reset_l");
    if(MEDIA_PLAYER_STATE_ERROR == mCurrentState)
    {
        alogd("can't reset in stateError, try to destruct it.");
        return INVALID_OPERATION;
    }
    if(MEDIA_PLAYER_PREPARING == mCurrentState)
    {
        aloge("fatal error! can't reset_l() in statePreparing!");
        return INVALID_OPERATION;
    }

    if(mCurrentState & (MEDIA_PLAYER_PREPARED|MEDIA_PLAYER_STARTED|MEDIA_PLAYER_PAUSED|MEDIA_PLAYER_PLAYBACK_COMPLETE))
    {
        stop_l();
    }
    if(mCurrentState & (MEDIA_PLAYER_INITIALIZED|MEDIA_PLAYER_STOPPED))
    {
        if(mDmxChn >= 0)
        {
            AW_MPI_DEMUX_DestroyChn(mDmxChn);
            mDmxChn = MM_INVALID_CHN;
        }
        mCurrentState = MEDIA_PLAYER_IDLE;
    }
    mbGrantAudio = true;
    mCurSubtitleTrackIndex = -1;

    mDisplayWidth = 0;
    mDisplayHeight = 0;
    mDmxNotifyEof = false;
    mVdecNotifyEof = false;
    mAdecNotifyEof = false;
    mVONotifyEof = false;
    mAONotifyEof = false;
    mDmxChn = MM_INVALID_CHN;
    mVdecChn = MM_INVALID_CHN;
    mVOLayer = MM_INVALID_DEV;
    mVOChn = MM_INVALID_CHN;
    mAdecChn = MM_INVALID_CHN;
    mAODev = MM_INVALID_DEV;
    mAOChn = MM_INVALID_CHN;
    mClockChn = MM_INVALID_CHN;
    return NO_ERROR;
}

status_t EyeseePlayer::reset()
{
    alogv("reset");
    Mutex::Autolock _l(mLock);
    return reset_l();
}

status_t EyeseePlayer::setAudioStreamType(int streamtype)
{
    alogv("setAudioStreamType: %d", streamtype);
    alogw("unsupported temporary");
    //streamtype: AUDIO_STREAM_MUSIC
    return UNKNOWN_ERROR;
}

status_t EyeseePlayer::setLooping(bool looping)
{
    alogv("setLooping: %d", looping);
    Mutex::Autolock _l(mLock);
    status_t opStatus = NO_ERROR;
    mLoop = looping;
    return opStatus;
}

bool EyeseePlayer::isLooping()
{
    alogv("isLooping");
    Mutex::Autolock _l(mLock);
    return mLoop;
}

status_t EyeseePlayer::getVolume(float *leftVolume, float *rightVolume)
{
    int val;
    AW_MPI_AO_GetDevVolume(mAODev, &val);
    *leftVolume = *rightVolume = (float)val/100;
    return NO_ERROR;
}

status_t EyeseePlayer::setVolume(float leftVolume, float rightVolume)
{
    AW_MPI_AO_SetDevVolume(mAODev, (int)(leftVolume*100));

    return NO_ERROR;
}

status_t EyeseePlayer::setMuteMode(bool mute)
{
    if (true == mute)
        AW_MPI_AO_SetDevMute(mAODev, 1, NULL);
    else
        AW_MPI_AO_SetDevMute(mAODev, 0, NULL);

    return NO_ERROR;
}

status_t EyeseePlayer::getMuteMode(BOOL* pMute)
{
    AW_MPI_AO_GetDevMute(mAODev, pMute, NULL);

    return NO_ERROR;
}

status_t EyeseePlayer::setAOCardType(PCM_CARD_TYPE_E cardId)
{
    alogd("set audio card type(%d)", cardId);
    mAOCardId = cardId;
    return NO_ERROR;
}

status_t EyeseePlayer::setAudioSessionId(int sessionId)
{
    alogv("set_session_id(): %d", sessionId);
    alogw("unsupported temporary");
    return UNKNOWN_ERROR;
}

int EyeseePlayer::getAudioSessionId()
{
    alogv("get_session_id()");
    alogw("unsupported temporary");
    return UNKNOWN_ERROR;
}

status_t EyeseePlayer::attachAuxEffect(int effectId)
{
    alogv("attachAuxEffect(): %d", effectId);
    alogw("unsupported temporary");
    return UNKNOWN_ERROR;
}

status_t EyeseePlayer::setAuxEffectSendLevel(float level)
{
    alogv("setAuxEffectSendLevel: level %f", level);
    alogw("unsupported temporary");
    return UNKNOWN_ERROR;
}

status_t EyeseePlayer::grantPlayAudio(bool bGrant)
{
    Mutex::Autolock _l(mLock);
    if(!(mCurrentState&MEDIA_PLAYER_IDLE))
    {
        aloge("called in wrong state %d", mCurrentState);
        return INVALID_OPERATION;
    }
    mbGrantAudio = bGrant;
    return NO_ERROR;
}

int EyeseePlayer::TrackInfo::getTrackType() const
{
    return mTrackType;
}

const string& EyeseePlayer::TrackInfo::getLanguage() const
{
    return mLanguage;
}

EyeseePlayer::TrackInfo::TrackInfo(int trackType, string language, int StreamIndex, DEMUX_MEDIA_INFO_S &DemuxMediaInfo) :
    mTrackType(trackType),
    mLanguage(language)
{
    switch (mTrackType)
    {
        case MEDIA_TRACK_TYPE_VIDEO:
        {
            mVideoTrackInfo = DemuxMediaInfo.mVideoStreamInfo[StreamIndex];
            break;
        }

        case MEDIA_TRACK_TYPE_AUDIO:
        {
            mAudioTrackInfo = DemuxMediaInfo.mAudioStreamInfo[StreamIndex];
            break;
        }

        case MEDIA_TRACK_TYPE_TIMEDTEXT:
        {
            mSubitleTrackInfo = DemuxMediaInfo.mSubtitleStreamInfo[StreamIndex];
            break;
        }

        default :
            break;
    }
}

EyeseePlayer::TrackInfo::TrackInfo(const TrackInfo& trackInfo) :
    mTrackType(trackInfo.getTrackType()),
    mLanguage(trackInfo.getLanguage())
{
    switch (mTrackType)
    {
        case MEDIA_TRACK_TYPE_VIDEO:
        {
            mVideoTrackInfo = trackInfo.mVideoTrackInfo;
            break;
        }

        case MEDIA_TRACK_TYPE_AUDIO:
        {
            mAudioTrackInfo = trackInfo.mAudioTrackInfo;
            break;
        }

        case MEDIA_TRACK_TYPE_TIMEDTEXT:
        {
            mSubitleTrackInfo = trackInfo.mSubitleTrackInfo;
            break;
        }

        default :
            break;
    }
}

EyeseePlayer::TrackInfo& EyeseePlayer::TrackInfo::operator=(const EyeseePlayer::TrackInfo& trackInfo)
{
    mTrackType = trackInfo.getTrackType();
    mLanguage = trackInfo.getLanguage();
    switch (mTrackType)
    {
        case MEDIA_TRACK_TYPE_VIDEO:
        {
            mVideoTrackInfo = trackInfo.mVideoTrackInfo;
            break;
        }

        case MEDIA_TRACK_TYPE_AUDIO:
        {
            mAudioTrackInfo = trackInfo.mAudioTrackInfo;
            break;
        }

        case MEDIA_TRACK_TYPE_TIMEDTEXT:
        {
            mSubitleTrackInfo = trackInfo.mSubitleTrackInfo;
            break;
        }

        default :
            break;
    }
    return *this;
}

void EyeseePlayer::getTrackInfo(vector<TrackInfo> &trackInfo)
{
    Mutex::Autolock _l(mLock);
    if(mCurrentState&MEDIA_PLAYER_IDLE || MEDIA_PLAYER_STATE_ERROR==mCurrentState)
    {
        alogw("not get track info in state[0x%x]!", mCurrentState);
        return;
    }
    DEMUX_MEDIA_INFO_S DemuxMediaInfo;
    if(AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &DemuxMediaInfo) != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
        return;
    }
    int i;
    TrackInfo *pTrackInfo;
    for(i=0; i<DemuxMediaInfo.mVideoNum; i++)
    {
        pTrackInfo = new TrackInfo(TrackInfo::MEDIA_TRACK_TYPE_VIDEO,
            "Video"+std::to_string(i), i, DemuxMediaInfo);;
        trackInfo.push_back(*pTrackInfo);
        delete pTrackInfo;
    }
    for(i=0; i<DemuxMediaInfo.mAudioNum; i++)
    {
        pTrackInfo = new TrackInfo(TrackInfo::MEDIA_TRACK_TYPE_AUDIO,
            (const char*)DemuxMediaInfo.mAudioStreamInfo[i].strLang, i, DemuxMediaInfo);
        trackInfo.push_back(*pTrackInfo);
        delete pTrackInfo;
    }
    for(i=0; i<DemuxMediaInfo.mSubtitleNum; i++)
    {
        pTrackInfo = new TrackInfo(TrackInfo::MEDIA_TRACK_TYPE_TIMEDTEXT,
            (const char*)DemuxMediaInfo.mSubtitleStreamInfo[i].strLang, i, DemuxMediaInfo);
        trackInfo.push_back(*pTrackInfo);
        delete pTrackInfo;
    }
}

void EyeseePlayer::addTimedTextSource(std::string path, std::string mimeType)
{
    if (!availableMimeTypeForExternalSource(mimeType))
    {
        aloge("Illegal mimeType for timed text source: %s", mimeType.c_str());
        return;
    }

    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        aloge("Failed to open file %s", path.c_str());
        return;
    }
    addTimedTextSource(fd, mimeType);
    close(fd);
}

void EyeseePlayer::addTimedTextSource(int fd, string mimeType)
{
    addTimedTextSource(fd, 0, 0x7ffffffffffffffL, mimeType);
}

void EyeseePlayer::addTimedTextSource(int fd, int64_t offset, int64_t length, string mimeType)
{
    if (!availableMimeTypeForExternalSource(mimeType))
    {
        aloge("Illegal mimeType for timed text source: %s", mimeType.c_str());
        return;
    }
    alogw("unsupported temporary");
}

void EyeseePlayer::selectTrack(int index)
{
    selectOrDeselectTrack(index, true /* select */);
}

void EyeseePlayer::deselectTrack(int index)
{
    selectOrDeselectTrack(index, false /* select */);
}
void EyeseePlayer::selectOrDeselectTrack(int index, bool select)
{
    Mutex::Autolock _l(mLock);
    if(mCurrentState&MEDIA_PLAYER_IDLE || MEDIA_PLAYER_STATE_ERROR==mCurrentState)
    {
        alogw("not get track info in state[0x%x]!", mCurrentState);
        return;
    }
    DEMUX_MEDIA_INFO_S DemuxMediaInfo;
    if(AW_MPI_DEMUX_GetMediaInfo(mDmxChn, &DemuxMediaInfo) != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
        return;
    }

    if (select)
    {
        if (index < DemuxMediaInfo.mVideoNum)
        {
            AW_MPI_DEMUX_SelectVideoStream(mDmxChn, index);
        }
        else if (index >= DemuxMediaInfo.mVideoNum &&
                    index < (DemuxMediaInfo.mVideoNum+DemuxMediaInfo.mAudioNum))
        {
            index = index - DemuxMediaInfo.mVideoNum;
            AW_MPI_DEMUX_SelectAudioStream(mDmxChn, index);
        }
        else if (index >= (DemuxMediaInfo.mVideoNum+DemuxMediaInfo.mAudioNum) &&
                    index < (DemuxMediaInfo.mVideoNum+DemuxMediaInfo.mAudioNum+DemuxMediaInfo.mSubtitleNum))
        {
            index = index - DemuxMediaInfo.mVideoNum - DemuxMediaInfo.mAudioNum;
            AW_MPI_DEMUX_SelectSubtitleStream(mDmxChn, index);
        }
    }
}

void EyeseePlayer::setOnPreparedListener(OnPreparedListener *pListener)
{
    mOnPreparedListener = pListener;
}

void EyeseePlayer::setOnCompletionListener(OnCompletionListener *pListener)
{
    mOnCompletionListener = pListener;
}

void EyeseePlayer::setOnSeekCompleteListener(OnSeekCompleteListener *pListener)
{
    mOnSeekCompleteListener = pListener;
}

void EyeseePlayer::setOnVideoSizeChangedListener(OnVideoSizeChangedListener *pListener)
{
    mOnVideoSizeChangedListener = pListener;
}

void EyeseePlayer::setOnTimedTextListener(OnTimedTextListener *pListener)
{
    mOnTimedTextListener = pListener;
}

void EyeseePlayer::setOnErrorListener(OnErrorListener *pListener)
{
    mOnErrorListener = pListener;
}

void EyeseePlayer::setOnInfoListener(OnInfoListener *pListener)
{
    mOnInfoListener = pListener;
}

status_t EyeseePlayer::setSubCharset(string charset)
{
    alogw("unsupported temporary");
    return UNKNOWN_ERROR;
}

status_t EyeseePlayer::getSubCharset(string &charset)
{
    alogw("unsupported temporary");
    return UNKNOWN_ERROR;
}

status_t EyeseePlayer::setSubDelay(int time)
{
    alogw("unsupported temporary");
    return UNKNOWN_ERROR;
}

int EyeseePlayer::getSubDelay()
{
    alogw("unsupported temporary");
    return 0;
}

status_t EyeseePlayer::enableScaleMode(bool enable, int width, int height)
{
    Mutex::Autolock _l(mLock);
    if(!(mCurrentState&(MEDIA_PLAYER_IDLE|MEDIA_PLAYER_INITIALIZED|MEDIA_PLAYER_STOPPED)))
    {
        alogw("not set scale mode in state[0x%x]!", mCurrentState);
        return UNKNOWN_ERROR;
    }
    if(enable)
    {
        mMaxVdecOutputWidth = width;
        mMaxVdecOutputHeight = height;
    }
    else
    {
        mMaxVdecOutputWidth = 0;
        mMaxVdecOutputHeight = 0;
    }
    return NO_ERROR;
}

status_t EyeseePlayer::setRotation(ROTATE_E val)
{
    Mutex::Autolock _l(mLock);
    if(!(mCurrentState&(MEDIA_PLAYER_IDLE|MEDIA_PLAYER_INITIALIZED|MEDIA_PLAYER_STOPPED)))
    {
        alogw("not set rotation in state[0x%x]!", mCurrentState);
        return UNKNOWN_ERROR;
    }
    mInitRotation = val;
    return NO_ERROR;
}

status_t EyeseePlayer::ForceFramePackage(bool bFlag)
{
    Mutex::Autolock _l(mLock);
    if(!(mCurrentState&(MEDIA_PLAYER_IDLE|MEDIA_PLAYER_INITIALIZED|MEDIA_PLAYER_STOPPED)))
    {
        alogw("not set force frame package in state[0x%x]!", mCurrentState);
        return UNKNOWN_ERROR;
    }
    mbForceFramePackage = bFlag;
    return NO_ERROR;
}

void EyeseePlayer::postEventFromNative(EyeseePlayer *pPlayer, int what, int arg1, int arg2, const std::shared_ptr<CMediaMemory>* pDataPtr)
{
    if (pPlayer == NULL)
    {
        aloge("pPlayer == NULL");
        return;
    }

    if (pPlayer->mEventHandler != NULL)
    {
        CallbackMessage msg;
        msg.what = what;
        msg.arg1 = arg1;
        msg.arg2 = arg2;
        //msg.obj = obj;
        if(pDataPtr)
        {
            msg.mDataPtr = std::const_pointer_cast<const CMediaMemory>(*pDataPtr);
        }
        pPlayer->mEventHandler->post(msg);
    }
}

void EyeseePlayer::notify(MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{

    if(MOD_ID_DEMUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                postEventFromNative(this, MEDIA_NOTIFY_EOF, pChn->mModId, 0, NULL);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if(MOD_ID_VDEC == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                postEventFromNative(this, MEDIA_NOTIFY_EOF, pChn->mModId, 0, NULL);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if(MOD_ID_VOU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                postEventFromNative(this, MEDIA_NOTIFY_EOF, pChn->mModId, 0, NULL);
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                if(mDisplayWidth!=(int)pDisplaySize->Width || mDisplayHeight!=(int)pDisplaySize->Height)
                {
                    alogd("vdec display size[%dx%d] not match with demuxInfo[%dx%d]", pDisplaySize->Width, pDisplaySize->Height, mDisplayWidth, mDisplayHeight);
                    mDisplayWidth = pDisplaySize->Width;
                    mDisplayHeight = pDisplaySize->Height;
                    postEventFromNative(this, MEDIA_SET_VIDEO_SIZE, pDisplaySize->Width, pDisplaySize->Height, NULL);
                }
                else
                {
                    alogd("vdec display size[%dx%d] same to demuxInfo", pDisplaySize->Width, pDisplaySize->Height);
                }
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                postEventFromNative(this, MEDIA_INFO, MEDIA_INFO_VIDEO_RENDERING_START, 0, NULL);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if (MOD_ID_ADEC == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                postEventFromNative(this, MEDIA_NOTIFY_EOF, pChn->mModId, 0, NULL);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if (MOD_ID_AO == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                postEventFromNative(this, MEDIA_NOTIFY_EOF, pChn->mModId, 0, NULL);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    } else {
        aloge("fatal error! need implement!");
    }
}

/* Do not change these values without updating their counterparts
 * in include/media/stagefright/MediaDefs.h and media/libstagefright/MediaDefs.cpp!
 */
/**
 * MIME type for SubRip (SRT) container. Used in addTimedTextSource APIs.
 */
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_SUBRIP[] = "application/x-subrip";

/* Do not change these values without updating their counterparts
 * in media/CedarX-Projects/CedarX/libutil/CDX_MediaDefs.h
 * and media/CedarX-Projects/CedarX/libutil/CDX_MediaDefs.cpp
 */
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_IDXSUB[]   = "application/idx-sub";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_SUB[]    = "application/sub";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_SMI[]    = "text/smi";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_RT[]         = "text/rt";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_TXT[]  = "text/txt";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_SSA[]  = "text/ssa";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_AQT[]  = "text/aqt";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_JSS[]  = "text/jss";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_JS[]     = "text/js";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_ASS[]  = "text/ass";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_VSF[]  = "text/vsf";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_TTS[]  = "text/tts";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_STL[]  = "text/stl";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_ZEG[]  = "text/zeg";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_OVR[]  = "text/ovr";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_DKS[]  = "text/dks";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_LRC[]  = "text/lrc";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_PAN[]  = "text/pan";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_SBT[]  = "text/sbt";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_VKT[]  = "text/vkt";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_PJS[]  = "text/pjs";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_MPL[]  = "text/mpl";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_SCR[]  = "text/scr";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_PSB[]  = "text/psb";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_ASC[]  = "text/asc";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_RTF[]  = "text/rtf";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_S2K[]  = "text/s2k";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_SST[]  = "text/sst";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_SON[]  = "text/son";
const char EyeseePlayer::MEDIA_MIMETYPE_TEXT_SSTS[] = "text/ssts";

bool EyeseePlayer::availableMimeTypeForExternalSource(string mimeType)
{
    if (mimeType == MEDIA_MIMETYPE_TEXT_SUBRIP ||
        mimeType == MEDIA_MIMETYPE_TEXT_IDXSUB ||
        mimeType == MEDIA_MIMETYPE_TEXT_SUB ||
        mimeType == MEDIA_MIMETYPE_TEXT_SMI ||
        mimeType == MEDIA_MIMETYPE_TEXT_RT ||
        mimeType == MEDIA_MIMETYPE_TEXT_TXT ||
        mimeType == MEDIA_MIMETYPE_TEXT_SSA ||
        mimeType == MEDIA_MIMETYPE_TEXT_AQT ||
        mimeType == MEDIA_MIMETYPE_TEXT_JSS ||
        mimeType == MEDIA_MIMETYPE_TEXT_JS ||
        mimeType == MEDIA_MIMETYPE_TEXT_ASS ||
        mimeType == MEDIA_MIMETYPE_TEXT_VSF ||
        mimeType == MEDIA_MIMETYPE_TEXT_TTS ||
        mimeType == MEDIA_MIMETYPE_TEXT_STL ||
        mimeType == MEDIA_MIMETYPE_TEXT_ZEG ||
        mimeType == MEDIA_MIMETYPE_TEXT_OVR ||
        mimeType == MEDIA_MIMETYPE_TEXT_DKS ||
        mimeType == MEDIA_MIMETYPE_TEXT_LRC ||
        mimeType == MEDIA_MIMETYPE_TEXT_PAN ||
        mimeType == MEDIA_MIMETYPE_TEXT_SBT ||
        mimeType == MEDIA_MIMETYPE_TEXT_VKT ||
        mimeType == MEDIA_MIMETYPE_TEXT_PJS ||
        mimeType == MEDIA_MIMETYPE_TEXT_MPL ||
        mimeType == MEDIA_MIMETYPE_TEXT_SCR ||
        mimeType == MEDIA_MIMETYPE_TEXT_PSB ||
        mimeType == MEDIA_MIMETYPE_TEXT_ASC ||
        mimeType == MEDIA_MIMETYPE_TEXT_RTF ||
        mimeType == MEDIA_MIMETYPE_TEXT_S2K ||
        mimeType == MEDIA_MIMETYPE_TEXT_SST ||
        mimeType == MEDIA_MIMETYPE_TEXT_SON ||
        mimeType == MEDIA_MIMETYPE_TEXT_SSTS) {
        return true;
    }
    return false;
}

status_t EyeseePlayer::config_VDEC_CHN_ATTR_S(DEMUX_VIDEO_STREAM_INFO_S *pStreamInfo)
{
    memset(&mVdecAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    mVdecAttr.mType = pStreamInfo->mCodecType;
    mVdecAttr.mBufSize = mVdecInputBufferSize;
    mVdecAttr.mPicWidth = mMaxVdecOutputWidth;
    mVdecAttr.mPicHeight = mMaxVdecOutputHeight;
    mVdecAttr.mInitRotation = mInitRotation;
    mVdecAttr.mOutputPixelFormat = mUserSetPixelFormat;
    mVdecAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
    mVdecAttr.mVdecVideoAttr.mSupportBFrame = 1;

    return NO_ERROR;
}

status_t EyeseePlayer::config_ADEC_CHN_ATTR_S(DEMUX_AUDIO_STREAM_INFO_S *pStreamInfo)
{
    memset(&mAdecAttr, 0, sizeof(ADEC_CHN_ATTR_S));
    mAdecAttr.mType = pStreamInfo->mCodecType;
    mAdecAttr.sampleRate = pStreamInfo->mSampleRate;
    mAdecAttr.channels = pStreamInfo->mChannelNum;
    mAdecAttr.bitsPerSample = pStreamInfo->mBitsPerSample;
    // only support mono and 8k now
    //assert(mAdecAttr.channels <= 1 && mAdecAttr.sampleRate == 8000);
    return NO_ERROR;
}

status_t EyeseePlayer::config_AIO_ATTR_S(DEMUX_AUDIO_STREAM_INFO_S *pStreamInfo)
{
    memset(&mAioAttr, 0, sizeof(AIO_ATTR_S));
    mAioAttr.u32ChnCnt = pStreamInfo->mChannelNum;
    mAioAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)pStreamInfo->mSampleRate;
    switch(pStreamInfo->mBitsPerSample) {
    case 8:
        mAioAttr.enBitwidth = AUDIO_BIT_WIDTH_8;
        break;
    case 16:
        mAioAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
        break;
    case 24:
        mAioAttr.enBitwidth = AUDIO_BIT_WIDTH_24;
        break;
    case 32:
        mAioAttr.enBitwidth = AUDIO_BIT_WIDTH_32;
        break;
    default:
        alogw("Why movie's audio_bitwidth is %d?! Init Audio_Hw with 16!", pStreamInfo->mBitsPerSample);
        break;
    }
    mAioAttr.mPcmCardId = mAOCardId;
    alogd("AIO_ATTR: chncnt-%d, bitwidth-%d, samplerate-%d, cardid-%d", mAioAttr.u32ChnCnt,
        pStreamInfo->mBitsPerSample, mAioAttr.enSamplerate, mAOCardId);
    return NO_ERROR;
}

void* EyeseePlayer::EyeseeCommandThread(void *pThreadData)
{
    EyeseePlayer *pPlayer = (EyeseePlayer*)pThreadData;
    EyeseeMessage cmdMsg;
    while(1)
    {
        if(pPlayer->mCommandQueue.dequeueMessage(&cmdMsg) == NO_ERROR)
        {
            if(PLAYER_COMMAND_SEEK == cmdMsg.mMsgType)
            {
                pPlayer->processSeekTo(cmdMsg.mPara0);
            }
            else if(PLAYER_COMMAND_STOP == cmdMsg.mMsgType)
            {
                alogd("receive stop command, exit!");
                goto _exit;
            }
        }
        else
        {
            pPlayer->mCommandQueue.waitMessage(0);
        }
    }
_exit:
    return (void*)NO_ERROR;
}

};
