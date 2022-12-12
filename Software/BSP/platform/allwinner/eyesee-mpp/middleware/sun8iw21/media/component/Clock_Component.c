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
#define LOG_TAG "Clock_Component"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <sys/prctl.h>

#include <mm_comm_clock.h>
#include <mm_component.h>
#include <ComponentCommon.h>
#include <Clock_Component.h>
#include <tmessage.h>
#include <tsemaphore.h>
#include <cedarx_avs_counter.h>

#define MAX_CLOCK_PORTS  5

#define MAX_64BIT 0x7FFFFFFFFFFFFFFLL
#define MIN_64BIT -1
#define MAX_32BIT 0x7FFFFFFF
#define MIN_32BIT -1


static void* Clock_ComponentThread(void* pThreadData);

typedef struct CLOCKDATATYPE {
    COMP_STATETYPE state;
    COMP_CALLBACKTYPE *pCallbacks;
    void* pAppData;
    COMP_HANDLETYPE hSelf;
    COMP_PORT_PARAM_TYPE sPortParam;

    COMP_PARAM_PORTDEFINITIONTYPE sOutPortDef[MAX_CLOCK_PORTS];
    COMP_INTERNAL_TUNNELINFOTYPE sOutPortTunnelInfo[MAX_CLOCK_PORTS];
    COMP_PARAM_BUFFERSUPPLIERTYPE sPortBufSupplier[MAX_CLOCK_PORTS];
    CDX_NotifyStartToRunTYPE mNotifyStartToRunInfo[MAX_CLOCK_PORTS];

    COMP_TIME_CONFIG_CLOCKSTATETYPE      sClockState;
    COMP_TIME_CONFIG_ACTIVEREFCLOCKTYPE  sRefClock;
    int64_t                           WallTimeBase;   // record cedarx clock pts as cedarxClockTime base. start_to_play pts in cedarxClock. unit:us
    int64_t                           MediaTimeBase;  //media clock start pts, final stream start pts, == sMinStartTime.nTimestamp, select minimum start pts of streams as mediaTimeBase.
    COMP_TIME_CONFIG_TIMESTAMPTYPE       sMinStartTime;  //media clock, min(audio, video), select the smallest time as stream start time.
    int     nDstRatio;      //CedarX Play speed vps value. default is 0, but can be set to other value to support variable speed play. 40~-100. +:slow, -:speed up.

    CedarxAvscounterContext *avs_counter;
    pthread_t thread_id;
    message_queue_t  cmd_queue;
    cdx_sem_t cdx_sem_wait_message;

    unsigned int ports_connect_info; //store value of nWaitMask
    
    pthread_mutex_t clock_state;
    
    int64_t port_start_pts[MAX_CLOCK_PORTS];  //media clock, video/audio start pts, unit:us.
}CLOCKDATATYPE;

