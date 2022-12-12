/******************************************************************************
 Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
 File Name     : VideoEnc_Component.c
 Version       : Initial Draft
 Author        : Allwinner BU3-PD2 Team
 Created       : 2016/05/09
 Last Modified :
 Description   : mpp component implement
 Function List :
 History       :
 ******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "VideoIse_Component"
#include <utils/plat_log.h>

// ref platform headers
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include "cdx_list.h"
#include "plat_defines.h"
#include "plat_errno.h"
#include "plat_math.h"
#include "plat_type.h"
#include <sys/time.h>
// media api headers to app
#include "SystemBase.h"
#include "mm_comm_ise.h"
#include "mm_comm_vi.h"
#include "mm_comm_video.h"
#include "mm_common.h"
#include "mpi_ise.h"

// media internal common headers.
#include "ComponentCommon.h"
#include "media_common.h"
#include "mm_component.h"
#include "tmessage.h"
#include "tsemaphore.h"
#include "ion_memmanager.h"
#include <VideoISECompStream.h>
#include "VideoISE_Component.h"
#include "memoryAdapter.h"
#include "sc_interface.h"
#include <ConfigOption.h>
//#define VIDEO_ISE_PROC_TIME_DEBUG
//#define VIDEO_ISE_SETMOATTR_TIME_DEBUG
//#define VIDEO_ISE_SETBIATTR_TIME_DEBUG
//#define VIDEO_ISE_CREATSTIHANDLE_TIME_DEBUG
//#define VIDEO_TakePicture_Debug_Time

static void *VideoIse_ComponentThread(void *pThreadData);
static float timeuse_setmoattr = 0;
#define FISH_LIB    1
#define DFISH_LIB   1
#define ISE_LIB     1

static ERRORTYPE VideoIse_OutputBufferInit(VIDEOISEDATATYPE *pVideoIseData, ISEChnNode_t *pIseChnData)
{
    ISEOutBuf_t *pOutBuf = NULL;
    IonAllocAttr stIonAllocAttr;
    int width = 0, height = 0;

    for (int i = 0; i < pIseChnData->mIseOutBufNum; i++)
    {
        pOutBuf = (ISEOutBuf_t *)malloc(sizeof(ISEOutBuf_t));
        memset(pOutBuf, 0, sizeof(ISEOutBuf_t));
        INIT_LIST_HEAD(&pOutBuf->mList);
        if (ISEMODE_ONE_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode || \
            ISEMODE_TWO_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
        {
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
            memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
            if (Warp_BirdsEye == pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode)
            {
                int ldc_width, ldc_height;
                width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.birdsEyeImage.width;
                height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.birdsEyeImage.height;
                ldc_width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.width;
                ldc_height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.height;
                stIonAllocAttr.mLen = ALIGN(width, 16)*ALIGN(height, 16) + \
                    ALIGN(ldc_width, 16)*ALIGN(ldc_height, 16);
                alogd("Warp_BirdsEye ISE GDC out image ionallcomem birdsEyeImage:%d*%d + undistImage:%d*%d!!!", \
                    width,height,ldc_width,ldc_height);
            }
            else
            {
                width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.width;
                height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.height;
                stIonAllocAttr.mLen = ALIGN(width, 16)*ALIGN(height, 16);
                alogd("ISE GDC out image ionallcomem W*H=%d*%d!!!",width,height);
            }
            stIonAllocAttr.mAlign = 0;
            stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
            stIonAllocAttr.mbSupportCache = 0;
            pOutBuf->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn] = ion_allocMem_extend(&stIonAllocAttr);
            if (NULL == pOutBuf->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn])
            {
                aloge("Add ise chn%d, Palloc pano_luma failed.", pIseChnData->mIseChn);
            }
            memset(pOutBuf->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn], 0x0, stIonAllocAttr.mLen);
            pOutBuf->mIseGdcOut.out_luma_phy_Addr[pIseChnData->mIseChn] = \
                (unsigned int)ion_getMemPhyAddr(pOutBuf->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn]);
            pOutBuf->VFrame.VFrame.mpVirAddr[0] = pOutBuf->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn];
            pOutBuf->VFrame.VFrame.mPhyAddr[0] = pOutBuf->mIseGdcOut.out_luma_phy_Addr[pIseChnData->mIseChn];
            memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
            if (Warp_BirdsEye == pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode)
            {
                int ldc_width, ldc_height;
                ldc_width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.width;
                ldc_height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.height;
                stIonAllocAttr.mLen = (ALIGN(width, 16)*ALIGN(height, 16)/2 + \
                    ALIGN(ldc_width, 16)*ALIGN(ldc_height, 16)/2);
            }
            else
            {
                stIonAllocAttr.mLen = (ALIGN(width, 16)*ALIGN(height, 16))/2;
            }
            stIonAllocAttr.mAlign = 0;
            stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
            stIonAllocAttr.mbSupportCache = 0;
            pOutBuf->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = ion_allocMem_extend(&stIonAllocAttr);
            if (NULL == pOutBuf->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn])
            {
                aloge("Add ise chn%d, Palloc pano_chroma_u failed.", pIseChnData->mIseChn);
            }
            memset(pOutBuf->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn], 0x0, sizeof(stIonAllocAttr.mLen));
            pOutBuf->VFrame.VFrame.mpVirAddr[1] = pOutBuf->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn];
            pOutBuf->VFrame.VFrame.mPhyAddr[1] = pOutBuf->mIseGdcOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn];
#endif
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
            width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_cfg.out_chroma_pitch[pIseChnData->mIseChn];
            height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_cfg.out_h[pIseChnData->mIseChn];
            memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
            stIonAllocAttr.mLen = ALIGN(width, 16)*ALIGN(height, 16);
            stIonAllocAttr.mAlign = 0;
            stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
            stIonAllocAttr.mbSupportCache = 0;
            pOutBuf->mIseMoOut.out_luma_mmu_Addr[pIseChnData->mIseChn] = ion_allocMem_extend(&stIonAllocAttr);
            if (NULL == pOutBuf->mIseMoOut.out_luma_mmu_Addr[pIseChnData->mIseChn])
            {
                aloge("Add ise chn%d, Palloc pano_luma failed.", pIseChnData->mIseChn);
            }
            pOutBuf->mIseMoOut.out_luma_phy_Addr[pIseChnData->mIseChn] = \
                (unsigned int)ion_getMemPhyAddr(pOutBuf->mIseMoOut.out_luma_mmu_Addr[pIseChnData->mIseChn]);
            pOutBuf->VFrame.VFrame.mpVirAddr[0] = pOutBuf->mIseMoOut.out_luma_mmu_Addr[pIseChnData->mIseChn];
            pOutBuf->VFrame.VFrame.mPhyAddr[0] = pOutBuf->mIseMoOut.out_luma_phy_Addr[pIseChnData->mIseChn];
            memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
            stIonAllocAttr.mLen = (ALIGN(width, 16)*ALIGN(height, 16))/2;
            stIonAllocAttr.mAlign = 0;
            stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
            stIonAllocAttr.mbSupportCache = 0;
            pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = ion_allocMem_extend(&stIonAllocAttr);
            if (NULL == pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn])
            {
                aloge("Add ise chn%d, Palloc pano_chroma_u failed.", pIseChnData->mIseChn);
            }
            memset(pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr, 0x0, stIonAllocAttr.mLen);
            pOutBuf->mIseMoOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn] = \
                ion_getMemPhyAddr(pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn]);
            pOutBuf->VFrame.VFrame.mpVirAddr[1] = pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn];
            pOutBuf->VFrame.VFrame.mPhyAddr[1] = pOutBuf->mIseMoOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn];
#endif
#if MPPCFG_ISE_TWO_FISHEYE
            width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_cfg.out_chroma_pitch[pIseChnData->mIseChn];
            height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_cfg.out_h[pIseChnData->mIseChn];
            memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
            stIonAllocAttr.mLen = ALIGN(width, 16)*ALIGN(height, 16);
            stIonAllocAttr.mAlign = 0;
            stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
            stIonAllocAttr.mbSupportCache = 0;
            pOutBuf->mIseBiOut.out_luma_mmu_Addr[pIseChnData->mIseChn] = ion_allocMem_extend(&stIonAllocAttr);
            if (NULL == pOutBuf->mIseMoOut.out_luma_mmu_Addr[pIseChnData->mIseChn])
            {
                aloge("Add ise chn%d, Palloc pano_luma failed.", pIseChnData->mIseChn);
            }
            pOutBuf->mIseMoOut.out_luma_phy_Addr[pIseChnData->mIseChn] = \
                (unsigned int)ion_getMemPhyAddr(pOutBuf->mIseMoOut.out_luma_mmu_Addr[pIseChnData->mIseChn]);
            pOutBuf->VFrame.VFrame.mpVirAddr[0] = pOutBuf->mIseMoOut.out_luma_mmu_Addr[pIseChnData->mIseChn];
            pOutBuf->VFrame.VFrame.mPhyAddr[0] = pOutBuf->mIseMoOut.out_luma_phy_Addr[pIseChnData->mIseChn];
            memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
            stIonAllocAttr.mLen = (ALIGN(width, 16)*ALIGN(height, 16))/2;
            stIonAllocAttr.mAlign = 02;
            stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
            stIonAllocAttr.mbSupportCache = 0;
            pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = ion_allocMem_extend(&stIonAllocAttr);
            if (NULL == pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn])
            {
                aloge("Add ise chn%d, Palloc pano_chroma_u failed.", pIseChnData->mIseChn);
            }
            memset(pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr, 0x0, stIonAllocAttr.mLen);
            pOutBuf->mIseMoOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn] = \
                ion_getMemPhyAddr(pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn]);
            pOutBuf->VFrame.VFrame.mpVirAddr[1] = pOutBuf->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn];
            pOutBuf->VFrame.VFrame.mPhyAddr[1] = pOutBuf->mIseMoOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn];
#endif
        }
        else if (ISEMODE_TWO_ISE == pVideoIseData->mIseGrpAttr.iseMode)
        {
#if MPPCFG_ISE_TWO_ISE
            width = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_cfg.pano_chroma_pitch;
            height = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_cfg.pano_h;
            if (0 == pIseChnData->mIseChn)
            {
                memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
                stIonAllocAttr.mLen = width*height;
                stIonAllocAttr.mAlign = 0;
                stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
                stIonAllocAttr.mbSupportCache = 0;
                pOutBuf->mIseStiOut.pano_luma_mmu_Addr = ion_allocMem_extend(&stIonAllocAttr);
                if (NULL == pOutBuf->mIseStiOut.pano_luma_mmu_Addr)
                {
                    aloge("Add ise chn%d, Palloc pano_luma failed.", pIseChnData->mIseChn);
                }
                memset(pOutBuf->mIseStiOut.pano_luma_mmu_Addr, 0x0, stIonAllocAttr.mLen);
                pOutBuf->mIseStiOut.pano_luma_phy_Addr = \
                    (unsigned int)ion_getMemPhyAddr(pOutBuf->mIseStiOut.pano_luma_mmu_Addr);
                pOutBuf->VFrame.VFrame.mpVirAddr[0] = pOutBuf->mIseStiOut.pano_luma_mmu_Addr;
                pOutBuf->VFrame.VFrame.mPhyAddr[0] = pOutBuf->mIseStiOut.pano_luma_phy_Addr;
                memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
                stIonAllocAttr.mLen = width * height / 2;
                stIonAllocAttr.mAlign = 0;
                stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
                stIonAllocAttr.mbSupportCache = 0;
                pOutBuf->mIseStiOut.pano_chroma_mmu_Addr = ion_allocMem_extend(&stIonAllocAttr);
                if (NULL == pOutBuf->mIseStiOut.pano_chroma_mmu_Addr)
                {
                    aloge("Add ise chn%d, Palloc pano_chroma failed.", pIseChnData->mIseChn);
                }
                pOutBuf->mIseStiOut.pano_chroma_phy_Addr = \
                    (unsigned int)ion_getMemPhyAddr(pOutBuf->mIseStiOut.pano_chroma_mmu_Addr);
                pOutBuf->VFrame.VFrame.mpVirAddr[1] = pOutBuf->mIseStiOut.pano_chroma_mmu_Addr;
                pOutBuf->VFrame.VFrame.mPhyAddr[1] = pOutBuf->mIseStiOut.pano_chroma_phy_Addr;
            }
            else if (1 <= pIseChnData->mIseChn && 3 >= pIseChnData->mIseChn)
            {
                memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
                stIonAllocAttr.mLen = width * height;
                stIonAllocAttr.mAlign = 0;
                stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
                stIonAllocAttr.mbSupportCache = 0;
                pOutBuf->mIseStiOut.scalar_luma_mmu_Addr[pIseChnData->mIseChn-1] = \
                    ion_allocMem_extend(&stIonAllocAttr);
                if (NULL == pOutBuf->mIseStiOut.scalar_luma_mmu_Addr[pIseChnData->mIseChn-1])
                {
                    aloge("Add ise chn%d, Palloc scalar_luma failed.", pIseChnData->mIseChn);
                }
                pOutBuf->mIseStiOut.scalar_luma_phy_Addr[pIseChnData->mIseChn-1] = \
                    (unsigned int)ion_getMemPhyAddr(pOutBuf->mIseStiOut.scalar_luma_mmu_Addr[pIseChnData->mIseChn-1]);
                pOutBuf->VFrame.VFrame.mpVirAddr[0] = \
                    pOutBuf->mIseStiOut.scalar_luma_mmu_Addr[pIseChnData->mIseChn-1];
                pOutBuf->VFrame.VFrame.mPhyAddr[0] = \
                    pOutBuf->mIseStiOut.scalar_luma_phy_Addr[pIseChnData->mIseChn-1];
                memset(&stIonAllocAttr, 0, sizeof(IonAllocAttr));
                stIonAllocAttr.mLen = width * height / 2;
                stIonAllocAttr.mAlign = 0;
                stIonAllocAttr.mIonHeapType = IonHeapType_IOMMU;
                stIonAllocAttr.mbSupportCache = 0;
                pOutBuf->mIseStiOut.scalar_chroma_mmu_Addr[pIseChnData->mIseChn - 1] = \
                    ion_allocMem_extend(&stIonAllocAttr);
                if (NULL == pOutBuf->mIseStiOut.scalar_chroma_mmu_Addr[pIseChnData->mIseChn-1])
                {
                    aloge("Add ise chn%d, Palloc pano_chroma failed.", pIseChnData->mIseChn);
                }
                pOutBuf->mIseStiOut.scalar_chroma_phy_Addr[pIseChnData->mIseChn-1] = \
                    ion_getMemPhyAddr(pOutBuf->mIseStiOut.scalar_chroma_mmu_Addr[pIseChnData->mIseChn-1]);
                pOutBuf->VFrame.VFrame.mpVirAddr[1] = \
                    pOutBuf->mIseStiOut.scalar_chroma_mmu_Addr[pIseChnData->mIseChn-1];
                pOutBuf->VFrame.VFrame.mPhyAddr[1] = \
                    pOutBuf->mIseStiOut.scalar_chroma_phy_Addr[pIseChnData->mIseChn-1];
            }
#endif
        }
        pOutBuf->mIseOutId = (i << 8) | 0xFFFF0000;
        VIDEO_FRAME_INFO_S *pVFrameInfo = &pOutBuf->VFrame;
        pVFrameInfo->mId = i;
        pVFrameInfo->VFrame.mWidth = width;
        pVFrameInfo->VFrame.mHeight = height;
        pVFrameInfo->VFrame.mPixelFormat = pVideoIseData->mIseGrpAttr.mPixelFormat;
        pVFrameInfo->VFrame.mOffsetTop = 0;
        pVFrameInfo->VFrame.mOffsetBottom = height;
        pVFrameInfo->VFrame.mOffsetLeft = 0;
        pVFrameInfo->VFrame.mOffsetRight = height;
        pVFrameInfo->VFrame.mStride[0] = width;
        pVFrameInfo->VFrame.mStride[1] = width;
        alogd("Port[%d]data_index=%d: phy:y=%#x,uv=%#x; vir:y=%p,uv=%p,pixel format=%d",
                pIseChnData->mIseChn, i,
                pOutBuf->VFrame.VFrame.mPhyAddr[0],
                pOutBuf->VFrame.VFrame.mPhyAddr[1],
                pOutBuf->VFrame.VFrame.mpVirAddr[0],
                pOutBuf->VFrame.VFrame.mpVirAddr[1],
                pVFrameInfo->VFrame.mPixelFormat);
        list_move_tail(&pOutBuf->mList, &pIseChnData->mIdleOutYUVList);
    }

    return SUCCESS;
}

static ERRORTYPE VideoIse_OutputBufferDeInit(VIDEOISEDATATYPE *pVideoIseData, ISEChnNode_t *pIseChnData)
{
    ISEOutBuf_t *pEntry = NULL, *pTmp = NULL;
    if (!list_empty(&pIseChnData->mReadyOutYUVList))
    {
        aloge("fatal error! outReadyFrame must be 0,move to idle list");
        list_for_each_entry_safe(pEntry, pTmp, &pIseChnData->mReadyOutYUVList, mList)
        {
           list_move_tail(&pEntry->mList, &pIseChnData->mIdleOutYUVList);
        }
    }
    if (!list_empty(&pIseChnData->mUsedOutYUVList))
    {
        aloge("fatal error! outUsedFrame must be 0,move to idle list");
        list_for_each_entry_safe(pEntry, pTmp, &pIseChnData->mUsedOutYUVList, mList)
        {
            list_move_tail(&pEntry->mList, &pIseChnData->mIdleOutYUVList);
        }
    }
    if (!list_empty(&pIseChnData->mIdleOutYUVList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pIseChnData->mIdleOutYUVList, mList)
        {
            if (NULL != pEntry->VFrame.VFrame.mpVirAddr[0])
            {
                ion_freeMem(pEntry->VFrame.VFrame.mpVirAddr[0]);
                pEntry->VFrame.VFrame.mpVirAddr[0] = NULL;
                pEntry->VFrame.VFrame.mPhyAddr[0] = 0;
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
                pEntry->mIseMoOut.out_luma_mmu_Addr[pIseChnData->mIseChn] = NULL;
                pEntry->mIseMoOut.out_luma_phy_Addr[pIseChnData->mIseChn] = 0;
#endif
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
                pEntry->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn] = NULL;
                pEntry->mIseGdcOut.out_luma_phy_Addr[pIseChnData->mIseChn] = 0;
#endif
#if MPPCFG_ISE_TWO_FISHEYE
                pEntry->mIseBiOut.out_luma_mmu_Addr[pIseChnData->mIseChn] = NULL;
                pEntry->mIseBiOut.out_luma_phy_Addr[pIseChnData->mIseChn] = 0;
#endif
#if MPPCFG_ISE_TWO_ISE
                if (0 == pIseChnData->mIseChn)
                {
                    pEntry->mIseStiOut.pano_luma_mmu_Addr = NULL;
                    pEntry->mIseStiOut.pano_luma_phy_Addr = 0;
                }
                else if (1 <= pIseChnData->mIseChn && 3 >= pIseChnData->mIseChn)
                {
                    pEntry->mIseStiOut.scalar_luma_mmu_Addr[pIseChnData->mIseChn-1] = NULL;
                    pEntry->mIseStiOut.scalar_luma_phy_Addr[pIseChnData->mIseChn-1] = 0 ;
                }
#endif
            }
            if (NULL != pEntry->VFrame.VFrame.mpVirAddr[1])
            {
                ion_freeMem(pEntry->VFrame.VFrame.mpVirAddr[1]);
                pEntry->VFrame.VFrame.mpVirAddr[1] = NULL;
                pEntry->VFrame.VFrame.mPhyAddr[1] = 0;
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
                pEntry->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = NULL;
                pEntry->mIseMoOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn] = 0;
#endif
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
                pEntry->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = NULL;
                pEntry->mIseGdcOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn] = 0;
#endif
#if MPPCFG_ISE_TWO_FISHEYE
                pEntry->mIseBiOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = NULL;
                pEntry->mIseBiOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn] = 0;
#endif
#if MPPCFG_ISE_TWO_ISE
                if (0 == pIseChnData->mIseChn)
                {
                    pEntry->mIseStiOut.pano_chroma_mmu_Addr = NULL;
                    pEntry->mIseStiOut.pano_chroma_phy_Addr = 0;
                }
                else if (1 <= pIseChnData->mIseChn && 3 >= pIseChnData->mIseChn)
                {
                    pEntry->mIseStiOut.scalar_chroma_mmu_Addr[pIseChnData->mIseChn-1] = NULL;
                    pEntry->mIseStiOut.scalar_chroma_phy_Addr[pIseChnData->mIseChn-1] = 0 ;
                }
#endif
            }
        }
    }

    return SUCCESS;
}

static ERRORTYPE VideoIse_GetIdleOutputBuffer(VIDEOISEDATATYPE *pVideoIseData, ISEChnNode_t *pIseChnData, ISEOutBuf_t **pOutBuf)
{
    int width, height, ldc_width, ldc_height, ret;
    ISEOutBuf_t *pOutBufTmp = NULL;

    if (!list_empty(&pIseChnData->mIdleOutYUVList))
    {
        *pOutBuf = list_first_entry_or_null(&pIseChnData->mIdleOutYUVList, ISEOutBuf_t, mList);
        if (NULL == *pOutBuf)
        {
            aloge("fatal error! get Idle output buffer fail!");
            return FAILURE;
        }
        pOutBufTmp = *pOutBuf;
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
        if (Warp_BirdsEye == pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode)
        {
            width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.birdsEyeImage.width;
            height= pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.birdsEyeImage.height;
            ldc_width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.width;
            ldc_height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.height;
            ion_flushCache(pOutBufTmp->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn], \
                width*height+ldc_width*ldc_height);
            ion_flushCache(pOutBufTmp->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn], \
                ((width*height)/2)+((ldc_width*ldc_height)/2));
        }
        else
        {
            width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.width;
            height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.height;
            ion_flushCache(pOutBufTmp->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn], width*height);
            ion_flushCache(pOutBufTmp->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn], width*height/2);
        }
#endif
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
        width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.width;
        height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.height;
        ion_flushCache(pOutBufTmp->mIseMoOut.out_luma_mmu_Addr[pIseChnData->mIseChn], width*height);
        ion_flushCache(pOutBufTmp->mIseMoOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn], width*height/2);
#endif
#if MPPCFG_ISE_TWO_FISHEYE
        width = pVideoIseData->mIseChnAttr.mode_attr.mDFish.ise_cfg.out_luma_pitch[pIseChnData->mIseChn];
        height = pVideoIseData->mIseChnAttr.mode_attr.mDFish.ise_cfg.out_h[pIseChnData->mIseChn];
        ion_flushCache(pOutBufTmp->mIseBiOut.out_luma_mmu_Addr[pIseChnData->mIseChn], width*height);
        ion_flushCache(pOutBufTmp->mIseBiOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn], width*height/2);
#endif
#if MPPCFG_ISE_TWO_ISE
        if (0 == pIseChnData->mIseChn)
        {
            width = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_cfg.pano_luma_pitch;
            height = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_cfg.pano_h;
            ion_flushCache(pOutBufTmp->mIseStiOut.pano_luma_mmu_Addr, width*height);
            ion_flushCache(pOutBufTmp->mIseStiOut.pano_chroma_mmu_Addr, width*height/2);
        }
        else if (1 <= pIseChnData->mIseChn && 3 >= pIseChnData->mIseChn)
        {
            width = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_proccfg.scalar_luma_pitch[pIseChnData->mIseChn-1];
            height = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_proccfg.scalar_h[pIseChnData->mIseChn-1];
            ion_flushCache(pOutBufTmp->mIseStiOut.scalar_luma_mmu_Addr[pIseChnData->mIseChn-1], width*height);
            ion_flushCache(pOutBufTmp->mIseStiOut.scalar_chroma_mmu_Addr[pIseChnData->mIseChn-1], width*height/2);
        }
#endif
    }

    return SUCCESS;
}

#if ((MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE) || (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE))
static ERRORTYPE VideoIse_ConfigIseProcOutBuffer(
                VIDEOISEDATATYPE *pVideoIseData,
                ISEChnNode_t *pIseChnData,
                ISEOutBuf_t *pOutBuf,
                ISE_PROCOUT_PARA_GDC *pIseGdcProcOut,
                ISE_PROCOUT_PARA_GDC *pIseGdcLdcProcOut,
                ISE_PROCOUT_PARA_MO  *pIseMoProcOut)
#endif
#if MPPCFG_ISE_TWO_FISHEYE
static ERRORTYPE VideoIse_ConfigIseProcOutBuffer(
                VIDEOISEDATATYPE *pVideoIseData,
                ISEChnNode_t *pIseChnData,
                ISEOutBuf_t *pOutBuf,
                ISE_PROCOUT_PARA_BI  *pIseBiProcOut)

#endif
#if MPPCFG_ISE_TWO_ISE
static ERRORTYPE VideoIse_ConfigIseProcOutBuffer(
                VIDEOISEDATATYPE *pVideoIseData,
                ISEChnNode_t *pIseChnData,
                ISEOutBuf_t *pOutBuf,
                ISE_PROCOUT_PARA_STI *pIseStiProcOut)
#endif
{
    if (NULL == pOutBuf)
    {
        aloge("fatal error! ISE out buf is NULL!");
        return FAILURE;
    }
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
    if (NULL == pIseGdcProcOut)
    {
        aloge("fatal error! ISE gdc proc out buffer is NULL!");
        return FAILURE;
    }
    pIseGdcProcOut->out_luma_mmu_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn];
    pIseGdcProcOut->out_luma_phy_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseGdcOut.out_luma_phy_Addr[pIseChnData->mIseChn];
    pIseGdcProcOut->out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn];
    pIseGdcProcOut->out_chroma_u_phy_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseGdcOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn];
    if (Warp_BirdsEye == pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode)
    {
        if (NULL == pIseGdcLdcProcOut)
        {
            aloge("fatal error! ISE gdc ldc proc out buffer is NULL!");
            return FAILURE;
        }
        pIseGdcLdcProcOut->out_luma_mmu_Addr[pIseChnData->mIseChn] = \
            pOutBuf->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn];
        pIseGdcLdcProcOut->out_luma_phy_Addr[pIseChnData->mIseChn] = \
            pOutBuf->mIseGdcOut.out_luma_phy_Addr[pIseChnData->mIseChn];
        pIseGdcLdcProcOut->out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = \
            pOutBuf->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn];
        pIseGdcLdcProcOut->out_chroma_u_phy_Addr[pIseChnData->mIseChn] = \
            pOutBuf->mIseGdcOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn];
    }
#endif
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
    if (NULL == pIseMoProcOut)
    {
        aloge("fatal error! ISE mo proc out buffer is NULL!");
        return FAILURE;
    }
    pIseMoProcOut->out_luma_mmu_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseGdcOut.out_luma_mmu_Addr[pIseChnData->mIseChn];
    pIseMoProcOut->out_luma_phy_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseGdcOut.out_luma_phy_Addr[pIseChnData->mIseChn];
    pIseMoProcOut->out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseGdcOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn];
    pIseMoProcOut->out_chroma_u_phy_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseGdcOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn];
#endif
#if MPPCFG_ISE_TWO_FISHEYE
    if (NULL == pIseBiProcOut)
    {
        aloge("fatal error! ISE bi proc out buffer is NULL!");
        return FAILURE;
    }
    pIseBiProcOut->out_luma_mmu_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseBiOut.out_luma_mmu_Addr[pIseChnData->mIseChn];
    pIseBiProcOut->out_luma_phy_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseBiOut.out_luma_phy_Addr[pIseChnData->mIseChn];
    pIseBiProcOut->out_chroma_u_mmu_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseBiOut.out_chroma_u_mmu_Addr[pIseChnData->mIseChn];
    pIseBiProcOut->out_chroma_u_phy_Addr[pIseChnData->mIseChn] = \
        pOutBuf->mIseBiOut.out_chroma_u_phy_Addr[pIseChnData->mIseChn];
#endif
#if MPPCFG_ISE_TWO_ISE
    if (NULL == pIseStiProcOut)
    {
        aloge("fatal error! ISE sti proc out buffer is NULL!");
        return FAILURE;
    }
    if (0 == pIseChnData->mIseChn)
    {
        pIseStiProcOut->pano_luma_mmu_Addr = pOutBuf->mIseStiOut.pano_luma_mmu_Addr;
        pIseStiProcOut->pano_luma_phy_Addr = pOutBuf->mIseStiOut.pano_luma_phy_Addr;
        pIseStiProcOut->pano_chroma_mmu_Addr = pOutBuf->mIseStiOut.pano_chroma_mmu_Addr;
        pIseStiProcOut->pano_chroma_phy_Addr = pOutBuf->mIseStiOut.pano_chroma_phy_Addr;
    }
    else if (1 <= pIseChnData->mIseChn && 3 >= pIseChnData->mIseChn)
    {
        pIseStiProcOut->scalar_luma_mmu_Addr[pIseChnData->mIseChn-1] = \
            pOutBuf->mIseStiOut.scalar_luma_mmu_Addr[pIseChnData->mIseChn-1];
        pIseStiProcOut->scalar_luma_phy_Addr[pIseChnData->mIseChn-1] = \
            pOutBuf->mIseStiOut.scalar_luma_phy_Addr[pIseChnData->mIseChn-1];
        pIseStiProcOut->scalar_chroma_mmu_Addr[pIseChnData->mIseChn-1] = \
            pOutBuf->mIseStiOut.scalar_chroma_mmu_Addr[pIseChnData->mIseChn-1];
        pIseStiProcOut->scalar_chroma_phy_Addr[pIseChnData->mIseChn-1] = \
            pOutBuf->mIseStiOut.scalar_chroma_phy_Addr[pIseChnData->mIseChn-1];
    }
#endif

    return SUCCESS;
}

ERRORTYPE copy_ISE_GRP_ATTR_S(ISE_GROUP_ATTR_S *pDst, ISE_GROUP_ATTR_S *pSrc)
{
    *pDst = *pSrc;
    return SUCCESS;
}

ERRORTYPE copy_ISE_CHN_ATTR_S(ISE_CHN_ATTR_S *pDst, ISE_CHN_ATTR_S *pSrc)
{
    *pDst = *pSrc;
    return SUCCESS;
}

static ERRORTYPE Hal_CreateIseHandle(VIDEOISEDATATYPE *pVideoIseData, IseChnAttr *pChnAttr)
{
    int hresult = 0;
    if (ISEMODE_ONE_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
    {
//#if FISH_LIB
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
        alogd("===>ISE_Create_Mo");
        ISE_CFG_PARA_MO   *fish_default_config = NULL,*pChnAttr_fish_cfg = NULL, *pVideoIseData_fish_cfg = NULL;
        fish_default_config = &pVideoIseData->fish_default_config;
        pChnAttr_fish_cfg = &pChnAttr->pChnAttr->mode_attr.mFish.ise_cfg;
        pVideoIseData_fish_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_cfg;
        // cfg para init
        memcpy(fish_default_config,pChnAttr_fish_cfg,sizeof(ISE_CFG_PARA_MO));
        memcpy(pVideoIseData_fish_cfg,fish_default_config,sizeof(ISE_CFG_PARA_MO));

        pVideoIseData->fish_handle = NULL;
        hresult = ISE_Create_Mo(pVideoIseData_fish_cfg,&pVideoIseData->fish_handle);
        if (0 != hresult)
        {
            if(EN_ERR_EFUSE_ERROR == hresult)
            {
                aloge("mo efuse check fail,return %#010x",hresult);
                return ERR_ISE_EFUSE_ERROR;
            }
            else
            {
                aloge("Create MO handle fail,return %#010x",hresult);
                return ERR_ISE_ILLEGAL_PARAM;
            }
        }
        alogd("%p",pVideoIseData->fish_handle);
#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
        struct timeval tpstart,tpend;
        float timeuse;
        gettimeofday(&tpstart, NULL);
#endif

        hresult = ISE_SetAttr_Mo(&pVideoIseData->fish_handle);
        if (0 != hresult)
        {
            aloge("Set Mo attr fail,return %#010x",hresult);
            return ERR_ISE_ILLEGAL_PARAM;
        }

#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
        gettimeofday(&tpend,NULL);
        timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+(tpend.tv_usec-tpstart.tv_usec);
        timeuse/=1000;
        alogd("------>Initial Used Time:%f ms<------\n", timeuse);
#endif

#endif
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
        alogd("===>ISE_Create_GDC");
        ISE_CFG_PARA_GDC   *fish_gdc_default_config = NULL,*pChnAttr_fish_gdc_cfg = NULL, *pVideoIseData_fish_gdc_cfg = NULL;
        fish_gdc_default_config = &pVideoIseData->fish_gdc_default_config;
        pChnAttr_fish_gdc_cfg = &pChnAttr->pChnAttr->mode_attr.mFish.ise_gdc_cfg;
        pVideoIseData_fish_gdc_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg;
        // cfg para init
        memcpy(fish_gdc_default_config,pChnAttr_fish_gdc_cfg,sizeof(ISE_CFG_PARA_GDC));
        memcpy(pVideoIseData_fish_gdc_cfg,fish_gdc_default_config,sizeof(ISE_CFG_PARA_GDC));

        pVideoIseData->gdc_handle = NULL;
        hresult = ISE_Create_GDC(&pVideoIseData->gdc_handle);
        if(pChnAttr->pChnAttr->mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode == Warp_User)
        {
            alogd("warpMode = Warp_User");
            ISE_SetCallBack_GDC(&pVideoIseData->gdc_handle,pChnAttr->pChnAttr->UserCallback, NULL);
        }
        if (0 != hresult)
        {
            if(EN_ERR_EFUSE_ERROR == hresult)
            {
                aloge("gdc efuse check fail,return %#010x",hresult);
                return ERR_ISE_EFUSE_ERROR;
            }
            else
            {
                aloge("Create GDC handle fail,return %#010x",hresult);
                return ERR_ISE_ILLEGAL_PARAM;
            }
        }
#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
        struct timeval tpstart,tpend;
        float timeuse;
        gettimeofday(&tpstart, NULL);
#endif
        hresult = ISE_SetAttr_GDC(&pVideoIseData->gdc_handle,*pVideoIseData_fish_gdc_cfg);
        if (0 != hresult)
        {
            aloge("Set GDC attr fail,return %#010x",hresult);
            return ERR_ISE_ILLEGAL_PARAM;
        }

#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
        gettimeofday(&tpend,NULL);
        timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+(tpend.tv_usec-tpstart.tv_usec);
        timeuse/=1000;
        alogd("------>Initial Used Time:%f ms<------\n", timeuse);
#endif
#endif
//#endif
    }
    else if (ISEMODE_TWO_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
    {
#if MPPCFG_ISE_TWO_FISHEYE
        ISE_CFG_PARA_BI  *dfish_default_config = NULL,*pChnAttr_dfish_cfg = NULL,*pVideoIseData_dfish_cfg = NULL;
        if(pChnAttr->pChnAttr->mode_attr.mDFish.handle_mode != HANDLE_BY_HARDWARE &&
                pChnAttr->pChnAttr->mode_attr.mDFish.handle_mode != HANDLE_BY_SOFT)
        {
            alogd("bi handle mode:%d",pChnAttr->pChnAttr->mode_attr.mDFish.handle_mode);
            alogd("user not set bi handle mode,use default handle by handware mode.");
            pVideoIseData->dfish_handle_mode = HANDLE_BY_HARDWARE;
        }
        else
        {
            pVideoIseData->dfish_handle_mode = pChnAttr->pChnAttr->mode_attr.mDFish.handle_mode;
        }
        dfish_default_config = &pVideoIseData->dfish_default_config;
        pChnAttr_dfish_cfg = &pChnAttr->pChnAttr->mode_attr.mDFish.ise_cfg;
        pVideoIseData_dfish_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mDFish.ise_cfg;
        // cfg para init
        memcpy(dfish_default_config,pChnAttr_dfish_cfg,sizeof(ISE_CFG_PARA_BI));
        memcpy(pVideoIseData_dfish_cfg,dfish_default_config,sizeof(ISE_CFG_PARA_BI));
    	if(pVideoIseData->dfish_handle_mode == HANDLE_BY_HARDWARE)
    	{
    	    alogd("creat hardware handle");
            pVideoIseData->dfish_handle_by_hardware = NULL;
            hresult = ISE_Create_Bi(pVideoIseData_dfish_cfg,&pVideoIseData->dfish_handle_by_hardware);
            if (0 != hresult)
            {
                if(EN_ERR_EFUSE_ERROR == hresult)
                {
                    aloge("bi efuse check fail,return %#010x",hresult);
                    return ERR_ISE_EFUSE_ERROR;
                }
                else
                {
                    aloge("Create Bi hardware handle fail,return %#010x",hresult);
                    return ERR_ISE_ILLEGAL_PARAM;
                }
            }

#ifdef VIDEO_ISE_SETBIATTR_TIME_DEBUG
        struct timeval tpstart,tpend;
        float timeuse;
        gettimeofday(&tpstart, NULL);
#endif

            hresult = ISE_SetAttr_Bi(&pVideoIseData->dfish_handle_by_hardware);
            if (0 != hresult)
            {
                aloge("Set Bi hardware attr fail,return %#010x",hresult);
                return ERR_ISE_ILLEGAL_PARAM;
            }

#ifdef VIDEO_ISE_SETBIATTR_TIME_DEBUG
        gettimeofday(&tpend,NULL);
        timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+(tpend.tv_usec-tpstart.tv_usec);
        timeuse/=1000;
        alogd("------>Set Bi attr Used Time:%f ms<------\n", timeuse);
#endif
    	}

    	else if(pVideoIseData->dfish_handle_mode == HANDLE_BY_SOFT)
    	{
    	    alogd("creat soft handle");
    	    pVideoIseData->dfish_handle_by_soft = NULL;
    	    hresult = ISE_Create_BI_Soft(pVideoIseData_dfish_cfg, &pVideoIseData->dfish_handle_by_soft);
            if (0 != hresult)
            {
                if(EN_ERR_EFUSE_ERROR == hresult)
                {
                    aloge("bi soft efuse check fail,return %#010x",hresult);
                    return ERR_ISE_EFUSE_ERROR;
                }
                else
                {
                    aloge("Create Bi soft handle fail,return %#010x",hresult);
                    return ERR_ISE_ILLEGAL_PARAM;
                }
            }

#ifdef VIDEO_ISE_SETBIATTR_TIME_DEBUG
        struct timeval tpstart,tpend;
        float timeuse;
        gettimeofday(&tpstart, NULL);
#endif

            hresult = ISE_SetConfig_BI_Soft(&pVideoIseData->dfish_handle_by_soft);
            if (0 != hresult)
            {
                aloge("Set Bi soft config fail,return %#010x",hresult);
                return ERR_ISE_ILLEGAL_PARAM;
            }

#ifdef VIDEO_ISE_SETBIATTR_TIME_DEBUG
        gettimeofday(&tpend,NULL);
        timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+(tpend.tv_usec-tpstart.tv_usec);
        timeuse/=1000;
        alogd("------>Set Bi soft config Used Time:%f ms<------\n", timeuse);
#endif
    	}
#endif
    }
    else if (ISEMODE_TWO_ISE == pVideoIseData->mIseGrpAttr.iseMode)
    {
#if MPPCFG_ISE_TWO_ISE
        ISE  *ise_default_config = NULL,*pChnAttr_ise_cfg = NULL,*pVideoIseData_ise_cfg = NULL;
        ise_default_config = &pVideoIseData->ise_default_config;
        pChnAttr_ise_cfg = &pChnAttr->pChnAttr->mode_attr.mIse;
        pVideoIseData_ise_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mIse;
        // cfg para init
        memcpy(ise_default_config,pChnAttr_ise_cfg,sizeof(ISE));
        memcpy(pVideoIseData_ise_cfg,ise_default_config,sizeof(ISE));
        pVideoIseData->ise_handle = NULL;

#ifdef VIDEO_ISE_CREATSTIHANDLE_TIME_DEBUG
        struct timeval tpstart,tpend;
        float timeuse;
        gettimeofday(&tpstart, NULL);
#endif

        hresult = ISE_Create_Sti(&pVideoIseData_ise_cfg->ise_cfg,&pVideoIseData->ise_handle);
        if (0 != hresult)
        {
            if(EN_ERR_EFUSE_ERROR == hresult)
            {
                aloge("sti efuse check fail,return %#010x",hresult);
                return ERR_ISE_EFUSE_ERROR;
            }
            else
            {
                aloge("Create Sti handle fail,return %#010x",hresult);
                return ERR_ISE_ILLEGAL_PARAM;
            }
        }

#ifdef VIDEO_ISE_CREATSTIHANDLE_TIME_DEBUG
        gettimeofday(&tpend,NULL);
        timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+(tpend.tv_usec-tpstart.tv_usec);
        timeuse/=1000;
        alogd("------>Creat Sti Handle Used Time:%f ms<------\n", timeuse);
#endif

        if(pChnAttr_ise_cfg->ise_proccfg.bgfgmode_en == 1)
        {
            int bgfg_intvl = pChnAttr_ise_cfg->ise_bgfg.bgfg_intvl;
            hresult = ISE_SetConfig_Sti((int)ISE_SETCFG_BGFG_INTVL_PARAM,
                                        &bgfg_intvl, sizeof(int), &pVideoIseData->ise_handle);
            if (0 != hresult)
            {
                aloge("Set Sti bgfg intvl attr fail");
            }
            int getbgd_intvl = pChnAttr_ise_cfg->ise_bgfg.getbgd_intvl;
            hresult = ISE_SetConfig_Sti((int)ISE_SETCFG_GETBGD_INTVL_PARAM,
                                        &getbgd_intvl, sizeof(int), &pVideoIseData->ise_handle);
            if (0 != hresult)
            {
                aloge("Set Sti getbgd intvl attr fail");
            }
            int bgfg_sleep_ms = pChnAttr_ise_cfg->ise_bgfg.bgfg_sleep_ms;
            hresult = ISE_SetConfig_Sti((int)ISE_SETCFG_BGFG_SLEEP_MS_PARAM,
                                        &bgfg_sleep_ms, sizeof(int), &pVideoIseData->ise_handle);
            if (0 != hresult)
            {
                aloge("Set Sti bgfg sleep_ms attr fail");
            }
        }
#endif
    }
    else
    {
        aloge("No ise mode. Must select ise mode. 1.one fish; 2.two fish; 3.ise fish.\r\n");
        return ERR_ISE_ILLEGAL_PARAM;
    }
    return 0;
}

static ERRORTYPE Hal_DestroyIseHandle(VIDEOISEDATATYPE *pVideoIseData, ISE_GROUP_ATTR_S *pGrpAttr)
{
    if(pVideoIseData == NULL)
    {
        aloge("fatal error! pVideoIseData is NULL");
        return ERR_ISE_ILLEGAL_PARAM;
    }
    if (ISEMODE_ONE_FISHEYE == pGrpAttr->iseMode)
    {
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
        if(pVideoIseData->fish_handle == NULL)
        {
            aloge("fatal error! pVideoIseData is NULL");
            return ERR_ISE_ILLEGAL_PARAM;
        }
        alogd("%p",pVideoIseData->fish_handle);
        ISE_Destroy_Mo(&pVideoIseData->fish_handle);
        alogd("ISE_Destroy_Mo end");
        pVideoIseData->fish_handle = NULL;
#endif
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
            if(pVideoIseData->gdc_handle == NULL)
            {
                aloge("fatal error! pVideoIseData is NULL");
                return ERR_ISE_ILLEGAL_PARAM;
            }
            alogd("%p",pVideoIseData->gdc_handle);
            ISE_Destroy_GDC(&pVideoIseData->gdc_handle);
            alogd("ISE_Destroy_GDC end");
            pVideoIseData->gdc_handle = NULL;
#endif

    }
    else if (ISEMODE_TWO_FISHEYE == pGrpAttr->iseMode)
    {
        DFISH  *pChnAttr_dfish_cfg = NULL;
        pChnAttr_dfish_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mDFish;
        if(pVideoIseData->dfish_handle_mode == HANDLE_BY_HARDWARE)
        {
#if MPPCFG_ISE_TWO_FISHEYE
            ISE_Destroy_Bi(&pVideoIseData->dfish_handle_by_hardware);
            pVideoIseData->dfish_handle_by_hardware = NULL;
#endif
        }
        else if(pVideoIseData->dfish_handle_mode == HANDLE_BY_SOFT)
        {
#if MPPCFG_ISE_TWO_FISHEYE
            ISE_Destroy_BI_Soft(&pVideoIseData->dfish_handle_by_soft);
            pVideoIseData->dfish_handle_by_soft = NULL;
#endif
        }
    }
    else if (ISEMODE_TWO_ISE == pGrpAttr->iseMode)
    {
#if MPPCFG_ISE_TWO_ISE
        ISE_Destroy_Sti(&pVideoIseData->ise_handle);
        pVideoIseData->ise_handle = NULL;
#endif
    }
    else
    {
        aloge("No ise mode. Must select ise mode. 1.one fish; 2.two fish; 3.ise.\r\n");
        return -1;
    }
    return SUCCESS;
}

static ERRORTYPE Hal_UpdateIseConfig(VIDEOISEDATATYPE *pVideoIseData, ISE_CHN_ATTR_S *pChnAttr)
{
    unsigned int hresult = -1;

    if (ISEMODE_ONE_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
    {

    }
    else if (ISEMODE_TWO_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
    {

    }
    else if (ISEMODE_TWO_ISE == pVideoIseData->mIseGrpAttr.iseMode)
    {

    }
    else
    {
       aloge("No ise mode. Must select ise mode. 1.one fish; 2.two fish; 3.ise.\r\n");
       return -1;
    }

    return hresult;
}

ERRORTYPE DoVideoIseAddPort(PARAM_IN COMP_HANDLETYPE hComponent, int iseGrpId, int iseChnId, IseChnAttr *pIseChnAttr)
{
    int eRet = ERR_ISE_UNEXIST;
    int i = 0;
    int width = 0, height = 0;
    int ldc_width = 0,ldc_height = 0 ;
    int hresult = 0;
    // find if the same ise chn exist
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if(iseChnId == 0)
    {
        alogd("===>>CreateIseHandle");
        hresult = Hal_CreateIseHandle(pVideoIseData, pIseChnAttr);
        if(hresult != 0)
        {
            aloge("fatal error!,creat hanle failed");
            return ERR_ISE_ILLEGAL_PARAM;
        }
    }
    ISEChnNode_t *pEntry;
    pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
    if (!list_empty(&pVideoIseData->mValidChnAttrList))
    {
        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
        {
            pthread_mutex_lock(&pEntry->mOutYUVListLock);
            if (pEntry->mIseChn == iseChnId)
            {
                eRet = ERR_ISE_EXIST;
            }
            pthread_mutex_unlock(&pEntry->mOutYUVListLock);
        }
    }
    pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);

    // not find ise chn, add a new one
    if (ERR_ISE_UNEXIST == eRet)
    {
        pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
        if (list_empty(&pVideoIseData->mIdleChnAttrList))
        {
            // alogw("Low probability! sinkInfo is not enough, increase one!");
            ISEChnNode_t *pNode = (ISEChnNode_t *)malloc(sizeof(ISEChnNode_t));
            if (pNode)
            {
                memset(pNode, 0, sizeof(ISEChnNode_t));
                INIT_LIST_HEAD(&pNode->mList);
                list_add_tail(&pNode->mList, &pVideoIseData->mIdleChnAttrList);
                pVideoIseData->mIdleChnNodeNum++;
            }
            else
            {
                aloge("fatal error! malloc fail[%s]!", strerror(errno));
                eRet = ERR_ISE_ILLEGAL_PARAM;
                pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);
                return eRet;
            }
        }

        ISEChnNode_t *pEntry = list_first_entry(&pVideoIseData->mIdleChnAttrList, ISEChnNode_t, mList);
        if (ISEMODE_ONE_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
        {
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
            ISE_CFG_PARA_MO *pChnAttr_fish_cfg = NULL,*pVideoIseData_fish_cfg = NULL;

            pVideoIseData_fish_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_cfg;
            pChnAttr_fish_cfg = &pIseChnAttr->pChnAttr->mode_attr.mFish.ise_cfg;
            pVideoIseData_fish_cfg->out_chroma_pitch[iseChnId] = pChnAttr_fish_cfg->out_chroma_pitch[iseChnId];
            pVideoIseData_fish_cfg->out_h[iseChnId] = pChnAttr_fish_cfg->out_h[iseChnId];
            if (1 <= iseChnId && iseChnId <= 3) //chn0~3
            {
                pVideoIseData_fish_cfg->out_en[iseChnId] = pChnAttr_fish_cfg->out_en[iseChnId];
                pVideoIseData_fish_cfg->out_h[iseChnId] = pChnAttr_fish_cfg->out_h[iseChnId];
                pVideoIseData_fish_cfg->out_w[iseChnId] = pChnAttr_fish_cfg->out_w[iseChnId];
                pVideoIseData_fish_cfg->out_flip[iseChnId] = pChnAttr_fish_cfg->out_flip[iseChnId];
                pVideoIseData_fish_cfg->out_mirror[iseChnId] = pChnAttr_fish_cfg->out_mirror[iseChnId];
                pVideoIseData_fish_cfg->out_luma_pitch[iseChnId] = pChnAttr_fish_cfg->out_luma_pitch[iseChnId];
                pVideoIseData_fish_cfg->out_chroma_pitch[iseChnId] = pChnAttr_fish_cfg->out_chroma_pitch[iseChnId];
            }
#endif
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
            ISE_CFG_PARA_GDC *pChnAttr_fish_gdc_cfg = NULL,*pVideoIseData_fish_gdc_cfg = NULL;
            pChnAttr_fish_gdc_cfg = &pIseChnAttr->pChnAttr->mode_attr.mFish.ise_gdc_cfg;
            pVideoIseData_fish_gdc_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg;
            pVideoIseData_fish_gdc_cfg->undistImage.width = pChnAttr_fish_gdc_cfg->undistImage.width;
            pVideoIseData_fish_gdc_cfg->undistImage.height = pChnAttr_fish_gdc_cfg->undistImage.height;
            pVideoIseData_fish_gdc_cfg->birdsEyeImage.width = pChnAttr_fish_gdc_cfg->birdsEyeImage.width;
            pVideoIseData_fish_gdc_cfg->birdsEyeImage.height = pChnAttr_fish_gdc_cfg->birdsEyeImage.height;
            pVideoIseData_fish_gdc_cfg->rectPara.warpMode = pChnAttr_fish_gdc_cfg->rectPara.warpMode;
#endif
        }
        else if (ISEMODE_TWO_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
        {
#if MPPCFG_ISE_TWO_FISHEYE
            ISE_CFG_PARA_BI  *pChnAttr_dfish_cfg = NULL,*pVideoIseData_dfish_cfg = NULL;
            pVideoIseData_dfish_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mDFish.ise_cfg;
            pChnAttr_dfish_cfg = &pIseChnAttr->pChnAttr->mode_attr.mDFish.ise_cfg;
            pVideoIseData_dfish_cfg->out_chroma_pitch[iseChnId] = pChnAttr_dfish_cfg->out_chroma_pitch[iseChnId];
            pVideoIseData_dfish_cfg->out_h[iseChnId] = pChnAttr_dfish_cfg->out_h[iseChnId];
            if (1 <= iseChnId && iseChnId <= 3)
            {
	    		pVideoIseData_dfish_cfg->out_en[iseChnId] = pChnAttr_dfish_cfg->out_en[iseChnId];
	    		pVideoIseData_dfish_cfg->out_h[iseChnId] = pChnAttr_dfish_cfg->out_h[iseChnId];
	    		pVideoIseData_dfish_cfg->out_w[iseChnId] = pChnAttr_dfish_cfg->out_w[iseChnId];
	    		pVideoIseData_dfish_cfg->out_flip[iseChnId] = pChnAttr_dfish_cfg->out_flip[iseChnId];
	    		pVideoIseData_dfish_cfg->out_mirror[iseChnId] = pChnAttr_dfish_cfg->out_mirror[iseChnId];
	    		pVideoIseData_dfish_cfg->out_luma_pitch[iseChnId] = pChnAttr_dfish_cfg->out_luma_pitch[iseChnId];
	    		pVideoIseData_dfish_cfg->out_chroma_pitch[iseChnId] = pChnAttr_dfish_cfg->out_chroma_pitch[iseChnId];
    		}
#endif
        }
        else if (ISEMODE_TWO_ISE == pVideoIseData->mIseGrpAttr.iseMode)
        {
#if MPPCFG_ISE_TWO_ISE
            ISE_PROCCFG_PARA_STI  *pChnAttr_ise_cfg = NULL,*pVideoIseData_ise_cfg = NULL;
            pVideoIseData_ise_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_proccfg;
            pChnAttr_ise_cfg = &pIseChnAttr->pChnAttr->mode_attr.mIse.ise_proccfg;
            if (0 == iseChnId)
            {
                pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_cfg.pano_chroma_pitch = \
                    pIseChnAttr->pChnAttr->mode_attr.mIse.ise_cfg.pano_chroma_pitch;
                pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_cfg.pano_h = \
                    pIseChnAttr->pChnAttr->mode_attr.mIse.ise_cfg.pano_h;
            }
            else if (1 <= iseChnId && iseChnId <= 3)
            {
                pVideoIseData_ise_cfg->scalar_chroma_pitch[iseChnId - 1] = \
                    pChnAttr_ise_cfg->scalar_chroma_pitch[iseChnId - 1];
                pVideoIseData_ise_cfg->scalar_h[iseChnId - 1] = pChnAttr_ise_cfg->scalar_h[iseChnId - 1];
    			pVideoIseData_ise_cfg->scalar_en[iseChnId - 1] = pChnAttr_ise_cfg->scalar_en[iseChnId - 1];
                pVideoIseData_ise_cfg->scalar_h[iseChnId - 1] = pChnAttr_ise_cfg->scalar_h[iseChnId - 1];
                pVideoIseData_ise_cfg->scalar_w[iseChnId - 1] = pChnAttr_ise_cfg->scalar_w[iseChnId - 1];
                pVideoIseData_ise_cfg->scalar_flip[iseChnId - 1] = pChnAttr_ise_cfg->scalar_flip[iseChnId - 1];
                pVideoIseData_ise_cfg->scalar_mirr[iseChnId - 1] = pChnAttr_ise_cfg->scalar_mirr[iseChnId - 1];
                pVideoIseData_ise_cfg->scalar_luma_pitch[iseChnId - 1] = pChnAttr_ise_cfg->scalar_luma_pitch[iseChnId - 1];
                pVideoIseData_ise_cfg->scalar_chroma_pitch[iseChnId - 1] = pChnAttr_ise_cfg->scalar_chroma_pitch[iseChnId - 1];
            }
#endif
        }

        pEntry->mIseGrp = iseGrpId;
        pEntry->mIseChn = iseChnId;
        pEntry->mBgfgmode_enbale = 0;

        INIT_LIST_HEAD(&pEntry->mIdleOutYUVList);
        INIT_LIST_HEAD(&pEntry->mReadyOutYUVList);
        INIT_LIST_HEAD(&pEntry->mUsedOutYUVList);
        pthread_mutex_init(&pEntry->mOutYUVListLock, NULL);
        // pthread_condattr_t  condAttr;
        // pthread_condattr_init(&condAttr);
        // pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
        // pthread_cond_init(&pEntry->mOutFrameFullCondition, &condAttr);
        if(pIseChnAttr->pChnAttr->buffer_num != 0)
        {
            pEntry->mIseOutBufNum = pIseChnAttr->pChnAttr->buffer_num;
            alogd("port%d use user appoint buffer num %d",pEntry->mIseChn,pEntry->mIseOutBufNum);
        }
        else
        {
            pEntry->mIseOutBufNum = 5;
            alogd("port%d use default buffer num %d",pEntry->mIseChn,pEntry->mIseOutBufNum);
        }
        VideoIse_OutputBufferInit(pVideoIseData, pEntry);
        list_move_tail(&pEntry->mList, &pVideoIseData->mValidChnAttrList);
        pVideoIseData->mIdleChnNodeNum--;
        pVideoIseData->mValidChnNodeNum++;
        pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);
        eRet = SUCCESS;
    }
    return eRet;
}

static ERRORTYPE DoVideoIseRemovePort(PARAM_IN COMP_HANDLETYPE hComponent, int iseChnId)
{
    int nFindFlag = 0,cnt = 0;
    ISEChnNode_t *pEntry = NULL, *pTmp = NULL;
    if(hComponent == NULL) {
        aloge("fatal error! hComponent is NULL");
        return ERR_ISE_ILLEGAL_PARAM;
    }
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if(pVideoIseData == NULL) {
        aloge("fatal error! pVideoIseData is NULL");
        return ERR_ISE_ILLEGAL_PARAM;
    }
    // find if the same ise chn exist
    pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
    if (!list_empty(&pVideoIseData->mValidChnAttrList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pVideoIseData->mValidChnAttrList, mList)
        {
            if (pEntry->mIseChn == iseChnId)
            {
                if(0==nFindFlag)
                    nFindFlag = 1;
                else
                    aloge("fatal error! why find more than one?");
                pthread_mutex_lock(&(pEntry->mOutYUVListLock));
                VideoIse_OutputBufferDeInit(pVideoIseData, pEntry);
                pEntry->mIseGrp = -1;
                pEntry->mIseChn = -1;
                pEntry->mBgfgmode_enbale = 0;
                pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                pthread_mutex_destroy(&pEntry->mOutYUVListLock);
                // pthread_cond_destroy(&pEntry->mOutFrameFullCondition);
                list_move_tail(&pEntry->mList, &pVideoIseData->mIdleChnAttrList);
            }
        }
    }
    pVideoIseData->mValidChnNodeNum--;
    pVideoIseData->mIdleChnNodeNum++;
    pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);
    alogd("ValidChnNodeNum %d,IdleChnNodeNum %d",
            pVideoIseData->mValidChnNodeNum,pVideoIseData->mIdleChnNodeNum);
//    if(iseChnId == 0)
    if(pVideoIseData->mValidChnNodeNum == 0)
    {
        alogd("Destroy Ise handle");
        Hal_DestroyIseHandle(pVideoIseData, &pVideoIseData->mIseGrpAttr);
        alogd("Hal_DestroyIseHandle exit");
    }
    if (0 == nFindFlag) {
        aloge("fatal error! not find an exist iseChnId[%d]", iseChnId);
        return ERR_ISE_ILLEGAL_PARAM;
    }
    alogd("ISE chn%d DoVideoIseRemovePort exit",iseChnId);
    return SUCCESS;
}

static ERRORTYPE DoVideoIseSendBackInputFrame(VIDEOISEDATATYPE *pVideoIseData, VIDEO_FRAME_INFO_S *pFrameInfo,
                                            int pFrameInfoPort)
{
    COMP_BUFFERHEADERTYPE BufferHeader;
    BufferHeader.nFlags = pFrameInfoPort; // chn0==0, chn1==1
    if (FALSE == pVideoIseData->mInputPortTunnelFlag)
    {
        BufferHeader.pAppPrivate = pFrameInfo;
        pVideoIseData->pCallbacks->EmptyBufferDone(pVideoIseData->hSelf, pVideoIseData->pAppData, &BufferHeader);
    }
    else
    {
        BufferHeader.pOutputPortPrivate = pFrameInfo;
//        alogw("ise return one frame in tunnel mode, port %d.", pFrameInfoPort);
        if (ISE_PORT_INDEX_CAP0_IN == pFrameInfoPort)
        {
            MM_COMPONENTTYPE *pTunnelComp = (MM_COMPONENTTYPE *)pVideoIseData->sInPortTunnelInfo[ISE_PORT_INDEX_CAP0_IN].hTunnel;
            BufferHeader.nOutputPortIndex = 0x02;
            pTunnelComp->FillThisBuffer(pTunnelComp, &BufferHeader);

        } else if ((ISE_PORT_INDEX_CAP1_IN == pFrameInfoPort))
        {
            MM_COMPONENTTYPE *pTunnelComp = (MM_COMPONENTTYPE *)pVideoIseData->sInPortTunnelInfo[ISE_PORT_INDEX_CAP1_IN].hTunnel;
            BufferHeader.nOutputPortIndex = 0x02;
            pTunnelComp->FillThisBuffer(pTunnelComp, &BufferHeader);

        }
    }
    alogv("release input FrameId[%d], port=%d.", pFrameInfo->mId, pFrameInfoPort);
    return SUCCESS;
}

ERRORTYPE DoVideoIseGetPortDefinition(PARAM_IN COMP_HANDLETYPE hComponent,
                                      PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int i, find = 0;

	for(i = 0; i <= ISE_PORT_INDEX_CAP1_IN; i++) // inport definition
	{
        if (pPortDef->nPortIndex == pVideoIseData->sInPortDef[i].nPortIndex)
        {
            memcpy(pPortDef, &pVideoIseData->sInPortDef[i], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
           find = 1;
        }
    }
    for(i = ISE_PORT_INDEX_OUT0; i <= ISE_PORT_INDEX_OUT3; i++) // outport definition
    {
        if (pPortDef->nPortIndex == pVideoIseData->sOutPortDef[i].nPortIndex)
        {
            memcpy(pPortDef, &pVideoIseData->sOutPortDef[i], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
            find = 1;
        }
    }
    return (1 == find) ? SUCCESS : ERR_ISE_ILLEGAL_PARAM;
}
ERRORTYPE DoVideoIseSetPortDefinition(PARAM_IN COMP_HANDLETYPE hComponent,
                                      PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int i, find = 0;

	for(i = 0; i <= ISE_PORT_INDEX_CAP1_IN; i++) // inport definition
	{
        if (pPortDef->nPortIndex == pVideoIseData->sInPortDef[i].nPortIndex)
        {
            memcpy(&pVideoIseData->sInPortDef[i], pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
           find = 1;
        }
    }
    for(i = ISE_PORT_INDEX_OUT0; i <= ISE_PORT_INDEX_OUT3; i++)  // outport definition
    {
        if (pPortDef->nPortIndex == pVideoIseData->sOutPortDef[i].nPortIndex)
        {
            memcpy(&pVideoIseData->sOutPortDef[i], pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
            find = 1;
        }
    }
    return (1 == find) ? SUCCESS : ERR_ISE_ILLEGAL_PARAM;
}

ERRORTYPE DoVideoIseGetCompBufferSupplier(PARAM_IN COMP_HANDLETYPE hComponent,
                                          PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    BOOL bFindFlag = FALSE; // find nPortIndex
    int i;
    for (i = 0; i < ISE_MAX_PORT; i++) {
        if (pVideoIseData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
            memcpy(pPortBufSupplier, &pVideoIseData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            bFindFlag = TRUE;
            break;
        }
    }
    if (bFindFlag) {
        eError = SUCCESS;
    } else {
        eError = ERR_ISE_ILLEGAL_PARAM;
    }
    return eError;
}
ERRORTYPE DoVideoIseSetCompBufferSupplier(PARAM_IN COMP_HANDLETYPE hComponent,
                                          PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    BOOL bFindFlag = FALSE; // find nPortIndex;
    int i;
    for (i = 0; i < ISE_MAX_PORT; i++) {
        if (pVideoIseData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
            bFindFlag = TRUE;
            memcpy(&pVideoIseData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if (bFindFlag) {
        eError = SUCCESS;
    } else {
        eError = ERR_ISE_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE DoVideoIseGetMPPChannelInfo(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT MPP_CHN_S *pChn)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(pChn, &pVideoIseData->mMppChnInfo);
    return SUCCESS;
}

ERRORTYPE DoVideoIseSetMPPChannelInfo(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN MPP_CHN_S *pChn)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(&pVideoIseData->mMppChnInfo, pChn);
    return SUCCESS;
}

ERRORTYPE DoVideoIseGetTunnelInfo(PARAM_IN COMP_HANDLETYPE hComponent,
                                  PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = ERR_ISE_UNEXIST;
    int i, find = 0;

	for(i = 0; i <= ISE_PORT_INDEX_CAP1_IN; i++) { // inport definition
        if (pVideoIseData->sInPortTunnelInfo[i].nPortIndex == pTunnelInfo->nPortIndex) {
           memcpy(pTunnelInfo, &pVideoIseData->sInPortTunnelInfo[i], sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
           find = 1;
        }
    }
    for(i = ISE_PORT_INDEX_OUT0; i <= ISE_PORT_INDEX_OUT3; i++) { // outport definition
        if (pVideoIseData->sOutPortTunnelInfo[i].nPortIndex == pTunnelInfo->nPortIndex) {
            memcpy(pTunnelInfo, &pVideoIseData->sOutPortTunnelInfo[i], sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
            find = 1;
        }
    }
    return (1 == find) ? SUCCESS : ERR_ISE_UNEXIST;
}

ERRORTYPE DoVideoIseGetGroupAttr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT ISE_GROUP_ATTR_S *pGroupAttr)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    copy_ISE_GRP_ATTR_S(pGroupAttr, &pVideoIseData->mIseGrpAttr);
    pVideoIseData->mIseGrpAttr.iseMode = pVideoIseData->iseMode;
    return SUCCESS;
}

ERRORTYPE DoVideoIseSetGroupAttr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN ISE_GROUP_ATTR_S *pGroupAttr)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    copy_ISE_GRP_ATTR_S(&pVideoIseData->mIseGrpAttr, pGroupAttr);
    pVideoIseData->iseMode = pVideoIseData->mIseGrpAttr.iseMode;
    return SUCCESS;
}

ERRORTYPE DoVideoIseGetChnDefaultAttr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT IseChnAttr *pIseChnAttr)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = FAILURE;
    /*
    ISEChnNode_t *pEntry;
    if (!list_empty(&pVideoIseData->mValidChnAttrList)) {
        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
        {
            if (pEntry->mIseChn == pIseChnAttr->mChnId) {
                copy_ISE_CHN_ATTR_S(pIseChnAttr->pChnAttr, &pEntry->mChnAttr);
                eError = SUCCESS;
                break;
            }
        }
    }
    */
    if (FAILURE == eError) {
        eError = ERR_ISE_UNEXIST;
    }
    return eError;
}

