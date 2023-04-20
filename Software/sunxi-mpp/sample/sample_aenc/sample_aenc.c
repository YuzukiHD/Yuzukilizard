//#define LOG_NDEBUG 0
#define LOG_TAG "sample_aenc"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_aenc.h>

#include <confparser.h>
#include "sample_aenc.h"
#include <cdx_list.h>


static int ParseCmdLine(int argc, char **argv, SampleAEncCmdLineParam *pCmdLinePara)
{
    alogd("sample aenc path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleAEncCmdLineParam));
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
            alogd("CmdLine param: -path /home/sample_aenc.conf");
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

static ERRORTYPE loadSampleAEncConfig(SampleAEncConfig *pConfig, const char *conf_path)
{
    int ret;
	memset(pConfig, 0, sizeof(SampleAEncConfig));
   
	// Create config parser
    CONFPARSER_S stConfParser;
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    
	// Parse config file
	char *ptr = NULL;
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AENC_SRC_FILE_PATH, NULL);
    strncpy(pConfig->mSrcFilePath, ptr, MAX_FILE_PATH_SIZE);
	if (!strlen(pConfig->mSrcFilePath))
    {
        strncpy(pConfig->mSrcFilePath, DEFAULT_SRC_FILE_PATH, MAX_FILE_PATH_SIZE);
    }
	
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AENC_DST_FILE_PATH, NULL);
    strncpy(pConfig->mDstFilePath, ptr, MAX_FILE_PATH_SIZE);
    if (!strlen(pConfig->mDstFilePath))
    {
        strncpy(pConfig->mDstFilePath, DEFAULT_DST_FILE_PATH, MAX_FILE_PATH_SIZE);
    }
	
    ptr = strrchr(pConfig->mDstFilePath, '.') + 1;
    if (!strcmp(ptr, "aac"))
    {
        pConfig->mCodecType = PT_AAC;
		pConfig->bAddWaveHeader = 0;
    }
    else if (!strcmp(ptr, "mp3"))
    {
        pConfig->mCodecType = PT_MP3;
		pConfig->bAddWaveHeader = 0;
    }
    else if (!strcmp(ptr, "pcm"))
    {
        pConfig->mCodecType = PT_PCM_AUDIO;
		pConfig->bAddWaveHeader = 1;
    }
    else if (!strcmp(ptr, "adpcm"))
    {
        pConfig->mCodecType = PT_ADPCMA;
		pConfig->bAddWaveHeader = 1;
    }
    else if (!strcmp(ptr, "g726"))
    {
        pConfig->mCodecType = PT_G726;
		pConfig->bAddWaveHeader = 1;
    }
    else if (!strcmp(ptr, "g711a"))
    {
        pConfig->mCodecType = PT_G711A;
		pConfig->bAddWaveHeader = 1;
    }
    else if (!strcmp(ptr, "g711u"))
    {
        pConfig->mCodecType = PT_G711U;
		pConfig->bAddWaveHeader = 1;
    }
    else
    {
        alogw("Unknown codec type[%s]! Set to default codec type[aac]!", ptr);
        ptr = "aac";
        pConfig->mCodecType = PT_AAC;
		pConfig->bAddWaveHeader = 0;
    }
	
	if (pConfig->bAddWaveHeader == 1)
	{
		strcat(pConfig->mDstFilePath, ".wav");
	}
	
	// Destroy config parser
    alogd("parse info: mSrcFilePath[%s]", pConfig->mSrcFilePath);
    alogd("            mDstFilePath [%s] with codec [%s]", pConfig->mDstFilePath, ptr);
    destroyConfParser(&stConfParser);

    return SUCCESS;
}

void configAEncAttrByParseWaveHeader(AENC_ATTR_S *pAttr, WaveHeader *pWaveHeader, PAYLOAD_TYPE_E type)
{
    pAttr->sampleRate      = pWaveHeader->sample_rate;
    pAttr->channels        = pWaveHeader->num_chn;
    pAttr->bitRate         = 0;     // usful when codec G726
    pAttr->bitsPerSample   = pWaveHeader->bits_per_sample;
    pAttr->attachAACHeader = 1;     // ADTS
    pAttr->Type            = type;  // Codec type
	
    alogd("aenc attr by use pcm attr: channels[%d], bitWidth[%d], sampleRate[%d], codec[%d]",
        pAttr->channels, pAttr->bitsPerSample, pAttr->sampleRate, pAttr->Type);
}


