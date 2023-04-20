/*
********************************************************************************
*                           eyesee-mpp package
*
*          (c) Copyright 2020-2030, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : sample_vi_g2d.c
* Version: V1.0
* By     : eric_wang@allwinnertech.com
* Date   : 2020-4-29
* Description:
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_vi_g2d"
#include <utils/plat_log.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/g2d_driver.h>

#include <vo/hwdisplay.h>
#include <mpi_vo.h>
#include <media/mpi_sys.h>
#include <media/mm_comm_vi.h>
#include <mpi_videoformat_conversion.h>
#include <SystemBase.h>
#include <VideoFrameInfoNode.h>
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include <utils/PIXEL_FORMAT_E_g2d_format_convert.h>
#include <utils/VIDEO_FRAME_INFO_S.h>
#include <confparser.h>
#include "sample_vi_g2d.h"
#include "sample_vi_g2d_config.h"
#include <cdx_list.h>

#define ISP_RUN 1

static int ParseCmdLine(int argc, char **argv, SampleViG2dCmdLineParam *pCmdLinePara)
{
    alogd("sample virvi path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleViG2dCmdLineParam));
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
            alogd("CmdLine param:\n"
                "\t-path /mnt/extsd/sample_vi_g2d.conf");
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

static ERRORTYPE loadSampleViG2dConfig(SampleViG2dConfig *pConfig, const char *pConfPath)
{
    int ret = SUCCESS;

    pConfig->mDevNum = 0;
    pConfig->mFrameRate = 60;
    pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pConfig->mDropFrameNum = 60;
    pConfig->mSrcWidth = 1920;
    pConfig->mSrcHeight = 1080;
    pConfig->mSrcRectX = 0;
    pConfig->mSrcRectY = 0;
    pConfig->mSrcRectW = 1920;
    pConfig->mSrcRectH = 1080;
    pConfig->mDstRotate = 90;
    pConfig->mDstWidth = 1080;
    pConfig->mDstHeight = 1920;
    pConfig->mDstRectX = 0;
    pConfig->mDstRectY = 0;
    pConfig->mDstRectW = 1080;
    pConfig->mDstRectH = 1920;
    pConfig->mDstStoreCount = 2;
    pConfig->mDstStoreInterval = 60;
    strcpy(pConfig->mStoreDir, "/mnt/extsd");
    pConfig->mDisplayFlag = true;
    pConfig->mDisplayX = 60;
    pConfig->mDisplayY = 0;
    pConfig->mDisplayW = 360;
    pConfig->mDisplayH = 640;
    pConfig->mTestDuration = 30;

    if(pConfPath != NULL)
    {
        CONFPARSER_S stConfParser;
        ret = createConfParser(pConfPath, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }
        pConfig->mDevNum    = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DEV_NUM, 0);
        pConfig->mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_FRAME_RATE, 0);
        char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_VIG2D_KEY_PIC_FORMAT, NULL);
        if(!strcmp(pStrPixelFormat, "nv21"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if(!strcmp(pStrPixelFormat, "nv12"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else
        {
            aloge("fatal error! conf file pic_format[%s] is unsupported", pStrPixelFormat);
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        pConfig->mDropFrameNum = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DROP_FRAME_NUM, 0);
        pConfig->mSrcWidth  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_SRC_WIDTH, 0);
        pConfig->mSrcHeight = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_SRC_HEIGHT, 0);
        pConfig->mSrcRectX  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_SRC_RECT_X, 0);
        pConfig->mSrcRectY  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_SRC_RECT_Y, 0);
        pConfig->mSrcRectW  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_SRC_RECT_W, 0);
        pConfig->mSrcRectH  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_SRC_RECT_H, 0);
        pConfig->mDstRotate = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DST_ROTATE, 0);
        pConfig->mDstWidth  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DST_WIDTH, 0);
        pConfig->mDstHeight = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DST_HEIGHT, 0);
        pConfig->mDstRectX  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DST_RECT_X, 0);
        pConfig->mDstRectY  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DST_RECT_Y, 0);
        pConfig->mDstRectW  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DST_RECT_W, 0);
        pConfig->mDstRectH  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DST_RECT_H, 0);
        pConfig->mDstStoreCount = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DST_STORE_COUNT, 0);
        pConfig->mDstStoreInterval = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DST_STORE_INTERVAL, 0);
        char *pStrDir = (char*)GetConfParaString(&stConfParser, SAMPLE_VIG2D_KEY_STORE_DIR, NULL);
        strcpy(pConfig->mStoreDir, pStrDir);
        pConfig->mDisplayFlag = (bool)GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DISPLAY_FLAG, 0);
        pConfig->mDisplayX  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DISPLAY_X, 0);
        pConfig->mDisplayY  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DISPLAY_Y, 0);
        pConfig->mDisplayW  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DISPLAY_W, 0);
        pConfig->mDisplayH  = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_DISPLAY_H, 0);
        pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_VIG2D_KEY_TEST_DURATION, 0);
        destroyConfParser(&stConfParser);
    }
    alogd("dev_num=%d, frameRate=%d, pixFmt=0x%x, dropFrameNum=%d,srcSize=[%dx%d], srcRect=[%d,%d,%dx%d],"
        "dstRot=%d, dstSize=[%dx%d], dstRect=[%d,%d,%dx%d], dstStoreCnt=%d, dstStoreInterval=%d, storeDir=[%s],"
        "displayFlag=%d, displayRect=[%d,%d,%dx%d], testDuration=%d",
       pConfig->mDevNum, pConfig->mFrameRate, pConfig->mPicFormat, pConfig->mDropFrameNum, 
       pConfig->mSrcWidth, pConfig->mSrcHeight, pConfig->mSrcRectX, pConfig->mSrcRectY, pConfig->mSrcRectW, pConfig->mSrcRectH,
       pConfig->mDstRotate, pConfig->mDstWidth, pConfig->mDstHeight, pConfig->mDstRectX, pConfig->mDstRectY, pConfig->mDstRectW, pConfig->mDstRectH,
       pConfig->mDstStoreCount, pConfig->mDstStoreInterval, pConfig->mStoreDir,
       pConfig->mDisplayFlag, pConfig->mDisplayX, pConfig->mDisplayY, pConfig->mDisplayW, pConfig->mDisplayH, pConfig->mTestDuration);
    return ret;
}

static int saveRawPicture(SampleViCapS *pViCapCtx, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    int ret = 0;
    SampleViG2dContext *pContext = (SampleViG2dContext*)pViCapCtx->mpContext;
    //make file name, e.g., /mnt/extsd/pic[0].NV21M
    int i;
    char strPixFmt[16] = {0};
    switch(pContext->mConfigPara.mPicFormat)
    {
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        {
            strcpy(strPixFmt,"nv12");
            break;
        }
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        {
            strcpy(strPixFmt,"nv21");
            break;
        }
        default:
        {
            strcpy(strPixFmt,"unknown");
            break;
        }
    }
    char strFilePath[MAX_FILE_PATH_SIZE];
    snprintf(strFilePath, MAX_FILE_PATH_SIZE, "%s/pic[%d].%s", pContext->mConfigPara.mStoreDir, pViCapCtx->mRawStoreNum, strPixFmt);

    FILE *fpPic = fopen(strFilePath, "wb");
    if(fpPic != NULL)
    {
        VideoFrameBufferSizeInfo FrameSizeInfo;
        getVideoFrameBufferSizeInfo(pFrameInfo, &FrameSizeInfo);
        int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
        for(i=0; i<3; i++)
        {
            if(pFrameInfo->VFrame.mpVirAddr[i] != NULL)
            {
                AW_MPI_SYS_MmzFlushCache(pFrameInfo->VFrame.mPhyAddr[i], pFrameInfo->VFrame.mpVirAddr[i], yuvSize[i]);
                fwrite(pFrameInfo->VFrame.mpVirAddr[i], 1, yuvSize[i], fpPic);
                alogd("virAddr[%d]=[%p], length=[%d]", i, pFrameInfo->VFrame.mpVirAddr[i], yuvSize[i]);
                AW_MPI_SYS_MmzFlushCache(pFrameInfo->VFrame.mPhyAddr[i], pFrameInfo->VFrame.mpVirAddr[i], yuvSize[i]);
            }
        }
        fclose(fpPic);
        alogd("store raw frame in file[%s]", strFilePath);
        ret = 0;
    }
    else
    {
        aloge("fatal error! open file[%s] fail!", strFilePath);
        ret = -1;
    }
    return ret;
}

/*******************************************************************************
Function name: GetCSIFrameThread
Description: 
    Get frame from vipp, convert frame to VideoFrameManager, store some rotated pictures.
Parameters: 
    
Return: 
    
Time: 2020/5/4
*******************************************************************************/
static void *GetCSIFrameThread(void *pArg)
{
    int ret = 0;
    int i = 0;
    SampleViCapS *pViCapCtx = (SampleViCapS*)pArg;
    SampleViG2dContext *pContext = (SampleViG2dContext*)pViCapCtx->mpContext;
    VIDEO_FRAME_INFO_S stSrcFrameInfo;
    VIDEO_FRAME_INFO_S *pDstFrameInfo = NULL;
    alogd("Cap threadid=0x%lx, ViDev = %d, ViCh = %d", pViCapCtx->mThreadId, pViCapCtx->mDev, pViCapCtx->mChn);
    while (1)
    {
        if(pContext->mbExitFlag)
        {
            alogd("csi frame thread detects exit flag:%d, exit now", pContext->mbExitFlag);
            break;
        }
        //get frame from vipp.
        if ((ret = AW_MPI_VI_GetFrame(pViCapCtx->mDev, pViCapCtx->mChn, &stSrcFrameInfo, pViCapCtx->mTimeout)) != 0)
        {
            alogw("Get Frame from viChn[%d,%d] failed!", pViCapCtx->mDev, pViCapCtx->mChn);
            continue ;
        }
        i++;
        //get dstFrame from video frame manager.
        _retryGetIdelFrame:
        pDstFrameInfo = pContext->mpFrameManager->GetIdleFrame(pContext->mpFrameManager, 1000);
        if(NULL == pDstFrameInfo)
        {
            aloge("fatal error! why video frame manager has none idle frame?");
            goto _retryGetIdelFrame;
        }
        //use g2d to convert frame.
        ret = pViCapCtx->mpG2dConvert->G2dConvertVideoFrame(pViCapCtx->mpG2dConvert, &stSrcFrameInfo, pDstFrameInfo);
        if(0 == ret)
        {
            ret = pContext->mpFrameManager->FillingFrameDone(pContext->mpFrameManager, pDstFrameInfo);
            if(ret!=0)
            {
                aloge("fatal error! Filling frame done fail[%d], pDstFrame[%p]", ret, pDstFrameInfo);
            }
        }
        else
        {
            aloge("fatal error! g2d convert fail[%d]", ret);
            ret = pContext->mpFrameManager->FillingFrameFail(pContext->mpFrameManager, pDstFrameInfo);
            if(ret!=0)
            {
                aloge("fatal error! Filling frame fail fail[%d], pDstFrame[%p]", ret, pDstFrameInfo);
            }
        }
        if ((ret = AW_MPI_VI_ReleaseFrame(pViCapCtx->mDev, pViCapCtx->mChn, &stSrcFrameInfo)) != 0)
        {
            aloge("fatal error! release Frame from viChn[%d,%d] failed!", pViCapCtx->mDev, pViCapCtx->mChn);
        }

        //store raw frame if needed.
        if(pViCapCtx->mRawStoreNum < pContext->mConfigPara.mDstStoreCount)
        {
            if(0 == i%pContext->mConfigPara.mDstStoreInterval)
            {
                ret = saveRawPicture(pViCapCtx, pDstFrameInfo);
                if(ret != 0)
                {
                    aloge("fatal error! save raw picture fail[%d]", ret);
                }
                pViCapCtx->mRawStoreNum++;
            }
        }
    }
    return (void*)ret;
}

