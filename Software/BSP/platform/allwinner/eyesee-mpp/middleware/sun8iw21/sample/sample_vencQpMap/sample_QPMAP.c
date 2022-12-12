//#define LOG_NDEBUG 0
#define LOG_TAG "sample_QPMAP"
#include <utils/plat_log.h>

#include <endian.h>
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

#include <sys/mman.h>
#include <sys/ioctl.h>

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"

#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <confparser.h>

#include "mpi_isp.h"

#include "sample_QPMAP_config.h"
#include "sample_QPMAP.h"

/**
 *  This macro contains operations related to QPMAP.
 */
#define TEST_QPMAP

// #define DEBUG_QP

static SampleQPMAPContext *gpQPMAPContext = NULL;

static int parseCmdLine(SampleQPMAPContext *pContext, int argc, char** argv)
{
    int ret = -1;

    alogd("argc=%d", argc);

    if (argc != 3)
    {
        printf("CmdLine param:\n"
                "\t-path ./sample_QPMAP.conf\n");
        return -1;
    }

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

                if (pContext)
                {
                    strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_SIZE-1);
                    pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
                }
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path ./sample_QPMAP.conf\n");
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
    VENC_PIXEL_FMT PicFormat = VENC_PIXEL_YVU420SP;
    char *pStrPixelFormat = (char*)GetConfParaString(pConfParser, key, NULL);

    if (!strcmp(pStrPixelFormat, "yu12"))
    {
        PicFormat = VENC_PIXEL_YUV420P;
    }
    else if ((!strcmp(pStrPixelFormat, "nv21")) || (!strcmp(pStrPixelFormat, "nv21s")))
    {
        PicFormat = VENC_PIXEL_YVU420SP;
    }
    else if (!strcmp(pStrPixelFormat, "nv12"))
    {
        PicFormat = VENC_PIXEL_YUV420SP;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        PicFormat = VENC_PIXEL_YVU420SP;
    }

    return PicFormat;
}

static VENC_CODEC_TYPE getEncoderTypeFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    VENC_CODEC_TYPE EncType = VENC_CODEC_H264;
    char *ptr = (char *)GetConfParaString(pConfParser, key, NULL);
    if(!strcmp(ptr, "H.264"))
    {
        alogd("H.264");
        EncType = VENC_CODEC_H264;
    }
    else if(!strcmp(ptr, "H.265"))
    {
        alogd("H.265");
        EncType = VENC_CODEC_H265;
    }
    else if (!strcmp(ptr, "MJPEG"))
    {
        alogd("MJPEG");
        EncType = VENC_CODEC_JPEG;
    }
    else
    {
        aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
        EncType = VENC_CODEC_H264;
    }

    return EncType;
}

static ERRORTYPE loadConfigPara(SampleQPMAPContext *pContext, const char *conf_path)
{
    ERRORTYPE ret = SUCCESS;
    CONFPARSER_S stConfParser;
    char *ptr = NULL;
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

    pContext->mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_QPMAP_TEST_DURATION, 0);
    ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_QPMAP_SRC_FILE_STR, NULL);
    strncpy(pContext->mConfigPara.inputFile, ptr, MAX_FILE_PATH_SIZE);
    pContext->mConfigPara.srcWidth  = GetConfParaInt(&stConfParser, SAMPLE_QPMAP_SRC_WIDTH, 0);
    pContext->mConfigPara.srcHeight = GetConfParaInt(&stConfParser, SAMPLE_QPMAP_SRC_HEIGHT, 0);
    pContext->mConfigPara.srcPixFmt = getPicFormatFromConfig(&stConfParser, SAMPLE_QPMAP_SRC_PIXFMT);

    ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_QPMAP_DST_FILE_STR, NULL);
    strncpy(pContext->mConfigPara.outputFile, ptr, MAX_FILE_PATH_SIZE);
    pContext->mConfigPara.dstWidth  = GetConfParaInt(&stConfParser, SAMPLE_QPMAP_DST_WIDTH, 0);
    pContext->mConfigPara.dstHeight = GetConfParaInt(&stConfParser, SAMPLE_QPMAP_DST_HEIGHT, 0);
    pContext->mConfigPara.dstPixFmt = getPicFormatFromConfig(&stConfParser, SAMPLE_QPMAP_DST_PIXFMT);

    pContext->mConfigPara.mVideoEncoderFmt = getEncoderTypeFromConfig(&stConfParser, SAMPLE_QPMAP_ENCODER);
    pContext->mConfigPara.mEncUseProfile = GetConfParaInt(&stConfParser, SAMPLE_QPMAP_ENC_PROFILE, 0);
    pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&stConfParser, SAMPLE_QPMAP_FRAMERATE, 0);
    pContext->mConfigPara.mVideoBitRate = GetConfParaInt(&stConfParser, SAMPLE_QPMAP_BITRATE, 0);

    alogd("user configuration: testDuration=%ds, enc_fmt=%d, profile=%d, src_pix_fmt=%d, dst_pix_fmt=%d\n"
        "src=%s, dst=%s, src_size=%dx%d, dst_size=%dx%d, framerate=%d, bitrate=%d",
        pContext->mConfigPara.mTestDuration, pContext->mConfigPara.mVideoEncoderFmt,
        pContext->mConfigPara.mEncUseProfile, pContext->mConfigPara.srcPixFmt, pContext->mConfigPara.dstPixFmt,
        pContext->mConfigPara.inputFile, pContext->mConfigPara.outputFile, pContext->mConfigPara.srcWidth, pContext->mConfigPara.srcHeight,
        pContext->mConfigPara.dstWidth, pContext->mConfigPara.dstHeight,
        pContext->mConfigPara.mVideoFrameRate, pContext->mConfigPara.mVideoBitRate);

    // set default params
    pContext->mConfigPara.mVencChnID = 1;

#ifdef TEST_QPMAP
    pContext->mConfigPara.mRcMode = 3; // for H264/H265: 0->CBR;  1->VBR  2->FIXQP  3->QPMAP; for mjpeg  0->CBR  1->FIXQP.
    alogd("test QPMAP");
#else
    pContext->mConfigPara.mRcMode = 0;
    alogd("test CBR");
