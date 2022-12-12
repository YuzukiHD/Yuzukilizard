//#define LOG_NDEBUG 0
#define LOG_TAG "sample_Camera"
#include <utils/plat_log.h>

#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>

#include <hwdisplay.h>
#include <confparser.h>
#include <mpi_sys.h>
#include <mpi_vi.h>
#include <mpi_vo.h>
#include <BITMAP_S.h>

#include "sample_Camera_config.h"
#include "sample_Camera.h"

// using ScalerOutChns 1 get Preview and JPEG.
#define  SAMPLE_CAMERA_SCALER_OUT_CHN_PREVIEW_JPEG    (0)

using namespace std;
using namespace EyeseeLinux;

static SampleCameraContext *pSampleCameraContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleCameraContext!=NULL)
    {
        cdx_sem_up(&pSampleCameraContext->mSemExit);
    }
}

bool EyeseeCameraCallback::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    if(PicType_Main != mPicType)
    {
        aloge("fatal error! picType[%d], check code!", mPicType);
    }
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId == mpContext->mScalerOutChns)
            {
                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mScalerOutChns);
            }
            break;
        }
        default:
        {
            aloge("fatal error! unknown info[0x%x] from channel[%d]", info, chnId);
            bHandleInfoFlag = false;
            break;
        }
    }
    return bHandleInfoFlag;
}

void EyeseeCameraCallback::onPictureTaken(int chnId, const void *data, int size, EyeseeCamera *pCamera)
{
    int ret = -1;
    alogd("channel %d picture data size %d", chnId, size);
    if(chnId != mpContext->mScalerOutChns)
    {
        aloge("fatal error! channel[%d] is not match current channel[%d]", chnId, mpContext->mScalerOutChns);
    }
    char picName[64];
    if(PicType_Main == mPicType)
    {
        sprintf(picName, "pic[%02d].jpg", mpContext->mPicNum++);
    }
    else
    {
        sprintf(picName, "pic[%02d]_thumb.jpg", mpContext->mPicNum++);
    }
    std::string PicFullPath = mpContext->mConfigPara.mJpegFolderPath + '/' + picName;
    FILE *fp = fopen(PicFullPath.c_str(), "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
}

EyeseeCameraCallback::EyeseeCameraCallback(SampleCameraContext *pContext)
    : mpContext(pContext)
{
    mPicType = PicType_Main;
}

SampleCameraContext::SampleCameraContext()
    :mCameraCallbacks(this), mCameraThumbPictureCallback(this)
{
	alogd("---------->SampleCameraContext construction!!\n");
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mVoDev = -1;
    mVoLayer = -1;
    mpCamera = NULL;
    mPicNum = 0;
    mCameraThumbPictureCallback.mPicType = EyeseeCameraCallback::PicType_Thumb;
    bzero(&mConfigPara,sizeof(mConfigPara));
    mScalerOutChns = -1;
}

SampleCameraContext::~SampleCameraContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    if(mpCamera!=NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
}

status_t SampleCameraContext::ParseCmdLine(int argc, char *argv[])
{
    alogd("this program path:[%s], arg number is [%d]", argv[0], argc);
    status_t ret = NO_ERROR;
    int i=1;
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                std::string errorString;
                errorString = "fatal error! use -h to learn how to set parameter!!!";
                cout<<errorString<<endl;
                ret = -1;
                break;
            }
            mCmdLinePara.mConfigFilePath = argv[i];
        }
        else if(!strcmp(argv[i], "-h"))
        {
            std::string helpString;
            helpString += "CmdLine param example:\n";
            helpString += "\t run -path /home/sample_Camera.conf\n";
            cout<<helpString<<endl;
            ret = 1;
            break;
        }
        else
        {
            std::string ignoreString;
            ignoreString += "ignore invalid CmdLine param:[";
            ignoreString += argv[i];
            ignoreString += ", type -h to get how to set parameter!";
            cout<<ignoreString<<endl;
        }
        i++;
    }
    return ret;
}

