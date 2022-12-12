/******************************************************************************
  Copyright (C), 2001-2022, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : media_common_aio.h
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2022/04/24
  Last Modified :
  Description   : some enum conversion.
  Function List :
  History       :
******************************************************************************/
#ifndef _MEDIA_COMMON_AIO_H_
#define _MEDIA_COMMON_AIO_H_

#include <mm_comm_aio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

AUDIO_SAMPLE_RATE_E map_SampleRate_to_AUDIO_SAMPLE_RATE_E(unsigned int nSampleRate);
AUDIO_BIT_WIDTH_E map_BitWidth_to_AUDIO_BIT_WIDTH_E(unsigned int nBitWidth);
unsigned int map_AUDIO_SAMPLE_RATE_E_to_SampleRate(AUDIO_SAMPLE_RATE_E eSampleRate);
unsigned int map_AUDIO_BIT_WIDTH_E_to_BitWidth(AUDIO_BIT_WIDTH_E eBitWidth);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* _MEDIA_COMMON_AIO_H_ */

