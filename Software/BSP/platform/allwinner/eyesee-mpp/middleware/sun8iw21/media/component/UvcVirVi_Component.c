/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : UvcVirVi_Component.c
  Version       : 
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/12/10
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "UvcVirVi_Component"
#include <utils/plat_log.h>
#include <errno.h>
#include <string.h>
#include <sys/prctl.h>
#include <cdx_list.h>

#include <ComponentCommon.h>
#include <mm_component.h>
#include <cedarx_demux.h>
#include <EncodedStream.h>
#include <UvcCompStream.h>
#include "UvcVirVi_Component.h"

#define UVC_FIFO_LEVEL_INIT (30)

static void *Uvc_ComponentThread(void *pThreadData);

ERRORTYPE UvcSetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent,
                                PARAM_IN COMP_CALLBACKTYPE* pCallbacks, 
                                PARAM_IN void* pAppData)
{
    ERRORTYPE eError = SUCCESS;

    if(!hComponent || !pCallbacks || !pAppData)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pUvcInputData->pCallbacks = pCallbacks;
    pUvcInputData->pAppData = pAppData;

COMP_CONF_CMD_FAIL:
        return eError;    
}

ERRORTYPE UvcGetState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_STATETYPE* pState )
{
    ERRORTYPE eError = SUCCESS;
    if(!hComponent || !pState)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }
    
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pState = pUvcInputData->state;
    return eError;
}

ERRORTYPE UvcSendCommand(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_COMMANDTYPE Cmd, 
        PARAM_IN unsigned int nParam1,
        PARAM_IN void* pCmdData)
{
    ERRORTYPE eError = SUCCESS;

    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    CompInternalMsgType eCmd;
    message_t msg;

    if(COMP_StateInvalid == pUvcInputData->state)
    {
        eError = ERR_UVC_SYS_NOTREADY;
        return eError;
    }
    switch(Cmd)
    {
        case COMP_CommandStateSet:
            eCmd = SetState;
            break;
        case COMP_CommandFlush:
            eCmd = Flush;
            break;
        default:
            alogw("impossible comp_command[0x%x]", Cmd);
            break;        
    }
    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pUvcInputData->cmd_queue, &msg);

    return eError;    
}

ERRORTYPE UvcSetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MPP_CHN_S *pChn)
{
    ERRORTYPE eError = SUCCESS;
    if(!hComponent || !pChn)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }

    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pUvcInputData->mMppChnInfo = *pChn;
    return eError;
}

ERRORTYPE UvcGetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MPP_CHN_S *pChn)
{
    ERRORTYPE eError = SUCCESS;
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pChn = pUvcInputData->mMppChnInfo;
    return eError;
}


ERRORTYPE UvcSetDevInfo(PARAM_IN COMP_HANDLETYPE hComponent,
                             PARAM_IN UvcChnManager *pUvcChnManager)
{
    ERRORTYPE eError = SUCCESS;    
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pUvcInputData->pUvcChnManager = pUvcChnManager;

    return eError;
}


ERRORTYPE UvcGetPortDefinition(PARAM_IN COMP_HANDLETYPE hComponent,
                                        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    ERRORTYPE eError = SUCCESS;
    if(!hComponent || !pPortDef)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }

    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    BOOL bFindFlag = FALSE;
    int i;
    for(i = 0; i < UVC_CHN_MAX_PORTS; i++)
    {
        if(pUvcInputData->sPortDef[i].nPortIndex == pPortDef->nPortIndex)
        {
            memcpy(pPortDef, &pUvcInputData->sPortDef[i], sizeof pUvcInputData->sPortDef[i]);
            bFindFlag = TRUE;
        }
    }
    if(!bFindFlag)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
    }
    return eError;
}
                                           
ERRORTYPE UvcSetPortDefinition(PARAM_IN COMP_HANDLETYPE hComponent,
                                        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    ERRORTYPE eError = SUCCESS;
    if(!hComponent || !pPortDef)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }

    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    BOOL bFindFlag = FALSE;
    int i;
    for(i = 0; i < UVC_CHN_MAX_PORTS; i++)
    {
        if(pUvcInputData->sPortDef[i].nPortIndex == pPortDef->nPortIndex)
        {
            memcpy(&pUvcInputData->sPortDef[i], pPortDef, sizeof pUvcInputData->sPortDef[i]);
            bFindFlag = TRUE;
        }
    }
    if(!bFindFlag)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
    }
    return eError;
    
}