ERRORTYPE ClockGetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    unsigned int i;
    unsigned int portIdx = pPortDef->nPortIndex;
    for(i=0; i<pClockData->sPortParam.nPorts; i++)
    {
        if(portIdx == pClockData->sOutPortDef[i].nPortIndex)
        {
            memcpy(pPortDef, &pClockData->sOutPortDef[i], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
            break;
        }
    }
    if(i==pClockData->sPortParam.nPorts)
    {
        eError = ERR_CLOCK_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE ClockGetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_CLOCK_PORTS; i++)
    {
        if(pClockData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(pPortBufSupplier, &pClockData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_CLOCK_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE ClockGetClockState(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT COMP_TIME_CONFIG_CLOCKSTATETYPE *pClockState)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    memcpy(pClockState, &pClockData->sClockState, sizeof(COMP_TIME_CONFIG_CLOCKSTATETYPE));
    return eError;
}

ERRORTYPE ClockGetCurrentWallTime(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT COMP_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    pClockData->avs_counter->get_time(pClockData->avs_counter, &pTimeStamp->nTimestamp);
    //alogv("COMP_IndexConfigTimeCurrentWallTime =%x wallbase:%d\n", (int) timestamp->nTimestamp, (int)pClockData->WallTimeBase);
    return eError;
}

ERRORTYPE ClockGetCurrentMediaTime(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT COMP_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    if(pClockData->sClockState.nWaitMask)
    {
        //alogd("waitMask[0x%x] is still not null, no valid media time now.", pClockData->sClockState.nWaitMask);
        pTimeStamp->nTimestamp = -1;
    }
    else
    {
        pClockData->avs_counter->get_time(pClockData->avs_counter, &pTimeStamp->nTimestamp);
        //alogv("timestamp->nTimestamp 0.........%lld",timestamp->nTimestamp / 1000);
        pTimeStamp->nTimestamp -= pClockData->WallTimeBase;
        pTimeStamp->nTimestamp += pClockData->MediaTimeBase;
        //alogv("timestamp->nTimestamp 1.........%lld",timestamp->nTimestamp / 1000);
    }
    return eError;
}

ERRORTYPE ClockGetVps(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT int *pVps)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    *pVps = pClockData->nDstRatio;
    return eError;
}

ERRORTYPE ClockSetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    unsigned int i;
    for(i=0; i<pClockData->sPortParam.nPorts; i++)
    {
        if(pPortDef->nPortIndex == pClockData->sOutPortDef[i].nPortIndex)
        {
            memcpy(&pClockData->sOutPortDef[i], pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
            break;
        }
    }
    if(i==pClockData->sPortParam.nPorts)
    {
        eError = ERR_CLOCK_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE ClockSetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_CLOCK_PORTS; i++)
    {
        if(pClockData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(&pClockData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_CLOCK_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE ClockSeek(PARAM_IN COMP_HANDLETYPE hComponent)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    pClockData->sClockState.nWaitMask = pClockData->ports_connect_info;
    pClockData->sClockState.eState = COMP_TIME_ClockStateWaitingForStartTime;
    pClockData->sMinStartTime.nTimestamp = MAX_64BIT;
    {
        unsigned int i;
        for(i=0; i<MAX_CLOCK_PORTS; i++)
        {
            pClockData->port_start_pts[i] = MAX_64BIT;
        }
    }
    alogv("waitmask....%x", pClockData->sClockState.nWaitMask);
    return eError;
}

ERRORTYPE ClockSwitchAudio(PARAM_IN COMP_HANDLETYPE hComponent)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    alogd("need start to play again after switch audio!");
    pClockData->sClockState.nWaitMask = pClockData->ports_connect_info;
    pClockData->sClockState.eState = COMP_TIME_ClockStateWaitingForStartTime;
    pClockData->sMinStartTime.nTimestamp = MAX_64BIT;
    unsigned int i;
    for(i=0; i<MAX_CLOCK_PORTS; i++)
    {
        pClockData->port_start_pts[i] = MAX_64BIT;
    }
    alogv("waitmask....%x", pClockData->sClockState.nWaitMask);
    return eError;
}

ERRORTYPE ClockAdjustClock(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int64_t mediatime;
    int64_t mediaTimediff;  //unit:us
    pClockData->avs_counter->get_time(pClockData->avs_counter, &mediatime);
    mediatime -= pClockData->WallTimeBase;
    mediatime += pClockData->MediaTimeBase;
    mediaTimediff = pTimeStamp->nTimestamp - mediatime;
    int adjust_ratio = 0;
    if(mediaTimediff > 50*1000 || mediaTimediff < -50*1000)
    {
        int     tmpRatio = 0;
        int     precise_adjust_ratio = 0;
        //here I want to give detailed description about time:
        //ao component uses audio frame pts in media file to determine AVS_ADJUST_PERIOD, and system plays audio frames according to system clock.
        //so we can consider AVS_ADJUST_PERIOD as system clock time.
        //when add variable play function, AVS_ADJUST_PERIOD must be considered as variable speed time clock duration.
        //Rule of adjusting cedarx clock is that at next adjust point, cedarx clock's diff must match current diff to make cedarx clock's error to audio frame pts in mediaFile is zero.
        //So formula is: (1) (100 - dstRatio - ratio)/100*systemTimeDuration = (100 - dstRatio)/100*systemTimeDuration + mediaDiff;
        //               (2) (100 - dstRatio)/100*systemTimeDuration = AVS_ADJUST_PERIOD
        // adjust_ratio = dstRatio + ratio;
        //adjust_ratio = -mediaTimediff / (AVS_ADJUST_PERIOD/100);
        precise_adjust_ratio = pClockData->nDstRatio - (100-pClockData->nDstRatio)*mediaTimediff/AVS_ADJUST_PERIOD;
        tmpRatio = precise_adjust_ratio - pClockData->nDstRatio;
        tmpRatio = tmpRatio > 5 ? 5 : tmpRatio;
        tmpRatio = tmpRatio < -5 ? -5 : tmpRatio;
        adjust_ratio = tmpRatio + pClockData->nDstRatio;
        alogd("----adjust ratio:%d, precise_adjust_ratio:%d, ref:%lld media:%lld diff:%lld diff-percent:%lld ----",
                adjust_ratio, precise_adjust_ratio, pTimeStamp->nTimestamp, mediatime, mediaTimediff,
                mediaTimediff / (AVS_ADJUST_PERIOD/100));
    }
    else
    {
        alogv("----adjust ratio do nothing:ref0:%lld media:%lld diff:%d diff-percent:%d ----", pTimeStamp->nTimestamp, mediatime, mediaTimediff,mediaTimediff / (AVS_ADJUST_PERIOD/100));
    }
    pClockData->avs_counter->adjust(pClockData->avs_counter, adjust_ratio);
    return eError;
}

ERRORTYPE ClockSetVps(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN int adjust_ratio)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    if(pClockData->nDstRatio == adjust_ratio)
    {
        alogv("clock component:same vps value[%d], return", adjust_ratio);
        return eError;
    }
    alogd("ClockComponent will set new vpsspeed[%d], old is [%d]", adjust_ratio, pClockData->nDstRatio);
    pClockData->nDstRatio = adjust_ratio;
    pClockData->avs_counter->adjust(pClockData->avs_counter, pClockData->nDstRatio);
    return eError;
}

ERRORTYPE ClockSetClockState(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_TIME_CONFIG_CLOCKSTATETYPE *pClockState)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    switch (pClockState->eState)
    {
    case COMP_TIME_ClockStateRunning:
        //alogv( "in  %s ...set to COMP_TIME_ClockStateRunning\n", __func__);
        memcpy(&pClockData->sClockState, pClockState, sizeof(COMP_TIME_CONFIG_CLOCKSTATETYPE));
        break;

    case COMP_TIME_ClockStateWaitingForStartTime:
        if (pClockData->sClockState.eState == COMP_TIME_ClockStateRunning)
        {
            eError = ERR_CLOCK_INCORRECT_STATE_TRANSITION;
            break;
        }
        else if (pClockData->sClockState.eState == COMP_TIME_ClockStateWaitingForStartTime)
        {
            eError = ERR_CLOCK_SAMESTATE;
            break;
        }

        //alogv("...set to COMP_TIME_ClockStateWaitingForStartTime  mask sent=%d", (int) clockstate->nWaitMask);
        pClockData->ports_connect_info = pClockState->nWaitMask;
        memcpy(&pClockData->sClockState, pClockState, sizeof(COMP_TIME_CONFIG_CLOCKSTATETYPE));
        break;

    case COMP_TIME_ClockStateStopped:
        //alogv(" in  %s ...set to COMP_TIME_ClockStateStopped\n", __func__);
        memcpy(&pClockData->sClockState, pClockState, sizeof(COMP_TIME_CONFIG_CLOCKSTATETYPE));
        break;

    default:
        aloge("fatal error! unknown clock state[0x%x]", pClockState->eState);
        break;
    }

    return eError;
}

ERRORTYPE ClockSetClientStartTime(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_TIME_CONFIG_TIMESTAMPTYPE *pRefTimeStamp)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    unsigned int PortSuffix;
    unsigned int nMask;
    int64_t walltime;
    for(PortSuffix=0; PortSuffix<pClockData->sPortParam.nPorts; PortSuffix++)
    {
        if(pRefTimeStamp->nPortIndex == pClockData->sOutPortDef[PortSuffix].nPortIndex)
        {
            break;
        }
    }
    if(PortSuffix >= pClockData->sPortParam.nPorts)
    {
        aloge("fatal error! not find PortIndex[%d]!", pRefTimeStamp->nPortIndex);
        return ERR_CLOCK_ILLEGAL_PARAM;
    }
    COMP_PARAM_PORTDEFINITIONTYPE *pPort = &pClockData->sOutPortDef[PortSuffix];

    pthread_mutex_lock(&pClockData->clock_state);
    /* update the nWaitMask to clear the flag for the client which has sent its start time */
    if (pClockData->sClockState.nWaitMask)
    {
        alogv( "$$$$$$$$ refTime set is = [%lld]ms nWaitMask1: 0x%x", pRefTimeStamp->nTimestamp/1000, pClockData->sClockState.nWaitMask);
        nMask = ~(0x1 << pRefTimeStamp->nPortIndex);
        pClockData->sClockState.nWaitMask = pClockData->sClockState.nWaitMask & nMask;

        alogv( "PortIndex: %d nWaitMask2: 0x%x", pRefTimeStamp->nPortIndex, pClockData->sClockState.nWaitMask);
        pClockData->port_start_pts[PortSuffix] = pRefTimeStamp->nTimestamp;
    }

    if (!pClockData->sClockState.nWaitMask && pClockData->sClockState.eState == COMP_TIME_ClockStateWaitingForStartTime)
    {
        //start to set base player timer
        pClockData->sMinStartTime.nTimestamp = MAX_64BIT;
        unsigned int i;
        for(i = 0; i < MAX_CLOCK_PORTS; i++)
        {
            if(pClockData->port_start_pts[i] < pClockData->sMinStartTime.nTimestamp)
            {
                pClockData->sMinStartTime.nTimestamp = pClockData->port_start_pts[i];
                pClockData->sMinStartTime.nPortIndex = pClockData->sOutPortDef[i].nPortIndex;
            }
        }

        pClockData->sClockState.eState = COMP_TIME_ClockStateRunning;
        pClockData->sClockState.nStartTime = pClockData->sMinStartTime.nTimestamp;
        pClockData->MediaTimeBase = pClockData->sMinStartTime.nTimestamp;

        pClockData->avs_counter->get_time(pClockData->avs_counter, &walltime);
        pClockData->WallTimeBase = walltime;

        for(i = 0; i < MAX_CLOCK_PORTS; i++)
        {
            if(pClockData->mNotifyStartToRunInfo[i].mbNotify)
            {
                COMP_SetConfig(
                    pClockData->sOutPortTunnelInfo[i].hTunnel,
                    COMP_IndexVendorNotifyStartToRun,
                    NULL);
            }
        }

        alogd("notifyStartToRun, Mediatimebase=[%lld]ms walltimebase=[%lld]ms", pClockData->MediaTimeBase/1000, pClockData->WallTimeBase/1000);
    }
    pthread_mutex_unlock(&pClockData->clock_state);
    return eError;
}

ERRORTYPE ClockSetClientForceStartTime(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_TIME_CONFIG_TIMESTAMPTYPE *pRefTimeStamp)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    unsigned int PortSuffix;
    int64_t walltime;
    for(PortSuffix=0; PortSuffix<pClockData->sPortParam.nPorts; PortSuffix++)
    {
        if(pRefTimeStamp->nPortIndex == pClockData->sOutPortDef[PortSuffix].nPortIndex)
        {
            break;
        }
    }
    if(PortSuffix >= pClockData->sPortParam.nPorts)
    {
        aloge("fatal error! not find PortIndex[%d]!", pRefTimeStamp->nPortIndex);
        return ERR_CLOCK_ILLEGAL_PARAM;
    }
    COMP_PARAM_PORTDEFINITIONTYPE *pPort = &pClockData->sOutPortDef[PortSuffix];

    pthread_mutex_lock(&pClockData->clock_state);
    /* update the nWaitMask to clear the flag for the client which has sent its start time */
    if(pClockData->sClockState.nWaitMask && pClockData->sClockState.eState == COMP_TIME_ClockStateWaitingForStartTime)
    {
        pClockData->sClockState.nWaitMask = 0;
    }

    if (!pClockData->sClockState.nWaitMask && pClockData->sClockState.eState == COMP_TIME_ClockStateWaitingForStartTime)
    {
        //start to set base player timer
        pClockData->sMinStartTime.nTimestamp = pRefTimeStamp->nTimestamp;
        pClockData->sMinStartTime.nPortIndex = pRefTimeStamp->nPortIndex;
        pClockData->sClockState.eState       = COMP_TIME_ClockStateRunning;
        pClockData->sClockState.nStartTime   = pClockData->sMinStartTime.nTimestamp;
        pClockData->MediaTimeBase            = pClockData->sMinStartTime.nTimestamp;

        pClockData->avs_counter->get_time(pClockData->avs_counter, &walltime);
        pClockData->WallTimeBase = walltime;
        unsigned int i;
        for(i = 0; i < MAX_CLOCK_PORTS; i++)
        {
            if(pClockData->mNotifyStartToRunInfo[i].mbNotify)
            {
                COMP_SetConfig(
                    pClockData->sOutPortTunnelInfo[i].hTunnel,
                    COMP_IndexVendorNotifyStartToRun,
                    NULL);
            }
        }

        alogd("forceStart Mediatimebase=%lld walltimebase=%lld", pClockData->MediaTimeBase/1000, pClockData->WallTimeBase/1000);
    }
    pthread_mutex_unlock(&pClockData->clock_state);
    return eError;
}

