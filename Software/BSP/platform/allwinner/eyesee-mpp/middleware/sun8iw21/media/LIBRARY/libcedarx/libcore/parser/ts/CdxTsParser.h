/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxTsParser.h
 * Description : TsParser
 * History :
 *
 */

#ifndef _CDX_TS_PARSER_H_
#define _CDX_TS_PARSER_H_

#include <CdxTypes.h>
#include <CdxList.h>
#include <CdxBuffer.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>

#define MAX_PID_NUM 8192
//* codec id

#define CODEC_ID_DVB_SUBTITLE       18
#define CODEC_ID_BLUERAY_SUBTITLE   19

//* packet size of different format
#define TS_PACKET_SIZE              188
#define TS_DVHS_PACKET_SIZE         192
#define TS_FEC_PACKET_SIZE          204

/* pids */
#define TS_PAT_PID                  0x0000

/* descriptor ids */
#define DVB_SUBT_DESCID             0x59

#define CDX_STREAM_TYPE_VIDEO_MPEG1     0x01
#define CDX_STREAM_TYPE_VIDEO_MPEG2     0x02
#define CDX_STREAM_TYPE_AUDIO_MPEG1     0x03
#define CDX_STREAM_TYPE_AUDIO_MPEG2     0x04
#define CDX_STREAM_TYPE_PRIVATE_SECTION 0x05
#define CDX_STREAM_TYPE_PRIVATE_DATA    0x06
#define CDX_STREAM_TYPE_AUDIO_AAC       0x0f
#define CDX_STREAM_TYPE_VIDEO_MPEG4     0x10
#define CDX_STREAM_TYPE_AUDIO_MPEG4     0x11
#define CDX_STREAM_TYPE_OMX_VIDEO_CodingAVC      0x1b
#define CDX_STREAM_TYPE_VIDEO_H265      0x24
#define CDX_STREAM_TYPE_VIDEO_SHVC      0x27    /*add for scable HEVC*/
#define CDX_STREAM_TYPE_VIDEO_VC1       0xea

#define CDX_STREAM_TYPE_PCM_BLURAY      0x80    //* add for blue ray
#define CDX_STREAM_TYPE_AUDIO_AC3       0x81
#define CDX_STREAM_TYPE_AUDIO_HDMV_DTS  0x82
#define CDX_STREAM_TYPE_AUDIO_AC3_TRUEHD    0x83
#define CDX_STREAM_TYPE_AUDIO_EAC3      0x87
#define CDX_STREAM_TYPE_AUDIO_DTS_HRA   0x85    //* add for blue ray
#define CDX_STREAM_TYPE_AUDIO_DTS_MA    0x86    //* add for blue ray
#define CDX_STREAM_TYPE_AUDIO_DTS       0x8a
#define CDX_STREAM_TYPE_AUDIO_AC3_      0xa1
#define CDX_STREAM_TYPE_AUDIO_DTS_      0xa2
#define CDX_STREAM_TYPE_METADATA        0x15
#define CDX_STREAM_TYPE_VIDEO_MVC        0x20

#define CDX_STREAM_TYPE_HDMV_PGS_SUBTITLE 0x90

#define CDX_STREAM_TYPE_UNKOWN          0x0  // streamType is 0
#define METADATA_STREAM_ID            0xbd

#define SUPPORT_SUBTITLE_DVB        (1)
#if     SUPPORT_SUBTITLE_DVB
#define CDX_STREAM_TYPE_SUBTITLE_DVB    0x104
#endif
#define CDX_STREAM_TYPE_VIDEO_AVS        0x42

static const cdx_int32 AC3_Channels[]=
{2, 1, 2, 3, 3, 4, 4, 5};
static const cdx_int32 AC3_SamplingRate[]=
{ 48000,  44100,  32000,      0,};
static const cdx_int32 AC3_BitRate[]=
{
     32,
     40,
     48,
     56,
     64,
     80,
     96,
    112,
    128,
    160,
    192,
    224,
    256,
    320,
    384,
    448,
    512,
    576,
    640,
};

enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
};

typedef enum {
    /* Do not change these values (starting with HDCP_STREAM_TYPE_),
     * keep them in sync with header file "DX_Hdcp_Types.h" from Discretix.
     */
    HDCP_STREAM_TYPE_UNKNOWN,
    HDCP_STREAM_TYPE_VIDEO,
    HDCP_STREAM_TYPE_AUDIO,
    HDCP_STREAM_TYPE_INVALUD = 0x7FFFFFFF
}HdcpStreamType;

