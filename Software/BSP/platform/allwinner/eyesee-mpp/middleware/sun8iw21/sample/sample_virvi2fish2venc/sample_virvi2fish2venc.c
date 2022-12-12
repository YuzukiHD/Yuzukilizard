/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_virvi2fish2venc.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/1/5
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

#define LOG_TAG "sample_virvi2fish2venc"
#include <plat_log.h>
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
#include <ConfigOption.h>
#include "media/mpi_sys.h"
#include "media/mpi_vi.h"
#include "media/mpi_ise.h"
#include "media/mpi_venc.h"
#include "media/mpi_isp.h"
#include <mpi_videoformat_conversion.h>
#include <confparser.h>
#include "sample_virvi2fish2venc.h"
#include "sample_virvi2fish2venc_config.h"

#define Len_110             1   //lens angle in undistort mode
#define Len_130             0
#define Fish_Len_1          0
#define Fish_Len_2          1

typedef struct awVI_pCap_S {
    VI_DEV VI_Dev;
    VI_CHN VI_Chn;
    MPP_CHN_S VI_CHN_S;
    AW_S32 s32MilliSec;
    VIDEO_FRAME_INFO_S pstFrameInfo;
    VI_ATTR_S stAttr;
} VIRVI_Cap_S;

typedef struct awISE_PortCap_S {
    MPP_CHN_S ISE_Port_S;
    FILE *OutputFilePath;
    ISE_CHN_ATTR_S PortAttr;
}ISE_PortCap_S;

typedef struct awISE_pGroupCap_S {
    ISE_GRP ISE_Group;
    MPP_CHN_S ISE_Group_S;
    ISE_GROUP_ATTR_S pGrpAttr;
    ISE_CHN ISE_Port[4];
    ISE_PortCap_S *PortCap_S[4];
}ISE_GroupCap_S;

typedef struct awVENC_pCap_S {
    pthread_t thid;
    VENC_CHN Venc_Chn;
    MPP_CHN_S VENC_CHN_S;
    int EncoderCount;
    FILE *OutputFilePath;
    VENC_STREAM_S VencFrame;
    VENC_CHN_ATTR_S mVEncChnAttr;
    VENC_FRAME_RATE_S stFrameRate;
    VencHeaderData vencheader;
}VENC_Cap_S;

/*MPI VI*/
int aw_vipp_creat(VI_DEV ViDev, VI_ATTR_S *pstAttr)
{
    int ret = -1;
    ret = AW_MPI_VI_CreateVipp(ViDev);
    if(ret < 0)
    {
        aloge("Create vi dev[%d] falied!", ViDev);
        return ret;
    }
    ret = AW_MPI_VI_SetVippAttr(ViDev, pstAttr);
    if(ret < 0)
    {
        aloge("Set vi attr[%d] falied!", ViDev);
        return ret;
    }
    AW_MPI_ISP_Run(0); // 3A ini
    ret = AW_MPI_VI_EnableVipp(ViDev);
    if(ret < 0)
    {
        aloge("Enable vi dev[%d] falied!", ViDev);
        return ret;
    }
    return 0;
}

int aw_virvi_creat(VI_DEV ViDev, VI_CHN ViCh, void *pAttr)
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
    return 0;
}

int aw_vipp_destory(VI_DEV ViDev)
{
    int ret = -1;
    ret = AW_MPI_VI_DisableVipp(ViDev);
    if(ret < 0)
    {
        aloge("Disable vi dev[%d] falied!", ViDev);
        return ret;
    }
    ret = AW_MPI_VI_DestroyVipp(ViDev);
    if(ret < 0)
    {
        aloge("Destory vi dev[%d] falied!", ViDev);
        return ret;
    }
    return 0;
}