ERRORTYPE DoVideoIseGetChnAttr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT IseChnAttr *pIseChnAttr)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = FAILURE;
    ISEChnNode_t *pEntry;
    pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
    if (!list_empty(&pVideoIseData->mValidChnAttrList)) {
        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
        {
            pthread_mutex_lock(&(pEntry->mOutYUVListLock));
            if (pEntry->mIseChn == pIseChnAttr->mChnId) {
                if(NULL != pIseChnAttr->pChnAttr)
                {
                    memcpy(pIseChnAttr->pChnAttr,&pVideoIseData->mIseChnAttr,sizeof(pVideoIseData->mIseChnAttr));
                }
//                copy_ISE_CHN_ATTR_S(pIseChnAttr->pChnAttr, &pVideoIseData->mIseChnAttr);
                eError = SUCCESS;
                pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                break;
            }
            pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
        }
    }
    pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);
    if (FAILURE == eError) {
        eError = ERR_ISE_UNEXIST;
    }
    return eError;
}

ERRORTYPE DoVideoIseSetChnAttr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT IseChnAttr *pIseChnAttr)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = FAILURE;
    ISEChnNode_t *pEntry;
    int hresult = 0;
    BOOL set_subattr = FALSE;
    pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
    if (!list_empty(&pVideoIseData->mValidChnAttrList)) {
        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
        {
            pthread_mutex_lock(&(pEntry->mOutYUVListLock));
            if (pEntry->mIseChn == pIseChnAttr->mChnId)
            {
                if (ISEMODE_ONE_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
                {
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
                    ISE_CFG_PARA_GDC   *fish_gdc_default_config = NULL,*pChnAttr_fish_gdc_cfg = NULL,*pVideoIseData_fish_gdc_cfg = NULL;
                    fish_gdc_default_config = &pVideoIseData->fish_gdc_default_config;
                    pVideoIseData_fish_gdc_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg;
                    pChnAttr_fish_gdc_cfg = &pIseChnAttr->pChnAttr->mode_attr.mFish.ise_gdc_cfg;
                    if (0 == pIseChnAttr->mChnId)  // main
                    {
                        if(pVideoIseData_fish_gdc_cfg->rectPara.warpMode == Warp_Normal)
                        {
                             if(pVideoIseData_fish_gdc_cfg->rectPara.fishEyePara.ptz.pan != pChnAttr_fish_gdc_cfg->rectPara.fishEyePara.ptz.pan
                                || pVideoIseData_fish_gdc_cfg->rectPara.fishEyePara.ptz.tilt != pChnAttr_fish_gdc_cfg->rectPara.fishEyePara.ptz.tilt
                                || pVideoIseData_fish_gdc_cfg->rectPara.fishEyePara.ptz.zoomH != pChnAttr_fish_gdc_cfg->rectPara.fishEyePara.ptz.zoomH)
                            {
                                static struct timeval last_call_tv = {0}, cur_call_tv = {0};
                                static long call_diff_ms = -1;

                                if (call_diff_ms < 0) {
                                    gettimeofday(&last_call_tv, NULL);
                                }

                                gettimeofday(&cur_call_tv, NULL);

                                call_diff_ms = (cur_call_tv.tv_sec*1000 + cur_call_tv.tv_usec/1000) -
                                                     (last_call_tv.tv_sec*1000 + last_call_tv.tv_usec/1000);

                                if(call_diff_ms >= 60 || call_diff_ms == 0)
                                {
                                    last_call_tv.tv_sec = cur_call_tv.tv_sec;
                                    last_call_tv.tv_usec = cur_call_tv.tv_usec;
                                    pthread_mutex_lock(&pVideoIseData->mWaitSetGdcConfigLock);
                                    pVideoIseData->mWaitSetGdcConfigFlag = FALSE;
                                    pVideoIseData_fish_gdc_cfg->rectPara.fishEyePara.ptz.pan = pChnAttr_fish_gdc_cfg->rectPara.fishEyePara.ptz.pan;
                                    pVideoIseData_fish_gdc_cfg->rectPara.fishEyePara.ptz.tilt = pChnAttr_fish_gdc_cfg->rectPara.fishEyePara.ptz.tilt;
                                    pVideoIseData_fish_gdc_cfg->rectPara.fishEyePara.ptz.zoomH = pChnAttr_fish_gdc_cfg->rectPara.fishEyePara.ptz.zoomH;

#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
                                    struct timeval tpstart,tpend;
                                    static float timeuse = 0,temp = 0;
                                    gettimeofday(&tpstart, NULL);
#endif
                                    hresult = ISE_SetAttr_GDC(&pVideoIseData->gdc_handle, *pVideoIseData_fish_gdc_cfg);
                                    if (0 != hresult)
                                    {
                                        aloge("Set Mo attr fail");
                                        eError = ERR_ISE_ILLEGAL_PARAM;
                                    }
#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
                                    gettimeofday(&tpend,NULL);
                                    timeuse = (tpend.tv_sec*1000 + tpend.tv_usec/1000) -
                                            (tpstart.tv_sec*1000 + tpstart.tv_usec/1000);
                            //                                timeuse /= 1000;
                                    /*if(timeuse > temp)
                                    {
                                        alogw("------>Set Mo Attr Used Time:%f ms<------\n", timeuse);
                                        temp = timeuse;
                                    }*/
                                    timeuse_setmoattr = timeuse;
#endif
                                    pVideoIseData->mWaitSetGdcConfigFlag = TRUE;
                                    pthread_mutex_unlock(&pVideoIseData->mWaitSetGdcConfigLock);
                                }
                                else
                                {
                                    aloge("update attr too fast");
                                }
                            }
                        }else if(pVideoIseData_fish_gdc_cfg->rectPara.warpMode == Warp_Ptz4In1)
                        {
                            for(int kk = 0;kk < 4;kk++)
                            {
                                if(pVideoIseData_fish_gdc_cfg->rectPara.ptz4In1Para[kk].pan !=
                                        pChnAttr_fish_gdc_cfg->rectPara.ptz4In1Para[kk].pan){
                                    pVideoIseData_fish_gdc_cfg->rectPara.ptz4In1Para[kk].pan =
                                            pChnAttr_fish_gdc_cfg->rectPara.ptz4In1Para[kk].pan;
                                    set_subattr = TRUE;
                                }
                                if(pVideoIseData_fish_gdc_cfg->rectPara.ptz4In1Para[kk].tilt !=
                                        pChnAttr_fish_gdc_cfg->rectPara.ptz4In1Para[kk].tilt){
                                    pVideoIseData_fish_gdc_cfg->rectPara.ptz4In1Para[kk].tilt =
                                    pChnAttr_fish_gdc_cfg->rectPara.ptz4In1Para[kk].tilt;
                                    set_subattr = TRUE;
                                }
                                if(pVideoIseData_fish_gdc_cfg->rectPara.ptz4In1Para[kk].zoomH !=
                                        pChnAttr_fish_gdc_cfg->rectPara.ptz4In1Para[kk].zoomH){
                                    pVideoIseData_fish_gdc_cfg->rectPara.ptz4In1Para[kk].zoomH =
                                    pChnAttr_fish_gdc_cfg->rectPara.ptz4In1Para[kk].zoomH;
                                    set_subattr = TRUE;
                                }
                            }
                            if(set_subattr == TRUE)
                            {
                                static struct timeval last_call_tv = {0}, cur_call_tv = {0};
                                static long call_diff_ms = -1;

                                if (call_diff_ms < 0) {
                                    gettimeofday(&last_call_tv, NULL);
                                }

                                gettimeofday(&cur_call_tv, NULL);

                                call_diff_ms = (cur_call_tv.tv_sec*1000 + cur_call_tv.tv_usec/1000) -
                                                     (last_call_tv.tv_sec*1000 + last_call_tv.tv_usec/1000);

                                if(call_diff_ms >= 60 || call_diff_ms == 0)
                                {
                                    last_call_tv.tv_sec = cur_call_tv.tv_sec;
                                    last_call_tv.tv_usec = cur_call_tv.tv_usec;
                                    pthread_mutex_lock(&pVideoIseData->mWaitSetGdcConfigLock);
                                    pVideoIseData->mWaitSetGdcConfigFlag = FALSE;
                                    for(int kk = 0;kk < 4;kk++)
                                    {
                                        pVideoIseData_fish_gdc_cfg->rectPara.ptz4In1Para[kk].pan = pChnAttr_fish_gdc_cfg->rectPara.ptz4In1Para[kk].pan;
                                        pVideoIseData_fish_gdc_cfg->rectPara.ptz4In1Para[kk].tilt = pChnAttr_fish_gdc_cfg->rectPara.ptz4In1Para[kk].tilt;
                                        pVideoIseData_fish_gdc_cfg->rectPara.ptz4In1Para[kk].zoomH = pChnAttr_fish_gdc_cfg->rectPara.ptz4In1Para[kk].zoomH;
                                    }
//#if FISH_LIB
#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
                                    struct timeval tpstart,tpend;
                                    static float timeuse = 0,temp = 0;
                                    gettimeofday(&tpstart, NULL);
#endif
                                    hresult = ISE_SetAttr_GDC(&pVideoIseData->gdc_handle, *pVideoIseData_fish_gdc_cfg);
                                    if (0 != hresult)
                                    {
                                        aloge("Set GDC attr fail");
                                        eError = ERR_ISE_ILLEGAL_PARAM;
                                    }

#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
                                    gettimeofday(&tpend,NULL);
                                    timeuse = (tpend.tv_sec*1000 + tpend.tv_usec/1000) -
                                            (tpstart.tv_sec*1000 + tpstart.tv_usec/1000);
                            //                                timeuse /= 1000;
                                    /*if(timeuse > temp)
                                    {
                                        alogw("------>Set Mo Attr Used Time:%f ms<------\n", timeuse);
                                        temp = timeuse;
                                    }*/
                                    timeuse_setmoattr = timeuse;
#endif
//#endif
                                    pVideoIseData->mWaitSetGdcConfigFlag = TRUE;
                                    pthread_mutex_unlock(&pVideoIseData->mWaitSetGdcConfigLock);
                                } else {
                                    aloge("update ptz attr too fast");
                                }
                            }
                        }
                    }
#else
                    ISE_CFG_PARA_MO   *fish_default_config = NULL,*pChnAttr_fish_cfg = NULL,*pVideoIseData_fish_cfg = NULL;
                    fish_default_config = &pVideoIseData->fish_default_config;
                    pVideoIseData_fish_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_cfg;
                    pChnAttr_fish_cfg = &pIseChnAttr->pChnAttr->mode_attr.mFish.ise_cfg;
                    if (0 == pIseChnAttr->mChnId)  // main
                    {
                        if(pVideoIseData_fish_cfg->dewarp_mode == WARP_NORMAL)
                        {
                            if(pVideoIseData_fish_cfg->pan != pChnAttr_fish_cfg->pan
                                   || pVideoIseData_fish_cfg->tilt != pChnAttr_fish_cfg->tilt
                                   || pVideoIseData_fish_cfg->zoom != pChnAttr_fish_cfg->zoom)
                            {
                                static struct timeval last_call_tv = {0}, cur_call_tv = {0};
                                static long call_diff_ms = -1;

                                if (call_diff_ms < 0) {
                                    gettimeofday(&last_call_tv, NULL);
                                }

                                gettimeofday(&cur_call_tv, NULL);

                                call_diff_ms = (cur_call_tv.tv_sec*1000 + cur_call_tv.tv_usec/1000) -
                                                     (last_call_tv.tv_sec*1000 + last_call_tv.tv_usec/1000);

                                if(call_diff_ms >= 60 || call_diff_ms == 0)
                                {
                                    last_call_tv.tv_sec = cur_call_tv.tv_sec;
                                    last_call_tv.tv_usec = cur_call_tv.tv_usec;
                                    pthread_mutex_lock(&pVideoIseData->mWaitSetMoConfigLock);
                                    pVideoIseData->mWaitSetMoConfigFlag = FALSE;
                                    pVideoIseData_fish_cfg->pan = pChnAttr_fish_cfg->pan;
                                    pVideoIseData_fish_cfg->tilt = pChnAttr_fish_cfg->tilt;
                                    pVideoIseData_fish_cfg->zoom = pChnAttr_fish_cfg->zoom;
//#if FISH_LIB
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
                                    struct timeval tpstart,tpend;
                                    static float timeuse = 0,temp = 0;
                                    gettimeofday(&tpstart, NULL);
#endif

                                    hresult = ISE_SetAttr_Mo(&pVideoIseData->fish_handle);
                                    if (0 != hresult)
                                    {
                                        aloge("Set Mo attr fail");
                                        eError = ERR_ISE_ILLEGAL_PARAM;
                                    }

#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
                                    gettimeofday(&tpend,NULL);
                                    timeuse = (tpend.tv_sec*1000 + tpend.tv_usec/1000) -
                                            (tpstart.tv_sec*1000 + tpstart.tv_usec/1000);
                            //                                timeuse /= 1000;
                                    /*if(timeuse > temp)
                                    {
                                        alogw("------>Set Mo Attr Used Time:%f ms<------\n", timeuse);
                                        temp = timeuse;
                                    }*/
                                    timeuse_setmoattr = timeuse;
#endif
#endif
//#endif
                                    pVideoIseData->mWaitSetMoConfigFlag = TRUE;
                                    pthread_mutex_unlock(&pVideoIseData->mWaitSetMoConfigLock);
                                }
                                else
                                {
                                    aloge("update ptz attr too fast");
                                }
                            }
                        }
                        else if(pVideoIseData_fish_cfg->dewarp_mode == WARP_PTZ4IN1)
                        {
                            for(int kk = 0;kk < 4;kk++)
                            {
                                if(pVideoIseData_fish_cfg->ptzsub_chg[kk] == 1)
                                {
                                    if(pVideoIseData_fish_cfg->pan_sub[kk] != pChnAttr_fish_cfg->pan_sub[kk]){
                                        pVideoIseData_fish_cfg->pan_sub[kk] = pChnAttr_fish_cfg->pan_sub[kk];
                                        set_subattr = TRUE;
                                    }
                                    if(pVideoIseData_fish_cfg->tilt_sub[kk] != pChnAttr_fish_cfg->tilt_sub[kk]) {
                                        pVideoIseData_fish_cfg->tilt_sub[kk] = pChnAttr_fish_cfg->tilt_sub[kk];
                                        set_subattr = TRUE;
                                    }
                                    if(pVideoIseData_fish_cfg->zoom_sub[kk] != pChnAttr_fish_cfg->zoom_sub[kk]){
                                        pVideoIseData_fish_cfg->zoom_sub[kk] = pChnAttr_fish_cfg->zoom_sub[kk];
                                        set_subattr = TRUE;
                                    }
                                }
                            }
                            if(set_subattr == TRUE)
                            {
                                static struct timeval last_call_tv = {0}, cur_call_tv = {0};
                                static long call_diff_ms = -1;

                                if (call_diff_ms < 0) {
                                    gettimeofday(&last_call_tv, NULL);
                                }

                                gettimeofday(&cur_call_tv, NULL);

                                call_diff_ms = (cur_call_tv.tv_sec*1000 + cur_call_tv.tv_usec/1000) -
                                                     (last_call_tv.tv_sec*1000 + last_call_tv.tv_usec/1000);

                                if(call_diff_ms >= 60 || call_diff_ms == 0)
                                {
                                    last_call_tv.tv_sec = cur_call_tv.tv_sec;
                                    last_call_tv.tv_usec = cur_call_tv.tv_usec;
                                    pthread_mutex_lock(&pVideoIseData->mWaitSetMoConfigLock);
                                    pVideoIseData->mWaitSetMoConfigFlag = FALSE;
                                    for(int kk = 0;kk < 4;kk++)
                                    {
                                        pVideoIseData_fish_cfg->ptzsub_chg[kk] = pChnAttr_fish_cfg->ptzsub_chg[kk];
                                        pVideoIseData_fish_cfg->pan_sub[kk] = pChnAttr_fish_cfg->pan_sub[kk];
                                        pVideoIseData_fish_cfg->tilt_sub[kk] = pChnAttr_fish_cfg->tilt_sub[kk];
                                        pVideoIseData_fish_cfg->zoom_sub[kk] = pChnAttr_fish_cfg->zoom_sub[kk];
                                    }
//#if FISH_LIB
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
                                    struct timeval tpstart,tpend;
                                    static float timeuse = 0,temp = 0;
                                    gettimeofday(&tpstart, NULL);
#endif

                                    hresult = ISE_SetAttr_Mo(&pVideoIseData->fish_handle);
                                    if (0 != hresult)
                                    {
                                        aloge("Set Mo attr fail");
                                        eError = ERR_ISE_ILLEGAL_PARAM;
                                    }

#ifdef VIDEO_ISE_SETMOATTR_TIME_DEBUG
                                    gettimeofday(&tpend,NULL);
                                    timeuse = (tpend.tv_sec*1000 + tpend.tv_usec/1000) -
                                            (tpstart.tv_sec*1000 + tpstart.tv_usec/1000);
                            //                                timeuse /= 1000;
                                    /*if(timeuse > temp)
                                    {
                                        alogw("------>Set Mo Attr Used Time:%f ms<------\n", timeuse);
                                        temp = timeuse;
                                    }*/
                                    timeuse_setmoattr = timeuse;
#endif
#endif
//#endif
                                    pVideoIseData->mWaitSetMoConfigFlag = TRUE;
                                    pthread_mutex_unlock(&pVideoIseData->mWaitSetMoConfigLock);
                                } else {
                                    aloge("update ptz attr too fast");
                                }
                            }
                        }
                    }
                    else if (1 <= pIseChnAttr->mChnId && pIseChnAttr->mChnId <= 3) //chn0~3
                    {
                       pVideoIseData_fish_cfg->out_en[pIseChnAttr->mChnId] = pChnAttr_fish_cfg->out_en[pIseChnAttr->mChnId];
                       if(pVideoIseData_fish_cfg->out_en[pIseChnAttr->mChnId])
                       {
                           pVideoIseData_fish_cfg->out_h[pIseChnAttr->mChnId] = pChnAttr_fish_cfg->out_h[pIseChnAttr->mChnId];
                           pVideoIseData_fish_cfg->out_w[pIseChnAttr->mChnId] = pChnAttr_fish_cfg->out_w[pIseChnAttr->mChnId];
                           pVideoIseData_fish_cfg->out_flip[pIseChnAttr->mChnId] = pChnAttr_fish_cfg->out_flip[pIseChnAttr->mChnId];
                           pVideoIseData_fish_cfg->out_mirror[pIseChnAttr->mChnId] = pChnAttr_fish_cfg->out_mirror[pIseChnAttr->mChnId];
                           pVideoIseData_fish_cfg->out_luma_pitch[pIseChnAttr->mChnId] = pChnAttr_fish_cfg->out_luma_pitch[pIseChnAttr->mChnId];
                           pVideoIseData_fish_cfg->out_chroma_pitch[pIseChnAttr->mChnId] = pChnAttr_fish_cfg->out_chroma_pitch[pIseChnAttr->mChnId];
                       }
                    }
#endif
                }
                else if (ISEMODE_TWO_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
                {
#if MPPCFG_ISE_TWO_FISHEYE
                    ISE_CFG_PARA_BI  *dfish_default_config = NULL,*pChnAttr_dfish_cfg = NULL,*pVideoIseData_dfish_cfg = NULL;
                    dfish_default_config = &pVideoIseData->dfish_default_config;
                    pVideoIseData_dfish_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mDFish.ise_cfg;
                    pChnAttr_dfish_cfg = &pIseChnAttr->pChnAttr->mode_attr.mDFish.ise_cfg;
                    if (0 == pIseChnAttr->mChnId)  // main
                    {
//                       memcpy(pVideoIseData_dfish_cfg,dfish_default_config,sizeof(ISE_CFG_PARA_BI));
                    }
                    else if (1 <= pIseChnAttr->mChnId && pIseChnAttr->mChnId <= 3) //chn0~3
                    {
                       pVideoIseData_dfish_cfg->out_en[pIseChnAttr->mChnId] = pChnAttr_dfish_cfg->out_en[pIseChnAttr->mChnId];
                       if(pVideoIseData_dfish_cfg->out_en[pIseChnAttr->mChnId])
                       {
                           pVideoIseData_dfish_cfg->out_h[pIseChnAttr->mChnId] = pChnAttr_dfish_cfg->out_h[pIseChnAttr->mChnId];
                           pVideoIseData_dfish_cfg->out_w[pIseChnAttr->mChnId] = pChnAttr_dfish_cfg->out_w[pIseChnAttr->mChnId];
                           pVideoIseData_dfish_cfg->out_flip[pIseChnAttr->mChnId] = pChnAttr_dfish_cfg->out_flip[pIseChnAttr->mChnId];
                           pVideoIseData_dfish_cfg->out_mirror[pIseChnAttr->mChnId] = pChnAttr_dfish_cfg->out_mirror[pIseChnAttr->mChnId];
                           pVideoIseData_dfish_cfg->out_luma_pitch[pIseChnAttr->mChnId] = pChnAttr_dfish_cfg->out_luma_pitch[pIseChnAttr->mChnId];
                           pVideoIseData_dfish_cfg->out_chroma_pitch[pIseChnAttr->mChnId] = pChnAttr_dfish_cfg->out_chroma_pitch[pIseChnAttr->mChnId];
                       }
                    }
#endif
                }
                else if (ISEMODE_TWO_ISE == pVideoIseData->mIseGrpAttr.iseMode)
                {
#if MPPCFG_ISE_TWO_ISE
                    if (0 == pIseChnAttr->mChnId)
                    {
                        // main
                        ISE  *ise_default_config = NULL,*pChnAttr_ise_cfg = NULL,*pVideoIseData_ise_cfg = NULL;
                        ise_default_config = &pVideoIseData->ise_default_config;
                        pChnAttr_ise_cfg = &pIseChnAttr->pChnAttr->mode_attr.mIse;
                        pVideoIseData_ise_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mIse;
//                        memcpy(pVideoIseData_ise_cfg,ise_default_config,sizeof(ISE));
                        pVideoIseData_ise_cfg->ise_proccfg.bgfgmode_en = pChnAttr_ise_cfg->ise_proccfg.bgfgmode_en;
                        if(pChnAttr_ise_cfg->ise_proccfg.bgfgmode_en == 1)
                        {
//#if ISE_LIB
#if (MPPCFG_ISE_STI == OPTION_ISE_STI_ENABLE)
                            if(pVideoIseData_ise_cfg->ise_bgfg.bgfg_intvl != pChnAttr_ise_cfg->ise_bgfg.bgfg_intvl) //1
                            {
                                pVideoIseData_ise_cfg->ise_bgfg.bgfg_intvl = pChnAttr_ise_cfg->ise_bgfg.bgfg_intvl;
                                hresult = ISE_SetConfig_Sti((int)ISE_SETCFG_BGFG_INTVL_PARAM,
                                         &pVideoIseData_ise_cfg->ise_bgfg.bgfg_intvl, sizeof(int), &pVideoIseData->ise_handle);
                                if (0 != hresult)
                                {
                                    aloge("Set Sti attr bgfg_intvl fail");
                                }
                                alogd("set ise config bgfg_intvl = %d",pVideoIseData_ise_cfg->ise_bgfg.bgfg_intvl);
                            }
                            if(pVideoIseData_ise_cfg->ise_bgfg.getbgd_intvl != pChnAttr_ise_cfg->ise_bgfg.getbgd_intvl) //200
                            {
                                pVideoIseData_ise_cfg->ise_bgfg.getbgd_intvl = pChnAttr_ise_cfg->ise_bgfg.getbgd_intvl;
                                hresult = ISE_SetConfig_Sti((int)ISE_SETCFG_GETBGD_INTVL_PARAM,
                                         &pVideoIseData_ise_cfg->ise_bgfg.getbgd_intvl, sizeof(int), &pVideoIseData->ise_handle);
                                if (0 != hresult)
                                {
                                    aloge("Set Sti attr getbgd_intvl fail");
                                }
                                alogd("set ise config getbgd_intvl = %d",pVideoIseData_ise_cfg->ise_bgfg.getbgd_intvl);
                            }
                            if(pVideoIseData_ise_cfg->ise_bgfg.bgfg_sleep_ms != pChnAttr_ise_cfg->ise_bgfg.bgfg_sleep_ms) //200
                            {
                                pVideoIseData_ise_cfg->ise_bgfg.bgfg_sleep_ms = pChnAttr_ise_cfg->ise_bgfg.bgfg_sleep_ms;
                                hresult = ISE_SetConfig_Sti((int)ISE_SETCFG_BGFG_SLEEP_MS_PARAM,
                                        &pVideoIseData_ise_cfg->ise_bgfg.bgfg_sleep_ms, sizeof(int), &pVideoIseData->ise_handle);
                                if (0 != hresult)
                                {
                                    aloge("Set Sti attr bgfg_sleep_ms fail");
                                }
                                alogd("set ise config bgfg_sleep_ms = %d",pVideoIseData_ise_cfg->ise_bgfg.bgfg_sleep_ms);
                            }
#endif
//#endif
                        }
                    }
                    else if (1 <= pIseChnAttr->mChnId && pIseChnAttr->mChnId <= 3) //chn0~3
                    {
                        ISE_PROCCFG_PARA_STI  *pChnAttr_ise_cfg = NULL,*pVideoIseData_ise_cfg = NULL;
                        pVideoIseData_ise_cfg = &pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_proccfg;
                        pChnAttr_ise_cfg = &pIseChnAttr->pChnAttr->mode_attr.mIse.ise_proccfg;
                        pVideoIseData_ise_cfg->scalar_en[pIseChnAttr->mChnId] = pChnAttr_ise_cfg->scalar_en[pIseChnAttr->mChnId];
                        if(pVideoIseData_ise_cfg->scalar_en[pIseChnAttr->mChnId])
                        {
                            pVideoIseData_ise_cfg->scalar_h[pIseChnAttr->mChnId] = pChnAttr_ise_cfg->scalar_h[pIseChnAttr->mChnId];
                            pVideoIseData_ise_cfg->scalar_w[pIseChnAttr->mChnId] = pChnAttr_ise_cfg->scalar_w[pIseChnAttr->mChnId];
                            pVideoIseData_ise_cfg->scalar_flip[pIseChnAttr->mChnId] = pChnAttr_ise_cfg->scalar_flip[pIseChnAttr->mChnId];
                            pVideoIseData_ise_cfg->scalar_mirr[pIseChnAttr->mChnId] = pChnAttr_ise_cfg->scalar_mirr[pIseChnAttr->mChnId];
                            pVideoIseData_ise_cfg->scalar_luma_pitch[pIseChnAttr->mChnId] = pChnAttr_ise_cfg->scalar_luma_pitch[pIseChnAttr->mChnId];
                            pVideoIseData_ise_cfg->scalar_chroma_pitch[pIseChnAttr->mChnId] = pChnAttr_ise_cfg->scalar_chroma_pitch[pIseChnAttr->mChnId];
                        }
                    }
#endif
                }
                eError = SUCCESS;
                pVideoIseData->mChangeChnAttr = 1;
                pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                break;
            }
            pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
        }
    }
    pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);
    if (FAILURE == eError) {
        eError = ERR_ISE_UNEXIST;
    }
    return eError;
}

