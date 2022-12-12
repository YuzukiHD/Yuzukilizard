#define LOG_TAG "sample_uvcout"
#include <utils/plat_log.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <getopt.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <linux/usb/ch9.h>
#include <linux/g2d_driver.h>

#include "linux/videodev2.h"
#include "./include/video.h"
#include "./include/uvc.h"
#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include "media/mpi_venc.h"
#include "media/mpi_sys.h"
#include "mm_common.h"
#include "mm_comm_venc.h"
#include "mm_comm_rc.h"
#include <confparser.h>
#include <utils/PIXEL_FORMAT_E_g2d_format_convert.h>
#include <mpi_videoformat_conversion.h>
#include <cdx_list.h>

#include "sample_uvcout.h"
#include "sample_uvcout_config.h"

#define TIME_TEST
#ifdef TIME_TEST
#include "SystemBase.h"
#endif

#define ISP_RUN             (1)

static int g_bWaitVencDone = 0;
static int g_bSampleExit   = 0;

static SampleUVCFormat gFormat[MAX_FORMAT_SUPPORT_NUM][MAX_FRAME_SUPPORT_NUM] =
{
    {
        {
            .iWidth  = 1920,
            .iHeight = 1080,
            .iFormat = V4L2_PIX_FMT_MJPEG,
            .iInterval = 333333,
        },
        {
            .iWidth  = 1280,
            .iHeight = 720,
            .iFormat = V4L2_PIX_FMT_MJPEG,
            .iInterval = 333333,
        },
        {
            .iWidth  = 640,
            .iHeight = 480,
            .iFormat = V4L2_PIX_FMT_MJPEG,
            .iInterval = 333333,
        },
    },
    {
        {
            .iWidth  = 320,
            .iHeight = 240,
            .iFormat = V4L2_PIX_FMT_YUYV,
            .iInterval = 333333,
        },
    },
    {
        {
            .iWidth  = 1920,
            .iHeight = 1080,
            .iFormat = V4L2_PIX_FMT_H264,
            .iInterval = 333333,
        },
        {
            .iWidth  = 1280,
            .iHeight = 720,
            .iFormat = V4L2_PIX_FMT_H264,
            .iInterval = 333333,
        },
    },
};

static int OpenUVCDevice(SampleUVCDevice *pstUVCDev)
{
    struct v4l2_capability stCap;
    int iRet;
    int iFd;
    char pcDevName[256];

    sprintf(pcDevName, "/dev/video%d", pstUVCDev->iDev);
    alogd("open uvc device[%s]", pcDevName);
    iFd = open(pcDevName, O_RDWR | O_NONBLOCK);
    if (iFd < 0) {
        aloge("open video device failed: device[%s] %s\n", pcDevName, strerror(errno));
        goto open_err;
    }
    iRet = ioctl(iFd, VIDIOC_QUERYCAP, &stCap);
    if (iRet < 0) {
        aloge("unable to query device: %s (%d)\n", strerror(errno), errno);
        goto query_cap_err;
    }
    alogd("device is %s on bus %s\n", stCap.card, stCap.bus_info);

    pstUVCDev->iFd = iFd;

    return 0;

query_cap_err:
    close(iFd);
open_err:
    return -1;
}

static void CloseUVCDevice(SampleUVCDevice *pUVCDev)
{
    close(pUVCDev->iFd);
}

static int OpenG2d(SampleUVCInfo *pUVCInfo, SampleUVCConfig *pUVCConfig)
{
    int nFrmSize = 0;

    pUVCInfo->mG2dFd = open("/dev/g2d", O_RDWR, 0);
    if (pUVCInfo->mG2dFd < 0)
    {
        aloge("fatal error! open g2d device fail!");
        return -1;
    }
    memset(&pUVCInfo->mG2dProcFrm, 0, sizeof(VIDEO_FRAME_INFO_S));
    pUVCInfo->mG2dProcFrm.VFrame.mWidth = pUVCConfig->iCapWidth;
    pUVCInfo->mG2dProcFrm.VFrame.mHeight = pUVCConfig->iCapHeight;
    pUVCInfo->mG2dProcFrm.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YVYU_AW_PACKAGE_422;
    nFrmSize = pUVCInfo->mG2dProcFrm.VFrame.mWidth * pUVCInfo->mG2dProcFrm.VFrame.mHeight;
    AW_MPI_SYS_MmzAlloc_Cached(&pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[0], \
        &pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[0], nFrmSize*2);
    if (0 == pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[0] || NULL == pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[0])
    {
        aloge("fatal error! alloc g2d proc y data buffer fail!");
        return -1;
    }

    return 0;
}

static int G2dProc(SampleUVCInfo *pUVCInfo, VIDEO_FRAME_INFO_S *pSrcFrame, VIDEO_FRAME_INFO_S *pDstFrame)
{
    int ret = 0;
    g2d_blt_h blt;
    g2d_fmt_enh eSrcFormat, eDstFormat;
    int nFrmSize = 0;

    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pSrcFrame->VFrame.mPixelFormat, &eSrcFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pSrcFrame->VFrame.mPixelFormat);
        return -1;
    }
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pDstFrame->VFrame.mPixelFormat, &eDstFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pDstFrame->VFrame.mPixelFormat);
        return -1;
    }

    memset(&blt, 0, sizeof(g2d_blt_h));
    blt.flag_h = G2D_BLT_NONE_H;
    blt.src_image_h.format = eSrcFormat;
    blt.src_image_h.laddr[0] = (unsigned int)pSrcFrame->VFrame.mPhyAddr[0];
    blt.src_image_h.laddr[1] = (unsigned int)pSrcFrame->VFrame.mPhyAddr[1];
    blt.src_image_h.laddr[2] = (unsigned int)pSrcFrame->VFrame.mPhyAddr[2];
    blt.src_image_h.width = pSrcFrame->VFrame.mWidth;
    blt.src_image_h.height = pSrcFrame->VFrame.mHeight;
    blt.src_image_h.align[0] = 0;
    blt.src_image_h.align[1] = 0;
    blt.src_image_h.align[2] = 0;
    blt.src_image_h.clip_rect.x = 0;
    blt.src_image_h.clip_rect.y = 0;
    blt.src_image_h.clip_rect.w = pSrcFrame->VFrame.mWidth;
    blt.src_image_h.clip_rect.h = pSrcFrame->VFrame.mHeight;
    blt.src_image_h.gamut = G2D_BT601;
    blt.src_image_h.bpremul = 0;
    blt.src_image_h.mode = G2D_PIXEL_ALPHA;
    blt.src_image_h.fd = -1;
    blt.src_image_h.use_phy_addr = 1;

    blt.dst_image_h.format = eDstFormat;
    blt.dst_image_h.laddr[0] = (unsigned int)pDstFrame->VFrame.mPhyAddr[0];
    blt.dst_image_h.laddr[1] = (unsigned int)pDstFrame->VFrame.mPhyAddr[1];
    blt.dst_image_h.laddr[2] = (unsigned int)pDstFrame->VFrame.mPhyAddr[2];
    blt.dst_image_h.width = pDstFrame->VFrame.mWidth;
    blt.dst_image_h.height = pDstFrame->VFrame.mHeight;
    blt.dst_image_h.align[0] = 0;
    blt.dst_image_h.align[1] = 0;
    blt.dst_image_h.align[2] = 0;
    blt.dst_image_h.clip_rect.x = 0;
    blt.dst_image_h.clip_rect.y = 0;
    blt.dst_image_h.clip_rect.w = pDstFrame->VFrame.mWidth;
    blt.dst_image_h.clip_rect.h = pDstFrame->VFrame.mHeight;
    blt.dst_image_h.gamut = G2D_BT601;
    blt.dst_image_h.bpremul = 0;
    blt.dst_image_h.mode = G2D_PIXEL_ALPHA;
    blt.dst_image_h.fd = -1;
    blt.dst_image_h.use_phy_addr = 1;

    ret = ioctl(pUVCInfo->mG2dFd, G2D_CMD_BITBLT_H, (unsigned long)&blt);
    if (ret < 0)
    {
        aloge("fatal error! g2d proc fail!");
        return -1;
    }
    nFrmSize = pDstFrame->VFrame.mWidth * pDstFrame->VFrame.mHeight;
    AW_MPI_SYS_MmzFlushCache(pDstFrame->VFrame.mPhyAddr[0], pDstFrame->VFrame.mpVirAddr[0], nFrmSize*2);

    return 0;
}

static void CloseG2d(SampleUVCInfo *pUVCInfo)
{
    if (pUVCInfo->mG2dFd >= 0)
    {
        close(pUVCInfo->mG2dFd);
        pUVCInfo->mG2dFd = -1;
    }
    if (pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[0] > 0 || NULL != pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[0])
    {
        AW_MPI_SYS_MmzFree(pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[0], pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[0]);
        pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[0] = 0;
        pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[0] = NULL;
    }
    if (pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[1] > 0 || NULL != pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[1])
    {
        AW_MPI_SYS_MmzFree(pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[1], pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[1]);
        pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[1] = 0;
        pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[1] = NULL;
    }
    if (pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[2] > 0 || NULL != pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[2])
    {
        AW_MPI_SYS_MmzFree(pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[2], pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[2]);
        pUVCInfo->mG2dProcFrm.VFrame.mPhyAddr[2] = 0;
        pUVCInfo->mG2dProcFrm.VFrame.mpVirAddr[2] = NULL;
    }
}

