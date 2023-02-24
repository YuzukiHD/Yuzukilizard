
/*
 * Hawkview ISP - isp_comm.h module
 * Copyright (c) 2018 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 * Version: 1.0
 * Author: zhaowei
 * Date: 2018/07/11
 * Description isp500 && isp520 register header file
 */

#ifndef __BSP__ISP__COMM__H
#define __BSP__ISP__COMM__H

#include "isp_type.h"
#include "../isp_version.h"

#define ISP_REG_TBL_LENGTH			33
#define ISP_REG1_TBL_LENGTH			17
#define ISP_REG_TBL_NODE_LENGTH			9


#if (ISP_VERSION == 600)
#define USE_ENCPP
#define ISP_GAMMA_TBL_LENGTH			(3*1024)


//ISP LOAD DRAM
#define ISP_LOAD_DRAM_SIZE			0x7700
#define ISP_LOAD_REG_SIZE			0x1000
#define ISP_FE_TABLE_SIZE			0x1800
#define ISP_BAYER_TABLE_SIZE			0x2600
#define ISP_RGB_TABLE_SIZE			0x1200
#define ISP_YUV_TABLE_SIZE			0x1700
#define ISP_DBG_TABLE_SIZE			0x0000

/*fe table*/
#define ISP_CH0_MSC_FE_MEM_OFS			0x0000
#define ISP_CH1_MSC_FE_MEM_OFS			0x0800
#define ISP_CH2_MSC_FE_MEM_OFS			0x1000

#define ISP_CH0_MSC_FE_TBL_SIZE			0x0800
#define ISP_CH1_MSC_FE_TBL_SIZE			0x0800
#define ISP_CH2_MSC_FE_TBL_SIZE			0x0800

/*bayer table*/
#define ISP_LSC_MEM_OFS				0x0000
#define ISP_MSC_MEM_OFS				0x0800
#define ISP_MSC_NR_MEM_OFS			0x1000
#define ISP_TDNF_DK_MEM_OFS			0x1400
#define ISP_PLTM_TM_MEM_OFS			0x1c00
#define ISP_PLTM_LM_MEM_OFS			0x2400

#define ISP_RSC_TBL_SIZE			0x0800
//#define ISP_MSC_TBL_SIZE			0x0800
#define ISP_MSC_NR_TBL_SIZE			0x03c8
#define ISP_D3D_DK_TBL_SIZE			0x0800
#define ISP_PLTM_TM_TBL_SIZE			0x0800
#define ISP_PLTM_LM_TBL_SIZE			0x0200
#define ISP_PLTM_MEM_SIZE (ISP_PLTM_TM_TBL_SIZE + ISP_PLTM_LM_TBL_SIZE)

/*rgbtable*/
#define ISP_DRC_MEM_OFS				0x0000
#define ISP_GAMMA_MEM_OFS			0x0200

#define ISP_RGB_DRC_TBL_SIZE			0x0200
#define ISP_GAMMA_TBL_SIZE			0x1000

/*yuv table*/
#define ISP_CEM_MEM_OFS				0x0000

#define ISP_CEM_TBL0_SIZE			0x0cc0
#define ISP_CEM_TBL1_SIZE			0x0a40
#define ISP_CEM_MEM_SIZE (ISP_CEM_TBL0_SIZE + ISP_CEM_TBL1_SIZE)

//ISP SAVE DRAM
#define ISP_SAVE_DRAM_SIZE			0x6a00
#define ISP_SAVE_REG_SIZE			0x0100
#define ISP_STAT_TOTAL_SIZE			0x6a00
#define ISP_STATISTIC_SIZE			0x6900
#define ISP_DBG_STAT_SIZE			0x0000

#define ISP_STAT_AE_MEM_SIZE			0x0360
#define ISP_STAT_RES_SIZE			0x0020
#define ISP_STAT_AF_IIR_ACC_SIZE		0x0c00
#define ISP_STAT_AF_FIR_ACC_SIZE		0x0c00
#define ISP_STAT_AF_IIR_CNT_SIZE		0x0c00
#define ISP_STAT_AF_FIR_CNT_SIZE		0x0c00
#define ISP_STAT_AF_HL_CNT_SIZE			0x0c00
#define ISP_STAT_AWB_RGB_MEM_SIZE		0x1800
#define ISP_STAT_AFS_MEM_SIZE			0x0200
#define ISP_STAT_HIST0_MEM_SIZE			0x0400
#define ISP_STAT_HIST1_MEM_SIZE			0x0400
#define ISP_STAT_PLTM_LUM_SIZE			0x0780

#define ISP_STAT_AE_MEM_OFS			0x0100
#define ISP_STAT_AF_MEM_OFS			0x0480
#define ISP_STAT_AWB_MEM_OFS			0x4080
#define ISP_STAT_AFS_MEM_OFS			0x5880
#define ISP_STAT_HIST_MEM_OFS			0x5a80
#define ISP_STAT_HIST0_MEM_OFS			0x5a80
#define ISP_STAT_HIST1_MEM_OFS			0x5e80
#define ISP_STAT_PLTM_LUM_MEM_OFS		0x6280

//ISP SAVE_LOAD DRAM
#define ISP_SAVE_LOAD_DRAM_SIZE			0x3400
#define ISP_SAVE_LOAD_REG_SIZE			0x0100
#define ISP_SAVE_LOAD_STATISTIC_SIZE		0x3300

#define ISP_SAVE_LOAD_PLTM_PLX_SIZE		0x3000
#define ISP_SAVE_LOAD_D3D_K_SIZE		0x300

#define ISP_SAVE_LOAD_PLTM_PLX_OFS		ISP_STAT_TOTAL_SIZE
#define ISP_SAVE_LOAD_D3D_K_OFS			(ISP_STAT_TOTAL_SIZE + 0x3000)

/*
 *  update table
 */
#define LENS_UPDATE			(1 << 8)
#define GAMMA_UPDATE			(1 << 9)
#define DRC_UPDATE			(1 << 10)
#define D3D_UPDATE			(1 << 13)
#define PLTM_UPDATE			(1 << 14)
#define CEM_UPDATE			(1 << 15)
#define MSC_UPDATE			(1 << 16)
#define FE0_MSC_UPDATE			(1 << 17)
#define NR_MSC_UPDATE			(1 << 18)
#define FE1_MSC_UPDATE			(1 << 19)
#define FE2_MSC_UPDATE			(1 << 20)

#endif

#define TABLE_UPDATE_ALL 0xffffffff

/*
 *  ISP Module enable
 */
#if (ISP_VERSION >= 600)
#define WDR_EN		(1 << 0)
#define WDR_SPLIT_EN	(1 << 1)
#define CH0_DG_EN	(1 << 8)
#define CH1_DG_EN	(1 << 9)
#define CH2_DG_EN	(1 << 10)
#define CH0_MSC_EN	(1 << 12)
#define CH1_MSC_EN	(1 << 13)
#define CH2_MSC_EN	(1 << 14)

#define WDR_STITCH_EN	(1 << 0)
#define DPC_EN		(1 << 1)
#define CTC_EN		(1 << 2)
#define GCA_EN		(1 << 3)
#define D2D_EN		(1 << 4)
#define D3D_EN		(1 << 5)
#define BLC_EN		(1 << 6)
#define WB_EN		(1 << 7)
#define DG_EN		(1 << 8)
#define LSC_EN		(1 << 9)
#define RSC_EN     	(1 << 9)
#define MSC_EN     	(1 << 10)
#define PLTM_EN		(1 << 11)
#define LCA_EN		(1 << 12)
#define SHARP_EN	(1 << 13)
#define CCM_EN		(1 << 14)
#define CNR_EN		(1 << 15)
#define RGB_DRC_EN	(1 << 16)
#define DRC_EN		(1 << 16)
#define BGC_EN		(1 << 17)
#define GAMMA_EN	(1 << 17)
#define CEM_EN		(1 << 18)
#define AE_EN		(1 << 24)
#define AF_EN		(1 << 25)
#define AWB_EN		(1 << 26)
#define AFS_EN		(1 << 27)
#define HIST0_EN	(1 << 28)
#define HIST1_EN	(1 << 29)

#define ISP_MODULE0_EN_ALL	(0xffffffff)
#define ISP_MODULE1_EN_ALL	(0xffffffff)

#endif
/* ISP module config */

/* TABLE */
#define ISP_LUT_TBL_SIZE        256
#define ISP_LENS_TBL_SIZE       256
#define ISP_MSC_TBL_SIZE        484
#define ISP_DRC_TBL_SIZE        256
#define ISP_TDNF_TBL_SIZE       256

#define ISP_MSC_TBL_LENGTH              (3*ISP_MSC_TBL_SIZE)
#define ISP_LSC_TBL_LENGTH              (3*ISP_LENS_TBL_SIZE)

#define ISP_MSC_TBL_LUT_DLT_SIZE        12
#define ISP_MSC_TBL_LUT_SIZE            11
#define ISP_AF_SQUARE_TBL_LUT_SIZE      16
#define ISP_WDR_TBL_SIZE                24

#define ISP_GAMMA_TRIGGER_POINTS        5

#define ISP_CCM_TEMP_NUM                3
#define ISP_LSC_TEMP_NUM                6
#define ISP_MSC_TEMP_NUM                6

#define GTM_LUM_IDX_NUM                 9
#define GTM_VAR_IDX_NUM                 9

#define PLTM_MAX_STAGE                  4
#define FE_CHN_NUM                      3

enum lensmode {
	FF_MODE = 1,
	AF_MODE = 2,
};

enum colorspace {
	BT601_FULL_RANGE = 0,
	BT709_FULL_RANGE,
	BT2020_FULL_RANGE,
	BT601_PART_RANGE,
	BT709_PART_RANGE,
	BT2020_PART_RANGE,
};

enum isp_raw_ch {
	ISP_RAW_CH_R =  0,
	ISP_RAW_CH_GR,
	ISP_RAW_CH_GB,
	ISP_RAW_CH_G,
	ISP_RAW_CH_MAX,
};

