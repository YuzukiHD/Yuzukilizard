/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxMp3Parser.c
* Description :
* History :
*   Author  : Khan <chengkan@allwinnertech.com>
*   Date    : 2014/08/08
*   Comment : 创建初始版本，实现 MP3 的解复用功能
*/

#define LOG_TAG "_mp3psr"
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>

#include "CdxMp3Parser.h"

#include <fcntl.h>
const int kMaxReadBytes     = 1024;
const int kMaxBytesChecked  = 128 * 1024;

#define SYNCFRMNUM 40
#define MIN_MP3_VALID_FRAME_COMBO 4

#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))

static uint16_t MP3_U16_AT(const uint8_t *ptr) {
    return ptr[0] << 8 | ptr[1];
}

static uint32_t MP3_U32_AT(const uint8_t *ptr) {
    return ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
}

static uint32_t MP3_U24_AT(const uint8_t *ptr) {
    return ptr[0] << 16 | ptr[1] << 8 | ptr[2];
}

#define AV_RB32(x)  ((((const unsigned char*)(x))[0] << 24) | \
    (((const unsigned char*)(x))[1] << 16) | \
    (((const unsigned char*)(x))[2] <<  8) | \
                      ((const unsigned char*)(x))[3])

static const uint32_t kMask = 0xfffe0c00;

cdx_bool GetMPEGAudioFrameSize(
    uint32_t header, size_t *frameSize,
    int *outSamplingRate, int *outChannels,
    int *outBitrate, int *outNumSamples);

static float Resync(CdxStreamProbeDataT *p){
    //const sp<DataSource> &source, uint32_t match_header,
    //off64_t *inout_pos, off64_t *post_id3_pos, uint32_t *out_header) {
    cdx_int64  inout_pos,post_id3_pos;
    cdx_uint32 out_header,match_header;
    cdx_uint8 *source = (cdx_uint8 *)p->buf;
    float score = 0.0;

    post_id3_pos = 0;
    inout_pos = 0;
    match_header = 0;
    cdx_int64 pos1 = 0;

    if (inout_pos == 0) {
        // Skip an optional ID3 header if syncing at the very beginning
        // of the datasource.
        for (;;) {
            cdx_uint8 id3header[10];
            pos1 += sizeof(id3header);
            if(pos1 > p->len)
            {
                return 0.;
            }
            memcpy(id3header,source + inout_pos,sizeof(id3header));
            //if (source->readAt(*inout_pos, id3header, sizeof(id3header))
              //      < (ssize_t)sizeof(id3header)) {
                // If we can't even read these 10 bytes, we might as well bail
                // out, even if there _were_ 10 bytes of valid mp3 audio data...
                //return false;
            //}
            if (memcmp("ID3", id3header, 3)) {
                break;
            }
            // Skip the ID3v2 header.
            cdx_int32 len =
                ((id3header[6] & 0x7f) << 21)
                | ((id3header[7] & 0x7f) << 14)
                | ((id3header[8] & 0x7f) << 7)
                | (id3header[9] & 0x7f);

            len += 10;
            inout_pos += len;
            CDX_LOGD("skipped ID3 tag, new starting offset is %lld (0x%016llx)",
                 inout_pos, inout_pos);
            if(inout_pos != 0)
            {
                CDX_LOGD("this is ID3 first : %lld",inout_pos);
                return 0.;
            }
        }
        post_id3_pos = inout_pos;
    }
    cdx_int64 pos = inout_pos;
    cdx_int32  valid = 0;

    //const cdx_int32 kMaxReadBytes = 1024;
    cdx_int32 MaxBytesChecked = kMaxBytesChecked > p->len?p->len:kMaxBytesChecked;
    cdx_uint8 buf[kMaxReadBytes];
    cdx_int32 bytesToRead = kMaxReadBytes;
    cdx_int32 totalBytesRead = 0;
    cdx_int32 remainingBytes = 0;
    cdx_int32 max_frame_combo = 0;
    cdx_int32 reachEOS = 0;
    cdx_uint8 *tmp = buf;

    do {
        if (pos >= inout_pos + MaxBytesChecked) {
            // Don't scan forever.
            CDX_LOGW("giving up at offset %lld", pos);
            break;
        }

        if (remainingBytes < 4) {
            if (reachEOS) {
                break;
            } else {
                memcpy(buf, tmp, remainingBytes);
                bytesToRead = kMaxReadBytes - remainingBytes;
                /*
                 * The next read position should start from the end of
                 * the last buffer, and thus should include the remaining
                 * bytes in the buffer.
                 */
                if(pos + remainingBytes + bytesToRead > p->len)
                {
                    reachEOS = 1;
                    bytesToRead = p->len - pos - remainingBytes;
                }
                memcpy(buf + remainingBytes,source + pos + remainingBytes,bytesToRead);
                totalBytesRead = bytesToRead;
                //totalBytesRead = source->readAt(pos + remainingBytes,
                                               // buf + remainingBytes,
                                               // bytesToRead);
                //if (totalBytesRead <= 0) {
                 //   break;
                //}
                totalBytesRead += remainingBytes;
                remainingBytes = totalBytesRead;
                tmp = buf;
                continue;
            }
        }
        cdx_uint32 header = AV_RB32(tmp);
        if (match_header != 0 && (header & kMask) != (match_header & kMask)) {
            ++pos;
            ++tmp;
            --remainingBytes;
            continue;
        }

        size_t frame_size;
        cdx_int32  sample_rate,num_channels, bitrate, NumSamples;
        if (!GetMPEGAudioFrameSize(
                    header, &frame_size,
                    &sample_rate, &num_channels, &bitrate, &NumSamples)) {
            ++pos;
            ++tmp;
            --remainingBytes;
            continue;
        }
        CDX_LOGV("found possible 1st frame at %lld (header = 0x%08x)", pos, header);
        // We found what looks like a valid frame,
        // now find its successors.

        cdx_int64 test_pos = pos + frame_size;
        valid = 1;
        cdx_int32 j = 0;
        for (j = 0;j < SYNCFRMNUM - 1; ++j) {
            cdx_uint8 tmp[4];
            if(test_pos + 4 > p->len)
            {
                if(j < MIN_MP3_VALID_FRAME_COMBO)
                    valid = 0;
                break;
            }
            memcpy(tmp, source + test_pos, 4);
            //if (source->readAt(test_pos, tmp, 4) < 4) {
               // valid = false;
               // break;
            //}

            cdx_uint32 test_header = AV_RB32(tmp);
            CDX_LOGV("subsequent header is %08x", test_header);
            if ((test_header & kMask) != (header & kMask)) {
                if(j < MIN_MP3_VALID_FRAME_COMBO)
                    valid = 0;
                break;
            }

            size_t test_frame_size;
            if (!GetMPEGAudioFrameSize(
                        test_header, &test_frame_size,
                        &sample_rate, &num_channels, &bitrate, &NumSamples)) {
                if(j < MIN_MP3_VALID_FRAME_COMBO)
                    valid = 0;
                break;
            }

            CDX_LOGV("found subsequent frame #%d at %lld", j + 2, test_pos);
            test_pos += test_frame_size;
        }
        max_frame_combo = max_frame_combo > (j + 1)?max_frame_combo:(j + 1);
        if (valid) {
            inout_pos = pos;
            out_header = header;

        } else {
            CDX_LOGW("no dice, no valid sequence of frames found. combo : %d", j);
        }

        ++pos;
        ++tmp;
        --remainingBytes;
    } while (!valid);

    if(valid)
    {
        CDX_LOGD("inout_pos : %lld, frame_combo : %d, probe len : %d",
            inout_pos, max_frame_combo, p->len);
        if(inout_pos == 0)//If mp3 sync header start with the very begining, just trust it.
        {
            score = 1.0;
        }
        else
        {
            score = 0.01;
            if(max_frame_combo >= 7)
            {
                score = 0.25;
            }
            if(max_frame_combo >= SYNCFRMNUM)
            {
                score = 0.75;
            }
        }
    }
    else
        score = 0.0;
    return score;
}

