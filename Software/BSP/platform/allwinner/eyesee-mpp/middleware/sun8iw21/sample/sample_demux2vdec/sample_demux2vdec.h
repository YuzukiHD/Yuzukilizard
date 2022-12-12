#ifndef __SAMPLE_DEMUX2VDEC_H__
#define __SAMPLE_DEMUX2VDEC_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>

#include "mm_comm_sys.h"
#include "mpi_sys.h"

#include "DemuxCompStream.h"
#include "mm_comm_demux.h"
#include "mm_comm_vdec.h"

#include "tsemaphore.h"

#include <confparser.h>


#define MAX_FILE_PATH_LEN  (128)

typedef enum {
   MEDIA_VIDEO = 0,
   MEDIA_AUDIO = 1,
   MEDIA_SUBTITLE = 2,
}MEDIA_TYPE_E;


typedef enum
{
    STATE_PREPARED = 0,
    STATE_PAUSE,
    STATE_PLAY,
    STATE_STOP,
}STATE_E;


typedef struct Demuxer2Vdec_CmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
}DEMUXER2VDEC_CMDLINEPARAM_S;

typedef struct Demuxer2Vdec_Config
{
    char srcFile[MAX_FILE_PATH_LEN];
    char dstYFile[MAX_FILE_PATH_LEN];
    char dstUFile[MAX_FILE_PATH_LEN];
    char dstVFile[MAX_FILE_PATH_LEN];
    char dstYUVFile[MAX_FILE_PATH_LEN];

    int srcFd;
    int dstYFd;
    int dstUFd;
    int dstVFd;
    int dstYUVFd;
    int seekTime;

    int mMaxVdecOutputWidth;
    int mMaxVdecOutputHeight;
    int mInitRotation;
    int mUserSetPixelFormat;
    int mCodecType;

    int mTestDuration;

}DEMUXER2VDEC_CONFIG_S;


typedef struct sample_demux2vdec_s
{
    DEMUXER2VDEC_CMDLINEPARAM_S mCmdLinePara;
    DEMUXER2VDEC_CONFIG_S mConfigPara;

    MPP_SYS_CONF_S mSysConf;

    cdx_sem_t mSemExit;

    DEMUX_CHN mDmxChn;
    DEMUX_CHN_ATTR_S mDmxChnAttr;
    DEMUX_MEDIA_INFO_S mDemuxMediaInfo;

    VDEC_CHN mVdecChn;
    VDEC_CHN_ATTR_S mVdecChnAttr;

    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;

    STATE_E mState;
    int mTrackDisFlag;

    pthread_t mThdId;

    BOOL mWaitFrameFlag;
    BOOL mOverFlag;

}SAMPLE_DEMUX2VDEC_S;





#endif //#define __SAMPLE_DEMUX2VDEC_H__

