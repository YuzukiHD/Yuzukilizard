#define LOG_TAG "sample_run_nna"
#include <utils/plat_log.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
#include "media/mm_comm_vi.h"
#include "media/mm_comm_region.h"
#include "media/mpi_vi.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"
#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <utils/VIDEO_FRAME_INFO_S.h>
#include <confparser.h>
#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_isp.h>
#include <mm_comm_venc.h>
#include <mpi_venc.h>
#include <mpi_region.h>
#include "aw_object_detection.h"
#include "det_res.h"

#define DISABLE_OSD
#define DISABLE_RECORD
//#define DISABLE_VO
//#define BYPSASS_NPU
static int pipe_fd = -1;
static int npu_det_fps = 0;
static float npu_check_time = 0;

extern int yolov3_tiny_odet(int argc, char **argv, detect_res_t *box);
#define VIPP_INPUT_FPS       30
#define H265_ENCODE_STREAM
#define DBG(fmt, ...)        do { printf("%s line %d, "fmt, __func__, __LINE__, ##__VA_ARGS__); } while (0)
#define MAX_FILE_PATH_SIZE   (256)
#define VIPP2VO_NUM          (3)
#define LCD_WIDTH            (720)
#define LCD_HEIGHT           (1280)
//#define NPU_DET_PIC_WIDTH    352
//#define NPU_DET_PIC_HEIGHT   198
#define NPU_DET_PIC_WIDTH    416
#define NPU_DET_PIC_HEIGHT   416

#define ENCODER_PIC_WIDTH    1920
#define ENCODER_PIC_HEIGH    1080

#ifdef __cplusplus
extern "C" {
#endif

float run_single_object_det(VIDEO_FRAME_INFO_S *pFrameInfo, int width, int height, detect_res_t *res, float *single_det_time);
void run_single_object_det_old(unsigned char *image_data, int width, int height);

#ifdef __cplusplus
}
#endif
typedef struct __sample_run_nna_vipp2vo_config
{
    int mDevNum;
    int mCaptureWidth;
    int mCaptureHeight;
    int mDisplayX;
    int mDisplayY;
    int mDisplayWidth;
    int mDisplayHeight;
    int mLayerNum;
    int mValid;
} sample_run_nna_vipp2vo_config_t;

typedef struct __sample_run_nna_channel_config
{
    sample_run_nna_vipp2vo_config_t mVIPP2VOConfigArray[VIPP2VO_NUM];
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;
    ISP_CFG_MODE mIspWdrSetting;
} sample_run_nna_channel_config_t;

typedef struct __vipp2vo_linkinfo
{
    VI_DEV mVIDev;
    VI_CHN mVIChn;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_CHN mVOChn;
} vipp2vo_lininfo_t;

typedef struct __venc_channel_settings
{
    VENC_CHN mVeChn;
    VENC_CHN_ATTR_S   mVEncChnAttr;
    VENC_RC_PARAM_S   mVEncRcParam;
    VENC_FRAME_RATE_S mVencFrameRateConfig;
} venc_channel_setting_t;

typedef struct __sample_run_nna_context_s
{
    sample_run_nna_channel_config_t mConfigPara;
    vipp2vo_lininfo_t               mLinkInfoArray[VIPP2VO_NUM];
    venc_channel_setting_t          venc_setting;
    MPP_SYS_CONF_S                  mSysConf;
    ISP_DEV                         mIspDev;
    VO_DEV                          mVoDev;
    pthread_t                       nna_detect_thread;
    pthread_t                       ven_record_thread;
} sample_run_nna_context_t;

typedef struct __box_s
{
    int      x;
    int      y;
    int      width;
    int      height;
    int      label;
} aw_det_box_t;

void npu_deinit_models(void);
int npu_init_models(void);
static void draw_boxes(sample_run_nna_context_t *pctx, int nums, aw_det_box_t *boxes, aw_det_box_t *boxes_enc);
static int config_venc_channel(sample_run_nna_context_t *pctx);
static void load_vipp_parameters(sample_run_nna_context_t *pctx)
{
    DBG("run start!\n");
    sample_run_nna_channel_config_t *pctx_chan = &pctx->mConfigPara;

    // basic setting for resources
    // display engine 0, ISP0.
    pctx->nna_detect_thread = 0;
    pctx->ven_record_thread = 0;
    pctx->mVoDev            = 0;
    pctx->mIspDev           = 0;

#if 0
    // this is the default configuration.
    pctx_chan->mVIPP2VOConfigArray[0].mValid         = 1;
    pctx_chan->mVIPP2VOConfigArray[0].mDevNum = 0;
    pctx_chan->mVIPP2VOConfigArray[0].mCaptureWidth  = ENCODER_PIC_WIDTH;
    pctx_chan->mVIPP2VOConfigArray[0].mCaptureHeight = ENCODER_PIC_HEIGH;
    pctx_chan->mVIPP2VOConfigArray[0].mDisplayX = 0;
    pctx_chan->mVIPP2VOConfigArray[0].mDisplayY = 0;
    pctx_chan->mVIPP2VOConfigArray[0].mDisplayWidth = LCD_WIDTH;
    pctx_chan->mVIPP2VOConfigArray[0].mDisplayHeight = LCD_HEIGHT;
    pctx_chan->mVIPP2VOConfigArray[0].mLayerNum = 0;
    pctx_chan->mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pctx_chan->mFrameRate = VIPP_INPUT_FPS;
    pctx_chan->mDispType = VO_INTF_LCD;
    pctx_chan->mDispSync = VO_OUTPUT_NTSC;
    pctx_chan->mIspWdrSetting = NORMAL_CFG;
#else
    // the default configuration rewrite by following.
    // v833 DE has 3 channels, channel 0,1 are scaler layers which support video.
    // and channel 2 is UI Channels which only support RGB. each channel supports
    // 4 layers. so layer 0~7 is Video layer. and 8~11 is RGB UI Layer.

    // configuration vipp channel 0, used for encoder.
    pctx_chan->mVIPP2VOConfigArray[0].mValid         = 1;                  //vipp0 function enable.
    pctx_chan->mVIPP2VOConfigArray[0].mDevNum        = 0;                  //vipp0 as source.
    pctx_chan->mVIPP2VOConfigArray[0].mCaptureWidth  = ENCODER_PIC_WIDTH;  //vipp0 scaler&capture output
    pctx_chan->mVIPP2VOConfigArray[0].mCaptureHeight = ENCODER_PIC_HEIGH;  //high res.picture.
    pctx_chan->mVIPP2VOConfigArray[0].mDisplayX      = 0;                  //Xposition on screen.
    pctx_chan->mVIPP2VOConfigArray[0].mDisplayY      = 0;                  //Yposition on screen.
    pctx_chan->mVIPP2VOConfigArray[0].mDisplayWidth  = 0;                  //Display Width on screen.
    pctx_chan->mVIPP2VOConfigArray[0].mDisplayHeight = 0;                  //Display height on screen.
    pctx_chan->mVIPP2VOConfigArray[0].mLayerNum      = 0;                  //channel 0, layer 0 on DE.

    // configuration vipp channel 1, used for preview
    pctx_chan->mVIPP2VOConfigArray[1].mValid         = 1;                  //vipp1 function enable.
    pctx_chan->mVIPP2VOConfigArray[1].mDevNum        = 4;                  //vipp1 as source.
    pctx_chan->mVIPP2VOConfigArray[1].mCaptureWidth  = 1280;               //vipp1 scaler&capture output
    pctx_chan->mVIPP2VOConfigArray[1].mCaptureHeight = 720;                //high res.picture.
    pctx_chan->mVIPP2VOConfigArray[1].mDisplayX      = 0;                  //Xposition on screen.
    pctx_chan->mVIPP2VOConfigArray[1].mDisplayY      = 0;                  //Yposition on screen.
    pctx_chan->mVIPP2VOConfigArray[1].mDisplayWidth  = LCD_WIDTH;          //Display Width on screen.
    pctx_chan->mVIPP2VOConfigArray[1].mDisplayHeight = LCD_HEIGHT;         //Display height on screen.
    pctx_chan->mVIPP2VOConfigArray[1].mLayerNum      = 4;                  //channel 1, layer 0 on DE, cant be same with vipp0 for scaler difference.

    // configuration vipp channel 2, used for npu.
    pctx_chan->mVIPP2VOConfigArray[2].mValid         = 1;                  //vipp2 function enable.
    pctx_chan->mVIPP2VOConfigArray[2].mDevNum        = 8;                  //vipp2 as source.
    pctx_chan->mVIPP2VOConfigArray[2].mCaptureWidth  = NPU_DET_PIC_WIDTH;  //vipp1 scaler&capture output
    pctx_chan->mVIPP2VOConfigArray[2].mCaptureHeight = NPU_DET_PIC_HEIGHT; //high res.picture.
    pctx_chan->mVIPP2VOConfigArray[2].mDisplayX      = 0;                  //Xposition on screen.
    pctx_chan->mVIPP2VOConfigArray[2].mDisplayY      = 0;                  //Yposition on screen.
    pctx_chan->mVIPP2VOConfigArray[2].mDisplayWidth  = 0;                  //Display Width on screen.
    pctx_chan->mVIPP2VOConfigArray[2].mDisplayHeight = 0;                  //Display height on screen.
    pctx_chan->mVIPP2VOConfigArray[2].mLayerNum      = 5;                  //channel 1, layer 1 on DE, can be same with vipp1 for scaler same.

    // other settings.
    // nna only support nv21 format.
    pctx_chan->mPicFormat     = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pctx_chan->mDispType      = VO_INTF_LCD;
    pctx_chan->mDispSync      = VO_OUTPUT_NTSC;
    pctx_chan->mFrameRate     = VIPP_INPUT_FPS;
    pctx_chan->mIspWdrSetting = NORMAL_CFG;
#endif

    DBG("run finish!\n");
    return;
}

