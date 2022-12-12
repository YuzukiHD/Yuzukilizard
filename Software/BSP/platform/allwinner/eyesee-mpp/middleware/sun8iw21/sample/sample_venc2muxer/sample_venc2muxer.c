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
#define LOG_TAG "SampleVenc2Muxer"

#include <unistd.h>
#include <fcntl.h>
#include "plat_log.h"
#include <time.h>
#include <mm_common.h>

#include "sample_venc2muxer.h"
#include "sample_venc2muxer_conf.h"

#include "ion_memmanager.h"



#define MUX_DEFAULT_SRC_FILE   "/mnt/extsd/sample_venc2muxer/yuv.bin"
#define MUX_DEFAULT_DST_VIDEO_FILE      "/mnt/extsd/sample_venc2muxer/video.mp4"
#define MUX_DEFAULT_SRC_SIZE   1080

#define MUX_DEFAULT_MAX_DURATION  60
#define MUX_DEFAULT_DST_VIDEO_FRAMERATE 25
#define MUX_DEFAULT_DST_VIDEO_BITRATE 8*1000*1000

#define MUX_DEFAULT_SRC_PIXFMT   MM_PIXEL_FORMAT_YUV_PLANAR_420
#define MUX_DEFAULT_DST_PIXFMT   MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420
#define MUX_DEFAULT_ENCODER   PT_H264

#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (4*1024)



static SAMPLE_VENC2MUXER_S *pVenc2MuxerData;


static int setOutputFileSync(SAMPLE_VENC2MUXER_S *pVenc2MuxerData, char* path, int64_t fallocateLength, int muxerId);


static ERRORTYPE initVenc2MuxerData(void)
{
    pVenc2MuxerData = (SAMPLE_VENC2MUXER_S* )malloc(sizeof(SAMPLE_VENC2MUXER_S));
    if (pVenc2MuxerData == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }

    memset(pVenc2MuxerData, 0, sizeof(SAMPLE_VENC2MUXER_S));

    pVenc2MuxerData->mConfigPara.srcSize = pVenc2MuxerData->mConfigPara.dstSize = MUX_DEFAULT_SRC_SIZE;
    if (pVenc2MuxerData->mConfigPara.srcSize == 3840)
    {
        pVenc2MuxerData->mConfigPara.srcWidth = 3840;
        pVenc2MuxerData->mConfigPara.srcHeight = 2160;
    }
    else if (pVenc2MuxerData->mConfigPara.srcSize == 1080)
    {
        pVenc2MuxerData->mConfigPara.srcWidth = 1920;
        pVenc2MuxerData->mConfigPara.srcHeight = 1080;
    }
    else if (pVenc2MuxerData->mConfigPara.srcSize == 720)
    {
        pVenc2MuxerData->mConfigPara.srcWidth = 1280;
        pVenc2MuxerData->mConfigPara.srcHeight = 720;
    }


    if (pVenc2MuxerData->mConfigPara.dstSize == 3840)
    {
        pVenc2MuxerData->mConfigPara.dstWidth = 3840;
        pVenc2MuxerData->mConfigPara.dstHeight = 2160;
    }
	else if (pVenc2MuxerData->mConfigPara.dstSize == 1080)
    {
        pVenc2MuxerData->mConfigPara.dstWidth = 1920;
        pVenc2MuxerData->mConfigPara.dstHeight = 1080;
    }
    else if (pVenc2MuxerData->mConfigPara.dstSize == 720)
    {
        pVenc2MuxerData->mConfigPara.dstWidth = 1280;
        pVenc2MuxerData->mConfigPara.dstHeight = 720;
    }

    pVenc2MuxerData->mConfigPara.mMaxFileDuration = MUX_DEFAULT_MAX_DURATION;
    pVenc2MuxerData->mConfigPara.mVideoFrameRate = MUX_DEFAULT_DST_VIDEO_FRAMERATE;
    pVenc2MuxerData->mConfigPara.mVideoBitRate = MUX_DEFAULT_DST_VIDEO_BITRATE;

    pVenc2MuxerData->mConfigPara.srcPixFmt = MUX_DEFAULT_SRC_PIXFMT;
    pVenc2MuxerData->mConfigPara.dstPixFmt = MUX_DEFAULT_DST_PIXFMT;
    pVenc2MuxerData->mConfigPara.mVideoEncoderFmt = MUX_DEFAULT_ENCODER;
    pVenc2MuxerData->mConfigPara.mField = VIDEO_FIELD_FRAME;

    strcpy(pVenc2MuxerData->mConfigPara.srcYUVFile, MUX_DEFAULT_SRC_FILE);
    strcpy(pVenc2MuxerData->mConfigPara.dstVideoFile, MUX_DEFAULT_DST_VIDEO_FILE);

    pVenc2MuxerData->mMuxGrp = MM_INVALID_CHN;
    pVenc2MuxerData->mVeChn = MM_INVALID_CHN;

    pVenc2MuxerData->mCurRecState = REC_NOT_PREPARED;

    return SUCCESS;
}

