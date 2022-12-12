// #include <CDX_LogNDebug.h>
#define LOG_TAG "Mp4MuxerDrv.c"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Mp4Muxer.h"
//#include <recorde_writer.h>
#include <FsWriter.h>
#include <vencoder.h>
//#include <type_camera.h>
#include <aencoder.h>

#include <ConfigOption.h>

#if (MPPCFG_GPS_PACK_METHOD==OPTION_GPS_PACK_IN_TRACK)
    unsigned int gps_pack_method = GPS_PACK_IN_TRACK;
#elif (MPPCFG_GPS_PACK_METHOD==OPTION_GPS_PACK_IN_MDAT)
    unsigned int gps_pack_method = GPS_PACK_IN_MDAT;
#else
    unsigned int gps_pack_method = GPS_PACK_IN_MDAT;
#endif

static FILE *fp_bs = NULL;
static int debug_pkt_id = 0xff;

static int Mp4MuxerCacheBuffMalloc(AVFormatContext *AvFomatCtx)
{ 
    AVFormatContext *Mp4MuxerCtx = AvFomatCtx;
    MOVContext *mov = Mp4MuxerCtx->priv_data;
    int stream_num = 0;
    int idx_tbl_cache_buff_size = 0;        // unit:word
    int tiny_page_cache_buff_size = 0;      // unit:word

    stream_num = Mp4MuxerCtx->nb_streams;
    stream_num += 1; // special process for gps,reverse one more space
    idx_tbl_cache_buff_size = (STCO_CACHE_SIZE+STSZ_CACHE_SIZE+STSC_CACHE_SIZE+STTS_CACHE_SIZE)*stream_num;
    
    Mp4MuxerCtx->mov_inf_cache  = (unsigned char*)malloc(idx_tbl_cache_buff_size*4);
    if(!Mp4MuxerCtx->mov_inf_cache)
    {
        printf("fatal_error_can't malloc mov info cache buffer,s:%d!\n",(idx_tbl_cache_buff_size*4));
        return -1;
    }
    memset(Mp4MuxerCtx->mov_inf_cache,0,(idx_tbl_cache_buff_size*4));

    tiny_page_cache_buff_size = MOV_CACHE_TINY_PAGE_SIZE*2;
    mov->cache_tiny_page_ptr = (unsigned int*)malloc(tiny_page_cache_buff_size*4);
    if(NULL == mov->cache_tiny_page_ptr)
    {
        if(NULL != Mp4MuxerCtx->mov_inf_cache)
        {
            free(Mp4MuxerCtx->mov_inf_cache);
            Mp4MuxerCtx->mov_inf_cache = NULL;
        }
        printf("fatal_error_can't malloc tiny page cache buffer,s:%d!\n",(tiny_page_cache_buff_size*4));
        return -1;
    }
    memset(mov->cache_tiny_page_ptr,0,(tiny_page_cache_buff_size*4));

    char *p_stss_start = (char *)malloc(KEYFRAME_CACHE_SIZE*stream_num);
    if(NULL == p_stss_start)
    {
        if(NULL != mov->cache_tiny_page_ptr)
        {
            free(mov->cache_tiny_page_ptr);
            mov->cache_tiny_page_ptr = NULL;
        }
        if(NULL != Mp4MuxerCtx->mov_inf_cache)
        {
            free(Mp4MuxerCtx->mov_inf_cache);
            Mp4MuxerCtx->mov_inf_cache = NULL;
        }
        printf("fatal_error_can't malloc keyfrm info cache buffer,s:%d!\n",(KEYFRAME_CACHE_SIZE*stream_num));
        return -1;
    }
    memset(p_stss_start,0,(KEYFRAME_CACHE_SIZE*stream_num));

    for(int i=0; i<stream_num; ++i)
    {
        mov->cache_keyframe_ptr[i] = (unsigned int *)(p_stss_start + i* KEYFRAME_CACHE_SIZE);
    }

    unsigned int *p_stco_start = (unsigned int*)Mp4MuxerCtx->mov_inf_cache;
    for(int i=0; i<stream_num; ++i)
    {
        mov->cache_start_ptr[STCO_ID][i] = p_stco_start + i*STCO_CACHE_SIZE;
        mov->cache_read_ptr[STCO_ID][i] = p_stco_start + i*STCO_CACHE_SIZE; 
        mov->cache_write_ptr[STCO_ID][i] = p_stco_start + i*STCO_CACHE_SIZE;
        mov->cache_end_ptr[STCO_ID][i] = mov->cache_start_ptr[STCO_ID][i] + (STCO_CACHE_SIZE - 1);
    }

    unsigned int *p_stsz_start = p_stco_start + stream_num*STCO_CACHE_SIZE;
    for(int i=0; i<stream_num; ++i)
    {
        mov->cache_start_ptr[STSZ_ID][i] = p_stsz_start + i*STSZ_CACHE_SIZE;
        mov->cache_read_ptr[STSZ_ID][i] = p_stsz_start + i*STSZ_CACHE_SIZE; 
        mov->cache_write_ptr[STSZ_ID][i] = p_stsz_start + i*STSZ_CACHE_SIZE;
        mov->cache_end_ptr[STSZ_ID][i] = mov->cache_start_ptr[STSZ_ID][i] + (STSZ_CACHE_SIZE - 1);
    }

    unsigned int *p_stsc_start = p_stsz_start + stream_num*STSZ_CACHE_SIZE;
    for(int i=0; i<stream_num; ++i)
    {
        mov->cache_start_ptr[STSC_ID][i] = p_stsc_start + i*STSC_CACHE_SIZE;
        mov->cache_read_ptr[STSC_ID][i] = p_stsc_start + i*STSC_CACHE_SIZE; 
        mov->cache_write_ptr[STSC_ID][i] = p_stsc_start + i*STSC_CACHE_SIZE;
        mov->cache_end_ptr[STSC_ID][i] = mov->cache_start_ptr[STSC_ID][i] + (STSC_CACHE_SIZE - 1);
    }

    unsigned int *p_stts_start = p_stsc_start + stream_num*STSC_CACHE_SIZE;
    for(int i=0; i<stream_num; ++i)
    {
        mov->cache_start_ptr[STTS_ID][i] = p_stts_start + i*STTS_CACHE_SIZE;
        mov->cache_read_ptr[STTS_ID][i] = p_stts_start + i*STTS_CACHE_SIZE; 
        mov->cache_write_ptr[STTS_ID][i] = p_stts_start + i*STTS_CACHE_SIZE;
        mov->cache_end_ptr[STTS_ID][i] = mov->cache_start_ptr[STTS_ID][i] + (STTS_CACHE_SIZE - 1);
    }

    return 0;
}

