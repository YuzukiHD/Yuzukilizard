/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxPmpParser.c
 * Description : pmp parser
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxPmpParser"
#include "CdxPmpParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#define CheckNalIntegrity (0)

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))

//* this table is for AAC audio.
#define PMP_NUM_SAMPLE_RATES    12
static const cdx_uint32 sampRateTab[PMP_NUM_SAMPLE_RATES] = {
    96000, 88200, 64000, 48000,
    44100, 32000, 24000, 22050,
    16000, 12000, 11025,  8000
};

static int CheckPmpHeader(CdxPmpParser *pmpParser)
{
    if (pmpParser->vdCodecID > 1)
    {
        CDX_LOGE("unknown video codec id.");
        return -1;
    }
    if (pmpParser->vdFrmNum == 0)
    {
        CDX_LOGE("total frame number is zero.");
        return -1;
    }
    if (pmpParser->auCodecID > 1)
    {
        CDX_LOGE("unknow audio format.");
        return -1;
    }
    if (pmpParser->vdWidth == 0 || pmpParser->vdHeight == 0)
    {
        CDX_LOGW("video width and height error.");
    }
    if (pmpParser->vdScale == 0 || pmpParser->vdRate == 0)
    {
        CDX_LOGW("video frame rate error.");
        pmpParser->vdScale = 1000;
        pmpParser->vdRate = 30000;
    }
    return 0;
}
static int CreateDataFrameDescription(CdxPmpParser *pmpParser)
{
    cdx_uint8 *tmpBuf = (cdx_uint8 *)CdxMalloc(pmpParser->auStrmNum *
                                    pmpParser->maxAuPerFrame * 4 + 13);
    if (!tmpBuf)
    {
        CDX_LOGE("malloc fail");
        return -1;
    }
    cdx_uint32 **auSize = (cdx_uint32 **)CdxMalloc(pmpParser->auStrmNum * sizeof(cdx_uint32 *));
    if (!auSize)
    {
        free(tmpBuf);
        return -1;
    }

    cdx_uint32 *tmpAuFrameSize = (cdx_uint32 *)CdxMalloc(pmpParser->auStrmNum *
                                                pmpParser->maxAuPerFrame * 4);
    if (!tmpAuFrameSize)
    {
        free(tmpBuf);
        free(auSize);
        return -1;
    }
    cdx_uint32 i;
    for (i = 0; i < pmpParser->auStrmNum; i++)
    {
        auSize[i] = tmpAuFrameSize + i * pmpParser->maxAuPerFrame * 4;
    }

    DataFrameDescription *dataFrame = &pmpParser->dataFrame;
    dataFrame->tmpBuf = tmpBuf;
    dataFrame->auSize = auSize;

    if(pmpParser->auCodecID == 1)
    {
        cdx_uint16 sampRateIdx_4bits             = 0;
        cdx_uint16 chlConfig_3bits           = 0;

        for(i = 0; i < PMP_NUM_SAMPLE_RATES; i++)
        {
            if(pmpParser->auRate == sampRateTab[i])
                break;
        }

        if (i < PMP_NUM_SAMPLE_RATES)
            sampRateIdx_4bits = i;
        else
        {
            CDX_LOGW("unsupport audio sample rate.");
            sampRateIdx_4bits = 0;
        }

        if(pmpParser->bStereo)
            chlConfig_3bits = 2;
        else
            chlConfig_3bits = 1;

        dataFrame->aac_hdr[0] = 0xff;
        dataFrame->aac_hdr[1] = 0xf1;
        dataFrame->aac_hdr[2] = 1<<6|sampRateIdx_4bits<<2;
        dataFrame->aac_hdr[3] = chlConfig_3bits<<6;
        dataFrame->aac_hdr[4] = 0;
        dataFrame->aac_hdr[5] = 0x1f;
        dataFrame->aac_hdr[6] = 0xfc;
    }
    return 0;
}
CDX_INTERFACE int DestoryDataFrameDescription(CdxPmpParser *pmpParser)
{
    DataFrameDescription *dataFrame = &pmpParser->dataFrame;
    if(dataFrame->tmpBuf)
    {
        free(dataFrame->tmpBuf);
    }
    if(dataFrame->auSize)
    {
        free(dataFrame->auSize[0]);
        free(dataFrame->auSize);
    }
    return 0;
}