static ERRORTYPE parseCmdLine(SAMPLE_VENC2MUXER_S *pVenc2MuxerData, int argc, char** argv)
{
    ERRORTYPE ret = FAILURE;

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = SUCCESS;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pVenc2MuxerData->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pVenc2MuxerData->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_venc2muxer.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_VENC2MUXER_S *pVenc2MuxerData, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S mConf;

    ret = createConfParser(conf_path, &mConf);
    if (ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

    ptr = (char *)GetConfParaString(&mConf, VENC2MUXER_CFG_SRC_FILE_STR, NULL);
    strcpy(pVenc2MuxerData->mConfigPara.srcYUVFile, ptr);

    ptr = (char *)GetConfParaString(&mConf, VENC2MUXER_CFG_DST_VIDEO_FILE_STR, NULL);
    strcpy(pVenc2MuxerData->mConfigPara.dstVideoFile, ptr);

    pVenc2MuxerData->mConfigPara.srcSize = GetConfParaInt(&mConf, VENC2MUXER_CFG_SRC_SIZE, 0);
    if (pVenc2MuxerData->mConfigPara.srcSize == 3840)
    {
        pVenc2MuxerData->mConfigPara.srcWidth = 3840;
        pVenc2MuxerData->mConfigPara.srcHeight = 2160;
    }
    else if (pVenc2MuxerData->mConfigPara.srcSize == 1080)
    {
        pVenc2MuxerData->mConfigPara.srcWidth = 1920;
        pVenc2MuxerData->mConfigPara.srcHeight = 1080;
    }
    else if (pVenc2MuxerData->mConfigPara.srcSize == 720)
    {
        pVenc2MuxerData->mConfigPara.srcWidth = 1280;
        pVenc2MuxerData->mConfigPara.srcHeight = 720;
    }
    pVenc2MuxerData->mConfigPara.mSrcRegionX = GetConfParaInt(&mConf, VENC2MUXER_CFG_SRC_REGION_X, 0);
    pVenc2MuxerData->mConfigPara.mSrcRegionY = GetConfParaInt(&mConf, VENC2MUXER_CFG_SRC_REGION_Y, 0);
    pVenc2MuxerData->mConfigPara.mSrcRegionW = GetConfParaInt(&mConf, VENC2MUXER_CFG_SRC_REGION_W, 0);
    pVenc2MuxerData->mConfigPara.mSrcRegionH = GetConfParaInt(&mConf, VENC2MUXER_CFG_SRC_REGION_H, 0);
    pVenc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    ptr = (char *)GetConfParaString(&mConf, VENC2MUXER_CFG_SRC_PIXFMT, NULL);
    if (ptr != NULL)
    {
        if (!strcmp(ptr, "nv21"))
        {
            pVenc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if (!strcmp(ptr, "aw_afbc"))
        {
            pVenc2MuxerData->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_AFBC;
        }
    }

    pVenc2MuxerData->mConfigPara.dstSize = GetConfParaInt(&mConf, VENC2MUXER_CFG_DST_VIDEO_SIZE, 0);
    if (pVenc2MuxerData->mConfigPara.dstSize == 3840)
    {
        pVenc2MuxerData->mConfigPara.dstWidth = 3840;
        pVenc2MuxerData->mConfigPara.dstHeight = 2160;
    }
    else if (pVenc2MuxerData->mConfigPara.dstSize == 1080)
    {
        pVenc2MuxerData->mConfigPara.dstWidth = 1920;
        pVenc2MuxerData->mConfigPara.dstHeight = 1080;
    }
    else if (pVenc2MuxerData->mConfigPara.dstSize == 720)
    {
        pVenc2MuxerData->mConfigPara.dstWidth = 1280;
        pVenc2MuxerData->mConfigPara.dstHeight = 720;
    }

    pVenc2MuxerData->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, VENC2MUXER_CFG_DST_VIDEO_FRAMERATE, 0);
    pVenc2MuxerData->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, VENC2MUXER_CFG_DST_VIDEO_BITRATE, 0);
    pVenc2MuxerData->mConfigPara.mMaxFileDuration = GetConfParaInt(&mConf, VENC2MUXER_CFG_DST_VIDEO_DURATION, 0);
    pVenc2MuxerData->mConfigPara.mRcMode = GetConfParaInt(&mConf, VENC2MUXER_CFG_ENC_RC_MODE, 0);


    ptr	= (char *)GetConfParaString(&mConf, VENC2MUXER_CFG_DST_VIDEO_ENCODER, NULL);
    if (!strcmp(ptr, "H.264"))
    {
        pVenc2MuxerData->mConfigPara.mVideoEncoderFmt = PT_H264;
    }
    else if (!strcmp(ptr, "H.265"))
    {
        pVenc2MuxerData->mConfigPara.mVideoEncoderFmt = PT_H265;
    }
    else if (!strcmp(ptr, "MJPEG"))
    {
        pVenc2MuxerData->mConfigPara.mVideoEncoderFmt = PT_MJPEG;
    }
    else
    {
        aloge("error conf encoder type! use default h264 instead");
        pVenc2MuxerData->mConfigPara.mVideoEncoderFmt = PT_H264;
    }

    ptr	= (char *)GetConfParaString(&mConf, VENC2MUXER_CFG_DST_FILE_FORMAT, NULL);
    if (!strcmp(ptr, "mp4"))
    {
        pVenc2MuxerData->mConfigPara.mMediaFileFmt = MEDIA_FILE_FORMAT_MP4;
    }
    else if (!strcmp(ptr, "ts"))
    {
        pVenc2MuxerData->mConfigPara.mMediaFileFmt = MEDIA_FILE_FORMAT_TS;
    }
    else
    {
        aloge("error conf encoder type! use default mp4 format!");
        pVenc2MuxerData->mConfigPara.mMediaFileFmt = MEDIA_FILE_FORMAT_MP4;
    }

    pVenc2MuxerData->mConfigPara.mEncUseProfile = GetConfParaInt(&mConf, VENC2MUXER_CFG_DST_ENC_PROFILE, 0);
    pVenc2MuxerData->mConfigPara.mTestDuration = GetConfParaInt(&mConf, VENC2MUXER_CFG_TEST_DURATION, 0);

    alogd("frame rate:%d, bitrate:%d, duration=%d", pVenc2MuxerData->mConfigPara.mVideoFrameRate,\
            pVenc2MuxerData->mConfigPara.mVideoBitRate, pVenc2MuxerData->mConfigPara.mMaxFileDuration);
    alogd("encoder fmt=%d, rc_mode=%d, file format=%d", pVenc2MuxerData->mConfigPara.mVideoEncoderFmt,\
            pVenc2MuxerData->mConfigPara.mRcMode, pVenc2MuxerData->mConfigPara.mMediaFileFmt);

    destroyConfParser(&mConf);
    return SUCCESS;
}


static int getFileNameByCurTime(char *pNameBuf)
{
    static int file_cnt = 0;
    int len = strlen(pVenc2MuxerData->mConfigPara.dstVideoFile);
    char *ptr = pVenc2MuxerData->mConfigPara.dstVideoFile;
	char *pNameBuf_ = NULL;
	pNameBuf_ = (char *)malloc(128);
    while (*(ptr+len-1) != '.')
    {
        len--;
    }

    strncpy(pNameBuf_, pVenc2MuxerData->mConfigPara.dstVideoFile, len-1);
    sprintf(pNameBuf, "%s_%d.mp4", pNameBuf_, file_cnt);
    file_cnt++;
    alogd("file name: %s", pNameBuf);
	free(pNameBuf_);
    return 0;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_VENC2MUXER_S *pVenc2MuxerData = (SAMPLE_VENC2MUXER_S *)cookie;

    if (MOD_ID_VENC == pChn->mModId)
    {
        VIDEO_FRAME_INFO_S *pFrame = (VIDEO_FRAME_INFO_S *)pEventData;
        switch(event)
        {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                if (pFrame != NULL)
                {
                    pthread_mutex_lock(&pVenc2MuxerData->mInBuf_Q.mUseListLock);
                    if (!list_empty(&pVenc2MuxerData->mInBuf_Q.mUseList))
                    {
                        IN_FRAME_NODE_S *pEntry, *pTmp;
                        list_for_each_entry_safe(pEntry, pTmp, &pVenc2MuxerData->mInBuf_Q.mUseList, mList)
                        {
                            if (pEntry->mFrame.mId == pFrame->mId)
                            {
                                pthread_mutex_lock(&pVenc2MuxerData->mInBuf_Q.mIdleListLock);
                                list_move_tail(&pEntry->mList, &pVenc2MuxerData->mInBuf_Q.mIdleList);
                                pthread_mutex_unlock(&pVenc2MuxerData->mInBuf_Q.mIdleListLock);
                                break;
                            }
                        }
                    }
                    pthread_mutex_unlock(&pVenc2MuxerData->mInBuf_Q.mUseListLock);
                }
            }
            break;

        default:
            break;
        }
    }
    else if (MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
        case MPP_EVENT_RECORD_DONE:
            {
                int muxerId = *(int*)pEventData;
                alogd("file done, mux_id=%d", muxerId);
            }
            break;

        case MPP_EVENT_NEED_NEXT_FD:
            {
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Venc2Muxer_MessageData stMsgData;

                alogd("mux need next fd");
                stMsgData.pVenc2MuxerData = (SAMPLE_VENC2MUXER_S*)cookie;
                stCmdMsg.command = Rec_NeedSetNextFd;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;

                putMessageWithData(&pVenc2MuxerData->mMsgQueue, &stCmdMsg);
                /*
                int muxerId = *(int*)pEventData;
                SAMPLE_VENC2MUXER_S *pVenc2MuxerData = (SAMPLE_VENC2MUXER_S *)cookie;
                char fileName[128] = {0};

                alogd("mux need next fd");
                getFileNameByCurTime(fileName);
                setOutputFileSync(pVenc2MuxerData, fileName, 0, muxerId);
                */
            }
            break;

        case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
            }
            break;

        default:
            break;
        }
    }

    return SUCCESS;
}