ERRORTYPE VencStreamCallBack(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    VENC_STREAM_S *pFrame = (VENC_STREAM_S *)pEventData;

    switch (event) {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            if (pFrame != NULL) {
                g_bWaitVencDone = 1;
            }
            break;

        default:
            break;
    }

    return SUCCESS;
}

static inline int DoUVCVideoBufProcess(SampleUVCDevice *pstUVCDev)
{
    SampleUVCContext *pConfig = (SampleUVCContext *)pstUVCDev->pPrivite;
    SampleUVCInfo *pUVCInfo = &pConfig->mUVCInfo;
    struct v4l2_buffer stBuf;
    int iRet;

    pthread_mutex_lock(&pUVCInfo->mFrmLock);
    if (list_empty(&pUVCInfo->mValidFrm)) {
//        aloge("valid frame is empty!!\n");
        iRet = -1;
        goto frm_empty;
    }

    SampleUVCOutBuf *pUvcOutBuf;
    pUvcOutBuf = list_first_entry(&pUVCInfo->mValidFrm, SampleUVCOutBuf, mList);
    memset(&stBuf, 0, sizeof(struct v4l2_buffer));
    stBuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    stBuf.memory = V4L2_MEMORY_MMAP;
    iRet = ioctl(pstUVCDev->iFd, VIDIOC_DQBUF, &stBuf);
    if (iRet < 0) {
        aloge("Unable to dequeue buffer: %s (%d).\n", strerror(errno), errno);
        if (BUF_DATA_TYPE_H264_HEADER == pUvcOutBuf->eDataType)
        {
            goto frm_empty;
        }
        else
        {
            goto qbuf_err;
        }
    }

    /* fill the v4l2 buffer */
    unsigned int data_size = pUvcOutBuf->iDataSize0 + pUvcOutBuf->iDataSize1 + pUvcOutBuf->iDataSize2;
    if (pstUVCDev->pstFrames[stBuf.index].iBufLen >= data_size)
    {
        memcpy(pstUVCDev->pstFrames[stBuf.index].pVirAddr, pUvcOutBuf->pcData, data_size);
        stBuf.bytesused = data_size;
    }
    else
    {
        aloge("fatal error! buf size %d is less than data size %d", pstUVCDev->pstFrames[stBuf.index].iBufLen, data_size);
        stBuf.bytesused = 0;
    }

    iRet = ioctl(pstUVCDev->iFd, VIDIOC_QBUF, &stBuf);
    if (iRet < 0) {
        aloge("Unable to requeue buffer: %s (%d).\n", strerror(errno), errno);
        if (BUF_DATA_TYPE_H264_HEADER == pUvcOutBuf->eDataType)
        {
            goto frm_empty;
        }
        else
        {
            goto dqbuf_err;
        }
    }
qbuf_err:
dqbuf_err:
    pUvcOutBuf->iDataSize0 = 0;
    pUvcOutBuf->iDataSize1 = 0;
    pUvcOutBuf->iDataSize2 = 0;
    memset(pUvcOutBuf->pcData, 0, pUvcOutBuf->iDataBufSize);
    list_move_tail(&pUvcOutBuf->mList, &pUVCInfo->mIdleFrm);
//  AW_MPI_VENC_ReleaseStream(pUVCInfo->iVencChn, &pUvcOutBuf->stStream);
frm_empty:
    pthread_mutex_unlock(&pUVCInfo->mFrmLock);
    return iRet;
}

static int ConfigUVCBufByVE(SampleUVCOutBuf *pBuf, VENC_STREAM_S *pVencBuf)
{
    unsigned int offsetLen = 0;

    if (pVencBuf->mpPack != NULL && pVencBuf->mpPack->mpAddr0 && pVencBuf->mpPack->mLen0)
    {
        if (pBuf->iDataBufSize < pVencBuf->mpPack->mLen0)
        {
            aloge("fatal error! buf remain size %d is less than data size0 %d", pBuf->iDataBufSize, pVencBuf->mpPack->mLen0);
            return -1;
        }
        pBuf->iDataSize0 = pVencBuf->mpPack->mLen0;
        memcpy(pBuf->pcData, pVencBuf->mpPack->mpAddr0, pVencBuf->mpPack->mLen0);
        offsetLen = pVencBuf->mpPack->mLen0;
    }
    if (pVencBuf->mpPack != NULL && pVencBuf->mpPack->mpAddr1 && pVencBuf->mpPack->mLen1)
    {
        if (pBuf->iDataBufSize < pVencBuf->mpPack->mLen1 + offsetLen)
        {
            aloge("fatal error! buf remain size %d is less than data size1 %d", pBuf->iDataBufSize - offsetLen, pVencBuf->mpPack->mLen1);
            return -1;
        }
        pBuf->iDataSize1 = pVencBuf->mpPack->mLen1;
        memcpy(pBuf->pcData + offsetLen, pVencBuf->mpPack->mpAddr1, pVencBuf->mpPack->mLen1);
        offsetLen += pVencBuf->mpPack->mLen1;
    }
    if (pVencBuf->mpPack != NULL && pVencBuf->mpPack->mpAddr2 && pVencBuf->mpPack->mLen2)
    {
        if (pBuf->iDataBufSize < pVencBuf->mpPack->mLen2 + offsetLen)
        {
            aloge("fatal error! buf remain size %d is less than data size2 %d", pBuf->iDataBufSize - offsetLen, pVencBuf->mpPack->mLen2);
            return -1;
        }
        pBuf->iDataSize2 = pVencBuf->mpPack->mLen2;
        memcpy(pBuf->pcData + offsetLen, pVencBuf->mpPack->mpAddr2, pVencBuf->mpPack->mLen2);
        offsetLen += pVencBuf->mpPack->mLen2;
    }
    return 0;
}

static int ConfigUVCBufByVI(SampleUVCOutBuf *pBuf, VIDEO_FRAME_INFO_S *pFrmInfo)
{
    unsigned int offsetLen = 0;

    if (pFrmInfo->VFrame.mpVirAddr[0])
    {
        pBuf->iDataSize0 = pFrmInfo->VFrame.mWidth * pFrmInfo->VFrame.mHeight * 2;
        memcpy(pBuf->pcData + offsetLen, pFrmInfo->VFrame.mpVirAddr[0], pBuf->iDataSize0);
        offsetLen += pBuf->iDataSize0;
    }
    if (pFrmInfo->VFrame.mpVirAddr[1])
    {
        pBuf->iDataSize1 = pFrmInfo->VFrame.mWidth * pFrmInfo->VFrame.mHeight / 2;
        memcpy(pBuf->pcData + offsetLen, pFrmInfo->VFrame.mpVirAddr[1], pBuf->iDataSize1);
        offsetLen += pBuf->iDataSize1;
    }
    if (pFrmInfo->VFrame.mpVirAddr[2])
    {
        pBuf->iDataSize2 = pFrmInfo->VFrame.mWidth * pFrmInfo->VFrame.mHeight / 2;
        memcpy(pBuf->pcData + offsetLen, pFrmInfo->VFrame.mpVirAddr[2], pBuf->iDataSize2);
        offsetLen += pBuf->iDataSize2;
    }

    return 0;
}

