
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_select"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_aenc.h>

#include <confparser.h>
#include "sample_select.h"
#include <cdx_list.h>


static int ParseCmdLine(int argc, char **argv, SampleSelectCmdLineParam *pCmdLinePara)
{
    alogd("sample select path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleSelectCmdLineParam));
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
            alogd("CmdLine param: -path /home/sample_select.conf");
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

static ERRORTYPE loadSampleSelectConfig(SampleSelectConfig *pConfig, const char *conf_path)
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
    memset(pConfig, 0, sizeof(SampleSelectConfig));
    //audio path
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_SELECT_ASRC_FILE_PATH, NULL);
    strncpy(pConfig->mASrcFilePath, ptr, MAX_FILE_PATH_SIZE);
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_SELECT_ADST_FILE_PATH, NULL);
    strncpy(pConfig->mADstFilePath, ptr, MAX_FILE_PATH_SIZE);

    alogd("parse info: mASrcFilePath[%s]", pConfig->mASrcFilePath);
    alogd("            mADstFilePath[%s]", pConfig->mADstFilePath);
    destroyConfParser(&stConfParser);

    return SUCCESS;
}

int configAEncAttrByWavFile(SampleSelectContext *pCtx, PAYLOAD_TYPE_E type, int index)
{
    pCtx->mFpSrcFile[index] = fopen(pCtx->mConfigPara.mASrcFilePath, "rb");
    if (NULL == pCtx->mFpSrcFile[index])
    {
        aloge("can not open src file [%s]", pCtx->mConfigPara.mASrcFilePath);
        return -1;
    }

    WaveHeader header;
    fread(&header, 1, sizeof(WaveHeader), pCtx->mFpSrcFile[index]);

    AENC_ATTR_S *pAttr = &pCtx->mAEncChnAttr[index];
    pAttr->sampleRate = header.sample_rate;
    pAttr->channels = header.num_chn;
    pAttr->bitRate = 0;             // usful when codec G726
    pAttr->bitsPerSample = header.block_align/header.num_chn*8;
    pAttr->attachAACHeader = 1;     // ADTS
    pAttr->Type = type;

    alogd("aenc attr by use pcm attr: channels[%d], bitWidth[%d], sampleRate[%d], codecType[%d]",
        pAttr->channels, pAttr->bitsPerSample, pAttr->sampleRate, pAttr->Type);

    return 0;
}

void* sendPcmDataLoop(void* argv)
{
    SampleSelectContext *pCtx = (SampleSelectContext *)argv;
    int index = pCtx->mCurrentChnNum;
    alogd("---------begin AEncChn[%d] to send pcm continuously!---------", index);
    fseek(pCtx->mFpSrcFile[index], sizeof(WaveHeader), SEEK_SET);
    int wantedSize = pCtx->mAEncChnAttr[index].channels * pCtx->mAEncChnAttr[index].bitsPerSample/8 * 1024;
    char buf[4096];
    int realSize;
    AUDIO_FRAME_S frame;
    while(1)
    {
        realSize = fread(buf, 1, wantedSize, pCtx->mFpSrcFile[index]);
        frame.mpAddr = buf;
        frame.mLen = realSize;
        if (realSize < wantedSize)
        {
            if (realSize>0)
            {
                AW_MPI_AENC_SendFrame(pCtx->mAEncChn[index], &frame, NULL);
            }
            int bEof = feof(pCtx->mFpSrcFile[index]);
            if(bEof)
            {
                alogd("aenc chn[%d] loop read pcm file finish!", index);
                pCtx->mSendPcmEnd[index] = TRUE;
            }
            break;
        }
        AW_MPI_AENC_SendFrame(pCtx->mAEncChn[index], &frame, NULL);
        // audio send pcm with 1000/10Hz, camera send yuv with 25~60Hz
        usleep(20*1000);
    }

    return NULL;
}

int createAEncDstFile(SampleSelectContext *pCtx, const char *pCodecType, int index)
{
    char dstFilePath[64] = {0};
    strcpy(dstFilePath, pCtx->mConfigPara.mADstFilePath);
    char *pSuffixName = strrchr(dstFilePath, '.') + 1;
    strcpy(pSuffixName, pCodecType);
    pCtx->mFpDstFile[index] = fopen(dstFilePath, "wb");
    if (NULL == pCtx->mFpDstFile[index])
    {
        aloge("can not create dst_file[%d]:[%s]", index, dstFilePath);
        return -1;
    }
    return 0;
}

