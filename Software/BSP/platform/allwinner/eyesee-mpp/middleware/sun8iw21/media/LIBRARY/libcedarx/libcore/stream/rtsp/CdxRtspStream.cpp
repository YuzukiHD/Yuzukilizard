/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxRtspStream.cpp
 * Description : Rtsp stream implementation.
 * History :
 *
 */

//#define CONFIG_LOG_LEVEL 4

#include <CdxRtspStream.h>
#include <assert.h>
#include <dlfcn.h>
#include <time.h>

#define LOG_VERBOSE 0
#define FRAME_BUFFER_SIZE 100000

#define RTSP_MAX_STREAM_BUF_SIZE (10*1024*1024)

#if __RTSP_SAVE_BITSTREAMS
static int streamIndx = 0;
#endif

#define RTSP_FOURCC( a, b, c, d ) \
        (((cdx_uint32)a) | (((cdx_uint32)b) << 8)| \
        (((cdx_uint32)c) << 16 ) | (((cdx_uint32)d) << 24))

#define TABLE_INIT(n, t)                \
    do {                                \
        (n) = 0;                        \
        (t) = NULL;                     \
    } while(0)

#define TABLE_APPEND_CAST(cast, n, t, p)             \
      do {                                          \
        if( (n) > 0 )                           \
            (t) = cast realloc( t, sizeof( void ** ) * ( (n) + 1 ) ); \
        else                                        \
            (t) = cast malloc( sizeof( void ** ) );    \
        if( !(t) ) abort();                       \
        (t)[n] = (p);                         \
        (n)++;                                  \
      } while(0)

#define TABLE_CLEAN(count, tab) \
    do {                        \
        free(tab);              \
        (count) = 0;            \
        (tab) = NULL;           \
    } while (0)