status_t SampleCameraContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 1920;
        mConfigPara.mCaptureHeight = 1080;
        mConfigPara.mViClock = 0;
        mConfigPara.mDigitalZoom = 0;
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mFrameNum = 5;
        mConfigPara.mFrameRate = 60;
        mConfigPara.mDisplayFrameRate = 0;
        mConfigPara.mDisplayWidth = 480;
        mConfigPara.mDisplayHeight = 640;
        mConfigPara.mPreviewRotation = 0;
        mConfigPara.nSwitchRenderTimes = 0;

        mConfigPara.mbAntishakeEnable = false;
        mConfigPara.mAntishakeInputBuffers = mConfigPara.mFrameNum;
        mConfigPara.mAntishakeOutputBuffers = 7;
        mConfigPara.mAntishakeOperationMode = 13;
        
        mConfigPara.mJpegWidth = 1920;
        mConfigPara.mJpegHeight = 1080;
        mConfigPara.mJpegQuality = 90;
        mConfigPara.mJpegThumbWidth = 320;
        mConfigPara.mJpegThumbHeight = 180;
        mConfigPara.mJpegThumbQuality = 80;
        mConfigPara.mbJpegThumbfileFlag = false;
        mConfigPara.mJpegNum = 0;
        mConfigPara.mJpegInterval = 500;
        mConfigPara.mJpegFolderPath = "/mnt/extsd/sample_Camera_Files";

        mConfigPara.jpeg_region_overlay_w = 0;
        mConfigPara.jpeg_region_overlay_h = 0;
        mConfigPara.jpeg_region_cover_w = 0;
        mConfigPara.jpeg_region_cover_h = 0;

        mConfigPara.mTestDuration = 0;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_CAPTURE_HEIGHT, 0);
    mConfigPara.mViClock = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_VI_CLOCK, 0);
    mConfigPara.mDigitalZoom = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_DIGITAL_ZOOM, 0);
    mConfigPara.mUserRegion.X = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_REGION_X, 0);
    mConfigPara.mUserRegion.Y = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_REGION_Y, 0);
    mConfigPara.mUserRegion.Width = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_REGION_WIDTH, 0);
    mConfigPara.mUserRegion.Height = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_REGION_HEIGHT, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_CAMERA_KEY_PIC_FORMAT, NULL);

    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv61"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
    }
    else if(!strcmp(pStrPixelFormat, "nv16"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mFrameNum = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_FRAME_NUM, 0); 
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_FRAME_RATE, 0); 
    mConfigPara.mDisplayFrameRate = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_DISPLAY_FRAME_RATE, 0); 
    mConfigPara.mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_DISPLAY_WIDTH, 0); 
    mConfigPara.mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_DISPLAY_HEIGHT, 0);
    
    //set mPreviewRotation
    int mPreviewMirror = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_PREVIEW_MIRROR, 0);
    mConfigPara.mPreviewRotation = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_PREVIEW_ROTATION, 0);
    mConfigPara.mPreviewRotation = (mPreviewMirror << 16) | mConfigPara.mPreviewRotation;

    mConfigPara.nSwitchRenderTimes = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_SWITCH_RENDER_TIMES, 0);
    mConfigPara.mbAntishakeEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_ANTISHAKE_ENABLE, 0);
    mConfigPara.mAntishakeInputBuffers = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_ANTISHAKE_INPUT_BUFFERS, 0);
    mConfigPara.mAntishakeOutputBuffers = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_ANTISHAKE_OUTPUT_BUFFERS, 0);
    mConfigPara.mAntishakeOperationMode = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_ANTISHAKE_OPERATION_MODE, 0);

    mConfigPara.mJpegWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_WIDTH, 0);
    mConfigPara.mJpegHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_HEIGHT, 0);
    mConfigPara.mJpegQuality = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_QUALITY, 90);
    mConfigPara.mJpegThumbWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_THUMB_WIDTH, 0);
    mConfigPara.mJpegThumbHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_THUMB_HEIGHT, 0);
    mConfigPara.mJpegThumbQuality = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_THUMB_QUALITY, 60);
    mConfigPara.mbJpegThumbfileFlag = (bool)GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_THUMBFILE_FLAG, 0);
    mConfigPara.mJpegNum = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_NUM, 0);
    mConfigPara.mJpegInterval = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_INTERVAL, 0);
    mConfigPara.mJpegFolderPath = GetConfParaString(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_FOLDER, NULL);

    mConfigPara.jpeg_region_overlay_x = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_OVERLAY_X, 0);
    mConfigPara.jpeg_region_overlay_y = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_OVERLAY_Y, 0);
    mConfigPara.jpeg_region_overlay_w = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_OVERLAY_W, 0);
    mConfigPara.jpeg_region_overlay_h = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_OVERLAY_H, 0);
    mConfigPara.jpeg_region_overlay_priority = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_OVERLAY_PRIORITY, 0);
    mConfigPara.jpeg_region_overlay_invert = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_OVERLAY_INVERT, 0);

    mConfigPara.jpeg_region_cover_x = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_COVER_X, 0);
    mConfigPara.jpeg_region_cover_y = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_COVER_Y, 0);
    mConfigPara.jpeg_region_cover_w = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_COVER_W, 0);
    mConfigPara.jpeg_region_cover_h = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_COVER_H, 0);
    mConfigPara.jpeg_region_cover_color = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_COVER_COLOR, 0);
    mConfigPara.jpeg_region_cover_priority = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_JPEG_REGION_COVER_PRIORITY, 0);
    
    OSDConfigPara stOSDConfigPara;
    char *pStrOsdType = (char*)GetConfParaString(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_TYPE, NULL); 
    if(!strcmp(pStrOsdType, "overlay"))
    {
        stOSDConfigPara.mOSDType = OVERLAY_RGN;
    }
    else if(!strcmp(pStrOsdType, "cover"))
    {
        stOSDConfigPara.mOSDType = COVER_RGN;
    }
    else
    {
        aloge("fatal error! OSD type is [%s]?", pStrOsdType);
        stOSDConfigPara.mOSDType = OVERLAY_RGN;
    }
    stOSDConfigPara.mOSDPriority = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_PRIORITY, 0);
    stOSDConfigPara.mOSDColor = GetConfParaUInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_COLOR, 0);
    stOSDConfigPara.mbOSDColorInvert = (bool)GetConfParaUInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_COLOR_INVERT, 0);
    stOSDConfigPara.mInvertLumThresh = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_INVERT_LUM_THRESH, 0);
    stOSDConfigPara.mInvertUnitWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_INVERT_UNIT_WIDTH, 0);
    stOSDConfigPara.mInvertUnitHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_INVERT_UNIT_HEIGHT, 0);
    stOSDConfigPara.mOSDRect.X = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_X, 0);
    stOSDConfigPara.mOSDRect.Y = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_Y, 0);
    stOSDConfigPara.mOSDRect.Width = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_W, 0);
    stOSDConfigPara.mOSDRect.Height = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD1_H, 0);
    if(stOSDConfigPara.mOSDRect.Width!=0 && stOSDConfigPara.mOSDRect.Height!=0)
    {
        mConfigPara.mOSDConfigParams.push_back(stOSDConfigPara);
    }
    
    pStrOsdType = (char*)GetConfParaString(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_TYPE, NULL); 
    if(!strcmp(pStrOsdType, "overlay"))
    {
        stOSDConfigPara.mOSDType = OVERLAY_RGN;
    }
    else if(!strcmp(pStrOsdType, "cover"))
    {
        stOSDConfigPara.mOSDType = COVER_RGN;
    }
    else
    {
        aloge("fatal error! OSD type is [%s]?", pStrOsdType);
        stOSDConfigPara.mOSDType = OVERLAY_RGN;
    }
    stOSDConfigPara.mOSDPriority = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_PRIORITY, 0);
    stOSDConfigPara.mOSDColor = GetConfParaUInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_COLOR, 0);
    stOSDConfigPara.mbOSDColorInvert = (bool)GetConfParaUInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_COLOR_INVERT, 0);
    stOSDConfigPara.mInvertLumThresh = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_INVERT_LUM_THRESH, 0);
    stOSDConfigPara.mInvertUnitWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_INVERT_UNIT_WIDTH, 0);
    stOSDConfigPara.mInvertUnitHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_INVERT_UNIT_HEIGHT, 0);
    stOSDConfigPara.mOSDRect.X = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_X, 0);
    stOSDConfigPara.mOSDRect.Y = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_Y, 0);
    stOSDConfigPara.mOSDRect.Width = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_W, 0);
    stOSDConfigPara.mOSDRect.Height = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD2_H, 0);
    if(stOSDConfigPara.mOSDRect.Width!=0 && stOSDConfigPara.mOSDRect.Height!=0)
    {
        mConfigPara.mOSDConfigParams.push_back(stOSDConfigPara);
    }

    pStrOsdType = (char*)GetConfParaString(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_TYPE, NULL); 
    if(!strcmp(pStrOsdType, "overlay"))
    {
        stOSDConfigPara.mOSDType = OVERLAY_RGN;
    }
    else if(!strcmp(pStrOsdType, "cover"))
    {
        stOSDConfigPara.mOSDType = COVER_RGN;
    }
    else
    {
        aloge("fatal error! OSD type is [%s]?", pStrOsdType);
        stOSDConfigPara.mOSDType = OVERLAY_RGN;
    }
    stOSDConfigPara.mOSDPriority = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_PRIORITY, 0);
    stOSDConfigPara.mOSDColor = GetConfParaUInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_COLOR, 0);
    stOSDConfigPara.mbOSDColorInvert = (bool)GetConfParaUInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_COLOR_INVERT, 0);
    stOSDConfigPara.mInvertLumThresh = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_INVERT_LUM_THRESH, 0);
    stOSDConfigPara.mInvertUnitWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_INVERT_UNIT_WIDTH, 0);
    stOSDConfigPara.mInvertUnitHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_INVERT_UNIT_HEIGHT, 0);
    stOSDConfigPara.mOSDRect.X = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_X, 0);
    stOSDConfigPara.mOSDRect.Y = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_Y, 0);
    stOSDConfigPara.mOSDRect.Width = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_W, 0);
    stOSDConfigPara.mOSDRect.Height = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_OSD3_H, 0);
    if(stOSDConfigPara.mOSDRect.Width!=0 && stOSDConfigPara.mOSDRect.Height!=0)
    {
        mConfigPara.mOSDConfigParams.push_back(stOSDConfigPara);
    }
    int i = 0;
    for(OSDConfigPara& elem : mConfigPara.mOSDConfigParams)
    {
        alogd("OSD[%d] type[0x%x] priority[%d] Color[0x%x]", i, elem.mOSDType, elem.mOSDPriority, elem.mOSDColor);
        i++;
    }

    mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_KEY_TEST_DURATION, 0);
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleCameraContext::CreateFolder(const std::string& strFolderPath)
{
    if(strFolderPath.empty())
    {
        aloge("jpeg path is not set!");
        return UNKNOWN_ERROR;
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
            return UNKNOWN_ERROR;
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
        return UNKNOWN_ERROR;
    }
}

