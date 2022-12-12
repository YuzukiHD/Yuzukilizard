/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviOdmlIndx.c
 * Description : Part of avi parser.
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxAviOdmlIndx"

#include <CdxAviInclude.h>
#include <string.h>

/*******************************************************************************
Function name: IsIndxChunkId
Description:
    1.��ⷶΧ:ix00 ~ ix09
Parameters:

Return:
    AVI_TRUE:��
    AVI_FALSE:��
Time: 2010/8/30
*******************************************************************************/
cdx_int32 IsIndxChunkId(FOURCC fcc)
{
    switch(fcc)
    {
        case CDX_CKID_IX00:
        case CDX_CKID_IX01:
        case CDX_CKID_IX02:
        case CDX_CKID_IX03:
        case CDX_CKID_IX04:
        case CDX_CKID_IX05:
        case CDX_CKID_IX06:
        case CDX_CKID_IX07:
        case CDX_CKID_IX08:
        case CDX_CKID_IX09:
            return AVI_TRUE;
        default:
            return AVI_FALSE;
    }
}

/*******************************************************************************
Function name: config_indx_chunk_info
Description:
    1. fcc and size will read outside this function.
    2. ��ʼ��ODML_INDEX_READER
Parameters:

Return:

Time: 2009/6/2
*******************************************************************************/
cdx_int32 ConfigIndxChunkInfo(ODML_INDEX_READER *pReader, AviStdIndexChunkT *pCkhead)
{
    cdx_int32   ret = AVI_SUCCESS;

    if(NULL == pReader || NULL == pCkhead)
    {
        return AVI_ERR_PARA_ERR;
    }
    pReader->bIndexSubType = pCkhead->bIndexSubType;
    pReader->wLongsPerEntry = pCkhead->wLongsperEntry;
    pReader->dwChunkId = pCkhead->dwChunkId;
    pReader->nEntriesInUse = pCkhead->nEntriesInUse;
    pReader->qwBaseOffset = (cdx_int64)pCkhead->baseOffsetHigh<<32 | pCkhead->baseOffsetLow;
    pReader->leftIdxItem = pReader->nEntriesInUse;
    pReader->bufIdxItem = 0;
    pReader->ckReadEndFlag = 0;
    pReader->pIdxEntry = NULL;

    //verify
    switch(pReader->bIndexSubType)
    {
        case CDX_AVI_INDEX_2FIELD:
        {
            if(pReader->wLongsPerEntry != 3)
            {
                CDX_LOGV("wLongsPerEntry!=3");
                pReader->wLongsPerEntry = 3;
                ret = AVI_ERR_FILE_DATA_WRONG;
            }
            break;
        }
        case CDX_AVI_INDEX_SUBTYPE_1FRMAE:
        {
            if(pReader->wLongsPerEntry != 2)
            {
                CDX_LOGV("wLongsPerEntry!=2");
                pReader->wLongsPerEntry = 2;
                ret = AVI_ERR_FILE_DATA_WRONG;
            }
            break;
        }
        default:
        {
            CDX_LOGV("bIndexSubType[%d] wrong.", pReader->bIndexSubType);
            ret = AVI_ERR_FILE_DATA_WRONG;
            break;
        }
    }
    return ret;
}

