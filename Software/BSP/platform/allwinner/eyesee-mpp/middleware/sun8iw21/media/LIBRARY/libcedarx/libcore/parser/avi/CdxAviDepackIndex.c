/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviDepackIndex.c
 * Description : Part of avi parser.
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxAviDepackIndex"
#include <CdxAviInclude.h>
#include <string.h>

/*******************************************************************************
Function name: AviGetIndexByNumReadMode1
//1.建FFRRKeyframeTable用
//2.状态转换SetProcMode()用,分别为FFRR->PLAY和PLAY->FFRR
Description:
    1. 状态转换PLAY->FFRR， SetProcMode()->...->avi_get_index_by_num_readmode1()

    2.  从当前播放的帧的序号，在FFRRKeyframe表中找到一个接近的keyframe entry,
        其实主要配置两项:aviIn->frame_index 和 aviIn->index_in_keyfrm_tbl,
        而avi_in->frame_index其实也不重要，因为在AVI_read()时,会根据
        avi_in->index_in_keyfrm_tbl确定的entry重新赋值
        其实*position不重要,到时候会从ffrrkeyframetable中读出来,
        *diff记录的是当前找到的keyframeentry后紧跟着的audio chunk 的PTS，
        其实也不重要,因为也会从ffrrkeyframetable读出来
    3. 在两项中选一个小的项做为找到的项
Parameters:
   1: mode: 0:当前找到的合适的keyframe entry
            1:找到当前keyframe entry时，该keyframe entry的下一个entry.
   2: *off_set:找到的keyframe entry在ffrr keyframe table中的序号
   3. *diff:在Keyframe table中找到的那一个entry的正在播放的音轨的pts
   4. *frame_num: 当前的帧序号(相对于总帧数)，函数运行时会改.改成找到的entry的帧序号.
Return:
Time: 2010/9/5
*******************************************************************************/
cdx_int32 AviGetIndexByNumReadMode1(CdxAviParserImplT *p, cdx_int32 mode,
                cdx_uint32 *frameNum, cdx_uint32 *offSet, cdx_uint64 *position, cdx_int32 *diff)
{
    AviFileInT *aviIn = (AviFileInT *)p->privData;
    IdxTableItemT  *pIdxItem;
    cdx_uint32   indexNum0;
    cdx_uint32   indexNum1;
    cdx_uint32   pos;
    cdx_uint32   tmpLow, tmpHigh, tmpOffset;
    cdx_int32    audioPts = 0;

    if(!aviIn)
    {
        CDX_LOGE("aviIn NULL.");
        return AVI_ERR_PARA_ERR;
    }
    if(!aviIn->idx1Buf || !aviIn->indexCountInKeyfrmTbl)
    {
        CDX_LOGE("idx1Buf OR indexCountInKeyfrmTbl error.");
        return AVI_ERR_PARA_ERR;
    }
    if(frameNum == NULL)
    {
        CDX_LOGE("frameNum NULL.");
        return AVI_ERR_PARA_ERR;
    }
    pIdxItem = (IdxTableItemT *)aviIn->idx1Buf;
    tmpLow = 0;
    tmpHigh = aviIn->indexCountInKeyfrmTbl - 1;
    indexNum0 = indexNum1 = pIdxItem->frameIdx;
    while(1)
    {
        tmpOffset = (tmpLow + tmpHigh) / 2;

        if(tmpLow >= tmpHigh)
        {
            break;
        }

        pIdxItem = (IdxTableItemT *)aviIn->idx1Buf + tmpOffset;
        indexNum0 = pIdxItem->frameIdx;
        indexNum1 = (pIdxItem + 1)->frameIdx;

        if(*frameNum < indexNum0)
        {
            tmpHigh = tmpOffset - 1;
        }
        else if(*frameNum > indexNum1)
        {
            tmpLow = tmpOffset + 1;
        }
        else
        {
            break;
        }
    }
    *offSet = tmpOffset;

    if(mode)    //选大一点的entry
    {
        if(aviIn->indexCountInKeyfrmTbl > 1)
        {
            *offSet += 1;
        }
        *frameNum = indexNum1;
    }
    else
    {
        *frameNum = indexNum0;
    }

    pos = pIdxItem->vidChunkOffset;

    if(p->hasAudio && diff != NULL)
    {
        audioPts = pIdxItem->audPtsArray[p->curAudStreamNum];
        *diff = audioPts;
    }

    if(position)
    {
        *position = pos;
    }

    return AVI_SUCCESS;
}

void CopyIdxTableItem(IdxTableItemT *pDes, IdxTableItemT *pSrc)
{
    memcpy(pDes, pSrc, sizeof(IdxTableItemT));
}

