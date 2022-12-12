//#define LOG_NDEBUG 0
#define LOG_TAG "sample_EyeseeIse"
#include <utils/plat_log.h>

#include <iostream>
#include <csignal>
#include <sys/time.h>
#include <dlfcn.h>

#include <hwdisplay.h>
#include <record_writer.h>
#include <confparser.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>
#include <EyeseeISE.h>
#include <SystemBase.h>
#include <BITMAP_S.h>
#include "sample_EyeseeIse.h"

using namespace std;
using namespace EyeseeLinux;

SampleEyeseeIseContext *gpSampleContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleContext != NULL)
    {
        cdx_sem_up(&gpSampleContext->mSemExit);
    }
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId==mpContext->mCamDispChnId[0] || chnId==mpContext->mCamDispChnId[1])
            {
                alogd("CamDispChnId(%d) notify app preview!", chnId);
            }
            else
            {
                aloge("fatal error! channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mCamDispChnId[0]);
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

void EyeseeCameraListener::onPictureTaken(int chnId, const void *data, int size, EyeseeCamera *pCamera)
{
    int camId;
    SampleEyeseeIseConfig *pConf = &mpContext->mConfigParam;
    PhotoParam *pPhotoParam;
    BOOL findFlag = FALSE;

    for (camId=0; camId<2; camId++)
    {
        if (pConf->mCamPhotoEnable[camId])
        {
            pPhotoParam = &pConf->mCamPhotoParam[camId];
            int wantedChnId = pPhotoParam->mCamChnId;
            EyeseeCamera *pWantedCamera = mpContext->mpCamera[camId];
            if (wantedChnId != chnId || pWantedCamera != pCamera)
            {
                alogw("wantedChnId(%d) != chnId(%d) || pWantedCamera(%p) != pCamera(%d), maybe next cam!",
                    wantedChnId, chnId, pWantedCamera, pCamera);
            }
            else
            {
                findFlag = TRUE;
                char picPath[128] = {0};
                sprintf(picPath, "%s/Cam%dChnId%d_%02d.jpg", pPhotoParam->mPicPath, camId, chnId, pPhotoParam->mPicNumId++);
                alogd("cam(%d) chnId(%d) callback jpeg with size(%d)!", camId, chnId, size);
                alogd("picture path: %s", picPath);
                FILE *fp = fopen(picPath, "wb");
                if (fp == NULL)
                {
                    aloge("open file error!");
                    return;
                }
                else
                {
                    fwrite(data, 1, size, fp);
                    fclose(fp);
                    return;
                }
            }
        }
    }

    if (findFlag != TRUE)
    {
        aloge("Why not find cam chn to take picture?! check this chnId=%d", chnId);
    }
}

void EyeseeCameraListener::addPictureRegion(std::list<PictureRegionType> &rPicRgnList)
{
    rPicRgnList.clear();

    SampleEyeseeIseConfig *pConf = &mpContext->mConfigParam;
    int regionId;
    if (pConf->mCamRgnEnable[0] == 1)
    {
        regionId = 0;
    }
    else if (pConf->mCamRgnEnable[1] == 1)
    {
        regionId = 1;
    }
    else
    {
        alogw("make sure whether you want to add region for jpeg! this time do not add region! en_val(%d, %d)",
            pConf->mCamRgnEnable[0], pConf->mCamRgnEnable[1]);
        return;
    }
    RegionParam *pRegionParam = &pConf->mCamRgnParam[regionId];

    if (pRegionParam->mRgnOvlyEnable == 1)
    {
        PictureRegionType picRgn;
        picRgn.mType = OVERLAY_RGN;

        int x = pRegionParam->mRgnOvlyX;
        int y = pRegionParam->mRgnOvlyY;
        unsigned int w = pRegionParam->mRgnOvlyW;
        unsigned int h = pRegionParam->mRgnOvlyH;
        picRgn.mRect = {x, y, w, h};

        BITMAP_S stBmp;
        int nSize = 0;
        bzero(&stBmp, sizeof(stBmp));
        if (pRegionParam->mRgnOvlyPixFmt == 0)
        {
            picRgn.mInfo.mOverlay.mPixFmt = MM_PIXEL_FORMAT_RGB_8888;
            picRgn.mInfo.mOverlay.mbInvColEn = pRegionParam->mRgnOvlyInvCorEn;
            picRgn.mInfo.mOverlay.mPriority = pRegionParam->mRgnOvlyPri;

            stBmp.mWidth = w;
            stBmp.mHeight = h;
            stBmp.mPixelFormat = picRgn.mInfo.mOverlay.mPixFmt;
            nSize = BITMAP_S_GetdataSize(&stBmp);
            picRgn.mInfo.mOverlay.mBitmap = malloc(nSize);
            int *pBmp = (int *)picRgn.mInfo.mOverlay.mBitmap;
            int i;
            for (i=0; i<nSize/4; i++)
            {
                pBmp[i] = pRegionParam->mRgnOvlyCorVal;
            }
            rPicRgnList.push_back(picRgn);
        }
        else if (pRegionParam->mRgnOvlyPixFmt == 1)
        {
            picRgn.mInfo.mOverlay.mPixFmt = MM_PIXEL_FORMAT_RGB_1555;
            picRgn.mInfo.mOverlay.mbInvColEn = pRegionParam->mRgnOvlyInvCorEn;
            picRgn.mInfo.mOverlay.mPriority = pRegionParam->mRgnOvlyPri;

            stBmp.mWidth = w;
            stBmp.mHeight = h;
            stBmp.mPixelFormat = picRgn.mInfo.mOverlay.mPixFmt;
            nSize = BITMAP_S_GetdataSize(&stBmp);
            picRgn.mInfo.mOverlay.mBitmap = malloc(nSize);
            short *pBMP = (short *)picRgn.mInfo.mOverlay.mBitmap;
            int i;
            for (i=0; i<nSize/2; i++)
            {
                pBMP[i] = (short)pRegionParam->mRgnOvlyCorVal;
            }
            rPicRgnList.push_back(picRgn);
        }
        else
        {
            aloge("not support region overlay pixel format=%d", pRegionParam->mRgnOvlyPixFmt);
            return;
        }
    }

    if (pRegionParam->mRgnCovEnable == 1)
    {
        PictureRegionType picRgn;
        picRgn.mType = COVER_RGN;

        int x = pRegionParam->mRgnCovX;
        int y = pRegionParam->mRgnCovY;
        unsigned int w = pRegionParam->mRgnCovW;
        unsigned int h = pRegionParam->mRgnCovH;
        picRgn.mRect = {x, y, w, h};

        picRgn.mInfo.mCover.mChromaKey = pRegionParam->mRgnCovCor;
        picRgn.mInfo.mCover.mPriority = pRegionParam->mRgnCovPri;
        rPicRgnList.push_back(picRgn);
    }
}

EyeseeCameraListener::EyeseeCameraListener(SampleEyeseeIseContext *pContext)
    : mpContext(pContext)
{
}

void EyeseeRecorderListener::onError(EyeseeRecorder *pMr, int what, int extra)
{
    alogd("receive onError message![%d][%d]", what, extra);
    switch(what)
    {
        case EyeseeRecorder::MEDIA_ERROR_SERVER_DIED:
            break;
        default:
            break;
    }
}

void EyeseeRecorderListener::onInfo(EyeseeRecorder *pMr, int what, int extra)
{
    switch(what)
    {
        case EyeseeRecorder::MEDIA_RECORDER_INFO_NEED_SET_NEXT_FD:
        {
            alogd("receive onInfo message: need_set_next_fd, muxer_id[%d]", extra);
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            alogd("receive onInfo message: record_file_done, muxer_id[%d]", extra);
            break;
        }
        default:
        {
            alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);
            break;
        }
    }
}

void EyeseeRecorderListener::onData(EyeseeRecorder *pMr, int what, int extra)
{
    aloge("fatal error! Do not test CallbackOut!");
}



EyeseeRecorderListener::EyeseeRecorderListener(SampleEyeseeIseContext *pOwner)
    : mpContext(pOwner)
{
}

EyeseeIseListener::EyeseeIseListener(SampleEyeseeIseContext *pContext)
    :mpContext(pContext)
{
}

void EyeseeIseListener::onError(int chnId, int error, EyeseeLinux::EyeseeISE *pISE)
{
    aloge("ise receive error, but not register error handle!!! chnId[%d], error[%d]", chnId, error);
}

bool EyeseeIseListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeISE *pISE)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if (mpContext->mConfigParam.mIseMode == 1)
            {
                if (mpContext->mConfigParam.mIseMoParam.mIseMoType == 3)
                {
                    alogd("ise mo-ptz mode, IseChnId(%d) notify app preview!", chnId);
                }
                else
                {
                    if (chnId==mpContext->mIseChnId[0][chnId])
                    {
                        alogd("ise mo-other mode, IseChnId(%d) notify app preview!", chnId);
                    }
                    else
                    {
                        bHandleInfoFlag = false;
                        aloge("ise mo-other mode, unknown IseChnId(%d) notify app preview!!!", chnId);
                    }
                }
            }
            else if ((mpContext->mConfigParam.mIseMode == 2) || (mpContext->mConfigParam.mIseMode == 3))
            {
                if (chnId==mpContext->mIseChnId[0][chnId])
                {
                    alogd("ise mo/sti type(%d), IseChnId(%d) notify app preview!", mpContext->mConfigParam.mIseMode, chnId);
                }
                else
                {
                    bHandleInfoFlag = false;
                    aloge("ise mo/sti type(%d), unknown IseChnId(%d) notify app preview!!!", mpContext->mConfigParam.mIseMode, chnId);
                }
            }
            else
            {
                bHandleInfoFlag = false;
                aloge("fatal error! unknown IseChnId[%d] notify render start!", chnId);
            }
            break;
        }
        default:
        {
            bHandleInfoFlag = false;
            aloge("fatal error! unknown info[0x%x] from IseChnId[%d]", info, chnId);
            break;
        }
    }
    return bHandleInfoFlag;
}

void EyeseeIseListener::onPictureTaken(int chnId, const void *data, int size, EyeseeISE* pISE)
{
    int tmpChnId;
    SampleEyeseeIseConfig *pConf = &mpContext->mConfigParam;
    PhotoParam *pPhotoParam;
    BOOL findFlag = FALSE;

    for (tmpChnId=0; tmpChnId<4; tmpChnId++)
    {
        if (tmpChnId == chnId && pConf->mIseMoParam.mIseMoOutEn[tmpChnId] && pConf->mIseMoPhotoEnable[tmpChnId])
        {
            findFlag = TRUE;
            pPhotoParam = &pConf->mIseMoPhotoParam[chnId];
            char picPath[128] = {0};
            sprintf(picPath, "%s/IseMoChnId%d_%02d.jpg", pPhotoParam->mPicPath, chnId, pPhotoParam->mPicNumId++);
            alogd("Ise-Mo chnId(%d) callback jpeg with size(%d)!", chnId, size);
            alogd("picture path: %s", picPath);
            FILE *fp = fopen(picPath, "wb");
            if (fp == NULL)
            {
                aloge("open file error!");
                return;
            }
            else
            {
                fwrite(data, 1, size, fp);
                fclose(fp);
                return;
            }
        }
    }

    if (findFlag != TRUE)
    {
        aloge("Why not find ise-mo chn to take picture?! check this chnId=%d", chnId);
    }
}