static void FillBmpWithColor(BITMAP_S *pBitmap, int nColor)
{
    unsigned int *pPtRgb32 = (unsigned int*)pBitmap->mpData;
    unsigned short *pPtRgb1555 = (unsigned short*)pBitmap->mpData;
    if(MM_PIXEL_FORMAT_RGB_8888 == pBitmap->mPixelFormat)
    {
        int nARGB32 = nColor;
        int nDataSize = BITMAP_S_GetdataSize(pBitmap);
        int i;
        for(i=0;i<nDataSize;i+=4)
        {
            *pPtRgb32 = nARGB32;
            pPtRgb32++;
        }
    }
    else if(MM_PIXEL_FORMAT_RGB_1555 == pBitmap->mPixelFormat)
    {
        unsigned short nRGB1555 = nColor;
        int nDataSize = BITMAP_S_GetdataSize(pBitmap);
        int i;
        for(i=0;i<nDataSize;i+=2)
        {
            *pPtRgb1555 = nRGB1555;
            pPtRgb1555++;
        }
    }
    else
    {
        aloge("fatal error! not support other bmp pixel format[0x%x]", pBitmap->mPixelFormat);
    }
}

void SampleCameraPictureRegionCallback::addPictureRegion(std::list<PictureRegionType> &rPictureRegionList)
{
    rPictureRegionList.clear();

    if(pSampleCameraContext->mConfigPara.jpeg_region_overlay_w && pSampleCameraContext->mConfigPara.jpeg_region_overlay_h)
    {
        PictureRegionType Region1;
        Region1.mType = OVERLAY_RGN;

        int x = pSampleCameraContext->mConfigPara.jpeg_region_overlay_x;
        int y = pSampleCameraContext->mConfigPara.jpeg_region_overlay_y;
        unsigned int w = pSampleCameraContext->mConfigPara.jpeg_region_overlay_w;
        unsigned int h = pSampleCameraContext->mConfigPara.jpeg_region_overlay_h;
        Region1.mRect ={x, y, w, h};

        BITMAP_S stBmp;        
        int nSize = 0;
        memset(&stBmp, 0, sizeof(BITMAP_S));
        Region1.mInfo.mOverlay.mPixFmt = MM_PIXEL_FORMAT_RGB_8888;
        stBmp.mPixelFormat = Region1.mInfo.mOverlay.mPixFmt;
        stBmp.mWidth = w;
        stBmp.mHeight = h;
        nSize = BITMAP_S_GetdataSize(&stBmp);
        stBmp.mpData = malloc(nSize);
        if(NULL == stBmp.mpData)      
        {                        
            aloge("fatal error! malloc fail!");                
        }
        memset(stBmp.mpData, 0xFF, nSize);
        Region1.mInfo.mOverlay.mBitmap = stBmp.mpData;

        Region1.mInfo.mOverlay.mbInvColEn = pSampleCameraContext->mConfigPara.jpeg_region_overlay_invert;
        Region1.mInfo.mOverlay.mPriority = pSampleCameraContext->mConfigPara.jpeg_region_overlay_priority; 

        rPictureRegionList.push_back(Region1);
    }
    if(pSampleCameraContext->mConfigPara.jpeg_region_cover_w && pSampleCameraContext->mConfigPara.jpeg_region_cover_h)
    {
        PictureRegionType Region2;
        Region2.mType = COVER_RGN;

        int x = pSampleCameraContext->mConfigPara.jpeg_region_cover_x;
        int y = pSampleCameraContext->mConfigPara.jpeg_region_cover_y;
        unsigned int w = pSampleCameraContext->mConfigPara.jpeg_region_cover_w;
        unsigned int h = pSampleCameraContext->mConfigPara.jpeg_region_cover_h;

        Region2.mRect ={x, y, w, h};

        Region2.mInfo.mCover.mChromaKey = pSampleCameraContext->mConfigPara.jpeg_region_cover_color;
        Region2.mInfo.mCover.mPriority =  pSampleCameraContext->mConfigPara.jpeg_region_cover_priority;

        rPictureRegionList.push_back(Region2);
    }
}

