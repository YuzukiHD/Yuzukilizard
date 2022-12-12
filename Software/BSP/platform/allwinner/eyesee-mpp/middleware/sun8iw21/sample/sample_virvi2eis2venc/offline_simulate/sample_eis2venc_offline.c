/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_virvi2venc.c
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2018/10/23
  Last Modified :
  Description   : EIS mpp component sample implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "sample_virvi2eis2venc"
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
#include <stdbool.h>
#include <signal.h>
#include <string.h>

#include "media/mpi_sys.h"
#include "mpi_eis.h"
#include "mpi_vi.h"
#include "mpi_isp.h"
#include "mpi_venc.h"
#include "mpi_videoformat_conversion.h"
#include "mm_common.h"
#include <cdx_list.h>

#include <confparser.h>
#include "sample_eis2venc_offline.h"
//#include "sample_virvi2eis2venc_config.h"

static bool g_bExitFlag = 0;
static bool g_bBreakProgress = 0;
static int g_iGetFrameCnt = 0;
static int g_iProcessOneFrmSuccess = 0;

void SampleExitHandle(int iSig)
{
    aloge("Get one exit signal.");
    g_bExitFlag = 1;
    g_bBreakProgress = 1;
    g_iGetFrameCnt = 0xFFFFFFF;
}

typedef struct FrameNode
{
    VIDEO_FRAME_INFO_S mFrame;
    struct list_head mList;
} FrameNode;

static VIDEO_FRAME_INFO_S* FrameManagerPrefetchIdleFrame(void *pThiz)
{
    FrameManager *pFrameManager = (FrameManager *)pThiz;
    FrameNode *pFirstNode;
    VIDEO_FRAME_INFO_S *pFrameInfo;

    pthread_mutex_lock(&pFrameManager->mLock);
    if(!list_empty(&pFrameManager->mIdleList)) {
        pFirstNode = list_first_entry(&pFrameManager->mIdleList, FrameNode, mList);
        pFrameInfo = &pFirstNode->mFrame;
    } else {
        pFrameInfo = NULL;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);

    return pFrameInfo;
}

static int FrameManagerUseFrame(void *pThiz, VIDEO_FRAME_INFO_S *pFrame)
{
    int ret = 0;
    FrameManager *pFrameManager = (FrameManager*)pThiz;

    if(NULL == pFrame) {
        aloge("fatal error! pNode == NULL!");
        return -1;
    }
    pthread_mutex_lock(&pFrameManager->mLock);
    FrameNode *pFirstNode = list_first_entry_or_null(&pFrameManager->mIdleList, FrameNode, mList);
    if(pFirstNode) {
        if(&pFirstNode->mFrame == pFrame)
            list_move_tail(&pFirstNode->mList, &pFrameManager->mUsingList);
        else {
            aloge("fatal error! node is not match [%p]!=[%p]", pFrame, &pFirstNode->mFrame);
            ret = -1;
        }
    } else {
        aloge("fatal error! idle list is empty");
        ret = -1;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);

    return ret;
}

static int FrameManagerReleaseFrame(void *pThiz, unsigned int nFrameId)
{
    int ret = 0;
    FrameManager *pFrameManager = (FrameManager*)pThiz;
    pthread_mutex_lock(&pFrameManager->mLock);
    int bFindFlag = 0;
    FrameNode *pEntry, *pTmp;

    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mUsingList, mList)
    {
        if(pEntry->mFrame.mId == nFrameId) {
            list_move_tail(&pEntry->mList, &pFrameManager->mIdleList);
            bFindFlag = 1;
            break;
        }
    }

    if(0 == bFindFlag) {
        aloge("fatal error! frameId[%d] is not find", nFrameId);
        ret = -1;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);

    return ret;
}

