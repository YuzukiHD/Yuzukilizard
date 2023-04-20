/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/11/4
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "sample_ai2aenc"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <confparser.h>
#include "sample_ai2aenc.h"
#include <cdx_list.h>


#define SAMPLE_AI2AENC_DST_FILE   "dst_file"
#define SAMPLE_AI2AENC_ENCODER_TYPE   "encoder_type"

#define SAMPLE_AI2AENC_SAMPLE_RATE   "sample_rate"
#define SAMPLE_AI2AENC_CHANNEL_CNT   "channel_cnt"
#define SAMPLE_AI2AENC_BIT_WIDTH     "bit_width"
#define SAMPLE_AI2AENC_FRAME_SIZE    "frame_size"
#define SAMPLE_AI2AENC_DEV_VOLUME    "dev_volume"
#define SAMPLE_AI2AENC_TEST_DURATION "test_duration"



static int ParseCmdLine(int argc, char **argv, SampleAi2AencContext *pStContext)
{
    alogd("sample aenc path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;

    memset(&pStContext->mCmdLinePara, 0, sizeof(SampleAi2AencCmdLineParam));
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
            strncpy(pStContext->mCmdLinePara.mConfigFilePath, argv[i], MAX_FILE_PATH_SIZE-1);
            pStContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
            alogd("CmdLine param: -path /home/sample_ai2aenc.conf");
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

static ERRORTYPE loadSampleAi2AencConfig(SampleAi2AencContext *pStContext, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;

    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

    memset(&pStContext->mConfigPara, 0, sizeof(SampleAi2AencConfig));

    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AI2AENC_DST_FILE, NULL);
    strncpy(pStContext->mConfigPara.mDstFilePath, ptr, MAX_FILE_PATH_SIZE);

    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AI2AENC_ENCODER_TYPE, NULL);
    if (!strcmp(ptr, "aac"))
    {
        pStContext->mConfigPara.mCodecType = PT_AAC;
    }
    else if (!strcmp(ptr, "mp3"))
    {
        pStContext->mConfigPara.mCodecType = PT_MP3;
    }
    else if (!strcmp(ptr, "pcm"))
    {
        pStContext->mConfigPara.mCodecType = PT_PCM_AUDIO;
    }
    else if (!strcmp(ptr, "adpcm"))
    {
        pStContext->mConfigPara.mCodecType = PT_ADPCMA;
    }
    else if (!strcmp(ptr, "g726a"))
    {
        pStContext->mConfigPara.mCodecType = PT_G726;
    }
    else if (!strcmp(ptr, "g711a"))
    {
        pStContext->mConfigPara.mCodecType = PT_G711A;
    }
    else if (!strcmp(ptr, "g711u"))
    {
        pStContext->mConfigPara.mCodecType = PT_G711U;
    }
    else if (!strcmp(ptr, "g726u"))
    {
        pStContext->mConfigPara.mCodecType = PT_G726U;
    }
    else
    {
        alogw("Unknown codec type[%s]! Set to default codec type[aac]!", ptr);
        ptr = "aac";
        pStContext->mConfigPara.mCodecType = PT_AAC;
    }
    strcat(pStContext->mConfigPara.mDstFilePath, ptr);

    pStContext->mConfigPara.mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AI2AENC_SAMPLE_RATE, 0);
    pStContext->mConfigPara.mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AI2AENC_BIT_WIDTH, 0);
    pStContext->mConfigPara.mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AI2AENC_CHANNEL_CNT, 0);
    pStContext->mConfigPara.mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AI2AENC_FRAME_SIZE, 0);
    pStContext->mConfigPara.mDevVolume = GetConfParaInt(&stConfParser, SAMPLE_AI2AENC_DEV_VOLUME, 0);
    pStContext->mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_AI2AENC_TEST_DURATION, 0);

    alogd("parse info:samplerate=%d,bitwidth=%d,chn=%d,framesize=%d, devVolume=%d", pStContext->mConfigPara.mSampleRate, pStContext->mConfigPara.mBitWidth, \
                    pStContext->mConfigPara.mChannelCnt, pStContext->mConfigPara.mFrameSize, pStContext->mConfigPara.mDevVolume);
    alogd("mDstFilePath [%s] with codec [%s]", pStContext->mConfigPara.mDstFilePath, ptr);

    destroyConfParser(&stConfParser);

    return SUCCESS;
}

static void configAiAttr(SampleAi2AencContext *pStContext)
{
    memset(&pStContext->mAiAttr, 0, sizeof(AIO_ATTR_S));
    pStContext->mAiAttr.u32ChnCnt = pStContext->mConfigPara.mChannelCnt;
    pStContext->mAiAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)pStContext->mConfigPara.mSampleRate;
    pStContext->mAiAttr.enBitwidth = (AUDIO_BIT_WIDTH_E)(pStContext->mConfigPara.mBitWidth/8-1);
}

