#ifndef __DEMUX2ADEC_DEMO_H__
#define __DEMUX2ADEC_DEMO_H__

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
#include "mm_comm_adec.h"

#include "tmessage.h"
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

typedef enum thdstate
{
   THD_StateInvalid = 0,
   THD_StateLoaded,
   THD_StateIdle,
   THD_StateExecuting,
   THD_StatePause,
   THD_Stop
}THDSTATE_T;

typedef enum thdcmd
{
   CMD_START = 0,
   CMD_STOP,
   CMD_PAUSE
}THDCMD_T;

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
}WaveHeader;

typedef struct sample_demux2adec_s
{
    char srcFile[MAX_FILE_PATH_LEN];
	char dstFile[MAX_FILE_PATH_LEN];
    char confFilePath[MAX_FILE_PATH_LEN];

    int srcFd;
	FILE* dstFp;

    STATE_E mState;
    BOOL overFlag;

    CONFPARSER_S mConf;

    int seekTime;
    int mTrackDisableFlag;

    MPP_SYS_CONF_S mSysConf;
    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;

    DEMUX_CHN mDmxChn;
    DEMUX_CHN_ATTR_S mDmxChnAttr;
    DEMUX_MEDIA_INFO_S mDemuxMediaInfo;

    int mCodecType;

    ADEC_CHN mAdecChn;
    ADEC_CHN_ATTR_S mAdecChnAttr;
    AUDIO_FRAME_S mAudioFrame;
	
    cdx_sem_t mWaitEof;
}SAMPLE_DEMUX2ADEC_S;

#endif //#define __DEMUX2ADEC_DEMO_H__

