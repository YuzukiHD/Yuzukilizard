#define LOG_TAG "audio_demo"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <utils/Thread.h>
#include <utils/Errors.h>
#include <utils/Mutex.h>

#include "plat_defines.h"
#include "mm_common.h"
#include "mm_comm_aio.h"
#include "mm_comm_aenc.h"

#include "mpi_sys.h"
#include "mpi_ai.h"
#include "mpi_aenc.h"

using namespace std;

namespace EyeseeLinux {
class AudioRecorder {
public:
    AudioRecorder();
    int init();
    int config();
    int prepare();
    int start();
    int stop();

    int setEncoder(PAYLOAD_TYPE_E type);
    int config_ATTR();
    int config_AI_ATTR_S();
    int config_AENC_ATTR_S();

private:
    class StreamSaveThread: public Thread
    {
    public:
        StreamSaveThread(int id, char *pStr);
        ~StreamSaveThread();
        int startThread();
        void stopThread();
        virtual bool threadLoop();
    private:
        BOOL mRunFlag;
        int chn_id;
        FILE *fp;
        char *file_name;
        int file_size;
    };
    StreamSaveThread *mpStreamSaveThread;

/*
 * below for aenc attr param
 */
    PAYLOAD_TYPE_E mAudioEncoder;
    int mSampleRate;
    int mAudioChannels;
    int mAudioBitRate;  // encoder output bitrate

/*
 * below for ai attr param
 */
    AUDIO_SAMPLE_RATE_E mAiSampleRate;
    AIO_SOUND_MODE_E mAiSoundMode;
    AUDIO_BIT_WIDTH_E mAiBitwidth;

    AIO_ATTR_S mAioAttr;
    AENC_CHN_ATTR_S mAEncChnAttr;

    MPP_CHN_S mAiChn;
    MPP_CHN_S mAeChn;

