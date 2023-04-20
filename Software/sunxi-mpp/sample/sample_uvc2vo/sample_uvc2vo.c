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

#include "sample_uvc2vo.h"
#include "sample_uvc2vo_config.h"

static SampleUvc2VoContext *pSampleUvc2VoContext = NULL;

int initSampleUvc2VoContext(SampleUvc2VoContext *pContext)
{
    memset(pContext, 0, sizeof *pContext);
    pContext->mUILayer = HLAY(2, 0);
    cdx_sem_init(&pContext->mSemExit, 0);
    return 0;
}

int destroySampleUvc2VoContext(SampleUvc2VoContext *pContex)
{
    cdx_sem_deinit(&pContex->mSemExit);
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleUvc2VoCmdLineParam *pCmdLinePara)
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

static ERRORTYPE loadSampleUvc2VoConfig(SampleUvc2VoConfig *pConfig, const char *conf_path)
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
        pConfig->mTestDuration = 0; 
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
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC2VO_KEY_DEV_NAME, NULL);
        if(ptr)
        {
            strcpy(pConfig->mDevName, ptr);
        }
        else
        {
            aloge("fatal error! the uvc dev name is error!");
            return FAILURE;   
        }

        pConfig->mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_CAPTURE_WIDTH, 640);
        pConfig->mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_CAPTURE_HEIGHT, 480);
        char* pStrPixelFormat = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC2VO_KEY_PIC_FORMAT, NULL);
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
        pConfig->mCaptureVideoBufCnt = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_CAPTURE_VIDEOBUFCNT, 0);
        pConfig->mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_DISPLAY_WIDTH, 240);
        pConfig->mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_DISPLAY_HEIGHT, 320);
        pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_TEST_DURATION, 0);

        pConfig->mBrightness = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_BRIGHTNESS, 0);
        pConfig->mContrast = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_CONTRAST, 0);
        pConfig->mHue = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_HUE, 0);
        pConfig->mSaturation = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_SATURATION, 0);
        pConfig->mSharpness = GetConfParaInt(&stConfParser, SAMPLE_UVC2VO_KEY_SHARPNESS, 0);

        destroyConfParser(&stConfParser);
    }
    return SUCCESS;
}

static ERRORTYPE SampleUvc2Vo_VOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleUvc2VoContext *pContext = ( SampleUvc2VoContext*)cookie;
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
                aloge("fatal error! sample_vi2vo use binding mode!");
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

static ERRORTYPE SampleUvc2Vo_CLOCKCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    alogw("clock channel[%d] has some event[0x%x]", pChn->mChnId, event);
    return SUCCESS;
}

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleUvc2VoContext != NULL)
    {
        cdx_sem_up(&pSampleUvc2VoContext->mSemExit);
    }
}