static void Mp4MuxerCacheBuffFree(AVFormatContext *AvFomatCtx)
{
    AVFormatContext *Mp4MuxerCtx = AvFomatCtx;
    MOVContext *mov = Mp4MuxerCtx->priv_data;

    if(NULL != mov->cache_keyframe_ptr[0])
    {
        free(mov->cache_keyframe_ptr[0]);
        mov->cache_keyframe_ptr[0] = NULL;
    }

    if(NULL != mov->cache_tiny_page_ptr)
    {
        free(mov->cache_tiny_page_ptr);
        mov->cache_tiny_page_ptr = NULL;
    }
    
    if(NULL != Mp4MuxerCtx->mov_inf_cache)
    {
        free(Mp4MuxerCtx->mov_inf_cache);
        Mp4MuxerCtx->mov_inf_cache = NULL;
    }
}
int Mp4MuxerWriteVos(void *handle, unsigned char *vosData, unsigned int vosLen, unsigned int idx)
{
	AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle;
	MOVContext *mov = Mp4MuxerCtx->priv_data;
	MOVTrack *trk = NULL;
    if(idx < MAX_STREAMS)
    {
        trk = &mov->tracks[idx];
    }
    MOV_META *p_mov_meta = &mov->mov_meta_info;

	if(vosLen)
	{
    	trk->vosData = (char *)malloc(vosLen);
        if(NULL == trk->vosData)
        {
            aloge("mp4_vos_buff_malloc_failed,s:%d",vosLen);
            return -1;
        }
    	trk->vosLen  = vosLen;
        memcpy(trk->vosData,vosData,vosLen);
        
        alogd("mp4_vos_info,strm:%d,s:%d",idx,vosLen);

        aloge("mp4_vos_info,strm:%d,s:%d",idx,vosLen);
        if(idx < p_mov_meta->v_strm_num)        // video vos
        {
            p_mov_meta->v_strms_meta[idx].v_vos_len = vosLen;
            p_mov_meta->v_strms_meta[idx].v_vos_buff = trk->vosData;
        }
        else if(idx == p_mov_meta->v_strm_num)  // audio vos
        {
            p_mov_meta->a_strms_meta[0].a_vos_len = vosLen;
            p_mov_meta->a_strms_meta[0].a_vos_buff = trk->vosData;
        }
        else if(idx == p_mov_meta->v_strm_num+p_mov_meta->a_strm_num) // text vos
        {
            p_mov_meta->t_strms_meta.t_vos_len = vosLen;
            p_mov_meta->t_strms_meta.t_vos_buff = trk->vosData;
        }

		if(debug_pkt_id == idx)
		{
			fwrite(trk->vosData,1,vosLen,fp_bs);
		}
	}
    else
    {
        trk->vosData = NULL;
        trk->vosLen  = 0;
    }

	return 0;
}