static int InitFrameManager(FrameManager *pFrameManager, int nFrameNum, SampleEis2VencConfig *pConfigPara)
{
    memset(pFrameManager, 0, sizeof(FrameManager));
    pthread_mutex_init(&pFrameManager->mLock, NULL);

    INIT_LIST_HEAD(&pFrameManager->mIdleList);
    INIT_LIST_HEAD(&pFrameManager->mUsingList);
    unsigned int nFrameSize = pConfigPara->stUsrCfg.iCapVideoWidth*pConfigPara->stUsrCfg.iCapVideoHeight*3/2;

    int i = 0;;
    FrameNode *pNode;
    unsigned int uPhyAddr;
    void *pVirtAddr;

    for(i = 0; i < nFrameNum; i++) {
        pNode = (FrameNode*)malloc(sizeof(FrameNode));
        memset(pNode, 0, sizeof(FrameNode));
        AW_MPI_SYS_MmzAlloc_Cached(&uPhyAddr, &pVirtAddr, nFrameSize);
        pNode->mFrame.VFrame.mPhyAddr[0] = uPhyAddr;
        pNode->mFrame.VFrame.mPhyAddr[1] = uPhyAddr + pConfigPara->stUsrCfg.iCapVideoWidth*pConfigPara->stUsrCfg.iCapVideoHeight;
        pNode->mFrame.VFrame.mpVirAddr[0] = pVirtAddr;
        pNode->mFrame.VFrame.mpVirAddr[1] = pVirtAddr + pConfigPara->stUsrCfg.iCapVideoWidth*pConfigPara->stUsrCfg.iCapVideoHeight;
        pNode->mFrame.VFrame.mWidth = pConfigPara->stUsrCfg.iCapVideoWidth;
        pNode->mFrame.VFrame.mHeight = pConfigPara->stUsrCfg.iCapVideoHeight;
        pNode->mFrame.VFrame.mField = VIDEO_FIELD_FRAME;
        pNode->mFrame.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pNode->mFrame.VFrame.mVideoFormat = VIDEO_FORMAT_LINEAR;
        pNode->mFrame.VFrame.mCompressMode = COMPRESS_MODE_NONE;
        pNode->mFrame.VFrame.mOffsetTop = 0;
        pNode->mFrame.VFrame.mOffsetBottom = pConfigPara->stUsrCfg.iCapVideoHeight;
        pNode->mFrame.VFrame.mOffsetLeft = 0;
        pNode->mFrame.VFrame.mOffsetRight = pConfigPara->stUsrCfg.iCapVideoWidth;
        pNode->mFrame.mId = i;
        list_add_tail(&pNode->mList, &pFrameManager->mIdleList);
    }
    pFrameManager->mNodeCnt = i;

    pFrameManager->PrefetchFirstIdleFrame = FrameManagerPrefetchIdleFrame;
    pFrameManager->UseFrame = FrameManagerUseFrame;
    pFrameManager->ReleaseFrame = FrameManagerReleaseFrame;
    return 0;
}

static void DestroyFrameManager(FrameManager *pFrameManager)
{
    pthread_mutex_lock(&pFrameManager->mLock);
    if(!list_empty(&pFrameManager->mUsingList))
        aloge("fatal error! why using list is not empty");

    FrameNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mIdleList, mList)
    {
        AW_MPI_SYS_MmzFree(pEntry->mFrame.VFrame.mPhyAddr[0], pEntry->mFrame.VFrame.mpVirAddr[0]);
        list_del(&pEntry->mList);
        free(pEntry);
    }

    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mUsingList, mList)
    {
        AW_MPI_SYS_MmzFree(pEntry->mFrame.VFrame.mPhyAddr[0], pEntry->mFrame.VFrame.mpVirAddr[0]);
        list_del(&pEntry->mList);
        free(pEntry);
    }

    pthread_mutex_unlock(&pFrameManager->mLock);
    pthread_mutex_destroy(&pFrameManager->mLock);
}

static void *SampleEisVencThread(void *pArgs)
{
    SampleEis2VencConfig* pViEisVencCfg = (SampleEis2VencConfig*)pArgs;

    int iVeChn = pViEisVencCfg->VencCfg.iVencChn;
    int iRet = 0;
    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPkt;

    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPkt;
    while (!g_bExitFlag && !g_bBreakProgress) {
        iRet = AW_MPI_VENC_GetStream(iVeChn, &stVencStream, 200);
        if (iRet != SUCCESS)
            continue;
#if 1
        if (stVencPkt.mLen0)
            fwrite(stVencPkt.mpAddr0, 1, stVencPkt.mLen0, pViEisVencCfg->VencCfg.pSaveFd);
        if (stVencPkt.mLen1)
            fwrite(stVencPkt.mpAddr1, 1, stVencPkt.mLen1, pViEisVencCfg->VencCfg.pSaveFd);
#endif
        AW_MPI_VENC_ReleaseStream(iVeChn, &stVencStream);
        g_iGetFrameCnt++;
    }

    return NULL;
}

