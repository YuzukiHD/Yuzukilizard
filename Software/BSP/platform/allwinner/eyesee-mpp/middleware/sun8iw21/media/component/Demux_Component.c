/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : Demux_Component.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/05
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "Demux_Component"
#include <utils/plat_log.h>

//ref platform headers
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>

#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//media api headers to app
#include "SystemBase.h"
#include "mm_common.h"
#include "mm_comm_demux.h"
#include "mpi_demux.h"

//media internal common headers.
#include "media_common.h"
#include "mm_component.h"
#include "ComponentCommon.h"
#include "tmessage.h"
#include "tsemaphore.h"
#include <DemuxTypeMap.h>
#include "Demux_Component.h"
#include <CDX_ErrorType.h>

#define DEBUG_SAVE_VIDEO_STREAM    (0)
#if DEBUG_SAVE_VIDEO_STREAM
static const char* fpVideoStreamPath = "/tmp/DmxVideo.dat";  //"/data/camera/1011.dat";
static FILE* fpVideoStream = NULL;
#endif

#define MAX_USE_BUFFER_NUM  (4)
#define PER_BLOCK_SIZE   (1024*64)

typedef struct _BUFFER_MNG
{
    int mPktListNodeCnt;
    struct list_head idlePktList;   //DMXPKT_NODE_T
    struct list_head fillingPktList;
    struct list_head readyPktList;
    struct list_head usingPktList;
    pthread_mutex_t  mPktlistMutex;

    BOOL mWaitReadyPktFlag;
    BOOL mWaitIdlePktFlag;
    pthread_cond_t mWaitIdleCondition;
    pthread_cond_t mWaitReadyCondition;
}BUFFER_MNG;

typedef struct _DEMUXDATATYPE
{
    COMP_STATETYPE                  state;
    pthread_mutex_t                 mStateLock;
    COMP_CALLBACKTYPE               *pCallbacks;
    void*                           pAppData;
    COMP_HANDLETYPE                 hSelf;

    COMP_PORT_PARAM_TYPE            sPortParam;
    COMP_PARAM_PORTDEFINITIONTYPE   sPortDef[DEMUX_MAX_PORT_COUNT];
    COMP_PARAM_PORTEXTRADEFINITIONTYPE sPortExtraDef[DEMUX_MAX_PORT_COUNT];
    COMP_INTERNAL_TUNNELINFOTYPE    sPortTunnelInfo[DEMUX_MAX_PORT_COUNT];
    COMP_PARAM_BUFFERSUPPLIERTYPE   sPortBufSupplier[DEMUX_MAX_PORT_COUNT];
    BOOL mInputPortTunnelFlag;
    BOOL mOutputPortTunnelFlag[DEMUX_MAX_PORT_COUNT]; //TRUE: tunnel mode; FALSE: non-tunnel mode.

    MPP_CHN_S                       mMppChnInfo;    // do not know what to do -> keep nChnId
    DEMUX_CHN_ATTR_S                mDemuxChnAttr;  // keep sourcedata info

    pthread_t                       thread_id;
    CompInternalMsgType             eTCmd;
    message_queue_t                 cmd_queue;
    int                             demux_type; //CDX_MEDIA_FILE_FORMAT, mp4/ts
    int                             disable_track;          //cdx_player set it to disable audio/video/subtitle track. DEMUX_DISABLE_AUDIO_TRACK
    // below 2 may not be used
    //int                             disable_media_type;     //cdx_player set it.CDX_MEDIA_TYPE_DISABLE_AVI, CDX_CODEC_TYPE_DISABLE_MPEG2
    int                             disable_proprity_track; //cdx_player set it to disable proprietary media type(audio ac3/dts\), 1 or 0.
    int                             mFd;    //record fd of setDataSource().
    CEDARX_SOURCETYPE               mSourceType;    //* url or fd or IStreamSource.
    CdxDataSourceT                  datasrc_desc;   //CedarXDataSourceDesc
    CedarXDemuxerAPI                *cdx_epdk_dmx;
    CdxMediaInfoT                   cdx_mediainfo;  //CedarXMediainfo, some pointer is malloc by parserLib, so if parserLib is close, these pointers will become wild pointer.
    CdxPacketT                      cdx_pkt;    //CedarXPacket
    CedarXSeekPara                  seek_para;
    int                         mCurVideoStreamIndex;
    int                         mCurAudioStreamIndex;   //from 0
    int                         mCurSubtitleStreamIndex;    //from 0
    BOOL                        mSwitchAudioFlag;

    int                         video_port_idx;
    int                         audio_port_idx;
    int                         subtitle_port_idx;
    int                         clock_port_idx;

    int64_t                     max_send_packet_time_video; //unit:us
    int64_t                     max_send_packet_time_audio; //unit:us
    int64_t                     max_send_packet_time_subtitle;

    int                         demux_init_end;
    int                         dmx_eof;
    int                         dmx_eof_send;
    int                         dmx_seek_flag;
    int                         sub_disable;
    unsigned int                prefetch_done;

    unsigned int                playBDFileTag;  ////1:bd, 0:common file. same as CedarXPlayerContext->play_bd_file

    int                         force_exit;
    int                         switching_state;

    BOOL mAllocBufFlag;
    BUFFER_MNG mBufferMng[DEMUX_MAX_PORT_COUNT];
    BOOL mWaitVideoBufFlag;
    BOOL mWaitAudioBufFlag;
    BOOL mWaitSubtitleBufFlag;
} DEMUXDATATYPE;

static void* ComponentThread(void* pThreadData);
static int CB_EventHandler(void* cookie, int event, void *data);


static int map_PacketType_To_MediaType(cdx_int32 pkt_type)
{
    int media_type = CDX_PacketUnkown;
    if (pkt_type == CDX_MEDIA_VIDEO)
    {
        media_type = CDX_PacketVideo;
    }
    else if (pkt_type == CDX_MEDIA_AUDIO)
    {
        media_type = CDX_PacketAudio;
    }
    else if (pkt_type == CDX_MEDIA_SUBTITLE)
    {
        media_type = CDX_PacketSubtitle;
    }
    else
    {
        aloge("fatal error! Unknown packet type (%d) detected", pkt_type);
    }
    return media_type;
}

static void allocDmxOutputBuf(DEMUXDATATYPE *pDemuxData)
{
    DMXPKT_NODE_T *pEntry, *pTmp;
    ERRORTYPE eError = SUCCESS;
    COMP_PARAM_PORTDEFINITIONTYPE* pPortDef = NULL;
    COMP_INTERNAL_TUNNELINFOTYPE* pTunnelInfo = NULL;
    COMP_PARAM_BUFFERSUPPLIERTYPE* pBufSupplier = NULL;
    BUFFER_MNG* pBufMng = NULL;

    if (pDemuxData->mAllocBufFlag)
    {
        alogw("buffer has alloc, never alloc again");
        return;
    }

    // find all ports
    int valid_ports[DEMUX_MAX_PORT_COUNT];
    int valid_port_num = 0;
    if (pDemuxData->video_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->video_port_idx;
        valid_port_num++;
    }
    if (pDemuxData->audio_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->audio_port_idx;
        valid_port_num++;
    }
    if (pDemuxData->subtitle_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->subtitle_port_idx;
        valid_port_num++;
    }

    for (int i = 0; i < valid_port_num; i++)
    {
        int cnt = 0;
        int port_idx = valid_ports[i];
        pPortDef     = &pDemuxData->sPortDef[port_idx];
        pTunnelInfo  = &pDemuxData->sPortTunnelInfo[port_idx];
        pBufSupplier = &pDemuxData->sPortBufSupplier[port_idx];
        pBufMng      = &pDemuxData->mBufferMng[port_idx];

        if (  pDemuxData->mOutputPortTunnelFlag[port_idx]
           && pBufSupplier->eBufferSupplier == COMP_BufferSupplyInput
           )
        {// audio/video/muxer supply buffer, no need to alloc
            continue;
        }

        pthread_mutex_lock(&pBufMng->mPktlistMutex);
        list_for_each_entry_safe(pEntry, pTmp, &pBufMng->idlePktList, mList)
        {
            pEntry->stEncodedStream.nBufferLen = PER_BLOCK_SIZE;
            pEntry->stEncodedStream.pBuffer = (unsigned char *)malloc(PER_BLOCK_SIZE);
            if (pEntry->stEncodedStream.pBuffer == NULL)
            {
                pEntry->stEncodedStream.nBufferLen = 0;
                break;
            }
            memset(pEntry->stEncodedStream.pBuffer, 0, pEntry->stEncodedStream.nBufferLen);
            pEntry->stEncodedStream.pBufferExtra = NULL;
            pEntry->stEncodedStream.nBufferExtraLen = 0;
            cnt++;
        }

        list_for_each_entry_safe(pEntry, pTmp, &pBufMng->idlePktList, mList)
        {
            if (pEntry->stEncodedStream.pBuffer == NULL) //free no alloc buf node
            {
                list_del(&pEntry->mList);
                free(pEntry);
                pBufMng->mPktListNodeCnt--;
            }
        }
        pthread_mutex_unlock(&pBufMng->mPktlistMutex);

        if (pBufMng->mPktListNodeCnt != cnt)
        {
            aloge("[%d]node alloc buf, but there is[%d] we marked, not compitible", pBufMng->mPktListNodeCnt, cnt);
        }

        alogd("port_idx[%d] alloc_buf, cnt = %d", port_idx, pBufMng->mPktListNodeCnt);
    }

    pDemuxData->mAllocBufFlag = TRUE;
    return;
}


static int DemuxGetCurrentProgramAudioNum(DEMUXDATATYPE *pDemuxData)
{
    struct CdxProgramS  *pProgram = &pDemuxData->cdx_mediainfo.program[0];
    return pProgram->audioNum;
}

