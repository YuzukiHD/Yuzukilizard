/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviDepack.c
 * Description : Part of avi parser.
 * History :
 *
 */

//#define CONFIG_LOG_LEVEL 4
#define LOG_TAG "CdxAviDepack"
#include <CdxAviInclude.h>
#include <string.h>

cdx_int32 ConvertAudStreamidToArrayNum(CdxAviParserImplT *p, cdx_int32 streamId)
{
    cdx_int32   i;
    for(i = 0; i< p->hasAudio; i++)
    {
        if(p->audioStreamIndexArray[i] == streamId)
        {
            return i;
        }
    }
    return -1;
}
cdx_int32 ConvertSubStreamidToArrayNum(CdxAviParserImplT *p, cdx_int32 streamId)
{
    cdx_int32   i;
    for(i = 0; i< p->hasSubTitle; i++)
    {
        if(p->subtitleStreamIndexArray[i] == streamId)
        {
            return i;
        }
    }
    return -1;
}

cdx_int32 EvaluateStrhValid(AVIStreamHeaderT *pabsHdr)
{
    //cdx_int32   ret = AVI_ERR_PARA_ERR;

    if(0 == pabsHdr->dwRate || 0 == pabsHdr->dwScale)
    {
        logd("pabsHdr->dwRate=%u, pabsHdr->dwScale=%u", pabsHdr->dwRate, pabsHdr->dwScale);
        return AVI_ERR_PARA_ERR;
    }

    logv("dwSampleSize=%u, dwRate = %u, dwScale=%u, r/s=%u",
        pabsHdr->dwSampleSize, pabsHdr->dwRate, pabsHdr->dwScale, pabsHdr->dwRate/pabsHdr->dwScale);

    //STRH_DWSCALE_THRESH & MIN_BYTERATE may just empiric value, not seen in spec.
    #if 0
    if(pabsHdr->dwSampleSize > 0 || pabsHdr->dwScale < STRH_DWSCALE_THRESH)
    {
        if(pabsHdr->dwRate/pabsHdr->dwScale < MIN_BYTERATE)
        {
            logd("pabsHdr->dwSampleSize=%u, pabsHdr->dwScale=%u, %u",
                pabsHdr->dwSampleSize, pabsHdr->dwScale, pabsHdr->dwRate/pabsHdr->dwScale);
            ret = AVI_ERR_PARA_ERR;
        }
        else
        {
            ret = AVI_SUCCESS;
        }
    }
    else
    {
        ret = AVI_SUCCESS;
    }
    return ret;
    #endif

    return AVI_SUCCESS;
}
/*******************************************************************************
Function name: calc_avi_audio_chunk_pts
Description:

*******************************************************************************/
cdx_int32 CalcAviAudioChunkPts(AudioStreamInfoT *pAudioStreamInfo, cdx_int64 totalSize,
    cdx_int32 totalNum)

{
    cdx_int32   audioPts = 0;

    logv("totalSize=%lld, totalNum=%d, cbr=%d, "
         "sampleNumPerFrame=%d, sampleRate=%d, avgBytesPerSec=%d", totalSize, totalNum,
         pAudioStreamInfo->cbrFlag, pAudioStreamInfo->sampleNumPerFrame,
         pAudioStreamInfo->sampleRate, pAudioStreamInfo->avgBytesPerSec);

    if(pAudioStreamInfo->cbrFlag)
    {
        audioPts = totalSize * 1000/pAudioStreamInfo->avgBytesPerSec;
    }
    else
    {
        audioPts = (cdx_int32)((cdx_int64)totalNum * pAudioStreamInfo->sampleNumPerFrame * 1000
                                                     / pAudioStreamInfo->sampleRate);
    }
    return audioPts;
}

// parse time from buf. 00:00:02.000
cdx_int64 ParseTime(cdx_uint8 *buf)
{
    int h, min, s, ms;
    char str[] = "00:00:00.000";
    memcpy(str, buf, sizeof(str) - 1);
    if (sscanf(str, "%d:%d:%d.%d", &h, &min, &s, &ms) != 4)
        return -1;

    return (h*60*60*1000 + min*60*1000 + s*1000 + ms);
}

//calculate pts for divx bitmap subtitle. buf: [00:00:02.000-00:00:03.700]
cdx_int64 CalcAviSubChunkPts(cdx_uint8 *buf, cdx_int64 *duration)
{
    cdx_int64   startTime, endTime;

    //read start & end time
    if(buf[0] != '[' || buf[13] != '-' || buf[26] != ']')
    {
        CDX_LOGE("invalid time format.");
        return -1;
    }

    startTime = ParseTime(buf + 1);
    if(startTime < 0)
    {
        CDX_LOGE("wrong startTime.");
        return -1;
    }

    endTime   = ParseTime(buf + 14);
    if(endTime < 0)
    {
        CDX_LOGE("wrong endTime.");
        return -1;
    }

    *duration = endTime - startTime;
    return startTime;
}

/*******************************************************************************
Function name: calc_avi_chunk_audio_frame_num
Description:
    1.只针对VBR的音频才有意义，如果是CBR，是不知道一个chunk里面有多少audioframe的.
    如果是CBR，直接认为1 chunk == 1 audio frame,这是错的,但无所谓.
Parameters:
    nChunkSize: 就是chunk body size,不包括chunk head的长度.

Return:

Time: 2011/6/30
*******************************************************************************/
cdx_int32 CalcAviChunkAudioFrameNum(AudioStreamInfoT *pAudStrmInfo, cdx_int32 nChunkSize)
{
    cdx_int32 num;
    if(NULL == pAudStrmInfo)    //说明不是avi audio chunk.返回0就行了
    {
        return 0;
    }
    if(0 == pAudStrmInfo->cbrFlag)
    {
        if(pAudStrmInfo->nBlockAlign)
        {
            num = (nChunkSize + pAudStrmInfo->nBlockAlign - 1)/pAudStrmInfo->nBlockAlign;
        }
        else
        {
            num = 1;
        }
        if(0 == num)
        {
            CDX_LOGD("aud chunk size[%x]\n", nChunkSize);
            num = 1;
        }
        return num;
    }
    else    //如果是CBR，随便返回1就行了.
    {
        return 1;
    }
}

/*******************************************************************************
Function name: get_next_chunk_head
Time: 2009/5/26
*******************************************************************************/
cdx_int16 GetNextChunkHead(CdxStreamT *pf, AviChunkT *chunk, cdx_uint32 *length)
{
    if(NULL == pf || NULL == chunk)
    {
        CDX_LOGE("Bad para.");
        return AVI_ERR_PARA_ERR;
    }
    chunk->fcc = 0;
    chunk->flag = 0;
    chunk->length = 0;

    if (CdxStreamTell(pf) % 2)
    {
        if(CdxStreamSeek(pf, 1, STREAM_SEEK_CUR) < 0)
        {
            CDX_LOGE("Seek failed.");
            return AVI_ERR_FAIL;
        }
    }

    // read the FourCC code
    if(CdxStreamRead(pf, &chunk->fcc, 4) != 4)
    {
        CDX_LOGE("Read the FourCC code error.");
        return AVI_ERR_READ_FILE_FAIL;
    }

    // read the length
    if(CdxStreamRead(pf, &chunk->length, 4) != 4)
    {
        CDX_LOGE("Read the length error.");
        return AVI_ERR_READ_FILE_FAIL;
    }

    if (chunk->fcc == CDX_FORM_HEADER_RIFF)
    {
        // This is the beginning of the file
        if(length)
        {
            *length = 4;
        }
    }
    else if(chunk->fcc == CDX_LIST_HEADER_LIST)
    {
        // This is the starting of a list
        if(length)
        {
            *length = 4;
        }
    }
    else
    {
        // This is a data chunk
        if(length)
        {
            *length = chunk->length;
        }
    }

    return AVI_SUCCESS;
}

/*
*********************************************************************************************
*                           GET NEXT CHUNK
*
*Description: Get next chunk for parse the file structure.
*
* fcc + length + data
* if fcc is RIFF/LIST, data size is 4, otherwise it is chunk length.
*
*********************************************************************************************
*/
cdx_int16 GetNextChunk(CdxStreamT *fp, AviChunkT *chunk)
{
    //AVI_CHUNK   *chunk;
    cdx_uint32       length;
    if(GetNextChunkHead(fp, chunk, &length) < 0)
    {
        CDX_LOGE("GetNextChunkHead failed.");
        return -1;
    }

    //get the chunk data
    //chunk = &s->data_chunk;
    if(length)
    {
        if(length >= MAX_CHUNK_BUF_SIZE)
        {
            CDX_LOGV("Chunk size too large, maybe some error happened!");
            if(CdxStreamRead(fp, chunk->buffer, MAX_CHUNK_BUF_SIZE) != MAX_CHUNK_BUF_SIZE)
            {
                CDX_LOGE("file read error happened!");
                return -1;
            }
            if(CdxStreamSeek(fp, (cdx_uint32)(length - MAX_CHUNK_BUF_SIZE), STREAM_SEEK_CUR))
            {
                CDX_LOGE("file seek error happened!");
                return -1;
            }
            return AVI_ERR_CHUNK_OVERFLOW;
        }

        if(CdxStreamRead(fp, chunk->buffer, length) != (cdx_int32)length)
            // for RIFF/LIST, length=4; for data chunk, length=chunk->length
        {
            CDX_LOGE("file read error happened!");
            return -1;
        }
    }

    return 0;
}

//searchDirection: 1:forward 0:backward
cdx_int32 ReconfigAviReadContext(CdxAviParserImplT *p, cdx_uint32 vidTime,
    cdx_int32 searchDirection, cdx_int32 searchMode)
{
    cdx_int32   ret = AVI_EXCEPTION;
    AviFileInT  *aviIn = (AviFileInT *)p->privData;

    switch(aviIn->readmode)
    {
        case READ_CHUNK_SEQUENCE:
        {
            ret = ReconfigAviReadContextReadMode0(p, vidTime, searchDirection, searchMode);
            break;
        }
        case READ_CHUNK_BY_INDEX:
        {
            ret = ReconfigAviReadContextReadMode1(p, vidTime, searchDirection, searchMode);
            break;
        }
        default:
        {
            CDX_LOGV("fatal error! reconfig_avi_read_context().");
            ret = AVI_EXCEPTION;
            break;
        }
    }
    return ret;
}