void EyeseeIseListener::addPictureRegion(std::list<PictureRegionType> &rPicRgnList)
{
    rPicRgnList.clear();

    SampleEyeseeIseConfig *pConf = &mpContext->mConfigParam;
    int regionId;
    if (pConf->mIseMoRgnEnable[0] == 1)
    {
        regionId = 0;
    }
    else
    {
        alogw("make sure whether you want to add region for jpeg! this time do not add region!");
        return;
    }
    RegionParam *pRegionParam = &pConf->mIseMoRgnParam[regionId];

    if (pRegionParam->mRgnOvlyEnable == 1)
    {
        PictureRegionType picRgn;
        picRgn.mType = OVERLAY_RGN;

        int x = pRegionParam->mRgnOvlyX;
        int y = pRegionParam->mRgnOvlyY;
        unsigned int w = pRegionParam->mRgnOvlyW;
        unsigned int h = pRegionParam->mRgnOvlyH;
        picRgn.mRect = {x, y, w, h};

        BITMAP_S stBmp;
        int nSize = 0;
        bzero(&stBmp, sizeof(stBmp));
        if (pRegionParam->mRgnOvlyPixFmt == 0)
        {
            picRgn.mInfo.mOverlay.mPixFmt = MM_PIXEL_FORMAT_RGB_8888;
            picRgn.mInfo.mOverlay.mbInvColEn = pRegionParam->mRgnOvlyInvCorEn;
            picRgn.mInfo.mOverlay.mPriority = pRegionParam->mRgnOvlyPri;

            stBmp.mWidth = w;
            stBmp.mHeight = h;
            stBmp.mPixelFormat = picRgn.mInfo.mOverlay.mPixFmt;
            nSize = BITMAP_S_GetdataSize(&stBmp);
            picRgn.mInfo.mOverlay.mBitmap = malloc(nSize);
            int *pBmp = (int *)picRgn.mInfo.mOverlay.mBitmap;
            int i;
            for (i=0; i<nSize/4; i++)
            {
                pBmp[i] = pRegionParam->mRgnOvlyCorVal;
            }
            rPicRgnList.push_back(picRgn);
        }
        else if (pRegionParam->mRgnOvlyPixFmt == 1)
        {
            picRgn.mInfo.mOverlay.mPixFmt = MM_PIXEL_FORMAT_RGB_1555;
            picRgn.mInfo.mOverlay.mbInvColEn = pRegionParam->mRgnOvlyInvCorEn;
            picRgn.mInfo.mOverlay.mPriority = pRegionParam->mRgnOvlyPri;

            stBmp.mWidth = w;
            stBmp.mHeight = h;
            stBmp.mPixelFormat = picRgn.mInfo.mOverlay.mPixFmt;
            nSize = BITMAP_S_GetdataSize(&stBmp);
            picRgn.mInfo.mOverlay.mBitmap = malloc(nSize);
            short *pBmp = (short *)picRgn.mInfo.mOverlay.mBitmap;
            int i;
            for (i=0; i<nSize/2; i++)
            {
                pBmp[i] = (short)pRegionParam->mRgnOvlyCorVal;
            }
            rPicRgnList.push_back(picRgn);
        }
        else
        {
            aloge("not support region overlay pixel format=%d", pRegionParam->mRgnOvlyPixFmt);
            return;
        }
    }

    if (pRegionParam->mRgnCovEnable == 1)
    {
        PictureRegionType picRgn;
        picRgn.mType = COVER_RGN;

        int x = pRegionParam->mRgnCovX;
        int y = pRegionParam->mRgnCovY;
        unsigned int w = pRegionParam->mRgnCovW;
        unsigned int h = pRegionParam->mRgnCovH;
        picRgn.mRect = {x, y, w, h};

        picRgn.mInfo.mCover.mChromaKey = pRegionParam->mRgnCovCor;
        picRgn.mInfo.mCover.mPriority = pRegionParam->mRgnCovPri;
        rPicRgnList.push_back(picRgn);
    }
}

SampleEyeseeIseContext::SampleEyeseeIseContext()
    :mCameraListener(this)
    ,mIseListener(this)
    ,mRecorderListener(this)
{

    bzero(&mCmdLineParam, sizeof(mCmdLineParam));
    bzero(&mConfigParam, sizeof(mConfigParam));

    cdx_sem_init(&mSemExit, 0);

    mVoDev = 0;
    mUILayer = HLAY(2, 0);

    bzero(&mSysConf, sizeof(mSysConf));

    bzero(&mCamVoLayer, sizeof(mCamVoLayer));
    bzero(&mCamDispChnId, sizeof(mCamDispChnId));

    bzero(&mIseChnAttr, sizeof(mIseChnAttr));
    bzero(&mIseVoLayer, sizeof(mIseVoLayer));
    bzero(&mIseVoLayerEnable, sizeof(mIseVoLayerEnable));
    bzero(&mIseChnId, sizeof(mIseChnId));
    bzero(&mIseChnEnable, sizeof(mIseChnEnable));

    bzero(&mCameraInfo, sizeof(mCameraInfo));
    bzero(&mpCamera, sizeof(mpCamera));
    bzero(&mpIse, sizeof(mpIse));
    bzero(&mpRecorder, sizeof(mpRecorder));

    bzero(&mMuxerId, sizeof(mMuxerId));

    mCamPicNum = 0;
    mIsePicNum = 0;
}

SampleEyeseeIseContext::~SampleEyeseeIseContext()
{
    cdx_sem_deinit(&mSemExit);
    if(mpCamera[0]!=NULL || mpCamera[1]!=NULL)
    {
        aloge("fatal error! some EyeseeCameras is not destruct!");
    }
    if(mpIse!=NULL)
    {
        aloge("fatal error! some EyeseeIses is not destruct!");
    }
    if(mpRecorder[0]!=NULL || mpRecorder[1]!=NULL || mpRecorder[2]!=NULL || mpRecorder[3]!=NULL)
    {
        aloge("fatal error! some EyeseeRecorders is not destruct!");
    }
}

