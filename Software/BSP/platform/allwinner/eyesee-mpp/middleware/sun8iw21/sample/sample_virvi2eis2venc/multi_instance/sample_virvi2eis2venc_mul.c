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
#define LOG_TAG "sample_virvi2eis2venc_mul"
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
#include "vo/hwdisplay.h"

#include <confparser.h>
#include "sample_virvi2eis2venc_mul.h"
//#include "sample_virvi2eis2venc_config.h"

static bool g_bExitFlag = 0;
static bool g_bBreakProgress = 0;
static int g_iGetFrameCnt = 0;

void SampleExitHandle(int iSig)
{
    aloge("Get one exit signal.");
    g_bExitFlag = 1;
    g_bBreakProgress = 1;
    g_iGetFrameCnt = 0xFFFFFFF;
}

static int SampleVippStart(VI_DEV ViDev, VI_ATTR_S *pstAttr)
{
    int iRet = 0;

    iRet |= AW_MPI_VI_CreateVipp(ViDev);
    /* For tow vipp linked in the same once isp device,
    * must can invoke once. */
    iRet |= AW_MPI_VI_SetVippAttr(ViDev, pstAttr);
    iRet |= AW_MPI_VI_EnableVipp(ViDev);

    aloge("Begin set flip[%d].", ViDev);
//    AW_MPI_VI_SetVippFlip(ViDev, 1);
//    AW_MPI_VI_SetVippMirror(ViDev, 1);
    aloge("End set flip[%d].", ViDev);

    return iRet;
}

static void SampleVippEnd(VI_DEV ViDev)
{
    AW_MPI_VI_DisableVipp(ViDev);
    AW_MPI_VI_DestoryVipp(ViDev);
}

static int SampleVirviStart(VI_DEV ViDev, VI_CHN ViCh)
{
    int iRet = 0;

    iRet |= AW_MPI_VI_CreateVirChn(ViDev, ViCh, NULL);
    return iRet;
}

static void SampleVirviEnd(VI_DEV ViDev, VI_CHN ViCh)
{
    AW_MPI_VI_DestoryVirChn(ViDev, ViCh);
}

static void *SampleEisVencThread(void *pArgs)
{
    SampleVirvi2Eis2VencConfig* pViEisVencCfg = (SampleVirvi2Eis2VencConfig*)pArgs;

    int iVeChn = pViEisVencCfg->VencCfg.iVencChn;
    int iRet = 0;
    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPkt;

    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPkt;
    while (!g_bExitFlag && !g_bBreakProgress) {
        if (g_iGetFrameCnt >= pViEisVencCfg->stUsrCfg.iFrameCnt)
            g_bExitFlag = 1;
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
    }

    return NULL;
}

ERRORTYPE SampleEisVencBufferNotify(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleVirvi2Eis2VencConfig* pViEisVencCfg = (SampleVirvi2Eis2VencConfig*)cookie;

    VIDEO_FRAME_INFO_S* pFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
    switch (event) {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER: {
            AW_MPI_EIS_ReleaseData(pViEisVencCfg->EisCfg[0].iEisChn, pFrameInfo);
            //aloge("Release frame[%d].", pFrameInfo->mId);
        } break;

        default: break;
    }

    return SUCCESS;
}

static void *SampleEisVoThread(void *pArgs)
{
    SampleVirvi2Eis2VencConfig* pViEisVencCfg = (SampleVirvi2Eis2VencConfig*)pArgs;

    int iVoDev = pViEisVencCfg->VoCfg.iVoDev;
    int iVoChn = pViEisVencCfg->VoCfg.iVoChn;
    int iVoLyr = pViEisVencCfg->VoCfg.iVoLyr;
    int iRet = 0;

    VIDEO_FRAME_INFO_S stEisStabFrmTmp;
    while (!g_bExitFlag && !g_bBreakProgress) {
        if (g_iGetFrameCnt >= pViEisVencCfg->stUsrCfg.iFrameCnt)
            g_bExitFlag = 1;

        iRet = AW_MPI_EIS_GetData(pViEisVencCfg->EisCfg[1].iEisChn, &stEisStabFrmTmp, 500);
        if (SUCCESS == iRet) {
            iRet = AW_MPI_VO_SendFrame(iVoLyr, iVoChn, &stEisStabFrmTmp, 200);
            if (SUCCESS != iRet)
                AW_MPI_EIS_ReleaseData(pViEisVencCfg->EisCfg[1].iEisChn, &stEisStabFrmTmp);
        }
    }

    return NULL;
}

ERRORTYPE SampleEisVoBufferNotify(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleVirvi2Eis2VencConfig* pViEisVencCfg = (SampleVirvi2Eis2VencConfig*)cookie;

    VIDEO_FRAME_INFO_S* pFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
    switch (event) {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER: {
            AW_MPI_EIS_ReleaseData(pViEisVencCfg->EisCfg[1].iEisChn, pFrameInfo);
            //aloge("Release frame[%d].", pFrameInfo->mId);
        } break;

        default: break;
    }

    return SUCCESS;
}