/*
***********************************************************************************************
*
***********************************************************************************************
*/
AviStreamInfoT *GetNextStreamInfo(AviFileInT *s)
{
    CdxStreamT          *pf;
    AviChunkT           *chunk = NULL;
    AviStreamInfoT      *streamInfo;
    AVIStreamHeaderT    *streamHeader;
    cdx_int32           listEnd;
    cdx_int32           ret;
    cdx_int64           fpOffset;

    pf = s->fp;
    chunk = &s->dataChunk;

    // read forward until we get to the 'strl' list
    do
    {
        ret = GetNextChunk(s->fp, &s->dataChunk);
        if(ret < 0)
        {
            CDX_LOGE("GetNextChunk failed.");
            return NULL;
        }
    } while((chunk->fcc != CDX_LIST_HEADER_LIST) || strncmp((char *)chunk->buffer, "strl", 4) != 0);

    listEnd = CdxStreamTell(pf) + chunk->length - 4;// current pos point to  strl's end

    streamInfo = (AviStreamInfoT *)malloc(sizeof(AviStreamInfoT));
    memset(streamInfo, 0, sizeof(AviStreamInfoT));
    streamInfo->streamType = 'u';
    streamInfo->mediaType  = 'u';
    streamInfo->isODML     =  0;

    while((fpOffset = CdxStreamTell(pf)) < listEnd - 1)
    {
        if(fpOffset % 2)
        {
            fpOffset += 1;
            CdxStreamSeek(pf, 1, STREAM_SEEK_CUR);
        }
        ret = GetNextChunk(s->fp, &s->dataChunk);
        if(ret < 0)
        {
            CDX_LOGV("GetNextChunk failed.");
            break;
        }

        switch(chunk->fcc)
        {
            case CDX_CKID_STREAM_HEADER://strh
            {
                streamInfo->strh = (AviChunkT *)malloc(sizeof(AviChunkT));
                streamInfo->strh->fcc = chunk->fcc;
                streamInfo->strh->flag = chunk->flag;
                streamInfo->strh->length = chunk->length;
                streamInfo->strh->buffer =(cdx_char *)malloc((int)chunk->length);
                memcpy(streamInfo->strh->buffer,chunk->buffer,chunk->length);

                streamHeader = (AVIStreamHeaderT *)chunk->buffer;
                if(streamHeader->fccType == CDX_CKID_STREAM_TYPE_VIDEO)
                {
                    CDX_LOGV("xxxxxxxxxxx video xxxxxxxxxxxxxxxxxx");
                    streamInfo->mediaType = 'v';
                }
                else if(streamHeader->fccType == CDX_CKID_STREAM_TYPE_AUDIO)
                {
                    streamInfo->mediaType = 'a';
                }
                else if(streamHeader->fccType == CDX_STREAM_TYPE_TEXT)
                {
                    streamInfo->mediaType = 't';
                }
                break;
            }
            case CDX_CKID_STREAM_FORMAT://strf
            {
                streamInfo->strf = (AviChunkT *)malloc(sizeof(AviChunkT));
                streamInfo->strf->fcc = chunk->fcc;
                streamInfo->strf->flag = chunk->flag;
                streamInfo->strf->length = chunk->length;
                streamInfo->strf->buffer =(cdx_char *)malloc((int)chunk->length);
                memcpy(streamInfo->strf->buffer,chunk->buffer,chunk->length);

                if(streamInfo->mediaType == 'v') //may not really video
                {
                    BITMAPINFOHEADER    *bmpInfoHdr;
                    bmpInfoHdr = (BITMAPINFOHEADER *)streamInfo->strf->buffer;

                    if(bmpInfoHdr->biCompression == CDX_MMIO_FOURCC('D', 'X', 'S', 'B') ||
                       bmpInfoHdr->biCompression == CDX_MMIO_FOURCC('D', 'X', 'S', 'A'))
                    {
                        streamInfo->mediaType = 't';
                        //CDX_LOGD("xxxxxxxxxxxx subtitle.");
                    }
                }
                break;
            }
            case CDX_CKID_STREAM_HANDLER_DATA://strd
            {
                streamInfo->strd = (AviChunkT *)malloc(sizeof(AviChunkT));
                streamInfo->strd->fcc = chunk->fcc;
                streamInfo->strd->flag = chunk->flag;
                streamInfo->strd->length = chunk->length;
                streamInfo->strd->buffer =(cdx_char *)malloc((int)chunk->length);
                memcpy(streamInfo->strd->buffer,chunk->buffer,chunk->length);
                break;
            }
            case CDX_CKID_STREAM_NAME://strn
            {
                streamInfo->strn = (AviChunkT *)malloc(sizeof(AviChunkT));
                streamInfo->strn->fcc = chunk->fcc;
                streamInfo->strn->flag = chunk->flag;
                streamInfo->strn->length = chunk->length;
                streamInfo->strn->buffer =(cdx_char *)malloc((int)chunk->length);
                memcpy(streamInfo->strn->buffer,chunk->buffer,chunk->length);

                if(strncmp((char *)chunk->buffer, "video", 5) == 0)
                {
                    streamInfo->streamType = 'v';
                }
                else if(strncmp((char *)chunk->buffer, "audio", 5) == 0)
                {
                    streamInfo->streamType = 'a';
                }
                else if(strncmp((char *)chunk->buffer, "chapter", 7) == 0)
                {
                    streamInfo->streamType = 'c';
                }
                else if(strncmp((char *)chunk->buffer, "subtitle", 8) == 0)
                {
                    streamInfo->streamType = 's';
                }
                break;
            }

            case CDX_CKID_ODMLINDX://indx
            {
                cdx_uint32 off, i;
                streamInfo->indx = (AviSuperIndexChunkT *)malloc(sizeof(AviSuperIndexChunkT));
                memcpy((cdx_uint8*)streamInfo->indx, chunk->buffer, 0x18);
                streamInfo->indxTblOffset = fpOffset;
                //For 'indx', bIndexSubType must be 0, and here indx chunk
                //should point to index chunk
                if(streamInfo->indx->bIndexSubType
                    || streamInfo->indx->bIndexType != CDX_AVI_INDEX_OF_INDEXES
                    || streamInfo->indx->wLongsPerEntry != 4)//bIndexSubType?0|CDX_AVI_INDEX_2FIELD
                {
                    CDX_LOGW("special ODML index! IndexSubType[%x],IndexType[%x],"
                        " wLongsperEntry[%d]\n",
                        streamInfo->indx->bIndexSubType,
                        streamInfo->indx->bIndexType,
                        streamInfo->indx->wLongsPerEntry);
                    break;
                }

                //check file base, right now the supported file size is maximum 4G
//                if(stream_info->indx->BaseOffset_high)
//                    break;
//                if(s->filesize>0 && stream_info->indx->BaseOffset_low >= s->filesize)
//                    break;
                if(s->fileSize>0 && ((cdx_uint64)streamInfo->indx->baseOffsetHigh << 32
                        | streamInfo->indx->baseOffsetLow) >= s->fileSize)
                {
                    CDX_LOGW("indx offset >= filesize ? I don't know why write this,"
                        " if happen, check code!\n");
                    break;
                }

                if(streamInfo->indx->nEntriesInUse > MAX_IX_ENTRY_NUM)
                {
                    CDX_LOGW("nEntriesInUse[%d] > MAX_IX_ENTRY_NUM[128]",
                        streamInfo->indx->nEntriesInUse);
                    streamInfo->indx->nEntriesInUse = MAX_IX_ENTRY_NUM;
                }

                streamInfo->isODML = 1;
                off = 0x18;
                for(i=0; i<streamInfo->indx->nEntriesInUse; i++)
                {
                    if(streamInfo->indx->bIndexType == CDX_AVI_INDEX_OF_INDEXES)
                    {
                        streamInfo->indx->aIndex[i] =
                            (AviSuperIndexEntryT *)malloc(sizeof(AviSuperIndexEntryT));
                        memcpy(streamInfo->indx->aIndex[i], chunk->buffer + off,
                            sizeof(AviSuperIndexEntryT));
                        off += sizeof(AviSuperIndexEntryT);
                    }
                }
                break;
            }

            default:
            {
                break;
            }
        }
    }

    return streamInfo;
}

/*******************************************************************************

*******************************************************************************/
cdx_int32 GetIdx1Info(AviFileInT *aviIn)
{
    FOURCC      mFcc;
    cdx_int32   mLength = 0;

    aviIn->hasIdx1 = 0;
    if(CdxStreamSeek(aviIn->fp, (cdx_int64)aviIn->moviEnd, STREAM_SEEK_SET))
    {
        //seek to movi end failed
        aviIn->idx1Total = 0;
        CDX_LOGE("seek to movi end failed.");
        return AVI_ERR_GET_INDEX_ERR;
    }

    int i = 0;
    do
    {
        if(AVI_SUCCESS != GetNextChunkHead(aviIn->fp, &aviIn->dataChunk, (cdx_uint32 *)&mLength))
        {
            aviIn->idx1Total = 0;
            CDX_LOGE("get next chunk head failed.");
            return AVI_ERR_GET_INDEX_ERR;
        }
        aviIn->idx1Start = CdxStreamTell(aviIn->fp);
        if(CdxStreamSeek(aviIn->fp, aviIn->dataChunk.length, STREAM_SEEK_CUR))
        {
            aviIn->idx1Total = 0;
            CDX_LOGE("seek failed.");
            return AVI_ERR_GET_INDEX_ERR;
        }
        mFcc = aviIn->dataChunk.fcc;
    }
    while(mFcc != CDX_CKID_AVI_NEWINDEX && (i++ < 1000));

    if(mFcc == CDX_CKID_AVI_NEWINDEX)
    {
        //check if index length is valid
        if((CdxStreamTell(aviIn->fp) - aviIn->idx1Start) == (cdx_int64)mLength)
        {
            AviIndexEntryT  mIdx;
            cdx_int32       indexType = 0;
            cdx_uint32      tmpTag, tmpSize;

            aviIn->idx1Total = mLength / sizeof(AviIndexEntryT);

            // reset the movi_start just in case some of the indexes are
            // measured from the beginning of the file
            if(CdxStreamSeek(aviIn->fp, (cdx_int64)aviIn->idx1Start, STREAM_SEEK_SET))
            {
                aviIn->idx1Total = 0;
                CDX_LOGE("seek failed.");
                return AVI_ERR_GET_INDEX_ERR;
            }
            while(1)
            {
                if(CdxStreamRead(aviIn->fp, &mIdx, sizeof(AviIndexEntryT)) !=
                    sizeof(AviIndexEntryT))
                {
                    aviIn->idx1Total = 0;
                    CDX_LOGE("read failed.");
                    return AVI_ERR_GET_INDEX_ERR;
                }
                if(mIdx.ckid != CDX_LIST_TYPE_AVI_RECORD)
                {
                    break;
                }
                else
                {
                    CDX_LOGV("idx1 has 'rec ' structure, skip to next index entry\n");
                }
            }
            if(CdxStreamSeek(aviIn->fp, (cdx_int64)mIdx.dwChunkOffset, STREAM_SEEK_SET))
                //dwChunkOffset: from file start
            {
                aviIn->idx1Total = 0;
                CDX_LOGE("seek failed.");
                return AVI_ERR_GET_INDEX_ERR;
            }
            if(CdxStreamRead(aviIn->fp, &tmpTag, 4) != 4)
            {
                aviIn->idx1Total = 0;
                CDX_LOGE("read failed.");
                return AVI_ERR_GET_INDEX_ERR;
            }
            if(CdxStreamRead(aviIn->fp, &tmpSize, 4) != 4)
            {
                aviIn->idx1Total = 0;
                CDX_LOGE("read failed.");
                return AVI_ERR_GET_INDEX_ERR;
            }
            if((tmpTag == mIdx.ckid && tmpSize == mIdx.dwChunkLength)
                || (tmpTag == mIdx.ckid && tmpTag == CDX_LIST_HEADER_LIST))
            {
                indexType = 1;
            }
            else
            {
                if(CdxStreamSeek(aviIn->fp, (cdx_int64)(mIdx.dwChunkOffset+aviIn->moviStart-4),
                    STREAM_SEEK_SET))//dwChunkOffset: from movi
                {
                    aviIn->idx1Total = 0;
                    CDX_LOGE("seek failed.");
                    return AVI_ERR_GET_INDEX_ERR;
                }
                if(CdxStreamRead(aviIn->fp, &tmpTag, 4) != 4)
                {
                    aviIn->idx1Total = 0;
                    CDX_LOGE("read failed.");
                    return AVI_ERR_GET_INDEX_ERR;
                }
                if(CdxStreamRead(aviIn->fp, &tmpSize, 4) != 4 )
                {
                    aviIn->idx1Total = 0;
                    CDX_LOGE("read failed.");
                    return AVI_ERR_GET_INDEX_ERR;
                }
                if((tmpTag == mIdx.ckid /*&& tmp_size ==m_idx.dwChunkLength*/)
                    || (tmpTag == mIdx.ckid && tmpTag == CDX_LIST_HEADER_LIST))
                {
                    indexType = 2;
                }
            }
            if (indexType == 0)
            {
                aviIn->moviStart = 0;
            }
            else if(indexType == 1)
            {
                aviIn->ckBaseOffset = 0;
            }
            else if(indexType == 2)
            {
                aviIn->ckBaseOffset = aviIn->moviStart - 4;
            }
            if(aviIn->drmFlag)
            {
                aviIn->ckBaseOffset -= 18;
            }

            aviIn->hasIdx1 = 1;
        }
        else
        {
            aviIn->idx1Total = 0;
            CDX_LOGE("idx1 length is invalid.");
        }
    }
    return AVI_SUCCESS;
}

/*
***********************************************************************************************
*                       RELEASE THE MEMORY RESOURCE OF A CHUNK
*
*Description: Release the memory resource of a chunk.
*
*Arguments  : chunk     the pointer to the chunk buffer.
*
*Return     : none.
***********************************************************************************************
*/
void ReleaseChunk(AviChunkT *chunk)
{
    if(chunk)
    {
        if(chunk->buffer)
        {
            free(chunk->buffer);
            chunk->buffer = NULL;
        }
        free(chunk);
    }
}

/*
************************************************************************************************
*                       RELEASE THE MEMORY RESOURCE OF A BITSTREAM INFORMATION
*
*Description: Release the memory resource of a bitstream information.
*
*Arguments  : SInfo     the pointer to the bitstream information.
*
*Return     : none.
************************************************************************************************
*/
void ReleaseStreamInfo(AviStreamInfoT *sInfo)
{
    if(sInfo)
    {
        if(sInfo->strh)
        {
            ReleaseChunk(sInfo->strh);
            sInfo->strh = NULL;
        }
        if(sInfo->strf)
        {
            ReleaseChunk(sInfo->strf);
            sInfo->strf = NULL;
        }
        if(sInfo->strd)
        {
            ReleaseChunk(sInfo->strd);
            sInfo->strd = NULL;
        }
        if(sInfo->strn)
        {
            ReleaseChunk(sInfo->strn);
            sInfo->strn = NULL;
        }
        if(sInfo->indx)
        {
            cdx_uint32 i;
            for(i = 0; i < sInfo->indx->nEntriesInUse; i++)
            {
                if(sInfo->indx->aIndex[i])
                {
                    free(sInfo->indx->aIndex[i]);
                    sInfo->indx->aIndex[i] = NULL;
                }
            }
            free(sInfo->indx);
            sInfo->indx = NULL;
        }

        free(sInfo);
    }
}