status_t SampleEyeseeIseContext::ParseCmdLine(int argc, char *argv[])
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
            else
            {
                mCmdLineParam.mConfigFilePath = argv[i];
                break;
            }
        }
        else if(!strcmp(argv[i], "-h"))
        {
            std::string helpString;
            helpString += "CmdLine param example:\n";
            helpString += "\t run -path /home/sample_EyeseeIse.conf\n";
            cout<<helpString<<endl;
            ret = 1;
            break;
        }
        else
        {
            std::string ignoreString;
            ignoreString += "ignore invalid CmdLine param:[";
            ignoreString += argv[i];
            ignoreString += "], type -h to get how to set parameter!";
            cout<<ignoreString<<endl;
        }
        i++;
    }
    return ret;
}
status_t SampleEyeseeIseContext::loadConfig()
{
    int ret = SUCCESS;
    const char *pConfPath;
    CONFPARSER_S stConfParser;
    CamDeviceParam *pCamDevParam;
    DispParam      *pDispParam;
    PhotoParam     *pPhotoParam;
    RecParam       *pRecParam;
    IseMoParam     *pIseMoParam;
    IseBiParam     *pIseBiParam;
    IseStiParam    *pIseStiParam;
    int i, j;
    char buf[64] = {0};

    pConfPath = mCmdLineParam.mConfigFilePath.c_str();
    ret = createConfParser(pConfPath, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail!");
        ret = FAILURE;
        return ret;
    }

    mConfigParam.mCamEnable[0] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM0_ENABLE, 0);
    if (mConfigParam.mCamEnable[0])
    {
        pCamDevParam = &mConfigParam.mCamDevParam[0];
        pCamDevParam->mCamCapWidth      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_CAP_WIDTH, 0);
        pCamDevParam->mCamCapHeight     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_CAP_HEIGHT, 0);
        pCamDevParam->mCamPrevWidth     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_PREV_WIDTH, 0);
        pCamDevParam->mCamPrevHeight    = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_PREV_HEIGHT, 0);
        pCamDevParam->mCamFrameRate     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_FRAME_RATE, 0);
        pCamDevParam->mCamBufNum        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_BUF_NUM, 0);
        pCamDevParam->mCamRotation      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RORATION, 0);
        pCamDevParam->mCamId            = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_ID, 0);
        pCamDevParam->mCamCsiChn        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_CSI_CHN, 0);
        pCamDevParam->mCamIspDev        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_ISP_DEV, 0);
        pCamDevParam->mCamIspScale0     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_ISP_SCALE0, 0);
        pCamDevParam->mCamIspScale1     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_ISP_SCALE1, 0);

        mConfigParam.mCamDispEnable[0] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM0_DISP_ENABLE, 0);
        if (mConfigParam.mCamDispEnable[0])
        {
            pDispParam = &mConfigParam.mDispParam[0];
            pDispParam->mDispRectX      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_DISP_RECT_X, 0);
            pDispParam->mDispRectY      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_DISP_RECT_Y, 0);
            pDispParam->mDispRectWidth  = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_DISP_RECT_WIDTH, 0);
            pDispParam->mDispRectHeight = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_DISP_RECT_HEIGHT, 0);
            //pDispParam->mCamDispVoLayer    = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_DISP_VO_LAYER, 0);
        }

        mConfigParam.mCamPhotoEnable[0] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM0_PHOTO_ENABLE, 0);
        if (mConfigParam.mCamPhotoEnable[0])
        {
            pPhotoParam = &mConfigParam.mCamPhotoParam[0];
            pPhotoParam->mPhotoMode         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_PHOTO_MODE, 0);
            pPhotoParam->mCamChnId          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_PHOTO_CHNID, 0);
            pPhotoParam->mPicWidth          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_PHOTO_WIDTH, 0);
            pPhotoParam->mPicHeight         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_PHOTO_HEIGHT, 0);
            strncpy(pPhotoParam->mPicPath, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM0_PHOTO_PATH, NULL), 64);
            pPhotoParam->mContPicCnt        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_PHOTO_CONT_CNT, 0);
            pPhotoParam->mContPicItlMs      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_PHOTO_CONT_ITL, 0);

            mConfigParam.mCamRgnEnable[0]   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_ENABLE, 0);
            if (mConfigParam.mCamRgnEnable[0])
            {
                RegionParam *pRgnParam = &mConfigParam.mCamRgnParam[0];
                pRgnParam->mRgnOvlyEnable   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_ENABLE, 0);
                pRgnParam->mRgnOvlyX        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_X, 0);
                pRgnParam->mRgnOvlyY        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_Y, 0);
                pRgnParam->mRgnOvlyW        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_W, 0);
                pRgnParam->mRgnOvlyH        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_H, 0);
                pRgnParam->mRgnOvlyPixFmt   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_PIX, 0);
                char buf[64] = {0};
                strncpy(buf, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_COR, NULL), 64);
                pRgnParam->mRgnOvlyCorVal   = strtoul(buf, NULL, 16);
                pRgnParam->mRgnOvlyInvCorEn = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_INV_COR_EN, 0);
                pRgnParam->mRgnOvlyPri      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_PRI, 0);

                pRgnParam->mRgnCovEnable    = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_COV_ENABLE, 0);
                pRgnParam->mRgnCovX         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_COV_X, 0);
                pRgnParam->mRgnCovY         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_COV_Y, 0);
                pRgnParam->mRgnCovW         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_COV_W, 0);
                pRgnParam->mRgnCovH         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_COV_H, 0);
                strncpy(buf, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_COV_COR, NULL), 64);
                pRgnParam->mRgnCovCor       = strtoul(buf, NULL, 16);
                pRgnParam->mRgnCovPri       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_COV_PRI, 0);
            }
        }

        mConfigParam.mCamRecEnable[0] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_ENABLE, 0);
        if (mConfigParam.mCamRecEnable[0])
        {
            pRecParam = &mConfigParam.mCamRecParam[0];
            strncpy(pRecParam->mRecParamFileName, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_PATH, NULL), 64);
            strncpy(pRecParam->mRecParamVideoEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_V_TYPE, NULL), 64);
            pRecParam->mRecParamVideoSizeWidth       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_V_WIDTH, 0);
            pRecParam->mRecParamVideoSizeHeight      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_V_HEIGHT, 0);
            pRecParam->mRecParamVideoBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_V_BITRATE, 0);
            pRecParam->mRecParamVideoFrameRate       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_V_FRAMERATE, 0);
            strncpy(pRecParam->mRecParamAudioEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_A_TYPE, NULL), 64);
            pRecParam->mRecParamAudioSamplingRate      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_A_SR, 0);
            pRecParam->mRecParamAudioChannels        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_A_CHN, 0);
            pRecParam->mRecParamAudioBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM0_REC_A_BR, 0);
        }
    }

    mConfigParam.mCamEnable[1] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM1_ENABLE, 0);
    if (mConfigParam.mCamEnable[1])
    {
        pCamDevParam = &mConfigParam.mCamDevParam[1];
        pCamDevParam->mCamCapWidth      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_CAP_WIDTH, 0);
        pCamDevParam->mCamCapHeight     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_CAP_HEIGHT, 0);
        pCamDevParam->mCamPrevWidth     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_PREV_WIDTH, 0);
        pCamDevParam->mCamPrevHeight    = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_PREV_HEIGHT, 0);
        pCamDevParam->mCamFrameRate     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_FRAME_RATE, 0);
        pCamDevParam->mCamBufNum        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_BUF_NUM, 0);
        pCamDevParam->mCamRotation      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RORATION, 0);
        pCamDevParam->mCamId            = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_ID, 0);
        pCamDevParam->mCamCsiChn        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_CSI_CHN, 0);
        pCamDevParam->mCamIspDev        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_ISP_DEV, 0);
        pCamDevParam->mCamIspScale0     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_ISP_SCALE0, 0);
        pCamDevParam->mCamIspScale1     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_ISP_SCALE1, 0);

        mConfigParam.mCamDispEnable[1] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM1_DISP_ENABLE, 0);
        if (mConfigParam.mCamDispEnable[1])
        {
            pDispParam = &mConfigParam.mDispParam[1];
            pDispParam->mDispRectX      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_DISP_RECT_X, 0);
            pDispParam->mDispRectY      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_DISP_RECT_Y, 0);
            pDispParam->mDispRectWidth  = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_DISP_RECT_WIDTH, 0);
            pDispParam->mDispRectHeight = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_DISP_RECT_HEIGHT, 0);
            //pDispParam->mCamDispVoLayer    = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_DISP_VO_LAYER, 0);
        }

        mConfigParam.mCamPhotoEnable[1] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM0_PHOTO_ENABLE, 0);
        if (mConfigParam.mCamPhotoEnable[1])
        {
            pPhotoParam = &mConfigParam.mCamPhotoParam[1];
            pPhotoParam->mPhotoMode         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_PHOTO_MODE, 0);
            pPhotoParam->mCamChnId          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_PHOTO_CHNID, 0);
            pPhotoParam->mPicWidth          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_PHOTO_WIDTH, 0);
            pPhotoParam->mPicHeight         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_PHOTO_HEIGHT, 0);
            strncpy(pPhotoParam->mPicPath, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM1_PHOTO_PATH, NULL), 64);
            pPhotoParam->mContPicCnt        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_PHOTO_CONT_CNT, 0);
            pPhotoParam->mContPicItlMs      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_PHOTO_CONT_ITL, 0);

            mConfigParam.mCamRgnEnable[1]   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_ENABLE, 0);
            if (mConfigParam.mCamRgnEnable[1])
            {
                RegionParam *pRgnParam = &mConfigParam.mCamRgnParam[1];
                pRgnParam->mRgnOvlyEnable   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_OVLY_ENABLE, 0);
                pRgnParam->mRgnOvlyX        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_OVLY_X, 0);
                pRgnParam->mRgnOvlyY        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_OVLY_Y, 0);
                pRgnParam->mRgnOvlyW        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_OVLY_W, 0);
                pRgnParam->mRgnOvlyH        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_OVLY_H, 0);
                pRgnParam->mRgnOvlyPixFmt   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_OVLY_PIX, 0);
                char buf[64] = {0};
                strncpy(buf, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM0_RGN_OVLY_COR, NULL), 64);
                pRgnParam->mRgnOvlyCorVal   = strtoul(buf, NULL, 16);
                pRgnParam->mRgnOvlyInvCorEn = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_OVLY_INV_COR_EN, 0);
                pRgnParam->mRgnOvlyPri      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_OVLY_PRI, 0);

                pRgnParam->mRgnCovEnable    = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_COV_ENABLE, 0);
                pRgnParam->mRgnCovX         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_COV_X, 0);
                pRgnParam->mRgnCovY         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_COV_Y, 0);
                pRgnParam->mRgnCovW         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_COV_W, 0);
                pRgnParam->mRgnCovH         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_COV_H, 0);
                strncpy(buf, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_COV_COR, NULL), 64);
                pRgnParam->mRgnCovCor       = strtoul(buf, NULL, 16);
                pRgnParam->mRgnCovPri       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_RGN_COV_PRI, 0);
            }
        }

        mConfigParam.mCamRecEnable[1] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_ENABLE, 0);
        if (mConfigParam.mCamRecEnable[1])
        {
            pRecParam = &mConfigParam.mCamRecParam[1];
            strncpy(pRecParam->mRecParamFileName, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_PATH, NULL), 64);
            strncpy(pRecParam->mRecParamVideoEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_V_TYPE, NULL), 64);
            pRecParam->mRecParamVideoSizeWidth       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_V_WIDTH, 0);
            pRecParam->mRecParamVideoSizeHeight      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_V_HEIGHT, 0);
            pRecParam->mRecParamVideoBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_V_BITRATE, 0);
            pRecParam->mRecParamVideoFrameRate       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_V_FRAMERATE, 0);
            strncpy(pRecParam->mRecParamAudioEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_A_TYPE, NULL), 64);
            pRecParam->mRecParamAudioSamplingRate      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_A_SR, 0);
            pRecParam->mRecParamAudioChannels        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_A_CHN, 0);
            pRecParam->mRecParamAudioBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM1_REC_A_BR, 0);
        }

    }

    mConfigParam.mIseMode                   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MODE, 0);
    if (mConfigParam.mIseMode)
    {
        mConfigParam.mIseMoEnable            = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_ENABLE, 0);
        pIseMoParam = &mConfigParam.mIseMoParam;
        //ise param
        pIseMoParam->mIseMoType              = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_TYPE, 0);
        pIseMoParam->mIseMoMountTpye         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_MOUNT_TYPE, 0);
        pIseMoParam->mIseMoInWidth           = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_IN_W, 0);
        pIseMoParam->mIseMoInHeight          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_IN_H, 0);
        pIseMoParam->mIseMoInLumaPitch       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_IN_LP, 0);
        pIseMoParam->mIseMoInChromaPitch     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_IN_CP, 0);
        pIseMoParam->mIseMoInYuvType         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_IN_YUV_TYPE, 0);

        pIseMoParam->mIseMoPan               = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PAN, 0);
        pIseMoParam->mIseMoTilt              = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_TILT, 0);
        pIseMoParam->mIseMoZoom              = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_ZOOM, 0);
        pIseMoParam->mIseMoPanSub[0]         = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PANSUB0, 0);
        pIseMoParam->mIseMoPanSub[1]         = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PANSUB1, 0);
        pIseMoParam->mIseMoPanSub[2]         = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PANSUB2, 0);
        pIseMoParam->mIseMoPanSub[3]         = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PANSUB3, 0);
        pIseMoParam->mIseMoTiltSub[0]        = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_TILTSUB0, 0);
        pIseMoParam->mIseMoTiltSub[1]        = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_TILTSUB1, 0);
        pIseMoParam->mIseMoTiltSub[2]        = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_TILTSUB2, 0);
        pIseMoParam->mIseMoTiltSub[3]        = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_TILTSUB3, 0);
        pIseMoParam->mIseMoZoomSub[0]        = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_ZOOMSUB0, 0);
        pIseMoParam->mIseMoZoomSub[1]        = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_ZOOMSUB1, 0);
        pIseMoParam->mIseMoZoomSub[2]        = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_ZOOMSUB2, 0);
        pIseMoParam->mIseMoZoomSub[3]        = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_ZOOMSUB3, 0);
        pIseMoParam->mIseMoPtzSubChg[0]      = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PTZ_SUB_CHG0, 0);
        pIseMoParam->mIseMoPtzSubChg[1]      = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PTZ_SUB_CHG1, 0);
        pIseMoParam->mIseMoPtzSubChg[2]      = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PTZ_SUB_CHG2, 0);
        pIseMoParam->mIseMoPtzSubChg[3]      = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PTZ_SUB_CHG3, 0);

        pIseMoParam->mIseMoOutEn[0]          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_EN0, 0);
        pIseMoParam->mIseMoOutEn[1]          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_EN1, 0);
        pIseMoParam->mIseMoOutEn[2]          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_EN2, 0);
        pIseMoParam->mIseMoOutEn[3]          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_EN3, 0);
        pIseMoParam->mIseMoOutWidth[0]       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_W0, 0);
        pIseMoParam->mIseMoOutWidth[1]       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_W1, 0);
        pIseMoParam->mIseMoOutWidth[2]       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_W2, 0);
        pIseMoParam->mIseMoOutWidth[3]       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_W3, 0);
        pIseMoParam->mIseMoOutHeight[0]      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_H0, 0);
        pIseMoParam->mIseMoOutHeight[1]      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_H1, 0);
        pIseMoParam->mIseMoOutHeight[2]      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_H2, 0);
        pIseMoParam->mIseMoOutHeight[3]      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_H3, 0);

        pIseMoParam->mIseMoOutLumaPitch[0]   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_LP0, 0);
        pIseMoParam->mIseMoOutLumaPitch[1]   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_LP1, 0);
        pIseMoParam->mIseMoOutLumaPitch[2]   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_LP2, 0);
        pIseMoParam->mIseMoOutLumaPitch[3]   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_LP3, 0);
        pIseMoParam->mIseMoOutChromaPitch[0] = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_CP0, 0);
        pIseMoParam->mIseMoOutChromaPitch[1] = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_CP1, 0);
        pIseMoParam->mIseMoOutChromaPitch[2] = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_CP2, 0);
        pIseMoParam->mIseMoOutChromaPitch[3] = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_CP3, 0);

        pIseMoParam->mIseMoOutFlip[0]        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_FLIP0, 0);
        pIseMoParam->mIseMoOutFlip[1]        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_FLIP1, 0);
        pIseMoParam->mIseMoOutFlip[2]        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_FLIP2, 0);
        pIseMoParam->mIseMoOutFlip[3]        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_FLIP3, 0);
        pIseMoParam->mIseMoOutMirror[0]      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_MIR0, 0);
        pIseMoParam->mIseMoOutMirror[1]      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_MIR1, 0);
        pIseMoParam->mIseMoOutMirror[2]      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_MIR2, 0);
        pIseMoParam->mIseMoOutMirror[3]      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_MIR3, 0);

        pIseMoParam->mIseMoOutYuvType        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_OUT_YUV_TYPE, 0);

        pIseMoParam->mIseMoP                 = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_P, 0);
        pIseMoParam->mIseMoCx                = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_CX, 0);
        pIseMoParam->mIseMoCy                = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_CY, 0);
        pIseMoParam->mIseMoFx                = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_FX, 0);
        pIseMoParam->mIseMoFy                = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_FY, 0);
        pIseMoParam->mIseMoCxd               = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_CXD, 0);
        pIseMoParam->mIseMoCyd               = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_CYD, 0);
        pIseMoParam->mIseMoFxd               = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_FXD, 0);
        pIseMoParam->mIseMoFyd               = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_FYD, 0);


        for (i=0; i<8; i++)
        {
            char buf[64] = {0};
            snprintf(buf, 64, "%s%d", SAMPLE_EYESEEISE_ISE_MO_K, i);
            pIseMoParam->mIseMoK[i]         = GetConfParaDouble(&stConfParser, buf, 0);
        }
        pIseMoParam->mIseMoPUndis[0]         = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PU0, 0);
        pIseMoParam->mIseMoPUndis[1]         = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PU1, 0);


        for (i=0; i<3; i++)
        {
            for (j=0; j<3; j++)
            {
                snprintf(buf, 64, "%s%d%d", SAMPLE_EYESEEISE_ISE_MO_CM, i, j);
                pIseMoParam->mIseMoCalibMatrCv[i][j] = GetConfParaDouble(&stConfParser, buf, 0);
            }
        }

        for (i=0; i<3; i++)
        {
            for (j=0; j<3; j++)
            {
                snprintf(buf, 64, "%s%d%d", SAMPLE_EYESEEISE_ISE_MO_CMC, i, j);
                pIseMoParam->mIseMoCalibMatrCv[i][j] = GetConfParaDouble(&stConfParser, buf, 0);
            }
        }

        for (i=0; i<8; i++)
        {
            char buf[64] = {0};
            snprintf(buf, 64, "%s%d", SAMPLE_EYESEEISE_ISE_MO_DISTORT, i);
            pIseMoParam->mIseMoDistort[i]        = GetConfParaInt(&stConfParser, buf, 0);
        }

        strncpy(pIseMoParam->mIseMoReserved, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REV, NULL), 32);

        //disp param
        int i;
        for (i=0; i<4; i++)
        {
            char buf[64] = {0};
            snprintf(buf, 64, "%s%d", SAMPLE_EYESEEISE_ISE_MO_DISP_ENABLE, i);
            mConfigParam.mIseMoDispEnable[i]    = GetConfParaBoolean(&stConfParser, buf, 0);            
            if (mConfigParam.mIseMoDispEnable[i])
            {
                DispParam *pDispParam           = &mConfigParam.mIseMoDispParam[i];
                snprintf(buf, 64, "%s%d", SAMPLE_EYESEEISE_ISE_MO_DISP_RECT_X, i);
                pDispParam->mDispRectX          = GetConfParaInt(&stConfParser, buf, 0);
                snprintf(buf, 64, "%s%d", SAMPLE_EYESEEISE_ISE_MO_DISP_RECT_Y, i);
                pDispParam->mDispRectY          = GetConfParaInt(&stConfParser, buf, 0);
                snprintf(buf, 64, "%s%d", SAMPLE_EYESEEISE_ISE_MO_DISP_RECT_WIDTH, i);
                pDispParam->mDispRectWidth      = GetConfParaInt(&stConfParser, buf, 0);
                snprintf(buf, 64, "%s%d", SAMPLE_EYESEEISE_ISE_MO_DISP_RECT_HEIGHT, i);
                pDispParam->mDispRectHeight     = GetConfParaInt(&stConfParser, buf, 0);
            }
        }

        //photo param
        for (i=0; i<4; i++)
        {
            mConfigParam.mIseMoPhotoEnable[i] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PHOTO_ENABLE, 0);
            if (pIseMoParam->mIseMoOutEn[i] && mConfigParam.mIseMoPhotoEnable[i])
            {
                pPhotoParam = &mConfigParam.mIseMoPhotoParam[i];
                pPhotoParam->mPhotoMode         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PHOTO_MODE, 0);
                pPhotoParam->mCamChnId          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PHOTO_CHNID, 0);
                pPhotoParam->mPicWidth          = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PHOTO_WIDTH, 0);
                pPhotoParam->mPicHeight         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PHOTO_HEIGHT, 0);
                strncpy(pPhotoParam->mPicPath, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PHOTO_PATH, NULL), 64);
                pPhotoParam->mContPicCnt        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PHOTO_CONT_CNT, 0);
                pPhotoParam->mContPicItlMs      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_PHOTO_CONT_ITL, 0);

                mConfigParam.mIseMoRgnEnable[i]   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_ENABLE, 0);
                if (mConfigParam.mIseMoRgnEnable[i])
                {
                    RegionParam *pRgnParam = &mConfigParam.mIseMoRgnParam[i];
                    pRgnParam->mRgnOvlyEnable   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_ENABLE, 0);
                    pRgnParam->mRgnOvlyX        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_X, 0);
                    pRgnParam->mRgnOvlyY        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_Y, 0);
                    pRgnParam->mRgnOvlyW        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_W, 0);
                    pRgnParam->mRgnOvlyH        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_H, 0);
                    pRgnParam->mRgnOvlyPixFmt   = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_PIX, 0);
                    char buf[64] = {0};
                    strncpy(buf, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_COR, NULL), 64);
                    pRgnParam->mRgnOvlyCorVal   = strtoul(buf, NULL, 16);
                    pRgnParam->mRgnOvlyInvCorEn = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_INV_COR_EN, 0);
                    pRgnParam->mRgnOvlyPri      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_PRI, 0);

                    pRgnParam->mRgnCovEnable    = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_COV_ENABLE, 0);
                    pRgnParam->mRgnCovX         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_COV_X, 0);
                    pRgnParam->mRgnCovY         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_COV_Y, 0);
                    pRgnParam->mRgnCovW         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_COV_W, 0);
                    pRgnParam->mRgnCovH         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_COV_H, 0);
                    pRgnParam->mRgnCovCor       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_COV_COR, 0);
                    pRgnParam->mRgnCovPri       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_RGN_COV_PRI, 0);
                }
            }
        }


        mConfigParam.mIseMoRecEnable[0] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_ENABLE, 0);
        if (mConfigParam.mIseMoRecEnable[0])
        {
            pRecParam = &mConfigParam.mIseMoRecParam[0];
            strncpy(pRecParam->mRecParamFileName, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_PATH, NULL), 64);
            strncpy(pRecParam->mRecParamVideoEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_V_TYPE, NULL), 64);
            pRecParam->mRecParamVideoSizeWidth       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_V_WIDTH, 0);
            pRecParam->mRecParamVideoSizeHeight      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_V_HEIGHT, 0);
            pRecParam->mRecParamVideoBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_V_BITRATE, 0);
            pRecParam->mRecParamVideoFrameRate       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_V_FRAMERATE, 0);
            strncpy(pRecParam->mRecParamAudioEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_A_TYPE, NULL), 64);
            pRecParam->mRecParamAudioSamplingRate      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_A_SR, 0);
            pRecParam->mRecParamAudioChannels        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_A_CHN, 0);
            pRecParam->mRecParamAudioBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC0_A_BR, 0);
        }
        mConfigParam.mIseMoRecEnable[1] = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_ENABLE, 0);
        if (mConfigParam.mIseMoRecEnable[1])
        {
            pRecParam = &mConfigParam.mIseMoRecParam[1];
            strncpy(pRecParam->mRecParamFileName, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_PATH, NULL), 64);
            strncpy(pRecParam->mRecParamVideoEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_V_TYPE, NULL), 64);
            pRecParam->mRecParamVideoSizeWidth       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_V_WIDTH, 0);
            pRecParam->mRecParamVideoSizeHeight      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_V_HEIGHT, 0);
            pRecParam->mRecParamVideoBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_V_BITRATE, 0);
            pRecParam->mRecParamVideoFrameRate       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_V_FRAMERATE, 0);
            strncpy(pRecParam->mRecParamAudioEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_A_TYPE, NULL), 64);
            pRecParam->mRecParamAudioSamplingRate      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_A_SR, 0);
            pRecParam->mRecParamAudioChannels        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_A_CHN, 0);
            pRecParam->mRecParamAudioBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC1_A_BR, 0);
        }

    }

    destroyConfParser(&stConfParser);

    return SUCCESS;
}

