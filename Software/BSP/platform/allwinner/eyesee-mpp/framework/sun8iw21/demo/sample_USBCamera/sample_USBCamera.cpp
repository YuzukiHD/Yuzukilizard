/*-----------------------------------------------------------------------------
    
    Copyright (C) 2018 T2-Design Corp. All rights reserved.
    
    File: sample_USBCamera.cpp
    
    Content: 
    
    History: 
    2018/1/7 by folks
        
-----------------------------------------------------------------------------*/

//---------------------------- PRIVATE VARIABLES ----------------------------//


//---------------------------- PRIVATE FUNCTIONS ----------------------------//


//---------------------------- PUBLIC FUNCTIONS -----------------------------//


//#define LOG_NDEBUG 0
#define LOG_TAG "sample_USBCamera"
#include <utils/plat_log.h>
#include <signal.h>
#include <iostream>

#include <mpi_sys.h>
#include <mpi_vo.h>
#include <confparser.h>

#include "sample_USBCamera.h"
#include "sample_USBCamera_config.h"

using namespace EyeseeLinux;

SampleUSBCamContext *pSampleUSBCamContext = NULL;
void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleUSBCamContext)
    {
        cdx_sem_up(&pSampleUSBCamContext->mSemExit);
    }
}

void EyeseeUSBCameraCallback::onError(UvcChn chnId, CameraMsgErrorType error, EyeseeUSBCamera *pCamera)
{
    alogw("uvcChn[%d] send error[0x%x]! ignore.", chnId, error);
}
bool EyeseeUSBCameraCallback::onInfo(UvcChn chnId, CameraMsgInfoType info, int extra, EyeseeUSBCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            alogd("uvcChn[%d] notify render start!", chnId);
            break;
        }
        default:
        {
            aloge("fatal error! unknown info[0x%x] from uvcChn[%d]", info, chnId);
            bHandleInfoFlag = false;
            break;
        }
    }
    return bHandleInfoFlag;
}

/**
 * @param data = jpeg + thumbOffset(4 bytes) + thumbLen(4 bytes) + jpegSize(4 bytes)
 */
void EyeseeUSBCameraCallback::onPictureTaken(UvcChn chnId, const void *data, int size, EyeseeUSBCamera* pCamera)
{
    int ret = -1;
    alogd("channel %d picture data[%p] size %d", chnId, data, size);
    char picName[64];
    sprintf(picName, "pic[chn%d][%02d].jpg", (int)chnId, mpContext->mPicNumArray[(int)chnId]);
    mpContext->mPicNumArray[(int)chnId]++;

    //void *p = const_cast<void *>(data) + size - 2 * sizeof(size_t) - sizeof(off_t);
    char *p = static_cast<char*>(const_cast<void *>(data)) + size - 2 * sizeof(size_t) - sizeof(off_t);

    if(1)
    {
        off_t nThumbOffset = *(off_t*)p;
        
        p += sizeof(off_t);
        size_t nThumbLen = *(size_t*)p;

        p+= sizeof(size_t);
        size_t nJpegSize = *(size_t*)p;
    }
    else
    {
        PictureROIType roitype = (PictureROIType)(*((off_t*)p));

        p += sizeof(off_t);
        size_t id = *((size_t*)p);

        p += sizeof(size_t);
        size_t fscore = *((size_t*)p);
        //float Ffscore = fscore / 100.0f;
        alogd("the photo PictureROIType[%d], Id[%d], Fscore[%d]!", roitype, id, fscore);
    }
    
    
    std::string PicFullPath = mpContext->mConfigParam.mDirPath + '/' + picName;
    FILE *fp = fopen(PicFullPath.c_str(), "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
    alogd("save jpeg[%s]", picName);
}

void EyeseeUSBCameraCallback::addPictureRegion(std::list<PictureRegionType> &rPictureRegionList)
{
    alogd("you can set picture region here.");
}

EyeseeUSBCameraCallback::EyeseeUSBCameraCallback(SampleUSBCamContext *pContext)
    :mpContext(pContext)
{
}
EyeseeUSBCameraCallback::~EyeseeUSBCameraCallback()
{
}

void EyeseeRecorderListener::onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra)
{
    alogd("receive onError message![%d][%d]", what, extra);
    switch(what)
    {
        case EyeseeRecorder::MEDIA_ERROR_VENC_TIMEOUT:
        {
            uint64_t framePts = *(uint64_t*)extra;
            alogd("app know error afbc frame pts[%lld]us", framePts);
            break;
        }
        case EyeseeRecorder::MEDIA_ERROR_SERVER_DIED:
            break;
        default:
            break;
    }
}

