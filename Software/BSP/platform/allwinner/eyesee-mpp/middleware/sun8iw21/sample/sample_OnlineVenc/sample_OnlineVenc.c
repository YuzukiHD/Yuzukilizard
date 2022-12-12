/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_OnlineVenc.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/1/5
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "sample_OnlineVenc"

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
#include "sample_OnlineVenc.h"
#include "sample_OnlineVenc_config.h"


static SampleOnlineVencContext *gpSampleOnlineVencContext = NULL;

static unsigned int getSysTickMs()
{
    unsigned int ms = 0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ms = tv.tv_sec*1000 + tv.tv_usec/1000;
    return ms;
}

static void GenerateARGB1555(void* pBuf, int nSize)
{
    unsigned short nA0Color = 0x7C00;
    unsigned short nA1Color = 0xFC00;
    unsigned short *pShort = (unsigned short*)pBuf;

    int i = 0;
    for(i=0; i< nSize/4; i++)
    {
        *pShort = nA0Color;
        pShort++;
    }
    for(i=0; i< nSize/4; i++)
    {
        *pShort = nA1Color;
        pShort++;
    }
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleOnlineVencContext)
    {
        cdx_sem_up(&gpSampleOnlineVencContext->mSemExit);
    }
}

static int ParseCmdLine(int argc, char **argv, SampleOnlineVencCmdLineParam *pCmdLinePara)
{
    alogd("sample virvi2venc path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleOnlineVencCmdLineParam));
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

static ERRORTYPE loadSampleConfig(SampleOnlineVencConfig *pConfig, const char *conf_path)
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

        // main stream
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
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainFilePathh, NULL);
        strncpy(pConfig->mMainFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMainOnlineEnable = GetConfParaInt(&stConfParser, CFG_MainOnlineEnable, 0);
        pConfig->mMainOnlineShareBufNum = GetConfParaInt(&stConfParser, CFG_MainOnlineShareBufNum, 0);
        pConfig->mMainEncppEnable = GetConfParaInt(&stConfParser, CFG_MainEncppEnable, 0);

        // sub stream
        pConfig->mSubIsp = GetConfParaInt(&stConfParser, CFG_SubIsp, 0);
        pConfig->mSubVipp = GetConfParaInt(&stConfParser, CFG_SubVipp, 0);
        pConfig->mSubSrcWidth = GetConfParaInt(&stConfParser, CFG_SubSrcWidth, 0);
        pConfig->mSubSrcHeight = GetConfParaInt(&stConfParser, CFG_SubSrcHeight, 0);
        pConfig->mSubPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_SubPixelFormat);
        pConfig->mSubWdrEnable = GetConfParaInt(&stConfParser, CFG_SubWdrEnable, 0);
        pConfig->mSubViBufNum = GetConfParaInt(&stConfParser, CFG_SubViBufNum, 0);
        pConfig->mSubSrcFrameRate = GetConfParaInt(&stConfParser, CFG_SubSrcFrameRate, 0);

        pConfig->mSubVippCropEnable = GetConfParaInt(&stConfParser, CFG_SubVippCropEnable, 0);
        pConfig->mSubVippCropRectX = GetConfParaInt(&stConfParser, CFG_SubVippCropRectX, 0);
        pConfig->mSubVippCropRectY = GetConfParaInt(&stConfParser, CFG_SubVippCropRectY, 0);
        pConfig->mSubVippCropRectWidth = GetConfParaInt(&stConfParser, CFG_SubVippCropRectWidth, 0);
        pConfig->mSubVippCropRectHeight = GetConfParaInt(&stConfParser, CFG_SubVippCropRectHeight, 0);

        pConfig->mSubViChn = GetConfParaInt(&stConfParser, CFG_SubViChn, 0);
        pConfig->mSubVEncChn = GetConfParaInt(&stConfParser, CFG_SubVEncChn, 0);
        pConfig->mSubEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_SubEncodeType);
        pConfig->mSubEncodeWidth = GetConfParaInt(&stConfParser, CFG_SubEncodeWidth, 0);
        pConfig->mSubEncodeHeight = GetConfParaInt(&stConfParser, CFG_SubEncodeHeight, 0);
        pConfig->mSubEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_SubEncodeFrameRate, 0);
        pConfig->mSubEncodeBitrate = GetConfParaInt(&stConfParser, CFG_SubEncodeBitrate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubFilePath, NULL);
        strncpy(pConfig->mSubFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubEncppSharpAttenCoefPer = 100 * pConfig->mSubSrcWidth / pConfig->mMainSrcWidth;
        pConfig->mSubEncppEnable = GetConfParaInt(&stConfParser, CFG_SubEncppEnable, 0);

        // sub lapse stream
        pConfig->mSubLapseVipp = pConfig->mSubVipp;
        pConfig->mSubLapseSrcWidth = pConfig->mSubSrcWidth;
        pConfig->mSubLapseSrcHeight = pConfig->mSubSrcHeight;
        pConfig->mSubLapsePixelFormat = pConfig->mSubPixelFormat;
        pConfig->mSubLapseWdrEnable = pConfig->mSubWdrEnable;
        pConfig->mSubLapseViBufNum = pConfig->mSubViBufNum;
        pConfig->mSubLapseSrcFrameRate = pConfig->mSubSrcFrameRate;
        pConfig->mSubLapseViChn = GetConfParaInt(&stConfParser, CFG_SubLapseViChn, 0);
        pConfig->mSubLapseVEncChn = GetConfParaInt(&stConfParser, CFG_SubLapseVEncChn, 0);
        pConfig->mSubLapseEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_SubLapseEncodeType);
        pConfig->mSubLapseEncodeWidth = GetConfParaInt(&stConfParser, CFG_SubLapseEncodeWidth, 0);
        pConfig->mSubLapseEncodeHeight = GetConfParaInt(&stConfParser, CFG_SubLapseEncodeHeight, 0);
        pConfig->mSubLapseEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_SubLapseEncodeFrameRate, 0);
        pConfig->mSubLapseEncodeBitrate = GetConfParaInt(&stConfParser, CFG_SubLapseEncodeBitrate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubLapseFilePath, NULL);
        strncpy(pConfig->mSubLapseFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubLapseEncppSharpAttenCoefPer = 100 * pConfig->mSubLapseSrcWidth / pConfig->mMainSrcWidth;
        pConfig->mSubLapseTime = GetConfParaInt(&stConfParser, CFG_SubLapseTime, 0);
        pConfig->mSubLapseEncppEnable = GetConfParaInt(&stConfParser, CFG_SubLapseEncppEnable, 0);

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
    
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleOnlineVencContext *pContext = (SampleOnlineVencContext *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                aloge("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                OnlineVenc_MessageData stMsgData;
                stMsgData.mpOnlineVencData = (SampleOnlineVencContext*)cookie;
                stCmdMsg.command = Vi_Timeout;
                stCmdMsg.para0 = pChn->mDevId;
                stCmdMsg.mDataSize = sizeof(OnlineVenc_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&pContext->mMsgQueue, &stCmdMsg);
                break;
            }
        }
    }
    else if (MOD_ID_VENC == pChn->mModId)
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
            case VENC_CHN_SUB_LAPSE_STREAM:
            {
                pStreamContext = &pContext->mSubLapseStream;
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

static void configMainStream(SampleOnlineVencContext *pContext)
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
    pContext->mMainStream.mViAttr.format.colorspace = V4L2_COLORSPACE_REC709_PART_RANGE;
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
    pContext->mMainStream.mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT;
    pContext->mMainStream.mVEncRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
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
}

