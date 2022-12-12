#ifndef __SAMPLE_VENC_H__
#define __SAMPLE_VENC_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include <pthread.h>

#include "vencoder.h"
#include "mm_comm_sys.h"
#include "mm_comm_video.h"
#include "mpi_sys.h"
#include "mpi_venc.h"

#include "cdx_list.h"

//#include "tmessage.h"
#include "tsemaphore.h"

#include "mm_common.h"

#include "sample_venc_mem.h"
#include "sample_venc_common.h"
#include <confparser.h>



#define IN_FRAME_BUF_NUM  (6)

#define MAX_FILE_PATH_LEN  (128)


typedef struct VencCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
}VENCCMDLINEPARAM_S;

typedef enum H264_PROFILE_E
{
   H264_PROFILE_BASE = 0,
   H264_PROFILE_MAIN,
   H264_PROFILE_HIGH,
}H264_PROFILE_E;

typedef enum H265_PROFILE_E
{
   H265_PROFILE_MAIN = 0,
   H265_PROFILE_MAIN10,
   H265_PROFILE_STI11,
}H265_PROFILE_E;

typedef struct VencConfig
{
    char intputFile[MAX_FILE_PATH_LEN];
    char outputFile[MAX_FILE_PATH_LEN];

    FILE * fd_in;
    FILE * fd_out;

    int srcWidth;
    int srcHeight;
    int dstWidth;
    int dstHeight;

    PAYLOAD_TYPE_E mVideoEncoderFmt;
    int srcPixFmt;
    int mEncUseProfile;
    int mVideoBitRate;
    int mVideoFrameRate;
    enum v4l2_colorspace mColorSpace;
    ROTATE_E rotate;

    int mRcMode;
    int mGopMode;
    int mGopSize;
    int mProductMode;
    int mSensorType;
    int mKeyFrameInterval;

    /* VBR/CBR */
    int mInitQp;
    int mMinIQp;
    int mMaxIQp;
    int mMinPQp;
    int mMaxPQp;
    /* FixQp */
    int mIQp;
    int mPQp;
    /* VBR */
    int mMovingTh;
    int mQuality;
    int mPBitsCoef;
    int mIBitsCoef;

    int mTestDuration;
    int mOsdEnable;
}VENCCONFIG_S;

typedef struct IN_FRAME_NODE_S
{
    VIDEO_FRAME_INFO_S  mFrame;
    struct list_head mList;
}IN_FRAME_NODE_S;

typedef struct INPUT_BUF_Q
{
    int mFrameNum;

    struct list_head mIdleList;
    struct list_head mReadyList;
    struct list_head mUseList;
    pthread_mutex_t mIdleListLock;
    pthread_mutex_t mReadyListLock;
    pthread_mutex_t mUseListLock;
}INPUT_BUF_Q;

typedef struct sample_venc_para_s
{
    VENCCONFIG_S mConfigPara;
    VENCCMDLINEPARAM_S mCmdLinePara;

    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;
    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    VENC_CHN mVeChn;

    pthread_t mReadFrmThdId;
    pthread_t mSaveEncThdId;

    char *tmpBuf;

    INPUT_BUF_Q mInBuf_Q;

    BOOL mOverFlag;

}SAMPLE_VENC_PARA_S;





#endif //#define __SAMPLE_VENC_H__
