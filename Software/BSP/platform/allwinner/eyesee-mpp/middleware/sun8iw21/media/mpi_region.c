
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#include <plat_log.h>
#include <mpi_vi_private.h>
#include <mpi_venc_private.h>
#include <mpi_region.h>
#include <BITMAP_S.h>

#include <cdx_list.h>

#define IF_MALLOC_FAIL(ptr) \
    if(NULL == ptr) \
    { \
        aloge("fatal error! malloc fail! error(%s)", strerror(errno)); \
        return ERR_RGN_NOMEM; \
    }

typedef struct
{
    MPP_CHN_S mChn;
    RGN_CHN_ATTR_S mRgnChnAttr;
    struct list_head    mList;
}RegionAttachChnInfo;

typedef struct
{
    RGN_HANDLE mRgnHandle;  //[0,RGN_HANDLE_MAX)
    RGN_ATTR_S mRgnAttr;
    BOOL mbSetBmp;
    BITMAP_S mBmp;
    pthread_mutex_t mLock;
    struct list_head mAttachChnList;    //RegionAttachChnInfo
    struct list_head mList;
}RegionInfo;

typedef struct
{
    struct list_head mRegionList;   //element type: RegionInfo
    pthread_mutex_t mLock;
} RegionManager;

static RegionManager *gpRegionManager = NULL;
ERRORTYPE RegionManager_Construct(void)
{
    int ret;
    if (gpRegionManager != NULL) 
    {
        return SUCCESS;
    }
    gpRegionManager = (RegionManager*)malloc(sizeof(RegionManager));
    IF_MALLOC_FAIL(gpRegionManager);

    ret = pthread_mutex_init(&gpRegionManager->mLock, NULL);
    if (ret != 0) 
    {
        aloge("fatal error! mutex init fail");
        free(gpRegionManager);
        gpRegionManager = NULL;
        return FAILURE;
    }
    INIT_LIST_HEAD(&gpRegionManager->mRegionList);
    return SUCCESS;
}

ERRORTYPE RegionManager_Destruct(void)
{
    if (gpRegionManager != NULL) 
    {
        if (!list_empty(&gpRegionManager->mRegionList)) 
        {
            aloge("fatal error! some regions still exist when destroy RegionManager!");
        }
        pthread_mutex_destroy(&gpRegionManager->mLock);
        free(gpRegionManager);
        gpRegionManager = NULL;
    }
    return SUCCESS;
}

static ERRORTYPE searchExistRegion_l(RGN_HANDLE Handle, RegionInfo** ppRegion)
{
    ERRORTYPE ret = FAILURE;
    RegionInfo* pEntry;

    if (gpRegionManager == NULL) 
    {
        return FAILURE;
    }
    list_for_each_entry(pEntry, &gpRegionManager->mRegionList, mList)
    {
        if(pEntry->mRgnHandle == Handle)
        {
            if(ppRegion)
            {
                *ppRegion = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    return ret;
}

static ERRORTYPE searchExistRegion(RGN_HANDLE Handle, RegionInfo** ppRegion)
{
    ERRORTYPE ret = FAILURE;
    RegionInfo* pEntry;

    if (gpRegionManager == NULL)
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpRegionManager->mLock);
    ret = searchExistRegion_l(Handle, ppRegion);
    pthread_mutex_unlock(&gpRegionManager->mLock);
    return ret;
}

static ERRORTYPE addRegion_l(RegionInfo *pRegion)
{
    if (gpRegionManager == NULL)
    {
        return FAILURE;
    }
    list_add_tail(&pRegion->mList, &gpRegionManager->mRegionList);
    return SUCCESS;
}

static ERRORTYPE addRegion(RegionInfo *pRegion)
{
    if (gpRegionManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpRegionManager->mLock);
    ERRORTYPE ret = addRegion_l(pRegion);
    pthread_mutex_unlock(&gpRegionManager->mLock);
    return ret;
}

static ERRORTYPE removeRegion(RegionInfo *pRegion)
{
    if (gpRegionManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpRegionManager->mLock);
    list_del(&pRegion->mList);
    pthread_mutex_unlock(&gpRegionManager->mLock);
    return SUCCESS;
}

static RegionInfo* RegionInfo_Construct()
{
    RegionInfo *pRegion = (RegionInfo*)malloc(sizeof(RegionInfo));
    if(NULL == pRegion)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pRegion, 0, sizeof(RegionInfo));
    int ret = pthread_mutex_init(&pRegion->mLock, NULL);
    if (ret != 0) 
    {
        aloge("fatal error! mutex init fail");
        free(pRegion);
        return NULL;
    }
    INIT_LIST_HEAD(&pRegion->mAttachChnList);
    return pRegion;
}

static void RegionInfo_Destruct(RegionInfo *pRegion)
{
    int cnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pRegion->mAttachChnList) { cnt++; }
    if(cnt > 0)
    {
        aloge("fatal error! region still has [%d] attach channels!", cnt);
    }
    if(pRegion->mBmp.mpData)
    {
        free(pRegion->mBmp.mpData);
        pRegion->mBmp.mpData = NULL;
    }
    pthread_mutex_destroy(&pRegion->mLock);
    free(pRegion);
}

