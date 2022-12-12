
//#define LOG_NDEBUG 0
#define LOG_TAG "mpi_vo"
#include <utils/plat_log.h>

//#include <cutils/log.h>
//#include <cutils/properties.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "hwdisplay.h"
#include <mm_component.h>
#include <tsemaphore.h>
//#include "..\..\middleware\include\media\mm_comm_vo.h"
#include "mm_comm_vo.h"
//#include "common/app_log.h"
#include <cdx_list.h>

typedef struct VO_CHN_MAP_S
{
    VO_LAYER            mVoLayer;   //video layer which video channel connect.
    VO_CHN              mVoChn;     // video output channel index, [0, VO_MAX_CHN_NUM), output to video layer. Our chip: one channel => one layer.
    MM_COMPONENTTYPE    *mComp;  // video render component instance
    cdx_sem_t           mSemCompCmd;
    MPPCallbackInfo     mCallbackInfo;
    struct list_head    mList;
}VO_CHN_MAP_S;
typedef struct VOChnManager
{
    struct list_head mList;   //element type: VO_CHN_MAP_S
    pthread_mutex_t mLock;
} VOChnManager;
static VOChnManager *gpVOChnManager = NULL;
static ERRORTYPE searchExistChannel_l(VO_LAYER VoLayer, VO_CHN VoChn, VO_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    VO_CHN_MAP_S* pEntry;

    if (gpVOChnManager == NULL)
    {
        return FAILURE;
    }

    list_for_each_entry(pEntry, &gpVOChnManager->mList, mList)
    {
        if(pEntry->mVoChn == VoChn && pEntry->mVoLayer == VoLayer)
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

static ERRORTYPE searchExistChannel(VO_LAYER VoLayer, VO_CHN VoChn, VO_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    VO_CHN_MAP_S* pEntry;

    if (gpVOChnManager == NULL) 
    {
        return FAILURE;
    }

    pthread_mutex_lock(&gpVOChnManager->mLock);
    ret = searchExistChannel_l(VoLayer, VoChn, ppChn);
    pthread_mutex_unlock(&gpVOChnManager->mLock);
    return ret;
}

static ERRORTYPE addChannel_l(VO_CHN_MAP_S *pChn)
{
    if (gpVOChnManager == NULL)
    {
        return FAILURE;
    }
    list_add_tail(&pChn->mList, &gpVOChnManager->mList);
    return SUCCESS;
}

static ERRORTYPE addChannel(VO_CHN_MAP_S *pChn)
{
    if (gpVOChnManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVOChnManager->mLock);
    ERRORTYPE ret = addChannel_l(pChn);
    pthread_mutex_unlock(&gpVOChnManager->mLock);
    return ret;
}

static ERRORTYPE removeChannel(VO_CHN_MAP_S *pChn)
{
    if (gpVOChnManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVOChnManager->mLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gpVOChnManager->mLock);
    return SUCCESS;
}

static VO_CHN_MAP_S* VO_CHN_MAP_S_Construct()
{
    VO_CHN_MAP_S *pChannel = (VO_CHN_MAP_S*)malloc(sizeof(VO_CHN_MAP_S));
    if(NULL == pChannel)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChannel, 0, sizeof(VO_CHN_MAP_S));
    cdx_sem_init(&pChannel->mSemCompCmd, 0);
    return pChannel;
}

static void VO_CHN_MAP_S_Destruct(VO_CHN_MAP_S *pChannel)
{
    if(pChannel->mComp)
    {
        aloge("fatal error! VO component need free before!");
        COMP_FreeHandle(pChannel->mComp);
        pChannel->mComp = NULL;
    }
    cdx_sem_deinit(&pChannel->mSemCompCmd);
    free(pChannel);
}

typedef struct VOLayerInfo
{
    VO_LAYER            mVoLayer;   //video layer which has been enabled.
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    unsigned int        mPriority;  //same to z-order
    VO_VIDEO_LAYER_ALPHA_S mAlpha;
    struct list_head    mList;
}VOLayerInfo;
typedef struct VOLayerManager
{
    struct list_head mVOLayerList;   //element type: VOLayerInfo
    pthread_mutex_t mLock;
} VOLayerManager;
static VOLayerManager *gpVOLayerManager = NULL;
static ERRORTYPE searchExistVOLayerInfo(VO_LAYER VoLayer, VOLayerInfo** ppVOLayerInfo)
{
    ERRORTYPE ret = FAILURE;
    VOLayerInfo* pEntry;
    if (gpVOLayerManager == NULL) 
    {
        return FAILURE;
    }

    pthread_mutex_lock(&gpVOLayerManager->mLock);
    list_for_each_entry(pEntry, &gpVOLayerManager->mVOLayerList, mList)
    {
        if(pEntry->mVoLayer == VoLayer)
        {
            if(ppVOLayerInfo)
            {
                *ppVOLayerInfo = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&gpVOLayerManager->mLock);
    return ret;
}
static ERRORTYPE addVOLayerInfo(VOLayerInfo *pVOLayerInfo)
{
    if (gpVOLayerManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVOLayerManager->mLock);
    list_add_tail(&pVOLayerInfo->mList, &gpVOLayerManager->mVOLayerList);
    pthread_mutex_unlock(&gpVOLayerManager->mLock);
    return SUCCESS;
}
static ERRORTYPE removeVOLayerInfo(VOLayerInfo *pVOLayerInfo)
{
    if (gpVOLayerManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVOLayerManager->mLock);
    list_del(&pVOLayerInfo->mList);
    pthread_mutex_unlock(&gpVOLayerManager->mLock);
    return SUCCESS;
}
static VOLayerInfo* VOLayerInfo_Construct()
{
    VOLayerInfo *pVOLayerInfo = (VOLayerInfo*)malloc(sizeof(VOLayerInfo));
    if(NULL == pVOLayerInfo)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pVOLayerInfo, 0, sizeof(VOLayerInfo));
    return pVOLayerInfo;
}

static void VOLayerInfo_Destruct(VOLayerInfo *pVOLayerInfo)
{
    free(pVOLayerInfo);
}

typedef struct VODevInfo
{
    VO_DEV mVoDev;
    VO_PUB_ATTR_S mVoPubAttr;
    struct list_head mList;
} VODevInfo;
typedef struct VODevManager
{
    struct list_head mVODevList;   //element type: VODevInfo
    pthread_mutex_t mLock;
} VODevManager;
static VODevManager *gpVODevManager = NULL;
static ERRORTYPE searchExistVODevInfo(VO_DEV VoDev, VODevInfo** ppVODevInfo)
{
    ERRORTYPE ret = FAILURE;
    VODevInfo* pEntry;
    if (gpVODevManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVODevManager->mLock);
    list_for_each_entry(pEntry, &gpVODevManager->mVODevList, mList)
    {
        if(pEntry->mVoDev == VoDev)
        {
            if(ppVODevInfo)
            {
                *ppVODevInfo = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&gpVODevManager->mLock);
    return ret;
}
static ERRORTYPE addVODevInfo(VODevInfo *pVODevInfo)
{
    if (gpVODevManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVODevManager->mLock);
    list_add_tail(&pVODevInfo->mList, &gpVODevManager->mVODevList);
    pthread_mutex_unlock(&gpVODevManager->mLock);
    return SUCCESS;
}
static ERRORTYPE removeVODevInfo(VODevInfo *pVODevInfo)
{
    if (gpVODevManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVODevManager->mLock);
    list_del(&pVODevInfo->mList);
    pthread_mutex_unlock(&gpVODevManager->mLock);
    return SUCCESS;
}
static VODevInfo* VODevInfo_Construct()
{
    VODevInfo *pVODevInfo = (VODevInfo*)malloc(sizeof(VODevInfo));
    if(NULL == pVODevInfo)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pVODevInfo, 0, sizeof(VODevInfo));
    return pVODevInfo;
}

static void VODevInfo_Destruct(VODevInfo *pVODevInfo)
{
    free(pVODevInfo);
}

ERRORTYPE VO_Construct(void)
{
    ERRORTYPE eError = SUCCESS;
    int ret;
    if (gpVODevManager != NULL) 
    {
        alogd("already construct vo");
        return SUCCESS;
    }
    gpVODevManager = (VODevManager*)malloc(sizeof(VODevManager));
    if (NULL == gpVODevManager)
    {
        aloge("alloc VODevManager error(%s)!", strerror(errno));
        return FAILURE;
    }
    ret = pthread_mutex_init(&gpVODevManager->mLock, NULL);
    if (ret != 0) 
    {
        aloge("fatal error! mutex init fail");
    }
    INIT_LIST_HEAD(&gpVODevManager->mVODevList);

    gpVOLayerManager = (VOLayerManager*)malloc(sizeof(VOLayerManager));
    if (NULL == gpVOLayerManager)
    {
        aloge("alloc VOLayerManager error(%s)!", strerror(errno));
        eError = ERR_VO_NO_MEM;
        goto VOLayer_fail;
    }
    ret = pthread_mutex_init(&gpVOLayerManager->mLock, NULL);
    if (ret != 0) 
    {
        aloge("fatal error! mutex init fail");
    }
    INIT_LIST_HEAD(&gpVOLayerManager->mVOLayerList);

    gpVOChnManager = (VOChnManager*)malloc(sizeof(VOChnManager));
    if (NULL == gpVOChnManager)
    {
        aloge("alloc VOChnManager error(%s)!", strerror(errno));
        eError = ERR_VO_NO_MEM;
        goto VOChn_fail;
    }
    ret = pthread_mutex_init(&gpVOChnManager->mLock, NULL);
    if (ret != 0) 
    {
        aloge("fatal error! mutex init fail");
    }
    INIT_LIST_HEAD(&gpVOChnManager->mList);

    if(hw_display_init()!=0)
    {
        aloge("fatal error! hw display init fail!");
        eError = ERR_VO_SYS_NOTREADY;
        goto hw_display_init_fail;
    }
    return SUCCESS;

hw_display_init_fail:
    pthread_mutex_destroy(&gpVOChnManager->mLock);
    free(gpVOChnManager);
    gpVOChnManager = NULL;
VOChn_fail:
    pthread_mutex_destroy(&gpVOLayerManager->mLock);
    free(gpVOLayerManager);
    gpVOLayerManager = NULL;
VOLayer_fail:
    pthread_mutex_destroy(&gpVODevManager->mLock);
    free(gpVODevManager);
    gpVODevManager = NULL;
    return eError;

}

ERRORTYPE VO_Destruct(void)
{
    if(NULL == gpVODevManager)
    {
        alogd("already destruct vo");
        return SUCCESS;
    }
    if (gpVOChnManager != NULL) 
    {
        if (!list_empty(&gpVOChnManager->mList)) 
        {
            aloge("fatal error! some vo channel still running when destroy vo device!");
        }
        pthread_mutex_destroy(&gpVOChnManager->mLock);
        free(gpVOChnManager);
        gpVOChnManager = NULL;
    }
    if (gpVOLayerManager != NULL) 
    {
        if (!list_empty(&gpVOLayerManager->mVOLayerList)) 
        {
            aloge("fatal error! some vo layer still running when disable vo!");
            VOLayerInfo *pEntry;
            list_for_each_entry(pEntry, &gpVOLayerManager->mVOLayerList, mList)
            {
                alogd("VOLayerInfo: layerId[%d], dispRect[%d,%d,%d,%d], Priority[%d], alphaMode[%d], alphaValue[%d]", 
                    pEntry->mVoLayer, 
                    pEntry->mLayerAttr.stDispRect.X, pEntry->mLayerAttr.stDispRect.Y,
                    pEntry->mLayerAttr.stDispRect.Width, pEntry->mLayerAttr.stDispRect.Height,
                    pEntry->mPriority, pEntry->mAlpha.mAlphaMode, pEntry->mAlpha.mAlphaValue);
            }
        }
        pthread_mutex_destroy(&gpVOLayerManager->mLock);
        free(gpVOLayerManager);
        gpVOLayerManager = NULL;
    }
    if (gpVODevManager != NULL) 
    {
        if (!list_empty(&gpVODevManager->mVODevList)) 
        {
            aloge("fatal error! some vo dev still running when disable vo!");
        }
        pthread_mutex_destroy(&gpVODevManager->mLock);
        free(gpVODevManager);
        gpVODevManager = NULL;
    }
    hw_display_deinit();
    return SUCCESS;
}

MM_COMPONENTTYPE *VO_GetChnComp(MPP_CHN_S *pMppChn)
{
    VO_CHN_MAP_S* pChn;

    if (searchExistChannel(pMppChn->mDevId, pMppChn->mChnId, &pChn) != SUCCESS)
    {
        return NULL;
    }
    return pChn->mComp;
}

static ERRORTYPE VideoRenderEventHandler(
	 PARAM_IN COMP_HANDLETYPE hComponent,
	 PARAM_IN void* pAppData,
	 PARAM_IN COMP_EVENTTYPE eEvent,
	 PARAM_IN unsigned int nData1,
	 PARAM_IN unsigned int nData2,
	 PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
//    MPP_CHN_S VOChnInfo;
//    ret = COMP_GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &VOChnInfo);
//    if(ret == SUCCESS)
//    {
//        alogv("video render event, MppChannel[%d][%d][%d]", VOChnInfo.mModId, VOChnInfo.mDevId, VOChnInfo.mChnId);
//    }
	VO_CHN_MAP_S *pChn = (VO_CHN_MAP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("vo EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(COMP_CommandVendorChangeANativeWindow == nData1)
            {
                alogd("change video layer?");
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
            if(ERR_VO_CHN_SAMESTATE == nData1)
            {
                alogv("set same state to vo!");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_VO_CHN_INVALIDSTATE == nData1)
            {
                aloge("why vo state turn to invalid?");
                break;
            }
            else if(ERR_VO_CHN_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! vo state transition incorrect.");
                break;
            }
            else
            {
                aloge("fatal error! unknown error[0x%x]!", nData1);
                break;
            }
        }
        case COMP_EventBufferFlag:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_VOU;
            ChannelInfo.mDevId = pChn->mVoLayer;
            ChannelInfo.mChnId = pChn->mVoChn;
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_NOTIFY_EOF, NULL);
            break;
        }
        case COMP_EventKeyFrameDecoded:
        {
            alogd("KeyFrameDecoded, pts[%lld]us", *(int64_t*)pEventData);
            break;
        }
        case COMP_EventVideoDisplaySize:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_VOU;
            ChannelInfo.mDevId = pChn->mVoLayer;
            ChannelInfo.mChnId = pChn->mVoChn;
            SIZE_S displaySize;
            displaySize.Width = nData1;
            displaySize.Height = nData2;
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_SET_VIDEO_SIZE, &displaySize);
            break;
        }
        case COMP_EventRenderingStart:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_VOU;
            ChannelInfo.mDevId = pChn->mVoLayer;
            ChannelInfo.mChnId = pChn->mVoChn;
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_RENDERING_START, NULL);
            break;
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
	return SUCCESS;
}

