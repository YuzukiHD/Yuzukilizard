#define LOG_TAG "sample_ai"
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

#include "mpi_sys.h"
#include "mpi_ai.h"

using namespace std;

namespace EyeseeLinux {
class Sample_AI {
public:
    Sample_AI();
    int init();
    int config();
    int prepare();
    int start();
    int stop();

    int config_ATTR();
    int config_AI_ATTR_S();

protected:
    class PcmSaveThread: public Thread
    {
    public:
        PcmSaveThread(int id1, int id2, char *str);
        ~PcmSaveThread();
        int startThread();
        void stopThread();
        virtual bool threadLoop();
    private:
        BOOL mRunFlag;
        int dev_id;
        int chn_id;
        FILE *fp;
        char *file_name;
        int file_size;
    };

private:
    PcmSaveThread *mpPcmSaveThread;

    AUDIO_SAMPLE_RATE_E mAiSampleRate;
    AIO_SOUND_MODE_E mAiSoundMode;
    AUDIO_BIT_WIDTH_E mAiBitwidth;

    AIO_ATTR_S mAioAttr;
    AUDIO_DEV mAiDev;
    AI_CHN mAiChn;

    enum SAMPLE_AI_STATE {
        // Error state.
        SAMPLE_AI_ERROR                 =      0,
        // Recorder was just created.
        SAMPLE_AI_IDLE                  = 1 << 0,
        // Recorder has been initialized.
        SAMPLE_AI_INITIALIZED           = 1 << 1,
        // Configuration of the recorder has been completed.
        SAMPLE_AI_CONFIGURED            = 1 << 2,
        // Recorder is ready to start.
        SAMPLE_AI_PREPARED              = 1 << 3,
        // Recorder is in progress.
        SAMPLE_AI_RECORDING             = 1 << 4,
    };
    char mFileName[128];
    SAMPLE_AI_STATE mCurrentState;
    const int mRecorderId;
    static int gRecorderIdCounter;
};


int Sample_AI::gRecorderIdCounter = 0;

Sample_AI::Sample_AI():
    mRecorderId(gRecorderIdCounter++)
{
	mpPcmSaveThread = NULL;
	mFileName = NULL;
	
    alogd("Sample_AI construct");

    mCurrentState = SAMPLE_AI_IDLE;
}

int Sample_AI::init()
{
    alogv("init");
    if (!(mCurrentState & SAMPLE_AI_IDLE))
    {
        aloge("init called in an invalid state(%d)", mCurrentState);
        return INVALID_OPERATION;
    }

    mCurrentState = SAMPLE_AI_INITIALIZED;
    return NO_ERROR;
}

int Sample_AI::config_ATTR()
{
    // ai attr
    mAiBitwidth = AUDIO_BIT_WIDTH_16;
    mAiSampleRate = AUDIO_SAMPLE_RATE_8000;
    mAiSoundMode = AUDIO_SOUND_MODE_MONO;

    // ai channel
    mAiDev = 0;
    mAiChn = MM_INVALID_CHN;

    return NO_ERROR;
}

int Sample_AI::config_AI_ATTR_S()
{
    memset(&mAioAttr, 0, sizeof(AIO_ATTR_S));
    mAioAttr.enBitwidth = mAiBitwidth;
    mAioAttr.enSamplerate = mAiSampleRate;
    mAioAttr.enSoundmode = mAiSoundMode;
    mAioAttr.u32ChnCnt = 1;

    return NO_ERROR;
}

int Sample_AI::config()
{
    if (!(mCurrentState & SAMPLE_AI_INITIALIZED))
    {
        aloge("prepare called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    config_ATTR();
    config_AI_ATTR_S();
    AW_MPI_AI_SetPubAttr(mAiDev, &mAioAttr);

    mCurrentState = SAMPLE_AI_CONFIGURED;

    return NO_ERROR;
}

int Sample_AI::prepare()
{
    int result = UNKNOWN_ERROR;
    if (!(mCurrentState & SAMPLE_AI_CONFIGURED))
    {
        aloge("prepare called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    //enable audio_hw_ai
    AW_MPI_AI_Enable(mAiDev);

    //create ai channel.
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;
    mAiChn = 0;
    while (mAiChn < AIO_MAX_CHN_NUM) {
        ret = AW_MPI_AI_CreateChn(mAiDev, mAiChn);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", mAiChn);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", mAiChn);
            mAiChn++;
        }
        else if (ERR_AI_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! ret[0x%x]!", mAiChn, ret);
            break;
        }
    }
    if(FALSE == nSuccessFlag)
    {
        mAiChn = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        result = UNKNOWN_ERROR;
        goto _Error;
    }

    mCurrentState = SAMPLE_AI_PREPARED;

_Error:
    return result;
}

int Sample_AI::start()
{
    if (!(mCurrentState & SAMPLE_AI_PREPARED))
    {
        aloge("start called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    int result = NO_ERROR;

    if(mAiChn >= 0)
    {
        sprintf(mFileName, "/mnt/extsd/sample_ai_chn%d.pcm", mAiChn);
        result = AW_MPI_AI_EnableChn(mAiDev, mAiChn);
        mpPcmSaveThread = new PcmSaveThread(mAiDev, mAiChn, mFileName);
        mpPcmSaveThread->startThread();
    }

    mCurrentState = SAMPLE_AI_RECORDING;

    alogd("~~~~~~~~~~~~~[audio capturing]~~~~~~~~~~~~~~");

    return result;
}

int Sample_AI::stop()
{
    alogd("~~~~~~~~~~~~~[ready stop]~~~~~~~~~~~~~~");
    int ret = NO_ERROR;
    if (!(mCurrentState & SAMPLE_AI_RECORDING))
    {
        aloge("stop called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }

    mpPcmSaveThread->stopThread();
    delete mpPcmSaveThread;
    if(mAiChn >= 0)
    {
        AW_MPI_AI_DisableChn(mAiDev, mAiChn);
        AW_MPI_AI_ResetChn(mAiDev, mAiChn);
        AW_MPI_AI_DestroyChn(mAiDev, mAiChn);
        mAiChn = MM_INVALID_CHN;
    }
    mCurrentState = SAMPLE_AI_IDLE;

    alogd("~~~~~~~~~~~~~[stop finish]~~~~~~~~~~~~~~");

    return ret;
}

Sample_AI::PcmSaveThread::PcmSaveThread(int devid, int chnid, char* str) :
    dev_id(devid),
    chn_id(chnid),
    file_name(str)
{
    fp = fopen(file_name, "wb");
    file_size = 0;
    mRunFlag = false;
}

int Sample_AI::PcmSaveThread::startThread()
{
    mRunFlag = true;
    int ret = run("PcmSaveThread");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void Sample_AI::PcmSaveThread::stopThread()
{
    mRunFlag = false;
    requestExitAndWait();
    fclose(fp);
    alogd("file[%s] size = %d Bytes", file_name, file_size);
}

bool Sample_AI::PcmSaveThread::threadLoop()
{
    AUDIO_FRAME_S frame;
    if (mRunFlag) {
        AW_MPI_AI_GetFrame(dev_id, chn_id, &frame, NULL, -1);
        fwrite(frame.mpAddr, 1, frame.mLen, fp);
        file_size += frame.mLen;
        AW_MPI_AI_ReleaseFrame(dev_id, chn_id, &frame, NULL);
        return mRunFlag;
    } else {
        return false;
    }
}

}

int sys_init()
{
    int ret;

    ret = AW_MPI_SYS_Init();
    if (ret != SUCCESS) {
        aloge("AW_MPI_SYS_Init error!");
    }

    return ret;
}

int sys_exit()
{
    int ret;

    ret = AW_MPI_SYS_Exit();
    if (ret != SUCCESS) {
        aloge("AW_MPI_SYS_Exit error!");
    }

    return ret;
}

using namespace EyeseeLinux;
int main(void)
{
    alogd("Welcome to sample_ai demo!");

    sys_init();
    Sample_AI capInstance1, capInstance2;

    capInstance1.init();
    capInstance1.config();
    capInstance1.prepare();
    capInstance1.start();

    capInstance2.init();
    capInstance2.config();
    capInstance2.prepare();
    capInstance2.start();

    alogd("wait for 15s, then produce file...");
    sleep(15);

    capInstance1.stop();
    capInstance2.stop();

    sys_exit();

    alogd("Test finish! Goodbye!!!");

    return 0;
}
