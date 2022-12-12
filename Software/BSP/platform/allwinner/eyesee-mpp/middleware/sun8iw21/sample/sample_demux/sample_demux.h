#ifndef __SAMPLE_DEMUX_H__
#define __SAMPLE_DEMUX_H__

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

#include "tsemaphore.h"

#include <confparser.h>


#define MAX_FILE_PATH_LEN  (128)

typedef struct SampleDemuxCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
}SampleDemuxCmdLineParam;

typedef struct SampleDemuxConfig
{
    char mSrcFile[MAX_FILE_PATH_LEN];
    char mDstVideoFile[MAX_FILE_PATH_LEN];
    char mDstAudioFile[MAX_FILE_PATH_LEN];
    char mDstSubtileFile[MAX_FILE_PATH_LEN];

    int mSelectVideoIndex;
    int mSeekTime;  //unit:ms
    int mTestDuration;  //unit:s
}SampleDemuxConfig;


typedef struct SampleDemuxContext
{
    SampleDemuxCmdLineParam mCmdLinePara;
    SampleDemuxConfig mConfigPara;

    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;

    int mSrcFd;
    int mDstVideoFd;
    int mDstAudioFd;
    int mDstSubFd;

    DEMUX_CHN mDmxChn;
    DEMUX_CHN_ATTR_S mDmxChnAttr;
    DEMUX_MEDIA_INFO_S mMediaInfo;

    pthread_t mThdId;

    BOOL mOverFlag;

    BOOL mWaitBufFlag;
    int mTrackDisFlag;
}SampleDemuxContext;

#endif //#define __SAMPLE_DEMUX_H__

