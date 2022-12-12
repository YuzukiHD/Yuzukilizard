//#define LOG_NDEBUG 0
#define LOG_TAG "sample_uvc2vdenc2vo"
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

#include "sample_uvc2vdenc2vo.h"
#include "sample_uvc2vdenc2vo_config.h"


static SampleUvc2Vdenc2VoContext *pSampleUvc2Vdenc2VoContext = NULL;

int initSampleUvc2Vdenc2VoContext(SampleUvc2Vdenc2VoContext *pContext)
{
    memset(pContext, 0, sizeof *pContext);
    pContext->mUILayer = HLAY(2, 0);
    cdx_sem_init(&pContext->mSemExit, 0);
    return 0;
}

int destroySampleUvc2Vdenc2VoContext(SampleUvc2Vdenc2VoContext *pContex)
{
    cdx_sem_deinit(&pContex->mSemExit);
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleUvc2Vdenc2VoCmdLineParam *pCmdLinePara)
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

static ERRORTYPE loadSampleUvc2Vdenc2VoConfig(SampleUvc2Vdenc2VoConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;
    if(!conf_path)
    {
        alogd("user not set config file. use default test parameter!");
        strcpy(pConfig->mDevName, (void *)"/dev/video0");
        pConfig->mCaptureWidth = 1280;
        pConfig->mCaptureHeight = 720;
        pConfig->mPicFormat = UVC_MJPEG;
        pConfig->mCaptureVideoBufCnt = 5;
        pConfig->mDisplayWidth = 640;
        pConfig->mDisplayHeight = 480;
        pConfig->mTestDuration = 0;  
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

        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_DEV_NAME, NULL);
        if(ptr)
        {
            strcpy(pConfig->mDevName, ptr);
        }
        else
        {
            aloge("fatal error! the uvc dev name is error!");
            return FAILURE;   
        }
        char* pStrPixelFormat = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_PIC_FORMAT, NULL);
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
        pConfig->mCaptureVideoBufCnt = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_CAPTURE_VIDEOBUFCNT, 0);
        pConfig->mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_CAPTURE_WIDTH, 640);
        pConfig->mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_CAPTURE_HEIGHT, 480);
        pConfig->mCaptureFrameRate = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_CAPTURE_FRAMERATE, 0);
        pConfig->mDecodeOutWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_DECODE_OUT_WIDTH, 0);
        pConfig->mDecodeOutHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_DECODE_OUT_HEIGHT, 0);
        pConfig->mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_DISPLAY_WIDTH, 640);
        pConfig->mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_DISPLAY_HEIGHT, 480);
        pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_UVC2VDENC2VO_KEY_TEST_DURATION, 0);

        destroyConfParser(&stConfParser);
    }
    return SUCCESS;
}

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleUvc2Vdenc2VoContext != NULL)
    {
        cdx_sem_up(&pSampleUvc2Vdenc2VoContext->mSemExit);
    }
}


int
main(int argc, char **argv)
{
    int result = 0;

    SampleUvc2Vdenc2VoContext stContext;
    initSampleUvc2Vdenc2VoContext(&stContext);
    pSampleUvc2Vdenc2VoContext = &stContext;

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
    if(loadSampleUvc2Vdenc2VoConfig(&stContext.mConfigParam, pConfigFilePath) != SUCCESS)
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

    int eRet = AW_MPI_UVC_CreateDevice(stContext.mConfigParam.mDevName);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC device can not create!", stContext.mConfigParam.mDevName);
    }
    UVC_ATTR_S attr;
    memset(&attr, 0, sizeof attr);
    attr.mPixelformat = stContext.mConfigParam.mPicFormat;
    attr.mUvcVideo_BufCnt = stContext.mConfigParam.mCaptureVideoBufCnt;
    attr.mUvcVideo_Width = stContext.mConfigParam.mCaptureWidth;
    attr.mUvcVideo_Height = stContext.mConfigParam.mCaptureHeight;
    attr.mUvcVideo_Fps = stContext.mConfigParam.mCaptureFrameRate;
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
    
    stContext.mUvcChn = 0;
    eRet = AW_MPI_UVC_CreateVirChn(stContext.mConfigParam.mDevName, stContext.mUvcChn);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC can not create virchannel[%d]", stContext.mConfigParam.mDevName, stContext.mUvcChn);
    }

    stContext.mVdecChn = 0;
    VDEC_CHN_ATTR_S VdecChnAttr;
    memset(&VdecChnAttr, 0, sizeof VdecChnAttr);
    VdecChnAttr.mType = PT_MJPEG;
    VdecChnAttr.mPicWidth = stContext.mConfigParam.mDecodeOutWidth;
    VdecChnAttr.mPicHeight = stContext.mConfigParam.mDecodeOutHeight;
    VdecChnAttr.mInitRotation = ROTATE_NONE;
    VdecChnAttr.mOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
    VdecChnAttr.mVdecVideoAttr.mSupportBFrame = 1;
    eRet = AW_MPI_VDEC_CreateChn(stContext.mVdecChn, &VdecChnAttr);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s uvc can not create vdec chn[%s]", stContext.mConfigParam.mDevName, stContext.mVdecChn);
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
    //stContext.mLayerAttr.mDispFrmRt = 30;
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
    AW_MPI_VO_SetChnDispBufNum(stContext.mVoLayer, stContext.mVOChn, 2);

    MPP_CHN_S UvcChn = {MOD_ID_UVC, (int)stContext.mConfigParam.mDevName, stContext.mUvcChn}; // note: UvcChn->mDevId is the pointer of char *;
    MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, stContext.mVdecChn};
    MPP_CHN_S VoChn = {MOD_ID_VOU, stContext.mVoLayer, stContext.mVOChn};

    AW_MPI_SYS_Bind(&UvcChn, &VdecChn);
    AW_MPI_SYS_Bind(&VdecChn, &VoChn);

    AW_MPI_UVC_StartRecvPic(stContext.mConfigParam.mDevName, stContext.mUvcChn);
    AW_MPI_UVC_EnableDevice(stContext.mConfigParam.mDevName);
    AW_MPI_VDEC_StartRecvStream(stContext.mVdecChn);
    AW_MPI_VO_StartChn(stContext.mVoLayer, stContext.mVOChn);

    if(stContext.mConfigParam.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigParam.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }
    
    AW_MPI_VO_StopChn(stContext.mVoLayer, stContext.mVOChn);
    AW_MPI_VDEC_StopRecvStream(stContext.mVdecChn);
    AW_MPI_UVC_StopRecvPic(stContext.mConfigParam.mDevName, stContext.mUvcChn);

    AW_MPI_VO_DestroyChn(stContext.mVoLayer, stContext.mVOChn);
    stContext.mVOChn = MM_INVALID_CHN;
    AW_MPI_VDEC_DestroyChn(stContext.mVdecChn);
    stContext.mVdecChn = MM_INVALID_CHN;
    AW_MPI_UVC_DestroyVirChn(stContext.mConfigParam.mDevName, stContext.mUvcChn);
    stContext.mUvcChn = MM_INVALID_CHN;
    
    AW_MPI_UVC_DisableDevice(stContext.mConfigParam.mDevName);
    AW_MPI_UVC_DestroyDevice(stContext.mConfigParam.mDevName);
    //stContext.mConfigParam.mDevName = NULL; 
    
    
    AW_MPI_VO_OpenVideoLayer(stContext.mUILayer); /* open ui layer. */
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;
    
    AW_MPI_SYS_Exit();
    
_exit:
    destroySampleUvc2Vdenc2VoContext(&stContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;

}