ERRORTYPE UvcGetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    ERRORTYPE eError = SUCCESS;
    if(!hComponent || !pPortBufSupplier)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }

    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    BOOL bFindFlag = FALSE;
    int i;
    for(i = 0; i < UVC_CHN_MAX_PORTS; i++)
    {
        if(pUvcInputData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            memcpy(pPortBufSupplier, &pUvcInputData->sPortBufSupplier[i], sizeof pUvcInputData->sPortBufSupplier[i]);
            bFindFlag = TRUE;
            break;
        }
    }

    if(!bFindFlag)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE UvcSetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    ERRORTYPE eError = SUCCESS;
    if(!hComponent || !pPortBufSupplier)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }

    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    BOOL bFindFlag = FALSE;
    int i;
    for(i = 0; i < UVC_CHN_MAX_PORTS; i++)
    {
        if(pUvcInputData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            memcpy(&pUvcInputData->sPortBufSupplier[i], pPortBufSupplier, sizeof pUvcInputData->sPortBufSupplier[i]);
            bFindFlag = TRUE;
            break;
        }
    }

    if(!bFindFlag)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE UvcGetPortExtraDef(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT COMP_PARAM_PORTEXTRADEFINITIONTYPE *pPortDef)
{
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    int portIdx = pPortDef->nPortIndex;
    if(!pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_DATA_OUT].pVendorInfo)
    {
        VideoStreamInfo *ptr = malloc(sizeof(VideoStreamInfo));
        if(!ptr)
        {
            aloge("can not malloc the VideoStreamInfo!!");
            return ERR_UVC_NOMEM;
        }
        memset(ptr, 0, sizeof(VideoStreamInfo));
        if(V4L2_PIX_FMT_MJPEG == pUvcInputData->pUvcChnManager->mUvcAttr.mPixelformat)
        {
            ptr->eCodecFormat = VIDEO_CODEC_FORMAT_MJPEG;
            ptr->bIsFramePackage = 1;
        }
        else if(V4L2_PIX_FMT_H264 == pUvcInputData->pUvcChnManager->mUvcAttr.mPixelformat)
        {
            ptr->eCodecFormat = VIDEO_CODEC_FORMAT_H264;
        }
        else
        {
            ptr->eCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
        }
        ptr->nWidth = pUvcInputData->pUvcChnManager->mUvcAttr.mUvcVideo_Width;
        ptr->nHeight = pUvcInputData->pUvcChnManager->mUvcAttr.mUvcVideo_Height;
        ptr->nFrameRate = (pUvcInputData->pUvcChnManager->mUvcAttr.mUvcVideo_Fps)* 1000;
        ptr->nFrameDuration = (1000*1000*1000) / (ptr->nFrameRate);
        ptr->bIsFramePackage = 1;
        
        pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_DATA_OUT].pVendorInfo = (void *)ptr;
    }
     
    if(portIdx == UVC_CHN_PORT_INDEX_DATA_OUT)
    {
        memcpy(pPortDef, &pUvcInputData->sPortExtraDef[portIdx], sizeof(COMP_PARAM_PORTEXTRADEFINITIONTYPE));
        return TRUE;
    }
    else
    {
        return FAILURE;
    }
}


ERRORTYPE UvcComponentTunnelRequest(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  unsigned int nPort,
        PARAM_IN  COMP_HANDLETYPE hTunneledComp,
        PARAM_IN  unsigned int nTunneledPort,
        PARAM_INOUT  COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    if(!hComponent)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }

    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(COMP_StateExecuting == pUvcInputData->state)
    {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    }
    else if(pUvcInputData->state != COMP_StateIdle)
    {
        aloge("fatal error! tunnel request process_thread_state can't be in state[0x%x]", pUvcInputData->state);
        eError = ERR_UVC_INCORRECT_STATE_OPERATION;
        return eError;
    }

    COMP_PARAM_PORTDEFINITIONTYPE   *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE    *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE   *pPortBufSupplier;
    BOOL bFindFlag = FALSE;

    int i;
    for(i = 0; i < UVC_CHN_MAX_PORTS; i++)
    {
        if(pUvcInputData->sPortDef[i].nPortIndex == nPort)
        {
            pPortDef = &pUvcInputData->sPortDef[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(!bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }
    
    bFindFlag = FALSE;
    for(i = 0; i < UVC_CHN_MAX_PORTS; i++)
    {
        if(pUvcInputData->sPortTunnelInfo[i].nPortIndex == nPort)
        {
            pPortTunnelInfo = &pUvcInputData->sPortTunnelInfo[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(!bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }

    bFindFlag = FALSE;
    for(i = 0; i < UVC_CHN_MAX_PORTS; i++)
    {
        if(pUvcInputData->sPortBufSupplier[i].nPortIndex == nPort)
        {
            pPortBufSupplier = &pUvcInputData->sPortBufSupplier[i];
            bFindFlag = TRUE;
            break;
        }
    }
    
    if(!bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AI_ILLEGAL_PARAM;
        return eError;
    }

    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther)?TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if(NULL == hTunneledComp && 0 == nTunneledPort && NULL == pTunnelSetup)
    {
        alogd("omx_core cancel setup tunnel on port[%d]", nPort);
        eError = SUCCESS;
        if(pPortDef->eDir == COMP_DirOutput)
        {
            pUvcInputData->mOutputPortTunnelFlag = FALSE;
        }
        else
        {
            //pVideoViData->mInputPortTunnelFlag = FALSE;
        }
        return eError;
    }

    if(COMP_DirOutput == pPortDef->eDir)
    {
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        pUvcInputData->mOutputPortTunnelFlag = TRUE;
    }
    else
    {
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE *)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if(out_port_def.eDir != COMP_DirOutput)
        {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_UVC_ILLEGAL_PARAM;
            return eError;
        }

        pPortDef->format = out_port_def.format;
        if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier)
        {
            alogw("Low probability! use input portIndex[%d] buffer supplier[%d] as final!", nPort, pPortBufSupplier->eBufferSupplier);
            pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;   
        }

        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE *)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;        
        ((MM_COMPONENTTYPE *)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
    }
    return eError;
}

static VIDEO_FRAME_INFO_S *UvcGetReadyFrame(UVCDATATYPE *pUvcInputData)
{
    UvcFrameInfo *pEntry = NULL;

    pthread_mutex_lock(&pUvcInputData->mOutFrameLock);

    if(list_empty(&pUvcInputData->mReadyOutFrameList))
    {
        pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);
        return NULL;
    }

    pEntry = list_first_entry(&pUvcInputData->mReadyOutFrameList, UvcFrameInfo, mList);
    list_move_tail(&pEntry->mList, &pUvcInputData->mUsedOutFrameList);
    pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);

    return &pEntry->VFrame;
}

