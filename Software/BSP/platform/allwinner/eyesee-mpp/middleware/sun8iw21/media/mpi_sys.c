/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_sys.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/23
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/
#define LOG_TAG "mpi_sys"
#include <utils/plat_log.h>

//ref platform headers
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//media api headers to app
#include "mm_common.h"
#include "mm_component.h"
#include "mm_comm_sys.h"

//media internal common headers.
#include <mpp_version.h>
#include <mpi_demux_common.h>
#include <mpi_vdec_common.h>
#include <mpi_adec_common.h>
#include <mpi_vo_common.h>
#include <mpi_clock_common.h>
#include <mpi_venc_private.h>
#include <mpi_aenc_common.h>
#include <mpi_tenc_common.h>
#include <mpi_mux_common.h>
#include <mpi_eis_common.h>
#include "mm_comm_eis.h"
#include <mpi_uvc_common.h>
#include <mpi_vi_private.h>
#include <mpi_ise_common.h>
#include <mpi_region_private.h>
#include <mpi_isp.h>
//#include <mpi_vi.h>
#include <SystemBase.h>
#include <memoryAdapter.h>
#include <sc_interface.h>
#include <audio_hw.h>
#include <VideoDec_Component.h>
#include <AudioDec_Component.h>
#include <Clock_Component.h>
#include <VideoRender_Component.h>
#include <VideoVirViCompPortIndex.h>
#include <UvcVirVi_Component.h>
#include <VideoISE_Component.h>
#include <cdx_plugin.h>

#include "ion_memmanager.h"
#include <iniparserapi.h>
#include <veAdapter.h>
#include <ConfigOption.h>

#define SYS_ReadReg(n)      (*((volatile unsigned int *)(n)))
#define SYS_WriteReg(n,c)   (*((volatile unsigned int *)(n)) = (unsigned int)(c))

typedef enum MPI_SYS_STATES_E
{
    MPI_SYS_STATE_INVALID = 0,
    MPI_SYS_STATE_CONFIGURED,
    MPI_SYS_STATE_STARTED,
} MPI_SYS_STATES_E;


//#define USE_LIBCEDARC_MEM_ALLOC

typedef struct SYS_MANAGER_S
{
    MPI_SYS_STATES_E mState;
    MPP_SYS_CONF_S mConfig;
#ifdef USE_LIBCEDARC_MEM_ALLOC
    struct ScMemOpsS *mMemOps;
#endif
    BOOL mbVeInitFlag;   //when use iommu, must init ve first, because we need ve to get iommu phyAddress
    VeOpsS *mpVeOps;
    VeConfig mVeConfig;
    void *mpVeContext;  //VeContext
    void *pKFCEnchandle;
} SYS_MANAGER_S;

static SYS_MANAGER_S gSysManager =
{
    .mState = MPI_SYS_STATE_INVALID,
    .mConfig = {
        .nAlignWidth = 32,
    },
#ifdef USE_LIBCEDARC_MEM_ALLOC
    .mMemOps = NULL,
#endif
    .mbVeInitFlag = FALSE,
    .pKFCEnchandle = NULL,
};

static void SYS_GetComp(MPP_CHN_S *pChn, MM_COMPONENTTYPE **pComp)
{
    switch (pChn->mModId)
    {
#if (MPPCFG_DEMUXER == OPTION_DEMUXER_ENABLE)
        case MOD_ID_DEMUX:
        {
            *pComp = DEMUX_GetChnComp(pChn);
            break;
        }
#endif
#if (MPPCFG_VDEC == OPTION_VDEC_ENABLE)
        case MOD_ID_VDEC:
        {
            *pComp = VDEC_GetChnComp(pChn);
            break;
        }
#endif
#if (MPPCFG_VI == OPTION_VI_ENABLE)
		case MOD_ID_VIU:
        {
            *pComp = videoInputHw_GetChnComp(pChn->mDevId, pChn->mChnId);
            break;
        }
#endif
#if (MPPCFG_VO == OPTION_VO_ENABLE)
        case MOD_ID_VOU:
        {
            *pComp = VO_GetChnComp(pChn);
            break;
        }
#endif
        case MOD_ID_CLOCK:
        {
            *pComp = CLOCK_GetChnComp(pChn);
            break;
        }
#if (MPPCFG_VENC == OPTION_VENC_ENABLE)
        case MOD_ID_VENC:
        {
            *pComp = VENC_GetChnComp(pChn);
            break;
        }
#endif
#if (MPPCFG_MUXER == OPTION_MUXER_ENABLE)
        case MOD_ID_MUX:
        {
            *pComp = MUX_GetGroupComp(pChn);
            break;
        }
#endif
#if (MPPCFG_AIO == OPTION_AIO_ENABLE)
        case MOD_ID_AI:
        {
            *pComp = audioHw_AI_GetChnComp(pChn);
            break;
        }
        case MOD_ID_AO:
        {
            *pComp = audioHw_AO_GetChnComp(pChn);
            break;
        }
#endif
#if (MPPCFG_TEXTENC == OPTION_TEXTENC_ENABLE)
        case MOD_ID_TENC:
        {
            *pComp = TENC_GetChnComp(pChn);
            break;
        }
#endif
#if (MPPCFG_AENC == OPTION_AENC_ENABLE)
        case MOD_ID_AENC:
        {
            *pComp = AENC_GetChnComp(pChn);
            break;
        }
#endif
#if (MPPCFG_ADEC == OPTION_ADEC_ENABLE)
        case MOD_ID_ADEC:
        {
            *pComp = ADEC_GetChnComp(pChn);
            break;
        }
#endif
#if (MPPCFG_ISE == OPTION_ISE_ENABLE)
        case MOD_ID_ISE:
        {
            *pComp = ISE_GetGroupComp(pChn);
            break;
        }
#endif
#if (MPPCFG_EIS == OPTION_EIS_ENABLE)
        case MOD_ID_EIS:
        {
            *pComp = EIS_GetGroupComp(pChn);
            break;
        }
#endif
#ifdef MPPCFG_UVC
        case MOD_ID_UVC:
        {
            *pComp = uvcInput_GetChnComp((UVC_DEV)pChn->mDevId, pChn->mChnId);
            break;
        }
#endif
        default:
        {
            aloge("fatal error! Undefine source mod id %d!", pChn->mModId);
            break;
        }
    }
}

typedef struct MppBindMap {
    MOD_ID_E mSrcModId;
    //unsigned int mSrcPortIdx;
    MOD_ID_E mDstModId;
    //unsigned int mDstPortIdx;
}MppBindMap;

