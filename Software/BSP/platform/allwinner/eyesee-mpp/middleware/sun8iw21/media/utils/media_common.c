/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : media_common.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/03/15
  Last Modified :
  Description   : multimedia common function for internal use.
  Function List :
  History       :
******************************************************************************/
// ref platform headers
#include <linux/videodev2.h>
#include <string.h>
#include <utils/plat_log.h>
#include "cdx_list.h"
#include "plat_defines.h"
#include "plat_errno.h"
#include "plat_math.h"
#include "plat_type.h"

// media api headers to app
#include "mm_comm_demux.h"
#include "mm_comm_venc.h"
#include "mm_comm_aenc.h"
#include "mm_comm_adec.h"
#include "mm_comm_video.h"
#include "mm_comm_uvc.h"
#include "mm_comm_aio.h"
#include "mm_common.h"

// media internal common headers.
#include "mm_component.h"

#include <media_common.h>
#include <media_common_vcodec.h>
#include <media_common_acodec.h>

ERRORTYPE copy_MPP_CHN_S(MPP_CHN_S *pDst, MPP_CHN_S *pSrc)
{
    *pDst = *pSrc;
    return SUCCESS;
}

VENC_CODEC_TYPE map_PAYLOAD_TYPE_E_to_VENC_CODEC_TYPE(PAYLOAD_TYPE_E nPayLoadType)
{
    VENC_CODEC_TYPE nVenclibType;
    switch (nPayLoadType) {
        case PT_H264:
            nVenclibType = VENC_CODEC_H264;
            break;
        case PT_H265:
            nVenclibType = VENC_CODEC_H265;
            break;
        case PT_JPEG:
        case PT_MJPEG:
            nVenclibType = VENC_CODEC_JPEG;
            break;
        default:
            aloge("fatal error! unknown PlayLoadType[%d]", nPayLoadType);
            nVenclibType = VENC_CODEC_H264;
            break;
    }
    return nVenclibType;
}

PAYLOAD_TYPE_E map_VENC_CODEC_TYPE_to_PAYLOAD_TYPE_E(VENC_CODEC_TYPE eVencCodecType)
{
    PAYLOAD_TYPE_E ePayloadType;
    switch (eVencCodecType)
    {
        case VENC_CODEC_H264:
            ePayloadType = PT_H264;
            break;
        case VENC_CODEC_H265:
            ePayloadType = PT_H265;
            break;
        case VENC_CODEC_JPEG:
            ePayloadType = PT_JPEG;
            break;
        default:
            aloge("fatal error! unknown VencCodecType[%d]", eVencCodecType);
            ePayloadType = PT_H264;
            break;
    }
    return ePayloadType;
}

AUDIO_ENCODER_TYPE map_PAYLOAD_TYPE_E_to_AUDIO_ENCODER_TYPE(PAYLOAD_TYPE_E nPayLoadType)
{
    AUDIO_ENCODER_TYPE nAenclibType;
    switch (nPayLoadType) {
        case PT_AAC:
            nAenclibType = AUDIO_ENCODER_AAC_TYPE;
            break;
        case PT_LPCM:
            nAenclibType = AUDIO_ENCODER_LPCM_TYPE;
            break;
        case PT_PCM_AUDIO:
            nAenclibType = AUDIO_ENCODER_PCM_TYPE;
            break;
        case PT_MP3:
            nAenclibType = AUDIO_ENCODER_MP3_TYPE;
            break;
        default:
            aloge("fatal error! unknown PlayLoadType[%d]", nPayLoadType);
            nAenclibType = AUDIO_ENCODER_AAC_TYPE;
            break;
    }
    return nAenclibType;
}

