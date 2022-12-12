
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_vo"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>

#include <mm_common.h>
#include <vo/hwdisplay.h>
#include <mpi_sys.h>
#include <mpi_vo.h>
#include <ClockCompPortIndex.h>

#include <confparser.h>

#include "sample_UILayer_config.h"
#include "sample_UILayer.h"

#include <cdx_list.h>

SampleUILayerContext *gpSampleUILayerContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleUILayerContext)
    {
        cdx_sem_up(&gpSampleUILayerContext->mSemExit);
    }
}

int initSampleUILayerContext(SampleUILayerContext *pContext)
{
    memset(pContext, 0, sizeof(SampleUILayerContext));
    pContext->mUILayer = HLAY(2, 0);
    cdx_sem_init(&pContext->mSemExit, 0);
    return 0;
}

int destroySampleUILayerContext(SampleUILayerContext *pContext)
{
    cdx_sem_deinit(&pContext->mSemExit);
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleUILayerCmdLineParam *pCmdLinePara)
{
    alogd("sample vo path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleUILayerCmdLineParam));
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
                "\t-path /home/sample_UILayer.conf\n");
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

static ERRORTYPE loadSampleUILayerConfig(SampleUILayerConfig *pConfig, const char *conf_path)
{
    int ret;
    CONFPARSER_S stConfParser;
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleUILayerConfig));
    pConfig->mPicWidth = GetConfParaInt(&stConfParser, SAMPLE_UILAYER_KEY_PIC_WIDTH, 0);
    pConfig->mPicHeight = GetConfParaInt(&stConfParser, SAMPLE_UILAYER_KEY_PIC_HEIGHT, 0);
    pConfig->mDisplayWidth  = GetConfParaInt(&stConfParser, SAMPLE_UILAYER_KEY_DISPLAY_WIDTH, 0);
    pConfig->mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_UILAYER_KEY_DISPLAY_HEIGHT, 0);
    pConfig->mTestUILayer = GetConfParaInt(&stConfParser, SAMPLE_UILAYER_KEY_TEST_UI_LAYER, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_UILAYER_KEY_BITMAP_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "ARGB8888"))
    {
        pConfig->mBitmapFormat = MM_PIXEL_FORMAT_RGB_8888;
    }
    else if(!strcmp(pStrPixelFormat, "ARGB1555"))
    {
        pConfig->mBitmapFormat = MM_PIXEL_FORMAT_RGB_1555;
    }
    else
    {
        aloge("fatal error! conf file bitmap_format is [%s]?", pStrPixelFormat);
        pConfig->mBitmapFormat = MM_PIXEL_FORMAT_RGB_8888;
    }
    char *pStrDispType = (char*)GetConfParaString(&stConfParser, SAMPLE_UILAYER_KEY_DISP_TYPE, NULL);
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
    else
    {
        alogd("disp type use default LCD!");
        pConfig->mDispType = VO_INTF_LCD;
        pConfig->mDispSync = VO_OUTPUT_NTSC;
    }
    pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_UILAYER_KEY_TEST_DURATION, 0);
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

//    #include <unistd.h> 
//    #include <stdio.h> 
//    #include <stdlib.h> 
//    #include <fcntl.h> 
//    #include <string.h> 
    #include <linux/fb.h> 
    #include <sys/mman.h> 
