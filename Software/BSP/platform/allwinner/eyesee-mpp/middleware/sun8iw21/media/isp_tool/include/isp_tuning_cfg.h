/*
 ******************************************************************************
 * 
 * isp_tuning_cfg.h
 * 
 * Hawkview ISP - isp_tuning_cfg.h module
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

#include "isp_type.h"
#include "isp_comm.h"
#include "isp_tuning.h"
#include "isp_tuning_priv.h"
/*
 * id defination
 * 5 bytes total, first byte is the group id(current 4 groups total), left 4 bytes is the cfg id
 *   |group id|cfg id|cfg id|cfg id|cfg id|
 * group id : 0x1 ~ 0xFF, each number means a individual group id, total 255 group ids
 * cfg id: 0x1 ~ 0xFFFFFFFF, each bit means a individual cfg id, total 32 cfg ids
 */

typedef enum {
	HW_ISP_CFG_GET = 0x1,
	HW_ISP_CFG_SET = 0x2,
	HW_ISP_CFG_QUIT = 0x3     //new

	
} hw_isp_cfg_operation_cmd;

typedef enum {
	HW_ISP_CFG_TEST			= 0x01,
	HW_ISP_CFG_3A			= 0x02,
	HW_ISP_CFG_TUNING		= 0x03,
	HW_ISP_CFG_DYNAMIC		= 0x04,

    HW_ISP_CFG_RAW,       // new
    HW_ISP_CFG_YUV,       // new
    HW_ISP_CFG_JPEG,      // new

    HW_ISP_CFG_ISE,      // new
	HW_ISP_CFG_GROUP_COUNT  
} hw_isp_cfg_groups;

typedef enum {
	/* isp_test_param_cfg */
	HW_ISP_CFG_TEST_PUB		= 0x00000001,
	HW_ISP_CFG_TEST_EXPTIME		= 0x00000002,
	HW_ISP_CFG_TEST_GAIN		= 0x00000004,
	HW_ISP_CFG_TEST_FOCUS		= 0x00000008,
	HW_ISP_CFG_TEST_FORCED		= 0x00000010,
	HW_ISP_CFG_TEST_ENABLE		= 0x00000020,
} hw_isp_cfg_test_ids;

typedef enum {
	/* isp_3a_param_cfg */
	/* ae */
	HW_ISP_CFG_AE_PUB		= 0x00000001,
	HW_ISP_CFG_AE_PREVIEW_TBL	= 0x00000002,
	HW_ISP_CFG_AE_CAPTURE_TBL	= 0x00000004,
	HW_ISP_CFG_AE_VIDEO_TBL		= 0x00000008,
	HW_ISP_CFG_AE_WIN_WEIGHT	= 0x00000010,
	HW_ISP_CFG_AE_DELAY		= 0x00000020,
	/* awb */
	HW_ISP_CFG_AWB_SPEED		= 0x00000040,
	HW_ISP_CFG_AWB_TEMP_RANGE	= 0x00000080,
	HW_ISP_CFG_AWB_LIGHT_INFO	= 0x00000100,
	HW_ISP_CFG_AWB_EXT_LIGHT_INFO	= 0x00000200,
	HW_ISP_CFG_AWB_SKIN_INFO	= 0x00000400,
	HW_ISP_CFG_AWB_SPECIAL_INFO	= 0x00000800,
	HW_ISP_CFG_AWB_PRESET_GAIN	= 0x00001000,
	HW_ISP_CFG_AWB_FAVOR		= 0x00002000,
	/* af */
	HW_ISP_CFG_AF_VCM_CODE		= 0x00004000,
	HW_ISP_CFG_AF_OTP		= 0x00008000,
	HW_ISP_CFG_AF_SPEED		= 0x00010000,
	HW_ISP_CFG_AF_FINE_SEARCH	= 0x00020000,
	HW_ISP_CFG_AF_REFOCUS		= 0x00040000,
	HW_ISP_CFG_AF_TOLERANCE		= 0x00080000,
	HW_ISP_CFG_AF_SCENE		= 0x00100000,
} hw_isp_cfg_3a_ids;