ERRORTYPE RGN_DetachFromChn_l(RegionInfo *pRegion, const MPP_CHN_S *pstChn)
{
    ERRORTYPE ret = SUCCESS;
    int nCnt = 0;
    RegionAttachChnInfo *pEntry, *pDstChn;
    list_for_each_entry(pEntry, &pRegion->mAttachChnList, mList)
    {
        if(pEntry->mChn.mModId == pstChn->mModId && pEntry->mChn.mDevId == pstChn->mDevId && pEntry->mChn.mChnId == pstChn->mChnId)
        {
            if(0 == nCnt)
            {
                pDstChn = pEntry;
            }
            nCnt++;
        }
    }
    if(nCnt <= 0)
    {
        aloge("fatal error! regionHandle[%d] not find chn[%d][%d][%d]?", pRegion->mRgnHandle, pstChn->mModId, pstChn->mDevId, pstChn->mChnId);
        return ERR_RGN_UNEXIST;
    }
    if(nCnt > 1)
    {
        aloge("fatal error! more [%d]same channel?", nCnt);
    }
    if(MOD_ID_VIU == pDstChn->mChn.mModId)
    {
        AW_MPI_VI_DeleteRegion(pDstChn->mChn.mDevId, pRegion->mRgnHandle);
    }
    else if(MOD_ID_VENC == pDstChn->mChn.mModId)
    {
        AW_MPI_VENC_DeleteRegion(pDstChn->mChn.mChnId, pRegion->mRgnHandle);
    }
    else
    {
        aloge("fatal error! impossible moduleType[0x%x]", pstChn->mModId);
    }
    //delete from chnList
    list_del(&pDstChn->mList);
    free(pDstChn);
    return SUCCESS;
}

ERRORTYPE AW_MPI_RGN_Create(RGN_HANDLE Handle, const RGN_ATTR_S *pstRegion)
{
    if(!(Handle>=0 && Handle <RGN_HANDLE_MAX))
    {
        aloge("fatal error! invalid RgnHandle[%d]!", Handle);
        return ERR_RGN_INVALID_CHNID;
    }
    if(NULL == pstRegion)
    {
        aloge("fatal error! illegal RGNAttr!");
        return ERR_RGN_ILLEGAL_PARAM;
    }
    if(pstRegion->enType != OVERLAY_RGN && pstRegion->enType != COVER_RGN && pstRegion->enType != ORL_RGN)
    {
        aloge("fatal error! illegal rgnType[0x%x]!", pstRegion->enType);
        return ERR_RGN_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&gpRegionManager->mLock);
    if(SUCCESS == searchExistRegion_l(Handle, NULL))
    {
        pthread_mutex_unlock(&gpRegionManager->mLock);
        return ERR_RGN_EXIST;
    }
    BOOL bNeedMatch = FALSE;
    BOOL bCanAdd = TRUE;
    PIXEL_FORMAT_E nOverlayPixelFmt;
    if(pstRegion->enType == OVERLAY_RGN)
    {
        bNeedMatch = TRUE;
        nOverlayPixelFmt = pstRegion->unAttr.stOverlay.mPixelFmt;
    }
    if(bNeedMatch)
    {
        RegionInfo *pEntry;
        list_for_each_entry(pEntry, &gpRegionManager->mRegionList, mList)
        {
            if(pEntry->mRgnAttr.enType == OVERLAY_RGN)
            {
                if(nOverlayPixelFmt != pEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt)
                {
                    aloge("fatal error! new overlay pixel format[0x%x] is unmatch [0x%x], can't add!", nOverlayPixelFmt, pEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt);
                    bCanAdd = FALSE;
                }
                break;
            }
        }
    }
    if(bCanAdd)
    {
        RegionInfo *pNode = RegionInfo_Construct();
        pNode->mRgnHandle = Handle;
        pNode->mRgnAttr = *pstRegion;
        addRegion_l(pNode);
        pthread_mutex_unlock(&gpRegionManager->mLock);
        return SUCCESS;
    }
    else
    {
        pthread_mutex_unlock(&gpRegionManager->mLock);
        return ERR_RGN_ILLEGAL_PARAM;
    }
}