static void configAencAttr(SampleAi2AencContext *pStContext)
{
    memset(&pStContext->mAencAttr, 0, sizeof(AENC_ATTR_S));

    pStContext->mAencAttr.sampleRate = pStContext->mConfigPara.mSampleRate;
    pStContext->mAencAttr.channels = pStContext->mConfigPara.mChannelCnt;
    pStContext->mAencAttr.bitRate = 0;             // usful when codec G726
    pStContext->mAencAttr.bitsPerSample = pStContext->mConfigPara.mBitWidth;
    pStContext->mAencAttr.attachAACHeader = 1;     // ADTS
    pStContext->mAencAttr.Type = pStContext->mConfigPara.mCodecType;
	//pStContext->mAencAttr.enc_law = 1;		// defualt to a-law
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleAi2AencContext *pStContext = (SampleAi2AencContext *)cookie;

    if (pChn->mModId == MOD_ID_AI)
    {
        switch (event)
        {
        default:
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_AENC)
    {
        switch (event)
        {
        default:
            break;
        }
    }

    return SUCCESS;
}

static ERRORTYPE createAencChn(SampleAi2AencContext *pStContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configAencAttr(pStContext);
    pStContext->mAencChn = 0;
    while (pStContext->mAencChn < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(pStContext->mAencChn, (AENC_CHN_ATTR_S*)&pStContext->mAencAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", pStContext->mAencChn);
            break;
        }
        else if(ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", pStContext->mAencChn);
            pStContext->mAencChn++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[0x%x], find next!", pStContext->mAencChn, ret);
            pStContext->mAencChn++;
        }
    }

    if(FALSE == nSuccessFlag)
    {
        pStContext->mAencChn = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pStContext;
        cbInfo.callback = NULL;
        AW_MPI_AENC_RegisterCallback(pStContext->mAencChn, &cbInfo);

        if (pStContext->mAiChn >= 0)
        {
            alogd("bind ai & aenc");
            MPP_CHN_S AiChn = {MOD_ID_AI, 0, pStContext->mAiChn};
            MPP_CHN_S AencChn = {MOD_ID_AENC, 0, pStContext->mAencChn};
            AW_MPI_SYS_Bind(&AiChn, &AencChn);
        }
        return SUCCESS;
    }
}

static ERRORTYPE createAiChn(SampleAi2AencContext *pStContext)
{
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;

    pStContext->mAiDev = 0;
    configAiAttr(pStContext);
    AW_MPI_AI_SetPubAttr(pStContext->mAiDev, &pStContext->mAiAttr);
    AW_MPI_AI_Enable(pStContext->mAiDev);
    AW_MPI_AI_SetDevVolume(pStContext->mAiDev, pStContext->mConfigPara.mDevVolume);

    pStContext->mAiChn = 0;
    while (pStContext->mAiChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(pStContext->mAiDev, pStContext->mAiChn);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", pStContext->mAiChn);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", pStContext->mAiChn);
            pStContext->mAiChn++;
        }
        else
        {
            alogd("create ai channel[%d] ret[0x%x], find next!", pStContext->mAiChn, ret);
            pStContext->mAiChn++;
        }
    }

    if (FALSE == bSuccessFlag)
    {
        pStContext->mAiChn = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pStContext;
        cbInfo.callback = NULL;
        AW_MPI_AI_RegisterCallback(pStContext->mAiDev, pStContext->mAiChn, &cbInfo);
        return SUCCESS;
    }
}

static void *getFrameThread(void *pThreadData)
{
    AUDIO_STREAM_S stream;
    SampleAi2AencContext *pStContext = (SampleAi2AencContext *)pThreadData;

    fseek(pStContext->mFpDstFile, 0, SEEK_SET);

    while (!pStContext->mOverFlag)
    {
        if (SUCCESS == AW_MPI_AENC_GetStream(pStContext->mAencChn, &stream, 100/*0*/))
        {
            //alogd("get one stream with size: [%d]", stream.mLen);
            fwrite(stream.pStream, 1, stream.mLen, pStContext->mFpDstFile);
            AW_MPI_AENC_ReleaseStream(pStContext->mAencChn, &stream);
        }
    }

    return NULL;
}


int main(int argc, char **argv)
{
    int ret = 0;
    SampleAi2AencContext stContext;

    memset(&stContext, 0, sizeof(SampleAi2AencContext));

    if (ParseCmdLine(argc, argv, &stContext) != 0)
    {
        return -1;
    }

    if (loadSampleAi2AencConfig(&stContext, stContext.mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        return -1;
    }

    cdx_sem_init(&stContext.mSemExit, 0);

    stContext.mFpDstFile = fopen(stContext.mConfigPara.mDstFilePath, "wb");
    if (NULL == stContext.mFpDstFile)
    {
        aloge("can not open dst file [%s]", stContext.mConfigPara.mDstFilePath);
        ret = -1;
        goto _exit_0;
    }

    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    stContext.mAiDev = 0;
    stContext.mAiChn = MM_INVALID_CHN;
    stContext.mAencChn = MM_INVALID_CHN;

    ret = createAiChn(&stContext);
    if (ret < 0)
    {
        ret = -1;
        goto _exit_1;
    }
    ret = createAencChn(&stContext);
    if (ret < 0)
    {
        ret = -1;
        goto _exit_2;
    }

    pthread_create(&stContext.mThdId, NULL, getFrameThread, &stContext);

    //start transe
    AW_MPI_AI_EnableChn(stContext.mAiDev, stContext.mAiChn);
    AW_MPI_AENC_StartRecvPcm(stContext.mAencChn);

    if (stContext.mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }
    stContext.mOverFlag = TRUE;

    usleep(100*1000);

    //stop
    AW_MPI_AI_DisableChn(stContext.mAiDev, stContext.mAiChn);
    AW_MPI_AENC_StopRecvPcm(stContext.mAencChn);

    pthread_join(stContext.mThdId, NULL);

_exit_2:
    if (stContext.mAiChn >= 0)
    {
        AW_MPI_AI_ResetChn(stContext.mAiDev, stContext.mAiChn);
        AW_MPI_AI_DestroyChn(stContext.mAiDev, stContext.mAiChn);
        AW_MPI_AI_Disable(stContext.mAiDev);
    }
    if (stContext.mAencChn >= 0)
    {
        AW_MPI_AENC_ResetChn(stContext.mAencChn);
        AW_MPI_AENC_DestroyChn(stContext.mAencChn);
    }
_exit_1:
    AW_MPI_SYS_Exit();
    fclose(stContext.mFpDstFile);
_exit_0:
    cdx_sem_deinit(&stContext.mSemExit);
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;
}