enum isp_sharp_cfg {
	ISP_SHARP_SS_NS_LW        = 0,//SS
	ISP_SHARP_SS_NS_HI        = 1,
	ISP_SHARP_SS_LW_COR       = 2,
	ISP_SHARP_SS_HI_COR       = 3,
	ISP_SHARP_SS_BLK_STREN    = 4,
	ISP_SHARP_SS_WHT_STREN    = 5,
	ISP_SHARP_SS_AVG_SMTH     = 6,
	ISP_SHARP_SS_DIR_SMTH     = 7,
	ISP_SHARP_LS_NS_LW        = 8,//LS
	ISP_SHARP_LS_NS_HI        = 9,
	ISP_SHARP_LS_LW_COR       = 10,
	ISP_SHARP_LS_HI_COR       = 11,
	ISP_SHARP_LS_BLK_STREN    = 12,
	ISP_SHARP_LS_WHT_STREN    = 13,
	ISP_SHARP_HFR_SMTH_RATIO  = 14,//HFR
	ISP_SHARP_HFR_HF_WHT_STREN = 15,
	ISP_SHARP_HFR_HF_BLK_STREN = 16,
	ISP_SHARP_HFR_MF_WHT_STREN = 17,
	ISP_SHARP_HFR_MF_BLK_STREN = 18,
	ISP_SHARP_HFR_RD_WHT_STREN = 19,
	ISP_SHARP_HFR_RD_BLK_STREN = 20,
	ISP_SHARP_HFR_HF_COR_RATIO = 21,
	ISP_SHARP_HFR_MF_COR_RATIO = 22,
	ISP_SHARP_HFR_RD_COR_RATIO = 23,
	ISP_SHARP_HFR_HF_MIX_RATIO = 24,
	ISP_SHARP_HFR_MF_MIX_RATIO = 25,
	ISP_SHARP_HFR_RD_MIX_RATIO = 26,
	ISP_SHARP_HFR_HF_MIX_MIN_RATIO = 27,
	ISP_SHARP_HFR_MF_MIX_MIN_RATIO = 28,
	ISP_SHARP_HFR_RD_MIX_MIN_RATIO = 29,
	ISP_SHARP_HFR_HF_WHT_CLP   = 30,
	ISP_SHARP_HFR_HF_BLK_CLP   = 31,
	ISP_SHARP_HFR_MF_WHT_CLP   = 32,
	ISP_SHARP_HFR_MF_BLK_CLP   = 33,
	ISP_SHARP_HFR_RD_WHT_CLP   = 34,
	ISP_SHARP_HFR_RD_BLK_CLP   = 35,
	ISP_SHARP_DIR_SMTH0       = 36,//COMM
	ISP_SHARP_DIR_SMTH1       = 37,
	ISP_SHARP_DIR_SMTH2       = 38,
	ISP_SHARP_DIR_SMTH3       = 39,
	ISP_SHARP_WHT_CLP_PARA     = 40,
	ISP_SHARP_BLK_CLP_PARA     = 41,
	ISP_SHARP_WHT_CLP_SLP      = 42,
	ISP_SHARP_BLK_CLP_SLP      = 43,
	ISP_SHARP_MAX_CLP_RATIO    = 44,
	ISP_SHARP_MAX,
};

enum isp_sharp_comm_cfg {
	ISP_SHARP_SS_SHP_RATIO    = 0,
	ISP_SHARP_SS_DIR_RATIO    = 1,
	ISP_SHARP_SS_CRC_STREN    = 2,
	ISP_SHARP_SS_CRC_MIN      = 3,
	ISP_SHARP_LS_SHP_RATIO    = 4,
	ISP_SHARP_LS_DIR_RATIO    = 5,
	ISP_SHARP_WHT_SAT_RATIO   = 6,
	ISP_SHARP_BLK_SAT_RATIO   = 7,
	ISP_SHARP_WHT_SLP_BT      = 8,
	ISP_SHARP_BLK_SLP_BT      = 9,
	ISP_SHARP_COMM_MAX,
};

enum enc_encpp_sharp_cfg {
	ENCPP_SHARP_SS_NS_LW        = 0,//SS
	ENCPP_SHARP_SS_NS_HI        = 1,
	ENCPP_SHARP_SS_LW_COR       = 2,
	ENCPP_SHARP_SS_HI_COR       = 3,
	ENCPP_SHARP_SS_BLK_STREN    = 4,
	ENCPP_SHARP_SS_WHT_STREN    = 5,
	ENCPP_SHARP_SS_AVG_SMTH     = 6,
	ENCPP_SHARP_SS_DIR_SMTH     = 7,
	ENCPP_SHARP_LS_NS_LW        = 8,//LS
	ENCPP_SHARP_LS_NS_HI        = 9,
	ENCPP_SHARP_LS_LW_COR       = 10,
	ENCPP_SHARP_LS_HI_COR       = 11,
	ENCPP_SHARP_LS_BLK_STREN    = 12,
	ENCPP_SHARP_LS_WHT_STREN    = 13,
	ENCPP_SHARP_HFR_SMTH_RATIO  = 14,//HFR
	ENCPP_SHARP_HFR_HF_WHT_STREN = 15,
	ENCPP_SHARP_HFR_HF_BLK_STREN = 16,
	ENCPP_SHARP_HFR_MF_WHT_STREN = 17,
	ENCPP_SHARP_HFR_MF_BLK_STREN = 18,
	ENCPP_SHARP_HFR_HF_COR_RATIO = 19,
	ENCPP_SHARP_HFR_MF_COR_RATIO = 20,
	ENCPP_SHARP_HFR_HF_MIX_RATIO = 21,
	ENCPP_SHARP_HFR_MF_MIX_RATIO = 22,
	ENCPP_SHARP_HFR_HF_MIX_MIN_RATIO = 23,
	ENCPP_SHARP_HFR_MF_MIX_MIN_RATIO = 24,
	ENCPP_SHARP_HFR_HF_WHT_CLP   = 25,
	ENCPP_SHARP_HFR_HF_BLK_CLP   = 26,
	ENCPP_SHARP_HFR_MF_WHT_CLP   = 27,
	ENCPP_SHARP_HFR_MF_BLK_CLP   = 28,
	ENCPP_SHARP_DIR_SMTH0       = 29,//COMM
	ENCPP_SHARP_DIR_SMTH1       = 30,
	ENCPP_SHARP_DIR_SMTH2       = 31,
	ENCPP_SHARP_DIR_SMTH3       = 32,
	ENCPP_SHARP_WHT_CLP_PARA     = 33,
	ENCPP_SHARP_BLK_CLP_PARA     = 34,
	ENCPP_SHARP_WHT_CLP_SLP      = 35,
	ENCPP_SHARP_BLK_CLP_SLP      = 36,
	ENCPP_SHARP_MAX_CLP_RATIO    = 37,
	ENCPP_SHARP_MAX,
};

enum enc_encpp_sharp_comm_cfg {
	ENCPP_SHARP_SS_SHP_RATIO    = 0,
	ENCPP_SHARP_SS_DIR_RATIO    = 1,
	ENCPP_SHARP_SS_CRC_STREN    = 2,
	ENCPP_SHARP_SS_CRC_MIN      = 3,
	ENCPP_SHARP_LS_SHP_RATIO    = 4,
	ENCPP_SHARP_LS_DIR_RATIO    = 5,
	ENCPP_SHARP_WHT_SAT_RATIO   = 6,
	ENCPP_SHARP_BLK_SAT_RATIO   = 7,
	ENCPP_SHARP_WHT_SLP_BT      = 8,
	ENCPP_SHARP_BLK_SLP_BT      = 9,
	ENCPP_SHARP_COMM_MAX,
};

enum encoder_denoise_cfg {
	ENCODER_DENOISE_3D_ADJ_PIX_LV_EN,//3DNR
	ENCODER_DENOISE_3D_SMT_FILT_EN,
	ENCODER_DENOISE_3D_MAX_PIX_DF_TH,
	ENCODER_DENOISE_3D_MAX_MAD_TH,
	ENCODER_DENOISE_3D_MAX_MV_TH,
	ENCODER_DENOISE_3D_MIN_COEF,
	ENCODER_DENOISE_3D_MAX_COEF,
	ENCODER_DENOISE_2D_FILT_STREN_UV,//2DNR
	ENCODER_DENOISE_2D_FILT_STREN_Y,
	ENCODER_DENOISE_2D_FILT_TH_UV,
	ENCODER_DENOISE_2D_FILT_TH_Y,
	ENCODER_DENOISE_MAX,
};

enum isp_denoise_cfg {
	ISP_DENOISE_BLACK_GAIN    = 0,//DNR
	ISP_DENOISE_BLACK_OFFSET  = 1,
	ISP_DENOISE_WHITE_GAIN    = 2,
	ISP_DENOISE_WHITE_OFFSET  = 3,
	ISP_DENOISE_LYR0_DNR_YRATIO = 4,
	ISP_DENOISE_LYR1_DNR_YRATIO = 5,
	ISP_DENOISE_LYR2_DNR_YRATIO = 6,
	ISP_DENOISE_LYR3_DNR_YRATIO = 7,
	ISP_DENOISE_HF_SMTH_RATIO = 8,//DTC
	ISP_DENOISE_DTC_HF_WHT_STR = 9,
	ISP_DENOISE_DTC_HF_BLK_STR = 10,
	ISP_DENOISE_DTC_HF_WHT_CLP = 11,
	ISP_DENOISE_DTC_HF_BLK_CLP = 12,
	ISP_DENOISE_DTC_HF_COR_RT = 13,
	ISP_DENOISE_DTC_MF_WHT_STR = 14,
	ISP_DENOISE_DTC_MF_BLK_STR = 15,
	ISP_DENOISE_DTC_MF_WHT_CLP = 16,
	ISP_DENOISE_DTC_MF_BLK_CLP = 17,
	ISP_DENOISE_DTC_MF_COR_RT = 18,
	ISP_DENOISE_LYR0_DNR_LM_AMP = 19,//WDR
	ISP_DENOISE_LYR1_DNR_LM_AMP = 20,
	ISP_DENOISE_LYR2_DNR_LM_AMP = 21,
	ISP_DENOISE_LYR3_DNR_LM_AMP = 22,
	ISP_DENOISE_LYR0_DNR_LMS_AMP = 23,
	ISP_DENOISE_LYR1_DNR_LMS_AMP = 24,
	ISP_DENOISE_LYR2_DNR_LMS_AMP = 25,
	ISP_DENOISE_LYR3_DNR_LMS_AMP = 26,
	ISP_DENOISE_MAX,
};

