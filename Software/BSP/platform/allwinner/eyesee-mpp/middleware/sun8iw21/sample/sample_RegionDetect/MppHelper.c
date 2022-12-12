#include <string.h>

#include <VIDEO_FRAME_INFO_S.h>
#include <mpi_sys.h>
#include <mpi_isp.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_vi.h>
#include <mpi_vo.h>
#include <utils/plat_log.h>

#include "MppHelper.h"

/**
 * mpp sys
 */

ERRORTYPE MPP_HELPER_INIT()
{
    ERRORTYPE ret;
    MPP_SYS_CONF_S stSysConf;
    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);
    ret = AW_MPI_SYS_Init();
    return ret;
}

ERRORTYPE MPP_HELPER_UNINIT()
{
    ERRORTYPE ret;
    /* exit mpp systerm */
    ret = AW_MPI_SYS_Exit();
    return ret;
}

/**
 * mpp vipp
 */

ERRORTYPE MPP_HELPER_CREATE_VIPP(MPP_CHN_S viChn, int width, int height, int fps, PIXEL_FORMAT_E format)
{
    ERRORTYPE ret;
    VI_ATTR_S viAttr;
    memset(&viAttr, 0, sizeof(VI_ATTR_S));
    //viAttr.mOnlineEnable = 0;
    //viAttr.mOnlineShareBufNum = 2;
    viAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    viAttr.memtype = V4L2_MEMORY_MMAP;
    viAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(format);
    viAttr.format.field = V4L2_FIELD_NONE;
    viAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    viAttr.format.width = width;
    viAttr.format.height = height;
    viAttr.fps = fps;
    viAttr.use_current_win = viChn.mDevId > 0 ? 1 : 0;
    viAttr.nbufs = 5;
    viAttr.nplanes = 2;
    viAttr.drop_frame_num = 0;

    ret = AW_MPI_VI_CreateVipp(viChn.mDevId);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_SetVippAttr(viChn.mDevId, &viAttr);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_EnableVipp(viChn.mDevId);
    _CHECK_RET(ret);
    ret = AW_MPI_VI_SetVippMirror(viChn.mDevId, 0);//0,1
    _CHECK_RET(ret);
    // if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations must be after stream_on.
    ret = AW_MPI_VI_SetVippFlip(viChn.mDevId, 0);//0,1
    _CHECK_RET(ret);

    ret = AW_MPI_VI_CreateVirChn(viChn.mDevId, viChn.mChnId, NULL);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_HELPER_DESTROY_VIPP(MPP_CHN_S viChn)
{
    ERRORTYPE ret;
    ret = AW_MPI_VI_DestroyVirChn(viChn.mDevId, viChn.mChnId);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_DisableVipp(viChn.mDevId);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_DestroyVipp(viChn.mDevId);
    _CHECK_RET(ret);
    return ret;
}

ERRORTYPE MPP_HELPER_ENABLE_VIPP(MPP_CHN_S viChn)
{
    ERRORTYPE ret;
    ret = AW_MPI_VI_EnableVirChn(viChn.mDevId, viChn.mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_HELPER_DISABLE_VIPP(MPP_CHN_S viChn)
{
    ERRORTYPE ret;
    ret = AW_MPI_VI_DisableVirChn(viChn.mDevId, viChn.mChnId);
    _CHECK_RET(ret);

    return ret;
}

/**
 * mpp vo
 */

static ERRORTYPE vocallback_wrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    if (MOD_ID_VOU == pChn->mModId) {
    } else {
        ret = FAILURE;
    }

    return ret;
}

ERRORTYPE MPP_HELPER_CREATE_VO(MPP_CHN_S voChn, int x, int y, int width, int height)
{
    ERRORTYPE ret;

    int layer = 4;
    int chnId = 0;

    ret = AW_MPI_VO_EnableVideoLayer(voChn.mDevId);
    _CHECK_RET(ret);

    VO_VIDEO_LAYER_ATTR_S layerAttr;
    ret = AW_MPI_VO_GetVideoLayerAttr(voChn.mDevId, &layerAttr);
    layerAttr.stDispRect.X = x;
    layerAttr.stDispRect.Y = y;
    layerAttr.stDispRect.Width = width;
    layerAttr.stDispRect.Height = height;
    /*layerAttr.stImageSize.Width = 720;
    layerAttr.stImageSize.Height = 1280;
    layerAttr.enPixFormat = MM_PIXEL_FORMAT_RGB_888;*/
    ret = AW_MPI_VO_SetVideoLayerAttr(voChn.mDevId, &layerAttr);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_CreateChn(voChn.mDevId, voChn.mChnId);
    _CHECK_RET(ret);

    MPPCallbackInfo cbinfo;
    memset(&cbinfo, 0, sizeof(cbinfo));
    //cbinfo.cookie = (void *)xxx;
    cbinfo.callback = (MPPCallbackFuncType)&vocallback_wrapper;
    ret = AW_MPI_VO_RegisterCallback(voChn.mDevId, voChn.mChnId, &cbinfo);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_SetChnDispBufNum(voChn.mDevId, voChn.mChnId, 2);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_SetVideoLayerPriority(voChn.mDevId, 11);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_HELPER_DESTROY_VO(MPP_CHN_S voChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VO_DestroyChn(voChn.mDevId, voChn.mChnId);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_DisableVideoLayer(voChn.mDevId);
    _CHECK_RET(ret);

    sleep(1);

    return ret;
}

/**
 * mpp ve
 */

ERRORTYPE MPP_HELPER_CREATE_VENC(MPP_CHN_S veChn, int srcWidth, int srcHeight, int srcFps, int distWidth, int distHeight, int distFps, PAYLOAD_TYPE_E payloadType, int bitRate, PIXEL_FORMAT_E format)
{
    ERRORTYPE ret;
    VENC_CHN_ATTR_S mVEncChnAttr;
    VENC_RC_PARAM_S mVEncRcParam;
    VENC_FRAME_RATE_S mVencFrameRateConfig;

    /* venc chn attr */
    memset(&mVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    //mVEncChnAttr.VeAttr.mOnlineEnable = 0;
    //mVEncChnAttr.VeAttr.mOnlineShareBufNum = 2;
    mVEncChnAttr.VeAttr.Type = payloadType; // PT_H264, PT_H265, PT_MJPEG
    mVEncChnAttr.VeAttr.MaxKeyInterval = distFps;
    mVEncChnAttr.VeAttr.SrcPicWidth = srcWidth;
    mVEncChnAttr.VeAttr.SrcPicHeight = srcHeight;
    mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    mVEncChnAttr.VeAttr.PixelFormat = format; // NV12
    mVEncChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_JPEG;
    mVEncRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
    mVEncRcParam.sensor_type = VENC_ST_EN_WDR;
    if (PT_H264 == mVEncChnAttr.VeAttr.Type) {
        mVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        mVEncChnAttr.VeAttr.AttrH264e.PicWidth = distWidth;
        mVEncChnAttr.VeAttr.AttrH264e.PicHeight = distHeight;
        mVEncChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
        mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        mVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = bitRate;
        mVEncRcParam.ParamH264Vbr.mMaxQp = 51;
        mVEncRcParam.ParamH264Vbr.mMinQp = 1;
        mVEncRcParam.ParamH264Vbr.mMaxPqp = 51;
        mVEncRcParam.ParamH264Vbr.mMinPqp = 1;
        mVEncRcParam.ParamH264Vbr.mQpInit = 30;
        mVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
        mVEncRcParam.ParamH264Vbr.mMovingTh = 20;
        mVEncRcParam.ParamH264Vbr.mQuality = 5;
        mVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 15;
        mVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    } else if(PT_H265 == mVEncChnAttr.VeAttr.Type) {
        mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = distWidth;
        mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = distHeight;
        mVEncChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
        mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        mVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = bitRate;
        mVEncRcParam.ParamH265Vbr.mMaxQp = 51;
        mVEncRcParam.ParamH265Vbr.mMinQp = 1;
        mVEncRcParam.ParamH265Vbr.mMaxPqp = 51;
        mVEncRcParam.ParamH265Vbr.mMinPqp = 1;
        mVEncRcParam.ParamH265Vbr.mQpInit = 30;
        mVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        mVEncRcParam.ParamH265Vbr.mMovingTh = 20;
        mVEncRcParam.ParamH265Vbr.mQuality = 5;
        mVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 15;
        mVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    } else if(PT_MJPEG == mVEncChnAttr.VeAttr.Type) {
        mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth= distWidth;
        mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = distHeight;
        mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = bitRate;
    }
    mVencFrameRateConfig.SrcFrmRate = srcFps;
    mVencFrameRateConfig.DstFrmRate = distFps;

    ret = AW_MPI_VENC_CreateChn(veChn.mChnId, &mVEncChnAttr);
    _CHECK_RET(ret);

    ret = AW_MPI_VENC_SetRcParam(veChn.mChnId, &mVEncRcParam);
    _CHECK_RET(ret);
    ret = AW_MPI_VENC_SetFrameRate(veChn.mChnId, &mVencFrameRateConfig);
    _CHECK_RET(ret);
    // 3D降噪
    s3DfilterParam m3DnrPara;
    memset(&m3DnrPara, 0, sizeof(s3DfilterParam));
    m3DnrPara.enable_3d_filter = 1;
    m3DnrPara.adjust_pix_level_enable = 0;
    m3DnrPara.smooth_filter_enable = 1;
    m3DnrPara.max_pix_diff_th = 6;
    m3DnrPara.max_mv_th = 2;
    m3DnrPara.max_mad_th = 11;
    m3DnrPara.min_coef = 14;
    m3DnrPara.max_coef = 16;
    ret = AW_MPI_VENC_Set3DFilter(veChn.mChnId, &m3DnrPara);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_HELPER_DESTROY_VENC(MPP_CHN_S veChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_ResetChn(veChn.mChnId);
    _CHECK_RET(ret);

    ret = AW_MPI_VENC_DestroyChn(veChn.mChnId);
    _CHECK_RET(ret);

    return ret;
}

/**
 * mpp function
 */
#define SRC_WIDTH 1920
#define SRC_HEIGHT 1080
#define SRC_FRAME_RATE 30
#define DST_WIDTH 1920
#define DST_HEIGHT 1080
#define DST_FRAME_RATE 30
// PT_H264, PT_H265, PT_MJPEG
#define DST_ENCODER_TYPE PT_H265
#define DST_BIT_RATE 2000000
#define PREVIEW_WIDTH 720
#define PREVIEW_HEIGHT 405
#define NPU_DET_PIC_WIDTH    352
#define NPU_DET_PIC_HEIGHT   352
#define VIPP_INPUT_FPS       30

#define ISP_DEV 0
#define VO_DEV 0

ERRORTYPE MPP_HELPER_START_PREVIEW()
{
    ERRORTYPE ret;

    // enable vo dev, display engine no.
    AW_MPI_VO_Enable(VO_DEV);
    VO_PUB_ATTR_S pub_attr;
    memset(&pub_attr, 0, sizeof(pub_attr));
    AW_MPI_VO_GetPubAttr(VO_DEV, &pub_attr);
    pub_attr.enIntfType = VO_INTF_LCD;
    pub_attr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(VO_DEV, &pub_attr);

    MPP_CHN_S previewViChn = {MOD_ID_VIU, 4, 0};
    MPP_CHN_S previewVoChn = {MOD_ID_VOU, 4, 0};
    ret = MPP_HELPER_CREATE_VIPP(previewViChn, PREVIEW_WIDTH, PREVIEW_HEIGHT, SRC_FRAME_RATE, MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420);
    //ret = MPP_HELPER_CREATE_VIPP(previewViChn, PREVIEW_HEIGHT, PREVIEW_HEIGHT, SRC_FRAME_RATE, MM_PIXEL_FORMAT_RGB_888);
    _CHECK_RET(ret);

    AW_MPI_ISP_Run(ISP_DEV);

    ret = MPP_HELPER_CREATE_VO(previewVoChn, 0, 150, 720, 405);
    //ret = MPP_HELPER_CREATE_VO(previewVoChn, 0, 0, 720, 720);
    _CHECK_RET(ret);

    //ret = AW_MPI_SYS_Bind(&previewViChn, &previewVoChn);
    //_CHECK_RET(ret);

    ret = MPP_HELPER_ENABLE_VIPP(previewViChn);
    _CHECK_RET(ret);
    ret = AW_MPI_VO_StartChn(previewVoChn.mDevId, previewVoChn.mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_HELPER_STOP_PREVIEW()
{
    ERRORTYPE ret;

    MPP_CHN_S previewViChn = {MOD_ID_VIU, 4, 0};
    MPP_CHN_S previewVoChn = {MOD_ID_VOU, 4, 0};

    ret = AW_MPI_VO_StopChn(previewVoChn.mDevId, previewVoChn.mChnId);
    _CHECK_RET(ret);

    ret = MPP_HELPER_DISABLE_VIPP(previewViChn);
    _CHECK_RET(ret);

    ret = AW_MPI_SYS_UnBind(&previewViChn, &previewVoChn);
    _CHECK_RET(ret);

    ret = MPP_HELPER_DESTROY_VO(previewVoChn);
    _CHECK_RET(ret);

    ret = AW_MPI_ISP_Stop(ISP_DEV);

    ret = MPP_HELPER_DESTROY_VIPP(previewViChn);
    _CHECK_RET(ret);

    AW_MPI_VO_Disable(VO_DEV);
    _CHECK_RET(ret);

    return ret;
}

BOOL MPP_HELPER_DO_PREVIEW(preview_callback_t preview_callback)
{
    BOOL ret = FALSE;
    MPP_CHN_S previewViChn = {MOD_ID_VIU, 4, 0};
    MPP_CHN_S previewVoChn = {MOD_ID_VOU, 4, 0};

    VIDEO_FRAME_INFO_S pFrameInfo;
    AW_MPI_VI_GetFrame(previewViChn.mDevId, 0, &pFrameInfo, 500);
    /*VideoFrameBufferSizeInfo vfbsInfo;
    memset(&vfbsInfo, 0, sizeof(VideoFrameBufferSizeInfo));
    getVideoFrameBufferSizeInfo(&pFrameInfo, &vfbsInfo);*/

    if (preview_callback) {
        if (preview_callback(pFrameInfo.VFrame.mpVirAddr[0], pFrameInfo.VFrame.mpVirAddr[1], pFrameInfo.VFrame.mpVirAddr[2]))
            ret = TRUE;
    }
    AW_MPI_VO_SendFrame(previewVoChn.mDevId, previewVoChn.mChnId, &pFrameInfo, 0);
    AW_MPI_VI_ReleaseFrame(previewViChn.mDevId, 0, &pFrameInfo);

    return ret;
}

VENC_STREAM_S stVencStream;
VENC_PACK_S stVencPack;
VencMotionSearchResult motionResult;
ERRORTYPE MPP_HELPER_START_RECORD(venc_record_callback_t record_callback)
{
    ERRORTYPE ret;
    MPP_CHN_S viChn = {MOD_ID_VIU, 0, 0};
    MPP_CHN_S veChn = {MOD_ID_VENC, 0, 0};

    ret = MPP_HELPER_CREATE_VIPP(viChn, SRC_WIDTH, SRC_HEIGHT, SRC_FRAME_RATE, MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420);
    _CHECK_RET(ret);

    ret = MPP_HELPER_CREATE_VENC(veChn, SRC_WIDTH, SRC_HEIGHT, SRC_FRAME_RATE, DST_WIDTH, DST_HEIGHT, DST_FRAME_RATE, DST_ENCODER_TYPE, DST_BIT_RATE, MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420);
    _CHECK_RET(ret);

    ret = AW_MPI_SYS_Bind(&viChn, &veChn);
    _CHECK_RET(ret);

    AW_MPI_ISP_Run(ISP_DEV);

    ret = MPP_HELPER_ENABLE_VIPP(viChn);
    _CHECK_RET(ret);

    ret = AW_MPI_VENC_StartRecvPic(veChn.mChnId);
    _CHECK_RET(ret);

    VencMotionSearchParam mMotionParam;
    memset(&mMotionParam, 0, sizeof(VencMotionSearchParam));
    mMotionParam.en_motion_search = 1;
    mMotionParam.dis_default_para = 1;
    mMotionParam.hor_region_num = 10;
    mMotionParam.ver_region_num = 5;
    mMotionParam.large_mv_th = 20;
    mMotionParam.large_mv_ratio_th = 12.0f;
    mMotionParam.non_zero_mv_ratio_th = 20.0f;
    mMotionParam.large_sad_ratio_th = 30.0f;
    AW_MPI_VENC_SetMotionSearchParam(veChn.mChnId, &mMotionParam);
    //AW_MPI_VENC_GetMotionSearchParam(veChn.mChnId, &mMotionParam);
    unsigned int size = mMotionParam.hor_region_num * mMotionParam.ver_region_num * sizeof(VencMotionSearchRegion);
    motionResult.region = (VencMotionSearchRegion *)malloc(size);
    if (NULL == motionResult.region)
    {
        aloge("fatal error! malloc region failed! size=%d", size);
    }
    else
    {
        memset(motionResult.region, 0, size);
    }

    if (record_callback) {
        VencHeaderData header;
        if (PT_H264 == DST_ENCODER_TYPE) {
            ret = AW_MPI_VENC_GetH264SpsPpsInfo(veChn.mChnId, &header);
        } else if (PT_H265 == DST_ENCODER_TYPE) {
            ret = AW_MPI_VENC_GetH265SpsPpsInfo(veChn.mChnId, &header);
        } else {
            ret = -1;
        }
        if (ret == SUCCESS && header.nLength) {
            record_callback(header.pBuffer, header.nLength);
        } else {
            return ret;
        }
    }

    memset(&stVencStream, 0, sizeof(VENC_STREAM_S));
    memset(&stVencPack, 0, sizeof(VENC_PACK_S));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;

    return ret;
}

ERRORTYPE MPP_HELPER_STOP_RECORD()
{
    ERRORTYPE ret;
    MPP_CHN_S viChn = {MOD_ID_VIU, 0, 0};
    MPP_CHN_S veChn = {MOD_ID_VENC, 0, 0};

    VencMotionSearchParam mMotionParam;
    memset(&mMotionParam, 0, sizeof(VencMotionSearchParam));
    mMotionParam.en_motion_search = 0;
    AW_MPI_VENC_SetMotionSearchParam(veChn.mChnId, &mMotionParam);

    ret = AW_MPI_VENC_StopRecvPic(veChn.mChnId);
    _CHECK_RET(ret);

    ret = MPP_HELPER_DISABLE_VIPP(viChn);
    _CHECK_RET(ret);

    ret = AW_MPI_SYS_UnBind(&viChn, &veChn);
    _CHECK_RET(ret);

    ret = AW_MPI_ISP_Stop(ISP_DEV);
    _CHECK_RET(ret);

    ret = MPP_HELPER_DESTROY_VENC(veChn);
    _CHECK_RET(ret);

    ret = MPP_HELPER_DESTROY_VIPP(viChn);
    _CHECK_RET(ret);

    return ret;
}

BOOL MPP_HELPER_DO_RECORD(venc_record_callback_t record_callback, venc_motion_callback_t motion_callback)
{
    ERRORTYPE ret;
    MPP_CHN_S veChn = {MOD_ID_VENC, 0, 0};

    if ((ret = AW_MPI_VENC_GetStream(veChn.mChnId, &stVencStream, 1000)) < 0) {
        printf("get venc stream failed!");
        return FALSE;
    }

    if (record_callback) {
        if (stVencStream.mpPack != NULL && stVencStream.mpPack->mLen0) {
            record_callback(stVencStream.mpPack->mpAddr0, stVencStream.mpPack->mLen0);
        }
        if (stVencStream.mpPack != NULL && stVencStream.mpPack->mLen1) {
            record_callback(stVencStream.mpPack->mpAddr1, stVencStream.mpPack->mLen1);
        }
    }
    if (motion_callback) {
        ret = AW_MPI_VENC_GetMotionSearchResult(veChn.mChnId, &motionResult);
        if (ret == SUCCESS) {
            motion_callback(motionResult);
        }
    }
    ret = AW_MPI_VENC_ReleaseStream(veChn.mChnId, &stVencStream);
    if (SUCCESS != ret) {
        printf("release stream failed.");
    }
    return TRUE;
}

ERRORTYPE MPP_HELPER_START_NPU(int width, int height, int fps)
{
    ERRORTYPE ret;
    MPP_CHN_S viChn = {MOD_ID_VIU, 8, 0};

    ret = MPP_HELPER_CREATE_VIPP(viChn, width, height, fps, MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420);
    _CHECK_RET(ret);

    AW_MPI_ISP_Run(ISP_DEV);

    ret = MPP_HELPER_ENABLE_VIPP(viChn);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_HELPER_STOP_NPU()
{
    ERRORTYPE ret;
    MPP_CHN_S viChn = {MOD_ID_VIU, 8, 0};

    ret = MPP_HELPER_DISABLE_VIPP(viChn);
    _CHECK_RET(ret);

    AW_MPI_ISP_Stop(ISP_DEV);

    ret = MPP_HELPER_DESTROY_VIPP(viChn);
    _CHECK_RET(ret);

    return ret;
}

static unsigned char y_buffer[NPU_DET_PIC_WIDTH * NPU_DET_PIC_HEIGHT];
static unsigned char uv_buffer[NPU_DET_PIC_WIDTH * NPU_DET_PIC_HEIGHT / 2];
BOOL MPP_HELPER_DO_NPU(void *phuman_run, npu_callback_t npu_callback)
{
    BOOL ret = FALSE;
    MPP_CHN_S viChn = {MOD_ID_VIU, 8, 0};

    VIDEO_FRAME_INFO_S pFrameInfo;
    VideoFrameBufferSizeInfo vfbsInfo;
    AW_MPI_VI_GetFrame(viChn.mDevId, 0, &pFrameInfo, 500);
    memset(&vfbsInfo, 0, sizeof(VideoFrameBufferSizeInfo));
    getVideoFrameBufferSizeInfo(&pFrameInfo, &vfbsInfo);

    //YUV
    if (pFrameInfo.VFrame.mpVirAddr[0])
        memcpy(y_buffer, pFrameInfo.VFrame.mpVirAddr[0], vfbsInfo.mYSize);
    if (pFrameInfo.VFrame.mpVirAddr[1])
        memcpy(uv_buffer, pFrameInfo.VFrame.mpVirAddr[1], vfbsInfo.mUSize);
    if (pFrameInfo.VFrame.mpVirAddr[2])
        memcpy(uv_buffer + vfbsInfo.mUSize, pFrameInfo.VFrame.mpVirAddr[1], vfbsInfo.mVSize);

    if (npu_callback) {
        if (npu_callback(y_buffer, uv_buffer, phuman_run))
            ret = TRUE;
    }
    AW_MPI_VI_ReleaseFrame(viChn.mDevId, 0, &pFrameInfo);

    return ret;
}