/*******************************************************************************
Function name: load_indx_chunk
Description:
    1. by indxtbl_entry_idx, select a index chunk to config ODML_INDEX_READER.
    2. when function over, pSuperReader->fp_pos is point to index chunk body!
    3. ���غ�˳���ʼ��ODML_INDEX_READER: pSuperReader->odml_ix_reader
    4. û�ж�indx�е����
Parameters:

Return:
    AVI_SUCCESS;
    AVI_ERR_PARA_ERR;
Time: 2009/6/4
*******************************************************************************/
cdx_int32 LoadIndxChunk(ODML_SUPERINDEX_READER *pSuperReader, cdx_int32 indxTblEntryIdx)
{
    CdxStreamT      *fp;
    AviChunkT       chunk;
    cdx_uint32      length;
    cdx_int32       ret;
    ODML_INDEX_READER   *indxReader = NULL;
    struct OdmlIndxTblEntry *pSuperIndxEntry = NULL;

    if(NULL == pSuperReader || NULL == pSuperReader->idxFp
        || NULL == pSuperReader->indxTblEntryArray)
    {
        CDX_LOGE("impossile case, check!");
        return AVI_ERR_PARA_ERR;
    }

    pSuperIndxEntry = pSuperReader->indxTblEntryArray + indxTblEntryIdx;
    pSuperReader->indxTblEntryIdx = indxTblEntryIdx;
    fp = pSuperReader->idxFp;
    indxReader = &pSuperReader->odmlIdxReader;

    if(CdxStreamSeek(fp, pSuperIndxEntry->qwOffset, STREAM_SEEK_SET))
    {
        CDX_LOGE("cdx_seek error.");
        return AVI_ERR_READ_FILE_FAIL;
    }
    ret = GetNextChunkHead(fp, &chunk, &length); //fp read 8 byte here.
    if(ret < AVI_SUCCESS)
    {
        return ret;
    }
    indxReader->fcc = chunk.fcc;
    indxReader->size = chunk.length;
    //verify indx chunk length.
    if(indxReader->size != pSuperIndxEntry->dwSize - 8)
    {
        //some file not same, so we ignore verify here.
        CDX_LOGV("indx chunk size not same: [%d, %d]\n", indxReader->size, pSuperIndxEntry->dwSize);
        //return AVI_ERR_FILE_DATA_WRONG;
    }
    if(24 != CdxStreamRead(fp, indxReader->pFileBuffer, 24))//fp read 24 byte here
    {
        return AVI_ERR_READ_FILE_FAIL;
    }
    pSuperReader->fpPos = CdxStreamTell(fp);
    ConfigIndxChunkInfo(indxReader,(AviStdIndexChunkT *)indxReader->pFileBuffer);
    if(indxReader->bIndexSubType != pSuperReader->bIndexSubType
        || indxReader->dwChunkId != pSuperReader->dwChunkId)
    {
        CDX_LOGV("something wrong, bIndexSubType[%d, %d], dwChunkId[%x, %x]\n",
            indxReader->bIndexSubType, pSuperReader->bIndexSubType,
            indxReader->dwChunkId, pSuperReader->dwChunkId);
    }
    return AVI_SUCCESS;
}

/*******************************************************************************
Function name: search_indx_entry
Description:

Parameters:

Return:
    <= AVI_SUCCESS;
    AVI_ERR_PARA_ERR
    AVI_ERR_SEARCH_INDEX_CHUNK_END;
Time: 2009/6/3
*******************************************************************************/
cdx_int32 SearchIndxEntry(ODML_SUPERINDEX_READER *pSuperReader, AVI_CHUNK_POSITION *pCkPos)
{
    ODML_INDEX_READER   *pReader = NULL;
    cdx_int32   entrySize = 0;
    cdx_uint8    findFlag = 0;
    AviFieldIndexEntyT *pFieldIndex = NULL;
    pReader = &pSuperReader->odmlIdxReader;
    entrySize = pReader->wLongsPerEntry * 4;   //unit: byte.
    if(entrySize <= 0 || NULL == pCkPos)
    {
        return AVI_ERR_PARA_ERR;
    }
    pCkPos->ixTblEntOffset = -1;
    pCkPos->ckOffset = -1;
    pCkPos->isKeyframe = 0;
    pCkPos->chunkBodySize = 0;
    while(pReader->bufIdxItem > 0 || pReader->leftIdxItem > 0)
    {
        //1. check if load index.
        if(0 == pReader->bufIdxItem)
        {
            if(pReader->leftIdxItem > INDEX_CHUNK_BUFFER_SIZE/entrySize)
            {
                pReader->bufIdxItem = INDEX_CHUNK_BUFFER_SIZE/entrySize;
                pReader->leftIdxItem -= pReader->bufIdxItem;
            }
            else
            {
                pReader->bufIdxItem = pReader->leftIdxItem;
                pReader->leftIdxItem = 0;
            }
            if(CdxStreamSeek(pSuperReader->idxFp, pSuperReader->fpPos, STREAM_SEEK_SET))
            {
                CDX_LOGE("file seek error!\n");
                return AVI_ERR_READ_FILE_FAIL;
            }
            if((pReader->bufIdxItem * entrySize) !=
                CdxStreamRead(pSuperReader->idxFp, pReader->pFileBuffer, pReader->bufIdxItem *
                entrySize))
            {
                CDX_LOGE("file read error!\n");
                return AVI_ERR_READ_FILE_FAIL;
            }
            pReader->pIdxEntry = pReader->pFileBuffer;   //AVIINDEXENTRY
            pSuperReader->fpPos += pReader->bufIdxItem * entrySize;
        }
        pFieldIndex = (AviFieldIndexEntyT *)pReader->pIdxEntry;

        pCkPos->ckOffset = pReader->qwBaseOffset + pFieldIndex->offset - 8;
        pCkPos->ixTblEntOffset = pSuperReader->fpPos - pReader->bufIdxItem * entrySize;
        pCkPos->isKeyframe = (~ pFieldIndex->size >> 31) & 0x01;
        pCkPos->chunkBodySize = pFieldIndex->size & (~(1<<31));
        pReader->bufIdxItem--;
        pReader->pIdxEntry += entrySize;

        pSuperReader->chunkCounter++;
        pSuperReader->chunkSize = pFieldIndex->size&(~(1<<31));
        pSuperReader->chunkIxTblEntOffset = pCkPos->ixTblEntOffset;
        pSuperReader->chunkOffset = pCkPos->ckOffset;
        pSuperReader->totalChunkSize += pSuperReader->chunkSize;
        findFlag = 1;
        break;
    }
    if(1 == findFlag)
    {
        return AVI_SUCCESS;
    }
    else
    {
        CDX_LOGV("this indx chunk search over.\n");
        pCkPos->ckOffset = -1;
        pCkPos->ixTblEntOffset = -1;
        pReader->ckReadEndFlag = 1;
        return AVI_ERR_SEARCH_INDEX_CHUNK_END;
    }
}