#define CTS (0)
#define ProbeDataLen (2*1024)
#define HandlePtsInLocal 0
#define ParseAdaptationField 0
#define PES_PRIVATE_DATA_SIZE (16)

#define SizeToReadEverytimeInLocal (500*1024)
#define SizeToReadEverytimeInNet (4*1024)
#define SizeToReadEverytimeInMiracast (188*7)
#define VideoStreamBufferSize (1024*1024)
#define OPEN_CHECK (0)
#define Debug (0)
#define PtsDebug (0)
#define PROBE_STREAM (1)
#define ProbeSpecificData (1)
#define SIZE_OF_VIDEO_PROVB_DATA (2*1024*1024)
#define SIZE_OF_AUDIO_PROVB_DATA (150*1024)
#define    DVB_USED    (1)
#define    DVB_TEST    (0)

//add by xhw
#if DVB_USED
#define TS_PES_START_SIZE    9
#endif

typedef enum {
    OK = 0,
    ERROR_MALFORMED = -1,
    ERROR_SECTION = -2
}status_t;

typedef enum {
    TYPE_UNKNOWN    = -1,
    TYPE_VIDEO      = 0,
    TYPE_AUDIO        = 1,
    TYPE_SUBS        = 2,
    TYPE_META        = 3,
}MediaType;

typedef struct {
    struct CdxListNodeS node;
    unsigned PID;
    cdx_bool mPayloadStarted;
    CdxBufferT *mBuffer;
}PSISection;
/*
struct ESNode
{
    cdx_int64 mTimestampUs;
    const cdx_uint8 *data;
    size_t size;
    cdx_int64 pts;
    cdx_int64 durationUs;
};
*/
typedef struct {
    cdx_uint8 *bigBuf;
    cdx_uint32 bufSize;
    cdx_uint32 validDataSize;
    cdx_uint32 writePos;//标记要写入的位置
    cdx_uint32 readPos;//标记要读取的位置
    cdx_int32 endPos;//标记要读取数据的截止位置
}CacheBuffer;


typedef struct ProgramS Program;
typedef struct StreamS Stream;
typedef struct TSParserS TSParser;
typedef struct PacketS Packet;

#if DVB_USED
typedef struct {
    cdx_uint32 pid;
    cdx_uint32 counter;
    cdx_uint8 packet[188];
}PIDCounterMap;
#endif

struct TSParserS
{
    CdxParserT base;

    CdxStreamT *cdxStream;
    cdx_int64 fileSize;
    cdx_int64 fileValidSize;
    cdx_int32 isNetStream;
    cdx_int32 isDvbStream;
    cdx_uint64 durationMs;
    cdx_uint64 byteRate;
    cdx_int32 seekMethod;
    cdx_uint64 preOutputTimeUs;
    cdx_uint32 pidForProbePTS;
    //cdx_int64 preSeekPos;
    //cdx_int64 preSeekPTS;
    cdx_int32 hasAudioSync;
    cdx_int32 hasVideoSync;
    cdx_int32 syncOn;
    cdx_int64 videoPtsBaseUs;
    cdx_int64 audioPtsBaseUs;
    cdx_int64 commonPtsBaseUs;
    cdx_int64 vd_pts;
    cdx_int64 ad_pts;
    cdx_uint32 unsyncVideoFrame;
    cdx_uint32 unsyncAudioFrame;

    struct CdxListS mPSISections;
    struct CdxListS mPrograms;
    cdx_uint8 mProgramCount;
    unsigned pat_version_number;

    cdx_uint32 mRawPacketSize;
    cdx_int32 autoGuess;/*-1,选择节目结束，0依靠pat,pmt,1依靠猜测的pmt,2没有pat,pmt,构成杂散节目*/
    cdx_uint32 mStreamCount;

    cdx_uint64 mPCR[2];
    size_t mPCRBytes[2];
    cdx_int64 mSystemTimeUs[2];//目前没用
    size_t mNumPCRs;
    size_t mNumTSPacketsParsed;
    cdx_uint64 dynamicRate;//bytes/sec
    cdx_uint64 overallRate;//bytes/sec
    cdx_uint64 accumulatedDeltaPCR;
    //size_t pesCount;

    Stream *currentES;

    CacheBuffer mCacheBuffer;
    cdx_uint8 *tmpBuf;

    cdx_int32 hasVideo;
    Stream *curVideo;

    cdx_int32 hasMinorVideo;
    Stream *curMinorVideo;

    cdx_int32 hasAudio;
    //Stream *curAudio;

    cdx_int32 hasSubtitle;
    //Stream *curSubtitlePid;
    cdx_int32 hasMetadata;

