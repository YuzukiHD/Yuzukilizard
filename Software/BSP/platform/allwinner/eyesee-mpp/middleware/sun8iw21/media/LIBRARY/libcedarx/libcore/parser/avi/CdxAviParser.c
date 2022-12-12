/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviParser.c
 * Description : AVI parser implementation.
 * History :
 *
 */

//#define CONFIG_LOG_LEVEL 4
#define LOG_TAG "CdxAviParser"
#include <CdxTypes.h>
#include <assert.h>
#include <CdxAviInclude.h>
#include <CdxParser.h>
#include <string.h>
//#include <CdxMediaDef.h>

#define ABS_PACKET_HEDAR_BUF_SIZE   (64)
//static cdx_int32   AbsPackHdrLen = 0;
//static cdx_uint8   AbsPackHdr[ABS_PACKET_HEDAR_BUF_SIZE];
enum CdxAviStatusE
{
    CDX_AVI_INITIALIZED, /*control , getMediaInfo, not prefetch or read, seekTo,.*/
    CDX_AVI_IDLE,
    CDX_AVI_PREFETCHING,
    CDX_AVI_PREFETCHED,
    CDX_AVI_SEEKING,
    CDX_AVI_EOS,
};

static cdx_uint8 aviHeaders[][8] = {{ 0x52, 0x49, 0x46, 0x46, 0x41, 0x56, 0x49, 0x20 },
                                    // 'R', 'I', 'F', 'F', 'A', 'V', 'I', ' '
                                    { 0x52, 0x49, 0x46, 0x46, 0x41, 0x56, 0x49, 0x58 },
                                    // 'R', 'I', 'F', 'F', 'A', 'V', 'I', 'X'
                                    { 0x52, 0x49, 0x46, 0x46, 0x41, 0x56, 0x49, 0x19 },
                                    // 'R', 'I', 'F', 'F', 'A', 'V', 'I', 0x19
                                    { 0x4f, 0x4e, 0x32, 0x20, 0x4f, 0x4e, 0x32, 0x66 },
                                    // 'O', 'N', '2', ' ', 'O', 'N', '2', 'f'
                                    { 0x52, 0x49, 0x46, 0x46, 0x41, 0x4d, 0x56, 0x20 },
                                    // 'R', 'I', 'F', 'F', 'A', 'M', 'V', ' '
                                    { 0 } };

static cdx_int32 AviProbe(CdxStreamProbeDataT *p)
{
    cdx_int32 i;

    /* check file header */
    for (i = 0; aviHeaders[i][0]; i++)
        if (!memcmp(p->buf, aviHeaders[i], 4) && !memcmp(p->buf + 8,
                aviHeaders[i] + 4, 4))
            return CDX_TRUE;

    return CDX_FALSE;
}

static int GetChunkDataDummy(CdxAviParserImplT *impl, CdxPacketT *pkt)
{
    AviFileInT  *aviIn;
    CdxStreamT  *fp = NULL;

    if(!impl)
    {
        return AVI_ERR_PARA_ERR;
    }

    aviIn = (AviFileInT *)impl->privData;
    if(!aviIn)
    {
        return AVI_ERR_PARA_ERR;
    }

    if(READ_CHUNK_SEQUENCE == aviIn->readmode)
    {
        fp = aviIn->fp;
    }
    else
    {
        switch(pkt->type)
        {
            case CDX_MEDIA_AUDIO:
            {
                fp = aviIn->audFp;//TODO...
                break;
            }
            case CDX_MEDIA_VIDEO:
            {
                fp = aviIn->fp;
                break;
            }
            default:
            {
                CDX_LOGV("READ_CHUNK_BY_INDEX ignore skip chunk.");
                break;
            }
        }
        return AVI_SUCCESS;
    }

    if(CdxStreamSeek(fp, pkt->length, STREAM_SEEK_CUR) != 0)
    {
        CDX_LOGV("seek file ptr failed when skip chunk data!");
        return AVI_ERR_PARA_ERR;
    }

    return AVI_SUCCESS;
}
static cdx_int32 CdxAviParserForceStop(CdxParserT *parser)
{
    CdxAviParserImplT *impl;
    CdxStreamT *stream;

    CDX_CHECK(parser);
    impl = CdxContainerOf(parser, CdxAviParserImplT, base);
    stream = impl->stream;

    pthread_mutex_lock(&impl->lock);
    impl->exitFlag = 1;
    pthread_mutex_unlock(&impl->lock);
    if(stream)
    {
        CdxStreamForceStop(stream);
    }
    pthread_mutex_lock(&impl->lock);
    while((impl->mStatus != CDX_AVI_IDLE) && (impl->mStatus != CDX_AVI_PREFETCHED))
    {
        pthread_cond_wait(&impl->cond, &impl->lock);
    }
    pthread_mutex_unlock(&impl->lock);

    impl->mErrno = PSR_USER_CANCEL;
    impl->mStatus = CDX_AVI_IDLE;
    CDX_LOGV("xxx flv forcestop finish.");

    return CDX_SUCCESS;
}
static cdx_int32 CdxAviParserClrForceStop(CdxParserT *parser)
{
    CdxAviParserImplT *impl;
    CdxStreamT *stream;

    CDX_CHECK(parser);
    impl = CdxContainerOf(parser, CdxAviParserImplT, base);
    stream = impl->stream;

    if(impl->mStatus != CDX_AVI_IDLE)
    {
        CDX_LOGW("impl->mStatus != CDX_AVI_IDLE");
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    impl->exitFlag = 0;
    if(stream)
    {
        CdxStreamClrForceStop(stream);
    }

    return 0;
}
static int AviGetCacheState(CdxAviParserImplT *impl, struct ParserCacheStateS *cacheState)
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

    return 0;
}

