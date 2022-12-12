/******************************************************************************
  Copyright (C), 2001-2021, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : media_common_vcodec.h
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2021/05/15
  Last Modified :
  Description   : divide media_common to several parts, to decrease correlation.
  Function List :
  History       :
******************************************************************************/
#ifndef _MEDIA_COMMON_VCODEC_H_
#define _MEDIA_COMMON_VCODEC_H_

#include <mm_comm_venc.h>
#include "vdecoder.h"
#include "vencoder.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

VENC_CODEC_TYPE map_PAYLOAD_TYPE_E_to_VENC_CODEC_TYPE(PAYLOAD_TYPE_E nPayLoadType);
enum EVIDEOCODECFORMAT map_PAYLOAD_TYPE_E_to_EVIDEOCODECFORMAT(PAYLOAD_TYPE_E nPayLoadType);
PAYLOAD_TYPE_E map_VENC_CODEC_TYPE_to_PAYLOAD_TYPE_E(VENC_CODEC_TYPE eVencCodecType);
PAYLOAD_TYPE_E map_EVIDEOCODECFORMAT_to_PAYLOAD_TYPE_E(enum EVIDEOCODECFORMAT eCodecFormat);

unsigned int GetBitRateFromVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pAttr);
ERRORTYPE GetEncodeDstSizeFromVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pAttr, SIZE_S *pDstSize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* _MEDIA_COMMON_VCODEC_H_ */

