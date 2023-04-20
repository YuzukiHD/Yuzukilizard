
#ifndef _SAMPLE_AENC_H_
#define _SAMPLE_AENC_H_

#include <plat_type.h>

#define MAX_FILE_PATH_SIZE (256)

#define SAMPLE_AENC_SRC_FILE_PATH   "sample_aenc_src_file"
#define SAMPLE_AENC_DST_FILE_PATH   "sample_aenc_dst_file"

#define DEFAULT_CONFIGURE_PATH  "/mnt/extsd/sample_aenc/sample_aenc.conf"
#define DEFAULT_SRC_FILE_PATH   "/mnt/extsd/sample_aenc/sample_aenc.wav"
#define DEFAULT_DST_FILE_PATH   "/mnt/extsd/sample_aenc/sample_aenc.aac"

#define WAVE_FORMAT_UNKNOWN    	(0x0000)
#define WAVE_FORMAT_PCM       	(0x0001)
#define WAVE_FORMAT_ADPCM  		(0x0011)
#define WAVE_FORMAT_G711_ALAW   (0x0006)
#define WAVE_FORMAT_G711_MULAW  (0x0007)
#define WAVE_FORMAT_G726_ADPCM  (0x0045)

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
	
typedef struct SampleAEncCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleAEncCmdLineParam;

typedef struct SampleAEncConfig
{
    char mSrcFilePath[MAX_FILE_PATH_SIZE];
    char mDstFilePath[MAX_FILE_PATH_SIZE];
    PAYLOAD_TYPE_E mCodecType;
	int bAddWaveHeader;
}SampleAEncConfig;

typedef struct SampleAEncContext
{
    SampleAEncCmdLineParam mCmdLinePara;
    SampleAEncConfig mConfigPara;

    FILE *mFpSrcFile;   //pcm file
    FILE *mFpDstFile;   //compressed file
    int mDstFileSize;

    MPP_SYS_CONF_S mSysConf;
    AENC_CHN mAEncChn;
    AENC_ATTR_S mAEncAttr;
}SampleAEncContext;

#endif  /* _SAMPLE_AENC_H_ */