#ifdef __cplusplus
extern "C"
{
#endif

static int RtspConnect(CdxRtspStreamImplT *impl);
static int RtspSessionsSetup(CdxRtspStreamImplT *impl);
static int RtspPlay(CdxRtspStreamImplT *impl);
static int Base64Decode( cdx_uint8 *pDst, int iDst, const char *pSrc );

/**
 * Converts a UTF-8 nul-terminated IDN to nul-terminated ASCII domain name.
 * \param idn UTF-8 Internationalized Domain Name to convert
 * \return a heap-allocated string or NULL on error.
 */
static char *IdnaToAscii (const char *idn)
{
    const char *p;
    for (p = idn; *p; p++)
        if (((unsigned char)*p) >= 0x80)
            return NULL;

    return strdup (idn);
}

char *DecodeUri (char *str)
{
    char *pIn = str, *pOut = str;
    if (pIn == NULL)
    {
        loge("check param.");
        return NULL;
    }

    char c;
    while ((c = *(pIn++)) != '\0')
    {
        if (c == '%')
        {
            char cHex[3];

            if (!(cHex[0] = *(pIn++)) || !(cHex[1] = *(pIn++)))
            {
                return NULL;
            }
            cHex[2] = '\0';
            *(pOut++) = strtoul (cHex, NULL, 0x10);
        }
        else
        {
            *(pOut++) = c;
        }
    }
    *pOut = '\0';

    return str;
}

void RtspUrlParse (RtspUrlT *url, const char *pStr, unsigned char opt)
{
    url->pProtocol = NULL;
    url->pUsername = NULL;
    url->pPassword = NULL;
    url->pHost = NULL;
    url->iPort = 0;
    url->pPath = NULL;
    url->pOption = NULL;
    url->pBuffer = NULL;

    if (pStr == NULL)
    {
        logd("check param");
        return;
    }
    char *buf = strdup(pStr);
    if (buf == NULL)
    {
        loge("strdup failed.");
        abort();
    }
    url->pBuffer = buf;

    char *pCur = buf;
    char *pNext = buf;

    /* URL scheme */
    while ((*pNext >= 'A' && *pNext <= 'Z') || (*pNext >= 'a' && *pNext <= 'z')
        || (*pNext >= '0' && *pNext <= '9') || (strchr ("+-.", *pNext) != NULL))
    {
        pNext++;
    }

    if (!strncmp (pNext, "://", 3))
    {
        *pNext = '\0';
        pNext += 3;
        url->pProtocol = pCur;
        pCur = pNext;
    }

    /* Path */
    pNext = strchr (pCur, '/');
    if (pNext != NULL)
    {
        *pNext = '\0'; /* temporary nul, reset to slash later */
        url->pPath = pNext;
        if (opt && (pNext = strchr (pNext, opt)) != NULL)
        {
            *(pNext++) = '\0';
            url->pOption = pNext;
        }
    }

    /* User name */
    pNext = strrchr (pCur, '@');
    if (pNext != NULL)
    {
        *(pNext++) = '\0';
        url->pUsername = pCur;
        pCur = pNext;

        /* Password (obsolete) */
        pNext = strchr (url->pUsername, ':');
        if (pNext != NULL)
        {
            *(pNext++) = '\0';
            url->pPassword = pNext;
            DecodeUri (url->pPassword);
        }
        DecodeUri (url->pUsername);
    }

    /* Host name */
    if (*pCur == '[' && (pNext = strrchr (pCur, ']')) != NULL)
    {   /* Try IPv6 numeral within brackets */
        *(pNext++) = '\0';
        url->pHost = strdup (pCur + 1);

        if (*pNext == ':')
            pNext++;
        else
            pNext = NULL;
    }
    else
    {
        pNext = strchr (pCur, ':');
        if (pNext != NULL)
            *(pNext++) = '\0';

        url->pHost = IdnaToAscii (pCur);
    }

    /* Port number */
    if (pNext != NULL)
        url->iPort = atoi (pNext);

    if (url->pPath != NULL)
        *url->pPath = '/'; /* restore leading slash */
}

static int RtspRollOverTcp( CdxRtspStreamImplT *impl) //* todo
{
    StreamSysT *pSys = impl->pSys;
    int i, iReturn;

    impl->bRtsptcp = 1;

    /* We close the old RTSP session */
    pSys->rtsp->sendTeardownCommand( *pSys->ms, NULL );
    Medium::close( pSys->ms );
    RTSPClient::close( pSys->rtsp );

    for( i = 0; i < pSys->iTrack; i++ )
    {
        LiveTrackT *tk = pSys->track[i];
        free(tk->pBuffer);
        free(tk);
    }
    TABLE_CLEAN(pSys->iTrack, pSys->track);

    //if( pSys->bOutAsf ) stream_Delete( pSys->p_out_asf );

    pSys->ms = NULL;
    pSys->rtsp = NULL;
    pSys->bNoData = 1;
    pSys->iNodatati = 0;
    pSys->bOutAsf = 0;

    /* Reopen rtsp client */
    if( ( iReturn = RtspConnect( impl ) ) != 0 )
    {
        loge(  "Failed to connect with rtsp://%s",
                 pSys->pPath );
        goto error;
    }

    if( pSys->pSdp == NULL )
    {
        loge( "Failed to retrieve the RTSP Session Description" );
        goto error;
    }

    if( ( iReturn = RtspSessionsSetup( impl ) ) != 0 )
    {
        loge(  "Nothing to play for rtsp://%s", pSys->pPath );
        goto error;
    }

    if( ( iReturn = RtspPlay( impl ) ) != 0 )
        goto error;

    return 0;

error:
    return -1;
}

static void RtspStreamClose( void *p_private )
{
    LiveTrackT   *tk = (LiveTrackT *)p_private;
    CdxRtspStreamImplT *impl = tk->impl;
    StreamSysT   *pSys = impl->pSys;
    tk->bSelected = 0;
    pSys->eventRtsp = 0xff;
    pSys->eventData = 0xff;

   // if( tk->p_es )
   //     es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, tk->p_es, false );

    int nTracks = 0;
    for( int i = 0; i < pSys->iTrack; i++ )
    {
        if( pSys->track[i]->bSelected )
            nTracks++;
    }
    logd( "RTSP track Close, %d track remaining", nTracks );
    if( !nTracks )
        pSys->bError = 1;

}

static int ReadAsfDataToBuffer(CdxRtspStreamImplT *impl, char bMarker, void *buf, cdx_uint32 len)
{
    cdx_uint8 *data = (cdx_uint8 *)buf;
    const cdx_uint32 pktSize = impl->pSys->asfh.minDataPktSize;
    cdx_uint8 *tempBuf = NULL;
    cdx_uint32  iBuf = 0;

    logv("========== len(%u), pktSize(%u)", len, pktSize);

    //* get rid of rtp payload format header. [C0 00 0A EA 82 00...]
    while(len >= 4)
    {
        if(impl->forceStop || impl->exitFlag)
        {
            if (NULL != tempBuf)
            {
                free(tempBuf);
            }
            pthread_mutex_lock(&impl->lock);
            impl->ioState = CDX_IO_STATE_ERROR;
            pthread_mutex_unlock(&impl->lock);
            return -1;
        }
        cdx_uint32 iFlags        = data[0];
        cdx_uint32 iLengthOffset = (data[1] << 16) | (data[2] <<  8) | (data[3]);
        cdx_uint8 bLength         = iFlags & 0x40;
        cdx_uint8 bRelativeTs     = iFlags & 0x20;
        cdx_uint8 bDuration       = iFlags & 0x10;
        cdx_uint8 bLocationId     = iFlags & 0x08;

        cdx_uint32 iHdrsize = 4;
        if(bRelativeTs)
            iHdrsize += 4;
        if(bDuration)
            iHdrsize += 4;
        if(bLocationId)
            iHdrsize += 4;

        if(iHdrsize > len)
        {
            logw("Invalid header size");
            break;
        }

        /*
         * When b_length is true, the streams I found do not seems to respect
         * the documentation.
         * From them, I have failed to find which choice between '__MIN()' or
         * 'i_length_offset - i_header_size' is the right one.
         */
        cdx_uint32 iPayload;
        if( bLength )
            iPayload = CedarXMin(iLengthOffset, len - iHdrsize);
        else
            iPayload = len - iHdrsize;

        if(!tempBuf)
        {
            tempBuf = (cdx_uint8 *)malloc(pktSize);
            if(!tempBuf)
            {
                loge("malloc failed.");
                return -1;
            }
            iBuf = 0;
        }

        cdx_uint32 iOffset  = bLength ? 0 : iLengthOffset;

        if(iOffset == iBuf && iOffset + iPayload <= pktSize)
        {
            //* store asf data.
            memcpy(&tempBuf[iOffset], &data[iHdrsize], iPayload);
            iBuf += iPayload;

            if(bMarker)
            {
                //* complete packet
                logv("=========== iBuf(%u), pktSize(%u)", iBuf, pktSize);
                iBuf = pktSize;
                memcpy(impl->bufWritePtr, tempBuf, iBuf);
#if __RTSP_SAVE_BITSTREAMS
                int n;
                n = fwrite(impl->bufWritePtr, 1, iBuf, impl->fp_rtsp);
                CDX_LOGD("xxx write size(%d), n=(%d), errno(%d), impl->fp_rtsp(%p)", iBuf, n,
                    errno, impl->fp_rtsp);
                fsync(fileno(impl->fp_rtsp));
#endif
                pthread_mutex_lock(&impl->bufferMutex);
                impl->bufWritePtr   += iBuf;
                impl->validDataSize += iBuf;
                impl->bufPos        += iBuf;
                if(impl->bufWritePtr > impl->bufEndPtr)
                {
                    impl->bufWritePtr -= impl->maxBufSize;
                }
                pthread_mutex_unlock(&impl->bufferMutex);

                free(tempBuf);
                tempBuf = NULL;
            }
        }
        else
        {
            logd("Broken packet detected (%u vs %u or %u + %u vs %u)",
                iOffset, iBuf, iOffset, iPayload, pktSize);
            iBuf = 0;
        }

        data += iHdrsize + iPayload;
        len  -= iHdrsize + iPayload;
    }

#if 0
    memcpy(impl->bufWritePtr, buf, len);

#if __RTSP_SAVE_BITSTREAMS
    int n;
    n = fwrite(buf, 1, len, impl->fp_rtsp);
    CDX_LOGD("xxx write size(%d), n=(%d), errno(%d), impl->fp_rtsp(%p)",
        len, n, errno, impl->fp_rtsp);
    fsync(fileno(impl->fp_rtsp));
#endif

    pthread_mutex_lock(&impl->bufferMutex);
    impl->bufWritePtr   += len;
    impl->validDataSize += len;
    impl->bufPos        += len;
    if(impl->bufWritePtr > impl->bufEndPtr)
    {
        impl->bufWritePtr -= impl->maxBufSize;
    }
    pthread_mutex_unlock(&impl->bufferMutex);

    logd("================ impl->validDataSize(%lld)", impl->validDataSize);
#endif

    return 0;
}

static int WriteToBuffer(CdxRtspStreamImplT *impl, void *buf, int len)
{
    int freeSpace;
    int copySize;
    while (len > 0) {
        if(impl->forceStop)
        {
            pthread_mutex_lock(&impl->lock);
            impl->ioState = CDX_IO_STATE_ERROR;
            pthread_mutex_unlock(&impl->lock);
            return -1;
        }

        pthread_mutex_lock(&impl->bufferMutex);
        if (impl->bufWritePtr > impl->bufEndPtr)
            impl->bufWritePtr -= impl->maxBufSize;

        if (impl->bufWritePtr == impl->bufReadPtr && impl->validDataSize > 0)
        {
            pthread_mutex_unlock(&impl->bufferMutex);
            logd("buffer is full...");
            usleep(50000);
            continue;
        }

        if (impl->bufWritePtr < impl->bufReadPtr)
            freeSpace = impl->bufReadPtr - impl->bufWritePtr;
        else
            freeSpace = impl->bufEndPtr - impl->bufWritePtr + 1;

        copySize = freeSpace >= len ? len : freeSpace;
        memcpy(impl->bufWritePtr, buf, copySize);
        impl->bufWritePtr += copySize;
        impl->validDataSize += copySize;
        impl->bufPos += copySize;
        len -= copySize;
        pthread_mutex_unlock(&impl->bufferMutex);
    }

    return 0;
}

static void RtspStreamRead( void *p_private, unsigned int i_size,
                        unsigned int i_truncated_bytes, struct timeval pts,
                        unsigned int duration )
{
    CDX_UNUSE( duration );

    LiveTrackT *tk           = (LiveTrackT *)p_private;
    CdxRtspStreamImplT *impl = tk->impl;
    StreamSysT         *pSys = impl->pSys;

    //logd( "pts: %lld", pts.tv_sec );

    cdx_int64 iPts = (cdx_int64)pts.tv_sec * (1000000LL) + (cdx_int64)pts.tv_usec;

    // Avoid having negative value
    iPts &= (0x00ffffffffffffffLL);

    /* Retrieve NPT for this pts */
    tk->fNpt = tk->sub->getNormalPlayTime(pts);
    //logd("========== iPts(%lld), tk->fNpt(%lld)", iPts, (cdx_int64)tk->fNpt);

    logv( "StreamRead size=%d pts=%lld", i_size, pts.tv_sec * 1000000LL + pts.tv_usec );

    // grow buffer if it looks like buffer is too small, but don't eat
    // up all the memory on strange streams
    if( i_truncated_bytes > 0 )
    {
        if( tk->iBuffer < 2000000 )
        {
            void *ptmp;
            logd( "lost %d bytes", i_truncated_bytes );
            logd( "increasing buffer size to %d", tk->iBuffer * 2 );
            ptmp = realloc( tk->pBuffer, tk->iBuffer * 2 );
            if(ptmp == NULL)
            {
                logw( "realloc failed" );
            }
            else
            {
                tk->pBuffer = (unsigned char *)ptmp;
                tk->iBuffer *= 2;
            }
        }

        if( tk->bDiscardTrunc )
        {
            pSys->eventData = 0xff;
            tk->waiting = 0;
            return;
        }
    }

    assert( i_size <= tk->iBuffer );

    if( tk->fmt.iCodec == AUDIO_CODEC_FORMAT_AMR )
    {
        AMRAudioSource *amrSource = (AMRAudioSource*)tk->sub->readSource();

        impl->mCurptk.pktData = (cdx_uint8 *)malloc( i_size + 1 );
        assert(impl->mCurptk.pktData);
        impl->mCurptk.pktData[0] = amrSource->lastFrameHeader();
        memcpy( impl->mCurptk.pktData + 1, tk->pBuffer, i_size );
        impl->mCurptk.pktHeader.length = i_size + 1;
    }
    else if( tk->fmt.iCodec == VIDEO_CODEC_FORMAT_H264 ) //* do it in remux parser
    {
        if( (tk->pBuffer[0] & 0x1f) >= 24 )
            logw(  "unsupported NAL type for H264" );

        /* Normal NAL type */
        impl->mCurptk.pktData = (cdx_uint8 *)malloc( i_size + 4 );
        assert(impl->mCurptk.pktData);
        impl->mCurptk.pktData[0] = 0x00;
        impl->mCurptk.pktData[1] = 0x00;
        impl->mCurptk.pktData[2] = 0x00;
        impl->mCurptk.pktData[3] = 0x01;
        memcpy( &impl->mCurptk.pktData[4], tk->pBuffer, i_size );

        impl->mCurptk.pktHeader.length = i_size + 4;
        logv("========== h264 i_size(%u), impl->mCurptk.pktData(%p)", i_size,
            impl->mCurptk.pktData);
    }
#if 1
    else if( tk->bAsf )
    {
        logv("read rtsp data... first byte:0x%x", tk->pBuffer[0]);
        ReadAsfDataToBuffer(impl, tk->sub->rtpSource()->curPacketMarkerBit(), tk->pBuffer, i_size);

    }
#endif
    else if (tk->rtpPt == MP2T)
    {
        WriteToBuffer(impl, tk->pBuffer, i_size);
    }
    else
    {
        impl->mCurptk.pktData = (cdx_uint8 *)malloc( i_size );
        assert(impl->mCurptk.pktData);
        memcpy( impl->mCurptk.pktData, tk->pBuffer, i_size );
        impl->mCurptk.pktHeader.length = i_size;
    }

    if( pSys->iPcr < iPts )
    {
        pSys->iPcr = iPts;
    }

    /* Update our global npt value */
    if( tk->fNpt > 0 &&
        ( tk->fNpt < pSys->fNptLength || pSys->fNptLength <= 0 ) )
        pSys->fNpt = tk->fNpt;

    if( impl->mCurptk.pktData )
    {
        if( !tk->bMuxed && !tk->bAsf )
        {
            //if( iPts != tk->iPts )
                impl->mCurptk.pktHeader.pts = iPts;

            logv("impl->mCurptk.pktHeader.pts(%lld)", impl->mCurptk.pktHeader.pts);
        }
#if 0
        if( tk->b_muxed )
            stream_DemuxSend( tk->p_out_muxed, p_block );
        else if( tk->b_asf )
            stream_DemuxSend( p_sys->p_out_asf, p_block );
#endif
    }

    /* warn that's ok */
    pSys->eventData = 0xff;

    /* we have read data */
    tk->waiting = 0;
    impl->pSys->bNoData = 0;
    impl->pSys->iNodatati = 0;

    impl->mCurptk.pktHeader.type = tk->fmt.iCat;

    if(iPts > 0 && !tk->bMuxed)
    {
        tk->iPts = iPts;
    }

    logv("========== RtspStreamRead type(%d), length(%d), pts(%lld)", impl->mCurptk.pktHeader.type,
        impl->mCurptk.pktHeader.length, impl->mCurptk.pktHeader.pts);
}

static void CdxTaskInterruptData(void *pPrivate)
{
    CdxRtspStreamImplT *impl = (CdxRtspStreamImplT*)pPrivate;

    impl->pSys->iNodatati++;

    /* Avoid lock */
    impl->pSys->eventData = 0xff;
}

static int GetOneFrame(CdxRtspStreamImplT *impl)
{
    StreamSysT *pSys = impl->pSys;
    TaskToken task;

    int            bSendpcr = 1;
    cdx_int64      iPcr = 0;
    int            i;

    //* decide which track to read
    impl->curTrack = 0;
    if(pSys->iTrack >= 2)
    {
        if(pSys->track[0]->iPts > pSys->track[1]->iPts)
        {
            impl->curTrack = 1;
        }
        else
            impl->curTrack = 0;

        //* check
        if(pSys->bOutAsf)
        {
            if(pSys->track[0]->fmt.iCat == CDX_MEDIA_VIDEO)
                impl->curTrack = 0;
            else
                impl->curTrack = 1;
        }
    }

    logv("===========curTrack(%d), pSys->iTrack(%d)", impl->curTrack, pSys->iTrack);

    LiveTrackT *tk = pSys->track[impl->curTrack];

    pSys->eventData = 0;
    if( tk->waiting == 0 )
    {
        tk->waiting = 1;
        tk->sub->readSource()->getNextFrame( tk->pBuffer, tk->iBuffer,
            RtspStreamRead, tk, RtspStreamClose, tk );
            //* 注册具体处理函数RtspStreamRead，读取数据，以便给处理函数处理
    }

    /* Create a task that will be called if we wait more than 10s
     * Do it for udp unicast to check if we need to switch to tcp
     */
    if (!impl->bRtsptcp && !pSys->bMulticast)
        task = pSys->scheduler->scheduleDelayedTask(10 * 1000 * 1000, CdxTaskInterruptData, impl);

    //* Do the read
    pSys->scheduler->doEventLoop( &pSys->eventData );
    //* 调用RtspStreamRead，读到数据后改变eventData使doEventLoop返回

    //* remove the task
    if (!impl->bRtsptcp && !pSys->bMulticast)
        pSys->scheduler->unscheduleDelayedTask(task);

    if( pSys->bMulticast && pSys->bNoData &&
        ( pSys->iNodatati > 120 ) )
    {

    }
    else if( !pSys->bMulticast && !pSys->bPaused &&
              pSys->bNoData && ( pSys->iNodatati > 0 ) )
    {
        logw("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        int bRtsptcp = impl->bRtsphttp || impl->bRtsptcp;

        if( !bRtsptcp && pSys->rtsp && pSys->ms )
        {
            logw( "no data received in 10s. Switching to TCP" );
            if( RtspRollOverTcp( impl ) )
            {
                loge( "TCP rollover failed, aborting" );
                return -1;
            }
            return -2;
        }
        loge( "no data received in 10s, aborting" );
        return -1;
    }
    else if( !pSys->bMulticast && !pSys->bPaused &&
             ( pSys->iNodatati > 34 ) )
    {
        logw( "no data received in 10s, eos ?" );
        return -1;
    }

    return pSys->bError ? -1 : 0;
}

static int __CdxRtspStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxRtspStreamImplT *impl;
    int ret = 0;
    int unitLen = 0;
    cdx_uint32 sendSize = len;
    cdx_uint32 remainSize = 0;

    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStop)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->status = RTSP_STREAM_READING;
    pthread_mutex_unlock(&impl->lock);

    if(impl->pSys->bOutAsf || impl->pSys->track[0]->rtpPt == MP2T)
    {
        while(impl->validDataSize < len)
        {
            if (impl->forceStop || impl->ioState == CDX_IO_STATE_ERROR)
            {
                sendSize = -1;
                goto __exit;
            }
            usleep(10*1000);
        }

        pthread_mutex_lock(&impl->bufferMutex);
        remainSize = impl->bufEndPtr - impl->bufReadPtr + 1;

        if(remainSize >= sendSize)
        {
            memcpy((char*)buf, impl->bufReadPtr, sendSize);
        }
        else
        {
            memcpy((char*)buf, impl->bufReadPtr, remainSize);
            memcpy((char*)buf+remainSize, impl->buffer, sendSize-remainSize);
        }

        impl->bufReadPtr    += sendSize;
        impl->readPos       += sendSize;
        impl->validDataSize -= sendSize;

        if(impl->bufReadPtr > impl->bufEndPtr)
        {
            impl->bufReadPtr -= impl->maxBufSize;
        }
        pthread_mutex_unlock(&impl->bufferMutex);

__exit:
        pthread_mutex_lock(&impl->lock);
        impl->status = RTSP_STREAM_IDLE;
        pthread_mutex_unlock(&impl->lock);
        pthread_cond_signal(&impl->cond);

        return sendSize;
    }
    else if(impl->pSys->bOutMuxed)
    {
        logw("impl->pSys->bOutMuxed(%d)", impl->pSys->bOutMuxed);
        ret = -1;
        goto __exit1;
    }
    else
    {
        logv("impl->bPktheaderRead=%d, impl->bPktdataRead=%d, impl->mPos.type=%d, len=%d",
            impl->bPktheaderRead, impl->bPktdataRead, impl->mPos.type, len);

        if(!impl->bPktheaderRead && !impl->bPktdataRead)
        {
            ret = GetOneFrame(impl);
            if (ret == -2)
            {
                ret = 0;
                goto __exit1;
            }

            if(ret < 0)
            {
                loge("get frame failed.");
                ret = -1;
                goto __exit1;
            }
            impl->bPktheaderRead = 1;
            impl->bPktdataRead = 1;
        }

        switch(impl->mPos.type)
        {
            case PKT_HEADER:
            {
                if(impl->bPktheaderRead == 1)
                {
                    unitLen = CedarXMin(len, sizeof(CdxRtspPktHeaderT) - impl->mPos.offset);
                    memcpy((cdx_uint8 *)buf, (cdx_uint8 *)&impl->mCurptk.pktHeader
                                + impl->mPos.offset, unitLen);
                    impl->mPos.offset += unitLen;
                    if(impl->mPos.offset == sizeof(CdxRtspPktHeaderT))
                    {
                        impl->mPos.offset = 0;
                        impl->mPos.type = PKT_DATA;
                        impl->bPktheaderRead = 0;
                    }
                }
                else
                {
                    logd("impl->bPktheaderRead==0");
                }
                ret = 0;
                break;
            }
            case PKT_DATA:
            {
                if(impl->bPktdataRead == 1)
                {
                    if(impl->mCurptk.pktData)
                    {
                        unitLen = CedarXMin(len, impl->mCurptk.pktHeader.length -
                            impl->mPos.offset);//* ring buf
                        memcpy((cdx_uint8 *)buf, (cdx_uint8 *)impl->mCurptk.pktData
                                        + impl->mPos.offset, unitLen);
                        impl->mPos.offset += unitLen;
                        if(impl->mPos.offset == impl->mCurptk.pktHeader.length)
                        {
                            free( impl->mCurptk.pktData);
                            impl->mCurptk.pktData = NULL;
                            impl->mPos.type = PKT_HEADER;
                            impl->mPos.offset = 0;
                            impl->bPktdataRead = 0;
                        }
                        else
                        {
                            logd("impl->mPos.offset(%d), impl->mCurptk.pktHeader.length(%d)",
                                impl->mPos.offset, impl->mCurptk.pktHeader.length);
                        }
                    }
                    else
                    {
                        logd("impl->mCurptk.pktData=NULL");
                        ret = -1;
                        goto __exit1;
                    }
                }
                ret = 0;
                break;
            }
            default:
            {
                loge("unsupport type..");
                break;
            }
        }
    }

__exit1:
    pthread_mutex_lock(&impl->lock);
    impl->status = RTSP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return ret;
}

