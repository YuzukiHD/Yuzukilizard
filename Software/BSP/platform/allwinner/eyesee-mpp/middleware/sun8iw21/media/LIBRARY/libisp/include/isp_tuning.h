/*
 ******************************************************************************
 *
 * isp_tuning_adapter.h
 *
 * Hawkview ISP - isp_tuning_adapter.h module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   1.0		  clarkyy	2016/05/09	VIDEO INPUT
 *
 *****************************************************************************
 */

#ifndef _ISP_TUNING_CFG_H_
#define _ISP_TUNING_CFG_H_

#include "../include/isp_type.h"
#include "../include/isp_comm.h"

#define AW_ERR_VI_INVALID_PARA            -1
#define AW_ERR_VI_INVALID_DEVID           -2
#define AW_ERR_VI_INVALID_CHNID           -3
#define AW_ERR_VI_INVALID_NULL_PTR        -4
#define AW_ERR_VI_FAILED_NOTCONFIG        -5
#define AW_ERR_VI_SYS_NOTREADY            -6
#define AW_ERR_VI_BUF_EMPTY               -7
#define AW_ERR_VI_BUF_FULL                -8
#define AW_ERR_VI_NOMEM                   -9
#define AW_ERR_VI_NOT_SUPPORT             -10
#define AW_ERR_VI_BUSY                    -11
#define AW_ERR_VI_FAILED_NOTENABLE        -12
#define AW_ERR_VI_FAILED_NOTDISABLE       -13
#define AW_ERR_VI_CFG_TIMEOUT             -14
#define AW_ERR_VI_NORM_UNMATCH            -15
#define AW_ERR_VI_INVALID_PHYCHNID        -16
#define AW_ERR_VI_FAILED_NOTBIND          -17
#define AW_ERR_VI_FAILED_BINDED           -18

/*
 * id defination
 * 5 bytes total, first byte is the group id(current 4 groups total), left 4 bytes is the cfg id
 *   |group id|cfg id|cfg id|cfg id|cfg id|
 * group id : 0x1 ~ 0xFF, each number means a individual group id, total 255 group ids
 * cfg id: 0x1 ~ 0xFFFFFFFF, each bit means a individual cfg id, total 32 cfg ids
 */
typedef enum {
	HW_ISP_CFG_TEST                    = 0x01,
	HW_ISP_CFG_3A                      = 0x02,
	HW_ISP_CFG_TUNING                  = 0x03,
	HW_ISP_CFG_TUNING_TABLES           = 0x04,
	HW_ISP_CFG_DYNAMIC                 = 0x05,
	HW_ISP_CFG_GROUP_COUNT
} hw_isp_cfg_groups;

typedef enum {
	/* isp_test_param_cfg */
	HW_ISP_CFG_TEST_PUB                        = 0x00000001,
	HW_ISP_CFG_TEST_EXPTIME                    = 0x00000002,
	HW_ISP_CFG_TEST_GAIN                       = 0x00000004,
	HW_ISP_CFG_TEST_FOCUS                      = 0x00000008,
	HW_ISP_CFG_TEST_FORCED                     = 0x00000010,
	HW_ISP_CFG_TEST_ENABLE                     = 0x00000020,
	HW_ISP_CFG_TEST_SPECIAL_CTRL               = 0x00000040,
} hw_isp_cfg_test_ids;

typedef enum {
	/* isp_3a_param_cfg */
	/* ae */
	HW_ISP_CFG_AE_PUB                          = 0x00000001,
	HW_ISP_CFG_AE_PREVIEW_TBL                  = 0x00000002,
	HW_ISP_CFG_AE_CAPTURE_TBL                  = 0x00000004,
	HW_ISP_CFG_AE_VIDEO_TBL                    = 0x00000008,
	HW_ISP_CFG_AE_WIN_WEIGHT                   = 0x00000010,
	HW_ISP_CFG_AE_DELAY                        = 0x00000020,
	/* awb */
	HW_ISP_CFG_AWB_PUB                         = 0x00000040,
	HW_ISP_CFG_AWB_TEMP_RANGE                  = 0x00000080,
	HW_ISP_CFG_AWB_DIST                        = 0x00000100,
	HW_ISP_CFG_AWB_LIGHT_INFO                  = 0x00000200,
	HW_ISP_CFG_AWB_EXT_LIGHT_INFO              = 0x00000400,
	HW_ISP_CFG_AWB_SKIN_INFO                   = 0x00000800,
	HW_ISP_CFG_AWB_SPECIAL_INFO                = 0x00001000,
	HW_ISP_CFG_AWB_PRESET_GAIN                 = 0x00002000,
	HW_ISP_CFG_AWB_FAVOR                       = 0x00004000,
	/* af */
	HW_ISP_CFG_AF_VCM_CODE                     = 0x00008000,
	HW_ISP_CFG_AF_OTP                          = 0x00010000,
	HW_ISP_CFG_AF_SPEED                        = 0x00020000,
	HW_ISP_CFG_AF_FINE_SEARCH                  = 0x00040000,
	HW_ISP_CFG_AF_REFOCUS                      = 0x00080000,
	HW_ISP_CFG_AF_TOLERANCE                    = 0x00100000,
	HW_ISP_CFG_AF_SCENE                        = 0x00200000,
	/* wdr */
	HW_ISP_CFG_WDR_SPLIT                       = 0x00400000,
	HW_ISP_CFG_WDR_STITCH                      = 0x00800000,
} hw_isp_cfg_3a_ids;