static int BuildKeyFrameIndexTable(CdxPmpParser *pmpParser)
{
    DataFrameIndex *pIdx = (DataFrameIndex *)CdxMalloc(pmpParser->vdFrmNum * 4);
    if (!pIdx)
    {
        CDX_LOGE("malloc fail");
        return -1;
    }
    KeyFrameEntry *pKeyIdx = NULL;
    int len = pmpParser->vdFrmNum * 4;
    int ret = CdxStreamRead(pmpParser->file, pIdx, len);
    if(ret != len)
    {
        CDX_LOGE("CdxStreamRead fail, request(%d), ret(%d)", len, ret);
        goto _fail;
    }

    //* get key frame number
    cdx_uint32 frmCntNum, keyFrmNum = 0;
    for(frmCntNum = 0; frmCntNum < pmpParser->vdFrmNum; frmCntNum++)
    {
        keyFrmNum += pIdx[frmCntNum].bKeyFrame;
    }

    //* build key frame table
    pKeyIdx = (KeyFrameEntry *)CdxMalloc(keyFrmNum * sizeof(KeyFrameEntry));
    if(!pKeyIdx)
    {
        CDX_LOGE("malloc fail");
        goto _fail;
    }

    cdx_int64 filePos = CdxStreamTell(pmpParser->file);
    if(filePos < 0)
    {
        CDX_LOGE("CdxStreamTell fail, return(%lld)", filePos);
        goto _fail;
    }

    cdx_uint32 keyFrmCntNum = 0;
    for(frmCntNum = 0; keyFrmCntNum < keyFrmNum; frmCntNum++)
    {
        if(pIdx[frmCntNum].bKeyFrame)
        {
            pKeyIdx[keyFrmCntNum].filePos = filePos;
            pKeyIdx[keyFrmCntNum].frmID   = frmCntNum;
            keyFrmCntNum++;
        }

        filePos += pIdx[frmCntNum].nFrmSize;
    }

    free(pIdx);
    pmpParser->pKeyFrmIdx = pKeyIdx;
    pmpParser->keyIdxNum  = keyFrmNum;
    return 0;

_fail:
    if (pIdx)
        free(pIdx);

    if (pKeyIdx)
        free(pKeyIdx);

    return -1;

}
static int SetMediaInfo(CdxPmpParser *pmpParser)
{
    CdxMediaInfoT *mediaInfo = &pmpParser->mediaInfo;
    mediaInfo->fileSize = CdxStreamSize(pmpParser->file);
    mediaInfo->bSeekable = CdxStreamSeekAble(pmpParser->file);
    struct CdxProgramS *program = &mediaInfo->program[0];
    pmpParser->durationUs = (cdx_uint64)pmpParser->vdFrmInterval * pmpParser->vdFrmNum;
    program->duration = pmpParser->durationUs / 1000;//ms
    program->videoNum = 1;
    program->audioNum = pmpParser->auStrmNum;
    program->subtitleNum = 0;
    program->audioIndex = 0;
    VideoStreamInfo *video = &program->video[0];
    if(pmpParser->vdCodecID == 1)
        video->eCodecFormat = VIDEO_CODEC_FORMAT_H264;
    else
        video->eCodecFormat = VIDEO_CODEC_FORMAT_XVID;

    video->nWidth = pmpParser->vdWidth;
    video->nHeight = pmpParser->vdHeight;
    /*视频帧率，表示每1000秒有多少帧画面被播放，例如25000、29970、30000等*/
    video->nFrameRate = pmpParser->vdRate * 1000 / pmpParser->vdScale;
    /*两帧画面之间的间隔时间，是帧率的倒数，单位为us*/
    video->nFrameDuration = pmpParser->vdFrmInterval;

    video->nCodecSpecificDataLen = pmpParser->tempVideoInfo.nCodecSpecificDataLen;
    video->pCodecSpecificData = pmpParser->tempVideoInfo.pCodecSpecificData;
    if(pmpParser->auStrmNum)
    {
        AudioStreamInfo *audio = &program->audio[0];
        if(pmpParser->auCodecID == 1)
        {
            audio->eCodecFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
            audio->nFlags |= 1;
        }
        else
        {
            CDX_LOGD("auCodecID(%d), audio is mp3 format.", pmpParser->auCodecID);
            audio->eCodecFormat = AUDIO_CODEC_FORMAT_MP3;
        }
        if(pmpParser->bStereo)
            audio->nChannelNum = 2;
        else
            audio->nChannelNum = 1;

        audio->nSampleRate = pmpParser->auRate;

        if(pmpParser->auStrmNum > 1)
        {
            CDX_LOGD("have %d audio streams", pmpParser->auStrmNum);
            cdx_uint32 i;
            for(i = 1; i < pmpParser->auStrmNum && i < AUDIO_STREAM_LIMIT; i++)
            {
                memcpy(&program->audio[i], audio, sizeof(AudioStreamInfo));
            }
        }

    }
    return 0;
}
static cdx_int32 PrefetchPacket(CdxPmpParser *pmpParser, CdxPacketT *cdxPkt)
{
    DataFrameDescription *dataFrame = &pmpParser->dataFrame;
    int ret = 0, len;
    cdx_uint32 i, j;
    memset(cdxPkt, 0, sizeof(CdxPacketT));
_newDataFrame:
    if(pmpParser->curStream == 0)
    {
        pmpParser->dataFrameCount++;
        if(pmpParser->dataFrameCount > pmpParser->vdFrmNum)
        {
            CDX_LOGD("pmpParser EOS");
            pmpParser->mErrno = PSR_EOS;
            ret = -1;
            goto _exit;
        }

        cdx_uint8 *buf = dataFrame->tmpBuf;
        ret = CdxStreamRead(pmpParser->file, buf, 13);
        if(ret != 13)
        {
            CDX_LOGE("CdxStreamRead fail, request(13), ret(%d)", ret);
            ret = -1;
            goto _exit;
        }
        dataFrame->audFrmNum = buf[0];
        dataFrame->firstDelay = buf[1] | buf[2]<<8 | buf[3]<<16 | buf[4]<<24;
        //dataFrame->lastDelay = buf[5] | buf[6]<<8 | buf[7]<<16 | buf[8]<<24;
        dataFrame->vdFrmSize = buf[9] | buf[10]<<8 | buf[11]<<16 | buf[12]<<24;

        //* Calculate video and audio pts in millisecond.
        dataFrame->vdPts = pmpParser->dataFrameCount * (cdx_uint64)pmpParser->vdFrmInterval;
        dataFrame->auPts = dataFrame->vdPts + dataFrame->firstDelay;

        if(dataFrame->audFrmNum > pmpParser->maxAuPerFrame)
        {
            CDX_LOGE("audFrmNum(%u) > maxAuPerFrame(%u)",
                    dataFrame->audFrmNum, pmpParser->maxAuPerFrame);
            ret = -1;
            pmpParser->mErrno = PSR_UNKNOWN_ERR;
            goto _exit;
        }
        len = dataFrame->audFrmNum * 4 * pmpParser->auStrmNum;
        ret = CdxStreamRead(pmpParser->file, buf, len);
        if(ret != len)
        {
            CDX_LOGE("CdxStreamRead fail, request(%d), ret(%d)", len, ret);
            ret = -1;
            goto _exit;
        }
        cdx_uint32 *audioFrameSize = (cdx_uint32 *)buf;
        for (i = 0; i < pmpParser->auStrmNum; i++)
        {
            for (j = 0; j < dataFrame->audFrmNum; j++)
            {
                dataFrame->auSize[i][j] = *audioFrameSize++;
            }
        }

        if(dataFrame->vdFrmSize == 0)
        {
            pmpParser->curStream = (pmpParser->curStream + 1)%(pmpParser->auStrmNum + 1);
        }
    }

    if(pmpParser->curStream == 0)
    {
        if(pmpParser->flags & DISABLE_VIDEO)
        {
            ret = CdxStreamSeek(pmpParser->file, dataFrame->vdFrmSize, SEEK_CUR);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamSeek fail, return(%d)", ret);
                goto _exit;
            }
            pmpParser->curStream = (pmpParser->curStream + 1)%(pmpParser->auStrmNum + 1);
        }
        else
        {
            cdxPkt->length = dataFrame->vdFrmSize;
            cdxPkt->pts = dataFrame->vdPts;
            cdxPkt->type = CDX_MEDIA_VIDEO;
            goto _succeed;
        }
    }

    if(pmpParser->flags & DISABLE_AUDIO)
    {
        len = 0;
        for (i = 0; i < pmpParser->auStrmNum; i++)
        {
            for (j = 0; j < dataFrame->audFrmNum; j++)
            {
                len += dataFrame->auSize[i][j];
            }
        }
        ret = CdxStreamSeek(pmpParser->file, len, SEEK_CUR);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamSeek fail, return(%d)", ret);
            goto _exit;
        }
        pmpParser->curStream = 0;
        goto _newDataFrame;
    }
    else
    {
        cdxPkt->pts = dataFrame->auPts;
        cdxPkt->type = CDX_MEDIA_AUDIO;
        cdxPkt->streamIndex = pmpParser->curStream - 1;
        len = 0;
        for (j = 0; j < dataFrame->audFrmNum; j++)
        {
            len += dataFrame->auSize[cdxPkt->streamIndex][j];
        }

        if(pmpParser->auCodecID == 1)
        {
            len += dataFrame->audFrmNum * 7;//aac head
        }
        cdxPkt->length = len;
        goto _succeed;
    }
