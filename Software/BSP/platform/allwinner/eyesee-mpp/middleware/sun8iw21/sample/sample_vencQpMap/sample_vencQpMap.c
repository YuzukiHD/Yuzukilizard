// #define LOG_NDEBUG 0
#define LOG_TAG "sample_vencQpMap"

#include "sample_vencQpMap.h"
#include "plat_log.h"
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <mm_common.h>
#include <mpi_sys.h>
#include <plat_math.h>
#include <memoryAdapter.h>
#include "sc_interface.h"
#include "ion_memmanager.h"
#include "sunxi_camera_v2.h"

/**
 *  This macro contains operations related to QPMAP.
 */
#define TEST_QPMAP

// #define DEBUG_QP

#define USER_SET_MB_QP_DEFAULT   (40)
#define USER_SET_OSD_QP_DEFAULT  (20)

static SampleVencQPMAPContext *gpVencQPMAPContext = NULL;

#if 1 /* mem func */
static int venc_MemOpen(void)
{
    return ion_memOpen();
}

static int venc_MemClose(void)
{
    return ion_memClose();
}

static unsigned char* venc_allocMem(unsigned int size)
{
    IonAllocAttr allocAttr;
    memset(&allocAttr, 0, sizeof(IonAllocAttr));
    allocAttr.mLen = size;
    allocAttr.mAlign = 0;
    allocAttr.mIonHeapType = IonHeapType_IOMMU;
    allocAttr.mbSupportCache = 0;
    return ion_allocMem_extend(&allocAttr);
}

static int venc_freeMem(void *vir_ptr)
{
    return ion_freeMem(vir_ptr);
}

static unsigned int venc_getPhyAddrByVirAddr(void *vir_ptr)
{
    return ion_getMemPhyAddr(vir_ptr);
}

static int venc_flushCache(void *vir_ptr, unsigned int size)
{
    return ion_flushCache(vir_ptr, size);
}
#endif

static int parseCmdLine(SampleVencQPMAPContext *pContext, int argc, char** argv)
{
    int ret = -1;

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = 0;
              if (strlen(*argv) >= MAX_FILE_PATH_SIZE)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_SIZE-1);
              pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_venc.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static PIXEL_FORMAT_E getPicFormatFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PIXEL_FORMAT_E PicFormat = MM_PIXEL_FORMAT_BUTT;
    char *pStrPixelFormat = (char*)GetConfParaString(pConfParser, key, NULL);

    if (!strcmp(pStrPixelFormat, "yu12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "yv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv21"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv21s"))
    {
        PicFormat = MM_PIXEL_FORMAT_AW_NV21S;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }

    return PicFormat;
}

static PAYLOAD_TYPE_E getEncoderTypeFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PAYLOAD_TYPE_E EncType = PT_BUTT;
    char *ptr = (char *)GetConfParaString(pConfParser, key, NULL);
    if(!strcmp(ptr, "H.264"))
    {
        alogd("H.264");
        EncType = PT_H264;
    }
    else if(!strcmp(ptr, "H.265"))
    {
        alogd("H.265");
        EncType = PT_H265;
    }
    else
    {
        aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
        EncType = PT_H264;
    }

    return EncType;
}

static ERRORTYPE loadConfigPara(SampleVencQPMAPContext *pContext, const char *conf_path)
{
    ERRORTYPE ret = SUCCESS;
    CONFPARSER_S stConfParser;    
    char *ptr = NULL;

    pContext->mConfigPara.mBitmapFormat = MM_PIXEL_FORMAT_RGB_8888;
    pContext->mConfigPara.overlay_x = 640;
    pContext->mConfigPara.overlay_y = 320;
    pContext->mConfigPara.overlay_w = 160;
    pContext->mConfigPara.overlay_h = 80;
    pContext->mConfigPara.mVencRoiEnable = 1;

    pContext->mConfigPara.cover_x = 80;
    pContext->mConfigPara.cover_y = 160;
    pContext->mConfigPara.cover_w = 160;
    pContext->mConfigPara.cover_h = 320;

    if (NULL == pContext)
    {
        aloge("pContext is NULL!");
        return FAILURE;
    }

    if (NULL == conf_path)
    {
        aloge("user not set config file!");
        return FAILURE;
    }

    if (0 > createConfParser(conf_path, &stConfParser))
    {
        aloge("load conf fail!");
        return FAILURE;
    }

    pContext->mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_VENC_QPMAP_TEST_DURATION, 0);
    ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_VENC_QPMAP_SRC_FILE_STR, NULL);
    strncpy(pContext->mConfigPara.inputFile, ptr, MAX_FILE_PATH_SIZE);
    pContext->mConfigPara.srcWidth  = GetConfParaInt(&stConfParser, SAMPLE_VENC_QPMAP_SRC_WIDTH, 0);
    pContext->mConfigPara.srcHeight = GetConfParaInt(&stConfParser, SAMPLE_VENC_QPMAP_SRC_HEIGHT, 0);
    pContext->mConfigPara.srcPixFmt = getPicFormatFromConfig(&stConfParser, SAMPLE_VENC_QPMAP_SRC_PIXFMT);

    ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_VENC_QPMAP_COLOR_SPACE, NULL);
    if (!strcmp(ptr, "jpeg"))
    {
        pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
    }
    else if (!strcmp(ptr, "rec709"))
    {
        pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709;
    }
    else if (!strcmp(ptr, "rec709_part_range"))
    {
        pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
    }
    else
    {
        aloge("fatal error! wrong color space:%s", ptr);
        pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
    }

    ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_VENC_QPMAP_DST_FILE_STR, NULL);
    strncpy(pContext->mConfigPara.outputFile, ptr, MAX_FILE_PATH_SIZE);
    pContext->mConfigPara.dstWidth  = GetConfParaInt(&stConfParser, SAMPLE_VENC_QPMAP_DST_WIDTH, 0);
    pContext->mConfigPara.dstHeight = GetConfParaInt(&stConfParser, SAMPLE_VENC_QPMAP_DST_HEIGHT, 0);

    pContext->mConfigPara.mVideoEncoderFmt = getEncoderTypeFromConfig(&stConfParser, SAMPLE_VENC_QPMAP_ENCODER);
    pContext->mConfigPara.mEncUseProfile = GetConfParaInt(&stConfParser, SAMPLE_VENC_QPMAP_ENC_PROFILE, 0);
    pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&stConfParser, SAMPLE_VENC_QPMAP_FRAMERATE, 0);
    pContext->mConfigPara.mVideoBitRate = GetConfParaInt(&stConfParser, SAMPLE_VENC_QPMAP_BITRATE, 0);

    alogd("user configuration: testDuration=%ds, enc_fmt=%d, profile=%d, src_pix_fmt=%d\n"
        "src=%s, dst=%s, src_size=%dx%d, dst_size=%dx%d, framerate=%d, bitrate=%d",
        pContext->mConfigPara.mTestDuration, pContext->mConfigPara.mVideoEncoderFmt,
        pContext->mConfigPara.mEncUseProfile, pContext->mConfigPara.srcPixFmt,
        pContext->mConfigPara.inputFile, pContext->mConfigPara.outputFile, pContext->mConfigPara.srcWidth, pContext->mConfigPara.srcHeight,
        pContext->mConfigPara.dstWidth, pContext->mConfigPara.dstHeight,
        pContext->mConfigPara.mVideoFrameRate, pContext->mConfigPara.mVideoBitRate);

#ifdef TEST_QPMAP
    pContext->mConfigPara.mRcMode = 3; // for H264/H265: 0->CBR;  1->VBR  2->FIXQP  3->QPMAP; for mjpeg  0->CBR  1->FIXQP.
    alogd("test QPMAP");
#else
    pContext->mConfigPara.mRcMode = 0;
    alogd("test CBR");
#endif

    alogd("defalut configuration: rc_mode=%d", pContext->mConfigPara.mRcMode);

    switch(pContext->mConfigPara.mRcMode)
    {
        case 0: // CBR
        {
            pContext->mConfigPara.nMinqp = 10;
            pContext->mConfigPara.nMaxqp = 40;
            pContext->mConfigPara.nMinPqp = 10;
            pContext->mConfigPara.nMaxPqp = 50;
            pContext->mConfigPara.nQpInit = 30;
            pContext->mConfigPara.bEnMbQpLimit = 0;
            alogd("Minqp=%d, Maxqp=%d, MinPqp=%d, MaxPqp=%d, QpInit=%d, EnMbQpLimit=%d",
                pContext->mConfigPara.nMinqp, pContext->mConfigPara.nMaxqp,
                pContext->mConfigPara.nMinPqp, pContext->mConfigPara.nMaxPqp,
                pContext->mConfigPara.nQpInit, pContext->mConfigPara.bEnMbQpLimit);
            break;
        }
        case 1: // VBR
        {
            pContext->mConfigPara.nMinqp = 10;
            pContext->mConfigPara.nMaxqp = 40;
            pContext->mConfigPara.nMinPqp = 10;
            pContext->mConfigPara.nMaxPqp = 50;
            pContext->mConfigPara.nQpInit = 30;
            pContext->mConfigPara.bEnMbQpLimit = 0;
            pContext->mConfigPara.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mConfigPara.mMovingTh = 20;
            pContext->mConfigPara.mQuality = 5;
            pContext->mConfigPara.mIFrmBitsCoef = 0;
            pContext->mConfigPara.mPFrmBitsCoef = 0;
            alogd("Minqp=%d, Maxqp=%d, MinPqp=%d, MaxPqp=%d, QpInit=%d, EnMbQpLimit=%d",
                pContext->mConfigPara.nMinqp, pContext->mConfigPara.nMaxqp,
                pContext->mConfigPara.nMinPqp, pContext->mConfigPara.nMaxPqp,
                pContext->mConfigPara.nQpInit, pContext->mConfigPara.bEnMbQpLimit);
            alogd("MaxBitRate=%d, MovingTh=%d, Quality=%d, IFrmBitsCoef=%d, PFrmBitsCoef=%d",
                pContext->mConfigPara.mMaxBitRate,
                pContext->mConfigPara.mMovingTh, pContext->mConfigPara.mQuality,
                pContext->mConfigPara.mIFrmBitsCoef, pContext->mConfigPara.mPFrmBitsCoef);
            break;
        }
        case 2: // FIXQP
        {
            pContext->mConfigPara.mIQp = 30;
            pContext->mConfigPara.mPQp = 30;
            alogd("IQp=%d, PQp=%d", pContext->mConfigPara.mIQp, pContext->mConfigPara.mPQp);
            break;
        }
        case 3: // QPMAP
        {
            pContext->mConfigPara.nMinqp = 10;
            pContext->mConfigPara.nMaxqp = 40;
            pContext->mConfigPara.nMinPqp = 10;
            pContext->mConfigPara.nMaxPqp = 50;
            pContext->mConfigPara.nQpInit = 30;
            pContext->mConfigPara.bEnMbQpLimit = 0;
            alogd("Minqp=%d, Maxqp=%d, MinPqp=%d, MaxPqp=%d, QpInit=%d, EnMbQpLimit=%d",
                pContext->mConfigPara.nMinqp, pContext->mConfigPara.nMaxqp,
                pContext->mConfigPara.nMinPqp, pContext->mConfigPara.nMaxPqp,
                pContext->mConfigPara.nQpInit, pContext->mConfigPara.bEnMbQpLimit);
            break;
        }
        default:
        {
            aloge("fatal error! wrong h264 rc mode[0x%x], check code!", pContext->mConfigPara.mRcMode);
            ret = FAILURE;
            break;
        }
    }

    destroyConfParser(&stConfParser);
    return ret;
}