ERRORTYPE DoVideoIseGetData(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT ISE_DATA_S *pData, PARAM_IN int nMilliSec)
{
    ERRORTYPE eError = FAILURE;
    int ret = -1;
    int width = 0, height = 0; // PixelFormat
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (COMP_StateIdle != pVideoIseData->state && COMP_StateExecuting != pVideoIseData->state)
    {
        alogw("call DoVideoIseGetData in wrong state[0x%x]", pVideoIseData->state);
        return ERR_ISE_NOT_PERM;
    }
  	if(TRUE == pVideoIseData->mOutputPortTunnelFlag)
	{
		aloge("fatal error! can't call DoVideoIseGetData() in tunnel mode!");
		return ERR_ISE_NOT_PERM;
	}
    ISEChnNode_t *pEntry = NULL;
    if (ISEMODE_ONE_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
    {
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
        if (0 <= pData->mIseChn && pData->mIseChn <= 3)
        {
            if(pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode == Warp_BirdsEye)
            {
                width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.birdsEyeImage.width;
                height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.birdsEyeImage.height;
            }
            else
            {
                width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.width;
                height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.height;
            }
        }
#else
        if (0 <= pData->mIseChn && pData->mIseChn <= 3)
        {
            width = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_cfg.out_luma_pitch[pData->mIseChn];
            height = pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_cfg.out_h[pData->mIseChn];
        }
#endif
    }
    else if (ISEMODE_TWO_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
    {
#if MPPCFG_ISE_TWO_FISHEYE
        if (0 <= pData->mIseChn && pData->mIseChn <= 3)
        {
            width = pVideoIseData->mIseChnAttr.mode_attr.mDFish.ise_cfg.out_luma_pitch[pData->mIseChn];
            height = pVideoIseData->mIseChnAttr.mode_attr.mDFish.ise_cfg.out_h[pData->mIseChn];
        }
#endif
    }
    else if (ISEMODE_TWO_ISE == pVideoIseData->mIseGrpAttr.iseMode)
    {
#if MPPCFG_ISE_TWO_ISE
        if (pData->mIseChn == 0)
        {
            width = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_cfg.pano_luma_pitch;
            height = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_cfg.pano_h;
        }
        else if(1 <= pData->mIseChn && pData->mIseChn <= 3)
        {
            width = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_proccfg.scalar_luma_pitch[pData->mIseChn-1];
            height = pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_proccfg.scalar_h[pData->mIseChn-1];
        }
#endif
    }
_TryToGetOutData:
    pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
    if (!list_empty(&pVideoIseData->mValidChnAttrList))
    {
        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
        {
            pthread_mutex_lock(&(pEntry->mOutYUVListLock));
            if (pEntry->mIseChn == pData->mIseChn)
            {
                if (!list_empty(&pEntry->mReadyOutYUVList))
                {
                    ISEOutBuf_t *pISEOutBuf = list_first_entry(&(pEntry->mReadyOutYUVList), ISEOutBuf_t, mList);
                    memcpy(pData->frame, &pISEOutBuf->VFrame, sizeof(pISEOutBuf->VFrame));
                    pData->frame->VFrame.mWidth = width;
                    pData->frame->VFrame.mHeight = height;
                    list_move_tail(&(pISEOutBuf->mList), &(pEntry->mUsedOutYUVList));
                    eError = SUCCESS;
                }
                else
                {
                    pVideoIseData->mWaitOutFrameFlag[pData->mIseChn] = TRUE;
                }
            }
            pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
        }
    }
    pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);
    if (FAILURE == eError)
    {
        if (0 == nMilliSec)
        {
            eError = ERR_ISE_BUF_EMPTY;
        }
        else if (nMilliSec < 0)
        {
            pthread_mutex_lock(&pVideoIseData->mOutDataMutex[pData->mIseChn]);
            if (TRUE == pVideoIseData->mWaitOutFrameFlag[pData->mIseChn])
            {
                pthread_cond_wait(&pVideoIseData->mOutFrameFullCond[pData->mIseChn],
                    &pVideoIseData->mOutDataMutex[pData->mIseChn]);
            }
            pthread_mutex_unlock(&pVideoIseData->mOutDataMutex[pData->mIseChn]);
            goto _TryToGetOutData;
        }
        else
        {
            // pEntry->mWaitOutFrameFlag = TRUE;
            pthread_mutex_lock(&pVideoIseData->mOutDataMutex[pData->mIseChn]);
            if (TRUE == pVideoIseData->mWaitOutFrameFlag[pData->mIseChn])
            {
                ret = pthread_cond_wait_timeout(&pVideoIseData->mOutFrameFullCond[pData->mIseChn], &pVideoIseData->mOutDataMutex[pData->mIseChn], nMilliSec);
                if (ETIMEDOUT == ret)
                {
	                aloge("wait output frame timeout[%d]ms, ret[%d]", nMilliSec, ret);

	                eError = ERR_ISE_BUF_EMPTY;
	            }
                else if (0 == ret)
                {
                    pthread_mutex_unlock(&pVideoIseData->mOutDataMutex[pData->mIseChn]);
	                goto _TryToGetOutData;
	            }
                else
                {
	                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
	                eError = ERR_ISE_BUF_EMPTY;
	            }
			}
            pthread_mutex_unlock(&pVideoIseData->mOutDataMutex[pData->mIseChn]);
        }
    }
    return eError;
}

ERRORTYPE DoVideoIseReleaseData(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN ISE_DATA_S *pData)
{
    ERRORTYPE eError = ERR_ISE_UNEXIST;
    int frame_find = 0;
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (COMP_StateIdle != pVideoIseData->state && COMP_StateExecuting != pVideoIseData->state)
    {
        alogw("call getStream in wrong state[0x%x]", pVideoIseData->state);
        return ERR_ISE_NOT_PERM;
    }
  	if(TRUE == pVideoIseData->mOutputPortTunnelFlag)
	{
		aloge("fatal error! can't call DoVideoIseGetData() in tunnel mode!");
		return ERR_ISE_NOT_PERM;
	}

    pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
    if (!list_empty(&pVideoIseData->mValidChnAttrList))
    {
        ISEChnNode_t *pEntry;
        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
        {
            pthread_mutex_lock(&(pEntry->mOutYUVListLock));
            if (pEntry->mIseChn == pData->mIseChn)
            {
                if (!list_empty(&pEntry->mUsedOutYUVList))
                {
               #if 0 //by andy
                    ISEOutBuf_t *pISEOutBuf = list_first_entry(&(pEntry->mUsedOutYUVList), ISEOutBuf_t, mList);
                    /*if ((pISEOutBuf->mIseOut.out_luma_mmu_Addr[pData->mIseChn] == (unsigned char *)pData->frame->VFrame.mpVirAddr[0]) &&
                        (pISEOutBuf->mIseOut.out_chroma_u_mmu_Addr[pData->mIseChn] == (unsigned char *)pData->frame->VFrame.mpVirAddr[1]))*/
                    if ((pISEOutBuf->VFrame.VFrame.mpVirAddr[0] == (unsigned char *)pData->frame->VFrame.mpVirAddr[0]) &&
                        (pISEOutBuf->VFrame.VFrame.mpVirAddr[1] == (unsigned char *)pData->frame->VFrame.mpVirAddr[1]))
                    {
//                    	list_add_tail(&(pISEOutBuf->mList), &(pEntry->mIdleOutYUVList));
                        list_move_tail(&(pISEOutBuf->mList), &(pEntry->mIdleOutYUVList));
                        eError = SUCCESS;
                    }
               #else
                   ISEOutBuf_t *pISEOutBuf, *pTemp;
                   list_for_each_entry_safe(pISEOutBuf, pTemp, &pEntry->mUsedOutYUVList, mList)
                   {
                       if ((pISEOutBuf->VFrame.VFrame.mpVirAddr[0] == (unsigned char *)pData->frame->VFrame.mpVirAddr[0]) &&
                           (pISEOutBuf->VFrame.VFrame.mpVirAddr[1] == (unsigned char *)pData->frame->VFrame.mpVirAddr[1]))
                       {
                           list_move_tail(&(pISEOutBuf->mList), &(pEntry->mIdleOutYUVList));
                           eError = SUCCESS;
                           frame_find = 1;
                           break;
                       }
                   }
                   if(!frame_find)
                   {
                       aloge("fatal error! ise data[%p][%p] is not match UsedOutFrameList[%p][%p]",
                               pData->frame->VFrame.mpVirAddr[0], pData->frame->VFrame.mpVirAddr[1],
                               pISEOutBuf->VFrame.VFrame.mpVirAddr[0], pISEOutBuf->VFrame.VFrame.mpVirAddr[1]);
                       eError = ERR_ISE_ILLEGAL_PARAM;
                   }

               #endif
                }
                else
                {
                    aloge("want to release to Grp/Chn[%d:%d] UsedOutYUVList, why NULL?!!!", pEntry->mIseGrp, pEntry->mIseChn);
                    ISEChnNode_t *pTmp;
                    int idle_cnt = 0;
                    list_for_each_entry(pTmp, &pEntry->mIdleOutYUVList, mList)
                    {
                        idle_cnt++;
                    }
                    int ready_cnt = 0;
                    list_for_each_entry(pTmp, &pEntry->mReadyOutYUVList, mList)
                    {
                        ready_cnt++;
                    }
                    int used_cnt = 0;
                    list_for_each_entry(pTmp, &pEntry->mUsedOutYUVList, mList)
                    {
                        used_cnt++;
                    }
                    alogd("in Grp/Chn[%d:%d], IdleNode:%d, ReadyNode:%d, UsedNode:%d", pEntry->mIseGrp, pEntry->mIseChn, idle_cnt, ready_cnt, used_cnt);
                }
            }
            pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
        }
    }
    pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);

    return eError;
}

