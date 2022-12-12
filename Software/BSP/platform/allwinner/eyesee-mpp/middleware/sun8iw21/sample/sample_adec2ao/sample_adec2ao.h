
#ifndef _SAMPLE_ADec_H_
#define _SAMPLE_ADec_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_clock.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleADecCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleADecCmdLineParam;

typedef struct SampleADecConfig
{
    char mCompressAudioFilePath[MAX_FILE_PATH_SIZE];    //compressed audio file source
    //char mDecompressAudioFilePath[MAX_FILE_PATH_SIZE];  //target pcm file
    PAYLOAD_TYPE_E mType;                         /*the type of payload*/
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mHeaderLen;
    int mFrameSize;
    int mFirstPktLen;
}SampleADecConfig;

typedef struct ADTSHeader{
    unsigned int SyncWord:12;
    unsigned int MpegVer:1;
    unsigned int Layer:2;
    unsigned int ProtectionAbsent:1;
    unsigned int Profile:2;
    unsigned int FreqIdx:4;
    unsigned int PrivateStream:1;
    unsigned int ChnConf:3;
    unsigned int Originality:1;
    unsigned int Home:1;
    unsigned int CRStream:1;
    unsigned int CRStart:1;
    unsigned int FrameLen:13;    //include this header(size: ProtectionAbsent==1?7:9)
    unsigned int BufferFullness:11;
    unsigned int NumFrames:2;
    unsigned int CRC:16;
}ADTSHeader;

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

typedef struct SampleADecStreamNode
{
    AUDIO_STREAM_S mAStream;
    struct list_head mList;
}SampleADecStreamNode;

typedef struct SampleADecStreamManager
{
    struct list_head mIdleList; //SampleADecStreamNode
    struct list_head mUsingList;
    int mNodeCnt;
    pthread_mutex_t mLock;
    AUDIO_STREAM_S* (*PrefetchFirstIdleStream)(void *pThiz);
    int (*UseStream)(void *pThiz, AUDIO_STREAM_S *pStream);
    int (*ReleaseStream)(void *pThiz, unsigned int nFrameId);
}SampleADecStreamManager;
int initSampleADecStreamManager();
int destroySampleADecStreamManager();

typedef struct SampleADecContext
{
    SampleADecCmdLineParam mCmdLinePara;
    SampleADecConfig mConfigPara;

    FILE *mFpAudioFile; //src
    FILE *mFpPcmFile;   //dst
    SampleADecStreamManager mStreamManager;
    pthread_mutex_t mWaitStreamLock;
    int mbWaitStreamFlag;
    cdx_sem_t mSemStreamCome;
    cdx_sem_t mSemEofCome;

    MPP_SYS_CONF_S mSysConf;
    ADEC_CHN mADecChn;
    //MPP_CHN_S mADecChn;
    ADEC_CHN_ATTR_S mADecAttr;

    AUDIO_DEV mAIODev; 
    AO_CHN mAOChn;
    CLOCK_CHN mClkChn;
    AIO_ATTR_S mAIOAttr;
    CLOCK_CHN_ATTR_S mClockChnAttr;

    int loop_cnt;
    int ao_eof_flag;
}SampleADecContext;
int initSampleADecContext();
int destroySampleADecContext();

#endif  /* _SAMPLE_ADEC_H_ */