PAYLOAD_TYPE_E map_AUDIO_ENCODER_TYPE_to_PAYLOAD_TYPE_E(AUDIO_ENCODER_TYPE eAencCodecType)
{
    PAYLOAD_TYPE_E ePayloadType;
    switch (eAencCodecType)
    {
        case AUDIO_ENCODER_AAC_TYPE:
            ePayloadType = PT_AAC;
            break;
        case AUDIO_ENCODER_LPCM_TYPE:
            ePayloadType = PT_LPCM;
            break;
        case AUDIO_ENCODER_PCM_TYPE:
            ePayloadType = PT_PCM_AUDIO;
            break;
        case AUDIO_ENCODER_MP3_TYPE:
            ePayloadType = PT_MP3;
            break;
        default:
            aloge("fatal error! unknown AencCodecType[%d]", eAencCodecType);
            ePayloadType = PT_AAC;
            break;
    }
    return ePayloadType;
}

enum EVIDEOCODECFORMAT map_PAYLOAD_TYPE_E_to_EVIDEOCODECFORMAT(PAYLOAD_TYPE_E nPayLoadType)
{
    enum EVIDEOCODECFORMAT nVDecLibType;
    switch (nPayLoadType) {
        case PT_H264:
            nVDecLibType = VIDEO_CODEC_FORMAT_H264;
            break;
        case PT_H265:
            nVDecLibType = VIDEO_CODEC_FORMAT_H265;
            break;
        case PT_JPEG:
        case PT_MJPEG:
            nVDecLibType = VIDEO_CODEC_FORMAT_MJPEG;
            break;
        default:
            alogw("fatal error! unsupported format[0x%x]", nPayLoadType);
            nVDecLibType = VIDEO_CODEC_FORMAT_MJPEG;
            break;
    }
    return nVDecLibType;
}

PAYLOAD_TYPE_E map_EVIDEOCODECFORMAT_to_PAYLOAD_TYPE_E(enum EVIDEOCODECFORMAT eCodecFormat)
{
    PAYLOAD_TYPE_E dstType;
    switch (eCodecFormat) {
        case VIDEO_CODEC_FORMAT_H264:
            dstType = PT_H264;
            break;
        case VIDEO_CODEC_FORMAT_MJPEG:
            dstType = PT_MJPEG;
            break;
        case VIDEO_CODEC_FORMAT_H265:
            dstType = PT_H265;
            break;
        default:
            aloge("fatal error! unsupported format[0x%x]", eCodecFormat);
            dstType = PT_MAX;
            break;
    }
    return dstType;
}

PAYLOAD_TYPE_E map_EAUDIOCODECFORMAT_to_PAYLOAD_TYPE_E(enum EAUDIOCODECFORMAT eCodecFormat, int eSubCodecFormat)
{
    PAYLOAD_TYPE_E dstType;
    switch(eCodecFormat)
    {
        case AUDIO_CODEC_FORMAT_MPEG_AAC_LC:
        case AUDIO_CODEC_FORMAT_RAAC:
            dstType = PT_AAC;
            break;
        case AUDIO_CODEC_FORMAT_MP1:
        case AUDIO_CODEC_FORMAT_MP2:
        case AUDIO_CODEC_FORMAT_MP3:
            dstType = PT_MP3;
            break;
        case AUDIO_CODEC_FORMAT_PCM:
        {
            //WAVE_FORMAT_ALAW, ABS_EDIAN_FLAG_BIG, ABS_EDIAN_FLAG_MASK
            //if((tmpAbsFmt->eSubCodecFormat & ABS_EDIAN_FLAG_MASK) == ABS_EDIAN_FLAG_BIG)
            switch(eSubCodecFormat & (~ABS_EDIAN_FLAG_MASK))
            {
                case 0x1:
                {
                    dstType = PT_PCM_AUDIO;
                    break;
                }
                case 0x6:
                {
                    dstType = PT_G711A;
                    break;
                }
                case 0x7:
                {
                    dstType = PT_G711U;
                    break;
                }
                default:
                {
                    aloge("fatal error! unknown subCodecFormat:0x%x for pcm!", eSubCodecFormat);
                    dstType = PT_PCM_AUDIO;
                    break;
                }
            }
            break;
        }
        case AUDIO_CODEC_FORMAT_G711a:
            dstType = PT_G711A;
            break;
        case AUDIO_CODEC_FORMAT_G711u:
            dstType = PT_G711U;
            break;
        case AUDIO_CODEC_FORMAT_G726a:
            dstType = PT_G726;
            break;
        case AUDIO_CODEC_FORMAT_G726u:
            dstType = PT_G726U;
            break;
        default:
            aloge("fatal error! unsupported format[0x%x]", eCodecFormat);
            dstType = PT_MAX;
            break;
    }
    return dstType;
}

