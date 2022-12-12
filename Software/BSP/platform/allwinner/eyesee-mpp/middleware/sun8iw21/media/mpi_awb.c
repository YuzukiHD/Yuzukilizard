
/*
 ******************************************************************************
 *
 * MPI_AWB.c
 *
 * Hawkview ISP - mpi_awb.c module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   1.1		  yuanxianfeng   2016/08/01		AWB
 *
 *****************************************************************************
 */

#include "mpi_awb.h"
#include "isp.h"
#include "isp_tuning.h"
#include "mpi_isp.h"

#if 0 /* not open customer */
AW_S32 AW_MPI_ISP_AWB_SetSpeed(ISP_DEV IspDev, ISP_AWB_SPEED_S *pSpeed)
{
    AW_S32 ret = FAILURE;
    ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_PUB, pSpeed);
    isp_update(IspDev);
    return ret;
}

AW_S32 AW_MPI_ISP_AWB_GetSpeed(ISP_DEV IspDev, ISP_AWB_SPEED_S *pSpeed)
{
    AW_S32 ret = FAILURE;
    ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_PUB, pSpeed);
    return ret;
}

AW_S32 AW_MPI_ISP_AWB_SetTempRange(ISP_DEV IspDev, ISP_AWB_TEMP_RANGE_S *pTempRange)
{
    AW_S32 ret = FAILURE;
    ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_TEMP_RANGE, pTempRange);
    isp_update(IspDev);
    return ret;
}

AW_S32 AW_MPI_ISP_AWB_GetTempRange(ISP_DEV IspDev, ISP_AWB_TEMP_RANGE_S *pTempRange)
{
    AW_S32 ret = FAILURE;
    ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_TEMP_RANGE, pTempRange);
    return ret;
}

// mode = 0x01; // standard
// mode = 0x02; // user
// mode = 0x03; // skin
// mode = 0x04; // special
AW_S32 AW_MPI_ISP_AWB_SetLight(ISP_DEV IspDev, AW_S32 mode, ISP_AWB_TEMP_INFO_S *pTempInfo)
{
    AW_S32 ret = FAILURE;
    switch (mode) {
        case 0x01:
            ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_LIGHT_INFO, pTempInfo);
            break;
        case 0x02:
            ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_EXT_LIGHT_INFO, pTempInfo);
            break;
        case 0x03:
            ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_SKIN_INFO, pTempInfo);
            break;
        case 0x04:
            ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_SPECIAL_INFO, pTempInfo);
            break;
        default:
            break;
    }
    isp_update(IspDev);
    return ret;
}

AW_S32 AW_MPI_ISP_AWB_GetLight(ISP_DEV IspDev, AW_S32 mode, ISP_AWB_TEMP_INFO_S *pTempInfo)
{
    AW_S32 ret = FAILURE;
    switch (mode) {
        case 0x01:
            ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_LIGHT_INFO, pTempInfo);
            break;
        case 0x02:
            ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_EXT_LIGHT_INFO, pTempInfo);
            break;
        case 0x03:
            ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_SKIN_INFO, pTempInfo);
            break;
        case 0x04:
            ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_SPECIAL_INFO, pTempInfo);
            break;
        default:
            break;
    }
    return ret;
}

AW_S32 AW_MPI_ISP_AWB_SetFavor(ISP_DEV IspDev, ISP_AWB_FAVOR_S *pFavor)
{
    AW_S32 ret = FAILURE;
    ret = isp_set_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_FAVOR, pFavor);
    isp_update(IspDev);
    return ret;
}

AW_S32 AW_MPI_ISP_AWB_GetFavor(ISP_DEV IspDev, ISP_AWB_FAVOR_S *pFavor)
{
    AW_S32 ret = FAILURE;
    ret = isp_get_cfg(IspDev, HW_ISP_CFG_3A, HW_ISP_CFG_AWB_FAVOR, pFavor);
    return ret;
}

#endif
