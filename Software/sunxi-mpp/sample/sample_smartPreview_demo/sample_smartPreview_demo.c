/******************************************************************************
  Copyright (C), 2001-2022, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_smartPreview_demo.c
  Version       : Initial Draft
  Author        : Allwinner
  Created       : 2022/6/22
  Last Modified :
  Description   : Demonstrate Smart Preview scenarios
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "sample_smartPreview_demo"
#include <utils/plat_log.h>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h> 
#include <sys/mman.h> 
#include <sys/ioctl.h> 

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"

#include <mpi_videoformat_conversion.h>
#include <confparser.h>
#include "sample_smartPreview_demo_config.h"
#include "sample_smartPreview_demo.h"
#include "mpi_isp.h"
#include "aiservice.h"
#include <awnn_det.h>

static SampleSmartPreviewDemoContext *gpSampleSmartPreviewDemoContext = NULL;

static int ParseCmdLine(int argc, char **argv, SampleSmartPreviewDemoCmdLineParam *pCmdLinePara)
{
    alogd("sample_virvi2vo path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleSmartPreviewDemoCmdLineParam));
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
            printf("CmdLine param example:\n"
                "\t run -path /home/sample_virvi2vo.conf\n");
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

static void LoadVIPP2VOConfig(int idx, VIPP2VOConfig *pConfig, CONFPARSER_S *pConfParser)
{
    char *ptr = NULL;
    if(0 == idx)
    {
        pConfig->mIspDev        = GetConfParaInt(pConfParser, CFG_MainISP_DEV, 0);
        pConfig->mVippDev        = GetConfParaInt(pConfParser, CFG_MainVIPP_DEV, 0);
        pConfig->mCaptureWidth  = GetConfParaInt(pConfParser, CFG_MainCAPTURE_WIDTH, 0);
        pConfig->mCaptureHeight = GetConfParaInt(pConfParser, CFG_MainCAPTURE_HEIGHT, 0);
        pConfig->mDisplayX   = GetConfParaInt(pConfParser, CFG_MainDISPLAY_X, 0);
        pConfig->mDisplayY   = GetConfParaInt(pConfParser, CFG_MainDISPLAY_Y, 0);
        pConfig->mDisplayWidth  = GetConfParaInt(pConfParser, CFG_MainDISPLAY_WIDTH, 0);
        pConfig->mDisplayHeight = GetConfParaInt(pConfParser, CFG_MainDISPLAY_HEIGHT, 0);
        pConfig->mLayerNum = GetConfParaInt(pConfParser, CFG_MainLAYER_NUM, 0);
        pConfig->mNnNbgType = GetConfParaInt(pConfParser, CFG_MainNnNbgType, 0);
        pConfig->mNnIsp = GetConfParaInt(pConfParser, CFG_MainNnIsp, 0);
        pConfig->mNnVipp = GetConfParaInt(pConfParser, CFG_MainNnVipp, 0);
        pConfig->mNnViBufNum = GetConfParaInt(pConfParser, CFG_MainNnViBufNum, 0);
        pConfig->mNnSrcFrameRate = GetConfParaInt(pConfParser, CFG_MainNnSrcFrameRate, 0);
        ptr = (char*)GetConfParaString(pConfParser, CFG_MainNnNbgFilePath, NULL);
        strncpy(pConfig->mNnNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mNnDrawOrlEnable = GetConfParaInt(pConfParser, CFG_MainNnDrawOrlEnable, 0);
    }
    else if(1 == idx)
    {
        pConfig->mIspDev        = GetConfParaInt(pConfParser, CFG_SubISP_DEV, 0);
        pConfig->mVippDev        = GetConfParaInt(pConfParser, CFG_SubVIPP_DEV, 0);
        pConfig->mCaptureWidth  = GetConfParaInt(pConfParser, CFG_SubCAPTURE_WIDTH, 0);
        pConfig->mCaptureHeight = GetConfParaInt(pConfParser, CFG_SubCAPTURE_HEIGHT, 0);
        pConfig->mDisplayX   = GetConfParaInt(pConfParser, CFG_SubDISPLAY_X, 0);
        pConfig->mDisplayY   = GetConfParaInt(pConfParser, CFG_SubDISPLAY_Y, 0);
        pConfig->mDisplayWidth  = GetConfParaInt(pConfParser, CFG_SubDISPLAY_WIDTH, 0);
        pConfig->mDisplayHeight = GetConfParaInt(pConfParser, CFG_SubDISPLAY_HEIGHT, 0);
        pConfig->mLayerNum = GetConfParaInt(pConfParser, CFG_SubLAYER_NUM, 0);
        pConfig->mNnNbgType = GetConfParaInt(pConfParser, CFG_SubNnNbgType, 0);
        pConfig->mNnIsp = GetConfParaInt(pConfParser, CFG_SubNnIsp, 0);
        pConfig->mNnVipp = GetConfParaInt(pConfParser, CFG_SubNnVipp, 0);
        pConfig->mNnViBufNum = GetConfParaInt(pConfParser, CFG_SubNnViBufNum, 0);
        pConfig->mNnSrcFrameRate = GetConfParaInt(pConfParser, CFG_SubNnSrcFrameRate, 0);
        ptr = (char*)GetConfParaString(pConfParser, CFG_SubNnNbgFilePath, NULL);
        strncpy(pConfig->mNnNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mNnDrawOrlEnable = GetConfParaInt(pConfParser, CFG_SubNnDrawOrlEnable, 0);
    }
    else
    {
        aloge("fatal error! need add vipp[%d] config!", idx);
    }
}

static ERRORTYPE loadSampleConfig(SampleSmartPreviewDemoConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;

    if(NULL == conf_path)
    {
        alogd("user not set config file. use default test parameter!");
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleSmartPreviewDemoConfig));
    int i;
    for(i=0;i<VIPP2VO_NUM;i++)
    {
        LoadVIPP2VOConfig(i, &pConfig->mVIPP2VOConfigArray[i], &stConfParser);
    }

    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, CFG_PIC_FORMAT, NULL);
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
    else if(!strcmp(pStrPixelFormat, "nv21s"))
    {
        pConfig->mPicFormat = MM_PIXEL_FORMAT_AW_NV21S;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    char *pStrDispType = (char*)GetConfParaString(&stConfParser, CFG_DISP_TYPE, NULL);
    if (!strcmp(pStrDispType, "hdmi"))
    {
        pConfig->mDispType = VO_INTF_HDMI;
        if (pConfig->mVIPP2VOConfigArray[0].mDisplayWidth > 1920)
            pConfig->mDispSync = VO_OUTPUT_3840x2160_30;
        else if (pConfig->mVIPP2VOConfigArray[0].mDisplayWidth > 1280)
            pConfig->mDispSync = VO_OUTPUT_1080P30;
        else
            pConfig->mDispSync = VO_OUTPUT_720P60;
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

    pConfig->mFrameRate = GetConfParaInt(&stConfParser, CFG_FRAME_RATE, 0);
    pConfig->mTestDuration = GetConfParaInt(&stConfParser, CFG_TEST_DURATION, 0);

    for(i=0;i<VIPP2VO_NUM;i++)
    {
        alogd("vipp[%d]: captureSize[%dx%d], displayArea[%d,%d,%dx%d]", 
            pConfig->mVIPP2VOConfigArray[i].mVippDev, pConfig->mVIPP2VOConfigArray[i].mCaptureWidth, pConfig->mVIPP2VOConfigArray[i].mCaptureHeight,
            pConfig->mVIPP2VOConfigArray[i].mDisplayX, pConfig->mVIPP2VOConfigArray[i].mDisplayY, pConfig->mVIPP2VOConfigArray[i].mDisplayWidth, pConfig->mVIPP2VOConfigArray[i].mDisplayHeight);
    }
    alogd("dispSync[%d], dispType[%d], frameRate[%d], testDuration[%d]", pConfig->mDispSync, pConfig->mDispType, pConfig->mFrameRate, pConfig->mTestDuration);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleSmartPreviewDemoContext)
    {
        cdx_sem_up(&gpSampleSmartPreviewDemoContext->mSemExit);
    }
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleSmartPreviewDemoContext *pContext = (SampleSmartPreviewDemoContext*)cookie;
    if(MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                alogd("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if(MOD_ID_VOU == pChn->mModId)
    {
        alogd("VO callback: VO Layer[%d] chn[%d] event:%d", pChn->mDevId, pChn->mChnId, event);
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                aloge("vo layer[%d] release frame id[0x%x]!", pChn->mDevId, pFrameInfo->mId);
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("vo layer[%d] report video display size[%dx%d]", pChn->mDevId, pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                alogd("vo layer[%d] report rendering start", pChn->mDevId);
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
        aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
    }
    return ret;
}

ERRORTYPE CreateVIPP2VOLink(int idx, SampleSmartPreviewDemoContext *pContext)
{
    ERRORTYPE result = SUCCESS;
    VIPP2VOConfig *pConfig = &pContext->mConfigPara.mVIPP2VOConfigArray[idx];
    VIPP2VOLinkInfo *pLinkInfo = &pContext->mLinkInfoArray[idx];
    if(0 == pConfig->mCaptureWidth || 0 == pConfig->mCaptureHeight)
    {
        alogd("do not need create link for idx[%d]", idx);
        return result;
    }
    /* create vi channel */
    pLinkInfo->mVIDev = pConfig->mVippDev;
    pLinkInfo->mVIChn = 0;
    alogd("Vipp dev[%d] vir_chn[%d]", pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    ERRORTYPE eRet = AW_MPI_VI_CreateVipp(pLinkInfo->mVIDev);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp:%d failed", pLinkInfo->mVIDev);
        result = FAILURE;
        goto _err0;
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    eRet = AW_MPI_VI_RegisterCallback(pLinkInfo->mVIDev, &cbInfo);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! vipp[%d] RegisterCallback failed", pLinkInfo->mVIDev);
    }
    VI_ATTR_S stAttr;
    eRet = AW_MPI_VI_GetVippAttr(pLinkInfo->mVIDev, &stAttr);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI GetVippAttr failed");
    }
    memset(&stAttr, 0, sizeof(VI_ATTR_S));
    stAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    stAttr.memtype = V4L2_MEMORY_MMAP;
    stAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mPicFormat);
    stAttr.format.field = V4L2_FIELD_NONE;
    stAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    stAttr.format.width = pConfig->mCaptureWidth;
    stAttr.format.height =pConfig->mCaptureHeight;
    stAttr.nbufs = 5;
    stAttr.nplanes = 2;
    stAttr.drop_frame_num = 0; // drop 2 second video data, default=0
    stAttr.mbEncppEnable = TRUE;
    /* do not use current param, if set to 1, all this configuration will
     * not be used.
     */
    if(0 == idx)
    {
        stAttr.use_current_win = 0;
    }
    else
    {
        stAttr.use_current_win = 1;
    }
    stAttr.fps = pContext->mConfigPara.mFrameRate;
    eRet = AW_MPI_VI_SetVippAttr(pLinkInfo->mVIDev, &stAttr);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr:%d failed", pLinkInfo->mVIDev);
    }
    AW_MPI_ISP_Run(pConfig->mIspDev);
    eRet = AW_MPI_VI_CreateVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn, NULL);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pLinkInfo->mVIChn);
    }
    eRet = AW_MPI_VI_EnableVipp(pLinkInfo->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
        result = FAILURE;
        goto _err1;
    }
    //if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations must be after stream_on.
    //AW_MPI_VI_SetVippMirror(pLinkInfo->mVIDev, 0);
    //AW_MPI_VI_SetVippFlip(pLinkInfo->mVIDev, 0);

    /* enable vo layer */
    int hlay0 = 0;
    hlay0 = pConfig->mLayerNum;
    if(SUCCESS != AW_MPI_VO_EnableVideoLayer(hlay0))
    {
        aloge("fatal error! enable video layer[%d] fail!", hlay0);
        hlay0 = MM_INVALID_LAYER;
        pLinkInfo->mVoLayer = hlay0;
        result = FAILURE;
        goto _err1_5;
    }
    pLinkInfo->mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(pLinkInfo->mVoLayer, &pLinkInfo->mLayerAttr);
    pLinkInfo->mLayerAttr.stDispRect.X = pConfig->mDisplayX;
    pLinkInfo->mLayerAttr.stDispRect.Y = pConfig->mDisplayY;
    pLinkInfo->mLayerAttr.stDispRect.Width = pConfig->mDisplayWidth;
    pLinkInfo->mLayerAttr.stDispRect.Height = pConfig->mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(pLinkInfo->mVoLayer, &pLinkInfo->mLayerAttr);

    BOOL bSuccessFlag = FALSE;
    pLinkInfo->mVOChn = 0;
    while(pLinkInfo->mVOChn < VO_MAX_CHN_NUM)
    {
        eRet = AW_MPI_VO_CreateChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);
        if(SUCCESS == eRet)
        {
            bSuccessFlag = TRUE;
            alogd("create vo channel[%d] success!", pLinkInfo->mVOChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == eRet)
        {
            alogd("vo channel[%d] is exist, find next!", pLinkInfo->mVOChn);
            pLinkInfo->mVOChn++;
        }
        else
        {
            aloge("fatal error! create vo channel[%d] ret[0x%x]!", pLinkInfo->mVOChn, eRet);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        pLinkInfo->mVOChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        result = FAILURE;
        goto _err2;
    }
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VO_RegisterCallback(pLinkInfo->mVoLayer, pLinkInfo->mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(pLinkInfo->mVoLayer, pLinkInfo->mVOChn, 2);

    MPP_CHN_S VOChn = {MOD_ID_VOU, pLinkInfo->mVoLayer, pLinkInfo->mVOChn};
    MPP_CHN_S VIChn = {MOD_ID_VIU, pLinkInfo->mVIDev, pLinkInfo->mVIChn};
    AW_MPI_SYS_Bind(&VIChn, &VOChn);

    /* start vo, vi_channel. */
    eRet = AW_MPI_VI_EnableVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! enableVirChn fail!");
        result = FAILURE;
        goto _err3;
    }
    AW_MPI_VO_StartChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);
    return result;

