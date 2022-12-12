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
#define LOG_TAG "video_render"
#include <utils/plat_log.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "video_render.h"

#ifdef __OS_ANDROID
extern CDX_VideoRenderHAL video_render_stagefright_hal;
#else
extern CDX_VideoRenderHAL video_render_linux_hal;
#endif

CDX_VideoRenderHAL* cedarx_video_render[] =
{
#ifdef __OS_ANDROID
	&video_render_stagefright_hal,
#else
	&video_render_linux_hal,
#endif
	0
};

CDX_VideoRenderHAL *cedarx_video_render_create(void *callback_info)
{
	CDX_VideoRenderHAL *cedarx_video_render_handle;

	cedarx_video_render_handle = (CDX_VideoRenderHAL *)malloc(sizeof(CDX_VideoRenderHAL));
	if(cedarx_video_render_handle == NULL)
		return NULL;

	memcpy(cedarx_video_render_handle, cedarx_video_render[0], sizeof(CDX_VideoRenderHAL));

	cedarx_video_render_handle->callback_info = callback_info;

	return cedarx_video_render_handle;
}

void cedarx_video_render_destroy(void *handle)
{
	if (handle != NULL) {
		free(handle);
		//handle = NULL;
	}
}