typedef enum {
	/* isp_tuning_param_cfg */
	HW_ISP_CFG_TUNING_FLASH		= 0x00000001,
	HW_ISP_CFG_TUNING_FLICKER	= 0x00000002,
	HW_ISP_CFG_TUNING_DEFOG		= 0x00000004,
	HW_ISP_CFG_TUNING_VISUAL_ANGLE	= 0x00000008,
	HW_ISP_CFG_TUNING_GTM		= 0x00000010,
	HW_ISP_CFG_TUNING_DPC_OTF	= 0x00000020,
	HW_ISP_CFG_TUNING_GAIN_OFFSET	= 0x00000040,
	HW_ISP_CFG_TUNING_CCM_LOW	= 0x00000080,
	HW_ISP_CFG_TUNING_CCM_MID	= 0x00000100,
	HW_ISP_CFG_TUNING_CCM_HIGH	= 0x00000200,
	HW_ISP_CFG_TUNING_LSC		= 0x00000400,
	HW_ISP_CFG_TUNING_GAMMA		= 0x00000800,
	HW_ISP_CFG_TUNING_LINEARITY	= 0x00001000,
	HW_ISP_CFG_TUNING_DISTORTION	= 0x00002000,
} hw_isp_cfg_tuning_ids;

typedef enum {
	/* isp_dynamic_cfg */
	HW_ISP_CFG_DYNAMIC_TRIGGER	= 0x00000001,
	HW_ISP_CFG_DYNAMIC_LUM_POINT	= 0x00000002,
	HW_ISP_CFG_DYNAMIC_GAIN_POINT	= 0x00000004,
	HW_ISP_CFG_DYNAMIC_SHARP	= 0x00000008,
	HW_ISP_CFG_DYNAMIC_CONTRAST	= 0x00000010,
	HW_ISP_CFG_DYNAMIC_DENOISE	= 0x00000020,
	HW_ISP_CFG_DYNAMIC_BRIGHTNESS	= 0x00000040,
	HW_ISP_CFG_DYNAMIC_SATURATION	= 0x00000080,
	HW_ISP_CFG_DYNAMIC_TDF		= 0x00000100,
	HW_ISP_CFG_DYNAMIC_AE		= 0x00000200,
	HW_ISP_CFG_DYNAMIC_GTM		= 0x00000400,
	HW_ISP_CFG_DYNAMIC_RESERVED	= 0x00000800,
} hw_isp_cfg_dynamic_ids;


struct isp_test_pub_cfg {
	HW_S32		test_mode;
	HW_S32		gain;
	HW_S32		exp_line;
	HW_S32		color_temp;
	HW_S32		log_param;
};

struct isp_test_forced_cfg {
	HW_S32		ae_enable;
	HW_S32		lum;
};

struct isp_test_item_cfg {
	HW_S32		enable;
	HW_S32		start;
	HW_S32		step;
	HW_S32		end;
	HW_S32		change_interval;
};

struct isp_test_enable_cfg {
	HW_S32		manual;
	HW_S32		afs;
	HW_S32		sharp;
	HW_S32		pri_contrast;
	HW_S32		contrast;
	HW_S32		denoise;
	HW_S32		drc;
	HW_S32		lut_dpc;
	HW_S32		lsc;
	HW_S32		gamma;
	HW_S32		cm;
	HW_S32		ae;
	HW_S32		af;
	HW_S32		awb;
	HW_S32		hist;
	HW_S32		gain_offset;
	HW_S32		wb;
	HW_S32		otf_dpc;
	HW_S32		cfa;
	HW_S32		sprite;
	HW_S32		tdf;
	HW_S32		cnr;
	HW_S32		saturation;
	HW_S32		defog;
	HW_S32		linearity;
	HW_S32		distortion;
	HW_S32		hdr_gamma;
	HW_S32		gtm;
	HW_S32		dig_gain;
	HW_S32		brightness;
};

/* isp_test_param cfg */
struct isp_test_param_cfg {
	struct isp_test_pub_cfg		isp_test_pub;		/* id: 0x0100000001 */
	struct isp_test_item_cfg	isp_test_exptime;	/* id: 0x0100000002 */
	struct isp_test_item_cfg	isp_test_gain;		/* id: 0x0100000004 */
	struct isp_test_item_cfg	isp_test_focus;		/* id: 0x0100000008 */
	struct isp_test_forced_cfg	isp_test_forced;	/* id: 0x0100000010 */
	struct isp_test_enable_cfg	isp_test_enable;	/* id: 0x0100000020 */
};

struct isp_ae_pub_cfg {
	HW_S32		define_table;
	HW_S32		max_lv;
	HW_S32		aperture; /* fno  */
	HW_S32		hist_mode_en;
	HW_S32		compensation_step;
	HW_S32		touch_dist_index;
};

