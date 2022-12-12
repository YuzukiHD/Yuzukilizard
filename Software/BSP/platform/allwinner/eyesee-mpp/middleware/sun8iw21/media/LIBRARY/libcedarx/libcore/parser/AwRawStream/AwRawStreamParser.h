/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : AwRawStreamParser.h
* Description :parse raw video stream
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment : parse raw video stream
*
*
*/

#ifndef AWTS_PARSER_H
#define AWTS_PARSER_H
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>
#define AWTS_MAX_TRACE_COUNT 2

#define AW_RAW_STREAM_BUF_SIZE (4*1024*1024)

enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    CDX_PSR_READING,
};

typedef struct RawStreamTrace
{
    cdx_uint32 duration;
    //cdx_uint8 trackIndex;
    cdx_uint8 version;
    cdx_uint8 streamIndex;

    enum CdxMediaTypeE eType;
    int eCodecFormat;
    int nCodecSpecificDataLen;
    char* pCodecSpecificData;

    int nBitrate;
    int nBitsPerSample;
    int nChannelNum;
    int nSampleRate;
    int nWidth;
    int nHeight;
}RawStreamTrace;


typedef struct RAW_STREAM_PARSER
{
    CdxParserT base;
    enum CdxParserStatus status;
    pthread_mutex_t statusLock;
    int mErrno;
    cdx_uint32 flags;

    int forceStop;
    cdx_atomic_t ref;
    pthread_mutex_t refLock;
    pthread_t openTid;

    CdxStreamT *file;
    CdxMediaInfoT mediaInfo;
    CdxPacketT pkt;

    cdx_uint8 packetNumInStream;
    cdx_uint32 curPacketLength;
    cdx_uint8  curTraceIndex;
    cdx_uint8  curPacketFlags;
    //cdx_uint16 sequenceNumInPacket;
    cdx_int64 timeUs;

    cdx_uint8  isPacketHeader;
    cdx_uint8  isKeyFrame;
    cdx_uint8  sequenceNumIncr;

    int trackCount;

    cdx_uint8 *pRawStreamBuf;
    cdx_uint32  nRawStreamBufSize;
    int bEndofStream;

    RawStreamTrace *tracks[AWTS_MAX_TRACE_COUNT];
    VideoStreamInfo tempVideoInfo;
}RawStreamParser;

typedef enum RAW_STREAM_TYPE
{
    h265_raw_stream = 0,
    h264_raw_stream
}RawStreamTyep;

#endif
