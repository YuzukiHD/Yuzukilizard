//#define LOG_NDEBUG 0
#define LOG_TAG "sample_region"
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
#include "sample_region_config.h"
#include "sample_region.h"
#include "mpi_isp.h"
#include <BITMAP_S.h>
#include "media/mpi_venc.h"
#include "mm_comm_venc.h"


#define HAVE_H264

static SampleRegionContext *gpSampleRegionContext = NULL;

static int initSampleRegionContext(SampleRegionContext *pContext)
{
    memset(pContext, 0, sizeof(SampleRegionContext));
    pContext->mUILayer = HLAY(2, 0);
    cdx_sem_init(&pContext->mSemExit, 0);
    return 0;
}

static int destroySampleRegionContext(SampleRegionContext *pContext)
{    
    cdx_sem_deinit(&pContext->mSemExit);    
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleRegionCmdLineParam *pCmdLinePara)
{
    //alogd("sample_region input path is : %s", argv[0]);
    int ret = 0;
    int i = 1;
    memset(pCmdLinePara, 0, sizeof(SampleRegionCmdLineParam));
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if((++i) >= argc)
            {
                aloge("fatal error!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error!");
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], strlen(argv[i]));
            pCmdLinePara->mConfigFilePath[strlen(argv[i])] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
             printf("CmdLine param example:\n"                
                "\t run -path /home/sample_vi2vo.conf\n");
             ret = 1;
             break;
        }
        else
        {
             printf("CmdLine param example:\n"                "\t run -path /home/sample_vi2vo.conf\n");
        }
        ++i;
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

static ERRORTYPE loadSampleRegionConfig(SampleRegionConfig *pConfig, const char *conf_path)
{
    int ret;

    if(NULL == conf_path)
    {
        alogd("user not set config file. use default test parameter!");        
        pConfig->mCaptureWidth = 1920;        
        pConfig->mCaptureHeight = 1080;        
        pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;        
        pConfig->mFrameRate = 60;        
        pConfig->mDispWidth = 480;
        pConfig->mDispHeight = 640;
        pConfig->mTestDuration = 20;

        pConfig->mBitmapFormat = MM_PIXEL_FORMAT_RGB_8888;
        pConfig->overlay_x = 860;
        pConfig->overlay_y = 540;
        pConfig->overlay_w = 100;
        pConfig->overlay_h = 100;

        pConfig->cover_x = 100;
        pConfig->cover_y = 100;
        pConfig->cover_w = 400;
        pConfig->cover_h = 400;

        pConfig->orl_x = 100;
        pConfig->orl_y = 600;
        pConfig->orl_w = 200;
        pConfig->orl_h = 200;
        pConfig->orl_thick = 6;

        pConfig->mbAttachToVi = true;
        pConfig->add_venc_channel = true;
        pConfig->encoder_count = 5000;
        pConfig->bit_rate = 8388608;
        pConfig->EncoderType = PT_H264;
         //pConfig->OutputFilePath = "test.h264";
        strcpy(pConfig->OutputFilePath, "/mnt/extsd/testRegion.h264");
         
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    
    memset(pConfig, 0, sizeof(SampleRegionConfig));

    pConfig->mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_CAPTURE_WIDTH, 1920);
    pConfig->mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_CAPTURE_HEIGHT, 1080);

    pConfig->mPicFormat = getPicFormatFromConfig(&stConfParser, SAMPLE_REGION_KEY_PIC_FORMAT);

    pConfig->mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_FRAME_RATE, 30);
    pConfig->mDispWidth = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_DISP_WIDTH, 0);
    pConfig->mDispHeight = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_DISP_HEIGHT, 0);
    pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_TEST_DURATION, 0);

    char *strFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_REGION_KEY_BITMAP_FORMAT, NULL);
    if(!strcmp(strFormat, "ARGB1555"))
    {
        pConfig->mBitmapFormat = MM_PIXEL_FORMAT_RGB_1555;
    }
    else
    {
        pConfig->mBitmapFormat = MM_PIXEL_FORMAT_RGB_8888;
    }
    pConfig->overlay_x = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_OVERLAY_X, 0);
    pConfig->overlay_y = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_OVERLAY_Y, 0);
    pConfig->overlay_w = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_OVERLAY_W, 0);
    pConfig->overlay_h = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_OVERLAY_H, 0);

    pConfig->cover_x = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_COVER_X, 0);
    pConfig->cover_y = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_COVER_Y, 0);
    pConfig->cover_w = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_COVER_W, 0);
    pConfig->cover_h = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_COVER_H, 0);

    pConfig->orl_x = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ORL_X, 0);
    pConfig->orl_y = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ORL_Y, 0);
    pConfig->orl_w = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ORL_W, 0);
    pConfig->orl_h = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ORL_H, 0);
    pConfig->orl_thick = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ORL_THICK, 0);

    pConfig->mbAttachToVi = (bool)GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ATTACH_TO_VI, 0);
    char *AddVenc = (char*)GetConfParaString(&stConfParser, SAMPLE_REGION_KEY_ADD_VENC_CHANNEL, NULL);
    if(!strcmp(AddVenc, "yes"))
    {
        pConfig->add_venc_channel = TRUE;
        pConfig->mbAttachToVe = (bool)GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ATTACH_TO_VE, 0);
    }
    else
    {
        pConfig->add_venc_channel = FALSE;
        pConfig->mbAttachToVe = 0;
    }
    pConfig->mChangeDisplayAttrEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_CHANGE_DISPLAY_ATTR_ENABLE, 0);
    alogd("AttachToVi: %d, add_venc_channel: %d, AttachToVe: %d, ChangeDisplayAttrEnable: %d",
        pConfig->mbAttachToVi, pConfig->add_venc_channel, pConfig->mbAttachToVe,
        pConfig->mChangeDisplayAttrEnable);

    pConfig->encoder_count = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ENCODER_COUNT, 5000);
    pConfig->bit_rate = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_BIT_RATE, 0);
    char *EncoderType = (char*)GetConfParaString(&stConfParser, SAMPLE_REGION_KEY_ENCODERTYPE, NULL);
    if(!strcmp(EncoderType, "H.264"))
    {
        pConfig->EncoderType = PT_H264;
    }
    else if(!strcmp(EncoderType, "H.265"))
    {
        pConfig->EncoderType = PT_H265;
    }
    else if(!strcmp(EncoderType, "MJPEG"))
    {
        pConfig->EncoderType = PT_MJPEG;
    }
    else
    {
        alogw("unsupported venc type:%p,encoder type turn to H.264!",EncoderType);
        pConfig->EncoderType = PT_H264;
    }
    char *pStr = (char *)GetConfParaString(&stConfParser, SAMPLE_REGION_KEY_OUTPUT_FILE_PATH, NULL);
    strncpy(pConfig->OutputFilePath, pStr, MAX_FILE_PATH_SIZE-1);
    pConfig->OutputFilePath[MAX_FILE_PATH_SIZE-1] = '\0';

    pConfig->mOnlineEnable = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ONLINE_ENABLE, 0);
    pConfig->mOnlineShareBufNum = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_ONLINE_SHARE_BUF_NUM, 0);
    alogd("OnlineEnable: %d, OnlineShareBufNum: %d", pConfig->mOnlineEnable, pConfig->mOnlineShareBufNum);

    pConfig->mVippDevID = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_VIPP_DEV_ID, 0);
    pConfig->mVeChnID = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_VE_CH_ID, 0);
    alogd("vipp: %d, VeCh: %d", pConfig->mVippDevID, pConfig->mVeChnID);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleRegionContext != NULL)
    {
        cdx_sem_up(&gpSampleRegionContext->mSemExit);
    }
}