/*
************************************************************************************************
*                       GET INDEX BY TIME
*
*Description: This function looks up index table to get the key frame's position which
*             is most closed to the "timeMs".
*               状态转换FFRR->CDX_MEDIA_STATUS_PLAY
             寻找规则是:向前找，序号必须比算出的目的帧要大;向后找,序号必须比算出的目的帧小；
             (等于都不行)然后再看last_or_next的标记决定是否退到与寻找方向相反的上一个entry对应的帧
*Arguments  : aviIn        global AVI file information
*             timeMs       the time to be searched in unit of ms
*             keyfrmIdx     current key frame index in index table;在FFRR过程中，
                            是一直查FFRRKeyframeTable的,所以记录了当前找到了哪个entry，在进入
                            本函数时，就是将要读取的那一个entry的在KeyframeTable中的序号
*             direction     search direction
*                           > 0, search forward,
*                           <=0, search backward
*             lastOrNext  the mode of get index
*                           =0, goto next key frame
*                           =1, goto last key frame
*             file_pst      the key frame's position in the file
*             keyfrmnum     the key frame number
*             paud_pts      the audio chunk's pts, audio chunk follows the video chunk in time.
*             pdes_item     : it's a pointer which point to FFRR_index_table_item. its space
                              is malloc out of function. 填写找到的那个entry的信息

*Return     : result of get index
*               = 0     get index successed;
*               < 0     get index failed.
************************************************************************************************
*/
cdx_int16 AviGetIndexByMsReadMode1(CdxAviParserImplT *p, cdx_uint32 timeMs,
                cdx_uint32 *keyfrmIdx, cdx_int32 direction, cdx_int32 lastOrNext,
                IdxTableItemT *pDesItem)
{
    AviFileInT      *aviIn = (AviFileInT *)p->privData;
    cdx_int32       i;
    cdx_uint32      targetNum, indexNum;//相对于总帧数
    IdxTableItemT   *pIdxItem = NULL;

    if(!aviIn)
    {
        return AVI_ERR_PARA_ERR;
    }

    if(!aviIn->idx1Buf)
    {
        return AVI_ERR_PARA_ERR;
    }

    if(*keyfrmIdx >= (cdx_uint32)aviIn->indexCountInKeyfrmTbl)
    {
        if(aviIn->indexCountInKeyfrmTbl > 0)
        {
            // get the last key frame in the index table
            *keyfrmIdx = aviIn->indexCountInKeyfrmTbl - 1;
        }
        else
        {
            *keyfrmIdx = 0;
        }
    }

    //calculate the target frame number by time and frame scale
    targetNum = (cdx_uint32)((cdx_int64)timeMs * 1000 / p->aviFormat.nMicSecPerFrame);

    //set search index table pointer
    pIdxItem = (IdxTableItemT *)aviIn->idx1Buf + *keyfrmIdx;
    if(direction > 0)
    {
        //forward search
        for(i=*keyfrmIdx; i<aviIn->indexCountInKeyfrmTbl; i++)
        {
            //indexNum = *buf;
            indexNum = pIdxItem->frameIdx;
            if(indexNum > targetNum)
            {
                if(i && lastOrNext)
                {
                    // point to previous key frame
                    //buf -= 3;
                    pIdxItem--;
                    i--;
                    //indexNum = *buf;
                    indexNum = pIdxItem->frameIdx;
                }
                CopyIdxTableItem(pDesItem, pIdxItem);

                *keyfrmIdx = i;

                return AVI_SUCCESS;
            }
            pIdxItem++;
        }

        //Not find the key frame, set to last key frame
        if(aviIn->indexCountInKeyfrmTbl>0)
        {
            pIdxItem--;
            CopyIdxTableItem(pDesItem, pIdxItem);
            *keyfrmIdx = i-1;   //等价于avi_in->indexCountInKeyfrmTbl - 1
            return AVI_SUCCESS;
        }
        else
        {
            CDX_LOGV("fatal error! impossible aviIn->indexCountInKeyfrmTbl[%d] == 0\n",
                aviIn->indexCountInKeyfrmTbl);
            return AVI_ERR_PARA_ERR;
        }
    }
    else
    {
        //backward search
        for(i=*keyfrmIdx; i>=0; i--)
        {
            //indexNum = *buf;
            indexNum = pIdxItem->frameIdx;
            if(indexNum < targetNum)
            {
                if(i<(aviIn->indexCountInKeyfrmTbl-1) && lastOrNext)
                {
                    // point to next key frame
                    pIdxItem++;
                    i++;
                    //indexNum = *buf;
                    indexNum = pIdxItem->frameIdx;
                }
                CopyIdxTableItem(pDesItem, pIdxItem);

                *keyfrmIdx = i;

                return AVI_SUCCESS;
            }
            //buf -= 3;
            pIdxItem--;
        }
        //don't find the key frame, set to first key frame
        if(aviIn->indexCountInKeyfrmTbl>0)
        {
            pIdxItem++;
            CopyIdxTableItem(pDesItem, pIdxItem);
            *keyfrmIdx = 0;
            return AVI_SUCCESS;
        }
        else
        {
            CDX_LOGV("fatal error! impossible aviIn->indexCountInKeyfrmTbl[%d] == 0\n",
                aviIn->indexCountInKeyfrmTbl);
            return AVI_ERR_FAIL;
        }
    }
}

/*******************************************************************************
Function name: FindIndxChunkIdx
Description:
    1. by index entry's offset, find its index chunk which contain him.
        return this index chunk's idx in superindx_reader. 一个stream的indx chunk
        有很多，要根据当前的frame的绝对地址offset，算出它在哪个indx chunk中包含。返回那个
        indx chunk的序号
Parameters:

Return:
    indexChunkIdx;
    -1:means not find.
Time: 2009/6/4
*******************************************************************************/
cdx_int32 FindIndxChunkIdx(ODML_SUPERINDEX_READER *pSuperReader, cdx_int64 entryOffset)
{
    cdx_int32   i;
    cdx_int32   minIdx = -1;
    cdx_int64   minValue = -1;
    cdx_int64   value = 0;

    if(NULL == pSuperReader || NULL == pSuperReader->indxTblEntryArray)
    {
        CDX_LOGE("imposible case, check code.");
        return AVI_ERR_PARA_ERR;
    }

    for(i = 0; i < pSuperReader->indxTblEntryCnt; i++)
    {
        value = entryOffset - pSuperReader->indxTblEntryArray[i].qwOffset;
        if(value >= 0)
        {
            if(minValue < 0)
            {
                minIdx = i;
                minValue = value;
            }
            else if(value < minValue)
            {
                minIdx = i;
                minValue = value;
            }
            else if(value == minValue)
            {
                CDX_LOGV("avi_file has wrong, check indx table! [%d],total[%d]\n", i,
                    pSuperReader->indxTblEntryCnt);
            }
        }
        else
        {
            break;
        }
    }
    return minIdx;
}

