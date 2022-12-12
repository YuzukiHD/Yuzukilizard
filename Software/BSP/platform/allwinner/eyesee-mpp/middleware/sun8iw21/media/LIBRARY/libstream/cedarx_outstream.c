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
#define LOG_TAG "cedarx_outstream"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>
//#include <CDX_Common.h>
#include "cedarx_stream.h"
#include "cedarx_stream_file.h"

//extern int create_outstream_handle_http(struct cdx_stream_info *stm_info, CedarXDataSourceDesc *datasource_desc);
extern int create_outstream_handle_external(struct cdx_stream_info *stm_info, CedarXDataSourceDesc *datasource_desc);

struct cdx_stream_info *create_outstream_handle(CedarXDataSourceDesc *datasource_desc)
{
	int ret = 0;
	struct cdx_stream_info *stm_info;

	stm_info = (struct cdx_stream_info *)malloc(sizeof(struct cdx_stream_info));
	memset(stm_info,0,sizeof(struct cdx_stream_info));

	datasource_desc->stream_type = CEDARX_STREAM_LOCALFILE;
	if (datasource_desc->source_type == CEDARX_SOURCE_FILEPATH) {
		if(!strncasecmp("http://", datasource_desc->source_url, 7))
			datasource_desc->stream_type = CEDARX_STREAM_NETWORK;
	}

	if (datasource_desc->source_type == CEDARX_SOURCE_WRITER_CALLBACK) {
		ret = create_outstream_handle_external(stm_info, datasource_desc);
	}
	else if (datasource_desc->stream_type == CEDARX_STREAM_NETWORK) {
		//ret = create_outstream_handle_http(stm_info, datasource_desc);
	}
	else
		ret = create_outstream_handle_file(stm_info, datasource_desc);

    if(ret!=0)
    {
        aloge("(f:%s, l:%d) fatal error! create stream fail[%d]!", __FUNCTION__, __LINE__, ret);
        free(stm_info);
        stm_info = NULL;
    }

//_Exit:
	return stm_info;
}

void destroy_outstream_handle(struct cdx_stream_info *stm_info)
{
	if(!stm_info){
		return;
	}

	stm_info->destory(stm_info);

	free(stm_info);
//	stm_info = NULL;
}

