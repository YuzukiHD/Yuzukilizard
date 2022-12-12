

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

//media api headers to app
#include "mm_common.h"
#include "mm_comm_tenc.h"
#include "mpi_tenc.h"

//media internal common headers.
#include <media_common.h>
#include <tsemaphore.h>
#include <mm_component.h>
#include <mpi_tenc_common.h>
#include <TencCompStream.h>
#include <TextEncApi.h> 


typedef struct TENC_CHN_MAP_S
{
    TENC_CHN            mTeChn;     // audio encoder channel index, [0, AENC_MAX_CHN_NUM)
    MM_COMPONENTTYPE    *mEncComp;  // audio encoder component instance
    cdx_sem_t           mSemCompCmd;
    MPPCallbackInfo     mCallbackInfo;
    struct list_head    mList;
}TENC_CHN_MAP_S; 

typedef struct TencChnManager
{
    struct list_head mList;
    pthread_mutex_t mLock;
} TencChnManager;

static TencChnManager *gpTencChnMap = NULL;

ERRORTYPE TENC_Construct(void)
{
    int ret = 0;
    if (gpTencChnMap != NULL) {
        return SUCCESS;
    }
    gpTencChnMap = (TencChnManager*)malloc(sizeof(TencChnManager));
    if (NULL == gpTencChnMap) {
        aloge("fatal error!alloc TencChnManager error(%s)!", strerror(errno));
        return FAILURE;
    }
    ret = pthread_mutex_init(&gpTencChnMap->mLock, NULL);
    if (ret != 0) {
        aloge("fatal error! mutex init fail");
        free(gpTencChnMap);
        gpTencChnMap = NULL;
        return FAILURE;
    }
    INIT_LIST_HEAD(&gpTencChnMap->mList);
    return SUCCESS;
}

ERRORTYPE TENC_Destruct(void)
{
    if (gpTencChnMap != NULL) {
        if (!list_empty(&gpTencChnMap->mList)) {
            aloge("fatal error! some tenc channel still running when destroy aenc device!");
        }
        pthread_mutex_destroy(&gpTencChnMap->mLock);
        free(gpTencChnMap);
        gpTencChnMap = NULL;
    }
    return SUCCESS;
}

