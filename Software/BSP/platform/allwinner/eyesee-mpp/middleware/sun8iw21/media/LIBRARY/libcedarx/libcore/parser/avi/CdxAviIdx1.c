/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviIdx1.c
 * Description : Part of avi parser.
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "TEMP_TAG"

#include <CdxAviInclude.h>
#include <string.h>

/*******************************************************************************
Function name: initial_psr_idx1_table_reader
Description:
    1.
Parameters:

Return:

Time: 2010/9/2
*******************************************************************************/
cdx_int32 InitialPsrIdx1TableReader(struct PsrIdx1TableReader *pReader,
                                    AviFileInT *aviIn, cdx_int32 streamIndex)
{
    memset(pReader, 0, sizeof(struct PsrIdx1TableReader));
    pReader->bufIdxItem = 0;
    pReader->leftIdxItem = aviIn->idx1Total;
    pReader->idxFp = aviIn->idxFp;
    pReader->fpPos = aviIn->idx1Start;
    CdxStreamSeek(pReader->idxFp, pReader->fpPos, STREAM_SEEK_SET);
    pReader->fileBuffer = (cdx_uint8 *)malloc(INDEX_CHUNK_BUFFER_SIZE);
    if(NULL == pReader->fileBuffer)
    {
        CDX_LOGE("malloc fail.");
        return AVI_ERR_REQMEM_FAIL;
    }
    pReader->pIdxEntry = NULL;
    pReader->streamIdx = streamIndex;
    pReader->ckBaseOffset = aviIn->ckBaseOffset;
    pReader->readEndFlg = 0;
    return AVI_SUCCESS;
}

void DeinitialPsrIdx1TableReader(struct PsrIdx1TableReader *pReader)
{
    if(pReader->fileBuffer)
    {
        free(pReader->fileBuffer);
        pReader->fileBuffer = NULL;
    }
    memset(pReader, 0, sizeof(struct PsrIdx1TableReader));
}

/*******************************************************************************
Function name: search_next_idx1_index_entry
Description:
    1. must set read_end_flg if state has change.
    2. must set offset of chunk,
    3. return value is flag if success.
    4. preader is init in AVI_build_idx().
    5.  idx1 streamid entry
    6.
Parameters:
    *chunk_offset : absolute offset from the file_header.
Return:
    AVI_SUCCESS
    AVI_ERR_SEARCH_INDEX_CHUNK_END

    FILE_PARSER_READ_FILE_FAIL
    AVI_ERR_FILE_FMT_ERR
Time: 2009/5/25
*******************************************************************************/
cdx_int32 SearchNextIdx1IndexEntry(struct PsrIdx1TableReader *pReader,
    AVI_CHUNK_POSITION *pChunkPos)
{
    cdx_int32   chunkid = 0;
    cdx_int32   findFlag = 0;

    //*chunk_offset = -1;
    //*pchunk_body_size = 0;
    while(pReader->bufIdxItem > 0 || pReader->leftIdxItem > 0)
    {
        //1. check if load index.
        if(0 == pReader->bufIdxItem)
        {
            if(pReader->leftIdxItem > INDEX_CHUNK_BUFFER_SIZE/(cdx_int32)sizeof(AviIndexEntryT))
            {
                pReader->bufIdxItem = INDEX_CHUNK_BUFFER_SIZE/sizeof(AviIndexEntryT);
                pReader->leftIdxItem -= pReader->bufIdxItem;
            }
            else
            {
                pReader->bufIdxItem = pReader->leftIdxItem;
                pReader->leftIdxItem = 0;
            }
            if(CdxStreamSeek(pReader->idxFp, pReader->fpPos, STREAM_SEEK_SET))
            {
                CDX_LOGE("file seek error!");
                return AVI_ERR_READ_FILE_FAIL;
            }
            if(pReader->bufIdxItem*sizeof(AviIndexEntryT) !=
                (cdx_uint32)CdxStreamRead(pReader->idxFp,
                pReader->fileBuffer, pReader->bufIdxItem*sizeof(AviIndexEntryT)))
            {
                return AVI_ERR_READ_FILE_FAIL;
            }
            pReader->pIdxEntry = (AviIndexEntryT *)pReader->fileBuffer;
            pReader->fpPos += pReader->bufIdxItem*sizeof(AviIndexEntryT)*1;
        }
        //chunkid = ((preader->pidxentry->ckid&0xff)-0x30)<<8;
        //chunkid |= (preader->pidxentry->ckid>>8 & 0xff) - 0x30;
        chunkid = CDX_STREAM_FROM_FOURCC(pReader->pIdxEntry->ckid);
        if(chunkid == pReader->streamIdx)
        {
            pChunkPos->ckOffset       = pReader->pIdxEntry->dwChunkOffset + pReader->ckBaseOffset;
            pChunkPos->chunkBodySize  = pReader->pIdxEntry->dwChunkLength;
            pChunkPos->ixTblEntOffset = pReader->fpPos - pReader->bufIdxItem*sizeof(AviIndexEntryT);
            pChunkPos->isKeyframe     = (pReader->pIdxEntry->dwFlags & CDX_AVIIF_KEYFRAME) ? 1 : 0;
            pReader->bufIdxItem--;
            pReader->pIdxEntry++;
            pReader->chunkCounter++;
            pReader->chunkSize = pChunkPos->chunkBodySize;
            pReader->chunkIndexOffset = pChunkPos->ixTblEntOffset;
            pReader->totalChunkSize += pChunkPos->chunkBodySize;
            findFlag = 1;
            break;
        }
        pReader->bufIdxItem--;
        pReader->pIdxEntry++;
    }
    if(1 == findFlag)
    {
        return AVI_SUCCESS;
    }
    else if(pReader->bufIdxItem == 0 && pReader->leftIdxItem == 0)
    {
        CDX_LOGV("INDEX TABLE search end.");
        //*chunk_offset = -1;
        pReader->readEndFlg = 1;
        return AVI_ERR_SEARCH_INDEX_CHUNK_END;
    }
    else
    {
        CDX_LOGE("fatal error!check code.\n");
        //*chunk_offset = -1;
        return AVI_ERR_PARA_ERR;
    }
}

