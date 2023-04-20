/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2018/01/22
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleDemux2VdecSaveFrame"

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "plat_log.h"
#include <VIDEO_FRAME_INFO_S.h>
#include <ClockCompPortIndex.h>
#include <mpi_demux.h>
#include <mpi_vdec.h>
#include <mpi_clock.h>

#include "sample_demux2vdec_saveFrame_config.h"
#include "sample_demux2vdec_saveFrame.h"

static SampleDemux2VdecSaveFrameContext gSampleDemux2VdecSaveFrameContext;


static ERRORTYPE SampleDemux2VdecSaveFrameContext_Init(SampleDemux2VdecSaveFrameContext *pContext)
{
    memset(pContext, 0, sizeof(SampleDemux2VdecSaveFrameContext));
    pContext->mDmxChn = MM_INVALID_CHN;
    pContext->mVdecChn = MM_INVALID_CHN;
    pContext->mClockChn = MM_INVALID_CHN;
    return SUCCESS;
}
static ERRORTYPE SampleDemux2VdecSaveFrameContext_Destroy(SampleDemux2VdecSaveFrameContext *pContext)
{
    if(pContext->mDmxChn != MM_INVALID_CHN)
    {
        aloge("fatal error! dmxChn[%d] is not destruct", pContext->mDmxChn);
    }
    if(pContext->mVdecChn != MM_INVALID_CHN)
    {
        aloge("fatal error! vdecChn[%d] is not destruct", pContext->mVdecChn);
    }
    if(pContext->mClockChn != MM_INVALID_CHN)
    {
        aloge("fatal error! clockChn[%d] is not destruct", pContext->mClockChn);
    }
    return SUCCESS;
}

static int ParseCmdLine(int argc, char *argv[], SampleDemux2VdecSaveFrameCmdLineParam *pCmdLinePara)
{
    alogd("sample_demux2vdec_saveFrame path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleDemux2VdecSaveFrameCmdLineParam));
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
            printf("CmdLine param example:\n"
                "\t ./sample_demux2vdec_saveFrame -path /mnt/extsd/sample_demux2vdec_saveFrame.conf\n");
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

static ERRORTYPE LoadSampleDemux2VdecSaveFrameConfig(SampleDemux2VdecSaveFrameConfig *pConfig, const char *pConfPath)
{
    int ret;
    char *ptr;
    if(NULL == pConfPath || 0 == strlen(pConfPath))
    {
        alogd("user don't set config file. use default test parameter!");
        strcpy(pConfig->mSrcFile, "/mnt/extsd/h264.mp4");
        pConfig->mSeekTime = 0;
        pConfig->mSaveNum = 1;
        strcpy(pConfig->mDstDir, "/mnt/extsd");
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(pConfPath, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleDemux2VdecSaveFrameConfig));
    char *pSrcFilePath = (char*)GetConfParaString(&stConfParser, SAMPLE_DMX2VDEC_SAVEFRAME_KEY_SRC_FILE, NULL);
    strcpy(pConfig->mSrcFile, pSrcFilePath);
    pConfig->mSeekTime = GetConfParaInt(&stConfParser, SAMPLE_DMX2VDEC_SAVEFRAME_KEY_SEEK_POSITION, 0);
    pConfig->mSaveNum = GetConfParaInt(&stConfParser, SAMPLE_DMX2VDEC_SAVEFRAME_KEY_SAVE_NUM, 0);
    char *pDstDirPath = (char*)GetConfParaString(&stConfParser, SAMPLE_DMX2VDEC_SAVEFRAME_KEY_DST_DIR, NULL);
    strcpy(pConfig->mDstDir, pDstDirPath);
    alogd("pConfig: srcFile[%s], seekTime[%d]ms, saveNum[%d], dstDir[%s]", pConfig->mSrcFile, pConfig->mSeekTime, pConfig->mSaveNum, pConfig->mDstDir);
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

static ERRORTYPE SampleDemux2VdecSaveFrame_MPPCallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleDemux2VdecSaveFrameContext *pContext = (SampleDemux2VdecSaveFrameContext *)cookie;

    if (pChn->mModId == MOD_ID_DEMUX)
    {
        switch (event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("demux to end of file");
                if (pContext->mVdecChn >= 0)
                {
                    alogd("set vdec streamEof");
                    AW_MPI_VDEC_SetStreamEof(pContext->mVdecChn, TRUE);
                }
                break;
            }
            default:
            {
                alogd("ignore demuxChn[%d] event[0x%x]", pChn->mChnId, event);
                break;
            }
        }
    }
    else if (pChn->mModId == MOD_ID_VDEC)
    {
        switch (event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("vdec notify streamEof");
                pContext->mbEndFlag = TRUE;
                break;
            }
            default:
            {
                alogd("ignore vdecChn[%d] event[0x%x]", pChn->mChnId, event);
                break;
            }
        }
    }
    else if(pChn->mModId == MOD_ID_CLOCK)
    {
        alogd("ignore clockChn[%d] event[0x%x]", pChn->mChnId, event);
    }
    else
    {
        aloge("fatal error! mod[0x%x]?", pChn->mModId);
    }
    return SUCCESS;
}

