
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_vo"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <mm_common.h>
#include <vo/hwdisplay.h>
#include <mpi_sys.h>
#include <mpi_vo.h>
#include <ClockCompPortIndex.h>

#include <confparser.h>

#include "sample_vo_config.h"
#include "sample_vo.h"

#include <cdx_list.h>

VIDEO_FRAME_INFO_S* SampleVOFrameManager_PrefetchFirstIdleFrame(void *pThiz)
{
    SampleVOFrameManager *pFrameManager = (SampleVOFrameManager*)pThiz;
    SampleVOFrameNode *pFirstNode;
    VIDEO_FRAME_INFO_S *pFrameInfo;
    pthread_mutex_lock(&pFrameManager->mLock);
    if(!list_empty(&pFrameManager->mIdleList))
    {
        pFirstNode = list_first_entry(&pFrameManager->mIdleList, SampleVOFrameNode, mList);
        pFrameInfo = &pFirstNode->mFrame;
    }
    else
    {
        pFrameInfo = NULL;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return pFrameInfo;
}
int SampleVOFrameManager_UseFrame(void *pThiz, VIDEO_FRAME_INFO_S *pFrame)
{
    int ret = 0;
    SampleVOFrameManager *pFrameManager = (SampleVOFrameManager*)pThiz;
    if(NULL == pFrame)
    {
        aloge("fatal error! pNode == NULL!");
        return -1;
    }
    pthread_mutex_lock(&pFrameManager->mLock);
    SampleVOFrameNode *pFirstNode = list_first_entry_or_null(&pFrameManager->mIdleList, SampleVOFrameNode, mList);
    if(pFirstNode)
    {
        if(&pFirstNode->mFrame == pFrame)
        {
            list_move_tail(&pFirstNode->mList, &pFrameManager->mUsingList);
        }
        else
        {
            aloge("fatal error! node is not match [%p]!=[%p]", pFrame, &pFirstNode->mFrame);
            ret = -1;
        }
    }
    else
    {
        aloge("fatal error! idle list is empty");
        ret = -1;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return ret;
}

int SampleVOFrameManager_ReleaseFrame(void *pThiz, unsigned int nFrameId)
{
    int ret = 0;
    SampleVOFrameManager *pFrameManager = (SampleVOFrameManager*)pThiz;
    pthread_mutex_lock(&pFrameManager->mLock);
    int bFindFlag = 0;
    SampleVOFrameNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mUsingList, mList)
    {
        if(pEntry->mFrame.mId == nFrameId)
        {
            list_move_tail(&pEntry->mList, &pFrameManager->mIdleList);
            bFindFlag = 1;
            break;
        }
    }
    if(0 == bFindFlag)
    {
        aloge("fatal error! frameId[%d] is not find", nFrameId);
        ret = -1;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return ret;
}

int initSampleVOFrameManager(SampleVOFrameManager *pFrameManager, int nFrameNum, SampleVOConfig *pConfigPara)
{
    memset(pFrameManager, 0, sizeof(SampleVOFrameManager));
    int err = pthread_mutex_init(&pFrameManager->mLock, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    INIT_LIST_HEAD(&pFrameManager->mIdleList);
    INIT_LIST_HEAD(&pFrameManager->mUsingList);
    unsigned int nFrameSize;
    switch(pConfigPara->mPicFormat)
    {
        case MM_PIXEL_FORMAT_YUV_PLANAR_420:
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
            nFrameSize = pConfigPara->mPicWidth*pConfigPara->mPicHeight*3/2;
            break;
        default:
            aloge("fatal error! unknown pixel format[0x%x]", pConfigPara->mPicFormat);
            nFrameSize = pConfigPara->mPicWidth*pConfigPara->mPicHeight;
            break;
    }
    int i;
    SampleVOFrameNode *pNode;
    unsigned int uPhyAddr;
    void *pVirtAddr;
    for(i=0; i<nFrameNum; i++)
    {
        pNode = (SampleVOFrameNode*)malloc(sizeof(SampleVOFrameNode));
        memset(pNode, 0, sizeof(SampleVOFrameNode));
        pNode->mFrame.mId = i;
        AW_MPI_SYS_MmzAlloc_Cached(&uPhyAddr, &pVirtAddr, nFrameSize);
        pNode->mFrame.VFrame.mPhyAddr[0] = uPhyAddr;
        pNode->mFrame.VFrame.mPhyAddr[1] = uPhyAddr + pConfigPara->mPicWidth*pConfigPara->mPicHeight;
        pNode->mFrame.VFrame.mPhyAddr[2] = uPhyAddr + pConfigPara->mPicWidth*pConfigPara->mPicHeight + pConfigPara->mPicWidth*pConfigPara->mPicHeight/4;
        pNode->mFrame.VFrame.mpVirAddr[0] = pVirtAddr;
        pNode->mFrame.VFrame.mpVirAddr[1] = pVirtAddr + pConfigPara->mPicWidth*pConfigPara->mPicHeight;
        pNode->mFrame.VFrame.mpVirAddr[2] = pVirtAddr + pConfigPara->mPicWidth*pConfigPara->mPicHeight + pConfigPara->mPicWidth*pConfigPara->mPicHeight/4;
        pNode->mFrame.VFrame.mWidth = pConfigPara->mPicWidth;
        pNode->mFrame.VFrame.mHeight = pConfigPara->mPicHeight;
        pNode->mFrame.VFrame.mField = VIDEO_FIELD_FRAME;
        pNode->mFrame.VFrame.mPixelFormat = pConfigPara->mPicFormat;
        pNode->mFrame.VFrame.mVideoFormat = VIDEO_FORMAT_LINEAR;
        pNode->mFrame.VFrame.mCompressMode = COMPRESS_MODE_NONE;
        pNode->mFrame.VFrame.mOffsetTop = 0;
        pNode->mFrame.VFrame.mOffsetBottom = pConfigPara->mPicHeight;
        pNode->mFrame.VFrame.mOffsetLeft = 0;
        pNode->mFrame.VFrame.mOffsetRight = pConfigPara->mPicWidth;
        list_add_tail(&pNode->mList, &pFrameManager->mIdleList);
    }
    pFrameManager->mNodeCnt = nFrameNum;

    pFrameManager->PrefetchFirstIdleFrame = SampleVOFrameManager_PrefetchFirstIdleFrame;
    pFrameManager->UseFrame = SampleVOFrameManager_UseFrame;
    pFrameManager->ReleaseFrame = SampleVOFrameManager_ReleaseFrame;
    return 0;
}

int destroySampleVOFrameManager(SampleVOFrameManager *pFrameManager)
{
    if(!list_empty(&pFrameManager->mUsingList))
    {
        aloge("fatal error! why using list is not empty");
    }
    int cnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pFrameManager->mIdleList)
    {
        cnt++;
    }
    if(cnt != pFrameManager->mNodeCnt)
    {
        aloge("fatal error! frame count is not match [%d]!=[%d]", cnt, pFrameManager->mNodeCnt);
    }
    SampleVOFrameNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mIdleList, mList)
    {
        AW_MPI_SYS_MmzFree(pEntry->mFrame.VFrame.mPhyAddr[0], pEntry->mFrame.VFrame.mpVirAddr[0]);
        list_del(&pEntry->mList);
        free(pEntry);
    }
    pthread_mutex_destroy(&pFrameManager->mLock);
    return 0;
}

int initSampleVOContext(SampleVOContext *pContext)
{
    memset(pContext, 0, sizeof(SampleVOContext));
    int err = pthread_mutex_init(&pContext->mWaitFrameLock, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    err = cdx_sem_init(&pContext->mSemFrameCome, 0);
    if(err!=0)
    {
        aloge("cdx sem init fail!");
    }
    pContext->mUILayer = HLAY(2, 0);
    return 0;
}
int destroySampleVOContext(SampleVOContext *pContext)
{
    pthread_mutex_destroy(&pContext->mWaitFrameLock);
    cdx_sem_deinit(&pContext->mSemFrameCome);
    return 0;
}

static ERRORTYPE SampleVOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleVOContext *pContext = (SampleVOContext*)cookie;
    if(MOD_ID_VOU == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mVOChn)
        {
            aloge("fatal error! VO chnId[%d]!=[%d]", pChn->mChnId, pContext->mVOChn);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                pContext->mFrameManager.ReleaseFrame(&pContext->mFrameManager, pVideoFrameInfo->mId);
                pthread_mutex_lock(&pContext->mWaitFrameLock);
                if(pContext->mbWaitFrameFlag)
                {
                    pContext->mbWaitFrameFlag = 0;
                    cdx_sem_up(&pContext->mSemFrameCome);
                }
                pthread_mutex_unlock(&pContext->mWaitFrameLock);
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("vo report video display size[%dx%d]", pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                alogd("vo report rendering start");
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! why modId[0x%x]?", pChn->mModId);
        ret = FAILURE;
    }
    return ret;
}

static ERRORTYPE SampleVO_CLOCKCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    alogw("clock channel[%d] has some event[0x%x]", pChn->mChnId, event);
    return SUCCESS;
}

static int ParseCmdLine(int argc, char **argv, SampleVOCmdLineParam *pCmdLinePara)
{
    alogd("sample vo path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleVOCmdLineParam));
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                aloge("fatal error! use -h to learn how to set parameter!!!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error! file path[%s] too long: [%d]>=[%d]!", argv[i], strlen(argv[i]), MAX_FILE_PATH_SIZE);
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], MAX_FILE_PATH_SIZE-1);
            pCmdLinePara->mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
            printf("CmdLine param:\n"
                "\t-path /home/sample_vo.conf\n");
            ret = 1;
            break;
        }
        else
        {
            alogd("ignore invalid CmdLine param:[%s], type -h to get how to set parameter!", argv[i]);
        }
        i++;
    }
    return ret;
}

