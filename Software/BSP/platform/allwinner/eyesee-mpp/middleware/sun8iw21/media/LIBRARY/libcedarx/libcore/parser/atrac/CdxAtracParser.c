/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxAtracParser.c
* Description :
* History :
*   Author  : lszhang <lszhang@allwinnertech.com>
*   Date    : 2014/08/08
*   Comment : 创建初始版本，实现 atrc 解复用功能
*/

#define LOG_TAG "CdxAtracParser"
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <string.h>

#include "CdxAtracParser.h"
#include <fcntl.h>

#ifndef ATRAC_RB24
#   define ATRAC_RB24(c)                           \
    ((((const uint8_t*)(c))[0] << 16) |         \
     (((const uint8_t*)(c))[1] <<  8) |         \
      ((const uint8_t*)(c))[2])
#endif

#ifndef ATRAC_RB16
#   define ATRAC_RB16(c)                           \
    ((((const uint8_t*)(c))[0] << 8) |          \
      ((const uint8_t*)(c))[1])
#endif

int AtracId3v2Match(const cdx_uint8 *pBuf, const char *pMagic)
{
    return  pMagic[0]        == pBuf[0] &&
            pMagic[1]        == pBuf[1] &&
            pMagic[2]        == pBuf[2] &&
            0xff         != pBuf[3]     &&
            0xff         != pBuf[4]     &&
            0 == (pBuf[6] & 0x80)       &&
            0 == (pBuf[7] & 0x80)       &&
            0 == (pBuf[8] & 0x80)       &&
            0 == (pBuf[9] & 0x80);
}

int AtracId3v2TagLen(const cdx_uint8 *pBuf)
{
    int len = ((pBuf[6] & 0x7f) << 21) + ((pBuf[7] & 0x7f) << 14) +
              ((pBuf[8] & 0x7f) << 7) + (pBuf[9] & 0x7f) + ID3v2_HEADER_SIZE;

    if (pBuf[5] & 0x10)
        len += ID3v2_HEADER_SIZE;

    return len;
}

static int AtracDumpInfo(void *Parameter)
{
#if ENABLE_INFO_DEBUG
    ATRACParserImpl *atrac;
    atrac = (ATRACParserImpl *)Parameter;

    CDX_LOGV("SampleRate %d", atrac->mSampleRate);
    CDX_LOGV("Channels %d", atrac->mChannels);
    CDX_LOGV("BitRate %d", atrac->mBitrate);
    CDX_LOGV("mFrameSize %d", atrac->mFrameSize);
    CDX_LOGV("mDuration %lld", atrac->mDuration);
    CDX_LOGV("mFileSize %lld", atrac->mFileSize);
#endif
    return 0;
}

