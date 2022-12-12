/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : EyeseeBDII.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/05
  Last Modified :
  Description   : BDII module.
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "EyeseeBDII"
#include <utils/plat_log.h>

#include "CveBDII.h"
#include "EyeseeBDII.h"


using namespace std;

namespace EyeseeLinux {

class EyeseeBDII;
class EyeseeBDIIContext : public DataListener
                        , public NotifyListener
{
public:
    EyeseeBDIIContext(EyeseeBDII *pBDII) : mpBDII(pBDII) {}
    ~EyeseeBDIIContext(){}

    virtual void postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize);
    virtual void notify(int msgType, int chnId, int ext1, int ext2);

private:
    EyeseeBDII *mpBDII;
    Mutex mLock;
};

void EyeseeBDIIContext::postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize)
{
    AutoMutex lock(mLock);
    EyeseeBDII::postEventFromNative(mpBDII, msgType, chnId, (int)dataSize, 0, &dataPtr);
}

void EyeseeBDIIContext::notify(int msgType, int chnId, int ext1, int ext2)
{
    AutoMutex lock(mLock);
    EyeseeBDII::postEventFromNative(mpBDII, msgType, chnId, ext1, ext2, NULL);
}


void EyeseeBDII::EventHandler::handleMessage(const CallbackMessage &msg)
{
    switch (msg.what)
    {
        case CAMERA_MSG_ERROR :
        {
            aloge("Channel %d error %d", msg.arg1, msg.arg2);
            if (mpBDII->mpErrorCallback != NULL)
            {
                mpBDII->mpErrorCallback->onError(msg.arg1, msg.arg2, mpBDII);
            }
            return;
        }

        case CAMERA_MSG_INFO:
        {
            int chnId = msg.arg1;
            CameraMsgInfoType what = (CameraMsgInfoType)msg.arg2;
            int extra = 0;
            if(msg.extArgs.size() > 0)
            {
                extra = msg.extArgs[0];
            }
            alogd("msg info[0x%x]", what);
            return;
        }

        case CAMERA_MSG_BDII_DATA:
        {
            if(mpBDII->mpBDIIDataCallback != NULL)
            {
                if(msg.mDataPtr)
                {
                    if(msg.mDataPtr->getPointer() != NULL)
                    {
                        int chnId = msg.arg1;
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        mpBDII->mpBDIIDataCallback->onBDIIData(chnId, (const CVE_BDII_RES_OUT_S*)pData, mpBDII);
                    }
                }
            }
            break;
        }

        default:
        {
            aloge("Unknown message type %d", msg.what);
            return;
        }
    }
}

void EyeseeBDII::postEventFromNative(EyeseeBDII *pBDII, int what, int arg1, int arg2, int arg3, const std::shared_ptr<CMediaMemory>* pDataPtr)
{
    CallbackMessage msg;

    msg.what = what;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    msg.extArgs.push_back(arg3);
    //msg.obj = obj;
    if(pDataPtr)
    {
        msg.mDataPtr = *pDataPtr;
    }
    pBDII->mpEventHandler->post(msg);
}


unsigned int EyeseeBDII::gBDIIIdCounter = BDIIIdPrefixMark | 0x00;

EyeseeBDII::EyeseeBDII()
    : mBDII_Id(gBDIIIdCounter++)
    , mpCveBDII(NULL)
    , mpBDIIDataCallback(NULL)
    , mpErrorCallback(NULL)
{
    alogd("EyeseeBDII constructor!");
    mpEventHandler = new EventHandler(this);
    mpNativeContext = new EyeseeBDIIContext(this);
    mpCveBDII = new CveBDII(mBDII_Id);
    if (mpCveBDII)
    {
        mpCveBDII->setDataListener(mpNativeContext);
        mpCveBDII->setNotifyListener(mpNativeContext);
    }
}

EyeseeBDII::~EyeseeBDII()
{
    alogd("EyeseeBDII destructor!");
    if (mpCveBDII)
    {
        delete mpCveBDII;
    }
    if (mpEventHandler)
    {
        delete mpEventHandler;
    }
    if (mpNativeContext)
    {
        delete mpNativeContext;
    }
}

EyeseeBDII *EyeseeBDII::open()
{
    return new EyeseeBDII();
}

void EyeseeBDII::close(EyeseeBDII *pBDII)
{
    delete pBDII;
}

status_t EyeseeBDII::setCamera(std::vector<BDIICameraChnInfo>& cameraChannels)
{
    if (mpCveBDII)
    {
        return mpCveBDII->setCamera(cameraChannels);
    }
    return UNKNOWN_ERROR;
}

status_t EyeseeBDII::setBDIIConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &param)
{
    if (mpCveBDII)
    {
        return mpCveBDII->setBDIIConfigParams(param);
    }
    return UNKNOWN_ERROR;
}

status_t EyeseeBDII::getBDIIConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &param)
{
    if (mpCveBDII)
    {
        return mpCveBDII->getBDIIConfigParams(param);
    }
    return UNKNOWN_ERROR;
}

status_t EyeseeBDII::debugYuvDataOut(bool saveOut, std::vector<BDIIDebugFileOutString>&vec)
{
    if (mpCveBDII)
    {
        mpCveBDII->setDebugYuvDataOut(saveOut, vec);
    }
    return UNKNOWN_ERROR;
}

status_t EyeseeBDII::prepare()
{
    return mpCveBDII->prepare();
}

status_t EyeseeBDII::start()
{
    return mpCveBDII->start();
}

status_t EyeseeBDII::stop()
{
    return mpCveBDII->stop();
}

status_t EyeseeBDII::setDisplay(int hlay)
{
    return mpCveBDII->setDisplay(hlay);
}

status_t EyeseeBDII::setPreviewRotation(int rotation)
{
    return mpCveBDII->setPreviewRotation(rotation);
}
void EyeseeBDII::setBDIIDataCallback(BDIIDataCallback *pCb)
{
    mpBDIIDataCallback = pCb;
    if(mpBDIIDataCallback)
    {
        mpCveBDII->enableMessage(CAMERA_MSG_BDII_DATA);
    }
    else
    {
        mpCveBDII->disableMessage(CAMERA_MSG_BDII_DATA);
    }
}

void EyeseeBDII::setErrorCallback(ErrorCallback *pCb)
{
    mpErrorCallback = pCb;
}


}; /* namespace EyeseeLinux */