static inline int CdxMpaCheckHeader(cdx_uint32 nHeader){
    /* header */
    if ((nHeader & 0xffe00000) != 0xffe00000)
        return -1;
    /* layer check */
    if ((nHeader & (3<<17)) == 0)
        return -1;
    /* bit rate */
    if ((nHeader & (0xf<<12)) == 0xf<<12)
        return -1;
    /* frequency */
    if ((nHeader & (3<<10)) == 3<<10)
        return -1;
    return 0;
}

/*
static int CdxAudioDecodeHeader(MPADecodeHeader *s, cdx_uint32 header)
{
    int mSampleRate, mFrameSize, mpeg25, padding;
    int mSampleRateIndex, mBitrateIndex;
    if ((1<<20) & header) {
        s->lsf = (header & (1<<19)) ? 0 : 1;
        mpeg25 = 0;
    } else {
        s->lsf = 1;
        mpeg25 = 1;
    }

    s->layer = 4 - ((header >> 17) & 3);
    // extract frequency
    mSampleRateIndex = (header >> 10) & 3;
    mSampleRate = CdxMpaFreqTab[mSampleRateIndex] >> (s->lsf + mpeg25);
    mSampleRateIndex += 3 * (s->lsf + mpeg25);
    s->mSampleRateIndex = mSampleRateIndex;
    s->errorProtection = ((header >> 16) & 1) ^ 1;
    s->mSampleRate = mSampleRate;

    mBitrateIndex = (header >> 12) & 0xf;
    padding = (header >> 9) & 1;
    //extension = (header >> 8) & 1;
    s->mode = (header >> 6) & 3;
    s->modeExt = (header >> 4) & 3;
    //copyright = (header >> 3) & 1;
    //original = (header >> 2) & 1;
    //emphasis = header & 3;

    if (s->mode == MPA_MONO)
        s->nChannels = 1;
    else
        s->nChannels = 2;

    if (mBitrateIndex != 0) {
        mFrameSize = CdxMpaBitrateTab[s->lsf][s->layer - 1][mBitrateIndex];
        s->mBitRate = mFrameSize * 1000;
        switch(s->layer) {
        case 1:
            mFrameSize = (mFrameSize * 12000) / mSampleRate;
            mFrameSize = (mFrameSize + padding) * 4;
            break;
        case 2:
            mFrameSize = (mFrameSize * 144000) / mSampleRate;
            mFrameSize += padding;
            break;
        default:
        case 3:
            mFrameSize = (mFrameSize * 144000) / (mSampleRate << s->lsf);
            mFrameSize += padding;
            break;
        }
        s->mFrameSize = mFrameSize;
    } else {
        //if no frame size computed, signal it
        return 1;
    }

    return 0;
}

static int CdxMpaDecodeHeader(cdx_uint32 head, int *mSampleRate, int *nChannels, int *frameSize,
                              int *mBitRate,int *layer)
{
    MPADecodeHeader s1, *s = &s1;

    if (CdxMpaCheckHeader(head) != 0)
        return -1;

    if (CdxAudioDecodeHeader(s, head) != 0) {
        return -1;
    }

    switch(s->layer) {
    case 1:
        //avctx->codec_id = CODEC_ID_MP1;
        *frameSize = 384;
        break;
    case 2:
        //avctx->codec_id = CODEC_ID_MP2;
        *frameSize = 1152;
        break;
    default:
    case 3:
        //avctx->codec_id = CODEC_ID_MP3;
        if (s->lsf)
            *frameSize = 576;
        else
            *frameSize = 1152;
        break;
    }

    *mSampleRate = s->mSampleRate;
    *nChannels = s->nChannels;
    *mBitRate = s->mBitRate;
    *layer = s->layer;
    return s->mFrameSize;
}
*/

#if 0
static int CdxMp3Probe(CdxStreamProbeDataT *p)
{
    int mMaxFrames, mFirstFrames = 0;
    int fSize, mFrames, mSampleRate;
    cdx_uint32 nHeader;
    cdx_uint8 *buf, *buf0, *buf2, *end;
    int layer = 0;

    buf0 = (uint8_t *)p->buf;
    if(CdxId3v2Match(buf0)) {
        buf0 += CdxId3v2TagLen(buf0);
    }
    end = (uint8_t *)p->buf + p->len - sizeof(cdx_uint32);
    while(buf0 < end && !*buf0)
        buf0++;

    mMaxFrames = 0;
    buf = buf0;

    for(; buf < end; buf= buf2 + 1) {
        buf2 = buf;
        if (CdxMpaCheckHeader(AV_RB32(buf2)))
            continue;

        for(mFrames = 0; buf2 < end; mFrames++) {
            nHeader = AV_RB32(buf2);
            fSize = CdxMpaDecodeHeader(nHeader, &mSampleRate, &mSampleRate, &mSampleRate,
                                       &mSampleRate,&layer);
            if(fSize < 0){
                return 0;//break;//for error check aac->mp3;
            }
            buf2 += fSize;
        }
        mMaxFrames = FFMAX(mMaxFrames, mFrames);
        if(buf == buf0)
            mFirstFrames= mFrames;
    }

    // keep this in sync with ac3 probe, both need to avoid
    // issues with MPEG-files!
    if((layer == 1)||(layer == 2))
        return AVPROBE_SCORE_MAX -3;
    if (mFirstFrames >= 4)
        return AVPROBE_SCORE_MAX / 2 + 1;
    else if(mMaxFrames > 500)
        return AVPROBE_SCORE_MAX / 2;
    else if(mMaxFrames >= 2)
        return AVPROBE_SCORE_MAX / 4;
    else if(buf0 != (uint8_t *)p->buf)
        return AVPROBE_SCORE_MAX / 4 - 1;
    else
        return 0;
}
#endif

