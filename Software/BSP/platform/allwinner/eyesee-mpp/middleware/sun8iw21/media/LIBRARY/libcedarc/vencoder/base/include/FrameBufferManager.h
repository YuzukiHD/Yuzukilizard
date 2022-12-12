/*
 * Copyright (C) 2008-2015 Allwinner Technology Co. Ltd.
 * Author: Ning Fang <fangning@allwinnertech.com>
 *         Caoyuan Yang <yangcaoyuan@allwinnertech.com>
 *
 * This software is confidential and proprietary and may be used
 * only as expressly authorized by a licensing agreement from
 * Softwinner Products.
 *
 * The entire notice above must be reproduced on all copies
 * and should not be removed.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#ifndef _FRAME_BUFFER_MANAGER_H
#define _FRAME_BUFFER_MANAGER_H

#define MAX_SELF_ALLOCATE_FBM_BUF_NUM (8)

typedef struct VencInputBufferInfo VencInputBufferInfo;

struct VencInputBufferInfo
{
    VencInputBuffer           inputbuffer;
    VencInputBufferInfo*    next;
};

typedef struct InputBufferList
{
    unsigned int           max_buf_node_num;
    unsigned int           valid_num;
    VencInputBufferInfo*   buffer_node;

    VencInputBufferInfo*   valid_quene;
    VencInputBufferInfo*   empty_quene;
    pthread_mutex_t        mutex;
}InputBufferList;

typedef struct FrameBufferManager
{
   VencInputBuffer      selfAllocateBufArry[MAX_SELF_ALLOCATE_FBM_BUF_NUM];
   unsigned int         selfAllocateBufValidNum;
   InputBufferList      inputbuffer_list;
   unsigned int         size_y;
   unsigned int         size_c;
   struct ScMemOpsS     *memops;
   VeOpsS*              veops;
   void*                pVeOpsSelf;
}FrameBufferManager;

FrameBufferManager* VencFbmCreate(int num, struct ScMemOpsS *memops, void *veOps, void *pVeopsSelf);
void VencFbmDestroy(FrameBufferManager* fbm);
int VencFbmReset(FrameBufferManager* fbm);

int VencFbmAllocateBuffer(FrameBufferManager* fbm, VencAllocateBufferParam *buffer_param, VencInputBuffer* dst_inputBuf);
int VencFbmAddValidBuffer(FrameBufferManager* fbm, VencInputBuffer *inputbuffer);
int VencFbmRequestValidBuffer(FrameBufferManager* fbm, VencInputBuffer *inputbuffer);
int VencFbmReturnValidBuffer(FrameBufferManager* fbm, VencInputBuffer *inputbuffer);
int VencFbmGetValidBufferNum(FrameBufferManager* fbm);

#endif //_FRAME_BUFFER_MANAGER_H

#ifdef __cplusplus
}
#endif /* __cplusplus */