static ERRORTYPE VideoRenderEmptyBufferDone(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN void* pAppData,
        PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    VO_CHN_MAP_S *pChn = (VO_CHN_MAP_S*)pAppData;
    VIDEO_FRAME_INFO_S *pFrameInfo =  (VIDEO_FRAME_INFO_S*)pBuffer->pAppPrivate;
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_VOU;
    ChannelInfo.mDevId = pChn->mVoLayer;
    ChannelInfo.mChnId = pChn->mVoChn;
    CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
    pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_RELEASE_VIDEO_BUFFER, (void*)pFrameInfo);
    return SUCCESS;
}


COMP_CALLBACKTYPE VideoRenderCallback = {
	.EventHandler = VideoRenderEventHandler,
	.EmptyBufferDone = VideoRenderEmptyBufferDone,
	.FillBufferDone = NULL,
};

VO_INTF_TYPE_E map_disp_output_type_to_VO_INTF_TYPE_E(disp_output_type disp_type)
{
	VO_INTF_TYPE_E eIntfType = 0;
    if(disp_type&DISP_OUTPUT_TYPE_LCD)
    {
        eIntfType |= VO_INTF_LCD;
    }
    if(disp_type&DISP_OUTPUT_TYPE_TV)
    {
        eIntfType |= VO_INTF_CVBS;
    }
    if(disp_type&DISP_OUTPUT_TYPE_HDMI)
    {
        eIntfType |= VO_INTF_HDMI;
    }
    if(disp_type&DISP_OUTPUT_TYPE_VGA)
    {
        eIntfType |= VO_INTF_VGA;
    }
//    if(DISP_OUTPUT_TYPE_NONE == disp_type)
//    {
//        eIntfType |= VO_INTF_LCD;
//    }
    return eIntfType;
}