static CdxStreamProbeDataT *__CdxRtspStreamGetProbeData(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    return &impl->probeData;
}

static cdx_int32 __CdxRtspStreamGetIOState(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    return impl->ioState;
}

static cdx_uint32 __CdxRtspStreamAttribute(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    cdx_uint32 flag = 0;

    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);
    if(impl->seekAble)
    {
        flag |= CDX_STREAM_FLAG_SEEK;
    }

    return flag|CDX_STREAM_FLAG_NET;
}

static cdx_int32 __CdxRtspStreamGetMetaData(CdxStreamT *stream, const cdx_char *key,
    void **pVal)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    if(strcmp(key, "extra-data") == 0)
    {
        *pVal = &impl->mediaInfoContainer;

        return 0;
    }

    return -1;
}

static cdx_int32 __CdxRtspStreamSeekToTime(CdxStreamT *stream, cdx_int64 timeUs)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    CDX_UNUSE(timeUs);

    return -1;
}

static cdx_bool __CdxRtspStreamEos(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    return CDX_FALSE;
}

static cdx_int64 __CdxRtspStreamTell(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    return impl->readPos;
}

static cdx_int64 __CdxRtspStreamSize(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    return -1;
}

static cdx_int32 __CdxRtspStreamForceStop(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    pthread_mutex_lock(&impl->lock);

    impl->pSys->eventData = 0xff;
    impl->pSys->eventRtsp = 0xff;

    impl->forceStop = 1;
    while(impl->status != RTSP_STREAM_IDLE)
    {
        pthread_cond_wait(&impl->cond, &impl->lock);
    }
    pthread_mutex_unlock(&impl->lock);

    return 0;
}

cdx_int32 __CdxRtspStreamClrForceStop(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    impl->forceStop = 0;
    impl->status = RTSP_STREAM_IDLE;

    impl->pSys->eventData = 0x00;
    impl->pSys->eventRtsp = 0x00;
    pthread_mutex_unlock(&impl->lock);

    return 0;
}

static cdx_int32 __CdxRtspStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    CDX_UNUSE(param);

    switch(cmd)
    {
        case STREAM_CMD_SET_FORCESTOP:
            return __CdxRtspStreamForceStop(stream);
        case STREAM_CMD_CLR_FORCESTOP:
            return __CdxRtspStreamClrForceStop(stream);
        default:
            break;
    }

    return 0;
}

static void GetPathFromUrl(char *url, char **path)
{
    char *p;

    p = strstr(url, "://");
    if(p != NULL)
    {
        *p = '\0';
        p += 3; // skip "://"
        *path = p;
        /* Remove HTML anchor if present (not supported).
                * The hash symbol itself should be URI-encoded. */
        p = strchr(p, '#');
        if(p != NULL)
        {
            *(p++) = '\0';
        }
    }
    else
    {
        *path = url + strlen(url);
    }

    return;
}

static void CdxTaskInterruptRTSP(void *pPrivate)
{
    CdxRtspStreamImplT *impl = (CdxRtspStreamImplT*)pPrivate;

    /* Avoid lock */
    impl->pSys->eventRtsp = 0xff;
}

static void DefaultLive555Callback( RTSPClient* client, int resultCode, char* resultString)
{
    CdxRTSPClient *cdxclient = static_cast<CdxRTSPClient *> ( client );
    StreamSysT *pSys = cdxclient->pSys;
    delete []resultString;
    pSys->iLive555ret = resultCode;
    pSys->bError = pSys->iLive555ret != 0;
    pSys->eventRtsp = 1;
}

static int WaitLive555Response(CdxRtspStreamImplT *impl, int iTimeout = 0) //ms
{
    TaskToken task;
    StreamSysT *pSys = impl->pSys;
    pSys->eventRtsp = 0;
    if(iTimeout > 0)
    {
        //* Create a task that will be called if we wait more than timeout ms
        task = pSys->scheduler->scheduleDelayedTask(iTimeout*1000,
                                                      CdxTaskInterruptRTSP,
                                                      impl);
    }

    pSys->eventRtsp = 0;
    pSys->bError = 1;
    pSys->iLive555ret = 0;
    pSys->scheduler->doEventLoop(&pSys->eventRtsp);
    //* here, if bError is true and iLive555ret = 0 we didn't receive a response
    if(iTimeout > 0)
    {
        //* remove the task
        pSys->scheduler->unscheduleDelayedTask(task);
    }
    return !pSys->bError;
}

static void CdxContinueAfterDESCRIBE(RTSPClient* client, int resultCode,
                                   char* resultString)
{
    CdxRTSPClient *clientCdx = static_cast<CdxRTSPClient *> (client);
    StreamSysT *pSys = clientCdx->pSys;
    pSys->iLive555ret = resultCode;
    if ( resultCode == 0 )
    {
        char* sdpDescription = resultString;
        free(pSys->pSdp);
        pSys->pSdp = NULL;
        if(sdpDescription)
        {
            pSys->pSdp = strdup(sdpDescription);
            pSys->bError = 0;
            //logd("----- pSys->pSdp: (\n %s)", pSys->pSdp);
        }
    }
    else
        pSys->bError = 1;
    delete [] resultString;
    pSys->eventRtsp = 1;
}

