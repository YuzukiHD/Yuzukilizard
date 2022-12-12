/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxCafParser.c
* Description :
* History :
*   Author  : Khan <chengkan@allwinnertech.com>
*   Date    : 2014/10/08
*   Comment : ¡ä¡ä?¡§3?¨º?¡ã?¡À?¡ê?¨º¦Ì?? CAF ¦Ì??a?¡ä¨®?1|?¨¹
*/

#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxCafParser.h>

#define CAF_BSWAP16(n) ((((n >> 8) & 0x00ff) | (n << 8)))
#define CAF_BSWAP32(n) ((((n >> 8) & 0x0000ff00) | ((n >> 24) & 0x000000ff) | (n << 24) | \
                         ((n << 8) & 0x00ff0000)))
#define CAF_BSWAP64(n) ((((cdx_int64)n << 56) | (((cdx_int64)n << 40) & 0x00ff000000000000LL) | \
                        (((cdx_int64)n << 24) & 0x0000ff0000000000LL) | \
                        (((cdx_int64)n << 8) & 0x000000ff00000000LL) | \
                        (((cdx_int64)n >> 8) & 0x00000000ff000000LL) | \
                        (((cdx_int64)n >> 24) & 0x0000000000ff0000LL) | \
                        (((cdx_int64)n >> 40) & 0x000000000000ff00LL) | \
                        (((cdx_int64)n >> 56) & 0x00000000000000ffLL)))

cdx_uint32 CafSwap32BtoN(cdx_uint32 inUInt32)
{
#if CAF_TARGET_RT_LITTLE_ENDIAN
    return CAF_BSWAP32(inUInt32);
#else
    return inUInt32;
#endif
}

double CafSwapFloat64BtoN(double in)
{
#if CAF_TARGET_RT_LITTLE_ENDIAN
    union {
        double dbl;
        int64_t ii;
    } x;
    x.dbl = in;
    x.ii = CAF_BSWAP64(x.ii);
    return x.dbl;
#else
    return in;
#endif
}

cdx_uint32 ReadBERInteger(cdx_uint8 * theInputBuffer, cdx_int32 * ioNumBytes)
{
    cdx_uint32 theAnswer = 0;
    cdx_uint8 theData;
    cdx_int32 size = 0;
    do
    {
        theData = theInputBuffer[size];
        theAnswer = (theAnswer << 7) | (theData & 0x7F);
        if (++size > 5)
        {
            size = 0xFFFFFFFF;
            return 0;
        }
    }
    while(((theData & 0x80) != 0) && (size <= *ioNumBytes));

    *ioNumBytes = size;

    return theAnswer;
}

cdx_bool FindCAFFDataStart(CdxStreamT * inputFile, cdx_int32 * dataPos, cdx_int32 * dataSize)
{
    cdx_bool   done = CDX_FALSE;
    cdx_int32  bytesRead = 8;
    cdx_uint32 chunkType = 0, chunkSize = 0;
    cdx_uint8  theBuffer[12];

    CdxStreamSeek(inputFile, bytesRead, SEEK_SET); // start at 8!

    while (!done && bytesRead > 0) // no file size here
    {
        bytesRead = CdxStreamRead(inputFile,theBuffer,12);
        chunkType = ((cdx_int32)(theBuffer[0]) << 24) + ((cdx_int32)(theBuffer[1]) << 16) +
                    ((cdx_int32)(theBuffer[2]) << 8) + theBuffer[3];
        switch(chunkType)
        {
            case TAG_DATA:
                *dataPos = CdxStreamTell(inputFile) + sizeof(cdx_uint32); // skip the edits
                *dataSize = ((cdx_int32)(theBuffer[8]) << 24) + ((cdx_int32)(theBuffer[9]) << 16) +
                            ((cdx_int32)(theBuffer[10]) << 8) + theBuffer[11];
                *dataSize -= 4; // the edits are included in the size
                done = CDX_TRUE;
                break;
            default:
                chunkSize = ((cdx_int32)(theBuffer[8]) << 24) + ((cdx_int32)(theBuffer[9]) << 16) +
                            ((cdx_int32)(theBuffer[10]) << 8) + theBuffer[11];
                CdxStreamSeek(inputFile, chunkSize, SEEK_CUR);
                break;
        }
    }
    return done;
}

