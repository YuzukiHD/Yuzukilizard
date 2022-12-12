//#define LOG_NDEBUG 0
#define LOG_TAG "sample_virvi2vo_zoom"
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

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"

#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <confparser.h>

#include "mpi_isp.h"

#include "sample_virvi2vo_zoom_config.h"
#include "sample_virvi2vo_zoom.h"

#define ISP_RUN 1

static SampleVIRVI2VOZoomContext *gpVIRVI2VOZoomContext = NULL;

static int initContext(SampleVIRVI2VOZoomContext *pContext)
{
    memset(pContext, 0, sizeof(SampleVIRVI2VOZoomContext));
    pContext->mUILayer = HLAY(2, 0);
    cdx_sem_init(&pContext->mSemExit, 0);
    return 0;
}

static int destroyContext(SampleVIRVI2VOZoomContext *pContext)
{
    cdx_sem_deinit(&pContext->mSemExit);
    return 0;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleVIRVI2VOZoomContext *pContext = (SampleVIRVI2VOZoomContext*)cookie;
    if (MOD_ID_VOU == pChn->mModId)
    {
        if ((pContext) && (pChn->mChnId != pContext->mVOChn))
        {
            aloge("fatal error! VO chnId[%d]!=[%d]", pChn->mChnId, pContext->mVOChn);
        }

        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                // aloge("fatal error! sample_virvi2vo_zoom use binding mode!");
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

static int ParseCmdLine(int argc, char **argv, SampleVIRVI2VOZoomCmdLineParam *pCmdLinePara)
{
    alogd("sample_virvi2vo_zoom path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleVIRVI2VOZoomCmdLineParam));
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
                "\t run -path /mnt/extsd/sample_virvi2vo_zoom.conf\n");
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

static ERRORTYPE loadConfig(SampleVIRVI2VOZoomConfig *pConfig, const char *conf_path)
{
    int ret;
    if(NULL == conf_path)
    {
        alogd("user not set config file. use default test parameter!");
        pConfig->mDevNum = 3;
        pConfig->mCaptureWidth = 1920;
        pConfig->mCaptureHeight = 1080;
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        pConfig->mFrameRate = 30;
        pConfig->mTestDuration = 0;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleVIRVI2VOZoomConfig));
    pConfig->mDevNum        = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_DEVICE_NUMBER, 0);
    pConfig->mCaptureWidth  = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_CAPTURE_WIDTH, 0);
    pConfig->mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_CAPTURE_HEIGHT, 0);
    pConfig->mDisplayWidth  = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_DISPLAY_WIDTH, 0);
    pConfig->mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_DISPLAY_HEIGHT, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_VIRVI2VO_KEY_PIC_FORMAT, NULL);
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
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    char *pStrDispType = (char*)GetConfParaString(&stConfParser, SAMPLE_VIRVI2VO_KEY_DISP_TYPE, NULL);
    if (!strcmp(pStrDispType, "hdmi"))
    {
        pConfig->mDispType = VO_INTF_HDMI;
        if (pConfig->mDisplayWidth > 1920)
            pConfig->mDispSync = VO_OUTPUT_3840x2160_30;
        else if (pConfig->mDisplayWidth > 1280)
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

    pConfig->mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_FRAME_RATE, 0);
    pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_TEST_DURATION, 0);

    alogd("pConfig->mCaptureWidth=[%d], pConfig->mCaptureHeight=[%d], pConfig->mDevNum=[%d]",
        pConfig->mCaptureWidth, pConfig->mCaptureHeight, pConfig->mDevNum);
    alogd("pConfig->mDisplayWidth=[%d], pConfig->mDisplayHeight=[%d], pConfig->mDispSync=[%d], pConfig->mDispType=[%d]",
        pConfig->mDisplayWidth, pConfig->mDisplayHeight, pConfig->mDispSync, pConfig->mDispType);
    alogd("pConfig->mFrameRate=[%d], pConfig->mTestDuration=[%d]",
        pConfig->mFrameRate, pConfig->mTestDuration);

    pConfig->mZoomSpeed = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_ZOOM_SPEED, 0);
    pConfig->mZoomMaxCnt = GetConfParaInt(&stConfParser, SAMPLE_VIRVI2VO_KEY_ZOOM_MAX_CNT, 0);
    alogd("pConfig->mZoomSpeed=[%d], pConfig->mZoomMaxCnt=[%d]", pConfig->mZoomSpeed, pConfig->mZoomMaxCnt);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");

    if (NULL != gpVIRVI2VOZoomContext)
    {
        cdx_sem_up(&gpVIRVI2VOZoomContext->mSemExit);
    }
}