static void *CaptureThread(void *pArg)
{
    SampleUVCContext *pContext = (SampleUVCContext *)pArg;
    SampleUVCInfo *pUVCInfo = &pContext->mUVCInfo;
    ERRORTYPE eRet = SUCCESS;
    int ret = 0;
    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    VIDEO_FRAME_INFO_S stVideoFrmameInfo;
    VencHeaderData stVencHeadDat;

    pUVCInfo->mbCapRunningFlag = TRUE;
    alogd("get video frame thread running!");
    if (MM_INVALID_CHN != pUVCInfo->iVencChn && \
        PT_H264 == pContext->mUVCConfig.eEncoderType)
    {
        pthread_mutex_lock(&pUVCInfo->mFrmLock);
        SampleUVCOutBuf *pBuf = list_first_entry_or_null(&pUVCInfo->mIdleFrm, SampleUVCOutBuf, mList);
        if (NULL != pBuf)
        {
            memset(&stVencHeadDat, 0, sizeof(VencHeaderData));
            eRet = AW_MPI_VENC_GetH264SpsPpsInfo(pUVCInfo->iVencChn, &stVencHeadDat);
            if (SUCCESS != eRet)
            {
                aloge("fatal error! venc get h264 stream header fail!");
            }
            alogd("get h264 spspps info, info len[%d]", stVencHeadDat.nLength);
            pBuf->eDataType = BUF_DATA_TYPE_H264_HEADER;
            pBuf->iDataSize0 = stVencHeadDat.nLength;
            pBuf->iDataSize1 = 0;
            pBuf->iDataSize2 = 0;
            memcpy(pBuf->pcData, stVencHeadDat.pBuffer, stVencHeadDat.nLength);
            list_move_tail(&pBuf->mList, &pUVCInfo->mValidFrm);
        }
        else
        {
            alogw("impossible why uvc idle frm is null!");
        }
        pthread_mutex_unlock(&pUVCInfo->mFrmLock);
    }

    while(1)
    {
        if (pUVCInfo->mbCapExitFlag)
        {
            alogd("capture thread exit!");
            break;
        }
        if (MM_INVALID_CHN != pUVCInfo->iVencChn && \
            PT_BUTT != pContext->mUVCConfig.eEncoderType)
        {
            memset(&stVencStream, 0, sizeof(VENC_STREAM_S));
            memset(&stVencPack, 0, sizeof(VENC_PACK_S));
            stVencStream.mPackCount = 1;
            stVencStream.mpPack = &stVencPack;
            eRet = AW_MPI_VENC_GetStream(pUVCInfo->iVencChn, &stVencStream, 4000);
            if (SUCCESS != eRet)
            {
                alogw("get venc frame fail!");
                continue;
            }
        }
        else
        {
            memset(&stVideoFrmameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));
            eRet = AW_MPI_VI_GetFrame(pUVCInfo->iVippDev, pUVCInfo->iVippChn, &stVideoFrmameInfo, 200);
            if (SUCCESS != eRet)
            {
                alogw("get vi frame fail!");
                continue;
            }
        }

        pthread_mutex_lock(&pUVCInfo->mFrmLock);
        SampleUVCOutBuf *pBuf = list_first_entry_or_null(&pUVCInfo->mIdleFrm, SampleUVCOutBuf, mList);
        if (NULL == pBuf)
        {
            if (MM_INVALID_CHN != pUVCInfo->iVencChn && \
                PT_BUTT != pContext->mUVCConfig.eEncoderType)
            {
                AW_MPI_VENC_ReleaseStream(pUVCInfo->iVencChn, &stVencStream);
            }
            else
            {
                AW_MPI_VI_ReleaseFrame(pUVCInfo->iVippDev, pUVCInfo->iVippChn, &stVideoFrmameInfo);
            }
            pthread_mutex_unlock(&pUVCInfo->mFrmLock);
            continue;
        }
        if (MM_INVALID_CHN != pUVCInfo->iVencChn && \
            PT_BUTT != pContext->mUVCConfig.eEncoderType)
        {
            if (PT_JPEG == pContext->mUVCConfig.eEncoderType)
            {
                pBuf->eDataType = BUF_DATA_TYPE_JPEG_STREAM;
            }
            else if (PT_MJPEG == pContext->mUVCConfig.eEncoderType)
            {
                pBuf->eDataType = BUF_DATA_TYPE_MJPEG_STREAM;
            }
            else if (PT_H264 == pContext->mUVCConfig.eEncoderType)
            {
                pBuf->eDataType = BUF_DATA_TYPE_H264_STREAM;
            }
            ret = ConfigUVCBufByVE(pBuf, &stVencStream);
            if (ret)
            {
                AW_MPI_VENC_ReleaseStream(pUVCInfo->iVencChn, &stVencStream);
                pthread_mutex_unlock(&pUVCInfo->mFrmLock);
                continue;
            }
            AW_MPI_VENC_ReleaseStream(pUVCInfo->iVencChn, &stVencStream);
        }
        else
        {
            ret = G2dProc(pUVCInfo, &stVideoFrmameInfo, &pUVCInfo->mG2dProcFrm);
            if (ret)
            {
                aloge("fatal error! g2d proc fail!");
                AW_MPI_VI_ReleaseFrame(pUVCInfo->iVippDev, pUVCInfo->iVippChn, &stVideoFrmameInfo);
                pthread_mutex_unlock(&pUVCInfo->mFrmLock);
                continue;
            }
            pBuf->eDataType = BUF_DATA_TYPE_YUV2_STREAM;
            ConfigUVCBufByVI(pBuf, &pUVCInfo->mG2dProcFrm);
            AW_MPI_VI_ReleaseFrame(pUVCInfo->iVippDev, pUVCInfo->iVippChn, &stVideoFrmameInfo);
        }
        list_move_tail(&pBuf->mList, &pUVCInfo->mValidFrm);
        pthread_mutex_unlock(&pUVCInfo->mFrmLock);
    }
    pthread_exit(NULL);
}

static int InitFrameList(int iFrmNum, SampleUVCContext *pContext)
{
    SampleUVCInfo *pInfo = &pContext->mUVCInfo;
    unsigned int iFrameSize = 0;
    if (V4L2_PIX_FMT_YUYV == pContext->mUVCDev.iFormat)
    {
        iFrameSize = pContext->mUVCDev.iWidth * pContext->mUVCDev.iHeight * 2;
    }
    else
    {
        iFrameSize = pContext->mUVCDev.iWidth * pContext->mUVCDev.iHeight * 3 / 2;
    }

    alogd("begin to alloc frame list.\n");
    if (iFrmNum <= 0) {
        aloge("frame list number must bigger than 0!!\n");
        return -1;
    }

    INIT_LIST_HEAD(&pInfo->mIdleFrm);
    INIT_LIST_HEAD(&pInfo->mValidFrm);
    INIT_LIST_HEAD(&pInfo->mUsedFrm);

    SampleUVCOutBuf *pBufTmp;
    pthread_mutex_init(&pInfo->mFrmLock, NULL);
    for (int i = 0; i < iFrmNum; i++) {
        pBufTmp = malloc(sizeof(SampleUVCOutBuf));
        pBufTmp->pcData = malloc(iFrameSize);
        pBufTmp->iDataBufSize = iFrameSize;
        pBufTmp->iDataSize0 = 0;
        pBufTmp->iDataSize1 = 0;
        pBufTmp->iDataSize2 = 0;
        list_add_tail(&pBufTmp->mList, &pInfo->mIdleFrm);
    }

    return 0;
}

static void FlushFrameList(SampleUVCContext *pContext)
{
    SampleUVCInfo *pUVCInfo = &pContext->mUVCInfo;
    SampleUVCOutBuf *pBufTmp, *pBufTmpNext;

    if (list_empty(&pUVCInfo->mUsedFrm))
    {
        list_for_each_entry_safe(pBufTmp, pBufTmpNext, &pUVCInfo->mUsedFrm, mList)
        {
            list_move_tail(&pBufTmp->mList, &pUVCInfo->mIdleFrm);
        }
    }
    if (list_empty(&pUVCInfo->mValidFrm))
    {
        list_for_each_entry_safe(pBufTmp, pBufTmpNext, &pUVCInfo->mValidFrm, mList)
        {
            list_move_tail(&pBufTmp->mList, &pUVCInfo->mValidFrm);
        }
    }
    if (list_empty(&pUVCInfo->mIdleFrm))
    {
        list_for_each_entry_safe(pBufTmp, pBufTmpNext, &pUVCInfo->mIdleFrm, mList)
        {
            memset(pBufTmp->pcData, 0, pBufTmp->iDataBufSize);
            pBufTmp->eDataType = BUF_DATA_TYPE_BUTT;
            pBufTmp->iDataSize0 = 0;
            pBufTmp->iDataSize1 = 0;
            pBufTmp->iDataSize2 = 0;
        }
    }
}

static void DeinitFrameList(SampleUVCContext *pContext)
{
    SampleUVCInfo *pInfo = &pContext->mUVCInfo;

    alogd("begin to free frame list.\n");
    SampleUVCOutBuf *pBufTmp;
    SampleUVCOutBuf *pBufTmpNext;
    list_for_each_entry_safe(pBufTmp, pBufTmpNext, &pInfo->mUsedFrm, mList)
    {
        list_del(&pBufTmp->mList);
        if (pBufTmp->pcData)
        {
            free(pBufTmp->pcData);
            pBufTmp->pcData = NULL;
        }
        pBufTmp->iDataBufSize = 0;
        pBufTmp->iDataSize0 = 0;
        pBufTmp->iDataSize1 = 0;
        pBufTmp->iDataSize2 = 0;
        if (pBufTmp)
        {
            free(pBufTmp);
            pBufTmp = NULL;
        }
    }
    list_for_each_entry_safe(pBufTmp, pBufTmpNext, &pInfo->mValidFrm, mList)
    {
        list_del(&pBufTmp->mList);
        if (pBufTmp->pcData)
        {
            free(pBufTmp->pcData);
            pBufTmp->pcData = NULL;
        }
        pBufTmp->iDataBufSize = 0;
        pBufTmp->iDataSize0 = 0;
        pBufTmp->iDataSize1 = 0;
        pBufTmp->iDataSize2 = 0;
        if (pBufTmp)
        {
            free(pBufTmp);
            pBufTmp = NULL;
        }
    }
    list_for_each_entry_safe(pBufTmp, pBufTmpNext, &pInfo->mIdleFrm, mList)
    {
        list_del(&pBufTmp->mList);
        if (pBufTmp->pcData)
        {
            free(pBufTmp->pcData);
            pBufTmp->pcData = NULL;
        }
        pBufTmp->iDataBufSize = 0;
        pBufTmp->iDataSize0 = 0;
        pBufTmp->iDataSize1 = 0;
        pBufTmp->iDataSize2 = 0;
        if (pBufTmp)
        {
            free(pBufTmp);
            pBufTmp = NULL;
        }
    }

    pthread_mutex_destroy(&pInfo->mFrmLock);
}

