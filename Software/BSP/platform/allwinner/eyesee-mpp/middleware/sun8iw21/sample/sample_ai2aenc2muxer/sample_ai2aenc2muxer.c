/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_ai2aenc2muxer.c
  Version       : V1.0
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/09/08
  Last Modified :
  Description   : test code for ai & aenc & muxer
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleAI2AEnc2Muxer"
#include "plat_log.h"

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>

#include "mm_comm_sys.h"
#include "mm_comm_aio.h"
#include "mm_comm_aenc.h"
#include <media_common_aio.h>
#include <mpi_sys.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>

#include "sample_ai2aenc2muxer.h"

// Default Params definition
#define DEFAULT_CONF_FILE_PATH      "/mnt/extsd/sample_ai2aenc2muxer/sample_ai2aenc2muxer.conf"
#define DEFAULT_DST_FILE_PATH       "/mnt/extsd/test.aac"
#define DEFAULT_CAPTURE_DURATION    (10)   //Unit:Second
#define DEFAULT_CHANNEL_COUNT       (1)
#define DEFAULT_BIT_WIDTH           (16)
#define DEFAULT_SAMPLE_RATE         (8000)
#define DEFAULT_BITRATE             (16000)

static int parseCmdLine(SAMPLE_AI2AENC2MUXER_S *pSampleData, int argc, char** argv)
{
    int ret = 0;

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = 0;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pSampleData->confFilePath, *argv, MAX_FILE_PATH_LEN);
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            alogd("CmdLine param: -path /mnt/extsd/sample_ai2aenc2muxer/sample_ai2aenc2muxer.conf");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_AI2AENC2MUXER_S *pSampleData)
{
    int ret;
    char *ptr;

    char *pConfFilePath;
	// Ensure config file valid
    if (!strlen(pSampleData->confFilePath))
    {
        //alogw("use dafault confFile [%s]", DEFAULT_CONF_FILE_PATH);
        //strncpy(pSampleData->confFilePath, DEFAULT_CONF_FILE_PATH, MAX_FILE_PATH_LEN);
        pConfFilePath = NULL;
    }
    else
    {
        pConfFilePath = pSampleData->confFilePath;
    }

    strncpy(pSampleData->mConfDstFile, DEFAULT_DST_FILE_PATH, MAX_FILE_PATH_LEN);
    pSampleData->mConfCapDuration = DEFAULT_CAPTURE_DURATION;
    pSampleData->mConfChnCnt      = DEFAULT_CHANNEL_COUNT;
    pSampleData->mConfBitWidth    = DEFAULT_BIT_WIDTH;
    pSampleData->mConfSampleRate  = DEFAULT_SAMPLE_RATE;
    pSampleData->mConfBitRate   = DEFAULT_BITRATE;
    ptr = strrchr(pSampleData->mConfDstFile, '.') + 1;
    strcpy(pSampleData->mConfCodecType, ptr);	// separate encode type
    pSampleData->mConfAISaveFileFlag = false;
        
    if(pConfFilePath)
    {
		// Load config file
        CONFPARSER_S cfg;
        ret = createConfParser(pSampleData->confFilePath, &cfg);
        if (ret < 0)
        {
            aloge("load conf fail!");
            return FAILURE;
        }
		
		// Read params
        ptr = (char*)GetConfParaString(&cfg, DST_FILE_PATH, NULL);   //read dest file
        strncpy(pSampleData->mConfDstFile, ptr, MAX_FILE_PATH_LEN);
		
        ptr = strrchr(pSampleData->mConfDstFile, '.') + 1;
        strcpy(pSampleData->mConfCodecType, ptr);	// separate encode type
       
        pSampleData->mConfCapDuration = GetConfParaInt(&cfg, CAPTURE_DURATION, 0); // capture duration
		
        pSampleData->mConfChnCnt     = GetConfParaInt(&cfg, CHANNEL_COUNT, 0);	// audio params
        pSampleData->mConfBitWidth   = GetConfParaInt(&cfg, BIT_WIDTH, 0);
        pSampleData->mConfSampleRate = GetConfParaInt(&cfg, SAMPLE_RATE, 0);
        pSampleData->mConfBitRate = GetConfParaInt(&cfg, KEY_BITRATE, 0);
		
		// Release config file
        destroyConfParser(&cfg);
    }
	// show config
    alogd("config para: dst_file [%s], codec_type [%s]", pSampleData->mConfDstFile, pSampleData->mConfCodecType);
    alogd("             cap_duration [%d], chn_cnt [%d], bit_width [%d], sample_rate [%d], bitRate [%d]",
        pSampleData->mConfCapDuration, pSampleData->mConfChnCnt, pSampleData->mConfBitWidth, pSampleData->mConfSampleRate, pSampleData->mConfBitRate);

    return SUCCESS;
}