/*
************************************************************************************************
*                       OPEN AVI struct cdx_stream_info TO READ
*
*Description: open avi file to read.
*
*Arguments  : avi_file      the pointer to avi file informaiton, for return;
*             file_name     avi file path;
*
*Return     : open avi file result;
*               =   0   open avi file successed;
*               <   0   open avi file failed;
************************************************************************************************
*/
cdx_int16 AVIReaderOpenFile(CdxAviParserImplT *impl)
{
    cdx_int32           i, ret;
    AviChunkT           *chunk = NULL;
    MainAVIHeaderT      *avih;
    AviFileInT          *aviFile;

    aviFile = (AviFileInT *)impl->privData;
    chunk = &aviFile->dataChunk;

    // initialize
    aviFile->fp = NULL;
    aviFile->audFp = NULL;
    aviFile->idxFp = NULL;
    aviFile->avih = NULL;
    aviFile->nStream = 0;
    for(i = 0; i < MAX_STREAM; i++)
    {
        aviFile->sInfo[i] = NULL;
    }
    aviFile->moviStart = 0;
    aviFile->moviEnd = 0;
    aviFile->idx1Buf = NULL;
    aviFile->idx1Total = 0;
    aviFile->frameIndex = 0;
    aviFile->isNetworkStream = CdxStreamIsNetStream(impl->stream);
    // open the media file
    aviFile->fp = impl->stream;

    // verify that this is an AVI file
    ret = GetNextChunk(aviFile->fp, &aviFile->dataChunk);
    if((ret < 0) || (chunk->fcc != CDX_FORM_HEADER_RIFF) || (strncmp((char *)chunk->buffer,
        "AVI ", 4) != 0))
    {
        CdxStreamClose(aviFile->fp);
        aviFile->fp = NULL;
        CDX_LOGE("error...ret(%d) buf(%.4s)", ret, (ret >= 0) ? chunk->buffer : "null");
        return AVI_ERR_FILE_FMT_ERR;
    }

    // read forward until we get to the 'hdrl' list
    do
    {
        ret = GetNextChunk(aviFile->fp, &aviFile->dataChunk);
        if(ret < 0)
        {
            CdxStreamClose(aviFile->fp);
            aviFile->fp = NULL;
            CDX_LOGE("Get hdrl list failed...");
            return AVI_ERR_FILE_FMT_ERR;
        }
    } while(chunk->fcc != CDX_LIST_HEADER_LIST || strncmp((char*)chunk->buffer, "hdrl", 4) != 0);

    // read the 'avih' chunk
    ret = GetNextChunk(aviFile->fp, &aviFile->dataChunk);
    if ((ret < 0) || (chunk->fcc != CDX_CKID_AVI_MAINHDR))
    {
        CdxStreamClose(aviFile->fp);
        aviFile->fp = NULL;
        CDX_LOGE("Get avih failed...");
        return AVI_ERR_FILE_FMT_ERR;
    }

    // read 'avih' and extract some information
    aviFile->avih = (AviChunkT *)malloc(sizeof(AviChunkT));
    aviFile->avih->fcc = chunk->fcc;
    aviFile->avih->flag = chunk->flag;
    aviFile->avih->length = chunk->length;
    aviFile->avih->buffer = malloc(chunk->length);
    memcpy(aviFile->avih->buffer,chunk->buffer,chunk->length);

    avih = (MainAVIHeaderT *)chunk->buffer;
    aviFile->nStream = avih->dwStreams;
    if (aviFile->nStream > MAX_STREAM)
    {
        aviFile->nStream = MAX_STREAM;
    }

    //get file size
    aviFile->fileSize = CdxStreamSize(aviFile->fp);

    // read in the stream info for all the streams
    for(i = 0; i < aviFile->nStream; i++)
    {
        aviFile->sInfo[i] = GetNextStreamInfo(aviFile);

        if (!aviFile->sInfo[i])
        {
            return AVI_ERR_FILE_FMT_ERR;
        }

        CDX_LOGV("xxxxxxxxxxxxxx aviFile->sInfo[%d].mediaType(%d) ", i,
            aviFile->sInfo[i]->mediaType);
        if(aviFile->sInfo[i]->isODML)
        {
            aviFile->hasIndx = 1;
            CDX_LOGV("The avi file is Open-DML Index format!");
        }
    }

    return 0;
}

/*
**********************************************************************************************
*                       CLOSE struct cdx_stream_info
*
*Description: Close file, release some memory resource.
*
*Arguments  : avi_file  the pointer to the avi file information.
*
*Return     : none.
**********************************************************************************************
*/
cdx_int16 AviReaderCloseFile(AviFileInT *aviFile)
{
    cdx_int32   i;

    if(aviFile)
    {
        // release the avih chunks
        if (aviFile->avih)
        {
            ReleaseChunk(aviFile->avih);
            aviFile->avih = NULL;
        }

        // release the stream info
        for(i = 0; i < MAX_STREAM; i++)
        {
            ReleaseStreamInfo(aviFile->sInfo[i]);
            aviFile->sInfo[i] = NULL;
        }

        // release the idx1 buffer
        if(aviFile->idx1Buf)
        {
            free(aviFile->idx1Buf);
            aviFile->idx1Buf = NULL;
        }
        //
        if(READ_CHUNK_BY_INDEX == aviFile->readmode
            && USE_IDX1 == aviFile->idxStyle)
        {
            DeinitialPsrIdx1TableReader(&aviFile->vidIdx1Reader);
            DeinitialPsrIdx1TableReader(&aviFile->audIdx1Reader);
        }
        else if(READ_CHUNK_BY_INDEX == aviFile->readmode
            && USE_INDX == aviFile->idxStyle)
        {
            DeinitialPsrIndxTableReader(&aviFile->vidIndxReader);
            DeinitialPsrIndxTableReader(&aviFile->audIndxReader);
        }
        // simply close the file
        if(aviFile->fp)
        {
            CdxStreamClose(aviFile->fp);
            aviFile->fp = NULL;
        }
        /*if(aviFile->audFp)
        {
            CdxStreamClose(aviFile->audFp);
            aviFile->audFp = NULL;
        }
        if(aviFile->idxFp)
        {
            CdxStreamClose(aviFile->idxFp);
            aviFile->idxFp = NULL;
        }*/
    }

    return 0;
}

/*
**********************************************************************************************
*                       GET BITSTREAM INFORMATION
*
*Description: Get the bitstream information from the bitstream information buffer.

*Arguments  : avi_file      the avi file information handle;
*             SInfo         the pointer to the bitstream information, for return;
*             index         the index of the bitstream in the bitstream information buffer.
*
*Return     : get bitstream information result;
*               =   0   get bitstream information successed;
*               <   0   get bitstream information failed.
**********************************************************************************************
*/
cdx_int16 AviReaderGetStreamInfo(AviFileInT *aviFile, AviStreamInfoT *sInfo, cdx_int32 index)
{
    if(index >= aviFile->nStream)
    {
        CDX_LOGE("Bad param.");
        return -1;
    }

    *sInfo = *(aviFile->sInfo[index]);
    return 0;
}