static inline int LoadSampleViEisConfig(SampleViEisUsrCfg *pConfig, const char *conf_path)
{
    int iRet;

    CONFPARSER_S stConfParser;
    iRet = createConfParser(conf_path, &stConfParser);
    if(iRet < 0)
    {
        alogd("user not set config file. use default test parameter!");
        pConfig->iCapDev = 1;
        pConfig->iCapVideoWidth = 1920;
        pConfig->iCapVideoHeight= 1080;
        pConfig->iVideoFps = 30;
        pConfig->iInputBufNum = 8;
        pConfig->iOutBufNum   = 5;
        pConfig->iTimeDelayMs = 33;
        pConfig->iTimeSyncErrTolerance = 2;

        pConfig->iRebootCnt = 1000;
        pConfig->iFrameCnt  = 30*5;
        strncpy(pConfig->pSaveFilePath, SAVE_FILE_NAME, sizeof(pConfig->pSaveFilePath));

        pConfig->bUseVo = 1;
        pConfig->bUseVenc = 0;
        pConfig->iDisW = 1920;
        pConfig->iDisH = 1080;
        pConfig->eDispType = VO_INTF_HDMI;
        pConfig->eDispSize = VO_OUTPUT_3840x2160_30;
        goto use_default_conf;
    }

    if (pConfig->iCapDev == -1)
        pConfig->iCapDev = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_VDEV, 0);
    if (pConfig->iCapVideoWidth == -1)
        pConfig->iCapVideoWidth  = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_CAPWIDTH, 0);
    if (pConfig->iCapVideoHeight == -1)
        pConfig->iCapVideoHeight = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_CAPHEIGH, 0);
    if (pConfig->iVideoFps == -1)
        pConfig->iVideoFps = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_FPS, 0);
    if (pConfig->iInputBufNum == -1)
        pConfig->iInputBufNum = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_INBUFCNT, 0);
    if (pConfig->iOutBufNum == -1)
        pConfig->iOutBufNum = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_OUTBUFCNT, 0);
    if (pConfig->iTimeDelayMs == -1)
        pConfig->iTimeDelayMs = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_TDELAY, 0);
    if (pConfig->iTimeSyncErrTolerance == -1)
        pConfig->iTimeSyncErrTolerance = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_SYNCTOL, 0);
    if (pConfig->iRebootCnt == -1)
        pConfig->iRebootCnt = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_REBOOTCNT, 0);
    if (pConfig->iFrameCnt == -1)
        pConfig->iFrameCnt = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_FRMCNT, 0);

    if (pConfig->bUseVo == 0)
        pConfig->bUseVo = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_USEVO, 0);
    if (pConfig->bUseVenc == 0)
        pConfig->bUseVenc = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_USEVENC, 0);
    if (pConfig->iDisW == -1)
        pConfig->iDisW = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_DISPW, 0);
    if (pConfig->iDisH == -1)
        pConfig->iDisH = GetConfParaInt(&stConfParser, SAMPLE_VIEIS_DISPH, 0);

    char *pcTmpPtr;
    pcTmpPtr = (char *)GetConfParaString(&stConfParser, SAMPLE_VIEIS_SAVEFILE, NULL);
    if (NULL != pcTmpPtr && pConfig->pSaveFilePath[0] == 0xFF) {
        strncpy(pConfig->pSaveFilePath, pcTmpPtr, sizeof(pConfig->pSaveFilePath));
    }

    pConfig->eDispType = VO_INTF_HDMI;
    if (pConfig->iDisW > 1920)
        pConfig->eDispSize = VO_OUTPUT_3840x2160_30;
    else if (pConfig->iDisW > 1280)
        pConfig->eDispSize = VO_OUTPUT_1080P30;
    else
        pConfig->eDispSize = VO_OUTPUT_720P60;

    pcTmpPtr = (char *)GetConfParaString(&stConfParser, SAMPLE_VIEIS_DISPTYPE, NULL);
    if (NULL != pcTmpPtr && pConfig->pVoDispType[0] == 0xFF) {
        if (!strcmp(pcTmpPtr, "lcd")) {
            pConfig->eDispType = VO_INTF_LCD;
            pConfig->eDispSize = VO_OUTPUT_NTSC;
        }
    } else if (pConfig->pVoDispType[0] != 0xFF) {
        if (!strcmp(pConfig->pVoDispType, "lcd")) {
            pConfig->eDispType = VO_INTF_LCD;
            pConfig->eDispSize = VO_OUTPUT_NTSC;
        }
    }

use_default_conf:
    alogd("in_dev=%d,in_width=%d,in_height=%d,fps=%d,ibuf_num=%d,obuf_num=%d,delayms=%d,tolerance=%d",
        pConfig->iCapDev, pConfig->iCapVideoWidth, pConfig->iCapVideoHeight, pConfig->iVideoFps,
        pConfig->iInputBufNum, pConfig->iOutBufNum, pConfig->iTimeDelayMs, pConfig->iTimeSyncErrTolerance);
    alogd("reboot_cnt=%d,frame_cnt=%d,save_file=%s,bUseVo=%d,disp_w=%d,disp_h=%d,bUseVenc=%d,disp_type=%d,",
        pConfig->iRebootCnt, pConfig->iFrameCnt, pConfig->pSaveFilePath, pConfig->bUseVo, pConfig->iDisW, pConfig->iDisH,
        pConfig->bUseVenc, pConfig->eDispType);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

static struct option pstLongOptions[] = {
   {"help",    no_argument,       0, 'h'},
   {"path",    required_argument, 0, 'p'},
   {"device",  required_argument, 0, 'e'},
   {"width",   required_argument, 0, 'x'},
   {"height",  required_argument, 0, 'y'},
   {"fps",     required_argument, 0, 'f'},
   {"testcnt", required_argument, 0, 't'},
   {"input",   required_argument, 0, 'i'},
   {"output",  required_argument, 0, 'o'},
   {"delay",   required_argument, 0, 'd'},
   {"errtol",  required_argument, 0, 128},
   {"frmcnt",  required_argument, 0, 129},
   {"savefile",required_argument, 0, 130},
   {"usevo",   no_argument,       0, 131},
   {"disp_w",  required_argument, 0, 132},
   {"disp_h",  required_argument, 0, 133},
   {"disp_type",required_argument, 0, 134},
   {"usevenc", no_argument,       0, 135},
   {0,         0,                 0, 0}
};