#ifdef TEST_QPMAP

#if (AWCHIP == AW_V853)
static void MBParamInit(VencMBModeCtrl *pMBModeCtrl, sGetMbInfo *pGetMbInfo, SampleVencQPMAPConfig *pConfigPara, unsigned int *pMbNum)
{
    unsigned int mb_num = 0;

    if ((NULL == pMBModeCtrl) || (NULL == pGetMbInfo) || (NULL == pConfigPara) || (NULL == pMbNum))
    {
        aloge("fatal error! invalid input params! %p,%p,%p,%p", pMBModeCtrl, pGetMbInfo, pConfigPara, pMbNum);
        return;
    }

    // calc mb num and malloc GetMbInfo buffer
    if (PT_H264 == pConfigPara->mVideoEncoderFmt || PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width * pic_mb_height;
        mb_num = total_mb_num;

        if (NULL == pGetMbInfo->mb)
        {
            pGetMbInfo->mb = (sH264GetMbInfo *)malloc(total_mb_num * sizeof(sH264GetMbInfo));
            if (NULL == pGetMbInfo->mb)
            {
                aloge("fatal error! malloc mb fail!");
                return;
            }
            memset(pGetMbInfo->mb, 0, total_mb_num * sizeof(sH264GetMbInfo));
            alogd("pGetMbInfo->mb=%p", pGetMbInfo->mb);
        }
        else
        {
            alogw("init mb_ctu repeated? pGetMbInfo->mb is not NULL!");
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }

    // malloc mb mode p_map_info
    if (NULL == pMBModeCtrl->p_map_info)
    {
        pMBModeCtrl->p_map_info = malloc(sizeof(VencMBModeCtrlInfo) * mb_num);
        if (NULL == pMBModeCtrl->p_map_info)
        {
            aloge("fatal error! malloc pMBModeCtrl->p_map_info fail! size=%d", sizeof(VencMBModeCtrlInfo) * mb_num);
            return;
        }
    }
    else
    {
        alogw("init mb mode repeated? pMBModeCtrl->p_map_info is not NULL!");
    }
    memset(pMBModeCtrl->p_map_info, 0, sizeof(VencMBModeCtrlInfo) * mb_num);

    *pMbNum = mb_num;

    pMBModeCtrl->mode_ctrl_en = 1;
    alogd("mb_num=%d, pMBModeCtrl->p_map_info=%p, pMBModeCtrl->mode_ctrl_en=%d", mb_num, pMBModeCtrl->p_map_info, pMBModeCtrl->mode_ctrl_en);
}

static void MBParamDeinit(VencMBModeCtrl *pMBModeCtrl, sGetMbInfo *pGetMbInfo, SampleVencQPMAPConfig *pConfigPara)
{
    if ((NULL == pMBModeCtrl) || (NULL == pGetMbInfo) || (NULL == pConfigPara))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", pMBModeCtrl, pGetMbInfo, pConfigPara);
        return;
    }

    if (pMBModeCtrl->p_map_info)
    {
        free(pMBModeCtrl->p_map_info);
        pMBModeCtrl->p_map_info = NULL;
    }
    pMBModeCtrl->mode_ctrl_en = 0;

    if (PT_H264 == pConfigPara->mVideoEncoderFmt || PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        if (pGetMbInfo->mb)
        {
            free(pGetMbInfo->mb);
            pGetMbInfo->mb = NULL;
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void ParseMbMadQpSSE(unsigned char *p_mb_mad_qp_sse, SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == p_mb_mad_qp_sse) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", p_mb_mad_qp_sse, pConfigPara, pGetMbInfo);
        return;
    }

    unsigned char *mb_addr = p_mb_mad_qp_sse;

    if (PT_H264 == pConfigPara->mVideoEncoderFmt || PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width * pic_mb_height;

        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_mb_width << 4) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_mb_width << 4));
        }

        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        unsigned int cnt = 0;
        for (cnt = 0; cnt < total_mb_num; cnt++)
        {
            mb[cnt].mad = mb_addr[0];
            mb[cnt].qp  = mb_addr[1] & 0x3F;
            mb[cnt].sse = (((unsigned int)mb_addr[3]) << 16) + (((unsigned int)mb_addr[4]) << 8) + (unsigned int)mb_addr[5];
            mb_addr += 6;
#ifdef DEBUG_QP
            if (0 == (cnt % 100))
                alogd("GET mb[%d] QP=%d MAD=%d SSE=%d", cnt, mb[cnt].qp, mb[cnt].mad, mb[cnt].sse);
#endif
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void ParseMbBinImg(unsigned char *p_mb_bin_img, SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == p_mb_bin_img) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", p_mb_bin_img, pConfigPara, pGetMbInfo);
        return;
    }

    if (PT_H264 == pConfigPara->mVideoEncoderFmt || PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        sH264ParcelMbBinImg *parcel = (sH264ParcelMbBinImg *)p_mb_bin_img;
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_width_2align = ALIGN(pic_mb_width, 2);
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width_2align * pic_mb_height;

        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_mb_width_2align << 4) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_mb_width_2align << 4));
        }

        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        unsigned int cnt = 0, x = 0, y = 0;
        for (y = 0; y < pic_mb_height; y++)
        {
            for (x = 0; x < pic_mb_width_2align; x+=2)
            {
                mb[cnt].sub[0].sad = parcel->mb0_sad0;
                mb[cnt].sub[1].sad = parcel->mb0_sad1;
                mb[cnt].sub[2].sad = parcel->mb0_sad2;
                mb[cnt].sub[3].sad = parcel->mb0_sad3;
                if (x+1 < pic_mb_width)
                {
                    mb[cnt+1].sub[0].sad = parcel->mb1_sad0;
                    mb[cnt+1].sub[1].sad = parcel->mb1_sad1;
                    mb[cnt+1].sub[2].sad = parcel->mb1_sad2;
                    mb[cnt+1].sub[3].sad = parcel->mb1_sad3;
                }
                cnt += 2;
                parcel += 2;
            }
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void ParseMbMv(unsigned char *p_mb_mv, SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == p_mb_mv) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", p_mb_mv, pConfigPara, pGetMbInfo);
        return;
    }

    if (PT_H264 == pConfigPara->mVideoEncoderFmt || PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        sH264ParcelMbMvInfo *parcel = (sH264ParcelMbMvInfo *)p_mb_mv;
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_width_4align = ALIGN(pic_mb_width, 4);
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width_4align * pic_mb_height;
        
        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_mb_width_4align << 4) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_mb_width_4align << 4));
        }

        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        unsigned int cnt = 0, x = 0, y = 0, w = 0, k = 0;
        for (y = 0; y < pic_mb_height; y++)
        {
            for (x = 0; x < pic_mb_width_4align; x++)
            {
                if (x < pic_mb_width)
                {
                    mb[cnt].type = parcel->pred_type & 0x1; // intra:1; inter:0
                    mb[cnt].min_cost = parcel->min_cost; // Minimal overhead obtained by using this prediction mode.
                    if (mb[cnt].type == 0)
                    {
                        unsigned int submvx[4] = {parcel->blk0_mv_x & 0x1F,
                                                  parcel->blk1_mv_x & 0x1F,
                                                  parcel->blk2_mv_x & 0x1F,
                                                  parcel->blk3_mv_x & 0x1F};

                        unsigned int submvy[4] = {parcel->blk0_mv_y & 0x1F,
                                                  parcel->blk1_mv_y & 0x1F,
                                                  parcel->blk2_mv_y & 0x1F,
                                                  parcel->blk3_mv_y & 0x1F};

                        unsigned int cen_mvx = parcel->cen_mv_x_rs2 & 0xFF;
                        unsigned int cen_mvy = parcel->cen_mv_y_rs2 & 0x7F;

                        cen_mvx = 4 * ((cen_mvx & 0x80) ? (cen_mvx | (~0x7F)) : cen_mvx);
                        cen_mvy = 4 * ((cen_mvy & 0x40) ? (cen_mvy | (~0x3F)) : cen_mvy);
                        for (k = 0; k < 4; k++)
                        {
                            submvx[k] = (submvx[k] & 0x10) ? (submvx[k] | (~0x1F)) : submvx[k];
                            mb[cnt].sub[k].mv_x = submvx[k] + cen_mvx;
                            submvy[k] = (submvy[k] & 0x10) ? (submvy[k] | (~0x1F)) : submvy[k];
                            mb[cnt].sub[k].mv_y = submvy[k] + cen_mvy;
                        }
                    }
                }
                parcel++;
                cnt++;
            }
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void FillQpMapInfo(VencMBModeCtrl *pMBModeCtrl, SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == pMBModeCtrl) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", pMBModeCtrl, pConfigPara, pGetMbInfo);
        return;
    }

    if ((NULL == pMBModeCtrl->p_map_info) || (0 == pMBModeCtrl->mode_ctrl_en))
    {
        aloge("fatal error! invalid input param! p_map_info=%p, mode_ctrl_en=%d",
            pMBModeCtrl->p_map_info, pMBModeCtrl->mode_ctrl_en);
        return;
    }

    if (PT_H264 == pConfigPara->mVideoEncoderFmt || PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        sParcelMbQpMap *parcel = (sParcelMbQpMap *)pMBModeCtrl->p_map_info;
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num =  pic_mb_width * pic_mb_height;

        unsigned int osd_mb_width = ALIGN(pConfigPara->overlay_w, 16) >> 4;
        unsigned int osd_mb_height = ALIGN(pConfigPara->overlay_h, 16) >> 4;
        unsigned int osd_mb_x = ALIGN(pConfigPara->overlay_x, 16) >> 4;
        unsigned int osd_mb_y = ALIGN(pConfigPara->overlay_y, 16) >> 4;

        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_mb_width << 4) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_mb_width << 4));
        }

        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        unsigned int cnt = 0, x = 0, y = 0;
        for (y = 0; y < pic_mb_height; y++)
        {
            for (x = 0; x < pic_mb_width; x++)
            {
                if ((osd_mb_x < x) && (x < osd_mb_x + osd_mb_width) &&
                    (osd_mb_y < y) && (y < osd_mb_y + osd_mb_height))
                {
                    parcel[cnt].qp   = USER_SET_OSD_QP_DEFAULT;//mb[cnt].qp; // user set qp
                    parcel[cnt].skip = 0;  // user set skip
                    parcel[cnt].en   = 1;  // user set en
                }
                else
                {
                    parcel[cnt].qp   = USER_SET_MB_QP_DEFAULT;//mb[cnt].qp; // user set qp
                    parcel[cnt].skip = 1;  // user set skip
                    parcel[cnt].en   = 1;  // user set en
                }
                cnt++;
#ifdef DEBUG_QP
                if (0 == (cnt % 100))
                    alogd("SET mb[%d] QP=%d", cnt, parcel[cnt].qp);
#endif
            }
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void QpMapInfoUserProcessFunc(SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p", pConfigPara, pGetMbInfo);
        return;
    }

    if (PT_H264 == pConfigPara->mVideoEncoderFmt || PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        // The user can modify QP of a macro block. At the same time, user can set whether to
        // skip this macro block, or whether to enable this macro block.
        // The specific modification strategy needs to be judged by the user.
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}
#else
static void MBParamInit(VencMBModeCtrl *pMBModeCtrl, sGetMbInfo *pGetMbInfo, SampleVencQPMAPConfig *pConfigPara, unsigned int *pMbNum)
{
    unsigned int mb_num = 0;

    if ((NULL == pMBModeCtrl) || (NULL == pGetMbInfo) || (NULL == pConfigPara) || (NULL == pMbNum))
    {
        aloge("fatal error! invalid input params! %p,%p,%p,%p", pMBModeCtrl, pGetMbInfo, pConfigPara, pMbNum);
        return;
    }

    // calc mb num and malloc GetMbInfo buffer
    if (PT_H264 == pConfigPara->mVideoEncoderFmt)
    {
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width * pic_mb_height;
        mb_num = total_mb_num;

        if (NULL == pGetMbInfo->mb)
        {
            pGetMbInfo->mb = (sH264GetMbInfo *)malloc(total_mb_num * sizeof(sH264GetMbInfo));
            if (NULL == pGetMbInfo->mb)
            {
                aloge("fatal error! malloc mb fail!");
                return;
            }
            memset(pGetMbInfo->mb, 0, total_mb_num * sizeof(sH264GetMbInfo));
            alogd("pGetMbInfo->mb=%p", pGetMbInfo->mb);
        }
        else
        {
            alogw("init mb_ctu repeated? pGetMbInfo->mb is not NULL!");
        }
    }
    else if (PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        unsigned int pic_4ctu_width = ALIGN(pConfigPara->dstWidth, 128) >> 5;
        unsigned int pic_ctu_height = ALIGN(pConfigPara->dstHeight, 32) >> 5;
        unsigned int total_ctu_num = pic_4ctu_width * pic_ctu_height;
        mb_num = total_ctu_num;

        if (NULL == pGetMbInfo->ctu)
        {
            pGetMbInfo->ctu = (sH265GetCtuInfo *)malloc(total_ctu_num * sizeof(sH265GetCtuInfo));
            if (NULL == pGetMbInfo->ctu)
            {
                aloge("fatal error! malloc ctu fail!");
                return;
            }
            memset(pGetMbInfo->ctu, 0, total_ctu_num * sizeof(sH265GetCtuInfo));
            alogd("pGetMbInfo->ctu=%p", pGetMbInfo->ctu);
        }
        else
        {
            alogw("init mb_ctu repeated? pGetMbInfo->ctu is not NULL!");
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }

    // malloc mb mode p_map_info
    if (NULL == pMBModeCtrl->p_map_info)
    {
        pMBModeCtrl->p_map_info = malloc(sizeof(VencMBModeCtrlInfo) * mb_num);
        if (NULL == pMBModeCtrl->p_map_info)
        {
            aloge("fatal error! malloc pMBModeCtrl->p_map_info fail! size=%d", sizeof(VencMBModeCtrlInfo) * mb_num);
            return;
        }
    }
    else
    {
        alogw("init mb mode repeated? pMBModeCtrl->p_map_info is not NULL!");
    }
    memset(pMBModeCtrl->p_map_info, 0, sizeof(VencMBModeCtrlInfo) * mb_num);

    *pMbNum = mb_num;

    pMBModeCtrl->mode_ctrl_en = 1;
    alogd("mb_num=%d, pMBModeCtrl->p_map_info=%p, pMBModeCtrl->mode_ctrl_en=%d", mb_num, pMBModeCtrl->p_map_info, pMBModeCtrl->mode_ctrl_en);
}

static void MBParamDeinit(VencMBModeCtrl *pMBModeCtrl, sGetMbInfo *pGetMbInfo, SampleVencQPMAPConfig *pConfigPara)
{
    if ((NULL == pMBModeCtrl) || (NULL == pGetMbInfo) || (NULL == pConfigPara))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", pMBModeCtrl, pGetMbInfo, pConfigPara);
        return;
    }

    if (pMBModeCtrl->p_map_info)
    {
        free(pMBModeCtrl->p_map_info);
        pMBModeCtrl->p_map_info = NULL;
    }
    pMBModeCtrl->mode_ctrl_en = 0;

    if (PT_H264 == pConfigPara->mVideoEncoderFmt)
    {
        if (pGetMbInfo->mb)
        {
            free(pGetMbInfo->mb);
            pGetMbInfo->mb = NULL;
        }
    }
    else if (PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        if (pGetMbInfo->ctu)
        {
            free(pGetMbInfo->ctu);
            pGetMbInfo->ctu = NULL;
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void ParseMbMadQpSSE(unsigned char *p_mb_mad_qp_sse, SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == p_mb_mad_qp_sse) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", p_mb_mad_qp_sse, pConfigPara, pGetMbInfo);
        return;
    }

    unsigned char *mb_addr = p_mb_mad_qp_sse;

    if (PT_H264 == pConfigPara->mVideoEncoderFmt)
    {
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width * pic_mb_height;

        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_mb_width << 4) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_mb_width << 4));
        }

        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        unsigned int cnt = 0;
        for (cnt = 0; cnt < total_mb_num; cnt++)
        {
            mb[cnt].mad = mb_addr[0];
            mb[cnt].qp  = mb_addr[1] & 0x3F;
            mb[cnt].sse = (((unsigned int)mb_addr[3]) << 16) + (((unsigned int)mb_addr[4]) << 8) + (unsigned int)mb_addr[5];
            mb_addr += 6;
#ifdef DEBUG_QP
            if (0 == (cnt % 100))
                alogd("GET mb[%d] QP=%d MAD=%d SSE=%d", cnt, mb[cnt].qp, mb[cnt].mad, mb[cnt].sse);
#endif
        }
    }
    else if (PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        unsigned int pic_4ctu_width = ALIGN(pConfigPara->dstWidth, 128) >> 5;
        unsigned int pic_ctu_height = ALIGN(pConfigPara->dstHeight, 32) >> 5;
        unsigned int total_ctu_num = pic_4ctu_width * pic_ctu_height;

        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_4ctu_width << 5) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_4ctu_width << 5));
        }

        sH265GetCtuInfo *ctu = (sH265GetCtuInfo *)pGetMbInfo->ctu;
        unsigned int ctu_cnt = 0, mb_cnt = 0;
        for (ctu_cnt = 0; ctu_cnt < total_ctu_num; ctu_cnt++)
        {
            for (mb_cnt = 0; mb_cnt < 4; mb_cnt++)
            {
                ctu[ctu_cnt].mb[mb_cnt].mad = mb_addr[0];
                ctu[ctu_cnt].mb[mb_cnt].qp  = mb_addr[1] & 0x3F;
                ctu[ctu_cnt].mb[mb_cnt].sse = (((unsigned int)mb_addr[3]) << 16) + (((unsigned int)mb_addr[4]) << 8) + (unsigned int)mb_addr[5];
                mb_addr += 6;
#ifdef DEBUG_QP
                if (0 == (ctu_cnt % 50))
                    alogd("GET ctu[%d].mb[%d] QP=%d MAD=%d SSE=%d", ctu_cnt, mb_cnt, ctu[ctu_cnt].mb[mb_cnt].qp, ctu[ctu_cnt].mb[mb_cnt].mad, ctu[ctu_cnt].mb[mb_cnt].sse);
#endif
            }
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void ParseMbBinImg(unsigned char *p_mb_bin_img, SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == p_mb_bin_img) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", p_mb_bin_img, pConfigPara, pGetMbInfo);
        return;
    }

    if (PT_H264 == pConfigPara->mVideoEncoderFmt)
    {
        sH264ParcelMbBinImg *parcel = (sH264ParcelMbBinImg *)p_mb_bin_img;
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_width_2align = ALIGN(pic_mb_width, 2);
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width_2align * pic_mb_height;

        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_mb_width_2align << 4) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_mb_width_2align << 4));
        }

        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        unsigned int cnt = 0, x = 0, y = 0;
        for (y = 0; y < pic_mb_height; y++)
        {
            for (x = 0; x < pic_mb_width_2align; x+=2)
            {
                mb[cnt].sub[0].sad = parcel->mb0_sad0;
                mb[cnt].sub[1].sad = parcel->mb0_sad1;
                mb[cnt].sub[2].sad = parcel->mb0_sad2;
                mb[cnt].sub[3].sad = parcel->mb0_sad3;
                if (x+1 < pic_mb_width)
                {
                    mb[cnt+1].sub[0].sad = parcel->mb1_sad0;
                    mb[cnt+1].sub[1].sad = parcel->mb1_sad1;
                    mb[cnt+1].sub[2].sad = parcel->mb1_sad2;
                    mb[cnt+1].sub[3].sad = parcel->mb1_sad3;
                }
                cnt += 2;
                parcel += 2;
            }
        }
    }
    else if (PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        sH265ParcelMbBinImg *parcel = (sH265ParcelMbBinImg *)p_mb_bin_img;
        unsigned int pic_ctu_width = ALIGN(pConfigPara->dstWidth, 32) >> 5;
        unsigned int pic_4ctu_width = ALIGN(pConfigPara->dstWidth, 128) >> 5;
        unsigned int pic_ctu_height = ALIGN(pConfigPara->dstHeight, 32) >> 5;
        unsigned int total_ctu_num = pic_4ctu_width * pic_ctu_height;
        
        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_4ctu_width << 5) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_4ctu_width << 5));
        }

        sH265GetCtuInfo *ctu = (sH265GetCtuInfo *)pGetMbInfo->ctu;
        unsigned int cnt = 0, x = 0, y = 0;
        for (y = 0; y < pic_ctu_height; y++)
        {
            for (x = 0; x < pic_4ctu_width; x++)
            {
                if (x < pic_ctu_width)
                {
                    ctu[cnt].mb[0].sad[0] = parcel->mb0_sad0;
                    ctu[cnt].mb[0].sad[1] = parcel->mb0_sad1;
                    ctu[cnt].mb[1].sad[0] = parcel->mb1_sad0;
                    ctu[cnt].mb[1].sad[1] = parcel->mb1_sad1;
                    ctu[cnt].mb[0].sad[2] = parcel->mb0_sad2;
                    ctu[cnt].mb[0].sad[3] = parcel->mb0_sad3;
                    ctu[cnt].mb[2].sad[0] = parcel->mb2_sad0;
                    ctu[cnt].mb[2].sad[1] = parcel->mb2_sad1;
                    ctu[cnt].mb[3].sad[0] = parcel->mb3_sad0;
                    ctu[cnt].mb[3].sad[1] = parcel->mb3_sad1;
                    ctu[cnt].mb[2].sad[2] = parcel->mb2_sad2;
                    ctu[cnt].mb[2].sad[3] = parcel->mb2_sad3;
                    ctu[cnt].mb[3].sad[2] = parcel->mb3_sad2;
                    ctu[cnt].mb[3].sad[3] = parcel->mb3_sad3;
                }
                cnt++;
                parcel++;
            }
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void ParseMbMv(unsigned char *p_mb_mv, SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == p_mb_mv) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", p_mb_mv, pConfigPara, pGetMbInfo);
        return;
    }

    if (PT_H264 == pConfigPara->mVideoEncoderFmt)
    {
        sH264ParcelMbMvInfo *parcel = (sH264ParcelMbMvInfo *)p_mb_mv;
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_width_4align = ALIGN(pic_mb_width, 4);
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width_4align * pic_mb_height;
        
        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_mb_width_4align << 4) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_mb_width_4align << 4));
        }

        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        unsigned int cnt = 0, x = 0, y = 0, w = 0, k = 0;
        for (y = 0; y < pic_mb_height; y++)
        {
            for (x = 0; x < pic_mb_width_4align; x++)
            {
                if (x < pic_mb_width)
                {
                    mb[cnt].type = parcel->pred_type & 0x1; // intra:1; inter:0
                    mb[cnt].min_cost = parcel->min_cost; // Minimal overhead obtained by using this prediction mode.
                    if (mb[cnt].type == 0)
                    {
                        unsigned int submvx[4] = {parcel->blk0_mv_x & 0x1F,
                                                  parcel->blk1_mv_x & 0x1F,
                                                  parcel->blk2_mv_x & 0x1F,
                                                  parcel->blk3_mv_x & 0x1F};

                        unsigned int submvy[4] = {parcel->blk0_mv_y & 0x1F,
                                                  parcel->blk1_mv_y & 0x1F,
                                                  parcel->blk2_mv_y & 0x1F,
                                                  parcel->blk3_mv_y & 0x1F};

                        unsigned int cen_mvx = parcel->cen_mv_x_rs2 & 0xFF;
                        unsigned int cen_mvy = parcel->cen_mv_y_rs2 & 0x7F;

                        cen_mvx = 4 * ((cen_mvx & 0x80) ? (cen_mvx | (~0x7F)) : cen_mvx);
                        cen_mvy = 4 * ((cen_mvy & 0x40) ? (cen_mvy | (~0x3F)) : cen_mvy);
                        for (k = 0; k < 4; k++)
                        {
                            submvx[k] = (submvx[k] & 0x10) ? (submvx[k] | (~0x1F)) : submvx[k];
                            mb[cnt].sub[k].mv_x = submvx[k] + cen_mvx;
                            submvy[k] = (submvy[k] & 0x10) ? (submvy[k] | (~0x1F)) : submvy[k];
                            mb[cnt].sub[k].mv_y = submvy[k] + cen_mvy;
                        }
                    }
                }
                parcel++;
                cnt++;
            }
        }
    }
    else if (PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        sH265ParcelMbMvInfo *parcel = (sH265ParcelMbMvInfo *)p_mb_mv;
        unsigned int pic_mb_width = pConfigPara->dstWidth >> 4;
        unsigned int pic_mb_height = pConfigPara->dstHeight >> 4;
        unsigned int pic_4ctu_width = ALIGN(pConfigPara->dstWidth, 128) >> 5;
        unsigned int pic_4ctu_width_16align = ALIGN(pic_4ctu_width, 16);
        unsigned int pic_ctu_height = ALIGN(pConfigPara->dstHeight, 32) >> 5;
        unsigned int total_ctu_num = pic_4ctu_width_16align * pic_ctu_height;
        
        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_4ctu_width_16align << 5) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_4ctu_width_16align << 5));
        }

        sH265GetCtuInfo *ctu = (sH265GetCtuInfo *)pGetMbInfo->ctu;
        unsigned int mb_order[4] = {0, 2, 1, 3};
        unsigned int cnt = 0, ctu_x = 0, ctu_y = 0, k = 0;
        for (ctu_y = 0; ctu_y < pic_ctu_height; ctu_y++)
        {
            for (ctu_x = 0; ctu_x < pic_4ctu_width_16align; ctu_x++)
            {
                for (k = 0; k < 4; k++)
                {
                    unsigned int mb_x = ctu_x * 2 + mb_order[k] % 2;
                    unsigned int mb_y = ctu_y * 2 + mb_order[k] / 2;
                    if (mb_x < pic_mb_width && mb_y < pic_mb_height)
                    {
                        ctu[cnt].mb[k].type = parcel->pred_type; // intra:1, inter:0
                        if (ctu[cnt].mb[k].type == 0)
                        {
                            ctu[cnt].mb[k].long_term_flag = parcel->long_term_flag;
                            unsigned int mvx = parcel->mv_x;
                            unsigned int mvy = parcel->mv_y;
                            ctu[cnt].mb[k].mv_x = (mvx & 0x200) ? (mvx | (~0x1FF)) : mvx;
                            ctu[cnt].mb[k].mv_y = (mvy & 0x100) ? (mvy | (~0x0FF)) : mvy;
                        }
                    }
                    parcel++;
                }
                cnt++;
            }
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void FillQpMapInfo(VencMBModeCtrl *pMBModeCtrl, SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == pMBModeCtrl) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", pMBModeCtrl, pConfigPara, pGetMbInfo);
        return;
    }

    if ((NULL == pMBModeCtrl->p_map_info) || (0 == pMBModeCtrl->mode_ctrl_en))
    {
        aloge("fatal error! invalid input param! p_map_info=%p, mode_ctrl_en=%d",
            pMBModeCtrl->p_map_info, pMBModeCtrl->mode_ctrl_en);
        return;
    }

    if (PT_H264 == pConfigPara->mVideoEncoderFmt)
    {
        sParcelMbQpMap *parcel = (sParcelMbQpMap *)pMBModeCtrl->p_map_info;
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num =  pic_mb_width * pic_mb_height;

        unsigned int osd_mb_width = ALIGN(pConfigPara->overlay_w, 16) >> 4;
        unsigned int osd_mb_height = ALIGN(pConfigPara->overlay_h, 16) >> 4;
        unsigned int osd_mb_x = ALIGN(pConfigPara->overlay_x, 16) >> 4;
        unsigned int osd_mb_y = ALIGN(pConfigPara->overlay_y, 16) >> 4;

        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if ((pic_mb_width << 4) != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - (pic_mb_width << 4));
        }

        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        unsigned int cnt = 0, x = 0, y = 0;
        for (y = 0; y < pic_mb_height; y++)
        {
            for (x = 0; x < pic_mb_width; x++)
            {
                if ((osd_mb_x < x) && (x < osd_mb_x + osd_mb_width) &&
                    (osd_mb_y < y) && (y < osd_mb_y + osd_mb_height))
                {
                    parcel[cnt].qp   = USER_SET_OSD_QP_DEFAULT;//mb[cnt].qp; // user set qp
                    parcel[cnt].skip = 0;  // user set skip
                    parcel[cnt].en   = 1;  // user set en
                }
                else
                {
                    parcel[cnt].qp   = USER_SET_MB_QP_DEFAULT;//mb[cnt].qp; // user set qp
                    parcel[cnt].skip = 1;  // user set skip
                    parcel[cnt].en   = 1;  // user set en
                }
                cnt++;
#ifdef DEBUG_QP
                if (0 == (cnt % 100))
                    alogd("SET mb[%d] QP=%d", cnt, parcel[cnt].qp);
#endif
            }
        }
    }
    else if (PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        sParcelCtuQpMap *parcel = (sParcelCtuQpMap *)pMBModeCtrl->p_map_info;
        unsigned int pic_ctu_width =  ALIGN(pConfigPara->dstWidth, 32) >> 5;
        unsigned int pic_4ctu_width = ALIGN(pConfigPara->dstWidth, 128) >> 5;
        unsigned int pic_ctu_height = ALIGN(pConfigPara->dstHeight, 32) >> 5;
        unsigned int total_ctu_num =  pic_4ctu_width * pic_ctu_height;

        unsigned int osd_ctu_width = ALIGN(pConfigPara->overlay_w, 32) >> 5;
        unsigned int osd_ctu_height = ALIGN(pConfigPara->overlay_h, 32) >> 5;
        unsigned int osd_ctu_x = ALIGN(pConfigPara->overlay_x, 32) >> 5;
        unsigned int osd_ctu_y = ALIGN(pConfigPara->overlay_y, 32) >> 5;
        
        // Note that according to this alignment width calculation,
        // there may be some invalid blocks on the right side of the image.
        if (pic_4ctu_width != pConfigPara->dstWidth)
        {
            alogw("There are [%d] invalid blocks on the right side of the image.",
                pConfigPara->dstWidth - pic_4ctu_width);
        }

        sH265GetCtuInfo *ctu = (sH265GetCtuInfo *)pGetMbInfo->ctu;
        unsigned int cnt = 0, x = 0, y = 0, k = 0;
        for (y = 0; y < pic_ctu_height; y++)
        {
            for (x = 0; x < pic_ctu_width; x++)
            {
                for (k = 0; k < 4; k++)
                {
                    if (x * 2 + k % 2 < pic_ctu_width)
                    {
                        if ((osd_ctu_y < y) && (y < osd_ctu_y + osd_ctu_height) &&
                            (osd_ctu_x < x) && (x < osd_ctu_x + osd_ctu_width))
                        {
                            parcel[cnt].mb[k].qp   = USER_SET_OSD_QP_DEFAULT;//ctu[cnt].mb[k].qp; // user set qp
                            parcel[cnt].mb[k].skip = 0;  // user set skip
                            parcel[cnt].mb[k].en   = 1;  // user set en
                        }
                        else
                        {
                            parcel[cnt].mb[k].qp   = USER_SET_MB_QP_DEFAULT;//ctu[cnt].mb[k].qp; // user set qp
                            parcel[cnt].mb[k].skip = 1;  // user set skip
                            parcel[cnt].mb[k].en   = 1;  // user set en
                        }
#ifdef DEBUG_QP
                        if (0 == (cnt % 50))
                            alogd("SET ctu[%d].mb[%d] QP=%d", cnt, k, parcel[cnt].mb[k].qp);
#endif
                    }
                }
                cnt++;
            }
        }
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}

