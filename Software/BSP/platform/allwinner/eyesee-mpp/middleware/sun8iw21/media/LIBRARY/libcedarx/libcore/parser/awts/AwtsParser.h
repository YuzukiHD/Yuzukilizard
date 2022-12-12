/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : AwtsParser.h
 * Description : Allwinner TS Parser Definition
 * History :
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

enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    CDX_PSR_READING,
};

typedef struct AwtsTrace
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
}AwtsTrace;

typedef struct AwtsParser
{
    CdxParserT base;
    enum CdxParserStatus status;
    pthread_mutex_t statusLock;
    int mErrno;
    cdx_uint32 flags;
    
    int forceStop;
    pthread_cond_t cond;
    
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
    AwtsTrace *tracks[AWTS_MAX_TRACE_COUNT];
}AwtsParser;

#endif
