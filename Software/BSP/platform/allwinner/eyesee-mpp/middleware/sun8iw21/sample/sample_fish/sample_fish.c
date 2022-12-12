/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_fish.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/1/5
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

#define LOG_TAG "sample_fish"
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
#include <cdx_list.h>
#include <utils/plat_log.h>
#include "mpi_ise_common.h"
#include "media/mm_comm_ise.h"
#include "media/mpi_ise.h"
#include "media/mpi_sys.h"
#include "ion_memmanager.h"
#include "sc_interface.h"
#include "memoryAdapter.h"
#include <tsemaphore.h>
#include <ConfigOption.h>
#include <confparser.h>
#include "sample_fish.h"
#include "sample_fish_config.h"

#define Save_Picture         1
#define Dynamic_PTZ          0   //Dynamic adjust PTZ
#define Load_Len_Parameter   0
#define FILE_EXIST(PATH)   (access(PATH, F_OK) == 0)

int Thread_EXIT = 0;
typedef struct awPic_Cap_s
{
    int PicWidth;
    int PicHeight;
    int PicStride;
    int PicFrameRate;
    FILE* PicFilePath;
    pthread_t thread_id;
}awPic_Cap_s;

typedef struct awISE_PortCap_S {
    ISE_GRP  ISE_Group;
    ISE_CHN  ISE_Port;
    int width;
    int height;
    char *OutputFilePath;
    ISE_CHN_ATTR_S PortAttr;
    AW_S32   s32MilliSec;
    pthread_t thread_id;
}ISE_PortCap_S;

typedef struct awISE_pGroupCap_S {
    ISE_GRP ISE_Group;
    ISE_GROUP_ATTR_S pGrpAttr;
    ISE_PortCap_S PortCap_S[4];
}ISE_GroupCap_S;

typedef struct SampleFishFrameNode
{
    VIDEO_FRAME_INFO_S mFrame;
    struct list_head mList;
}SampleFishFrameNode;

typedef struct SampleFishFrameManager
{
    struct list_head mIdleList; //SampleFishFrameNode
    struct list_head mUsingList;
    pthread_mutex_t mWaitFrameLock;
    int mbWaitFrameFlag;
    cdx_sem_t mSemFrameCome;
    int mNodeCnt;
    pthread_mutex_t mLock;
    VIDEO_FRAME_INFO_S* (*PrefetchFirstIdleFrame)(void *pThiz);
    int (*UseFrame)(void *pThiz, VIDEO_FRAME_INFO_S *pFrame);
    int (*ReleaseFrame)(void *pThiz, unsigned int nFrameId);
}SampleFishFrameManager;

typedef struct SampleFishParameter
{
    int Process_Count;
    ISE_GroupCap_S pISEGroupCap[4];
    SampleFishFrameManager  mFrameManager;
    awPic_Cap_s    PictureCap;
}SampleFishParameter;

#define ISE_GROUP_INS_CNT 1