static int mpp_init(sample_run_nna_context_t *pctx)
{
    int result;

    DBG("run start!\n");
    memset(&pctx->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pctx->mSysConf.nAlignWidth = 32;

    AW_MPI_SYS_SetConf(&pctx->mSysConf);

    result = AW_MPI_SYS_Init();
    if (result)
    {
        DBG("fatal error, mpp init failure.\n");
        return -1;
    }

    DBG("run finish!\n");
    return 0;
}

static void mpp_exit(void)
{
    DBG("run start!\n");
    // exit mpp system
    AW_MPI_SYS_Exit();
    DBG("run finish!\n");
}

static ERRORTYPE sample_run_nna_callback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    sample_run_nna_context_t *pctx __attribute__((__unused__)) = (sample_run_nna_context_t *)cookie;
    if (MOD_ID_VIU == pChn->mModId)
    {
        switch (event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                DBG("receive vi timeout. vipp:%d, chn:%d\n", pChn->mDevId, pChn->mChnId);
                break;
            }
            default:
            {
                DBG("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!\n", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else
    {
        DBG("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!\n", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
    }

    return SUCCESS;
}

static ERRORTYPE vocallback_wrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    sample_run_nna_context_t *pctx __attribute__((__unused__)) = (sample_run_nna_context_t *)cookie;
    if (MOD_ID_VOU == pChn->mModId)
    {
        DBG("VO callback: VO Layer[%d] chn[%d] event:%d\n", pChn->mDevId, pChn->mChnId, event);
        switch (event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pFrameInfo = (VIDEO_FRAME_INFO_S *)pEventData;
                DBG("vo layer[%d] release frame id[0x%x]!\n", pChn->mDevId, pFrameInfo->mId);
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S *)pEventData;
                DBG("vo layer[%d] report video display size[%dx%d].\n", pChn->mDevId, pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                DBG("vo layer[%d] report rendering start.\n", pChn->mDevId);
                break;
            }
            default:
            {
                DBG("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!.\n", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else
    {
        DBG("fatal error! why modId[0x%x]?.\n", pChn->mModId);
        ret = FAILURE;
    }

    return ret;
}

static int destory_vipp_display2_vo(int channel, sample_run_nna_context_t *pctx)
{
    ERRORTYPE result;

    DBG("run start!\n");

    if (pctx == NULL)
    {
        DBG("parameter is wrong.\n");
        return FAILURE;
    }

    sample_run_nna_vipp2vo_config_t *pcfg = &pctx->mConfigPara.mVIPP2VOConfigArray[channel];
    vipp2vo_lininfo_t *plnk = &pctx->mLinkInfoArray[channel];

    if (pcfg->mValid == 0)
    {
        DBG("channel %d not enabled.!\n", channel);
        return SUCCESS;
    }

    /* stop vo channel, vi channel */
    result = AW_MPI_VO_StopChn(plnk->mVoLayer, plnk->mVOChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO StopChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DisableVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DisableVirChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VO_DestroyChn(plnk->mVoLayer, plnk->mVOChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    plnk->mVOChn = MM_INVALID_CHN;
    /* disable vo layer */
    result = AW_MPI_VO_DisableVideoLayer(plnk->mVoLayer);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    plnk->mVoLayer = -1;

    // wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then vdec can free frame buffer.
    // usleep(50*1000);
    sleep(1);

    result = AW_MPI_VI_DestroyVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVirChn fail!.\n");
        return FAILURE;
    }

    result = AW_MPI_ISP_Stop(pctx->mIspDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DisableVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DestroyVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    plnk->mVIDev = MM_INVALID_DEV;
    plnk->mVIChn = MM_INVALID_CHN;

    DBG("run finish!\n");
    return SUCCESS;
}

static int destory_venc_channel(int channel, sample_run_nna_context_t *pctx)
{
    ERRORTYPE result;

    sample_run_nna_vipp2vo_config_t *pcfg = &pctx->mConfigPara.mVIPP2VOConfigArray[channel];
    vipp2vo_lininfo_t *plnk = &pctx->mLinkInfoArray[channel];

    DBG("run start!\n");

    if (pcfg->mValid == 0)
    {
        DBG("channel %d not enabled.!\n", channel);
        return SUCCESS;
    }

    if (pctx == NULL)
    {
        DBG("parameter is wrong.\n");
        return FAILURE;
    }

    result = AW_MPI_VENC_StopRecvPic(pctx->venc_setting.mVeChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VENC StopRecvPic fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DisableVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DisableVirChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VENC_ResetChn(pctx->venc_setting.mVeChn);
    if (result != SUCCESS)
    {
        DBG("VENC Reset Chn error!\n");
        return FAILURE;
    }

    AW_MPI_VENC_DestroyChn(pctx->venc_setting.mVeChn);
    if (result != SUCCESS)
    {
        DBG("VENC Destroy Chn error!\n");
        return FAILURE;
    }

    // wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then vdec can free frame buffer.
    // usleep(50*1000);
    sleep(1);

    result = AW_MPI_VI_DestroyVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVirChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_ISP_Stop(pctx->mIspDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! ISP Stop fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DisableVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DisableVipp fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DestroyVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVipp fail!\n");
        return FAILURE;
    }

    plnk->mVIDev = MM_INVALID_DEV;
    plnk->mVIChn = MM_INVALID_CHN;

    DBG("run finish!\n");
    return SUCCESS;
}

static int npu_thread_destory(int channel, sample_run_nna_context_t *pctx)
{
    ERRORTYPE result;

    DBG("run start!\n");

    if (pctx == NULL)
    {
        DBG("parameter is wrong.\n");
        return FAILURE;
    }

    sample_run_nna_vipp2vo_config_t *pcfg = &pctx->mConfigPara.mVIPP2VOConfigArray[channel];
    vipp2vo_lininfo_t *plnk = &pctx->mLinkInfoArray[channel];

    if (pcfg->mValid == 0)
    {
        DBG("channel %d not enabled.!\n", channel);
        return SUCCESS;
    }

    /* stop vo channel, vi channel */
    result = AW_MPI_VO_StopChn(plnk->mVoLayer, plnk->mVOChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO StopChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DisableVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DisableVirChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VO_DestroyChn(plnk->mVoLayer, plnk->mVOChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    plnk->mVOChn = MM_INVALID_CHN;
    /* disable vo layer */
    result = AW_MPI_VO_DisableVideoLayer(plnk->mVoLayer);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    plnk->mVoLayer = -1;

    // wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then vdec can free frame buffer.
    // usleep(50*1000);
    sleep(1);

    result = AW_MPI_VI_DestroyVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVirChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_ISP_Stop(pctx->mIspDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DisableVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DestroyVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    plnk->mVIDev = MM_INVALID_DEV;
    plnk->mVIChn = MM_INVALID_CHN;

    DBG("run finish!\n");
    return SUCCESS;
}

static detect_res_t det_res;

static void dump_detect_result(detect_res_t *res)
{
    int i;

    //printf("%d objects detectd.\n", res->num);
    for (i = 0; i < res->num && i < MAX_OBJECT_DET_NUM; i ++)
    {
        int width  = res->objs[i].right_down_x - res->objs[i].left_up_x;
        int height = res->objs[i].right_down_y - res->objs[i].left_up_y;

        res->objs[i].width  = width;
        res->objs[i].height = height;
		printf("label:%d, prob:%f, tlx:%d, tly:%d, rbx:%d, rby:%d, w:%d,h:%d.\n",res->objs[i].label, res->objs[i].prob, \
				res->objs[i].left_up_x, res->objs[i].left_up_y, res->objs[i].right_down_x, res->objs[i].right_down_y, width, height);
    }
}

static unsigned int load_file(const char *name, void *dst)
{
    FILE *fp = fopen(name, "rb");
    unsigned int size = 0;
    size_t read_count = 0;

    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);

        fseek(fp, 0, SEEK_SET);
        read_count = fread(dst, size, 1, fp);
        if(read_count != 1)
        {
            printf("Read file %s failed.\n", name);
        }

        fclose(fp);
    } else {
        printf("Load file %s failed.\n", name);
    }

    return size;
}

static unsigned char *yuv_buffer = NULL;
static void object_detection_feed(VIDEO_FRAME_INFO_S *pFrameInfo)
{
    char *argv[4] = {NULL, NULL, NULL, NULL};

#if 1
    VideoFrameBufferSizeInfo FrameSizeInfo;

    memset(&FrameSizeInfo, 0x00, sizeof(FrameSizeInfo));
    getVideoFrameBufferSizeInfo(pFrameInfo, &FrameSizeInfo);
    int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};

    if (yuv_buffer == NULL)
    {
        DBG("fatal error, malloc yuvbuffer failure.\n");
        while (1);
    }

    unsigned char *pcur = yuv_buffer;
    if (pFrameInfo->VFrame.mpVirAddr[0])
    {
        memcpy(pcur, pFrameInfo->VFrame.mpVirAddr[0], yuvSize[0]);
        pcur += yuvSize[0];
    }

    if (pFrameInfo->VFrame.mpVirAddr[1])
    {
        memcpy(pcur, pFrameInfo->VFrame.mpVirAddr[1], yuvSize[1]);
        pcur += yuvSize[1];
    }

    if (pFrameInfo->VFrame.mpVirAddr[2])
    {
        memcpy(pcur, pFrameInfo->VFrame.mpVirAddr[2], yuvSize[2]);
        pcur += yuvSize[2];
    }

#else
    load_file("/mnt/sdcard/Y.bin", yuv_buffer);
    load_file("/mnt/sdcard/UV.bin", yuv_buffer + (NPU_DET_PIC_WIDTH * NPU_DET_PIC_HEIGHT));
#endif

    argv[0] = "null";
    argv[1] = "/mnt/sdcard/network_binary.nb";
    argv[2] = yuv_buffer;
    argv[3] = yuv_buffer + (NPU_DET_PIC_WIDTH * NPU_DET_PIC_HEIGHT);
    
    yolov3_tiny_odet(4, argv, &det_res);

    //static struct timeval start, end;

    //float single_pic_time;
    //gettimeofday(&start, NULL);
    //npu_check_time = run_single_object_det(pFrameInfo, NPU_DET_PIC_WIDTH, NPU_DET_PIC_HEIGHT, &det_res, &single_pic_time);
    //gettimeofday(&end, NULL);

    //signed long long time_sum = ((1000.f * 1000 * end.tv_sec + end.tv_usec) - (1000.f * 1000 * start.tv_sec + start.tv_usec));
    //npu_check_time = time_sum / 1000;   // convert to ms.

	//DBG("body detect average cost %f ms, this cost %f ms.\n", npu_check_time, single_pic_time);
    dump_detect_result(&det_res);

    return;
}

static void convert_res_to_box_preview(aw_det_box_t *boxes)
{
    int num = det_res.num;
    int i;

    for (i = 0; i < num; i ++)
    {
        boxes[i].x      = det_res.objs[i].left_up_x * 1280 / NPU_DET_PIC_WIDTH - 260;
        boxes[i].y      = det_res.objs[i].left_up_y * 720 / NPU_DET_PIC_HEIGHT;
        boxes[i].width  = det_res.objs[i].width * 1280 / NPU_DET_PIC_WIDTH;
        boxes[i].height = det_res.objs[i].height * 720 / NPU_DET_PIC_HEIGHT;
        boxes[i].label  = det_res.objs[i].label;
        boxes[i].label  --;
    }

    return;
}

static void convert_res_to_box_encoder(aw_det_box_t *boxes)
{
    int num = det_res.num;
    int i;

    for (i = 0; i < num; i ++)
    {
        boxes[i].x      = det_res.objs[i].left_up_x * 1920 / NPU_DET_PIC_WIDTH;
        boxes[i].y      = det_res.objs[i].left_up_y * 1080 / NPU_DET_PIC_HEIGHT;
        boxes[i].width  = det_res.objs[i].width * 1920 / NPU_DET_PIC_WIDTH;
        boxes[i].height = det_res.objs[i].height * 1080 / NPU_DET_PIC_HEIGHT;
        boxes[i].label  = det_res.objs[i].label;
        boxes[i].label  --;
    }

    return;
}

static void save_raw_picture(int picno, int width, int height, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    int len;
    char pic_file_path[256];

    memset(pic_file_path, 0x00, 256);
    snprintf(pic_file_path, 256, "%s/pic[%d-%dx%d].nv12", "/mnt/sdcard", picno, width, height);

    FILE *fp = fopen(pic_file_path, "wb+");
    if (fp != NULL)
    {
        int i;
        VideoFrameBufferSizeInfo FrameSizeInfo;

        getVideoFrameBufferSizeInfo(pFrameInfo, &FrameSizeInfo);
        int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
        for (i = 0; i < 3; i ++)
        {
            if (pFrameInfo->VFrame.mpVirAddr[i] != NULL)
            {
                len = fwrite(pFrameInfo->VFrame.mpVirAddr[i], 1, yuvSize[i], fp);
                if (len != yuvSize[i])
                {
                    DBG("fatal error! fwrite fail,[%d]!=[%d], virAddr[%d]=[%p].\n", len, yuvSize[i], i, pFrameInfo->VFrame.mpVirAddr[i]);
                }
                DBG("virAddr[%d]=[%p], length=[%d].\n", i, pFrameInfo->VFrame.mpVirAddr[i], yuvSize[i]);
            }
        }

       fflush(fp);
       fsync(fileno(fp));
       fclose(fp);
       DBG("store raw frame in file[%s].\n", pic_file_path);
    }
    else
    {
        DBG("fatal error! open file[%s] fail!\n", pic_file_path);
    }

    return;
}

static int npu_config_and_run(int channel, sample_run_nna_context_t *pctx)
{
    ERRORTYPE result = SUCCESS;

    DBG("run start!\n");

    if (pctx == NULL)
    {
        DBG("parameter is wrong.\n");
        result = FAILURE;
        goto _err0;
    }

    sample_run_nna_vipp2vo_config_t *pcfg = &pctx->mConfigPara.mVIPP2VOConfigArray[channel];
    vipp2vo_lininfo_t *plnk = &pctx->mLinkInfoArray[channel];

    if (pcfg->mValid == 0)
    {
        DBG("channel %d not enabled.!\n", channel);
        result = SUCCESS;
        goto _err0;
    }

    // Create virtual channel [0] based on specify vipp
    plnk->mVIDev = pcfg->mDevNum;
    plnk->mVIChn = 0;

    DBG("vipp[%d] vir_chn[%d] creating.\n", plnk->mVIDev, plnk->mVIChn);

    // create a VIPP hardware channel.
    result = AW_MPI_VI_CreateVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! AW_MPI_VI CreateVipp:%d failed\n", plnk->mVIDev);
        result = FAILURE;
        goto _err0;
    }

    MPPCallbackInfo cbinfo;

    memset(&cbinfo, 0x00, sizeof(cbinfo));
    cbinfo.cookie = (void *)pctx;
    cbinfo.callback = (MPPCallbackFuncType)&sample_run_nna_callback;
    result = AW_MPI_VI_RegisterCallback(plnk->mVIDev, &cbinfo);
    if (result != SUCCESS)
    {
        DBG("fatal error! vipp[%d] RegisterCallback failed.\n", plnk->mVIDev);
        result = FAILURE;
        goto _err1;
    }

    VI_ATTR_S vi_attr;
    memset(&vi_attr, 0x00, sizeof(vi_attr));
    result = AW_MPI_VI_GetVippAttr(plnk->mVIDev, &vi_attr);
    if (result != SUCCESS)
    {
        DBG("fatal error! AW_MPI_VI GetVippAttr failed.\n");
        result = FAILURE;
        goto _err1;
    }

    memset(&vi_attr, 0, sizeof(VI_ATTR_S));
    vi_attr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    vi_attr.memtype = V4L2_MEMORY_MMAP;
    vi_attr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pctx->mConfigPara.mPicFormat);
    vi_attr.format.field = V4L2_FIELD_NONE;
    vi_attr.format.colorspace = V4L2_COLORSPACE_JPEG;
    vi_attr.format.width = pcfg->mCaptureWidth;
    vi_attr.format.height = pcfg->mCaptureHeight;
    vi_attr.nbufs = 5;
    vi_attr.nplanes = 2;
    vi_attr.drop_frame_num = 0; // drop 2 second video data, default=0

    vi_attr.use_current_win = channel ? 1 : 0;

    vi_attr.fps = pctx->mConfigPara.mFrameRate;
    result = AW_MPI_VI_SetVippAttr(plnk->mVIDev, &vi_attr);
    if (result != SUCCESS)
    {
        DBG("fatal error! AW_MPI_VI SetVippAttr:%d failed.\n", plnk->mVIDev);
        result = FAILURE;
        goto _err1;
    }

    /* open isp */
    AW_MPI_ISP_Run(pctx->mIspDev);

    result = AW_MPI_VI_EnableVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! enableVipp fail!\n");
        result = FAILURE;
        goto _err1;
    }

    // Create a virtual channel on specify VIPP.
    result = AW_MPI_VI_CreateVirChn(plnk->mVIDev, plnk->mVIChn, NULL);
    if (result != SUCCESS)
    {
        DBG("fatal error! createVirChn[%d] fail!\n", plnk->mVIChn);
        result = FAILURE;
        goto _err1;
    }

    // if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations must be after stream_on.
    AW_MPI_VI_SetVippMirror(plnk->mVIDev, 0);//0,1
    AW_MPI_VI_SetVippFlip(plnk->mVIDev, 0);//0,1

    // enable vo layer
    int hlay0 = pcfg->mLayerNum;
    if (SUCCESS != AW_MPI_VO_EnableVideoLayer(hlay0))
    {
        DBG("fatal error! enable video layer[%d] fail!\n", hlay0);
        result = FAILURE;
        goto _err1_5;
    }

    plnk->mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(plnk->mVoLayer, &plnk->mLayerAttr);
    plnk->mLayerAttr.stDispRect.X = pcfg->mDisplayX;
    plnk->mLayerAttr.stDispRect.Y = pcfg->mDisplayY;
    plnk->mLayerAttr.stDispRect.Width = pcfg->mDisplayWidth;
    plnk->mLayerAttr.stDispRect.Height = pcfg->mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(plnk->mVoLayer, &plnk->mLayerAttr);

    /* create vo channel and clock channel.
     * (because frame information has 'pts', there is no need clock channel now)
     */
    BOOL bSuccessFlag = FALSE;
    plnk->mVOChn = 0;
    while (plnk->mVOChn < VO_MAX_CHN_NUM)
    {
        // create VO component, and create compoent thread.
        result = AW_MPI_VO_CreateChn(plnk->mVoLayer, plnk->mVOChn);
        if (SUCCESS == result)
        {
            bSuccessFlag = TRUE;
            DBG("create vo channel[%d] success!\n", plnk->mVOChn);
            break;
        }
        else if (ERR_VO_CHN_NOT_DISABLE == result)
        {
            DBG("vo channel[%d] is exist, find next!\n", plnk->mVOChn);
            plnk->mVOChn++;
        }
        else
        {
            DBG("fatal error! create vo channel[%d] ret[0x%x]!\n", plnk->mVOChn, result);
            break;
        }
    }
    if (FALSE == bSuccessFlag)
    {
        plnk->mVOChn = MM_INVALID_CHN;
        DBG("fatal error! create vo channel fail!\n");
        result = FAILURE;
        goto _err2;
    }

    cbinfo.cookie = (void *)pctx;
    cbinfo.callback = (MPPCallbackFuncType)&vocallback_wrapper;
    AW_MPI_VO_RegisterCallback(plnk->mVoLayer, plnk->mVOChn, &cbinfo);
    AW_MPI_VO_SetChnDispBufNum(plnk->mVoLayer, plnk->mVOChn, 2);

    result = AW_MPI_VI_EnableVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! enableVirChn fail!\n");
        result = FAILURE;
        goto _err3;
    }

    AW_MPI_VO_StartChn(plnk->mVoLayer, plnk->mVOChn);

    static unsigned long long picture_counter = 0;
    static unsigned long long old_picture_counter = 0;
    static struct timeval start, end;
    memset(&start, 0x00, sizeof(start));
    memset(&end, 0x00, sizeof(end));

    while (1)
    {
        VIDEO_FRAME_INFO_S buffer;
        aw_det_box_t box_preview[20];
        aw_det_box_t box_encoder[20];

        memset(&buffer, 0, sizeof(buffer));

        gettimeofday(&end, NULL);
        signed long long time_sum = ((1000.f * 1000 * end.tv_sec + end.tv_usec) - (1000.f * 1000 * start.tv_sec + start.tv_usec));

        //DBG("time_sum = %lld, usec %d, sec %d.\n", time_sum, start.tv_usec, start.tv_sec);
        //DBG("usec %d, sec %d.\n", end.tv_usec, end.tv_sec);
        if ((time_sum >= (1000 * 1000)) && (start.tv_sec != 0 || start.tv_usec != 0))
        {
            start = end;
            npu_det_fps = picture_counter - old_picture_counter;
            old_picture_counter = picture_counter;
			DBG("frame no %d.\n", npu_det_fps);
        }

        AW_MPI_VI_GetFrame(plnk->mVIDev, 0, &buffer, 500);
        //AW_MPI_VO_SendFrame(plnk->mVoLayer, plnk->mVOChn, &buffer, 0);

#if 0
        int actualWidth = 0;
        int actualHeight = 0;
        actualWidth = buffer.VFrame.mOffsetRight - buffer.VFrame.mOffsetLeft;
        actualHeight = buffer.VFrame.mOffsetBottom - buffer.VFrame.mOffsetTop;
        DBG("width %d, height %d.\n", actualWidth, actualHeight);
#endif

		//save raw picture.
#if 0
        {
            int width, height;
            width  = buffer.VFrame.mOffsetRight - buffer.VFrame.mOffsetLeft;
            height = buffer.VFrame.mOffsetBottom - buffer.VFrame.mOffsetTop;
            save_raw_picture(picture_counter, width, height, &buffer);
        }
#endif

        if (picture_counter % 1 == 0)
        {
            object_detection_feed(&buffer);
#ifndef BYPSASS_NPU
            convert_res_to_box_preview(&box_preview[0]);
            convert_res_to_box_encoder(&box_encoder[0]);

            if (det_res.num)
            {
                draw_boxes(pctx, det_res.num, &box_preview[0], &box_encoder[0]);
            }
            else
            {
                draw_boxes(pctx, 0, &box_preview[0], &box_encoder[0]);
            }
#endif

#ifndef DISABLE_OSD
            //write pipeline
            if (pipe_fd >= 0)
            {
                unsigned char send[256];
                memset(send, 0x00, 256);

                send[0] = det_res.num;
                box_preview[0].width = npu_det_fps + 2;
                box_preview[0].height = npu_check_time;
                memcpy(send + 4, &box_preview[0], det_res.num * sizeof(aw_det_box_t));
                write(pipe_fd, send, 256);
                //DBG("write %d boxes.\n", send[0]);
            }
#endif
        }

        if (picture_counter == 0)
        {
            gettimeofday(&start, NULL);
            old_picture_counter = 0;
        }

        AW_MPI_VI_ReleaseFrame(plnk->mVIDev, 0, &buffer);
        picture_counter ++;
    }

    DBG("run finish!\n");

    return result;
_err3:
    result = AW_MPI_VO_DestroyChn(plnk->mVoLayer, plnk->mVOChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! Vo Disable Chn fail!\n");
    }
_err2:
    result = AW_MPI_VO_DisableVideoLayer(plnk->mVoLayer);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableVideoLayer fail!\n");
    }
_err1_5:
    result = AW_MPI_VI_DestroyVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVirChn fail!\n");
    }

    result = AW_MPI_ISP_Stop(pctx->mIspDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! ISP Stop fail!\n");
    }
_err1:
    result = AW_MPI_VI_DestroyVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVipp fail!\n");
    }
_err0:

    DBG("run finish!\n");

    return result;
}

static int old_nums = 0;
static void draw_boxes(sample_run_nna_context_t *pctx, int nums, aw_det_box_t *boxes, aw_det_box_t *boxes_enc)
{
    int ret;
    RGN_ATTR_S stRgnAttr;
    RGN_CHN_ATTR_S stRgnChnAttr;

    RGN_ATTR_S stRgnAttr_enc;
    RGN_CHN_ATTR_S stRgnChnAttrEnc;

    // channel 0 are fixed for vo display.
    vipp2vo_lininfo_t *plnk_ = &pctx->mLinkInfoArray[0];

    // channel 1 are fixed for vo display.
    vipp2vo_lininfo_t *plnk = &pctx->mLinkInfoArray[1];

    memset(&stRgnAttr, 0, sizeof(RGN_ATTR_S));
    memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));

    memset(&stRgnAttr_enc, 0, sizeof(RGN_ATTR_S));
    memset(&stRgnChnAttrEnc, 0, sizeof(RGN_CHN_ATTR_S));

    MPP_CHN_S viChn = {MOD_ID_VIU, plnk->mVIDev, plnk->mVIChn};
    MPP_CHN_S viChn_enc = {MOD_ID_VIU, plnk_->mVIDev, plnk_->mVIChn};
    stRgnAttr.enType = ORL_RGN;
    stRgnAttr_enc.enType = ORL_RGN;

    if (old_nums > 0)
    {
        int k;
        for (k = 0; k < 2 * old_nums; k += 2)
        {
            ret = AW_MPI_RGN_Destroy(k);
            if (ret != SUCCESS)
            {
                DBG("fatal error! destory region failure!\n");
            }
            if (plnk_->mVIDev != MM_INVALID_DEV)
            {
                ret = AW_MPI_RGN_Destroy(k + 1);
                if (ret != SUCCESS)
                {
                    DBG("fatal error! destory region failure!\n");
                }
            }
        }
    }

    int i;

    for (i = 0; i < 2 * nums; i += 2)
    {
        ret = AW_MPI_RGN_Create(i, &stRgnAttr);
        if (ret != SUCCESS)
        {
            DBG("Create region failure.\n");
        }

        stRgnChnAttr.unChnAttr.stOrlChn.stRect.X        = (boxes[i].x % 2 == 0) ?  boxes[i].x : boxes[i].x + 1;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y        = (boxes[i].y % 2 == 0) ?  boxes[i].y : boxes[i].y + 1;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width    = (boxes[i].width % 2 == 0)  ? boxes[i].width : boxes[i].width + 1;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height   = (boxes[i].height % 2 == 0) ? boxes[i].height : boxes[i].height + 1;
        stRgnChnAttr.bShow                              = TRUE;
        stRgnChnAttr.enType                             = ORL_RGN;
        stRgnChnAttr.unChnAttr.stOrlChn.enAreaType      = AREA_RECT;
        stRgnChnAttr.unChnAttr.stOrlChn.mColor          = 0x00FF00;
        stRgnChnAttr.unChnAttr.stOrlChn.mThick          = 1;
        stRgnChnAttr.unChnAttr.stOrlChn.mLayer          = i;
        AW_MPI_RGN_AttachToChn(i, &viChn, &stRgnChnAttr);

        if (plnk_->mVIDev != MM_INVALID_DEV)
        {
            ret = AW_MPI_RGN_Create(i + 1, &stRgnAttr_enc);
            if (ret != SUCCESS)
            {
                DBG("Create region failure.\n");
            }

            stRgnChnAttrEnc.unChnAttr.stOrlChn.stRect.X        = (boxes_enc[i].x % 2 == 0) ?  boxes_enc[i].x : boxes_enc[i].x + 1;
            stRgnChnAttrEnc.unChnAttr.stOrlChn.stRect.Y        = (boxes_enc[i].y % 2 == 0) ?  boxes_enc[i].y : boxes_enc[i].y + 1;
            stRgnChnAttrEnc.unChnAttr.stOrlChn.stRect.Width    = (boxes_enc[i].width % 2 == 0)  ? boxes_enc[i].width : boxes_enc[i].width + 1;
            stRgnChnAttrEnc.unChnAttr.stOrlChn.stRect.Height   = (boxes_enc[i].height % 2 == 0) ? boxes_enc[i].height : boxes_enc[i].height + 1;
            stRgnChnAttrEnc.bShow                              = TRUE;
            stRgnChnAttrEnc.enType                             = ORL_RGN;
            stRgnChnAttrEnc.unChnAttr.stOrlChn.enAreaType      = AREA_RECT;
            stRgnChnAttrEnc.unChnAttr.stOrlChn.mColor          = 0x00FF00;
            stRgnChnAttrEnc.unChnAttr.stOrlChn.mThick          = 1;
            stRgnChnAttrEnc.unChnAttr.stOrlChn.mLayer          = i + 1;
            AW_MPI_RGN_AttachToChn(i + 1, &viChn_enc, &stRgnChnAttrEnc);
        }
    }

    old_nums = nums;
    return;
}