static void *SampleEisSendDataThread(void *pArgs)
{
    SampleEis2VencConfig* pViEisVencCfg = (SampleEis2VencConfig*)pArgs;

    int iVeChn = pViEisVencCfg->VencCfg.iVencChn;
    int iEisChn = pViEisVencCfg->EisCfg.iEisChn;
    int iRet = 0;

    EIS_GYRO_PACKET_S stGyroPktTmp;
    char pCharStoreTmp[1024];
    char *pCharRetTmp = NULL;

    /* Read gyro datas to fill full the gyro ring buffer. */
    int i = 0;
    for (i = 0; i < 200; i++) { /* Read 400ms gyro datas */
        pCharRetTmp = fgets(pCharStoreTmp, sizeof(pCharStoreTmp), pViEisVencCfg->pGyroDataFd);
        if (NULL == pCharRetTmp) {
            aloge("Gyro file read empty.");
            break;
//            return NULL;
        }
        sscanf(pCharStoreTmp, "%llu %f %f %f %f %f %f", &stGyroPktTmp.dTimeStamp,
            &stGyroPktTmp.fAccelVX, &stGyroPktTmp.fAccelVY, &stGyroPktTmp.fAccelVZ,
            &stGyroPktTmp.fAnglrVX, &stGyroPktTmp.fAnglrVY, &stGyroPktTmp.fAnglrVZ);
//        aloge("=================================================%llu %f %f %f %f %f %f.", stGyroPktTmp.dTimeStamp,
//            stGyroPktTmp.fAccelVX, stGyroPktTmp.fAccelVY, stGyroPktTmp.fAccelVZ,
//            stGyroPktTmp.fAnglrVX, stGyroPktTmp.fAnglrVY, stGyroPktTmp.fAnglrVZ);
        AW_MPI_EIS_SendGyroPacket(iEisChn, &stGyroPktTmp, 1);
    }

    VIDEO_FRAME_INFO_S *pstFrmInfo;
    unsigned int uResolutionSize = pViEisVencCfg->stUsrCfg.iCapVideoWidth*pViEisVencCfg->stUsrCfg.iCapVideoHeight;

    while (!g_bExitFlag && !g_bBreakProgress) {
        for (i = 0; i < 1000/pViEisVencCfg->stUsrCfg.iVideoFps+4; i++) {
            pCharRetTmp = fgets(pCharStoreTmp, sizeof(pCharStoreTmp), pViEisVencCfg->pGyroDataFd);
            if (NULL == pCharRetTmp) {
                aloge("Gyro file read empty.");
                break;
            }
            sscanf(pCharStoreTmp, "%llu %f %f %f %f %f %f", &stGyroPktTmp.dTimeStamp,
                &stGyroPktTmp.fAccelVX, &stGyroPktTmp.fAccelVY, &stGyroPktTmp.fAccelVZ,
                &stGyroPktTmp.fAnglrVX, &stGyroPktTmp.fAnglrVY, &stGyroPktTmp.fAnglrVZ);
            AW_MPI_EIS_SendGyroPacket(iEisChn, &stGyroPktTmp, 1);
        }

        pstFrmInfo = FrameManagerPrefetchIdleFrame(&pViEisVencCfg->stFrmMgr);
        if (pstFrmInfo) {
            pCharRetTmp = fgets(pCharStoreTmp, sizeof(pCharStoreTmp), pViEisVencCfg->pTsDataFd);
            if (pCharRetTmp) {
                sscanf(pCharStoreTmp, "%llu %u",
                    &pstFrmInfo->VFrame.mpts, &pstFrmInfo->VFrame.mExposureTime);
            }

            fread(pstFrmInfo->VFrame.mpVirAddr[0], uResolutionSize, 1, pViEisVencCfg->pVideoDataFd);
            fread(pstFrmInfo->VFrame.mpVirAddr[1], uResolutionSize/2, 1, pViEisVencCfg->pVideoDataFd);
            FrameManagerUseFrame(&pViEisVencCfg->stFrmMgr, pstFrmInfo);
            AW_MPI_EIS_SendPic(iEisChn, pstFrmInfo, 200);
            g_iGetFrameCnt++;
        }
        if (feof(pViEisVencCfg->pVideoDataFd) || feof(pViEisVencCfg->pTsDataFd)) {
            aloge("Get end of video data or video's timestamp file, and will exit.");
            g_bExitFlag = 1;
            g_bBreakProgress = 1;
        }

        if (g_iGetFrameCnt > pViEisVencCfg->EisCfg.stEisAttr.iEisFilterWidth) {
            g_iProcessOneFrmSuccess = 0;
            while (!g_iProcessOneFrmSuccess) { usleep(1*1000); }
        }
    }

    return NULL;
}

ERRORTYPE SampleEisVencBufferNotify(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleEis2VencConfig* pViEisVencCfg = (SampleEis2VencConfig*)cookie;

    VIDEO_FRAME_INFO_S* pFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
    switch (event) {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER: {
            AW_MPI_EIS_ReleaseData(pViEisVencCfg->EisCfg.iEisChn, pFrameInfo);
            //aloge("Release frame[%d].", pFrameInfo->mId);
        } break;

        default: break;
    }

    return SUCCESS;
}

ERRORTYPE SampleEisCompBufferNotify(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleEis2VencConfig* pViEisVencCfg = (SampleEis2VencConfig*)cookie;

    VIDEO_FRAME_INFO_S* pFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
    switch (event) {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER: {
            FrameManagerReleaseFrame(&pViEisVencCfg->stFrmMgr, pFrameInfo->mId);
            g_iProcessOneFrmSuccess = 1;
            //aloge("Release frame[%d].", pFrameInfo->mId);
        } break;

        default: break;
    }

    return SUCCESS;
}