#endif

    alogd("defalut configuration: VencChnID=%d, rc_mode=%d", pContext->mConfigPara.mVencChnID, pContext->mConfigPara.mRcMode);

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

static void enqueue(InputBufferInfo** pp_head, InputBufferInfo* p)
{
    InputBufferInfo* cur;

    cur = *pp_head;

    if (cur == NULL)
    {
        *pp_head = p;
        p->next  = NULL;
        return;
    }
    else
    {
        while(cur->next != NULL)
            cur = cur->next;

        cur->next = p;
        p->next   = NULL;

        return;
    }
}

static InputBufferInfo* dequeue(InputBufferInfo** pp_head)
{
    InputBufferInfo* head;

    head = *pp_head;

    if (head == NULL)
    {
        return NULL;
    }
    else
    {
        *pp_head = head->next;
        head->next = NULL;
        return head;
    }
}

#ifdef TEST_QPMAP

static void MBParamInit(VencMBModeCtrl *pMBModeCtrl, VencMBInfo *pMBInfo, sGetMbInfo *pGetMbInfo, SampleQPMAPConfig *pConfigPara)
{
    unsigned int mb_num = 0;

    if ((NULL == pMBModeCtrl) || (NULL == pMBInfo) || (NULL == pGetMbInfo) || (NULL == pConfigPara))
    {
        aloge("fatal error! invalid input params! %p,%p,%p,%p", pMBModeCtrl, pMBInfo, pGetMbInfo, pConfigPara);
        return;
    }

    // calc mb num and malloc GetMbInfo buffer
    if (VENC_CODEC_H264 == pConfigPara->mVideoEncoderFmt)
    {
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width * pic_mb_height;
        pMBInfo->num_mb = total_mb_num;

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
    else if (VENC_CODEC_H265 == pConfigPara->mVideoEncoderFmt)
    {
        unsigned int pic_4ctu_width = ALIGN(pConfigPara->dstWidth, 128) >> 5;
        unsigned int pic_ctu_height = ALIGN(pConfigPara->dstHeight, 32) >> 5;
        unsigned int total_ctu_num = pic_4ctu_width * pic_ctu_height;
        pMBInfo->num_mb = total_ctu_num;

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

    mb_num = pMBInfo->num_mb;

    // malloc mb info p_para
    if (NULL == pMBInfo->p_para)
    {
        pMBInfo->p_para = (VencMBInfoPara *)malloc(sizeof(VencMBInfoPara) * mb_num);
        if (NULL == pMBInfo->p_para)
        {
            aloge("fatal error! malloc pMBInfo->p_para fail! size=%d", sizeof(VencMBInfoPara) * mb_num);
            return;
        }
    }
    else
    {
        alogw("init mb info repeated? pMBInfo->p_para is not NULL!");
    }
    memset(pMBInfo->p_para, 0, sizeof(VencMBInfoPara) * mb_num);
    alogd("mb_num=%d, pMBInfo->p_para=%p", mb_num, pMBInfo->p_para);

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
    pMBModeCtrl->mode_ctrl_en = 1;
    alogd("mb_num=%d, pMBModeCtrl->p_map_info=%p, pMBModeCtrl->mode_ctrl_en=%d", mb_num, pMBModeCtrl->p_map_info, pMBModeCtrl->mode_ctrl_en);
}

static void MBParamDeinit(VencMBModeCtrl *pMBModeCtrl, VencMBInfo *pMBInfo, sGetMbInfo *pGetMbInfo, SampleQPMAPConfig *pConfigPara)
{
    if ((NULL == pMBModeCtrl) || (NULL == pMBInfo) || (NULL == pGetMbInfo) || (NULL == pConfigPara))
    {
        aloge("fatal error! invalid input params! %p,%p,%p,%p", pMBModeCtrl, pMBInfo, pGetMbInfo, pConfigPara);
        return;
    }

    if (pMBModeCtrl->p_map_info)
    {
        free(pMBModeCtrl->p_map_info);
        pMBModeCtrl->p_map_info = NULL;
    }
    pMBModeCtrl->mode_ctrl_en = 0;

    if (pMBInfo->p_para)
    {
        free(pMBInfo->p_para);
        pMBInfo->p_para = NULL;
    }
    pMBInfo->num_mb = 0;

    if (VENC_CODEC_H264 == pConfigPara->mVideoEncoderFmt)
    {
        if (pGetMbInfo->mb)
        {
            free(pGetMbInfo->mb);
            pGetMbInfo->mb = NULL;
        }
    }
    else if (VENC_CODEC_H265 == pConfigPara->mVideoEncoderFmt)
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

static void SetMbInfo(VideoEncoder *pVideoEnc, VencMBInfo *pMBInfo)
{
    VencSetParameter(pVideoEnc, VENC_IndexParamMBInfoOutput, pMBInfo);
}

static void SetMbModeCtrl(VideoEncoder *pVideoEnc, VencMBModeCtrl *pMBModeCtrl)
{
    if (pMBModeCtrl->mode_ctrl_en == 1)
    {
        // Notice: Only for P-frame, I-frame is NOT support.
        // The encoding library only sets the QPMAP parameter for the P-frame,
        // if the current frame is an I-frame, the QPMAP setting is ignored.
        VencSetParameter(pVideoEnc, VENC_IndexParamMBModeCtrl, pMBModeCtrl);
    }
    else
    {
        aloge("fatal error! pMBModeCtrl->mode_ctrl_en is NOT enabled!");
    }
}

static void GetMbSumInfo(VideoEncoder *pVideoEnc, VencMBSumInfo *pMbSumInfo)
{
    VencGetParameter(pVideoEnc, VENC_IndexParamMBSumInfoOutput, pMbSumInfo);

    if ((0 == pMbSumInfo->avg_qp) || (NULL == pMbSumInfo->p_mb_mad_qp_sse) || (NULL == pMbSumInfo->p_mb_bin_img) || (NULL == pMbSumInfo->p_mb_mv))
    {
        alogw("Venc MBSumInfo: avg_mad=%d, avg_sse=%d, avg_qp=%d, avg_psnr=%lf, "
            "p_mb_mad_qp_sse=%p, p_mb_bin_img=%p, p_mb_mv=%p",
            pMbSumInfo->avg_mad, pMbSumInfo->avg_sse, pMbSumInfo->avg_qp, pMbSumInfo->avg_psnr,
            pMbSumInfo->p_mb_mad_qp_sse, pMbSumInfo->p_mb_bin_img, pMbSumInfo->p_mb_mv);
    }
}

static void ParseMbMadQpSSE(unsigned char *p_mb_mad_qp_sse, SampleQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == p_mb_mad_qp_sse) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", p_mb_mad_qp_sse, pConfigPara, pGetMbInfo);
        return;
    }

    unsigned char *mb_addr = p_mb_mad_qp_sse;

    if (VENC_CODEC_H264 == pConfigPara->mVideoEncoderFmt)
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
    else if (VENC_CODEC_H265 == pConfigPara->mVideoEncoderFmt)
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

static void ParseMbBinImg(unsigned char *p_mb_bin_img, SampleQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == p_mb_bin_img) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", p_mb_bin_img, pConfigPara, pGetMbInfo);
        return;
    }

    if (VENC_CODEC_H264 == pConfigPara->mVideoEncoderFmt)
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
    else if (VENC_CODEC_H265 == pConfigPara->mVideoEncoderFmt)
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

static void ParseMbMv(unsigned char *p_mb_mv, SampleQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == p_mb_mv) || (NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p,%p", p_mb_mv, pConfigPara, pGetMbInfo);
        return;
    }

    if (VENC_CODEC_H264 == pConfigPara->mVideoEncoderFmt)
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
    else if (VENC_CODEC_H265 == pConfigPara->mVideoEncoderFmt)
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

static void FillQpMapInfo(VencMBModeCtrl *pMBModeCtrl, SampleQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
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

    if (VENC_CODEC_H264 == pConfigPara->mVideoEncoderFmt)
    {
        sParcelMbQpMap *parcel = (sParcelMbQpMap *)pMBModeCtrl->p_map_info;
        unsigned int pic_mb_width = ALIGN(pConfigPara->dstWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pConfigPara->dstHeight, 16) >> 4;
        unsigned int total_mb_num =  pic_mb_width * pic_mb_height;

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
                parcel[cnt].qp   = mb[cnt].qp; // user set qp
                parcel[cnt].skip = 0;  // user set skip
                parcel[cnt].en   = 1;  // user set en
                cnt++;
#ifdef DEBUG_QP
                if (0 == (cnt % 100))
                    alogd("SET mb[%d] QP=%d", cnt, parcel[cnt].qp);
#endif
            }
        }
    }
    else if (VENC_CODEC_H265 == pConfigPara->mVideoEncoderFmt)
    {
        sParcelCtuQpMap *parcel = (sParcelCtuQpMap *)pMBModeCtrl->p_map_info;
        unsigned int pic_ctu_width =  ALIGN(pConfigPara->dstWidth, 32) >> 5;
        unsigned int pic_4ctu_width = ALIGN(pConfigPara->dstWidth, 128) >> 5;
        unsigned int pic_ctu_height = ALIGN(pConfigPara->dstHeight, 32) >> 5;
        unsigned int total_ctu_num =  pic_4ctu_width * pic_ctu_height;
        
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
                        parcel[cnt].mb[k].qp   = ctu[cnt].mb[k].qp; // user set qp
                        parcel[cnt].mb[k].skip = 0;  // user set skip
                        parcel[cnt].mb[k].en   = 1;  // user set en
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

static void QpMapInfoUserProcessFunc(SampleQPMAPConfig *pConfigPara, sGetMbInfo *pGetMbInfo)
{
    if ((NULL == pConfigPara) || (NULL == pGetMbInfo))
    {
        aloge("fatal error! invalid input params! %p,%p", pConfigPara, pGetMbInfo);
        return;
    }

    if (VENC_CODEC_H264 == pConfigPara->mVideoEncoderFmt)
    {
        sH264GetMbInfo *mb = (sH264GetMbInfo *)pGetMbInfo->mb;
        // The user can modify QP of a macro block. At the same time, user can set whether to
        // skip this macro block, or whether to enable this macro block.
        // The specific modification strategy needs to be judged by the user.
    }
    else if (VENC_CODEC_H265 == pConfigPara->mVideoEncoderFmt)
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
#endif /* TEST_QPMAP */

static int VencEventHandler(VideoEncoder* pEncoder, void* pAppData, VencEventType eEvent,
                             unsigned int nData1, unsigned int nData2, void* pEventData)
{
    SampleQPMAPContext* pContext = (SampleQPMAPContext*)pAppData;
    if (NULL == pContext)
    {
        aloge("fatal error! invalid input param!");
        return -1;
    }

    alogv("event = %d", eEvent);
    switch (eEvent)
    {
        case VencEvent_UpdateMbModeInfo:
        {
            alogv("VencEvent UpdateMbModeInfo");
            // The encoding has just been completed, but the encoding of the next frame has not yet started.
            // Before encoding, set the encoding method of the frame to be encoded.
            // The user can modify the encoding parameters before setting.
#ifdef TEST_QPMAP
            QpMapInfoUserProcessFunc(&pContext->mConfigPara, &pContext->mGetMbInfo);
            FillQpMapInfo(&pContext->mMBModeCtrl, &pContext->mConfigPara, &pContext->mGetMbInfo);
            SetMbModeCtrl(pContext->pVideoEnc, &pContext->mMBModeCtrl);
#endif
            break;
        }
        case VencEvent_UpdateMbStatInfo:
        {
            alogv("VencEvent UpdateMbStatInfo");
            // The encoding has just been completed, but the encoding of the next frame has not yet started.
            // and the QpMap information of the frame that has just been encoded is obtained.
#ifdef TEST_QPMAP
            GetMbSumInfo(pContext->pVideoEnc, &pContext->mMbSumInfo);
            ParseMbMadQpSSE(pContext->mMbSumInfo.p_mb_mad_qp_sse, &pContext->mConfigPara, &pContext->mGetMbInfo);
            ParseMbBinImg(pContext->mMbSumInfo.p_mb_bin_img, &pContext->mConfigPara, &pContext->mGetMbInfo);
            ParseMbMv(pContext->mMbSumInfo.p_mb_mv, &pContext->mConfigPara, &pContext->mGetMbInfo);
#endif
            break;
        }
        default:
            alogw("not support the event: %d", eEvent);
            break;
    }

    return 0;
}

static int VencInputBufferDone(VideoEncoder* pEncoder, void* pAppData,
                          VencCbInputBufferDoneInfo* pBufferDoneInfo)
{
    SampleQPMAPContext* pContext = (SampleQPMAPContext*)pAppData;
    if (NULL == pContext)
    {
        aloge("fatal error! invalid input param!");
        return -1;
    }

    InputBufferInfo *pInputBufInfo = dequeue(&pContext->mInputBufMgr.empty_quene);
    if (NULL == pInputBufInfo)
    {
        aloge("fatal error! get input empty buffer failed!");
        return -1;
    }
    memcpy(&pInputBufInfo->inputbuffer, pBufferDoneInfo->pInputBuffer, sizeof(VencInputBuffer));
    enqueue(&pContext->mInputBufMgr.valid_quene, pInputBufInfo);

    // for vbv buf full, the encoding library has been processed.
    alogv("VencInput BufferDone! result=%d", pBufferDoneInfo->nResult);
    if (VENC_RESULT_BITSTREAM_IS_FULL == pBufferDoneInfo->nResult)
    {
        alogw("vbv buf is full!");
    }
    else if (VENC_RESULT_OK != pBufferDoneInfo->nResult)
    {
        aloge("fatal error! venc result=%d is error!", pBufferDoneInfo->nResult);
    }
    else
    {
        alogv("venc result is OK");
    }

    cdx_sem_up(&pContext->mSemInputBufferDone);

    return 0;
}

static VencCbType gVencCompCbType = {
	.EventHandler = VencEventHandler,
	.InputBufferDone = VencInputBufferDone,
};

static inline unsigned int map_H264_UserSet2Profile(int val)
{
    unsigned int profile = (unsigned int)VENC_H264ProfileHigh;
    switch (val)
    {
    case 0:
        profile = (unsigned int)VENC_H264ProfileBaseline;
        break;

    case 1:
        profile = (unsigned int)VENC_H264ProfileMain;
        break;

    case 2:
        profile = (unsigned int)VENC_H264ProfileHigh;
        break;

    default:
        break;
    }

    return profile;
}

static inline unsigned int map_H265_UserSet2Profile(int val)
{
    unsigned int profile = VENC_H265ProfileMain;
    switch (val)
    {
    case 0:
        profile = (unsigned int)VENC_H265ProfileMain;
        break;

    case 1:
        profile = (unsigned int)VENC_H265ProfileMain10;
        break;

    case 2:
        profile = (unsigned int)VENC_H265ProfileMainStill;
        break;

    default:
        break;
    }
    return profile;
}

static int VideoEncInit(SampleQPMAPContext* pContext)
{
    int ret = 0;
    if (NULL == pContext)
    {
        aloge("fatal error! invalid input param!");
        return -1;
    }
    if (NULL == pContext->pVideoEnc)
    {
        aloge("fatal error! the video encoder do not creat!");
        return -1;
    }

    unsigned int nDstWidth = 0;
    unsigned int nDstHeight = 0;
    unsigned int nSrcStride = 0;

    if (VENC_CODEC_H264 == pContext->mConfigPara.mVideoEncoderFmt)
    {
        VencH264Param h264Param;
        memset(&h264Param, 0, sizeof(VencH264Param));
        h264Param.nCodingMode = VENC_FRAME_CODING;
        h264Param.bEntropyCodingCABAC = 1;
        h264Param.nBitrate = pContext->mConfigPara.mVideoBitRate;
        h264Param.nMaxKeyInterval = pContext->mConfigPara.mVideoFrameRate;

        h264Param.sProfileLevel.nProfile = map_H264_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        h264Param.sProfileLevel.nLevel = VENC_H264Level51;

        switch(pContext->mConfigPara.mRcMode)
        {
            case 0: // CBR
            {
                h264Param.sRcParam.eRcMode = AW_CBR;
                int minQp = pContext->mConfigPara.nMinqp;
                int maxQp = pContext->mConfigPara.nMaxqp;
                if (!(minQp>=1 && minQp<=51))
                {
                    alogw("h264CBR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                    pContext->mConfigPara.nMinqp = 1;
                }
                if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                {
                    alogw("h264CBR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                    pContext->mConfigPara.nMaxqp = 51;
                }
                h264Param.sQPRange.nMinqp = pContext->mConfigPara.nMinqp;
                h264Param.sQPRange.nMaxqp = pContext->mConfigPara.nMaxqp;
                h264Param.sQPRange.nMinPqp = pContext->mConfigPara.nMinPqp;
                h264Param.sQPRange.nMaxPqp = pContext->mConfigPara.nMaxPqp;
                h264Param.sQPRange.nQpInit = pContext->mConfigPara.nQpInit;
                h264Param.sQPRange.bEnMbQpLimit = pContext->mConfigPara.bEnMbQpLimit;
                break;
            }
            case 1: // VBR
            {
                h264Param.sRcParam.eRcMode = AW_VBR;
                int minQp = pContext->mConfigPara.nMinqp;
                int maxQp = pContext->mConfigPara.nMaxqp;
                if (!(minQp>=1 && minQp<=51))
                {
                    alogw("h264VBR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                    pContext->mConfigPara.nMinqp = 1;
                }
                if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                {
                    alogw("h264VBR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                    pContext->mConfigPara.nMaxqp = 51;
                }
                h264Param.sQPRange.nMinqp = pContext->mConfigPara.nMinqp;
                h264Param.sQPRange.nMaxqp = pContext->mConfigPara.nMaxqp;
                h264Param.sQPRange.nMinPqp = pContext->mConfigPara.nMinPqp;
                h264Param.sQPRange.nMaxPqp = pContext->mConfigPara.nMaxPqp;
                h264Param.sQPRange.nQpInit = pContext->mConfigPara.nQpInit;
                h264Param.sQPRange.bEnMbQpLimit = pContext->mConfigPara.bEnMbQpLimit;
                h264Param.sRcParam.sVbrParam.uMaxBitRate = pContext->mConfigPara.mMaxBitRate;
                h264Param.sRcParam.sVbrParam.nMovingTh = pContext->mConfigPara.mMovingTh;
                h264Param.sRcParam.sVbrParam.nQuality = pContext->mConfigPara.mQuality;
                h264Param.sRcParam.sVbrParam.nIFrmBitsCoef = pContext->mConfigPara.mIFrmBitsCoef;
                h264Param.sRcParam.sVbrParam.nPFrmBitsCoef = pContext->mConfigPara.mPFrmBitsCoef;
                break;
            }
            case 2: // FIXQP
            {
                h264Param.sRcParam.eRcMode = AW_FIXQP;
                h264Param.sRcParam.sFixQp.bEnable = 1;
                int IQp = pContext->mConfigPara.mIQp;
                int PQp = pContext->mConfigPara.mPQp;
                if (!(IQp>=1 && IQp<=51))
                {
                    alogw("h264FixQp IQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 30!", IQp);
                    pContext->mConfigPara.mIQp = 30;
                }
                if (!(PQp>=1 && PQp<=51))
                {
                    alogw("h264FixQp PQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 30!", PQp);
                    pContext->mConfigPara.mPQp = 30;
                }
                h264Param.sRcParam.sFixQp.nIQp = pContext->mConfigPara.mIQp;
                h264Param.sRcParam.sFixQp.nPQp = pContext->mConfigPara.mPQp;
                break;
            }
            case 3: // QPMAP
            {
#ifdef TEST_QPMAP
                h264Param.sRcParam.eRcMode = AW_QPMAP;
                alogd("H264 set RcMode = AW_QPMAP");

                h264Param.sQPRange.nMinqp = pContext->mConfigPara.nMinqp;
                h264Param.sQPRange.nMaxqp = pContext->mConfigPara.nMaxqp;
                h264Param.sQPRange.nMinPqp = pContext->mConfigPara.nMinPqp;
                h264Param.sQPRange.nMaxPqp = pContext->mConfigPara.nMaxPqp;
                h264Param.sQPRange.nQpInit = pContext->mConfigPara.nQpInit;
                h264Param.sQPRange.bEnMbQpLimit = pContext->mConfigPara.bEnMbQpLimit;
#endif
                break;
            }
            default:
            {
                aloge("fatal error! wrong h264 rc mode[0x%x], check code!", pContext->mConfigPara.mRcMode);
                break;
            }
        }

        h264Param.nFramerate = pContext->mConfigPara.mVideoFrameRate;
        h264Param.nSrcFramerate = pContext->mConfigPara.mVideoFrameRate;
        // config the GopParam
        h264Param.sGopParam.bUseGopCtrlEn = 0;
        h264Param.sGopParam.eGopMode = AW_NORMALP;

        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamH264Param, &h264Param);

        int nIFilterEnable = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamIfilter, &nIFilterEnable);

        VencH264VideoSignal sVideoSignal;
        memset(&sVideoSignal, 0, sizeof(VencH264VideoSignal));
        sVideoSignal.video_format = DEFAULT;
        sVideoSignal.full_range_flag = 0;//checkColorSpaceFullRange((int)pVideoEncData->mEncChnAttr.VeAttr.mColorSpace);
        sVideoSignal.src_colour_primaries = VENC_YCC;//map_v4l2_colorspace_to_VENC_COLOR_SPACE(pVideoEncData->mEncChnAttr.VeAttr.mColorSpace);
        sVideoSignal.dst_colour_primaries = sVideoSignal.src_colour_primaries;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamH264VideoSignal, &sVideoSignal);

        //int nPskip = 0;
        //VencSetParameter(pContext->pVideoEnc, VENC_IndexParamSetPSkip, &nPskip);

        int nFastEnc = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamFastEnc, &nFastEnc);

        int sliceHeight = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamSliceHeight, &sliceHeight);
        unsigned char bPFrameIntraEn = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamPFrameIntraEn, (void*)&bPFrameIntraEn);

        int vbvSize = 8*1024*1024;
        int nThreshSize = vbvSize;
        alogd("set encode vbv size %d, frame length threshold %d", vbvSize, nThreshSize);
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamSetVbvSize, &vbvSize);
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamSetFrameLenThreshold, &nThreshSize);

        nDstWidth = pContext->mConfigPara.dstWidth;
        nDstHeight = pContext->mConfigPara.dstHeight;
        nSrcStride = ALIGN(pContext->mConfigPara.srcWidth, 16);

        int rotate = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamRotation, &rotate);
        // int IQpOffset = 0;  // IQp offset value to offset I frame Qp to decrease I frame size.
        // VencSetParameter(pContext->pVideoEnc, VENC_IndexParamIQpOffset, &IQpOffset);

        unsigned int isNight = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamIsNightCaseFlag, (void*)&isNight);
        // VencHighPassFilter mVencHighPassFilter;
        // VencSetParameter(pContext->pVideoEnc, VENC_IndexParamHighPassFilter, &mVencHighPassFilter);
        unsigned int productMode = 0; // 0:normal; 1:ipc
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamProductCase, &productMode);
        unsigned int sensorType = 0; // 0:DIS_WDR; 1:EN_WDR
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamSensorType, &sensorType);
    }
    else if (VENC_CODEC_H265 == pContext->mConfigPara.mVideoEncoderFmt)
    {
        VencH265Param h265Param;
        memset(&h265Param, 0, sizeof(VencH265Param));

        h265Param.sProfileLevel.nProfile = map_H265_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);        
        h265Param.sProfileLevel.nLevel = VENC_H265Level62;

        switch(pContext->mConfigPara.mRcMode)
        {
            case 0: // CBR
            {
                h265Param.sRcParam.eRcMode = AW_CBR;
                int minQp = pContext->mConfigPara.nMinqp;
                int maxQp = pContext->mConfigPara.nMaxqp;
                if (!(minQp>=1 && minQp<=51))
                {
                    alogw("h265CBR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                    pContext->mConfigPara.nMinqp = 1;
                }
                if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                {
                    alogw("h265CBR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                    pContext->mConfigPara.nMaxqp = 51;
                }
                h265Param.sQPRange.nMinqp = pContext->mConfigPara.nMinqp;
                h265Param.sQPRange.nMaxqp = pContext->mConfigPara.nMaxqp;
                h265Param.sQPRange.nMinPqp = pContext->mConfigPara.nMinPqp;
                h265Param.sQPRange.nMaxPqp = pContext->mConfigPara.nMaxPqp;
                h265Param.sQPRange.nQpInit = pContext->mConfigPara.nQpInit;
                h265Param.sQPRange.bEnMbQpLimit = pContext->mConfigPara.bEnMbQpLimit;
                break;
            }
            case 1: // VBR
            {
                h265Param.sRcParam.eRcMode = AW_VBR;
                int minQp = pContext->mConfigPara.nMinqp;
                int maxQp = pContext->mConfigPara.nMaxqp;
                if (!(minQp>=1 && minQp<=51))
                {
                    alogw("h265VBR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                    pContext->mConfigPara.nMinqp = 1;
                }
                if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                {
                    alogw("h265VBR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                    pContext->mConfigPara.nMaxqp = 51;
                }
                h265Param.sQPRange.nMinqp = pContext->mConfigPara.nMinqp;
                h265Param.sQPRange.nMaxqp = pContext->mConfigPara.nMaxqp;
                h265Param.sQPRange.nMinPqp = pContext->mConfigPara.nMinPqp;
                h265Param.sQPRange.nMaxPqp = pContext->mConfigPara.nMaxPqp;
                h265Param.sQPRange.nQpInit = pContext->mConfigPara.nQpInit;
                h265Param.sQPRange.bEnMbQpLimit = pContext->mConfigPara.bEnMbQpLimit;
                h265Param.sRcParam.sVbrParam.uMaxBitRate = pContext->mConfigPara.mMaxBitRate;
                h265Param.sRcParam.sVbrParam.nMovingTh = pContext->mConfigPara.mMovingTh;
                h265Param.sRcParam.sVbrParam.nQuality = pContext->mConfigPara.mQuality;
                h265Param.sRcParam.sVbrParam.nIFrmBitsCoef = pContext->mConfigPara.mIFrmBitsCoef;
                h265Param.sRcParam.sVbrParam.nPFrmBitsCoef = pContext->mConfigPara.mPFrmBitsCoef;
                break;
            }
            case 2: // FIXQP
            {
                h265Param.sRcParam.eRcMode = AW_FIXQP;
                h265Param.sRcParam.sFixQp.bEnable = 1;
                int IQp = pContext->mConfigPara.mIQp;
                int PQp = pContext->mConfigPara.mPQp;
                if (!(IQp>=1 && IQp<=51))
                {
                    alogw("h265FixQp IQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 30!", IQp);
                    pContext->mConfigPara.mIQp = 30;
                }
                if (!(PQp>=1 && PQp<=51))
                {
                    alogw("h265FixQp PQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 30!", PQp);
                    pContext->mConfigPara.mPQp = 30;
                }
                h265Param.sRcParam.sFixQp.nIQp = pContext->mConfigPara.mIQp;
                h265Param.sRcParam.sFixQp.nPQp = pContext->mConfigPara.mPQp;
                break;
            }
            case 3: // QPMAP
            {
#ifdef TEST_QPMAP
                h265Param.sRcParam.eRcMode = AW_QPMAP;
                alogd("H265 set RcMode = AW_QPMAP");

                h265Param.sQPRange.nMinqp = pContext->mConfigPara.nMinqp;
                h265Param.sQPRange.nMaxqp = pContext->mConfigPara.nMaxqp;
                h265Param.sQPRange.nMinPqp = pContext->mConfigPara.nMinPqp;
                h265Param.sQPRange.nMaxPqp = pContext->mConfigPara.nMaxPqp;
                h265Param.sQPRange.nQpInit = pContext->mConfigPara.nQpInit;
                h265Param.sQPRange.bEnMbQpLimit = pContext->mConfigPara.bEnMbQpLimit;
#endif
                break;
            }
            default:
            {
                aloge("fatal error! wrong h265 rc mode[0x%x], check code!", pContext->mConfigPara.mRcMode);
                break;
            }
        }

        h265Param.nFramerate = pContext->mConfigPara.mVideoFrameRate;
        h265Param.nSrcFramerate = pContext->mConfigPara.mVideoFrameRate;
        h265Param.nBitrate = pContext->mConfigPara.mVideoBitRate;

        //set gop size
        h265Param.idr_period = pContext->mConfigPara.mVideoFrameRate;
        h265Param.nIntraPeriod = pContext->mConfigPara.mVideoFrameRate;
        h265Param.nGopSize = pContext->mConfigPara.mVideoFrameRate;

        h265Param.nQPInit = 30; /* qp of first IDR_frame if use rate control */
        // config the GopParam
        h265Param.sGopParam.bUseGopCtrlEn = 0;
        h265Param.sGopParam.eGopMode = AW_NORMALP;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamH265Param, &h265Param);

        int nFastEnc = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamFastEnc, &nFastEnc);
        unsigned char bPFrameIntraEn = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamPFrameIntraEn, (void*)&bPFrameIntraEn);

        int vbvSize = 8*1024*1024;
        int nThreshSize = vbvSize;
        alogd("set encode vbv size %d, frame length threshold %d", vbvSize, nThreshSize);
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamSetVbvSize, &vbvSize);
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamSetFrameLenThreshold, &nThreshSize);

        nDstWidth = pContext->mConfigPara.dstWidth;
        nDstHeight = pContext->mConfigPara.dstHeight;
        nSrcStride = ALIGN(pContext->mConfigPara.srcWidth, 16);

        int rotate = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamRotation, &rotate);
        // int IQpOffset = 0;  // IQp offset value to offset I frame Qp to decrease I frame size.
        // VencSetParameter(pContext->pVideoEnc, VENC_IndexParamIQpOffset, &IQpOffset);

        VencH264VideoSignal sVideoSignal;
        memset(&sVideoSignal, 0, sizeof(VencH264VideoSignal));
        sVideoSignal.video_format = DEFAULT;
        sVideoSignal.full_range_flag = 0;
        sVideoSignal.src_colour_primaries = VENC_YCC;
        sVideoSignal.dst_colour_primaries = sVideoSignal.src_colour_primaries;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamVUIVideoSignal, &sVideoSignal); 

        unsigned int isNight = 0;
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamIsNightCaseFlag, (void*)&isNight);
        // VencHighPassFilter mVencHighPassFilter;
        // VencSetParameter(pContext->pVideoEnc, VENC_IndexParamHighPassFilter, &mVencHighPassFilter);
        unsigned int productMode = 0; // 0:normal; 1:ipc
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamProductCase, &productMode);
        unsigned int sensorType = 0; // 0:DIS_WDR; 1:EN_WDR
        VencSetParameter(pContext->pVideoEnc, VENC_IndexParamSensorType, &sensorType);
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pContext->mConfigPara.mVideoEncoderFmt);
        return -1;
    }

    if (pContext->mConfigPara.srcWidth != nSrcStride)
    {
        aloge("fatal error! srcWidth[%d]!=srcStride[%d]", pContext->mConfigPara.srcWidth, nSrcStride);
    }

