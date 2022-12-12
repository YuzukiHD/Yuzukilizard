#ifndef __SAMPLE_VENC_QPMAP_H__
#define __SAMPLE_VENC_QPMAP_H__

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
#include <mpi_region.h>
#include "cdx_list.h"
#include "tsemaphore.h"
#include "mm_common.h"
#include "sample_vencQpMap_config.h"
#include <confparser.h>


#define IN_FRAME_BUF_NUM  (6)
#define MAX_FILE_PATH_SIZE  (256)

typedef struct SampleVencQPMAPCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleVencQPMAPCmdLineParam;

typedef struct SampleVencQPMAPConfig
{
    char inputFile[MAX_FILE_PATH_SIZE];
    char outputFile[MAX_FILE_PATH_SIZE];

    int srcWidth;
    int srcHeight;
    int srcPixFmt;
    enum v4l2_colorspace mColorSpace;

    int dstWidth;
    int dstHeight;

    PAYLOAD_TYPE_E mVideoEncoderFmt;
    int mEncUseProfile;
    int mField;
    int mVideoMaxKeyItl;
    int mVideoBitRate;
    int mVideoFrameRate;

    int mRcMode;

    /* qp params */
    int nMaxqp;
    int nMinqp;
    int nMaxPqp;
    int nMinPqp;
    int nQpInit;
    int bEnMbQpLimit;

    /* FixQp params */
    int mIQp;
    int mPQp;

    /* VBR params */
    int mMaxBitRate;
    int mMovingTh;
    int mQuality;
    int mIFrmBitsCoef;
    int mPFrmBitsCoef;

    /* venc cover */
    int cover_x;
    int cover_y;
    int cover_w;
    int cover_h;

    /* venc overlay */
    PIXEL_FORMAT_E mBitmapFormat;
    int overlay_x;
    int overlay_y;
    int overlay_w;
    int overlay_h;

    int mVencRoiEnable;

    int mTestDuration;
}SampleVencQPMAPConfig;

/* frame mgr */
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

/* QpMap get params */
// mad_qp_sse
typedef struct sH264GetSubInfo
{
    int mv_x;
    int mv_y;
    unsigned char sad;
}sH264GetSubInfo;

typedef struct sH264GetMbInfo
{
    unsigned char type;
    unsigned char min_cost;
    unsigned char mad;
    unsigned char qp;
    unsigned int  sse;
    sH264GetSubInfo sub[4];
}sH264GetMbInfo;

typedef struct sH265GetMbInfo
{
    unsigned char type;
    unsigned char long_term_flag;
    unsigned char mad;
    unsigned char qp;
    unsigned char sad[4];
    unsigned int  sse;
    int mv_x;
    int mv_y;
}sH265GetMbInfo;

typedef struct sH265GetCtuInfo
{
    sH265GetMbInfo mb[4];
}sH265GetCtuInfo;

// Mv
typedef struct sH264ParcelMbMvInfo
{
    long long blk3_mv_y        : 5;
    long long blk3_mv_x        : 5;
    long long blk2_mv_y        : 5;
    long long blk2_mv_x        : 5;
    long long blk1_mv_y        : 5;
    long long blk1_mv_x        : 5;
    long long blk0_mv_y        : 5;
    long long blk0_mv_x        : 5;
    long long cen_mv_y_rs2     : 7;
    long long cen_mv_x_rs2     : 8;
    long long pred_type        : 1;
    long long min_cost         : 7;
    long long rserve           : 1;
}sH264ParcelMbMvInfo;

typedef struct sH265ParcelMbMvInfo
{
    unsigned int mv_x             : 10;
    unsigned int mv_y             : 9;
    unsigned int pred_type        : 1;
    unsigned int long_term_flag   : 1;
    unsigned int reserve          : 11;
}sH265ParcelMbMvInfo;

// sad
typedef struct sH264ParcelMbBinImg
{
    unsigned char mb0_sad0    : 1;
    unsigned char mb0_sad1    : 1;
    unsigned char mb0_sad2    : 1;
    unsigned char mb0_sad3    : 1;
    unsigned char mb1_sad0    : 1;
    unsigned char mb1_sad1    : 1;
    unsigned char mb1_sad2    : 1;
    unsigned char mb1_sad3    : 1;
}sH264ParcelMbBinImg;

typedef struct sH265ParcelMbBinImg
{
    unsigned short mb0_sad0    : 1;
    unsigned short mb0_sad1    : 1;
    unsigned short mb1_sad0    : 1;
    unsigned short mb1_sad1    : 1;
    unsigned short mb0_sad2    : 1;
    unsigned short mb0_sad3    : 1;
    unsigned short mb1_sad2    : 1;
    unsigned short mb1_sad3    : 1;
    unsigned short mb2_sad0    : 1;
    unsigned short mb2_sad1    : 1;
    unsigned short mb3_sad0    : 1;
    unsigned short mb3_sad1    : 1;
    unsigned short mb2_sad2    : 1;
    unsigned short mb2_sad3    : 1;
    unsigned short mb3_sad2    : 1;
    unsigned short mb3_sad3    : 1;
}sH265ParcelMbBinImg;

/* QpMap set params */
typedef struct sParcelMbQpMap{
    unsigned char qp    : 6;
    unsigned char skip  : 1;
    unsigned char en    : 1;
}sParcelMbQpMap;

typedef struct sParcelCtuQpMap{
   sParcelMbQpMap mb[4];
}sParcelCtuQpMap;

/* sample Context */
typedef struct sGetMbInfo
{
    sH264GetMbInfo *mb;
    sH265GetCtuInfo *ctu;
}sGetMbInfo;

typedef struct SampleVencQPMAPContext
{
    SampleVencQPMAPConfig mConfigPara;
    SampleVencQPMAPCmdLineParam mCmdLinePara;

    cdx_sem_t mSemExit;

    FILE *pFpInput;
    FILE *pFpOutput;

    MPP_SYS_CONF_S mSysConf;
    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    VENC_CHN mVeChn;

    pthread_t mReadFrmThdId;
    pthread_t mSaveEncThdId;
    BOOL mOverFlag;

    char *tmpBuf;
    INPUT_BUF_Q mInBuf_Q;

    VencMBModeCtrl  mMBModeCtrl;
    VencMBSumInfo   mMbSumInfo;

    sGetMbInfo mGetMbInfo;
    unsigned int mMbNum;

    RGN_HANDLE mOverlayHandle;
    RGN_HANDLE mCoverHandle;
}SampleVencQPMAPContext;

#endif //#define __SAMPLE_VENC_QPMAP_H__