enum isp_denoise_comm_cfg {
	ISP_DENOISE_LYR0_CORE_RATIO = 0,
	ISP_DENOISE_LYR1_CORE_RATIO = 1,
	ISP_DENOISE_LYR2_CORE_RATIO = 2,
	ISP_DENOISE_LYR3_CORE_RATIO = 3,
	ISP_DENOISE_LYR0_SIDE_RATIO = 4,
	ISP_DENOISE_LYR1_SIDE_RATIO = 5,
	ISP_DENOISE_LYR2_SIDE_RATIO = 6,
	ISP_DENOISE_LYR3_SIDE_RATIO = 7,
	ISP_DENOISE_LYR0_DNR_CRATIO = 8,
	ISP_DENOISE_LYR1_DNR_CRATIO = 9,
	ISP_DENOISE_LYR2_DNR_CRATIO = 10,
	ISP_DENOISE_LYR3_DNR_CRATIO = 11,
	ISP_DENOISE_LYR0_GAUSS_TYPE = 12,
	ISP_DENOISE_LYR1_GAUSS_TYPE = 13,
	ISP_DENOISE_LYR2_GAUSS_TYPE = 14,
	ISP_DENOISE_LYR3_GAUSS_TYPE = 15,
	ISP_DENOISE_LYR0_GAUSS_WGTH = 16,
	ISP_DENOISE_LYR1_GAUSS_WGTH = 17,
	ISP_DENOISE_LYR2_GAUSS_WGTH = 18,
	ISP_DENOISE_LYR3_GAUSS_WGTH = 19,
	ISP_DENOISE_CNT_RATIO0 = 20,
	ISP_DENOISE_CNT_RATIO1 = 21,
	ISP_DENOISE_CNT_RATIO2 = 22,
	ISP_DENOISE_CNT_RATIO3 = 23,
	ISP_DENOISE_WDR_LM_HI_SLP = 24,
	ISP_DENOISE_WDR_MS_HI_SLP = 25,
	ISP_DENOISE_WDR_LM_LW_SLP = 26,
	ISP_DENOISE_WDR_MS_LW_SLP = 27,
	ISP_DENOISE_WDR_LM_MAX_CLP = 28,
	ISP_DENOISE_WDR_MS_MAX_CLP = 29,
	ISP_DENOISE_MSC_CMP_RATIO = 30,
	ISP_DENOISE_DTC_MG_MODE = 31,
	ISP_DENOISE_DTC_MG_CLIP = 32,
	ISP_DENOISE_COMM_MAX,
};

enum black_level {
	ISP_BLC_R_OFFSET = 0,
	ISP_BLC_GR_OFFSET = 1,
	ISP_BLC_GB_OFFSET = 2,
	ISP_BLC_B_OFFSET = 3,
	ISP_BLC_MAX,
};

enum dpc_cfg {
	ISP_DPC_HOT_RATIO = 0,
	ISP_DPC_COLD_RATIO = 1,
	ISP_DPC_NBHD_SMAD_RATIO = 2,
	ISP_DPC_NEAREST_SMAD_RATIO = 3,
	ISP_DPC_SLOPE_TH = 4,
	ISP_DPC_COLD_ABS_TH_COEFF = 5,
	ISP_DPC_SUP_TWINKLE_THR_H = 6,
	ISP_DPC_SUP_TWINKLE_THR_L = 7,
	ISP_DPC_MAX,
};
enum pltm_dynamic_cfg {
	ISP_PLTM_DYNAMIC_AUTO_STREN = 0,
	ISP_PLTM_DYNAMIC_MANUL_STREN = 1,
	ISP_PLTM_DYNAMIC_LUM_SCALE = 2,
	ISP_PLTM_DYNAMIC_LUM_RATIO = 3,
	ISP_PLTM_DYNAMIC_DCC_LW_RT = 4,
	ISP_PLTM_DYNAMIC_MIN_STREN_STEP = 5,
	ISP_PLTM_DYNAMIC_MAX_STREN_CLIP = 6,
	ISP_PLTM_DYNAMIC_SHP_SS_COMP = 7,
	ISP_PLTM_DYNAMIC_SHP_LS_COMP = 8,
	ISP_PLTM_DYNAMIC_D2D_COMP = 9,
	ISP_PLTM_DYNAMIC_D3D_COMP = 10,
	ISP_PLTM_DYNAMIC_MAX,
};


enum isp_tdf_cfg {
	ISP_TDF_BLACK_GAIN      = 0,//DNR
	ISP_TDF_WHITE_GAIN      = 1,
	ISP_TDF_FLT1_THR_GAIN   = 2,
	ISP_TDF_SS_MV_DNR       = 3,
	ISP_TDF_SS_STL_DNR      = 4,
	ISP_TDF_LS_MV_DNR       = 5,
	ISP_TDF_LS_STL_DNR      = 6,
	ISP_TDF_NR_LM_AMP       = 7,
	ISP_TDF_NR_LMS_AMP      = 8,
	ISP_TDF_DIFF_INTRA_SENS = 9,//MTD
	ISP_TDF_DIFF_INTER_SENS = 10,
	ISP_TDF_THR_INTRA_SENS  = 11,
	ISP_TDF_THR_INTER_SENS  = 12,
	ISP_TDF_STL_CYC_VAL     = 13,
	ISP_TDF_MOT_CYC_VAL     = 14,
	ISP_TDF_CYC_DEC_SLP     = 15,
	ISP_TDF_DTC_HF_COR      = 16,//DTC
	ISP_TDF_DTC_HF_BLK_CLP  = 17,
	ISP_TDF_DTC_HF_WHT_CLP  = 18,
	ISP_TDF_DTC_MF_COR      = 19,
	ISP_TDF_DTC_MF_BLK_CLP  = 20,
	ISP_TDF_DTC_MF_WHT_CLP  = 21,
	ISP_TDF_SS_MV_SHP       = 22,
	ISP_TDF_SS_STL_SHP      = 23,
	ISP_TDF_LS_MV_SHP       = 24,
	ISP_TDF_LS_STL_SHP      = 25,
	ISP_TDF_D2D0_CNR_STREN  = 26,//SRD
	ISP_TDF_D2D1_CNR_STREN  = 27,
	ISP_TDF_MV_SATU         = 28,
	ISP_TDF_MV_K_MIN        = 29,
	ISP_TDF_MV_K_RATIO      = 30,
	ISP_TDF_MV_R_MIN        = 31,
	ISP_TDF_MV_R_RATIO      = 32,
	ISP_TDF_DIFF_CV_CLP     = 33,
	ISP_TDF_NPU_FACE_NR     = 34,
	ISP_TDF_MAX,
};

enum isp_tdf_comm_cfg {
	ISP_TDF_DIFF_INTRA_AMP = 0,
	ISP_TDF_DIFF_INTER_AMP = 1,
	ISP_TDF_THR_INTRA_AMP  = 2,
	ISP_TDF_THR_INTER_AMP  = 3,
	ISP_TDF_DIFF_MIN_CLP   = 4,
	ISP_TDF_WDR_LM_HI_SLP  = 5,
	ISP_TDF_WDR_MS_HI_SLP  = 6,
	ISP_TDF_WDR_LM_LW_SLP  = 7,
	ISP_TDF_WDR_MS_LW_SLP  = 8,
	ISP_TDF_WDR_LM_MAX_CLP = 9,
	ISP_TDF_WDR_MS_MAX_CLP = 10,
	ISP_TDF_MSC_CMP_RATIO  = 11,
	ISP_TDF_STL_STG_CTH_0  = 12,
	ISP_TDF_STL_STG_CTH_1  = 13,
	ISP_TDF_STL_STG_CTH_2  = 14,
	ISP_TDF_STL_STG_CTH_3  = 15,
	ISP_TDF_STL_STG_CTH_4  = 16,
	ISP_TDF_STL_STG_CTH_5  = 17,
	ISP_TDF_STL_STG_CTH_6  = 18,
	ISP_TDF_STL_STG_CTH_7  = 19,
	ISP_TDF_STL_STG_KTH_0  = 20,
	ISP_TDF_STL_STG_KTH_1  = 21,
	ISP_TDF_STL_STG_KTH_2  = 22,
	ISP_TDF_STL_STG_KTH_3  = 23,
	ISP_TDF_STL_STG_KTH_4  = 24,
	ISP_TDF_STL_STG_KTH_5  = 25,
	ISP_TDF_STL_STG_KTH_6  = 26,
	ISP_TDF_STL_STG_KTH_7  = 27,
	ISP_TDF_MOT_STG_CTH_0  = 28,
	ISP_TDF_MOT_STG_CTH_1  = 29,
	ISP_TDF_MOT_STG_CTH_2  = 30,
	ISP_TDF_MOT_STG_CTH_3  = 31,
	ISP_TDF_MOT_STG_CTH_4  = 32,
	ISP_TDF_MOT_STG_CTH_5  = 33,
	ISP_TDF_MOT_STG_CTH_6  = 34,
	ISP_TDF_MOT_STG_CTH_7  = 35,
	ISP_TDF_MOT_STG_KTH_0  = 36,
	ISP_TDF_MOT_STG_KTH_1  = 37,
	ISP_TDF_MOT_STG_KTH_2  = 38,
	ISP_TDF_MOT_STG_KTH_3  = 39,
	ISP_TDF_MOT_STG_KTH_4  = 40,
	ISP_TDF_MOT_STG_KTH_5  = 41,
	ISP_TDF_MOT_STG_KTH_6  = 42,
	ISP_TDF_MOT_STG_KTH_7  = 43,
	ISP_TDF_FILT_OUT_SEL   = 44,
	ISP_TDF_COMM_MAX,
};

enum isp_ae_hist_cfg {
	ISP_AE_HIST_DARK_WEIGHT_MIN = 0,
	ISP_AE_HIST_DARK_WEIGHT_MAX = 1,
	ISP_AE_HIST_BRIGHT_WEIGHT_MIN = 2,
	ISP_AE_HIST_BRIGHT_WEIGHT_MAX = 3,
	ISP_AE_HIST_CFG_MAX,
};