static ERRORTYPE SampleRegion_VOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleRegionContext *pContext = ( SampleRegionContext*)cookie;
    if(MOD_ID_VOU == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mVOChn)
        {
            aloge("fatal error! VO chnId[%d]!=[%d]", pChn->mChnId, pContext->mVOChn);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                aloge("fatal error! sample_vi2vo use binding mode!");
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
                //postEventFromNative(this, event, 0, 0, pEventData);
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

static ERRORTYPE SampleRegion_CLOCKCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    alogw("clock channel[%d] has some event[0x%x]", pChn->mChnId, event);
    return SUCCESS;
}

static void *GetEncoderFrameThread(void * pArg)
{
    SampleRegionContext *pContext = (SampleRegionContext*)pArg;
    int count = 0;
    VENC_STREAM_S VencFrame;
    VENC_PACK_S venc_pack;
    VencFrame.mPackCount = 1;
    VencFrame.mpPack = &venc_pack;
    pContext->mVI2VEncChn = pContext->mVIChn + 1;
    int eRet =  AW_MPI_VI_CreateVirChn(pContext->mVIDev, pContext->mVI2VEncChn, NULL);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_VI_CreateVirChn failed");
    }

    MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mVIDev, pContext->mVI2VEncChn};
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVEChn};
    AW_MPI_SYS_Bind(&ViChn,&VeChn);

    eRet = AW_MPI_VI_EnableVirChn(pContext->mVIDev, pContext->mVI2VEncChn);
    if(eRet != SUCCESS)
    {
        aloge("error:enablecirchn failed");
    }

    AW_MPI_VENC_StartRecvPic(pContext->mVEChn);

    //save the video
    while(FALSE == pContext->mbEncThreadExitFlag)
    {
        if(AW_MPI_VENC_GetStream(pContext->mVEChn, &VencFrame, 4000) < 0)
        {
           aloge("get first frame failed!\n");
            continue;
        }
        else
        {
            if(VencFrame.mpPack != NULL && VencFrame.mpPack->mLen0)
            {
                fwrite(VencFrame.mpPack->mpAddr0, 1, VencFrame.mpPack->mLen0, pContext->mOutputFileFp);
            }
            if(VencFrame.mpPack != NULL && VencFrame.mpPack->mLen1)
            {
                fwrite(VencFrame.mpPack->mpAddr1, 1, VencFrame.mpPack->mLen1, pContext->mOutputFileFp);
            }

            AW_MPI_VENC_ReleaseStream(pContext->mVEChn, &VencFrame);
            count++;
        }
    }

    alogd("get [%d]encoded frames!", count);

    AW_MPI_VENC_StopRecvPic(pContext->mVEChn);
    AW_MPI_VI_DisableVirChn(pContext->mVIDev, pContext->mVI2VEncChn); 

    AW_MPI_VENC_ResetChn(pContext->mVEChn);
    AW_MPI_VENC_DestroyChn(pContext->mVEChn);
    AW_MPI_VI_DestroyVirChn(pContext->mVIDev, pContext->mVI2VEncChn);
    
    return NULL;
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
#if 0
    FILE* pOsdFile = fopen("/tmp/osd.file", "rb");
    int nRdBytes = fread(pBuf, 1, nSize, pOsdFile);
    if(nRdBytes != nSize)
    {
        aloge("fatal error! read argb1555 osd file wrong!%d!=%d", nRdBytes, nSize);
    }
    fclose(pOsdFile);
#endif
}