static ERRORTYPE configMuxGrpAttr(SAMPLE_VENC2MUXER_S *pVenc2MuxerData)
{
    memset(&pVenc2MuxerData->mMuxGrpAttr, 0, sizeof(MUX_GRP_ATTR_S));

    pVenc2MuxerData->mMuxGrpAttr.mVideoAttrValidNum = 1;
    pVenc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mVideoEncodeType = pVenc2MuxerData->mConfigPara.mVideoEncoderFmt;
    pVenc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mWidth = pVenc2MuxerData->mConfigPara.dstWidth;
    pVenc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mHeight = pVenc2MuxerData->mConfigPara.dstHeight;
    pVenc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mVideoFrmRate = pVenc2MuxerData->mConfigPara.mVideoFrameRate*1000;
    pVenc2MuxerData->mMuxGrpAttr.mVideoAttr[0].mVeChn = pVenc2MuxerData->mVeChn;
    //pVenc2MuxerData->mMuxGrpAttr.mMaxKeyInterval = 
    pVenc2MuxerData->mMuxGrpAttr.mAudioEncodeType = PT_MAX;

    return SUCCESS;
}

static ERRORTYPE createMuxGrp(SAMPLE_VENC2MUXER_S *pVenc2MuxerData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configMuxGrpAttr(pVenc2MuxerData);
    pVenc2MuxerData->mMuxGrp = 0;
    while (pVenc2MuxerData->mMuxGrp < MUX_MAX_GRP_NUM)
    {
        ret = AW_MPI_MUX_CreateGrp(pVenc2MuxerData->mMuxGrp, &pVenc2MuxerData->mMuxGrpAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux group[%d] success!", pVenc2MuxerData->mMuxGrp);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("mux group[%d] is exist, find next!", pVenc2MuxerData->mMuxGrp);
            pVenc2MuxerData->mMuxGrp++;
        }
        else
        {
            alogd("create mux group[%d] ret[0x%x], find next!", pVenc2MuxerData->mMuxGrp, ret);
            pVenc2MuxerData->mMuxGrp++;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pVenc2MuxerData->mMuxGrp = MM_INVALID_CHN;
        aloge("fatal error! create mux group fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pVenc2MuxerData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pVenc2MuxerData->mMuxGrp, &cbInfo);
        return SUCCESS;
    }
}

static int addOutputFormatAndOutputSink_1(SAMPLE_VENC2MUXER_S *pVenc2MuxerData, OUTSINKINFO_S *pSinkInfo)
{
    int retMuxerId = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;

    alogd("fmt:0x%x, fd:%d, FallocateLen:%d, callback_out_flag:%d", pSinkInfo->mOutputFormat, pSinkInfo->mOutputFd, pSinkInfo->mFallocateLen, pSinkInfo->mCallbackOutFlag);
    if (pSinkInfo->mOutputFd >= 0 && TRUE == pSinkInfo->mCallbackOutFlag)
    {
        aloge("fatal error! one muxer cannot support two sink methods!");
        return -1;
    }

    //find if the same output_format sinkInfo exist or callback out stream is exist.
    pthread_mutex_lock(&pVenc2MuxerData->mChnListLock);
    if (!list_empty(&pVenc2MuxerData->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pVenc2MuxerData->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFormat == pSinkInfo->mOutputFormat)
            {
                alogd("Be careful! same outputForamt[0x%x] exist in array", pSinkInfo->mOutputFormat);
            }
            if (pEntry->mSinkInfo.mCallbackOutFlag == pSinkInfo->mCallbackOutFlag)
            {
                aloge("fatal error! only support one callback out stream");
            }
        }
    }
    pthread_mutex_unlock(&pVenc2MuxerData->mChnListLock);

    MUX_CHN_INFO_S *p_node = (MUX_CHN_INFO_S *)malloc(sizeof(MUX_CHN_INFO_S));
    if (p_node == NULL)
    {
        aloge("alloc mux chn info node fail");
        return -1;
    }

    memset(p_node, 0, sizeof(MUX_CHN_INFO_S));
    p_node->mSinkInfo.mMuxerId = pVenc2MuxerData->mMuxerIdCounter;
    p_node->mSinkInfo.mOutputFormat = pSinkInfo->mOutputFormat;
    if (pSinkInfo->mOutputFd > 0)
    {
        p_node->mSinkInfo.mOutputFd = dup(pSinkInfo->mOutputFd);
    }
    else
    {
        p_node->mSinkInfo.mOutputFd = -1;
    }
    p_node->mSinkInfo.mFallocateLen = pSinkInfo->mFallocateLen;
    p_node->mSinkInfo.mCallbackOutFlag = pSinkInfo->mCallbackOutFlag;

    p_node->mMuxChnAttr.mMuxerId = p_node->mSinkInfo.mMuxerId;
    p_node->mMuxChnAttr.mMediaFileFormat = p_node->mSinkInfo.mOutputFormat;
    p_node->mMuxChnAttr.mMaxFileDuration = pVenc2MuxerData->mConfigPara.mMaxFileDuration * 1000; //s -> ms
    p_node->mMuxChnAttr.mFallocateLen = p_node->mSinkInfo.mFallocateLen;
    p_node->mMuxChnAttr.mCallbackOutFlag = p_node->mSinkInfo.mCallbackOutFlag;
    p_node->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    p_node->mMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;

    p_node->mMuxChn = MM_INVALID_CHN;

    if ((pVenc2MuxerData->mCurRecState == REC_PREPARED) || (pVenc2MuxerData->mCurRecState == REC_RECORDING))
    {
        ERRORTYPE ret;
        BOOL nSuccessFlag = FALSE;
        MUX_CHN nMuxChn = 0;
        while (nMuxChn < MUX_MAX_CHN_NUM)
        {
            ret = AW_MPI_MUX_CreateChn(pVenc2MuxerData->mMuxGrp, nMuxChn, &p_node->mMuxChnAttr, p_node->mSinkInfo.mOutputFd);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pVenc2MuxerData->mMuxGrp, nMuxChn, p_node->mMuxChnAttr.mMuxerId);
                break;
            }
            else if (ERR_MUX_EXIST == ret)
            {
                alogd("mux group[%d] channel[%d] is exist, find next!", pVenc2MuxerData->mMuxGrp, nMuxChn);
                nMuxChn++;
            }
            else
            {
                aloge("fatal error! create mux group[%d] channel[%d] fail ret[0x%x], find next!", pVenc2MuxerData->mMuxGrp, nMuxChn, ret);
                nMuxChn++;
            }
        }

        if (nSuccessFlag)
        {
            retMuxerId = p_node->mSinkInfo.mMuxerId;
            p_node->mMuxChn = nMuxChn;
            pVenc2MuxerData->mMuxerIdCounter++;
        }
        else
        {
            aloge("fatal error! create mux group[%d] channel fail!", pVenc2MuxerData->mMuxGrp);
            if (p_node->mSinkInfo.mOutputFd >= 0)
            {
                close(p_node->mSinkInfo.mOutputFd);
                p_node->mSinkInfo.mOutputFd = -1;
            }

            retMuxerId = -1;
        }

        pthread_mutex_lock(&pVenc2MuxerData->mChnListLock);
        list_add_tail(&p_node->mList, &pVenc2MuxerData->mMuxChnList);
        pthread_mutex_unlock(&pVenc2MuxerData->mChnListLock);
    }
    else
    {
        retMuxerId = p_node->mSinkInfo.mMuxerId;
        pVenc2MuxerData->mMuxerIdCounter++;
        pthread_mutex_lock(&pVenc2MuxerData->mChnListLock);
        list_add_tail(&p_node->mList, &pVenc2MuxerData->mMuxChnList);
        pthread_mutex_unlock(&pVenc2MuxerData->mChnListLock);
    }

    return retMuxerId;
}

