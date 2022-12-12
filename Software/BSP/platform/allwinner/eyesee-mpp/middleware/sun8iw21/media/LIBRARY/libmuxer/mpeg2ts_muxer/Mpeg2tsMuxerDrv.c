//#define LOG_NDEBUG 0
#define LOG_TAG "Mpeg2tstsMuxerDiv"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>
#include <vencoder.h>

#include "Mpeg2tsMuxer.h"

#include <cedarx_stream.h>

static int32_t av_rescale_rnd(int64_t a, int64_t b, int64_t c)
{
    return (a * b + c-1)/c;
}

int32_t Mpeg2tsMuxerWriteVos(void *handle, uint8_t *vosData, uint32_t vosLen, uint32_t idx)
{
	AVFormatContext *Mpeg2tsMuxerCtx = (AVFormatContext *)handle;
	AVStream *trk = Mpeg2tsMuxerCtx->streams[idx];

	if(vosLen)
	{
    	trk->vosData = (int8_t *)malloc(vosLen);
    	trk->vosLen  = vosLen;
        memcpy(trk->vosData,vosData,vosLen);
	}
    else
    {
        trk->vosData = NULL;
        trk->vosLen  = 0;
    }
	return 0;
}

int32_t Mpeg2tsMuxerWriteHeader(void *handle)
{
	AVFormatContext *Mpeg2tsMuxerCtx = (AVFormatContext *)handle;
    int8_t *pCache = NULL;
    uint32_t nCacheSize = 0;
    if(Mpeg2tsMuxerCtx->mpFsWriter)
    {
        aloge("fatal error! why Mpeg2tsMuxerCtx->mpFsWriter[%p]!=NULL", Mpeg2tsMuxerCtx->mpFsWriter);
        return -1;
    }
    if(Mpeg2tsMuxerCtx->pb_cache)
    {
        FSWRITEMODE mode = Mpeg2tsMuxerCtx->mFsWriteMode;
        if(FSWRITEMODE_CACHETHREAD == mode)
        {
    		if (Mpeg2tsMuxerCtx->mCacheMemInfo.mCacheSize > 0 && Mpeg2tsMuxerCtx->mCacheMemInfo.mpCache != NULL)
    		{
    			mode = FSWRITEMODE_CACHETHREAD;
                pCache = (int8_t*)Mpeg2tsMuxerCtx->mCacheMemInfo.mpCache;
                nCacheSize = Mpeg2tsMuxerCtx->mCacheMemInfo.mCacheSize;
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
            nCacheSize = Mpeg2tsMuxerCtx->mFsSimpleCacheSize;
        }
        Mpeg2tsMuxerCtx->mpFsWriter = createFsWriter(mode, Mpeg2tsMuxerCtx->pb_cache, (char*)pCache, nCacheSize,
                                                                            Mpeg2tsMuxerCtx->streams[0]->codec.codec_id);
        if(NULL == Mpeg2tsMuxerCtx->mpFsWriter)
        {
            aloge("fatal error! create FsWriter() fail!");
            return -1;
        }
    }
	return 0;
}

int32_t Mpeg2tsMuxerWriteTrailer(void *handle)
{
	AVFormatContext *Mpeg2tsMuxerCtx = (AVFormatContext *)handle;
	ts_write_trailer(Mpeg2tsMuxerCtx);
	return 0;
}

int32_t Mpeg2tsMuxerWritePacket(void *handle, void *pkt)
{
    AVPacket tsPkt = *(AVPacket*)pkt;
    tsPkt.pts = tsPkt.pts/1000;
	return ts_write_packet(handle, &tsPkt);
	//return 0;
}

int32_t Mpeg2tsMuxerIoctrl(void *handle, uint32_t uCmd, uint32_t uParam, void *pParam2)
{
	AVFormatContext *Mpeg2tsMuxerCtx = (AVFormatContext *)handle;
	MpegTSWrite *ts = Mpeg2tsMuxerCtx->priv_data;
	_media_file_inf_t *pMediaInf = 0;
    AVStream *st, *pcr_st = NULL;
    MpegTSWriteStream *ts_st;
    MpegTSService *service;
	int i;
	
	switch (uCmd)
	{
	case SETAVPARA:	
		
		pMediaInf = (_media_file_inf_t *)pParam2;
		
		if (NULL == pMediaInf) {
			aloge("error in param\n");
			return -1;
		}
		
		//set video parameters
		if(pMediaInf->mMediaVideoInfo[0].nWidth && pMediaInf->mMediaVideoInfo[0].nWidth) {
			Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.height = pMediaInf->mMediaVideoInfo[0].nHeight;
			Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.width  = pMediaInf->mMediaVideoInfo[0].nWidth;
			Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.frame_rate = pMediaInf->mMediaVideoInfo[0].uVideoFrmRate;
            switch(pMediaInf->mMediaVideoInfo[0].mVideoEncodeType)
            {
                case VENC_CODEC_H264:
                {
                    Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.codec_id = CODEC_ID_H264;;
                    break;
                }
                case VENC_CODEC_H265:
                {
                    Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.codec_id = CODEC_ID_H265;;
                    break;
                }
                default:
                {
                    Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.codec_id = CODEC_ID_H264;;
                    break;
                }
            }
            Mpeg2tsMuxerCtx->v_strm_flag = 1;
            Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.codec_type = CODEC_TYPE_VIDEO;;
			Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.time_base.den = pMediaInf->mMediaVideoInfo[0].uVideoFrmRate;
	        Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.time_base.num = 1;
			Mpeg2tsMuxerCtx->nb_streams++;
		} 

		//set audio parameters
		if(pMediaInf->sample_rate && pMediaInf->channels) {
			Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.channels = pMediaInf->channels;
			Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.bits_per_sample = pMediaInf->bits_per_sample;
			Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.frame_size  = pMediaInf->frame_size;
			Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.sample_rate = pMediaInf->sample_rate;
			if(pMediaInf->audio_encode_type == 0) //for aac 
			{
				Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.codec_id = CODEC_ID_AAC;
			}
			else
			{
				Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.codec_id = CODEC_ID_ADPCM;			
			} 

            
            Mpeg2tsMuxerCtx->a_strm_flag = 1;
            Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.codec_type = CODEC_TYPE_AUDIO;         
			Mpeg2tsMuxerCtx->nb_streams++;

			alogd("pMediaInf->sample_rate: %d\n", pMediaInf->sample_rate);
		}

        if(0==pMediaInf->text_encode_type && pMediaInf->geo_available)   // gps info enabled
        { 
            Mpeg2tsMuxerCtx->t_strm_flag = 1;
            Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.codec_id = CODEC_ID_GGAD; 
            Mpeg2tsMuxerCtx->streams[Mpeg2tsMuxerCtx->nb_streams]->codec.codec_type = CODEC_TYPE_DATA;         
			Mpeg2tsMuxerCtx->nb_streams++;
        }

		service = ts->services[0];
        Mpeg2tsMuxerCtx->mux_rate = 1;
        for(i = 0;i < Mpeg2tsMuxerCtx->nb_streams; i++)
        {
            st = Mpeg2tsMuxerCtx->streams[i];
            ts_st = st->priv_data; 
            
            if( st->codec.codec_type == CODEC_TYPE_VIDEO)
            {
                ts_st->pid = 0x1011;
            }
            else if(st->codec.codec_type == CODEC_TYPE_AUDIO)
            {
                ts_st->pid = 0x1100;
            }
            else if(st->codec.codec_type == CODEC_TYPE_DATA)
            {
                ts_st->pid = 0x300;         // to store gps data
            }
            if (st->codec.codec_type == CODEC_TYPE_VIDEO && service->pcr_pid == 0x1fff)
            {
                //service->pcr_pid = ts_st->pid;
                service->pcr_pid = 0x1fff;
                pcr_st = st;
            }
        }

        if (service->pcr_pid == 0x1fff && Mpeg2tsMuxerCtx->nb_streams > 0)
        {
            pcr_st = Mpeg2tsMuxerCtx->streams[0];
            ts_st = pcr_st->priv_data;
            service->pcr_pid = ts_st->pid;
		    alogv("service->pcr_pid: %x", service->pcr_pid);
        }

		service->pcr_pid = 0x1000; //fix it later

		ts->mux_rate = Mpeg2tsMuxerCtx->mux_rate ? Mpeg2tsMuxerCtx->mux_rate : 1;

        if (ts->mux_rate > 1)
        {
            service->pcr_packet_period = (ts->mux_rate * PCR_RETRANS_TIME) /
                (TS_PACKET_SIZE * 8 * 1000);
            ts->sdt_packet_period      = (ts->mux_rate * SDT_RETRANS_TIME) /
                (TS_PACKET_SIZE * 8 * 1000);
            ts->pat_packet_period      = (ts->mux_rate * PAT_RETRANS_TIME) /
                (TS_PACKET_SIZE * 8 * 1000);

            ts->cur_pcr = av_rescale_rnd(Mpeg2tsMuxerCtx->max_delay, 90000, AV_TIME_BASE);
        }
        else
        {
            /* Arbitrary values, PAT/PMT could be written on key frames */
            ts->sdt_packet_period = 200;
            ts->pat_packet_period = 3;
            if (pcr_st->codec.codec_type == CODEC_TYPE_AUDIO)
            {
                if (!pcr_st->codec.frame_size)
                {
                    alogd("frame size not set\n");
                    service->pcr_packet_period =
                        pcr_st->codec.sample_rate/(10*512);
                }
                else
                {
                    service->pcr_packet_period = pcr_st->codec.sample_rate/(10*pcr_st->codec.frame_size);
                }
            }
            else
            {
                // max delta PCR 0.1s
                service->pcr_packet_period =
                    pcr_st->codec.time_base.den/(1000*10*pcr_st->codec.time_base.num);
            }
        }

        // output a PCR as soon as possible
        service->pcr_packet_count = service->pcr_packet_period;
        ts->pat_packet_count = ts->pat_packet_period;
        ts->sdt_packet_count = ts->sdt_packet_period;

		break;	
	case SETTOTALTIME:
		break;
    case SETFALLOCATELEN:
        Mpeg2tsMuxerCtx->mFallocateLen = uParam;
        break;
	case SETCACHEFD:            // path case
    {
		//Mpeg2tsMuxerCtx->pb_cache = (FILE*)uParam;
        CedarXDataSourceDesc datasourceDesc;
        memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
        datasourceDesc.source_url = (char*)pParam2;
        datasourceDesc.source_type = CEDARX_SOURCE_FILEPATH;
        datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
        Mpeg2tsMuxerCtx->pb_cache = create_outstream_handle(&datasourceDesc);
        if(NULL == Mpeg2tsMuxerCtx->pb_cache)
        {
            aloge("fatal error! create ts outstream fail.");
            return -1;
        }
		
		Mpeg2tsMuxerCtx->output_buffer_mode = OUTPUT_TS_FILE;
		if(Mpeg2tsMuxerCtx->output_buffer_mode == OUTPUT_M3U8_FILE) {
			sprintf((char*)Mpeg2tsMuxerCtx->filename, "%s%d.ts", TS_FILE_SAVED_PATH, Mpeg2tsMuxerCtx->current_segment);
//			Mpeg2tsMuxerCtx->pb_cache = fopen(Mpeg2tsMuxerCtx->filename, "wb");
//			if(Mpeg2tsMuxerCtx->pb_cache == NULL) {
//				alogd("open Mpeg2tsMuxer file error");
//			}
            CedarXDataSourceDesc datasourceDesc;
            memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
            datasourceDesc.source_url = (char*)Mpeg2tsMuxerCtx->filename;
            datasourceDesc.source_type = CEDARX_SOURCE_FILEPATH;
            datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
            Mpeg2tsMuxerCtx->pb_cache = create_outstream_handle(&datasourceDesc);
            if(NULL == Mpeg2tsMuxerCtx->pb_cache)
            {
                aloge("fatal error! create m3u8 outstream fail.");
                return -1;
            }
		}
		break;
	}
    case SETCACHEFD2:       // fd case, current case
    {
        CedarXDataSourceDesc datasourceDesc;
        memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
        datasourceDesc.ext_fd_desc.fd = (int)uParam;
        datasourceDesc.source_type = CEDARX_SOURCE_FD;
        datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
        Mpeg2tsMuxerCtx->pb_cache = create_outstream_handle(&datasourceDesc);
        if(NULL == Mpeg2tsMuxerCtx->pb_cache)
        {
            aloge("fatal error! create ts outstream fail.");
            return -1;
        }
        if(Mpeg2tsMuxerCtx->mFallocateLen > 0)
        {
            if(Mpeg2tsMuxerCtx->pb_cache->fallocate(Mpeg2tsMuxerCtx->pb_cache, 0x01, 0, Mpeg2tsMuxerCtx->mFallocateLen) < 0)
            {
                aloge("fatal error! Failed to fallocate size %d, fd[%d](%s)", Mpeg2tsMuxerCtx->mFallocateLen, Mpeg2tsMuxerCtx->pb_cache->fd_desc.fd, strerror(errno));
            }
        }
        Mpeg2tsMuxerCtx->output_buffer_mode = OUTPUT_TS_FILE;
        break;
    }
	case SETOUTURL:
		aloge("DO not support set URL");
		break;

	case REGISTER_WRITE_CALLBACK:

		alogd("m2ts REGISTER_WRITE_CALLBACK");
		Mpeg2tsMuxerCtx->output_buffer_mode = OUTPUT_CALLBACK_BUFFER;
		Mpeg2tsMuxerCtx->datasource_desc.source_url = (char*)pParam2;
		Mpeg2tsMuxerCtx->datasource_desc.source_type = CEDARX_SOURCE_WRITER_CALLBACK;
		if(Mpeg2tsMuxerCtx->OutStreamHandle == NULL) {
			Mpeg2tsMuxerCtx->OutStreamHandle = create_outstream_handle(&Mpeg2tsMuxerCtx->datasource_desc);
            if (NULL == Mpeg2tsMuxerCtx->OutStreamHandle) {
                aloge("fatal error! create callback outstream fail.");
                return -1;
            }
		}
		else {
			aloge("Mpeg2tsMuxerCtx->OutStreamHandle not NULL");
		}
		break;
    case SETCACHEMEM:           // used in cache thread
        Mpeg2tsMuxerCtx->mCacheMemInfo = *(FsCacheMemInfo*)pParam2;
        break;
    case SET_FS_WRITE_MODE:         // cache thread/simple cache/ direct
        Mpeg2tsMuxerCtx->mFsWriteMode = uParam;
        break;
    case SET_FS_SIMPLE_CACHE_SIZE:
        Mpeg2tsMuxerCtx->mFsSimpleCacheSize = (int32_t)uParam;
        break;
    case SET_STREAM_CALLBACK:
    {
        cdx_write_callback_t *callback = (cdx_write_callback_t*)pParam2;
        if (Mpeg2tsMuxerCtx->pb_cache != NULL) {
            Mpeg2tsMuxerCtx->pb_cache->callback.hComp = callback->hComp;
            Mpeg2tsMuxerCtx->pb_cache->callback.cb = callback->cb;
        } else {
            alogw("Mp4MuxerCtx->pb_cache not initialize!!");
        }
        break;
    }
	default:
		break;
	}

	return 0;
}

void *Mpeg2tsMuxerOpen(int *ret)
{
    AVFormatContext *Mpeg2tsMuxerCtx;
    MpegTSWrite *ts;
    MpegTSService *service;
    AVStream *st;
    MpegTSWriteStream *ts_st;
    int32_t i;
    *ret = 0;

    Mpeg2tsMuxerCtx = (AVFormatContext *)malloc(sizeof(AVFormatContext));
    if(!Mpeg2tsMuxerCtx) {
        *ret = -1;
        return NULL;
    }
	
    memset(Mpeg2tsMuxerCtx, 0, sizeof(AVFormatContext));
    Mpeg2tsMuxerCtx->cache_in_ts_stream = (uint8_t *)malloc(TS_PACKET_SIZE*1024);
	
    if(!Mpeg2tsMuxerCtx->cache_in_ts_stream) {
        aloge("malloc the cache_in_ts_stream is error!");
		 *ret = -1;
         goto _err1;
    }
	
    ts = (MpegTSWrite *)malloc(sizeof(MpegTSWrite));
    if(!ts) {
       *ret = -1;
       aloge("fatal error! malloc fail!");
       goto _err2;
    }

    ts->cur_pcr = 0;
    ts->mux_rate = 1;
    ts->nb_services = 1;
    ts->tsid = DEFAULT_TSID;
    ts->onid = DEFAULT_ONID;

    ts->sdt.pid = SDT_PID;
    ts->sdt.cc = 15; // Initialize at 15 so that it wraps and be equal to 0 for the first packet we write
    ts->sdt.write_packet = section_write_packet;
    ts->sdt.opaque = Mpeg2tsMuxerCtx;

    ts->pat.pid = PAT_PID;
    ts->pat.cc = 15;
    ts->pat.write_packet = section_write_packet;
    ts->pat.opaque = Mpeg2tsMuxerCtx;

    ts->sdt.pid = SDT_PID;
    ts->sdt.cc = 15;
    ts->sdt.write_packet = section_write_packet;
    ts->sdt.opaque = Mpeg2tsMuxerCtx;

    ts->ts_cache_start = Mpeg2tsMuxerCtx->cache_in_ts_stream;
    ts->ts_cache_end = Mpeg2tsMuxerCtx->cache_in_ts_stream + TS_PACKET_SIZE*1024 - 1;

    ts->ts_write_ptr = ts->ts_read_ptr = ts->ts_cache_start;
    ts->cache_size = 0;
    ts->cache_page_num = 0;
    ts->cache_size_total = 0;

    ts->services = NULL;
    ts->services = (MpegTSService**)malloc(MAX_SERVERVICES_IN_FILE*sizeof(MpegTSService*));
    if(!ts->services) {
        aloge("fatal error! malloc fail!");
        *ret = -1;
        Mpeg2tsMuxerCtx->priv_data = (void *)ts;
        goto _err3;
    }

	Mpeg2tsMuxerCtx->pes_buffer = (__u8*)malloc(512*1024);
    if(NULL == Mpeg2tsMuxerCtx->pes_buffer)
    {
        aloge("fatal_error:pes_buffer_malloc_failed");
        
        *ret = -1;
        Mpeg2tsMuxerCtx->priv_data = (void *)ts;
        goto _err3_1;
    }
	Mpeg2tsMuxerCtx->pes_bufsize = 512*1024;
    Mpeg2tsMuxerCtx->priv_data = (void *)ts;
    Mpeg2tsMuxerCtx->max_delay = 1;
    Mpeg2tsMuxerCtx->mux_rate = 1;
	Mpeg2tsMuxerCtx->keyframe_num = -1;
	Mpeg2tsMuxerCtx->current_segment = 0;
	Mpeg2tsMuxerCtx->sequence_num = 0;
	Mpeg2tsMuxerCtx->first_segment = 0;
	Mpeg2tsMuxerCtx->Video_frame_number = 0;
	Mpeg2tsMuxerCtx->first_ts = 1;
	Mpeg2tsMuxerCtx->OutStreamHandle = NULL;
	Mpeg2tsMuxerCtx->audio_frame_num = 0;
	Mpeg2tsMuxerCtx->output_buffer_mode = OUTPUT_TS_FILE;
	Mpeg2tsMuxerCtx->nb_streams = 0;
	Mpeg2tsMuxerCtx->pat_pmt_counter = 0;
	Mpeg2tsMuxerCtx->pat_pmt_flag = 1;
	Mpeg2tsMuxerCtx->first_video_pts = 1;
	Mpeg2tsMuxerCtx->base_video_pts = 0;
    Mpeg2tsMuxerCtx->first_audio_pts = 1;
    Mpeg2tsMuxerCtx->base_audio_pts = 0;
	Mpeg2tsMuxerCtx->pcr_counter = 0;

    for(i=0;i<MAX_SERVERVICES_IN_FILE;i++)
    {
        service = (MpegTSService*)malloc(sizeof(MpegTSService));
        if(!service) {
            aloge("fatal error! malloc fail!");
            *ret = -1;
            goto _err4;
        }
		
        memset(service, 0, sizeof(MpegTSService));
        service->pmt.write_packet = section_write_packet;
        service->pmt.opaque = Mpeg2tsMuxerCtx;
        service->pmt.cc = 15;
        service->pcr_pid = 0x1fff;
        service->sid = 0x00000001;
		
        //service->pmt.pid = DEFAULT_PMT_START_PID + ts->nb_services - 1;
		service->pmt.pid = 0x0100;
        ts->services[i] = service;
    }

    for(i=0;i<MAX_STREAMS_IN_FILE;i++)
    {
        st = (AVStream *)malloc(sizeof(AVStream));
        if(!st) {
            aloge("fatal error! malloc fail!");
            *ret = -1;
            goto _err5;
        }
        memset(st, 0, sizeof(AVStream));

        Mpeg2tsMuxerCtx->streams[i] = st;

		Mpeg2tsMuxerCtx->streams[i]->firstframeflag = 1;
        ts_st = (MpegTSWriteStream *)malloc(sizeof(MpegTSWriteStream));
        if(!ts_st) {
            aloge("fatal error! malloc fail!");
            *ret = -1;
            goto _err5;
        }
        st->priv_data = ts_st;
        ts_st->service = ts->services[0];
        ts_st->pid = DEFAULT_START_PID + i;

		alogv("ts_st->pid: %x", ts_st->pid);
        ts_st->payload_pts = -1;
        ts_st->payload_dts = -1;
        ts_st->first_pts_check = 1;
        ts_st->cc = 15;

        if(0 == i)
        {
            st->codec.codec_type = CODEC_TYPE_VIDEO;
        }
        else if(1 == i)
        {
            st->codec.codec_type = CODEC_TYPE_AUDIO; 
        }
        else if(2 ==i)
        {
            st->codec.codec_type = CODEC_TYPE_DATA; 
        }
        
//        st->codec.codec_type = (i==0) ? CODEC_TYPE_VIDEO : CODEC_TYPE_AUDIO;

		if( st->codec.codec_type == CODEC_TYPE_VIDEO)
		{
			ts_st->pid = 0x1011;
		}
		else if(st->codec.codec_type == CODEC_TYPE_AUDIO)
		{
			ts_st->pid = 0x1100;
		}
        else if(st->codec.codec_type == CODEC_TYPE_DATA)
        {
            ts_st->pid = 0x300;         // to store gps data
        }
    }

	return (void*)Mpeg2tsMuxerCtx;
_err5:
    {
        for(i=0;i<MAX_STREAMS_IN_FILE;i++)
        {
            if(Mpeg2tsMuxerCtx->streams[i])
            {
                if(Mpeg2tsMuxerCtx->streams[i]->priv_data)
                {
                    free(Mpeg2tsMuxerCtx->streams[i]->priv_data);
                    Mpeg2tsMuxerCtx->streams[i]->priv_data = NULL;
                }
                free(Mpeg2tsMuxerCtx->streams[i]);
                Mpeg2tsMuxerCtx->streams[i] = NULL;
            }
        }
    }
_err4:
    {
        MpegTSWrite *ts = Mpeg2tsMuxerCtx->priv_data;
        for(i=0; i<MAX_SERVERVICES_IN_FILE; i++)
        {
            if(ts->services[i])
            {
                free(ts->services[i]);
                ts->services[i] = NULL;
            }
        }
        free(ts->services);
        ts->services = NULL;

        if(NULL != Mpeg2tsMuxerCtx->pes_buffer)
        {
            free(Mpeg2tsMuxerCtx->pes_buffer);
            Mpeg2tsMuxerCtx->pes_buffer = NULL;
        }
    }
_err3_1: 
    if(NULL != ts->services)
    {
        free(ts->services);
        ts->services = NULL;
    }
_err3:
    free(Mpeg2tsMuxerCtx->priv_data);
    Mpeg2tsMuxerCtx->priv_data = NULL;
_err2:
    free(Mpeg2tsMuxerCtx->cache_in_ts_stream);
    Mpeg2tsMuxerCtx->cache_in_ts_stream = NULL;
_err1:
    free(Mpeg2tsMuxerCtx);
    Mpeg2tsMuxerCtx = NULL;
    return NULL;

}

int32_t Mpeg2tsMuxerClose(void *handle)
{
	AVFormatContext *Mpeg2tsMuxerCtx = (AVFormatContext *)handle;
	MpegTSWrite *ts = Mpeg2tsMuxerCtx->priv_data;
	AVStream *st;
    int32_t i;

    aloge("ts_muxer_close:a_cnt:%lld,v_cnt:%lld",Mpeg2tsMuxerCtx->audio_frame_num,Mpeg2tsMuxerCtx->Video_frame_number);

    for(i=0;i<MAX_STREAMS_IN_FILE;i++)
    {
        st = Mpeg2tsMuxerCtx->streams[i];
        if(st)
        {
            if(NULL != st->vosData)
            {
                free(st->vosData);
                st->vosData = NULL;
            }
        	if(st->priv_data)
    		{
    			free(st->priv_data);
				st->priv_data = NULL;
    		}
            free(st);
            st = NULL;
        }
    }


    if(NULL != ts->services)
    {
        for(i=0; i<MAX_SERVERVICES_IN_FILE; i++)
        {
            if(ts->services[i])
            {
                free(ts->services[i]);
                ts->services[i] = NULL;
            }
        }
        free(ts->services);
        ts->services = NULL; 
    }


    if(Mpeg2tsMuxerCtx->cache_in_ts_stream)
    {
        free(Mpeg2tsMuxerCtx->cache_in_ts_stream);
        Mpeg2tsMuxerCtx->cache_in_ts_stream = NULL;
    }
    if(Mpeg2tsMuxerCtx->priv_data)
    {
        free(Mpeg2tsMuxerCtx->priv_data);
        Mpeg2tsMuxerCtx->priv_data = NULL;
    }

    if(Mpeg2tsMuxerCtx->mpFsWriter)
    {
        destroyFsWriter(Mpeg2tsMuxerCtx->mpFsWriter);
        Mpeg2tsMuxerCtx->mpFsWriter = NULL;
    }
	if(Mpeg2tsMuxerCtx->output_buffer_mode == OUTPUT_TS_FILE)
    {
		if(Mpeg2tsMuxerCtx->pb_cache)
		{
		    //fclose(Mpeg2tsMuxerCtx->pb_cache);
		    destroy_outstream_handle(Mpeg2tsMuxerCtx->pb_cache);
			Mpeg2tsMuxerCtx->pb_cache = NULL;
		}
	}
    
	if(Mpeg2tsMuxerCtx->pes_buffer) {
		free(Mpeg2tsMuxerCtx->pes_buffer);
		Mpeg2tsMuxerCtx->pes_buffer = NULL;
	}

	if(Mpeg2tsMuxerCtx->OutStreamHandle != NULL)
    {
		destroy_outstream_handle(Mpeg2tsMuxerCtx->OutStreamHandle);
		Mpeg2tsMuxerCtx->OutStreamHandle = NULL;
	}

    if(Mpeg2tsMuxerCtx)
    {
        free(Mpeg2tsMuxerCtx);
        Mpeg2tsMuxerCtx = NULL;
    }

	return 0;
}

CDX_RecordWriter record_writer_mpeg2ts = {
	.info 				 = "recode write Mpeg2ts"   ,
	.MuxerOpen           = Mpeg2tsMuxerOpen         ,
	.MuxerClose          = Mpeg2tsMuxerClose        ,
	.MuxerWriteExtraData = Mpeg2tsMuxerWriteVos     ,
	.MuxerWriteHeader    = Mpeg2tsMuxerWriteHeader  ,
	.MuxerWriteTrailer   = Mpeg2tsMuxerWriteTrailer ,
	.MuxerWritePacket    = Mpeg2tsMuxerWritePacket  ,
	.MuxerIoctrl         = Mpeg2tsMuxerIoctrl       ,
};