static ERRORTYPE UvcReleaseFrame(UVCDATATYPE *pUvcInputData, VIDEO_FRAME_INFO_S *pFrame)
{
    UvcFrameInfo *pEntry, *pTmp;
    BOOL bFindFlag = FALSE;

    pthread_mutex_lock(&pUvcInputData->mOutFrameLock);
    list_for_each_entry_safe(pEntry, pTmp, &pUvcInputData->mUsedOutFrameList, mList)
    {
        if(pEntry->VFrame.mId == pFrame->mId)
        {
            list_del(&pEntry->mList);
            bFindFlag = TRUE;
            break;
        }
    }

    if(list_empty(&pUvcInputData->mUsedOutFrameList))
    {
        pthread_cond_signal(&pUvcInputData->mWaiteUsedFrmeEmpty);
    }

    if(bFindFlag)
    {
        list_add_tail(&pEntry->mList, &pUvcInputData->mIdleOutFrameList);
    }
    else
    {
        aloge("Unknown video virvi frame, frame id[%d]!", pFrame->mId);
        pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);
        return FAILURE;
    }
    pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);
    return SUCCESS;
}

ERRORTYPE UvcGetData(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT UvcStream *pstParams)
{
    ERRORTYPE eError = SUCCESS;
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pUvcInputData->state != COMP_StateIdle && pUvcInputData->state != COMP_StateExecuting)
    {
        alogw("call getStream in wrong state[0x%x]", pUvcInputData->state);
        return ERR_UVC_NOT_PERM;
    }

    if(pUvcInputData->mOutputPortTunnelFlag)
    {
        return ERR_UVC_NOT_PERM;
    }

    VIDEO_FRAME_INFO_S *pstFrameInfo = pstParams->pStream;  
    int nMilliSec = pstParams->nMilliSec;
    
_TryToGetFrame:    
    if(list_empty(&pUvcInputData->mReadyOutFrameList))
    {
        if(0 == nMilliSec)
        {
            eError = FAILURE;
        }
        else if(nMilliSec < 0)
        {
            pUvcInputData->mWaitingCapDataFlag = TRUE;
            cdx_sem_down(&pUvcInputData->mSemWaitInputFrame);
            pUvcInputData->mWaitingCapDataFlag = FALSE;
            goto _TryToGetFrame;
        }
        else
        {
            pUvcInputData->mWaitingCapDataFlag = TRUE;
            int ret = cdx_sem_down_timedwait(&pUvcInputData->mSemWaitInputFrame, nMilliSec);
            if(ETIMEDOUT == ret)
            {
                eError = FAILURE;
                pUvcInputData->mWaitingCapDataFlag = FALSE;
            }
            else if(0 == ret)
            {
                pUvcInputData->mWaitingCapDataFlag = FALSE;
                goto _TryToGetFrame;
            }
            else
            {
                eError = FAILURE;
                pUvcInputData->mWaitingCapDataFlag = FALSE;
            }
        }       
    }
    else
    {
        pthread_mutex_lock(&pUvcInputData->mOutFrameLock);
        
        UvcFrameInfo *pEntry = list_first_entry(&pUvcInputData->mReadyOutFrameList, UvcFrameInfo, mList);
        memcpy(pstFrameInfo, &pEntry->VFrame, sizeof(VIDEO_FRAME_INFO_S));
        list_move_tail(&pEntry->mList, &pUvcInputData->mUsedOutFrameList); 

        pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);
    }

    return eError;
}

