/******************************************************************************
  Copyright (C), 2001-2022, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : media_debug.h
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2022/03/26
  Last Modified :
  Description   : media debug functions implement
  Function List :
  History       :
******************************************************************************/
#ifndef __MEDIA_DEBUG_H__
#define __MEDIA_DEBUG_H__

#include <videoInputHw.h>
#include <VideoEnc_Component.h>
#include <RecRender_Component.h>

void loadMppViParams(viChnManager *pVipp, const char *pConfPath);
void loadMppVencParams(VIDEOENCDATATYPE *pVideoEncData, const char *pConfPath);
void loadMppMuxParams(RECRENDERDATATYPE *pRecRenderData, const char *pConfPath);

#endif /* __MEDIA_DEBUG_H__ */
