/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_uvc.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/11/30
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/
#define LOG_NDEBUG 0
#define LOG_TAG "mpi_uvcin"
#include <utils/plat_log.h>

//ref platform headers
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"
#include "pthread.h"

//media api headers to app
#include "mm_common.h"
#include "mm_comm_uvc.h"
#include "mpi_uvc.h"
#include "mm_comm_video.h"
#include "uvcInput.h"

//media internal common headers.
#include <media_common.h>
#include <tsemaphore.h>
#include <mm_component.h>
#include "mpi_uvc_common.h"
#include <vdecoder.h>
#include <UvcCompStream.h>


ERRORTYPE AW_MPI_UVC_CreateDevice(UVC_DEV UvcDev)
{
    ERRORTYPE bResult = FAILURE;

    if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }
    bResult = uvcInput_Open_Video(UvcDev);

    return bResult;
}

ERRORTYPE AW_MPI_UVC_DestroyDevice(UVC_DEV UvcDev)
{
    ERRORTYPE bResult = FAILURE;

    if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }

    bResult = uvcInput_Close_Video(UvcDev);

    return bResult;
}

ERRORTYPE AW_MPI_UVC_SetDeviceAttr(UVC_DEV UvcDev, UVC_ATTR_S *pAttr)
{
    ERRORTYPE bResult = FAILURE;

    if(!UvcDev || !pAttr)
    {
        aloge("fatal error! the input param is null");
        return ERR_UVC_NULL_PTR;
    }
    
    bResult = uvcInput_SetDevAttr(UvcDev, pAttr);

    return bResult;
}

ERRORTYPE AW_MPI_UVC_GetDeviceAttr(UVC_DEV UvcDev, UVC_ATTR_S *pAttr)
{
    ERRORTYPE bResult = FAILURE;
    if(!UvcDev || !pAttr)
    {
        aloge("fatal error! the input param is null");
        return ERR_UVC_NULL_PTR;
    }

    bResult = uvcInput_GetDevAttr(UvcDev, pAttr);
    
    return bResult;
}

ERRORTYPE AW_MPI_UVC_EnableDevice(UVC_DEV UvcDev)
{
    ERRORTYPE bResult = FAILURE;
    if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }
    
    bResult = uvcInput_SetDevEnable(UvcDev);

    return bResult;
}

ERRORTYPE AW_MPI_UVC_DisableDevice(UVC_DEV UvcDev)
{
    ERRORTYPE bResult = FAILURE;  
    if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }
    
    bResult = uvcInput_SetDevDisable(UvcDev);

    return bResult;    
}