static ERRORTYPE UvcReleaseData(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN UvcStream *pstParams)
{
    ERRORTYPE eError = SUCCESS;
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pUvcInputData->state != COMP_StateIdle && pUvcInputData->state != COMP_StateExecuting)
    {
        alogw("call getStream in wrong state[0x%x]", pUvcInputData->state);
        return ERR_UVC_NOT_PERM;
    }

    if(pUvcInputData->mOutputPortTunnelFlag)
    {
        return ERR_UVC_NOT_PERM;
    }

    VIDEO_FRAME_INFO_S *pstFrameInfo = pstParams->pStream;

    uvcInput_RefsReduceAndRleaseData2(pUvcInputData->pUvcChnManager, pstFrameInfo);

    UvcReleaseFrame(pUvcInputData, pstFrameInfo);

    return eError;
    
}


static ERRORTYPE UvcEmptyThisBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    ERRORTYPE eError    = SUCCESS;
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pUvcInputData->mStateLock);
    if(pUvcInputData->state != COMP_StateExecuting)
    {
        aloge("send frame when uvc state[0x%x], is not executing", pUvcInputData->state);
        eError = FAILURE;
        goto err_state;
    }

    if(pBuffer->nInputPortIndex == pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].nPortIndex)
    {
        VIDEO_FRAME_INFO_S *pFrm = (VIDEO_FRAME_INFO_S *) pBuffer->pOutputPortPrivate;

        pthread_mutex_lock(&pUvcInputData->mOutFrameLock);
        if(list_empty(&pUvcInputData->mIdleOutFrameList))
        {
            pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);
            aloge("the IdleOutFrameList is empty!");
            eError = FAILURE;
            goto err_mem;
        }
        UvcFrameInfo *pEnty = list_first_entry(&pUvcInputData->mIdleOutFrameList, UvcFrameInfo, mList);
        pEnty->VFrame = *pFrm;
        list_move_tail(&pEnty->mList, &pUvcInputData->mReadyOutFrameList);       
        pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);
        
        if(pUvcInputData->mWaitingCapDataFlag)
        {
            if(pUvcInputData->mOutputPortTunnelFlag)
            {
                pUvcInputData->mWaitingCapDataFlag = FALSE;
                message_t msg;
                msg.command = UvcComp_InputFrameAvailable;
                put_message(&pUvcInputData->cmd_queue, &msg);
            }
            else
            {
                pUvcInputData->mWaitingCapDataFlag = FALSE;
                cdx_sem_up(&pUvcInputData->mSemWaitInputFrame);
            }
            
        }
    }
    else
    {
        aloge("fatal error! the portindex can not fit!!");
    }
err_mem:
    

err_state:    
    pthread_mutex_unlock(&pUvcInputData->mStateLock);
    return eError;
}

ERRORTYPE UvcFillThisBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    ERRORTYPE eError    = SUCCESS;
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    VIDEO_FRAME_INFO_S *pDstFrame = NULL;
    if(pBuffer->nOutputPortIndex == pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].nPortIndex)
    {
        if(UVC_H264 == pUvcInputData->pUvcChnManager->mUvcAttr.mPixelformat
            || UVC_MJPEG == pUvcInputData->pUvcChnManager->mUvcAttr.mPixelformat)
        {
            EncodedStream *pEncodedFrame = (EncodedStream*)pBuffer->pOutputPortPrivate;

            UvcFrameInfo *pEntry, *pTmp;
            int nFindFlag = 0;

            pthread_mutex_lock(&pUvcInputData->mOutFrameLock);
            list_for_each_entry_safe(pEntry, pTmp, &pUvcInputData->mUsedOutFrameList, mList)
            {
                if(pEntry->VFrame.mId == pEncodedFrame->nID)
                {
                    if(0 == nFindFlag)
                    {
                        pDstFrame = &pEntry->VFrame;
                    }
                    nFindFlag++;
                    if(nFindFlag > 1)
                    {
                        aloge("fatal error! why find [%d]frames? frameId[%d]", nFindFlag, pEncodedFrame->nID);
                    }
                    //break;
                }
            }
            pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);
            if(NULL == pDstFrame)
            {
                aloge("fatal error! can't find frameId[%d], check code!", pEncodedFrame->nID);
            }
        }
        else
        {
            pDstFrame = (VIDEO_FRAME_INFO_S*)pBuffer->pOutputPortPrivate;
        }

        if(pDstFrame != NULL)
        {
            uvcInput_RefsReduceAndRleaseData2(pUvcInputData->pUvcChnManager, pDstFrame);
            UvcReleaseFrame(pUvcInputData, pDstFrame);
        }
        else
        {
            aloge("fatal error! frame is null! check code!");
        }
    }
    else
    {
        aloge("fatal error! outputPortIndex[%d]!=[%d]", pBuffer->nOutputPortIndex, pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].nPortIndex);
    }

    return eError;
}


ERRORTYPE UvcSetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;
    
    switch(nIndex)
    {
        case COMP_IndexVendorMPPChannelInfo:
            eError = UvcSetMPPChannelInfo(hComponent,(MPP_CHN_S *)pComponentConfigStructure);
            break;
        
        case COMP_IndexVendorUvcChnAttr:
            break;
            
        case COMP_IndexParamPortDefinition:
            eError = UvcSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure);
            break;
        
        case COMP_IndexParamCompBufferSupplier:
            eError = UvcSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *) pComponentConfigStructure);
            break;

        case COMP_IndexVendorReleaseUvcFrame:
            eError = UvcReleaseData(hComponent, (UvcStream *)pComponentConfigStructure);
            break;

        case COMP_IndexVendorUvcSetDevInfo:
            eError = UvcSetDevInfo(hComponent, (UvcChnManager *)pComponentConfigStructure);
            break;
        
        default :
            eError = FAILURE;
            aloge("fatal error! check the nIndex[0x%x] !", nIndex);
            break;
            
    }
    return eError;
}