static void QpMapInfoUserProcessFunc(SampleVencQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p", pConfigPara, pGetMbInfo);
        return;
    }

    if (PT_H264 == pConfigPara->mVideoEncoderFmt)
    {
        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        // The user can modify QP of a macro block. At the same time, user can set whether to
        // skip this macro block, or whether to enable this macro block.
        // The specific modification strategy needs to be judged by the user.
    }
    else if (PT_H265 == pConfigPara->mVideoEncoderFmt)
    {
        sH265GetCtuInfo *ctu = (sH265GetCtuInfo *)pGetMbInfo->ctu;
        // The user can modify QP of a macro block of a ctu. At the same time, user can set whether to
        // skip this macro block, or whether to enable this macro block.
        // The specific modification strategy needs to be judged by the user.
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pConfigPara->mVideoEncoderFmt);
        return;
    }
}
#endif /* V853 */

#endif /* TEST_QPMAP */

static void GenerateARGB1555(void* pBuf, int nSize)
{
    unsigned short nA0Color = 0x7C00;
    unsigned short nA1Color = 0xFC00;
    unsigned short *pShort = (unsigned short*)pBuf;

    int i = 0;
    for(i=0; i< nSize/4; i++)
    {
        *pShort = nA0Color;
        pShort++;
    }
    for(i=0; i< nSize/4; i++)
    {
        *pShort = nA1Color;
        pShort++;
    }
#if 0
    FILE* pOsdFile = fopen("/tmp/osd.file", "rb");
    int nRdBytes = fread(pBuf, 1, nSize, pOsdFile);
    if(nRdBytes != nSize)
    {
        aloge("fatal error! read argb1555 osd file wrong!%d!=%d", nRdBytes, nSize);
    }
    fclose(pOsdFile);
#endif
}

