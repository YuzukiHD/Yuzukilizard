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

#include "cdc_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "vencoder.h"
#include "FrameBufferManager.h"
#include "EncAdapter.h"

static void enqueue(VencInputBufferInfo** pp_head, VencInputBufferInfo* p)
{
    VencInputBufferInfo* cur;

    cur = *pp_head;

    if (cur == NULL)
    {
        *pp_head = p;
        p->next  = NULL;
        return;
    }
    else
    {
        while(cur->next != NULL)
            cur = cur->next;

        cur->next = p;
        p->next   = NULL;

        return;
    }
}

static VencInputBufferInfo* dequeue(VencInputBufferInfo** pp_head)
{
    VencInputBufferInfo* head;

    head = *pp_head;

    if (head == NULL)
    {
        return NULL;
    }
    else
    {
        *pp_head = head->next;
        head->next = NULL;
        return head;
    }
}

static void printfAllQueueStatus(FrameBufferManager* fbm)
{
    InputBufferList *buffer_list = &fbm->inputbuffer_list;

    VencInputBufferInfo* cur = buffer_list->empty_quene;
    while(cur != NULL)
    {
        logd("empty_queue: id = %ld, node_addr = %p, addPhy = %p",
            cur->inputbuffer.nID, &cur->inputbuffer, cur->inputbuffer.pAddrPhyY);
        cur = cur->next;
    }
    cur = buffer_list->valid_quene;
    while(cur != NULL)
    {
        logd("valid_queue: id = %ld, node_addr = %p, addPhy = %p",
            cur->inputbuffer.nID, &cur->inputbuffer, cur->inputbuffer.pAddrPhyY);
        cur = cur->next;
    }

}

FrameBufferManager* VencFbmCreate(int num, struct ScMemOpsS *memops,
                                        void *veOps, void *pVeopsSelf)
{
    FrameBufferManager* context = NULL;
    int i;
    VencInputBufferInfo*   buffer_node = NULL;

    context = (FrameBufferManager*)malloc(sizeof(FrameBufferManager));
    if (!context)
    {
        loge("malloc FrameBufferManager fail!");
        return NULL;
    }

    memset(context, 0, sizeof(FrameBufferManager));

    context->inputbuffer_list.max_buf_node_num = num;
    context->inputbuffer_list.buffer_node = (VencInputBufferInfo*)calloc(sizeof(VencInputBufferInfo), num);

    if (!context->inputbuffer_list.buffer_node)
    {
        loge("inputbuffer alloc quene buffer failed");
        free(context);
        return NULL;
    }

    memset(context->inputbuffer_list.buffer_node, 0, sizeof(VencInputBufferInfo)*num);

    context->memops = memops;
    context->veops  = veOps;
    context->pVeOpsSelf = pVeopsSelf;
    pthread_mutex_init(&context->inputbuffer_list.mutex, NULL);

    //* enqueue buffer_node to empty_queue when create
    for (i = 0; i < num; i++)
    {
        enqueue(&context->inputbuffer_list.empty_quene, &context->inputbuffer_list.buffer_node[i]);
    }

    return context;
}

void VencFbmDestroy(FrameBufferManager* fbm)
{
    int i = 0;

    if (!fbm)
    {
        return;
    }
    struct ScMemOpsS *_memops = fbm->memops;
    void *veOps = fbm->veops;
    void *pVeopsSelf = fbm->pVeOpsSelf;
    InputBufferList *buffer_list = &fbm->inputbuffer_list;
    VencInputBufferInfo *buffer_node = NULL;

    for(i=0; i< MAX_SELF_ALLOCATE_FBM_BUF_NUM; i++)
    {
        if(fbm->selfAllocateBufArry[i].pAddrVirY)
            EncAdapterMemPfree(fbm->selfAllocateBufArry[i].pAddrVirY);
        if(fbm->selfAllocateBufArry[i].pAddrVirC)
            EncAdapterMemPfree(fbm->selfAllocateBufArry[i].pAddrVirC);
    }

    if(buffer_list->buffer_node)
    {
        free(buffer_list->buffer_node);
        buffer_list->buffer_node = NULL;
    }
    pthread_mutex_destroy(&fbm->inputbuffer_list.mutex);

    free(fbm);
}

