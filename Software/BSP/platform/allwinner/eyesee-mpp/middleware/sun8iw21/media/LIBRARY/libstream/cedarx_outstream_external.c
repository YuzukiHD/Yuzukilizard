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

#define LOG_NDEBUG 0
#define LOG_TAG "cedarx_outstream_external"
#include <utils/plat_log.h>

#include "cedarx_stream.h"
//#include <CDX_Recorder.h>
//#include <tsemaphore.h>
#include <errno.h>

static int cdx_write_stream_external(const void *ptr, size_t size, size_t nmemb, struct cdx_stream_info *stm_info)
{
	CDXRecorderBsInfo bs_info;

	bs_info.bs_count = 1;
	bs_info.bs_data[0] = (char *)ptr;
	bs_info.total_size = bs_info.bs_size[0] = size * nmemb;
    bs_info.mode = 2;
	return stm_info->extern_writer(stm_info->file_handle, &bs_info);
}

static int cdx_write_stream_external2(void *bs_info, struct cdx_stream_info *stm_info)
{
	return stm_info->extern_writer(stm_info->file_handle, bs_info);
}

static void destory_outstream_handle_external(struct cdx_stream_info *stm_info)
{
	//return 0;
}

int create_outstream_handle_external(struct cdx_stream_info *stm_info, CedarXDataSourceDesc *datasource_desc)
{
	//alogd("url:%s fd_desc.fd:%d",datasource_desc->source_url,stm_info->fd_desc.fd);

	CdxRecorderWriterCallbackInfo *callback= (CdxRecorderWriterCallbackInfo*)datasource_desc->source_url;

	stm_info->file_handle = callback->parent;
	stm_info->extern_writer = callback->writer;

	stm_info->write = cdx_write_stream_external;
	stm_info->write2 = cdx_write_stream_external2;
	stm_info->destory = destory_outstream_handle_external;

	return 0;
}