_succeed:
    cdxPkt->flags |= (FIRST_PART|LAST_PART);
    return 0;
_exit:
    return ret;
}
static cdx_int32 ReadPacket(CdxPmpParser *pmpParser, CdxPacketT *pkt)
{
    DataFrameDescription  *dataFrame = &pmpParser->dataFrame;
    int ret, len;
    if(pkt->type == CDX_MEDIA_AUDIO && pmpParser->auCodecID == 1)
    {
        cdx_uint32 i;
        cdx_uint8 *data = pkt->buf;
        if(pkt->length <= pkt->buflen)
        {
            for(i = 0; i < dataFrame->audFrmNum; i++)
            {
                len = dataFrame->auSize[pmpParser->curStream - 1][i];
                dataFrame->aac_hdr[3] = (dataFrame->aac_hdr[3]&(~0x3)) | ((len+7)>>(13-2) & 0x3);
                dataFrame->aac_hdr[4] = ((len+7)>>3) & 0xff;
                dataFrame->aac_hdr[5] = ((len+7)&0x07)<<(8-3) | 0x1f;
                memcpy(data, dataFrame->aac_hdr, 7);
                data += 7;
                ret = CdxStreamRead(pmpParser->file, data, len);
                if(ret != len)
                {
                    CDX_LOGE("CdxStreamRead fail, request(%d), ret(%d)", len, ret);
                    ret = -1;
                    pmpParser->mErrno = PSR_UNKNOWN_ERR;
                    goto _exit;
                }
                data += len;
            }
        }
        else
        {
            int remaining = pkt->buflen;
            for(i = 0; i < dataFrame->audFrmNum; i++)
            {
                len = dataFrame->auSize[pmpParser->curStream - 1][i];
                dataFrame->aac_hdr[3] = (dataFrame->aac_hdr[3]&(~0x3)) | ((len+7)>>(13-2) & 0x3);
                dataFrame->aac_hdr[4] = ((len+7)>>3) & 0xff;
                dataFrame->aac_hdr[5] = ((len+7)&0x07)<<(8-3) | 0x1f;
                if(remaining == -1 || remaining >= 7)
                {
                    memcpy(data, dataFrame->aac_hdr, 7);
                    data += 7;
                    if(remaining != -1)
                    {
                        remaining -= 7;
                        if(!remaining)
                        {
                            data = pkt->ringBuf;
                            remaining = -1;
                        }
                    }
                }
                else
                {
                    for(i = 0; i < (cdx_uint32)remaining; i++)
                    {
                        *data++ = dataFrame->aac_hdr[i];
                    }
                    data = pkt->ringBuf;
                    for(i = remaining; i < 7; i++)
                    {
                        *data++ = dataFrame->aac_hdr[i];
                    }
                    remaining = -1;
                }

                if(remaining == -1 || remaining >= len)
                {
                    ret = CdxStreamRead(pmpParser->file, data, len);
                    if(ret != len)
                    {
                        CDX_LOGE("CdxStreamRead fail, request(%d), ret(%d)", len, ret);
                        ret = -1;
                        pmpParser->mErrno = PSR_UNKNOWN_ERR;
                        goto _exit;
                    }
                    data += len;
                    if(remaining != -1)
                    {
                        remaining -= len;
                        if(!remaining)
                        {
                            data = pkt->ringBuf;
                            remaining = -1;
                        }
                    }
                }
                else
                {
                    ret = CdxStreamRead(pmpParser->file, data, remaining);
                    if(ret != remaining)
                    {
                        CDX_LOGE("CdxStreamRead fail, request(%d), ret(%d)", remaining, ret);
                        ret = -1;
                        pmpParser->mErrno = PSR_UNKNOWN_ERR;
                        goto _exit;
                    }
                    data = pkt->ringBuf;
                    ret = CdxStreamRead(pmpParser->file, data, len - remaining);
                    if(ret != len - remaining)
                    {
                        CDX_LOGE("CdxStreamRead fail, request(%d), ret(%d)", len - remaining, ret);
                        ret = -1;
                        pmpParser->mErrno = PSR_UNKNOWN_ERR;
                        goto _exit;
                    }
                    data += len - remaining;
                    remaining = -1;
                }
            }

        }
    }
    else
    {
        if(pkt->length <= pkt->buflen)
        {
            ret = CdxStreamRead(pmpParser->file, pkt->buf, pkt->length);
            if(ret != pkt->length)
            {
                CDX_LOGE("CdxStreamRead fail, request(%d), ret(%d)", pkt->length, ret);
                ret = -1;
                pmpParser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }
        }
        else
        {
            ret = CdxStreamRead(pmpParser->file, pkt->buf, pkt->buflen);
            if(ret != pkt->buflen)
            {
                CDX_LOGE("CdxStreamRead fail, request(%d), ret(%d)", pkt->buflen, ret);
                ret = -1;
                pmpParser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }

            len = pkt->length - pkt->buflen;
            ret = CdxStreamRead(pmpParser->file, pkt->ringBuf, len);
            if(ret != len)
            {
                CDX_LOGE("CdxStreamRead fail, request(%d), ret(%d)", len, ret);
                ret = -1;
                pmpParser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }
        }
#if CheckNalIntegrity
        if(pmpParser->vdCodecID == 1)
        {
            char *data = pkt->buf, *tmp = NULL;
            size_t offset = 0, offset1 = 0;
            while (offset < pkt->buflen && data[offset] == 0x00) {
                ++offset;
            }
            if (offset == pkt->buflen) {
                data = pkt->ringBuf;
                while (offset1 < pkt->ringBufLen && data[offset1] == 0x00) {
                    ++offset1;
                }
                if(offset1 == pkt->ringBufLen)
                {
                    CDX_LOGE(" nal is incomplete!!!");//不是整数个nal
                    goto _exit1;
                }
                else
                {
                    tmp = data + offset1;
                }
            }
            else
            {
                tmp = data + offset;
            }

            // A valid startcode consists of at least two 0x00 bytes followed by 0x01.

            if (offset + offset1 < 2 || *tmp != 0x01) {
                CDX_LOGE("nal is incomplete!!!");//不是整数个nal
            }
            else
            {
                CDX_LOGD("OK");//是整数个nal
            }
        }
#endif
    }
_exit1:
    pmpParser->curStream = (pmpParser->curStream + 1)%(pmpParser->auStrmNum + 1);
    ret = 0;
_exit:
    return ret;
}
static int MakeSpecificData(CdxPmpParser *pmpParser)
{
    int ret = 0;
    CdxPacketT *pkt;
    unsigned int vCodecFormat = pmpParser->vdCodecID == 1 ?
                        VIDEO_CODEC_FORMAT_H264 : VIDEO_CODEC_FORMAT_XVID;
    while(1)
    {
        pkt = NULL;
        if(pmpParser->forceStop)
        {
            CDX_LOGE("PSR_USER_CANCEL");
            goto _exit;
        }

        pkt = (CdxPacketT *)malloc(sizeof(CdxPacketT));
        if(!pkt)
        {
            CDX_LOGE("malloc fail.");
            goto _exit;
        }
        ret = PrefetchPacket(pmpParser, pkt);
        if(ret < 0)
        {
            CDX_LOGW("PrefetchPacket fail.");
            goto _exit;
        }
        pkt->buf = (char *)malloc(pkt->length);
        if(!pkt->buf)
        {
            CDX_LOGE("malloc fail.");
            goto _exit;
        }
        pkt->buflen = pkt->length;
        ret = ReadPacket(pmpParser, pkt);
        if(ret < 0)
        {
            CDX_LOGE("ReadPacket fail");
            goto _exit;
        }
        if(pmpParser->packetNum >= MaxProbePacketNum)
        {
            CDX_LOGE("pmpParser->probePacketNum >= MaxProbePacketNum");
            goto _exit;
        }
        pmpParser->packets[pmpParser->packetNum++] = pkt;
        if(pkt->type == CDX_MEDIA_VIDEO)
        {
            if((pkt->length + pmpParser->vProbeBuf.probeDataSize) >
                    pmpParser->vProbeBuf.probeBufSize)
            {
                CDX_LOGE("probeDataSize too big!");
                goto _exit1;
            }
            else
            {
                cdx_uint8 *data = pmpParser->vProbeBuf.probeBuf +
                                    pmpParser->vProbeBuf.probeDataSize;
                memcpy(data, pkt->buf, pkt->length);
                pmpParser->vProbeBuf.probeDataSize += pkt->length;
            }
            ret = ProbeVideoSpecificData(&pmpParser->tempVideoInfo,
                    pmpParser->vProbeBuf.probeBuf,
                    pmpParser->vProbeBuf.probeDataSize,
                    vCodecFormat, CDX_PARSER_PMP);
            if(ret == PROBE_SPECIFIC_DATA_ERROR)
            {
                CDX_LOGE("probeVideoSpecificData error");
                goto _exit1;
            }
            else if(ret == PROBE_SPECIFIC_DATA_SUCCESS)
            {
                return ret;
            }
            else if(ret == PROBE_SPECIFIC_DATA_NONE)
            {
                pmpParser->vProbeBuf.probeDataSize = 0;
            }
            else if(ret == PROBE_SPECIFIC_DATA_UNCOMPELETE)
            {

            }
            else
            {
                CDX_LOGE("probeVideoSpecificData (%d), it is unknown.", ret);
            }
        }
    }

_exit:
    if(pkt)
    {
        if(pkt->buf)
        {
            free(pkt->buf);
        }
        free(pkt);
    }
_exit1:
    return ret;
}