static int createVipp(SampleUVCInfo *pUVCInfo, SampleUVCConfig *pUVCConfig)
{
    ERRORTYPE eRet = SUCCESS;
    VI_ATTR_S stVippAttr;

    memset(&stVippAttr, 0, sizeof(stVippAttr));
    stVippAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    stVippAttr.memtype = V4L2_MEMORY_MMAP;
    stVippAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pUVCConfig->eCapFormat);
    stVippAttr.format.field = V4L2_FIELD_NONE;
    stVippAttr.format.width = pUVCConfig->iCapWidth;
    stVippAttr.format.height = pUVCConfig->iCapHeight;
    stVippAttr.fps = pUVCConfig->iCapFrameRate;
    stVippAttr.use_current_win = 0;
    stVippAttr.wdr_mode = 0;
    stVippAttr.nbufs = 5;
    stVippAttr.nplanes = 2;

    pUVCInfo->iVippDev = pUVCConfig->iCapDev;
    eRet = AW_MPI_VI_CreateVipp(pUVCInfo->iVippDev);
    if (SUCCESS != eRet)
    {
        pUVCInfo->iVippDev = MM_INVALID_DEV;
        aloge("fatal error! create vipp[%d] fail!", pUVCInfo->iVippDev);
        return -1;
    }
    AW_MPI_VI_SetVippAttr(pUVCInfo->iVippDev, &stVippAttr);
    eRet = AW_MPI_VI_EnableVipp(pUVCInfo->iVippDev);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! enable vipp[%d] fail!", pUVCInfo->iVippDev);
        return -1;
    }
#if ISP_RUN
    if (0 == pUVCInfo->iVippDev || 4 == pUVCInfo->iVippDev)
    {
        pUVCInfo->iIspDev = 0;
    }
    AW_MPI_ISP_Run(pUVCInfo->iIspDev);
#endif
    pUVCInfo->iVippChn = 0;
    eRet = AW_MPI_VI_CreateVirChn(pUVCInfo->iVippDev, pUVCInfo->iVippChn, NULL);
    if (SUCCESS != eRet)
    {
        pUVCInfo->iVippChn = MM_INVALID_CHN;
        aloge("fatal error! create vir chn[%d] fail!", pUVCInfo->iVippChn);
        return -1;
    }

    alogd("create vipp[%d] vir vi chn[%d]", pUVCInfo->iVippDev, pUVCInfo->iVippChn);
    return 0;
}

static int createVeChn(SampleUVCInfo *pUVCInfo, SampleUVCConfig *pUVCConfig)
{
    ERRORTYPE eRet = SUCCESS;
    VENC_CHN_ATTR_S stVEncChnAttr;
    VENC_RC_PARAM_S stVEncRcParam;

    memset(&stVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&stVEncRcParam, 0, sizeof(VENC_RC_PARAM_S));
    stVEncChnAttr.VeAttr.Type         = pUVCConfig->eEncoderType;
    stVEncChnAttr.VeAttr.SrcPicWidth  = pUVCConfig->iCapWidth;
    stVEncChnAttr.VeAttr.SrcPicHeight = pUVCConfig->iCapHeight;
    stVEncChnAttr.VeAttr.Field        = VIDEO_FIELD_FRAME;
    stVEncChnAttr.VeAttr.PixelFormat  = pUVCConfig->eCapFormat;
    stVEncChnAttr.VeAttr.Rotate       = ROTATE_NONE;
    if (PT_H264 == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        stVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        stVEncChnAttr.VeAttr.AttrH264e.PicWidth = pUVCConfig->iEncWidth;
        stVEncChnAttr.VeAttr.AttrH264e.PicHeight = pUVCConfig->iEncHeight;
        stVEncChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
        stVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
        stVEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pUVCConfig->iEncBitRate;
        stVEncRcParam.ParamH264Cbr.mMaxQp = 51;
        stVEncRcParam.ParamH264Cbr.mMinQp = 1;
    }
    else if(PT_MJPEG == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame  = TRUE;
        stVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth  = pUVCConfig->iEncWidth;
        stVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = pUVCConfig->iEncHeight;
        stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        stVEncChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pUVCConfig->iCapFrameRate;
        stVEncChnAttr.RcAttr.mAttrMjpegeCbr.fr32DstFrmRate = pUVCConfig->iEncFrameRate;
        stVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pUVCConfig->iEncBitRate;
    } else if (PT_JPEG == stVEncChnAttr.VeAttr.Type) {
        stVEncChnAttr.VeAttr.AttrJpeg.bByFrame   = TRUE;
        stVEncChnAttr.VeAttr.AttrJpeg.PicWidth   = pUVCConfig->iEncWidth;
        stVEncChnAttr.VeAttr.AttrJpeg.PicHeight  = pUVCConfig->iEncHeight;
        stVEncChnAttr.VeAttr.AttrJpeg.mThreshSize  = pUVCConfig->iEncWidth*pUVCConfig->iEncHeight*3/2;
        stVEncChnAttr.VeAttr.AttrJpeg.MaxPicWidth  = 0;
        stVEncChnAttr.VeAttr.AttrJpeg.MaxPicHeight = 0;
        stVEncChnAttr.VeAttr.AttrJpeg.bSupportDCF  = FALSE;
        stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
        stVEncChnAttr.RcAttr.mAttrMjpegeFixQp.mSrcFrmRate = pUVCConfig->iCapFrameRate;
        stVEncChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor    = pUVCConfig->iEncQuality;
        stVEncChnAttr.RcAttr.mAttrMjpegeFixQp.fr32DstFrmRate = pUVCConfig->iEncFrameRate;
    } else {
        aloge("fatal error! unsupport vencoder type[%d]!!\n", stVEncChnAttr.VeAttr.Type);
        return -1;
    }

    pUVCInfo->iVencChn = 0;
    eRet = AW_MPI_VENC_CreateChn(pUVCInfo->iVencChn, &stVEncChnAttr);
    if (eRet < 0) {
        pUVCInfo->iVencChn = MM_INVALID_CHN;
        aloge("create venc channle failed!! venc_chn[%d]\n", pUVCInfo->iVencChn);
        return -1;
    }
    VENC_FRAME_RATE_S stVencFrameRate;
    stVencFrameRate.SrcFrmRate = pUVCConfig->iCapFrameRate;
    stVencFrameRate.DstFrmRate = pUVCConfig->iEncFrameRate;
    eRet = AW_MPI_VENC_SetFrameRate(pUVCInfo->iVencChn, &stVencFrameRate);
    if (eRet < 0) {
        aloge("set venc channle frame rate failed!! venc_chn[%d]\n", pUVCInfo->iVencChn);
        return -1;
    }
    if (PT_H264 == stVEncChnAttr.VeAttr.Type)
    {
        AW_MPI_VENC_SetRcParam(pUVCInfo->iVencChn, &stVEncRcParam);
    }
    if(PT_JPEG == stVEncChnAttr.VeAttr.Type) {
        VENC_PARAM_JPEG_S stJpegParam;
        memset(&stJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
        stJpegParam.Qfactor = pUVCConfig->iEncQuality;
        AW_MPI_VENC_SetJpegParam(pUVCInfo->iVencChn, &stJpegParam);
    }
    MPPCallbackInfo cbInfo;
    cbInfo.callback = (MPPCallbackFuncType)&VencStreamCallBack;
    cbInfo.cookie = (void *)pUVCInfo;
    AW_MPI_VENC_RegisterCallback(pUVCInfo->iVencChn, &cbInfo);

    alogd("create ve chn[%d], encoder type[%d]", pUVCInfo->iVencChn, stVEncChnAttr.VeAttr.Type);
    return 0;
}

static int CreateCaptureThread(SampleUVCContext *pContext)
{
    ERRORTYPE eRet = SUCCESS;
    int ret = 0;
    SampleUVCDevice *pUVCDev = &pContext->mUVCDev;

    MPP_SYS_CONF_S mSysConf;
    mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&mSysConf);
    eRet = AW_MPI_SYS_Init();
    if (eRet < 0) {
        aloge("AW_MPI_SYS_Init failed!");
        goto _exit;
    }

    if (V4L2_PIX_FMT_MJPEG == pUVCDev->iFormat)
    {
        pContext->mUVCConfig.eEncoderType = PT_MJPEG;
    }
    else if (V4L2_PIX_FMT_YUYV == pUVCDev->iFormat)
    {
        pContext->mUVCConfig.eEncoderType = PT_BUTT;
    }
    else if (V4L2_PIX_FMT_H264 == pUVCDev->iFormat)
    {
        pContext->mUVCConfig.eEncoderType = PT_H264;
    }
    pContext->mUVCConfig.iCapWidth = pUVCDev->iWidth;
    pContext->mUVCConfig.iCapHeight = pUVCDev->iHeight;
    pContext->mUVCConfig.iEncWidth = pUVCDev->iWidth;
    pContext->mUVCConfig.iEncHeight = pUVCDev->iHeight;

    alogd("capture size[%dx%d], encode size[%dx%d]", \
        pContext->mUVCConfig.iCapWidth, pContext->mUVCConfig.iCapHeight, \
        pContext->mUVCConfig.iEncWidth, pContext->mUVCConfig.iEncHeight);

    eRet = createVipp(&pContext->mUVCInfo, &pContext->mUVCConfig);
    if (eRet)
    {
        aloge("fatal error! create vipp fail!");
        goto _sys_exit;
    }

    if (PT_BUTT != pContext->mUVCConfig.eEncoderType)
    {
        eRet = createVeChn(&pContext->mUVCInfo, &pContext->mUVCConfig);
        if (eRet)
        {
            aloge("fatal error! create ve chn fail!");
            goto _destroy_vipp;
        }

        MPP_CHN_S VIChn = {MOD_ID_VIU, pContext->mUVCInfo.iVippDev, pContext->mUVCInfo.iVippChn};
        MPP_CHN_S VEChn = {MOD_ID_VENC, 0, pContext->mUVCInfo.iVencChn};
        eRet = AW_MPI_SYS_Bind(&VIChn, &VEChn);
        if (SUCCESS != eRet)
        {
            aloge("fatal error1 bind vi and ve fail!");
            goto _destroy_ve;
        }
    }
    else
    {
        pContext->mUVCInfo.iVencChn = MM_INVALID_CHN;
        ret = OpenG2d(&pContext->mUVCInfo, &pContext->mUVCConfig);
        if (ret)
        {
            aloge("fatal error! open g2d device fail!");
            goto _destroy_ve;
        }
    }
    alogd("initialize vi and venc success, videv[%d],vichn[%d],vencchn[%d],ispdev[%d]\n",
        pContext->mUVCInfo.iVippDev, pContext->mUVCInfo.iVippChn, \
        pContext->mUVCInfo.iVencChn, pContext->mUVCInfo.iIspDev);

    return 0;
_destroy_ve:
    if (MM_INVALID_CHN != pContext->mUVCInfo.iVencChn)
    {
        AW_MPI_VENC_DestroyChn(pContext->mUVCInfo.iVencChn);
        pContext->mUVCInfo.iVencChn = MM_INVALID_CHN;
    }
_destroy_vipp:
    if (MM_INVALID_CHN != pContext->mUVCInfo.iVippChn)
    {
        AW_MPI_VI_DestroyVirChn(pContext->mUVCInfo.iVippDev, pContext->mUVCInfo.iVippChn);
        pContext->mUVCInfo.iVippChn = MM_INVALID_CHN;
    }
    if (MM_INVALID_DEV != pContext->mUVCInfo.iVippDev)
    {
        AW_MPI_VI_DestroyVipp(pContext->mUVCInfo.iVippDev);
        pContext->mUVCInfo.iVippDev = MM_INVALID_DEV;
    }
_sys_exit:
    AW_MPI_SYS_Exit();
_exit:
    return eRet;
}

