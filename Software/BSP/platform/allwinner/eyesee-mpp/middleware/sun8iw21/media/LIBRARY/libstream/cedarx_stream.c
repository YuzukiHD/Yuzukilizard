/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "cedarx_stream"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "cedarx_stream.h"
#include "cedarx_stream_file.h"
//#include "cedarx_stream_drm_file.h"
//#include <thirdpart_stream.h>
//#include "awdrm.h"

//extern int  mplayer_create_stream_handle(CedarXDataSourceDesc *datasource_desc, struct cdx_stream_info *stm_info);
//extern void mplayer_destory_stream_handle();
//extern int  sft_httplive_create_stream_handle(CedarXDataSourceDesc *datasource_desc, struct cdx_stream_info *stm_info);
//extern void sft_httplive_destory_stream_handle(struct cdx_stream_info *stm_info);
//extern int  sft_http_create_stream_handle(CedarXDataSourceDesc *datasource_desc, struct cdx_stream_info *stm_info);
//extern void sft_http_destory_stream_handle(struct cdx_stream_info *stream);
//extern int sft_rtsp_create_stream_handle(CedarXDataSourceDesc *datasource_desc, struct cdx_stream_info *stream);
//extern void sft_rtsp_destory_stream_handle(struct cdx_stream_info * stm_info);
//extern int  sft_streaming_create_stream_handle(CedarXDataSourceDesc *datasource_desc, struct cdx_stream_info *stm_info);
//extern void sft_streaming_destory_stream_handle(struct cdx_stream_info * stm_info);
//extern int  sft_wifi_display_create_stream_handle(CedarXDataSourceDesc *datasource_desc, struct cdx_stream_info *stm_info);
//extern void sft_wifi_display_destory_stream_handle(struct cdx_stream_info * stm_info);

struct cdx_stream_info *create_stream_handle(CedarXDataSourceDesc *datasource_desc)
{
	int ret = 0;
	struct cdx_stream_info *stm_info;
	stm_info = (struct cdx_stream_info *)malloc(sizeof(struct cdx_stream_info));
	assert(stm_info != NULL);
	memset(stm_info, 0, sizeof(struct cdx_stream_info));
	//Must init fd as -1, it is valid if we get a fd as 0.
	stm_info->fd_desc.fd  = -1;
	memcpy(&stm_info->data_src_desc, datasource_desc, sizeof(CedarXDataSourceDesc));
	//stm_info->another_data_src_desc = datasource_desc;
    
	if((datasource_desc->source_type == CEDARX_SOURCE_FILEPATH || datasource_desc->source_type == CEDARX_SOURCE_FD)
        && datasource_desc->stream_type == CEDARX_STREAM_LOCALFILE)
    {
        ret = file_create_instream_handle(datasource_desc, stm_info);
    }
    else
    {
        aloge("fatal error! source_type[%d] stream_type[%d] wrong!", datasource_desc->source_type, datasource_desc->stream_type);
        ret = -1;
    }

	if(ret < 0) 
    {
		//Must destory here, parser cannot destroy stream handle
		//when it holds a NULL stream info handle.
		aloge("create_stream_handle failed");
        //stm info have been freed in destory_stream_handle.
        //but stm_info is not NULL as it was passed by value.
        free(stm_info);
        stm_info = NULL;
	}
//_Exit:
	return stm_info;
}

void destory_stream_handle(struct cdx_stream_info *stm_info)
{
	if(!stm_info){
		return;
	}
    stm_info->destory(stm_info);
    free(stm_info);
    //stm_info = NULL;
}