static int venc_channel_run(int channel, sample_run_nna_context_t *pctx)
{
    ERRORTYPE result = SUCCESS;

    DBG("run start!\n");

    if (pctx == NULL)
    {
        DBG("parameter is wrong.\n");
        result = FAILURE;
        goto _err0;
    }

    sample_run_nna_vipp2vo_config_t *pcfg = &pctx->mConfigPara.mVIPP2VOConfigArray[channel];
    vipp2vo_lininfo_t *plnk = &pctx->mLinkInfoArray[channel];

    if (pcfg->mValid == 0)
    {
        DBG("channel %d not enabled.!\n", channel);
        result = SUCCESS;
        goto _err0;
    }

    // Create virtual channel [0] based on specify vipp
    plnk->mVIDev = pcfg->mDevNum;
    plnk->mVIChn = 0;

    DBG("vipp[%d] vir_chn[%d] creating.\n", plnk->mVIDev, plnk->mVIChn);

    // create a VIPP hardware channel.
    result = AW_MPI_VI_CreateVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! AW_MPI_VI CreateVipp:%d failed\n", plnk->mVIDev);
        result = FAILURE;
        goto _err0;
    }

    MPPCallbackInfo cbinfo;

    memset(&cbinfo, 0x00, sizeof(cbinfo));
    cbinfo.cookie = (void *)pctx;
    cbinfo.callback = (MPPCallbackFuncType)&sample_run_nna_callback;
    result = AW_MPI_VI_RegisterCallback(plnk->mVIDev, &cbinfo);
    if (result != SUCCESS)
    {
        DBG("fatal error! vipp[%d] RegisterCallback failed.\n", plnk->mVIDev);
        result = FAILURE;
        goto _err1;
    }

    VI_ATTR_S vi_attr;
    memset(&vi_attr, 0x00, sizeof(vi_attr));
    result = AW_MPI_VI_GetVippAttr(plnk->mVIDev, &vi_attr);
    if (result != SUCCESS)
    {
        DBG("fatal error! AW_MPI_VI GetVippAttr failed.\n");
        result = FAILURE;
        goto _err1;
    }

    memset(&vi_attr, 0, sizeof(VI_ATTR_S));
    vi_attr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    vi_attr.memtype = V4L2_MEMORY_MMAP;
    vi_attr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pctx->mConfigPara.mPicFormat);
    vi_attr.format.field = V4L2_FIELD_NONE;
    vi_attr.format.colorspace = V4L2_COLORSPACE_JPEG;
    vi_attr.format.width = pcfg->mCaptureWidth;
    vi_attr.format.height = pcfg->mCaptureHeight;
    vi_attr.nbufs = 5;
    vi_attr.nplanes = 2;
    vi_attr.drop_frame_num = 0; // drop 2 second video data, default=0

    vi_attr.use_current_win = channel ? 1 : 0;

    vi_attr.fps = pctx->mConfigPara.mFrameRate;
    result = AW_MPI_VI_SetVippAttr(plnk->mVIDev, &vi_attr);
    if (result != SUCCESS)
    {
        DBG("fatal error! AW_MPI_VI SetVippAttr:%d failed.\n", plnk->mVIDev);
        result = FAILURE;
        goto _err1;
    }

    /* open isp */
    AW_MPI_ISP_Run(pctx->mIspDev);

    result = AW_MPI_VI_EnableVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! enableVipp fail!\n");
        result = FAILURE;
        goto _err1;
    }

    // Create a virtual channel on specify VIPP.
    result = AW_MPI_VI_CreateVirChn(plnk->mVIDev, plnk->mVIChn, NULL);
    if (result != SUCCESS)
    {
        DBG("fatal error! createVirChn[%d] fail!\n", plnk->mVIChn);
        result = FAILURE;
        goto _err1;
    }

    // if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations must be after stream_on.
    AW_MPI_VI_SetVippMirror(plnk->mVIDev, 0);//0,1
    AW_MPI_VI_SetVippFlip(plnk->mVIDev, 0);  //0,1

    result = AW_MPI_VENC_CreateChn(pctx->venc_setting.mVeChn, &pctx->venc_setting.mVEncChnAttr);
    if (result < 0)
    {
        DBG("create venc channel[%d] falied!\n", pctx->venc_setting.mVeChn);
        result = FAILURE;
        goto _err1;
    }

    AW_MPI_VENC_SetRcParam(pctx->venc_setting.mVeChn, &pctx->venc_setting.mVEncRcParam);
    AW_MPI_VENC_SetFrameRate(pctx->venc_setting.mVeChn, &pctx->venc_setting.mVencFrameRateConfig);

    VencHeaderData vencheader;

    memset(&vencheader, 0x00, sizeof(VencHeaderData));

    //open output file
