#ifndef _TESTUVOICE_H_
#define _TESTUVOICE_H_

#include <pthread.h>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_ai.h>

#include <uvoice.h>
#include "SaveFile.h"

#define MAX_FILE_PATH_SIZE (256)

typedef struct TestUvoiceCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}TestUvoiceCmdLineParam;

typedef struct TestUvoiceConfig
{
    char mDirPath[MAX_FILE_PATH_SIZE];
    char mFileName[MAX_FILE_PATH_SIZE];
    unsigned int mSampleRate;
    unsigned int mChannelCount;
    unsigned int mBitWidth;
    int mCapureDuration; //unit:s, 0:infinite
    int mUvoiceDataInterval;    //unit:ms, 0:any value.
    int mbSaveWav;
}TestUvoiceConfig;

typedef struct
{
    //uvoice corp define these member.
    uvoiceCommand eCmd;
    char strUvoiceCommand[128];
    struct list_head mList;
} TestUvoiceResult;


typedef struct TestUvoiceContext
{
    TestUvoiceCmdLineParam mCmdLinePara;
    TestUvoiceConfig mConfigPara;

    message_queue_t  mMessageQueue;

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAIDev;
    AI_CHN mAIChn;
    AIO_ATTR_S mAIOAttr;
    int mPcmSize;   //unit: byte
    int64_t mPcmDurationMs;   //unit:ms

    SaveWavFile *mpSaveWavFile;
    void* mpUvoice;
    char *mpAudioBuffer;    // match 160ms suit to uvoicelib.
    unsigned int mAudioBufferSize;
    unsigned int mAudioValidSize;
    pthread_mutex_t mUvoiceListLock;
    struct list_head mUvoiceResultList; //TestUvoiceResult
}TestUvoiceContext;

TestUvoiceContext* constructTestUvoiceContext();
void destructTestUvoiceContext(TestUvoiceContext *pContext);

#endif  /* _TESTUVOICE_H_ */

