//#define LOG_NDEBUG 0
#define LOG_TAG "RawMuxer"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <vencoder.h>
#include <aencoder.h>
#include <TextEncApi.h>

#include "RawMuxer.h"
#include <cedarx_stream.h>

int RawMuxerWriteVos(void *handle, unsigned char *vosData, unsigned int vosLen, unsigned int idx)
{
	RAWCONTEXT *RawMuxerCtx = (RAWCONTEXT *)handle;
	RAWTRACK *track = RawMuxerCtx->Track[idx];
	alogv("RawMuxerWriteVos idx:%d len:%d",idx,vosLen);
	if(vosLen)
	{
		track->CodecInfo.extradata = (unsigned char *)malloc(vosLen);
		track->CodecInfo.extradata_size  = vosLen;
        memcpy(track->CodecInfo.extradata,vosData,vosLen);
	}

	return 0;
}

int RawMuxerWriteHeader(void *handle)
{
	RAWCONTEXT *RawMuxerCtx = (RAWCONTEXT *)handle;
	CDXRecorderBsInfo bs_info;
	//offset_t pos_packet, pos_header, pos_atom;
	int i, entries, ret;
	//unsigned int val;

	entries = RawMuxerCtx->TrackCount;

    for (i=0; i<entries; i++)
    {
    	RAWTRACK *track= RawMuxerCtx->Track[i];

    	if (track->CodecInfo.extradata_size > 0) {
        	RawPacketHeader *pkt_hdr = &RawMuxerCtx->RawPacketHdr;
            pkt_hdr->mStreamId = i;
        	pkt_hdr->stream_type = track->CodecInfo.codec_type == CODEC_TYPE_VIDEO ? RawPacketTypeVideoExtra : RawPacketTypeAudioExtra; //StreamIndex
        	pkt_hdr->size = track->CodecInfo.extradata_size;
        	pkt_hdr->pts = 0;

        	bs_info.total_size = 0;

			bs_info.bs_count = 1;
			bs_info.bs_data[0] = (char*)pkt_hdr;
			bs_info.bs_size[0] = sizeof(RawPacketHeader);
			bs_info.total_size += bs_info.bs_size[0];


			bs_info.bs_count++;
			bs_info.bs_data[1] = (char *)track->CodecInfo.extradata;
			bs_info.bs_size[1] = track->CodecInfo.extradata_size;
			bs_info.total_size += bs_info.bs_size[1];

            bs_info.mode = 0;

        	ret = cdx_write2(&bs_info, RawMuxerCtx->OutStreamHandle);
			if(ret < 0) {
				return -1;
			}
        }
    }

	return 0;
}

int RawMuxerWriteTrailer(void *handle)
{
	//RAWCONTEXT *RawMuxerCtx = (RAWCONTEXT *)handle;
	return 0;
}

int RawMuxerWritePacket(void *handle, void *packet)
{
	RAWCONTEXT *RawMuxerCtx = (RAWCONTEXT *)handle;
	AVPacket *pkt = (AVPacket *)packet;
	CDXRecorderBsInfo bs_info;
	int ret;

    RAWTRACK *track= RawMuxerCtx->Track[pkt->stream_index];
    
	RawPacketHeader *pkt_hdr = &RawMuxerCtx->RawPacketHdr;
	//unsigned int val;
	//unsigned int idx = 0;

    pkt_hdr->mStreamId = pkt->stream_index;
    switch(track->CodecInfo.codec_type)
    {
        case CODEC_TYPE_VIDEO:
        {
            pkt_hdr->stream_type = RawPacketTypeVideo;
            break;
        }
        case CODEC_TYPE_AUDIO:
        {
            pkt_hdr->stream_type = RawPacketTypeAudio;
            break;
        }
        case CODEC_TYPE_TEXT:
        {
            pkt_hdr->stream_type = RawPacketTypeText;
            break;
        }
        default:
        {
            aloge("fatal error! unknown codec_type:%d", track->CodecInfo.codec_type);
            pkt_hdr->stream_type = RawPacketTypeAudio;
            break;
        }
    }
	pkt_hdr->size = pkt->size0 + pkt->size1;
	pkt_hdr->pts = pkt->pts;
    pkt_hdr->CurrQp = pkt->CurrQp;
    pkt_hdr->avQp = pkt->avQp;
	pkt_hdr->nGopIndex = pkt->nGopIndex;
	pkt_hdr->nFrameIndex = pkt->nFrameIndex;
	pkt_hdr->nTotalIndex = pkt->nTotalIndex;

	bs_info.total_size = 0;

	bs_info.bs_count = 1;
	bs_info.bs_data[0] = (char *)pkt_hdr;
	bs_info.bs_size[0] = sizeof(RawPacketHeader);
	bs_info.total_size += bs_info.bs_size[0];

	if (pkt->size0) {
		bs_info.bs_count++;
		bs_info.bs_data[1] = (char *)pkt->data0;
		bs_info.bs_size[1] = pkt->size0;
		bs_info.total_size += bs_info.bs_size[1];
	}

	if (pkt->size1) {
		bs_info.bs_count++;
		bs_info.bs_data[2] = (char *)pkt->data1;
		bs_info.bs_size[2] = pkt->size1;
		bs_info.total_size += bs_info.bs_size[2];
	}

    bs_info.mode = 0;

	ret = cdx_write2(&bs_info, RawMuxerCtx->OutStreamHandle);
	if(ret < 0) {
		return -1;
	}
	return 0;
}

