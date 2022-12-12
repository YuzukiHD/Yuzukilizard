/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviDepackSequence.c
 * Description : Part of avi parser.
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "AVI_TAG"

#include "CdxAviInclude.h"
#include <string.h>

//1.FFRRKeyframeTable
/*
**********************************************************************************************
*                       BUILD FRAME INDEX TABLE
*
*Description: Build frame index table.

*Arguments  : pAviInf   the handle of avi file process information.
*
*Return     : result of build index table
*               = 0     build index table successed;
                AVI_SUCCESS
*
                >0
                AVI_ERR_INDEX_HAS_NO_KEYFRAME
                AVI_ERR_NO_INDEX_TABLE

                < 0     build index table failed.
                AVI_ERR_PARA_ERR
                AVI_ERR_REQMEM_FAIL
                AVI_ERR_READ_FILE_FAIL
**********************************************************************************************
*/
cdx_int16 AviBuildIdxForIdx1SequenceMode(CdxAviParserImplT *p)
{
    cdx_int32           ret = AVI_ERR_PARA_ERR;
    AviIndexEntryT      *mIdx;
    AviFileInT          *aviIn;
    cdx_int32           i, id;
    cdx_int32           frameCount = 0;

    cdx_uint32          leftIdxItem;
    cdx_uint32          bufIdxItem = 0;

    cdx_int32           curFilePos;
    OldIdxTableItemT    *pSeqKeyFrameTableEntry;
    cdx_uint32          *chunkBuf;
    cdx_int32        audChkCntrArray[MAX_AUDIO_STREAM] = {0}; //audio chunk, vbr, tmpAudChkCnt
    cdx_int64        audDatSizeCntrArray[MAX_AUDIO_STREAM] = {0};//tmpDatSize=0,audio data size.cbr

    cdx_int32           iAud = 0;
    cdx_int32           nKeyframeTableSize = 0; //keyframeTable OLD_IDX_TABLE_ITEM
    cdx_int32           nIdx1RecNum = 0;        //idx1 rec
    //MainAVIHeader       *tmpAviHdr;
    //AVIStreamHeader     *tmpAbsHdr;
    //WAVEFORMATEX        *tmpAbsFmt;
    //MEM_SET(AudChkCntrArray, 0, sizeof(CDX_S32)*MAX_AUDIO_STREAM);
    //MEM_SET(AudDatSizeCntrArray, 0, sizeof(CDX_S64)*MAX_AUDIO_STREAM);

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
    //check if the index table is exsit in media file idx1 indx
    if(USE_IDX1 != aviIn->idxStyle)
    {
        CDX_LOGE("avi_in->idx_style != USE_IDX1, fatal error!");
        return AVI_ERR_PARA_ERR;
    }

    if(!aviIn->idx1Total || !p->hasVideo)
    {
        CDX_LOGE("No valid index table in the media file!");
        return AVI_ERR_NO_INDEX_TABLE;
    }
    if(!aviIn->idx1Buf)
    {
        aviIn->idx1Buf = (cdx_uint32 *)malloc(MAX_IDX_BUF_SIZE);
        if(!aviIn->idx1Buf)
        {
            CDX_LOGE("Request buffer for build index failed.");
            return AVI_ERR_REQMEM_FAIL;
        }
    }
    memset(aviIn->idx1Buf, 0, MAX_IDX_BUF_SIZE);
    aviIn->idx1BufEnd = (cdx_uint32 *)((cdx_uint8 *)aviIn->idx1Buf + MAX_IDX_BUF_SIZE);
    nKeyframeTableSize = MAX_IDX_BUF_SIZE/sizeof(OldIdxTableItemT);
    //backup current file position
    curFilePos = CdxStreamTell(aviIn->fp);   //avi_in->movi_start

    pSeqKeyFrameTableEntry = (OldIdxTableItemT *)aviIn->idx1Buf;
    chunkBuf = (cdx_uint32 *)aviIn->dataChunk.buffer;
    frameCount = 0;

    if(CdxStreamSeek(aviIn->fp, (cdx_uint32)aviIn->idx1Start, STREAM_SEEK_SET) < 0)
    {
        CDX_LOGE("Seek file failed!");
        return AVI_ERR_READ_FILE_FAIL;
    }

    leftIdxItem = aviIn->idx1Total;
    bufIdxItem = 0;
    mIdx = (AviIndexEntryT *)chunkBuf;
    for(i = 0; i < aviIn->idx1Total; i++)
    {
        //check if need get file data
        if(p->bAbortFlag)
        {
            CDX_LOGV("idx1 seq:detect abort flag,quit now!");
            ret = AVI_ERR_ABORT;
            goto _err0;
        }
        if(bufIdxItem == 0)
        {
            //calculate need read file length
            if(leftIdxItem > MAX_CHUNK_BUF_SIZE/sizeof(AviIndexEntryT))
            {
                bufIdxItem = MAX_CHUNK_BUF_SIZE /sizeof(AviIndexEntryT);
                leftIdxItem -= bufIdxItem;
            }
            else
            {
                bufIdxItem = leftIdxItem;
                leftIdxItem = 0;
            }

            //check if need read file data
            if(bufIdxItem * sizeof(AviIndexEntryT) !=
             (cdx_uint32)CdxStreamRead(aviIn->fp, chunkBuf, bufIdxItem * sizeof(AviIndexEntryT)))
            {
                CDX_LOGE("Read error.");
                return AVI_ERR_READ_FILE_FAIL;
            }

            mIdx = (AviIndexEntryT *)chunkBuf;
        }

        id = CDX_STREAM_FROM_FOURCC(mIdx->ckid);
        if(id == p->videoStreamIndex)
        {
            if(i == 0)
            {
                aviIn->noPreLoad = 1;
            }
            if(mIdx->dwFlags & CDX_AVIIF_KEYFRAME)
            {
                pSeqKeyFrameTableEntry->frameIdx = frameCount;
                //CDX_LOGD("xxxxxxxxx keyframe idx(%d), AviBuildIdxForIdx1SequenceMode",
                                                        //pSeqKeyFrameTableEntry->frameIdx);
                pSeqKeyFrameTableEntry->vidChunkOffset = aviIn->ckBaseOffset + mIdx->dwChunkOffset;
                for(iAud = 0; iAud < p->hasAudio; iAud++)
                {
                    pSeqKeyFrameTableEntry->audPtsArray[iAud]
                        = (cdx_uint32)CalcAviAudioChunkPts(&aviIn->audInfoArray[iAud],
                            audDatSizeCntrArray[iAud], audChkCntrArray[iAud]);
                }
                CDX_LOGV("keyFrame:frmidx[%d],pts[%d]s, audioPts[%d]ms, mIdx->dwFlags[%u]",
                pSeqKeyFrameTableEntry->frameIdx,
                pSeqKeyFrameTableEntry->frameIdx*FRAME_RATE_BASE/p->aviFormat.vFormat.nFrameRate,
                pSeqKeyFrameTableEntry->audPtsArray[0], mIdx->dwFlags);
                aviIn->indexCountInKeyfrmTbl++;
                pSeqKeyFrameTableEntry++;
                if(nKeyframeTableSize <= aviIn->indexCountInKeyfrmTbl)
                {
                    CDX_LOGV("KeyframeTable full, cnt=%d, indextable_i[%d], indextablecnt[%d]\n",
                        aviIn->indexCountInKeyfrmTbl, i, aviIn->idx1Total);
                    break;
                }
            }
            frameCount++;
        }
        else if(mIdx->ckid == CDX_LIST_TYPE_AVI_RECORD)
        {
            CDX_LOGV("meet [rec ]");
            nIdx1RecNum++;
        }
        else    //audio chunk
        {
            for(iAud = 0; iAud < p->hasAudio; iAud++)
            {
                if(id == p->audioStreamIndexArray[iAud])
                {
                    audChkCntrArray[iAud] += CalcAviChunkAudioFrameNum(&aviIn->audInfoArray[iAud],
                        mIdx->dwChunkLength);
                    //LOGV("Audio[%d]FrameCount[%d]", iAud, AudChkCntrArray[iAud]);
                    audDatSizeCntrArray[iAud] += mIdx->dwChunkLength;
                    break;
                }
            }
            if(iAud == p->hasAudio && iAud > 0)
            {
                CDX_LOGV("stream id[%d] is not av, iAud[%d]\n", id, iAud);
            }
        }
        bufIdxItem--;
        mIdx++;
    }
    CDX_LOGV("KeyframeTable done, keyfrm entry cnt=%d, table_maxcnt=%d,"
        "total chunk[%d], video chunk[%d], nIdx1RecNum[%d]\n",
        aviIn->indexCountInKeyfrmTbl,  nKeyframeTableSize,
        aviIn->idx1Total - nIdx1RecNum, frameCount, nIdx1RecNum);

    if(p->totalFrames == 0)
    {
        p->totalFrames = frameCount;
    }

    //restore file position
    if(CdxStreamSeek(aviIn->fp, (cdx_int64)curFilePos, STREAM_SEEK_SET) < 0)
    {
        CDX_LOGE("Seek failed.");
        if(aviIn->idx1Buf)
        {
            free(aviIn->idx1Buf);
            aviIn->idx1Buf = NULL;
        }
        return AVI_ERR_READ_FILE_FAIL;
    }

    if(!aviIn->indexCountInKeyfrmTbl)
    {
        CDX_LOGE("No key frame in the index table!!!");

        if(aviIn->idx1Buf)
        {
            free(aviIn->idx1Buf);
            aviIn->idx1Buf = NULL;
        }
        return AVI_ERR_INDEX_HAS_NO_KEYFRAME;
    }

    return AVI_SUCCESS;
_err0:
    //restore file position
    CdxStreamSeek(aviIn->fp, (cdx_int64)curFilePos, STREAM_SEEK_SET);
    return ret;
}

