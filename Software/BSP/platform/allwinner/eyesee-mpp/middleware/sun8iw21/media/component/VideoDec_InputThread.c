/******************************************************************************
  Copyright (C), 2001-2018, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VideoDec_InputThread.c
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2018/12/06
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "VideoDec_InputThread"
#include <utils/plat_log.h>

#include <errno.h>
//#include <ion/ion.h>
#include <stdio.h>
#include <sys/prctl.h>
//#include <sys/ioctl.h>

#include <mm_component.h>
#include <tmessage.h>
#include <tsemaphore.h>

#include <CDX_MediaFormat.h>
#include <cedarx_demux.h>
#include <memoryAdapter.h>
#include <vdecoder.h>
#include <cdc_config.h>
#include <iniparserapi.h>
//#include <CDX_FormatConvert.h>
#include <EncodedStream.h>
#include <DemuxCompStream.h>
#include <SystemBase.h>
#include <VdecStream.h>
#include "VideoDec_Component.h"
#include <mpi_videoformat_conversion.h>
#include <EPIXELFORMAT_g2d_format_convert.h>
#include <ion_memmanager.h>
#include <linux/g2d_driver.h>
#include "VideoDec_DataType.h"
#include "VideoDec_InputThread.h"
//#include <ConfigOption.h>
#include <cdx_list.h>

#define INPUT_BUF_COUNT (1) //when input buffer is supplied by input port of mine.
//#define INPUT_BUF_COUNT_For_BufferSupplyOutput (30) //when input buffer is supplied by output port of tunnel component.

static void* InputThread(void* pThreadData) 
{
    ERRORTYPE eError = SUCCESS;
    COMP_BUFFERHEADERTYPE omx_buffer_header;
    memset(&omx_buffer_header, 0, sizeof(COMP_BUFFERHEADERTYPE));
    EncodedStream dmxOutBuf;
    DMXPKT_NODE_T *pEntry, *pTmp;

    int                 cmddata;
    CompInternalMsgType cmd;
    message_t           cmd_msg;

    int bsValidSize, bsTotalSize, bsEmptySize;

    // Get component private data
    VIDEODEC_INPUT_DATA* pInputData = (VIDEODEC_INPUT_DATA*)pThreadData;
    VIDEODECDATATYPE* pVideoDecData = (VIDEODECDATATYPE*)pInputData->pVideoDecData;

    aloge("VideoDecoder InputThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"VDecInput", 0, 0, 0);
    COMP_PARAM_PORTDEFINITIONTYPE *pPortDef            = &pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX];
    COMP_INTERNAL_TUNNELINFOTYPE  *pPortTunnelInfo     = &pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_DEMUX];
    COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufferSupplier = &pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_DEMUX];
    while (1) 
    {
PROCESS_MESSAGE:
        if (get_message(&pInputData->cmd_queue, &cmd_msg) == 0) 
        {// pump message from message queue

            alogv("input thread got message: cmd=%d cmddata=%d (2-Idle 3-Execute 4-Pause)", cmd, cmddata);
            cmd     = cmd_msg.command;
            cmddata = cmd_msg.para0;

            // State transition command
            if (cmd == SetState)
            {
                pthread_mutex_lock(&pInputData->mStateLock);
                switch ((COMP_STATETYPE) (cmddata))
                {
                    case COMP_StateLoaded:
                    {
                        if(pInputData->state!=COMP_StateIdle)
                        {
                            aloge("fatal error! inputThread state[0x%x] != Idle", pInputData->state);
                        }
                        if(pVideoDecData->mInputPortTunnelFlag)
                        {
                            if(pPortBufferSupplier->eBufferSupplier == COMP_BufferSupplyInput)
                            {
                                pthread_mutex_lock(&pInputData->mVbsListLock);
                                struct list_head *pList;
                                pInputData->mbWaitVbsFullFlag = TRUE;
                                while (1) 
                                {
                                    int cnt = 0;
                                    list_for_each(pList, &pInputData->mIdleVbsList) { cnt++; }
                                    if (cnt < pInputData->mNodeNum) 
                                    {
                                        alogd("wait input thread [%d]nodes to idleList!", pInputData->mNodeNum - cnt);
                                        pthread_cond_wait(&pInputData->mVbsFullCond, &pInputData->mVbsListLock);
                                    } 
                                    else 
                                    {
                                        break;
                                    }
                                }
                                pInputData->mbWaitVbsFullFlag = FALSE;
                                pthread_mutex_unlock(&pInputData->mVbsListLock);
                            }
                            else
                            {
                                alogd("tunnel_BufferSupplyOutput is not processed here.");
                            }
                        }
                        pInputData->state = COMP_StateLoaded;
                        pthread_cond_signal(&pInputData->mStateCond);
                        break;
                    }
                    case COMP_StateIdle:
                    {
                        pInputData->state = COMP_StateIdle;
                        pthread_cond_signal(&pInputData->mStateCond);
                        break;
                    }
                    case COMP_StateExecuting:
                    {
                        pInputData->state = COMP_StateExecuting;
                        pthread_cond_signal(&pInputData->mStateCond);
                        break;
                    }
                    case COMP_StatePause:
                    {
                        pInputData->state = COMP_StatePause;
                        pthread_cond_signal(&pInputData->mStateCond);
                        break;
                    }
                    default:
                        break;
                }
                pthread_mutex_unlock(&pInputData->mStateLock);
            } 
            else if(cmd == VDecComp_VbsAvailable)
            {
                alogv("input thread got VDecComp_VbsAvailable message");
            }
            else if(cmd == VDecComp_InputData_UsedVbsAvailable) //for COMP_BufferSupplyOutput
            {
                alogv("input thread got VDecComp_InputData UsedVbsAvailable message");
            }
            else if (cmd == StopPort) 
            {
            } 
            else if (cmd == Flush) 
            {
            } 
            else if (cmd == Stop) 
            {// Kill thread     
                goto EXIT;
            }

            //precede to process message
            goto PROCESS_MESSAGE;
        }
        
        // wait until executing state
        if (pInputData->state != COMP_StateExecuting)
        {
            TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 0);
            goto PROCESS_MESSAGE;
        }

        if (pVideoDecData->mInputPortTunnelFlag == FALSE)
        {// inport non-tunnel mode
            //Do nothing right now
            TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 0);
            goto PROCESS_MESSAGE;
        }

        // Check Video Decoder
        if (NULL == pVideoDecData->pCedarV) 
        {
            aloge("fatal error! vdecLib is not create, can't request buffer.");
            goto EXIT;
        }

        // inport tunnel mode
        // Get A component
        //MM_COMPONENTTYPE *pInTunnelComp = (MM_COMPONENTTYPE*)(pPortTunnelInfo->hTunnel);

        if(COMP_BufferSupplyInput == pPortBufferSupplier->eBufferSupplier)
        {
            if(pInputData->mNodeNum != INPUT_BUF_COUNT)
            {
                aloge("fatal error! when dmx inPort use bufferSupplyInput, nodeNum[%d]!=[%d]", pInputData->mNodeNum, INPUT_BUF_COUNT);
            }
            // check whether we should request buffer
            pthread_mutex_lock(&pInputData->mVbsListLock);
            if (list_empty(&pInputData->mIdleVbsList))
            {// no need to request buffer if Idle list is empty
                if (pInputData->mWaitVbsValidFlag == TRUE)
                {
                    alogv("waiting message again when mWaitVbsValidFlag is set");
                }
                pInputData->mWaitVbsValidFlag = TRUE;
                pthread_mutex_unlock(&pInputData->mVbsListLock);
                TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 0);
                goto PROCESS_MESSAGE;
            }
            else
            {// request buffer to fill node in Idle list

                // Get buffer status of Decode Lib
                bsTotalSize = VideoStreamBufferSize(pVideoDecData->pCedarV, 0); // inport bs space total size
                bsValidSize = VideoStreamDataSize(pVideoDecData->pCedarV, 0);   // inport bs data_size that can be decode but have not be decoded
                bsEmptySize = bsTotalSize - bsValidSize - 64;
                //alogv("bsValidSize[%d] bsTotalSize[%d] bsEmptySize[%d]",bsValidSize, bsTotalSize, bsEmptySize);
                if (bsEmptySize < pInputData->nRequestLen || bsEmptySize <= 0)
                {// request Vbs buffer again after A component return bs buffer
                    alogv("low buffer (size = %d) from decode lib", bsEmptySize);
                    pthread_mutex_unlock(&pInputData->mVbsListLock);
                    TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 200);
                    goto PROCESS_MESSAGE;
                }
                else
                {
                    pInputData->nRequestLen = 0;
                }

                // Request buffer from Decode lib
                pEntry = list_first_entry(&pInputData->mIdleVbsList, DMXPKT_NODE_T, mList);

                pEntry->stEncodedStream.media_type   = CDX_PacketVideo;
                pEntry->stEncodedStream.nTobeFillLen = bsEmptySize;
                omx_buffer_header.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
                omx_buffer_header.nInputPortIndex    = pPortDef->nPortIndex;
                omx_buffer_header.nOutputPortIndex   = pPortTunnelInfo->nTunnelPortIndex;

                pthread_mutex_lock(&pVideoDecData->mVbsInputMutex);
                eError = RequestVideoStreamBuffer
                    (pVideoDecData->pCedarV
                    ,pEntry->stEncodedStream.nTobeFillLen
                    ,(char **)&pEntry->stEncodedStream.pBuffer
                    ,(int *)&pEntry->stEncodedStream.nBufferLen
                    ,(char **)&pEntry->stEncodedStream.pBufferExtra
                    ,(int *)&pEntry->stEncodedStream.nBufferExtraLen
                    ,0 //nStreamBufIndex default 0, not support multi stream now
                    );
                pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);
                if (eError != SUCCESS)
                {
                    aloge("fatal error! request buffer (size = %d) from decode lib fail, eError = %d", bsEmptySize, eError);
                    pthread_mutex_unlock(&pInputData->mVbsListLock);
                    TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 200);
                    goto PROCESS_MESSAGE;
                }

                list_move_tail(&pEntry->mList, &pInputData->mUsingVbsList);
                pthread_mutex_unlock(&pInputData->mVbsListLock);

                // 调用A component的FillThisBuffer
                eError = COMP_FillThisBuffer(pPortTunnelInfo->hTunnel, &omx_buffer_header);
                if (eError != SUCCESS)
                {
                    pthread_mutex_lock(&pInputData->mVbsListLock);
                    list_move_tail(&pEntry->mList, &pInputData->mIdleVbsList);
                    pthread_mutex_unlock(&pInputData->mVbsListLock);
                }

                if (eError == SUCCESS)
                {
                    alogv("alloc buffer from decode lib success! len = %d", pEntry->stEncodedStream.nTobeFillLen);
                }
                else if ((eError & 0x1FFF) == EN_ERR_NOT_PERM)
                {
                    TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 200);
                    goto PROCESS_MESSAGE;
                }
                else
                {
                    aloge("fatal error! Low probability! Tunneled inport component FillThisBuffer fail(%d)", eError);
                    TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 0);
                    goto PROCESS_MESSAGE;
                }
            }
        }
        else if(COMP_BufferSupplyOutput == pPortBufferSupplier->eBufferSupplier)
        {
            alogd("tunnel_BufferSupplyOutput is not processed here.");
            TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 0);
            goto PROCESS_MESSAGE;
        }
        else
        {
            aloge("fatal error! unsupport demux inPort buffer supplier mode[0x%x]", pPortBufferSupplier->eBufferSupplier);
        }
    }

EXIT:
    alogv("VideoDecoder InputThread stopped");
    return (void*) SUCCESS;
}

ERRORTYPE VideoDec_InputDataInit(VIDEODEC_INPUT_DATA *pInputData, void *pVideoDecData)
{
    ERRORTYPE eError = SUCCESS;
    memset(pInputData, 0x0, sizeof(VIDEODEC_INPUT_DATA));
    // Init input thread data
    pInputData->state = COMP_StateLoaded;
    pthread_mutex_init(&pInputData->mStateLock, NULL);

    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    int err = pthread_cond_init(&pInputData->mStateCond, &condAttr);
    if (err != 0) 
    {
        aloge("fatal error! pthread cond init fail!");
    }
    err = pthread_cond_init(&pInputData->mVbsFullCond, &condAttr);
    if (err != 0) 
    {
        aloge("fatal error! pthread cond init fail!");
    }
    
    pInputData->mWaitVbsValidFlag = FALSE;
    pInputData->nRequestLen = 0;
    
    INIT_LIST_HEAD(&pInputData->mIdleVbsList);
    INIT_LIST_HEAD(&pInputData->mReadyVbsList);
    INIT_LIST_HEAD(&pInputData->mUsingVbsList);
    INIT_LIST_HEAD(&pInputData->mUsedVbsList);
    pthread_mutex_init(&pInputData->mVbsListLock, NULL);
    
    DMXPKT_NODE_T *pNode;
    int i;
    pthread_mutex_lock(&pInputData->mVbsListLock);
    for (i = 0; i < INPUT_BUF_COUNT; i++)
    {
        pNode = (DMXPKT_NODE_T*)malloc(sizeof(DMXPKT_NODE_T));
        if(pNode == NULL)
        {
            aloge("fatal error! malloc fail!");
            eError = ERR_VDEC_NOMEM;
            pthread_mutex_unlock(&pInputData->mVbsListLock);
            goto EXIT6;
        }
        memset(pNode, 0, sizeof(DMXPKT_NODE_T));
        list_add_tail(&pNode->mList, &pInputData->mIdleVbsList);
        pInputData->mNodeNum++;
    }
    pthread_mutex_unlock(&pInputData->mVbsListLock);
    
    pInputData->pVideoDecData = pVideoDecData;

    if (message_create(&pInputData->cmd_queue)<0)
    {
        aloge("create input cmd queue error!");
        eError = ERR_VDEC_NOMEM;
        goto EXIT6;
    }
    
    err = pthread_create(&pInputData->thread_id, NULL, InputThread, pInputData);
    if (err || !pInputData->thread_id)
    {
        eError = ERR_VDEC_NOMEM;
        goto EXIT7;
    }
    pthread_condattr_destroy(&condAttr);
    return SUCCESS;

EXIT7:
    message_destroy(&pInputData->cmd_queue);
EXIT6:
{
    DMXPKT_NODE_T *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pInputData->mIdleVbsList, mList)
    {
        list_del(&pEntry->mList);
        free(pEntry);
    }
}
    pthread_mutex_destroy(&pInputData->mStateLock);
    pthread_condattr_destroy(&condAttr);
    pthread_cond_destroy(&pInputData->mStateCond);
    pthread_cond_destroy(&pInputData->mVbsFullCond);
    pthread_mutex_destroy(&pInputData->mVbsListLock);
    return eError;
}

ERRORTYPE VideoDec_InputDataDestroy(VIDEODEC_INPUT_DATA *pInputData)
{
    ERRORTYPE eError = SUCCESS;
    message_t msg;
    msg.command = Stop;
    put_message(&pInputData->cmd_queue, &msg);
    // Wait for thread to exit so we can get the status into "error"
    pthread_join(pInputData->thread_id, (void*)&eError);
    message_destroy(&pInputData->cmd_queue);

    DMXPKT_NODE_T *pEntry, *pTmp;
    pthread_mutex_lock(&pInputData->mVbsListLock);
    if (!list_empty(&pInputData->mIdleVbsList)) 
    {
        list_for_each_entry_safe(pEntry, pTmp, &pInputData->mIdleVbsList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    if (!list_empty(&pInputData->mReadyVbsList)) 
    {
        aloge("fatal error! ready vbs list is not empty!");
        list_for_each_entry_safe(pEntry, pTmp, &pInputData->mReadyVbsList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    if (!list_empty(&pInputData->mUsingVbsList)) 
    {
        aloge("fatal error! using vbs list is not empty!");
        list_for_each_entry_safe(pEntry, pTmp, &pInputData->mUsingVbsList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    if (!list_empty(&pInputData->mUsedVbsList)) 
    {
        aloge("fatal error! used vbs list is not empty!");
        list_for_each_entry_safe(pEntry, pTmp, &pInputData->mUsedVbsList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pInputData->mVbsListLock);
    pthread_mutex_destroy(&pInputData->mVbsListLock);
    
    pthread_mutex_destroy(&pInputData->mStateLock);
    pthread_cond_destroy(&pInputData->mStateCond);
    pthread_cond_destroy(&pInputData->mVbsFullCond);

    return SUCCESS;
}