cdx_int32 SearchAudioChunkIdx1EntrySequence(struct PsrIdx1TableReader *pReader,
                                    cdx_uint32 vidPts,
                                    IdxTableItemT *pEntry,
                                    cdx_int32 iAud,
                                    AudioStreamInfoT *pAudStrmInfo)
{
    cdx_uint32   audPts = 0;
    cdx_int32    srchRet;
    AVI_CHUNK_POSITION  indexChunkEntry;
    if(pReader->chunkCounter > 0)  //audio chunk
    {
        audPts = (cdx_uint32)CalcAviAudioChunkPts(pAudStrmInfo,
                                pReader->totalChunkSize - pReader->chunkSize,
                                pReader->chunkCounter - 1);
        if(vidPts <= audPts)
        {
            pEntry->audPtsArray[iAud] = audPts;
            pEntry->audChunkIndexOffsetArray[iAud] = pReader->chunkIndexOffset;
            return AVI_SUCCESS;
        }
    }
    while(1)
    {
        srchRet = SearchNextIdx1IndexEntry(pReader, &indexChunkEntry);
        if(AVI_SUCCESS == srchRet)
        {
            //����index��ȡ��ʽ�£���audio chunk pts
            audPts = (cdx_uint32)CalcAviAudioChunkPts(pAudStrmInfo,
                    pReader->totalChunkSize - indexChunkEntry.chunkBodySize,
                    pReader->chunkCounter - 1);
            if(audPts >= vidPts)
            {
                pEntry->audPtsArray[iAud] = audPts;
                pEntry->audChunkIndexOffsetArray[iAud] = pReader->chunkIndexOffset;
                break;
            }
        }
        else
        { //use last valid value to fill pidx_item.
            if(AVI_ERR_SEARCH_INDEX_CHUNK_END == srchRet)
            {
                CDX_LOGV("search aud chunk over!");
            }
            audPts = (cdx_uint32)CalcAviAudioChunkPts(pAudStrmInfo,
                        pReader->totalChunkSize - pReader->chunkSize,
                        pReader->chunkCounter - 1);
            pEntry->audPtsArray[iAud] = audPts;
            pEntry->audChunkIndexOffsetArray[iAud] = pReader->chunkIndexOffset;
            break;
        }
    }
    return srchRet;
}