//#define Region_debug
int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_Camera!"<<endl;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 1,
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
    
    SampleCameraContext *pContext = new SampleCameraContext;
    pSampleCameraContext = pContext;
    //parse command line param
    if(pContext->ParseCmdLine(argc, argv) != NO_ERROR)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        return result;
    }
    //parse config file.
    if(pContext->loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }
    //check folder existence, create folder if necessary
    if(pContext->mConfigPara.mJpegNum > 0)
    {
        if(SUCCESS != pContext->CreateFolder(pContext->mConfigPara.mJpegFolderPath))
        {
            return -1;
        }
    }
    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");
    //init mpp system
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    if(pContext->mConfigPara.mViClock != 0)
    {
        alogd("set vi clock to [%d]MHz", pContext->mConfigPara.mViClock);
        AW_MPI_VI_SetVIFreq(0, pContext->mConfigPara.mViClock*1000000);
    }
    //AW_MPI_VENC_SetVEFreq(MM_INVALID_CHN, 632);
    pContext->mVoDev = 0;
    AW_MPI_VO_Enable(pContext->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);//close ui layer.
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(pContext->mVoDev, &spPubAttr);
    spPubAttr.enIntfType = VO_INTF_LCD;
    spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(pContext->mVoDev, &spPubAttr);

    //config camera.
    {
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
        CameraInfo cameraInfo;
        VI_CHN chn;
        ISPGeometry tmpISPGeometry;
        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 0;
        tmpISPGeometry.mISPDev = 0;
        tmpISPGeometry.mScalerOutChns.clear();
        tmpISPGeometry.mScalerOutChns.push_back(0);
        tmpISPGeometry.mScalerOutChns.push_back(1);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(tmpISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

    bool bOSDFlag = false;
    int cameraId;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &pContext->mCameraInfo);
	alogd("---EyeseeCamera::open->\n\n");
    pContext->mpCamera = EyeseeCamera::open(cameraId);//EyeseeCamera 构造函数
    pContext->mpCamera->setInfoCallback(&pContext->mCameraCallbacks);
    pContext->mpCamera->prepareDevice(); 
    pContext->mpCamera->startDevice();
    pContext->mScalerOutChns = pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];

    pContext->mpCamera->openChannel(pContext->mScalerOutChns, true);//VIChannel 构造函数 -->各种thread构造函数

    CameraParameters cameraParam;
    memset(&cameraParam, 0, sizeof(CameraParameters));
    pContext->mpCamera->getParameters(pContext->mScalerOutChns, cameraParam);
    SIZE_S captureSize={(unsigned int)pContext->mConfigPara.mCaptureWidth, (unsigned int)pContext->mConfigPara.mCaptureHeight};
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(pContext->mConfigPara.mFrameRate);
    cameraParam.setDisplayFrameRate(pContext->mConfigPara.mDisplayFrameRate);
    cameraParam.setPreviewFormat(pContext->mConfigPara.mPicFormat);
    cameraParam.setVideoBufferNumber(pContext->mConfigPara.mFrameNum); 
    cameraParam.setPreviewRotation(pContext->mConfigPara.mPreviewRotation);

    //cameraParam.setUserRegion(pContext->mConfigPara.mUserRegion);
    cameraParam.setB4G2dUserRegion(pContext->mConfigPara.mUserRegion);

    //test antishake
    //CameraParameters::AntishakeParam stAntishakeParam = {pContext->mConfigPara.mbAntishakeEnable, pContext->mConfigPara.mAntishakeInputBuffers, pContext->mConfigPara.mAntishakeOutputBuffers, pContext->mConfigPara.mAntishakeOperationMode};
    //cameraParam.setAntishakeParam(stAntishakeParam);
    pContext->mpCamera->setParameters(pContext->mScalerOutChns, cameraParam);//AW_MPI_VI_SetVippAttr
    pContext->mpCamera->prepareChannel(pContext->mScalerOutChns);//AW_MPI_VI_CreateVipp
    VO_LAYER hlay = 0;
    while(hlay < VO_MAX_LAYER_NUM)
    {
        if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay))
        {
            break;
        }
        hlay++;
    }
    if(hlay >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
    }
    AW_MPI_VO_GetVideoLayerAttr(hlay, &pContext->mLayerAttr);
    pContext->mLayerAttr.stDispRect.X = 0;
    pContext->mLayerAttr.stDispRect.Y = 0;
    pContext->mLayerAttr.stDispRect.Width = pContext->mConfigPara.mDisplayWidth;
    pContext->mLayerAttr.stDispRect.Height = pContext->mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &pContext->mLayerAttr);
    pContext->mVoLayer = hlay;
    //camera preview test
    alogd("prepare setPreviewDisplay(), hlay=%d", pContext->mVoLayer);
    pContext->mpCamera->setChannelDisplay(pContext->mScalerOutChns, pContext->mVoLayer);
    pContext->mpCamera->startChannel(pContext->mScalerOutChns);
	/*
	AW_MPI_VI_EnableVipp 
	AW_MPI_VI_CreateVirChn  
	AW_MPI_VI_EnableVirChn 
	AW_MPI_VO_CreateChn  
	AW_MPI_VO_StartChn
	AW_MPI_ISP_Run
	*/
    pContext->mpCamera->startRender(pContext->mScalerOutChns);
    //test take picture
    if(1 == pContext->mConfigPara.mJpegNum)
    {
        sleep(2);
        pContext->mpCamera->getParameters(pContext->mScalerOutChns, cameraParam);
        cameraParam.setPictureMode(TAKE_PICTURE_MODE_FAST);
        cameraParam.setPictureSize(SIZE_S{(unsigned int)pContext->mConfigPara.mJpegWidth, (unsigned int)pContext->mConfigPara.mJpegHeight});
        cameraParam.setJpegQuality(pContext->mConfigPara.mJpegQuality);
        cameraParam.setJpegThumbnailSize(SIZE_S{(unsigned int)pContext->mConfigPara.mJpegThumbWidth, (unsigned int)pContext->mConfigPara.mJpegThumbHeight});
        cameraParam.setJpegThumbnailQuality(pContext->mConfigPara.mJpegThumbQuality);
        pContext->mpCamera->setParameters(pContext->mScalerOutChns, cameraParam);
        if(pContext->mConfigPara.mbJpegThumbfileFlag)
        {
            pContext->mpCamera->takePicture(pContext->mScalerOutChns, NULL, NULL, &pContext->mCameraThumbPictureCallback, &pContext->mCameraCallbacks, &pContext->mSampleCameraPictureRegionCallback);
        }
        else
        {
            pContext->mpCamera->takePicture(pContext->mScalerOutChns, NULL, NULL, NULL, &pContext->mCameraCallbacks, &pContext->mSampleCameraPictureRegionCallback);
        }
    }
    else if(pContext->mConfigPara.mJpegNum > 1)
    {
        sleep(2);
        pContext->mpCamera->getParameters(pContext->mScalerOutChns, cameraParam);
        cameraParam.setPictureMode(TAKE_PICTURE_MODE_CONTINUOUS);
        cameraParam.setContinuousPictureNumber(pContext->mConfigPara.mJpegNum);
        cameraParam.setContinuousPictureIntervalMs(pContext->mConfigPara.mJpegInterval);
        cameraParam.setPictureSize(SIZE_S{(unsigned int)pContext->mConfigPara.mJpegWidth, (unsigned int)pContext->mConfigPara.mJpegHeight});
        cameraParam.setJpegThumbnailSize(SIZE_S{(unsigned int)pContext->mConfigPara.mJpegThumbWidth, (unsigned int)pContext->mConfigPara.mJpegThumbHeight});
        cameraParam.setJpegThumbnailQuality(pContext->mConfigPara.mJpegThumbQuality);
        pContext->mpCamera->setParameters(pContext->mScalerOutChns, cameraParam);
        if(pContext->mConfigPara.mbJpegThumbfileFlag)
        {
            pContext->mpCamera->takePicture(pContext->mScalerOutChns, NULL, NULL, &pContext->mCameraThumbPictureCallback, &pContext->mCameraCallbacks, &pContext->mSampleCameraPictureRegionCallback);
        }
        else
        {
            pContext->mpCamera->takePicture(pContext->mScalerOutChns, NULL, NULL, NULL, &pContext->mCameraCallbacks, &pContext->mSampleCameraPictureRegionCallback);
        }
    }