void EyeseeRecorderListener::onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra)
{
    switch(what)
    {
        case EyeseeRecorder::MEDIA_RECORDER_INFO_NEED_SET_NEXT_FD:
        {
            alogd("receive onInfo message! media_info_type[%d] muxer_id[%d]", what, extra);
            int nMuxerId1 = extra;
            if(nMuxerId1 != mpOwner->mMuxerId)
            {
                aloge("fatal error! why muxerId is not match[%d]!=[%d]?", mpOwner->mMuxerId, nMuxerId1);
            }
            std::string strSegmentFilePath = mpOwner->mConfigParam.mDirPath + "/" + mpOwner->MakeSegmentFileName();
            mpOwner->mpRecorder->setOutputFileSync(const_cast<char *>(strSegmentFilePath.c_str()), 0, nMuxerId1);
            mpOwner->mSegmentFiles.push_back(strSegmentFilePath);
            break;
        }   
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            alogd("receive onInfo message! media_info_type[%d] muxer_id[%d]", what, extra);
            int nMuxerId2 = extra;
            int ret;
            while(mpOwner->mSegmentFiles.size() > mpOwner->mConfigParam.mSegmentCounts)
            {
                if((ret = remove(mpOwner->mSegmentFiles[0].c_str())) < 0)
                {
                    aloge("fatal error! delete file[%s] failed:%s", mpOwner->mSegmentFiles[0].c_str(), strerror(errno));
                }
                else
                {
                    alogd("delete file[%s] success, ret[0x%x]", mpOwner->mSegmentFiles[0].c_str(), ret);
                }
                mpOwner->mSegmentFiles.pop_front();
            }
            break;
        }   
        default:
            alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);
            break;
    }
}

void EyeseeRecorderListener::onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra)
{
    aloge("fatal error! Do not test CallbackOut!");
}

EyeseeRecorderListener::EyeseeRecorderListener(SampleUSBCamContext *pOwner)
    :mpOwner(pOwner)
{
    
}


SampleUSBCamContext::SampleUSBCamContext()
    :mUSBCameraListener(this)
    ,mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    memset(&mSysConf, 0, sizeof mSysConf);
    mUILayer = HLAY(2, 0);
    mpUSBCamera = NULL;
    mpRecorder = NULL;
    mFileNum = 0;
    mPicNumArray.resize(3, 0);
    bzero(&mConfigParam,sizeof(mConfigParam));
    mMuxerId = -1;
    mPicNum = -1;
}

SampleUSBCamContext::~SampleUSBCamContext()
{
    if(mpUSBCamera)
    {
        delete mpUSBCamera;
        mpUSBCamera = NULL;
    }
    if(mpRecorder)
    {
        delete mpRecorder;
        mpRecorder = NULL;
    }

    cdx_sem_deinit(&mSemExit);
}

EyeseeLinux::status_t SampleUSBCamContext::ParseCmdLine(int argc, char *argv[])
{    
    alogd("this program path:[%s], arg number is [%d]", argv[0], argc);    
    EyeseeLinux::status_t ret = 0;    
    int i = 1;    
    while(i < argc)    
    {        
        if(!strcmp(argv[i], "-path"))        
        {            
            if(++i >= argc)            
            {                
                std::string errorString;                
                errorString = "fatal error! use -h to learn how to set parameter!!!";                
                std::cout << errorString << std::endl;                
                ret = -1;                
                break;            
            }            
            mCmdLineParam.mConfigFilePath = argv[i];        
        }       
        else if(!strcmp(argv[i], "-h"))        
        {            
            std::string helpString;            
            helpString += "CmdLine param example:\n";            
            helpString += "\t run -path /home/sample_ThumbRetriever.conf\n";            
            std::cout << helpString << std::endl;            
            ret = 1;            
            break;        
        }        
        else        
        {            
            std::string ignoreString;            
            ignoreString += "ignore invalid CmdLine param:[";            
            ignoreString += argv[i];            
            ignoreString += ", type -h to get how to set parameter!";            
            std::cout << ignoreString << std::endl;        
        }        
        i++;    
    }    
    return ret;    
}