enum EAUDIOCODECFORMAT map_PAYLOAD_TYPE_E_to_EAUDIOCODECFORMAT(PAYLOAD_TYPE_E srcFormat)
{
    enum EAUDIOCODECFORMAT dstFormat;
    switch(srcFormat)
    {
        case PT_AAC:
            dstFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
            break;
        case PT_MP3:
            dstFormat = AUDIO_CODEC_FORMAT_MP3;
            break;
        case PT_PCM_AUDIO:
            dstFormat = AUDIO_CODEC_FORMAT_PCM;
            break;
        case PT_ADPCMA:
            dstFormat = AUDIO_CODEC_FORMAT_ADPCM;
            break;
        case PT_G711A:
            dstFormat = AUDIO_CODEC_FORMAT_G711a;
            break;
        case PT_G711U:
            dstFormat = AUDIO_CODEC_FORMAT_G711u;
            break;
        case PT_G726:
            dstFormat = AUDIO_CODEC_FORMAT_G726a;
            break;
        case PT_G726U:
            dstFormat = AUDIO_CODEC_FORMAT_G726u;
            break;
        default:
            dstFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
            alogw("Unsupported audio decoder format [%d]! Set AAC to it!", srcFormat);
            break;
    }

    return dstFormat;
}

unsigned int GetBitRateFromVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pAttr)
{
    unsigned int nBitRate;
    if(PT_JPEG == pAttr->VeAttr.Type)
    {
        return 0;
    }
    switch (pAttr->RcAttr.mRcMode) 
    {
        case VENC_RC_MODE_H264CBR:
            nBitRate = pAttr->RcAttr.mAttrH264Cbr.mBitRate;
            break;
        case VENC_RC_MODE_H264VBR:
            nBitRate = pAttr->RcAttr.mAttrH264Vbr.mMaxBitRate;
            break;
        case VENC_RC_MODE_H264FIXQP:
            nBitRate = 0;
            break;
        case VENC_RC_MODE_H264ABR:
            nBitRate = pAttr->RcAttr.mAttrH264Abr.mMaxBitRate;
            break;
        case VENC_RC_MODE_H264QPMAP:
            nBitRate = pAttr->RcAttr.mAttrH264QpMap.mMaxBitRate;
            break;
        case VENC_RC_MODE_H265CBR:
            nBitRate = pAttr->RcAttr.mAttrH265Cbr.mBitRate;
            break;
        case VENC_RC_MODE_H265VBR:
            nBitRate = pAttr->RcAttr.mAttrH265Vbr.mMaxBitRate;
            break;
        case VENC_RC_MODE_H265FIXQP:
            nBitRate = 0;
            break;
        case VENC_RC_MODE_H265ABR:
            nBitRate = pAttr->RcAttr.mAttrH265Abr.mMaxBitRate;
            break;
        case VENC_RC_MODE_H265QPMAP:
            nBitRate = pAttr->RcAttr.mAttrH265QpMap.mMaxBitRate;
            break;
        case VENC_RC_MODE_MJPEGCBR:
            nBitRate = pAttr->RcAttr.mAttrMjpegeCbr.mBitRate;
            break;
        case VENC_RC_MODE_MJPEGFIXQP:
            nBitRate = 0;
            break;
        default:
            alogw("unsupported temporary: chn attr RcAttr RcMode[0x%x]", pAttr->RcAttr.mRcMode);
            nBitRate = 0;
            break;
    }
    return nBitRate;
}

ERRORTYPE GetEncodeDstSizeFromVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pAttr, SIZE_S *pDstSize)
{
    ERRORTYPE ret = SUCCESS;
    if(NULL == pDstSize || NULL == pAttr)
    {
        alogw("Be careful! null pointer!");
        return FAILURE;
    }
    switch (pAttr->VeAttr.Type)
    {
        case PT_JPEG:
        {
            pDstSize->Width = pAttr->VeAttr.AttrJpeg.PicWidth;
            pDstSize->Height = pAttr->VeAttr.AttrJpeg.PicHeight;
            break;
        }
        case PT_MJPEG:
        {
            pDstSize->Width = pAttr->VeAttr.AttrMjpeg.mPicWidth;
            pDstSize->Height = pAttr->VeAttr.AttrMjpeg.mPicHeight;
            break;
        }
        case PT_H264:
        {
            pDstSize->Width = pAttr->VeAttr.AttrH264e.PicWidth;
            pDstSize->Height = pAttr->VeAttr.AttrH264e.PicHeight;
            break;
        }
        case PT_H265:
        {
            pDstSize->Width = pAttr->VeAttr.AttrH265e.mPicWidth;
            pDstSize->Height = pAttr->VeAttr.AttrH265e.mPicHeight;
            break;
        }
        default:
        {
            aloge("fatal error! unknown encode type:%d", pAttr->VeAttr.Type);
            ret = FAILURE;
            break;
        }
    }
    return ret;
}

AUDIO_SAMPLE_RATE_E map_SampleRate_to_AUDIO_SAMPLE_RATE_E(unsigned int nSampleRate)
{
    AUDIO_SAMPLE_RATE_E eSampleRate;
    switch(nSampleRate)
    {
        case 8000:
        {
            eSampleRate = AUDIO_SAMPLE_RATE_8000;
            break;
        }
        case 16000:
        {
            eSampleRate = AUDIO_SAMPLE_RATE_16000;
            break;
        }
        case 44100:
        {
            eSampleRate = AUDIO_SAMPLE_RATE_44100;
            break;
        }
        case 48000:
        {
            eSampleRate = AUDIO_SAMPLE_RATE_48000;
            break;
        }
        default:
        {
            aloge("fatal error! unsupport sampleRate[%ld]", nSampleRate);
            eSampleRate = AUDIO_SAMPLE_RATE_16000;
            break;
        }
    }
    return eSampleRate;
}

AUDIO_BIT_WIDTH_E map_BitWidth_to_AUDIO_BIT_WIDTH_E(unsigned int nBitWidth)
{
    AUDIO_BIT_WIDTH_E eBitWidth = AUDIO_BIT_WIDTH_16;
    switch(nBitWidth)
    {
        case 8:
        {
            eBitWidth = AUDIO_BIT_WIDTH_8;
            break;
        }
        case 16:
        {
            eBitWidth = AUDIO_BIT_WIDTH_16;
            break;
        }
        case 24:
        {
            eBitWidth = AUDIO_BIT_WIDTH_24;
            break;
        }
        case 32:
        {
            eBitWidth = AUDIO_BIT_WIDTH_32;
            break;
        }
        default:
        {
            aloge("fatal error! unsupport bit width[%ld]", nBitWidth);
            eBitWidth = AUDIO_BIT_WIDTH_16;
            break;
        }
    }
    return eBitWidth;
}

unsigned int map_AUDIO_SAMPLE_RATE_E_to_SampleRate(AUDIO_SAMPLE_RATE_E eSampleRate)
{
    unsigned int nSampleRate = (unsigned int)eSampleRate;
    return nSampleRate;
}
unsigned int map_AUDIO_BIT_WIDTH_E_to_BitWidth(AUDIO_BIT_WIDTH_E eBitWidth)
{
    unsigned int nBitWidth = (eBitWidth+1)*8;
    return nBitWidth;
}