ERRORTYPE DoVideoISESetFreq(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nFreq)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    int ret = 0;
    pVideoIseData->mISEFreq = nFreq;
    if(nFreq > 0)
    {
        if(pVideoIseData->iseMode == ISEMODE_ONE_FISHEYE)
        {
//#if FISH_LIB
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
            alogd("set mo freq to [%d]MHz", nFreq);
            ISE_SetFrq_Mo(nFreq, &pVideoIseData->fish_handle);
#endif
//#endif
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
            alogd("set mo freq to [%d]MHz", nFreq);
            ISE_SetFrq_GDC(&pVideoIseData->gdc_handle,nFreq);
#endif
        }
        if(pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE)
        {
//#if DFISH_LIB
#if MPPCFG_ISE_TWO_FISHEYE
#if (MPPCFG_ISE_BI == OPTION_ISE_BI_ENABLE)
            alogd("set bi freq to [%d]MHz", nFreq);
            ISE_SetFrq_Bi(nFreq, &pVideoIseData->dfish_handle_by_hardware);
#endif
#endif
//#endif
        }
        if(pVideoIseData->iseMode == ISEMODE_TWO_ISE)
        {
#if MPPCFG_ISE_TWO_ISE
//#if ISE_LIB
#if (MPPCFG_ISE_BI == OPTION_ISE_BI_ENABLE)
            alogd("set sti freq to [%d]MHz", nFreq);
            ISE_SetFrq_Sti(nFreq, &pVideoIseData->ise_handle);
#endif
//#endif
#endif
        }
    }

    return SUCCESS;
}