/*******************************************************************************
Function name: search_next_ODML_index_entry
Description:
    1. indx_reader is init.
    2. search a chunk which chunkid is match to reader->dwChunkId.
    3. search all indx chunk!

Parameters:
    *ck_offset: chunk offset.
    *ixtbl_ent_offset: chunk index entry offset.
    *pchunk_body_size:indx chunkbody
Return:
    AVI_ERR_PARA_ERR;
    AVI_ERR_READ_FILE_FAIL
    AVI_ERR_SEARCH_INDEX_CHUNK_END
    AVI_SUCCESS
    //AVI_ERR_FILE_DATA_WRONG
Time: 2009/6/2
*******************************************************************************/
cdx_int32 SearchNextODMLIndexEntry(ODML_SUPERINDEX_READER *pSuperReader, AVI_CHUNK_POSITION *pCkPos)
{
    cdx_int32   ret;
    ODML_INDEX_READER   *indxReader;
    if(NULL == pSuperReader || NULL == pSuperReader->idxFp
        || pSuperReader->indxTblEntryCnt <= 0 || NULL == pCkPos)
    {
        CDX_LOGE("search_next_ODML_index_entry() para error.");
        return AVI_ERR_PARA_ERR;
    }
    //*pchunk_body_size = 0;
    indxReader = &pSuperReader->odmlIdxReader;
    pCkPos->ckOffset = -1;
    pCkPos->ixTblEntOffset = -1;
    pCkPos->isKeyframe = 0;
    pCkPos->chunkBodySize = 0;
    if(NULL == indxReader->pFileBuffer || NULL == pSuperReader->indxTblEntryArray)
    {
        CDX_LOGE("search_next_ODML_index_entry() para error2.");
        return AVI_ERR_PARA_ERR;
    }
    if(1 == pSuperReader->readEndFlg)
    {
        return AVI_ERR_SEARCH_INDEX_CHUNK_END;
    }
    if(pSuperReader->indxTblEntryIdx >= 0)
    {
        ret = SearchIndxEntry(pSuperReader, pCkPos);
        if(ret == AVI_SUCCESS)
        {
            return ret;
        }
        else if(ret < AVI_SUCCESS)
        {
            return ret;
        }
        else if(AVI_ERR_SEARCH_INDEX_CHUNK_END == ret)
        {
            CDX_LOGV("indx_chunk[%d] is search over\n", pSuperReader->indxTblEntryIdx);
            pSuperReader->indxTblEntryIdx++;
        }
        else
        {
            CDX_LOGV("exception happens, ret=%d\n", ret);
            return ret;
        }
    }
    else //indicate we will start to search.
    {
        pSuperReader->indxTblEntryIdx = 0;
    }

    while(pSuperReader->indxTblEntryIdx < pSuperReader->indxTblEntryCnt)
    {
        //load index table to filebuffer. init ODML_INDEX_READER.
        ret = LoadIndxChunk(pSuperReader, pSuperReader->indxTblEntryIdx);
        if(ret < AVI_SUCCESS)
        {
            return ret;
        }
        ret = SearchIndxEntry(pSuperReader, pCkPos);
        if(ret <= AVI_SUCCESS)
        {
            return ret;
        }
        else if(AVI_ERR_SEARCH_INDEX_CHUNK_END == ret)
        {
            CDX_LOGV("indx_chunk[%d] is search over\n", pSuperReader->indxTblEntryIdx);
            pSuperReader->indxTblEntryIdx++;
        }
        else
        {
            CDX_LOGV("exception happens, ret=%d.", ret);
            return ret;
        }
    }
    if(pSuperReader->indxTblEntryIdx >= pSuperReader->indxTblEntryCnt)
    {
        pSuperReader->readEndFlg = 1;
        return AVI_ERR_SEARCH_INDEX_CHUNK_END;
    }
    else
    {
        return AVI_EXCEPTION;
    }
}

