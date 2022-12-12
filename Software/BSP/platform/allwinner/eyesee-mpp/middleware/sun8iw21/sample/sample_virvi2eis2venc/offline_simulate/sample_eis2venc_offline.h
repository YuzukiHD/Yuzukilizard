#ifndef _SAMPLE_EIS2VENC_OFFLINE_H_
#define _SAMPLE_EIS2VENC_OFFLINE_H_

#include <plat_type.h>
#include <tsemaphore.h>

#include "mm_comm_venc.h"

//#define EIS_BYPASS_VENC

#define MAX_FILE_PATH_SIZE  (256)
#define SAVE_FILE_NAME "/mnt/extsd/SampleEis2Venc.H264"
#define GYRO_FILE_NAME "/mnt/extsd/SampleEis2VencGyro.txt"
#define VIDEO_FILE_NAME "/mnt/extsd/SampleEis2VencVideo.yuv"
#define TIMESTAMP_FILE_NAME "/mnt/extsd/SampleEis2VencTs.txt"

#define SAMPLE_EIS_CAPWIDTH      "cap_width"
#define SAMPLE_EIS_CAPHEIGH      "cap_height"
#define SAMPLE_EIS_FPS           "fps"
#define SAMPLE_EIS_INBUFCNT      "input_bufnum"
#define SAMPLE_EIS_OUTBUFCNT     "output_bufnum"
#define SAMPLE_EIS_TDELAY        "time_delay"
#define SAMPLE_EIS_SYNCTOL       "sync_tolerance"

#define SAMPLE_EIS_SAVEFILE      "save_file"
#define SAMPLE_EIS_GYROFILE      "gyro_file"
#define SAMPLE_EIS_VIDEOFILE     "video_file"
#define SAMPLE_EIS_TIMESTAMPFILE "timestamp_file"

typedef struct SampleViEisUsrCfg {
    int iCapVideoWidth;
    int iCapVideoHeight;
    int iVideoFps;
    int iInputBufNum;
    int iOutBufNum;
    int iTimeDelayMs; /* How much times the gyro device delay. */
    int iTimeSyncErrTolerance; /* How much error you can tolerance. */

    char pConfFilePath[MAX_FILE_PATH_SIZE];
    char pSaveFilePath[MAX_FILE_PATH_SIZE];
    char pGyroDataPath[MAX_FILE_PATH_SIZE];
    char pVideoDataPath[MAX_FILE_PATH_SIZE];
    char pTsDataPath[MAX_FILE_PATH_SIZE];
} SampleViEisUsrCfg;

typedef struct SampleEisConfig {
    int iEisChn;
    EIS_ATTR_S stEisAttr;
    MPPCallbackInfo stEisCallback;
} SampleEisConfig;

typedef struct SampleEncConfig {
    int iVencChn;
    VENC_CHN_ATTR_S stVencCfg;
    MPPCallbackInfo stVencCallback;
    FILE* pSaveFd;
} SampleEncConfig;

typedef struct FrameManager
{
    struct list_head mIdleList;
    struct list_head mUsingList;
    int mNodeCnt;
    pthread_mutex_t mLock;
    VIDEO_FRAME_INFO_S* (*PrefetchFirstIdleFrame)(void *pThiz);
    int (*UseFrame)(void *pThiz, VIDEO_FRAME_INFO_S *pFrame);
    int (*ReleaseFrame)(void *pThiz, unsigned int nFrameId);
} FrameManager;

typedef struct SampleEis2VencConfig
{
    /* User configuration */
    SampleViEisUsrCfg stUsrCfg;

    FrameManager stFrmMgr;

    /* EIS configuration */
    SampleEisConfig EisCfg;
    SampleEncConfig VencCfg;
    pthread_t pEncTrd;
    pthread_t pSendDataTrd;

    FILE *pGyroDataFd;
    FILE *pVideoDataFd;
    FILE *pTsDataFd;
} SampleEis2VencConfig;

#endif  /* _SAMPLE_VIRVI2EIS2VENC_H_ */