int VencFbmAllocateBuffer(FrameBufferManager* fbm, VencAllocateBufferParam *buffer_param, VencInputBuffer* dst_inputBuf)
{
    int i= 0;
    struct ScMemOpsS *_memops = fbm->memops;
    void *veOps = fbm->veops;
    void *pVeopsSelf = fbm->pVeOpsSelf;
    InputBufferList *buffer_list = &fbm->inputbuffer_list;
    VencInputBufferInfo*  buffer_node = NULL;
    unsigned char* pAddrVirY = NULL;
    unsigned char* pAddrVirC = NULL;
    VencInputBuffer *pInputBuf = NULL;

    if (!fbm)
    {
        loge("fbm is NULL, please check\n");
        return -1;
    }

    logd("nSizeY = %d, nSizeC = %d", buffer_param->nSizeY, buffer_param->nSizeC);

    for(i=0; i< MAX_SELF_ALLOCATE_FBM_BUF_NUM; i++)
    {
        if(fbm->selfAllocateBufArry[i].pAddrPhyY == NULL)
            break;
    }
    if(i >= MAX_SELF_ALLOCATE_FBM_BUF_NUM)
    {
        loge("the fbm buffer is overflow, max_num = %d", MAX_SELF_ALLOCATE_FBM_BUF_NUM);
        return -1;
    }
    pInputBuf = &fbm->selfAllocateBufArry[i];

    fbm->size_y = buffer_param->nSizeY;
    fbm->size_c = buffer_param->nSizeC;

    pInputBuf->bAllocMemSelf = 1;
    pInputBuf->nID = fbm->selfAllocateBufValidNum;

    pAddrVirY = (unsigned char *)EncAdapterMemPalloc(fbm->size_y);
    if (!pAddrVirY)
    {
        loge("ABM_inputbuffer Y alloc error");
        return -1;
    }
    EncAdapterMemFlushCache(pAddrVirY, fbm->size_y);

    pInputBuf->pAddrVirY = pAddrVirY;
    pInputBuf->pAddrPhyY = (unsigned char *)EncAdapterMemGetPhysicAddressCpu(pAddrVirY);

    if (fbm->size_c > 0)
    {
        pAddrVirC = (unsigned char *)EncAdapterMemPalloc((int)fbm->size_c);
        if (!pAddrVirC)
        {
            loge("ABM_inputbuffer C alloc error");
            return -1;
        }
        EncAdapterMemFlushCache(pAddrVirC, fbm->size_c);

        pInputBuf->pAddrVirC = pAddrVirC;
        pInputBuf->pAddrPhyC = (unsigned char *)EncAdapterMemGetPhysicAddressCpu(pAddrVirC);
    }

    fbm->selfAllocateBufValidNum++;
    memcpy(dst_inputBuf, pInputBuf, sizeof(VencInputBuffer));

    return 0;
}

//* empty_queue --> valid_queue
int VencFbmAddValidBuffer(FrameBufferManager* fbm, VencInputBuffer *src_validbuffer)
{
    InputBufferList *buffer_list = &fbm->inputbuffer_list;
    VencInputBufferInfo* buffer_node = NULL;
    struct ScMemOpsS *_memops = fbm->memops;

    pthread_mutex_lock(&buffer_list->mutex);

    buffer_node = dequeue(&buffer_list->empty_quene);
    if(buffer_node)
    {
        if(src_validbuffer->bNeedFlushCache == 1)
        {
            if(src_validbuffer->pAddrVirY)
                EncAdapterMemFlushCache(src_validbuffer->pAddrVirY, fbm->size_y);
            if(src_validbuffer->pAddrVirC)
                EncAdapterMemFlushCache(src_validbuffer->pAddrVirC, fbm->size_c);
        }

        memcpy(&buffer_node->inputbuffer, src_validbuffer, sizeof(VencInputBuffer));
        enqueue(&buffer_list->valid_quene, buffer_node);
        buffer_list->valid_num++;
    }
    else
    {
        loge("dequeue of buffer_list->empty_quene failed");
        pthread_mutex_unlock(&buffer_list->mutex);
        return -1;
    }

    pthread_mutex_unlock(&buffer_list->mutex);

    return 0;
}