//    #include <sys/ioctl.h> 
//    #include <arpa/inet.h> 
//    #include <errno.h> 
int testFb(SampleUILayerContext *pContext) 
{ 
    int ret;
    strncpy(pContext->mFbDev.dev, "/dev/fb0", sizeof(pContext->mFbDev.dev));
    pContext->mFbDev.nFbFd = open(pContext->mFbDev.dev, O_RDWR);
    if(pContext->mFbDev.nFbFd<=0)
    {
        aloge("fatal error! fd[%d]<=0", pContext->mFbDev.nFbFd);
    }
    if (-1 == ioctl(pContext->mFbDev.nFbFd, FBIOGET_VSCREENINFO, &pContext->mFbDev.fb_var))
    {
        aloge("ioctl FBIOGET_VSCREENINFO fail");
        return -1;
    }
    alogd("fb_var_screeninfo: [%d,%d], [%d,%d], [%d,%d], bitsPerPixel[%d], grayscale[%d]", 
        pContext->mFbDev.fb_var.xres, 
        pContext->mFbDev.fb_var.yres, 
        pContext->mFbDev.fb_var.xres_virtual, 
        pContext->mFbDev.fb_var.yres_virtual, 
        pContext->mFbDev.fb_var.xoffset, 
        pContext->mFbDev.fb_var.yoffset, 
        pContext->mFbDev.fb_var.bits_per_pixel, 
        pContext->mFbDev.fb_var.grayscale);
    if (-1 == ioctl(pContext->mFbDev.nFbFd, FBIOGET_FSCREENINFO, &pContext->mFbDev.fb_fix))
    {
        aloge("ioctl FBIOGET_FSCREENINFO fail");
        return -1;
    }
    alogd("fb_fix_screeninfo: smem_start[0x%x], smem_len[%ld], pageSize[%d]", pContext->mFbDev.fb_fix.smem_start, pContext->mFbDev.fb_fix.smem_len, getpagesize());
    //map physics address to virtual address
    pContext->mFbDev.fb_mem_offset = pContext->mFbDev.fb_fix.smem_start - (pContext->mFbDev.fb_fix.smem_start&~(getpagesize() - 1));
    pContext->mFbDev.fb_mem = mmap(NULL, pContext->mFbDev.fb_fix.smem_len + pContext->mFbDev.fb_mem_offset, PROT_READ | PROT_WRITE, MAP_SHARED, pContext->mFbDev.nFbFd, 0);
    alogd("mmap: mem:%p, offset:%ld", pContext->mFbDev.fb_mem, pContext->mFbDev.fb_mem_offset);
    if (MAP_FAILED == pContext->mFbDev.fb_mem)
    {
        aloge("mmap error! mem:%p offset:%ld", pContext->mFbDev.fb_mem, pContext->mFbDev.fb_mem_offset);
        return -1;
    }
    

/*
    if(bpp == 18)
    {
        var.red.length = 6;
        var.red.offset = 12;
        var.red.msb_right = 0;

        var.green.length = 6;
        var.green.offset = 6;
        var.green.msb_right = 0;

        var.blue.length = 6;
        var.blue.offset = 0;
        var.blue.msb_right = 0;
    }
    var.bits_per_pixel = bpp;
    
    if(ioctl(fd,FBIOPUT_VSCREENINFO,&var)==-1)
    {
        printf("put screen2 information failure\n");
        return -2;
    }
*/
    int *add = (int *)((char*)pContext->mFbDev.fb_mem + pContext->mFbDev.fb_mem_offset);
    int i;
    for(i=0; i<pContext->mFbDev.fb_fix.smem_len/4; i++)
    { 
        if(i < pContext->mFbDev.fb_fix.smem_len/8)
        {
            *add = 0xffff0000;
        }
        else
        {
            *add = 0xff0000ff;
        }
        add++; 
    } 

    char *mem_start = (char*)pContext->mFbDev.fb_mem + pContext->mFbDev.fb_mem_offset;
    unsigned int bytes_per_pixel = pContext->mFbDev.fb_var.bits_per_pixel >> 3;
    unsigned int pitch = bytes_per_pixel * pContext->mFbDev.fb_var.xres;
    int x = 0;
    int y = 0;
    int w = pContext->mFbDev.fb_var.xres;
    int h = pContext->mFbDev.fb_var.yres;

//    void *args[2];
//    void *dirty_rect_vir_addr_begin = (mem_start +  pitch*y + bytes_per_pixel*x);
//    void *dirty_rect_vir_addr_end  = (mem_start + pitch * (y+ h));
//    args[0] = dirty_rect_vir_addr_begin;
//    args[1] = dirty_rect_vir_addr_end;
//    ret = ioctl(pContext->mFbDev.nFbFd, FBIO_CACHE_SYNC, args);
//    if(ret != 0)
//    {
//        aloge("fatal error! FBIO CACHE SYNC fail[%d]", ret);
//    }
    pContext->mFbDev.fb_var.yoffset = 0;
    pContext->mFbDev.fb_var.reserved[0] = x;
    pContext->mFbDev.fb_var.reserved[1] = y;
    pContext->mFbDev.fb_var.reserved[2] = w;
    pContext->mFbDev.fb_var.reserved[3] = h;
    ret = ioctl(pContext->mFbDev.nFbFd, FBIOPAN_DISPLAY, &pContext->mFbDev.fb_var);
    if(ret != 0)
    {
        aloge("fatal error! FBIOPAN DISPLAY fail[%d]", ret);
    }

    munmap(pContext->mFbDev.fb_mem, pContext->mFbDev.fb_fix.smem_len + pContext->mFbDev.fb_mem_offset);
    pContext->mFbDev.fb_mem = NULL;

    if(pContext->mFbDev.nFbFd >= 0)
    {
        close(pContext->mFbDev.nFbFd);
        pContext->mFbDev.nFbFd = -1;
    }

    alogw("see UILayer for 5s.");
    sleep(5);
    return 0; 

#if 0
    int ret = 0;
    ret = layer_get_para(&pContext->mDispLayerConfig);
    if(ret!=0)
    {
        aloge("fatal error! layer get para fail[%d]", ret);
    }
    alogd("fb size[%dx%d],crop[%d,%d,%dx%d],format[0x%x],csc[0x%x]", 
            pContext->mDispLayerConfig.info.fb.size[0].width,
            pContext->mDispLayerConfig.info.fb.size[0].height,
            pContext->mDispLayerConfig.info.fb.crop.x,
            pContext->mDispLayerConfig.info.fb.crop.y,
            pContext->mDispLayerConfig.info.fb.crop.width,
            pContext->mDispLayerConfig.info.fb.crop.height,
            pContext->mDispLayerConfig.info.fb.format,
            pContext->mDispLayerConfig.info.fb.color_space);
#endif
}