MppBindMap gMppBindMapTable[] =
{
    {MOD_ID_AI,         MOD_ID_AENC },  //ai    -> aenc
    {MOD_ID_AI,         MOD_ID_AO   },  //ai    -> ao
    {MOD_ID_AO,         MOD_ID_AI   },  //ao    -> ai
    {MOD_ID_AENC,       MOD_ID_MUX  },  //aenc  -> mux
    {MOD_ID_TENC,       MOD_ID_MUX  },  //tenc  -> mux
    {MOD_ID_VIU,        MOD_ID_VENC },  //vi    -> venc
    {MOD_ID_VIU,        MOD_ID_VOU  },  //vi    -> vo
    {MOD_ID_VIU,        MOD_ID_ISE  },  //vi    -> ise
    {MOD_ID_VIU,        MOD_ID_EIS  },  //vi    -> eis
    {MOD_ID_ISE,        MOD_ID_EIS  },  //ise   -> eis
    {MOD_ID_EIS,        MOD_ID_VENC },  //eis   -> venc
    {MOD_ID_ISE,        MOD_ID_VENC },  //ise   -> venc
    {MOD_ID_ISE,        MOD_ID_VOU  },  //ise   -> vo
    {MOD_ID_VENC,       MOD_ID_MUX  },  //venc  -> mux
    {MOD_ID_CLOCK,      MOD_ID_AO   },  //clock -> ao
    {MOD_ID_CLOCK,      MOD_ID_VOU  },  //clock -> vo
    {MOD_ID_CLOCK,      MOD_ID_DEMUX},  //clock -> demux
    {MOD_ID_DEMUX,      MOD_ID_ADEC },  //demux -> adec
    {MOD_ID_DEMUX,      MOD_ID_VDEC },  //demux -> vdec
    {MOD_ID_DEMUX,      MOD_ID_MUX  },  //demux -> mux
    {MOD_ID_ADEC,       MOD_ID_AO   },  //adec  -> ao
    {MOD_ID_VDEC,       MOD_ID_VOU  },  //vdec  -> vo
    {MOD_ID_VDEC,       MOD_ID_VENC },  //vdec  -> venc
    {MOD_ID_UVC,        MOD_ID_VDEC },  //uvc   -> vdec (mjpeg h264 data)
    {MOD_ID_UVC,        MOD_ID_MUX  },  //uvc   -> mux
    {MOD_ID_UVC,        MOD_ID_VENC },  //uvc   -> venc (yuv data)
    {MOD_ID_UVC,        MOD_ID_VOU  },  //uvc   -> vo
    //{MOD_ID_UVC,        MOD_ID_ISE  },  //uvc   -> ise
};

static ERRORTYPE SYS_QueryBindRelation(MPP_CHN_S *pSrcChn, MPP_CHN_S *pDstChn)
{
    ERRORTYPE findFlag = FAILURE;
    int tableSize = sizeof(gMppBindMapTable) / sizeof(gMppBindMapTable[0]);
    for (int i=0; i<tableSize; i++)
    {
        if ((gMppBindMapTable[i].mSrcModId==pSrcChn->mModId) && (gMppBindMapTable[i].mDstModId==pDstChn->mModId))
        {
            findFlag = SUCCESS;
            break;
        }
    }
    return findFlag;
}

//static ERRORTYPE SYS_QueryBindPortIndex(MPP_CHN_S *pSrcChn, MPP_CHN_S *pDstChn, unsigned int *pSrcPortIdx, unsigned int *pDstPortIdx)
//{
//    ERRORTYPE retVal = FAILURE;
//    int tableSize = sizeof(gMppBindMapTable) / sizeof(gMppBindMapTable[0]);
//    for (int i=0; i<tableSize; i++)
//    {
//        if ((gMppBindMapTable[i].mSrcModId==pSrcChn->mModId) && (gMppBindMapTable[i].mDstModId==pDstChn->mModId))
//        {
//            retVal = SUCCESS;
//            *pSrcPortIdx = gMppBindMapTable[i].mSrcPortIdx;
//            *pDstPortIdx = gMppBindMapTable[i].mDstPortIdx;
//            break;
//        }
//    }
//    return retVal;
//}