static void CdxContinueAfterOPTIONS(RTSPClient* client, int resultCode,
                                  char* resultString)
{
    CdxRTSPClient *clientCdx = static_cast<CdxRTSPClient*> (client);
    StreamSysT *pSys = clientCdx->pSys;
    pSys->bGetparam =
      // If OPTIONS fails, assume GET_PARAMETER is not supported but
      // still continue on with the stream.  Some servers (foscam)
      // return 501/not implemented for OPTIONS.
      resultCode == 0
      && resultString != NULL
      && strstr(resultString, "GET_PARAMETER") != NULL;
    client->sendDescribeCommand(CdxContinueAfterDESCRIBE);
    delete [] resultString;
}

//* connect to RTSP server to setup the session DESCRIBE
static int RtspConnect(CdxRtspStreamImplT *impl)
{
    StreamSysT *pSys = impl->pSys;
    Authenticator authenticator;
    char *pUser    = NULL;
    char *pPwd     = NULL;
    char *pUrl     = NULL;
    int  iHttpPort = 0;
    int  iRet      = 0;
    const int iTimeout = 5*1000;// ms

    //* get the user name and passwd.
    if(pSys->url.pUsername || pSys->url.pPassword)
    {
        if(pSys->url.iPort == 0)
        {
            pSys->url.iPort = 554;
        }
        if(asprintf(&pUrl, "rtsp://%s:%d%s",
            pSys->url.pHost ? pSys->url.pHost : "",
            pSys->url.iPort,
            pSys->url.pPath ? pSys->url.pPath : "") == -1)
        {
            loge("asprintf failed.");
            return  -1;
        }
        pUser = strdup(pSys->url.pUsername ? pSys->url.pUsername : "");
        pPwd  = strdup(pSys->url.pPassword ? pSys->url.pPassword : "");
    }
    else
    {
        if(asprintf(&pUrl, "rtsp://%s", pSys->pPath) == -1)
        {
            loge("asprintf failed.");
            return -1;
        }

        pUser = strdup("");
        pPwd  = strdup("");
    }

create:

    //* todo. if it is HTTP tunneling mode ?
    if(impl->bRtsphttp)
    {
        iHttpPort = 80;
    }
    pSys->rtsp = new CdxRTSPClient(*pSys->env, pUrl,
                                    LOG_VERBOSE > 0 ? 1 : 0,
                                    "LibCdx", iHttpPort, pSys);

    if(!pSys->rtsp)
    {
        loge("RTSPClient::createNew failed (%s)", pSys->env->getResultMsg());
        iRet = -1;
        goto err_out;
    }

    //* todo. User-Agent, setUserAgentString "rtsp-kasenna"

describe:
    authenticator.setUsernameAndPassword(pUser, pPwd);
    //* 在发送完sendOptionsCommand请求，收到它的应答后调用回调函数continueAfterOPTIONS，
    //在continueAfterOPTIONS函数中发送Describe请求；
    pSys->rtsp->sendOptionsCommand(&CdxContinueAfterOPTIONS, &authenticator);

    if(!WaitLive555Response(impl, iTimeout))
    {
        int iCode = pSys->iLive555ret;
        if(iCode == 401)
        {
            loge("authentication failed");
            if(pUser)
                free(pUser);
            if(pPwd)
                free(pPwd);
            return -1;
        }
        else if(iCode > 0 && iCode != 404 && !impl->bRtsphttp)
        {
            logd("try HTTP tunneling mode. iCode(%d)", iCode);
            impl->bRtsphttp = 1;
            if(pSys->rtsp)
            {
                RTSPClient::close(pSys->rtsp);
                pSys->rtsp = NULL;
            }
            goto create;
        }
        else
        {
            if(iCode == 0)
            {
                logd("connect timeout.");
            }
            else
            {
                loge("connect error: %d", iCode);
                if(iCode == 403)
                {
                    loge("RTSP connection failed, denied by the server configuration");
                }
            }
            if(pSys->rtsp)
            {
                RTSPClient::close(pSys->rtsp);
                pSys->rtsp = NULL;
            }
            iRet = -1;
        }

    }
err_out:
    free(pUrl);
    free(pUser);
    free(pPwd);

    return iRet;
}

static unsigned char* RtspParseH264ConfigStr( char const* configStr,
                                          unsigned int& configSize)
{
    char *dup, *psz;
    size_t i_records = 1;

    configSize = 0;

    if( configStr == NULL || *configStr == '\0' )
    {
        return NULL;
    }

    psz = dup = strdup( configStr );

    /* Count the number of commas */
    for( psz = dup; *psz != '\0'; ++psz )
    {
        if( *psz == ',')
        {
            ++i_records;
            *psz = '\0';
        }
    }

    size_t configMax = 5*strlen(dup);
    unsigned char *cfg = new unsigned char[configMax];
    psz = dup;
    for( size_t i = 0; i < i_records; ++i )
    {
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x01;

        configSize += Base64Decode(cfg+configSize, configMax-configSize, psz );
        psz += strlen(psz)+1;
    }
    /*
    for (size_t i = 0; i < configSize; i++)
        logw("prop-parameter-sets[i] = 0x%x", cfg[i]);
    */

    free(dup);
    return cfg;
}

static cdx_uint8 *RtspParseVorbisConfigStr( char const* configStr,
                                      unsigned int& configSize )
{
    configSize = 0;
    if( configStr == NULL || *configStr == '\0' )
        return NULL;
#if LIVEMEDIA_LIBRARY_VERSION_INT >= 1332115200 // 2012.03.20
    unsigned char *p_cfg = base64Decode( configStr, configSize );
#else
    char* configStr_dup = strdup( configStr );
    unsigned char *p_cfg = base64Decode( configStr_dup, configSize );
    free( configStr_dup );
#endif
    uint8_t *p_extra = NULL;
    /* skip header count, ident number and length (cf. RFC 5215) */
    const unsigned int headerSkip = 9;
    if( configSize > headerSkip && ((cdx_uint8 *)p_cfg)[3] == 1 )
    {
        configSize -= headerSkip;
        p_extra = (cdx_uint8 *)malloc( configSize );
        assert(!p_extra);
        memcpy( p_extra, p_cfg+headerSkip, configSize );
    }
    delete [] p_cfg;
    return p_extra;
}

