#ifndef _SAMPLE_QPMAP_H_
#define _SAMPLE_QPMAP_H_

#include <plat_type.h>
#include <tsemaphore.h>

#include <confparser.h>

#include <vencoder.h>
#include <memoryAdapter.h>
#include <veAdapter.h>

#define MAX_FILE_PATH_SIZE (256)

#define DEMO_INPUT_BUFFER_NUM (3)

/* Command Line parameter */
typedef struct SampleQPMAPCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleQPMAPCmdLineParam;

typedef struct SampleQPMAPConfig
{
    char inputFile[MAX_FILE_PATH_SIZE];
    char outputFile[MAX_FILE_PATH_SIZE];

    int srcWidth;
    int srcHeight;
    int srcPixFmt;

    int dstWidth;
    int dstHeight;
    int dstPixFmt;

    int mVideoEncoderFmt;
    int mEncUseProfile;
    int mEncUseLevel;
    int mVideoBitRate;
    int mVideoFrameRate;

    int mRcMode;
    int mVencChnID;

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

    int mTestDuration; // unit: s, 0 mean infinite
}SampleQPMAPConfig;

typedef struct InputBufferInfo InputBufferInfo;

struct InputBufferInfo
{
    VencInputBuffer     inputbuffer;
    InputBufferInfo*    next;
};

typedef struct InputBufferMgr
{
    InputBufferInfo   buffer_node[DEMO_INPUT_BUFFER_NUM];
    InputBufferInfo*  valid_quene;
    InputBufferInfo*  empty_quene;
}InputBufferMgr;

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

typedef struct SampleQPMAPContext
{
    SampleQPMAPCmdLineParam mCmdLinePara; // configure file
    SampleQPMAPConfig mConfigPara;

    FILE *pFpInput;
    FILE *pFpOutput;
    VideoEncoder *pVideoEnc;
    
    // pthread_t mReadFrmThdId;
    pthread_t mVencDataProcessThdId;
    BOOL mOverFlag;

    InputBufferMgr mInputBufMgr;
    cdx_sem_t mSemInputBufferDone;

    VencMBModeCtrl  mMBModeCtrl;
    VencMBInfo      mMBInfo;
    VencMBSumInfo   mMbSumInfo;

    sGetMbInfo mGetMbInfo;

    cdx_sem_t mSemExit; // used to control the end of the test
}SampleQPMAPContext;

#endif  /* _SAMPLE_QPMAP_H_ */