enum isp_gtm_comm_cfg {
	ISP_GTM_GAIN = 0,
	ISP_GTM_EQ_RATIO = 1,
	ISP_GTM_EQ_SMOOTH = 2,
	ISP_GTM_BLACK = 3,
	ISP_GTM_WHITE = 4,
	ISP_GTM_BLACK_ALPHA = 5,
	ISP_GTM_WHITE_ALPHA = 6,
	ISP_GTM_GAMMA_IND = 7,

	ISP_GTM_GAMMA_PLUS = 8,
	ISP_GTM_HEQ_MAX,
};

enum isp_pltm_comm_cfg {
	ISP_PLTM_MODE           = 0,
	ISP_PLTM_SPEED          = 1,
	ISP_PLTM_TOLERANCE      = 2,
	ISP_PLTM_HDR_RATIO      = 3,
	ISP_PLTM_HDR_LOW        = 4,
	ISP_PLTM_HDR_HIGH       = 5,
	ISP_PLTM_GTM_EN         = 6,
	ISP_PLTM_LTF_EN         = 7,
	ISP_PLTM_DCC_EN         = 8,
	ISP_PLTM_DCC_SHF_BIT    = 9,
	ISP_PLTM_DCC_LW_TH      = 10,
	ISP_PLTM_DCC_HI_TH      = 11,
	ISP_PLTM_DSC_EN         = 12,
	ISP_PLTM_DSC_SHF_BIT    = 13,
	ISP_PLTM_DSC_LW_TH      = 14,
	ISP_PLTM_DSC_HI_TH      = 15,
	ISP_PLTM_DSC_LW_RT      = 16,
	ISP_PLTM_MGC_INT_SMTH0  = 17,
	ISP_PLTM_MGC_INT_SMTH1  = 18,
	ISP_PLTM_MGC_INT_SMTH2  = 19,
	ISP_PLTM_MGC_INT_SMTH3  = 20,
	ISP_PLTM_MGC_LUM_ADPT0  = 21,
	ISP_PLTM_MGC_LUM_ADPT1  = 22,
	ISP_PLTM_MGC_LUM_ADPT2  = 23,
	ISP_PLTM_MGC_LUM_ADPT3  = 24,
	ISP_PLTM_STS_CEIL_SLP0  = 25,
	ISP_PLTM_STS_CEIL_SLP1  = 26,
	ISP_PLTM_STS_CEIL_SLP2  = 27,
	ISP_PLTM_STS_CEIL_SLP3  = 28,
	ISP_PLTM_STS_FLOOR_SLP0 = 29,
	ISP_PLTM_STS_FLOOR_SLP1 = 30,
	ISP_PLTM_STS_FLOOR_SLP2 = 31,
	ISP_PLTM_STS_FLOOR_SLP3 = 32,
	ISP_PLTM_STS_GD_RT0     = 33,
	ISP_PLTM_STS_GD_RT1     = 34,
	ISP_PLTM_STS_GD_RT2     = 35,
	ISP_PLTM_STS_GD_RT3     = 36,
	ISP_PLTM_ADJK_CRCT_RT0  = 37,
	ISP_PLTM_ADJK_CRCT_RT1  = 38,
	ISP_PLTM_ADJK_CRCT_RT2  = 39,
	ISP_PLTM_ADJK_CRCT_RT3  = 40,
	ISP_PLTM_INTERVAL       = 41,
	ISP_PLTM_DARK_LOW_TH    = 42,
	ISP_PLTM_DARK_HIGH_TH   = 43,
	ISP_PLTM_MAX,
};

enum isp_gca_cfg {
	ISP_GCA_CT_W = 0,
	ISP_GCA_CT_H = 1,
	ISP_GCA_R_PARA0 = 2,
	ISP_GCA_R_PARA1 = 3,
	ISP_GCA_R_PARA2 = 4,
	ISP_GCA_B_PARA0 = 5,
	ISP_GCA_B_PARA1 = 6,
	ISP_GCA_B_PARA2 = 7,
	ISP_GCA_INT_CNS = 8,
	ISP_GCA_MAX,
};

enum isp_cem_cfg {
	ISP_CEM_RATIO = 0,
	ISP_CEM_DARK_SATU = 1,
	ISP_CEM_LOW_LI_SATU = 2,
	ISP_CEM_MID_LI_SATU = 3,
	ISP_CEM_HIGH_LI_SATU = 4,
	ISP_CEM_MAX,
};

enum isp_lca_cfg {
	ISP_LCA_GF_COR_RATIO = 0,
	ISP_LCA_PF_COR_RATIO = 1,

	ISP_LCA_LUM_TH = 2,
	ISP_LCA_GRAD_TH = 3,

	ISP_LCA_CLRS_GTH = 4,
	ISP_LCA_PF_RSHF = 5,
	ISP_LCA_PF_BSLP = 6,

	ISP_LCA_CLRS_LUM_TH = 7,
	ISP_LCA_PF_CLRC_RATIO = 8,
	ISP_LCA_GF_CLRC_RATIO = 9,
	ISP_LCA_PF_DECR_RATIO = 10,
	ISP_LCA_MAX,
};

enum isp_cfa_cfg {
	ISP_CFA_AFC_CNR_STREN = 0,
	ISP_CFA_AFC_RGB_STREN = 1,
	ISP_CFA_AFC_BAYER_STREN = 2,
	ISP_CFA_MAX,
};

enum isp_platform {
	ISP_PLATFORM_SUN8IW12P1,
	ISP_PLATFORM_SUN8IW16P1,
	ISP_PLATFORM_SUN8IW21P1,

	ISP_PLATFORM_NUM,
};

#if (ISP_VERSION >= 520)
struct isp_wdr_mode_cfg {
	unsigned char wdr_ch_seq;
	unsigned char wdr_exp_seq;
	unsigned char wdr_mode;
};
#endif

struct isp_size {
	HW_U32 width;
	HW_U32 height;
};

enum enable_flag {
	DISABLE    = 0,
	ENABLE     = 1,
};

enum wdr_mode {
	WDR_2FDOL = 0,
	WDR_2FCMD = 1,
	WDR_3FDOL = 2,
	WDR_3FCMD = 3,
};

enum channl_data_mode {
	CHN_DISABLE = 0,
	CHN_LINEAR,

	CHN_WDR_ORIGINAL_LM,
	CHN_WDR_ORIGINAL_L,
	CHN_WDR_ORIGINAL_M,
	CHN_WDR_ORIGINAL_S,
	CHN_WDR_ORIGINAL_MS,

	CHN_WDR_SPLIT_LM,
	CHN_WDR_SPLIT_L,
	CHN_WDR_SPLIT_M,
	CHN_WDR_SPLIT_S,
	CHN_WDR_SPLIT_MS,
	CHN_WDR_LMS,
};

enum isp_wdr_stitch_mode {
	ISP_WDR_STITCH_LINEAR = 0,
	ISP_WDR_STITCH_2F_LM,
	ISP_WDR_STITCH_3F_LMS,
};

enum attribute_of_channl_t {
	LINEAR_FROM_ONE_CHN = 0,
	WDR_2F_FROM_ONE_CHN = 4,
	WDR_2F_FROM_TWO_CHN = 5,
	WDR_3F_FROM_ONE_CHN = 8,
	WDR_3F_FROM_TWO_CHN = 9,
	WDR_3F_FROM_THREE_CHN = 10,
};

enum attribute_of_fe_channl_t {
	DATA_FROM_TDMTX0 = 0,
	DATA_FROM_TDMTX1 = 1,
	DATA_FROM_TDMTX2 = 2,
	DATA_FROM_RESERVED = 3,
	DATA_FROM_SPLITL = 4,
	DATA_FROM_SPLITM = 5,
	DATA_FROM_SPLITS = 6,
};

enum split_output_mode_t {
	SPLITO_LINEAR = 0,
	SPLITO_LM_TO_L_M = 4,
	SPLITO_LM_TO_LM_M = 5,
	SPLITO_MS_TO_M_MS = 6,
	SPLITO_LMS_TO_L_M_S = 8,
	SPLITO_LMS_TO_LM_M_S = 9,
	SPLITO_LMS_TO_L_M_MS = 10,
};

enum split_input_mode_t {
	SPLITI_LINEAR = 0,
	SPLITI_SENSOR_BUILD_IN = 1,
	SPLITI_16LOG = 2,
};

#if (ISP_VERSION >= 600)
enum isp_dmsc_mode {
	DMSC_NORMAL = 0,
	DMSC_BW = 1,
};

enum isp_gain_src {
	GAIN_BEFORE_DN = 0,
	GAIN_AFTER_DN = 1,
};

enum isp_wb_src {
	WB_BEFORE_DN = 0,
	WB_AFTER_DN = 1,
};

enum isp_hist_src {
	HIST_ST_MSC = 0,
	HIST_ST_CNR = 1,
	HIST_ST_DRC = 2,
	HIST_ST_CSC = 3,
};

enum isp_af_src {
	AF_BEFORE_D2D = 0,
	AF_AFTER_D2D = 1,
};

enum isp_awb_src {
	AWB_ST_MSC = 0,
	AWB_ST_PLTM = 1,
};

enum isp_ae_src {
	AE_ST_FE_MSC = 0,
	AE_FE_MSC = 1,
	AE_ST_CNR = 2,
	AE_ST_DRC = 3,
	AE_ST_CSC = 4,
};

enum isp_ae_stat_mode {
	AE_STAT_1CH = 0,
	AE_STAT_2CH = 1,
	AE_STAT_3CH = 2,
	AE_STAT_4CH = 3,
};

enum isp_proc_mode { /*wdr stitch input*/
	PROC_LINEAR = 0,
	PROC_LM_FOR_L_M = 4,
	PROC_LMS_FOR_L_M_S = 8,
	PROC_LMS_FOR_LM_M_S = 9,
	PROC_LMS_FOR_L_M_MS = 10,
};
#endif

enum isp_input_seq {
	ISP_BGGR = 4,
	ISP_RGGB = 5,
	ISP_GBRG = 6,
	ISP_GRBG = 7,
};