static int ExtractMediaInfo(MediaSubsession *sub, CdxRtspStreamImplT *impl)
{
    CdxMediaInfoT *pMediaInfo = &impl->mediaInfo;
    int            iFramebuffer = FRAME_BUFFER_SIZE;
    LiveTrackT     *tk;

    tk = (LiveTrackT *)calloc(1, sizeof( LiveTrackT ) );
    if( !tk )
    {
        loge("malloc failed.");
        return -1;
    }
    tk->impl        = impl;
    tk->sub         = sub;
    tk->rtpPt = INVALID;
    //tk->pEs        = NULL;
    tk->bQuicktime = 0;
    tk->bAsf       = 0;
    //tk->pAsfBlock = NULL;
    tk->bMuxed     = 0;
    tk->bDiscardTrunc = 0;
    //tk->pOutmuxed = NULL;
    tk->waiting     = 0;
    tk->bRtcpsync = 0;
    tk->iPts       = 0;
    tk->fNpt       = 0.;
    tk->bSelected  = 1;
    tk->iBuffer    = iFramebuffer;
    tk->pBuffer    = (unsigned char *)malloc(iFramebuffer);

    if( !tk->pBuffer )
    {
        loge("malloc failed.");
        free( tk );
        return -1;
    }

    if( !strcmp( sub->mediumName(), "audio" ) )
    {
        tk->fmt.iCat = CDX_MEDIA_AUDIO;
        pMediaInfo->program[0].audio[impl->nAudiotrack].nChannelNum = sub->numChannels();
        pMediaInfo->program[0].audio[impl->nAudiotrack].nSampleRate = sub->rtpTimestampFrequency();
        pMediaInfo->program[0].audioNum++;

        if( !strcmp( sub->codecName(), "MPA" ) ||
            !strcmp( sub->codecName(), "MPA-ROBUST" ) ||
            !strcmp( sub->codecName(), "X-MP3-DRAFT-00" ) )
        {
            pMediaInfo->program[0].audio[impl->nAudiotrack].eCodecFormat =
                AUDIO_CODEC_FORMAT_MPEG_AAC_LC; //* to check
            pMediaInfo->program[0].audio[impl->nAudiotrack].nSampleRate = 0;

            tk->fmt.iCodec = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
        }
        else if( !strcmp( sub->codecName(), "AC3" ) )
        {
            pMediaInfo->program[0].audio[impl->nAudiotrack].eCodecFormat = AUDIO_CODEC_FORMAT_AC3;
            pMediaInfo->program[0].audio[impl->nAudiotrack].nSampleRate = 0;

            tk->fmt.iCodec = AUDIO_CODEC_FORMAT_AC3;
        }
        else if( !strcmp( sub->codecName(), "L16" ) )
        {
            //pMediaInfo->program[0].audio[0].eCodecFormat = //* todo
            pMediaInfo->program[0].audio[impl->nAudiotrack].nBitsPerSample = 16;
            logd("which codec type L16?");
        }
        else if( !strcmp( sub->codecName(), "L20" ) )
        {
            //pMediaInfo->program[0].audio[0].eCodecFormat = //* todo
            pMediaInfo->program[0].audio[impl->nAudiotrack].nBitsPerSample = 20;
            logd("which codec type L20?");
        }
        else if( !strcmp( sub->codecName(), "L24" ) )
        {
            //pMediaInfo->program[0].audio[0].eCodecFormat = //* todo
            pMediaInfo->program[0].audio[impl->nAudiotrack].nBitsPerSample = 24;
            logd("which codec type L24?");
        }
        else if( !strcmp( sub->codecName(), "L8" ) )
        {
            //pMediaInfo->program[0].audio[0].eCodecFormat = //* todo
            pMediaInfo->program[0].audio[impl->nAudiotrack].nBitsPerSample = 8;
            logd("which codec type L8?");
        }
        else if( !strcmp( sub->codecName(), "DAT12" ) )
        {
            //pMediaInfo->program[0].audio[0].eCodecFormat = //* todo
            pMediaInfo->program[0].audio[impl->nAudiotrack].nBitsPerSample = 12;
            logd("which codec type L12?");
        }
        else if( !strcmp( sub->codecName(), "PCMU" ) )
        {
            //pMediaInfo->program[0].audio[0].eCodecFormat = //* todo
            pMediaInfo->program[0].audio[impl->nAudiotrack].nBitsPerSample = 8;
            logd("which codec type PCMU?");
        }
        else if( !strcmp( sub->codecName(), "PCMA" ) )
        {
            //pMediaInfo->program[0].audio[0].eCodecFormat = //* todo
            pMediaInfo->program[0].audio[impl->nAudiotrack].nBitsPerSample = 8;
            logd("which codec type PCMA?");
        }
        else if( !strncmp( sub->codecName(), "G726", 4 ) )
        {
            //pMediaInfo->program[0].audio[0].eCodecFormat = //* todo
            pMediaInfo->program[0].audio[impl->nAudiotrack].nSampleRate = 8000;
            pMediaInfo->program[0].audio[impl->nAudiotrack].nChannelNum = 1;
            logd("which codec type G726?");
        }
        else if( !strcmp( sub->codecName(), "AMR" ) )
        {
            logd("xxxxxx AMR");
            pMediaInfo->program[0].audio[impl->nAudiotrack].eCodecFormat = AUDIO_CODEC_FORMAT_AMR;
            pMediaInfo->program[0].audio[impl->nAudiotrack].eSubCodecFormat = AMR_FORMAT_NARROWBAND;
            tk->fmt.iCodec = AUDIO_CODEC_FORMAT_AMR;
        }
        else if( !strcmp( sub->codecName(), "AMR-WB" ) )
        {
            pMediaInfo->program[0].audio[impl->nAudiotrack].eCodecFormat = AUDIO_CODEC_FORMAT_AMR;
            pMediaInfo->program[0].audio[impl->nAudiotrack].eSubCodecFormat = AMR_FORMAT_WIDEBAND;
            tk->fmt.iCodec = AUDIO_CODEC_FORMAT_AMR;
        }
        else if( !strcmp( sub->codecName(), "MP4A-LATM" ) )
        {
            unsigned int i_extra;
            unsigned char *p_extra;

            //pMediaInfo->program[0].audio[0].eCodecFormat =//*todo  CODEC_MP4A;
            //tk->fmt.iCodec = ;
            logd("xxxx MP4A-LATM");
            if( ( p_extra = parseStreamMuxConfigStr( sub->fmtp_config(),
                                                     i_extra ) ) )
            {
                pMediaInfo->program[0].audio[impl->nAudiotrack].nCodecSpecificDataLen = i_extra;
                pMediaInfo->program[0].audio[impl->nAudiotrack].pCodecSpecificData =
                    (char*)malloc(i_extra);
                memcpy(pMediaInfo->program[0].audio[impl->nAudiotrack].pCodecSpecificData,
                    p_extra, i_extra);
                delete [] p_extra;
            }
        }
        else if( !strcmp( sub->codecName(), "MPEG4-GENERIC" ) )
        {
            unsigned int i_extra;
            unsigned char *p_extra;

            pMediaInfo->program[0].audio[impl->nAudiotrack].eCodecFormat =
                AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
            tk->fmt.iCodec = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;

            if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                   i_extra ) ) )
            {
                pMediaInfo->program[0].audio[impl->nAudiotrack].nCodecSpecificDataLen = i_extra;
                pMediaInfo->program[0].audio[impl->nAudiotrack].pCodecSpecificData =
                    (char *)malloc( i_extra );
                memcpy( pMediaInfo->program[0].audio[impl->nAudiotrack].pCodecSpecificData,
                    p_extra, i_extra );
                delete [] p_extra;
            }
        }
        else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
        {
            tk->bAsf = 1;
            if(impl->pSys->bOutAsf == 0)
                impl->pSys->bOutAsf = 1;
            logd("X-ASF-PF");
        }
        else if( !strcmp( sub->codecName(), "X-QT" ) ||
                 !strcmp( sub->codecName(), "X-QUICKTIME" ) )
        {
            tk->bQuicktime = 1;
        }
        else if( !strcmp( sub->codecName(), "VORBIS" ) )
        {
            pMediaInfo->program[0].audio[impl->nAudiotrack].eCodecFormat = AUDIO_CODEC_FORMAT_OGG;

            tk->fmt.iCodec = AUDIO_CODEC_FORMAT_OGG;
            unsigned int i_extra;
            unsigned char *p_extra;
            if( ( p_extra=RtspParseVorbisConfigStr( sub->fmtp_config(),
                                                i_extra ) ) )
            {
                pMediaInfo->program[0].audio[impl->nAudiotrack].nCodecSpecificDataLen = i_extra;
                pMediaInfo->program[0].audio[impl->nAudiotrack].pCodecSpecificData =
                    (char *)p_extra;
            }
            else
            {
                logw( "Missing or unsupported vorbis header." );
            }
        }
        impl->nAudiotrack++;
    }
    else if( !strcmp( sub->mediumName(), "video" ) )
    {
        tk->fmt.iCat = CDX_MEDIA_VIDEO;
        pMediaInfo->program[0].videoNum++;
        if( !strcmp( sub->codecName(), "MPV" ) )
        {
            //pMediaInfo->program[0].video[0].eCodecFormat = //* todo
            //fmt.b_packetized = false;// ??
            logd("MPV... CHECK");
        }
        else if( !strcmp( sub->codecName(), "H263" ) ||
                 !strcmp( sub->codecName(), "H263-1998" ) ||
                 !strcmp( sub->codecName(), "H263-2000" ) )
        {
            pMediaInfo->program[0].video[impl->nVideotrack].eCodecFormat = VIDEO_CODEC_FORMAT_H263;
            tk->fmt.iCodec = VIDEO_CODEC_FORMAT_H263;
        }
        else if( !strcmp( sub->codecName(), "H261" ) )
        {
            //pMediaInfo->program[0].video[0].eCodecFormat = VIDEO_CODEC_FORMAT_H263;
            logd("H261 not support...");
        }
        else if( !strcmp( sub->codecName(), "H264" ) )
        {
            unsigned int i_extra = 0;
            unsigned char *p_extra = NULL;

            tk->fmt.iCodec = VIDEO_CODEC_FORMAT_H264;
            pMediaInfo->program[0].video[impl->nVideotrack].eCodecFormat = VIDEO_CODEC_FORMAT_H264;
            //fmt.b_packetized = false;
#if 1
            if((p_extra = RtspParseH264ConfigStr( sub->fmtp_spropparametersets(),
                                            i_extra ) ) )
            {
                pMediaInfo->program[0].video[0].nCodecSpecificDataLen = i_extra;
                pMediaInfo->program[0].video[0].pCodecSpecificData = (char *)malloc( i_extra );
                memcpy( pMediaInfo->program[0].video[0].pCodecSpecificData, p_extra, i_extra );

                delete [] p_extra;
            }
#endif
        }
        else if( !strcmp( sub->codecName(), "JPEG" ) )
        {
             pMediaInfo->program[0].video[impl->nVideotrack].eCodecFormat =
                VIDEO_CODEC_FORMAT_MJPEG;
             tk->fmt.iCodec = VIDEO_CODEC_FORMAT_MJPEG;
        }
        else if( !strcmp( sub->codecName(), "MP4V-ES" ) )
        {
            unsigned int i_extra;
            uint8_t      *p_extra;

            logd("MP4V-ES");
            pMediaInfo->program[0].video[impl->nVideotrack].eCodecFormat = VIDEO_CODEC_FORMAT_XVID;
            tk->fmt.iCodec = VIDEO_CODEC_FORMAT_XVID;

            if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                   i_extra ) ) )
            {
                pMediaInfo->program[0].video[impl->nVideotrack].nCodecSpecificDataLen = i_extra;
                pMediaInfo->program[0].video[impl->nVideotrack].pCodecSpecificData =
                    (char *)malloc( i_extra );
                memcpy( pMediaInfo->program[0].video[impl->nVideotrack].pCodecSpecificData,
                    p_extra, i_extra );
                delete [] p_extra;
            }
        }
        else if( !strcmp( sub->codecName(), "X-QT" ) ||
                 !strcmp( sub->codecName(), "X-QUICKTIME" ) ||
                 !strcmp( sub->codecName(), "X-QDM" ) ||
                 !strcmp( sub->codecName(), "X-SV3V-ES" )  ||
                 !strcmp( sub->codecName(), "X-SORENSONVIDEO" ) )
        {
           tk->bQuicktime = 1;
           logd("========== Quicktime");
        }
        else if( !strcmp( sub->codecName(), "MP2T" ) )
        {
            tk->bMuxed = 1;
            tk->rtpPt = MP2T;
            if(impl->pSys->bOutMuxed == 0)
                impl->pSys->bOutMuxed = 1;
        }
        else if( !strcmp( sub->codecName(), "MP2P" ) ||
                 !strcmp( sub->codecName(), "MP1S" ) )
        {
            tk->bMuxed = 1;
            if(impl->pSys->bOutMuxed == 0)
                impl->pSys->bOutMuxed = 1;
            logw("MP2P MP1S not support");
        }
        else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
        {
            tk->bAsf = 1;
            if(impl->pSys->bOutAsf == 0)
                impl->pSys->bOutAsf = 1;
            logd("X-ASF-PF");
        }
        else if( !strcmp( sub->codecName(), "DV" ) )
        {
            logw("todo dv");
        }
        else if( !strcmp( sub->codecName(), "VP8" ) )
        {
            pMediaInfo->program[0].video[impl->nVideotrack].eCodecFormat = VIDEO_CODEC_FORMAT_VP8;
            tk->fmt.iCodec = VIDEO_CODEC_FORMAT_VP8;
        }
        impl->nVideotrack++;
    }
    else if( !strcmp( sub->mediumName(), "text" ) )
    {
        logd("xxx text");
        tk->fmt.iCat = CDX_MEDIA_SUBTITLE;
        if( !strcmp( sub->codecName(), "T140" ) )
        {
            logd("T140");
        }
    }

    if( sub->rtcpInstance() != NULL )
    {
        sub->rtcpInstance()->setByeHandler( RtspStreamClose, tk );
    }

    TABLE_APPEND_CAST( (LiveTrackT **), impl->pSys->iTrack, impl->pSys->track, tk );

    return 0;
}
static int RtspSessionsSetup(CdxRtspStreamImplT *impl)
{
    StreamSysT             *pSys  = impl->pSys;
    MediaSubsessionIterator *iter   = NULL;
    MediaSubsession         *sub    = NULL;

    int            bRtsptcp;
    int            iClientport;
    int            iReturn = 0;
    unsigned int   iReceivebuffer = 0;
    unsigned const thresh = 200000; // RTP reorder threshold 0.2 second (default 0.1)

    bRtsptcp    = impl->bRtsptcp || impl->bRtsphttp;
    iClientport = -1; //* todo    //var_InheritInteger( impl, "rtp-client-port" );

    //* Create the session from the SDP
    if(!(pSys->ms = MediaSession::createNew(*pSys->env, pSys->pSdp)))
    {
        loge("Could not create the RTSP Session: %s",
            pSys->env->getResultMsg());
        return -1;
    }

    //* Initialise each media subsession
    iter = new MediaSubsessionIterator(*pSys->ms);
    while((sub = iter->next()) != NULL)
    {
        int bInit;
        logv("sub(%p)", sub);

        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
        {
            iReceivebuffer = 100000;
            logv("xxx audio.");
        }
        else if( !strcmp( sub->mediumName(), "video" ) )
        {
            iReceivebuffer = 2000000;
            logv("xxx video.");
        }
        else if(!strcmp(sub->mediumName(), "text"))
        {
            logv("xxx text.");
        }
        else
        {
            continue;
            logv("xxx %s.", sub->mediumName());
        }
        if( iClientport != -1 ) //* todo
        {
            sub->setClientPortNum( iClientport );
            iClientport += 2;
        }

        if( strcasestr( sub->codecName(), "REAL" ) )
        {
            logd( "real codec detected, using real-RTSP instead" );
            pSys->bReal = 1;
            continue;
        }

        if( !strcmp( sub->codecName(), "X-ASF-PF" ) ) //* todo
            bInit = sub->initiate( 0 );
        else
            bInit = sub->initiate();

        if( !bInit )
        {
            logw("RTP subsession '%s/%s' failed (%s)",
                      sub->mediumName(), sub->codecName(),
                      pSys->env->getResultMsg());
        }
        else
        {
            if(sub->rtpSource() != NULL)
            {
                int fd = sub->rtpSource()->RTPgs()->socketNum();

                /* Increase the buffer size */
                if( iReceivebuffer > 0 )
                    increaseReceiveBufferTo( *pSys->env, fd, iReceivebuffer );

                /* Increase the RTP reorder timebuffer just a bit */
                sub->rtpSource()->setPacketReorderingThresholdTime(thresh);
            }
            logd("RTP subsession '%s/%s'", sub->mediumName(), sub->codecName());

            /* Issue the SETUP */
            if( pSys->rtsp )
            {
                pSys->rtsp->sendSetupCommand( *sub, DefaultLive555Callback, 0,
                                               bRtsptcp ? 1 : 0,
                                               (pSys->bForceMcast && !bRtsptcp) ? 1 : 0);
                if(!WaitLive555Response(impl))
                {
                    //* if we get an unsupported transport error, toggle TCP use and try again
                    if( pSys->iLive555ret == 461)
                        pSys->rtsp->sendSetupCommand(*sub, DefaultLive555Callback, 0,
                                                       !(bRtsptcp ? 1 : 0), 0);
                    if( pSys->iLive555ret != 461 || !WaitLive555Response(impl))
                    {
                        loge("SETUP of'%s/%s' failed %s",
                                 sub->mediumName(), sub->codecName(),
                                 pSys->env->getResultMsg() );
                        continue;
                    }
                    else
                    {
                        impl->bRtsptcp = 1;
                        bRtsptcp = 1;
                    }
                }
            }

            /* Check if we will receive data from this subsession for this track */
            if( sub->readSource() == NULL ) continue;
            if( !pSys->bMulticast )
            {
                //* We need different rollover behaviour for multicast
                pSys->bMulticast = IsMulticastAddress(sub->connectionEndpointAddress());
            }

            /* Value taken from mplayer */
            int ret;
            ret = ExtractMediaInfo(sub, impl);
            if(ret < 0)
            {
                delete iter;
                return -1;
            }

        }
    }
    delete iter;

    logd("pSys->iTrack=%d", pSys->iTrack);

    if( pSys->iTrack <= 0 )
    {
        loge("no track...");
        iReturn = -1;
    }

    /* Retrieve the starttime if possible */
    impl->pSys->fNptStart = pSys->ms->playStartTime();

    /* Retrieve the duration if possible */
    impl->pSys->fNptLength = pSys->ms->playEndTime();

    /* duration */
    logd("setup start: %f stop:%f", pSys->fNptStart, pSys->fNptLength);

    /* */
    pSys->bNoData = 1;
    pSys->iNodatati = 0; //*

    return iReturn;
}