static void SampleDemux2VdecSaveFrame_HandleExit(int signo)
{
    alogd("user want to exit!");
    gSampleDemux2VdecSaveFrameContext.mbUserExitFlag = TRUE;
}

ERRORTYPE SampleDemux2VdecSaveFrame_makeFrameName(char *pFrameName, int nFrameNameSize, int nNum, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    char *pStrPixelFormat;
    switch(pFrameInfo->VFrame.mPixelFormat)
    {
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        {
            pStrPixelFormat = "nv21";
            break;
        }
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
        {
            pStrPixelFormat = "yv12";
            break;
        }
        default:
        {
            pStrPixelFormat = "unknown";
            break;
        }
    }
    snprintf(pFrameName, nFrameNameSize, "frame[%dx%d][%02d].%s", pFrameInfo->VFrame.mWidth, pFrameInfo->VFrame.mHeight, nNum, pStrPixelFormat);
    return SUCCESS;
}

static ERRORTYPE SampleDemux2VdecSaveFrame_SaveVdecOutFrame(VIDEO_FRAME_INFO_S *pFrameInfo, char *pDstFilePath)
{
    FILE *pFile = fopen(pDstFilePath, "wb");
    if(pFile != NULL)
    {
        VideoFrameBufferSizeInfo FrameSizeInfo;
        getVideoFrameBufferSizeInfo(pFrameInfo, &FrameSizeInfo);
        int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
        for(int i=0; i<3; i++)
        {
            if(pFrameInfo->VFrame.mpVirAddr[i] != NULL)
            {
                fwrite(pFrameInfo->VFrame.mpVirAddr[i], 1, yuvSize[i], pFile);
                alogv("virAddr[%d]=[%p], length=[%d]", i, pFrameInfo->VFrame.mpVirAddr[i], yuvSize[i]);
            }
        }
        fclose(pFile);
        alogd("store frame in file[%s]", pDstFilePath);
    }
    else
    {
        aloge("fatal error! open file[%s] fail.", pDstFilePath);
    }
    return SUCCESS;
}

ERRORTYPE SampleDemux2VdecSaveFrameContext_CreateFolder(const char* pStrFolderPath)
{
    if(NULL == pStrFolderPath || 0 == strlen(pStrFolderPath))
    {
        aloge("folder path is wrong!");
        return FAILURE;
    }
    //check folder existence
    struct stat sb;
    if (stat(pStrFolderPath, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pStrFolderPath, sb.st_mode);
            return FAILURE;
        }
    }
    //create folder if necessary
    int ret = mkdir(pStrFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pStrFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pStrFolderPath);
        return FAILURE;
    }
}