/*******************************************************************************
Function name: __reconfig_avi_read_context_readmode1
Description:
    1. for readmode1(read_chunk_by_index mode).
    2. restore avi_read context when status change from FFRR->CDX_MEDIA_STATUS_PLAY.
    3. 得到keyframeTable中的一项后，配置table reader和avi_in的变量
        aviIn->frame_index
        aviIn->uAudioPtsArray[]
        aviIn->nAudChunkCounterArray[]
        aviIn->nAudChunkTotalSizeArray[]
Parameters:

Return:
    AVI_SUCCESS,
    AVI_ERR_FAIL,
    AVI_EXCEPTION
    AVI_ERR_FILE_DATA_WRONG
Time: 2009/5/31
*******************************************************************************/
cdx_int32 ReconfigAviReadContextReadMode1(CdxAviParserImplT *p,
                    cdx_uint32 vidTime, cdx_int32 searchDirection, cdx_int32 searchMode)
{
    cdx_int32   i;
    cdx_int32   ret = AVI_EXCEPTION;
    cdx_int32   indexChunkIdx = -1;
    cdx_uint32  tmpVideoTime = vidTime;
    cdx_int64   tmpValue;
    AviFileInT  *aviIn = (AviFileInT *)p->privData;
    struct PsrIdx1TableReader    *pVreader = NULL;
    struct PsrIdx1TableReader    *pAreader = NULL;
    ODML_SUPERINDEX_READER          *pOdmlVidReader = NULL;
    ODML_SUPERINDEX_READER          *pOdmlAudReader = NULL;
    ODML_INDEX_READER               *pIndxReader = NULL;
    IdxTableItemT idxItem;

    memset(&idxItem, 0, sizeof(IdxTableItemT));
#if 0
    if(p->status == CDX_MEDIA_STATUS_FORWARD)   //FF->CDX_MEDIA_STATUS_PLAY
    {
        if(!(aviIn->index_in_keyfrm_tbl < aviIn->indexCountInKeyfrmTbl))
        {
            //reach the end of the i-index list
            aviIn->index_in_keyfrm_tbl = aviIn->indexCountInKeyfrmTbl - 1;
        }

        //一般情况下，这时的avi_in->index_in_keyfrm_tbl指向的是将要读取的entry
        if(avi_get_index_by_ms_readmode1(p, tmpVideoTime,
                (cdx_uint32 *)&aviIn->index_in_keyfrm_tbl,
                -1, 1, &idxItem) < AVI_SUCCESS)
        {
            return AVI_ERR_FAIL;
        }
    }
    else if(p->status == CDX_MEDIA_STATUS_BACKWARD) //RR->CDX_MEDIA_STATUS_PLAY
    {
        if((aviIn->index_in_keyfrm_tbl < 0))
        {
            //reach the end of the i-index list
            aviIn->index_in_keyfrm_tbl = 0;
        }

        if(avi_get_index_by_ms_readmode1(p, tmpVideoTime,
                (cdx_uint32 *)&aviIn->index_in_keyfrm_tbl,
                1, 0, &idxItem) < AVI_SUCCESS)
        {
            return AVI_ERR_FAIL;
        }
    }
    else if((p->status == CDX_MEDIA_STATUS_IDLE)
        || (p->status == CDX_MEDIA_STATUS_STOP)
        || (p->status == CDX_MEDIA_STATUS_PLAY)) //播放过程中的跳播，默认在PLAY状态下
                                    //__reconfig_avi_read_context_readmode1()，只能是跳播的原因
#endif
    //if(p->status == PSR_OK)
    { //jump play.
        //aviIn->index_in_keyfrm_tbl是关键帧表的下标号，
        //因为avi_in->index_in_keyfrm_tbl是在快进快退->PLAY或FFRR找下一个entry时为减少
        //查找时间而设的，快进快退从index_in_keyfrm_tbl开始向某方向查找
        //所以在跳播时无用，必须设成最前或最后的一帧。否则查找出错。
        if(searchDirection > 0)
        {
            aviIn->indexInKeyfrmTbl = 0;
        }
        else
        {
            aviIn->indexInKeyfrmTbl = aviIn->indexCountInKeyfrmTbl > 0
                                    ? aviIn->indexCountInKeyfrmTbl - 1 : 0;
        }
        if(AviGetIndexByMsReadMode1(p, tmpVideoTime,
                (cdx_uint32 *)&aviIn->indexInKeyfrmTbl,
                searchDirection, searchMode, &idxItem) < AVI_SUCCESS)
        {
            return AVI_ERR_FAIL;
        }
    }

    //用获取的keyframeEntry的信息重新配置table_reader和avi_in的相关变量,.?
    if(USE_IDX1 == aviIn->idxStyle)   //idx1不马上装载idx1表
    {
        pVreader = &aviIn->vidIdx1Reader;
        pAreader = &aviIn->audIdx1Reader;

        //pvreader->fpPos = file_pst.vid_chunk_idx_offset;
        pVreader->fpPos = idxItem.vidChunkIndexOffset;
        pVreader->bufIdxItem = 0;
        pVreader->leftIdxItem = aviIn->idx1Total -
            (pVreader->fpPos - aviIn->idx1Start)/sizeof(AviIndexEntryT);
        pVreader->readEndFlg = 0;

        pVreader->chunkCounter = 0;
        pVreader->chunkSize = 0;
        pVreader->chunkIndexOffset = 0;
        pVreader->totalChunkSize = 0;

        if(p->hasAudio)
        {
            //pareader->fpPos = file_pst.aud_chunk_idx_offset;
            pAreader->fpPos = idxItem.audChunkIndexOffsetArray[p->curAudStreamNum];
            pAreader->bufIdxItem = 0;
            pAreader->leftIdxItem = aviIn->idx1Total - (pAreader->fpPos -
                aviIn->idx1Start)/sizeof(AviIndexEntryT);
            pAreader->readEndFlg = 0;
            pAreader->chunkCounter = 0;
            pAreader->chunkSize = 0;
            pAreader->chunkIndexOffset = 0;
            pAreader->totalChunkSize = 0;
        }

        aviIn->frameIndex = idxItem.frameIdx;
        for(i=0; i<p->hasAudio; i++)
        {
            aviIn->uBaseAudioPtsArray[i] = idxItem.audPtsArray[i];
            //tmpIdxBuf[aviIn->index_in_keyfrm_tbl * 3 + 2];
            aviIn->nAudChunkCounterArray[i] = 0;
            aviIn->nAudFrameCounterArray[i] = 0;
            aviIn->nAudChunkTotalSizeArray[i] = 0;
        }

        ret = AVI_SUCCESS;
    }
    else if(USE_INDX == aviIn->idxStyle)
    {
        //LOGV("INDX not support\n");
        pOdmlVidReader = &aviIn->vidIndxReader;
        pOdmlAudReader = &aviIn->audIndxReader;
        //1.restore video super reader context, before read a new chunk!
        indexChunkIdx = FindIndxChunkIdx(pOdmlVidReader, idxItem.vidChunkIndexOffset);
        if(indexChunkIdx < 0)
        {
            CDX_LOGW("video idx entry's offset wrong! check code!");
            return AVI_EXCEPTION;
        }
        pOdmlVidReader->indxTblEntryIdx = indexChunkIdx;
        ret = LoadIndxChunk(pOdmlVidReader, indexChunkIdx);
        if(ret < AVI_SUCCESS)
        {
            return ret;
        }
        pIndxReader = &pOdmlVidReader->odmlIdxReader;
        tmpValue = idxItem.vidChunkIndexOffset - pOdmlVidReader->fpPos;
        if(tmpValue%(pIndxReader->wLongsPerEntry*4) != 0)
        {
            CDX_LOGV("fatal error! indx offset wrong!\n");
            return AVI_ERR_FILE_DATA_WRONG;
        }
        pIndxReader->bufIdxItem = 0;
        pIndxReader->leftIdxItem = pIndxReader->nEntriesInUse -
            tmpValue/(pIndxReader->wLongsPerEntry*4);
        if(pIndxReader->leftIdxItem < 0)
        {
            CDX_LOGE("fatal error! leftIdxItem<0!");
            return AVI_ERR_FILE_DATA_WRONG;
        }
        pIndxReader->ckReadEndFlag = 0;
        pIndxReader->pIdxEntry = NULL;
        pOdmlVidReader->fpPos = idxItem.vidChunkIndexOffset;

        pOdmlVidReader->readEndFlg = 0;

        pOdmlVidReader->chunkCounter = 0;    //新方式下，计数器从配置点开始算
        pOdmlVidReader->chunkIxTblEntOffset = idxItem.vidChunkIndexOffset;
        pOdmlVidReader->chunkSize = -1;
        pOdmlVidReader->totalChunkSize = 0;

        //2.restore audio super reader context
        if(p->hasAudio)
        {
            indexChunkIdx = FindIndxChunkIdx(pOdmlAudReader,
                idxItem.audChunkIndexOffsetArray[p->curAudStreamNum]);
            if(indexChunkIdx < 0)
            {
                CDX_LOGW("audio idx entry's offset wrong! check code!");
                return AVI_EXCEPTION;
            }
            pOdmlAudReader->indxTblEntryIdx = indexChunkIdx;
            ret = LoadIndxChunk(pOdmlAudReader, indexChunkIdx);
            if(ret < AVI_SUCCESS)
            {
                return ret;
            }
            pIndxReader = &pOdmlAudReader->odmlIdxReader;
            tmpValue = idxItem.audChunkIndexOffsetArray[p->curAudStreamNum] -
                pOdmlAudReader->fpPos;
            if(tmpValue%(pIndxReader->wLongsPerEntry*4) != 0)
            {
                CDX_LOGV("fatal error! indx offset wrong!");
                return AVI_ERR_FILE_DATA_WRONG;
            }
            pIndxReader->bufIdxItem = 0;
            pIndxReader->leftIdxItem = pIndxReader->nEntriesInUse -
                tmpValue/(pIndxReader->wLongsPerEntry*4);
            if(pIndxReader->leftIdxItem < 0)
            {
                CDX_LOGV("fatal error! leftIdxItem<0!\n");
                return AVI_ERR_FILE_DATA_WRONG;
            }
            pIndxReader->ckReadEndFlag = 0;
            pIndxReader->pIdxEntry = NULL;

            pOdmlAudReader->fpPos = idxItem.audChunkIndexOffsetArray[p->curAudStreamNum];
            pOdmlAudReader->readEndFlg = 0;

            //pOdmlAudReader->chunk_idx = idxItem.aud_chunk_idx;
            pOdmlAudReader->chunkCounter = 0;    //新方式下，计数器从配置点开始算
            pOdmlAudReader->chunkIxTblEntOffset =
                idxItem.audChunkIndexOffsetArray[p->curAudStreamNum];
            pOdmlAudReader->chunkSize = -1;
            //pOdmlAudReader->total_chunk_size = idxItem.aud_total_chunk_size;
            pOdmlAudReader->totalChunkSize = 0; //新方式下，计数器从配置点开始算
        }
        //3.restore aviIn context,
        aviIn->frameIndex = idxItem.frameIdx;
        for(i=0; i<p->hasAudio; i++)
        {
            aviIn->uBaseAudioPtsArray[i] = idxItem.audPtsArray[i];
            //tmpIdxBuf[aviIn->index_in_keyfrm_tbl * 3 + 2];
            aviIn->nAudChunkCounterArray[i] = 0;
            aviIn->nAudFrameCounterArray[i] = 0;
            aviIn->nAudChunkTotalSizeArray[i] = 0;
        }
        ret = AVI_SUCCESS;
    }
    else
    {
        CDX_LOGV("impossible case!");
        ret = AVI_EXCEPTION;
    }
    return ret;
}