#ifdef TEST_QPMAP
    MBParamInit(&pContext->mMBModeCtrl, &pContext->mMBInfo, &pContext->mGetMbInfo, &pContext->mConfigPara);
    SetMbInfo(pContext->pVideoEnc, &pContext->mMBInfo);

    // Notice: for first frame, not set MBModeCtrl.
    // SetMbModeCtrl(pContext->pVideoEnc, &pContext->mMBModeCtrl);
#endif

    VencBaseConfig baseConfig;
    memset(&baseConfig, 0 ,sizeof(VencBaseConfig));
    baseConfig.nInputWidth = pContext->mConfigPara.srcWidth;
    baseConfig.nInputHeight = pContext->mConfigPara.srcHeight;
    baseConfig.nStride = nSrcStride;
    baseConfig.nDstWidth = nDstWidth;
    baseConfig.nDstHeight = nDstHeight;
    baseConfig.eInputFormat = pContext->mConfigPara.srcPixFmt;
    baseConfig.bIsVbvNoCache = 1; //video encode config vbv buffer no cache!

    ret = VencInit(pContext->pVideoEnc, &baseConfig);
    if (VENC_RESULT_OK != ret)
    {
        aloge("fatal error! Venc Init fail! ret=%d", ret);
        return -1;
    }
    alogd("Venc Init success");

    int nInputWidth = pContext->mConfigPara.srcWidth;
    int nInputHeight = pContext->mConfigPara.srcHeight;
    int nAlignW = (nInputWidth + 15)& ~15;
    int nAlignH = (nInputHeight + 15)& ~15;

    VencAllocateBufferParam bufferParam;
    memset(&bufferParam, 0 ,sizeof(VencAllocateBufferParam));
    bufferParam.nSizeY = nAlignW*nAlignH;
    bufferParam.nSizeC = nAlignW*nAlignH/2;
    for (int i = 0; i < DEMO_INPUT_BUFFER_NUM; i++)
    {
        VencAllocateInputBuf(pContext->pVideoEnc, &bufferParam, &pContext->mInputBufMgr.buffer_node[i].inputbuffer);
        enqueue(&pContext->mInputBufMgr.valid_quene, &pContext->mInputBufMgr.buffer_node[i]);
    }

    ret = VencSetCallbacks(pContext->pVideoEnc, &gVencCompCbType, (void *)pContext);
    if (VENC_RESULT_OK != ret)
    {
        aloge("fatal error! Venc SetCallbacks fail! ret=%d", ret);
    }
    else
    {
        alogd("Venc SetCallbacks success");
    }

    //open the proc to catch parame of VE.
    VeProcSet mProcSet;
    mProcSet.bProcEnable = 1;
    mProcSet.nProcFreq = 30;
    mProcSet.nStatisBitRateTime = 1000;
    mProcSet.nStatisFrRateTime  = 1000;
    if (VencSetParameter(pContext->pVideoEnc, VENC_IndexParamProcSet, (void*)&mProcSet) != 0)
    {
        aloge("fatal error! can not open proc info!");
    }

    unsigned char mChnId = pContext->mConfigPara.mVencChnID;
    if (VencSetParameter(pContext->pVideoEnc, VENC_IndexParamChannelNum, (void*)&mChnId) != 0)
    {
        aloge("fatal error! set channel num[%d] fail!", mChnId);
    }

    return ret;
}

