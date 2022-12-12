/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : ComponentsRegistryTable.c.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/03/15
  Last Modified :
  Description   : components registry table.
  Function List :
  History       :
******************************************************************************/
//ref platform headers
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//media api headers to app

//media internal common headers.
#include "mm_component.h"
#include <VideoDec_Component.h>
#include <VideoRender_Component.h>
#include <AudioDec_Component.h>
#include <Clock_Component.h>
#include "VideoEnc_Component.h"
#include "RecRender_Component.h"
#include "AudioEnc_Component.h"
#include "TextEnc_Component.h"

#include "AIChannel_Component.h"
#include "AOChannel_Component.h"
#include "VideoVirVi_Component.h"
#include "VideoISE_Component.h"
#include "VideoEIS_Component.h"
#include "Demux_Component.h"
//#include "UvcInput_Component.h"
#include "UvcVirVi_Component.h"

#include <ConfigOption.h>

CDX_COMPONENTREGISTRY cdx_comp_table[] =
{
#if (MPPCFG_DEMUXER == OPTION_DEMUXER_ENABLE)
    {
        CDX_ComponentNameDemux,
        DemuxComponentInit
    },
#endif
#if (MPPCFG_VDEC == OPTION_VDEC_ENABLE)
    {
        CDX_ComponentNameVideoDecoder,
        VideoDecComponentInit
    },
#endif
#if (MPPCFG_ADEC == OPTION_ADEC_ENABLE)
    {
        CDX_ComponentNameAudioDecoder,
        AudioDecComponentInit
    },
#endif
/*
    {
        CDX_ComponentNameSubtitleDecoder,
        SubtitleComponentInit
    },
*/
#if (MPPCFG_VO == OPTION_VO_ENABLE)
    {
        CDX_ComponentNameVideoRender,
        VideoRenderComponentInit
    },
#endif
/*
    {
        CDX_ComponentNameAudioRender,
        AudioRenderComponentInit
    },

    {
        CDX_ComponentNameSubtitleRender,
        SubtitleRenderComponentInit
    },
*/
    {
        CDX_ComponentNameClock,
        ClockComponentInit
    },
/*
    {
        CDX_ComponentNameVideoSource,
        VideoSourceComponentInit
    },

    {
        CDX_ComponentNameAudioSource,
        AudioSourceComponentInit
    },

    {
        CDX_ComponentNameTextSource,
        TextSourceComponentInit
    },
*/
#if (MPPCFG_VENC == OPTION_VENC_ENABLE)
    {
        CDX_ComponentNameVideoEncoder,
        VideoEncComponentInit
    },
#endif
#if (MPPCFG_AENC == OPTION_AENC_ENABLE)
    {
        CDX_ComponentNameAudioEncoder,
        AudioEncComponentInit
    },
#endif
#if (MPPCFG_TEXTENC == OPTION_TEXTENC_ENABLE)
    {
        CDX_ComponentNameTextEncoder,
        TextEncComponentInit
    },
#endif
#if (MPPCFG_MUXER == OPTION_MUXER_ENABLE)
    {
        CDX_ComponentNameMuxer,
        RecRenderComponentInit
    },
#endif
/*
    {
        CDX_ComponentNameStreamDemux,
        StreamDemuxComponentInit
    },
*/
#if (MPPCFG_AIO == OPTION_AIO_ENABLE)
    {
        CDX_ComponentNameAIChannel,
        AIChannel_ComponentInit
    },

    {
        CDX_ComponentNameAOChannel,
        AOChannel_ComponentInit
    },
#endif
#if (MPPCFG_VI == OPTION_VI_ENABLE)
    {
		CDX_ComponentNameViScale,
		VideoViComponentInit
	},
#endif
#if (MPPCFG_ISE == OPTION_ISE_ENABLE)
    {
		CDX_ComponentNameISE,
		VideoIseComponentInit
	},
#endif
#if (MPPCFG_EIS == OPTION_EIS_ENABLE)
    {
		CDX_ComponentNameEIS,
		VideoEISComponentInit
	},
#endif
#ifdef MPPCFG_UVC	
    {
        CDX_ComponentNameUVCInput,
		UvcComponentInit
    },
#endif
};

const unsigned int SIZE_OF_COMP = sizeof(cdx_comp_table) / sizeof(CDX_COMPONENTREGISTRY);

