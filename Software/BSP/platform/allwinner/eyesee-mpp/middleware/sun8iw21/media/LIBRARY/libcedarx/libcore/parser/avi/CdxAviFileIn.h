/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviFileIn.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _CDX_AVI_FILE_IN_H_
#define _CDX_AVI_FILE_IN_H_

/*******************************************************************************
struct name: _AVI_FILE_IN_
Note:   This struct is used to store stuff chunk from file.
*******************************************************************************/
typedef struct _AVI_FILE_IN_    //从AVI_DEPACK派生出来的派生类
{
    CdxStreamT        *fp;//used to read video chunk.如果是基于index表读取方式，就需要开3个文件指针.
    CdxStreamT        *audFp;    //used to read audio chunk
    CdxStreamT        *idxFp;    //used to read index table.
    AviChunkT         *avih;     //avi file header，需要malloc,但是长度不大。
                                 //全部复制，原始文件的内容。基本不修改内容，即使错误，
                                 //如avih->dwMicroSecPerFrame

    cdx_int32           nStream;    //记录AVI文件包含的音视频流总数。MainAviHeader的内容。
    AviStreamInfoT      *sInfo[MAX_STREAM]; //malloc出来的，通过get_next_stream_info(),
                                            //一个SInfo对应一个LIST strl的全部内容,其下标号就是
                                            //stream的index。基本上是原始文件的内容，
                                            //即使认为数据明显错误，不会修改
    AudioStreamInfoT    audInfoArray[MAX_AUDIO_STREAM]; //自行算出的音频信息，用于实际。
    SubStreamInfoT      subInfoArray[MAX_SUBTITLE_STREAM];
    cdx_int8            hasIdx1;    //idx1 index table exsit,仅表示idx1表是否存在而且完整正常，
                                    //只要有异常,has_idx1置0.
    cdx_int8            hasIndx;    //indx index table exsit (Open_DML AVI),只要是odml格式，
                                    //就一定置1，如果置0，表示是AVI1.0格式

    // readmode & idx_style decide the AVI parsing style and FFRR.
    enum READ_CHUNK_MODE    readmode;   //enum READ_CHUNK_MODE, READ_CHUNK_SEQUENCE
    enum USE_INDEX_STYLE    idxStyle;

    cdx_int32       moviStart;     //fileoffset,after "movi", 只是RIFF AVI部分的movi_start,
                                   //RIFF AVIX不考虑，所以CDX_S32足够
    cdx_int32       moviEnd;       //fileoffset,point to next byte of last_movi_byte.
                                   //只是RIFF AVI部分的movi_end， RIFF AVIX不考虑， 所以CDX_S32足够
    cdx_uint32      *idx1Buf;      //old:frame_count, offset, audio_time, (audio chunk offset),
                                   //two type key frame index table,(1)3 items,(2)ref to
                                   //IDX_TABLE_ITEM. idx1_buf是建FFRRKeyframeTable的buffer。
    cdx_uint32      *idx1BufEnd;   //point to the next byte of idx1's last byte.
    cdx_int32       idx1Total;     //total idx1 items in idx1 chunk，idx1专用
    cdx_int32       idx1Start;     //offset from the file start.指向第一个idx1_entry的位置。
    //cdx_int32       key_frame_index;  //current nearest key frame index,
                                        //index in keyframe_table items.这个key_frame在关键帧表里
                                        //的index，不是总的frame index表中的index。
    //cdx_int32       last_key_frame_index;   //last decode key frame index when ff/rr
    cdx_int32       indexInKeyfrmTbl;    //current nearest key frame index, index in
                                        //keyframe_table items.这个key_frame在关键帧表里
                                        //的index，不是总的frame index表中的index。
    cdx_int32       lastIndexInKeyfrmTbl;//last decode key frame index when ff/rr，
                                         //也就是上一次经过的entry的在keyframe table中的序号,
                                         //每次状态转换时,在SetProcMode()->AVI_IoCtrl()中初始化为-1
    cdx_int32       ckBaseOffset;  //idx1表指示的chunk offset value has two cases:
                                   //(1)from movi start,(2)from file start.
                                   //We base on file start to calc the start address.
    //cdx_int32       index_count;            //total items in created index buffer(idx1_buf).
    cdx_int32       indexCountInKeyfrmTbl;//total items in created index buffer(idx1_buf).
    cdx_int32       noPreLoad;            //audio before video, means pre_load.
    cdx_int32       drmFlag;
    cdx_uint64      fileSize;         //right now, we use 32bits for file length
    AviChunkT       dataChunk;        //for store chunk data.一般分配MAX_CHUNK_BUF_SIZE大小的
    //内存给它的buffer，而且在OpenMediaFile()中会调AVI_release_data_chunk_buf()释放掉，节省内存

    //CDX_S64       aud_chunk_counter;   //calc audio chunk count, not include current audio chunk
    //CDX_S64       aud_chunk_total_size;//calc current audio chunk total size, not include
                                         //current audio chunk size

    //顺序方式下, 在GetNextChunkInfo()中，读完一个chunk头之后,会累加统计，
    //这样算出来的就是下一个audio chunk的起始PTS了.
    //顺序方式和index方式都用这3个变量.因为GetNextChunkInfo()中统计audio持续时间的代码是共用的。
    //index模式下index reader有自己的统计变量，但基本不用.
    //这4个变量是avi_in统计时间用的，状态转换时需重置
    cdx_int32       frameIndex;            //frame idx in all frames.
    //在GetNextChunkInfo()中会增加, 状态转换时
    //setprocmode()->__reconfig_avi_read_context_readmode1()等重置，一定是从第一帧算起
    cdx_int32       frameIndex1;           //for Video stream 1
    cdx_int32       frameIndex2;           //for Video stream 2
    cdx_int64       nAudChunkCounterArray[MAX_AUDIO_STREAM];  //calc audio chunk count, not include
    // current audio chunk，顺序模式下，不一定是从文件头开始计数的，
    // 在FRRR->PLAY之后，定位的地方开始算
    cdx_int64       nAudFrameCounterArray[MAX_AUDIO_STREAM];  //作用同上.VBR音频应该以帧为单位计算,
    //一个chunk不一定等于一帧.这是nAudChunkCounterArray的精确替代
    cdx_int64       nAudChunkTotalSizeArray[MAX_AUDIO_STREAM];//calc current audio chunk total
    //size, not include current audio chunk size, 顺序模式下，
    //不一定是从文件头开始计数的，在FRRR->PLAY之后，定位的地方开始算
    cdx_uint32      uBaseAudioPtsArray[MAX_AUDIO_STREAM]; //pts of the audio who is follow
    //video key frame,基准时间,在parse过程中,audio_pts的计算以基准时间加上读到的数据的持续
    //时间为准,顺序方式和index方式下都用这个变量
    //for FFRR, after search key frame, reset avsync!现在又加了新的意义:随时记录当前audio chunk
    //的PTS，以便于换音轨后能正确的设置新音轨的时间
    struct PsrIdx1TableReader    vidIdx1Reader;   //use idx_fp, use vid_idx_data_chunk
    struct PsrIdx1TableReader    audIdx1Reader;   //use idx_fp, use aud_idx_data_chunk

    ODML_SUPERINDEX_READER  vidIndxReader;   //use idx_fp, use vid_idx_data_chunk
    ODML_SUPERINDEX_READER  audIndxReader;   //use idx_fp, use aud_idx_data_chunk

    cdx_char        *vopPrivInf; //vop private information, for some video format
                                 //to store private header information,e.g:H264,
                                 //VC1.需要时再malloc内存，然后指针赋值给p->vFormat.pCodecExtraData
    cdx_uint32     isNetworkStream;
} AviFileInT;

extern cdx_int32 AviMallocDataChunkBuf(AviFileInT *pAviIn);
extern cdx_int32 AviReleaseDataChunkBuf(AviFileInT *aviIn);
extern cdx_int16 GetNextChunk(CdxStreamT *fp, AviChunkT *chunk);
extern cdx_int16 AviReaderGetStreamInfo(AviFileInT *aviFile, AviStreamInfoT *sInfo,
    cdx_int32 index);
cdx_int32 CalcAviAudioChunkPts(AudioStreamInfoT *pAudioStreamInfo, cdx_int64 totalSize,
    cdx_int32 totalNum);
cdx_int64 CalcAviSubChunkPts(cdx_uint8 *buf, cdx_int64 *duration);
extern cdx_int16 GetNextChunkHead(CdxStreamT *pf, AviChunkT *chunk, cdx_uint32 *length);

#endif  /* _CDX_AVI_FILE_IN_H_ */