int PmpParserInit(CdxParserT *parser)
{
    CDX_LOGI("PmpParserInit start");

    CdxPmpParser *pmpParser = (CdxPmpParser *)parser;

    cdx_uint8 pmpHead[56] = {0};

    int ret = CdxStreamRead(pmpParser->file, pmpHead, 56);
    if(ret != 56)
    {
        CDX_LOGE("CdxStreamRead(%d)", ret);
        goto _exit;
    }
    cdx_uint32 *data = (cdx_uint32 *)pmpHead;
    if((MKTAG('p', 'm', 'p', 'm') != *data++) || (1 != *data++))
    {
        CDX_LOGE("may not be pmp-2.0");
        goto _exit;
    }
    pmpParser->vdCodecID        = *data++;
    pmpParser->vdFrmNum         = *data++;
    pmpParser->vdWidth          = *data++;
    pmpParser->vdHeight         = *data++;
    pmpParser->vdScale          = *data++;
    pmpParser->vdRate           = *data++;
    pmpParser->auCodecID        = *data++;
    pmpParser->auStrmNum        = *data++;
    pmpParser->maxAuPerFrame    = *data++;
    pmpParser->auScale          = *data++;
    pmpParser->auRate           = *data++;
    pmpParser->bStereo          = *data++;

    if(CheckPmpHeader(pmpParser) != 0)
    {
        CDX_LOGE("format error");
        goto _exit;
    }
    pmpParser->vdFrmInterval = pmpParser->vdScale * 1000LL *1000 / pmpParser->vdRate;

    if(CreateDataFrameDescription(pmpParser) != 0)
    {
        CDX_LOGE("PmpPacketCreate fail");
        goto _exit;
    }
    if(BuildKeyFrameIndexTable(pmpParser) != 0)
    {
        CDX_LOGE("PmpIndexTableBuild fail");
        goto _exit;
    }
    ret = MakeSpecificData(pmpParser);
    if(ret != PROBE_SPECIFIC_DATA_SUCCESS)
    {
        CDX_LOGE("MakeSpecificData fail");
        goto _exit;
    }
    SetMediaInfo(pmpParser);

    pmpParser->mErrno = PSR_OK;
    pthread_mutex_lock(&pmpParser->statusLock);
    pmpParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&pmpParser->statusLock);
    pthread_cond_signal(&pmpParser->cond);
    CDX_LOGI("PmpParserInit success");
    return 0;

