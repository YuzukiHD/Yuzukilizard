/******************************************************************************
  Copyright (C), 2001-2022, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_smartIPC_demo.c
  Version       : Initial Draft
  Author        : Allwinner
  Created       : 2022/5/12
  Last Modified :
  Description   : Demonstrate Smart IPC scenarios
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "sample_smartIPC_demo"

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
#include <sys/prctl.h>
#include <sys/time.h>

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include "media/mpi_venc.h"
#include "media/mpi_sys.h"
#include "mm_common.h"
#include "mm_comm_venc.h"
#include "mm_comm_rc.h"
#include <mpi_videoformat_conversion.h>
#include <SystemBase.h>
#include <confparser.h>
#include <utils/VIDEO_FRAME_INFO_S.h>

#include "sample_smartIPC_demo.h"
#include "sample_smartIPC_demo_config.h"

#include "rtsp_server.h"
#include "record.h"
#include "aiservice.h"
#include <awnn_det.h>

#define SUPPORT_RTSP_TEST
//#define SUPPORT_STREAM_OSD_TEST
//#define SAVE_ONE_FILE

static SampleSmartIPCDemoContext *gpSampleSmartIPCDemoContext = NULL;

static unsigned int getSysTickMs()
{
    unsigned int ms = 0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ms = tv.tv_sec*1000 + tv.tv_usec/1000;
    return ms;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleSmartIPCDemoContext)
    {
        cdx_sem_up(&gpSampleSmartIPCDemoContext->mSemExit);
    }
}

static int ParseCmdLine(int argc, char **argv, SampleSmartIPCDemoCmdLineParam *pCmdLinePara)
{
    alogd("path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleSmartIPCDemoCmdLineParam));
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
                "\t-path /home/sample_OnlineVenc.conf\n");
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

static PIXEL_FORMAT_E getPicFormatFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PIXEL_FORMAT_E PicFormat = MM_PIXEL_FORMAT_BUTT;
    char *pStrPixelFormat = (char*)GetConfParaString(pConfParser, key, NULL);

    if (!strcmp(pStrPixelFormat, "nv21"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "yv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "yu12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_2_0x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_2_5x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_1_5x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_1_0x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }

    return PicFormat;
}

static PAYLOAD_TYPE_E getEncoderTypeFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PAYLOAD_TYPE_E EncType = PT_BUTT;
    char *ptr = (char *)GetConfParaString(pConfParser, key, NULL);
    if(!strcmp(ptr, "H.264"))
    {
        EncType = PT_H264;
    }
    else if(!strcmp(ptr, "H.265"))
    {
        EncType = PT_H265;
    }
    else
    {
        aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
        EncType = PT_H264;
    }

    return EncType;
}

static ERRORTYPE loadSampleConfig(SampleSmartIPCDemoConfig *pConfig, const char *conf_path)
{
    if (NULL == pConfig)
    {
        aloge("fatal error, pConfig is NULL!");
        return FAILURE;
    }

    if (NULL != conf_path)
    {
        char *ptr = NULL;
        CONFPARSER_S stConfParser;
        int ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("fatal error, load conf failed!");
            return FAILURE;
        }

        // rtsp
        pConfig->mRtspNetType = GetConfParaInt(&stConfParser, CFG_RtspNetType, 0);
        pConfig->mRtspBufNum = GetConfParaInt(&stConfParser, CFG_RtspBufNum, 0);
        // common params
        pConfig->mProductMode = GetConfParaInt(&stConfParser, CFG_ProductMode, 0);
        // main stream
        pConfig->mMainEnable = GetConfParaInt(&stConfParser, CFG_MainEnable, 0);
        pConfig->mMainRtspID = GetConfParaInt(&stConfParser, CFG_MainRtspID, 0);
        pConfig->mMainIsp = GetConfParaInt(&stConfParser, CFG_MainIsp, 0);
        pConfig->mMainVipp = GetConfParaInt(&stConfParser, CFG_MainVipp, 0);
        pConfig->mMainViChn = GetConfParaInt(&stConfParser, CFG_MainViChn, 0);
        pConfig->mMainSrcWidth = GetConfParaInt(&stConfParser, CFG_MainSrcWidth, 0);
        pConfig->mMainSrcHeight = GetConfParaInt(&stConfParser, CFG_MainSrcHeight, 0);
        pConfig->mMainPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_MainPixelFormat);
        pConfig->mMainWdrEnable = GetConfParaInt(&stConfParser, CFG_MainWdrEnable, 0);
        pConfig->mMainViBufNum = GetConfParaInt(&stConfParser, CFG_MainViBufNum, 0);
        pConfig->mMainSrcFrameRate = GetConfParaInt(&stConfParser, CFG_MainSrcFrameRate, 0);
        pConfig->mMainVEncChn = GetConfParaInt(&stConfParser, CFG_MainVEncChn, 0);
        pConfig->mMainEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_MainEncodeType);
        pConfig->mMainEncodeWidth = GetConfParaInt(&stConfParser, CFG_MainEncodeWidth, 0);
        pConfig->mMainEncodeHeight = GetConfParaInt(&stConfParser, CFG_MainEncodeHeight, 0);
        pConfig->mMainEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_MainEncodeFrameRate, 0);
        pConfig->mMainEncodeBitrate = GetConfParaInt(&stConfParser, CFG_MainEncodeBitrate, 0);
        pConfig->mMainOnlineEnable = GetConfParaInt(&stConfParser, CFG_MainOnlineEnable, 0);
        pConfig->mMainOnlineShareBufNum = GetConfParaInt(&stConfParser, CFG_MainOnlineShareBufNum, 0);
        pConfig->mMainEncppEnable = GetConfParaInt(&stConfParser, CFG_MainEncppEnable, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainFilePath, NULL);
        strncpy(pConfig->mMainFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMainSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_MainSaveOneFileDuration, 0);
        pConfig->mMainSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_MainSaveMaxFileCnt, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainDrawOSDText, NULL);
        strncpy(pConfig->mMainDrawOSDText, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMainNnNbgType = GetConfParaInt(&stConfParser, CFG_MainNnNbgType, 0);
        pConfig->mMainNnIsp = GetConfParaInt(&stConfParser, CFG_MainNnIsp, 0);
        pConfig->mMainNnVipp = GetConfParaInt(&stConfParser, CFG_MainNnVipp, 0);
        pConfig->mMainNnViBufNum = GetConfParaInt(&stConfParser, CFG_MainNnViBufNum, 0);
        pConfig->mMainNnSrcFrameRate = GetConfParaInt(&stConfParser, CFG_MainNnSrcFrameRate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainNnNbgFilePath, NULL);
        strncpy(pConfig->mMainNnNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMainNnDrawOrlEnable = GetConfParaInt(&stConfParser, CFG_MainNnDrawOrlEnable, 0);
        // sub stream
        pConfig->mSubEnable = GetConfParaInt(&stConfParser, CFG_SubEnable, 0);
        pConfig->mSubRtspID = GetConfParaInt(&stConfParser, CFG_SubRtspID, 0);
        pConfig->mSubIsp = GetConfParaInt(&stConfParser, CFG_SubIsp, 0);
        pConfig->mSubVipp = GetConfParaInt(&stConfParser, CFG_SubVipp, 0);
        pConfig->mSubSrcWidth = GetConfParaInt(&stConfParser, CFG_SubSrcWidth, 0);
        pConfig->mSubSrcHeight = GetConfParaInt(&stConfParser, CFG_SubSrcHeight, 0);
        pConfig->mSubPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_SubPixelFormat);
        pConfig->mSubWdrEnable = GetConfParaInt(&stConfParser, CFG_SubWdrEnable, 0);
        pConfig->mSubViBufNum = GetConfParaInt(&stConfParser, CFG_SubViBufNum, 0);
        pConfig->mSubSrcFrameRate = GetConfParaInt(&stConfParser, CFG_SubSrcFrameRate, 0);
        pConfig->mSubViChn = GetConfParaInt(&stConfParser, CFG_SubViChn, 0);
        pConfig->mSubVEncChn = GetConfParaInt(&stConfParser, CFG_SubVEncChn, 0);
        pConfig->mSubEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_SubEncodeType);
        pConfig->mSubEncodeWidth = GetConfParaInt(&stConfParser, CFG_SubEncodeWidth, 0);
        pConfig->mSubEncodeHeight = GetConfParaInt(&stConfParser, CFG_SubEncodeHeight, 0);
        pConfig->mSubEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_SubEncodeFrameRate, 0);
        pConfig->mSubEncodeBitrate = GetConfParaInt(&stConfParser, CFG_SubEncodeBitrate, 0);
        pConfig->mSubEncppSharpAttenCoefPer = 100 * pConfig->mSubEncodeWidth / pConfig->mMainEncodeWidth;
        pConfig->mSubEncppEnable = GetConfParaInt(&stConfParser, CFG_SubEncppEnable, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubFilePath, NULL);
        strncpy(pConfig->mSubFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_SubSaveOneFileDuration, 0);
        pConfig->mSubSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_SubSaveMaxFileCnt, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubDrawOSDText, NULL);
        strncpy(pConfig->mSubDrawOSDText, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubNnNbgType = GetConfParaInt(&stConfParser, CFG_SubNnNbgType, 0);
        pConfig->mSubNnIsp = GetConfParaInt(&stConfParser, CFG_SubNnIsp, 0);
        pConfig->mSubNnVipp = GetConfParaInt(&stConfParser, CFG_SubNnVipp, 0);
        pConfig->mSubNnViBufNum = GetConfParaInt(&stConfParser, CFG_SubNnViBufNum, 0);
        pConfig->mSubNnSrcFrameRate = GetConfParaInt(&stConfParser, CFG_SubNnSrcFrameRate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubNnNbgFilePath, NULL);
        strncpy(pConfig->mSubNnNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubNnDrawOrlEnable = GetConfParaInt(&stConfParser, CFG_SubNnDrawOrlEnable, 0);
        pConfig->mSubVippCropEnable = GetConfParaInt(&stConfParser, CFG_SubVippCropEnable, 0);
        pConfig->mSubVippCropRectX = GetConfParaInt(&stConfParser, CFG_SubVippCropRectX, 0);
        pConfig->mSubVippCropRectY = GetConfParaInt(&stConfParser, CFG_SubVippCropRectY, 0);
        pConfig->mSubVippCropRectWidth = GetConfParaInt(&stConfParser, CFG_SubVippCropRectWidth, 0);
        pConfig->mSubVippCropRectHeight = GetConfParaInt(&stConfParser, CFG_SubVippCropRectHeight, 0);
        // isp and ve linkage
        pConfig->mIspAndVeLinkageEnable = GetConfParaInt(&stConfParser, CFG_IspAndVeLinkageEnable, 0);
        pConfig->mIspAndVeLinkageStreamChn = GetConfParaInt(&stConfParser, CFG_IspAndVeLinkageStreamChn, 0);
        alogd("IspAndVeLinkage config: Enable=%d, StreamChn=%d", pConfig->mIspAndVeLinkageEnable, pConfig->mIspAndVeLinkageStreamChn);
        // wb yuv
        pConfig->mWbYuvEnable = GetConfParaInt(&stConfParser, CFG_WbYuvEnable, 0);
        pConfig->mWbYuvBufNum = GetConfParaInt(&stConfParser, CFG_WbYuvBufNum, 0);
        pConfig->mWbYuvStartIndex = GetConfParaInt(&stConfParser, CFG_WbYuvStartIndex, 0);
        pConfig->mWbYuvTotalCnt = GetConfParaInt(&stConfParser, CFG_WbYuvTotalCnt, 0);
        pConfig->mWbYuvStreamChn = GetConfParaInt(&stConfParser, CFG_WbYuvStreamChannel, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_WbYuvFilePath, NULL);
        strncpy(pConfig->mWbYuvFilePath, ptr, MAX_FILE_PATH_SIZE);
        // others
        pConfig->mTestDuration = GetConfParaInt(&stConfParser, CFG_TestDuration, 0);

        destroyConfParser(&stConfParser);
    }
    else
    {
        alogw("user not set config file, use default configs.");
    }

    if (-1 != pConfig->mMainRtspID && pConfig->mMainRtspID == pConfig->mSubRtspID)
    {
        aloge("fatal error, same MainRtspID:%d, SubRtspID:%d", pConfig->mMainRtspID, pConfig->mSubRtspID);
        return FAILURE;
    }
    
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        VencStreamContext *pStreamContext = NULL;
        switch(mVEncChn)
        {
            case VENC_CHN_MAIN_STREAM:
            {
                pStreamContext = &pContext->mMainStream;
                break;
            }
            case VENC_CHN_SUB_STREAM:
            {
                pStreamContext = &pContext->mSubStream;
                break;
            }
            default:
            {
                aloge("fatal error! invalid venc chn %d", mVEncChn);
                return FAILURE;
            }
        }

        switch(event)
        {
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                VencIsp2VeParam *pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                if (pIsp2VeParam)
                {
                    sEncppSharpParam *pSharpParam = &pIsp2VeParam->mSharpParam;
                    ISP_DEV mIspDev = 0;
                    ret = AW_MPI_VI_GetIspDev(pStreamContext->mVipp, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pStreamContext->mVipp, ret);
                        return -1;
                    }

                    struct enc_VencIsp2VeParam mIsp2VeParam;
                    memset(&mIsp2VeParam, 0, sizeof(struct enc_VencIsp2VeParam));
                    ret = AW_MPI_ISP_GetIsp2VeParam(mIspDev, &mIsp2VeParam);
                    if (ret)
                    {
                        aloge("fatal error, isp[%d] GetIsp2VeParam failed! ret=%d", mIspDev, ret);
                        return -1;
                    }

                    if (mIsp2VeParam.encpp_en)
                    {
                        VENC_CHN_ATTR_S stVencAttr;
                        memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
                        AW_MPI_VENC_GetChnAttr(mVEncChn, &stVencAttr);
                        if (FALSE == stVencAttr.EncppAttr.mbEncppEnable)
                        {
                            stVencAttr.EncppAttr.mbEncppEnable = TRUE;
                            AW_MPI_VENC_SetChnAttr(mVEncChn, &stVencAttr);
                        }
                        if (VENC_CHN_MAIN_STREAM != mVEncChn)
                        {
                            mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren * pStreamContext->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren * pStreamContext->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren * pStreamContext->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren * pStreamContext->mEncppSharpAttenCoefPer / 100;
                        }
                        memcpy(&pSharpParam->mDynamicParam, &mIsp2VeParam.mDynamicSharpCfg,sizeof(sEncppSharpParamDynamic));
                        memcpy(&pSharpParam->mStaticParam, &mIsp2VeParam.mStaticSharpCfg, sizeof(sEncppSharpParamStatic));
                    }
                    else
                    {
                        VENC_CHN_ATTR_S stVencAttr;
                        memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
                        AW_MPI_VENC_GetChnAttr(mVEncChn, &stVencAttr);
                        if (TRUE == stVencAttr.EncppAttr.mbEncppEnable)
                        {
                            stVencAttr.EncppAttr.mbEncppEnable = FALSE;
                            AW_MPI_VENC_SetChnAttr(mVEncChn, &stVencAttr);
                        }
                    }

                    pIsp2VeParam->mEnvLv = AW_MPI_ISP_GetEnvLV(mIspDev);
                    pIsp2VeParam->mAeWeightLum = AW_MPI_ISP_GetAeWeightLum(mIspDev);
                    pIsp2VeParam->mEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                }
                break;
            }
            case MPP_EVENT_LINKAGE_VE2ISP_PARAM:
            {
                if (mVEncChn == pContext->mConfigPara.mIspAndVeLinkageStreamChn)
                {
                    VencVe2IspParam *pVe2IspParam = (VencVe2IspParam *)pEventData;
                    if (pVe2IspParam && pContext->mConfigPara.mIspAndVeLinkageEnable)
                    {
                        ISP_DEV mIspDev = 0;
                        ret = AW_MPI_VI_GetIspDev(pStreamContext->mVipp, &mIspDev);
                        if (ret)
                        {
                            aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pStreamContext->mVipp, ret);
                            return -1;
                        }
                        alogv("update Ve2IspParam, route Isp[%d]-Vipp[%d]-VencChn[%d]", mIspDev, pStreamContext->mVipp, pStreamContext->mVEncChn);

                        struct enc_VencVe2IspParam mIspParam;
                        memcpy(&mIspParam.mMovingLevelInfo, &pVe2IspParam->mMovingLevelInfo, sizeof(MovingLevelInfo));
                        ret = AW_MPI_ISP_SetVe2IspParam(mIspDev, &mIspParam);
                        if (ret)
                        {
                            aloge("fatal error, isp[%d] SetVe2IspParam failed! ret=%d", mIspDev, ret);
                            return -1;
                        }
                        alogv("isp[%d] d2d_level=%d, d3d_level=%d", mIspDev, pVe2IspParam->d2d_level, pVe2IspParam->d3d_level);
                        ret = AW_MPI_ISP_SetNRAttr(mIspDev, pVe2IspParam->d2d_level);
                        if (ret)
                        {
                            aloge("fatal error, isp[%d] SetNRAttr failed! ret=%d", mIspDev, ret);
                            return -1;
                        }
                        ret = AW_MPI_ISP_Set3NRAttr(mIspDev, pVe2IspParam->d3d_level);
                        if (ret)
                        {
                            aloge("fatal error, isp[%d] Set3NRAttr failed! ret=%d", mIspDev, ret);
                            return -1;
                        }
                    }
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }
    return SUCCESS;
}

static void configBitsClipParam(VencTargetBitsClipParam *pBitsClipParam)
{
    pBitsClipParam->dis_default_para = 1;
    pBitsClipParam->mode = 1;
    pBitsClipParam->coef_th[0][0] = -0.5;
    pBitsClipParam->coef_th[0][1] =  0.5;
    pBitsClipParam->coef_th[1][0] = -0.3;
    pBitsClipParam->coef_th[1][1] =  0.3;
    pBitsClipParam->coef_th[2][0] = -0.3;
    pBitsClipParam->coef_th[2][1] =  0.3;
    pBitsClipParam->coef_th[3][0] = -0.5;
    pBitsClipParam->coef_th[3][1] =  0.5;
    pBitsClipParam->coef_th[4][0] =  0.4;
    pBitsClipParam->coef_th[4][1] =  0.7;

    alogd("BitsClipParam: %d %d {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}",
        pBitsClipParam->dis_default_para,
        pBitsClipParam->mode,
        pBitsClipParam->coef_th[0][0],
        pBitsClipParam->coef_th[0][1],
        pBitsClipParam->coef_th[1][0],
        pBitsClipParam->coef_th[1][1],
        pBitsClipParam->coef_th[2][0],
        pBitsClipParam->coef_th[2][1],
        pBitsClipParam->coef_th[3][0],
        pBitsClipParam->coef_th[3][1],
        pBitsClipParam->coef_th[4][0],
        pBitsClipParam->coef_th[4][1]);
}

static void configAeDiffParam(VencAeDiffParam *pAeDiffParam)
{
    pAeDiffParam->dis_default_para = 1;
    pAeDiffParam->diff_frames_th = 40;
    pAeDiffParam->stable_frames_th[0] = 5;
    pAeDiffParam->stable_frames_th[1] = 100;
    pAeDiffParam->diff_th[0] = 0.1;
    pAeDiffParam->diff_th[1] = 0.6;
    pAeDiffParam->small_diff_qp[0] = 20;
    pAeDiffParam->small_diff_qp[1] = 25;
    pAeDiffParam->large_diff_qp[0] = 35;
    pAeDiffParam->large_diff_qp[1] = 50;

    alogd("AeDiffParam: %d %d [%d,%d] [%.2f,%.2f], [%d,%d], [%d,%d]",
        pAeDiffParam->dis_default_para,
        pAeDiffParam->diff_frames_th,
        pAeDiffParam->stable_frames_th[0],
        pAeDiffParam->stable_frames_th[1],
        pAeDiffParam->diff_th[0],
        pAeDiffParam->diff_th[1],
        pAeDiffParam->small_diff_qp[0],
        pAeDiffParam->small_diff_qp[1],
        pAeDiffParam->large_diff_qp[0],
        pAeDiffParam->large_diff_qp[1]);
}

static void configMainStream(SampleSmartIPCDemoContext *pContext)
{
    pContext->mMainStream.priv = (void*)pContext;
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;

    pContext->mMainStream.mIsp = pContext->mConfigPara.mMainIsp;
    pContext->mMainStream.mVipp = pContext->mConfigPara.mMainVipp;
    if (pContext->mConfigPara.mMainOnlineEnable && (0 != pContext->mMainStream.mVipp))
    {
        aloge("fatal error! main vipp %d is wrong, only vipp0 support online.", pContext->mMainStream.mVipp);
    }

    pContext->mMainStream.mViChn = pContext->mConfigPara.mMainViChn; //0;
    pContext->mMainStream.mVEncChn = pContext->mConfigPara.mMainVEncChn; //0;

    if (pContext->mConfigPara.mMainOnlineEnable)
    {
        pContext->mMainStream.mViAttr.mOnlineEnable = 1;
        pContext->mMainStream.mViAttr.mOnlineShareBufNum = pContext->mConfigPara.mMainOnlineShareBufNum;//BK_TWO_BUFFER;
        pContext->mMainStream.mVEncChnAttr.VeAttr.mOnlineEnable = 1;
        pContext->mMainStream.mVEncChnAttr.VeAttr.mOnlineShareBufNum = pContext->mConfigPara.mMainOnlineShareBufNum;//BK_TWO_BUFFER;
    }
    alogd("main vipp%d ve_online_en:%d, dma_buf_num:%d, venc ch%d OnlineEnable:%d, OnlineShareBufNum:%d",
        pContext->mMainStream.mVipp, pContext->mMainStream.mViAttr.mOnlineEnable, pContext->mMainStream.mViAttr.mOnlineShareBufNum,
        pContext->mMainStream.mVEncChn, pContext->mMainStream.mVEncChnAttr.VeAttr.mOnlineEnable, pContext->mMainStream.mVEncChnAttr.VeAttr.mOnlineShareBufNum);

    pContext->mMainStream.mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mMainStream.mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mMainStream.mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mMainPixelFormat);
    pContext->mMainStream.mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mMainStream.mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pContext->mMainStream.mViAttr.format.width = pContext->mConfigPara.mMainSrcWidth;
    pContext->mMainStream.mViAttr.format.height = pContext->mConfigPara.mMainSrcHeight;
    pContext->mMainStream.mViAttr.fps = pContext->mConfigPara.mMainSrcFrameRate;
    pContext->mMainStream.mViAttr.use_current_win = 0;
    pContext->mMainStream.mViAttr.nbufs = pContext->mConfigPara.mMainViBufNum;
    pContext->mMainStream.mViAttr.nplanes = 2;
    pContext->mMainStream.mViAttr.wdr_mode = pContext->mConfigPara.mMainWdrEnable;
    pContext->mMainStream.mViAttr.drop_frame_num = 10;

    pContext->mMainStream.mVEncChnAttr.VeAttr.Type = pContext->mConfigPara.mMainEncodeType;
    pContext->mMainStream.mVEncChnAttr.VeAttr.MaxKeyInterval = 100;//40;
    pContext->mMainStream.mVEncChnAttr.VeAttr.SrcPicWidth = pContext->mConfigPara.mMainSrcWidth;
    pContext->mMainStream.mVEncChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.mMainSrcHeight;
    pContext->mMainStream.mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mMainStream.mVEncChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.mMainPixelFormat;
    pContext->mMainStream.mVEncChnAttr.VeAttr.mColorSpace = pContext->mMainStream.mViAttr.format.colorspace;
    pContext->mMainStream.mVEncRcParam.product_mode = pContext->mConfigPara.mProductMode;
    pContext->mMainStream.mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pContext->mMainStream.mEncppSharpAttenCoefPer = 100;
    alogd("main EncppSharpAttenCoefPer: %d%%", pContext->mMainStream.mEncppSharpAttenCoefPer);

    pContext->mMainStream.mViAttr.mbEncppEnable = pContext->mConfigPara.mMainEncppEnable;
    pContext->mMainStream.mVEncChnAttr.EncppAttr.mbEncppEnable = pContext->mConfigPara.mMainEncppEnable;

    if (pContext->mConfigPara.mMainSrcFrameRate)
    {
        vbvThreshSize = pContext->mConfigPara.mMainEncodeBitrate/8/pContext->mConfigPara.mMainSrcFrameRate*15;
    }
    vbvBufSize = pContext->mConfigPara.mMainEncodeBitrate/8*4 + vbvThreshSize;
    alogd("main vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    if(PT_H264 == pContext->mMainStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.PicWidth = pContext->mConfigPara.mMainEncodeWidth;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.mMainEncodeHeight;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.mLevel = 0;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.mThreshSize = vbvThreshSize;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.BufSize = vbvBufSize;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mMainEncodeBitrate;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        if (pContext->mMainStream.mVEncChnAttr.RcAttr.mRcMode == VENC_RC_MODE_H264VBR)
        {
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMaxQp = 50;//45;
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMinQp = 10;
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMaxPqp = 50;//45;
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMinPqp = 10;
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mQpInit = 37;
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMovingTh = 20;
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mQuality = 10;
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
            pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
        }
        else if (pContext->mMainStream.mVEncChnAttr.RcAttr.mRcMode == VENC_RC_MODE_H264CBR)
        {
            pContext->mMainStream.mVEncRcParam.ParamH264Cbr.mMaxQp = 50;
            pContext->mMainStream.mVEncRcParam.ParamH264Cbr.mMinQp = 10;
            pContext->mMainStream.mVEncRcParam.ParamH264Cbr.mMaxPqp = 50;
            pContext->mMainStream.mVEncRcParam.ParamH264Cbr.mMinPqp = 10;
            pContext->mMainStream.mVEncRcParam.ParamH264Cbr.mQpInit = 37;
            pContext->mMainStream.mVEncRcParam.ParamH264Cbr.mbEnMbQpLimit = 0;
        }
    }
    else if(PT_H265 == pContext->mMainStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.mMainEncodeWidth;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.mMainEncodeHeight;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mLevel = 0;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mThreshSize = vbvThreshSize;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mBufSize = vbvBufSize;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mMainEncodeBitrate;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMaxQp = 50;//45;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMinQp = 10;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMaxPqp = 50;//45;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMinPqp = 10;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mQpInit = 37;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mQuality = 10;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }
    else if(PT_MJPEG == pContext->mMainStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth= pContext->mConfigPara.mMainEncodeWidth;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.mMainEncodeHeight;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrMjpeg.mThreshSize = vbvThreshSize;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrMjpeg.mBufSize = vbvBufSize;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mMainEncodeBitrate;
    }
    pContext->mMainStream.mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->mMainStream.mVEncChnAttr.GopAttr.mGopSize = 2;
    pContext->mMainStream.mVEncFrameRateConfig.SrcFrmRate = pContext->mConfigPara.mMainSrcFrameRate;
    pContext->mMainStream.mVEncFrameRateConfig.DstFrmRate = pContext->mConfigPara.mMainEncodeFrameRate;

    configBitsClipParam(&pContext->mMainStream.mVEncRcParam.mBitsClipParam);
    configAeDiffParam(&pContext->mMainStream.mVEncRcParam.mAeDiffParam);
}

static void configSubStream(SampleSmartIPCDemoContext *pContext)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;

    pContext->mSubStream.priv = (void*)pContext;    
    pContext->mSubStream.mIsp = pContext->mConfigPara.mSubIsp;
    pContext->mSubStream.mVipp = pContext->mConfigPara.mSubVipp;
    pContext->mSubStream.mViChn = pContext->mConfigPara.mSubViChn;
    pContext->mSubStream.mVEncChn = pContext->mConfigPara.mSubVEncChn;
    pContext->mSubStream.mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mSubStream.mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mSubStream.mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mSubPixelFormat);
    pContext->mSubStream.mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mSubStream.mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pContext->mSubStream.mViAttr.format.width = pContext->mConfigPara.mSubSrcWidth;
    pContext->mSubStream.mViAttr.format.height = pContext->mConfigPara.mSubSrcHeight;
    pContext->mSubStream.mViAttr.fps = pContext->mConfigPara.mSubSrcFrameRate;
    pContext->mSubStream.mViAttr.use_current_win = 1;
    pContext->mSubStream.mViAttr.nbufs = pContext->mConfigPara.mSubViBufNum;
    pContext->mSubStream.mViAttr.nplanes = 2;
    pContext->mSubStream.mViAttr.wdr_mode = pContext->mConfigPara.mSubWdrEnable;
    pContext->mSubStream.mViAttr.drop_frame_num = 10;

    /* config vipp crop */
    pContext->mSubStream.mViAttr.mCropCfg.bEnable = pContext->mConfigPara.mSubVippCropEnable;
    pContext->mSubStream.mViAttr.mCropCfg.Rect.X = pContext->mConfigPara.mSubVippCropRectX;
    pContext->mSubStream.mViAttr.mCropCfg.Rect.Y = pContext->mConfigPara.mSubVippCropRectY;
    pContext->mSubStream.mViAttr.mCropCfg.Rect.Width = pContext->mConfigPara.mSubVippCropRectWidth;
    pContext->mSubStream.mViAttr.mCropCfg.Rect.Height = pContext->mConfigPara.mSubVippCropRectHeight;
    alogd("vipp[%d] crop en:%d X:%d Y:%d W:%d H:%d", pContext->mSubStream.mVipp,
        pContext->mConfigPara.mSubVippCropEnable,
        pContext->mConfigPara.mSubVippCropRectX,
        pContext->mConfigPara.mSubVippCropRectY,
        pContext->mConfigPara.mSubVippCropRectWidth,
        pContext->mConfigPara.mSubVippCropRectHeight);

    pContext->mSubStream.mVEncChnAttr.VeAttr.Type = pContext->mConfigPara.mSubEncodeType;
    pContext->mSubStream.mVEncChnAttr.VeAttr.MaxKeyInterval = 100;//40;
    pContext->mSubStream.mVEncChnAttr.VeAttr.SrcPicWidth = pContext->mConfigPara.mSubSrcWidth;
    pContext->mSubStream.mVEncChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.mSubSrcHeight;
    pContext->mSubStream.mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mSubStream.mVEncChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.mSubPixelFormat;
    pContext->mSubStream.mVEncChnAttr.VeAttr.mColorSpace = pContext->mSubStream.mViAttr.format.colorspace;
    pContext->mSubStream.mVEncRcParam.product_mode = pContext->mConfigPara.mProductMode;
    pContext->mSubStream.mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pContext->mSubStream.mEncppSharpAttenCoefPer = pContext->mConfigPara.mSubEncppSharpAttenCoefPer;
    alogd("sub EncppSharpAttenCoefPer: %d%%", pContext->mSubStream.mEncppSharpAttenCoefPer);

    pContext->mSubStream.mViAttr.mbEncppEnable = pContext->mConfigPara.mSubEncppEnable;
    pContext->mSubStream.mVEncChnAttr.EncppAttr.mbEncppEnable = pContext->mConfigPara.mSubEncppEnable;

    if (pContext->mConfigPara.mSubSrcFrameRate)
    {
        vbvThreshSize = pContext->mConfigPara.mSubEncodeBitrate/8/pContext->mConfigPara.mSubSrcFrameRate*15;
    }
    vbvBufSize = pContext->mConfigPara.mSubEncodeBitrate/8*4 + vbvThreshSize;
    alogd("sub vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    if(PT_H264 == pContext->mSubStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.PicWidth = pContext->mConfigPara.mSubEncodeWidth;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.mSubEncodeHeight;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.mLevel = 0;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.mThreshSize = vbvThreshSize;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.BufSize = vbvBufSize;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mSubEncodeBitrate;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        if (pContext->mSubStream.mVEncChnAttr.RcAttr.mRcMode == VENC_RC_MODE_H264VBR)
        {
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMaxQp = 50;//45;
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMinQp = 10;
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMaxPqp = 50;//45;
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMinPqp = 10;
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mQpInit = 37;
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMovingTh = 20;
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mQuality = 10;
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
            pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
        }
        else if (pContext->mSubStream.mVEncChnAttr.RcAttr.mRcMode == VENC_RC_MODE_H264CBR)
        {
            pContext->mSubStream.mVEncRcParam.ParamH264Cbr.mMaxQp = 50;
            pContext->mSubStream.mVEncRcParam.ParamH264Cbr.mMinQp = 10;
            pContext->mSubStream.mVEncRcParam.ParamH264Cbr.mMaxPqp = 50;
            pContext->mSubStream.mVEncRcParam.ParamH264Cbr.mMinPqp = 10;
            pContext->mSubStream.mVEncRcParam.ParamH264Cbr.mQpInit = 37;
            pContext->mSubStream.mVEncRcParam.ParamH264Cbr.mbEnMbQpLimit = 0;
        }
    }
    else if(PT_H265 == pContext->mSubStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.mSubEncodeWidth;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.mSubEncodeHeight;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mLevel = 0;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mThreshSize = vbvThreshSize;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mBufSize = vbvBufSize;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mSubEncodeBitrate;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMaxQp = 50;//45;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMinQp = 10;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMaxPqp = 50;//45;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMinPqp = 10;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mQpInit = 37;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mQuality = 10;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }
    else if(PT_MJPEG == pContext->mSubStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth= pContext->mConfigPara.mSubEncodeWidth;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.mSubEncodeHeight;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrMjpeg.mThreshSize = vbvThreshSize;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrMjpeg.mBufSize = vbvBufSize;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mSubEncodeBitrate;
    }
    pContext->mSubStream.mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->mSubStream.mVEncChnAttr.GopAttr.mGopSize = 2;
    pContext->mSubStream.mVEncFrameRateConfig.SrcFrmRate = pContext->mConfigPara.mSubSrcFrameRate;
    pContext->mSubStream.mVEncFrameRateConfig.DstFrmRate = pContext->mConfigPara.mSubEncodeFrameRate;

    configBitsClipParam(&pContext->mSubStream.mVEncRcParam.mBitsClipParam);
    configAeDiffParam(&pContext->mSubStream.mVEncRcParam.mAeDiffParam);
}

