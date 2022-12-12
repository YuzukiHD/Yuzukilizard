
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_vdec"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_vdec.h>

#include <confparser.h>

#include "sample_vdec_config.h"
#include "sample_vdec.h"

static ERRORTYPE SampleVDecCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleVDecContext *pContext = (SampleVDecContext*)cookie;
    if(MOD_ID_VDEC == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mVDecChn)
        {
            aloge("fatal error! VDec chnId[%d]!=[%d]", pChn->mChnId, pContext->mVDecChn);
        }
        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("VDec channel notify APP that decode complete!");
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VDEC_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! why modId[0x%x]?!", pChn->mModId);
        ret = FAILURE;
    }
    return ret;
}

static int ParseCmdLine(int argc, char **argv, SampleVDecCmdLineParam *pCmdLinePara)
{
    alogd("sample VDec path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleVDecCmdLineParam));
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
                "\t-path /home/sample_vdec.conf\n");
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

static ERRORTYPE loadSampleVDecConfig(SampleVDecConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;

    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail!");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleVDecConfig));
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_VDEC_FORMAT, NULL);

    if (strcmp(ptr, "jpeg") == 0)
    {
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_VDEC_JPEG_FILE_PATH, NULL);
        strncpy(pConfig->mJpegFilePath, ptr, MAX_FILE_PATH_SIZE-1);
        pConfig->mType = PT_JPEG;
    }
    else if (strcmp(ptr, "h264") == 0)
    {
        //vbs file keep naked stream(sps+pps+idr+p). len file keep every stream size.
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_VDEC_H264_VBS_PATH, NULL);
        strncpy(pConfig->mH264VbsPath, ptr, MAX_FILE_PATH_SIZE-1);

        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_VDEC_H264_LEN_PATH, NULL);
        strncpy(pConfig->mH264LenPath, ptr, MAX_FILE_PATH_SIZE-1);

        pConfig->mType = PT_H264;
    }
    else if (strcmp(ptr, "h265") == 0)
    {
        //vbs file keep naked stream(sps+pps+idr+p). len file keep every stream size.
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_VDEC_H265_VBS_PATH, NULL);
        strncpy(pConfig->mH265VbsPath, ptr, MAX_FILE_PATH_SIZE-1);

        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_VDEC_H265_LEN_PATH, NULL);
        strncpy(pConfig->mH265LenPath, ptr, MAX_FILE_PATH_SIZE-1);

        pConfig->mType = PT_H265;
    }
    else
    {
        aloge("fatal error! why not appoint vencoder type?! exit program!");
        exit(-1);
    }
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_VDEC_DST_FILE_PATH, NULL);
    strncpy(pConfig->mYuvFilePath, ptr, MAX_FILE_PATH_SIZE-1);

    destroyConfParser(&stConfParser);

    return SUCCESS;
}