int main(int argc, char *argv[])
{
    ERRORTYPE result = SUCCESS;
    BOOL bSuccessFlag;
    SampleDemux2VdecSaveFrameContext *pContext = &gSampleDemux2VdecSaveFrameContext;
    if (SampleDemux2VdecSaveFrameContext_Init(pContext) != SUCCESS)
    {
        return FAILURE;
    }

    if (ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        result = FAILURE;
        goto err_out_0;
    }

    if (LoadSampleDemux2VdecSaveFrameConfig(&pContext->mConfigPara, pContext->mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        result = FAILURE;
        goto err_out_0;
    }
    
    //check folder existence, create folder if necessary
    if(SUCCESS != SampleDemux2VdecSaveFrameContext_CreateFolder(pContext->mConfigPara.mDstDir))
    {
        result = FAILURE;
        goto err_out_0;
    }
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, SampleDemux2VdecSaveFrame_HandleExit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }
    
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    ERRORTYPE ret;
    //create mpi_demux.
    int fd = open(pContext->mConfigPara.mSrcFile, O_RDONLY);
    if (fd < 0) 
    {
        aloge("Failed to open file %s(%s)", pContext->mConfigPara.mSrcFile, strerror(errno));
        result = FAILURE;
        goto err_out_1;
    }
    memset(&pContext->mDmxChnAttr, 0, sizeof(DEMUX_CHN_ATTR_S));
    pContext->mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    pContext->mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    pContext->mDmxChnAttr.mSourceUrl = NULL;
    pContext->mDmxChnAttr.mFd = fd;
    pContext->mDmxChnAttr.mDemuxDisableTrack = DEMUX_DISABLE_AUDIO_TRACK | DEMUX_DISABLE_SUBTITLE_TRACK;

    bSuccessFlag = FALSE;
    pContext->mDmxChn = 0;
    while (pContext->mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(pContext->mDmxChn, &pContext->mDmxChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create demux channel[%d] success!", pContext->mDmxChn);
            break;
        }
        else if (ERR_DEMUX_EXIST == ret)
        {
            alogd("demux channel[%d] is exist, try next!", pContext->mDmxChn);
            pContext->mDmxChn++;
        }
        else
        {
            alogd("create demux channel[%d] ret[0x%x]!", pContext->mDmxChn, ret);
            break;
        }
    }
    if (bSuccessFlag)
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleDemux2VdecSaveFrame_MPPCallback;
        AW_MPI_DEMUX_RegisterCallback(pContext->mDmxChn, &cbInfo);
    }
    else
    {
        pContext->mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        result = FAILURE;
        goto err_out_2;
    }
    if (AW_MPI_DEMUX_GetMediaInfo(pContext->mDmxChn, &pContext->mDemuxMediaInfo) != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
        result = FAILURE;
        goto err_out_3;
    }
    if (pContext->mDemuxMediaInfo.mVideoNum <= 0)
    {
        aloge("fatal error! videoNum[%d]<=0", pContext->mDemuxMediaInfo.mVideoNum);
        result = FAILURE;
        goto err_out_3;
    }

    //create mpi_vdec.
    memset(&pContext->mVdecChnAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    pContext->mVdecChnAttr.mType = pContext->mDemuxMediaInfo.mVideoStreamInfo[0].mCodecType;
    pContext->mVdecChnAttr.mOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    bSuccessFlag = FALSE;
    pContext->mVdecChn = 0;
    while (pContext->mVdecChn < VDEC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VDEC_CreateChn(pContext->mVdecChn, &pContext->mVdecChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create vdec channel[%d] success!", pContext->mVdecChn);
            break;
        }
        else if (ERR_VDEC_EXIST == ret)
        {
            alogd("vdec channel[%d] is exist, find next!", pContext->mVdecChn);
            pContext->mVdecChn++;
        }
        else
        {
            alogd("create vdec channel[%d] ret[0x%x]!", pContext->mVdecChn, ret);
            break;
        }
    }
    if (bSuccessFlag)
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleDemux2VdecSaveFrame_MPPCallback;
        AW_MPI_VDEC_RegisterCallback(pContext->mVdecChn, &cbInfo);
    }
    else
    {
        pContext->mVdecChn = MM_INVALID_CHN;
        aloge("fatal error! create vdec channel fail!");
        result = FAILURE;
        goto err_out_3;
    }

    //create mpi_clock.
    memset(&pContext->mClockChnAttr, 0, sizeof(CLOCK_CHN_ATTR_S));
    pContext->mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_VIDEO;
    bSuccessFlag = FALSE;
    pContext->mClockChn = 0;
    while (pContext->mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(pContext->mClockChn, &pContext->mClockChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", pContext->mClockChn);
            break;
        }
        else if (ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", pContext->mClockChn);
            pContext->mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", pContext->mClockChn, ret);
            break;
        }
    }

    if (bSuccessFlag)
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleDemux2VdecSaveFrame_MPPCallback;
        AW_MPI_CLOCK_RegisterCallback(pContext->mClockChn, &cbInfo);
    }
    else
    {
        pContext->mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
        result = FAILURE;
        goto err_out_3;
    }

    //bind demux, vdec, clock.
    MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pContext->mDmxChn};
    MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pContext->mVdecChn};
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pContext->mClockChn};
    AW_MPI_SYS_Bind(&DmxChn, &VdecChn);
    AW_MPI_SYS_Bind(&ClockChn, &DmxChn);

    //start to decode.
    if (pContext->mConfigPara.mSeekTime > 0)
    {
        ret = AW_MPI_DEMUX_Seek(pContext->mDmxChn, pContext->mConfigPara.mSeekTime);
        if(ret != SUCCESS)
        {
            aloge("fatal error! dmx seek to [%d]ms fail[0x%x]!", pContext->mConfigPara.mSeekTime, ret);
        }
    }
    ret = AW_MPI_CLOCK_Start(pContext->mClockChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! clock start fail[0x%x]!", ret);
    }
    ret = AW_MPI_VDEC_StartRecvStream(pContext->mVdecChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vdec start fail[0x%x]!", ret);
    }
    ret = AW_MPI_DEMUX_Start(pContext->mDmxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! demux start fail[0x%x]!", ret);
    }

    //get decoded frame repeatedly util file end or user exit.
    int nSaveNum = 0;
    VIDEO_FRAME_INFO_S stVideoFrameInfo;
    while (1)
    {
        ret = AW_MPI_VDEC_GetImage(pContext->mVdecChn, &stVideoFrameInfo, 200);
        if(SUCCESS == ret)
        {
            if(nSaveNum < pContext->mConfigPara.mSaveNum)
            {
                char strDstFileName[128];
                char strDstFilePath[256];
                SampleDemux2VdecSaveFrame_makeFrameName(strDstFileName, sizeof(strDstFileName), nSaveNum, &stVideoFrameInfo);
                snprintf(strDstFilePath, sizeof(strDstFilePath), "%s/%s", pContext->mConfigPara.mDstDir, strDstFileName);
                SampleDemux2VdecSaveFrame_SaveVdecOutFrame(&stVideoFrameInfo, strDstFilePath);
                nSaveNum++;
            }
            ret = AW_MPI_VDEC_ReleaseImage(pContext->mVdecChn, &stVideoFrameInfo);
            if (ret != SUCCESS)
            {
                aloge("fatal error! image get, but release fail[0x%x]", ret);
            }
        }
        else if(ERR_VDEC_NOBUF == ret)
        {
            if(pContext->mbEndFlag)
            {
                alogd("all frames are got, exit!");
                break;
            }
        }
        else
        {
            aloge("fatal error! ret[0x%x], exit!", ret);
            break;
        }
        if(pContext->mbUserExitFlag)
        {
            alogd("respond to user exit request!");
            break;
        }
    }

    //stop mpp components
    ret = AW_MPI_DEMUX_Stop(pContext->mDmxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! demux stop fail[0x%x]!", ret);
    }
    ret = AW_MPI_VDEC_StopRecvStream(pContext->mVdecChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vdec stop fail[0x%x]!", ret);
    }
    ret = AW_MPI_CLOCK_Stop(pContext->mClockChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! clock stop fail[0x%x]!", ret);
    }
err_out_3:
    if (pContext->mDmxChn >= 0)
    {
        ret = AW_MPI_DEMUX_DestroyChn(pContext->mDmxChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! demux destruct fail[0x%x]!", ret);
        }
        pContext->mDmxChn = MM_INVALID_CHN;
    }
    if (pContext->mVdecChn >= 0)
    {
        ret = AW_MPI_VDEC_DestroyChn(pContext->mVdecChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vdec destruct fail[0x%x]!", ret);
        }
        pContext->mVdecChn = MM_INVALID_CHN;
    }
    if (pContext->mClockChn >= 0)
    {
        ret = AW_MPI_CLOCK_DestroyChn(pContext->mClockChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! clock destruct fail[0x%x]!", ret);
        }
        pContext->mClockChn = MM_INVALID_CHN;
    }
err_out_2:
    close(pContext->mDmxChnAttr.mFd);pContext->mDmxChnAttr.mFd = -1;
err_out_1:
    //exit mpp system
    AW_MPI_SYS_Exit();
err_out_0:
    SampleDemux2VdecSaveFrameContext_Destroy(&gSampleDemux2VdecSaveFrameContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