// create buffer manager
static ERRORTYPE Demux_CreateBufferMng(DEMUXDATATYPE *pDemuxData, int port_idx)
{
    ERRORTYPE eError;
    BUFFER_MNG* pBufferMng = &pDemuxData->mBufferMng[port_idx];

    INIT_LIST_HEAD(&pBufferMng->idlePktList);
    INIT_LIST_HEAD(&pBufferMng->readyPktList);
    INIT_LIST_HEAD(&pBufferMng->usingPktList);
    INIT_LIST_HEAD(&pBufferMng->fillingPktList);
    pBufferMng->mPktListNodeCnt = 0;

    for (int i = 0; i < MAX_USE_BUFFER_NUM; i++)
    {
        DMXPKT_NODE_T* pNode = (DMXPKT_NODE_T *)malloc(sizeof(DMXPKT_NODE_T));
        if (NULL == pNode)
        {
            aloge("fatal error! alloc pkt_node fail");
            break;
        }
        memset(pNode, 0, sizeof(DMXPKT_NODE_T));
        pNode->stEncodedStream.nID = i + 1;
        list_add_tail(&pNode->mList, &pBufferMng->idlePktList);
        pBufferMng->mPktListNodeCnt++;
    }

    if (pBufferMng->mPktListNodeCnt == 0)
    {
        return FAILURE;
    }

    int err = pthread_mutex_init(&pBufferMng->mPktlistMutex, NULL);
    if (err !=0 )
    {
        aloge("fatal error! pthread mutex init fail!");
        eError = FAILURE;
        goto err0;
    }

    pthread_condattr_t condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    err = pthread_cond_init(&pBufferMng->mWaitIdleCondition, &condAttr);
    if (err != 0)
    {
        aloge("fatal error! init thread condition fail");
        eError = FAILURE;
        goto err1;
    }
    err = pthread_cond_init(&pBufferMng->mWaitReadyCondition, &condAttr);
    if (err != 0)
    {
        aloge("fatal error! init thread condition fail");
        eError = FAILURE;
        goto err2;
    }

    return SUCCESS;

err3:
    pthread_cond_destroy(&pBufferMng->mWaitIdleCondition);
err2:
    pthread_cond_destroy(&pBufferMng->mWaitReadyCondition);
err1:
    pthread_mutex_destroy(&pBufferMng->mPktlistMutex);
err0:
    if (!list_empty(&pBufferMng->idlePktList))
    {
        DMXPKT_NODE_T *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pBufferMng->idlePktList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    return eError;
}

// release buffer manager
static ERRORTYPE Demux_ReleaseBufferMng(DEMUXDATATYPE *pDemuxData)
{
    ERRORTYPE eError = SUCCESS;
    COMP_PARAM_PORTDEFINITIONTYPE* pPortDef = NULL;
    COMP_INTERNAL_TUNNELINFOTYPE* pTunnelInfo = NULL;
    COMP_PARAM_BUFFERSUPPLIERTYPE* pBufSupplier = NULL;
    BUFFER_MNG* pBufMng = NULL;

    // find all ports
    int valid_ports[DEMUX_MAX_PORT_COUNT];
    int valid_port_num = 0;
    if (pDemuxData->video_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->video_port_idx;
        valid_port_num++;
    }
    if (pDemuxData->audio_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->audio_port_idx;
        valid_port_num++;
    }
    if (pDemuxData->subtitle_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->subtitle_port_idx;
        valid_port_num++;
    }

    DMXPKT_NODE_T *pEntry, *pTmp;
    for (int i = 0; i < valid_port_num; i++)
    {
        int port_idx = valid_ports[i];
        pPortDef     = &pDemuxData->sPortDef[port_idx];
        pTunnelInfo  = &pDemuxData->sPortTunnelInfo[port_idx];
        pBufSupplier = &pDemuxData->sPortBufSupplier[port_idx];
        pBufMng      = &pDemuxData->mBufferMng[port_idx];

        pthread_mutex_lock(&pBufMng->mPktlistMutex);
        if (!list_empty(&pBufMng->readyPktList))
        {
            aloge("fatal error! port_idx = %d, some ready list node is still in use", port_idx);
            list_for_each_entry_safe(pEntry, pTmp, &pBufMng->readyPktList, mList)
            {
                list_del(&pEntry->mList);
                if (pEntry->stEncodedStream.pBuffer != NULL)
                {
                    free(pEntry->stEncodedStream.pBuffer);
                    pEntry->stEncodedStream.pBuffer = NULL;
                }
                free(pEntry);
            }
        }
        if (!list_empty(&pBufMng->fillingPktList))
        {
            aloge("fatal error! port_idx = %d, some filling list node is still in use", port_idx);
            list_for_each_entry_safe(pEntry, pTmp, &pBufMng->fillingPktList, mList)
            {
                list_del(&pEntry->mList);
                if (pEntry->stEncodedStream.pBuffer != NULL && !pDemuxData->mOutputPortTunnelFlag)
                {
                    free(pEntry->stEncodedStream.pBuffer);
                    pEntry->stEncodedStream.pBuffer = NULL;
                }
                free(pEntry);
            }
        }
        if (!list_empty(&pBufMng->usingPktList))
        {
            aloge("fatal error! port_idx = %d, some using list node is still in use", port_idx);
            list_for_each_entry_safe(pEntry, pTmp, &pBufMng->usingPktList, mList)
            {
                list_del(&pEntry->mList);
                if (pEntry->stEncodedStream.pBuffer != NULL)
                {
                    free(pEntry->stEncodedStream.pBuffer);
                    pEntry->stEncodedStream.pBuffer = NULL;
                }
                free(pEntry);
            }
        }
        if (!list_empty(&pBufMng->idlePktList))
        {
            list_for_each_entry_safe(pEntry, pTmp, &pBufMng->idlePktList, mList)
            {
                list_del(&pEntry->mList);
                if (pEntry->stEncodedStream.pBuffer != NULL)
                {
                    free(pEntry->stEncodedStream.pBuffer);
                    pEntry->stEncodedStream.pBuffer = NULL;
                }
                free(pEntry);
            }
        }
        pthread_mutex_unlock(&pBufMng->mPktlistMutex);
        pthread_mutex_destroy(&pBufMng->mPktlistMutex);
        pthread_cond_destroy(&pBufMng->mWaitIdleCondition);
        pthread_cond_destroy(&pBufMng->mWaitReadyCondition);
    }

    return SUCCESS;
}

// check filling buffers empty when tunneo mode and input buf supplier
static ERRORTYPE Demux_CheckFillingPktEmpty(DEMUXDATATYPE *pDemuxData)
{
    ERRORTYPE ret = SUCCESS;
    COMP_PARAM_PORTDEFINITIONTYPE* pPortDef = NULL;
    COMP_INTERNAL_TUNNELINFOTYPE* pTunnelInfo = NULL;
    COMP_PARAM_BUFFERSUPPLIERTYPE* pBufSupplier = NULL;
    BUFFER_MNG* pBufMng = NULL;

    // find all ports
    int valid_ports[DEMUX_MAX_PORT_COUNT];
    int valid_port_num = 0;
    if (pDemuxData->video_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->video_port_idx;
        valid_port_num++;
    }
    if (pDemuxData->audio_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->audio_port_idx;
        valid_port_num++;
    }
    if (pDemuxData->subtitle_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->subtitle_port_idx;
        valid_port_num++;
    }

    for (int i = 0; i < valid_port_num; i++)
    {
        int port_idx = valid_ports[i];
        pPortDef     = &pDemuxData->sPortDef[port_idx];
        pTunnelInfo  = &pDemuxData->sPortTunnelInfo[port_idx];
        pBufSupplier = &pDemuxData->sPortBufSupplier[port_idx];
        pBufMng      = &pDemuxData->mBufferMng[port_idx];

        if (  pDemuxData->mOutputPortTunnelFlag[port_idx] != TRUE
           || pBufSupplier->eBufferSupplier != COMP_BufferSupplyInput
           )
        {
            continue;
        }

        // check filling Pkt buffer
        pthread_mutex_lock(&pBufMng->mPktlistMutex);
        int idleCnt = 0;
        struct list_head *pList;
        list_for_each(pList, &pBufMng->idlePktList) { idleCnt++; }
        int fillingCnt = 0;
        list_for_each(pList, &pBufMng->fillingPktList) { fillingCnt++; }
        int readyCnt = 0;
        list_for_each(pList, &pBufMng->readyPktList) { readyCnt++; }
        int usingCnt = 0;
        list_for_each(pList, &pBufMng->usingPktList) { usingCnt++; }
        pthread_mutex_unlock(&pBufMng->mPktlistMutex);

        if (fillingCnt != 0)
        {
            if (pPortDef->eDomain == COMP_PortDomainAudio)
            {
                aloge("fatal error! Port[%d]:Audio filling number:[%d]", port_idx, fillingCnt);
            }
            else if (pPortDef->eDomain == COMP_PortDomainVideo)
            {
                aloge("fatal error! Port[%d]:Video filling number:[%d]", port_idx, fillingCnt);
            }
            else if (pPortDef->eDomain == COMP_PortDomainSubtitle)
            {
                aloge("fatal error! Port[%d]:Subtitle filling number:[%d]", port_idx, fillingCnt);
            }
        }
    }

    return SUCCESS;
}

/**
 * use in tunnel mode, return all buffers belong to other component.
 */
static ERRORTYPE Demux_ReturnAllBuffer(DEMUXDATATYPE *pDemuxData)
{
    ERRORTYPE ret = SUCCESS;
    COMP_PARAM_PORTDEFINITIONTYPE* pPortDef = NULL;
    COMP_INTERNAL_TUNNELINFOTYPE* pTunnelInfo = NULL;
    COMP_PARAM_BUFFERSUPPLIERTYPE* pBufSupplier = NULL;
    BUFFER_MNG* pBufMng = NULL;

    // find all ports
    int valid_ports[DEMUX_MAX_PORT_COUNT];
    int valid_port_num = 0;
    if (pDemuxData->video_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->video_port_idx;
        valid_port_num++;
    }
    if (pDemuxData->audio_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->audio_port_idx;
        valid_port_num++;
    }
    if (pDemuxData->subtitle_port_idx != -1)
    {
        valid_ports[valid_port_num] = pDemuxData->subtitle_port_idx;
        valid_port_num++;
    }

    for (int i = 0; i < valid_port_num; i++)
    {
        int port_idx = valid_ports[i];
        pPortDef     = &pDemuxData->sPortDef[port_idx];
        pTunnelInfo  = &pDemuxData->sPortTunnelInfo[port_idx];
        pBufSupplier = &pDemuxData->sPortBufSupplier[port_idx];
        pBufMng      = &pDemuxData->mBufferMng[port_idx];

        // Show buffer status
        pthread_mutex_lock(&pBufMng->mPktlistMutex);
        int idleCnt = 0;
        struct list_head *pList;
        list_for_each(pList, &pBufMng->idlePktList) { idleCnt++; }
        int fillingCnt = 0;
        list_for_each(pList, &pBufMng->fillingPktList) { fillingCnt++; }
        int readyCnt = 0;
        list_for_each(pList, &pBufMng->readyPktList) { readyCnt++; }
        int usingCnt = 0;
        list_for_each(pList, &pBufMng->usingPktList) { usingCnt++; }
        pthread_mutex_unlock(&pBufMng->mPktlistMutex);

        alogd(" Port[%d]: OutTunnelFlag[%d], BufSuppier[%d] 1-input 2-output pkt num:[idle-%d][filling-%d][ready-%d][using-%d]\n"
              ,port_idx
              ,pDemuxData->mOutputPortTunnelFlag[port_idx]
              ,pBufSupplier->eBufferSupplier
              ,idleCnt, fillingCnt, readyCnt, usingCnt
              );

        if (  pDemuxData->mOutputPortTunnelFlag[port_idx] == TRUE
           && pBufSupplier->eBufferSupplier == COMP_BufferSupplyInput
           )
        {// return buffer to B comp
            while(1)
            {
                pthread_mutex_lock(&pBufMng->mPktlistMutex);
                DMXPKT_NODE_T *pNode = list_first_entry_or_null(&pBufMng->fillingPktList, DMXPKT_NODE_T, mList);
                if (NULL == pNode)
                {
                    pthread_mutex_unlock(&pBufMng->mPktlistMutex);
                    ret = SUCCESS;
                    break;
                }
                list_del(&pNode->mList);
                pthread_mutex_unlock(&pBufMng->mPktlistMutex);

                COMP_BUFFERHEADERTYPE omx_buffer_header;
                EncodedStream dmxOutBuf;
                dmxOutBuf.media_type    = pNode->stEncodedStream.media_type;
                dmxOutBuf.nFilledLen    = -1;
                dmxOutBuf.nTobeFillLen  = 0;
                dmxOutBuf.pBuffer       = pNode->stEncodedStream.pBuffer;
                dmxOutBuf.pBufferExtra  = pNode->stEncodedStream.pBufferExtra;

                omx_buffer_header.pOutputPortPrivate = (void*)&dmxOutBuf;
                omx_buffer_header.nInputPortIndex = pTunnelInfo->nTunnelPortIndex;
                alogv("curState[0x%x] nFilledLen[%d] nTobeFillLen[%d] pBuffer[%p] pBufferExtra[%p] media_type[%d]",
                      pDemuxData->state, dmxOutBuf.nFilledLen, dmxOutBuf.nTobeFillLen,
                      dmxOutBuf.pBuffer, dmxOutBuf.pBufferExtra, dmxOutBuf.media_type);
                COMP_EmptyThisBuffer(pTunnelInfo->hTunnel,  (void*)&omx_buffer_header);

                pthread_mutex_lock(&pBufMng->mPktlistMutex);
                memset(&pNode->stEncodedStream, 0, sizeof(EncodedStream));
                list_add_tail(&pNode->mList, &pBufMng->idlePktList);
                pthread_mutex_unlock(&pBufMng->mPktlistMutex);
            }
        }
        else
        {// move pkt to idle list;

            DMXPKT_NODE_T *pEntry, *pTmp;
            pthread_mutex_lock(&pBufMng->mPktlistMutex);
            if (!list_empty(&pBufMng->fillingPktList))
            {
                list_for_each_entry_safe(pEntry, pTmp, &pBufMng->fillingPktList, mList)
                {
                    list_move_tail(&pEntry->mList, &pBufMng->idlePktList);
                }
            }
            if (!list_empty(&pBufMng->readyPktList))
            {
                list_for_each_entry_safe(pEntry, pTmp, &pBufMng->readyPktList, mList)
                {
                    list_move_tail(&pEntry->mList, &pBufMng->idlePktList);
                }
            }
            pthread_mutex_unlock(&pBufMng->mPktlistMutex);
        }
    }

    return ret;
}

/*****************************************************************************/
static ERRORTYPE DemuxOpenParserLib(DEMUXDATATYPE *pDemuxData)
{
    ERRORTYPE eError = SUCCESS;
    pDemuxData->cdx_epdk_dmx = cedarx_demux_create(pDemuxData->mSourceType); //don't need demux_type
    CdxDemuxEventCallbackInfo dmxEventCbInfo;
    dmxEventCbInfo.mpCbEventHandler = (void*)CB_EventHandler;
    dmxEventCbInfo.mpCookie = (void*)pDemuxData;
    pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_SET_DEMUX_EVENT_CALLBACK, 0, (void*)&dmxEventCbInfo);
    eError = pDemuxData->cdx_epdk_dmx->open(pDemuxData->cdx_epdk_dmx, &pDemuxData->cdx_mediainfo, &pDemuxData->datasrc_desc);
    if(eError < 0)
    {
        aloge("mpp demuxer open error");
        //pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf, pDemuxData->pAppData, OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateInvalid, (OMX_PTR)&ret);
        //eError = COMP_ErrorUndefined;
    }
    else
    {
        clearDataSourceFields(&pDemuxData->datasrc_desc);
        close(pDemuxData->mFd);
        pDemuxData->mFd = -1;
        pDemuxData->demux_type = (int)ParserType2DemuxType(pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_GET_PARSER_TYPE, 0, NULL));
    }
    return eError;
}

static ERRORTYPE DemuxOpenReopenParserLib(DEMUXDATATYPE *pDemuxData)
{
    if (NULL != pDemuxData->cdx_epdk_dmx)
    {
        pDemuxData->cdx_epdk_dmx->close(pDemuxData->cdx_epdk_dmx);
        cedarx_demux_destory(pDemuxData->cdx_epdk_dmx);
        clearCdxMediaInfoT(&pDemuxData->cdx_mediainfo, (int)DemuxType2ParserType(pDemuxData->demux_type));
        pDemuxData->cdx_epdk_dmx = NULL;
        pDemuxData->demux_init_end = 0;
    }

    if (NULL == pDemuxData->cdx_epdk_dmx && 0 == pDemuxData->cdx_epdk_dmx)
    {
        if (SUCCESS != DemuxOpenParserLib(pDemuxData))
        {
            aloge("open next file parser fail!");
            return FAILURE;
        }
        pDemuxData->dmx_eof = 0;
        pDemuxData->dmx_eof_send = 0;
        pDemuxData->demux_init_end = 1;
    }

    return SUCCESS;
}

static ERRORTYPE CreateDemuxPorts(PARAM_IN DEMUXDATATYPE *pDemuxData, PARAM_IN CdxMediaInfoT *cdx_mediainfo)
{
    pDemuxData->video_port_idx              = -1;
    pDemuxData->audio_port_idx              = -1;
    pDemuxData->subtitle_port_idx           = -1;
    pDemuxData->max_send_packet_time_video  = -1;
    pDemuxData->max_send_packet_time_audio  = -1;
    pDemuxData->max_send_packet_time_subtitle = -1;
    BOOL audioValid = FALSE;
    BOOL videoValid = FALSE;
    BOOL subtitleValid = FALSE;

    if (cdx_mediainfo->programNum <= 0 || cdx_mediainfo->programIndex < 0 || cdx_mediainfo->programIndex >= cdx_mediainfo->programNum)
    {
        aloge("fatal error! no program[%d][%d]!", cdx_mediainfo->programNum, cdx_mediainfo->programIndex);
        return FAILURE;
    }
    struct CdxProgramS *pProgram = &cdx_mediainfo->program[cdx_mediainfo->programIndex];

    // create audio output port.
    if (pProgram->audioNum > 0 && pProgram->audioIndex >= 0 && pProgram->audioIndex < pProgram->audioNum)
    {
        if (pProgram->audio[pProgram->audioIndex].eCodecFormat > AUDIO_CODEC_FORMAT_UNKNOWN)
        {
            audioValid = TRUE;
        }
    }
    else
    {
        alogw("no audio?[%d][%d]", pProgram->audioIndex, pProgram->audioNum);
    }
    if (audioValid && !(pDemuxData->disable_track & DEMUX_DISABLE_AUDIO_TRACK))
    {
        int i = 0;

        if (pDemuxData->disable_proprity_track)
        {
            for (i = 0; i < pProgram->audioNum; i++)
            {
                if (  pProgram->audio[i].eCodecFormat != AUDIO_CODEC_FORMAT_AC3
                   && pProgram->audio[i].eCodecFormat != AUDIO_CODEC_FORMAT_DTS
                   )
                {
                    break;
                }
            }

            if(i >= pProgram->audioNum)
            {
                alogw("no supported audio track found!");
                goto _SKIP_AUDIO_PORT;
            }

            if (i > 0) //switch to supported track
            {
                alogw("demuxLib will read all audio stream, so no need to set audio track index to demuxLib now.");
                pProgram->audioIndex = i;
            }
        }

        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].nPortIndex              = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].bEnabled                = TRUE;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].eDomain                 = COMP_PortDomainAudio;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].eDir                    = COMP_DirOutput;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].format.audio.cMIMEType  = "compressed_audio";

        pDemuxData->sPortExtraDef[pDemuxData->sPortParam.nPorts].nPortIndex         = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortExtraDef[pDemuxData->sPortParam.nPorts].pVendorInfo        = (void*)&pProgram->audio[pProgram->audioIndex];

        pDemuxData->sPortBufSupplier[pDemuxData->sPortParam.nPorts].nPortIndex      = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortBufSupplier[pDemuxData->sPortParam.nPorts].eBufferSupplier = COMP_BufferSupplyInput;

        pDemuxData->sPortTunnelInfo[pDemuxData->sPortParam.nPorts].nPortIndex       = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortTunnelInfo[pDemuxData->sPortParam.nPorts].eTunnelType      = TUNNEL_TYPE_COMMON;

        pDemuxData->audio_port_idx = pDemuxData->sPortParam.nPorts;
        Demux_CreateBufferMng(pDemuxData, pDemuxData->audio_port_idx);
        pDemuxData->sPortParam.nPorts++;
    }