    enum RECORDER_STATE {
        // Error state.
        AudioRecorder_ERROR                 =      0,
        // Recorder was just created.
        AudioRecorder_IDLE                  = 1 << 0,
        // Recorder has been initialized.
        AudioRecorder_INITIALIZED           = 1 << 1,
        // Configuration of the recorder has been completed.
        AudioRecorder_CONFIGURED           = 1 << 2,
        // Recorder is ready to start.
        AudioRecorder_PREPARED              = 1 << 3,
        // Recording is in progress.
        AudioRecorder_RECORDING             = 1 << 4,
    };
    char mFileName[128];
    RECORDER_STATE mCurrentState;
    const int mRecorderId;
    static int gRecorderIdCounter;
};


int AudioRecorder::gRecorderIdCounter = 0;

AudioRecorder::AudioRecorder():
    mRecorderId(gRecorderIdCounter++)
{
    alogd("AudioRecorder construct");

    mCurrentState = AudioRecorder_IDLE;
    mAudioBitRate = -1;
    mAudioChannels = -1;
    bzero(&mFileName, sizeof(mFileName));
    mpStreamSaveThread = NULL;
    mSampleRate = -1;

}

int AudioRecorder::init()
{
    alogv("init");
    if (!(mCurrentState & AudioRecorder_IDLE)) 
    {
        aloge("init called in an invalid state(%d)", mCurrentState);
        return INVALID_OPERATION;
    }

    mCurrentState = AudioRecorder_INITIALIZED;
    return NO_ERROR;
}

int AudioRecorder::setEncoder(PAYLOAD_TYPE_E type)
{
    mAudioEncoder = type;
    memset(mFileName, 0, sizeof(mFileName));
    sprintf(mFileName, "/mnt/extsd/audio_recorder_id%d.", mRecorderId);
    switch(mAudioEncoder)
    {
        case PT_AAC:
            strcat(mFileName, "aac");
            break;
        case PT_PCM_AUDIO:
            strcat(mFileName, "pcm");
            break;
        case PT_LPCM:
            strcat(mFileName, "lpcm");
            break;
        case PT_ADPCMA:
            strcat(mFileName, "adpcm");
            break;
        case PT_MP3:
            strcat(mFileName, "mp3");
            break;
        case PT_G726:
            strcat(mFileName, "g726");
            break;
        case PT_G711A:
            strcat(mFileName, "g711a");
            break;
        case PT_G711U:
            strcat(mFileName, "g711u");
            break;
        default: 
            aloge("unknow audio encoder[%d], set to mp3 defaultly!", mAudioEncoder);
            mAudioEncoder = PT_MP3;
            strcat(mFileName, "mp3");
            break;
    }
    return NO_ERROR;
}

int AudioRecorder::config_ATTR()
{
    // ai attr
    mAiBitwidth = AUDIO_BIT_WIDTH_16;
    mAiSampleRate = AUDIO_SAMPLE_RATE_8000;
    mAiSoundMode = AUDIO_SOUND_MODE_MONO;

    // aenc attr
    //mAudioEncoder = PT_AAC;
    mSampleRate = 8000;
    mAudioChannels = 1;
    mAudioBitRate = 0;

    // ai channel
    mAiChn.mModId = MOD_ID_AI;
    mAiChn.mDevId = 0;
    mAiChn.mChnId = MM_INVALID_CHN;

    // aenc channel
    mAeChn.mModId = MOD_ID_AENC;
    mAeChn.mDevId = 0;
    mAeChn.mChnId = MM_INVALID_CHN;

    return NO_ERROR;
}

int AudioRecorder::config_AI_ATTR_S()
{
    memset(&mAioAttr, 0, sizeof(AIO_ATTR_S));
    mAioAttr.enBitwidth = mAiBitwidth;
    mAioAttr.enSamplerate = mAiSampleRate;
    mAioAttr.enSoundmode = mAiSoundMode;
    mAioAttr.u32ChnCnt = 1;

    return NO_ERROR;
}

int AudioRecorder::config_AENC_ATTR_S()
{
    memset(&mAEncChnAttr, 0, sizeof(AENC_CHN_ATTR_S));
    if((PT_AAC==mAudioEncoder) ||
       (PT_PCM_AUDIO==mAudioEncoder) ||
       (PT_LPCM==mAudioEncoder) ||
       (PT_ADPCMA==mAudioEncoder) ||
       (PT_MP3==mAudioEncoder) ||
       (PT_G726==mAudioEncoder) ||
       (PT_G711A==mAudioEncoder) ||
       (PT_G711U==mAudioEncoder))
    {
        mAEncChnAttr.AeAttr.Type = mAudioEncoder;
        mAEncChnAttr.AeAttr.sampleRate = mSampleRate;
        mAEncChnAttr.AeAttr.channels = mAudioChannels;
        mAEncChnAttr.AeAttr.bitRate = mAudioBitRate;
        mAEncChnAttr.AeAttr.bitsPerSample = 16;
        mAEncChnAttr.AeAttr.attachAACHeader = 1;    // 1-ADTS; 0-ADIF
    }
    else
    {
        aloge("unsupported audio encoder formate(%d) temporaryly", mAudioEncoder);
    }

    return NO_ERROR;
}

int AudioRecorder::config()
{
    if (!(mCurrentState & AudioRecorder_INITIALIZED))
    {
        aloge("prepare called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    config_ATTR();
    config_AI_ATTR_S();
    AW_MPI_AI_SetPubAttr(mAiChn.mDevId, &mAioAttr);
    config_AENC_ATTR_S();

    mCurrentState = AudioRecorder_CONFIGURED;

    return NO_ERROR;
}

int AudioRecorder::prepare()
{
    int result = UNKNOWN_ERROR;
    if (!(mCurrentState & AudioRecorder_CONFIGURED))
    {
        aloge("prepare called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    //enable audio_hw_ai
    AW_MPI_AI_Enable(mAiChn.mDevId);

    //create ai channel.
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;
    mAiChn.mChnId = 0;
    while (mAiChn.mChnId < AIO_MAX_CHN_NUM) {
        ret = AW_MPI_AI_CreateChn(mAiChn.mDevId, mAiChn.mChnId);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", mAiChn.mChnId);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", mAiChn.mChnId);
            mAiChn.mChnId++;
        }
        else if (ERR_AI_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! ret[0x%x]!", mAiChn.mChnId, ret);
            break;
        }
    }
    if(FALSE == nSuccessFlag)
    {
        mAiChn.mChnId = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        result = UNKNOWN_ERROR;
        goto _err0;
    }

    //create aenc channel.
    nSuccessFlag = FALSE;
    mAeChn.mChnId = 0;
    while(mAeChn.mChnId < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(mAeChn.mChnId, &mAEncChnAttr);
        if(SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", mAeChn.mChnId);
            break;
        }
        else if(ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", mAeChn.mChnId);
            mAeChn.mChnId++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[0x%x], find next!", mAeChn.mChnId, ret);
            mAeChn.mChnId++;
        }
    }
    if(FALSE == nSuccessFlag)
    {
        mAeChn.mChnId = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        result = UNKNOWN_ERROR;
        goto _err0;
    }

    if (mAeChn.mChnId >= 0 && mAiChn.mChnId >= 0)
    {
        AW_MPI_SYS_Bind(&mAiChn, &mAeChn);
    }

    mCurrentState = AudioRecorder_PREPARED;

_err0:
    return result;
}

int AudioRecorder::start()
{
    if (!(mCurrentState & AudioRecorder_PREPARED)) 
    {
        aloge("start called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    int result = NO_ERROR;

    if(mAiChn.mChnId >= 0)
    {
        result = AW_MPI_AI_EnableChn(mAiChn.mDevId, mAiChn.mChnId);
    }
    if(mAeChn.mChnId >= 0)
    {
        mpStreamSaveThread = new StreamSaveThread(mAeChn.mChnId, mFileName);
        mpStreamSaveThread->startThread();
        result = AW_MPI_AENC_StartRecvPcm(mAeChn.mChnId);
    }

    mCurrentState = AudioRecorder_RECORDING;

    alogd("~~~~~~~~~~~~~[audio recording]~~~~~~~~~~~~~~");

    return result;
}

int AudioRecorder::stop()
{
    alogd("~~~~~~~~~~~~~[ready stop]~~~~~~~~~~~~~~");
    int ret = NO_ERROR;
    if (!(mCurrentState & AudioRecorder_RECORDING))
    {
        aloge("stop called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }

    mpStreamSaveThread->stopThread();
    delete mpStreamSaveThread;
    if(mAiChn.mChnId >= 0)
    {
        AW_MPI_AI_DisableChn(mAiChn.mDevId, mAiChn.mChnId);
        AW_MPI_AI_ResetChn(mAiChn.mDevId, mAiChn.mChnId);
        AW_MPI_AI_DestroyChn(mAiChn.mDevId, mAiChn.mChnId);
        mAiChn.mChnId = MM_INVALID_CHN;
    }
    if(mAeChn.mChnId >= 0)
    {
        AW_MPI_AENC_StopRecvPcm(mAeChn.mChnId);
        AW_MPI_AENC_ResetChn(mAeChn.mChnId);
        AW_MPI_AENC_DestroyChn(mAeChn.mChnId);
        mAeChn.mChnId = MM_INVALID_CHN;
    }
    mCurrentState = AudioRecorder_IDLE;

    alogd("~~~~~~~~~~~~~[stop finish]~~~~~~~~~~~~~~");

    return ret;
}

AudioRecorder::StreamSaveThread::StreamSaveThread(int id, char* str) :
    chn_id(id),
    file_name(str)
{
    fp = fopen(file_name, "wb");
    file_size = 0;
    mRunFlag = false;
}

int AudioRecorder::StreamSaveThread::startThread()
{
    mRunFlag = true;
    int ret = run("StreamSaveThread");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void AudioRecorder::StreamSaveThread::stopThread()
{
    mRunFlag = false;
    requestExitAndWait();
    fclose(fp);
    alogd("file[%s] size = %d Bytes", file_name, file_size);
}

bool AudioRecorder::StreamSaveThread::threadLoop()
{
    AUDIO_STREAM_S stream;
    if (mRunFlag){
        // get stream by block style
        AW_MPI_AENC_GetStream(chn_id, &stream, -1);
        fwrite(stream.pStream, 1, stream.mLen, fp);
        file_size += stream.mLen;
        // release
        AW_MPI_AENC_ReleaseStream(chn_id, &stream);
        return mRunFlag;
    } else {
        return false;
    }
}

}

using namespace EyeseeLinux;
int main(void)
{
    alogd("Welcome to audio_recorder demo!");

    AudioRecorder recorder1, recorder2, recorder3, recorder4;

    MPP_SYS_CONF_S conf;
    memset(&conf, 0, sizeof(MPP_SYS_CONF_S));
    conf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&conf);

    int ret = AW_MPI_SYS_Init();
    if (ret != SUCCESS) {
        aloge("AW_MPI_SYS_Init error!");
        goto ERR_EXIT;
    }

    recorder1.setEncoder(PT_MP3);
    recorder1.init();
    recorder1.config();
    recorder1.prepare();
    recorder1.start();

    recorder2.setEncoder(PT_ADPCMA);
    recorder2.init();
    recorder2.config();
    recorder2.prepare();
    recorder2.start();

    recorder3.setEncoder(PT_AAC);
    recorder3.init();
    recorder3.config();
    recorder3.prepare();
    recorder3.start();

    recorder4.setEncoder(PT_G711A);
    recorder4.init();
    recorder4.config();
    recorder4.prepare();
    recorder4.start();

    sleep(15);

    recorder1.stop();
    recorder2.stop();
    recorder3.stop();
    recorder4.stop();

    AW_MPI_SYS_Exit();

    alogd("Test finish! Goodbye!!!");

ERR_EXIT:
    return 0;
}