int Mp4MuxerWriteHeader(void *handle)
{
	AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle;
    //MOVContext *mov = Mp4MuxerCtx->priv_data;
    char *pCache = NULL;
    unsigned int nCacheSize = 0;

    if(Mp4MuxerCtx->mpFsWriter)
    {
        aloge("fatal error! why mov->mpFsWriter[%p]!=NULL", Mp4MuxerCtx->mpFsWriter);
        return -1;
    }
    if(Mp4MuxerCtx->pb_cache)
    {
        FSWRITEMODE mode = Mp4MuxerCtx->mFsWriteMode;
        if(FSWRITEMODE_CACHETHREAD == mode)
        {
    		if (Mp4MuxerCtx->mCacheMemInfo.mCacheSize > 0 && Mp4MuxerCtx->mCacheMemInfo.mpCache != NULL)
    		{
    			mode = FSWRITEMODE_CACHETHREAD;
                pCache = Mp4MuxerCtx->mCacheMemInfo.mpCache;
                nCacheSize = Mp4MuxerCtx->mCacheMemInfo.mCacheSize;
    		}
    		else
    		{
                aloge("fatal error! not set cacheMemory but set mode FSWRITEMODE_CACHETHREAD! use FSWRITEMODE_DIRECT.");
                mode = FSWRITEMODE_DIRECT;
    		}
        }
        else if(FSWRITEMODE_SIMPLECACHE == mode)
        {
            pCache = NULL;
            nCacheSize = Mp4MuxerCtx->mFsSimpleCacheSize;
        }
        Mp4MuxerCtx->mpFsWriter = createFsWriter(mode, Mp4MuxerCtx->pb_cache, pCache, nCacheSize, 
																		Mp4MuxerCtx->streams[0]->codec.codec_id);
        if(NULL == Mp4MuxerCtx->mpFsWriter)
        {
            aloge("fatal error! create FsWriter() fail!");
            return -1;
        }
    }

    if(Mp4MuxerCtx->file_repair_flag)
    {
        return mov_write_header_fake(Mp4MuxerCtx);
    }
    else
    {
        return mov_write_header(Mp4MuxerCtx);
    }
}

int Mp4MuxerWriteTrailer(void *handle)
{
	AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle;
    return mov_write_trailer(Mp4MuxerCtx);
}

int Mp4MuxerWritePacket(void *handle, void *pkt)
{
	AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle;
    AVPacket * tmp_pkt = (AVPacket *)pkt;

    if(Mp4MuxerCtx->file_repair_flag)
    {
        return mov_write_packet_fake(Mp4MuxerCtx, (AVPacket *)pkt);
    }
    else
    {
        if(debug_pkt_id == tmp_pkt->stream_index)
        {
            fwrite(tmp_pkt->data0,1,tmp_pkt->size0,fp_bs);
            fwrite(tmp_pkt->data1,1,tmp_pkt->size1,fp_bs);
        }

        return mov_write_packet(Mp4MuxerCtx, (AVPacket *)pkt);
    }
}