static void *DisplayThread(void *pArg)
{
    int ret = 0;
    SampleVoDisplayS *pVoDisplayCtx = (SampleVoDisplayS*)pArg;
    SampleViG2dContext *pContext = pVoDisplayCtx->mpContext;
    VIDEO_FRAME_INFO_S *pFrameInfo = NULL;
    while(1)
    {
        if(pContext->mbExitFlag)
        {
            alogd("display thread detects exit flag:%d, exit now", pContext->mbExitFlag);
            break;
        }
        //get frame from video frame manager
        pFrameInfo = pContext->mpFrameManager->GetReadyFrame(pContext->mpFrameManager, 200);
        if(NULL == pFrameInfo)
        {
            alogw("Be careful! get ready frame fail");
            continue;
        }
        ret = AW_MPI_VO_SendFrame(pVoDisplayCtx->mVoLayer, pVoDisplayCtx->mVOChn, pFrameInfo, 0);
        if(ret != SUCCESS)
        {
            aloge("fatal error! send frame to vo fail[0x%x]!", ret);
        }
    }
    return (void*)ret;
}

int initSampleViCapS(SampleViCapS *pThiz)
{
    if(pThiz)
    {
        memset(pThiz, 0, sizeof(*pThiz));
    }
    return 0;
}
int destroySampleViCapS(SampleViCapS *pThiz)
{
    if(pThiz->mbThreadExistFlag)
    {
        aloge("fatal error! SampleViCapS thread is still exist!");
    }
    if(pThiz->mpG2dConvert)
    {
        aloge("fatal error! SampleViCapS G2dConvert[%p] is still exist!", pThiz->mpG2dConvert);
    }
    return 0;
}
int initSampleVoDisplayS(SampleVoDisplayS *pThiz)
{
    if(pThiz)
    {
        memset(pThiz, 0, sizeof(*pThiz));
    }
    return 0;
}
int destroySampleVoDisplayS(SampleVoDisplayS *pThiz)
{
    if(pThiz->mbThreadExistFlag)
    {
        aloge("fatal error! SampleVoDisplayS thread is still exist!");
    }
    return 0;
}

