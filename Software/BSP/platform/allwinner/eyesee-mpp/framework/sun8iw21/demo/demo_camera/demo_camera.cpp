#define LOG_NDEBUG 0
#define LOG_TAG "demo_camera"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <memory.h>
#include <iostream>
#include <fcntl.h>

#include <Errors.h>

#include <camera.h>
#include <EyeseeCamera.h>
#include <vo/hwdisplay.h>
#include <media/mpi_vi.h>
#include <media/mm_comm_vi.h>
#include <mpi_sys.h>
#include <mpi_vo.h>

#define CHANNEL_ID  1
#define DEVICE_ID   1


using namespace std;
using namespace EyeseeLinux;

typedef struct CamDeviceInfo
{
    int devId;
    int chnId;
    int width;
    int height;

    EyeseeCamera *pCam;
    int hlay;
    struct view_info sur;

    bool started;
} CamDeviceInfo;

class CameraCallback : public EyeseeCamera::PictureCallback
{
public:
    CameraCallback(){ mpCam = NULL;}
    ~CameraCallback(){}

    virtual void onPictureTaken(int chnId, const void *data, int size, EyeseeCamera* pCamera);

private:
    EyeseeCamera *mpCam;
};

void CameraCallback::onPictureTaken(int chnId, const void * data, int size, EyeseeCamera* pCamera)
{
    alogd("channel %d picture data size %d", chnId, size);
    char name[256];
    snprintf(name, 256, "/home/pic_channel%d.jpg", chnId);
    int fd = open(name, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        aloge("open file /home/yuvdata.dat failed(%s)!", strerror(errno));
        return;
    }
    write(fd, data, size);
    close(fd);
}

static EyeseeCamera* openCameraDevice(int id)
{
    VI_DEV_ATTR_S pstDevAttr;
    EyeseeCamera *pCam;
    int ret;

    pCam = EyeseeCamera::open(id);
    if (pCam == NULL) {
        aloge("open EyeseeCamera error!\n");
        return NULL;
    }

    memset(&pstDevAttr, 0, sizeof(pstDevAttr));
    //pCam->setDeviceAttr(&pstDevAttr);

    ret = pCam->prepareDevice();
    if (ret != 0) {
        aloge("prepare device error!\n");
        goto ERR_PREPARE_DEV;
    }

    ret = pCam->startDevice();
    if (ret != 0) {
        aloge("startDevice error!\n");
        goto ERR_START_DEV;
    }

    return pCam;

ERR_START_DEV:
    pCam->releaseDevice();
ERR_PREPARE_DEV:
    EyeseeCamera::close(pCam);
    return NULL;
}

static void closeCameraDevice(EyeseeCamera* pCam)
{
    pCam->stopDevice();
    pCam->releaseDevice();
    EyeseeCamera::close(pCam);
}

static int openCameraChannel(EyeseeCamera* pCam, int id, int w, int h, int hlay)
{
    CameraParameters param;
    SIZE_S size;
    int ret;

    ret = pCam->openChannel(id, true);
    if (ret != NO_ERROR) {
        aloge("open channel error!\n");
        return -1;
    }
    ret = pCam->prepareChannel(id);
    if (ret != 0) {
        aloge("prepareChannel error!\n");
        goto ERR_SET_PARAM;
    }
    ret = pCam->getParameters(id, param);
    if (ret != 0) {
        aloge("getParameters error!\n");
        goto ERR_SET_PARAM;
    }
    size.Width = w;
    size.Height = h;
    param.setVideoSize(size);
    param.setVideoBufferNumber(5);
    param.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    ret = pCam->setParameters(id, param);
    if (ret != 0) {
        aloge("setParameters error!\n");
        goto ERR_SET_PARAM;
    }

    pCam->setChannelDisplay(id, hlay);

    ret = pCam->startChannel(id);
    if (ret != 0) {
        aloge("startChannel error!\n");
        goto ERR_START_CHN;
    }

    return 0;

ERR_START_CHN:
    pCam->releaseChannel(id);
ERR_SET_PARAM:
    pCam->closeChannel(id);
    return -1;
}

static void closeCameraChannel(EyeseeCamera* pCam, int id)
{
    pCam->stopChannel(id);
    pCam->releaseChannel(id);
    pCam->closeChannel(id);
}