typedef enum {
	/* isp_tuning_param_cfg */
	HW_ISP_CFG_TUNING_FLASH                    = 0x00000001,
	HW_ISP_CFG_TUNING_FLICKER                  = 0x00000002,
	HW_ISP_CFG_TUNING_VISUAL_ANGLE             = 0x00000004,
	HW_ISP_CFG_TUNING_GTM                      = 0x00000008,
	HW_ISP_CFG_TUNING_CFA                      = 0x00000010,
	HW_ISP_CFG_TUNING_CTC                      = 0x00000020,
	HW_ISP_CFG_TUNING_DIGITAL_GAIN             = 0x00000040,
	HW_ISP_CFG_TUNING_CCM_LOW                  = 0x00000080,
	HW_ISP_CFG_TUNING_CCM_MID                  = 0x00000100,
	HW_ISP_CFG_TUNING_CCM_HIGH                 = 0x00000200,
	HW_ISP_CFG_TUNING_PLTM                     = 0x00000400,
	HW_ISP_CFG_TUNING_GCA                      = 0x00000800,
	HW_ISP_CFG_TUNING_BDNF_COMM                = 0x00001000,
	HW_ISP_CFG_TUNING_TDNF_COMM                = 0x00002000,
	HW_ISP_CFG_TUNING_SHARP_COMM               = 0x00004000,
	HW_ISP_CFG_TUNING_DPC                      = 0x00008000,
	HW_ISP_CFG_TUNING_ENCPP_SHARP_COMM         = 0x00010000,
	HW_ISP_CFG_TUNING_SENSOR                   = 0x00020000,

	/* vencoder param */
	HW_VENCODER_CFG_TUNING_COMMON              = 0x00100000,
	HW_VENCODER_CFG_TUNING_VBR                 = 0x00200000,
	HW_VENCODER_CFG_TUNING_AVBR                = 0x00400000,
	HW_VENCODER_CFG_TUNING_FIXQP               = 0x00800000,
	HW_VENCODER_CFG_TUNING_PROC                = 0x01000000,
	HW_VENCODER_CFG_TUNING_SAVEBSFILE          = 0x02000000,
} hw_isp_cfg_tuning_ids;

typedef enum {
	/* tuning tables */
	HW_ISP_CFG_TUNING_LSC                      = 0x00000001,
	HW_ISP_CFG_TUNING_GAMMA                    = 0x00000002,
	HW_ISP_CFG_TUNING_BDNF                     = 0x00000010,
	HW_ISP_CFG_TUNING_TDNF                     = 0x00000020,
	HW_ISP_CFG_TUNING_SHARP	                   = 0x00000080,
	HW_ISP_CFG_TUNING_CEM                      = 0x00000100,
	HW_ISP_CFG_TUNING_CEM_1                    = 0x00000200,
	HW_ISP_CFG_TUNING_PLTM_TBL                 = 0x00000400,
	HW_ISP_CFG_TUNING_WDR                      = 0x00000800,
	HW_ISP_CFG_TUNING_MSC                      = 0x00001000,
	HW_ISP_CFG_TUNING_LCA_TBL                  = 0x00002000,
	HW_ISP_CFG_TUNING_ENCPP_SHARP              = 0x00004000,
} hw_isp_cfg_tuning_table_ids;

typedef enum {
	/* isp_dynamic_cfg */
	HW_ISP_CFG_DYNAMIC_LUM_POINT               = 0x00000001,
	HW_ISP_CFG_DYNAMIC_GAIN_POINT              = 0x00000002,
	HW_ISP_CFG_DYNAMIC_SHARP                   = 0x00000004,
	HW_ISP_CFG_DYNAMIC_DENOISE                 = 0x00000010,
	HW_ISP_CFG_DYNAMIC_BLACK_LV                = 0x00000040,
	HW_ISP_CFG_DYNAMIC_DPC                     = 0x00000080,
	HW_ISP_CFG_DYNAMIC_PLTM                    = 0x00000100,
	HW_ISP_CFG_DYNAMIC_DEFOG                   = 0x00000200,
	HW_ISP_CFG_DYNAMIC_HISTOGRAM               = 0x00000400,
	HW_ISP_CFG_DYNAMIC_CEM                     = 0x00001000,
	HW_ISP_CFG_DYNAMIC_TDF                     = 0x00002000,
	HW_ISP_CFG_DYNAMIC_AE                      = 0x00004000,
	HW_ISP_CFG_DYNAMIC_GTM                     = 0x00008000,
	HW_ISP_CFG_DYNAMIC_LCA                     = 0x00010000,
	HW_ISP_CFG_DYNAMIC_CFA                     = 0x00020000,
	HW_ISP_CFG_DYNAMIC_ENCPP_SHARP             = 0x00040000,
	HW_ISP_CFG_DYNAMIC_WDR                     = 0x00080000,
	HW_ISP_CFG_DYNAMIC_ENCODER_DENOISE         = 0x00100000,
	HW_ISP_CFG_DYNAMIC_SHADING                 = 0x00200000,
} hw_isp_cfg_dynamic_ids;

typedef enum {
	/*isp_ctrl*/
	ISP_CTRL_MODULE_EN = 0,
	ISP_CTRL_DIGITAL_GAIN,
	ISP_CTRL_PLTMWDR_STR,
	ISP_CTRL_DN_STR,
	ISP_CTRL_3DN_STR,
	ISP_CTRL_HIGH_LIGHT,
	ISP_CTRL_BACK_LIGHT,
	ISP_CTRL_WB_MGAIN,
	ISP_CTRL_AGAIN_DGAIN,
	ISP_CTRL_COLOR_EFFECT,
	ISP_CTRL_AE_ROI,
	ISP_CTRL_AF_METERING,
	ISP_CTRL_COLOR_TEMP,
	ISP_CTRL_EV_IDX,
	ISP_CTRL_PLTM_HARDWARE_STR,
	ISP_CTRL_ISO_LUM_IDX,
	ISP_CTRL_COLOR_SPACE,
	ISP_CTRL_VENC2ISP_PARAM,
	ISP_CTRL_NPU_NR_PARAM,
	ISP_CTRL_TOTAL_GAIN,
} hw_isp_ctrl_cfg_ids;