/*
CDX_PSR_CMD_MEDIAMODE_CONTRL,
CDX_PSR_CMD_SWITCH_AUDIO,
CDX_PSR_CMD_SWITCH_SUBTITLE,
CDX_PSR_CMD_DISABLE_AUDIO,
CDX_PSR_CMD_DISABLE_VIDEO,
CDX_PSR_CMD_DISABLE_SUBTITLE,
*/
static cdx_int32 __CdxAviParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    CdxAviParserImplT   *impl;
    cdx_int32            idx;

    impl = CdxContainerOf(parser, CdxAviParserImplT, base);
    if(!impl)
    {
        CDX_LOGE("avi file parser lib has not been initiated!");
        return -1;
    }

    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
        {
            CDX_LOGW("not support yet... cmd(%d)", cmd);
            return -1;
        }
        case CDX_PSR_CMD_SWITCH_AUDIO:
        {
            idx = *(cdx_int32*)param;
            if (idx >= impl->hasAudio || idx < 0)
            {
                CDX_LOGE("bad idx(%d)", idx);
                return CDX_FAILURE;
            }
            else
            {
                impl->nextCurAudStreamNum = idx;
            }
            break;
        }
        case CDX_PSR_CMD_SET_FORCESTOP:
        {
            return CdxAviParserForceStop(parser);
        }
        case CDX_PSR_CMD_CLR_FORCESTOP:
        {
            return CdxAviParserClrForceStop(parser);
        }
        case CDX_PSR_CMD_GET_CACHESTATE:
        {
            return AviGetCacheState(impl, param);
        }
        default:
        {
            CDX_LOGW("xxx cmd(%d) not support yet...", cmd);
            break;
        }
    }

    return CDX_SUCCESS;

}

