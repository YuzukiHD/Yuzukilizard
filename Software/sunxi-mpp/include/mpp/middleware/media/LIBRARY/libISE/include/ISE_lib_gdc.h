/*
 * =====================================================================================
 *   Copyright (c)  Allwinner Technology Co. Ltd.
 *   All rights reserved.
 *
 *   Filename:  ISE_lib_gdc.h
 *   Description: 
 *   Version:  1.0
 *   Created:  08/08/2019 04:18:11 PM
 *   Author:  Gan Qiuye(ganqiuye@allwinnertech.com)
 * =====================================================================================
 */
#ifndef _ISE_LIB_GDC_H_
#define _ISE_LIB_GDC_H_

#include "IEGcommon.h"
#include "stdint.h"

#ifndef DISTORTORDER_FISHEYE
#define DISTORTORDER_FISHEYE       (5)
#endif

#ifndef DISTORTORDER_WIDEANGLE_RA
#define DISTORTORDER_WIDEANGLE_RA  (3)
#endif

#ifndef DISTORTORDER_WIDEANGLE_TA
#define DISTORTORDER_WIDEANGLE_TA  (2)
#endif

#ifndef DISTORTORDER_FISHEYE_K
#define DISTORTORDER_FISHEYE_K     (4)
#endif

#ifndef PI
#define PI  (3.1415926f)
#endif

#ifndef DEG2RAD
#define DEG2RAD(angle) ((angle)*PI/180.0f)
#endif

#ifndef RAD2DEG
#define RAD2DEG(angle) ((angle)*180.0f/PI)
#endif

//#ifndef DBL_EPSILON
//#define DBL_EPSILON    (2.2204460492503131e-16)
//#endif
//
//#ifndef FLT_EPSILON
//#define FLT_EPSILON    (1.192092896e-07F)
//#endif

#ifndef DIM_PROJECTIONMAT
#define DIM_PROJECTIONMAT    (9)
#endif

typedef enum _eMount_Type_{

	Mount_Top,

	Mount_Wall,

	Mount_Bottom
}eMount_Type;

typedef enum _eWarpType_ {

	Warp_LDC,       //0

	Warp_Pano180,   //1

	Warp_Pano360,   //2

	Warp_180with2,   //3

	Warp_Normal,    //4

	Warp_Fish2Wide, //5

	Warp_Perspective, //6

	Warp_BirdsEye, //7

	Warp_Ptz4In1, //8

	Warp_User //9
}eWarpType;

typedef enum _ePerspFunc_ {
	Persp_Only,
	Persp_LDC
}ePerspFunc;

typedef enum _eLensDistModel_ {
	DistModel_WideAngle,
	DistModel_FishEye
}eLensDistModel;

typedef struct VEC2D
{
	float x;
	float y;

}sVec2D;

typedef struct VEC3D
{
	float x;
	float y;
	float z;

}sVec3D;

typedef struct ROTATEPARA
{
	float roll;
	float pitch;
	float yaw;
}sRotPara;

typedef struct WARPPARA
{
	int radialDistortCoef;    //[-255,255]
	int fanDistortCoef;       //[-255,255]
	int trapezoidDistortCoef; //[-255,255]
	float lut_trapezoid[9];
}sWarpPara;

typedef struct LENSDISTORTIONPARA
{
	float fx;
	float fy;
	float cx;
	float cy;

	float fx_scale;
	float fy_scale;
	float cx_scale;
	float cy_scale;

	eLensDistModel distModel;
	float distCoef_wide_ra[DISTORTORDER_WIDEANGLE_RA];
	float distCoef_wide_ta[DISTORTORDER_WIDEANGLE_TA];
	float distCoef_fish_k[DISTORTORDER_FISHEYE_K];

	int zoomH;
	int zoomV;
	int centerOffsetX;
	int centerOffsetY;
	float rotateAngle; //[0,360]

	ePerspFunc perspFunc;
	float perspectiveProjMat[DIM_PROJECTIONMAT];
}sLensDistortPara;

typedef struct FISHEYELENSPARA
{
	int width;
	int height;
	float cx;
	float cy;
	float radius;
	float fov;
	float k[DISTORTORDER_FISHEYE];
	int k_order; //<=5
}sFishEyeLensPara;