VIDEO_FRAME_INFO_S* SampleFishFrameManager_PrefetchFirstIdleFrame(void *pThiz)
{
    SampleFishFrameManager *pFrameManager = (SampleFishFrameManager*)pThiz;
    SampleFishFrameNode *pFirstNode;
    VIDEO_FRAME_INFO_S *pFrameInfo;
    pthread_mutex_lock(&pFrameManager->mLock);
    if(!list_empty(&pFrameManager->mIdleList))
    {
        pFirstNode = list_first_entry(&pFrameManager->mIdleList, SampleFishFrameNode, mList);
        pFrameInfo = &pFirstNode->mFrame;
    }
    else
    {
        pFrameInfo = NULL;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return pFrameInfo;
}

int SampleFishFrameManager_UseFrame(void *pThiz, VIDEO_FRAME_INFO_S *pFrame)
{
    int ret = 0;
    SampleFishFrameManager *pFrameManager = (SampleFishFrameManager*)pThiz;
    if(NULL == pFrame)
    {
        aloge("fatal error! pNode == NULL!");
        return -1;
    }
    pthread_mutex_lock(&pFrameManager->mLock);
    SampleFishFrameNode *pFirstNode = list_first_entry_or_null(&pFrameManager->mIdleList, SampleFishFrameNode, mList);
    if(pFirstNode)
    {
        if(&pFirstNode->mFrame == pFrame)
        {
            list_move_tail(&pFirstNode->mList, &pFrameManager->mUsingList);
        }
        else
        {
            aloge("fatal error! node is not match [%p]!=[%p]", pFrame, &pFirstNode->mFrame);
            ret = -1;
        }
    }
    else
    {
        aloge("fatal error! idle list is empty");
        ret = -1;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return ret;
}

int SampleFishFrameManager_ReleaseFrame(void *pThiz, unsigned int nFrameId)
{
    int ret = 0;
    SampleFishFrameManager *pFrameManager = (SampleFishFrameManager*)pThiz;
    pthread_mutex_lock(&pFrameManager->mLock);
    int bFindFlag = 0;
    SampleFishFrameNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mUsingList, mList)
    {
        if(pEntry->mFrame.mId == nFrameId)
        {
            list_move_tail(&pEntry->mList, &pFrameManager->mIdleList);
            bFindFlag = 1;
            break;
        }
    }
    if(0 == bFindFlag)
    {
        aloge("fatal error! frameId[%d] is not find", nFrameId);
        ret = -1;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return ret;
}

static ERRORTYPE SampleFishCallbackWrapper(void *cookie,MPP_CHN_S *Port, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleFishFrameManager *pContext = (SampleFishFrameManager*)cookie;
    if(MOD_ID_ISE == Port->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RELEASE_ISE_VIDEO_BUFFER0:
            {
                alogd("SampleFishCallbackWrapper");
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                pContext->ReleaseFrame(pContext, pVideoFrameInfo->mId);
                pthread_mutex_lock(&pContext->mWaitFrameLock);
                if(pContext->mbWaitFrameFlag)
                {
                    pContext->mbWaitFrameFlag = 0;
                    cdx_sem_up(&pContext->mSemFrameCome);
                }
                pthread_mutex_unlock(&pContext->mWaitFrameLock);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!",
                        event, Port->mModId, Port->mDevId, Port->mChnId);
                ret = FALSE;
                break;
            }
        }
    }
    return ret;
}
void SampleGdcFishCallback(void* handle, sVec2D srcPos, sVec2D *dstPos, sRectifyPara rectPara)
{
    alogd("SampleGdcFishCallback s.x:%f,s.y:%f",srcPos.x,srcPos.y);
}
int initSampleFishFrameManager(SampleFishParameter *pFishParameter, int nFrameNum)
{
    int ret = 0;
    ret = pthread_mutex_init(&pFishParameter->mFrameManager.mLock, NULL);
    if(ret!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
        return ret;
    }
    ret = pthread_mutex_init(&pFishParameter->mFrameManager.mWaitFrameLock, NULL);
    if(ret != 0)
    {
        aloge("fatal error! pthread mutex init fail!");
        return ret;
    }
    ret = cdx_sem_init(&pFishParameter->mFrameManager.mSemFrameCome, 0);
    if(ret != 0)
    {
        aloge("cdx sem init fail!");
        return ret;
    }
    Thread_EXIT = FALSE;
    INIT_LIST_HEAD(&pFishParameter->mFrameManager.mIdleList);
    INIT_LIST_HEAD(&pFishParameter->mFrameManager.mUsingList);

    FILE* fd = NULL;
    fd = pFishParameter->PictureCap.PicFilePath;

    int width = 0, height = 0;
    width = pFishParameter->PictureCap.PicWidth;
    height = pFishParameter->PictureCap.PicHeight;
    int i = 0;
    SampleFishFrameNode *pNode;
    unsigned int uPhyAddr;
    void *pVirtAddr;
    unsigned int block_size = 0;
    unsigned int read_size = 0;
    aloge("=============w %d h %d=============",width,height);
    ret = ion_memOpen();
    if (ret != 0)
    {
        aloge("Open ion failed!");
        return ret;
    }

    for(i=0; i<nFrameNum; i++)
    {
        aloge("=============w %d h %d=============",width,height);
        pNode = (SampleFishFrameNode*)malloc(sizeof(SampleFishFrameNode));
        memset(pNode, 0, sizeof(SampleFishFrameNode));
        pNode->mFrame.mId = i;
#if 0
        IonAllocAttr allocAttr;
        memset(&allocAttr, 0, sizeof(IonAllocAttr));
        aloge("w %d h %d",width,height);
        allocAttr.mLen = width * height * 1.5;
        allocAttr.mAlign = 0;
        allocAttr.mIonHeapType = IonHeapType_IOMMU;
        allocAttr.mbSupportCache = 0;
        pNode->mFrame.VFrame.mpVirAddr[0] = ion_allocMem_extend(&allocAttr);
        if (pNode->mFrame.VFrame.mpVirAddr[0] == NULL)
        {
            aloge("allocMem error!");
            return -1;
        }
        memset(pNode->mFrame.VFrame.mpVirAddr[0], 0x0, width * height * 1.5);
        pNode->mFrame.VFrame.mPhyAddr[0] = (unsigned int)ion_getMemPhyAddr(pNode->mFrame.VFrame.mpVirAddr[0]);
        pNode->mFrame.VFrame.mpVirAddr[1] = pNode->mFrame.VFrame.mpVirAddr[0] + width * height;
        pNode->mFrame.VFrame.mPhyAddr[1] = pNode->mFrame.VFrame.mPhyAddr[0] + width * height;
        block_size = width * height * sizeof(unsigned char);
#endif
        IonAllocAttr allocAttr;
        memset(&allocAttr, 0, sizeof(IonAllocAttr));
        allocAttr.mLen = ALIGN(width,16)*ALIGN(height,16);
        allocAttr.mAlign = 0;
        allocAttr.mIonHeapType = IonHeapType_IOMMU;
        allocAttr.mbSupportCache = 0;
        pNode->mFrame.VFrame.mpVirAddr[0] = ion_allocMem_extend(&allocAttr);
        if (pNode->mFrame.VFrame.mpVirAddr[0] == NULL)
        {
            aloge("allocMem error!");
			free(pNode);
            return -1;
        }
        memset(pNode->mFrame.VFrame.mpVirAddr[0], 0x0, allocAttr.mLen);
        pNode->mFrame.VFrame.mPhyAddr[0] = (unsigned int)ion_getMemPhyAddr(pNode->mFrame.VFrame.mpVirAddr[0]);

        memset(&allocAttr, 0, sizeof(IonAllocAttr));
        allocAttr.mLen = (ALIGN(width,16)*ALIGN(height,16))/2;
        allocAttr.mAlign = 0;
        allocAttr.mIonHeapType = IonHeapType_IOMMU;
        allocAttr.mbSupportCache = 0;
        pNode->mFrame.VFrame.mpVirAddr[1] = ion_allocMem_extend(&allocAttr);
        if (pNode->mFrame.VFrame.mpVirAddr[1] == NULL)
        {
            aloge("allocMem error!");
			free(pNode);
            return -1;
        }
        memset(pNode->mFrame.VFrame.mpVirAddr[1], 0x0, allocAttr.mLen);
        pNode->mFrame.VFrame.mPhyAddr[1] = (unsigned int)ion_getMemPhyAddr(pNode->mFrame.VFrame.mpVirAddr[1]);

        block_size = ALIGN(width,16)*ALIGN(height,16);
//        aloge("mpVirAddr[0] %p,mpVirAddr[1] %p",pNode->mFrame.VFrame.mpVirAddr[0],pNode->mFrame.VFrame.mpVirAddr[1]);
//        aloge("mPhyAddr[0] %#x,mPhyAddr[1] %#x",pNode->mFrame.VFrame.mPhyAddr[0],pNode->mFrame.VFrame.mPhyAddr[1]);
        read_size = fread(pNode->mFrame.VFrame.mpVirAddr[0], 1 ,width*height, fd);
        if (read_size < 0)
        {
            aloge("read yuv file fail\n");
            fclose(fd);
			free(pNode);
            fd = NULL;
            return -1;
        }
        block_size =  (ALIGN(width,16)*ALIGN(height,16))/2;
        read_size = fread(pNode->mFrame.VFrame.mpVirAddr[1], 1, width*height, fd);
        if (read_size < 0)
        {
            aloge("read yuv file fail\n");
            fclose(fd);
			free(pNode);
            fd = NULL;
            return -1;
        }
        ion_flushCache(pNode->mFrame.VFrame.mpVirAddr[0],width * height);
        ion_flushCache(pNode->mFrame.VFrame.mpVirAddr[1],((width * height)/2));
        pNode->mFrame.VFrame.mWidth = width;
        pNode->mFrame.VFrame.mHeight = height;
        list_add_tail(&pNode->mList, &pFishParameter->mFrameManager.mIdleList);
    }
    fclose(fd);
    aloge("=============w %d h %d=============",width,height);
    pFishParameter->mFrameManager.mNodeCnt = nFrameNum;
    pFishParameter->mFrameManager.PrefetchFirstIdleFrame = SampleFishFrameManager_PrefetchFirstIdleFrame;
    pFishParameter->mFrameManager.UseFrame = SampleFishFrameManager_UseFrame;
    pFishParameter->mFrameManager.ReleaseFrame = SampleFishFrameManager_ReleaseFrame;
    return 0;
}

int destroySampleFishFrameManager(SampleFishParameter *pFishParameter)
{
    if(!list_empty(&pFishParameter->mFrameManager.mUsingList))
    {
        aloge("fatal error! why using list is not empty");
    }
    int cnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pFishParameter->mFrameManager.mIdleList)
    {
        cnt++;
    }
    if(cnt != pFishParameter->mFrameManager.mNodeCnt)
    {
        aloge("fatal error! frame count is not match [%d]!=[%d]", cnt,
                pFishParameter->mFrameManager.mNodeCnt);
    }
    SampleFishFrameNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pFishParameter->mFrameManager.mIdleList, mList)
    {
        ion_freeMem(pEntry->mFrame.VFrame.mpVirAddr[0]);
        ion_freeMem(pEntry->mFrame.VFrame.mpVirAddr[1]);
        list_del(&pEntry->mList);
        free(pEntry);
    }
    pthread_mutex_destroy(&pFishParameter->mFrameManager.mLock);
    return 0;
}