/*******************************************************************************
Function name: config_avi_read_context_after_change_audio_stream_index_mode
Description:
    1.基于index读取方式下,在换音轨后,要为新音轨重新配置audio index reader.
        包括stream id等，都要重配
    2.首先要根据当前的帧序号(aviIn->frame_index)找到附近的较小一点的
        FFRRKeyframeTable中的关键帧的entry,
        然后根据entry中的audio部分的信息去重配audio index reader, stream_id一定要改掉!
        还要配置avi_in->中的audio统计的相关变量
    3.GetNextChunkInfo()->config_avi_reader_context_after_change_audio_stream()
Parameters:

Return:

Time: 2010/9/6
*******************************************************************************/
cdx_int32 ConfigNewAudioAviReadContextIndexMode(CdxAviParserImplT *p)
{
    cdx_int32   ret;
    cdx_int32   i;
    cdx_int32   indexChunkIdx;
    AviFileInT  *aviIn = (AviFileInT *)p->privData;
    IdxTableItemT *pEntry = NULL;
    struct PsrIdx1TableReader *paReader = NULL;
    ODML_SUPERINDEX_READER    *pOdmlAudReader = NULL;
    ODML_INDEX_READER         *pIndxReader = NULL;
    cdx_int32 nCurFrameIdx = aviIn->frameIndex;
    cdx_uint64 videoChunkOffset = 0;
    cdx_int32  audioDiff;   //现在是audio_pts
    cdx_int64  tmpValue;

    //找较小的keyframeTable的entry
    ret = AviGetIndexByNumReadMode1(p, 0, (cdx_uint32 *)&nCurFrameIdx,
                (cdx_uint32*)&aviIn->indexInKeyfrmTbl, &videoChunkOffset, &audioDiff);
    if(ret != AVI_SUCCESS)
    {
        CDX_LOGE("call AviGetIndexByNumReadMode1() fail,ret[%d]\n", ret);
        return ret;
    }
    pEntry = (IdxTableItemT *)aviIn->idx1Buf + aviIn->indexInKeyfrmTbl;

    //重新配置audio index reader
    if(USE_IDX1 == aviIn->idxStyle)   //idx1不马上装载idx1表
    {
        //不重新初始化，因为只有一张idx1总表,没必要重新初始化，修改strean_idx即可
        if(p->hasAudio)
        {
            paReader = &aviIn->audIdx1Reader;
            paReader->streamIdx = p->audioStreamIndexArray[p->curAudStreamNum];

            paReader->fpPos = pEntry->audChunkIndexOffsetArray[p->curAudStreamNum];
            paReader->bufIdxItem = 0;
            paReader->leftIdxItem = aviIn->idx1Total - (paReader->fpPos -
                aviIn->idx1Start)/sizeof(AviIndexEntryT);
            paReader->readEndFlg = 0;
            paReader->chunkCounter = 0;
            paReader->chunkSize = 0;
            paReader->chunkIndexOffset = 0;
            paReader->totalChunkSize = 0;
        }
        else
        {
            CDX_LOGV("change audio stream, fatal error.");
        }
        //avi_in的音频相关统计变量的配置
        for(i = 0; i < p->hasAudio; i++)
        {
            aviIn->uBaseAudioPtsArray[i] = pEntry->audPtsArray[i];
            //tmpIdxBuf[aviIn->index_in_keyfrm_tbl * 3 + 2];
            aviIn->nAudChunkCounterArray[i] = 0;
            aviIn->nAudFrameCounterArray[i] = 0;
            aviIn->nAudChunkTotalSizeArray[i] = 0;
        }
        ret = AVI_SUCCESS;
    }
    else if(USE_INDX == aviIn->idxStyle)
    {
        //1.restore audio super reader context
        if(p->hasAudio)
        {
            DeinitialPsrIndxTableReader(&aviIn->audIndxReader);
            ret = InitialPsrIndxTableReader(&aviIn->audIndxReader, aviIn,
                p->audioStreamIndexArray[p->curAudStreamNum]);
            if(ret < AVI_SUCCESS)
            {
                CDX_LOGV("change audio, initial odml fail.");
                return ret;
            }
            pOdmlAudReader = &aviIn->audIndxReader;
            indexChunkIdx = FindIndxChunkIdx(pOdmlAudReader,
                pEntry->audChunkIndexOffsetArray[p->curAudStreamNum]);
            if(indexChunkIdx < 0)
            {
                CDX_LOGE("audio idx entry's offset wrong! check code!\n");
                return AVI_EXCEPTION;
            }
            pOdmlAudReader->indxTblEntryIdx = indexChunkIdx;
            ret = LoadIndxChunk(pOdmlAudReader, indexChunkIdx);
            if(ret < AVI_SUCCESS)
            {
                return ret;
            }
            pIndxReader = &pOdmlAudReader->odmlIdxReader;
            tmpValue = pEntry->audChunkIndexOffsetArray[p->curAudStreamNum] -
                pOdmlAudReader->fpPos;
            if(tmpValue%(pIndxReader->wLongsPerEntry * 4) != 0)
            {
                CDX_LOGE("fatal error! indx offset wrong!\n");
                return AVI_ERR_FILE_DATA_WRONG;
            }
            pIndxReader->bufIdxItem = 0;
            pIndxReader->leftIdxItem = pIndxReader->nEntriesInUse -
                tmpValue/(pIndxReader->wLongsPerEntry*4);
            if(pIndxReader->leftIdxItem < 0)
            {
                CDX_LOGE("fatal error! leftIdxItem<0!");
                return AVI_ERR_FILE_DATA_WRONG;
            }
            pIndxReader->ckReadEndFlag = 0;
            pIndxReader->pIdxEntry = NULL;

            pOdmlAudReader->fpPos = pEntry->audChunkIndexOffsetArray[p->curAudStreamNum];
            pOdmlAudReader->readEndFlg = 0;

            //pOdmlAudReader->chunk_idx = idxItem.aud_chunk_idx;
            pOdmlAudReader->chunkCounter = 0;    //新方式下，计数器从配置点开始算
            //pOdmlAudReader->chunk_ixtbl_ent_offset =
            //pEntry->aud_chunk_index_offset_array[p->CurAudStreamNum];
            pOdmlAudReader->chunkIxTblEntOffset = 0;
            pOdmlAudReader->chunkSize = -1;
            //pOdmlAudReader->total_chunk_size = idxItem.aud_total_chunk_size;
            pOdmlAudReader->totalChunkSize = 0; //新方式下，计数器从配置点开始算
        }
        else
        {
            CDX_LOGV("change audio stream, odml, fatal error.");
        }
        //3.restore aviIn context,
        for(i = 0; i < p->hasAudio; i++)
        {
            aviIn->uBaseAudioPtsArray[i] = pEntry->audPtsArray[i];
            //tmpIdxBuf[aviIn->index_in_keyfrm_tbl * 3 + 2];
            aviIn->nAudChunkCounterArray[i] = 0;
            aviIn->nAudFrameCounterArray[i] = 0;
            aviIn->nAudChunkTotalSizeArray[i] = 0;
        }
        ret = AVI_SUCCESS;
    }
    else
    {
        CDX_LOGV("impossible case!");
        ret = AVI_EXCEPTION;
    }
    return ret;
}