_SKIP_AUDIO_PORT:
    // create video output port.
    if (pProgram->videoNum > 0 && pProgram->videoIndex >= 0 && pProgram->videoIndex < pProgram->videoNum)
    {
        if (  pProgram->video[pProgram->videoIndex].eCodecFormat >= VIDEO_CODEC_FORMAT_MIN
           && pProgram->video[pProgram->videoIndex].eCodecFormat <= VIDEO_CODEC_FORMAT_MAX
           )
        {
            videoValid = TRUE;
        }
    }
    else
    {
        alogw("no video?[%d][%d]", pProgram->videoIndex, pProgram->videoNum);
    }

    if (videoValid && !(pDemuxData->disable_track & DEMUX_DISABLE_VIDEO_TRACK))
    {
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].nPortIndex = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].bEnabled   = TRUE;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].eDomain    = COMP_PortDomainVideo;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].eDir       = COMP_DirOutput;

        pDemuxData->sPortExtraDef[pDemuxData->sPortParam.nPorts].nPortIndex  = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortExtraDef[pDemuxData->sPortParam.nPorts].pVendorInfo = (void*)&pProgram->video[pProgram->videoIndex];

        pDemuxData->sPortBufSupplier[pDemuxData->sPortParam.nPorts].nPortIndex      = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortBufSupplier[pDemuxData->sPortParam.nPorts].eBufferSupplier = COMP_BufferSupplyInput;

        pDemuxData->sPortTunnelInfo[pDemuxData->sPortParam.nPorts].nPortIndex  = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortTunnelInfo[pDemuxData->sPortParam.nPorts].eTunnelType = TUNNEL_TYPE_COMMON;

        pDemuxData->video_port_idx = pDemuxData->sPortParam.nPorts;
        Demux_CreateBufferMng(pDemuxData, pDemuxData->video_port_idx);
        pDemuxData->sPortParam.nPorts++;

#if DEBUG_SAVE_VIDEO_STREAM
        VideoStreamInfo *pVideoStreamInfo = (void*)&pProgram->video[pProgram->videoIndex];
        if(pVideoStreamInfo->nCodecSpecificDataLen > 0)
        {
            fwrite(pVideoStreamInfo->pCodecSpecificData, 1, pVideoStreamInfo->nCodecSpecificDataLen, fpVideoStream);
        }
#endif
    }

    //* if there is neither video nor audio, cedarx do not support.
    if(pDemuxData->sPortParam.nPorts < 1)
        //return COMP_ErrorInsufficientResources;
        return ERR_DEMUX_NOT_SUPPORT;

    //* create embedded subtitle output port.
    if (pProgram->subtitleNum > 0 && pProgram->subtitleIndex >= 0 && pProgram->subtitleIndex < pProgram->subtitleNum)
    {
        if (pProgram->subtitle[pProgram->subtitleIndex].eCodecFormat > SUBTITLE_CODEC_UNKNOWN)
        {
            subtitleValid = TRUE;
        }
    }
    else
    {
        alogw("no subtitle?[%d][%d]", pProgram->subtitleIndex, pProgram->subtitleNum);
    }

    if (subtitleValid && !(pDemuxData->disable_track & DEMUX_DISABLE_SUBTITLE_TRACK))
    {
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].nPortIndex = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].bEnabled   = TRUE;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].eDomain    = COMP_PortDomainSubtitle;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].eDir       = COMP_DirOutput;

        pDemuxData->sPortExtraDef[pDemuxData->sPortParam.nPorts].nPortIndex  = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortExtraDef[pDemuxData->sPortParam.nPorts].pVendorInfo = (void*)&pProgram->subtitle[pProgram->subtitleIndex];

        pDemuxData->sPortBufSupplier[pDemuxData->sPortParam.nPorts].nPortIndex      = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortBufSupplier[pDemuxData->sPortParam.nPorts].eBufferSupplier = COMP_BufferSupplyInput;

        pDemuxData->sPortTunnelInfo[pDemuxData->sPortParam.nPorts].nPortIndex  = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortTunnelInfo[pDemuxData->sPortParam.nPorts].eTunnelType = TUNNEL_TYPE_COMMON;

        pDemuxData->subtitle_port_idx = pDemuxData->sPortParam.nPorts;
        Demux_CreateBufferMng(pDemuxData, pDemuxData->subtitle_port_idx);
        pDemuxData->sPortParam.nPorts++;
    }

    //* create an input port for getting clock component, the Parser may use this port to get current clock value.
    {
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].nPortIndex = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].bEnabled   = TRUE;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].eDomain    = COMP_PortDomainOther;
        pDemuxData->sPortDef[pDemuxData->sPortParam.nPorts].eDir       = COMP_DirInput;

        pDemuxData->sPortExtraDef[pDemuxData->sPortParam.nPorts].nPortIndex  = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortExtraDef[pDemuxData->sPortParam.nPorts].pVendorInfo = NULL;

        pDemuxData->sPortBufSupplier[pDemuxData->sPortParam.nPorts].nPortIndex      = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortBufSupplier[pDemuxData->sPortParam.nPorts].eBufferSupplier = COMP_BufferSupplyOutput;

        pDemuxData->sPortTunnelInfo[pDemuxData->sPortParam.nPorts].nPortIndex = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortTunnelInfo[pDemuxData->sPortParam.nPorts].eTunnelType = TUNNEL_TYPE_CLOCK;

        pDemuxData->clock_port_idx = pDemuxData->sPortParam.nPorts;
        pDemuxData->sPortParam.nPorts++;
    }
    return SUCCESS;
}

static ERRORTYPE DemuxSwitchDataSource(PARAM_IN COMP_HANDLETYPE hComponent, DEMUX_CHN_ATTR_S *pChnAttr)
{
    ERRORTYPE eError = SUCCESS;
    CedarXDataSourceDesc DataSrc;
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if (NULL != pDemuxData->cdx_epdk_dmx)
    {
        pDemuxData->cdx_epdk_dmx->close(pDemuxData->cdx_epdk_dmx);
        cedarx_demux_destory(pDemuxData->cdx_epdk_dmx);
        clearDataSourceFields(&pDemuxData->datasrc_desc);
        clearCdxMediaInfoT(&pDemuxData->cdx_mediainfo, (int)DemuxType2ParserType(pDemuxData->demux_type));
        pDemuxData->cdx_epdk_dmx = NULL;
        pDemuxData->demux_init_end = 0;
        close(pDemuxData->mFd);
        pDemuxData->mFd = -1;
        pDemuxData->prefetch_done = 0;
    }

    if (pChnAttr->mSourceType == SOURCETYPE_FD)
    {
        if (pChnAttr->mFd > 0) {
            pDemuxData->mSourceType = CEDARX_SOURCE_FD;
            memset(&DataSrc, 0, sizeof(CedarXDataSourceDesc));
            DataSrc.source_type = CEDARX_SOURCE_FD;
            DataSrc.ext_fd_desc.fd = pChnAttr->mFd;
            pDemuxData->mFd = dup(DataSrc.ext_fd_desc.fd);
            DataSrc.ext_fd_desc.fd = pDemuxData->mFd;
            AwDemuxerSetDataSource(&pDemuxData->datasrc_desc, &DataSrc);
        }
        else
        {
            alogd("Why ChnAttr.mFd is %d!?", pChnAttr->mFd);
        }
    }
    else if (pChnAttr->mSourceType == SOURCETYPE_FILEPATH)
    {
        alogw("SourceType(FILEPATH) NOT support now!");
        eError = FAILURE;
    }
    else
    {
        aloge("SourceType(%d) NOT support now!", pChnAttr->mSourceType);
        eError = FAILURE;
    }

    eError = DemuxOpenReopenParserLib(pDemuxData);
    return eError;
}


/*****************************************************************************/
ERRORTYPE DemuxSetCallbacks(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_CALLBACKTYPE* pCallbacks,
        PARAM_IN void* pAppData)
{
    DEMUXDATATYPE *pDemuxData;
    ERRORTYPE eError = SUCCESS;

    pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(!pDemuxData || !pCallbacks || !pAppData)
    {
        eError = ERR_DEMUX_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    pDemuxData->pCallbacks = pCallbacks;
    pDemuxData->pAppData = pAppData;

    COMP_CONF_CMD_FAIL: return eError;
}

/*****************************************************************************/
ERRORTYPE DemuxSendCommand(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_COMMANDTYPE Cmd,
        PARAM_IN unsigned int nParam1,
        PARAM_IN void* pCmdData)
{
    DEMUXDATATYPE *pDemuxData;
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;

    alogv("DemuxSendCommand: %d", Cmd);

    pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(!pDemuxData)
    {
        eError = ERR_DEMUX_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    pthread_mutex_lock(&pDemuxData->mStateLock);
    if (pDemuxData->state == COMP_StateInvalid)
    {
        eError = ERR_DEMUX_SYS_NOTREADY;
        pthread_mutex_unlock(&pDemuxData->mStateLock);
        goto COMP_CONF_CMD_FAIL;
    }

    switch (Cmd)
    {
        case COMP_CommandStateSet:
            eCmd = SetState;
            break;

        case COMP_CommandFlush:
            eCmd = Flush;
            break;

        case COMP_CommandPortDisable:
            eCmd = StopPort;
            break;

        default:
            alogw("impossible comp_command[0x%x]", Cmd);
            eCmd = -1;
            break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pDemuxData->cmd_queue, &msg);

    pthread_mutex_unlock(&pDemuxData->mStateLock);
    COMP_CONF_CMD_FAIL: return eError;
}

static ERRORTYPE DemuxsetDataSource(PARAM_IN COMP_HANDLETYPE hComponent)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    DEMUX_CHN_ATTR_S *pChnAttr = &pDemuxData->mDemuxChnAttr;
    CedarXDataSourceDesc DataSrc;

    if (pChnAttr->mSourceType == SOURCETYPE_FD)
    {
        if (pChnAttr->mFd > 0) {
            pDemuxData->mSourceType = CEDARX_SOURCE_FD;
            memset(&DataSrc, 0, sizeof(CedarXDataSourceDesc));
            DataSrc.source_type = CEDARX_SOURCE_FD;
            DataSrc.ext_fd_desc.fd = pChnAttr->mFd;
            pDemuxData->mFd = dup(DataSrc.ext_fd_desc.fd);
            DataSrc.ext_fd_desc.fd = pDemuxData->mFd;
            AwDemuxerSetDataSource(&pDemuxData->datasrc_desc, &DataSrc);
        }
        else
        {
            alogd("Why ChnAttr.mFd is %d!?", pChnAttr->mFd);
        }
    }
    else if (pChnAttr->mSourceType == SOURCETYPE_FILEPATH)
    {
        alogw("SourceType(FILEPATH) NOT support now!");
        eError = FAILURE;
    }
    else
    {
        aloge("SourceType(%d) NOT support now!", pChnAttr->mSourceType);
        eError = FAILURE;
    }

    return eError;
}

//static ERRORTYPE DemuxDisableTrack(
//        PARAM_IN COMP_HANDLETYPE hComponent,
//        PARAM_IN int nDisableTrack)
//{
//    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
//    pDemuxData->disable_track = nDisableTrack;
//    return SUCCESS;
//}

//static ERRORTYPE DemuxDisableMediaType(
//        PARAM_IN COMP_HANDLETYPE hComponent,
//        PARAM_IN int nDisableMediaType)
//{
//    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
//    pDemuxData->disable_media_type = nDisableMediaType;
//    return SUCCESS;
//}

static ERRORTYPE DemuxPreparePorts(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN void* ptr)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if(0 == pDemuxData->demux_init_end)
    {
        if(DemuxOpenParserLib(pDemuxData) != SUCCESS)
        {
            return COMP_EventError;
        }
        CreateDemuxPorts(pDemuxData, &pDemuxData->cdx_mediainfo);
        pDemuxData->demux_init_end = 1;
    }
    else
    {
        alogd("Demux comp already init ports! init_end[%d]", pDemuxData->demux_init_end);
    }

    return eError;
}

ERRORTYPE DemuxSeekToPosition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN CedarXSeekPara *pSeekPara)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if(pDemuxData->prefetch_done == 1)
    {
        alogd("***************seekTo[%d]ms discard data.", pSeekPara->seek_time);
        if(CDX_OK != pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_SKIP_CHUNK_DATA, 0, &pDemuxData->cdx_pkt))
        {
            pDemuxData->prefetch_done = 0;
            return ERR_DEMUX_ILLEGAL_PARAM;
        }
    }
    pDemuxData->prefetch_done = 0;

    if(CDX_OK != pDemuxData->cdx_epdk_dmx->seek(pDemuxData->cdx_epdk_dmx, pSeekPara->seek_time/1000, 0))
    {
        return ERR_DEMUX_ILLEGAL_PARAM;
    }
    memcpy(&pDemuxData->seek_para, pSeekPara, sizeof(CedarXSeekPara));
    pDemuxData->max_send_packet_time_video = -1;
    pDemuxData->max_send_packet_time_audio = -1;
    pDemuxData->max_send_packet_time_subtitle = -1;
    pDemuxData->dmx_eof = 0;
    pDemuxData->dmx_seek_flag = 1;
    return eError;
}


