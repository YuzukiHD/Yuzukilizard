/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_mux.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/04/27
  Last Modified :
  Description   : mpi functions declaration for mux
  Function List :
  History       :
******************************************************************************/
#ifndef __IPCLINUX_MPI_MUX_H__
#define __IPCLINUX_MPI_MUX_H__

//ref platform headers
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"

//media api headers to app
#include "mm_common.h"
#include "mm_comm_mux.h"
#include "mm_comm_venc.h"

//media internal common headers.
#include <vencoder.h>
#include <ComponentCommon.h>
#include <RecorderMode.h>


#ifdef __cplusplus
extern "C" {
#endif

/* Group Settings */
ERRORTYPE AW_MPI_MUX_CreateGrp(MUX_GRP muxGrp, MUX_GRP_ATTR_S *pGrpAttr);
ERRORTYPE AW_MPI_MUX_DestroyGrp(MUX_GRP muxGrp);
/**
* if bShutDownNowFlag is true, the all mux component will be destroyed,
* and it maybe will discard cache data.
**/
ERRORTYPE AW_MPI_MUX_DestroyGrpEx(MUX_GRP muxGrp, BOOL bShutDownNowFlag);

ERRORTYPE AW_MPI_MUX_StartGrp(MUX_GRP muxGrp);
ERRORTYPE AW_MPI_MUX_StopGrp(MUX_GRP muxGrp);
ERRORTYPE AW_MPI_MUX_GetGrpAttr(MUX_GRP muxGrp, MUX_GRP_ATTR_S *pGrpAttr);
ERRORTYPE AW_MPI_MUX_SetGrpAttr(MUX_GRP muxGrp, MUX_GRP_ATTR_S *pGrpAttr);
//ERRORTYPE AW_MPI_MUX_SetGrpParam(MUX_GRP muxGrp, MUX_GRP_PARAM_S *pGroupParam);
//ERRORTYPE AW_MPI_MUX_GetGrpParam(MUX_GRP muxGrp, MUX_GRP_PARAM_S *pGroupParam);
ERRORTYPE AW_MPI_MUX_SetH264SpsPpsInfo(MUX_GRP muxGrp, VENC_CHN VeChn, VencHeaderData *pH264SpsPpsInfo);
ERRORTYPE AW_MPI_MUX_SetH265SpsPpsInfo(MUX_GRP muxGrp, VENC_CHN VeChn, VencHeaderData *pH265SpsPpsInfo);

/* Chn Settings */
ERRORTYPE AW_MPI_MUX_CreateChn(MUX_GRP muxGrp, MUX_CHN muxChn, MUX_CHN_ATTR_S *pChnAttr, int nFd);
ERRORTYPE AW_MPI_MUX_DestroyChn(MUX_GRP muxGrp, MUX_CHN muxChn);
/**
* if bShutDownNowFlag is true, the muxchn will be destroyed,
* and it maybe will discard cache data.
**/
ERRORTYPE AW_MPI_MUX_DestroyChnEx(MUX_GRP muxGrp, MUX_CHN muxChn, BOOL bShutDownNowFlag);

ERRORTYPE AW_MPI_MUX_GetChnAttr(MUX_GRP muxGrp, MUX_CHN muxChn, MUX_CHN_ATTR_S *pChnAttr);
ERRORTYPE AW_MPI_MUX_SetChnAttr(MUX_GRP muxGrp, MUX_CHN muxChn, MUX_CHN_ATTR_S *pChnAttr);
//ERRORTYPE AW_MPI_MUX_SetChnParam(MUX_GRP muxGrp, MUX_CHN muxChn, MUX_CHN_PARAM_S *pChnParam);
//ERRORTYPE AW_MPI_MUX_GetChnParam(MUX_GRP muxGrp, MUX_CHN muxChn, MUX_CHN_PARAM_S *pChnParam);
ERRORTYPE AW_MPI_MUX_SwitchFd(MUX_GRP muxGrp, MUX_CHN muxChn, int fd, int nFallocateLen);
ERRORTYPE AW_MPI_MUX_SwitchFileNormal(MUX_GRP muxGrp, MUX_CHN muxChn, int fd, int nFallocateLen, BOOL bIncludeCache);
ERRORTYPE AW_MPI_MUX_RegisterCallback(MUX_GRP muxGrp, MPPCallbackInfo *pCallback);
ERRORTYPE AW_MPI_MUX_GetCacheStatus(MUX_GRP muxGrp, CacheState *pCacheState);
ERRORTYPE AW_MPI_MUX_SetMuxCacheDuration(MUX_GRP muxGrp, int nCacheMs);
ERRORTYPE AW_MPI_MUX_SetMuxCacheStrmIds(MUX_GRP muxGrp, unsigned int nStreamIds);
ERRORTYPE AW_MPI_MUX_SetSwitchFileDurationPolicy(MUX_GRP muxGrp, MUX_CHN muxChn, RecordFileDurationPolicy ePolicy);
ERRORTYPE AW_MPI_MUX_GetSwitchFileDurationPolicy(MUX_GRP muxGrp, MUX_CHN muxChn, RecordFileDurationPolicy *pPolicy);
ERRORTYPE AW_MPI_MUX_SetSdCardState(MUX_GRP muxGrp, BOOL bExist);
ERRORTYPE AW_MPI_MUX_SetThmPic(MUX_GRP muxGrp, MUX_CHN muxChn, char *p_thm_pic, int thm_pic_size);
/*
 * for input non-tunnel mode, streamId = portIndex in mpi_mux.
 */
ERRORTYPE AW_MPI_MUX_SetVeChnBindStreamId(MUX_GRP muxGrp, VENC_CHN VeChn, int nStreamId);
/*
 * use sync mode, when function return, user can release stream.
 * component use internal memory to store input stream, so it can return immediately.
 * @param nTimeout unit:ms. 0:return immediately if no memory, -1: wait to send done, then return, >0:wait nTimeout ms to send done, then return.
 */
ERRORTYPE AW_MPI_MUX_SendVideoStream(MUX_GRP muxGrp, VENC_STREAM_S *pStream, int nStreamId, int nTimeout);
ERRORTYPE AW_MPI_MUX_SendAudioStream(MUX_GRP muxGrp, AUDIO_STREAM_S *pStream, int nStreamId, int nTimeout);

#ifdef __cplusplus
}
#endif

#endif /* __IPCLINUX_MPI_MUX_H__ */