static void GenerateARGB8888(void* pBuf, int nSize)
{
    unsigned int nA0Color = 0x80FF0000;
    unsigned int nA1Color = 0xFFFF0000;
    unsigned int *pInt = (unsigned int*)pBuf;
    int i = 0;
    for(i=0; i< nSize/8; i++)
    {
        *pInt = nA0Color;
        pInt++;
    }
    for(i=0; i< nSize/8; i++)
    {
        *pInt = nA1Color;
        pInt++;
    }
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleVencQPMAPContext *pContext = (SampleVencQPMAPContext *)cookie;
    
    switch (event)
    {
    case MPP_EVENT_RELEASE_VIDEO_BUFFER:
    {
        VIDEO_FRAME_INFO_S *pFrame = (VIDEO_FRAME_INFO_S *)pEventData;
        if (pFrame != NULL)
        {
            pthread_mutex_lock(&pContext->mInBuf_Q.mUseListLock);
            if (!list_empty(&pContext->mInBuf_Q.mUseList))
            {
                IN_FRAME_NODE_S *pEntry, *pTmp;
                list_for_each_entry_safe(pEntry, pTmp, &pContext->mInBuf_Q.mUseList, mList)
                {
                    if (pEntry->mFrame.mId == pFrame->mId)
                    {
                        pthread_mutex_lock(&pContext->mInBuf_Q.mIdleListLock);
                        list_move_tail(&pEntry->mList, &pContext->mInBuf_Q.mIdleList);
                        pthread_mutex_unlock(&pContext->mInBuf_Q.mIdleListLock);
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&pContext->mInBuf_Q.mUseListLock);
        }
        break;
    }
    case MPP_EVENT_VENC_QPMAP_UPDATE_MB_MODE_INFO:
    {
        alogv("MPP_EVENT_VENC_QPMAP_UPDATE_MB_MODE_INFO");
        // The encoding has just been completed, but the encoding of the next frame has not yet started.
        // Before encoding, set the encoding method of the frame to be encoded.
        // The user can modify the encoding parameters before setting.
#ifdef TEST_QPMAP
        QpMapInfoUserProcessFunc(&pContext->mConfigPara, &pContext->mGetMbInfo);

        FillQpMapInfo(&pContext->mMBModeCtrl, &pContext->mConfigPara, &pContext->mGetMbInfo);

        VencMBModeCtrl *pMBModeCtrl = (VencMBModeCtrl *)pEventData;
        memcpy(pMBModeCtrl, &pContext->mMBModeCtrl, sizeof(VencMBModeCtrl));

        if ((0 == pMBModeCtrl->mode_ctrl_en) || (NULL == pMBModeCtrl->p_map_info))
        {
            alogw("Venc ModeCtrl: mode_ctrl_en=%d, p_map_info=%p",
                pMBModeCtrl->mode_ctrl_en, pMBModeCtrl->p_map_info);
        }
#endif
        break;
    }
    case MPP_EVENT_VENC_QPMAP_UPDATE_MB_STAT_INFO:
    {
        alogv("MPP_EVENT_VENC_QPMAP_UPDATE_MB_STAT_INFO");
        // The encoding has just been completed, but the encoding of the next frame has not yet started.
        // and the QpMap information of the frame that has just been encoded is obtained.
#ifdef TEST_QPMAP
        VencMBSumInfo *pMbSumInfo = (VencMBSumInfo *)pEventData;
        memcpy(&pContext->mMbSumInfo, pMbSumInfo, sizeof(VencMBSumInfo));

        if ((0 == pMbSumInfo->avg_qp) || (NULL == pMbSumInfo->p_mb_mad_qp_sse) || (NULL == pMbSumInfo->p_mb_bin_img) || (NULL == pMbSumInfo->p_mb_mv))
        {
            alogw("Venc MBSumInfo: avg_mad=%d, avg_sse=%d, avg_qp=%d, avg_psnr=%lf, "
                "p_mb_mad_qp_sse=%p, p_mb_bin_img=%p, p_mb_mv=%p",
                pMbSumInfo->avg_mad, pMbSumInfo->avg_sse, pMbSumInfo->avg_qp, pMbSumInfo->avg_psnr,
                pMbSumInfo->p_mb_mad_qp_sse, pMbSumInfo->p_mb_bin_img, pMbSumInfo->p_mb_mv);
        }

        ParseMbMadQpSSE(pContext->mMbSumInfo.p_mb_mad_qp_sse, &pContext->mConfigPara, &pContext->mGetMbInfo);
        ParseMbBinImg(pContext->mMbSumInfo.p_mb_bin_img, &pContext->mConfigPara, &pContext->mGetMbInfo);
        ParseMbMv(pContext->mMbSumInfo.p_mb_mv, &pContext->mConfigPara, &pContext->mGetMbInfo);
#endif
        break;
    }
    default:
        break;
    }

    return SUCCESS;
}

static ERRORTYPE configVencChnAttr(SampleVencQPMAPContext *pContext)
{
    memset(&pContext->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));

    pContext->mVencChnAttr.VeAttr.Type = pContext->mConfigPara.mVideoEncoderFmt;
    pContext->mVencChnAttr.VeAttr.MaxKeyInterval = 50;

    pContext->mVencChnAttr.VeAttr.SrcPicWidth  = pContext->mConfigPara.srcWidth;
    pContext->mVencChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.srcHeight;

    if (pContext->mConfigPara.srcPixFmt == MM_PIXEL_FORMAT_YUV_AW_AFBC)
    {
        alogd("aw_afbc");
        pContext->mVencChnAttr.VeAttr.PixelFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
        pContext->mConfigPara.dstWidth = pContext->mConfigPara.srcWidth;  //cannot compress_encoder
        pContext->mConfigPara.dstHeight = pContext->mConfigPara.srcHeight; //cannot compress_encoder
    }
    else
    {
        pContext->mVencChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.srcPixFmt;
    }
    pContext->mVencChnAttr.VeAttr.mColorSpace = pContext->mConfigPara.mColorSpace;
    pContext->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;

    if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH264e.Profile = 2; //0:base 1:main 2:high
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;            
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH264Vbr.mMinQp = pContext->mConfigPara.nMinqp; //10;
            pContext->mVencRcParam.ParamH264Vbr.mMaxQp = pContext->mConfigPara.nMaxqp; //40;
            pContext->mVencRcParam.ParamH264Vbr.mMovingTh = 20;
            pContext->mVencRcParam.ParamH264Vbr.mQuality = 5;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = pContext->mConfigPara.nMinqp;//35;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = pContext->mConfigPara.nMaxqp;//35;
            break;
        case 3:
            alogd("H264 set RcMode=VENC_RC_MODE_H264QPMAP");
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264QPMAP;
            pContext->mVencChnAttr.RcAttr.mAttrH264QpMap.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264QpMap.fr32DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264QpMap.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH264QpMap.mMinQp = pContext->mConfigPara.nMinqp; //10;
            pContext->mVencRcParam.ParamH264QpMap.mMaxQp = pContext->mConfigPara.nMaxqp; //40;
            pContext->mVencRcParam.ParamH264QpMap.mMinPqp = pContext->mConfigPara.nMinPqp;
            pContext->mVencRcParam.ParamH264QpMap.mMaxPqp = pContext->mConfigPara.nMaxPqp;
            pContext->mVencRcParam.ParamH264QpMap.mQpInit = pContext->mConfigPara.nQpInit;
            pContext->mVencRcParam.ParamH264QpMap.mbEnMbQpLimit = pContext->mConfigPara.bEnMbQpLimit;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.fr32DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH264Cbr.mMinQp = pContext->mConfigPara.nMinqp;
            pContext->mVencRcParam.ParamH264Cbr.mMaxQp = pContext->mConfigPara.nMaxqp;
            break;
        }
    }
    else if (PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mProfile = 0; //0:main 1:main10 2:sti11
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH265Vbr.mMinQp = pContext->mConfigPara.nMinqp; //10;
            pContext->mVencRcParam.ParamH265Vbr.mMaxQp = pContext->mConfigPara.nMaxqp; //40;
            pContext->mVencRcParam.ParamH265Vbr.mMovingTh = 20;
            pContext->mVencRcParam.ParamH265Vbr.mQuality = 5;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = pContext->mConfigPara.nMinqp; //35;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = pContext->mConfigPara.nMaxqp; //35;
            break;
        case 3:
            alogd("H265 set RcMode=VENC_RC_MODE_H265QPMAP");
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265QPMAP;
            pContext->mVencChnAttr.RcAttr.mAttrH265QpMap.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265QpMap.fr32DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265QpMap.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH265QpMap.mMinQp = pContext->mConfigPara.nMinqp; //10;
            pContext->mVencRcParam.ParamH265QpMap.mMaxQp = pContext->mConfigPara.nMaxqp; //40;
            pContext->mVencRcParam.ParamH265QpMap.mMinPqp = pContext->mConfigPara.nMinPqp;
            pContext->mVencRcParam.ParamH265QpMap.mMaxPqp = pContext->mConfigPara.nMaxPqp;
            pContext->mVencRcParam.ParamH265QpMap.mQpInit = pContext->mConfigPara.nQpInit;
            pContext->mVencRcParam.ParamH265QpMap.mbEnMbQpLimit = pContext->mConfigPara.bEnMbQpLimit;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.fr32DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH265Cbr.mMinQp = pContext->mConfigPara.nMinqp;
            pContext->mVencRcParam.ParamH265Cbr.mMaxQp = pContext->mConfigPara.nMaxqp;
            break;
        }
    }
    else if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
    }

    return SUCCESS;
}

