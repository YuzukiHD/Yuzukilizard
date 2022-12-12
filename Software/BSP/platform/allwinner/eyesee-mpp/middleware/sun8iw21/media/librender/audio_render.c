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
#define LOG_TAG "audio_render"
#include <CDX_Debug.h>

#include <CDX_Types.h>

#include "audio_render.h"

#ifdef __OS_ANDROID
extern CDX_AudioRenderHAL audio_render_stagefright_hal;
#else
extern CDX_AudioRenderHAL audio_render_alsa_hal;
#endif

CDX_AudioRenderHAL* cedarx_audio_render[] =
{
#ifdef __OS_ANDROID
	&audio_render_stagefright_hal,
#else
	&audio_render_alsa_hal,
#endif
	0
};

CDX_AudioRenderHAL *cedarx_audio_render_create(void *callback_info)
{
	CDX_AudioRenderHAL *cedarx_audio_render_handle;

	cedarx_audio_render_handle = (CDX_AudioRenderHAL *)malloc(sizeof(CDX_AudioRenderHAL));
	if(cedarx_audio_render_handle == NULL)
		return NULL;

	memcpy(cedarx_audio_render_handle, cedarx_audio_render[0], sizeof(CDX_AudioRenderHAL));

	cedarx_audio_render_handle->callback_info = callback_info;

	return cedarx_audio_render_handle;
}

void cedarx_audio_render_destroy(void *handle)
{
	if (handle != NULL) {
		free(handle);
		//handle = NULL;
	}
}