int Mp4MuxerIoctrl(void *handle, unsigned int uCmd, unsigned int uParam, void *pParam2)
{
	AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle;
	MOVContext *mov = Mp4MuxerCtx->priv_data;
	_media_file_inf_t *pMediaInf = 0;

	switch (uCmd)
	{
	case SETAVPARA:	
    {
        pMediaInf = (_media_file_inf_t *)pParam2;
        if (NULL == pMediaInf)
        {
            aloge("mp4_media_info_fatal_error in param\n");
            return -1;
        }
        
        int tmp_idx = 0;
        int v_strm_num = pMediaInf->mVideoInfoValidNum;
        MediaVideoInfo *p_v_info = NULL;
        AVStream *p_av_strm = NULL;
        MOV_META *p_mov_meta = &mov->mov_meta_info;

        p_mov_meta->v_strm_num = v_strm_num;

        // video info setting
        for(tmp_idx=0; tmp_idx<v_strm_num; ++tmp_idx)
        {
            p_v_info = &pMediaInf->mMediaVideoInfo[tmp_idx];
            p_av_strm = Mp4MuxerCtx->streams[tmp_idx];

            p_mov_meta->v_strms_meta[tmp_idx].v_enc_type = 
                                                p_v_info->mVideoEncodeType;
            p_mov_meta->v_strms_meta[tmp_idx].v_frm_rate = 
                                                p_v_info->uVideoFrmRate;
            p_mov_meta->v_strms_meta[tmp_idx].v_frm_width = 
                                                p_v_info->nWidth;
            p_mov_meta->v_strms_meta[tmp_idx].v_frm_height = 
                                                p_v_info->nHeight;
            p_mov_meta->v_strms_meta[tmp_idx].v_frm_rotate_degree = 
                                                p_v_info->rotate_degree;
            p_mov_meta->v_strms_meta[tmp_idx].v_frm_max_key_frm_interval = 
                                pMediaInf->mMediaVideoInfo[0].maxKeyInterval;
            p_mov_meta->v_strms_meta[tmp_idx].v_create_time = 
                                pMediaInf->mMediaVideoInfo[0].create_time;

            p_av_strm->codec.height = p_v_info->nHeight;
            p_av_strm->codec.width  = p_v_info->nWidth;
            p_av_strm->codec.frame_rate = p_v_info->uVideoFrmRate;
            
            aloge("mp4_v_strm_info:%d-%d-%d-%d",tmp_idx,p_v_info->nHeight,p_v_info->nWidth,p_v_info->uVideoFrmRate);
            p_av_strm->codec.rotate_degree = p_v_info->rotate_degree; //set rotate degree
            p_av_strm->codec.codec_type = CODEC_TYPE_VIDEO;

            mov->tracks[tmp_idx].timescale = 1000;

            switch(p_v_info->mVideoEncodeType)
            {
                case VENC_CODEC_JPEG:
                    p_av_strm->codec.codec_id = CODEC_ID_MJPEG;
                    break;
                case VENC_CODEC_H264:
                    p_av_strm->codec.codec_id = CODEC_ID_H264;
                    break;
                case VENC_CODEC_H265:
                    p_av_strm->codec.codec_id = CODEC_ID_H265;
                    break;
                default:
                    aloge("fatal error! unknown video encode type[0x%x]", p_v_info->mVideoEncodeType);
                    p_av_strm->codec.codec_id = CODEC_ID_H264;
                    break;
            }
        }
        //set video parameters
        mov->create_time = pMediaInf->mMediaVideoInfo[0].create_time;
        mov->keyframe_interval = pMediaInf->mMediaVideoInfo[0].maxKeyInterval;// not use anymore

        Mp4MuxerCtx->nb_v_streams = v_strm_num;
        Mp4MuxerCtx->nb_streams += v_strm_num;

        // audio info setting
        tmp_idx = Mp4MuxerCtx->nb_streams;
        p_av_strm = Mp4MuxerCtx->streams[tmp_idx];

        if(0 == pMediaInf->channels && 0 == pMediaInf->sample_rate)
        {
            p_mov_meta->a_strm_num = 0;
        }
        else
        {
            p_mov_meta->a_strm_num = 1;

            if(0 != p_mov_meta->a_strm_num)
            {
                p_mov_meta->a_strms_meta[0].a_enc_type = 
                                                pMediaInf->audio_encode_type;
                p_mov_meta->a_strms_meta[0].a_sample_rate = 
                                                pMediaInf->sample_rate;
                p_mov_meta->a_strms_meta[0].a_chn_num = 
                                                pMediaInf->channels;
                p_mov_meta->a_strms_meta[0].a_bit_depth = 
                                                pMediaInf->bits_per_sample;
                p_mov_meta->a_strms_meta[0].a_frm_size = 
                                                pMediaInf->frame_size;
            }
            p_av_strm->codec.channels = pMediaInf->channels;
            p_av_strm->codec.bits_per_sample = pMediaInf->bits_per_sample;
            p_av_strm->codec.frame_size  = pMediaInf->frame_size;
            p_av_strm->codec.sample_rate = pMediaInf->sample_rate;
            p_av_strm->codec.codec_type = CODEC_TYPE_AUDIO;

            mov->tracks[tmp_idx].timescale = p_av_strm->codec.sample_rate;
            mov->tracks[tmp_idx].sampleDuration = 1;

            aloge("mp4_a_strm_info:%d-%d-%d-%d-%d",tmp_idx,pMediaInf->channels,pMediaInf->bits_per_sample,
                    pMediaInf->sample_rate,pMediaInf->frame_size);

            switch(pMediaInf->audio_encode_type)
            {
                case AUDIO_ENCODER_PCM_TYPE:
                    p_av_strm->codec.codec_id = CODEC_ID_PCM;
                    break;
                case AUDIO_ENCODER_AAC_TYPE:
                    p_av_strm->codec.codec_id = CODEC_ID_AAC;
                    break;
                default:
                    aloge("fatal error! unknown audio encode type[0x%x]", pMediaInf->audio_encode_type);
                    p_av_strm->codec.codec_id = CODEC_ID_AAC;
                    break;
            }
            Mp4MuxerCtx->nb_streams++;
        }

        // text info setting 
        mov->geo_available = pMediaInf->geo_available;
        mov->latitudex10000 = pMediaInf->latitudex10000;
        mov->longitudex10000 = pMediaInf->longitudex10000;
        
        if(mov->geo_available)
        {
            p_mov_meta->t_strm_num = 1;
        }
        else
        {
            p_mov_meta->t_strm_num = 0;
        }

        if(0 != p_mov_meta->t_strm_num)
        {
            p_mov_meta->t_strms_meta.t_enc_mode = gps_pack_method;
        }

        if(mov->geo_available && gps_pack_method==GPS_PACK_IN_MDAT)  // to prepare buffer needed by mp4
        { 
            if(mov->gps_entry_buff)
            {
                alogw("Be careful! free gps_entry_buff[%p] first.", mov->gps_entry_buff);
                free(mov->gps_entry_buff);
                mov->gps_entry_buff = NULL;
            }
            mov->gps_entry_buff = (GPS_ENTRY *)malloc(sizeof(GPS_ENTRY)*MOV_GPS_MAX_ENTRY_NUM);
            if(NULL == mov->gps_entry_buff)
            {
                mov->geo_available = 0;     // no reource to store gps info
            }
            mov->gps_strm_idx = Mp4MuxerCtx->nb_streams; // calculate the gps stream idx  used when writing pkt 
        }
        if(mov->geo_available && gps_pack_method==GPS_PACK_IN_TRACK)
        {
            //set text parameters
            Mp4MuxerCtx->streams[Mp4MuxerCtx->nb_streams]->codec.codec_id = CODEC_ID_GGAD; // pMediaInf->text_encode_type;
            Mp4MuxerCtx->streams[Mp4MuxerCtx->nb_streams]->codec.codec_type = CODEC_TYPE_TEXT;
            mov->tracks[Mp4MuxerCtx->nb_streams].timescale = 1000;
            Mp4MuxerCtx->nb_streams++;
        }

        int tmp_ret = Mp4MuxerCacheBuffMalloc(Mp4MuxerCtx);
        if(0 != tmp_ret)
        {
            aloge("mp4_malloc_idex_cache_failed");
            return -1;
        }
        break;  
    }
	case SETTOTALTIME: //ms  pending ?????
		aloge("mp4_set_dur:%d",(int)uParam);
		// mov->tracks[0].trackDuration = (unsigned int)(((__u64)uParam * mov->tracks[0].timescale) / 1000);
		// mov->tracks[1].trackDuration = (unsigned int)(((__u64)uParam * mov->tracks[1].timescale) / 1000);
		// mov->tracks[2].trackDuration = (unsigned int)(((__u64)uParam * mov->tracks[2].timescale) / 1000);
		break;
    case SETFALLOCATELEN:
        Mp4MuxerCtx->mFallocateLen = uParam;
        break;
	case SETCACHEFD:
    {
        //s->pb = (FILE *)uParam;
        CedarXDataSourceDesc datasourceDesc;
        memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
        datasourceDesc.source_url = (char*)pParam2;
        datasourceDesc.source_type = CEDARX_SOURCE_FILEPATH;
        datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
        Mp4MuxerCtx->pb_cache = create_outstream_handle(&datasourceDesc);
        if(NULL == Mp4MuxerCtx->pb_cache)
        {
            aloge("fatal error! create mp4 outstream fail.");
            return -1;
        }
		break;
    }
    case SETCACHEFD2:
    {
        CedarXDataSourceDesc datasourceDesc;
        memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
        datasourceDesc.ext_fd_desc.fd = (int)uParam;
        datasourceDesc.source_type = CEDARX_SOURCE_FD;
        datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
        Mp4MuxerCtx->pb_cache = create_outstream_handle(&datasourceDesc);
        if(NULL == Mp4MuxerCtx->pb_cache)
        {
            aloge("fatal error! create aac outstream fail.");
            return -1;
        }
        if(Mp4MuxerCtx->mFallocateLen > 0)
        {
            if(Mp4MuxerCtx->pb_cache->fallocate(Mp4MuxerCtx->pb_cache, 0x01, 0, Mp4MuxerCtx->mFallocateLen) < 0) 
            {
                aloge("fatal error! Failed to fallocate size %d, fd[%d](%s)", 
						Mp4MuxerCtx->mFallocateLen, Mp4MuxerCtx->pb_cache->fd_desc.fd, strerror(errno));
            }
        }
		break;
    }
	case  REGISTER_WRITE_CALLBACK:
		Mp4MuxerCtx->datasource_desc.source_url = (char*)pParam2;
		Mp4MuxerCtx->datasource_desc.source_type = CEDARX_SOURCE_WRITER_CALLBACK;
		if(Mp4MuxerCtx->OutStreamHandle == NULL) {
			Mp4MuxerCtx->OutStreamHandle = create_outstream_handle(&Mp4MuxerCtx->datasource_desc);
            if (NULL == Mp4MuxerCtx->OutStreamHandle)
            {
                aloge("fatal error! create callback outstream fail.");
                return -1;
            }
		}
		else {
			aloge("RawMuxerCtx->OutStreamHandle not NULL");
		}
		break;

    case SETSDCARDSTATE:
        Mp4MuxerCtx->mbSdcardDisappear = !uParam;
        alogd("SETSDCARDSTATE, Mp4MuxerCtx->mbSdcardDisappear[%d]", Mp4MuxerCtx->mbSdcardDisappear);
        break;
    case SETCACHEMEM:
        Mp4MuxerCtx->mCacheMemInfo = *(FsCacheMemInfo*)pParam2;
        break;
    case SET_FS_WRITE_MODE:
        Mp4MuxerCtx->mFsWriteMode = uParam;
        break;
    case SET_FS_SIMPLE_CACHE_SIZE:
        Mp4MuxerCtx->mFsSimpleCacheSize = (int)uParam;
        break;
    case SET_STREAM_CALLBACK:
    {
        cdx_write_callback_t *callback = (cdx_write_callback_t*)pParam2;
        if (Mp4MuxerCtx->pb_cache != NULL) {
            Mp4MuxerCtx->pb_cache->callback.hComp = callback->hComp;
            Mp4MuxerCtx->pb_cache->callback.cb = callback->cb;
        } else {
            alogw("Mp4MuxerCtx->pb_cache not initialize!!");
        }
        break;
    }
    case SET_FILE_REPAIR_FLAG:
    {
        Mp4MuxerCtx->file_repair_flag = (int)uParam;
        break;
    }
    case SET_ADD_REPAIR_INFO_FLAG:
    {
        Mp4MuxerCtx->file_add_repair_info_flag = (int)uParam;
        break;
    }
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    case SET_FILE_REPAIR_INTERVAL:
    {
        mov->frms_tag_max_interval = uParam;
        if(mov->frms_tag_max_interval <=0)
        {
            mov->frms_tag_max_interval = 1000000;
        }
        break;
    }
#endif
	default:
		break;
	}

	return 0;
}