static void GenerateARGB8888(void* pBuf, int nSize)
{
    unsigned int nA0Color = 0x80FF0000;
    unsigned int nA1Color = 0xFFFF0000;
    unsigned int *pInt = (unsigned int*)pBuf;
    int i = 0;
    for(i=0; i< nSize/8; i++)
    {
        *pInt = nA0Color;
        pInt++;
    }
    for(i=0; i< nSize/8; i++)
    {
        *pInt = nA1Color;
        pInt++;
    }
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleRegionContext *pContext = (SampleRegionContext *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        switch(event)
        {
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                VencIsp2VeParam *pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                if (pIsp2VeParam)
                {
                    sEncppSharpParam *pSharpParam = &pIsp2VeParam->mSharpParam;
                    ISP_DEV mIspDev = 0;
                    ret = AW_MPI_VI_GetIspDev(pContext->mVIDev, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pContext->mVIDev, ret);
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
                VencVe2IspParam *pVe2IspParam = (VencVe2IspParam *)pEventData;
                if (pVe2IspParam)
                {
                    ISP_DEV mIspDev = 0;
                    ret = AW_MPI_VI_GetIspDev(pContext->mVIDev, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pContext->mVIDev, ret);
                        return -1;
                    }
                    alogv("update Ve2IspParam, route Isp[%d]-Vipp[%d]-VencChn[%d]", mIspDev, pContext->mVIDev, pContext->mVEChn);

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

int main(int argc, char **argv)
{
    int result = 0;
    BOOL bAddVencChannel = FALSE;
    bool bDisplayFlag = false;
    ERRORTYPE eRet = SUCCESS;
    
    SampleRegionContext *pContext = (SampleRegionContext*)malloc(sizeof(SampleRegionContext));
    if(NULL == pContext)
    {
        alogd("malloc fail!");
        return -1;
    }
    initSampleRegionContext(pContext);
    gpSampleRegionContext = pContext;

    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        result = -1;
        goto _exit;
        
    }
    char *pConfigFilePath = NULL;
    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    if(loadSampleRegionConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
  
    alogw("=============================================>");
    if(pContext->mConfigPara.mDispWidth > 0 && pContext->mConfigPara.mDispHeight > 0)
    {
        bDisplayFlag = true;
    }
    if(bDisplayFlag)
    {
        alogd("enable display, VI_to_VO");
    }
    bAddVencChannel = pContext->mConfigPara.add_venc_channel;
    if(bAddVencChannel)
    {
        alogd("Had add the VencChanel, add VI_To_Venc");
    }

    alogw("=============================================>");

    if (signal(SIGINT, handle_exit) == SIG_ERR)        
    {
        perror("error:can't catch SIGSEGV");
    }

    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    if (pContext->mConfigPara.mOnlineEnable)
        pContext->mVIDev = 0; // only vipp0 support online.
    else
        pContext->mVIDev = HVIDEO(pContext->mConfigPara.mVippDevID, 0);

    pContext->mVIChn = 0;
    eRet = AW_MPI_VI_CreateVipp(pContext->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_VI_CreateVipp failed");
    }

    VI_ATTR_S attr;
    eRet = AW_MPI_VI_GetVippAttr(pContext->mVIDev, &attr);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_VI_GetVippAttr failed");
    }
    memset(&attr, 0, sizeof(VI_ATTR_S));
    if (pContext->mConfigPara.mOnlineEnable)
    {
        attr.mOnlineEnable = 1;
        attr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    }
    attr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    attr.memtype = V4L2_MEMORY_MMAP;
    attr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mPicFormat);
    attr.format.field = V4L2_FIELD_NONE;
    attr.format.colorspace = V4L2_COLORSPACE_JPEG;
    attr.format.width = pContext->mConfigPara.mCaptureWidth;
    attr.format.height = pContext->mConfigPara.mCaptureHeight;
    attr.nbufs = 5;//10;
    attr.nplanes = 2;
    attr.fps = pContext->mConfigPara.mFrameRate;
    attr.mbEncppEnable = TRUE;
    eRet = AW_MPI_VI_SetVippAttr(pContext->mVIDev, &attr);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_SetVippAttr failed");
    }
    AW_MPI_ISP_Run(0);

    //AW_MPI_ISP_SetMirror(0, 0);
    eRet = AW_MPI_VI_EnableVipp(pContext->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_VI_EnableVipp failed");
    }

    eRet = AW_MPI_VI_CreateVirChn(pContext->mVIDev, pContext->mVIChn, NULL);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_VI_CreateVirChn failed ");
    }

#if (MPPCFG_VO!=0)
    if(bDisplayFlag)
    {
        //enable vo dev
        pContext->mVoDev = 0;
        AW_MPI_VO_Enable(pContext->mVoDev);
        AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
        AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);
        VO_PUB_ATTR_S spPubAttr;
        memset(&spPubAttr, 0, sizeof(VO_PUB_ATTR_S));
        AW_MPI_VO_GetPubAttr(pContext->mVoDev, &spPubAttr);
        spPubAttr.enIntfType = VO_INTF_LCD;
        spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
        AW_MPI_VO_SetPubAttr(pContext->mVoDev, &spPubAttr);

        //enable vo layer
        int hlay0 = 0;
        while(hlay0 < VO_MAX_LAYER_NUM)
        {
            if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
            {
                break;
            }
            ++hlay0;
        }
        if(hlay0 == VO_MAX_LAYER_NUM)
        {
            aloge("error: enable video layer failed");
        }
        pContext->mVoLayer = hlay0;
        AW_MPI_VO_GetVideoLayerAttr(pContext->mVoLayer, &pContext->mLayerAttr);
        pContext->mLayerAttr.stDispRect.X = 0;
        pContext->mLayerAttr.stDispRect.Y = 0;
        pContext->mLayerAttr.stDispRect.Width = pContext->mConfigPara.mDispWidth;
        pContext->mLayerAttr.stDispRect.Height = pContext->mConfigPara.mDispHeight;
        AW_MPI_VO_SetVideoLayerAttr(pContext->mVoLayer, &pContext->mLayerAttr);

        //creat vo channel and clock channel
        BOOL bSuccessFlag = FALSE;
        pContext->mVOChn = 0;
        while(pContext->mVOChn < VO_MAX_CHN_NUM)
        {
            eRet = AW_MPI_VO_CreateChn(pContext->mVoLayer, pContext->mVOChn);
            if(eRet == SUCCESS)
            {
                bSuccessFlag = TRUE;
                break;
            }
            else if(eRet == ERR_VO_CHN_NOT_DISABLE)
            {
                alogd("vo channel[%d] is exist, find next", pContext->mVOChn);
                pContext->mVOChn++;
            }
            else
            {
                aloge("error:create vo channel[%d] ret[0x%x]", pContext->mVOChn, eRet);
            }
        }
        if(FALSE == bSuccessFlag)
        {
            pContext->mVOChn = MM_INVALID_CHN;
            aloge("error: create vo channel failed");
        }

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void *)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleRegion_VOCallbackWrapper;
        AW_MPI_VO_RegisterCallback(pContext->mVoLayer, pContext->mVOChn, &cbInfo);
        AW_MPI_VO_SetChnDispBufNum(pContext->mVoLayer, pContext->mVOChn, 0);
    }
