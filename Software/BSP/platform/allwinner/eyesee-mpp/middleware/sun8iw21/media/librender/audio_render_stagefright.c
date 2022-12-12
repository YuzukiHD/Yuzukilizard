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
#define LOG_TAG "audio_render_stagefright"
#include <CDX_Debug.h>

#include <CDX_Types.h>
#include "audio_render.h"
//#include <CDX_PlayerAPI.h>
#include <CDX_CallbackType.h>
#include <CdxAudioOut.h>

static int ar4sft_init(struct CDX_AudioRenderHAL *handle, int rate_hz, int channels, int format)
{
	int para[3];
	para[0] = rate_hz;
	para[1] = channels;
	para[2] = format;
	//LOGV("ar4sft_init:%p %p",cdx_ctx->cookie_server,cdx_ctx->cdx_callback);
	((CedarXPlayerCallbackType*)(handle->callback_info))->callback(((CedarXPlayerCallbackType*)(handle->callback_info))->cookie,CDX_EVENT_AUDIORENDERINIT, para);
	return 1;
}

static void ar4sft_exit(struct CDX_AudioRenderHAL *handle, int immed)
{
	((CedarXPlayerCallbackType*)(handle->callback_info))->callback(((CedarXPlayerCallbackType*)(handle->callback_info))->cookie,CDX_EVENT_AUDIORENDEREXIT, NULL);
}

static int ar4sft_play(struct CDX_AudioRenderHAL *handle, void* data, int len)
{
//	int para[3];
//	para[0] = (int)data;
//	para[1] = len;
    CdxARRenderDataPara para;
    para.data = data;
    para.len = len;
	return ((CedarXPlayerCallbackType*)(handle->callback_info))->callback(((CedarXPlayerCallbackType*)(handle->callback_info))->cookie,CDX_EVENT_AUDIORENDERDATA, &para);
}

static int ar4sft_get_space(struct CDX_AudioRenderHAL *handle)
{
	return ((CedarXPlayerCallbackType*)(handle->callback_info))->callback(((CedarXPlayerCallbackType*)(handle->callback_info))->cookie,CDX_EVENT_AUDIORENDERGETSPACE, NULL);
}

static int ar4sft_get_delay(struct CDX_AudioRenderHAL *handle)
{
	return ((CedarXPlayerCallbackType*)(handle->callback_info))->callback(((CedarXPlayerCallbackType*)(handle->callback_info))->cookie,CDX_EVENT_AUDIORENDERGETDELAY, NULL);
}

static int ar4sft_flush_cache(struct CDX_AudioRenderHAL *handle)
{
	return ((CedarXPlayerCallbackType*)(handle->callback_info))->callback(((CedarXPlayerCallbackType*)(handle->callback_info))->cookie,CDX_EVENT_AUDIORENDERFLUSHCACHE, NULL);
}

static int ar4sft_pause(struct CDX_AudioRenderHAL *handle)
{
	return ((CedarXPlayerCallbackType*)(handle->callback_info))->callback(((CedarXPlayerCallbackType*)(handle->callback_info))->cookie,CDX_EVENT_AUDIORENDERPAUSE, NULL);
}

CDX_AudioRenderHAL audio_render_stagefright_hal = {
	.info = "audio stagefright render",
	.init = ar4sft_init,
	.exit = ar4sft_exit,
	.render = ar4sft_play,
	.get_space = ar4sft_get_space,
	.get_delay = ar4sft_get_delay,
	.flush_cache = ar4sft_flush_cache,
	.pause = ar4sft_pause,
};