static ERRORTYPE loadSampleVOConfig(SampleVOConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleVOConfig));
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_VO_KEY_YUV_FILE_PATH, NULL);
    strncpy(pConfig->mYuvFilePath, ptr, MAX_FILE_PATH_SIZE-1);
    pConfig->mYuvFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
    pConfig->mPicWidth = GetConfParaInt(&stConfParser, SAMPLE_VO_KEY_PIC_WIDTH, 0);
    pConfig->mPicHeight = GetConfParaInt(&stConfParser, SAMPLE_VO_KEY_PIC_HEIGHT, 0);
    pConfig->mDisplayWidth  = GetConfParaInt(&stConfParser, SAMPLE_VO_KEY_DISPLAY_WIDTH, 0);
    pConfig->mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_VO_KEY_DISPLAY_HEIGHT, 0);

    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_VO_KEY_PIC_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    char *pStrDispType = (char*)GetConfParaString(&stConfParser, SAMPLE_VO_KEY_DISP_TYPE, NULL);
    if (!strcmp(pStrDispType, "hdmi"))
    {
        pConfig->mDispType = VO_INTF_HDMI;
        pConfig->mDispSync = VO_OUTPUT_1080P25;
    }
    else if (!strcmp(pStrDispType, "lcd"))
    {
        pConfig->mDispType = VO_INTF_LCD;
        pConfig->mDispSync = VO_OUTPUT_NTSC;
    }
    else if (!strcmp(pStrDispType, "cvbs"))
    {
        pConfig->mDispType = VO_INTF_CVBS;
        pConfig->mDispSync = VO_OUTPUT_NTSC;
    }

    pConfig->mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_VO_KEY_FRAME_RATE, 0);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