/*******************************************************************************
Function name: set_FILE_PARSER_video_info
Description:

Parameters:

Return:

Time: 2011/3/26
*******************************************************************************/
cdx_int32 SetFileParserVideoInfo(CdxAviParserImplT *p)
{
    AVIStreamHeaderT    *strmHdr;
    BITMAPINFOHEADER    *bmpInfoHdr;
    AviFileInT          *aviIn = (AviFileInT *)p->privData;
    //AVI_FILE_IN         *avi_in = (AVI_FILE_IN *)p;
    if(p->hasVideo)
    {
        MainAVIHeaderT       *avih;
        strmHdr = (AVIStreamHeaderT *)aviIn->sInfo[p->videoStreamIndex]->strh->buffer;
        bmpInfoHdr = (BITMAPINFOHEADER *)aviIn->sInfo[p->videoStreamIndex]->strf->buffer;

        avih = (MainAVIHeaderT *)aviIn->avih->buffer;
#if 0
        p->vFormat.nFrameWidth = (CDX_U16)avih->dwWidth;
        p->vFormat.nFrameHeight = (CDX_U16)avih->dwHeight;
        p->total_frames = avih->dwTotalFrames;
#else
        p->aviFormat.vFormat.nWidth  = bmpInfoHdr->biWidth;
        p->aviFormat.vFormat.nHeight = bmpInfoHdr->biHeight;
        p->totalFrames = strmHdr->dwLength;
#endif
        CDX_LOGV("avih->dwTotalFrames[%x],strm_hdr->dwLength[%x]\n", avih->dwTotalFrames,
            strmHdr->dwLength);
        if(avih->dwTotalFrames != strmHdr->dwLength)
        {
            CDX_LOGV("avih->dwTotalFrames[%x] != strm_hdr->dwLength[%x]\n",
                avih->dwTotalFrames, strmHdr->dwLength);
        }

        if(avih->dwMicroSecPerFrame
            && ((avih->dwMicroSecPerFrame >= 16000)
            && (avih->dwMicroSecPerFrame <= 200*1000)))//5-60FPS
        {
            //calculate frame rate with parameter from avi main header
            p->aviFormat.vFormat.nFrameRate = (cdx_uint16)(1000000 / avih->dwMicroSecPerFrame *
                FRAME_RATE_BASE);
            p->aviFormat.vFormat.nFrameRate += (cdx_uint16)((1000000%avih->dwMicroSecPerFrame)*
                FRAME_RATE_BASE / avih->dwMicroSecPerFrame);
            p->aviFormat.nMicSecPerFrame = avih->dwMicroSecPerFrame;
        }
        else if((strmHdr->dwScale && strmHdr->dwRate)
            && ((strmHdr->dwRate / strmHdr->dwScale) >= 1)
            && ((strmHdr->dwRate / strmHdr->dwScale) <= 60))
        {
            //calculate frame rate with parameter from video bitstream header
            //avih->dwMicroSecPerFrame
            p->aviFormat.nMicSecPerFrame =
                (cdx_int64)((cdx_int64)strmHdr->dwScale* 1000000) / strmHdr->dwRate;
            p->aviFormat.vFormat.nFrameRate =
                (cdx_int64)((cdx_int64)strmHdr->dwRate* FRAME_RATE_BASE) / strmHdr->dwScale;
        }
        else
        {
            CDX_LOGD("xxxxxxxxxxxx avih->dwMicroSecPerFrame=%u", avih->dwMicroSecPerFrame);
            p->aviFormat.nMicSecPerFrame = avih->dwMicroSecPerFrame;
            p->aviFormat.vFormat.nFrameRate = (cdx_uint16)(1000000 / avih->dwMicroSecPerFrame *
                FRAME_RATE_BASE);
            p->aviFormat.vFormat.nFrameRate += (cdx_uint16)((1000000%avih->dwMicroSecPerFrame)*
                FRAME_RATE_BASE / avih->dwMicroSecPerFrame);
            //set default value to try it
            //p->aviFormat.nMicSecPerFrame = /*avih->dwMicroSecPerFrame =*/ 1000000 / 25;
            //p->aviFormat.vFormat.nFrameRate = 25 * FRAME_RATE_BASE;
        }
        //p->vFormat.nMicSecPerFrame = avih->dwMicroSecPerFrame;

        p->nVidPtsOffset = (cdx_uint32)((cdx_uint64)strmHdr->dwStart * FRAME_RATE_BASE * 1000 /
            (p->aviFormat.vFormat.nFrameRate));
        if(0 != p->nVidPtsOffset)
        {
            CDX_LOGV("this file nVidPtsBaseTime[%d]ms\n", p->nVidPtsOffset);
        }
        switch (strmHdr->fccHandler)
        {
            case CDX_MMIO_FOURCC('m','p','g','4'):            //MSMPEGV1
            {
                CDX_LOGV("video bitstream type: DIVX1!");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MSMPEG4V1;
                break;
            }
            case CDX_MMIO_FOURCC('d','i','v','2'):            //MSMPEGV2
            case CDX_MMIO_FOURCC('D','I','V','2'):
            case CDX_MMIO_FOURCC('M','P','4','2'):
            {
                CDX_LOGV("video bitstream type: DIVX2!");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MSMPEG4V2;
                break;
            }
            case CDX_MMIO_FOURCC('d','i','v','3'):
            case CDX_MMIO_FOURCC('D','I','V','3'):
            case CDX_MMIO_FOURCC('d','i','v','4'):
            case CDX_MMIO_FOURCC('D','I','V','4'):
            case CDX_MMIO_FOURCC('M','P','4','3'):
            {
                CDX_LOGV("video bitstream type: DIVX3!");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX3;
                break;
            }

            case CDX_MMIO_FOURCC('d','i','v','5'):
            case CDX_MMIO_FOURCC('D','I','V','5'):
            {
                switch (bmpInfoHdr->biCompression)
                {
                    case CDX_MMIO_FOURCC('d','i','v','5'):
                    case CDX_MMIO_FOURCC('D','I','V','5'):
                    {
                        CDX_LOGV("video bitstream type: DIVX5!");
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX5;
                        break;
                    }
                    default:
                    {
                        CDX_LOGV("video bitstream type(%x, %x): UNKNOWNVBS!\n", \
                            strmHdr->fccHandler, bmpInfoHdr->biCompression);
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
                        break;
                    }
                }
                break;
            }
            case CDX_MMIO_FOURCC('C','A','V','S'):
            {
                CDX_LOGV("video bitstream type: CAVS!");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_AVS;
                break;
            }
            case CDX_MMIO_FOURCC('d','i','v','x'):
            case CDX_MMIO_FOURCC('D','I','V','X'):
            case CDX_MMIO_FOURCC('d','x','5','0'):
            case CDX_MMIO_FOURCC('D','X','5','0'):
            case CDX_MMIO_FOURCC('x','v','i','d'):
            case CDX_MMIO_FOURCC('X','V','I','D'):
            case CDX_MMIO_FOURCC('M','S','V','C'):
            {
                switch (bmpInfoHdr->biCompression)
                {
                    case CDX_MMIO_FOURCC('d','i','v','x'):
                    case CDX_MMIO_FOURCC('D','I','V','X'):
                    {
                        CDX_LOGV("video bitstream type: DIVX4!");
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX4;
                        break;
                    }
                    case CDX_MMIO_FOURCC('d','x','5','0'):
                    case CDX_MMIO_FOURCC('D','X','5','0'):
                    {
                        CDX_LOGV("video bitstream type: DIVX5!");
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX5;
                        break;
                    }
                    case CDX_MMIO_FOURCC('x','v','i','d'):
                    case CDX_MMIO_FOURCC('X','V','I','D'):
                    case CDX_MMIO_FOURCC('F','M','P','4'):
                    {
                        CDX_LOGV("video bitstream type: XVID!");
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_XVID;
                        break;
                    }
                    case CDX_MMIO_FOURCC('d','i','v','3'):
                    case CDX_MMIO_FOURCC('D','I','V','3'):
                    {
                        CDX_LOGV("video bitstream type: DIVX3!");
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX3;
                        break;
                    }

                    default:
                    {
                        CDX_LOGV("video bitstream type(%x, %x): UNKNOWNVBS, try it with xvid!\n", \
                                                strmHdr->fccHandler, bmpInfoHdr->biCompression);
                        if(strmHdr->fccHandler == CDX_MMIO_FOURCC('M','S','V','C'))
                        {
                            p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
                        }
                        else
                        {
                            p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG4;
                        }
                        break;
                    }
                }
                break;
            }
            case CDX_MMIO_FOURCC('M','J','P','G'):
            case CDX_MMIO_FOURCC('m','j','p','g'):
            {
                CDX_LOGV("video bitstream type: MJPEG!");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MJPEG;
                break;
            }

            //case CDX_MMIO_FOURCC('M','P','4','2'):
            //case CDX_MMIO_FOURCC('m','p','4','2'):
            case CDX_MMIO_FOURCC('f','m','p','4'):
            case CDX_MMIO_FOURCC('F','M','P','4'):
            {
                CDX_LOGV("video bitstream type: XVID.");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_XVID;
                break;
            }

            case CDX_MMIO_FOURCC('h','2','6','3'):
            case CDX_MMIO_FOURCC('H','2','6','3'):
            {
                CDX_LOGV("video bitstream type: H263.");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_H263;
                break;
            }

            case CDX_MMIO_FOURCC('f', 'l', 'v', '1'):
            case CDX_MMIO_FOURCC('F', 'L', 'V', '1'):
            {
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_SORENSSON_H263;
                break;
            }

            case CDX_MMIO_FOURCC('x','2','6','4'):
            case CDX_MMIO_FOURCC('X','2','6','4'):
            case CDX_MMIO_FOURCC('h','2','6','4'):
               case CDX_MMIO_FOURCC('H','2','6','4'):
               case CDX_MMIO_FOURCC('A','V','C','1'):
               case CDX_MMIO_FOURCC('a','v','c','1'):
            {
                CDX_LOGV("video bitstream type: H264.");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
                CDX_LOGV("aviIn->sInfo[p->videoStreamIndex]->strf->length(%u)",
                    aviIn->sInfo[p->videoStreamIndex]->strf->length);
                if(aviIn->sInfo[p->videoStreamIndex]->strf->length <= 40)
                    break;
                aviIn->vopPrivInf = malloc(aviIn->sInfo[p->videoStreamIndex]->strf->length - 40);
                if(!aviIn->vopPrivInf)
                {
                    CDX_LOGV("Request memory failed!");
                    return AVI_ERR_FILE_FMT_ERR;
                }

                memcpy(aviIn->vopPrivInf,
                        aviIn->sInfo[p->videoStreamIndex]->strf->buffer + 40,
                        aviIn->sInfo[p->videoStreamIndex]->strf->length - 40);
                p->aviFormat.vFormat.pCodecSpecificData = aviIn->vopPrivInf;
                p->aviFormat.vFormat.nCodecSpecificDataLen =
                    aviIn->sInfo[p->videoStreamIndex]->strf->length - 40;
                CDX_LOGV("pCodecSpecificData(%p),nCodecSpecificDataLen(%u)",
                                p->aviFormat.vFormat.pCodecSpecificData,
                                p->aviFormat.vFormat.nCodecSpecificDataLen);
                break;
            }

            case CDX_MMIO_FOURCC('w','m','v','1'):
            case CDX_MMIO_FOURCC('W','M','V','1'):
            {
                CDX_LOGV("video bitstream type: WMV1!");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_WMV1;
                break;
            }

            case CDX_MMIO_FOURCC('w','m','v','2'):
            case CDX_MMIO_FOURCC('W','M','V','2'):
            {
                CDX_LOGV("video bitstream type: WMV2!");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_WMV2;
                aviIn->vopPrivInf = malloc(8);
                if(!aviIn->vopPrivInf)
                {
                    CDX_LOGV("Request memory failed!");
                    return AVI_ERR_FILE_FMT_ERR;
                }
                memcpy(aviIn->vopPrivInf, aviIn->sInfo[p->videoStreamIndex]->strf->buffer + 40, 4);
                p->aviFormat.vFormat.pCodecSpecificData = aviIn->vopPrivInf;
                p->aviFormat.vFormat.nCodecSpecificDataLen = 4;
                break;
            }

            case CDX_MMIO_FOURCC('w','m','v','3'):
            case CDX_MMIO_FOURCC('W','M','V','3'):
            {
                CDX_LOGV("video bitstream type: WMV3/VC1!");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_WMV3;
                aviIn->vopPrivInf = malloc(8);
                if(!aviIn->vopPrivInf)
                {
                    CDX_LOGV("Request memory failed!");
                    return AVI_ERR_FILE_FMT_ERR;
                }

                memcpy(aviIn->vopPrivInf,
                        aviIn->sInfo[p->videoStreamIndex]->strf->buffer + 40, 4);
                p->aviFormat.vFormat.pCodecSpecificData = aviIn->vopPrivInf;
                p->aviFormat.vFormat.nCodecSpecificDataLen = 4;
                break;
            }
//#if(EPDK_ARCH == EPDK_ARCH_SUNI)
//#elif(EPDK_ARCH == EPDK_ARCH_SUNII)
            case CDX_MMIO_FOURCC('w','v','c','1'):
            case CDX_MMIO_FOURCC('W','V','C','1'):
            {
                CDX_LOGV("video bitstream type: VC1!");
               // p->vFormat.video_bs_src = 1;
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_WMV3;
                if(aviIn->sInfo[p->videoStreamIndex]->strf->length <= 40)
                    break;
                aviIn->vopPrivInf = malloc(aviIn->sInfo[p->videoStreamIndex]->strf->length - 40);
                if(!aviIn->vopPrivInf)
                {
                    CDX_LOGV("Request memory failed!");
                    return AVI_ERR_FILE_FMT_ERR;
                }

                memcpy(aviIn->vopPrivInf,
                        aviIn->sInfo[p->videoStreamIndex]->strf->buffer + 40,
                        aviIn->sInfo[p->videoStreamIndex]->strf->length - 40);
                p->aviFormat.vFormat.pCodecSpecificData = aviIn->vopPrivInf;
                p->aviFormat.vFormat.nCodecSpecificDataLen =
                    aviIn->sInfo[p->videoStreamIndex]->strf->length - 40;
                break;
            }

            case CDX_MMIO_FOURCC('m','p','g','2'):
            case CDX_MMIO_FOURCC('M','P','G','2'):
            case CDX_MMIO_FOURCC('m','p','e','g'):
            case CDX_MMIO_FOURCC('M','P','E','G'):
            {
                CDX_LOGV("video bitstream type: MPEG2!");
                p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
                break;
            }
            //case CDX_MMIO_FOURCC('I','V','5','0'): // Intel/Ligos Indeo Interactive 5.0 (IV50)
            //{
            //    CDX_LOGW("codec not support IV50 yet.");
            //}

            //Cinepack, not support
            case CDX_MMIO_FOURCC('d','i','v','c'):
            case CDX_MMIO_FOURCC('D','I','V','C'):
//            case CDX_MMIO_FOURCC('M','P','4','2'):   //MSMPEGV2
            case CDX_MMIO_FOURCC('m','p','4','2'):
            case CDX_MMIO_FOURCC('f','f','d','s'):
            default:
            {
                //stream handler is invalid, try to check the compression
                switch (bmpInfoHdr->biCompression)
                {
                    case CDX_MMIO_FOURCC('x','v','i','d'):
                    case CDX_MMIO_FOURCC('X','V','I','D'):
                    case CDX_MMIO_FOURCC('m','p','4','v'):
                    case CDX_MMIO_FOURCC('M','P','4','V'):
                    {
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_XVID;
                        break;
                    }
                    case CDX_MMIO_FOURCC('d','x','5','0'):
                    case CDX_MMIO_FOURCC('D','X','5','0'):
                    {
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX5;
                        break;
                    }
                    case CDX_MMIO_FOURCC('d','i','v','x'):
                    case CDX_MMIO_FOURCC('D','I','V','X'):
                    {
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX4;
                        break;
                    }

                    case CDX_MMIO_FOURCC('d','i','v','3'):
                    case CDX_MMIO_FOURCC('D','I','V','3'):
                    case CDX_MMIO_FOURCC('M','P','4','3'):
                    {
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX3;
                        break;
                    }

                    case CDX_MMIO_FOURCC('x','2','6','4'):
                    case CDX_MMIO_FOURCC('X','2','6','4'):
                    case CDX_MMIO_FOURCC('h','2','6','4'):
                    case CDX_MMIO_FOURCC('H','2','6','4'):
                    case CDX_MMIO_FOURCC('A','V','C','1'):
                    case CDX_MMIO_FOURCC('a','v','c','1'):
                    {
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
                       if(aviIn->sInfo[p->videoStreamIndex]->strf->length<=40)
                            break;
                        aviIn->vopPrivInf =
                            malloc(aviIn->sInfo[p->videoStreamIndex]->strf->length-40);
                        if(!aviIn->vopPrivInf)
                        {
                            CDX_LOGV("Request memory failed!");
                            return AVI_ERR_FILE_FMT_ERR;
                        }

                        memcpy(aviIn->vopPrivInf,
                                aviIn->sInfo[p->videoStreamIndex]->strf->buffer + 40,
                                aviIn->sInfo[p->videoStreamIndex]->strf->length-40);
                        p->aviFormat.vFormat.pCodecSpecificData = aviIn->vopPrivInf;
                        p->aviFormat.vFormat.nCodecSpecificDataLen =
                            aviIn->sInfo[p->videoStreamIndex]->strf->length-40;
                        break;
                    }
                    case CDX_MMIO_FOURCC('v','p','6','2'):
                    case CDX_MMIO_FOURCC('V','P','6','2'):
                    {
                        CDX_LOGV("video bitstream type: VP62.");
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_VP6;
                        break;
                    }
                    case CDX_MMIO_FOURCC('M','P','E','G'):
                    case 0x10000002:
                    // format 0x10000002: mpeg 2; ffmpeg-0.4.9-pre1.ta[CODEC_ID_MPEG2VIDEO].
                    // 20140903 add.
                    {
                        CDX_LOGV("video bitstream type: MPEG2!");
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
                        break;
                    }
                    case CDX_MMIO_FOURCC('C','A','V','S'):
                    {
                        CDX_LOGV("video bitstream type: CAVS!");
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_AVS;
                        break;
                    }
                    default:
                    {
                        CDX_LOGW("video bitstream type(%x, %x): UNKNOWNVBS!\n",\
                            strmHdr->fccHandler, bmpInfoHdr->biCompression);
                        p->aviFormat.vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
                        //p->hasVideo = 0;//* if set 0, should discard video frame when prefetch.
                        break;
                    }
                }
                break;
            }
        }
    }
    return AVI_SUCCESS;
}