int main(int argc, char *argv[])
{
    int result = 0;
    alogd("Hello, sample_vdec! func: decode h264/h265/jpeg to yuv!");
    SampleVDecContext stContext;

    //parse command line param
    if (ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0)
    {
        //aloge("fatal error! command line param is wrong, exit!");
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
        pConfigFilePath = DEFAULT_SAMPLE_VDEC_CONF_PATH;
    }

    //parse config file.
    if (loadSampleVDecConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //config vdec chn attr.
    memset(&stContext.mVDecAttr, 0, sizeof(stContext.mVDecAttr));
    stContext.mVDecAttr.mType      = stContext.mConfigPara.mType;
    stContext.mVDecAttr.mOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stContext.mVDecAttr.mBufSize   = 2048*1024;
    stContext.mVDecAttr.mPicWidth  = 1920;
    stContext.mVDecAttr.mPicHeight = 1088;

    //create vdec channel.
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
    stContext.mVDecChn = 0;
    while (stContext.mVDecChn < VDEC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VDEC_CreateChn(stContext.mVDecChn, &stContext.mVDecAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create VDec channel[%d] success!", stContext.mVDecChn);
            break;
        }
        else if (ERR_VDEC_EXIST == ret)
        {
            alogd("VDec channel[%d] exist, find next!", stContext.mVDecChn);
            stContext.mVDecChn++;
        }
        else
        {
            aloge("create VDec channel[%d] fail! ret[0x%x]!", stContext.mVDecChn, ret);
            break;
        }
    }
    if (FALSE == bSuccessFlag)
    {
        stContext.mVDecChn = MM_INVALID_CHN;
        aloge("fatal error! create VDec channel fail!");
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleVDecCallbackWrapper;
    AW_MPI_VDEC_RegisterCallback(stContext.mVDecChn, &cbInfo);
    AW_MPI_VDEC_StartRecvStream(stContext.mVDecChn);

    char DstPath[128] = {0};
    VDEC_STREAM_S nStreamInfo;
    VIDEO_FRAME_INFO_S nFrameInfo;
    memset(&nStreamInfo, 0, sizeof(nStreamInfo));
    nStreamInfo.pAddr = malloc(4096*1024); // max size
    if (NULL == nStreamInfo.pAddr)
    {
        aloge("fatal error! malloc nStreamInfo.pAddr fail! size=4MB");
        goto _exit_1;
    }
    memset(nStreamInfo.pAddr, 0, 4096*1024);

    int yuvId = 0;
    int waitFrmCnt = 0;

    if (stContext.mVDecAttr.mType == PT_JPEG)
    {
        alogd("-----------------------decode jpeg pic begin-----------------------------");
        while(1)
        {
            char SrcPath[64] = {0};
            char DstPath[64] = {0};
            sprintf(SrcPath, "%s%d.jpg", stContext.mConfigPara.mJpegFilePath, yuvId);
            //open jpeg file
            stContext.mFpSrcFile = fopen(SrcPath, "rb");
            if(!stContext.mFpSrcFile)
            {
                alogw("can't open jpeg file[%s], exit loop!", SrcPath);
                break;
            }

            //get stream from file
            {
                fseek(stContext.mFpSrcFile, 0, SEEK_END);
                nStreamInfo.mLen = ftell(stContext.mFpSrcFile);
                rewind(stContext.mFpSrcFile);
                fread(nStreamInfo.pAddr, 1, nStreamInfo.mLen, stContext.mFpSrcFile);
                nStreamInfo.mbEndOfFrame = 1;
                nStreamInfo.mbEndOfStream = 0;
            }
            alogd("------------read file(len:%d,path:%s) and ready sendframe--------------", nStreamInfo.mLen, SrcPath);

            //reopen ve for pic resolution may change
            ret = AW_MPI_VDEC_ReopenVideoEngine(stContext.mVDecChn);
            if (ret != SUCCESS)
            {
                aloge("reopen ve failed?!");
            }

            //send stream to vdec
            ret = AW_MPI_VDEC_SendStream(stContext.mVDecChn, &nStreamInfo, 100);
            if(ret != SUCCESS)
            {
                alogw("send stream with 100ms timeout fail?!");
            }

            //get frame from vdec
            if (AW_MPI_VDEC_GetImage(stContext.mVDecChn, &nFrameInfo, -1) != ERR_VDEC_NOBUF)
            {
                sprintf(DstPath, "%s%02d_%d_%d.yuv", stContext.mConfigPara.mYuvFilePath, yuvId, nFrameInfo.VFrame.mWidth, nFrameInfo.VFrame.mHeight);
                yuvId++;
                //open yuv file
                stContext.mFpDstFile = fopen(DstPath, "wb");
                fwrite(nFrameInfo.VFrame.mpVirAddr[0], 1, nFrameInfo.VFrame.mWidth*nFrameInfo.VFrame.mHeight, stContext.mFpDstFile);
                fwrite(nFrameInfo.VFrame.mpVirAddr[1], 1, nFrameInfo.VFrame.mWidth*nFrameInfo.VFrame.mHeight/2, stContext.mFpDstFile);
                fclose(stContext.mFpSrcFile);
                fclose(stContext.mFpDstFile);
                AW_MPI_VDEC_ReleaseImage(stContext.mVDecChn, &nFrameInfo);
                alogd("--------------get one yuv frame(%s)-----------------", DstPath);
            }

        }
        alogd("-----------------------decode jpeg pic end-----------------------------");
    }
    else if (stContext.mVDecAttr.mType == PT_H264)
    {
        alogd("-----------------------decode h264 naked stream begin-----------------------------");
        FILE *fp_bs = fopen(stContext.mConfigPara.mH264VbsPath, "rb");
        FILE *fp_sz = fopen(stContext.mConfigPara.mH264LenPath, "rb");
        if ((fp_bs==NULL) || (fp_sz==NULL))
        {
            aloge("fopen fail! BitStreamFile: %p, %s, LenFile: %p, %s",
                fp_bs, stContext.mConfigPara.mH264VbsPath, fp_sz, stContext.mConfigPara.mH264LenPath);
            goto _exit_1;
        }
        fseek(fp_sz, 0, SEEK_END);
        int nLenFileSize = ftell(fp_sz);
        fseek(fp_sz, 0, SEEK_SET);
        char *pLenStr = malloc(nLenFileSize);
        if (NULL == pLenStr)
        {
            aloge("fatal error! malloc pLenStr fail! size=%d", nLenFileSize);
            goto _exit_1;
        }
        memset(pLenStr, 0, nLenFileSize);
        fread(pLenStr, 1, nLenFileSize, fp_sz);
        char *endptr0 =  pLenStr, *endptr1 = pLenStr + nLenFileSize;

		sprintf(DstPath, "%sh264.yuv", stContext.mConfigPara.mYuvFilePath);
		stContext.mFpDstFile = fopen(DstPath, "wb");
		if (stContext.mFpDstFile == NULL)
		{
		    aloge("fatal error! Can't create file %s", DstPath);
		    exit(-1);
		}

        while(1)
        {
            int pkt_sz;
            pkt_sz = strtol(endptr0, &endptr1, 10);
            endptr0 += 8;//endptr1;
            if (pkt_sz && yuvId <= 20)
            {
                alogd("yuvId=%d, pkt_sz=%d", yuvId++, pkt_sz);
            }
            else
            {
                alogw("pkt_sz=0 or yuvId big enough, exit loop!");
                break;
            }

            int rd_cnt = fread(nStreamInfo.pAddr, 1, pkt_sz, fp_bs);
            if (rd_cnt != pkt_sz)
            {
                aloge("error happen! pkt_sz=%d, rd_cnt=%d", pkt_sz, rd_cnt);
                break;
            }
            else
            {
                nStreamInfo.mLen = rd_cnt;
                nStreamInfo.mbEndOfFrame = TRUE;
                alogd("read vbs packet! rd_cnt=%d", rd_cnt);
            }

__TryToSendStream:
            //send stream to vdec
            ret = AW_MPI_VDEC_SendStream(stContext.mVDecChn, &nStreamInfo, 100);
            if(ret != SUCCESS)
            {
                alogw("send stream with 100ms timeout fail?! Maybe Vbs is full!");
                goto __TryToSendStream;
            }

__TryToGetFrame:
            //get frame from vdec
            if ((ret=AW_MPI_VDEC_GetImage(stContext.mVDecChn, &nFrameInfo, 1000)) == SUCCESS)
            {
                //sprintf(DstPath, "%s%02d_%d_%d.yuv", stContext.mConfigPara.mYuvFilePath, yuvId++, nFrameInfo.VFrame.mWidth, nFrameInfo.VFrame.mHeight);
                //open yuv file
                //stContext.mFpDstFile = fopen(DstPath, "wb");
                fwrite(nFrameInfo.VFrame.mpVirAddr[0], 1, nFrameInfo.VFrame.mWidth*nFrameInfo.VFrame.mHeight, stContext.mFpDstFile);
                fwrite(nFrameInfo.VFrame.mpVirAddr[1], 1, nFrameInfo.VFrame.mWidth*nFrameInfo.VFrame.mHeight/2, stContext.mFpDstFile);
                //fclose(stContext.mFpDstFile);
                AW_MPI_VDEC_ReleaseImage(stContext.mVDecChn, &nFrameInfo);
                alogd("--------------get one yuv frame(%s)-----------------", DstPath);
                waitFrmCnt = 0;
            }
            else if (ret == ERR_VDEC_NOBUF)
            {
                alogd("no valid buf! waitFrmCnt=%d", waitFrmCnt++);
                if (waitFrmCnt<5)
                    goto __TryToGetFrame;
                alogw("wait one frame for 5s, do not wait anymore! we send another stream!");
            }
        }
        free(pLenStr);
		fclose(stContext.mFpDstFile);
        alogd("-----------------------decode h264 naked stream end-----------------------------");
    }
	else if (stContext.mVDecAttr.mType == PT_H265)
	{
		alogd("-----------------------decode h265 naked stream begin-----------------------------");
		FILE *fp_bs = fopen(stContext.mConfigPara.mH265VbsPath, "rb");
		FILE *fp_sz = fopen(stContext.mConfigPara.mH265LenPath, "rb");
		if ((fp_bs==NULL) || (fp_sz==NULL))
		{
			aloge("fopen fail! fp: %p, sz: %p", fp_bs, fp_sz);
			goto _exit_1;
		}
		fseek(fp_sz, 0, SEEK_END);
		int nLenFileSize = ftell(fp_sz);
		fseek(fp_sz, 0, SEEK_SET);
		char *pLenStr = malloc(nLenFileSize);
        if (NULL == pLenStr)
        {
            aloge("fatal error! malloc pLenStr fail! size=%d", nLenFileSize);
            goto _exit_1;
        }
        memset(pLenStr, 0, nLenFileSize);
		fread(pLenStr, 1, nLenFileSize, fp_sz);
		char *endptr0 =  pLenStr, *endptr1 = pLenStr + nLenFileSize;

		sprintf(DstPath, "%sh265.yuv", stContext.mConfigPara.mYuvFilePath);
		stContext.mFpDstFile = fopen(DstPath, "wb");

		while(1)
		{
			int pkt_sz;
			pkt_sz = strtol(endptr0, &endptr1, 10);
			endptr0 += 8;//endptr1;
			if (pkt_sz)
			{
				alogd("yuvId=%d, pkt_sz=%d", yuvId++, pkt_sz);
			}
			else
			{
				alogw("pkt_sz=0, exit loop!");
				break;
			}

			int rd_cnt = fread(nStreamInfo.pAddr, 1, pkt_sz, fp_bs);
			if (rd_cnt != pkt_sz)
			{
				aloge("error happen! pkt_sz=%d, rd_cnt=%d", pkt_sz, rd_cnt);
				break;
			}
			else
			{
				nStreamInfo.mLen = rd_cnt;
				nStreamInfo.mbEndOfFrame = TRUE;
				alogd("read vbs packet! rd_cnt=%d", rd_cnt);
			}

__TryToSendStream2:
			//send stream to vdec
			ret = AW_MPI_VDEC_SendStream(stContext.mVDecChn, &nStreamInfo, 100);
			if(ret != SUCCESS)
			{
				alogw("send stream with 100ms timeout fail?! Maybe Vbs is full!");
				goto __TryToSendStream2;
			}

__TryToGetFrame2:
			//get frame from vdec
			if ((ret=AW_MPI_VDEC_GetImage(stContext.mVDecChn, &nFrameInfo, 1000)) == SUCCESS)
			{
				//sprintf(DstPath, "%s%02d_%d_%d.yuv", stContext.mConfigPara.mYuvFilePath, yuvId++, nFrameInfo.VFrame.mWidth, nFrameInfo.VFrame.mHeight);
				//open yuv file
				//stContext.mFpDstFile = fopen(DstPath, "wb");
				fwrite(nFrameInfo.VFrame.mpVirAddr[0], 1, nFrameInfo.VFrame.mWidth*nFrameInfo.VFrame.mHeight, stContext.mFpDstFile);
				fwrite(nFrameInfo.VFrame.mpVirAddr[1], 1, nFrameInfo.VFrame.mWidth*nFrameInfo.VFrame.mHeight/2, stContext.mFpDstFile);
				//fclose(stContext.mFpDstFile);
				AW_MPI_VDEC_ReleaseImage(stContext.mVDecChn, &nFrameInfo);
				alogd("--------------get one yuv frame(%s)-----------------", DstPath);
				waitFrmCnt = 0;
			}
			else if (ret == ERR_VDEC_NOBUF)
			{
				alogd("no valid buf! waitFrmCnt=%d", waitFrmCnt++);
				if (waitFrmCnt<5)
					goto __TryToGetFrame2;
				alogw("wait one frame for 5s, do not wait anymore! we send another stream!");
			}
		}
		free(pLenStr);
		
		fclose(stContext.mFpDstFile);
		alogd("-----------------------decode h265 naked stream end-----------------------------");
	}

_exit_1:
    if (nStreamInfo.pAddr)
    {
        free(nStreamInfo.pAddr);
    }

    //stop vdec channel.
    AW_MPI_VDEC_StopRecvStream(stContext.mVDecChn);
    AW_MPI_VDEC_DestroyChn(stContext.mVDecChn);

    //exit mpp system
    AW_MPI_SYS_Exit();

_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
