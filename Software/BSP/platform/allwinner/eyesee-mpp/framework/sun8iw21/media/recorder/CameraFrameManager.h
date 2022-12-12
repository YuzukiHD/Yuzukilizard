/*
 * Copyright (C) 2007 The Android Open Source Project
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

/**
 * Used to record audio and video. The recording control is based on a
 * simple state machine (see below).
 *
 * <p><img src="{@docRoot}images/mediarecorder_state_diagram.gif" border="0" />
 * </p>
 *
 * <p>A common case of using MediaRecorder to record audio works as follows:
 *
 * <pre>MediaRecorder recorder = new MediaRecorder();
 * recorder.setAudioSource(MediaRecorder.AudioSource.MIC);
 * recorder.setOutputFormat(MediaRecorder.OutputFormat.THREE_GPP);
 * recorder.setAudioEncoder(MediaRecorder.AudioEncoder.AMR_NB);
 * recorder.setOutputFile(PATH_NAME);
 * recorder.prepare();
 * recorder.start();   // Recording is now started
 * ...
 * recorder.stop();
 * recorder.reset();   // You can reuse the object by going back to setAudioSource() step
 * recorder.release(); // Now the object cannot be reused
 * </pre>
 *
 * <p>Applications may want to register for informational and error
 * events in order to be informed of some internal update and possible
 * runtime errors during recording. Registration for such events is
 * done by setting the appropriate listeners (via calls
 * (to {@link #setOnInfoListener(OnInfoListener)}setOnInfoListener and/or
 * {@link #setOnErrorListener(OnErrorListener)}setOnErrorListener).
 * In order to receive the respective callback associated with these listeners,
 * applications are required to create MediaRecorder objects on threads with a
 * Looper running (the main UI thread by default already has a Looper running).
 *
 * <p><strong>Note:</strong> Currently, MediaRecorder does not work on the emulator.
 *
 * <div class="special reference">
 * <h3>Developer Guides</h3>
 * <p>For more information about how to use MediaRecorder for recording video, read the
 * <a href="{@docRoot}guide/topics/media/camera.html#capture-video">Camera</a> developer guide.
 * For more information about how to use MediaRecorder for recording sound, read the
 * <a href="{@docRoot}guide/topics/media/audio-capture.html">Audio Capture</a> developer guide.</p>
 * </div>
 */

#ifndef __IPCLINUX_CAMERAFRAMEMANAGER_H__
#define __IPCLINUX_CAMERAFRAMEMANAGER_H__

#include <EyeseeCamera.h>
#include <mm_comm_video.h>
#include <map>

namespace EyeseeLinux {

class CameraFrameManager
{
public:
    class VideoFrameInfo
    {
    public:
        VIDEO_FRAME_INFO_S mFrameInfo;
        VI_DEV mVipp;
    };

    CameraFrameManager(CameraRecordingProxy *pCameraProxy,           VI_DEV nChannel);
    ~CameraFrameManager();
    status_t addReleaseFrame(VI_DEV Vipp, VIDEO_FRAME_INFO_S *pReleaseFrame);
    status_t addCameraProxy(CameraRecordingProxy *pCameraProxy, VI_DEV Vipp);
private:
    static void* CameraFrameThread(void* user);
    pthread_t  mThreadId;
    bool mDone;
    Mutex mLock;
    Condition mCondWaitFrame;
    bool mbWaitFrameFlag;
    //CameraRecordingProxy *mpCameraProxy;
    //int mCameraChannel;
    std::map<VI_DEV, CameraRecordingProxy*> mCameraProxyMap;

    std::list<VideoFrameInfo> mIdleEncBufList;
    std::list<VideoFrameInfo> mWaitReleaseEncBufList;
    std::list<VideoFrameInfo> mReleasingEncBufList;    //only one element in it.
};

};

#endif  /* __IPCLINUX_CAMERAFRAMEMANAGER_H__ */