static void *CdxTimeoutProcess(void *pArg)
{
    CdxRtspStreamImplT *impl = (CdxRtspStreamImplT *)pArg;
    TimeoutThreadT *pTimeout = impl->pSys->pTimeout;

    while(1)
    {
        char *pszBye = NULL;
        logv("xxx iTimeout(%d s)", pTimeout->pSys->iTimeout);
        struct timespec time;
        clock_gettime(CLOCK_REALTIME, &time);
        time.tv_sec += pTimeout->pSys->iTimeout * 2 / 3;
        if (sem_timedwait(&pTimeout->sem, &time))   // timeout
            pTimeout->pSys->rtsp->sendGetParameterCommand(*pTimeout->pSys->ms, NULL, pszBye);
        else // force exit
            break;
    }

    return NULL;
}

static int RtspPlay(CdxRtspStreamImplT *impl)
{
    StreamSysT *pSys = impl->pSys;

    if(pSys->rtsp)
    {
        //* PLAY request
        if (impl->startTime != NULL)
            pSys->rtsp->sendPlayCommand(*pSys->ms,
                DefaultLive555Callback, impl->startTime, impl->endTime, 1);
        else
            pSys->rtsp->sendPlayCommand(*pSys->ms,
                DefaultLive555Callback, pSys->fNptStart, -1.0, 1);

        if( !WaitLive555Response(impl) )
        {
            loge( "RTSP PLAY failed %s", pSys->env->getResultMsg() );
            return -1;
        }

        /* Retrieve the timeout value and set up a timeout prevention thread */
        pSys->iTimeout = pSys->rtsp->sessionTimeoutParameter();
        if( pSys->iTimeout <= 0 )
            pSys->iTimeout = 60; /* default value from RFC2326 */

        /* start timeout-thread only if GET_PARAMETER is supported by the server */
        /* or start it if wmserver dialect, since they don't report that GET_PARAMETER
            is supported correctly */
        if( !pSys->pTimeout && ( pSys->bGetparam /*"rtsp-wmserver"*/ ) )
        {
            logd( "We have a timeout of %d seconds",  pSys->iTimeout );
            pSys->pTimeout = (TimeoutThreadT *)malloc( sizeof(TimeoutThreadT) );
            if( pSys->pTimeout )
            {
                memset( pSys->pTimeout, 0, sizeof(TimeoutThreadT) );
                pSys->pTimeout->pSys = impl->pSys; /* lol, object recursion :D */

                sem_init(&pSys->pTimeout->sem, 0, 0);
                if(pthread_create(&pSys->pTimeout->handle, NULL, CdxTimeoutProcess, impl) != 0)
                {
                    loge("create timeout thread failed.");
                    sem_destroy(&pSys->pTimeout->sem);
                    free(pSys->pTimeout);
                    pSys->pTimeout = NULL;
                }

            }
            else
            {
                loge( "cannot spawn liveMedia timeout thread" );
            }
        }
    }
    pSys->iPcr = 0;

    /* Retrieve the starttime if possible */
    pSys->fNptStart = pSys->ms->playStartTime();
    if (pSys->ms->playEndTime() > 0)
        pSys->fNptLength = pSys->ms->playEndTime();

    logd( "play start: %f stop:%f", pSys->fNptStart, pSys->fNptLength );
    return 0;
}

static int __CdxRtspStreamClose(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    StreamSysT *pSys = impl->pSys;

    pthread_mutex_lock(&impl->lock);
    impl->exitFlag = 1;
    pthread_cond_signal(&impl->cond);
    pthread_mutex_unlock(&impl->lock);

    impl->pSys->eventData = 0xff;
    impl->pSys->eventRtsp = 0xff;

    if (pthread_join(impl->threadId, NULL) == 0)
    {
        pthread_mutex_destroy(&impl->bufferMutex);
        pthread_mutex_destroy(&impl->lock);
        pthread_cond_destroy(&impl->cond);
    }

    if(pSys)
    {
        if(pSys->pTimeout) //* in rtspPlay
        {
            sem_post(&pSys->pTimeout->sem);
            pthread_join(pSys->pTimeout->handle, NULL);
            sem_destroy(&pSys->pTimeout->sem);
            free(pSys->pTimeout);
        }

        if(pSys->rtsp && pSys->ms)
        {
            pSys->rtsp->sendTeardownCommand( *pSys->ms, NULL );
        }
        if( pSys->ms )
        {
            Medium::close( pSys->ms );
        }
        if( pSys->rtsp )
        {
            RTSPClient::close( pSys->rtsp );
        }

        if( pSys->env )
        {
            pSys->env->reclaim();
        }
        for( int i = 0; i < pSys->iTrack; i++ )
        {
            LiveTrackT *tk = pSys->track[i];

            free( tk->pBuffer );
            free( tk );
        }
        TABLE_CLEAN(pSys->iTrack, pSys->track);

        delete pSys->scheduler;

        free(pSys->pSdp);
        free(pSys->url.pHost);
        free(pSys->url.pBuffer);
        free(pSys);
        impl->pSys = NULL;
    }

    free(impl->buffer);
    free(impl->bufAsfheader);

    free(impl->url);
    impl->url = NULL;

    free(impl->startTime);
    free(impl->endTime);

    free(impl->mediaInfo.program[0].audio[0].pCodecSpecificData);
    impl->mediaInfo.program[0].audio[0].pCodecSpecificData = NULL;
    free(impl->mediaInfo.program[0].video[0].pCodecSpecificData);
    impl->mediaInfo.program[0].video[0].pCodecSpecificData = NULL;
    free(impl->probeData.buf);
    impl->probeData.buf = NULL;

#if __RTSP_SAVE_BITSTREAMS
    fclose(impl->fp_rtsp);
#endif

    free(impl);

    return 0;
}

// Base64 decoding
static int Base64Decode( cdx_uint8 *pDst, int iDst, const char *pSrc )
{
    //* 64 valid value: 'A-Z', 'a-z', '0-9', '+', '/'
    static const int base64[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
    };
    cdx_uint8 *pStart = pDst;
    cdx_uint8 *p = (cdx_uint8 *)pSrc;

    int iLevel;
    int iLast;

    for( iLevel = 0, iLast = 0; (int)( pDst - pStart ) < iDst && *p != '\0'; p++ )
    {
        const int c = base64[(unsigned int)*p];
        if( c == -1 )
            break;

        switch( iLevel )
        {
            case 0:
                iLevel++;
                break;
            case 1:
                *pDst++ = ( iLast << 2 ) | ( ( c >> 4)&0x03 );
                iLevel++;
                break;
            case 2:
                *pDst++ = ( ( iLast << 4 )&0xf0 ) | ( ( c >> 2 )&0x0f );
                iLevel++;
                break;
            case 3:
                *pDst++ = ( ( iLast &0x03 ) << 6 ) | c;
                iLevel = 0;
        }
        iLast = c;
    }

    return (pDst - pStart);
}

