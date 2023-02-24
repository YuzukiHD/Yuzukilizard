/*
 * =====================================================================================
 *   Copyright (c)  Allwinner Technology Co. Ltd.
 *   All rights reserved.
 *
 *   Filename:  ISE_lib_mo.h
 *   Description:
 *   Version:  1.0
 *   Created:  08/08/2019 04:18:11 PM
 *   Author:  Gan Qiuye(ganqiuye@allwinnertech.com)
 * =====================================================================================
 */
#ifndef _ISE_LIB_MO_H_
#define _ISE_LIB_MO_H_

#include "IEGcommon.h"

//! 矫正方式
typedef enum _WARP_Type_MO_{

	WARP_PANO180 = 0x0001,

	WARP_PANO360 = 0x0002,

	WARP_NORMAL = 0x0003,

	WARP_UNDISTORT = 0x0004,

	WARP_180WITH2 = 0x0005,

	WARP_PTZ4IN1 = 0x0006

}WARP_Type_MO;


//! 安装方式
typedef enum _MOUNT_Type_MO_{

	MOUNT_TOP = 0x0001,

	MOUNT_WALL = 0x0002,

	MOUNT_BOTTOM = 0x0003

}MOUNT_Type_MO;


/*****************************************************************************
* 基本参数配置参数
* dewarp_mode			矫正模式
* mount_mode			安装模式
* in_h                  原图分辨率h，4的倍数
* in_w                  原图分辨率w，4的倍数
* in_yuv_type           原图格式
* pan/tilt/zoom       	PTZ模式下p参数
* pan_sub				4路PTZ模式下p参数
* out_h            		矫正后图像分辨率h
* out_w           	    矫正后图像分辨率w
* out_flip/out_mirror   矫正后图像翻转/镜像
* in_luma_pitch         原图亮度pitch
* in_chroma_pitch       原图色度pitch
* out_luma_pitch        矫正后图像亮度pitch
* out_chroma_pitch      矫正后图像色度pitch
* p/cx/cy      			鱼眼镜头参数
* reserved          	保留字段，字节对齐
*******************************************************************************/
typedef struct _ISE_CFG_PARA_MO_{
	WARP_Type_MO			 dewarp_mode;
	MOUNT_Type_MO			 mount_mode;
	int                      in_h;
	int                      in_w;
	int						 in_luma_pitch;
	int						 in_chroma_pitch;
	int                      in_yuv_type;
	float					 pan;
	float					 tilt;
	float					 zoom;
	float					 pan_sub[4];
	float					 tilt_sub[4];
	float					 zoom_sub[4];
	int						 ptzsub_chg[4];
	int						 out_en[1+MAX_SCALAR_CHNL];
	int                      out_h[1+MAX_SCALAR_CHNL];
	int                      out_w[1+MAX_SCALAR_CHNL];
	int						 out_luma_pitch[1+MAX_SCALAR_CHNL];
	int						 out_chroma_pitch[1+MAX_SCALAR_CHNL];
	int						 out_flip[1+MAX_SCALAR_CHNL];
	int					     out_mirror[1+MAX_SCALAR_CHNL];
	int                      out_yuv_type;
	float					 p;
	float					 cx;
	float					 cy;
	float					 fx;
	float					 fy;
	float					 cxd;
	float					 cyd;
	float					 fxd;
	float					 fyd;
	float					 k[6];
	float					 p_undis[2];
	double					 calib_matr[3][3];
	double					 calib_matr_cv[3][3];
	double					 distort[8];
	char                     reserved[32];
}ISE_CFG_PARA_MO;

typedef struct _ISE_PROCIN_PARA_MO_{
	unsigned int		in_luma_phy_Addr;
	void*				in_luma_mmu_Addr;
	unsigned int		in_chroma_phy_Addr;
	void*				in_chroma_mmu_Addr;
	char				reserved[32];
}ISE_PROCIN_PARA_MO;

typedef struct _ISE_PROCOUT_PARA_MO_{
	unsigned int		out_luma_phy_Addr[1+MAX_SCALAR_CHNL];
	void*				out_luma_mmu_Addr[1+MAX_SCALAR_CHNL];
	unsigned int		out_chroma_u_phy_Addr[1+MAX_SCALAR_CHNL];
	void*				out_chroma_u_mmu_Addr[1+MAX_SCALAR_CHNL];
	unsigned int		out_chroma_v_phy_Addr[1+MAX_SCALAR_CHNL];
	void*				out_chroma_v_mmu_Addr[1+MAX_SCALAR_CHNL];
	char				reserved[32];
}ISE_PROCOUT_PARA_MO;

// 接口函数
int ISE_Create_Mo(ISE_CFG_PARA_MO *ise_cfg, ISE_HANDLE_MO *handle);

void ISE_SetFrq_Mo(int freMHz, ISE_HANDLE_MO *handle);

int ISE_SetAttr_Mo(ISE_HANDLE_MO *handle);

int ISE_Proc_Mo(
	ISE_HANDLE_MO 		*handle,
	ISE_PROCIN_PARA_MO 	*ise_procin, 
	ISE_PROCOUT_PARA_MO *ise_procout);

int ISE_Destroy_Mo(ISE_HANDLE_MO *handle);

#endif