static void configAiService(SampleSmartIPCDemoConfig *pConfig, ai_service_attr_t *pNnAttr)
{
    // main
    if (pConfig->mMainEnable)
    {
        pNnAttr->ch_info[0].ch_idx = 0;
        if (0 == pConfig->mMainNnNbgType)
        {
            pNnAttr->ch_info[0].nbg_type = AWNN_HUMAN_DET;
            pNnAttr->ch_info[0].src_width = NN_HUMAN_SRC_WIDTH;
            pNnAttr->ch_info[0].src_height = NN_HUMAN_SRC_HEIGHT;
        }
        else if (1 == pConfig->mMainNnNbgType)
        {
            pNnAttr->ch_info[0].nbg_type = AWNN_FACE_DET;
            pNnAttr->ch_info[0].src_width = NN_FACE_SRC_WIDTH;
            pNnAttr->ch_info[0].src_height = NN_FACE_SRC_HEIGHT;
        }
        else
        {
            pNnAttr->ch_info[0].nbg_type = AWNN_TYPE_MAX;
        }
        pNnAttr->ch_info[0].isp = pConfig->mMainNnIsp;
        pNnAttr->ch_info[0].vipp = pConfig->mMainNnVipp;
        pNnAttr->ch_info[0].viChn = 0;
        pNnAttr->ch_info[0].pixel_format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pNnAttr->ch_info[0].vi_buf_num = pConfig->mMainNnViBufNum;
        pNnAttr->ch_info[0].src_frame_rate = pConfig->mMainNnSrcFrameRate;
        strncpy(pNnAttr->ch_info[0].model_file, pConfig->mMainNnNbgFilePath, NN_NBG_MAX_FILE_PATH_SIZE);
        pNnAttr->ch_info[0].draw_orl_enable = pConfig->mMainNnDrawOrlEnable;
        pNnAttr->ch_info[0].draw_orl_vipp = pConfig->mMainVipp;
        pNnAttr->ch_info[0].draw_orl_src_width = pConfig->mMainSrcWidth;
        pNnAttr->ch_info[0].draw_orl_src_height = pConfig->mMainSrcHeight;
        pNnAttr->ch_info[0].region_hdl_base = 10;
    }
    else
    {
        pNnAttr->ch_info[0].ch_idx = 0;
        pNnAttr->ch_info[0].nbg_type = AWNN_TYPE_MAX;
    }
    // sub
    if (pConfig->mSubEnable)
    {
        pNnAttr->ch_info[1].ch_idx = 1;
        if (0 == pConfig->mSubNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_HUMAN_DET;
            pNnAttr->ch_info[1].src_width = NN_HUMAN_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_HUMAN_SRC_HEIGHT;
        }
        else if (1 == pConfig->mSubNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_FACE_DET;
            pNnAttr->ch_info[1].src_width = NN_FACE_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_FACE_SRC_HEIGHT;
        }
        else
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_TYPE_MAX;
        }
        pNnAttr->ch_info[1].isp = pConfig->mSubNnIsp;
        pNnAttr->ch_info[1].vipp = pConfig->mSubNnVipp;
        pNnAttr->ch_info[1].viChn = 0;
        pNnAttr->ch_info[1].pixel_format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pNnAttr->ch_info[1].vi_buf_num = pConfig->mSubNnViBufNum;
        pNnAttr->ch_info[1].src_frame_rate = pConfig->mSubNnSrcFrameRate;
        strncpy(pNnAttr->ch_info[1].model_file, pConfig->mSubNnNbgFilePath, NN_NBG_MAX_FILE_PATH_SIZE);
        pNnAttr->ch_info[1].draw_orl_enable = pConfig->mSubNnDrawOrlEnable;
        pNnAttr->ch_info[1].draw_orl_vipp = pConfig->mSubVipp;
        pNnAttr->ch_info[1].draw_orl_src_width = pConfig->mSubSrcWidth;
        pNnAttr->ch_info[1].draw_orl_src_height = pConfig->mSubSrcHeight;
        pNnAttr->ch_info[1].region_hdl_base = 20;
    }
    else
    {
        pNnAttr->ch_info[1].ch_idx = 1;
        pNnAttr->ch_info[1].nbg_type = AWNN_TYPE_MAX;
    }
}