ERRORTYPE DemuxComponentTunnelRequest(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  unsigned int nPort,
        PARAM_IN  COMP_HANDLETYPE hTunneledComp,
        PARAM_IN  unsigned int nTunneledPort,
        PARAM_INOUT  COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pDemuxData->mStateLock);
    if (pDemuxData->state == COMP_StateExecuting)
    {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    }
    else if(pDemuxData->state != COMP_StateIdle)
    {
        aloge("fatal error! tunnel request can't be in state[0x%x]", pDemuxData->state);
        eError = ERR_DEMUX_NOT_PERM;
        goto COMP_CMD_FAIL;
    }

    COMP_PARAM_PORTDEFINITIONTYPE   *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE    *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE   *pPortBufSupplier;
    BOOL bFindFlag;
    int i;

    bFindFlag = FALSE;
    for(i=0; i<DEMUX_MAX_PORT_COUNT; i++)
    {
        if(pDemuxData->sPortDef[i].nPortIndex == nPort)
        {
            pPortDef = &pDemuxData->sPortDef[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%u] wrong!", nPort);
        eError = ERR_DEMUX_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }


    bFindFlag = FALSE;
    for(i=0; i<DEMUX_MAX_PORT_COUNT; i++)
    {
        if(pDemuxData->sPortTunnelInfo[i].nPortIndex == nPort)
        {
            pPortTunnelInfo = &pDemuxData->sPortTunnelInfo[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%u] wrong!", nPort);
        eError = ERR_DEMUX_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for(i=0; i<DEMUX_MAX_PORT_COUNT; i++)
    {
        if(pDemuxData->sPortBufSupplier[i].nPortIndex == nPort)
        {
            pPortBufSupplier = &pDemuxData->sPortBufSupplier[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_DEMUX_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if(NULL==hTunneledComp && 0==nTunneledPort && NULL==pTunnelSetup)
    {
        alogd("omx_core cancel setup tunnel on port[%d]", nPort);
        eError = SUCCESS;
        if(pPortDef->eDir == COMP_DirOutput)
        {
            pDemuxData->mOutputPortTunnelFlag[nPort] = FALSE;
        }
        else
        {
            pDemuxData->mInputPortTunnelFlag = FALSE;
        }
        goto COMP_CMD_FAIL;
    }
    if(pPortDef->eDir == COMP_DirOutput)
    {
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        pDemuxData->mOutputPortTunnelFlag[nPort] = TRUE;
    }
    else
    {
        if (pDemuxData->mInputPortTunnelFlag) {
            aloge("Dmx_Comp inport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        //Check the data compatibility between the ports using one or more GetParameter calls.
        //B checks if its input port is compatible with the output port of component A.
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if(out_port_def.eDir != COMP_DirOutput)
        {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_DEMUX_ILLEGAL_PARAM;
            goto COMP_CMD_FAIL;
        }
        pPortDef->format = out_port_def.format;

        //The component B informs component A about the final result of negotiation.
        if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier)
        {
            alogw("Low probability! use input portIndex[%d] buffer supplier[%d] as final!", nPort, pPortBufSupplier->eBufferSupplier);
            pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        }
        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        ((MM_COMPONENTTYPE*)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        pDemuxData->mInputPortTunnelFlag = TRUE;
    }

COMP_CMD_FAIL:
    pthread_mutex_unlock(&pDemuxData->mStateLock);

    return eError;
}


/*****************************************************************************/
ERRORTYPE DemuxGetState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_STATETYPE* pState)
{
    ERRORTYPE eError = SUCCESS;
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pDemuxData->mStateLock);
    *pState = pDemuxData->state;
    pthread_mutex_unlock(&pDemuxData->mStateLock);

    return eError;
}

ERRORTYPE DemuxGetPortParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_PORT_PARAM_TYPE *pPortParam)
{
    ERRORTYPE eError = SUCCESS;
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    memcpy(pPortParam, &pDemuxData->sPortParam, sizeof(COMP_PORT_PARAM_TYPE));
    return SUCCESS;
}

ERRORTYPE DemuxGetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<DEMUX_MAX_PORT_COUNT; i++)
    {
        if(pDemuxData->sPortDef[i].nPortIndex == pPortDef->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(pPortDef, &pDemuxData->sPortDef[i], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_DEMUX_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE DemuxSetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<DEMUX_MAX_PORT_COUNT; i++)
    {
        if(pDemuxData->sPortDef[i].nPortIndex == pPortDef->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(&pDemuxData->sPortDef[i], pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_DEMUX_ILLEGAL_PARAM;
    }

    return eError;
}

ERRORTYPE DemuxGetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<DEMUX_MAX_PORT_COUNT; i++)
    {
        if(pDemuxData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(pPortBufSupplier, &pDemuxData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_DEMUX_ILLEGAL_PARAM;
    }

    return eError;
}

ERRORTYPE DemuxSetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<DEMUX_MAX_PORT_COUNT; i++)
    {
        if(pDemuxData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(&pDemuxData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_DEMUX_ILLEGAL_PARAM;
    }

    return eError;
}

ERRORTYPE DemuxSetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MPP_CHN_S *pChn)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(&pDemuxData->mMppChnInfo, pChn);
    return SUCCESS;
}

ERRORTYPE DemuxGetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT MPP_CHN_S *pChn)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(pChn, &pDemuxData->mMppChnInfo);
    return SUCCESS;
}

ERRORTYPE DemuxSetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN DEMUX_CHN_ATTR_S *pChnAttr)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pDemuxData->mDemuxChnAttr = *pChnAttr;
    pDemuxData->disable_track = pChnAttr->mDemuxDisableTrack;
    return SUCCESS;
}

ERRORTYPE DemuxGetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT DEMUX_CHN_ATTR_S *pChnAttr)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pChnAttr = pDemuxData->mDemuxChnAttr;
    return SUCCESS;
}

static ERRORTYPE DemuxGetDemuxType(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT int *pDemuxType)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pDemuxType = pDemuxData->demux_type;
    return SUCCESS;
}

static ERRORTYPE DemuxGetMediaInfo(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT CdxMediaInfoT *pMediaInfoT)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pDemuxData->cdx_mediainfo.program[0].videoIndex = pDemuxData->mCurVideoStreamIndex;
    pDemuxData->cdx_mediainfo.program[0].audioIndex = pDemuxData->mCurAudioStreamIndex;
    pDemuxData->cdx_mediainfo.program[0].subtitleIndex = pDemuxData->mCurSubtitleStreamIndex;
    memcpy(pMediaInfoT, &pDemuxData->cdx_mediainfo, sizeof(CdxMediaInfoT));
    return SUCCESS;
}

static ERRORTYPE DemuxGetMediaType(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT int *pMediaType)
{
    //DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pMediaType = CEDARX_MEDIATYPE_NORMAL;
    return SUCCESS;
}

static ERRORTYPE DemuxGetPortExtraDef(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT COMP_PARAM_PORTEXTRADEFINITIONTYPE *pPortDef)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    int portIdx = pPortDef->nPortIndex;
    if (portIdx < pDemuxData->sPortParam.nPorts)
    {
        memcpy(pPortDef, &pDemuxData->sPortExtraDef[portIdx], sizeof(COMP_PARAM_PORTEXTRADEFINITIONTYPE));
        return TRUE;
    }
    else
    {
        return FAILURE;
    }
}

static ERRORTYPE DemuxGetOutputBuffer(PARAM_IN COMP_HANDLETYPE hComponent,
                                      PARAM_OUT EncodedStream *pDmxOutBuf,
                                      PARAM_IN int nMilliSec)
{
    ERRORTYPE eError;
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pDmxOutBuf == NULL)
    {
       aloge("error input func param");
       return ERR_DEMUX_ILLEGAL_PARAM;
    }

    pthread_mutex_lock(&pDemuxData->mStateLock);
    if (pDemuxData->dmx_eof)
    {
        pthread_mutex_unlock(&pDemuxData->mStateLock);
        return DBG_DEMUX_FILE_PARSER_COMPLETE;
    }

    if(COMP_StateIdle != pDemuxData->state && COMP_StateExecuting != pDemuxData->state)
    {
        alogw("call getbuf in wrong state[0x%x]", pDemuxData->state);
        pthread_mutex_unlock(&pDemuxData->mStateLock);
        return ERR_DEMUX_NOT_PERM;
    }

    if (  pDemuxData->mOutputPortTunnelFlag[pDemuxData->audio_port_idx]
       && pDemuxData->mOutputPortTunnelFlag[pDemuxData->video_port_idx]
       )
    {
        aloge("fatal error! can't call getoutbuf in tunnel mode!");
        pthread_mutex_unlock(&pDemuxData->mStateLock);
        return ERR_DEMUX_NOT_PERM;
    }

    int port_idx = -1;
    //int audio_pkt_cnt = 0;
    //int video_pkt_cnt = 0;
    //int subtitle_pkt_cnt = 0;
    DMXPKT_NODE_T *pEntry = NULL;
    int64_t nMinPts = -1;
    BUFFER_MNG* pBufferMng = NULL;
    if (  pDemuxData->sPortDef[pDemuxData->audio_port_idx].bEnabled == TRUE
       && !pDemuxData->mOutputPortTunnelFlag[pDemuxData->audio_port_idx]
       )
    {
        pBufferMng = &pDemuxData->mBufferMng[pDemuxData->audio_port_idx];
        //struct list_head *pList;
        //list_for_each(pList, &pBufferMng->readyPktList) { audio_pkt_cnt++; }
        if(!list_empty(&pBufferMng->readyPktList))
        {
            pEntry = list_first_entry(&pBufferMng->readyPktList, DMXPKT_NODE_T, mList);
            nMinPts = pEntry->stEncodedStream.nTimeStamp;
        }
        port_idx = pDemuxData->audio_port_idx;
    }
    if (  pDemuxData->sPortDef[pDemuxData->video_port_idx].bEnabled == TRUE
       && !pDemuxData->mOutputPortTunnelFlag[pDemuxData->video_port_idx]
       )
    {
        pBufferMng = &pDemuxData->mBufferMng[pDemuxData->video_port_idx];
//        struct list_head *pList;
//        list_for_each(pList, &pBufferMng->readyPktList) { video_pkt_cnt++; }
//        if (video_pkt_cnt > audio_pkt_cnt)
//        {
//            port_idx = pDemuxData->video_port_idx;
//        }
        if(!list_empty(&pBufferMng->readyPktList))
        {
            pEntry = list_first_entry(&pBufferMng->readyPktList, DMXPKT_NODE_T, mList);
            if((-1==nMinPts) || (pEntry->stEncodedStream.nTimeStamp < nMinPts))
            {
                nMinPts = pEntry->stEncodedStream.nTimeStamp;
                port_idx = pDemuxData->video_port_idx;
            }
        }
        if(-1 == port_idx)
        {
            port_idx = pDemuxData->video_port_idx;
        }
    }
    if (  pDemuxData->sPortDef[pDemuxData->subtitle_port_idx].bEnabled == TRUE
       && !pDemuxData->mOutputPortTunnelFlag[pDemuxData->subtitle_port_idx]
       )
    {
        pBufferMng = &pDemuxData->mBufferMng[pDemuxData->subtitle_port_idx];
//        struct list_head *pList;
//        list_for_each(pList, &pBufferMng->readyPktList) { subtitle_pkt_cnt++; }
//        if (subtitle_pkt_cnt > video_pkt_cnt && subtitle_pkt_cnt > audio_pkt_cnt)
//        {
//            port_idx = pDemuxData->subtitle_port_idx;
//        }
        if(!list_empty(&pBufferMng->readyPktList))
        {
            pEntry = list_first_entry(&pBufferMng->readyPktList, DMXPKT_NODE_T, mList);
            if((-1==nMinPts) || (pEntry->stEncodedStream.nTimeStamp < nMinPts))
            {
                nMinPts = pEntry->stEncodedStream.nTimeStamp;
                port_idx = pDemuxData->subtitle_port_idx;
            }
        }
        if(-1 == port_idx)
        {
            port_idx = pDemuxData->subtitle_port_idx;
        }
    }
    if (port_idx == -1)
    {
        aloge("fatal error! can't find port!");
        pthread_mutex_unlock(&pDemuxData->mStateLock);
        return ERR_DEMUX_NOT_PERM;
    }

    pBufferMng = &pDemuxData->mBufferMng[port_idx];
    pthread_mutex_lock(&pBufferMng->mPktlistMutex);
_TryToGetOutBuf:
    if(!list_empty(&pBufferMng->readyPktList))
    {
        DMXPKT_NODE_T *pPktOut = list_first_entry(&pBufferMng->readyPktList, DMXPKT_NODE_T, mList);
        memcpy(pDmxOutBuf, &pPktOut->stEncodedStream, sizeof(EncodedStream));
        list_move_tail(&pPktOut->mList, &pBufferMng->usingPktList);
        eError = SUCCESS;
    }
    else
    {
        if(0 == nMilliSec)
        {
            eError = ERR_DEMUX_NOBUF;
        }
        else if(nMilliSec < 0)
        {
            pBufferMng->mWaitReadyPktFlag = TRUE;
            while(list_empty(&pBufferMng->readyPktList))
            {
                pthread_cond_wait(&pBufferMng->mWaitReadyCondition, &pBufferMng->mPktlistMutex);
            }
            pBufferMng->mWaitReadyPktFlag = FALSE;
            goto _TryToGetOutBuf;
        }
        else
        {
            pBufferMng->mWaitReadyPktFlag = TRUE;
            eError = pthread_cond_wait_timeout(&pBufferMng->mWaitReadyCondition, &pBufferMng->mPktlistMutex, nMilliSec);
            if(ETIMEDOUT == eError)
            {
                alogv("wait output dmxpkt timeout[%d]ms, ret[%d]", nMilliSec, eError);
                eError = ERR_DEMUX_NOBUF;
                pBufferMng->mWaitReadyPktFlag = FALSE;
            }
            else if(0 == eError)
            {
                pBufferMng->mWaitReadyPktFlag = FALSE;
                goto _TryToGetOutBuf;
            }
            else
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", eError);
                eError = ERR_DEMUX_NOBUF;
                pBufferMng->mWaitReadyPktFlag = FALSE;
            }
        }
    }
    pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
    pthread_mutex_unlock(&pDemuxData->mStateLock);
    return eError;
}

static ERRORTYPE DemuxReleaseOutputBuffer(PARAM_IN COMP_HANDLETYPE hComponent,
                                          PARAM_OUT EncodedStream *pDmxOutBuf)
{
    ERRORTYPE eError;
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pDmxOutBuf == NULL)
    {
        aloge("error input func param");
        return ERR_DEMUX_ILLEGAL_PARAM;
    }

    // Get port properties
    int port_idx = -1;
    if (CDX_PacketVideo == pDmxOutBuf->media_type)
        port_idx = pDemuxData->video_port_idx;
    else if (CDX_PacketAudio == pDmxOutBuf->media_type)
        port_idx = pDemuxData->audio_port_idx;
    else if (CDX_PacketSubtitle == pDmxOutBuf->media_type)
        port_idx = pDemuxData->subtitle_port_idx;
    else
    {
        aloge("fatal error! unknown media_type[%d]", pDmxOutBuf->media_type);
        return ERR_DEMUX_ILLEGAL_PARAM;
    }

    COMP_PARAM_PORTDEFINITIONTYPE* pPortDef = &pDemuxData->sPortDef[port_idx];
    COMP_INTERNAL_TUNNELINFOTYPE* pTunnelInfo = &pDemuxData->sPortTunnelInfo[port_idx];
    COMP_PARAM_BUFFERSUPPLIERTYPE* pBufSupplier = &pDemuxData->sPortBufSupplier[port_idx];
    BUFFER_MNG* pBufMng = &pDemuxData->mBufferMng[port_idx];

    pthread_mutex_lock(&pDemuxData->mStateLock);

    if (COMP_StateIdle != pDemuxData->state && COMP_StateExecuting != pDemuxData->state)
    {
        alogw("call release buf in wrong state[0x%x]", pDemuxData->state);
        pthread_mutex_unlock(&pDemuxData->mStateLock);
        return ERR_DEMUX_NOT_PERM;
    }

    if (pDemuxData->mOutputPortTunnelFlag[port_idx])
    {
        aloge("fatal error! can't call release buf() in tunnel mode!");
        pthread_mutex_unlock(&pDemuxData->mStateLock);
        return ERR_DEMUX_NOT_PERM;
    }

    pthread_mutex_lock(&pBufMng->mPktlistMutex);
    if (!list_empty(&pBufMng->usingPktList))
    {
        DMXPKT_NODE_T *pPktOut = list_first_entry(&pBufMng->usingPktList, DMXPKT_NODE_T, mList);
        if (pPktOut->stEncodedStream.pBuffer == pDmxOutBuf->pBuffer && pPktOut->stEncodedStream.nBufferLen == pDmxOutBuf->nBufferLen)
        {
            list_move_tail(&pPktOut->mList, &pBufMng->idlePktList);
            if (pBufMng->mWaitIdlePktFlag)
            {
                pthread_cond_signal(&pBufMng->mWaitIdleCondition);
            }
            eError = SUCCESS;
        }
        else
        {
            aloge("fatal error! release buf[%p][%p] is not match usingPktList first entry[%p][%p]",
                pPktOut->stEncodedStream.pBuffer, pPktOut->stEncodedStream.pBufferExtra,
                pDmxOutBuf->pBuffer, pDmxOutBuf->pBufferExtra);
            eError = ERR_DEMUX_ILLEGAL_PARAM;
        }
    }
    else
    {
        alogw("Be careful! buf not find, maybe reset channel before call this function?");
        eError = ERR_DEMUX_ILLEGAL_PARAM;
    }
    pthread_mutex_unlock(&pBufMng->mPktlistMutex);
    pthread_mutex_unlock(&pDemuxData->mStateLock);
    return eError;
}

