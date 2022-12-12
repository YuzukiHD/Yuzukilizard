/*
 * =====================================================================================
 *   Copyright (c)  Allwinner Technology Co. Ltd.
 *   All rights reserved.
 *
 *   Filename:  IEGcommon.h
 *   Description:
 *   Version:  1.0
 *   Created:  08/08/2019 04:18:11 PM
 *   Author:  Gan Qiuye(ganqiuye@allwinnertech.com)
 * =====================================================================================
 */
#ifndef _ISE_EIS_COMMON_H_
#define _ISE_EIS_COMMON_H_

#define MAX_SCALAR_CHNL         3        // scalar最多路数
// 当前版本号
#define ISE_EIS_MAJOR_VER       1         // 主版本号，最大63
#define ISE_EIS_SUB_VER         1         // 子版本号，最大31
#define ISE_EIS_REV_VER         1         // 修正版本号，最大31

// 版本日期
#define ISE_EIS_VER_YEAR        2020
#define ISE_EIS_VER_MONTH       06
#define ISE_EIS_VER_DAY         10

// mem tab
#define ISE_EIS_MTAB_NUM        2

// returning Flag
#define LIB_S_OK                    0x00
#define LIB_S_FAIL                  0x80000001  //  发生错误
#define LIB_E_PARA_NULL             0x80000002  //  传入指针为空
#define LIB_E_MEM_NULL              0x80000003  //
#define LIB_E_MEM_OVER              0x80000004  //  内存分配失败
#define LIB_E_PARA_KEY              0x80000005  //
#define LIB_E_PARA_VAL              0x80000006  //  传入参数错误
#define LIB_E_MEM_ALIGN             0x80000007
#define LIB_E_DATA_FORMAT           0x80000008
#define LIB_E_IRQ_TIME_OUT          0x80000009  //  硬件处理超时
#define LIB_E_LUT_HIGHT_TOO_SMALL   0x8000000A
#define LIB_E_LUT_OUT_OF_RANGE      0x8000000B
#define LIB_E_ROIP_OVER             0x8000000C  //  ROIP溢出
#define LIB_E_BUFFER_NOT_ENOUGH     0x8000000D  //缓存Buffer不足

#define EN_ERR_EFUSE_ERROR          25          //模块绑定未开启

typedef enum IMAGE_FORMAT
{
	PLANE_YUV420  = 0x0, // nv12 YYYYYYYY UVUV ==>yuv420sp
	PLANE_YVU420  = 0x01, //nv21 YYYYYYYY VUVU ==>yuv420sp
	PLANE_YUV420p = 0x02, //yv12
	PLANE_YUV422  = 0x03,
	PLANE_YVU422  = 0x04,
	PLANE_YUV422p = 0x05,
	PLANE_RGB     = 0x06,
	PLANE_RGBp    = 0x07
}IMAGE_FORMAT;

/**内存对齐方式**/
typedef enum _ISE_EIS_MEM_ALIGNMENT_
{
	ISE_EIS_MEM_ALIGN_4BYTE    = 4,
	ISE_EIS_MEM_ALIGN_8BYTE    = 8,
	ISE_EIS_MEM_ALIGN_16BYTE   = 16,
	ISE_EIS_MEM_ALIGN_32BYTE   = 32,
	ISE_EIS_MEM_ALIGN_64BYTE   = 64,
	ISE_EIS_MEM_ALIGN_128BYTE  = 128,
	ISE_EIS_MEM_ALIGN_256BYTE  = 256,
	ISE_EIS_MEM_ALIGN_512BYTE  = 512,
	ISE_EIS_MEM_ALIGN_1024BYTE = 1024
}ISE_EIS_MEM_ALIGNMENT;

/** 内存类型标志**/
typedef enum _ISE_EIS_MEM_ATTRS_
{
	ISE_EIS_MEM_SCRATCH,                                // 可复用
	ISE_EIS_MEM_PERSIST                                 // 不可复用
} ISE_EIS_MEM_ATTRS;

typedef enum _ISE_EIS_MEM_SPACE_
{
	ISE_EIS_MEM_EXTER_CACHED_DATA,                   // 外部可Cache存储区
    ISE_EIS_MEM_EXTER_UNCACHED_DATA,                 // 外部不可Cache存储区
    ISE_EIS_MEM_INTER_DATA,                          // 内部存储区
} ISE_EIS_MEM_SPACE;

// 获取参数类型
typedef enum _ISE_EIS_GET_CFG_TYPE_
{
	ISE_EIS_GET_CFG_SINGLE_PARAM    =    0x0001,        // 单个参数
	ISE_EIS_GET_CFG_VERSION         =    0x0002         // 版本信息
} ISE_EIS_GET_CFG_TYPE;


/*****************************************************************************
* 算法库单一配置参数结构体
* index                键值索引，填写 AVM_SINGLE_PARAM_KEY 定义类型
* val_int              整型参数，如果要设置的参数是浮点型，这个域不用填写
* val_float            浮点型参数，如果要设置的参数是整型，这个域不用填写
******************************************************************************/
typedef struct _ISE_EIS_SINGLE_PARAM_
{
	int                      index;
	int                      val_int;
	float                    val_float;
} ISE_EIS_SINGLE_PARAM;

typedef void* ISE_HANDLE_BI;
typedef void* ISE_HANDLE_MO;
typedef void* ISE_HANDLE_STI;
typedef void* ISE_HANDLE_GDC;
typedef void* EISE_HANDLE;

#endif