//==============================================================================
//3.读数据.PLAY下读和FFRR下读

/*******************************************************************************
Function name: check_psr_idx1_reader
Description:
    check if psr_inder_reader has read to end.
Parameters:

Return:
    AVI_SUCCESS: means reader still work, not read to end.
    AVI_ERR_SEARCH_INDEX_CHUNK_END; read to index end.
Time: 2009/5/25
*******************************************************************************/
cdx_int32 CheckPsrIdx1Reader(struct PsrIdx1TableReader *pReader)
{
    //1. check if reader is effect. because audio or video may not exist in file.
    if(NULL == pReader->idxFp || NULL == pReader->fileBuffer)
    {
        return AVI_ERR_SEARCH_INDEX_CHUNK_END;
    }
    //2. check reader's read_end_flg
    if(pReader->readEndFlg)
    {
        return AVI_ERR_SEARCH_INDEX_CHUNK_END;
    }
    else
    {
        return AVI_SUCCESS;
    }
}

cdx_int32 CheckOdmlSuperIndexReader(ODML_SUPERINDEX_READER *pReader)
{
    //1. check if reader is effect. because audio or video may not exist in file.
    if(NULL == pReader->idxFp || NULL == pReader->indxTblEntryArray)
    {
        return AVI_ERR_SEARCH_INDEX_CHUNK_END;
    }
    //2. check reader's read_end_flg
    if(pReader->readEndFlg)
    {
        return AVI_ERR_SEARCH_INDEX_CHUNK_END;
    }
    else
    {
        return AVI_SUCCESS;
    }
}
/*******************************************************************************
Function name: decide_read_aud_vid_chunk
Description:
    decide to read which chunk, audio or video?, for readmode1.
Parameters:
    vid_idx_flg: if read end vid index table.
Return:
    AVI_ERR_SEARCH_INDEX_CHUNK_END(FILE_PARSER_END_OF_MOVI);
    CHUNK_TYPE_AUDIO;
    CHUNK_TYPE_VIDEO;
Time: 2009/5/25
*******************************************************************************/
cdx_int32 DecideReadAvChunk(CdxAviParserImplT *p, cdx_int32 vidIdxFlg, cdx_int32 audIdxFlg)
{
    cdx_int32   ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
    cdx_int64   curVideoPts = 0;  //the PTS of the video chunk which is now try to get.unit: ms.
    cdx_int64   curAudioPts = 0;
    cdx_uint32  audioDuration = 0;
    AviFileInT  *aviIn = (AviFileInT *)p->privData;

    if( AVI_ERR_SEARCH_INDEX_CHUNK_END == vidIdxFlg
                && AVI_ERR_SEARCH_INDEX_CHUNK_END == audIdxFlg)
    {
        ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
        return ret;
    }
    if(p->hasVideo && p->hasAudio)
    {
        if(AVI_SUCCESS == vidIdxFlg && AVI_SUCCESS == audIdxFlg)
        { // audio and video don't reach end now.
            curVideoPts = (cdx_int64)aviIn->frameIndex * p->aviFormat.nMicSecPerFrame / 1000;

            audioDuration = CalcAviAudioChunkPts(&aviIn->audInfoArray[p->curAudStreamNum],
                                    aviIn->nAudChunkTotalSizeArray[p->curAudStreamNum],
                                    aviIn->nAudFrameCounterArray[p->curAudStreamNum]);
            curAudioPts = (cdx_int64)aviIn->uBaseAudioPtsArray[p->curAudStreamNum] + audioDuration;
            if(curVideoPts < curAudioPts)
            { //video chunk is less than audio. So try to get video chunk.
                ret = CHUNK_TYPE_VIDEO;
            }
            else
            {
                ret = CHUNK_TYPE_AUDIO;
            }
        }
        else if(AVI_SUCCESS == vidIdxFlg && AVI_ERR_SEARCH_INDEX_CHUNK_END == audIdxFlg)
        { //get video chunk
            ret = CHUNK_TYPE_VIDEO;
        }
        else if(AVI_ERR_SEARCH_INDEX_CHUNK_END == vidIdxFlg && AVI_SUCCESS == audIdxFlg)
        {
            ret = CHUNK_TYPE_AUDIO;
        }
        else
        {
            CDX_LOGE("impossible case.");
            ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
        }
    }
    else if(p->hasVideo && !p->hasAudio)
    {
        if(AVI_SUCCESS == vidIdxFlg)
        {
            ret = CHUNK_TYPE_VIDEO;
        }
    }
    else if(!p->hasVideo && p->hasAudio)
    {
        if(AVI_SUCCESS == audIdxFlg)
        {
            ret = CHUNK_TYPE_AUDIO;
        }
    }
    else
    {
        CDX_LOGE("impossible case!\n");
        ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
    }
    return ret;
}