ERRORTYPE AW_MPI_RGN_Destroy(RGN_HANDLE Handle)
{
    ERRORTYPE ret;
    if(!(Handle>=0 && Handle<RGN_HANDLE_MAX))
    {
        aloge("fatal error! invalid RGN handle[%d]!", Handle);
        return ERR_RGN_INVALID_CHNID;
    }
    RegionInfo *pRegion;
    if(SUCCESS != searchExistRegion(Handle, &pRegion))
    {
        return ERR_RGN_UNEXIST;
    }
    pthread_mutex_lock(&pRegion->mLock);
    if(!list_empty(&pRegion->mAttachChnList))
    {
        int cnt = 0;
        RegionAttachChnInfo *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pRegion->mAttachChnList, mList)
        {
            RGN_DetachFromChn_l(pRegion, &pEntry->mChn);
            cnt++;
        }
        alogd("detach region from [%d] channels!", cnt);
    }
    pthread_mutex_unlock(&pRegion->mLock);
    removeRegion(pRegion);
    RegionInfo_Destruct(pRegion);
    return SUCCESS;
}

ERRORTYPE AW_MPI_RGN_GetAttr(RGN_HANDLE Handle, RGN_ATTR_S *pstRegion)
{
    if(!(Handle>=0 && Handle <RGN_HANDLE_MAX))
    {
        aloge("fatal error! invalid RgnHandle[%d]!", Handle);
        return ERR_RGN_INVALID_CHNID;
    }
    if(NULL == pstRegion)
    {
        aloge("fatal error! illegal RGNAttr!");
        return ERR_RGN_ILLEGAL_PARAM;
    }
    RegionInfo *pRegion;
    if(SUCCESS != searchExistRegion(Handle, &pRegion))
    {
        return ERR_RGN_UNEXIST;
    }
    *pstRegion = pRegion->mRgnAttr;
    return SUCCESS;
}

/**
 * must set rgn attr before setBitmap, or else bitmap will be invalid.
 * if has been attached to channels, can't set attr.
 */
ERRORTYPE AW_MPI_RGN_SetAttr(RGN_HANDLE Handle, const RGN_ATTR_S *pstRegion)
{
    ERRORTYPE ret = SUCCESS;
    if(!(Handle>=0 && Handle <RGN_HANDLE_MAX))
    {
        aloge("fatal error! invalid RgnHandle[%d]!", Handle);
        return ERR_RGN_INVALID_CHNID;
    }
    if(NULL == pstRegion)
    {
        aloge("fatal error! illegal RGNAttr!");
        return ERR_RGN_ILLEGAL_PARAM;
    }
    RegionInfo *pRegion;
    if(SUCCESS != searchExistRegion(Handle, &pRegion))
    {
        return ERR_RGN_UNEXIST;
    }
    pthread_mutex_lock(&pRegion->mLock);
    if(list_empty(&pRegion->mAttachChnList))
    {
        if(pRegion->mbSetBmp)
        {
            alogw("Be careful! Bitmap will release before set attr!");
            if(pRegion->mBmp.mpData)
            {
                free(pRegion->mBmp.mpData);
                pRegion->mBmp.mpData = NULL;
            }
            memset(&pRegion->mBmp, 0, sizeof(BITMAP_S));
            pRegion->mbSetBmp = FALSE;
        }
        pRegion->mRgnAttr = *pstRegion;
    }
    else
    {
        alogw("Be careful! region has attach to channel, can't set region attribute!");
        ret = ERR_RGN_BUSY;
    }
    pthread_mutex_unlock(&pRegion->mLock);
    return ret;
}
/**
 * If setBitmap again, the previous one will be release, and will update every channel which has been attached.
 */