cdx_int32 FindCAFFPacketTableStart(CdxStreamT *inputFile, cdx_int32 *paktPos, cdx_int32 *paktSize)
{
    //returns the absolute position within the file
    cdx_int32 currentPosition = CdxStreamTell(inputFile); // record the current position
    cdx_uint8 theReadBuffer[12];
    cdx_uint32 chunkType = 0, chunkSize = 0;
    cdx_bool done = CDX_FALSE;
    cdx_int32 bytesRead = 8;

    CdxStreamSeek(inputFile, bytesRead, SEEK_SET); // start at 8!

    while (!done && bytesRead > 0) // no file size here
    {
        bytesRead = CdxStreamRead(inputFile,theReadBuffer,12);
        chunkType = ((cdx_int32)(theReadBuffer[0]) << 24) + ((cdx_int32)(theReadBuffer[1]) << 16) +
                    ((cdx_int32)(theReadBuffer[2]) << 8) + theReadBuffer[3];
        switch(chunkType)
        {
            case TAG_PAKT:
                *paktPos = CdxStreamTell(inputFile) + kMinCAFFPacketTableHeaderSize;
                done = CDX_TRUE;
                break;
            default:
                chunkSize = ((cdx_int32)(theReadBuffer[8]) << 24) +
                            ((cdx_int32)(theReadBuffer[9]) << 16) +
                            ((cdx_int32)(theReadBuffer[10]) << 8) + theReadBuffer[11];
                CdxStreamSeek(inputFile, chunkSize, SEEK_CUR);
                break;
        }
    }
    CdxStreamRead(inputFile,theReadBuffer,8);
    *paktSize = ((cdx_int32)(theReadBuffer[4]) << 24) + ((cdx_int32)(theReadBuffer[5]) << 16) +
                ((cdx_int32)(theReadBuffer[6]) << 8) + theReadBuffer[7];
    CdxStreamSeek(inputFile, currentPosition, SEEK_SET); // start at 0
    return done;
}

cdx_bool GetCAFFdescFormat(CdxStreamT * inputFile, CafDescription * theInputFormat)
{
    cdx_bool   done = CDX_FALSE;
    cdx_uint32 theChunkSize = 0, theChunkType = 0;
    cdx_uint8  theReadBuffer[32] = {0};

    CdxStreamSeek(inputFile, 8, SEEK_CUR); // skip 4 bytes

    while (!done)
    {
        CdxStreamRead(inputFile,theReadBuffer,4);
        theChunkType = ((cdx_int32)(theReadBuffer[0]) << 24) +
                       ((cdx_int32)(theReadBuffer[1]) << 16) +
                       ((cdx_int32)(theReadBuffer[2]) << 8) +
                       theReadBuffer[3];
        switch (theChunkType)
        {
            case TAG_DESC:
                CdxStreamSeek(inputFile, 8, SEEK_CUR); // skip 8 bytes
                CdxStreamRead(inputFile,theReadBuffer,sizeof(port_CAFAudioDescription));
                theInputFormat->mFormatID =
                           CafSwap32BtoN(((port_CAFAudioDescription *)(theReadBuffer))->mFormatID);

                theInputFormat->mChannelsPerFrame =
                   CafSwap32BtoN(((port_CAFAudioDescription *)(theReadBuffer))->mChannelsPerFrame);

                theInputFormat->mSampleRate =
                    CafSwapFloat64BtoN(((port_CAFAudioDescription *)(theReadBuffer))->mSampleRate);

                theInputFormat->mBitsPerChannel =
                     CafSwap32BtoN(((port_CAFAudioDescription *)(theReadBuffer))->mBitsPerChannel);

                theInputFormat->mFormatFlags =
                        CafSwap32BtoN(((port_CAFAudioDescription *)(theReadBuffer))->mFormatFlags);

                theInputFormat->mBytesPerPacket =
                     CafSwap32BtoN(((port_CAFAudioDescription *)(theReadBuffer))->mBytesPerPacket);

                if (theInputFormat->mFormatID == kALACFormatAppleLossless)
                {
                    theInputFormat->mBytesPerFrame = 0;
                }
                else
                {
                    theInputFormat->mBytesPerFrame = theInputFormat->mBytesPerPacket;
                    if ((theInputFormat->mFormatFlags & 0x02) == 0x02)
                    {
                        theInputFormat->mFormatFlags &= 0xfffffffc;
                    }
                    else
                    {
                        theInputFormat->mFormatFlags |= 0x02;
                    }
                }
                theInputFormat->mFramesPerPacket =
                    CafSwap32BtoN(((port_CAFAudioDescription *)(theReadBuffer))->mFramesPerPacket);

                theInputFormat->mReserved = 0;
                done = CDX_TRUE;
                break;
            default:
                // read the size and skip
                CdxStreamRead(inputFile,theReadBuffer,8);
                theChunkSize = ((cdx_int32)(theReadBuffer[4]) << 24) +
                               ((cdx_int32)(theReadBuffer[5]) << 16) +
                               ((cdx_int32)(theReadBuffer[6]) << 8) +
                               theReadBuffer[7];
                CdxStreamSeek(inputFile, theChunkSize, SEEK_CUR);
                break;
        }
    }
    return done;
}