ERRORTYPE UvcGetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;

    switch(nIndex)
    {
        case COMP_IndexVendorMPPChannelInfo:
            eError = UvcGetMPPChannelInfo(hComponent, (MPP_CHN_S *)pComponentConfigStructure);
            break;
        case COMP_IndexParamPortDefinition:
            eError = UvcGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure);
            break;
        
        case COMP_IndexParamCompBufferSupplier:
            eError = UvcGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *)pComponentConfigStructure);
            break;
        
        case COMP_IndexVendorGetUvcFrame:
            eError = UvcGetData(hComponent, (UvcStream *)pComponentConfigStructure);
            break;

        case COMP_IndexVendorParamPortExtraDefinition:
            eError = UvcGetPortExtraDef(hComponent, (COMP_PARAM_PORTEXTRADEFINITIONTYPE *)pComponentConfigStructure);
            break;
        default :
            eError = FAILURE;
            aloge("fatal error! check the nIndex[0x%x] !", nIndex);
            break;      
    }
    return eError;    
}

ERRORTYPE UvcComponentDeInit(void *hComponent)
{
    ERRORTYPE eError = SUCCESS;
    if(!hComponent)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }

    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    CompInternalMsgType eCmd = Stop;
    message_t msg;
    int ret = 0,i = 0;

    msg.command = eCmd;
    put_message(&pUvcInputData->cmd_queue, &msg);
    pthread_join(pUvcInputData->thread_id, (void *)&eError);
    message_destroy(&pUvcInputData->cmd_queue);

    pthread_mutex_lock(&pUvcInputData->mOutFrameLock);
    if(!list_empty(&pUvcInputData->mReadyOutFrameList))
    {
        aloge("fatal error! why readyOutFramelist is not empty?");
    }
    if(!list_empty(&pUvcInputData->mIdleOutFrameList))
    {
        UvcFrameInfo *pEntry,*pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pUvcInputData->mIdleOutFrameList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }     
    }
    pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);

    pthread_cond_destroy(&pUvcInputData->mWaiteUsedFrmeEmpty);
    pthread_mutex_destroy(&pUvcInputData->mOutFrameLock);
    cdx_sem_deinit(&pUvcInputData->mSemWaitInputFrame);
    pthread_mutex_destroy(&pUvcInputData->mStateLock);

    if(pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_DATA_OUT].pVendorInfo)
    {
        free((VideoStreamInfo *)pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_DATA_OUT].pVendorInfo);
        pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_DATA_OUT].pVendorInfo = NULL;
    }

    free(pUvcInputData);

    return eError;
    
}


