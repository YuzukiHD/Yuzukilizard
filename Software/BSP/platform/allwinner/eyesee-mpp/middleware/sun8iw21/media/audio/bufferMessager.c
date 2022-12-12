/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : bufferMessager.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/20
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "bufferMessager"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <memory.h>


static ssize_t bufferMgr_WriteData(BufferManager *pMgr, char *pInBuf, size_t inSize)
{
	pthread_mutex_lock(&pMgr->mLock);
    if (inSize > pMgr->mFreeSize) {
    	pthread_mutex_unlock(&pMgr->mLock);
        alogw("Not enough buffer to write! freeSize[%d]<inSize[%d]", pMgr->mFreeSize, inSize);
        return -1;
    }
    int endSize = pMgr->mpStart + pMgr->mTotalSize - pMgr->mpWrite;
    if (endSize < inSize) {
        memcpy(pMgr->mpWrite, pInBuf, endSize);
        memcpy(pMgr->mpStart, pInBuf + endSize, inSize - endSize);
        pMgr->mpWrite = pMgr->mpStart + inSize - endSize;
    } else {
        memcpy(pMgr->mpWrite, pInBuf, inSize);
        pMgr->mpWrite += inSize;
    }
    pMgr->mFreeSize -= inSize;
    pMgr->mDataSize += inSize;
	pthread_mutex_unlock(&pMgr->mLock);

    return inSize;
}

static ssize_t bufferMgr_ReadData(BufferManager *pMgr, char *pOutBuf, size_t reqSize)
{
	pthread_mutex_lock(&pMgr->mLock);
    if (pMgr->mDataSize < reqSize) {
        pthread_mutex_unlock(&pMgr->mLock);
        alogw("Not enough buffer to read! dataSize[%d]<reqSize[%d]", pMgr->mDataSize, reqSize);
        return -1;
    }
    int endSize = pMgr->mpStart + pMgr->mTotalSize - pMgr->mpRead;
    if (endSize < reqSize) {
        memcpy(pOutBuf, pMgr->mpRead, endSize);
        memcpy(pOutBuf + endSize, pMgr->mpStart, reqSize - endSize);
        pMgr->mpRead = pMgr->mpStart + reqSize - endSize;
    } else {
        memcpy(pOutBuf, pMgr->mpRead, reqSize);
        pMgr->mpRead += reqSize;
    }
    pMgr->mFreeSize += reqSize;
    pMgr->mDataSize -= reqSize;
	pthread_mutex_unlock(&pMgr->mLock);

    return reqSize;
}

static size_t bufferMgr_GetFreeSize(BufferManager *pMgr)
{
    return pMgr->mFreeSize;
}

void bufferMgr_Destroy(BufferManager *pMgr)
{
    if (pMgr != NULL) {
        pthread_mutex_lock(&pMgr->mLock);
        free(pMgr->mpStart);
    	pthread_mutex_unlock(&pMgr->mLock);
        pthread_mutex_destroy(&pMgr->mLock);
        free(pMgr);
    }
}

BufferManager *bufferMgr_Create(size_t size)
{
    BufferManager *pMgr = (BufferManager*)malloc(sizeof(BufferManager));
    if (pMgr == NULL) {
        aloge("alloc BufferManager error!");
        return NULL;
    }

    pMgr->mpStart = (char*)malloc(size);
    if (pMgr->mpStart == NULL) {
        aloge("buffer manager alloc size %d error!", size);
        free(pMgr);
        return NULL;
    }

    pMgr->mpRead = pMgr->mpStart;
    pMgr->mpWrite = pMgr->mpStart;
    pMgr->mTotalSize = size;
    pMgr->mFreeSize = size;
    pMgr->mDataSize = 0;

    pMgr->readData = bufferMgr_ReadData;
    pMgr->writeData = bufferMgr_WriteData;
    pMgr->getFreeSize = bufferMgr_GetFreeSize;
    pthread_mutex_init(&pMgr->mLock, NULL);

    return pMgr;
}