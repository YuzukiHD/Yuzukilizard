#ifndef __SAMPLE_DEMUX2VDEC2VO_H__
#define __SAMPLE_DEMUX2VDEC2VO_H__

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
#include "mpi_demux.h"

#include "mm_comm_vdec.h"
#include "mpi_vdec.h"

#include "mm_comm_vo.h"
#include "mpi_vo.h"

#include "tsemaphore.h"

#include <confparser.h>


#define MAX_FILE_PATH_LEN  (128)


typedef enum
{
    STATE_PREPARED = 0,
    STATE_PAUSE,
    STATE_PLAY,
    STATE_STOP,
}STATE_E;

enum CLOCK_COMP_PORT_INDEX{
    CLOCK_PORT_AUDIO = 0, //to audio render
    CLOCK_PORT_VIDEO = 1, //to video render
    CLOCK_PORT_DEMUX = 2, //to demux
    CLOCK_PORT_VDEC  = 3, //to vdec
};


typedef struct Demuxer2Vdec2Vo_CmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
}DEMUXER2VDEC2VO_CMDLINEPARAM_S;

typedef struct Demuxer2Vdec2Vo_Config
{
    char srcFile[MAX_FILE_PATH_LEN];

    int seekTime;

    int mMaxVdecOutputWidth;
    int mMaxVdecOutputHeight;
    int mInitRotation;
    int mUserSetPixelFormat;

    int mTestDuration;

    int mDisplayX;
    int mDisplayY;
    int mDisplayWidth;
    int mDisplayHeight;

    int mVeFreq;  // unit: MHz

    BOOL mbForceFramePackage;
}DEMUXER2VDEC2VO_CONFIG_S;


typedef struct sample_demux2vdec2vo_s
{
    DEMUXER2VDEC2VO_CMDLINEPARAM_S mCmdLinePara;
    DEMUXER2VDEC2VO_CONFIG_S mConfigPara;

    int srcFd;
    MPP_SYS_CONF_S mSysConf;

    cdx_sem_t mSemExit;

    DEMUX_CHN mDmxChn;
    DEMUX_CHN_ATTR_S mDmxChnAttr;
    DEMUX_MEDIA_INFO_S mDemuxMediaInfo;

    VDEC_CHN mVdecChn;
    VDEC_CHN_ATTR_S mVdecChnAttr;

    int mUILayer;

    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_CHN mVoChn;
    VO_VIDEO_LAYER_ATTR_S mVoLayerAttr;
    VO_CHN_ATTR_S mVoChnAttr;

    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;

    STATE_E mState;
    int mTrackDisFlag;

    BOOL mOverFlag;

}SAMPLE_DEMUX2VDEC2VO_S;





#endif //#define __SAMPLE_DEMUX2VDEC2VO_H__