static int DestroyCaptureThread(SampleUVCContext *pContext)
{
    if (pContext->mUVCInfo.mG2dFd >= 0)
    {
        CloseG2d(&pContext->mUVCInfo);
    }
    if (MM_INVALID_CHN != pContext->mUVCInfo.iVencChn)
    {
        AW_MPI_VENC_DestroyChn(pContext->mUVCInfo.iVencChn);
        pContext->mUVCInfo.iVencChn = MM_INVALID_CHN;
    }
    if (MM_INVALID_CHN != pContext->mUVCInfo.iVippChn)
    {
        AW_MPI_VI_DestroyVirChn(pContext->mUVCInfo.iVippDev, pContext->mUVCInfo.iVippChn);
        pContext->mUVCInfo.iVippChn = MM_INVALID_CHN;
    }
    if (MM_INVALID_DEV != pContext->mUVCInfo.iVippDev)
    {
        AW_MPI_VI_DisableVipp(pContext->mUVCInfo.iVippDev);
        #if ISP_RUN
        AW_MPI_ISP_Stop(pContext->mUVCInfo.iIspDev);
        #endif
        AW_MPI_VI_DestroyVipp(pContext->mUVCInfo.iVippDev);
        pContext->mUVCInfo.iVippDev = MM_INVALID_DEV;
        pContext->mUVCInfo.iIspDev = MM_INVALID_DEV;
    }
    AW_MPI_SYS_Exit();
    return 0;
}

static int StartCapture(SampleUVCContext *pContext)
{
    int iRet = 0;
    ERRORTYPE eRet = SUCCESS;
    alogd("vir chn is %d", pContext->mUVCInfo.iVippChn);

    if (MM_INVALID_CHN != pContext->mUVCInfo.iVippChn)
    {
        eRet = AW_MPI_VI_EnableVirChn(pContext->mUVCInfo.iVippDev, pContext->mUVCInfo.iVippChn);
        if (SUCCESS  != eRet)
        {
            aloge("fatal error! vipp[%d] enable vir chn[%d]", \
                pContext->mUVCInfo.iVippDev, pContext->mUVCInfo.iVippChn);
            return eRet;
        }
    }
    if (MM_INVALID_CHN != pContext->mUVCInfo.iVencChn)
    {
        eRet = AW_MPI_VENC_StartRecvPic(pContext->mUVCInfo.iVencChn);
        if (SUCCESS != eRet)
        {
            aloge("fatal error! ve chn[%d] start recive picture fail!", \
                pContext->mUVCInfo.iVencChn);
            return eRet;
        }
    }
    pContext->mUVCInfo.mbCapExitFlag = FALSE;
    iRet = pthread_create(&pContext->mUVCInfo.mCaptureTrd, NULL, CaptureThread, pContext);
    if (iRet < 0) {
        aloge("caeate GetVideoFrameThread failed!!\n");
        DestroyCaptureThread(pContext);
        return iRet;
    }

    return iRet;
}

static int StopCapture(SampleUVCContext *pContext)
{
    if (pContext->mUVCInfo.mbCapRunningFlag)
    {
        pContext->mUVCInfo.mbCapExitFlag = TRUE;
        pContext->mUVCInfo.mbCapRunningFlag = FALSE;
        pthread_join(pContext->mUVCInfo.mCaptureTrd, NULL);
    }
    if (MM_INVALID_CHN != pContext->mUVCInfo.iVencChn)
    {
        AW_MPI_VENC_StopRecvPic(pContext->mUVCInfo.iVencChn);
    }
    if (MM_INVALID_CHN != pContext->mUVCInfo.iVippChn)
    {
        AW_MPI_VI_DisableVirChn(pContext->mUVCInfo.iVippDev, pContext->mUVCInfo.iVippChn);
    }
    if (MM_INVALID_CHN != pContext->mUVCInfo.iVencChn)
    {
        AW_MPI_VENC_ResetChn(pContext->mUVCInfo.iVencChn);
        AW_MPI_VENC_DestroyChn(pContext->mUVCInfo.iVencChn);
    }

    return 0;
}

static int UVCVideoSetFormat(SampleUVCDevice *pstUVCDev)
{
    int iRet;
    struct v4l2_format stFormat;
    iRet = ioctl(pstUVCDev->iFd, VIDIOC_G_FMT, &stFormat);
    alogd("width=[%d],height=[%d],sizeimage=[%d], format[%d]\n",
        stFormat.fmt.pix.width, stFormat.fmt.pix.height, stFormat.fmt.pix.sizeimage, stFormat.fmt.pix.pixelformat);

    alogd("iWidth=[%d],iHeight=[%d],iFormat=[0x%08x]\n",
        pstUVCDev->iWidth, pstUVCDev->iHeight, pstUVCDev->iFormat);

    memset(&stFormat, 0, sizeof(struct v4l2_format));
    stFormat.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    stFormat.fmt.pix.pixelformat = pstUVCDev->iFormat;
    stFormat.fmt.pix.width       = pstUVCDev->iWidth;
    stFormat.fmt.pix.height      = pstUVCDev->iHeight;
    stFormat.fmt.pix.field       = V4L2_FIELD_NONE;
    stFormat.fmt.pix.sizeimage   = pstUVCDev->iWidth * pstUVCDev->iHeight * 2;
    alogd("VIDIOC_S_FMT size[%dx%d] fmt[%d] mjpeg[%d] nv21[%d] yuyv[%d]", \
        stFormat.fmt.pix.width, stFormat.fmt.pix.height, stFormat.fmt.pix.pixelformat, \
        V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_NV21, V4L2_PIX_FMT_YUYV);
    iRet = ioctl(pstUVCDev->iFd, VIDIOC_S_FMT, &stFormat);
    if (iRet < 0) {
        aloge("VIDIOC_S_FMT failed!! %s (%d).\n", strerror(errno), errno);
    }

    iRet = ioctl(pstUVCDev->iFd, VIDIOC_G_FMT, &stFormat);
    alogd("width=[%d],height=[%d],sizeimage=[%d], format[%d]\n",
        stFormat.fmt.pix.width, stFormat.fmt.pix.height, stFormat.fmt.pix.sizeimage, stFormat.fmt.pix.pixelformat);

    return iRet;
}

