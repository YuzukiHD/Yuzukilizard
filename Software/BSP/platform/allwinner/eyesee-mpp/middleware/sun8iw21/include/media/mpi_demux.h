/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_demux.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/04/25
  Last Modified :
  Description   : MPP Programe Interface for demux
  Function List :
  History       :
******************************************************************************/

#ifndef  __IPCLINUX_MPI_DEMUX_H__
#define  __IPCLINUX_MPI_DEMUX_H__

#include "mm_common.h"
#include "mm_comm_demux.h"

#include "EncodedStream.h"
#include "DemuxCompStream.h"


#ifdef __cplusplus
extern "C"{
#endif /* End of #ifdef __cplusplus */

ERRORTYPE AW_MPI_DEMUX_CreateChn(DEMUX_CHN dmxChn, const DEMUX_CHN_ATTR_S *pstAttr);
ERRORTYPE AW_MPI_DEMUX_DestroyChn(DEMUX_CHN dmxChn);
ERRORTYPE AW_MPI_DEMUX_RegisterCallback(DEMUX_CHN dmxChn, MPPCallbackInfo *pCallback);
ERRORTYPE AW_MPI_DEMUX_SetChnAttr(DEMUX_CHN dmxChn, DEMUX_CHN_ATTR_S *pAttr);
ERRORTYPE AW_MPI_DEMUX_GetChnAttr(DEMUX_CHN dmxChn, DEMUX_CHN_ATTR_S *pstAttr);
ERRORTYPE AW_MPI_DEMUX_GetMediaInfo(DEMUX_CHN dmxChn, DEMUX_MEDIA_INFO_S *pMediaInfo);
ERRORTYPE AW_MPI_DEMUX_Start(DEMUX_CHN dmxChn);
ERRORTYPE AW_MPI_DEMUX_Stop(DEMUX_CHN dmxChn);
ERRORTYPE AW_MPI_DEMUX_Pause(DEMUX_CHN dmxChn);
//ERRORTYPE AW_MPI_DEMUX_ResetChn(PARAM_IN DEMUX_CHN dmxChn);
ERRORTYPE AW_MPI_DEMUX_Seek(DEMUX_CHN dmxChn, int msec);

ERRORTYPE AW_MPI_DEMUX_getDmxOutPutBuf(DEMUX_CHN dmxChn, EncodedStream *pDmxOutBuf, int nMilliSec);
ERRORTYPE AW_MPI_DEMUX_releaseDmxBuf(DEMUX_CHN dmxChn, EncodedStream *pDmxOutBuf);

ERRORTYPE AW_MPI_DEMUX_SelectVideoStream(DEMUX_CHN dmxChn, int nVideoIndex); //index from 0
ERRORTYPE AW_MPI_DEMUX_SelectAudioStream(DEMUX_CHN dmxChn, int nAudioIndex); //index from 0
ERRORTYPE AW_MPI_DEMUX_SelectSubtitleStream(DEMUX_CHN dmxChn, int nSubtitleIndex);

ERRORTYPE AW_MPI_DEMUX_SwitchDateSource(DEMUX_CHN dmxChn, DEMUX_CHN_ATTR_S *pChnAttr);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif /* End of #ifndef  __IPCLINUX_MPI_DEMUX_H__ */