void BufferGetInit(BufferT *pBuf, void *pData, int iData)
{
    pBuf->nSize = iData;
    pBuf->nData = 0;
    pBuf->pData = (cdx_uint8 *)pData;
}

cdx_uint8 BufferGet8 (BufferT *pBuf)
{
    cdx_uint8  i;
    if( pBuf->nData >= pBuf->nSize )
    {
        return 0;
    }
    i = pBuf->pData[pBuf->nData];
    pBuf->nData++;

    return i;
}

cdx_uint16 BufferGet16( BufferT *pBuf )
{
    cdx_uint16 i1, i2;

    i1 = BufferGet8( pBuf );
    i2 = BufferGet8( pBuf );

    return( i1 + ( i2 << 8 ) );

}

cdx_uint32 BufferGet32( BufferT *pBuf )
{
    cdx_uint32 iw1, iw2;

    iw1 = BufferGet16( pBuf );
    iw2 = BufferGet16( pBuf );

    return( iw1 + ( iw2 << 16 ) );
}

cdx_uint64 BufferGet64( BufferT *pBuf )
{
    cdx_uint64 idw1, idw2;

    idw1 = BufferGet32( pBuf );
    idw2 = BufferGet32( pBuf );

    return( idw1 + ( idw2 << 32 ) );
}

void BufferGetasfGuid(BufferT *pBuf, asfGuid *pGuid)
{
    int i;

    for( i = 0; i < 16; i++ )
    {
        pGuid->data[i] = BufferGet8(pBuf);
    }
}

int BufferGetMemory (BufferT *pBuf, void *pMem, cdx_int64 iMem )
{
    int iCopy;

    iCopy = CedarXMin(iMem, pBuf->nSize - pBuf->nData);
    if(iCopy > 0 && pMem != NULL)
    {
        memcpy(pMem, pBuf->pData + pBuf->nData , iCopy);
    }
    if( iCopy < 0 )
    {
        iCopy = 0;
    }
    pBuf->nData += iCopy;
    return iCopy;
}

int BufferReadEmpty(BufferT *pBuf)
{
    return ((pBuf->nData >= pBuf->nSize) ? 1 : 0);
}

static const cdx_uint8 asf_header[16] =
{
    0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C
};

