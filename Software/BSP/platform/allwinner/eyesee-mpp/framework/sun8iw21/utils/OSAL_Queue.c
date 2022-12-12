/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : OSAL_Queue.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/02
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "OSAL_Queue"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/plat_log.h>

#include "OSAL_Mutex.h"
#include "OSAL_Queue.h"
#include <cdx_list.h>

int OSAL_QueueCreate(OSAL_QUEUE *queueHandle, int maxQueueElem)
{
    int i = 0;
    OSAL_QElem *newqelem = NULL;
    OSAL_QUEUE *queue = (OSAL_QUEUE *)queueHandle;

    int ret = 0;

    if (!queue)
        return -1;

    ret = OSAL_MutexCreate(&queue->qMutex);
    if (ret != 0)
        return ret;

    queue->maxElem = maxQueueElem;
    INIT_LIST_HEAD(&queue->mIdleElemList);
    INIT_LIST_HEAD(&queue->mValidElemList);
    for(i=0; i<queue->maxElem; i++)
    {
        newqelem = (OSAL_QElem*)malloc(sizeof(OSAL_QElem));
        memset(newqelem, 0, sizeof(OSAL_QElem));
        list_add_tail(&newqelem->mList, &queue->mIdleElemList);
    }
    return 0;
}

int OSAL_QueueTerminate(OSAL_QUEUE *queueHandle)
{
    int i = 0;
    OSAL_QElem *currentqelem = NULL;
    OSAL_QUEUE *queue = (OSAL_QUEUE *)queueHandle;
    int ret = 0;

    if (!queue)
        return -1;

    OSAL_MutexLock(queue->qMutex);
    int cnt = 0;
    if(!list_empty(&queue->mValidElemList))
    {
        OSAL_QElem *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &queue->mValidElemList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            cnt++;
        }
        alogw("Be careful! free [%d] valid QElems", cnt);
    }
    int cnt2 = 0;
    if(!list_empty(&queue->mIdleElemList))
    {
        OSAL_QElem *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &queue->mIdleElemList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            cnt2++;
        }
        alogv("free [%d] idle QElems", cnt2);
    }
    if(cnt + cnt2 != queue->maxElem)
    {
        aloge("fatal error! OSAL queue elem number [%d]+[%d]!=[%d]", cnt, cnt2, queue->maxElem);
    }
    OSAL_MutexUnlock(queue->qMutex);

    ret = OSAL_MutexTerminate(queue->qMutex);

    return ret;
}

int OSAL_Queue(OSAL_QUEUE *queueHandle, void *data)
{
    int ret = 0;
    OSAL_QUEUE *queue = (OSAL_QUEUE *)queueHandle;
    if (queue == NULL)
        return -1;

    OSAL_MutexLock(queue->qMutex);
    OSAL_QElem *pEntry = list_first_entry_or_null(&queue->mIdleElemList, OSAL_QElem, mList);
    if(pEntry)
    {
        pEntry->data = data;
        list_move_tail(&pEntry->mList, &queue->mValidElemList);
        ret = 0;
    }
    else
    {
        aloge("fatal error! osal queue idle list is empty!");
        ret = -1;
    }
    OSAL_MutexUnlock(queue->qMutex);
    return ret;
}

void *OSAL_Dequeue(OSAL_QUEUE *queueHandle)
{
    void *data = NULL;
    OSAL_QUEUE *queue = (OSAL_QUEUE *)queueHandle;
    if (queue == NULL)
        return NULL;

    OSAL_MutexLock(queue->qMutex);
    OSAL_QElem *pEntry = list_first_entry_or_null(&queue->mValidElemList, OSAL_QElem, mList);
    if(pEntry)
    {
        data = pEntry->data;
        list_move_tail(&pEntry->mList, &queue->mIdleElemList);
    }
    else
    {
        data = NULL;
    }
    OSAL_MutexUnlock(queue->qMutex);
    return data;
}

int OSAL_GetElemNum(OSAL_QUEUE *queueHandle)
{
    OSAL_QUEUE *queue = (OSAL_QUEUE *)queueHandle;
    if (queue == NULL)
        return -1;

    OSAL_MutexLock(queue->qMutex);
    int cnt;
    struct list_head *pList;
    cnt = 0;list_for_each(pList, &queue->mValidElemList)cnt++;
    OSAL_MutexUnlock(queue->qMutex);
    return cnt;
}

int OSAL_QueueSetElem(OSAL_QUEUE *queueHandle, void *data)
{
    OSAL_QUEUE *queue = (OSAL_QUEUE *)queueHandle;
    if (queue == NULL)
        return -1;

    OSAL_MutexLock(queue->qMutex);
    int ret = 0;
    int bFindFlag = 0;
    if(!list_empty(&queue->mValidElemList))
    {
        OSAL_QElem *pEntry;
        list_for_each_entry(pEntry, &queue->mValidElemList, mList)
        {
            // if there is an same elem, do not in queue anyway
            if(pEntry->data == data)
            {
                bFindFlag = 1;
                ret = 0;
                break;
            }
        }
    }
    if(!bFindFlag)
    {
        OSAL_QElem *pEntry = list_first_entry_or_null(&queue->mIdleElemList, OSAL_QElem, mList);
        if(pEntry)
        {
            pEntry->data = data;
            list_move_tail(&pEntry->mList, &queue->mValidElemList);
            ret = 0;
        }
        else
        {
            ret = -1;
        }
    }
    OSAL_MutexUnlock(queue->qMutex);
    return 0;
}