/*
// demux not need it?
ERRORTYPE DemuxGetChannelFd(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT int *pChnFd)
{
    DEMUXDATATYPE *pDemuxEncData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pChnFd = pDemuxEncData->mOutputFrameNotifyPipeFds[0];
    return SUCCESS;
}
*/

ERRORTYPE DemuxGetTunnelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError;

    int portIdx = pTunnelInfo->nPortIndex;
    if (portIdx < pDemuxData->sPortParam.nPorts)
    {
        memcpy(pTunnelInfo, &pDemuxData->sPortTunnelInfo[portIdx], sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
        eError = TRUE;
    }
    else
    {
        eError = ERR_DEMUX_UNEXIST;
    }
    return eError;
}

static ERRORTYPE DemuxSelectVideoStream(
        PARAM_IN COMP_HANDLETYPE hCOmponent,
        PARAM_IN int nVideoIndex)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hCOmponent)->pComponentPrivate);
    ERRORTYPE eError;

    if (nVideoIndex > 0
        && nVideoIndex < pDemuxData->cdx_mediainfo.program[pDemuxData->cdx_mediainfo.programIndex].videoNum
        && nVideoIndex < VIDEO_STREAM_LIMIT)
    {
        pDemuxData->mCurVideoStreamIndex = nVideoIndex;
        //update video vencdor info
        for (int i = 0; i < DEMUX_MAX_PORT_COUNT; i++)
        {
            if (COMP_PortDomainVideo == pDemuxData->sPortDef[i].eDomain)
            {
                pDemuxData->sPortExtraDef[i].pVendorInfo =
                    &pDemuxData->cdx_mediainfo.program[pDemuxData->cdx_mediainfo.programIndex].video[nVideoIndex];
                eError = SUCCESS;
                break;
            }
        }
        pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_SWITCH_VIDEO, 0, (void*)&pDemuxData->mCurVideoStreamIndex);
    }
    else
    {
        alogw("use default video stream track[%d]", pDemuxData->mCurVideoStreamIndex);
    }
    return eError;
}

static ERRORTYPE DemuxSelectAudioStream(
        PARAM_IN COMP_HANDLETYPE hCOmponent,
        PARAM_IN int nAudioIndex)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hCOmponent)->pComponentPrivate);
    ERRORTYPE eError;

    if (nAudioIndex > 0 &&
        nAudioIndex < pDemuxData->cdx_mediainfo.program[pDemuxData->cdx_mediainfo.programIndex].audioNum)
    {
        for (int i = 0; i < DEMUX_MAX_PORT_COUNT; i++)
        {
            pDemuxData->mCurAudioStreamIndex = nAudioIndex;
            //update audio Vendor info
            if (COMP_PortDomainAudio == pDemuxData->sPortDef[i].eDomain)
            {
                pDemuxData->sPortExtraDef[i].pVendorInfo =
                    &pDemuxData->cdx_mediainfo.program[pDemuxData->cdx_mediainfo.programIndex].audio[nAudioIndex];
                eError = SUCCESS;
                break;
            }
        }
    }
    else
    {
        alogw("use default audio stream track[%d]", pDemuxData->mCurAudioStreamIndex);
    }
    return eError;
}

static ERRORTYPE DemuxSelectSubtitleStream(
        PARAM_IN COMP_HANDLETYPE hCOmponent,
        PARAM_IN int nSubtitleIndex)
{
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hCOmponent)->pComponentPrivate);
    ERRORTYPE eError;

    if (nSubtitleIndex > 0 &&
        nSubtitleIndex < pDemuxData->cdx_mediainfo.program[pDemuxData->cdx_mediainfo.programIndex].subtitleNum)
    {
        for (int i = 0; i < DEMUX_MAX_PORT_COUNT; i++)
        {
            pDemuxData->mCurSubtitleStreamIndex = nSubtitleIndex;
            //update subtitle Vendor info
            if (COMP_PortDomainSubtitle == pDemuxData->sPortDef[i].eDomain)
            {
                pDemuxData->sPortExtraDef[i].pVendorInfo =
                    &pDemuxData->cdx_mediainfo.program[pDemuxData->cdx_mediainfo.programIndex].subtitle[nSubtitleIndex];
                eError = SUCCESS;
                break;
            }
        }
    }
    else
    {
        alogw("use default subtitle stream track[%d]", pDemuxData->mCurAudioStreamIndex);
    }
    return eError;
}