static ERRORTYPE createVencChn(SampleVencQPMAPContext *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pContext);
    pContext->mVeChn = 0;
    while(pContext->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pContext->mVeChn, &pContext->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pContext->mVeChn);
            break;
        }
        else if(ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pContext->mVeChn);
            pContext->mVeChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", pContext->mVeChn, ret);
            pContext->mVeChn++;
        }
    }

    if (!nSuccessFlag)
    {
        pContext->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }
    else
    {
        ret = AW_MPI_VENC_SetRcParam(pContext->mVeChn, &pContext->mVencRcParam);
        if (ret != SUCCESS)
        {
            aloge("fatal error! venc chn[%d] set rc param fail[0x%x]!", pContext->mVeChn, ret);
        }

        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = stFrameRate.DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
        AW_MPI_VENC_SetFrameRate(pContext->mVeChn, &stFrameRate);

        if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
        {
            VENC_PARAM_JPEG_S stJpegParam;
            memset(&stJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
            stJpegParam.Qfactor = pContext->mConfigPara.nMinqp;
            AW_MPI_VENC_SetJpegParam(pContext->mVeChn, &stJpegParam);
        }

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mVeChn, &cbInfo);
        return SUCCESS;
    }
}

static int vencBufMgrCreate(int frmNum, int width, int height, INPUT_BUF_Q *pBufList, BOOL isAwAfbc)
{
    unsigned int size = 0;
    unsigned int afbc_header = 0;

    size = ALIGN(width, 16) * ALIGN(height, 16);

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
    SampleVencQPMAPContext *pContext = (SampleVencQPMAPContext*)pThreadData;
    uint64_t curPts = 0;
    IN_FRAME_NODE_S *pFrame;

    fseek(pContext->pFpInput, 0, SEEK_SET);

    while (!pContext->mOverFlag)
    {
        pthread_mutex_lock(&pContext->mInBuf_Q.mReadyListLock);
        while (1)
        {
            if (list_empty(&pContext->mInBuf_Q.mReadyList))
            {
                //alogd("ready list empty");
                break;
            }

            pFrame = list_first_entry(&pContext->mInBuf_Q.mReadyList, IN_FRAME_NODE_S, mList);
            pthread_mutex_lock(&pContext->mInBuf_Q.mUseListLock);
            list_move_tail(&pFrame->mList, &pContext->mInBuf_Q.mUseList);
            pthread_mutex_unlock(&pContext->mInBuf_Q.mUseListLock);
            ret = AW_MPI_VENC_SendFrame(pContext->mVeChn, &pFrame->mFrame, 0);
            if (ret == SUCCESS)
            {
                //alogd("send frame[%d] to enc", pFrame->mFrame.mId);
            }
            else
            {
                alogd("send frame[%d] fail", pFrame->mFrame.mId);
                pthread_mutex_lock(&pContext->mInBuf_Q.mUseListLock);
                list_move(&pFrame->mList, &pContext->mInBuf_Q.mReadyList);
                pthread_mutex_unlock(&pContext->mInBuf_Q.mUseListLock);
                break;
            }
        }
        pthread_mutex_unlock(&pContext->mInBuf_Q.mReadyListLock);

        if (feof(pContext->pFpInput))
        {
            alogd("read to end");
            cdx_sem_up(&pContext->mSemExit);
            break;
        }

        pthread_mutex_lock(&pContext->mInBuf_Q.mIdleListLock);
        if (!list_empty(&pContext->mInBuf_Q.mIdleList))
        {
            IN_FRAME_NODE_S *pFrame = list_first_entry(&pContext->mInBuf_Q.mIdleList, IN_FRAME_NODE_S, mList);
            if (pFrame != NULL)
            {
                pFrame->mFrame.VFrame.mWidth  = pContext->mConfigPara.srcWidth;
                pFrame->mFrame.VFrame.mHeight = pContext->mConfigPara.srcHeight;
                pFrame->mFrame.VFrame.mOffsetLeft = 0;
                pFrame->mFrame.VFrame.mOffsetTop  = 0;
                pFrame->mFrame.VFrame.mOffsetRight = pFrame->mFrame.VFrame.mOffsetLeft + pFrame->mFrame.VFrame.mWidth;
                pFrame->mFrame.VFrame.mOffsetBottom = pFrame->mFrame.VFrame.mOffsetTop + pFrame->mFrame.VFrame.mHeight;
                pFrame->mFrame.VFrame.mField  = VIDEO_FIELD_FRAME;
                pFrame->mFrame.VFrame.mVideoFormat	= VIDEO_FORMAT_LINEAR;
                pFrame->mFrame.VFrame.mCompressMode = COMPRESS_MODE_NONE;
                pFrame->mFrame.VFrame.mpts = curPts;
                //alogd("frame pts=%llu", pFrame->mFrame.VFrame.mpts);
                curPts += (1*1000*1000) / pContext->mConfigPara.mVideoFrameRate;

                if (MM_PIXEL_FORMAT_YUV_AW_AFBC != pContext->mConfigPara.srcPixFmt)
                {
                    //pFrame->mFrame.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
                    read_len = pFrame->mFrame.VFrame.mWidth * pFrame->mFrame.VFrame.mHeight;
                    size1 = fread(pFrame->mFrame.VFrame.mpVirAddr[0], 1, read_len, pContext->pFpInput);
                    size2 = fread(pFrame->mFrame.VFrame.mpVirAddr[1], 1, read_len /2, pContext->pFpInput);
                    if ((size1 != read_len) || (size2 != read_len/2))
                    {
                        alogw("warning: read to eof or somthing wrong, should stop. readlen=%d, size1=%d, size2=%d, w=%d,h=%d", read_len,\
                                        size1, size2, pFrame->mFrame.VFrame.mWidth, pFrame->mFrame.VFrame.mHeight);
                        pthread_mutex_unlock(&pContext->mInBuf_Q.mIdleListLock);
                        cdx_sem_up(&pContext->mSemExit);
                        break;
                    }
                    //Yu12ToNv21(pFrame->mFrame.VFrame.mWidth, pFrame->mFrame.VFrame.mHeight, pFrame->mFrame.VFrame.mpVirAddr[1], pContext->tmpBuf);
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
                    size1 = fread(pFrame->mFrame.VFrame.mpVirAddr[0], 1, read_len, pContext->pFpInput);
                    if (size1 != read_len)
                    {
                        alogw("warning: awafbc eof or somthing wrong, should stop. readlen=%d, size=%d, w=%d,h=%d", read_len,\
                                        size1, pFrame->mFrame.VFrame.mWidth, pFrame->mFrame.VFrame.mHeight);
                        pthread_mutex_unlock(&pContext->mInBuf_Q.mIdleListLock);
                        cdx_sem_up(&pContext->mSemExit);
                        break;
                    }
                    venc_flushCache((void *)pFrame->mFrame.VFrame.mpVirAddr[0], read_len);
                }

                pthread_mutex_lock(&pContext->mInBuf_Q.mReadyListLock);
                list_move_tail(&pFrame->mList, &pContext->mInBuf_Q.mReadyList);
                pthread_mutex_unlock(&pContext->mInBuf_Q.mReadyListLock);
                //alogd("get frame[%d] to ready list", pFrame->mFrame.mId);
            }
        }
        else
        {
            //alogd("idle list empty!!");
        }
        pthread_mutex_unlock(&pContext->mInBuf_Q.mIdleListLock);
    }

    alogd("read thread exit!");
    return NULL;
}