static void* getWbYuvThread(void *pThreadData)
{
    int result = 0;
    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)pThreadData;
    VencStreamContext *pStreamContext = NULL;

    if (VENC_CHN_MAIN_STREAM == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mMainStream;
    }
    else if (VENC_CHN_SUB_STREAM == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mSubStream;
    }
    else
    {
        aloge("fatal error, WbYuvStreamChn:%d is not support for wb yuv!", pContext->mConfigPara.mWbYuvStreamChn);
        return NULL;
    }

    FILE* fp_wb_yuv = fopen(pContext->mConfigPara.mWbYuvFilePath, "wb");
    if (NULL == fp_wb_yuv)
    {
        aloge("fatal error! why open file[%s] fail?", pContext->mConfigPara.mWbYuvFilePath);
        return NULL;
    }

    unsigned int wbYuvCnt = 0;
    unsigned int preCnt = 0;
    unsigned mHadEnableWbYuv = 0;
    unsigned int wb_wdith  = 0;
    unsigned int wb_height = 0;

    if (PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        wb_wdith  = pStreamContext->mVEncChnAttr.VeAttr.AttrH264e.PicWidth;
        wb_height = pStreamContext->mVEncChnAttr.VeAttr.AttrH264e.PicHeight;
    }
    else if (PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        wb_wdith  = pStreamContext->mVEncChnAttr.VeAttr.AttrH265e.mPicWidth;
        wb_height = pStreamContext->mVEncChnAttr.VeAttr.AttrH265e.mPicHeight;
    }
    else
    {
        aloge("fatal error, Type:%d is not support for wb yuv!", pStreamContext->mVEncChnAttr.VeAttr.Type);
        if (fp_wb_yuv)
        {
            fclose(fp_wb_yuv);
            fp_wb_yuv = NULL;
        }
        return NULL;
    }
    wb_wdith  = ALIGN(wb_wdith, 16);
    wb_height = ALIGN(wb_height, 16);

    while (!pContext->mbExitFlag && wbYuvCnt < pContext->mConfigPara.mWbYuvTotalCnt)
    {
        if (pStreamContext->mStreamDataCnt < pContext->mConfigPara.mWbYuvStartIndex)
        {
            usleep(10*1000);
            continue;
        }
        else
        {
            if (mHadEnableWbYuv == 0)
            {
                mHadEnableWbYuv = 1;
                sWbYuvParam mWbYuvParam;
                memset(&mWbYuvParam, 0, sizeof(sWbYuvParam));
                mWbYuvParam.bEnableWbYuv = 1;
                mWbYuvParam.nWbBufferNum = pContext->mConfigPara.mWbYuvBufNum;
                mWbYuvParam.scalerRatio  = VENC_ISP_SCALER_0;
                result = AW_MPI_VENC_SetWbYuv(pStreamContext->mVEncChn, &mWbYuvParam);
                if (result)
                {
                    aloge("fatal error, VencChn[%d] SetWbYuv failed! ret:%d", pStreamContext->mVEncChn, result);
                }
            }
        }

        if (fp_wb_yuv)
        {
            VencThumbInfo mThumbInfo;
            memset(&mThumbInfo, 0, sizeof(VencThumbInfo));
            mThumbInfo.bWriteToFile = 1;
            mThumbInfo.fp = fp_wb_yuv;
            int startTime = getSysTickMs();
            result = AW_MPI_VENC_GetWbYuv(pStreamContext->mVEncChn, &mThumbInfo);
            int endTime = getSysTickMs();
            if (result == 0)
            {
                wbYuvCnt++;
                alogd("get wb yuv[%dx%d], curWbCnt = %d, saveTotalCnt = %d, time = %d ms, encodeCnt = %d, diffCnt = %d",
                    wb_wdith, wb_height, wbYuvCnt, pContext->mConfigPara.mWbYuvTotalCnt,
                    endTime - startTime, mThumbInfo.nEncodeCnt, (mThumbInfo.nEncodeCnt - preCnt));

                preCnt = mThumbInfo.nEncodeCnt;
            }
            else
            {
                usleep(10*1000);
            }
        }
        else
        {
            alogw("exit thread: fp_wb_yuv = %p", fp_wb_yuv);
        }
    }

    if (fp_wb_yuv)
    {
        fclose(fp_wb_yuv);
        fp_wb_yuv = NULL;
    }

    alogd("exit");

    return (void*)result;
}

