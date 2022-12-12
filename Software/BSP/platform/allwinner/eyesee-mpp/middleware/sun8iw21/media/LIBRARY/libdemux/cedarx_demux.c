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
#define LOG_TAG "cedarx_demux"
#include <utils/plat_log.h>


#include "cedarx_demux.h"
#include "aw_demux.h"

CedarXDemuxerAPI *cedarx_demux_create(CEDARX_SOURCETYPE nSourceType)
{
    CedarXDemuxerAPI *cedarx_demuxer_handle = malloc(sizeof(CedarXDemuxerAPI));

    alogd("cedarx_demux_create: sourceType[0x%x]", nSourceType);
    memcpy(cedarx_demuxer_handle, &cdx_dmxs_aw, sizeof(CedarXDemuxerAPI));
    AwDemuxInfo *pPrivInfo = (AwDemuxInfo*)malloc(sizeof(AwDemuxInfo));
    memset(pPrivInfo, 0, sizeof(AwDemuxInfo));
    pPrivInfo->mSourceType = nSourceType;

    cedarx_demuxer_handle->reserved_usr_0 = (void*)pPrivInfo;
    return cedarx_demuxer_handle;
}

void cedarx_demux_destory(CedarXDemuxerAPI *handle)
{
    if(0 == strcmp(handle->name, cdx_dmxs_aw.name))
    {
        AwDemuxInfo *pPrivInfo = (AwDemuxInfo*)handle->reserved_usr_0;
        free(pPrivInfo);
        handle->reserved_usr_0 = NULL;
    }

    if(handle != NULL)
    {
        free(handle);
        //handle = NULL;
    }
}

