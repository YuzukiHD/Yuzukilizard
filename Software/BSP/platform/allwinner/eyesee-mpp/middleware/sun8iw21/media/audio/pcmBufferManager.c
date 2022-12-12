/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : pcmBufferManager.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/28
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "pcmBufferManager"
#include <utils/plat_log.h>

#include <memory.h>
#include <cdx_list.h>
#include <pcmBufferManager.h>

#define MAX_PCM_FRAME_NUM (100)
typedef struct AudioFrameInfo
{
    AUDIO_FRAME_S mFrame;
    struct list_head mList;
} AudioFrameInfo;

static AUDIO_FRAME_S *pcmBufMgrGetValidFrame(PcmBufferManager *pMgr)
{
    AudioFrameInfo *pEntry = NULL;

    pthread_mutex_lock(&pMgr->mValidFrmListLock);
    if (!list_empty(&pMgr->mValidFrmList)) {
        pEntry = list_first_entry(&pMgr->mValidFrmList, AudioFrameInfo, mList);
        list_del(&pEntry->mList);
    } else {
        pthread_mutex_unlock(&pMgr->mValidFrmListLock);
        return NULL;
    }
    pthread_mutex_unlock(&pMgr->mValidFrmListLock);

    pthread_mutex_lock(&pMgr->mUsingFrmListLock);
    list_add_tail(&pEntry->mList, &pMgr->mUsingFrmList);
    pthread_mutex_unlock(&pMgr->mUsingFrmListLock);

    return &pEntry->mFrame;
}