void requestKeyFrameFunc(int nVeChn)
{
    int ret = AW_MPI_VENC_RequestIDR(nVeChn, TRUE);
    if (ret)
    {
        aloge("fatal error! VeChn[%d] req IDR failed, ret=%d", nVeChn, ret);
    }
    else
    {
        alogd("***** VeChn[%d] req IDR *****", nVeChn);
    }
}

static void* getVencStreamThread(void *pThreadData)
{
    VencStreamContext *pStreamContext = (VencStreamContext*)pThreadData;
    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)pStreamContext->priv;
    char strThreadName[32];
    sprintf(strThreadName, "venc%d-stream", pStreamContext->mVEncChn);
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    ERRORTYPE ret = SUCCESS;
    int result = 0;
    unsigned int nStreamLen = 0;

    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    memset(&stVencStream, 0, sizeof(stVencStream));
    memset(&stVencPack, 0, sizeof(stVencPack));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;

    //get spspps from venc.
    VencHeaderData stSpsPpsInfo;
    memset(&stSpsPpsInfo, 0, sizeof(stSpsPpsInfo));
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pStreamContext->mVEncChn, &stSpsPpsInfo);
        if(ret != SUCCESS)
        {
            aloge("fatal error! get spspps fail[0x%x]!", ret);
        }
    }
    else if(PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pStreamContext->mVEncChn, &stSpsPpsInfo);
        if(ret != SUCCESS)
        {
            aloge("fatal error! get spspps fail[0x%x]!", ret);
        }
    }
    else
    {
        aloge("fatal error! vencType:0x%x is wrong!", pStreamContext->mVEncChnAttr.VeAttr.Type);
    }