typedef enum {
	/*encpp_ctrl*/
	ISP_CTRL_ENCPP_EN = 0,
	ISP_CTRL_ENCPP_STATIC_CFG,
	ISP_CTRL_ENCPP_DYNAMIC_CFG,
	ISP_CTRL_ENCODER_3DNR_CFG,
	ISP_CTRL_ENCODER_2DNR_CFG,
}hw_encpp_ctrl_cfg_ids;

struct isp_test_pub_cfg {
	HW_S32		test_mode;
	HW_S32		gain;
	HW_S32		exp_line;
	HW_S32		color_temp;
	HW_S32		log_param;
};

struct isp_test_item_cfg {
	HW_S32		enable;
	HW_S32		start;
	HW_S32		step;
	HW_S32		end;
	HW_S32		change_interval;
};

struct isp_test_forced_cfg {
	HW_S32		ae_enable;
	HW_S32		lum;
};

struct isp_test_enable_cfg {
	HW_S8 manual;
	HW_S8 afs;
	HW_S8 ae;
	HW_S8 af;
	HW_S8 awb;
	HW_S8 hist;
	HW_S8 wdr_split;
	HW_S8 wdr_stitch;
	HW_S8 otf_dpc;
	HW_S8 ctc;
	HW_S8 gca;
	HW_S8 nrp;
	HW_S8 denoise;
	HW_S8 tdf;
	HW_S8 blc;
	HW_S8 wb;
	HW_S8 dig_gain;
	HW_S8 lsc;
	HW_S8 msc;
	HW_S8 pltm;
	HW_S8 cfa;
	HW_S8 lca;
	HW_S8 sharp;
	HW_S8 ccm;
	HW_S8 defog;
	HW_S8 cnr;
	HW_S8 drc;
	HW_S8 gtm;
	HW_S8 gamma;
	HW_S8 cem;
	HW_S8 encpp_en;
	HW_S8 enc_3dnr_en;
	HW_S8 enc_2dnr_en;
};

struct isp_test_special_ctrl_cfg {
	HW_S8 ir_mode;
	HW_S8 color_space;
};

/* isp_test_param cfg */
struct isp_test_param_cfg {
	struct isp_test_pub_cfg       test_pub;      /* id: 0x0100000001 */
	struct isp_test_item_cfg      test_exptime;  /* id: 0x0100000002 */
	struct isp_test_item_cfg      test_gain;     /* id: 0x0100000004 */
	struct isp_test_item_cfg      test_focus;    /* id: 0x0100000008 */
	struct isp_test_forced_cfg    test_forced;   /* id: 0x0100000010 */
	struct isp_test_enable_cfg    test_enable;   /* id: 0x0100000020 */
	struct isp_test_special_ctrl_cfg test_special_ctrl;/* id: 0x0100000040 */
};

struct isp_ae_pub_cfg {
	HW_S32		define_table;
	HW_S32		max_lv;
	HW_S32		hist_mode_en;
	HW_S32		hist0_sel;
	HW_S32		hist1_sel;
	HW_S32		stat_sel;
	HW_S32		compensation_step;
	HW_S32		touch_dist_index;
	HW_S32		iso2gain_ratio;
	HW_S32		fno_table[16];
	HW_S32		ev_step;
	HW_S32		conv_data_index;
	HW_S32		blowout_pre_en;
	HW_S32		blowout_attr;
};

struct isp_ae_table_cfg {
	HW_S32		length;
	struct ae_table	value[7];
};

struct isp_ae_weight_cfg {
	HW_S32		weight[64];
};

struct isp_ae_delay_cfg {
	HW_S32		ae_frame;
	HW_S32		exp_frame;
	HW_S32		gain_frame;
};

struct isp_wdr_split_cfg {
	HW_U16 wdr_split_cfg[ISP_WDR_SPLIT_CFG_MAX];
};

struct isp_wdr_comm_cfg {
	HW_U16 value[ISP_WDR_COMM_CFG_MAX];
};

struct isp_awb_pub_cfg {
	HW_S32		interval;
	HW_S32		speed;
	HW_S32		stat_sel;
};

struct isp_awb_temp_range_cfg {
	HW_S32		low;
	HW_S32		high;
	HW_S32		base;
};

struct isp_awb_dist_cfg {
	HW_S32		green_zone;
	HW_S32		blue_sky;
};

struct isp_awb_temp_info_cfg {
	HW_S32		number;
	HW_S32		value[320];
};

struct isp_awb_preset_gain_cfg {
	HW_S32		value[22];
};

struct isp_awb_favor_cfg {
	HW_S32		rgain;
	HW_S32		bgain;
};

struct isp_af_vcm_code_cfg {
	HW_S32		min;
	HW_S32		max;
};

struct isp_af_otp_cfg {
	HW_S32		use_otp;
};

struct isp_af_speed_cfg {
	HW_S32		interval_time;
	HW_S32		index;
};

struct isp_af_fine_search_cfg {
	HW_S32		auto_en;
	HW_S32		single_en;
	HW_S32		step;
};

struct isp_af_refocus_cfg {
	HW_S32		move_cnt;
	HW_S32		still_cnt;
	HW_S32		move_monitor_cnt;
	HW_S32		still_monitor_cnt;
};