/*******************************************************************************
Function name: try_to_get_next_chunk_entry
Description:
    1. after decide_read_av_chunk(), we will try to get next chunk position info
    by idx1 or indx.
    2. we don't opearte fp now.
    3. for idx1 and indx.
Parameters:
    chunk_type : enum AVI_CHUNK_TYPE & AVI_ERR_SEARCH_INDEX_CHUNK_END.

    vid_idx_flg : AVI_ERR_SEARCH_INDEX_CHUNK_END or AVI_SUCCESS
    aud_idx_flg :

    pchunk_offset : chunk's absolute offset from the file start.
Return:
    AVI_EXCEPTION;
    enum AVI_CHUNK_TYPE;
    AVI_ERR_SEARCH_INDEX_CHUNK_END
Time: 2009/5/25
*******************************************************************************/
cdx_int32 TryToGetNextChunkEntry(cdx_int32 chunkType, AviFileInT *aviIn,
                                    cdx_int32 vidIdxFlg, cdx_int32 audIdxFlg,
                                    cdx_int64 *pChunkOffset, cdx_int32 *pChunkBodySize)
{
    cdx_int64   chunkOffset = -1;
    cdx_int32   chunkBodySize = 0;
    cdx_int32   ret = AVI_EXCEPTION;
    cdx_int32   srchRet = AVI_EXCEPTION;
    AVI_CHUNK_POSITION  chunkPos;

    switch(chunkType)
    {
        case (cdx_int32)CHUNK_TYPE_VIDEO:
        {
            if(USE_IDX1 == aviIn->idxStyle)
            {
                srchRet = SearchNextIdx1IndexEntry(&aviIn->vidIdx1Reader, &chunkPos);
                chunkOffset = chunkPos.ckOffset;
                chunkBodySize = chunkPos.chunkBodySize;
            }
            else if(USE_INDX == aviIn->idxStyle)
            {
                srchRet = SearchNextODMLIndexEntry(&aviIn->vidIndxReader,&chunkPos);
                chunkOffset = chunkPos.ckOffset;
                chunkBodySize = chunkPos.chunkBodySize;
            }
            else
            {
                CDX_LOGE("idxStyle not right.");
                return AVI_EXCEPTION;
            }

            if(srchRet < AVI_SUCCESS)
            { //fatal error, return;
                return srchRet;
            }
            if(AVI_SUCCESS == srchRet)
            {
                ret = CHUNK_TYPE_VIDEO;
            }
            else
            { //if video chunk has none, we search audio chunk.
                if(AVI_SUCCESS == audIdxFlg)
                {
                    if(USE_IDX1 == aviIn->idxStyle)
                    {
                        srchRet = SearchNextIdx1IndexEntry(&aviIn->audIdx1Reader, &chunkPos);
                        chunkOffset = chunkPos.ckOffset;
                        chunkBodySize = chunkPos.chunkBodySize;
                    }
                    else if(USE_INDX == aviIn->idxStyle)
                    {
                        srchRet = SearchNextODMLIndexEntry(&aviIn->audIndxReader, &chunkPos);
                        chunkOffset = chunkPos.ckOffset;
                        chunkBodySize = chunkPos.chunkBodySize;
                    }
                    else
                    {
                        CDX_LOGE("idxStyle not right.");
                        return AVI_EXCEPTION;
                    }

                    if(srchRet < AVI_SUCCESS)
                    { //fatal error, return;
                        return srchRet;
                    }
                    if(AVI_SUCCESS == srchRet)
                    {
                        ret = CHUNK_TYPE_AUDIO;
                    }
                    else
                    {
                        ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
                    }
                }
                else
                {
                    ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
                }
            }
            break;
        }
        case (cdx_int32)CHUNK_TYPE_AUDIO:
        {
            if(USE_IDX1 == aviIn->idxStyle)
            {
                srchRet = SearchNextIdx1IndexEntry(&aviIn->audIdx1Reader, &chunkPos);
                chunkOffset = chunkPos.ckOffset;
                chunkBodySize = chunkPos.chunkBodySize;
            }
            else if(USE_INDX == aviIn->idxStyle)
            {
                srchRet = SearchNextODMLIndexEntry(&aviIn->audIndxReader, &chunkPos);
                chunkOffset = chunkPos.ckOffset;
                chunkBodySize = chunkPos.chunkBodySize;
            }
            else
            {
                CDX_LOGE("idxStyle not right.");
                return AVI_EXCEPTION;
            }

            if(srchRet < AVI_SUCCESS)
            { //fatal error, return;
                return srchRet;
            }
            if(AVI_SUCCESS == srchRet)
            {
                ret = CHUNK_TYPE_AUDIO;
            }
            else
            { //if audio chunk has none, we search video chunk.
                if(AVI_SUCCESS == vidIdxFlg)
                {
                    if(USE_IDX1 == aviIn->idxStyle)
                    {
                        srchRet = SearchNextIdx1IndexEntry(&aviIn->vidIdx1Reader, &chunkPos);
                        chunkOffset = chunkPos.ckOffset;
                        chunkBodySize = chunkPos.chunkBodySize;
                    }
                    else if(USE_INDX == aviIn->idxStyle)
                    {
                        srchRet = SearchNextODMLIndexEntry(&aviIn->vidIndxReader,&chunkPos);
                        chunkOffset = chunkPos.ckOffset;
                        chunkBodySize = chunkPos.chunkBodySize;
                    }
                    else
                    {
                        CDX_LOGE("idx_style not right.");
                        return AVI_EXCEPTION;
                    }

                    if(srchRet < AVI_SUCCESS)
                    { //fatal error, return;
                        return srchRet;
                    }
                    if(AVI_SUCCESS == srchRet)
                    {
                        ret = CHUNK_TYPE_VIDEO;
                    }
                    else
                    {
                        ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
                    }
                }
                else
                {
                    ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
                }
            }
            break;
        }
        case (cdx_int32)AVI_ERR_SEARCH_INDEX_CHUNK_END:
        {
            ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
            break;
        }
        default:
        {
            CDX_LOGV("impossible case, chunk_type=%d.", chunkType);
            ret = AVI_ERR_SEARCH_INDEX_CHUNK_END;
            break;
        }
    }

    *pChunkOffset = chunkOffset;
    *pChunkBodySize = chunkBodySize;
    switch(ret)
    {
        case (cdx_int32)CHUNK_TYPE_VIDEO:
        {
            //LOGV("V[%x]\n", (cdx_int32)chunk_offset);
            break;
        }
        case (cdx_int32)CHUNK_TYPE_AUDIO:
        {
            //LOGV("A[%x]\n", (cdx_int32)chunk_offset);
            break;
        }
        case (cdx_int32)AVI_ERR_SEARCH_INDEX_CHUNK_END:
        {
            CDX_LOGV("AV index search all end!");
            break;
        }
        default:
        {
            return AVI_EXCEPTION;
        }
    }
    return ret;
}

