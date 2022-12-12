/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviDepackIndex.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _CDX_AVI_DEPACK_INDEX_H_
#define _CDX_AVI_DEPACK_INDEX_H_

//原来40字节, 现在4音轨的话，100字节; 8音轨的话,180字节
//            现在           72字节,            116字节  120字节
typedef struct IdxTableItemT
{
    cdx_int32   frameIdx;
    cdx_int64   vidChunkOffset;          // for FFRR.指向chunk head
    cdx_int64   vidChunkIndexOffset;     // offset of video chunk index entry, absolue offset!
    cdx_uint32  audPtsArray[MAX_AUDIO_STREAM];
    //多音轨情况下，每条音轨的对应的时间戳(播放持续时间)，音轨数量参考p->hasAudio.
    //当前的音轨下标参考p->CurItemNumInAudioStreamIndexArray
    //因为是基于index表的读取方式,所以记录在这个帧的PTS之后的一个audio chunk所携带的PTS.单位:ms
    cdx_int64   audChunkIndexOffsetArray[MAX_AUDIO_STREAM];
    // offset of audio chunk index entry, absolute offset!
    //那个audio frame对应的index表中的entry的绝对地址
}IdxTableItemT;   //为基于index表读取方式设计的，快进快退表的entry

typedef struct {
    cdx_int64   ckOffset;       //point to chunk head!
    cdx_int64   ixTblEntOffset; //chunk对应的index表中的entry所在的位置，绝对地址, idx1的entry也适用
    cdx_int32   chunkBodySize;  //chunk body的大小，不包括chunk head
    cdx_int32   isKeyframe;     //for video chunk
} AVI_CHUNK_POSITION;           //记录的是odml indx entry所指示的chunk的信息，
                                //也可以指示idx1表的entry

extern cdx_int32 AviGetIndexByNumReadMode1(CdxAviParserImplT *p, cdx_int32 mode,
                cdx_uint32 *frameNum, cdx_uint32 *offSet, cdx_uint64 *position, cdx_int32 *diff);
extern cdx_int32 ReconfigAviReadContextReadMode1(CdxAviParserImplT *p,
                    cdx_uint32 vidTime, cdx_int32 searchDirection, cdx_int32 searchMode);
extern cdx_int32 ConfigNewAudioAviReadContextIndexMode(CdxAviParserImplT *p);
extern cdx_int16 AviReadByIndex(CdxAviParserImplT *p);

//avi_idx1.h
extern cdx_int32 SearchNextIdx1IndexEntry(struct PsrIdx1TableReader *pReader,
    AVI_CHUNK_POSITION *pChunkPos);
extern cdx_int16 AviBuildIdxForIndexMode(CdxAviParserImplT *p);
extern cdx_int32 InitialPsrIdx1TableReader(struct PsrIdx1TableReader *pReader,
                                    AviFileInT *aviIn, cdx_int32 streamIndex);
extern void DeinitialPsrIdx1TableReader(struct PsrIdx1TableReader *pReader);

//avi_odml_indx.h
extern cdx_int32 IsIndxChunkId(FOURCC fcc);
extern cdx_int32 LoadIndxChunk(ODML_SUPERINDEX_READER *pSuperReader, cdx_int32 indxTblEntryIdx);
extern cdx_int32 SearchNextODMLIndexEntry(ODML_SUPERINDEX_READER *pSuperReader,
    AVI_CHUNK_POSITION *pCkPos);
extern cdx_int16 AviBuildIdxForODMLIndexMode(CdxAviParserImplT *p);
extern cdx_int32 InitialPsrIndxTableReader(ODML_SUPERINDEX_READER *pReader,
    AviFileInT *aviIn, cdx_int32 streamIndex);
extern void DeinitialPsrIndxTableReader(ODML_SUPERINDEX_READER *pReader);

#endif  /* _CDX_AVI_DEPACK_INDEX_H_ */

