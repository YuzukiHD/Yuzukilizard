
 /*
 ******************************************************************************
 *
 * MPI_AE.c
 *
 * Hawkview ISP - mpi_ae.c module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   1.1		  yuanxianfeng   2016/08/01		AE
 *
 *****************************************************************************
 */

#include "mpi_isp.h"
#include "mpi_ae.h"
#include "isp_tuning.h"
#include "isp.h"

#if 0 /* not open customer */
AW_S32 AW_MPI_ISP_AE_SetWeightAttr(ISP_DEV IspDev, ISP_AE_WEIGHT_S *pWeight)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_WIN_WEIGHT, pWeight);
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AE_GetWeightAttr(ISP_DEV IspDev, ISP_AE_WEIGHT_S *pWeight)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_WIN_WEIGHT, pWeight);
	return ret;
}

AW_S32 AW_MPI_ISP_AE_SetExposureAttr(ISP_DEV IspDev, const ISP_AE_ATTR_PUB_S *pstExpAttr)
{
	struct isp_ae_pub_cfg isp_ae_pub;
    memcpy(&isp_ae_pub, pstExpAttr, sizeof(struct isp_ae_pub_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_PUB , &isp_ae_pub);
    isp_update(IspDev);
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_AE_GetExposureAttr(ISP_DEV IspDev, ISP_AE_ATTR_PUB_S *pstExpAttr)
{
	struct isp_ae_pub_cfg isp_ae_pub;
	isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_PUB , &isp_ae_pub);
    memcpy(pstExpAttr, &isp_ae_pub, sizeof(struct isp_ae_pub_cfg));
  	return SUCCESS;
}

// mode = 0x01; // HW_ISP_CFG_AE_PREVIEW_TBL
// mode = 0x02; // HW_ISP_CFG_AE_CAPTURE_TBL
// mode = 0x03; // HW_ISP_CFG_AE_VIDEO_TBL
AW_S32 AW_MPI_ISP_AE_SetTableAttr(ISP_DEV IspDev, AW_S32 mode, ISP_AE_TABLE_S *pTable)
{
	AW_S32 ret = FAILURE;
	switch (mode)
	{
	case 0x01:
		ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_PREVIEW_TBL, pTable);
		break;
	case 0x02:
		ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_CAPTURE_TBL, pTable);
		break;
	case 0x03:
		ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_VIDEO_TBL, pTable);
		break;
	default :
		break;
	}
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AE_GetTableAttr(ISP_DEV IspDev, AW_S32 mode, ISP_AE_TABLE_S *pTable)
{
	AW_S32 ret = FAILURE;
	switch (mode)
	{
	case 0x01:
		ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_PREVIEW_TBL, pTable);
		break;
	case 0x02:
		ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_CAPTURE_TBL, pTable);
		break;
	case 0x03:
		ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_VIDEO_TBL, pTable);
		break;
	default :
		break;
	}
	return ret;
}

AW_S32 AW_MPI_ISP_AE_SetDelayAttr(ISP_DEV IspDev, ISP_AE_DELAY_S *pDelay)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_DELAY, pDelay);
    isp_update(IspDev);
	return ret;
}

AW_S32 AW_MPI_ISP_AE_GetDelayAttr(ISP_DEV IspDev, ISP_AE_DELAY_S *pDelay)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AE_DELAY, pDelay);
	return ret;
}
#endif