/*******************************************************************************
Function name: AVI_get_chunk_header
Description:
    1. select and get chunk header information.
    2. we guarantee fp is set to right position, we can read immediately.
    3. we only get audio and video chunk in this function.
    4. get chunk mode is by index!只使用于基于index表读取的方式
Parameters:
    avi_file  avi reader handle;
    ck_type : we guarantee ck_type is valid before call this function.

Return:
    FILE_PARSER_PARA_ERR
    FILE_PARSER_END_OF_MOVI

Time: 2009/5/22
*******************************************************************************/
cdx_int16 AviGetChunkHeader(AviFileInT *aviFile, enum AVI_CHUNK_TYPE ckType)
{
    cdx_uint32       length;
    cdx_uint64       pos;
    AviChunkT        *chunk = NULL;
    CdxStreamT       *fp = NULL;

    if(!aviFile)
    {
        return AVI_ERR_PARA_ERR;
    }
    switch(ckType)
    {
        case CHUNK_TYPE_VIDEO:
        {
            fp = aviFile->fp;
            break;
        }
        case CHUNK_TYPE_AUDIO:
        {
            fp = aviFile->audFp;
            break;
        }
        default:
        {
            return AVI_ERR_PARA_ERR;
        }
    }
    //check file end
    pos = CdxStreamTell(fp);
    if(pos >= aviFile->fileSize)
    { //never possible to exceed filesize.
        CDX_LOGE("Bad pos.");
        return AVI_ERR_END_OF_MOVI;
    }
    // read a data chunk
    if(GetNextChunkHead(fp, &aviFile->dataChunk, &length) < 0)
    {
        CDX_LOGE("GetNextChunkHead failed.");
        return AVI_ERR_FILE_FMT_ERR;
    }

    //verify
    chunk = &aviFile->dataChunk;
    if(chunk->fcc == CDX_LIST_HEADER_LIST || chunk->fcc == CDX_CKID_AVI_NEWINDEX)
    {
        CDX_LOGV("get chunk wrong!");
        return AVI_ERR_FILE_FMT_ERR;
    }
    //Do we need to add the chunk length checking? Sometimes the length
    //is very very big becuase of error.
    //TBD
    return AVI_SUCCESS;
}