ERRORTYPE DoVideoIseResetChannel(PARAM_IN COMP_HANDLETYPE hComponent)
{
	int chn0_idle_buf_num = 0, chn1_idle_buf_num = 0;
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if ((pVideoIseData->state != COMP_StateIdle) && (pVideoIseData->state != COMP_StateExecuting))
    {
        aloge("fatal error! must reset channel in stateIdle!");
        return ERR_ISE_NOT_PERM;
    }
    // return input frames
    if (ISEMODE_ONE_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode)
    {
        pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
        if (!list_empty(&pVideoIseData->mBufQ.mReadyFrameList))
        {
            VideoInYuv *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pVideoIseData->mBufQ.mReadyFrameList, mList)
            {
                // alogd("buf_unused[%d]", pVideoIseData->mBufQ.buf_unused);
                DoVideoIseSendBackInputFrame(pVideoIseData, &pEntry->mInYuv, ISE_PORT_INDEX_CAP0_IN);
                list_move_tail(&pEntry->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                pVideoIseData->mBufQ.buf_unused++;
            }
        }
        pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
    }
    else if ((ISEMODE_TWO_FISHEYE == pVideoIseData->mIseGrpAttr.iseMode) ||
        (ISEMODE_TWO_ISE == pVideoIseData->mIseGrpAttr.iseMode))
    {
        // chn0
        pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
        if (!list_empty(&pVideoIseData->mBufQ.mReadyFrameList))
        {
            VideoInYuv *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pVideoIseData->mBufQ.mReadyFrameList, mList)
            {
                // alogd("buf_unused[%d]", pVideoIseData->mBufQ.buf_unused);
                DoVideoIseSendBackInputFrame(pVideoIseData, &pEntry->mInYuv, ISE_PORT_INDEX_CAP0_IN);
                list_move_tail(&pEntry->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                pVideoIseData->mBufQ.buf_unused++;
            }
        }
        // chn1
        if (!list_empty(&pVideoIseData->mBufQ.mReadyFrameList1))
        {
            VideoInYuv *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pVideoIseData->mBufQ.mReadyFrameList1, mList)
            {
                // alogd("buf_unused[%d]", pVideoIseData->mBufQ.buf_unused);
                DoVideoIseSendBackInputFrame(pVideoIseData, &pEntry->mInYuv, ISE_PORT_INDEX_CAP1_IN);
                list_move_tail(&pEntry->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
                pVideoIseData->mBufQ.buf_unused1++;
            }
        }
        pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
    }
    return SUCCESS;
}