disp_output_type map_VO_INTF_TYPE_E_to_disp_output_type(VO_INTF_TYPE_E eIntfType)
{
	disp_output_type disp_type = 0;
    if(eIntfType&VO_INTF_LCD)
    {
        disp_type |= DISP_OUTPUT_TYPE_LCD;
    }
    if(eIntfType&VO_INTF_CVBS)
    {
        disp_type |= DISP_OUTPUT_TYPE_TV;
    }
    if(eIntfType&VO_INTF_HDMI)
    {
        disp_type |= DISP_OUTPUT_TYPE_HDMI;
    }
    if(eIntfType&VO_INTF_VGA)
    {
        disp_type |= DISP_OUTPUT_TYPE_VGA;
    }
    if(0 == eIntfType)
    {
        alogw("be careful! unknown intf_type:0x%x, return disp_output_type:0x%x", eIntfType, disp_type);
    }
    return disp_type;
}

VO_INTF_SYNC_E map_disp_tv_mode_to_VO_INTF_SYNC_E(disp_tv_mode tv_mode)
{
    VO_INTF_SYNC_E eIntfSync;
    switch(tv_mode) 
    {
        case DISP_TV_MOD_720P_50HZ:
            eIntfSync = VO_OUTPUT_720P50;
            break;
        case DISP_TV_MOD_720P_60HZ:
            eIntfSync = VO_OUTPUT_720P60;
            break;
        case DISP_TV_MOD_1080I_50HZ:
            eIntfSync = VO_OUTPUT_1080I50;
            break;
        case DISP_TV_MOD_1080I_60HZ:
            eIntfSync = VO_OUTPUT_1080I60;
            break;
        case DISP_TV_MOD_1080P_24HZ:
            eIntfSync = VO_OUTPUT_1080P24;
            break;
        case DISP_TV_MOD_1080P_50HZ:
            eIntfSync = VO_OUTPUT_1080P50;
            break;
        case DISP_TV_MOD_1080P_60HZ:
            eIntfSync = VO_OUTPUT_1080P60;
            break;
        case DISP_TV_MOD_1080P_25HZ:
            eIntfSync = VO_OUTPUT_1080P25;
            break;
        case DISP_TV_MOD_1080P_30HZ:
            eIntfSync = VO_OUTPUT_1080P30;
            break;
        case DISP_TV_MOD_576I:
            eIntfSync = VO_OUTPUT_PAL;
            break;
        case DISP_TV_MOD_480I:
            eIntfSync = VO_OUTPUT_NTSC;
            break;
        case DISP_TV_MOD_3840_2160P_30HZ:
            eIntfSync = VO_OUTPUT_3840x2160_30;
            break;
        case DISP_TV_MOD_3840_2160P_25HZ:
            eIntfSync = VO_OUTPUT_3840x2160_25;
            break;
        case DISP_TV_MOD_3840_2160P_24HZ:
            eIntfSync = VO_OUTPUT_3840x2160_24;
            break;
        case DISP_TV_MOD_480P:
        case DISP_TV_MOD_576P:
    	case DISP_TV_MOD_1080P_24HZ_3D_FP:
    	case DISP_TV_MOD_720P_50HZ_3D_FP:
    	case DISP_TV_MOD_720P_60HZ_3D_FP:
        case DISP_TV_MOD_PAL_SVIDEO:
    	case DISP_TV_MOD_NTSC_SVIDEO:
    	case DISP_TV_MOD_PAL_M:
    	case DISP_TV_MOD_PAL_M_SVIDEO:
    	case DISP_TV_MOD_PAL_NC:
    	case DISP_TV_MOD_PAL_NC_SVIDEO:
        default:
            aloge("fatal error! Unknown tv_mode 0x%x", tv_mode);
            eIntfSync = VO_OUTPUT_BUTT;
            break;
    }
    return eIntfSync;
}