int RawMuxerIoctrl(void *handle, unsigned int uCmd, unsigned int uParam, void *pParam2)
{
	RAWCONTEXT *RawMuxerCtx = (RAWCONTEXT *)handle;
	_media_file_inf_t *pMediaInf = 0;
	
	switch (uCmd)
	{
	case SETAVPARA:
	{
		pMediaInf = (_media_file_inf_t *)pParam2;
		
		if (NULL == pMediaInf)
		{
			aloge("error in param\n");
			return -1;
		}
		
        int i;
        for (i = 0; i < pMediaInf->mVideoInfoValidNum; ++i)
        {
            RawMuxerCtx->Track[RawMuxerCtx->TrackCount] = (RAWTRACK*)malloc(sizeof(RAWTRACK));
            if(RawMuxerCtx->Track[RawMuxerCtx->TrackCount] == NULL) {
                aloge("fatal error! malloc fail!");
                return -1;
            }
            memset(RawMuxerCtx->Track[RawMuxerCtx->TrackCount], 0, sizeof(RAWTRACK));
            RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->StreamIndex = RawMuxerCtx->TrackCount;
            RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.height = pMediaInf->mMediaVideoInfo[i].nHeight;
            RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.width  = pMediaInf->mMediaVideoInfo[i].nWidth;
            RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.frame_rate = pMediaInf->mMediaVideoInfo[i].uVideoFrmRate;
            switch(pMediaInf->mMediaVideoInfo[i].mVideoEncodeType)
            {
                case VENC_CODEC_JPEG:
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_MJPEG;
                    break;
                case VENC_CODEC_H264:
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_H264;
                    break;
                case VENC_CODEC_H265:
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_H265;
                    break;
                default:
                    aloge("fatal error! unknown video encode type[0x%x]", pMediaInf->mMediaVideoInfo[i].mVideoEncodeType);
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_H264;
                    break;
            }
            RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_type = CODEC_TYPE_VIDEO;
            //RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.rotate_degree = pMediaInf->rotate_degree; //set rotate degree
            RawMuxerCtx->TrackCount++;
        }

		//set audio parameters
		if(pMediaInf->sample_rate && pMediaInf->channels) {
			RawMuxerCtx->Track[RawMuxerCtx->TrackCount] = (RAWTRACK*)malloc(sizeof(RAWTRACK));
			if(RawMuxerCtx->Track[RawMuxerCtx->TrackCount] == NULL) {
				return -1;
			}
			memset(RawMuxerCtx->Track[RawMuxerCtx->TrackCount], 0, sizeof(RAWTRACK));
			RawMuxerCtx->AudioStreamIndex = RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->StreamIndex = RawMuxerCtx->TrackCount;

			RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.channels = pMediaInf->channels;
			RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.bits_per_sample = pMediaInf->bits_per_sample;
			RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.frame_size  = pMediaInf->frame_size;
			RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.sample_rate = pMediaInf->sample_rate;
            switch(pMediaInf->audio_encode_type)
            {
                case AUDIO_ENCODER_AAC_TYPE:
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_AAC;
                    break;
                case AUDIO_ENCODER_MP3_TYPE:
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_MP3;
                    break;
                case AUDIO_ENCODER_PCM_TYPE:
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_PCM;
                    break;
                default:
                    aloge("fatal error! unknown audio encode type[0x%x]", pMediaInf->audio_encode_type);
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_AAC;
                    break;
            }
			RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_type = CODEC_TYPE_AUDIO;
			RawMuxerCtx->TrackCount++;
		}

        //set text parameters, maybe gps text
        if(pMediaInf->geo_available)
        {
            RawMuxerCtx->Track[RawMuxerCtx->TrackCount] = (RAWTRACK*)malloc(sizeof(RAWTRACK));
            if(RawMuxerCtx->Track[RawMuxerCtx->TrackCount] == NULL)
            {
                aloge("fatal error! malloc fail!");
                return -1;
            }
            memset(RawMuxerCtx->Track[RawMuxerCtx->TrackCount], 0, sizeof(RAWTRACK));
            switch(pMediaInf->text_encode_type)
            {
                case TEXT_ENCODER_GGAD:
                {
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_GGAD;
                    break;
                }
                default:
                {
                    aloge("fatal error! unknown text encode type[0x%x]", pMediaInf->text_encode_type);
                    RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_id = CODEC_ID_GGAD;
                    break;
                }
            }
            RawMuxerCtx->Track[RawMuxerCtx->TrackCount]->CodecInfo.codec_type = CODEC_TYPE_TEXT;
            RawMuxerCtx->TrackCount++;
        }
		break;	
    }
	case SETTOTALTIME: //ms
		//mov->tracks[0].trackDuration = (unsigned int)(((CDX_U64)uParam * mov->tracks[0].timescale) / 1000);
		//mov->tracks[1].trackDuration = (unsigned int)(((CDX_U64)uParam * mov->tracks[1].timescale) / 1000);
		break;
    case SETFALLOCATELEN:
        aloge("fatal error, not support SETFALLOCATELEN");
        break;
	case SETCACHEFD2:
		RawMuxerCtx->datasource_desc.ext_fd_desc.fd = uParam;
		RawMuxerCtx->datasource_desc.source_type = CEDARX_SOURCE_FD;
		if(RawMuxerCtx->OutStreamHandle == NULL) {
			RawMuxerCtx->OutStreamHandle = create_outstream_handle(&RawMuxerCtx->datasource_desc);
            if (NULL == RawMuxerCtx->OutStreamHandle)
            {
                aloge("SET CACHEFD2 create_outstream_handle error.");
                return -1;
            }
		}
		else {
			aloge("RawMuxerCtx->OutStreamHandle not NULL");
		}
		break;

	case SETOUTURL:
		RawMuxerCtx->datasource_desc.source_url = (char*)pParam2;
		RawMuxerCtx->datasource_desc.source_type = CEDARX_SOURCE_FILEPATH;
		if(RawMuxerCtx->OutStreamHandle == NULL) {
			RawMuxerCtx->OutStreamHandle = create_outstream_handle(&RawMuxerCtx->datasource_desc);
            if (NULL == RawMuxerCtx->OutStreamHandle)
            {
                aloge("SETOUTURL create_outstream_handle error.");
                return -1;
            }
		}
		else {
			aloge("RawMuxerCtx->OutStreamHandle not NULL");
		}
		break;

	case REGISTER_WRITE_CALLBACK:
		RawMuxerCtx->datasource_desc.source_url = (char*)pParam2;
		RawMuxerCtx->datasource_desc.source_type = CEDARX_SOURCE_WRITER_CALLBACK;
		if(RawMuxerCtx->OutStreamHandle == NULL) {
			RawMuxerCtx->OutStreamHandle = create_outstream_handle(&RawMuxerCtx->datasource_desc);
            if (NULL == RawMuxerCtx->OutStreamHandle)
            {
                aloge("REGISTER_WRITE_CALLBACK create_outstream_handle error.");
                return -1;
            }
		}
		else {
			aloge("RawMuxerCtx->OutStreamHandle not NULL");
		}
		break;

	default:
		break;
	}

	return 0;
}