/*******************************************************************************
Function name: search_index_audio_chunk
Description:
    1. search file_idx to match the video time, fill the IDX_TABLE_ITEM,
    2. default think: preader is inited before call this function.fp has already
        pointed to the index table.
    3. the audio chunk's pts must >= video chunk's pts, unless no audio chunk.
    4. fill IDX_TABLE_ITEM.

Parameters:
    vid_pts: (ms)
    pidx_item: the keyframe table's item. Its keyframe's pts is vid_pts!

Return:
    AVI_SUCCESS:
    AVI_ERR_SEARCH_INDEX_CHUNK_END:
    <AVI_SUCCESS
Time: 2009/5/19
*******************************************************************************/
cdx_int32 SearchAudioChunkIndxEntryOdmlSequence(ODML_SUPERINDEX_READER *pAudSuperReader,
                OldIdxTableItemT *pSeqEntry, cdx_int32 iAud, AudioStreamInfoT *pAudStrmInfo)
{
    cdx_int32   srchRet;
    AVI_CHUNK_POSITION  chunkPos;
    cdx_uint32   audPts;

    //audio chunk offset video chunk audio chunk
    if(iAud >= MAX_AUDIO_STREAM)
    {
        CDX_LOGV("fatal error!iAud[%d]>= MAX_AUDIO_STREAM[%d]\n", iAud, MAX_AUDIO_STREAM);
        return -1;
    }
    if(pAudSuperReader->chunkOffset > pSeqEntry->vidChunkOffset
        && pAudSuperReader->chunkCounter > 0) //find it pSeqEntry
    {
        audPts = (cdx_uint32)CalcAviAudioChunkPts(pAudStrmInfo,
                            pAudSuperReader->totalChunkSize - pAudSuperReader->chunkSize,
                            pAudSuperReader->chunkCounter - 1);
        pSeqEntry->audPtsArray[iAud] = audPts;
        return AVI_SUCCESS;
    }
    while(1)
    {
        srchRet = SearchNextODMLIndexEntry(pAudSuperReader, &chunkPos);
        if(AVI_SUCCESS == srchRet)
        {
             //audio chunk offset
            if(chunkPos.ckOffset >= pSeqEntry->vidChunkOffset) //find it pSeqEntry
            {
                if(chunkPos.ckOffset == pSeqEntry->vidChunkOffset)
                {
                    CDX_LOGV("file fatal error! audio chunk video chunk pos is same!\n");
                }
                audPts = (cdx_uint32)CalcAviAudioChunkPts(pAudStrmInfo,
                                    pAudSuperReader->totalChunkSize - pAudSuperReader->chunkSize,
                                    pAudSuperReader->chunkCounter - 1);
                pSeqEntry->audPtsArray[iAud] = audPts;
                break;
            }
        }
        else
        { //use last valid value to fill pidx_item.
            if(AVI_ERR_SEARCH_INDEX_CHUNK_END == srchRet)
            {
                CDX_LOGV("search aud chunk over!\n");
            }
            audPts = (cdx_uint32)CalcAviAudioChunkPts(pAudStrmInfo,
                                    pAudSuperReader->totalChunkSize - pAudSuperReader->chunkSize,
                                    pAudSuperReader->chunkCounter - 1);
            pSeqEntry->audPtsArray[iAud] = audPts;
            break;
        }
    }
    return srchRet;
}