static int SampleViG2d_G2dConvert_G2dOpen(SampleViG2d_G2dConvert *pThiz)
{
    int ret = 0;
    pThiz->mG2dFd = open("/dev/g2d", O_RDWR, 0);
    if (pThiz->mG2dFd < 0)
    {
        aloge("fatal error! open /dev/g2d failed");
        ret = -1;
    }
    return ret;
}
static int SampleViG2d_G2dConvert_G2dClose(SampleViG2d_G2dConvert *pThiz)
{
    if(pThiz->mG2dFd >= 0)
    {
        close(pThiz->mG2dFd);
        pThiz->mG2dFd = -1;
    }
    return 0;
}
static int SampleViG2d_G2dConvert_G2dSetConvertParam(SampleViG2d_G2dConvert *pThiz, G2dConvertParam *pConvertParam)
{
    if(pConvertParam)
    {
        pThiz->mConvertParam = *pConvertParam;
    }
    return 0;
}
/*******************************************************************************
Function name: SampleViG2d_G2dConvert_G2dConvertVideoFrame
Description: 
    convert source frame to dst frame.
    support rotation, resize, crop, both from source frame and to dst frame.
Parameters: 
    
Return: 
    
Time: 2020/5/4
*******************************************************************************/
static int SampleViG2d_G2dConvert_G2dConvertVideoFrame(SampleViG2d_G2dConvert *pThiz, VIDEO_FRAME_INFO_S* pSrcFrameInfo, VIDEO_FRAME_INFO_S* pDstFrameInfo)
{
    if(pThiz->mConvertParam.mDstWidth!=pDstFrameInfo->VFrame.mWidth || pThiz->mConvertParam.mDstHeight!=pDstFrameInfo->VFrame.mHeight)
    {
        aloge("fatal error! dst size is not match: [%dx%d]!=[%dx%d]", pThiz->mConvertParam.mDstWidth, pThiz->mConvertParam.mDstHeight, pDstFrameInfo->VFrame.mWidth, pDstFrameInfo->VFrame.mHeight);
        return -1;
    }
    int ret = 0;
    g2d_blt_h blit;
    g2d_fmt_enh eSrcFormat, eDstFormat;
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pSrcFrameInfo->VFrame.mPixelFormat, &eSrcFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pSrcFrameInfo->VFrame.mPixelFormat);
        return -1;
    }
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pDstFrameInfo->VFrame.mPixelFormat, &eDstFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pDstFrameInfo->VFrame.mPixelFormat);
        return -1;
    }

    //config blit
    memset(&blit, 0, sizeof(g2d_blt_h));
    switch(pThiz->mConvertParam.mDstRotate)
    {
        case 0:
            blit.flag_h = G2D_BLT_NONE_H;   //G2D_ROT_0, G2D_BLT_NONE_H
            break;
        case 90:
            blit.flag_h = G2D_ROT_90;
            break;
        case 180:
            blit.flag_h = G2D_ROT_180;
            break;
        case 270:
            blit.flag_h = G2D_ROT_270;
            break;
        default:
            aloge("fatal error! rotation[%d] is invalid!", pThiz->mConvertParam.mDstRotate);
            blit.flag_h = G2D_BLT_NONE_H;
            break;
    }
    //blit.src_image_h.bbuff = 1;
    //blit.src_image_h.color = 0xff;
    blit.src_image_h.format = eSrcFormat;
    blit.src_image_h.laddr[0] = pSrcFrameInfo->VFrame.mPhyAddr[0];
    blit.src_image_h.laddr[1] = pSrcFrameInfo->VFrame.mPhyAddr[1];
    blit.src_image_h.laddr[2] = pSrcFrameInfo->VFrame.mPhyAddr[2];
    //blit.src_image_h.haddr[] = 
    blit.src_image_h.width = pSrcFrameInfo->VFrame.mWidth;
    blit.src_image_h.height = pSrcFrameInfo->VFrame.mHeight;
    blit.src_image_h.align[0] = 0;
    blit.src_image_h.align[1] = 0;
    blit.src_image_h.align[2] = 0;
    blit.src_image_h.clip_rect.x = pThiz->mConvertParam.mSrcRectX;
    blit.src_image_h.clip_rect.y = pThiz->mConvertParam.mSrcRectY;
    blit.src_image_h.clip_rect.w = pThiz->mConvertParam.mSrcRectW;
    blit.src_image_h.clip_rect.h = pThiz->mConvertParam.mSrcRectH;
    blit.src_image_h.gamut = G2D_BT601;
    blit.src_image_h.bpremul = 0;
    //blit.src_image_h.alpha = 0xff;
    blit.src_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.src_image_h.fd = -1;
    blit.src_image_h.use_phy_addr = 1;

    //blit.dst_image_h.bbuff = 1;
    //blit.dst_image_h.color = 0xff;
    blit.dst_image_h.format = eDstFormat;
    blit.dst_image_h.laddr[0] = pDstFrameInfo->VFrame.mPhyAddr[0];
    blit.dst_image_h.laddr[1] = pDstFrameInfo->VFrame.mPhyAddr[1];
    blit.dst_image_h.laddr[2] = pDstFrameInfo->VFrame.mPhyAddr[2];
    //blit.dst_image_h.haddr[] = 
    blit.dst_image_h.width = pDstFrameInfo->VFrame.mWidth;
    blit.dst_image_h.height = pDstFrameInfo->VFrame.mHeight;
    blit.dst_image_h.align[0] = 0;
    blit.dst_image_h.align[1] = 0;
    blit.dst_image_h.align[2] = 0;
    blit.dst_image_h.clip_rect.x = pThiz->mConvertParam.mDstRectX;
    blit.dst_image_h.clip_rect.y = pThiz->mConvertParam.mDstRectY;
    blit.dst_image_h.clip_rect.w = pThiz->mConvertParam.mDstRectW;
    blit.dst_image_h.clip_rect.h = pThiz->mConvertParam.mDstRectH;
    blit.dst_image_h.gamut = G2D_BT601;
    blit.dst_image_h.bpremul = 0;
    //blit.dst_image_h.alpha = 0xff;
    blit.dst_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.dst_image_h.fd = -1;
    blit.dst_image_h.use_phy_addr = 1;

    ret = ioctl(pThiz->mG2dFd, G2D_CMD_BITBLT_H, (unsigned long)&blit);
    if(ret < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed[%d]", ret);
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
    }

    pDstFrameInfo->VFrame.mOffsetTop = pThiz->mConvertParam.mDstRectY;
    pDstFrameInfo->VFrame.mOffsetBottom = pThiz->mConvertParam.mDstRectY + pThiz->mConvertParam.mDstRectH;
    pDstFrameInfo->VFrame.mOffsetLeft = pThiz->mConvertParam.mDstRectX;
    pDstFrameInfo->VFrame.mOffsetRight = pThiz->mConvertParam.mDstRectX + pThiz->mConvertParam.mDstRectW;
    //alogd("debug g2d[0x%x]: virAddr[0x%x][0x%x], phyAddr[0x%x][0x%x], size[%dx%d]", mG2DHandle,
    //    pDes->VFrame.mpVirAddr[0], pDes->VFrame.mpVirAddr[1], pDes->VFrame.mPhyAddr[0], pDes->VFrame.mPhyAddr[1],
    //    pDes->VFrame.mWidth, pDes->VFrame.mHeight);
    //memset(pDes->VFrame.mpVirAddr[0], 0xff, pDes->VFrame.mWidth * pDes->VFrame.mHeight);
    //memset(pDes->VFrame.mpVirAddr[1], 0xff, pDes->VFrame.mWidth * pDes->VFrame.mHeight/2);
    //memcpy(pDes->VFrame.mpVirAddr[0], pSrc->VFrame.mpVirAddr[0], pDes->VFrame.mWidth * pDes->VFrame.mHeight);
    //memcpy(pDes->VFrame.mpVirAddr[1], pSrc->VFrame.mpVirAddr[1], pDes->VFrame.mWidth * pDes->VFrame.mHeight/2);
    return ret;
}
static int SampleViG2d_G2dConvert_G2dConvertVideoFrame_1(SampleViG2d_G2dConvert *pThiz, VIDEO_FRAME_INFO_S* pSrcFrameInfo, VIDEO_FRAME_INFO_S* pDstFrameInfo)
{
    if(pThiz->mConvertParam.mDstRotate != 0)
    {
        aloge("fatal error! g2d cmd mixer task don't support rotate[%d]", pThiz->mConvertParam.mDstRotate);
        return -1;
    }
    int ret = 0;
    g2d_fmt_enh eSrcFormat, eDstFormat;
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pSrcFrameInfo->VFrame.mPixelFormat, &eSrcFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pSrcFrameInfo->VFrame.mPixelFormat);
        return -1;
    }
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pDstFrameInfo->VFrame.mPixelFormat, &eDstFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pDstFrameInfo->VFrame.mPixelFormat);
        return -1;
    }
    int num = 1;
    struct mixer_para *g2d_info = (struct mixer_para*)malloc(sizeof(*g2d_info)*num);
    if(NULL == g2d_info)
    {
        aloge("fatal error! malloc fail!");
        return -1;
    }
    memset(g2d_info, 0, sizeof(*g2d_info)*num);
    int idx = 0;
    g2d_info[idx].op_flag = OP_BITBLT;
    g2d_info[idx].flag_h = G2D_BLT_NONE_H;
    //g2d_info[idx].back_flag
    //g2d_info[idx].fore_flag;
    //g2d_info[idx].bld_cmd;
    //g2d_info[idx].src_image_h.bbuff;
    //g2d_info[idx].src_image_h.color = 0xFFFFFFFF;
    g2d_info[idx].src_image_h.format = eSrcFormat;
    g2d_info[idx].src_image_h.laddr[0] = pSrcFrameInfo->VFrame.mPhyAddr[0];
    g2d_info[idx].src_image_h.laddr[1] = pSrcFrameInfo->VFrame.mPhyAddr[1];
    g2d_info[idx].src_image_h.laddr[2] = pSrcFrameInfo->VFrame.mPhyAddr[2];
    //g2d_info[idx].src_image_h.haddr[]
    g2d_info[idx].src_image_h.width  = pSrcFrameInfo->VFrame.mWidth;
    g2d_info[idx].src_image_h.height = pSrcFrameInfo->VFrame.mHeight;
    g2d_info[idx].src_image_h.align[0] = 0;
    g2d_info[idx].src_image_h.align[1] = 0;
    g2d_info[idx].src_image_h.align[2] = 0;
    g2d_info[idx].src_image_h.clip_rect.x = pThiz->mConvertParam.mSrcRectX;
    g2d_info[idx].src_image_h.clip_rect.y = pThiz->mConvertParam.mSrcRectY;
    g2d_info[idx].src_image_h.clip_rect.w = pThiz->mConvertParam.mSrcRectW;
    g2d_info[idx].src_image_h.clip_rect.h = pThiz->mConvertParam.mSrcRectH;
    g2d_info[idx].src_image_h.gamut = G2D_BT601;
    g2d_info[idx].src_image_h.bpremul = 0;
    //g2d_info[idx].src_image_h.alpha = 0x0;
    g2d_info[idx].src_image_h.mode = G2D_PIXEL_ALPHA;// G2D_PIXEL_ALPHA;G2D_GLOBAL_ALPHA
    g2d_info[idx].src_image_h.fd = -1;
    g2d_info[idx].src_image_h.use_phy_addr = 1;
    //g2d_info[idx].dst_image_h.bbuff;
    //g2d_info[idx].dst_image_h.color = 0xFFFFFFFF;
    g2d_info[idx].dst_image_h.format = eDstFormat;
    g2d_info[idx].dst_image_h.laddr[0] = pDstFrameInfo->VFrame.mPhyAddr[0];
    g2d_info[idx].dst_image_h.laddr[1] = pDstFrameInfo->VFrame.mPhyAddr[1];
    g2d_info[idx].dst_image_h.laddr[2] = pDstFrameInfo->VFrame.mPhyAddr[2];
    //g2d_info[idx].dst_image_h.haddr[]
    g2d_info[idx].dst_image_h.width  = pDstFrameInfo->VFrame.mWidth;
    g2d_info[idx].dst_image_h.height = pDstFrameInfo->VFrame.mHeight;
    g2d_info[idx].dst_image_h.align[0] = 0;
    g2d_info[idx].dst_image_h.align[1] = 0;
    g2d_info[idx].dst_image_h.align[2] = 0;
    g2d_info[idx].dst_image_h.clip_rect.x = pThiz->mConvertParam.mDstRectX;
    g2d_info[idx].dst_image_h.clip_rect.y = pThiz->mConvertParam.mDstRectY;
    g2d_info[idx].dst_image_h.clip_rect.w = pThiz->mConvertParam.mDstRectW;
    g2d_info[idx].dst_image_h.clip_rect.h = pThiz->mConvertParam.mDstRectH;
    g2d_info[idx].dst_image_h.gamut = G2D_BT601;
    g2d_info[idx].dst_image_h.bpremul = 0;
    //g2d_info[idx].dst_image_h.alpha = 0x0;
    g2d_info[idx].dst_image_h.mode = G2D_PIXEL_ALPHA; //G2D_PIXEL_ALPHA;G2D_GLOBAL_ALPHA
    g2d_info[idx].dst_image_h.fd = -1;
    g2d_info[idx].dst_image_h.use_phy_addr = 1;
    //g2d_info[idx].ptn_image_h
    //g2d_info[idx].mask_image_h
    //g2d_info[idx].ck_para

    unsigned long arg[2] = {0};
    arg[0] = (unsigned long)g2d_info;
    arg[1] = num;
    ret = ioctl(pThiz->mG2dFd, G2D_CMD_MIXER_TASK, arg);
    if (ret < 0)
    {
        aloge("fatal error! G2D_CMD_MIXER_TASK failure![%s]", strerror(errno));
    }
    if(g2d_info != NULL)
    {
        free(g2d_info);
        g2d_info = NULL;
    }
    pDstFrameInfo->VFrame.mOffsetTop = pThiz->mConvertParam.mDstRectY;
    pDstFrameInfo->VFrame.mOffsetBottom = pThiz->mConvertParam.mDstRectY + pThiz->mConvertParam.mDstRectH;
    pDstFrameInfo->VFrame.mOffsetLeft = pThiz->mConvertParam.mDstRectX;
    pDstFrameInfo->VFrame.mOffsetRight = pThiz->mConvertParam.mDstRectX + pThiz->mConvertParam.mDstRectW;
    return ret;

}
SampleViG2d_G2dConvert *constructSampleViG2d_G2dConvert()
{
    SampleViG2d_G2dConvert *pConvert = (SampleViG2d_G2dConvert*)malloc(sizeof(SampleViG2d_G2dConvert));
    if(NULL == pConvert)
    {
        aloge("fatal error! malloc fail!");
        return NULL;
    }
    memset(pConvert, 0, sizeof(*pConvert));
    pConvert->mG2dFd = -1;
    pConvert->G2dOpen = SampleViG2d_G2dConvert_G2dOpen;
    pConvert->G2dClose = SampleViG2d_G2dConvert_G2dClose;
    pConvert->G2dSetConvertParam = SampleViG2d_G2dConvert_G2dSetConvertParam;
    pConvert->G2dConvertVideoFrame = SampleViG2d_G2dConvert_G2dConvertVideoFrame;
    return pConvert;
}
void destructSampleViG2d_G2dConvert(SampleViG2d_G2dConvert *pThiz)
{
    if(pThiz != NULL)
    {
        if(pThiz->mG2dFd >= 0)
        {
            close(pThiz->mG2dFd);
            pThiz->mG2dFd = -1;
        }
        free(pThiz);
    }
}