cdx_bool GetMPEGAudioFrameSize(
        uint32_t header, size_t *frameSize,
        int *outSamplingRate, int *outChannels,
        int *outBitrate, int *outNumSamples) {
    *frameSize = 0;
    if (outSamplingRate) {
        *outSamplingRate = 0;
    }

    if (outChannels) {
        *outChannels = 0;
    }

    if (outBitrate) {
        *outBitrate = 0;
    }

    if (outNumSamples) {
        *outNumSamples = 1152;
    }

    if ((header & 0xffe00000) != 0xffe00000) {
        return CDX_FALSE;
    }

    unsigned nVersion = (header >> 19) & 0x03;

    if (nVersion == 0x01) {
        return CDX_FALSE;
    }

    unsigned layer = (header >> 17) & 0x03;

    if (layer == 0x00) {
        return CDX_FALSE;
    }

    //unsigned protection = (header >> 16) & 1;

    unsigned bitrateIndex = (header >> 12) & 0x0f;

    if (bitrateIndex == 0 || bitrateIndex == 0x0f) {
        // Disallow "free" bitrate.
        return CDX_FALSE;
    }

    unsigned nSamplingRateIndex = (header >> 10) & 0x03;

    if (nSamplingRateIndex == 3) {
        return CDX_FALSE;
    }

    static const int nSamplingRateV1[] = {44100, 48000, 32000};
    int samplingRate = nSamplingRateV1[nSamplingRateIndex];
    if (nVersion == 2 /* V2 */) {
        samplingRate /= 2;
    } else if (nVersion == 0 /* V2.5 */) {
        samplingRate /= 4;
    }

    unsigned padding = (header >> 9) & 1;

    if (layer == 3) {
        // layer I
        static const int nBitrateV1[] = {
            32, 64, 96, 128, 160, 192, 224,
            256, 288, 320, 352, 384, 416, 448
        };

        static const int nBitrateV2[] = {
            32, 48, 56, 64, 80, 96, 112, 128,
            144, 160, 176, 192, 224, 256
        };

        int bitRate =
            (nVersion == 3 /* V1 */)
                ? nBitrateV1[bitrateIndex - 1]
                : nBitrateV2[bitrateIndex - 1];

        if (outBitrate) {
            *outBitrate = bitRate;
        }

        *frameSize = (12000 * bitRate / samplingRate + padding) * 4;

        if (outNumSamples) {
            *outNumSamples = 384;
        }
    } else {
        // layer II or III

        static const int nBitrateV1L2[] = {
            32, 48, 56, 64, 80, 96, 112, 128,
            160, 192, 224, 256, 320, 384
        };

        static const int nBitrateV1L3[] = {
            32, 40, 48, 56, 64, 80, 96, 112,
            128, 160, 192, 224, 256, 320
        };

        static const int nBitrateV2[] = {
            8, 16, 24, 32, 40, 48, 56, 64,
            80, 96, 112, 128, 144, 160
        };

        int bitRate;
        if (nVersion == 3 /* V1 */) {
            bitRate = (layer == 2 /* L2 */)
                ? nBitrateV1L2[bitrateIndex - 1]
                : nBitrateV1L3[bitrateIndex - 1];

            if (outNumSamples) {
                *outNumSamples = 1152;
            }
        } else {
            // V2 (or 2.5)

            bitRate = nBitrateV2[bitrateIndex - 1];
            if (outNumSamples) {
                *outNumSamples = (layer == 1 /* L3 */) ? 576 : 1152;
            }
        }

        if (outBitrate) {
            *outBitrate = bitRate;
        }

        if (nVersion == 3 /* V1 */) {
            *frameSize = 144000 * bitRate / samplingRate + padding;
        } else {
            // V2 or V2.5
            size_t tmp = (layer == 1 /* L3 */) ? 72000 : 144000;
            *frameSize = tmp * bitRate / samplingRate + padding;
        }
    }

    if (outSamplingRate) {
        *outSamplingRate = samplingRate;
    }

    if (outChannels) {
        int channelMode = (header >> 6) & 3;

        *outChannels = (channelMode == 3) ? 1 : 2;
    }

    return CDX_TRUE;
}