int
main(int argc, char **argv)
{
    int result = 0;

    SampleUvc2VoContext stContext;
    initSampleUvc2VoContext(&stContext);
    pSampleUvc2VoContext = &stContext;

    if(ParseCmdLine(argc, argv, &stContext.mCmdLineParam) != 0)
    {
        result = -1;
        goto _exit;
    }

    char *pConfigFilePath = NULL;
    if(strlen(stContext.mCmdLineParam.mConfigFilePath) > 0)
    {
        pConfigFilePath = stContext.mCmdLineParam.mConfigFilePath;
    }
    if(loadSampleUvc2VoConfig(&stContext.mConfigParam, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    if (signal(SIGINT, handle_exit) == SIG_ERR)        
    {
        perror("error:can't catch SIGSEGV");
    }

    memset(&stContext.mSysconf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysconf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysconf);
    AW_MPI_SYS_Init();

	if(stContext.mConfigParam.mDevName == NULL)
    {
        aloge("fatal error! why devname == NULL?");
	}

    int eRet = AW_MPI_UVC_CreateDevice(stContext.mConfigParam.mDevName);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC device can not create!", stContext.mConfigParam.mDevName);
    }

    UVC_ATTR_S attr;
    eRet = AW_MPI_UVC_GetDeviceAttr(stContext.mConfigParam.mDevName, &attr);
    if(eRet != SUCCESS)
    {
        aloge("error: the %UVC can not get device attr", stContext.mConfigParam.mDevName);
    }

    attr.mPixelformat = stContext.mConfigParam.mPicFormat;
    attr.mUvcVideo_BufCnt = stContext.mConfigParam.mCaptureVideoBufCnt;
    attr.mUvcVideo_Width = stContext.mConfigParam.mCaptureWidth;
    attr.mUvcVideo_Height = stContext.mConfigParam.mCaptureHeight;
    attr.mUvcVideo_Fps = 30;
    eRet = AW_MPI_UVC_SetDeviceAttr(stContext.mConfigParam.mDevName, &attr);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC can not set device attr", stContext.mConfigParam.mDevName);
    }

    eRet = AW_MPI_UVC_GetDeviceAttr(stContext.mConfigParam.mDevName, &attr);
    if(eRet != SUCCESS)
    {
        aloge("error: the %UVC can not get device attr", stContext.mConfigParam.mDevName);
    }
    alogd("uvc device attr setting result: format[0x%x], size[%dx%d], fps[%d]", attr.mPixelformat, attr.mUvcVideo_Width, attr.mUvcVideo_Height, attr.mUvcVideo_Fps);

    alogw("-------> get %s uvc control param <-------", stContext.mConfigParam.mDevName);
    int BrightnessValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetBrightness(stContext.mConfigParam.mDevName, &BrightnessValue))
    {
        alogd("current brightness = %d", BrightnessValue);
        if(0 == stContext.mConfigParam.mBrightness)
        {
            alogd("use brightness = %d as default!", BrightnessValue);
            stContext.mConfigParam.mBrightness = BrightnessValue;
        }
        alogd("set brightness = %d", stContext.mConfigParam.mBrightness);
        AW_MPI_UVC_SetBrightness(stContext.mConfigParam.mDevName, stContext.mConfigParam.mBrightness);
    }
    int ContrastValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetContrast(stContext.mConfigParam.mDevName, &ContrastValue))
    {
        alogw("current Contrast = %d", ContrastValue);
        if(0 == stContext.mConfigParam.mContrast)
        {
            alogd("use Contrast = %d as default!", ContrastValue);
            stContext.mConfigParam.mContrast = ContrastValue;
        }
        alogd("set Contrast = %d", stContext.mConfigParam.mContrast);
        AW_MPI_UVC_SetContrast(stContext.mConfigParam.mDevName, stContext.mConfigParam.mContrast);
    }
    int HueValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetHue(stContext.mConfigParam.mDevName, &HueValue))
    {
        alogw("current Hue = %d", HueValue);
        if(0 == stContext.mConfigParam.mHue)
        {
            alogd("use Hue = %d as default!", HueValue);
            stContext.mConfigParam.mHue = HueValue;
        }
        alogd("set Hue = %d", stContext.mConfigParam.mHue);
        AW_MPI_UVC_SetHue(stContext.mConfigParam.mDevName, stContext.mConfigParam.mHue);
    }
    int SaturationValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetSaturation(stContext.mConfigParam.mDevName, &SaturationValue))
    {
        alogw("current Saturation = %d", SaturationValue);
        if(0 == stContext.mConfigParam.mSaturation)
        {
            alogd("use Saturation = %d as default!", SaturationValue);
            stContext.mConfigParam.mSaturation = SaturationValue;
        }
        alogd("set Saturation = %d", stContext.mConfigParam.mSaturation);
        AW_MPI_UVC_SetBrightness(stContext.mConfigParam.mDevName, stContext.mConfigParam.mSaturation);
    }
    int SharpnessValue = 0;
    if(SUCCESS == AW_MPI_UVC_GetSharpness(stContext.mConfigParam.mDevName, &SharpnessValue))
    {
        alogw("current Sharpness = %d", SharpnessValue);
        if(0 == stContext.mConfigParam.mSharpness)
        {
            alogd("use Sharpness = %d as default!", SharpnessValue);
            stContext.mConfigParam.mSharpness = SharpnessValue;
        }
        alogd("set Sharpness = %d", stContext.mConfigParam.mSharpness);
        AW_MPI_UVC_SetSharpness(stContext.mConfigParam.mDevName, stContext.mConfigParam.mSharpness);
    }
    
    
    stContext.mUvcChn = 0;
    eRet = AW_MPI_UVC_CreateVirChn(stContext.mConfigParam.mDevName, stContext.mUvcChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %s UVC can not create virchannel[%d]", stContext.mConfigParam.mDevName, stContext.mUvcChn);
    }

    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);
    VO_PUB_ATTR_S stPubAttr;
    memset(&stPubAttr, 0, sizeof(VO_PUB_ATTR_S));
    AW_MPI_VO_GetPubAttr(stContext.mVoDev, &stPubAttr);
    stPubAttr.enIntfType = VO_INTF_LCD;
    stPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(stContext.mVoDev, &stPubAttr);

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

    stContext.mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 0;
    stContext.mLayerAttr.stDispRect.Width = stContext.mConfigParam.mDisplayWidth;
    stContext.mLayerAttr.stDispRect.Height = stContext.mConfigParam.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);

    BOOL bSuccessFlag = FALSE;
    stContext.mVOChn = 0;
    while(stContext.mVOChn < VO_MAX_CHN_NUM)
    {
        eRet = AW_MPI_VO_CreateChn(stContext.mVoLayer, stContext.mVOChn);
        if(SUCCESS == eRet)
        {
            bSuccessFlag = TRUE;
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == eRet)
        {
            alogd("vo channel[%d] is exist, find next", stContext.mVOChn);
            stContext.mVOChn++; 
        }
        else
        {
            aloge("error:create vo channel[%d] ret[0x%x]", stContext.mVOChn, eRet);            
            stContext.mVOChn++; 
        }      
    }
    if(!bSuccessFlag)
    {
        stContext.mVOChn = MM_INVALID_CHN;
        aloge("error: create vo channel failed");
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void *)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleUvc2Vo_VOCallbackWrapper;
    AW_MPI_VO_RegisterCallback(stContext.mVoLayer, stContext.mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(stContext.mVoLayer, stContext.mVOChn, 0);

    MPP_CHN_S UvcChn = {MOD_ID_UVC, (int)stContext.mConfigParam.mDevName, stContext.mUvcChn}; // note: UvcChn->mDevId is the pointer of char *;
    MPP_CHN_S VoChn = {MOD_ID_VOU, stContext.mVoLayer, stContext.mVOChn};

    AW_MPI_SYS_Bind(&UvcChn, &VoChn);

    AW_MPI_VO_StartChn(stContext.mVoLayer, stContext.mVOChn);        

    eRet = AW_MPI_UVC_StartRecvPic(stContext.mConfigParam.mDevName, stContext.mUvcChn);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC can not start virchannel[%d]", stContext.mConfigParam.mDevName, stContext.mUvcChn);
    }

    eRet = AW_MPI_UVC_EnableDevice(stContext.mConfigParam.mDevName);
    if(eRet != SUCCESS)
    {
        aloge("error: the %UVC device can not start", stContext.mConfigParam.mDevName);
    }

     if(stContext.mConfigParam.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigParam.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }

    AW_MPI_VO_StopChn(stContext.mVoLayer, stContext.mVOChn);
    AW_MPI_UVC_StopRecvPic(stContext.mConfigParam.mDevName, stContext.mUvcChn);
    

    //AW_MPI_VO_Disable(stContext.mVoDev);
    AW_MPI_UVC_DestroyVirChn(stContext.mConfigParam.mDevName, stContext.mUvcChn);

    AW_MPI_UVC_DisableDevice(stContext.mConfigParam.mDevName);
    AW_MPI_UVC_DestroyDevice(stContext.mConfigParam.mDevName);

    AW_MPI_VO_DestroyChn(stContext.mVoLayer, stContext.mVOChn);
    stContext.mVOChn = MM_INVALID_CHN;
    AW_MPI_VO_OpenVideoLayer(stContext.mUILayer); /* open ui layer. */
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;

    AW_MPI_SYS_Exit();

_exit:
    destroySampleUvc2VoContext(&stContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