static void writeSpsPpsHead(SampleVencQPMAPContext *pContext)
{
    int result = 0;
    alogd("write spspps head");
    if (pContext->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        VencHeaderData pH264SpsPpsInfo={0};
        result = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVeChn, &pH264SpsPpsInfo);
        if (SUCCESS == result)
        {
            if (pH264SpsPpsInfo.nLength)
            {
                fwrite(pH264SpsPpsInfo.pBuffer, 1, pH264SpsPpsInfo.nLength, pContext->pFpOutput);
            }
        }
        else
        {
            alogd("AW_MPI_VENC_GetH264SpsPpsInfo failed!\n");
        }
    }
    else if (pContext->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        result = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVeChn, &H265SpsPpsInfo);
        if (SUCCESS == result)
        {
            if (H265SpsPpsInfo.nLength)
            {
                fwrite(H265SpsPpsInfo.pBuffer, 1, H265SpsPpsInfo.nLength, pContext->pFpOutput);
            }
        }
        else
        {
            alogd("AW_MPI_VENC_GetH265SpsPpsInfo failed!\n");
        }
    }
}

static void *saveFromEncThread(void *pThreadData)
{
    int ret;
    VENC_PACK_S mpPack;
    VENC_STREAM_S vencFrame;
    SampleVencQPMAPContext *pContext = (SampleVencQPMAPContext*)pThreadData;
    unsigned int totalBS = 0;
    unsigned int encFramecnt = 0;

    fseek(pContext->pFpOutput, 0, SEEK_SET);
    writeSpsPpsHead(pContext);

    while (!pContext->mOverFlag)
    {
        memset(&vencFrame, 0, sizeof(VENC_STREAM_S));
        vencFrame.mpPack = &mpPack;
        vencFrame.mPackCount = 1;

        ret = AW_MPI_VENC_GetStream(pContext->mVeChn, &vencFrame, 4000);
        if (SUCCESS == ret)
        {
            if (vencFrame.mpPack->mLen0)
            {
                fwrite(vencFrame.mpPack->mpAddr0, 1, vencFrame.mpPack->mLen0, pContext->pFpOutput);
            }

            if (vencFrame.mpPack->mLen1)
            {
                fwrite(vencFrame.mpPack->mpAddr1, 1, vencFrame.mpPack->mLen1, pContext->pFpOutput);
            }
            //alogd("get pts=%llu, seq=%d", vencFrame.mpPack->mPTS, vencFrame.mSeq);
            AW_MPI_VENC_ReleaseStream(pContext->mVeChn, &vencFrame);

            encFramecnt++;
            if (encFramecnt >= pContext->mConfigPara.mVideoFrameRate)
            //if ((vencFrame.mpPack->mDataType.enH264EType == H264E_NALU_ISLICE) || (vencFrame.mpPack->mDataType.enH265EType == H265E_NALU_ISLICE))
            {
                alogd("total encoder size=[%d]", totalBS);
                encFramecnt = 0;
                totalBS = vencFrame.mpPack->mLen0 + vencFrame.mpPack->mLen1;
            }
            else
            {
                totalBS = totalBS + vencFrame.mpPack->mLen0 + vencFrame.mpPack->mLen1;
            }
        }
    }

    alogd("get thread exit!");
    return NULL;
}