status_t SampleEyeseeIseContext::checkConfig()
{
    int configRet = FALSE;

    int camOutputWidth0=0, camOutputHeight0=0;
    int camOutputWidth1=0, camOutputHeight1=0;

    CamDeviceParam *pCamDevParam;
    DispParam   *pDispParam;
    PhotoParam   *pPhotoParam;
    int camId = 0;
    for (; camId<2; camId++)
    {
        if (mConfigParam.mCamEnable[camId])
        {
            alogd("-------------------------cam%d enable-------------------------", camId);
            pCamDevParam = &mConfigParam.mCamDevParam[camId];
            alogd("cam%d-dev_param:", camId);
            alogd("               cap_width:        %-4d", pCamDevParam->mCamCapWidth);
            alogd("               cap_height:       %-4d", pCamDevParam->mCamCapHeight);
            alogd("               prev_width:       %-4d", pCamDevParam->mCamPrevWidth);
            alogd("               prev_height:      %-4d", pCamDevParam->mCamPrevHeight);
            alogd("               frame_rate:       %-4d", pCamDevParam->mCamFrameRate);
            alogd("               rotation:         %-4d", pCamDevParam->mCamRotation);
            alogd("               cam_id:           %-4d", pCamDevParam->mCamId);
            alogd("               csi_chn:          %-4d", pCamDevParam->mCamCsiChn);
            alogd("               isp_dev:          %-4d", pCamDevParam->mCamIspDev);
            alogd("               isp_scale0:       %-4d", pCamDevParam->mCamIspScale0);
            alogd("               isp_scale1:       %-4d", pCamDevParam->mCamIspScale1);
            alogd("               disp_flag:        %-4d", mConfigParam.mCamDispEnable[camId]);
            if (mConfigParam.mCamDispEnable[camId])
            {
                pDispParam = &mConfigParam.mDispParam[camId];
                alogd("cam%d-disp_param:", camId);
                alogd("               disp_rect_x:  %-4d", pDispParam->mDispRectX);
                alogd("               disp_rect_x:  %-4d", pDispParam->mDispRectY);
                alogd("               disp_rect_w:  %-4d", pDispParam->mDispRectWidth);
                alogd("               disp_rect_h:  %-4d", pDispParam->mDispRectHeight);
                //alogd("               disp_vo_layer:%-4d", pDispParam->mCamDispVoLayer);
            }
            if (mConfigParam.mCamPhotoEnable[camId])
            {
                pPhotoParam = &mConfigParam.mCamPhotoParam[camId];
                alogd("cam%d-photo_param:", camId);
                alogd("               photo_mode:       %-4d", pPhotoParam->mPhotoMode);
                alogd("               photo_camChnId:   %-4d", pPhotoParam->mCamChnId);
                alogd("               photo_width:      %-4d", pPhotoParam->mPicWidth);
                alogd("               photo_height:     %-4d", pPhotoParam->mPicHeight);
                alogd("               path:  %s", pPhotoParam->mPicPath);
            }

            if (!pCamDevParam->mCamCapWidth || !pCamDevParam->mCamCapHeight)
            {
                aloge("careful! cam%d w&h config wrong!", camId);
                configRet = FAILURE;
                return configRet;
            }
            else
            {
                if (camId == 0)
                {
                    camOutputWidth0 = pCamDevParam->mCamCapWidth;
                    camOutputHeight0 = pCamDevParam->mCamCapHeight;
                }
                else
                {
                    camOutputWidth1 = pCamDevParam->mCamCapWidth;
                    camOutputHeight1 = pCamDevParam->mCamCapHeight;
                }
            }

            if (configRet == FAILURE)
            {
                configRet = SUCCESS;
            }
        }
        else
        {
            alogd("-------------------------cam%d disable-------------------------", camId);
        }
    }
    if (mConfigParam.mCamEnable[0]==0 && mConfigParam.mCamEnable[1]==0)
    {
        aloge("why both cameras are disable?!");
        configRet = FAILURE;
        return configRet;
    }

    if (mConfigParam.mIseMode)
    {
        alogd("-------------------------ise enable, mode=%d-------------------------", mConfigParam.mIseMode);
        if (mConfigParam.mIseMode == 1)
        {
            IseMoParam *pIseMoParam = &mConfigParam.mIseMoParam;
            switch (pIseMoParam->mIseMoType)
            {
                case 1:
                {
                    alogw("==============ise mo-pano180, fix some params...==============");

                    break;
                }
                case 2:
                {
                    alogw("==============ise mo-pano360, fix some params...==============");
                    break;
                }
                case 3:
                {
                    alogw("================ise mo-ptz, fix some params...================");

                    break;
                }
                case 4:
                {
                    alogw("=============ise mo-undistort, fix some params...=============");
                    pIseMoParam->mIseMoInWidth               = (camOutputWidth0==0) ? camOutputWidth1:camOutputWidth0;
                    pIseMoParam->mIseMoInHeight              = (camOutputHeight0==0) ? camOutputHeight1:camOutputHeight0;
                    pIseMoParam->mIseMoInLumaPitch           = pIseMoParam->mIseMoInWidth;
                    pIseMoParam->mIseMoInChromaPitch         = pIseMoParam->mIseMoInWidth;

                    pIseMoParam->mIseMoOutLumaPitch[0]       = pIseMoParam->mIseMoOutWidth[0];
                    pIseMoParam->mIseMoOutLumaPitch[1]       = pIseMoParam->mIseMoOutWidth[1];
                    pIseMoParam->mIseMoOutLumaPitch[2]       = pIseMoParam->mIseMoOutWidth[2];
                    pIseMoParam->mIseMoOutLumaPitch[3]       = pIseMoParam->mIseMoOutWidth[3];
                    pIseMoParam->mIseMoOutChromaPitch[0]     = pIseMoParam->mIseMoOutWidth[0];
                    pIseMoParam->mIseMoOutChromaPitch[1]     = pIseMoParam->mIseMoOutWidth[1];
                    pIseMoParam->mIseMoOutChromaPitch[2]     = pIseMoParam->mIseMoOutWidth[2];
                    pIseMoParam->mIseMoOutChromaPitch[3]     = pIseMoParam->mIseMoOutWidth[3];

                    pIseMoParam->mIseMoP                     = pIseMoParam->mIseMoInWidth*1.0/3.1415;
                    pIseMoParam->mIseMoCx                    = 918.18164 * (pIseMoParam->mIseMoInWidth*1.0/1920);
                    pIseMoParam->mIseMoCy                    = 479.86784 * (pIseMoParam->mIseMoInHeight*1.0/1080);
                    pIseMoParam->mIseMoFx                    = 1059.11146 * (pIseMoParam->mIseMoInWidth*1.0/1920);
                    pIseMoParam->mIseMoFy                    = 1053.26240 * (pIseMoParam->mIseMoInHeight*1.0/1080);
                    pIseMoParam->mIseMoCxd                   = 876.34066 * (pIseMoParam->mIseMoInWidth*1.0/1920);
                    pIseMoParam->mIseMoCyd                   = 465.93162 * (pIseMoParam->mIseMoInHeight*1.0/1080);
                    pIseMoParam->mIseMoFxd                   = 691.76196 * (pIseMoParam->mIseMoInWidth*1.0/1920);
                    pIseMoParam->mIseMoFyd                   = 936.82897 * (pIseMoParam->mIseMoInHeight*1.0/1080);
                    pIseMoParam->mIseMoK[0]                  = 0.182044494560808;
                    pIseMoParam->mIseMoK[1]                  = -0.1481043082174997;
                    pIseMoParam->mIseMoK[2]                  = -0.005128687334715951;
                    pIseMoParam->mIseMoK[3]                  = 0.567926713301489;
                    pIseMoParam->mIseMoK[4]                  = -0.1789466261819578;
                    pIseMoParam->mIseMoK[5]                  = -0.03561367966855939;
                    pIseMoParam->mIseMoPUndis[0]             = 0.000649146914880072;
                    pIseMoParam->mIseMoPUndis[1]             = 0.0002534155740808075;
                    break;
                }
                case 5:
                {
                    alogw("==============ise mo-180with2, fix some params...=============");
                    
                    break;
                }
                case 6:
                {
                    alogw("==============ise mo-ptz4in1, fix some params...==============");
                    
                    break;
                }
                default:
                {
                    
                    break;
                }
            }
        }
        else if (mConfigParam.mIseMode == 2)
        {

        }
        else if (mConfigParam.mIseMode == 3)
        {

        }
        else
        {
            alogd("why ise mode=%d?! sdk not support!", mConfigParam.mIseMode);
            configRet = FAILURE;
            return configRet;
        }
    }
    else
    {
        alogd("-------------------------ise disable-------------------------");
    }

    return configRet;
}