static int VideoEncDeInit(SampleQPMAPContext* pContext)
{
    int ret = 0;
    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

    alogd("enter");

    VencDestroy(pContext->pVideoEnc);

#ifdef TEST_QPMAP
    MBParamDeinit(&pContext->mMBModeCtrl, &pContext->mMBInfo, &pContext->mGetMbInfo, &pContext->mConfigPara);
#endif

    alogd("success");

    return 0;
}

static void *VencDataProcessThread(void *pThreadData)
{
    SampleQPMAPContext *pContext = (SampleQPMAPContext*)pThreadData;
    int ret = 0;

    // write SpsPps Header
    VencHeaderData sps_pps_data;
    memset(&sps_pps_data, 0, sizeof(VencHeaderData));
    if (VENC_CODEC_H264 == pContext->mConfigPara.mVideoEncoderFmt)
    {
        VencGetParameter(pContext->pVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
    }
    else if (VENC_CODEC_H265 == pContext->mConfigPara.mVideoEncoderFmt)
    {
        VencGetParameter(pContext->pVideoEnc, VENC_IndexParamH265Header, &sps_pps_data);
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pContext->mConfigPara.mVideoEncoderFmt);
    }
    fwrite(sps_pps_data.pBuffer, 1, sps_pps_data.nLength, pContext->pFpOutput);
    alogd("write SpsPps Header, length=%d", sps_pps_data.nLength);

    int nInputWidth = pContext->mConfigPara.srcWidth;
    int nInputHeight = pContext->mConfigPara.srcHeight;

    while (!pContext->mOverFlag)
    {
        InputBufferInfo *pInputBufInfo = dequeue(&pContext->mInputBufMgr.valid_quene);
        if (pInputBufInfo == NULL)
        {
            aloge("fatal error! get input buffer valid quene failed");
            usleep(10*1000);
            continue;
        }
        VencInputBuffer *pInputBuf = &pInputBufInfo->inputbuffer;

        unsigned int size0, size1, csize;
        if (VENC_CODEC_H264 == pContext->mConfigPara.mVideoEncoderFmt)
        {
            csize = nInputWidth * ALIGN(nInputHeight, 16) / 4;
            alogv("csize=%d, W=%d, H=%d\n", csize, nInputWidth, nInputHeight);
        }
        else
        {
            csize = nInputWidth * nInputHeight / 4;
        }
        size0 = fread(pInputBuf->pAddrVirY, 1, nInputWidth*nInputHeight, pContext->pFpInput);
        size1 = fread(pInputBuf->pAddrVirC, 1, nInputWidth*nInputHeight/4, pContext->pFpInput);
        size1 += fread(pInputBuf->pAddrVirC + csize, 1, nInputWidth*nInputHeight/4, pContext->pFpInput);

        if ((size0 != nInputWidth*nInputHeight) || (size1 != nInputWidth*nInputHeight/2))
        {
            alogw("warning: read to eof or somthing wrong, should stop! size0=%d, size1=%d, W=%d, H=%d\n",
                size0, size1, nInputWidth, nInputHeight);
            cdx_sem_up(&pContext->mSemExit);
            break;
        }
        alogv("Venc Input size0=%d, size1=%d", size0, size1);

        pInputBuf->bNeedFlushCache = 1;
        ret = VencQueueInputBuf(pContext->pVideoEnc, pInputBuf);
        if (ret != 0)
        {
            aloge("fatal error! Venc QueueInputBuf fail, ret=%d", ret);
            return -1;
        }
        enqueue(&pContext->mInputBufMgr.empty_quene, pInputBufInfo);

        // Wait for this frame to be encoded.
        cdx_sem_down(&pContext->mSemInputBufferDone);

        VencOutputBuffer outputBuffer;
        memset(&outputBuffer, 0, sizeof(VencOutputBuffer));
        ret = VencDequeueOutputBuf(pContext->pVideoEnc, &outputBuffer);
        if (ret != 0)
        {
            aloge("fatal error! get stream failed, ret = %d", ret);
            usleep(10*1000);
            continue;
        }
        alogv("Venc Output size0=%d, size1=%d", outputBuffer.nSize0, outputBuffer.nSize1);

        if (outputBuffer.nSize0)
            fwrite(outputBuffer.pData0, 1, outputBuffer.nSize0, pContext->pFpOutput);
        if (outputBuffer.nSize1)
            fwrite(outputBuffer.pData1, 1, outputBuffer.nSize1, pContext->pFpOutput);

        VencQueueOutputBuf(pContext->pVideoEnc, &outputBuffer);
    }

    alogd("get thread exit!");
    return NULL;
}