void *RawMuxerOpen(int *ret)
{
	RAWCONTEXT *RawMuxerCtx;

	*ret = 0;
		
	RawMuxerCtx = (RAWCONTEXT *)malloc(sizeof(RAWCONTEXT));
	if(!RawMuxerCtx)
	{	
		*ret = -1;
		return NULL;
	}
	memset(RawMuxerCtx, 0, sizeof(RAWCONTEXT));

	return (void*)RawMuxerCtx;
}

int RawMuxerClose(void *handle)
{
	RAWCONTEXT *RawMuxerCtx = (RAWCONTEXT *)handle;
	int i;
	
	if(RawMuxerCtx)
	{
		for(i=0; i<RawMuxerCtx->TrackCount; i++) {
			if(RawMuxerCtx->Track[i]->CodecInfo.extradata) {
				free(RawMuxerCtx->Track[i]->CodecInfo.extradata);
			}
			free(RawMuxerCtx->Track[i]);
			RawMuxerCtx->Track[i] = NULL;
		}

		if(RawMuxerCtx->OutStreamHandle != NULL) {
			destroy_outstream_handle(RawMuxerCtx->OutStreamHandle);
			RawMuxerCtx->OutStreamHandle = NULL;
		}

		free(RawMuxerCtx);
		RawMuxerCtx = NULL;
	}
	
	return 0;
}

CDX_RecordWriter record_writer_raw = {
	.info 				 = "recode write Raw"   ,
	.MuxerOpen           = RawMuxerOpen         ,
	.MuxerClose          = RawMuxerClose        ,
	.MuxerWriteExtraData = RawMuxerWriteVos     ,
	.MuxerWriteHeader    = RawMuxerWriteHeader  ,
	.MuxerWriteTrailer   = RawMuxerWriteTrailer ,
	.MuxerWritePacket    = RawMuxerWritePacket  ,
	.MuxerIoctrl         = RawMuxerIoctrl       ,
};
