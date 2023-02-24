/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_virvi.c
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
#include <mpi_videoformat_conversion.h>
#include <confparser.h>
#include <cdx_list.h>
#include "sample_virvi.h"
#include "sample_virvi_config.h"

#define ISP_RUN 1
#define DEFAULT_SAVE_PIC_DIR    "/tmp"

SampleVirViContext *gpSampleVirViContext = NULL;

static void handle_exit()
{
    alogd("user want to exit!");
    if(NULL != gpSampleVirViContext)
    {
        cdx_sem_up(&gpSampleVirViContext->mSemExit);
    }
}

static int ParseCmdLine(int argc, char **argv, SampleVirViCmdLineParam *pCmdLinePara)
{
    alogd("sample virvi path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleVirViCmdLineParam));
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
                "\t-path /home/sample_virvi.conf\n");
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

static char *parserKeyCfg(char *pKeyCfg, int nKey)
{
    static char keyCfg[MAX_FILE_PATH_SIZE] = {0};

    if (pKeyCfg)
    {
        sprintf(keyCfg, "%s_%d", pKeyCfg, nKey);
        return keyCfg;
    }

    aloge("fatal error! key cfg is null key num[%d]", nKey);
    return NULL;
}

static void judgeCaptureFormat(char *pFormatConf, PIXEL_FORMAT_E *pCapFromat)
{
    if (NULL != pFormatConf)
    {
        if (!strcmp(pFormatConf, "nv21"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if (!strcmp(pFormatConf, "yv12"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
        }
        else if (!strcmp(pFormatConf, "nv12"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if (!strcmp(pFormatConf, "yu12"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
        }
        /* aw compression format */
        else if (!strcmp(pFormatConf, "aw_afbc"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
        }
        else if (!strcmp(pFormatConf, "aw_lbc_2_0x"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
        }
        else if (!strcmp(pFormatConf, "aw_lbc_2_5x"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        }
        else if (!strcmp(pFormatConf, "aw_lbc_1_5x"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
        }
        else if (!strcmp(pFormatConf, "aw_lbc_1_0x"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
        }
        else if (!strcmp(pFormatConf, "srggb10"))
        {
            *pCapFromat = MM_PIXEL_FORMAT_RAW_SRGGB10;
        }
        /* aw_package_422 NOT support */
        // else if (!strcmp(ptr, "aw_package_422"))
        // {
        //     *pCapFromat = MM_PIXEL_FORMAT_YVYU_AW_PACKAGE_422;
        // }
        else
        {
            *pCapFromat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            aloge("fatal error! wrong src pixfmt:%s", pFormatConf);
            alogw("use the default pixfmt %d", *pCapFromat);
        }
    }
}

void judgeCaptureColorspace(char *pColorSpConf, enum v4l2_colorspace *pV4L2ColorSp)
{
    if (NULL != pColorSpConf)
    {
        if (!strcmp(pColorSpConf, "jpeg"))
        {
            *pV4L2ColorSp = V4L2_COLORSPACE_JPEG;
        }
        else if (!strcmp(pColorSpConf, "rec709"))
        {
            *pV4L2ColorSp = V4L2_COLORSPACE_REC709;
        }
        else if (!strcmp(pColorSpConf, "rec709_part_range"))
        {
            *pV4L2ColorSp = V4L2_COLORSPACE_REC709_PART_RANGE;
        }
        else
        {
            aloge("fatal error! wrong color space:%s, use dafault jpeg", pColorSpConf);
            *pV4L2ColorSp = V4L2_COLORSPACE_JPEG;
        }
    }
}

static ERRORTYPE loadConfigPara(SampleVirViContext *pContext, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;
    SampleVirViConfig *pConfig = NULL;
    SampleVirviSaveBufMgrConfig *pSaveBufMgrConfig = &pContext->mSaveBufMgrConfig;

    //default onlu use vipp0
    pConfig = &pContext->mCaps[0].mConfig;
    memset(pConfig, 0, sizeof(SampleVirViConfig));
    pConfig->DevNum = 0;
    pConfig->mIspDevNum = 0;
    pConfig->FrameRate = 20;
    pConfig->PicWidth = 1920;
    pConfig->PicHeight = 1080;
    pConfig->PicFormat = V4L2_PIX_FMT_NV21M;
    pConfig->mColorSpace = V4L2_COLORSPACE_JPEG;
    pConfig->mViDropFrmCnt = 0;

    pConfig = &pContext->mCaps[1].mConfig;
    memset(pConfig, 0, sizeof(SampleVirViConfig));
    pConfig->DevNum = MM_INVALID_DEV;
    pConfig->mIspDevNum = MM_INVALID_DEV;

    pConfig = &pContext->mCaps[2].mConfig;
    memset(pConfig, 0, sizeof(SampleVirViConfig));
    pConfig->DevNum = MM_INVALID_DEV;
    pConfig->mIspDevNum = MM_INVALID_DEV;

    pConfig = &pContext->mCaps[3].mConfig;
    memset(pConfig, 0, sizeof(SampleVirViConfig));
    pConfig->DevNum = MM_INVALID_DEV;
    pConfig->mIspDevNum = MM_INVALID_DEV;

    memset(pSaveBufMgrConfig, 0, sizeof(SampleVirviSaveBufMgrConfig));
    pSaveBufMgrConfig->mSavePicDev = 0;
    pSaveBufMgrConfig->mYuvFrameCount = 5;
    sprintf(pSaveBufMgrConfig->mYuvFile, "%s/test.yuv", DEFAULT_SAVE_PIC_DIR);
    pSaveBufMgrConfig->mRawStoreCount = 5;
    pSaveBufMgrConfig->mRawStoreInterval = 20;
    sprintf(pSaveBufMgrConfig->mStoreDirectory, "%s", DEFAULT_SAVE_PIC_DIR);
    pSaveBufMgrConfig->mSavePicBufferLen = 1920*1080*3/2;
    pSaveBufMgrConfig->mSavePicBufferNum = 5;

    if(conf_path != NULL)
    {
        CONFPARSER_S stConfParser;
        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }

        for (int i = 0; i < MAX_CAPTURE_NUM; i++)
        {
            pConfig = &pContext->mCaps[i].mConfig;

            pConfig->AutoTestCount = GetConfParaInt(&stConfParser, parserKeyCfg(SAMPLE_VirVi_Auto_Test_Count, i), 0);
            pConfig->GetFrameCount = GetConfParaInt(&stConfParser, parserKeyCfg(SAMPLE_VirVi_Get_Frame_Count, i), 0);
            pConfig->DevNum = GetConfParaInt(&stConfParser, parserKeyCfg(SAMPLE_VirVi_Dev_Num, i), 0);
            pConfig->mIspDevNum = GetConfParaInt(&stConfParser, parserKeyCfg(SAMPLE_VirVi_Dev_Num, i), 0);
            pConfig->FrameRate = GetConfParaInt(&stConfParser, parserKeyCfg(SAMPLE_VirVi_Frame_Rate, i), 0);
            pConfig->PicWidth = GetConfParaInt(&stConfParser, parserKeyCfg(SAMPLE_VirVi_Pic_Width, i), 0);
            pConfig->PicHeight = GetConfParaInt(&stConfParser, parserKeyCfg(SAMPLE_VirVi_Pic_Height, i), 0);
            ptr = (char*)GetConfParaString(&stConfParser, parserKeyCfg(SAMPLE_VirVi_Pic_Format, i), NULL);
            judgeCaptureFormat(ptr, &pConfig->PicFormat);
            ptr = (char *)GetConfParaString(&stConfParser, parserKeyCfg(SAMPLE_VirVi_COLOR_SPACE, i), NULL);
            judgeCaptureColorspace(ptr, &pConfig->mColorSpace);
            //alogd("srcPixFmt=%d, ColorSpace=%d", pConfig->PicFormat, pConfig->mColorSpace);
            pConfig->mEnableWDRMode = GetConfParaInt(&stConfParser, parserKeyCfg(SAMPLE_VirVi_Enable_WDR, i), 0);
            pConfig->mViDropFrmCnt = GetConfParaInt(&stConfParser, parserKeyCfg(SAMPLE_VirVi_ViDropFrmCnt, i), 0);
            //alogd("ViDropFrameNum: %d", pConfig->mViDropFrmCnt);
         }

        pSaveBufMgrConfig->mSavePicDev = GetConfParaInt(&stConfParser, SAMPLE_VirVi_SavePicDev, 0);
        pSaveBufMgrConfig->mYuvFrameCount = GetConfParaInt(&stConfParser, SAMPLE_VirVi_YuvFrameCount, 0);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_VirVi_YuvFilePath, NULL);
        if (ptr)
            strcpy(pSaveBufMgrConfig->mYuvFile, ptr);
        pSaveBufMgrConfig->mRawStoreCount = GetConfParaInt(&stConfParser, SAMPLE_VirVi_RawStoreCount, 0);
        pSaveBufMgrConfig->mRawStoreInterval = GetConfParaInt(&stConfParser, SAMPLE_VirVi_RawStoreInterval, 0);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_VirVi_StoreDir, NULL);
        if (ptr)
            strcpy(pSaveBufMgrConfig->mStoreDirectory, ptr);

        pSaveBufMgrConfig->mSavePicBufferNum = GetConfParaInt(&stConfParser, SAMPLE_VirVi_SavePicBufferNum, 0);
        pSaveBufMgrConfig->mSavePicBufferLen = GetConfParaInt(&stConfParser, SAMPLE_VirVi_SavePicBufferLen, 0);
        if (0 == pSaveBufMgrConfig->mSavePicBufferLen)
        {
            pSaveBufMgrConfig->mSavePicBufferLen = pConfig->PicWidth*pConfig->PicHeight*3/2;
            alogd("user did not specify buf len. set a default value %d bytes", pSaveBufMgrConfig->mSavePicBufferLen);
        }

        pContext->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_VirVi_Test_Duration, 0);

        for(int i = 0; i < MAX_CAPTURE_NUM; i++)
        {
            pConfig = &pContext->mCaps[i].mConfig;
            alogd("capture[%d] dev num[%d], capture size[%dx%d] format[%d] framerate[%d] colorspace[%d] drop frm[%d]", \
                i, pConfig->DevNum, pConfig->PicWidth, pConfig->PicHeight, pConfig->PicFormat, \
                pConfig->FrameRate, pConfig->mColorSpace, pConfig->mViDropFrmCnt);
        }

        alogd("save pic dev[%d], yuv count[%d] file[%s], raw count[%d] interval[%d] save dir[%s], buf len[%d] num[%d]", \
            pSaveBufMgrConfig->mSavePicDev, pSaveBufMgrConfig->mYuvFrameCount, pSaveBufMgrConfig->mYuvFile, \
            pSaveBufMgrConfig->mRawStoreCount, pSaveBufMgrConfig->mRawStoreInterval, pSaveBufMgrConfig->mStoreDirectory, \
            pSaveBufMgrConfig->mSavePicBufferLen, pSaveBufMgrConfig->mSavePicBufferNum);

        alogd("test duration[%d]", pContext->mTestDuration);

        destroyConfParser(&stConfParser);
    }
    return SUCCESS;
}

static int saveRawData(SampleVirviSaveBufMgr *pSaveBufMgr, SampleVirviSaveBufNode *pNode)
{
    SampleVirviSaveBufMgrConfig *pConfig = &pSaveBufMgr->mConfig;
    //make file name, e.g., /mnt/extsd/pic[0].NV21M
    char strPixFmt[16] = {0};
    char fileType[16] = {0};
    strcpy(fileType, "yuv");
    switch(pNode->mFrmFmt)
    {
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        {
            strcpy(strPixFmt,"nv21");
            break;
        }
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        {
            strcpy(strPixFmt,"nv12");
            break;
        }
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
        {
            strcpy(strPixFmt,"yv12");
            break;
        }
        case MM_PIXEL_FORMAT_YUV_PLANAR_420:
        {
            strcpy(strPixFmt,"yu12");
            break;
        }
        case MM_PIXEL_FORMAT_YUV_AW_AFBC:
        {
            strcpy(strPixFmt, "afbc");
            break;
        }
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X:
        {
            strcpy(strPixFmt, "lbc1_0x");
            break;
        }
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X:
        {
            strcpy(strPixFmt, "lbc1_5x");
            break;
        }
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X:
        {
            strcpy(strPixFmt, "lbc2_0x");
            break;
        }
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X:
        {
            strcpy(strPixFmt, "lbc2_5x");
            break;
        }
        case MM_PIXEL_FORMAT_RAW_SRGGB10:
        {
            strcpy(strPixFmt, "srggb10");
            memset(fileType, 0, sizeof(fileType));
            strcpy(fileType, "raw");
            break;
        }
        default:
        {
            strcpy(strPixFmt,"unknown");
            memset(fileType, 0, sizeof(fileType));
            strcpy(fileType, "bin");
            break;
        }
    }
    char strFilePath[MAX_FILE_PATH_SIZE];
    snprintf(strFilePath, MAX_FILE_PATH_SIZE, "%s/%dx%d_%s_vipp%d_%d.%s", \
        pConfig->mStoreDirectory, pNode->mFrmSize.Width, pNode->mFrmSize.Height, strPixFmt, \
        pConfig->mSavePicDev, pNode->mFrmCnt, fileType);

    int nLen;
    FILE *fpPic = fopen(strFilePath, "wb");
    if(fpPic != NULL)
    {
        nLen = fwrite(pNode->mpDataVirAddr, 1, pNode->mFrmLen, fpPic);
        if(nLen != pNode->mFrmLen)
        {
            aloge("fatal error! fwrite fail, write len[%d], frm len[%d], virAddr[%p]", \
                nLen, pNode->mFrmLen, pNode->mpDataVirAddr);
        }
        alogd("virAddr[%p], length=[%d]", pNode->mpDataVirAddr, pNode->mFrmLen);
        fclose(fpPic);
        fpPic = NULL;
        alogd("store raw frame in file[%s]", strFilePath);
    }
    else
    {
        aloge("fatal error! open file[%s] fail!", strFilePath);
    }
    return 0;
}

static int copyVideoFrame(VIDEO_FRAME_INFO_S *pSrc, SampleVirviSaveBufNode *pNode)
{
    unsigned int nFrmsize[3] = {0,0,0};
    void *pTmp = pNode->mpDataVirAddr;
    unsigned int dataWidth  = pSrc->VFrame.mOffsetRight - pSrc->VFrame.mOffsetLeft;
    unsigned int dataHeight = pSrc->VFrame.mOffsetBottom - pSrc->VFrame.mOffsetTop;
    alogv("dataWidth:%d, dataHeight:%d", dataWidth, dataHeight);

    switch(pSrc->VFrame.mPixelFormat)
    {
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_AW_NV21M:
            nFrmsize[0] = dataWidth*dataHeight;
            nFrmsize[1] = dataWidth*dataHeight/2;
            nFrmsize[2] = 0;
            break;
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
        case MM_PIXEL_FORMAT_YUV_PLANAR_420:
            nFrmsize[0] = dataWidth*dataHeight;
            nFrmsize[1] = dataWidth*dataHeight/4;
            nFrmsize[2] = dataWidth*dataHeight/4;
            break;
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X:
            nFrmsize[0] = pSrc->VFrame.mStride[0];
            nFrmsize[1] = pSrc->VFrame.mStride[1];
            nFrmsize[2] = pSrc->VFrame.mStride[2];
            break;
        default:
            aloge("fatal error! not support pixel format[0x%x]", pSrc->VFrame.mPixelFormat);
            return -1;
    }

    if (pNode->mDataLen < (nFrmsize[0]+nFrmsize[1]+nFrmsize[2]))
    {
        aloge("fatal error! buffer len[%d] < frame len[%d]", \
            pNode->mDataLen, nFrmsize[0]+nFrmsize[1]+nFrmsize[2]);
        return -1;
    }

    pNode->mFrmSize.Width = dataHeight;
    pNode->mFrmSize.Height = dataWidth;
    pNode->mFrmFmt = pSrc->VFrame.mPixelFormat;

    for (int i = 0; i < 3; i++)
    {
        if (pSrc->VFrame.mpVirAddr[i])
        {
            memcpy(pTmp, pSrc->VFrame.mpVirAddr[i], nFrmsize[i]);
            pTmp += nFrmsize[i];
            pNode->mFrmLen += nFrmsize[i];
        }
    }

    return 0;
}

static void *SaveCsiFrameThrad(void *pThreadData)
{
    SampleVirViContext *pContext = (SampleVirViContext *)pThreadData;
    SampleVirviSaveBufMgr *pBufMgr = pContext->mpSaveBufMgr;
    SampleVirviSaveBufMgrConfig *pConfig = &pBufMgr->mConfig;
    int nRawStoreNum = 0;
    FILE *fp = NULL;

    fp = fopen(pConfig->mYuvFile, "wb");

    pBufMgr->mbTrdRunningFlag = TRUE;
    while (1)
    {
        if (pContext->mbSaveCsiTrdExitFlag)
        {
            break;
        }

        SampleVirviSaveBufNode *pEntry = \
            list_first_entry_or_null(&pBufMgr->mReadyList, SampleVirviSaveBufNode, mList);
        if (pEntry)
        {
            pthread_mutex_lock(&pBufMgr->mIdleListLock);
            if (pEntry->mFrmCnt <= pConfig->mYuvFrameCount)
            {
                if (fp)
                {
                    alogv("save frm[%d] len[%d] file[%s] success!", \
                        pEntry->mFrmCnt, pEntry->mFrmLen, pConfig->mYuvFile);
                    fwrite(pEntry->mpDataVirAddr, 1, pEntry->mFrmLen, fp);
                }
            }
            if (pConfig->mRawStoreCount > 0
                && nRawStoreNum < pConfig->mRawStoreCount
                && 0 == pEntry->mFrmCnt % pConfig->mRawStoreInterval)
            {
                saveRawData(pBufMgr, pEntry);
                nRawStoreNum++;
            }
            pEntry->mFrmLen = 0;
            pEntry->mFrmFmt = 0;
            memset(&pEntry->mFrmSize, 0, sizeof(SIZE_S));
            memset(pEntry->mpDataVirAddr, 0, pEntry->mDataLen);
            pthread_mutex_lock(&pBufMgr->mReadyListLock);
            list_move_tail(&pEntry->mList, &pBufMgr->mIdleList);
            pthread_mutex_unlock(&pBufMgr->mReadyListLock);
            pthread_mutex_unlock(&pBufMgr->mIdleListLock);
        }
        else
        {
            usleep(10*1000);
        }
    }
    if (fp)
    {
        fclose(fp);
        fp = NULL;
    }

    return (void *)NULL;
}

static void *GetCSIFrameThread(void *pThreadData)
{
    int ret = 0;
    int frm_cnt = 0;
    SampleVirviCap *pCap = (SampleVirviCap *)pThreadData;
    SampleVirViConfig *pConfig = &pCap->mConfig;
    SampleVirViContext *pContext = pCap->mpContext;
    SampleVirviSaveBufMgr *pBufMgr = pContext->mpSaveBufMgr;
    SampleVirviSaveBufMgrConfig *pBufMgrConfig = &pBufMgr->mConfig;

    alogd("loop Sample_virvi, Cap threadid=0x%lx, ViDev = %d, ViCh = %d", \
        pCap->thid, pCap->Dev, pCap->Chn);

    pCap->mbTrdRunning = TRUE;
    pCap->mRawStoreNum = 0;
    while (1)
    {
        if (pCap->mbExitFlag)
        {
            break;
        }

        // vi get frame
        if ((ret = AW_MPI_VI_GetFrame(pCap->Dev, pCap->Chn, &pCap->pstFrameInfo, pCap->s32MilliSec)) != 0)
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
            alogd("Cap threadid=0x%lx, ViDev=%d,VirVi=%d,mpts=%lld; local time is %s", \
                pCap->thid, pCap->Dev, pCap->Chn, pCap->pstFrameInfo.VFrame.mpts, asctime(timenow));
        }

        if (NULL != pBufMgr)
        {
            if (pBufMgrConfig->mSavePicDev == pCap->Dev)
            {
                SampleVirviSaveBufNode *pEntry = \
                    list_first_entry_or_null(&pBufMgr->mIdleList, SampleVirviSaveBufNode, mList);
                if (pEntry)
                {
                    pthread_mutex_lock(&pBufMgr->mIdleListLock);
                    ret = copyVideoFrame(&pCap->pstFrameInfo, pEntry);
                    if (!ret)
                    {
                        pEntry->mpCap = (void *)pCap;
                        pEntry->mFrmCnt = frm_cnt;
                        pthread_mutex_lock(&pBufMgr->mReadyListLock);
                        list_move_tail(&pEntry->mList, &pBufMgr->mReadyList);
                        pthread_mutex_unlock(&pBufMgr->mReadyListLock);
                    }
                    pthread_mutex_unlock(&pBufMgr->mIdleListLock);
                }
            }
        }

        // vi release frame
        AW_MPI_VI_ReleaseFrame(pCap->Dev, pCap->Chn, &pCap->pstFrameInfo);
    }

    alogd("vi dev[%d] get csi frame trd exit!", pCap->Dev);
    return NULL;
}

static SampleVirviSaveBufMgr *initSaveBufMgr(SampleVirViContext *pContext)
{
    SampleVirviSaveBufMgr *pSaveBufMgr = NULL;
    SampleVirviSaveBufMgrConfig *pConfig = NULL;

    pSaveBufMgr = malloc(sizeof(SampleVirviSaveBufMgr));
    if (NULL == pSaveBufMgr)
    {
        aloge("fatal error! save buffer mgr malloc fail!");
        return NULL;
    }
    memset(pSaveBufMgr, 0, sizeof(SampleVirviSaveBufMgr));
    pConfig = &pSaveBufMgr->mConfig;
    memset(pConfig, 0, sizeof(SampleVirviSaveBufMgrConfig));
    memcpy(pConfig, &pContext->mSaveBufMgrConfig, sizeof(SampleVirviSaveBufMgrConfig));

    INIT_LIST_HEAD(&pSaveBufMgr->mIdleList);
    INIT_LIST_HEAD(&pSaveBufMgr->mReadyList);
    pthread_mutex_init(&pSaveBufMgr->mIdleListLock, NULL);
    pthread_mutex_init(&pSaveBufMgr->mReadyListLock, NULL);

    for (int i = 0; i < pConfig->mSavePicBufferNum; i++)
    {
        SampleVirviSaveBufNode *pNode = malloc(sizeof(SampleVirviSaveBufNode));
        if (NULL == pNode)
        {
            aloge("fatal error! malloc save buf node fail!");
            return NULL;
        }
        memset(pNode, 0, sizeof(SampleVirviSaveBufNode));
        pNode->mId = i;
        pNode->mDataLen = pConfig->mSavePicBufferLen;
        AW_MPI_SYS_MmzAlloc_Cached(&pNode->mDataPhyAddr, &pNode->mpDataVirAddr, pNode->mDataLen);
        if ((0 == pNode->mDataPhyAddr) || (NULL == pNode->mpDataVirAddr))
        {
            aloge("fatal error! alloc buf[%d] fail!", i);
            return NULL;
        }
        memset(pNode->mpDataVirAddr, 0, pNode->mDataLen);
        alogd("node[%d] alloc data len[%d] phy addr[%d] vir addr[%p]", \
            i, pNode->mDataLen, pNode->mDataPhyAddr, pNode->mpDataVirAddr);
        pthread_mutex_lock(&pSaveBufMgr->mIdleListLock);
        list_add_tail(&pNode->mList, &pSaveBufMgr->mIdleList);
        pthread_mutex_unlock(&pSaveBufMgr->mIdleListLock);
    }

    return pSaveBufMgr;
}

static int deinitSaveBufMgr(SampleVirviSaveBufMgr *pSaveBufMgr)
{
    if (NULL == pSaveBufMgr)
    {
        alogw("why save buf mgr is null");
        return -1;
    }

    SampleVirviSaveBufNode *pEntry = NULL, *pTmp = NULL;

    pthread_mutex_lock(&pSaveBufMgr->mReadyListLock);
    if (list_empty(&pSaveBufMgr->mReadyList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mReadyList, mList)
        {
            list_move_tail(&pEntry->mList, &pSaveBufMgr->mIdleList);
        }
    }
    pthread_mutex_unlock(&pSaveBufMgr->mReadyListLock);

    pthread_mutex_lock(&pSaveBufMgr->mIdleListLock);
    list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mIdleList, mList)
    {
        if (NULL == pEntry)
            continue;

        if (0 != pEntry->mDataPhyAddr && NULL != pEntry->mpDataVirAddr)
        {
            alogd("node[%d] alloc data len[%d] phy addr[%d] vir addr[%p]", \
                pEntry->mId, pEntry->mDataLen, pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            AW_MPI_SYS_MmzFree(pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            pEntry->mDataPhyAddr = 0;
            pEntry->mpDataVirAddr = NULL;
        }
        list_del(&pEntry->mList);
        free(pEntry);
        pEntry = NULL;
    }
    pthread_mutex_unlock(&pSaveBufMgr->mIdleListLock);

    pthread_mutex_destroy(&pSaveBufMgr->mIdleListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mReadyListLock);

    return 0;
}

static void initCaps(SampleVirViContext *pContext)
{
    SampleVirviCap *pCap = NULL;
    SampleVirViConfig *pConfig = NULL;

    for (int i = 0; i < MAX_CAPTURE_NUM; i++)
    {
        pCap = &pContext->mCaps[i];
        pConfig = &pCap->mConfig;

        pCap->Dev = MM_INVALID_DEV;
        pCap->mIspDev = MM_INVALID_DEV;
        pCap->Chn = MM_INVALID_CHN;

        if ((pConfig->DevNum < 0) || \
            (0 == pConfig->PicWidth) || (0 == pConfig->PicHeight))
        {
            pCap->mbCapValid = FALSE;
        }
        else
        {
            pCap->mbCapValid = TRUE;
            pCap->mpContext = (void *)pContext;
        }
    }
}

static void configViAttr(VI_ATTR_S *pViAttr, SampleVirViConfig *pConfig)
{
    pViAttr->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pViAttr->memtype = V4L2_MEMORY_MMAP;
    pViAttr->format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pConfig->PicFormat);
    pViAttr->format.field = V4L2_FIELD_NONE;
    pViAttr->format.colorspace = pConfig->mColorSpace;
    pViAttr->format.width = pConfig->PicWidth;
    pViAttr->format.height = pConfig->PicHeight;
    pViAttr->nbufs = 3;
    pViAttr->nplanes = 2;
    pViAttr->fps = pConfig->FrameRate;
    pViAttr->use_current_win = 0;
    pViAttr->wdr_mode = pConfig->mEnableWDRMode;
    pViAttr->capturemode = V4L2_MODE_VIDEO; /* V4L2_MODE_VIDEO; V4L2_MODE_IMAGE; V4L2_MODE_PREVIEW */
    pViAttr->drop_frame_num = pConfig->mViDropFrmCnt; // drop 2 second video data, default=0
}

static int prepare(SampleVirviCap *pCap)
{
    ERRORTYPE eRet = SUCCESS;
    SampleVirViConfig *pConfig = &pCap->mConfig;

    if (!pCap->mbCapValid)
    {
        return 0;
    }

    VI_ATTR_S stVIAttr;
    memset(&stVIAttr, 0, sizeof(VI_ATTR_S));
    configViAttr(&stVIAttr, &pCap->mConfig);
    pCap->Dev = pConfig->DevNum;
    eRet = AW_MPI_VI_CreateVipp(pCap->Dev);
    if (eRet)
    {
        pCap->Dev = MM_INVALID_DEV;
        aloge("fatal error! vi dev[%d] create fail!", pCap->Dev);
        return -1;
    }

    eRet = AW_MPI_VI_SetVippAttr(pCap->Dev, &stVIAttr);
    if (eRet)
    {
        aloge("fatal error! vi dev[%d] set vipp attr fail!", pCap->Dev);
    }

    pCap->mIspDev = pConfig->mIspDevNum;
    if (pCap->mIspDev >= 0)
    {
        eRet = AW_MPI_ISP_Run(pCap->mIspDev);
        if (eRet)
        {
            pCap->mIspDev = MM_INVALID_DEV;
            aloge("fatal error! isp[%d] start fail!", pCap->mIspDev);
        }
    }

    eRet = AW_MPI_VI_EnableVipp(pCap->Dev);
    if (eRet)
    {
        aloge("fatal error! vi dev[%d] enable fail!", pCap->Dev);
    }

    pCap->Chn = 0;
    eRet = AW_MPI_VI_CreateVirChn(pCap->Dev, pCap->Chn, NULL);
    if (eRet)
    {
        pCap->Chn = MM_INVALID_DEV;
        aloge("fatal error! vi dev[%d] create vi chn[0] fail!", pCap->Dev);
        return -1;
    }

    alogd("create vi dev[%d] vi chn[%d] success!", pCap->Dev, pCap->Chn);
    return 0;
}

static int start(SampleVirviCap *pCap)
{
    ERRORTYPE eRet = SUCCESS;

    if (pCap->Dev < 0 || pCap->Chn < 0)
    {
        return 0;
    }

    eRet = AW_MPI_VI_EnableVirChn(pCap->Dev, pCap->Chn);
    if (eRet)
    {
        aloge("fatal error! vi dev[%d] enable virvi chn[%d] fail!", pCap->Dev, pCap->Chn);
        return -1;
    }

    pthread_create(&pCap->thid, NULL, GetCSIFrameThread, (void *)pCap);

    return 0;
}

static int stop(SampleVirviCap *pCap)
{
    if (pCap->mbTrdRunning)
    {
        pCap->mbExitFlag = TRUE;
        pthread_join(pCap->thid, NULL);
    }
    if (pCap->Chn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pCap->Dev, pCap->Chn);
        AW_MPI_VI_DestroyVirChn(pCap->Dev, pCap->Chn);
    }
    if (pCap->Dev >= 0)
    {
        AW_MPI_VI_DisableVipp(pCap->Dev);
        if (pCap->mIspDev >= 0)
            AW_MPI_ISP_Stop(pCap->mIspDev);
        AW_MPI_VI_DestroyVipp(pCap->Dev);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0,result = 0;
    int vipp_dev, virvi_chn;
    int count = 0;
    pthread_t saveFrmTrd;

    printf("Sample virvi buile time = %s, %s.\r\n", __DATE__, __TIME__);
    SampleVirViContext *pContext = (SampleVirViContext*)malloc(sizeof(SampleVirViContext));
    memset(pContext, 0, sizeof(SampleVirViContext));
    gpSampleVirViContext = pContext;
    SampleVirviCap *pCap = NULL;

    cdx_sem_init(&pContext->mSemExit, 0);
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }

    char *pConfigFilePath;
    if (argc > 1)
    {
        /* parse command line param,read sample_virvi.conf */
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
    if(loadConfigPara(pContext, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    initCaps(pContext);

    MPP_SYS_CONF_S stConf;
    memset(&stConf, 0, sizeof(MPP_SYS_CONF_S));
    AW_MPI_SYS_SetConf(&stConf);
    AW_MPI_SYS_Init();

    if (pContext->mSaveBufMgrConfig.mSavePicDev >= 0 && \
        pContext->mSaveBufMgrConfig.mSavePicBufferLen >= 0 && \
        pContext->mSaveBufMgrConfig.mSavePicBufferNum >= 0)
    {
        pContext->mpSaveBufMgr = initSaveBufMgr(pContext);
        if (NULL == pContext->mpSaveBufMgr)
        {
            aloge("fatal error! init save csi frame mgr fail!");
            goto _deinit_buf_mgr;
        }
        pthread_create(&saveFrmTrd, NULL, SaveCsiFrameThrad, (void *)pContext);
    }

    for (int i = 0; i < MAX_CAPTURE_NUM; i++)
    {
        pCap = &pContext->mCaps[i];
        ret = prepare(pCap);
        if (ret)
        {
            aloge("fatal error! capture[%d] prepare fail!", i);
        }
    }

    for (int i = 0; i < MAX_CAPTURE_NUM; i++)
    {
        pCap = &pContext->mCaps[i];
        ret = start(pCap);
        if (ret)
        {
            aloge("fatal error! capture[%d] start fail!", i);
        }
    }

    if (pContext->mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mTestDuration*1000);
    }
    else
    {
        cdx_sem_wait(&pContext->mSemExit);
    }

    for (int i = 0; i < MAX_CAPTURE_NUM; i++)
    {
        pCap = &pContext->mCaps[i];
        ret = stop(pCap);
        if (ret)
        {
            aloge("fatal error! capture[%d] stop fail!", i);
        }
    }

_deinit_buf_mgr:
    if (pContext->mpSaveBufMgr)
    {
        pContext->mbSaveCsiTrdExitFlag = TRUE;
        if (pContext->mpSaveBufMgr->mbTrdRunningFlag)
            pthread_join(saveFrmTrd, NULL);
        deinitSaveBufMgr(pContext->mpSaveBufMgr);
    }

    AW_MPI_SYS_Exit();

    cdx_sem_deinit(&pContext->mSemExit);
    free(pContext);
_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