/*******************************************************************************
Function name: SampleViG2d_VideoFrameManager_GetIdleFrame
Description: 
    
Parameters: 
    nTimeout: -1: wait forever; 
              0 : return immediately; 
              >0: wait until timeout, unit:ms
Return: 
    
Time: 2020/4/30
*******************************************************************************/
static VIDEO_FRAME_INFO_S* SampleViG2d_VideoFrameManager_GetIdleFrame(SampleViG2d_VideoFrameManager *pThiz, int nTimeout) //unit:ms
{
    int ret;
    VideoFrameInfoNode* pNode = NULL;
    VIDEO_FRAME_INFO_S *pFrameInfo = NULL;
    pthread_mutex_lock(&pThiz->mLock);
_retry:
    if(!list_empty(&pThiz->mIdleFrameList))
    {
        pNode = list_first_entry(&pThiz->mIdleFrameList, VideoFrameInfoNode, mList);
        pFrameInfo = &pNode->VFrame;
        list_move_tail(&pNode->mList, &pThiz->mFillingFrameList);
    }
    else
    {
        if(0 == nTimeout)
        {
            pFrameInfo = NULL;
        }
        else if(nTimeout < 0)
        {
            pThiz->mWaitIdleFrameFlag = true;
            while(list_empty(&pThiz->mIdleFrameList))
            {
                pthread_cond_wait(&pThiz->mIdleFrameCond, &pThiz->mLock);
            }
            pThiz->mWaitIdleFrameFlag = false;
            goto _retry;
        }
        else
        {
            pThiz->mWaitIdleFrameFlag = true;
            ret = pthread_cond_wait_timeout(&pThiz->mIdleFrameCond, &pThiz->mLock, nTimeout);
            if(ETIMEDOUT == ret)
            {
                alogv("wait output frame timeout[%d]ms, ret[%d]", nTimeout, ret);
                pFrameInfo = NULL;
                pThiz->mWaitIdleFrameFlag = false;
            }
            else if(0 == ret)
            {
                pThiz->mWaitIdleFrameFlag = false;
                goto _retry;
            }
            else
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                pFrameInfo = NULL;
                pThiz->mWaitIdleFrameFlag = false;
            }
        }
    }
    pthread_mutex_unlock(&pThiz->mLock);
    return pFrameInfo;
}
static int SampleViG2d_VideoFrameManager_FillingFrameDone(SampleViG2d_VideoFrameManager *pThiz, VIDEO_FRAME_INFO_S *pFrame)
{
    int ret = 0;
    int nFindFlag = 0;
    VideoFrameInfoNode *pNode = NULL, *pTemp;
    pthread_mutex_lock(&pThiz->mLock);
    list_for_each_entry_safe(pNode, pTemp, &pThiz->mFillingFrameList, mList)
    {
        if(pNode->VFrame.mId == pFrame->mId)
        {
            list_move_tail(&pNode->mList, &pThiz->mReadyFrameList);
            nFindFlag++;
            break;
        }
    }
    if(1 == nFindFlag)
    {
        if(pThiz->mWaitReadyFrameFlag)
        {
            pthread_cond_signal(&pThiz->mReadyFrameCond);
        }
    }
    else
    {
        aloge("fatal error! find [%d] nodes", nFindFlag);
        ret = -1;
    }
    pthread_mutex_unlock(&pThiz->mLock);
    return ret;
}

static int SampleViG2d_VideoFrameManager_FillingFrameFail(SampleViG2d_VideoFrameManager *pThiz, VIDEO_FRAME_INFO_S *pFrame)
{
    int ret = 0;
    int nFindFlag = 0;
    VideoFrameInfoNode *pNode = NULL, *pTemp;
    pthread_mutex_lock(&pThiz->mLock);
    list_for_each_entry_safe(pNode, pTemp, &pThiz->mFillingFrameList, mList)
    {
        if(pNode->VFrame.mId == pFrame->mId)
        {
            list_move(&pNode->mList, &pThiz->mIdleFrameList);
            nFindFlag++;
            break;
        }
    }
    if(1 == nFindFlag)
    {
        if(pThiz->mWaitIdleFrameFlag)
        {
            pthread_cond_signal(&pThiz->mIdleFrameCond);
        }
    }
    else
    {
        aloge("fatal error! find [%d] nodes", nFindFlag);
        ret = -1;
    }
    pthread_mutex_unlock(&pThiz->mLock);
    return ret;
}