static inline int LoadSampleEisConfig(SampleViEisUsrCfg *pConfig, const char *conf_path)
{
    int iRet;

    CONFPARSER_S stConfParser;
    iRet = createConfParser(conf_path, &stConfParser);
    if(iRet < 0)
    {
        alogd("user not set config file. use default test parameter!");
        pConfig->iCapVideoWidth = 1920;
        pConfig->iCapVideoHeight= 1080;
        pConfig->iVideoFps = 30;
        pConfig->iInputBufNum = 11;
        pConfig->iOutBufNum   = 10;
        pConfig->iTimeDelayMs = 33;
        pConfig->iTimeSyncErrTolerance = 2;

        strncpy(pConfig->pSaveFilePath, SAVE_FILE_NAME, sizeof(pConfig->pSaveFilePath));
        strncpy(pConfig->pGyroDataPath, GYRO_FILE_NAME, sizeof(pConfig->pGyroDataPath));
        strncpy(pConfig->pVideoDataPath, VIDEO_FILE_NAME, sizeof(pConfig->pVideoDataPath));
        strncpy(pConfig->pTsDataPath, TIMESTAMP_FILE_NAME, sizeof(pConfig->pTsDataPath));
        goto use_default_conf;
    }

    if (pConfig->iCapVideoWidth == -1)
        pConfig->iCapVideoWidth  = GetConfParaInt(&stConfParser, SAMPLE_EIS_CAPWIDTH, 0);
    if (pConfig->iCapVideoHeight == -1)
        pConfig->iCapVideoHeight = GetConfParaInt(&stConfParser, SAMPLE_EIS_CAPHEIGH, 0);
    if (pConfig->iVideoFps == -1)
        pConfig->iVideoFps = GetConfParaInt(&stConfParser, SAMPLE_EIS_FPS, 0);
    if (pConfig->iInputBufNum == -1)
        pConfig->iInputBufNum = GetConfParaInt(&stConfParser, SAMPLE_EIS_INBUFCNT, 0);
    if (pConfig->iOutBufNum == -1)
        pConfig->iOutBufNum = GetConfParaInt(&stConfParser, SAMPLE_EIS_OUTBUFCNT, 0);
    if (pConfig->iTimeDelayMs == -1)
        pConfig->iTimeDelayMs = GetConfParaInt(&stConfParser, SAMPLE_EIS_TDELAY, 0);
    if (pConfig->iTimeSyncErrTolerance == -1)
        pConfig->iTimeSyncErrTolerance = GetConfParaInt(&stConfParser, SAMPLE_EIS_SYNCTOL, 0);

    char *pcTmpPtr;
    pcTmpPtr = (char *)GetConfParaString(&stConfParser, SAMPLE_EIS_SAVEFILE, NULL);
    if (NULL != pcTmpPtr && pConfig->pSaveFilePath[0] == 0xFF)
        strncpy(pConfig->pSaveFilePath, pcTmpPtr, sizeof(pConfig->pSaveFilePath));

    pcTmpPtr = (char *)GetConfParaString(&stConfParser, SAMPLE_EIS_GYROFILE, NULL);
    if (NULL != pcTmpPtr && pConfig->pGyroDataPath[0] == 0xFF)
        strncpy(pConfig->pGyroDataPath, pcTmpPtr, sizeof(pConfig->pGyroDataPath));

    pcTmpPtr = (char *)GetConfParaString(&stConfParser, SAMPLE_EIS_VIDEOFILE, NULL);
    if (NULL != pcTmpPtr && pConfig->pVideoDataPath[0] == 0xFF)
        strncpy(pConfig->pVideoDataPath, pcTmpPtr, sizeof(pConfig->pVideoDataPath));

    pcTmpPtr = (char *)GetConfParaString(&stConfParser, SAMPLE_EIS_TIMESTAMPFILE, NULL);
    if (NULL != pcTmpPtr && pConfig->pTsDataPath[0] == 0xFF)
        strncpy(pConfig->pTsDataPath, pcTmpPtr, sizeof(pConfig->pTsDataPath));

use_default_conf:
    printf("in_width=%d,in_height=%d,fps=%d,ibuf_num=%d,obuf_num=%d,delayms=%d,tolerance=%d.\r\n",
        pConfig->iCapVideoWidth, pConfig->iCapVideoHeight, pConfig->iVideoFps,
        pConfig->iInputBufNum, pConfig->iOutBufNum, pConfig->iTimeDelayMs, pConfig->iTimeSyncErrTolerance);
    printf("save_file=%s,gyro_path=%s,video_path=%s,timestamp=%s.\r\n",
        pConfig->pSaveFilePath, pConfig->pGyroDataPath, pConfig->pVideoDataPath, pConfig->pTsDataPath);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

static struct option pstLongOptions[] = {
   {"help",    no_argument,       0, 'h'},
   {"path",    required_argument, 0, 'p'},
   {"width",   required_argument, 0, 'x'},
   {"height",  required_argument, 0, 'y'},
   {"fps",     required_argument, 0, 'f'},
   {"input",   required_argument, 0, 'i'},
   {"output",  required_argument, 0, 'o'},
   {"delay",   required_argument, 0, 'd'},
   {"errtol",  required_argument, 0, 128},
   {"savefile",required_argument, 0, 129},
   {"gyrofile",required_argument, 0, 130},
   {"videofile",required_argument, 0, 131},
   {"tsfile",  required_argument, 0, 132},
   {0,         0,                 0, 0}
};

static char pcHelpInfo[] = 
"\033[33m"
"exec [-h|--help] [-p|--path]\n"
"   <-h|--help>: print to the help information\n"
"   <-p|--path>   <args>: point to the configuration file path\n"
"   <-x|--width>  <args>: set video picture width\n"
"   <-y|--height> <args>: set video picture height\n"
"   <-f|--fps>    <args>: set the video capture frequency\n"
"   <-i|--input>  <args>: how much input video frame number for EIS process\n"
"   <-o|--output> <args>: how much output video frame number for EIS output\n"
"   <-d|--delay>  <args>: how much times delay in gyro and video sync\n"
"   <--errtol>    <args>: how much times sync error can you endure\n"
"   <--savefile>  <args>: where do you want to save the processed video frames.\n"
"   <--gyrofile>  <args>: where should I get gyro datas.\n"
"   <--videofile> <args>: where should I get video datas.\n"
"   <--tsfile>    <args>: where should I get video's timestamp datas.\n"
"\033[0m\n";

static int ParseCmdLine(int argc, char **argv, SampleViEisUsrCfg *pCmdLinePara)
{
    int mRet;
    int iOptIndex = 0;

    memset(pCmdLinePara, -1, sizeof(SampleViEisUsrCfg));
    pCmdLinePara->pConfFilePath[0] = 0;

    while (1) {
        mRet = getopt_long(argc, argv, ":p:hx:y:f:i:o:d:", pstLongOptions, &iOptIndex);
        if (mRet == -1) {
            break;
        }

        switch (mRet) {
            case 'h':
                printf("%s", pcHelpInfo);
                goto print_help_exit;
                break;
            /* let the "sampleXXX -path sampleXXX.conf" command to be compatible with
             * "sampleXXX -p sampleXXX.conf"
             */
            case 'p':
                if (strcmp("ath", optarg) == 0) {
                    if (NULL == argv[optind]) {
                        printf("%s", pcHelpInfo);
                        goto opt_need_arg;
                    }
                    alogd("path is [%s]\n", argv[optind]);
                    strncpy(pCmdLinePara->pConfFilePath, argv[optind], sizeof(pCmdLinePara->pConfFilePath));
                } else {
                    alogd("path is [%s]\n", optarg);
                    strncpy(pCmdLinePara->pConfFilePath, optarg, sizeof(pCmdLinePara->pConfFilePath));
                }
                break;
            case 'x':
                pCmdLinePara->iCapVideoWidth  = atoi(optarg);
                break;
            case 'y':
                pCmdLinePara->iCapVideoHeight = atoi(optarg);
                break;
            case 'f':
                pCmdLinePara->iVideoFps = atoi(optarg);
                break;
            case 'i':
                pCmdLinePara->iInputBufNum = atoi(optarg);
                break;
            case 'o':
                pCmdLinePara->iOutBufNum = atoi(optarg);
                break;
            case 'd':
                pCmdLinePara->iTimeDelayMs = atoi(optarg);
                break;
            case 128: /* sync error tolerance: ms */
                pCmdLinePara->iTimeSyncErrTolerance = atoi(optarg);
                break;
            case 129: /* save file path */
                strncpy(pCmdLinePara->pSaveFilePath, optarg, sizeof(pCmdLinePara->pSaveFilePath));
                break;
            case 130: /* Input gyro data file path */
                strncpy(pCmdLinePara->pGyroDataPath, optarg, sizeof(pCmdLinePara->pGyroDataPath));
                break;
            case 131: /* Input video data file path */
                strncpy(pCmdLinePara->pVideoDataPath, optarg, sizeof(pCmdLinePara->pVideoDataPath));
                break;
            case 132: /* Input video's timestamp data file path */
                strncpy(pCmdLinePara->pTsDataPath, optarg, sizeof(pCmdLinePara->pTsDataPath));
                break;
            case ':':
                aloge("option \"%s\" need <arg>\n", argv[optind - 1]);
                goto opt_need_arg;
                break;
            case '?':
                if (optind > 2) {
                    break;
                }
                aloge("unknow option \"%s\"\n", argv[optind - 1]);
                printf("%s", pcHelpInfo);
                goto unknow_option;
                break;
            default:
                printf("?? why getopt_long returned character code 0%o ??\n", mRet);
                break;
        }
    }

    return 0;
opt_need_arg:
unknow_option:
    return -1;
print_help_exit:
    return 1;
}

int main(int argc, char *argv[])
{
    int iRet = 0;
    SampleEis2VencConfig EisVencCfg;
    memset(&EisVencCfg, 0, sizeof(SampleEis2VencConfig));

    iRet = ParseCmdLine(argc, argv, &EisVencCfg.stUsrCfg);
    if (iRet < 0)
        return iRet;
    else if (0 != iRet)
        return 0;

    /* Always be success */
    LoadSampleEisConfig(&EisVencCfg.stUsrCfg, EisVencCfg.stUsrCfg.pConfFilePath);

    int i = 0;
    MPP_SYS_CONF_S stSysConf;
    /* EIS create and enable */
    EisVencCfg.EisCfg.iEisChn = 0;
    EisVencCfg.EisCfg.stEisAttr.iGyroFreq     = 1000;
    EisVencCfg.EisCfg.stEisAttr.iGyroAxiNum   = 3;
    EisVencCfg.EisCfg.stEisAttr.iGyroPoolSize = 1000;
    EisVencCfg.EisCfg.stEisAttr.iInputBufNum  = EisVencCfg.stUsrCfg.iInputBufNum;
    EisVencCfg.EisCfg.stEisAttr.iEisFilterWidth = EisVencCfg.stUsrCfg.iInputBufNum;
    EisVencCfg.EisCfg.stEisAttr.iOutputBufBum = EisVencCfg.stUsrCfg.iOutBufNum;
    EisVencCfg.EisCfg.stEisAttr.iVideoInWidth = EisVencCfg.stUsrCfg.iCapVideoWidth;
    EisVencCfg.EisCfg.stEisAttr.iVideoInHeight = EisVencCfg.stUsrCfg.iCapVideoHeight;
    EisVencCfg.EisCfg.stEisAttr.iVideoInWidthStride  = EisVencCfg.stUsrCfg.iCapVideoWidth;
    EisVencCfg.EisCfg.stEisAttr.iVideoInHeightStride = EisVencCfg.stUsrCfg.iCapVideoHeight;
    EisVencCfg.EisCfg.stEisAttr.iVideoOutWidth  = 1536;//EisVencCfg.stUsrCfg.iCapVideoWidth;  /* In V5, it is equal */
    EisVencCfg.EisCfg.stEisAttr.iVideoOutHeight = 864;//EisVencCfg.stUsrCfg.iCapVideoHeight; /* In V5, it is equal */
    EisVencCfg.EisCfg.stEisAttr.iVideoFps = EisVencCfg.stUsrCfg.iVideoFps;
    EisVencCfg.EisCfg.stEisAttr.eVideoFmt = map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(V4L2_PIX_FMT_NV12M);
    EisVencCfg.EisCfg.stEisAttr.bUseKmat  = 0;
    EisVencCfg.EisCfg.stEisAttr.bSimuOffline = 1;
    EisVencCfg.EisCfg.stEisAttr.iDelayTimeMs = EisVencCfg.stUsrCfg.iTimeDelayMs;
    EisVencCfg.EisCfg.stEisAttr.iSyncErrTolerance = EisVencCfg.stUsrCfg.iTimeSyncErrTolerance;

    EisVencCfg.VencCfg.iVencChn = 0;
    EisVencCfg.VencCfg.stVencCfg.VeAttr.Type = PT_H264;
    EisVencCfg.VencCfg.stVencCfg.VeAttr.SrcPicWidth  = EisVencCfg.EisCfg.stEisAttr.iVideoOutWidth;
    EisVencCfg.VencCfg.stVencCfg.VeAttr.SrcPicHeight = EisVencCfg.EisCfg.stEisAttr.iVideoOutHeight;
    EisVencCfg.VencCfg.stVencCfg.VeAttr.Field = VIDEO_FIELD_FRAME;
    EisVencCfg.VencCfg.stVencCfg.VeAttr.PixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    EisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.bByFrame = TRUE;
    EisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.Profile  = 2;
    EisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.PicWidth = EisVencCfg.EisCfg.stEisAttr.iVideoOutWidth;
    EisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.PicHeight = EisVencCfg.EisCfg.stEisAttr.iVideoOutHeight;
    EisVencCfg.VencCfg.stVencCfg.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
    EisVencCfg.VencCfg.stVencCfg.RcAttr.mAttrH264Cbr.mSrcFrmRate = EisVencCfg.EisCfg.stEisAttr.iVideoFps;
    EisVencCfg.VencCfg.stVencCfg.RcAttr.mAttrH264Cbr.fr32DstFrmRate = EisVencCfg.EisCfg.stEisAttr.iVideoFps;
    EisVencCfg.VencCfg.stVencCfg.RcAttr.mAttrH264Cbr.mBitRate = 8388608; //8M

    EisVencCfg.pGyroDataFd = fopen(EisVencCfg.stUsrCfg.pGyroDataPath, "r");
    if (EisVencCfg.pGyroDataFd < 0) {
        aloge("Open %s file failed, check it.", EisVencCfg.stUsrCfg.pGyroDataPath);
        goto EOGyro;
    }
    EisVencCfg.pVideoDataFd = fopen(EisVencCfg.stUsrCfg.pVideoDataPath, "r");
    if (EisVencCfg.pVideoDataFd < 0) {
        aloge("Open %s file failed, check it.", EisVencCfg.stUsrCfg.pVideoDataPath);
        goto EOVideo;
    }
    EisVencCfg.pTsDataFd= fopen(EisVencCfg.stUsrCfg.pTsDataPath, "r");
    if (EisVencCfg.pTsDataFd < 0) {
        aloge("Open %s file failed, check it.", EisVencCfg.stUsrCfg.pTsDataPath);
        goto EOTs;
    }

    int iVeChn = EisVencCfg.VencCfg.iVencChn;
    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);
    AW_MPI_SYS_Init();

    /* First of all, create frame buffer manager. */
    InitFrameManager(&EisVencCfg.stFrmMgr, EisVencCfg.stUsrCfg.iInputBufNum, &EisVencCfg);

    g_iGetFrameCnt = 0;
    g_bExitFlag = 0;

    printf("sample_virvi2eis2venc buile time = %s, %s.\r\n", __DATE__, __TIME__);

    EisVencCfg.VencCfg.pSaveFd = fopen(EisVencCfg.stUsrCfg.pSaveFilePath, "w+");
    if (EisVencCfg.VencCfg.pSaveFd < 0) {
        aloge("Open saved file %s failed. errno %d.", EisVencCfg.stUsrCfg.pSaveFilePath, errno);
        goto EOSaveFile;
    }

    /* First of all: set all device attributions.
    * Then:
    * 1. Open and create venc->eis->vi device, then bind it.
    * 2. enable venc->eis->vi device.
    * 3. start venc->eis->vi channel.
    */
#ifndef EIS_BYPASS_VENC
    iRet = AW_MPI_VENC_CreateChn(iVeChn, &EisVencCfg.VencCfg.stVencCfg);
    if (SUCCESS != iRet) {
        aloge("Create VENC channel[%d] failed, iRet 0x%x.", iVeChn, iRet);
        goto ECrtVenc;
    }

    VencHeaderData stVencHeader;
    iRet = AW_MPI_VENC_GetH264SpsPpsInfo(iVeChn, &stVencHeader);
    if (SUCCESS == iRet)
    {
        if(stVencHeader.nLength)
        {
            fwrite(stVencHeader.pBuffer, stVencHeader.nLength, 1, EisVencCfg.VencCfg.pSaveFd);
        }
    }
    else
    {
        alogd("AW_MPI_VENC_GetH264SpsPpsInfo failed!\n");
        iRet = -1;
    }

    MPPCallbackInfo* pCallback = &EisVencCfg.VencCfg.stVencCallback;

    pCallback->callback = SampleEisVencBufferNotify;
    pCallback->cookie   = (void*)&EisVencCfg;
    AW_MPI_VENC_RegisterCallback(iVeChn, pCallback);
#endif
    iRet = AW_MPI_EIS_CreateChn(EisVencCfg.EisCfg.iEisChn, &EisVencCfg.EisCfg.stEisAttr);
    if (iRet != SUCCESS) {
        aloge("Create EIS channel[%d] failed. return 0x%x", EisVencCfg.EisCfg.iEisChn, iRet);
        goto ECrtEis;
    }

    MPPCallbackInfo* pEisCallback = &EisVencCfg.EisCfg.stEisCallback;

    pEisCallback->callback = SampleEisCompBufferNotify;
    pEisCallback->cookie   = (void*)&EisVencCfg;
    AW_MPI_EIS_RegisterCallback(EisVencCfg.EisCfg.iEisChn, pEisCallback);

    iRet = AW_MPI_EIS_EnableChn(EisVencCfg.EisCfg.iEisChn);
    if (iRet != SUCCESS) {
        aloge("Enable EIS channel[%d] failed. return 0x%x", EisVencCfg.EisCfg.iEisChn, iRet);
        goto EEnbEis;
    }

#ifndef EIS_BYPASS_VENC
    iRet = AW_MPI_VENC_StartRecvPic(iVeChn);
    if (SUCCESS != iRet) {
        aloge("Start receive VENC channel[%d] picture failed. iRet 0x%x!", iVeChn, iRet);
        goto EStrtVenc;
    }
#endif
    iRet = AW_MPI_EIS_StartChn(EisVencCfg.EisCfg.iEisChn);
    if (iRet != SUCCESS) {
        aloge("Start EIS channel[%d] failed. return 0x%x", EisVencCfg.EisCfg.iEisChn, iRet);
        goto EStrtEis;
    }

    signal(SIGINT, SampleExitHandle);

#ifndef EIS_BYPASS_VENC
    iRet = pthread_create(&EisVencCfg.pEncTrd, NULL, SampleEisVencThread, (void *)&EisVencCfg);
    if (iRet < 0) {
        aloge("Create EIS->VENC thread failed, iRet %d.", iRet);

        goto ECrtTrd1;
    }
#else
    FILE *pSaveOffYuv;
    pSaveOffYuv = fopen("/tmp/SampleEis2File.yuv", "w");
#endif
    iRet = pthread_create(&EisVencCfg.pSendDataTrd, NULL, SampleEisSendDataThread, (void *)&EisVencCfg);
    if (iRet < 0) {
        aloge("Create FILE->EIS thread failed, iRet %d.", iRet);
        goto ECrtTrd2;
    }

    while (!g_bExitFlag) {
        VIDEO_FRAME_INFO_S stEisStabFrmTmp;
        //1. Get picture datas.
        //2. Process picture datas.
        //3. Release picture datas.
        //4. break.
        iRet = AW_MPI_EIS_GetData(EisVencCfg.EisCfg.iEisChn, &stEisStabFrmTmp, 500);
        if (SUCCESS == iRet) {
#ifndef EIS_BYPASS_VENC
            AW_MPI_VENC_SendFrame(iVeChn, &stEisStabFrmTmp, 200);
#else
            fwrite(stEisStabFrmTmp.VFrame.mpVirAddr[0], stEisStabFrmTmp.VFrame.mHeight*stEisStabFrmTmp.VFrame.mWidth, 1, pSaveOffYuv);
            fwrite(stEisStabFrmTmp.VFrame.mpVirAddr[1], stEisStabFrmTmp.VFrame.mHeight*stEisStabFrmTmp.VFrame.mWidth/2, 1, pSaveOffYuv);
//            printf("%d, %d\r\n", stEisStabFrmTmp.VFrame.mWidth, stEisStabFrmTmp.VFrame.mHeight);
            AW_MPI_EIS_ReleaseData(EisVencCfg.EisCfg.iEisChn, &stEisStabFrmTmp);
#endif
        }
//        usleep(10*1000);
    }

#ifndef EIS_BYPASS_VENC
    pthread_join(EisVencCfg.pEncTrd, NULL);
#else
    fclose(pSaveOffYuv);
#endif
    pthread_join(EisVencCfg.pSendDataTrd, NULL);

    /* The close sequence.
    * 1. Stop vi->eis->venc channel.
    * 2. Disbale vi->eis->venc channel.
    * 3. Destroy venc->eis->vi channel.  VVVVVVVVVVVVVVVVery important.
    */
    AW_MPI_EIS_StopChn(EisVencCfg.EisCfg.iEisChn);
#ifndef EIS_BYPASS_VENC
    AW_MPI_VENC_StopRecvPic(iVeChn);
#endif
    AW_MPI_EIS_DisableChn(EisVencCfg.EisCfg.iEisChn);
#ifndef EIS_BYPASS_VENC
    AW_MPI_VENC_ResetChn(iVeChn);
    AW_MPI_VENC_DestroyChn(iVeChn);
#endif
    AW_MPI_EIS_DestroyChn(EisVencCfg.EisCfg.iEisChn);
    fclose(EisVencCfg.VencCfg.pSaveFd);
    fclose(EisVencCfg.pTsDataFd);
    fclose(EisVencCfg.pVideoDataFd);
    fclose(EisVencCfg.pGyroDataFd);
    DestroyFrameManager(&EisVencCfg.stFrmMgr);
    AW_MPI_SYS_Exit();

    return 0;

ECrtTrd2:
ECrtTrd1:
    AW_MPI_EIS_StopChn(EisVencCfg.EisCfg.iEisChn);
EStrtEis:
#ifndef EIS_BYPASS_VENC
    AW_MPI_VENC_ResetChn(iVeChn);
    AW_MPI_VENC_StopRecvPic(iVeChn);
EStrtVenc:
#endif
    AW_MPI_EIS_DisableChn(EisVencCfg.EisCfg.iEisChn);
EEnbEis:
    AW_MPI_EIS_DestroyChn(EisVencCfg.EisCfg.iEisChn);
ECrtEis:
#ifndef EIS_BYPASS_VENC
    AW_MPI_VENC_DestroyChn(iVeChn);
ECrtVenc:
#endif
    fclose(EisVencCfg.VencCfg.pSaveFd);
EOSaveFile:
    AW_MPI_SYS_Exit();
    fclose(EisVencCfg.pTsDataFd);
EOTs:
    fclose(EisVencCfg.pVideoDataFd);
EOVideo:
    fclose(EisVencCfg.pGyroDataFd);
EOGyro:
    DestroyFrameManager(&EisVencCfg.stFrmMgr);
    return iRet;
}