static char pcHelpInfo[] = 
"\033[33m"
"exec [-h|--help] [-p|--path]\n"
"   <-h|--help>: print to the help information\n"
"   <-p|--path>   <args>: point to the configuration file path\n"
"   <-e|--device> <args>: witch device do you want to open\n"
"   <-x|--width>  <args>: set video picture width\n"
"   <-y|--height> <args>: set video picture height\n"
"   <-f|--fps>    <args>: set the video capture frequency\n"
"   <-t|--testcnt><args>: set the reboot test count number\n"
"   <-i|--input>  <args>: how much input video frame number for EIS process\n"
"   <-o|--output> <args>: how much output video frame number for EIS output\n"
"   <-d|--delay>  <args>: how much times delay in gyro and video sync\n"
"   <--usevo>     <args>: If use VO to preview the output video\n"
"   <--disp_w>    <args>: Disply width.\n"
"   <--disp_h>    <args>: Display width.\n"
"   <--disp_type> <args>: Disply in HDMI or LCD\n"
"   <--usevenc>   <args>: If you use venc to encode output video.\n"
"   <--errtol>    <args>: how much times sync error can you endure\n"
"   <--frmcnt>    <args>: how much video frame count do you want to got in every test time.\n"
"   <--savefile>  <args>: where do you want to save the processed video frames.\n"
"\033[0m\n";

