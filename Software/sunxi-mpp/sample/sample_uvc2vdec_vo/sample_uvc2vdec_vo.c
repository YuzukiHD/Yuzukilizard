//#define LOG_NDEBUG 0
#define LOG_TAG "sample_uvc2vdec_vo"
#include <utils/plat_log.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <confparser.h>
#include <mpi_uvc.h>
#include <mpi_vdec.h>
#include <mpi_vo.h>
#include "vo/hwdisplay.h"

#include "sample_uvc2vdec_vo.h"
#include "sample_uvc2vdec_vo_config.h"


static SampleUvc2VdecVoContext *gpSampleUvc2VdecVoContext = NULL;

int initSampleUvc2VdecVoContext(SampleUvc2VdecVoContext *pContext)
{
    memset(pContext, 0, sizeof *pContext);
    pContext->mUILayer = HLAY(2, 0);
    pContext->mVoLayer = MM_INVALID_LAYER;
    pContext->mVOChn = MM_INVALID_CHN;
    pContext->mSubVoLayer = MM_INVALID_LAYER;
    pContext->mSubVOChn = MM_INVALID_CHN;
    cdx_sem_init(&pContext->mSemExit, 0);
    int ret = pthread_mutex_init(&pContext->mFrameLock, NULL);
	if (ret!=0)
	{
        aloge("fatal error! pthread mutex init fail!");
	}
    int i;
    for(i=0; i< MAX_FRAMEPAIR_ARRAY_SIZE; i++)
    {
        pContext->mDoubleFrameArray[i].mMainFrame.mId = -1;
        pContext->mDoubleFrameArray[i].mSubFrame.mId = -1;
    }
    return 0;
}

int destroySampleUvc2VdecVoContext(SampleUvc2VdecVoContext *pContext)
{
    cdx_sem_deinit(&pContext->mSemExit);
    pthread_mutex_destroy(&pContext->mFrameLock);
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleUvc2VdecVoCmdLineParam *pCmdLinePara)
{
    //alogd("sample_region input path is : %s", argv[0]);
    int ret = 0;
    int i = 1;
    memset(pCmdLinePara, 0, sizeof *pCmdLinePara);
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if((++i) >= argc)
            {
                aloge("fatal error!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error!");
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], strlen(argv[i]));
            pCmdLinePara->mConfigFilePath[strlen(argv[i])] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
             printf("CmdLine param example:\n"                
                "\t run -path /home/sample_uvc2vo.conf\n");
             ret = 1;
             break;
        }
        else
        {
            printf("CmdLine param example:\n"                "\t run -path /home/sample_uvc2vo.conf\n");
        }
        ++i;
    }
    return ret;
}