static const cdx_uint8 file_header[16] =
{
    0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

static const cdx_uint8 head1_guid[16] =
{
    0xb5, 0x03, 0xbf, 0x5f, 0x2E, 0xA9, 0xCF, 0x11, 0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65
};

static const cdx_uint8 ext_stream_header[16] =
{
    0xCB, 0xA5, 0xE6, 0x14, 0x72, 0xC6, 0x32, 0x43, 0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A
};

static const cdx_uint8 stream_header[16] =
{
    0x91, 0x07, 0xDC, 0xB7, 0xB7, 0xA9, 0xCF, 0x11, 0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

static const cdx_uint8 stream_bitrate_guid[16] =
{
    0xce, 0x75, 0xf8, 0x7b, 0x8d, 0x46, 0xd1, 0x11, 0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2
};

static void GetPktSize (AsfHdrT *hdr,cdx_uint8 *pHeader, int iHeader )
{
    BufferT buf;
    asfGuid guid;
    cdx_int64 iSize;

    hdr->fileSize = 0;
    hdr->dataPktCount = 0;
    hdr->minDataPktSize = 0;

    BufferGetInit(&buf, pHeader, iHeader);
    BufferGetasfGuid(&buf, &guid); //* guid  16B

    if(memcmp(guid.data, asf_header, 16))
    {
        loge("not asf format file.");
        return ;
    }
    BufferGetMemory(&buf, NULL, 30 - 16);
    //*Object Size(8B), Number of Header Objects(4B),Reserved1(1B), Reserved2(1)

    for( ;; )
    {
        BufferGetasfGuid(&buf, &guid);
        iSize = BufferGet64(&buf); //* Size

        if(!memcmp(guid.data, file_header, 16)) //* file properties object
        {
            BufferGetMemory(&buf, NULL, 16); //* file id
            hdr->fileSize = BufferGet64(&buf); //* file size
            BufferGetMemory(&buf, NULL, 8); //* creation date
            hdr->dataPktCount = BufferGet64(&buf); //* data pkt count
            BufferGetMemory(&buf, NULL, 8+8+8+4); //*
            hdr->minDataPktSize = BufferGet32(&buf);
            logd("============= minDataPktSize=%u", hdr->minDataPktSize);
            BufferGetMemory(&buf, NULL, iSize - 24 - 16 - 8 - 8 - 8 - 8-8-8-4 - 4);
        }
        else if(!memcmp(guid.data, head1_guid, 16))
        {
            BufferGetMemory(&buf, NULL, 46 - 24);
        }
        else if(!memcmp(guid.data, ext_stream_header, 16))
        {
            cdx_int16 i_count1, i_count2;
            int i_subsize;

            BufferGetMemory(&buf, NULL, 84 - 24);

            i_count1 = BufferGet16(&buf);
            i_count2 = BufferGet16(&buf);

            i_subsize = 88;
            for( int i = 0; i < i_count1; i++ )
            {
                int i_len;

                BufferGet16(&buf);
                i_len = BufferGet16(&buf);
                BufferGetMemory(&buf, NULL, i_len);

                i_subsize += 4 + i_len;
            }

            for( int i = 0; i < i_count2; i++ )
            {
                int i_len;
                BufferGetMemory(&buf, NULL, 16 + 2);
                i_len = BufferGet32(&buf);
                BufferGetMemory(&buf, NULL, i_len);

                i_subsize += 16 + 6 + i_len;
            }

            if( iSize - i_subsize <= 24 )
            {
                BufferGetMemory(&buf, NULL, iSize - i_subsize);
            }

        }
        else if(!memcmp(guid.data, stream_header, 16))
        {
            int     i_stream_id;
            asfGuid stream_type;

            BufferGetasfGuid( &buf, &stream_type );
            BufferGetMemory( &buf, NULL, 32 );

            i_stream_id = BufferGet8(&buf) & 0x7f;
            BufferGetMemory( &buf, NULL, iSize - 24 - 32 - 16 - 1);
        }
        else if(!memcmp(guid.data, stream_bitrate_guid, 16))
        {
            int     i_count;
            cdx_uint8 i_stream_id;
            int i_bitrate;

            i_count = BufferGet16(&buf);
            iSize -= 2;
            while( i_count > 0 )
            {
                i_stream_id = BufferGet16(&buf) & 0x7f;
                i_bitrate =  BufferGet32(&buf);
                i_count--;
                iSize -= 6;
            }
            BufferGetMemory(&buf, NULL, iSize - 24);
        }
        else
        {
            // skip unknown guid
            BufferGetMemory( &buf, NULL, iSize - 24 );
        }

        if( BufferReadEmpty(&buf))
            return;
    }
}

static int GetAsfHeaderInfo(CdxRtspStreamImplT *impl)
{
    StreamSysT *pSys = impl->pSys;

    const char *pMarker = "a=pgmpu:data:application/vnd.ms.wms-hdr.asfv1;base64,";
    char *pAsf = strcasestr( pSys->pSdp, pMarker );
    char *pEnd;
    cdx_uint8 *pHeader = NULL;
    int nHeader = 0;
    int ret = 0;

    /* Parse the asf header */
    if( pAsf == NULL )
    {
        loge("not fit marker...");
        return -1;
    }

    pAsf += strlen( pMarker );
    pAsf = strdup( pAsf );
    pEnd = strchr( pAsf, '\n' );

    while( pEnd > pAsf && ( *pEnd == '\n' || *pEnd == '\r' ) )
        *pEnd-- = '\0';

    if( pAsf >= pEnd )
    {
        loge("no info.");
        ret = -1;
        goto out;
    }
    //logd("asf info: %s", pAsf);

    nHeader =  pEnd - pAsf;
    pHeader = (cdx_uint8 *)malloc( pEnd - pAsf );
    if(!pHeader)
    {
        loge("malloc failed.");
        ret = -1;
        goto out;
    }
    memset(pHeader, 0, nHeader);

    nHeader = Base64Decode( (cdx_uint8*)pHeader, nHeader, pAsf );

    //logd("nHeader(%d), Hdrbase64(%s)", nHeader, pAsf);
    /*logd("pHeader[%x %x %x %x %x %x]", pHeader[0], pHeader[1], pHeader[2],
        pHeader[3], pHeader[4],pHeader[5]);*/
    if( nHeader <= 0 )
    {
        loge("nHeader <= 0.");
        ret = -1;
        goto out;
    }

    //* Parse it to get packet size
    //asf_HeaderParse( &p_sys->asfh, p_header->p_buffer, p_header->i_buffer );
    GetPktSize(&pSys->asfh, pHeader, nHeader);

    impl->bufAsfhSize = nHeader;
    impl->bufAsfheader = (cdx_uint8 *)malloc(nHeader);
    if(!impl->bufAsfheader)
    {
        loge("malloc failed.");
        ret = -1;
        goto out;
    }
    memcpy(impl->bufAsfheader, pHeader, nHeader);
    logd("Asf header size(%d)", nHeader);

out:
    if(pAsf)
        free(pAsf);
    if(pHeader)
        free(pHeader);
    return ret;
}

static cdx_int32 CreateRtspStreamBuffer(CdxRtspStreamImplT *impl)
{

    impl->buffer = (char *)malloc(RTSP_MAX_STREAM_BUF_SIZE);
    if(!impl->buffer)
    {
        loge("malloc failed.");
        return -1;
    }
    impl->maxBufSize = RTSP_MAX_STREAM_BUF_SIZE;

    impl->bufEndPtr = impl->buffer + impl->maxBufSize -1;
    impl->bufWritePtr = impl->buffer;
    impl->bufReadPtr  = impl->buffer;
    impl->validDataSize = 0;
    impl->bufPos = 0;

    pthread_mutex_init(&impl->bufferMutex, NULL);
    pthread_mutex_init(&impl->lock, NULL);
    pthread_cond_init(&impl->cond, NULL);

    return 0;
}

static void *StartRtspStreamThread(void *pArg)
{
    CdxRtspStreamImplT *impl = (CdxRtspStreamImplT *)pArg;
    int ret = 0;

    pthread_mutex_lock(&impl->lock);
    impl->ioState = CDX_IO_STATE_OK;
    pthread_cond_signal(&impl->cond);
    pthread_mutex_unlock(&impl->lock);

    while(1)
    {
        if(impl->exitFlag)
        {
            pthread_mutex_lock(&impl->lock);
            impl->ioState = CDX_IO_STATE_ERROR;
            pthread_mutex_unlock(&impl->lock);
            break;
        }

        //* buffer is too full
        if(impl->maxBufSize - impl->validDataSize < 1024*1024)
        {
            usleep(100*1000);
            continue;
        }

        ret = GetOneFrame(impl);
        if(ret < 0)
        {
            loge("GetOneFrame failed.");
            pthread_mutex_lock(&impl->lock);
            impl->ioState = CDX_IO_STATE_ERROR;
            pthread_mutex_unlock(&impl->lock);
            break;
        }
    }

    return NULL;
}

static cdx_int32 __CdxRtspStreamConnect(CdxStreamT *stream)
{
    CdxRtspStreamImplT *impl;
    StreamSysT *pSys = NULL;
    int iRet;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxRtspStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStop)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->status = RTSP_STREAM_CONNECTING;
    pthread_mutex_unlock(&impl->lock);

    CDX_LOGV("begin rtsp stream connect.");
    impl->pSys = pSys = (StreamSysT *)malloc(sizeof(StreamSysT));
    if(!pSys)
    {
        CDX_LOGE("malloc pSys failed.");
        return -1;
    }
    memset(impl->pSys, 0x00, sizeof(StreamSysT));

    TABLE_INIT(pSys->iTrack, pSys->track);
    pSys->fNpt = 0.; //* normal presentation time
    pSys->fNptLength = 0.;
    pSys->fNptStart = 0.;
    pSys->bNoData =  1;
    GetPathFromUrl(impl->url, &pSys->pPath); //*pPath: rtsp://pPath
    logv("=========pSys->pPath(%s)", pSys->pPath);
    pSys->fSeekRequest = -1; //* todo

    //* parse URL for rtsp://[user:[passwd]@]serverip:port/options
    RtspUrlParse(&pSys->url, pSys->pPath, 0);

    if((pSys->scheduler = BasicTaskScheduler::createNew()) == NULL)
    {
        loge("BasicTaskScheduler::createNew failed.");
        goto err_out;
    }

    if((pSys->env = BasicUsageEnvironment::createNew(*pSys->scheduler)) == NULL)
    {
        loge("BasicUsageEnvironment::createNew failed.");
        goto err_out;
    }
    if((iRet = RtspConnect(impl)) != 0)
    {
        loge("failed to connect with %s", impl->url);
        goto err_out;
    }

    if(pSys->pSdp == NULL)
    {
        loge("failed to retrieve the rtsp session description.");
        goto err_out;
    }

    /* 1. There is no simple way to find out we should use tcp or udp.
     * 2. Try udp first and switch to tcp is too slow in our implementation.
     * 3. For wide area network, tcp is more likely to be used.
     * 4. RTSP + TS + TCP is the single real use case for now.
     */
    char *p;
    int payloadType;
    p = strstr(pSys->pSdp, "m=video");
    if (p != NULL &&
            (sscanf(p, "m=video %*u %*s %d", &payloadType)) == 1 &&
            payloadType == MP2T)
    {
        impl->bRtsptcp = 1;
        logd("use tcp interleaved mode");
    }

    //* 2. setup
    if((iRet = RtspSessionsSetup(impl)) != 0)
    {
        loge("nothing to play for %s", impl->url);
        goto err_out;
    }
    if(pSys->bReal) //* todo
    {
        loge("check ...");
        goto err_out;
    }

    //* 3. play
    if((iRet = RtspPlay(impl)) != 0)
    {
        loge("play failed.");
        goto err_out;
    }

    if(pSys->iTrack <= 0)
    {
        loge("no valid track.");
        goto err_out;
    }

    if(pSys->bOutAsf) //* asf
    {
        if(CreateRtspStreamBuffer(impl) < 0)
        {
            loge("CreateRtspStreamBuffer failed.");
            goto err_out;
        }

        if(GetAsfHeaderInfo(impl) < 0) //* need store to buffer
        {
            loge("asf header invalid.");
            goto err_out;
        }

        //* copy asf header
        memcpy(impl->bufWritePtr, impl->bufAsfheader, impl->bufAsfhSize);

        //pthread_mutex_lock(&impl->bufferMutex);
        impl->bufWritePtr += impl->bufAsfhSize;
        impl->validDataSize += impl->bufAsfhSize;
        impl->bufPos = impl->bufAsfhSize;
        //pthread_mutex_unlock(&impl->bufferMutex);

        //* create a download thread
        iRet = pthread_create(&impl->threadId, NULL, StartRtspStreamThread, (void *)impl);
        if(iRet)
        {
            loge("create thread failed.");
            goto err_out;
        }
        #if 0
        pthread_mutex_lock(&impl->lock);
        while(impl->ioState != CDX_IO_STATE_OK
              && impl->ioState != CDX_IO_STATE_EOS
              && impl->ioState != CDX_IO_STATE_ERROR)
        {
            pthread_cond_wait(&impl->cond, &impl->lock);
        }
        pthread_mutex_unlock(&impl->lock);
        #endif
        impl->probeData.buf = (char *)malloc(16);
        memcpy(impl->probeData.buf, asf_header, 16);
        impl->probeData.len = 16;
    }
    else if(pSys->bOutMuxed)
    {
        if (pSys->track[0]->rtpPt != MP2T)
            goto err_out;

        if(CreateRtspStreamBuffer(impl) < 0)
        {
            loge("CreateRtspStreamBuffer failed.");
            goto err_out;
        }

        //* create a download thread
        iRet = pthread_create(&impl->threadId, NULL, StartRtspStreamThread, (void *)impl);
        if(iRet)
        {
            loge("create thread failed.");
            goto err_out;
        }
#if 0
        pthread_mutex_lock(&impl->lock);
        while(impl->ioState != CDX_IO_STATE_OK
              && impl->ioState != CDX_IO_STATE_EOS
              && impl->ioState != CDX_IO_STATE_ERROR)
        {
            pthread_cond_wait(&impl->cond, &impl->lock);
        }
        pthread_mutex_unlock(&impl->lock);
#endif
        /* We should do a stream read actually. However, I prefer speed and
         * trust the SDP file.
         * I don't want to change Ts parser, so no probeData.buf = "tsrtsp".
         */
        const int tsPacketNum = 5;
        int i;
        impl->probeData.len = 188 * tsPacketNum;
        impl->probeData.buf = (char *)malloc(impl->probeData.len);
        memset(impl->probeData.buf, 0xff, impl->probeData.len);
        for (i = 0; i < tsPacketNum; i++)
        {
            impl->probeData.buf[i * 188] = 0x47;
            impl->probeData.buf[i * 188 + 1] = 0x1f;
            impl->probeData.buf[i * 188 + 3] = 0x1f;
        }
    }
    else //* raw data
    {
        impl->probeData.buf = (char *)malloc(9);
        memcpy(impl->probeData.buf, "remuxrtsp", 9);
        impl->probeData.len = 9;
        impl->mediaInfoContainer.extraDataType = EXTRA_DATA_RTSP;
        impl->mediaInfoContainer.extraData = &impl->mediaInfo;
    }

    logd("rtsp connect ok.");

    pthread_mutex_lock(&impl->lock);
    impl->status = RTSP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return 0;

err_out:

    if(impl->buffer)
    {
        free(impl->buffer);
        impl->buffer = NULL;
    }
    pthread_mutex_lock(&impl->lock);
    impl->status = RTSP_STREAM_IDLE;
    impl->ioState = CDX_IO_STATE_ERROR;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return -1;
}

static struct CdxStreamOpsS rtspStreamOps =
{
connect:  __CdxRtspStreamConnect,
getProbeData:  __CdxRtspStreamGetProbeData,
read:  __CdxRtspStreamRead,
close: __CdxRtspStreamClose,
getIOState: __CdxRtspStreamGetIOState,
attribute:  __CdxRtspStreamAttribute,
control:    __CdxRtspStreamControl,
write:     NULL,
getMetaData: __CdxRtspStreamGetMetaData,
seek:       NULL,
seekToTime:    __CdxRtspStreamSeekToTime,
eos:         __CdxRtspStreamEos,
tell:      __CdxRtspStreamTell,
size:      __CdxRtspStreamSize,
};

#if 0
static time_t str2second(const char *str)
{
    struct tm tm;
    char *p;
    p = strptime(str, "%Y%m%dT%H%M%SZ", &tm);
    if (p == NULL || *p != '\0')
        return -1;

    tm.tm_isdst = 0;
    return mktime(&tm);
}
#endif

static void extraDataParse(CdxRtspStreamImplT *impl, CdxKeyedVectorT *header)
{
    int i;
    int size = CdxKeyedVectorGetSize(header);
    for (i = 0; i < size; i++)
    {
        char *key = CdxKeyedVectorGetKey(header, i);
        if (key == NULL || strcmp(key, "Range") != 0)
           continue;

        /* clock=19961108T142300Z-19961108T143520Z */
        char *str = CdxKeyedVectorGetValue(header, i);
        if (str == NULL || strncmp(str, "clock=", 6) != 0)
            continue;

        str += strlen("clock=");

        char *str2;
        for (str2 = str; *str2 != '\0'; str2++)
        {
            if (*str2 == '-')
            {
                *str2++ = '\0';
                break;
            }
        }

        logd("start time %s, end time %s", str, str2);
        impl->startTime = strdup(str);
        if (*str2 != '\0')
            impl->endTime = strdup(str2);
        break;
    }
}

static CdxStreamT *__CdxRtspStreamCreate(CdxDataSourceT *source)
{
    CdxRtspStreamImplT *impl;

    CDX_LOGD("source uri:(%s)", source->uri);

    if(strncasecmp("rtsp://", source->uri, 7) != 0)
    {
        CDX_LOGE("not rtsp source.");
        return NULL;
    }

    if (!dlopen("liblive555.so", RTLD_NOW))
    {
        loge("open plugin 'liblive555' failure. dlopen err:%s.\n", dlerror());
        return NULL;
    }

    impl = (CdxRtspStreamImplT *)calloc(1, sizeof(*impl));
    if (NULL == impl)
    {
        CDX_LOGE("CreateRtspStreamImpl fail...");
        return NULL;
    }

    impl->base.ops = &rtspStreamOps;
    impl->url = strdup(source->uri);
    impl->ioState = CDX_IO_STATE_INVALID;
    impl->mPos.type = PKT_HEADER;

    if (source->extraData != NULL &&
            source->extraDataType == EXTRA_DATA_RTSP)
        extraDataParse(impl, (CdxKeyedVectorT *)source->extraData);

#if __RTSP_SAVE_BITSTREAMS
    //sprintf(impl->location, "/data/camera/rtsp_stream_%d.es", streamIndx++);
    //impl->fp_rtsp = fopen(impl->location, "wb");
    impl->fp_rtsp = fopen("/data/camera/rtsp_stream", "wb");
    logd("errno=%d", errno);
#endif

    CDX_LOGD("create rtsp stream ok.");

    return &impl->base;
}

CdxStreamCreatorT rtspStreamCtor =
{
create:  __CdxRtspStreamCreate
};

#ifdef __cplusplus
}
#endif

