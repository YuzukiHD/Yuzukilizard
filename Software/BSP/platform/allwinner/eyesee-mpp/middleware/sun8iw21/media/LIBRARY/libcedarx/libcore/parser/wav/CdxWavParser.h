/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxWavParser.h
* Description :
* History :
*   Author  : Ls Zhang <lszhang@allwinnertech.com>
*   Date    : 2014/08/08
*/

#ifndef CDX_WAV_PARSER_H
#define CDX_WAV_PARSER_H

#define WAV_MAX(a,b) (a>b?a:b)
#define WAV_MIN(a,b) (a<b?a:b)

typedef struct WavObjectStruct
{
    cdx_int32    UID;
    cdx_int32    Dsize;
}WAVObjectT;

typedef struct WaveFormatStruct
{
    cdx_uint16 wFormatag;  //编码格式，包括WAVE_FORMAT_PCM，//WAVEFORMAT_ADPCM等
    cdx_int16 nChannls;       //声道数，单声道为1，双声道为2;
    cdx_int32 nSamplesPerSec;//采样频率；
    cdx_int32 nAvgBytesperSec;//每秒的数据量；
    cdx_int16 nBlockAlign;//块对齐；
    cdx_int16 wBitsPerSample;//WAVE文件的采样大小；
    cdx_int16 sbSize;        //PCM中忽略此值
    cdx_int16 nSamplesPerBlock;
    cdx_int32    nDataCKSzie;            //length of data chunk
}WavFormatT;

typedef struct WavParserImplS
{
    //audio common
    CdxParserT  base;
    CdxStreamT  *stream;
    pthread_cond_t  cond;

    cdx_int64   ulDuration;//ms
    cdx_int64   dFileSize;//total file length
    cdx_int64   dFileOffset; //now read location
    cdx_int32   mErrno; //Parser Status
    cdx_uint32  nFlags; //cmd
    //wav base
    WavFormatT  WavFormat;
    cdx_int64   pts;
    cdx_int32   nHeadLen;
    cdx_int32   nFrames;//samples
    cdx_int32   nFrameLen;//bytes of frame(maybe nBlockAlign)
    cdx_int32   nFrameSamples;//samples of frame(maybe nSamplesPerBlock)
    cdx_int32   nBigEndian;
}WavParserImplS;

#define    RIFX    0X58464952
#define    RIFF    0X46464952
#define    WAVE    0X45564157
#define    fmt     0x20746d66
#define    fact    0x74636166
#define    dataF   0x61746164

#define READBUF_SIZE_1 3*512
#define FFMAXBUFLEN (0x01<<15)

#endif