ERRORTYPE AW_MPI_RGN_SetBitMap(RGN_HANDLE Handle, const BITMAP_S *pstBitmap)
{
    ERRORTYPE ret = SUCCESS;
    if(!(Handle>=0 && Handle <RGN_HANDLE_MAX))
    {
        aloge("fatal error! invalid RgnHandle[%d]!", Handle);
        return ERR_RGN_INVALID_CHNID;
    }
    RegionInfo *pRegion;
    if(SUCCESS != searchExistRegion(Handle, &pRegion))
    {
        return ERR_RGN_UNEXIST;
    }
    if(OVERLAY_RGN != pRegion->mRgnAttr.enType)
    {
        aloge("fatal error! RgnType[0x%x] is not overlay!", pRegion->mRgnAttr.enType);
        return ERR_RGN_NOT_PERM;
    }
    if(NULL == pstBitmap)
    {
        aloge("fatal error! bitmap can't be NULL!");
        return ERR_RGN_ILLEGAL_PARAM;
    }
    if(pstBitmap->mPixelFormat != pRegion->mRgnAttr.unAttr.stOverlay.mPixelFmt)
    {
        aloge("fatal error! bitmap pixFmt[0x%x] != region pixFmt[0x%x]!", pstBitmap->mPixelFormat, pRegion->mRgnAttr.unAttr.stOverlay.mPixelFmt);
        return ERR_RGN_ILLEGAL_PARAM;
    }
    if(pstBitmap->mWidth != pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width || pstBitmap->mHeight != pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height)
    {
        alogw("Be careful! bitmap size[%dx%d] != region size[%dx%d], need update region size", pstBitmap->mWidth, pstBitmap->mHeight, pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width, pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height);
        pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width = pstBitmap->mWidth;
        pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height = pstBitmap->mHeight;
        //return ERR_RGN_ILLEGAL_PARAM;
    }
    void* pBmpDataWaitFree = NULL;
    pthread_mutex_lock(&pRegion->mLock);
    if(pRegion->mbSetBmp)
    {
        alogv("Be careful! set bmp again!");
        if(pRegion->mBmp.mpData)
        {
            //free(pRegion->mBmp.mpData);
            pBmpDataWaitFree = pRegion->mBmp.mpData;
            pRegion->mBmp.mpData = NULL;
        }
        //memset(&pRegion->mBmp, 0, sizeof(BITMAP_S));
        pRegion->mbSetBmp = FALSE;
    }
    
    pRegion->mBmp = *pstBitmap;
    int nSize = BITMAP_S_GetdataSize(pstBitmap);
    pRegion->mBmp.mpData = malloc(nSize);
    IF_MALLOC_FAIL(pRegion->mBmp.mpData);
    memcpy(pRegion->mBmp.mpData, pstBitmap->mpData, nSize);
    pRegion->mbSetBmp = TRUE;

    if(!list_empty(&pRegion->mAttachChnList))
    {
        int cnt = 0;
        RegionAttachChnInfo *pAttachChnInfo;
        list_for_each_entry(pAttachChnInfo, &pRegion->mAttachChnList, mList)
        {
            if(MOD_ID_VIU == pAttachChnInfo->mChn.mModId)
            {
                ret = AW_MPI_VI_UpdateOverlayBitmap(pAttachChnInfo->mChn.mDevId, Handle, &pRegion->mBmp);
            }
            else if(MOD_ID_VENC == pAttachChnInfo->mChn.mModId)
            {
                ret = AW_MPI_VENC_UpdateOverlayBitmap(pAttachChnInfo->mChn.mChnId, Handle, &pRegion->mBmp);
            }
            else
            {
                aloge("fatal error! modId[0x%x] don't support attach region!", pAttachChnInfo->mChn.mModId);
                ret = ERR_RGN_ILLEGAL_PARAM;
            }
            cnt++;
        }
        alogv("update region to [%d] channels!", cnt);
    }
    pthread_mutex_unlock(&pRegion->mLock);
    if(pBmpDataWaitFree)
    {
        free(pBmpDataWaitFree);
        pBmpDataWaitFree = NULL;
    }
    return ret;
}

