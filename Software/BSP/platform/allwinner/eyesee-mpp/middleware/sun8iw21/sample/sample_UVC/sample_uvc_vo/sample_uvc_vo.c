//#define LOG_NDEBUG 0
#define LOG_TAG "sample_uvc2vo"

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
#include <mpi_vo.h>
#include "vo/hwdisplay.h"

#include "sample_uvc_vo.h"
#include "sample_uvc_vo_config.h"

static SampleUvcVoContext *gpSampleUvcVoContext = NULL;

int initSampleUvcVoContext(SampleUvcVoContext *pContext)
{
    memset(pContext, 0, sizeof *pContext);
    pContext->mUILayer = HLAY(2, 0);
    cdx_sem_init(&pContext->mSemExit, 0);

    int ret = pthread_mutex_init(&pContext->mFrameCountLock, NULL);
	if (ret!=0)
	{
        aloge("fatal error! pthread mutex init fail!");
	}
    return 0;
}

int destroySampleUvcVoContext(SampleUvcVoContext *pContext)
{
    cdx_sem_deinit(&pContext->mSemExit);
    pthread_mutex_destroy(&pContext->mFrameCountLock);
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleUvcVoCmdLineParam *pCmdLinePara)
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

static ERRORTYPE LoadSampleUvcVoConfig(SampleUvcVoConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;
    memset(pConfig, 0, sizeof *pConfig);
    if(!conf_path)
    {
        alogd("user not set config file. use default test parameter!");
        strcpy(pConfig->mDevName, (void *)"/dev/video0");
        pConfig->mCaptureWidth = 640;
        pConfig->mCaptureHeight = 480;
        pConfig->mPicFormat = UVC_YUY2;
        pConfig->mCaptureVideoBufCnt = 5;
        pConfig->mDisplayWidth = 640;
        pConfig->mDisplayHeight = 480;
        pConfig->mTestFrameCount = 0; 
        pConfig->mBrightness = 0;
        pConfig->mContrast = 0;
        pConfig->mHue = 0;
        pConfig->mSaturation = 0;
        pConfig->mSharpness = 0;
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
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VO_KEY_DEV_NAME, NULL);
        if(ptr)
        {
            strcpy(pConfig->mDevName, ptr);
        }
        else
        {
            aloge("fatal error! the uvc dev name is error!");
            return FAILURE;   
        }

        pConfig->mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_CAPTURE_WIDTH, 640);
        pConfig->mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_CAPTURE_HEIGHT, 480);
        char* pStrPixelFormat = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VO_KEY_PIC_FORMAT, NULL);
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
        pConfig->mCaptureVideoBufCnt = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_CAPTURE_VIDEOBUFCNT, 0);
        pConfig->mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_DISPLAY_WIDTH, 240);
        pConfig->mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_DISPLAY_HEIGHT, 320);
        pConfig->mTestFrameCount = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_TEST_FRAME_COUNT, 0);

        pConfig->mBrightness = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_BRIGHTNESS, 0);
        pConfig->mContrast = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_CONTRAST, 0);
        pConfig->mHue = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_HUE, 0);
        pConfig->mSaturation = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_SATURATION, 0);
        pConfig->mSharpness = GetConfParaInt(&stConfParser, SAMPLE_UVC_VO_KEY_SHARPNESS, 0);

        destroyConfParser(&stConfParser);
    }
    return SUCCESS;
}

static ERRORTYPE SampleUvcVo_VOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleUvcVoContext *pContext = (SampleUvcVoContext*)cookie;
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
                ERRORTYPE ret = AW_MPI_UVC_ReleaseFrame(pContext->mConfigParam.mDevName, pContext->mUvcChn, pVideoFrameInfo);
                if(ret == SUCCESS)
                {
                    pthread_mutex_lock(&pContext->mFrameCountLock);
                    pContext->mHoldFrameNum--;
                    if(pContext->mHoldFrameNum < 0)
                    {
                        aloge("fatal error! holdframe num[%d] < 0, check code!", pContext->mHoldFrameNum);
                    }
                    pthread_mutex_unlock(&pContext->mFrameCountLock);
                }
                else
                {
                    aloge("fatal error! release frame to uvc fail[0x%x]!", ret);
                }
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

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleUvcVoContext != NULL)
    {
        cdx_sem_up(&gpSampleUvcVoContext->mSemExit);
    }
}