static XINGSeeker *CreateXINGSeeker(CdxParserT *parser,  cdx_int64 firstFramePos)
{
    MP3ParserImpl *mp3;
    cdx_int32     noSeeker = 0;
    mp3 = (MP3ParserImpl *)parser;
    if (!mp3) {
        CDX_LOGE("Mp3 file parser lib has not been initiated!");
        return NULL;
    }

    XINGSeeker *seeker = (XINGSeeker *)malloc (sizeof(XINGSeeker));
    memset(seeker, 0x00, sizeof(XINGSeeker));

    seeker->mFirstFramePos = firstFramePos;

    uint8_t ucBuffer[4];
    int offset = firstFramePos;
    if (CdxStreamSeek(mp3->stream, offset, SEEK_SET)) {
         noSeeker = 1;
         goto EXIT;
    }
    if (CdxStreamRead(mp3->stream, &ucBuffer, 4) < 4) { // get header
        noSeeker = 1;
        goto EXIT;
    }
    offset += 4;

    int header = MP3_U32_AT(ucBuffer);;
    size_t xingFramesize = 0;
    int samplingRate = 0;
    int mNumChannels;
    int mSamplePerFrame = 0;
    if (!GetMPEGAudioFrameSize(header, &xingFramesize, &samplingRate, &mNumChannels,
                               NULL, &mSamplePerFrame)) {
        noSeeker = 1;
        goto EXIT;
    }
    seeker->mFirstFramePos += xingFramesize;

    uint8_t nVersion = (ucBuffer[1] >> 3) & 3;

    // determine offset of XING header
    if(nVersion & 1) { // mpeg1
        if (mNumChannels != 1) offset += 32;
        else offset += 17;
    } else { // mpeg 2 or 2.5
        if (mNumChannels != 1) offset += 17;
        else offset += 9;
    }

    int xingbase = offset;
    if (CdxStreamSeek(mp3->stream, offset, SEEK_SET)) {
         noSeeker = 1;
         goto EXIT;
    }
    if (CdxStreamRead(mp3->stream, &ucBuffer, 4) < 4) { // XING header ID
        noSeeker = 1;
        goto EXIT;
    }
    offset += 4;

    // Check XING ID
    if ((ucBuffer[0] != 'X') || (ucBuffer[1] != 'i')
                || (ucBuffer[2] != 'n') || (ucBuffer[3] != 'g')) {
        if ((ucBuffer[0] != 'I') || (ucBuffer[1] != 'n')
                    || (ucBuffer[2] != 'f') || (ucBuffer[3] != 'o')) {
            noSeeker = 1;
            goto EXIT;
        }
    }
    if (CdxStreamSeek(mp3->stream, offset, SEEK_SET)) {
         noSeeker = 1;
         goto EXIT;
    }
    if (CdxStreamRead(mp3->stream, &ucBuffer, 4) < 4) { // flags
        noSeeker = 1;
        goto EXIT;
    }
    offset += 4;
    uint32_t flags = MP3_U32_AT(ucBuffer);

    if (flags & 0x0001) {  // Frames field is present
        if (CdxStreamSeek(mp3->stream, offset, SEEK_SET)) {
             noSeeker = 1;
             goto EXIT;
        }
        if (CdxStreamRead(mp3->stream, ucBuffer, 4) < 4) {
            noSeeker = 1;
            goto EXIT;
        }
        int32_t nFrames = MP3_U32_AT(ucBuffer);
        // only update mDurationUs if the calculated duration is valid (non zero)
        // otherwise, leave duration at -1 so that getDuration() and getOffsetForTime()
        // return false when called, to indicate that this xing tag does not have the
        // requested information
        if (nFrames) {
            seeker->mDurationUs = (int64_t)nFrames * mSamplePerFrame * 1000000LL / samplingRate;
        }
        offset += 4;
    }
    if (flags & 0x0002) {  // Bytes field is present
        if (CdxStreamSeek(mp3->stream, offset, SEEK_SET)) {
            noSeeker = 1;
            goto EXIT;
        }
        if (CdxStreamRead(mp3->stream, ucBuffer, 4) < 4) {
            noSeeker = 1;
            goto EXIT;
        }
        seeker->mSizeBytes = MP3_U32_AT(ucBuffer);
        offset += 4;
    }
    if (flags & 0x0004) {  // TOC field is present
        if (CdxStreamSeek(mp3->stream, offset, SEEK_SET)) {
            noSeeker = 1;
            goto EXIT;
        }
        if (CdxStreamRead(mp3->stream, seeker->mTOC, 99) < 99) {
            noSeeker = 1;
            goto EXIT;
        }
        seeker->mTOCValid = CDX_TRUE;
        offset += 100;
    }

    if (CdxStreamSeek(mp3->stream, xingbase + 0xb1 - 0x24, SEEK_SET)) {
         noSeeker = 1;
         goto EXIT;
    }

    if (CdxStreamRead(mp3->stream, &ucBuffer, 3) == 3) {
        seeker->mEncoderDelay = (ucBuffer[0] << 4) + (ucBuffer[1] >> 4);
        seeker->mEncoderPadding = ((ucBuffer[1] & 0xf) << 8) + ucBuffer[2];
    }

EXIT:
    if (noSeeker) {
        free(seeker);
        seeker = NULL;
    }
    return seeker;
}

cdx_bool GetXINGOffsetForTime(XINGSeeker *seeker, int64_t *timeUs, int64_t *pos) {
    if (seeker == NULL ||seeker->mSizeBytes == 0 || !seeker->mTOCValid || seeker->mDurationUs < 0)
    {
        return CDX_FALSE;
    }

    float fPercent = (float)(*timeUs) * 100 / seeker->mDurationUs;
    float fx;
    if( fPercent <= 0.0f ) {
        fx = 0.0f;
    } else if( fPercent >= 100.0f ) {
        fx = 256.0f;
    } else {
        int a = (int)fPercent;
        float fa, fb;
        if ( a == 0 ) {
            fa = 0.0f;
        } else {
            fa = (float)seeker->mTOC[a-1];
        }
        if ( a < 99 ) {
            fb = (float)seeker->mTOC[a];
        } else {
            fb = 256.0f;
        }
        fx = fa + (fb-fa)*(fPercent-a);
    }

    *pos = (int)((1.0f/256.0f)*fx*seeker->mSizeBytes) + seeker->mFirstFramePos;
    CDX_LOGV("GetXINGOffsetForTime %lld us => %08lld", *timeUs, *pos);
    return CDX_TRUE;
}

static VBRISeeker *CreateVBRISeeker(CdxParserT *parser,  cdx_int64 postId3Pos)
{
    MP3ParserImpl *mp3;
    cdx_int32     noSeeker = 0;
    VBRISeeker *seeker  = NULL;

    mp3 = (MP3ParserImpl *)parser;
    if (!mp3) {
        CDX_LOGE("Mp3 file parser lib has not been initiated!");
        return NULL;
    }

    cdx_int64 pos = postId3Pos;
    if (CdxStreamSeek(mp3->stream, pos, SEEK_SET))
         return NULL;

    uint8_t header[4];
    ssize_t n = CdxStreamRead(mp3->stream, header, sizeof(header));
    if (n < (ssize_t)sizeof(header)) {
        return NULL;
    }

    uint32_t tmp = MP3_U32_AT(&header[0]);
    size_t frameSize;
    int sampleRate;
    if (!GetMPEGAudioFrameSize(tmp, &frameSize, &sampleRate, NULL, NULL, NULL)) {
        return NULL;
    }

    // VBRI header follows 32 bytes after the header _ends_.
    pos += sizeof(header) + 32;

    uint8_t vbriHeader[26];
    if (CdxStreamSeek(mp3->stream, pos, SEEK_SET))
        return NULL;
    n = CdxStreamRead(mp3->stream, vbriHeader, sizeof(vbriHeader));
    if (n < (ssize_t)sizeof(vbriHeader)) {
        return NULL;
    }

    if (memcmp(vbriHeader, "VBRI", 4)) {
        return NULL;
    }

    size_t numFrames = MP3_U32_AT(&vbriHeader[14]);

    int64_t durationUs =
        numFrames * 1000000ll * (sampleRate >= 32000 ? 1152 : 576) / sampleRate;

    CDX_LOGV("duration = %.2f secs", durationUs / 1E6);
    size_t numEntries = MP3_U16_AT(&vbriHeader[18]);
    size_t entrySize = MP3_U16_AT(&vbriHeader[22]);
    size_t scale = MP3_U16_AT(&vbriHeader[20]);

    CDX_LOGV("%d entries, scale=%d, size_per_entry=%d",
         numEntries,
         scale,
         entrySize);

    size_t totalEntrySize = numEntries * entrySize;
    uint8_t *buffer = (uint8_t *)malloc(totalEntrySize);

    if (CdxStreamSeek(mp3->stream, pos, SEEK_SET)) {
        noSeeker = 1;
        goto EXIT;
    }
    n = CdxStreamRead(mp3->stream, buffer, totalEntrySize);
    if (n < (ssize_t)totalEntrySize) {
        noSeeker = 1;
        goto EXIT;
    }

    seeker = (VBRISeeker *)malloc(sizeof(VBRISeeker));
    memset(seeker, 0x00, sizeof(VBRISeeker));

    seeker->mBasePos = postId3Pos + frameSize;
    // only update mDurationUs if the calculated duration is valid (non zero)
    // otherwise, leave duration at -1 so that getDuration() and getOffsetForTime()
    // return false when called, to indicate that this vbri tag does not have the
    // requested information
    if (durationUs) {
        seeker->mDurationUs = durationUs;
    }

/*    seeker->mSegments = (cdx_uint32 *)malloc(numEntries);*/
/*    if (!seeker->mSegments) {*/
/*        noSeeker = 1;*/
/*        goto EXIT;*/
/*    }*/
    memset(&seeker->mSegments, 0, sizeof(seeker->mSegments));
    cdx_int64 offset = postId3Pos;
    size_t i;

    for (i = 0; i < numEntries; i++) {
        uint32_t numBytes;
        switch (entrySize)
        {
            case 1: numBytes = buffer[i]; break;
            case 2: numBytes = MP3_U16_AT(buffer + 2 * i); break;
            case 3: numBytes = MP3_U24_AT(buffer + 3 * i); break;
            default:
            {
                numBytes = MP3_U32_AT(buffer + 4 * i); break;
            }
        }

        numBytes *= scale;

        seeker->mSegments[i] = numBytes;
        seeker->mSegmentsize++;

        CDX_LOGV("entry #%d: %d offset %08lld", i, numBytes, offset);
        offset += numBytes;
    }

    CDX_LOGI("Found VBRI header.");
EXIT:
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }
    if (noSeeker) {
        if (seeker) {
/*          if (seeker->mSegments) {*/
/*              free(seeker->mSegments);*/
/*              seeker->mSegments = NULL;*/
/*          }*/
            free(seeker);
            seeker = NULL;
        }
    }
    return seeker;
}