_err3:
    eRet = AW_MPI_SYS_UnBind(&VIChn, &VOChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! ViChn && VoChn SYS_UnBind fail!");
    }
    eRet = AW_MPI_VO_DestroyChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! Vo Disable Chn fail!");
    }
_err2:
    eRet = AW_MPI_VO_DisableVideoLayer(pLinkInfo->mVoLayer);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableVideoLayer fail!");
    }
_err1_5:
    eRet = AW_MPI_VI_DestroyVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VI DestoryVirChn fail!");
    }
#if ISP_RUN
    eRet = AW_MPI_ISP_Stop(pConfig->mIspDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! ISP Stop fail!");
    }
#endif
_err1:
    eRet = AW_MPI_VI_DestroyVipp(pLinkInfo->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VI DestoryVipp fail!");
    }
_err0:
    return result;

}

ERRORTYPE DestroyVIPP2VOLink(int idx, SampleSmartPreviewDemoContext *pContext)
{
    ERRORTYPE eRet;
    VIPP2VOConfig *pConfig = &pContext->mConfigPara.mVIPP2VOConfigArray[idx];
    VIPP2VOLinkInfo *pLinkInfo = &pContext->mLinkInfoArray[idx];
    if(0 == pConfig->mCaptureWidth || 0 == pConfig->mCaptureHeight)
    {
        alogd("do not need destroy link for idx[%d]", idx);
        return SUCCESS;
    }
    /* stop vo channel, vi channel */
    eRet = AW_MPI_VO_StopChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO StopChn fail!");
    }
    eRet = AW_MPI_VI_DisableVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VI DisableVirChn fail!");
    }
    eRet = AW_MPI_VO_DestroyChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
    pLinkInfo->mVOChn = MM_INVALID_CHN;
    /* disable vo layer */
    eRet = AW_MPI_VO_DisableVideoLayer(pLinkInfo->mVoLayer);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
    pLinkInfo->mVoLayer = -1;
    //wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then vdec can free frame buffer.
    usleep(50*1000);

    eRet = AW_MPI_VI_DestroyVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VI DestoryVirChn fail!");
    }
    eRet = AW_MPI_ISP_Stop(pConfig->mIspDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
    eRet = AW_MPI_VI_DisableVipp(pLinkInfo->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
    eRet = AW_MPI_VI_DestroyVipp(pLinkInfo->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
    pLinkInfo->mVIDev = MM_INVALID_DEV;
    pLinkInfo->mVIChn = MM_INVALID_CHN;
    return SUCCESS;
}

static void configAiService(SampleSmartPreviewDemoConfig *pConfig, ai_service_attr_t *pNnAttr)
{
    // main
    if (pConfig->mVIPP2VOConfigArray[0].mVippDev >= 0)
    {
        pNnAttr->ch_info[0].ch_idx = 0;
        if (0 == pConfig->mVIPP2VOConfigArray[0].mNnNbgType)
        {
            pNnAttr->ch_info[0].nbg_type = AWNN_HUMAN_DET;
            pNnAttr->ch_info[0].src_width = NN_HUMAN_SRC_WIDTH;
            pNnAttr->ch_info[0].src_height = NN_HUMAN_SRC_HEIGHT;
        }
        else if (1 == pConfig->mVIPP2VOConfigArray[0].mNnNbgType)
        {
            pNnAttr->ch_info[0].nbg_type = AWNN_FACE_DET;
            pNnAttr->ch_info[0].src_width = NN_FACE_SRC_WIDTH;
            pNnAttr->ch_info[0].src_height = NN_FACE_SRC_HEIGHT;
        }
        else
        {
            pNnAttr->ch_info[0].nbg_type = AWNN_TYPE_MAX;
        }
        pNnAttr->ch_info[0].src_width = NN_HUMAN_SRC_WIDTH;
        pNnAttr->ch_info[0].src_height = NN_HUMAN_SRC_HEIGHT;
        pNnAttr->ch_info[0].isp = pConfig->mVIPP2VOConfigArray[0].mNnIsp;
        pNnAttr->ch_info[0].vipp = pConfig->mVIPP2VOConfigArray[0].mNnVipp;
        pNnAttr->ch_info[0].viChn = 0;
        pNnAttr->ch_info[0].pixel_format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pNnAttr->ch_info[0].vi_buf_num = pConfig->mVIPP2VOConfigArray[0].mNnViBufNum;
        pNnAttr->ch_info[0].src_frame_rate = pConfig->mVIPP2VOConfigArray[0].mNnSrcFrameRate;
        strncpy(pNnAttr->ch_info[0].model_file, pConfig->mVIPP2VOConfigArray[0].mNnNbgFilePath, NN_NBG_MAX_FILE_PATH_SIZE);
        pNnAttr->ch_info[0].draw_orl_enable = pConfig->mVIPP2VOConfigArray[0].mNnDrawOrlEnable;
        pNnAttr->ch_info[0].draw_orl_vipp = pConfig->mVIPP2VOConfigArray[0].mVippDev;
        pNnAttr->ch_info[0].draw_orl_src_width = pConfig->mVIPP2VOConfigArray[0].mCaptureWidth;
        pNnAttr->ch_info[0].draw_orl_src_height = pConfig->mVIPP2VOConfigArray[0].mCaptureHeight;
        pNnAttr->ch_info[1].region_hdl_base = 10;
    }
    else
    {
        pNnAttr->ch_info[0].ch_idx = 0;
        pNnAttr->ch_info[0].nbg_type = AWNN_TYPE_MAX;
    }
    // sub
    if (pConfig->mVIPP2VOConfigArray[1].mVippDev >= 0)
    {
        pNnAttr->ch_info[1].ch_idx = 1;
        if (0 == pConfig->mVIPP2VOConfigArray[1].mNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_HUMAN_DET;
            pNnAttr->ch_info[1].src_width = NN_HUMAN_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_HUMAN_SRC_HEIGHT;
        }
        else if (1 == pConfig->mVIPP2VOConfigArray[1].mNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_FACE_DET;
            pNnAttr->ch_info[1].src_width = NN_FACE_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_FACE_SRC_HEIGHT;
        }
        else
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_TYPE_MAX;
        }
        pNnAttr->ch_info[1].src_width = NN_HUMAN_SRC_WIDTH;
        pNnAttr->ch_info[1].src_height = NN_HUMAN_SRC_HEIGHT;
        pNnAttr->ch_info[1].isp = pConfig->mVIPP2VOConfigArray[1].mNnIsp;
        pNnAttr->ch_info[1].vipp = pConfig->mVIPP2VOConfigArray[1].mNnVipp;
        pNnAttr->ch_info[1].viChn = 0;
        pNnAttr->ch_info[1].pixel_format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pNnAttr->ch_info[1].vi_buf_num = pConfig->mVIPP2VOConfigArray[1].mNnViBufNum;
        pNnAttr->ch_info[1].src_frame_rate = pConfig->mVIPP2VOConfigArray[1].mNnSrcFrameRate;
        strncpy(pNnAttr->ch_info[1].model_file, pConfig->mVIPP2VOConfigArray[1].mNnNbgFilePath, NN_NBG_MAX_FILE_PATH_SIZE);
        pNnAttr->ch_info[1].draw_orl_enable = pConfig->mVIPP2VOConfigArray[1].mNnDrawOrlEnable;
        pNnAttr->ch_info[1].draw_orl_vipp = pConfig->mVIPP2VOConfigArray[1].mVippDev;
        pNnAttr->ch_info[1].draw_orl_src_width = pConfig->mVIPP2VOConfigArray[1].mCaptureWidth;
        pNnAttr->ch_info[1].draw_orl_src_height = pConfig->mVIPP2VOConfigArray[1].mCaptureHeight;
        pNnAttr->ch_info[1].region_hdl_base = 20;
    }
    else
    {
        pNnAttr->ch_info[1].ch_idx = 1;
        pNnAttr->ch_info[1].nbg_type = AWNN_TYPE_MAX;
    }
}

int main(int argc, char *argv[])
{
    int result = 0;

    SampleSmartPreviewDemoContext *pContext = (SampleSmartPreviewDemoContext*)malloc(sizeof(SampleSmartPreviewDemoContext));
    if (NULL == pContext)
    {
        aloge("fatal error! malloc pContext failed! size=%d", sizeof(SampleSmartPreviewDemoContext));
        return -1;
    }
    gpSampleSmartPreviewDemoContext = pContext;
    memset(pContext, 0, sizeof(SampleSmartPreviewDemoContext));

    alogd("sample_smartPreview_demo running!\n");
    pContext->mUILayer = HLAY(2, 0);
    cdx_sem_init(&pContext->mSemExit, 0);

    /* parse command line param */
    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }

    /* parse config file. */
    if(loadSampleConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        perror("can't catch SIGSEGV");
    }

    /* init mpp system */
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    result = AW_MPI_SYS_Init();
    if (result) {
        aloge("fatal error! AW_MPI_SYS_Init failed");
        goto _exit;
    }

    /* enable vo dev */
    pContext->mVoDev = 0;
    AW_MPI_VO_Enable(pContext->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pContext->mUILayer); /* close ui layer. */
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(pContext->mVoDev, &spPubAttr);
    spPubAttr.enIntfType = pContext->mConfigPara.mDispType;
    spPubAttr.enIntfSync = pContext->mConfigPara.mDispSync;
    AW_MPI_VO_SetPubAttr(pContext->mVoDev, &spPubAttr);

    for(int i=0;i<VIPP2VO_NUM;i++)
    {
        CreateVIPP2VOLink(i, pContext);
    }

    ai_service_attr_t aiservice_attr;
    memset(&aiservice_attr, 0, sizeof(ai_service_attr_t));
    configAiService(&pContext->mConfigPara, &aiservice_attr);
    ai_service_start(&aiservice_attr);

    if(pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    ai_service_stop();

    for(int i=0;i<VIPP2VO_NUM;i++)
    {
        DestroyVIPP2VOLink(i, pContext);
    }

    AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);
    AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(pContext->mVoDev);
    pContext->mVoDev = -1;

    /* exit mpp system */
    AW_MPI_SYS_Exit();
_exit:
    cdx_sem_deinit(&pContext->mSemExit);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