#ifdef SAVE_ONE_FILE
    if (pStreamContext->mFile)
    {
        if (stSpsPpsInfo.nLength > 0)
        {
            fwrite(stSpsPpsInfo.pBuffer, 1, stSpsPpsInfo.nLength, pStreamContext->mFile);
        }
    }
#else
    if (pStreamContext->mRecordHandler >= 0)
    {
        RECORD_CONFIG_S mRecConfig;
        memset(&mRecConfig, 0, sizeof(RECORD_CONFIG_S));
        mRecConfig.mVeChn = pStreamContext->mVEncChn;
        mRecConfig.mRecordHandler = pStreamContext->mRecordHandler;
        if (VENC_CHN_MAIN_STREAM == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mMainSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mMainSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mMainFilePath);
        }
        else if (VENC_CHN_SUB_STREAM == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mSubSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mSubSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mSubFilePath);
        }
        else
        {
            aloge("fatal error! invalid venc chn %d", pStreamContext->mVEncChn);
        }
        mRecConfig.requestKeyFrame = requestKeyFrameFunc;
        record_set_config(&mRecConfig);
    }
#endif

#ifdef SUPPORT_RTSP_TEST
    int mSubEncodeWidth = 0;
    int mSubEncodeHeight = 0;
    if (VENC_CHN_MAIN_STREAM == pStreamContext->mVEncChn)
    {
        mSubEncodeWidth = pContext->mConfigPara.mMainEncodeWidth;
        mSubEncodeHeight = pContext->mConfigPara.mMainEncodeHeight;
    }
    else
    {
        mSubEncodeWidth = pContext->mConfigPara.mSubEncodeWidth;
        mSubEncodeHeight = pContext->mConfigPara.mSubEncodeHeight;
    }
    int mRtspBufNum = 3;
    if (pContext->mConfigPara.mRtspBufNum > mRtspBufNum)
    {
        mRtspBufNum = pContext->mConfigPara.mRtspBufNum;
        alogd("update RtspBufNum=%d", mRtspBufNum);
    }
    unsigned int rtspBufSize = mSubEncodeWidth * mSubEncodeHeight * mRtspBufNum;
    unsigned char *stream_buf = (unsigned char *)malloc(rtspBufSize);
    if (NULL == stream_buf)
    {
        aloge("malloc stream_buf failed, size=%d", rtspBufSize);
        return NULL;
    }

    if (VENC_CHN_MAIN_STREAM == pStreamContext->mVEncChn)
    {
        rtsp_start(pContext->mConfigPara.mMainRtspID);
    }
    else if (VENC_CHN_SUB_STREAM == pStreamContext->mVEncChn)
    {
        rtsp_start(pContext->mConfigPara.mSubRtspID);
    }