static cdx_bool GetVBRIOffsetForTime(MP3ParserImpl *mp3,
                                     VBRISeeker *seeker,
                                     int64_t *timeUs,
                                     int64_t *pos)
{
    if (seeker == NULL || seeker->mDurationUs < 0) {
        return CDX_FALSE;
    }

    int64_t segmentDurationUs = seeker->mDurationUs / seeker->mSegmentsize;
    int64_t nowUs = 0;
    *pos = seeker->mBasePos;
    size_t segmentIndex = 0;
    while (segmentIndex < seeker->mSegmentsize && nowUs < *timeUs) {
        if(mp3->exitFlag)
        {
            return CDX_FALSE;
        }
        nowUs += segmentDurationUs;
        *pos += seeker->mSegments[segmentIndex];
    }
    CDX_LOGV("GetVBRIOffsetForTime %lld us => 0x%08lld", *timeUs, *pos);
    *timeUs = nowUs;

    return CDX_TRUE;
}

static cdx_bool Mp3HeadResync(CdxParserT *parser, cdx_uint32 matchHeader, cdx_int64 *inoutPos,
                              cdx_int64 *postId3Pos, cdx_int32 *outHead)
{
    MP3ParserImpl *mp3;
    cdx_bool   ret = CDX_FALSE;
    size_t mFrameSize;
    int mSampleRate, mNumChannels, mBitRate;

    mp3 = (MP3ParserImpl *)parser;
    if (!mp3) {
        CDX_LOGE("Mp3 file parser lib has not been initiated!");
        return ret;
    }
#if 0
// Id3 detection has totally done by Cdxid3v2 parser
// Any question -> chengkan@allwinnertech.com
    if (*inoutPos == 0) {
        for (;;) {
            uint8_t id3header[10];
            if(mp3->exitFlag)
            {
                mp3->mStatus = MP3_STA_IDLE;
                return CDX_FALSE;
            }
            if (CdxStreamRead(mp3->stream, id3header, sizeof(id3header))
                        < (cdx_int32)sizeof(id3header)) {
                 return ret;
                }

            if (memcmp("ID3", id3header, 3)) {
                break;
            }

           size_t len =
                ((id3header[6] & 0x7f) << 21)
                | ((id3header[7] & 0x7f) << 14)
                | ((id3header[8] & 0x7f) << 7)
                | (id3header[9] & 0x7f);

            len += 10;
            *inoutPos += len;
            if (postId3Pos != NULL) {
                *postId3Pos = *inoutPos;
            }
        }
    }
#endif
    cdx_int64 pos = *inoutPos;
    if (CdxStreamSeek(mp3->stream, pos, SEEK_SET))
         return ret;

    CDX_LOGV("pos %lld", pos);
    cdx_bool valid = CDX_FALSE;
    cdx_uint8 buf[kMaxReadBytes];
    int bytesToRead = kMaxReadBytes;
    int totalBytesRead = 0;
    int remainingBytes = 0;
    cdx_bool reachEOS = CDX_FALSE;
    cdx_uint8 *tmp = buf;

    do {
        if(mp3->exitFlag)
        {
            mp3->mStatus = MP3_STA_IDLE;
            return CDX_FALSE;
        }
        if (pos >= *inoutPos + kMaxBytesChecked) {
            // Don't scan forever.
            CDX_LOGV("giving up at offset %lld", pos);
            break;
        }

        if (remainingBytes < 4) {
            if (reachEOS) {
                break;
            } else {
                memcpy(buf, tmp, remainingBytes);
                bytesToRead = kMaxReadBytes - remainingBytes;

                if (CdxStreamSeek(mp3->stream, pos + remainingBytes, SEEK_SET))
                    return ret;

                totalBytesRead = CdxStreamRead(mp3->stream,
                                                buf + remainingBytes,
                                                bytesToRead);
                if (totalBytesRead <= 0) {
                    break;
                }
                reachEOS = (totalBytesRead != bytesToRead);
                totalBytesRead += remainingBytes;
                remainingBytes = totalBytesRead;
                tmp = buf;
                continue;
            }
        }
        cdx_uint32 header = MP3_U32_AT(tmp);

        if (matchHeader != 0 && (header & kMask) != (matchHeader & kMask)) {
            ++pos;
            ++tmp;
            --remainingBytes;
            continue;
        }
        if (!GetMPEGAudioFrameSize(
                    header, &mFrameSize,
                    &mSampleRate, &mNumChannels, &mBitRate, NULL)) {
            ++pos;
            ++tmp;
            --remainingBytes;
            continue;
        }

        CDX_LOGD("found possible 1st frame at %lld (header = 0x%08x) mFrameSize %d, mBitRate : %d",
                  pos, header, mFrameSize, mBitRate);

        // We found what looks like a valid frame,
        // now find its successors.
        cdx_int64 test_pos = pos + mFrameSize;

        valid = CDX_TRUE;
        int j;
        int avgbitrate = mBitRate;
        for (j = 0; j < SYNCFRMNUM - 1; ++j) {
            uint8_t tmp[4];
            //Read at test_pos for 4 bytes to tmp buffer
            if (CdxStreamSeek(mp3->stream, test_pos, SEEK_SET)) {
                if(j < MIN_MP3_VALID_FRAME_COMBO)
                {
                    valid = CDX_FALSE;
                }
                break;
            }

            if (CdxStreamRead(mp3->stream, tmp, 4) < 4) {
                if(j < MIN_MP3_VALID_FRAME_COMBO)
                {
                    valid = CDX_FALSE;
                }
                break;
            }

            uint32_t test_header = MP3_U32_AT(tmp);

            CDX_LOGV("subsequent header is %08x", test_header);

            if ((test_header & kMask) != (header & kMask)) {
                if(j < MIN_MP3_VALID_FRAME_COMBO)
                {
                    valid = CDX_FALSE;
                }
                break;
            }

            size_t test_frame_size;
            int testSampleRate, testNumChannels, testBitRate;
            if (!GetMPEGAudioFrameSize(
                        test_header, &test_frame_size, &testSampleRate,
                        &testNumChannels, &testBitRate, NULL)) {
                if(j < MIN_MP3_VALID_FRAME_COMBO)
                {
                    valid = CDX_FALSE;
                }
                break;
            }

            CDX_LOGV("found subsequent frame #%d at %lld", j + 2, test_pos);
            CDX_LOGD("subsequent test_frame_size(%d), SR(%d), CH(%d), BITR(%d)",
                    test_frame_size, testSampleRate, testNumChannels, testBitRate);
            test_pos += test_frame_size;
            avgbitrate += testBitRate;
        }
        CDX_LOGD("==== Run for %d frame to calc avgbitrate ===", j+1);
        avgbitrate /= (j+1);//SYNCFRMNUM;
        if (valid) {
            *inoutPos = pos;
            mp3->mavgBitRate = avgbitrate;
            if (outHead != NULL) {
                *outHead = header;
            }
        } else {
            CDX_LOGV("no dice, no valid sequence of frames found.");
        }

        ++pos;
        ++tmp;
        --remainingBytes;

    }while (!valid);
    if(valid)
    {
        mp3->mFrameSize  = mFrameSize;
        mp3->mSampleRate = mSampleRate;
        mp3->mChannels   = mNumChannels;
        if(mp3->mavgBitRate == 0)
            mp3->mBitRate = mBitRate * 1E3;
        else
            mp3->mBitRate = mp3->mavgBitRate * 1E3;
    }

    return valid;
}

