/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VIPPDrawOsd_V5.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/07/31
  Last Modified :
  Description   : V5 VIPP OSD chip design has some problems: overlay can't support
                overlap, and cover will on top of overlay. But app can't obey this rule, 
                app wants cover below overlay, and overlay maybe overlap.
                So we have to develop this function to regroup osd region that user set, 
                to guarantee overlays after regroup are not overlap.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "VIPPDrawOsd_V5"
#include <utils/plat_log.h>

#include <errno.h>
#include <memory.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

#include <mpi_videoformat_conversion.h>
#include <ChannelRegionInfo.h>
#include <mm_comm_vi.h>
#include <videoInputHw.h>
#include <isp_dev.h>
#include <OsdGroups.h>
#include "VIPPDrawOSD_V5.h"

#include <cdx_list.h>

#define MAX_GLOBAL_ALPHA (16)
//ref to vipp_reg.h
#define MAX_OVERLAY_NUM 64
#define MAX_COVER_NUM 8

#define IF_MALLOC_FAIL(ptr) \
    if(NULL == ptr) \
    { \
        aloge("fatal error! malloc fail!"); \
        return ERR_VI_NOMEM; \
    }
    
extern VIDevManager *gpVIDevManager;



/**
 * @param nPixelFormat V4L2_PIX_FMT_RGB32
 */
int calcBitmapSize(int nPixelFormat, RECT_S *pRect)
{
    int size = 0;
    switch(nPixelFormat)
    {
        case V4L2_PIX_FMT_RGB32:
            size = pRect->Width*pRect->Height*4;
            break;
        case V4L2_PIX_FMT_RGB555:
            size = pRect->Width*pRect->Height*2;
            break;
        default:
            aloge("fatal error! unsupport pixel format[0x%x]?", nPixelFormat);
            break;
    }
    return size;
}

/**
 * @param eV4L2PixFmt : V4L2_PIX_FMT_RGB32 or V4L2_PIX_FMT_RGB555.
 */
static ERRORTYPE mallocBmpBufForOsdRegion(OsdRegion *pRegion, int eV4L2PixFmt)
{
    int nSize;
    if(pRegion->mType != OVERLAY_RGN)
    {
        aloge("fatal error! rgnType[0x%x] is wrong!", pRegion->mType);
        return ERR_VI_INVALID_PARA;
    }
    nSize = calcBitmapSize(eV4L2PixFmt, &pRegion->mRect);
    if(nSize > 0)
    {
        pRegion->mInfo.mOverlay.mBitmap = malloc(nSize);
        if(NULL == pRegion->mInfo.mOverlay.mBitmap)
        {
            aloge("fatal error! malloc [%d]bytes fail", nSize);
            return ERR_VI_NOMEM;
        }
        memset(pRegion->mInfo.mOverlay.mBitmap, 0xFF, nSize);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! rgn size[%dx%d]", pRegion->mRect.Width, pRegion->mRect.Height);
        return ERR_VI_INVALID_PARA;
    }
}

OsdGroup* OsdGroupConstruct()
{
    OsdGroup *pGroup = (OsdGroup*)malloc(sizeof(OsdGroup));
    if(pGroup)
    {
        memset(pGroup, 0, sizeof(OsdGroup));
        INIT_LIST_HEAD(&pGroup->mOsdList);
        INIT_LIST_HEAD(&pGroup->mRedrawOsdList);
        INIT_LIST_HEAD(&pGroup->mList);
    }
    return pGroup;
}

void OsdGroupDestruct(OsdGroup *pGroup)
{
    if(!list_empty(&pGroup->mOsdList))
    {
        OsdRegion *pRegionEntry, *pRegionTmp;
        list_for_each_entry_safe(pRegionEntry, pRegionTmp, &pGroup->mOsdList, mList)
        {
            list_del(&pRegionEntry->mList);
            free(pRegionEntry);
        }
    }
    if(!list_empty(&pGroup->mRedrawOsdList))
    {
        OsdRegion *pRegionEntry, *pRegionTmp;
        list_for_each_entry_safe(pRegionEntry, pRegionTmp, &pGroup->mRedrawOsdList, mList)
        {
            list_del(&pRegionEntry->mList);
            if(OVERLAY_RGN == pRegionEntry->mType)
            {
                if(pRegionEntry->mInfo.mOverlay.mBitmap)
                {
                    free(pRegionEntry->mInfo.mOverlay.mBitmap);
                    pRegionEntry->mInfo.mOverlay.mBitmap = NULL;
                }
            }
            free(pRegionEntry);
        }
    }
    free(pGroup);
}

OsdGroups* OsdGroupsConstruct()
{
    OsdGroups *pGroups = (OsdGroups*)malloc(sizeof(OsdGroups));
    if(NULL == pGroups)
    {
        aloge("fatal error! malloc fail!");
        return NULL;
    }
    memset(pGroups, 0, sizeof(OsdGroups));
    INIT_LIST_HEAD(&pGroups->mGroupList);
    return pGroups;
}

void OsdGroupsDestruct(OsdGroups* pThiz)
{
    OsdGroup *pGroupEntry, *pGroupTmp;
    if(!list_empty(&pThiz->mGroupList))
    {
        list_for_each_entry_safe(pGroupEntry, pGroupTmp, &pThiz->mGroupList, mList)
        {
            list_del(&pGroupEntry->mList);
            OsdGroupDestruct(pGroupEntry);
        }
    }
    free(pThiz);
}

typedef enum
{
    OsdOverlap_None         = 0x00,
    OsdOverlap_OnlyOverlay  = 0x01,
    OsdOverlap_OnlyCover    = 0x02,
    OsdOverlap_CoverOverlay = 0x03,
}OsdOverlapType;
typedef enum
{
    OsdOverlapArea_None             = 0x00,
    OsdOverlapArea_Part             = 0x01,
    OsdOverlapArea_WholeInvolve     = 0x02, //involve other entirely
    OsdOverlapArea_WholeInvolved    = 0x03,    //be involved by other entirely.
}OsdOverlapAreaType;

/**
 * @return int : -1: priority first < second, 0: first==second, 1:priority first > second
 */