/*MPI ise*/
int aw_iseport_creat(ISE_GRP IseGrp, ISE_CHN IsePort, ISE_CHN_ATTR_S *PortAttr)
{
    int ret = -1;
    alogd("===>>>AW_MPI_ISE_CreatePort");
    ret = AW_MPI_ISE_CreatePort(IseGrp, IsePort, PortAttr);
    if(ret < 0)
    {
        aloge("Create ISE Port failed,IseGrp= %d,IsePort = %d",IseGrp,IsePort);
        return ret ;
    }
    alogd("===>>>AW_MPI_ISE_SetPortAttr");
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
    alogd("=====>AW_MPI_ISE_CreateGroup");
    ret = AW_MPI_ISE_CreateGroup(IseGrp, pGrpAttr);
    if(ret < 0)
    {
        aloge("Create ISE Group failed, IseGrp= %d",IseGrp);
        return ret ;
    }
    alogd("=====>AW_MPI_ISE_SetGrpAttr");
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

static int ParseCmdLine(int argc, char **argv, SampleFishCmdLineParam *pCmdLinePara)
{
    alogd("sample_fish path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleFishCmdLineParam));
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
                "\t-path /home/sample_fish.conf\n");
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

static ERRORTYPE loadSampleFishConfig(SampleFishConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr = NULL;
    char *pStrPixelFormat = NULL,*EncoderType = NULL;
    int i = 0,ISEPortNum = 0;
    CONFPARSER_S stConfParser;
    char name[256];
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleFishConfig));
    pConfig->AutoTestCount = GetConfParaInt(&stConfParser, SAMPLE_FISH_Auto_Test_Count, 0);
    pConfig->Process_Count = GetConfParaInt(&stConfParser, SAMPLE_FISH_Process_Count, 0);

    /* Get Picture parameter*/
    pConfig->PicConfig.PicFrameRate = GetConfParaInt(&stConfParser, SAMPLE_Fish_Pic_Frame_Rate, 0);
    pConfig->PicConfig.PicWidth = GetConfParaInt(&stConfParser, SAMPLE_Fish_Pic_Width, 0);
    pConfig->PicConfig.PicHeight = GetConfParaInt(&stConfParser, SAMPLE_Fish_Pic_Height, 0);
    pConfig->PicConfig.PicStride = GetConfParaInt(&stConfParser, SAMPLE_Fish_Pic_Stride, 0);
    alogd("pic_width = %d, pic_height = %d, pic_frame_rate = %d\n",
       pConfig->PicConfig.PicWidth,pConfig->PicConfig.PicHeight,pConfig->PicConfig.PicFrameRate);
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_Fish_Pic_File_Path, NULL);
    strncpy(pConfig->PicConfig.PicFilePath, ptr, MAX_FILE_PATH_SIZE-1);
    pConfig->PicConfig.PicFilePath[MAX_FILE_PATH_SIZE-1] = '\0';

    /* Get ISE parameter*/
    pConfig->ISEGroupConfig.ISEPortNum = GetConfParaInt(&stConfParser, SAMPLE_Fish_ISE_Port_Num, 0);
    ISEPortNum = pConfig->ISEGroupConfig.ISEPortNum;
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_Fish_ISE_Output_File_Path, NULL);
    strncpy(pConfig->ISEGroupConfig.OutputFilePath, ptr, MAX_FILE_PATH_SIZE-1);
    pConfig->ISEGroupConfig.OutputFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
    alogd("ise output_file_path = %s\n",pConfig->ISEGroupConfig.OutputFilePath);

    pConfig->ISEGroupConfig.ISE_Dewarp_Mode = GetConfParaInt(&stConfParser, SAMPLE_Fish_Dewarp_Mode, 0);
    pConfig->ISEGroupConfig.Lens_Parameter_P = pConfig->PicConfig.PicWidth/3.1415;
    pConfig->ISEGroupConfig.Lens_Parameter_Cx = pConfig->PicConfig.PicWidth/2;
    pConfig->ISEGroupConfig.Lens_Parameter_Cy = pConfig->PicConfig.PicHeight/2;
    pConfig->ISEGroupConfig.Mount_Mode = GetConfParaInt(&stConfParser, SAMPLE_Fish_Mount_Mode, 0);
    /*---------------------------ISE GDC------------------------------------*/
    pConfig->ISEGroupConfig.ISE_GDC_Mount_Mode = GetConfParaInt(&stConfParser, SAMPLE_Fish_Gdc_Mount_Mode, 0);
    pConfig->ISEGroupConfig.ISE_GDC_Dewarp_Mode = GetConfParaInt(&stConfParser, SAMPLE_Fish_Gdc_Dewarp_Mode, 0);
    /*-----------------------------------------------------------------*/
    if(pConfig->ISEGroupConfig.ISE_Dewarp_Mode == WARP_NORMAL)
    {
        pConfig->ISEGroupConfig.normal_pan = (float)GetConfParaInt(&stConfParser, SAMPLE_Fish_NORMAL_Pan, 0);
        pConfig->ISEGroupConfig.normal_tilt = (float)GetConfParaInt(&stConfParser, SAMPLE_Fish_NORMAL_Tilt, 0);
        pConfig->ISEGroupConfig.normal_zoom = (float)GetConfParaInt(&stConfParser,   SAMPLE_Fish_NORMAL_Zoom, 0);
        alogd("pan = %f,tilt = %f,zoom = %f\n",pConfig->ISEGroupConfig.normal_pan,
                pConfig->ISEGroupConfig.normal_tilt,pConfig->ISEGroupConfig.normal_zoom);
    }
    else if(pConfig->ISEGroupConfig.ISE_Dewarp_Mode == WARP_PTZ4IN1)
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
    alogd("ISE Group Parameter:dewarp_mode = %d,mount_mode = %d,Lens_Parameter_p = %f,"
            "Lens_Parameter_cx = %f,Lens_Parameter_cy = %f",
            pConfig->ISEGroupConfig.ISE_Dewarp_Mode,pConfig->ISEGroupConfig.Mount_Mode,
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
        alogd("ISE Port%d Parameter:ISE_Width = %d,ISE_Height = %d,ISE_Stride = %d,"
               "ISE_flip_enable = %d,mirror_enable = %d\n",i,pConfig->ISEPortConfig[i].ISEWidth,
                pConfig->ISEPortConfig[i].ISEHeight,pConfig->ISEPortConfig[i].ISEStride,
                pConfig->ISEPortConfig[i].flip_enable,pConfig->ISEPortConfig[i].mirror_enable);
    }
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