/*****************************************************************************/
ERRORTYPE DemuxGetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;

    switch(nIndex)
    {
        case COMP_IndexVendorGetPortParam:
        {
            eError = DemuxGetPortParam(hComponent, (COMP_PORT_PARAM_TYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamPortDefinition:
        {
            eError = DemuxGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = DemuxGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = DemuxGetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
#if 0
        case COMP_IndexVendorMPPChannelFd:
        {
            eError = DemuxGetChannelFd(hComponent, (int*)pComponentConfigStructure);
            break;
        }
#endif
        case COMP_IndexVendorTunnelInfo:
        {
            eError = DemuxGetTunnelInfo(hComponent, (COMP_INTERNAL_TUNNELINFOTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorDemuxChnAttr:
        {
            eError = DemuxGetChnAttr(hComponent, (DEMUX_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorDemuxChnPriority:
        {
            alogw("unsupported temporary get demux chn priority!");
            eError = ERR_DEMUX_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorDemuxType:
        {
            eError = DemuxGetDemuxType(hComponent, (int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorDemuxMediaInfo:
        {
            eError = DemuxGetMediaInfo(hComponent, (CdxMediaInfoT*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorDemuxMediaType:
        {
            eError = DemuxGetMediaType(hComponent, (int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorParamPortExtraDefinition:
        {
            eError = DemuxGetPortExtraDef(hComponent, (COMP_PARAM_PORTEXTRADEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorDemuxOutBuffer:
        {
            DemuxStream *pStream = (DemuxStream *)pComponentConfigStructure;
            eError = DemuxGetOutputBuffer(hComponent, pStream->pEncodedStream, pStream->nMilliSec);
            break;
        }
        default:
        {
            aloge("fatal error! unknown getConfig Index[0x%x]", nIndex);
            eError = ERR_DEMUX_ILLEGAL_PARAM;
            break;
        }
    }
    return eError;
}

/*****************************************************************************/
ERRORTYPE DemuxSetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = DemuxSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = DemuxSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = DemuxSetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorDemuxChnAttr:
        {
            eError = DemuxSetChnAttr(hComponent, (DEMUX_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorDemuxChnPriority:
        {
            alogw("not impl SetChnPriority!");
            eError = ERR_DEMUX_NOT_SUPPORT;
            break;
        }
        //case COMP_IndexVendorDemuxResetChannel:
        //{
        //    alogw("not impl ResetChannel!");
        //    //eError = DemuxResetChannel(hComponent);
        //    eError = ERR_DEMUX_NOT_SUPPORT;
        //    break;
        //}
        case COMP_IndexVendorDemuxSetDataSource:
        {
            eError = DemuxsetDataSource(hComponent);
            break;
        }
        //case COMP_IndexVendorDemuxDisableTrack:
        //{
        //    eError = DemuxDisableTrack(hComponent, *(int*)pComponentConfigStructure);
        //    break;
        //}
        //case COMP_IndexVendorDemuxDisableMediaType:
        //{
        //    eError = DemuxDisableMediaType(hComponent, *(int*)pComponentConfigStructure);
        //    break;
        //}
        //case COMP_IndexVendorDemuxDisableProprityTrack:
        //{
        //    aloge("not impl DisableProrityTrack!");
        //    eError = ERR_DEMUX_NOT_SUPPORT;
        //    break;
        //}
        case COMP_IndexVendorDemuxPreparePorts:
        {
            eError = DemuxPreparePorts(hComponent, pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorSeekToPosition:
        {
            eError = DemuxSeekToPosition(hComponent, (CedarXSeekPara*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorDemuxOutBuffer:
        {
            eError = DemuxReleaseOutputBuffer(hComponent, (EncodedStream*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorSwitchVideo:
        {
            eError = DemuxSelectVideoStream(hComponent, *(int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorSwitchAudio:
        {
            eError = DemuxSelectAudioStream(hComponent, *(int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorSwitchSubtitle:
        {
            eError = DemuxSelectSubtitleStream(hComponent, *(int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorDemuxSwitchDateSource:
        {
            eError = DemuxSwitchDataSource(hComponent, (DEMUX_CHN_ATTR_S *)pComponentConfigStructure);
            break;
        }
        default:
        {
            aloge("unknown Index[0x%x]", nIndex);
            eError = ERR_DEMUX_ILLEGAL_PARAM;
            break;
        }
    }

    return eError;
}


/*****************************************************************************/
ERRORTYPE DemuxComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    DEMUXDATATYPE           *pDemuxData;
    ERRORTYPE               eError;
    CompInternalMsgType     eCmd;
    message_t               msg;

    pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    eError     = SUCCESS;
    eCmd       = Stop;

    msg.command = eCmd;
    put_message(&pDemuxData->cmd_queue, &msg);

    alogd("wait demux component exit!...");
    // Wait for thread to exit
    pthread_join(pDemuxData->thread_id, (void*) &eError);

    if(pDemuxData->cdx_epdk_dmx != NULL)
    {
        pDemuxData->cdx_epdk_dmx->close(pDemuxData->cdx_epdk_dmx);
        cedarx_demux_destory(pDemuxData->cdx_epdk_dmx);
    }

    message_destroy(&pDemuxData->cmd_queue);

    clearDataSourceFields(&pDemuxData->datasrc_desc);
    clearCdxMediaInfoT(&pDemuxData->cdx_mediainfo, (int)DemuxType2ParserType(pDemuxData->demux_type));
    pthread_mutex_destroy(&pDemuxData->mStateLock);

    if(pDemuxData->mFd >= 0)
    {
        if(0 == pDemuxData->mFd)
        {
            alogw("Be careful! fd==0!");
        }
        close(pDemuxData->mFd);
        pDemuxData->mFd = -1;
    }

    Demux_ReleaseBufferMng(pDemuxData);

#if DEBUG_SAVE_VIDEO_STREAM
    if(fpVideoStream)
    {
        fclose(fpVideoStream);
        fpVideoStream = NULL;
    }
#endif

    if(pDemuxData)
    {
        free(pDemuxData);
    }

    alogd("demux component exited!");

    return eError;
}


/*****************************************************************************/
ERRORTYPE DemuxComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE*   pComp;
    DEMUXDATATYPE*      pDemuxData;
    ERRORTYPE           eError;
    int                 err;

    eError = SUCCESS;
    pComp  = (MM_COMPONENTTYPE *)hComponent;

    // Create private data
    pDemuxData = (DEMUXDATATYPE *)malloc(sizeof(DEMUXDATATYPE));
    if (NULL == pDemuxData)
    {
       return FAILURE;
    }

    memset(pDemuxData, 0x0, sizeof(DEMUXDATATYPE));
    pComp->pComponentPrivate    = (void*) pDemuxData;
    pDemuxData->state           = COMP_StateLoaded;
    pthread_mutex_init(&pDemuxData->mStateLock, NULL);
    pDemuxData->hSelf           = hComponent;
    pDemuxData->mFd             = -1;

    // Fill in function pointers
    pComp->SetCallbacks         = DemuxSetCallbacks;
    pComp->SendCommand          = DemuxSendCommand;
    pComp->GetState             = DemuxGetState;
    pComp->GetConfig            = DemuxGetConfig;
    pComp->SetConfig            = DemuxSetConfig;
    pComp->ComponentTunnelRequest = DemuxComponentTunnelRequest;
    pComp->ComponentDeInit      = DemuxComponentDeInit;
    pComp->FillThisBuffer       = DemuxFillThisBuffer;
    pComp->EmptyThisBuffer      = DemuxEmptyThisBuffer;

    // Initialize component data structures to default values
    pDemuxData->sPortParam.nPorts = 0; //no ports currently
    pDemuxData->sPortParam.nStartPortNumber = 0x0;

	/* To initialize some info that indicating the attribute of stream to
	 fix bug that system crash when to play file that created by app but terminated by kill cmd.
	 pls chk the beggining point of function Demux_ReleaseBufferMng(),where the variable valid_port_num
	 will get incorrect value,because of the initialization  of variable showing below.*/

    pDemuxData->video_port_idx              = -1;
    pDemuxData->audio_port_idx              = -1;
    pDemuxData->subtitle_port_idx           = -1;
    pDemuxData->max_send_packet_time_video  = -1;
    pDemuxData->max_send_packet_time_audio  = -1;
    pDemuxData->max_send_packet_time_subtitle = -1;

    if(message_create(&pDemuxData->cmd_queue)<0)
    {
        aloge("message error!");
        eError = ERR_DEMUX_NOMEM;
        goto err_out1;
    }

    // Create the component thread
    err = pthread_create(&pDemuxData->thread_id, NULL, ComponentThread, pDemuxData);
    if (err || !pDemuxData->thread_id)
    {
        eError = ERR_DEMUX_NOMEM;
        goto err_out2;
    }

#if DEBUG_SAVE_VIDEO_STREAM
    fpVideoStream = fopen(fpVideoStreamPath, "wb");
    if(NULL == fpVideoStream)
    {
        aloge("fatal error! fopen[%s] fail", fpVideoStreamPath);
    }
#endif

EXIT:
    return eError;

err_out2:
    message_destroy(&pDemuxData->cmd_queue);
err_out1:
    pthread_mutex_destroy(&pDemuxData->mStateLock);
    free(pDemuxData);
    return FAILURE;
}

ERRORTYPE DemuxEmptyThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    ERRORTYPE eError = SUCCESS;

    alogd("fatal error! DemuxEmptyThisBuffer.");

    return eError;
}

ERRORTYPE DemuxFillThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    ERRORTYPE eError = SUCCESS;

    // Get component private data
    DEMUXDATATYPE *pDemuxData = (DEMUXDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    EncodedStream stEncodedStream = *(EncodedStream*)pBuffer->pOutputPortPrivate;
    if (NULL == stEncodedStream.pBuffer)
    {
        aloge("fatal error! call FillThisBuffer using the buffer is NULL!");
        return ERR_DEMUX_NOBUF;
    }

    // Get port properties
    int port_idx = -1;
    if (CDX_PacketVideo == stEncodedStream.media_type)
        port_idx = pDemuxData->video_port_idx;
    else if (CDX_PacketAudio == stEncodedStream.media_type)
        port_idx = pDemuxData->audio_port_idx;
    else if (CDX_PacketSubtitle == stEncodedStream.media_type)
        port_idx = pDemuxData->subtitle_port_idx;
    else
    {
        aloge("fatal error! unknown media_type[%d]", stEncodedStream.media_type);
        return ERR_DEMUX_ILLEGAL_PARAM;
    }

    COMP_PARAM_PORTDEFINITIONTYPE* pPortDef = &pDemuxData->sPortDef[port_idx];
    COMP_INTERNAL_TUNNELINFOTYPE* pTunnelInfo = &pDemuxData->sPortTunnelInfo[port_idx];
    COMP_PARAM_BUFFERSUPPLIERTYPE* pBufSupplier = &pDemuxData->sPortBufSupplier[port_idx];
    BUFFER_MNG* pBufMng = &pDemuxData->mBufferMng[port_idx];

    pthread_mutex_lock(&pDemuxData->mStateLock);

    // Check state
    if (pDemuxData->state != COMP_StateExecuting)
    {
        alogw("Be careful! Call DemuxFillThisBuffer in state[0x%x]!", pDemuxData->state);
        pthread_mutex_unlock(&pDemuxData->mStateLock);
        return ERR_DEMUX_NOT_PERM;
    }

    // Ensure it is outport tunnel mode
    if (!pDemuxData->mOutputPortTunnelFlag[port_idx])
    {
        aloge("fatal error! can't call DemuxFillThisBuffer in non-tunnel mode!");
        pthread_mutex_unlock(&pDemuxData->mStateLock);
        return ERR_DEMUX_NOT_SUPPORT;
    }

    if (pBufSupplier->eBufferSupplier == COMP_BufferSupplyOutput)
    {
        pthread_mutex_lock(&pBufMng->mPktlistMutex);
        if (!list_empty(&pBufMng->usingPktList))
        {
            BOOL bFindFlag = FALSE;
            DMXPKT_NODE_T *pEntry;
            list_for_each_entry(pEntry, &pBufMng->usingPktList, mList)
            {
                if(  pEntry->stEncodedStream.nID == stEncodedStream.nID
                  && pEntry->stEncodedStream.pBuffer == stEncodedStream.pBuffer
                  && pEntry->stEncodedStream.nBufferLen == stEncodedStream.nBufferLen
                  )
                {
                    bFindFlag = TRUE;
                    break;
                }
            }
            if (bFindFlag)
            {
                list_move_tail(&pEntry->mList, &pBufMng->idlePktList);
                if (pBufMng->mWaitIdlePktFlag)
                {
                    pthread_cond_signal(&pBufMng->mWaitIdleCondition);
                }
            }
            else
            {
                aloge("fatal error! not find buf[ID=%d][%p][%p] in used list."
                    ,stEncodedStream.nID
                    ,stEncodedStream.pBuffer
                    ,stEncodedStream.pBufferExtra
                    );
                pthread_mutex_unlock(&pBufMng->mPktlistMutex);
                pthread_mutex_unlock(&pDemuxData->mStateLock);
                return ERR_DEMUX_UNEXIST;
            }
        }
        else
        {
            alogw("Be careful! buf not find, maybe reset channel before call this function?");
        }
        pthread_mutex_unlock(&pBufMng->mPktlistMutex);
    }
    else if (pBufSupplier->eBufferSupplier == COMP_BufferSupplyInput)
    {
        pthread_mutex_lock(&pBufMng->mPktlistMutex);

_TryToGetIdlePktNode:
        if (!list_empty(&pBufMng->idlePktList))
        {
            // Record buf from B component to idlePktList
            DMXPKT_NODE_T *pPktIdleNode = list_first_entry(&pBufMng->idlePktList, DMXPKT_NODE_T, mList);

            pPktIdleNode->stEncodedStream.media_type      = stEncodedStream.media_type;
            pPktIdleNode->stEncodedStream.pBuffer         = stEncodedStream.pBuffer;
            pPktIdleNode->stEncodedStream.nBufferLen      = stEncodedStream.nBufferLen;
            pPktIdleNode->stEncodedStream.pBufferExtra    = stEncodedStream.pBufferExtra;
            pPktIdleNode->stEncodedStream.nBufferExtraLen = stEncodedStream.nBufferExtraLen;

            alogv("DemuxFillThisBuffer media_type[%d] pBuffer[%p] nBufferLen[%d] pBufferExtra[%p] nBufferExtraLen[%d]"
                 ,stEncodedStream.media_type
                 ,pPktIdleNode->stEncodedStream.pBuffer
                 ,pPktIdleNode->stEncodedStream.nBufferLen
                 ,pPktIdleNode->stEncodedStream.pBufferExtra
                 ,pPktIdleNode->stEncodedStream.nBufferExtraLen
                 );

            list_move_tail(&pPktIdleNode->mList, &pBufMng->fillingPktList);

            // Send message to DemuxCompThread
            message_t cmd_msg;
            if (CDX_PacketVideo == pPktIdleNode->stEncodedStream.media_type)
            {
                cmd_msg.command = DemuxComp_VideoBufferAvailable;
                cmd_msg.para0   = 0;
                if (TRUE == pDemuxData->mWaitVideoBufFlag)
                {
                    pDemuxData->mWaitVideoBufFlag = FALSE;
                }
            }
            else if (CDX_PacketAudio == pPktIdleNode->stEncodedStream.media_type)
            {
                cmd_msg.command = DemuxComp_AudioBufferAvailable;
                cmd_msg.para0   = 0;
                if (TRUE == pDemuxData->mWaitAudioBufFlag)
                {
                    pDemuxData->mWaitAudioBufFlag = FALSE;
                }
            }
            else if (CDX_PacketSubtitle == pPktIdleNode->stEncodedStream.media_type)
            {
                cmd_msg.command = DemuxComp_SubtitleBufferAvailable;
                cmd_msg.para0   = 0;
                if (TRUE == pDemuxData->mWaitSubtitleBufFlag)
                {
                    pDemuxData->mWaitSubtitleBufFlag = FALSE;
                }
            }
            else
            {
                pthread_mutex_unlock(&pBufMng->mPktlistMutex);
                pthread_mutex_unlock(&pDemuxData->mStateLock);
                aloge("fatal error! failed to get the unknow media_type [%d]", pPktIdleNode->stEncodedStream.media_type);
                return ERR_DEMUX_UNEXIST;
            }
            put_message(&pDemuxData->cmd_queue, &cmd_msg);
        }
        else
        {// no idle buffer, create a new node
            alogw("idlePktList Empty, create new node");
            DMXPKT_NODE_T *pNode = (DMXPKT_NODE_T *)malloc(sizeof(DMXPKT_NODE_T));
            if (NULL == pNode)
            {
                pthread_mutex_unlock(&pBufMng->mPktlistMutex);
                pthread_mutex_unlock(&pDemuxData->mStateLock);
                aloge("alloc idle_pkt_node fail");
                return ERR_DEMUX_NOMEM;
            }

            memset(pNode, 0, sizeof(DMXPKT_NODE_T));
            list_add_tail(&pNode->mList, &pBufMng->idlePktList);
            pBufMng->mPktListNodeCnt++;
            goto _TryToGetIdlePktNode;
        }
        pthread_mutex_unlock(&pBufMng->mPktlistMutex);
    }
    else
    {
        aloge("fatal error! Unsupported buffer supplier type %d", pBufSupplier->eBufferSupplier);
    }

    pthread_mutex_unlock(&pDemuxData->mStateLock);
    return eError;
}


#define MAX_BUFFER_PACKET_TIMES 2000000 //unit:us
static void* ComponentThread(void* pThreadData)
{
    CompInternalMsgType         cmd;
    unsigned int                cmddata;
    message_t                   cmd_msg;

    MM_COMPONENTTYPE* hnd_clock_comp = NULL;
    DMXPKT_NODE_T *pNode, *pEntry, *pTmp;

    // Get component private data
    DEMUXDATATYPE* pDemuxData = (DEMUXDATATYPE*)pThreadData;

    alogd("Demux ComponentThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"DemuxComp", 0, 0, 0);

    while (1)
    {
PROCESS_MESSAGE:
        if (get_message(&pDemuxData->cmd_queue, &cmd_msg) == 0)
        {// pump message from message queue

            cmd     = cmd_msg.command;
            cmddata = cmd_msg.para0;

            alogv("Demux ComponentThread get_message cmd:%d", cmd);

            // State transition command
            if (cmd == SetState)
            {
                pthread_mutex_lock(&pDemuxData->mStateLock);
                // If the parameter states a transition to the same state
                // raise a same state transition error.
                pDemuxData->switching_state = 0;
                if (pDemuxData->state == (COMP_STATETYPE)(cmddata))
                {
                    pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf, pDemuxData->pAppData, COMP_EventError, ERR_DEMUX_SAMESTATE, 0, NULL);
                }
                else
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE)(cmddata))
                    {
                    case COMP_StateInvalid:
                        {
                            pDemuxData->state = COMP_StateInvalid;
                            pDemuxData->pCallbacks->EventHandler
                                (pDemuxData->hSelf
                                ,pDemuxData->pAppData
                                ,COMP_EventCmdComplete
                                ,COMP_CommandStateSet
                                ,pDemuxData->state
                                ,NULL
                                );
                            break;
                        }
                    case COMP_StateLoaded:
                        {
                            if (pDemuxData->demux_init_end)
                            {
                                if (pDemuxData->cdx_epdk_dmx != NULL)
                                {
                                    pDemuxData->cdx_epdk_dmx->close(pDemuxData->cdx_epdk_dmx);
                                    cedarx_demux_destory(pDemuxData->cdx_epdk_dmx);
                                    pDemuxData->cdx_epdk_dmx = NULL;
                                }
                                pDemuxData->demux_init_end = 0;
                            }
                            if(pDemuxData->state != COMP_StateIdle)
                            {
                                aloge("fatal error! state[0x%x] is not idle!", pDemuxData->state);
                            }

                            Demux_CheckFillingPktEmpty(pDemuxData);

                            //return all buffers to vdec, adec, etc.
                            Demux_ReturnAllBuffer(pDemuxData);

                            pDemuxData->state = COMP_StateLoaded;
                            pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf, pDemuxData->pAppData, COMP_EventCmdComplete, COMP_CommandStateSet, pDemuxData->state, NULL);
                            break;
                        }
                    case COMP_StateIdle:
                        {
                            if (pDemuxData->state == COMP_StateInvalid)
                            {
                                pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf, pDemuxData->pAppData, COMP_EventError, ERR_DEMUX_INCORRECT_STATE_TRANSITION, pDemuxData->state, NULL);
                            }
                            else
                            {
                                if(0 == pDemuxData->demux_init_end)
                                {
                                    if(DemuxOpenParserLib(pDemuxData) != SUCCESS)
                                    {
                                        int ret = -1;
                                        pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf, pDemuxData->pAppData, COMP_EventCmdComplete, COMP_CommandStateSet, COMP_StateInvalid, (void*)&ret);
                                        break;
                                    }
                                    CreateDemuxPorts(pDemuxData, &pDemuxData->cdx_mediainfo);
                                    pDemuxData->demux_init_end  = 1;
                                }
                                if(COMP_StateLoaded == pDemuxData->state)
                                {
                                }
                                else if(COMP_StatePause == pDemuxData->state || COMP_StateExecuting == pDemuxData->state)
                                {
                                    //return all buffers to vdec, adec, etc.
                                    Demux_ReturnAllBuffer(pDemuxData);
                                }
                                else
                                {
                                    aloge("fatal error! unknown state[0x%x]", pDemuxData->state);
                                }
                                pDemuxData->state = COMP_StateIdle;
                                pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf,
                                                                     pDemuxData->pAppData,
                                                                     COMP_EventCmdComplete,
                                                                     COMP_CommandStateSet,
                                                                     pDemuxData->state,
                                                                     NULL);
                            }
                            break;
                        }
                    case COMP_StateExecuting:
                        {
                            // Transition can only happen from pause or idle state
                            if (pDemuxData->state == COMP_StateIdle || pDemuxData->state == COMP_StatePause)
                            {
                                hnd_clock_comp = (MM_COMPONENTTYPE *)(pDemuxData->sPortTunnelInfo[pDemuxData->clock_port_idx].hTunnel);

                                if (pDemuxData->state == COMP_StateIdle && !pDemuxData->mAllocBufFlag)
                                {
                                    allocDmxOutputBuf(pDemuxData);
                                }

                                pDemuxData->dmx_eof_send = 0;
                                pDemuxData->state = COMP_StateExecuting;
                                pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf,
                                                                     pDemuxData->pAppData,
                                                                     COMP_EventCmdComplete,
                                                                     COMP_CommandStateSet,
                                                                     pDemuxData->state, NULL);
                            }
                            else
                            {
                                pDemuxData->state = COMP_StateIdle;
                                pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf,
                                                                     pDemuxData->pAppData,
                                                                     COMP_EventError,
                                                                     ERR_DEMUX_INCORRECT_STATE_TRANSITION,
                                                                     pDemuxData->state,
                                                                     NULL);
                            }
                            break;
                        }
                    case COMP_StatePause:
                        {
                            // Transition can only happen from idle or executing state
                            if (pDemuxData->state == COMP_StateIdle || pDemuxData->state == COMP_StateExecuting)
                            {
                                pDemuxData->state = COMP_StatePause;
                                pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf,
                                                                     pDemuxData->pAppData,
                                                                     COMP_EventCmdComplete,
                                                                     COMP_CommandStateSet,
                                                                     pDemuxData->state,
                                                                     NULL);
                            }
                            else
                            {
                                aloge("fatal error! state[0x%x]->Pause!", pDemuxData->state);
                                pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf,
                                                                     pDemuxData->pAppData,
                                                                     COMP_EventError,
                                                                     ERR_DEMUX_INCORRECT_STATE_TRANSITION,
                                                                     0,
                                                                     NULL);
                            }
                            Demux_ReturnAllBuffer(pDemuxData);
                            break;
                        }
                    default:
                        {
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&pDemuxData->mStateLock);
            }
            else if (cmd == StopPort)
            {
                //* do nothing.
            }
            else if (cmd == Flush)
            {
                //* do nothing.
            }
            else if (cmd == Stop)
            {
                //* kill the thread.
                goto EXIT;
            }
            else if (cmd == DemuxComp_VideoBufferAvailable)
            {
                //* do nothing.
            }
            else if (cmd == DemuxComp_AudioBufferAvailable)
            {
                //* do nothing.
            }
            else if (cmd == DemuxComp_SubtitleBufferAvailable)
            {
                //* do nothing.
            }

            //precede to process message
            goto PROCESS_MESSAGE;
        } /* End of process message */

        // Check EOF flag
        if (pDemuxData->dmx_eof)
        {
            alogw("[%p]:Demux EOF found! A", pDemuxData->hSelf);
            if (pDemuxData->dmx_eof_send == 0)
            {
                pDemuxData->dmx_eof_send = 1;
                pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf, pDemuxData->pAppData, COMP_EventBufferFlag, 0, 0, NULL);
            }
            TMessage_WaitQueueNotEmpty(&pDemuxData->cmd_queue, 0);
            goto PROCESS_MESSAGE;
        }

        // Ensure it is in Exec state
        if (pDemuxData->state != COMP_StateExecuting)
        {
            TMessage_WaitQueueNotEmpty(&pDemuxData->cmd_queue, 0);
            goto PROCESS_MESSAGE;
        }

        // Get system time
        int64_t system_time;
        if (pDemuxData->mOutputPortTunnelFlag[0])
        {
            if(hnd_clock_comp)
            {
                COMP_TIME_CONFIG_TIMESTAMPTYPE  time_stamp;
                COMP_GetConfig(hnd_clock_comp, COMP_IndexConfigTimeCurrentMediaTime, &time_stamp);
                system_time = time_stamp.nTimestamp;
            }
            else
            {
                system_time = -1;
            }
        }
        else
        {
            system_time = -1;
        }

        // Control read speed
        if(  (system_time == -1)
          || (  (pDemuxData->max_send_packet_time_video - system_time < MAX_BUFFER_PACKET_TIMES)
             || (pDemuxData->max_send_packet_time_audio - system_time < MAX_BUFFER_PACKET_TIMES)
             )
          )
        {
            //nothing to do
        }
        else
        {// read too fast, wait a while
            TMessage_WaitQueueNotEmpty(&pDemuxData->cmd_queue, 200);
            goto PROCESS_MESSAGE;
        }

        // Prefetch media data
        if (pDemuxData->prefetch_done == 0)
        {
            if (pDemuxData->cdx_epdk_dmx->prefetch(pDemuxData->cdx_epdk_dmx, &pDemuxData->cdx_pkt) != 0)
            {// prefetch failed
                int err = pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_GET_STATUS, 0, NULL);
                if (err == PSR_IO_ERR)
                {
                    aloge("IO ERROR");
                }
                else if (err == PSR_USER_CANCEL)
                {
                    aloge("PSR USER CANCEL, do nothing");
                }
                else
                {
                    alogd("getconfig [%p]:Demuxer EOF found! B", pDemuxData->hSelf);
                    pDemuxData->dmx_eof = 1;
                    pDemuxData->dmx_eof_send = 1;
                    pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf,
                                                         pDemuxData->pAppData,
                                                         COMP_EventBufferFlag,
                                                         0,
                                                         0,
                                                         NULL);
            }
                TMessage_WaitQueueNotEmpty(&pDemuxData->cmd_queue, 200);
                goto PROCESS_MESSAGE;
            }
            /*
            alogd("pDemuxData->cdx_pkt.type: %d, pDemuxData->cdx_pkt.streamIndex: %d",
                    pDemuxData->cdx_pkt.type, pDemuxData->cdx_pkt.streamIndex);
            */

            // prefetch success
            pDemuxData->prefetch_done = 1;  // Set prefetch success flag, only prefetch once
        }

        // Get tunnel_port_idx and nStreamIndex
        int tunnel_port_idx = -1;
        int nStreamIndex    = -1;
        if (pDemuxData->cdx_pkt.type == CDX_MEDIA_VIDEO)
        {// video data
            //alogd("vpkt_streamId:%d, pts:[%lld]ms", pDemuxData->cdx_pkt.streamIndex, pDemuxData->cdx_pkt.pts/1000);
            if (0 == (pDemuxData->disable_track & DEMUX_DISABLE_VIDEO_TRACK))
            {
                tunnel_port_idx = pDemuxData->video_port_idx;
                nStreamIndex    = (pDemuxData->cdx_pkt.flags & MINOR_STREAM) == 0 ? 0 : 1;

                if (pDemuxData->max_send_packet_time_video < pDemuxData->cdx_pkt.pts)
                {
                    pDemuxData->max_send_packet_time_video = pDemuxData->cdx_pkt.pts;
                }
            }
            else
            {
                tunnel_port_idx = -1;
                //alogv("[%d] is not current video stream index, skip", pDemuxData->cdx_pkt.streamIndex);
            }
        }
        else if (pDemuxData->cdx_pkt.type == CDX_MEDIA_AUDIO)
        {
            //alogd("apkt_streamId:%d, pts:[%lld]ms", pDemuxData->cdx_pkt.streamIndex, pDemuxData->cdx_pkt.pts/1000);
            if (  0 == (pDemuxData->disable_track & DEMUX_DISABLE_AUDIO_TRACK)
               && pDemuxData->cdx_pkt.streamIndex == pDemuxData->mCurAudioStreamIndex // mCurAudioStreamIndex for 2 channels
               )
            {
                tunnel_port_idx = pDemuxData->audio_port_idx;
                nStreamIndex    = pDemuxData->cdx_pkt.streamIndex;
                if (pDemuxData->max_send_packet_time_audio < pDemuxData->cdx_pkt.pts)
                {
                    pDemuxData->max_send_packet_time_audio = pDemuxData->cdx_pkt.pts;
                }
            }
            else
            {
                tunnel_port_idx = -1;
                alogv("[%d] is not current audio stream index[%d], skip", pDemuxData->cdx_pkt.streamIndex, pDemuxData->mCurAudioStreamIndex);
            }
        }
        else if (pDemuxData->cdx_pkt.type == CDX_MEDIA_SUBTITLE)
        {
            //alogd("spkt_streamId:%d, pts:[%lld]ms", pDemuxData->cdx_pkt.streamIndex, pDemuxData->cdx_pkt.pts/1000);
            if (  0 == (pDemuxData->disable_track & DEMUX_DISABLE_SUBTITLE_TRACK)
               && pDemuxData->cdx_pkt.streamIndex == pDemuxData->mCurSubtitleStreamIndex
               )
            {
                tunnel_port_idx = pDemuxData->subtitle_port_idx;
                nStreamIndex    = pDemuxData->cdx_pkt.streamIndex;
                if (pDemuxData->max_send_packet_time_subtitle < pDemuxData->cdx_pkt.pts)
                {
                    pDemuxData->max_send_packet_time_subtitle = pDemuxData->cdx_pkt.pts;
                }
            }
            else
            {
                tunnel_port_idx = -1;
                alogv("[%d] is not current subtitle stream index[%d], skip", pDemuxData->cdx_pkt.streamIndex, pDemuxData->mCurSubtitleStreamIndex);
            }
        }
        else
        {
            aloge("fatal error! media type from parser not valid, should not run here, abort().");
            tunnel_port_idx = -1;
        }

        // Ensure tunnel_port_idx is valid
        if (tunnel_port_idx < 0)
        {
            if(CDX_OK!=pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_SKIP_CHUNK_DATA, 0, &pDemuxData->cdx_pkt))
            {
                aloge("fatal error! skip chunk data fail!");
            }
            pDemuxData->prefetch_done = 0;
            goto PROCESS_MESSAGE;
        }

        // Prepare data
        EncodedStream dmxOutBuf;
        memset(&dmxOutBuf, 0, sizeof(EncodedStream));
        dmxOutBuf.nTobeFillLen = pDemuxData->cdx_pkt.length;
        if (pDemuxData->cdx_pkt.type == CDX_MEDIA_VIDEO)
        {
            enum VIDEO_TYPE video_type = (nStreamIndex == 0) ? VIDEO_TYPE_MAJOR : VIDEO_TYPE_MINOR;
            if (video_type == VIDEO_TYPE_MAJOR)
            {
                dmxOutBuf.video_stream_type   = VIDEO_TYPE_MAJOR;   //CDX_VIDEO_STREAM_MAJOR;
            }
            else if (video_type == VIDEO_TYPE_MINOR)
            {
                dmxOutBuf.video_stream_type   = VIDEO_TYPE_MINOR;   //CDX_VIDEO_STREAM_MINOR;
            }
            else
            {
                dmxOutBuf.video_stream_type   = VIDEO_TYPE_NOT_VIDEO; //CDX_VIDEO_STREAM_NONE;
            }
        }

        if (pDemuxData->mOutputPortTunnelFlag[tunnel_port_idx])
        {// outport tunnel mode

            // Get tunnel_info handle
            COMP_INTERNAL_TUNNELINFOTYPE* tunnel_info = &pDemuxData->sPortTunnelInfo[tunnel_port_idx];
            COMP_PARAM_BUFFERSUPPLIERTYPE* pBufSupplier = &pDemuxData->sPortBufSupplier[tunnel_port_idx];
            BUFFER_MNG* pBufferMng = &pDemuxData->mBufferMng[tunnel_port_idx];

            COMP_BUFFERHEADERTYPE omx_buffer_header;
            omx_buffer_header.pOutputPortPrivate = (void*)&dmxOutBuf;
            omx_buffer_header.nInputPortIndex    = tunnel_info->nTunnelPortIndex;

            if (pBufSupplier->eBufferSupplier == COMP_BufferSupplyOutput)
            {
                pthread_mutex_lock(&pBufferMng->mPktlistMutex);
                while (list_empty(&pBufferMng->idlePktList))
                {
                    pBufferMng->mWaitIdlePktFlag = TRUE;
                    pthread_cond_wait(&pBufferMng->mWaitIdleCondition, &pBufferMng->mPktlistMutex);
                }
                pBufferMng->mWaitIdlePktFlag = FALSE;

                BOOL bFindFlag = FALSE;
                list_for_each_entry_safe(pEntry, pTmp, &pBufferMng->idlePktList, mList)
                {
                    if (pEntry->stEncodedStream.nBufferLen >= dmxOutBuf.nTobeFillLen)
                    {
                        bFindFlag = TRUE;
                        break;
                    }
                }

                if (bFindFlag)
                {
                    pNode = pEntry;
                }
                else
                {
                    pNode = list_first_entry(&pBufferMng->idlePktList, DMXPKT_NODE_T, mList);
                    int size = dmxOutBuf.nTobeFillLen;
                    alogv("search and find no node that capacity is big enough, realloc size: %d", size);

                    if (pNode->stEncodedStream.pBuffer != NULL)
                    {
                        free(pNode->stEncodedStream.pBuffer);
                        pNode->stEncodedStream.pBuffer    = NULL;
                        pNode->stEncodedStream.nBufferLen = 0;
                    }
                    pNode->stEncodedStream.nBufferLen = size;
                    pNode->stEncodedStream.pBuffer    = (unsigned char *)malloc(size);
                    if (pNode->stEncodedStream.pBuffer == NULL)
                    {
                        pNode->stEncodedStream.nBufferLen = 0;
                        aloge("malloc %d Bytes fail, skip this pkt", size);
                        if (CDX_OK != pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_SKIP_CHUNK_DATA, 0, &pDemuxData->cdx_pkt))
                        {
                            aloge("fatal error! skip chunk data fail!");
                        }
                        pDemuxData->prefetch_done = 0;
                        pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                        goto PROCESS_MESSAGE;
                    }
                }
                list_move_tail(&pNode->mList, &pBufferMng->fillingPktList);

                pNode->stEncodedStream.media_type        = map_PacketType_To_MediaType(pDemuxData->cdx_pkt.type);
                pNode->stEncodedStream.video_stream_type = dmxOutBuf.video_stream_type;

                pDemuxData->cdx_pkt.buf        = NULL;
                pDemuxData->cdx_pkt.ringBuf    = NULL;
                pDemuxData->cdx_pkt.buflen     = 0;
                pDemuxData->cdx_pkt.ringBufLen = 0;
                pDemuxData->cdx_pkt.buf        = pNode->stEncodedStream.pBuffer;
                if(pNode->stEncodedStream.nBufferLen >= pDemuxData->cdx_pkt.length)
                {
                    pDemuxData->cdx_pkt.buflen = pDemuxData->cdx_pkt.length;
                }
                else
                {
                    pDemuxData->cdx_pkt.buflen     = pNode->stEncodedStream.nBufferLen;
                    pDemuxData->cdx_pkt.ringBuf    = pNode->stEncodedStream.pBufferExtra;
                    pDemuxData->cdx_pkt.ringBufLen = pDemuxData->cdx_pkt.length - pNode->stEncodedStream.nBufferLen;
                }
            }
            else if (pBufSupplier->eBufferSupplier == COMP_BufferSupplyInput)
            {
                // Find correspond media data buffer
                BOOL bFindFlag;
                BOOL bBufferEnough;
_TryToFindBuffer:
                bFindFlag     = FALSE;
                bBufferEnough = FALSE;
                pthread_mutex_lock(&pBufferMng->mPktlistMutex);
                list_for_each_entry_safe(pEntry, pTmp, &pBufferMng->fillingPktList, mList)
                {
                    bFindFlag     = FALSE;
                    bBufferEnough = FALSE;

                    int media_type = map_PacketType_To_MediaType(pDemuxData->cdx_pkt.type);
                    if (pEntry->stEncodedStream.media_type == media_type)
                    {
                        bFindFlag = TRUE;
                        if ((pEntry->stEncodedStream.nBufferLen + pEntry->stEncodedStream.nBufferExtraLen) >= dmxOutBuf.nTobeFillLen)
                        {
                            bBufferEnough = TRUE;
                        }
                        break;
                    }
                }

                if (bFindFlag && bBufferEnough)
                {
                    pNode = pEntry;
                }
                else if (bFindFlag && !bBufferEnough)
                {
                    alogv("find that node buffer capacity is not big enough, request size: %d", dmxOutBuf.nTobeFillLen);
                    pNode = pEntry;
                    dmxOutBuf.nFilledLen    = -1;  // indicate buffer is not enough. requir length is nTobeFillLen
                    dmxOutBuf.pBuffer       = pNode->stEncodedStream.pBuffer;
                    dmxOutBuf.pBufferExtra  = pNode->stEncodedStream.pBufferExtra;
                    pthread_mutex_unlock(&pBufferMng->mPktlistMutex);

                    COMP_EmptyThisBuffer(tunnel_info->hTunnel,  (void*)&omx_buffer_header);

                    pthread_mutex_lock(&pBufferMng->mPktlistMutex);
                    memset(&pNode->stEncodedStream, 0, sizeof(EncodedStream));
                    list_move_tail(&pNode->mList, &pBufferMng->idlePktList);
                    pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                    goto _TryToFindBuffer;
                }
                else if (!bFindFlag)
                {
                    alogv("call Thread  pDemuxData->cdx_pkt.type[%d] mWaitVideoBufFlag[%d] mWaitAudioBufFlag[%d]",
                        pDemuxData->cdx_pkt.type,
                        pDemuxData->mWaitVideoBufFlag,
                        pDemuxData->mWaitAudioBufFlag);

                    // wait stream buffer
                    if (pDemuxData->cdx_pkt.type == CDX_MEDIA_VIDEO)
                    {
                        pDemuxData->mWaitVideoBufFlag = TRUE;
                        pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                        TMessage_WaitQueueNotEmpty(&pDemuxData->cmd_queue, 0);
                        goto PROCESS_MESSAGE;
                    }
                    else if (pDemuxData->cdx_pkt.type == CDX_MEDIA_AUDIO)
                    {
                        pDemuxData->mWaitAudioBufFlag = TRUE;
                        pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                        TMessage_WaitQueueNotEmpty(&pDemuxData->cmd_queue, 0);
                        goto PROCESS_MESSAGE;
                    }
                    else if (pDemuxData->cdx_pkt.type == CDX_MEDIA_SUBTITLE)
                    {
                        pDemuxData->mWaitSubtitleBufFlag = TRUE;
                        pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                        TMessage_WaitQueueNotEmpty(&pDemuxData->cmd_queue, 0);
                        goto PROCESS_MESSAGE;
                    }
                    else
                    {
                        aloge("fatal error! Unknown media_type[%d]", pDemuxData->cdx_pkt.type);
                        pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                        goto PROCESS_MESSAGE;
                    }
                }
                else
                {
                   aloge("fatal error! Should not come here");
                   pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                   goto PROCESS_MESSAGE;
                }

                pDemuxData->cdx_pkt.buf        = NULL;
                pDemuxData->cdx_pkt.ringBuf    = NULL;
                pDemuxData->cdx_pkt.buflen     = 0;
                pDemuxData->cdx_pkt.ringBufLen = 0;
                pDemuxData->cdx_pkt.buf        = pNode->stEncodedStream.pBuffer;
                if(pNode->stEncodedStream.nBufferLen >= pDemuxData->cdx_pkt.length)
                {
                    pDemuxData->cdx_pkt.buflen = pDemuxData->cdx_pkt.length;
                }
                else
                {
                    pDemuxData->cdx_pkt.buflen     = pNode->stEncodedStream.nBufferLen;
                    pDemuxData->cdx_pkt.ringBuf    = pNode->stEncodedStream.pBufferExtra;
                    pDemuxData->cdx_pkt.ringBufLen = pDemuxData->cdx_pkt.length - pNode->stEncodedStream.nBufferLen;
                }
            }
            else
            {
                aloge("fatal error: invalide BufferSupplier type %d", pBufSupplier->eBufferSupplier);
                abort();
            }


            if (pDemuxData->cdx_epdk_dmx->read(pDemuxData->cdx_epdk_dmx, &pDemuxData->cdx_pkt) == CDX_ERROR)
            {// read failed
                int err = pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_GET_STATUS, 0, NULL);
                if(err == PSR_IO_ERR)
                {
                    aloge("IO ERROR");
                }
                else if (err == PSR_USER_CANCEL)
                {
                    /* do noting */
                    alogd("PSR USER CANCEL, do nothing");
                }
                else
                {
                    alogd("getconfig [%p]:Demuxer EOF found! C", pDemuxData->hSelf);
                    pDemuxData->dmx_eof = 1;
                    pDemuxData->dmx_eof_send = 1;
                    pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf,
                                                         pDemuxData->pAppData,
                                                         COMP_EventBufferFlag,
                                                         0,
                                                         0,
                                                         NULL);
                }
                pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                TMessage_WaitQueueNotEmpty(&pDemuxData->cmd_queue, 200);
                goto PROCESS_MESSAGE;
            }

#if DEBUG_SAVE_VIDEO_STREAM
            if (CDX_MEDIA_VIDEO == pDemuxData->cdx_pkt.type)
            {
                if(pDemuxData->cdx_pkt.buflen > 0)
                {
                    fwrite(pDemuxData->cdx_pkt.buf, 1, pDemuxData->cdx_pkt.buflen, fpVideoStream);
                }
                if(pDemuxData->cdx_pkt.ringBufLen > 0)
                {
                    fwrite(pDemuxData->cdx_pkt.ringBuf, 1, pDemuxData->cdx_pkt.ringBufLen, fpVideoStream);
                }
            }
#endif
            // Get ctrl_bits
            int ctrl_bits = 0;
            ctrl_bits |= CEDARV_FLAG_PTS_VALID;
            if (pDemuxData->cdx_pkt.flags & FIRST_PART)
            {
                ctrl_bits |= CEDARV_FLAG_FIRST_PART;
            }
            if (pDemuxData->cdx_pkt.flags & LAST_PART)
            {
                ctrl_bits |= CEDARV_FLAG_LAST_PART;
            }
            if (pDemuxData->cdx_pkt.flags & KEY_FRAME)
            {
                ctrl_bits |= CEDARV_FLAG_KEYFRAME;
            }

            dmxOutBuf.media_type    = map_PacketType_To_MediaType(pDemuxData->cdx_pkt.type);
            dmxOutBuf.nID           = pNode->stEncodedStream.nID;
            dmxOutBuf.nFilledLen    = pDemuxData->cdx_pkt.length;
            dmxOutBuf.nTobeFillLen  = pDemuxData->cdx_pkt.length;
            dmxOutBuf.nTimeStamp    = pDemuxData->cdx_pkt.pts;
            dmxOutBuf.nFlags        = ctrl_bits;

            dmxOutBuf.pBuffer         = pNode->stEncodedStream.pBuffer;
            dmxOutBuf.nBufferLen      = pNode->stEncodedStream.nBufferLen;
            dmxOutBuf.pBufferExtra    = pNode->stEncodedStream.pBufferExtra;
            dmxOutBuf.nBufferExtraLen = pNode->stEncodedStream.nBufferExtraLen;
            //dmxOutBuf.video_stream_type has already been set
            //dmxOutBuf.video_frame_info only needed by venc

            dmxOutBuf.infoVersion         = pDemuxData->cdx_pkt.infoVersion;
            dmxOutBuf.pChangedStreamsInfo = pDemuxData->cdx_pkt.info;
            dmxOutBuf.duration            = pDemuxData->cdx_pkt.duration;

            pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
			alogv("========= media_type[%d](1-video 2-audio) ID[%d] pts[%lld]", dmxOutBuf.media_type, dmxOutBuf.nID, dmxOutBuf.nTimeStamp);

            pthread_mutex_lock(&pBufferMng->mPktlistMutex);
            if (pBufSupplier->eBufferSupplier == COMP_BufferSupplyOutput)
            {
                list_move_tail(&pNode->mList, &pBufferMng->usingPktList);
            }
            else if (pBufSupplier->eBufferSupplier == COMP_BufferSupplyInput)
            {
                memset(&pNode->stEncodedStream, 0, sizeof(EncodedStream));
                list_move_tail(&pNode->mList, &pBufferMng->idlePktList);
            }
            else
            {
                aloge("fatal error! Unsupported buffer suppier type %d", pBufSupplier->eBufferSupplier);
                abort();
            }
            pthread_mutex_unlock(&pBufferMng->mPktlistMutex);

            // Send data to component B
            COMP_EmptyThisBuffer(tunnel_info->hTunnel,  (void*)&omx_buffer_header);

            pDemuxData->prefetch_done = 0;
        } //outporttunnelflag
        else
        {// outport non-tunnel mode
            BUFFER_MNG* pBufferMng = &pDemuxData->mBufferMng[tunnel_port_idx];

            int eWaitRet;
            const int nMilliSec = 1000;   //unit:ms
            pthread_mutex_lock(&pBufferMng->mPktlistMutex);
            while (list_empty(&pBufferMng->idlePktList))
            {
                //alogd("wait idle dmxpkt node");
                pBufferMng->mWaitIdlePktFlag = TRUE;
                eWaitRet = pthread_cond_wait_timeout(&pBufferMng->mWaitIdleCondition, &pBufferMng->mPktlistMutex, nMilliSec);
                if(ETIMEDOUT == eWaitRet)
                {
                    alogv("wait idle dmxpkt timeout[%d]ms, ret[%d]", nMilliSec, eWaitRet);
                    //detect message
                    if(get_message_count(&pDemuxData->cmd_queue) > 0)
                    {
                        alogd("dmx wait idle packet timeout in non-tunnel mode, goto process message.");
                        pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                        goto PROCESS_MESSAGE;
                    }
                }
                else if(0 == eWaitRet)
                {
                    continue;
                }
                else
                {
                    aloge("fatal error! pthread cond wait timeout ret[%d]", eWaitRet);
                }
            }
            pBufferMng->mWaitIdlePktFlag = FALSE;

            BOOL bFindFlag = FALSE;
            list_for_each_entry_safe(pEntry, pTmp, &pBufferMng->idlePktList, mList)
            {
                if (pEntry->stEncodedStream.nBufferLen >= dmxOutBuf.nTobeFillLen)
                {
                    bFindFlag = TRUE;
                    break;
                }
            }

            if (bFindFlag)
            {
                pNode = pEntry;
            }
            else
            {
                pNode = list_first_entry(&pBufferMng->idlePktList,DMXPKT_NODE_T, mList);
                int size = dmxOutBuf.nTobeFillLen;
                alogd("search and find no node that capacity is big enough, realloc size: %d", size);
                if (pNode->stEncodedStream.pBuffer != NULL)
                {
                    free(pNode->stEncodedStream.pBuffer);
                    pNode->stEncodedStream.pBuffer = NULL;
                    pNode->stEncodedStream.nBufferLen = 0;
                }
                pNode->stEncodedStream.nBufferLen = size;
                pNode->stEncodedStream.pBuffer = (unsigned char *)malloc(size);
                if (pNode->stEncodedStream.pBuffer == NULL)
                {
                    pNode->stEncodedStream.nBufferLen = 0;
                    aloge("fatal error! malloc %d Bytes fail, skip this pkt", size);
                    if (CDX_OK!=pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_SKIP_CHUNK_DATA, 0, &pDemuxData->cdx_pkt))
                    {
                        aloge("fatal error! skip chunk data fail!");
                    }
                    pDemuxData->prefetch_done = 0;
                    pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                    goto PROCESS_MESSAGE;
                }
            }
            list_move_tail(&pNode->mList, &pBufferMng->fillingPktList); //in case thread go out, the node is discard, not link to list
            pthread_mutex_unlock(&pBufferMng->mPktlistMutex);

            pNode->stEncodedStream.media_type = map_PacketType_To_MediaType(pDemuxData->cdx_pkt.type);
            pNode->stEncodedStream.video_stream_type = dmxOutBuf.video_stream_type;

            pDemuxData->cdx_pkt.buf        = pNode->stEncodedStream.pBuffer;
            pDemuxData->cdx_pkt.buflen     = pNode->stEncodedStream.nBufferLen;
            pDemuxData->cdx_pkt.ringBuf    = pNode->stEncodedStream.pBufferExtra;
            pDemuxData->cdx_pkt.ringBufLen = pNode->stEncodedStream.nBufferExtraLen;

            if (pDemuxData->cdx_epdk_dmx->read(pDemuxData->cdx_epdk_dmx, &pDemuxData->cdx_pkt) == CDX_ERROR)
            {
                int err = pDemuxData->cdx_epdk_dmx->control(pDemuxData->cdx_epdk_dmx, CDX_DMX_CMD_GET_STATUS, 0, NULL);
                if (err == PSR_IO_ERR)
                {
                    aloge("IO ERROR");
                }
                else if (err == PSR_USER_CANCEL)
                {
                   /* do noting */
                    alogd("PSR USER CANCEL, do nothing");
                }
                else
                {
                    alogd("getconfig [%p]:Demuxer EOF found! C", pDemuxData->hSelf);
                    pDemuxData->dmx_eof = 1;
                    pDemuxData->dmx_eof_send = 1;
                    pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf,
                                                        pDemuxData->pAppData,
                                                        COMP_EventBufferFlag,
                                                        0,
                                                        0,
                                                        NULL);

                }

                pthread_mutex_lock(&pBufferMng->mPktlistMutex);
                list_move_tail(&pNode->mList, &pBufferMng->idlePktList);
                pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
                goto PROCESS_MESSAGE;
            }

            int ctrl_bits = 0;
            ctrl_bits |= CEDARV_FLAG_PTS_VALID;
            if (pDemuxData->cdx_pkt.flags & FIRST_PART)
            {
                ctrl_bits |= CEDARV_FLAG_FIRST_PART;
            }
            if (pDemuxData->cdx_pkt.flags & LAST_PART)
            {
                ctrl_bits |= CEDARV_FLAG_LAST_PART;
            }
            if (pDemuxData->cdx_pkt.flags & KEY_FRAME)
            {
                ctrl_bits |= CEDARV_FLAG_KEYFRAME;
            }

            pNode->stEncodedStream.nFilledLen   = pDemuxData->cdx_pkt.length;
            pNode->stEncodedStream.nTobeFillLen = pDemuxData->cdx_pkt.length;
            pNode->stEncodedStream.nTimeStamp   = pDemuxData->cdx_pkt.pts;
            pNode->stEncodedStream.nFlags       = ctrl_bits;//pDemuxData->cdx_pkt.flags;

            pNode->stEncodedStream.infoVersion = pDemuxData->cdx_pkt.infoVersion;
            pNode->stEncodedStream.pChangedStreamsInfo = pDemuxData->cdx_pkt.info;
            pNode->stEncodedStream.duration    = pDemuxData->cdx_pkt.duration;

            pthread_mutex_lock(&pBufferMng->mPktlistMutex);
            list_move_tail(&pNode->mList, &pBufferMng->readyPktList);
            if (pBufferMng->mWaitReadyPktFlag)
            {
                pthread_cond_signal(&pBufferMng->mWaitReadyCondition);
                pBufferMng->mWaitReadyPktFlag = FALSE;
            }
            pthread_mutex_unlock(&pBufferMng->mPktlistMutex);
            pDemuxData->prefetch_done = 0;
        }

        // Clear seek flag
        if (  pDemuxData->dmx_seek_flag
           && (pDemuxData->cdx_pkt.flags & LAST_PART)
           )
        {
            alogd("seek done? start schedure to other thread for first frame");
            pDemuxData->dmx_seek_flag = 0;
            if (get_message_count(&pDemuxData->cmd_queue) > 0)
            {
                goto PROCESS_MESSAGE;
            }
        }
    }

EXIT:
    alogv("Demuxer ComponentThread stopped");
    return (void*) SUCCESS;
}


static ERRORTYPE CB_EventHandler(void* cookie, int event, void *data)
{
    DEMUXDATATYPE* pDemuxData;

    pDemuxData = (DEMUXDATATYPE*)cookie;
    pDemuxData->pCallbacks->EventHandler(pDemuxData->hSelf, pDemuxData->pAppData, event, 0, 0, data);

    return SUCCESS;
}