static ERRORTYPE searchExistChannel_l(TENC_CHN TeChn, TENC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    TENC_CHN_MAP_S* pEntry = NULL;

    if (gpTencChnMap == NULL) {
        return FAILURE;
    }

    list_for_each_entry(pEntry, &gpTencChnMap->mList, mList)
    {
        if(pEntry->mTeChn == TeChn)
        {
            if(ppChn)
            {
                *ppChn = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    return ret;
}

static ERRORTYPE searchExistChannel(TENC_CHN TeChn, TENC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    TENC_CHN_MAP_S* pEntry = NULL;

    if (gpTencChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpTencChnMap->mLock);
    ret = searchExistChannel_l(TeChn, ppChn);
    pthread_mutex_unlock(&gpTencChnMap->mLock);
    return ret;
}

static ERRORTYPE addChannel_l(TENC_CHN_MAP_S *pChn)
{
    if (gpTencChnMap == NULL) {
        return FAILURE;
    }
    list_add_tail(&pChn->mList, &gpTencChnMap->mList);
    return SUCCESS;
}

static ERRORTYPE addChannel(TENC_CHN_MAP_S *pChn)
{
    if (gpTencChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpTencChnMap->mLock);
    ERRORTYPE ret = addChannel_l(pChn);
    pthread_mutex_unlock(&gpTencChnMap->mLock);
    return ret;
}

static ERRORTYPE removeChannel(TENC_CHN_MAP_S *pChn)
{
    if (gpTencChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpTencChnMap->mLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gpTencChnMap->mLock);
    return SUCCESS;
}

MM_COMPONENTTYPE *TENC_GetChnComp(PARAM_IN MPP_CHN_S *pMppChn)
{
    TENC_CHN_MAP_S* pChn = NULL;

    if (searchExistChannel(pMppChn->mChnId, &pChn) != SUCCESS) {
        return NULL;
    }
    return pChn->mEncComp;
}

static TENC_CHN_MAP_S* TENC_CHN_MAP_S_Construct()
{
    TENC_CHN_MAP_S *pChannel = (TENC_CHN_MAP_S*)malloc(sizeof(TENC_CHN_MAP_S));
    if(NULL == pChannel)
    {
        return NULL;
    }
    memset(pChannel, 0, sizeof(TENC_CHN_MAP_S));
    cdx_sem_init(&pChannel->mSemCompCmd, 0);
    return pChannel;
}

static void TENC_CHN_MAP_S_Destruct(TENC_CHN_MAP_S *pChannel)
{
    if(NULL != pChannel)
    {
        if(NULL!=pChannel->mEncComp)
        {
            aloge("fatal error! Tenc component need free before!");
            COMP_FreeHandle(pChannel->mEncComp);
            pChannel->mEncComp = NULL;
        }
        cdx_sem_deinit(&pChannel->mSemCompCmd);
        free(pChannel);
    }
}

static ERRORTYPE TextEncEventHandler( PARAM_IN COMP_HANDLETYPE hComponent,
     PARAM_IN void* pAppData, PARAM_IN COMP_EVENTTYPE eEvent,
     PARAM_IN unsigned int nData1, PARAM_IN unsigned int nData2,
     PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
    TENC_CHN_MAP_S *pChn = (TENC_CHN_MAP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("encoder EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else
            {
                alogd("Low probability! what command[0x%x]?", nData1);
                break;
            }
        }
        case COMP_EventError:
        {
            if(ERR_TENC_SAMESTATE == nData1)
            {
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_TENC_INVALIDSTATE == nData1)
            {
                aloge("why tenc state turn to invalid?");
                break;
            }
            else if(ERR_TENC_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! tenc state transition incorrect.");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
    return SUCCESS;
}

COMP_CALLBACKTYPE TextEncCallback = {
    .EventHandler = TextEncEventHandler,
    .EmptyBufferDone = NULL,
    .FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_TENC_CreateChn(TENC_CHN TeChn, const TENC_CHN_ATTR_S *pAttr)
{

    if(!(TeChn>=0 && TeChn <TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    if(NULL == pAttr)
    {
        aloge("fatal error! illagal AEncAttr!");
        return ERR_TENC_ILLEGAL_PARAM;
    }
    if (NULL == gpTencChnMap)
    {
        return FAILURE;
    }

    pthread_mutex_lock(&gpTencChnMap->mLock);

    if(SUCCESS == searchExistChannel_l(TeChn, NULL))
    {
        pthread_mutex_unlock(&gpTencChnMap->mLock);
        return ERR_TENC_EXIST;
    } 
    
    TENC_CHN_MAP_S *pNode = TENC_CHN_MAP_S_Construct();
    pNode->mTeChn = TeChn;

    //create Aenc Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mEncComp, CDX_ComponentNameTextEncoder, (void*)pNode, &TextEncCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    } 
    
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_TENC;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mTeChn;
    eRet = pNode->mEncComp->SetConfig(pNode->mEncComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    eRet = pNode->mEncComp->SetConfig(pNode->mEncComp, COMP_IndexVendorTencChnAttr, (void*)pAttr);
    eRet = pNode->mEncComp->SendCommand(pNode->mEncComp, COMP_CommandStateSet, COMP_StateIdle, NULL);

    cdx_sem_down(&pNode->mSemCompCmd);

    addChannel_l(pNode);

    pthread_mutex_unlock(&gpTencChnMap->mLock); 
    
    return SUCCESS; 
}

ERRORTYPE AW_MPI_TENC_DestroyChn(TENC_CHN TeChn)
{
    ERRORTYPE ret;
    
    if(!(TeChn>=0 && TeChn <TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    ERRORTYPE eRet;
    if(pChn->mEncComp)
    {
        COMP_STATETYPE nCompState;
        if(SUCCESS == pChn->mEncComp->GetState(pChn->mEncComp, &nCompState))
        {
            if(nCompState == COMP_StateIdle)
            { 
                eRet = pChn->mEncComp->SendCommand(pChn->mEncComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);

                cdx_sem_down(&pChn->mSemCompCmd);
                eRet = SUCCESS;
            }
            else if(nCompState == COMP_StateLoaded)
            {
                eRet = SUCCESS;
            }
            else if(nCompState == COMP_StateInvalid)
            {
                alogw("Low probability! Component StateInvalid?");
                eRet = SUCCESS;
            }
            else
            {
                aloge("Fatal error! invalid TeChn[%d] state[0x%x]!", TeChn, nCompState);
                eRet = FAILURE;
            }

            if(eRet == SUCCESS)
            {
                removeChannel(pChn);
                COMP_FreeHandle(pChn->mEncComp);
                pChn->mEncComp = NULL;
                TENC_CHN_MAP_S_Destruct(pChn);
                ret = SUCCESS;
            }
            else
            {
                ret = ERR_TENC_BUSY;
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_TENC_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no tenc component!");
        list_del(&pChn->mList);
        TENC_CHN_MAP_S_Destruct(pChn);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_TENC_ResetChn(TENC_CHN TeChn)
{
    if(!(TeChn>=0 && TeChn <TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    
    COMP_STATETYPE nCompState = COMP_StateInvalid;
    ERRORTYPE eRet, eRet2;
    eRet = pChn->mEncComp->GetState(pChn->mEncComp, &nCompState);
    if(eRet == SUCCESS && COMP_StateIdle == nCompState)
    { 
        eRet2 = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorTencResetChannel, NULL);
        if(eRet2 != SUCCESS)
        {
            aloge("fatal error! reset channel fail[0x%x]!", eRet2);
        }
        
        return eRet2;
    }
    else
    {
        aloge("wrong status[0x%x], can't reset aenc channel!", nCompState);
        return ERR_TENC_NOT_PERM;
    }
}

ERRORTYPE AW_MPI_TENC_StartRecvText(TENC_CHN TeChn)
{
    if(!(TeChn>=0 && TeChn<TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mEncComp->GetState(pChn->mEncComp, &nCompState);
    if(COMP_StateIdle == nCompState)
    {
        eRet = pChn->mEncComp->SendCommand(pChn->mEncComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        
        cdx_sem_down(&pChn->mSemCompCmd); 
        ret = SUCCESS;
    }
    else
    {
        alogd("TencChannelState[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_TENC_StopRecvText(TENC_CHN TeChn)
{
    if(!(TeChn>=0 && TeChn < TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mEncComp->GetState(pChn->mEncComp, &nCompState);
    if(COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mEncComp->SendCommand(pChn->mEncComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogv("TencChannelState[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! check TencChannelState[0x%x]!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_TENC_RegisterCallback(TENC_CHN TeChn, MPPCallbackInfo *pCallback)
{
    if(!(TeChn>=0 && TeChn <TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_TENC_SetChnAttr(TENC_CHN TeChn, const TENC_CHN_ATTR_S *pAttr)
{
    if(!(TeChn>=0 && TeChn <TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_TENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorTencChnAttr, (void*)pAttr);
    
    return ret;
}

ERRORTYPE AW_MPI_TENC_GetChnAttr(TENC_CHN TeChn, TENC_CHN_ATTR_S *pAttr)
{
    if(!(TeChn>=0 && TeChn <TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_TENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorTencChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_TENC_SendFrame(TENC_CHN TeChn, TEXT_FRAME_S *pFrame)
{
    if(!(TeChn>=0 && TeChn<TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_TENC_NOT_PERM;
    }
    COMP_BUFFERHEADERTYPE bufferHeader;
    bufferHeader.pOutputPortPrivate = pFrame;
    ret = pChn->mEncComp->EmptyThisBuffer(pChn->mEncComp, &bufferHeader);
    return ret;
}

ERRORTYPE AW_MPI_TENC_GetStream(TENC_CHN TeChn, TEXT_STREAM_S *pStream, int nMilliSec)
{
    if(!(TeChn>=0 && TeChn<TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    if(NULL == pStream)
    {
        aloge("fatal error! pStream == NULL!");
        return ERR_TENC_NULL_PTR;
    }
    if(nMilliSec < -1)
    {
        aloge("fatal error! illegal nMilliSec[%d]!", nMilliSec);
        return ERR_TENC_ILLEGAL_PARAM;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_TENC_NOT_PERM;
    }
    TEncStream stTencStream;
    memset(&stTencStream,0,sizeof(stTencStream));
    
    stTencStream.pStream = pStream;
    stTencStream.nMilliSec = nMilliSec;
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorTencGetStream, (void*)&stTencStream);
    return ret;
}

ERRORTYPE AW_MPI_TENC_ReleaseStream(TENC_CHN TeChn, TEXT_STREAM_S *pStream)
{
    if(!(TeChn>=0 && TeChn<TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    if(NULL == pStream)
    {
        aloge("fatal error! pStream == NULL!");
        return ERR_TENC_NULL_PTR;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_TENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorTencReleaseStream, (void*)pStream);
    return ret;
}

int AW_MPI_TENC_GetHandle(TENC_CHN TeChn)
{
    if(!(TeChn>=0 && TeChn <TENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid TeChn[%d]!", TeChn);
        return ERR_TENC_INVALID_CHNID;
    }
    TENC_CHN_MAP_S *pChn = NULL;
    if(SUCCESS != searchExistChannel(TeChn, &pChn))
    {
        return ERR_TENC_UNEXIST;
    }
    ERRORTYPE ret;
    int readFd = -1;
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorMPPChannelFd, (void*)&readFd);
    if(ret == SUCCESS)
    {
        return readFd;
    }
    else
    {
        return -1;
    }
}