static int addOutputFormatAndOutputSink(SAMPLE_VENC2MUXER_S *pVenc2MuxerData, char* path, MEDIA_FILE_FORMAT_E mediaFileFmt)
{
    int muxerId = -1;
    OUTSINKINFO_S sinkInfo = {0};

    if (path != NULL)
    {
        sinkInfo.mFallocateLen = 0;
        sinkInfo.mCallbackOutFlag = FALSE;
        sinkInfo.mOutputFormat = mediaFileFmt;
        sinkInfo.mOutputFd = open(path, O_RDWR | O_CREAT, 0666);
        if (sinkInfo.mOutputFd < 0)
        {
            aloge("Failed to open %s", path);
            return -1;
        }

        muxerId = addOutputFormatAndOutputSink_1(pVenc2MuxerData, &sinkInfo);
        close(sinkInfo.mOutputFd);
    }

    return muxerId;
}

static int setOutputFileSync_1(SAMPLE_VENC2MUXER_S *pVenc2MuxerData, int fd, int64_t fallocateLength, int muxerId)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;

    alogv("setOutputFileSync fd=%d", fd);
    if (fd < 0)
    {
        aloge("Invalid parameter");
        return -1;
    }

    MUX_CHN muxChn = MM_INVALID_CHN;
    pthread_mutex_lock(&pVenc2MuxerData->mChnListLock);
    if (!list_empty(&pVenc2MuxerData->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pVenc2MuxerData->mMuxChnList, mList)
        {
            if (pEntry->mMuxChnAttr.mMuxerId == muxerId)
            {
                muxChn = pEntry->mMuxChn;
                break;
            }
        }
    }
    pthread_mutex_unlock(&pVenc2MuxerData->mChnListLock);

    if (muxChn != MM_INVALID_CHN)
    {
        AW_MPI_MUX_SwitchFd(pVenc2MuxerData->mMuxGrp, muxChn, fd, fallocateLength);
        return 0;
    }
    else
    {
        aloge("fatal error! can't find muxChn which muxerId[%d]", muxerId);
        return -1;
    }
}

static int setOutputFileSync(SAMPLE_VENC2MUXER_S *pVenc2MuxerData, char* path, int64_t fallocateLength, int muxerId)
{
    int ret;

    if (pVenc2MuxerData->mCurRecState != REC_RECORDING)
    {
        aloge("not in recording state");
        return -1;
    }

    if (path != NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT, 0666);
        if (fd < 0)
        {
            aloge("fail to open %s", path);
            return -1;
        }
        ret = setOutputFileSync_1(pVenc2MuxerData, fd, fallocateLength, muxerId);
        close(fd);

        return ret;
    }
    else
    {
        return -1;
    }
}

static inline unsigned int map_H264_UserSet2Profile(int val)
{
    unsigned int profile = (unsigned int)H264_PROFILE_HIGH;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H264_PROFILE_BASE;
        break;

    case 1:
        profile = (unsigned int)H264_PROFILE_MAIN;
        break;

    case 2:
        profile = (unsigned int)H264_PROFILE_HIGH;
        break;

    default:
        break;
    }

    return profile;
}

static inline unsigned int map_H265_UserSet2Profile(int val)
{
    unsigned int profile = H265_PROFILE_MAIN;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H265_PROFILE_MAIN;
        break;

    case 1:
        profile = (unsigned int)H265_PROFILE_MAIN10;
        break;

    case 2:
        profile = (unsigned int)H265_PROFILE_STI11;
        break;

    default:
        break;
    }
    return profile;
}

static int configVencChnAttr(SAMPLE_VENC2MUXER_S *pVenc2MuxerData)
{
    memset(&pVenc2MuxerData->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));

    pVenc2MuxerData->mVencChnAttr.VeAttr.Type = pVenc2MuxerData->mConfigPara.mVideoEncoderFmt;
    //pVenc2MuxerData->mVencChnAttr.VeAttr.MaxKeyInterval = pVenc2MuxerData->mConfigPara.mVideoMaxKeyItl;

    pVenc2MuxerData->mVencChnAttr.VeAttr.SrcPicWidth  = pVenc2MuxerData->mConfigPara.srcWidth;
    pVenc2MuxerData->mVencChnAttr.VeAttr.SrcPicHeight = pVenc2MuxerData->mConfigPara.srcHeight;

    //pVenc2MuxerData->mVencChnAttr.VeAttr.PixelFormat = pVenc2MuxerData->mConfigPara.dstPixFmt;
    if (pVenc2MuxerData->mConfigPara.srcPixFmt == MM_PIXEL_FORMAT_YUV_AW_AFBC)
    {
        alogd("aw_afbc");
        pVenc2MuxerData->mVencChnAttr.VeAttr.PixelFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
        pVenc2MuxerData->mConfigPara.dstWidth = pVenc2MuxerData->mConfigPara.srcWidth;  //cannot compress_encoder
        pVenc2MuxerData->mConfigPara.dstHeight = pVenc2MuxerData->mConfigPara.srcHeight; //cannot compress_encoder
    }
    else
    {
        pVenc2MuxerData->mVencChnAttr.VeAttr.PixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    pVenc2MuxerData->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;

    pVenc2MuxerData->mVencRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
    pVenc2MuxerData->mVencRcParam.sensor_type = VENC_ST_EN_WDR;

    if (PT_H264 == pVenc2MuxerData->mVencChnAttr.VeAttr.Type)
    {
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        //pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.Profile = VENC_H264ProfileHigh;
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.Profile = map_H264_UserSet2Profile(pVenc2MuxerData->mConfigPara.mEncUseProfile);
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pVenc2MuxerData->mConfigPara.dstWidth;
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pVenc2MuxerData->mConfigPara.dstHeight;
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        switch (pVenc2MuxerData->mConfigPara.mRcMode)
        {
        case 1:
            pVenc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mMinQp = 10;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mMaxQp = 50;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pVenc2MuxerData->mConfigPara.mVideoBitRate;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mMaxPqp = 50;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mMinPqp = 10;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mQpInit = 35;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mMovingTh = 20;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mQuality = 10;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
            pVenc2MuxerData->mVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
            break;
        case 2:
            pVenc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 35;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 35;
            break;
        case 3:
            pVenc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264QPMAP;
            break;
        case 0:
        default:
            pVenc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pVenc2MuxerData->mConfigPara.mVideoFrameRate;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Cbr.fr32DstFrmRate = pVenc2MuxerData->mConfigPara.mVideoFrameRate;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pVenc2MuxerData->mConfigPara.mVideoBitRate;
            pVenc2MuxerData->mVencRcParam.ParamH264Cbr.mMaxQp = 50;
            pVenc2MuxerData->mVencRcParam.ParamH264Cbr.mMinQp = 10;
            pVenc2MuxerData->mVencRcParam.ParamH264Cbr.mMaxPqp = 50;
            pVenc2MuxerData->mVencRcParam.ParamH264Cbr.mMinPqp = 10;
            pVenc2MuxerData->mVencRcParam.ParamH264Cbr.mQpInit = 35;
            pVenc2MuxerData->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 0;
            break;
        }
    }
    else if (PT_H265 == pVenc2MuxerData->mVencChnAttr.VeAttr.Type)
    {
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mProfile = map_H265_UserSet2Profile(pVenc2MuxerData->mConfigPara.mEncUseProfile);
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pVenc2MuxerData->mConfigPara.dstWidth;
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pVenc2MuxerData->mConfigPara.dstHeight;
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        switch (pVenc2MuxerData->mConfigPara.mRcMode)
        {
        case 1:
            pVenc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mMinQp = 10;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mMaxQp = 50;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pVenc2MuxerData->mConfigPara.mVideoBitRate;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mMaxPqp = 50;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mMinPqp = 10;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mQpInit = 35;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mMovingTh = 20;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mQuality = 10;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
            pVenc2MuxerData->mVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
            break;
        case 2:
            pVenc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 35;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 35;
            break;
        case 3:
            pVenc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265QPMAP;
            break;
        case 0:
        default:
            pVenc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pVenc2MuxerData->mConfigPara.mVideoFrameRate;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Cbr.fr32DstFrmRate = pVenc2MuxerData->mConfigPara.mVideoFrameRate;
            pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pVenc2MuxerData->mConfigPara.mVideoBitRate;
            pVenc2MuxerData->mVencRcParam.ParamH265Cbr.mMaxQp = 50;
            pVenc2MuxerData->mVencRcParam.ParamH265Cbr.mMinQp = 10;
            pVenc2MuxerData->mVencRcParam.ParamH265Cbr.mMaxPqp = 50;
            pVenc2MuxerData->mVencRcParam.ParamH265Cbr.mMinPqp = 10;
            pVenc2MuxerData->mVencRcParam.ParamH265Cbr.mQpInit = 35;
            pVenc2MuxerData->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 0;
            break;
        }
    }
    else if (PT_MJPEG == pVenc2MuxerData->mVencChnAttr.VeAttr.Type)
    {
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pVenc2MuxerData->mConfigPara.dstWidth;
        pVenc2MuxerData->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pVenc2MuxerData->mConfigPara.dstHeight;
        pVenc2MuxerData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        pVenc2MuxerData->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pVenc2MuxerData->mConfigPara.mVideoBitRate;
    }

    return SUCCESS;
}