static int compareOsdRegionPriority(const OsdRegion *pFirst, const OsdRegion *pSecond)
{
    int ret = 0;
    if(pFirst->mType != pSecond->mType)
    {
        if(OVERLAY_RGN ==pFirst->mType && COVER_RGN == pSecond->mType)
        {
            return 1;
        }
        else if(COVER_RGN ==pFirst->mType && OVERLAY_RGN == pSecond->mType)
        {
            return -1;
        }
        else
        {
            aloge("fatal error! unsupport rgnType[0x%x][0x%x]", pFirst->mType, pSecond->mType);
            return 0;
        }
    }
    if(OVERLAY_RGN == pFirst->mType)
    {
        ret = pFirst->mInfo.mOverlay.mPriority - pSecond->mInfo.mOverlay.mPriority;
        if(ret < 0)
        {
            return -1;
        }
        else if(0 == ret)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else if(COVER_RGN == pFirst->mType)
    {
        ret = pFirst->mInfo.mCover.mPriority - pSecond->mInfo.mCover.mPriority;
        if(ret < 0)
        {
            return -1;
        }
        else if(0 == ret)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        aloge("fatal error! unsupport rgnType[0x%x]", pFirst->mType);
        return 0;
    }
}

/**
 * priority sequence: low -> high.
 * struct list_head *pRegionList : OsdRegion
 */
ERRORTYPE ResortOsdRegionByPriority(struct list_head *pRegionList)
{
    OsdRegion *pEntry, *pTmp;
    pEntry = list_first_entry_or_null(pRegionList, OsdRegion, mList);
    if(NULL == pEntry)
    {
        return SUCCESS;
    }
    int nPrioRet;
    int nPrioRet2;
    OsdRegion *pPrevEntry;
    list_for_each_entry_safe_continue(pEntry, pTmp, pRegionList, mList)
    {
        pPrevEntry = list_prev_entry(pEntry, mList);
        nPrioRet = compareOsdRegionPriority(pPrevEntry, pEntry);
        if(nPrioRet > 0)
        {
            list_del(&pEntry->mList);
            BOOL bInsert = FALSE;
            list_for_each_entry_continue_reverse(pPrevEntry, pRegionList, mList)
            {
                nPrioRet2 = compareOsdRegionPriority(pPrevEntry, pEntry);
                if(nPrioRet2 <= 0)
                {
                    list_add(&pEntry->mList, &pPrevEntry->mList);
                    bInsert = TRUE;
                    break;
                }
            }
            if(FALSE == bInsert)
            {
                list_add(&pEntry->mList, pRegionList);
            }
        }
    }
    return SUCCESS;
}

/**
 * elem type: OsdRegion
 */
static ERRORTYPE ShallowCopyOsdRegionList(struct list_head *pDstHead, struct list_head *pSrcHead)
{
    if(!list_empty(pDstHead))
    {
        aloge("fatal error! dst list is not empty!");
    }
    OsdRegion *pNewEntry;
    OsdRegion *pSrcEntry;
    list_for_each_entry(pSrcEntry, pSrcHead, mList)
    {
        pNewEntry = (OsdRegion*)malloc(sizeof(OsdRegion));
        if(NULL == pNewEntry)
        {
            aloge("fatal error! malloc fail!");
        }
        memcpy(pNewEntry, pSrcEntry, sizeof(OsdRegion));
        list_add_tail(&pNewEntry->mList, pDstHead);
    }
    return SUCCESS;
}

BOOL IfPointBelongToRect(POINT_S *pPt, RECT_S *pRect, /*out*/POINT_S *pRelativePos)
{
    BOOL bBelong;
    if(pPt->X >= pRect->X && pPt->X < pRect->X + pRect->Width && pPt->Y >= pRect->Y && pPt->Y < pRect->Y + pRect->Height)
    {
        bBelong = TRUE;
        if(pRelativePos != NULL)
        {
            pRelativePos->X = pPt->X - pRect->X;
            pRelativePos->Y = pPt->Y - pRect->Y;
        }
    }
    else
    {
        bBelong = FALSE;
    }
    return bBelong;
}
/**
 * bit31 ~ bit 0
 *  A  R  G  B
 */
static unsigned short convertRGB32ToRGB1555(unsigned int nRGB32)
{
    unsigned short nRGB1555 = 0;
    unsigned int alpha = nRGB32 >> 24;
    unsigned int nR = nRGB32 >> 16 & 0xFF;
    unsigned int nG = nRGB32 >> 8 & 0xFF;
    unsigned int nB = nRGB32 & 0xFF;
    if(alpha >= 128)
    {
        nRGB1555 |= 0x8000;
    }
    nRGB1555 |= (nR*31/255 & 0x1F)<<10 | (nG*31/255 & 0x1F)<<5 | (nB*31/255 & 0x1F);
    return nRGB1555;
}

/**
 * origin list is sort by priority: low->high, OsdRegion, cover's priority must smaller than overlay.
 * cover must take effect. overlay can get highest one to take effect.
 * @param eV4L2PixFmt : V4L2_PIX_FMT_RGB32 or V4L2_PIX_FMT_RGB555.
 */
ERRORTYPE DrawRegionByOriginRegions(OsdRegion *pDstRegion, struct list_head *pOriginList, int eV4L2PixFmt)
{
    if(pDstRegion->mType != OVERLAY_RGN)
    {
        aloge("fatal error! rgnType[0x%x] is wrong", pDstRegion->mType);
        return ERR_VI_INVALID_PARA;
    }
    BOOL bCoverExist = FALSE;
    OsdRegion *pRegionEntry;
    list_for_each_entry(pRegionEntry, pOriginList, mList)
    {
        if(COVER_RGN == pRegionEntry->mType)
        {
            bCoverExist = TRUE;
            break;
        }
    }
    //tranverse every point from left to right, from top to bottom.
    unsigned int *pPtRgb32;
    unsigned short *pPtRgb1555;
    int nLineIndex = 0;
    int nColumnIndex = 0;
    for(nLineIndex = 0; nLineIndex < pDstRegion->mRect.Height; nLineIndex++)
    {
        for(nColumnIndex = 0; nColumnIndex < pDstRegion->mRect.Width; nColumnIndex++)
        {
            POINT_S pt = {pDstRegion->mRect.X + nColumnIndex, pDstRegion->mRect.Y + nLineIndex};
            //decide address
            switch(eV4L2PixFmt)
            {
                case V4L2_PIX_FMT_RGB32:
                    pPtRgb32 = (unsigned int*)pDstRegion->mInfo.mOverlay.mBitmap + pDstRegion->mRect.Width*nLineIndex + nColumnIndex;
                    break;
                case V4L2_PIX_FMT_RGB555:
                    pPtRgb1555 = (unsigned short*)pDstRegion->mInfo.mOverlay.mBitmap + pDstRegion->mRect.Width*nLineIndex + nColumnIndex;
                    break;
                default:
                    aloge("fatal error!");
                    break;
            }
            BOOL bFillDone = FALSE;   //this point is filled by one region.
            OsdRegion *pRegionEntry;
            list_for_each_entry_reverse(pRegionEntry, pOriginList, mList)
            {
                if(OVERLAY_RGN == pRegionEntry->mType)
                {
                    if(bFillDone)
                    {
                        continue;
                    }
                    POINT_S RelativePos;
                    if(IfPointBelongToRect(&pt, &pRegionEntry->mRect, &RelativePos))
                    {
                        switch(eV4L2PixFmt)
                        {
                            case V4L2_PIX_FMT_RGB32:
                                *pPtRgb32 = *((unsigned int*)pRegionEntry->mInfo.mOverlay.mBitmap + pRegionEntry->mRect.Width*RelativePos.Y + RelativePos.X);
                                break;
                            case V4L2_PIX_FMT_RGB555:
                                *pPtRgb1555 = *((unsigned short*)pRegionEntry->mInfo.mOverlay.mBitmap + pRegionEntry->mRect.Width*RelativePos.Y + RelativePos.X);
                                break;
                            default:
                                aloge("fatal error!");
                                break;
                        }
                        bFillDone = TRUE;
                        if(bCoverExist)
                        {
                            //need continue to detect.
                            continue;
                        }
                        else
                        {
                            //if cover is not exist, then we can break.
                            break;
                        }
                    }
                    else
                    {
                        continue;
                    }
                }
                else if(COVER_RGN == pRegionEntry->mType)
                {
                    if(IfPointBelongToRect(&pt, &pRegionEntry->mRect, NULL))
                    {
                        if(bFillDone)
                        {
                            //this point alpha must be 0xFF.
                            switch(eV4L2PixFmt)
                            {
                                case V4L2_PIX_FMT_RGB32:
                                    *pPtRgb32 |= 0xFF000000;
                                    break;
                                case V4L2_PIX_FMT_RGB555:
                                    *pPtRgb1555 |= 0x8000;
                                    break;
                                default:
                                    aloge("fatal error!");
                                    break;
                            }
                        }
                        else
                        {
                            //if cover is high priority at this poirt, use cover color.
                            switch(eV4L2PixFmt)
                            {
                                case V4L2_PIX_FMT_RGB32:
                                    *pPtRgb32 = pRegionEntry->mInfo.mCover.mChromaKey | 0xFF000000;
                                    break;
                                case V4L2_PIX_FMT_RGB555:
                                    *pPtRgb1555 = convertRGB32ToRGB1555(pRegionEntry->mInfo.mCover.mChromaKey | 0xFF000000);
                                    break;
                                default:
                                    aloge("fatal error!");
                                    break;
                            }
                            bFillDone = TRUE;
                        }
                        //now we can break.
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    aloge("fatal error!");
                }
            }
        }
    }
    return SUCCESS;
}

/**
 *    pt1        pt2
 *    ------------
 *    |          |
 *    |----------|
 *    pt3        pt4
 * probe dstRect's points which in srcRect. result is stored in PointsArray.
 */
typedef enum
{
    PointPos_LeftTop        = 0,
    PointPos_RightTop       = 1,
    PointPos_LeftBottom     = 2,
    PointPos_RightBottom    = 3,
}PointPositionType;
typedef struct
{
    int mNum;
    POINT_S mPtArray[4];
    PointPositionType mPointIndexArray[4];    //store point index, start from PointPos_LeftTop, to PointPos_RightBottom.
}PointsArray;
/**
 * probe how many points of dstRect are in srcRect.
 */
void ProbePointsInRect(const RECT_S *pDst, const RECT_S *pSrc, PointsArray *pPoints)
{
    memset(pPoints, 0, sizeof(PointsArray));
    POINT_S ptDst[4] = 
        {
            {pDst->X, pDst->Y},
            {pDst->X+pDst->Width-1, pDst->Y},
            {pDst->X, pDst->Y+pDst->Height-1},
            {pDst->X+pDst->Width-1, pDst->Y+pDst->Height-1}
        };
    POINT_S ptSrc[4] = 
        {
            {pSrc->X, pSrc->Y},
            {pSrc->X+pSrc->Width-1, pSrc->Y},
            {pSrc->X, pSrc->Y+pSrc->Height-1},
            {pSrc->X+pSrc->Width-1, pSrc->Y+pSrc->Height-1}
        };
    int i;
    for(i=0; i<4; i++)
    {
        if(ptDst[i].X >= ptSrc[PointPos_LeftTop].X && ptDst[i].X <= ptSrc[PointPos_RightBottom].X
            && ptDst[i].Y >= ptSrc[PointPos_LeftTop].Y && ptDst[i].Y <= ptSrc[PointPos_RightBottom].Y)
        {
            pPoints->mPtArray[pPoints->mNum].X = ptDst[i].X;
            pPoints->mPtArray[pPoints->mNum].Y = ptDst[i].Y;
            pPoints->mPointIndexArray[pPoints->mNum] = i;
            pPoints->mNum++;
        }
    }
}

/**
 * This function only process two cases:
 * (1) cut to half
 *   _______
     |  ____|____
rect |  |_______| region
     |______|
 * (2) base one point to cut.
     ________
     |  _____|___
rect |  |    |  |
     |__|____|  | region
 *      |_______|
 *
 *  (3)cut to more
  *   _______
      |  ____|____
region|  |_______| rect
      |______|

 *  (4) cross overlap
 *          _________
 *      ____|_______|___
 *      |   |       |  |
 *      |___|_______|__|
 *          |_______| 
 *
 *  (5)involve
 *      _________
 *      |       |
 *      |  _    |
 *      | |_|   |
 *      |       |
 *      |_______|
 *
 * other cases is impossible to come here! involve relation is processed before, if meet here, fatal error!
 */
ERRORTYPE CutRegionByRect(const OsdRegion *pRegion, const RECT_S *pRect, struct list_head *pDividedList)
{
    ERRORTYPE ret = SUCCESS;
    if(!list_empty(pDividedList))
    {
        aloge("fatal error! why divided list is not empty?");
        return ERR_VI_INVALID_PARA;
    }
    PointsArray stRectPoints;
    ProbePointsInRect(pRect, &pRegion->mRect, &stRectPoints);
    PointsArray stRegionPoints;
    ProbePointsInRect(&pRegion->mRect, pRect, &stRegionPoints);
    if((0 == stRectPoints.mNum && 2 == stRegionPoints.mNum) || (1 == stRectPoints.mNum && 2 == stRegionPoints.mNum) || (2 == stRectPoints.mNum && 2 == stRegionPoints.mNum))
    {
        //case (1), cut region half.
        OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
        memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
        //there are 4 cases in cut_region_half: top, bottom ,left, right.
        if(PointPos_LeftBottom == stRegionPoints.mPointIndexArray[0] && PointPos_RightBottom == stRegionPoints.mPointIndexArray[1])
        {
            //case (1.1): region is on top of rect.
            if(pRect->Y <= pRegion->mRect.Y)
            {
                aloge("fatal error! check rect [%d,%d, %dx%d],[%d,%d, %dx%d]", 
                    pRect->X, pRect->Y, pRect->Width, pRect->Height, 
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height);
            }
            pDivideRegion->mRect.Height = pRect->Y - pRegion->mRect.Y;
        }
        else if(PointPos_LeftTop == stRegionPoints.mPointIndexArray[0] && PointPos_RightTop == stRegionPoints.mPointIndexArray[1])
        {
            //case (1.2): region is under bottom of rect.
            if(pRect->Y + pRect->Height <= pRegion->mRect.Y || pRect->Y + pRect->Height >= pRegion->mRect.Y + pRegion->mRect.Height)
            {
                aloge("fatal error! check rect [%d,%d, %dx%d],[%d,%d, %dx%d]", 
                    pRect->X, pRect->Y, pRect->Width, pRect->Height, 
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height);
            }
            pDivideRegion->mRect.Y = pRect->Y + pRect->Height;
            pDivideRegion->mRect.Height = (pRegion->mRect.Y + pRegion->mRect.Height) - (pRect->Y + pRect->Height);
        }
        else if(PointPos_RightTop == stRegionPoints.mPointIndexArray[0] && PointPos_RightBottom == stRegionPoints.mPointIndexArray[1])
        {
            //case (1.3): region is at left of rect.
            if(pRect->X <= pRegion->mRect.X)
            {
                aloge("fatal error! check rect [%d,%d, %dx%d],[%d,%d, %dx%d]", 
                    pRect->X, pRect->Y, pRect->Width, pRect->Height, 
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height);
            }
            pDivideRegion->mRect.Width = pRect->X - pRegion->mRect.X;
        }
        else if(PointPos_LeftTop == stRegionPoints.mPointIndexArray[0] && PointPos_LeftBottom == stRegionPoints.mPointIndexArray[1])
        {
            //case (1.4): region is at right of rect.
            if(pRect->X + pRect->Width <= pRegion->mRect.X || pRect->X + pRect->Width >= pRegion->mRect.X + pRegion->mRect.Width)
            {
                aloge("fatal error! check rect [%d,%d, %dx%d],[%d,%d, %dx%d]", 
                    pRect->X, pRect->Y, pRect->Width, pRect->Height, 
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height);
            }
            pDivideRegion->mRect.X = pRect->X + pRect->Width;
            pDivideRegion->mRect.Width = (pRegion->mRect.X + pRegion->mRect.Width) - (pRect->X + pRect->Width);
        }
        else
        {
            aloge("fatal error! check rect [%d,%d, %dx%d],[%d,%d, %dx%d]", 
                    pRect->X, pRect->Y, pRect->Width, pRect->Height, 
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height);
        }
        list_add_tail(&pDivideRegion->mList, pDividedList);
        return ret;
    }
    else if(1 == stRectPoints.mNum && 1 == stRegionPoints.mNum)
    {
        //case (2), base one point to cut
        //there are 4 cases in base_one_point_cut: leftTop , rightTop , leftBottom, rightBottom.
        BOOL bError = FALSE;
        if(PointPos_LeftTop == stRectPoints.mPointIndexArray[0])
        {
            POINT_S PtLeftTop = stRectPoints.mPtArray[0];
            if(!(PtLeftTop.X == pRect->X && PtLeftTop.Y == pRect->Y))
            {
                aloge("fatal error! point error [%d,%d]!=[%d,%d]", PtLeftTop.X, PtLeftTop.Y, pRect->X, pRect->Y);
            }
            //case (2.1) point is leftTop in Region
            if(PtLeftTop.Y == pRegion->mRect.Y)
            {
                if(PtLeftTop.X > pRegion->mRect.X)
                {
                    //cut one region is enough.
                    OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.Width = PtLeftTop.X - pRegion->mRect.X;
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                }
                else
                {
                    aloge("fatal error! X");
                    bError = TRUE;
                }
            }
            else if(PtLeftTop.Y > pRegion->mRect.Y)
            {
                if(PtLeftTop.Y < pRegion->mRect.Y + pRegion->mRect.Height)
                {
                    if(PtLeftTop.X == pRegion->mRect.X)
                    {
                        //cut one region is enough.
                        OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                        memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                        pDivideRegion->mRect.Height = PtLeftTop.Y - pRegion->mRect.Y;
                        list_add_tail(&pDivideRegion->mList, pDividedList);
                    }
                    else if(PtLeftTop.X > pRegion->mRect.X)
                    {
                        //cut two region.
                        OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                        memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                        pDivideRegion->mRect.Height = PtLeftTop.Y - pRegion->mRect.Y;
                        list_add_tail(&pDivideRegion->mList, pDividedList);
                        
                        pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                        memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                        pDivideRegion->mRect.Y = PtLeftTop.Y;
                        pDivideRegion->mRect.Width = PtLeftTop.X - pRegion->mRect.X;
                        pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - PtLeftTop.Y;
                        list_add_tail(&pDivideRegion->mList, pDividedList);
                    }
                    else
                    {
                        aloge("fatal error! X");
                        bError = TRUE;
                    }
                }
                else
                {
                    aloge("fatal error! Height");
                    bError = TRUE;
                }
            }
            else
            {
                aloge("fatal error! Y");
                bError = TRUE;
            }
        }
        else if(PointPos_RightTop == stRectPoints.mPointIndexArray[0])
        {
            //case (2.2) point is rightTop in Region
            POINT_S PtRightTop = stRectPoints.mPtArray[0];
            if(!(PtRightTop.X == pRect->X+pRect->Width-1 && PtRightTop.Y == pRect->Y))
            {
                aloge("fatal error! point error [%d,%d]!=[%d,%d, %dx%d]", PtRightTop.X, PtRightTop.Y, pRect->X, pRect->Y, pRect->Width, pRect->Height);
            }
            if(PtRightTop.Y == pRegion->mRect.Y)
            {
                if(PtRightTop.X >= pRegion->mRect.X + pRegion->mRect.Width - 1)
                {
                    aloge("fatal error! X");
                    bError = TRUE;
                }
                //cut one region is enough.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.X = PtRightTop.X+1;
                pDivideRegion->mRect.Width = pRegion->mRect.X + pRegion->mRect.Width - (PtRightTop.X + 1);
                list_add_tail(&pDivideRegion->mList, pDividedList);
            }
            else if(PtRightTop.Y > pRegion->mRect.Y)
            {
                if(PtRightTop.Y >= pRegion->mRect.Y + pRegion->mRect.Height)
                {
                    aloge("fatal error! Height");
                    bError = TRUE;
                }
                if(PtRightTop.X == pRegion->mRect.X + pRegion->mRect.Width - 1)
                {
                    //cut one region is enough.
                    OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.Height = PtRightTop.Y - pRegion->mRect.Y;
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                }
                else if(PtRightTop.X < pRegion->mRect.X + pRegion->mRect.Width - 1)
                {
                    //cut two region.
                    OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.Height = PtRightTop.Y - pRegion->mRect.Y;
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                    
                    pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.X = PtRightTop.X+1;
                    pDivideRegion->mRect.Y = PtRightTop.Y;
                    pDivideRegion->mRect.Width = pRegion->mRect.X + pRegion->mRect.Width - (PtRightTop.X + 1);
                    pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - PtRightTop.Y;
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                }
                else
                {
                    aloge("fatal error! X");
                    bError = TRUE;
                }
            }
            else
            {
                aloge("fatal error! Y");
                bError = TRUE;
            }
        }
        else if(PointPos_LeftBottom == stRectPoints.mPointIndexArray[0])
        {
            //case (2.3) point is leftBottom in Region
            POINT_S PtLeftBottom = stRectPoints.mPtArray[0];
            if(!(PtLeftBottom.X == pRect->X && PtLeftBottom.Y == pRect->Y + pRect->Height - 1))
            {
                aloge("fatal error! point error [%d,%d]!=[%d,%d, %dx%d]", PtLeftBottom.X, PtLeftBottom.Y, pRect->X, pRect->Y, pRect->Width, pRect->Height);
            }
            if(PtLeftBottom.Y == pRegion->mRect.Y + pRegion->mRect.Height - 1)
            {
                if(PtLeftBottom.X <= pRegion->mRect.X)
                {
                    aloge("fatal error! X");
                    bError = TRUE;
                }
                //cut one region is enough.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Width = PtLeftBottom.X - pRegion->mRect.X;
                list_add_tail(&pDivideRegion->mList, pDividedList);
            }
            else if(PtLeftBottom.Y < pRegion->mRect.Y + pRegion->mRect.Height - 1)
            {
                if(PtLeftBottom.Y < pRegion->mRect.Y)
                {
                    aloge("fatal error! Y");
                    bError = TRUE;
                }
                if(PtLeftBottom.X == pRegion->mRect.X)
                {
                    //cut one region is enough.
                    OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.Y = PtLeftBottom.Y + 1;
                    pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - (PtLeftBottom.Y + 1);
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                }
                else if(PtLeftBottom.X > pRegion->mRect.X)
                {
                    //cut two region.
                    OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.Width = PtLeftBottom.X - pRegion->mRect.X;
                    pDivideRegion->mRect.Height = PtLeftBottom.Y - pRegion->mRect.Y + 1;
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                    
                    pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.Y = PtLeftBottom.Y + 1;
                    pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - (PtLeftBottom.Y + 1);
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                }
                else
                {
                    aloge("fatal error! X");
                    bError = TRUE;
                }
            }
            else
            {
                aloge("fatal error! Y");
                bError = TRUE;
            }
        }
        else if(PointPos_RightBottom == stRectPoints.mPointIndexArray[0])
        {
            //case(2.4) point is rightBottom in Region
            POINT_S PtRightBottom = stRectPoints.mPtArray[0];
            if(!(PtRightBottom.X == pRect->X + pRect->Width - 1 && PtRightBottom.Y == pRect->Y + pRect->Height - 1))
            {
                aloge("fatal error! point error [%d,%d]!=[%d,%d, %dx%d]", PtRightBottom.X, PtRightBottom.Y, pRect->X, pRect->Y, pRect->Width, pRect->Height);
            }
            if(PtRightBottom.Y == pRegion->mRect.Y + pRegion->mRect.Height - 1)
            {
                if(PtRightBottom.X < pRegion->mRect.X || PtRightBottom.X >= pRegion->mRect.X + pRegion->mRect.Width - 1)
                {
                    aloge("fatal error! X");
                    bError = TRUE;
                }
                //cut one region is enough.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.X = PtRightBottom.X + 1;
                pDivideRegion->mRect.Width = pRegion->mRect.X + pRegion->mRect.Width - (PtRightBottom.X + 1);
                list_add_tail(&pDivideRegion->mList, pDividedList);
            }
            else if(PtRightBottom.Y < pRegion->mRect.Y + pRegion->mRect.Height - 1)
            {
                if(PtRightBottom.Y < pRegion->mRect.Y)
                {
                    aloge("fatal error! Y");
                    bError = TRUE;
                }
                if(PtRightBottom.X == pRegion->mRect.X + pRegion->mRect.Width - 1)
                {
                    //cut one region is enough.
                    OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.Y = PtRightBottom.Y + 1;
                    pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - (PtRightBottom.Y + 1);
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                }
                else if(PtRightBottom.X < pRegion->mRect.X + pRegion->mRect.Width - 1)
                {
                    if(PtRightBottom.X < pRegion->mRect.X)
                    {
                        aloge("fatal error! X");
                        bError = TRUE;
                    }
                    //cut two region.
                    OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.X = PtRightBottom.X+1;
                    pDivideRegion->mRect.Width = pRegion->mRect.X + pRegion->mRect.Width - (PtRightBottom.X+1);
                    pDivideRegion->mRect.Height = PtRightBottom.Y - pRegion->mRect.Y + 1;
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                    
                    pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                    memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                    pDivideRegion->mRect.Y = PtRightBottom.Y + 1;
                    pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - (PtRightBottom.Y + 1);
                    list_add_tail(&pDivideRegion->mList, pDividedList);
                }
                else
                {
                    aloge("fatal error! X");
                    bError = TRUE;
                }
            }
            else
            {
                aloge("fatal error! Y");
                bError = TRUE;
            }
        }
        else
        {
            aloge("fatal error! unknown point pos is[0x%x]?", stRectPoints.mPointIndexArray[0]);
        }

        if(bError)
        {
            aloge("fatal error! check rect [%d,%d, %dx%d],[%d,%d, %dx%d]", 
                    pRect->X, pRect->Y, pRect->Width, pRect->Height, 
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height);
            ret = ERR_VI_INVALID_PARA;
        }
        return ret;
    }
    else if((2 == stRectPoints.mNum && 0 == stRegionPoints.mNum) || (2 == stRectPoints.mNum && 1 == stRegionPoints.mNum))
    {
        //case(3) cut more.
        BOOL bError = FALSE;
        //case(3.1) rect is at top of region.
        if(PointPos_LeftBottom == stRectPoints.mPointIndexArray[0] && PointPos_RightBottom == stRectPoints.mPointIndexArray[1])
        {
            int rgnNum = 0;
            POINT_S PtLeftBottom = stRectPoints.mPtArray[0];
            POINT_S PtRightBottom = stRectPoints.mPtArray[1];
            if(PtLeftBottom.X > pRegion->mRect.X)
            {
                //leftTop region is cutted out.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Width = PtLeftBottom.X - pRegion->mRect.X;
                pDivideRegion->mRect.Height = PtLeftBottom.Y - pRegion->mRect.Y + 1;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            if(PtRightBottom.X < pRegion->mRect.X + pRegion->mRect.Width - 1)
            {
                //rightTop region is cutted out.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.X = PtRightBottom.X + 1;
                pDivideRegion->mRect.Width = pRegion->mRect.X + pRegion->mRect.Width - pDivideRegion->mRect.X;
                pDivideRegion->mRect.Height = PtRightBottom.Y - pRegion->mRect.Y + 1;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            if(PtLeftBottom.Y < pRegion->mRect.Y + pRegion->mRect.Height - 1)
            {
                //bottom region is cutted out
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Y = PtLeftBottom.Y + 1;
                pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - pDivideRegion->mRect.Y;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            //judge if wrong.
            if(0 == stRegionPoints.mNum && 2 == rgnNum)
            {
                if(PtLeftBottom.Y != pRegion->mRect.Y + pRegion->mRect.Height - 1)
                {
                    aloge("fatal error! cut region exception!");
                    bError = TRUE;
                }
            }
            else if(1 == stRegionPoints.mNum)
            {
                if(rgnNum != 2)
                {
                    aloge("fatal error! cut region exception2!");
                    bError = TRUE;
                }
            }
            if(bError)
            {
                aloge("fatal error! region[%d,%d,%dx%d], rect[%d,%d,%dx%d], ptLeftBottom[%d,%d], ptRightBottom[%d,%d]",
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height,
                    pRect->X, pRect->Y, pRect->Width, pRect->Height,
                    PtLeftBottom.X, PtLeftBottom.Y, PtRightBottom.X, PtRightBottom.Y);
                ret = ERR_VI_INVALID_PARA;
            }
        }
        //case(3.2) rect is at bottom of region.
        else if(PointPos_LeftTop == stRectPoints.mPointIndexArray[0] && PointPos_RightTop == stRectPoints.mPointIndexArray[1])
        {
            int rgnNum = 0;
            POINT_S PtLeftTop = stRectPoints.mPtArray[0];
            POINT_S PtRightTop = stRectPoints.mPtArray[1];
            if(PtLeftTop.X > pRegion->mRect.X)
            {
                //leftbottom region is cutted out.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Y = PtLeftTop.Y;
                pDivideRegion->mRect.Width = PtLeftTop.X - pRegion->mRect.X;
                pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - pDivideRegion->mRect.Y;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            if(PtRightTop.X < pRegion->mRect.X + pRegion->mRect.Width - 1)
            {
                //rightBottom region is cutted out.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.X = PtRightTop.X + 1;
                pDivideRegion->mRect.Y = PtRightTop.Y;
                pDivideRegion->mRect.Width = pRegion->mRect.X + pRegion->mRect.Width - pDivideRegion->mRect.X;
                pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - pDivideRegion->mRect.Y;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            if(PtLeftTop.Y > pRegion->mRect.Y)
            {
                //top region is cutted out
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Height = PtLeftTop.Y - pRegion->mRect.Y;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            //judge if wrong.
            if(0 == stRegionPoints.mNum && 2 == rgnNum)
            {
                if(PtLeftTop.Y != pRegion->mRect.Y)
                {
                    aloge("fatal error! cut region exception!");
                    bError = TRUE;
                }
            }
            else if(1 == stRegionPoints.mNum)
            {
                if(rgnNum != 2)
                {
                    aloge("fatal error! cut region exception2!");
                    bError = TRUE;
                }
            }
            if(bError)
            {
                aloge("fatal error! region[%d,%d,%dx%d], rect[%d,%d,%dx%d], ptLeftTop[%d,%d], ptRightTop[%d,%d]",
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height,
                    pRect->X, pRect->Y, pRect->Width, pRect->Height,
                    PtLeftTop.X, PtLeftTop.Y, PtRightTop.X, PtRightTop.Y);
                ret = ERR_VI_INVALID_PARA;
            }
        }
        //case(3.3) rect is at left of region.
        else if(PointPos_RightTop == stRectPoints.mPointIndexArray[0] && PointPos_RightBottom == stRectPoints.mPointIndexArray[1])
        {
            int rgnNum = 0;
            POINT_S PtRightTop = stRectPoints.mPtArray[0];
            POINT_S PtRightBottom = stRectPoints.mPtArray[1];
            if(PtRightTop.Y > pRegion->mRect.Y)
            {
                //top region is cutted out.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Height = PtRightTop.Y - pRegion->mRect.Y;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            if(PtRightBottom.Y < pRegion->mRect.Y + pRegion->mRect.Height - 1)
            {
                //bottom region is cutted out.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Y = PtRightBottom.Y + 1;
                pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - pDivideRegion->mRect.Y;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            if(PtRightTop.X < pRegion->mRect.X + pRegion->mRect.Width - 1)
            {
                //middle region is cutted out
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.X = PtRightTop.X + 1;
                pDivideRegion->mRect.Y = PtRightTop.Y;
                pDivideRegion->mRect.Width = pRegion->mRect.X + pRegion->mRect.Width - pDivideRegion->mRect.X;
                pDivideRegion->mRect.Height = PtRightBottom.Y - PtRightTop.Y + 1;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            //judge if wrong.
            if(0 == stRegionPoints.mNum && 2 == rgnNum)
            {
                if(PtRightTop.X != pRegion->mRect.X + pRegion->mRect.Width - 1)
                {
                    aloge("fatal error! cut region exception!");
                    bError = TRUE;
                }
            }
            else if(1 == stRegionPoints.mNum)
            {
                if(rgnNum != 2)
                {
                    aloge("fatal error! cut region exception2!");
                    bError = TRUE;
                }
            }
            if(bError)
            {
                aloge("fatal error! region[%d,%d,%dx%d], rect[%d,%d,%dx%d], ptRightTop[%d,%d], ptRightBottom[%d,%d]",
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height,
                    pRect->X, pRect->Y, pRect->Width, pRect->Height,
                    PtRightTop.X, PtRightTop.Y, PtRightBottom.X, PtRightBottom.Y);
                ret = ERR_VI_INVALID_PARA;
            }
        }
        //case(3.4) rect is at right of region.
        else if(PointPos_LeftTop == stRectPoints.mPointIndexArray[0] && PointPos_LeftBottom == stRectPoints.mPointIndexArray[1])
        {
            int rgnNum = 0;
            POINT_S PtLeftTop = stRectPoints.mPtArray[0];
            POINT_S PtLeftBottom = stRectPoints.mPtArray[1];
            if(PtLeftTop.Y > pRegion->mRect.Y)
            {
                //top region is cutted out.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Height = PtLeftTop.Y - pRegion->mRect.Y;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            if(PtLeftBottom.Y < pRegion->mRect.Y + pRegion->mRect.Height - 1)
            {
                //bottom region is cutted out.
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Y = PtLeftBottom.Y + 1;
                pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - pDivideRegion->mRect.Y;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            if(PtLeftTop.X > pRegion->mRect.X)
            {
                //middle region is cutted out
                OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
                memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
                pDivideRegion->mRect.Y = PtLeftTop.Y;
                pDivideRegion->mRect.Width = PtLeftTop.X - pRegion->mRect.X;
                pDivideRegion->mRect.Height = PtLeftBottom.Y - PtLeftTop.Y + 1;
                list_add_tail(&pDivideRegion->mList, pDividedList);
                rgnNum++;
            }
            //judge if wrong.
            if(0 == stRegionPoints.mNum && 2 == rgnNum)
            {
                if(PtLeftTop.X != pRegion->mRect.X)
                {
                    aloge("fatal error! cut region exception!");
                    bError = TRUE;
                }
            }
            else if(1 == stRegionPoints.mNum)
            {
                if(rgnNum != 2)
                {
                    aloge("fatal error! cut region exception2!");
                    bError = TRUE;
                }
            }
            if(bError)
            {
                aloge("fatal error! region[%d,%d,%dx%d], rect[%d,%d,%dx%d], ptLeftTop[%d,%d], ptLeftBottom[%d,%d]",
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height,
                    pRect->X, pRect->Y, pRect->Width, pRect->Height,
                    PtLeftTop.X, PtLeftTop.Y, PtLeftBottom.X, PtLeftBottom.Y);
                ret = ERR_VI_INVALID_PARA;
            }
        }
        else
        {
            aloge("fatal error! check region[%d,%d,%dx%d], rect[%d,%d,%dx%d]",
                    pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height,
                    pRect->X, pRect->Y, pRect->Width, pRect->Height);
            ret = ERR_VI_INVALID_PARA;
        }
        return ret;
    }
    else if(0 == stRectPoints.mNum && 0 == stRegionPoints.mNum)
    {
        //case(4) cross overlap.
        POINT_S ptRegion[4] = 
        {
            {pRegion->mRect.X, pRegion->mRect.Y},
            {pRegion->mRect.X+pRegion->mRect.Width-1, pRegion->mRect.Y},
            {pRegion->mRect.X, pRegion->mRect.Y+pRegion->mRect.Height-1},
            {pRegion->mRect.X+pRegion->mRect.Width-1, pRegion->mRect.Y+pRegion->mRect.Height-1}
        };
        POINT_S ptRect[4] = 
        {
            {pRect->X, pRect->Y},
            {pRect->X+pRect->Width-1, pRect->Y},
            {pRect->X, pRect->Y+pRect->Height-1},
            {pRect->X+pRect->Width-1, pRect->Y+pRect->Height-1}
        };
        if(    ptRegion[PointPos_LeftTop].X     < ptRect[PointPos_LeftTop].X 
            && ptRegion[PointPos_LeftTop].Y     > ptRect[PointPos_LeftTop].Y
            && ptRegion[PointPos_RightBottom].X > ptRect[PointPos_RightBottom].X
            && ptRegion[PointPos_RightBottom].Y < ptRect[PointPos_RightBottom].Y    )
        {
            //case(4.1) region is divided to left and right.
            POINT_S PtLeftTop = ptRect[PointPos_LeftTop];
            POINT_S PtRightBottom = ptRect[PointPos_RightBottom];
            OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
            memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
            pDivideRegion->mRect.Width = PtLeftTop.X - pRegion->mRect.X;
            list_add_tail(&pDivideRegion->mList, pDividedList);
            pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
            memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
            pDivideRegion->mRect.X = PtRightBottom.X + 1;
            pDivideRegion->mRect.Width = pRegion->mRect.X+pRegion->mRect.Width - pDivideRegion->mRect.X;
            list_add_tail(&pDivideRegion->mList, pDividedList);
        }
        else if(   ptRegion[PointPos_LeftTop].X     > ptRect[PointPos_LeftTop].X
                && ptRegion[PointPos_LeftTop].Y     < ptRect[PointPos_LeftTop].Y
                && ptRegion[PointPos_RightBottom].X < ptRect[PointPos_RightBottom].X
                && ptRegion[PointPos_RightBottom].Y > ptRect[PointPos_RightBottom].Y    )
        {
            //case(4.2) region is divided to top and bottom.
            POINT_S PtLeftTop = ptRect[PointPos_LeftTop];
            POINT_S PtRightBottom = ptRect[PointPos_RightBottom];
            OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
            memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
            pDivideRegion->mRect.Height = PtLeftTop.Y - pRegion->mRect.Y;
            list_add_tail(&pDivideRegion->mList, pDividedList);
            pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
            memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
            pDivideRegion->mRect.Y = PtRightBottom.Y + 1;
            pDivideRegion->mRect.Height = pRegion->mRect.Y+pRegion->mRect.Height - pDivideRegion->mRect.Y;
            list_add_tail(&pDivideRegion->mList, pDividedList);
        }
        else
        {
            aloge("fatal error! region must overlap here!");
            ret = ERR_VI_INVALID_PARA;
        }
        return ret;
    }
    else if(4 == stRectPoints.mNum)
    {
        BOOL bError = FALSE;
        //case(5) involve.
        if(!(stRegionPoints.mNum>=0 && stRegionPoints.mNum<=2))
        {
            aloge("fatal error! region points number[%d] wrong!", stRegionPoints.mNum);
            return ERR_VI_INVALID_PARA;
        }
        int rgnNum = 0;
        //divide to 4 region max, from top to bottom, left to right.
        if(stRectPoints.mPtArray[PointPos_LeftTop].Y > pRegion->mRect.Y)
        {
            //top region can be divided
            OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
            memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
            pDivideRegion->mRect.Height = stRectPoints.mPtArray[PointPos_LeftTop].Y - pRegion->mRect.Y;
            list_add_tail(&pDivideRegion->mList, pDividedList);
            rgnNum++;
        }
        if(stRectPoints.mPtArray[PointPos_LeftTop].X > pRegion->mRect.X)
        {
            //left region can be divided
            OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
            memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
            pDivideRegion->mRect.Y = stRectPoints.mPtArray[PointPos_LeftTop].Y;
            pDivideRegion->mRect.Width = stRectPoints.mPtArray[PointPos_LeftTop].X - pRegion->mRect.X;
            pDivideRegion->mRect.Height = stRectPoints.mPtArray[PointPos_RightBottom].Y - stRectPoints.mPtArray[PointPos_LeftTop].Y + 1;
            list_add_tail(&pDivideRegion->mList, pDividedList);
            rgnNum++;
        }
        if(stRectPoints.mPtArray[PointPos_RightTop].X < pRegion->mRect.X + pRegion->mRect.Width - 1)
        {
            //right region can be divided
            OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
            memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
            pDivideRegion->mRect.X = stRectPoints.mPtArray[PointPos_RightTop].X + 1;
            pDivideRegion->mRect.Y = stRectPoints.mPtArray[PointPos_RightTop].Y;
            pDivideRegion->mRect.Width = pRegion->mRect.X + pRegion->mRect.Width - pDivideRegion->mRect.X;
            pDivideRegion->mRect.Height = stRectPoints.mPtArray[PointPos_RightBottom].Y - stRectPoints.mPtArray[PointPos_RightTop].Y + 1;
            list_add_tail(&pDivideRegion->mList, pDividedList);
            rgnNum++;
        }
        if(stRectPoints.mPtArray[PointPos_LeftBottom].Y < pRegion->mRect.Y + pRegion->mRect.Height - 1)
        {
            //bottom region can be divided
            OsdRegion *pDivideRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
            memcpy(pDivideRegion, pRegion, sizeof(OsdRegion));
            pDivideRegion->mRect.Y = stRectPoints.mPtArray[PointPos_LeftBottom].Y + 1;
            pDivideRegion->mRect.Height = pRegion->mRect.Y + pRegion->mRect.Height - pDivideRegion->mRect.Y;
            list_add_tail(&pDivideRegion->mList, pDividedList);
            rgnNum++;
        }
        //judge if wrong.
        int nSideNum = 0;
        if(stRectPoints.mPtArray[PointPos_LeftTop].X == pRegion->mRect.X)
        {
            nSideNum++;
        }
        if(stRectPoints.mPtArray[PointPos_LeftTop].Y == pRegion->mRect.Y)
        {
            nSideNum++;
        }
        if(stRectPoints.mPtArray[PointPos_RightBottom].X == pRegion->mRect.X + pRegion->mRect.Width - 1)
        {
            nSideNum++;
        }
        if(stRectPoints.mPtArray[PointPos_RightBottom].Y == pRegion->mRect.Y + pRegion->mRect.Height - 1)
        {
            nSideNum++;
        }
        if(0 == nSideNum)
        {
            if(rgnNum != 4)
            {
                aloge("fatal error! sideNum[%d], rgnNum[%d]!=4", nSideNum, rgnNum);
                bError = TRUE;
            }
        }
        else if(1 == nSideNum)
        {
            if(rgnNum != 3)
            {
                aloge("fatal error! sideNum[%d], rgnNum[%d]!=3", nSideNum, rgnNum);
                bError = TRUE;
            }
        }
        else if(2 == nSideNum)
        {
            if(rgnNum != 2)
            {
                aloge("fatal error! sideNum[%d], rgnNum[%d]!=2", nSideNum, rgnNum);
                bError = TRUE;
            }
        }
        else if(3 == nSideNum)
        {
            if(rgnNum != 1)
            {
                aloge("fatal error! sideNum[%d], rgnNum[%d]!=1", nSideNum, rgnNum);
                bError = TRUE;
            }
        }
        else
        {
            aloge("fatal error! sideNum[%d], rgnNum[%d]!=1", nSideNum, rgnNum);
            bError = TRUE;
        }
        if(bError)
        {
            aloge("fatal error! region[%d,%d,%dx%d], rect[%d,%d,%dx%d]",
                pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height,
                pRect->X, pRect->Y, pRect->Width, pRect->Height);
            ret = ERR_VI_INVALID_PARA;
        }
        return ret;
    }
    else
    {
        aloge("fatal error! stRectPoints[%d], stRegionPoints[%d], rect[%d,%d,%dx%d], regionRect[%d,%d,%dx%d]", 
            stRectPoints.mNum, stRegionPoints.mNum, 
            pRect->X, pRect->Y, pRect->Width, pRect->Height,
            pRegion->mRect.X, pRegion->mRect.Y, pRegion->mRect.Width, pRegion->mRect.Height);
        return ERR_VI_INVALID_PARA;
    }
}

/**
 *    pt1        pt2
 *    ------------
 *    |          |
 *    |----------|
 *    pt3        pt4
 * involve prior than involved!
 * careful: OsdOverlapArea_Part has cases blow:
 *          dstPointsInSrc  srcPointsInDst
 *      (1)         1          1
 *      (2)         1          2
 *      (3)         2          0
 *      (4)         2          1
 *      (5)         2          2
 *      (6)         0          2
 *      (7)         0          0
 *  If case(0, 0), it means:
 *          _________
 *      ____|_______|___
 *      |   |       |  |
 *      |___|_______|__|
 *          |_______| 
 * @return 
 *      OsdOverlapArea_WholeInvolve: pDst overlap pSrc entirely.
 *      OsdOverlapArea_WholeInvolved: pDst is overlaped by pSrc entirely.
 */ 
OsdOverlapAreaType IfOsdRegionOverlap(RECT_S *pDst, RECT_S *pSrc)
{
    POINT_S ptDst[4] = 
        {
            {pDst->X, pDst->Y},
            {pDst->X+pDst->Width-1, pDst->Y},
            {pDst->X, pDst->Y+pDst->Height-1},
            {pDst->X+pDst->Width-1, pDst->Y+pDst->Height-1}
        };
    POINT_S ptSrc[4] = 
        {
            {pSrc->X, pSrc->Y},
            {pSrc->X+pSrc->Width-1, pSrc->Y},
            {pSrc->X, pSrc->Y+pSrc->Height-1},
            {pSrc->X+pSrc->Width-1, pSrc->Y+pSrc->Height-1}
        };
    int i;
    BOOL bDstPtIn[4];
    for(i=0; i<4; i++)
    {
        if(ptDst[i].X >= ptSrc[PointPos_LeftTop].X && ptDst[i].X <= ptSrc[PointPos_RightBottom].X
            && ptDst[i].Y >= ptSrc[PointPos_LeftTop].Y && ptDst[i].Y <= ptSrc[PointPos_RightBottom].Y)
        {
            bDstPtIn[i] = TRUE;
        }
        else
        {
            bDstPtIn[i] = FALSE;
        }
    }
    int nDstInNum = 0;
    for(i=0; i<4; i++)
    {
        if(bDstPtIn[i])
        {
            nDstInNum++;
        }
    }

    BOOL bSrcPtIn[4];
    for(i=0; i<4; i++)
    {
        if(ptSrc[i].X >= ptDst[PointPos_LeftTop].X && ptSrc[i].X <= ptDst[PointPos_RightBottom].X
            && ptSrc[i].Y >= ptDst[PointPos_LeftTop].Y && ptSrc[i].Y <= ptDst[PointPos_RightBottom].Y)
        {
            bSrcPtIn[i] = TRUE;
        }
        else
        {
            bSrcPtIn[i] = FALSE;
        }
    }
    int nSrcInNum = 0;
    for(i=0; i<4; i++)
    {
        if(bSrcPtIn[i])
        {
            nSrcInNum++;
        }
    }

    if(4 == nSrcInNum)
    {
        return OsdOverlapArea_WholeInvolve;
    }
    if(4 == nDstInNum)
    {
        return OsdOverlapArea_WholeInvolved;
    }
    if(nDstInNum > 0 || nSrcInNum > 0)
    {
        return OsdOverlapArea_Part;
    }
    else if(0 == nDstInNum && 0 == nSrcInNum)
    {
        if( (  ptDst[PointPos_LeftTop].X     < ptSrc[PointPos_LeftTop].X 
            && ptDst[PointPos_LeftTop].Y     > ptSrc[PointPos_LeftTop].Y
            && ptDst[PointPos_RightBottom].X > ptSrc[PointPos_RightBottom].X
            && ptDst[PointPos_RightBottom].Y < ptSrc[PointPos_RightBottom].Y)
        ||
            (  ptDst[PointPos_LeftTop].X     > ptSrc[PointPos_LeftTop].X
            && ptDst[PointPos_LeftTop].Y     < ptSrc[PointPos_LeftTop].Y
            && ptDst[PointPos_RightBottom].X < ptSrc[PointPos_RightBottom].X
            && ptDst[PointPos_RightBottom].Y > ptSrc[PointPos_RightBottom].Y)   )
        {
            return OsdOverlapArea_Part;
        }
        else
        {
            return OsdOverlapArea_None;
        }
    }
    else
    {
        aloge("fatal error! region overlap wrong. nDstInNum[%d], nSrcInNum[%d], dstRgn[%d,%d,%dx%d], srcRgn[%d,%d,%dx%d]", 
            nDstInNum, nSrcInNum, pDst->X, pDst->Y, pDst->Width, pDst->Height,
            pSrc->X, pSrc->Y, pSrc->Width, pSrc->Height);
        return OsdOverlapArea_None;
    }
}

OsdOverlapType IfOverlayCoverOverlap(OsdGroup *pGroup)
{
    BOOL bOverlayExist = FALSE;
    BOOL bCoverExist = FALSE;

    OsdRegion *pEntry;
    list_for_each_entry(pEntry, &pGroup->mOsdList, mList)
    {
        if(OVERLAY_RGN == pEntry->mType)
        {
            bOverlayExist = TRUE;
        }
        else if(COVER_RGN == pEntry->mType)
        {
            bCoverExist = TRUE;
        }
        else
        {
            aloge("fatal error!");
        }
    }
    if(bOverlayExist && bCoverExist)
    {
        return OsdOverlap_CoverOverlay;
    }
    else if(bOverlayExist)
    {
        return OsdOverlap_OnlyOverlay;
    }
    else if(bCoverExist)
    {
        return OsdOverlap_OnlyCover;
    }
    else
    {
        aloge("fatal error!");
        return OsdOverlap_None;
    }
}
/**
 * pRegionList: type is OsdRegion.
 * @return TRUE:involved, FALSE:not involved.
 */
BOOL IfOsdRegionInvolvedByOsdRegions(OsdRegion *pRegion, struct list_head *pRegionList)
{
    if(!list_empty(pRegionList))
    {
        BOOL bInvolved = FALSE;
        OsdRegion *pEntry;
        OsdOverlapAreaType eOverlapAreaType;
        list_for_each_entry(pEntry, pRegionList, mList)
        {
            eOverlapAreaType = IfOsdRegionOverlap(&pEntry->mRect, &pRegion->mRect);
            if(OsdOverlapArea_WholeInvolve == eOverlapAreaType)
            {
                bInvolved = TRUE;
                break;
            }
        }
        return bInvolved;
    }
    else
    {
        return FALSE;
    }
}
/**
 * According To OsdList to redraw new OsdList which don't overlap each other.
 * We will malloc buffer for RedrawOsdList.
 * divide region method: overloop from top and cut, tranverse node from bottom to top.
 * resort cover in pure cover list.
 *
 * @param nPixelFormat pixel format of overlay, V4L2_PIX_FMT_RGB32
 * @param pCoverGroup contain cover cutted, MAX_COVER_NUM is max.
 */
ERRORTYPE RedrawOsdGroup_V5(OsdGroup *pGroup, int nPixelFormat, OsdGroup *pCoverGroup)
{
    INIT_LIST_HEAD(&pGroup->mRedrawOsdList);
    OsdOverlapType eOverlapType = IfOverlayCoverOverlap(pGroup);
    //(1) only covers
    if(OsdOverlap_OnlyCover == eOverlapType)
    {
        //keep OsdList. let RedrawOsdList be empty.
        ResortOsdRegionByPriority(&pGroup->mOsdList);
        return SUCCESS;
    }
    else if(OsdOverlap_OnlyOverlay == eOverlapType || OsdOverlap_CoverOverlay == eOverlapType)
    {
        //if only one overlay, keep OsdList.
        //if multi overlay, construct RedrawOsdList.
        if(!list_is_singular(&pGroup->mOsdList))
        {
            ResortOsdRegionByPriority(&pGroup->mOsdList);
            //copy OsdList to RedrawOsdList
            LIST_HEAD(tmpOsdList);
            ShallowCopyOsdRegionList(&tmpOsdList, &pGroup->mOsdList);
            //divide method: 
            while(1)
            {
                OsdOverlapAreaType eOverlapAreaType;
                OsdRegion *pEntry, *pTmp;
                pEntry = list_first_entry_or_null(&tmpOsdList, OsdRegion, mList);
                if(NULL == pEntry)
                {
                    //all region has been processed, can break now
                    break;
                }
                LIST_HEAD(regionDividedList);
                list_move_tail(&pEntry->mList, &regionDividedList);
                list_for_each_entry(pEntry, &tmpOsdList, mList)
                {
                    //divide all entries of regionDividedList by every region of tmpOsdList, so need tranverse regionDividedList
                    LIST_HEAD(tmpRegionDividedList);
                    OsdRegion *pRegionDividedEntry, *pRegionDividedTmp;
                    list_for_each_entry_safe(pRegionDividedEntry, pRegionDividedTmp, &regionDividedList, mList)
                    {
                        //(2.1)look if this entry is involved entirely.
                        if(IfOsdRegionInvolvedByOsdRegions(pRegionDividedEntry, &tmpOsdList))
                        {
                            list_del(&pRegionDividedEntry->mList);
                            free(pRegionDividedEntry);
                            continue;
                        }
                        //(2.2)divide dividedEntry to unoverlapped regions, add to tmpRegionDividedList tail. free it if divided.
                        eOverlapAreaType = IfOsdRegionOverlap(&pEntry->mRect, &pRegionDividedEntry->mRect);
                        if(OsdOverlapArea_None == eOverlapAreaType)
                        {
                        }
                        else if(OsdOverlapArea_Part == eOverlapAreaType)
                        {
                            //merge is prior, if dstRegion's zero point, one point is in srcRegion, divide srcRegion.
                            //if dstRegion's two points are in srcRegion, devide dstRegion.
                            //must divide dividedEntry.
                            //divide OsdRegion to several regions, put increased regions to tmpRegionDividedList tail.
                            PointsArray stPoints;
                            ProbePointsInRect(&pEntry->mRect, &pRegionDividedEntry->mRect, &stPoints);
                            if(0 == stPoints.mNum)
                            {
                                //only one case is possible: srcEntry has two points in DstEntry.
                                //cut srcEntry
                                PointsArray srcPoints;
                                ProbePointsInRect(&pRegionDividedEntry->mRect, &pEntry->mRect, &srcPoints);
                                if(2 == srcPoints.mNum || 0 == srcPoints.mNum)
                                {
                                    LIST_HEAD(regionCuttedList);
                                    if(SUCCESS!=CutRegionByRect(pRegionDividedEntry, &pEntry->mRect, &regionCuttedList))
                                    {
                                        aloge("fatal error! why rect is not cut?cutRgn[%d,%d,%dx%d], refRgn[%d,%d,%dx%d]", 
                                            pRegionDividedEntry->mRect.X, pRegionDividedEntry->mRect.Y, pRegionDividedEntry->mRect.Width, pRegionDividedEntry->mRect.Height,
                                            pEntry->mRect.X, pEntry->mRect.Y, pEntry->mRect.Width, pEntry->mRect.Height);
                                    }
                                    list_splice_tail_init(&regionCuttedList, &tmpRegionDividedList);
                                    list_del(&pRegionDividedEntry->mList);
                                    free(pRegionDividedEntry);
                                }
                                else
                                {
                                    aloge("fatal error! check code!");
                                }
                            }
                            else if(1 == stPoints.mNum)
                            {
                                LIST_HEAD(regionCuttedList);
                                if(SUCCESS!=CutRegionByRect(pRegionDividedEntry, &pEntry->mRect, &regionCuttedList))
                                {
                                    aloge("fatal error! why rect is not cut?cutRgn[%d,%d,%dx%d], refRgn[%d,%d,%dx%d]", 
                                        pRegionDividedEntry->mRect.X, pRegionDividedEntry->mRect.Y, pRegionDividedEntry->mRect.Width, pRegionDividedEntry->mRect.Height,
                                        pEntry->mRect.X, pEntry->mRect.Y, pEntry->mRect.Width, pEntry->mRect.Height);
                                }
                                list_splice_tail_init(&regionCuttedList, &tmpRegionDividedList);
                                list_del(&pRegionDividedEntry->mList);
                                free(pRegionDividedEntry);
                            }
                            else if(2 == stPoints.mNum)
                            {
                                LIST_HEAD(regionCuttedList);
                                if(SUCCESS!=CutRegionByRect(pRegionDividedEntry, &pEntry->mRect, &regionCuttedList))
                                {
                                    aloge("fatal error! why rect is not cut?cutRgn[%d,%d,%dx%d], refRgn[%d,%d,%dx%d]", 
                                        pRegionDividedEntry->mRect.X, pRegionDividedEntry->mRect.Y, pRegionDividedEntry->mRect.Width, pRegionDividedEntry->mRect.Height,
                                        pEntry->mRect.X, pEntry->mRect.Y, pEntry->mRect.Width, pEntry->mRect.Height);
                                }
                                list_splice_tail_init(&regionCuttedList, &tmpRegionDividedList);
                                list_del(&pRegionDividedEntry->mList);
                                free(pRegionDividedEntry);
                            }
                            else
                            {
                                aloge("fatal error! overlay area type is part, check code!");
                            }
                        }
                        else if(OsdOverlapArea_WholeInvolved == eOverlapAreaType)
                        {
                            LIST_HEAD(regionCuttedList);
                            if(SUCCESS!=CutRegionByRect(pRegionDividedEntry, &pEntry->mRect, &regionCuttedList))
                            {
                                aloge("fatal error! why rect is not cut? cutRgn[%d,%d,%dx%d], refRgn[%d,%d,%dx%d]", 
                                    pRegionDividedEntry->mRect.X, pRegionDividedEntry->mRect.Y, pRegionDividedEntry->mRect.Width, pRegionDividedEntry->mRect.Height,
                                    pEntry->mRect.X, pEntry->mRect.Y, pEntry->mRect.Width, pEntry->mRect.Height);
                            }
                            list_splice_tail_init(&regionCuttedList, &tmpRegionDividedList);
                            list_del(&pRegionDividedEntry->mList);
                            free(pRegionDividedEntry);
                        }
                        else
                        {
                            aloge("fatal error! overlapAreaType[0x%x]?", eOverlapAreaType);
                        }
                    }
                    //add new divided regions to regionDividedList
                    list_splice_tail_init(&tmpRegionDividedList, &regionDividedList);
                }
                //add new region divided list to tmpDividedlist
                list_splice_tail_init(&regionDividedList, &pGroup->mRedrawOsdList);
            }
            //redraw list is constructed now, then malloc for them.
            int nLeftCoverNum = 0;
            if(pCoverGroup)
            {
                int cnt = 0;
                struct list_head *pList;
                list_for_each(pList, &pCoverGroup->mOsdList) { cnt++; }
                if(cnt <= MAX_COVER_NUM)
                {
                    nLeftCoverNum = MAX_COVER_NUM - cnt;
                    alogd("left [%d]covers can use", nLeftCoverNum);
                }
                else
                {
                    aloge("fatal error! cover number[%d] is exceed!", cnt);
                    nLeftCoverNum = 0;
                }
            }
            OsdRegion *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pGroup->mRedrawOsdList, mList)
            {
                if(OVERLAY_RGN == pEntry->mType)
                {
                    mallocBmpBufForOsdRegion(pEntry, nPixelFormat);
                }
                else if(COVER_RGN == pEntry->mType)
                {
                    if(nLeftCoverNum > 0)
                    {
                        list_move_tail(&pEntry->mList, &pCoverGroup->mOsdList);
                        nLeftCoverNum--;
                    }
                    else
                    {
                        alogd("Be careful! cover region will be translate to overlay region.");
                        pEntry->mType = OVERLAY_RGN;
                        pEntry->mInfo.mOverlay.mBitmap = NULL;
                        pEntry->mInfo.mOverlay.mbInvColEn = FALSE;
                        pEntry->mInfo.mOverlay.mPriority = 0;   //cover all set lowest priority when transform to overlay.
                        mallocBmpBufForOsdRegion(pEntry, nPixelFormat);
                    }
                }
                else
                {
                    aloge("fatal error! rgnType[0x%x] is wrong!", pEntry->mType);
                }
                //alogd("for debug: redraw rect[%d,%d, %dx%d]", pEntry->mRect.X, pEntry->mRect.Y, pEntry->mRect.Width, pEntry->mRect.Height);
            }
            //fill these bmpBufs of redrawList.
            ERRORTYPE ret = SUCCESS;
            list_for_each_entry(pEntry, &pGroup->mRedrawOsdList, mList)
            {
                ret = DrawRegionByOriginRegions(pEntry, &pGroup->mOsdList, nPixelFormat);
                if(ret!=SUCCESS)
                {
                    break;
                }
            }
            return ret;
        }
        else
        {
            return SUCCESS;
        }
    }
    else
    {
        aloge("fatal error!");
        return ERR_VI_INVALID_PARA;
    }
}

ERRORTYPE AddRegionToOsdGroup_V5(OsdGroup *pGroup, ChannelRegionInfo *pRegion)
{
    OsdRegion *pOsdRegion = (OsdRegion*)malloc(sizeof(OsdRegion));
    IF_MALLOC_FAIL(pOsdRegion);

    memset(pOsdRegion, 0, sizeof(OsdRegion));
    pOsdRegion->mType = pRegion->mRgnAttr.enType;
    if(OVERLAY_RGN == pRegion->mRgnAttr.enType)
    {
        pOsdRegion->mRect.X = pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X;
        pOsdRegion->mRect.Y = pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y;
        pOsdRegion->mRect.Width = pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width;
        pOsdRegion->mRect.Height = pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height;
    }
    else if(COVER_RGN == pRegion->mRgnAttr.enType)
    {
        if(AREA_RECT == pRegion->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType)
        {
            pOsdRegion->mRect = pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect;
        }
        else
        {
            aloge("fatal error! cover type[0x%x] is not rect!", pRegion->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType);
        }
    }
    else
    {
        aloge("fatal error!");
    }
    if(OVERLAY_RGN == pOsdRegion->mType)
    {
        pOsdRegion->mInfo.mOverlay.mBitmap = pRegion->mBmp.mpData;
        pOsdRegion->mInfo.mOverlay.mbInvColEn = pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn;
        pOsdRegion->mInfo.mOverlay.mPriority = pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mLayer;
    }
    else if(COVER_RGN == pOsdRegion->mType)
    {
        pOsdRegion->mInfo.mCover.mChromaKey = pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mColor;
        pOsdRegion->mInfo.mCover.mPriority = pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mLayer;
    }
    else
    {
        aloge("fatal error! unknown osd type[0x%x]", pOsdRegion->mType);
    }
    list_add_tail(&pOsdRegion->mList, &pGroup->mOsdList);
    return SUCCESS;
}

/**
 *    pt1        pt2
 *    ------------
 *    |          |
 *    |----------|
 *    pt3        pt4
 */ 
BOOL IfOsdRegionOverlapRegion_V5(OsdRegion *pDst, ChannelRegionInfo *pSrc)
{
    BOOL bOverlap = FALSE;
    POINT_S ptDst[4] = 
        {
            {pDst->mRect.X, pDst->mRect.Y},
            {pDst->mRect.X+pDst->mRect.Width-1, pDst->mRect.Y},
            {pDst->mRect.X, pDst->mRect.Y+pDst->mRect.Height-1},
            {pDst->mRect.X+pDst->mRect.Width-1, pDst->mRect.Y+pDst->mRect.Height-1}
        };
    RECT_S srcRect;
    if(OVERLAY_RGN == pSrc->mRgnAttr.enType)
    {
        srcRect.X = pSrc->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X;
        srcRect.Y = pSrc->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y;
        srcRect.Width = pSrc->mRgnAttr.unAttr.stOverlay.mSize.Width;
        srcRect.Height = pSrc->mRgnAttr.unAttr.stOverlay.mSize.Height;
    }
    else if(COVER_RGN == pSrc->mRgnAttr.enType)
    {
        if(AREA_RECT == pSrc->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType)
        {
            srcRect = pSrc->mRgnChnAttr.unChnAttr.stCoverChn.stRect;
        }
        else
        {
            aloge("fatal error! cover type[0x%x] is not rect!", pSrc->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType);
        }
    }
    else
    {
        aloge("fatal error!");
    }
    POINT_S ptSrc[4] = 
        {
            {srcRect.X, srcRect.Y},
            {srcRect.X+srcRect.Width-1, srcRect.Y},
            {srcRect.X, srcRect.Y+srcRect.Height-1},
            {srcRect.X+srcRect.Width-1, srcRect.Y+srcRect.Height-1}
        };
    for(int i=0; i<4; i++)
    {
        if(ptDst[i].X >= ptSrc[PointPos_LeftTop].X && ptDst[i].X <= ptSrc[PointPos_RightBottom].X
            && ptDst[i].Y >= ptSrc[PointPos_LeftTop].Y && ptDst[i].Y <= ptSrc[PointPos_RightBottom].Y)
        {
            bOverlap = TRUE;
            break;
        }
    }
    if(FALSE == bOverlap)
    {
        for(int i=0; i<4; i++)
        {
            if(ptSrc[i].X >= ptDst[PointPos_LeftTop].X && ptSrc[i].X <= ptDst[PointPos_RightBottom].X
                && ptSrc[i].Y >= ptDst[PointPos_LeftTop].Y && ptSrc[i].Y <= ptDst[PointPos_RightBottom].Y)
            {
                bOverlap = TRUE;
                break;
            }
        }
    }
    if(FALSE == bOverlap)
    {
        //anyone's point is not in other's rect. There is still one possible to overlap.
        //ref to IfOsdRegionOverlap()
        if( (  ptDst[PointPos_LeftTop].X     < ptSrc[PointPos_LeftTop].X 
            && ptDst[PointPos_LeftTop].Y     > ptSrc[PointPos_LeftTop].Y
            && ptDst[PointPos_RightBottom].X > ptSrc[PointPos_RightBottom].X
            && ptDst[PointPos_RightBottom].Y < ptSrc[PointPos_RightBottom].Y)
        ||
            (  ptDst[PointPos_LeftTop].X     > ptSrc[PointPos_LeftTop].X
            && ptDst[PointPos_LeftTop].Y     < ptSrc[PointPos_LeftTop].Y
            && ptDst[PointPos_RightBottom].X < ptSrc[PointPos_RightBottom].X
            && ptDst[PointPos_RightBottom].Y > ptSrc[PointPos_RightBottom].Y)   )
        {
            bOverlap = TRUE;
        }
    }
    return bOverlap;
}

BOOL IfOsdGroupOverlapRegion_V5(OsdGroup *pGroup, ChannelRegionInfo *pRegion)
{
    BOOL bOverlap = FALSE;
    OsdRegion *pEntry;
    list_for_each_entry(pEntry, &pGroup->mOsdList, mList)
    {
        if(IfOsdRegionOverlapRegion_V5(pEntry, pRegion))
        {
            bOverlap = TRUE;
            break;
        }
    }
    return bOverlap;
}

/**
 * reconstruct osd:
 * (1)group
 * (2)check if cover&overlay overlap
 * (3)every group divide into unoverlapped regions
 * (4)every region calculate color value.
 * (5)all covers are put in one group, this group is at first node.
 * we don't resort here.
 */
ERRORTYPE ConfigOsdGroups_V5(OsdGroups *pOsdGroups, viChnManager *pVipp)
{
    memset(pOsdGroups, 0, sizeof(OsdGroups));
    pOsdGroups->mPixFmt = V4L2_PIX_FMT_RGB32;
    pOsdGroups->mGlobalAlpha = MAX_GLOBAL_ALPHA;
    INIT_LIST_HEAD(&pOsdGroups->mGroupList);
    
    //step 0: join list of draw
    LIST_HEAD(DrawList);    //ChannelRegionInfo
    ChannelRegionInfo *pRegionEntry;
    list_for_each_entry(pRegionEntry, &pVipp->mCoverList, mList)
    {
        if(pRegionEntry->mbDraw)
        {
            list_add_tail(&pRegionEntry->mTmpList, &DrawList);
        }
    }
    BOOL bFlag = FALSE;
    list_for_each_entry(pRegionEntry, &pVipp->mOverlayList, mList)
    {
        if(FALSE == bFlag)
        {
            pOsdGroups->mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pRegionEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt);
            if(V4L2_PIX_FMT_RGB32 == pOsdGroups->mPixFmt)
            {
                pOsdGroups->mGlobalAlpha = MAX_GLOBAL_ALPHA;
            }
            else if(V4L2_PIX_FMT_RGB555 == pOsdGroups->mPixFmt)
            {
                pOsdGroups->mGlobalAlpha = (unsigned int)pRegionEntry->mRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha * MAX_GLOBAL_ALPHA/128;
            }
            else
            {
                aloge("fatal error! unkown pix fmt [0x%x]->[0x%x]", pRegionEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt, pOsdGroups->mPixFmt);
            }
            bFlag = TRUE;
        }
        if(pRegionEntry->mbDraw)
        {
            list_add_tail(&pRegionEntry->mTmpList, &DrawList);
        }
    }
    //step 1: group
    list_for_each_entry(pRegionEntry, &DrawList, mTmpList)
    {
        //put every region to a osd group
        int nOverlapGroupNum = 0;
        OsdGroup *pGroupEntry, *pGroupTmp;
        OsdGroup *pDstGroup;
        list_for_each_entry_safe(pGroupEntry, pGroupTmp, &pOsdGroups->mGroupList, mList)
        {
            //find a group which overlap current region
            if(IfOsdGroupOverlapRegion_V5(pGroupEntry, pRegionEntry))
            {
                if(0 == nOverlapGroupNum)
                {
                    //put to this group
                    AddRegionToOsdGroup_V5(pGroupEntry, pRegionEntry);
                    pDstGroup = pGroupEntry;
                }
                else
                {
                    //merge current group into dst group
                    list_splice_tail_init(&pGroupEntry->mOsdList, &pDstGroup->mOsdList);
                    list_del(&pGroupEntry->mList);
                    free(pGroupEntry);
                }
                nOverlapGroupNum++;
            }
        }
        if(0 == nOverlapGroupNum)
        {
            //need add a group
            OsdGroup *pGroup = OsdGroupConstruct();
            IF_MALLOC_FAIL(pGroup);
            AddRegionToOsdGroup_V5(pGroup, pRegionEntry);
            list_add_tail(&pGroup->mList, &pOsdGroups->mGroupList);
        }
    }
    // step 2: merge all cover groups to one, and check if cover&overlay overlap
    int nCoverGroupNum = 0;
    OsdGroup *pDstCoverGroup = NULL;
    OsdGroup *pGroupEntry, *pGroupTmp;
    list_for_each_entry_safe(pGroupEntry, pGroupTmp, &pOsdGroups->mGroupList, mList)
    {
        if(OsdOverlap_OnlyCover == IfOverlayCoverOverlap(pGroupEntry))
        {
            if(0 == nCoverGroupNum)
            {
                pDstCoverGroup = pGroupEntry;
            }
            else
            {
                //merge current group into dst group
                list_splice_tail_init(&pGroupEntry->mOsdList, &pDstCoverGroup->mOsdList);
                list_del(&pGroupEntry->mList);
                free(pGroupEntry);
            }
            nCoverGroupNum++;
        }
        else if(OsdOverlap_CoverOverlay == IfOverlayCoverOverlap(pGroupEntry))
        {
            pOsdGroups->mGlobalAlpha = MAX_GLOBAL_ALPHA;
        }
    }
    BOOL bCoverGroupExist = FALSE;
    if(nCoverGroupNum > 0)
    {
        alogd("merge [%d] cover groups into one, and move it to first node", nCoverGroupNum);
        list_move(&pDstCoverGroup->mList, &pOsdGroups->mGroupList);
        bCoverGroupExist = TRUE;
    }
    OsdGroup *pTmpCoverGroup = NULL;
    if(!bCoverGroupExist)
    {
        pTmpCoverGroup = OsdGroupConstruct();
    }
    //step 3: divide every group into unoverlapped regions
    list_for_each_entry(pGroupEntry, &pOsdGroups->mGroupList, mList)
    {
        RedrawOsdGroup_V5(pGroupEntry, pOsdGroups->mPixFmt, bCoverGroupExist?pDstCoverGroup:pTmpCoverGroup);
    }
    if(!bCoverGroupExist)
    {
        if(!list_empty(&pTmpCoverGroup->mOsdList))
        {
            list_add(&pTmpCoverGroup->mList, &pOsdGroups->mGroupList);
        }
        else
        {
            OsdGroupDestruct(pTmpCoverGroup);
        }
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_DrawOSD_V5(VI_DEV vipp_id)
{
    ERRORTYPE ret = SUCCESS;
    viChnManager *pVipp = gpVIDevManager->gpVippManager[vipp_id];
    struct isp_video_device *video = gpVIDevManager->media->video_dev[vipp_id];
    //convert to OsdGroups *mpOsdGroups;
    if(pVipp->mpOsdGroups)
    {
        aloge("fatal error! why OsdGroups not destruct?");
        OsdGroupsDestruct(pVipp->mpOsdGroups);
        pVipp->mpOsdGroups = NULL;
    }
    pVipp->mpOsdGroups = OsdGroupsConstruct();
    ConfigOsdGroups_V5(pVipp->mpOsdGroups, pVipp);
    //now all overlays are not overlap. all covers may be overlap.
    //draw cover
    OsdGroup *pFirstGroup = list_first_entry_or_null(&pVipp->mpOsdGroups->mGroupList, OsdGroup, mList);
    BOOL bCoverExist = FALSE;
    if(pFirstGroup)
    {
        if(list_empty(&pFirstGroup->mRedrawOsdList))    //when redraw, can't be cover
        {
            if(!list_empty(&pFirstGroup->mOsdList))
            {
                OsdRegion *pFirstRegion = list_first_entry(&pFirstGroup->mOsdList, OsdRegion, mList);
                if(COVER_RGN == pFirstRegion->mType)
                {
                    bCoverExist = TRUE;
                }
            }
            else
            {
                aloge("fatal error! osdList in OsdGroup can't be empty!");
            }
        }
    }
    if(bCoverExist)
    {
        struct osd_fmt stOsdFmt;
        memset(&stOsdFmt, 0, sizeof(struct osd_fmt));
        OsdRegion *pEntry;
        list_for_each_entry(pEntry, &pFirstGroup->mOsdList, mList)
        {
            if(COVER_RGN == pEntry->mType)
            {
                stOsdFmt.global_alpha = pVipp->mpOsdGroups->mGlobalAlpha;   //MAX_GLOBAL_ALPHA
                stOsdFmt.rgb_cover[stOsdFmt.clipcount] = pEntry->mInfo.mCover.mChromaKey;
                stOsdFmt.bitmap[stOsdFmt.clipcount] = NULL;
                stOsdFmt.region[stOsdFmt.clipcount].left = pEntry->mRect.X;
                stOsdFmt.region[stOsdFmt.clipcount].top = pEntry->mRect.Y;
                stOsdFmt.region[stOsdFmt.clipcount].width = pEntry->mRect.Width;
                stOsdFmt.region[stOsdFmt.clipcount].height = pEntry->mRect.Height;
                stOsdFmt.clipcount++;
                if(stOsdFmt.clipcount > MAX_COVER_NUM)
                {
                    aloge("fatal error! clipcount[%d] exceed!", stOsdFmt.clipcount);
                }
            }
            else
            {
                aloge("fatal error! region type[0x%x] is not cover!", pEntry->mType);
            }
        }
        int ret1 = overlay_set_fmt(video, &stOsdFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set cover fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! cover update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    else
    {
        struct osd_fmt stOsdFmt;
        memset(&stOsdFmt, 0, sizeof(struct osd_fmt));
        stOsdFmt.clipcount = 0;
        stOsdFmt.bitmap[0] = NULL;
        int ret1 = overlay_set_fmt(video, &stOsdFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set cover fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! cover update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    //draw overlay
    BOOL bOverlayExist = FALSE;
    OsdGroup *pEntryGroup;
    list_for_each_entry(pEntryGroup, &pVipp->mpOsdGroups->mGroupList, mList)
    {
        if(!list_empty(&pEntryGroup->mRedrawOsdList))
        {
            OsdRegion *pFirstRegion = list_first_entry(&pEntryGroup->mRedrawOsdList, OsdRegion, mList);
            if(OVERLAY_RGN == pFirstRegion->mType)
            {
                bOverlayExist = TRUE;
                break;
            }
            else
            {
                aloge("fatal error! redrawOsdList must be overlay!");
            }
        }
        else
        {
            if(!list_empty(&pEntryGroup->mOsdList))
            {
                OsdRegion *pFirstRegion = list_first_entry(&pEntryGroup->mOsdList, OsdRegion, mList);
                if(OVERLAY_RGN == pFirstRegion->mType)
                {
                    if(list_is_singular(&pEntryGroup->mOsdList))
                    {
                        bOverlayExist = TRUE;
                        break;
                    }
                    else
                    {
                        aloge("fatal error! when not redraw, overlay in OsdList must be single!");
                    }
                }
            }
            else
            {
                aloge("fatal error! osdList in OsdGroup can't be empty!");
            }
        }
    }
    if(bOverlayExist)
    {
        BOOL bCountExceed = FALSE;
        struct osd_fmt stOsdFmt;
        memset(&stOsdFmt, 0, sizeof(struct osd_fmt));
        stOsdFmt.chromakey = pVipp->mpOsdGroups->mPixFmt;
        stOsdFmt.global_alpha = pVipp->mpOsdGroups->mGlobalAlpha; //MAX_GLOBAL_ALPHA
        list_for_each_entry_from(pEntryGroup, &pVipp->mpOsdGroups->mGroupList, mList)
        {
            if(!list_empty(&pEntryGroup->mRedrawOsdList))
            {
                OsdRegion *pEntryRegion;
                list_for_each_entry(pEntryRegion, &pEntryGroup->mRedrawOsdList, mList)
                {
                    if(pEntryRegion->mType != OVERLAY_RGN)
                    {
                        aloge("fatal error! region type[0x%x] is not overlay!", pEntryRegion->mType);
                    }
                    if(NULL == pEntryRegion->mInfo.mOverlay.mBitmap)
                    {
                        aloge("fatal error! bmpData is not set");
                    }
                    stOsdFmt.reverse_close[stOsdFmt.clipcount] = pEntryRegion->mInfo.mOverlay.mbInvColEn?0:1;
                    stOsdFmt.bitmap[stOsdFmt.clipcount] = pEntryRegion->mInfo.mOverlay.mBitmap;
                    stOsdFmt.region[stOsdFmt.clipcount].left = pEntryRegion->mRect.X;
                    stOsdFmt.region[stOsdFmt.clipcount].top = pEntryRegion->mRect.Y;
                    stOsdFmt.region[stOsdFmt.clipcount].width = pEntryRegion->mRect.Width;
                    stOsdFmt.region[stOsdFmt.clipcount].height = pEntryRegion->mRect.Height;
                    stOsdFmt.clipcount++;
                    if(stOsdFmt.clipcount > MAX_OVERLAY_NUM)
                    {
                        aloge("fatal error! clipcount[%d] exceed!", stOsdFmt.clipcount);
                        bCountExceed = TRUE;
                        break;
                    }
                }
                if(bCountExceed)
                {
                    break;
                }
            }
            else
            {
                if(list_is_singular(&pEntryGroup->mOsdList))
                {
                    OsdRegion *pEntryRegion = list_first_entry(&pEntryGroup->mOsdList, OsdRegion, mList);
                    if(pEntryRegion->mType != OVERLAY_RGN)
                    {
                        aloge("fatal error! region type[0x%x] is not overlay!", pEntryRegion->mType);
                    }
                    if(NULL == pEntryRegion->mInfo.mOverlay.mBitmap)
                    {
                        aloge("fatal error! bmpData is not set");
                    }
                    stOsdFmt.reverse_close[stOsdFmt.clipcount] = pEntryRegion->mInfo.mOverlay.mbInvColEn?0:1;
                    stOsdFmt.bitmap[stOsdFmt.clipcount] = pEntryRegion->mInfo.mOverlay.mBitmap;
                    stOsdFmt.region[stOsdFmt.clipcount].left = pEntryRegion->mRect.X;
                    stOsdFmt.region[stOsdFmt.clipcount].top = pEntryRegion->mRect.Y;
                    stOsdFmt.region[stOsdFmt.clipcount].width = pEntryRegion->mRect.Width;
                    stOsdFmt.region[stOsdFmt.clipcount].height = pEntryRegion->mRect.Height;
                    stOsdFmt.clipcount++;
                    if(stOsdFmt.clipcount > MAX_OVERLAY_NUM)
                    {
                        aloge("fatal error! clipcount[%d] exceed!", stOsdFmt.clipcount);
                        bCountExceed = TRUE;
                        break;
                    }
                }
                else
                {
                    aloge("fatal error! This overlay group must be single!");
                }
            }
        }
        char bitmap[100];
        if(0 == stOsdFmt.clipcount)
        {
            bitmap[0] = 'c';
            stOsdFmt.bitmap[0] = &bitmap[0];
        }
        int ret1 = overlay_set_fmt(video, &stOsdFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set overlay fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! overlay update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    else
    {
        struct osd_fmt stOsdFmt;
        memset(&stOsdFmt, 0, sizeof(struct osd_fmt));
        char bitmap[100];
        bitmap[0] = 'c';
        stOsdFmt.clipcount = 0;
        stOsdFmt.bitmap[0] = &bitmap[0];
        int ret1 = overlay_set_fmt(video, &stOsdFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set overlay fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! overlay update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    //after draw, we can destroy OsdGroups
    if(pVipp->mpOsdGroups)
    {
        OsdGroupsDestruct(pVipp->mpOsdGroups);
        pVipp->mpOsdGroups = NULL;
    }
    return ret;
}