static VIDEO_FRAME_INFO_S* SampleViG2d_VideoFrameManager_GetReadyFrame(SampleViG2d_VideoFrameManager *pThiz, int nTimeout)
{
    int ret;
    VideoFrameInfoNode* pNode = NULL;
    VIDEO_FRAME_INFO_S *pFrameInfo = NULL;
    pthread_mutex_lock(&pThiz->mLock);
_retry:
    if(!list_empty(&pThiz->mReadyFrameList))
    {
        pNode = list_first_entry(&pThiz->mReadyFrameList, VideoFrameInfoNode, mList);
        pFrameInfo = &pNode->VFrame;
        list_move_tail(&pNode->mList, &pThiz->mUsingFrameList);
    }
    else
    {
        if(0 == nTimeout)
        {
            pFrameInfo = NULL;
        }
        else if(nTimeout < 0)
        {
            pThiz->mWaitReadyFrameFlag = true;
            while(list_empty(&pThiz->mReadyFrameList))
            {
                pthread_cond_wait(&pThiz->mReadyFrameCond, &pThiz->mLock);
            }
            pThiz->mWaitReadyFrameFlag = false;
            goto _retry;
        }
        else
        {
            pThiz->mWaitReadyFrameFlag = true;
            ret = pthread_cond_wait_timeout(&pThiz->mReadyFrameCond, &pThiz->mLock, nTimeout);
            if(ETIMEDOUT == ret)
            {
                alogv("wait output frame timeout[%d]ms, ret[%d]", nTimeout, ret);
                pFrameInfo = NULL;
                pThiz->mWaitReadyFrameFlag = false;
            }
            else if(0 == ret)
            {
                pThiz->mWaitReadyFrameFlag = false;
                goto _retry;
            }
            else
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                pFrameInfo = NULL;
                pThiz->mWaitReadyFrameFlag = false;
            }
        }
    }
    pthread_mutex_unlock(&pThiz->mLock);
    return pFrameInfo;
}
static int SampleViG2d_VideoFrameManager_UsingFrameDone(SampleViG2d_VideoFrameManager *pThiz, VIDEO_FRAME_INFO_S *pFrame)
{
    int ret = 0;
    int nFindFlag = 0;
    VideoFrameInfoNode *pNode = NULL, *pTemp;
    pthread_mutex_lock(&pThiz->mLock);
    list_for_each_entry_safe(pNode, pTemp, &pThiz->mUsingFrameList, mList)
    {
        if(pNode->VFrame.mId == pFrame->mId)
        {
            list_move_tail(&pNode->mList, &pThiz->mIdleFrameList);
            nFindFlag++;
            break;
        }
    }
    if(1 == nFindFlag)
    {
        if(pThiz->mWaitIdleFrameFlag)
        {
            pthread_cond_signal(&pThiz->mIdleFrameCond);
        }
    }
    else
    {
        aloge("fatal error! find [%d] nodes", nFindFlag);
        ret = -1;
    }
    pthread_mutex_unlock(&pThiz->mLock);
    return ret;
}
static int SampleViG2d_VideoFrameManager_QueryIdleFrameNum(SampleViG2d_VideoFrameManager *pThiz)
{
    int cnt = 0;
    struct list_head *pList;
    pthread_mutex_lock(&pThiz->mLock);
    list_for_each(pList, &pThiz->mIdleFrameList)
    {
        cnt++;
    }
    pthread_mutex_unlock(&pThiz->mLock);
    return cnt;
}
static int SampleViG2d_VideoFrameManager_QueryFillingFrameNum(SampleViG2d_VideoFrameManager *pThiz)
{
    int cnt = 0;
    struct list_head *pList;
    pthread_mutex_lock(&pThiz->mLock);
    list_for_each(pList, &pThiz->mFillingFrameList)
    {
        cnt++;
    }
    pthread_mutex_unlock(&pThiz->mLock);
    return cnt;
}
static int SampleViG2d_VideoFrameManager_QueryReadyFrameNum(SampleViG2d_VideoFrameManager *pThiz)
{
    int cnt = 0;
    struct list_head *pList;
    pthread_mutex_lock(&pThiz->mLock);
    list_for_each(pList, &pThiz->mReadyFrameList)
    {
        cnt++;
    }
    pthread_mutex_unlock(&pThiz->mLock);
    return cnt;
}
static int SampleViG2d_VideoFrameManager_QueryUsingFrameNum(SampleViG2d_VideoFrameManager *pThiz)
{
    int cnt = 0;
    struct list_head *pList;
    pthread_mutex_lock(&pThiz->mLock);
    list_for_each(pList, &pThiz->mUsingFrameList)
    {
        cnt++;
    }
    pthread_mutex_unlock(&pThiz->mLock);
    return cnt;
}

SampleViG2d_VideoFrameManager *constructSampleViG2d_VideoFrameManager(int nFrameNum, PIXEL_FORMAT_E nPixelFormat, int nBufWidth, int nBufHeight)
{
    int ret;
    VideoFrameInfoNode *pNode, *pTemp;
    SampleViG2d_VideoFrameManager *pManager = (SampleViG2d_VideoFrameManager*)malloc(sizeof(SampleViG2d_VideoFrameManager));
    if(NULL == pManager)
    {
        aloge("fatal error! malloc fail!");
        return NULL;
    }
    memset(pManager, 0, sizeof(*pManager));
    INIT_LIST_HEAD(&pManager->mIdleFrameList);
    INIT_LIST_HEAD(&pManager->mFillingFrameList);
    INIT_LIST_HEAD(&pManager->mReadyFrameList);
    INIT_LIST_HEAD(&pManager->mUsingFrameList);
    ret = pthread_mutex_init(&pManager->mLock, NULL);
    if(ret != 0)
    {
        aloge("fatal error! pthread mutex init fail[%d]", ret);
        goto _err0;
    }
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    ret = pthread_cond_init(&pManager->mIdleFrameCond, &condAttr);
    if(ret != 0)
    {
        aloge("pthread cond init fail!");
    }
    ret = pthread_cond_init(&pManager->mReadyFrameCond, &condAttr);
    if(ret != 0)
    {
        aloge("pthread cond init fail!");
    }
    pManager->mFrameNodeNum = nFrameNum;
    pManager->mPixelFormat = nPixelFormat;
    pManager->mBufWidth = nBufWidth;
    pManager->mBufHeight = nBufHeight;
    int i;
    for(i = 0; i < pManager->mFrameNodeNum; i++)
    {
        pNode = (VideoFrameInfoNode*)malloc(sizeof(VideoFrameInfoNode));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
            goto _err1;
        }
        memset(pNode, 0, sizeof(*pNode));
        list_add_tail(&pNode->mList, &pManager->mIdleFrameList);
        pNode->VFrame.mId = i;
        pNode->VFrame.VFrame.mWidth = pManager->mBufWidth;
        pNode->VFrame.VFrame.mHeight = pManager->mBufHeight;
        pNode->VFrame.VFrame.mPixelFormat = pManager->mPixelFormat;
        if(pManager->mBufWidth * pManager->mBufHeight > 0)
        {
            switch(pManager->mPixelFormat)
            {
                case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
                case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
                {
                    ret = AW_MPI_SYS_MmzAlloc_Cached(&pNode->VFrame.VFrame.mPhyAddr[0], &pNode->VFrame.VFrame.mpVirAddr[0], pManager->mBufWidth*pManager->mBufHeight);
                    if(ret != SUCCESS)
                    {
                        pNode->VFrame.VFrame.mPhyAddr[0] = 0;
                        pNode->VFrame.VFrame.mpVirAddr[0] = NULL;
                        aloge("fatal error! phy alloc fail:%d", ret);
                        goto _err1;
                    }
                    ret = AW_MPI_SYS_MmzAlloc_Cached(&pNode->VFrame.VFrame.mPhyAddr[1], &pNode->VFrame.VFrame.mpVirAddr[1], pManager->mBufWidth*pManager->mBufHeight/2);
                    if(ret != SUCCESS)
                    {
                        pNode->VFrame.VFrame.mPhyAddr[1] = 0;
                        pNode->VFrame.VFrame.mpVirAddr[1] = NULL;
                        aloge("fatal error! phy alloc fail:%d", ret);
                        goto _err1;
                    }
                    break;
                }
                case MM_PIXEL_FORMAT_YUV_PLANAR_420:
                case MM_PIXEL_FORMAT_YVU_PLANAR_420:
                {
                    ret = AW_MPI_SYS_MmzAlloc_Cached(&pNode->VFrame.VFrame.mPhyAddr[0], &pNode->VFrame.VFrame.mpVirAddr[0], pManager->mBufWidth*pManager->mBufHeight);
                    if(ret != SUCCESS)
                    {
                        pNode->VFrame.VFrame.mPhyAddr[0] = 0;
                        pNode->VFrame.VFrame.mpVirAddr[0] = NULL;
                        aloge("fatal error! phy alloc fail:%d", ret);
                        goto _err1;
                    }
                    ret = AW_MPI_SYS_MmzAlloc_Cached(&pNode->VFrame.VFrame.mPhyAddr[1], &pNode->VFrame.VFrame.mpVirAddr[1], pManager->mBufWidth*pManager->mBufHeight/4);
                    if(ret != SUCCESS)
                    {
                        pNode->VFrame.VFrame.mPhyAddr[1] = 0;
                        pNode->VFrame.VFrame.mpVirAddr[1] = NULL;
                        aloge("fatal error! phy alloc fail:%d", ret);
                        goto _err1;
                    }
                    ret = AW_MPI_SYS_MmzAlloc_Cached(&pNode->VFrame.VFrame.mPhyAddr[2], &pNode->VFrame.VFrame.mpVirAddr[2], pManager->mBufWidth*pManager->mBufHeight/4);
                    if(ret != SUCCESS)
                    {
                        pNode->VFrame.VFrame.mPhyAddr[2] = 0;
                        pNode->VFrame.VFrame.mpVirAddr[2] = NULL;
                        aloge("fatal error! phy alloc fail:%d", ret);
                        goto _err1;
                    }
                    break;
                }
                default:
                {
                    aloge("fatal error! not support pixel format:0x%x", pManager->mPixelFormat);
                    goto _err1;
                }
            }
        }
    }
    pManager->GetIdleFrame          = SampleViG2d_VideoFrameManager_GetIdleFrame;
    pManager->FillingFrameDone      = SampleViG2d_VideoFrameManager_FillingFrameDone;
    pManager->FillingFrameFail      = SampleViG2d_VideoFrameManager_FillingFrameFail;
    pManager->GetReadyFrame         = SampleViG2d_VideoFrameManager_GetReadyFrame;
    pManager->UsingFrameDone        = SampleViG2d_VideoFrameManager_UsingFrameDone;
    pManager->QueryIdleFrameNum     = SampleViG2d_VideoFrameManager_QueryIdleFrameNum;
    pManager->QueryFillingFrameNum  = SampleViG2d_VideoFrameManager_QueryFillingFrameNum;
    pManager->QueryReadyFrameNum    = SampleViG2d_VideoFrameManager_QueryReadyFrameNum;
    pManager->QueryUsingFrameNum    = SampleViG2d_VideoFrameManager_QueryUsingFrameNum;
    return pManager;