static ERRORTYPE createVencChn(SAMPLE_VENC2MUXER_S *pVenc2MuxerData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pVenc2MuxerData);
    pVenc2MuxerData->mVeChn = 0;
    while (pVenc2MuxerData->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pVenc2MuxerData->mVeChn, &pVenc2MuxerData->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pVenc2MuxerData->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pVenc2MuxerData->mVeChn);
            pVenc2MuxerData->mVeChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", pVenc2MuxerData->mVeChn, ret);
            pVenc2MuxerData->mVeChn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pVenc2MuxerData->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }
    else
    {
        AW_MPI_VENC_SetRcParam(pVenc2MuxerData->mVeChn, &pVenc2MuxerData->mVencRcParam);

        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = stFrameRate.DstFrmRate = pVenc2MuxerData->mConfigPara.mVideoFrameRate;
        AW_MPI_VENC_SetFrameRate(pVenc2MuxerData->mVeChn, &stFrameRate);

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pVenc2MuxerData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pVenc2MuxerData->mVeChn, &cbInfo);
        return SUCCESS;
    }
}

static ERRORTYPE prepare(SAMPLE_VENC2MUXER_S *pVenc2MuxerData)
{
    BOOL nSuccessFlag;
    MUX_CHN nMuxChn;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret;
    ERRORTYPE result = SUCCESS;

    if (createVencChn(pVenc2MuxerData) != SUCCESS)
    {
        aloge("create venc chn fail");
        return FAILURE;
    }

    if (createMuxGrp(pVenc2MuxerData) != SUCCESS)
    {
        aloge("create mux group fail");
        return FAILURE;
    }

    //set spspps
    if (pVenc2MuxerData->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        AW_MPI_VENC_GetH264SpsPpsInfo(pVenc2MuxerData->mVeChn, &H264SpsPpsInfo);
        AW_MPI_MUX_SetH264SpsPpsInfo(pVenc2MuxerData->mMuxGrp, pVenc2MuxerData->mVeChn, &H264SpsPpsInfo);
    }
    else if(pVenc2MuxerData->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        AW_MPI_VENC_GetH265SpsPpsInfo(pVenc2MuxerData->mVeChn, &H265SpsPpsInfo);
        AW_MPI_MUX_SetH265SpsPpsInfo(pVenc2MuxerData->mMuxGrp, pVenc2MuxerData->mVeChn, &H265SpsPpsInfo);
    }

    pthread_mutex_lock(&pVenc2MuxerData->mChnListLock);
    if (!list_empty(&pVenc2MuxerData->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pVenc2MuxerData->mMuxChnList, mList)
        {
            nMuxChn = 0;
            nSuccessFlag = FALSE;
            while (pEntry->mMuxChn < MUX_MAX_CHN_NUM)
            {
                ret = AW_MPI_MUX_CreateChn(pVenc2MuxerData->mMuxGrp, nMuxChn, &pEntry->mMuxChnAttr, pEntry->mSinkInfo.mOutputFd);
                if (SUCCESS == ret)
                {
                    nSuccessFlag = TRUE;
                    alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pVenc2MuxerData->mMuxGrp, pEntry->mMuxChn, pEntry->mMuxChnAttr.mMuxerId);
                    break;
                }
                else if(ERR_MUX_EXIST == ret)
                {
                    nMuxChn++;
                    //break;
                }
                else
                {
                    nMuxChn++;
                }
            }

            if (FALSE == nSuccessFlag)
            {
                result = FAILURE;
                pEntry->mMuxChn = MM_INVALID_CHN;
                aloge("fatal error! create mux group[%d] channel fail!", pVenc2MuxerData->mMuxGrp);
            }
            else
            {
                pEntry->mMuxChn = nMuxChn;
            }
        }
    }
    else
    {
        aloge("maybe something wrong,chn list is empty");
    }
    pthread_mutex_unlock(&pVenc2MuxerData->mChnListLock);

    if (pVenc2MuxerData->mVeChn >= 0 && pVenc2MuxerData->mMuxGrp >= 0)
    {
        MPP_CHN_S MuxGrp = {MOD_ID_MUX, 0, pVenc2MuxerData->mMuxGrp};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pVenc2MuxerData->mVeChn};

        AW_MPI_SYS_Bind(&VeChn, &MuxGrp);
        pVenc2MuxerData->mCurRecState = REC_PREPARED;
    }

    return result;
}

static ERRORTYPE start(SAMPLE_VENC2MUXER_S *pVenc2MuxerData)
{
    ERRORTYPE ret = SUCCESS;

    if (pVenc2MuxerData->mVeChn >= 0)
    {
        AW_MPI_VENC_StartRecvPic(pVenc2MuxerData->mVeChn);
    }

    if (pVenc2MuxerData->mMuxGrp >= 0)
    {
        AW_MPI_MUX_StartGrp(pVenc2MuxerData->mMuxGrp);
    }

    pVenc2MuxerData->mCurRecState = REC_RECORDING;

    return ret;
}

static ERRORTYPE stop(SAMPLE_VENC2MUXER_S *pVenc2MuxerData)
{
    ERRORTYPE ret = SUCCESS;
    MUX_CHN_INFO_S *pEntry, *pTmp;

    alogd("stop");

    pVenc2MuxerData->mCurRecState = REC_STOP;

    if (pVenc2MuxerData->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pVenc2MuxerData->mVeChn);
    }

    if (pVenc2MuxerData->mMuxGrp >= 0)
    {
        alogd("stop mux grp");
        AW_MPI_MUX_StopGrp(pVenc2MuxerData->mMuxGrp);
    }

    if (pVenc2MuxerData->mMuxGrp >= 0)
    {
        alogd("destory mux grp");
        AW_MPI_MUX_DestroyGrp(pVenc2MuxerData->mMuxGrp);
        pVenc2MuxerData->mMuxGrp = MM_INVALID_CHN;
    }

    if (pVenc2MuxerData->mVeChn >= 0)
    {
        alogd("destory venc");
        AW_MPI_VENC_ResetChn(pVenc2MuxerData->mVeChn);
        AW_MPI_VENC_DestroyChn(pVenc2MuxerData->mVeChn);
        pVenc2MuxerData->mVeChn = MM_INVALID_CHN;
    }

    return ret;
}

