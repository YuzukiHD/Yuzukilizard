/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxPmpParser.h
 * Description : pmp parser
 * History :
 *
 */

#ifndef PMP_PARSER_H
#define PMP_PARSER_H
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>

#define MaxProbePacketNum (1024)
#define SIZE_OF_VIDEO_PROVB_DATA (2*1024*1024)
enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    CDX_PSR_READING,
};

typedef struct {
    cdx_uint32 bKeyFrame    :1;
    cdx_uint32 nFrmSize     :31;
} DataFrameIndex;

typedef struct {
    cdx_uint32 frmID;                //* the frame number;
    cdx_uint32 filePos;              //* file position of the key frame;
} KeyFrameEntry;

typedef struct {
    cdx_uint32 audFrmNum;
    cdx_uint32 firstDelay;         //* time delay between the first audio frame
                                //* and the video frame in this packet;
    cdx_uint32 lastDelay;       //* time delay between the last audio frame
                                //* and the video frame in this packet;
    cdx_uint32 vdFrmSize;          //* size of video data in this packet;
    cdx_uint32 **auSize;           //* auSize[j][i] means the size of the ith
                                //* audio frame of the jth audio stream;
    cdx_uint64 vdPts;              //* pts of video frame in this packet;
    cdx_uint64 auPts;              //* pts of first audio frame in this packet;

    cdx_uint8 aac_hdr[7];         //* for AAC audio use.
    cdx_uint8 *tmpBuf;            //* temporarily used buffer to read packet header;
} DataFrameDescription;

typedef struct {
    cdx_uint8 *probeBuf;
    unsigned probeBufSize;
    unsigned probeDataSize;
} ProbeSpecificDataBuf;

typedef struct {
    CdxParserT base;
    cdx_uint32 flags;/*使能标志*/

    enum CdxParserStatus status;
    pthread_mutex_t statusLock;
    pthread_cond_t cond;

    int mErrno;
    int forceStop;

    CdxStreamT *file;
    CdxMediaInfoT mediaInfo;
    CdxPacketT cdxPkt;

    cdx_uint32 vdCodecID;      //* 0-MP4V(xvid, divx), 1-AVC(h264);
    cdx_uint32 vdFrmNum;       //* total frame number of this file
    cdx_uint32 vdWidth;        //* video frame width;
    cdx_uint32 vdHeight;       //* video frame height;
    cdx_uint32 vdScale;        //* video fps = rate/scale;
    cdx_uint32 vdRate;         //* used with vdScale to de

    cdx_uint32 auCodecID;      //* 0-MP3, 1-AAC;
    cdx_uint32 auStrmNum;      //* audio stream number in this file;
    cdx_uint32 maxAuPerFrame;  //* max audio frame within one packet;
    cdx_uint32 auScale;        //* default is 1152;
    cdx_uint32 auRate;         //* default is 44100;
    cdx_uint32 bStereo;        //* audio is Stereo;
    //cdx_uint32                         auId;           //* which audio stream is to be play;

    cdx_uint32 vdFrmInterval;  //* time interval between two video frames;//us
    cdx_uint64 durationUs;

    cdx_uint32 dataFrameCount;     //* current dataFrame, start counting from 1
    DataFrameDescription dataFrame; //* packet header information;
    int curStream;

    KeyFrameEntry* pKeyFrmIdx;    //* key frame index table;
    cdx_uint32 keyIdxNum;      //* how many entry in key frame index table;

    CdxPacketT *packets[MaxProbePacketNum];
    cdx_uint32 packetNum;
    cdx_uint32 packetPos;
    ProbeSpecificDataBuf vProbeBuf;
    VideoStreamInfo tempVideoInfo;
} CdxPmpParser;

#endif