EyeseeLinux::status_t SampleUSBCamContext::loadConfig()
{
    EyeseeLinux::status_t ret;
    char *ptr = NULL;
    std::string& ConfigFilePath = mCmdLineParam.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        mConfigParam.mUSBCamDevName = "/dev/video0";
        mConfigParam.mCaptureWidth = 1280;
        mConfigParam.mCaptureHeight = 720;
        mConfigParam.mPicFormat = UVC_MJPEG;
        mConfigParam.mFrameRate = 30;
        mConfigParam.mFrameNum = 5;

        mConfigParam.mVdecFlag = 1;
        mConfigParam.mVdecBufSize = 0;
        mConfigParam.mVdecPriority = 0;
        mConfigParam.mVdecPicWidth = 0;
        mConfigParam.mVdecPicHeight = 0;
        mConfigParam.mVdecOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigParam.mVdecSubPicEnable = 1;
        mConfigParam.mVdecSubPicWidth = 640;
        mConfigParam.mVdecSubPicHeight = 360;
        mConfigParam.mVdecSubOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigParam.mVdecExtraFrameNum = 0;

        mConfigParam.mDispChnNum = 1;
        mConfigParam.mDispWidth = 640;
        mConfigParam.mDispHeight = 480;
        mConfigParam.mPreviewRotation = 0;

        mConfigParam.mPhotoChn = UvcChn::VdecMainChn;
        mConfigParam.mPhotoNum = 0;
        mConfigParam.mPhotoInterval = 0;
        mConfigParam.mPhotoPostviewEnable = false;
        mConfigParam.mPhotoWidth = 0;
        mConfigParam.mPhotoHeight = 0;
        mConfigParam.mPhotoThumbWidth = 0;
        mConfigParam.mPhotoThumbHeight = 0;

        mConfigParam.mEncodeChn = UvcChn::VdecMainChn;
        mConfigParam.mEncodeType = PT_H264;
        mConfigParam.mEncodeWidth = 1280;
        mConfigParam.mEncodeHeight = 720;
        mConfigParam.mEncodeBitrate = 8 * 1024 * 1024;
        mConfigParam.mDirPath = "/mnt/extsd/sample_USBCamera_Files";
        mConfigParam.mSegmentCounts = 0;
        mConfigParam.mSegmentDuration = 0;

        mConfigParam.mTestDuration = 0;
        return SUCCESS;
        
    }

    
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return -1;
    }

    mConfigParam.mUSBCamDevName = GetConfParaString(&stConfParser, SAMPLE_USBCAMERA_KEY_USBCAM_DEVNAME, NULL);
    mConfigParam.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_CAPTURE_WIDTH, 1280);
    mConfigParam.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_CAPTURE_HEIGHT, 720);

    ptr = const_cast<char *>(GetConfParaString(&stConfParser, SAMPLE_USBCAMERA_KEY_PIC_FORMAT, NULL));
    if(!strcmp(ptr, "yuyv"))
    {
        mConfigParam.mPicFormat = UVC_YUY2;
    }
    else if(!strcmp(ptr, "mjpeg"))
    {
        mConfigParam.mPicFormat = UVC_MJPEG;   
    }
    else if(!strcmp(ptr, "h264"))
    {
        mConfigParam.mPicFormat = UVC_H264;
    }
    else
    {
        aloge("fatal error!, the pic format is error");
        mConfigParam.mPicFormat = UVC_MJPEG;
    }
    mConfigParam.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_FRAME_RATE, 30);
    mConfigParam.mFrameNum = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_FRAME_NUM, 5);
    mConfigParam.mVdecFlag = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_FLAG, 1);
    mConfigParam.mVdecBufSize = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_BUFSIZE, 0);
    mConfigParam.mVdecPriority = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_PRIORITY, 0);
    mConfigParam.mVdecPicWidth = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_PIC_WIDTH, 0);
    mConfigParam.mVdecPicHeight = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_PIC_HEIGHT, 0);
    ptr = const_cast<char *>(GetConfParaString(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_OUTPUT_PIXELFORMAT, NULL));
    if(!strcmp(ptr, "NV21"))
    {
        mConfigParam.mVdecOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(ptr, "NV12"))
    {
        mConfigParam.mVdecOutputPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else
    {
        aloge("fatal error, the vdec Main output pixelformat is error!");
        mConfigParam.mVdecOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }

    mConfigParam.mVdecSubPicEnable = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_SUBPIC_ENABLE, 1);
    mConfigParam.mVdecSubPicWidth = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_SUBPIC_WIDTH, 640);
    mConfigParam.mVdecSubPicHeight = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_SUBPIC_HEIGHT, 360);
    ptr = const_cast<char *>(GetConfParaString(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_SUBOUTPUT_PIXELFORMAT, NULL));
    if(!strcmp(ptr, "NV21"))
    {
        mConfigParam.mVdecSubOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(ptr, "NV12"))
    {
        mConfigParam.mVdecSubOutputPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else
    {
        aloge("fatal error!, the vdec sub output pixelformat is error!");
        mConfigParam.mVdecSubOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigParam.mVdecExtraFrameNum = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_VDEC_EXTRA_FRAME_NUM, 0);
    mConfigParam.mDispChnNum = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_DISP_CHNNUM, 1);
    mConfigParam.mDispWidth = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_DISP_WIDTH, 640);
    mConfigParam.mDispHeight = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_DISP_HEIGHT, 480);
    mConfigParam.mPreviewRotation = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_PREVIEW_ROTATIOM, 0);

    mConfigParam.mPhotoChn = (UvcChn)GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_PHOTO_CHN, 0);
    mConfigParam.mPhotoNum = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_PHOTO_NUM, 0);
    mConfigParam.mPhotoInterval = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_PHOTO_INTERVAL, 0);
    mConfigParam.mPhotoPostviewEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_PHOTO_POSTVIEW_ENABLE, 0);
    mConfigParam.mPhotoWidth = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_PHOTO_WIDTH, 0);
    mConfigParam.mPhotoHeight = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_PHOTO_HEIGHT, 0);
    mConfigParam.mPhotoThumbWidth = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_PHOTO_THUMB_WIDTH, 0);
    mConfigParam.mPhotoThumbHeight = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_PHOTO_THUMB_HEIGHT, 0);

    mConfigParam.mEncodeChn = (UvcChn)GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_ENCODE_CHN, 0);
    ptr = const_cast<char *>(GetConfParaString(&stConfParser, SAMPLE_USBCAMERA_KEY_ENCODE_TYPE, NULL));
    if(!strcmp(ptr, "h264"))
    {
        mConfigParam.mEncodeType = PT_H264;   
    }
    else if(!strcmp(ptr, "h265"))
    {
        mConfigParam.mEncodeType = PT_H265;
    }
    else if(!strcmp(ptr, "mjpeg"))
    {
        mConfigParam.mEncodeType = PT_MJPEG;
    }
    else if(!strcmp(ptr, "\"\"") || !strcmp(ptr, "''") || !strcmp(ptr, ""))
    {
        alogd("encode type is empty string[%s], mean to not encode!", ptr);
        mConfigParam.mEncodeType = PT_BUTT;
    }
    else
    {
        aloge("fatal error, the encode type is error[%p], strlen[%d], [%s]!", ptr, strlen(ptr), ptr);
        mConfigParam.mEncodeType = PT_H264;
    }
    mConfigParam.mEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_ENCODE_WIDTH, 1280);
    mConfigParam.mEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_ENCODE_HEIGHT, 720);
    mConfigParam.mEncodeBitrate = 1024 * 1024 * GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_ENCODE_BITRATE, 14);
    mConfigParam.mDirPath = GetConfParaString(&stConfParser, SAMPLE_USBCAMERA_KEY_DIR_PATH, NULL);
    mConfigParam.mSegmentCounts = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_SEGMENT_COUNT, 3);
    int mVdecPicWidth;
    mConfigParam.mSegmentDuration = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_SEGMENT_DURATION, 50);

    mConfigParam.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_USBCAMERA_KEY_TEST_DURATION, 0);
    
    destroyConfParser(&stConfParser);

    alogd("param: dev[%s]", mConfigParam.mUSBCamDevName.c_str());
    alogd("captureSize:[%dx%d],fmt[0x%x],fps[%d]frmNum[%d]", mConfigParam.mCaptureWidth, mConfigParam.mCaptureHeight, mConfigParam.mPicFormat, mConfigParam.mFrameRate, mConfigParam.mFrameNum);
    alogd("vdec[%d],bufSize[%d],mainSize[%dx%d],fmt[0x%x]", mConfigParam.mVdecFlag, mConfigParam.mVdecBufSize, mConfigParam.mVdecPicWidth, mConfigParam.mVdecPicHeight, mConfigParam.mVdecOutputPixelFormat);
    alogd("subVdec[%d],subSize[%dx%d],subFmt[0x%x], extraFrmNum[%d]", mConfigParam.mVdecSubPicEnable, mConfigParam.mVdecSubPicWidth, mConfigParam.mVdecSubPicHeight, mConfigParam.mVdecSubOutputPixelFormat, mConfigParam.mVdecExtraFrameNum);
    alogd("dispChnNum[%d],size[%dx%d],rot[%d]", mConfigParam.mDispChnNum, mConfigParam.mDispWidth, mConfigParam.mDispHeight, mConfigParam.mPreviewRotation);
    alogd("photoChn[%d],num[%d],itl[%d],postview[%d],size[%dx%d],thumbSize[%dx%d]", mConfigParam.mPhotoChn, mConfigParam.mPhotoNum, 
        mConfigParam.mPhotoInterval, mConfigParam.mPhotoPostviewEnable, mConfigParam.mPhotoWidth, mConfigParam.mPhotoHeight,
        mConfigParam.mPhotoThumbWidth, mConfigParam.mPhotoThumbHeight);
    alogd("encodeType[0x%x],size[%dx%d],bitRate[%d],dirPath[%s],counts[%d],dur[%d],totalDur[%d]", mConfigParam.mEncodeType, 
        mConfigParam.mEncodeWidth, mConfigParam.mEncodeHeight, mConfigParam.mEncodeBitrate, mConfigParam.mDirPath.c_str(), 
        mConfigParam.mSegmentCounts, mConfigParam.mSegmentDuration, mConfigParam.mTestDuration);
    return 0;    
}