VO_LAYER requestChannelDisplayLayer(DispParam *pDiapParam)
{
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
        return -1;
    }

    VO_VIDEO_LAYER_ATTR_S layer_attr;
    AW_MPI_VO_GetVideoLayerAttr(hlay, &layer_attr);
    layer_attr.stDispRect.X = pDiapParam->mDispRectX;
    layer_attr.stDispRect.Y = pDiapParam->mDispRectY;
    layer_attr.stDispRect.Width = pDiapParam->mDispRectWidth;
    layer_attr.stDispRect.Height = pDiapParam->mDispRectHeight;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &layer_attr);

    return hlay;
}

int configCamera(CamDeviceParam *pCamDevParam)
{
    int cameraId;
    CameraInfo cameraInfo;
    VI_CHN viChn;
    ISP_DEV ispDev;
    ISPGeometry mISPGeometry;

    cameraId = pCamDevParam->mCamId;
    cameraInfo.facing = CAMERA_FACING_BACK;
    cameraInfo.orientation = 0;
    cameraInfo.canDisableShutterSound = true;
    cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
    cameraInfo.mMPPGeometry.mCSIChn = pCamDevParam->mCamCsiChn;
    mISPGeometry.mISPDev = pCamDevParam->mCamIspDev;
    viChn = pCamDevParam->mCamIspScale0;
    mISPGeometry.mScalerOutChns.push_back(viChn);
    viChn = pCamDevParam->mCamIspScale1;
    mISPGeometry.mScalerOutChns.push_back(viChn);
    cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
    EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);

    return SUCCESS;
}