static int ParseCmdLine(int argc, char **argv, SampleViEisUsrCfg *pCmdLinePara)
{
    int mRet;
    int iOptIndex = 0;

    memset(pCmdLinePara, -1, sizeof(SampleViEisUsrCfg));
    pCmdLinePara->pConfFilePath[0] = 0;
    pCmdLinePara->bUseVo = 0;
    pCmdLinePara->bUseVenc = 0;
    while (1) {
        mRet = getopt_long(argc, argv, ":p:he:x:y:f:t:i:o:d:", pstLongOptions, &iOptIndex);
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
            case 'e':
                alogd("video device is [%s]\n", optarg);
                pCmdLinePara->iCapDev = atoi(optarg);
                break;
            case 'x':
                alogd("width is [%d]\n", atoi(optarg));
                pCmdLinePara->iCapVideoWidth  = atoi(optarg);
                break;
            case 'y':
                alogd("height is [%d]\n", atoi(optarg));
                pCmdLinePara->iCapVideoHeight = atoi(optarg);
                break;
            case 'f':
                alogd("video frame frequency is [%s]\n", optarg);
                pCmdLinePara->iVideoFps = atoi(optarg);
                break;
            case 't':
                alogd("do reboot sample test count is [%s]\n", optarg);
                pCmdLinePara->iRebootCnt = atoi(optarg);
                break;
            case 'i':
                alogd("set input video frame count [%s]\n", optarg);
                pCmdLinePara->iInputBufNum = atoi(optarg);
                break;
            case 'o':
                alogd("set output video frame count [%s]\n", optarg);
                pCmdLinePara->iOutBufNum = atoi(optarg);
                break;
            case 'd':
                alogd("set sync delay time is [%s]ms\n", optarg);
                pCmdLinePara->iTimeDelayMs = atoi(optarg);
                break;
            case 128: /* sync error tolerance: ms */
                alogd("sync error tolerance: ms [%s]\n", optarg);
                pCmdLinePara->iTimeSyncErrTolerance = atoi(optarg);
                break;
            case 129: /* video frames in every test time */
                alogd("we will get [%s] video frames in every test time\n", optarg);
                pCmdLinePara->iFrameCnt = atoi(optarg);
                break;
            case 130: /* save file path */
                alogd("we will save the processed frame to [%s]\n", optarg);
                strncpy(pCmdLinePara->pSaveFilePath, optarg, sizeof(pCmdLinePara->pSaveFilePath));
                break;
            case 131: /* if use VO */
                alogd("we will use VO to display output video\n");
                pCmdLinePara->bUseVo = 1;
                break;
            case 132: /* display width */
                alogd("we will save the processed frame to [%s]\n", optarg);
                pCmdLinePara->iDisW = atoi(optarg);
                break;
            case 133: /* display height */
                alogd("we will save the processed frame to [%s]\n", optarg);
                pCmdLinePara->iDisH = atoi(optarg);
                break;
            case 134: /* display type, lcd/hdmi */
                alogd("we will save the processed frame to [%s]\n", optarg);
                strncpy(pCmdLinePara->pVoDispType, optarg, sizeof(pCmdLinePara->pVoDispType));
                break;
            case 135: /* if use venc */
                alogd("we will use venc to encode output videos\n");
                pCmdLinePara->bUseVenc = 1;
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
    SampleVirvi2Eis2VencConfig ViEisVencCfg;
    memset(&ViEisVencCfg, 0, sizeof(SampleVirvi2Eis2VencConfig));

    iRet = ParseCmdLine(argc, argv, &ViEisVencCfg.stUsrCfg);
    if (iRet < 0)
        return iRet;
    else if (0 != iRet)
        return 0;

    /* Always be success */
    LoadSampleViEisConfig(&ViEisVencCfg.stUsrCfg, ViEisVencCfg.stUsrCfg.pConfFilePath);

    int i = 0;
    MPP_SYS_CONF_S stSysConf;
    VI_ATTR_S* pVippAttr[2];
    pVippAttr[0] = &ViEisVencCfg.ViCfg[0].stVippAttr;
    ViEisVencCfg.ViCfg[0].iViDev = 0;//ViEisVencCfg.stUsrCfg.iCapDev;
    ViEisVencCfg.ViCfg[0].iViChn = 0;

    pVippAttr[1] = &ViEisVencCfg.ViCfg[1].stVippAttr;
    ViEisVencCfg.ViCfg[1].iViDev = 1;//ViEisVencCfg.stUsrCfg.iCapDev;
    ViEisVencCfg.ViCfg[1].iViChn = 0;

    /* Set VIPP device's Attribute */
    pVippAttr[0]->type    = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pVippAttr[0]->memtype = V4L2_MEMORY_MMAP;
    pVippAttr[0]->format.pixelformat = V4L2_PIX_FMT_NV12M;
    pVippAttr[0]->format.field  = V4L2_FIELD_NONE;
    pVippAttr[0]->format.width  = ViEisVencCfg.stUsrCfg.iCapVideoWidth;
    pVippAttr[0]->format.height = ViEisVencCfg.stUsrCfg.iCapVideoHeight;
    pVippAttr[0]->fps = ViEisVencCfg.stUsrCfg.iVideoFps;
    /* Update configuration anyway, do not use current configuration */
    pVippAttr[0]->use_current_win = 0;
    pVippAttr[0]->nbufs = ViEisVencCfg.stUsrCfg.iInputBufNum+2;
    pVippAttr[0]->nplanes = 2;

    /* Set VIPP device's Attribute */
    pVippAttr[1]->type    = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pVippAttr[1]->memtype = V4L2_MEMORY_MMAP;
    pVippAttr[1]->format.pixelformat = V4L2_PIX_FMT_NV12M;
    pVippAttr[1]->format.field  = V4L2_FIELD_NONE;
    pVippAttr[1]->format.width  = 800;//ViEisVencCfg.stUsrCfg.iCapVideoWidth;
    pVippAttr[1]->format.height = 448;//ViEisVencCfg.stUsrCfg.iCapVideoHeight;
    pVippAttr[1]->fps = ViEisVencCfg.stUsrCfg.iVideoFps;
    /* Update configuration anyway, do not use current configuration */
    pVippAttr[1]->use_current_win = 1;
    pVippAttr[1]->nbufs = ViEisVencCfg.stUsrCfg.iInputBufNum+2;
    pVippAttr[1]->nplanes = 2;

    /* EIS create and enable */
    ViEisVencCfg.EisCfg[0].iEisChn = 0;
    ViEisVencCfg.EisCfg[0].stEisAttr.iGyroFreq     = 1000;
    ViEisVencCfg.EisCfg[0].stEisAttr.iGyroAxiNum   = 3;
    ViEisVencCfg.EisCfg[0].stEisAttr.iGyroPoolSize = 1000;
    ViEisVencCfg.EisCfg[0].stEisAttr.iInputBufNum  = ViEisVencCfg.stUsrCfg.iInputBufNum;
    ViEisVencCfg.EisCfg[0].stEisAttr.iOutputBufBum = ViEisVencCfg.stUsrCfg.iOutBufNum;
    ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInWidth = pVippAttr[0]->format.width;
    ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInHeight = pVippAttr[0]->format.height;
    ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInWidthStride  = pVippAttr[0]->format.width;
    ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInHeightStride = pVippAttr[0]->format.height;
    ViEisVencCfg.EisCfg[0].stEisAttr.iVideoFps = pVippAttr[0]->fps;
    ViEisVencCfg.EisCfg[0].stEisAttr.eOperationMode = (pVippAttr[0]->fps == 30) ? EIS_OPR_1080P30 : EIS_OPR_1080P60;
    ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutWidth  = 1536;
    ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutHeight = 864;
    if (ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInWidth == 800
        && ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInHeight == 448) {
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutWidth  = 640;
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutHeight = 360;
        ViEisVencCfg.EisCfg[0].stEisAttr.eOperationMode = (pVippAttr[0]->fps == 30) ? EIS_OPR_VGA30 : EIS_OPR_VGA60;
    } else if (ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInWidth == 2400
        && ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInHeight == 1344) {
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutWidth  = 1920;
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutHeight = 1080;
        ViEisVencCfg.EisCfg[0].stEisAttr.eOperationMode = EIS_OPR_1080P30;
    } else if (ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInWidth == 2016
        && ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInHeight == 1128) {
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutWidth  = 1600;
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutHeight = 896;
        ViEisVencCfg.EisCfg[0].stEisAttr.eOperationMode = EIS_OPR_1080P60;
    } else if (ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInWidth == 4032
        && ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInHeight == 2256) {
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutWidth  = 3840;
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutHeight = 2160;
        ViEisVencCfg.EisCfg[0].stEisAttr.eOperationMode = EIS_OPR_4K30;
    } else if (ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInWidth == 3360
        && ViEisVencCfg.EisCfg[0].stEisAttr.iVideoInHeight == 1888) {
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutWidth  = 2688;
        ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutHeight = 1520;
        ViEisVencCfg.EisCfg[0].stEisAttr.eOperationMode = EIS_OPR_2P7K30;
    }
    ViEisVencCfg.EisCfg[0].stEisAttr.iVideoFps = pVippAttr[0]->fps;

    ViEisVencCfg.EisCfg[1].iEisChn = 1;
    ViEisVencCfg.EisCfg[1].stEisAttr.iGyroFreq     = 1000;
    ViEisVencCfg.EisCfg[1].stEisAttr.iGyroAxiNum   = 3;
    ViEisVencCfg.EisCfg[1].stEisAttr.iGyroPoolSize = 1000;
    ViEisVencCfg.EisCfg[1].stEisAttr.iInputBufNum  = ViEisVencCfg.stUsrCfg.iInputBufNum;
    ViEisVencCfg.EisCfg[1].stEisAttr.iOutputBufBum = ViEisVencCfg.stUsrCfg.iOutBufNum;
    ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInWidth = pVippAttr[1]->format.width;
    ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInHeight = pVippAttr[1]->format.height;
    ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInWidthStride  = pVippAttr[1]->format.width;
    ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInHeightStride = pVippAttr[1]->format.height;
    ViEisVencCfg.EisCfg[1].stEisAttr.iVideoFps = pVippAttr[1]->fps;
    ViEisVencCfg.EisCfg[1].stEisAttr.eOperationMode = (pVippAttr[1]->fps == 30) ? EIS_OPR_1080P30 : EIS_OPR_1080P60;
    ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutWidth  = 1536;
    ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutHeight = 864;
    if (ViEisVencCfg.EisCfg[1].stEisAttr.eOperationMode == EIS_OPR_1080P60) {
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutWidth  = 1344;
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutHeight = 768;
    }
    if (ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInWidth == 800
        && ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInHeight == 448) {
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutWidth  = 640;
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutHeight = 360;
        ViEisVencCfg.EisCfg[1].stEisAttr.eOperationMode = (pVippAttr[0]->fps == 30) ? EIS_OPR_VGA30 : EIS_OPR_VGA60;
    } else if (ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInWidth == 2400
        && ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInHeight == 1344) {
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutWidth  = 1920;
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutHeight = 1080;
        ViEisVencCfg.EisCfg[1].stEisAttr.eOperationMode = EIS_OPR_1080P30;
    } else if (ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInWidth == 2016
        && ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInHeight == 1128) {
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutWidth  = 1600;
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutHeight = 896;
        ViEisVencCfg.EisCfg[1].stEisAttr.eOperationMode = EIS_OPR_1080P60;
    } else if (ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInWidth == 4032
        && ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInHeight == 2256) {
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutWidth  = 3840;
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutHeight = 2160;
        ViEisVencCfg.EisCfg[1].stEisAttr.eOperationMode = EIS_OPR_4K30;
    } else if (ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInWidth == 3360
        && ViEisVencCfg.EisCfg[1].stEisAttr.iVideoInHeight == 1888) {
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutWidth  = 2688;
        ViEisVencCfg.EisCfg[1].stEisAttr.iVideoOutHeight = 1520;
        ViEisVencCfg.EisCfg[1].stEisAttr.eOperationMode = EIS_OPR_2P7K30;
    }
    ViEisVencCfg.EisCfg[1].stEisAttr.iVideoFps = pVippAttr[1]->fps;

#if 0
    ViEisVencCfg.EisCfg[0].stEisAttr.eEisAlgoMode = EIS_ALGO_MODE_BP;
    ViEisVencCfg.EisCfg[0].stEisAttr.pBPDataSavePath = "/tmp";
    ViEisVencCfg.EisCfg[0].stEisAttr.bSaveYUV = 0;

    ViEisVencCfg.EisCfg[1].stEisAttr.eEisAlgoMode = EIS_ALGO_MODE_BP;
    ViEisVencCfg.EisCfg[1].stEisAttr.pBPDataSavePath = "/tmp";
    ViEisVencCfg.EisCfg[1].stEisAttr.bSaveYUV = 0;
#endif
    ViEisVencCfg.EisCfg[0].stEisAttr.eVideoFmt = map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(V4L2_PIX_FMT_NV12M);
    ViEisVencCfg.EisCfg[0].stEisAttr.bUseKmat  = 0;
    ViEisVencCfg.EisCfg[0].stEisAttr.iDelayTimeMs = ViEisVencCfg.stUsrCfg.iTimeDelayMs;
    ViEisVencCfg.EisCfg[0].stEisAttr.iSyncErrTolerance = ViEisVencCfg.stUsrCfg.iTimeSyncErrTolerance;

    ViEisVencCfg.EisCfg[1].stEisAttr.eVideoFmt = map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(V4L2_PIX_FMT_NV12M);
    ViEisVencCfg.EisCfg[1].stEisAttr.bUseKmat  = 0;
    ViEisVencCfg.EisCfg[1].stEisAttr.iDelayTimeMs = ViEisVencCfg.stUsrCfg.iTimeDelayMs;
    ViEisVencCfg.EisCfg[1].stEisAttr.iSyncErrTolerance = ViEisVencCfg.stUsrCfg.iTimeSyncErrTolerance;

    memset(&ViEisVencCfg.VencCfg, 0, sizeof(SampleEncConfig));
    ViEisVencCfg.VencCfg.iVencChn = 0;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.Type = PT_H264;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.MaxKeyInterval = ViEisVencCfg.EisCfg[0].stEisAttr.iVideoFps;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.SrcPicWidth  = ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutWidth;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.SrcPicHeight = ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutHeight;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.Field = VIDEO_FIELD_FRAME;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.PixelFormat = ViEisVencCfg.EisCfg[0].stEisAttr.eVideoFmt;

    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.Profile  = 1;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.bByFrame = TRUE;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.PicWidth = ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutWidth;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.PicHeight = ViEisVencCfg.EisCfg[0].stEisAttr.iVideoOutHeight;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
    ViEisVencCfg.VencCfg.stVencCfg.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
    ViEisVencCfg.VencCfg.stVencCfg.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
    ViEisVencCfg.VencCfg.stVencCfg.RcAttr.mAttrH264Cbr.mSrcFrmRate = ViEisVencCfg.EisCfg[0].stEisAttr.iVideoFps;
    ViEisVencCfg.VencCfg.stVencCfg.RcAttr.mAttrH264Cbr.fr32DstFrmRate = ViEisVencCfg.EisCfg[0].stEisAttr.iVideoFps;
    ViEisVencCfg.VencCfg.stVencCfg.RcAttr.mAttrH264Cbr.mBitRate = 8388608; //8M
    ViEisVencCfg.VencCfg.stVencCfg.RcAttr.mAttrH264Cbr.mMaxQp = 51;
    ViEisVencCfg.VencCfg.stVencCfg.RcAttr.mAttrH264Cbr.mMinQp = 1;

    int iVeChn = ViEisVencCfg.VencCfg.iVencChn;
    MPP_CHN_S stViComp[2]  = {
        {MOD_ID_VIU, ViEisVencCfg.ViCfg[0].iViDev, ViEisVencCfg.ViCfg[0].iViChn},
        {MOD_ID_VIU, ViEisVencCfg.ViCfg[1].iViDev, ViEisVencCfg.ViCfg[1].iViChn},
    };
    MPP_CHN_S stEisComp[2] = {
        {MOD_ID_EIS, 0, ViEisVencCfg.EisCfg[0].iEisChn},
        {MOD_ID_EIS, 0, ViEisVencCfg.EisCfg[1].iEisChn},
    };

    for (i = 0; (i < ViEisVencCfg.stUsrCfg.iRebootCnt)&&!g_bBreakProgress; i++) {
        aloge("This is the =====================%d==================== tests.", i);

        memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
        stSysConf.nAlignWidth = 32;
        AW_MPI_SYS_SetConf(&stSysConf);
        AW_MPI_SYS_Init();

        g_iGetFrameCnt = 0;
        g_bExitFlag = 0;

        printf("sample_virvi2eis2venc_mul buile time = %s, %s.\r\n", __DATE__, __TIME__);
#if 1
        ViEisVencCfg.VoCfg.iVoDev = 0;
        ViEisVencCfg.VoCfg.iVoLyr = 0;

        VO_PUB_ATTR_S spPubAttr;
        AW_MPI_VO_GetPubAttr(ViEisVencCfg.VoCfg.iVoDev, &spPubAttr);
        spPubAttr.enIntfType = ViEisVencCfg.stUsrCfg.eDispType;
        spPubAttr.enIntfSync = ViEisVencCfg.stUsrCfg.eDispSize;
        AW_MPI_VO_SetPubAttr(ViEisVencCfg.VoCfg.iVoDev, &spPubAttr);
        ViEisVencCfg.VoCfg.eDispType = ViEisVencCfg.stUsrCfg.eDispType;
        ViEisVencCfg.VoCfg.eDispSize = ViEisVencCfg.stUsrCfg.eDispSize;

        iRet = AW_MPI_VO_Enable(ViEisVencCfg.VoCfg.iVoDev);
        if (SUCCESS != iRet) {
            aloge("Enable VO device[%d] failed. errno 0x%x.", ViEisVencCfg.VoCfg.iVoDev, iRet);
            goto EOVODev;
        }

        AW_MPI_VO_AddOutsideVideoLayer(HLAY(2,0));
        AW_MPI_VO_CloseVideoLayer(HLAY(2,0)); /* close ui layer. */
        iRet = AW_MPI_VO_EnableVideoLayer(ViEisVencCfg.VoCfg.iVoLyr);
        if (SUCCESS != iRet) {
            aloge("Enable VO layer[%d] failed. errno 0x%x.", ViEisVencCfg.VoCfg.iVoLyr, iRet);
            goto EOVOLyr;
        }

        AW_MPI_VO_GetVideoLayerAttr(ViEisVencCfg.VoCfg.iVoLyr, &ViEisVencCfg.VoCfg.stVoAttr);
        ViEisVencCfg.VoCfg.stVoAttr.stDispRect.X = 0;
        ViEisVencCfg.VoCfg.stVoAttr.stDispRect.Y = 0;
        ViEisVencCfg.VoCfg.stVoAttr.stDispRect.Width  = ViEisVencCfg.stUsrCfg.iDisW;
        ViEisVencCfg.VoCfg.stVoAttr.stDispRect.Height = ViEisVencCfg.stUsrCfg.iDisH;
        AW_MPI_VO_SetVideoLayerAttr(ViEisVencCfg.VoCfg.iVoLyr, &ViEisVencCfg.VoCfg.stVoAttr);
        AW_MPI_VO_OpenVideoLayer(ViEisVencCfg.VoCfg.iVoLyr);

        ViEisVencCfg.VoCfg.iVoChn = 0;
        AW_MPI_VO_EnableChn(ViEisVencCfg.VoCfg.iVoLyr, ViEisVencCfg.VoCfg.iVoChn);

        MPPCallbackInfo *pVoCallBack = &ViEisVencCfg.VoCfg.stVoCallback;
        pVoCallBack->cookie = (void*)&ViEisVencCfg;
        pVoCallBack->callback = (MPPCallbackFuncType)&SampleEisVoBufferNotify;
        AW_MPI_VO_RegisterCallback(ViEisVencCfg.VoCfg.iVoLyr, ViEisVencCfg.VoCfg.iVoChn, pVoCallBack);
        AW_MPI_VO_SetChnDispBufNum(ViEisVencCfg.VoCfg.iVoLyr, ViEisVencCfg.VoCfg.iVoChn, 2);
        AW_MPI_VO_StartChn(ViEisVencCfg.VoCfg.iVoLyr, ViEisVencCfg.VoCfg.iVoChn);

        ViEisVencCfg.VencCfg.pSaveFd = fopen(ViEisVencCfg.stUsrCfg.pSaveFilePath, "w+");
        if (ViEisVencCfg.VencCfg.pSaveFd < 0) {
            aloge("Open saved file %s failed. errno %d.", SAVE_FILE_NAME, errno);
            goto EOSaveFile;
        }

        /* First of all: set all device attributions.
        * Then:
        * 1. Open and create venc->eis->vi device, then bind it.
        * 2. enable venc->eis->vi device.
        * 3. start venc->eis->vi channel.
        */
        iRet = AW_MPI_VENC_CreateChn(iVeChn, &ViEisVencCfg.VencCfg.stVencCfg);
        if (SUCCESS != iRet) {
            aloge("Create VENC channel[%d] failed, iRet 0x%x.", iVeChn, iRet);
            goto ECrtVenc;
        }

        VENC_FRAME_RATE_S stVencFrmRate;
        stVencFrmRate.SrcFrmRate = ViEisVencCfg.EisCfg[0].stEisAttr.iVideoFps;
        stVencFrmRate.DstFrmRate = ViEisVencCfg.EisCfg[0].stEisAttr.iVideoFps;
        AW_MPI_VENC_SetFrameRate(iVeChn, &stVencFrmRate);

        VencHeaderData stVencHeader;
        iRet = AW_MPI_VENC_GetH264SpsPpsInfo(iVeChn, &stVencHeader);
        if (SUCCESS == iRet)
        {
            if(stVencHeader.nLength)
            {
                fwrite(stVencHeader.pBuffer, stVencHeader.nLength, 1, ViEisVencCfg.VencCfg.pSaveFd);
            }
        }
        else
        {
            alogd("AW_MPI_VENC_GetH264SpsPpsInfo failed!\n");
            iRet = -1;
        }

        MPPCallbackInfo* pCallback = &ViEisVencCfg.VencCfg.stVencCallback;

        pCallback->callback = SampleEisVencBufferNotify;
        pCallback->cookie   = (void*)&ViEisVencCfg;
        AW_MPI_VENC_RegisterCallback(iVeChn, pCallback);

        iRet = AW_MPI_VENC_StartRecvPic(iVeChn);
        if (SUCCESS != iRet) {
            aloge("Start receive VENC channel[%d] picture failed. iRet 0x%x!", iVeChn, iRet);
            goto EStrtVenc;
        }

        iRet = pthread_create(&ViEisVencCfg.pEncTrd, NULL, SampleEisVencThread, (void *)&ViEisVencCfg);
        if (iRet < 0) {
            aloge("Create EIS->VENC thread failed, iRet %d.", iRet);
            goto ECrtTrd;
        }
#endif

#if 1
        iRet = AW_MPI_EIS_CreateChn(ViEisVencCfg.EisCfg[0].iEisChn, &ViEisVencCfg.EisCfg[0].stEisAttr);
        if (iRet != SUCCESS) {
            aloge("Create EIS channel[%d] failed. return 0x%x", ViEisVencCfg.EisCfg[0].iEisChn, iRet);
            goto ECrtEis0;
        }

        iRet = AW_MPI_EIS_CreateChn(ViEisVencCfg.EisCfg[1].iEisChn, &ViEisVencCfg.EisCfg[1].stEisAttr);
        if (iRet != SUCCESS) {
            aloge("Create EIS channel[%d] failed. return 0x%x", ViEisVencCfg.EisCfg[1].iEisChn, iRet);
            goto ECrtEis1;
        }

        iRet = AW_MPI_EIS_EnableChn(ViEisVencCfg.EisCfg[0].iEisChn);
        if (iRet != SUCCESS) {
            aloge("Enable EIS channel[%d] failed. return 0x%x", ViEisVencCfg.EisCfg[0].iEisChn, iRet);
            goto EEnbEis0;
        }

        iRet = AW_MPI_EIS_EnableChn(ViEisVencCfg.EisCfg[1].iEisChn);
        if (iRet != SUCCESS) {
            aloge("Enable EIS channel[%d] failed. return 0x%x", ViEisVencCfg.EisCfg[1].iEisChn, iRet);
            goto EEnbEis1;
        }
#endif
        iRet = SampleVippStart(ViEisVencCfg.ViCfg[0].iViDev, pVippAttr[0]);
        if (iRet != SUCCESS) {
            aloge("Open vipp device[%d] failed, ret 0x%x.", ViEisVencCfg.ViCfg[0].iViDev, iRet);
            goto EVippOpen1;
        }

        AW_MPI_ISP_Init();
        AW_MPI_ISP_Run(0);

        iRet = SampleVippStart(ViEisVencCfg.ViCfg[1].iViDev, pVippAttr[1]);
        if (iRet != SUCCESS) {
            aloge("Open vipp device[%d] failed, ret 0x%x.", ViEisVencCfg.ViCfg[1].iViDev, iRet);
            goto EVippOpen0;
        }

        iRet = SampleVirviStart(ViEisVencCfg.ViCfg[0].iViDev, ViEisVencCfg.ViCfg[0].iViChn);
        if (iRet != SUCCESS) {
            aloge("Create vipp device[%d]Chn[%d] failed, ret 0x%x.",
                ViEisVencCfg.ViCfg[0].iViDev, ViEisVencCfg.ViCfg[0].iViChn, iRet);
            goto EVirviCrt0;
        }

        iRet = SampleVirviStart(ViEisVencCfg.ViCfg[1].iViDev, ViEisVencCfg.ViCfg[1].iViChn);
        if (iRet != SUCCESS) {
            aloge("Create vipp device[%d]Chn[%d] failed, ret 0x%x.",
                ViEisVencCfg.ViCfg[1].iViDev, ViEisVencCfg.ViCfg[1].iViChn, iRet);
            goto EVirviCrt1;
        }
#if 1
        iRet = AW_MPI_SYS_Bind(&stViComp[0], &stEisComp[0]);
        if (iRet != SUCCESS) {
            aloge("Bind VIDev[%d]VIchn[%d] to EISChn[%d] failed. return 0x%x",
                ViEisVencCfg.ViCfg[0].iViDev, ViEisVencCfg.ViCfg[0].iViChn, ViEisVencCfg.EisCfg[0].iEisChn, iRet);
            goto EBindViEis0;
        }

        iRet = AW_MPI_SYS_Bind(&stViComp[1], &stEisComp[1]);
        if (iRet != SUCCESS) {
            aloge("Bind VIDev[%d]VIchn[%d] to EISChn[%d] failed. return 0x%x",
                ViEisVencCfg.ViCfg[1].iViDev, ViEisVencCfg.ViCfg[1].iViChn, ViEisVencCfg.EisCfg[1].iEisChn, iRet);
            goto EBindViEis1;
        }

        iRet = AW_MPI_EIS_StartChn(ViEisVencCfg.EisCfg[0].iEisChn);
        if (iRet != SUCCESS) {
            aloge("Start EIS channel[%d] failed. return 0x%x", ViEisVencCfg.EisCfg[0].iEisChn, iRet);
            goto EStrtEis0;
        }

        iRet = AW_MPI_EIS_StartChn(ViEisVencCfg.EisCfg[1].iEisChn);
        if (iRet != SUCCESS) {
            aloge("Start EIS channel[%d] failed. return 0x%x", ViEisVencCfg.EisCfg[1].iEisChn, iRet);
            goto EStrtEis1;
        }

        pthread_create(&ViEisVencCfg.pDispTrd, NULL, SampleEisVoThread, &ViEisVencCfg);
#endif

        AW_MPI_VI_EnableVirChn(ViEisVencCfg.ViCfg[0].iViDev, ViEisVencCfg.ViCfg[0].iViChn);
        AW_MPI_VI_EnableVirChn(ViEisVencCfg.ViCfg[1].iViDev, ViEisVencCfg.ViCfg[1].iViChn);

        signal(SIGINT, SampleExitHandle);

        while (!g_bExitFlag) {
            VIDEO_FRAME_INFO_S stEisStabFrmTmp;
            //1. Get picture datas.
            //2. Process picture datas.
            //3. Release picture datas.
            //4. break.
            iRet = AW_MPI_EIS_GetData(ViEisVencCfg.EisCfg[0].iEisChn, &stEisStabFrmTmp, 500);
            if (SUCCESS == iRet) {
                iRet = AW_MPI_VENC_SendFrame(iVeChn, &stEisStabFrmTmp, 200);
                if (SUCCESS != iRet)
                    AW_MPI_EIS_ReleaseData(ViEisVencCfg.EisCfg[0].iEisChn, &stEisStabFrmTmp);

                g_iGetFrameCnt++;
                if (g_iGetFrameCnt >= ViEisVencCfg.stUsrCfg.iFrameCnt)
                    g_bExitFlag = 1;
            }
    //        usleep(10*1000);
        }

        pthread_join(ViEisVencCfg.pDispTrd, NULL);
        AW_MPI_VO_StopChn(ViEisVencCfg.VoCfg.iVoLyr, ViEisVencCfg.VoCfg.iVoChn);
        pthread_join(ViEisVencCfg.pEncTrd, NULL);

        /* The close sequence.
        * 1. Stop vi->eis->venc channel.
        * 2. Disbale vi->eis->venc channel.
        * 3. Destroy venc->eis->vi channel.  VVVVVVVVVVVVVVVVery important.
        */
        AW_MPI_VI_DisableVirChn(ViEisVencCfg.ViCfg[0].iViDev, ViEisVencCfg.ViCfg[0].iViChn);
        AW_MPI_EIS_StopChn(ViEisVencCfg.EisCfg[0].iEisChn);

        AW_MPI_VI_DisableVirChn(ViEisVencCfg.ViCfg[1].iViDev, ViEisVencCfg.ViCfg[1].iViChn);
        AW_MPI_EIS_StopChn(ViEisVencCfg.EisCfg[1].iEisChn);

        AW_MPI_VO_DisableChn(ViEisVencCfg.VoCfg.iVoLyr, ViEisVencCfg.VoCfg.iVoChn);
        AW_MPI_VENC_StopRecvPic(iVeChn);

        AW_MPI_ISP_Stop(0);
        AW_MPI_ISP_Exit();
        AW_MPI_EIS_DisableChn(ViEisVencCfg.EisCfg[0].iEisChn);
        AW_MPI_EIS_DisableChn(ViEisVencCfg.EisCfg[1].iEisChn);

        AW_MPI_VO_CloseVideoLayer(ViEisVencCfg.VoCfg.iVoLyr);
        AW_MPI_VO_DisableVideoLayer(ViEisVencCfg.VoCfg.iVoLyr);
        AW_MPI_VO_RemoveOutsideVideoLayer(HLAY(2,0));
        AW_MPI_VENC_ResetChn(iVeChn);
        AW_MPI_VENC_DestroyChn(iVeChn);

//        AW_MPI_SYS_UnBind(&stViComp, &stEisComp);

        AW_MPI_EIS_DestroyChn(ViEisVencCfg.EisCfg[0].iEisChn);
        SampleVirviEnd(ViEisVencCfg.ViCfg[0].iViDev, ViEisVencCfg.ViCfg[0].iViChn);
        SampleVippEnd(ViEisVencCfg.ViCfg[0].iViDev);

        AW_MPI_EIS_DestroyChn(ViEisVencCfg.EisCfg[1].iEisChn);
        SampleVirviEnd(ViEisVencCfg.ViCfg[1].iViDev, ViEisVencCfg.ViCfg[1].iViChn);
        SampleVippEnd(ViEisVencCfg.ViCfg[1].iViDev);

        AW_MPI_VO_Disable(ViEisVencCfg.VoCfg.iVoDev);
        fclose(ViEisVencCfg.VencCfg.pSaveFd);
        AW_MPI_SYS_Exit();
    }

    return 0;

    AW_MPI_EIS_StopChn(ViEisVencCfg.EisCfg[1].iEisChn);
EStrtEis1:
    AW_MPI_EIS_StopChn(ViEisVencCfg.EisCfg[0].iEisChn);
EStrtEis0:
EBindViEis1:
EBindViEis0:
    SampleVirviEnd(ViEisVencCfg.ViCfg[1].iViDev, ViEisVencCfg.ViCfg[1].iViChn);
EVirviCrt1:
    SampleVirviEnd(ViEisVencCfg.ViCfg[0].iViDev, ViEisVencCfg.ViCfg[0].iViChn);
EVirviCrt0:
    AW_MPI_ISP_Stop(0);
    AW_MPI_ISP_Exit();
    SampleVippEnd(ViEisVencCfg.ViCfg[1].iViDev);
EVippOpen1:
    SampleVippEnd(ViEisVencCfg.ViCfg[0].iViDev);
EVippOpen0:
    AW_MPI_EIS_DestroyChn(ViEisVencCfg.EisCfg[1].iEisChn);
EEnbEis1:
    AW_MPI_EIS_DestroyChn(ViEisVencCfg.EisCfg[0].iEisChn);
EEnbEis0:
    AW_MPI_EIS_DestroyChn(ViEisVencCfg.EisCfg[1].iEisChn);
ECrtEis1:
    AW_MPI_EIS_DestroyChn(ViEisVencCfg.EisCfg[0].iEisChn);    
ECrtEis0:
    g_bExitFlag = 1;
    pthread_join(ViEisVencCfg.pEncTrd, NULL);
ECrtTrd:
    AW_MPI_VENC_StartRecvPic(iVeChn);    
EStrtVenc:
    AW_MPI_VENC_DestroyChn(iVeChn);
ECrtVenc:
    fclose(ViEisVencCfg.VencCfg.pSaveFd);
EOSaveFile:
    AW_MPI_VO_StopChn(ViEisVencCfg.VoCfg.iVoLyr, ViEisVencCfg.VoCfg.iVoChn);
    AW_MPI_VO_DisableChn(ViEisVencCfg.VoCfg.iVoLyr, ViEisVencCfg.VoCfg.iVoChn);
    AW_MPI_VO_CloseVideoLayer(ViEisVencCfg.VoCfg.iVoLyr);
    AW_MPI_VO_DisableVideoLayer(ViEisVencCfg.VoCfg.iVoLyr);
EOVOLyr:
    AW_MPI_VO_CloseVideoLayer(HLAY(2,0)); /* close ui layer. */
    AW_MPI_VO_RemoveOutsideVideoLayer(HLAY(2,0));
    AW_MPI_VO_Disable(ViEisVencCfg.VoCfg.iVoDev);
EOVODev:
    return iRet;
}