struct isp_ae_table_cfg {
	HW_S32		length;
	struct ae_table	value[7];
};

struct isp_ae_weight_cfg {
	HW_S32		weight[64];
};

struct isp_ae_delay_cfg {
	HW_S32		exp_frame;
	HW_S32		gain_frame;
};

struct isp_awb_speed_cfg {
	HW_S32		interval;
	HW_S32		value;
};

struct isp_awb_temp_range_cfg {
	HW_S32		low;
	HW_S32		high;
	HW_S32		base;
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
	struct isp_ae_pub_cfg		isp_ae_pub;		/* id: 0x0200000001 */
	struct isp_ae_table_cfg		isp_ae_preview_tbl;	/* id: 0x0200000002 */
	struct isp_ae_table_cfg		isp_ae_capture_tbl;	/* id: 0x0200000004 */
	struct isp_ae_table_cfg		isp_ae_video_tbl;	/* id: 0x0200000008 */
	struct isp_ae_weight_cfg	isp_ae_win_weight;	/* id: 0x0200000010 */
	struct isp_ae_delay_cfg		isp_ae_delay;		/* id: 0x0200000020 */
	/* awb */
	struct isp_awb_speed_cfg	isp_awb_speed;		/* id: 0x0200000040 */
	struct isp_awb_temp_range_cfg	isp_awb_temp_range;	/* id: 0x0200000080 */
	struct isp_awb_temp_info_cfg	isp_awb_light_info;	/* id: 0x0200000100 */
	struct isp_awb_temp_info_cfg	isp_awb_ext_light_info;	/* id: 0x0200000200 */
	struct isp_awb_temp_info_cfg	isp_awb_skin_info;	/* id: 0x0200000400 */
	struct isp_awb_temp_info_cfg	isp_awb_special_info;	/* id: 0x0200000800 */
	struct isp_awb_preset_gain_cfg	isp_awb_preset_gain;	/* id: 0x0200001000 */
	struct isp_awb_favor_cfg	isp_awb_favor;		/* id: 0x0200002000 */
	/* af */
	struct isp_af_vcm_code_cfg	isp_af_vcm_code;	/* id: 0x0200004000 */
	struct isp_af_otp_cfg		isp_af_otp;		/* id: 0x0200008000 */
	struct isp_af_speed_cfg		isp_af_speed;		/* id: 0x0200010000 */
	struct isp_af_fine_search_cfg	isp_af_fine_search;	/* id: 0x0200020000 */
	struct isp_af_refocus_cfg	isp_af_refocus;		/* id: 0x0200040000 */
	struct isp_af_tolerance_cfg	isp_af_tolerance;	/* id: 0x0200080000 */
	struct isp_af_scene_cfg		isp_af_scene;		/* id: 0x0200100000 */
};

struct isp_flash_cfg {
	HW_S32		gain;
	HW_S32		delay_frame;
};

struct isp_flicker_cfg {
	HW_S32		type;
	HW_S32		ratio;
};

struct isp_defog_cfg {
	HW_S32		strength;
};

struct isp_visual_angle_cfg {
	HW_S32		horizontal;
	HW_S32		vertical;
	HW_S32		focus_length;
};

struct isp_gain_offset_cfg {
	HW_S32		gain[4];
	HW_S32		offset[4];
};

struct isp_ccm_cfg {
	HW_U16				temperature;
	struct isp_rgb2rgb_gain_offset	value;
};


/* global tone mapping */
struct isp_gtm_cfg {
	HW_S32		type;
	HW_S32		gamma_type;
	HW_S32		auto_alpha_en;
};

struct isp_dpc_otf_cfg {
	HW_S32		thres_slop;
	HW_S32		min_thres;
	HW_S32		max_thres;
	HW_S32		cfa_dir_thres;
};


struct isp_gamma_table_cfg {
	HW_S32		number;
	HW_U16		value[5][256];
	HW_U16		lv_triggers[5];
};

struct isp_lens_shading_cfg {
	HW_S32		ff_mod;
	HW_S32		center_x;
	HW_S32		center_y;
	HW_U16		value[12][768];
	HW_U16		color_temp_triggers[6];
};

struct isp_linearity_cfg {
	HW_U16		value[768];
};

struct isp_distortion_cfg {
	HW_U16		value[512];
};