/**
 * This is just a calculation method of scaling parameters, we can fit a specific curve as needed.
 */
static int calculateCrop(RECT_S *rect, int zoomCnt, SampleVIRVI2VOZoomContext *pContext)
{
    if (NULL == pContext)
    {
        aloge("invalid input param, pContext=NULL");
        return -1;
    }

	int zoomSpeed = pContext->mConfigPara.mZoomSpeed;
    if ((0 == zoomSpeed) && (0 == zoomCnt))
    {
        aloge("invalid nZoomSpeed=0 and zoomCnt=0");
        return -1;
    }

    int actualWidth = pContext->mActualCaptureWidth;
    int actualHeight = pContext->mActualCaptureHeight;
    int displayWidth = pContext->mConfigPara.mDisplayWidth;
    int displayHeight = pContext->mConfigPara.mDisplayHeight;

    rect->X = (actualWidth - actualWidth * zoomSpeed / (zoomSpeed + zoomCnt)) / 2;
    rect->Y = (actualHeight - actualHeight * zoomSpeed / (zoomSpeed + zoomCnt)) / 2;
	rect->X = (rect->X % 16) ? (rect->X / 16 * 16) : rect->X;
	rect->Y = (rect->Y % 8) ? (rect->Y / 8 * 8) : rect->Y;
    rect->Width = actualWidth - rect->X * 2;
    //rect->Height = actualHeight - rect->Y * 2;
	rect->Width = (rect->Width % 16) ? (rect->Width / 16 * 16) : rect->Width;
	//rect->Height = (rect->Height % 8) ? (rect->Height / 8 * 8) : rect->Height;

	// The aspect ratio after fixed zoom is consistent with the display.
    if (displayHeight)
    {
        rect->Height = rect->Width * displayWidth / displayHeight;
        rect->Height = (rect->Height % 8) ? (rect->Height / 8 * 8) : rect->Height;
    }
    else
    {
        rect->Height = 0;
    }

    /* skip parameters with the same X coordinates to avoid video lag. */
    if ((pContext->mLastRect.X != 0) && (pContext->mLastRect.X == rect->X))
    {
        //alogd("[skip] X: %d, Y: %d, W: %d, H: %d ", rect->X, rect->Y, rect->Width, rect->Height);
        return -1;
    }
    alogd("X: %d, Y: %d, W: %d, H: %d ", rect->X, rect->Y, rect->Width, rect->Height);

    memcpy(&pContext->mLastRect, rect, sizeof(RECT_S));

    return 0;
}

static void getActualCaptureArea(SampleVIRVI2VOZoomContext *pContext)
{
    VIDEO_FRAME_INFO_S buffer;
    memset(&buffer, 0, sizeof(buffer));
    AW_MPI_VI_GetFrame(pContext->mVIDev, 0, &buffer, 500);
    pContext->mActualCaptureWidth = buffer.VFrame.mOffsetRight - buffer.VFrame.mOffsetLeft;
    pContext->mActualCaptureHeight = buffer.VFrame.mOffsetBottom - buffer.VFrame.mOffsetTop;
    AW_MPI_VI_ReleaseFrame(pContext->mVIDev, 0, &buffer);
}