static int AtracInit(CdxParserT* Parameter)
{
    ATRACParserImpl *atrac;
    cdx_int64 nEa3TagLen, nEa3Pos;
    short eId;
    int nCoderTag, nChannelId, nCodecParams, nSampleRate;
    unsigned char Ea3[] = {'e', 'a', '3', 3, 0};
    unsigned char Ea3Tag[3] ={'E', 'A', '3'};
    unsigned char nBuf[EA3_HEADER_SIZE] = {0};
    unsigned short nSrateTab[6] = {320, 441, 480, 882, 960, 0};
    const int nChidToNumChannels[7] = {1, 2, 3, 4, 6, 7, 8};

    atrac = (ATRACParserImpl *)Parameter;
    atrac->mFileSize = CdxStreamSize(atrac->stream);
    if(atrac->mFileSize <= 0)
    {
        atrac->mErrno = PSR_OPEN_FAIL;
        goto Exit;
    }

    if(CdxStreamRead(atrac->stream, nBuf, ID3v2_HEADER_SIZE) < ID3v2_HEADER_SIZE)
    {
        atrac->mErrno = PSR_OPEN_FAIL;
        goto Exit;
    }
    if(memcmp(nBuf, Ea3, 5))
    {
        atrac->mErrno = PSR_OPEN_FAIL;
        goto Exit;
    }

    nEa3TagLen = ((nBuf[6] & 0x7f) << 21) | ((nBuf[7] & 0x7f) << 14) | \
        ((nBuf[8] & 0x7f) << 7) | (nBuf[9] & 0x7f);
    nEa3Pos = nEa3TagLen + 10;
    if(nBuf[5] & 0x10)
        nEa3Pos += 10;

    if(CdxStreamSkip(atrac->stream, nEa3Pos - 10))
    {
        atrac->mErrno = PSR_OPEN_FAIL;
        goto Exit;
    }
    if(CdxStreamRead(atrac->stream, nBuf, EA3_HEADER_SIZE) < EA3_HEADER_SIZE)
    {
        atrac->mErrno = PSR_OPEN_FAIL;
        goto Exit;
    }

    if(memcmp(nBuf, Ea3Tag, 3) || nBuf[4] != 0 || nBuf[5] != EA3_HEADER_SIZE)
    {
        atrac->mErrno = PSR_OPEN_FAIL;
        goto Exit;
    }

    eId = ATRAC_RB16(&nBuf[6]);
    if(eId != -1 && eId != -128)
    {
        atrac->mErrno = PSR_OPEN_FAIL;
        goto Exit;
    }

    nCoderTag = nBuf[32];
    nCodecParams = ATRAC_RB24(&nBuf[33]);
    atrac->mHeadSize = nEa3Pos + EA3_HEADER_SIZE;
    atrac->mSampleRate = nSampleRate = nSrateTab[(nCodecParams >> 13) & 7] * 100;
    switch(nCoderTag)
    {
        case OMA_CODECID_ATRAC3:
            atrac->mFrameSize = (nCodecParams & 0x3FF) * 8;
            atrac->mChannels = nChannelId = 2;
            break;
        case OMA_CODECID_ATRAC3P:
            atrac->mFrameSize = ((nCodecParams & 0x3FF) * 8) + 8;
            nChannelId = (nCodecParams >> 10) & 7;
            atrac->mChannels = nChidToNumChannels[nChannelId - 1];
            break;
        default:
            atrac->mErrno = PSR_OPEN_FAIL;
            goto Exit;
    }
    atrac->mBitrate = atrac->mSampleRate * atrac->mFrameSize * 8 / (1024 * (nCoderTag + 1));
    atrac->mDuration = (atrac->mFileSize - nEa3Pos - EA3_HEADER_SIZE) * 8000LL / atrac->mBitrate;

    AtracDumpInfo((void *)atrac);
    atrac->mErrno = PSR_OK;
    atrac->mFirstFrame = CDX_TRUE;
    return 0;
Exit:
    return -1;
}

static int CdxAtracParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    ATRACParserImpl *atrac;
    int ret = 0;
    AudioStreamInfo *audio = NULL;

    atrac = (ATRACParserImpl *)parser;
    if(!atrac)
    {
        CDX_LOGE("atrac file parser lib has not been initiated!");
        ret = -1;
        goto Exit;
    }

    mediaInfo->fileSize = CdxStreamSize(atrac->stream);
    if(CdxStreamSeekAble(atrac->stream))
        mediaInfo->bSeekable = CDX_TRUE;
    else
        CDX_LOGW("atrac file Unable To Seek");

    audio = &mediaInfo->program[0].audio[0];
    audio->eCodecFormat = AUDIO_CODEC_FORMAT_ATRC;
    audio->nChannelNum = atrac->mChannels;
    audio->nSampleRate = atrac->mSampleRate;
    audio->nAvgBitrate = atrac->mBitrate;
    audio->nMaxBitRate = atrac->mBitrate;
    mediaInfo->program[0].audioNum++;
    mediaInfo->program[0].duration = atrac->mDuration;
    /*for the request from ericwang, for */
    mediaInfo->programNum = 1;
    mediaInfo->programIndex = 0;
    /**/

Exit:
    return ret;
}

static int CdxAtracParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    ATRACParserImpl *atrac;
    (void *)param;

    atrac = (ATRACParserImpl *)parser;
    if(!atrac)
    {
        CDX_LOGE("Atrac file parser Control failed!");
        return -1;
    }

    switch(cmd)
    {
        case CDX_PSR_CMD_DISABLE_AUDIO:
        case CDX_PSR_CMD_DISABLE_VIDEO:
        case CDX_PSR_CMD_SWITCH_AUDIO:
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            CdxStreamForceStop(atrac->stream);
            break;
        case CDX_PSR_CMD_CLR_FORCESTOP:
            CdxStreamClrForceStop(atrac->stream);
            break;
        default:
            CDX_LOGW("not implement...(%d)", cmd);
            break;
    }
    return 0;
}

