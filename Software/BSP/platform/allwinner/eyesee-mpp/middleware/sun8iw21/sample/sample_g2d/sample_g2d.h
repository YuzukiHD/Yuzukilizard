#ifndef __SAMPLE_G2D_H__
#define __SAMPLE_G2D_H__ 

#include <plat_type.h>

#define FRAME_TO_BE_PROCESS     (120)
#define MAX_FILE_PATH_LEN       (128)
#define MAX_FILE_PATH_SIZE      (256)

typedef struct SampleG2dCmdLineParam_s
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleG2dCmdLineParam; 

typedef struct SampleG2dConfig_s
{
    PIXEL_FORMAT_E  mPicFormat; 
    PIXEL_FORMAT_E  mDstPicFormat;
    int mSrcWidth;
    int mSrcHeight;
    int mSrcRectX;
    int mSrcRectY;
    int mSrcRectW;
    int mSrcRectH;
    
    int mDstRotate; //0, 90, 180, 270, clock-wise.
    
    int mDstWidth;
    int mDstHeight;
    int mDstRectX;
    int mDstRectY;
    int mDstRectW;
    int mDstRectH;
    
    char flip_flag;
    
    char SrcFile[MAX_FILE_PATH_LEN];
    char DstFile[MAX_FILE_PATH_LEN];

    int g2d_mod;

    int g2d_bld_mod;
}SampleG2dConfig; 

typedef struct frm_info_s
{
    unsigned int frm_width;
    unsigned int frm_height;
    void *p_vir_addr[3];
    void *p_phy_addr[3];
}FRM_INFO; 

typedef struct SampleG2dMixerTaskConfig
{
    PIXEL_FORMAT_E mInputYUVFormat;
    PIXEL_FORMAT_E mInputRGBFormat;
}SampleG2dMixerTaskConfig;

typedef struct SampleG2dMixerTaskFrmInfo
{
    g2d_fmt_enh mSrcPicFormat;
    int mSrcWidth;
    int mSrcHeight;
    g2d_fmt_enh mDstPicFormat;
    int mDstWidth;
    int mDstHeight;
    void *mpSrcVirFrmAddr[3];
    void *mpDstVirFrmAddr[3];
}SampleG2dMixerTaskFrmInfo;

typedef struct SampleG2dMixerTaskContext
{
    int mG2dFd;
    SampleG2dMixerTaskFrmInfo mYUVFrmInfo;
    SampleG2dMixerTaskFrmInfo mRGBFrmInfo;
    SampleG2dMixerTaskFrmInfo mFrmInfo[FRAME_TO_BE_PROCESS];
    struct mixer_para stMixPara[FRAME_TO_BE_PROCESS];
}SampleG2dMixerTaskContext;

typedef struct sample_g2d_ctx_s
{ 
    SampleG2dConfig mConfigPara; 
    SampleG2dCmdLineParam mCmdLinePara;
    SampleG2dMixerTaskContext mixertask;

    MPP_SYS_CONF_S mSysConf;
    
    FRM_INFO src_frm_info;
    FRM_INFO dst_frm_info; 
    
    FILE * fd_in;
    FILE * fd_out;
    int mG2dFd;
}SAMPLE_G2D_CTX;

#endif