_err1:
    list_for_each_entry_safe(pNode, pTemp, &pManager->mIdleFrameList, mList)
    {
        for(i=0;i<3;i++)
        {
            if(pNode->VFrame.VFrame.mpVirAddr[i] != NULL)
            {
                AW_MPI_SYS_MmzFree(pNode->VFrame.VFrame.mPhyAddr[i], pNode->VFrame.VFrame.mpVirAddr[i]);
            }
        }
        list_del(&pNode->mList);
        free(pNode);
    }
    pthread_mutex_destroy(&pManager->mLock);
_err0:
    free(pManager);
    pManager = NULL;
    return NULL;
}

void destructSampleViG2d_VideoFrameManager(SampleViG2d_VideoFrameManager *pThiz)
{
    VideoFrameInfoNode *pEntry, *pTemp;
    pthread_mutex_lock(&pThiz->mLock);
    int i;
    int cnt = 0;
    list_for_each_entry_safe(pEntry, pTemp, &pThiz->mUsingFrameList, mList)
    {
        list_move_tail(&pEntry->mList, &pThiz->mIdleFrameList);
        cnt++;
    }
    if(cnt > 0)
    {
        aloge("fatal error! usingFrameList has [%d] entries.", cnt);
    }
    cnt = 0;
    list_for_each_entry_safe(pEntry, pTemp, &pThiz->mReadyFrameList, mList)
    {
        list_move_tail(&pEntry->mList, &pThiz->mIdleFrameList);
        cnt++;
    }
    if(cnt > 0)
    {
        alogw("Be careful! readyFrameList has [%d] entries.", cnt);
    }
    cnt = 0;
    list_for_each_entry_safe(pEntry, pTemp, &pThiz->mFillingFrameList, mList)
    {
        list_move_tail(&pEntry->mList, &pThiz->mIdleFrameList);
        cnt++;
    }
    if(cnt > 0)
    {
        aloge("fatal error! fillingFrameList has [%d] entries.", cnt);
    }
    cnt = 0;
    list_for_each_entry_safe(pEntry, pTemp, &pThiz->mIdleFrameList, mList)
    {
        for(i=0;i<3;i++)
        {
            if(pEntry->VFrame.VFrame.mpVirAddr[i] != NULL)
            {
                AW_MPI_SYS_MmzFree(pEntry->VFrame.VFrame.mPhyAddr[i], pEntry->VFrame.VFrame.mpVirAddr[i]);
            }
        }
        list_del(&pEntry->mList);
        free(pEntry);
        cnt++;
    }
    if(cnt != pThiz->mFrameNodeNum)
    {
        aloge("fatal error! idle frame number is not match! [%d!=%d].", cnt, pThiz->mFrameNodeNum);
    }
    pthread_mutex_unlock(&pThiz->mLock);
    pthread_mutex_destroy(&pThiz->mLock);
    pthread_cond_destroy(&pThiz->mIdleFrameCond);
    pthread_cond_destroy(&pThiz->mReadyFrameCond);
    free(pThiz);
}

SampleViG2dContext *constructSampleViG2dContext()
{
    int ret;
    SampleViG2dContext *pThiz = (SampleViG2dContext*)malloc(sizeof(SampleViG2dContext));
    if(NULL == pThiz)
    {
        aloge("fatal error! malloc fail!");
        return NULL;
    }
    memset(pThiz, 0, sizeof(SampleViG2dContext));
    initSampleViCapS(&pThiz->mViCapCtx);
    pThiz->mViCapCtx.mpContext = (void*)pThiz;
    initSampleVoDisplayS(&pThiz->mVoDisplayCtx);
    pThiz->mVoDisplayCtx.mpContext = (void*)pThiz;

    ret = cdx_sem_init(&pThiz->mSemExit, 0);
    if(ret != 0)
    {
        aloge("fatal error! cdx sem init fail[%d]", ret);
    }
    return pThiz;
}

void destructSampleViG2dContext(SampleViG2dContext *pThiz)
{
    if(pThiz != NULL)
    {
        destroySampleVoDisplayS(&pThiz->mVoDisplayCtx);
        destroySampleViCapS(&pThiz->mViCapCtx);
        if(pThiz->mpFrameManager)
        {
            alogw("fatal error! video frame manager still exist? destruct it!");
            destructSampleViG2d_VideoFrameManager(pThiz->mpFrameManager);
            pThiz->mpFrameManager = NULL;
        }
        cdx_sem_deinit(&pThiz->mSemExit);
        free(pThiz);
    }
}