int GenerateWaveHeader(AENC_ATTR_S* pAencAttr, int iPcmSize, WaveHeader* pWaveHeader)
{
	memset(pWaveHeader, 0, sizeof(WaveHeader));

	// Trans audio format, decide BlockAlign and BitsPerSample
	short sAudioFormat   = WAVE_FORMAT_UNKNOWN;
	int   iByteRate      = pAencAttr->sampleRate * pAencAttr->bitsPerSample / 8;
	short sBlockAlign    = pAencAttr->bitsPerSample / 8;
	short sBitsPerSample = pAencAttr->bitsPerSample;
	
	if (pAencAttr->Type == PT_PCM_AUDIO)
	{
		sAudioFormat   = WAVE_FORMAT_PCM;
		iByteRate      = iByteRate * 1;
		sBlockAlign    = pAencAttr->bitsPerSample / 8;
		sBitsPerSample = pAencAttr->bitsPerSample;
	}
	else if (pAencAttr->Type == PT_ADPCMA)
	{
		sAudioFormat   = WAVE_FORMAT_ADPCM;
		iByteRate      = iByteRate / 4;
		sBlockAlign    = 0x400;
		sBitsPerSample = 0x4;
	}
	else if (pAencAttr->Type == PT_G711A)
	{
		sAudioFormat   = WAVE_FORMAT_G711_ALAW;
		iByteRate      = iByteRate / 2;
		sBlockAlign    = 0x1;
		sBitsPerSample = 0x8;
	}
	else if (pAencAttr->Type == PT_G711U)
	{
		sAudioFormat   = WAVE_FORMAT_G711_MULAW;
		iByteRate      = iByteRate / 2;
		sBlockAlign    = 0x1;
		sBitsPerSample = 0x8;		
	}
	else if (pAencAttr->Type == PT_G726)
	{
		sAudioFormat = WAVE_FORMAT_G726_ADPCM;  //2 bits
		iByteRate      = iByteRate / 8;
		sBlockAlign    = 0x2;
		sBitsPerSample = 0x2;	
	}
	else
	{
		aloge("Invalid Audio Format %d", pAencAttr->Type);
		return -1;
	}
	
	// Fill Wave Header
    memcpy(&pWaveHeader->riff_id, "RIFF", 4);
    pWaveHeader->riff_sz = iPcmSize + sizeof(WaveHeader) - 8;
    memcpy(&pWaveHeader->riff_fmt, "WAVE", 4);
    memcpy(&pWaveHeader->fmt_id, "fmt ", 4);
    pWaveHeader->fmt_sz          = 16;  //16 or 18, 18 means the wav header have extended region
    pWaveHeader->audio_fmt       = sAudioFormat;
    pWaveHeader->num_chn         = pAencAttr->channels;
    pWaveHeader->sample_rate     = pAencAttr->sampleRate;
    pWaveHeader->byte_rate       = iByteRate;
    pWaveHeader->block_align     = sBlockAlign;
    pWaveHeader->bits_per_sample = sBitsPerSample;
    memcpy(&pWaveHeader->data_id, "data", 4);
    pWaveHeader->data_sz         = iPcmSize;
	
	return SUCCESS;
}