/*******************************************************************************
Function name: AVI_build_idx_for_index_mode
Description:
    1. decide avi file chunk readmode and idx_style.
    2. build system FFRR key frame index table.
    3. this keyframe index table is for read chunk by index table.
    4. for idx1 mode.
    5.
    6. idx1_buf free
Parameters:

Return:
    1. some error reason.
        -1, fatal error, file seek fail,
        AVI_ERR_FILE_FMT_ERR: read file fail
        FILE_PARSER_PARA_ERR: p==NULL
        FILE_PARSER_REQMEM_FAIL
Time: 2009/5/19
*******************************************************************************/
cdx_int16 AviBuildIdxForIndexMode(CdxAviParserImplT *p)
{
    AviFileInT      *aviIn;
    cdx_int32       ret;
    cdx_int32       srchRet;
    cdx_int32       i; //CDX_S32               id;
    cdx_uint32      vidPts;
    cdx_int32       curFilePos;
    cdx_uint32      vframePts;
    cdx_int32       nKeyframeTableSize;
    cdx_int32       iAud = 0;
    //MainAVIHeader       *tmpAviHdr;
    //AVIStreamHeader     *tmpAbsHdr = NULL;
    //WAVEFORMATEX        *tmpAbsFmt = NULL;
    //IDX_TABLE_ITEM      *pidx_item = NULL;
    IdxTableItemT      *pKeyFrameTableEntry = NULL;
    //struct index_table_reader idx_reader;
    AVI_CHUNK_POSITION  indexChunkEntry;
    cdx_uint8           indexChunkOkFlag = 1;
    AudioStreamInfoT   *pAudStrmInfo = NULL;

    //memset(&idx_reader, 0, sizeof(struct index_table_reader));
    if(!p)
    {
        return AVI_ERR_PARA_ERR;
    }

    aviIn = (AviFileInT *)p->privData;
    if(!aviIn)
    {
        return AVI_ERR_PARA_ERR;
    }
    //decide index use mode.
    if(READ_CHUNK_BY_INDEX != aviIn->readmode || USE_IDX1 != aviIn->idxStyle)
    {
        CDX_LOGV("read mode[%d], idx_style[%d]\n", aviIn->readmode, aviIn->idxStyle);
        return AVI_ERR_PARA_ERR;
    }
    if(!p->hasVideo)
    {
        return AVI_ERR_PARA_ERR;
    }

    //check if the index table is exsit in media file
    if((!aviIn->idx1Total) || (!p->hasVideo))
    {
        CDX_LOGW("No valid index table in the media file!\n");
        return AVI_ERR_NO_INDEX_TABLE;
    }

    if(!aviIn->idx1Buf)
    {
        aviIn->idx1Buf = (cdx_uint32 *)malloc(MAX_IDX_BUF_SIZE_FOR_INDEX_MODE);
        if(!aviIn->idx1Buf)
        {
            CDX_LOGE("Request buffer for build index failed\n");
            ret = AVI_ERR_REQMEM_FAIL;
            return AVI_ERR_REQMEM_FAIL;
        }
    }
    memset(aviIn->idx1Buf, 0, MAX_IDX_BUF_SIZE_FOR_INDEX_MODE);
    aviIn->idx1BufEnd = (cdx_uint32 *)((cdx_uint8 *)aviIn->idx1Buf +
        MAX_IDX_BUF_SIZE_FOR_INDEX_MODE);

    //back current file position
    curFilePos = CdxStreamTell(aviIn->fp);

    if(CdxStreamSeek(aviIn->fp, (cdx_int64)aviIn->idx1Start, STREAM_SEEK_SET))
    {
        CDX_LOGE("Seek file failed!\n");
        ret = AVI_ERR_FILE_FMT_ERR;
        goto __err1_idx1_buf;
    }

    //left_idx_item = avi_in->idx1_total;
    //buf_idx_item = 0;
    //m_idx = (AVIINDEXENTRY*)avi_in->data_chunk.buffer;

    //1.build key_frame idx table
    nKeyframeTableSize = MAX_IDX_BUF_SIZE_FOR_INDEX_MODE/sizeof(IdxTableItemT);
    //1.2 ��дkeyframe entry
    //1.first build video key frame table.
    pKeyFrameTableEntry = (IdxTableItemT *)aviIn->idx1Buf;

    ret = InitialPsrIdx1TableReader(&aviIn->vidIdx1Reader, aviIn, p->videoStreamIndex);
    if(ret < AVI_SUCCESS)
    {
       goto __err1_idx1_buf;
    }
    while(1)
    {
        if(p->bAbortFlag)
        {
            CDX_LOGD("idx1 index:detect abort flag,quit now1!\n");
            ret = AVI_ERR_ABORT;
            goto __err1_idx1_buf;
        }
        //srchret = search_next_ODML_index_entry(&avi_in->vid_indx_reader,
        //&chunk_pos, &chunk_body_size);
        srchRet = SearchNextIdx1IndexEntry(&aviIn->vidIdx1Reader, &indexChunkEntry);
        if(AVI_SUCCESS == srchRet)
        {
            if(0 == indexChunkEntry.isKeyframe)
            {
                if(p->aviFormat.vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_MJPEG)
                {
                    CDX_LOGV("MJPEG all video frame is keyframe! change!");
                    indexChunkEntry.isKeyframe = 1;
                }
            }
            //check if it is a key frame.
            if(1 == indexChunkEntry.isKeyframe)
            {
                //because chunk_idx has increase 1 in search_next_ODML_index_entry()
                pKeyFrameTableEntry->frameIdx = aviIn->vidIdx1Reader.chunkCounter - 1;
                //CDX_LOGD("xxxxxxxxx keyframe idx(%d), AviBuildIdxForIndexMode",
                //pKeyFrameTableEntry->frameIdx);
                pKeyFrameTableEntry->vidChunkOffset =
                    indexChunkEntry.ckOffset;//chunk_pos.ck_offset;
                pKeyFrameTableEntry->vidChunkIndexOffset = indexChunkEntry.ixTblEntOffset;
                vframePts = (cdx_uint32)((cdx_int64)pKeyFrameTableEntry->frameIdx
                                                         * p->aviFormat.nMicSecPerFrame / 1000);
                for(i = 0; i < p->hasAudio; i++)
                {
                    pKeyFrameTableEntry->audPtsArray[i] = vframePts;
                }
                pKeyFrameTableEntry++;
                aviIn->indexCountInKeyfrmTbl++;
                if(aviIn->indexCountInKeyfrmTbl >= nKeyframeTableSize)
                {
                    CDX_LOGV("FFRRKeyframe Table full,[%d] == [%d], video entry num[%d]\n",
                        aviIn->indexCountInKeyfrmTbl, nKeyframeTableSize,
                         aviIn->vidIdx1Reader.chunkCounter);
                    break;
                }
            }
        }
        else if(AVI_ERR_SEARCH_INDEX_CHUNK_END == srchRet)
        {
            break;
        }
        else
        {
            CDX_LOGV("search idx1 chunk error, [%d]", srchRet);
            indexChunkOkFlag = 0;
            ret = srchRet;
            DeinitialPsrIdx1TableReader(&aviIn->vidIdx1Reader);
            goto __err1_idx1_buf;
        }
    }
    DeinitialPsrIdx1TableReader(&aviIn->vidIdx1Reader);
    CDX_LOGV("KeyframeTable index video done, cnt=%d, maxcnt=%d\n",
        aviIn->indexCountInKeyfrmTbl,  nKeyframeTableSize);
    //2. fill audio_chunk and audio pts.
    if(!p->hasAudio)
    {
        ret = AVI_SUCCESS;
        goto RESTORE_FILE;
    }
    if(CdxStreamSeek(aviIn->fp, (cdx_int64)aviIn->idx1Start, STREAM_SEEK_SET))
    {
        CDX_LOGE("Seek file failed!");
        return AVI_ERR_FILE_FMT_ERR;
    }

    for(iAud = 0; iAud < p->hasAudio; iAud++)
    {
        if(iAud >= MAX_AUDIO_STREAM)
        {
            CDX_LOGV("iAud[%d] >= MAX_AUDIO_STREAM[%d]\n", iAud, MAX_AUDIO_STREAM);
            indexChunkOkFlag = 0;
            break;
        }
        ret = InitialPsrIdx1TableReader(&aviIn->audIdx1Reader, aviIn,
            p->audioStreamIndexArray[iAud]);
        if(ret != AVI_SUCCESS)
        {
            indexChunkOkFlag = 0;
            DeinitialPsrIdx1TableReader(&aviIn->audIdx1Reader);
            break;
        }
        pKeyFrameTableEntry = (IdxTableItemT *)aviIn->idx1Buf;
        pAudStrmInfo = &aviIn->audInfoArray[iAud];
        for(i = 0; i < aviIn->indexCountInKeyfrmTbl; i++)
        {
            if(p->bAbortFlag)
            {
                CDX_LOGV("idx1 index:detect abort flag,quit now2!\n");
                ret = AVI_ERR_ABORT;
                goto __err1_idx1_buf;
            }
            vidPts = (cdx_int64)pKeyFrameTableEntry->frameIdx *
                p->aviFormat.nMicSecPerFrame /1000;//unit : ms
            srchRet = SearchAudioChunkIdx1EntrySequence(&aviIn->audIdx1Reader, vidPts,
                                               pKeyFrameTableEntry, iAud, pAudStrmInfo);
            if(srchRet < AVI_SUCCESS)
            {
                CDX_LOGV("ret[%d], build ffrr table for idx1 index mode fail.", srchRet);
                if(srchRet == AVI_ERR_READ_FILE_FAIL)
                {
                    indexChunkOkFlag = 0;
                    break;
                }
                else //fatal error,
                {
                    indexChunkOkFlag = 0;
                    DeinitialPsrIdx1TableReader(&aviIn->audIdx1Reader);
                    ret = srchRet;
                    goto __err1_idx1_buf;
                }
            }
            pKeyFrameTableEntry++;
        }
        DeinitialPsrIdx1TableReader(&aviIn->audIdx1Reader);
        if(indexChunkOkFlag == 0)
        {
            break;
        }
    }

    //1.4  table entry
    if(indexChunkOkFlag)
    {
        ret = AVI_SUCCESS;
    }
    else
    {
        ret = AVI_ERR_NO_INDEX_TABLE;
        goto __err1_idx1_buf;
    }

RESTORE_FILE:
    DeinitialPsrIdx1TableReader(&aviIn->audIdx1Reader);
    DeinitialPsrIdx1TableReader(&aviIn->vidIdx1Reader);
    if(!aviIn->indexCountInKeyfrmTbl)
    {
        CDX_LOGV("No key frame in the index table!!!");

        if(aviIn->idx1Buf)
        {
            free(aviIn->idx1Buf);
            aviIn->idx1Buf = 0;
        }
        ret = AVI_ERR_INDEX_HAS_NO_KEYFRAME;
    }
    //restore file position
    if(CdxStreamSeek(aviIn->fp, (cdx_int64)curFilePos, STREAM_SEEK_SET))
    {
        return AVI_ERR_FILE_FMT_ERR;
    }
    if(ret == AVI_SUCCESS)
    {
        if(p->hasVideo)
        {
            InitialPsrIdx1TableReader(&aviIn->vidIdx1Reader, aviIn, p->videoStreamIndex);
        }
        if(p->hasAudio)
        {
            InitialPsrIdx1TableReader(&aviIn->audIdx1Reader, aviIn, p->audioStreamIndex);
        }
    }
    return ret;

__err1_idx1_buf:
    CdxStreamSeek(aviIn->fp, (cdx_int64)curFilePos, STREAM_SEEK_SET);
    if(aviIn->idx1Buf)
    {
        free(aviIn->idx1Buf);
        aviIn->idx1Buf = NULL;
    }
    DeinitialPsrIdx1TableReader(&aviIn->audIdx1Reader);
    DeinitialPsrIdx1TableReader(&aviIn->vidIdx1Reader);
    return ret;
}