static int CdxAtracParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    ATRACParserImpl *atrac;
    atrac = (ATRACParserImpl *)parser;
    if(!atrac)
    {
        CDX_LOGE("Atrac file parser prefetch failed!");
        return -1;
    }

    pkt->type = CDX_MEDIA_AUDIO;
    pkt->length = atrac->mFrameSize;
    pkt->pts = atrac->mSeekTime;
    pkt->flags |= (FIRST_PART|LAST_PART);

    if(atrac->mSeeking)
    {
        atrac->mSeeking = CDX_FALSE;
    }

    //CDX_LOGV("pkt length %d, pkt pts %lld", pkt->length, pkt->pts);
    return 0;
}

static int CdxAtracParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    ATRACParserImpl *atrac;
    int ret = 0;
    cdx_int64 nReadPos = 0;
    int nReadSize = 0;
    int nRetSize  = 0;

    atrac = (ATRACParserImpl *)parser;
    if(!atrac)
    {
        CDX_LOGE("Atrac file parser Read failed!");
        ret = -1;
        goto Exit;
    }
    if(atrac->mFirstFrame)
    {
        if(CdxStreamSeek(atrac->stream, 0, SEEK_SET))
        {
            ret = -1;
            goto Exit;
        }
        atrac->mFirstFrame = CDX_FALSE;
    }

    nReadSize = atrac->mFrameSize;
    nReadPos = CdxStreamTell(atrac->stream);
    if(nReadPos < atrac->mFileSize)
    {
        nReadSize = FFMIN(nReadSize, atrac->mFileSize - nReadPos);
    }

    nRetSize= CdxStreamRead(atrac->stream, pkt->buf, nReadSize);
    if(nRetSize < 0)
    {
        CDX_LOGE("File Read Fail");
        atrac->mErrno = PSR_IO_ERR;
        ret = -1;
        goto Exit;
    }
    else if(nRetSize == 0)
    {
        CDX_LOGV("Flie Read EOS");
        atrac->mErrno = PSR_EOS;
        ret = -1;
        goto Exit;
    }
Exit:
#if ENABLE_FILE_DEBUG
    CDX_LOGV("nReadPos %lld, nRetSize %d", nReadPos, nRetSize);
    if(atrac->teeFd >= 0)
    {
        write(atrac->teeFd, pkt->buf, nRetSize);
    }
#endif
    return ret;
}

static int CdxAtracParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    ATRACParserImpl *atrac;
    int ret = 0;
    int nSeekBlock = 0;
    int64_t nSeekPos = 0;
    atrac = (ATRACParserImpl *)parser;
    if(!atrac)
    {
        CDX_LOGE("Atrac file parser seekto failed!");
        ret = -1;
        goto Exit;
    }
    if(timeUs >= 0 && timeUs <= atrac->mDuration * 1000)
    {
        nSeekBlock = timeUs * atrac->mSampleRate / 1024 / 1E6;
    }
    else if(timeUs > atrac->mDuration * 1000)
    {
        timeUs = atrac->mDuration * 1000;
        nSeekBlock = timeUs * atrac->mSampleRate / 1024 / 1E6;
    }
    else
    {
        CDX_LOGW("SeekTime Negative");
        ret = -1;
        goto Exit;
    }

    atrac->mSeekTime = nSeekBlock * 1024 * 1E6 / atrac->mSampleRate ;
    nSeekPos = (int64_t)nSeekBlock * atrac->mFrameSize + atrac->mHeadSize;
    CDX_LOGV("SeekTime %lld, nSeekPos %lld", atrac->mSeekTime, nSeekPos);
    if(CdxStreamSeek(atrac->stream, nSeekPos, SEEK_SET))
    {
        CDX_LOGE("Atrac seekto failed!");
        ret = -1;
        goto Exit;
    }
    atrac->mSeeking = CDX_TRUE;
