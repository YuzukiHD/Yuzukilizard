
/*
******************************************************************************
*
* MPI_AF.c
*
* Hawkview ISP - mpi_af.c module
*
* Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
*
* Version		 Author 		Date		   Description
*
*	1.1 		 yuanxianfeng	2016/08/01	   AF
*
*****************************************************************************
*/

#include "mpi_isp.h"
#include "mpi_af.h"
#include "isp_tuning.h"
#include "isp.h"

#if 0 /* not open customer */
AW_S32 AW_MPI_ISP_AF_SetVcmCode(ISP_DEV IspDev, ISP_AF_VCM_CODE_S *pVcmCode)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_VCM_CODE, pVcmCode);
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_GetVcmCode(ISP_DEV IspDev, ISP_AF_VCM_CODE_S *pVcmCode)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_VCM_CODE, pVcmCode);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_SetOtp(ISP_DEV IspDev, ISP_AF_OTP_S *pOtp)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_OTP, pOtp);
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_GetOtp(ISP_DEV IspDev, ISP_AF_OTP_S *pOtp)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_OTP, pOtp);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_SetSpeed(ISP_DEV IspDev, ISP_AF_SPEED_S *pSpeed)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_SPEED, pSpeed);
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_GetSpeed(ISP_DEV IspDev, ISP_AF_SPEED_S *pSpeed)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_SPEED, pSpeed);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_SetFineSearch(ISP_DEV IspDev, ISP_AF_FINE_SEARCH_S *pFineSearch)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_FINE_SEARCH, pFineSearch);
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_GetFineSearch(ISP_DEV IspDev, ISP_AF_FINE_SEARCH_S *pFineSearch)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_FINE_SEARCH, pFineSearch);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_SetRefocus(ISP_DEV IspDev, ISP_AF_REFOCUS_S *pRefocus)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_REFOCUS, pRefocus);
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_GetRefocus(ISP_DEV IspDev, ISP_AF_REFOCUS_S *pRefocus)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_REFOCUS, pRefocus);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_SetTolerance(ISP_DEV IspDev, ISP_AF_TOLERANCE_S *pTolerance)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_TOLERANCE, pTolerance);
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_GetTolerance(ISP_DEV IspDev, ISP_AF_TOLERANCE_S *pTolerance)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_TOLERANCE, pTolerance);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_SetScene(ISP_DEV IspDev, ISP_AF_SCENE_S *pScene)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_SCENE, pScene);
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AF_GetScene(ISP_DEV IspDev, ISP_AF_SCENE_S *pScene)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AF_SCENE, pScene);
	return ret;
}

#endif