#ifdef Region_debug
    char str[256]  = {0};
    char val[16] = {0};
    int num = 0, vl = 0;
    RECT_S get_test_rect;
    memset(&get_test_rect, 0, sizeof(RECT_S));

    RECT_S test_rect;
    memset(&test_rect, 0, sizeof(RECT_S));
    alogd("\n===Test Preview Region!!===");

    while (1)
    {
        memset(str, 0, sizeof(str));
        memset(val,0,sizeof(val));
        fgets(str, 256, stdin);
        num = atoi(str);
        if (99 == num) {
            printf("break test.\n");
            printf("enter ctrl+c to exit this program.\n");
            break;
        }
        switch (num) {;
            case 1:
                alogd("\n\n---Please enter the display setB4G2dUserRegion region:[X,Y,W,H]---:\n\n");
                scanf("%d,%d,%d,%d",&test_rect.X,&test_rect.Y,&test_rect.Width,&test_rect.Height);
                alogd("\n\nX:%d  Y:%d  W:%d  H:%d\n\n",test_rect.X, test_rect.Y,test_rect.Width,test_rect.Height);
                pContext->mConfigPara.mUserRegion.X = test_rect.X;
                pContext->mConfigPara.mUserRegion.Y = test_rect.Y;
                pContext->mConfigPara.mUserRegion.Width = test_rect.Width;
                pContext->mConfigPara.mUserRegion.Height = test_rect.Height;
                pContext->mpCamera->getParameters(pContext->mScalerOutChns, cameraParam);
                cameraParam.setB4G2dUserRegion(pContext->mConfigPara.mUserRegion);
                pContext->mpCamera->setParameters(pContext->mScalerOutChns, cameraParam);
                break;
            case 2:
                alogd("\n\n---Please enter the display setUserRegion region:[X,Y,W,H]---:\n\n");
                scanf("%d,%d,%d,%d",&test_rect.X,&test_rect.Y,&test_rect.Width,&test_rect.Height);
                alogd("\n\nX:%d  Y:%d  W:%d  H:%d\n\n",test_rect.X, test_rect.Y,test_rect.Width,test_rect.Height);
                pContext->mConfigPara.mUserRegion.X = test_rect.X;
                pContext->mConfigPara.mUserRegion.Y = test_rect.Y;
                pContext->mConfigPara.mUserRegion.Width = test_rect.Width;
                pContext->mConfigPara.mUserRegion.Height = test_rect.Height;
                pContext->mpCamera->getParameters(pContext->mScalerOutChns, cameraParam);
                cameraParam.setUserRegion(pContext->mConfigPara.mUserRegion);
                pContext->mpCamera->setParameters(pContext->mScalerOutChns, cameraParam);
                break;
            default:
                printf("intput error.\r\n");
                break;
        }
    }