static int Mp3GetCacheState(MP3ParserImpl *impl, struct ParserCacheStateS *cacheState)
{
    struct StreamCacheStateS streamCS;
    if (CdxStreamControl(impl->stream, STREAM_CMD_GET_CACHESTATE, &streamCS) < 0)
    {
        CDX_LOGE("STREAM_CMD_GET_CACHESTATE fail");
        return -1;
    }

    cacheState->nCacheCapacity = streamCS.nCacheCapacity;
    cacheState->nCacheSize = streamCS.nCacheSize;
    cacheState->nBandwidthKbps = streamCS.nBandwidthKbps;
    cacheState->nPercentage = streamCS.nPercentage;

    CDX_LOGV("Mp3GetCacheState nCacheCapacity[%d] nCacheSize[%d] nBandwidthKbps[%d]\
              nPercentage[%d]",
              cacheState->nCacheCapacity,cacheState->nCacheSize,cacheState->nBandwidthKbps,
              cacheState->nPercentage);

    return 0;
}

static int CdxMp3Init(CdxParserT* Parameter)
{
    MP3ParserImpl *mp3;
    CdxParserT *parser;
    cdx_int32  testlen = 0;

    mp3 = (MP3ParserImpl *)Parameter;
    parser = (CdxParserT *)Parameter;

    mp3->mFileSize = CdxStreamSize(mp3->stream);
    mp3->mStatus = MP3_STA_INITING;
    cdx_bool sucess;
    cdx_int64 Pos = 0;
    cdx_int32 Header = 0;
    cdx_int64 postId3Pos = 0;
    if(!CdxStreamIsNetStream(mp3->stream))//do not parse v1 tag in network stream...
    {
        Pos = CdxStreamTell(mp3->stream);
        CDX_LOGD("Mp3 start Pos : %lld", Pos);
        mp3->id3v1 = GenerateId3(mp3->stream, NULL, 0, kfalse);
        CdxStreamSeek(mp3->stream, Pos, SEEK_SET);
    }
    sucess = Mp3HeadResync(parser, 0, &Pos, &postId3Pos, &Header);
    CDX_LOGD("Notice!!  testlen:%d,postId3Pos:%lld",testlen,postId3Pos);
    if (!sucess) {
        mp3->mErrno = PSR_OPEN_FAIL;
        pthread_cond_signal(&mp3->cond);
        goto Exit;
    }

    mp3->mFirstFramePos  = Pos;
    mp3->mFixedHeader   = Header;

    mp3->mXINGSeeker = CreateXINGSeeker(parser, mp3->mFirstFramePos);
    if (mp3->mXINGSeeker == NULL) {
        mp3->mVBRISeeker = CreateVBRISeeker(parser, postId3Pos);
    }
    CDX_LOGV("mXINGSeeker %p, mVBRISeeker %p", mp3->mXINGSeeker, mp3->mVBRISeeker);

    if (mp3->mXINGSeeker != NULL)
    {
        mp3->mDuration = mp3->mXINGSeeker->mDurationUs / 1E3;
    } else if (mp3->mVBRISeeker != NULL) {
        mp3->mDuration = mp3->mVBRISeeker->mDurationUs / 1E3;
    } else if (mp3->mFileSize > 0 && mp3->mBitRate != 0) {
        mp3->mDuration = 8000LL * (mp3->mFileSize - mp3->mFirstFramePos) / mp3->mBitRate;
    } else {
        mp3->mDuration = -1;
        CDX_LOGW("Duration can not calculate");
    }
    CdxStreamSeek(mp3->stream,mp3->mFirstFramePos,SEEK_SET);
    mp3->mErrno = PSR_OK;
    pthread_cond_signal(&mp3->cond);
    mp3->mStatus = MP3_STA_IDLE;
    return 0;
Exit:
    mp3->mStatus = MP3_STA_IDLE;
    return -1;
}

static int CdxMp3ParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    MP3ParserImpl *mp3;
    int ret = 0;
    AudioStreamInfo *audio = NULL;

    mp3 = (MP3ParserImpl *)parser;
    mediaInfo->fileSize     = CdxStreamSize(mp3->stream);
    audio                   = &mediaInfo->program[0].audio[0];
    audio->eCodecFormat     = AUDIO_CODEC_FORMAT_MP3;
    audio->nChannelNum      = mp3->mChannels;
    audio->nSampleRate      = mp3->mSampleRate;
    audio->nAvgBitrate      = mp3->mBitRate;
    audio->nMaxBitRate      = mp3->mBitRate;

    mediaInfo->program[0].audioNum++;
    mediaInfo->program[0].duration  = mp3->mDuration;
    mediaInfo->bSeekable = CdxStreamSeekAble(mp3->stream);
    /*for the request from ericwang, for */
    mediaInfo->programNum = 1;
    mediaInfo->programIndex = 0;
    /**/
    if(mp3->id3v1 && mp3->id3v1->mIsValid
        && !mediaInfo->id3v2HadParsed)
    {
        CDX_LOGD("mp3 version 1...");
        Id3BaseGetMetaData(mediaInfo, mp3->id3v1);
    }
    else
        CDX_LOGD("No id3 version 1 or id3 version 2 has been parsed, return none...");
    return ret;
}