static int prepare(SampleVencQPMAPContext* pContext)
{
    int ret = 0;
    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

    alogd("enter");

    if (venc_MemOpen() != 0)
    {
        aloge("fatal error! open mem fail!");
        goto err_out_0;
    }

    BOOL isAwAfbc = (MM_PIXEL_FORMAT_YUV_AW_AFBC == pContext->mConfigPara.srcPixFmt);
    ret = vencBufMgrCreate(IN_FRAME_BUF_NUM, pContext->mConfigPara.srcWidth, pContext->mConfigPara.srcHeight, &pContext->mInBuf_Q, isAwAfbc);
    if (ret != 0)
    {
        aloge("fatal error! create FrameBuf manager fail");
        goto err_out_1;
    }

    pContext->pFpInput = fopen(pContext->mConfigPara.inputFile, "r");
    if (pContext->pFpInput == NULL)
    {
        aloge("fatal error! cannot open yuv src in file %s, errno(%d)", pContext->mConfigPara.inputFile, errno);
        goto err_out_2;
    }

    pContext->pFpOutput = fopen(pContext->mConfigPara.outputFile, "wb");
    if (pContext->pFpOutput == NULL)
    {
        aloge("fatal error! cannot create out file %s, errno(%d)", pContext->mConfigPara.outputFile, errno);
        goto err_out_3;
    }

    int tmpBufSize = pContext->mConfigPara.srcWidth * pContext->mConfigPara.srcHeight / 2; //for yuv420p -> yuv420sp tmp_buf
    pContext->tmpBuf = (char *)malloc(tmpBufSize);
    if (pContext->tmpBuf == NULL)
    {
        aloge("fatal error! malloc pContext->tmpBuf fail! size=%d", tmpBufSize);
        goto err_out_4;
    }
    memset(pContext->tmpBuf, 0, tmpBufSize);

    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    if (createVencChn(pContext) != SUCCESS)
    {
        aloge("fatal error! create vec chn fail");
        goto err_out_5;
    }

#ifdef TEST_QPMAP
    MBParamInit(&pContext->mMBModeCtrl, &pContext->mGetMbInfo, &pContext->mConfigPara, &pContext->mMbNum);
#endif

    ret = pthread_create(&pContext->mReadFrmThdId, NULL, readInFrameThread, pContext);
    if (ret || !pContext->mReadFrmThdId)
    {
        aloge("fatal error! create saveFromEncThread fail");
        goto err_out_6;
    }

    ret = pthread_create(&pContext->mSaveEncThdId, NULL, saveFromEncThread, pContext);
    if (ret || !pContext->mSaveEncThdId)
    {
        aloge("fatal error! create saveFromEncThread fail");
        goto err_out_7;
    }

    alogd("success");
    return 0;

err_out_7:
    pthread_join(pContext->mReadFrmThdId, NULL);
err_out_6:
    if (pContext->mVeChn >= 0)
    {
        AW_MPI_VENC_ResetChn(pContext->mVeChn);
        AW_MPI_VENC_DestroyChn(pContext->mVeChn);
        pContext->mVeChn = MM_INVALID_CHN;
    }
#ifdef TEST_QPMAP
    MBParamDeinit(&pContext->mMBModeCtrl, &pContext->mGetMbInfo, &pContext->mConfigPara);
#endif
err_out_5:
    AW_MPI_SYS_Exit();
    if (pContext->tmpBuf != NULL)
    {
        free(pContext->tmpBuf);
        pContext->tmpBuf = NULL;
    }
err_out_4:
    if (pContext->pFpInput)
    {
        fclose(pContext->pFpInput);
        pContext->pFpInput = NULL;
    }
err_out_3:
    if (pContext->pFpOutput)
    {
        fclose(pContext->pFpOutput);
        pContext->pFpOutput = NULL;
    }
err_out_2:
    vencBufMgrDestroy(&pContext->mInBuf_Q);
err_out_1:
    venc_MemClose();
err_out_0:
    alogd("fail");
    return -1;
}