int startCam(int camId, CamDeviceParam *pCamDevParam, DispParam *pDispParam, SampleEyeseeIseContext *pContext)
{
    alogw("-----------------start Cam%d begin--------------------", camId);

    CameraInfo& cameraInfoRef = pContext->mCameraInfo[camId];
    EyeseeCamera::getCameraInfo(camId, &cameraInfoRef);
    pContext->mpCamera[camId] = EyeseeCamera::open(camId);
    pContext->mpCamera[camId]->setInfoCallback(&pContext->mCameraListener);
    pContext->mpCamera[camId]->prepareDevice();

    pContext->mpCamera[camId]->startDevice();

    int vi_chn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
    pContext->mpCamera[camId]->openChannel(vi_chn, TRUE);
    alogd(">>>>> CAM_INFO(camId:%d, chnId:%d, mpCamera:%p)", camId, vi_chn, pContext->mpCamera[camId]);

    CameraParameters cameraParam;
    pContext->mpCamera[camId]->getParameters(vi_chn, cameraParam);
    SIZE_S captureSize;
    captureSize.Width = pCamDevParam->mCamCapWidth;
    captureSize.Height = pCamDevParam->mCamCapHeight;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(pCamDevParam->mCamFrameRate);
    cameraParam.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    cameraParam.setVideoBufferNumber(pCamDevParam->mCamBufNum);
    cameraParam.setPreviewRotation(pCamDevParam->mCamRotation);
    pContext->mpCamera[camId]->setParameters(vi_chn, cameraParam);

    pContext->mpCamera[camId]->prepareChannel(vi_chn);
    pContext->mpCamera[camId]->startChannel(vi_chn);

    if (pContext->mConfigParam.mCamDispEnable[camId])
    {
        int vi_prev_chn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1];
        pContext->mCamDispChnId[camId] = vi_prev_chn;
        pContext->mpCamera[camId]->openChannel(vi_prev_chn, FALSE);
        alogd(">>>>> CAM_INFO(camId:%d, chnId:%d, mpCamera:%p)", camId, vi_prev_chn, pContext->mpCamera[camId]);

        pContext->mpCamera[camId]->getParameters(vi_prev_chn, cameraParam);
        SIZE_S previewSize;
        previewSize.Width = pCamDevParam->mCamPrevWidth;
        previewSize.Height = pCamDevParam->mCamPrevHeight;
        cameraParam.setVideoSize(previewSize);
        cameraParam.setPreviewFrameRate(pCamDevParam->mCamFrameRate);
        cameraParam.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
        cameraParam.setVideoBufferNumber(pCamDevParam->mCamBufNum);
        cameraParam.setPreviewRotation(pCamDevParam->mCamRotation);
        pContext->mpCamera[camId]->setParameters(vi_prev_chn, cameraParam);
        pContext->mpCamera[camId]->prepareChannel(vi_prev_chn);

        int hlay = 0;
        while(hlay < VO_MAX_LAYER_NUM)
        {
            if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay))
            {
                alogd("---------cam_id(%d)&chn_id(%d) use vo_layer(%d) to display---------", camId, vi_prev_chn, hlay);
                break;
            }
            hlay++;
        }
        if(hlay >= VO_MAX_LAYER_NUM)
        {
            aloge("fatal error! enable video layer fail!");
        }

        VO_VIDEO_LAYER_ATTR_S vo_layer_attr;
        AW_MPI_VO_GetVideoLayerAttr(hlay, &vo_layer_attr);
        vo_layer_attr.stDispRect.X = pDispParam->mDispRectX;
        vo_layer_attr.stDispRect.Y = pDispParam->mDispRectY;
        vo_layer_attr.stDispRect.Width = pDispParam->mDispRectWidth;
        vo_layer_attr.stDispRect.Height = pDispParam->mDispRectHeight;
        AW_MPI_VO_SetVideoLayerAttr(hlay, &vo_layer_attr);
        pContext->mCamVoLayer[camId] = hlay;
        pContext->mpCamera[camId]->setChannelDisplay(vi_prev_chn, hlay);
        pContext->mpCamera[camId]->startChannel(vi_prev_chn);
        //pContext->mpCamera[camId]->startRender(vi_prev_chn);
    }

    alogw("-----------------start Cam%d end--------------------", camId);

    return SUCCESS;
}

int stopCam(int camId, CamDeviceParam *pCamDevParam, DispParam *pDispParam, SampleEyeseeIseContext *pContext)
{
    alogw("-----------------stop Cam%d begin--------------------", camId);

    CameraInfo& cameraInfoRef = pContext->mCameraInfo[camId];
    int vi_chn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
    pContext->mpCamera[camId]->stopChannel(vi_chn);
    pContext->mpCamera[camId]->releaseChannel(vi_chn);
    pContext->mpCamera[camId]->closeChannel(vi_chn);
    if (pContext->mConfigParam.mCamDispEnable[camId])
    {
        int vi_prev_chn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1];
        pContext->mpCamera[camId]->stopRender(vi_prev_chn);
        pContext->mpCamera[camId]->stopChannel(vi_prev_chn);
        pContext->mpCamera[camId]->releaseChannel(vi_prev_chn);
        pContext->mpCamera[camId]->closeChannel(vi_prev_chn);
    }
    pContext->mpCamera[camId]->stopDevice();
    pContext->mpCamera[camId]->releaseDevice();
    EyeseeCamera::close(pContext->mpCamera[camId]);
    pContext->mpCamera[camId] = NULL;
    AW_MPI_VO_DisableVideoLayer(pContext->mCamVoLayer[camId]);
    pContext->mCamVoLayer[camId] = -1;

    alogw("-----------------stop Cam%d end--------------------", camId);

    return SUCCESS;
}

void dumpIseParam(ISE_CFG_PARA_MO *pMoCfg)
{
    alogd("-------------------ISE Configure Param-------------------------");
    alogd("|dewarp_mode             = %-8d                           |", pMoCfg->dewarp_mode);
    alogd("|mount_mode              = %-8d                           |", pMoCfg->mount_mode);
    alogd("|in_h                    = %-8d                           |", pMoCfg->in_h);
    alogd("|in_w                    = %-8d                           |", pMoCfg->in_w);
    alogd("|in_luma_pitch           = %-8d                           |", pMoCfg->in_luma_pitch);
    alogd("|in_chroma_pitch         = %-8d                           |", pMoCfg->in_chroma_pitch);
    alogd("|in_yuv_type             = %-8d                           |", pMoCfg->in_yuv_type);

    alogd("|pan                     = %-3.8f                       |", pMoCfg->pan);
    alogd("|tilt                    = %-3.8f                       |", pMoCfg->tilt);
    alogd("|zoom                    = %-3.8f                       |", pMoCfg->zoom);
    alogd("|ptzsub_chg[0]           = %-3.8d                       |", pMoCfg->ptzsub_chg[0]);
    alogd("|ptzsub_chg[1]           = %-3.8d                       |", pMoCfg->ptzsub_chg[1]);

    alogd("|out_en[0]               = %-8d                           |", pMoCfg->out_en[0]);
    alogd("|out_h[0]                = %-8d                           |", pMoCfg->out_h[0]);
    alogd("|out_w[0]                = %-8d                           |", pMoCfg->out_w[0]);
    alogd("|out_luma_pitch[0]       = %-8d                           |", pMoCfg->out_luma_pitch[0]);
    alogd("|out_chroma_pitch[0]     = %-8d                           |", pMoCfg->out_chroma_pitch[0]);
    alogd("|out_flip[0]             = %-8d                           |", pMoCfg->out_flip[0]);
    alogd("|out_mirror[0]           = %-8d                           |", pMoCfg->out_mirror[0]);
    alogd("|out_yuv_type            = %-8d                           |", pMoCfg->out_yuv_type);

    alogd("|p                       = %-3.8f                       |", pMoCfg->p);
    alogd("|cx                      = %-3.8f                       |", pMoCfg->cx);
    alogd("|cy                      = %-3.8f                       |", pMoCfg->cy);
    alogd("|fx                      = %-3.8f                       |", pMoCfg->fx);
    alogd("|fy                      = %-3.8f                       |", pMoCfg->fy);
    alogd("|cxd                     = %-3.8f                       |", pMoCfg->cxd);
    alogd("|cyd                     = %-3.8f                       |", pMoCfg->cyd);
    alogd("|fxd                     = %-3.8f                       |", pMoCfg->fxd);
    alogd("|fyd                     = %-3.8f                       |", pMoCfg->fyd);

    alogd("|k[0]                    = %-3.8f                       |", pMoCfg->k[0]);
    alogd("|k[1]                    = %-3.8f                       |", pMoCfg->k[1]);
    alogd("|k[2]                    = %-3.8f                       |", pMoCfg->k[2]);
    alogd("|k[3]                    = %-3.8f                       |", pMoCfg->k[3]);
    alogd("|k[4]                    = %-3.8f                       |", pMoCfg->k[4]);
    alogd("|k[5]                    = %-3.8f                       |", pMoCfg->k[5]);

    alogd("|p_undis[0]              = %-3.8f                       |", pMoCfg->p_undis[0]);
    alogd("|p_undis[1]              = %-3.8f                       |", pMoCfg->p_undis[1]);

    alogd("|distort[0]              = %-3.8f                       |", pMoCfg->distort[0]);
    alogd("|distort[1]              = %-3.8f                       |", pMoCfg->distort[1]);
    alogd("|distort[2]              = %-3.8f                       |", pMoCfg->distort[2]);
    alogd("|distort[3]              = %-3.8f                       |", pMoCfg->distort[3]);
    alogd("|distort[4]              = %-3.8f                       |", pMoCfg->distort[4]);
    alogd("|distort[5]              = %-3.8f                       |", pMoCfg->distort[5]);
    alogd("|distort[6]              = %-3.8f                       |", pMoCfg->distort[6]);
    alogd("|distort[7]              = %-3.8f                       |", pMoCfg->distort[7]);
    alogd("---------------------------------------------------------------");
}

int copyIseParamFromConfigFile(ISE_CHN_ATTR_S *pIseAttr, SampleEyeseeIseConfig *pIseConfig)
{
    int ret = SUCCESS;

    switch (pIseConfig->mIseMode)
    {
        case 1:
        {
            ISE_CFG_PARA_MO *pMoCfg = &pIseAttr->mode_attr.mFish.ise_cfg;
            IseMoParam *pIseMoParam = &pIseConfig->mIseMoParam;
            memcpy(pMoCfg, pIseMoParam, sizeof(ISE_CFG_PARA_MO));
            break;
        }
        case 2:
        {
            break;
        }
        case 3:
        {
            break;
        }
        default:
        {
            aloge("---------can not happen!-----------");
            break;
        }
    }

    return ret;
}

EyeseeISE* openIseDevice(ISE_MODE_E mode, EyeseeIseListener *pListener)
{
    EyeseeISE *pIse = EyeseeISE::open();
    pIse->setErrorCallback(pListener);
    pIse->setInfoCallback(pListener);

    pIse->prepareDevice(mode);

    return pIse;
}