void configAioAttr(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    AIO_ATTR_S *pAttr = &ctx->mAioAttr;

    pAttr->u32ChnCnt    = ctx->mConfChnCnt;
    pAttr->enBitwidth   = map_BitWidth_to_AUDIO_BIT_WIDTH_E(ctx->mConfBitWidth);
    pAttr->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(ctx->mConfSampleRate);
}

void configAEncAttr(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    AENC_CHN_ATTR_S *pAttr = &ctx->mAEncAttr;

    if (!strcmp(ctx->mConfCodecType, "aac"))
    {
        pAttr->AeAttr.Type = PT_AAC;
    }
    else if (!strcmp(ctx->mConfCodecType, "mp3"))
    {
        pAttr->AeAttr.Type = PT_MP3;
    }
    else if (!strcmp(ctx->mConfCodecType, "pcm"))
    {
        pAttr->AeAttr.Type = PT_PCM_AUDIO;
    }
    else if (!strcmp(ctx->mConfCodecType, "adpcm"))
    {
        pAttr->AeAttr.Type = PT_ADPCMA;
    }
    else if (!strcmp(ctx->mConfCodecType, "g711a"))
    {
        pAttr->AeAttr.Type = PT_G711A;
    }
    else if (!strcmp(ctx->mConfCodecType, "g711u"))
    {
        pAttr->AeAttr.Type = PT_G711U;
    }
    else if (!strcmp(ctx->mConfCodecType, "g726"))
    {
        pAttr->AeAttr.Type = PT_G726;
    }
    else
    {
        alogw("Unknown audio codec[%s]! Set to default [aac]", ctx->mConfCodecType);
        pAttr->AeAttr.Type = PT_AAC;
    }
    pAttr->AeAttr.channels = ctx->mConfChnCnt;
    pAttr->AeAttr.bitsPerSample = ctx->mConfBitWidth;
    pAttr->AeAttr.sampleRate = ctx->mConfSampleRate;
    pAttr->AeAttr.bitRate = ctx->mConfBitRate;
    pAttr->AeAttr.attachAACHeader = 1;
    pAttr->AeAttr.mInBufSize = 0;
    pAttr->AeAttr.mOutBufCnt = 0;
}

static ERRORTYPE createAIChn(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    //enable audio_hw_ai
    AW_MPI_AI_SetPubAttr(ctx->mAIDevId, &ctx->mAioAttr);
    AW_MPI_AI_Enable(ctx->mAIDevId);

    BOOL nSuccessFlag = FALSE;
    ERRORTYPE ret = 0;
    while (ctx->mAIChnId < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(ctx->mAIDevId, ctx->mAIChnId);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", ctx->mAIChnId);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", ctx->mAIChnId);
            ctx->mAIChnId++;
        }
        else if (ERR_AI_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! ret[0x%x]!", ctx->mAIChnId, ret);
            break;
        }
    }
    if(FALSE == nSuccessFlag)
    {
        ctx->mAIChnId = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        ret = -1;
    }
    else
    {
        ctx->mAiChn.mModId = MOD_ID_AI;
        ctx->mAiChn.mDevId = ctx->mAIDevId;
        ctx->mAiChn.mChnId = ctx->mAIChnId;
    }

    return ret;
}

static ERRORTYPE createAEncChn(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    BOOL nSuccessFlag = FALSE;
    ERRORTYPE ret = 0;
    while (ctx->mAEncChnId < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(ctx->mAEncChnId, &ctx->mAEncAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", ctx->mAEncChnId);
            break;
        }
        else if (ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", ctx->mAEncChnId);
            ctx->mAEncChnId++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[0x%x], find next!", ctx->mAEncChnId, ret);
            ctx->mAEncChnId++;
        }
    }
    if (FALSE == nSuccessFlag)
    {
        ctx->mAEncChnId = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        ret = -1;
    }
    else
    {
        ctx->mAEncChn.mModId = MOD_ID_AENC;
        ctx->mAEncChn.mDevId = 0;
        ctx->mAEncChn.mChnId = ctx->mAEncChnId;
    }

    return ret;
}