static int venc_MemOpen(void)
{
    return ion_memOpen();
}

static int venc_MemClose(void)
{
    return ion_memClose();
}

static void* venc_allocMem(unsigned int size)
{
    //return (void *)ion_allocMem(size);
    IonAllocAttr allocAttr;
    memset(&allocAttr, 0, sizeof(IonAllocAttr));
    allocAttr.mLen = size;
    allocAttr.mAlign = 0;
    allocAttr.mIonHeapType = IonHeapType_IOMMU;
    allocAttr.mbSupportCache = 0;
    return ion_allocMem_extend(&allocAttr);
}

static int venc_freeMem(void *pVirAddr)
{
    return ion_freeMem(pVirAddr);
}

static void* venc_getPhyAddrByVirAddr(void *pVirAddr)
{
    return (void *)ion_getMemPhyAddr(pVirAddr);
}

static int venc_flushCache(void *pVirAddr, unsigned int size)
{
    return ion_flushCache(pVirAddr, size);
}

static int vencBufMgrCreate(int frmNum, int videoSize, INPUT_BUF_Q *pBufList, BOOL isAwAfbc)
{
    unsigned int width, height, size;
    unsigned int afbc_header;

    if (videoSize == 720)
    {
        width = 1280;
        height = 720;
        size = ALIGN(1280,16)*ALIGN(720,16);
    }
    else if (videoSize == 1080)
    {
        width = 1920;
        height = 1080;
        size = ALIGN(1920,16)*ALIGN(1080,16);
    }
    else if (videoSize == 3840)
    {
        width = 3840;
        height = 2160;
        size = ALIGN(3840,16)*ALIGN(2160,16);
    }
    else
    {
        aloge("not support this video size:%d p", videoSize);
        return -1;
    }

    INIT_LIST_HEAD(&pBufList->mIdleList);
    INIT_LIST_HEAD(&pBufList->mReadyList);
    INIT_LIST_HEAD(&pBufList->mUseList);

    pthread_mutex_init(&pBufList->mIdleListLock, NULL);
    pthread_mutex_init(&pBufList->mReadyListLock, NULL);
    pthread_mutex_init(&pBufList->mUseListLock, NULL);

    if (isAwAfbc)
    {
        afbc_header = ((width +127)>>7)*((height+31)>>5)*96;
    }

    int i;
    for (i = 0; i < frmNum; ++i)
    {
        IN_FRAME_NODE_S *pFrameNode = (IN_FRAME_NODE_S*)malloc(sizeof(IN_FRAME_NODE_S));
        if (pFrameNode == NULL)
        {
            aloge("alloc IN_FRAME_NODE_S error!");
            break;
        }
        memset(pFrameNode, 0, sizeof(IN_FRAME_NODE_S));

        if (!isAwAfbc)
        {
            pFrameNode->mFrame.VFrame.mpVirAddr[0] = (void *)venc_allocMem(size);
            if (pFrameNode->mFrame.VFrame.mpVirAddr[0] == NULL)
            {
                aloge("alloc y_vir_addr size %d error!", size);
                free(pFrameNode);
                break;
            }
            pFrameNode->mFrame.VFrame.mpVirAddr[1] = (void *)venc_allocMem(size/2);
            if (pFrameNode->mFrame.VFrame.mpVirAddr[1] == NULL)
            {
                aloge("alloc uv_vir_addr size %d error!", size/2);
                venc_freeMem(pFrameNode->mFrame.VFrame.mpVirAddr[0]);
                pFrameNode->mFrame.VFrame.mpVirAddr[0] = NULL;
                free(pFrameNode);
                break;
            }

            pFrameNode->mFrame.VFrame.mPhyAddr[0] = (unsigned int)venc_getPhyAddrByVirAddr(pFrameNode->mFrame.VFrame.mpVirAddr[0]);
            pFrameNode->mFrame.VFrame.mPhyAddr[1] = (unsigned int)venc_getPhyAddrByVirAddr(pFrameNode->mFrame.VFrame.mpVirAddr[1]);
        }
        else
        {
            pFrameNode->mFrame.VFrame.mpVirAddr[0] = (void *)venc_allocMem(size+size/2+afbc_header);
            if (pFrameNode->mFrame.VFrame.mpVirAddr[0] == NULL)
            {
                aloge("fail to alloc aw_afbc y_vir_addr!");
                free(pFrameNode);
                break;
            }
            pFrameNode->mFrame.VFrame.mPhyAddr[0] = (unsigned int)venc_getPhyAddrByVirAddr(pFrameNode->mFrame.VFrame.mpVirAddr[0]);
        }

        pFrameNode->mFrame.mId = i;
        list_add_tail(&pFrameNode->mList, &pBufList->mIdleList);
        pBufList->mFrameNum++;
    }

    if (pBufList->mFrameNum == 0)
    {
        aloge("alloc no node!!");
        return -1;
    }
    else
    {
        return 0;
    }
}

static int vencBufMgrDestroy(INPUT_BUF_Q *pBufList)
{
    IN_FRAME_NODE_S *pEntry, *pTmp;
    int frmnum = 0;

    if (pBufList == NULL)
    {
        aloge("pBufList null");
        return -1;
    }

    pthread_mutex_lock(&pBufList->mUseListLock);
    if (!list_empty(&pBufList->mUseList))
    {
        aloge("error! SendingFrmList should be 0! maybe some frames not release!");
        list_for_each_entry_safe(pEntry, pTmp, &pBufList->mUseList, mList)
        {
            list_del(&pEntry->mList);
            venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[0]);
            if (NULL != pEntry->mFrame.VFrame.mpVirAddr[1])
            {
                venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[1]);
            }
            free(pEntry);
            ++frmnum;
        }
    }
    pthread_mutex_unlock(&pBufList->mUseListLock);

    pthread_mutex_lock(&pBufList->mReadyListLock);
    if (!list_empty(&pBufList->mReadyList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pBufList->mReadyList, mList)
        {
            list_del(&pEntry->mList);
            venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[0]);
            if (NULL != pEntry->mFrame.VFrame.mpVirAddr[1])
            {
                venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[1]);
            }
            free(pEntry);
            ++frmnum;
        }
    }
    pthread_mutex_unlock(&pBufList->mReadyListLock);


    pthread_mutex_lock(&pBufList->mIdleListLock);
    if (!list_empty(&pBufList->mIdleList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pBufList->mIdleList, mList)
        {
            list_del(&pEntry->mList);
            venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[0]);
            if (NULL != pEntry->mFrame.VFrame.mpVirAddr[1])
            {
                venc_freeMem(pEntry->mFrame.VFrame.mpVirAddr[1]);
            }
            free(pEntry);
            ++frmnum;
        }
    }
    pthread_mutex_unlock(&pBufList->mIdleListLock);

    if (frmnum != pBufList->mFrameNum)
    {
        aloge("Fatal error! frame node number is not match[%d][%d]", frmnum, pBufList->mFrameNum);
    }

    pthread_mutex_destroy(&pBufList->mIdleListLock);
    pthread_mutex_destroy(&pBufList->mReadyListLock);
    pthread_mutex_destroy(&pBufList->mUseListLock);

    return 0;
}

static int Yu12ToNv21(int width, int height, char *addr_uv, char *addr_tmp_uv)
{
    int i, chroma_bytes;
    char *u_addr = NULL;
    char *v_addr = NULL;
    char *tmp_addr = NULL;

    chroma_bytes = width*height/4;

    u_addr = addr_uv;
    v_addr = addr_uv + chroma_bytes;
    tmp_addr = addr_tmp_uv;

    for (i=0; i<chroma_bytes; i++)
    {
        *(tmp_addr++) = *(v_addr++);
        *(tmp_addr++) = *(u_addr++);
    }

    memcpy(addr_uv, addr_tmp_uv, chroma_bytes*2);

    return 0;
}