_exit:
    pmpParser->mErrno = PSR_OPEN_FAIL;
    pthread_mutex_lock(&pmpParser->statusLock);
    pmpParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&pmpParser->statusLock);
    pthread_cond_signal(&pmpParser->cond);
    CDX_LOGI("PmpParserInit fail");
    return -1;
}

static cdx_int32 PmpParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    CdxPmpParser *pmpParser = (CdxPmpParser*)parser;
    if(pmpParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGE("status < CDX_PSR_IDLE, PmpParserGetMediaInfo invaild");
        pmpParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    memcpy(pMediaInfo, &pmpParser->mediaInfo, sizeof(CdxMediaInfoT));
    return 0;
}

static cdx_int32 PmpParserPrefetch(CdxParserT *parser, CdxPacketT *cdxPkt)
{
    CdxPmpParser *pmpParser = (CdxPmpParser*)parser;
    if(pmpParser->status != CDX_PSR_IDLE && pmpParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_IDLE && status != CDX_PSR_PREFETCHED, "
                "PmpParserPrefetch invaild");
        pmpParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(pmpParser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(cdxPkt, &pmpParser->cdxPkt, sizeof(CdxPacketT));
        return 0;
    }
    if(pmpParser->packetPos < pmpParser->packetNum)
    {
        memcpy(cdxPkt, pmpParser->packets[pmpParser->packetPos], sizeof(CdxPacketT));
        cdxPkt->buf = NULL;
        cdxPkt->buflen = 0;
        memcpy(&pmpParser->cdxPkt, cdxPkt, sizeof(CdxPacketT));
        pmpParser->status = CDX_PSR_PREFETCHED;
        return 0;
    }
    if(pmpParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if((pmpParser->flags & DISABLE_VIDEO) && (pmpParser->flags & DISABLE_AUDIO))
    {
        CDX_LOGE("DISABLE_VIDEO && DISABLE_AUDIO");
        pmpParser->mErrno = PSR_UNKNOWN_ERR;
        return -1;
    }

    pthread_mutex_lock(&pmpParser->statusLock);
    if(pmpParser->forceStop)
    {
        pthread_mutex_unlock(&pmpParser->statusLock);
        return -1;
    }
    pmpParser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&pmpParser->statusLock);
    int ret = PrefetchPacket(pmpParser, cdxPkt);
    pthread_mutex_lock(&pmpParser->statusLock);
    if(ret != 0)
    {
        pmpParser->status = CDX_PSR_IDLE;
    }
    else
    {
        memcpy(&pmpParser->cdxPkt, cdxPkt, sizeof(CdxPacketT));
        pmpParser->status = CDX_PSR_PREFETCHED;
    }
    pthread_mutex_unlock(&pmpParser->statusLock);
    pthread_cond_signal(&pmpParser->cond);
    return ret;
}