/*static int split_xiph_header(unsigned char *codec_priv, int codec_priv_size,
                        unsigned char **extradata, int *extradata_size)
{
    #define AV_RN16(a) (*((unsigned short *) (a)))

    unsigned char  *header_start[3];
    unsigned int header_len[3];
    int i;
    int overall_len;
    unsigned char  *OggHeader;
    unsigned char  lacing_fill, lacing_fill_0, lacing_fill_1, lacing_fill_2;

    if (codec_priv_size >= 6 && AV_RN16(codec_priv) == 30)
    {
        overall_len = 6;
        for (i=0; i<3; i++)
        {
            header_len[i] = AV_RN16(codec_priv);
            codec_priv += 2;
            header_start[i] = codec_priv;
            codec_priv += header_len[i];
            if (overall_len > codec_priv_size - (int)header_len[i])
            {
                CDX_LOGE("overall_len(%d)，codec_priv_size(%d), header_len[%d](%u)",
                overall_len, codec_priv_size,i, header_len[i]);
                return -1;
            }
            overall_len += header_len[i];
        }
    }
    else if (codec_priv_size >= 3 && codec_priv[0] == 2)
    {
        overall_len = 3;
        codec_priv++;
        for (i=0; i<2; i++, codec_priv++)
        {
            header_len[i] = 0;
            for (; overall_len < codec_priv_size && *codec_priv==0xff; codec_priv++)
            {
                header_len[i] += 0xff;
                overall_len   += 0xff + 1;
            }
            header_len[i] += *codec_priv;
            overall_len   += *codec_priv;
            if (overall_len > codec_priv_size)
            {
                CDX_LOGE("overall_len(%d) > codec_priv_size(%d)",overall_len,codec_priv_size);
                return -1;
            }
        }
        header_len[2] = codec_priv_size - overall_len;
        header_start[0] = codec_priv;
        header_start[1] = header_start[0] + header_len[0];
        header_start[2] = header_start[1] + header_len[1];
    }
    else
    {
        CDX_LOGE("codec_priv_size(%d)", codec_priv_size);
        return -1;
    }

    for(lacing_fill_0=0; lacing_fill_0*0xff<header_len[0]; lacing_fill_0++);
    for(lacing_fill_1=0; lacing_fill_1*0xff<header_len[1]; lacing_fill_1++);
    for(lacing_fill_2=0; lacing_fill_2*0xff<header_len[2]; lacing_fill_2++);

    *extradata_size =  27 + lacing_fill_0 + header_len[0]
                       +  27 + lacing_fill_1 + header_len[1]
                          + lacing_fill_2 + header_len[2];
    OggHeader = *extradata = malloc(*extradata_size);
    if(OggHeader == NULL)
    {
        CDX_LOGE("malloc failed.");
        return -1;
    }
    for(i=0; i<26; i++)
    {
       OggHeader[i] = 0;
    }
    OggHeader[0] = 0x4f; OggHeader[1] = 0x67;  OggHeader[2] = 0x67;  OggHeader[3] = 0x53;
    OggHeader[5] = 0x2;
    lacing_fill = lacing_fill_0;
    OggHeader[26] = lacing_fill;
    OggHeader += 27;
    for(i=0; i<lacing_fill; i++)
    {
        if(i!=(lacing_fill-1))
        {
            OggHeader[i] = 0xff;
        }
        else
        {
            OggHeader[i] = header_len[0] - (lacing_fill-1)*0xff;
        }
    }
    OggHeader += lacing_fill;

    memcpy(OggHeader, header_start[0], header_len[0]);
    OggHeader += header_len[0];

    for(i=0; i<26; i++)
    {
       OggHeader[i] = 0;
    }
    OggHeader[0] = 0x4f; OggHeader[1] = 0x67;  OggHeader[2] = 0x67;  OggHeader[3] = 0x53;
    OggHeader[18] = 0x1;
    OggHeader[26] = lacing_fill_1 + lacing_fill_2;
    OggHeader += 27;
    lacing_fill = lacing_fill_1;
    for(i=0; i<lacing_fill; i++)
    {
        if(i!=(lacing_fill-1))
        {
            OggHeader[i] = 0xff;
        }
        else
        {
            OggHeader[i] = header_len[1] - (lacing_fill-1)*0xff;
        }
    }
    OggHeader += lacing_fill;

    lacing_fill = lacing_fill_2;
    for(i=0; i<lacing_fill; i++)
    {
        if(i!=(lacing_fill-1))
        {
            OggHeader[i] = 0xff;
        }
        else
        {
            OggHeader[i] = header_len[2] - lacing_fill*0xff - 1;
        }
    }
    OggHeader += lacing_fill;

    memcpy(OggHeader, header_start[1], header_len[1]);
    OggHeader += header_len[1];
    memcpy(OggHeader, header_start[2], header_len[2]);
    OggHeader += header_len[2];

    return 0;
}*/