static void *Loop_SendImageFileThread(void *pArg)
{
    alogd("Start Loop_SendImageFileThread");
    SampleFishParameter *pCap = (SampleFishParameter *)pArg;
    ISE_GRP ISEGroup = pCap->pISEGroupCap[0].ISE_Group;
    int ret = 0;
    int i = 0;
    int s32MilliSec = -1;
    int framerate = pCap->PictureCap.PicFrameRate;
    VIDEO_FRAME_INFO_S *pFrameInfo = NULL;
    ret = AW_MPI_ISE_Start(ISEGroup);
    if(ret < 0)
    {
        aloge("ise start error!\n");
        return (void*)ret;
    }

    while((i != pCap->Process_Count) || (pCap->Process_Count == -1))
    {
        //request idle frame
        pFrameInfo = pCap->mFrameManager.PrefetchFirstIdleFrame(&pCap->mFrameManager);
        if(NULL == pFrameInfo)
        {
            pthread_mutex_lock(&pCap->mFrameManager.mWaitFrameLock);
            pFrameInfo = pCap->mFrameManager.PrefetchFirstIdleFrame(&pCap->mFrameManager);
            if(pFrameInfo!=NULL)
            {
                pthread_mutex_unlock(&pCap->mFrameManager.mWaitFrameLock);
            }
            else
            {
                pCap->mFrameManager.mbWaitFrameFlag = 1;
                pthread_mutex_unlock(&pCap->mFrameManager.mWaitFrameLock);
                cdx_sem_down_timedwait(&pCap->mFrameManager.mSemFrameCome, 500);
                continue;
            }
        }
        pCap->mFrameManager.UseFrame(&pCap->mFrameManager, pFrameInfo);
        alogd("AW_MPI_ISE_SendPic==> i:%d pCap->Process_Count:%d",i,pCap->Process_Count);
        ret = AW_MPI_ISE_SendPic(ISEGroup, pFrameInfo, NULL, s32MilliSec);
        if(ret != SUCCESS)
        {
            alogd("impossible, send frameId[%d] fail?", pFrameInfo->mId);
            pCap->mFrameManager.ReleaseFrame(&pCap->mFrameManager, pFrameInfo->mId);
            continue;
        }
        i++;
//        while (!g_bProcessDone) { usleep(1*1000); g_bProcessDone = 0; }
        usleep(1000*1000/framerate);
    }
    Thread_EXIT++;
    return NULL;
}

static void *Loop_GetIseData(void *pArg)
{
    alogd("Loop Start %s.\r\n", __func__);
    int i = 0,ret = 0,j =0;
    int width = 0,height = 0;
    BOOL Dynamic_Flag = FALSE;
    ISE_CHN_ATTR_S ISEPortAttr;
    ISE_PortCap_S *pCap = (ISE_PortCap_S *)pArg;
    ISE_GRP ISEGroup = pCap->ISE_Group;
    ISE_CHN ISEPort = pCap->ISE_Port;
    pthread_t thread_id = pCap->thread_id;
    int s32MilliSec = pCap->s32MilliSec;
    VIDEO_FRAME_INFO_S ISE_Frame_buffer;
    // YUV Out
    width = pCap->width;
    height = pCap->height;
    char *name = pCap->OutputFilePath;
    static struct timeval last_call_tv = {0}, cur_call_tv = {0};
    static long call_diff_ms = -1;
    alogd("Loop_GetIseData W*H=%d*%d",width,height);
    while(1)
    {
        if(Thread_EXIT == ISE_GROUP_INS_CNT)
        {
            alogd("Thread_EXIT is True!\n");
            break;
        }
        if ((ret = AW_MPI_ISE_GetData(ISEGroup, ISEPort, &ISE_Frame_buffer, s32MilliSec)) < 0)
        {
            aloge("ISE Port%d get data failed!\n",ISEPort);
            continue ;
        }
        else
        {
            j++;
            if (j % 30 == 0)
            {
                time_t now;
                struct tm *timenow;
                time(&now);
                timenow = localtime(&now);
                alogd("Cap threadid = 0x%lx,port = %d; local time is %s\r\n",
                        thread_id, ISEPort, asctime(timenow));
            }

#if Save_Picture
            if(i % 300 == 0)
            {
                alogd("Save_Picture");
                char filename[125];
                sprintf(filename,"/%s/fish_ch%dport%d_%d.yuv",name, width, height,i);
                FILE* fd = fopen(filename,"wb+");
                fwrite(ISE_Frame_buffer.VFrame.mpVirAddr[0], width*height, 1, fd);
                fwrite(ISE_Frame_buffer.VFrame.mpVirAddr[1], width*height/2 , 1, fd);
                fclose(fd);
                if(pCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode == Warp_BirdsEye)
                {
                    char filename[125];
                    int ldc_width=0,ldc_height=0;
                    ldc_width = pCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.width;
                    ldc_height = pCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.height;
                    sprintf(filename,"/%s/ldc_out_fish_ch%dport%d_%d.yuv",name, ldc_width, ldc_height,i);
                    FILE *fd = NULL;
                    fd = fopen(filename,"wb+");
                    fwrite(ISE_Frame_buffer.VFrame.mpVirAddr[0]+width*height, ldc_width*ldc_height, 1, fd);
                    fwrite(ISE_Frame_buffer.VFrame.mpVirAddr[1]+width*height/2, ldc_width*ldc_height/2 , 1, fd);
                    fclose(fd);
                }
            }
#endif
            AW_MPI_ISE_ReleaseData(ISEGroup, ISEPort, &ISE_Frame_buffer);
            i++;
#if Dynamic_PTZ
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
            if (call_diff_ms < 0) {
                gettimeofday(&last_call_tv, NULL);
            }
            gettimeofday(&cur_call_tv, NULL);

            call_diff_ms = (cur_call_tv.tv_sec*1000 + cur_call_tv.tv_usec/1000) -
                                 (last_call_tv.tv_sec*1000 + last_call_tv.tv_usec/1000);
            if(pCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode == Warp_Ptz4In1)
            {
                if(ISEPort == 0  && (call_diff_ms >= 60 || call_diff_ms == 0))
                {
//                    aloge("set gdc attr");
                    last_call_tv.tv_sec = cur_call_tv.tv_sec;
                    last_call_tv.tv_usec = cur_call_tv.tv_usec;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[0].pan = 45;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[0].tilt = 0;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[0].zoomH = 100;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[1].pan = 45;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[1].tilt = 0;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[1].zoomH = 100;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[2].pan = 45;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[2].tilt = 0;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[2].zoomH = 100;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[3].pan = 45;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[3].tilt = 0;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.ptz4In1Para[3].zoomH = 100;
                    AW_MPI_ISE_SetPortAttr(ISEGroup,ISEPort,&ISEPortAttr);
    //                Dynamic_Flag = TRUE;
                }
            }
            else if(pCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode == Warp_Normal)
            {
                if(ISEPort == 0 && (call_diff_ms >= 60 || call_diff_ms == 0))
                {
//                    aloge("set port attr");
                    last_call_tv.tv_sec = cur_call_tv.tv_sec;
                    last_call_tv.tv_usec = cur_call_tv.tv_usec;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.fishEyePara.ptz.pan = 45;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.fishEyePara.ptz.tilt = 0;
                    ISEPortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.fishEyePara.ptz.zoomH = 100;
                    AW_MPI_ISE_SetPortAttr(ISEGroup,ISEPort,&ISEPortAttr);
    //                Dynamic_Flag = TRUE;
                }
            }
#elif (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
            if (call_diff_ms < 0) {
                gettimeofday(&last_call_tv, NULL);
            }
            gettimeofday(&cur_call_tv, NULL);

            call_diff_ms = (cur_call_tv.tv_sec*1000 + cur_call_tv.tv_usec/1000) -
                                 (last_call_tv.tv_sec*1000 + last_call_tv.tv_usec/1000);
            if(pCap->PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_PTZ4IN1)
            {
                if(ISEPort == 0 && !Dynamic_Flag && (call_diff_ms >= 60 || call_diff_ms == 0))
                {
                    last_call_tv.tv_sec = cur_call_tv.tv_sec;
                    last_call_tv.tv_usec = cur_call_tv.tv_usec;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.ptzsub_chg[0] = 1;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.pan_sub[0] = 125.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.tilt_sub[0] = 45.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.zoom_sub[0] = 2.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.ptzsub_chg[1] = 1;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.pan_sub[1] = 90.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.tilt_sub[1] = 45.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.zoom_sub[1] = 2.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.ptzsub_chg[2] = 1;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.pan_sub[2] = 100.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.tilt_sub[2] = 45.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.zoom_sub[2] = 2.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.ptzsub_chg[3] = 1;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.pan_sub[3] = 150.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.tilt_sub[3] = 45.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.zoom_sub[3] = 2.0f;
                    AW_MPI_ISE_SetPortAttr(ISEGroup,ISEPort,&ISEPortAttr);
    //                Dynamic_Flag = TRUE;
                }
            }
            else if(pCap->PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_NORMAL)
            {
                if(ISEPort == 0 && !Dynamic_Flag && (call_diff_ms >= 60 || call_diff_ms == 0))
                {
                    last_call_tv.tv_sec = cur_call_tv.tv_sec;
                    last_call_tv.tv_usec = cur_call_tv.tv_usec;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.pan = 125.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.tilt = 45.0f;
                    ISEPortAttr.mode_attr.mFish.ise_cfg.zoom = 2.0f;
                    AW_MPI_ISE_SetPortAttr(ISEGroup,ISEPort,&ISEPortAttr);
    //                Dynamic_Flag = TRUE;
                }
            }
#endif
#endif /*Dynamic_PTZ*/
        }
    }
    return NULL;
}