static void *readInFrameThread(void *pThreadData)
{
    int ret;
    int read_len;
    int size1, size2;
    SAMPLE_VENC2MUXER_S *pVenc2MuxerData = (SAMPLE_VENC2MUXER_S*)pThreadData;
    uint64_t curPts = 0;
    IN_FRAME_NODE_S *pFrame;

    fseek(pVenc2MuxerData->mConfigPara.srcYUVFd, 0, SEEK_SET);

    while (!pVenc2MuxerData->mOverFlag)
    {
        pthread_mutex_lock(&pVenc2MuxerData->mInBuf_Q.mReadyListLock);
        while (1)
        {
            if (list_empty(&pVenc2MuxerData->mInBuf_Q.mReadyList))
            {
                //alogd("ready list empty");
                break;
            }

            pFrame = list_first_entry(&pVenc2MuxerData->mInBuf_Q.mReadyList, IN_FRAME_NODE_S, mList);
            pthread_mutex_lock(&pVenc2MuxerData->mInBuf_Q.mUseListLock);
            list_move_tail(&pFrame->mList, &pVenc2MuxerData->mInBuf_Q.mUseList);
            pthread_mutex_unlock(&pVenc2MuxerData->mInBuf_Q.mUseListLock);

            ret = AW_MPI_VENC_SendFrame(pVenc2MuxerData->mVeChn, &pFrame->mFrame, 0);
            if (ret == SUCCESS)
            {
                //alogd("send frame[%d] to enc", pFrame->mFrame.mId);
            }
            else
            {
                alogd("send frame[%d] fail", pFrame->mFrame.mId);
                pthread_mutex_lock(&pVenc2MuxerData->mInBuf_Q.mUseListLock);
                list_move_tail(&pFrame->mList, &pVenc2MuxerData->mInBuf_Q.mReadyList);
                pthread_mutex_unlock(&pVenc2MuxerData->mInBuf_Q.mUseListLock);
                break;
            }
        }
        pthread_mutex_unlock(&pVenc2MuxerData->mInBuf_Q.mReadyListLock);

        if (feof(pVenc2MuxerData->mConfigPara.srcYUVFd))
        {
            alogd("read to end");
            cdx_sem_up(&pVenc2MuxerData->mSemExit);
            break;
        }

        pthread_mutex_lock(&pVenc2MuxerData->mInBuf_Q.mIdleListLock);
        if (!list_empty(&pVenc2MuxerData->mInBuf_Q.mIdleList))
        {
            IN_FRAME_NODE_S *pFrame = list_first_entry(&pVenc2MuxerData->mInBuf_Q.mIdleList, IN_FRAME_NODE_S, mList);
            if (pFrame != NULL)
            {
                pFrame->mFrame.VFrame.mWidth  = pVenc2MuxerData->mConfigPara.srcWidth;
                pFrame->mFrame.VFrame.mHeight = pVenc2MuxerData->mConfigPara.srcHeight;
                //test area encode.
                if(0 == pVenc2MuxerData->mConfigPara.mSrcRegionW || 0 == pVenc2MuxerData->mConfigPara.mSrcRegionY)
                {
                    pFrame->mFrame.VFrame.mOffsetLeft = 0;
                    pFrame->mFrame.VFrame.mOffsetTop  = 0;
                    pFrame->mFrame.VFrame.mOffsetRight = pFrame->mFrame.VFrame.mOffsetLeft + pFrame->mFrame.VFrame.mWidth;
                    pFrame->mFrame.VFrame.mOffsetBottom = pFrame->mFrame.VFrame.mOffsetTop + pFrame->mFrame.VFrame.mHeight;
                }
                else
                {
                    pFrame->mFrame.VFrame.mOffsetLeft = pVenc2MuxerData->mConfigPara.mSrcRegionX;
                    pFrame->mFrame.VFrame.mOffsetTop  = pVenc2MuxerData->mConfigPara.mSrcRegionY;
                    pFrame->mFrame.VFrame.mOffsetRight = pFrame->mFrame.VFrame.mOffsetLeft + pVenc2MuxerData->mConfigPara.mSrcRegionW;
                    pFrame->mFrame.VFrame.mOffsetBottom = pFrame->mFrame.VFrame.mOffsetTop + pVenc2MuxerData->mConfigPara.mSrcRegionY;
                }
                
                pFrame->mFrame.VFrame.mField  = VIDEO_FIELD_FRAME;
                pFrame->mFrame.VFrame.mVideoFormat	= VIDEO_FORMAT_LINEAR;
                pFrame->mFrame.VFrame.mCompressMode = COMPRESS_MODE_NONE;
                pFrame->mFrame.VFrame.mpts = curPts;
                curPts += (1*1000*1000) / pVenc2MuxerData->mConfigPara.mVideoFrameRate;

                if (MM_PIXEL_FORMAT_YUV_AW_AFBC != pVenc2MuxerData->mConfigPara.srcPixFmt)
                {
                    pFrame->mFrame.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
                    read_len = pFrame->mFrame.VFrame.mWidth * pFrame->mFrame.VFrame.mHeight;
                    size1 = fread(pFrame->mFrame.VFrame.mpVirAddr[0], 1, read_len, pVenc2MuxerData->mConfigPara.srcYUVFd);
                    size2 = fread(pFrame->mFrame.VFrame.mpVirAddr[1], 1, read_len /2, pVenc2MuxerData->mConfigPara.srcYUVFd);
                    if ((size1 != read_len) || (size2 != read_len/2))
                    {
                        alogw("warning: read to eof or somthing wrong, should stop. readlen=%d, size1=%d, size2=%d, w=%d,h=%d", read_len,\
                                        size1, size2, pFrame->mFrame.VFrame.mWidth, pFrame->mFrame.VFrame.mHeight);
                        pthread_mutex_unlock(&pVenc2MuxerData->mInBuf_Q.mIdleListLock);
                        cdx_sem_up(&pVenc2MuxerData->mSemExit);
                        break;
                    }
                    //Yu12ToNv21(pFrame->mFrame.VFrame.mWidth, pFrame->mFrame.VFrame.mHeight, pFrame->mFrame.VFrame.mpVirAddr[1], pVenc2MuxerData->tmpBuf);
                    venc_flushCache((void *)pFrame->mFrame.VFrame.mpVirAddr[0], read_len);
                    venc_flushCache((void *)pFrame->mFrame.VFrame.mpVirAddr[1], read_len/2);
                }
                else
                {
                    pFrame->mFrame.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
                    unsigned int afbc_size;
                    afbc_size = ((pFrame->mFrame.VFrame.mWidth + 127)>>7) * ((pFrame->mFrame.VFrame.mHeight + 31)>>5)*96;
                    read_len = pFrame->mFrame.VFrame.mWidth * pFrame->mFrame.VFrame.mHeight;
                    read_len = read_len + read_len/2 + afbc_size;
                    size1 = fread(pFrame->mFrame.VFrame.mpVirAddr[0], 1, read_len, pVenc2MuxerData->mConfigPara.srcYUVFd);
                    if (size1 != read_len)
                    {
                        alogw("warning: awafbc eof or somthing wrong, should stop. readlen=%d, size=%d, w=%d,h=%d", read_len,\
                                        size1, pFrame->mFrame.VFrame.mWidth, pFrame->mFrame.VFrame.mHeight);
                        pthread_mutex_unlock(&pVenc2MuxerData->mInBuf_Q.mIdleListLock);
                        cdx_sem_up(&pVenc2MuxerData->mSemExit);
                        break;
                    }
                    venc_flushCache((void *)pFrame->mFrame.VFrame.mpVirAddr[0], read_len);
                }

                pthread_mutex_lock(&pVenc2MuxerData->mInBuf_Q.mReadyListLock);
                list_move_tail(&pFrame->mList, &pVenc2MuxerData->mInBuf_Q.mReadyList);
                pthread_mutex_unlock(&pVenc2MuxerData->mInBuf_Q.mReadyListLock);
                //alogd("get frame[%d] to ready list", pFrame->mFrame.mId);
            }
        }
        else
        {
            alogd("idle list empty!!");
        }
        pthread_mutex_unlock(&pVenc2MuxerData->mInBuf_Q.mIdleListLock);
    }

    alogd("read thread exit!");
    return NULL;
}