static int CafInit(CdxParserT *caf_impl)
{
    cdx_int32  ret = 0, i = 0,skiplen = 0;
    cdx_int32  dataSize = 0;
    cdx_uint8  tembuf[64]={0};
    cdx_bool   done = CDX_FALSE;
    cdx_int32   thePacketTableSize = 0, packetTablePos = 0, numBytes = 0,chunkType = 0;
    //CafDescription  theInputFormat;
    CdxStreamT *cdxStream = NULL;
    CafParserImplS *impl= (CafParserImplS*)caf_impl;

    impl->fileSize = CdxStreamSize(impl->stream);
    cdxStream = impl->stream;

    if(impl->fileSize<0)
    {
        goto OPENFAILURE;
    }
    if(0)//!GetCAFFdescFormat(cdxStream,&theInputFormat))
    {
        goto OPENFAILURE;
    }
    if(!FindCAFFDataStart(cdxStream, &impl->firstpktpos, &dataSize))
    {
        goto OPENFAILURE;
    }

    FindCAFFPacketTableStart(cdxStream, &packetTablePos, &thePacketTableSize);

    if (!impl->theIndexBuffer)
    {
        impl->theIndexBuffer = (cdx_uint32 *)malloc(thePacketTableSize * sizeof(uint32_t));
        if (!impl->theIndexBuffer)
        {
            CDX_LOGW("Warning!! No enough memory for theIndexBuffer!!!");
            goto OPENFAILURE;
        }
        memset(impl->theIndexBuffer, 0, thePacketTableSize);
    }

    for(i = 0; i < thePacketTableSize; i++)
    {
        CdxStreamSeek(cdxStream, packetTablePos, SEEK_SET);
        numBytes = CdxStreamRead(cdxStream,tembuf, kMaxBERSize);
        numBytes = kMaxBERSize;
        impl->theIndexBuffer[i] = ReadBERInteger(tembuf, &numBytes);
        packetTablePos += numBytes;
    }

    ret = 8;
    CdxStreamSeek(cdxStream,ret,SEEK_SET);
    while(!done && (ret > 0))
    {
        ret = CdxStreamRead(cdxStream,tembuf, 12);
        chunkType = ((cdx_int32)(tembuf[0]) << 24) + ((cdx_int32)(tembuf[1]) << 16) +
                    ((cdx_int32)(tembuf[2]) << 8) + tembuf[3];
        switch(chunkType)
        {
            case TAG_KUKI:
                impl->extradata_size = ((tembuf[8]<<24)|
                                        (tembuf[9]<<16)|
                                        (tembuf[10]<<8)|
                                        (tembuf[11]&0xff))+4;
                impl->extradata = malloc(impl->extradata_size + 8);
                if(!impl->extradata)
                {
                    CDX_LOGW("Warning!! No enough memory for extradata!!!");
                    goto OPENFAILURE;
                }
                memset(impl->extradata,0,impl->extradata_size + 8);
                if(CdxStreamRead(cdxStream,impl->extradata+4,impl->extradata_size-4) <
                                 impl->extradata_size-4)
                {
                    CDX_LOGW("Read extradata failed!!! No enough %d bytes!",impl->extradata_size);
                    goto OPENFAILURE;
                }
                done = CDX_TRUE;
                break;
            default:
                skiplen = ((cdx_int32)(tembuf[8]) << 24) +
                          ((cdx_int32)(tembuf[9]) << 16) +
                          ((cdx_int32)(tembuf[10]) << 8) +
                          tembuf[11];
                CdxStreamSeek(cdxStream, skiplen, SEEK_CUR);
                break;
        }
    }
    impl->sampleperpkt = (impl->extradata[4]<<24)|
                         (impl->extradata[5]<<16)|
                         (impl->extradata[6]<<8)|
                         (impl->extradata[7]&0xff);
    impl->ulBitsSample = impl->extradata[9];
    impl->ulChannels   = impl->extradata[13];
    impl->ulSampleRate = (impl->extradata[24]<<24)|
                         (impl->extradata[25]<<16)|
                         (impl->extradata[26]<<8)|
                         (impl->extradata[27]&0xff);
    impl->totalsamples = impl->sampleperpkt * thePacketTableSize;
    impl->thePacketTableSize = thePacketTableSize;
    impl->ulDuration   = (1000LL * impl->totalsamples)/impl->ulSampleRate;
    CDX_LOGV("sample per packet:%d,packets:%d",impl->sampleperpkt,impl->thePacketTableSize);
    CdxStreamSeek(cdxStream,impl->firstpktpos,SEEK_SET);
    impl->file_offset = impl->firstpktpos;
    impl->mErrno = PSR_OK;
    pthread_cond_signal(&impl->cond);
    return 0;

OPENFAILURE:
    CDX_LOGE("CafOpenThread fail!!!");
    impl->mErrno = PSR_OPEN_FAIL;
    pthread_cond_signal(&impl->cond);
    return -1;
}