disp_tv_mode map_VO_INTF_SYNC_E_to_disp_tv_mode(VO_INTF_SYNC_E eIntfSync)
{
    disp_tv_mode tv_mode;
    switch(eIntfSync)
    {
        case VO_OUTPUT_PAL:
            tv_mode = DISP_TV_MOD_576I;
            break;
        case VO_OUTPUT_NTSC:
            tv_mode = DISP_TV_MOD_480I;
            break;
        case VO_OUTPUT_1080P24:
            tv_mode = DISP_TV_MOD_1080P_24HZ;
            break;
        case VO_OUTPUT_1080P25:
            tv_mode = DISP_TV_MOD_1080P_25HZ;
            break;
        case VO_OUTPUT_1080P30:
            tv_mode = DISP_TV_MOD_1080P_30HZ;
            break;
        case VO_OUTPUT_720P50:
            tv_mode = DISP_TV_MOD_720P_50HZ;
            break;
        case VO_OUTPUT_720P60:
            tv_mode = DISP_TV_MOD_720P_60HZ;
            break;
        case VO_OUTPUT_1080I50:
            tv_mode = DISP_TV_MOD_1080I_50HZ;
            break;
        case VO_OUTPUT_1080I60:
            tv_mode = DISP_TV_MOD_1080I_60HZ;
            break;
        case VO_OUTPUT_1080P50:
            tv_mode = DISP_TV_MOD_1080P_50HZ;
            break;
        case VO_OUTPUT_1080P60:
            tv_mode = DISP_TV_MOD_1080P_60HZ;
            break;
        case VO_OUTPUT_3840x2160_24:
            tv_mode = DISP_TV_MOD_3840_2160P_24HZ;
            break;
        case VO_OUTPUT_3840x2160_25:
            tv_mode = DISP_TV_MOD_3840_2160P_25HZ;
            break;
        case VO_OUTPUT_3840x2160_30:
            tv_mode = DISP_TV_MOD_3840_2160P_30HZ;
            break;
        case VO_OUTPUT_576P50:
        case VO_OUTPUT_480P60:
        case VO_OUTPUT_800x600_60:
        case VO_OUTPUT_1024x768_60:
        case VO_OUTPUT_1280x1024_60:
        case VO_OUTPUT_1366x768_60:
        case VO_OUTPUT_1440x900_60:
        case VO_OUTPUT_1280x800_60:
        case VO_OUTPUT_1600x1200_60:
        case VO_OUTPUT_1680x1050_60:
        case VO_OUTPUT_1920x1200_60:
        case VO_OUTPUT_640x480_60:
        case VO_OUTPUT_960H_PAL:
        case VO_OUTPUT_960H_NTSC:
        case VO_OUTPUT_320X240_30:
        case VO_OUTPUT_320X240_50:
        case VO_OUTPUT_240X320_50:
        case VO_OUTPUT_240X320_60:
        default:
            aloge("fatal error! Unknown vo interface sync 0x%x", eIntfSync);
            tv_mode = DISP_TV_MOD_576I;
            break;
    }
    return tv_mode;
}

ERRORTYPE AW_MPI_VO_Enable(VO_DEV VoDev)
{
    ERRORTYPE eError = SUCCESS;
    int ret;
    if (NULL == gpVODevManager) 
    {
        aloge("fatal error! VODevManager is not init!");
        return ERR_VO_SYS_NOTREADY;
    }
    if(SUCCESS == searchExistVODevInfo(VoDev, NULL))
    {
        return ERR_VO_DEV_HAS_ENABLED;
    }
    VODevInfo *pVODevInfo = VODevInfo_Construct();
    pVODevInfo->mVoDev = VoDev;
    disp_output_type disp_type;
    disp_tv_mode tv_mode;
    if(0 == hwd_get_disp_type((int*)&disp_type, (int*)&tv_mode))
    {
        pVODevInfo->mVoPubAttr.enIntfType = map_disp_output_type_to_VO_INTF_TYPE_E(disp_type);
        pVODevInfo->mVoPubAttr.enIntfSync = map_disp_tv_mode_to_VO_INTF_SYNC_E(tv_mode);
    }
    else
    {
        aloge("fatal error! get disp type fail!");
        eError = ERR_VO_NOT_SUPPORT;
        goto get_disp_type_fail;
    }
    addVODevInfo(pVODevInfo);
    return eError;

get_disp_type_fail:
    VODevInfo_Destruct(pVODevInfo);
    pVODevInfo = NULL;
    return eError;
}

