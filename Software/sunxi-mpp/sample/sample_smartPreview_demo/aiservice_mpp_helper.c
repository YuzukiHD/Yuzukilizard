#include <utils/plat_log.h>
#include <utils/plat_math.h>
#include <string.h>
#include <assert.h>
#include <VIDEO_FRAME_INFO_S.h>
#include <mpi_sys.h>
#include <mpi_isp.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_vi.h>
#include "aiservice_mpp_helper.h"
#include "aiservice_detect.h"

ERRORTYPE mpp_helper_init(void)
{
#if 0
    ERRORTYPE ret;
    MPP_SYS_CONF_S stSysConf;

    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);
    ret = AW_MPI_SYS_Init();
    return ret;
#endif
    return SUCCESS;
}

ERRORTYPE mpp_helper_uninit(void)
{
#if 0
    ERRORTYPE ret;
    /* exit mpp systerm */
    ret = AW_MPI_SYS_Exit();
    return ret;
#endif
    return SUCCESS;
}

static ERRORTYPE mpp_event_callback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    if (MOD_ID_VIU == pChn->mModId)
    {
        switch (event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                alogd("receive vi timeout. vipp:%d, chn:%d.", pChn->mDevId, pChn->mChnId);
                break;
            }
            default:
            {
                alogd("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!.", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!.", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
    }

    return SUCCESS;
}

static ERRORTYPE mpp_helper_create_vipp(ISP_DEV isp, MPP_CHN_S viChn, int vipp_buf_num, int width, int height, int fps, PIXEL_FORMAT_E format)
{
    ERRORTYPE ret;
    VI_ATTR_S viAttr;
    MPPCallbackInfo cbinfo;

    memset(&viAttr, 0x00, sizeof(VI_ATTR_S));
    memset(&cbinfo, 0x00, sizeof(MPPCallbackInfo));

    ret = AW_MPI_VI_CreateVipp(viChn.mDevId);
    _CHECK_RET(ret);

    cbinfo.cookie = NULL;
    cbinfo.callback = (MPPCallbackFuncType)&mpp_event_callback;
    ret = AW_MPI_VI_RegisterCallback(viChn.mDevId, &cbinfo);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_GetVippAttr(viChn.mDevId, &viAttr);
    _CHECK_RET(ret);

    memset(&viAttr, 0x00, sizeof(VI_ATTR_S));
    // disable online function, mOnlineShareBufNum is invalid if mOnlineEnable is zero.
    viAttr.mOnlineEnable = 0;
    viAttr.mOnlineShareBufNum = 0;
    viAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    viAttr.memtype = V4L2_MEMORY_MMAP;
    viAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(format);
    viAttr.format.field = V4L2_FIELD_NONE;
    viAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    viAttr.format.width = width;
    viAttr.format.height = height;
    viAttr.fps = fps;
    viAttr.use_current_win = viChn.mDevId > 0 ? 1 : 0;
    viAttr.nbufs = vipp_buf_num;
    viAttr.nplanes = 2;
    viAttr.drop_frame_num = 0;
    ret = AW_MPI_VI_SetVippAttr(viChn.mDevId, &viAttr);
    _CHECK_RET(ret);

    AW_MPI_ISP_Run(isp);

    ret = AW_MPI_VI_EnableVipp(viChn.mDevId);
    _CHECK_RET(ret);

    // if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations must be after stream_on.
//    ret = AW_MPI_VI_SetVippMirror(viChn.mDevId, 0);
//    _CHECK_RET(ret);
//    ret = AW_MPI_VI_SetVippFlip(viChn.mDevId, 0);//0,1
//    _CHECK_RET(ret);

    ret = AW_MPI_VI_CreateVirChn(viChn.mDevId, viChn.mChnId, NULL);
    _CHECK_RET(ret);

    alogd("vipp%d chn%d create success", viChn.mDevId, viChn.mChnId);

    return ret;
}

static ERRORTYPE mpp_helper_destroy_vipp(MPP_CHN_S viChn)
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

static ERRORTYPE mpp_helper_enable_vipp(MPP_CHN_S viChn)
{
    ERRORTYPE ret;
    ret = AW_MPI_VI_EnableVirChn(viChn.mDevId, viChn.mChnId);
    _CHECK_RET(ret);

    return ret;
}

static ERRORTYPE mpp_helper_disable_vipp(MPP_CHN_S viChn)
{
    ERRORTYPE ret;
    ret = AW_MPI_VI_DisableVirChn(viChn.mDevId, viChn.mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE mpp_helper_start_npu(int isp, int vipp, int vi_buf_num, int width, int height, int fps, int format)
{
    ERRORTYPE ret;
    MPP_CHN_S viChn = {MOD_ID_VIU, vipp, 0};

    ret = mpp_helper_create_vipp(isp, viChn, vi_buf_num, width, height, fps, format);
    _CHECK_RET(ret);

    ret = mpp_helper_enable_vipp(viChn);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE mpp_helper_stop_npu(int isp, int vipp)
{
    ERRORTYPE ret;
    MPP_CHN_S viChn = {MOD_ID_VIU, vipp, 0};

    ret = mpp_helper_disable_vipp(viChn);
    _CHECK_RET(ret);

    AW_MPI_ISP_Stop(isp);

    ret = mpp_helper_destroy_vipp(viChn);
    _CHECK_RET(ret);

    return ret;
}

BOOL mpp_helper_do_npu(unsigned char *yuv_buffer, int ch_idx, void *pRunParam, npu_callback_t npu_callback)
{
    BOOL ret = FALSE;
    VIDEO_FRAME_INFO_S pFrameInfo;
    VideoFrameBufferSizeInfo vfbsInfo;

    aialgo_context_t *pctx = get_aicontext();
    assert(pctx != NULL);

    MPP_CHN_S viChn = {MOD_ID_VIU, pctx->attr.ch_info[ch_idx].vipp, 0};

    memset(&pFrameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));
    memset(&vfbsInfo, 0, sizeof(VideoFrameBufferSizeInfo));

    if(AW_MPI_VI_GetFrame(viChn.mDevId, viChn.mChnId, &pFrameInfo, 1000) != SUCCESS)
    {
        //alogw("get frame failure this chance, re-again.");
        return FALSE;
    }
    if (getVideoFrameBufferSizeInfo(&pFrameInfo, &vfbsInfo) != SUCCESS)
    {
        aloge("fatal error, get pixfmnt failure.");
        return FALSE;
    }

    int size = 0;
    if (pctx->use_g2d_flag)
    {
        unsigned int npu_det_pic_width = ALIGN(pctx->human_src_width, 32);
        unsigned int npu_det_pic_height = ALIGN(pctx->human_src_height, 32);
        //alogd("right %d, left %d, top %d, bot %d.", pFrameInfo.VFrame.mOffsetRight, pFrameInfo.VFrame.mOffsetLeft, pFrameInfo.VFrame.mOffsetTop, pFrameInfo.VFrame.mOffsetBottom);
        size = npu_det_pic_width * npu_det_pic_height;
    }
    else
    {
        size = pctx->attr.ch_info[ch_idx].src_width * pctx->attr.ch_info[ch_idx].src_height;
    }

    //YUV
    if (pFrameInfo.VFrame.mpVirAddr[0])
    {
        memcpy(yuv_buffer, pFrameInfo.VFrame.mpVirAddr[0], vfbsInfo.mYSize);
    }
    if (pFrameInfo.VFrame.mpVirAddr[1])
    {
        memcpy(yuv_buffer + size, pFrameInfo.VFrame.mpVirAddr[1], vfbsInfo.mUSize);
    }
    if (pFrameInfo.VFrame.mpVirAddr[2])
    {
        aloge("fatal error, the V component should be null.");
        memcpy(yuv_buffer + size + vfbsInfo.mUSize, pFrameInfo.VFrame.mpVirAddr[2], vfbsInfo.mVSize);
    }

    // return the picture ASAP.
    AW_MPI_VI_ReleaseFrame(viChn.mDevId, viChn.mChnId, &pFrameInfo);

    if (npu_callback)
    {
        if (0 < npu_callback(yuv_buffer, ch_idx, pRunParam))
        {
            ret = TRUE;
        }
    }

    return ret;
}

void paint_object_detect_region_body(BBoxResults_t *res, int ch_idx)
{
    int i = 0;
    RGN_ATTR_S rgnAttr;
    RGN_CHN_ATTR_S rgnChnAttr;

    aialgo_context_t *pctx = get_aicontext();
    assert(pctx != NULL);

    MPP_CHN_S ViChn = {MOD_ID_VIU, pctx->attr.ch_info[ch_idx].draw_orl_vipp, 0};

    alogv("ch_idx=%d, region hdl base=%d, vipp=%d, orl num old=%d cur=%d", ch_idx,
        pctx->region_info[ch_idx].region_hdl_base, pctx->attr.ch_info[ch_idx].draw_orl_vipp,
        pctx->region_info[ch_idx].old_num_of_boxes, res->valid_cnt);

    for (i = 0; i < pctx->region_info[ch_idx].old_num_of_boxes; i ++)
    {
        alogv("ch_idx=%d, detach region hdl=%d, vipp=%d", ch_idx, i + pctx->region_info[ch_idx].region_hdl_base, pctx->attr.ch_info[ch_idx].draw_orl_vipp);
        AW_MPI_RGN_DetachFromChn(i + pctx->region_info[ch_idx].region_hdl_base, &ViChn);
        AW_MPI_RGN_Destroy(i + pctx->region_info[ch_idx].region_hdl_base);
    }

    for (i = 0; i < res->valid_cnt; i++)
    {
        memset(&rgnAttr, 0x00, sizeof(RGN_ATTR_S));
        rgnAttr.enType = ORL_RGN;
        AW_MPI_RGN_Create(i + pctx->region_info[ch_idx].region_hdl_base, &rgnAttr);

        int left   = res->boxes[i].xmin * pctx->attr.ch_info[ch_idx].draw_orl_src_width   / pctx->human_src_width;
        int up     = res->boxes[i].ymin * pctx->attr.ch_info[ch_idx].draw_orl_src_height  / pctx->human_src_height;
        int right  = res->boxes[i].xmax * pctx->attr.ch_info[ch_idx].draw_orl_src_width   / pctx->human_src_width;
        int bottom = res->boxes[i].ymax * pctx->attr.ch_info[ch_idx].draw_orl_src_height  / pctx->human_src_height;

        memset(&rgnChnAttr, 0x00, sizeof(RGN_CHN_ATTR_S));
        rgnChnAttr.unChnAttr.stOrlChn.stRect.X        = left;
        rgnChnAttr.unChnAttr.stOrlChn.stRect.Y        = up;
        rgnChnAttr.unChnAttr.stOrlChn.stRect.Width    = right - left;
        rgnChnAttr.unChnAttr.stOrlChn.stRect.Height   = bottom - up;
        rgnChnAttr.bShow                              = TRUE;
        rgnChnAttr.enType                             = ORL_RGN;
        rgnChnAttr.unChnAttr.stOrlChn.enAreaType      = AREA_RECT;
        rgnChnAttr.unChnAttr.stOrlChn.mColor          = 0x00FF00;
        rgnChnAttr.unChnAttr.stOrlChn.mThick          = 1;
        rgnChnAttr.unChnAttr.stOrlChn.mLayer          = i;
        alogv("ch_idx=%d, attach region hdl=%d, vipp=%d", ch_idx, i + pctx->region_info[ch_idx].region_hdl_base, pctx->attr.ch_info[ch_idx].draw_orl_vipp);
        AW_MPI_RGN_AttachToChn(i + pctx->region_info[ch_idx].region_hdl_base, &ViChn, &rgnChnAttr);
    }

    pctx->region_info[ch_idx].old_num_of_boxes = res->valid_cnt;
    return;
}

void paint_object_detect_region_face(BBoxResults_t *res, int ch_idx)
{
    int i = 0;
    RGN_ATTR_S rgnAttr;
    RGN_CHN_ATTR_S rgnChnAttr;
    
    aialgo_context_t *pctx = get_aicontext();
    assert(pctx != NULL);

    MPP_CHN_S ViChn = {MOD_ID_VIU, pctx->attr.ch_info[ch_idx].draw_orl_vipp, 0};

    alogv("ch_idx=%d, region hdl base=%d, vipp=%d, orl num old=%d cur=%d", ch_idx,
        pctx->region_info[ch_idx].region_hdl_base, pctx->attr.ch_info[ch_idx].draw_orl_vipp,
        pctx->region_info[ch_idx].old_num_of_boxes, res->valid_cnt);

    for (i = 0; i < pctx->region_info[ch_idx].old_num_of_boxes; i ++)
    {
        alogv("ch_idx=%d, detach region hdl=%d, vipp=%d", ch_idx, i + pctx->region_info[ch_idx].region_hdl_base, pctx->attr.ch_info[ch_idx].draw_orl_vipp);
        AW_MPI_RGN_DetachFromChn(i + pctx->region_info[ch_idx].region_hdl_base, &ViChn);
        AW_MPI_RGN_Destroy(i + pctx->region_info[ch_idx].region_hdl_base);
    }

    for (i = 0; i < res->valid_cnt; i++)
    {
        memset(&rgnAttr, 0x00, sizeof(RGN_ATTR_S));
        rgnAttr.enType = ORL_RGN;
        AW_MPI_RGN_Create(i + pctx->region_info[ch_idx].region_hdl_base, &rgnAttr);

        int left   = res->boxes[i].xmin * pctx->attr.ch_info[ch_idx].draw_orl_src_width   / pctx->face_src_width;
        int up     = res->boxes[i].ymin * pctx->attr.ch_info[ch_idx].draw_orl_src_height  / pctx->face_src_height;
        int right  = res->boxes[i].xmax * pctx->attr.ch_info[ch_idx].draw_orl_src_width   / pctx->face_src_width;
        int bottom = res->boxes[i].ymax * pctx->attr.ch_info[ch_idx].draw_orl_src_height  / pctx->face_src_height;

        memset(&rgnChnAttr, 0x00, sizeof(RGN_CHN_ATTR_S));
        rgnChnAttr.unChnAttr.stOrlChn.stRect.X        = left;
        rgnChnAttr.unChnAttr.stOrlChn.stRect.Y        = up;
        rgnChnAttr.unChnAttr.stOrlChn.stRect.Width    = right - left;
        rgnChnAttr.unChnAttr.stOrlChn.stRect.Height   = bottom - up;
        rgnChnAttr.bShow                              = TRUE;
        rgnChnAttr.enType                             = ORL_RGN;
        rgnChnAttr.unChnAttr.stOrlChn.enAreaType      = AREA_RECT;
        rgnChnAttr.unChnAttr.stOrlChn.mColor          = 0xFF0000;
        rgnChnAttr.unChnAttr.stOrlChn.mThick          = 1;
        rgnChnAttr.unChnAttr.stOrlChn.mLayer          = i;
        alogv("ch_idx=%d, attach region hdl=%d, vipp=%d", ch_idx, i + pctx->region_info[ch_idx].region_hdl_base, pctx->attr.ch_info[ch_idx].draw_orl_vipp);
        AW_MPI_RGN_AttachToChn(i + pctx->region_info[ch_idx].region_hdl_base, &ViChn, &rgnChnAttr);
    }

    pctx->region_info[ch_idx].old_num_of_boxes = res->valid_cnt;
    return;
}