/*******************************************************************************
Function name: AVI_build_idx_for_odml_sequence_mode
Description:
Parameters:

Return:
    >0:
    AVI_ERR_NO_INDEX_TABLE
    AVI_ERR_PART_INDEX_TABLE

    <0:
    AVI_ERR_PARA_ERR
    AVI_ERR_READ_FILE_FAIL
    AVI_ERR_REQMEM_FAIL
Time: 2010/8/22
*******************************************************************************/
cdx_int16 AviBuildIdxForOdmlSequenceMode(CdxAviParserImplT *p)
{
    AviFileInT         *aviIn;
    cdx_int32          i;
    cdx_int32          curFilePos;
    cdx_uint8          indexChunkOkFlag = 1;
    cdx_int32          ret;
    cdx_int32          srchRet;
    OldIdxTableItemT   *pSeqKeyFrameTableEntry = NULL;
    cdx_int32          nKeyframeTableSize = 0; //keyframeTable OLD_IDX_TABLE_ITEM
    cdx_int32          nStdindexEntryNum = 0;
    cdx_int32          nStdindexEntryCnt = 0;
    cdx_int32          nTotalSuperIndexDwDuration = 0;
    AVI_CHUNK_POSITION chunkPos;
    cdx_uint32         vframePts = 0; //PTS
    cdx_int32          iAud;   //audio stream
    AudioStreamInfoT   *pAudStrmInfo = NULL;

    if(!p)
    {
        CDX_LOGE("Check para.");
        return AVI_ERR_PARA_ERR;
    }
    //calculate audio bitstream time offset

    aviIn = (AviFileInT *)p->privData;
    //avi_in = (AVI_FILE_IN *)p;
    if(!aviIn)
    {
        CDX_LOGE("Check privData.");
        return AVI_ERR_PARA_ERR;
    }
    if(USE_INDX != aviIn->idxStyle)
    {
        CDX_LOGE("avi_in->idx_style != USE_INDX, fatal error!\n");
        return AVI_ERR_PARA_ERR;
    }
    //check if the index table is exist in media file idx1 indx
    if((!p->hasVideo) || (!aviIn->sInfo[p->videoStreamIndex]->isODML))
    {
        CDX_LOGE("No valid index table in the media file!");
        return AVI_ERR_NO_INDEX_TABLE;
    }

    if(!aviIn->idx1Buf)
    {
        aviIn->idx1Buf = (cdx_uint32 *)malloc(MAX_IDX_BUF_SIZE);
        if(!aviIn->idx1Buf)
        {
            CDX_LOGE("Request buffer for build index failed.");
            return AVI_ERR_REQMEM_FAIL;
        }
    }
    memset(aviIn->idx1Buf, 0, MAX_IDX_BUF_SIZE);
    aviIn->idx1BufEnd = (cdx_uint32 *)((cdx_uint8 *)aviIn->idx1Buf + MAX_IDX_BUF_SIZE);
    nKeyframeTableSize = MAX_IDX_BUF_SIZE/sizeof(OldIdxTableItemT);
    pSeqKeyFrameTableEntry = (OldIdxTableItemT *)aviIn->idx1Buf;

    //back current file position
    curFilePos = CdxStreamTell(aviIn->fp);
    //1,keyframe entry,audio chunk PTS video chunk PTS
    //1.1 indx table reader,idx_fp fp
    ret = InitialPsrIndxTableReader(&aviIn->vidIndxReader, aviIn, p->videoStreamIndex);
    if(ret != AVI_SUCCESS)
    {
        ret = AVI_ERR_NO_INDEX_TABLE;
        CDX_LOGE("initial psr indx failed.");
        goto _err0;
    }
    for(i =0; i<aviIn->vidIndxReader.indxTblEntryCnt; i++)
    {
        nStdindexEntryCnt += (aviIn->vidIndxReader.indxTblEntryArray+i)->dwDuration;
    }

    //1.2  keyframe entry
    while(1)
    {
        //chunk_body_size = 0;
        if(p->bAbortFlag)
        {
            CDX_LOGV("odml seq:detect abort flag,quit now1!");
            ret = AVI_ERR_ABORT;
            goto _err0;
        }
        srchRet = SearchNextODMLIndexEntry(&aviIn->vidIndxReader, &chunkPos);
        if(AVI_SUCCESS == srchRet)
        {
            nStdindexEntryNum++;
            //CDX_LOGD("nStdindexEntryNum (%d)", nStdindexEntryNum);
            //check if it is a key frame.
            if(1 == chunkPos.isKeyframe)
            {
                //because chunk_idx has increase 1 in search_next_ODML_index_entry()
                //CDX_LOGD("nStdindexEntryNum(%d), is keyframe.", nStdindexEntryNum);
                pSeqKeyFrameTableEntry->frameIdx = aviIn->vidIndxReader.chunkCounter - 1;
                //CDX_LOGD("xxxxxxxxx keyframe idx(%d)", pSeqKeyFrameTableEntry->frameIdx);
                pSeqKeyFrameTableEntry->vidChunkOffset = chunkPos.ckOffset;
                vframePts = (cdx_uint32)((cdx_int64)pSeqKeyFrameTableEntry->frameIdx *
                    p->aviFormat.nMicSecPerFrame / 1000);
                for(i = 0; i < p->hasAudio; i++)
                {
                    pSeqKeyFrameTableEntry->audPtsArray[i] = vframePts;
                }
                pSeqKeyFrameTableEntry++;
                aviIn->indexCountInKeyfrmTbl++;
                if(aviIn->indexCountInKeyfrmTbl >= nKeyframeTableSize)
                {
                    CDX_LOGW("FFRRKeyframe Table full,[%d] == [%d], cur_std_indx[%d],"
                        " total indx[%d], entry num[%d]\n",
                        aviIn->indexCountInKeyfrmTbl, nKeyframeTableSize,
                        aviIn->vidIndxReader.indxTblEntryIdx,
                        aviIn->vidIndxReader.indxTblEntryCnt, nStdindexEntryNum);
                    break;
                }
            }
        }
        else if(AVI_ERR_SEARCH_INDEX_CHUNK_END == srchRet)
        {
            //CDX_LOGD("AVI_ERR_SEARCH_INDEX_CHUNK_END");
            break;
        }
        else
        {
            CDX_LOGE("search ODML indx chunk error, [%d]\n", srchRet);
            indexChunkOkFlag = 0;
            break;
        }
    }
    DeinitialPsrIndxTableReader(&aviIn->vidIndxReader);
   // CDX_LOGD("odml seq: ffrrkeyframe entry cnt[%d]\n", aviIn->indexCountInKeyfrmTbl);
    if(!aviIn->indexCountInKeyfrmTbl)
    {
        CDX_LOGE("No key frame in the index table!!!\n");
        ret = AVI_ERR_INDEX_HAS_NO_KEYFRAME;
        goto _err0;;
    }
    //1.3 audio chunk PTS
    for(iAud = 0; iAud < p->hasAudio; iAud++)
    {
        if(iAud >= MAX_AUDIO_STREAM)
        {
            CDX_LOGW("iAud[%d] >= MAX_AUDIO_STREAM[%d]\n", iAud, MAX_AUDIO_STREAM);
            indexChunkOkFlag = 0;
            break;
        }
        ret = InitialPsrIndxTableReader(&aviIn->audIndxReader, aviIn,
            p->audioStreamIndexArray[iAud]);
        if(ret != AVI_SUCCESS)
        {
            indexChunkOkFlag = 0;
            DeinitialPsrIndxTableReader(&aviIn->audIndxReader);
            continue;
        }
        nTotalSuperIndexDwDuration = 0;
        for(i=0; i<aviIn->audIndxReader.indxTblEntryCnt; i++)
        {
            nTotalSuperIndexDwDuration += (aviIn->audIndxReader.indxTblEntryArray+i)->dwDuration;
        }
        CDX_LOGV("audio[stream num %d] super index entry total dwDuration is [%d]\n",
            p->audioStreamIndexArray[iAud], nTotalSuperIndexDwDuration);
        pSeqKeyFrameTableEntry = (OldIdxTableItemT *)aviIn->idx1Buf;
        pAudStrmInfo = &aviIn->audInfoArray[iAud];
        for(i=0; i<aviIn->indexCountInKeyfrmTbl; i++)
        {
            if(p->bAbortFlag)
            {
                CDX_LOGV("odml seq:detect abort flag,quit now2!\n");
                ret = AVI_ERR_ABORT;
                goto _err0;
            }
            srchRet = SearchAudioChunkIndxEntryOdmlSequence(&aviIn->audIndxReader,
                pSeqKeyFrameTableEntry, iAud, pAudStrmInfo);
            if(srchRet < AVI_SUCCESS)
            {
                if(srchRet == AVI_ERR_READ_FILE_FAIL)
                {
                    indexChunkOkFlag = 0;
                    break;
                }
                else //fatal error
                {
                    indexChunkOkFlag = 0;
                    DeinitialPsrIndxTableReader(&aviIn->audIndxReader);
                    ret = srchRet;
                    CDX_LOGE("errout...");
                    goto _err0;
                }
            }
            pSeqKeyFrameTableEntry++;
        }
        DeinitialPsrIndxTableReader(&aviIn->audIndxReader);
    }

    //1.4  table entry
    if(indexChunkOkFlag)
    {
        ret = AVI_SUCCESS;
    }
    else
    {
        ret = AVI_ERR_NO_INDEX_TABLE;   //if odml index chunk is not full,
    }                                   //we think avi file is not full.forbit jump!

    //restore file position
    if(CdxStreamSeek(aviIn->fp, (cdx_uint32)curFilePos, STREAM_SEEK_SET) < 0)
    {
        ret =  AVI_ERR_READ_FILE_FAIL;
        CDX_LOGE("seek failed.");
        goto _err0;
    }
    return ret;

_err0:
    if(aviIn->idx1Buf)
    {
        free(aviIn->idx1Buf);
        aviIn->idx1Buf = 0;
    }
    return ret;
}