#ifdef H265_ENCODE_STREAM
    FILE *fp = fopen("/mnt/sdcard/H265_REC_tina.raw", "wb+");
#else
    FILE *fp = fopen("/mnt/sdcard/H264_REC_tina.raw", "wb+");
#endif
    if (!fp)
    {
        DBG("fatal error! can't open record file.\n");
        result = FAILURE;
        goto _err1;
    }

#ifdef H265_ENCODE_STREAM
    AW_MPI_VENC_GetH265SpsPpsInfo(pctx->venc_setting.mVeChn, &vencheader);
#else
    AW_MPI_VENC_GetH264SpsPpsInfo(pctx->venc_setting.mVeChn, &vencheader);
#endif
    if (vencheader.nLength)
    {
        fwrite(vencheader.pBuffer, vencheader.nLength, 1, fp);
    }

    DBG("sps and pps written, buf[0x%lx],length[%d].\n", (unsigned long)vencheader.pBuffer, vencheader.nLength);

    MPP_CHN_S ViChn = {MOD_ID_VIU, plnk->mVIDev, plnk->mVIChn};
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pctx->venc_setting.mVeChn};
    result = AW_MPI_SYS_Bind(&ViChn, &VeChn);
    if (result != SUCCESS)
    {
        DBG("error!!! vi can not bind venc!!!\n");
        result = FAILURE;
        goto _err1;
    }

    // start vi, venc channel.
    result = AW_MPI_VI_EnableVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! enableVirChn fail!\n");
        result = FAILURE;
        goto _err3;
    }

    result = AW_MPI_VENC_StartRecvPic(pctx->venc_setting.mVeChn);
    if (result != SUCCESS)
    {
        DBG("VENC Start RecvPic error!.\n");
        result = FAILURE;
        goto _err3;
    }

    VENC_STREAM_S VencFrame;
    VENC_PACK_S venc_pack;
    memset(&venc_pack, 0x00, sizeof(VENC_PACK_S));
    memset(&VencFrame, 0x00, sizeof(VENC_STREAM_S));
    VencFrame.mPackCount = 1;
    VencFrame.mpPack = &venc_pack;

    static int picture_count;
    //picture_count = 3600;
    //while (picture_count --)
    while (1)
    {
        int ret;
        if ((ret = AW_MPI_VENC_GetStream(pctx->venc_setting.mVeChn, &VencFrame, 4000)) < 0) //6000(25fps) 4000(30fps)
        {
            DBG("get first frmae failed!\n");
            continue;
        }
        else
        {
            //DBG("Get bitstream ok, picvbv %d, len: %d, %d, pts %lld.\n", picture_count, VencFrame.mpPack->mLen0, VencFrame.mpPack->mLen1, VencFrame.mpPack->mPTS);
            if (VencFrame.mpPack != NULL && VencFrame.mpPack->mLen0)
            {
                fwrite(VencFrame.mpPack->mpAddr0, 1, VencFrame.mpPack->mLen0, fp);
            }
            if (VencFrame.mpPack != NULL && VencFrame.mpPack->mLen1)
            {
                fwrite(VencFrame.mpPack->mpAddr1, 1, VencFrame.mpPack->mLen1, fp);
            }
            ret = AW_MPI_VENC_ReleaseStream(pctx->venc_setting.mVeChn, &VencFrame);
            if (ret < 0)
            {
                DBG("falied error,release failed!!!\n");
            }
        }
    }

    fflush(fp);
    fclose(fp);

    DBG("run finish!\n");

    return result;