void Sample_Fish_HELP()
{
    alogd("Run sample_fish command: ./sample_fish -path ./sample_fish.conf\r\n");
}

int main(int argc, char *argv[])
{
    int ret = 0, i = 0, j = 0, count = 0;
    int result = 0;
    int AutoTestCount = 0;
    alogd("Sample fish buile time = %s, %s.\r\n", __DATE__, __TIME__);
    if(argc != 3)
    {
        Sample_Fish_HELP();
        exit(0);
    }

    SampleFishParameter FisheyePara[4];
    SampleFishConfparser stContext;

    //parse command line param,read sample_virvi2fish2vo.conf
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
        pConfigFilePath = DEFAULT_SAMPLE_FISH_CONF_PATH;
    }
    //parse config file.
    if(loadSampleFishConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    AutoTestCount = stContext.mConfigPara.AutoTestCount;
    int ISEPortNum = 0;
    ISEPortNum = stContext.mConfigPara.ISEGroupConfig.ISEPortNum;

    while (count != AutoTestCount)
    {
        /* start mpp systerm */
        MPP_SYS_CONF_S mSysConf;
        mSysConf.nAlignWidth = 32;
        AW_MPI_SYS_SetConf(&mSysConf);
        ret = AW_MPI_SYS_Init();
        if (ret < 0)
        {
            aloge("sys init failed");
            result = -1;
            goto sys_exit;
        }

        if(!FILE_EXIST(stContext.mConfigPara.PicConfig.PicFilePath))
        {
            aloge("source pic file %s is not exist",stContext.mConfigPara.PicConfig.PicFilePath);
            result = -1;
            goto sys_exit;
        }

        for(j = 0; j < ISE_GROUP_INS_CNT; j++)
        {
            //init frame manager
            FisheyePara[j].Process_Count = stContext.mConfigPara.Process_Count;
            FisheyePara[j].PictureCap.PicWidth = stContext.mConfigPara.PicConfig.PicWidth;
            FisheyePara[j].PictureCap.PicHeight = stContext.mConfigPara.PicConfig.PicHeight;
            FisheyePara[j].PictureCap.PicStride = stContext.mConfigPara.PicConfig.PicStride;
            FisheyePara[j].PictureCap.PicFrameRate = ((float)1/stContext.mConfigPara.PicConfig.PicFrameRate) * 1000000;
            FisheyePara[j].PictureCap.PicFilePath = fopen(stContext.mConfigPara.PicConfig.PicFilePath,"rb");
            ret = initSampleFishFrameManager(&FisheyePara[j], 5);
            if(ret < 0)
            {
                aloge("Init FrameManager failed!");
                goto destory_framemanager;
            }

            ISE_PortCap_S *pISEPortCap;
#if  Load_Len_Parameter
            pISEPortCap = &FisheyePara[j].pISEGroupCap[0].PortCap_S[0];
            FILE *ise_cfg_fd = NULL;
            ise_cfg_fd = fopen("./mo_cfg.bin","rb+");
            fread(&pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.p,sizeof(float),1,ise_cfg_fd);
            fread(&pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cx,sizeof(float),1,ise_cfg_fd);
            fread(&pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cy,sizeof(float),1,ise_cfg_fd);
            alogd("load mo len parameter:p = %f, cx = %f, cy = %f",
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.p,
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cx,
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cy);
#endif
            /*Set ISE Group Attribute*/
            memset(&FisheyePara[j].pISEGroupCap[0], 0, sizeof(ISE_GroupCap_S));
            FisheyePara[j].pISEGroupCap[0].ISE_Group = j;
            FisheyePara[j].pISEGroupCap[0].pGrpAttr.iseMode = ISEMODE_ONE_FISHEYE;
            FisheyePara[j].pISEGroupCap[0].pGrpAttr.mPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

            alogd("===>aw_isegroup_creat");
            ret = aw_isegroup_creat(FisheyePara[j].pISEGroupCap[0].ISE_Group, &FisheyePara[j].pISEGroupCap[0].pGrpAttr);
            if(ret < 0)
            {
                aloge("ISE Group %d creat failed",FisheyePara[j].pISEGroupCap[0].ISE_Group);
                goto destory_isegroup;
            }
#if (MPPCFG_ISE_GDC == OPTION_ISE_GDC_ENABLE)
            alogd("==========>>>> ise gdc <<============\n");
            for(i = 0; i < ISEPortNum; i++)
            {
                /*Set ISE Port Attribute*/
                memset(&FisheyePara[j].pISEGroupCap[0].PortCap_S[i], 0, sizeof(ISE_PortCap_S));
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Group = j;
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Port = i;
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].thread_id = i;
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].s32MilliSec = 4000;
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].OutputFilePath =  stContext.mConfigPara.ISEGroupConfig.OutputFilePath;
                pISEPortCap = &FisheyePara[j].pISEGroupCap[0].PortCap_S[i];
                if(i == 0)
                {
                    ISE_CFG_PARA_GDC *ise_gdc_cfg = &pISEPortCap->PortAttr.mode_attr.mFish.ise_gdc_cfg;

                    ise_gdc_cfg->rectPara.warpMode = stContext.mConfigPara.ISEGroupConfig.ISE_GDC_Dewarp_Mode;
                    ise_gdc_cfg->srcImage.width = stContext.mConfigPara.PicConfig.PicWidth;
                    ise_gdc_cfg->srcImage.height = stContext.mConfigPara.PicConfig.PicHeight;
                    ise_gdc_cfg->srcImage.img_format = PLANE_YUV420; // YUV420
                    ise_gdc_cfg->srcImage.stride[0] = stContext.mConfigPara.PicConfig.PicStride;
                    switch(ise_gdc_cfg->rectPara.warpMode)
                    {
                        case Warp_LDC:
                            alogd("warpMode:Warp_LDC");
                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420; // YUV420
                            ise_gdc_cfg->undistImage.width = 2048;
                            ise_gdc_cfg->undistImage.height = 2048;
                            ise_gdc_cfg->undistImage.stride[0] = 2048;
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

                            ise_gdc_cfg->rectPara.warpPara.radialDistortCoef    = 0; //[-255,255]
                            ise_gdc_cfg->rectPara.warpPara.trapezoidDistortCoef = 0; //[-255,255]
                            ise_gdc_cfg->rectPara.LensDistPara.distModel = DistModel_FishEye;
                        break;
                        case Warp_Pano180:
                            alogd("warpMode:Warp_Pano180");
                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420; // YUV420
                            ise_gdc_cfg->undistImage.width = 2048;
                            ise_gdc_cfg->undistImage.height = 1024;
                            ise_gdc_cfg->undistImage.stride[0] = 2048;
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
                            //180全景校正参数
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.pan = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.tilt = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.zoomH = 100;
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.zoomV = 100;
                            ise_gdc_cfg->rectPara.warpPara.radialDistortCoef = 0;
                            ise_gdc_cfg->rectPara.warpPara.fanDistortCoef = 0;
                            ise_gdc_cfg->rectPara.warpPara.trapezoidDistortCoef = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.rotPara.roll  = 0;  //[-90,90]
                            ise_gdc_cfg->rectPara.fishEyePara.rotPara.pitch = 0;  //[-90,90]
                            ise_gdc_cfg->rectPara.fishEyePara.rotPara.yaw   = 0;  //[-90,90]
                            ise_gdc_cfg->rectPara.fishEyePara.mountMode = stContext.mConfigPara.ISEGroupConfig.ISE_GDC_Mount_Mode;
                        break;
                        case Warp_Pano360:
                            alogd("warpMode:Warp_Pano360");
                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420; // YUV420
                            ise_gdc_cfg->undistImage.width = 4096;
                            ise_gdc_cfg->undistImage.height = 1024;
                            ise_gdc_cfg->undistImage.stride[0] = 4096;
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
                            //360全景校正参数
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.pan = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.tilt = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.zoomH = 100;
                            ise_gdc_cfg->rectPara.fishEyePara.ptz.zoomV = 100;
                            ise_gdc_cfg->rectPara.fishEyePara.scale = 100;
                            ise_gdc_cfg->rectPara.fishEyePara.innerRadius = 0;

                            ise_gdc_cfg->rectPara.warpPara.trapezoidDistortCoef = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.rotPara.roll  = 0;  //[-90,90]
                            ise_gdc_cfg->rectPara.fishEyePara.rotPara.pitch = 0;  //[-90,90]
                            ise_gdc_cfg->rectPara.fishEyePara.rotPara.yaw   = 0;  //[-90,90]
                            ise_gdc_cfg->rectPara.fishEyePara.mountMode = stContext.mConfigPara.ISEGroupConfig.ISE_GDC_Mount_Mode;
                        break;
                        case Warp_Normal:
                            alogd("warpMode:Warp_Normal");
                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420; // YUV420
                            ise_gdc_cfg->undistImage.width = 2048;
                            ise_gdc_cfg->undistImage.height = 2048;
                            ise_gdc_cfg->undistImage.stride[0] = 2048;
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
                            ise_gdc_cfg->rectPara.fishEyePara.mountMode = stContext.mConfigPara.ISEGroupConfig.ISE_GDC_Mount_Mode;
                        break;
                        case Warp_Ptz4In1:
                            alogd("warpMode:Warp_Ptz4In1");
                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420; // YUV420
                            ise_gdc_cfg->undistImage.width = 2048;
                            ise_gdc_cfg->undistImage.height = 2048;
                            ise_gdc_cfg->undistImage.stride[0] = 2048;
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
                            ise_gdc_cfg->rectPara.ptz4In1Para[0].pan = 0;
                            ise_gdc_cfg->rectPara.ptz4In1Para[0].tilt = 0;
                            ise_gdc_cfg->rectPara.ptz4In1Para[0].zoomH = 100;

                            ise_gdc_cfg->rectPara.ptz4In1Para[1].pan = 20;
                            ise_gdc_cfg->rectPara.ptz4In1Para[1].tilt = 0;
                            ise_gdc_cfg->rectPara.ptz4In1Para[1].zoomH = 100;

                            ise_gdc_cfg->rectPara.ptz4In1Para[2].pan = 0;
                            ise_gdc_cfg->rectPara.ptz4In1Para[2].tilt = 25;
                            ise_gdc_cfg->rectPara.ptz4In1Para[2].zoomH = 100;

                            ise_gdc_cfg->rectPara.ptz4In1Para[3].pan = 20;
                            ise_gdc_cfg->rectPara.ptz4In1Para[3].tilt = 10;
                            ise_gdc_cfg->rectPara.ptz4In1Para[3].zoomH = 100;

                            ise_gdc_cfg->rectPara.fishEyePara.scale = 100;
                            ise_gdc_cfg->rectPara.warpPara.trapezoidDistortCoef = 0;
                            ise_gdc_cfg->rectPara.fishEyePara.mountMode = stContext.mConfigPara.ISEGroupConfig.ISE_GDC_Mount_Mode;
                        break;
                        case Warp_Fish2Wide:
                            alogd("warpMode:Warp_Fish2Wide");
                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420; // YUV420
                            ise_gdc_cfg->undistImage.width = 2048;
                            ise_gdc_cfg->undistImage.height = 1024;
                            ise_gdc_cfg->undistImage.stride[0] = 2048;
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
                            ise_gdc_cfg->rectPara.fishEyePara.rotPara.roll  = 0;//[-90,90]
                            ise_gdc_cfg->rectPara.fishEyePara.rotPara.pitch = 0;//[-90,90]
                            ise_gdc_cfg->rectPara.fishEyePara.rotPara.yaw   = 0;//[-180,180]
                            ise_gdc_cfg->rectPara.fishEyePara.mountMode = stContext.mConfigPara.ISEGroupConfig.ISE_GDC_Mount_Mode;
                        break;
                        case Warp_BirdsEye:
                            alogd("warpMode:Warp_BirdsEye");
                            ise_gdc_cfg->birdsEyeImage.img_format = PLANE_YUV420;
                            ise_gdc_cfg->birdsEyeImage.width      = 768;
                            ise_gdc_cfg->birdsEyeImage.height     = 1080;
                            ise_gdc_cfg->birdsEyeImage.stride[0]  = 768;
                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420;
                            ise_gdc_cfg->undistImage.width  = 1920;
                            ise_gdc_cfg->undistImage.height = 1080;
                            ise_gdc_cfg->undistImage.stride[0] = 1920;
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
                            ise_gdc_cfg->rectPara.warpPara.radialDistortCoef    = 0; //[-255,255]
                            ise_gdc_cfg->rectPara.warpPara.trapezoidDistortCoef = 0; //[-255,255]
                            ise_gdc_cfg->rectPara.LensDistPara.distModel = DistModel_FishEye;

                            sBirdsEyePara *birdsPara = &ise_gdc_cfg->rectPara.birdsEyePara;
                            birdsPara->outWidth  = ise_gdc_cfg->birdsEyeImage.width;  //set 0, aspect ratio will be preserved.
                            birdsPara->outHeight = ise_gdc_cfg->birdsEyeImage.height; //set 0, aspect ratio will be preserved.
                            birdsPara->offsetX = 0; //default: 0
                            birdsPara->offsetY = 0; //default: 0
                            birdsPara->zoomH = 100;
                            birdsPara->zoomV = 100;
                            //camera mount height
                            birdsPara->mountHeight = 0.85f;  //meters
                            //camera mount angle
                            birdsPara->rotPara.roll   = -21;
                            birdsPara->rotPara.pitch  = 0;
                            birdsPara->rotPara.yaw    = 0;
                            //meters
                            birdsPara->roiDist_ahead  =  4.5f;  //default:  4.5
                            birdsPara->roiDist_left   = -1.5f;  //default: -1.5
                            birdsPara->roiDist_right  =  1.5f;  //default:  1.5
                            birdsPara->roiDist_bottom =  0.65f; //default:  0.65
                        break;
                        case Warp_User:
                            ise_gdc_cfg->birdsEyeImage.img_format = PLANE_YUV420;
                            ise_gdc_cfg->birdsEyeImage.width      = 768;
                            ise_gdc_cfg->birdsEyeImage.height     = 1080;
                            ise_gdc_cfg->birdsEyeImage.stride[0]  = 768;

                            ise_gdc_cfg->undistImage.img_format = PLANE_YUV420;
                            ise_gdc_cfg->undistImage.width  = 1920;
                            ise_gdc_cfg->undistImage.height = 1080;
                            ise_gdc_cfg->undistImage.stride[0] = 1920;

                            sFishEyePara *fParams = &ise_gdc_cfg->rectPara.fishEyePara;
                            sLensDistortPara *wParams = &ise_gdc_cfg->rectPara.LensDistPara;
                            //typedef void (*cbRectifyPixel)(void* handle, sVec2D srcPos, sVec2D *dstPos, sRectifyPara rectPara);
                            alogd("callback %p",SampleGdcFishCallback);
                            pISEPortCap->PortAttr.UserCallback = SampleGdcFishCallback;
                        break;
                        default:
                        alogd("Input the warp error!!!");
                        break;
                    }
                    alogd("=====>>aw_iseport_creat");
                    ret = aw_iseport_creat(FisheyePara[j].pISEGroupCap[0].ISE_Group, FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Port,
                                           &pISEPortCap->PortAttr);
                    if(ret < 0)
                    {
                         aloge("ISE Port%d creat failed",FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Port);
                         goto destory_iseport;
                    }
                    if(pISEPortCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.rectPara.warpMode == Warp_BirdsEye)
                    {
                        FisheyePara[j].pISEGroupCap[0].PortCap_S[i].width = pISEPortCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.birdsEyeImage.width;
                        FisheyePara[j].pISEGroupCap[0].PortCap_S[i].height = pISEPortCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.birdsEyeImage.height;
                    }
                    else
                    {
                        FisheyePara[j].pISEGroupCap[0].PortCap_S[i].width = pISEPortCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.width;
                        FisheyePara[j].pISEGroupCap[0].PortCap_S[i].height = pISEPortCap->PortAttr.mode_attr.mFish.ise_gdc_cfg.undistImage.height;
                    }
                }
            }