/*******************************************************************************
Function name: AVI_read_by_index
Description:
    1. select and read a chunk header.
    2. support idx1 & indx
Parameters:

Return:

    AVI_ERR_PARA_ERR;
    FILE_PARSER_END_OF_MOVI;
    FILE_PARSER_EXCEPTION;

    AVI_SUCCESS;
    AVI_EXCEPTION
    -1;
Time: 2009/5/25
*******************************************************************************/
cdx_int16 AviReadByIndex(CdxAviParserImplT *p)
{
    AviFileInT *aviIn;
    //IdxTableItemT *pIdxItem = NULL;
    //IdxTableItemT *pLastIdxItem = NULL;
    cdx_int32       ret = AVI_SUCCESS;
    cdx_int32       chunkTypeRet = AVI_SUCCESS;
    cdx_int64       chunkOffset = -1;
    cdx_int32       vidIdxFlg = -1;
    cdx_int32       audIdxFlg = -1;
    cdx_int32       chunkBodySize = 0xffffffff;

    if(!p)
    {
        CDX_LOGE("bad param.");
        return AVI_ERR_PARA_ERR;
    }

    aviIn = (AviFileInT *)p->privData;
    if(!aviIn)
    {
        CDX_LOGE("aviIn is NULL.");
        return AVI_ERR_PARA_ERR;
    }
    //1. first, make sure to read which chunk type, then read idx1.
    //  fp, chunk offset must be got.
    switch (p->mErrno)
    {
        case PSR_OK:

        {
            chunkBodySize = 0;
            p->nFRPicCnt = 0;
            if(USE_IDX1 == aviIn->idxStyle)
            {
                vidIdxFlg = CheckPsrIdx1Reader(&aviIn->vidIdx1Reader);
                audIdxFlg = CheckPsrIdx1Reader(&aviIn->audIdx1Reader);
            }
            else if(USE_INDX == aviIn->idxStyle)
            {
                vidIdxFlg = CheckOdmlSuperIndexReader(&aviIn->vidIndxReader);
                audIdxFlg = CheckOdmlSuperIndexReader(&aviIn->audIndxReader);
            }
            else
            {
                CDX_LOGE("fatal error!");
                return AVI_EXCEPTION;
            }
            //1.1 compare current video frame's PTS and current audio frame's PTS,
            //    to decide which should be read.
            ret = DecideReadAvChunk(p, vidIdxFlg, audIdxFlg);
            //1.2 read and search idx table buffer, decide a fp_offset.
            chunkTypeRet = TryToGetNextChunkEntry(ret, aviIn,
                    vidIdxFlg, audIdxFlg, &chunkOffset, &chunkBodySize);
            break;
        }
        default:
            break;

    }

    if(chunkTypeRet < AVI_SUCCESS)
    {
        return chunkTypeRet;
    }
    switch(chunkTypeRet)
    {
        case CHUNK_TYPE_VIDEO:
        {
            if(CdxStreamSeek(aviIn->fp, chunkOffset, STREAM_SEEK_SET))
            {
                CDX_LOGV("seek error video.");
                return AVI_ERR_READ_FILE_FAIL;
            }
            break;
        }
        case CHUNK_TYPE_AUDIO:
        {
            if(CdxStreamSeek(aviIn->audFp, chunkOffset, STREAM_SEEK_SET))//TODO....
            {
                CDX_LOGV("seek error audio.");
                return AVI_ERR_READ_FILE_FAIL;
            }
            break;
        }
        case AVI_ERR_SEARCH_INDEX_CHUNK_END:
        {
            CDX_LOGV("read by index wrong , AVI_ERR_SEARCH_INDEX_CHUNK_END.");
            return AVI_ERR_END_OF_MOVI;
        }
        default:
        {
            CDX_LOGV("read by index wrong , FILE_PARSER_EXCEPTION.");
            return AVI_EXCEPTION;
        }
    }
    if(chunkBodySize)
    {
        return AviGetChunkHeader(aviIn, (enum AVI_CHUNK_TYPE)chunkTypeRet);
    }
    else
    {
        aviIn->dataChunk.length = 0;
        switch(chunkTypeRet)
        {
            case CHUNK_TYPE_VIDEO:
            {
                aviIn->dataChunk.fcc = CDX_MAKE_AVI_CKID(CDX_CKTYPE_DIB_BITS, p->videoStreamIndex);
                break;
            }
            case CHUNK_TYPE_AUDIO:
            {
                aviIn->dataChunk.fcc = CDX_MAKE_AVI_CKID(CDX_CKTYPE_WAVE_BYTES,
                    p->audioStreamIndex);
                break;
            }
            default:
            {
                CDX_LOGV("AVI_read_by_index: chunk type[%d] error\n", chunkTypeRet);
                return AVI_ERR_PARA_ERR;
            }
        }
        return AVI_SUCCESS;
    }
}