static ERRORTYPE LoadSampleUvc2VdecVoConfig(SampleUvc2VdecVoConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;
    if(!conf_path)
    {
        alogd("user not set config file. use default test parameter!");
        strcpy(pConfig->mDevName, (void *)"/dev/video0");
        pConfig->mPicFormat = UVC_MJPEG;
        pConfig->mCaptureVideoBufCnt = 5;
        pConfig->mCaptureWidth = 1280;
        pConfig->mCaptureHeight = 720;
        pConfig->mCaptureFrameRate = 30;
        pConfig->mbDecodeSubOutEnable = FALSE;
        pConfig->mDecodeSubOutWidthScalePower = 0;
        pConfig->mDecodeSubOutHeightScalePower = 0;
        pConfig->mDisplayMainX = 0;
        pConfig->mDisplayMainY = 0;
        pConfig->mDisplayMainWidth = 320;
        pConfig->mDisplayMainHeight = 240;
        pConfig->mDisplaySubX = 320;
        pConfig->mDisplaySubY = 0;
        pConfig->mDisplaySubWidth = 320;
        pConfig->mDisplaySubHeight = 240;
        pConfig->mTestFrameCount = 0;  
    }
    else
    {
        CONFPARSER_S stConfParser;
        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail!");
            return FAILURE;
        }

        memset(pConfig, 0, sizeof *pConfig);

        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DEV_NAME, NULL);
        if(ptr)
        {
            strcpy(pConfig->mDevName, ptr);
        }
        else
        {
            aloge("fatal error! the uvc dev name is error!");
            return FAILURE;   
        }
        char* pStrPixelFormat = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_PIC_FORMAT, NULL);
        if(!strcmp(pStrPixelFormat, "UVC_MJPEG"))
        {
            pConfig->mPicFormat = UVC_MJPEG;
        }
        else if(!strcmp(pStrPixelFormat, "UVC_H264"))
        {
            pConfig->mPicFormat = UVC_H264;
        }
        else if(!strcmp(pStrPixelFormat, "UVC_YUY2"))
        {
            pConfig->mPicFormat = UVC_YUY2;
        }
        else
        {
            aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
            pConfig->mPicFormat = UVC_YUY2;
        }
        pConfig->mCaptureVideoBufCnt = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_CAPTURE_VIDEOBUFCNT, 0);
        pConfig->mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_CAPTURE_WIDTH, 640);
        pConfig->mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_CAPTURE_HEIGHT, 480);
        pConfig->mCaptureFrameRate = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_CAPTURE_FRAMERATE, 0);
        pConfig->mbDecodeSubOutEnable = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DECODE_SUB_OUT_ENABLE, 0);
        pConfig->mDecodeSubOutWidthScalePower = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DECODE_SUB_OUT_WIDTH_SCALE_POWER, 0);
        pConfig->mDecodeSubOutHeightScalePower = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DECODE_SUB_OUT_HEIGHT_SCALE_POWER, 0);
        pConfig->mDisplayMainX = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DISPLAY_MAIN_X, 0);
        pConfig->mDisplayMainY = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DISPLAY_MAIN_Y, 0);
        pConfig->mDisplayMainWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DISPLAY_MAIN_WIDTH, 0);
        pConfig->mDisplayMainHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DISPLAY_MAIN_HEIGHT, 0);
        pConfig->mDisplaySubX = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DISPLAY_SUB_X, 0);
        pConfig->mDisplaySubY = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DISPLAY_SUB_Y, 0);
        pConfig->mDisplaySubWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DISPLAY_SUB_WIDTH, 0);
        pConfig->mDisplaySubHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_DISPLAY_SUB_HEIGHT, 0);
        pConfig->mTestFrameCount = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDEC_VO_KEY_TEST_FRAME_COUNT, 0);

        destroyConfParser(&stConfParser);
    }
    return SUCCESS;
}

/**
 *
 * @return suffix of array, if not find, return -1.
 */
int FindFrameIdInArray(SampleUvc2VdecVoContext *pContext, unsigned int nFrameId)
{
    int suffix = -1;
    int matchNum = 0;
    int i;
    for(i=0; i<MAX_FRAMEPAIR_ARRAY_SIZE; i++)
    {
        if(nFrameId == pContext->mDoubleFrameArray[i].mMainFrame.mId)
        {
            if(0 == matchNum)
            {
                suffix = i;
            }
            else
            {
                aloge("fatal error! already match num[%d], current suffix[%d], id[%d]", matchNum, i, nFrameId);
            }
            matchNum++;
        }
    }
    return suffix;
}