void *MsgQueueThread(void *pThreadData)
{
    message_t stCmdMsg;
    Venc2MuxerMsgType cmd;
    int nCmdPara;
    SAMPLE_VENC2MUXER_S *pVenc2MuxerData = (SAMPLE_VENC2MUXER_S*)pThreadData;

    alogd("message queue start.");
    while (1)
    {
        if (0 == get_message(&pVenc2MuxerData->mMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case Rec_NeedSetNextFd:
                {
                    int muxerId = nCmdPara;
                    Venc2Muxer_MessageData *pMsgData = (Venc2Muxer_MessageData*)stCmdMsg.mpData;
                    char fileName[128] = {0};

                    getFileNameByCurTime(fileName);
                    setOutputFileSync(pMsgData->pVenc2MuxerData, fileName, 0, muxerId);
                    //free message mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case MsgQueue_Stop:
                {
                    goto _EXIT;
                    break;
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pVenc2MuxerData->mMsgQueue, 0);
        }
    }
_EXIT:
    alogd("message queue stop.");
    return NULL;
}

int main(int argc, char** argv)
{
    int result = -1;
    int ret = 0;
    int size;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    message_t stCmdMsg;

    if (initVenc2MuxerData() != SUCCESS)
    {
        return -1;
    }
    pVenc2MuxerData->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pVenc2MuxerData->mSysConf);
    AW_MPI_SYS_Init();

    cdx_sem_init(&pVenc2MuxerData->mSemExit, 0);

    if (parseCmdLine(pVenc2MuxerData, argc, argv) != SUCCESS)
    {
        aloge("parse cmdline fail");
        goto err_out_0;
    }

    if (loadConfigPara(pVenc2MuxerData, pVenc2MuxerData->mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("load config file fail");
        goto err_out_0;
    }

    if (venc_MemOpen() != 0 )
    {
        alogd("open mem fail!");
        goto err_out_0;
    }

    INIT_LIST_HEAD(&pVenc2MuxerData->mMuxChnList);
    pthread_mutex_init(&pVenc2MuxerData->mChnListLock, NULL);

    if (message_create(&pVenc2MuxerData->mMsgQueue) != 0)
    {
        aloge("fatal error! create message queue fail!");
        goto err_out_0;
    }

    BOOL isAwAfbc = (MM_PIXEL_FORMAT_YUV_AW_AFBC == pVenc2MuxerData->mConfigPara.srcPixFmt);
    ret = vencBufMgrCreate(IN_FRAME_BUF_NUM, pVenc2MuxerData->mConfigPara.srcSize, &pVenc2MuxerData->mInBuf_Q, isAwAfbc);
    if (ret != 0)
    {
        aloge("ERROR: create FrameBuf manager fail");
        goto err_out_1;
    }

    if (MM_PIXEL_FORMAT_YUV_AW_AFBC != pVenc2MuxerData->mConfigPara.srcPixFmt)
    {
        //for yuv420p -> yvu420sp tmp_buf
        size = pVenc2MuxerData->mConfigPara.srcWidth * pVenc2MuxerData->mConfigPara.srcHeight / 2;
        pVenc2MuxerData->tmpBuf = (char *)malloc(size);
        if (pVenc2MuxerData->tmpBuf == NULL)
        {
            goto err_out_2;
        }
    }

    pVenc2MuxerData->mConfigPara.srcYUVFd = fopen(pVenc2MuxerData->mConfigPara.srcYUVFile, "r");
    if (pVenc2MuxerData->mConfigPara.srcYUVFd == NULL)
    {
        aloge("ERROR: cannot open yuv src in file %s", pVenc2MuxerData->mConfigPara.srcYUVFile);
        goto err_out_3;
    }

    addOutputFormatAndOutputSink(pVenc2MuxerData, pVenc2MuxerData->mConfigPara.dstVideoFile, pVenc2MuxerData->mConfigPara.mMediaFileFmt);

    if (prepare(pVenc2MuxerData) != SUCCESS)
    {
        aloge("prepare fail!");
        goto err_out_4;
    }

    pVenc2MuxerData->mOverFlag = FALSE;
    ret = pthread_create(&pVenc2MuxerData->mReadThdId, NULL, readInFrameThread, pVenc2MuxerData);
    if (ret || !pVenc2MuxerData->mReadThdId)
    {
        aloge("create read frame thread fail!");
        goto err_out_5;
    }

    //create message thread
    ret = pthread_create(&pVenc2MuxerData->mMsgQueueThreadId, NULL, MsgQueueThread, pVenc2MuxerData);
    if (ret != 0)
    {
        aloge("create message queue thread fail!");
        goto err_out_6;
    }

    start(pVenc2MuxerData);

    if (pVenc2MuxerData->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pVenc2MuxerData->mSemExit, pVenc2MuxerData->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pVenc2MuxerData->mSemExit);
    }
    result = 0;
    //stop message thread
    stCmdMsg.command = MsgQueue_Stop;
    put_message(&pVenc2MuxerData->mMsgQueue, &stCmdMsg);
    pthread_join(pVenc2MuxerData->mMsgQueueThreadId, NULL);
err_out_6:
    pVenc2MuxerData->mOverFlag = TRUE;
    alogd("over, start to free res");
    pthread_join(pVenc2MuxerData->mReadThdId, (void*) &ret);
err_out_5:
    stop(pVenc2MuxerData);
err_out_4:
    pthread_mutex_lock(&pVenc2MuxerData->mChnListLock);
    if (!list_empty(&pVenc2MuxerData->mMuxChnList))
    {
        alogd("free chn list node");
        list_for_each_entry_safe(pEntry, pTmp, &pVenc2MuxerData->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFd > 0)
            {
                alogd("close file");
                close(pEntry->mSinkInfo.mOutputFd);
                pEntry->mSinkInfo.mOutputFd = -1;
            }

            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pVenc2MuxerData->mChnListLock);

    if (NULL != pVenc2MuxerData->mConfigPara.srcYUVFd)
    {
        fclose(pVenc2MuxerData->mConfigPara.srcYUVFd);
        pVenc2MuxerData->mConfigPara.srcYUVFd = NULL;
    }
err_out_3:
    if (pVenc2MuxerData->tmpBuf != NULL)
    {
        free(pVenc2MuxerData->tmpBuf);
        pVenc2MuxerData->tmpBuf = NULL;
    }
err_out_2:
    vencBufMgrDestroy(&pVenc2MuxerData->mInBuf_Q);
err_out_1:
    pthread_mutex_destroy(&pVenc2MuxerData->mChnListLock);
    message_destroy(&pVenc2MuxerData->mMsgQueue);
err_out_0:
    cdx_sem_deinit(&pVenc2MuxerData->mSemExit);
    AW_MPI_SYS_Exit();
    venc_MemClose();
    free(pVenc2MuxerData);
    pVenc2MuxerData = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