ERRORTYPE AW_MPI_RGN_AttachToChn(RGN_HANDLE Handle, const MPP_CHN_S *pstChn, const RGN_CHN_ATTR_S *pstChnAttr)
{
    ERRORTYPE ret = SUCCESS;
    if(!(Handle>=0 && Handle <RGN_HANDLE_MAX))
    {
        aloge("fatal error! invalid RgnHandle[%d]!", Handle);
        return ERR_RGN_INVALID_CHNID;
    }
    RegionInfo *pRegion;
    if(SUCCESS != searchExistRegion(Handle, &pRegion))
    {
        return ERR_RGN_UNEXIST;
    }
    if(pstChnAttr->enType != pRegion->mRgnAttr.enType)
    {
        aloge("fatal error! RGN_CHN_ATTR's RGNType[0x%x] != RGN_ATTR's RGNType[0x%x]", pstChnAttr->enType, pRegion->mRgnAttr.enType);
        return ERR_RGN_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&pRegion->mLock);
    BOOL bExist = FALSE;
    RegionAttachChnInfo *pChnEntry;
    list_for_each_entry(pChnEntry, &pRegion->mAttachChnList, mList)
    {
        if(pChnEntry->mChn.mModId == pstChn->mModId
            && pChnEntry->mChn.mDevId == pstChn->mDevId
            && pChnEntry->mChn.mChnId == pstChn->mChnId)
        {
            alogw("Be careful! attach channel[0x%x][0x%x][0x%x] is exist, can't attach again!", pChnEntry->mChn.mModId, pChnEntry->mChn.mDevId, pChnEntry->mChn.mChnId);
            bExist = TRUE;
            break;
        }
    }
    if(bExist)
    {
        pthread_mutex_unlock(&pRegion->mLock);
        return ERR_RGN_EXIST;
    }
    BOOL bAddFlag = FALSE;
    //set chn
    if(MOD_ID_VIU == pstChn->mModId)
    {
        if(SUCCESS == AW_MPI_VI_SetRegion(pstChn->mDevId, Handle, &pRegion->mRgnAttr, pstChnAttr, pRegion->mbSetBmp?&pRegion->mBmp:NULL))
        {
            bAddFlag = TRUE;
        }
    }
    else if(MOD_ID_VENC == pstChn->mModId)
    {
        if(SUCCESS == AW_MPI_VENC_SetRegion(pstChn->mChnId, Handle, &pRegion->mRgnAttr, pstChnAttr, pRegion->mbSetBmp?&pRegion->mBmp:NULL))
        {
            bAddFlag = TRUE;
        }
    }
    else
    {
        aloge("fatal error! impossible moduleType[0x%x]", pstChn->mModId);
    }
    //add to list
    if(bAddFlag)
    {
        RegionAttachChnInfo *pAttachChnInfo = (RegionAttachChnInfo*)malloc(sizeof(RegionAttachChnInfo));
        IF_MALLOC_FAIL(pAttachChnInfo);
        memset(pAttachChnInfo, 0, sizeof(RegionAttachChnInfo));
        pAttachChnInfo->mChn = *pstChn;
        pAttachChnInfo->mRgnChnAttr = *pstChnAttr;
        list_add_tail(&pAttachChnInfo->mList, &pRegion->mAttachChnList);
        ret = SUCCESS;
    }
    else
    {
        ret = ERR_RGN_ILLEGAL_PARAM;
    }
    pthread_mutex_unlock(&pRegion->mLock);
    return ret;
}