ERRORTYPE ClockSetActiveRefClock(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_TIME_CONFIG_ACTIVEREFCLOCKTYPE *pRefClock)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    memcpy(&pClockData->sRefClock, pRefClock, sizeof(COMP_TIME_CONFIG_ACTIVEREFCLOCKTYPE));
    return eError;
}

ERRORTYPE ClockSetWallTimeBase(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN int64_t mediaTime)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int64_t walltime;
    pClockData->avs_counter->get_time(pClockData->avs_counter, &walltime);
    pClockData->WallTimeBase = walltime + pClockData->MediaTimeBase - mediaTime;
    return eError;
}

ERRORTYPE ClockSetNotifyStartToRunInfo(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN CDX_NotifyStartToRunTYPE *pNotifyStartToRunInfo)
{
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    unsigned int i;
    for(i=0; i<pClockData->sPortParam.nPorts; i++)
    {
        if(pNotifyStartToRunInfo->nPortIndex == pClockData->mNotifyStartToRunInfo[i].nPortIndex)
        {
            break;
        }
    }
    if(i < pClockData->sPortParam.nPorts)
    {
        memcpy(&pClockData->mNotifyStartToRunInfo[i], pNotifyStartToRunInfo, sizeof(CDX_NotifyStartToRunTYPE));
    }
    else
    {
        eError = ERR_CLOCK_ILLEGAL_PARAM;
    }
    return eError;
}