ERRORTYPE AW_MPI_VO_Disable(VO_DEV VoDev)
{
    ERRORTYPE eError = SUCCESS;
    VODevInfo *pVODevInfo = NULL;
    if(SUCCESS != searchExistVODevInfo(VoDev, &pVODevInfo))
    {
        return ERR_VO_DEV_NOT_ENABLE;
    }
    removeVODevInfo(pVODevInfo);
    VODevInfo_Destruct(pVODevInfo);
    return eError;
}

ERRORTYPE AW_MPI_VO_SetPubAttr(VO_DEV VoDev, const VO_PUB_ATTR_S *pstPubAttr)
{
    ERRORTYPE eError = SUCCESS;
    VODevInfo *pVODevInfo = NULL;
    if(SUCCESS != searchExistVODevInfo(VoDev, &pVODevInfo))
    {
        return ERR_VO_DEV_NOT_ENABLE;
    }
    BOOL bUpdateFlag = FALSE;
    VO_PUB_ATTR_S *pCurPubAttr = &pVODevInfo->mVoPubAttr;
    if(pCurPubAttr->enIntfType != pstPubAttr->enIntfType)
    {
        bUpdateFlag = TRUE;
    }
    else
    {
        if(pCurPubAttr->enIntfType & VO_INTF_LCD)
        {
            bUpdateFlag = FALSE;
        }
        else if(pCurPubAttr->enIntfSync != pstPubAttr->enIntfSync)
        {
            bUpdateFlag = TRUE;
        }
        else
        {
            bUpdateFlag = FALSE;
        }
    }
    if(bUpdateFlag)
    {
        alogd("vo interface changed, [0x%x, 0x%x]->[0x%x, 0x%x]", 
            pCurPubAttr->enIntfType, pCurPubAttr->enIntfSync, pstPubAttr->enIntfType, pstPubAttr->enIntfSync);
        disp_output_type disp_type = map_VO_INTF_TYPE_E_to_disp_output_type(pstPubAttr->enIntfType);
        disp_tv_mode tv_mode = map_VO_INTF_SYNC_E_to_disp_tv_mode(pstPubAttr->enIntfSync);
        int ret = hwd_switch_vo_device(disp_type, tv_mode);
        if(0 == ret)
        {
            *pCurPubAttr = *pstPubAttr;
        }
        else
        {
            aloge("fatal error! hwd switch_vo_device error! ret:%d", ret);
            eError = ERR_VO_NOT_SUPPORT;
        }
    }
    return eError;
}

ERRORTYPE AW_MPI_VO_GetPubAttr(VO_DEV VoDev, VO_PUB_ATTR_S *pstPubAttr)
{
    ERRORTYPE eError = SUCCESS;
    VODevInfo *pVODevInfo = NULL;
    if(SUCCESS != searchExistVODevInfo(VoDev, &pVODevInfo))
    {
        return ERR_VO_DEV_NOT_ENABLE;
    }
    *pstPubAttr = pVODevInfo->mVoPubAttr;
    return eError;
}

ERRORTYPE AW_MPI_VO_GetHdmiHwMode(VO_DEV VoDev, VO_INTF_SYNC_E *mode)
{
    ERRORTYPE eError = SUCCESS;
    VODevInfo *pVODevInfo = NULL;
    if(SUCCESS != searchExistVODevInfo(VoDev, &pVODevInfo))
    {
        return ERR_VO_DEV_NOT_ENABLE;
    }

    disp_tv_mode disp_mode;
    eError = hwd_get_hdmi_hw_mode(&disp_mode);
    if (eError == -1) {
        aloge("there has no hdmi device!!\n");
        return ERR_VO_DEV_NOT_CONFIG;
    } else if (eError == 1) {
        aloge("there has no supported display mode for this hdmi device!!\n");
        return ERR_VO_NOT_SUPPORT;
    } else if (eError == SUCCESS) {
        *mode = map_disp_tv_mode_to_VO_INTF_SYNC_E(disp_mode);
    }

    return SUCCESS;
}

ERRORTYPE AW_MPI_VO_EnableVideoLayer(VO_LAYER VoLayer)
{
    ERRORTYPE eError = SUCCESS;
    if(SUCCESS == searchExistVOLayerInfo(VoLayer, NULL))
    {
        return ERR_VO_VIDEO_NOT_DISABLE;
    }
    int ret = hwd_layer_request_hlay(VoLayer);
    if(0 == ret)
    {
        VOLayerInfo *pVOLayerInfo = VOLayerInfo_Construct();
        pVOLayerInfo->mVoLayer = VoLayer;
        disp_layer_config config;
        memset(&config, 0, sizeof(disp_layer_config));
        config.channel  = HD2CHN(VoLayer);
        config.layer_id = HD2LYL(VoLayer);
        layer_get_para(&config);
        pVOLayerInfo->mLayerAttr.stDispRect.X = config.info.screen_win.x;
        pVOLayerInfo->mLayerAttr.stDispRect.Y = config.info.screen_win.y;
        pVOLayerInfo->mLayerAttr.stDispRect.Width = config.info.screen_win.width;
        pVOLayerInfo->mLayerAttr.stDispRect.Height = config.info.screen_win.height;
        pVOLayerInfo->mPriority = config.info.zorder;
        pVOLayerInfo->mAlpha.mAlphaMode = config.info.alpha_mode;
        pVOLayerInfo->mAlpha.mAlphaValue = config.info.alpha_value;
        addVOLayerInfo(pVOLayerInfo);
    }
    else
    {
        aloge("fatal error! why request hlay[%d] fail?", VoLayer);
        eError = ERR_VO_DEV_NOT_ENABLE;
    }
    return eError;
}

ERRORTYPE AW_MPI_VO_DisableVideoLayer(VO_LAYER VoLayer)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    int ret = hwd_layer_release(VoLayer);
    if(0 == ret)
    {
        removeVOLayerInfo(pVOLayerInfo);
        VOLayerInfo_Destruct(pVOLayerInfo);
    }
    else
    {
        eError = ERR_VO_BUSY;
    }
    return eError;
}

