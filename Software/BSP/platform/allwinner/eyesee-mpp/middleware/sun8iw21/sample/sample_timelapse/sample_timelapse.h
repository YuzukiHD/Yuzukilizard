#ifndef _SAMPLE_TIMELAPSE_H_
#define _SAMPLE_TIMELAPSE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cdx_list.h>

#include <pthread.h>

#include "mm_comm_sys.h"
#include "mpi_sys.h"

#include "mm_comm_vi.h"
#include "mpi_vi.h"

#include "vencoder.h"
#include "mpi_venc.h"
#include "mm_comm_video.h"

#include "mm_comm_mux.h"
#include "mpi_mux.h"

#include "tmessage.h"
#include "tsemaphore.h"

#include <memoryAdapter.h>
#include "sc_interface.h"

#include <confparser.h>


#define MAX_FILE_PATH_LEN  (128)

typedef struct SampleTimelapseCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
}SampleTimelapseCmdLineParam;

typedef struct SampleTimelapseConfig
{
    int mVippIndex;
    int mVippWidth;
    int mVippHeight;
    int mVippFrameRate;
    int mTimelapse; //unit:us
    int mVideoFrameRate;
    int mVideoDuration; //unit:s
    int mVideoBitrate; //unit:Mbit/s.
    char mVideoFilePath[MAX_FILE_PATH_LEN];
}SampleTimelapseConfig;

typedef struct SampleTimelapseContext
{
    SampleTimelapseCmdLineParam mCmdLinePara;
    SampleTimelapseConfig mConfigPara;

    cdx_sem_t mSemExit;
    MPP_SYS_CONF_S mSysConf;

    PIXEL_FORMAT_E mVippPixelFormat;
    PAYLOAD_TYPE_E mVideoEncodeType;
    
    ISP_DEV mIspDev;
    VI_ATTR_S mViAttr;
    VI_DEV mViDev;
    VI_CHN mViChn;

    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    VENC_CHN mVeChn;
    
    MUX_GRP_ATTR_S mMuxGrpAttr;
    MUX_GRP mMuxGrp;
    MUX_CHN_ATTR_S mMuxChnAttr;
    MUX_CHN mMuxChn;
}SampleTimelapseContext;



#endif  /* _SAMPLE_TIMELAPSE_H_ */