/*
**********************************************************************************************
*                       GET INDEX BY TIME
*
*Description: This function looks up index table to get the key frame's position which
*             is most closed to the "time_ms".
*
*Arguments  : avi_in        global AVI file information
*             time_ms       the time to be searched in unit of ms
*             keyfrmidx     current key frame index in index table;
*             direction     search direction
*                           > 0, search forward,
*                           <=0, search backward,
*             last_or_next  the mode of get index
*                           =0, goto next key frame
*                           =1, goto last key frame,
*             file_pst      the key frame's position in the file
*             keyfrmnum     the key frame number
*             diff          the time difference between the video key frame and the
                            followed audio frame
*
*Return     : result of get index
*               = 0     get index successed;
                         ֻҪFFRRKeyframeTable
*               < 0     get index failed.
                        AVI_ERR_PARA_ERR:
*********************************************************************************************
*/
cdx_int16 AviGetIndexByMsReadMode0(CdxAviParserImplT *p, cdx_uint32 timeMs,
                    cdx_uint32 *keyfrmIdx, cdx_int32 direction, cdx_int32 lastOrNext,
                    cdx_uint64 *filePst, cdx_uint32 *keyfrmNum, cdx_int32 *diff)
{
    OldIdxTableItemT *pSeqEntry = NULL;
    cdx_int32         i;
    cdx_uint32        targetNum, indexNum, avDiff;
    cdx_uint64        pos;
    AviFileInT        *aviIn = (AviFileInT *)p->privData;

    if(!aviIn)
    {
        CDX_LOGE("aviIn is NULL.");
        return AVI_ERR_PARA_ERR;
    }

    if(!aviIn->idx1Buf)
    {
        CDX_LOGE("idx1Buf is NULL.");
        return AVI_ERR_PARA_ERR;
    }

    if(*keyfrmIdx >= (cdx_uint32)aviIn->indexCountInKeyfrmTbl)
    {
        CDX_LOGV("sth wrong! keyfrmidx_inkeyframetable[%d] >= total[%d]\n", *keyfrmIdx,
            aviIn->indexCountInKeyfrmTbl);
        if(aviIn->indexCountInKeyfrmTbl > 0)
        {
            // get the last key frame in the index table
            *keyfrmIdx = aviIn->indexCountInKeyfrmTbl - 1;
        }
        else
        {
            *keyfrmIdx = 0;
            return AVI_ERR_PARA_ERR;
        }
    }

    targetNum = (cdx_uint32)((cdx_int64)timeMs * 1000 / p->aviFormat.nMicSecPerFrame);

    //set search index table pointer
    pSeqEntry = (OldIdxTableItemT *)aviIn->idx1Buf + *keyfrmIdx;

    //avi_in->index_count_in_keyfrm_tbl>0
    if(direction > 0)
    {
        //forward search
        for(i=*keyfrmIdx; i<aviIn->indexCountInKeyfrmTbl; i++)
        {
            //index_num = *buf;
            indexNum = pSeqEntry->frameIdx;
            if(indexNum > targetNum)
            {
                if(i && lastOrNext)
                {
                    // point to previous key frame
                    pSeqEntry--;
                    i--;
                    indexNum = pSeqEntry->frameIdx;
                }
                pos = pSeqEntry->vidChunkOffset;
                avDiff = pSeqEntry->audPtsArray[p->curAudStreamNum];

                if(filePst)
                {
                    *filePst = pos;
                }
                if(keyfrmNum)
                {
                    *keyfrmNum = indexNum;
                }
                if(diff)
                {
                    *diff = avDiff;
                }
                *keyfrmIdx = i;
                return AVI_SUCCESS;
            }
            //buf += 3;
            pSeqEntry++;
        }

        //Not find the key frame, set to last key frame
        pSeqEntry--;
        i--;
        *keyfrmNum = pSeqEntry->frameIdx;
        *filePst = pSeqEntry->vidChunkOffset;
        *keyfrmIdx = i;
        if(diff)
        {
            *diff = pSeqEntry->audPtsArray[p->curAudStreamNum];
        }
        return AVI_SUCCESS;
    }
    else
    {
        //backward search
        for(i=*keyfrmIdx; i>=0; i--)
        {
            indexNum = pSeqEntry->frameIdx;
            if(indexNum < targetNum)
            {
                if(i<(aviIn->indexCountInKeyfrmTbl-1) && lastOrNext)
                {
                    // point to next key frame
                    pSeqEntry++;
                    i++;
                    indexNum = pSeqEntry->frameIdx;
                }
                pos = pSeqEntry->vidChunkOffset;
                avDiff = pSeqEntry->audPtsArray[p->curAudStreamNum];
                if(filePst)
                {
                    *filePst = pos;
                }
                if(keyfrmNum)
                {
                    *keyfrmNum = indexNum;
                }
                if(diff)
                {
                    *diff = avDiff;
                }
                *keyfrmIdx = i;
                //CDX_LOGD("xxx keyfrmidx =%d", i);
                return AVI_SUCCESS;
            }
            pSeqEntry--;
        }

        pSeqEntry++;
        i++;
        *keyfrmNum = pSeqEntry->frameIdx;
        *filePst = pSeqEntry->vidChunkOffset;
        *keyfrmIdx = 0;
        if(diff)
        {
            *diff = pSeqEntry->audPtsArray[p->curAudStreamNum];
        }
        return AVI_SUCCESS;
    }
}