ERRORTYPE AW_MPI_VO_AddOutsideVideoLayer(VO_LAYER VoLayer)
{
    ERRORTYPE eError = SUCCESS;
    if(SUCCESS == searchExistVOLayerInfo(VoLayer, NULL))
    {
        return ERR_VO_VIDEO_NOT_DISABLE;
    }
    VOLayerInfo *pVOLayerInfo = VOLayerInfo_Construct();
    pVOLayerInfo->mVoLayer = VoLayer;
    disp_layer_config config;
    memset(&config, 0, sizeof(disp_layer_config));
    config.channel  = HD2CHN(VoLayer);
    config.layer_id = HD2LYL(VoLayer);
    int getParaRet = layer_get_para(&config);
    if(RET_OK == getParaRet)
    {
        pVOLayerInfo->mLayerAttr.stDispRect.X = config.info.screen_win.x;
        pVOLayerInfo->mLayerAttr.stDispRect.Y = config.info.screen_win.y;
        pVOLayerInfo->mLayerAttr.stDispRect.Width = config.info.screen_win.width;
        pVOLayerInfo->mLayerAttr.stDispRect.Height = config.info.screen_win.height;
        pVOLayerInfo->mPriority = config.info.zorder;
        pVOLayerInfo->mAlpha.mAlphaMode = config.info.alpha_mode;
        pVOLayerInfo->mAlpha.mAlphaValue = config.info.alpha_value;
    }
    else
    {
        alogd("fatal error! why hlay[%d] is not request?", VoLayer);
    }
    addVOLayerInfo(pVOLayerInfo);
    return eError;
}
ERRORTYPE AW_MPI_VO_RemoveOutsideVideoLayer(VO_LAYER VoLayer)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    removeVOLayerInfo(pVOLayerInfo);
    VOLayerInfo_Destruct(pVOLayerInfo);
    return eError;
}

ERRORTYPE AW_MPI_VO_OpenVideoLayer (VO_LAYER VoLayer)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    int ret = hwd_layer_open(VoLayer);
    if(ret != RET_OK)
    {
        eError = ERR_VO_NOT_SUPPORT;
    }
    return eError;
}
ERRORTYPE AW_MPI_VO_CloseVideoLayer(VO_LAYER VoLayer)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    int ret = hwd_layer_close(VoLayer);
    if(ret != RET_OK)
    {
        eError = ERR_VO_NOT_SUPPORT;
    }
    return eError;
}

ERRORTYPE AW_MPI_VO_SetVideoLayerAttr(VO_LAYER VoLayer, const VO_VIDEO_LAYER_ATTR_S *pstLayerAttr)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    VO_VIDEO_LAYER_ATTR_S *pCurAttr = &pVOLayerInfo->mLayerAttr;
    if(pCurAttr->stDispRect.X != pstLayerAttr->stDispRect.X
        || pCurAttr->stDispRect.Y != pstLayerAttr->stDispRect.Y
        || pCurAttr->stDispRect.Width != pstLayerAttr->stDispRect.Width
        || pCurAttr->stDispRect.Height != pstLayerAttr->stDispRect.Height)
    {
        int channel  = HD2CHN(VoLayer);
        int layer_id = HD2LYL(VoLayer);
        alogd("ch[%d]lyl[%d]:dispRect changed, [%d, %d, %dx%d]->[%d, %d, %dx%d]", channel, layer_id,
            pCurAttr->stDispRect.X, pCurAttr->stDispRect.Y, pCurAttr->stDispRect.Width, pCurAttr->stDispRect.Height,
            pstLayerAttr->stDispRect.X, pstLayerAttr->stDispRect.Y, pstLayerAttr->stDispRect.Width, pstLayerAttr->stDispRect.Height);
        struct view_info dispRect;
        dispRect.x = pstLayerAttr->stDispRect.X;
        dispRect.y = pstLayerAttr->stDispRect.Y;
        dispRect.w = pstLayerAttr->stDispRect.Width;
        dispRect.h = pstLayerAttr->stDispRect.Height;
        int ret = hwd_layer_set_rect(VoLayer, &dispRect);
        if(RET_OK == ret)
        {
            pCurAttr->stDispRect = pstLayerAttr->stDispRect;
        }
        else
        {
            eError = ERR_VO_NOT_SUPPORT;
        }
    }

    return eError;
}

ERRORTYPE AW_MPI_VO_GetVideoLayerAttr(VO_LAYER VoLayer, VO_VIDEO_LAYER_ATTR_S *pstLayerAttr)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    *pstLayerAttr = pVOLayerInfo->mLayerAttr;
    return eError;
}

ERRORTYPE AW_MPI_VO_SetVideoLayerPriority(VO_LAYER VoLayer, unsigned int uPriority)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(uPriority<ZORDER_MIN || uPriority > ZORDER_MAX)
    {
        alogw("video layer priority[%d] is invalid, valid scope[%d, %d]", uPriority, ZORDER_MIN, ZORDER_MAX);
        return ERR_VO_ILLEGAL_PARAM;
    }
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    pthread_mutex_lock(&gpVOLayerManager->mLock);
    if(pVOLayerInfo->mPriority != uPriority)
    {
        alogd("layer[%d] priority changed [%d]->[%d]", pVOLayerInfo->mVoLayer, pVOLayerInfo->mPriority, uPriority);
        VOLayerInfo* pEntry;
        BOOL bFindFlag = FALSE;
        list_for_each_entry(pEntry, &gpVOLayerManager->mVOLayerList, mList)
        {
            if(pEntry->mPriority == uPriority)
            {
                alogd("layer[%d] take priority[%d], need exchange", pEntry->mVoLayer, uPriority);
                bFindFlag = TRUE;
                break;
            }
        }
        if(bFindFlag)
        {
            hwd_layer_exchange_zorder(pVOLayerInfo->mVoLayer, pEntry->mVoLayer);
            unsigned int tmp = pEntry->mPriority;
            pEntry->mPriority = pVOLayerInfo->mPriority;
            pVOLayerInfo->mPriority = tmp;
        }
        else
        {
            hwd_layer_set_zorder(pVOLayerInfo->mVoLayer, uPriority);
            pVOLayerInfo->mPriority = uPriority;
        }
    }
    pthread_mutex_unlock(&gpVOLayerManager->mLock);
    return eError;
}

ERRORTYPE AW_MPI_VO_GetVideoLayerPriority(VO_LAYER VoLayer, unsigned int *puPriority)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    *puPriority = pVOLayerInfo->mPriority;
    return eError;
}