static int CdxMp3ParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    MP3ParserImpl *mp3;

    mp3 = (MP3ParserImpl *)parser;
    switch (cmd) {
        case CDX_PSR_CMD_DISABLE_AUDIO:
        case CDX_PSR_CMD_DISABLE_VIDEO:
        case CDX_PSR_CMD_SWITCH_AUDIO:
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
        {
            mp3->mErrno = PSR_USER_CANCEL;
            mp3->exitFlag = 1;
            CdxStreamForceStop(mp3->stream);

            while(mp3->mStatus != MP3_STA_IDLE)
            {
                usleep(2000);
            }

            break;
        }
        case CDX_PSR_CMD_CLR_FORCESTOP:
            mp3->exitFlag = 0;
            CdxStreamClrForceStop(mp3->stream);
            break;
        case CDX_PSR_CMD_GET_CACHESTATE:
        {
            return Mp3GetCacheState(mp3, param);
        }
        default:
            CDX_LOGW("not implement...(%d)", cmd);
            break;
    }

   return 0;
}

static int CdxMp3ParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    MP3ParserImpl *mp3;

    mp3 = (MP3ParserImpl *)parser;

    cdx_int64 nReadPos = CdxStreamTell(mp3->stream);
    if (nReadPos >= mp3->mFileSize && mp3->mFileSize > 0) {
        CDX_LOGD("Flie EOS");
        mp3->mErrno = PSR_EOS;
        return -1;
    }

    pkt->type = CDX_MEDIA_AUDIO;
    pkt->length = MP3_PACKET_SIZE;
    pkt->flags |= (FIRST_PART|LAST_PART);

    //if (1)//mp3->mSeeking)
    {
        pkt->pts = mp3->mSeekingTime;
        mp3->mSeeking = 0;
    }

       //CDX_LOGV("pkt->pts %lld", pkt->pts);
    return 0;
}

static int CdxMp3ParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    MP3ParserImpl *mp3;
    int ret = CDX_SUCCESS;
    cdx_int64 nReadPos = 0;
    int nReadSize = MP3_PACKET_SIZE;
    int nRetSize = 0;

    mp3 = (MP3ParserImpl *)parser;
    if (!mp3) {
        CDX_LOGE("Mp3 file parser prefetch failed!");
        ret = -1;
        goto Exit;
    }

    mp3->mStatus = MP3_STA_READING;
    nReadPos = CdxStreamTell(mp3->stream);
#if 0
    if (nReadPos < mp3->mFileSize) {
        nReadSize = FFMIN(nReadSize, mp3->mFileSize - nReadPos);
    }
#endif

    nRetSize= CdxStreamRead(mp3->stream, pkt->buf, nReadSize);
    if(nRetSize < 0)
    {
        CDX_LOGE("File Read Fail");
        mp3->mErrno = PSR_IO_ERR;
        ret = -1;
        goto Exit;
    }
    else if(nRetSize == 0)
    {
       CDX_LOGD("Flie Read EOS");
       mp3->mErrno = PSR_EOS;
       ret = -1;
       goto Exit;
    }
    pkt->length = nRetSize;
    mp3->readPacketSize += nRetSize;
Exit:
    mp3->mStatus = MP3_STA_IDLE;
    return ret;
}

static int CdxMp3ParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    MP3ParserImpl *mp3;
    int         ret = 0;
    //cdx_int64   mSeekTime = timeUs;
    cdx_bool    seekCBR = CDX_FALSE;
    cdx_int64   mSamplesRead = 0;
    cdx_int64   mCurrentTimeUs = 0;
    cdx_int64   mBasisTimeUs   = 0;
    mp3 = (MP3ParserImpl *)parser;

    if (!mp3) {
        CDX_LOGE("Mp3 file parser seekto failed!");
        ret = -1;
        goto Exit;
    }

    mp3->mStatus = MP3_STA_SEEKING;
    int64_t actualSeekTimeUs = timeUs;
    if ((mp3->mXINGSeeker == NULL ||
       !GetXINGOffsetForTime(mp3->mXINGSeeker, &actualSeekTimeUs, &mp3->mCurrentPos))
        && (mp3->mVBRISeeker == NULL ||
           !GetVBRIOffsetForTime(mp3, mp3->mVBRISeeker, &actualSeekTimeUs,&mp3->mCurrentPos))) {
        if (!mp3->mBitRate) {
            CDX_LOGW("no bitrate");
            ret = -1;
            goto Exit;
        }
        seekCBR = CDX_TRUE;
        mCurrentTimeUs = timeUs;
        mp3->mCurrentPos = mp3->mFirstFramePos + timeUs * mp3->mBitRate / 8000000;
    } else {
        mCurrentTimeUs = actualSeekTimeUs;
    }

    mBasisTimeUs = mCurrentTimeUs;
    CDX_LOGV("mCurrentPos %lld, mBitRate %d, mCurrentTimeUs %lld",
              mp3->mCurrentPos, mp3->mBitRate, mCurrentTimeUs);

    if (mp3->mCurrentPos > mp3->mFileSize && mp3->mFileSize > 0) {
        //return -1;
        mp3->mCurrentPos = mp3->mFileSize;
        CdxStreamSeek(mp3->stream, mp3->mCurrentPos, SEEK_SET);
        mp3->mSeeking   = 1;
        mp3->mSeekingTime = mCurrentTimeUs;
        goto Exit;
    }

    size_t mFrameSize;
    int mBitRate;
    int mNumSamples;
    int mSampleRate;
    uint8_t buffer[4] = {0};
    for (;;) {
        if (CdxStreamSeek(mp3->stream, mp3->mCurrentPos, SEEK_SET)) {
            ret = -1;
            goto Exit;
        }
        ssize_t n = CdxStreamRead(mp3->stream, buffer, 4);
        if (n < 4) {
            ret = 0;
            goto Exit;
            //return -1;
        }

        uint32_t Header = MP3_U32_AT((const uint8_t *)buffer);

        if ((Header & kMask) == (mp3->mFixedHeader & kMask)
            && GetMPEGAudioFrameSize(
                Header, &mFrameSize, &mSampleRate, NULL,
                &mBitRate, &mNumSamples)) {

            // re-calculate mCurrentTimeUs because we might have called Resync()
            if (seekCBR)
            {
                mCurrentTimeUs = (mp3->mCurrentPos - mp3->mFirstFramePos) * 8000 * 1000 /
                                  mp3->mBitRate;
                mBasisTimeUs = mCurrentTimeUs;
                CDX_LOGV("seekCBR mCurrentTimeUs %lld", mCurrentTimeUs);
            }
            break;
        }

        // Lost sync.
        CDX_LOGV("lost sync! header = 0x%08x, old header = 0x%08x\n", Header, mp3->mFixedHeader);

        cdx_int64 Pos = mp3->mCurrentPos;
        if (!Mp3HeadResync(parser, mp3->mFixedHeader, &Pos, NULL, NULL)) {
            CDX_LOGW("Unable to resync. Signalling end of stream.");
            ret = 0;
            goto Exit;
        }
        mp3->mCurrentPos = Pos;
    }

    if (CdxStreamSeek(mp3->stream, mp3->mCurrentPos, SEEK_SET)) {
        ret = -1;
        goto Exit;
    }

    mSamplesRead += mNumSamples;
    mCurrentTimeUs = mBasisTimeUs + ((mSamplesRead * 1000000) / mp3->mSampleRate);

    mp3->mSeeking   = 1;
    mp3->mSeekingTime = mCurrentTimeUs;
    mp3->mCurrentPos += mFrameSize;
    CDX_LOGV("mSeekingTime %lld", mp3->mSeekingTime);