int aw_virvi_destory(VI_DEV ViDev, VI_CHN ViCh)
{
    int ret = -1;
    ret = AW_MPI_VI_DestroyVirChn(ViDev, ViCh);
    if(ret < 0)
    {
        aloge("Destory VI Chn failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }
    return 0;
}

/*MPI ISE*/
int aw_iseport_creat(ISE_GRP IseGrp, ISE_CHN IsePort, ISE_CHN_ATTR_S *PortAttr)
{
    int ret = -1;
    ret = AW_MPI_ISE_CreatePort(IseGrp,IsePort,PortAttr);
    if(ret < 0)
    {
        aloge("Create ISE Port failed,IseGrp= %d,IsePort = %d",IseGrp,IsePort);
        return ret ;
    }
    ret = AW_MPI_ISE_SetPortAttr(IseGrp,IsePort,PortAttr);
    if(ret < 0)
    {
        aloge("Set ISE Port Attr failed,IseGrp= %d,IsePort = %d",IseGrp,IsePort);
        return ret ;
    }
    return 0;
}

int aw_iseport_destory(ISE_GRP IseGrp, ISE_CHN IsePort)
{
    int ret = -1;
    ret = AW_MPI_ISE_DestroyPort(IseGrp,IsePort);
    if(ret < 0)
    {
        aloge("Destory ISE Port failed, IseGrp= %d,IsePort = %d",IseGrp,IsePort);
        return ret ;
    }
    return 0;
}

int aw_isegroup_creat(ISE_GRP IseGrp, ISE_GROUP_ATTR_S *pGrpAttr)
{
    int ret = -1;
    ret = AW_MPI_ISE_CreateGroup(IseGrp, pGrpAttr);
    if(ret < 0)
    {
        aloge("Create ISE Group failed, IseGrp= %d",IseGrp);
        return ret ;
    }
    ret = AW_MPI_ISE_SetGrpAttr(IseGrp, pGrpAttr);
    if(ret < 0)
    {
        aloge("Set ISE GrpAttr failed, IseGrp= %d",IseGrp);
        return ret ;
    }
    return 0;
}

int aw_isegroup_destory(ISE_GRP IseGrp)
{
    int ret = -1;
    ret = AW_MPI_ISE_DestroyGroup(IseGrp);
    if(ret < 0)
    {
        aloge("Destroy ISE Group failed, IseGrp= %d",IseGrp);
        return ret ;
    }
    return 0;
}

/*MPI VENC*/
int aw_venc_chn_creat(VENC_Cap_S* pVENCCap)
{
    int ret = -1;
    ret = AW_MPI_VENC_CreateChn(pVENCCap->Venc_Chn, &pVENCCap->mVEncChnAttr);
    if(ret < 0)
    {
        aloge("Create venc channel[%d] falied!", pVENCCap->Venc_Chn);
        return ret ;
    }
    ret = AW_MPI_VENC_SetFrameRate(pVENCCap->Venc_Chn, &pVENCCap->stFrameRate);
    if(ret < 0)
    {
        aloge("Set venc channel[%d] Frame Rate falied!", pVENCCap->Venc_Chn);
        return ret ;
    }
    //AW_MPI_VENC_SetVEFreq(pVENCCap->Venc_Chn, 534);
    return ret;
}

int aw_venc_chn_destory(VENC_Cap_S* pVENCCap)
{
    int ret = -1;
    ret = AW_MPI_VENC_ResetChn(pVENCCap->Venc_Chn);
    if (ret < 0)
    {
        aloge("VENC Chn%d Reset error!",pVENCCap->Venc_Chn);
        return ret ;
    }
    ret = AW_MPI_VENC_DestroyChn(pVENCCap->Venc_Chn);
    if (ret < 0)
    {
        aloge("VENC Chn%d Destroy error!",pVENCCap->Venc_Chn);
        return ret ;
    }
    return ret;
}

static int ParseCmdLine(int argc, char **argv, SampleVirvi2Fish2VencCmdLineParam *pCmdLinePara)
{
    alogd("sample_virvi2fish2venc path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleVirvi2Fish2VencCmdLineParam));
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
                "\t-path /home/sample_virvi2fish2venc.conf\n");
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

static ERRORTYPE loadSampleVirvi2Fish2VencConfig(SampleVirvi2Fish2VencConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;
    char *pStrPixelFormat = NULL,*EncoderType = NULL;
    int VencChnNum = 0,i = 0,ISEPortNum = 0;
    CONFPARSER_S stConfParser;
    char name[256];

    memset(pConfig, 0, sizeof(SampleVirvi2Fish2VencConfig));
    pConfig->AutoTestCount = 1;
    /* VI parameter*/
    pConfig->VIDevConfig.DevId = 0;
    pConfig->VIDevConfig.SrcFrameRate = 60;
    pConfig->VIDevConfig.SrcWidth = 1920;
    pConfig->VIDevConfig.SrcHeight = 1080;
    /*Get ISE parameter*/
    pConfig->ISEGroupConfig.ISEPortNum = 1;
    pConfig->ISEGroupConfig.ISE_GDC_Dewarp_Mode = Warp_LDC;
    pConfig->ISEGroupConfig.Lens_Parameter_P = pConfig->VIDevConfig.SrcHeight/3.1415;
    pConfig->ISEGroupConfig.Lens_Parameter_Cx = pConfig->VIDevConfig.SrcWidth/2;
    pConfig->ISEGroupConfig.Lens_Parameter_Cy = pConfig->VIDevConfig.SrcHeight/2;
    /*ISE Port parameter*/
    pConfig->ISEPortConfig[0].ISEWidth = 1920;
    pConfig->ISEPortConfig[0].ISEHeight = 1080;
    pConfig->ISEPortConfig[0].ISEStride = 1920;
    pConfig->ISEPortConfig[0].flip_enable = 0;
    pConfig->ISEPortConfig[0].mirror_enable = 0;
    /* Venc parameter*/
    pConfig->VencChnNum = 1;
    pConfig->EncoderType = PT_H264;
    pConfig->DestPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pConfig->EncoderCount = 500;
    pConfig->VencChnConfig[0].DestWidth = 1920;
    pConfig->VencChnConfig[0].DestHeight = 1080;
    pConfig->VencChnConfig[0].DestFrameRate = 30;
    pConfig->VencChnConfig[0].DestBitRate = 8*1024*1024;
    strcpy(pConfig->VencChnConfig[0].OutputFilePath, "/mnt/extsd/AW_FishEncoderVideoChn0.H264");
    if(pConfig->ISEGroupConfig.ISEPortNum != pConfig->VencChnNum)
    {
        aloge("fatal error! VencChnNum not equal to ISEPortNum!!");
        ret = -1;
        return ret;
    }
    
    if(conf_path)
    {
        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }

        pConfig->AutoTestCount = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_Auto_Test_Count, 0);

        /* Get VI parameter*/
        pConfig->VIDevConfig.DevId = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_Dev_ID, 0);
        pConfig->VIDevConfig.SrcFrameRate = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_Src_Frame_Rate, 0);
        pConfig->VIDevConfig.SrcWidth = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_Src_Width, 0);
        pConfig->VIDevConfig.SrcHeight = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_Src_Height, 0);
        alogd("VI Parameter:dev_id = %d, src_width = %d, src_height = %d, src_frame_rate = %d\n",
            pConfig->VIDevConfig.DevId, pConfig->VIDevConfig.SrcWidth, pConfig->VIDevConfig.SrcHeight, pConfig->VIDevConfig.SrcFrameRate);
#if 0
        if(pConfig->VIDevConfig.SrcWidth % 32 != 0 || pConfig->VIDevConfig.SrcHeight % 32 != 0)
        {
            aloge("fatal error! vi src width and src height must multiple of 32,width = %d,height = %d\n",
                    pConfig->VIDevConfig.SrcWidth,pConfig->VIDevConfig.SrcHeight);
            return FAILURE;
        }