static void pcmBufMgrReleaseFrame(PcmBufferManager *pMgr, AUDIO_FRAME_S *pFrame)
{
    AudioFrameInfo *pEntry, *pTmp;
    int found = 0;

    pthread_mutex_lock(&pMgr->mUsingFrmListLock);
    list_for_each_entry_safe(pEntry, pTmp, &pMgr->mUsingFrmList, mList)
    {
        if (pEntry->mFrame.mId == pFrame->mId && pEntry->mFrame.mpAddr == pFrame->mpAddr) {
            list_del(&pEntry->mList);
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&pMgr->mUsingFrmListLock);

    if (found) {
        pthread_mutex_lock(&pMgr->mFreeFrmListLock);
        list_add_tail(&pEntry->mList, &pMgr->mFreeFrmList);
        pthread_mutex_unlock(&pMgr->mFreeFrmListLock);
    } else {
        aloge("fatal error! Unknown audio frame!");
    }
}

static AUDIO_FRAME_S* pcmBufMgrGetFreeFrame(PcmBufferManager *pMgr)
{
    AudioFrameInfo *pEntry = NULL;

    pthread_mutex_lock(&pMgr->mFreeFrmListLock);
    if (list_empty(&pMgr->mFreeFrmList)) 
    {
        if(pMgr->mFrameNodeNum < MAX_PCM_FRAME_NUM)
        {
            AudioFrameInfo *pNode = (AudioFrameInfo*)malloc(sizeof(AudioFrameInfo));
            if (pNode != NULL) 
            {
                memset(pNode, 0, sizeof(AudioFrameInfo));
                pNode->mFrame.mpAddr = malloc(pMgr->mFrameSize);
                if (pNode->mFrame.mpAddr != NULL) 
                {
                    pNode->mFrame.mId = pMgr->mFrameNodeNum;
                    list_add_tail(&pNode->mList, &pMgr->mFreeFrmList);
                    pMgr->mFrameNodeNum++;
                    alogd("Be careful! add one pcm node, curNum[%d]!", pMgr->mFrameNodeNum);
                }
                else
                {
                    aloge("fatal error! Alloc mpAddr size %d error!", pMgr->mFrameSize);
                    free(pNode);
                }
            }
            else
            {
                aloge("fatal error! Alloc AudioFrameInfo error!");
            }
        }
    }
    if (!list_empty(&pMgr->mFreeFrmList)) {
        pEntry = list_first_entry(&pMgr->mFreeFrmList, AudioFrameInfo, mList);
        list_del(&pEntry->mList);
    } else {
        pthread_mutex_unlock(&pMgr->mFreeFrmListLock);
        return NULL;
    }
    pthread_mutex_unlock(&pMgr->mFreeFrmListLock);

    pthread_mutex_lock(&pMgr->mFillingFrmListLock);
    list_add_tail(&pEntry->mList, &pMgr->mFillingFrmList);
    pthread_mutex_unlock(&pMgr->mFillingFrmListLock);

    return &pEntry->mFrame;
}

static void pcmBufMgrPushFrame(PcmBufferManager *pMgr, AUDIO_FRAME_S *pFrame)
{
    AudioFrameInfo *pEntry, *pTmp;
    int found = 0;

    pthread_mutex_lock(&pMgr->mFillingFrmListLock);
    list_for_each_entry_safe(pEntry, pTmp, &pMgr->mFillingFrmList, mList)
    {
        if (pEntry->mFrame.mId == pFrame->mId && pEntry->mFrame.mpAddr == pFrame->mpAddr) {
            list_del(&pEntry->mList);
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&pMgr->mFillingFrmListLock);

    if (found) {
        pEntry->mFrame = *pFrame;
        pthread_mutex_lock(&pMgr->mValidFrmListLock);
        list_add_tail(&pEntry->mList, &pMgr->mValidFrmList);
        pthread_mutex_unlock(&pMgr->mValidFrmListLock);
    } else {
        aloge("Unknown audio frame!");
    }
}

static int pcmBufMgrUsingFrmEmpty(PcmBufferManager *pMgr)
{
    int ret;
    pthread_mutex_lock(&pMgr->mUsingFrmListLock);
    ret = list_empty(&pMgr->mUsingFrmList);
    pthread_mutex_unlock(&pMgr->mUsingFrmListLock);
    return ret;
}

static int pcmBufMgrFillingFrmEmpty(PcmBufferManager *pMgr)
{
    int ret;
    pthread_mutex_lock(&pMgr->mFillingFrmListLock);
    ret = list_empty(&pMgr->mFillingFrmList);
    pthread_mutex_unlock(&pMgr->mFillingFrmListLock);
    return ret;
}


static int pcmBufMgrValidFrmEmpty(PcmBufferManager *pMgr)
{
    int ret;
    pthread_mutex_lock(&pMgr->mValidFrmListLock);
    ret = list_empty(&pMgr->mValidFrmList);
    pthread_mutex_unlock(&pMgr->mValidFrmListLock);
    return ret;
}


static int pcmBufMgrFillingFrmCnt(PcmBufferManager *pMgr)
{
    struct list_head *pEntry;
    int cnt = 0;
    pthread_mutex_lock(&pMgr->mFillingFrmListLock);
    list_for_each(pEntry, &pMgr->mFillingFrmList)
        cnt++;
    pthread_mutex_unlock(&pMgr->mFillingFrmListLock);
    return cnt;
}

static int pcmBufMgrValidFrmCnt(PcmBufferManager *pMgr)
{
    struct list_head *pEntry;
    int cnt = 0;
    pthread_mutex_lock(&pMgr->mValidFrmListLock);
    list_for_each(pEntry, &pMgr->mValidFrmList)
        cnt++;
    pthread_mutex_unlock(&pMgr->mValidFrmListLock);
    return cnt;
}

static int pcmBufMgrUsingFrmCnt(PcmBufferManager *pMgr)
{
    struct list_head *pEntry;
    int cnt = 0;
    pthread_mutex_lock(&pMgr->mUsingFrmListLock);
    list_for_each(pEntry, &pMgr->mUsingFrmList)
        cnt++;
    pthread_mutex_unlock(&pMgr->mUsingFrmListLock);
    return cnt;
}

PcmBufferManager *pcmBufMgrCreate(int frmNum, int frmSize)
{
    PcmBufferManager *pMgr = (PcmBufferManager*)malloc(sizeof(PcmBufferManager));
    if (pMgr == NULL) {
        aloge("Alloc PcmBufferManager error!");
        return NULL;
    }
    memset(pMgr, 0, sizeof(PcmBufferManager));
    INIT_LIST_HEAD(&pMgr->mFreeFrmList);
    INIT_LIST_HEAD(&pMgr->mValidFrmList);
    INIT_LIST_HEAD(&pMgr->mUsingFrmList);
    INIT_LIST_HEAD(&pMgr->mFillingFrmList);
    pthread_mutex_init(&pMgr->mFreeFrmListLock, NULL);
    pthread_mutex_init(&pMgr->mValidFrmListLock, NULL);
    pthread_mutex_init(&pMgr->mUsingFrmListLock, NULL);
    pthread_mutex_init(&pMgr->mFillingFrmListLock, NULL);

    pMgr->mFrameSize = frmSize;
    int i;
    for (i = 0; i < frmNum; ++i) {
        AudioFrameInfo *pNode = (AudioFrameInfo*)malloc(sizeof(AudioFrameInfo));
        if (pNode == NULL) {
            aloge("Alloc AudioFrameInfo error!");
            break;
        }
        memset(pNode, 0, sizeof(AudioFrameInfo));
        pNode->mFrame.mpAddr = malloc(frmSize);
        if (pNode->mFrame.mpAddr == NULL) {
            aloge("Alloc mpAddr size %d error!", frmSize);
            free(pNode);
            break;
        }
        pNode->mFrame.mId = i;
        list_add_tail(&pNode->mList, &pMgr->mFreeFrmList);
        pMgr->mFrameNodeNum++;
    }

    pMgr->getValidFrame = pcmBufMgrGetValidFrame;
    pMgr->releaseFrame = pcmBufMgrReleaseFrame;
    pMgr->getFreeFrame = pcmBufMgrGetFreeFrame;
    pMgr->pushFrame = pcmBufMgrPushFrame;
    pMgr->usingFrmEmpty = pcmBufMgrUsingFrmEmpty;
    pMgr->fillingFrmEmpty = pcmBufMgrFillingFrmEmpty;
    pMgr->validFrmEmpty = pcmBufMgrValidFrmEmpty;
    pMgr->fillingFrmCnt = pcmBufMgrFillingFrmCnt;
    pMgr->validFrmCnt = pcmBufMgrValidFrmCnt;
    pMgr->usingFrmCnt = pcmBufMgrUsingFrmCnt;

    return pMgr;
}

void pcmBufMgrDestroy(PcmBufferManager *pMgr)
{
    AudioFrameInfo *pEntry, *pTmp;
    int frmnum = 0;

    if (pMgr == NULL) {
        aloge("pMgr==NULL!");
        return;
    }

    pthread_mutex_lock(&pMgr->mUsingFrmListLock);
    if (!list_empty(&pMgr->mUsingFrmList)) {
        aloge("Fatal error! UsingFrmList should be 0! maybe some frames not release!");
    }
    pthread_mutex_unlock(&pMgr->mUsingFrmListLock);

    pthread_mutex_lock(&pMgr->mFillingFrmListLock);
    if (!list_empty(&pMgr->mFillingFrmList)) {
        aloge("Fatal error! FillingFrmLis should be 0! maybe some frames not release!");
    }
    pthread_mutex_unlock(&pMgr->mFillingFrmListLock);

    pthread_mutex_lock(&pMgr->mValidFrmListLock);
    if (!list_empty(&pMgr->mValidFrmList)) {
        list_for_each_entry_safe(pEntry, pTmp, &pMgr->mValidFrmList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry->mFrame.mpAddr);
            free(pEntry);
            ++frmnum;
        }
    }
    pthread_mutex_unlock(&pMgr->mValidFrmListLock);

    pthread_mutex_lock(&pMgr->mFreeFrmListLock);
    if (!list_empty(&pMgr->mFreeFrmList)) {
        list_for_each_entry_safe(pEntry, pTmp, &pMgr->mFreeFrmList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry->mFrame.mpAddr);
            free(pEntry);
            ++frmnum;
        }
    }
    pthread_mutex_unlock(&pMgr->mFreeFrmListLock);

    if (frmnum != pMgr->mFrameNodeNum) {
        aloge("Fatal error! frame node number is not match[%d][%d]", frmnum, pMgr->mFrameNodeNum);
    }

    pthread_mutex_destroy(&pMgr->mFreeFrmListLock);
    pthread_mutex_destroy(&pMgr->mValidFrmListLock);
    pthread_mutex_destroy(&pMgr->mUsingFrmListLock);
    pthread_mutex_destroy(&pMgr->mFillingFrmListLock);

    free(pMgr);
}