static int start(SampleVencQPMAPContext* pContext)
{
    int ret = 0;
    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }
    alogd("enter");

    ret = AW_MPI_VENC_StartRecvPic(pContext->mVeChn);
    if (0 != ret)
    {
        aloge("fatal error! AW_MPI_VENC StartRecvPic fail, ret=%d", ret);
    }
    else
    {
        alogd("AW_MPI_VENC StartRecvPic success");
    }

    pContext->mOverlayHandle = 0;
    if (pContext->mConfigPara.overlay_h != 0 && pContext->mConfigPara.overlay_w != 0)
    {
        alogd("ve test overlay");
        RGN_ATTR_S stRegion;
        memset(&stRegion, 0, sizeof(RGN_ATTR_S));
        stRegion.enType = OVERLAY_RGN;
        stRegion.unAttr.stOverlay.mPixelFmt = pContext->mConfigPara.mBitmapFormat;
        stRegion.unAttr.stOverlay.mSize.Width = pContext->mConfigPara.overlay_w;
        stRegion.unAttr.stOverlay.mSize.Height = pContext->mConfigPara.overlay_h;
        AW_MPI_RGN_Create(pContext->mOverlayHandle, &stRegion);

        BITMAP_S stBmp;
        memset(&stBmp, 0, sizeof(BITMAP_S));
        stBmp.mPixelFormat = stRegion.unAttr.stOverlay.mPixelFmt;
        stBmp.mWidth = stRegion.unAttr.stOverlay.mSize.Width;
        stBmp.mHeight = stRegion.unAttr.stOverlay.mSize.Height;
        int nSize = BITMAP_S_GetdataSize(&stBmp);
        stBmp.mpData = malloc(nSize);
        if (NULL == stBmp.mpData)
        {
            aloge("fatal error! malloc Bmp fail! size=%d", nSize);
            return -1;
        }

        if (MM_PIXEL_FORMAT_RGB_1555 == pContext->mConfigPara.mBitmapFormat)
        {
            GenerateARGB1555(stBmp.mpData, nSize);
        }
        else if (MM_PIXEL_FORMAT_RGB_8888 == pContext->mConfigPara.mBitmapFormat)
        {
            GenerateARGB8888(stBmp.mpData, nSize);
        }
        else
        {
            aloge("fatal error! unsupport bitmap format:%d", pContext->mConfigPara.mBitmapFormat);
        }
        AW_MPI_RGN_SetBitMap(pContext->mOverlayHandle, &stBmp);
        if (stBmp.mpData)
        {
            free(stBmp.mpData);
        }

        RGN_CHN_ATTR_S stRgnChnAttr;
        memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
        stRgnChnAttr.bShow = TRUE;
        stRgnChnAttr.enType = stRegion.enType;
        stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X = pContext->mConfigPara.overlay_x;
        stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = pContext->mConfigPara.overlay_y;
        stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = 0;
        stRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha = 0x40; // global alpha mode value for ARGB1555
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = 60;
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUMDIFF_THRESH;
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = FALSE;
        alogd("overlay attach to ve");
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        AW_MPI_RGN_AttachToChn(pContext->mOverlayHandle,  &VeChn, &stRgnChnAttr);
        alogd("overlay attach to ve done");

        if (pContext->mConfigPara.mVencRoiEnable)
        {
            //use roi to enhance region encode quality.
            VENC_ROI_CFG_S stVencRoiCfg;
            memset(&stVencRoiCfg, 0, sizeof(VENC_ROI_CFG_S));
            stVencRoiCfg.Index = 0;
            stVencRoiCfg.bEnable = TRUE;
            stVencRoiCfg.bAbsQp = TRUE;
            stVencRoiCfg.Qp = USER_SET_OSD_QP_DEFAULT;
            stVencRoiCfg.Rect.X = stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X;
            stVencRoiCfg.Rect.Y = stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y;
            stVencRoiCfg.Rect.Width = stRegion.unAttr.stOverlay.mSize.Width;
            stVencRoiCfg.Rect.Height = stRegion.unAttr.stOverlay.mSize.Height;
            alogd("set Roi, qp:%d, Rect:X[%d]Y[%d]W[%d]H[%d]", stVencRoiCfg.Qp,
                stVencRoiCfg.Rect.X, stVencRoiCfg.Rect.Y, stVencRoiCfg.Rect.Width,
                stVencRoiCfg.Rect.Height);
            AW_MPI_VENC_SetRoiCfg(pContext->mVeChn, &stVencRoiCfg);
        }
    }

    pContext->mCoverHandle = 10;
    if (pContext->mConfigPara.cover_h != 0 && pContext->mConfigPara.cover_w != 0)
    {
        alogd("ve test cover");
        RGN_ATTR_S stRegion;
        memset(&stRegion, 0, sizeof(RGN_ATTR_S));
        stRegion.enType = COVER_RGN;
        AW_MPI_RGN_Create(pContext->mCoverHandle, &stRegion);

        RGN_CHN_ATTR_S stRgnChnAttr;
        memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
        stRgnChnAttr.bShow = TRUE;
        stRgnChnAttr.enType = stRegion.enType;
        stRgnChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;
        stRgnChnAttr.unChnAttr.stCoverChn.stRect.X = pContext->mConfigPara.cover_x;
        stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y = pContext->mConfigPara.cover_y;
        stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width = pContext->mConfigPara.cover_w;
        stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height= pContext->mConfigPara.cover_h;
        stRgnChnAttr.unChnAttr.stCoverChn.mColor = 255;
        stRgnChnAttr.unChnAttr.stCoverChn.mLayer = 0;
        alogd("cover attach to ve");
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        AW_MPI_RGN_AttachToChn(pContext->mCoverHandle, &VeChn, &stRgnChnAttr);
        alogd("cover attach to ve done");
    }

    alogd("%s \n", ((0==ret) ? "success" : "fail"));
    return ret;
}

static int stop(SampleVencQPMAPContext* pContext)
{
    int ret = 0;

    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

    pContext->mOverFlag = TRUE;

    ret = AW_MPI_VENC_StopRecvPic(pContext->mVeChn);
    if (0 != ret)
    {
        aloge("fatal error! AW_MPI_VENC StopRecvPic fail, ret=%d", ret);
    }
    else
    {
        alogd("AW_MPI_VENC StopRecvPic success");
    }

    alogd("over, join thread");
    pthread_join(pContext->mSaveEncThdId, NULL);
    pthread_join(pContext->mReadFrmThdId, NULL);

    if (pContext->mConfigPara.overlay_h != 0 && pContext->mConfigPara.overlay_w != 0)
    {
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        AW_MPI_RGN_DetachFromChn(pContext->mOverlayHandle, &VeChn);
        AW_MPI_RGN_Destroy(pContext->mOverlayHandle);
    }
    if (pContext->mConfigPara.cover_h != 0 && pContext->mConfigPara.cover_w != 0)
    {
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        AW_MPI_RGN_DetachFromChn(pContext->mCoverHandle, &VeChn);
        AW_MPI_RGN_Destroy(pContext->mCoverHandle);
    }

    alogd("%s \n", ((0==ret) ? "success" : "fail"));
    return ret;
}

static int release(SampleVencQPMAPContext *pContext)
{
    int ret = 0;

    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

    if (pContext->mVeChn >= 0)
    {
        AW_MPI_VENC_ResetChn(pContext->mVeChn);
        AW_MPI_VENC_DestroyChn(pContext->mVeChn);
        pContext->mVeChn = MM_INVALID_CHN;
    }

#ifdef TEST_QPMAP
    MBParamDeinit(&pContext->mMBModeCtrl, &pContext->mGetMbInfo, &pContext->mConfigPara);
#endif

    AW_MPI_SYS_Exit();
    if (pContext->tmpBuf != NULL)
    {
        free(pContext->tmpBuf);
        pContext->tmpBuf = NULL;
    }

    if (pContext->pFpInput)
    {
        fclose(pContext->pFpInput);
        pContext->pFpInput = NULL;
    }

    if (pContext->pFpOutput)
    {
        fclose(pContext->pFpOutput);
        pContext->pFpOutput = NULL;
    }

    ret += vencBufMgrDestroy(&pContext->mInBuf_Q);
    ret += venc_MemClose();
 
    alogd("%s \n", ((0==ret) ? "success" : "fail"));
    return ret;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");

    if(NULL != gpVencQPMAPContext)
    {
        cdx_sem_up(&gpVencQPMAPContext->mSemExit);
    }
}

int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
    int result = 0;
    int ret = 0;

    alogd("sample_QPMAP running!");

    /* malloc Context */
    SampleVencQPMAPContext *pContext = (SampleVencQPMAPContext *)malloc(sizeof(SampleVencQPMAPContext));
    if (NULL == pContext)
    {
        aloge("malloc SampleVencQPMAPContext fail, size=%d", sizeof(SampleVencQPMAPContext));
        return -1;
    }
    memset(pContext, 0, sizeof(SampleVencQPMAPContext));
    gpVencQPMAPContext = pContext; // for handle_exit()

    /* init sem */
    cdx_sem_init(&pContext->mSemExit, 0);

    /* parse command line param */
    char *pConfigFilePath = NULL;
    if (parseCmdLine(pContext, argc, argv) != SUCCESS)
    {
        aloge("fatal error! parse cmd line fail");
        result = -1;
        goto err_out_0;
    }
    pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;

    /* parse config file */
    if (loadConfigPara(pContext, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto err_out_0;
    }

    /* register process function for SIGINT, to exit program */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    if (SUCCESS != prepare(pContext))
    {
        aloge("prepare failed");
        goto err_out_1;
    }

    if (SUCCESS != start(pContext))
    {
        aloge("start fail");
        result = -1;
        goto err_out_2;
    }

    /* wait for test done */
    if (pContext->mConfigPara.mTestDuration > 0)
    {
        alogd("The test time is %d s, continues ...", pContext->mConfigPara.mTestDuration);
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
        alogd("The test time is up, end the test.");
    }
    else
    {
        alogd("No test time is specified, you need to pass 'ctrl+c' to exit the test.");
        cdx_sem_down(&pContext->mSemExit);
    }

err_out_2:
    if (SUCCESS != stop(pContext))
    {
        aloge("stop fail");
        result = -1;
    }

err_out_1:
    release(pContext);

err_out_0:
    if (pContext)
    {
        cdx_sem_deinit(&pContext->mSemExit);
        free(pContext);
        pContext = NULL;
    }

    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