ERRORTYPE UvcComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    ERRORTYPE eError = SUCCESS;
    if(!hComponent)
    {
        eError = ERR_UVC_ILLEGAL_PARAM;
        return eError;
    }

    int ret = 0, i = 0;
    MM_COMPONENTTYPE *pComp = (MM_COMPONENTTYPE *)hComponent;

    UVCDATATYPE *pUvcInputData = (UVCDATATYPE*)malloc(sizeof(UVCDATATYPE));
    if(!pUvcInputData)
    {
        eError = ERR_UVC_NOMEM;
        return eError;
    }
    memset(pUvcInputData, 0, sizeof *pUvcInputData);


    pComp->pComponentPrivate = (void *)pUvcInputData;
    pUvcInputData->hSelf = hComponent;
    pUvcInputData->state = COMP_StateLoaded;
    pthread_mutex_init(&pUvcInputData->mStateLock, NULL);

    pUvcInputData->mWaitingCapDataFlag = FALSE;    
    cdx_sem_init(&pUvcInputData->mSemWaitInputFrame, 0 );
    
    pthread_cond_init(&pUvcInputData->mWaiteUsedFrmeEmpty, NULL);
    pthread_mutex_init(&pUvcInputData->mOutFrameLock, NULL);
    INIT_LIST_HEAD(&pUvcInputData->mIdleOutFrameList);
    INIT_LIST_HEAD(&pUvcInputData->mReadyOutFrameList);
    INIT_LIST_HEAD(&pUvcInputData->mUsedOutFrameList);

    for(i = 0; i < UVC_FIFO_LEVEL_INIT; i++)
    {
        UvcFrameInfo *pUvcFrameInfo = (UvcFrameInfo *)malloc(sizeof(UvcFrameInfo));
        if(!pUvcFrameInfo)
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            eError = ERR_UVC_NOMEM;
            return eError;
        }
        list_add_tail(&pUvcFrameInfo->mList, &pUvcInputData->mIdleOutFrameList);
    }
        
    pComp->SetCallbacks     = UvcSetCallbacks;
    pComp->SendCommand      = UvcSendCommand;
    pComp->GetConfig        = UvcGetConfig;
    pComp->SetConfig        = UvcSetConfig;
    pComp->GetState         = UvcGetState;
    pComp->ComponentTunnelRequest = UvcComponentTunnelRequest;
    pComp->ComponentDeInit  = UvcComponentDeInit;
    pComp->EmptyThisBuffer  = UvcEmptyThisBuffer;
    pComp->FillThisBuffer   = UvcFillThisBuffer;

    pUvcInputData->sPortParam.nPorts = 0;
    pUvcInputData->sPortParam.nStartPortNumber = 0;

    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].nPortIndex = pUvcInputData->sPortParam.nPorts;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].bEnabled = TRUE;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].eDomain = COMP_PortDomainVideo;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].eDir = COMP_DirInput;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].format.video.cMIMEType = "YVU420";
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].format.video.nFrameWidth = 176;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].format.video.nFrameHeight = 144;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].format.video.eCompressionFormat = PT_BUTT;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_NCOM_IN].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pUvcInputData->sPortBufSupplier[UVC_CHN_PORT_INDEX_NCOM_IN].nPortIndex = pUvcInputData->sPortParam.nPorts;
    pUvcInputData->sPortBufSupplier[UVC_CHN_PORT_INDEX_NCOM_IN].eBufferSupplier = COMP_BufferSupplyInput;
    pUvcInputData->sPortTunnelInfo[UVC_CHN_PORT_INDEX_NCOM_IN].nPortIndex = pUvcInputData->sPortParam.nPorts;
    pUvcInputData->sPortTunnelInfo[UVC_CHN_PORT_INDEX_NCOM_IN].eTunnelType = TUNNEL_TYPE_COMMON;
    pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_NCOM_IN].nPortIndex = pUvcInputData->sPortParam.nPorts;
    pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_NCOM_IN].pVendorInfo = NULL;
    pUvcInputData->sPortParam.nPorts++;

//    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_COMP_IN].nPortIndex = pUvcInputData->sPortParam.nPorts;
//    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_COMP_IN].bEnabled = TRUE;
//    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_COMP_IN].eDomain = COMP_PortDomainVideo;
//    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_COMP_IN].eDir = COMP_DirInput;
//    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_COMP_IN].format.video.cMIMEType = "YVU420";
//    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_COMP_IN].format.video.nFrameWidth = 176;
//    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_COMP_IN].format.video.nFrameHeight = 144;
//    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_COMP_IN].format.video.eCompressionFormat = PT_BUTT;
//    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_COMP_IN].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
//    pUvcInputData->sPortBufSupplier[UVC_CHN_PORT_INDEX_COMP_IN].nPortIndex = pUvcInputData->sPortParam.nPorts;
//    pUvcInputData->sPortBufSupplier[UVC_CHN_PORT_INDEX_COMP_IN].eBufferSupplier = COMP_BufferSupplyInput;
//    pUvcInputData->sPortTunnelInfo[UVC_CHN_PORT_INDEX_COMP_IN].nPortIndex = pUvcInputData->sPortParam.nPorts;
//    pUvcInputData->sPortTunnelInfo[UVC_CHN_PORT_INDEX_COMP_IN].eTunnelType = TUNNEL_TYPE_COMMON;
//    pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_COMP_IN].nPortIndex = pUvcInputData->sPortParam.nPorts;
//    pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_COMP_IN].pVendorInfo = NULL;
//    pUvcInputData->sPortParam.nPorts++;

    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].nPortIndex = pUvcInputData->sPortParam.nPorts;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].bEnabled = TRUE;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].eDomain = COMP_PortDomainVideo;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].eDir = COMP_DirOutput;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].format.video.cMIMEType = "YVU420";
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].format.video.nFrameWidth = 176;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].format.video.nFrameHeight = 144;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].format.video.eCompressionFormat = PT_BUTT;  // YCbCr420;
    pUvcInputData->sPortDef[UVC_CHN_PORT_INDEX_DATA_OUT].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pUvcInputData->sPortBufSupplier[UVC_CHN_PORT_INDEX_DATA_OUT].nPortIndex = pUvcInputData->sPortParam.nPorts;
    pUvcInputData->sPortBufSupplier[UVC_CHN_PORT_INDEX_DATA_OUT].eBufferSupplier = COMP_BufferSupplyOutput;
    pUvcInputData->sPortTunnelInfo[UVC_CHN_PORT_INDEX_DATA_OUT].nPortIndex = pUvcInputData->sPortParam.nPorts;
    pUvcInputData->sPortTunnelInfo[UVC_CHN_PORT_INDEX_DATA_OUT].eTunnelType = TUNNEL_TYPE_COMMON;

    pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_DATA_OUT].nPortIndex = pUvcInputData->sPortParam.nPorts;
    pUvcInputData->sPortExtraDef[UVC_CHN_PORT_INDEX_DATA_OUT].pVendorInfo = NULL;
    
    pUvcInputData->sPortParam.nPorts++;

    if(message_create(&pUvcInputData->cmd_queue) < 0)
    {
        aloge("message error!");
        eError = ERR_UVC_NOMEM;
        return eError;
    }

    if(pthread_create(&pUvcInputData->thread_id, NULL, Uvc_ComponentThread, pUvcInputData) || !pUvcInputData->thread_id)
    {
        aloge("creat Uvc_ComponentThread fail!");
        eError = ERR_UVC_NOMEM;
        return eError;
    }
    alogw("UvcVirvi_Component had Init!!!");
    
    return eError;
}