static cdx_int32 __CafParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    struct CafParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct CafParserImplS, base);
    (void)param;

    switch (cmd)
    {
        case CDX_PSR_CMD_DISABLE_AUDIO:
        case CDX_PSR_CMD_DISABLE_VIDEO:
        case CDX_PSR_CMD_SWITCH_AUDIO:
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            CdxStreamForceStop(impl->stream);
            break;
        case CDX_PSR_CMD_CLR_FORCESTOP:
            CdxStreamClrForceStop(impl->stream);
            break;
        default :
            CDX_LOGW("not implement...(%d)", cmd);
            break;
    }
    impl->flags = cmd;
    return CDX_SUCCESS;
}

static cdx_int32 __CafParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    struct CafParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct CafParserImplS, base);

    pkt->type = CDX_MEDIA_AUDIO;
    pkt->length = impl->theIndexBuffer[impl->framecount];
    pkt->pts = impl->nowsamples*1000000ll/impl->ulSampleRate;//one frame pts;
    pkt->flags |= (FIRST_PART|LAST_PART);

    return CDX_SUCCESS;
}

static cdx_int32 __CafParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 read_length;
    struct CafParserImplS *impl = NULL;
    CdxStreamT *cdxStream = NULL;

    impl = CdxContainerOf(parser, struct CafParserImplS, base);
    cdxStream = impl->stream;
    //READ ACTION

    if(pkt->length <= pkt->buflen)
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->length);
    }
    else
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->buflen);
        read_length += CdxStreamRead(cdxStream, pkt->ringBuf,    pkt->length - pkt->buflen);
    }

    if(read_length < 0)
    {
        CDX_LOGE("CdxStreamRead fail");
        impl->mErrno = PSR_IO_ERR;
        return CDX_FAILURE;
    }
    else if(read_length == 0)
    {
       CDX_LOGD("CdxStream EOS");
       impl->mErrno = PSR_EOS;
       return CDX_FAILURE;
    }
    pkt->length = read_length;
    impl->file_offset += read_length;
    impl->nowsamples += impl->sampleperpkt;
    impl->framecount++;

    // TODO: fill pkt
    return CDX_SUCCESS;
}

static cdx_int32 __CafParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    struct CafParserImplS *impl = NULL;
    struct CdxProgramS *cdxProgram = NULL;

    impl = CdxContainerOf(parser, struct CafParserImplS, base);
    memset(mediaInfo, 0x00, sizeof(*mediaInfo));

    if(impl->mErrno != PSR_OK)
    {
        CDX_LOGD("audio parse status no PSR_OK");
        return CDX_FAILURE;
    }

    mediaInfo->programNum = 1;
    mediaInfo->programIndex = 0;
    cdxProgram = &mediaInfo->program[0];
    memset(cdxProgram, 0, sizeof(struct CdxProgramS));
    cdxProgram->id = 0;
    cdxProgram->audioNum = 1;
    cdxProgram->videoNum = 0;//
    cdxProgram->subtitleNum = 0;
    cdxProgram->audioIndex = 0;
    cdxProgram->videoIndex = 0;
    cdxProgram->subtitleIndex = 0;

    mediaInfo->bSeekable = CdxStreamSeekAble(impl->stream);
    mediaInfo->fileSize = impl->fileSize;

    cdxProgram->duration = impl->ulDuration;
    cdxProgram->audio[0].eCodecFormat    = AUDIO_CODEC_FORMAT_ALAC;
    cdxProgram->audio[0].eSubCodecFormat = 0;
    cdxProgram->audio[0].nChannelNum     = impl->ulChannels;
    cdxProgram->audio[0].nBitsPerSample  = impl->ulBitsSample;
    cdxProgram->audio[0].nSampleRate     = impl->ulSampleRate;
    cdxProgram->audio[0].nCodecSpecificDataLen = impl->extradata_size;
    cdxProgram->audio[0].pCodecSpecificData = (char *)impl->extradata;
    return CDX_SUCCESS;
}