static int prepare(SampleQPMAPContext* pContext)
{
    int ret = 0;
    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

    alogd("enter");

    cdx_sem_init(&pContext->mSemInputBufferDone, 0);

    pContext->pFpInput = fopen(pContext->mConfigPara.inputFile, "r");
    if (NULL == pContext->pFpInput)
    {
        aloge("fatal error! open file %s fail\n", pContext->mConfigPara.inputFile);
        goto exit_err;
    }
    fseek(pContext->pFpInput, 0L, SEEK_SET);

    pContext->pFpOutput = fopen(pContext->mConfigPara.outputFile, "wb");
    if (NULL == pContext->pFpOutput)
    {
        aloge("fatal error! open file %s fail\n", pContext->mConfigPara.outputFile);
        goto exit_err;
    }
    fseek(pContext->pFpOutput, 0L, SEEK_SET);

    VideoEncoder *pVideoEnc = VencCreate(pContext->mConfigPara.mVideoEncoderFmt);
    if (NULL == pVideoEnc)
    {
        aloge("fatal error! Venc Create fail! venc type=%d", pContext->mConfigPara.mVideoEncoderFmt);
        goto exit_err;
    }
    pContext->pVideoEnc = pVideoEnc;
    alogd("Venc Create success, hdl=%p ", pContext->pVideoEnc);

    ret = VideoEncInit(pContext);
    if (0 != ret)
    {
        aloge("fatal error! Video EncInit fail! ret=%d", ret);
        goto exit_err;
    }

    pContext->mOverFlag = FALSE;
    ret = pthread_create(&pContext->mVencDataProcessThdId, NULL, VencDataProcessThread, pContext);
    if (ret || !pContext->mVencDataProcessThdId)
    {
        aloge("fatal error! create VencDataProcess Thread fail\n");
        goto exit_err;
    }

    alogd("success");

    return 0;

exit_err:
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

    VideoEncDeInit(pContext);

    alogd("fail");
    return -1;
}