ERRORTYPE VideoIseSendCommand(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_COMMANDTYPE Cmd,
                              PARAM_IN unsigned int nParam1, PARAM_IN void *pCmdData)
{
    VIDEOISEDATATYPE *pVideoIseData;
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;
    InitMessage(&msg);
    void *pMsgData = NULL;
    int nMsgDataSize = 0;
    alogv("VideoIseSendCommand: %d", Cmd);

    pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (!pVideoIseData) {
        eError = ERR_ISE_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    if (pVideoIseData->state == COMP_StateInvalid) {
        eError = ERR_ISE_SYS_NOTREADY;
        goto COMP_CONF_CMD_FAIL;
    }
    switch (Cmd) {
        case COMP_CommandStateSet: {
            eCmd = SetState;
            break;
        }
        case COMP_CommandFlush: {
            eCmd = Flush;
            break;
        }
//        case COMP_CommandVendorAddChn: {
//            alogd("call COMP CommandVendorAddChn in state[%d]", pVideoIseData->state);
//            if (pVideoIseData->state != COMP_StateExecuting && pVideoIseData->state != COMP_StateIdle) {
//                aloge("fatal error! why call COMP CommandVendorAddChn in invalid state[%d]", pVideoIseData->state);
//                eError = ERR_ISE_INCORRECT_STATE_OPERATION;
//                goto COMP_CONF_CMD_FAIL;
//            }
//            eCmd = VendorAddIseChn;
//            if (NULL == pCmdData) {
//                alogw("ISE_CHN_ATTR_S == NULL");
//                eError = ERR_ISE_ILLEGAL_PARAM;
//                goto COMP_CONF_CMD_FAIL;
//            }
//            pMsgData = pCmdData;
//            nMsgDataSize = sizeof(IseChnAttr);
//            break;
//        }
//        case COMP_CommandVendorRemoveChn: {
//            // alogd("call COMP CommandVendorRemoveChn in state[%d], remove
//            if (pVideoIseData->state != COMP_StateExecuting && pVideoIseData->state != COMP_StateIdle) {
//                // aloge("fatal error! why call COMP CommandVendorRemoveChn in invalid
//                eError = ERR_ISE_ILLEGAL_PARAM;
//                goto COMP_CONF_CMD_FAIL;
//            }
//            eCmd = VendorRemoveIseChn;  // VendorRemoveOutputSinkInfo
//            if (NULL == pCmdData) {
//                alogw("ISE_CHN_ATTR_S == NULL");
//                eError = ERR_ISE_ILLEGAL_PARAM;
//                goto COMP_CONF_CMD_FAIL;
//            }
//            pMsgData = pCmdData;
//            nMsgDataSize = sizeof(IseChnAttr);
//            break;
//        }
        default:
            alogw("Warning comp_command[0x%x]", Cmd);
            eCmd = -1;
            break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    msg.mpData = pMsgData;
    msg.mDataSize = nMsgDataSize;
//    put_message(&pVideoIseData->cmd_queue, &msg);
    putMessageWithData(&pVideoIseData->cmd_queue, &msg);

COMP_CONF_CMD_FAIL:
    return eError;
}

ERRORTYPE VideoIseGetState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT COMP_STATETYPE *pState)
{
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    *pState = pVideoIseData->state;
    return SUCCESS;
}

ERRORTYPE VideoIseSetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_CALLBACKTYPE *pCallbacks,
                               PARAM_IN void *pAppData)
{
    VIDEOISEDATATYPE *pVideoIseData;
    ERRORTYPE eError = SUCCESS;

    pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (!pVideoIseData || !pCallbacks || !pAppData) {
        eError = ERR_ISE_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    pVideoIseData->pCallbacks = pCallbacks;
    pVideoIseData->pAppData = pAppData;

COMP_CONF_CMD_FAIL:
    return eError;
}

ERRORTYPE VideoIseGetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex,
                            PARAM_IN void *pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;

    switch (nIndex) {
        case COMP_IndexParamPortDefinition: {
            eError =
              DoVideoIseGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier: {
            eError =
              DoVideoIseGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo: {
            eError = DoVideoIseGetMPPChannelInfo(hComponent, (MPP_CHN_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTunnelInfo: {
            eError = DoVideoIseGetTunnelInfo(hComponent, (COMP_INTERNAL_TUNNELINFOTYPE *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorIseGroupAttr: {
            eError = DoVideoIseGetGroupAttr(hComponent, (ISE_GROUP_ATTR_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorIseChnAttr: {
            eError = DoVideoIseGetChnAttr(hComponent, pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorGetIseData: {
        	ISE_DATA_S *iseData = (ISE_DATA_S *)pComponentConfigStructure;
            eError = DoVideoIseGetData(hComponent, iseData, iseData->nMilliSec);
            break;
        }
        default: {
            aloge("fatal error! unknown getConfig Index[0x%x]", nIndex);
            eError = ERR_ISE_NOT_SUPPORT;
            break;
        }
    }
    return eError;
}

ERRORTYPE VideoIseSetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex,
                            PARAM_IN void *pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;
    switch (nIndex) {
        case COMP_IndexParamPortDefinition: {
            eError =
              DoVideoIseSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier: {
            eError =
              DoVideoIseSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo: {
            eError = DoVideoIseSetMPPChannelInfo(hComponent, (MPP_CHN_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorIseGroupAttr: {
            eError = DoVideoIseSetGroupAttr(hComponent, (ISE_GROUP_ATTR_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorIseChnAttr: {
            eError = DoVideoIseSetChnAttr(hComponent, (IseChnAttr *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorIseResetChannel: {
            eError = DoVideoIseResetChannel(hComponent);
            break;
        }
        case COMP_IndexVendorReleaseIseData: {
            eError = DoVideoIseReleaseData(hComponent, (ISE_DATA_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorIseFreq: {
            eError = DoVideoISESetFreq(hComponent, *(int *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorIseAddChn: {
            IseChnAttr *pIseChnAttr = (IseChnAttr *)pComponentConfigStructure;
            int IseGrpId = ((IseChnAttr *)pIseChnAttr)->mGrpId;
            int IseChnId = ((IseChnAttr *)pIseChnAttr)->mChnId;
            eError = DoVideoIseAddPort(hComponent, IseGrpId, IseChnId, pIseChnAttr);
            if (eError != SUCCESS) {
                aloge("fatal error! why DoVideoIseAddPort fail?,return [0x%x]", eError);
                return ERR_ISE_ILLEGAL_PARAM;
            }
            break;
        }
        case COMP_IndexVendorIseRemoveChn: {
            IseChnAttr *pIseChnAttr = (IseChnAttr *)pComponentConfigStructure;
            int IseGrpId = ((IseChnAttr *)pIseChnAttr)->mGrpId;
            int IseChnId = ((IseChnAttr *)pIseChnAttr)->mChnId;
            eError = DoVideoIseRemovePort(hComponent, IseChnId);
            if (eError != SUCCESS)
            {
                aloge("fatal error! why DoVideoIseRemovePort()[0x%x] fail?", eError);
                return ERR_ISE_ILLEGAL_PARAM;
            }
            break;
        }
        case COMP_IndexVendorIseStart: {// do tunnel mode
            break;
        }
        case COMP_IndexVendorIseStop: {// do tunnel mode
            break;
        }
        default: {
            aloge("(f:%s, l:%d) unknown Index[0x%x]", __FUNCTION__, __LINE__, nIndex);
            // eError = OMX_ErrorUnsupportedIndex;
            break;
        }
    }

    return eError;
}

ERRORTYPE VideoIseComponentTunnelRequest(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN unsigned int nPort,
                                         PARAM_IN COMP_HANDLETYPE hTunneledComp, PARAM_IN unsigned int nTunneledPort,
                                         PARAM_INOUT COMP_TUNNELSETUPTYPE *pTunnelSetup)
{
	ERRORTYPE eError = SUCCESS;
	VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
	if (pVideoIseData->state == COMP_StateExecuting)
	{
		alogw("Be careful! tunnel request may be some danger in StateExecuting");
	}
	else if(pVideoIseData->state != COMP_StateIdle)
	{
		aloge("fatal error! tunnel request can't be in state[0x%x]", pVideoIseData->state);
		eError = ERR_ISE_INCORRECT_STATE_OPERATION;
		goto COMP_CMD_FAIL;
	}
	COMP_PARAM_PORTDEFINITIONTYPE *pPortDef;
	COMP_INTERNAL_TUNNELINFOTYPE *pPortTunnelInfo;
	COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier;
	BOOL bFindFlag;
	int i;
	bFindFlag = FALSE;

    for(i = 0; i <= ISE_PORT_INDEX_CAP1_IN; i++) {
    	if(pVideoIseData->sInPortDef[i].nPortIndex == nPort)
    	{
    		pPortDef = &pVideoIseData->sInPortDef[i];
    		bFindFlag = TRUE;
    	}
    }
	if(FALSE == bFindFlag)
	{
        for(i = ISE_PORT_INDEX_OUT0; i <= ISE_PORT_INDEX_OUT3; i++) {
    		if(pVideoIseData->sOutPortDef[i].nPortIndex == nPort)
    		{
    			pPortDef = &pVideoIseData->sOutPortDef[i];
    			bFindFlag = TRUE;
    		}
        }
	}
	if(FALSE == bFindFlag)
	{
		aloge("(f:%s, l:%d) fatal error! portIndex[%d] wrong!", __FUNCTION__, __LINE__, nPort);
		eError = ERR_ISE_ILLEGAL_PARAM;
		goto COMP_CMD_FAIL;
	}

	bFindFlag = FALSE;
	for(i = 0; i <= ISE_PORT_INDEX_CAP1_IN; i++) {
        if(pVideoIseData->sInPortTunnelInfo[i].nPortIndex == nPort)
    	{
    		pPortTunnelInfo = &pVideoIseData->sInPortTunnelInfo[i];
    		bFindFlag = TRUE;
    	}
	}
	if(FALSE == bFindFlag)
	{
        for(i = ISE_PORT_INDEX_OUT0; i <= ISE_PORT_INDEX_OUT3; i++) {
    		if(pVideoIseData->sOutPortTunnelInfo[i].nPortIndex == nPort)
    		{
    			pPortTunnelInfo = &pVideoIseData->sOutPortTunnelInfo[i];
    			bFindFlag = TRUE;
    		}
        }
	}
	if(FALSE == bFindFlag)
	{
		aloge("(f:%s, l:%d) fatal error! portIndex[%d] wrong!", __FUNCTION__, __LINE__, nPort);
		eError = ERR_ISE_ILLEGAL_PARAM;
		goto COMP_CMD_FAIL;
	}

	bFindFlag = FALSE;
	for(i=0; i< ISE_MAX_PORT; i++)
	{
		if(pVideoIseData->sPortBufSupplier[i].nPortIndex == nPort)
		{
			pPortBufSupplier = &pVideoIseData->sPortBufSupplier[i];
			bFindFlag = TRUE;
			break;
		}
	}
	if(FALSE == bFindFlag)
	{
		aloge("(f:%s, l:%d) fatal error! portIndex[%d] wrong!", __FUNCTION__, __LINE__, nPort);
		eError = ERR_ISE_ILLEGAL_PARAM;
		goto COMP_CMD_FAIL;
	}

	pPortTunnelInfo->nPortIndex = nPort;
	pPortTunnelInfo->hTunnel = hTunneledComp;
	pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
	pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
	if(NULL==hTunneledComp && 0==nTunneledPort && NULL==pTunnelSetup)
	{
		alogd("(f:%s, l:%d) omx_core cancel setup tunnel on port[%d]", __FUNCTION__, __LINE__, nPort);
		eError = SUCCESS;
		if(pPortDef->eDir == COMP_DirOutput)
		{
			pVideoIseData->mOutputPortTunnelFlag = FALSE;
		}
		else
		{
			pVideoIseData->mInputPortTunnelFlag = FALSE;
		}
		goto COMP_CMD_FAIL;
	}
	if(pPortDef->eDir == COMP_DirOutput)
	{
        if (pVideoIseData->mOutputPortTunnelFlag
                && pVideoIseData->mOutputPortTunnelEnable[nPort] == 1) {
            aloge("ISE_Comp outport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
		pTunnelSetup->nTunnelFlags = 0;
		pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
		pVideoIseData->mOutputPortTunnelFlag = TRUE;
		pVideoIseData->mOutputPortTunnelEnable[nPort] = 1;
	}
	else
	{
        if (pVideoIseData->mInputPortTunnelFlag
                && pVideoIseData->mInputPortTunnelEnable[nPort] == 1) {
            aloge("ISE_Comp inport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
		//Check the data compatibility between the ports using one or more GetParameter calls.
		//B checks if its input port is compatible with the output port of component A.
		COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
		out_port_def.nPortIndex = nTunneledPort;
		((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
		if(out_port_def.eDir != COMP_DirOutput)
		{
			aloge("(f:%s, l:%d) fatal error! tunnel port index[%d] direction is not output!", __FUNCTION__, __LINE__, nTunneledPort);
			eError = ERR_ISE_ILLEGAL_PARAM;
			goto COMP_CMD_FAIL;
		}
		pPortDef->format = out_port_def.format;

		//The component B informs component A about the final result of negotiation.
		if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier)
		{
			alogw("(f:%s, l:%d) Low probability! use input portIndex[%d] buffer supplier[%d] as final!", __FUNCTION__, __LINE__, nPort, pPortBufSupplier->eBufferSupplier);
			pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
		}
		COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
		oSupplier.nPortIndex = nTunneledPort;
		((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
		oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
		((MM_COMPONENTTYPE*)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
		pVideoIseData->mInputPortTunnelFlag = TRUE;
		pVideoIseData->mInputPortTunnelEnable[nPort] = 1;
	}

COMP_CMD_FAIL:
	return eError;

    return 0;
}

ERRORTYPE VideoIseEmptyThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (pVideoIseData->state != COMP_StateExecuting)
    {
        alogw("send frame when ise state[0x%x] isn not executing", pVideoIseData->state);
        return ERR_ISE_SYS_NOTREADY;
    }
    if (TRUE == pVideoIseData->mInputPortTunnelFlag)
    {
        /* video input bind mode. chn0 or chn1 must only in pIseFrame. */
        VIDEO_FRAME_INFO_S *pIseFrame = (VIDEO_FRAME_INFO_S *)pBuffer->pOutputPortPrivate;
        if (NULL == pIseFrame)
        {
            alogw("ise input ptr is empty.");
            return -1;
        }
        if (pBuffer->nInputPortIndex == pVideoIseData->sInPortDef[ISE_PORT_INDEX_CAP0_IN].nPortIndex)
        {
            // chn0
            pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
            if (list_empty(&pVideoIseData->mBufQ.mIdleFrameList))
            {
                alogw("Warning! ISE Chn0 mBufQ idle frame is empty!");
                if (pVideoIseData->mBufQ.buf_unused != 0)
                {
                    aloge("fatal error! buf_unused must be zero!");
                }
                VideoInYuv *pNode = (VideoInYuv *)malloc(sizeof(VideoInYuv));
                if (NULL == pNode)
                {
                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                    aloge("fatal error! malloc fail!");
                    eError = ERR_ISE_NOMEM;
                    goto ERROR;
                }
                list_add_tail(&pNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                pVideoIseData->mBufQ.buf_unused++;
            }
            VideoInYuv *pFirstNode = list_first_entry(&pVideoIseData->mBufQ.mIdleFrameList, VideoInYuv, mList);
            VIDEO_FRAME_INFO_S *pDstIseFrame = &pFirstNode->mInYuv;
            memcpy(pDstIseFrame, pIseFrame, sizeof(VIDEO_FRAME_INFO_S));
            pFirstNode->mInYuvPort = ISE_PORT_INDEX_CAP0_IN;
            pVideoIseData->mBufQ.buf_unused--;
            list_move_tail(&pFirstNode->mList, &pVideoIseData->mBufQ.mReadyFrameList);
            pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
        }
        else if (pBuffer->nInputPortIndex == pVideoIseData->sInPortDef[ISE_PORT_INDEX_CAP1_IN].nPortIndex)
        {
            // chn1
            pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
            if (list_empty(&pVideoIseData->mBufQ.mIdleFrameList1))
            {
                alogw("Warning! ISE Chn1 mBufQ idle frame is empty!");
                if (pVideoIseData->mBufQ.buf_unused1 != 0)
                {
                    aloge("fatal error! buf_unused must be zero!");
                }
                VideoInYuv *pNode = (VideoInYuv *)malloc(sizeof(VideoInYuv));
                if (NULL == pNode)
                {
                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                    aloge("fatal error! malloc fail!");
                    eError = ERR_ISE_NOMEM;
                    goto ERROR;
                }
                list_add_tail(&pNode->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
                pVideoIseData->mBufQ.buf_unused1++;
            }
            VideoInYuv *pFirstNode1 = list_first_entry(&pVideoIseData->mBufQ.mIdleFrameList1, VideoInYuv, mList);
            VIDEO_FRAME_INFO_S *pDstIseFrame = &pFirstNode1->mInYuv;
            memcpy(pDstIseFrame, pIseFrame, sizeof(VIDEO_FRAME_INFO_S));
            pFirstNode1->mInYuvPort = ISE_PORT_INDEX_CAP1_IN;
            pVideoIseData->mBufQ.buf_unused1--;
            list_move_tail(&pFirstNode1->mList, &pVideoIseData->mBufQ.mReadyFrameList1);
            pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
        } else
        {
            aloge("fatal error! inputPortIndex[%d] match nothing!", pBuffer->nOutputPortIndex);
        }
    }
    else if (FALSE == pVideoIseData->mInputPortTunnelFlag)
    {
        /* video input no bind mode, use send yuv to this component. */
        VIDEO_FRAME_INFO_S *pIseFrame = (VIDEO_FRAME_INFO_S *)pBuffer->pAppPrivate;
        if (NULL == pIseFrame)
        {
            alogw("ise input ptr is empty.");
            return -1;
        }
        VIDEO_FRAME_INFO_S *pIseFrame1 = (VIDEO_FRAME_INFO_S *)pBuffer->pPlatformPrivate;
        pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
        // chn0
        if (((pVideoIseData->iseMode == ISEMODE_ONE_FISHEYE) && (NULL != pIseFrame)) ||
            ((pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) && (NULL != pIseFrame)) ||
            ((pVideoIseData->iseMode == ISEMODE_TWO_ISE) && (NULL != pIseFrame)))
        {
            if (list_empty(&pVideoIseData->mBufQ.mIdleFrameList))
            {
                alogw("Warning! ISE Chn0 mBufQ idle frame is empty!");
                if (pVideoIseData->mBufQ.buf_unused != 0)
                {
                    aloge("fatal error! buf_unused must be zero!");
                }
                VideoInYuv *pNode = (VideoInYuv *)malloc(sizeof(VideoInYuv));
                if (NULL == pNode)
                {
                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                    aloge("fatal error! malloc fail!");
                    eError = ERR_ISE_NOMEM;
                    goto ERROR;
                }
                list_add_tail(&pNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                pVideoIseData->mBufQ.buf_unused++;
            }
            VideoInYuv *pFirstNode = list_first_entry(&pVideoIseData->mBufQ.mIdleFrameList, VideoInYuv, mList);
            VIDEO_FRAME_INFO_S *pDstIseFrame = &pFirstNode->mInYuv;
            memcpy(pDstIseFrame, pIseFrame, sizeof(VIDEO_FRAME_INFO_S));
            pFirstNode->mInYuvPort = ISE_PORT_INDEX_CAP0_IN;
            pVideoIseData->mBufQ.buf_unused--;
            list_move_tail(&pFirstNode->mList, &pVideoIseData->mBufQ.mReadyFrameList);
        }
        // chn1
        if (((pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) && (NULL != pIseFrame1)) ||
            ((pVideoIseData->iseMode == ISEMODE_TWO_ISE) && (NULL != pIseFrame1)))
        {
            if (list_empty(&pVideoIseData->mBufQ.mIdleFrameList1))
            {
                alogw("Warning! ISE Chn1 mBufQ idle frame is empty!");
                if (pVideoIseData->mBufQ.buf_unused1 != 0)
                {
                    aloge("fatal error! buf_unused1 must be zero!");
                }
                VideoInYuv *pNode = (VideoInYuv *)malloc(sizeof(VideoInYuv));
                if (NULL == pNode)
                {
                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                    aloge("fatal error! malloc fail!");
                    eError = ERR_ISE_NOMEM;
                    goto ERROR;
                }
                list_add_tail(&pNode->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
                pVideoIseData->mBufQ.buf_unused1++;
            }
            VideoInYuv *pFirstNode = list_first_entry(&pVideoIseData->mBufQ.mIdleFrameList1, VideoInYuv, mList);
            VIDEO_FRAME_INFO_S *pDstIseFrame = &pFirstNode->mInYuv;
            memcpy(pDstIseFrame, pIseFrame1, sizeof(VIDEO_FRAME_INFO_S));
            pFirstNode->mInYuvPort = ISE_PORT_INDEX_CAP1_IN;
            pVideoIseData->mBufQ.buf_unused1--;
            list_move_tail(&pFirstNode->mList, &pVideoIseData->mBufQ.mReadyFrameList1);
        }
        /*else
        {
//            alogv("IseMose : two camera. \r\n");
        }*/
        pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
    }

    if (pVideoIseData->mNoInputFrameFlag)
    {
        message_t msg;
        msg.command = VIseComp_InputFrameAvailable;
        put_message(&pVideoIseData->cmd_queue, &msg);
    }

ERROR:
    return eError;
}

ERRORTYPE VideoIseFillThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
	VIDEOISEDATATYPE *pVideoIseData;
	ERRORTYPE eError = SUCCESS;
	int ret;
	pVideoIseData = (VIDEOISEDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    VIDEO_FRAME_INFO_S *pFrameInfo = (VIDEO_FRAME_INFO_S *)pBuffer->pOutputPortPrivate;
//    int iseChn = pFrameInfo->mId & 0x0000FFFF;
    int iseChn = pBuffer->nOutputPortIndex;
    pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
    if (!list_empty(&pVideoIseData->mValidChnAttrList))
    {
        ISEChnNode_t *pEntry;
        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
        {
            pthread_mutex_lock(&(pEntry->mOutYUVListLock));
//            if (pEntry->mIseChn == iseChn)
            if (pEntry->mIseChn == (iseChn - 2))
            {
                if (!list_empty(&pEntry->mUsedOutYUVList))
                {
              #if 0 //by andy
                    ISEOutBuf_t *pISEOutBuf = list_first_entry(&(pEntry->mUsedOutYUVList), ISEOutBuf_t, mList);
                    /*if ((pISEOutBuf->mIseOut.out_luma_mmu_Addr[pEntry->mIseChn] == (unsigned char *)pFrameInfo->VFrame.mpVirAddr[0]) &&
                        (pISEOutBuf->mIseOut.out_chroma_u_mmu_Addr[pEntry->mIseChn] == (unsigned char *)pFrameInfo->VFrame.mpVirAddr[1]))*/
                    if ((pISEOutBuf->VFrame.VFrame.mpVirAddr[0] == (unsigned char *)pFrameInfo->VFrame.mpVirAddr[0]) &&
                        (pISEOutBuf->VFrame.VFrame.mpVirAddr[1] == (unsigned char *)pFrameInfo->VFrame.mpVirAddr[1]))
                    {
//                        list_add_tail(&(pISEOutBuf->mList), &(pEntry->mIdleOutYUVList));
                        list_move_tail(&(pISEOutBuf->mList), &(pEntry->mIdleOutYUVList));
                        eError = SUCCESS;
                    }
               #else
                    ISEOutBuf_t *pISEOutBuf, *pTemp;
                    list_for_each_entry_safe(pISEOutBuf, pTemp, &pEntry->mUsedOutYUVList, mList)
                    {
                        if ((pISEOutBuf->VFrame.VFrame.mpVirAddr[0] == (unsigned char *)pFrameInfo->VFrame.mpVirAddr[0]) &&
                            (pISEOutBuf->VFrame.VFrame.mpVirAddr[1] == (unsigned char *)pFrameInfo->VFrame.mpVirAddr[1]))
                        {
                            list_move_tail(&(pISEOutBuf->mList), &(pEntry->mIdleOutYUVList));
                            eError = SUCCESS;
                            break;
                        }
                    }
               #endif //end
                }
            }
            pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
        }
    }
    else
    {
        aloge("fatal error! no chn, but some ise data no release.");
    }
    pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);

    return 0;
}

ERRORTYPE VideoIseComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    VIDEOISEDATATYPE *pVideoIseData;
    ERRORTYPE eError = SUCCESS;
    CompInternalMsgType eCmd = Stop;
    message_t msg;
	int i;
    alogd("VideoIse Component DeInit");
    pVideoIseData = (VIDEOISEDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    int cnt = 0, cnt1=0, cnt2=0;
    struct list_head *pList;
    list_for_each(pList, &pVideoIseData->mBufQ.mIdleFrameList) { cnt++; }
    list_for_each(pList, &pVideoIseData->mBufQ.mIdleFrameList1) { cnt1++; }
    alogd("cnt = %d,cnt1 = %d,buf_unused = %d,buf_unused1 = %d",
            cnt, cnt1,pVideoIseData->mBufQ.buf_unused,pVideoIseData->mBufQ.buf_unused1);
    if (pVideoIseData->mBufQ.buf_unused != cnt) {
        aloge("fatal error! chn0 inputFrames[%d]<[%d] must return all before!",
              ISE_FIFO_LEVEL - pVideoIseData->mBufQ.buf_unused, cnt);
    }
    if (pVideoIseData->mBufQ.buf_unused1 != cnt1) {
        aloge("fatal error! chn1 inputFrames[%d]<[%d] must return all before!",
              ISE_FIFO_LEVEL - pVideoIseData->mBufQ.buf_unused1, cnt1);
    }
    if (!list_empty(&pVideoIseData->mBufQ.mReadyFrameList)) {
        aloge("fatal error! why chn0 readyInputFrame is not empty?");
    }
    if (!list_empty(&pVideoIseData->mBufQ.mReadyFrameList1)) {
        aloge("fatal error! why chn1 readyInputFrame1 is not empty?");
    }
    pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
	// chn0
    if (!list_empty(&pVideoIseData->mBufQ.mIdleFrameList)) {
        VideoInYuv *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoIseData->mBufQ.mIdleFrameList, mList)
        {
            list_del(&pEntry->mList); //
            free(pEntry);
            pEntry = NULL;
        }
    }
	// chn1
	if (!list_empty(&pVideoIseData->mBufQ.mIdleFrameList1)) {
        VideoInYuv *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoIseData->mBufQ.mIdleFrameList1, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            pEntry = NULL;
        }
    }
    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
    pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
    if (!list_empty(&pVideoIseData->mIdleChnAttrList))
    {
        ISEChnNode_t *pEntry_2, *pTmp_2;
        list_for_each_entry_safe(pEntry_2, pTmp_2, &pVideoIseData->mIdleChnAttrList, mList)
        {
                list_del(&(pEntry_2->mList));
                if(pEntry_2 != NULL){
                   free(pEntry_2);
                }
        }
    }
    pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);
    pthread_mutex_destroy(&pVideoIseData->mutex_fifo_ops_lock);
    // YUV output
    pthread_mutex_destroy(&pVideoIseData->mMutexChnListLock);
    msg.command = eCmd;
    put_message(&pVideoIseData->cmd_queue, &msg);
    alogd("wait VideoISE component exit!...");
    // Wait for thread to exit so we can get the status into "error"
    pthread_join(pVideoIseData->thread_id, (void *)&eError);
    message_destroy(&pVideoIseData->cmd_queue);
    pthread_mutex_destroy(&pVideoIseData->mStateLock);
    pthread_mutex_destroy(&pVideoIseData->mWaitSetMoConfigLock);
    pthread_mutex_destroy(&pVideoIseData->mWaitSetGdcConfigLock);
	for (i = 0; i < 4; i++)
	{
		pthread_condattr_destroy(&pVideoIseData->condAttr[i]);
		pthread_cond_destroy(&pVideoIseData->mOutFrameFullCond[i]);
		pthread_mutex_destroy(&pVideoIseData->mOutDataMutex[i]);
		pVideoIseData->mWaitOutFrameFlag[i] = FALSE;
	}
	if (pVideoIseData) {
        free(pVideoIseData); //fix
    }
	ion_memClose();
    alogd("VideoIse component exited!");

    return eError;
}

ERRORTYPE VideoIseComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE *pComp;
    VIDEOISEDATATYPE *pVideoIseData;
    ERRORTYPE eError = SUCCESS;
    unsigned int err;
    int i = 0;

    pComp = (MM_COMPONENTTYPE *)hComponent;

    // Create private data
    pVideoIseData = (VIDEOISEDATATYPE *)malloc(sizeof(VIDEOISEDATATYPE));
    memset(pVideoIseData, 0x0, sizeof(VIDEOISEDATATYPE));

    pComp->pComponentPrivate = (void *)pVideoIseData;
    pVideoIseData->state = COMP_StateLoaded;
    pthread_mutex_init(&pVideoIseData->mStateLock, NULL);
    pVideoIseData->hSelf = hComponent;

    // YUV input
    pthread_mutex_init(&pVideoIseData->mutex_fifo_ops_lock, NULL);
    // ch0
    pVideoIseData->mBufQ.buf_unused = ISE_FIFO_LEVEL;
    INIT_LIST_HEAD(&pVideoIseData->mBufQ.mIdleFrameList);
    INIT_LIST_HEAD(&pVideoIseData->mBufQ.mReadyFrameList);
    for (i = 0; i < ISE_FIFO_LEVEL; i++) {
        VideoInYuv *pNode = (VideoInYuv *)malloc(sizeof(VideoInYuv));
        if (NULL == pNode) {
            aloge("(f:%s, l:%d) fatal error! malloc fail!", __FUNCTION__, __LINE__);
            eError = ERR_VENC_NOMEM;
            goto EXIT;
        }
        memset(pNode, 0, sizeof(VideoInYuv));
        list_add_tail(&pNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
    }
    pVideoIseData->mBufQ.mYUVTatolNum = 0;
    pVideoIseData->mBufQ.mYUVProcessTatolNum = 0;
    pVideoIseData->mBufQ.mYUVLossTatolNum = 0;
    // ch1
    pVideoIseData->mBufQ.buf_unused1 = ISE_FIFO_LEVEL;
    INIT_LIST_HEAD(&pVideoIseData->mBufQ.mIdleFrameList1);
    INIT_LIST_HEAD(&pVideoIseData->mBufQ.mReadyFrameList1);
    for (i = 0; i < ISE_FIFO_LEVEL; i++) {
        VideoInYuv *pNode = (VideoInYuv *)malloc(sizeof(VideoInYuv));
        if (NULL == pNode) {
            aloge("(f:%s, l:%d) fatal error! malloc fail!", __FUNCTION__, __LINE__);
            eError = ERR_VENC_NOMEM;
            goto EXIT;
        }
        memset(pNode, 0, sizeof(VideoInYuv));
        list_add_tail(&pNode->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
    }
    pVideoIseData->mBufQ.mYUVTatolNum1 = 0;
    pVideoIseData->mBufQ.mYUVProcessTatolNum1 = 0;
    pVideoIseData->mBufQ.mYUVLossTatolNum1 = 0;

    pVideoIseData->mNoInputFrameFlag = 0;

    // YUV output
    pthread_mutex_init(&pVideoIseData->mMutexChnListLock, NULL);
    INIT_LIST_HEAD(&pVideoIseData->mValidChnAttrList);
    INIT_LIST_HEAD(&pVideoIseData->mIdleChnAttrList);

	for (i = 0; i < 4; i++) {
        pthread_condattr_init(&pVideoIseData->condAttr[i]);
        pthread_condattr_setclock(&pVideoIseData->condAttr[i], CLOCK_MONOTONIC);
        pthread_cond_init(&pVideoIseData->mOutFrameFullCond[i], &pVideoIseData->condAttr[i]);
		pthread_mutex_init(&pVideoIseData->mOutDataMutex[i], NULL);
		pVideoIseData->mWaitOutFrameFlag[i] = FALSE;
    }

	pVideoIseData->mWaitSetMoConfigFlag = TRUE;
	pthread_mutex_init(&pVideoIseData->mWaitSetMoConfigLock, NULL);

	pVideoIseData->mWaitSetGdcConfigFlag = TRUE;
	pthread_mutex_init(&pVideoIseData->mWaitSetGdcConfigLock, NULL);

    // Fill in function pointers
    pComp->SetCallbacks = VideoIseSetCallbacks;
    pComp->SendCommand = VideoIseSendCommand;
    pComp->GetConfig = VideoIseGetConfig;
    pComp->SetConfig = VideoIseSetConfig;
    pComp->GetState = VideoIseGetState;
    pComp->ComponentTunnelRequest = VideoIseComponentTunnelRequest;
    pComp->ComponentDeInit = VideoIseComponentDeInit;
    pComp->EmptyThisBuffer = VideoIseEmptyThisBuffer;
    pComp->FillThisBuffer = VideoIseFillThisBuffer;

    // Initialize component data structures to default values
    pVideoIseData->sPortParam.nPorts = 0x6;
    pVideoIseData->sPortParam.nStartPortNumber = 0x0;
    // Initialize the video parameters for input port
    for (i = 0; i < 2; i++) {
        pVideoIseData->sInPortDef[i].nPortIndex = i;
        pVideoIseData->sInPortDef[i].bEnabled = TRUE;
        pVideoIseData->sInPortDef[i].eDomain = COMP_PortDomainVideo;
        pVideoIseData->sInPortDef[i].format.video.cMIMEType = "YVU420";
        pVideoIseData->sInPortDef[i].format.video.nFrameWidth = 176;
        pVideoIseData->sInPortDef[i].format.video.nFrameHeight = 144;
        pVideoIseData->sInPortDef[i].eDir = COMP_DirInput;
        pVideoIseData->sInPortDef[i].format.video.eCompressionFormat = PT_NV;
        pVideoIseData->sInPortDef[i].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        pVideoIseData->sInPortTunnelInfo[i].nPortIndex = i;
        pVideoIseData->sInPortTunnelInfo[i].eTunnelType = TUNNEL_TYPE_COMMON;
        pVideoIseData->sPortBufSupplier[i].nPortIndex = i;
        pVideoIseData->sPortBufSupplier[i].eBufferSupplier = COMP_BufferSupplyInput;
        pVideoIseData->mInputPortTunnelEnable[i] = 0;
    }

    // Initialize the video parameters for output port
    for (i = 2; i < 6; i++) {
        pVideoIseData->sOutPortDef[i].nPortIndex = i;
        pVideoIseData->sOutPortDef[i].bEnabled = TRUE;
        pVideoIseData->sOutPortDef[i].eDomain = COMP_PortDomainVideo;
        pVideoIseData->sOutPortDef[i].format.video.cMIMEType = "YVU420";
        pVideoIseData->sOutPortDef[i].format.video.nFrameWidth = 176;
        pVideoIseData->sOutPortDef[i].format.video.nFrameHeight = 144;
        pVideoIseData->sOutPortDef[i].eDir = COMP_DirOutput;
        pVideoIseData->sOutPortDef[i].format.video.eCompressionFormat = PT_NV;  // PT_BUTT;
        pVideoIseData->sOutPortDef[i].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        pVideoIseData->sOutPortTunnelInfo[i].nPortIndex = i;
        pVideoIseData->sOutPortTunnelInfo[i].eTunnelType = TUNNEL_TYPE_COMMON;
        pVideoIseData->sPortBufSupplier[i].nPortIndex = i;
        pVideoIseData->sPortBufSupplier[i].eBufferSupplier = COMP_BufferSupplyOutput;
        pVideoIseData->mOutputPortTunnelEnable[i] = 0;
    }

    if (message_create(&pVideoIseData->cmd_queue) < 0) {
        aloge("message error!");
        eError = ERR_VENC_NOMEM;
        goto EXIT;
    }

    // Create the component thread
    err = pthread_create(&pVideoIseData->thread_id, NULL, VideoIse_ComponentThread, pVideoIseData);
    if (err) {
        eError = ERR_VENC_NOMEM;
        goto EXIT;
    }
    alogd("VideoISE component Init, threadId[0x%lx]!", pVideoIseData->thread_id);
EXIT:
    return eError;
}

/**
 *  Component Thread
 *    The ComponentThread function is exeuted in a separate pThread and
 *    is used to implement the actual component functions.
 */
static void *VideoIse_ComponentThread(void *pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    VIDEOISEDATATYPE *pVideoIseData = (VIDEOISEDATATYPE *)pThreadData;
    message_t cmd_msg;
    int omxRet;
    int width = 0, height = 0;
    int ldc_width = 0, ldc_height = 0;
    int result = 0;
    alogv("VideoISE ComponentThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"VISEComp", 0, 0, 0);
    while (1) {
PROCESS_MESSAGE:
        if (get_message(&pVideoIseData->cmd_queue, &cmd_msg) == 0) {
            cmd = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;

            alogv("VideoEnc ComponentThread get_message cmd:%d", cmd);

            if (cmd == SetState)
            {
                if (pVideoIseData->state == (COMP_STATETYPE)(cmddata))
                    pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                            COMP_EventError, ERR_ISE_SAMESTATE, 0, NULL);
                else
                {
                    switch ((COMP_STATETYPE)(cmddata))
                    {
                        case COMP_StateInvalid:
                        {
                            pVideoIseData->state = COMP_StateInvalid;
                            pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                    COMP_EventError, ERR_ISE_INVALIDSTATE, 0, NULL);
                            pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                    COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                    pVideoIseData->state, NULL);
                            break;
                        }
                        case COMP_StateLoaded:
                        {
                            if (pVideoIseData->state != COMP_StateIdle)
                            {
                                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                        COMP_EventError,
                                                                        ERR_VENC_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            alogd("OMX_StateLoaded begin");
                            pVideoIseData->state = COMP_StateLoaded;
                            pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                    COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                    pVideoIseData->state, NULL);
                            alogd("OMX_StateLoaded ok");
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            if (pVideoIseData->state == COMP_StateLoaded)
                            {
                                alogv("video ise: loaded->idle ...");
                                // init ise
                                int ret = 0;
                                ret = ion_memOpen();
                                if (ret != 0)
                                {
                                    aloge("Open ion failed!");
                                    return (void *)FAILURE;
                                }
                                pVideoIseData->state = COMP_StateIdle;
                                // callback sem++
                                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                        pVideoIseData->state, NULL);
                            }
                            else if (pVideoIseData->state == COMP_StatePause || pVideoIseData->state == COMP_StateExecuting)
                            {
                                alogv("video encoder: pause/executing[0x%x]->idle ...", pVideoIseData->state);
                                pVideoIseData->state = COMP_StateIdle;
								DoVideoIseResetChannel(pVideoIseData->hSelf);
                                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                        pVideoIseData->state, NULL);
                            }
                            else
                            {
                                aloge("fatal error! current state[0x%x] can't turn to idle!", pVideoIseData->state);
                                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                        COMP_EventError,
                                                                        ERR_VENC_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            // Transition can only happen from pause or idle state
                            if (pVideoIseData->state == COMP_StateIdle || pVideoIseData->state == COMP_StatePause)
                            {
                                pVideoIseData->state = COMP_StateExecuting;
                                alogd("COMP_StateExecuting ok");
                                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                        pVideoIseData->state, NULL);
                            }
                            else
                            {
                                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                        COMP_EventError,
                                                                        ERR_VENC_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            break;
                        }
                        case COMP_StatePause:
                        {
                            // Transition can only happen from idle or executing state
                            if (pVideoIseData->state == COMP_StateIdle || pVideoIseData->state == COMP_StateExecuting) {
                                pVideoIseData->state = COMP_StatePause;
                                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                        pVideoIseData->state, NULL);
                            }
                            else
                            {
                                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
                                                                        COMP_EventError,
                                                                        ERR_VENC_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                }
            }
            else if (cmd == Flush)
            {

            }
            else if (cmd == Stop)
            {
                goto EXIT; // Kill thread
            }
//            else if (cmd == VendorAddIseChn)
//            {
//                ERRORTYPE eRet = SUCCESS;
//                IseChnAttr *pIseChnAttr = (IseChnAttr *)cmd_msg.mpData;
//                int IseGrpId = ((IseChnAttr *)cmd_msg.mpData)->mGrpId;
//                int IseChnId = ((IseChnAttr *)cmd_msg.mpData)->mChnId;
//                eRet = DoVideoIseAddPort(pVideoIseData, IseGrpId, IseChnId, pIseChnAttr);
//                if (eRet != SUCCESS) {
//                    aloge("fatal error! why DoVideoIseAddPort fail?,return [0x%x]", eRet);
//                    pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
//                            COMP_EventError, ERR_ISE_ILLEGAL_PARAM, 0, NULL);
//                    return (void*)ERR_ISE_ILLEGAL_PARAM;
//                }
//                free(cmd_msg.mpData);
//                cmd_msg.mpData = NULL;
//                cmd_msg.mDataSize = 0;
//                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
//                                                        COMP_EventCmdComplete, COMP_CommandVendorAddChn, eRet, NULL);
//            }
//            else if (cmd == VendorRemoveIseChn)
//            {
//                ERRORTYPE eRet = SUCCESS;
//                int IseChnId = ((IseChnAttr *)cmd_msg.mpData)->mChnId;
//                eRet = DoVideoIseRemovePort(pVideoIseData, IseChnId);
//                if (eRet != SUCCESS)
//                {
//                    aloge("fatal error! why DoVideoIseRemovePort()[0x%x] fail?", eRet);
//                }
//                free(cmd_msg.mpData);
//                cmd_msg.mpData = NULL;
//                cmd_msg.mDataSize = 0;
//                pVideoIseData->pCallbacks->EventHandler(pVideoIseData->hSelf, pVideoIseData->pAppData,
//                                                        COMP_EventCmdComplete, COMP_CommandVendorRemoveChn, 0, NULL);
//            }
            else if (cmd == VIseComp_InputFrameAvailable)
            {
                if (0 == pVideoIseData->mNoInputFrameFlag)
                {
                    // alogd("BeCareful! noInputFrameFlag already 0");
                }
                pVideoIseData->mNoInputFrameFlag = 0;
            }
            goto PROCESS_MESSAGE;
        }
        VideoInYuv *pFrameNode = NULL;
        VideoInYuv *pFrameNode1 = NULL;

        if (pVideoIseData->state == COMP_StateExecuting)
        {
            int ret_proc = -1;
            // Get one frame from mReadyFrameList;
            pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
            // chn0
            if ((pVideoIseData->iseMode == ISEMODE_ONE_FISHEYE) || (pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE)
                      || (pVideoIseData->iseMode == ISEMODE_TWO_ISE) )
            {
                if (list_empty(&pVideoIseData->mBufQ.mReadyFrameList))
                {
                    pVideoIseData->mNoInputFrameFlag = 1;
                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                    TMessage_WaitQueueNotEmpty(&pVideoIseData->cmd_queue, 0);
                    goto PROCESS_MESSAGE;
                }
                pFrameNode = list_first_entry(&pVideoIseData->mBufQ.mReadyFrameList, VideoInYuv, mList);
            }

            // chn1
            if (pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE || (pVideoIseData->iseMode == ISEMODE_TWO_ISE) )
            {
                if (list_empty(&pVideoIseData->mBufQ.mReadyFrameList1))
                {
                    pVideoIseData->mNoInputFrameFlag = 1;
                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                    TMessage_WaitQueueNotEmpty(&pVideoIseData->cmd_queue, 0);
                    goto PROCESS_MESSAGE;
                }
                pFrameNode1 = list_first_entry(&pVideoIseData->mBufQ.mReadyFrameList1, VideoInYuv, mList);
            }
            pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);

            // ise process. // Prepare Fish ISE Data
            if (pVideoIseData->iseMode == ISEMODE_ONE_FISHEYE)
            {
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
                ISE_PROCIN_PARA_GDC ise_procin0;
                ISE_PROCOUT_PARA_GDC ise_procout;
                ISE_PROCOUT_PARA_GDC ise_procout_ldc;
#else
                ISE_PROCIN_PARA_MO ise_procin0;
                ISE_PROCOUT_PARA_MO ise_procout;
#endif
                if ((pVideoIseData->iseMode == ISEMODE_ONE_FISHEYE) ||
                    (pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) || (pVideoIseData->iseMode == ISEMODE_TWO_ISE))
                {
                    // chn0
                    ise_procin0.in_luma_phy_Addr = pFrameNode->mInYuv.VFrame.mPhyAddr[0];
                    ise_procin0.in_chroma_phy_Addr = pFrameNode->mInYuv.VFrame.mPhyAddr[1];
                    ise_procin0.in_luma_mmu_Addr = (unsigned char *)pFrameNode->mInYuv.VFrame.mpVirAddr[0];
                    ise_procin0.in_chroma_mmu_Addr = (unsigned char *)pFrameNode->mInYuv.VFrame.mpVirAddr[1];
                }

                ISEChnNode_t *pEntry = NULL;
                ISEOutBuf_t *pOutBuf[4],*pOutReadyBuf[4];
                pthread_mutex_lock(&(pVideoIseData->mMutexChnListLock));
                if (!list_empty(&(pVideoIseData->mValidChnAttrList)))
                {
                    list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
                    {
                        // Prepare ISE_Process Out buf. Get out buf from queue. or reset queue
                        if ((0 == pEntry->mIseChn) || (1 == pEntry->mIseChn) ||
                            (2 == pEntry->mIseChn) || (3 == pEntry->mIseChn))
                        {
                            pthread_mutex_lock(&(pEntry->mOutYUVListLock));
                            if (!list_empty(&(pEntry->mIdleOutYUVList))) // copy item to ise proc out buf
                            {
                                VideoIse_GetIdleOutputBuffer(pVideoIseData, pEntry, &pOutBuf[pEntry->mIseChn]);
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
                                if (Warp_BirdsEye == \
                                    pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode)
                                {
                                    VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutBuf[pEntry->mIseChn], \
                                        &ise_procout, &ise_procout_ldc, NULL);
                                }
                                else
                                {
                                    VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutBuf[pEntry->mIseChn], \
                                        &ise_procout, NULL, NULL);
                                }
#endif
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
                                VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutBuf[pEntry->mIseChn], \
                                        NULL, NULL, &ise_procout);
#endif
                            }
                            else
                            {
                                if (!list_empty(&(pEntry->mReadyOutYUVList)))
                                {
                                    alogw("Warning! ISE Chn%d idle outlist is empty,move ready list to idle!",pEntry->mIseChn);
                                    pOutReadyBuf[pEntry->mIseChn] = list_first_entry(&(pEntry->mReadyOutYUVList), ISEOutBuf_t, mList);
                                    list_move_tail(&(pOutReadyBuf[pEntry->mIseChn]->mList), &(pEntry->mIdleOutYUVList));
                                    VideoIse_GetIdleOutputBuffer(pVideoIseData, pEntry, &pOutReadyBuf[pEntry->mIseChn]);
                                    alogd("pOutReadyBuf[%d]: %p", pEntry->mIseChn, pOutReadyBuf[pEntry->mIseChn]);
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
                                    if (Warp_BirdsEye == \
                                        pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode)
                                    {
                                        VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutReadyBuf[pEntry->mIseChn], \
                                            &ise_procout, &ise_procout_ldc, NULL);
                                    }
                                    else
                                    {
                                        VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutReadyBuf[pEntry->mIseChn], \
                                            &ise_procout, NULL, NULL);
                                    }
#endif
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
                                    VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutReadyBuf[pEntry->mIseChn], \
                                                NULL, NULL, &ise_procout);
#endif
                                }
                                else
                                {
                                    alogw("Warning! ISE Chn%d ready outlist is empty !",pEntry->mIseChn);
                                    // notify user.
                                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode->mInYuv, ISE_PORT_INDEX_CAP0_IN);
                                    // return this frame to this component queue.
                                    pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
                                    list_move_tail(&pFrameNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                                    pVideoIseData->mBufQ.buf_unused++;
                                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                                    pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                                    pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
                                    goto PROCESS_MESSAGE;
                                }

                            }
                            pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                        }
                    }
				}
                pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
//#if
#if (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
                pthread_mutex_lock((&pVideoIseData->mWaitSetMoConfigLock));
                if(pVideoIseData->mWaitSetMoConfigFlag == TRUE)
                {
#ifdef VIDEO_ISE_PROC_TIME_DEBUG
                    struct timeval tpstart;
                    struct timeval tpend;
                    static float average,tmp;
                    static int frame_num = 0;
                    gettimeofday(&tpstart,NULL);
#endif

                    ret_proc = ISE_Proc_Mo(&pVideoIseData->fish_handle, &ise_procin0, &ise_procout);
#ifdef VIDEO_ISE_PROC_TIME_DEBUG
                    gettimeofday(&tpend,NULL);
                    if(0 == ret_proc)
                    {
                        float timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+(tpend.tv_usec-tpstart.tv_usec);
                        timeuse/=1000;
                        frame_num ++;
                        if(frame_num <= 10000 && timeuse > tmp)
                        {
                            tmp = timeuse;
                            alogw("------>Proc Mo Used Time:%f ms<------\n", tmp);
                        }
                    }
#endif
                }
                pthread_mutex_unlock((&pVideoIseData->mWaitSetMoConfigLock));
//#endif
#endif

//#if
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
                                pthread_mutex_lock((&pVideoIseData->mWaitSetGdcConfigLock));
                                if(pVideoIseData->mWaitSetGdcConfigFlag == TRUE)
                                {
#ifdef VIDEO_ISE_PROC_TIME_DEBUG
                                    struct timeval tpstart;
                                    struct timeval tpend;
                                    static float average,tmp;
                                    static int frame_num = 0;
                                    gettimeofday(&tpstart,NULL);
#endif
                                    if(pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode == Warp_BirdsEye)
                                    {
                                        ret_proc = ISE_Proc_GDC(&pVideoIseData->gdc_handle, &ise_procin0, &ise_procout_ldc,Warp_LDC);
                                        alogd("ISE_Proc_GDC in Warp_LDC");
                                    }
                                    ret_proc = ISE_Proc_GDC(&pVideoIseData->gdc_handle, &ise_procin0, &ise_procout,pVideoIseData->mIseChnAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode);
#ifdef VIDEO_ISE_PROC_TIME_DEBUG
                                    gettimeofday(&tpend,NULL);
                                    if(0 == ret_proc)
                                    {
                                        float timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+(tpend.tv_usec-tpstart.tv_usec);
                                        timeuse/=1000;
                                        frame_num ++;
                                        if(frame_num <= 10000 && timeuse > tmp)
                                        {
                                            tmp = timeuse;
                                            alogw("------>Proc Gdc Used Time:%f ms<------\n", tmp);
                                        }
                                    }
#endif
                                }
                                pthread_mutex_unlock((&pVideoIseData->mWaitSetGdcConfigLock));
                //#endif
#endif
                pEntry = NULL;
                if (0 == ret_proc)
                {
                    pthread_mutex_lock(&(pVideoIseData->mMutexChnListLock));
                    if (!list_empty(&(pVideoIseData->mValidChnAttrList)))
                    {
                        uint64_t pts = pFrameNode->mInYuv.VFrame.mpts;
                        unsigned int exptime = pFrameNode->mInYuv.VFrame.mExposureTime;
                        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
                        {
                            if ((0 == pEntry->mIseChn) || (1 == pEntry->mIseChn) ||
                            (2 == pEntry->mIseChn) || (3 == pEntry->mIseChn))
                            {
                                pthread_mutex_lock(&(pEntry->mOutYUVListLock));
                                if (!list_empty(&(pEntry->mIdleOutYUVList))) // copy item to ise proc out buf
                                {
                                    pOutBuf[pEntry->mIseChn]->VFrame.VFrame.mpts = pts;
                                    pOutBuf[pEntry->mIseChn]->VFrame.VFrame.mExposureTime = exptime;
                                    list_move_tail(&(pOutBuf[pEntry->mIseChn]->mList), &(pEntry->mReadyOutYUVList));
                                    pthread_mutex_lock(&pVideoIseData->mOutDataMutex[pEntry->mIseChn]);
                                    if (pVideoIseData->mWaitOutFrameFlag[pEntry->mIseChn] == TRUE)
                                    {
                                        pVideoIseData->mWaitOutFrameFlag[pEntry->mIseChn] = FALSE;
                                        pthread_cond_signal(&pVideoIseData->mOutFrameFullCond[pEntry->mIseChn]);
                                    }
                                    pthread_mutex_unlock(&pVideoIseData->mOutDataMutex[pEntry->mIseChn]);
                                }
                                pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                            }
                        }
                    }
                    pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
                }
                else
                {
                    aloge("fatal error!,ISE Proc Mo failed!,return %#010x",ret_proc);
                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode->mInYuv, ISE_PORT_INDEX_CAP0_IN);
                    // return this frame to this component queue.
                    pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
                    list_move_tail(&pFrameNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                    pVideoIseData->mBufQ.buf_unused++;
                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                    ret_proc = -1;
                    goto PROCESS_MESSAGE;
                }
                ret_proc = -1;
            }
            else if (pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE)
            {
#if MPPCFG_ISE_TWO_FISHEYE
                // Prepare ISE Data
                ISE_PROCIN_PARA_BI ise_procin0;
                ISE_PROCOUT_PARA_BI ise_procout;
                if ((pVideoIseData->iseMode == ISEMODE_ONE_FISHEYE) ||
                    (pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) || (pVideoIseData->iseMode == ISEMODE_TWO_ISE))
                {
                    // chn0
                    ise_procin0.in_luma_phy_Addr[0] = pFrameNode->mInYuv.VFrame.mPhyAddr[0];
					ise_procin0.in_chroma_phy_Addr[0] = pFrameNode->mInYuv.VFrame.mPhyAddr[1];
                    ise_procin0.in_luma_mmu_Addr[0] = (unsigned char *)pFrameNode->mInYuv.VFrame.mpVirAddr[0];
                    ise_procin0.in_chroma_mmu_Addr[0] = (unsigned char *)pFrameNode->mInYuv.VFrame.mpVirAddr[1];
                }
                if ((pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) || (pVideoIseData->iseMode == ISEMODE_TWO_ISE))
                {
                    // chn1
                    ise_procin0.in_luma_phy_Addr[1] = pFrameNode1->mInYuv.VFrame.mPhyAddr[0];
					ise_procin0.in_chroma_phy_Addr[1] = pFrameNode1->mInYuv.VFrame.mPhyAddr[1];
                    ise_procin0.in_luma_mmu_Addr[1] = (unsigned char *)pFrameNode1->mInYuv.VFrame.mpVirAddr[0];
                    ise_procin0.in_chroma_mmu_Addr[1] = (unsigned char *)pFrameNode1->mInYuv.VFrame.mpVirAddr[1];
                }
                ISEChnNode_t *pEntry = NULL;
                ISEOutBuf_t *pOutBuf[4],*pOutReadyBuf[4];
                pthread_mutex_lock(&(pVideoIseData->mMutexChnListLock));
                if (!list_empty(&(pVideoIseData->mValidChnAttrList)))
                {
                    list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
                    {
                        // Prepare ISE_Process Out buf. Get out buf from queue. or reset queue
                        if ((0 == pEntry->mIseChn) || (1 == pEntry->mIseChn) ||
                           (2 == pEntry->mIseChn) || (3 == pEntry->mIseChn))
                        {
                            pthread_mutex_lock(&(pEntry->mOutYUVListLock));
                            if (!list_empty(&(pEntry->mIdleOutYUVList))) // copy item to ise proc out buf
                            {
                                VideoIse_GetIdleOutputBuffer(pVideoIseData, pEntry, &pOutBuf[pEntry->mIseChn]);
                                VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutBuf[pEntry->mIseChn], \
                                    &ise_procout);
                            }
                            else // reset this bufs
                            {
                                if (!list_empty(&(pEntry->mReadyOutYUVList)))
                                {
                                    alogw("Warning! ISE Chn%d idle outlist is empty,move ready list to idle!",pEntry->mIseChn); // get data slow
                                    pOutReadyBuf[pEntry->mIseChn] = list_first_entry(&(pEntry->mReadyOutYUVList), ISEOutBuf_t, mList);
                                    list_move_tail(&(pOutReadyBuf[pEntry->mIseChn]->mList), &(pEntry->mIdleOutYUVList));
                                    VideoIse_GetIdleOutputBuffer(pVideoIseData, pEntry, &pOutReadyBuf[pEntry->mIseChn]);
                                    VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutReadyBuf[pEntry->mIseChn], \
                                        &ise_procout);
                                }
                                else
                                {
                                    alogw("Warning! ISE Chn%d ready outlist is empty !",pEntry->mIseChn);   // data not back
                                    // notify user.
                                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode->mInYuv, ISE_PORT_INDEX_CAP0_IN);
                                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode1->mInYuv, ISE_PORT_INDEX_CAP1_IN);
                                    // return this frame to this component queue.
                                    pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
                                    list_move_tail(&pFrameNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                                    pVideoIseData->mBufQ.buf_unused++;
                                    list_move_tail(&pFrameNode1->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
                                    pVideoIseData->mBufQ.buf_unused1++;
                                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                                    pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                                    pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
                                    goto PROCESS_MESSAGE;
                                }
                            }
                            pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                        }
                    }
                }
                pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
//#if DFISH_LIB
#ifdef VIDEO_TakePicture_Debug_Time
                struct timeval tpstart,tpend;
                long timeuse;
                gettimeofday(&tpstart, NULL);
#endif
                if(pVideoIseData->dfish_handle_mode == HANDLE_BY_HARDWARE)
                {
#if (MPPCFG_ISE_BI == OPTION_ISE_BI_ENABLE)
                    ret_proc =
                            ISE_Proc_Bi(&pVideoIseData->dfish_handle_by_hardware,&ise_procin0, &ise_procout);
#endif
                }
                else if(pVideoIseData->dfish_handle_mode == HANDLE_BY_SOFT)
                {
#if (MPPCFG_ISE_BI_SOFT == OPTION_ISE_BI_SOFT_ENABLE)
                    ret_proc =
                            ISE_Proc_BI_Soft(&pVideoIseData->dfish_handle_by_soft,&ise_procin0, &ise_procout);

#endif
                }
                else
                {
                    aloge("fatal error!No dfish handle mode. Must select dfish handle mode.");
                    ret_proc = -1;
                }
//#endif

#ifdef VIDEO_TakePicture_Debug_Time
                gettimeofday(&tpend,NULL);
                timeuse=(tpend.tv_sec*1000+tpend.tv_usec/1000)-(tpstart.tv_sec*1000+tpstart.tv_usec/1000);
                alogd("------>Take Picture Used Time:%ld ms<------\n", timeuse);
#endif
                pEntry = NULL;
                if (0 == ret_proc)
                {
                    pthread_mutex_lock(&(pVideoIseData->mMutexChnListLock));
                    if (!list_empty(&(pVideoIseData->mValidChnAttrList)))
                    {
                        uint64_t pts = pFrameNode->mInYuv.VFrame.mpts;
                        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
                        {
                            if ((0 == pEntry->mIseChn) || (1 == pEntry->mIseChn) ||
                                (2 == pEntry->mIseChn) || (3 == pEntry->mIseChn))
                            {
                                pthread_mutex_lock(&(pEntry->mOutYUVListLock));
                                if (!list_empty(&(pEntry->mIdleOutYUVList))) // copy item to ise proc out buf
                                {
                                    pOutBuf[pEntry->mIseChn]->VFrame.VFrame.mpts = pts;
                                    list_move_tail(&(*pOutBuf[pEntry->mIseChn]).mList, &(pEntry->mReadyOutYUVList));
                                    pthread_mutex_lock(&pVideoIseData->mOutDataMutex[pEntry->mIseChn]);
                                    if (pVideoIseData->mWaitOutFrameFlag[pEntry->mIseChn] == TRUE)
                                    {
                                        pVideoIseData->mWaitOutFrameFlag[pEntry->mIseChn] = FALSE;
                                        pthread_cond_signal(&pVideoIseData->mOutFrameFullCond[pEntry->mIseChn]);
                                    }
                                    pthread_mutex_unlock(&pVideoIseData->mOutDataMutex[pEntry->mIseChn]);
                                }
                                pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                            }
                        }
                    }
                    pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
                }
                else
                {
                    aloge("fatal error!,ISE Proc Bi failed!,return %x",ret_proc);
                    // notify user.
                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode->mInYuv, ISE_PORT_INDEX_CAP0_IN);
                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode1->mInYuv, ISE_PORT_INDEX_CAP1_IN);
                    // return this frame to this component queue.
                    pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
                    list_move_tail(&pFrameNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                    pVideoIseData->mBufQ.buf_unused++;
                    list_move_tail(&pFrameNode1->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
                    pVideoIseData->mBufQ.buf_unused1++;
                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                    ret_proc = -1;
                    goto PROCESS_MESSAGE;
                }
                ret_proc = -1;
 #endif
            }
            else if (pVideoIseData->iseMode == ISEMODE_TWO_ISE)
            {
#if MPPCFG_ISE_TWO_ISE
                // Prepare ISE Data
                ISE_PROCIN_PARA_STI ise_procin0;
                ISE_PROCOUT_PARA_STI ise_procout;
                if ((pVideoIseData->iseMode == ISEMODE_ONE_FISHEYE) ||
                    (pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) || (pVideoIseData->iseMode == ISEMODE_TWO_ISE))
                {
                    // chn0
                    ise_procin0.in_luma_phy_Addr[0] = pFrameNode->mInYuv.VFrame.mPhyAddr[0];
					ise_procin0.in_chroma_phy_Addr[0] = pFrameNode->mInYuv.VFrame.mPhyAddr[1];
                    ise_procin0.in_luma_mmu_Addr[0] = (unsigned char *)pFrameNode->mInYuv.VFrame.mpVirAddr[0];
                    ise_procin0.in_chroma_mmu_Addr[0] = (unsigned char *)pFrameNode->mInYuv.VFrame.mpVirAddr[1];
                }
                if ((pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) || (pVideoIseData->iseMode == ISEMODE_TWO_ISE))
                {
                    // chn1
                    ise_procin0.in_luma_phy_Addr[1] = pFrameNode1->mInYuv.VFrame.mPhyAddr[0];
					ise_procin0.in_chroma_phy_Addr[1] = pFrameNode1->mInYuv.VFrame.mPhyAddr[1];
                    ise_procin0.in_luma_mmu_Addr[1] = (unsigned char *)pFrameNode1->mInYuv.VFrame.mpVirAddr[0];
                    ise_procin0.in_chroma_mmu_Addr[1] = (unsigned char *)pFrameNode1->mInYuv.VFrame.mpVirAddr[1];
                }
                ISEChnNode_t *pEntry = NULL;
                ISEOutBuf_t *pOutBuf[4],*pOutReadyBuf[4];
                pthread_mutex_lock(&(pVideoIseData->mMutexChnListLock));
                if (!list_empty(&(pVideoIseData->mValidChnAttrList)))
                {
                    list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
                    {
                        // Prepare ISE_Process Out buf. Get out buf from queue. or reset queue
                        if (0 == pEntry->mIseChn)
                        {
                            pthread_mutex_lock(&(pEntry->mOutYUVListLock));
                            if (!list_empty(&(pEntry->mIdleOutYUVList))) // copy item to ise proc out buf
                            {
                                VideoIse_GetIdleOutputBuffer(pVideoIseData, pEntry, &pOutBuf[pEntry->mIseChn]);
                                VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutBuf[pEntry->mIseChn], \
                                    &ise_procout);
                            }
                            else // reset this bufs
                            {
                                if (!list_empty(&(pEntry->mReadyOutYUVList)))
                                {
                                    alogw("Warning! ISE Chn%d idle outlist is empty,move ready list to idle!",pEntry->mIseChn);
                                    pOutReadyBuf[pEntry->mIseChn] = list_first_entry(&(pEntry->mReadyOutYUVList), ISEOutBuf_t, mList);
                                    list_move_tail(&(pOutReadyBuf[pEntry->mIseChn]->mList), &(pEntry->mIdleOutYUVList));
                                    VideoIse_GetIdleOutputBuffer(pVideoIseData, pEntry, &pOutReadyBuf[pEntry->mIseChn]);
                                    VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutReadyBuf[pEntry->mIseChn], \
                                        &ise_procout);
                                }
                                else
                                {
                                    alogw("Warning! ISE Chn%d ready outlist is empty !",pEntry->mIseChn);
                                    // notify user.
                                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode->mInYuv, ISE_PORT_INDEX_CAP0_IN);
                                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode1->mInYuv, ISE_PORT_INDEX_CAP1_IN);
                                    // return this frame to this component queue.
                                    pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
                                    list_move_tail(&pFrameNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                                    pVideoIseData->mBufQ.buf_unused++;
                                    list_move_tail(&pFrameNode1->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
                                    pVideoIseData->mBufQ.buf_unused1++;
                                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                                    pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                                    pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
                                    goto PROCESS_MESSAGE;
                                }
                            }
                            pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                        }
                        if ((1 == pEntry->mIseChn) ||
                           (2 == pEntry->mIseChn) || (3 == pEntry->mIseChn))
                        {
                            pthread_mutex_lock(&(pEntry->mOutYUVListLock));
                            if (!list_empty(&(pEntry->mIdleOutYUVList))) // copy item to ise proc out buf
                            {
                                VideoIse_GetIdleOutputBuffer(pVideoIseData, pEntry, &pOutBuf[pEntry->mIseChn]);
                                VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutBuf[pEntry->mIseChn], \
                                    &ise_procout);
                            }
                            else   // reset this bufs
                            {
                                alogw("Warning! ISE Chn%d idle outlist is empty,move ready list to idle!",pEntry->mIseChn);
                                if (!list_empty(&(pEntry->mReadyOutYUVList)))
                                {
                                    pOutReadyBuf[pEntry->mIseChn] = list_first_entry(&(pEntry->mReadyOutYUVList), ISEOutBuf_t, mList);
                                    list_move_tail(&(pOutReadyBuf[pEntry->mIseChn]->mList), &(pEntry->mIdleOutYUVList));
                                    VideoIse_GetIdleOutputBuffer(pVideoIseData, pEntry, &pOutReadyBuf[pEntry->mIseChn]);
                                    VideoIse_ConfigIseProcOutBuffer(pVideoIseData, pEntry, pOutReadyBuf[pEntry->mIseChn], \
                                        &ise_procout);
                                }
                                else
                                {
                                    alogw("Warning! ISE Chn%d ready outlist is empty !",pEntry->mIseChn);
                                    // notify user.
                                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode->mInYuv, ISE_PORT_INDEX_CAP0_IN);
                                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode1->mInYuv, ISE_PORT_INDEX_CAP1_IN);
                                    // return this frame to this component queue.
                                    pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
                                    list_move_tail(&pFrameNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                                    pVideoIseData->mBufQ.buf_unused++;
                                    list_move_tail(&pFrameNode1->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
                                    pVideoIseData->mBufQ.buf_unused1++;
                                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                                    pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                                    pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
                                    goto PROCESS_MESSAGE;
                                }
                            }
                            pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                        }
                    }
                }
                pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
//#if ISE_LIB
#if (MPPCFG_ISE_STI == OPTION_ISE_STI_ENABLE)
                ret_proc = ISE_Proc_Sti(&ise_procin0,&ise_procout,&pVideoIseData->mIseChnAttr.mode_attr.mIse.ise_proccfg,
                                        &pVideoIseData->ise_handle);
#endif
//#endif
                pEntry = NULL;
                if (0 == ret_proc)
                {
                    pthread_mutex_lock(&(pVideoIseData->mMutexChnListLock));
                    if (!list_empty(&(pVideoIseData->mValidChnAttrList)))
                    {
                        uint64_t pts = pFrameNode->mInYuv.VFrame.mpts;
                        list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
                        {
                            if ((0 == pEntry->mIseChn) || (1 == pEntry->mIseChn) ||
                                (2 == pEntry->mIseChn) || (3 == pEntry->mIseChn))
                            {
                                pthread_mutex_lock(&(pEntry->mOutYUVListLock));
                                if (!list_empty(&(pEntry->mIdleOutYUVList))) // copy item to ise proc out buf
                                {
                                    pOutBuf[pEntry->mIseChn]->VFrame.VFrame.mpts = pts;
                                    list_move_tail(&(*pOutBuf[pEntry->mIseChn]).mList, &(pEntry->mReadyOutYUVList));
                                    pthread_mutex_lock(&pVideoIseData->mOutDataMutex[pEntry->mIseChn]);
                                    if (pVideoIseData->mWaitOutFrameFlag[pEntry->mIseChn] == TRUE)
                                    {
                                        pVideoIseData->mWaitOutFrameFlag[pEntry->mIseChn] = FALSE;
                                        pthread_cond_signal(&pVideoIseData->mOutFrameFullCond[pEntry->mIseChn]);
                                    }
                                    pthread_mutex_unlock(&pVideoIseData->mOutDataMutex[pEntry->mIseChn]);
                                }
                                pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                            }
                        }
                    }
                    pthread_mutex_unlock(&(pVideoIseData->mMutexChnListLock));
                }
                else
                {
                    aloge("fatal error!,ISE Proc Sti failed!,return %#010x",ret_proc);
                    // notify user.
                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode->mInYuv, ISE_PORT_INDEX_CAP0_IN);
                    DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode1->mInYuv, ISE_PORT_INDEX_CAP1_IN);
                    // return this frame to this component queue.
                    pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
                    list_move_tail(&pFrameNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                    pVideoIseData->mBufQ.buf_unused++;
                    list_move_tail(&pFrameNode1->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
                    pVideoIseData->mBufQ.buf_unused1++;
                    pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
                    ret_proc = -1;
                    goto PROCESS_MESSAGE;
                }
                ret_proc = -1;
#endif
            }
            // notify user.
            if ((pVideoIseData->iseMode == ISEMODE_ONE_FISHEYE) || (pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) ||
                (pVideoIseData->iseMode == ISEMODE_TWO_ISE))
            {
                DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode->mInYuv, ISE_PORT_INDEX_CAP0_IN);
            }
            if ((pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) || (pVideoIseData->iseMode == ISEMODE_TWO_ISE))
            {
                DoVideoIseSendBackInputFrame(pVideoIseData, &pFrameNode1->mInYuv, ISE_PORT_INDEX_CAP1_IN);
            }
            // return this frame to this component queue.
            pthread_mutex_lock(&pVideoIseData->mutex_fifo_ops_lock);
            if ((pVideoIseData->iseMode == ISEMODE_ONE_FISHEYE) || (pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE) ||
                (pVideoIseData->iseMode == ISEMODE_TWO_ISE))
            {
                list_move_tail(&pFrameNode->mList, &pVideoIseData->mBufQ.mIdleFrameList);
                pVideoIseData->mBufQ.buf_unused++;
            }
            if (pVideoIseData->iseMode == ISEMODE_TWO_FISHEYE || (pVideoIseData->iseMode == ISEMODE_TWO_ISE))
            {
                list_move_tail(&pFrameNode1->mList, &pVideoIseData->mBufQ.mIdleFrameList1);
                pVideoIseData->mBufQ.buf_unused1++;
            }
            pthread_mutex_unlock(&pVideoIseData->mutex_fifo_ops_lock);
            /* tunnel mode */
            if(TRUE == pVideoIseData->mOutputPortTunnelFlag)
            {
                ISEChnNode_t *pEntry = NULL;
                pthread_mutex_lock(&pVideoIseData->mMutexChnListLock);
                if (!list_empty(&pVideoIseData->mValidChnAttrList))
                {
                    list_for_each_entry(pEntry, &pVideoIseData->mValidChnAttrList, mList)
                    {
                        pthread_mutex_lock(&(pEntry->mOutYUVListLock));
                         /* ISE_PORT_INDEX_OUT0==2, ISE_PORT_INDEX_OUT1==3, ISE_PORT_INDEX_OUT2==4, ISE_PORT_INDEX_OUT3==5 */
                        MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE*)
                                                (pVideoIseData->sOutPortTunnelInfo[2 + pEntry->mIseChn].hTunnel);
                        if (NULL != pOutTunnelComp)
                        {
                            if (!list_empty(&pEntry->mReadyOutYUVList))
                            {
                                ISEOutBuf_t *pISEOutBuf = list_first_entry(&(pEntry->mReadyOutYUVList), ISEOutBuf_t, mList);
                                COMP_BUFFERHEADERTYPE obh;
                                obh.nOutputPortIndex = pVideoIseData->sOutPortTunnelInfo[2 + pEntry->mIseChn].nPortIndex;
                                obh.nInputPortIndex = pVideoIseData->sOutPortTunnelInfo[2 + pEntry->mIseChn].nTunnelPortIndex;
                                obh.pOutputPortPrivate = (void*)&pISEOutBuf->VFrame;
                                omxRet = pOutTunnelComp->EmptyThisBuffer(pOutTunnelComp, &obh);
                                if(SUCCESS == omxRet)
                                {
//                                    list_add_tail(&(pISEOutBuf->mList), &(pEntry->mUsedOutYUVList));
                                    list_move_tail(&(pISEOutBuf->mList), &(pEntry->mUsedOutYUVList));
                                }
                                else
                                {
                                    alogw("ISE OutTunnelComp EmptyThisBuffer failed!");
                                }
                            }
                        }
                        pthread_mutex_unlock(&(pEntry->mOutYUVListLock));
                    }
                }
                pthread_mutex_unlock(&pVideoIseData->mMutexChnListLock);
            }
        }
        else
        {
            alogv("ISE ComponentThread not OMX_StateExecuting\n");
            TMessage_WaitQueueNotEmpty(&pVideoIseData->cmd_queue, 0);
        }
    }

EXIT:
    alogv("VideoISE ComponentThread stopped");
    return (void *)SUCCESS;
}
