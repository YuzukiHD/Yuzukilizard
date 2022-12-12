

#include <utils/plat_log.h>

//ref platform headers
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#include <assert.h>

#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//media api headers to app
#include "SystemBase.h"
#include "mm_common.h"
#include "mm_comm_tenc.h"
#include "mpi_tenc.h"

//media internal common headers.
#include "media_common.h"
#include "mm_component.h"
#include "ComponentCommon.h"
#include "tmessage.h"
#include "tsemaphore.h"
#include "TextEncApi.h"
#include "TextEnc_Component.h"
#include "TencCompStream.h"
#include "EncodedStream.h"

extern ERRORTYPE TextEncResetChannel(PARAM_IN COMP_HANDLETYPE hComponent);
extern int TextEncLibInit(TEXTENCDATATYPE *pTextEncData);
static void* TextComponentThread(void* pThreadData) 
{ 
    int cmddata;
    CompInternalMsgType cmd;
    TEXTENCDATATYPE* pTextEncData = (TEXTENCDATATYPE*) pThreadData;
    message_t cmd_msg;

    int ret = 0;
    aloge("TextEncoder TextEncComponentThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"TextEncComp", 0, 0, 0);

    while(1)
    {
PROCESS_MESSAGE: 
        if(get_message(&pTextEncData->cmd_queue, &cmd_msg) == 0)
        {
            cmd = cmd_msg.command;
            cmddata = cmd_msg.para0; 

            // State transition command
            if (cmd == SetState)
            {
                // If the parameter states a transition to the same state
                //   raise a same state transition error.
                if (pTextEncData->state == (COMP_STATETYPE) (cmddata))
                    pTextEncData->pCallbacks->EventHandler(pTextEncData->hSelf,
                            pTextEncData->pAppData,
                            COMP_EventError,
                            ERR_TENC_SAMESTATE,
                            0,
                            NULL);
                else
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE) (cmddata))
                    {
                        case COMP_StateInvalid:
                        {
                            pTextEncData->state = COMP_StateInvalid;
                            pTextEncData->pCallbacks->EventHandler(pTextEncData->hSelf,
                                    pTextEncData->pAppData,
                                    COMP_EventError,
                                    ERR_TENC_INVALIDSTATE,
                                    0,
                                    NULL);
                            pTextEncData->pCallbacks->EventHandler(pTextEncData->hSelf,
                                    pTextEncData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pTextEncData->state,
                                    NULL);
                            break;
                        }
                        case COMP_StateLoaded:
                        {
                            if (pTextEncData->state != COMP_StateIdle)
                            {
                                pTextEncData->pCallbacks->EventHandler(
                                        pTextEncData->hSelf,
                                        pTextEncData->pAppData,
                                        COMP_EventError,
                                        ERR_TENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            TextEncResetChannel(pTextEncData->hSelf);

                            pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
                            pTextEncData->mWaitOutFrameFullFlag = TRUE;
                            //wait all outFrame return.
                            int cnt;
                            struct list_head *pList;
                            while(1)
                            {
                                cnt = 0;
                                list_for_each(pList, &pTextEncData->mIdleOutFrameList)
                                {
                                    cnt++;
                                }
                                if(cnt < pTextEncData->mFrameNodeNum)
                                {
                                    pthread_cond_wait(&pTextEncData->mOutFrameFullCondition, &pTextEncData->mOutFrameListMutex);
                                }
                                else
                                {
                                    break;
                                }
                            }
                            pTextEncData->mWaitOutFrameFullFlag = FALSE;
                            pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);


                            pTextEncData->state = COMP_StateLoaded;
                            pTextEncData->pCallbacks->EventHandler(
                                    pTextEncData->hSelf, pTextEncData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pTextEncData->state,
                                    NULL);
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            if(pTextEncData->state == COMP_StateLoaded)
                            {
                                if(NULL == pTextEncData->htext_enc)
                                {
                                    TextEncLibInit(pTextEncData);
                                }
                                pTextEncData->state = COMP_StateIdle;
                                pTextEncData->pCallbacks->EventHandler(
                                        pTextEncData->hSelf,
                                        pTextEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pTextEncData->state,
                                        NULL);
                            }
                            else if(pTextEncData->state == COMP_StatePause || pTextEncData->state == COMP_StateExecuting)
                            {
                                pTextEncData->state = COMP_StateIdle;
                                pTextEncData->pCallbacks->EventHandler(
                                        pTextEncData->hSelf,
                                        pTextEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pTextEncData->state,
                                        NULL);
                            }
                            else
                            {
                                pTextEncData->pCallbacks->EventHandler(
                                        pTextEncData->hSelf,
                                        pTextEncData->pAppData,
                                        COMP_EventError,
                                        ERR_TENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            
                            // Transition can only happen from pause or idle state
                            if (pTextEncData->state == COMP_StateIdle || pTextEncData->state == COMP_StatePause)
                            {
                                pTextEncData->state = COMP_StateExecuting;
                                pTextEncData->pCallbacks->EventHandler(
                                        pTextEncData->hSelf,
                                        pTextEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pTextEncData->state,
                                        NULL);
                            }
                            else
                            {
                                pTextEncData->pCallbacks->EventHandler(
                                        pTextEncData->hSelf,
                                        pTextEncData->pAppData,
                                        COMP_EventError,
                                        ERR_TENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        case COMP_StatePause:
                        {
                            // Transition can only happen from idle or executing state
                            if (pTextEncData->state == COMP_StateIdle || pTextEncData->state == COMP_StateExecuting)
                            {
                                pTextEncData->state = COMP_StatePause;
                                pTextEncData->pCallbacks->EventHandler(
                                        pTextEncData->hSelf,
                                        pTextEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pTextEncData->state,
                                        NULL);
                            }
                            else
                            {
                                pTextEncData->pCallbacks->EventHandler(
                                        pTextEncData->hSelf, pTextEncData->pAppData,
                                        COMP_EventError,
                                        ERR_TENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            else if (cmd == Flush)
            {
            }
            else if (cmd == Stop)
            {
                // Kill thread
                goto EXIT;
            }
            else if(cmd == TEncComp_InputTextAvailable)
            {
                pTextEncData->mNoInputTextFlag = 0;
            }
            else if(cmd == TEncComp_OutFrameAvailable)
            {
                pTextEncData->mNoOutFrameFlag = 0;
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if(pTextEncData->state == COMP_StateExecuting)
        { 
            // to enc one text frame
            ret = pTextEncData->htext_enc->TextEncodePro(pTextEncData->htext_enc);

            if(0 == ret)
            { 
TryGetTextEncBuf:
                pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
                if(list_empty(&pTextEncData->mIdleOutFrameList))
                {
                    alogw("Low probability! TEnc_Comp IdleOutFrameList is empty, malloc more!");
                    ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
                    if(pNode)
                    {
                        memset(pNode, 0, sizeof(ENCODER_NODE_T));
                        list_add_tail(&pNode->mList, &pTextEncData->mIdleOutFrameList);
                        pTextEncData->mFrameNodeNum++;
                    }
                    else
                    {
                        aloge("Fatal error! malloc fail!");
                        pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
                        if(TMessage_WaitQueueNotEmpty(&pTextEncData->cmd_queue, 200) > 0)
                        {
                            goto PROCESS_MESSAGE;
                        }
                        else
                        {
                            goto TryGetTextEncBuf;
                        }
                    }
                }

                ENCODER_NODE_T *pEntry = list_first_entry(&pTextEncData->mIdleOutFrameList, ENCODER_NODE_T, mList); 

                
                ret = pTextEncData->htext_enc->RequestTextEncBuf(pTextEncData->htext_enc, 
                    (void **)&pEntry->stEncodedStream.pBuffer
                    ,(unsigned int*)&pEntry->stEncodedStream.nBufferLen
                    ,&pEntry->stEncodedStream.nTimeStamp
                    ,&pEntry->stEncodedStream.nID
                    );
                pEntry->stEncodedStream.nFilledLen      = pEntry->stEncodedStream.nBufferLen;
                pEntry->stEncodedStream.nTimeStamp      = pEntry->stEncodedStream.nTimeStamp * 1000; //unit ms-->us
                pEntry->stEncodedStream.pBufferExtra    = NULL;
                pEntry->stEncodedStream.nBufferExtraLen = 0;
                if (ret != 0)
                {
                    alogw("(f:%s, l:%d) getTextEncBuf fail.", __FUNCTION__, __LINE__);
                    pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
                    continue;
                }

                if(pTextEncData->mOutputPortTunnelFlag)
                {
                    list_move_tail(&pEntry->mList, &pTextEncData->mUsedOutFrameList);  //Notes:Must move buf node to UsedList before calling EmptyThisBuffer
                    pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);

                    MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE*)(pTextEncData->sOutPortTunnelInfo.hTunnel);
                    COMP_BUFFERHEADERTYPE obh;
                    obh.nOutputPortIndex   = pTextEncData->sOutPortTunnelInfo.nPortIndex;
                    obh.nInputPortIndex    = pTextEncData->sOutPortTunnelInfo.nTunnelPortIndex;
                    obh.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;

                    
                    int omxRet = pOutTunnelComp->EmptyThisBuffer(pOutTunnelComp, &obh);
                    if(SUCCESS == omxRet)
                    { 
                        ret = pTextEncData->htext_enc->GetValidEncPktCnt(pTextEncData->htext_enc);
                        if (ret != 0) {
                            alogw("(f:%s, l:%d) Low probability! why not pick enced_pkt(cnt: %d) in time?!", __FUNCTION__, __LINE__, ret);
                            goto TryGetTextEncBuf;
                        }
                    }
                    else
                    { 
                        ret = pTextEncData->htext_enc->ReleaseTextEncBuf(pTextEncData->htext_enc, 
                            pEntry->stEncodedStream.pBuffer, 
                            pEntry->stEncodedStream.nBufferLen,
                            pEntry->stEncodedStream.nTimeStamp, 
                            pEntry->stEncodedStream.nID);

                        if(0 != ret)    
                        {
                            aloge("fatal error,releas text enc frm fail");
                        }
                        pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
                        list_move_tail(&pEntry->mList, &pTextEncData->mIdleOutFrameList);
                        pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
                    }
                }
                else
                {
                    list_move_tail(&pEntry->mList, &pTextEncData->mReadyOutFrameList);
                    if(pTextEncData->mWaitOutFrameFlag)
                    {
                        pthread_cond_signal(&pTextEncData->mOutFrameCondition);
                    }
                    pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex); 
                } 
            }
            else if(ERR_TEXT_ENC_INPUT_UNDERFLOW == ret)
            {
                pthread_mutex_lock(&pTextEncData->mInputTextMutex);
                int cnt = pTextEncData->htext_enc->GetValidInputPktCnt(pTextEncData->htext_enc);
                if (0 < cnt)
                {
                    alogw("(f:%s, l:%d) Low probability! TextEnc Component has input, cnt: [%ld]", __FUNCTION__, __LINE__, cnt);
                    pthread_mutex_unlock(&pTextEncData->mInputTextMutex);
                }
                else
                {
                    pTextEncData->mNoInputTextFlag = 1;
                    pthread_mutex_unlock(&pTextEncData->mInputTextMutex);
                    TMessage_WaitQueueNotEmpty(&pTextEncData->cmd_queue, 0);
                }
            }
            else if(ERR_TEXT_ENC_OUTFRM_UNDERFLOW == ret)
            { 
                alogw("TextEncComp_no out frm");
                pthread_mutex_lock(&pTextEncData->mOutFrameListMutex); 
                int emptyFrameCnt = pTextEncData->htext_enc->GetEmptyOutFrameCnt(pTextEncData->htext_enc); 
                if(0 < emptyFrameCnt)
                {
                    alogw("(f:%s, l:%d) Low probability! TextEnc Component has empty frames[%ld]", __FUNCTION__, __LINE__, emptyFrameCnt);
                    pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
                }
                else
                {
                    pTextEncData->mNoOutFrameFlag = 1;
                    pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
                    TMessage_WaitQueueNotEmpty(&pTextEncData->cmd_queue, 0);
                }
            }
            
        }
        else
        { 
            aloge("TextEnc_Comp not ind StateExecuting");
            TMessage_WaitQueueNotEmpty(&pTextEncData->cmd_queue, 0);
        }
    }

EXIT:    
    alogv("TextEncoder ComponentThread stopped");
    return (void*) SUCCESS; 
}


static ERRORTYPE TextEncSetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_CALLBACKTYPE* pCallbacks, PARAM_IN void* pAppData)
{
    TEXTENCDATATYPE *pTextEncData = NULL;
    ERRORTYPE eError = SUCCESS;

    pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(!pTextEncData || !pCallbacks || !pAppData)
    {
        eError = ERR_TENC_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    pTextEncData->pCallbacks = pCallbacks;
    pTextEncData->pAppData = pAppData;

COMP_CONF_CMD_FAIL: 
    return eError;
}

static ERRORTYPE TextEncSendCommand( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_COMMANDTYPE Cmd, PARAM_IN unsigned int nParam1, PARAM_IN void* pCmdData)
{
    TEXTENCDATATYPE *pTextEncData = NULL;
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;

    alogv("TextEncSendCommand: %d", Cmd);

    pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(!pTextEncData)
    {
        eError = ERR_TENC_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    if (pTextEncData->state == COMP_StateInvalid)
    {
        eError = ERR_TENC_SYS_NOTREADY;
        goto COMP_CONF_CMD_FAIL;
    }
    switch (Cmd)
    {
        case COMP_CommandStateSet:
            eCmd = SetState;
            break;

        case COMP_CommandFlush:
            eCmd = Flush;
            break;

        default:
            alogw("impossible comp_command[0x%x]", Cmd);
            eCmd = -1;
            break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pTextEncData->cmd_queue, &msg);

    COMP_CONF_CMD_FAIL: return eError;
}

static ERRORTYPE TextEncSetPortDefinition( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    if (pPortDef->nPortIndex == pTextEncData->sInPortDef.nPortIndex)
        memcpy(&pTextEncData->sInPortDef, pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else if (pPortDef->nPortIndex == pTextEncData->sOutPortDef.nPortIndex)
        memcpy(&pTextEncData->sOutPortDef, pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else
        eError = ERR_TENC_ILLEGAL_PARAM;

    return eError;
}

static ERRORTYPE TextEncSetCompBufferSupplier( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i = 0;
    
    for(i=0; i<MAX_TENCODER_PORTS; i++)
    {
        if(pPortBufSupplier->nPortIndex == pTextEncData->sPortBufSupplier[i].nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(&pTextEncData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_TENC_ILLEGAL_PARAM;
    }
    return eError;
}



static ERRORTYPE TextEncReleaseStream( PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN TEXT_STREAM_S *pStream)
{
    ERRORTYPE eError;
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(COMP_StateIdle != pTextEncData->state && COMP_StateExecuting != pTextEncData->state)
    {
        alogw("call getStream in wrong state[0x%x]", pTextEncData->state);
        return ERR_TENC_NOT_PERM;
    }
    if(pTextEncData->mOutputPortTunnelFlag)
    {
        aloge("fatal error! can't call getStream() in tunnel mode!");
        return ERR_TENC_NOT_PERM;
    }
    pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
    if(!list_empty(&pTextEncData->mUsedOutFrameList))
    {
        ENCODER_NODE_T *pEntry = list_first_entry(&pTextEncData->mUsedOutFrameList, ENCODER_NODE_T, mList);
        if(  pStream->pStream == pEntry->stEncodedStream.pBuffer
          && pStream->mLen == pEntry->stEncodedStream.nBufferLen )
        { 
           int ret = pTextEncData->htext_enc->ReleaseTextEncBuf(pTextEncData->htext_enc, 
                        pEntry->stEncodedStream.pBuffer, 
                        pEntry->stEncodedStream.nBufferLen, 
                        pEntry->stEncodedStream.nTimeStamp, 
                        pEntry->stEncodedStream.nID);
                
            list_move_tail(&pEntry->mList, &pTextEncData->mIdleOutFrameList);
            eError = SUCCESS;
        }
        else
        {
            aloge("fatal error! tenc stream[%p][%u] is not match UsedOutFrameList first entry[%p][%d]",
                pStream->pStream, pStream->mLen, pEntry->stEncodedStream.pBuffer, pEntry->stEncodedStream.nBufferLen);
            eError = ERR_TENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        alogw("Be careful! tenc stream[%p][%u] is not find, maybe reset channel before call this function?", pStream->pStream, pStream->mLen);
        eError = ERR_TENC_ILLEGAL_PARAM;                // samuel
    }
    pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
    if(pTextEncData->mNoOutFrameFlag)
    {
        message_t msg;
        msg.command = TEncComp_OutFrameAvailable;
        put_message(&pTextEncData->cmd_queue, &msg);
    }

    return eError;
}

 ERRORTYPE TextEncResetChannel(PARAM_IN COMP_HANDLETYPE hComponent)
{
    //ERRORTYPE eError;
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pTextEncData->state != COMP_StateIdle)
    {
        aloge("fatal error! must reset channel in stateIdle!");
        return ERR_TENC_NOT_PERM;
    }

    // not need return input pcm.
    // return output frames to tenclib.
    if(FALSE == pTextEncData->mOutputPortTunnelFlag)
    {
        //return output streams to tenclib directly. Don't worry about user take streams,
        //if user return stream after it, return ERR_TENC_ILLEGAL_PARAM.
        //user must guarantee that he return all streams before call this function.
        pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
        if(!list_empty(&pTextEncData->mUsedOutFrameList))
        {
            ENCODER_NODE_T *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pTextEncData->mUsedOutFrameList, mList)
            { 
                int ret = pTextEncData->htext_enc->ReleaseTextEncBuf(pTextEncData->htext_enc, 
                            pEntry->stEncodedStream.pBuffer, 
                            pEntry->stEncodedStream.nBufferLen, 
                            pEntry->stEncodedStream.nTimeStamp, 
                            pEntry->stEncodedStream.nID);
                    
                list_move_tail(&pEntry->mList, &pTextEncData->mIdleOutFrameList);
            }
        }
        if(!list_empty(&pTextEncData->mReadyOutFrameList))
        {
            ENCODER_NODE_T *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pTextEncData->mReadyOutFrameList, mList)
            {
                int ret = pTextEncData->htext_enc->ReleaseTextEncBuf(pTextEncData->htext_enc, 
                            pEntry->stEncodedStream.pBuffer, 
                            pEntry->stEncodedStream.nBufferLen, 
                            pEntry->stEncodedStream.nTimeStamp, 
                            pEntry->stEncodedStream.nID);

                list_move_tail(&pEntry->mList, &pTextEncData->mIdleOutFrameList);
            }
        }
        pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
    }
    else
    {
        //verify all output frames back to tenclib.
        //when component turn to stateIdle, it will guarantee all output frames back.
        pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
        if(!list_empty(&pTextEncData->mUsedOutFrameList))
        {
            aloge("fatal error! tenc is in tunnel mode!");
        }
        if(!list_empty(&pTextEncData->mReadyOutFrameList))
        {
            aloge("fatal error! tenc is in tunnel mode!");
        }
        pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
    }

    pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
    int cnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pTextEncData->mIdleOutFrameList)
    {
        cnt++;
    }
    if(cnt != pTextEncData->mFrameNodeNum)
    {
        alogw("Be careful! tenc output frames count not match [%d]!=[%d]", cnt, pTextEncData->mFrameNodeNum);
    }
    pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
    return SUCCESS;
}

static ERRORTYPE TextEncSetMPPChannelInfo( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MPP_CHN_S *pChn)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pTextEncData->mMppChnInfo = *pChn;
    
    return SUCCESS;
} 
        
int TextEncLibInit(TEXTENCDATATYPE *pTextEncData)
{
    int ret = 0;
    TEXTENCCONTENT_t *tenc_handle;

    tenc_handle = TextEncInit(&pTextEncData->text_info);
    if (tenc_handle == NULL) {
        ret = -1;
        aloge("TextEncInit failed");
    }
    pTextEncData->htext_enc = tenc_handle;

    return ret;
}


static ERRORTYPE TextEncSetChnAttr( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN TENC_CHN_ATTR_S *pChnAttr)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    
    pTextEncData->text_info.enc_type_enable_flag = pChnAttr->tInfo.enc_enable_type;
    pTextEncData->text_info.video_frame_rate = pChnAttr->tInfo.video_frame_rate;
    pTextEncData->text_info.gsensor_rate = pChnAttr->tInfo.gsensor_rate;
    pTextEncData->text_info.adas_rate = pChnAttr->tInfo.adas_rate;
    
    if (TextEncLibInit(pTextEncData) != 0)
    {
        return FAILURE;
    }
    return SUCCESS;
}


static ERRORTYPE TextEncSetConfig( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex, PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = TextEncSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = TextEncSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = TextEncSetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
       case COMP_IndexVendorTencChnAttr:
       {
            eError = TextEncSetChnAttr(hComponent, (TENC_CHN_ATTR_S*)pComponentConfigStructure);
            break;
       }
        case COMP_IndexVendorTencReleaseStream:
        {
            eError = TextEncReleaseStream(hComponent, (TEXT_STREAM_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTencResetChannel:
        {
            eError = TextEncResetChannel(hComponent);
            break;
        }
        default:
        {
            aloge("unknown Index[0x%x]", nIndex);
            eError = ERR_TENC_ILLEGAL_PARAM;
            break;
        }
    }

    return eError;
}

static ERRORTYPE TextEncGetPortDefinition( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if (pPortDef->nPortIndex == pTextEncData->sInPortDef.nPortIndex)
        memcpy(pPortDef, &pTextEncData->sInPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else if (pPortDef->nPortIndex == pTextEncData->sOutPortDef.nPortIndex)
        memcpy(pPortDef, &pTextEncData->sOutPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else
        eError = ERR_TENC_ILLEGAL_PARAM;

    return eError;
}

static ERRORTYPE TextEncGetCompBufferSupplier( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_TENCODER_PORTS; i++)
    {
        if(pPortBufSupplier->nPortIndex == pTextEncData->sPortBufSupplier[i].nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(pPortBufSupplier, &pTextEncData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_TENC_ILLEGAL_PARAM;
    }
    return eError;
}

static ERRORTYPE TextEncGetMPPChannelInfo( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT MPP_CHN_S *pChn)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pChn = pTextEncData->mMppChnInfo;
    return SUCCESS;
}


static ERRORTYPE TextEncGetChnAttr( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT TENC_CHN_ATTR_S *pChnAttr)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    
    pChnAttr->tInfo.enc_enable_type = pTextEncData->text_info.enc_type_enable_flag;
    pChnAttr->tInfo.video_frame_rate = pTextEncData->text_info.video_frame_rate;
    pChnAttr->tInfo.gsensor_rate = pTextEncData->text_info.gsensor_rate;
    pChnAttr->tInfo.adas_rate = pTextEncData->text_info.adas_rate;

    return SUCCESS;
}

static ERRORTYPE TextEncGetChannelFd(PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT int *pChnFd)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pChnFd = pTextEncData->mOutputFrameNotifyPipeFds[0];
    return SUCCESS;
}

static ERRORTYPE TextEncGetTunnelInfo( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo)
{
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = ERR_TENC_UNEXIST;

    if(pTunnelInfo->nPortIndex == pTextEncData->sInPortTunnelInfo.nPortIndex)
    {
        memcpy(pTunnelInfo, &pTextEncData->sInPortTunnelInfo, sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
        eError = SUCCESS;
    }
    else if(pTunnelInfo->nPortIndex == pTextEncData->sOutPortTunnelInfo.nPortIndex)
    {
        memcpy(pTunnelInfo, &pTextEncData->sOutPortTunnelInfo, sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_TENC_UNEXIST;
    }
    return eError;
}

static ERRORTYPE config_TENC_STREAM_S_by_TencOutputBuffer(TEXT_STREAM_S *pDst, EncodedStream *pSrc, 
                                           TEXTENCDATATYPE *pTextEncData)
{
    pDst->pStream = (unsigned char*)pSrc->pBuffer;
    pDst->mLen = pSrc->nBufferLen;
    pDst->mTimeStamp = pSrc->nTimeStamp;
    pDst->mId = pSrc->nID;

    return SUCCESS;
} 


static ERRORTYPE TextEncGetStream( PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT TEXT_STREAM_S *pStream, PARAM_IN int nMilliSec)
{
    ERRORTYPE eError;
    int ret;
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(COMP_StateIdle != pTextEncData->state && COMP_StateExecuting != pTextEncData->state)
    {
        alogw("call getStream in wrong state[0x%x]", pTextEncData->state);
        return ERR_TENC_NOT_PERM;
    }
    if(pTextEncData->mOutputPortTunnelFlag)
    {
        aloge("fatal error! can't call getStream() in tunnel mode!");
        return ERR_TENC_NOT_PERM;
    }
    pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
_TryToGetOutFrame:
    if(!list_empty(&pTextEncData->mReadyOutFrameList))
    {
        ENCODER_NODE_T *pEntry = list_first_entry(&pTextEncData->mReadyOutFrameList, ENCODER_NODE_T, mList);
        config_TENC_STREAM_S_by_TencOutputBuffer(pStream, &pEntry->stEncodedStream, pTextEncData);
        list_move_tail(&pEntry->mList, &pTextEncData->mUsedOutFrameList);
        char tmpRdCh;
        read(pTextEncData->mOutputFrameNotifyPipeFds[0], &tmpRdCh, 1);
        eError = SUCCESS;
    }
    else
    {
        if(0 == nMilliSec)
        {
            eError = ERR_TENC_BUF_EMPTY;
        }
        else if(nMilliSec < 0)
        {
            pTextEncData->mWaitOutFrameFlag = TRUE;
            while(list_empty(&pTextEncData->mReadyOutFrameList))
            {
                pthread_cond_wait(&pTextEncData->mOutFrameCondition, &pTextEncData->mOutFrameListMutex);
            }
            pTextEncData->mWaitOutFrameFlag = FALSE;
            goto _TryToGetOutFrame;
        }
        else
        {
            pTextEncData->mWaitOutFrameFlag = TRUE;
            ret = pthread_cond_wait_timeout(&pTextEncData->mOutFrameCondition, &pTextEncData->mOutFrameListMutex, nMilliSec);
            if(ETIMEDOUT == ret)
            {
                alogv("wait output frame timeout[%d]ms, ret[%d]", nMilliSec, ret);
                eError = ERR_TENC_BUF_EMPTY;
                pTextEncData->mWaitOutFrameFlag = FALSE;
            }
            else if(0 == ret)
            {
                pTextEncData->mWaitOutFrameFlag = FALSE;
                goto _TryToGetOutFrame;
            }
            else
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                eError = ERR_TENC_BUF_EMPTY;
                pTextEncData->mWaitOutFrameFlag = FALSE;
            }
        }
    }
    pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
    return eError;
}

static ERRORTYPE TextEncGetConfig( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex, PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;

    switch(nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = TextEncGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = TextEncGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = TextEncGetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelFd:
        {
            eError = TextEncGetChannelFd(hComponent, (int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTunnelInfo:
        {
            eError = TextEncGetTunnelInfo(hComponent, (COMP_INTERNAL_TUNNELINFOTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTencChnAttr:
        {
            eError = TextEncGetChnAttr(hComponent, (TENC_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTencGetStream:
        {
            TEncStream *pStream = (TEncStream*)pComponentConfigStructure;
            eError = TextEncGetStream(hComponent, pStream->pStream, pStream->nMilliSec);
            break;
        }
        default:
        {
            aloge("fatal error! unknown getConfig Index[0x%x]", nIndex);
            eError = ERR_TENC_NOT_SUPPORT;
            break;
        }
    }
    return eError;
}


static ERRORTYPE TextEncGetState( PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_STATETYPE* pState)
{
    ERRORTYPE eError = SUCCESS;
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    *pState = pTextEncData->state;

    return eError;
} 

static ERRORTYPE TextEncComponentTunnelRequest( PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  unsigned int nPort, 
        PARAM_IN COMP_HANDLETYPE hTunneledComp, PARAM_IN  unsigned int nTunneledPort, 
        PARAM_INOUT  COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pTextEncData->state == COMP_StateExecuting)
    {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    }
    else if(pTextEncData->state != COMP_StateIdle)
    {
        aloge("fatal error! tunnel request can't be in state[0x%x]", pTextEncData->state);
        eError = ERR_TENC_INCORRECT_STATE_OPERATION;
        goto COMP_CMD_FAIL;
    }

    COMP_PARAM_PORTDEFINITIONTYPE   *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE    *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE   *pPortBufSupplier;
    BOOL bFindFlag;
    int i;
    bFindFlag = FALSE;
    if(pTextEncData->sInPortDef.nPortIndex == nPort)
    {
        pPortDef = &pTextEncData->sInPortDef;
        bFindFlag = TRUE;
    }
    if(FALSE == bFindFlag)
    {
        if(pTextEncData->sOutPortDef.nPortIndex == nPort)
        {
            pPortDef = &pTextEncData->sOutPortDef;
            bFindFlag = TRUE;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_TENC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    if(pTextEncData->sInPortTunnelInfo.nPortIndex == nPort)
    {
        pPortTunnelInfo = &pTextEncData->sInPortTunnelInfo;
        bFindFlag = TRUE;
    }
    if(FALSE == bFindFlag)
    {
        if(pTextEncData->sOutPortTunnelInfo.nPortIndex == nPort)
        {
            pPortTunnelInfo = &pTextEncData->sOutPortTunnelInfo;
            bFindFlag = TRUE;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_TENC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for(i=0; i<MAX_TENCODER_PORTS; i++)
    {
        if(pTextEncData->sPortBufSupplier[i].nPortIndex == nPort)
        {
            pPortBufSupplier = &pTextEncData->sPortBufSupplier[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_TENC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }
    
    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if(NULL==hTunneledComp && 0==nTunneledPort && NULL==pTunnelSetup)
    {
        alogd("omx_core cancel setup tunnel on port[%d]", nPort);
        eError = SUCCESS;
        if(pPortDef->eDir == COMP_DirOutput)
        {
            pTextEncData->mOutputPortTunnelFlag = FALSE;
        }
        else
        {
            pTextEncData->mInputPortTunnelFlag = FALSE;
        }
        goto COMP_CMD_FAIL;
    }
    if(pPortDef->eDir == COMP_DirOutput)
    {
        if (pTextEncData->mOutputPortTunnelFlag) {
            aloge("TEnc_Comp outport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        pTextEncData->mOutputPortTunnelFlag = TRUE;

    }
    else
    {
        if (pTextEncData->mInputPortTunnelFlag) {
            aloge("TEnc_Comp inport already bind, why bind again?!");
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
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_TENC_ILLEGAL_PARAM;
            goto COMP_CMD_FAIL;
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
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        ((MM_COMPONENTTYPE*)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        pTextEncData->mInputPortTunnelFlag = TRUE;
    }

COMP_CMD_FAIL:
    return eError;
}

static ERRORTYPE TextEncEmptyThisBuffer( PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    ERRORTYPE eError = SUCCESS;
    TEXTENCDATATYPE *pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if (pTextEncData->state != COMP_StateExecuting)
    {
        alogw("send frame when tenc state[0x%x] isn't executing", pTextEncData->state);
        //eError = COMP_ErrorInvalidState;
        //goto ERROR;
    }

    if ((pTextEncData->mInputPortTunnelFlag==TRUE && pBuffer->nInputPortIndex==pTextEncData->sInPortDef.nPortIndex) ||
        (pTextEncData->mInputPortTunnelFlag==FALSE))
    {
        TEXT_FRAME_S *pFrm = (TEXT_FRAME_S *)pBuffer->pOutputPortPrivate;     // samuel
        void *pTextBuf = pFrm->mpAddr;
        unsigned int dataSize = pFrm->mLen;

        int ret = pTextEncData->htext_enc->RequestWriteBuf(pTextEncData->htext_enc, pTextBuf,dataSize); 
        if (ret != 0)
        {
            pTextEncData->mSendTextFailCnt++;
            eError = FAILURE;
            goto ERROR;
        }

        if(pTextEncData->mNoInputTextFlag)
        {
            message_t msg;
            msg.command = TEncComp_InputTextAvailable;
            put_message(&pTextEncData->cmd_queue, &msg);
        }
    }
    else
    {
        aloge("fatal error! RecRender should not call this function!");
        eError = FAILURE;
    } 

ERROR:
    return eError;
}

static ERRORTYPE TextEncFillThisBuffer( PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    TEXTENCDATATYPE *pTextEncData = NULL;
    ERRORTYPE eError = SUCCESS;
    int ret;

    pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pBuffer->nOutputPortIndex == pTextEncData->sOutPortDef.nPortIndex)
    {
        pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
        EncodedStream *pOutFrame = (EncodedStream*)pBuffer->pOutputPortPrivate;

        BOOL bFind = FALSE;
        ENCODER_NODE_T *pEntry;
        list_for_each_entry(pEntry, &pTextEncData->mUsedOutFrameList, mList)
        {
            if (pEntry->stEncodedStream.nID == pOutFrame->nID)
            {
                bFind = TRUE;
                break;
            }
        }

        if (!bFind)
        {
            pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
            aloge("fatal error! try to release one output buffer that never be used! ID = %d", pOutFrame->nID);
            return ERR_TENC_ILLEGAL_PARAM;
        }

        ret = pTextEncData->htext_enc->ReleaseTextEncBuf(pTextEncData->htext_enc, 
                pEntry->stEncodedStream.pBuffer, pEntry->stEncodedStream.nBufferLen,
                pEntry->stEncodedStream.nTimeStamp, pEntry->stEncodedStream.nID); 
        if (ret != 0)
        {
            aloge("textenc_to_releas_frm_fail");
            eError = FAILURE;
        }

        list_move_tail(&pEntry->mList, &pTextEncData->mIdleOutFrameList);

        if (pTextEncData->mNoOutFrameFlag)
        {
            message_t msg;
            msg.command = TEncComp_OutFrameAvailable;
            put_message(&pTextEncData->cmd_queue, &msg);
        }

        if(pTextEncData->mWaitOutFrameFullFlag)
        {
            int cnt = 0;
            struct list_head *pList;
            list_for_each(pList, &pTextEncData->mIdleOutFrameList)
            {
                cnt++;
            }
            if(cnt>=pTextEncData->mFrameNodeNum)
            {
                pthread_cond_signal(&pTextEncData->mOutFrameFullCondition);
            }
        }
        pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);
    }
    else
    {
        aloge("fatal error! outPortIndex[%d]!=[%d]", pBuffer->nOutputPortIndex, pTextEncData->sOutPortDef.nPortIndex);
        eError = FAILURE;
    }

    return eError;
} 


ERRORTYPE TextEncComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE *pComp = NULL;
    TEXTENCDATATYPE *pTextEncData = NULL;
    ERRORTYPE eError = SUCCESS;
    unsigned int err;
    int i = 0;

    pComp = (MM_COMPONENTTYPE *)hComponent;

    // Create private data
    pTextEncData = (TEXTENCDATATYPE *) malloc(sizeof(TEXTENCDATATYPE));
    memset(pTextEncData, 0x0, sizeof(TEXTENCDATATYPE));

    err = pipe(pTextEncData->mOutputFrameNotifyPipeFds);
    if (err)
    {
        eError = ERR_TENC_NOMEM;
        goto EXIT;
    }
    pComp->pComponentPrivate = (void*)pTextEncData;
    pTextEncData->state = COMP_StateLoaded;
    pthread_mutex_init(&pTextEncData->mStateLock,NULL);
    pTextEncData->hSelf = hComponent;

    pTextEncData->mNoInputTextFlag = 0;
    pTextEncData->mNoOutFrameFlag = 0;
    
    INIT_LIST_HEAD(&pTextEncData->mIdleOutFrameList);
    INIT_LIST_HEAD(&pTextEncData->mReadyOutFrameList);
    INIT_LIST_HEAD(&pTextEncData->mUsedOutFrameList);
    for(i=0; i<TENC_FRAME_COUNT; i++)
    {
        ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            break;
        }
        memset(pNode, 0, sizeof(ENCODER_NODE_T));
        list_add_tail(&pNode->mList, &pTextEncData->mIdleOutFrameList);
        pTextEncData->mFrameNodeNum++;
    }
    err = pthread_mutex_init(&pTextEncData->mInputTextMutex, NULL);
    if(err != 0)
    {
        aloge("pthread mutex init fail!");
        eError = ERR_TENC_NOMEM;
        goto EXIT;
    }
    err = pthread_mutex_init(&pTextEncData->mOutFrameListMutex, NULL);
    if(err != 0)
    {
        aloge("pthread mutex init fail!");
        eError = ERR_TENC_NOMEM;
        goto EXIT;
    }

    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    err = pthread_cond_init(&pTextEncData->mOutFrameFullCondition, &condAttr);
    if(err != 0)
    {
        aloge("pthread cond init fail!");
        eError = ERR_TENC_NOMEM;
        goto EXIT;
    }
    pthread_cond_init(&pTextEncData->mOutFrameCondition, &condAttr);   // used to notify the new frame is produced

    // Fill in function pointers
    pComp->SetCallbacks     = TextEncSetCallbacks;
    pComp->SendCommand      = TextEncSendCommand;
    pComp->GetConfig        = TextEncGetConfig;
    pComp->SetConfig        = TextEncSetConfig;
    pComp->GetState         = TextEncGetState;
    pComp->ComponentTunnelRequest = TextEncComponentTunnelRequest;
    pComp->ComponentDeInit  = TextEncComponentDeInit;
    pComp->EmptyThisBuffer  = TextEncEmptyThisBuffer;
    pComp->FillThisBuffer   = TextEncFillThisBuffer;

    // Initialize component data structures to default values
    pTextEncData->sPortParam.nPorts = 0x2;
    pTextEncData->sPortParam.nStartPortNumber = 0x0;

    // Initialize the audio parameters for input port
    pTextEncData->sInPortDef.nPortIndex = 0x0;
    pTextEncData->sInPortDef.bEnabled = TRUE;
    pTextEncData->sInPortDef.eDomain = COMP_PortDomainText;
    pTextEncData->sInPortDef.eDir = COMP_DirInput;
//    pTextEncData->sInPortDef.format.audio.cMIMEType = "AAC";
//    pTextEncData->sInPortDef.format.audio.eEncoding = PT_AAC;

    // Initialize the audio parameters for output port
    pTextEncData->sOutPortDef.nPortIndex = 0x1;
    pTextEncData->sOutPortDef.bEnabled = TRUE;
    pTextEncData->sOutPortDef.eDomain = COMP_PortDomainText;
    pTextEncData->sOutPortDef.eDir = COMP_DirOutput;
//    pTextEncData->sOutPortDef.format.audio.cMIMEType = "AAC";
//    pTextEncData->sOutPortDef.format.audio.eEncoding = PT_AAC;

    pTextEncData->sPortBufSupplier[0].nPortIndex = 0x0;
    pTextEncData->sPortBufSupplier[0].eBufferSupplier = COMP_BufferSupplyOutput;
    pTextEncData->sPortBufSupplier[1].nPortIndex = 0x1;
    pTextEncData->sPortBufSupplier[1].eBufferSupplier = COMP_BufferSupplyOutput;

    pTextEncData->sInPortTunnelInfo.nPortIndex = 0x0;
    pTextEncData->sInPortTunnelInfo.eTunnelType = TUNNEL_TYPE_COMMON;
    pTextEncData->sOutPortTunnelInfo.nPortIndex = 0x1;
    pTextEncData->sOutPortTunnelInfo.eTunnelType = TUNNEL_TYPE_COMMON;

    if (message_create(&pTextEncData->cmd_queue)<0)
    {
        aloge("message error!");
        eError = ERR_TENC_NOMEM;
        goto EXIT;
    }

    // Create the component thread
    err = pthread_create(&pTextEncData->thread_id, NULL, TextComponentThread, pTextEncData);
    if (err || !pTextEncData->thread_id)
    {
        eError = ERR_TENC_NOMEM;
        goto EXIT;
    }

    EXIT: return eError;
}


ERRORTYPE TextEncComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    TEXTENCDATATYPE *pTextEncData = NULL;
    ERRORTYPE eError = SUCCESS;
    CompInternalMsgType eCmd = Stop;
    
    message_t msg; 
    memset(&msg,0,sizeof(msg));
    
    alogv("TextEnc Component DeInit");
    pTextEncData = (TEXTENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
	if(NULL == pTextEncData)
	{
		aloge("pTextEncData is NULL!!!\n");
		return FAILURE;
	}
    msg.command = eCmd;
    put_message(&pTextEncData->cmd_queue, &msg);
    alogv("wait TextEnc component exit!...");
    pthread_join(pTextEncData->thread_id, (void*) &eError);

    if(NULL != pTextEncData->htext_enc)
    {
        TextEncExit(pTextEncData->htext_enc);
        pTextEncData->htext_enc = NULL;
    }

    message_destroy(&pTextEncData->cmd_queue);

    pthread_mutex_lock(&pTextEncData->mOutFrameListMutex);
    if(!list_empty(&pTextEncData->mUsedOutFrameList))
    {
        aloge("fatal error! outUsedFrame must be 0!");
    }
    if(!list_empty(&pTextEncData->mReadyOutFrameList))
    {
        aloge("fatal error! outReadyFrame must be 0!");
    }
    int nodeNum = 0;
    if(!list_empty(&pTextEncData->mIdleOutFrameList))
    {
        ENCODER_NODE_T *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pTextEncData->mIdleOutFrameList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            nodeNum++;
        }
    }
    if(nodeNum != pTextEncData->mFrameNodeNum)
    {
        aloge("Fatal error! TextEnc frame_node number is not match: [%d][%d]", nodeNum, pTextEncData->mFrameNodeNum);
    }
    pthread_mutex_unlock(&pTextEncData->mOutFrameListMutex);

    pthread_mutex_destroy(&pTextEncData->mInputTextMutex);
    pthread_mutex_destroy(&pTextEncData->mOutFrameListMutex);
    pthread_mutex_destroy(&pTextEncData->mStateLock);
    pthread_cond_destroy(&pTextEncData->mOutFrameFullCondition);
    pthread_cond_destroy(&pTextEncData->mOutFrameCondition);
	if(NULL != pTextEncData)
	{
		close(pTextEncData->mOutputFrameNotifyPipeFds[0]);
		close(pTextEncData->mOutputFrameNotifyPipeFds[1]);
	}
    if (pTextEncData)
    {
        free(pTextEncData);
    }

    alogd("TextEnc component exited!");

    return eError;
}