ERRORTYPE AW_MPI_VO_SetVideoLayerAlpha(VO_LAYER VoLayer, VO_VIDEO_LAYER_ALPHA_S *pAlpha)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    VO_VIDEO_LAYER_ALPHA_S *pCurAlpha = &pVOLayerInfo->mAlpha;
    if(pCurAlpha->mAlphaMode!=pAlpha->mAlphaMode || pCurAlpha->mAlphaValue!=pAlpha->mAlphaValue)
    {
        alogd("video layer alpha changed, [%d, %d]->[%d, %d]", pCurAlpha->mAlphaMode, pCurAlpha->mAlphaValue, pAlpha->mAlphaMode, pAlpha->mAlphaValue);
        disp_layer_config config;
        memset(&config, 0, sizeof(disp_layer_config));
        config.channel  = HD2CHN(VoLayer);
        config.layer_id = HD2LYL(VoLayer);
        layer_get_para(&config);
        config.info.alpha_mode = pAlpha->mAlphaMode;
        config.info.alpha_value = pAlpha->mAlphaValue;
        int ret = layer_set_para(&config);
        if(RET_OK == ret)
        {
            *pCurAlpha = *pAlpha;
        }
        else
        {
            eError = ERR_VO_NOT_SUPPORT;
        }
    }
    return eError;
}

ERRORTYPE AW_MPI_VO_GetVideoLayerAlpha(VO_LAYER VoLayer, VO_VIDEO_LAYER_ALPHA_S *pAlpha)
{
    ERRORTYPE eError = SUCCESS;
    VOLayerInfo *pVOLayerInfo = NULL;
    if(SUCCESS != searchExistVOLayerInfo(VoLayer, &pVOLayerInfo))
    {
        return ERR_VO_VIDEO_NOT_ENABLE;
    }
    *pAlpha = pVOLayerInfo->mAlpha;
    return eError;
}

ERRORTYPE AW_MPI_VO_CreateChn(VO_LAYER VoLayer, VO_CHN VoChn)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    pthread_mutex_lock(&gpVOChnManager->mLock);
    if(SUCCESS == searchExistChannel_l(VoLayer, VoChn, NULL))
    {
        pthread_mutex_unlock(&gpVOChnManager->mLock);
        return ERR_VO_CHN_NOT_DISABLE;
    }
    VO_CHN_MAP_S *pNode = VO_CHN_MAP_S_Construct();
    pNode->mVoLayer = VoLayer;
    pNode->mVoChn = VoChn;
    
    //create Venc Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mComp, CDX_ComponentNameVideoRender, (void*)pNode, &VideoRenderCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_VOU;
    ChannelInfo.mDevId = pNode->mVoLayer;
    ChannelInfo.mChnId = pNode->mVoChn;
    eRet = pNode->mComp->SetConfig(pNode->mComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    eRet = pNode->mComp->SetConfig(pNode->mComp, COMP_IndexVendorInitInstance, NULL);
    char avSyncFlag = 1;
    eRet = pNode->mComp->SetConfig(pNode->mComp, COMP_IndexVendorSetAVSync, &avSyncFlag);
    eRet = pNode->mComp->SendCommand(pNode->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    //create VO Component done!

    addChannel_l(pNode);
    pthread_mutex_unlock(&gpVOChnManager->mLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_VO_DestroyChn(VO_LAYER VoLayer, VO_CHN VoChn)
{
    ERRORTYPE ret;
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE eRet;
    if(pChn->mComp)
    {
        COMP_STATETYPE nCompState;
        if(SUCCESS == pChn->mComp->GetState(pChn->mComp, &nCompState))
        {
            if(nCompState == COMP_StateIdle)
            {
                eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
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
                aloge("fatal error! invalid VoChn[%d] state[0x%x]!", VoChn, nCompState);
                eRet = FAILURE;
            }

            if(eRet == SUCCESS)
            {
                removeChannel(pChn);
                COMP_FreeHandle(pChn->mComp);
                pChn->mComp = NULL;
                VO_CHN_MAP_S_Destruct(pChn);
                ret = SUCCESS;
            }
            else
            {
                ret = ERR_VO_BUSY;
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_VO_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no VO component!");
        list_del(&pChn->mList);
        VO_CHN_MAP_S_Destruct(pChn);
        ret = SUCCESS;
    }
    return ret;
}

/** 
 * Register callback, mpp_vo use callback to notify caller.
 *
 * @return SUCCESS.
 * @param VoLayer vo channel number.
 * @param VoChn vo channel number.
 * @param pCallback callback struct.
 */
ERRORTYPE AW_MPI_VO_RegisterCallback(VO_LAYER VoLayer, VO_CHN VoChn, MPPCallbackInfo *pCallback)
{
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_VO_SetChnAttr(VO_LAYER VoLayer, VO_CHN VoChn, const VO_CHN_ATTR_S *pstChnAttr)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorVOChnAttr, (void*)pstChnAttr);
    return ret;
}

ERRORTYPE AW_MPI_VO_GetChnAttr(VO_LAYER VoLayer, VO_CHN VoChn, VO_CHN_ATTR_S *pstChnAttr)
{
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVOChnAttr, (void*)pstChnAttr);
    return ret;
}

ERRORTYPE AW_MPI_VO_SetFrameDisplayRegion(VO_LAYER VoLayer, VO_CHN VoChn, const RECT_S *pRect)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorDisplayRegion, (void*)pRect);
    return ret;
}

ERRORTYPE AW_MPI_VO_GetFrameDisplayRegion(VO_LAYER VoLayer, VO_CHN VoChn, RECT_S *pRect)
{
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorDisplayRegion, (void*)pRect);
    return ret;
}

ERRORTYPE AW_MPI_VO_SetChnField(VO_LAYER VoLayer, VO_CHN VoChn, const VO_DISPLAY_FIELD_E enField)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorVOChnDisplayField, (void*)&enField);
    return ret;
}

ERRORTYPE AW_MPI_VO_GetChnField(VO_LAYER VoLayer, VO_CHN VoChn, VO_DISPLAY_FIELD_E *pField)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVOChnDisplayField, (void*)pField);
    return ret;
}

ERRORTYPE AW_MPI_VO_SetChnDispBufNum(VO_LAYER VoLayer, VO_CHN VoChn, unsigned int uBufNum)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorVODispBufNum, (void*)&uBufNum);
    return ret;
}

ERRORTYPE AW_MPI_VO_GetChnDispBufNum(VO_LAYER VoLayer, VO_CHN VoChn, unsigned int *puBufNum)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVODispBufNum, (void*)puBufNum);
    return ret;
}