static void *VoZoomThread(void *pThreadData)
{
    int ret = 0;
    RECT_S mRectCrop;
    VIDEO_FRAME_INFO_S buffer;
    SampleVIRVI2VOZoomContext *pContext = (SampleVIRVI2VOZoomContext*)pThreadData;
    if (NULL == pContext)
        return NULL;

    memset(&mRectCrop, 0, sizeof(mRectCrop));
    memset(&buffer, 0, sizeof(buffer));

    /**
     * if the maximum resolution supported by sensor imx335_mipi is smaller than the set resolution.
     * to avoid green screen at the edge of the display, we should be correct it.
     */
    getActualCaptureArea(pContext);

    int zoomCnt = 0;

    while(!pContext->mOverFlag)
    {
        ret = calculateCrop(&mRectCrop, zoomCnt, pContext);

        zoomCnt++;
        if (zoomCnt == pContext->mConfigPara.mZoomMaxCnt)
            zoomCnt = 0;

        if (0 != ret)
            continue;

        AW_MPI_VI_GetFrame(pContext->mVIDev, 0, &buffer, 500);  // get one frame cost 40ms

        buffer.VFrame.mOffsetTop = mRectCrop.Y;
        buffer.VFrame.mOffsetBottom = mRectCrop.Y + mRectCrop.Height;
        buffer.VFrame.mOffsetLeft = mRectCrop.X;
        buffer.VFrame.mOffsetRight = mRectCrop.X + mRectCrop.Width;
        AW_MPI_VO_SendFrame(pContext->mVoLayer, pContext->mVOChn, &buffer, 0);
        AW_MPI_VI_ReleaseFrame(pContext->mVIDev, 0, &buffer);
    }

    return NULL;
}

int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
    int result = 0;
    ERRORTYPE eRet = SUCCESS;
    int iIspDev = 0;

    SampleVIRVI2VOZoomContext stContext;

    printf("sample_virvi2vo_zoom running!\n");
    initContext(&stContext);
    gpVIRVI2VOZoomContext = &stContext;

    /* parse command line param */
    if (ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0)
    {
        aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }

    char *pConfigFilePath;
    if (strlen(stContext.mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = stContext.mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }

    /* parse config file. */
    if (loadConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    /* init mpp system */
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    eRet = AW_MPI_SYS_Init();
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_SYS_Init failed");
        result = -1;
        goto sys_init_err;
    }

    /* create vi channel */
    stContext.mVIDev = stContext.mConfigPara.mDevNum;
    stContext.mVIChn = 0;
    alogd("Vipp dev[%d] vir_chn[%d]", stContext.mVIDev, stContext.mVIChn);
    eRet = AW_MPI_VI_CreateVipp(stContext.mVIDev);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
        result = -1;
        goto vi_create_vipp_err;
    }

    VI_ATTR_S stAttr;
    eRet = AW_MPI_VI_GetVippAttr(stContext.mVIDev, &stAttr);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI GetVippAttr failed");
    }
    memset(&stAttr, 0, sizeof(VI_ATTR_S));
    stAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    stAttr.memtype = V4L2_MEMORY_MMAP;
    stAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(stContext.mConfigPara.mPicFormat);
    stAttr.format.field = V4L2_FIELD_NONE;
    // stAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    stAttr.format.width = stContext.mConfigPara.mCaptureWidth;
    stAttr.format.height = stContext.mConfigPara.mCaptureHeight;
    stAttr.nbufs = 3;//10;
    stAttr.nplanes = 2;
    /* do not use current param, if set to 1, all this configuration will
     * not be used.
     */
    stAttr.use_current_win = 0;
    stAttr.fps = stContext.mConfigPara.mFrameRate;
    eRet = AW_MPI_VI_SetVippAttr(stContext.mVIDev, &stAttr);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }

    eRet = AW_MPI_VI_EnableVipp(stContext.mVIDev);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
        result = -1;
        goto vi_enable_vipp_err;
    }

#if ISP_RUN
    /* open isp */
    if (stContext.mVIDev == 0 || stContext.mVIDev == 2)
    {
        iIspDev = 0;
    }
    else if (stContext.mVIDev == 1 || stContext.mVIDev == 3)
    {
        iIspDev = 0;
    }
    AW_MPI_ISP_Init();
    AW_MPI_ISP_Run(iIspDev);
    AW_MPI_VI_SetVippMirror(stContext.mVIDev,0);
    AW_MPI_VI_SetVippFlip(stContext.mVIDev,0);