//* get the head of valid_queue
int VencFbmRequestValidBuffer(FrameBufferManager* fbm, VencInputBuffer *dst_validbuffer)
{
    InputBufferList *buffer_list = &fbm->inputbuffer_list;
    VencInputBufferInfo* buffer_node = NULL;

    if (!fbm)
    {
        return -1;
    }

    pthread_mutex_lock(&buffer_list->mutex);
    buffer_node = buffer_list->valid_quene;
    pthread_mutex_unlock(&buffer_list->mutex);

    if (buffer_node != NULL)
    {
        //printfAllQueueStatus(fbm);
        memcpy(dst_validbuffer, &buffer_node->inputbuffer, sizeof(VencInputBuffer));
    }
    else
    {
        return -1;
    }

    return 0;
}

//* valid_queue --> empty_queue
int VencFbmReturnValidBuffer(FrameBufferManager* fbm, VencInputBuffer *src_inputbuffer)
{
    InputBufferList *buffer_list = &fbm->inputbuffer_list;
    VencInputBufferInfo* buffer_node = NULL;

    pthread_mutex_lock(&buffer_list->mutex);

    buffer_node = dequeue(&buffer_list->valid_quene);
    if(buffer_node)
    {
        if(src_inputbuffer->nID != buffer_node->inputbuffer.nID)
        {
            logw("buffer is not match, id = %lx, %lx", src_inputbuffer->nID, buffer_node->inputbuffer.nID);
        }
        memcpy(&buffer_node->inputbuffer, src_inputbuffer, sizeof(VencInputBuffer));
        enqueue(&buffer_list->empty_quene, buffer_node);
        buffer_list->valid_num--;
    }
    else
    {
        loge("dequeue of buffer_list->valid_quene failed");
        pthread_mutex_unlock(&buffer_list->mutex);
        return -1;
    }

    pthread_mutex_unlock(&buffer_list->mutex);

    return 0;
}

int VencFbmReset(FrameBufferManager* fbm)
{
    int num, i;
    int result = 0;

    if (!fbm)
    {
        loge("ResetFrameBuffer error, the fbm pointer is null\n");
        return -1;
    }

    num = fbm->inputbuffer_list.max_buf_node_num;
    if (num > 0)
    {
        pthread_mutex_lock(&fbm->inputbuffer_list.mutex);

        //memset(fbm->inputbuffer_list.buffer_node, 0, sizeof(VencInputBufferInfo)*num);
        fbm->inputbuffer_list.empty_quene = NULL;
        fbm->inputbuffer_list.valid_quene = NULL;
        fbm->inputbuffer_list.valid_num = 0;

        // all buffer enquene empty quene
        for(i=0; i<(int)num; i++)
        {
            enqueue(&fbm->inputbuffer_list.empty_quene,&fbm->inputbuffer_list.buffer_node[i]);
        }

        pthread_mutex_unlock(&fbm->inputbuffer_list.mutex);
    }

    return result;
}

int VencFbmGetValidBufferNum(FrameBufferManager* fbm)
{
   unsigned int result = 0;
   pthread_mutex_lock(&fbm->inputbuffer_list.mutex);
   result = fbm->inputbuffer_list.valid_num;
   pthread_mutex_unlock(&fbm->inputbuffer_list.mutex);

   return result;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

