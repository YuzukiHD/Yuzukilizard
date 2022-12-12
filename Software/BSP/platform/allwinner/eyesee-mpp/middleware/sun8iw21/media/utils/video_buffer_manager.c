/******************************************************************************
  Copyright (C), 2018-2020, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : video buffer list manager.c
  Version       : Initial Draft
  Author        : huangbohan
  Created       : 2018/10/26
  Last Modified :
  Description   : utils for mpi components' buffer list manager
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "video_buffer_manager"
#include <string.h>
#include <utils/plat_log.h>

#include "cdx_list.h"
#include "video_buffer_manager.h"

static VIDEO_FRAME_INFO_S *VBMGetValidFrame(VideoBufferManager *pMgr)
{
    VideoFrameListInfo *pEntry = NULL;

    pthread_mutex_lock(&pMgr->mFrmListLock);
    if (list_empty(&pMgr->mValidFrmList)) {
        pthread_mutex_unlock(&pMgr->mFrmListLock);
        return NULL;
    }

    pEntry = list_first_entry(&pMgr->mValidFrmList, VideoFrameListInfo, mList);
    list_move_tail(&pEntry->mList, &pMgr->mUsingFrmList);
    pthread_mutex_unlock(&pMgr->mFrmListLock);

    return &pEntry->mFrame;
}

static VIDEO_FRAME_INFO_S *VBMGetAllValidUsingFrame(VideoBufferManager *pMgr)
{
    VideoFrameListInfo *pEntry = NULL;

    pthread_mutex_lock(&pMgr->mFrmListLock);
    if (list_empty(&pMgr->mUsingFrmList) && list_empty(&pMgr->mValidFrmList)) {
        pthread_mutex_unlock(&pMgr->mFrmListLock);
        return NULL;
    }

    if (!list_empty(&pMgr->mUsingFrmList)) {
        pEntry = list_first_entry(&pMgr->mUsingFrmList, VideoFrameListInfo, mList);
        list_move_tail(&pEntry->mList, &pMgr->mFreeFrmList);
        goto EDone;
    }

    if (!list_empty(&pMgr->mValidFrmList)) {
        pEntry = list_first_entry(&pMgr->mValidFrmList, VideoFrameListInfo, mList);
        list_move_tail(&pEntry->mList, &pMgr->mFreeFrmList);
        goto EDone;
    }

EDone:
    pthread_mutex_unlock(&pMgr->mFrmListLock);

    return &pEntry->mFrame;
}

static VIDEO_FRAME_INFO_S *VBMGetOldestUsingFrame(VideoBufferManager *pMgr)
{
    VideoFrameListInfo *pEntry;

    pthread_mutex_lock(&pMgr->mFrmListLock);
    pEntry = list_first_entry_or_null(&pMgr->mUsingFrmList, VideoFrameListInfo, mList);
    pthread_mutex_unlock(&pMgr->mFrmListLock);

    return (pEntry == NULL) ? NULL : &pEntry->mFrame;
}

static VIDEO_FRAME_INFO_S *VBMGetOldestValidFrame(VideoBufferManager *pMgr)
{
    VideoFrameListInfo *pEntry;

    pthread_mutex_lock(&pMgr->mFrmListLock);
    pEntry = list_first_entry_or_null(&pMgr->mValidFrmList, VideoFrameListInfo, mList);
    pthread_mutex_unlock(&pMgr->mFrmListLock);

    return (pEntry == NULL) ? NULL : &pEntry->mFrame;
}

static VIDEO_FRAME_INFO_S *VBMGetSpecUsingFrameWithAddr(VideoBufferManager *pMgr, void *pVirAddr)
{
    VideoFrameListInfo *pEntry, *pTmp;
    int found = 0;

    pthread_mutex_lock(&pMgr->mFrmListLock);
    list_for_each_entry_safe(pEntry, pTmp, &pMgr->mUsingFrmList, mList)
    {
        if (pEntry->mFrame.VFrame.mpVirAddr[0] == pVirAddr) {
            found = 1;
            break;
        }
    }

    if (found) {
        pthread_mutex_unlock(&pMgr->mFrmListLock);
        return &pEntry->mFrame;
    } else {
        aloge("Unknown video virvi frame, frame virtual address[%p]!", pVirAddr);
        pthread_mutex_unlock(&pMgr->mFrmListLock);
        return NULL;
    }
}

static int VBMReleaseFrame(VideoBufferManager *pMgr, VIDEO_FRAME_INFO_S *pFrame)
{
    VideoFrameListInfo *pEntry, *pTmp;
    int found = 0;

    pthread_mutex_lock(&pMgr->mFrmListLock);
    list_for_each_entry_safe(pEntry, pTmp, &pMgr->mUsingFrmList, mList)
    {
        if (pEntry->mFrame.VFrame.mpVirAddr[0] == pFrame->VFrame.mpVirAddr[0] ||
            pEntry->mFrame.mId == pFrame->mId) {
            *pFrame = pEntry->mFrame;
            list_del(&pEntry->mList);
            found = 1;
            break;
        }
    }

    if (found) {
        list_add_tail(&pEntry->mList, &pMgr->mFreeFrmList);
    } else {
        aloge("Unknown video frame, frame id[%d]!", pFrame->mId);
        pthread_mutex_unlock(&pMgr->mFrmListLock);
        return FAILURE;
    }

    if(pMgr->mbWaitUsingFrmEmptyFlag && list_empty(&pMgr->mUsingFrmList)) {
        pthread_cond_signal(&pMgr->mCondUsingFrmEmpty);
    }

    pthread_mutex_unlock(&pMgr->mFrmListLock);
    return SUCCESS;
}

static int VBMPushFrame(VideoBufferManager *pMgr, VIDEO_FRAME_INFO_S *pFrame)
{
    VideoFrameListInfo *pEntry = NULL;

    pthread_mutex_lock(&pMgr->mFrmListLock);
    if (list_empty(&pMgr->mFreeFrmList)) {
        pthread_mutex_unlock(&pMgr->mFrmListLock);
        return FAILURE;
    }

    pEntry = list_first_entry(&pMgr->mFreeFrmList, VideoFrameListInfo, mList);
    pEntry->mFrame = *pFrame;
    list_move_tail(&pEntry->mList, &pMgr->mValidFrmList);
    pthread_mutex_unlock(&pMgr->mFrmListLock);

    return SUCCESS;
}

static int VBMIsUsingFrmEmpty(VideoBufferManager *pMgr)
{
    int iRet;
    pthread_mutex_lock(&pMgr->mFrmListLock);
    iRet = list_empty(&pMgr->mUsingFrmList);
    pthread_mutex_unlock(&pMgr->mFrmListLock);
    return iRet;
}

static int VBMWaitUsingFrmEmpty(VideoBufferManager *pMgr)
{
    pthread_mutex_lock(&pMgr->mFrmListLock);
    pMgr->mbWaitUsingFrmEmptyFlag = TRUE;
    int nCondWaitFlag = 0;
    while(!list_empty(&pMgr->mUsingFrmList)) {
        alogd("wait all Using frame return");
        nCondWaitFlag++;
        pthread_cond_wait(&pMgr->mCondUsingFrmEmpty, &pMgr->mFrmListLock);
    }
    if(nCondWaitFlag != 0)
    {
        alogd("wait all Using frame return done[%d]!", nCondWaitFlag);
    }
    pMgr->mbWaitUsingFrmEmptyFlag = FALSE;
    pthread_mutex_unlock(&pMgr->mFrmListLock);
    return SUCCESS;
}

static int VBMGetValidUsingFrameNum(struct VideoBufferManager *pMgr)
{
    int n = 0;
    pthread_mutex_lock(&pMgr->mFrmListLock);
    struct list_head *pNode;
    list_for_each(pNode, &pMgr->mUsingFrmList){n++;}
    list_for_each(pNode, &pMgr->mValidFrmList){n++;}
    pthread_mutex_unlock(&pMgr->mFrmListLock);
    return n;
}

VideoBufferManager *VideoBufMgrCreate(int frmNum, int frmSize)
{
    VideoBufferManager *pMgr = (VideoBufferManager *)malloc(sizeof(VideoBufferManager));
    if (pMgr == NULL) {
        aloge("Alloc VideoBufferManager error!");
        return NULL;
    }
    memset(pMgr, 0, sizeof(VideoBufferManager));
    INIT_LIST_HEAD(&pMgr->mFreeFrmList);
    INIT_LIST_HEAD(&pMgr->mValidFrmList);
    INIT_LIST_HEAD(&pMgr->mUsingFrmList);
    pthread_mutex_init(&pMgr->mFrmListLock, NULL);
    pthread_cond_init(&pMgr->mCondUsingFrmEmpty, NULL);
    int i;
    for (i = 0; i < frmNum; ++i) {
        VideoFrameListInfo *pNode = (VideoFrameListInfo *)malloc(sizeof(VideoFrameListInfo));
        if (pNode == NULL) {
            aloge("Alloc VideoFrameListInfo error!");
            break;
        }
        memset(pNode, 0, sizeof(VideoFrameListInfo));
        list_add_tail(&pNode->mList, &pMgr->mFreeFrmList);
        pMgr->mFrameNodeNum++;
    }

    //aloge("Alloc %d input frame buffers in list manager.", pMgr->mFrameNodeNum);

    pMgr->GetOldestUsingFrame = VBMGetOldestUsingFrame;
    pMgr->GetOldestValidFrame = VBMGetOldestValidFrame;
    pMgr->GetSpecUsingFrameWithAddr = VBMGetSpecUsingFrameWithAddr;
    pMgr->getValidFrame = VBMGetValidFrame;
    pMgr->GetAllValidUsingFrame = VBMGetAllValidUsingFrame;
    pMgr->releaseFrame = VBMReleaseFrame;
    pMgr->pushFrame = VBMPushFrame;
    pMgr->usingFrmEmpty = VBMIsUsingFrmEmpty;     /* is usingFrm Empty */
    pMgr->waitUsingFrmEmpty = VBMWaitUsingFrmEmpty;
    pMgr->GetValidUsingFrameNum = VBMGetValidUsingFrameNum;

    return pMgr;
}