/*******************************************************************************
Function name: __reconfig_avi_read_context_readmode0
Description:
    1. for readmode0(read_chunk_sequence mode).
    2. restore avi_read context when status change from FFRR to CDX_MEDIA_STATUS_PLAY.
       avi_in->index_in_keyfrm_tbl;
       avi_in->frame_index
       avi_in->uAudioPtsArray[];

Parameters:
    search_mode:
        1. last entry;
        0. next  entry
Return:
    AVI_SUCCESS
    AVI_ERR_FAIL
    AVI_ERR_READ_FILE_FAIL
Time: 2009/5/31
*******************************************************************************/
//searchDirection: 1 forward; 0 backward.
cdx_int32 ReconfigAviReadContextReadMode0(CdxAviParserImplT *p, cdx_uint32 vidTime,
                    cdx_int32 searchDirection, cdx_int32 searchMode)
{
    cdx_int32       i;
    cdx_uint32      tmpVideoTime = vidTime;
    cdx_uint64      filePst;
    AviFileInT      *aviIn = (AviFileInT *)p->privData;
    cdx_uint32      *tmpIdxBuf = (cdx_uint32 *)aviIn->idx1Buf;
    OldIdxTableItemT  *pSeqEntry = NULL;
#if 0
    if(p->status == CDX_MEDIA_STATUS_FORWARD)
    {
        if(!(aviIn->indexInKeyfrmTbl < aviIn->indexCountInKeyfrmTbl))
        {
            //reach the end of the i-index listkeyfrm
            aviIn->indexInKeyfrmTbl = aviIn->indexCountInKeyfrmTbl - 1;
        }

        if(avi_get_index_by_ms_readmode0(p, tmpVideoTime,
                                    (CDX_U32 *)&aviIn->indexInKeyfrmTbl,
                                    -1, 1, &filePst,
                                    (CDX_U32 *)&aviIn->frameIndex,
                                    NULL) < AVI_SUCCESS)
        {
            return AVI_ERR_FAIL;
        }
    }
    else if(p->status == CDX_MEDIA_STATUS_BACKWARD)
    {
        if((aviIn->indexInKeyfrmTbl < 0))
        {
            //reach the end of the i-index list
            aviIn->indexInKeyfrmTbl = 0;
        }

        if(avi_get_index_by_ms_readmode0(p, tmpVideoTime,
                (CDX_U32 *)&aviIn->indexInKeyfrmTbl,
                1, 0, &filePst,
                (CDX_U32 *)&aviIn->frameIndex,
                NULL) < AVI_SUCCESS)
        {
            return AVI_ERR_FAIL;
        }
    }
    else if((p->status == CDX_MEDIA_STATUS_IDLE)
            || (p->status == CDX_MEDIA_STATUS_STOP)
            || (p->status == CDX_MEDIA_STATUS_PLAY))
#endif
    { //jump play.
        if(searchDirection > 0)
        {
            aviIn->indexInKeyfrmTbl = 0;
        }
        else
        {
            aviIn->indexInKeyfrmTbl = aviIn->indexCountInKeyfrmTbl > 0
                                    ? aviIn->indexCountInKeyfrmTbl - 1 : 0;
        }
        if(AviGetIndexByMsReadMode0(p, tmpVideoTime,
                (cdx_uint32 *)&aviIn->indexInKeyfrmTbl,
                searchDirection, searchMode, &filePst,
                (cdx_uint32 *)&aviIn->frameIndex,
                NULL) < AVI_SUCCESS)
        {
            CDX_LOGE("AviGetIndexByMsReadMode0 failed.");
            return AVI_ERR_FAIL;
        }
    }

    pSeqEntry = (OldIdxTableItemT *)tmpIdxBuf + aviIn->indexInKeyfrmTbl;
    for(i = 0; i < p->hasAudio; i++)
    {
        aviIn->uBaseAudioPtsArray[i] = pSeqEntry->audPtsArray[i];
        aviIn->nAudChunkCounterArray[i] = 0;
        aviIn->nAudFrameCounterArray[i] = 0;
        aviIn->nAudChunkTotalSizeArray[i] = 0;
    }

    //seek file pointer to current key frame
    if(CdxStreamSeek(aviIn->fp, filePst, STREAM_SEEK_SET))
    {
        CDX_LOGE("file seek error.");
        return AVI_ERR_READ_FILE_FAIL;
    }

    return AVI_SUCCESS;
}