static cdx_int32 CheckKeyFrame(AviFileInT *aviIn)
{
    OldIdxTableItemT *pSeqKeyFrameTableEntry = (OldIdxTableItemT *)aviIn->idx1Buf;
    cdx_int32 i;
    cdx_int32 ret = 0;

    for(i = 0; i < aviIn->indexCountInKeyfrmTbl; i++)
    {
        if(aviIn->frameIndex == pSeqKeyFrameTableEntry->frameIdx) //current frame is keyframe
        {
            ret = 1;
            //CDX_LOGD("cur frame idx(%d) is keyframe", aviIn->frameIndex);
            break;
        }
        else
        {
            pSeqKeyFrameTableEntry++;
        }
    }

    return ret;
}
static cdx_int32 __CdxAviParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxAviParserImplT *impl;
    cdx_int32          result = 0;
    AviChunkT          *chunk;
    AviFileInT         *aviIn;
    cdx_int32          nIgnoreFlg = 0;
    cdx_int8           streamId = -1;  //video stream id
    cdx_int8           vSteamIndex;    //video stream index

    impl = CdxContainerOf(parser, CdxAviParserImplT, base);
    if(!impl || !pkt)
    {
        CDX_LOGE("bad impl, please check...");
        return AVI_ERR_PARA_ERR;
    }
    aviIn = (AviFileInT *)impl->privData;
    if(!aviIn)
    {
        CDX_LOGE("bad aviIn, please check...");
        return AVI_ERR_PARA_ERR;
    }

    if(impl->mErrno == PSR_EOS)
    {
        CDX_LOGD("Avi eos.");
        return -1;
    }

    memset(pkt, 0x00, sizeof(CdxPacketT));

    if(impl->mStatus == CDX_AVI_PREFETCHED)//last time not read after prefetch.
    {
        memcpy(pkt, &impl->pkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&impl->lock);
    if(impl->exitFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->mStatus = CDX_AVI_PREFETCHING;
    impl->mErrno = PSR_OK;
    pthread_mutex_unlock(&impl->lock);

_read_one_chunk:
    if(impl->exitFlag == 1)
    {
        result = -1;
        goto __exit;
    }

    if(impl->curAudStreamNum != impl->nextCurAudStreamNum)
    {
        cdx_uint8 nTmpNum = impl->nextCurAudStreamNum;

        impl->curAudStreamNum = nTmpNum;
        impl->audioStreamIndex = impl->audioStreamIndexArray[impl->curAudStreamNum];

        if(READ_CHUNK_BY_INDEX == aviIn->readmode)
        {
            result = ConfigNewAudioAviReadContextIndexMode(impl);
            if(result < AVI_SUCCESS)
            {
                CDX_LOGW("change audio fail.");
                impl->mErrno = PSR_UNKNOWN_ERR;
                result = -1;
                goto __exit;
            }
        }
    }

    //clear current chunk information
    pkt->type = CDX_MEDIA_UNKNOWN;
    nIgnoreFlg = 0;

    result = AviRead(impl);
    if(result < 0)
    {
        CDX_LOGE("read result[%d] < 0.", result);
        if(CdxStreamEos(aviIn->fp) || AVI_ERR_END_OF_MOVI == result)
            impl->mErrno = PSR_EOS;
        else
            impl->mErrno = PSR_UNKNOWN_ERR;
        result = -1;
        goto __exit;
    }

    chunk = &aviIn->dataChunk;
    pkt->length  = chunk->length;

    switch(chunk->fcc >> 16)
    {
        case CDX_CKTYPE_DIB_BITS:
        case CDX_CKTYPE_DIB_COMPRESSED:
        {
            if(impl->hasVideo == 2)
            {
                if(CDX_STREAM_FROM_FOURCC(chunk->fcc) == impl->videoStreamIndexArray[0]
                    || CDX_STREAM_FROM_FOURCC(chunk->fcc) == impl->videoStreamIndexArray[1])
                {
                    pkt->type = CDX_MEDIA_VIDEO;
                    if(CDX_STREAM_FROM_FOURCC(chunk->fcc) == impl->videoStreamIndexArray[1])
                        // minor_stream
                    {
                        pkt->flags |= MINOR_STREAM;
                    }
                    CDX_LOGV("xxxxxxxxx video num[%d]", CDX_STREAM_FROM_FOURCC(chunk->fcc));
                }
                else
                {
                    pkt->type = CDX_MEDIA_UNKNOWN;
                    //CDX_LOGV("xxx pkt->type = CDX_MEDIA_UNKNOWN");
                    nIgnoreFlg = 1;
                }
            }
            else
            {
                //check if the stream number is wanted
                if(CDX_STREAM_FROM_FOURCC(chunk->fcc) == impl->videoStreamIndex)
                {
                    pkt->type = CDX_MEDIA_VIDEO;
                    if(CheckKeyFrame(aviIn) == 1)
                    {
                        pkt->flags |= KEY_FRAME;
                    }
                    aviIn->frameIndex++;
                }
                else
                {
                    pkt->type = CDX_MEDIA_UNKNOWN;
                   // CDX_LOGV("unknown type of bitstream, tag:%x, len:%d!\n",
                   //chunk->fcc, chunk->length);
                    nIgnoreFlg = 1;
                }
            }
            break;
        }
        case CDX_CKTYPE_WAVE_BYTES:
        {
/*          cdx_int32   streamId; // for support switch audio stream in CdxAviParserControl
            //check if the stream number is wanted
            streamId = CDX_STREAM_FROM_FOURCC(chunk->fcc);
            if(streamId == impl->audioStreamIndex)
            {
                if(impl->audioStreamIndex == impl->audioStreamIndexArray[impl->curAudStreamNum])
                {
                    cdx_int32   audPts;
                    audPts = CalcAviAudioChunkPts(&aviIn->audInfoArray[impl->curAudStreamNum],
                                    aviIn->nAudChunkTotalSizeArray[impl->curAudStreamNum],
                                    aviIn->nAudFrameCounterArray[impl->curAudStreamNum]);

                    audPts += aviIn->uBaseAudioPtsArray[impl->curAudStreamNum]
                              + impl->nAudPtsOffsetArray[impl->curAudStreamNum];
                    CDX_LOGV("avi audio pts: %d.",audPts);

                    pkt->pts = (cdx_int64)audPts;
                    pkt->pts *= 1000;
                    aviIn->nAudChunkCounterArray[impl->curAudStreamNum]++;
                    aviIn->nAudFrameCounterArray[impl->curAudStreamNum] +=
                        CalcAviChunkAudioFrameNum(&aviIn->audInfoArray[impl->curAudStreamNum],
                        chunk->length);

                    aviIn->nAudChunkTotalSizeArray[impl->curAudStreamNum] += chunk->length;
                }
                else
                {
                    CDX_LOGE("audioStreamIndex(%d) != audioStreamIndexArray[%d](%d)\n",
                        impl->audioStreamIndex, impl->curAudStreamNum,
                        impl->audioStreamIndexArray[impl->curAudStreamNum]);
                    impl->mStatus = CDX_AVI_IDLE;
                    return AVI_ERR_PARA_ERR;
                }

                pkt->type = CDX_MEDIA_AUDIO;
            }
            else
            {
                cdx_int32   nAudStreamNum = ConvertAudStreamidToArrayNum(impl, streamId);
                if(nAudStreamNum >= 0)
                {
                    aviIn->nAudChunkCounterArray[nAudStreamNum]++;
                    aviIn->nAudFrameCounterArray[nAudStreamNum] +=
                            CalcAviChunkAudioFrameNum(&aviIn->audInfoArray[nAudStreamNum],
                            chunk->length);

                    aviIn->nAudChunkTotalSizeArray[nAudStreamNum] += chunk->length;
                }
                pkt->type = CDX_MEDIA_UNKNOWN;
                //CDX_LOGV("unknown type of bitstream, tag:%x, len:%d!\n", chunk->fcc,
                chunk->length);
                nIgnoreFlg = 1;
            }*/
            cdx_int32   streamId;
            cdx_int32   nAudStreamNum;

            streamId = CDX_STREAM_FROM_FOURCC(chunk->fcc);
            nAudStreamNum = ConvertAudStreamidToArrayNum(impl, streamId);
            if(nAudStreamNum >= 0)
            {
                cdx_int32   audPts;
                audPts = CalcAviAudioChunkPts(&aviIn->audInfoArray[nAudStreamNum],
                                aviIn->nAudChunkTotalSizeArray[nAudStreamNum],
                                aviIn->nAudFrameCounterArray[nAudStreamNum]);

                audPts += aviIn->uBaseAudioPtsArray[nAudStreamNum]
                          + impl->nAudPtsOffsetArray[nAudStreamNum];

                pkt->pts = (cdx_int64)audPts;
                pkt->pts *= 1000;
                CDX_LOGV("avi audio index(%d), audio pts: %d. basepts=%u ms, ptsoffset=%u ms",
                    nAudStreamNum, audPts, aviIn->uBaseAudioPtsArray[nAudStreamNum],
                    impl->nAudPtsOffsetArray[nAudStreamNum]);

                aviIn->nAudChunkCounterArray[nAudStreamNum]++;
                aviIn->nAudFrameCounterArray[nAudStreamNum] +=
                    CalcAviChunkAudioFrameNum(&aviIn->audInfoArray[nAudStreamNum], chunk->length);

                aviIn->nAudChunkTotalSizeArray[nAudStreamNum] += chunk->length;
                pkt->type = CDX_MEDIA_AUDIO;
                pkt->streamIndex = nAudStreamNum;
            }
            else
            {
                CDX_LOGW("invalid audio stream index.");
                pkt->type = CDX_MEDIA_UNKNOWN;
                nIgnoreFlg = 1;
            }
            break;
        }
        case CDX_CKTYPE_SUB_TEXT: //TODO
        {
            //check if the stream number is wanted
            if(CDX_STREAM_FROM_FOURCC(chunk->fcc) == impl->subTitleStreamIndex)
            {
                pkt->type = CDX_MEDIA_SUBTITLE;
            }
            else
            {
                pkt->type = CDX_MEDIA_UNKNOWN;
                //CDX_LOGV("unknown type of bitstream, tag:%x, len:%d!\n", chunk->fcc,
                //chunk->length);
                nIgnoreFlg = 1;
            }
            break;
        }

        case CDX_CKTYPE_DIB_DRM:
        case CDX_CKTYPE_PAL_CHANGE:
        case CDX_CKTYPE_SUB_BMP:
        {
            cdx_int32   streamId;
            cdx_int32   nSubStreamNum;

            streamId = ((chunk->fcc & 0xff) - '0') * 10 + ((chunk->fcc>>8 & 0xff) - '0');
            nSubStreamNum = ConvertSubStreamidToArrayNum(impl, streamId);
            if(nSubStreamNum >= 0)
            {
                cdx_int64   subPts,subDuration;
                cdx_int32   ret;
                cdx_uint8   tempPktTime[28]={0};//[00:00:02.000-00:00:03.700]: 27 bytes.

                ret = CdxStreamRead(aviIn->fp, tempPktTime, 27);
                if(ret != 27)
                {
                    CDX_LOGW("read sb time failed.");
                    impl->mErrno = PSR_UNKNOWN_ERR;
                    result = -1;
                    goto __exit;
                }
                CDX_LOGV("tempPktTime:%s", tempPktTime);

                subPts = CalcAviSubChunkPts(tempPktTime, &subDuration);
                if(subPts >= 0)
                {
                    pkt->pts = subPts*1000;
                    pkt->duration = subDuration*1000;
                    CDX_LOGV("avi sub index[%d], sub pts:[%lld], sub duration:[%lld].",
                        nSubStreamNum, pkt->pts, pkt->duration);

                    pkt->length = chunk->length - 27;
                    pkt->type = CDX_MEDIA_SUBTITLE;
                    pkt->streamIndex = nSubStreamNum;
                }
                else
                {
                    ret = CdxStreamSeek(aviIn->fp, -27, STREAM_SEEK_CUR);
                    if(ret < 0)
                    {
                        CDX_LOGE("Seek to sub header failed.");
                        impl->mErrno = PSR_UNKNOWN_ERR;
                        result = -1;
                        goto __exit;
                    }
                    pkt->type = CDX_MEDIA_UNKNOWN;
                    nIgnoreFlg = 1;
                }
            }
            else
            {
                CDX_LOGW("invalid audio stream index.");
                pkt->type = CDX_MEDIA_UNKNOWN;
                nIgnoreFlg = 1;
            }
            break;
        }
        case CDX_CKTYPE_CHAP:
        {
            pkt->type = CDX_MEDIA_UNKNOWN;
            nIgnoreFlg = 1;
            //CDX_LOGV("unknown type of bitstream, tag:%x, len:%d!\n", chunk->fcc, chunk->length);
            break;
        }
        case CDX_CKID_AVI_NEWINDEX:
        {
            //get the index list, reach the end of movi data
            //CDX_LOGV("meet string idx1!");
            impl->mErrno = PSR_EOS;
            impl->mStatus = CDX_AVI_IDLE;
            return AVI_ERR_END_OF_MOVI;
        }
        default:
        {
            CDX_LOGV("unknown type of bitstream, tag:%x, len:%d!\n", chunk->fcc, chunk->length);
            pkt->type = CDX_MEDIA_UNKNOWN;
            nIgnoreFlg = 0;
            break;
        }
    }

    if(pkt->length >= MAX_FRAME_CHUNK_SIZE || pkt->length < 0)
    {
        if(aviIn->readmode == READ_CHUNK_BY_INDEX && (pkt->type == CDX_MEDIA_UNKNOWN))
        {
            CDX_LOGW("Chunk size(%x) is invalid, but we can skip it by index table!\n",
                pkt->length);
        }
        else
        {
            cdx_int64 vidPos;
            vidPos = CdxStreamTell(aviIn->fp);

            CDX_LOGW("chunk fcc[%x], Chunk size(%x) is too large, chunk_offset[%x,%x].",
                chunk->fcc, pkt->length, (cdx_int32)(vidPos>>32), (cdx_int32)(vidPos&0xFFFFFFFF));

//==================================================================>
            //try to find out a new valid chunk!
            const cdx_int32   nAnalyseDataBufSize = 1024*64;
            cdx_int32         nAnalyseDataLen;
            cdx_int32         i;
            cdx_int32         nFindNewValidChunkFlag=0;
            cdx_uint8        *pAnalyseBuf = (cdx_uint8 *)malloc(nAnalyseDataBufSize);
            if(pAnalyseBuf)
            {
                nAnalyseDataLen = CdxStreamRead(aviIn->fp, pAnalyseBuf, nAnalyseDataBufSize);
                if(nAnalyseDataLen < nAnalyseDataBufSize)
                {
                    CDX_LOGE("read failed.");
                    impl->mErrno = PSR_UNKNOWN_ERR;
                    result = AVI_ERR_FILE_FMT_ERR;
                    goto __exit;
                }
                for(i=0;(i+4)<=nAnalyseDataLen;i++)
                {
                    if((pAnalyseBuf[i]=='0' && pAnalyseBuf[i+1]=='0' && pAnalyseBuf[i+2]=='d'
                        && pAnalyseBuf[i+3]=='c')
                        || (pAnalyseBuf[i]=='0' && pAnalyseBuf[i+1]=='1' && pAnalyseBuf[i+2]=='w'
                        && pAnalyseBuf[i+3]=='b'))
                    {
                        CDX_LOGV("find new valid avi chunk! [%c%c%c%c],i=%d",
                            pAnalyseBuf[i], pAnalyseBuf[i+1],
                            pAnalyseBuf[i+2], pAnalyseBuf[i+3], i);
                        nFindNewValidChunkFlag = 1;
                        break;
                    }
                }
                free(pAnalyseBuf);
                pAnalyseBuf = NULL;
                if(nFindNewValidChunkFlag)
                {
                    if(CdxStreamSeek(aviIn->fp, vidPos+i, STREAM_SEEK_SET) < 0)
                    {
                        CDX_LOGE("seek failed.");
                        impl->mErrno = PSR_UNKNOWN_ERR;
                        result = AVI_ERR_FILE_FMT_ERR;
                        goto __exit;
                    }
                    goto _read_one_chunk;
                }
                else
                {
                    CDX_LOGW("cannot find new valid avi chunk, quit! cur pos(%x)",
                        (cdx_int32)(CdxStreamTell(aviIn->fp)&0xFFFFFFFF));
                    impl->mErrno = PSR_UNKNOWN_ERR;
                    result = AVI_ERR_FILE_FMT_ERR;
                    goto __exit;
                }
            }
            else
            {
                CDX_LOGE("malloc avibuf fail!");
                impl->mErrno = PSR_UNKNOWN_ERR;
                result = AVI_ERR_FILE_FMT_ERR;
                goto __exit;
            }
//<===============================================================
        }
    }
    if(pkt->type != CDX_MEDIA_UNKNOWN)
    {
        impl->unkownChunkNum = 0;
    }

    if(pkt->type == CDX_MEDIA_AUDIO)
    {
        if(impl->bDiscardAud || impl->curAudStreamNum != impl->nextCurAudStreamNum)
        {
            GetChunkDataDummy(impl, pkt);
            CDX_LOGV("bDiscardAud[%d], AudStreamNum[%d][%d]\n",impl->bDiscardAud,
                impl->curAudStreamNum, impl->nextCurAudStreamNum);
            goto _read_one_chunk;
        }
        //pkt->streamIndex = impl->curAudStreamNum;//
        //CDX_LOGV("xxxxxxxxx pkt->streamIndex(%d) impl->curAudStreamNum(%d),
        //impl->nextCurAudStreamNum(%d)",pkt->streamIndex,impl->curAudStreamNum,
        //impl->nextCurAudStreamNum);
    }
    else if(pkt->type == CDX_MEDIA_UNKNOWN)
    {
        if(0 == nIgnoreFlg)
        {
            impl->unkownChunkNum++;
        }
    }
 //   LOGV("Chunk FCC:%x, chunk type:%d, tsize:%d\n",
 //                       chunk->fcc, tmpAviPsr->CurChunkInfo.chk_type, cdx_pkt->pkt_length);

    if(impl->unkownChunkNum >= MAX_READ_CHUNK_ERROR_NUM)
    {
        CDX_LOGV("unkown chunk num [%d] >=[%d].", impl->unkownChunkNum, MAX_READ_CHUNK_ERROR_NUM);
        impl->mErrno = PSR_UNKNOWN_ERR;
        result = AVI_ERR_FILE_FMT_ERR;
        goto __exit;
    }

    if(pkt->type == CDX_MEDIA_VIDEO) //TODO: PTS
    {
        cdx_int64   tmpPts;

        tmpPts = impl->aviFormat.nMicSecPerFrame;
        //when there are two Video streams
        if(impl->hasVideo == 2)
        {
            for(vSteamIndex = 0; vSteamIndex < 2; vSteamIndex++)
            {
                if(impl->videoStreamIndexArray[(cdx_int32)vSteamIndex] ==
                    CDX_STREAM_FROM_FOURCC(chunk->fcc))
                {
                    streamId = vSteamIndex;
                    break;
                }
            }
            if(streamId == 0)
            {
                aviIn->frameIndex++;
                tmpPts = tmpPts * aviIn->frameIndex1++;
            }
            else
            {
                tmpPts = tmpPts * aviIn->frameIndex2++;
            }
        }
        //when only one Video stream
        else
        {
            tmpPts = tmpPts * (aviIn->frameIndex - 1);
            //because we stat frame_index in GetNextChunkInfo().
        }

        pkt->pts = impl->nVidPtsOffset * 1000 + tmpPts;
        CDX_LOGV("avi video pts: %lld[ms], pkt_length[0x%x], aviIn->frameIndex[%d], tmpPts[%lld]",
            pkt->pts/1000, pkt->length, aviIn->frameIndex, tmpPts);
    }

    if(pkt->type == CDX_MEDIA_UNKNOWN)
    {
        GetChunkDataDummy(impl, pkt);
        CDX_LOGD("pkt->type == CDX_MEDIA_UNKNOWN, prefetch again."); //skip others...
        goto _read_one_chunk;
    }
    pkt->flags |= (FIRST_PART|LAST_PART);
       memcpy(&impl->pkt, pkt, sizeof(CdxPacketT));
    result = 0;

__exit:
    pthread_mutex_lock(&impl->lock);
    if(result < 0)
    {
        impl->mStatus = CDX_AVI_IDLE;
    }
    else
    {
        impl->mStatus = CDX_AVI_PREFETCHED;
    }
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return result;
}

static cdx_int32 __CdxAviParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32           readSize;
    AviFileInT          *aviIn;
    CdxAviParserImplT   *impl;
    CdxStreamT          *fp;

    impl = CdxContainerOf(parser, CdxAviParserImplT, base);
    if(!impl)
    {
        CDX_LOGE("impl NULL.");
        return AVI_ERR_PARA_ERR;
    }

    if(impl->exitFlag == 1)
    {
        CDX_LOGW("avi parser read forcestop");
        return -1;
    }

    if(impl->mStatus != CDX_AVI_PREFETCHED)
    {
        CDX_LOGW("mStatus != CDX_PSR_PREFETCHED, invaild operation.");
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    aviIn = (AviFileInT *)impl->privData;

    if(!aviIn)
    {
        CDX_LOGE("aviIn NULL.");
        return AVI_ERR_PARA_ERR;
    }

    if(READ_CHUNK_SEQUENCE == aviIn->readmode)
    {
        fp = aviIn->fp;
    }
    else
    {
        switch(pkt->type)
        {
            case CDX_MEDIA_AUDIO:
            {
                fp = aviIn->audFp;
                break;
            }
            case CDX_MEDIA_VIDEO:
            {
                fp = aviIn->fp;
                break;
            }
            case CDX_MEDIA_SUBTITLE:
            {
                fp = aviIn->fp;
                break;
            }
            default:
            {
                CDX_LOGE("AVI fp wrong!");
                return AVI_EXCEPTION;
            }
        }
    }

    if(pkt->length <= 0)
    {
        //pkt->ctrl_bits |= CEDARV_FLAG_MPEG4_EMPTY_FRAME;
    }
    else
    {
        if(pkt->length <= pkt->buflen)
        {
            readSize = CdxStreamRead(fp, pkt->buf,pkt->length);
            if(readSize < 0)
            {
                CDX_LOGW("read failed.");
                return AVI_ERR_FAIL;
            }
#if 0
            if(fp->data_src_desc.thirdpart_stream_type == CEDARX_THIRDPART_STREAM_USER1)
            {
                if(pkt->pkt_type == CDX_PacketVideo &&
                        (fp->data_src_desc.thirdpart_encrypted_type &
                        CEDARX_THIRDPART_ENCRYPTED_VIDEO))
                {
                    fp->decrypt(pkt->pkt_info.epdk_read_pkt_info.pkt_buf0,
                        read_size, (int)CDX_PacketVideo, fp);
                }
            }
#endif
        }
        else
        {
            readSize = CdxStreamRead(fp, pkt->buf, pkt->buflen);
            if(readSize < 0)
            {
                CDX_LOGW("read failed.");
                return AVI_ERR_FAIL;
            }
#if 0
            if(fp->data_src_desc.thirdpart_stream_type == CEDARX_THIRDPART_STREAM_USER1)
            {
                if(pkt->pkt_type == CDX_PacketVideo &&
                        (fp->data_src_desc.thirdpart_encrypted_type &
                        CEDARX_THIRDPART_ENCRYPTED_VIDEO))
                {
                    fp->decrypt(pkt->pkt_info.epdk_read_pkt_info.pkt_buf0,
                        read_size, (int)CDX_PacketVideo, fp);
                }
            }
#endif
            readSize = CdxStreamRead(fp, pkt->ringBuf, pkt->length - pkt->buflen);
            if(readSize < 0)
            {
                CDX_LOGW("read failed.");
                return AVI_ERR_FAIL;
            }
#if 0
            if(fp->data_src_desc.thirdpart_stream_type == CEDARX_THIRDPART_STREAM_USER1)
            {
                if(pkt->pkt_type == CDX_PacketVideo &&
                        (fp->data_src_desc.thirdpart_encrypted_type &
                        CEDARX_THIRDPART_ENCRYPTED_VIDEO))
                {
                    fp->decrypt(pkt->pkt_info.epdk_read_pkt_info.pkt_buf1, read_size,
                        (int)CDX_PacketVideo, fp);
                }
            }
#endif
        }
    }

    impl->mStatus = CDX_AVI_IDLE;
    return CDX_SUCCESS;
}

static cdx_int32 __CdxAviParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    CdxAviParserImplT   *impl;
    cdx_int32           i;
    AviFileInT          *aviIn;

    impl = CdxContainerOf(parser, CdxAviParserImplT, base);
    if(!impl)
    {
        CDX_LOGE("avi file parser lib has not been initiated!");
        return -1;
    }
    aviIn = (AviFileInT *)impl->privData;
    pMediaInfo->fileSize = aviIn->fileSize;
    //set if the media file contain audio, video or subtitle
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pMediaInfo->program[0].audioNum     = impl->hasAudio;
    pMediaInfo->program[0].subtitleNum  = impl->hasSubTitle;
    pMediaInfo->program[0].videoNum     = impl->hasVideo;
    pMediaInfo->bSeekable = (impl->hasIndex)!=0 ? 1 : 0;

    //set audio bitstream information
    for(i=0; i< AUDIO_STREAM_LIMIT; i++)
    {
        if(i < impl->hasAudio && impl->hasAudio <= AUDIO_STREAM_LIMIT)
        {

            memcpy(&pMediaInfo->program[0].audio[i], &impl->aFormatArray[i],
                sizeof(AudioStreamInfo));
            strcpy((char*)pMediaInfo->program[0].audio[i].strLang,
                (const char*)aviIn->audInfoArray[i].sStreamName);
            //CDX_LOGD("xxxxxxxxxxxxxx %s", pMediaInfo->program[0].audio[i].strLang);

            pMediaInfo->program[0].audio[i].pCodecSpecificData =
                impl->aFormatArray[i].pCodecSpecificData;
            pMediaInfo->program[0].audio[i].nCodecSpecificDataLen =
                impl->aFormatArray[i].nCodecSpecificDataLen;
        }
        else
        {
            break;
        }
    }

    for(i = 0; i < VIDEO_STREAM_LIMIT; i++)
    {
        if(i < impl->hasVideo)
        {
            CDX_LOGV("i %d, hasVideo num %d", i, impl->hasVideo);
            //set video bitstream information.
            memcpy(&pMediaInfo->program[0].video[i],
            &impl->aviFormat.vFormat, sizeof(VideoStreamInfo));

            pMediaInfo->program[0].video[i].nCodecSpecificDataLen =
                impl->aviFormat.vFormat.nCodecSpecificDataLen;
            pMediaInfo->program[0].video[i].pCodecSpecificData =
                impl->aviFormat.vFormat.pCodecSpecificData;
        }
        else
        {
            break;
        }

        if(impl->hasVideo == 2)
        {
            pMediaInfo->program[0].video[i].bIs3DStream = 1;
        }
        else
        {
            pMediaInfo->program[0].video[i].bIs3DStream = 0;
        }
    }

    //set subtitle bitstream information
    for(i=0; i< SUBTITLE_STREAM_LIMIT; i++)
    {
        if(i < impl->hasSubTitle && impl->hasSubTitle <= SUBTITLE_STREAM_LIMIT)
        {
            memcpy(&pMediaInfo->program[0].subtitle[i], &impl->tFormatArray[i],
                sizeof(SubtitleStreamInfo));
            strcpy((char*)pMediaInfo->program[0].subtitle[i].strLang,
                (const char*)aviIn->subInfoArray[i].sStreamName);
            //CDX_LOGD("xxxxxxxxxxxxxx %s", pMediaInfo->program[0].subtitle[i].strLang);

            //CDX_LOGD("xxxxxxxxx impl->tFormatArray[i].pCodecSpecificData(%s)",
            //impl->tFormatArray[i].pCodecSpecificData);
            pMediaInfo->program[0].subtitle[i].pCodecSpecificData =
                impl->tFormatArray[i].pCodecSpecificData;
            pMediaInfo->program[0].subtitle[i].nCodecSpecificDataLen =
                impl->tFormatArray[i].nCodecSpecificDataLen;
        }
        else
        {
            break;
        }
    }

    //set total time
    pMediaInfo->program[0].duration = (cdx_int64)impl->totalFrames *
        impl->aviFormat.nMicSecPerFrame / 1000;
    if(pMediaInfo->fileSize > 0 && pMediaInfo->program[0].duration > 0)
    {
        pMediaInfo->bitrate = pMediaInfo->fileSize / pMediaInfo->program[0].duration * 8;
    }
    else
    {
        pMediaInfo->bitrate = 0;
    }

    impl->mStatus = CDX_AVI_IDLE;

    return AVI_SUCCESS;
}
static cdx_int32 __CdxAviParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    CdxAviParserImplT *impl;
    AviFileInT        *aviIn;
    cdx_int32   totalTime;
    cdx_int64   seekTime = timeUs / 1000;
    cdx_int32   ret;
    //CDX_LOGV("xxx begin to seek..");
    CDX_FORCE_CHECK(parser);
    impl = CdxContainerOf(parser, CdxAviParserImplT, base);
    aviIn = (AviFileInT *)impl->privData;

    totalTime = ((cdx_int64)impl->totalFrames * impl->aviFormat.nMicSecPerFrame) / 1000;

    if(seekTime < 0)
    {
        CDX_LOGE("Bad seek_time. seekTime = %lld, totalTime = %d", seekTime, totalTime);
        impl->mStatus = CDX_AVI_IDLE;
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    else if(seekTime >= totalTime)
    {
        CDX_LOGI("R- eos. seekTime = %lld, totTime= %u", seekTime, totalTime);
        impl->mErrno = PSR_EOS;
        return 0;
    }

    pthread_mutex_lock(&impl->lock);
    if(impl->exitFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->mStatus = CDX_AVI_SEEKING;
    pthread_mutex_unlock(&impl->lock);

    ret = ReconfigAviReadContext(impl, seekTime, 0, 0);
    if(ret < AVI_SUCCESS)
    {
        CDX_LOGE("ReconfigAviReadContext failed.");
        impl->mErrno = PSR_UNKNOWN_ERR;
        ret = -1;
        goto __exit;
    }

    if(impl->hasVideo == 2)
    {
        aviIn->frameIndex1 = aviIn->frameIndex;
        aviIn->frameIndex2 = aviIn->frameIndex;
    }
    ret = 0;
    //CDX_LOGV("xxx finish seek...");

__exit:
    pthread_mutex_lock(&impl->lock);
    impl->mStatus = CDX_AVI_IDLE;
    impl->mErrno = PSR_OK;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return ret;
}
/*return flags define as open's flags*/
static cdx_uint32 __CdxAviParserAttribute(CdxParserT *parser)
{
    CdxAviParserImplT *impl;

    CDX_FORCE_CHECK(parser);
    impl = CdxContainerOf(parser, CdxAviParserImplT, base);
    impl->flags = 0;

    return impl->flags;
}

static cdx_int32 __CdxAviParserGetStatus(CdxParserT *parser)
{
    CdxAviParserImplT *impl;

    CDX_FORCE_CHECK(parser);
    impl = CdxContainerOf(parser, CdxAviParserImplT, base);

    return impl->mErrno;
}

static cdx_int32 __CdxAviParserClose(CdxParserT *parser)
{
    CdxAviParserImplT *impl;

    CDX_FORCE_CHECK(parser);
    impl = CdxContainerOf(parser, CdxAviParserImplT, base);

    if(!impl)
    {
        CDX_LOGE("Close error, there is no file information.");
        return -1;
    }
    impl->exitFlag = 1;
    pthread_mutex_destroy(&impl->lock);
    pthread_cond_destroy(&impl->cond);
    AviClose(impl);
    AviExit(impl);

    return CDX_SUCCESS;
}

static int __CdxAviParserInit(CdxParserT *parser)
{
    cdx_int32 result, ret = 0;
    CdxAviParserImplT *impl;

    CDX_FORCE_CHECK(parser);
    impl = CdxContainerOf(parser, CdxAviParserImplT, base);

    //open media file to parse file information
    result = AviOpen(impl);
    if(result < 0)
    {
        CDX_LOGE("Depack media file structure failed!");
        impl->mErrno = PSR_OPEN_FAIL;
        ret = -1;
        goto __exit;
    }

    //try to build index table, for process fast-forward, fast-reverse and timestamp of key frame
    result = AVIBuildIdx(impl);
    if(AVI_SUCCESS == result)
    {
        impl->hasIndex = 1;
    }
    else if(AVI_ERR_PART_INDEX_TABLE == result)
    {
        impl->hasIndex = 2;
    }
    else if(AVI_ERR_ABORT == result)
    {
        impl->hasIndex = 0;
        CDX_LOGE("AVI abort now!");
        impl->mErrno = PSR_OPEN_FAIL;
        ret = -1;
        goto __exit;
    }
    else if(result > 0)
    {
        impl->hasIndex = 0;
    }
    else
    {
        CDX_LOGE("Build avi index table failed!ret[%d].", result);
        impl->mErrno = PSR_OPEN_FAIL;
        ret = -1;
        goto __exit;
    }
    //release data chunk.
    AviReleaseDataChunkBuf((AviFileInT *)impl->privData);
    CDX_LOGI("read avi head finish.");
    impl->mErrno = PSR_OK;
    ret = 0;

__exit:
    pthread_mutex_lock(&impl->lock);
    impl->mStatus = CDX_AVI_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return ret;
}

static struct CdxParserOpsS aviParserOps =
{
    .control      = __CdxAviParserControl,
    .prefetch     = __CdxAviParserPrefetch,
    .read         = __CdxAviParserRead,
    .getMediaInfo = __CdxAviParserGetMediaInfo,
    .seekTo       = __CdxAviParserSeekTo,
    .attribute    = __CdxAviParserAttribute,
    .getStatus    = __CdxAviParserGetStatus,
    .close        = __CdxAviParserClose,
    .init         = __CdxAviParserInit
};

static CdxParserT *__CdxAviParserCreate(CdxStreamT *stream, cdx_uint32 flags)
{
    CdxAviParserImplT *impl;
    cdx_int32 result;

    if(flags > 0)
    {
        CDX_LOGI("Avi not support multi-stream yet...");
    }
    impl = AviInit(&result);
    CDX_FORCE_CHECK(impl);
    if(result < 0)
    {
        CDX_LOGE("Initiate avi file parser lib module error.");
        goto _err_open_media_file;
    }
    impl->base.ops = &aviParserOps;
    impl->stream = stream;

    pthread_mutex_init(&impl->lock, NULL);
    pthread_cond_init(&impl->cond, NULL);
    impl->mStatus = CDX_AVI_INITIALIZED;
    impl->mErrno = PSR_INVALID;
    return &impl->base;

_err_open_media_file:
    if(impl)
    {
        __CdxAviParserClose(&impl->base);
        impl = NULL;
    }

    return NULL;
}
static cdx_uint32 __CdxAviParserProbe(CdxStreamProbeDataT *probeData)
{
    if(probeData->len < 8)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }

    if(!AviProbe(probeData))
    {
        CDX_LOGE("AviProbe failed.");
        return 0;
    }

    return 100;
}
CdxParserCreatorT aviParserCtor =
{
    .create  = __CdxAviParserCreate,
    .probe = __CdxAviParserProbe
};