static ERRORTYPE SampleUILayer_MPPCallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleUILayerContext *pContext = (SampleUILayerContext*)cookie;
    if(MOD_ID_VOU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("seriously? vo send eof?");
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                if(pContext->mConfigPara.mPicWidth!=(int)pDisplaySize->Width || pContext->mConfigPara.mPicHeight!=(int)pDisplaySize->Height)
                {
                    alogd("vo src display size[%dx%d] not match with config[%dx%d]", pDisplaySize->Width, pDisplaySize->Height, pContext->mConfigPara.mPicWidth, pContext->mConfigPara.mPicHeight);
                }
                else
                {
                    alogd("vo src display size[%dx%d] same to config", pDisplaySize->Width, pDisplaySize->Height);
                }
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                alogd("vo begin to render");
                break;
            }
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                alogd("vo release video frame, pixelFormat=0x%x", pFrameInfo->VFrame.mPixelFormat);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x,0x%x,0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! wrong mod[%d]", pChn->mModId);
    }
    return SUCCESS;
}

static int fillColorToRGBFrame(VIDEO_FRAME_INFO_S *pFrameInfo)
{
    if(pFrameInfo->VFrame.mPixelFormat != MM_PIXEL_FORMAT_RGB_8888)
    {
        aloge("fatal error! pixelFormat[0x%x] is not argb8888!");
        return -1;
    }
    int nFrameBufLen = pFrameInfo->VFrame.mStride[0];
    //memset(pFrameInfo->VFrame.mpVirAddr[0], 0xFF, nFrameBufLen);

    int *add = (int *)pFrameInfo->VFrame.mpVirAddr[0];
    int i;
    for(i=0; i<nFrameBufLen/4; i++)
    { 
        if(i<nFrameBufLen/16)
        {
            *add = 0xff00ff00;
        }
        else
        {
            *add = 0xff0000ff;
        }
        add++; 
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int result = 0;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 0,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);
    alogd("sample_UILayer running!");

    SampleUILayerContext *pContext = (SampleUILayerContext*)malloc(sizeof(SampleUILayerContext));;
    gpSampleUILayerContext = pContext;

    initSampleUILayerContext(pContext);
    /* parse command line param */
    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    /* parse config file. */
    if(loadSampleUILayerConfig(&pContext->mConfigPara, pContext->mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    /* init mpp system */
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();
    /* enable vo dev, default 0 */
    pContext->mVoDev = 0;
    AW_MPI_VO_Enable(pContext->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
    //AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer.

#if 1
	VO_PUB_ATTR_S spPubAttr;
	AW_MPI_VO_GetPubAttr(pContext->mVoDev, &spPubAttr);
	spPubAttr.enIntfType = pContext->mConfigPara.mDispType;
	spPubAttr.enIntfSync = pContext->mConfigPara.mDispSync;
	AW_MPI_VO_SetPubAttr(pContext->mVoDev, &spPubAttr);
#endif
    
    AW_MPI_VO_GetVideoLayerAttr(pContext->mUILayer, &pContext->mUILayerAttr);
    pContext->mUILayerAttr.stDispRect.X = 0;
    pContext->mUILayerAttr.stDispRect.Y = 0;
    pContext->mUILayerAttr.stDispRect.Width = pContext->mConfigPara.mDisplayWidth;
    pContext->mUILayerAttr.stDispRect.Height = pContext->mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(pContext->mUILayer, &pContext->mUILayerAttr);

    AW_MPI_VO_OpenVideoLayer(pContext->mUILayer);
    //testFb(pContext);
    AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);

    //test other ui layer
    ERRORTYPE ret;
    pContext->mTestUILayer = pContext->mConfigPara.mTestUILayer;
    ret = AW_MPI_VO_EnableVideoLayer(pContext->mTestUILayer);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! enable video layer fail");
    }
    AW_MPI_VO_GetVideoLayerAttr(pContext->mTestUILayer, &pContext->mTestUILayerAttr);
    pContext->mTestUILayerAttr.stDispRect.X = 0;
    pContext->mTestUILayerAttr.stDispRect.Y = 0;
    pContext->mTestUILayerAttr.stDispRect.Width = pContext->mConfigPara.mDisplayWidth;
    pContext->mTestUILayerAttr.stDispRect.Height = pContext->mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(pContext->mTestUILayer, &pContext->mTestUILayerAttr);


    //create bmp frame buffer.
    unsigned int nPhyAddr = 0;
    void *pVirtAddr = NULL;
    unsigned int nFrameBufLen = 0;
    switch(pContext->mConfigPara.mBitmapFormat)
    {
        case MM_PIXEL_FORMAT_RGB_8888:
            nFrameBufLen = pContext->mConfigPara.mPicWidth*pContext->mConfigPara.mPicHeight*4;
            break;
        case MM_PIXEL_FORMAT_RGB_1555:
        case MM_PIXEL_FORMAT_RGB_565:
            nFrameBufLen = pContext->mConfigPara.mPicWidth*pContext->mConfigPara.mPicHeight*2;
            break;
        default:
            aloge("fatal error! unsupport pixel format:0x%x", pContext->mConfigPara.mBitmapFormat);
            break;
    }
    ret = AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &pVirtAddr, nFrameBufLen);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! ion malloc fail:0x%x",ret);
        goto _err1;
    }
    pContext->mTestUIFrame.mId = 0x21;
    pContext->mTestUIFrame.VFrame.mWidth = pContext->mConfigPara.mPicWidth;
    pContext->mTestUIFrame.VFrame.mHeight = pContext->mConfigPara.mPicHeight;
    pContext->mTestUIFrame.VFrame.mField = VIDEO_FIELD_FRAME;
    pContext->mTestUIFrame.VFrame.mPixelFormat = pContext->mConfigPara.mBitmapFormat;
    pContext->mTestUIFrame.VFrame.mPhyAddr[0] = nPhyAddr;
    pContext->mTestUIFrame.VFrame.mpVirAddr[0] = pVirtAddr;
    pContext->mTestUIFrame.VFrame.mStride[0] = nFrameBufLen;
    pContext->mTestUIFrame.VFrame.mOffsetTop = 0;
    pContext->mTestUIFrame.VFrame.mOffsetBottom = pContext->mTestUIFrame.VFrame.mHeight;
    pContext->mTestUIFrame.VFrame.mOffsetLeft = 0;
    pContext->mTestUIFrame.VFrame.mOffsetRight = pContext->mTestUIFrame.VFrame.mWidth;

    //fill color in frame.
    fillColorToRGBFrame(&pContext->mTestUIFrame);
    //create vo channel
    bool bSuccessFlag = false;
    pContext->mTestVOChn = 0;
    while(pContext->mTestVOChn < VO_MAX_CHN_NUM)
    {
        ret = AW_MPI_VO_CreateChn(pContext->mTestUILayer, pContext->mTestVOChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("create vo channel[%d] success!", pContext->mTestVOChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == ret)
        {
            alogd("vo channel[%d] is exist, find next!", pContext->mTestVOChn);
            pContext->mTestVOChn++;
        }
        else
        {
            aloge("fatal error! create vo channel[%d] ret[0x%x]!", pContext->mTestVOChn, ret);
            break;
        }
    }

    if(false == bSuccessFlag)
    {
        pContext->mTestVOChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        result = -1;
        goto _err0;
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleUILayer_MPPCallback;
    AW_MPI_VO_RegisterCallback(pContext->mTestUILayer, pContext->mTestVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(pContext->mTestUILayer, pContext->mTestVOChn, 0);

    ret = AW_MPI_VO_StartChn(pContext->mTestUILayer, pContext->mTestVOChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo start chn fail!");
    }
    ret = AW_MPI_VO_SendFrame(pContext->mTestUILayer, pContext->mTestVOChn, &pContext->mTestUIFrame, 0);
    if(ret != SUCCESS)
    {
        aloge("fatal error! send frame to vo fail!");
    }

    if(pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    ret = AW_MPI_VO_StopChn(pContext->mTestUILayer, pContext->mTestVOChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo stop chn fail!");
    }
_err1:
    AW_MPI_VO_DestroyChn(pContext->mTestUILayer, pContext->mTestVOChn);
    ret = AW_MPI_VO_DisableVideoLayer(pContext->mTestUILayer);
    if(ret != SUCCESS)
    {
        aloge("fatal error! disable video layer fail!");
    }
    //wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then we can free frame buffer.
    usleep(50*1000);
    ret = AW_MPI_SYS_MmzFree(pContext->mTestUIFrame.VFrame.mPhyAddr[0], pContext->mTestUIFrame.VFrame.mpVirAddr[0]);
    if(ret != SUCCESS)
    {
        aloge("fatal error! free ion memory fail!");
    }
_err0:
    AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(pContext->mVoDev);
    pContext->mVoDev = -1;
    /* exit mpp system */
    AW_MPI_SYS_Exit();

_exit:
    destroySampleUILayerContext(pContext);
    free(pContext);
    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