static cdx_int32 PmpParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxPmpParser *pmpParser = (CdxPmpParser*)parser;
    if(pmpParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_PREFETCHED, we can not read!");
        pmpParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(pmpParser->packetPos < pmpParser->packetNum)
    {

        cdx_uint8 *data = (cdx_uint8 *)pmpParser->packets[pmpParser->packetPos]->buf;
        if(pkt->length <= pkt->buflen)
        {
            memcpy(pkt->buf, data, pkt->length);
        }
        else
        {
            memcpy(pkt->buf, data, pkt->buflen);
            memcpy(pkt->ringBuf, data + pkt->buflen, pkt->length - pkt->buflen);
        }
        if(pmpParser->packets[pmpParser->packetPos])
        {
            if(pmpParser->packets[pmpParser->packetPos]->buf)
            {
                free(pmpParser->packets[pmpParser->packetPos]->buf);
            }
            free(pmpParser->packets[pmpParser->packetPos]);
            pmpParser->packets[pmpParser->packetPos] = NULL;
        }
        pmpParser->packetPos++;
        pmpParser->status = CDX_PSR_IDLE;
        return 0;
    }
    pthread_mutex_lock(&pmpParser->statusLock);
    if(pmpParser->forceStop)
    {
        pthread_mutex_unlock(&pmpParser->statusLock);
        return -1;
    }
    pmpParser->status = CDX_PSR_READING;
    pthread_mutex_unlock(&pmpParser->statusLock);
    int ret = ReadPacket(pmpParser, pkt);
    pthread_mutex_lock(&pmpParser->statusLock);
    pmpParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&pmpParser->statusLock);
    pthread_cond_signal(&pmpParser->cond);
    return ret;
}
cdx_int32 PmpParserForceStop(CdxParserT *parser)
{
    CDX_LOGV("PmpParserForceStop start");
    CdxPmpParser *pmpParser = (CdxPmpParser*)parser;
    pthread_mutex_lock(&pmpParser->statusLock);
    pmpParser->forceStop = 1;
    pmpParser->mErrno = PSR_USER_CANCEL;
    int ret = CdxStreamForceStop(pmpParser->file);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamForceStop fail");
    }
    while(pmpParser->status != CDX_PSR_IDLE && pmpParser->status != CDX_PSR_PREFETCHED)
    {
        pthread_cond_wait(&pmpParser->cond, &pmpParser->statusLock);
    }
    pthread_mutex_unlock(&pmpParser->statusLock);
    pmpParser->mErrno = PSR_USER_CANCEL;
    pmpParser->status = CDX_PSR_IDLE;
    CDX_LOGV("PmpParserForceStop end");
    return 0;
}
cdx_int32 PmpParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGV("PmpParserClrForceStop start");
    CdxPmpParser *pmpParser = (CdxPmpParser*)parser;
    if(pmpParser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("pmpParser->status != CDX_PSR_IDLE");
        pmpParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    pmpParser->forceStop = 0;
    int ret = CdxStreamClrForceStop(pmpParser->file);
    if(ret < 0)
    {
        CDX_LOGW("CdxStreamClrForceStop fail");
    }
    CDX_LOGI("PmpParserClrForceStop end");
    return 0;
}