static void configSubStream(SampleOnlineVencContext *pContext)
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
    pContext->mSubStream.mViAttr.format.colorspace = V4L2_COLORSPACE_REC709_PART_RANGE;
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
    pContext->mSubStream.mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT;
    pContext->mSubStream.mVEncRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
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
}

static void configSubLapseStream(SampleOnlineVencContext *pContext)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;

    pContext->mSubLapseStream.priv = (void*)pContext;    
    pContext->mSubLapseStream.mIsp = pContext->mConfigPara.mSubIsp;
    pContext->mSubLapseStream.mVipp = pContext->mConfigPara.mSubLapseVipp;
    pContext->mSubLapseStream.mViChn = pContext->mConfigPara.mSubLapseViChn;
    pContext->mSubLapseStream.mVEncChn = pContext->mConfigPara.mSubLapseVEncChn;
    pContext->mSubLapseStream.mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mSubLapseStream.mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mSubLapseStream.mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mSubLapsePixelFormat);
    pContext->mSubLapseStream.mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mSubLapseStream.mViAttr.format.colorspace = V4L2_COLORSPACE_REC709_PART_RANGE;
    pContext->mSubLapseStream.mViAttr.format.width = pContext->mConfigPara.mSubLapseSrcWidth;
    pContext->mSubLapseStream.mViAttr.format.height = pContext->mConfigPara.mSubLapseSrcHeight;
    pContext->mSubLapseStream.mViAttr.fps = pContext->mConfigPara.mSubLapseSrcFrameRate;
    pContext->mSubLapseStream.mViAttr.use_current_win = 1;
    pContext->mSubLapseStream.mViAttr.nbufs = pContext->mConfigPara.mSubLapseViBufNum;
    pContext->mSubLapseStream.mViAttr.nplanes = 2;
    pContext->mSubLapseStream.mViAttr.wdr_mode = pContext->mConfigPara.mSubLapseWdrEnable;
    pContext->mSubLapseStream.mViAttr.drop_frame_num = 10;

    pContext->mSubLapseStream.mVEncChnAttr.VeAttr.Type = pContext->mConfigPara.mSubLapseEncodeType;
    pContext->mSubLapseStream.mVEncChnAttr.VeAttr.MaxKeyInterval = 100;//40;
    pContext->mSubLapseStream.mVEncChnAttr.VeAttr.SrcPicWidth = pContext->mConfigPara.mSubLapseSrcWidth;
    pContext->mSubLapseStream.mVEncChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.mSubLapseSrcHeight;
    pContext->mSubLapseStream.mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mSubLapseStream.mVEncChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.mSubLapsePixelFormat;
    pContext->mSubLapseStream.mVEncChnAttr.VeAttr.mColorSpace = pContext->mSubLapseStream.mViAttr.format.colorspace;
    pContext->mSubLapseStream.mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT;
    pContext->mSubLapseStream.mVEncRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
    pContext->mSubLapseStream.mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pContext->mSubLapseStream.mEncppSharpAttenCoefPer = pContext->mConfigPara.mSubLapseEncppSharpAttenCoefPer;
    alogd("subLapse EncppSharpAttenCoefPer: %d%%", pContext->mSubLapseStream.mEncppSharpAttenCoefPer);

    pContext->mSubLapseStream.mViAttr.mbEncppEnable = pContext->mConfigPara.mSubLapseEncppEnable;
    pContext->mSubLapseStream.mVEncChnAttr.EncppAttr.mbEncppEnable = pContext->mConfigPara.mSubLapseEncppEnable;

    if (pContext->mConfigPara.mSubLapseSrcFrameRate)
    {
        vbvThreshSize = pContext->mConfigPara.mSubLapseEncodeBitrate/8/pContext->mConfigPara.mSubLapseSrcFrameRate*15;
    }
    vbvBufSize = pContext->mConfigPara.mSubLapseEncodeBitrate/8*4 + vbvThreshSize;
    alogd("SubLapse vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    if(PT_H264 == pContext->mSubLapseStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH264e.PicWidth = pContext->mConfigPara.mSubLapseEncodeWidth;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.mSubLapseEncodeHeight;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH264e.mLevel = 0;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH264e.mThreshSize = vbvThreshSize;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH264e.BufSize = vbvBufSize;
        pContext->mSubLapseStream.mVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mSubLapseEncodeBitrate;
        pContext->mSubLapseStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        if (pContext->mSubLapseStream.mVEncChnAttr.RcAttr.mRcMode == VENC_RC_MODE_H264VBR)
        {
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mMaxQp = 50;//45;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mMinQp = 10;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mMaxPqp = 50;//45;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mMinPqp = 10;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mQpInit = 37;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mMovingTh = 20;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mQuality = 10;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
        }
        else if (pContext->mSubLapseStream.mVEncChnAttr.RcAttr.mRcMode == VENC_RC_MODE_H264CBR)
        {
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Cbr.mMaxQp = 50;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Cbr.mMinQp = 10;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Cbr.mMaxPqp = 50;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Cbr.mMinPqp = 10;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Cbr.mQpInit = 37;
            pContext->mSubLapseStream.mVEncRcParam.ParamH264Cbr.mbEnMbQpLimit = 0;
        }
    }
    else if(PT_H265 == pContext->mSubLapseStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.mSubLapseEncodeWidth;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.mSubLapseEncodeHeight;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH265e.mLevel = 0;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH265e.mThreshSize = vbvThreshSize;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrH265e.mBufSize = vbvBufSize;
        pContext->mSubLapseStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        pContext->mSubLapseStream.mVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mSubLapseEncodeBitrate;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mMaxQp = 50;//45;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mMinQp = 10;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mMaxPqp = 50;//45;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mMinPqp = 10;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mQpInit = 37;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mQuality = 10;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
        pContext->mSubLapseStream.mVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }
    else if(PT_MJPEG == pContext->mSubLapseStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth= pContext->mConfigPara.mSubLapseEncodeWidth;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.mSubLapseEncodeHeight;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrMjpeg.mThreshSize = vbvThreshSize;
        pContext->mSubLapseStream.mVEncChnAttr.VeAttr.AttrMjpeg.mBufSize = vbvBufSize;
        pContext->mSubLapseStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        pContext->mSubLapseStream.mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mSubLapseEncodeBitrate;
    }
    pContext->mSubLapseStream.mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->mSubLapseStream.mVEncChnAttr.GopAttr.mGopSize = 2;
    pContext->mSubLapseStream.mVEncFrameRateConfig.SrcFrmRate = pContext->mConfigPara.mSubLapseSrcFrameRate;
    pContext->mSubLapseStream.mVEncFrameRateConfig.DstFrmRate = pContext->mConfigPara.mSubLapseEncodeFrameRate;
}