#endif

    int ret;
    ret = cdx_sem_down_timedwait(&pContext->mSemRenderStart, 5000);
    if(0 == ret)
    {
        alogd("app receive message that camera start render!");
    }
    else if(ETIMEDOUT == ret)
    {
        aloge("fatal error! wait render start timeout");
    }
    else
    {
        aloge("fatal error! other error[0x%x]", ret);
    }

    if(pContext->mConfigPara.mDigitalZoom > 0)
    {
        ret = cdx_sem_down_timedwait(&pContext->mSemExit, 5*1000);
        if(0 == ret)
        {
            alogd("user want to exit!");
            goto _exit0;
        }
        pContext->mpCamera->getParameters(pContext->mScalerOutChns, cameraParam);
        int oldZoom = cameraParam.getZoom();
        cameraParam.setZoom(pContext->mConfigPara.mDigitalZoom);
        pContext->mpCamera->setParameters(pContext->mScalerOutChns, cameraParam);
        alogd("change digital zoom[%d]->[%d]", oldZoom, pContext->mConfigPara.mDigitalZoom);
    }

    for(int i=0; i<pContext->mConfigPara.nSwitchRenderTimes; i++)
    {
        ret = cdx_sem_down_timedwait(&pContext->mSemExit, 4*1000);
        if(0 == ret)
        {
            alogd("user want to exit!");
            goto _exit0;
        }
        pContext->mpCamera->stopRender(pContext->mScalerOutChns);
        alogd("stop render");
        ret = cdx_sem_down_timedwait(&pContext->mSemExit, 1*1000);
        if(0 == ret)
        {
            alogd("user want to exit!");
            goto _exit0;
        }
        pContext->mpCamera->startRender(pContext->mScalerOutChns);
        alogd("switch render, test[%d] done", i+1);
    }

    //test overlay and cover
    for(OSDConfigPara& elem : pContext->mConfigPara.mOSDConfigParams)
    {
        if(elem.mOSDRect.Width!=0 && elem.mOSDRect.Height!=0)
        {
            bOSDFlag = true;
            RGN_ATTR_S stRegion;
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            if(OVERLAY_RGN == elem.mOSDType)
            {
                stRegion.enType = elem.mOSDType;
                stRegion.unAttr.stOverlay.mPixelFmt = MM_PIXEL_FORMAT_RGB_8888;
                stRegion.unAttr.stOverlay.mSize = {(unsigned int)elem.mOSDRect.Width, (unsigned int)elem.mOSDRect.Height};
                RGN_HANDLE nOverlayHandle = pContext->mpCamera->createRegion(&stRegion);
                pContext->mRgnHandleList.push_back(nOverlayHandle);
                BITMAP_S stBmp;
                memset(&stBmp, 0, sizeof(BITMAP_S));
                stBmp.mPixelFormat = stRegion.unAttr.stOverlay.mPixelFmt;
                stBmp.mWidth = stRegion.unAttr.stOverlay.mSize.Width;
                stBmp.mHeight = stRegion.unAttr.stOverlay.mSize.Height;
                int nSize = BITMAP_S_GetdataSize(&stBmp);
                stBmp.mpData = malloc(nSize);
                if(NULL == stBmp.mpData)
                {
                    aloge("fatal error! malloc fail!");
                }
                //memset(stBmp.mpData, 0x80, nSize);
                FillBmpWithColor(&stBmp, elem.mOSDColor);
                pContext->mpCamera->setRegionBitmap(nOverlayHandle, &stBmp);
                free(stBmp.mpData);
                RGN_CHN_ATTR_S stRgnChnAttr = {0};
                stRgnChnAttr.bShow = TRUE;
                stRgnChnAttr.enType = stRegion.enType;
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint = {elem.mOSDRect.X, elem.mOSDRect.Y};
                stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = elem.mOSDPriority;
                stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = (BOOL)elem.mbOSDColorInvert;
                stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = elem.mInvertLumThresh;
                stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUMDIFF_THRESH;
                stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = elem.mInvertUnitWidth;
                stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = elem.mInvertUnitHeight;
                pContext->mpCamera->attachRegionToChannel(nOverlayHandle, pContext->mScalerOutChns, &stRgnChnAttr);
            }
            else if(COVER_RGN == elem.mOSDType)
            {
                stRegion.enType = elem.mOSDType;
                RGN_HANDLE nCoverHandle = pContext->mpCamera->createRegion(&stRegion);
                pContext->mRgnHandleList.push_back(nCoverHandle);
                RGN_CHN_ATTR_S stRgnChnAttr = {0};
                stRgnChnAttr.bShow = TRUE;
                stRgnChnAttr.enType = stRegion.enType;
                stRgnChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect = {elem.mOSDRect.X, elem.mOSDRect.Y, (unsigned int)elem.mOSDRect.Width, (unsigned int)elem.mOSDRect.Height};
                stRgnChnAttr.unChnAttr.stCoverChn.mColor = elem.mOSDColor;
                stRgnChnAttr.unChnAttr.stCoverChn.mLayer = elem.mOSDPriority;
                pContext->mpCamera->attachRegionToChannel(nCoverHandle, pContext->mScalerOutChns, &stRgnChnAttr);
            }
            else
            {
                aloge("fatal error!");
            }
        }
        else
        {
            aloge("fatal error! Why WxH[%dx%d]", elem.mOSDRect.Width, elem.mOSDRect.Height);
        }
    }
    
    if(pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }
    if(bOSDFlag)
    {
        for(RGN_HANDLE& handle : pContext->mRgnHandleList)
        {
            RGN_CHN_ATTR_S stRgnChnAttr;
            pContext->mpCamera->getRegionDisplayAttr(handle, pContext->mScalerOutChns, &stRgnChnAttr);
            if(OVERLAY_RGN == stRgnChnAttr.enType)
            {
                //try to change overlay display position
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X += 16;
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y += 16;
            }
            else if(COVER_RGN == stRgnChnAttr.enType)
            {
                //try to change cover display position, cover can even change size!
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.X += 10;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y += 10;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width += 10;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height += 10;
            }
            else
            {
                aloge("fatal error!");
            }
            pContext->mpCamera->setRegionDisplayAttr(handle, pContext->mScalerOutChns, &stRgnChnAttr);
        }
        alogd("change osd to watch 5s!");
        sleep(1);
        //detach region from channel.
        for(RGN_HANDLE& handle : pContext->mRgnHandleList)
        {
            pContext->mpCamera->detachRegionFromChannel(handle, pContext->mScalerOutChns);
        }
        alogd("detach osd to watch 5s!");
        sleep(1);
        //destroy region
        for(RGN_HANDLE& handle : pContext->mRgnHandleList)
        {
            pContext->mpCamera->destroyRegion(handle);
            handle = MM_INVALID_HANDLE;
        }
    }
_exit0:
    //close camera
    alogd("HerbCamera::release()");
    alogd("HerbCamera stopPreview()");
    pContext->mpCamera->stopRender(pContext->mScalerOutChns);
    pContext->mpCamera->stopChannel(pContext->mScalerOutChns);
    pContext->mpCamera->releaseChannel(pContext->mScalerOutChns);
    pContext->mpCamera->closeChannel(pContext->mScalerOutChns);
    pContext->mpCamera->stopDevice(); 
    pContext->mpCamera->releaseDevice(); 
    EyeseeCamera::close(pContext->mpCamera);
    pContext->mpCamera = NULL;
    //close vo
    AW_MPI_VO_DisableVideoLayer(pContext->mVoLayer);
    pContext->mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_Disable(pContext->mVoDev);
    pContext->mVoDev = -1;

	//exit mpp system
    AW_MPI_SYS_Exit(); 

    delete pContext;
    pContext = NULL;
    log_quit();
    cout<<"bye, sample_Camera!"<<endl;
    return result;
}