static int PmpParserControl(CdxParserT *parser, int cmd, void *param)
{
    CdxPmpParser *pmpParser = (CdxPmpParser*)parser;
    (void)pmpParser;
    (void)param;

    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI("pmp parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            return PmpParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return PmpParserClrForceStop(parser);
        default:
            break;
    }
    return 0;
}

cdx_int32 PmpParserGetStatus(CdxParserT *parser)
{
    CdxPmpParser *pmpParser = (CdxPmpParser *)parser;
    return pmpParser->mErrno;
}
cdx_int32 PmpParserSeekTo(CdxParserT *parser, cdx_int64  timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    CDX_LOGV("PmpParserSeekTo start, timeUs = %lld", timeUs);
    CdxPmpParser *pmpParser = (CdxPmpParser *)parser;
    pmpParser->mErrno = PSR_OK;
    pmpParser->status = CDX_PSR_IDLE;
    pmpParser->packetPos = pmpParser->packetNum;
    if(timeUs < 0)
    {
        CDX_LOGE("timeUs invalid");
        pmpParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    else if(((cdx_uint64)timeUs/1000) >= (pmpParser->durationUs/1000))
    {
        CDX_LOGI("PSR_EOS");
        pmpParser->mErrno = PSR_EOS;
        return 0;
    }

    if(!pmpParser->mediaInfo.bSeekable)
    {
        CDX_LOGE("bSeekable = false");
        pmpParser->mErrno = PSR_UNKNOWN_ERR;
        return -1;
    }

    pthread_mutex_lock(&pmpParser->statusLock);
    if(pmpParser->forceStop)
    {
        pmpParser->mErrno = PSR_USER_CANCEL;
        pthread_mutex_unlock(&pmpParser->statusLock);
        return -1;
    }
    pmpParser->status = CDX_PSR_SEEKING;
    pthread_mutex_unlock(&pmpParser->statusLock);

    int ret;
    cdx_uint32 i, pos, frmId;
    frmId = timeUs / pmpParser->vdFrmInterval;
    for(i = 1; i < pmpParser->keyIdxNum; i++)
    {
        if(pmpParser->pKeyFrmIdx[i].frmID > frmId)
            break;
    }
    i--;
    pos = pmpParser->pKeyFrmIdx[i].filePos;
    ret = CdxStreamSeek(pmpParser->file, pos, SEEK_SET);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamSeek fail, return(%d)", ret);
        pmpParser->mErrno = PSR_UNKNOWN_ERR;
        goto _exit;
    }

    pmpParser->dataFrameCount = pmpParser->pKeyFrmIdx[i].frmID;
    pmpParser->curStream = 0;
    ret = 0;
    //cdx_uint64 seekTo = (cdx_uint64)pmpParser->vdFrmInterval * pmpParser->dataFrameCount;
    //CDX_LOGV("PmpParserSeekTo timeUs = %llu", seekTo);
_exit:
    pthread_mutex_lock(&pmpParser->statusLock);
    pmpParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&pmpParser->statusLock);
    pthread_cond_signal(&pmpParser->cond);
    CDX_LOGV("PmpParserSeekTo end, ret = %d", ret);
    return ret;
}
static cdx_int32 PmpParserClose(CdxParserT *parser)
{
    CdxPmpParser *pmpParser = (CdxPmpParser *)parser;

    int ret = PmpParserForceStop(parser);
    if(ret < 0)
    {
        CDX_LOGW("PmpParserForceStop fail");
    }

    CdxStreamClose(pmpParser->file);
    if(pmpParser->pKeyFrmIdx)
    {
        free(pmpParser->pKeyFrmIdx);
    }
    DestoryDataFrameDescription(pmpParser);
    unsigned int i;
    for(i = 0; i < pmpParser->packetNum; i++)
    {
        if(pmpParser->packets[i])
        {
            if(pmpParser->packets[i]->buf)
            {
                free(pmpParser->packets[i]->buf);
            }
            free(pmpParser->packets[i]);
        }
    }
    if(pmpParser->vProbeBuf.probeBuf)
    {
        free(pmpParser->vProbeBuf.probeBuf);
    }
    if(pmpParser->tempVideoInfo.pCodecSpecificData)
    {
        free(pmpParser->tempVideoInfo.pCodecSpecificData);
    }
    pthread_mutex_destroy(&pmpParser->statusLock);
    pthread_cond_destroy(&pmpParser->cond);
    free(pmpParser);
    return 0;
}