/*******************************************************************************
Function name: search_index_audio_chunk
Description:
    1. search file_idx to match the video time, fill the IDX_TABLE_ITEM,
    2. default think: preader is inited before call this function.fp has already
        pointed to the index table.
    3. the audio chunk's pts must >= video chunk's pts, unless no audio chunk.
    4. fill IDX_TABLE_ITEM.
    5. רΪodml avi��ʽ����index��ȡ��ʽ��,��ffrrkeyframetableר��
Parameters:
    vidPts: (ms)
    pidxItem: the keyframe table's item. Its keyframe's pts is vidPts!

Return:
    AVI_SUCCESS:
    AVI_ERR_SEARCH_INDEX_CHUNK_END:
    <AVI_SUCCESS
Time: 2009/5/19
*******************************************************************************/
cdx_int32 SearchAudioChunkIndxEntryOdmlIndex(ODML_SUPERINDEX_READER *pSuperReader,
                                cdx_uint32 vidPts, IdxTableItemT *pidxItem,
                                cdx_int32 iAud, AudioStreamInfoT *pAudStrmInfo)
{
    cdx_int32   srchRet;
    AVI_CHUNK_POSITION  chunkPos;
    cdx_uint32   audPts;
    if(pSuperReader->chunkCounter > 0)  //��ʾ���ٶ���һ��audio chunk
    {
        audPts = (cdx_uint32)CalcAviAudioChunkPts(pAudStrmInfo,
                    pSuperReader->totalChunkSize - pSuperReader->chunkSize,
                    pSuperReader->chunkCounter-1);
        if(vidPts <= audPts)  //���Ҫ��
        {
            pidxItem->audPtsArray[iAud] = audPts;
            pidxItem->audChunkIndexOffsetArray[iAud] = pSuperReader->chunkIxTblEntOffset;
            return AVI_SUCCESS;
        }
    }
    while(1)
    {
        srchRet = SearchNextODMLIndexEntry(pSuperReader, &chunkPos);
        if(AVI_SUCCESS == srchRet)
        {
            audPts = (cdx_uint32)CalcAviAudioChunkPts(pAudStrmInfo,
                pSuperReader->totalChunkSize - pSuperReader->chunkSize,
                pSuperReader->chunkCounter - 1);

            if(audPts >= vidPts)//we find it!
            {
                pidxItem->audPtsArray[iAud] = audPts;
                pidxItem->audChunkIndexOffsetArray[iAud] = pSuperReader->chunkIxTblEntOffset;
                //LOGV("vidPts[%d], aud_ck_idx[%x]\n", vidPts, pidxItem->aud_chunk_idx);
                break;
            }
        }
        else
        { //use last valid value to fill pidxItem.
            if(AVI_ERR_SEARCH_INDEX_CHUNK_END == srchRet)
            {
                CDX_LOGV("search aud chunk over!\n");
            }
            audPts = (cdx_uint32)CalcAviAudioChunkPts(pAudStrmInfo,
                pSuperReader->totalChunkSize - pSuperReader->chunkSize,
                pSuperReader->chunkCounter - 1);
            pidxItem->audPtsArray[iAud] = audPts;
            pidxItem->audChunkIndexOffsetArray[iAud] = pSuperReader->chunkIxTblEntOffset;
            break;
        }
    }
    return srchRet;
}

