/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmsStream.h
 * Description : MmsStream
 * History :
 *
 */

#ifndef CDX_MMS_STREAM_H
#define CDX_MMS_STREAM_H

#include<pthread.h>
#include<sys/types.h>
#include<unistd.h>
#include<stdio.h>
#include <stdint.h>
#include<string.h>
#include<stdlib.h>
#include "CdxUrl.h"
#include "CdxMmsBase.h"
#include <CdxStream.h>
#include <CdxAtomic.h>
#include "CdxSocketUtil.h"

enum MmsStreamStateE
{
    MMS_STREAM_IDLE        = 0x00L,
    MMS_STREAM_OPENING     = 0x01L,
    MMS_STREAM_READING     = 0x02L,
    MMS_STREAM_SEEKING     = 0x03L,
    MMS_STREAM_FORCESTOPPED = 0x04L,
};

#ifdef __cplusplus
extern "C" {
#endif

#if !HAVE_CLOSESOCKET
    #define closesocket close
#endif

//#define SAVE_MMS

#define MMS_PROBE_DATA_LEN 1024

//************************************************************//
//********* mms stram information*****************************//
//************************************************************//

// Definition of the stream type
    #define ASF_STREAMING_CLEAR            0x4324        // $C   // next stream comes
    #define ASF_STREAMING_DATA            0x4424        // $D
    #define ASF_STREAMING_END_TRANS        0x4524        // $E    //end of stream
    #define    ASF_STREAMING_HEADER        0x4824        // $H
    #define ASF_STREAMING_DATA2         0x44a4
    #define ASF_STREAMING_IGNORE        0x4d24      /* $M */

#ifdef MSG_NOSIGNAL
#define DEFAULT_SEND_FLAGS MSG_NOSIGNAL
#else
#define DEFAULT_SEND_FLAGS 0
#endif

#define AV_RB16(x)  ((((const cdx_uint8*)(x))[0] << 8) | ((const cdx_uint8*)(x))[1])
#define AV_WB16(p, d) do { \
                    ((cdx_uint8*)(p))[1] = (d); \
                    ((cdx_uint8*)(p))[0] = (d)>>8; } while(0)
#define AV_RL16(x)  ((((const cdx_uint8*)(x))[1] << 8) | \
                          ((const cdx_uint8*)(x))[0])
#define AV_WL16(p, d) do { \
                        ((cdx_uint8*)(p))[0] = (d); \
                        ((cdx_uint8*)(p))[1] = (d)>>8; } while(0)

#define AV_RB32(x)  ((((const cdx_uint8*)(x))[0] << 24) | \
                         (((const cdx_uint8*)(x))[1] << 16) | \
                         (((const cdx_uint8*)(x))[2] <<  8) | \
                          ((const cdx_uint8*)(x))[3])
#define AV_WB32(p, d) do { \
                        ((cdx_uint8*)(p))[3] = (d); \
                        ((cdx_uint8*)(p))[2] = (d)>>8; \
                        ((cdx_uint8*)(p))[1] = (d)>>16; \
                        ((cdx_uint8*)(p))[0] = (d)>>24; } while(0)

#define AV_WL32(p, d) do { \
                    ((cdx_uint8*)(p))[0] = (d); \
                    ((cdx_uint8*)(p))[1] = (d)>>8; \
                    ((cdx_uint8*)(p))[2] = (d)>>16; \
                    ((cdx_uint8*)(p))[3] = (d)>>24; } while(0)

#define le2me_16(x) (x)
#define le2me_32(x) (x)
#define le2me_64(x) (x)

#define le2me_ASF_stream_chunck_t(h) {                    \
    (h)->size = le2me_16((h)->size);                    \
    (h)->sequenceNumber = le2me_32((h)->sequenceNumber);        \
    (h)->unknown = le2me_16((h)->unknown);                \
    (h)->sizeConfirm = le2me_16((h)->sizeConfirm);            \
}

#define    ASF_LOAD_GUID_PREFIX(guid)    GetLE32(guid)
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))