int startIseMo(SampleEyeseeIseContext *pContext)
{
    int ret = SUCCESS;
    SampleEyeseeIseConfig *pConfigParam = &pContext->mConfigParam;
    IseMoParam *pIseMoParam = &pConfigParam->mIseMoParam;
    EyeseeLinux::EyeseeISE *pIseContext;

    alogd("-----------------start Ise-Mo-Undistort begin--------------------");

    pIseContext = openIseDevice(ISE_MODE_ONE_FISHEYE, &pContext->mIseListener);
    pContext->mpIse = pIseContext;

    // init iseParam with camParam
    int camId;
    EyeseeLinux::EyeseeCamera *pCamera;
    if (pConfigParam->mCamEnable[0])
    {
        camId = 0;
        pCamera = pContext->mpCamera[0];
    }
    else if (pConfigParam->mCamEnable[1])
    {
        camId = 1;
        pCamera = pContext->mpCamera[1];
    }
    else
    {
        ret = FAILURE;
        aloge("why both cameras are disabled?!");
        return ret;
    }

    CameraParameters iseParam;
    CameraInfo& cameraInfoRef = pContext->mCameraInfo[camId];
    int camChn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
	
//    pContext->mpCamera[camId]->getParameters(camChn, iseParam);

    copyIseParamFromConfigFile(&pContext->mIseChnAttr[0], pConfigParam);
    ISE_CFG_PARA_MO *pMoCfg = &pContext->mIseChnAttr[0].mode_attr.mFish.ise_cfg;
    dumpIseParam(pMoCfg);

    int iseChn;
    for (iseChn=0; iseChn<4; iseChn++)
    {
        if (pMoCfg->out_en[iseChn])
        {
            int chn = pIseContext->openChannel(&pContext->mIseChnAttr[0]);
            if (chn != iseChn)
            {
                aloge("fatal error! why get real chn(%d) != expected iseChn(%d)? check code!", chn, iseChn);
            }
            pContext->mIseChnId[0][iseChn] = iseChn;
            SIZE_S ise_area;
            ise_area.Width = pMoCfg->out_w[iseChn];    // complete same with CamInfo except width&height&rotation, for rec
            ise_area.Height = pMoCfg->out_h[iseChn];
			
			pIseContext->getParameters(iseChn, iseParam);
            iseParam.setVideoSize(ise_area);
            //iseParam.setPreviewRotation(90);
            pIseContext->setParameters(iseChn, iseParam);

            if (pConfigParam->mIseMoDispEnable[iseChn])
            {
                pContext->mIseVoLayer[iseChn] = requestChannelDisplayLayer(&pConfigParam->mIseMoDispParam[iseChn]);
                pContext->mIseVoLayerEnable[iseChn] = TRUE;
                pIseContext->setChannelDisplay(iseChn, pContext->mIseVoLayer[iseChn]);
            }
            pIseContext->startChannel(iseChn);
            pContext->mIseChnEnable[0][iseChn] = TRUE;
        }
    }

    CameraChannelInfo camChnInfo;
    camChnInfo.mnCameraChannel = camChn;
    camChnInfo.mpCameraRecordingProxy = pCamera->getRecordingProxy();
    vector<CameraChannelInfo> camChnVec;
    camChnVec.push_back(camChnInfo);
    pIseContext->setCamera(camChnVec);

    pIseContext->startDevice();

    alogd("-----------------start Ise-Mo-Undistort end--------------------");

    return ret;
}

int stopIseMo(SampleEyeseeIseContext *pContext)
{
    int ret = SUCCESS;
    EyeseeLinux::EyeseeISE *pIseContext = pContext->mpIse;

    alogd("-----------------stop Ise-Mo-Undistort begin--------------------");

    pIseContext->stopDevice();

    int chnId;
    for (chnId=0; chnId<4; chnId++)
    {
        if (pContext->mIseChnEnable[0][chnId])
        {
            //pIseContext->stopChannel(iseChn);
            pIseContext->closeChannel(pContext->mIseChnId[0][chnId]);
        }
    }

    pIseContext->releaseDevice();
    EyeseeISE::close(pContext->mpIse);
    pContext->mpIse = NULL;

    int voLayer;
    for (voLayer = 0; voLayer < 4; voLayer++)
    {
        if (pContext->mIseVoLayerEnable[voLayer])
        {
            AW_MPI_VO_DisableVideoLayer(pContext->mIseVoLayer[voLayer]);
        }
    }

    alogd("-----------------stop Ise-Mo-Undistort end--------------------");

    return ret;
}

int startIseBi(SampleEyeseeIseContext *pContext)
{
    int ret = SUCCESS;

    return ret;
}

int stopIseBi(SampleEyeseeIseContext *pContext)
{
    int ret = SUCCESS;

    return ret;
}

int startIseSti(SampleEyeseeIseContext *pContext)
{
    int ret = SUCCESS;

    return ret;
}

int stopIseSti(SampleEyeseeIseContext *pContext)
{
    int ret = SUCCESS;

    return ret;
}

int startIse(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;

    alogw("-------------start ise begin---------------");

    switch(pContext->mConfigParam.mIseMode)
    {
        case 1:
        {
            result = startIseMo(pContext);
            break;
        }
        case 2:
        {
            result = startIseBi(pContext);
            break;
        }
        case 3:
        {
            result = startIseSti(pContext);
            break;
        }
        default:
        {
            aloge("---------wrong ise type(%d)---------", pContext->mConfigParam.mIseMode);
            break;
        }
    }
    
    if (result != SUCCESS)
    {
        aloge("-------------open ise fail!---------------");
    }

    alogw("-------------start ise end---------------");

    return result;
}

int stopIse(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;

    alogw("-------------stop ise begin---------------");

    switch(pContext->mConfigParam.mIseMode)
    {
        case 1:
        {
            result = stopIseMo(pContext);
            break;
        }
        case 2:
        {
            result = stopIseBi(pContext);
            break;
        }
        case 3:
        {
            result = stopIseSti(pContext);
            break;
        }
        default:
        {
            aloge("---------wrong ise type(%d)---------", pContext->mConfigParam.mIseMode);
            break;
        }
    }

    if (result != SUCCESS)
    {
        aloge("-------------stop ise fail!---------------");
    }

    alogw("-------------stop ise end---------------");

    return result;
}

/**************************************PHOTO****************************************/
int startCamPhoto(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    PhotoParam *pPhotoParamm;
    CameraParameters camParamRef;
    int camId;

    for (camId=0; camId<2; camId++)
    {
        if (pContext->mConfigParam.mCamEnable[camId] && pContext->mConfigParam.mCamPhotoEnable[camId])
        {
            alogd("-------------start cam%d photo begin---------------", camId);
            pPhotoParamm = &pContext->mConfigParam.mCamPhotoParam[camId];
            int camChnId = pPhotoParamm->mCamChnId;
            CameraInfo& camInfoRef = pContext->mCameraInfo[camChnId];
            //camChn = camInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];    //scal0-main for photo&rec, scal1-sub for preview
            pContext->mpCamera[camId]->getParameters(camChnId, camParamRef);
            TAKE_PICTURE_MODE_E mode = pPhotoParamm->mPhotoMode==0 ? TAKE_PICTURE_MODE_CONTINUOUS:TAKE_PICTURE_MODE_FAST;
            camParamRef.setPictureMode(mode);
            SIZE_S size;
            size.Width = pPhotoParamm->mPicWidth;
            size.Height = pPhotoParamm->mPicHeight;
            camParamRef.setPictureSize(size);
            if (pPhotoParamm->mPhotoMode == 0)
            {
                int cnt = pPhotoParamm->mContPicCnt;
                int itl = pPhotoParamm->mContPicItlMs;
                alogd("cam%d photo by continuous, total pic cnt=%d, interval=%d, will last %d ms", camId, cnt, itl, cnt*itl);
                camParamRef.setContinuousPictureNumber(pPhotoParamm->mContPicCnt);
                camParamRef.setContinuousPictureIntervalMs(pPhotoParamm->mContPicItlMs);
                pContext->mpCamera[camId]->setParameters(camChnId, camParamRef);
                pContext->mpCamera[camId]->takePicture(camChnId, NULL, NULL, &pContext->mCameraListener, &pContext->mCameraListener);
            }
            else if (pPhotoParamm->mPhotoMode == 1)
            {
                alogd("cam%d photo by fast, you need key board to take picture!", camId);
                char buf[64];
                do {
                    alogd("help: use string with prefix 'q' to exit, others to take. ready...");
                    fgets(buf, 64, stdin);
                    pContext->mpCamera[camId]->takePicture(camChnId, NULL, NULL, &pContext->mCameraListener, &pContext->mCameraListener);
                } while(buf[0] != 'q');
            }
            alogd("-------------start cam%d photo end---------------", camId);
        }
    }

    return result;
}

int startIseMoPhoto(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    PhotoParam *pPhotoParamm;
    CameraParameters iseParamRef;
    int iseChnId;

    for (iseChnId=0; iseChnId<4; iseChnId++)
    {
        if (pContext->mConfigParam.mIseMoParam.mIseMoOutEn[iseChnId] && pContext->mConfigParam.mIseMoPhotoEnable[iseChnId])
        {
            alogd("-------------start iseChn%d photo begin---------------", iseChnId);
            pPhotoParamm = &pContext->mConfigParam.mIseMoPhotoParam[iseChnId];
            CameraInfo& camInfoRef = pContext->mCameraInfo[iseChnId];
            pContext->mpIse->getParameters(iseChnId, iseParamRef);
            TAKE_PICTURE_MODE_E mode = pPhotoParamm->mPhotoMode==0 ? TAKE_PICTURE_MODE_CONTINUOUS:TAKE_PICTURE_MODE_FAST;
            iseParamRef.setPictureMode(mode);
            SIZE_S size;
            size.Width = pPhotoParamm->mPicWidth;
            size.Height = pPhotoParamm->mPicHeight;
            iseParamRef.setPictureSize(size);
            if (pPhotoParamm->mPhotoMode == 0)
            {
                int cnt = pPhotoParamm->mContPicCnt;
                int itl = pPhotoParamm->mContPicItlMs;
                alogd("iseChn%d photo by continuous, total pic cnt=%d, interval=%d, will last %d ms", iseChnId, cnt, itl, cnt*itl);
                iseParamRef.setContinuousPictureNumber(pPhotoParamm->mContPicCnt);
                iseParamRef.setContinuousPictureIntervalMs(pPhotoParamm->mContPicItlMs);
                pContext->mpIse->setParameters(iseChnId, iseParamRef);
                pContext->mpIse->takePicture(iseChnId, NULL, NULL, &pContext->mIseListener, &pContext->mIseListener);
            }
            else if (pPhotoParamm->mPhotoMode == 1)
            {
                alogd("iseChn%d photo by fast, you need key board to take picture!");
                char buf[64];
                do {
                    alogd("help: use string with prefix 'q' to exit, others to take. ready...");
                    fgets(buf, 64, stdin);
                    pContext->mpIse->takePicture(iseChnId, NULL, NULL, &pContext->mIseListener, &pContext->mIseListener);
                } while(buf[0] != 'q');
            }
            alogd("-------------start isechn%d photo end---------------", iseChnId);
        }
    }

    return result;
}