static ERRORTYPE UvcEventHandler(
	 PARAM_IN COMP_HANDLETYPE hComponent,
	 PARAM_IN void* pAppData,
	 PARAM_IN COMP_EVENTTYPE eEvent,
	 PARAM_IN unsigned int nData1,
	 PARAM_IN unsigned int nData2,
	 PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
    MPP_CHN_S UvcChnInfo;
    ret = ((MM_COMPONENTTYPE*)hComponent)->GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &UvcChnInfo);
    //if(ret == SUCCESS)
    //{
        //alogv("video encoder event, MppChannel[%d][%d][%d]", UvcChnInfo.mModId, UvcChnInfo.mDevId, UvcChnInfo.mChnId);
    //}
	UVC_CHN_MAP_S *pChn = (UVC_CHN_MAP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("uvc input EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else
            {
                alogw("Low probability! what command[0x%x]?", nData1);
                break;
            }
        }
        case COMP_EventError:
        {
            if(ERR_UVC_SAMESTATE == nData1)
            {
                alogv("set same state to uvc!");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_UVC_INVALIDSTATE == nData1)
            {
                aloge("why uvc state turn to invalid?");
                break;
            }
            else if(ERR_UVC_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! uvc state transition incorrect.");
                break;
            }
        }
        case COMP_EventRecVbvFull:
        {
            alogw("need handle vbvFull!");
            break;
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
	return SUCCESS;
}


static COMP_CALLBACKTYPE UvcCallback = {
	.EventHandler = UvcEventHandler,
	.EmptyBufferDone = NULL,
	.FillBufferDone = NULL,
};


ERRORTYPE AW_MPI_UVC_CreateVirChn(UVC_DEV UvcDev, UVC_CHN UvcChn)
{
    ERRORTYPE bResult = FAILURE;

    if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }
    if(UvcChn < 0 || UvcChn >= UVC_MAX_CHN_NUM)
    {
        aloge("fatal error! invalid UvcChn[%d] channel", UvcChn);
        return ERR_UVC_ILLEGAL_PARAM;
    }
    
    UVC_CHN_MAP_S **ppChn = NULL;
    if(SUCCESS == uvcInput_SearchExitDevVirChn(UvcDev, UvcChn, ppChn))
    {
        return ERR_UVC_EXIST;
    }

    UVC_CHN_MAP_S *pNode = uvcInput_CHN_MAP_S_Construct();
    if(pNode)
    {
        pNode->mUvcChn = UvcChn;
        bResult = COMP_GetHandle((COMP_HANDLETYPE *)&pNode->mUvcComp, CDX_ComponentNameUVCInput, (void *)pNode , &UvcCallback);
        if(bResult != SUCCESS)
        {
            aloge("fatal error! get comp handle fail!");
            return bResult;
        }
        MPP_CHN_S ChannelInfo;
        ChannelInfo.mModId = MOD_ID_UVC;
        ChannelInfo.mDevId = (int)UvcDev; // note: the device pointer as ID.
        ChannelInfo.mChnId = pNode->mUvcChn;
        bResult = pNode->mUvcComp->SetConfig(pNode->mUvcComp, COMP_IndexVendorMPPChannelInfo, (void *) &ChannelInfo);
        bResult = pNode->mUvcComp->SendCommand(pNode->mUvcComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        cdx_sem_down(&pNode->mSemCompCmd);

        uvcInput_addChannel(UvcDev, pNode);
        
    }
    return bResult;
}

ERRORTYPE AW_MPI_UVC_DestroyVirChn(UVC_DEV UvcDev, UVC_CHN UvcChn)
{
    ERRORTYPE bResult = FAILURE;

    if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }
    if(UvcChn < 0 || UvcChn >= UVC_MAX_CHN_NUM)
    {
        aloge("fatal error! invalid UvcChn[%d] channel", UvcChn);
        return ERR_UVC_ILLEGAL_PARAM;
    }
    
    UVC_CHN_MAP_S *pChn = NULL;
    if(uvcInput_SearchExitDevVirChn(UvcDev, UvcChn, &pChn) != SUCCESS)
    {
        return ERR_UVC_UNEXIST ;
    }
    if(pChn->mUvcComp)
    {
        COMP_STATETYPE nCompState;
        if(SUCCESS == pChn->mUvcComp->GetState(pChn->mUvcComp, &nCompState))
        {
            if(COMP_StateIdle == nCompState)
            {
                bResult = pChn->mUvcComp->SendCommand(pChn->mUvcComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
                cdx_sem_down(&pChn->mSemCompCmd);
            }
            else if(COMP_StateLoaded == nCompState)
            {
                bResult = SUCCESS;
            }
            else if(COMP_StateInvalid == nCompState)
            {
                alogw("Low probability! Component StateInvalid?");
                bResult = SUCCESS;
            }
            else
            {
                aloge("fatal error! invalid VeChn[%d] state[0x%x]!", UvcChn, nCompState);
            }

            if(SUCCESS == bResult)
            {
                bResult |= uvcInput_removeChannel(UvcDev, pChn);
                bResult |= COMP_FreeHandle(pChn->mUvcComp);
                pChn->mUvcComp = NULL;
                uvcInput_CHN_MAP_S_Destruct(pChn);
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            bResult = ERR_UVC_BUSY;
        }     
    }
    else
    {
        aloge("fatal error! no Venc component!");
        list_del(&pChn->mList);
        uvcInput_CHN_MAP_S_Destruct(pChn);
    }

    alogd("uvc component exited!");
    return bResult;
}

ERRORTYPE AW_MPI_UVC_StartRecvPic(UVC_DEV UvcDev, UVC_CHN UvcChn)
{
    ERRORTYPE bResult = FAILURE;

    if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }
    if(UvcChn < 0 || UvcChn >= UVC_MAX_CHN_NUM)
    {
        aloge("fatal error! invalid UvcChn[%d] channel", UvcChn);
        return ERR_UVC_ILLEGAL_PARAM;
    }
    
    UVC_CHN_MAP_S *pChn = NULL;
    if(uvcInput_SearchExitDevVirChn(UvcDev, UvcChn, &pChn) != SUCCESS)
    {
        return ERR_UVC_UNEXIST;
    }
    
    COMP_STATETYPE  nCompState;
    if(SUCCESS == pChn->mUvcComp->GetState(pChn->mUvcComp, &nCompState))
    {
        if(COMP_StateIdle == nCompState)
        {
            bResult = pChn->mUvcComp->SendCommand(pChn->mUvcComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
            cdx_sem_down(&pChn->mSemCompCmd);
        }
        else
        {
            alogd("the %sUVC Channel[%d] UvcChannelState[0x%x], do nothing!", UvcDev, UvcChn, nCompState);
            bResult = SUCCESS;
        }
        
    }
    else
    {
        aloge("fatal error! GetState fail!");
        bResult = ERR_UVC_BUSY;
    }

    return bResult;
}

ERRORTYPE AW_MPI_UVC_StopRecvPic(UVC_DEV UvcDev, UVC_CHN UvcChn)
{
    ERRORTYPE bResult = FAILURE;

   if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }
    if(UvcChn < 0 || UvcChn >= UVC_MAX_CHN_NUM)
    {
        aloge("fatal error! invalid UvcChn[%d] channel", UvcChn);
        return ERR_UVC_ILLEGAL_PARAM;
    }
    
    UVC_CHN_MAP_S *pChn = NULL;
    if(uvcInput_SearchExitDevVirChn(UvcDev, UvcChn, &pChn) != SUCCESS)
    {
        return ERR_UVC_UNEXIST;
    }
    
    COMP_STATETYPE  nCompState;
    if(SUCCESS == pChn->mUvcComp->GetState(pChn->mUvcComp, &nCompState))
    {
        if(COMP_StateExecuting == nCompState)
        {
            bResult = pChn->mUvcComp->SendCommand(pChn->mUvcComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
            cdx_sem_down(&pChn->mSemCompCmd);
        }
        else if(COMP_StateIdle == nCompState)
        {
            alogv("UvcChannelState[0x%x], do nothing!", nCompState);
            bResult = SUCCESS;
        }
        else 
        {
             aloge("fatal error! check UvcChannelState[0x%x]!", nCompState);
             bResult = SUCCESS;
        }        
    }
    else
    {
        aloge("fatal error! GetState fail!");
        bResult = ERR_UVC_BUSY;
    }
    return bResult;    
}

ERRORTYPE AW_MPI_UVC_GetFrame(UVC_DEV UvcDev, UVC_CHN UvcChn, VIDEO_FRAME_INFO_S *pstFrameInfo, AW_S32 s32MilliSec)
{
    ERRORTYPE bResult = FAILURE;

    if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }
    if(UvcChn < 0 || UvcChn >= UVC_MAX_CHN_NUM)
    {
        aloge("fatal error! invalid UvcChn[%d] channel", UvcChn);
        return ERR_UVC_ILLEGAL_PARAM;
    }
    if(!pstFrameInfo)
    {
        aloge("fatal error! the input pointer is null!");        
        return ERR_UVC_ILLEGAL_PARAM;
    }
    
    UVC_CHN_MAP_S *pChn = NULL;
    if(uvcInput_SearchExitDevVirChn(UvcDev, UvcChn, &pChn) != SUCCESS)
    {
        return ERR_UVC_UNEXIST;
    }
    COMP_STATETYPE  nCompState;
    if(SUCCESS == pChn->mUvcComp->GetState(pChn->mUvcComp, &nCompState))
    {
        if(COMP_StateExecuting != nCompState && COMP_StateIdle != nCompState)
        {
            aloge("the %s UVC Channel[%d] wrong state[0x%x], return!", UvcDev, UvcChn, nCompState);
            return ERR_UVC_NOT_PERM;
        }
        UvcStream stUvcStream;
        stUvcStream.pStream = pstFrameInfo;
        stUvcStream.nMilliSec = s32MilliSec;
        //stUvcStream.streamIdx = streamIdx;
        bResult = pChn->mUvcComp->GetConfig(pChn->mUvcComp, COMP_IndexVendorGetUvcFrame, (void*)&stUvcStream);       
    }
    else
    {
        aloge("fatal error! the %s UVC Channel[%d] GetState fail!", UvcDev, UvcChn);
        bResult = ERR_UVC_BUSY; 
    }
    return bResult;
}

ERRORTYPE AW_MPI_UVC_ReleaseFrame(UVC_DEV UvcDev, UVC_CHN UvcChn, VIDEO_FRAME_INFO_S *pstFrameInfo)
{
    ERRORTYPE bResult = FAILURE;

    if(!UvcDev)
    {
        aloge("fatal error! the UvcDev is null");
        return ERR_UVC_NULL_PTR;
    }
    if(UvcChn < 0 || UvcChn >= UVC_MAX_CHN_NUM)
    {
        aloge("fatal error! invalid UvcChn[%d] channel", UvcChn);
        return ERR_UVC_ILLEGAL_PARAM;
    }
    if(!pstFrameInfo)
    {
        aloge("fatal error! the input pointer is null!");        
        return ERR_UVC_ILLEGAL_PARAM;
    }
    
    UVC_CHN_MAP_S *pChn = NULL;
    if(uvcInput_SearchExitDevVirChn(UvcDev, UvcChn, &pChn) != SUCCESS)
    {
        return ERR_UVC_UNEXIST;
    }
    COMP_STATETYPE  nCompState;
    if(SUCCESS == pChn->mUvcComp->GetState(pChn->mUvcComp, &nCompState))
    {
        if(COMP_StateExecuting != nCompState && COMP_StateIdle != nCompState)
        {
            aloge("the %s UVC Channel[%d] wrong state[0x%x], return!",UvcDev, UvcChn, nCompState);
            return ERR_UVC_NOT_PERM;
        }
        UvcStream stUvcStream;
        stUvcStream.pStream = pstFrameInfo;
        //stUvcStream.streamIdx = streamIdx;
        bResult = pChn->mUvcComp->SetConfig(pChn->mUvcComp, COMP_IndexVendorReleaseUvcFrame, (void*)&stUvcStream);
    }
    else
    {
        aloge("fatal error! the %s UVC Channel[%d] GetState fail!", UvcDev, UvcChn);
        bResult = ERR_UVC_BUSY;      
    }

    return bResult;
}


ERRORTYPE AW_MPI_UVC_SetBrightness(UVC_DEV UvcDev, int Value)
{
    return uvcInput_SetControl(UvcDev, V4L2_CID_BRIGHTNESS, Value, "Brightness");
}

ERRORTYPE AW_MPI_UVC_GetBrightness(UVC_DEV UvcDev, int *pValue)
{
    if(pValue)
    {
        return uvcInput_GetControl(UvcDev, V4L2_CID_BRIGHTNESS, pValue, "Brightness");
    }
    return ERR_UVC_NULL_PTR;
}

ERRORTYPE AW_MPI_UVC_SetContrast(UVC_DEV UvcDev, int Value)
{
    return uvcInput_SetControl(UvcDev, V4L2_CID_CONTRAST, Value, "Contrast");
}

ERRORTYPE AW_MPI_UVC_GetContrast(UVC_DEV UvcDev, int *pValue)
{
    if(pValue)
    {
        return uvcInput_GetControl(UvcDev, V4L2_CID_CONTRAST, pValue, "Contrast");
    }
    return ERR_UVC_NULL_PTR;
}

ERRORTYPE AW_MPI_UVC_SetHue(UVC_DEV UvcDev, int Value)
{
    return uvcInput_SetControl(UvcDev, V4L2_CID_HUE, Value, "Hue");
}

ERRORTYPE AW_MPI_UVC_GetHue(UVC_DEV UvcDev, int *pValue)
{
    if(pValue)
    {
        return uvcInput_GetControl(UvcDev, V4L2_CID_HUE, pValue, "Hue");
    }
    return ERR_UVC_NULL_PTR;
}

ERRORTYPE AW_MPI_UVC_SetSaturation(UVC_DEV UvcDev, int Value)
{
    return uvcInput_SetControl(UvcDev, V4L2_CID_SATURATION, Value, "Saturation");
}

ERRORTYPE AW_MPI_UVC_GetSaturation(UVC_DEV UvcDev, int *pValue)
{
    if(pValue)
    {
        return uvcInput_GetControl(UvcDev, V4L2_CID_SATURATION, pValue, "Saturation");
    }
    return ERR_UVC_NULL_PTR;
}

ERRORTYPE AW_MPI_UVC_SetSharpness(UVC_DEV UvcDev, int Value)
{
    return uvcInput_SetControl(UvcDev, V4L2_CID_SHARPNESS, Value, "Sharpness");
}

ERRORTYPE AW_MPI_UVC_GetSharpness(UVC_DEV UvcDev, int *pValue)
{
    if(pValue)
    {
        return uvcInput_GetControl(UvcDev, V4L2_CID_SHARPNESS, pValue, "sharpness");
    }
    return ERR_UVC_NULL_PTR;
}