enum JUDGE_COMP_CFG {
	STATUS_STATIC = 0,
	STATUS_SUS_STATIC = 1,
	STATUS_SUS_MOTION = 2,
	STATUS_MOTION = 3,
	STATUS_JUDGE_MAX,
};

struct isp_ctc_config {
	HW_U16 ctc_th_max;
	HW_U16 ctc_th_min;
	HW_U16 ctc_th_slope;
	HW_U16 ctc_dir_wt;
	HW_U16 ctc_dir_th;
};

struct video_input_config {
	HW_U8 input_cfg;
	HW_U8 output_cfg;
	HW_U8 output_ch0_data;
	HW_U8 output_ch1_data;
	HW_U8 output_ch2_data;
};

struct isp_wdr_split_config {
	HW_U8 blc_en;
	HW_U8 inv_blc_en;
	HW_U8 ch_sel;
	HW_U8 input_mode;
	HW_U8 decomp_input_bit;
	HW_U8 output_mode;
	HW_U8 bitexp_output_bit;
	HW_U8 input_nrml;
	HW_U16 nrml_ratio[2];
	HW_U16 decomp_range[5];
	HW_U16 decomp_slope[5];
	HW_U16 decomp_offset[4];
};

struct isp_offset {
	HW_S16 r_offset;
	HW_S16 gr_offset;
	HW_S16 gb_offset;
	HW_S16 b_offset;
};

struct isp_dg_gain {
	HW_U16 r_gain;
	HW_U16 gr_gain;
	HW_U16 gb_gain;
	HW_U16 b_gain;
};

struct isp_front_end_config {
	HW_U8 chn_data_mode;
	HW_U8 blc_en;
	HW_U8 inv_blc_en;
	HW_U8 input_bit;
	HW_U8 output_bit;
	struct isp_offset offset;
	struct isp_dg_gain gain;
};

struct isp_wdr_config {
	HW_U8 wdr_stitch_mode;
	HW_U8 wdr_lexp_blc_en;
	HW_U8 wdr_wb_l_en;
	HW_U8 wdr_wb_m_en;
	HW_U8 wdr_wb_s_en;
	HW_U8 wdr_sexp_chk_lm_en;
	HW_U8 wdr_sexp_chk_ms_en;
	HW_U8 wdr_lecc_lm_en;
	HW_U8 wdr_lecc_ms_en;
	HW_U8 wdr_de_purpl_lm_en;
	HW_U8 wdr_de_purpl_ms_en;
	HW_U8 wdr_mv_chk_lm_en;
	HW_U8 wdr_mv_chk_ms_en;
	HW_U8 wdr_wb_l_mode;
	HW_U8 wdr_wb_m_mode;
	HW_U8 wdr_wb_s_mode;
	HW_U8 wdr_out_sel;
	HW_U8 wdr_lwb_nratio;
	HW_U32 wdr_lms_exp_ratio;

	HW_U16 lm_cmp_slp;
	HW_U16 lm_exp_ratio;
	HW_U16 lm_cmp_lth;
	HW_U16 lm_cmp_hth;
	HW_U16 lm_sexp_chk_slp;
	HW_U16 lm_sexp_chk_lth;
	HW_U16 lm_lecc_jdg_slp;
	HW_U16 lm_lecc_jdg_lth;
	HW_U16 lm_lecc_crc_rth;
	HW_U16 lm_lecc_crc_lth;
	HW_U16 lm_de_purpl_lrt;
	HW_U16 lm_de_purpl_lth;
	HW_U16 lm_mv_chk_slp;
	HW_U16 lm_mv_chk_lth;
	HW_U16 lm_mv_fcl_slp;
	HW_U16 lm_mv_fcl_lth;

	HW_U16 ms_cmp_slp;
	HW_U16 ms_exp_ratio;
	HW_U16 ms_cmp_lth;
	HW_U16 ms_cmp_hth;
	HW_U16 ms_sexp_chk_slp;
	HW_U16 ms_sexp_chk_lth;
	HW_U16 ms_lecc_jdg_slp;
	HW_U16 ms_lecc_jdg_lth;
	HW_U16 ms_lecc_crc_rth;
	HW_U16 ms_lecc_crc_lth;
	HW_U16 ms_de_purpl_lrt;
	HW_U16 ms_de_purpl_lth;
	HW_U16 ms_mv_chk_slp;
	HW_U16 ms_mv_chk_lth;
	HW_U16 ms_mv_fcl_slp;
	HW_U16 ms_mv_fcl_lth;

	HW_U8 wdr_de_purpl_hsv_tbl[ISP_WDR_TBL_SIZE];
};

struct isp_dpc_config {
	HW_U8 hot_ratio;
	HW_U8 cold_ratio;
	HW_U8 nbhd_smad_ratio;
	HW_U8 nearest_smad_ratio;
	HW_U8 slope_th;
	HW_U16 cold_abs_th_coeff;
	HW_U8 sup_twinkle_thr_h;
	HW_S8 sup_twinkle_thr_l;
	HW_U16 slope_prec_coeff;
	HW_U8 remove_mode;
	HW_U8 sup_twinkle_en;
};

struct isp_nrp_config {
	HW_U8 blc_en;
	HW_U8 gamma_en;
	HW_U8 inv_blc_en;
	HW_U8 inv_gamma_en;
	HW_U8 gm_neg_ratio;
};

struct isp_nr_config {
	HW_U8 d2d_msc_cmp_en;
	HW_U8 d2d_msc_cmp_ratio;
	HW_U8 d3d_msc_cmp_en;
	HW_U8 d3d_msc_cmp_ratio;
	HW_U8 msc_blw;
	HW_U8 msc_blh;
	HW_U16 msc_blw_dlt;
	HW_U16 msc_blh_dlt;

	HW_U8 wdr_nrml;
	HW_U8 wdr_blc;
	HW_U16 nrml_ratio[2];
	HW_U16 wdr_lm_cmp_hth;
	HW_U16 wdr_lm_cmp_lth;
	HW_U16 wdr_ms_cmp_hth;
	HW_U16 wdr_ms_cmp_lth;
	HW_U16 wdr_lm_cmp_slp;
	HW_U16 wdr_ms_cmp_slp;
};

struct isp_gca_config {
	HW_U16 gca_ct_h;
	HW_U16 gca_ct_w;
	HW_U8 gca_r_para0;
	HW_S16 gca_r_para1;
	HW_S16 gca_r_para2;
	HW_U8 gca_b_para0;
	HW_S16 gca_b_para1;
	HW_S16 gca_b_para2;
	HW_U8 gca_int_cns;
};

struct isp_lca_config {
	HW_U16 lca_gf_cor_ratio;
	HW_U16 lca_pf_cor_ratio;
	HW_U16 lca_lum_th;
	HW_U16 lca_grad_th;
	HW_U16 lca_clr_gth;
	HW_U8 lca_pf_rshf;
	HW_U16 lca_pf_bslp;
	HW_U8 lca_clrs_lum_th;
	HW_U8 lca_pf_clrc_ratio;
	HW_U8 lca_gf_clrc_ratio;
	HW_U8 lca_pf_decr_ratio;
	HW_U8 lca_pf_satu_lut[ISP_REG_TBL_LENGTH];
	HW_U8 lca_gf_satu_lut[ISP_REG_TBL_LENGTH];
};

struct isp_cnr_config {
	HW_U16 c_threshold;
	HW_U16 y_threshold;
	HW_U16 st_v_yth;
	HW_U16 st_h_yth;
};

struct isp_d2d_config {
	HW_U8 lyr_core_ratio[4];
	HW_U8 lyr_side_ratio[4];
	HW_U16 lyr_dnr_yratio[4];
	HW_U16 lyr_dnr_cratio[4];
	HW_U8 lyr_gauss_type[4];
	HW_U8 lyr_gauss_wght[4];
	HW_U16 lyr_dnr_lm_amp[4];
	HW_U16 lyr_dnr_lms_amp[4];
	HW_U8 hf_smth_ratio;
	HW_U16 dtc_hf_wht_str;
	HW_U16 dtc_hf_blk_str;
	HW_U16 dtc_hf_wht_clp;
	HW_U16 dtc_hf_blk_clp;
	HW_U8 dtc_hf_cor_rt;
	HW_U16 dtc_mf_wht_str;
	HW_U16 dtc_mf_blk_str;
	HW_U16 dtc_mf_wht_clp;
	HW_U16 dtc_mf_blk_clp;
	HW_U8 dtc_mf_cor_rt;
	HW_U8 dtc_mg_mode;
	HW_U8 dtc_mg_clip;
	HW_U8 cnt_ratio[4];
	HW_U8 wdr_lm_hi_slp;
	HW_U8 wdr_ms_hi_slp;
	HW_U8 wdr_lm_lw_slp;
	HW_U8 wdr_ms_lw_slp;
	HW_U16 wdr_lm_max_clp;
	HW_U16 wdr_ms_max_clp;

	HW_U16 d2d_lp0_th[ISP_REG_TBL_LENGTH];
	HW_U16 d2d_lp1_th[ISP_REG_TBL_LENGTH];
	HW_U16 d2d_lp2_th[ISP_REG_TBL_LENGTH];
	HW_U16 d2d_lp3_th[ISP_REG_TBL_LENGTH];
	HW_U16 d2d_lp_cth[ISP_REG_TBL_LENGTH];
};

struct isp_tdnf_config {
	HW_U8 rec_en;
	HW_U8 rnd_en;
	HW_U8 flt_out_sel;
	HW_U8 bypass_mode;
	HW_U16 kb_wnum;
	HW_U16 kb_hnum;
	HW_U8 kst_hlen;
	HW_U8 kst_wlen;
	HW_U16 kst_pnum;
	HW_U16 kst_wphs;
	HW_U16 kst_wstp;
	HW_U16 kst_hphs;
	HW_U16 kst_hstp;