static int start(SampleQPMAPContext* pContext)
{
    int ret = 0;
    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }
    alogd("enter");

    ret = VencStart(pContext->pVideoEnc);
    if (VENC_RESULT_OK != ret)
    {
        aloge("fatal error! Venc Start fail, ret=%d", ret);
    }
    else
    {
        alogd("Venc Start success");
    }

    alogd("%s \n", ((0==ret) ? "success" : "fail"));
    return ret;
}

static int stop(SampleQPMAPContext* pContext)
{
    int ret = 0;

    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

    pContext->mOverFlag = TRUE;

    alogd("over, join thread");
    pthread_join(pContext->mVencDataProcessThdId, NULL);

    alogd("%s \n", ((0==ret) ? "success" : "fail"));
    return ret;
}

static int release(SampleQPMAPContext *pContext)
{
    int ret = 0;

    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
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

    ret = VideoEncDeInit(pContext);

    cdx_sem_deinit(&pContext->mSemInputBufferDone);

    alogd("%s \n", ((0==ret) ? "success" : "fail"));
    return ret;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");

    if(NULL != gpQPMAPContext)
    {
        cdx_sem_up(&gpQPMAPContext->mSemExit);
    }
}

int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
    int result = 0;
    int ret = 0;

    alogd("sample_QPMAP running!");

    /* malloc Context */
    SampleQPMAPContext *pContext = (SampleQPMAPContext *)malloc(sizeof(SampleQPMAPContext));
    if (NULL == pContext)
    {
        aloge("malloc SampleQPMAPContext fail, size=%d", sizeof(SampleQPMAPContext));
        return -1;
    }
    memset(pContext, 0, sizeof(SampleQPMAPContext));
    gpQPMAPContext = pContext; // for handle_exit()

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