#define MAX_STREAM_BUF_SIZE 12*1024*1024
typedef enum {
    STREAM_READ_INVALID = -2,
    STREAM_READ_ERROR = -1,
    STREAM_READ_END = 0,
    STREAM_READ_OK = 1
} stream_read_result_e;

#define PROTOCOL_MMS_T    0     //*  "mms" or "mmst"
#define PROTOCOL_MMS_H    1        //*  "mmsh"
#define PROTOCOL_MMS_HTTP 2        //*  "http"

typedef struct AW_MMS_STREAM_INF
{
    CdxStreamT             streaminfo;
    CdxStreamProbeDataT  probeData;

    char *uri;
//    CdxDataSourceT*     dataSource;
    cdx_atomic_t        ref;

    cdx_atomic_t        mState;
    int                 iostate;

    int                 fd;
   //* Connect to a server using a TCP connection
    // return -2 for fatal error, like unable to resolve name, connection timeout...
    // return -1 is unable to connect to a particular port
    // if success , return sockServerFd(sockFd)

    int                 eof;                  //* all stream data is read to buffer.
    int64_t             buf_pos;              //* stream data is buffered to this file pos.
    int64_t             buf_time_pos;         //* stream is buffered to this time pos.

    int64_t             dmx_read_pos;
   //* demuxer parses the stream to this file pos.the return value of Tell()

    int                    stream_buf_size;      //*cache size, set in StreamOPen(),

    CdxUrlT*             awUrl;                   //* url

    char*                 buffer;
   //* client buffer, store stream data from network and  demux read data from it
    char*                 bufReadPtr;           // the read pointer of the buffer
    char*                 bufWritePtr;          // the write pointer of the buffer
    char*                 bufReleasePtr;        // the release pointer of the buffer
    char*                bufEndPtr;            // pointer to the end of the buffer
    int                 validDataSize;        // the valid data size if the buffer
    int                 bufferDataSize;       // the total size of download from network
    char*                 asfChunkDataBuffer;   //* mmsh chunk recv buffer(32kb default)

    int                    mmsMode;              //* mmst or mmsh
    int                 sockFd;               //* socket fd

    int                 seqNum;
   //* client send packet to server need the sequence number

    int                 exitFlag;
    int                 closeFlag;

    /* the stream num of ASF_Stream_Properties_Object
          *(the unique number of vedio or audio and so on)
          * the range of stream number is 1-127, MMST_MAX_STREAMS is seted not so big*/
    int                 numStreamIds;         //*  number of stream(video, audio ...)
    int                 streamIds[20];        //*  stream number in ASF_Stream_Properties_Object
    int                 packetDataLen;
   //* Minimum Data Packet Size in ASF_File_Properties_Object,actually is the packet size

    pthread_mutex_t     bufferMutex;
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
    pthread_t            downloadTid;

    void*                 data;                 //* pointer to asfHttpCtrl
    void*                httpCore;              //* pointer to httpStreamInf

    int                 playListFlag;                    //* ASF_TYPE_PLAYLIST

    /* the buffer of storing store the first http packet entry body data
     * (socket buffer without the http header)
     *  to parse the asf header, the first chunk is recv to the buffer,
     *  so mmsStreamInf->buffer need copy this data first  */
    int                 asfHttpBufSize;                    //* the max size of the buffer
    char*                 asfHttpDataBuffer;                //* the start address of the buffer
    int                 asfHttpDataSize;                //* the data size of this buffer
    int                 asfHttpBufPos;                  //* the valid data start address of buffer

    int64_t             fileSize;
   //* entire size of the ASF file,
   //equal to filesize in aw_asf_file_header_t(in ASF_File_Properties_Object)
    int64_t             fileDuration;
   //* entire play time of the ASF file( in ASF_File_Properties_Object)

    char*                firstChunk;
   //* the first chunk data(chunk header and body),
   //used for parse asf_header_object in mms_http_streaming_start
    int                 firstChunkBodySize;
    int                 firstChunkPos;

    // for mms playlist
    char*                 tmpReadPtr;           // for reset bufReadPtr in mms playlist
    char*                 tmpReleasePtr;        // the release pointer of the buffer
    int64_t             tmp_dmx_read_pos;

    #ifdef    SAVE_MMS
    FILE*     fp;
    #endif
} aw_mms_inf_t;