	HW_U8 diff_intra_sens;
	HW_U8 diff_inter_sens;
	HW_U8 diff_intra_amp;
	HW_U8 diff_inter_amp;
	HW_U8 diff_min_clp;
	HW_U8 thr_intra_sens;
	HW_U8 thr_inter_sens;
	HW_U8 thr_intra_amp;
	HW_U8 thr_inter_amp;
	HW_U16 flt1_thr_gain;
	HW_U8 mv_k_min;
	HW_U8 mv_k_ratio;
	HW_U8 mv_r_min;
	HW_U8 mv_r_ratio;
	HW_U8 dtc_hf_cor;
	HW_U16 dtc_hf_blk_clp;
	HW_U16 dtc_hf_wht_clp;
	HW_U8 dtc_mf_cor;
	HW_U16 dtc_mf_blk_clp;
	HW_U16 dtc_mf_wht_clp;
	HW_U8 stl_cyc_val;
	HW_U8 mot_cyc_val;
	HW_U8 cyc_dec_slp;
	HW_U8 stl_stg_kth[8];
	HW_U8 stl_stg_cth[8];
	HW_U8 mot_stg_kth[8];
	HW_U8 mot_stg_cth[8];
	HW_U16 nr_lm_amp;
	HW_U16 nr_lms_amp;
	HW_U8 wdr_lm_hi_slp;
	HW_U8 wdr_ms_hi_slp;
	HW_U8 wdr_lm_lw_slp;
	HW_U8 wdr_ms_lw_slp;
	HW_U16 wdr_lm_max_clp;
	HW_U16 wdr_ms_max_clp;

	HW_U8 d2d0_cnr_bk[ISP_REG1_TBL_LENGTH];
	HW_U8 d2d1_cnr_bk[ISP_REG1_TBL_LENGTH];
	HW_U8 sat_ctrl_bk[ISP_REG1_TBL_LENGTH];
	HW_U8 ct_rt_bk[ISP_REG1_TBL_LENGTH];
	HW_U16 flt0_thr_vc[ISP_REG_TBL_LENGTH];
	HW_U8 k_mg_bk[ISP_REG1_TBL_LENGTH];
	HW_U8 r_mg_bk[ISP_REG1_TBL_LENGTH];
	HW_U8 df_shp_bk[ISP_REG1_TBL_LENGTH];
	HW_U8 r_amp_bk[ISP_REG1_TBL_LENGTH];
	HW_U8 k_dlt_bk[ISP_REG1_TBL_LENGTH];
	HW_U16 dtc_hf_bk[ISP_REG1_TBL_LENGTH];
	HW_U16 dtc_mf_bk[ISP_REG1_TBL_LENGTH];
	HW_U8 lay0_d2d0_rt_br[ISP_REG1_TBL_LENGTH];
	HW_U8 lay1_d2d0_rt_br[ISP_REG1_TBL_LENGTH];
	HW_U8 lay0_nrd_rt_br[ISP_REG1_TBL_LENGTH];
	HW_U8 lay1_nrd_rt_br[ISP_REG1_TBL_LENGTH];
};

struct isp_contrast_config {
	HW_U16 contrast_min_val;
	HW_U16 contrast_max_val;
	HW_U16 black_clip;
	HW_U16 white_clip;
	HW_U16 black_level;
	HW_U16 white_level;
	HW_U16 plat_th;
	HW_U16 contrast_val[ISP_REG_TBL_LENGTH];
	HW_U16 contrast_lum[ISP_REG_TBL_LENGTH];
};

struct isp_pltm_config {
	HW_U8 dsc_en;
	HW_U8 dcc_en;
	HW_U8 lft_en;
	HW_U8 gtm_en;
	HW_U8 cal_stg_en[PLTM_MAX_STAGE];
	HW_U8 lum_ratio;
	HW_U8 lum_scale;
	HW_U16 nrm_ratio;
	HW_U16 strength;
	HW_U8 max_stage;

	HW_U8 blk_width;
	HW_U8 blk_height;
	HW_U16 blk_num;
	HW_U16 wi_step;
	HW_U16 wi_phase;
	HW_U16 hi_step;
	HW_U16 hi_phase;

	HW_U16 sts_flt_stren[PLTM_MAX_STAGE][3];
	HW_U16 sts_ceil_slp[PLTM_MAX_STAGE];
	HW_U16 sts_floor_slp[PLTM_MAX_STAGE];
	HW_U8 sts_gd_ratio[PLTM_MAX_STAGE];
	HW_U8 mgc_int_smth[PLTM_MAX_STAGE];
	HW_U8 mgc_lum_adpt[PLTM_MAX_STAGE];
	HW_U8 adj1_asym_ratio[PLTM_MAX_STAGE];
	HW_U8 adjk_crct_ratio[PLTM_MAX_STAGE];

	HW_U16 dsc_lw_th;
	HW_U16 dsc_hi_th;
	HW_U8 dsc_shf_bit;
	HW_U8 dsc_lw_ratio;
	HW_U16 dsc_slp;
	HW_U16 dcc_lw_th;
	HW_U16 dcc_hi_th;
	HW_U8 dcc_shf_bit;
	HW_U8 dcc_lw_ratio;
	HW_U16 dcc_slp;

	HW_U8 stat_gd_cv[PLTM_MAX_STAGE*15];
	HW_U8 adj_k_df_cv[PLTM_MAX_STAGE*ISP_REG_TBL_LENGTH];

 	HW_U8 pltm_table[ISP_PLTM_MEM_SIZE];
};

struct isp_cfa_config {
	HW_U8 cfa_grad_th;
	HW_U16 cfa_dir_v_th;
	HW_U16 cfa_dir_h_th;
	HW_U8 res_smth_high;
	HW_U8 res_smth_low;
	HW_U8 afc_cnr_stren;
	HW_U8 afc_rgb_stren;
	HW_U8 afc_bayer_stren;
	HW_U16 res_high_th;
	HW_U16 res_low_th;
	HW_U8 res_dir_a;
	HW_U8 res_dir_d;
	HW_U8 res_dir_v;
	HW_U8 res_dir_h;
};

struct isp_sharp_config {
	HW_U16 ss_ns_lw;
	HW_U16 ss_ns_hi;
	HW_U16 ls_ns_lw;
	HW_U16 ls_ns_hi;
	HW_U8 ss_lw_cor;
	HW_U8 ss_hi_cor;
	HW_U8 ls_lw_cor;
	HW_U8 ls_hi_cor;
	HW_U8 ss_shp_ratio;
	HW_U8 ls_shp_ratio;
	HW_U16 ss_blk_stren;
	HW_U16 ss_wht_stren;
	HW_U16 ls_blk_stren;
	HW_U16 ls_wht_stren;
	HW_U16 ss_dir_ratio;
	HW_U16 ls_dir_ratio;
	HW_U16 ss_crc_stren;
	HW_U8 ss_crc_min;
	HW_U8 ss_avg_smth;
	HW_U8 ss_dir_smth;
	HW_U8 dir_smth[4];
	HW_U8 hfr_smth_ratio;
	HW_U16 hfr_hf_wht_stren;
	HW_U16 hfr_hf_blk_stren;
	HW_U16 hfr_mf_wht_stren;
	HW_U16 hfr_mf_blk_stren;
	HW_U16 hfr_rd_wht_stren;
	HW_U16 hfr_rd_blk_stren;
	HW_U8 hfr_hf_cor_ratio;
	HW_U8 hfr_mf_cor_ratio;
	HW_U8 hfr_rd_cor_ratio;
	HW_U16 hfr_hf_mix_ratio;
	HW_U16 hfr_mf_mix_ratio;
	HW_U16 hfr_rd_mix_ratio;
	HW_U8 hfr_hf_mix_min_ratio;
	HW_U8 hfr_mf_mix_min_ratio;
	HW_U8 hfr_rd_mix_min_ratio;
	HW_U16 hfr_hf_wht_clp;
	HW_U16 hfr_hf_blk_clp;
	HW_U16 hfr_mf_wht_clp;
	HW_U16 hfr_mf_blk_clp;
	HW_U16 hfr_rd_wht_clp;
	HW_U16 hfr_rd_blk_clp;
	HW_U16 wht_clp_para;
	HW_U16 blk_clp_para;
	HW_U8 wht_clp_slp;
	HW_U8 blk_clp_slp;
	HW_U8 max_clp_ratio;
	HW_U16 wht_sat_ratio;
	HW_U16 blk_sat_ratio;
	HW_U8 wht_slp_bt;
	HW_U8 blk_slp_bt;

	HW_U16 sharp_ss_value[ISP_REG_TBL_LENGTH];
	HW_U16 sharp_ls_value[ISP_REG_TBL_LENGTH];
	HW_U16 sharp_edge_lum[ISP_REG_TBL_LENGTH];
	HW_U16 sharp_hsv[46];
	HW_U16 sharp_gm_tbl[65];
	HW_U16 sharp_invgm_tbl[65];
};

struct encpp_static_sharp_config {
	HW_U8 ss_shp_ratio;
	HW_U8 ls_shp_ratio;
	HW_U16 ss_dir_ratio;
	HW_U16 ls_dir_ratio;
	HW_U16 ss_crc_stren;
	HW_U8 ss_crc_min;
	HW_U16 wht_sat_ratio;
	HW_U16 blk_sat_ratio;
	HW_U8 wht_slp_bt;
	HW_U8 blk_slp_bt;

	HW_U16 sharp_ss_value[ISP_REG_TBL_LENGTH];
	HW_U16 sharp_ls_value[ISP_REG_TBL_LENGTH];
	HW_U16 sharp_hsv[46];
};

struct encpp_dynamic_sharp_config {
	HW_U16 ss_ns_lw;
	HW_U16 ss_ns_hi;
	HW_U16 ls_ns_lw;
	HW_U16 ls_ns_hi;
	HW_U8 ss_lw_cor;
	HW_U8 ss_hi_cor;
	HW_U8 ls_lw_cor;
	HW_U8 ls_hi_cor;
	HW_U16 ss_blk_stren;
	HW_U16 ss_wht_stren;
	HW_U16 ls_blk_stren;
	HW_U16 ls_wht_stren;
	HW_U8 ss_avg_smth;
	HW_U8 ss_dir_smth;
	HW_U8 dir_smth[4];
	HW_U8 hfr_smth_ratio;
	HW_U16 hfr_hf_wht_stren;
	HW_U16 hfr_hf_blk_stren;
	HW_U16 hfr_mf_wht_stren;
	HW_U16 hfr_mf_blk_stren;
	HW_U8 hfr_hf_cor_ratio;
	HW_U8 hfr_mf_cor_ratio;
	HW_U16 hfr_hf_mix_ratio;
	HW_U16 hfr_mf_mix_ratio;
	HW_U8 hfr_hf_mix_min_ratio;
	HW_U8 hfr_mf_mix_min_ratio;
	HW_U16 hfr_hf_wht_clp;
	HW_U16 hfr_hf_blk_clp;
	HW_U16 hfr_mf_wht_clp;
	HW_U16 hfr_mf_blk_clp;
	HW_U16 wht_clp_para;
	HW_U16 blk_clp_para;
	HW_U8 wht_clp_slp;
	HW_U8 blk_clp_slp;
	HW_U8 max_clp_ratio;