#endif

        /*Get ISE parameter*/
        pConfig->ISEGroupConfig.ISEPortNum = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_ISE_Port_Num, 0);
        ISEPortNum = pConfig->ISEGroupConfig.ISEPortNum;
        pConfig->ISEGroupConfig.ISE_GDC_Dewarp_Mode = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_ISE_Dewarp_Mode, 0);
        pConfig->ISEGroupConfig.Lens_Parameter_P = pConfig->VIDevConfig.SrcHeight/3.1415;
        pConfig->ISEGroupConfig.Lens_Parameter_Cx = pConfig->VIDevConfig.SrcWidth/2;
        pConfig->ISEGroupConfig.Lens_Parameter_Cy = pConfig->VIDevConfig.SrcHeight/2;
        pConfig->ISEGroupConfig.ISE_GDC_Mount_Mode = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_ISE_Mount_Mode, 0);

        if(pConfig->ISEGroupConfig.ISE_GDC_Dewarp_Mode == Warp_Normal)
        {
            pConfig->ISEGroupConfig.normal_pan = (float)GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_ISE_NORMAL_Pan, 0);
            pConfig->ISEGroupConfig.normal_tilt = (float)GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_ISE_NORMAL_Tilt, 0);
            pConfig->ISEGroupConfig.normal_zoom = (float)GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_ISE_NORMAL_Zoom, 0);
            alogd("pan = %f,tilt = %f,zoom = %f",pConfig->ISEGroupConfig.normal_pan, pConfig->ISEGroupConfig.normal_tilt,pConfig->ISEGroupConfig.normal_zoom);
        }
        else if(pConfig->ISEGroupConfig.ISE_GDC_Dewarp_Mode == Warp_Ptz4In1)
        {
            for(i = 0;i < 4;i++)
            {
                snprintf(name, 256, "ise_ptz4in1_pan_%d", i);
                pConfig->ISEGroupConfig.ptz4in1_pan[i] = (float)GetConfParaInt(&stConfParser, name, 0);
                snprintf(name, 256, "ise_ptz4in1_tilt_%d", i);
                pConfig->ISEGroupConfig.ptz4in1_tilt[i] = (float)GetConfParaInt(&stConfParser, name, 0);
                snprintf(name, 256, "ise_ptz4in1_zoom_%d", i);
                pConfig->ISEGroupConfig.ptz4in1_zoom[i] = (float)GetConfParaInt(&stConfParser, name, 0);
                alogd("ptz4in1_Sub%d:pan = %f,tilt = %f,zoom = %f",i,
                      pConfig->ISEGroupConfig.ptz4in1_pan[i],pConfig->ISEGroupConfig.ptz4in1_tilt[i],
                      pConfig->ISEGroupConfig.ptz4in1_zoom[i]);
            }
        }
        alogd("\nISE Group Parameter: \ndewarp_mode = %d \nmount_mode = %d \nLens_Parameter_p = %f \n"
                "Lens_Parameter_cx = %d \nLens_Parameter_cy = %d\n",
                pConfig->ISEGroupConfig.ISE_GDC_Dewarp_Mode,pConfig->ISEGroupConfig.ISE_GDC_Mount_Mode,
                pConfig->ISEGroupConfig.Lens_Parameter_P,pConfig->ISEGroupConfig.Lens_Parameter_Cx,
                pConfig->ISEGroupConfig.Lens_Parameter_Cy);
        /*ISE Port parameter*/
        for(i = 0;i < ISEPortNum;i++)
        {
            snprintf(name, 256, "ise_port%d_width", i);
            pConfig->ISEPortConfig[i].ISEWidth = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_port%d_height", i);
            pConfig->ISEPortConfig[i].ISEHeight = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_port%d_stride", i);
            pConfig->ISEPortConfig[i].ISEStride = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_port%d_flip_enable", i);
            pConfig->ISEPortConfig[i].flip_enable = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_port%d_mirror_enable", i);
            pConfig->ISEPortConfig[i].mirror_enable = GetConfParaInt(&stConfParser, name, 0);
            alogd("\nISE Port%d Parameter: \nISE_Width = %d \nISE_Height = %d \nISE_Stride = %d \n"
                   "ISE_flip_enable = %d \nmirror_enable = %d\n",i,pConfig->ISEPortConfig[i].ISEWidth,
                    pConfig->ISEPortConfig[i].ISEHeight,pConfig->ISEPortConfig[i].ISEStride,
                    pConfig->ISEPortConfig[i].flip_enable,pConfig->ISEPortConfig[i].mirror_enable);
        }

        /* Get Venc parameter*/
        pConfig->VencChnNum = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_Venc_Chn_Num, 0);
        VencChnNum = pConfig->VencChnNum;
        if(ISEPortNum != VencChnNum)
        {
            aloge("fatal error! VencChnNum not equal to ISEPortNum!!");
            ret = -1;
            return ret;
        }
        EncoderType =(char*) GetConfParaString(&stConfParser, SAMPLE_Virvi2Fish2Venc_Venc_Encoder_Type, NULL);
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
            alogw("unsupported venc type:%p,encoder type turn to H.264!", EncoderType);
            pConfig->EncoderType = PT_H264;
        }
        pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_Virvi2Fish2Venc_Venc_Picture_Format, NULL);
        if(!strcmp(pStrPixelFormat, "nv21"))
        {
            pConfig->DestPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else
        {
            aloge("fatal error! conf file pic_format must be yuv420sp");
            pConfig->DestPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        pConfig->EncoderCount = GetConfParaInt(&stConfParser, SAMPLE_Virvi2Fish2Venc_Venc_Encoder_Count, 0);
        alogd("\nVenc Parameter: \nVencChnNum = %d \nencoder_type = %d \n"
               "encoder_count = %d \ndest_picture_format = %d\n",
               VencChnNum,pConfig->EncoderType,pConfig->EncoderCount,pConfig->DestPicFormat);
        for(i = 0;i < VencChnNum;i++)
        {
            snprintf(name, 256, "venc_chn%d_dest_width", i);
            pConfig->VencChnConfig[i].DestWidth = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "venc_chn%d_dest_height", i);
            pConfig->VencChnConfig[i].DestHeight = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "venc_chn%d_dest_frame_rate", i);
            pConfig->VencChnConfig[i].DestFrameRate = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "venc_chn%d_dest_bit_rate", i);
            pConfig->VencChnConfig[i].DestBitRate = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "venc_chn%d_output_file_path", i);
            ptr = (char*)GetConfParaString(&stConfParser, name, NULL);
            strncpy(pConfig->VencChnConfig[i].OutputFilePath, ptr, MAX_FILE_PATH_SIZE-1);
            pConfig->VencChnConfig[i].OutputFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
            alogd("VENC Chn%d Parameter:output_file_path = %s\n",
                    i,pConfig->VencChnConfig[i].OutputFilePath);
            alogd("dest_width = %d, dest_height = %d, dest_frame_rate = %d, dest_bit_rate = %d\n",
                pConfig->VencChnConfig[i].DestWidth,pConfig->VencChnConfig[i].DestHeight,
                pConfig->VencChnConfig[i].DestFrameRate,pConfig->VencChnConfig[i].DestBitRate);
        }
        destroyConfParser(&stConfParser);
    }
    return SUCCESS;
}

static void *GetVencImage_Thread(void *pArg)
{
    int ret = 0;
    int i = 0;
    VENC_Cap_S *pCap = (VENC_Cap_S *)pArg;
    VENC_STREAM_S VencFrame;
    VENC_PACK_S venc_pack;
    VencFrame.mPackCount = 1;
    VencFrame.mpPack = &venc_pack;
    while((i != pCap->EncoderCount) || (pCap->EncoderCount == -1))
    {
        i++;
        if((ret = AW_MPI_VENC_GetStream(pCap->Venc_Chn,&VencFrame,4000)) < 0) //6000(25fps) 4000(30fps)
        {
            aloge("venc chn%d get venc stream failed!",pCap->Venc_Chn);
            continue;
        }
        else
        {
#if 1
            if(VencFrame.mpPack->mpAddr0 != NULL && VencFrame.mpPack->mLen0)
            {
                fwrite(VencFrame.mpPack->mpAddr0,1,VencFrame.mpPack->mLen0,pCap->OutputFilePath);
            }
            if(VencFrame.mpPack->mpAddr1 != NULL && VencFrame.mpPack->mLen1)
            {
                fwrite(VencFrame.mpPack->mpAddr1,1,VencFrame.mpPack->mLen1,pCap->OutputFilePath);
            }
#endif
            ret = AW_MPI_VENC_ReleaseStream(pCap->Venc_Chn,&VencFrame);
            if(ret < 0) //108000
            {
                aloge("venc chn%d release venc stream failed!",pCap->Venc_Chn);
            }
         }
    }

    return NULL;
}

