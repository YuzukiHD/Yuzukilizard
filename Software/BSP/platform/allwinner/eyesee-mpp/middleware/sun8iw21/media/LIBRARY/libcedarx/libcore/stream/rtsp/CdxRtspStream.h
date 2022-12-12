/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxRtspStream.h
 * Description : Part of rtsp stream.
 * History :
 *
 */

#ifndef CDX_RTSP_STREAM_H
#define CDX_RTSP_STREAM_H

#include <stdint.h>
#include <CdxTypes.h>
#include <cdx_log.h>
#include <CdxStream.h>
#include <adecoder.h>
#include <vdecoder.h>
#include <CdxParser.h>
#include <CdxRtspSpec.h>

#include <Base64.hh>
#include <BasicUsageEnvironment.hh>
#include <UsageEnvironment.hh>
#include <liveMedia.hh>
#include <GroupsockHelper.hh>
#include <liveMedia_version.hh>

#include <semaphore.h>
#define __RTSP_SAVE_BITSTREAMS (0)

typedef struct RtspUrlS
{
    char *pProtocol;
    char *pUsername;
    char *pPassword;
    char *pHost;
    unsigned iPort;
    char *pPath;
    char *pOption;

    char *pBuffer; /* to be freed */
}RtspUrlT;

enum RtspPktType
{
    PKT_HEADER = 0,
    PKT_DATA,
};

typedef struct CdxRtspPktS
{
    CdxRtspPktHeaderT pktHeader;
    cdx_uint8 *pktData;
}CdxRtspPktT;

typedef struct CdxRtspPosS
{
    enum RtspPktType type;
    cdx_uint32 offset;
}CdxRtspPosT;

typedef struct EsFormatS
{
    int           iCat;              /**< ES category @see es_format_category_e */
    cdx_uint32    iCodec;            /**< FOURCC value as used in vlc */

    int     b_packetized;  /**< whether the data is packetized (ie. not truncated) */
    int     i_extra;        /**< length in bytes of extra data pointer */
    void    *p_extra;       /**< extra data needed by some decoders or muxers */

}EsFormatT;

typedef struct AsfHdrS
{
    cdx_int64   fileSize;
    cdx_int64   dataPktCount;
    cdx_int32   minDataPktSize;
}AsfHdrT;

typedef struct BufferS
{
    cdx_uint8 *pData;
    cdx_int32 nData;
    cdx_int32 nSize;
}BufferT;

//typedef cdx_uint8 asfGuid[16];
typedef struct asfGuids
{
    cdx_uint8 data[16];
}asfGuid;

typedef struct CdxRtspStreamImplS CdxRtspStreamImplT;

enum PayloadType {
    INVALID = -1,
    MP2T = 33,
};

typedef struct LiveTrackS
{
    CdxRtspStreamImplT *impl;
    MediaSubsession *sub;

    EsFormatT     fmt;
    enum PayloadType rtpPt;
    //es_out_id_t     *pEs;

    int            bMuxed;
    int            bQuicktime;
    int            bAsf;
    //block_t         *pAsfBlock;
    bool            bDiscardTrunc;
    //stream_t        *pOutmuxed;    /* for muxed stream */

    unsigned char   *pBuffer;
    unsigned int    iBuffer;

    int             bRtcpsync;
    char            waiting;
    cdx_int64       iPts;
    double          fNpt;

    int            bSelected;

} LiveTrackT;

class CdxRTSPClient;
typedef struct StreamSysS StreamSysT;

typedef struct TimeoutThreadS
{
    StreamSysT  *pSys;
    pthread_t   handle;
    int         b_handle_keep_alive;
    sem_t       sem;
}TimeoutThreadT;

struct StreamSysS
{
    char            *pSdp;      /* XXX mallocated */
    char            *pPath;     /* URL-encoded path */
    RtspUrlT        url;

    MediaSession     *ms;
    UsageEnvironment *env ;
    TaskScheduler    *scheduler;
    CdxRTSPClient    *rtsp;