/* isp_tuning_param cfg */
struct isp_tuning_param_cfg {
	struct isp_flash_cfg		isp_flash;		/* id: 0x0300000001 */
	struct isp_flicker_cfg		isp_flicker;		/* id: 0x0300000002 */
	struct isp_defog_cfg            isp_defog;              /* id: 0x0300000004 */
	struct isp_visual_angle_cfg 	isp_visual_angle;	/* id: 0x0300000008 */
	struct isp_gtm_cfg		isp_gtm;		/* id: 0x0300000010 */
	struct isp_dpc_otf_cfg		isp_dpc_otf;		/* id: 0x0300000020 */
	struct isp_gain_offset_cfg	isp_gain_offset;	/* id: 0x0300000040 */
	struct isp_ccm_cfg		isp_ccm_low;		/* id: 0x0300000080 */
	struct isp_ccm_cfg		isp_ccm_mid;		/* id: 0x0300000100 */
	struct isp_ccm_cfg		isp_ccm_high;		/* id: 0x0300000200 */
	struct isp_lens_shading_cfg	isp_lsc;		/* id: 0x0300000400 */
	struct isp_gamma_table_cfg	isp_gamma;		/* id: 0x0300000800 */
	struct isp_linearity_cfg	isp_linearity;		/* id: 0x0300001000 */
	struct isp_distortion_cfg	isp_distortion;		/* id: 0x0300002000 */
};

#define ISP_DYNAMIC_GROUP_COUNT 	8

struct isp_dynamic_single_cfg {
	HW_S32		value[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_range_cfg {
	HW_S32		lower;
	HW_S32		upper;
	HW_S32		strength;
};

struct isp_dynamic_sharp_cfg {
	struct isp_dynamic_range_cfg	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_contrast_cfg {
	struct isp_dynamic_range_cfg	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
	HW_S32				strength[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_denoise_cfg {
	struct isp_dynamic_range_cfg	tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
	HW_S32				color_denoise[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_saturation_tuning_cfg {
	HW_S32		cb;
	HW_S32		cr;
	HW_S32		value[4];
};

struct isp_dynamic_saturation_cfg {
	struct isp_dynamic_saturation_tuning_cfg value[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_tdf_cfg {
	HW_S32		value[ISP_DYNAMIC_GROUP_COUNT][4];
};

struct isp_dynamic_gtm_cfg {
	HW_S32		value[ISP_DYNAMIC_GROUP_COUNT][9];
};

struct isp_dynamic_ae_tuning_cfg {
	HW_S32		anti_win_over;
	HW_S32		anti_win_under;
	HW_S32		anti_hist_over;
	HW_S32		anti_hist_under;
	HW_S32		preview_speed;
	HW_S32		capture_speed;
	HW_S32		video_speed;
	HW_S32		touch_speed;
	HW_S32		tolerance;
	HW_S32		target;
	HW_S32		hist_cfg[4];
};

struct isp_dynamic_ae_cfg {
	struct isp_dynamic_ae_tuning_cfg tuning_cfg[ISP_DYNAMIC_GROUP_COUNT];
};

struct isp_dynamic_reserved {
	HW_S32		value[ISP_DYNAMIC_GROUP_COUNT][19];
};

/* isp_dynamic_param cfg */
struct isp_dynamic_param_cfg {
	isp_dynamic_triger_t	isp_trigger;		        /* id: 0x0400000001 */
	struct isp_dynamic_single_cfg	isp_lum_mapping_point;	/* id: 0x0400000002 */
	struct isp_dynamic_single_cfg	isp_gain_mapping_point;	/* id: 0x0400000004 */
	struct isp_dynamic_sharp_cfg	isp_sharp_cfg;		/* id: 0x0400000008 */
	struct isp_dynamic_contrast_cfg	isp_contrast_cfg;	/* id: 0x0400000010 */
	struct isp_dynamic_denoise_cfg	isp_denoise_cfg;	/* id: 0x0400000020 */
	struct isp_dynamic_single_cfg	isp_brightness;		/* id: 0x0400000040 */
	struct isp_dynamic_saturation_cfg	isp_saturation_cfg;	/* id: 0x0400000080 */
	struct isp_dynamic_tdf_cfg	isp_tdf_cfg;		/* id: 0x0400000100 */
	struct isp_dynamic_ae_cfg	isp_ae_cfg;		/* id: 0x0400000200 */
	struct isp_dynamic_gtm_cfg	isp_gtm_cfg;		/* id: 0x0400000400 */
	struct isp_dynamic_reserved	isp_reserved;		/* id: 0x0400000800 */
};


#endif /* _ISP_TUNING_H_ */