/*
*******************************************************************************************
*                           GET CHUNK HEADER
*
*Description: get chunk header information. for read avi file sequence mode.
*Arguments  : avi_file  avi reader handle;
                Because in cedar MOD, encapsulate a struct cdx_stream_info operation,
                read 64K more in buffer. So need use two fp to enhance performance.
*
*Return     : result;
******************************************************************************************
*/
cdx_int16 AviReaderGetChunkHeader(CdxAviParserImplT *p)
{
    cdx_uint32      length;
    cdx_uint64      pos;
    AviChunkT       *chunk = NULL;
    AviFileInT      *aviFile = (AviFileInT *)p->privData;

    if(!aviFile)
    {
        CDX_LOGE("aviFile NULL.");
        return AVI_ERR_PARA_ERR;
    }

    //check file end
    pos = CdxStreamTell(aviFile->fp);
    if(pos >= aviFile->fileSize)
    {
        CDX_LOGE("End of file.");
        return AVI_ERR_END_OF_MOVI;
    }

check_next_chunk_head:
    // read a data chunk
    if(GetNextChunkHead(aviFile->fp, &aviFile->dataChunk, &length) < 0)
    {
        CDX_LOGE("GetNextChunkHead failed.");
        return AVI_ERR_FILE_FMT_ERR;
    }

    chunk = &aviFile->dataChunk;
    if(chunk->fcc == CDX_LIST_HEADER_LIST)
    {
        //skip list header,  RIFF AVIX LIST movi LIST rec chunk......  moe_more.avi
        if(CdxStreamSeek(aviFile->fp, (cdx_int64)length, STREAM_SEEK_CUR))
        {//rec LIST,length,"rec " "rec "
            CDX_LOGE("seek avi file list header failed!");
            return AVI_ERR_READ_FILE_FAIL;
        }
        goto check_next_chunk_head;
    }
    else if(chunk->fcc == CDX_CKID_AVI_PADDING)
    {
        //skip JUNK
        if(CdxStreamSeek(aviFile->fp, (cdx_int64)length, STREAM_SEEK_CUR))
        {
            CDX_LOGE("seek avi file junk failed!");
            return AVI_ERR_READ_FILE_FAIL;
        }
        goto check_next_chunk_head;
    }

    if(chunk->fcc == CDX_CKID_AVI_NEWINDEX && !aviFile->hasIndx)    //AVI1.0 idx1
    {
        CDX_LOGV("End of file.");
        return AVI_ERR_END_OF_MOVI;
    }
    else if(aviFile->hasIndx) //AVI2.0
    {
        if(chunk->fcc == CDX_FORM_HEADER_RIFF)
        {
            //skip 'AVIX'
            if(CdxStreamSeek(aviFile->fp, (cdx_int64)length, STREAM_SEEK_CUR) < 0)
            {
                CDX_LOGE("seek avi file list header failed!");
                return AVI_ERR_READ_FILE_FAIL;
            }
            goto check_next_chunk_head;
        }
        else if(chunk->fcc == CDX_CKID_AVI_NEWINDEX || IsIndxChunkId(chunk->fcc))
        {
            //skip 'ix00' or 'ix01' or 'idx1'
            if(CdxStreamSeek(aviFile->fp, (cdx_int64)length, STREAM_SEEK_CUR))
            {
                CDX_LOGV("seek avi file list header failed!");
                return AVI_ERR_READ_FILE_FAIL;
            }
            goto check_next_chunk_head;
        }
    }

    //Do we need to add the chunk length checking? Sometimes the length is
    //very very big becuase of error.
    //GetNextChunkInfo() chunk
    return AVI_SUCCESS;
}

/*******************************************************************************
Function name: AVI_read_sequence
Description:
Parameters:

Return:

Time: 2010/8/30
*******************************************************************************/
cdx_int16 AviReadSequence(CdxAviParserImplT *p)
{
    AviFileInT     *aviIn;
    cdx_int32       ret;

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

    ret = AviReaderGetChunkHeader(p);
    if(ret != AVI_SUCCESS)
    {
        CDX_LOGE("get chunk header fail,ret[%d].", ret);
    }
    return ret;
}