#endif

    eRet = AW_MPI_VI_CreateVirChn(stContext.mVIDev, stContext.mVIChn, NULL);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", stContext.mVIChn);
    }

    /* enable vo dev */
    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer); /* close ui layer. */
    /* enable vo layer */
    int hlay0 = 0;
    while(hlay0 < VO_MAX_LAYER_NUM)
    {
        if (SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
        {
            break;
        }
        hlay0++;
    }
    if (hlay0 >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
    }

    /* set vo attr */
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(stContext.mVoDev, &spPubAttr);
    spPubAttr.enIntfType = stContext.mConfigPara.mDispType;
    spPubAttr.enIntfSync = stContext.mConfigPara.mDispSync;
    AW_MPI_VO_SetPubAttr(stContext.mVoDev, &spPubAttr);

    stContext.mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 0;
    //stContext.mLayerAttr.enPixFormat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(stContext.mConfigPara.mPicFormat);
    stContext.mLayerAttr.stDispRect.Width = stContext.mConfigPara.mDisplayWidth;
    stContext.mLayerAttr.stDispRect.Height = stContext.mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);

    /* create vo channel. */
    BOOL bSuccessFlag = FALSE;
    stContext.mVOChn = 0;
    while(stContext.mVOChn < VO_MAX_CHN_NUM)
    {
        eRet = AW_MPI_VO_CreateChn(stContext.mVoLayer, stContext.mVOChn);
        if(SUCCESS == eRet)
        {
            bSuccessFlag = TRUE;
            alogd("create vo channel[%d] success!", stContext.mVOChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == eRet)
        {
            alogd("vo channel[%d] is exist, find next!", stContext.mVOChn);
            stContext.mVOChn++;
        }
        else
        {
            aloge("fatal error! create vo channel[%d] ret[0x%x]!", stContext.mVOChn, eRet);
            break;
        }
    }
    if (FALSE == bSuccessFlag)
    {
        stContext.mVOChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        result = -1;
        goto vo_create_chn_err;
    }

    /* register vo callback func */
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VO_RegisterCallback(stContext.mVoLayer, stContext.mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(stContext.mVoLayer, stContext.mVOChn, 2);

    /* start vo, vi_channel. */
    eRet = AW_MPI_VI_EnableVirChn(stContext.mVIDev, stContext.mVIChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! enableVirChn fail!");
        result = -1;
        goto vi_enable_virchn_err;
    }
    AW_MPI_VO_StartChn(stContext.mVoLayer, stContext.mVOChn);

    /* create zoom thread */
    gpVIRVI2VOZoomContext->mOverFlag = FALSE;
    pthread_t mReadThdId = 0;
    result = pthread_create(&mReadThdId, NULL, VoZoomThread, gpVIRVI2VOZoomContext);
    if (result || !mReadThdId)
    {
        aloge("create VoZoomThread fail!");
        result = -1;
        goto VoZoomThread_err;
    }

    if (stContext.mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }

    gpVIRVI2VOZoomContext->mOverFlag = TRUE;
    alogd("test done");
    pthread_join(mReadThdId, (void*)&result);

VoZoomThread_err:
    /* stop vo channel, vi channel */
    AW_MPI_VO_StopChn(stContext.mVoLayer, stContext.mVOChn);
    AW_MPI_VI_DisableVirChn(stContext.mVIDev, stContext.mVIChn);

vi_enable_virchn_err:
    AW_MPI_VO_DestroyChn(stContext.mVoLayer, stContext.mVOChn);
    stContext.mVOChn = MM_INVALID_CHN;

vo_create_chn_err:
    AW_MPI_VI_DestroyVirChn(stContext.mVIDev, stContext.mVIChn);

#if ISP_RUN
    AW_MPI_ISP_Stop(iIspDev);
    AW_MPI_ISP_Exit();
#endif

    AW_MPI_VI_DisableVipp(stContext.mVIDev);
    AW_MPI_VI_DestroyVipp(stContext.mVIDev);
    stContext.mVIDev = MM_INVALID_DEV;
    stContext.mVIChn = MM_INVALID_CHN;

    /* disable vo layer */
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;

vi_enable_vipp_err:
vi_create_vipp_err:
    /* exit mpp system */
    AW_MPI_SYS_Exit();
sys_init_err:
_exit:
    destroyContext(&stContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