cdx_int16 AviBuildIdxForODMLIndexMode(CdxAviParserImplT *p)
{
    cdx_int32   ret;
    cdx_int32   srchRet;
    cdx_int32   i;
    cdx_int32   curFilePos = 0;
    cdx_int32   remainIdxCnt = 0; //record the number of idx item in keyframe table.
    cdx_uint32  vidPts = 0;

    cdx_int32   iAud=0;
    cdx_uint8   indexChunkOkFlag = 1;
    AviFileInT  *aviIn = NULL;
    //MainAVIHeader   *tmpAviHdr = NULL;
    //AVIStreamHeader *tmpAbsHdr = NULL;
    //WAVEFORMATEX    *tmpAbsFmt = NULL;
    IdxTableItemT  *pIdxItem = NULL;
    AVI_CHUNK_POSITION  chunkPos;
    ODML_SUPERINDEX_READER  *vidReader = NULL;
    ODML_SUPERINDEX_READER  *audReader = NULL;
    AudioStreamInfoT     *pAudStrmInfo = NULL;

    if(!p)
    {
        CDX_LOGE("Check the para...");
        return AVI_ERR_PARA_ERR;
    }
    aviIn = (AviFileInT *)p->privData;
    if(!aviIn)
    {
        CDX_LOGE("Check privData...");
        return AVI_ERR_PARA_ERR;
    }
    if(READ_CHUNK_BY_INDEX != aviIn->readmode || USE_INDX != aviIn->idxStyle)
    {
        CDX_LOGE("readmode[%d], idxStyle[%d]", aviIn->readmode, aviIn->idxStyle);
        return AVI_ERR_PARA_ERR;
    }

    //check if the index table is exist in media file
    if(!aviIn->hasIndx || !p->hasVideo)
    {
        CDX_LOGW("No valid index table in the media file!");
        return AVI_ERR_NO_INDEX_TABLE;
    }

    //malloc for FFRR_index_table
    if(!aviIn->idx1Buf)
    {
        aviIn->idx1Buf = (cdx_uint32 *)malloc(MAX_IDX_BUF_SIZE_FOR_INDEX_MODE);
        if(!aviIn->idx1Buf)
        {
            CDX_LOGE("Request buffer for build index failed.");
            return AVI_ERR_REQMEM_FAIL;
        }
    }
    memset(aviIn->idx1Buf, 0, MAX_IDX_BUF_SIZE_FOR_INDEX_MODE);
    aviIn->idx1BufEnd = (cdx_uint32*)((cdx_uint8 *)aviIn->idx1Buf +
        MAX_IDX_BUF_SIZE_FOR_INDEX_MODE);

    //backup current file pointer.
    curFilePos = CdxStreamTell(aviIn->fp);   //movi_start

    //1.first build video key frame table.
    CDX_LOGV("build video key frame table.");
    ret = InitialPsrIndxTableReader(&aviIn->vidIndxReader, aviIn, p->videoStreamIndex);
    if(ret != AVI_SUCCESS)
    {
        CDX_LOGV("InitialPsrIndxTableReader fail,ret[%d]\n", ret);
        DeinitialPsrIndxTableReader(&aviIn->vidIndxReader);
        goto __err0_idx1_buf;
    }
    vidReader = &aviIn->vidIndxReader;
    if(vidReader->indxTblEntryCnt <= 0)
    {
        CDX_LOGV("No valid index table in the media file!\n");
        ret = AVI_ERR_NO_INDEX_TABLE;
        goto __err0_idx1_buf;
    }
    remainIdxCnt = MAX_IDX_BUF_SIZE_FOR_INDEX_MODE/sizeof(IdxTableItemT);
    pIdxItem = (IdxTableItemT *)aviIn->idx1Buf;
    while(1)
    {
        //chunk_body_size = 0;
        if(p->bAbortFlag)
        {
            CDX_LOGV("odml index:detect abort flag,quit now!\n");
            ret = AVI_ERR_ABORT;
            goto __err0_idx1_buf;
        }
        srchRet = SearchNextODMLIndexEntry(vidReader, &chunkPos);
        if(AVI_SUCCESS == srchRet)
        {
            //check if it is a key frame.
            if(1 == chunkPos.isKeyframe)
            {
                //because chunk_idx has increase 1 in search_next_ODML_index_entry()
                pIdxItem->frameIdx = vidReader->chunkCounter - 1;
                //CDX_LOGD("xxxxxxxxx keyframe idx(%d), AviBuildIdxForODMLIndexMode",
                //pIdxItem->frameIdx);
                pIdxItem->vidChunkOffset = chunkPos.ckOffset;
                pIdxItem->vidChunkIndexOffset = chunkPos.ixTblEntOffset;
                pIdxItem++;

                aviIn->indexCountInKeyfrmTbl++;
                remainIdxCnt--;
                if(remainIdxCnt<=0)
                {
                    CDX_LOGV("idx1 buffer full! indx_idx[%d], indx_total[%d]\n",
                        vidReader->indxTblEntryIdx, vidReader->indxTblEntryCnt);
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
            CDX_LOGE("search ODML indx chunk error\n");
            DeinitialPsrIndxTableReader(&aviIn->vidIndxReader);
            ret = srchRet;
            goto __err0_idx1_buf;
            //return srchRet;
        }
    }
    DeinitialPsrIndxTableReader(&aviIn->vidIndxReader);
    CDX_LOGV("KeyframeTable odml index video done, cnt=%d, maxcnt=%d\n",
        aviIn->indexCountInKeyfrmTbl,  MAX_IDX_BUF_SIZE_FOR_INDEX_MODE/sizeof(IdxTableItemT));

    //2. fill video key frame table with audio info, if it exist.
    audReader = &aviIn->audIndxReader;
    //1.3��дaudio chunk��PTS
    for(iAud=0; iAud<p->hasAudio; iAud++)
    {
        if(iAud >= MAX_AUDIO_STREAM)
        {
            CDX_LOGE("iAud[%d] >= MAX_AUDIO_STREAM[%d]", iAud, MAX_AUDIO_STREAM);
            indexChunkOkFlag = 0;
            break;
        }
        ret = InitialPsrIndxTableReader(&aviIn->audIndxReader, aviIn,
            p->audioStreamIndexArray[iAud]);
        if(ret != AVI_SUCCESS)
        {
            indexChunkOkFlag = 0;
            DeinitialPsrIndxTableReader(&aviIn->audIndxReader);
            break;
        }
        pIdxItem = (IdxTableItemT *)aviIn->idx1Buf;
        pAudStrmInfo = &aviIn->audInfoArray[iAud];
        for(i=0; i<aviIn->indexCountInKeyfrmTbl; i++)
        {
            if(p->bAbortFlag)
            {
                CDX_LOGE("odml index : detect abort flag,quit now2!\n");
                ret = AVI_ERR_ABORT;
                goto __err0_idx1_buf;
            }
            vidPts = (cdx_uint32)pIdxItem->frameIdx*p->aviFormat.nMicSecPerFrame/1000; //unit : ms
            srchRet = SearchAudioChunkIndxEntryOdmlIndex(audReader, vidPts, pIdxItem,
                iAud, pAudStrmInfo);
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
                    CDX_LOGE("search failed.");
                    goto __err0_idx1_buf;
                }
            }
            pIdxItem++;
        }
        DeinitialPsrIndxTableReader(&aviIn->audIndxReader);
        if(0 == indexChunkOkFlag)
        {
            break;
        }
    }

    //1.4 ������ϣ����ߵ����table��һ����entry
    if(indexChunkOkFlag)
    {
        ret = AVI_SUCCESS;
    }
    else
    {
        ret = AVI_ERR_NO_INDEX_TABLE;
        goto __err0_idx1_buf;
    }

    if(CdxStreamSeek(aviIn->fp, (cdx_uint32)curFilePos, STREAM_SEEK_SET))
    {
        CDX_LOGE("seek failed.");
        return AVI_ERR_FILE_FMT_ERR;
    }
    if(!aviIn->indexCountInKeyfrmTbl)
    {
        CDX_LOGW("No key frame in the index table!!!");

        if(aviIn->idx1Buf)
        {
            free(aviIn->idx1Buf);
            aviIn->idx1Buf = 0;
        }
        return AVI_ERR_INDEX_HAS_NO_KEYFRAME;
    }
    if(p->hasVideo)
    {
        ret = InitialPsrIndxTableReader(vidReader, aviIn, p->videoStreamIndex);
        if(ret != AVI_SUCCESS)
        {
            CDX_LOGE("initial odml indx fail, ret[%d]",ret);
            goto __err0_idx1_buf;
        }
    }
    if(p->hasAudio)
    {
        ret = InitialPsrIndxTableReader(audReader, aviIn, p->audioStreamIndex);
        if(ret != AVI_SUCCESS)
        {
            CDX_LOGE("initial odml indx fail, ret[%d]",ret);
            goto __err0_idx1_buf;
        }
    }
    return AVI_SUCCESS;

__err0_idx1_buf:
    if(aviIn->idx1Buf)
    {
        free(aviIn->idx1Buf);
        aviIn->idx1Buf = 0;
    }
    DeinitialPsrIndxTableReader(&aviIn->vidIndxReader);
    DeinitialPsrIndxTableReader(&aviIn->audIndxReader);
    return ret;
}