ERRORTYPE AW_MPI_RGN_DetachFromChn(RGN_HANDLE Handle, const MPP_CHN_S *pstChn)
{
    ERRORTYPE ret = SUCCESS;
    if(!(Handle>=0 && Handle <RGN_HANDLE_MAX))
    {
        aloge("fatal error! invalid RgnHandle[%d]!", Handle);
        return ERR_RGN_INVALID_CHNID;
    }
    if(NULL == pstChn)
    {
        return ERR_RGN_ILLEGAL_PARAM;
    }
    RegionInfo *pRegion;
    if(SUCCESS != searchExistRegion(Handle, &pRegion))
    {
        return ERR_RGN_UNEXIST;
    }
    pthread_mutex_lock(&pRegion->mLock);
    RGN_DetachFromChn_l(pRegion, pstChn);
    pthread_mutex_unlock(&pRegion->mLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_RGN_SetDisplayAttr(RGN_HANDLE Handle, const MPP_CHN_S *pstChn, const RGN_CHN_ATTR_S *pstChnAttr)
{
    ERRORTYPE ret = SUCCESS;
    if(!(Handle>=0 && Handle <RGN_HANDLE_MAX))
    {
        aloge("fatal error! invalid RgnHandle[%d]!", Handle);
        return ERR_RGN_INVALID_CHNID;
    }
    if(NULL == pstChn || NULL == pstChnAttr)
    {
        return ERR_RGN_ILLEGAL_PARAM;
    }
    RegionInfo *pRegion;
    if(SUCCESS != searchExistRegion(Handle, &pRegion))
    {
        return ERR_RGN_UNEXIST;
    }
    pthread_mutex_lock(&pRegion->mLock);
    int nCnt = 0;
    RegionAttachChnInfo *pEntry, *pDstChn;
    list_for_each_entry(pEntry, &pRegion->mAttachChnList, mList)
    {
        if(pEntry->mChn.mModId == pstChn->mModId && pEntry->mChn.mDevId == pstChn->mDevId && pEntry->mChn.mChnId == pstChn->mChnId)
        {
            if(0 == nCnt)
            {
                pDstChn = pEntry;
            }
            nCnt++;
        }
    }
    if(nCnt <= 0)
    {
        aloge("fatal error! not find chn[%d][%d][%d]?", pstChn->mModId, pstChn->mDevId, pstChn->mChnId);
        pthread_mutex_unlock(&pRegion->mLock);
        return ERR_RGN_UNEXIST;
    }
    if(nCnt > 1)
    {
        aloge("fatal error! more [%d]same channels?", nCnt);
    }
    if(pDstChn->mRgnChnAttr.enType != pstChnAttr->enType)
    {
        aloge("fatal error! rgn type [0x%x] != [0x%x]?", pDstChn->mRgnChnAttr.enType, pstChnAttr->enType);
        return ERR_RGN_ILLEGAL_PARAM;
    }
    if(MOD_ID_VIU == pDstChn->mChn.mModId)
    {
        ret = AW_MPI_VI_UpdateRegionChnAttr(pDstChn->mChn.mDevId, Handle, pstChnAttr);
        if(SUCCESS == ret)
        {
            pDstChn->mRgnChnAttr = *pstChnAttr;
        }
    }
    else if(MOD_ID_VENC == pDstChn->mChn.mModId)
    {
        ret = AW_MPI_VENC_UpdateRegionChnAttr(pDstChn->mChn.mChnId, Handle, pstChnAttr);
        if(SUCCESS == ret)
        {
            pDstChn->mRgnChnAttr = *pstChnAttr;
        }
    }
    else
    {
        aloge("fatal error! impossible moduleType[0x%x]", pstChn->mModId);
        ret = ERR_RGN_ILLEGAL_PARAM;
    }
    pthread_mutex_unlock(&pRegion->mLock);
    return ret;
}

ERRORTYPE AW_MPI_RGN_GetDisplayAttr(RGN_HANDLE Handle, const MPP_CHN_S *pstChn, RGN_CHN_ATTR_S *pstChnAttr)
{
    ERRORTYPE ret = SUCCESS;
    if(!(Handle>=0 && Handle <RGN_HANDLE_MAX))
    {
        aloge("fatal error! invalid RgnHandle[%d]!", Handle);
        return ERR_RGN_INVALID_CHNID;
    }
    if(NULL == pstChn || NULL == pstChnAttr)
    {
        return ERR_RGN_ILLEGAL_PARAM;
    }
    RegionInfo *pRegion;
    if(SUCCESS != searchExistRegion(Handle, &pRegion))
    {
        return ERR_RGN_UNEXIST;
    }
    pthread_mutex_lock(&pRegion->mLock);
    int nCnt = 0;
    RegionAttachChnInfo *pEntry, *pDstChn;
    list_for_each_entry(pEntry, &pRegion->mAttachChnList, mList)
    {
        if(pEntry->mChn.mModId == pstChn->mModId && pEntry->mChn.mDevId == pstChn->mDevId && pEntry->mChn.mChnId == pstChn->mChnId)
        {
            if(0 == nCnt)
            {
                pDstChn = pEntry;
            }
            nCnt++;
        }
    }
    if(nCnt <= 0)
    {
        aloge("fatal error! not find chn[%d][%d][%d]?", pstChn->mModId, pstChn->mDevId, pstChn->mChnId);
        pthread_mutex_unlock(&pRegion->mLock);
        return ERR_RGN_UNEXIST;
    }
    if(nCnt > 1)
    {
        aloge("fatal error! more [%d]same channels?", nCnt);
    }
    *pstChnAttr = pDstChn->mRgnChnAttr;
    pthread_mutex_unlock(&pRegion->mLock);
    return ret;
}

//ERRORTYPE AW_MPI_RGN_SetAttachField(RGN_HANDLE Handle, VIDEO_FIELD_E enAttachField);
//ERRORTYPE AW_MPI_RGN_GetAttachField(RGN_HANDLE Handle, VIDEO_FIELD_E *penAttachField);

//ERRORTYPE AW_MPI_RGN_GetCanvasInfo(RGN_HANDLE Handle, RGN_CANVAS_INFO_S *pstCanvasInfo);
//ERRORTYPE AW_MPI_RGN_UpdateCanvas(RGN_HANDLE Handle);