#endif

    int mActualFrameRate = 0;
    int mTempFrameCnt = 0;
    int mTempCurrentTime = 0;
    int mTempLastTime = 0;
    pStreamContext->mStreamDataCnt = 0;

    while (!pContext->mbExitFlag)
    {
        memset(stVencStream.mpPack, 0, sizeof(VENC_PACK_S));
        ret = AW_MPI_VENC_GetStream(pStreamContext->mVEncChn, &stVencStream, 1000);
        if(SUCCESS == ret)
        {
            pStreamContext->mStreamDataCnt++;
            nStreamLen = stVencStream.mpPack[0].mLen0 + stVencStream.mpPack[0].mLen1 + stVencStream.mpPack[0].mLen2;
            if (nStreamLen <= 0)
            {
                aloge("fatal error! VencStream length error,[%d,%d,%d]!", stVencStream.mpPack[0].mLen0, stVencStream.mpPack[0].mLen1, stVencStream.mpPack[0].mLen2);
            }
#ifdef SAVE_ONE_FILE
            if (pStreamContext->mFile)
            {
                if (stVencStream.mpPack[0].mLen0 > 0)
                {
                    fwrite(stVencStream.mpPack[0].mpAddr0, 1, stVencStream.mpPack[0].mLen0, pStreamContext->mFile);
                }
                if (stVencStream.mpPack[0].mLen1 > 0)
                {
                    fwrite(stVencStream.mpPack[0].mpAddr1, 1, stVencStream.mpPack[0].mLen1, pStreamContext->mFile);
                }
                if (stVencStream.mpPack[0].mLen2 > 0)
                {
                    fwrite(stVencStream.mpPack[0].mpAddr2, 1, stVencStream.mpPack[0].mLen2, pStreamContext->mFile);
                }
            }
#endif
            // check actual framerate.
            mTempFrameCnt++;
            if (0 == mTempLastTime)
            {
                mTempLastTime = getSysTickMs();
            }
            else
            {
                mTempCurrentTime = getSysTickMs();
                if (mTempCurrentTime >= mTempLastTime + 1000)
                {
                    mActualFrameRate = mTempFrameCnt;
                    if (mActualFrameRate < pStreamContext->mVEncFrameRateConfig.DstFrmRate)
                    {
                        alogw("VencChn[%d], actualFrameRate %d < dstFrmRate %d", pStreamContext->mVEncChn, mActualFrameRate, pStreamContext->mVEncFrameRateConfig.DstFrmRate);
                    }
                    mTempLastTime = mTempCurrentTime;
                    mTempFrameCnt = 0;
                }
            }

#ifdef SUPPORT_RTSP_TEST
            if (stVencStream.mpPack != NULL && stVencStream.mpPack->mLen0 > 0)
            {
                uint64_t pts = stVencStream.mpPack->mPTS;
                int len = 0;
                RTSP_FRAME_DATA_TYPE frame_type = RTSP_FRAME_DATA_TYPE_LAST;

                if (PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                {
                    if (H264E_NALU_ISLICE == stVencStream.mpPack->mDataType.enH264EType)
                    {
                        if (NULL == stSpsPpsInfo.pBuffer)
                        {
                            alogd("SpsPpsInfo.pBuffer = NULL!!\n");
                        }
                        /* Get sps/pps first */
                        memcpy(stream_buf, stSpsPpsInfo.pBuffer, stSpsPpsInfo.nLength);
                        len += stSpsPpsInfo.nLength;
                        frame_type = RTSP_FRAME_DATA_TYPE_I;
                        alogv("***** VeChn[%d] H264 got I frame *****", pStreamContext->mVEncChn);
                    }
                    else
                    {
                        frame_type = RTSP_FRAME_DATA_TYPE_P;
                        alogv("VeChn[%d] H264 got P frame", pStreamContext->mVEncChn);
                    }
                }
                else if (PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                {
                    if (H265E_NALU_ISLICE == stVencStream.mpPack->mDataType.enH265EType)
                    {
                        if (NULL == stSpsPpsInfo.pBuffer)
                        {
                            alogd("SpsPpsInfo.pBuffer = NULL!!\n");
                        }
                        /* Get sps/pps first */
                        memcpy(stream_buf, stSpsPpsInfo.pBuffer, stSpsPpsInfo.nLength);
                        len += stSpsPpsInfo.nLength;
                        frame_type = RTSP_FRAME_DATA_TYPE_I;
                        alogv("***** VeChn[%d] H265 got I frame *****", pStreamContext->mVEncChn);
                    }
                    else
                    {
                        frame_type = RTSP_FRAME_DATA_TYPE_P;
                        alogv("VeChn[%d] H265 got P frame", pStreamContext->mVEncChn);
                    }
                }
                else
                {
                    aloge("fatal error! vencType:0x%x is wrong!", pStreamContext->mVEncChnAttr.VeAttr.Type);
                }

                if (rtspBufSize > len + stVencStream.mpPack->mLen0)
                {
                    memcpy(stream_buf + len, stVencStream.mpPack->mpAddr0, stVencStream.mpPack->mLen0);
                    len += stVencStream.mpPack->mLen0;
                }

                if (stVencStream.mpPack->mLen1 > 0)
                {
                    if (rtspBufSize > (len + stVencStream.mpPack->mLen1))
                    {
                        memcpy(stream_buf + len, stVencStream.mpPack->mpAddr1, stVencStream.mpPack->mLen1);
                        len += stVencStream.mpPack->mLen1;
                    }
                }
                if (stVencStream.mpPack->mLen2 > 0)
                {
                    if (rtspBufSize > (len + stVencStream.mpPack->mLen2))
                    {
                        memcpy(stream_buf + len, stVencStream.mpPack->mpAddr2, stVencStream.mpPack->mLen2);
                        len += stVencStream.mpPack->mLen2;
                    }
                }
#ifndef SAVE_ONE_FILE
                if (pStreamContext->mRecordHandler >= 0)
                {
                    RECORD_FRAME_S mRecFrm;
                    memset(&mRecFrm, 0, sizeof(RECORD_FRAME_S));
                    mRecFrm.mpFrm = stream_buf;
                    mRecFrm.mFrmSize = len;
                    if (RTSP_FRAME_DATA_TYPE_I == frame_type)
                        mRecFrm.mFrameType = RECORD_FRAME_TYPE_I;
                    else
                        mRecFrm.mFrameType = RECORD_FRAME_TYPE_P;
                    mRecFrm.mPts = pts;
                    //alogd("record[%d] FrameType=%d", pStreamContext->mRecordHandler, mRecFrm.mFrameType);
                    record_send_data(pStreamContext->mRecordHandler, &mRecFrm);
                }
#endif
                RtspSendDataParam stRtspParam;
                memset(&stRtspParam, 0, sizeof(RtspSendDataParam));
                stRtspParam.buf = stream_buf;
                stRtspParam.size = len;
                stRtspParam.frame_type = frame_type;
                stRtspParam.pts = pts;

                if (VENC_CHN_MAIN_STREAM == pStreamContext->mVEncChn)
                {
                    rtsp_sendData(pContext->mConfigPara.mMainRtspID, &stRtspParam);
                }
                else if (VENC_CHN_SUB_STREAM == pStreamContext->mVEncChn)
                {
                    rtsp_sendData(pContext->mConfigPara.mSubRtspID, &stRtspParam);
                }
            }
#endif

            ret = AW_MPI_VENC_ReleaseStream(pStreamContext->mVEncChn, &stVencStream);
            if(ret != SUCCESS)
            {
                aloge("fatal error! venc_chn[%d] releaseStream fail", pStreamContext->mVEncChn);
            }                
        }
        else
        {
            alogw("fatal error! vencChn[%d] get frame failed! check code!", pStreamContext->mVEncChn);
            continue;
        }
    }

#ifdef SUPPORT_RTSP_TEST
    if (stream_buf)
    {
        free(stream_buf);
        stream_buf = NULL;
    }
#endif

    alogd("exit");

    return (void*)result;
}

static void setVenc2Dnr3Dnr(VENC_CHN mVEncChn)
{
    s2DfilterParam m2DnrPara;
    memset(&m2DnrPara, 0, sizeof(s2DfilterParam));
    m2DnrPara.enable_2d_filter = 1;
    m2DnrPara.filter_strength_y = 127;
    m2DnrPara.filter_strength_uv = 127;
    m2DnrPara.filter_th_y = 11;
    m2DnrPara.filter_th_uv = 7;
    AW_MPI_VENC_Set2DFilter(mVEncChn, &m2DnrPara);
    alogd("VencChn[%d] enable and set 2DFilter param", mVEncChn);

    s3DfilterParam m3DnrPara;
    memset(&m3DnrPara, 0, sizeof(s3DfilterParam));
    m3DnrPara.enable_3d_filter = 1;
    m3DnrPara.adjust_pix_level_enable = 0;
    m3DnrPara.smooth_filter_enable = 1;
    m3DnrPara.max_pix_diff_th = 6;
    m3DnrPara.max_mv_th = 8;
    m3DnrPara.max_mad_th = 14;
    m3DnrPara.min_coef = 13;
    m3DnrPara.max_coef = 16;
    AW_MPI_VENC_Set3DFilter(mVEncChn, &m3DnrPara);
    alogd("VencChn[%d] enable and set 3DFilter param", mVEncChn);
}

static void setVencSuperFrameCfg(VENC_CHN mVEncChn, int mBitrate, int mFramerate)
{
    VENC_SUPERFRAME_CFG_S mSuperFrmParam;
    memset(&mSuperFrmParam, 0, sizeof(VENC_SUPERFRAME_CFG_S));
    mSuperFrmParam.enSuperFrmMode = SUPERFRM_NONE;
    float cmp_bits = 1.5*1024*1024 / 20;
    float dst_bits = (float)mBitrate / mFramerate;
    float bits_ratio = dst_bits / cmp_bits;
    mSuperFrmParam.SuperIFrmBitsThr = (unsigned int)((8.0*250*1024) * bits_ratio);
    mSuperFrmParam.SuperPFrmBitsThr = mSuperFrmParam.SuperIFrmBitsThr / 3;
    alogd("VencChn[%d] SuperFrm Mode:%d, IfrmSize:%d bits, PfrmSize:%d bits", mVEncChn,
        mSuperFrmParam.enSuperFrmMode, mSuperFrmParam.SuperIFrmBitsThr, mSuperFrmParam.SuperPFrmBitsThr);
    AW_MPI_VENC_SetSuperFrameCfg(mVEncChn, &mSuperFrmParam);
}

static void DrawStreamOSD(SampleSmartIPCDemoContext *pContext, int mVeChn, char *text, int x, int y, int idx)
{
#ifdef SUPPORT_STREAM_OSD_TEST
    RGN_ATTR_S stRegion;
    BITMAP_S stBitmap;
    RGN_CHN_ATTR_S stRgnChnAttr;
    int overlay_x = 0;
    int overlay_y = 0;
    int ret = 0;
    FONT_SIZE_TYPE font_size = FONT_SIZE_32;

    overlay_x = x;
    overlay_y = y;
    overlay_x = ALIGN(overlay_x, 16);
    overlay_y = ALIGN(overlay_y, 16);
    alogd("StreamOSD[%d] coordinate(%d,%d), font_size=%d", idx, overlay_x, overlay_y, font_size);

    ret = load_font_file(font_size);
    if (ret < 0)
    {
        aloge("load_font_file %d fail! ret:%d\n", ret, font_size);
    }

    FONT_RGBPIC_S font_pic;
    memset(&font_pic, 0, sizeof(FONT_RGBPIC_S));
    font_pic.font_type     = font_size;
    font_pic.rgb_type      = OSD_RGB_32;
    font_pic.enable_bg     = 0;
    font_pic.foreground[0] = 0x6;
    font_pic.foreground[1] = 0x1;
    font_pic.foreground[2] = 0xFF;
    font_pic.foreground[3] = 0xFF;
    font_pic.background[0] = 0x21;
    font_pic.background[1] = 0x21;
    font_pic.background[2] = 0x21;
    font_pic.background[3] = 0x11;

    memset(&pContext->mRgbPic[idx], 0, sizeof(RGB_PIC_S));
    pContext->mRgbPic[idx].enable_mosaic = 0;
    pContext->mRgbPic[idx].rgb_type      = OSD_RGB_32;
    create_font_rectangle(text, &font_pic, &pContext->mRgbPic[idx]);

    memset(&stRegion, 0, sizeof(RGN_ATTR_S));
    stRegion.enType = OVERLAY_RGN;
    stRegion.unAttr.stOverlay.mPixelFmt = MM_PIXEL_FORMAT_RGB_8888;
    stRegion.unAttr.stOverlay.mSize.Width = pContext->mRgbPic[idx].wide;
    stRegion.unAttr.stOverlay.mSize.Height = pContext->mRgbPic[idx].high;
    AW_MPI_RGN_Create(pContext->mOverlayDrawStreamOSDBase + idx, &stRegion);

    memset(&stBitmap, 0, sizeof(BITMAP_S));
    stBitmap.mPixelFormat = stRegion.unAttr.stOverlay.mPixelFmt;
    stBitmap.mWidth = stRegion.unAttr.stOverlay.mSize.Width;
    stBitmap.mHeight = stRegion.unAttr.stOverlay.mSize.Height;
    stBitmap.mpData  = pContext->mRgbPic[idx].pic_addr;
    AW_MPI_RGN_SetBitMap(pContext->mOverlayDrawStreamOSDBase + idx, &stBitmap);

    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, mVeChn};
    memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    stRgnChnAttr.bShow = TRUE;
    stRgnChnAttr.enType = stRegion.enType;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X = overlay_x;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = overlay_y;
    stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = 0;
    stRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha = 0x40; // global alpha mode value for ARGB1555
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = 60;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUMDIFF_THRESH;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = TRUE; // OSD反色
    AW_MPI_RGN_AttachToChn(pContext->mOverlayDrawStreamOSDBase + idx, &VeChn, &stRgnChnAttr);
#endif
}