void Virvi2Fish2Venc_HELP()
{
    alogd("Run CSI0/CSI1+ISE+Venc command: ./sample_virvi2fish2venc -path ./sample_virvi2fish2venc.conf\r\n");
}

int main(int argc, char *argv[])
{
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

    int ret = -1, i = 0, j = 0,count = 0;
    int result = 0;
    int AutoTestCount = 0;
    alogd("sample_virvi2fish2venc build time = %s, %s.", __DATE__, __TIME__);
//    if (argc != 3)
//    {
//        Virvi2Fish2Venc_HELP();
//        exit(0);
//    }
    VIRVI_Cap_S    pVICap[MAX_VIR_CHN_NUM];
    ISE_GroupCap_S pISEGroupCap[ISE_MAX_GRP_NUM];
    ISE_PortCap_S  pISEPortCap[ISE_MAX_CHN_NUM];
    VENC_Cap_S     pVENCCap[VENC_MAX_CHN_NUM];

    SampleVirvi2Fish2VencConfparser stContext;
    //parse command line param,read sample_virvi2fish2venc.conf
    if(ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0)
    {
        aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(stContext.mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = stContext.mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL; //DEFAULT_SAMPLE_VIRVI2FISH2VENC_CONF_PATH;
    }
    //parse config file.
    if(loadSampleVirvi2Fish2VencConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    AutoTestCount = stContext.mConfigPara.AutoTestCount;

    while (count != AutoTestCount)
    {
        /*Set VI Channel Attribute*/
        memset(&pVICap[0], 0, sizeof(VIRVI_Cap_S));
        pVICap[0].stAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pVICap[0].stAttr.memtype = V4L2_MEMORY_MMAP;
        pVICap[0].stAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
        pVICap[0].stAttr.format.field = V4L2_FIELD_NONE;
        pVICap[0].stAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
        pVICap[0].stAttr.format.width = stContext.mConfigPara.VIDevConfig.SrcWidth;
        pVICap[0].stAttr.format.height = stContext.mConfigPara.VIDevConfig.SrcHeight;
        pVICap[0].stAttr.nbufs = 5;
        pVICap[0].stAttr.nplanes = 2;
        pVICap[0].stAttr.fps = stContext.mConfigPara.VIDevConfig.SrcFrameRate;
        pVICap[0].VI_Dev = stContext.mConfigPara.VIDevConfig.DevId;
        pVICap[0].VI_Chn = 0;
        pVICap[0].s32MilliSec = 5000;
        /*vi bind channel attr*/
        pVICap[0].VI_CHN_S.mChnId = 0;
        pVICap[0].VI_CHN_S.mDevId = stContext.mConfigPara.VIDevConfig.DevId;
        pVICap[0].VI_CHN_S.mModId = MOD_ID_VIU;

#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
        alogd("==========>>>> ise gdc <<============\n");
        int ISEPortNum = 0;
        ISEPortNum = stContext.mConfigPara.ISEGroupConfig.ISEPortNum;
        memset(&pISEGroupCap[0], 0, sizeof(ISE_GroupCap_S));
        for(j = 0;j < 1;j++)
        {
            pISEGroupCap[j].ISE_Group = j;  //Group ID
            pISEGroupCap[j].pGrpAttr.iseMode = ISEMODE_ONE_FISHEYE;
            pISEGroupCap[j].pGrpAttr.mPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
            /*ise group bind channel attr*/
            pISEGroupCap[j].ISE_Group_S.mChnId = 0;
            pISEGroupCap[j].ISE_Group_S.mDevId = j;
            pISEGroupCap[j].ISE_Group_S.mModId = MOD_ID_ISE;
            for(i = 0;i < ISEPortNum;i++)
            {
                memset(&pISEPortCap[i], 0, sizeof(ISE_PortCap_S));
                pISEGroupCap[j].ISE_Port[i] = i;   //Port ID
                /*ise port bind channel attr*/
                pISEPortCap[i].ISE_Port_S.mChnId = i;
                pISEPortCap[i].ISE_Port_S.mDevId = j;
                pISEPortCap[i].ISE_Port_S.mModId = MOD_ID_ISE;

                /*Set ISE Port Attribute*/
                if(i == 0)  //fish arttr
                {
                    ISE_CFG_PARA_GDC *ise_gdc_cfg = &pISEPortCap[i].PortAttr.mode_attr.mFish.ise_gdc_cfg;
                    ise_gdc_cfg->rectPara.warpMode = stContext.mConfigPara.ISEGroupConfig.ISE_GDC_Dewarp_Mode;
                    ise_gdc_cfg->srcImage.width = stContext.mConfigPara.VIDevConfig.SrcWidth;
                    ise_gdc_cfg->srcImage.height = stContext.mConfigPara.VIDevConfig.SrcHeight;
                    ise_gdc_cfg->srcImage.img_format = PLANE_YUV420; // YUV420
                    ise_gdc_cfg->srcImage.stride[0] = stContext.mConfigPara.VIDevConfig.SrcWidth;
                    alogd("warpMode:%d", ise_gdc_cfg->rectPara.warpMode);
                    switch(ise_gdc_cfg->rectPara.warpMode)
                    {
                        case Warp_LDC:
                        {
                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420; //PLANE_YVU420;
                            ise_gdc_cfg->undistImage.width = stContext.mConfigPara.ISEPortConfig[0].ISEWidth;
                            ise_gdc_cfg->undistImage.height = stContext.mConfigPara.ISEPortConfig[0].ISEHeight;
                            ise_gdc_cfg->undistImage.stride[0] = stContext.mConfigPara.ISEPortConfig[0].ISEStride;
                            //镜头参数
                            ise_gdc_cfg->rectPara.LensDistPara.fx = 1891.85f / 2.0f;
                            ise_gdc_cfg->rectPara.LensDistPara.fy = 1891.85f / 2.0f;
                            ise_gdc_cfg->rectPara.LensDistPara.cx = 1928.20f / 2.0f;
                            ise_gdc_cfg->rectPara.LensDistPara.cy = 1115.24f / 2.0f;
                            ise_gdc_cfg->rectPara.LensDistPara.fx_scale = 1899.46f / 2.0f;
                            ise_gdc_cfg->rectPara.LensDistPara.fy_scale = 1899.46f / 2.0f;
                            ise_gdc_cfg->rectPara.LensDistPara.cx_scale = 1919.50f / 2.0f;
                            ise_gdc_cfg->rectPara.LensDistPara.cy_scale = 1079.50f / 2.0f;

                            //畸变系数
                            ise_gdc_cfg->rectPara.LensDistPara.distCoef_fish_k[0] = -0.0246f;
                            ise_gdc_cfg->rectPara.LensDistPara.distCoef_fish_k[1] = -0.0164f;
                            ise_gdc_cfg->rectPara.LensDistPara.distCoef_fish_k[2] =  0.0207f;
                            ise_gdc_cfg->rectPara.LensDistPara.distCoef_fish_k[3] = -0.0074f;

                            //广角径向畸变系数
                            ise_gdc_cfg->rectPara.LensDistPara.distCoef_wide_ra[0] = -0.3560f;
                            ise_gdc_cfg->rectPara.LensDistPara.distCoef_wide_ra[1] =  0.1886f;
                            ise_gdc_cfg->rectPara.LensDistPara.distCoef_wide_ra[2] = -0.0656f;

                            //广角切向畸变系数
                            ise_gdc_cfg->rectPara.LensDistPara.distCoef_wide_ta[0] = -0.00025f;
                            ise_gdc_cfg->rectPara.LensDistPara.distCoef_wide_ta[1] = -0.00006f;
                            //LDC校正参数
                            ise_gdc_cfg->rectPara.LensDistPara.zoomH = 100;//[0,200]
                            ise_gdc_cfg->rectPara.LensDistPara.zoomV = 100;//[0,200]
                            ise_gdc_cfg->rectPara.LensDistPara.centerOffsetX = 0;
                            ise_gdc_cfg->rectPara.LensDistPara.centerOffsetY = 0;
                            ise_gdc_cfg->rectPara.LensDistPara.rotateAngle   = 0;//[0,360]
                            ise_gdc_cfg->rectPara.LensDistPara.distModel = DistModel_WideAngle;

                            ise_gdc_cfg->rectPara.warpPara.radialDistortCoef    = 0; //[-255,255]
                            ise_gdc_cfg->rectPara.warpPara.trapezoidDistortCoef = 0; //[-255,255]
                            break;
                        }
                        case Warp_Normal:
                        {
                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420;
                            ise_gdc_cfg->undistImage.width = stContext.mConfigPara.ISEPortConfig[0].ISEWidth;
                            ise_gdc_cfg->undistImage.height = stContext.mConfigPara.ISEPortConfig[0].ISEHeight;
                            ise_gdc_cfg->undistImage.stride[0] = stContext.mConfigPara.ISEPortConfig[0].ISEWidth;
                            //鱼眼镜头视场角
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.fov = 180;
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.width = ise_gdc_cfg->srcImage.width;
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.height = ise_gdc_cfg->srcImage.height;
                            //镜头参数
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.cx     = (float)(ise_gdc_cfg->rectPara.fishEyePara.lensParams.width  - 1) / 2.0f;
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.cy     = (float)(ise_gdc_cfg->rectPara.fishEyePara.lensParams.height - 1) / 2.0f;
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.radius = (float)(ise_gdc_cfg->rectPara.fishEyePara.lensParams.width  - 1) / 2.0f;
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.k[0] = 2 * ise_gdc_cfg->rectPara.fishEyePara.lensParams.radius / DEG2RAD(ise_gdc_cfg->rectPara.fishEyePara.lensParams.fov);
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.k[1] = ise_gdc_cfg->rectPara.fishEyePara.lensParams.k[0];
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.k[2] = ise_gdc_cfg->rectPara.fishEyePara.lensParams.k[0];
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.k[3] = ise_gdc_cfg->rectPara.fishEyePara.lensParams.k[0];
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.k[4] = ise_gdc_cfg->rectPara.fishEyePara.lensParams.k[0];
                            ise_gdc_cfg->rectPara.fishEyePara.lensParams.k_order = 1;
                            //校正参数
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.pan = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.tilt = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.zoomH = 100;
                            ise_gdc_cfg->rectPara.fishEyePara.scale = 100;
                            ise_gdc_cfg->rectPara.warpPara.radialDistortCoef = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.mountMode = Mount_Wall;
                            break;
                        }
                        default:
                        {
                            aloge("fatal error! warpMode[%d] is not support temporary.", ise_gdc_cfg->rectPara.warpMode);
                            break;
                        }
                    }
                }
                pISEGroupCap[j].PortCap_S[i] = &pISEPortCap[i];
            }
        }
#elif (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
        alogd("==========>>>> ise mo <<============\n");
        /*Set ISE Group Attribute*/
        int ISEPortNum = 0;
        ISEPortNum = stContext.mConfigPara.ISEGroupConfig.ISEPortNum;
        memset(&pISEGroupCap[0], 0, sizeof(ISE_GroupCap_S));
        for(j = 0;j < 1;j++)
        {
            pISEGroupCap[j].ISE_Group = j;  //Group ID
            pISEGroupCap[j].pGrpAttr.iseMode = ISEMODE_ONE_FISHEYE;
            /*ise group bind channel attr*/
            pISEGroupCap[j].ISE_Group_S.mChnId = 0;
            pISEGroupCap[j].ISE_Group_S.mDevId = j;
            pISEGroupCap[j].ISE_Group_S.mModId = MOD_ID_ISE;
            for(i = 0;i < ISEPortNum;i++)
            {
                memset(&pISEPortCap[i], 0, sizeof(ISE_PortCap_S));
                pISEGroupCap[j].ISE_Port[i] = i;   //Port ID
                /*ise port bind channel attr*/
                pISEPortCap[i].ISE_Port_S.mChnId = i;
                pISEPortCap[i].ISE_Port_S.mDevId = j;
                pISEPortCap[i].ISE_Port_S.mModId = MOD_ID_ISE;
                /*Set ISE Port Attribute*/
                if(i == 0)  //fish arttr
                {
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode = stContext.mConfigPara.ISEGroupConfig.ISE_Dewarp_Mode;
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.in_w = stContext.mConfigPara.VIDevConfig.SrcWidth;
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.in_h = stContext.mConfigPara.VIDevConfig.SrcHeight;
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.in_luma_pitch = stContext.mConfigPara.VIDevConfig.SrcWidth;
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.in_chroma_pitch = stContext.mConfigPara.VIDevConfig.SrcWidth;
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.in_yuv_type = 0;
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.out_yuv_type = 0;
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.p =  stContext.mConfigPara.ISEGroupConfig.Lens_Parameter_P;
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cx =  stContext.mConfigPara.ISEGroupConfig.Lens_Parameter_Cx;
                    pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cy = stContext.mConfigPara.ISEGroupConfig.Lens_Parameter_Cy;
                    if(pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_NORMAL)
                    {
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.mount_mode = stContext.mConfigPara.ISEGroupConfig.Mount_Mode;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.pan = stContext.mConfigPara.ISEGroupConfig.normal_pan;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.tilt = stContext.mConfigPara.ISEGroupConfig.normal_tilt;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.zoom = stContext.mConfigPara.ISEGroupConfig.normal_zoom;
                    }
                    if(pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_PANO360)
                    {
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.mount_mode = stContext.mConfigPara.ISEGroupConfig.Mount_Mode;
                    }
                    if(pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_180WITH2)
                    {
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.mount_mode = stContext.mConfigPara.ISEGroupConfig.Mount_Mode;
                    }
                    if(pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_UNDISTORT)
                    {
#if Len_110
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cx = 918.18164 * (stContext.mConfigPara.VIDevConfig.SrcWidth*1.0/1920);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cy = 479.86784 * (stContext.mConfigPara.VIDevConfig.SrcHeight*1.0/1080);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.fx  = 1059.11146 * (stContext.mConfigPara.VIDevConfig.SrcWidth*1.0/1920);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.fy = 1053.26240 * (stContext.mConfigPara.VIDevConfig.SrcHeight*1.0/1080);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cxd = 876.34066 * (stContext.mConfigPara.VIDevConfig.SrcWidth*1.0/1920);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cyd = 465.93162 * (stContext.mConfigPara.VIDevConfig.SrcHeight*1.0/1080);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.fxd = 691.76196 * (stContext.mConfigPara.VIDevConfig.SrcWidth*1.0/1920);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.fyd = 936.82897 * (stContext.mConfigPara.VIDevConfig.SrcHeight*1.0/1080);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[0] = 0.182044494560808;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[1] = -0.1481043082174997;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[2] = -0.005128687334715951;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[3] = 0.567926713301489;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[4] = -0.1789466261819578;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[5] = -0.03561367966855939;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.p_undis[0] = 0.000649146914880072;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.p_undis[1] = 0.0002534155740808075;
#endif

#if Len_130
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cx = 889.50411 * (stContext.mConfigPara.VIDevConfig.SrcWidth*1.0/1920);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cy = 522.42100 * (stContext.mConfigPara.VIDevConfig.SrcHeight*1.0/1080);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.fx  = 963.98364 * (stContext.mConfigPara.VIDevConfig.SrcWidth*1.0/1920);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.fy = 965.10729 * (stContext.mConfigPara.VIDevConfig.SrcHeight*1.0/1080);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cxd = 801.04388 * (stContext.mConfigPara.VIDevConfig.SrcWidth*1.0/1920);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.cyd = 518.79163 * (stContext.mConfigPara.VIDevConfig.SrcHeight*1.0/1080);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.fxd = 580.58685 * (stContext.mConfigPara.VIDevConfig.SrcWidth*1.0/1920);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.fyd = 850.71746 * (stContext.mConfigPara.VIDevConfig.SrcHeight*1.0/1080);
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[0] = 0.5874970340655806;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[1] = 0.01263866896598456;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[2] = -0.003440797814786819;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[3] = 0.9486826799125321;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[4] = 0.1349250696268053;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.k[5] = -0.01052234728693081;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.p_undis[0] = 0.000362407777916013;
                        pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.p_undis[1] = -0.0001245920435755955;

#endif
                    }
                    if(pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_PTZ4IN1)
                    {
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.mount_mode = stContext.mConfigPara.ISEGroupConfig.Mount_Mode;
                        for(int sub_num = 0;sub_num < 4;sub_num++)
                        {
                            pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.pan_sub[sub_num] = stContext.mConfigPara.ISEGroupConfig.ptz4in1_pan[sub_num];
                            pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.tilt_sub[sub_num] = stContext.mConfigPara.ISEGroupConfig.ptz4in1_tilt[sub_num];;
                            pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.zoom_sub[sub_num] = stContext.mConfigPara.ISEGroupConfig.ptz4in1_zoom[sub_num];;
                        }
                    }
                }
                pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.out_en[i] = 1;
                pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.out_w[i] =  stContext.mConfigPara.ISEPortConfig[i].ISEWidth;
                pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.out_h[i] =  stContext.mConfigPara.ISEPortConfig[i].ISEHeight;
                pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.out_flip[i] =  stContext.mConfigPara.ISEPortConfig[i].flip_enable;
                pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.out_mirror[i] =  stContext.mConfigPara.ISEPortConfig[i].mirror_enable;
                pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.out_luma_pitch[i] =  stContext.mConfigPara.ISEPortConfig[i].ISEStride;
                pISEPortCap[i].PortAttr.mode_attr.mFish.ise_cfg.out_chroma_pitch[i] =  stContext.mConfigPara.ISEPortConfig[i].ISEStride;
                pISEGroupCap[j].PortCap_S[i] = &pISEPortCap[i];
            }
        }
#endif
        /*Set VENC Channel Attribute*/
        int VencChnNum = 0;
        VencChnNum = stContext.mConfigPara.VencChnNum;
        int VIFrameRate = stContext.mConfigPara.VIDevConfig.SrcFrameRate;
        int VencFrameRate = 0;
        int wantedFrameRate = 0,wantedVideoBitRate = 0;
        PAYLOAD_TYPE_E videoCodec = stContext.mConfigPara.EncoderType;
        PIXEL_FORMAT_E wantedPreviewFormat = stContext.mConfigPara.DestPicFormat;
        for(i = 0;i < VencChnNum;i++)
        {
            memset(&pVENCCap[i], 0, sizeof(VENC_Cap_S));
            SIZE_S videosize = {stContext.mConfigPara.ISEPortConfig[i].ISEWidth, stContext.mConfigPara.ISEPortConfig[i].ISEHeight};
            VencFrameRate = stContext.mConfigPara.VencChnConfig[i].DestFrameRate;
            SIZE_S wantedVideoSize = {stContext.mConfigPara.VencChnConfig[i].DestWidth, stContext.mConfigPara.VencChnConfig[i].DestHeight};
            wantedFrameRate = stContext.mConfigPara.VencChnConfig[i].DestFrameRate;
            wantedVideoBitRate = stContext.mConfigPara.VencChnConfig[i].DestBitRate;

            pVENCCap[i].thid = 0;
            pVENCCap[i].Venc_Chn = i;
            pVENCCap[i].mVEncChnAttr.VeAttr.Type = videoCodec;
            pVENCCap[i].mVEncChnAttr.VeAttr.SrcPicWidth = videosize.Width;
            pVENCCap[i].mVEncChnAttr.VeAttr.SrcPicHeight = videosize.Height;
            pVENCCap[i].mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
            pVENCCap[i].mVEncChnAttr.VeAttr.PixelFormat = wantedPreviewFormat;
            pVENCCap[i].stFrameRate.SrcFrmRate = VIFrameRate;
            pVENCCap[i].stFrameRate.DstFrmRate = VencFrameRate;
            pVENCCap[i].EncoderCount = stContext.mConfigPara.EncoderCount;
            /*venc chn bind channel attr*/
            pVENCCap[i].VENC_CHN_S.mChnId = i;
            pVENCCap[i].VENC_CHN_S.mDevId = 0;
            pVENCCap[i].VENC_CHN_S.mModId = MOD_ID_VENC;
            if(PT_H264 == pVENCCap[i].mVEncChnAttr.VeAttr.Type)
            {
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH264e.Profile = 2;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH264e.PicWidth = wantedVideoSize.Width;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH264e.PicHeight = wantedVideoSize.Height;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
                pVENCCap[i].mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                pVENCCap[i].mVEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate = wantedVideoBitRate;
            }
            else if(PT_H265 == pVENCCap[i].mVEncChnAttr.VeAttr.Type)
            {
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = wantedVideoSize.Width;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = wantedVideoSize.Height;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
                pVENCCap[i].mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                pVENCCap[i].mVEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate = wantedVideoBitRate;
            }
            else if(PT_MJPEG == pVENCCap[i].mVEncChnAttr.VeAttr.Type)
            {
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth= videosize.Width;
                pVENCCap[i].mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = videosize.Height;
                pVENCCap[i].mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
                pVENCCap[i].mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = wantedVideoBitRate;
            }
            pVENCCap[i].OutputFilePath = fopen(stContext.mConfigPara.VencChnConfig[i].OutputFilePath, "wb+");
            if(!pVENCCap[i].OutputFilePath)
            {
                aloge("fatal error! can't open encodervideo file[%s]", stContext.mConfigPara.VencChnConfig[i].OutputFilePath);
                result = -1;
                goto _exit;
            }
        }

        /* start mpp systerm */
        MPP_SYS_CONF_S mSysConf;
        memset(&mSysConf,0,sizeof(MPP_SYS_CONF_S));
        mSysConf.nAlignWidth = 32;
        AW_MPI_SYS_SetConf(&mSysConf);
        ret = AW_MPI_SYS_Init();
        if (ret < 0)
        {
            aloge("sys init failed");
            result = -1;
            goto sys_exit;
        }

        /* creat vi component */
        ret = aw_vipp_creat(pVICap[0].VI_Dev, &pVICap[0].stAttr);
        if(ret < 0)
        {
            aloge("vipp creat failed,VIDev = %d",pVICap[0].VI_Dev);
            result = -1;
            goto vipp_exit;
        }
        /*AW_MPI_ISP_Init(0);
        AW_MPI_ISP_Run(0);*/
        ret = aw_virvi_creat(pVICap[0].VI_Dev,pVICap[0].VI_Chn, NULL);
        if(ret < 0)
        {
            aloge("virvi creat failed,VIDev = %d,VIChn = %d",pVICap[0].VI_Dev,pVICap[0].VI_Chn);
            result = -1;
            goto virvi_exit;
        }

        /* creat ise component */
        ret = aw_isegroup_creat(pISEGroupCap[0].ISE_Group, &pISEGroupCap[0].pGrpAttr);
        if(ret < 0)
        {
            aloge("ISE Group%d creat failed",pISEGroupCap[0].ISE_Group);
            result = -1;
            goto ise_group_exit;
        }
        for(i = 0;i < ISEPortNum;i++)
        {
            ret = aw_iseport_creat(pISEGroupCap[0].ISE_Group,pISEGroupCap[0].ISE_Port[i],&(pISEGroupCap[0].PortCap_S[i]->PortAttr));
            if(ret < 0)
            {
                aloge("ISE Port%d creat failed",pISEGroupCap[0].ISE_Port[i]);
                result = -1;
                goto ise_port_exit;
            }
        }

        /* creat venc component */
        for(i = 0; i < VencChnNum;i++)
        {
            ret = aw_venc_chn_creat(&pVENCCap[i]);
            if(ret < 0)
            {
                aloge("Venc Chn%d Creat failed!",pVENCCap[i].Venc_Chn);
                result = -1;
                goto venc_chn_exit;
            }
        }
        /* write spspps*/
        for(i = 0;i < VencChnNum;i++)
        {
            if(PT_H264 == pVENCCap[i].mVEncChnAttr.VeAttr.Type)
            {
                result = AW_MPI_VENC_GetH264SpsPpsInfo(pVENCCap[i].Venc_Chn, &pVENCCap[i].vencheader);
                if (SUCCESS == result)
                {
                    if(pVENCCap[i].vencheader.nLength)
                    {
                        fwrite(pVENCCap[i].vencheader.pBuffer,pVENCCap[i].vencheader.nLength,1,pVENCCap[i].OutputFilePath);
                    }
                }
                else
                {
                    alogd("AW_MPI_VENC_GetH264SpsPpsInfo failed!\n");
                    result = -1;
                }
            }
            else if(PT_H265 == pVENCCap[i].mVEncChnAttr.VeAttr.Type)
            {
                result = AW_MPI_VENC_GetH265SpsPpsInfo(pVENCCap[i].Venc_Chn, &pVENCCap[i].vencheader);
                if (SUCCESS == result)
                {
                    if(pVENCCap[i].vencheader.nLength)
                    {
                        fwrite(pVENCCap[i].vencheader.pBuffer,pVENCCap[i].vencheader.nLength,1,pVENCCap[i].OutputFilePath);
                    }
                }
                else
                {
                    alogd("AW_MPI_VENC_GetH265SpsPpsInfo failed!\n");
                    result = -1;
                }
            }
        }
        /* bind component */
        if (pVICap[0].VI_CHN_S.mDevId >= 0 && pISEGroupCap[0].ISE_Group_S.mDevId >= 0)
        {
            ret = AW_MPI_SYS_Bind(&pVICap[0].VI_CHN_S,&pISEGroupCap[0].ISE_Group_S);
           if(ret != SUCCESS)
           {
               aloge("error!!! VI dev%d can not bind ISE Group%d!!!\n",
                       pVICap[0].VI_CHN_S.mDevId,pISEGroupCap[0].ISE_Group_S.mDevId);
               result = -1;
               goto vi_bind_ise_exit;
           }
        }
        for(i = 0;i < ISEPortNum;i++)
        {
            if (pISEGroupCap[0].PortCap_S[i]->ISE_Port_S.mChnId >= 0 && pVENCCap[i].VENC_CHN_S.mChnId >= 0)
            {
                ret = AW_MPI_SYS_Bind(&pISEGroupCap[0].PortCap_S[i]->ISE_Port_S,&pVENCCap[i].VENC_CHN_S);
                if(ret != SUCCESS)
                {
                    aloge("error!!! ISE Port%d can not bind VencPort%d!!!\n",
                            pISEGroupCap[0].PortCap_S[i]->ISE_Port_S.mChnId,
                            pVENCCap[i].VENC_CHN_S.mChnId);
                    result = -1;
                    goto ise_bind_venc_exit;
                }
            }
        }

        /* start component */
        ret = AW_MPI_VI_EnableVirChn(pVICap[0].VI_Dev, pVICap[0].VI_Chn);
        if (ret < 0)
        {
            aloge("VI enable error! VIDev = %d,VIChn = %d",pVICap[0].VI_Dev, pVICap[0].VI_Chn);
            result = -1;
            goto vi_stop;
        }
        ret = AW_MPI_ISE_Start(pISEGroupCap[0].ISE_Group);
        if (ret < 0)
        {
            aloge("ISE Start error!");
            result = -1;
            goto ise_stop;
        }
        for(i = 0; i < VencChnNum;i++)
        {
            ret = AW_MPI_VENC_StartRecvPic(pVENCCap[i].Venc_Chn);
            if (ret < 0)
            {
                aloge("VENC Chn%d Start RecvPic error!",pVENCCap[i].Venc_Chn);
                result = -1;
                goto venc_stop;
            }
        }
        for (i = 0; i < VencChnNum; i++)
        {
            ret = pthread_create(&pVENCCap[i].thid, NULL, GetVencImage_Thread, &pVENCCap[i]);
        }
        for (i = 0; i < VencChnNum; i++)
        {
            pthread_join(pVENCCap[i].thid, NULL);
        }

venc_stop:
        /* stop component */
        for(i = 0;i < VencChnNum;i++)
        {
            ret = AW_MPI_VENC_StopRecvPic(pVENCCap[i].Venc_Chn);
            if (ret < 0)
            {
                aloge("VENC Chn%d Stop Receive Picture error!",pVENCCap[i].Venc_Chn);
                result = -1;
                goto _exit;
            }
        }

ise_stop:
        ret = AW_MPI_ISE_Stop(pISEGroupCap[0].ISE_Group);
        if (ret < 0)
        {
            aloge("ISE Stop error!");
            result = -1;
            goto _exit;
        }

vi_stop:
        ret = AW_MPI_VI_DisableVirChn(pVICap[0].VI_Dev, pVICap[0].VI_Chn);
        if(ret < 0)
        {
            aloge("Disable VI Chn failed,VIDev = %d,VIChn = %d",pVICap[0].VI_Dev, pVICap[0].VI_Chn);
            result = -1;
            goto _exit;
        }

vi_bind_ise_exit:
        if (pVICap[0].VI_CHN_S.mDevId >= 0 && pISEGroupCap[0].ISE_Group_S.mDevId >= 0)
        {
            /*ret = AW_MPI_SYS_UnBind(&pVICap[0].VI_CHN_S,&pISEGroupCap[0].ISE_Group_S);
           if(ret != SUCCESS)
           {
               aloge("error!!! Unbind VI dev%d ISE Group%d failed!!!\n",
                       pVICap[0].VI_CHN_S.mDevId,pISEGroupCap[0].ISE_Group_S.mDevId);
               result = -1;
               goto _exit;
           }*/
        }
ise_bind_venc_exit:
        for(i = 0;i < ISEPortNum;i++)
        {
            if (pISEGroupCap[0].PortCap_S[i]->ISE_Port_S.mChnId >= 0 && pVENCCap[i].VENC_CHN_S.mChnId >= 0)
            {
                /*ret = AW_MPI_SYS_UnBind(&pISEGroupCap[0].PortCap_S[i]->ISE_Port_S,&pVENCCap[i].VENC_CHN_S);
                if(ret != SUCCESS)
                {
                    aloge("error!!! Unbind ISE Port%d VencPort%d failed!!!\n",
                            pISEGroupCap[0].PortCap_S[i]->ISE_Port_S.mChnId,
                            pVENCCap[i].VENC_CHN_S.mChnId);
                    result = -1;
                    goto _exit;
                }*/
            }
        }

venc_chn_exit:
        /* destory venc component */
        for(i = 0;i < VencChnNum;i++)
        {
            ret = aw_venc_chn_destory(&pVENCCap[i]);
            if(ret < 0)
            {
                aloge("Venc Chn%d distory failed!",pVENCCap[i].Venc_Chn);
                result = -1;
                goto _exit;
            }
        }

ise_port_exit:
        /* destory ise component */
        for(i = 0;i < ISEPortNum;i++)
        {
            ret = aw_iseport_destory(pISEGroupCap[0].ISE_Group, pISEGroupCap[0].ISE_Port[i]);
            if (ret < 0)
            {
                aloge("ISE Port%d distory error!",pISEGroupCap[0].ISE_Port[i]);
                result = -1;
                goto _exit;
            }
        }

ise_group_exit:
        ret = aw_isegroup_destory(pISEGroupCap[0].ISE_Group);
        if (ret < 0)
        {
            aloge("ISE Destroy Group%d error!",pISEGroupCap[0].ISE_Group);
            result = -1;
            goto _exit;
        }

virvi_exit:
        /* destory vi component */
        ret = aw_virvi_destory(pVICap[0].VI_Dev, pVICap[0].VI_Chn);
        if (ret < 0)
        {
            aloge("virvi end error! VIDev = %d,VIChn = %d",
                    pVICap[0].VI_Dev, pVICap[0].VI_Chn);
            result = -1;
            goto _exit;
        }
vipp_exit:
        ret = aw_vipp_destory(pVICap[0].VI_Dev);
        if (ret < 0)
        {
            aloge("vipp end error! VIDev = %d",pVICap[0].VI_Dev);
            result = -1;
            goto _exit;
        }
        AW_MPI_ISP_Stop(0);
sys_exit:
        /* exit mpp systerm */
        ret = AW_MPI_SYS_Exit();
        if (ret < 0)
        {
            aloge("sys exit failed!");
            result = -1;
            goto _exit;
        }
        for (i = 0; i < VencChnNum; i++)
        {
            fflush(pVENCCap[i].OutputFilePath);
            fclose(pVENCCap[i].OutputFilePath);
        }
        printf("======================================.\r\n");
        printf("Auto Test count end: %d. (MaxCount==1000).\r\n", count);
        printf("======================================.\r\n");
        count ++;
    }

_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