int main(int argc, char *argv[])
{
    int result = 0;
	printf("sample_vo running!\n");

    SampleVOContext stContext;
    initSampleVOContext(&stContext);
    /* parse command line param */
    if(ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0)
    {
        aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(stContext.mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = stContext.mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = DEFAULT_SAMPLE_VO_CONF_PATH;
    }
    /* parse config file. */
    if(loadSampleVOConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    /* open yuv file */
    stContext.mFpYuvFile = fopen(stContext.mConfigPara.mYuvFilePath, "rb");
    if(!stContext.mFpYuvFile)
    {
        aloge("fatal error! can't open yuv file[%s]", stContext.mConfigPara.mYuvFilePath);
        result = -1;
        goto _exit;
    }
    /* init mpp system */
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();
    /* init frame manager */
    initSampleVOFrameManager(&stContext.mFrameManager, 5, &stContext.mConfigPara);
    /* enable vo dev, default 0 */
    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer.
    /* enable vo layer */
    int hlay0 = 0;
    while(hlay0 < VO_MAX_LAYER_NUM)
    {
        if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
        {
            break;
        }
        hlay0++;
    }
    if(hlay0 >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
    }

#if 1
	VO_PUB_ATTR_S spPubAttr;
	AW_MPI_VO_GetPubAttr(stContext.mVoDev, &spPubAttr);
	spPubAttr.enIntfType = stContext.mConfigPara.mDispType;
	spPubAttr.enIntfSync = stContext.mConfigPara.mDispSync;
	AW_MPI_VO_SetPubAttr(stContext.mVoDev, &spPubAttr);
#endif

    stContext.mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 0;
    stContext.mLayerAttr.stDispRect.Width = stContext.mConfigPara.mDisplayWidth;
    stContext.mLayerAttr.stDispRect.Height = stContext.mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);

    /* create vo channel and clock channel. */
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
    stContext.mVOChn = 0;
    while(stContext.mVOChn < VO_MAX_CHN_NUM)
    {
        ret = AW_MPI_VO_CreateChn(stContext.mVoLayer, stContext.mVOChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create vo channel[%d] success!", stContext.mVOChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == ret)
        {
            alogd("vo channel[%d] is exist, find next!", stContext.mVOChn);
            stContext.mVOChn++;
        }
        else
        {
            aloge("fatal error! create vo channel[%d] ret[0x%x]!", stContext.mVOChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mVOChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleVOCallbackWrapper;
    AW_MPI_VO_RegisterCallback(stContext.mVoLayer, stContext.mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(stContext.mVoLayer, stContext.mVOChn, 2);

    bSuccessFlag = FALSE;
    stContext.mClockChnAttr.nWaitMask = 0;
    stContext.mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_VIDEO;
    stContext.mClockChn = 0;
    while(stContext.mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(stContext.mClockChn, &stContext.mClockChnAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", stContext.mClockChn);
            break;
        }
        else if(ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", stContext.mClockChn);
            stContext.mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", stContext.mClockChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
    }
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleVO_CLOCKCallbackWrapper;
    AW_MPI_CLOCK_RegisterCallback(stContext.mClockChn, &cbInfo);
    /* bind clock and vo */
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, stContext.mClockChn};
    MPP_CHN_S VOChn = {MOD_ID_VOU, stContext.mVoLayer, stContext.mVOChn};
    AW_MPI_SYS_Bind(&ClockChn, &VOChn);

    /* start vo and clock. */
    AW_MPI_CLOCK_Start(stContext.mClockChn);
    AW_MPI_VO_StartChn(stContext.mVoLayer, stContext.mVOChn);

    /* read frame from file, display frame through mpi_vo.
     * we set pts by stContext.mConfigPara.mFrameRate.
     */
    uint64_t nPts = 0;   /* unit:us */
    uint64_t nFrameInterval = 1000000/stContext.mConfigPara.mFrameRate; //unit:us
    int nFrameSize;
    switch(stContext.mConfigPara.mPicFormat)
    {
        case MM_PIXEL_FORMAT_YUV_PLANAR_420:
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
            nFrameSize = stContext.mConfigPara.mPicWidth*stContext.mConfigPara.mPicHeight*3/2;
            break;
        default:
            aloge("fatal error! unknown pixel format[0x%x]", stContext.mConfigPara.mPicFormat);
            nFrameSize = stContext.mConfigPara.mPicWidth*stContext.mConfigPara.mPicHeight;
            break;
    }

    VIDEO_FRAME_INFO_S *pFrameInfo;
    int nReadLen;
    while(1)
    {
        /* request idle frame */
        pFrameInfo = stContext.mFrameManager.PrefetchFirstIdleFrame(&stContext.mFrameManager);
        if(NULL == pFrameInfo)
        {
            pthread_mutex_lock(&stContext.mWaitFrameLock);
            pFrameInfo = stContext.mFrameManager.PrefetchFirstIdleFrame(&stContext.mFrameManager);
            if(pFrameInfo!=NULL)
            {
                pthread_mutex_unlock(&stContext.mWaitFrameLock);
            }
            else
            {
                stContext.mbWaitFrameFlag = 1;
                pthread_mutex_unlock(&stContext.mWaitFrameLock);
                cdx_sem_down_timedwait(&stContext.mSemFrameCome, 500);
                continue;
            }
        }
        AW_MPI_SYS_MmzFlushCache(pFrameInfo->VFrame.mPhyAddr[0], pFrameInfo->VFrame.mpVirAddr[0], nFrameSize);
        /* read data to idle frame */
        nReadLen = fread(pFrameInfo->VFrame.mpVirAddr[0], 1, nFrameSize, stContext.mFpYuvFile);
        if(nReadLen < nFrameSize)
        {
            int bEof = feof(stContext.mFpYuvFile);
            if(bEof)
            {
                alogd("read file finish!");
            }
            break;
        }
        pFrameInfo->VFrame.mpts = nPts;
        nPts += nFrameInterval;
        AW_MPI_SYS_MmzFlushCache(pFrameInfo->VFrame.mPhyAddr[0], pFrameInfo->VFrame.mpVirAddr[0], nFrameSize);
        stContext.mFrameManager.UseFrame(&stContext.mFrameManager, pFrameInfo);
        ret = AW_MPI_VO_SendFrame(stContext.mVoLayer, stContext.mVOChn, pFrameInfo, 0);
        if(ret != SUCCESS)
        {
            alogd("impossible, send frameId[%d] fail?", pFrameInfo->mId);
            stContext.mFrameManager.ReleaseFrame(&stContext.mFrameManager, pFrameInfo->mId);
        }
    }

    /* stop vo channel, clock channel */
    AW_MPI_VO_StopChn(stContext.mVoLayer, stContext.mVOChn);
    AW_MPI_CLOCK_Stop(stContext.mClockChn);
    AW_MPI_VO_DestroyChn(stContext.mVoLayer, stContext.mVOChn);
    stContext.mVOChn = MM_INVALID_CHN;
    AW_MPI_CLOCK_DestroyChn(stContext.mClockChn);
    stContext.mClockChn = MM_INVALID_CHN;
    /* disable vo layer */
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;
    /* destroy frame manager */
    destroySampleVOFrameManager(&stContext.mFrameManager);
    /* exit mpp system */
    AW_MPI_SYS_Exit();
    /* close yuv file */
    fclose(stContext.mFpYuvFile);
    stContext.mFpYuvFile = NULL;
    destroySampleVOContext(&stContext);

_exit:
    destroySampleVOContext(&stContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

//}