static void SYS_DecideBindPortIndex(MPP_CHN_S *pSrcChn, MPP_CHN_S *pDestChn, unsigned int *pSrcPortIdx, unsigned int *pDstPortIdx, MppBindControl* pBindControl)
{
    //source channel's portIndex
    switch (pSrcChn->mModId)
    {
        case MOD_ID_DEMUX:
        {
            MM_COMPONENTTYPE *pSrcComp;
            SYS_GetComp(pSrcChn, &pSrcComp);
            if (pSrcComp == NULL)
            {
                aloge("fatal error! source demux component not found!");
                return;
            }
            COMP_PORT_PARAM_TYPE ports_para;
            if(SUCCESS != pSrcComp->GetConfig(pSrcComp, COMP_IndexVendorGetPortParam, &ports_para))
            {
                aloge("fatal error! demux component get port param fail!");
                return;
            }
            int bFindFlag = 0;
            int port_idx;
            COMP_PARAM_PORTDEFINITIONTYPE PortDef;
            for (port_idx = ports_para.nStartPortNumber; port_idx < (ports_para.nStartPortNumber + ports_para.nPorts); port_idx++)
            {
                PortDef.nPortIndex = port_idx;
                pSrcComp->GetConfig(pSrcComp, COMP_IndexParamPortDefinition, &PortDef);
                switch (PortDef.eDomain)
                {
                    case COMP_PortDomainAudio:
                    {
                        if (pDestChn->mModId == MOD_ID_ADEC)
                        {
                            *pSrcPortIdx = PortDef.nPortIndex;
                            bFindFlag = 1;
                        }

                        if (pDestChn->mModId == MOD_ID_MUX)
                        {
                            if (pBindControl == NULL || pBindControl->eDomain == COMP_PortDomainAudio)
                            {// default bind audio port of demux and mux
                                *pSrcPortIdx = PortDef.nPortIndex;
                                bFindFlag = 1;
                            }
                        }
                        break;
                    }
                    case COMP_PortDomainVideo:
                    {
                        if (pDestChn->mModId == MOD_ID_VDEC)
                        {
                            *pSrcPortIdx = PortDef.nPortIndex;
                            bFindFlag = 1;
                        }

                        if (pDestChn->mModId == MOD_ID_MUX)
                        {
                            if (pBindControl != NULL && pBindControl->eDomain == COMP_PortDomainVideo)
                            {
                                *pSrcPortIdx = PortDef.nPortIndex;
                                bFindFlag = 1;
                            }
                        }
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                if(bFindFlag)
                {
                    break;
                }
            }
            if (0 == bFindFlag)
            {
                aloge("fatal error! demux component not find match port for dst component mod[0x%x]!", pDestChn->mModId);
            }
            break;
        }
        case MOD_ID_VDEC:
        {
            *pSrcPortIdx = VDEC_OUT_PORT_INDEX_VRENDER;
            break;
        }
        case MOD_ID_ADEC:
        {
            *pSrcPortIdx = ADEC_OUT_PORT_INDEX_VRENDER;
            break;
        }
        case MOD_ID_CLOCK:
        {
            if(pDestChn->mModId == MOD_ID_AO)
            {
                *pSrcPortIdx = CLOCK_PORT_INDEX_AUDIO;
            }
            else if(pDestChn->mModId == MOD_ID_VOU)
            {
                *pSrcPortIdx = CLOCK_PORT_INDEX_VIDEO;
            }
            else if(pDestChn->mModId == MOD_ID_DEMUX)
            {
                *pSrcPortIdx = CLOCK_PORT_INDEX_DEMUX;
            }
            else if(pDestChn->mModId == MOD_ID_VDEC)
            {
                *pSrcPortIdx = CLOCK_PORT_INDEX_VDEC;
            }
            else
            {
                aloge("fatal error! clock component not find match port for dst component mod[0x%x]!", pDestChn->mModId);
            }
            break;
        }
        case MOD_ID_VENC:
        case MOD_ID_AENC:
        {
            *pSrcPortIdx = 1;
            break;
        }
        case MOD_ID_TENC:
        {
            *pSrcPortIdx = 1;
            break;
        }
        case MOD_ID_AI:
        {
            if (pDestChn->mModId == MOD_ID_AENC)
            {
                // ai -> aenc
                *pSrcPortIdx = AI_CHN_PORT_INDEX_OUT_AENC;
            }
            else if (pDestChn->mModId == MOD_ID_AO)
            {
                // ai -> ao
                *pSrcPortIdx = AI_CHN_PORT_INDEX_OUT_AO;
            }
            else
            {
                aloge("fatal error! clock component not find match port for dst component mod[0x%x]!", pDestChn->mModId);
            }
            break;
        }
        case MOD_ID_AO:
        {
            if (pDestChn->mModId == MOD_ID_AI)
            {
                *pSrcPortIdx = AO_CHN_PORT_INDEX_OUT_AI;
            }
            else
            {
                *pSrcPortIdx = AO_CHN_PORT_INDEX_OUT_PLAY;
            }
            break;
        }
		case MOD_ID_VIU:
        {   //pDestChn->mModId =MOD_ID_VDA, MOD_ID_VENC, MOD_ID_ISE, MOD_ID_VOU
            *pSrcPortIdx = VI_CHN_PORT_INDEX_OUT;
			break;
        }
        case MOD_ID_ISE:
        {   //pDestChn->mModId =MOD_ID_VDA, MOD_ID_VENC, MOD_ID_VOU
            if (pSrcChn->mChnId == 0) {
                *pSrcPortIdx = ISE_PORT_INDEX_OUT0; // 2
            } else if (pSrcChn->mChnId == 1) {
                *pSrcPortIdx = ISE_PORT_INDEX_OUT1; // 3
            } else if (pSrcChn->mChnId == 2) {
                *pSrcPortIdx = ISE_PORT_INDEX_OUT2; // 4
            } else if (pSrcChn->mChnId == 3) {
                *pSrcPortIdx = ISE_PORT_INDEX_OUT3; // 5
            }
            break;
        }
        case MOD_ID_EIS:
        {
            *pSrcPortIdx = EIS_CHN_PORT_INDEX_OUT;
            break;
        }
        case MOD_ID_UVC:
        {
            *pSrcPortIdx = UVC_CHN_PORT_INDEX_DATA_OUT;
            break;
        }
        default:
        {
            aloge("fatal error! srcModId[0x%x] not support!", pSrcChn->mModId);
            break;
        }
    }

    //destination channel's portIndex
    switch (pDestChn->mModId)
    {
        case MOD_ID_DEMUX:
        {
            if(pSrcChn->mModId == MOD_ID_CLOCK)
            {
                MM_COMPONENTTYPE *pDstComp;
                SYS_GetComp(pDestChn, &pDstComp);
                if (pDstComp == NULL)
                {
                    aloge("fatal error! Dst demux component not found!");
                    return;
                }
                COMP_PORT_PARAM_TYPE ports_para;
                if(SUCCESS != pDstComp->GetConfig(pDstComp, COMP_IndexVendorGetPortParam, &ports_para))
                {
                    aloge("fatal error! demux component get port param fail!");
                    return;
                }
                int bFindFlag = 0;
                int port_idx;
                COMP_PARAM_PORTDEFINITIONTYPE PortDef;
                for (port_idx = ports_para.nStartPortNumber; port_idx < (ports_para.nStartPortNumber + ports_para.nPorts); port_idx++)
                {
                    PortDef.nPortIndex = port_idx;
                    pDstComp->GetConfig(pDstComp, COMP_IndexParamPortDefinition, &PortDef);
                    switch (PortDef.eDomain)
                    {
                        case COMP_PortDomainOther:
                        {
                            if (pSrcChn->mModId == MOD_ID_CLOCK)
                            {
                                *pDstPortIdx = PortDef.nPortIndex;
                                bFindFlag = 1;
                            }
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                    if(bFindFlag)
                    {
                        break;
                    }
                }
                if(0 == bFindFlag)
                {
                    aloge("fatal error! demux component not find match port for dst component mod[0x%x]!", pDestChn->mModId);
                }
                break;
            }
            else
            {
                aloge("fatal error! demux find not match port for sourceMod[0x%x]!", pSrcChn->mModId);
            }
            break;
        }
        case MOD_ID_VENC:
        {
            #if 0
		if(pSrcChn->mModId == MOD_ID_VIU)
		{
			*pDstPortIdx = 0;
		}
		if(pSrcChn->mModId == MOD_ID_ISE)
            {
                *pDstPortIdx = 0;
            }
            if(pSrcChn->mModId == MOD_ID_VDEC)
            {
                *pDstPortIdx = 0;
            }
            #endif
            *pDstPortIdx = 0;
		break;
        }

        case MOD_ID_AENC:
        {
            *pDstPortIdx = 0;
            break;
        }
        case MOD_ID_VDEC:
        {
            if(pSrcChn->mModId == MOD_ID_DEMUX || pSrcChn->mModId == MOD_ID_UVC)
            {
                *pDstPortIdx = VDEC_PORT_INDEX_DEMUX;
            }
            else if(pSrcChn->mModId == MOD_ID_CLOCK)
            {
                *pDstPortIdx = VDEC_PORT_INDEX_CLOCK;
            }
            else
            {
                aloge("fatal error! adec not find match port for sourceMod[0x%x]!", pSrcChn->mModId);
            }
            break;
        }
        case MOD_ID_VOU:
        {
            if(pSrcChn->mModId == MOD_ID_VDEC || pSrcChn->mModId == MOD_ID_VIU || pSrcChn->mModId == MOD_ID_UVC || pSrcChn->mModId == MOD_ID_ISE)
            {
                *pDstPortIdx = VDR_PORT_INDEX_VIDEO;
            }
            else if(pSrcChn->mModId == MOD_ID_CLOCK)
            {
                *pDstPortIdx = VDR_PORT_INDEX_CLOCK;
            }
            //else if(pSrcChn->mModId == MOD_ID_ISE)
            //{
            //    *pDstPortIdx = VDR_PORT_INDEX_VIDEO;
            //}
            else
            {
                aloge("fatal error! vo not find match port for sourceMod[0x%x]!", pSrcChn->mModId);
            }
            break;
        }
        case MOD_ID_MUX:
        {
            MM_COMPONENTTYPE *pDestComp;
            SYS_GetComp(pDestChn, &pDestComp);
            if (pDestComp == NULL)
            {
                aloge("fatal error! destination mux component not found!");
                return;
            }
            COMP_PORT_PARAM_TYPE ports_para;
            if (SUCCESS != pDestComp->GetConfig(pDestComp, COMP_IndexVendorGetPortParam, &ports_para))
            {
                aloge("fatal error! mux component get port param fail!");
                return;
            }
            int bFindFlag = 0;
            int port_idx;
            COMP_PARAM_PORTDEFINITIONTYPE PortDef;
            for (port_idx = ports_para.nStartPortNumber; port_idx < (ports_para.nStartPortNumber + ports_para.nPorts); port_idx++)
            {
                if (bFindFlag)
                {
                    break;
                }
                memset(&PortDef, 0, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
                PortDef.nPortIndex = port_idx;
                pDestComp->GetConfig(pDestComp, COMP_IndexParamPortDefinition, &PortDef);
                if (COMP_PortDomainVendorStartUnused == PortDef.eDomain)
                {
                    switch (pSrcChn->mModId)
                    {
                        case MOD_ID_VENC:
                        {
                            PortDef.nPortIndex = port_idx;
                            PortDef.eDir = COMP_DirInput;
                            PortDef.bEnabled = true;
                            PortDef.eDomain = COMP_PortDomainVideo;
                            pDestComp->SetConfig(pDestComp, COMP_IndexParamPortDefinition, &PortDef);
                            *pDstPortIdx = port_idx;
                            alogd("MOD_ID_VENC: "
                                  "PortDef.nPortIndex: %d, "
                                  "PortDef.eDir: %d, "
                                  "PortDef.bEnabled: %d, "
                                  "PortDef.eDomain: %d, "
                                  "pDstPortIdx: %d",
                                  PortDef.nPortIndex, PortDef.eDir, PortDef.bEnabled, PortDef.eDomain, *pDstPortIdx);
                            bFindFlag = 1;
                            break;
                        }
                        case MOD_ID_AENC:
                        {
                            PortDef.nPortIndex = port_idx;
                            PortDef.eDir = COMP_DirInput;
                            PortDef.bEnabled = true;
                            PortDef.eDomain = COMP_PortDomainAudio;
                            pDestComp->SetConfig(pDestComp, COMP_IndexParamPortDefinition, &PortDef);
                            *pDstPortIdx = port_idx;
                                  alogd("MOD_ID_AENC: "
                                  "PortDef.nPortIndex: %d, "
                                  "PortDef.eDir: %d, "
                                  "PortDef.bEnabled: %d, "
                                  "PortDef.eDomain: %d, "
                                  "pDstPortIdx: %d",
                                  PortDef.nPortIndex, PortDef.eDir, PortDef.bEnabled, PortDef.eDomain, *pDstPortIdx);
                            bFindFlag = 1;
                            break;
                        }
                        case MOD_ID_TENC:
                        {
                            PortDef.nPortIndex = port_idx;
                            PortDef.eDir = COMP_DirInput;
                            PortDef.bEnabled = true;
                            PortDef.eDomain = COMP_PortDomainText;
                            pDestComp->SetConfig(pDestComp, COMP_IndexParamPortDefinition, &PortDef);
                            *pDstPortIdx = port_idx;
                            alogd("MOD_ID_TENC: "
                                  "PortDef.nPortIndex: %d, "
                                  "PortDef.eDir: %d, "
                                  "PortDef.bEnabled: %d, "
                                  "PortDef.eDomain: %d, "
                                  "pDstPortIdx: %d",
                                  PortDef.nPortIndex, PortDef.eDir, PortDef.bEnabled, PortDef.eDomain, *pDstPortIdx);
                            bFindFlag = 1;
                            break;
                        }
                        default:
                        {
                            aloge("fatal error! srcModId[0x%x] not support!", pSrcChn->mModId);
                            break;
                        }
                    }

                }

            }
            break;
        }

        case MOD_ID_AI:
        {
            if (MOD_ID_AO == pSrcChn->mModId)
            {
                *pDstPortIdx = AI_CHN_PORT_INDEX_AO_IN;
            }
            else if (MOD_ID_AENC == pSrcChn->mModId)
            {
                *pDstPortIdx = AI_CHN_PORT_INDEX_CAP_IN;
            }
            else
            {
                aloge("fatal error! ai not find match port for sourceMod[0x%x]!", pSrcChn->mModId);
            }
            break;
        }
        case MOD_ID_AO:
        {
            if (MOD_ID_ADEC == pSrcChn->mModId)
            {
                // adec -> ao
                *pDstPortIdx = AO_CHN_PORT_INDEX_IN_PCM;
            }
            else if (MOD_ID_AI == pSrcChn->mModId)
            {
                // ai -> ao
                *pDstPortIdx = AO_CHN_PORT_INDEX_IN_PCM;
            }
            else if (MOD_ID_CLOCK == pSrcChn->mModId)
            {
                // clk -> ao
                *pDstPortIdx = AO_CHN_PORT_INDEX_IN_CLK;
            }
            else
            {
                aloge("fatal error! ao not find match port for sourceMod[0x%x]!", pSrcChn->mModId);
            }
            break;
        }
        case MOD_ID_ADEC:
        {
            if(pSrcChn->mModId == MOD_ID_DEMUX)
            {
                *pDstPortIdx = ADEC_PORT_INDEX_DEMUX;
            }
            else if(pSrcChn->mModId == MOD_ID_CLOCK)
            {
                *pDstPortIdx = ADEC_PORT_INDEX_CLOCK;
            }
            else
            {
                aloge("fatal error! adec not find match port for sourceMod[0x%x]!", pSrcChn->mModId);
            }
            break;
        }
        case MOD_ID_ISE:
        {
            int find = 0;
            int i;
            if(pSrcChn->mModId == MOD_ID_VIU)
            {
                MM_COMPONENTTYPE *pDstComp;
                SYS_GetComp(pDestChn, &pDstComp);
                if (pDstComp == NULL)
                {
                    aloge("fatal error! Dst ise component not found!");
                    return;
                }
                for (i = ISE_PORT_INDEX_CAP0_IN; i <= ISE_PORT_INDEX_CAP1_IN; i++)
                {
                    COMP_INTERNAL_TUNNELINFOTYPE TunnelInfo;
                    TunnelInfo.nPortIndex = i; // ISE_PORT_INDEX_CAP0_IN;
                    if(SUCCESS != pDstComp->GetConfig(pDstComp, COMP_IndexVendorTunnelInfo, &TunnelInfo))
                    {
                        aloge("fatal error! ise component get port param fail!");
                        return;
                    }
                    if (NULL == TunnelInfo.hTunnel)
                    {
                        *pDstPortIdx = i; // ISE_PORT_INDEX_CAP0_IN
                        find = 1;
                        break;
                    }
                }
                if (0 == find)
                {
                    alogw("Already ise bind mode.");
                }
            }
			else
            {
               aloge("fatal error! vda not find match port for sourceMod[0x%x]!", pSrcChn->mModId);
            }
            break;
        }
        case MOD_ID_EIS:
        {
            int find = 0;
            if(pSrcChn->mModId == MOD_ID_VIU || pSrcChn->mModId == MOD_ID_ISE)
            {
                *pDstPortIdx = EIS_CHN_PORT_INDEX_VIDEO_IN;
            }
			else
            {
               aloge("fatal error! eis not find match port for sourceMod[0x%x]!", pSrcChn->mModId);
            }
            break;
        }
        default:
        {
            aloge("fatal error! dstModId[0x%x] not support!", pDestChn->mModId);
            break;
        }
    }
}

#define KFCAPIENC_LIBPATH "/usr/lib/libkfcapi_enc.so"
#define IPLOADER_LIBPATH "/usr/lib/libip_loader_soft.so"
#define KFCTMPDIR "/tmp"

ERRORTYPE AW_MPI_SYS_SetConf(const MPP_SYS_CONF_S* pSysConf)
{
    if (pSysConf == NULL)
    {
        alogw("AW_MPI_SYS SetConf illegal param");
        return ERR_SYS_ILLEGAL_PARAM;
    }
    if (gSysManager.mState == MPI_SYS_STATE_STARTED)
    {
        alogw("AW_MPI_SYS SetConf state[0x%x] is invalid", gSysManager.mState);
        return ERR_SYS_NOT_PERM;
    }

    gSysManager.mConfig = *pSysConf;
    if(strlen(gSysManager.mConfig.mkfcTmpDir) <= 0)
    {
        strcpy(gSysManager.mConfig.mkfcTmpDir, KFCTMPDIR);
    }
    alogd("kfctmpdir is [%s]", gSysManager.mConfig.mkfcTmpDir);
    gSysManager.mState = MPI_SYS_STATE_CONFIGURED;
    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_GetConf(MPP_SYS_CONF_S* pSysConf)
{
    if (pSysConf == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }
    if (gSysManager.mState == MPI_SYS_STATE_INVALID) {
        return ERR_SYS_NOTREADY;
    }

    *pSysConf = gSysManager.mConfig;

    return SUCCESS;
}

static int is_iommu_enabled(void)
{
    struct stat iommu_sysfs;
    if (stat("/sys/class/iommu", &iommu_sysfs) == 0 && S_ISDIR(iommu_sysfs.st_mode))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static ERRORTYPE ion_iommu_prepare()
{
    if(0 == is_iommu_enabled())
    {
        gSysManager.mbVeInitFlag = FALSE;
        gSysManager.mpVeOps = NULL;
        memset(&gSysManager.mVeConfig, 0, sizeof(VeConfig));
        gSysManager.mpVeContext = NULL;
        return SUCCESS;
    }
    if(gSysManager.mbVeInitFlag)
    {
        alogw("Be careful! ve is already init!");
        return SUCCESS;
    }
    gSysManager.mbVeInitFlag = FALSE;
    gSysManager.mpVeOps = NULL;
    memset(&gSysManager.mVeConfig, 0, sizeof(VeConfig));
    gSysManager.mpVeContext = NULL;

    gSysManager.mpVeOps = GetVeOpsS(VE_OPS_TYPE_NORMAL);
    if(gSysManager.mpVeOps == NULL)
    {
        aloge("fatal error! get ve ops failed");
        goto _err0;
    }
    memset(&gSysManager.mVeConfig, 0, sizeof(VeConfig));
    gSysManager.mpVeContext = CdcVeInit(gSysManager.mpVeOps, &gSysManager.mVeConfig);
    if(NULL == gSysManager.mpVeContext)
    {
        aloge("fatal error! VeInit failed");
        goto _err1;
    }
    gSysManager.mbVeInitFlag = TRUE;
    return SUCCESS;

_err1:
    gSysManager.mpVeOps = NULL;
_err0:
    return FAILURE;
}

static ERRORTYPE ion_iommu_over()
{
    if(gSysManager.mbVeInitFlag)
    {
        CdcVeRelease(gSysManager.mpVeOps, gSysManager.mpVeContext);
        gSysManager.mpVeContext = NULL;
        gSysManager.mpVeOps = NULL;
        gSysManager.mbVeInitFlag = FALSE;
    }
    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_Init_S1(void)
{
    ERRORTYPE eError = SUCCESS;
    MPPLogVersionInfo();

    if (gSysManager.mState == MPI_SYS_STATE_INVALID) {
        return ERR_SYS_NOTREADY;
    }
    if (gSysManager.mState == MPI_SYS_STATE_STARTED) {
        return SUCCESS;
    }
    int ret = SUCCESS;
#if (MPPCFG_VI == OPTION_VI_ENABLE)
    ret = AW_MPI_VI_Init();
    if (0 != ret) {
        aloge("AW_MPI_VI_Init failed");
        return FAILURE;
    }
    alogd("ISP init");
    AW_MPI_ISP_Init();
    alogd("ISP init done");
#endif
#ifdef USE_LIBCEDARC_MEM_ALLOC
    gSysManager.mMemOps = MemAdapterGetOpsS();
    if (CdcMemOpen(gSysManager.mMemOps) < 0) {
        aloge("MemAdapterOpen failed!");
        return FAILURE;
    }
    alogv("MemAdapterOpen ok");
#else
    ret = ion_iommu_prepare();
    if(ret != SUCCESS)
    {
        aloge("fatal error! ion iommu prepare fail!");
        return FAILURE;
    }
    ret = ion_memOpen();
    if (ret != 0)
    {
        aloge("Open ion failed!");
        return FAILURE;
    }
#endif
#if (MPPCFG_VO == OPTION_VO_ENABLE)
    eError = VO_Construct();
    if (eError != SUCCESS)
    {
        aloge("VO Construct error!");
        goto ERR_EXIT0;
    }
#endif
    eError = RegionManager_Construct();
    if (eError != SUCCESS)
    {
        aloge("RGN Construct error!");
        goto ERR_EXIT0;
    }

    return SUCCESS;

ERR_EXIT0:
#ifdef USE_LIBCEDARC_MEM_ALLOC
    CdcMemClose(gSysManager.mMemOps);
#else
    ion_memClose();
#endif
    return eError;
}

ERRORTYPE AW_MPI_SYS_Init_S2(void)
{
    ERRORTYPE eError = SUCCESS;
    MPPLogVersionInfo();

    if (gSysManager.mState == MPI_SYS_STATE_INVALID) {
        return ERR_SYS_NOTREADY;
    }
    if (gSysManager.mState == MPI_SYS_STATE_STARTED) {
        return SUCCESS;
    }
    //CdxPluginInit();
#if (MPPCFG_DEMUXER == OPTION_DEMUXER_ENABLE)
    eError = DEMUX_Construct();
    if (eError != SUCCESS)
    {
        aloge("DEMUX Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_VDEC == OPTION_VDEC_ENABLE)
    eError = VDEC_Construct();
    if (eError != SUCCESS)
    {
        aloge("VDEC Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_ADEC == OPTION_ADEC_ENABLE)
    eError = ADEC_Construct();
    if (eError != SUCCESS)
    {
        aloge("ADEC Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_TEXTENC == OPTION_TEXTENC_ENABLE)
    eError = TENC_Construct();
    if (eError != SUCCESS) {
        aloge("AW_MPI_TENC_Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_AENC == OPTION_AENC_ENABLE)
    eError = AENC_Construct();
    if (eError != SUCCESS) {
        aloge("AW_MPI_AENC_Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_AIO == OPTION_AIO_ENABLE)
    eError = audioHw_Construct();
    if (eError != SUCCESS) {
        aloge("audioHw_Construct error!");
        goto ERR_EXIT0;
    }
#endif
    eError = CLOCK_Construct();
    if (eError != SUCCESS)
    {
        aloge("CLOCK Construct error!");
        goto ERR_EXIT0;
    }
#if (MPPCFG_VENC == OPTION_VENC_ENABLE)
    eError = VENC_Construct();
    if (eError != SUCCESS) {
        aloge("VENC Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_MUXER == OPTION_MUXER_ENABLE)
    eError = MUX_Construct();
    if (eError != SUCCESS) {
        aloge("MUX Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_ISE == OPTION_ISE_ENABLE)
    eError = ISE_Construct();
    if (eError != SUCCESS) {
        aloge("ISE Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_EIS == OPTION_EIS_ENABLE)
    eError = EIS_Construct();
    if (eError != SUCCESS) {
        aloge("EIS Construct error!");
        goto ERR_EXIT0;
    }
#endif
#ifdef MPPCFG_UVC
    eError = UVC_Construct();
    if (eError != SUCCESS) {
        aloge("UVC Construct error!");
        goto ERR_EXIT0;
    }
#endif
    // todo

    gSysManager.mState = MPI_SYS_STATE_STARTED;
    return SUCCESS;

ERR_EXIT0:
#ifdef USE_LIBCEDARC_MEM_ALLOC
    CdcMemClose(gSysManager.mMemOps);
#else
    ion_memClose();
#endif
    return eError;
}

ERRORTYPE AW_MPI_SYS_Init_S3(void)
{
    ERRORTYPE eError = SUCCESS;
    MPPLogVersionInfo();

    if (gSysManager.mState == MPI_SYS_STATE_INVALID) {
        return ERR_SYS_NOTREADY;
    }
    if (gSysManager.mState == MPI_SYS_STATE_STARTED) {
        return SUCCESS;
    }
    int ret = SUCCESS;
#if (MPPCFG_VI == OPTION_VI_ENABLE)
    ret = AW_MPI_VI_Init();
    if (0 != ret) {
        aloge("AW_MPI_VI_Init failed");
        return FAILURE;
    }
    AW_MPI_ISP_Init();
#endif
#ifdef USE_LIBCEDARC_MEM_ALLOC

    gSysManager.mMemOps = MemAdapterGetOpsS();
    if (CdcMemOpen(gSysManager.mMemOps) < 0) {
        aloge("MemAdapterOpen failed!");
        return FAILURE;
    }
    alogv("MemAdapterOpen ok");
#else
    ret = ion_iommu_prepare();
    if(ret != SUCCESS)
    {
        aloge("fatal error! ion iommu prepare fail!");
        return FAILURE;
    }
    ret = ion_memOpen();
    if (ret != 0)
    {
        aloge("Open ion failed!");
        return FAILURE;
    }
#endif
    //CdxPluginInit();
#if (MPPCFG_DEMUXER == OPTION_DEMUXER_ENABLE)
    eError = DEMUX_Construct();
    if (eError != SUCCESS)
    {
        aloge("DEMUX Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_VDEC == OPTION_VDEC_ENABLE)
    eError = VDEC_Construct();
    if (eError != SUCCESS)
    {
        aloge("VDEC Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_VO == OPTION_VO_ENABLE)
    eError = VO_Construct();
    if (eError != SUCCESS)
    {
        aloge("VO Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_TEXTENC == OPTION_TEXTENC_ENABLE)
    eError = TENC_Construct();
    if (eError != SUCCESS) {
        aloge("AW_MPI_TENC_Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_ADEC == OPTION_ADEC_ENABLE)
    eError = ADEC_Construct();
    if (eError != SUCCESS)
    {
        aloge("ADEC Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_AENC == OPTION_AENC_ENABLE)
    eError = AENC_Construct();
    if (eError != SUCCESS) {
        aloge("AW_MPI_AENC_Construct error!");
        goto ERR_EXIT0;
    }
#endif
    //eError = audioHw_Construct();
    //if (eError != SUCCESS) {
        //aloge("audioHw_Construct error!");
        //goto ERR_EXIT0;
    //}
    eError = CLOCK_Construct();
    if (eError != SUCCESS)
    {
        aloge("CLOCK Construct error!");
        goto ERR_EXIT0;
    }
#if (MPPCFG_VENC == OPTION_VENC_ENABLE)
    eError = VENC_Construct();
    if (eError != SUCCESS) {
        aloge("VENC Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_MUXER == OPTION_MUXER_ENABLE)
    eError = MUX_Construct();
    if (eError != SUCCESS) {
        aloge("MUX Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_ISE == OPTION_ISE_ENABLE)
    eError = ISE_Construct();
    if (eError != SUCCESS) {
        aloge("ISE Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_EIS == OPTION_EIS_ENABLE)
		eError = EIS_Construct();
		if (eError != SUCCESS) {
			aloge("EIS Construct error!");
			goto ERR_EXIT0;
		}
#endif

    eError = RegionManager_Construct();
    if (eError != SUCCESS)
    {
        aloge("RGN Construct error!");
        goto ERR_EXIT0;
    }
#ifdef MPPCFG_UVC
    eError = UVC_Construct();
    if (eError != SUCCESS) {
        aloge("UVC Construct error!");
        goto ERR_EXIT0;
    }
#endif
    gSysManager.mState = MPI_SYS_STATE_STARTED;
    return SUCCESS;

ERR_EXIT0:
#ifdef USE_LIBCEDARC_MEM_ALLOC
    CdcMemClose(gSysManager.mMemOps);
#else
    ion_memClose();
#endif
    return eError;
}

ERRORTYPE AW_MPI_SYS_Init(void)
{
    ERRORTYPE eError = SUCCESS;
    MPPLogVersionInfo();
    int cedarx_log_level = GetConfigParamterInt("log_level", 0);    //only for that log_set_level() can be called to set loglevel of cedarx.conf.

    if (gSysManager.mState == MPI_SYS_STATE_INVALID) {
        return ERR_SYS_NOTREADY;
    }
    if (gSysManager.mState == MPI_SYS_STATE_STARTED) {
        return SUCCESS;
    }
    int ret = SUCCESS;
#if (MPPCFG_VI == OPTION_VI_ENABLE)
    ret = AW_MPI_VI_Init();
    if (0 != ret) {
        aloge("AW_MPI_VI_Init failed");
        return FAILURE;
    }
    alogd("ISP init");
    AW_MPI_ISP_Init();
    alogd("ISP init done");
#endif
#ifdef USE_LIBCEDARC_MEM_ALLOC
    gSysManager.mMemOps = MemAdapterGetOpsS();
    if (CdcMemOpen(gSysManager.mMemOps) < 0) {
        aloge("MemAdapterOpen failed!");
        return FAILURE;
    }
    alogv("MemAdapterOpen ok");
#else

    ret = ion_iommu_prepare();
    if(ret != SUCCESS)
    {
        aloge("fatal error! ion iommu prepare fail!");
        return FAILURE;
    }
    ret = ion_memOpen();
    if (ret != 0)
    {
        aloge("Open ion failed!");
        return FAILURE;
    }
#endif
    //CdxPluginInit();
#if (MPPCFG_DEMUXER == OPTION_DEMUXER_ENABLE)
    eError = DEMUX_Construct();
    if (eError != SUCCESS)
    {
        aloge("DEMUX Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_VDEC == OPTION_VDEC_ENABLE)
    eError = VDEC_Construct();
    if (eError != SUCCESS)
    {
        aloge("VDEC Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_VO == OPTION_VO_ENABLE)
    eError = VO_Construct();
    if (eError != SUCCESS)
    {
        aloge("VO Construct error!");
        //goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_ADEC == OPTION_ADEC_ENABLE)
    eError = ADEC_Construct();
    if (eError != SUCCESS)
    {
        aloge("ADEC Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_TEXTENC == OPTION_TEXTENC_ENABLE)
    eError = TENC_Construct();
    if (eError != SUCCESS) {
        aloge("AW_MPI_TENC_Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_AENC == OPTION_AENC_ENABLE)
    eError = AENC_Construct();
    if (eError != SUCCESS) {
        aloge("AW_MPI_AENC_Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_AIO == OPTION_AIO_ENABLE)
    eError = audioHw_Construct();
    if (eError != SUCCESS) {
        aloge("audioHw_Construct error!");
        goto ERR_EXIT0;
    }
#endif
    eError = CLOCK_Construct();
    if (eError != SUCCESS)
    {
        aloge("CLOCK Construct error!");
        goto ERR_EXIT0;
    }
#if (MPPCFG_VENC == OPTION_VENC_ENABLE)
    eError = VENC_Construct();
    if (eError != SUCCESS) {
        aloge("VENC Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_MUXER == OPTION_MUXER_ENABLE)
    eError = MUX_Construct();
    if (eError != SUCCESS) {
        aloge("MUX Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_ISE == OPTION_ISE_ENABLE)
    eError = ISE_Construct();
    if (eError != SUCCESS) {
        aloge("ISE Construct error!");
        goto ERR_EXIT0;
    }
#endif
#if (MPPCFG_EIS == OPTION_EIS_ENABLE)
    eError = EIS_Construct();
    if (eError != SUCCESS) {
        aloge("EIS Construct error!");
        goto ERR_EXIT0;
    }
#endif

    eError = RegionManager_Construct();
    if (eError != SUCCESS)
    {
        aloge("RGN Construct error!");
        goto ERR_EXIT0;
    }

#ifdef MPPCFG_UVC
    eError = UVC_Construct();
    if (eError != SUCCESS) {
        aloge("UVC Construct error!");
        goto ERR_EXIT0;
    }
#endif
    // todo

    gSysManager.mState = MPI_SYS_STATE_STARTED;
    return SUCCESS;

ERR_EXIT0:
#ifdef USE_LIBCEDARC_MEM_ALLOC
    CdcMemClose(gSysManager.mMemOps);
#else
    ion_memClose();
#endif
    return eError;
}

ERRORTYPE AW_MPI_SYS_Exit(void)
{
    ERRORTYPE ret = SUCCESS;

    if (gSysManager.mState != MPI_SYS_STATE_STARTED) {
        return SUCCESS;
    }
#if (MPPCFG_AENC == OPTION_AENC_ENABLE)
    ret = AENC_Destruct();
    if (ret != SUCCESS) {
        aloge("AENC_Destroy error!");
    }
#endif
#if (MPPCFG_ADEC == OPTION_ADEC_ENABLE)
    ret = ADEC_Destruct();
    if (ret != SUCCESS) {
        aloge("AW_MPI_AENC_PRIV_Destroy error!");
    }
#endif
#if (MPPCFG_AIO == OPTION_AIO_ENABLE)
    ret = audioHw_Destruct();
    if (ret != SUCCESS) {
        aloge("audioHw_Destruct error!");
    }
#endif
#if (MPPCFG_TEXTENC == OPTION_TEXTENC_ENABLE)
    ret = TENC_Destruct();
    if (ret != SUCCESS)
    {
        aloge("TENC_Destroy error!");
    }
#endif
#if (MPPCFG_MUXER == OPTION_MUXER_ENABLE)
    ret = MUX_Destruct();
    if (ret != SUCCESS) {
        aloge("MUX_Destroy error!");
    }
#endif
#if (MPPCFG_DEMUXER == OPTION_DEMUXER_ENABLE)
    ret = DEMUX_Destruct();
    if (ret != SUCCESS) {
        aloge("DEMUX_Destroy error!");
    }
#endif
#if (MPPCFG_VENC == OPTION_VENC_ENABLE)
    ret = VENC_Destruct();
    if (ret != SUCCESS) {
        aloge("AW_MPI_VENC_PRIV_Destroy error!");
    }
#endif
#if (MPPCFG_VDEC == OPTION_VDEC_ENABLE)
    ret = VDEC_Destruct();
    if (ret != SUCCESS) {
        aloge("AW_MPI_VDEC_PRIV_Destroy error!");
    }
#endif
#if (MPPCFG_EIS == OPTION_EIS_ENABLE)
    ret = EIS_Destruct();
    if (ret != SUCCESS) {
        aloge("AW_MPI_EIS_Destroy error!");
    }
#endif
#if (MPPCFG_ISE == OPTION_ISE_ENABLE)
    ret = ISE_Destruct();
    if (ret != SUCCESS) {
        aloge("AW_MPI_ISE_Destroy error!");
    }
#endif
#ifdef MPPCFG_UVC
    ret = UVC_Destruct();
    if (ret != SUCCESS) {
        aloge("AW_MPI_UVC_Destroy error!");
    }
#endif
#if (MPPCFG_VO == OPTION_VO_ENABLE)
    ret = VO_Destruct();
    if (ret != SUCCESS)
    {
        aloge("AW MPI VO Destruct error!");
    }
#endif
    ret = RegionManager_Destruct();
    if (ret != SUCCESS)
    {
        aloge("AW MPI RGN Destruct error!");
    }

    // todo

#ifdef USE_LIBCEDARC_MEM_ALLOC
    CdcMemClose(gSysManager.mMemOps);
#else
    ion_memClose();
    ion_iommu_over();
#endif
#if (MPPCFG_VI == OPTION_VI_ENABLE)
    AW_MPI_ISP_Exit();
    AW_MPI_VI_Exit();
#endif
    gSysManager.mState = MPI_SYS_STATE_CONFIGURED;
    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_Bind(MPP_CHN_S* pSrcChn, MPP_CHN_S* pDestChn)
{
    MM_COMPONENTTYPE *pSrcComp = NULL, *pDstComp = NULL;
    unsigned int srcPortIdx, dstPortIdx;
    ERRORTYPE eError = SUCCESS;

    if (pSrcChn == NULL || pDestChn == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

    SYS_GetComp(pSrcChn, &pSrcComp);
    if (pSrcComp == NULL) {
        aloge("fatal error! Bind error! source component not found!");
        return FAILURE;
    }

    SYS_GetComp(pDestChn, &pDstComp);
    if (pDstComp == NULL) {
        aloge("Bind error! dest component not found!");
        return FAILURE;
    }

    eError = SYS_QueryBindRelation(pSrcChn, pDestChn);
    if (eError == FAILURE)
    {
        aloge("Bind type error! SrcMod[0x%x], DestChn[0x%x]", pSrcChn->mModId, pDestChn->mModId);
        return FAILURE;
    }

    SYS_DecideBindPortIndex(pSrcChn, pDestChn, &srcPortIdx, &dstPortIdx, NULL);
    eError = COMP_SetupTunnel(pSrcComp, srcPortIdx, pDstComp, dstPortIdx);
    if (eError != SUCCESS)
    {
        aloge("COMP_SetupTunnel error! SrcChn[0x%x][%d][%d], DstChn[0x%x][%d][%d]",
            pSrcChn->mModId, pSrcChn->mDevId, pSrcChn->mChnId, pDestChn->mModId, pDestChn->mDevId, pDestChn->mChnId);
        return eError;
    }

    return SUCCESS;
}

ERRORTYPE  AW_MPI_SYS_BindExt(MPP_CHN_S* pSrcChn, MPP_CHN_S* pDestChn, MppBindControl* pBindControl)
{
    MM_COMPONENTTYPE *pSrcComp = NULL, *pDstComp = NULL;
    unsigned int srcPortIdx, dstPortIdx;
    ERRORTYPE eError = SUCCESS;

    if (pSrcChn == NULL || pDestChn == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

    SYS_GetComp(pSrcChn, &pSrcComp);
    if (pSrcComp == NULL) {
        aloge("fatal error! Bind error! source component not found!");
        return FAILURE;
    }

    SYS_GetComp(pDestChn, &pDstComp);
    if (pDstComp == NULL) {
        aloge("Bind error! dest component not found!");
        return FAILURE;
    }

    eError = SYS_QueryBindRelation(pSrcChn, pDestChn);
    if (eError == FAILURE)
    {
        aloge("Bind type error! SrcMod[0x%x], DestChn[0x%x]", pSrcChn->mModId, pDestChn->mModId);
        return FAILURE;
    }

    SYS_DecideBindPortIndex(pSrcChn, pDestChn, &srcPortIdx, &dstPortIdx, pBindControl);
    eError = COMP_SetupTunnel(pSrcComp, srcPortIdx, pDstComp, dstPortIdx);
    if (eError != SUCCESS)
    {
        aloge("COMP_SetupTunnel error! SrcChn[0x%x][%d][%d], DstChn[0x%x][%d][%d]",
            pSrcChn->mModId, pSrcChn->mDevId, pSrcChn->mChnId, pDestChn->mModId, pDestChn->mDevId, pDestChn->mChnId);
        return eError;
    }

    return SUCCESS;
}


ERRORTYPE  AW_MPI_SYS_UnBind(MPP_CHN_S* pSrcChn, MPP_CHN_S* pDestChn)
{
    MM_COMPONENTTYPE *pSrcComp = NULL, *pDstComp = NULL;
    unsigned int srcPortIdx, dstPortIdx;
    ERRORTYPE eError = SUCCESS;

    if (pSrcChn == NULL || pDestChn == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

    SYS_GetComp(pSrcChn, &pSrcComp);
    if (pSrcComp == NULL) {
        aloge("UnBind error! source component not found!");
        return FAILURE;
    }
    COMP_STATETYPE srcState;
    pSrcComp->GetState(pSrcComp, &srcState);

    SYS_GetComp(pDestChn, &pDstComp);
    if (pDstComp == NULL) {
        aloge("UnBind error! dest component not found!");
        return FAILURE;
    }
    COMP_STATETYPE dstState;
    pDstComp->GetState(pDstComp, &dstState);

    if((COMP_StateLoaded == srcState && COMP_StateLoaded == dstState)
        || (COMP_StateIdle == srcState && COMP_StateIdle == dstState))
    {
    }
    else
    {
        aloge("fatal error! srcChn[%d,%d,%d] and dstChn[%d,%d,%d] state not match [%d]!=[%d], can't unbind!",
            pSrcChn->mModId, pSrcChn->mDevId, pSrcChn->mChnId, pDestChn->mModId, pDestChn->mDevId, pDestChn->mChnId,
            srcState, dstState);
        return FAILURE;
    }

    SYS_DecideBindPortIndex(pSrcChn, pDestChn, &srcPortIdx, &dstPortIdx, NULL);
    eError = COMP_ResetTunnel(pSrcComp, srcPortIdx, pDstComp, dstPortIdx);
    if (eError != SUCCESS) {
        aloge("COMP_SetupTunnel error!");
        return eError;
    }

    return SUCCESS;
}

ERRORTYPE  AW_MPI_SYS_GetBindbyDest(MPP_CHN_S* pDestChn, MPP_CHN_S* pSrcChn)
{
    MM_COMPONENTTYPE *pDstComp = NULL, *pSrcComp = NULL;
    ERRORTYPE eError = SUCCESS;

    if (pSrcChn == NULL || pDestChn == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

    SYS_GetComp(pDestChn, &pDstComp);
    if (pDstComp == NULL) {
        aloge("GetBindbyDest error! dest component not found!");
        return FAILURE;
    }
    //get input port tunnel info of DestChn
    COMP_PARAM_PORTDEFINITIONTYPE portDef;
    portDef.nPortIndex = 0;
    pDstComp->GetConfig(pDstComp, COMP_IndexParamPortDefinition, &portDef);
    if(portDef.eDir != COMP_DirInput)
    {
        aloge("fatal error! portIndex[%d] of ModId[0x%x] is not inputPort?", portDef.nPortIndex, pDestChn->mModId);
    }
    COMP_INTERNAL_TUNNELINFOTYPE tunnel;
    tunnel.nPortIndex = portDef.nPortIndex;
    eError = pDstComp->GetConfig(pDstComp, COMP_IndexVendorTunnelInfo, &tunnel);
    if (eError != SUCCESS) {
        aloge("get tunnel info error!");
        return eError;
    }
    pSrcComp = (MM_COMPONENTTYPE*)tunnel.hTunnel;
    eError = pSrcComp->GetConfig(pSrcComp, COMP_IndexVendorMPPChannelInfo, pSrcChn);
    if (eError != SUCCESS) {
        aloge("get mpp channel info error!");
        return eError;
    }

    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_GetVersion(MPP_VERSION_S* pstVersion)
{
    if (pstVersion == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

    // todo

    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_GetCurPts(uint64_t* pu64CurPts)
{
    if (pu64CurPts == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

    *pu64CurPts = CDX_GetTimeUs();

    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_InitPtsBase(uint64_t u64PtsBase)
{
    if (CDX_SetTimeUs(u64PtsBase) != 0) {
        aloge("CDX_SetTimeUs error(%s)!", strerror(errno));
        return ERR_SYS_NOT_PERM;
    }

    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_SyncPts(uint64_t u64PtsBase)
{
    if (CDX_SetTimeUs(u64PtsBase) != 0) {
        aloge("SyncPts error(%s)!", strerror(errno));
        return ERR_SYS_NOT_PERM;
    }

    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_MmzAlloc_Cached(unsigned int* pPhyAddr, void** ppVirtAddr, unsigned int uLen)
{
    if (pPhyAddr == NULL || ppVirtAddr == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

#ifdef USE_LIBCEDARC_MEM_ALLOC
    *ppVirtAddr = CdcMemPalloc(gSysManager.mMemOps, uLen);
#else
    //*ppVirtAddr = ion_allocMem(uLen);
    IonAllocAttr stAttr;
    memset(&stAttr, 0, sizeof(IonAllocAttr));
    stAttr.mLen = uLen;
    stAttr.mAlign = 0;
    stAttr.mIonHeapType = IonHeapType_IOMMU;
    stAttr.mbSupportCache = 1;
    *ppVirtAddr = ion_allocMem_extend(&stAttr);
#endif
    if (*ppVirtAddr == NULL) {
        aloge("MemAdapterPalloc error!");
        return ERR_SYS_NOMEM;
    }

#ifdef USE_LIBCEDARC_MEM_ALLOC
    *pPhyAddr = (unsigned int)CdcMemGetPhysicAddressCpu(gSysManager.mMemOps, *ppVirtAddr);
#else
    *pPhyAddr = ion_getMemPhyAddr(*ppVirtAddr);
#endif

    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_MmzFree(unsigned int uPhyAddr, void* pVirtAddr)
{
    if (pVirtAddr == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

#ifdef USE_LIBCEDARC_MEM_ALLOC
    CdcMemPfree(gSysManager.mMemOps, pVirtAddr);
#else
    ion_freeMem(pVirtAddr);
#endif
    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_MmzFlushCache(unsigned int u32PhyAddr, void* pVitAddr, unsigned int u32Size)
{
    if (pVitAddr == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

#ifdef USE_LIBCEDARC_MEM_ALLOC
    CdcMemFlushCache(gSysManager.mMemOps, pVitAddr, u32Size);
#else
    ion_flushCache(pVitAddr, u32Size);
#endif
    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_MmzFlushCache_check(unsigned int u32PhyAddr, void* pVitAddr, unsigned int u32Size, BOOL bCheck)
{
    if (pVitAddr == NULL)
    {
        return ERR_SYS_ILLEGAL_PARAM;
    }
    return ion_flushCache_check(pVitAddr, u32Size, (int)bCheck);
}

ERRORTYPE AW_MPI_SYS_SetReg(unsigned int u32Addr, unsigned int u32Value)
{
    SYS_WriteReg(u32Addr, u32Value);
    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_GetReg(unsigned int u32Addr, unsigned int* pu32Value)
{
    if (pu32Value == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

    *pu32Value = SYS_ReadReg(u32Addr);
    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_SetProfile(PROFILE_TYPE_E enProfile)
{
    // todo

    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_GetVirMemInfo(const void* pVitAddr, SYS_VIRMEM_INFO_S* pstMemInfo)
{
    void *pPhyAddr = NULL;

    if (pstMemInfo == NULL) {
        return ERR_SYS_ILLEGAL_PARAM;
    }

#ifdef USE_LIBCEDARC_MEM_ALLOC
    pPhyAddr = CdcMemGetPhysicAddressCpu(gSysManager.mMemOps, (void*)pVitAddr);
#else
    pPhyAddr = (void*)ion_getMemPhyAddr((void*)pVitAddr);
#endif

    if (pPhyAddr == NULL) {
        pstMemInfo->u32PhyAddr = 0;
        pstMemInfo->bCached = FALSE;
    } else {
        pstMemInfo->u32PhyAddr = (unsigned int)pPhyAddr;
        pstMemInfo->bCached = TRUE;
    }

    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_HANDLE_ZERO(handle_set *pHandleSet)
{
    FD_ZERO(pHandleSet);
    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_HANDLE_SET(int handle, handle_set *pHandleSet)
{
    FD_SET(handle, pHandleSet);
    return SUCCESS;
}

ERRORTYPE AW_MPI_SYS_HANDLE_ISSET(int handle, handle_set *pHandleSet)
{
    return FD_ISSET(handle, pHandleSet);
}

int AW_MPI_SYS_HANDLE_Select(int nHandles, handle_set *pRdFds, int nMilliSecs)
{
    int retval;
    struct timeval timeout, *pTmOut;

    if (nMilliSecs>=0) {
        timeout.tv_sec = nMilliSecs/1000;
        timeout.tv_usec = (nMilliSecs%1000)*1000;
        pTmOut = &timeout;
    } else {
        pTmOut = NULL;
    }

    retval = select(nHandles, pRdFds, NULL, NULL, pTmOut);
    if (retval == -1) {
        aloge("HANDLE_Select error! retVal:%d", retval);
    } else if (retval == 0) {
        //alogw("mpi select timeout(%d ms)", nMilliSecs);
    } else {
        //alogd("some handle can be read");
    }

    return retval;
}