#elif (MPPCFG_ISE_MO == OPTION_ISE_MO_ENABLE)
            for(i = 0; i < ISEPortNum; i++)
            {
                alogd("==========>>>> ise mo <<============\n");
                /*Set ISE Port Attribute*/
                memset(&FisheyePara[j].pISEGroupCap[0].PortCap_S[i], 0, sizeof(ISE_PortCap_S));
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Group = j;
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Port = i;
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].thread_id = i;
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].s32MilliSec = 4000;
                FisheyePara[j].pISEGroupCap[0].PortCap_S[i].OutputFilePath =  stContext.mConfigPara.ISEGroupConfig.OutputFilePath;
                pISEPortCap = &FisheyePara[j].pISEGroupCap[0].PortCap_S[i];
                if(i == 0)  //fish arttr
                {
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode = stContext.mConfigPara.ISEGroupConfig.ISE_Dewarp_Mode;
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.in_h = stContext.mConfigPara.PicConfig.PicHeight;
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.in_w = stContext.mConfigPara.PicConfig.PicWidth;
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.p  = stContext.mConfigPara.ISEGroupConfig.Lens_Parameter_P;
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cx = stContext.mConfigPara.ISEGroupConfig.Lens_Parameter_Cx;
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cy = stContext.mConfigPara.ISEGroupConfig.Lens_Parameter_Cy;
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.in_yuv_type = 0; // YUV420
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_yuv_type = 0; // YUV420
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.in_luma_pitch = stContext.mConfigPara.PicConfig.PicWidth;
                    pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.in_chroma_pitch = stContext.mConfigPara.PicConfig.PicWidth;
                    if(pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_NORMAL)
                    {
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.mount_mode = stContext.mConfigPara.ISEGroupConfig.Mount_Mode;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.pan = stContext.mConfigPara.ISEGroupConfig.normal_pan;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.tilt = stContext.mConfigPara.ISEGroupConfig.normal_tilt;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.zoom = stContext.mConfigPara.ISEGroupConfig.normal_zoom;
                    }
                    if(pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_PANO360)
                    {
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.mount_mode = stContext.mConfigPara.ISEGroupConfig.Mount_Mode;
                    }
                    if(pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_180WITH2)
                    {
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.mount_mode = stContext.mConfigPara.ISEGroupConfig.Mount_Mode;
                    }
                    if(pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.dewarp_mode == WARP_UNDISTORT)
                    {
#if 1
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cx = 918.18164 * (stContext.mConfigPara.PicConfig.PicWidth*1.0/1920);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cy = 479.86784 * (stContext.mConfigPara.PicConfig.PicHeight*1.0/1080);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.fx  = 1059.11146 * (stContext.mConfigPara.PicConfig.PicWidth*1.0/1920);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.fy = 1053.26240 * (stContext.mConfigPara.PicConfig.PicHeight*1.0/1080);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cxd = 876.34066 * (stContext.mConfigPara.PicConfig.PicWidth*1.0/1920);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cyd = 465.93162 * (stContext.mConfigPara.PicConfig.PicHeight*1.0/1080);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.fxd = 691.76196 * (stContext.mConfigPara.PicConfig.PicWidth*1.0/1920);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.fyd = 936.82897 * (stContext.mConfigPara.PicConfig.PicHeight*1.0/1080);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[0] = 0.182044494560808;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[1] = -0.1481043082174997;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[2] = -0.005128687334715951;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[3] = 0.567926713301489;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[4] = -0.1789466261819578;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[5] = -0.03561367966855939;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.p_undis[0] = 0.000649146914880072;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.p_undis[1] = 0.0002534155740808075;
#endif

#if 1
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cx = 889.50411 * (stContext.mConfigPara.PicConfig.PicWidth*1.0/1920);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cy = 522.42100 * (stContext.mConfigPara.PicConfig.PicHeight*1.0/1080);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.fx  = 963.98364 * (stContext.mConfigPara.PicConfig.PicWidth*1.0/1920);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.fy = 965.10729 * (stContext.mConfigPara.PicConfig.PicHeight*1.0/1080);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cxd = 801.04388 * (stContext.mConfigPara.PicConfig.PicWidth*1.0/1920);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.cyd = 518.79163 * (stContext.mConfigPara.PicConfig.PicHeight*1.0/1080);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.fxd = 580.58685 * (stContext.mConfigPara.PicConfig.PicWidth*1.0/1920);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.fyd = 850.71746 * (stContext.mConfigPara.PicConfig.PicHeight*1.0/1080);
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[0] = 0.5874970340655806;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[1] = 0.01263866896598456;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[2] = -0.003440797814786819;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[3] = 0.9486826799125321;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[4] = 0.1349250696268053;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.k[5] = -0.01052234728693081;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.p_undis[0] = 0.000362407777916013;
                        pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.p_undis[1] = -0.0001245920435755955;
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
                 pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_en[i] = 1;
                 pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_w[i] =  stContext.mConfigPara.ISEPortConfig[i].ISEWidth;
                 pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_h[i] =  stContext.mConfigPara.ISEPortConfig[i].ISEHeight;
                 pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_flip[i] =  stContext.mConfigPara.ISEPortConfig[i].flip_enable;
                 pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_mirror[i] =  stContext.mConfigPara.ISEPortConfig[i].mirror_enable;
                 pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_luma_pitch[i] =  stContext.mConfigPara.ISEPortConfig[i].ISEStride;
                 pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_chroma_pitch[i] =  stContext.mConfigPara.ISEPortConfig[i].ISEStride;
                 ret = aw_iseport_creat(FisheyePara[j].pISEGroupCap[0].ISE_Group, FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Port,
                                     &pISEPortCap->PortAttr);
                 if(ret < 0)
                 {
                     aloge("ISE Port%d creat failed",FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Port);
                     goto destory_iseport;
                 }
                 FisheyePara[j].pISEGroupCap[0].PortCap_S[i].width = pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_w[i];
                 FisheyePara[j].pISEGroupCap[0].PortCap_S[i].height = pISEPortCap->PortAttr.mode_attr.mFish.ise_cfg.out_h[i];
            }
#endif

            MPPCallbackInfo cbInfo;
            cbInfo.cookie = (void*)&FisheyePara[j].mFrameManager;
            cbInfo.callback = (MPPCallbackFuncType)&SampleFishCallbackWrapper;
            ret = AW_MPI_ISE_RegisterCallback(FisheyePara[j].pISEGroupCap[0].ISE_Group, &cbInfo);
            if(ret != SUCCESS)
            {
                aloge("Fish Register Callback error!\n");
                result = -1;
                goto _exit;
            }
            alogd("AW_MPI_ISE_SetISEFreq!!");
            ret = AW_MPI_ISE_SetISEFreq(FisheyePara[j].pISEGroupCap[0].ISE_Group, 300);
            ret = pthread_create(&FisheyePara[j].PictureCap.thread_id, NULL, Loop_SendImageFileThread, (void *)&FisheyePara[j]);
            if (ret < 0)
            {
                aloge("Loop_SendImageFileThread create failed");
                result = -1;
                goto _exit;
            }
            for(i = 0;i < ISEPortNum;i++)
            {
                pthread_create(&FisheyePara[j].pISEGroupCap[0].PortCap_S[i].thread_id, NULL,
                        Loop_GetIseData, (void *)&FisheyePara[j].pISEGroupCap[0].PortCap_S[i]);
            }
        }
        for (j = 0; j < ISE_GROUP_INS_CNT; j++)
        {
            pthread_join(FisheyePara[j].PictureCap.thread_id, NULL);
            for(i = 0;i < ISEPortNum;i++)
            {
                pthread_join(FisheyePara[j].pISEGroupCap[0].PortCap_S[i].thread_id, NULL);
            }

            ret = AW_MPI_ISE_Stop(FisheyePara[j].pISEGroupCap[0].ISE_Group);
            if(ret < 0)
            {
                aloge("ise stop error!\n");
                result = -1;
                goto _exit;
            }
    destory_iseport:
            for(i = 0;i < ISEPortNum;i++)
            {
                alogd("=====>>aw_iseport_destory");
                ret = aw_iseport_destory(FisheyePara[j].pISEGroupCap[0].ISE_Group,
                        FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Port);
                if(ret < 0)
                {
                    aloge("ISE Port%d distory error!",FisheyePara[j].pISEGroupCap[0].PortCap_S[i].ISE_Port);
                    result = -1;
                    goto _exit;
                }
            }

    destory_isegroup: 
            alogd("=====>>aw_isegroup_destory");
            ret = aw_isegroup_destory(FisheyePara[j].pISEGroupCap[0].ISE_Group);
            if(ret < 0)
            {
                aloge("ISE Destroy Group%d error!",FisheyePara[j].pISEGroupCap[0].ISE_Group);
                result = -1;
                goto _exit;
            }
    destory_framemanager:
            alogd("=====>>destroySampleFishFrameManager");
            destroySampleFishFrameManager(&FisheyePara[j]);
            ret = ion_memClose();
            if (ret != 0)
            {
                aloge("Close ion failed!");
            }
        }

sys_exit:
        /* exit mpp systerm */
        ret = AW_MPI_SYS_Exit();
        if (ret < 0)
        {
            aloge("sys exit failed!");
            result = -1;
            goto _exit;
        }
        alogd("======================================.\r\n");
        alogd("Auto Test count end: %d. (MaxCount==1000).\r\n", count);
        alogd("======================================.\r\n");
        count ++;
    }

_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}