_err3:
    result = AW_MPI_SYS_UnBind(&ViChn, &VeChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! ViChn && VoChn SYS_UnBind fail!\n");
    }

    result = AW_MPI_VI_DestroyVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVirChn fail!\n");
    }

    result = AW_MPI_ISP_Stop(pctx->mIspDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! ISP Stop fail!\n");
    }
_err1:
    result = AW_MPI_VI_DestroyVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVipp fail!\n");
    }
_err0:

    DBG("run finish!\n");
    return result;
}

static int config_vipp_display2_vo(int channel, sample_run_nna_context_t *pctx)
{
    ERRORTYPE result = SUCCESS;

    DBG("run start!\n");

    if (pctx == NULL)
    {
        DBG("parameter is wrong.\n");
        result = FAILURE;
        goto _err0;
    }

    sample_run_nna_vipp2vo_config_t *pcfg = &pctx->mConfigPara.mVIPP2VOConfigArray[channel];
    vipp2vo_lininfo_t *plnk = &pctx->mLinkInfoArray[channel];

    if (pcfg->mValid == 0)
    {
        DBG("channel %d not enabled.!\n", channel);
        result = SUCCESS;
        goto _err0;
    }

    // Create virtual channel [0] based on specify vipp
    plnk->mVIDev = pcfg->mDevNum;
    plnk->mVIChn = 0;

    DBG("vipp[%d] vir_chn[%d] creating.\n", plnk->mVIDev, plnk->mVIChn);

    // create a VIPP hardware channel.
    result = AW_MPI_VI_CreateVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! AW_MPI_VI CreateVipp:%d failed\n", plnk->mVIDev);
        result = FAILURE;
        goto _err0;
    }

    MPPCallbackInfo cbinfo;

    memset(&cbinfo, 0x00, sizeof(cbinfo));
    cbinfo.cookie = (void *)pctx;
    cbinfo.callback = (MPPCallbackFuncType)&sample_run_nna_callback;
    result = AW_MPI_VI_RegisterCallback(plnk->mVIDev, &cbinfo);
    if (result != SUCCESS)
    {
        DBG("fatal error! vipp[%d] RegisterCallback failed.\n", plnk->mVIDev);
        result = FAILURE;
        goto _err1;
    }

    VI_ATTR_S vi_attr;
    memset(&vi_attr, 0x00, sizeof(vi_attr));
    result = AW_MPI_VI_GetVippAttr(plnk->mVIDev, &vi_attr);
    if (result != SUCCESS)
    {
        DBG("fatal error! AW_MPI_VI GetVippAttr failed.\n");
        result = FAILURE;
        goto _err1;
    }

    memset(&vi_attr, 0, sizeof(VI_ATTR_S));
    vi_attr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    vi_attr.memtype = V4L2_MEMORY_MMAP;
    vi_attr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pctx->mConfigPara.mPicFormat);
    vi_attr.format.field = V4L2_FIELD_NONE;
    vi_attr.format.colorspace = V4L2_COLORSPACE_JPEG;
    vi_attr.format.width = pcfg->mCaptureWidth;
    vi_attr.format.height = pcfg->mCaptureHeight;
    vi_attr.nbufs = 5;
    vi_attr.nplanes = 2;
    vi_attr.drop_frame_num = 0; // drop 2 second video data, default=0

    vi_attr.use_current_win = channel ? 1 : 0;

    vi_attr.fps = pctx->mConfigPara.mFrameRate;
    result = AW_MPI_VI_SetVippAttr(plnk->mVIDev, &vi_attr);
    if (result != SUCCESS)
    {
        DBG("fatal error! AW_MPI_VI SetVippAttr:%d failed.\n", plnk->mVIDev);
        result = FAILURE;
        goto _err1;
    }

    /* open isp */
    AW_MPI_ISP_Run(pctx->mIspDev);

    result = AW_MPI_VI_EnableVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! enableVipp fail!\n");
        result = FAILURE;
        goto _err1;
    }

    // Create a virtual channel on specify VIPP.
    result = AW_MPI_VI_CreateVirChn(plnk->mVIDev, plnk->mVIChn, NULL);
    if (result != SUCCESS)
    {
        DBG("fatal error! createVirChn[%d] fail!\n", plnk->mVIChn);
        result = FAILURE;
        goto _err1;
    }

    // if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations must be after stream_on.
    AW_MPI_VI_SetVippMirror(plnk->mVIDev, 0);//0,1
    AW_MPI_VI_SetVippFlip(plnk->mVIDev, 0);//0,1

    // enable vo layer
    int hlay0 = pcfg->mLayerNum;
    if (SUCCESS != AW_MPI_VO_EnableVideoLayer(hlay0))
    {
        DBG("fatal error! enable video layer[%d] fail!\n", hlay0);
        result = FAILURE;
        goto _err1_5;
    }

    plnk->mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(plnk->mVoLayer, &plnk->mLayerAttr);
    plnk->mLayerAttr.stDispRect.X = pcfg->mDisplayX;
    plnk->mLayerAttr.stDispRect.Y = pcfg->mDisplayY;
    plnk->mLayerAttr.stDispRect.Width = pcfg->mDisplayWidth;
    plnk->mLayerAttr.stDispRect.Height = pcfg->mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(plnk->mVoLayer, &plnk->mLayerAttr);

    /*
     * create vo channel and clock channel.
     * (because frame information has 'pts', there is no need clock channel now)
     */
    BOOL bSuccessFlag = FALSE;
    plnk->mVOChn = 0;
    while (plnk->mVOChn < VO_MAX_CHN_NUM)
    {
        // create VO component, and create compoent thread.
        result = AW_MPI_VO_CreateChn(plnk->mVoLayer, plnk->mVOChn);
        if (SUCCESS == result)
        {
            bSuccessFlag = TRUE;
            DBG("create vo channel[%d] success!\n", plnk->mVOChn);
            break;
        }
        else if (ERR_VO_CHN_NOT_DISABLE == result)
        {
            DBG("vo channel[%d] is exist, find next!\n", plnk->mVOChn);
            plnk->mVOChn++;
        }
        else
        {
            DBG("fatal error! create vo channel[%d] ret[0x%x]!\n", plnk->mVOChn, result);
            break;
        }
    }
    if (FALSE == bSuccessFlag)
    {
        plnk->mVOChn = MM_INVALID_CHN;
        DBG("fatal error! create vo channel fail!\n");
        result = FAILURE;
        goto _err2;
    }

    cbinfo.cookie = (void *)pctx;
    cbinfo.callback = (MPPCallbackFuncType)&vocallback_wrapper;
    AW_MPI_VO_RegisterCallback(plnk->mVoLayer, plnk->mVOChn, &cbinfo);
    AW_MPI_VO_SetChnDispBufNum(plnk->mVoLayer, plnk->mVOChn, 2);
    AW_MPI_VO_SetVideoLayerPriority(plnk->mVoLayer, 11);

    /*
     * bind clock,vo, viChn
     * (because frame information has 'pts', there is no need to bind clock channel now)
     */
    MPP_CHN_S VOChn = {MOD_ID_VOU, plnk->mVoLayer, plnk->mVOChn};
    MPP_CHN_S VIChn = {MOD_ID_VIU, plnk->mVIDev, plnk->mVIChn};
    AW_MPI_SYS_Bind(&VIChn, &VOChn);

    // start vo, vi_channel.
    result = AW_MPI_VI_EnableVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! enableVirChn fail!\n");
        result = FAILURE;
        goto _err3;
    }

    AW_MPI_VO_StartChn(plnk->mVoLayer, plnk->mVOChn);

    DBG("run finish!\n");

    return result;