static void *Uvc_ComponentThread(void *pThreadData)
{
    unsigned int cmddaata;
    CompInternalMsgType cmd;
    UVCDATATYPE *pUvcInputData = (UVCDATATYPE *)pThreadData;
    message_t   cmd_msg;

    alogd("UVC ComponentThread start run!");
    prctl(PR_SET_NAME, "UvcComponentThread", 0, 0, 0);

    while(1)
    {
    PROCESS_MESSAGE:
        if(0 == get_message(&pUvcInputData->cmd_queue, &cmd_msg))
        {
            cmd = cmd_msg.command;
            cmddaata = (unsigned int)cmd_msg.para0;
            if(cmd == SetState)
            {
                pthread_mutex_lock(&pUvcInputData->mStateLock);
                if(pUvcInputData->state == (COMP_STATETYPE)cmddaata)
                {
                    pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                            COMP_EventError, ERR_UVC_SAMESTATE, 0, NULL);
                }
                else
                {
                    switch((COMP_STATETYPE)cmddaata)
                    {
                        case COMP_StateInvalid:
                            pUvcInputData->state = COMP_StateInvalid;
                            pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                    COMP_EventError, ERR_UVC_INVALIDSTATE, 0, NULL);
                            pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                    COMP_EventCmdComplete, COMP_CommandStateSet, 
                                                                    pUvcInputData->state, NULL);                                    
                            break;

                        case COMP_StateLoaded:
                            if(pUvcInputData->state != COMP_StateIdle)
                            {
                                
                                pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                        COMP_EventError, ERR_UVC_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            else
                            {
                                pthread_mutex_lock(&pUvcInputData->mOutFrameLock);
                                if(!list_empty(&pUvcInputData->mReadyOutFrameList))
                                {
                                    aloge("fatal error! ready frame list is not empty!");
                                }
                                while(!list_empty(&pUvcInputData->mUsedOutFrameList))
                                {
                                    alogd("wait all Using frame return");
                                    pthread_cond_wait(&pUvcInputData->mWaiteUsedFrmeEmpty, &pUvcInputData->mOutFrameLock);
                                }
                                pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);
                                pUvcInputData->state = COMP_StateLoaded;
                                pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet, 
                                                                        pUvcInputData->state, NULL);                                    
                            }
                            break;
                            
                        case COMP_StateIdle:
                            if(COMP_StateLoaded == pUvcInputData->state)
                            {
                                pUvcInputData->state = COMP_StateIdle;                                
                                pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet, 
                                                                        pUvcInputData->state, NULL);
                            }
                            else if(COMP_StatePause == pUvcInputData->state || COMP_StateExecuting == pUvcInputData->state)
                            {
                                pthread_mutex_lock(&pUvcInputData->mOutFrameLock);
                                if(!list_empty(&pUvcInputData->mReadyOutFrameList))
                                {
                                    int num = 0;
                                    UvcFrameInfo *pEntry, *pTmp;
                                    list_for_each_entry_safe(pEntry, pTmp, &pUvcInputData->mReadyOutFrameList, mList)
                                    {
                                        uvcInput_RefsReduceAndRleaseData2(pUvcInputData->pUvcChnManager, &pEntry->VFrame);
                                        list_move_tail(&pEntry->mList, &pUvcInputData->mIdleOutFrameList);
                                        num++;
                                    }
                                    alogd("There are [%d] ready frames to be return!", num);
                                }
                                if(!list_empty(&pUvcInputData->mUsedOutFrameList))
                                {
                                    int cnt = 0;
                                    struct list_head *pList;
                                    list_for_each(pList, &pUvcInputData->mUsedOutFrameList) { cnt++; }
                                    alogd("Be careful! [%d]used out frame not return, need wait when in convertion to loaded", cnt);
                                }
                                pthread_mutex_unlock(&pUvcInputData->mOutFrameLock);
                                pUvcInputData->state = COMP_StateIdle;
                                pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet, 
                                                                        pUvcInputData->state, NULL);
                            }
                            else
                            {
                                aloge("fatal error! current state[0x%x] can't turn to idle!", pUvcInputData->state);
                                pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                        COMP_EventError, ERR_UVC_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            break;
                            
                        case COMP_StateExecuting:
                            if(COMP_StateIdle == pUvcInputData->state || COMP_StateExecuting == pUvcInputData->state
                                || COMP_StatePause == pUvcInputData->state)
                            {
                                pUvcInputData->state = COMP_StateExecuting; 
                                pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet, 
                                                                        pUvcInputData->state, NULL);
                            }
                            else
                            {
                                pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                        COMP_EventError, ERR_UVC_INCORRECT_STATE_TRANSITION, 0, NULL);   
                            }
                            break;
                            
                        case COMP_StatePause:
                            if(COMP_StateIdle == pUvcInputData->state || COMP_StateExecuting == pUvcInputData->state)
                            {
                                pUvcInputData->state = COMP_StatePause;
                                pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet, 
                                                                        pUvcInputData->state, NULL);
                            }
                            else
                            {
                                pUvcInputData->pCallbacks->EventHandler(pUvcInputData->hSelf, pUvcInputData->pAppData,
                                                                        COMP_EventError, ERR_UVC_INCORRECT_STATE_TRANSITION, 0, NULL);                                
                            }
                            break;

                        default:
                            break;
                            
                        
                    }
                }
                pthread_mutex_unlock(&pUvcInputData->mStateLock);
            }
            else if(Flush == cmd)
            {
                
            }
            else if(UvcComp_InputFrameAvailable == cmd)
            {
                
            }
            else if(Stop == cmd)
            {
                goto EXIT;
            }

            goto PROCESS_MESSAGE;
        }

        int eError;
        if(COMP_StateExecuting == pUvcInputData->state)
        {
            if(TRUE == pUvcInputData->mOutputPortTunnelFlag)
            {
                COMP_INTERNAL_TUNNELINFOTYPE *pPortTunnelInfo = &pUvcInputData->sPortTunnelInfo[UVC_CHN_PORT_INDEX_DATA_OUT];
                MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE *)pPortTunnelInfo->hTunnel;
                if(pOutTunnelComp)
                {
                    COMP_BUFFERHEADERTYPE obh;
                    EncodedStream stEncodedFrame;
                    VIDEO_FRAME_INFO_S *pFrm = UvcGetReadyFrame(pUvcInputData);
                    if(!pFrm)
                    {
                        pUvcInputData->mWaitingCapDataFlag = TRUE;
                        TMessage_WaitQueueNotEmpty(&pUvcInputData->cmd_queue, 0);
                        goto PROCESS_MESSAGE;
                    }
                    obh.nOutputPortIndex = pPortTunnelInfo->nPortIndex;
                    obh.nInputPortIndex = pPortTunnelInfo->nTunnelPortIndex;
                    if(UVC_H264 == pUvcInputData->pUvcChnManager->mUvcAttr.mPixelformat
                        || UVC_MJPEG == pUvcInputData->pUvcChnManager->mUvcAttr.mPixelformat)
                    {
                        //h264, jpeg, etc.
                        memset(&stEncodedFrame, 0, sizeof(EncodedStream));
                        stEncodedFrame.media_type = CDX_PacketVideo;
                        stEncodedFrame.nID = pFrm->mId;
                        stEncodedFrame.nFilledLen = pFrm->VFrame.mStride[0];
                        stEncodedFrame.nTobeFillLen = pFrm->VFrame.mStride[0];
                        stEncodedFrame.nTimeStamp = pFrm->VFrame.mpts;
                        stEncodedFrame.nFlags = CEDARV_FLAG_PTS_VALID | CEDARV_FLAG_FIRST_PART | CEDARV_FLAG_LAST_PART;
                        stEncodedFrame.pBuffer = pFrm->VFrame.mpVirAddr[0];
                        stEncodedFrame.pBufferExtra = NULL;
                        stEncodedFrame.nBufferLen = pFrm->VFrame.mStride[0];
                        stEncodedFrame.nBufferExtraLen = 0;
                        stEncodedFrame.video_stream_type = VIDEO_TYPE_MAJOR;

                        obh.pOutputPortPrivate = (void*)&stEncodedFrame;
                    }
                    else
                    {
                        obh.pOutputPortPrivate = (void*)pFrm;
                    }
                    eError = pOutTunnelComp->EmptyThisBuffer(pOutTunnelComp, &obh);
                    if(eError != SUCCESS)
                    {
                        alogw("Loop UvcVirVi_ComponentThread OutTunnelComp EmptyThisBuffer failed %x, return this frame", eError);
                        uvcInput_RefsReduceAndRleaseData2(pUvcInputData->pUvcChnManager,pFrm);
                        UvcReleaseFrame(pUvcInputData, pFrm);
                    }                       
                }
                else
                {
                    TMessage_WaitQueueNotEmpty(&pUvcInputData->cmd_queue, 0);
                    goto PROCESS_MESSAGE;                    
                }
            }
            else
            {                
                TMessage_WaitQueueNotEmpty(&pUvcInputData->cmd_queue, 0);
            }
            
        }
        else
        {            
            TMessage_WaitQueueNotEmpty(&pUvcInputData->cmd_queue, 0);
        }       
    }
EXIT:
    alogv("UVC ComponentThread stopeed!");
    return (void *)SUCCESS;
}