int
main(int argc, char **argv)
{
    int result = 0;

    SampleUvcVoContext *pContext = (SampleUvcVoContext*)malloc(sizeof(SampleUvcVoContext));
    initSampleUvcVoContext(pContext);
    gpSampleUvcVoContext = pContext;

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
    if(LoadSampleUvcVoConfig(&pContext->mConfigParam, pConfigFilePath) != SUCCESS)
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

    if(pContext->mConfigParam.mDevName == NULL)
    {
        aloge("fatal error! why devname == NULL?");
    }

    int eRet = AW_MPI_UVC_CreateDevice(pContext->mConfigParam.mDevName);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC device can not create!", pContext->mConfigParam.mDevName);
    }

    UVC_ATTR_S attr;
    memset(&attr, 0, sizeof attr);
    attr.mPixelformat = pContext->mConfigParam.mPicFormat;
    attr.mUvcVideo_Width = pContext->mConfigParam.mCaptureWidth;
    attr.mUvcVideo_Height = pContext->mConfigParam.mCaptureHeight;
    attr.mUvcVideo_Fps = 30;
    attr.mUvcVideo_BufCnt = pContext->mConfigParam.mCaptureVideoBufCnt;
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
    alogd("uvc device attr setting result: format[0x%x], size[%dx%d], fps[%d]", attr.mPixelformat, attr.mUvcVideo_Width, attr.mUvcVideo_Height, attr.mUvcVideo_Fps);

    alogw("-------> get %s uvc control param <-------", pContext->mConfigParam.mDevName);
    int BrightnessValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetBrightness(pContext->mConfigParam.mDevName, &BrightnessValue))
    {
        alogd("current brightness = %d", BrightnessValue);
        if(0 == pContext->mConfigParam.mBrightness)
        {
            alogd("use brightness = %d as default!", BrightnessValue);
            pContext->mConfigParam.mBrightness = BrightnessValue;
        }
        alogd("set brightness = %d", pContext->mConfigParam.mBrightness);
        AW_MPI_UVC_SetBrightness(pContext->mConfigParam.mDevName, pContext->mConfigParam.mBrightness);
    }
    int ContrastValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetContrast(pContext->mConfigParam.mDevName, &ContrastValue))
    {
        alogw("current Contrast = %d", ContrastValue);
        if(0 == pContext->mConfigParam.mContrast)
        {
            alogd("use Contrast = %d as default!", ContrastValue);
            pContext->mConfigParam.mContrast = ContrastValue;
        }
        alogd("set Contrast = %d", pContext->mConfigParam.mContrast);
        AW_MPI_UVC_SetContrast(pContext->mConfigParam.mDevName, pContext->mConfigParam.mContrast);
    }
    int HueValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetHue(pContext->mConfigParam.mDevName, &HueValue))
    {
        alogw("current Hue = %d", HueValue);
        if(0 == pContext->mConfigParam.mHue)
        {
            alogd("use Hue = %d as default!", HueValue);
            pContext->mConfigParam.mHue = HueValue;
        }
        alogd("set Hue = %d", pContext->mConfigParam.mHue);
        AW_MPI_UVC_SetHue(pContext->mConfigParam.mDevName, pContext->mConfigParam.mHue);
    }
    int SaturationValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetSaturation(pContext->mConfigParam.mDevName, &SaturationValue))
    {
        alogw("current Saturation = %d", SaturationValue);
        if(0 == pContext->mConfigParam.mSaturation)
        {
            alogd("use Saturation = %d as default!", SaturationValue);
            pContext->mConfigParam.mSaturation = SaturationValue;
        }
        alogd("set Saturation = %d", pContext->mConfigParam.mSaturation);
        AW_MPI_UVC_SetBrightness(pContext->mConfigParam.mDevName, pContext->mConfigParam.mSaturation);
    }
    int SharpnessValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetSharpness(pContext->mConfigParam.mDevName, &SharpnessValue))
    {
        alogw("current Sharpness = %d", SharpnessValue);
        if(0 == pContext->mConfigParam.mSharpness)
        {
            alogd("use Sharpness = %d as default!", SharpnessValue);
            pContext->mConfigParam.mSharpness = SharpnessValue;
        }
        alogd("set Sharpness = %d", pContext->mConfigParam.mSharpness);
        AW_MPI_UVC_SetSharpness(pContext->mConfigParam.mDevName, pContext->mConfigParam.mSharpness);
    }
    
    pContext->mUvcChn = 0;
    eRet = AW_MPI_UVC_CreateVirChn(pContext->mConfigParam.mDevName, pContext->mUvcChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %s UVC can not create virchannel[%d]", pContext->mConfigParam.mDevName, pContext->mUvcChn);
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

    //printf("------5------------------------------------\n");
    int hlay0 = 0;
    while(hlay0 < VO_MAX_LAYER_NUM)
    {
        if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
        {
            break;
        }
        hlay0++;
    }
    if(VO_MAX_LAYER_NUM == hlay0)
    {
        aloge("error: enable video layer failed");
    }

    pContext->mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(pContext->mVoLayer, &pContext->mLayerAttr);
    pContext->mLayerAttr.stDispRect.X = 0;
    pContext->mLayerAttr.stDispRect.Y = 0;
    pContext->mLayerAttr.stDispRect.Width = pContext->mConfigParam.mDisplayWidth;
    pContext->mLayerAttr.stDispRect.Height = pContext->mConfigParam.mDisplayHeight;
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
            aloge("fatal error:create vo channel[%d] ret[0x%x]", pContext->mVOChn, eRet);
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
    cbInfo.callback = (MPPCallbackFuncType)&SampleUvcVo_VOCallbackWrapper;
    AW_MPI_VO_RegisterCallback(pContext->mVoLayer, pContext->mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(pContext->mVoLayer, pContext->mVOChn, 2);

    AW_MPI_VO_StartChn(pContext->mVoLayer, pContext->mVOChn);        

    eRet = AW_MPI_UVC_StartRecvPic(pContext->mConfigParam.mDevName, pContext->mUvcChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %s UVC can not start virchannel[%d]", pContext->mConfigParam.mDevName, pContext->mUvcChn);
    }

    eRet = AW_MPI_UVC_EnableDevice(pContext->mConfigParam.mDevName);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %UVC device can not start", pContext->mConfigParam.mDevName);
    }

    //get frame from uvc, send frame to vo.
    VIDEO_FRAME_INFO_S stFrameInfo;
    while(1)
    {
        if(pContext->mConfigParam.mTestFrameCount > 0)
        {
            if(pContext->mFrameCount>=pContext->mConfigParam.mTestFrameCount)
            {
                alogd("get [%d] frames from uvc, prepare to exit!", pContext->mFrameCount);
                break;
            }
        }
        if(cdx_sem_get_val(&pContext->mSemExit) > 0)
        {
            alogd("detect user exit signal! prepare to exit!");
            break;
        }
        ERRORTYPE ret = AW_MPI_UVC_GetFrame(pContext->mConfigParam.mDevName, pContext->mUvcChn, &stFrameInfo, 500);
        if(ret != SUCCESS)
        {
            aloge("why not get frame from uvc? check code! 0x%x", ret);
            continue;
        }
        ret = AW_MPI_VO_SendFrame(pContext->mVoLayer, pContext->mVOChn, &stFrameInfo, 0);
        if(ret != SUCCESS)
        {
            aloge("why send frame to vo fail? check code! 0x%x", ret);
            continue;
        }
        pthread_mutex_lock(&pContext->mFrameCountLock);
        pContext->mFrameCount++;
        pContext->mHoldFrameNum++;
        pthread_mutex_unlock(&pContext->mFrameCountLock);
    }

    AW_MPI_UVC_StopRecvPic(pContext->mConfigParam.mDevName, pContext->mUvcChn);
    AW_MPI_VO_StopChn(pContext->mVoLayer, pContext->mVOChn);

    AW_MPI_VO_DestroyChn(pContext->mVoLayer, pContext->mVOChn);
    pContext->mVOChn = MM_INVALID_CHN;
    AW_MPI_UVC_DestroyVirChn(pContext->mConfigParam.mDevName, pContext->mUvcChn);
    pContext->mUvcChn = MM_INVALID_CHN;
    AW_MPI_UVC_DisableDevice(pContext->mConfigParam.mDevName);
    AW_MPI_UVC_DestroyDevice(pContext->mConfigParam.mDevName);
    
    AW_MPI_VO_OpenVideoLayer(pContext->mUILayer); /* open ui layer. */
    AW_MPI_VO_DisableVideoLayer(pContext->mVoLayer);
    pContext->mVoLayer = MM_INVALID_LAYER;
    AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(pContext->mVoDev);
    pContext->mVoDev = MM_INVALID_DEV;

    AW_MPI_SYS_Exit();

    if(pContext->mHoldFrameNum != 0)
    {
        aloge("fatal error! why holdframe num[%d] != 0?", pContext->mHoldFrameNum);
    }

_exit:
    destroySampleUvcVoContext(pContext);
    free(pContext);
    gpSampleUvcVoContext = pContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