	HW_U16 sharp_edge_lum[ISP_REG_TBL_LENGTH];
};

struct enc_VencIsp2VeParam {
    unsigned char encpp_en;
    struct encpp_static_sharp_config mStaticSharpCfg;
    struct encpp_dynamic_sharp_config mDynamicSharpCfg;
};

struct encoder_3dnr_config {
	unsigned char enable_3d_fliter;
	unsigned char adjust_pix_level_enable; // adjustment of coef pix level enable
	unsigned char smooth_filter_enable;    //* 3x3 smooth filter enable
	unsigned char max_pix_diff_th;         //* range[0~31]: maximum threshold of pixel difference
	unsigned char max_mad_th;              //* range[0~63]: maximum threshold of mad
	unsigned char max_mv_th;               //* range[0~63]: maximum threshold of motion vector
	unsigned char min_coef;                //* range[0~16]: minimum weight of 3d filter
	unsigned char max_coef;                //* range[0~16]: maximum weight of 3d filter,
};

struct encoder_2dnr_config {
	unsigned char enable_2d_fliter;
	unsigned char filter_strength_uv; //* range[0~255], 0 means close 2d filter, advice: 32
	unsigned char filter_strength_y;  //* range[0~255], 0 means close 2d filter, advice: 32
	unsigned char filter_th_uv;       //* range[0~15], advice: 2
	unsigned char filter_th_y;        //* range[0~15], advice: 2
};

struct enc_MovingLevelInfo {
	unsigned char is_overflow;
	unsigned short moving_level_table[ISP_MSC_TBL_SIZE];
};

struct enc_VencVe2IspParam {
//	int d2d_level; //[1,1024], 256 means 1X
//	int d3d_level; //[1,1024], 256 means 1X
	struct enc_MovingLevelInfo mMovingLevelInfo;
};

struct isp_h3a_coor_win {
	HW_S32 x1;
	HW_S32 y1;
	HW_S32 x2;
	HW_S32 y2;
};

struct npu_face_nr_config {
	unsigned char roi_num;
	struct isp_h3a_coor_win face_roi[20];
};

struct isp_saturation_config {
	HW_S16 satu_r;
	HW_S16 satu_g;
	HW_S16 satu_b;
	HW_S16 satu_gain;
};

#if (ISP_VERSION >= 520)
struct isp_dehaze_config {
	unsigned char dog_width_set;
	unsigned char dog_stat_num_power;
	unsigned short bright_scale;
	unsigned char blc_num_w;
	unsigned char blc_num_h;
	unsigned char blc_min_ratio;
	unsigned char fogw;
	unsigned char hazew;
	unsigned short blc_w;
	unsigned short blc_h;
	unsigned short blc_w_rec;
	unsigned short blc_h_rec;
	unsigned short airlight_r;
	unsigned short airlight_g;
	unsigned short airlight_b;
	unsigned int airlight_r_rec;
	unsigned int airlight_g_rec;
	unsigned int airlight_b_rec;
	unsigned int airlight_stat_num_th;
	unsigned short airlight_stat_value_th;
	unsigned short protect_dark_mean;
	unsigned short protect_proj_mean;
};
#endif

#if (ISP_VERSION >= 521)
struct isp_af_en_config {
	unsigned char af_iir0_en;
	unsigned char af_fir0_en;
	unsigned char af_iir0_sec0_en;
	unsigned char af_iir0_sec1_en;
	unsigned char af_iir0_sec2_en;
	unsigned char af_iir0_ldg_en;
	unsigned char af_fir0_ldg_en;
	unsigned char af_iir_ds_en;
	unsigned char af_fir_ds_en;
	unsigned char af_offset_en;
	unsigned char af_peak_en;
	unsigned char af_squ_en;
};

struct isp_af_filter_config {
	short af_iir0_g0;
	short af_iir0_g1;
	short af_iir0_g2;
	short af_iir0_g3;
	short af_iir0_g4;
	short af_iir0_g5;
	unsigned short af_iir0_s0;
	unsigned short af_iir0_s1;
	unsigned short af_iir0_s2;
	unsigned short af_iir0_s3;
	char af_fir0_g0;
	char af_fir0_g1;
	char af_fir0_g2;
	char af_fir0_g3;
	char af_fir0_g4;
	unsigned char af_iir0_dilate;
	unsigned char af_iir0_ldg_lgain;
	unsigned char af_iir0_ldg_hgain;
	unsigned char af_iir0_ldg_lth;
	unsigned char af_iir0_ldg_hth;
	unsigned char af_fir0_ldg_lgain;
	unsigned char af_fir0_ldg_hgain;
	unsigned char af_fir0_ldg_lth;
	unsigned char af_fir0_ldg_hth;
	unsigned char af_iir0_ldg_lslope;
	unsigned char af_iir0_ldg_hslope;
	unsigned char af_fir0_ldg_lslope;
	unsigned char af_fir0_ldg_hslope;
	unsigned char af_iir0_core_th;
	unsigned char af_iir0_core_peak;
	unsigned char af_fir0_core_th;
	unsigned char af_fir0_core_peak;
	unsigned char af_iir0_core_slope;
	unsigned char af_fir0_core_slope;
	unsigned char af_hlt_th;
	short af_r_offset;
	short af_g_offset;
	short af_b_offset;
};
#else
struct isp_af_en_config {
	unsigned char af_iir0_en;
	unsigned char af_iir1_en;
	unsigned char af_fir0_en;
	unsigned char af_fir1_en;
	unsigned char af_iir0_sec0_en;
	unsigned char af_iir0_sec1_en;
	unsigned char af_iir0_sec2_en;
	unsigned char af_iir1_sec0_en;
	unsigned char af_iir1_sec1_en;
	unsigned char af_iir1_sec2_en;
	unsigned char af_iir0_ldg_en;
	unsigned char af_iir1_ldg_en;
	unsigned char af_fir0_ldg_en;
	unsigned char af_fir1_ldg_en;
	unsigned char af_iir_ds_en;
	unsigned char af_fir_ds_en;
	unsigned char af_offset_en;
	unsigned char af_peak_en;
	unsigned char af_squ_en;
};

struct isp_af_filter_config {
	unsigned short af_iir0_g0;
	unsigned short af_iir0_g1;
	unsigned short af_iir0_g2;
	unsigned short af_iir0_g3;
	unsigned short af_iir0_g4;
	unsigned short af_iir0_g5;
	unsigned short af_iir1_g0;
	unsigned short af_iir1_g1;
	unsigned short af_iir1_g2;
	unsigned short af_iir1_g3;
	unsigned short af_iir1_g4;
	unsigned short af_iir1_g5;
	unsigned short af_iir0_s0;
	unsigned short af_iir1_s0;
	unsigned short af_iir0_s1;
	unsigned short af_iir1_s1;
	unsigned short af_iir0_s2;
	unsigned short af_iir1_s2;
	unsigned short af_iir0_s3;
	unsigned short af_iir1_s3;
	unsigned char af_fir0_g0;
	unsigned char af_fir0_g1;
	unsigned char af_fir0_g2;
	unsigned char af_fir0_g3;
	unsigned char af_fir0_g4;
	unsigned char af_fir1_g0;
	unsigned char af_fir1_g1;
	unsigned char af_fir1_g2;
	unsigned char af_fir1_g3;
	unsigned char af_fir1_g4;
	unsigned char af_iir0_dilate;
	unsigned char af_iir1_dilate;
	unsigned char af_iir0_ldg_lgain;
	unsigned char af_iir0_ldg_hgain;
	unsigned char af_iir1_ldg_lgain;
	unsigned char af_iir1_ldg_hgain;
	unsigned char af_iir0_ldg_lth;
	unsigned char af_iir0_ldg_hth;
	unsigned char af_iir1_ldg_lth;
	unsigned char af_iir1_ldg_hth;
	unsigned char af_fir0_ldg_lgain;
	unsigned char af_fir0_ldg_hgain;
	unsigned char af_fir1_ldg_lgain;
	unsigned char af_fir1_ldg_hgain;
	unsigned char af_fir0_ldg_lth;
	unsigned char af_fir0_ldg_hth;
	unsigned char af_fir1_ldg_lth;
	unsigned char af_fir1_ldg_hth;
	unsigned char af_iir0_ldg_lslope;
	unsigned char af_iir0_ldg_hslope;
	unsigned char af_iir1_ldg_lslope;
	unsigned char af_iir1_ldg_hslope;
	unsigned char af_fir0_ldg_lslope;
	unsigned char af_fir0_ldg_hslope;
	unsigned char af_fir1_ldg_lslope;
	unsigned char af_fir1_ldg_hslope;
	unsigned char af_iir0_core_th;
	unsigned char af_iir1_core_th;
	unsigned char af_iir0_core_peak;
	unsigned char af_iir1_core_peak;
	unsigned char af_fir0_core_th;
	unsigned char af_fir1_core_th;
	unsigned char af_fir0_core_peak;
	unsigned char af_fir1_core_peak;
	unsigned char af_iir0_core_slope;
	unsigned char af_iir1_core_slope;
	unsigned char af_fir0_core_slope;
	unsigned char af_fir1_core_slope;
	unsigned char af_hlt_th;
	unsigned char af_hlt_cnt_shift;
	unsigned short af_r_offset;
	unsigned short af_g_offset;
	unsigned short af_b_offset;
};
#endif

enum isp_output_speed {
	ISP_OUTPUT_SPEED_0 = 0,
	ISP_OUTPUT_SPEED_1 = 1,
	ISP_OUTPUT_SPEED_2 = 2,
	ISP_OUTPUT_SPEED_3 = 3,
};

