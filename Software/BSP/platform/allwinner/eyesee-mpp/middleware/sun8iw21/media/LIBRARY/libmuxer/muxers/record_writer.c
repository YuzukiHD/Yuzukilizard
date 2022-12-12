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

///#define LOG_NDEBUG 0
#define LOG_TAG "record_writer"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>
#include <record_writer.h>
#include <encoder_type.h>

extern CDX_RecordWriter record_writer_mp4;
//extern CDX_RecordWriter record_writer_awts;
extern CDX_RecordWriter record_writer_raw;
extern CDX_RecordWriter record_writer_mpeg2ts;
extern CDX_RecordWriter record_writer_mp3;
extern CDX_RecordWriter record_writer_aac;

//ref to MUXERMODES
CDX_RecordWriter* cedarx_record_writer[] =
{
	&record_writer_mp4,
    &record_writer_mp3,
    &record_writer_aac,
#ifndef __OS_LINUX
	NULL,   //&record_writer_awts,
	&record_writer_raw,
	&record_writer_mpeg2ts,
#endif
	0
};

CDX_RecordWriter *cedarx_record_writer_create(int mode)
{
	CDX_RecordWriter *cedarx_record_writer_handle;

	cedarx_record_writer_handle = (CDX_RecordWriter *)malloc(sizeof(CDX_RecordWriter));
	if(cedarx_record_writer_handle == NULL)
		return NULL;

	memcpy(cedarx_record_writer_handle, cedarx_record_writer[mode], sizeof(CDX_RecordWriter));

	//cedarx_record_writer_handle->callback_info = callback_info;

	return cedarx_record_writer_handle;
}

void cedarx_record_writer_destroy(void *handle)
{
	if (handle != NULL) {
		free(handle);
//		handle = NULL;
	}
}