// Definition of the differents type of ASF streaming
typedef enum {
    ASF_TYPE_UNKNOWN,
    ASF_TYPE_LIVE,
    ASF_TYPE_PLAYLIST,
    ASF_TYPE_PRERECORDED,
    ASF_TYPE_REDIRECTOR,
    ASF_TYPE_PLAINTEXT,
    ASF_TYPE_AUTHENTICATE
} asf_StreamType_e;

typedef struct {
    asf_StreamType_e     streamingType;
    int                 request;                 //*  first request only recv the asf header
    int                    packetSize;
   //* ASF every packet size, equal to the maxPacketSize in aw_asf_file_header_t
    int*                audioStreams;
   //* every audio stream have diffreant audio number
   //(不同音轨对应ASF_Stream_Properties_Object中不同的stream num)
    int                 nAudio;                    //* number of audio stream (音轨数)
    int*                videoStreams;
    int                 nVideo;
    int                 audioId;
   //* chose the proper audio bumber according to bandwith
    int                 videoId;
   //* chose the proper vedio bumber according to bandwith
} aw_mmsh_ctrl_t;

//chunk header in mmsh
typedef struct {
    unsigned short         type;
    unsigned short         size;                //* chunk size
    unsigned int         sequenceNumber;
    unsigned short         unknown;
    unsigned short         sizeConfirm;        //* equal to size
} aw_asf_stream_chunck_t;

typedef struct {
    unsigned char             guid[16];
    unsigned long long         size;
} aw_asf_obj_header_t;

/* ASF_Header_Object */
typedef struct {
    aw_asf_obj_header_t     objh;
    unsigned int             cno;     // number of objects contain with the Header Object
    unsigned char             v1;     // unknown (0x01)
    unsigned char             v2;     // unknown (0x02)
} aw_asf_header_t;

//* the data after GUID and object size in ASF_File_Properties_Object
typedef struct __attribute__((packed)) {
    unsigned char             streamId[16];          // stream GUID
    unsigned long long         fileSize;            // the size of the entire ASF file
    unsigned long long         creationTime;          // File creation time FILETIME 8
    unsigned long long         numPackets;            // Number of packets UINT64 8
    unsigned long long         playDuration;          // Timestamp of the end position UINT64 8
    unsigned long long         sendDuration;          // Duration of the playback UINT64 8
    unsigned long long         preroll;             // Time to bufferize before playing UINT32 4
    unsigned int             flags;
   // Unknown, maybe flags ( usually contains 2 ) UINT32 4
    unsigned int             minPacketSize;         // Min size of the packet, in bytes UINT32 4
    unsigned int             maxPacketSize;
   // Max size of the packet(equal to minPacketSize usually )  UINT32 4
    unsigned int             maxBitrate;
   // Maximum bitrate of the media (sum of all the stream)
} aw_asf_file_header_t;

#define OFFSET_ASF_HEADER(x)    offsetof(aw_asf_file_header_t, x)

/*
 * the data after GUID and object size in ASF_Stream_properties_Obeject
 */
typedef struct {
    unsigned char             type[16];            // Stream type (audio/video) GUID 16
    unsigned char             concealment[16];     // Audio error concealment type GUID 16
    unsigned long long         unk1;
   // Time offset ,Unknown, maybe reserved ( usually contains 0 ) UINT64 8
    unsigned int             typeSize;            // Total size of type-specific data UINT32 4
    unsigned int             streamSize;          // Size of stream-specific data UINT32 4
    unsigned short             streamNo;            // Stream number UINT16 2
    unsigned int             unk2;                // Resvered,  Unknown UINT32 4
} aw_asf_stream_header_t;

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#ifdef __cplusplus
}
#endif

#endif

