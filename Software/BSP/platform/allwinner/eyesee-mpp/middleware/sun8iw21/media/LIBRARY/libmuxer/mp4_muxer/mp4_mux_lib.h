/*
********************************************************************************
*                                    eMOD
*                   the Easy Portable/Player Develop Kits
*                               mod_herb sub-system
*                               mp4 mux lib module
*
*          (c) Copyright 2009-2010, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : mp4_mux_lib.h
* Version: V1.0
* By     : Eric_wang
* Date   : 2010-4-2
* Description: mp4������API�ļ�
********************************************************************************
*/
#ifndef _MP4_MUX_LIB_H_
#define _MP4_MUX_LIB_H_
// #include "muxer_mp4_cfg.h"

//#include <encoder_type.h>
#include <stdio.h>

#include <record_writer.h>
#include <FsWriter.h>

// star
// #define ByteIOContext FILE
#define MAX_STREAMS 8		// 2 -> 3(add text track)

typedef struct AVRational{
    int num; ///< numerator
    int den; ///< denominator
} AVRational;

typedef struct AVCodecContext {

    int bit_rate;
	int width;
	int height;
	enum CodecType codec_type; /* see CODEC_TYPE_xxx */
	int rotate_degree;
	unsigned int codec_tag;
    unsigned int codec_id;

	int channels;
	int frame_size;   //for audio, it means sample count a audioFrame contain; 
	                      //in fact, its value is assigned by pcmDataSize(MAXDECODESAMPLE=1024) which is transport by previous AudioSourceComponent.
	                      //aac encoder will encode a frame from MAXDECODESAMPLE samples; but for pcm, one frame == one sample according to movSpec.
	int frame_rate;

	int bits_per_sample;
	int sample_rate;

	AVRational time_base;
}AVCodecContext;

typedef struct AVStream {
    int index;    /**< stream index in AVFormatContext */
    int id;       /**< format specific stream id */
    AVCodecContext codec; /**< codec context */
    AVRational r_frame_rate;
    void *priv_data;

    /* internal data used in av_find_stream_info() */
    int64_t first_dts;
    /** encoding: PTS generation when outputing stream */
    //struct AVFrac pts;


    AVRational time_base;
    //FIXME move stuff to a flags field?
    /** quality, as it has been removed from AVCodecContext and put in AVVideoFrame
     * MN: dunno if that is the right place for it */
    float quality;
    int64_t start_time;
    int64_t duration;

    int64_t cur_dts;

} AVStream;

typedef struct AVFormatContext {
	AVRational time_base;
    void *priv_data;    //MOVContext
    //ByteIOContext *pb;
    //FILE* pb_cache;    //cache�����µ��ļ�ָ��, ���HERB�Լ��������ļ�ϵͳ,__herb_file_ptr_t *
    struct cdx_stream_info  *pb_cache;
    unsigned int     mFallocateLen; 
    int nb_streams;
    int nb_v_streams;
    AVStream *streams[MAX_STREAMS];
    char filename[1024]; /**< input or output filename */

    int64_t data_offset; /** offset of the first packet */
    int index_built;

    unsigned int nb_programs;
	int64_t timestamp;
	char firstframe[2];
    unsigned int total_video_frames;
    unsigned char *mov_inf_cache;

	struct cdx_stream_info *OutStreamHandle;
	CedarXDataSourceDesc datasource_desc;
	RawPacketHeader RawPacketHdr;
    unsigned int mbSdcardDisappear;  //1:sdcard disappear, 0:sdcard is normal.

    FsWriter *mpFsWriter;
    FsCacheMemInfo mCacheMemInfo;   //RecRender malloc, set to mp4Muxer to use.
    FSWRITEMODE mFsWriteMode;
    int     mFsSimpleCacheSize;
    int file_repair_flag;
    int file_add_repair_info_flag;
} AVFormatContext;

typedef struct
{
    unsigned int extra_data_len;
    unsigned char extra_data[32];
} __extra_data_t;

#endif  /* _MP4_MUX_LIB_H_ */