    /* */
    int              iTrack;
    LiveTrackT     **track;

    AsfHdrT         asfh;
    int             bOutAsf;
    int             bReal;
    int             bOutMuxed;

    /* */
    cdx_int64        iPcr; /* The clock */
    double           fNpt;
    double           fNptLength;
    double           fNptStart;

    /* timeout thread information */
    int              iTimeout;     /* session timeout value in seconds */
    bool             b_timeout_call;/* mark to send an RTSP call to prevent server timeout */
    TimeoutThreadT  *pTimeout;    /* the actual thread that makes sure we don't timeout */

    /* */
    int             bForceMcast;
    int             bMulticast;   /* if one of the tracks is multicasted */
    int             bNoData;     /* if we never received any data */
    int              iNodatati;  /* consecutive number of TaskInterrupt */

    char             eventRtsp;
    char             eventData;

    int             bGetparam;   /* Does the server support GET_PARAMETER */
    int             bPaused;      /* Are we paused? */
    int             bError;
    int              iLive555ret; /* live555 callback return code */

    float            fSeekRequest; /* In case we receive a seek request while paused*/
};

enum RtspStreamStateE
{
    RTSP_STREAM_IDLE    = 0x00,
    RTSP_STREAM_CONNECTING = 0x01,
    RTSP_STREAM_READING = 0x02,
    RTSP_STREAM_SEEKING = 0x03,
};

typedef struct CdxRtspStreamImplS
{
    CdxStreamT base;
    StreamSysT *pSys;
    char *url;
    int bRtsphttp; //HTTP tunneling mode
    int bRtsptcp;
    CdxMediaInfoT mediaInfo;
    ExtraDataContainerT mediaInfoContainer;
    int bAsf;
    int bQuicktime;
    int bMuxed;
    int pOutMuxed;
    int bPktheaderRead; //has pkt header to be read
    int bPktdataRead;   //has pkt data to be read
    CdxRtspPosT mPos;
    //int ePkttype;
    int curTrack;
    CdxRtspPktT mCurptk;

    int nAudiotrack;
    int nVideotrack;

    CdxStreamProbeDataT probeData;
    cdx_int32 ioState;                          //for interface use
    int seekAble;                               //seekable flag.

    cdx_int64  readPos;
    cdx_uint8  *bufAsfheader;
    cdx_int32  bufAsfhSize;

#if __RTSP_SAVE_BITSTREAMS
    FILE *fp_rtsp;
    cdx_int32 streamIndx;
    cdx_char location[256];
#endif

    char *buffer;
    cdx_int64 maxBufSize;                       //max buffer size
    cdx_int64 validDataSize;
    cdx_char *bufReadPtr;                       //read pointer
    cdx_char *bufWritePtr;                      //write pointer
    cdx_char *bufEndPtr;                        //point to the end of buffer
    cdx_int64 bufPos;                           //stream data is buffered to this file pos.
    pthread_t threadId;
    pthread_mutex_t lock;                      //for break blocking operation
    pthread_cond_t cond;
    pthread_mutex_t bufferMutex;
    int exitFlag;

    cdx_char *sourceUrl;
    void *sftRtspSource;
    void *sft_stream_handle;
    cdx_int32 demuxType;
    pthread_mutex_t sft_handle_mutex;

    char *startTime;
    char *endTime;

    int  forceStop;
    enum RtspStreamStateE status;
}CdxRtspStreamImplT;

class CdxRTSPClient : public RTSPClient
{
public:
    CdxRTSPClient( UsageEnvironment& env, char const* pUrl, int nLevel,
                   char const* pAppName, portNumBits nTunnelOverHTTPPortNum,
                   StreamSysT *pSys) :
                   RTSPClient( env, pUrl, nLevel, pAppName,
                   nTunnelOverHTTPPortNum
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1373932800
                   , -1
#endif
                   )
    {
        this->pSys = pSys;
    }
    StreamSysT *pSys;
};

#define DEFAULT_PROBE_DATA_LEN (1024 * 2)

#endif