int createAEncChn(SampleSelectContext *pCtx, int index)
{
    int ret;
    BOOL nSuccessFlag = FALSE;
    AENC_CHN_ATTR_S attr;
    attr.AeAttr = pCtx->mAEncChnAttr[index];
    pCtx->mAEncChn[index] = 0;
    while(pCtx->mAEncChn[index] < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(pCtx->mAEncChn[index], &attr);
        if(SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", pCtx->mAEncChn[index]);
            break;
        }
        else if(ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", pCtx->mAEncChn[index]);
            pCtx->mAEncChn[index]++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[%#x], find next!", pCtx->mAEncChn[index], ret);
            pCtx->mAEncChn[index]++;
        }
    }
    if(FALSE == nSuccessFlag)
    {
        pCtx->mAEncChn[index] = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        ret = -1;
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = pCtx;
    cbInfo.callback = NULL;
    AW_MPI_AENC_RegisterCallback(pCtx->mAEncChn[index], &cbInfo);
    AW_MPI_AENC_StartRecvPcm(pCtx->mAEncChn[index]);

    return ret;
}

int main(int argc, char **argv)
{
    int ret = 0;
    alogd("Hello, sample_select!");
    SampleSelectContext stContext;
    memset(&stContext, 0, sizeof(SampleSelectContext));

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
    if(loadSampleSelectConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        ret = -1;
        goto _Exit;
    }

    EncoderDescription AEncArray[] =
    {
        {PT_MP3, "mp3"},
        {PT_AAC, "aac"},
        //{PT_G726, "g726"},
        //{PT_ADPCMA, "adpcm"},
        //{PT_G711A, "g711a"},
        //{PT_G711U, "g711u"},
        {PT_PCM_AUDIO, "pcm"}
    };

    int AEncChnCnt = sizeof(AEncArray)/sizeof(AEncArray[0]);
    for (int i=0; i<AEncChnCnt; i++)
    {
        //open src file and config aenc chn attr
        configAEncAttrByWavFile(&stContext, AEncArray[i].type, i);
        //dst file
        createAEncDstFile(&stContext, AEncArray[i].name, i);
    }

    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    pthread_t tid;
    for(int i=0; i<AEncChnCnt; i++)
    {
        //aenc chn instance
        createAEncChn(&stContext, i);
        //get enc handle
        stContext.mHandleFd[i] = AW_MPI_AENC_GetHandle(stContext.mAEncChn[i]);
        alogd("---------Ready for select, AEncChn[%d], HandleId[%d]---------", stContext.mAEncChn[i], stContext.mHandleFd[i]);
        stContext.mCurrentChnNum = i;
        pthread_create(&tid, NULL, sendPcmDataLoop, &stContext);
        // you'd better sleep some time
        usleep(20*1000);
    }

    /* find maxFd for select */
    int maxFd = stContext.mHandleFd[0];
    for (int i = 0; i < AEncChnCnt; i++)
    {
        if (maxFd < stContext.mHandleFd[i])
        {
            maxFd = stContext.mHandleFd[i];
        }
    }

    alogd("-------------------wait some time while encoding--------------------");
    handle_set rdFds;
    AUDIO_STREAM_S astream;
    while(1)
    {
        AW_MPI_SYS_HANDLE_ZERO(&rdFds);
        int exit_flag = TRUE;
        for (int i=0; i<AEncChnCnt; i++)
        {
            if (stContext.mSendPcmEnd[i] == FALSE)
            {
                AW_MPI_SYS_HANDLE_SET(stContext.mHandleFd[i], &rdFds);
                exit_flag = FALSE;
            }
        }

        if (exit_flag == FALSE)
        {
            ret = AW_MPI_SYS_HANDLE_Select(maxFd + 1, &rdFds, 100);
            if (ret == 0)
            {
                alogw("handle_select timeout! check end_flag again!");
                int exit_flag = TRUE;
                for (int i=0; i<AEncChnCnt; i++)
                {
                    if (stContext.mSendPcmEnd[i] == FALSE)
                        exit_flag = FALSE;
                }
                if (exit_flag)
                {
                    alogd("----------after check end_flag, all end_flag is TRUE, exit GetStream loop---------");
                    break;
                }
                else
                {
                    alogw("------check code! maybe send_pcm too slow!------");
                }
            }
            else
            {
                for (int i=0; i<AEncChnCnt; i++)
                {
                    if (AW_MPI_SYS_HANDLE_ISSET(stContext.mHandleFd[i], &rdFds))
                    {
                        while(1)
                        {
                            ret = AW_MPI_AENC_GetStream(stContext.mAEncChn[i], &astream, 0);
                            if (ret == SUCCESS)
                            {
                                fwrite(astream.pStream, 1, astream.mLen, stContext.mFpDstFile[i]);
                                stContext.mDstFileSize[i] += astream.mLen;
                                //alogd("CHN[%d] get one stream with size: [%d]", i, stream.mLen);
                                AW_MPI_AENC_ReleaseStream(stContext.mAEncChn[i], &astream);
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            alogd("--------------------receive all channels' end_flag, exit GetStream loop!-----------------------");
            break;
        }
    }

    alogd("------------stop and detroy Enc begin...---------------");
    for (int i=0; i<AEncChnCnt; i++)
    {
//        while(AW_MPI_AENC_GetStream(stContext.mAEncChn[i], &stream, 200) == SUCCESS)
//        {
//            alogd("chn[%d] get one frame with size[%d]", i, stream.mLen);
//            fwrite(stream.pStream, 1, stream.mLen, stContext.mFpDstFile[i]);
//            stContext.mDstFileSize[i] += stream.mLen;
//        }
        AW_MPI_AENC_StopRecvPcm(stContext.mAEncChn[i]);
        //stop and deinit aenc.
        AW_MPI_AENC_DestroyChn(stContext.mAEncChn[i]);
    }

    alogd("------------stop and detroy Enc end!---------------");

    for (int i=0; i<AEncChnCnt; i++)
    {
        alogd("chn[%d] produce dstfile with [%d]bytes & type[%s]!", i, stContext.mDstFileSize[i], AEncArray[i].name);
        fclose(stContext.mFpSrcFile[i]);
        fclose(stContext.mFpDstFile[i]);
    }

    //exit mpp system
    AW_MPI_SYS_Exit();

_Exit:
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;
}
