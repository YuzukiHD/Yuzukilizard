/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxDashParser.h
 * Description : DashParser
 * History :
 *
 */

#ifndef CDX_DASH_PARSER_H
#define CDX_DASH_PARSER_H

#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxAtomic.h>
#include "CdxMpd.h"
#include "CdxMovList.h"

#define DASH_DISABLE_AUDIO 0

typedef enum {
    // media segment
    CDX_DASH_RESOLVE_URL_MEDIA,
    // init segment
    CDX_DASH_RESOLVE_URL_INIT,
    CDX_DASH_RESOLVE_URL_INDEX,
    //same as CDX_DASH_RESOLVE_URL_MEDIA but does not replace $Time$ and $Number$
    CDX_DASH_RESOLVE_URL_MEDIA_TEMPLATE,
} CDX_DASHURLResolveType;

typedef struct _mediaStream CdxDashMedia;
struct _mediaStream
{
    //the streaminfo of httpCore, used for download media segemnt
    //CdxStreamT*                 httpCoreStreamInfo;

    // the parameter of HttpCoreOpen when download every media segment
    CdxDataSourceT*            datasource;
    int                        activeAdapSetIndex; // media type(audio/ video)
    int                        activeRepSetIndex;  // media bandwidth
    int                        activeSegmentIndex; // media segment( not include init segment )

    int                        eou;                   // end of updata url
    int                        eos;                   // end of download stream

    CdxParserT*                parser;                // the parser of this media stream
    CdxStreamT*                stream;                // this media stream

    /* for multi segments */
    int                     flag;              // flag of parserOpen, SEGMENT_MP4
    int64_t                 baseTime;          //us,  get the pts  for seek

    // is not used yet
    AW_List*                    packetList;            //the list of packet of this stream
};

typedef struct _CdxDashParser CdxDashParser;
struct _CdxDashParser
{
    CdxParserT             parserinfo;
    CdxStreamT*             stream;
    awMpd*                  mpd;
    CdxPacketT          packet;

    /**< mpd file url, use for getting the absolute url of every media segment */
    /**< if the mpd file is refresh( live play ), you should update the baseUrl in the same time */
    char*                mpdUrl;
    char*                mpdBuffer;
    cdx_int64            mpdSize;
    cdx_atomic_t        ref;

    char*                 tmpUrl;

    int                   hasVideo;
    int                      hasAudio;
    int                    hasSubtitle;
    int                 activePeriodIndex;

    /***< the thread id in ParserOpen, for unblocking in open.   */
    /***<  dowbload the mpd file and parse it, then create the media stream thread*/
    pthread_t            openTid;

    pthread_t            videoTid;        // thread to buffering data
    pthread_t            audioTid;
    pthread_t            mpdTid;         //thread to update mpd

    int                    switchFlag;      // switch the bitstream
    char                extraBuffer[1024];         // extradata buffer
    int                 extraBufferSize;

    int                    exitFlag;          // for forceStop()
    int                 mErrno;          // errno
    int                 mStatus;

    int                 prefetchType;  //0:video; 1:audio; 2:subtitle
    cdx_int64            streamPts[3];    // us
    CdxDashMedia         mediaStream[3];

    pthread_mutex_t     mutex;
};

#endif