    cdx_int32 bdFlag;
    Program *curProgram;
    cdx_uint8 enablePid[MAX_PID_NUM];/*0被丢弃，1被解析*/

    cdx_int32 cdxStreamEos;
    enum CdxParserStatus status;
    pthread_mutex_t statusLock;
    pthread_cond_t cond;
    cdx_uint32 attribute;
    cdx_bool forceStop;
    cdx_int32 mErrno;
    CdxMediaInfoT mediaInfo;
    CdxPacketT pkt;
    cdx_int32 sizeToReadEverytime;

    cdx_int32 needSelectProgram;
    cdx_int32 needUpdateProgram;

    cdx_int32 miracast;
    struct HdcpOpsS *hdcpOps;
    void *hdcpHandle;

    ParserCallback callback;
    void *pUserData;
    cdx_int32 endPosFlag;

    VideoStreamInfo tempVideoInfo;
    cdx_int64 firstPts;
    cdx_int32 mIslastSegment;
    cdx_int32 mClrInfo;
#if DVB_USED
    PIDCounterMap mapCounter[MAX_PID_NUM];
    cdx_int32 curCountPos;
    cdx_uint8 crcValidity[MAX_PID_NUM];
#endif
    cdx_bool b_hls_discontinue;
    cdx_bool b_steam_change;
    pthread_rwlock_t controlLock;
};

struct ProgramS
{
    struct CdxListNodeS node;
    cdx_int32 mProgramNumber;
    cdx_int32 mProgramMapPID;
    unsigned version_number;

    struct CdxListS mStreams;
    unsigned mStreamCount;

    unsigned audioCount;
    unsigned videoCount;//目前从流也算入Count
    unsigned subsCount;
    unsigned metaCount;
    TSParser *mTSParser;

    cdx_int64 mFirstPTS;
    //cdx_bool mFirstPTSValid;//
    cdx_int32 format;/*0普通ts，1HDMV*/
    cdx_int32 existInNewPat;

};
typedef struct PesS PES;
typedef struct AccessUnitS AccessUnit;
struct PesS
{
    CdxBufferT *mBuffer;
    cdx_int64 pts;
    //cdx_int64 durationUs;

    const cdx_uint8 *ESdata;
    size_t size;

    const cdx_uint8 *AUdata;
};
struct AccessUnitS
{
    //PES *head;
    //PES *tail;
    const cdx_uint8 *data;
    size_t size;
    cdx_int64 pts;
    cdx_int64 mTimestampUs;
    cdx_int64 durationUs;
};
typedef struct AudioMetaDataS AudioMetaData;
struct AudioMetaDataS
{
    cdx_int32 channelNum;
    cdx_int32 samplingFrequency;
    cdx_int32 bitPerSample;
    cdx_int32 bitRate;
    cdx_int32 maxBitRate;
};
typedef struct VideoMetaDataS VideoMetaData;
struct VideoMetaDataS
{
    cdx_int32 videoFormat;
    cdx_int32 frameRate;
    cdx_int32 aspectRatio;
    cdx_int32 ccFlag;

    unsigned width;
    unsigned height;
    unsigned bitRate;
};
struct StreamS
{
    struct CdxListNodeS node;
    Program *mProgram;
    unsigned mElementaryPID;
    cdx_int32 mStreamType;
    cdx_int32 streamIndex;

    //unsigned mPCR_PID;
    cdx_int32 mExpectedContinuityCounter;

    //CdxBufferT *mBuffer[2];//PES
    PES pes[2];
    cdx_int32 pesIndex;//标记当前要parse的pes,解完后反转
    cdx_uint8 *tmpBuf;
    unsigned tmpDataSize;
    AccessUnit accessUnit;
    cdx_bool accessUnitStarted;

    cdx_bool mPayloadStarted;
    char lang[4];
    MediaType mMediaType;
    unsigned codec_id;
    unsigned codec_sub_id;
    unsigned codec_meta_id;
    void *metadata;
    cdx_uint64 preAudioFrameDuration;

    cdx_int32 hasFirstPTS;
    cdx_uint64 firstPTS;

    cdx_int32 counter;
    //cdx_uint64 mPrevPTS;

    cdx_uint8 privateData[PES_PRIVATE_DATA_SIZE];
    cdx_int32 hdcpEncrypted;

    cdx_int32 existInNewPmt;

#if (PROBE_STREAM && !DVB_TEST)
    cdx_uint8 *probeBuf;
    unsigned probeBufSize;
    unsigned probeDataSize;
#endif
};

#endif