int main(int argc, char **argv)
{
    int ret = 0;
    alogd("Hello, sample_aenc!");
    SampleAEncContext stContext;
    memset(&stContext, 0, sizeof(SampleAEncContext));

    //parse command line param
    if(ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        ret = -1;
        goto _Exit;
    }
    char *pConfigFilePath;
    if(strlen(stContext.mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = stContext.mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = DEFAULT_CONFIGURE_PATH;
    }

    //parse config file.
    if(loadSampleAEncConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        ret = -1;
        goto _Exit;
    }

    stContext.mFpSrcFile = fopen(stContext.mConfigPara.mSrcFilePath, "rb");
    if (NULL == stContext.mFpSrcFile)
    {
        aloge("can not open src file [%s]", stContext.mConfigPara.mSrcFilePath);
		ret = -1;
        goto _Exit;
    }
    stContext.mFpDstFile = fopen(stContext.mConfigPara.mDstFilePath, "wb");
    if (NULL == stContext.mFpDstFile)
    {
        aloge("can not open dst file [%s]", stContext.mConfigPara.mDstFilePath);
		ret = -1;
        goto _Exit;
    }
	if (stContext.mConfigPara.bAddWaveHeader == 1)
	{
		fseek(stContext.mFpDstFile, sizeof(WaveHeader), SEEK_SET);
	}
	
	// Parse wav header
	WaveHeader stWaveHeader;
    fread(&stWaveHeader, 1, sizeof(WaveHeader), stContext.mFpSrcFile);
    configAEncAttrByParseWaveHeader(&stContext.mAEncAttr, &stWaveHeader, stContext.mConfigPara.mCodecType);

    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //create aenc channel.
    BOOL nSuccessFlag = FALSE;
    stContext.mAEncChn = 0;
    AENC_CHN_ATTR_S attr;
    attr.AeAttr = stContext.mAEncAttr;
    while (stContext.mAEncChn < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(stContext.mAEncChn, &attr);
        if(SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", stContext.mAEncChn);
            break;
        }
        else if(ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", stContext.mAEncChn);
            stContext.mAEncChn++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[0x%x], find next!", stContext.mAEncChn, ret);
            stContext.mAEncChn++;
        }
    }
    if(FALSE == nSuccessFlag)
    {
        stContext.mAEncChn = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        ret = -1;
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = NULL;
    ret = AW_MPI_AENC_RegisterCallback(stContext.mAEncChn, &cbInfo);

    //start aenc.
    ret = AW_MPI_AENC_StartRecvPcm(stContext.mAEncChn);

	fseek(stContext.mFpSrcFile, sizeof(WaveHeader), SEEK_SET);
	int wantedSize = stContext.mAEncAttr.channels * stContext.mAEncAttr.bitsPerSample / 8 * 1024;
	
    char* pAudioBuf = (char*)malloc(wantedSize);
	
    while (1)
    {
		// Load PCM
		AUDIO_FRAME_S frame;
        int iFrameSize = fread(pAudioBuf, sizeof(char), wantedSize, stContext.mFpSrcFile);
        frame.mpAddr = pAudioBuf;
        frame.mLen   = iFrameSize;
        if (iFrameSize < wantedSize)
        {
            int bEof = feof(stContext.mFpSrcFile);
            if(bEof)
            {
                alogd("read pcm file finish!");
            }
            break;
        }
		
		// Send data to AENC comp
        AW_MPI_AENC_SendFrame(stContext.mAEncChn, &frame, NULL);
		
		// Wait codec data ready and write to dst file
		while (1)
		{
			AUDIO_STREAM_S stream;
			if (SUCCESS == AW_MPI_AENC_GetStream(stContext.mAEncChn, &stream, 100))
			{// get codec data success
				fwrite(stream.pStream, 1, stream.mLen, stContext.mFpDstFile);
				stContext.mDstFileSize += stream.mLen;
				alogd("get one stream with size: [%d]", stream.mLen);
				
				if (SUCCESS != AW_MPI_AENC_ReleaseStream(stContext.mAEncChn, &stream))
				{// release frame fail, stop try to get codec frame. shouldn't go into this branch
					assert(1);
					break;
				}
			}
			else
			{// no available codec data
				break;
			}
		}
    }
	
	// Add wave header if necessary
	if (  (stContext.mConfigPara.bAddWaveHeader == 1)
	   && (GenerateWaveHeader(&stContext.mAEncAttr, stContext.mDstFileSize, &stWaveHeader) == SUCCESS)
	   )
	{
		fseek(stContext.mFpDstFile, 0, SEEK_SET);
		fwrite(&stWaveHeader, sizeof(WaveHeader), 1, stContext.mFpDstFile);
	}
	
	// release resource
    alogd("produce file [%s] with [%d] bytes!", stContext.mConfigPara.mDstFilePath, stContext.mDstFileSize);
    fclose(stContext.mFpSrcFile);
    fclose(stContext.mFpDstFile);
	free(pAudioBuf);

    //stop and deinit aenc.
    AW_MPI_AENC_StopRecvPcm(stContext.mAEncChn);
    AW_MPI_AENC_ResetChn(stContext.mAEncChn);
    AW_MPI_AENC_DestroyChn(stContext.mAEncChn);

    //exit mpp system
    AW_MPI_SYS_Exit();

_Exit:
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;
}
