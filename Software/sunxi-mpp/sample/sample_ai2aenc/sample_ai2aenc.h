
#ifndef _SAMPLE_AENC_H_
#define _SAMPLE_AENC_H_

#include <plat_type.h>
#include <mm_common.h>
#include <mpi_sys.h>
#include <mm_comm_aenc.h>
#include <mm_comm_aio.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>

#include <pthread.h>
#include "tsemaphore.h"


#define MAX_FILE_PATH_SIZE (256)

typedef struct WaveHeader{
    int riff_id;
    int riff_sz;
    int riff_fmt;
    int fmt_id;
    int fmt_sz;
    short audio_fmt;
    short num_chn;
    int sample_rate;
    int byte_rate;
    short block_align;
    short bits_per_sample;
    int data_id;
    int data_sz;
} WaveHeader;

typedef struct SampleAi2AencCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleAi2AencCmdLineParam;

typedef struct SampleAi2AencConfig
{
    char mDstFilePath[MAX_FILE_PATH_SIZE];

    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mFrameSize;
    int mDevVolume; //[0,100]

    PAYLOAD_TYPE_E mCodecType;

    int mTestDuration;
}SampleAi2AencConfig;

typedef struct SampleAi2AencContext
{
    SampleAi2AencCmdLineParam mCmdLinePara;
    SampleAi2AencConfig mConfigPara;

    cdx_sem_t mSemExit;

    FILE *mFpDstFile;   //compressed file
    int mDstFileSize;

    MPP_SYS_CONF_S mSysConf;

    int mAiDev;
    AI_CHN mAiChn;
    AIO_ATTR_S mAiAttr;

    AENC_CHN mAencChn;
    AENC_ATTR_S mAencAttr;

    BOOL mOverFlag;
    pthread_t mThdId;
}SampleAi2AencContext;

#endif  /* _SAMPLE_AENC_H_ */