typedef struct PTZPARA
{
	float pan;
	float tilt;
	int zoomH;
	int zoomV;
}sPtzPara;

typedef struct FISHEYEPARA
{
	sPtzPara ptz;
	int scale;
	int innerRadius;//[0,radius]
	float rotMat[9];
	eMount_Type mountMode;
	sRotPara rotPara;
	sFishEyeLensPara lensParams;
}sFishEyePara;

typedef struct BIRDSEYEVIEWPARA
{
	sRotPara rotPara;

	int offsetX;
	int offsetY;
	int zoomH;
	int zoomV;
	float mountHeight;

	int outWidth;
	int outHeight;

	float roiDist_ahead;
	float roiDist_left;
	float roiDist_right;
	float roiDist_bottom;

	float projMat_birds2src[DIM_PROJECTIONMAT];
	float projMat_src2birds[DIM_PROJECTIONMAT];
	float projMat_vehicle2birds[DIM_PROJECTIONMAT];
	float projMat_birds2vehicle[DIM_PROJECTIONMAT];

}sBirdsEyePara;

typedef struct RECTIFYPARA
{
	eWarpType warpMode;

	sWarpPara warpPara;
	sLensDistortPara LensDistPara;
	sFishEyePara fishEyePara;
	sBirdsEyePara birdsEyePara;
	sPtzPara ptz4In1Para[4];
}sRectifyPara;

//! 图像数据结构体
typedef struct UserImageHeader
{
    int32_t width;          // 图像长度
	int32_t height;         // 图像宽度
	//int32_t channel;

    uint8_t *pdata[3];       // 数组指针，分别指向Y、U、V存储数据地址
	int32_t stride[3];       // Y、U、V数据的存储宽度
	int32_t pixels_step[3];  // Y、U、V数据存储步长

	IMAGE_FORMAT img_format;
}IMGHeader;

//*the callback, you can complete it in your code for "warpMode == Warp_User"
//*the userData is defined by your own struct, can be NULL
typedef void (*cbRectifyPixel)(void* userData, sVec2D srcPos, sVec2D *dstPos, sRectifyPara rectPara);

typedef struct _ISE_CFG_PARA_GDC_{
	IMGHeader 		srcImage;
	IMGHeader 		birdsEyeImage;
	IMGHeader 		undistImage;
	sRectifyPara 	rectPara;
	char            reserved[32];
}ISE_CFG_PARA_GDC;

typedef struct _ISE_PROCIN_PARA_GDC_{
	unsigned int		in_luma_phy_Addr;
	void*				in_luma_mmu_Addr;
	unsigned int		in_chroma_phy_Addr;
	void*				in_chroma_mmu_Addr;
	char				reserved[32];
}ISE_PROCIN_PARA_GDC;

typedef struct _ISE_PROCOUT_PARA_GDC_{
	unsigned int		out_luma_phy_Addr[1+MAX_SCALAR_CHNL];
	void*				out_luma_mmu_Addr[1+MAX_SCALAR_CHNL];
	unsigned int		out_chroma_u_phy_Addr[1+MAX_SCALAR_CHNL];
	void*				out_chroma_u_mmu_Addr[1+MAX_SCALAR_CHNL];
	unsigned int		out_chroma_v_phy_Addr[1+MAX_SCALAR_CHNL];
	void*				out_chroma_v_mmu_Addr[1+MAX_SCALAR_CHNL];
	char				reserved[32];
}ISE_PROCOUT_PARA_GDC;

// 接口函数
int  ISE_Create_GDC(ISE_HANDLE_GDC *handle);

void ISE_SetFrq_GDC(ISE_HANDLE_GDC *handle, int freMHz);

int  ISE_SetCallBack_GDC(ISE_HANDLE_GDC *handle, cbRectifyPixel callback, void* userData);

int  ISE_SetAttr_GDC(ISE_HANDLE_GDC *handle, ISE_CFG_PARA_GDC gdc_cfg);

int ISE_Proc_GDC(
	ISE_HANDLE_GDC  *handle,
	ISE_PROCIN_PARA_GDC  *ise_procin,
	ISE_PROCOUT_PARA_GDC *ise_procout,
	eWarpType warpMode);

int ISE_Destroy_GDC(ISE_HANDLE_GDC *handle);

#endif
