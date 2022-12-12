/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviDepackSequence.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _AVI_DEPACK_SEQUENCE_H_
#define _AVI_DEPACK_SEQUENCE_H_
//如果是8音轨，48字节; 4音轨,32字节
typedef struct OldIdxTableItemT
{
    cdx_int32  frameIdx;        //该帧在所有帧中的序号(不是在FFRR关键帧表中的序号)，
                                //该帧一般是关键帧,用于建立FFRRKeyframeTable.
    cdx_int64  vidChunkOffset;  //for FFRR.绝对地址，必须指向chunk_head!
    cdx_uint32 audPtsArray[MAX_AUDIO_STREAM];  //多音轨情况下，每条音轨的对应的时间戳(播放持续时间)
        //音轨数量参考p->hasAudio.当前的音轨下标参考p->CurItemNumInAudioStreamIndexArray
                                          //记录在这个帧之后的一个audio chunk所携带的PTS.单位:ms
}OldIdxTableItemT; //为顺序读取方式设计的，这是FFRRKeyframeTable中的一个entry的数据结构
extern cdx_int16 AviBuildIdxForOdmlSequenceMode(CdxAviParserImplT *p);
extern cdx_int16 AviBuildIdxForIdx1SequenceMode(CdxAviParserImplT *p);
extern cdx_int32 ReconfigAviReadContextReadMode0(CdxAviParserImplT *p, cdx_uint32 vidTime,
                    cdx_int32 searchDirection, cdx_int32 searchMode);
extern cdx_int32 ReconfigAviReadContextReadMode1(CdxAviParserImplT *p,
                    cdx_uint32 vidTime, cdx_int32 searchDirection, cdx_int32 searchMode);
extern cdx_int32 CalcAviChunkAudioFrameNum(AudioStreamInfoT *pAudStrmInfo, cdx_int32 nChunkSize);
//extern CDX_S32 avi_get_index_by_num_readmode0(struct FILE_PARSER *p, CDX_S32 mode,
//CDX_U32 *frame_num,CDX_U32 *off_set, CDX_U64 *position,CDX_S32 *diff);
//extern CDX_S32 __reconfig_avi_read_context_readmode0(struct FILE_PARSER *p,CDX_U32 vid_time,
//CDX_S32 search_direction, CDX_S32 search_mode);
extern cdx_int16 AviReadSequence(CdxAviParserImplT *p);

#endif  /* _AVI_DEPACK_SEQUENCE_H_ */