void *Mp4MuxerOpen(int *ret)
{
	AVFormatContext *Mp4MuxerCtx;
	MOVContext *mov;
	AVStream *st;

	*ret = 0;
	Mp4MuxerCtx = (AVFormatContext *)malloc(sizeof(AVFormatContext));
	if(!Mp4MuxerCtx)
	{	
		*ret = -1;
		aloge("mp4_context_buf_malloc_failed,s:%d",(sizeof(AVFormatContext)));
		return NULL;
	}
	
	memset(Mp4MuxerCtx,0,sizeof(AVFormatContext));
	
	mov = (MOVContext *)malloc(sizeof(MOVContext));
	if(!mov) {
        if(NULL != Mp4MuxerCtx)
        {
            free(Mp4MuxerCtx);
            Mp4MuxerCtx = NULL;
        }
        *ret = -1;
        aloge("mp4_movctx_buf_malloc_failed,s:%d",(sizeof(MOVContext)));
        return NULL;
	}
	memset(mov,0,sizeof(MOVContext));
	Mp4MuxerCtx->priv_data = (void *)mov;
	
	for(int j=0; j<MAX_STREAMS_IN_FILE; ++j)
	{
		st = (AVStream *)malloc(sizeof(AVStream));
		if(!st) 
        {
            if(NULL != mov)
            {
                free(mov);
                mov = NULL;
            }
            if(NULL != Mp4MuxerCtx)
            {
                free(Mp4MuxerCtx);
                Mp4MuxerCtx = NULL;
            }
            *ret = -1;
            aloge("mp4_stmctx_buf_malloc_failed,idx:%d,total:%d,s:%d",j,MAX_STREAMS_IN_FILE,(sizeof(AVStream)));
            return (void*)Mp4MuxerCtx;
		}
		memset(st,0,sizeof(AVStream));
		Mp4MuxerCtx->streams[j] = st;
	}

    mov->last_stream_index = -1;
	Mp4MuxerCtx->firstframe[0] = 1;
	Mp4MuxerCtx->firstframe[1] = 1;
	mov->keyframe_interval = 1;
    
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    mov->frms_tag_pts_strm_idx = 0;
    mov->frms_tag_first_frm_pts = -1;
    mov->frms_tag_first_frm_pts_set = 0;
    mov->frms_tag_max_interval = 1000000;
    if(NULL == mov->frms_tag.frm_hdrs)
    {
        mov->frms_tag.frm_hdrs = (MOV_FRM_HDR *)malloc(MAX_FRM_HDRS_IN_FRMS_TAG*sizeof(MOV_FRM_HDR));
        if(NULL == mov->frms_tag.frm_hdrs)
        {
            for(int j=0; j<MAX_STREAMS_IN_FILE; ++j)
            {
                if(NULL !=  Mp4MuxerCtx->streams[j])
                {
                    free(Mp4MuxerCtx->streams[j]);
                    Mp4MuxerCtx->streams[j] = NULL;
                }
            }
            if(NULL != mov)
            {
                free(mov);
                mov = NULL;
            }
            if(NULL != Mp4MuxerCtx)
            {
                free(Mp4MuxerCtx);
                Mp4MuxerCtx = NULL;
            }
            *ret = -1;
            aloge("mp4_muxer_fatal_error_malloc_frm_hdrs_buff_failed,%d",MAX_FRM_HDRS_IN_FRMS_TAG*sizeof(MOV_FRM_HDR));
            return (void*)Mp4MuxerCtx;
        }
        memset(mov->frms_tag.frm_hdrs,0,MAX_FRM_HDRS_IN_FRMS_TAG*sizeof(MOV_FRM_HDR));
    }
#endif
    if(debug_pkt_id != 0xff)
    {
        fp_bs = fopen("/mnt/extsd/input_video.h264", "wb");
    }
	return (void*)Mp4MuxerCtx;
}