int main(int argc, char** argv)
{
    ERRORTYPE ret = 0;
    SAMPLE_AI2AENC2MUXER_S nSampleContext;
    memset(&nSampleContext, 0, sizeof(SAMPLE_AI2AENC2MUXER_S));

    if (parseCmdLine(&nSampleContext, argc, argv) != 0)
    {
        aloge("parseCmdLine fail!");
    }

    if (loadConfigPara(&nSampleContext) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        goto _END;
    }

    nSampleContext.mFpDst = fopen(nSampleContext.mConfDstFile, "wb");
	if (nSampleContext.mFpDst == NULL)
	{
		aloge("cann't open dest file %s", nSampleContext.mConfDstFile);
		goto _END;
	}

    // init mpp system
    nSampleContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&nSampleContext.mSysConf);
    AW_MPI_SYS_Init();

    // config ai & aenc attr
    configAioAttr(&nSampleContext);
    configAEncAttr(&nSampleContext);

    // config ai & aenc chn id
    nSampleContext.mAIDevId = 0;
    nSampleContext.mAIChnId = 0;
    nSampleContext.mAEncChnId = 0;

    // create ai & aenc chn
    if (createAIChn(&nSampleContext) != SUCCESS)
    {
        aloge("create ai chn fail!");
        goto _END;
    }
    if (createAEncChn(&nSampleContext) != SUCCESS)
    {
        aloge("create aenc chn fail!");
        goto _END;
    }

    // test ai save file api
    if(nSampleContext.mConfAISaveFileFlag)
    {
        strcpy(nSampleContext.mSaveFileInfo.mFilePath, "/mnt/extsd/");
        strcpy(nSampleContext.mSaveFileInfo.mFileName, "SampleAi2Aenc2Muxer_AiSaveFile.pcm");
        AW_MPI_AI_SaveFile(nSampleContext.mAIDevId, nSampleContext.mAIChnId, &nSampleContext.mSaveFileInfo);
    }

    // bind ai & aenc
    AW_MPI_SYS_Bind(&nSampleContext.mAiChn, &nSampleContext.mAEncChn);

	// set start time
    alogd("will capture for %d seconds, wait ...", nSampleContext.mConfCapDuration);
    struct timeval tv;
    long long val_begin, val_end;
    gettimeofday(&tv, NULL);
    val_begin = 1000000 * tv.tv_sec + tv.tv_usec;

    //start ai & aenc
    AW_MPI_AI_EnableChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    AW_MPI_AENC_StartRecvPcm(nSampleContext.mAEncChnId);

    //capturing
    int iCapPktCnt = 0;
    AUDIO_STREAM_S stream;
    
    while (1)
    {
        ret = AW_MPI_AENC_GetStream(nSampleContext.mAEncChnId, &stream, 100);
        if (!ret)
        {// Get encodes frame sucess
            fwrite(stream.pStream, 1, stream.mLen, nSampleContext.mFpDst);
            AW_MPI_AENC_ReleaseStream(nSampleContext.mAEncChnId, &stream);
            if (!(iCapPktCnt++ % 30))
            {
                if(nSampleContext.mConfAISaveFileFlag)
                {
                    alogd("query file status!");
                    AUDIO_SAVE_FILE_INFO_S info;
    				memset(&info, 0, sizeof(AUDIO_SAVE_FILE_INFO_S));
                    AW_MPI_AI_QueryFileStatus(nSampleContext.mAIDevId, nSampleContext.mAIChnId, &info);
                    alogd("AI comp save_file(%s%s) exist_flag:%d, current_file_size:%d",
                        info.mFilePath, info.mFileName, info.bCfg, info.mFileSize);
                }
            }
			
			// check stop time
            gettimeofday(&tv, NULL);
            val_end = 1000000 * tv.tv_sec + tv.tv_usec;
            if (val_end - val_begin > nSampleContext.mConfCapDuration * 1000000)
            {
                alogd("capture for %d seconds finish!", nSampleContext.mConfCapDuration);
                break;
            }
        }
    }

    // stop ai & aenc
    AW_MPI_AI_DisableChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    AW_MPI_AENC_StopRecvPcm(nSampleContext.mAEncChnId);

    // destruct ai & aenc
    //AW_MPI_AENC_ResetChn(nSampleContext.mAEncChnId);
    AW_MPI_AENC_DestroyChn(nSampleContext.mAEncChnId);
    //AW_MPI_AI_ResetChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    AW_MPI_AI_DestroyChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    

    // exit mpp system
    AW_MPI_SYS_Exit();

_END:
    fclose(nSampleContext.mFpDst);
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;
}