_err3:
    result = AW_MPI_SYS_UnBind(&VIChn, &VOChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! ViChn && VoChn SYS_UnBind fail!\n");
    }
    result = AW_MPI_VO_DestroyChn(plnk->mVoLayer, plnk->mVOChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! Vo Disable Chn fail!\n");
    }
_err2:
    result = AW_MPI_VO_DisableVideoLayer(plnk->mVoLayer);
    if (result != SUCCESS)
    {
        DBG("fatal error! VO DisableVideoLayer fail!\n");
    }
_err1_5:
    result = AW_MPI_VI_DestroyVirChn(plnk->mVIDev, plnk->mVIChn);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVirChn fail!\n");
    }

    result = AW_MPI_ISP_Stop(pctx->mIspDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! ISP Stop fail!\n");
    }
_err1:
    result = AW_MPI_VI_DestroyVipp(plnk->mVIDev);
    if (result != SUCCESS)
    {
        DBG("fatal error! VI DestoryVipp fail!\n");
    }
_err0:

    DBG("run finish!\n");
    return result;
}

void *venc_recorder_thread(void *para)
{
    sample_run_nna_context_t *pctx = (sample_run_nna_context_t *)para;

    DBG("run start!\n");

    config_venc_channel(pctx);

    venc_channel_run(0, pctx);

    destory_venc_channel(0, pctx);

    DBG("run finish!\n");
    return NULL;
}