/*******************************************************************************
Function name: init_psr_indx_table_reader
Description:
    1. we don't init ODML_INDEX_READER, we only init ODML_SUPERINDEX_READER.
    2. ODML_INDEX_READER will init in read_index status.
    3. we will malloc mem for some pointer, so need deinit_psr_indx_table_reader()
        to free mem.
        Don't call init_psr_indx_table_reader() repeatly and not
        call deinit_psr_indx_table_reader().

    4.��ʼ��preader�����ɱ�����
      �����ڴ��preader->indxtbl_entry_array��preader->odml_ix_reader.

    5.��ʼ��ʱ����load indx chunk!
Parameters:

Return:
    >0:
    AVI_ERR_NO_INDEX_TABLE,

    =0:
    AVI_SUCCESS;

    <0:
    AVI_ERR_PARA_ERR,
    AVI_ERR_FAIL
Time: 2009/6/2
*******************************************************************************/
cdx_int32 InitialPsrIndxTableReader(ODML_SUPERINDEX_READER *pReader, AviFileInT *aviIn,
    cdx_int32 streamIndex)
{
    cdx_int32   i;
    cdx_int32   ret;
    AviStreamInfoT *sInfo = aviIn->sInfo[streamIndex];
    CdxStreamT    *indxFp = aviIn->idxFp;
    if(NULL == sInfo || !sInfo->isODML || NULL == sInfo->indx || NULL == pReader)
    {
        CDX_LOGE("no ODML indx, fatal error!");
        return AVI_ERR_PARA_ERR;
    }
    if(aviIn->readmode == READ_CHUNK_SEQUENCE)
    {
        CDX_LOGD("odml avi in readmode=READ_CHUNK_SEQUENCE, use avi_in->fp");
        indxFp = aviIn->fp;   //because in readmode READ_CHUNK_SEQUENCE, idx_fp is NULL.
    }
    else
    {
        indxFp = aviIn->idxFp;
    }
    memset(pReader, 0, sizeof(ODML_SUPERINDEX_READER));
    if(CDX_AVI_INDEX_OF_INDEXES == sInfo->indx->bIndexType)
    {
        pReader->bIndexSubType = sInfo->indx->bIndexSubType;
        pReader->dwChunkId = sInfo->indx->dwChunkId;//byte low->high, "00db"
        pReader->indxTblEntryCnt = sInfo->indx->nEntriesInUse;
        pReader->indxTblEntryIdx = -1;
        pReader->fpSuperindexTable = pReader->fpPos = sInfo->indxTblOffset;
        //first default point to super index chunk's header.
        pReader->idxFp = indxFp;
        pReader->readEndFlg = 0;
        CDX_LOGV("stream index[%d], indxtbl_entry_cnt[%d]\n", streamIndex,
            pReader->indxTblEntryCnt);
        if(pReader->indxTblEntryCnt > 0)
        {
            pReader->indxTblEntryArray = malloc(pReader->indxTblEntryCnt *
                sizeof(struct OdmlIndxTblEntry));
            if(NULL == pReader->indxTblEntryArray)
            {
                ret = AVI_ERR_REQMEM_FAIL;
                goto _error1;
            }
            memset(pReader->indxTblEntryArray, 0, pReader->indxTblEntryCnt *
                sizeof(struct OdmlIndxTblEntry));
            for(i=0; i<pReader->indxTblEntryCnt; i++)
            {
                (pReader->indxTblEntryArray + i)->qwOffset
                    = (cdx_int64)sInfo->indx->aIndex[i]->offsetHigh<<32 |
                    sInfo->indx->aIndex[i]->offsetLow;
                (pReader->indxTblEntryArray + i)->dwSize = sInfo->indx->aIndex[i]->size;
                (pReader->indxTblEntryArray + i)->dwDuration = sInfo->indx->aIndex[i]->duration;
            }
            pReader->odmlIdxReader.pFileBuffer = (cdx_uint8 *)malloc(INDEX_CHUNK_BUFFER_SIZE);
            if(NULL == pReader->odmlIdxReader.pFileBuffer)
            {
                CDX_LOGE("malloc fail.");
                ret = AVI_ERR_REQMEM_FAIL;
                goto _error2;
            }
            ret = AVI_SUCCESS;
        }
        else
        {
            pReader->indxTblEntryArray = NULL;
            pReader->indxTblEntryIdx = -1;
            ret = AVI_ERR_NO_INDEX_TABLE;
        }
    }
    else if(CDX_AVI_INDEX_OF_CHUNKS == sInfo->indx->bIndexType)
    { //we build super index for ourselves.
        CDX_LOGW("super index type is CDX_AVI_INDEX_OF_CHUNKS, we don't support now!\n");
        return AVI_ERR_FAIL;
        #if 0
        avistdindex_chunk   *pindx_chunk = (avistdindex_chunk*)sinfo->indx;
        AVI_CHUNK   chunk;
        preader->bIndexSubType = pindx_chunk->bIndexSubType;
        preader->dwChunkId = pindx_chunk->dwChunkId;//byte low->high, "00db"
        preader->indxtbl_entry_cnt = 1;
        preader->indxtbl_entry_idx = -1;
        preader->indxtbl_entry_array = malloc(preader->indxtbl_entry_cnt
                * sizeof(struct odml_indxtbl_entry));
        if(NULL==preader->indxtbl_entry_array)
        {
            ret = AVI_ERR_REQMEM_FAIL;
            goto _error1;
        }
        memset(preader->indxtbl_entry_array, 0,
                preader->indxtbl_entry_cnt*sizeof(struct odml_indxtbl_entry));
        preader->indxtbl_entry_array->qwOffset = sinfo->indxtbl_offset;

        //try to get chunk length;
        cdx_seek(indx_fp, sinfo->indxtbl_offset, SEEK_SET);
        if (cdx_tell(indx_fp) % 2)
        {
            cdx_seek(indx_fp, 1, SEEK_CUR);
        }
        if(cdx_read(&chunk.fcc, 4, 1, indx_fp) != 1)
        {
            ret = AVI_ERR_READ_FILE_FAIL;
            goto _error2;
        }
        if(cdx_read(&chunk.length, 4, 1, indx_fp) != 1)
        {
            ret = AVI_ERR_READ_FILE_FAIL;
            goto _error2;
        }

        preader->indxtbl_entry_array->dwSize = chunk.length + 8;
        preader->indxtbl_entry_array->dwDuration = 0;
        preader->odml_ix_reader.filebuffer
            = (CDX_U8*)PHY_MALLOC(INDEX_CHUNK_BUFFER_SIZE, 1024);
        if(NULL==preader->odml_ix_reader.filebuffer)
        {
            LOGV("malloc fail\n");
            ret = AVI_ERR_REQMEM_FAIL;
            goto _error2;
        }
        preader->fp_pos = sinfo->indxtbl_offset;
        //first default point to super index chunk's header.
        preader->idx_fp = indx_fp;
        preader->read_end_flg = 0;
        ret = AVI_SUCCESS;
        #endif
    }
    else
    {
        CDX_LOGW("we don't know how to process ODML case: CDX_AVI_INDEX_IS_DATA.");
        return AVI_ERR_FAIL;
    }
    return ret;
_error2:
    free(pReader->indxTblEntryArray);
    pReader->indxTblEntryArray = NULL;
_error1:
    return ret;
}

void DeinitialPsrIndxTableReader(ODML_SUPERINDEX_READER *pReader)
{
    if(pReader->odmlIdxReader.pFileBuffer)
    {
        free(pReader->odmlIdxReader.pFileBuffer);
        pReader->odmlIdxReader.pFileBuffer = NULL;
    }
    if(pReader->indxTblEntryArray)
    {
        free(pReader->indxTblEntryArray);
        pReader->indxTblEntryArray = NULL;
    }
    memset(pReader, 0, sizeof(ODML_SUPERINDEX_READER));
}