static void DestroyStreamOSD(SampleSmartIPCDemoContext *pContext, int mVeChn, int idx)
{
#ifdef SUPPORT_STREAM_OSD_TEST
    if (idx >= 16)
    {
        aloge("fatal error! invalid idx %d", idx);
        return;
    }
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, mVeChn};
    release_rgb_picture(&pContext->mRgbPic[idx]);
    AW_MPI_RGN_DetachFromChn(pContext->mOverlayDrawStreamOSDBase + idx, &VeChn);
    AW_MPI_RGN_Destroy(pContext->mOverlayDrawStreamOSDBase + idx);
#endif
}

int main(int argc, char *argv[])
{
    int result = 0;
    ERRORTYPE ret;

    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)malloc(sizeof(SampleSmartIPCDemoContext));
    if (NULL == pContext)
    {
        aloge("fatal error! malloc pContext failed! size=%d", sizeof(SampleSmartIPCDemoContext));
        return -1;
    }
    gpSampleSmartIPCDemoContext = pContext;
    memset(pContext, 0, sizeof(SampleSmartIPCDemoContext));
    cdx_sem_init(&pContext->mSemExit, 0);

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        aloge("fatal error! command line param is wrong, exit!");
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
    system("cat /proc/meminfo | grep Committed_AS");

    // prepare
    MPP_SYS_CONF_S stSysConf;
    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);
    ret = AW_MPI_SYS_Init();
    if (ret < 0)
    {
        aloge("sys Init failed!");
        return -1;
    }

#ifndef SAVE_ONE_FILE
    record_init();
    pContext->mMainStream.mRecordHandler = -1;
    pContext->mSubStream.mRecordHandler = -1;
