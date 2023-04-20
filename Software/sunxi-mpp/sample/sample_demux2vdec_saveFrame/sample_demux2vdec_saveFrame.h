#ifndef _SAMPLE_DEMUX2VDEC_SAVEFRAME_H_
#define _SAMPLE_DEMUX2VDEC_SAVEFRAME_H_

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


#define MAX_FILE_PATH_SIZE (256)

/*typedef enum {
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
}STATE_E;*/


typedef struct SampleDemux2VdecSaveFrameCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleDemux2VdecSaveFrameCmdLineParam;

typedef struct SampleDemux2VdecSaveFrameConfig
{
    char mSrcFile[MAX_FILE_PATH_SIZE];
    int mSeekTime;   //unit:ms
    int mSaveNum;
    char mDstDir[MAX_FILE_PATH_SIZE];
}SampleDemux2VdecSaveFrameConfig;

typedef struct SampleDemux2VdecSaveFrameContext
{
    SampleDemux2VdecSaveFrameCmdLineParam mCmdLinePara;
    SampleDemux2VdecSaveFrameConfig mConfigPara;

    MPP_SYS_CONF_S mSysConf;

    cdx_sem_t mSemExit;

    DEMUX_CHN mDmxChn;
    DEMUX_CHN_ATTR_S mDmxChnAttr;
    DEMUX_MEDIA_INFO_S mDemuxMediaInfo;

    VDEC_CHN mVdecChn;
    VDEC_CHN_ATTR_S mVdecChnAttr;

    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;

    volatile BOOL mbEndFlag;
    volatile BOOL mbUserExitFlag;

    //STATE_E mState;
    //int mTrackDisFlag;

    //pthread_t mThdId;

    //BOOL mWaitFrameFlag;
    //BOOL mOverFlag;

}SampleDemux2VdecSaveFrameContext;

#endif  /* _SAMPLE_DEMUX2VDEC_SAVEFRAME_H_ */