Exit:
    mp3->mStatus = MP3_STA_IDLE;
    return ret;
}

static cdx_uint32 CdxMp3ParserAttribute(CdxParserT *parser)
{
    MP3ParserImpl *mp3;
    int ret = 0;

    mp3 = (MP3ParserImpl *)parser;
    if (!mp3) {
        CDX_LOGE("Mp3 file parser Attribute failed!");
        ret = -1;
        goto Exit;
    }
Exit:
    return ret;
}

#if 0
static int CdxMp3ParserForceStop()
{
    return 0;
}
#endif
static int CdxMp3ParserGetStatus(CdxParserT *parser)
{
    MP3ParserImpl *mp3;
    mp3 = (MP3ParserImpl *)parser;
#if 0
    if (CdxStreamEos(mp3->stream)) {
        CDX_LOGE("File EOS! ");
        return mp3->mErrno = PSR_EOS;
    }
#endif
    return mp3->mErrno;
}

static int CdxMp3ParserClose(CdxParserT *parser)
{
    MP3ParserImpl *mp3;
    mp3 = (MP3ParserImpl *)parser;
    if (!mp3) {
        CDX_LOGE("Mp3 file parser prefetch failed!");
        return -1;
    }
    #if 0
    if(mp3->mInforBuftemp)
    {
       free(mp3->mInforBuftemp);
    }
    #endif
/*    if (mp3->mVBRISeeker && mp3->mVBRISeeker->mSegments) {*/
/*        free(mp3->mVBRISeeker->mSegments);*/
/*        mp3->mVBRISeeker->mSegments = NULL;*/
/*    }*/
    if (mp3->mVBRISeeker) {
        free(mp3->mVBRISeeker);
        mp3->mVBRISeeker = NULL;
    }

    if (mp3->mXINGSeeker) {
        free(mp3->mXINGSeeker);
        mp3->mXINGSeeker = NULL;
    }
    #if 0
    if(mp3->pAlbumArtBuf)
    {
        free(mp3->pAlbumArtBuf);
        mp3->pAlbumArtBuf = NULL;
    }
    #endif
#if ENABLE_FILE_DEBUG
    if (mp3->teeFd) {
        close(mp3->teeFd);
    }
#endif
    CDX_LOGD("Mp3 Parser Close...");
    EraseId3(&mp3->id3v1);
    if(mp3->stream) {
        CdxStreamClose(mp3->stream);
        mp3->stream = NULL;
    }
    pthread_cond_destroy(&mp3->cond);
    CdxFree(mp3);
    return 0;
}

static struct CdxParserOpsS Mp3ParserImpl =
{
    .control      = CdxMp3ParserControl,
    .prefetch     = CdxMp3ParserPrefetch,
    .read         = CdxMp3ParserRead,
    .getMediaInfo = CdxMp3ParserGetMediaInfo,
    .seekTo       = CdxMp3ParserSeekTo,
    .attribute    = CdxMp3ParserAttribute,
    .getStatus    = CdxMp3ParserGetStatus,
    .close        = CdxMp3ParserClose,
    .init         = CdxMp3Init
};

CdxParserT *CdxMp3ParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    MP3ParserImpl *Mp3ParserImple;
    //int ret = 0;
    if(flags > 0) {
        CDX_LOGI("Flag Not Zero");
    }
    Mp3ParserImple = (MP3ParserImpl *)malloc(sizeof(MP3ParserImpl));
    if (Mp3ParserImple == NULL) {
        CDX_LOGE("Mp3ParserOpen Failed");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(Mp3ParserImple, 0, sizeof(MP3ParserImpl));

    Mp3ParserImple->stream = stream;
    Mp3ParserImple->base.ops = &Mp3ParserImpl;
    Mp3ParserImple->mErrno = PSR_INVALID;
    pthread_cond_init(&Mp3ParserImple->cond, NULL);
    //ret = pthread_create(&Mp3ParserImple->openTid, NULL, Mp3OpenThread, (void*)Mp3ParserImple);
    //CDX_FORCE_CHECK(!ret);

#if ENABLE_FILE_DEBUG
    char teePath[64];
    strcpy(teePath, "/data/camera/mp3.dat");
    Mp3ParserImple->teeFd = open(teePath, O_WRONLY | O_CREAT | O_EXCL, 0775);
#endif

    return &Mp3ParserImple->base;
}

static cdx_uint32 Mp3Probe(CdxStreamProbeDataT *p)
{
    int score = 0;
    if(p != NULL && p->len >= 8 && p->buf != NULL && !memcmp(p->buf, "DTSHDHDR", 8))
    {
        CDX_LOGE("Mp3 occurs DTS-HD...");
        return 0;
    }
    score = (int)(Resync(p)*100);
    CDX_LOGD("Mp3 score : %d", score);
    return score;
    /*
    int ret = 0;

    if (((ret = CdxMp3Probe(p)) >= AVPROBE_SCORE_MAX / 4 - 1)|| Resync(p))
        return CDX_TRUE;

    return CDX_FALSE;
    */
}

static cdx_uint32 CdxMp3ParserProbe(CdxStreamProbeDataT *probeData)
{
    if (probeData->len < 2/*MP3_PROBE_SIZE*/) {
        CDX_LOGE("Mp3 Probe Failed");
        return 0;
    }

    return Mp3Probe(probeData);
}

CdxParserCreatorT mp3ParserCtor =
{
    .create = CdxMp3ParserOpen,
    .probe = CdxMp3ParserProbe
};