void VideoBufMgrDestroy(VideoBufferManager *pMgr)
{
    VideoFrameListInfo *pEntry, *pTmp;
    int frmnum = 0;

    if (pMgr == NULL) {
        aloge("video input buffer manager has been unexist!!\n");
        return;
    }

    pthread_mutex_lock(&pMgr->mFrmListLock);
    if (!list_empty(&pMgr->mUsingFrmList)) {
        // TODO: if we should release this frame list nodes
        aloge("Fatal error! UsingFrmList is not empty! maybe some frames not release!");
    }

    if (!list_empty(&pMgr->mValidFrmList)) {
        list_for_each_entry_safe(pEntry, pTmp, &pMgr->mValidFrmList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            ++frmnum;
        }
    }

    if (!list_empty(&pMgr->mFreeFrmList)) {
        list_for_each_entry_safe(pEntry, pTmp, &pMgr->mFreeFrmList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            ++frmnum;
        }
    }
    pthread_mutex_unlock(&pMgr->mFrmListLock);

    if (frmnum != pMgr->mFrameNodeNum) {
        aloge("Fatal error! frame node number is not match[%d][%d]", frmnum, pMgr->mFrameNodeNum);
    }

    pthread_cond_destroy(&pMgr->mCondUsingFrmEmpty);
    pthread_mutex_destroy(&pMgr->mFrmListLock);

    free(pMgr);
}