struct isp_af_tolerance_cfg {
	HW_S32		near_distance;
	HW_S32		far_distance;
	HW_S32		offset;
	HW_S32		table_length;
	HW_S32		std_code_table[20];
	HW_S32		value[20];
};

struct isp_af_scene_cfg {
	HW_S32		stable_min;
	HW_S32		stable_max;
	HW_S32		low_light_lv;
	HW_S32		peak_thres;
	HW_S32		direction_thres;
	HW_S32		change_ratio;
	HW_S32		move_minus;
	HW_S32		still_minus;
	HW_S32		scene_motion_thres;
};

/* isp_3a_param cfg*/
struct isp_3a_param_cfg {
	/* ae */
	struct isp_ae_pub_cfg           ae_pub;                 /* id: 0x0200000001 */
	struct isp_ae_table_cfg         ae_preview_tbl;         /* id: 0x0200000002 */
	struct isp_ae_table_cfg         ae_capture_tbl;         /* id: 0x0200000004 */
	struct isp_ae_table_cfg         ae_video_tbl;           /* id: 0x0200000008 */
	struct isp_ae_weight_cfg        ae_win_weight;          /* id: 0x0200000010 */
	struct isp_ae_delay_cfg         ae_delay;               /* id: 0x0200000020 */
	/* awb */
	struct isp_awb_pub_cfg          awb_pub;                /* id: 0x0200000040 */
	struct isp_awb_temp_range_cfg   awb_temp_range;         /* id: 0x0200000080 */
	struct isp_awb_dist_cfg         awb_dist;               /* id: 0x0200000100 */
	struct isp_awb_temp_info_cfg    awb_light_info;         /* id: 0x0200000200 */
	struct isp_awb_temp_info_cfg    awb_ext_light_info;     /* id: 0x0200000400 */
	struct isp_awb_temp_info_cfg    awb_skin_info;          /* id: 0x0200000800 */
	struct isp_awb_temp_info_cfg    awb_special_info;       /* id: 0x0200001000 */
	struct isp_awb_preset_gain_cfg  awb_preset_gain;        /* id: 0x0200002000 */
	struct isp_awb_favor_cfg        awb_favor;              /* id: 0x0200004000 */
	/* af */
	struct isp_af_vcm_code_cfg      af_vcm_code;            /* id: 0x0200008000 */
	struct isp_af_otp_cfg           af_otp;                 /* id: 0x0200010000 */
	struct isp_af_speed_cfg         af_speed;               /* id: 0x0200020000 */
	struct isp_af_fine_search_cfg   af_fine_search;         /* id: 0x0200040000 */
	struct isp_af_refocus_cfg       af_refocus;             /* id: 0x0200080000 */
	struct isp_af_tolerance_cfg     af_tolerance;           /* id: 0x0200100000 */
	struct isp_af_scene_cfg         af_scene;               /* id: 0x0200200000 */
	/* wdr */
	struct isp_wdr_split_cfg        wdr_split;              /* id: 0x0200400000 */
	struct isp_wdr_comm_cfg         wdr_comm;               /* id: 0x0200800000 */
};

#define ISP_DYNAMIC_GROUP_COUNT 	14

struct isp_tuning_flash_cfg {
	HW_S32		gain;
	HW_S32		delay_frame;
};

struct isp_tuning_flicker_cfg {
	HW_S32		type;
	HW_S32		ratio;
};

struct isp_tuning_visual_angle_cfg {
	HW_S32		horizontal;
	HW_S32		vertical;
	HW_S32		focus_length;
};

/* global tone mapping */
struct isp_tuning_gtm_cfg {
	HW_S32		gtm_hist_sel;
	HW_S32		type;
	HW_S32		gamma_type;
	HW_S32		auto_alpha_en;
	HW_S32		hist_pix_cnt;
	HW_S32		dark_minval;
	HW_S32		bright_minval;
	HW_S16		plum_var[GTM_LUM_IDX_NUM][GTM_VAR_IDX_NUM];
};

struct isp_tuning_cfa_cfg {
	HW_S16		grad_th;
	HW_S16		dir_v_th;
	HW_S16		dir_h_th;
	HW_S16		res_smth_high;
	HW_S16		res_smth_low;
	HW_S16		res_high_th;
	HW_S16		res_low_th;
	HW_S16		res_dir_a;
	HW_S16		res_dir_d;
	HW_S16		res_dir_v;
	HW_S16		res_dir_h;
};

struct isp_tuning_ctc_cfg {
	HW_U16		min_thres;
	HW_U16		max_thres;
	HW_U16		slope_thres;
	HW_U16		dir_wt;
	HW_U16		dir_thres;
};

struct isp_tuning_blc_gain_cfg {
	HW_S32		value[ISP_RAW_CH_MAX];
};

struct isp_tuning_ccm_cfg {
	HW_U16		temperature;
	struct isp_rgb2rgb_gain_offset	value;
};

struct isp_tuning_pltm_cfg {
	HW_S32		value[ISP_PLTM_MAX];

};

struct isp_tuning_lca_pf_satu_lut {
	HW_U8  value[ISP_REG_TBL_LENGTH];
};
struct isp_tuning_lca_gf_satu_lut {
	HW_U8 value[ISP_REG_TBL_LENGTH];
};

struct isp_tuning_gca_cfg {
	HW_S16		value[ISP_GCA_MAX];
};

struct isp_tuning_bdnf_comm_cfg {
	HW_S16		value[ISP_DENOISE_COMM_MAX];
};

struct isp_tuning_tdnf_comm_cfg {
	HW_S16		value[ISP_TDF_COMM_MAX];
};

struct isp_tuning_sharp_comm_cfg {
	HW_S16		value[ISP_SHARP_COMM_MAX];
};