EyeseeLinux::status_t SampleUSBCamContext::CreateFolder(const std::string& strFolderPath)
{
    if(strFolderPath.empty())
    {
        aloge("jpeg path is not set!");
        return -1;
    }
    const char* pJpegFolderPath = strFolderPath.c_str();
    //check folder existence
    struct stat sb;
    if (stat(pJpegFolderPath, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pJpegFolderPath, sb.st_mode);
            return -1;
        }
    }
    //create folder if necessary
    int ret = mkdir(pJpegFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pJpegFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pJpegFolderPath);
        return -1;
    }
    
}

std::string SampleUSBCamContext::MakeSegmentFileName()
{
    char fileName[64];
    sprintf(fileName, "File[%04d].mp4", mFileNum++);
    return std::string(fileName);
}

int
main(int argc, char **argv)
{
    int result = 0;
    status_t ret;
    std::cout << "Hello, Sample_USBCamera" << std::endl;

    SampleUSBCamContext stContext;
    pSampleUSBCamContext = &stContext;

    if(stContext.ParseCmdLine(argc, argv) != 0)
    {
        result = -1;
        return result;
    }
     if(stContext.loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }
    if(stContext.CreateFolder(stContext.mConfigParam.mDirPath) != SUCCESS)
    {
        return -1;
    }

    if(signal(SIGINT, handle_exit) == SIG_ERR)
    {
        perror("can not catch SIGSEGV!");
    }
    
    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    stContext.mpUSBCamera = new EyeseeUSBCamera();
    stContext.mpUSBCamera->setErrorCallback(&stContext.mUSBCameraListener);
    stContext.mpUSBCamera->setInfoCallback(&stContext.mUSBCameraListener);

    stContext.mpUSBCamera->getUSBCameraCaptureParam(stContext.mUSBCamCapParam);
    stContext.mUSBCamCapParam.mpUsbCam_DevName = stContext.mConfigParam.mUSBCamDevName;
    stContext.mUSBCamCapParam.mUsbCam_CapPixelformat = stContext.mConfigParam.mPicFormat;
    stContext.mUSBCamCapParam.mUsbCam_CapWidth = stContext.mConfigParam.mCaptureWidth;
    stContext.mUSBCamCapParam.mUsbCam_CapHeight = stContext.mConfigParam.mCaptureHeight;
    stContext.mUSBCamCapParam.mUsbCam_CapFps = stContext.mConfigParam.mFrameRate;
    stContext.mUSBCamCapParam.mUsbCam_CapBufCnt = stContext.mConfigParam.mFrameNum;
    stContext.mpUSBCamera->setUSBCameraCaptureParam(stContext.mUSBCamCapParam);

    if(stContext.mConfigParam.mVdecFlag)
    {
        stContext.mpUSBCamera->getUSBCameraVdecParam(stContext.mUsbCamVdecParam);
        stContext.mUsbCamVdecParam.mUsbCam_VdecBufSize = stContext.mConfigParam.mVdecBufSize;
        stContext.mUsbCamVdecParam.mUsbCam_VdecPriority = stContext.mConfigParam.mVdecPriority;
        stContext.mUsbCamVdecParam.mUsbCam_VdecPicWidth = stContext.mConfigParam.mVdecPicWidth;
        stContext.mUsbCamVdecParam.mUsbCam_VdecPicHeight = stContext.mConfigParam.mVdecPicHeight;
        stContext.mUsbCamVdecParam.mUsbCam_VdecInitRotation = ROTATE_NONE;
        stContext.mUsbCamVdecParam.mUsbCam_VdecOutputPixelFormat = stContext.mConfigParam.mVdecOutputPixelFormat;
        stContext.mUsbCamVdecParam.mUsbCam_VdecSubPicEnable = stContext.mConfigParam.mVdecSubPicEnable;
        stContext.mUsbCamVdecParam.mUsbCam_VdecSubPicWidth = stContext.mConfigParam.mVdecSubPicWidth;
        stContext.mUsbCamVdecParam.mUsbCam_VdecSubPicHeight = stContext.mConfigParam.mVdecSubPicHeight;
        stContext.mUsbCamVdecParam.mUsbCam_VdecSubOutputPixelFormat = stContext.mConfigParam.mVdecSubOutputPixelFormat;
        stContext.mUsbCamVdecParam.mUsbCam_VdecExtraFrameNum = stContext.mConfigParam.mVdecExtraFrameNum;
        stContext.mpUSBCamera->setUSBCameraVdecParam(stContext.mUsbCamVdecParam);
    }

    // set vo chnnel
    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);
    VO_LAYER tHlay = 0;
    while(tHlay < VO_MAX_LAYER_NUM)
    {
        if(SUCCESS == AW_MPI_VO_EnableVideoLayer(tHlay))
        {
            break;
        }
        tHlay++;
    }
    if(tHlay >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
    }

    stContext.mVoLayer = tHlay;
    AW_MPI_VO_GetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 0;
    stContext.mLayerAttr.stDispRect.Width = stContext.mConfigParam.mDispWidth;
    stContext.mLayerAttr.stDispRect.Height = stContext.mConfigParam.mDispHeight;
    AW_MPI_VO_SetVideoLayerAttr(stContext.mVoLayer, &stContext.mLayerAttr);
    
    if(0 == stContext.mConfigParam.mVdecFlag)
    {
        stContext.mDisplayUvcChn = UvcChn::UvcMainChn;
    }
    else
    {
        if(0 == stContext.mConfigParam.mVdecSubPicEnable)
        {
            stContext.mDisplayUvcChn = UvcChn::VdecMainChn;
        }
        else
        {
            stContext.mDisplayUvcChn = UvcChn::VdecSubChn;
        }
    }

    ret = stContext.mpUSBCamera->prepareDevice();
    if(ret != NO_ERROR)
    {
        aloge("fatal error! prepare device fail[0x%x]", ret);
        goto _err0;
    }
    stContext.mpUSBCamera->startDevice();
    if(0 == stContext.mConfigParam.mVdecFlag)
    {
        stContext.mpUSBCamera->openChannel(UvcChn::UvcMainChn);
        CameraParameters chn0Param;
        stContext.mpUSBCamera->getParameters(UvcChn::UvcMainChn, chn0Param);
        chn0Param.setPreviewRotation(0);
        stContext.mpUSBCamera->setParameters(UvcChn::UvcMainChn, chn0Param);
        stContext.mpUSBCamera->prepareChannel(UvcChn::UvcMainChn);
        stContext.mDisplayUvcChn = UvcChn::UvcMainChn;
        stContext.mpUSBCamera->setChannelDisplay(stContext.mDisplayUvcChn, stContext.mVoLayer);
        stContext.mpUSBCamera->startChannel(UvcChn::UvcMainChn);
    }
    else
    {
        stContext.mpUSBCamera->openChannel(UvcChn::UvcMainChn);
        CameraParameters chn0Param;
        stContext.mpUSBCamera->getParameters(UvcChn::UvcMainChn, chn0Param);
        chn0Param.setPreviewRotation(0);
        stContext.mpUSBCamera->setParameters(UvcChn::UvcMainChn, chn0Param);
        stContext.mpUSBCamera->prepareChannel(UvcChn::UvcMainChn);

        stContext.mpUSBCamera->openChannel(UvcChn::VdecMainChn);
        CameraParameters chn1Param;
        stContext.mpUSBCamera->getParameters(UvcChn::VdecMainChn, chn1Param);
        chn1Param.setPreviewRotation(0);
        stContext.mpUSBCamera->setParameters(UvcChn::VdecMainChn, chn1Param);
        stContext.mpUSBCamera->prepareChannel(UvcChn::VdecMainChn);
        if(stContext.mConfigParam.mVdecSubPicEnable)
        {
            stContext.mpUSBCamera->openChannel(UvcChn::VdecSubChn);
            CameraParameters chn2Param;
            stContext.mpUSBCamera->getParameters(UvcChn::VdecSubChn, chn2Param);
            chn2Param.setPreviewRotation(0);
            stContext.mpUSBCamera->setParameters(UvcChn::VdecSubChn, chn2Param);
            stContext.mpUSBCamera->prepareChannel(UvcChn::VdecSubChn);
        }
        if(0 == stContext.mConfigParam.mVdecSubPicEnable)
        {
            stContext.mDisplayUvcChn = UvcChn::VdecMainChn;
        }
        else
        {
            stContext.mDisplayUvcChn = UvcChn::VdecSubChn;
        }
        stContext.mpUSBCamera->setChannelDisplay(stContext.mDisplayUvcChn, stContext.mVoLayer);

        stContext.mpUSBCamera->startChannel(UvcChn::UvcMainChn);
        stContext.mpUSBCamera->startChannel(UvcChn::VdecMainChn);
        if(stContext.mConfigParam.mVdecSubPicEnable)
        {
            stContext.mpUSBCamera->startChannel(UvcChn::VdecSubChn);
        }
    }
    if(stContext.mConfigParam.mEncodeType != PT_BUTT)
    {
        stContext.mpRecorder = new EyeseeRecorder();
        stContext.mpRecorder->setOnInfoListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setOnDataListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setOnErrorListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setCameraProxy(stContext.mpUSBCamera->getRecordingProxy(), (int)stContext.mConfigParam.mEncodeChn);
        stContext.mpRecorder->setVideoSource(EyeseeLinux::EyeseeRecorder::VideoSource::CAMERA);
        std::string strSegmentFilePath = stContext.mConfigParam.mDirPath + "/" + stContext.MakeSegmentFileName();
        stContext.mMuxerId = stContext.mpRecorder->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, const_cast<char *>(strSegmentFilePath.c_str()), 0, false); 
        stContext.mSegmentFiles.push_back(strSegmentFilePath);
        stContext.mpRecorder->setVideoFrameRate(stContext.mConfigParam.mFrameRate);
        stContext.mpRecorder->setVideoSize(stContext.mConfigParam.mEncodeWidth, stContext.mConfigParam.mEncodeHeight);
        stContext.mpRecorder->setVideoEncoder(stContext.mConfigParam.mEncodeType);
        stContext.mpRecorder->setVideoEncodingBitRate(stContext.mConfigParam.mEncodeBitrate);
        if(stContext.mConfigParam.mSegmentCounts > 0 && stContext.mConfigParam.mSegmentDuration > 0)
        {
            stContext.mpRecorder->setMaxDuration(stContext.mConfigParam.mSegmentDuration*1000);
        }
        stContext.mpRecorder->prepare();
        stContext.mpRecorder->start();
    }

    //test take picture
    if(stContext.mConfigParam.mPhotoNum > 0)
    {
        EyeseeUSBCamera::PictureCallback *pCbPostView = NULL;
        if(stContext.mConfigParam.mPhotoPostviewEnable)
        {
            pCbPostView = &stContext.mUSBCameraListener;
        }
        CameraParameters cameraParam;
        stContext.mpUSBCamera->getParameters(stContext.mConfigParam.mPhotoChn, cameraParam);
        if(1 == stContext.mConfigParam.mPhotoNum)
        {
            cameraParam.setPictureMode(TAKE_PICTURE_MODE_FAST);
        }
        else
        {
            cameraParam.setPictureMode(TAKE_PICTURE_MODE_CONTINUOUS);
            cameraParam.setContinuousPictureNumber(stContext.mConfigParam.mPhotoNum);
            cameraParam.setContinuousPictureIntervalMs(stContext.mConfigParam.mPhotoInterval);
        }
        cameraParam.setPictureSize(SIZE_S{(unsigned int)stContext.mConfigParam.mPhotoWidth, (unsigned int)stContext.mConfigParam.mPhotoHeight});
        cameraParam.setJpegQuality(90);
        cameraParam.setJpegThumbnailSize(SIZE_S{(unsigned int)stContext.mConfigParam.mPhotoThumbWidth, (unsigned int)stContext.mConfigParam.mPhotoThumbHeight});
        cameraParam.setJpegThumbnailQuality(60);
        stContext.mpUSBCamera->setParameters(stContext.mConfigParam.mPhotoChn, cameraParam);
        
        ret = stContext.mpUSBCamera->takePicture(stContext.mConfigParam.mPhotoChn, pCbPostView, &stContext.mUSBCameraListener, &stContext.mUSBCameraListener, NULL);
        if(NO_ERROR != ret)
        {
            aloge("fatal error! why take picture fail[0x%x]?", ret);
        }
    }
    
    if(stContext.mConfigParam.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, 1000 * stContext.mConfigParam.mTestDuration);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }
    if(stContext.mConfigParam.mEncodeType != PT_BUTT)
    {
        stContext.mpRecorder->stop();
        delete stContext.mpRecorder;
        stContext.mpRecorder = NULL;
    }
    //test startRender() and stopRender()
    if(stContext.mpUSBCamera->previewEnabled(stContext.mDisplayUvcChn))
    {
        alogd("test stopRender() and startRender()");
        stContext.mpUSBCamera->stopRender(stContext.mDisplayUvcChn);
        sleep(5);
        stContext.mpUSBCamera->startRender(stContext.mDisplayUvcChn);
        sleep(5);
    }

    stContext.mpUSBCamera->stopChannel(UvcChn::UvcMainChn);
    stContext.mpUSBCamera->stopChannel(UvcChn::VdecMainChn);
    stContext.mpUSBCamera->stopChannel(UvcChn::VdecSubChn);
    stContext.mpUSBCamera->releaseChannel(UvcChn::UvcMainChn);
    stContext.mpUSBCamera->releaseChannel(UvcChn::VdecMainChn);
    stContext.mpUSBCamera->releaseChannel(UvcChn::VdecSubChn);
    stContext.mpUSBCamera->closeChannel(UvcChn::UvcMainChn);
    stContext.mpUSBCamera->closeChannel(UvcChn::VdecMainChn);
    stContext.mpUSBCamera->closeChannel(UvcChn::VdecSubChn);
    stContext.mpUSBCamera->stopDevice();
    stContext.mpUSBCamera->releaseDevice();
    delete stContext.mpUSBCamera;
    stContext.mpUSBCamera = NULL;
    
    AW_MPI_VO_OpenVideoLayer(stContext.mUILayer); /* open ui layer. */
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    /* disalbe vo dev */
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;

    
    AW_MPI_SYS_Exit();

    std::cout << "bye, sample_USBCamera!" << std::endl;
    
    return result;

_err0:
    if(stContext.mpUSBCamera)
    {
        delete stContext.mpUSBCamera;
        stContext.mpUSBCamera = NULL;
    }
    AW_MPI_SYS_Exit();
    std::cout << "bye, sample_USBCamera!" << std::endl;
    return result;
}