static void* getWbYuvThread(void *pThreadData)
{
    int result = 0;
    SampleOnlineVencContext *pContext = (SampleOnlineVencContext*)pThreadData;
    VencStreamContext *pStreamContext = NULL;

    if (VENC_CHN_MAIN_STREAM == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mMainStream;
    }
    else if (VENC_CHN_SUB_STREAM == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mSubStream;
    }
    else if (VENC_CHN_SUB_LAPSE_STREAM == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mSubLapseStream;
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

static void* getVencStreamThread(void *pThreadData)
{
    VencStreamContext *pStreamContext = (VencStreamContext*)pThreadData;
    SampleOnlineVencContext *pContext = (SampleOnlineVencContext*)pStreamContext->priv;
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
    if (pStreamContext->mFile)
    {
        if (stSpsPpsInfo.nLength > 0)
        {
            fwrite(stSpsPpsInfo.pBuffer, 1, stSpsPpsInfo.nLength, pStreamContext->mFile);
        }
    }

    int mActualFrameRate = 0;
    int mTempFrameCnt = 0;
    int mTempCurrentTime = 0;
    int mTempLastTime = 0;
    pStreamContext->mStreamDataCnt = 0;

    int nMilliSec = 1000;
    // This operation will cause the test to wait too long when exiting.
    if (TRUE == pStreamContext->mbSubLapseEnable)
    {
        int64_t mTimeLapse = 0;
        AW_MPI_VENC_GetTimeLapse(pStreamContext->mVEncChn, &mTimeLapse);
        nMilliSec = 2 * mTimeLapse / 1000;
        alogd("Lapse VencChn[%d] update nMilliSec to %d ms", pStreamContext->mVEncChn, nMilliSec);
    }

    while (!pContext->mbExitFlag)
    {
        memset(stVencStream.mpPack, 0, sizeof(VENC_PACK_S));
        ret = AW_MPI_VENC_GetStream(pStreamContext->mVEncChn, &stVencStream, nMilliSec);
        if(SUCCESS == ret)
        {
            pStreamContext->mStreamDataCnt++;
            nStreamLen = stVencStream.mpPack[0].mLen0 + stVencStream.mpPack[0].mLen1 + stVencStream.mpPack[0].mLen2;
            if (nStreamLen <= 0)
            {
                aloge("fatal error! VencStream length error,[%d,%d,%d]!", stVencStream.mpPack[0].mLen0, stVencStream.mpPack[0].mLen1, stVencStream.mpPack[0].mLen2);
            }
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

            if (FALSE == pStreamContext->mbSubLapseEnable)
            {
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
            }

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

    alogd("exit");

    return (void*)result;
}

static ERRORTYPE resetCamera(SampleOnlineVencContext *pContext, int vipp)
{
    VencStreamContext *pStreamContext = NULL;
    if (vipp == pContext->mMainStream.mVipp)
    {
        alogd("MainStream");
        pStreamContext = &pContext->mMainStream;
    }
    else if (vipp == pContext->mSubStream.mVipp)
    {
        alogd("SubStream");
        pStreamContext = &pContext->mSubStream;
    }
    else
    {
        alogd("invalid vipp %d", vipp);
        return -1;
    }

    alogd("stop");
    AW_MPI_VI_DisableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
    AW_MPI_VENC_StopRecvPic(pStreamContext->mVEncChn);
    MPP_CHN_S ViChn = {MOD_ID_VIU, pStreamContext->mVipp, pStreamContext->mViChn};
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pStreamContext->mVEncChn};
    AW_MPI_SYS_UnBind(&ViChn, &VeChn);
    AW_MPI_VI_DestroyVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
    AW_MPI_VI_DisableVipp(pStreamContext->mVipp);
    AW_MPI_ISP_Stop(pStreamContext->mIsp);
    AW_MPI_VI_DestroyVipp(pStreamContext->mVipp);

    alogd("prepare");
    if (vipp == pContext->mMainStream.mVipp)
        configMainStream(pContext);
    else
        configSubStream(pContext);
    AW_MPI_VI_CreateVipp(pStreamContext->mVipp);
    AW_MPI_VI_SetVippAttr(pStreamContext->mVipp, &pStreamContext->mViAttr);
    AW_MPI_ISP_Run(pStreamContext->mIsp);
    AW_MPI_VI_CreateVirChn(pStreamContext->mVipp, pStreamContext->mViChn, NULL);
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VI_RegisterCallback(pStreamContext->mVipp, &cbInfo);
    AW_MPI_VI_EnableVipp(pStreamContext->mVipp);
    AW_MPI_SYS_Bind(&ViChn, &VeChn);

    alogd("start");
    AW_MPI_VI_EnableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
    AW_MPI_VENC_StartRecvPic(pStreamContext->mVEncChn);

    alogd("ok");
}

static void *MsgQueueThread(void *pThreadData)
{
    SampleOnlineVencContext *pContext = (SampleOnlineVencContext*)pThreadData;
    message_t stCmdMsg;
    OnlineVencMsgType cmd;
    int nCmdPara;

    alogd("msg queue thread start run!");
    while (1)
    {
        if (0 == get_message(&pContext->mMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case Vi_Timeout:
                {
                    OnlineVenc_MessageData *pMsgData = (OnlineVenc_MessageData*)stCmdMsg.mpData;
                    int vipp = nCmdPara;

                    resetCamera(pContext, vipp);

                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case MsgQueue_Stop:
                {
                    goto _Exit;
                }
                default:
                {
                    break;
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pContext->mMsgQueue, 0);
        }
    }
_Exit:
    alogd("msg queue thread exit!");
    return NULL;
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
    float dst_bits = 0;
    if (0 != mFramerate)
        dst_bits = (float)mBitrate / mFramerate;
    else
        dst_bits = (float)mBitrate / 20;
    float bits_ratio = dst_bits / cmp_bits;
    mSuperFrmParam.SuperIFrmBitsThr = (unsigned int)((8.0*200*1024) * bits_ratio);
    mSuperFrmParam.SuperPFrmBitsThr = mSuperFrmParam.SuperIFrmBitsThr / 3;
    alogd("VencChn[%d] SuperFrm Mode:%d, IfrmSize:%d bits, PfrmSize:%d bits", mVEncChn,
        mSuperFrmParam.enSuperFrmMode, mSuperFrmParam.SuperIFrmBitsThr, mSuperFrmParam.SuperPFrmBitsThr);
    AW_MPI_VENC_SetSuperFrameCfg(mVEncChn, &mSuperFrmParam);
}

int main(int argc, char *argv[])
{
    int result = 0;
    ERRORTYPE ret;

    SampleOnlineVencContext *pContext = (SampleOnlineVencContext*)malloc(sizeof(SampleOnlineVencContext));
    if (NULL == pContext)
    {
        aloge("fatal error! malloc pContext failed! size=%d", sizeof(SampleOnlineVencContext));
        return -1;
    }
    gpSampleOnlineVencContext = pContext;
    memset(pContext, 0, sizeof(SampleOnlineVencContext));
    cdx_sem_init(&pContext->mSemExit, 0);

    if (message_create(&pContext->mMsgQueue) < 0)
    {
        aloge("message create fail!");
        goto _exit;
    }

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

    pContext->mMainIspRunFlag = 0;  
    pContext->mSubIspRunFlag = 0;
    if (pContext->mConfigPara.mMainVEncChn >= 0 && pContext->mConfigPara.mMainViChn >= 0)
    {
        configMainStream(pContext);
        AW_MPI_VI_CreateVipp(pContext->mMainStream.mVipp);
        AW_MPI_VI_SetVippAttr(pContext->mMainStream.mVipp, &pContext->mMainStream.mViAttr);
        if (0 == pContext->mMainIspRunFlag)
        {
            AW_MPI_ISP_Run(pContext->mMainStream.mIsp);
            pContext->mMainIspRunFlag = 1;
        }
        AW_MPI_VI_EnableVipp(pContext->mMainStream.mVipp);

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
        AW_MPI_VI_RegisterCallback(pContext->mMainStream.mVipp, &cbInfo);

        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mMainStream.mVipp, pContext->mMainStream.mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mMainStream.mVEncChn};
        AW_MPI_SYS_Bind(&ViChn,&VeChn);
    }
    if (pContext->mConfigPara.mSubVEncChn >= 0 && pContext->mConfigPara.mSubViChn >= 0)
    {
        configSubStream(pContext);
        AW_MPI_VI_CreateVipp(pContext->mSubStream.mVipp);
        AW_MPI_VI_SetVippAttr(pContext->mSubStream.mVipp, &pContext->mSubStream.mViAttr);
        if (0 == pContext->mSubIspRunFlag && (pContext->mMainStream.mIsp != pContext->mSubStream.mIsp || 0 == pContext->mMainIspRunFlag))
        {
            AW_MPI_ISP_Run(pContext->mSubStream.mIsp);
            pContext->mSubIspRunFlag = 1;
        }

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
        AW_MPI_VI_RegisterCallback(pContext->mMainStream.mVipp, &cbInfo);

        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mSubStream.mVipp, pContext->mSubStream.mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mSubStream.mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }
    if (pContext->mConfigPara.mSubLapseVEncChn >= 0 && pContext->mConfigPara.mSubLapseViChn >= 0)
    {
        configSubLapseStream(pContext);

        if (pContext->mSubStream.mVipp != pContext->mSubLapseStream.mVipp)
        {
            AW_MPI_VI_CreateVipp(pContext->mSubLapseStream.mVipp);
            AW_MPI_VI_SetVippAttr(pContext->mSubLapseStream.mVipp, &pContext->mSubLapseStream.mViAttr);
            if (0 == pContext->mSubIspRunFlag)
            {
                AW_MPI_ISP_Run(pContext->mSubLapseStream.mIsp);
                pContext->mSubIspRunFlag = 1;
            }
            AW_MPI_VI_EnableVipp(pContext->mSubLapseStream.mVipp);
        }

        if (strlen(pContext->mConfigPara.mSubLapseFilePath) > 0)
        {
            pContext->mSubLapseStream.mFile = fopen(pContext->mConfigPara.mSubLapseFilePath, "wb");
            if (NULL == pContext->mSubLapseStream.mFile)
            {
                aloge("fatal error! why open file[%s] fail?", pContext->mConfigPara.mSubLapseFilePath);
            }
        }
        else
        {
            pContext->mSubLapseStream.mFile = NULL;
        }
        AW_MPI_VI_CreateVirChn(pContext->mSubLapseStream.mVipp, pContext->mSubLapseStream.mViChn, NULL);
        AW_MPI_VENC_CreateChn(pContext->mSubLapseStream.mVEncChn, &pContext->mSubLapseStream.mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pContext->mSubLapseStream.mVEncChn, &pContext->mSubLapseStream.mVEncRcParam);
        AW_MPI_VENC_SetFrameRate(pContext->mSubLapseStream.mVEncChn, &pContext->mSubLapseStream.mVEncFrameRateConfig);

        if (pContext->mConfigPara.mSubLapseTime)
        {
            pContext->mSubLapseStream.mbSubLapseEnable = TRUE;
            alogd("Lapse set TimeLapse %d us", pContext->mConfigPara.mSubLapseTime);
            AW_MPI_VENC_SetTimeLapse(pContext->mSubLapseStream.mVEncChn, pContext->mConfigPara.mSubLapseTime);
        }

        setVenc2Dnr3Dnr(pContext->mSubLapseStream.mVEncChn);
        setVencSuperFrameCfg(pContext->mSubLapseStream.mVEncChn, pContext->mConfigPara.mSubLapseEncodeBitrate, pContext->mConfigPara.mSubLapseEncodeFrameRate);

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mSubLapseStream.mVEncChn, &cbInfo);

        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mSubLapseStream.mVipp, pContext->mSubLapseStream.mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mSubLapseStream.mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    //create msg queue thread
    result = pthread_create(&pContext->mMsgQueueThreadId, NULL, MsgQueueThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create Msg Queue Thread fail[%d]", result);
    }
    else
    {
        alogd("create Msg Queue Thread success! threadId[0x%x]", &pContext->mMsgQueueThreadId);
    }

    // start
    if (pContext->mConfigPara.mMainVEncChn >= 0 && pContext->mConfigPara.mMainViChn >= 0)
    {
        AW_MPI_VI_EnableVirChn(pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        AW_MPI_VENC_StartRecvPic(pContext->mMainStream.mVEncChn);
        result = pthread_create(&pContext->mMainStream.mStreamThreadId, NULL, getVencStreamThread, (void*)&pContext->mMainStream);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }
    if (pContext->mConfigPara.mSubVEncChn >= 0 && pContext->mConfigPara.mSubViChn >= 0)
    {
        AW_MPI_VI_EnableVirChn(pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        AW_MPI_VENC_StartRecvPic(pContext->mSubStream.mVEncChn);
        result = pthread_create(&pContext->mSubStream.mStreamThreadId, NULL, getVencStreamThread, (void*)&pContext->mSubStream);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }
    if (pContext->mConfigPara.mSubLapseVEncChn >= 0 && pContext->mConfigPara.mSubLapseViChn >= 0)
    {
        AW_MPI_VI_EnableVirChn(pContext->mSubLapseStream.mVipp, pContext->mSubLapseStream.mViChn);
        AW_MPI_VENC_StartRecvPic(pContext->mSubLapseStream.mVEncChn);
        result = pthread_create(&pContext->mSubLapseStream.mStreamThreadId, NULL, getVencStreamThread, (void*)&pContext->mSubLapseStream);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

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
    pContext->mbExitFlag = 1;

    //stop msg queue thread
    message_t stMsgCmd;
    stMsgCmd.command = MsgQueue_Stop;
    put_message(&pContext->mMsgQueue, &stMsgCmd);
    pthread_join(pContext->mMsgQueueThreadId, NULL);
    alogd("start to free res");

    void *pRetVal = NULL;

    if (pContext->mConfigPara.mWbYuvEnable)
    {
        pthread_join(pContext->mWbYuvThreadId, &pRetVal);
        alogd("WbYuv pRetVal=%p", pRetVal);
    }

    //stop
    if (pContext->mConfigPara.mMainVEncChn >= 0 && pContext->mConfigPara.mMainViChn >= 0)
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
    }
    if (pContext->mConfigPara.mSubVEncChn >= 0 && pContext->mConfigPara.mSubViChn >= 0)
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
    }
    if (pContext->mConfigPara.mSubLapseVEncChn >= 0 && pContext->mConfigPara.mSubLapseViChn >= 0)
    {
        pthread_join(pContext->mSubLapseStream.mStreamThreadId, &pRetVal);
        alogd("LapseStream pRetVal=%p", pRetVal);
        AW_MPI_VI_DisableVirChn(pContext->mSubLapseStream.mVipp, pContext->mSubLapseStream.mViChn);
        AW_MPI_VENC_StopRecvPic(pContext->mSubLapseStream.mVEncChn);
        AW_MPI_VENC_ResetChn(pContext->mSubLapseStream.mVEncChn);
        AW_MPI_VENC_DestroyChn(pContext->mSubLapseStream.mVEncChn);
        AW_MPI_VI_DestroyVirChn(pContext->mSubLapseStream.mVipp, pContext->mSubLapseStream.mViChn);
        if(pContext->mSubLapseStream.mFile)
        {
            fclose(pContext->mSubLapseStream.mFile);
            pContext->mSubLapseStream.mFile = NULL;
        }
    }

    // deinit
    if (pContext->mConfigPara.mMainVEncChn >= 0 && pContext->mConfigPara.mMainViChn >= 0)
    {
        AW_MPI_VI_DisableVipp(pContext->mMainStream.mVipp);
        if (pContext->mMainIspRunFlag)
        {
            AW_MPI_ISP_Stop(pContext->mMainStream.mIsp);
            pContext->mMainIspRunFlag = 0;
        }
        AW_MPI_VI_DestroyVipp(pContext->mMainStream.mVipp);
    }
    if (pContext->mConfigPara.mSubVEncChn >= 0 && pContext->mConfigPara.mSubViChn >= 0)
    {
        AW_MPI_VI_DisableVipp(pContext->mSubStream.mVipp);
        if (pContext->mSubIspRunFlag)
        {
            AW_MPI_ISP_Stop(pContext->mSubStream.mIsp);
            pContext->mSubIspRunFlag = 0;
        }
        AW_MPI_VI_DestroyVipp(pContext->mSubStream.mVipp);
    }
    if (pContext->mConfigPara.mSubLapseVEncChn >= 0 && pContext->mConfigPara.mSubLapseViChn >= 0)
    {
        AW_MPI_VI_DisableVipp(pContext->mSubLapseStream.mVipp);
        if (pContext->mSubIspRunFlag)
        {
            AW_MPI_ISP_Stop(pContext->mSubLapseStream.mIsp);
            pContext->mSubIspRunFlag = 0;
        }
        AW_MPI_VI_DestroyVipp(pContext->mSubLapseStream.mVipp);
    }

    ret = AW_MPI_SYS_Exit();
    if (ret != SUCCESS)
    {
        aloge("fatal error! sys exit failed!");
    }
    cdx_sem_deinit(&pContext->mSemExit);
    message_destroy(&pContext->mMsgQueue);
_exit:
    if(pContext!=NULL)
    {
        free(pContext);
        pContext = NULL;
    }
    gpSampleOnlineVencContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