struct isp_tuning_dpc_cfg {
	HW_S8 dpc_remove_mode;
	HW_S8 dpc_sup_twinkle_en;
};

struct isp_tuning_encpp_sharp_comm_cfg {
	HW_S16		value[ENCPP_SHARP_COMM_MAX];
};

struct isp_tuning_sensor_temp_cfg {
	HW_S16		sensor_temp[14*TEMP_COMP_MAX]; // 14 * 12
};

struct isp_tuning_lsc_table_cfg {
	HW_S32		ff_mod;
	HW_S32		lsc_mode;
	HW_S32		center_x;
	HW_S32		center_y;
	HW_S32		rolloff_ratio;
	HW_U16		value[ISP_LSC_TEMP_NUM + ISP_LSC_TEMP_NUM][ISP_LSC_TBL_LENGTH];
	HW_U16		color_temp_triggers[ISP_LSC_TEMP_NUM];
};

struct isp_tuning_msc_table_cfg {
	HW_S8		mff_mod;
	HW_S8		msc_mode;
	HW_S16		msc_blw_lut[ISP_MSC_TBL_LUT_SIZE];
	HW_S16		msc_blh_lut[ISP_MSC_TBL_LUT_SIZE];
	HW_U16		value[ISP_MSC_TEMP_NUM + ISP_MSC_TEMP_NUM][ISP_MSC_TBL_LENGTH];
	HW_U16		color_temp_triggers[ISP_MSC_TEMP_NUM];
};

struct isp_tuning_gamma_table_cfg {
	HW_S32		number;
	HW_U16		value[ISP_GAMMA_TRIGGER_POINTS][ISP_GAMMA_TBL_LENGTH];
	HW_U16		lv_triggers[ISP_GAMMA_TRIGGER_POINTS];
};

struct isp_tuning_bdnf_item_cfg {
	HW_S16		lp0_thres[ISP_REG_TBL_LENGTH];
	HW_S16		lp1_thres[ISP_REG_TBL_LENGTH];
	HW_S16		lp2_thres[ISP_REG_TBL_LENGTH];
	HW_S16		lp3_thres[ISP_REG_TBL_LENGTH];
};