#endif

    if(bAddVencChannel)
    {
        pContext->mVEChn = pContext->mConfigPara.mVeChnID;
        memset(&pContext->mVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
        if (pContext->mConfigPara.mOnlineEnable)
        {
            pContext->mVEncChnAttr.VeAttr.mOnlineEnable = 1;
            pContext->mVEncChnAttr.VeAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
        }
        SIZE_S wantedVideoSize = {pContext->mConfigPara.mCaptureWidth, pContext->mConfigPara.mCaptureHeight};
        SIZE_S videosize = {pContext->mConfigPara.mCaptureWidth, pContext->mConfigPara.mCaptureHeight};
        PAYLOAD_TYPE_E videoCodec = pContext->mConfigPara.EncoderType;
        pContext->mVEncChnAttr.VeAttr.Type = videoCodec;
        pContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pContext->mConfigPara.mFrameRate;
        pContext->mVEncChnAttr.VeAttr.SrcPicWidth = videosize.Width;
        pContext->mVEncChnAttr.VeAttr.SrcPicHeight = videosize.Height;
        pContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
        pContext->mVEncChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.mPicFormat;
        pContext->mVEncChnAttr.VeAttr.mColorSpace = attr.format.colorspace;
        pContext->mVEncChnAttr.EncppAttr.mbEncppEnable = TRUE;

        pContext->mVencRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
        pContext->mVencRcParam.sensor_type = VENC_ST_EN_WDR;

        int wantedVideoBitRate = pContext->mConfigPara.bit_rate;
        if(PT_H264 == pContext->mVEncChnAttr.VeAttr.Type)
        {
            pContext->mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
            pContext->mVEncChnAttr.VeAttr.AttrH264e.Profile = 2;
            pContext->mVEncChnAttr.VeAttr.AttrH264e.mLevel = VENC_H264Level51;
            pContext->mVEncChnAttr.VeAttr.AttrH264e.PicWidth = wantedVideoSize.Width;
            pContext->mVEncChnAttr.VeAttr.AttrH264e.PicHeight = wantedVideoSize.Height;
            pContext->mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
            pContext->mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->mVEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate = wantedVideoBitRate;
            pContext->mVencRcParam.ParamH264Cbr.mMaxQp = 51;
            pContext->mVencRcParam.ParamH264Cbr.mMinQp = 1;
            pContext->mVencRcParam.ParamH264Cbr.mMaxPqp = 50;
            pContext->mVencRcParam.ParamH264Cbr.mMinPqp = 10;
            pContext->mVencRcParam.ParamH264Cbr.mQpInit = 35;
            pContext->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 0;
        }
        else if(PT_H265 == pContext->mVEncChnAttr.VeAttr.Type)
        {
            pContext->mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
            pContext->mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
            pContext->mVEncChnAttr.VeAttr.AttrH265e.mLevel = VENC_H265Level62;
            pContext->mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = wantedVideoSize.Width;
            pContext->mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = wantedVideoSize.Height;
            pContext->mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
            pContext->mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->mVEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate = wantedVideoBitRate;
            pContext->mVencRcParam.ParamH265Cbr.mMaxQp = 51;
            pContext->mVencRcParam.ParamH265Cbr.mMinQp = 1;
            pContext->mVencRcParam.ParamH265Cbr.mMaxPqp = 50;
            pContext->mVencRcParam.ParamH265Cbr.mMinPqp = 10;
            pContext->mVencRcParam.ParamH265Cbr.mQpInit = 35;
            pContext->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 0;
        }
        else if(PT_MJPEG== pContext->mVEncChnAttr.VeAttr.Type)
        {
            pContext->mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
            pContext->mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth = wantedVideoSize.Width;
            pContext->mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = wantedVideoSize.Height;
            pContext->mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = wantedVideoBitRate;
        }

        eRet =  AW_MPI_VENC_CreateChn(pContext->mVEChn, &pContext->mVEncChnAttr);
        if(eRet != SUCCESS)
        {
            aloge("error:AW_MPI_VENC_CreateChn failed");
            goto _exit;
        
        }

        AW_MPI_VENC_SetRcParam(pContext->mVEChn, &pContext->mVencRcParam);

        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = pContext->mConfigPara.mFrameRate;
        stFrameRate.DstFrmRate = pContext->mConfigPara.mFrameRate;
        AW_MPI_VENC_SetFrameRate(pContext->mVEChn, &stFrameRate);

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mVEChn, &cbInfo);

        VencHeaderData vencheader;
        pContext->mOutputFileFp = fopen(pContext->mConfigPara.OutputFilePath,"wb+");
        if(!pContext->mOutputFileFp)
        {
            aloge("error:can not open the file[%s]", pContext->mConfigPara.OutputFilePath);
            goto _exit;
        }
        if(PT_H264 == pContext->mVEncChnAttr.VeAttr.Type)
        {
            result = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVEChn, &vencheader);
            if (SUCCESS == result)
            {
                if(vencheader.nLength)
                {
                    fwrite(vencheader.pBuffer, vencheader.nLength, 1, pContext->mOutputFileFp);
                }
            }
            else
            {
                alogd("AW_MPI_VENC_GetH264SpsPpsInfo failed!\n");
                result = -1;
            }
        }
        else if(PT_H265 == pContext->mVEncChnAttr.VeAttr.Type)
        {
            result = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVEChn, &vencheader);
            if (SUCCESS == result)
            {
                if(vencheader.nLength)
                {
                    fwrite(vencheader.pBuffer, vencheader.nLength, 1, pContext->mOutputFileFp);
                }
            }
            else
            {
                alogd("AW_MPI_VENC_GetH265SpsPpsInfo failed!\n");
                result = -1;
            }
        }
    
    }
     
    //bind clock,vo, viChn 	
    MPP_CHN_S VOChn = {MOD_ID_VOU, pContext->mVoLayer, pContext->mVOChn}; 	
    MPP_CHN_S VIChn = {MOD_ID_VIU, pContext->mVIDev, pContext->mVIChn};
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVEChn};