ERRORTYPE AW_MPI_VO_GetDisplaySize (VO_LAYER VoLayer, VO_CHN VoChn, SIZE_S *pDisplaySize)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState)
    {
        alogw("call in wrong state[0x%x], maybe display size is not right", nState);
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVOChnDisplaySize, (void*)pDisplaySize);
    return ret;
}

ERRORTYPE AW_MPI_VO_SetChnFrameRate(VO_LAYER VoLayer, VO_CHN VoChn, int nChnFrmRate)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorVOChnFrameRate, (void*)&nChnFrmRate);
    return ret;
}
ERRORTYPE AW_MPI_VO_GetChnFrameRate(VO_LAYER VoLayer, VO_CHN VoChn, int *pChnFrmRate)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetConfig(pChn->mComp, COMP_IndexVendorVOChnFrameRate, (void*)pChnFrmRate);
    return ret;
}

ERRORTYPE AW_MPI_VO_StartChn(VO_LAYER VoLayer, VO_CHN VoChn)
{
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! Send command stateExecuting fail!");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateExecuting == nCompState)
    {
        alogd("VOChannel[%d] already stateExecuting.", VoChn);
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! check voChannel[%d]State[0x%x]!", VoChn, nCompState);
        ret = ERR_VO_CHN_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_VO_StopChn(VO_LAYER VoLayer, VO_CHN VoChn)
{
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! Send command stateIdle fail!");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("VOChannel[%d] already stateIdle.", VoChn);
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! check voLayer[%d] voChannel[%d]State[0x%x]!", VoLayer, VoChn, nCompState);
        ret = ERR_VO_CHN_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_VO_PauseChn (VO_LAYER VoLayer, VO_CHN VoChn)
{
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateExecuting == nCompState)
    {
        eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StatePause, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! Send command statePause fail!");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StatePause == nCompState)
    {
        alogd("VOChannel[%d] already statePause.", VoChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("VOChannel[%d] stateIdle, can't turn to statePause!", VoChn);
        ret = ERR_VO_CHN_INCORRECT_STATE_TRANSITION;
    }
    else
    {
        aloge("fatal error! check voChannel[%d]State[0x%x]!", VoChn, nCompState);
        ret = ERR_VO_CHN_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_VO_ResumeChn(VO_LAYER VoLayer, VO_CHN VoChn)
{
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StatePause == nCompState)
    {
        eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! Send command statePause fail!");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateExecuting == nCompState)
    {
        alogd("VOChannel[%d] already stateExecuting.", VoChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("VOChannel[%d] stateIdle, can't turn to stateExecuting!", VoChn);
        ret = ERR_VO_CHN_INCORRECT_STATE_TRANSITION;
    }
    else
    {
        aloge("fatal error! check voChannel[%d]State[0x%x]!", VoChn, nCompState);
        ret = ERR_VO_CHN_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_VO_Seek(VO_LAYER VoLayer, VO_CHN VoChn)
{
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorSeekToPosition, NULL);
    }
    else
    {
        aloge("fatal error! can't seek in voChannel[%d]State[0x%x]!", VoChn, nCompState);
        ret = ERR_VO_CHN_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}


/** 
 * set or clear stream end of file, mpp_vo can send eof callback msg to upper when it be set eof.
 *
 * @return SUCCESS.
 * @param VoLayer vo layer number.
 * @param VoChn vo channel number.
 * @param bEofFlag indicate if eof.
 */
ERRORTYPE AW_MPI_VO_SetStreamEof(VO_LAYER VoLayer, VO_CHN VoChn, BOOL bEofFlag)
{
    if(!(VoChn>=0 && VoChn <VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
       aloge("wrong state[0x%x], return!", nState);
       return ERR_VO_NOT_PERMIT;
    }
    if(bEofFlag)
    {
        ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorSetStreamEof, NULL);
    }
    else
    {
        ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorClearStreamEof, NULL);
    }
    return ret;
}

ERRORTYPE AW_MPI_VO_ShowChn(VO_LAYER VoLayer, VO_CHN VoChn)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    BOOL bShowFlag = TRUE;
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorShow, &bShowFlag);
    return ret;
}

ERRORTYPE AW_MPI_VO_HideChn(VO_LAYER VoLayer, VO_CHN VoChn)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    BOOL bShowFlag = FALSE;
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorShow, &bShowFlag);
    return ret;
}

ERRORTYPE AW_MPI_VO_SetZoomInWindow(VO_LAYER VoLayer, VO_CHN VoChn, const VO_ZOOM_ATTR_S *pstZoomAttr)
{
    aloge("need implement");
    return ERR_VO_NOT_SUPPORT;
}
ERRORTYPE AW_MPI_VO_GetZoomInWindow(VO_LAYER VoLayer, VO_CHN VoChn, VO_ZOOM_ATTR_S *pstZoomAttr)
{
    aloge("need implement");
    return ERR_VO_NOT_SUPPORT;
}

ERRORTYPE AW_MPI_VO_SetVideoScalingMode(VO_LAYER VoLayer, VO_CHN VoChn, int mode)
{
    aloge("unsupported temporary");
    return ERR_VO_NOT_SUPPORT;
}

ERRORTYPE AW_MPI_VO_GetChnPts   (VO_LAYER VoLayer, VO_CHN VoChn, uint64_t *pChnPts)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    COMP_TIME_CONFIG_TIMESTAMPTYPE timeStamp;
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexConfigTimeCurrentMediaTime, (void*)&timeStamp);
    *pChnPts = timeStamp.nTimestamp;
    return ret;
}

ERRORTYPE AW_MPI_VO_SendFrame(VO_LAYER VoLayer, VO_CHN VoChn, VIDEO_FRAME_INFO_S *pstVFrame, int nMilliSec)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    COMP_BUFFERHEADERTYPE compBuffer;
    compBuffer.pAppPrivate = (void*)pstVFrame;
    ret = COMP_EmptyThisBuffer(pChn->mComp, &compBuffer);
    return ret;
}

ERRORTYPE AW_MPI_VO_Debug_StoreFrame(VO_LAYER VoLayer, VO_CHN VoChn, uint64_t framePts)
{
    if(!(VoChn>=0 && VoChn<VO_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VoChn[%d]!", VoChn);
        return ERR_VO_INVALID_CHNID;
    }
    VO_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VoLayer, VoChn, &pChn))
    {
        return ERR_VO_CHN_NOT_ENABLE;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VO_NOT_PERMIT;
    }
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorStoreFrame, (void*)&framePts);
    return ret;
}

