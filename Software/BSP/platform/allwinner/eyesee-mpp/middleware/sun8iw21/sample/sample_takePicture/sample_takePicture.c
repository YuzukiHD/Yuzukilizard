/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_takePicture.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/1/5
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

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

#include <utils/plat_log.h>
#include "media/mpi_sys.h"
#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include <mpi_venc.h>
#include <utils/VIDEO_FRAME_INFO_S.h>
#include <confparser.h>
#include "sample_takePicture.h"
#include "sample_takePicture_config.h"

#define ISP_RUN 1

static int hal_virvi_start(VI_DEV ViDev, VI_CHN ViCh, void *pAttr)
{
    int ret = -1;

    ret = AW_MPI_VI_CreateVirChn(ViDev, ViCh, pAttr);
    if(ret < 0)
    {
        aloge("Create VI Chn failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }
    ret = AW_MPI_VI_SetVirChnAttr(ViDev, ViCh, pAttr);
    if(ret < 0)
    {
        aloge("Set VI ChnAttr failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }
    ret = AW_MPI_VI_EnableVirChn(ViDev, ViCh);
    if(ret < 0)
    {
        aloge("VI Enable VirChn failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }

    return 0;
}

static int hal_virvi_end(VI_DEV ViDev, VI_CHN ViCh)
{
    int ret = -1;
    ret = AW_MPI_VI_DisableVirChn(ViDev, ViCh);
    if(ret < 0)
    {
        aloge("Disable VI Chn failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }
    ret = AW_MPI_VI_DestroyVirChn(ViDev, ViCh);
    if(ret < 0)
    {
        aloge("Destory VI Chn failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleTakePictureCmdLineParam *pCmdLinePara)
{
    alogd("sample virvi path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleTakePictureCmdLineParam));
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
                "\t-path /home/sample_takePicture.conf\n");
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

static ERRORTYPE loadConfigPara(SampleTakePictureConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;
    memset(pConfig, 0, sizeof(SampleTakePictureConfig));
    pConfig->DevNum = 0;
    pConfig->FrameRate = 60;
    pConfig->SrcWidth = 1920;
    pConfig->SrcHeight = 1080;
    pConfig->SrcFormat = V4L2_PIX_FMT_NV21M;
    pConfig->mColorSpace = V4L2_COLORSPACE_JPEG;
    pConfig->mJpegWidth = pConfig->SrcWidth;
    pConfig->mJpegHeight = pConfig->SrcHeight;
    strcpy(pConfig->mStoreDirectory, "/mnt/extsd");
    pConfig->mViDropFrmCnt = 0;
    pConfig->mEncodeRotate = 0;
    pConfig->mHorizonFlipFlag = FALSE;

    if(conf_path != NULL)
    {
        CONFPARSER_S stConfParser;
        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }
        pConfig->DevNum = GetConfParaInt(&stConfParser, CFG_Dev_Num, 0);
        pConfig->FrameRate = GetConfParaInt(&stConfParser, CFG_Frame_Rate, 0);
        pConfig->SrcWidth = GetConfParaInt(&stConfParser, CFG_Src_Width, 0);
        pConfig->SrcHeight = GetConfParaInt(&stConfParser, CFG_Src_Height, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_Src_Format, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "nv21"))
            {
                pConfig->SrcFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yv12"))
            {
                pConfig->SrcFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            }
            else if (!strcmp(ptr, "nv12"))
            {
                pConfig->SrcFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yu12"))
            {
                pConfig->SrcFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
            }
            /* aw compression format */
            else if (!strcmp(ptr, "aw_afbc"))
            {
                pConfig->SrcFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
            }
            else if (!strcmp(ptr, "aw_lbc_2_0x"))
            {
                pConfig->SrcFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
            }
            else if (!strcmp(ptr, "aw_lbc_2_5x"))
            {
                pConfig->SrcFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_5x"))
            {
                pConfig->SrcFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_0x"))
            {
                pConfig->SrcFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
            }
            /* aw_package_422 NOT support */
            // else if (!strcmp(ptr, "aw_package_422"))
            // {
            //     pConfig->PicFormat = MM_PIXEL_FORMAT_YVYU_AW_PACKAGE_422;
            // }
            else
            {
                aloge("fatal error! wrong src pixfmt:%s", ptr);
                alogw("use the default pixfmt %d", pConfig->SrcFormat);
            }
        }

        ptr = (char *)GetConfParaString(&stConfParser, CFG_COLOR_SPACE, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "jpeg"))
            {
                pConfig->mColorSpace = V4L2_COLORSPACE_JPEG;
            }
            else if (!strcmp(ptr, "rec709"))
            {
                pConfig->mColorSpace = V4L2_COLORSPACE_REC709;
            }
            else if (!strcmp(ptr, "rec709_part_range"))
            {
                pConfig->mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
            }
            else
            {
                aloge("fatal error! wrong color space:%s", ptr);
                pConfig->mColorSpace = V4L2_COLORSPACE_JPEG;
            }
        }

        alogd("srcPixFmt=%d, ColorSpace=%d", pConfig->SrcFormat, pConfig->mColorSpace);
        
        pConfig->mViDropFrmCnt = GetConfParaInt(&stConfParser, CFG_ViDropFrmCnt, 0);
        alogd("ViDropFrameNum: %d", pConfig->mViDropFrmCnt);

        pConfig->mTakePictureMode = GetConfParaInt(&stConfParser, CFG_TakePictureMode, 0);
        pConfig->mTakePictureNum = GetConfParaInt(&stConfParser, CFG_TakePictureNum, 0);
        pConfig->mTakePictureInterval = GetConfParaInt(&stConfParser, CFG_TakePictureInterval, 0);

        pConfig->mJpegWidth = GetConfParaInt(&stConfParser, CFG_JpegWidth, 0);
        pConfig->mJpegHeight = GetConfParaInt(&stConfParser, CFG_JpegHeight, 0);
        
        ptr = (char*)GetConfParaString(&stConfParser, CFG_StoreDir, NULL);
        if (ptr)
            strcpy(pConfig->mStoreDirectory, ptr);

        alogd("dev_num=%d, src_width=%d, src_height=%d, frame_rate=%d,"
              "TakePictureMode=%d, TakePictureNum=%d, TakePictureInterval=%d, JpegWidth=%d, JpegHeight=%d, storDir=[%s]",
           pConfig->DevNum,pConfig->SrcWidth,pConfig->SrcHeight,pConfig->FrameRate,
           pConfig->mTakePictureMode, pConfig->mTakePictureNum, pConfig->mTakePictureInterval,
           pConfig->mJpegWidth, pConfig->mJpegHeight, pConfig->mStoreDirectory);

        pConfig->mEncodeRotate = GetConfParaInt(&stConfParser, CFG_ENCODE_ROTATE, 0);
        pConfig->mHorizonFlipFlag = GetConfParaInt(&stConfParser, CFG_MIRROR, 0);
        alogd("rorate:%d, mirror:%d", pConfig->mEncodeRotate, pConfig->mHorizonFlipFlag);

        pConfig->mCropEnable = GetConfParaInt(&stConfParser, CFG_CROP_ENABLE, 0);
        pConfig->mCropRectX = GetConfParaInt(&stConfParser, CFG_CROP_RECT_X, 0);
        pConfig->mCropRectY = GetConfParaInt(&stConfParser, CFG_CROP_RECT_Y, 0);
        pConfig->mCropRectWidth = GetConfParaInt(&stConfParser, CFG_CROP_RECT_WIDTH, 0);
        pConfig->mCropRectHeight = GetConfParaInt(&stConfParser, CFG_CROP_RECT_HEIGHT, 0);

        alogd("venc crop enable:%d, X:%d, Y:%d, Width:%d, Height:%d",
            pConfig->mCropEnable, pConfig->mCropRectX,
            pConfig->mCropRectY, pConfig->mCropRectWidth,
            pConfig->mCropRectHeight);

        destroyConfParser(&stConfParser);
    }
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    TakePicture_Cap_S *pThiz = (TakePicture_Cap_S*)cookie;
    if(MOD_ID_VENC == pChn->mModId)
    {
        if(pChn->mChnId != pThiz->mChn)
        {
            aloge("fatal error! VO chnId[%d]!=[%d]", pChn->mChnId, pThiz->mChn);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                if(pThiz->mCurFrameId != pVideoFrameInfo->mId)
                {
                    aloge("fatal error! frameId is not match[%d]!=[%d]!", pThiz->mCurFrameId, pVideoFrameInfo->mId);
                }
                cdx_sem_up(&pThiz->mSemFrameBack);
                break;
            }
            case MPP_EVENT_VENC_BUFFER_FULL:
            {
                alogd("jpeg encoder chn[%d] vbvBuffer full", pChn->mChnId);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VENC_ILLEGAL_PARAM;
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

static int startJpegPicture(TakePicture_Cap_S *pCap)
{
    SampleTakePictureContext *pContext = (SampleTakePictureContext*)pCap->mpContext;

    //call mpi_venc to encode jpeg.
    int result = SUCCESS;
    ERRORTYPE ret;
    ERRORTYPE venc_ret = SUCCESS;
    unsigned int nVbvBufSize = 0;
    unsigned int vbvThreshSize = 0;
    unsigned int picWidth = pContext->mConfigPara.mJpegWidth;
    unsigned int picHeight = pContext->mConfigPara.mJpegHeight;
    unsigned int mPictureNum = 0;

    if (0 == pContext->mConfigPara.mTakePictureMode)
    {
        alogd("No photos mode.");
        return 0;
    }
    else if (1 == pContext->mConfigPara.mTakePictureMode)
    {
        mPictureNum = 1;
        pContext->mTakePicMode = TAKE_PICTURE_MODE_FAST;
        alogd("start jpeg take one picture at once.");
    }
    else if (2 == pContext->mConfigPara.mTakePictureMode)
    {
        mPictureNum = pContext->mConfigPara.mTakePictureNum;
        pContext->mTakePicMode = TAKE_PICTURE_MODE_CONTINUOUS;
        alogd("start jpeg continue to take %d pictures.", pContext->mConfigPara.mTakePictureNum);
    }
    else
    {
        aloge("fatal error, invalid TakePictureMode %d", pContext->mConfigPara.mTakePictureMode);
        return -1;
    }

    // calc vbv buf size and vbv threshold size
    unsigned int minVbvBufSize = picWidth * picHeight * 3/2;
    vbvThreshSize = picWidth * picHeight;
    nVbvBufSize = (picWidth * picHeight * 3/2 /10 * mPictureNum) + vbvThreshSize;
    if(nVbvBufSize < minVbvBufSize)
    {
        nVbvBufSize = minVbvBufSize;
    }
    if(nVbvBufSize > 16*1024*1024)
    {
        alogd("Be careful! vbvSize[%d]MB is too large, decrease to threshSize[%d]MB + 1MB", nVbvBufSize/(1024*1024), vbvThreshSize/(1024*1024));
        nVbvBufSize = vbvThreshSize + 1*1024*1024;
    }
    nVbvBufSize = ALIGN(nVbvBufSize, 1024);

    memset(&pCap->mChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    pCap->mChnAttr.VeAttr.Type = PT_JPEG;
    pCap->mChnAttr.VeAttr.AttrJpeg.MaxPicWidth = 0;
    pCap->mChnAttr.VeAttr.AttrJpeg.MaxPicHeight = 0;
    pCap->mChnAttr.VeAttr.AttrJpeg.BufSize = nVbvBufSize;
    pCap->mChnAttr.VeAttr.AttrJpeg.mThreshSize = vbvThreshSize;
    pCap->mChnAttr.VeAttr.AttrJpeg.bByFrame = TRUE;
    pCap->mChnAttr.VeAttr.AttrJpeg.PicWidth = picWidth;
    pCap->mChnAttr.VeAttr.AttrJpeg.PicHeight = picHeight;
    pCap->mChnAttr.VeAttr.AttrJpeg.bSupportDCF = FALSE;
    pCap->mChnAttr.VeAttr.MaxKeyInterval = 1;
    pCap->mChnAttr.VeAttr.SrcPicWidth = pContext->mConfigPara.SrcWidth;
    pCap->mChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.SrcHeight;
    pCap->mChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pCap->mChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.SrcFormat;
    pCap->mChnAttr.VeAttr.mColorSpace = pContext->mConfigPara.mColorSpace;
    alogd("pixfmt:0x%x, colorSpace:0x%x", pCap->mChnAttr.VeAttr.PixelFormat, pCap->mChnAttr.VeAttr.mColorSpace);
    pCap->mChnAttr.EncppAttr.mbEncppEnable = TRUE;
    switch(pContext->mConfigPara.mEncodeRotate)
    {
        case 90:
            pCap->mChnAttr.VeAttr.Rotate = ROTATE_90;
            break;
        case 180:
            pCap->mChnAttr.VeAttr.Rotate = ROTATE_180;
            break;
        case 270:
            pCap->mChnAttr.VeAttr.Rotate = ROTATE_270;
            break;
        default:
            pCap->mChnAttr.VeAttr.Rotate = ROTATE_NONE;
            break;
    }
    alogd("rotate:%d", pCap->mChnAttr.VeAttr.Rotate);
    bool bSuccessFlag = false;
    pCap->mChn = 0;
    while(pCap->mChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pCap->mChn, &pCap->mChnAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("create venc channel[%d] success!", pCap->mChn);
            break;
        }
        else if(ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pCap->mChn);
            pCap->mChn++;
        }
        else
        {
            aloge("fatal error! create venc channel[%d] ret[0x%x], find next!", pCap->mChn, ret);
            pCap->mChn++;
        }
    }
    if(false == bSuccessFlag)
    {
        pCap->mChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        result = FAILURE;
        goto _err0;
    }

    if (pContext->mConfigPara.mHorizonFlipFlag)
    {
        AW_MPI_VENC_SetHorizonFlip(pCap->mChn, pContext->mConfigPara.mHorizonFlipFlag);
        alogd("set HorizonFlip %d", pContext->mConfigPara.mHorizonFlipFlag);
    }

    if (pContext->mConfigPara.mCropEnable)
    {
        VENC_CROP_CFG_S stCropCfg;
        memset(&stCropCfg, 0, sizeof(VENC_CROP_CFG_S));
        stCropCfg.bEnable = pContext->mConfigPara.mCropEnable;
        stCropCfg.Rect.X = pContext->mConfigPara.mCropRectX;
        stCropCfg.Rect.Y = pContext->mConfigPara.mCropRectY;
        stCropCfg.Rect.Width = pContext->mConfigPara.mCropRectWidth;
        stCropCfg.Rect.Height = pContext->mConfigPara.mCropRectHeight;
        AW_MPI_VENC_SetCrop(pCap->mChn, &stCropCfg);
        alogd("set Crop %d, [%d][%d][%d][%d]", stCropCfg.bEnable, stCropCfg.Rect.X, stCropCfg.Rect.Y, stCropCfg.Rect.Width, stCropCfg.Rect.Height);
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pCap;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VENC_RegisterCallback(pCap->mChn, &cbInfo);

    memset(&pCap->mJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
    pCap->mJpegParam.Qfactor = 90;
    AW_MPI_VENC_SetJpegParam(pCap->mChn, &pCap->mJpegParam);
    AW_MPI_VENC_ForbidDiscardingFrame(pCap->mChn, TRUE);

    venc_ret = AW_MPI_VENC_StartRecvPic(pCap->mChn);
    if(SUCCESS != venc_ret)
    {
        aloge("fatal error:%x jpegEnc AW_MPI_VENC_StartRecvPic",venc_ret);
        result = FAILURE;
    }

    memset(&pCap->mExifInfo, 0, sizeof(VENC_EXIFINFO_S));
    time_t t;
    struct tm *tm_t;
    time(&t);
    tm_t = localtime(&t);
    snprintf((char*)pCap->mExifInfo.DateTime, MM_DATA_TIME_LENGTH, "%4d:%02d:%02d %02d:%02d:%02d",
        tm_t->tm_year+1900, tm_t->tm_mon+1, tm_t->tm_mday,
        tm_t->tm_hour, tm_t->tm_min, tm_t->tm_sec);
    pCap->mExifInfo.ThumbWidth = 320;
    pCap->mExifInfo.ThumbHeight = 240;
    pCap->mExifInfo.thumb_quality = 60;
    pCap->mExifInfo.Orientation = 0;
    pCap->mExifInfo.fr32FNumber = FRACTION32(10, 26);
    pCap->mExifInfo.MeteringMode = AVERAGE_AW_EXIF;
    pCap->mExifInfo.fr32FocalLength = FRACTION32(100, 228);
    pCap->mExifInfo.WhiteBalance = 0;
    pCap->mExifInfo.FocalLengthIn35mmFilm = 18;
    strcpy((char*)pCap->mExifInfo.ImageName, "aw-photo");
    AW_MPI_VENC_SetJpegExifInfo(pCap->mChn, &pCap->mExifInfo);

    alogd("start jpeg take picture");

    return result;

_err0:
    return result;
}

static int saveJpegPicture(TakePicture_Cap_S *pCap, VIDEO_FRAME_INFO_S *pFrameInfo, int jpegTestCnt, int continuousPictureCnt)
{
    SampleTakePictureContext *pContext = (SampleTakePictureContext*)pCap->mpContext;

    //call mpi_venc to encode jpeg.
    int result = SUCCESS;
    ERRORTYPE ret;

    pCap->mCurFrameId = pFrameInfo->mId;

    AW_MPI_VENC_SendFrame(pCap->mChn, pFrameInfo, 0);
    //cdx_sem_down(&mSemFrameBack);
    ret = cdx_sem_down_timedwait(&pCap->mSemFrameBack, 10*1000);
    if(ret != 0)
    {
        aloge("fatal error! jpeg encode error[0x%x]", ret);
        result = FAILURE;
    }

    ret = AW_MPI_VENC_GetStream(pCap->mChn, &pCap->mOutStream, 1000);
    if(ret != SUCCESS)
    {
        aloge("fatal error! why get stream fail?");
        result = FAILURE;
    }
    else
    {
        AW_MPI_VENC_GetJpegThumbBuffer(pCap->mChn, &pCap->mJpegThumbBuf);

        //make file name, e.g., /mnt/extsd/pic[0].jpg
        char strFilePath[MAX_FILE_PATH_SIZE];
        snprintf(strFilePath, MAX_FILE_PATH_SIZE, "%s/pic[%d][%d].jpg", pContext->mConfigPara.mStoreDirectory, jpegTestCnt, continuousPictureCnt);

        FILE *fpPic = fopen(strFilePath, "wb");
        if(fpPic != NULL)
        {
            if(pCap->mOutStream.mpPack[0].mpAddr0 != NULL && pCap->mOutStream.mpPack[0].mLen0 > 0)
            {
                fwrite(pCap->mOutStream.mpPack[0].mpAddr0, 1, pCap->mOutStream.mpPack[0].mLen0, fpPic);
                alogd("virAddr0=[%p], length=[%d]", pCap->mOutStream.mpPack[0].mpAddr0, pCap->mOutStream.mpPack[0].mLen0);
            }
            if(pCap->mOutStream.mpPack[0].mpAddr1 != NULL && pCap->mOutStream.mpPack[0].mLen1 > 0)
            {
                fwrite(pCap->mOutStream.mpPack[0].mpAddr1, 1, pCap->mOutStream.mpPack[0].mLen1, fpPic);
                alogd("virAddr1=[%p], length=[%d]", pCap->mOutStream.mpPack[0].mpAddr1, pCap->mOutStream.mpPack[0].mLen1);
            }
            if(pCap->mOutStream.mpPack[0].mpAddr2 != NULL && pCap->mOutStream.mpPack[0].mLen2 > 0)
            {
                fwrite(pCap->mOutStream.mpPack[0].mpAddr2, 1, pCap->mOutStream.mpPack[0].mLen2, fpPic);
                alogd("virAddr2=[%p], length=[%d]", pCap->mOutStream.mpPack[0].mpAddr2, pCap->mOutStream.mpPack[0].mLen2);
            }
            fsync(fpPic);
            fclose(fpPic);
            alogd("store jpeg in file[%s]", strFilePath);
        }
        else
        {
            aloge("fatal error! open file[%s] fail! errno(%d)", strFilePath, errno);
        }
    }

    ret = AW_MPI_VENC_ReleaseStream(pCap->mChn, &pCap->mOutStream);
    if(ret != SUCCESS)
    {
        aloge("fatal error! why release stream fail(0x%x)?", ret);
        result = FAILURE;
    }

    return result;
}

static int stopJpegPicture(TakePicture_Cap_S *pCap)
{
    int result = SUCCESS;

    AW_MPI_VENC_StopRecvPic(pCap->mChn);
    //can't reset channel now, because in non-tunnel mode, mpi_venc will force release out frames, but we don't want
    //those out frames to be released before we return them.
    //AW_MPI_VENC_ResetChn(mChn); 
    AW_MPI_VENC_DestroyChn(pCap->mChn);
    pCap->mChn = MM_INVALID_CHN;

    alogd("stop jpeg take picture");

    return result;
}

static void *GetCSIFrameThread(void *pArg)
{
    VI_DEV ViDev;
    VI_CHN ViCh;
    int ret = 0;
    int frm_cnt = 0;
    char isRunFlag = 1;

    // take pictures
    uint64_t mContinuousPictureStartTime = 0;   //unit:ms, start continuousPicture time.
    uint64_t mContinuousPictureLast = 0;    //unit:ms, next picture time.
    uint64_t mContinuousPictureInterval = 0;   //unit:ms
    int mJpegTestCnt = 0;
    int mContinuousPictureCnt = 0;

    TakePicture_Cap_S *pCap = (TakePicture_Cap_S *)pArg;
    SampleTakePictureContext *pContext = (SampleTakePictureContext*)pCap->mpContext;
    ViDev = pCap->Dev;
    ViCh = pCap->Chn;
    alogd("Cap threadid=0x%lx, ViDev = %d, ViCh = %d", pCap->thid, ViDev, ViCh);

    ret = startJpegPicture(pCap);
    if (0 != ret)
    {
        aloge("fatal error, startJpegPicture failed!");
        return NULL;
    }

    while (isRunFlag)
    {
        // vi get frame
        if ((ret = AW_MPI_VI_GetFrame(ViDev, ViCh, &pCap->pstFrameInfo, pCap->s32MilliSec)) != 0)
        {
            // printf("VI Get Frame failed!\n");
            continue ;
        }

        frm_cnt++;
        if (frm_cnt % 60 == 0)
        {
            time_t now;
            struct tm *timenow;
            time(&now);
            timenow = localtime(&now);
            alogd("Cap threadid=0x%lx, ViDev=%d,VirVi=%d,mpts=%lld; local time is %s", pCap->thid, ViDev, ViCh, pCap->pstFrameInfo.VFrame.mpts,asctime(timenow));
        }

        // take pictures
        if (TAKE_PICTURE_MODE_FAST == pContext->mTakePicMode)
        {
            if (mJpegTestCnt >= pContext->mConfigPara.mTakePictureNum)
            {
                alogd("one shot mode test end, take %d pictures", pContext->mConfigPara.mTakePictureNum);
                isRunFlag = 0;
            }
            bool bPermit = false;
            if (0 == mContinuousPictureStartTime)
            {
                mContinuousPictureStartTime = CDX_GetSysTimeUsMonotonic()/1000;
                mContinuousPictureLast = mContinuousPictureStartTime;
                mContinuousPictureInterval = pContext->mConfigPara.mTakePictureInterval;
                bPermit = true;
            }
            else
            {
                if(mContinuousPictureInterval <= 0)
                {
                    bPermit = true;
                }
                else
                {
                    uint64_t nCurTime = CDX_GetSysTimeUsMonotonic()/1000;
                    if(nCurTime >= mContinuousPictureLast + mContinuousPictureInterval)
                    {
                        alogd("capture picture, curTm[%lld]ms, [%lld][%lld]", nCurTime, mContinuousPictureLast, mContinuousPictureInterval);
                        bPermit = true;
                    }
                }
            }
            if(bPermit)
            {
                saveJpegPicture(pCap, &pCap->pstFrameInfo, mJpegTestCnt, 0);
                mJpegTestCnt++;
                mContinuousPictureLast += mContinuousPictureInterval;
            }
        }
        else if (TAKE_PICTURE_MODE_CONTINUOUS == pContext->mTakePicMode)
        {
            bool bPermit = false;
            if (0 == mContinuousPictureStartTime)
            {
                mContinuousPictureStartTime = CDX_GetSysTimeUsMonotonic()/1000;
                mContinuousPictureLast = mContinuousPictureStartTime;
                mContinuousPictureInterval = pContext->mConfigPara.mTakePictureInterval;
                mContinuousPictureCnt = 0;
                bPermit = true;
                alogd("start continous picture, will take [%d]pics, interval[%llu]ms, curTm[%lld]ms",
                    pContext->mConfigPara.mTakePictureNum, mContinuousPictureInterval, mContinuousPictureLast);
            }
            else
            {
                if(mContinuousPictureInterval <= 0)
                {
                    bPermit = true;
                }
                else
                {
                    uint64_t nCurTime = CDX_GetSysTimeUsMonotonic()/1000;
                    if(nCurTime >= mContinuousPictureLast + mContinuousPictureInterval)
                    {
                        alogd("capture picture, curTm[%lld]ms, [%lld][%lld]", nCurTime, mContinuousPictureLast, mContinuousPictureInterval);
                        bPermit = true;
                    }
                }
            }
            if(bPermit)
            {
                saveJpegPicture(pCap, &pCap->pstFrameInfo, 0, mContinuousPictureCnt);
                mContinuousPictureCnt++;
                if (mContinuousPictureCnt >= pContext->mConfigPara.mTakePictureNum)
                {
                    alogd("continuous shooting mode test end, take %d pictures", pContext->mConfigPara.mTakePictureNum);
                    isRunFlag = 0;
                }
                else
                {
                    mContinuousPictureLast = mContinuousPictureStartTime + mContinuousPictureCnt * mContinuousPictureInterval;
                }
            }
        }
        else
        {
            aloge("fatal error! any other take picture mode[0x%x]?", pContext->mTakePicMode);
        }
        
        // vi release frame
        AW_MPI_VI_ReleaseFrame(ViDev, ViCh, &pCap->pstFrameInfo);
    }

    stopJpegPicture(pCap);

    return NULL;
}

static int initVirVi_Cap_S(TakePicture_Cap_S *pThiz)
{
    memset(pThiz, 0, sizeof(TakePicture_Cap_S));
    cdx_sem_init(&pThiz->mSemFrameBack, 0);
    VENC_PACK_S *pEncPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S));
    memset(pEncPack, 0, sizeof(VENC_PACK_S));
    pThiz->mOutStream.mpPack = pEncPack;
    pThiz->mOutStream.mPackCount = 1;
    return 0;
}

static int destroyVirVi_Cap_S(TakePicture_Cap_S *pThiz)
{
    free(pThiz->mOutStream.mpPack);
    pThiz->mOutStream.mpPack = NULL;
    pThiz->mOutStream.mPackCount = 0;
    cdx_sem_deinit(&pThiz->mSemFrameBack);
    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0,result = 0;
    int vipp_dev, virvi_chn;

    printf("Sample takePicture buile time = %s, %s.\r\n", __DATE__, __TIME__);
    SampleTakePictureContext *pContext = (SampleTakePictureContext*)malloc(sizeof(SampleTakePictureContext));
    if (NULL == pContext)
    {
        aloge("fatal error, malloc failed! size %d", sizeof(SampleTakePictureContext));
        result = -1;
        goto _exit;
    }
    memset(pContext, 0, sizeof(SampleTakePictureContext));

    char *pConfigFilePath;
    if (argc > 1)
    {
        if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
        {
            aloge("fatal error! command line param is wrong, exit!");
            result = -1;
            goto _exit;
        }

        if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
        {
            pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
        }
        else
        {
            pConfigFilePath = DEFAULT_SAMPLE_VIRVI_CONF_PATH;
        }
    }
    else
    {
        pConfigFilePath = NULL;
    }
    /* parse config file. */
    if(loadConfigPara(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    vipp_dev = pContext->mConfigPara.DevNum;

    printf("======================================.\n");
    system("cat /proc/meminfo | grep Committed_AS");
    printf("======================================.\n");
    /* start mpp systerm */
    MPP_SYS_CONF_S mSysConf;
    memset(&mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&mSysConf);
    ret = AW_MPI_SYS_Init();
    if (ret < 0)
    {
        aloge("sys Init failed!");
        result = -1;
        goto _exit;
    }

    VI_ATTR_S stAttr;
    /*Set VI Channel Attribute*/
    memset(&stAttr, 0, sizeof(VI_ATTR_S));
    stAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    stAttr.memtype = V4L2_MEMORY_MMAP;
    stAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.SrcFormat);
    stAttr.format.field = V4L2_FIELD_NONE;
    stAttr.format.colorspace = pContext->mConfigPara.mColorSpace;
    stAttr.format.width = pContext->mConfigPara.SrcWidth;
    stAttr.format.height = pContext->mConfigPara.SrcHeight;
    stAttr.nbufs = 5;
    stAttr.nplanes = 2;
    stAttr.fps = pContext->mConfigPara.FrameRate;
    /* update configuration anyway, do not use current configuration */
    stAttr.use_current_win = 0;
    stAttr.wdr_mode = 0;
    stAttr.capturemode = V4L2_MODE_VIDEO; /* V4L2_MODE_VIDEO; V4L2_MODE_IMAGE; V4L2_MODE_PREVIEW */
    stAttr.drop_frame_num = pContext->mConfigPara.mViDropFrmCnt; // drop 2 second video data, default=0
    stAttr.mbEncppEnable = TRUE;
    AW_MPI_VI_CreateVipp(vipp_dev);
    AW_MPI_VI_SetVippAttr(vipp_dev, &stAttr);
#if ISP_RUN
    int iIspDev = 0;
    AW_MPI_ISP_Run(iIspDev);
#endif
    AW_MPI_VI_EnableVipp(vipp_dev);

    for (virvi_chn = 0; virvi_chn < 1; virvi_chn++)
    {
        initVirVi_Cap_S(&pContext->privCap[vipp_dev][virvi_chn]);
        pContext->privCap[vipp_dev][virvi_chn].Dev = pContext->mConfigPara.DevNum;
        pContext->privCap[vipp_dev][virvi_chn].Chn = virvi_chn;
        pContext->privCap[vipp_dev][virvi_chn].s32MilliSec = 5000;  // 2000;
        pContext->privCap[vipp_dev][virvi_chn].mpContext = (void*)pContext;

        ret = hal_virvi_start(vipp_dev, virvi_chn, NULL); /* For H264 */
        if(ret != 0)
        {
            aloge("virvi start failed!");
            result = -1;
            goto _exit;
        }
        pContext->privCap[vipp_dev][virvi_chn].thid = 0;
        ret = pthread_create(&pContext->privCap[vipp_dev][virvi_chn].thid, NULL, GetCSIFrameThread, (void *)&pContext->privCap[vipp_dev][virvi_chn]);
        if (ret < 0)
        {
            alogd("pthread_create failed, Dev[%d], Chn[%d].\n", pContext->privCap[vipp_dev][virvi_chn].Dev, pContext->privCap[vipp_dev][virvi_chn].Chn);
            continue;
        }
        else
        {
            alogd("vipp[%d]virChn[%d]: get csi frame thread id=[0x%lx]", vipp_dev, virvi_chn, pContext->privCap[vipp_dev][virvi_chn].thid);
        }
    }

    for (virvi_chn = 0; virvi_chn < 1; virvi_chn++)
    {
        pthread_join(pContext->privCap[vipp_dev][virvi_chn].thid, NULL);
    }
    for (virvi_chn = 0; virvi_chn < 1; virvi_chn++)
    {
        ret = hal_virvi_end(vipp_dev, virvi_chn);
        if(ret != 0)
        {
            aloge("virvi end failed!");
            result = -1;
            goto _exit;
        }
    }
    for (virvi_chn = 0; virvi_chn < 1; virvi_chn++)
    {
        destroyVirVi_Cap_S(&pContext->privCap[vipp_dev][virvi_chn]);
    }

    AW_MPI_VI_DisableVipp(vipp_dev);
#if ISP_RUN
    AW_MPI_ISP_Stop(iIspDev);
#endif
    AW_MPI_VI_DestroyVipp(vipp_dev);
    /* exit mpp systerm */
    ret = AW_MPI_SYS_Exit();
    if (ret < 0)
    {
        aloge("sys exit failed!");
        result = -1;
    }

    if (pContext)
    {
        free(pContext);
    }

_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
