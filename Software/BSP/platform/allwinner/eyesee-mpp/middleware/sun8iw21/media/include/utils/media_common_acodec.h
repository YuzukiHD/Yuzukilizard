/******************************************************************************
  Copyright (C), 2001-2021, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : media_common_acodec.h
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2021/05/15
  Last Modified :
  Description   : divide media_common to several parts, to decrease correlation.
  Function List :
  History       :
******************************************************************************/
#ifndef _MEDIA_COMMON_ACODEC_H_
#define _MEDIA_COMMON_ACODEC_H_

#include <mm_common.h>

#include <aencoder.h>
#include <adecoder.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

AUDIO_ENCODER_TYPE map_PAYLOAD_TYPE_E_to_AUDIO_ENCODER_TYPE(PAYLOAD_TYPE_E nPayLoadType);
PAYLOAD_TYPE_E map_AUDIO_ENCODER_TYPE_to_PAYLOAD_TYPE_E(AUDIO_ENCODER_TYPE eAencCodecType);
enum EAUDIOCODECFORMAT map_PAYLOAD_TYPE_E_to_EAUDIOCODECFORMAT(PAYLOAD_TYPE_E srcFormat);
PAYLOAD_TYPE_E map_EAUDIOCODECFORMAT_to_PAYLOAD_TYPE_E(enum EAUDIOCODECFORMAT eCodecFormat, int eSubCodecFormat);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* _MEDIA_COMMON_ACODEC_H_ */