/*******************************************************************************
Function name: set_FILE_PARSER_audio_info
Description:

Parameters:

Return:

Time: 2011/3/26
*******************************************************************************/
cdx_int32 SetFileParserAudioInfo(CdxAviParserImplT *p)
{
    cdx_uint8 i = 0;
    AviFileInT          *aviIn = (AviFileInT *)p->privData;
    WAVEFORMATEX        *audioFormat;
    cdx_int32           WAVEFORMATEX_size = 18;
    AudioStreamInfoT    *pStrmInfo;
    AVIStreamHeaderT    *strmHdr;
    if(p->hasAudio)
    {
        for(i = 0; i < p->hasAudio; i++)
        {
            strmHdr = (AVIStreamHeaderT *)aviIn->sInfo[p->audioStreamIndexArray[i]]->strh->buffer;
            audioFormat = (WAVEFORMATEX *)aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->buffer;
            p->aFormatArray[i].nChannelNum = audioFormat->nChannels;
            p->aFormatArray[i].nBitsPerSample = audioFormat->wBitsPerSample;
            p->aFormatArray[i].nMaxBitRate = p->aFormatArray[i].nAvgBitrate =
                audioFormat->nAvgBytesPerSec<<3;
            p->aFormatArray[i].nSampleRate = audioFormat->nSamplesPerSec;
            pStrmInfo = &aviIn->audInfoArray[i];
            if(pStrmInfo->cbrFlag)
            {
                p->nAudPtsOffsetArray[i] = (cdx_uint32)((cdx_uint64)strmHdr->dwStart*1000 /
                    pStrmInfo->avgBytesPerSec);
            }
            else
            {
                p->nAudPtsOffsetArray[i] = (cdx_uint32)((cdx_uint64)strmHdr->dwStart *
                    pStrmInfo->sampleNumPerFrame / pStrmInfo->sampleRate);
            }
            if(0 != p->nAudPtsOffsetArray[i])
            {
                CDX_LOGV("this avi p->nAudPtsOffsetArray[%d] = [%d]ms\n",
                    i, p->nAudPtsOffsetArray[i]);
            }
            switch(audioFormat->wFormatTag)
            {
                case PCM_TAG:
                case ALAW_TAG:
                case MULAW_TAG:
                    CDX_LOGV("audio bitstream type: AUDIO_CODEC_FORMAT_PCM!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    p->aFormatArray[i].eSubCodecFormat =
                        audioFormat->wFormatTag | ABS_EDIAN_FLAG_LITTLE;
                    //p->aFormatArray[i].audio_bs_src = CEDARLIB_FILE_FMT_AVI;
                    p->aFormatArray[i].nCodecSpecificDataLen = 0;
                    p->aFormatArray[i].pCodecSpecificData = 0;
                    break;
                case ADPCM11_TAG:
                    CDX_LOGV("audio bitstream type: AUDIO_CODEC_FORMAT_ADPCM!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_ADPCM;
                    p->aFormatArray[i].eSubCodecFormat =
                        ADPCM_CODEC_ID_IMA_WAV | ABS_EDIAN_FLAG_LITTLE;
                    //p->aFormatArray[i].audio_bs_src = CEDARLIB_FILE_FMT_AVI;
                    p->aFormatArray[i].nCodecSpecificDataLen = 0;
                    //p->aFormatArray[i].pCodecSpecificData = (char *)(int)audioFormat->nBlockAlign;
                    p->aFormatArray[i].nBlockAlign = (int)audioFormat->nBlockAlign;
                    break;
                case ADPCM_TAG:
                    CDX_LOGV("audio bitstream type: AUDIO_CODEC_FORMAT_ADPCM!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_ADPCM;
                    p->aFormatArray[i].eSubCodecFormat = ADPCM_CODEC_ID_MS | ABS_EDIAN_FLAG_LITTLE;
                    //p->aFormatArray[i].audio_bs_src = CEDARLIB_FILE_FMT_AVI;
                    p->aFormatArray[i].nCodecSpecificDataLen = 0;
                    //p->aFormatArray[i].pCodecSpecificData = (char *)(int)audioFormat->nBlockAlign;
                    p->aFormatArray[i].nBlockAlign = (int)audioFormat->nBlockAlign;
                    break;

                case MP3_TAG1:
                case MP3_TAG2:
                    CDX_LOGV("audio bitstream type: AUDIO_CODEC_FORMAT_MP3!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_MP3;
                    p->aFormatArray[i].eSubCodecFormat = 0;
                    //p->aFormatArray[i].audio_bs_src = CEDARLIB_FILE_FMT_AVI;
                    p->aFormatArray[i].nCodecSpecificDataLen = 0;
                    p->aFormatArray[i].pCodecSpecificData = 0;
                    break;
                case WMA1_TAG:
                case WMA2_TAG:
                {
                    /******************************
                              typedef struct
                              {
                                  WORD wFormatTag;
                                  WORD nChannels;
                                  DWORD nSamplesPerSec;
                                  DWORD nAvgBytesPerSec;
                                  WORD nBlockAlign;
                                  WORD wBitsPerSample;
                                  WORD cbSize; //Codec Specific Data Size
                                  ... //Codec Specific Data
                              } WAVEFORMATEX; *PWAVEFORMATEX;
                              *******************************/
                    CDX_LOGV("audio bitstream type: AUDIO_WMA!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_WMA_STANDARD;
                    p->aFormatArray[i].eSubCodecFormat = audioFormat->wFormatTag;
                    p->aFormatArray[i].nBlockAlign = audioFormat->nBlockAlign;
                    p->aFormatArray[i].nCodecSpecificDataLen =
                        aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length - 18;
                    p->aFormatArray[i].pCodecSpecificData =
                        (char *)aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->buffer+18;
                    break;
                }
                case AC3_TAG:
                    CDX_LOGV("audio bitstream type: AUDIO_CODEC_FORMAT_AC3!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_AC3;
                    p->aFormatArray[i].eSubCodecFormat = 0;
                    p->aFormatArray[i].nCodecSpecificDataLen = 0;
                    p->aFormatArray[i].pCodecSpecificData = 0;
                    break;
                case 0xfffe: //for support Video huangjinjia WAVE_FORMAT_EXTENSIBLE
                    CDX_LOGV("audio bitstream type: WAVE_FORMAT_EXTENSIBLE!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    p->aFormatArray[i].eSubCodecFormat = 0xfffe;
                    p->aFormatArray[i].nCodecSpecificDataLen = 0;
                    p->aFormatArray[i].pCodecSpecificData = 0;
                    break;
                case DTS_TAG:
                    CDX_LOGV("audio bitstream type: AUDIO_CODEC_FORMAT_DTS!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_DTS;
                    p->aFormatArray[i].eSubCodecFormat = 0;
                    //p->aFormatArray[i].audio_bs_src = CEDARLIB_FILE_FMT_AVI;
                    p->aFormatArray[i].nCodecSpecificDataLen = 0;
                    p->aFormatArray[i].pCodecSpecificData = 0;
                    break;
                case AAC_TAG:
                    CDX_LOGV("audio bitstream type: AUDIO_AAC!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
                    p->aFormatArray[i].eSubCodecFormat = 0;
                    //p->aFormatArray[i].audio_bs_src = CEDARLIB_FILE_FMT_AVI;
                    p->aFormatArray[i].nCodecSpecificDataLen = 0;
                    p->aFormatArray[i].pCodecSpecificData = 0;
                    if(aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length >
                        (cdx_uint32)WAVEFORMATEX_size)
                    {
                        p->aFormatArray[i].nCodecSpecificDataLen =
                            aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length
                            - WAVEFORMATEX_size;
                        p->aFormatArray[i].pCodecSpecificData =
                            aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->buffer
                            + WAVEFORMATEX_size;
                    }
                    break;
               /*case VORBIS_TAG: // todo...
               {
                   CDX_LOGD("audio bitstream type: AUDIO_CODEC_FORMAT_OGG!"
                   "aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length(%d)26",
                    aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length);
                   p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_OGG;
                   p->aFormatArray[i].eSubCodecFormat = 0;
                   //p->aFormatArray[i].audio_bs_src = CEDARLIB_FILE_FMT_AVI;
                   p->aFormatArray[i].nCodecSpecificDataLen = 0;
                   p->aFormatArray[i].pCodecSpecificData = 0;
                   //if(aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length >
                   (cdx_uint32)WAVEFORMATEX_size)
                   //{
                   //    p->aFormatArray[i].nCodecSpecificDataLen =
                   //    aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length
                   //    - WAVEFORMATEX_size;
                   //    p->aFormatArray[i].pCodecSpecificData =
                   //    aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->buffer
                   //    + WAVEFORMATEX_size;
                   //}

                  if(split_xiph_header((cdx_uint8 *)
                  aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->buffer + WAVEFORMATEX_size,
               (cdx_int32)aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length-WAVEFORMATEX_size,
                  (cdx_uint8 **)&p->aFormatArray[i].pCodecSpecificData,
                  (cdx_int32 *)&p->aFormatArray[i].nCodecSpecificDataLen) == -1)

                   {
                        CDX_LOGV("split_xiph_header failed."
                        "p->aFormatArray[i].pCodecSpecificData(%p), "
                        "p->aFormatArray[i].nCodecSpecificDataLen(%d)",
                         p->aFormatArray[i].pCodecSpecificData,
                         p->aFormatArray[i].nCodecSpecificDataLen);
                        if(aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length >
                            (cdx_uint32)WAVEFORMATEX_size)
                        {
                            p->aFormatArray[i].nCodecSpecificDataLen =
                            aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->length
                                                                       - WAVEFORMATEX_size;
                            p->aFormatArray[i].pCodecSpecificData =
                            aviIn->sInfo[p->audioStreamIndexArray[i]]->strf->buffer
                                                                        + WAVEFORMATEX_size;
                        }
                        CDX_LOGV("failed.p->aFormatArray[i].pCodecSpecificData(%p),
                        p->aFormatArray[i].nCodecSpecificDataLen(%d)",
                        p->aFormatArray[i].pCodecSpecificData,
                        p->aFormatArray[i].nCodecSpecificDataLen);
                   }
                   break;
               }*/
               default:
                    CDX_LOGV("audio bitstream type: AUDIO_CODEC_FORMAT_UNKNOWN!\n");
                    p->aFormatArray[i].eCodecFormat = AUDIO_CODEC_FORMAT_UNKNOWN;
                    //p->hasAudio = FALSE;
                    break;
            }
        }
    }
    return AVI_SUCCESS;
}
cdx_int32 SetFileParserSubInfo(CdxAviParserImplT *p)
{
    cdx_uint8 i = 0;
    AviFileInT          *aviIn = (AviFileInT *)p->privData;
    BITMAPINFOHEADER    *subFormat;
    SubStreamInfoT      *pStrmInfo;
    AVIStreamHeaderT    *strmHdr;

    if(p->hasSubTitle)
    {
        for(i = 0; i < p->hasSubTitle; i++)
        {
            strmHdr =
                (AVIStreamHeaderT *)aviIn->sInfo[p->subtitleStreamIndexArray[i]]->strh->buffer;
            subFormat =
                (BITMAPINFOHEADER *)aviIn->sInfo[p->subtitleStreamIndexArray[i]]->strf->buffer;
            pStrmInfo = &aviIn->subInfoArray[i];
            p->tFormatArray[i].nStreamIndex = i;

            if(strmHdr->fccType == CDX_CKID_STREAM_TYPE_VIDEO) //bitmap form of subtitles.
            {
                switch(pStrmInfo->aviSubTag)
                {
                    case CDX_MMIO_FOURCC('D','X','S','A'):
                    {
                        //CDX_LOGD("sub bitstream type: SUBTITLE_CODEC_XSUB!");
                        static cdx_char tempTag[4] = {'D','X','S','A'};
                        p->tFormatArray[i].eCodecFormat = SUBTITLE_CODEC_DIVX;
                        p->tFormatArray[i].nCodecSpecificDataLen = 4;
                        p->tFormatArray[i].pCodecSpecificData = tempTag;
                    }
                    case CDX_MMIO_FOURCC('D','X','S','B'):
                    {
                        //CDX_LOGD("sub bitstream type: SUBTITLE_CODEC_XSUB!");
                        static cdx_char tempTag[4] = {'D','X','S','B'};
                        p->tFormatArray[i].eCodecFormat = SUBTITLE_CODEC_DIVX;
                        p->tFormatArray[i].nCodecSpecificDataLen = 4;
                        p->tFormatArray[i].pCodecSpecificData = tempTag;
                        break;
                    }
                    default:
                    {
                        CDX_LOGW("sub bitstream type: SUBTITLE_CODEC_UNKNOWN!");
                        p->tFormatArray[i].eCodecFormat = SUBTITLE_CODEC_UNKNOWN;
                        break;
                    }
                }
            }
            else if(strmHdr->fccType == CDX_STREAM_TYPE_TEXT) //text form of subtitles.
            {
                CDX_LOGW("not yet ...");
            }
            else
            {
                CDX_LOGW("strmHdr->fccType(%u)", strmHdr->fccType);
            }
        }
    }
    return AVI_SUCCESS;
}

static cdx_int32 CreateStreamName(cdx_uint8 *pStreamName, cdx_int32 nNameSize, cdx_int32 streamIdx)
{
    char strStreamId[5] = {0};
    if(nNameSize < 10)
    {
        CDX_LOGV("stream name length[%d] < 10, can't make name.", nNameSize);
        return AVI_ERR_PARA_ERR;
    }
    strStreamId[0] = CDX_TO_HEX(((streamIdx) & 0xf0)>>4);
    strStreamId[1] = CDX_TO_HEX((streamIdx) & 0x0f);
    strcpy((char*)pStreamName, "stream");
    strcat((char*)pStreamName, strStreamId);
    return AVI_SUCCESS;
}

/*******************************************************************************
Function name: AVI_get_audio_stream_info
Description:

Parameters:

Return:

Time: 2010/8/18
*******************************************************************************/
cdx_int32 AVIGetAudioStreamInfo(AudioStreamInfoT *pAudioStreamInfo,
    AviStreamInfoT *pAviStreamInfo, cdx_int32 streamIdx)
{
    AVIStreamHeaderT *pabsHdr;
    WAVEFORMATEX     *pabsFmt;
    if(pAviStreamInfo->mediaType != 'a')
    {
        CDX_LOGV("avi stream is not audio stream!");
        return AVI_ERR_FAIL;
    }
    pabsHdr = (AVIStreamHeaderT *)pAviStreamInfo->strh->buffer;
    pabsFmt = (WAVEFORMATEX *)pAviStreamInfo->strf->buffer;

    pAudioStreamInfo->streamIdx = streamIdx;
    pAudioStreamInfo->aviAudioTag = pabsFmt->wFormatTag;
    pAudioStreamInfo->nBlockAlign = pabsFmt->nBlockAlign;
    pAudioStreamInfo->dataEncodeType = SUBTITLE_TEXT_FORMAT_UTF8;

    switch(pabsFmt->wFormatTag)
    {
        case PCM_TAG:
        case ALAW_TAG:
        case MULAW_TAG:
        case ADPCM11_TAG:
        case ADPCM_TAG:
        case MP3_TAG1:
        case MP3_TAG2:
        case WMA1_TAG:
        case WMA2_TAG:
        case AC3_TAG:
        case DTS_TAG:
        case AAC_TAG:
        case 0xfffe: //for support Video JianGuoDaYe
        //case VORBIS_TAG:
            break;
        default:
        {
            CDX_LOGE("Unkown wFormatTag, pabsFmt->wFormatTag(%u)", pabsFmt->wFormatTag);
            return -1;
        }
    }

    CreateStreamName(pAudioStreamInfo->sStreamName, AVI_STREAM_NAME_SIZE, streamIdx);
    CDX_LOGV("pabsHdr->dwRate[%d], pabsHdr->dwScale[%d]", pabsHdr->dwRate, pabsHdr->dwScale);
    switch(pabsFmt->wFormatTag)
    {
        case PCM_TAG:
        case ADPCM11_TAG:
        case ADPCM_TAG:
        {
            //pcm must be cbr, but pcm strh dwRate/dwScale = samplerate, frame/s;
            //adpcm strh dwRate/dwScale = frame/s
            //we convert to byte/s.
            //(1)if strh is valid.
            if(pabsHdr->dwScale && pabsFmt->nBlockAlign > 0)
                //nBlockAlign in pcm/adpcm is one frame byte size
            {
                pAudioStreamInfo->avgBytesPerSec = pabsHdr->dwRate *
                    pabsFmt->nBlockAlign / pabsHdr->dwScale;
                CDX_LOGV("pcmtag[%x], avgBytesPerSec[%d]",pabsFmt->wFormatTag,
                    pAudioStreamInfo->avgBytesPerSec);
            }
            else if(pabsFmt->nAvgBytesPerSec)
            {
                pAudioStreamInfo->avgBytesPerSec = pabsFmt->nAvgBytesPerSec;
                CDX_LOGV("use strf, pcmtag[%x], AvgBytesPerSec[%d]",pabsFmt->wFormatTag,
                    pAudioStreamInfo->avgBytesPerSec);
            }
            else
            {
                pAudioStreamInfo->avgBytesPerSec = 1;
                CDX_LOGW("pcmtag[%d] wrong strh and strf, avgBytesPerSec=1", pabsFmt->wFormatTag);
            }
            pAudioStreamInfo->cbrFlag = 1;
            break;
        }
        case MP3_TAG1:
        case MP3_TAG2:
        {
            //decide if cbr or vbr
            if(0 == pabsHdr->dwRate || 0 == pabsHdr->dwScale)
            {
                //cbr
                CDX_LOGW("avi mp3, strh wrong! must be cbr!");
                pAudioStreamInfo->avgBytesPerSec =
                    pabsFmt->nAvgBytesPerSec > 0 ? pabsFmt->nAvgBytesPerSec : 1;
                pAudioStreamInfo->cbrFlag = 1;
                break;
            }
            if(pabsHdr->dwRate/pabsHdr->dwScale >= MIN_BYTERATE)
            {
                //cbr
                pAudioStreamInfo->avgBytesPerSec = pabsHdr->dwRate/pabsHdr->dwScale;
                pAudioStreamInfo->cbrFlag = 1;
            }
            else
            {
                if(pabsFmt->nBlockAlign <= 4)
                {
                    //we still consider cbr
                    pAudioStreamInfo->avgBytesPerSec =
                    pabsFmt->nAvgBytesPerSec>0 ? pabsFmt->nAvgBytesPerSec : 1;
                    pAudioStreamInfo->cbrFlag = 1;
                }
                else if((pabsHdr->dwRate == pabsFmt->nAvgBytesPerSec) &&
                    (pabsFmt->nAvgBytesPerSec >= MIN_BYTERATE))
                {
                    //we still consider cbr
                    //CDX_LOGD(" dwRate[%d]==pabsFmt->nAvgBytesPerSec[%d], pabsHdr->dwScale[%d]",
                    //               pabsHdr->dwRate, pabsFmt->nAvgBytesPerSec, pabsHdr->dwScale);
                    pAudioStreamInfo->avgBytesPerSec = pabsFmt->nAvgBytesPerSec;
                    pAudioStreamInfo->cbrFlag = 1;
                }
                else
                {
                    // we consider vbr!
                    pAudioStreamInfo->sampleRate = pabsFmt->nSamplesPerSec;
                    pAudioStreamInfo->sampleNumPerFrame =
                        pabsHdr->dwScale*pabsFmt->nSamplesPerSec/pabsHdr->dwRate;
                    pAudioStreamInfo->cbrFlag = 0;
                    pAudioStreamInfo->avgBytesPerSec =
                        pabsFmt->nAvgBytesPerSec > 0 ? pabsFmt->nAvgBytesPerSec : 1;
                    CDX_LOGV("avi mp3 vbr, sample_rate[%d], sample_num_per_frame[%d]",
                        pAudioStreamInfo->sampleRate, pAudioStreamInfo->sampleNumPerFrame);
                }
            }
            break;
        }
        case AAC_TAG:
        {
            if(AVI_SUCCESS == EvaluateStrhValid(pabsHdr))
            {
                if (pabsHdr->dwSampleSize > 0 /*|| pabsHdr->dwScale < STRH_DWSCALE_THRESH*/)
                {
                    if((pabsHdr->dwSampleSize == 1024 && pabsFmt->nBlockAlign == 1024) ||
                       (pabsHdr->dwSampleSize == 4096 && pabsFmt->nBlockAlign == 4096))
                    {
                        pAudioStreamInfo->sampleRate = pabsHdr->dwRate;
                        pAudioStreamInfo->sampleNumPerFrame = pabsHdr->dwScale;
                        pAudioStreamInfo->cbrFlag = 0;
                    }
                    else
                    {
                        pAudioStreamInfo->avgBytesPerSec = pabsHdr->dwRate/pabsHdr->dwScale;
                        pAudioStreamInfo->cbrFlag = 1;
                    }
                }
                else //VBR
                {
                    pAudioStreamInfo->sampleRate = pabsHdr->dwRate;
                    pAudioStreamInfo->sampleNumPerFrame = pabsHdr->dwScale;
                    pAudioStreamInfo->cbrFlag = 0;
                    pAudioStreamInfo->avgBytesPerSec =
                        pabsFmt->nAvgBytesPerSec>0 ? pabsFmt->nAvgBytesPerSec : 1;
                }
            }
            else
            {
                pAudioStreamInfo->avgBytesPerSec =
                    pabsFmt->nAvgBytesPerSec>0 ? pabsFmt->nAvgBytesPerSec : 1;
                pAudioStreamInfo->cbrFlag = 1;
            }
            break;
        }
        //case VORBIS_TAG:
        {
                //TODO
        }
        default:
        {
            if(pabsFmt->nAvgBytesPerSec)
            {
                pAudioStreamInfo->avgBytesPerSec = pabsFmt->nAvgBytesPerSec;
            }
            else if(pabsHdr->dwRate != 0 && pabsHdr->dwScale != 0)
            {
                pAudioStreamInfo->avgBytesPerSec = pabsHdr->dwRate/pabsHdr->dwScale;
            }
            else
            {
                pAudioStreamInfo->avgBytesPerSec = 1;
            }
            pAudioStreamInfo->cbrFlag = 1;
            break;
        }
    }
    return AVI_SUCCESS;
}
cdx_int32 AVIGetSubStreamInfo(SubStreamInfoT *pSubStreamInfo,
    AviStreamInfoT *pAviStreamInfo, cdx_int32 streamIdx)
{ //The ‘strh’ chunk of a subtitle stream should be as it  is
  //normally defined, with fccType ‘txts’ for text form
  //of subtitles or ‘vids’ for bitmap form of subtitles.
    AVIStreamHeaderT  *psbsHdr;
    BITMAPINFOHEADER  *psbsFmt;

    if(pAviStreamInfo->mediaType != 't')
    {
        CDX_LOGW("stream is not subtitle stream!");
        return AVI_ERR_FAIL;
    }

    psbsHdr = (AVIStreamHeaderT *)pAviStreamInfo->strh->buffer;
    if(psbsHdr->fccType == CDX_CKID_STREAM_TYPE_VIDEO) //bitmap form of subtitles.
    {
        psbsFmt = (BITMAPINFOHEADER *)pAviStreamInfo->strf->buffer;

        pSubStreamInfo->streamIdx = streamIdx;
        pSubStreamInfo->aviSubTag = psbsFmt->biCompression;

        switch(psbsFmt->biCompression)
        {
            case CDX_MMIO_FOURCC('D','X','S','A'):
            case CDX_MMIO_FOURCC('D','X','S','B'):
            {
                //CDX_LOGD("xxxxx XSUB.");
                break;
            }
            default:
            {
                CDX_LOGE("Unkown biCompression, psbsFmt->biCompression(%u)",
                    psbsFmt->biCompression);
                return -1;
            }
        }
    }
    else if(psbsHdr->fccType == CDX_STREAM_TYPE_TEXT) //text form of subtitles.
    {
        CDX_LOGW("not yet...");
    }
    else
    {
        CDX_LOGW("psbsHdr->fccType[%u]", psbsHdr->fccType);
    }

    CreateStreamName(pSubStreamInfo->sStreamName, AVI_STREAM_NAME_SIZE, streamIdx);

    return AVI_SUCCESS;
}
/*
*******************************************************************************************
*                       OPEN AVI struct cdx_stream_info READER
*
*Description: avi file reader depack media file structure.
*
*Arguments  : p     avi reader handle;
*             fn    avi file path;
*
*Return     : depack avi file structure result;
*               =   0   depack successed;
*               <   0   depack failed;
*
*Note       : This function open the avi file with the file path, create the avi file information
*             handle, and get the video, audio and subtitle bitstream information from the file
*             to set in the avi file information handle. Then, set the avi file information handle
*             to the pAviInfo for return.
******************************************************************************************
*/
cdx_int16 AviOpen(CdxAviParserImplT *impl)
{
    cdx_int32           ret;
    cdx_int16           strmIndex;
    AviFileInT          *aviIn;
    AviStreamInfoT      strmInfo;
    cdx_int32           tmplength1, tmplength2;
    //AVIStreamHeader     *strm_hdr;
    //BITMAPINFOHEADER    *bmp_info_hdr;
    AviChunkT           *chunk = NULL;

    if(!impl)
    {
        CDX_LOGE("AviOpen bad param.");
        return AVI_ERR_PARA_ERR;
    }
    tmplength1 = sizeof(OldIdxTableItemT);
    tmplength2 = sizeof(IdxTableItemT);

    ret = AVIReaderOpenFile(impl);
    if(ret < 0)
    {
        CDX_LOGE("Open file failed...");
        return ret;
    }

    aviIn = (AviFileInT *)impl->privData;
    chunk = &aviIn->dataChunk;

    // look for all bitstream in the media file
    for(strmIndex = 0; strmIndex < aviIn->nStream; strmIndex++)
    {
        AviReaderGetStreamInfo(aviIn, &strmInfo, strmIndex);
        if (strmInfo.mediaType == 'v')
        {
            if(impl->hasVideo < 2)  // may have two stream
            {
                impl->videoStreamIndexArray[(cdx_uint8)impl->hasVideo] = strmIndex;
                impl->videoStreamIndex = strmIndex;
                impl->hasVideo++;
            }
            impl->videoStreamIndex = impl->videoStreamIndexArray[0];
        }
        else if (strmInfo.mediaType == 'a'/* && p->hasAudio==FALSE*/)
        {
            //p->AudioStreamIndex = strm_index;
            if(impl->hasAudio < MAX_AUDIO_STREAM)
            {
                ret = AVIGetAudioStreamInfo(&aviIn->audInfoArray[(cdx_uint8)impl->hasAudio],
                    &strmInfo, impl->hasAudio);
                if(ret < 0)
                {
                    continue;
                }
                impl->audioStreamIndexArray[(cdx_uint8)impl->hasAudio] = strmIndex;
                impl->hasAudio++;
            }
            else
            {
                CDX_LOGW("avi audio stream array full, discard more audio stream.");
                continue;
            }
        }
        else if (strmInfo.mediaType == 't')
        {
            if(impl->hasSubTitle < MAX_SUBTITLE_STREAM)
            {
                ret = AVIGetSubStreamInfo(&aviIn->subInfoArray[(cdx_uint8)impl->hasSubTitle],
                    &strmInfo, impl->hasSubTitle);
                if(ret < 0)
                {
                    continue;
                }
                impl->subtitleStreamIndexArray[(cdx_uint8)impl->hasSubTitle] = strmIndex;
                impl->hasSubTitle++;
            }
            else
            {
                CDX_LOGW("avi subtitle stream array full, discard more subtitle stream.");
                continue;
            }
        }
        else
        {
            CDX_LOGW("Unknown bitstream type %c.", strmInfo.mediaType);
        }
    }

    if(impl->hasAudio > 0)
    {
        impl->nextCurAudStreamNum = impl->curAudStreamNum = 0;
        impl->audioStreamIndex = impl->audioStreamIndexArray[impl->curAudStreamNum];
    }

    SetFileParserVideoInfo(impl);
    SetFileParserAudioInfo(impl);
    SetFileParserSubInfo(impl);

    // check if the media file contain valid audio and video, check if need play this file
    if(impl->hasVideo && impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_UNKNOWN)
    {
        CDX_LOGE("Invalid video bitstream!\n");
        return AVI_ERR_NO_AV;
    }
    else if(impl->hasVideo && impl->hasAudio)
    {
        if(impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_UNKNOWN
            && impl->aFormatArray[impl->curAudStreamNum].eCodecFormat == AUDIO_CODEC_FORMAT_UNKNOWN)
        {
            CDX_LOGV("No valid audio or video bitstream!\n");
            return AVI_ERR_NO_AV;
        }
    }
    else if(!impl->hasVideo && !impl->hasAudio)
    {
        CDX_LOGV("No valid audio or video bitstream!\n");
        return AVI_ERR_NO_AV;
    }

    if (aviIn->moviEnd != 0)
    {
        CDX_LOGE("avi_in->movi_end[%d]!=0, fatal error!\n",aviIn->moviEnd);
        return AVI_ERR_PARA_ERR;
    }
    do
    {
        ret = GetNextChunk(aviIn->fp, &aviIn->dataChunk);
        if(ret < 0)
        {
            CDX_LOGE("Get next chunk failed...");
            return AVI_ERR_FILE_FMT_ERR;
        }
        if(chunk->fcc == CDX_LIST_HEADER_LIST && strncmp((char *)chunk->buffer, "odml", 4) == 0)
        {
            if(0 == aviIn->sInfo[impl->videoStreamIndex]->isODML)
            {
                //if the file is open DML structure, we need update total frame count
                CDX_LOGV("video indx is not ODML? check file.");
            }
            if(0 == aviIn->hasIndx)
            {
                CDX_LOGV("avi psr don't think it's ODML AVI? check!");
            }
            ret = GetNextChunk(aviIn->fp, &aviIn->dataChunk);
            if(ret < 0)
            {
                CDX_LOGE("Get next chunk failed...");
                return AVI_ERR_FILE_FMT_ERR;
            }

            // we do not need code below, or the duration error, see ffmpeg
            /*
            if(chunk->fcc == CDX_CKID_ODMLHEADER)
            {
                ODMLExtendedAVIHeaderT *tmpOdmlHdr = (ODMLExtendedAVIHeaderT *)chunk->buffer;
                if(aviIn->hasIndx)
                {
                    if(tmpOdmlHdr->dwTotalFrames > impl->totalFrames)
                    {
                        impl->totalFrames = tmpOdmlHdr->dwTotalFrames;
                        logd("+++ impl->totalFrames: %d", impl->totalFrames);
                    }
                }
                CDX_LOGV("odmlhdr said:total frames[%x], now total frames[%x]\n",
                tmpOdmlHdr->dwTotalFrames, impl->totalFrames);
            }*/
        }

    }while(chunk->fcc != CDX_LIST_HEADER_LIST || strncmp((char *)chunk->buffer, "movi", 4) != 0);

    aviIn->moviStart = CdxStreamTell(aviIn->fp);
    CDX_LOGV("moviStart(%x)", aviIn->moviStart);
    aviIn->moviEnd = aviIn->moviStart + chunk->length - 4;
#if 0
    //process some private information for video header
    if((impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_H264 &&
        impl->aviFormat.vFormat.nCodecSpecificDataLen == 0) ||
               ((impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_MPEG4
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_XVID
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_MSMPEG4V1
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_MSMPEG4V2
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_DIVX3
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_DIVX4
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_DIVX5
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_XVID
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_SORENSSON_H263
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_H263
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_WMV1
              || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_WMV2) &&
              impl->aviFormat.vFormat.nCodecSpecificDataLen == 0))
    {
        cdx_int16   rdret;
        aviIn->vopPrivInf = 0;
        impl->aviFormat.vFormat.pCodecSpecificData = 0;
        impl->aviFormat.vFormat.nCodecSpecificDataLen = 0;

        do
        {
            rdret = GetNextChunk(aviIn->fp, &aviIn->dataChunk);
            //try to get private information for video decoder
            if(rdret < 0 && rdret != AVI_ERR_CHUNK_OVERFLOW)
            {
                CDX_LOGV("Get chunk failed!");
                return AVI_ERR_FILE_FMT_ERR;
            }
            if(!chunk->length)
            {
                continue;
            }

            if(((chunk->fcc >> 16) == CDX_CKTYPE_DIB_BITS) || ((chunk->fcc >> 16) ==
                CDX_CKTYPE_DIB_COMPRESSED))
            {
                cdx_int32       i;
                cdx_uint8       *tmpPtr = (cdx_uint8 *)chunk->buffer;

                for(i = 0; i < (cdx_int32)chunk->length - 4; i++)// avi: h264 not need extradata.
                {
                    /*if((tmpPtr[i] == 0x00)
                      && (tmpPtr[i+1] == 0x00)
                      && (tmpPtr[i+2] == 0x01)
                      && (((tmpPtr[i+3] & 0x1f) == 0x05
                         && impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_H264)
                           || (tmpPtr[i+3] == 0xb6
                         && impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_MPEG4)))*/
                    if((tmpPtr[i] == 0x00) && (tmpPtr[i+1] == 0x00) && (tmpPtr[i+2] == 0x01) &&
                        (tmpPtr[i+3] == 0xb6)
                       &&(impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_MPEG4
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_XVID
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_MSMPEG4V1
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_MSMPEG4V2
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_DIVX3
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_DIVX4
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_DIVX5
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_XVID
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_SORENSSON_H263
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_H263
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_WMV1
                       || impl->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_WMV2))
                    {
                        aviIn->vopPrivInf = malloc(i + 16);
                        if(!aviIn->vopPrivInf)
                        {
                            CDX_LOGV("Request memory failed!");
                            return AVI_ERR_FILE_FMT_ERR;
                        }

                        memcpy(aviIn->vopPrivInf, chunk->buffer, i);
                        impl->aviFormat.vFormat.pCodecSpecificData = aviIn->vopPrivInf;
                        impl->aviFormat.vFormat.nCodecSpecificDataLen = i;
                        CDX_LOGV("pCodecSpecificData(%p), nCodecSpecificDataLen(%d)",
                        impl->aviFormat.vFormat.pCodecSpecificData,
                        impl->aviFormat.vFormat.nCodecSpecificDataLen);
                        break;
                    }
                }
                break;
            }
        }while(1);

        //seek file back to movi header
        if(CdxStreamSeek(aviIn->fp, aviIn->moviStart, STREAM_SEEK_SET))
        {
            CDX_LOGV("Seek file to movi header failed!");
            return AVI_ERR_FILE_FMT_ERR;
        }
    }
#endif
    if(impl->hasVideo && impl->aviFormat.vFormat.nCodecSpecificDataLen == 0)
    {
        cdx_int16   rdret;
        aviIn->vopPrivInf = 0;
        impl->aviFormat.vFormat.pCodecSpecificData = 0;
        impl->aviFormat.vFormat.nCodecSpecificDataLen = 0;

        do
        {
            rdret = GetNextChunk(aviIn->fp, &aviIn->dataChunk);
            //try to get private information for video decoder
            if(rdret < 0 && rdret != AVI_ERR_CHUNK_OVERFLOW)
            {
                CDX_LOGE("Get chunk failed!");
                return AVI_ERR_FILE_FMT_ERR;
            }
            if(!chunk->length)
            {
                continue;
            }

            if(((chunk->fcc >> 16) == CDX_CKTYPE_DIB_BITS) || ((chunk->fcc >> 16) ==
                CDX_CKTYPE_DIB_COMPRESSED))
            {
                if(1)
                {
                    int ret = ProbeVideoSpecificData(&impl->vTempFormat, (cdx_uint8 *)chunk->buffer,
                        chunk->length, impl->aviFormat.vFormat.eCodecFormat, CDX_PARSER_AVI);
                    if(ret == PROBE_SPECIFIC_DATA_ERROR)
                    {
                        CDX_LOGE("probeVideoSpecificData error");
                    }
                    else if(ret == PROBE_SPECIFIC_DATA_SUCCESS)
                    {
                        CDX_LOGD("PROBE_SPECIFIC_DATA_SUCCESS");
                    }
                    else if(ret == PROBE_SPECIFIC_DATA_NONE)
                    {
                        CDX_LOGW("PROBE_SPECIFIC_DATA_NONE");
                        //combine = 0; //* drop current es, use next es
                    }
                    else if(ret == PROBE_SPECIFIC_DATA_UNCOMPELETE)
                    {
                        CDX_LOGW("PROBE_SPECIFIC_DATA_UNCOMPELETE");
                        //combine = 1; //* add next es
                    }
                    else
                    {
                        CDX_LOGE("probeVideoSpecificData (%d), it is unknown.", ret);
                    }
                    if(impl->vTempFormat.nCodecSpecificDataLen > 0 &&
                        impl->vTempFormat.pCodecSpecificData)
                    {
                        aviIn->vopPrivInf = impl->vTempFormat.pCodecSpecificData;
                        impl->aviFormat.vFormat.pCodecSpecificData =
                            impl->vTempFormat.pCodecSpecificData;
                        impl->aviFormat.vFormat.nCodecSpecificDataLen =
                            impl->vTempFormat.nCodecSpecificDataLen;
                        CDX_LOGD("pCodecSpecificData(%p), nCodecSpecificDataLen(%d)",
                                impl->aviFormat.vFormat.pCodecSpecificData,
                                impl->aviFormat.vFormat.nCodecSpecificDataLen);
                    }
                }
                break;
            }
        }while(1);

        //seek file back to movi header
        if(CdxStreamSeek(aviIn->fp, aviIn->moviStart, STREAM_SEEK_SET))
        {
            CDX_LOGV("Seek file to movi header failed!");
            return AVI_ERR_FILE_FMT_ERR;
        }
    }

    // read forward to set the "idx1" pointer
    ret = GetIdx1Info(aviIn);
    if(ret != AVI_SUCCESS)
    {
        CDX_LOGW("get_idx1_info() RETURN [%d].", ret);
        //return AVI_ERR_FILE_FMT_ERR;
    }

    if(CdxStreamSeek(aviIn->fp, (cdx_int64)aviIn->moviStart, STREAM_SEEK_SET))
    {
        CDX_LOGE("Seek to 'movi' chunk start failed!");
        return -1;
    }

    aviIn->indexInKeyfrmTbl = 0;

    return 0;
}

/*
**********************************************************************************************
*                       COLSE AVI struct cdx_stream_info READER
*
*Description: free system resource used by avi file reader.
*
*Arguments  : p     avi reader handle;
*
*Return     : 0;
**********************************************************************************************
*/
cdx_int16 AviClose(CdxAviParserImplT *p)
{
    AviFileInT     *aviIn;

    if(p)
    {
        aviIn = (AviFileInT *)p->privData;
        AviReaderCloseFile(aviIn);
        AviReleaseDataChunkBuf(aviIn);
        if(aviIn->vopPrivInf)
        {
            free(aviIn->vopPrivInf);
            aviIn->vopPrivInf = NULL;
        }
    }
    return 0;
}

/*
***********************************************************************************************
*                       AVI struct cdx_stream_info READER READ CHUNK INFORMATION
*
*Description: get chunk head information.
*
*Arguments  : p     avi reader handle;
*
*Return     : 0;
    FILE_PARSER_PARA_ERR
***********************************************************************************************
*/
cdx_int16 AviRead(CdxAviParserImplT *p)
{
    AviFileInT  *aviIn;

    if(!p)
    {
        CDX_LOGE("Check para.");
        return AVI_ERR_PARA_ERR;
    }

    aviIn = (AviFileInT *)p->privData;

    if(!aviIn)
    {
        CDX_LOGE("Check privData.");
        return AVI_ERR_PARA_ERR;
    }

    if(READ_CHUNK_SEQUENCE == aviIn->readmode)
    {
        return AviReadSequence(p);
    }
    else if(READ_CHUNK_BY_INDEX == aviIn->readmode)
    {
        switch(aviIn->idxStyle)
        {
            case USE_IDX1:
            case USE_INDX:
            {
                return AviReadByIndex(p);
            }
            default:
            {
                CDX_LOGE("idx_sytle[%d] exception error!\n", aviIn->idxStyle);
                return AVI_ERR_PARA_ERR;
            }
        }
    }
    else
    {
        CDX_LOGE("readmode[%d] exception error!", aviIn->readmode);
        return AVI_ERR_PARA_ERR;
    }
}

/*******************************************************************************
Function name: AVI_build_idx
Description:

Parameters:

Return:

Time: 2010/8/11
*******************************************************************************/

cdx_int16 AVIBuildIdx(CdxAviParserImplT *p)
{
    cdx_int32 ret       = AVI_ERR_FAIL;
    AviFileInT          *aviIn;
    MainAVIHeaderT      *avih;
    cdx_uint32          curFilePos;
    cdx_int32 AVI_READ_MODE = READ_CHUNK_BY_INDEX;

    if(!p)
    {
        CDX_LOGE("Check para.");
        return AVI_ERR_PARA_ERR;
    }

    aviIn = (AviFileInT *)p->privData;
    if(!aviIn)
    {
        CDX_LOGE("privData is NULL? Check...");
        return AVI_ERR_PARA_ERR;
    }
    if(p->hasVideo == 2)
    {
        AVI_READ_MODE = READ_CHUNK_SEQUENCE;
    }
    if(aviIn->isNetworkStream)
    {
        CDX_LOGV("Is network stream,force readmode_Sequence!");
        AVI_READ_MODE = READ_CHUNK_SEQUENCE;
    }
    //decide readmode and index_use_mode, build different FFRR index table for them.
    if(AVI_READ_MODE == READ_CHUNK_SEQUENCE)
    {
        aviIn->readmode = READ_CHUNK_SEQUENCE;
    }
    else
    {
        aviIn->readmode = READ_CHUNK_BY_INDEX;

        avih = (MainAVIHeaderT*)aviIn->avih->buffer;
        if((avih->dwFlags & CDX_AV_IF_IS_INTERLEAVED) || p->hasSubTitle)
        {
            aviIn->readmode = READ_CHUNK_SEQUENCE;
            CDX_LOGD("xxx READ_CHUNK_SEQUENCE");
        }
    }

    //if readmode is by_index, create more fp here
    if(aviIn->readmode == READ_CHUNK_BY_INDEX)
    {
        CDX_LOGV("readmode is by_index, create more fp!");
#if 0
        aviIn->aud_fp = create_stream_handle(datasrc_desc);//TDDO TODO..
        aviIn->idx_fp = create_stream_handle(datasrc_desc);
#endif
        aviIn->audFp = p->stream;//0506
        aviIn->idxFp = p->stream;//
        if(NULL == aviIn->audFp || NULL == aviIn->idxFp)
        {
            CDX_LOGW("create more fp fail! \n");
            aviIn->audFp = NULL;
            aviIn->idxFp = NULL;
            return AVI_ERR_OPEN_FILE_FAIL;
        }
    }

    //backup current file pointer.
    curFilePos = CdxStreamTell(aviIn->fp);//movi_start

    if(aviIn->readmode == READ_CHUNK_BY_INDEX)
    {
        if(aviIn->hasIndx)
        {
            aviIn->idxStyle = USE_INDX;
            ret = AviBuildIdxForODMLIndexMode(p);
            if(ret != AVI_SUCCESS)
            {
                CDX_LOGD("build index table fail, turn to sequence mode, ret[%d].", ret);
                aviIn->readmode = READ_CHUNK_SEQUENCE;
            }
        }
        else if(aviIn->hasIdx1)
        {
            aviIn->idxStyle = USE_IDX1;
            ret = AviBuildIdxForIndexMode(p);
            if(ret != AVI_SUCCESS)
            {
                CDX_LOGD("build index table fail, turn to sequence mode, ret[%d]", ret);
                aviIn->readmode = READ_CHUNK_SEQUENCE;
            }
        }
        else
        {
            ret = AVI_ERR_NO_INDEX_TABLE;
            CDX_LOGD("index read mode, don't find idx1, turn to sequence mode.");
            aviIn->readmode = READ_CHUNK_SEQUENCE;
        }
    }

    if(aviIn->readmode == READ_CHUNK_SEQUENCE)
    {
        if(aviIn->hasIndx)
        {
            //CDX_LOGD("read mode sequence, has indx.");
            aviIn->idxStyle = USE_INDX;
            ret = AviBuildIdxForOdmlSequenceMode(p);
            if(ret == AVI_SUCCESS)
            {
            }
            else if(ret == AVI_ERR_PART_INDEX_TABLE)//not use.
            {
                CDX_LOGW("sequence mode, indx type, build part ffrrkeyframe table.");
            }
            else if(ret == AVI_ERR_NO_INDEX_TABLE)
            {
                CDX_LOGW("sequence mode, indx type, no ffrrkeyframe table.");
            }
            else if(ret == AVI_ERR_INDEX_HAS_NO_KEYFRAME)
            {
                CDX_LOGW("sequence mode, indx type, no ffrrkeyframe table because index "
                    "table has no keyframe.");
            }
            else
            {
                CDX_LOGW("AVI_build_idx fatal error, ret[%x].", ret);
            }
        }
        else if(aviIn->hasIdx1)
        {
            aviIn->idxStyle = USE_IDX1;
            //CDX_LOGD("aviIn->hasIdx1 %d",aviIn->hasIdx1);
            ret = AviBuildIdxForIdx1SequenceMode(p);
            if(ret == AVI_SUCCESS)
            {
            }
            else if(ret == AVI_ERR_NO_INDEX_TABLE)
            {
                CDX_LOGW("sequence mode, idx1 type, no table because of no index.");
            }
            else if(ret == AVI_ERR_INDEX_HAS_NO_KEYFRAME)
            {
                CDX_LOGW("sequence mode, idx1 type, no table because of index table "
                    "has no keyframe.");
            }
            else
            {
                CDX_LOGW("AVI_build_idx sequence mode, idx1 type, fatal error, ret[%x].", ret);
            }
        }
        else
        {
            ret = AVI_ERR_NO_INDEX_TABLE;
            CDX_LOGW("sequence read mode, don't find idx1.");
        }
    }

    if(aviIn->readmode == READ_CHUNK_SEQUENCE)
    {
        CDX_LOGD("use readmode sequence.");

        if(aviIn->audFp)
        {
            aviIn->audFp = NULL;
        }
        if(aviIn->idxFp)
        {
            aviIn->idxFp = NULL;
        }
    }
    else
    {
        CDX_LOGD("use readmode index.");
    }

    if(CdxStreamSeek(aviIn->fp, (cdx_int64)curFilePos, STREAM_SEEK_SET) < 0)
    {
        CDX_LOGE("file seek error!");
        ret = AVI_ERR_READ_FILE_FAIL;
    }

    return ret;
}

cdx_int32 AviMallocDataChunkBuf(AviFileInT *pAviIn)
{
    pAviIn->dataChunk.buffer = (cdx_char *)malloc(MAX_CHUNK_BUF_SIZE);
    if(!pAviIn->dataChunk.buffer)
    {
        CDX_LOGE("Malloc pAviIn->dataChunk.buffer failed.");
        return AVI_ERR_REQMEM_FAIL;
    }
    memset(pAviIn->dataChunk.buffer, 0, MAX_CHUNK_BUF_SIZE);
    return AVI_SUCCESS;
}

cdx_int32 AviReleaseDataChunkBuf(AviFileInT *aviIn)
{
    if(NULL == aviIn)
        return AVI_SUCCESS;
    if(aviIn->dataChunk.buffer)
    {
        free(aviIn->dataChunk.buffer);
        aviIn->dataChunk.buffer = NULL;
    }
    return AVI_SUCCESS;
}

cdx_int32 AviFileInInitial(AviFileInT *aviIn)
{
    cdx_int32   ret = AVI_SUCCESS;
    memset(aviIn, 0, sizeof(AviFileInT));

    aviIn->lastIndexInKeyfrmTbl = -1;

    return ret;
}
cdx_int32 AviFileInDeinitial(AviFileInT *aviIn)
{
    if(aviIn)
    {
        AviReleaseDataChunkBuf(aviIn);
        //free(avi_in);
    }
    return AVI_SUCCESS;
}

CdxAviParserImplT *AviInit(cdx_int32 *ret)
{
    AviFileInT          *aviIn = NULL;
    CdxAviParserImplT   *p;

    *ret = 0;
    p = (CdxAviParserImplT *)malloc(sizeof(*p));
    if(!p)
    {
        *ret = -1;
        CDX_LOGE("AviInit malloc failed.");
        return NULL;
    }
    memset(p, 0, sizeof(*p));
    p->mErrno = PSR_INVALID;
    aviIn = (AviFileInT *)malloc(sizeof(AviFileInT));
    if(aviIn)
    {
        *ret = AviFileInInitial(aviIn);
        p->privData = (void *)aviIn;

        AviMallocDataChunkBuf(aviIn);
    }
    else
    {
        CDX_LOGE("AVI_FILE_IN init fail, quit.");
        *ret = AVI_ERR_PARA_ERR;
    }
    p->videoStreamIndex = (cdx_uint8)-1;
    p->audioStreamIndex = (cdx_uint8)-1;

    return p;
}

cdx_int16 AviExit(CdxAviParserImplT *p)
{
    AviFileInT *aviIn = (AviFileInT *)p->privData;
    if(aviIn)
    {
        AviFileInDeinitial(aviIn);
        free(aviIn);
        p->privData = NULL;
    }
    free(p);

    return AVI_SUCCESS;
}