int Mp4MuxerClose(void *handle)
{
	AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle;
	MOVContext *mov = Mp4MuxerCtx->priv_data;
    int i;

	Mp4MuxerCacheBuffFree(Mp4MuxerCtx);

    for(i=0;i<MAX_STREAMS_IN_FILE;i++)
    {
        if(mov->fd_stts[i])
        {
            destroy_outstream_handle((struct cdx_stream_info*)mov->fd_stts[i]);
            mov->fd_stts[i] = 0;
            if(0 == Mp4MuxerCtx->mbSdcardDisappear)
            {
                stream_remove_file(mov->FilePath_stts[i]);
                alogd("remove fd_stts[%d]name[%s]", i, mov->FilePath_stts[i]);
            }
        }

        if(mov->fd_stsz[i])
        {
            destroy_outstream_handle((struct cdx_stream_info*)mov->fd_stsz[i]);
            mov->fd_stsz[i] = 0;
            if(0 == Mp4MuxerCtx->mbSdcardDisappear)
            {
                stream_remove_file(mov->FilePath_stsz[i]);
                alogd("remove fd_stsz[%d]name[%s]", i, mov->FilePath_stsz[i]);
            }
        }

        if(mov->fd_stco[i])
        {
            destroy_outstream_handle((struct cdx_stream_info*)mov->fd_stco[i]);
            mov->fd_stco[i] = 0;
            if(0 == Mp4MuxerCtx->mbSdcardDisappear)
            {
                stream_remove_file(mov->FilePath_stco[i]);
                alogd("remove fd_stco[%d]name[%s]", i, mov->FilePath_stco[i]);
            }
        }

        if(mov->fd_stsc[i])
        {
            destroy_outstream_handle((struct cdx_stream_info*)mov->fd_stsc[i]);
            mov->fd_stsc[i] = 0;
            if(0 == Mp4MuxerCtx->mbSdcardDisappear)
            {
                stream_remove_file(mov->FilePath_stsc[i]);
                alogd("remove fd_stsc[%d]name[%s]", i, mov->FilePath_stsc[i]);
            }
        }
    }
    
	for(i=0;i<MAX_STREAMS_IN_FILE;i++)
	{
        if(mov->tracks[i].vosData)
    	{
    		free(mov->tracks[i].vosData);
            mov->tracks[i].vosData = NULL;
    	}
		if(Mp4MuxerCtx->streams[i])
		{
			free(Mp4MuxerCtx->streams[i]);
            Mp4MuxerCtx->streams[i] = 0;
		}
	}
    if(NULL != mov->p_thm_pic_addr)
    {
        free(mov->p_thm_pic_addr);
        mov->p_thm_pic_addr = NULL;
    }
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    if(NULL != mov->frms_tag.frm_hdrs)
    {
        free(mov->frms_tag.frm_hdrs);
        mov->frms_tag.frm_hdrs = NULL;
    }
#endif
    
    if(NULL != mov->gps_entry_buff)
    {
        free(mov->gps_entry_buff);
        mov->gps_entry_buff = NULL;
    }
    if(Mp4MuxerCtx->mpFsWriter)
    {
        destroyFsWriter(Mp4MuxerCtx->mpFsWriter);
        Mp4MuxerCtx->mpFsWriter = NULL;
    }
    if(Mp4MuxerCtx->pb_cache)
    {
        destroy_outstream_handle(Mp4MuxerCtx->pb_cache);
        Mp4MuxerCtx->pb_cache = NULL;
    }
	if(Mp4MuxerCtx->priv_data)
	{
		free(Mp4MuxerCtx->priv_data);
		Mp4MuxerCtx->priv_data = NULL;
	}
	
	if(Mp4MuxerCtx)
	{
		free(Mp4MuxerCtx);
		Mp4MuxerCtx = NULL;
	}

    if(fp_bs != NULL)
    {
        fclose(fp_bs);
        fp_bs = NULL;
    }
	return 0;
}

CDX_RecordWriter record_writer_mp4 = {
	.info 				 = "recode write mp4"   ,
	.MuxerOpen           = Mp4MuxerOpen         ,
	.MuxerClose          = Mp4MuxerClose        ,
    .MuxerWriteExtraData = Mp4MuxerWriteVos     ,
	.MuxerWriteHeader    = Mp4MuxerWriteHeader  ,
	.MuxerWriteTrailer   = Mp4MuxerWriteTrailer ,
	.MuxerWritePacket    = Mp4MuxerWritePacket  ,
	.MuxerIoctrl         = Mp4MuxerIoctrl       ,
};