#if (MPPCFG_VO!=0)
    if(bDisplayFlag)
    {
        AW_MPI_SYS_Bind(&VIChn, &VOChn);
        //start vo, clock, vi_channel andvvenc
        eRet = AW_MPI_VI_EnableVirChn(pContext->mVIDev, pContext->mVIChn);
        if(eRet != SUCCESS)
        {
            aloge("error:enablecirchn failed");
        }
        AW_MPI_VO_StartChn(pContext->mVoLayer, pContext->mVOChn);
    }
#endif

    if(bAddVencChannel)
    {
        if(pthread_create(&pContext->mEncThreadId, NULL, GetEncoderFrameThread, pContext) < 0)
        {
            aloge("error: the pthread can not be created");
        }
        alogw("the pthread[0x%x] of venc had created", pContext->mEncThreadId);
    }
    
    //test the region
    pContext->mOverlayHandle = 0;
    if(pContext->mConfigPara.overlay_h != 0 && pContext->mConfigPara.overlay_w != 0)
    {
        RGN_ATTR_S stRegion;
        BITMAP_S stBmp;
        int nSize = 0;
        RGN_CHN_ATTR_S stRgnChnAttr ;

        if(pContext->mConfigPara.mbAttachToVi)
        {
            alogd("vipp test overlay");
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            stRegion.enType = OVERLAY_RGN;
            stRegion.unAttr.stOverlay.mPixelFmt = pContext->mConfigPara.mBitmapFormat;   //MM_PIXEL_FORMAT_RGB_8888;
            //stRegion.unAttr.stOverlay.mSize = {(unsigned int)pContext->mConfigPara.overlay_w, (unsigned int)pContext->mConfigPara.overlay_h};
            stRegion.unAttr.stOverlay.mSize.Width = (unsigned int)pContext->mConfigPara.overlay_w;
            stRegion.unAttr.stOverlay.mSize.Height = (unsigned int)pContext->mConfigPara.overlay_h;
            AW_MPI_RGN_Create(pContext->mOverlayHandle, &stRegion);
            
            memset(&stBmp, 0, sizeof(BITMAP_S));
            stBmp.mPixelFormat = stRegion.unAttr.stOverlay.mPixelFmt;
            stBmp.mWidth = stRegion.unAttr.stOverlay.mSize.Width;
            stBmp.mHeight = stRegion.unAttr.stOverlay.mSize.Height;
            nSize = BITMAP_S_GetdataSize(&stBmp);
            stBmp.mpData = malloc(nSize);
            if(NULL == stBmp.mpData)        
            {            
                aloge("fatal error! malloc fail!");        
            }
            memset(stBmp.mpData, 0xCC, nSize);
            AW_MPI_RGN_SetBitMap(pContext->mOverlayHandle, &stBmp);
            free(stBmp.mpData);

            stRgnChnAttr.bShow = TRUE;
            stRgnChnAttr.enType = stRegion.enType;
            //stRgnChnAttr.unChnAttr.stOverlayChn.stPoint = {pContext->mConfigPara.overlay_x, pContext->mConfigPara.overlay_y};
            stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X = pContext->mConfigPara.overlay_x;
            stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = pContext->mConfigPara.overlay_y;
            stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = 0;
            stRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha = 0x5C; // global alpha mode value for ARGB1555
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = 60;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUMDIFF_THRESH;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = TRUE;
            AW_MPI_RGN_AttachToChn(pContext->mOverlayHandle,  &VIChn, &stRgnChnAttr);
        }

        if(pContext->mConfigPara.mbAttachToVe)
        {
            alogd("ve test overlay");
            //add to venchn
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            stRegion.enType = OVERLAY_RGN;
            stRegion.unAttr.stOverlay.mPixelFmt = pContext->mConfigPara.mBitmapFormat;   //MM_PIXEL_FORMAT_RGB_8888;
            //stRegion.unAttr.stOverlay.mSize = {(unsigned int)pContext->mConfigPara.overlay_w, (unsigned int)pContext->mConfigPara.overlay_h};
            stRegion.unAttr.stOverlay.mSize.Width = pContext->mConfigPara.overlay_w;
            stRegion.unAttr.stOverlay.mSize.Height = pContext->mConfigPara.overlay_h;
            //pContext->mOverlayHandle += 1;
            AW_MPI_RGN_Create(pContext->mOverlayHandle + 1, &stRegion);
            
            memset(&stBmp, 0, sizeof(BITMAP_S));
            stBmp.mPixelFormat = stRegion.unAttr.stOverlay.mPixelFmt;
            stBmp.mWidth = stRegion.unAttr.stOverlay.mSize.Width;
            stBmp.mHeight = stRegion.unAttr.stOverlay.mSize.Height;
            nSize = BITMAP_S_GetdataSize(&stBmp);
            stBmp.mpData = malloc(nSize);
            if(NULL == stBmp.mpData)        
            {            
                aloge("fatal error! malloc fail!");        
            }
            if(MM_PIXEL_FORMAT_RGB_1555 == pContext->mConfigPara.mBitmapFormat)
            {
                GenerateARGB1555(stBmp.mpData, nSize);
            }
            else if(MM_PIXEL_FORMAT_RGB_8888 == pContext->mConfigPara.mBitmapFormat)
            {
                GenerateARGB8888(stBmp.mpData, nSize);
            }
            else
            {
                aloge("fatal error! unsupport bitmap format:%d", pContext->mConfigPara.mBitmapFormat);
            }
            AW_MPI_RGN_SetBitMap(pContext->mOverlayHandle + 1, &stBmp);
            free(stBmp.mpData);

            stRgnChnAttr.bShow = TRUE;
            stRgnChnAttr.enType = stRegion.enType;
            //stRgnChnAttr.unChnAttr.stOverlayChn.stPoint = {pContext->mConfigPara.overlay_x, pContext->mConfigPara.overlay_y};
            stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X = pContext->mConfigPara.overlay_x+160;
            stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = pContext->mConfigPara.overlay_y+160;
            stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = 0;
            stRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha = 0x40; // global alpha mode value for ARGB1555
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = 60;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUMDIFF_THRESH;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = FALSE;
            alogd("overlay attach to ve");
            AW_MPI_RGN_AttachToChn(pContext->mOverlayHandle + 1,  &VeChn, &stRgnChnAttr);
            alogd("overlay attach to ve done");

            if (pContext->mConfigPara.mVencRoiEnable)
            {
                //use roi to enhance region encode quality.
                VENC_ROI_CFG_S stVencRoiCfg;
                memset(&stVencRoiCfg, 0, sizeof(VENC_ROI_CFG_S));
                stVencRoiCfg.Index = 0;
                stVencRoiCfg.bEnable = TRUE;
                stVencRoiCfg.bAbsQp = TRUE;
                stVencRoiCfg.Qp = 20;
                stVencRoiCfg.Rect.X = stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X;
                stVencRoiCfg.Rect.Y = stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y;
                stVencRoiCfg.Rect.Width = stRegion.unAttr.stOverlay.mSize.Width;
                stVencRoiCfg.Rect.Height = stRegion.unAttr.stOverlay.mSize.Height;
                AW_MPI_VENC_SetRoiCfg(pContext->mVEChn, &stVencRoiCfg);
            }
        }
    }

    pContext->mCoverHandle = 10;
    if(pContext->mConfigPara.cover_h != 0 && pContext->mConfigPara.cover_w != 0)
    {
        //add to vichn
        RGN_ATTR_S stRegion;
        RGN_CHN_ATTR_S stRgnChnAttr;

        if(pContext->mConfigPara.mbAttachToVi)
        {
            alogd("vipp test cover");
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            stRegion.enType = COVER_RGN;
            AW_MPI_RGN_Create(pContext->mCoverHandle, &stRegion);
            
            stRgnChnAttr.bShow = TRUE;
            stRgnChnAttr.enType = stRegion.enType;
            stRgnChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;
            //stRgnChnAttr.unChnAttr.stCoverChn.stRect = {pContext->mConfigPara.cover_x, pContext->mConfigPara.cover_y, (unsigned int)pContext->mConfigPara.cover_w, (unsigned int)pContext->mConfigPara.cover_h};
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.X = pContext->mConfigPara.cover_x;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y = pContext->mConfigPara.cover_y;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width = pContext->mConfigPara.cover_w;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height= pContext->mConfigPara.cover_h;    
            stRgnChnAttr.unChnAttr.stCoverChn.mColor = 0;
            stRgnChnAttr.unChnAttr.stCoverChn.mLayer = 0;
            AW_MPI_RGN_AttachToChn(pContext->mCoverHandle,  &VIChn, &stRgnChnAttr);
        }
        if(pContext->mConfigPara.mbAttachToVe)
        {
            alogd("ve test cover");
            //add to venc
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            stRegion.enType = COVER_RGN;
           // pContext->mCoverHandle += 1;
            AW_MPI_RGN_Create(pContext->mCoverHandle + 1, &stRegion);
            
            stRgnChnAttr.bShow = TRUE;
            stRgnChnAttr.enType = stRegion.enType;
            stRgnChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;
            //stRgnChnAttr.unChnAttr.stCoverChn.stRect = {pContext->mConfigPara.cover_x, pContext->mConfigPara.cover_y, (unsigned int)pContext->mConfigPara.cover_w, (unsigned int)pContext->mConfigPara.cover_h};
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.X = pContext->mConfigPara.cover_x + 96;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y = pContext->mConfigPara.cover_y + 96;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width = pContext->mConfigPara.cover_w;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height= pContext->mConfigPara.cover_h;
            stRgnChnAttr.unChnAttr.stCoverChn.mColor = 255;
            stRgnChnAttr.unChnAttr.stCoverChn.mLayer = 0;
            alogd("cover attach to ve");
            AW_MPI_RGN_AttachToChn(pContext->mCoverHandle + 1,  &VeChn, &stRgnChnAttr);
            alogd("cover attach to ve done");
        }
    }

    pContext->mOrlHandle = 20;
    if(pContext->mConfigPara.orl_h != 0 && pContext->mConfigPara.orl_w != 0)
    {
        //add to vichn
        RGN_ATTR_S stRegion;
        RGN_CHN_ATTR_S stRgnChnAttr;

        if(pContext->mConfigPara.mbAttachToVi)
        {
            alogd("vipp test orl");
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            stRegion.enType = ORL_RGN;
            AW_MPI_RGN_Create(pContext->mOrlHandle, &stRegion);
            
            stRgnChnAttr.bShow = TRUE;
            stRgnChnAttr.enType = stRegion.enType;
            stRgnChnAttr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.X = pContext->mConfigPara.orl_x;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y = pContext->mConfigPara.orl_y;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width = (unsigned int)pContext->mConfigPara.orl_w;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height = (unsigned int)pContext->mConfigPara.orl_h;
            stRgnChnAttr.unChnAttr.stOrlChn.mColor = 0x0000FF00;
            stRgnChnAttr.unChnAttr.stOrlChn.mThick = (unsigned int)pContext->mConfigPara.orl_thick;
            stRgnChnAttr.unChnAttr.stOrlChn.mLayer = 0;
            alogd("orl attach to vipp");
            AW_MPI_RGN_AttachToChn(pContext->mOrlHandle,  &VIChn, &stRgnChnAttr);
            alogd("orl attach to vipp done");
        }
        if(pContext->mConfigPara.mbAttachToVe)
        {
            alogd("ve test orl");
            //add to venc
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            stRegion.enType = ORL_RGN;
            AW_MPI_RGN_Create(pContext->mOrlHandle + 1, &stRegion);
            
            stRgnChnAttr.bShow = TRUE;
            stRgnChnAttr.enType = stRegion.enType;
            stRgnChnAttr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.X = pContext->mConfigPara.orl_x + 96;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y = pContext->mConfigPara.orl_y + 96;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width = (unsigned int)pContext->mConfigPara.orl_w;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height = (unsigned int)pContext->mConfigPara.orl_h;
            stRgnChnAttr.unChnAttr.stOrlChn.mColor = 0x0000FFFF;
            stRgnChnAttr.unChnAttr.stOrlChn.mThick = 6;
            stRgnChnAttr.unChnAttr.stOrlChn.mLayer = 0;
            AW_MPI_RGN_AttachToChn(pContext->mOrlHandle + 1,  &VeChn, &stRgnChnAttr);
        }
    }
    
    if(pContext->mConfigPara.mChangeDisplayAttrEnable)
    {
        sleep(5);
        alogw("now, let us change the displayattr");

        RGN_CHN_ATTR_S stRgnChnAttr;
        //change the overlay
        if(pContext->mConfigPara.mbAttachToVi)
        {
            eRet = AW_MPI_RGN_GetDisplayAttr(pContext->mOverlayHandle,  &VIChn, &stRgnChnAttr);
            if(SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X += 96;
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = 0;
                AW_MPI_RGN_SetDisplayAttr(pContext->mOverlayHandle, &VIChn, &stRgnChnAttr);
            }
            else
            {
                alogw("not find ViChn[%d,%d,%d] for overlayHandle[%d]", VIChn.mModId, VIChn.mDevId, VIChn.mChnId, pContext->mOverlayHandle);
            }
        }
        if(pContext->mConfigPara.mbAttachToVe)
        {
            eRet = AW_MPI_RGN_GetDisplayAttr(pContext->mOverlayHandle + 1,  &VeChn, &stRgnChnAttr);
            if(SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X += 96;
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y += 96;
                alogd("overlay attach to ve again");
                AW_MPI_RGN_SetDisplayAttr(pContext->mOverlayHandle + 1, &VeChn, &stRgnChnAttr);
                alogd("overlay attach to ve again done");
            }
            else
            {
                alogw("not find VencChn[%d,%d,%d] for overlayHandle[%d]", VeChn.mModId, VeChn.mDevId, VeChn.mChnId, pContext->mOverlayHandle);
            }
        }

        //change the cover
        if(pContext->mConfigPara.mbAttachToVi)
        {
            eRet = AW_MPI_RGN_GetDisplayAttr(pContext->mCoverHandle, &VIChn, &stRgnChnAttr);
            if(SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.X = 0;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y = 0;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width += 48;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height += 48;
                AW_MPI_RGN_SetDisplayAttr(pContext->mCoverHandle, &VIChn, &stRgnChnAttr);
            }
            else
            {
                alogw("not find ViChn[%d,%d,%d] for coverHandle[%d]", VIChn.mModId, VIChn.mDevId, VIChn.mChnId, pContext->mCoverHandle);
            }
        }

        if(pContext->mConfigPara.mbAttachToVe)
        {
            eRet = AW_MPI_RGN_GetDisplayAttr(pContext->mCoverHandle + 1, &VeChn, &stRgnChnAttr);
            if(SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.X += 208;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y += 208;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width += 16;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height += 16;
                alogd("cover attach to ve again");
                AW_MPI_RGN_SetDisplayAttr(pContext->mCoverHandle + 1, &VeChn, &stRgnChnAttr);  
                alogd("cover attach to ve again done");
            }
            else
            {
                alogw("not find VencChn[%d,%d,%d] for coverHandle[%d]", VeChn.mModId, VeChn.mDevId, VeChn.mChnId, pContext->mCoverHandle);
            }
        }

        //change the orl
        if(pContext->mConfigPara.mbAttachToVi)
        {
            eRet = AW_MPI_RGN_GetDisplayAttr(pContext->mOrlHandle, &VIChn, &stRgnChnAttr);
            if(SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.X += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height += 20;
                alogd("orl attach to vipp again");
                AW_MPI_RGN_SetDisplayAttr(pContext->mOrlHandle, &VIChn, &stRgnChnAttr);
                alogd("orl attach to vipp again done");
            }
            else
            {
                alogw("not find ViChn[%d,%d,%d] for OrlHandle[%d]", VIChn.mModId, VIChn.mDevId, VIChn.mChnId, pContext->mOrlHandle);
            }
        }

        if(pContext->mConfigPara.mbAttachToVe)
        {
            eRet = AW_MPI_RGN_GetDisplayAttr(pContext->mOrlHandle + 1, &VeChn, &stRgnChnAttr);
            if(SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.X += 30;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y += 30;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height += 20;
                AW_MPI_RGN_SetDisplayAttr(pContext->mOrlHandle + 1, &VeChn, &stRgnChnAttr);  
            }
            else
            {
                alogw("not find VencChn[%d,%d,%d] for OrlHandle[%d]", VeChn.mModId, VeChn.mDevId, VeChn.mChnId, pContext->mOrlHandle);
            }
        }
    }

    if(pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration * 1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    if(bAddVencChannel)
    {
        pContext->mbEncThreadExitFlag = TRUE;
        int eError = 0;
        pthread_join(pContext->mEncThreadId, (void*) &eError);
    }

    AW_MPI_RGN_DetachFromChn(pContext->mOverlayHandle, &VIChn);
    AW_MPI_RGN_Destroy(pContext->mOverlayHandle);
    
    AW_MPI_RGN_DetachFromChn(pContext->mCoverHandle, &VIChn);
    AW_MPI_RGN_Destroy(pContext->mCoverHandle);

    AW_MPI_RGN_DetachFromChn(pContext->mOrlHandle, &VIChn);
    AW_MPI_RGN_Destroy(pContext->mOrlHandle);

    AW_MPI_RGN_DetachFromChn(pContext->mOverlayHandle + 1, &VeChn);
    AW_MPI_RGN_Destroy(pContext->mOverlayHandle + 1);
    
    AW_MPI_RGN_DetachFromChn(pContext->mCoverHandle + 1, &VeChn);
    AW_MPI_RGN_Destroy(pContext->mCoverHandle + 1);

    AW_MPI_RGN_DetachFromChn(pContext->mOrlHandle + 1, &VeChn);
    AW_MPI_RGN_Destroy(pContext->mOrlHandle + 1);
    
_exit: 

    //stop vo channel, clock channel, vi channel,
    AW_MPI_VENC_StopRecvPic(pContext->mVEChn);
#if (MPPCFG_VO!=0)
    AW_MPI_VO_StopChn(pContext->mVoLayer, pContext->mVOChn);
#endif
 	AW_MPI_VI_DisableVirChn(pContext->mVIDev, pContext->mVIChn);
    AW_MPI_VI_DisableVirChn(pContext->mVIDev, pContext->mVIChn + 1);    
    AW_MPI_VENC_ResetChn(pContext->mVEChn);
#if (MPPCFG_VO!=0)
    AW_MPI_VO_DestroyChn(pContext->mVoLayer, pContext->mVOChn);
#endif

    pContext->mVOChn = MM_INVALID_CHN;
 	pContext->mClockChn = MM_INVALID_CHN;
    AW_MPI_VENC_DestroyChn(pContext->mVEChn);
    AW_MPI_VI_DestroyVirChn(pContext->mVIDev, pContext->mVIChn);
    AW_MPI_VI_DestroyVirChn(pContext->mVIDev, pContext->mVIChn + 1);
    AW_MPI_VI_DisableVipp(pContext->mVIDev);
    
    AW_MPI_ISP_Stop(0);
    
    AW_MPI_VI_DestroyVipp(pContext->mVIDev);
    pContext->mVIDev = MM_INVALID_DEV;
    pContext->mVIChn = MM_INVALID_CHN;
    pContext->mVEChn = MM_INVALID_CHN;

#if (MPPCFG_VO!=0)
    //disable vo layer
    AW_MPI_VO_DisableVideoLayer(pContext->mVoLayer);
    pContext->mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
    //disalbe vo dev
    AW_MPI_VO_Disable(pContext->mVoDev);
    pContext->mVoDev = -1;
#endif
    //exit mpp system
    AW_MPI_SYS_Exit();
    if(pContext->mOutputFileFp)
    {
        fclose(pContext->mOutputFileFp);
        pContext->mOutputFileFp = NULL;
    }

    destroySampleRegionContext(pContext);
    free(pContext);
    pContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}