static ERRORTYPE SampleViG2d_VICallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleViCapS *pViCapCtx = (SampleViCapS*)cookie;
    if(MOD_ID_VIU == pChn->mModId)
    {
        if(pViCapCtx->mDev != pChn->mDevId || pViCapCtx->mChn != pChn->mChnId)
        {
            aloge("fatal error! viG2d viCallback don't match [%d,%d]!=[%d,%d]", pViCapCtx->mDev, pViCapCtx->mChn, pChn->mDevId, pChn->mChnId);
        }
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                alogd("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
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
        aloge("fatal error! unknown event[0x%x] from channel[0x%x,0x%x,0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
    }
    return SUCCESS;
}

static ERRORTYPE SampleViG2d_VOCallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    int nRet;
    SampleVoDisplayS *pVoDisplayCtx = (SampleVoDisplayS*)cookie;
    SampleViG2dContext *pContext = pVoDisplayCtx->mpContext;
    if(MOD_ID_VOU == pChn->mModId)
    {
        //alogd("VO callback: VO Layer[%d] chn[%d] event:%d", pChn->mDevId, pChn->mChnId, event);
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                //alogd("vo layer[%d] release frame id[0x%x]!", pChn->mDevId, pFrameInfo->mId);
                nRet = pContext->mpFrameManager->UsingFrameDone(pContext->mpFrameManager, pFrameInfo);
                if(nRet != 0)
                {
                    aloge("fatal error! frame id[%d] using frame done fail!", pFrameInfo->mId);
                }
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
                aloge("fatal error! unknown event[0x%x] from channel[0x%x,0x%x,0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
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

static SampleViG2dContext *gpSampleViG2dContext = NULL;
static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleViG2dContext)
    {
        cdx_sem_up(&gpSampleViG2dContext->mSemExit);
    }
}

int main(int argc, char *argv[])
{
    int ret = 0;
    void *pValue = NULL;
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

    alogd("hello, sample_vi_g2d.");
    MPPCallbackInfo cbInfo;
    SampleViG2dContext *pContext = constructSampleViG2dContext();
    if(NULL == pContext)
    {
        ret = -1;
        goto _err0;
    }
    gpSampleViG2dContext = pContext;
    
    char *pConfigFilePath;
    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        ret = -1;
        goto _err1;
    }

    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }
    /* parse config file. */
    if(loadSampleViG2dConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        ret = -1;
        goto _err1;
    }
    
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    ret = AW_MPI_SYS_Init();
    if (ret < 0)
    {
        aloge("sys Init failed!");
        ret = -1;
        goto _err1;
    }
    //prepare frame manager
    pContext->mpFrameManager = constructSampleViG2d_VideoFrameManager(5, pContext->mConfigPara.mPicFormat, pContext->mConfigPara.mDstWidth, pContext->mConfigPara.mDstHeight);
    if(NULL == pContext->mpFrameManager)
    {
        aloge("fatal error! malloc fail!");
        ret = -1;
        goto _err2;
    }
    //begin Vi capture and g2d rotate.
    /*Set VI Channel Attribute*/
    memset(&pContext->mViCapCtx.mViAttr, 0, sizeof(VI_ATTR_S));
    pContext->mViCapCtx.mDev = pContext->mConfigPara.mDevNum;
    pContext->mViCapCtx.mChn = 0;
    pContext->mViCapCtx.mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mViCapCtx.mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mViCapCtx.mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mPicFormat); // V4L2_PIX_FMT_NV21M; 
    pContext->mViCapCtx.mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mViCapCtx.mViAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    pContext->mViCapCtx.mViAttr.format.width = pContext->mConfigPara.mSrcWidth;
    pContext->mViCapCtx.mViAttr.format.height = pContext->mConfigPara.mSrcHeight;
    pContext->mViCapCtx.mViAttr.nbufs = 3;//5;
    pContext->mViCapCtx.mViAttr.nplanes = 2;
    pContext->mViCapCtx.mViAttr.fps = pContext->mConfigPara.mFrameRate;
    pContext->mViCapCtx.mViAttr.use_current_win = 0; // update configuration anyway, do not use current configuration
    pContext->mViCapCtx.mViAttr.wdr_mode = 0;
    pContext->mViCapCtx.mViAttr.capturemode = V4L2_MODE_VIDEO; /* V4L2_MODE_VIDEO; V4L2_MODE_IMAGE; V4L2_MODE_PREVIEW */
    pContext->mViCapCtx.mViAttr.drop_frame_num = pContext->mConfigPara.mDropFrameNum;
    pContext->mViCapCtx.mIspDev = 0;
    pContext->mViCapCtx.mTimeout = 200;
    ret = AW_MPI_VI_CreateVipp(pContext->mViCapCtx.mDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! create vipp fail[%d]", ret);
    }
    cbInfo.cookie = (void*)&pContext->mViCapCtx;
    cbInfo.callback = (MPPCallbackFuncType)&SampleViG2d_VICallback;
    ret = AW_MPI_VI_RegisterCallback(pContext->mViCapCtx.mDev, &cbInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vipp[%d] RegisterCallback failed", pContext->mViCapCtx.mDev);
    }
    ret = AW_MPI_VI_SetVippAttr(pContext->mViCapCtx.mDev, &pContext->mViCapCtx.mViAttr);
    if(ret != SUCCESS)
    {
        aloge("fatal error! set vipp attr fail[%d]", ret);
    }
    ret = AW_MPI_VI_EnableVipp(pContext->mViCapCtx.mDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! enable vipp fail[%d]", ret);
    }
#if ISP_RUN
    ret = AW_MPI_ISP_Run(pContext->mViCapCtx.mIspDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! isp run fail[%d]", ret);
    }