struct isp_bayer_gain_offset {
	HW_U16 r_gain;
	HW_U16 gr_gain;
	HW_U16 gb_gain;
	HW_U16 b_gain;

	HW_S16 r_offset;
	HW_S16 gr_offset;
	HW_S16 gb_offset;
	HW_S16 b_offset;
};

struct isp_wb_gain {
	HW_U16 r_gain;
	HW_U16 gr_gain;
	HW_U16 gb_gain;
	HW_U16 b_gain;
};

/**
 * struct isp_rgb2rgb_gain_offset - RGB to RGB Blending
 * @matrix:
 *              [RR] [GR] [BR]
 *              [RG] [GG] [BG]
 *              [RB] [GB] [BB]
 * @offset: Blending offset value for R,G,B.
 */
struct isp_rgb2rgb_gain_offset {
	HW_S16 matrix[3][3];
	HW_S16 offset[3];
};

struct isp_rgb2yuv_gain_offset {
	HW_S16 matrix[3][3];
	HW_S16 offset[3];
};

struct isp_lsc_config {
	HW_U16 ct_x;
	HW_U16 ct_y;
	HW_U16 rs_val;
};

struct isp_disc_config {
	HW_U16 disc_ct_x;
	HW_U16 disc_ct_y;
	HW_U16 disc_rs_val;
};

struct isp_h3a_reg_win {
	HW_U8 hor_num;
	HW_U8 ver_num;
	HW_U32 width;
	HW_U32 height;
	HW_U32 hor_start;
	HW_U32 ver_start;
	HW_U8 shift_bit0;
	//HW_U8 shift_bit1;
	HW_U8 shift_bit2;
};

typedef struct isp_sensor_info {
	/*frome sensor*/
	char *name;
	HW_U8 hflip;
	HW_U8 vflip;
	HW_U32 hts;
	HW_U32 vts;
	HW_U32 pclk;
	HW_U16 fps_fixed;
	HW_U32 bin_factor;
	HW_U32 gain_min;
	HW_U32 gain_max;
	HW_U32 exp_min;
	HW_U32 exp_max;
	HW_S16 sensor_width;
	HW_S16 sensor_height;
	HW_U16 hoffset;
	HW_U16 voffset;
	HW_U8 input_seq;
	HW_U8 wdr_mode;
	HW_U8 color_space;
	HW_U8 bayer_bit;
	HW_S16 temperature;

	/*from ae*/
	HW_U32 exp_line;
	HW_U32 ang_gain;
	HW_U32 dig_gain;
	HW_U32 total_gain;

	HW_U32 ae_tbl_idx;
	HW_U32 ae_tbl_idx_max;

	HW_U16 fps;
	HW_U32 frame_time;
	HW_S32 ae_gain;
	HW_U8 is_ae_done;
	HW_U8 backlight;

	/*from motion detect*/
	HW_S32 motion_flag;

	/*awb*/
	HW_S32 ae_lv;
	struct isp_bayer_gain_offset gain_offset;

	/*from af*/
	HW_S32 is_af_busy;
} isp_sensor_info_t;

enum exposure_cfg_type {
	ANTI_EXP_WIN_OVER     = 0,
	ANTI_EXP_WIN_UNDER = 1,
	ANTI_EXP_HIST_OVER = 2,
	ANTI_EXP_HIST_UNDER = 3,

	AE_PREVIEW_SPEED = 4,
	AE_CAPTURE_SPEED = 5,
	AE_VIDEO_SPEED = 6,
	AE_TOUCH_SPEED = 7,
	AE_TOLERANCE = 8,
	AE_TARGET = 9,

	AE_HIST_DARK_WEIGHT_MIN = 10,
	AE_HIST_DARK_WEIGHT_MAX = 11,
	AE_HIST_BRIGHT_WEIGHT_MIN = 12,
	AE_HIST_BRIGHT_WEIGHT_MAX = 13,

	ISP_EXP_CFG_MAX,
};

enum wdr_cfg_type {
	WDR_CMP_LM_LTH = 0, //LM
	WDR_CMP_LM_HTH,
	WDR_SEXP_CHK_LM_LTH,
	WDR_SEXP_CHK_LM_HTH,
	WDR_LECC_JDG_LM_LTH,
	WDR_LECC_JDG_LM_HTH,
	WDR_LECC_CRC_LM_LTH,
	WDR_LECC_CRC_LM_RTH,
	WDR_DE_PURPL_LM_LTH,
	WDR_DE_PURPL_LM_LRT,
	WDR_MV_CHK_LM_LTH,
	WDR_MV_CHK_LM_HTH,
	WDR_MV_FCL_LM_LTH,
	WDR_MV_FCL_LM_HTH,

	WDR_CMP_MS_LTH, //MS
	WDR_CMP_MS_HTH,
	WDR_SEXP_CHK_MS_LTH,
	WDR_SEXP_CHK_MS_HTH,
	WDR_LECC_JDG_MS_LTH,
	WDR_LECC_JDG_MS_HTH,
	WDR_LECC_CRC_MS_LTH,
	WDR_LECC_CRC_MS_RTH,
	WDR_DE_PURPL_MS_LTH,
	WDR_DE_PURPL_MS_LRT,
	WDR_MV_CHK_MS_LTH,
	WDR_MV_CHK_MS_HTH,
	WDR_MV_FCL_MS_LTH,
	WDR_MV_FCL_MS_HTH,
	ISP_WDR_CFG_MAX,
};

enum wdr_output_mode {
	WDR_OUTPUT_STITCH = 0,
	WDR_OUTPUT_STITCH_LONG,
	WDR_OUTPUT_STITCH_MIDDLE,
	WDR_OUTPUT_STITCH_SHORT,
	WDR_OUTPUT_RESERVED,
	WDR_OUTPUT_LONG,
	WDR_OUTPUT_MIDDLE,
	WDR_OUTPUT_SHORT,
	ISP_WDR_OUTPUT_MODE_MAX,
};

enum wdr_split_cfg_type {
	WDR_SPLIT_INPUT_NRML = 0,
	WDR_SPLIT_INPUT_MODE,
	WDR_SPLIT_BLC_EN,
	WDR_SPLIT_DECOMP_INPUT_BIT,
	WDR_SPLIT_BITEXP_OUTPUT_BIT,
	WDR_SPLIT_DECOMP_ELM_RATIO,
	WDR_SPLIT_DECOMP_EMS_RATIO,
	WDR_SPLIT_DECOMP_RANGE0,
	WDR_SPLIT_DECOMP_RANGE1,
	WDR_SPLIT_DECOMP_RANGE2,
	WDR_SPLIT_DECOMP_RANGE3,
	WDR_SPLIT_DECOMP_RANGE4,
	WDR_SPLIT_DECOMP_SLOPE0,
	WDR_SPLIT_DECOMP_SLOPE1,
	WDR_SPLIT_DECOMP_SLOPE2,
	WDR_SPLIT_DECOMP_SLOPE3,
	WDR_SPLIT_DECOMP_SLOPE4,
	WDR_SPLIT_DECOMP_OFFSET0,
	WDR_SPLIT_DECOMP_OFFSET1 ,
	WDR_SPLIT_DECOMP_OFFSET2,
	WDR_SPLIT_DECOMP_OFFSET3,
	ISP_WDR_SPLIT_CFG_MAX,
};

enum wdr_comm_cfg_type {
	WDR_COMM_OUT_SEL = 0,
	WDR_COMM_LM_EXP_RATIO,
	WDR_COMM_MS_EXP_RATIO,
	WDR_COMM_LWB_NRATIO,
	WDR_COMM_SEXP_CHK_LM_EN,
	WDR_COMM_LECC_LM_EN,
	WDR_COMM_DE_PURPL_LM_EN,
	WDR_COMM_MV_CHK_LM_EN,
	WDR_COMM_SEXP_CHK_MS_EN,
	WDR_COMM_LECC_MS_EN,
	WDR_COMM_DE_PURPL_MS_EN,
	WDR_COMM_MV_CHK_MS_EN,
	ISP_WDR_COMM_CFG_MAX,
};

enum ae_table_mode {
	SCENE_MODE_PREVIEW = 0,
	SCENE_MODE_CAPTURE,
	SCENE_MODE_VIDEO,

	SCENE_MODE_BACKLIGHT,
	SCENE_MODE_BEACH_SNOW,
	SCENE_MODE_FIREWORKS,
	SCENE_MODE_LANDSCAPE,
	SCENE_MODE_NIGHT,
	SCENE_MODE_SPORTS,

	SCENE_MODE_USER_DEF0,
	SCENE_MODE_USER_DEF1,
	SCENE_MODE_USER_DEF2,
	SCENE_MODE_USER_DEF3,
	SCENE_MODE_USER_DEF4,
	SCENE_MODE_USER_DEF5,
	SCENE_MODE_SENSOR_DRIVER,

	SCENE_MODE_MAX,
};

enum temperture_comp_type {
	TEMP_COMP_2D_BLACK = 0,
	TEMP_COMP_2D_WHITE = 1,
	TEMP_COMP_3D_BLACK = 2,
	TEMP_COMP_3D_WHITE = 3,
	TEMP_COMP_DTC_STREN = 4,
	TEMP_COMP_BLC_R = 5,
	TEMP_COMP_BLC_G = 6,
	TEMP_COMP_BLC_B = 7,
	TEMP_COMP_SHARP = 8,
	TEMP_COMP_SATU_LOW = 9,
	TEMP_COMP_SATU_MID = 10,
	TEMP_COMP_SATU_HIGH = 11,
	TEMP_COMP_MAX,
};

struct sensor_temp_info {
	HW_U8 enable;
	HW_S16 temperature_param[TEMP_COMP_MAX];
};

struct ae_table {
	HW_U32 min_exp;  //us
	HW_U32 max_exp;
	HW_U32 min_gain;
	HW_U32 max_gain;
	HW_U32 min_iris;
	HW_U32 max_iris;
};

struct ae_table_info {
	struct ae_table ae_tbl[10];
	HW_S32 length;
	HW_S32 ev_step;
	HW_S32 shutter_shift;
};

#endif //__BSP__ISP__COMM__H