Exit:
    return ret;
}

static cdx_uint32 CdxAtracParserAttribute(CdxParserT *parser)
{
    ATRACParserImpl *atrac;
    int ret = 0;

    atrac = (ATRACParserImpl *)parser;
    if(!atrac)
    {
        CDX_LOGE("Atrac file parser Attribute failed!");
        ret = -1;
        goto Exit;
    }
Exit:
    return ret;
}


static int CdxAtracParserGetStatus(CdxParserT *parser)
{
    ATRACParserImpl *atrac;

    atrac = (ATRACParserImpl *)parser;
#if 0
    if(CdxStreamEos(atrac->stream))
    {
        CDX_LOGE("File EOS! ");
        return atrac->mErrno = PSR_EOS;
    }
#endif
    return atrac->mErrno;
}

static int CdxAtracParserClose(CdxParserT *parser)
{
    ATRACParserImpl *atrac;
    int ret = 0;
    atrac = (ATRACParserImpl *)parser;
    if(!atrac)
    {
        CDX_LOGE("Atrac file parser Close failed!");
        ret = -1;
        goto Exit;
    }
    //pthread_join(atrac->openTid, NULL);
    pthread_cond_destroy(&atrac->cond);
#if ENABLE_FILE_DEBUG
    if(atrac->teeFd)
    {
        close(atrac->teeFd);
    }
#endif

Exit:
    return ret;
}

static struct CdxParserOpsS AtracParserImpl =
{
    .control      = CdxAtracParserControl,
    .prefetch     = CdxAtracParserPrefetch,
    .read         = CdxAtracParserRead,
    .getMediaInfo = CdxAtracParserGetMediaInfo,
    .seekTo       = CdxAtracParserSeekTo,
    .attribute    = CdxAtracParserAttribute,
    .getStatus    = CdxAtracParserGetStatus,
    .close        = CdxAtracParserClose,
    .init         = AtracInit
};

CdxParserT *CdxAtracParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    ATRACParserImpl *AtracParserImple;
    int ret = 0;
    if(flags > 0)
    {
        CDX_LOGI("Flag Not Zero");
    }
    AtracParserImple = (ATRACParserImpl *)malloc(sizeof(ATRACParserImpl));
    if(AtracParserImple == NULL)
    {
        CDX_LOGE("AtracParserOpen Failed");
        return NULL;
    }
    memset(AtracParserImple, 0, sizeof(ATRACParserImpl));

    AtracParserImple->stream = stream;
    AtracParserImple->base.ops = &AtracParserImpl;
    AtracParserImple->mErrno = PSR_INVALID;
    pthread_cond_init(&AtracParserImple->cond, NULL);
#if ENABLE_FILE_DEBUG
    char teePath[64];
    strcpy(teePath, "/data/camera/atrac.dat");
    AtracParserImple->teeFd = open(teePath, O_WRONLY | O_CREAT | O_EXCL, 0775);
#endif

    return &AtracParserImple->base;
}

static int AtracProbe(CdxStreamProbeDataT *p)
{
    unsigned mTaglen = 0;
    if (AtracId3v2Match((cdx_uint8 *)p->buf, ID3v2_EA3_MAGIC))
        mTaglen = AtracId3v2TagLen((cdx_uint8 *)p->buf);

    if(p->len < mTaglen + 5)
    {
        CDX_LOGE("Atrac Probe Data Not Enough");
        return CDX_FALSE;
    }

    p->buf += mTaglen;
    if(!memcmp(p->buf, "EA3", 3) && !p->buf[4] && p->buf[5] == EA3_HEADER_SIZE)
        return CDX_TRUE;
    else
        return CDX_FALSE;
}

static cdx_uint32 CdxAtracParserProbe(CdxStreamProbeDataT *probeData)
{
    if(probeData->len < ID3v2_HEADER_SIZE || !AtracProbe(probeData))
    {
        CDX_LOGE("Atrac Probe Failed");
        return 0;
    }

    return 100;
}

CdxParserCreatorT atracParserCtor =
{
    .create = CdxAtracParserOpen,
    .probe = CdxAtracParserProbe
};