static void UVCFillStreamingControl(SampleUVCDevice *pstUVCDev, struct uvc_streaming_control *pstCtrl, int iFmtIndex, int iFrmIndex)
{
    SampleUVCFormat *pFormat = &gFormat[iFmtIndex][iFrmIndex];

    /* 0: interval fixed
     * 1: keyframe rate fixed
     * 2: Pframe rate fixed
     */
    pstCtrl->bmHint = 0;
    pstCtrl->bFormatIndex    = iFmtIndex + 1;
    pstCtrl->bFrameIndex     = iFrmIndex + 1;
    pstCtrl->dwFrameInterval = pFormat->iInterval;
    pstCtrl->wDelay = 0;
    pstCtrl->dwMaxVideoFrameSize = pFormat->iWidth * pFormat->iHeight;
    pstCtrl->dwMaxPayloadTransferSize = pFormat->iWidth * pFormat->iHeight;
//  pstCtrl->dwClockFrequency    = ;
    pstCtrl->bmFramingInfo = 3; //ignore in JPEG or MJPEG format
    pstCtrl->bPreferedVersion = 1;
    pstCtrl->bMinVersion = 1;
    pstCtrl->bMaxVersion = 1;
}

static void SubscribeUVCEvent(struct SampleUVCDevice *pstUVCDev)
{
    struct v4l2_event_subscription stSub;
#if 1
    UVCFillStreamingControl(pstUVCDev, &pstUVCDev->stProbe, 0, 0);
    UVCFillStreamingControl(pstUVCDev, &pstUVCDev->stCommit, 0, 0);
#endif
    /* subscribe events, for debug, subscribe all events */
    memset(&stSub, 0, sizeof stSub);
    stSub.type = UVC_EVENT_FIRST;
    ioctl(pstUVCDev->iFd, VIDIOC_SUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_CONNECT;
    ioctl(pstUVCDev->iFd, VIDIOC_SUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_DISCONNECT;
    ioctl(pstUVCDev->iFd, VIDIOC_SUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_STREAMON;
    ioctl(pstUVCDev->iFd, VIDIOC_SUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_STREAMOFF;
    ioctl(pstUVCDev->iFd, VIDIOC_SUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_SETUP;
    ioctl(pstUVCDev->iFd, VIDIOC_SUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_DATA;
    ioctl(pstUVCDev->iFd, VIDIOC_SUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_LAST;
    ioctl(pstUVCDev->iFd, VIDIOC_SUBSCRIBE_EVENT, &stSub);
}

static void UnSubscribeUVCEvent(struct SampleUVCDevice *pstUVCDev)
{
    struct v4l2_event_subscription stSub;
#if 0
    uvc_fill_streaming_control(pstUVCDev, &pstUVCDev->probe, 0, 0);
    uvc_fill_streaming_control(pstUVCDev, &pstUVCDev->commit, 0, 0);
#endif
    /* subscribe events, for debug, subscribe all events */
    memset(&stSub, 0, sizeof stSub);
    stSub.type = UVC_EVENT_FIRST;
    ioctl(pstUVCDev->iFd, VIDIOC_UNSUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_CONNECT;
    ioctl(pstUVCDev->iFd, VIDIOC_UNSUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_DISCONNECT;
    ioctl(pstUVCDev->iFd, VIDIOC_UNSUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_STREAMON;
    ioctl(pstUVCDev->iFd, VIDIOC_UNSUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_STREAMOFF;
    ioctl(pstUVCDev->iFd, VIDIOC_UNSUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_SETUP;
    ioctl(pstUVCDev->iFd, VIDIOC_UNSUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_DATA;
    ioctl(pstUVCDev->iFd, VIDIOC_UNSUBSCRIBE_EVENT, &stSub);
    stSub.type = UVC_EVENT_LAST;
    ioctl(pstUVCDev->iFd, VIDIOC_UNSUBSCRIBE_EVENT, &stSub);
}

static int DoUVCEventSetupClassStreaming(SampleUVCDevice *pstUVCDev, struct uvc_event *pstEvent, struct uvc_request_data *pstReq)
{
    struct uvc_streaming_control *pstCtrl;
    uint8_t ucReq = pstEvent->req.bRequest;
    uint8_t ucCtrlSet = pstEvent->req.wValue >> 8;

    alogd("streaming request (req %s cs %02x)\n",
        ucReq == UVC_SET_CUR?"SET_CUR":\
        ucReq == UVC_GET_CUR?"GET_CUR":\
        ucReq == UVC_GET_MIN?"GET_MIN":\
        ucReq == UVC_GET_MAX?"GET_MAX":\
        ucReq == UVC_GET_DEF?"GET_DEF":\
        ucReq == UVC_GET_RES?"GET_RES":\
        ucReq == UVC_GET_LEN?"GET_LEN":\
        ucReq == UVC_GET_INFO?"GET_INFO":"NULL",
        ucCtrlSet == UVC_VS_PROBE_CONTROL?"probe":\
        ucCtrlSet == UVC_VS_COMMIT_CONTROL?"commit":"unknown"
        );

    if (ucCtrlSet != UVC_VS_PROBE_CONTROL && ucCtrlSet != UVC_VS_COMMIT_CONTROL)
        return 0;

    pstCtrl = (struct uvc_streaming_control *)&pstReq->data[0];
    pstReq->length = sizeof(struct uvc_streaming_control);

    switch (ucReq) {
    case UVC_SET_CUR:
        pstUVCDev->iCtrlSetCur = ucCtrlSet;
        pstReq->length = 34;
        break;

    case UVC_GET_CUR:
//        alogd("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$getcur\n");
        if (ucCtrlSet == UVC_VS_PROBE_CONTROL)
            memcpy(pstCtrl, &pstUVCDev->stProbe, sizeof(struct uvc_streaming_control));
        else
            memcpy(pstCtrl, &pstUVCDev->stCommit, sizeof(struct uvc_streaming_control));
        break;

    case UVC_GET_MIN:
    case UVC_GET_MAX:
    case UVC_GET_DEF:
//        alogd("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$getdef\n");
        UVCFillStreamingControl(pstUVCDev, pstCtrl, 0, 0);
        break;

    case UVC_GET_RES:
//        alogd("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$get res\n");
        memset(pstCtrl, 0, sizeof(struct uvc_streaming_control));
        break;

    case UVC_GET_LEN:
//        alogd("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$get len\n");
        pstReq->data[0] = 0x00;
        pstReq->data[1] = 0x22;
        pstReq->length = 2;
        break;

    case UVC_GET_INFO:
//        alogd("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$get info\n");
        pstReq->data[0] = 0x03;
        pstReq->length = 1;
        break;
    }

    return 0;
}

static int DoUVCEventSetupClass(SampleUVCDevice *pstUVCDev, struct uvc_event *pstEvent, struct uvc_request_data *pstReq)
{
    if ((pstEvent->req.bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
        return 0;

    switch (pstEvent->req.wIndex & 0xff) {
    case UVC_INTF_CONTROL:
//      alogd("*******************************intfctonrol\n");
//        uvc_events_process_control(dev, ctrl->bRequest, ctrl->wValue >> 8, resp);
        break;

    case UVC_INTF_STREAMING:
//        alogd("*******************************intfstreaming\n");
        DoUVCEventSetupClassStreaming(pstUVCDev, pstEvent, pstReq);
        break;

    default:
        break;
    }

    return 0;
}

static int DoUVCEventSetup(SampleUVCDevice *pstUVCDev, struct uvc_event *pstEvent, struct uvc_request_data *pstReq)
{
    switch(pstEvent->req.bRequestType & USB_TYPE_MASK) {
        /* USB_TYPE_STANDARD: kernel driver will process it */
        case USB_TYPE_STANDARD:
        case USB_TYPE_VENDOR:
            aloge("do not care\n");
            break;
        case USB_TYPE_CLASS:
            DoUVCEventSetupClass(pstUVCDev, pstEvent, pstReq);
            break;
        default: break;
    }

    return 0;
}

static int DoUVDReqReleaseBufs(SampleUVCDevice *pstUVCDev,  int iBufsNum);
static int DoUVCStreamOnOff(SampleUVCDevice *pstUVCDev, int iOn);
static int DoUVCEventData(SampleUVCDevice *pstUVCDev, struct uvc_request_data *pstReq)
{
    struct uvc_streaming_control *pstTarget;

    switch(pstUVCDev->iCtrlSetCur) {
        case UVC_VS_PROBE_CONTROL:
//            alogd("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&probecontrol\n");
            printf("setting probe control, length = %d\n", pstReq->length);
            pstTarget = &pstUVCDev->stProbe;
            break;

        case UVC_VS_COMMIT_CONTROL:
//            alogd("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&commitcontrol\n");
            printf("setting commit control, length = %d\n", pstReq->length);
            pstTarget = &pstUVCDev->stCommit;
            break;

        default:
            printf("setting unknown control, length = %d\n", pstReq->length);
            return 0;
    }

    struct uvc_streaming_control *pstCtrl;
    pstCtrl = (struct uvc_streaming_control*)&pstReq->data[0];

    if (pstUVCDev->bIsBulkMode) {
	/* Notice: dwMaxPayloadTransferSize can't be 0 */
        pstTarget->bFormatIndex = pstCtrl->bFormatIndex;
        pstTarget->bFrameIndex = pstCtrl->bFrameIndex;
        /* TODO: select dwMaxVideoFrameSize, dwMaxPayloadTransferSize according bFormatIndex, bFrameIndex */
        pstTarget->dwFrameInterval = pstCtrl->dwFrameInterval;
    } else {
        memcpy(pstTarget, pstCtrl, sizeof(struct uvc_streaming_control));
    }

    alogd("pstCtrl->bmHint[%d],pstCtrl->bFormatIndex[%d],pstCtrl->bFrameIndex[%d],pstCtrl->dwFrameInterval[%d],\
        pstCtrl->dwMaxVideoFrameSize[%d],pstCtrl->dwMaxPayloadTransferSize[%d]",
        pstCtrl->bmHint, pstCtrl->bFormatIndex, pstCtrl->bFrameIndex, pstCtrl->dwFrameInterval,
        pstCtrl->dwMaxVideoFrameSize, pstCtrl->dwMaxPayloadTransferSize);
    alogd("MPJPEG[%d], JPEG[%d], YUYV[%d]", V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_JPEG, V4L2_PIX_FMT_YUYV);

    if (pstUVCDev->iCtrlSetCur == UVC_VS_COMMIT_CONTROL) {
        if (pstCtrl->bFormatIndex > MAX_FORMAT_SUPPORT_NUM || \
            pstCtrl->bFrameIndex > MAX_FRAME_SUPPORT_NUM)
        {
            alogw("unsupport format index[%d] frame index[%d], use default format index[1] frame index[1]", \
                pstCtrl->bFormatIndex, pstCtrl->bFrameIndex);
            pstCtrl->bFormatIndex = 1;
            pstCtrl->bFrameIndex = 1;
        }

        SampleUVCFormat *pFormat = &gFormat[pstCtrl->bFormatIndex - 1][pstCtrl->bFrameIndex - 1];
        pstUVCDev->iWidth  = pFormat->iWidth;
        pstUVCDev->iHeight = pFormat->iHeight;
        pstUVCDev->iFormat = pFormat->iFormat;
        UVCVideoSetFormat(pstUVCDev);
        if (pstUVCDev->bIsBulkMode) {
            /* bulk mode, stream on after commit */
	    if (pstUVCDev->bIsStreaming) {
	        DoUVDReqReleaseBufs(pstUVCDev, 0);
		    DoUVCStreamOnOff(pstUVCDev, 0);
	    }
            DoUVDReqReleaseBufs(pstUVCDev, 5);
            DoUVCStreamOnOff(pstUVCDev, 1);
        }
    }

    return 0;
}

static int DoUVDReqReleaseBufs(SampleUVCDevice *pstUVCDev,  int iBufsNum)
{
    int iRet;

    if (iBufsNum > 0) {
        struct v4l2_requestbuffers stReqBufs;
        memset(&stReqBufs, 0, sizeof(struct v4l2_requestbuffers));
        stReqBufs.count  = iBufsNum;
        stReqBufs.memory = V4L2_MEMORY_MMAP;
        stReqBufs.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        iRet = ioctl(pstUVCDev->iFd, VIDIOC_REQBUFS, &stReqBufs);
        if (iRet < 0) {
            aloge("VIDIOC_REQBUFS failed!!\n");
            goto reqbufs_err;
        }

        pstUVCDev->pstFrames = malloc(iBufsNum * sizeof(SampleUVCFrame));

        for (int i = 0; i < iBufsNum; i++) {
            struct v4l2_buffer stBuffer;
            memset(&stBuffer, 0, sizeof(struct v4l2_buffer));
            stBuffer.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            stBuffer.memory = V4L2_MEMORY_MMAP;
            stBuffer.index  = i;
            iRet = ioctl(pstUVCDev->iFd, VIDIOC_QUERYBUF, &stBuffer);
            if (iRet < 0) {
                aloge("VIDIOC_QUERYBUF failed!!\n");
            }

            pstUVCDev->pstFrames[i].pVirAddr = mmap(0, stBuffer.length,
                PROT_READ | PROT_WRITE, MAP_SHARED, pstUVCDev->iFd, stBuffer.m.offset);
            pstUVCDev->pstFrames[i].iBufLen  = stBuffer.length;

            ioctl(pstUVCDev->iFd, VIDIOC_QBUF, &stBuffer);
        }

        pstUVCDev->iBufsNum = stReqBufs.count;
        alogd("request [%d] buffers success\n", pstUVCDev->iBufsNum);
    } else {
        for (int i = 0; i < pstUVCDev->iBufsNum; i++) {
            munmap(pstUVCDev->pstFrames[i].pVirAddr, pstUVCDev->pstFrames[i].iBufLen);
        }

        free(pstUVCDev->pstFrames);
    }
reqbufs_err:
    return iRet;
}

static int DoUVCStreamOnOff(SampleUVCDevice *pstUVCDev, int iOn)
{
    int iRet;
    enum v4l2_buf_type stBufType;
    stBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    SampleUVCContext *pContext = (SampleUVCContext *)pstUVCDev->pPrivite;

    if (iOn) {
        iRet = InitFrameList(5, pContext);
        if (iRet)
        {
            aloge("fatal error! init frame list fail!");
            goto _destroy;
        }
        iRet = CreateCaptureThread(pContext);
        alogd("vir chn is %d", pContext->mUVCInfo.iVippChn);
        iRet = StartCapture(pContext);
        if (iRet)
        {
            aloge("fatal error! start capture fail!");
            goto _stop_capture;
        }
        iRet = ioctl(pstUVCDev->iFd, VIDIOC_STREAMON, &stBufType);
        if (iRet == 0) {
            alogd("begin to streaming\n");
            pstUVCDev->bIsStreaming = 1;
        }
    } else {
        iRet = ioctl(pstUVCDev->iFd, VIDIOC_STREAMOFF, &stBufType);
        if (iRet == 0) {
            alogd("stop to streaming\n");
            pstUVCDev->bIsStreaming = 0;
        }
        StopCapture(pContext);
        DestroyCaptureThread(pContext);
        DeinitFrameList(pContext);
    }
    return iRet;
_stop_capture:
    StopCapture(pContext);
_destroy:
    DestroyCaptureThread(pContext);
_deinit_frame_list:
    DeinitFrameList(pContext);
    return iRet;
}

static int DoUVCEventProcess(SampleUVCDevice *pstUVCDev)
{
    int iRet;
    struct v4l2_event stEvent;
    struct uvc_event *pstUVCEvent = (struct uvc_event*)&stEvent.u.data[0];
    struct uvc_request_data stUVCReq;
    memset(&stUVCReq, 0, sizeof(struct uvc_request_data));
    stUVCReq.length = -EL2HLT;

    iRet = ioctl(pstUVCDev->iFd, VIDIOC_DQEVENT, &stEvent);
    if (iRet < 0) {
        goto qevent_err;
    }

    alogd("event is %s\n",
        stEvent.type == UVC_EVENT_CONNECT?"first":\
        stEvent.type == UVC_EVENT_DISCONNECT?"disconnect":\
        stEvent.type == UVC_EVENT_STREAMON?"stream on":\
        stEvent.type == UVC_EVENT_STREAMOFF?"stream off":\
        stEvent.type == UVC_EVENT_SETUP?"setup":\
        stEvent.type == UVC_EVENT_DATA?"data":"NULL"
        );

    switch (stEvent.type) {
        case UVC_EVENT_CONNECT:
            /*alogd("uvc event first.\n");*/
            break;
        case UVC_EVENT_DISCONNECT:
            /*alogd("uvc event disconnect.\n");*/
            break;
        case UVC_EVENT_STREAMON:
            /*alogd("uvc event stream on.\n");*/
            DoUVDReqReleaseBufs(pstUVCDev, 5);
            DoUVCStreamOnOff(pstUVCDev, 1);
            goto qevent_err;
        case UVC_EVENT_STREAMOFF:
            /*alogd("uvc event stream off.\n");*/
            DoUVDReqReleaseBufs(pstUVCDev, 0);
            DoUVCStreamOnOff(pstUVCDev, 0);
            goto qevent_err;
        case UVC_EVENT_SETUP:
            /*alogd("uvc event setup.\n");*/
            DoUVCEventSetup(pstUVCDev, pstUVCEvent, &stUVCReq);
            break;
        case UVC_EVENT_DATA:
            DoUVCEventData(pstUVCDev, &pstUVCEvent->data);
            /*alogd("uvc event data.\n");*/
            break;

        default: break;
    }

    ioctl(pstUVCDev->iFd, UVCIOC_SEND_RESPONSE, &stUVCReq);
qevent_err:
    return iRet;
}

static void usage(const char *argv0)
{
    printf(
        "\033[33m"
        "exec [-h|--help] [-p|--path]\n"
        "   <-h|--help>: print the help information\n"
        "   <-p|--path>       <args>: point to the configuration file path\n"
        "   <-x|--width>      <args>: set video picture width\n"
        "   <-y|--height>     <args>: set video picture height\n"
        "   <-f|--framerate>  <args>: set the video frame rate\n"
        "   <-b|--bulk>       <args>: Use bulk mode or not[0|1]\n"
        "   <-d|--device>     <args>: uvc video device number[0-3]\n"
        "   <-i|--image>      <args>: MJPEG image\n"
        "\033[0m\n");
}

static ERRORTYPE LoadSampleUVCConfig(SampleUVCConfig *pConfig, const char *conf_path)
{
    int iRet;
    CONFPARSER_S stConfParser;

    pConfig->iCapFrameRate = 30;
    pConfig->eCapFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pConfig->iEncFrameRate = 30;
    pConfig->iEncBitRate  = 4194304;
    pConfig->iEncQuality = 99;
    iRet = createConfParser(conf_path, &stConfParser);
    if(iRet < 0)
    {
        alogd("user not set config file. use default test parameter!");
        pConfig->iUVCDev = 2;
        pConfig->iCapDev = 1;
        pConfig->eCapFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        pConfig->iEncBitRate  = 4194304;
        pConfig->iEncQuality = 99;
        goto use_default_conf;
    }

    pConfig->iUVCDev = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_UVC_DEVICE, 0);
    pConfig->iCapDev = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_VIN_DEVICE, 0);
    pConfig->iEncBitRate = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_ENC_BITRATE, 0);
use_default_conf:
    alogd("UVCDev=%d,CapDev=%d,CapWidth=%d,CapHeight=%d,CapFrmRate=%d,EncBitRate=%d,\
        EncWidth=%d,EncHeight=%d,EncFrmRate=%d,quality=%d\n",
        pConfig->iUVCDev, pConfig->iCapDev, pConfig->iCapWidth, pConfig->iCapHeight, pConfig->iCapFrameRate,
        pConfig->iEncBitRate, pConfig->iEncWidth, pConfig->iEncHeight, pConfig->iCapFrameRate, pConfig->iEncQuality);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

static struct option pstLongOptions[] = {
   {"help",        no_argument,       0, 'h'},
   {"bulk",        required_argument, 0, 'b'},
   {"path",        required_argument, 0, 'p'},
   {"width",       required_argument, 0, 'x'},
   {"height",      required_argument, 0, 'y'},
   {"framerate",   required_argument, 0, 'f'},
   {"device",      required_argument, 0, 'd'},
   {0,             0,                 0,  0 }
};

static int ParseCmdLine(int argc, char **argv, SampleUVCContext *pCmdLinePara)
{
    int mRet;
    int iOptIndex = 0;

    memset(pCmdLinePara, 0, sizeof(SampleUVCContext));
    pCmdLinePara->mCmdLinePara.mConfigFilePath[0] = 0;
    while (1) {
        mRet = getopt_long(argc, argv, ":p:b:x:y:f:d:h", pstLongOptions, &iOptIndex);
        if (mRet == -1) {
            break;
        }

        switch (mRet) {
            /* let the "sampleXXX -path sampleXXX.conf" command to be compatible with
             * "sampleXXX -p sampleXXX.conf"
             */
            case 'p':
                if (strcmp("ath", optarg) == 0) {
                    if (NULL == argv[optind]) {
                        usage(argv[0]);
                        goto opt_need_arg;
                    }
                    alogd("path is [%s]\n", argv[optind]);
                    strncpy(pCmdLinePara->mCmdLinePara.mConfigFilePath, argv[optind], sizeof(pCmdLinePara->mCmdLinePara.mConfigFilePath));
                } else {
                    alogd("path is [%s]\n", optarg);
                    strncpy(pCmdLinePara->mCmdLinePara.mConfigFilePath, optarg, sizeof(pCmdLinePara->mCmdLinePara.mConfigFilePath));
                }
                break;
            case 'x':
                alogd("width is [%d]\n", atoi(optarg));
                pCmdLinePara->mUVCConfig.iCapWidth = atoi(optarg);
                break;
            case 'y':
                alogd("height is [%d]\n", atoi(optarg));
                pCmdLinePara->mUVCConfig.iCapHeight = atoi(optarg);
                break;
            case 'f':
                alogd("frame rate is [%d]\n", atoi(optarg));
                pCmdLinePara->mUVCConfig.iCapFrameRate = atoi(optarg);
                break;
            case 'b':
                alogd("bulk mode is [%d]\n", atoi(optarg));
                pCmdLinePara->iBulkMode = atoi(optarg);
                break;
            case 'd':
                alogd("device is [%d]\n", atoi(optarg));
                pCmdLinePara->mUVCConfig.iUVCDev = atoi(optarg);
                break;
            case 'h':
                usage(argv[0]);
                goto print_help_exit;
                break;
            case ':':
                aloge("option \"%s\" need <arg>\n", argv[optind - 1]);
                goto opt_need_arg;
                break;
            case '?':
                if (optind > 2) {
                    break;
                }
                aloge("unknow option \"%s\"\n", argv[optind - 1]);
                usage(argv[0]);
                goto unknow_option;
                break;
            default:
                printf("?? why getopt_long returned character code 0%o ??\n", mRet);
                break;
        }
    }

    return 0;
opt_need_arg:
unknow_option:
print_help_exit:
    return -1;
}

void SignalHandle(int iArg)
{
    alogd("receive exit signal. \n");
    g_bSampleExit = 1;
}

static void InitContext(SampleUVCContext *pContext)
{
    pContext->mUVCInfo.iVippDev = MM_INVALID_DEV;
    pContext->mUVCInfo.iVencChn = MM_INVALID_CHN;
    pContext->mUVCInfo.iVencChn = MM_INVALID_CHN;
    pContext->mUVCInfo.iIspDev  = MM_INVALID_DEV;
    pContext->mUVCInfo.mG2dFd = -1;
}

int main(int argc, char *argv[])
{
    int iRet;

    alogd("sample_uvcout running!\n");
    SampleUVCContext stContext;
    memset(&stContext, 0, sizeof(SampleUVCContext));
    InitContext(&stContext);

    iRet = ParseCmdLine(argc, argv, &stContext);
    if (iRet < 0) {
        aloge("parse cmdline error.");
        return -1;
    }

    /* parse config file. */
    if(LoadSampleUVCConfig(&stContext.mUVCConfig , stContext.mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        iRet = -1;
        goto load_conf_err;
    }

    SampleUVCDevice *pstUVCDev = &stContext.mUVCDev;
    pstUVCDev->iDev = stContext.mUVCConfig.iUVCDev;
    pstUVCDev->pPrivite = (void *)&stContext;
    pstUVCDev->bIsBulkMode = stContext.iBulkMode > 0 ? 1 : 0;
    iRet = OpenUVCDevice(pstUVCDev);
    if (iRet < 0) {
        aloge("open uvc video device failed!!\n");
        iRet = -1;
        goto uvc_open_err;
    }
    pstUVCDev->bIsStreaming = 0;

    SubscribeUVCEvent(pstUVCDev);
    signal(SIGINT, SignalHandle);
    fd_set stFdSet;
    FD_ZERO(&stFdSet);
    FD_SET(pstUVCDev->iFd, &stFdSet);

    while (1) {
        fd_set stErSet = stFdSet;
        fd_set stWrSet = stFdSet;
        struct timeval stTimeVal;
        /* Wait up to five seconds. */
        stTimeVal.tv_sec = 0;
        stTimeVal.tv_usec = 10*1000;

        if (g_bSampleExit) {
            alogd("sample uvcout exit!");
            break;
        }

        iRet = select(pstUVCDev->iFd + 1, NULL, &stWrSet, &stErSet, &stTimeVal);
        if (FD_ISSET(pstUVCDev->iFd, &stErSet))
            DoUVCEventProcess(pstUVCDev);
        if (FD_ISSET(pstUVCDev->iFd, &stWrSet) && pstUVCDev->bIsStreaming) {
            DoUVCVideoBufProcess(pstUVCDev);
        }
    }

    if (pstUVCDev->bIsStreaming)
    {
        DoUVCStreamOnOff(pstUVCDev, 0);
    }
init_frm_err:
    UnSubscribeUVCEvent(pstUVCDev);
    CloseUVCDevice(pstUVCDev);
uvc_open_err:
load_conf_err:
    return iRet;
}