cdx_uint32 PmpParserAttribute(CdxParserT *parser)
{
    CdxPmpParser *pmpParser = (CdxPmpParser *)parser;
    return pmpParser->flags;
}

static struct CdxParserOpsS pmpParserOps =
{
    .control         = PmpParserControl,
    .prefetch         = PmpParserPrefetch,
    .read             = PmpParserRead,
    .getMediaInfo     = PmpParserGetMediaInfo,
    .close             = PmpParserClose,
    .seekTo            = PmpParserSeekTo,
    .attribute        = PmpParserAttribute,
    .getStatus        = PmpParserGetStatus,
    .init            = PmpParserInit
};

CdxParserT *PmpParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    CdxPmpParser *pmpParser = CdxMalloc(sizeof(CdxPmpParser));
    if(!pmpParser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(pmpParser, 0x00, sizeof(CdxPmpParser));

    int ret = pthread_cond_init(&pmpParser->cond, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_mutex_init(&pmpParser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);
    pmpParser->vProbeBuf.probeBuf = malloc(SIZE_OF_VIDEO_PROVB_DATA);
    CDX_FORCE_CHECK(pmpParser->vProbeBuf.probeBuf);
    pmpParser->vProbeBuf.probeBufSize = SIZE_OF_VIDEO_PROVB_DATA;

    pmpParser->file = stream;
    pmpParser->flags = flags & (DISABLE_VIDEO | DISABLE_AUDIO | DISABLE_SUBTITLE | MUTIL_AUDIO);
    pmpParser->base.ops = &pmpParserOps;
    pmpParser->base.type = CDX_PARSER_PMP;
    pmpParser->mErrno = PSR_INVALID;
    pmpParser->status = CDX_PSR_INITIALIZED;
    return &pmpParser->base;
}

cdx_uint32 PmpParserProbe(CdxStreamProbeDataT *probeData)
{
    if(probeData->len < 8)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }
    cdx_uint32 *data = (cdx_uint32 *)probeData->buf;
    if((MKTAG('p', 'm', 'p', 'm') != *data++) || (1 != *data))
    {
        CDX_LOGE("It is not pmp-2.0, and is not supported.");
        return 0;
    }
    return 100;
}

CdxParserCreatorT pmpParserCtor =
{
    .create    = PmpParserOpen,
    .probe     = PmpParserProbe
};
