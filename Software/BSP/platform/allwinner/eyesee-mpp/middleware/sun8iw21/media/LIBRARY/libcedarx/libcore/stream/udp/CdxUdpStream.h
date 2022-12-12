/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxUdpStream.h
 * Description : UdpStream
 * History :
 *
 */

#ifndef UDP_STREAM_H
#define UDP_STREAM_H
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxStream.h>
#include <CdxAtomic.h>
#include <CdxUrl.h>

#include <arpa/inet.h>

#define BANDWIDTH_ARRAY_SIZE (100)

enum CdxStreamStatus
{
    STREAM_IDLE,
    STREAM_CONNECTING,
    STREAM_SEEKING,
    STREAM_READING,
};

typedef struct BandwidthEntry
{
    cdx_int64 downloadTimeCost;
    cdx_int32 downloadSize;
}BandwidthEntryT;

typedef struct CdxUdpStream
{
    CdxStreamT base;
    cdx_uint32 attribute;
    enum CdxStreamStatus status;
    cdx_int32 ioState;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int forceStop;
    cdx_int32 exitFlag;/*需要UdpDownloadThread退出*/
    int downloadThreadExit;/*UdpDownloadThread已经退出*/

    cdx_char *sourceUri;/*从dataSource获得的原始的uri*/
    CdxUrlT* url; /*scheme,port,etc*/
    CdxStreamProbeDataT probeData;
    int socketFd;
    int isMulticast;/*是否多播*/
    struct ip_mreq multicast;
    pthread_t threadId;/*UdpDownloadThread*/

    cdx_uint8 *bigBuf;
    cdx_uint32 bufSize;
    cdx_uint32 validDataSize;
    cdx_uint32 writePos;/*标记要写入的位置*/
    cdx_uint32 readPos;/*标记要读取的位置*/
    cdx_uint32 endPos;/*标记要读取数据的截止位置, 0表示尚没有被置起*/
    cdx_int64 accumulatedDownload;/*累计下载数据量*/
    pthread_mutex_t bufferMutex;

 //估计带宽之用
    BandwidthEntryT mBWArray[BANDWIDTH_ARRAY_SIZE];
    cdx_int32 mBWWritePos;
    cdx_int32 mBWCount;
    cdx_int64 mBWTotalTimeCost;
    cdx_int32 mBWTotalSize;

}CdxUdpStream;

#endif