static cdx_int32 __CafParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    struct CafParserImplS *impl = NULL;
    cdx_int32 samples = 0,i = 0;
    cdx_int64 offset = 0;
    cdx_int32 tableindex = 0;
    impl = CdxContainerOf(parser, struct CafParserImplS, base);
    samples=(timeUs*impl->ulSampleRate)/1000000ll;
    tableindex = samples / impl->sampleperpkt;

    offset = (cdx_int64)impl->firstpktpos;
    for(i = 0; i < tableindex; i++)
    {
        offset += (cdx_int64)impl->theIndexBuffer[i];
    }

    if(offset != impl->file_offset)
    {
        if(offset > impl->fileSize)
        {
            offset = impl->fileSize;
        }
        if(CdxStreamSeek(impl->stream,offset,SEEK_SET))
        {
            CDX_LOGE("CdxStreamSeek fail");
            return CDX_FAILURE;
        }
    }
    impl->file_offset = offset;
    impl->framecount = samples / impl->sampleperpkt;
    impl->nowsamples = impl->framecount * impl->sampleperpkt;
    CdxStreamSeek(impl->stream,impl->file_offset,SEEK_SET);
    CDX_LOGV("Seek to time:%lld,to position:%lld,fliesize:%lld,packets index:%d",
              timeUs,impl->file_offset,impl->fileSize,tableindex);
    return CDX_SUCCESS;
}

static cdx_uint32 __CafParserAttribute(CdxParserT *parser)
{
    struct CafParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct CafParserImplS, base);
    return 0;
}

static cdx_int32 __CafParserGetStatus(CdxParserT *parser)
{
    struct CafParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct CafParserImplS, base);
#if 0
    if (CdxStreamEos(impl->stream))
    {
        return PSR_EOS;
    }
#endif
    return impl->mErrno;
}

static cdx_int32 __CafParserClose(CdxParserT *parser)
{
    struct CafParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct CafParserImplS, base);
    CdxStreamClose(impl->stream);

    if(impl->extradata)
    {
        CdxFree(impl->extradata);
    }
    if(impl->theIndexBuffer)
    {
        CdxFree(impl->theIndexBuffer);
    }
    pthread_cond_destroy(&impl->cond);
    CdxFree(impl);
    return CDX_SUCCESS;
}

static struct CdxParserOpsS cafParserOps =
{
    .control = __CafParserControl,
    .prefetch = __CafParserPrefetch,
    .read = __CafParserRead,
    .getMediaInfo = __CafParserGetMediaInfo,
    .seekTo = __CafParserSeekTo,
    .attribute = __CafParserAttribute,
    .getStatus = __CafParserGetStatus,
    .close = __CafParserClose,
    .init = CafInit
};

/*
static cdx_int32 CafProbe(CdxStreamProbeDataT *p)
{
    cdx_char *d;
    d = p->buf;
    if (p->len < 4 || memcmp(d, "caff", 4))
        return CDX_FALSE;
    return CDX_TRUE;
}
*/

static cdx_uint32 __CafParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_CHECK(probeData);
    if(probeData->len < 4)
    {
        CDX_LOGE("Probe Caf_header data is not enough.");
        return 0;
    }

    if(memcmp(probeData->buf, "caff", 4))
    {
        CDX_LOGE("Caf probe failed.");
        return 0;
    }
    return 100;
}

static CdxParserT *__CafParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    //cdx_int32 ret = 0;
    struct CafParserImplS *impl;
    impl = CdxMalloc(sizeof(*impl));
    if (impl == NULL) {
        CDX_LOGE("CafParserOpen Failed");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(impl, 0x00, sizeof(*impl));
    (void)flags;

    impl->stream = stream;
    impl->base.ops = &cafParserOps;
    pthread_cond_init(&impl->cond, NULL);
    //ret = pthread_create(&impl->openTid, NULL, CafOpenThread, (void*)impl);
    //CDX_FORCE_CHECK(!ret);
    impl->mErrno = PSR_INVALID;

    return &impl->base;
}

struct CdxParserCreatorS cafParserCtor =
{
    .probe = __CafParserProbe,
    .create = __CafParserOpen
};