/*****************************************************************************/
ERRORTYPE ClockSendCommand(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_COMMANDTYPE Cmd, PARAM_IN unsigned int nParam1, PARAM_IN void* pCmdData)
{
    CLOCKDATATYPE *pClockData;
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;

    pClockData = (CLOCKDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if (pClockData->state == COMP_StateInvalid)
    {
        eError = ERR_CLOCK_INVALIDSTATE;
        goto OMX_CONF_CMD_BAIL;
    }

    switch (Cmd)
    {
    case COMP_CommandStateSet:
        eCmd = SetState;
        break;

    case COMP_CommandFlush:
        eCmd = Flush;
        break;

    case COMP_CommandPortDisable:
        eCmd = StopPort;
        break;

    case COMP_CommandPortEnable:
        eCmd = RestartPort;
        break;

    default:
        eCmd = -1;
        break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pClockData->cmd_queue, &msg);
    cdx_sem_up(&pClockData->cdx_sem_wait_message);

    OMX_CONF_CMD_BAIL: return eError;
}

/*****************************************************************************/
ERRORTYPE ClockGetState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT COMP_STATETYPE* pState)
{
    CLOCKDATATYPE *pClockData;
    ERRORTYPE eError = SUCCESS;

    pClockData = (CLOCKDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    *pState = pClockData->state;

    OMX_CONF_CMD_BAIL: return eError;
}

/*****************************************************************************/
ERRORTYPE ClockSetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_CALLBACKTYPE* pCallbacks, PARAM_IN void* pAppData)
{
    CLOCKDATATYPE *pClockData;
    ERRORTYPE eError = SUCCESS;

    pClockData = (CLOCKDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pClockData->pCallbacks = pCallbacks;
    pClockData->pAppData = pAppData;

    OMX_CONF_CMD_BAIL: return eError;
}

ERRORTYPE ClockGetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex, PARAM_INOUT void* pComponentConfigStructure)
{
    CLOCKDATATYPE *pClockData;
    COMP_TIME_CONFIG_CLOCKSTATETYPE     *clockstate;
    COMP_TIME_CONFIG_TIMESTAMPTYPE      *timestamp;
    ERRORTYPE eError = SUCCESS;

    pClockData = (CLOCKDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    switch (nIndex)
    {
    case COMP_IndexParamPortDefinition:
    {
        eError = ClockGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexParamCompBufferSupplier:
    {
        eError = ClockGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexConfigTimeClockState:
    {
        eError = ClockGetClockState(hComponent, (COMP_TIME_CONFIG_CLOCKSTATETYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexConfigTimeCurrentWallTime:
    {
        eError = ClockGetCurrentWallTime(hComponent, (COMP_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexConfigTimeCurrentMediaTime:
    {
        eError = ClockGetCurrentMediaTime(hComponent, (COMP_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexVendorVps:
    {
        eError = ClockGetVps(hComponent, (int*)pComponentConfigStructure);
        break;
    }
    default:
        eError = ERR_CLOCK_ILLEGAL_PARAM;
        break;
    }

    return eError;
}

/*******************************************************************************
Function name: ClockSetConfig
Description: 
    1. vps calculate formula:
        adjust_ratio = nDstRatio - (100-nDstRatio)*mediaTimediff/AVS_ADJUST_PERIOD;
        
        +: slow
        -: fast
        AVS_ADJUST_PERIOD is base on nDstRatio Time System!
        In AVS_ADJUST_PERIOD(base on nDstRatio Time System) time, video clock will equal to nDstRatio Time System.
        
    2. We don't consider wifi display's case of VPS. So If in wifidisplay mode, use vps will lead to avsync error.
       If need vps in wifiDisplay mode, we will modify in the future.
Parameters: 
    
Return: 
    
Time: 2013/1/11
*******************************************************************************/
ERRORTYPE ClockSetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex, PARAM_IN void* pComponentConfigStructure)
{
    CLOCKDATATYPE *pClockData;
    ERRORTYPE eError = SUCCESS;
    COMP_TIME_CONFIG_CLOCKSTATETYPE*     clockstate;
    COMP_TIME_CONFIG_TIMESTAMPTYPE*      sRefTimeStamp;
    unsigned int                             PortIndex;
    unsigned int                             nMask;
    int64_t                           walltime, mediatime, reftime;
    int                             mediaTimediff;
    int                             adjust_ratio = 0;
    COMP_PARAM_PORTDEFINITIONTYPE        *pPort;
    pClockData = (CLOCKDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    adjust_ratio = pClockData->nDstRatio;

    switch (nIndex)
    {
    case COMP_IndexParamPortDefinition:
    {
        eError = ClockSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexParamCompBufferSupplier:
    {
        eError = ClockSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexVendorSeekToPosition:
    {
        eError = ClockSeek(hComponent);
        break;
    }
    case COMP_IndexVendorSwitchAudio:
    {
        eError = ClockSwitchAudio(hComponent);
        break;
    }
    case COMP_IndexVendorAdjustClock:
    {
        eError = ClockAdjustClock(hComponent, (COMP_TIME_CONFIG_TIMESTAMPTYPE*) pComponentConfigStructure);
        break;
    }
    case COMP_IndexVendorVps:
    {
        eError = ClockSetVps(hComponent, adjust_ratio);
        break;
    }
    case COMP_IndexConfigTimeClockState:
    {
        eError = ClockSetClockState(hComponent, (COMP_TIME_CONFIG_CLOCKSTATETYPE*)pComponentConfigStructure);
        break;
    }

    case COMP_IndexConfigTimeClientStartTime:
    {
        eError = ClockSetClientStartTime(hComponent, (COMP_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexVendorConfigTimeClientForceStart:
    {
        eError = ClockSetClientForceStartTime(hComponent, (COMP_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexConfigTimeActiveRefClock:
    {
        eError = ClockSetActiveRefClock(hComponent, (COMP_TIME_CONFIG_ACTIVEREFCLOCKTYPE*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexConfigWallTimeBase: //for rtsp reconfiguration
    {
        eError = ClockSetWallTimeBase(hComponent, *(int64_t*)pComponentConfigStructure);
        break;
    }
    case COMP_IndexVendorNotifyStartToRunInfo:
    {
        eError = ClockSetNotifyStartToRunInfo(hComponent, (CDX_NotifyStartToRunTYPE*)pComponentConfigStructure);
        break;
    }
    default:
        eError = ERR_CLOCK_ILLEGAL_PARAM;
        break;
    }

    return eError;
}

ERRORTYPE ClockComponentTunnelRequest(
    PARAM_IN  COMP_HANDLETYPE hComponent,
    PARAM_IN  unsigned int nPort,
    PARAM_IN  COMP_HANDLETYPE hTunneledComp,
    PARAM_IN  unsigned int nTunneledPort,
    PARAM_INOUT  COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pClockData->state == COMP_StateExecuting)
    {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    }
    else if(pClockData->state != COMP_StateIdle)
    {
        aloge("fatal error! tunnel request can't be in state[0x%x]", pClockData->state);
        eError = ERR_CLOCK_INCORRECT_STATE_OPERATION;
        goto OMX_CMD_FAIL;
    }
    COMP_PARAM_PORTDEFINITIONTYPE    *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE              *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE    *pPortBufSupplier;
    BOOL bFindFlag;
    int i;
    bFindFlag = FALSE;
    for(i=0; i<MAX_CLOCK_PORTS; i++)
    {
        if(pClockData->sOutPortDef[i].nPortIndex == nPort)
        {
            pPortDef = &pClockData->sOutPortDef[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_CLOCK_ILLEGAL_PARAM;
        goto OMX_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for(i=0; i<MAX_CLOCK_PORTS; i++)
    {
        if(pClockData->sOutPortTunnelInfo[i].nPortIndex == nPort)
        {
            pPortTunnelInfo = &pClockData->sOutPortTunnelInfo[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_CLOCK_ILLEGAL_PARAM;
        goto OMX_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for(i=0; i<MAX_CLOCK_PORTS; i++)
    {
        if(pClockData->sPortBufSupplier[i].nPortIndex == nPort)
        {
            pPortBufSupplier = &pClockData->sPortBufSupplier[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_CLOCK_ILLEGAL_PARAM;
        goto OMX_CMD_FAIL;
    }
    
    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if(NULL==hTunneledComp && 0==nTunneledPort && NULL==pTunnelSetup)
    {
        alogd("omx_core cancel setup tunnel on port[%d]", nPort);
        eError = SUCCESS;
        goto OMX_CMD_FAIL;
    }
    if(pPortDef->eDir == COMP_DirOutput)
    {
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
    }
    else
    {
        //Check the data compatibility between the ports using one or more GetParameter calls.
        //B checks if its input port is compatible with the output port of component A.
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        COMP_GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if(out_port_def.eDir != COMP_DirOutput)
        {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_CLOCK_ILLEGAL_PARAM;
            goto OMX_CMD_FAIL;
        }
        pPortDef->format = out_port_def.format;
    
        //The component B informs component A about the final result of negotiation.
        if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier)
        {
            alogw("Low probability! use input portIndex[%d] buffer supplier[%d] as final!", nPort, pPortBufSupplier->eBufferSupplier);
            pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        }
        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        COMP_GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        COMP_SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
    }

OMX_CMD_FAIL:
    return eError;
}

ERRORTYPE ClockComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    CLOCKDATATYPE *pClockData;
    ERRORTYPE eError = SUCCESS;
    CompInternalMsgType eCmd = Stop;
    message_t msg;

    pClockData = (CLOCKDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    msg.command = eCmd;
    put_message(&pClockData->cmd_queue, &msg);
    cdx_sem_up(&pClockData->cdx_sem_wait_message);

    alogv("wait clock component exit!...");

    // Wait for thread to exit so we can get the status into "error"
    pthread_join(pClockData->thread_id, (void*) &eError);

    alogv("clock component exited 0!");

    message_destroy(&pClockData->cmd_queue);
    cdx_sem_deinit(&pClockData->cdx_sem_wait_message);

    pthread_mutex_destroy(&pClockData->clock_state);

    if(pClockData->avs_counter != NULL) {
        cedarx_avs_counter_release(pClockData->avs_counter);
    }

    if(pClockData)
    {
        free(pClockData);
    }

    alogv("clock component exited 1!");

    return eError;
}

/*****************************************************************************/
ERRORTYPE ClockComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE *pComp;
    CLOCKDATATYPE *pClockData;
    ERRORTYPE eError = SUCCESS;
    unsigned int err;
    unsigned int i;

    pComp = (MM_COMPONENTTYPE *) hComponent;   

    // Create private data
    pClockData = (CLOCKDATATYPE *) malloc(sizeof(CLOCKDATATYPE));
    memset(pClockData, 0x0, sizeof(CLOCKDATATYPE));

    pComp->pComponentPrivate = (void*) pClockData;
    pClockData->state = COMP_StateLoaded;
    pClockData->hSelf = hComponent;

    // Fill in function pointers
    pComp->SetCallbacks = ClockSetCallbacks;
    pComp->SendCommand = ClockSendCommand;
    pComp->GetConfig = ClockGetConfig;
    pComp->SetConfig = ClockSetConfig;
    pComp->GetState = ClockGetState;
    pComp->ComponentTunnelRequest = ClockComponentTunnelRequest;
    pComp->ComponentDeInit = ClockComponentDeInit;

    // Initialize component data structures to default values
    pClockData->sPortParam.nPorts = 0;
    pClockData->sPortParam.nStartPortNumber = 0x0;

    // Initialize the video parameters for output port
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_AUDIO;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].bEnabled = FALSE;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDomain = COMP_PortDomainOther;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDir = COMP_DirOutput;
    pClockData->mNotifyStartToRunInfo[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_AUDIO;
    pClockData->sPortParam.nPorts++;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_VIDEO;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].bEnabled = FALSE;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDomain = COMP_PortDomainOther;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDir = COMP_DirOutput;
    pClockData->mNotifyStartToRunInfo[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_VIDEO;
    pClockData->sPortParam.nPorts++;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_DEMUX;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].bEnabled = FALSE;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDomain = COMP_PortDomainOther;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDir = COMP_DirOutput;
    pClockData->mNotifyStartToRunInfo[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_DEMUX;
    pClockData->sPortParam.nPorts++;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_VDEC;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].bEnabled = FALSE;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDomain = COMP_PortDomainOther;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDir = COMP_DirOutput;
    pClockData->mNotifyStartToRunInfo[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_VDEC;
    pClockData->sPortParam.nPorts++;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_SUBTITLE;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].bEnabled = FALSE;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDomain = COMP_PortDomainOther;
    pClockData->sOutPortDef[pClockData->sPortParam.nPorts].eDir = COMP_DirOutput;
    pClockData->mNotifyStartToRunInfo[pClockData->sPortParam.nPorts].nPortIndex = CLOCK_PORT_INDEX_SUBTITLE;
    pClockData->sPortParam.nPorts++;

    pClockData->sPortBufSupplier[0].nPortIndex = CLOCK_PORT_INDEX_AUDIO;
    pClockData->sPortBufSupplier[0].eBufferSupplier = COMP_BufferSupplyOutput;
    pClockData->sPortBufSupplier[1].nPortIndex = CLOCK_PORT_INDEX_VIDEO;
    pClockData->sPortBufSupplier[1].eBufferSupplier = COMP_BufferSupplyOutput;
    pClockData->sPortBufSupplier[2].nPortIndex = CLOCK_PORT_INDEX_DEMUX;
    pClockData->sPortBufSupplier[2].eBufferSupplier = COMP_BufferSupplyOutput;
    pClockData->sPortBufSupplier[3].nPortIndex = CLOCK_PORT_INDEX_VDEC;
    pClockData->sPortBufSupplier[3].eBufferSupplier = COMP_BufferSupplyOutput;
    pClockData->sPortBufSupplier[4].nPortIndex = CLOCK_PORT_INDEX_SUBTITLE;
    pClockData->sPortBufSupplier[4].eBufferSupplier = COMP_BufferSupplyOutput;

    pClockData->sOutPortTunnelInfo[0].nPortIndex = CLOCK_PORT_INDEX_AUDIO;
    pClockData->sOutPortTunnelInfo[0].eTunnelType = TUNNEL_TYPE_CLOCK;
    pClockData->sOutPortTunnelInfo[1].nPortIndex = CLOCK_PORT_INDEX_VIDEO;
    pClockData->sOutPortTunnelInfo[1].eTunnelType = TUNNEL_TYPE_CLOCK;
    pClockData->sOutPortTunnelInfo[2].nPortIndex = CLOCK_PORT_INDEX_DEMUX;
    pClockData->sOutPortTunnelInfo[2].eTunnelType = TUNNEL_TYPE_CLOCK;
    pClockData->sOutPortTunnelInfo[3].nPortIndex = CLOCK_PORT_INDEX_VDEC;
    pClockData->sOutPortTunnelInfo[3].eTunnelType = TUNNEL_TYPE_CLOCK;
    pClockData->sOutPortTunnelInfo[4].nPortIndex = CLOCK_PORT_INDEX_SUBTITLE;
    pClockData->sOutPortTunnelInfo[4].eTunnelType = TUNNEL_TYPE_CLOCK;

    cdx_sem_init(&pClockData->cdx_sem_wait_message,0);
    if(message_create(&pClockData->cmd_queue)<0)
    {
        aloge("message error!");
        eError = ERR_CLOCK_SYS_NOTREADY;
        goto EXIT;
    }

    pClockData->sClockState.eState = COMP_TIME_ClockStateStopped;
    pClockData->sMinStartTime.nTimestamp = MAX_64BIT;
    for(i=0; i<MAX_CLOCK_PORTS; i++)
    {
        pClockData->port_start_pts[i] = MAX_64BIT;
    }

    pClockData->avs_counter = cedarx_avs_counter_request();

    //* create a mutex for clock state.
    pthread_mutex_init(&pClockData->clock_state, NULL);

    // Create the component thread
    err = pthread_create(&pClockData->thread_id, NULL, Clock_ComponentThread, pClockData);
    if (err || !pClockData->thread_id)
    {
        eError = ERR_CLOCK_SYS_NOTREADY;
        goto EXIT;
    }
    EXIT: return eError;
}

/*****************************************************************************/
static void* Clock_ComponentThread(void* pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    CLOCKDATATYPE *pClockData = (CLOCKDATATYPE*) pThreadData;
    message_t cmd_msg;

    alogv("clock ComponentThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"CDX_Clock", 0, 0, 0);


    while (1)
    {
        if(get_message(&pClockData->cmd_queue, &cmd_msg) == 0)
        {
            cmd = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;

            alogv("Clock ComponentThread get_message cmd:%d",cmd);

            // State transition command
            if (cmd == SetState)
            {
                // If the parameter states a transition to the same state
                //   raise a same state transition error.
                if (pClockData->state == (COMP_STATETYPE) (cmddata))
                    pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventError, ERR_CLOCK_SAMESTATE, 0, NULL);
                else
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE) (cmddata))
                    {
                    case COMP_StateInvalid:
                        pClockData->state = COMP_StateInvalid;
                        pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventError, ERR_CLOCK_INVALIDSTATE, 0, NULL);
                        pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventCmdComplete, COMP_CommandStateSet, pClockData->state, NULL);
                        break;

                    case COMP_StateLoaded:
                        if (pClockData->state != COMP_StateIdle)
                        {
                            aloge("fatal error! ClockComp incorrect state transition [0x%x]->Loaded!", pClockData->state);
                            pClockData->pCallbacks->EventHandler(
                                    pClockData->hSelf, 
                                    pClockData->pAppData,
                                    COMP_EventError,
                                    ERR_CLOCK_INCORRECT_STATE_TRANSITION, 
                                    0, 
                                    NULL);
                        }
                        pClockData->state = COMP_StateLoaded;
                        pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventCmdComplete, COMP_CommandStateSet, pClockData->state, NULL);
                        break;

                    case COMP_StateIdle:
                        pClockData->avs_counter->reset(pClockData->avs_counter);
                        if (pClockData->state == COMP_StateInvalid)
                            pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventError, ERR_CLOCK_INCORRECT_STATE_OPERATION, 0, NULL);
                        else
                        {
                            alogd("ClockComp state[0x%x]->Idle!", pClockData->state);
                            pClockData->state = COMP_StateIdle;
                            pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventCmdComplete, COMP_CommandStateSet, pClockData->state, NULL);
                        }

                        break;

                    case COMP_StateExecuting:
                        // Transition can only happen from pause or idle state
                        if (pClockData->state == COMP_StateIdle || pClockData->state == COMP_StatePause)
                        {
                            if(COMP_StateIdle == pClockData->state)
                            {
                                pClockData->avs_counter->adjust(pClockData->avs_counter, pClockData->nDstRatio);
                            }
                            pClockData->avs_counter->start(pClockData->avs_counter);
                            pClockData->state = COMP_StateExecuting;
                            pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventCmdComplete, COMP_CommandStateSet, pClockData->state, NULL);
                        }
                        else
                            pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventError, ERR_CLOCK_INCORRECT_STATE_OPERATION, 0, NULL);

                        break;

                    case COMP_StatePause:
                        // Transition can only happen from idle or executing state
                        if(pClockData->state == COMP_StateIdle || pClockData->state == COMP_StateExecuting)
                        {
                            pClockData->avs_counter->pause(pClockData->avs_counter);
                            pClockData->state = COMP_StatePause;
                            //pClockData->sClockState.nWaitMask = pClockData->ports_connect_info; //set nWaitMask to invalid current positon
                            pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventCmdComplete, COMP_CommandStateSet, pClockData->state, NULL);
                        }
                        else
                            pClockData->pCallbacks->EventHandler(pClockData->hSelf, pClockData->pAppData, COMP_EventError, ERR_CLOCK_INCORRECT_STATE_TRANSITION, 0, NULL);

                        break;

                    default:
                        break;
                    }
                }
            }           
            else if (cmd == StopPort)
            {

            } 
            else if (cmd == Stop)
            {
                // Kill thread
                goto EXIT;
            }
        }

        cdx_sem_down(&pClockData->cdx_sem_wait_message);
    }
EXIT:
    alogv("clock ComponentThread stopped");
    return (void*) SUCCESS;
}