struct isp_tuning_bdnf_table_cfg {
	struct isp_tuning_bdnf_item_cfg thres[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_tuning_tdnf_table_cfg {
	HW_S16      thres[ISP_DYNAMIC_GROUP_COUNT][ISP_REG_TBL_LENGTH];
	HW_U8		df_shape[ISP_REG1_TBL_LENGTH];
	HW_U8		ratio_amp[ISP_REG1_TBL_LENGTH];
	HW_U8		k_dlt_bk[ISP_REG1_TBL_LENGTH];
	HW_U8		ct_rt_bk[ISP_REG1_TBL_LENGTH];
	HW_U8		dtc_hf_bk[ISP_REG1_TBL_LENGTH];
	HW_U8		dtc_mf_bk[ISP_REG1_TBL_LENGTH];
	HW_U8		lay0_d2d0_rt_br[ISP_REG1_TBL_LENGTH];
	HW_U8		lay1_d2d0_rt_br[ISP_REG1_TBL_LENGTH];
	HW_U8		lay0_nrd_rt_br[ISP_REG1_TBL_LENGTH];
	HW_U8		lay1_nrd_rt_br[ISP_REG1_TBL_LENGTH];
};

struct isp_tuning_sharp_table_cfg {
	HW_U16		edge_lum[ISP_DYNAMIC_GROUP_COUNT][ISP_REG_TBL_LENGTH];
	HW_U16		ss_val[ISP_REG_TBL_LENGTH];
	HW_U16		ls_val[ISP_REG_TBL_LENGTH];
	HW_U16		hsv[46];
};

struct isp_tuning_cem_table_cfg {
	HW_U8		value[ISP_CEM_MEM_SIZE];
};

struct isp_tuning_pltm_table_cfg {
	HW_U8		stat_gd_cv[PLTM_MAX_STAGE][15];
	HW_U8		df_cv[PLTM_MAX_STAGE][ISP_REG_TBL_LENGTH];
	HW_U8		lum_map_cv[PLTM_MAX_STAGE][128];
	HW_U16		gtm_tbl[512];
};

struct isp_tuning_wdr_table_cfg {
	HW_U8		wdr_de_purpl_hsv_tbl[ISP_WDR_TBL_SIZE];
};

/* vencoder parameter */
#define VENCODER_RC_THRD_SIZE 12
#define VENCODER_FILENAME_LEN 256
typedef struct vencoder_common_cfg {
	/* static vencoder param */
	HW_S32 EncodeFormat;
	HW_S32 MaxKeyInterval;
	HW_S32 SrcPicWidth;
	HW_S32 SrcPicHeight;
	HW_S32 InputPixelFormat;
	HW_S32 Rotate;
	HW_S32 DstPicWidth;
	HW_S32 DstPicHeight;
	HW_S32 mBitRate;
	HW_S32 SbmBufSize;
	HW_S32 Level;
	HW_S32 Profile;
	HW_S32 FastEncFlag;
	HW_S32 IQpOffset;
	HW_S32 mbPintraEnable;
	HW_S32 mMaxQp;
	HW_S32 mMixQp;
	HW_S32 mInitQp;
	HW_S32 enGopMode;
	/*RC PARAM*/
	HW_S32 RCEnable;
	/*3DNR*/
	HW_S32 Flag_3DNR;
	HW_S32 ThrdI;
	HW_S32 ThrdP;
	HW_S32 MaxReEncodeTimes;
} vencoder_common_cfg_t;

typedef struct vencoder_vbr_cfg {
	HW_S32 mMaxBitRate;
	HW_S32 mMinBitRate;
} vencoder_vbr_cfg_t;

typedef struct vencoder_avbr_cfg {
	HW_S32 mMaxBitRate;
	HW_S32 mMinBitRate;
	HW_S32 mQuality;
} vencoder_avbr_cfg_t;

typedef struct vencoder_fixqp_cfg {
	HW_S32 mIQp;
	HW_S32 mPQp;
} vencoder_fixqp_cfg_t;

typedef struct vencoder_proc_cfg {
	HW_S32 ProcEnable;
	HW_S32 ProcFreq;
	/*unit is ms*/
	HW_S32 StatisBitRateTime;
} vencoder_proc_cfg_t;

typedef struct vencoder_savebsfile_cfg {
	HW_CHAR Filename[VENCODER_FILENAME_LEN];
	HW_S32 Save_bsfile_flag;
	/*unit is ms*/
	HW_S32 Save_start_time;
	HW_S32 Save_end_time;
} vencoder_savebsfile_cfg_t;

#ifdef ANDROID_VENCODE
typedef struct vencoder_tuning_param_cfg {
	vencoder_common_cfg_t common_cfg; 			/* id: 0x0300100000 */
	vencoder_vbr_cfg_t vbr_cfg;					/* id: 0x0300200000 */
	vencoder_avbr_cfg_t avbr_cfg;				/* id: 0x0300400000 */
	vencoder_fixqp_cfg_t fixqp_cfg;				/* id: 0x0300800000 */
	vencoder_proc_cfg_t proc_cfg;				/* id: 0x0301000000 */
	vencoder_savebsfile_cfg_t savebsfile_cfg;	/* id: 0x0302000000 */
} vencoder_tuning_param_cfg_t;

vencoder_tuning_param_cfg_t *vencoder_tuning_param;
#endif

/* isp_tuning_param cfg */
struct isp_tuning_param_cfg {
	struct isp_tuning_flash_cfg             flash;          /* id: 0x0300000001 */
	struct isp_tuning_flicker_cfg           flicker;        /* id: 0x0300000002 */
	struct isp_tuning_visual_angle_cfg      visual_angle;   /* id: 0x0300000004 */
	struct isp_tuning_gtm_cfg               gtm;            /* id: 0x0300000008 */
	struct isp_tuning_cfa_cfg               cfa;            /* id: 0x0300000010 */
	struct isp_tuning_ctc_cfg               ctc;            /* id: 0x0300000020 */
	struct isp_tuning_blc_gain_cfg          digital_gain;   /* id: 0x0300000040 */
	struct isp_tuning_ccm_cfg               ccm_low;        /* id: 0x0300000080 */
	struct isp_tuning_ccm_cfg               ccm_mid;        /* id: 0x0300000100 */
	struct isp_tuning_ccm_cfg               ccm_high;       /* id: 0x0300000200 */
	struct isp_tuning_pltm_cfg              pltm;           /* id: 0x0300000400 */
	struct isp_tuning_gca_cfg               gca;            /* id: 0x0300000800 */
	struct isp_tuning_bdnf_comm_cfg         bdnf_comm;      /* id: 0x0300001000 */
	struct isp_tuning_tdnf_comm_cfg         tdnf_comm;      /* id: 0x0300002000 */
	struct isp_tuning_sharp_comm_cfg        sharp_comm;     /* id: 0x0300004000 */
	struct isp_tuning_dpc_cfg               dpc;            /* id: 0x0300008000 */
	/* tuning tables */
	struct isp_tuning_lsc_table_cfg         lsc;            /* id: 0x0400000001 */
	struct isp_tuning_gamma_table_cfg       gamma;          /* id: 0x0400000002 */
 	struct isp_tuning_bdnf_table_cfg        bdnf;           /* id: 0x0400000010 */
	struct isp_tuning_tdnf_table_cfg        tdnf;           /* id: 0x0400000020 */
	struct isp_tuning_sharp_table_cfg       sharp;          /* id: 0x0400000080 */
	struct isp_tuning_cem_table_cfg	        cem;            /* id: 0x0400000100 */
	struct isp_tuning_cem_table_cfg         cem_1;          /* id: 0x0400000200 */
	struct isp_tuning_pltm_table_cfg        pltm_tbl;       /* id: 0x0400000400 */
	struct isp_tuning_wdr_table_cfg	        wdr;            /* id: 0x0400000800 */
	struct isp_tuning_msc_table_cfg         msc;            /* id: 0x0400001000 */
	struct isp_tuning_lca_pf_satu_lut       lca_pf;         /* id: 0x0400002000 */
	struct isp_tuning_lca_gf_satu_lut       lca_gf;         /* id: 0x0400002000 */
};

struct isp_dynamic_single_cfg {
	HW_S32		value[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_sharp_ss_item {
	HW_S16		value[ISP_SHARP_LS_NS_LW];
};

struct isp_dynamic_sharp_ls_item {
	HW_S16		value[ISP_SHARP_HFR_SMTH_RATIO - ISP_SHARP_LS_NS_LW];
};

struct isp_dynamic_sharp_hfr_item {
	HW_S16		value[ISP_SHARP_DIR_SMTH0 - ISP_SHARP_HFR_SMTH_RATIO];
};

struct isp_dynamic_sharp_comm_item {
	HW_S16		value[ISP_SHARP_MAX - ISP_SHARP_DIR_SMTH0];
};

struct isp_dynamic_sharp_cfg {
	HW_S16		trigger;
	struct isp_dynamic_sharp_ss_item tuning_ss_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_sharp_ls_item tuning_ls_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_sharp_hfr_item tuning_hfr_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_sharp_comm_item tuning_comm_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_denoise_dnr_item {
	HW_S16		value[ISP_DENOISE_HF_SMTH_RATIO];
	HW_S16		color_denoise;
};

struct isp_dynamic_denoise_dtc_item {
	HW_S16		value[ISP_DENOISE_LYR0_DNR_LM_AMP - ISP_DENOISE_HF_SMTH_RATIO];
};

struct isp_dynamic_denoise_wdr_item {
	HW_S16		value[ISP_DENOISE_MAX - ISP_DENOISE_LYR0_DNR_LM_AMP];
};

struct isp_dynamic_denoise_cfg {
	HW_S16		trigger;
	HW_S16		color_trigger;
	struct isp_dynamic_denoise_dnr_item	tuning_dnr_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_denoise_dtc_item	tuning_dtc_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_denoise_wdr_item	tuning_wdr_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_black_level_item {
	HW_S16		value[ISP_BLC_MAX];
};

struct isp_dynamic_black_level_cfg {
	HW_S16		trigger;
	struct isp_dynamic_black_level_item	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_dpc_item {
	HW_S16		value[ISP_DPC_MAX];
};

struct isp_dynamic_dpc_cfg {
	HW_S16		trigger;
	struct isp_dynamic_dpc_item	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_pltm_item {
	HW_S16		value[ISP_PLTM_DYNAMIC_MAX];
};

struct isp_dynamic_pltm_cfg {
	HW_S16		trigger;
	struct isp_dynamic_pltm_item	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_defog_item {
	HW_S16		value;
};

struct isp_dynamic_defog_cfg {
	HW_S16		trigger;
	struct isp_dynamic_defog_item	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_cem_item {
	HW_S16		value[ISP_CEM_MAX];
};

struct isp_dynamic_cem_cfg {
	HW_S16		trigger;
	struct isp_dynamic_cem_item	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_histogram_item {
	HW_S16		brightness;
	HW_S16		contrast;
};
struct isp_dynamic_histogram_cfg {
	HW_S16		brightness_trigger;
	HW_S16		contrast_trigger;
	struct isp_dynamic_histogram_item	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_tdf_dnr_item {
	HW_S16		value[ISP_TDF_DIFF_INTRA_SENS];
};

struct isp_dynamic_tdf_mtd_item {
	HW_S16		value[ISP_TDF_DTC_HF_COR - ISP_TDF_DIFF_INTRA_SENS];
};

struct isp_dynamic_tdf_dtc_item {
	HW_S16		value[ISP_TDF_D2D0_CNR_STREN - ISP_TDF_DTC_HF_COR];
};

struct isp_dynamic_tdf_srd_item {
	HW_S16		value[ISP_TDF_MAX - ISP_TDF_D2D0_CNR_STREN];
};

struct isp_dynamic_tdf_cfg {
	HW_S16		trigger;
	struct isp_dynamic_tdf_dnr_item	tuning_dnr_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_tdf_mtd_item	tuning_mtd_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_tdf_dtc_item	tuning_dtc_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_tdf_srd_item	tuning_srd_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_ae_item {
	HW_S16		value[ISP_EXP_CFG_MAX];
};

struct isp_dynamic_ae_cfg {
	HW_S16		trigger;
	struct isp_dynamic_ae_item	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_gtm_item {
	HW_S16		value[ISP_GTM_HEQ_MAX];
};

struct isp_dynamic_gtm_cfg {
	HW_S16		trigger;
	struct isp_dynamic_gtm_item	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_lca_item {
	HW_S16		value[ISP_LCA_MAX];
};
struct isp_dynamic_lca_cfg {
	HW_S16		trigger;
	struct isp_dynamic_lca_item		tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_cfa_item {
	HW_S16		value[ISP_CFA_MAX];
};

struct isp_dynamic_cfa_cfg {
	HW_S16		trigger;
	struct isp_dynamic_cfa_item		tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_encpp_sharp_ss_item {
	HW_S16		value[ENCPP_SHARP_LS_NS_LW];
};

struct isp_dynamic_encpp_sharp_ls_item {
	HW_S16		value[ENCPP_SHARP_HFR_SMTH_RATIO - ENCPP_SHARP_LS_NS_LW];
};

struct isp_dynamic_encpp_sharp_hfr_item {
	HW_S16		value[ENCPP_SHARP_DIR_SMTH0 - ENCPP_SHARP_HFR_SMTH_RATIO];
};

struct isp_dynamic_encpp_sharp_comm_item {
	HW_S16		value[ENCPP_SHARP_MAX - ENCPP_SHARP_DIR_SMTH0];
};

struct isp_dynamic_encpp_sharp_cfg {
	HW_S16		trigger;
	struct isp_dynamic_encpp_sharp_ss_item tuning_ss_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_encpp_sharp_ls_item tuning_ls_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_encpp_sharp_hfr_item tuning_hfr_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_encpp_sharp_comm_item tuning_comm_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_encoder_denoise_3dnr_item {
	HW_S16		value[ENCODER_DENOISE_2D_FILT_STREN_UV];
};

struct isp_dynamic_encoder_denoise_2dnr_item {
	HW_S16		value[ENCODER_DENOISE_MAX - ENCODER_DENOISE_2D_FILT_STREN_UV];
};

struct isp_dynamic_encoder_denoise_cfg {
	HW_S16		trigger;
	struct isp_dynamic_encoder_denoise_3dnr_item tuning_3dnr_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_encoder_denoise_2dnr_item tuning_2dnr_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_wdr_lm_item {
	HW_S16		value[WDR_CMP_MS_LTH];
};

struct isp_dynamic_wdr_ms_item {
	HW_S16		value[ISP_WDR_CFG_MAX - WDR_CMP_MS_LTH];
};

struct isp_dynamic_wdr_cfg {
	HW_S16		trigger;
	struct isp_dynamic_wdr_lm_item tuning_lm_cfg[ISP_DYNAMIC_GROUP_COUNT];
	struct isp_dynamic_wdr_ms_item tuning_ms_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_shading_item {
	HW_S16		shading_comp;
};

struct isp_dynamic_shading_cfg {
	HW_S16		trigger;
	struct isp_dynamic_shading_item tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};
/* isp_dynamic_param cfg */
struct isp_dynamic_param_cfg {
	struct isp_dynamic_single_cfg           lum_mapping_point;      /* id: 0x0500000001 */
	struct isp_dynamic_single_cfg           gain_mapping_point;     /* id: 0x0500000002 */
	struct isp_dynamic_sharp_cfg            sharp;                  /* id: 0x0500000004 */
	struct isp_dynamic_denoise_cfg          denoise;                /* id: 0x0500000010 */
	struct isp_dynamic_black_level_cfg      black_level;            /* id: 0x0500000040 */
	struct isp_dynamic_dpc_cfg              dpc;                    /* id: 0x0500000080 */
	struct isp_dynamic_pltm_cfg             pltm;                   /* id: 0x0500000100 */
	struct isp_dynamic_defog_cfg            defog;                  /* id: 0x0500000200 */
	struct isp_dynamic_histogram_cfg        histogram;              /* id: 0x0500000400 */
	struct isp_dynamic_cem_cfg              cem;                    /* id: 0x0500001000 */
	struct isp_dynamic_tdf_cfg              tdf;                    /* id: 0x0500002000 */
	struct isp_dynamic_ae_cfg               ae;                     /* id: 0x0500004000 */
	struct isp_dynamic_gtm_cfg              gtm;                    /* id: 0x0500008000 */
	struct isp_dynamic_lca_cfg              lca;                    /* id: 0x0500010000 */
	struct isp_dynamic_cfa_cfg              cfa;                    /* id: 0x0500020000 */
};

struct isp_params_cfg {
	struct isp_test_param_cfg			isp_test_param;
	struct isp_3a_param_cfg				isp_3a_param;
	struct isp_tuning_param_cfg			isp_tuning_param;
	struct isp_dynamic_param_cfg		isp_dynamic_param;
};

struct isp_ae_log_cfg {
	HW_S32 ae_frame_id;
	HW_U32 ev_sensor_exp_line;
	HW_U32 ev_exposure_time;
	HW_U32 ev_analog_gain;
	HW_U32 ev_digital_gain;
	HW_U32 ev_total_gain;
	HW_U32 ev_idx;
	HW_U32 ev_idx_max;
	HW_U32 ev_lv;
	HW_U32 ev_lv_adj;
	HW_S32 ae_target;
	HW_S32 ae_avg_lum;
	HW_S32 ae_weight_lum;
	HW_S32 ae_delta_exp_idx;
};

struct isp_awb_log_cfg {
	HW_S32 awb_frame_id;
	HW_U16 r_gain;
	HW_U16 gr_gain;
	HW_U16 gb_gain;
	HW_U16 b_gain;
	HW_S32 color_temp_output;
};

struct isp_af_log_cfg {
	HW_S32 af_frame_id;
	HW_U32 last_code_output;
	HW_U32 real_code_output;
	HW_U32 std_code_output;
};

struct isp_iso_log_cfg {
	HW_U32 gain_idx;
	HW_U32 lum_idx;
};

struct isp_pltm_log_cfg {
	HW_U16 pltm_auto_stren;
};

struct isp_log_cfg {
	struct isp_ae_log_cfg ae_log;
	struct isp_awb_log_cfg awb_log;
	struct isp_af_log_cfg af_log;
	struct isp_iso_log_cfg iso_log;
	struct isp_pltm_log_cfg pltm_log;
};
/*
 * get isp cfg
 * @isp: hw isp device
 * @group_id: group id
 * @cfg_ids: cfg ids, supports one or more ids combination by '|'(bit operation)
 * @cfg_data: cfg data to get, data sorted by @cfg_ids
 * @returns: cfg data length, negative if something went wrong
 */
HW_S32 isp_tuning_get_cfg_run(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, struct isp_param_config *params, void *cfg_data);
HW_S32 isp_tuning_get_cfg(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data);

/*
 * set isp cfg
 * @isp: hw isp device
 * @group_id: group id
 * @cfg_id: cfg ids, supports one or more ids combination by '|'(bit operation)
 * @cfg_data: cfg data to set, data sorted by @cfg_ids
 * @returns: cfg data length, negative if something went wrong
 */
HW_S32 isp_tuning_set_cfg_run(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, struct isp_param_config *params, void *cfg_data);
HW_S32 isp_tuning_set_cfg(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data);
int isp_sensor_otp_init(struct hw_isp_device *isp);
int isp_sensor_otp_exit(struct hw_isp_device *isp);
int isp_config_sensor_info(struct hw_isp_device *isp);
int isp_params_parse(struct hw_isp_device *isp, struct isp_param_config *params, HW_U8 ir, int sync_mode);
int isp_cfg_bin_parse(struct hw_isp_device *isp, struct isp_param_config *params, char *isp_cfg_bin_path, int sync_mode);
int isp_tuning_reset(struct hw_isp_device *isp, struct isp_param_config *param);

int isp_tuning_update(struct hw_isp_device *isp);
struct isp_tuning * isp_tuning_init(struct hw_isp_device *isp, const struct isp_param_config *params);
void isp_tuning_exit(struct hw_isp_device *isp);
void isp_ini_tuning_run(struct hw_isp_device *isp);



#endif /* _ISP_TUNING_CFG_H_ */