static ERRORTYPE SampleUvc2VdecVo_VOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleUvc2VdecVoContext *pContext = (SampleUvc2VdecVoContext*)cookie;
    if(MOD_ID_VOU == pChn->mModId)
    {
        int bMainFrameFlag = 0;
        if(pChn->mDevId == pContext->mVoLayer && pChn->mChnId == pContext->mVOChn)
        {
            bMainFrameFlag = 1;
        }
        else
        {
            if(pContext->mConfigParam.mbDecodeSubOutEnable)
            {
                if(pChn->mDevId == pContext->mSubVoLayer && pChn->mChnId == pContext->mSubVOChn)
                {
                    bMainFrameFlag = 0;
                }
                else
                {
                    aloge("fatal error! VO layerId[%d]chnId[%d] is invalid! main/sub frame all wrong!", pChn->mDevId, pChn->mChnId);
                }
            }
            else
            {
                aloge("fatal error! VO layerId[%d]!=[%d], chnId[%d]!=[%d]", pChn->mDevId, pContext->mVoLayer, pChn->mChnId, pContext->mVOChn);
            }
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                pthread_mutex_lock(&pContext->mFrameLock);
                VdecDoubleFrameInfo *pDbFramePair = NULL;
                int suffix = FindFrameIdInArray(pContext, pVideoFrameInfo->mId);
                if(suffix >= 0)
                {
                    pDbFramePair = &pContext->mDoubleFrameArray[suffix];
                }
                else
                {
                    aloge("fatal error! why not find frameId[%d]?", pVideoFrameInfo->mId);
                }
                if(bMainFrameFlag)
                {
                    pDbFramePair->mMainRefCnt--;
                    if(pDbFramePair->mMainRefCnt < 0)
                    {
                        aloge("fatal error! mainRefCnt[%d] wrong!", pDbFramePair->mMainRefCnt);
                    }
                }
                else
                {
                    pDbFramePair->mSubRefCnt--;
                    if(pDbFramePair->mSubRefCnt < 0)
                    {
                        aloge("fatal error! subRefCnt[%d] wrong!", pDbFramePair->mSubRefCnt);
                    }
                }
                if(0 == pDbFramePair->mMainRefCnt && 0 == pDbFramePair->mSubRefCnt)
                {
                    ret = AW_MPI_VDEC_ReleaseDoubleImage(pContext->mVdecChn, &pDbFramePair->mMainFrame, &pDbFramePair->mSubFrame);
                    if(SUCCESS == ret)
                    {
                        pContext->mHoldFrameNum--;
                        if(pContext->mHoldFrameNum < 0)
                        {
                            aloge("fatal error! holdframe num[%d] < 0, check code!", pContext->mHoldFrameNum);
                        }
                    }
                    else
                    {
                        aloge("fatal error! why release frame to vdec fail[0x%x]?", ret);
                    }
                }
                pthread_mutex_unlock(&pContext->mFrameLock);
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("volayer[%d][%d] report video display size[%dx%d]", pChn->mDevId, pChn->mChnId, pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                alogd("volayer[%d][%d] report rendering start", pChn->mDevId, pChn->mChnId);
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

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleUvc2VdecVoContext != NULL)
    {
        cdx_sem_up(&gpSampleUvc2VdecVoContext->mSemExit);
    }
}

int main(int argc, char **argv)
{
    int result = 0;

    SampleUvc2VdecVoContext *pContext = (SampleUvc2VdecVoContext*)malloc(sizeof(SampleUvc2VdecVoContext));
    initSampleUvc2VdecVoContext(pContext);
    gpSampleUvc2VdecVoContext = pContext;

    if(ParseCmdLine(argc, argv, &pContext->mCmdLineParam) != 0)
    {
        result = -1;
        goto _exit;
    }

    char *pConfigFilePath = NULL;
    if(strlen(pContext->mCmdLineParam.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLineParam.mConfigFilePath;
    }
    if(LoadSampleUvc2VdecVoConfig(&pContext->mConfigParam, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        perror("error:can't catch SIGSEGV");
    }

    memset(&pContext->mSysconf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysconf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysconf);
    AW_MPI_SYS_Init();
    AW_MPI_VDEC_SetVEFreq(MM_INVALID_CHN, 648);

    ERRORTYPE eRet = AW_MPI_UVC_CreateDevice(pContext->mConfigParam.mDevName);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC device can not create!", pContext->mConfigParam.mDevName);
    }
    UVC_ATTR_S attr;
    memset(&attr, 0, sizeof attr);
    attr.mPixelformat = pContext->mConfigParam.mPicFormat;
    attr.mUvcVideo_BufCnt = pContext->mConfigParam.mCaptureVideoBufCnt;
    attr.mUvcVideo_Width = pContext->mConfigParam.mCaptureWidth;
    attr.mUvcVideo_Height = pContext->mConfigParam.mCaptureHeight;
    attr.mUvcVideo_Fps = pContext->mConfigParam.mCaptureFrameRate;
    eRet = AW_MPI_UVC_SetDeviceAttr(pContext->mConfigParam.mDevName, &attr);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC can not set device attr", pContext->mConfigParam.mDevName);
    }
    eRet = AW_MPI_UVC_GetDeviceAttr(pContext->mConfigParam.mDevName, &attr);
    if(eRet != SUCCESS)
    {
        aloge("error: the %UVC can not get device attr", pContext->mConfigParam.mDevName);
    }
    
    pContext->mUvcChn = 0;
    eRet = AW_MPI_UVC_CreateVirChn(pContext->mConfigParam.mDevName, pContext->mUvcChn);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC can not create virchannel[%d]", pContext->mConfigParam.mDevName, pContext->mUvcChn);
    }

    pContext->mVdecChn = 0;
    VDEC_CHN_ATTR_S VdecChnAttr;
    memset(&VdecChnAttr, 0, sizeof VdecChnAttr);
    VdecChnAttr.mType = PT_MJPEG;
    VdecChnAttr.mPicWidth = 0;
    VdecChnAttr.mPicHeight = 0;
    VdecChnAttr.mInitRotation = ROTATE_NONE;
    VdecChnAttr.mOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VdecChnAttr.mSubPicEnable = pContext->mConfigParam.mbDecodeSubOutEnable;
    VdecChnAttr.mSubPicWidthRatio = pContext->mConfigParam.mDecodeSubOutWidthScalePower;
    VdecChnAttr.mSubPicHeightRatio = pContext->mConfigParam.mDecodeSubOutHeightScalePower;
    VdecChnAttr.mSubOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
    VdecChnAttr.mVdecVideoAttr.mSupportBFrame = 1;
    eRet = AW_MPI_VDEC_CreateChn(pContext->mVdecChn, &VdecChnAttr);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %s uvc can not create vdec chn[%s]", pContext->mConfigParam.mDevName, pContext->mVdecChn);
    }

    pContext->mVoDev = 0;
    AW_MPI_VO_Enable(pContext->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);
    VO_PUB_ATTR_S stPubAttr;
    memset(&stPubAttr, 0, sizeof(VO_PUB_ATTR_S));
    AW_MPI_VO_GetPubAttr(pContext->mVoDev, &stPubAttr);
    stPubAttr.enIntfType = VO_INTF_LCD;
    stPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(pContext->mVoDev, &stPubAttr);

    int hlay0 = 0;
    while(hlay0 < VO_MAX_LAYER_NUM)
    {
        if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
        {
            break;
        }
        hlay0+=4;
    }
    if(hlay0 >= VO_MAX_LAYER_NUM)
    {
        aloge("error: enable video layer failed");
    }

    pContext->mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(pContext->mVoLayer, &pContext->mLayerAttr);
    pContext->mLayerAttr.stDispRect.X = pContext->mConfigParam.mDisplayMainX;
    pContext->mLayerAttr.stDispRect.Y = pContext->mConfigParam.mDisplayMainY;
    pContext->mLayerAttr.stDispRect.Width = pContext->mConfigParam.mDisplayMainWidth;
    pContext->mLayerAttr.stDispRect.Height = pContext->mConfigParam.mDisplayMainHeight;
    AW_MPI_VO_SetVideoLayerAttr(pContext->mVoLayer, &pContext->mLayerAttr);

    BOOL bSuccessFlag = FALSE;
    pContext->mVOChn = 0;
    while(pContext->mVOChn < VO_MAX_CHN_NUM)
    {
        eRet = AW_MPI_VO_CreateChn(pContext->mVoLayer, pContext->mVOChn);
        if(SUCCESS == eRet)
        {
            bSuccessFlag = TRUE;
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == eRet)
        {
            alogd("vo channel[%d] is exist, find next", pContext->mVOChn);
            pContext->mVOChn++; 
        }
        else
        {
            aloge("error:create vo channel[%d] ret[0x%x]", pContext->mVOChn, eRet);
            pContext->mVOChn++;
        }      
    }
    if(!bSuccessFlag)
    {
        pContext->mVOChn = MM_INVALID_CHN;
        aloge("fatal error: create vo channel failed");
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void *)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleUvc2VdecVo_VOCallbackWrapper;
    AW_MPI_VO_RegisterCallback(pContext->mVoLayer, pContext->mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(pContext->mVoLayer, pContext->mVOChn, 2);

    if(pContext->mConfigParam.mbDecodeSubOutEnable)
    {
        int hlay0 = 0;
        while(hlay0 < VO_MAX_LAYER_NUM)
        {
            if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
            {
                break;
            }
            hlay0+=4;
        }
        if(hlay0 >= VO_MAX_LAYER_NUM)
        {
            aloge("error: enable video layer failed");
        }

        pContext->mSubVoLayer = hlay0;
        AW_MPI_VO_GetVideoLayerAttr(pContext->mSubVoLayer, &pContext->mSubLayerAttr);
        pContext->mSubLayerAttr.stDispRect.X = pContext->mConfigParam.mDisplaySubX;
        pContext->mSubLayerAttr.stDispRect.Y = pContext->mConfigParam.mDisplaySubY;
        pContext->mSubLayerAttr.stDispRect.Width = pContext->mConfigParam.mDisplaySubWidth;
        pContext->mSubLayerAttr.stDispRect.Height = pContext->mConfigParam.mDisplaySubHeight;
        AW_MPI_VO_SetVideoLayerAttr(pContext->mSubVoLayer, &pContext->mSubLayerAttr);

        BOOL bSuccessFlag = FALSE;
        pContext->mSubVOChn = 0;
        while(pContext->mSubVOChn < VO_MAX_CHN_NUM)
        {
            eRet = AW_MPI_VO_CreateChn(pContext->mSubVoLayer, pContext->mSubVOChn);
            if(SUCCESS == eRet)
            {
                bSuccessFlag = TRUE;
                break;
            }
            else if(ERR_VO_CHN_NOT_DISABLE == eRet)
            {
                alogd("vo channel[%d] is exist, find next", pContext->mSubVOChn);
                pContext->mSubVOChn++; 
            }
            else
            {
                aloge("error:create vo channel[%d] ret[0x%x]", pContext->mSubVOChn, eRet);
                pContext->mSubVOChn++;
            }      
        }
        if(!bSuccessFlag)
        {
            pContext->mSubVOChn = MM_INVALID_CHN;
            aloge("fatal error: create vo channel failed");
        }
        AW_MPI_VO_RegisterCallback(pContext->mSubVoLayer, pContext->mSubVOChn, &cbInfo);
        AW_MPI_VO_SetChnDispBufNum(pContext->mSubVoLayer, pContext->mSubVOChn, 2);
    }

    
    MPP_CHN_S UvcChn = {MOD_ID_UVC, (int)pContext->mConfigParam.mDevName, pContext->mUvcChn}; // note: UvcChn->mDevId is the pointer of char *;
    MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pContext->mVdecChn};
    eRet = AW_MPI_SYS_Bind(&UvcChn, &VdecChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! bind uvc2vdec fail[0x%x]", eRet);
    }

    eRet = AW_MPI_UVC_EnableDevice(pContext->mConfigParam.mDevName);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %UVC device can not start", pContext->mConfigParam.mDevName);
    }
    eRet = AW_MPI_UVC_StartRecvPic(pContext->mConfigParam.mDevName, pContext->mUvcChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %s UVC can not start virchannel[%d]", pContext->mConfigParam.mDevName, pContext->mUvcChn);
    }
    AW_MPI_VDEC_StartRecvStream(pContext->mVdecChn);

    AW_MPI_VO_StartChn(pContext->mVoLayer, pContext->mVOChn);
    if(pContext->mConfigParam.mbDecodeSubOutEnable)
    {
        AW_MPI_VO_StartChn(pContext->mSubVoLayer, pContext->mSubVOChn);
    }

    //get frame from vdec, send frame to vo.
    VIDEO_FRAME_INFO_S mainFrameInfo;
    VIDEO_FRAME_INFO_S subFrameInfo;
    while(1)
    {
        if(pContext->mConfigParam.mTestFrameCount > 0)
        {
            if(pContext->mFrameCounter>=pContext->mConfigParam.mTestFrameCount)
            {
                alogd("get [%d] frames from vdec, prepare to exit!", pContext->mFrameCounter);
                break;
            }
        }
        if(cdx_sem_get_val(&pContext->mSemExit) > 0)
        {
            alogd("detect user exit signal! prepare to exit!");
            break;
        }

        eRet = AW_MPI_VDEC_GetDoubleImage(pContext->mVdecChn, &mainFrameInfo, &subFrameInfo, 500);
        VdecDoubleFrameInfo *pDstDbFrame = NULL;
        if(SUCCESS == eRet)
        {
            pthread_mutex_lock(&pContext->mFrameLock);
            int suffix = FindFrameIdInArray(pContext, mainFrameInfo.mId);
            if(-1 == suffix)
            {
                if(pContext->mValidDoubleFrameBufferNum >= MAX_FRAMEPAIR_ARRAY_SIZE)
                {
                    aloge("fatal error! frame number [%d] too much!", pContext->mValidDoubleFrameBufferNum);
                }
                pDstDbFrame = &pContext->mDoubleFrameArray[pContext->mValidDoubleFrameBufferNum];
                pContext->mValidDoubleFrameBufferNum++;
            }
            else
            {
                pDstDbFrame = &pContext->mDoubleFrameArray[suffix];
            }
            if(pDstDbFrame->mMainRefCnt!=0 || pDstDbFrame->mSubRefCnt!=0)
            {
                aloge("fatal error! refCnt[%d],[%d] != 0", pDstDbFrame->mMainRefCnt, pDstDbFrame->mSubRefCnt);
            }
            pDstDbFrame->mMainFrame = mainFrameInfo;
            
            if(pContext->mConfigParam.mbDecodeSubOutEnable)
            {
                pDstDbFrame->mSubFrame = subFrameInfo;
            }
            pContext->mFrameCounter++;
            pContext->mHoldFrameNum++;
            pthread_mutex_unlock(&pContext->mFrameLock);
        }
        else
        {
            alogw("why not get frame from vdec for too long? ret=0x%x", eRet);
            continue;
        }

        pthread_mutex_lock(&pContext->mFrameLock);
        pDstDbFrame->mMainRefCnt++;
        if(pContext->mConfigParam.mbDecodeSubOutEnable)
        {
            pDstDbFrame->mSubRefCnt++;
        }
        eRet = AW_MPI_VO_SendFrame(pContext->mVoLayer, pContext->mVOChn, &pDstDbFrame->mMainFrame, 0);
        if(SUCCESS == eRet)
        {
        }
        else
        {
            aloge("fatal error! why send frame to vo fail? layer[%d]chn[%d], ret[0x%x]", pContext->mVoLayer, pContext->mVOChn, eRet);
            pDstDbFrame->mMainRefCnt--;
        }
        if(pContext->mConfigParam.mbDecodeSubOutEnable)
        {
            eRet = AW_MPI_VO_SendFrame(pContext->mSubVoLayer, pContext->mSubVOChn, &pDstDbFrame->mSubFrame, 0);
            if(SUCCESS == eRet)
            {
            }
            else
            {
                aloge("fatal error! why send frame to vo fail? layer[%d]chn[%d], ret[0x%x]", pContext->mSubVoLayer, pContext->mSubVOChn, eRet);
                pDstDbFrame->mSubRefCnt--;
            }
        }
        pthread_mutex_unlock(&pContext->mFrameLock);
    }

    AW_MPI_VO_StopChn(pContext->mVoLayer, pContext->mVOChn);
    if(pContext->mConfigParam.mbDecodeSubOutEnable)
    {
        AW_MPI_VO_StopChn(pContext->mSubVoLayer, pContext->mSubVOChn);
    }
    AW_MPI_VDEC_StopRecvStream(pContext->mVdecChn);
    AW_MPI_UVC_StopRecvPic(pContext->mConfigParam.mDevName, pContext->mUvcChn);

    AW_MPI_VO_DestroyChn(pContext->mVoLayer, pContext->mVOChn);
    pContext->mVOChn = MM_INVALID_CHN;
    if(pContext->mConfigParam.mbDecodeSubOutEnable)
    {
        AW_MPI_VO_DestroyChn(pContext->mSubVoLayer, pContext->mSubVOChn);
        pContext->mSubVOChn = MM_INVALID_CHN;
    }
    
    AW_MPI_VDEC_DestroyChn(pContext->mVdecChn);
    pContext->mVdecChn = MM_INVALID_CHN;
    AW_MPI_UVC_DestroyVirChn(pContext->mConfigParam.mDevName, pContext->mUvcChn);
    pContext->mUvcChn = MM_INVALID_CHN;
    
    AW_MPI_UVC_DisableDevice(pContext->mConfigParam.mDevName);
    AW_MPI_UVC_DestroyDevice(pContext->mConfigParam.mDevName);
    //stContext.mConfigParam.mDevName = NULL; 
    
    AW_MPI_VO_OpenVideoLayer(pContext->mUILayer); /* open ui layer. */
    AW_MPI_VO_DisableVideoLayer(pContext->mVoLayer);
    pContext->mVoLayer = MM_INVALID_LAYER;
    if(pContext->mConfigParam.mbDecodeSubOutEnable)
    {
        AW_MPI_VO_DisableVideoLayer(pContext->mSubVoLayer);
        pContext->mSubVoLayer = MM_INVALID_LAYER;
    }
    AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(pContext->mVoDev);
    pContext->mVoDev = MM_INVALID_DEV;
    
    AW_MPI_SYS_Exit();
    if(pContext->mHoldFrameNum != 0)
    {
        aloge("fatal error! why holdframe num[%d] != 0?", pContext->mHoldFrameNum);
    }
    alogd("vdec buffer number is [%d]", pContext->mValidDoubleFrameBufferNum);
    int i;
    for(i=0;i<pContext->mValidDoubleFrameBufferNum;i++)
    {
        alogd("vdec buffer[%d] id =[%d]", i, pContext->mDoubleFrameArray[i].mMainFrame.mId);
    }
    
_exit:
    destroySampleUvc2VdecVoContext(pContext);
    free(pContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;

}
