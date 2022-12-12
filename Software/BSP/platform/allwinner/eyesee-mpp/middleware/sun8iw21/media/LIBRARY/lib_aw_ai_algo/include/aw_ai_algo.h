#ifndef __AW_AI_ALGO__
#define __AW_AI_ALGO__

#include "aw_ai_core.h"

#if ( defined _WIN64 || defined _WIN32 )
//#ifdef AW_AI_ALGO_EXPORTS
//#define AW_AI_API __declspec(dllexport)
//#else
//#define AW_AI_API __declspec(dllimport)
//#endif
//#else
#define AW_AI_API
#endif

AW_AI_API int  AW_AI_FindHomography(LP_POINT_F pSrcPoints, LP_POINT_F pDstPoints, int iCount, double *pWrapMatrix, float fDistThresh = 5.0);
AW_AI_API void AW_AI_GoodFeaturesToTrack(LP_IMAGE_DATA_I pImage, LP_POINT_F pCornerPoints, int* pCount, int maxCorners, double qualityLevel, double minDistance, int blockSize = 3, double k = 0.04);
AW_AI_API void AW_AI_WarpPerspective(const LP_IMAGE_DATA_I pSrcImage, LP_IMAGE_DATA_I pDstImage, double *pMatrix);
AW_AI_API int  AW_AI_OpticalFlow(const LP_IMAGE_DATA_I pPrevImg, const LP_IMAGE_DATA_I pCurrImg, LP_POINT_F pPrevPts, LP_POINT_F pCurrPts, int iCount, char* pStats, float* pError, int maxLevel, int winSize, float term_criteria_eps, int term_criteria_count);

#endif