#endif

    pContext->mOverlayDrawStreamOSDBase = 100;

    if (pContext->mConfigPara.mMainEnable)
    {
        configMainStream(pContext);
        AW_MPI_VI_CreateVipp(pContext->mMainStream.mVipp);
        AW_MPI_VI_SetVippAttr(pContext->mMainStream.mVipp, &pContext->mMainStream.mViAttr);
        AW_MPI_ISP_Run(pContext->mMainStream.mIsp);
        AW_MPI_VI_EnableVipp(pContext->mMainStream.mVipp);

        //AW_MPI_VI_SetVippMirror(pContext->mMainStream.mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pContext->mMainStream.mVipp, 1);

#ifdef SAVE_ONE_FILE
        if (strlen(pContext->mConfigPara.mMainFilePath) > 0)
        {
            pContext->mMainStream.mFile = fopen(pContext->mConfigPara.mMainFilePath, "wb");
            if(NULL == pContext->mMainStream.mFile)
            {
                aloge("fatal error! why open file[%s] fail?", pContext->mConfigPara.mMainFilePath);
            }
        }
        else
        {
            pContext->mMainStream.mFile = NULL;
        }
#else
        if (strlen(pContext->mConfigPara.mMainFilePath) > 0)
        {
            pContext->mMainStream.mRecordHandler = 0;
        }
#endif

        AW_MPI_VI_CreateVirChn(pContext->mMainStream.mVipp, pContext->mMainStream.mViChn, NULL);
        AW_MPI_VENC_CreateChn(pContext->mMainStream.mVEncChn, &pContext->mMainStream.mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pContext->mMainStream.mVEncChn, &pContext->mMainStream.mVEncRcParam);
        AW_MPI_VENC_SetFrameRate(pContext->mMainStream.mVEncChn, &pContext->mMainStream.mVEncFrameRateConfig);

        setVenc2Dnr3Dnr(pContext->mMainStream.mVEncChn);
        setVencSuperFrameCfg(pContext->mMainStream.mVEncChn, pContext->mConfigPara.mMainEncodeBitrate, pContext->mConfigPara.mMainEncodeFrameRate);

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mMainStream.mVEncChn, &cbInfo);

        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mMainStream.mVipp, pContext->mMainStream.mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mMainStream.mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }
    if (pContext->mConfigPara.mSubEnable)
    {
        configSubStream(pContext);
        AW_MPI_VI_CreateVipp(pContext->mSubStream.mVipp);
        AW_MPI_VI_SetVippAttr(pContext->mSubStream.mVipp, &pContext->mSubStream.mViAttr);
        AW_MPI_ISP_Run(pContext->mSubStream.mIsp);

        if (pContext->mConfigPara.mSubVippCropEnable)
        {
            VI_CROP_CFG_S stCropCfg;
            memset(&stCropCfg, 0, sizeof(VI_CROP_CFG_S));
            stCropCfg.bEnable = pContext->mConfigPara.mSubVippCropEnable;
            stCropCfg.Rect.X = pContext->mConfigPara.mSubVippCropRectX;
            stCropCfg.Rect.Y = pContext->mConfigPara.mSubVippCropRectY;
            stCropCfg.Rect.Width = pContext->mConfigPara.mSubVippCropRectWidth;
            stCropCfg.Rect.Height = pContext->mConfigPara.mSubVippCropRectHeight;
            AW_MPI_VI_SetCrop(pContext->mSubStream.mVipp, &stCropCfg);
        }

        AW_MPI_VI_EnableVipp(pContext->mSubStream.mVipp);

        //AW_MPI_VI_SetVippMirror(pContext->mSubStream.mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pContext->mSubStream.mVipp, 1);

#ifdef SAVE_ONE_FILE
        if (strlen(pContext->mConfigPara.mSubFilePath) > 0)
        {
            pContext->mSubStream.mFile = fopen(pContext->mConfigPara.mSubFilePath, "wb");
            if(NULL == pContext->mSubStream.mFile)
            {
                aloge("fatal error! why open file[%s] fail?", pContext->mConfigPara.mSubFilePath);
            }
        }
        else
        {
            pContext->mSubStream.mFile = NULL;
        }
#else
        if (strlen(pContext->mConfigPara.mSubFilePath) > 0)
        {
            pContext->mSubStream.mRecordHandler = 1;
        }
#endif

        AW_MPI_VI_CreateVirChn(pContext->mSubStream.mVipp, pContext->mSubStream.mViChn, NULL);
        AW_MPI_VENC_CreateChn(pContext->mSubStream.mVEncChn, &pContext->mSubStream.mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pContext->mSubStream.mVEncChn, &pContext->mSubStream.mVEncRcParam);
        AW_MPI_VENC_SetFrameRate(pContext->mSubStream.mVEncChn, &pContext->mSubStream.mVEncFrameRateConfig);

        setVenc2Dnr3Dnr(pContext->mSubStream.mVEncChn);
        setVencSuperFrameCfg(pContext->mSubStream.mVEncChn, pContext->mConfigPara.mSubEncodeBitrate, pContext->mConfigPara.mSubEncodeFrameRate);

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mSubStream.mVEncChn, &cbInfo);

        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mSubStream.mVipp, pContext->mSubStream.mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mSubStream.mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    // start
    if (pContext->mConfigPara.mMainEnable)
    {
        AW_MPI_VI_EnableVirChn(pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        AW_MPI_VENC_StartRecvPic(pContext->mMainStream.mVEncChn);

        if (pContext->mConfigPara.mMainDrawOSDText)
        {
            DrawStreamOSD(pContext, pContext->mMainStream.mVEncChn, pContext->mConfigPara.mMainDrawOSDText, 16, 32, 0);
        }

#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mMainRtspID >= 0)
        {
            RtspServerAttr rtsp_attr;
            memset(&rtsp_attr, 0, sizeof(RtspServerAttr));
            rtsp_attr.net_type = pContext->mConfigPara.mRtspNetType;
        
            if (PT_H264 == pContext->mMainStream.mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H264;
            else if (PT_H265 == pContext->mMainStream.mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H265;
            else
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_LAST;
        
            result = rtsp_open(pContext->mConfigPara.mMainRtspID, &rtsp_attr);
            if (result)
            {
                aloge("Do rtsp_open fail! ret:%d \n", result);
                return -1;
            }
        }
#endif
        result = pthread_create(&pContext->mMainStream.mStreamThreadId, NULL, getVencStreamThread, (void*)&pContext->mMainStream);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }
    if (pContext->mConfigPara.mSubEnable)
    {
        AW_MPI_VI_EnableVirChn(pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        AW_MPI_VENC_StartRecvPic(pContext->mSubStream.mVEncChn);

        if (pContext->mConfigPara.mSubDrawOSDText)
        {
            DrawStreamOSD(pContext, pContext->mSubStream.mVEncChn, pContext->mConfigPara.mSubDrawOSDText, 16, 32, 1);
        }

#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mSubRtspID >= 0)
        {
            RtspServerAttr rtsp_attr;
            memset(&rtsp_attr, 0, sizeof(RtspServerAttr));
            rtsp_attr.net_type = pContext->mConfigPara.mRtspNetType;
        
            if (PT_H264 == pContext->mSubStream.mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H264;
            else if (PT_H265 == pContext->mSubStream.mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H265;
            else
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_LAST;
        
            result = rtsp_open(pContext->mConfigPara.mSubRtspID, &rtsp_attr);
            if (result)
            {
                aloge("Do rtsp_open fail! ret:%d \n", result);
                return -1;
            }
        }
#endif
        result = pthread_create(&pContext->mSubStream.mStreamThreadId, NULL, getVencStreamThread, (void*)&pContext->mSubStream);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

    ai_service_attr_t aiservice_attr;
    memset(&aiservice_attr, 0, sizeof(ai_service_attr_t));
    configAiService(&pContext->mConfigPara, &aiservice_attr);
    ai_service_start(&aiservice_attr);

    // wb yuv
    if (pContext->mConfigPara.mWbYuvEnable)
    {
        result = pthread_create(&pContext->mWbYuvThreadId, NULL, getWbYuvThread, (void*)pContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

    if (pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    ai_service_stop();

    pContext->mbExitFlag = 1;

    void *pRetVal = NULL;

    if (pContext->mConfigPara.mWbYuvEnable)
    {
        pthread_join(pContext->mWbYuvThreadId, &pRetVal);
        alogd("WbYuv pRetVal=%p", pRetVal);
    }

    //stop
    if (pContext->mConfigPara.mMainEnable)
    {
        pthread_join(pContext->mMainStream.mStreamThreadId, &pRetVal);
        alogd("mainStream pRetVal=%p", pRetVal);
        AW_MPI_VI_DisableVirChn(pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        AW_MPI_VENC_StopRecvPic(pContext->mMainStream.mVEncChn);
        AW_MPI_VENC_ResetChn(pContext->mMainStream.mVEncChn);
        AW_MPI_VENC_DestroyChn(pContext->mMainStream.mVEncChn);
        AW_MPI_VI_DestroyVirChn(pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        if(pContext->mMainStream.mFile)
        {
            fclose(pContext->mMainStream.mFile);
            pContext->mMainStream.mFile = NULL;
        }
        if (pContext->mConfigPara.mMainDrawOSDText)
        {
            DestroyStreamOSD(pContext, pContext->mMainStream.mVEncChn, 0);
        }
    }
    if (pContext->mConfigPara.mSubEnable)
    {
        pthread_join(pContext->mSubStream.mStreamThreadId, &pRetVal);
        alogd("subStream pRetVal=%p", pRetVal);
        AW_MPI_VI_DisableVirChn(pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        AW_MPI_VENC_StopRecvPic(pContext->mSubStream.mVEncChn);
        AW_MPI_VENC_ResetChn(pContext->mSubStream.mVEncChn);
        AW_MPI_VENC_DestroyChn(pContext->mSubStream.mVEncChn);
        AW_MPI_VI_DestroyVirChn(pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        if(pContext->mSubStream.mFile)
        {
            fclose(pContext->mSubStream.mFile);
            pContext->mSubStream.mFile = NULL;
        }
        if (pContext->mConfigPara.mSubDrawOSDText)
        {
            DestroyStreamOSD(pContext, pContext->mSubStream.mVEncChn, 1);
        }
    }

    // deinit
    if (pContext->mConfigPara.mMainEnable)
    {
        AW_MPI_VI_DisableVipp(pContext->mMainStream.mVipp);
        AW_MPI_ISP_Stop(pContext->mMainStream.mIsp);
        AW_MPI_VI_DestroyVipp(pContext->mMainStream.mVipp);
#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mMainRtspID >= 0)
        {
            rtsp_stop(pContext->mConfigPara.mMainRtspID);
            rtsp_close(pContext->mConfigPara.mMainRtspID);
        }
#endif
    }
    if (pContext->mConfigPara.mSubEnable)
    {
        AW_MPI_VI_DisableVipp(pContext->mSubStream.mVipp);
        AW_MPI_ISP_Stop(pContext->mSubStream.mIsp);
        AW_MPI_VI_DestroyVipp(pContext->mSubStream.mVipp);
#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mSubRtspID >= 0)
        {
            rtsp_stop(pContext->mConfigPara.mSubRtspID);
            rtsp_close(pContext->mConfigPara.mSubRtspID);
        }
#endif
    }

#ifndef SAVE_ONE_FILE
    record_exit();
#endif

    ret = AW_MPI_SYS_Exit();
    if (ret != SUCCESS)
    {
        aloge("fatal error! sys exit failed!");
    }
    cdx_sem_deinit(&pContext->mSemExit);
_exit:
    if(pContext!=NULL)
    {
        free(pContext);
        pContext = NULL;
    }
    gpSampleSmartIPCDemoContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

