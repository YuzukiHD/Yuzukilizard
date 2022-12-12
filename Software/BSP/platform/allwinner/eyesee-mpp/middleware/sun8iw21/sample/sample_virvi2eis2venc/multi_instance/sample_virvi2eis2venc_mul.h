#ifndef _SAMPLE_VIRVI2EIS2VENC_H_
#define _SAMPLE_VIRVI2EIS2VENC_H_

#include <plat_type.h>
#include <tsemaphore.h>

#include <mpi_vo.h>

#include "mm_comm_venc.h"

#define MAX_FILE_PATH_SIZE  (256)
#define SAVE_FILE_NAME "/mnt/extsd/SampleVirvi2Eis2Venc.H264"

#define SAMPLE_VIEIS_VDEV          "video_dev"
#define SAMPLE_VIEIS_CAPWIDTH      "cap_width"
#define SAMPLE_VIEIS_CAPHEIGH      "cap_height"
#define SAMPLE_VIEIS_FPS           "fps"
#define SAMPLE_VIEIS_INBUFCNT      "input_bufnum"
#define SAMPLE_VIEIS_OUTBUFCNT     "output_bufnum"
#define SAMPLE_VIEIS_TDELAY        "time_delay"
#define SAMPLE_VIEIS_SYNCTOL       "sync_tolerance"
#define SAMPLE_VIEIS_REBOOTCNT     "reboot_cnt"
#define SAMPLE_VIEIS_FRMCNT        "frame_cnt"

#define SAMPLE_VIEIS_USEVO         "use_vo"
#define SAMPLE_VIEIS_USEVENC       "use_venc"
#define SAMPLE_VIEIS_DISPW         "disp_w"
#define SAMPLE_VIEIS_DISPH         "disp_h"
#define SAMPLE_VIEIS_DISPTYPE      "disp_type"

#define SAMPLE_VIEIS_SAVEFILE      "save_file"

typedef struct SampleViEisUsrCfg {
    int iCapDev;
    int iCapVideoWidth;
    int iCapVideoHeight;
    int iVideoFps;
    int iInputBufNum;
    int iOutBufNum;
    int iTimeDelayMs; /* How much times the gyro device delay. */
    int iTimeSyncErrTolerance; /* How much error you can tolerance. */

    bool bUseVo;
    bool bUseVenc;
    int iDisW;
    int iDisH;
    VO_INTF_TYPE_E eDispType;
    VO_INTF_SYNC_E eDispSize;
    char pVoDispType[MAX_FILE_PATH_SIZE];

    int iRebootCnt;   /* How many times you want to reboot this test case. */
    int iFrameCnt;    /* How much frames you want to got in every test time. */

    char pConfFilePath[MAX_FILE_PATH_SIZE];
    char pSaveFilePath[MAX_FILE_PATH_SIZE];
} SampleViEisUsrCfg;

typedef struct SampleEisConfig {
    int iEisChn;
    EIS_ATTR_S stEisAttr;
} SampleEisConfig;

typedef struct SampleViConfig {
    int iViChn;
    int iViDev;
    VI_ATTR_S stVippAttr;
} SampleViConfig;

typedef struct SampleEncConfig {
    int iVencChn;
    VENC_CHN_ATTR_S stVencCfg;
    MPPCallbackInfo stVencCallback;
    FILE* pSaveFd;
} SampleEncConfig;

typedef struct SampleVoConfig {
    bool bOpenVo;
    int iVoLyr;
    int iVoChn;
    int iVoDev;
    int iDispW;
    int iDispH;
    VO_INTF_TYPE_E eDispType;
    VO_INTF_SYNC_E eDispSize;
    VO_VIDEO_LAYER_ATTR_S stVoAttr;
    MPPCallbackInfo stVoCallback;
} SampleVoConfig;

typedef struct SampleVirvi2Eis2VencConfig
{
    /* User configuration */
    SampleViEisUsrCfg stUsrCfg;

    /* EIS configuration */
    SampleEisConfig EisCfg[2];
    SampleViConfig  ViCfg[2];
    SampleEncConfig VencCfg;
    SampleVoConfig VoCfg;
    pthread_t pEncTrd;
    pthread_t pDispTrd;

    char OutputFilePath[MAX_FILE_PATH_SIZE];
} SampleVirvi2Eis2VencConfig;

#endif  /* _SAMPLE_VIRVI2EIS2VENC_H_ */