void *npu_worker_thread(void *para)
{
    sample_run_nna_context_t *pctx = (sample_run_nna_context_t *)para;

    DBG("run start!\n");

    npu_config_and_run(2, pctx);
    npu_thread_destory(2, pctx);

    DBG("run finish!\n");
    return NULL;
}

// this is the preview channel task.
static void run_project(sample_run_nna_context_t *pctx)
{
    int ret;

    DBG("run start!\n");

    pctx->mIspDev = 0;

    // enable vo dev, display engine no.
    pctx->mVoDev = 0;
    AW_MPI_VO_Enable(pctx->mVoDev);

    VO_PUB_ATTR_S pub_attr;

    memset(&pub_attr, 0x00, sizeof(pub_attr));
    AW_MPI_VO_GetPubAttr(pctx->mVoDev, &pub_attr);
    pub_attr.enIntfType = pctx->mConfigPara.mDispType;
    pub_attr.enIntfSync = pctx->mConfigPara.mDispSync;
    AW_MPI_VO_SetPubAttr(pctx->mVoDev, &pub_attr);

    // config the link of pipeline.
#ifndef DISABLE_VO
    config_vipp_display2_vo(1, pctx);
#endif

#if 1
    int policy = 0;
    pthread_attr_t attr;
    struct sched_param param;
    bzero((void*)&param, sizeof(param));

    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr,PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr,SCHED_RR);
    pthread_attr_getschedpolicy(&attr,&policy);

    int max_priority = sched_get_priority_max(policy);
    param.sched_priority = max_priority;
    pthread_attr_setschedparam(&attr,&param);
    ret = pthread_create(&pctx->nna_detect_thread, &attr, npu_worker_thread, pctx);
