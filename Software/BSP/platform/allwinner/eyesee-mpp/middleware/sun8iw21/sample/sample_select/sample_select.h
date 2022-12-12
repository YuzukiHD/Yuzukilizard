
#ifndef _SAMPLE_SELECT_H_
#define _SAMPLE_SELECT_H_

#include <plat_type.h>

#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define MAX_FILE_PATH_SIZE (256)

#define SAMPLE_SELECT_ASRC_FILE_PATH   "sample_select_asrc_file"
#define SAMPLE_SELECT_ADST_FILE_PATH   "sample_select_adst_file"

#define DEFAULT_CONFIGURE_PATH  "/mnt/extsd/sample_select/sample_select.conf"
#define DEFAULT_SELECT_ASRC_FILE_PATH   "/mnt/extsd/sample_select/sample_select.wav"
#define DEFAULT_SELECT_ADST_FILE_PATH   "/mnt/extsd/sample_select/sample_select.aac"

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
} WaveHeader;

typedef struct SampleSelectCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleSelectCmdLineParam;

#define AENC_CHN_NUM 7

typedef struct SampleSelectConfig
{
    char mASrcFilePath[MAX_FILE_PATH_SIZE];
    char mADstFilePath[MAX_FILE_PATH_SIZE];
    PAYLOAD_TYPE_E mACodecType;
}SampleSelectConfig;

typedef struct EncoderDescription
{
    PAYLOAD_TYPE_E type;
    const char *name;
}EncoderDescription;

typedef struct SampleSelectContext
{
    SampleSelectCmdLineParam mCmdLinePara;
    SampleSelectConfig mConfigPara;

    FILE *mFpSrcFile[AENC_CHN_NUM];   //pcm or yuv fp
    FILE *mFpDstFile[AENC_CHN_NUM];   //compressed fp
    int mDstFileSize[AENC_CHN_NUM];
    int mHandleFd[AENC_CHN_NUM];
    BOOL mSendPcmEnd[AENC_CHN_NUM];

    MPP_SYS_CONF_S mSysConf;
    AENC_CHN mAEncChn[AENC_CHN_NUM];
    AENC_ATTR_S mAEncChnAttr[AENC_CHN_NUM];
    int mAEncHandle[AENC_CHN_NUM];

    int mCurrentChnNum;
}SampleSelectContext;

#endif  /* _SAMPLE_SELECT_H_ */