static int openDispLayer(view_info *sur)
{
    int hlay;
    int ret;

    hlay = 0;
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
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    AW_MPI_VO_GetVideoLayerAttr(hlay, &stLayerAttr);
    stLayerAttr.stDispRect.X = sur->x;
    stLayerAttr.stDispRect.Y = sur->y;
    stLayerAttr.stDispRect.Width = sur->w;
    stLayerAttr.stDispRect.Height = sur->h;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &stLayerAttr);
    ret = AW_MPI_VO_OpenVideoLayer(hlay);
    if (ret != 0) 
    {
        aloge("open video layer error!");
        AW_MPI_VO_DisableVideoLayer(hlay);
        return -1;
    }
    return hlay;
}

static void closeDispLayer(int hlay)
{
    AW_MPI_VO_DisableVideoLayer(hlay);
}

int main(int argc, char *argv[])
{
    if ((argc - 1) % 4 != 0 || argc == 1) {
        fprintf(stderr, "usage: camtest devid chnid width height ...\n");
        return -1;
    }
    fprintf(stderr, "camera demo start\n");

    int i = 0;
    int ret = 0;

    MPP_SYS_CONF_S nSysConf;
    memset(&nSysConf, 0, sizeof(MPP_SYS_CONF_S));
    nSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&nSysConf);
    AW_MPI_SYS_Init();
    //hw_display_init();
    AW_MPI_VO_Enable(0);
    hwd_layer_close(4); //close ui layer.
    //hwd_layer_close(8);

    vector<CamDeviceInfo> info;
    for (i = 1; i < argc; ) {
        CamDeviceInfo ci;
        memset(&ci, 0, sizeof(ci));
        ci.devId = strtol(argv[i++], NULL, 10);
        ci.chnId = strtol(argv[i++], NULL, 10);
        ci.width = strtol(argv[i++], NULL, 10);
        ci.height = strtol(argv[i++], NULL, 10);
        if (i == 5) {
            ci.sur.x = 0;
            ci.sur.y = 0;
            ci.sur.w = 640;
            ci.sur.h = 400;
        } else if (i == 9) {
            ci.sur.x = 640;
            ci.sur.y = 0;
            ci.sur.w = 640;
            ci.sur.h = 400;
        } else if (i == 13) {
            ci.sur.x = 0;
            ci.sur.y = 400;
            ci.sur.w = 640;
            ci.sur.h = 400;
        } else if (i == 17) {
            ci.sur.x = 640;
            ci.sur.y = 400;
            ci.sur.w = 640;
            ci.sur.h = 400;
        }
        info.push_back(ci);
        alogd("dev9[%d], chn[%d], size[%dx%d]", ci.devId, ci.chnId, ci.width, ci.height);
    }

    for (vector<CamDeviceInfo>::iterator it = info.begin(); it != info.end(); ++it) {
        it->pCam = openCameraDevice(it->devId);
        if (it->pCam == NULL) {
            aloge("openCameraDevice error!");
            continue;
        }
        it->hlay = openDispLayer(&it->sur);
        if (it->hlay < 0) {
            aloge("openDispLayer error, hlay=%d!", it->hlay);
            closeCameraDevice(it->pCam);
            continue;
        }
        ret = openCameraChannel(it->pCam, it->chnId, it->width, it->height, it->hlay);
        if (ret != 0) {
            aloge("openCameraChannel error!");
            closeDispLayer(it->hlay);
            closeCameraDevice(it->pCam);
            continue;
        }
        it->started = true;
    }

#if 0
    while (1) {
        sleep(5);
    }
#else
    sleep(1);
    CameraCallback cb;
    for (vector<CamDeviceInfo>::iterator it = info.begin(); it != info.end(); ++it) {
        if (it->started) {
            it->pCam->takePicture(it->chnId, NULL, NULL, NULL, &cb, NULL);
        }
    }
    sleep(3);
#endif

    for (vector<CamDeviceInfo>::iterator it = info.begin(); it != info.end(); ++it) {
        if (it->started) {
            closeCameraChannel(it->pCam, it->chnId);
            closeDispLayer(it->hlay);
            closeCameraDevice(it->pCam);
        }
    }

    while(info.begin() != info.end()) {
        info.erase(info.begin());
    }

    AW_MPI_VO_Disable(0);
    AW_MPI_SYS_Exit();
    //hw_display_deinit();
    return ret;
}