#else
    ret = pthread_create(&pctx->nna_detect_thread, NULL, npu_worker_thread, pctx);
#endif
    if (ret != 0)
    {
        DBG("fatal error! pthread create fail[0x%x].\n", ret);
        goto _err0;
    }

#ifndef DISABLE_RECORD
    ret = pthread_create(&pctx->ven_record_thread, NULL, venc_recorder_thread, pctx);
    if (ret != 0)
    {
        DBG("fatal error! pthread create fail[0x%x].\n", ret);
        goto _err1;
    }
#endif

    // must after isp_run invocated.
    ret = AW_MPI_ISP_SwitchIspConfig(pctx->mIspDev, pctx->mConfigPara.mIspWdrSetting);
    if (ret != SUCCESS)
    {
        DBG("fatal error! isp switch wdr[%d] fail[%d].\n", pctx->mConfigPara.mIspWdrSetting, ret);
        goto _err2;
    }

    int count = 6;
    while (count --)
    {
        sleep(10);
    }

    unsigned long value;

_err2:
#ifndef DISABLE_RECORD
    // finish the demo.
    ret = pthread_join(pctx->ven_record_thread, (void **)&value);
    DBG("ret = %d, value = %ld.\n", ret, value);
#endif

_err1:
    ret = pthread_join(pctx->nna_detect_thread, (void **)&value);
    // preview cant exit till the npu detect thread exit.
#ifndef DISABLE_VO
    destory_vipp_display2_vo(1, pctx);
#endif
    DBG("ret = %d, value = %ld.\n", ret, value);

_err0:
    AW_MPI_VO_Disable(pctx->mVoDev);
    pctx->mVoDev = -1;

    DBG("run finish!\n");
    return;
}

static int config_venc_channel(sample_run_nna_context_t *pctx)
{
    DBG("run start!\n");

    // use ve channel 0 default.
    pctx->venc_setting.mVeChn = 0;

    memset(&pctx->venc_setting.mVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&pctx->venc_setting.mVEncRcParam, 0, sizeof(VENC_RC_PARAM_S));

#ifdef H265_ENCODE_STREAM
    pctx->venc_setting.mVEncChnAttr.VeAttr.Type                     = PT_H265;
#else
    pctx->venc_setting.mVEncChnAttr.VeAttr.Type                     = PT_H264;
#endif
    pctx->venc_setting.mVEncChnAttr.VeAttr.MaxKeyInterval           = 30;

    // framesize before encoder.
    pctx->venc_setting.mVEncChnAttr.VeAttr.SrcPicWidth              = ENCODER_PIC_WIDTH;
    pctx->venc_setting.mVEncChnAttr.VeAttr.SrcPicHeight             = ENCODER_PIC_HEIGH;

    // non interlace.
    pctx->venc_setting.mVEncChnAttr.VeAttr.Field                    = VIDEO_FIELD_FRAME;
    pctx->venc_setting.mVEncChnAttr.VeAttr.PixelFormat              = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pctx->venc_setting.mVEncChnAttr.VeAttr.mColorSpace              = V4L2_COLORSPACE_JPEG;

#ifdef H265_ENCODE_STREAM
    // main profile.
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH265e.mProfile       = 0;
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH265e.mbByFrame      = TRUE;
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH265e.mPicWidth      = 1920;
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH265e.mPicHeight     = 1080;
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH265e.mLevel         = H265_LEVEL_62;
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
    pctx->venc_setting.mVEncChnAttr.RcAttr.mRcMode                  = VENC_RC_MODE_H265VBR;

    pctx->venc_setting.mVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = 1 * 1024 * 1024;
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH265e.mThreshSize    = ALIGN((1920 * 1080 * 3 / 2) / 3, 1024);
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH265e.mBufSize       = ALIGN(pctx->venc_setting.mVEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate * 4 / 8  \
            + pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH265e.mThreshSize, 1024);
    pctx->venc_setting.mVEncRcParam.ParamH265Cbr.mMaxQp             = 45;
    pctx->venc_setting.mVEncRcParam.ParamH265Cbr.mMinQp             = 10;
#else
    // main profile.
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH264e.Profile        = 1;
    // frame mode, not slice mode.
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH264e.bByFrame       = TRUE;
    // frame size after encoder(vbv bitstream).
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH264e.PicWidth       = 1920;
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH264e.PicHeight      = 1080;

    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH264e.mLevel         = H264_LEVEL_51;
    pctx->venc_setting.mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
    pctx->venc_setting.mVEncChnAttr.RcAttr.mRcMode                  = VENC_RC_MODE_H264CBR;

    // 8388608 = 8M.
    pctx->venc_setting.mVEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate    = 10 * 1024 * 1024;
    pctx->venc_setting.mVEncRcParam.ParamH264Cbr.mMaxQp             = 45;
    pctx->venc_setting.mVEncRcParam.ParamH264Cbr.mMinQp             = 10;
#endif

    pctx->venc_setting.mVencFrameRateConfig.SrcFrmRate              = VIPP_INPUT_FPS;
    pctx->venc_setting.mVencFrameRateConfig.DstFrmRate              = 30;

    // set the rate control parameter.
    pctx->venc_setting.mVEncRcParam.product_mode                    = VENC_PRODUCT_NORMAL_MODE;
    //pctx->venc_setting.mVEncRcParam.product_mode                    = VENC_PRODUCT_IPC_MODE;
    pctx->venc_setting.mVEncRcParam.sensor_type                     = VENC_ST_EN_WDR;

    DBG("run finish!\n");
    return 0;
}

static void pipe_init(void)
{
    int ret;

    ret = mkfifo("/tmp/npu_fifo", 0666);
    if (ret != 0)
    {
        perror("mkfifo eror");
    }

    pipe_fd = open("/tmp/npu_fifo", O_WRONLY);
    if (pipe_fd < 0)
    {
        perror("open fifo");
    }

    return;
}

int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
    int ret;

    DBG("run start!\n");

    static sample_run_nna_context_t nna_context;
    sample_run_nna_context_t *pcontext;

    int width  = NPU_DET_PIC_WIDTH;
    int height = NPU_DET_PIC_HEIGHT;

#ifndef DISABLE_OSD
    pipe_init();
#endif

    //npu_init_models();

	
    void odet_init(void);
    odet_init();
    int buffer_sz = width * height * 1.5;
    yuv_buffer = (unsigned char *)calloc(buffer_sz, sizeof(unsigned char));
    if (yuv_buffer == NULL)
    {
        DBG("alloc yuv buffer failure failure.\n");
        return -1;
    }

    memset(&nna_context, 0x00, sizeof(nna_context));
    pcontext = &nna_context;

    load_vipp_parameters(pcontext);
    ret = mpp_init(pcontext);
    if (ret != 0)
    {
        DBG("fatal error, mpp init failure.\n");
        return -1;
    }

    run_project(pcontext);

    //npu_deinit_models();

    free(yuv_buffer);
    yuv_buffer = NULL;
    mpp_exit();

    DBG("run finish!\n");
    return 0;
}