#endif
    ret = AW_MPI_VI_CreateVirChn(pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn, NULL);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Create VI Chn failed, VIDev = %d,VIChn = %d", pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    }
    ret = AW_MPI_VI_EnableVirChn(pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! VI Enable VirChn failed,VIDev = %d,VIChn = %d", pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    }

    pContext->mViCapCtx.mpG2dConvert = constructSampleViG2d_G2dConvert();
    if(NULL == pContext->mViCapCtx.mpG2dConvert)
    {
        aloge("fatal error! create g2dConvert fail[%d]", ret);
        goto _err3;
    }
    ret = pContext->mViCapCtx.mpG2dConvert->G2dOpen();
    if(ret != 0)
    {
        aloge("fatal error! g2d open fail[%d]", ret);
        goto _err4;
    }
    G2dConvertParam stConvertParam;
    memset(&stConvertParam, 0, sizeof(stConvertParam));
    stConvertParam.mSrcRectX = pContext->mConfigPara.mSrcRectX;
    stConvertParam.mSrcRectY = pContext->mConfigPara.mSrcRectY;
    stConvertParam.mSrcRectW = pContext->mConfigPara.mSrcRectW;
    stConvertParam.mSrcRectH = pContext->mConfigPara.mSrcRectH;
    stConvertParam.mDstRotate = pContext->mConfigPara.mDstRotate;
    stConvertParam.mDstWidth = pContext->mConfigPara.mDstWidth;
    stConvertParam.mDstHeight = pContext->mConfigPara.mDstHeight;
    stConvertParam.mDstRectX = pContext->mConfigPara.mDstRectX;
    stConvertParam.mDstRectY = pContext->mConfigPara.mDstRectY;
    stConvertParam.mDstRectW = pContext->mConfigPara.mDstRectW;
    stConvertParam.mDstRectH = pContext->mConfigPara.mDstRectH;
    pContext->mViCapCtx.mpG2dConvert->G2dSetConvertParam(pContext->mViCapCtx.mpG2dConvert, &stConvertParam);

    ret = pthread_create(&pContext->mViCapCtx.mThreadId, NULL, GetCSIFrameThread, (void *)&pContext->mViCapCtx);
    if (0 == ret)
    {
        pContext->mViCapCtx.mbThreadExistFlag = true;
        alogd("vipp[%d]virChn[%d]: get csi frame thread id=[0x%x]", pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn, pContext->mViCapCtx.mThreadId);
    }
    else
    {
        aloge("fatal error! pthread_create failed, vipp[%d]virChn[%d]", pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
        goto _err4;
    }

    //begin vo display
    pContext->mVoDisplayCtx.mVoDev = 0;
    pContext->mVoDisplayCtx.mUILayer = HLAY(2, 0);
    AW_MPI_VO_Enable(pContext->mVoDisplayCtx.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pContext->mVoDisplayCtx.mUILayer);
    AW_MPI_VO_CloseVideoLayer(pContext->mVoDisplayCtx.mUILayer); /* close ui layer. */
    AW_MPI_VO_GetPubAttr(pContext->mVoDisplayCtx.mVoDev, &pContext->mVoDisplayCtx.mPubAttr);
    pContext->mVoDisplayCtx.mPubAttr.enIntfType = VO_INTF_LCD;
    pContext->mVoDisplayCtx.mPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(pContext->mVoDisplayCtx.mVoDev, &pContext->mVoDisplayCtx.mPubAttr);

    pContext->mVoDisplayCtx.mVoLayer = 0;
    ret = AW_MPI_VO_EnableVideoLayer(pContext->mVoDisplayCtx.mVoLayer);
    if(ret != SUCCESS)
    {
        aloge("fatal error! enable video layer[%d] fail!", pContext->mVoDisplayCtx.mVoLayer);
        pContext->mVoDisplayCtx.mVoLayer = MM_INVALID_LAYER;
        goto _err5;
    }
    AW_MPI_VO_GetVideoLayerAttr(pContext->mVoDisplayCtx.mVoLayer, &pContext->mVoDisplayCtx.mLayerAttr);
    pContext->mVoDisplayCtx.mLayerAttr.stDispRect.X = pContext->mConfigPara.mDisplayX;
    pContext->mVoDisplayCtx.mLayerAttr.stDispRect.Y = pContext->mConfigPara.mDisplayY;
    pContext->mVoDisplayCtx.mLayerAttr.stDispRect.Width = pContext->mConfigPara.mDisplayW;
    pContext->mVoDisplayCtx.mLayerAttr.stDispRect.Height = pContext->mConfigPara.mDisplayH;
    AW_MPI_VO_SetVideoLayerAttr(pContext->mVoDisplayCtx.mVoLayer, &pContext->mVoDisplayCtx.mLayerAttr);

    pContext->mVoDisplayCtx.mVOChn = 0;
    ret = AW_MPI_VO_CreateChn(pContext->mVoDisplayCtx.mVoLayer, pContext->mVoDisplayCtx.mVOChn);
    if(ret != SUCCESS)
    {
        pContext->mVoDisplayCtx.mVOChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        goto _err6;
    }
    cbInfo.cookie = (void*)&pContext->mVoDisplayCtx;
    cbInfo.callback = (MPPCallbackFuncType)&SampleViG2d_VOCallback;
    AW_MPI_VO_RegisterCallback(pContext->mVoDisplayCtx.mVoLayer, pContext->mVoDisplayCtx.mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(pContext->mVoDisplayCtx.mVoLayer, pContext->mVoDisplayCtx.mVOChn, 2);
    ret = AW_MPI_VO_StartChn(pContext->mVoDisplayCtx.mVoLayer, pContext->mVoDisplayCtx.mVOChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo start chn fail[%d]!", ret);
    }
    ret = pthread_create(&pContext->mVoDisplayCtx.mThreadId, NULL, DisplayThread, (void *)&pContext->mVoDisplayCtx);
    if (0 == ret)
    {
        pContext->mVoDisplayCtx.mbThreadExistFlag = true;
        alogd("pthread create display threadId[0x%x]", pContext->mVoDisplayCtx.mThreadId);
    }
    else
    {
        aloge("fatal error! pthread_create failed");
        goto _err7;
    }
        
    //wait exit
    if(pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    //begin to exit.
    pContext->mbExitFlag = true;
    ret = pthread_join(pContext->mVoDisplayCtx.mThreadId, (void*)&pValue);
    alogd("VoDisplay threadId[0x%x] exit[%d], pValue[%p]", pContext->mVoDisplayCtx.mThreadId, ret, pValue);
    pContext->mVoDisplayCtx.mbThreadExistFlag = false;
    ret = pthread_join(pContext->mViCapCtx.mThreadId, (void*)&pValue);
    aloge("ViCap threadId[0x%x] exit[%d], pValue[%p]", pContext->mViCapCtx.mThreadId, ret, pValue);
    pContext->mViCapCtx.mbThreadExistFlag = false;
    ret = AW_MPI_VO_StopChn(pContext->mVoDisplayCtx.mVoLayer, pContext->mVoDisplayCtx.mVOChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo stop chn fail[%d]!", ret);
    }
    ret = AW_MPI_VO_DestroyChn(pContext->mVoDisplayCtx.mVoLayer, pContext->mVoDisplayCtx.mVOChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! disable vo channel fail[%d]!", ret);
    }
    ret = AW_MPI_VO_DisableVideoLayer(pContext->mVoDisplayCtx.mVoLayer);
    if(ret != SUCCESS)
    {
        aloge("fatal error! disable video layer[%d] fail!", pContext->mVoDisplayCtx.mVoLayer);
    }
    ret = AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mVoDisplayCtx.mUILayer);
    if(ret != SUCCESS)
    {
        aloge("fatal error! remove outside video layer[%d] fail[%d]!", pContext->mVoDisplayCtx.mVoLayer, ret);
    }
    /* disalbe vo dev */
    ret = AW_MPI_VO_Disable(pContext->mVoDisplayCtx.mVoDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo disable fail[%d]!", ret);
    }
    pContext->mVoDisplayCtx.mVoDev = -1;
    destructSampleViG2d_G2dConvert(pContext->mViCapCtx.mpG2dConvert);
    pContext->mViCapCtx.mpG2dConvert = NULL;
    ret = AW_MPI_VI_DisableVirChn(pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! VI disable VirChn failed,VIDev = %d,VIChn = %d", pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    }
    ret = AW_MPI_VI_DestroyVirChn(pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Destroy VI Chn failed, VIDev = %d,VIChn = %d", pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    }
    ret = AW_MPI_VI_DisableVipp(pContext->mViCapCtx.mDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Disable vipp fail[%d]", ret);
    }
    ret = AW_MPI_VI_DestroyVipp(pContext->mViCapCtx.mDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! destroy vipp fail[%d]", ret);
    }
#if ISP_RUN
    ret = AW_MPI_ISP_Stop(pContext->mViCapCtx.mIspDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! isp stop fail[%d]", ret);
    }
#endif
    destructSampleViG2d_VideoFrameManager(pContext->mpFrameManager);
    pContext->mpFrameManager = NULL;
    ret = AW_MPI_SYS_Exit();
    if (ret < 0)
    {
        aloge("fatal error! sys exit failed!");
    }
    destructSampleViG2dContext(pContext);
    pContext = NULL;
    gpSampleViG2dContext = NULL;
    log_quit();
    return ret;

_err7:
    ret = AW_MPI_VO_StopChn(pContext->mVoDisplayCtx.mVoLayer, pContext->mVoDisplayCtx.mVOChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo stop chn fail[%d]!", ret);
    }
    ret = AW_MPI_VO_DestroyChn(pContext->mVoDisplayCtx.mVoLayer, pContext->mVoDisplayCtx.mVOChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! disable vo channel fail[%d]!", ret);
    }
_err6:
    ret = AW_MPI_VO_DisableVideoLayer(pContext->mVoDisplayCtx.mVoLayer);
    if(ret != SUCCESS)
    {
        aloge("fatal error! disable video layer[%d] fail!", pContext->mVoDisplayCtx.mVoLayer);
    }
    ret = AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mVoDisplayCtx.mUILayer);
    if(ret != SUCCESS)
    {
        aloge("fatal error! remove outside video layer[%d] fail[%d]!", pContext->mVoDisplayCtx.mVoLayer, ret);
    }
    /* disalbe vo dev */
    ret = AW_MPI_VO_Disable(pContext->mVoDisplayCtx.mVoDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo disable fail[%d]!", ret);
    }
    pContext->mVoDisplayCtx.mVoDev = -1;
_err5:
    pContext->mbExitFlag = true;
    ret = pthread_join(pContext->mViCapCtx.mThreadId, (void*)&pValue);
    if(ret != 0)
    {
        aloge("fatal error! ViCap threadId[0x%x] join fail[%d]", pContext->mViCapCtx.mThreadId, ret);
    }
_err4:
    destructSampleViG2d_G2dConvert(pContext->mViCapCtx.mpG2dConvert);
    pContext->mViCapCtx.mpG2dConvert = NULL;
_err3:
    ret = AW_MPI_VI_DisableVirChn(pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! VI disable VirChn failed,VIDev = %d,VIChn = %d", pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    }
    ret = AW_MPI_VI_DestroyVirChn(pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Destroy VI Chn failed, VIDev = %d,VIChn = %d", pContext->mViCapCtx.mDev, pContext->mViCapCtx.mChn);
    }
    ret = AW_MPI_VI_DisableVipp(pContext->mViCapCtx.mDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Disable vipp fail[%d]", ret);
    }
    ret = AW_MPI_VI_DestroyVipp(pContext->mViCapCtx.mDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! destroy vipp fail[%d]", ret);
    }
#if ISP_RUN
    ret = AW_MPI_ISP_Stop(pContext->mViCapCtx.mIspDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! isp stop fail[%d]", ret);
    }
#endif
    destructSampleViG2d_VideoFrameManager(pContext->mpFrameManager);
    pContext->mpFrameManager = NULL;
_err2:
    ret = AW_MPI_SYS_Exit();
    if (ret < 0)
    {
        aloge("fatal error! sys exit failed!");
    }
_err1:
    destructSampleViG2dContext(pContext);
    pContext = NULL;
    gpSampleViG2dContext = NULL;
_err0:
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    log_quit();
    return ret;
}