/**************************************REC****************************************/
EyeseeRecorder* prepareAVRecord(RecParam *pRecParam, EyeseeRecorderListener *pListener)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder = new EyeseeRecorder();

    pRecorder->setOnInfoListener(pListener);
    pRecorder->setOnDataListener(pListener);
    pRecorder->setOnErrorListener(pListener);
    pRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    pRecorder->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, pRecParam->mRecParamFileName, 0, false);
    pRecorder->setVideoFrameRate(pRecParam->mRecParamVideoFrameRate);
    //pRecorder->setVideoEncodingBitRate(pRecParam->mRecParamVideoBitRate*1024*1024);
    pRecorder->setVideoSize(pRecParam->mRecParamVideoSizeWidth, pRecParam->mRecParamVideoSizeHeight);
    PAYLOAD_TYPE_E type;
    if (!strcmp(pRecParam->mRecParamVideoEncoder, "h264"))
    {
        type = PT_H264;
    }
    else if (!strcmp(pRecParam->mRecParamVideoEncoder, "h265"))
    {
        type = PT_H265;
    }
    else
    {
        alogw("unsupported video encoder: [%s], use default: [h264]", pRecParam->mRecParamVideoEncoder);
        type = PT_H264;
    }
    pRecorder->setVideoEncoder(type);

    pRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    pRecorder->setAudioSamplingRate(pRecParam->mRecParamAudioSamplingRate);
    pRecorder->setAudioChannels(pRecParam->mRecParamAudioChannels);
    pRecorder->setAudioEncodingBitRate(pRecParam->mRecParamAudioBitRate);
    if (!strcmp(pRecParam->mRecParamVideoEncoder, "aac"))
    {
        type = PT_AAC;
    }
    else
    {
        alogw("unsupported audio encoder: [%s], use default: [aac", pRecParam->mRecParamAudioEncoder);
        type = PT_AAC;
    }
    pRecorder->setAudioEncoder(type);

    return pRecorder;
}

int startCamRec(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder;

    if (pContext->mConfigParam.mCamEnable[0] && pContext->mConfigParam.mCamRecEnable[0])
    {
        alogd("-------------start cam0 rec begin---------------");
        pRecorder = prepareAVRecord(&pContext->mConfigParam.mCamRecParam[0], &pContext->mRecorderListener);
        pContext->mpRecorder[0] = pRecorder;
        CameraParameters iseParam;
        CameraInfo& cameraInfoRef = pContext->mCameraInfo[0];
        int camRecChn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
        pRecorder->setCameraProxy(pContext->mpCamera[0]->getRecordingProxy(), camRecChn);
        pRecorder->prepare();
        pRecorder->start();
        alogd("-------------start cam0 rec end---------------");
    }

    if (pContext->mConfigParam.mCamEnable[1] && pContext->mConfigParam.mCamRecEnable[1])
    {
        alogd("-------------start cam1 rec begin---------------");
        pRecorder = prepareAVRecord(&pContext->mConfigParam.mCamRecParam[1], &pContext->mRecorderListener);
        pContext->mpRecorder[1] = pRecorder;
        CameraParameters iseParam;
        CameraInfo& cameraInfoRef = pContext->mCameraInfo[1];
        int camRecChn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
        pRecorder->setCameraProxy(pContext->mpCamera[1]->getRecordingProxy(), camRecChn);
        pRecorder->prepare();
        pRecorder->start();
        alogd("-------------start cam1 rec end---------------");
    }

    return result;
}

int stopCamRec(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder;

    if (pContext->mConfigParam.mCamEnable[0] && pContext->mConfigParam.mCamRecEnable[0] && pContext->mpRecorder[0])
    {
        alogd("-------------stop cam0 rec begin---------------");
        pContext->mpRecorder[0]->stop();
        pContext->mpRecorder[0] = NULL;
        alogd("-------------stop cam0 rec end---------------");
    }

    if (pContext->mConfigParam.mCamEnable[1] && pContext->mConfigParam.mCamRecEnable[1] && pContext->mpRecorder[1])
    {
        alogd("-------------stop cam1 rec begin---------------");
        pContext->mpRecorder[1]->stop();
        pContext->mpRecorder[1] = NULL;
        alogd("-------------stop cam1 rec end---------------");
    }

    return result;
}

int startIseRec(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder;

    if (pContext->mConfigParam.mIseMoEnable && pContext->mConfigParam.mIseMoRecEnable[0])
    {
        alogd("-------------start ise-mo-0 rec begin---------------");
        pRecorder = prepareAVRecord(&pContext->mConfigParam.mIseMoRecParam[0], &pContext->mRecorderListener);
        pContext->mpRecorder[2] = pRecorder;
        pRecorder->setCameraProxy(pContext->mpIse->getRecordingProxy(), pContext->mIseChnId[0][0]);
        pRecorder->prepare();
        pRecorder->start();
        alogd("-------------start ise-mo-0 rec end---------------");
    }

    if (pContext->mConfigParam.mIseMoEnable && pContext->mConfigParam.mIseMoRecEnable[1])
    {
        alogd("-------------start ise-mo-1 rec begin---------------");
        pRecorder = prepareAVRecord(&pContext->mConfigParam.mIseMoRecParam[1], &pContext->mRecorderListener);
        pContext->mpRecorder[3] = pRecorder;
        pRecorder->setCameraProxy(pContext->mpIse->getRecordingProxy(), pContext->mIseChnId[0][1]);
        pRecorder->prepare();
        pRecorder->start();
        alogd("-------------start ise-mo-1 rec end---------------");
    }

    return result;
}

int stopIseRec(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder;

    if (pContext->mConfigParam.mIseMode && pContext->mConfigParam.mIseMoEnable && pContext->mConfigParam.mIseMoRecEnable[0])
    {
        alogd("-------------stop ise-mo-rec0 rec begin---------------");
        pContext->mpRecorder[2]->stop();
        pContext->mpRecorder[2] = NULL;
        alogd("-------------stop ise-mo-rec0 rec end---------------");
    }

    if (pContext->mConfigParam.mIseMode && pContext->mConfigParam.mIseMoEnable && pContext->mConfigParam.mIseMoRecEnable[1])
    {
        alogd("-------------stop ise-mo-rec1 rec begin---------------");
        pContext->mpRecorder[3]->stop();
        pContext->mpRecorder[3] = NULL;
        alogd("-------------stop ise-mo-rec1 rec end---------------");
    }

    return result;
}

int main(int argc, char *argv[])
{
    int result = 0;
    alogd("hello, sample_EyeseeIse2!");
    SampleEyeseeIseContext stContext;
    gpSampleContext = &stContext;
    //parse command line param
    if(stContext.ParseCmdLine(argc, argv) != NO_ERROR)
    {
        aloge("fatal error! command line param is wrong because of no config file! exit!");
        result = -1;
        return result;
    }
    //parse config file.
    if(stContext.loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail! exit!");
        result = -1;
        return result;
    }
    if(stContext.checkConfig() != SUCCESS)
    {
        aloge("fatal error! some configs is wrong! exit!");
        result = -1;
        return result;
    }

    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    alogd("---------------------start EyeseeIse2 sample-------------------------");

    //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();


    int camId;
    CameraParameters cameraParam;
    SIZE_S captureSize;
    SIZE_S previewSize;
    VO_LAYER hlay;

    //config vo
    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer. not display, but layer still exist!

    //config camera
    EyeseeCamera::clearCamerasConfiguration();
    CamDeviceParam *pCamDevParam;
    camId = 0;
    if (stContext.mConfigParam.mCamEnable[camId])
    {
        pCamDevParam = &stContext.mConfigParam.mCamDevParam[camId];
        configCamera(pCamDevParam);
    }
    camId = 1;
    if (stContext.mConfigParam.mCamEnable[camId])
    {
        pCamDevParam = &stContext.mConfigParam.mCamDevParam[camId];
        configCamera(pCamDevParam);
    }

    //start camera
    DispParam *pDispParam;
    camId = 0;
    if (stContext.mConfigParam.mCamEnable[camId])
    {
        pCamDevParam = &stContext.mConfigParam.mCamDevParam[camId];
        pDispParam = &stContext.mConfigParam.mDispParam[camId];
        startCam(camId, pCamDevParam, pDispParam, &stContext);
    }
    camId = 1;
    if (stContext.mConfigParam.mCamEnable[camId])
    {
        pCamDevParam = &stContext.mConfigParam.mCamDevParam[camId];
        pDispParam = &stContext.mConfigParam.mDispParam[camId];
        startCam(camId, pCamDevParam, pDispParam, &stContext);
    }

    //start ise
    if (stContext.mConfigParam.mIseMode)
    {
        startIse(&stContext);
    }

    //start ise take photo
    if (stContext.mConfigParam.mIseMoEnable)
    {
        startIseMoPhoto(&stContext);
    }

    //start cam take photo
    if (stContext.mConfigParam.mCamPhotoEnable[0] || stContext.mConfigParam.mCamPhotoEnable[1])
    {
        startCamPhoto(&stContext);
    }

    //start cam recorder
    if (stContext.mConfigParam.mCamRecEnable[0] || stContext.mConfigParam.mCamRecEnable[1])
    {
        startCamRec(&stContext);
    }

    //start ise recorder
    if (stContext.mConfigParam.mIseMode && stContext.mConfigParam.mIseMoEnable)
    {
        startIseRec(&stContext);
    }


    alogd("**********************wait user quit...*****************************");
    cdx_sem_down(&stContext.mSemExit);
    alogd("*************************user quit!!!*******************************");


    //stop ise recorder
    if (stContext.mConfigParam.mIseMoRecEnable[0] || stContext.mConfigParam.mIseMoRecEnable[1])
    {
        stopIseRec(&stContext);
    }

    //stop cam recorder
    if (stContext.mConfigParam.mCamRecEnable[0] || stContext.mConfigParam.mCamRecEnable[1])
    {
        stopCamRec(&stContext);
    }

    //stop ise
    if (stContext.mConfigParam.mIseMode)
    {
        stopIse(&stContext);
    }

    //stop camera
    camId = 0;
    if (stContext.mConfigParam.mCamEnable[camId])
    {
        pCamDevParam = &stContext.mConfigParam.mCamDevParam[camId];
        pDispParam = &stContext.mConfigParam.mDispParam[camId];
        stopCam(camId, pCamDevParam, pDispParam, &stContext);
    }
    camId = 1;
    if (stContext.mConfigParam.mCamEnable[camId])
    {
        pCamDevParam = &stContext.mConfigParam.mCamDevParam[camId];
        pDispParam = &stContext.mConfigParam.mDispParam[camId];
        stopCam(camId, pCamDevParam, pDispParam, &stContext);
    }

    // destruct UI layer
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;

    //exit mpp system
    AW_MPI_SYS_Exit(); 

    cout<<"bye, Sample_EyeseeIse2!"<<endl;
    return result;
}
