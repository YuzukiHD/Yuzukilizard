
/*
 ******************************************************************************
 *
 * isp_ini_parse.c
 *
 * Hawkview ISP - isp_ini_parse.c module
 *
 * Copyright (c) 2015 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2015/11/23	ISP Tuning Tools Support
 *
 ******************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../iniparser/src/iniparser.h"
#include "../include/isp_ini_parse.h"
#include "../include/isp_manage.h"
#include "../include/isp_debug.h"
#include "../isp_dev/tools.h"

#ifdef SENSOR_GC2053
#include "SENSOR_H/gc2053/gc2053_mipi_isp600_20220511_164617_vlc4_day.h"
#include "SENSOR_H/gc2053/gc2053_mipi_isp600_20220415_144837_ir.h"
#endif

#ifdef SENSOR_GC4663
#include "SENSOR_H/gc4663/gc4663_mipi_isp600_20220706_155211_day.h"
#include "SENSOR_H/gc4663/gc4663_mipi_isp600_20220118_171111_ir.h"
#include "SENSOR_H/gc4663/gc4663_mipi_wdr_default_v853.h"
#endif

#ifdef SENSOR_SC4336
#include "SENSOR_H/sc4336/sc4336_mipi_isp600_20220718_170000_day.h"
#endif

#define ISP_TUNING_ENABLE 1
#define ISP_LIB_USE_INIPARSER 0
#define EXTERNEL_COLOR_ISP_CFG_PATH "/data/isp_test/color/"
#define EXTERNEL_IR_ISP_CFG_PATH "/data/isp_test/ir/"

unsigned int isp_cfg_log_param = ISP_LOG_CFG;

#define SIZE_OF_LSC_TBL     (12*768*2)
#define SIZE_OF_GAMMA_TBL   (5*1024*3*2)

#if ISP_LIB_USE_INIPARSER

#define SET_ISP_SINGLE_VALUE(struct_name, key) \
void set_##key(struct isp_param_config *isp_ini_cfg, void *value, int len) \
{\
	isp_ini_cfg->struct_name.key = *(int *)value;\
}

#define SET_ISP_ARRAY_VALUE(struct_name, key) \
void set_##key(struct isp_param_config *isp_ini_cfg, void *value, int len) \
{\
	int i, *tmp = (int *)value;\
	for (i = 0; i < len; i++) \
		isp_ini_cfg->struct_name.key[i] = tmp[i];\
}

#define SET_ISP_ARRAY_VALUE_2(struct_name, key, row, col) \
void set_##key(struct isp_param_config *isp_ini_cfg, void *value, int len) \
{\
	int i, j, *tmp = (int *)value;\
	if (row * col != len)\
		return;\
	for (i = 0; i < row; i++) {\
		for (j = 0; j < col; j++) {\
			isp_ini_cfg->struct_name.key[i][j] = tmp[i*col+j];\
		}\
	}\
}

#define SET_ISP_ARRAY_INT(struct_name, key) \
void set_##key(struct isp_param_config *isp_ini_cfg, void *value, int len)\
{\
	memcpy(&isp_ini_cfg->struct_name.key[0], value, 4*len);\
}

#define SET_ISP_STRUCT_INT(struct_name, key) \
void set_##key(struct isp_param_config *isp_ini_cfg, void *value, int len)\
{\
	memcpy(&isp_ini_cfg->struct_name.key, value, 4*len);\
}
#if 0
#define SET_ISP_STRUCT_ARRAY_INT(struct_name, key, idx) \
void set_##key##_##idx(struct isp_param_config *isp_ini_cfg, void *value, int len)\
{\
	memcpy(&isp_ini_cfg->struct_name.key[idx], value, 4*len);\
}
#endif
#define SET_ISP_STRUCT_ARRAY_SHORT(struct_name, key, idx) \
void set_##key##_##idx(struct isp_param_config *isp_ini_cfg, void *value, int len)\
{\
	int i, *src = (int *)value;\
	short *dst = (short *)&isp_ini_cfg->struct_name.key[idx];\
	for (i = 0; i < len; i++) {\
		dst[i] = src[i];\
	}\
}

#define SET_ISP_CM_VALUE(key, idx) \
void set_##key##idx(struct isp_param_config *isp_ini_cfg, void *value, int len)\
{\
	int *tmp = (int *)value;\
	struct isp_rgb2rgb_gain_offset *color_matrix = &isp_ini_cfg->isp_tunning_settings.color_matrix_ini[idx];\
	color_matrix->matrix[0][0] = tmp[0];\
	color_matrix->matrix[0][1] = tmp[1];\
	color_matrix->matrix[0][2] = tmp[2];\
	color_matrix->matrix[1][0] = tmp[3];\
	color_matrix->matrix[1][1] = tmp[4];\
	color_matrix->matrix[1][2] = tmp[5];\
	color_matrix->matrix[2][0] = tmp[6];\
	color_matrix->matrix[2][1] = tmp[7];\
	color_matrix->matrix[2][2] = tmp[8];\
	color_matrix->offset[0] = tmp[9];\
	color_matrix->offset[1] = tmp[10];\
	color_matrix->offset[2] = tmp[11];\
}

#define ISP_FILE_SINGLE_ATTR(main_key, sub_key)\
{\
	#main_key,  #sub_key, 1,  set_##sub_key,\
}

#define ISP_FILE_ARRAY_ATTR(main_key, sub_key , len)\
{\
	#main_key, #sub_key, len,  set_##sub_key,\
}

#define ISP_FILE_STRUCT_ARRAY_ATTR(main_key, sub_key , len)\
{\
	#main_key, #sub_key, len,  set_##main_key,\
}

SET_ISP_SINGLE_VALUE(isp_test_settings, isp_test_mode);
SET_ISP_SINGLE_VALUE(isp_test_settings, isp_test_exptime);
SET_ISP_SINGLE_VALUE(isp_test_settings, exp_line_start);
SET_ISP_SINGLE_VALUE(isp_test_settings, exp_line_step);
SET_ISP_SINGLE_VALUE(isp_test_settings, exp_line_end);
SET_ISP_SINGLE_VALUE(isp_test_settings, exp_change_interval);
SET_ISP_SINGLE_VALUE(isp_test_settings, isp_test_gain);
SET_ISP_SINGLE_VALUE(isp_test_settings, gain_start);
SET_ISP_SINGLE_VALUE(isp_test_settings, gain_step);
SET_ISP_SINGLE_VALUE(isp_test_settings, gain_end);
SET_ISP_SINGLE_VALUE(isp_test_settings, gain_change_interval);

SET_ISP_SINGLE_VALUE(isp_test_settings, isp_test_focus);
SET_ISP_SINGLE_VALUE(isp_test_settings, focus_start);
SET_ISP_SINGLE_VALUE(isp_test_settings, focus_step);
SET_ISP_SINGLE_VALUE(isp_test_settings, focus_end);
SET_ISP_SINGLE_VALUE(isp_test_settings, focus_change_interval);
SET_ISP_SINGLE_VALUE(isp_test_settings, isp_log_param);
SET_ISP_SINGLE_VALUE(isp_test_settings, isp_gain);
SET_ISP_SINGLE_VALUE(isp_test_settings, isp_exp_line);
SET_ISP_SINGLE_VALUE(isp_test_settings, isp_color_temp);
SET_ISP_SINGLE_VALUE(isp_test_settings, ae_forced);
SET_ISP_SINGLE_VALUE(isp_test_settings, lum_forced);

SET_ISP_SINGLE_VALUE(isp_test_settings, manual_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, afs_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, ae_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, af_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, awb_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, hist_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, wdr_split_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, wdr_stitch_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, otf_dpc_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, ctc_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, gca_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, nrp_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, denoise_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, tdf_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, blc_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, wb_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, dig_gain_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, lsc_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, msc_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, pltm_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, cfa_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, lca_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, sharp_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, ccm_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, defog_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, cnr_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, drc_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, gtm_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, gamma_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, cem_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, encpp_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, enc_3dnr_en);
SET_ISP_SINGLE_VALUE(isp_test_settings, enc_2dnr_en);

SET_ISP_SINGLE_VALUE(isp_3a_settings, define_ae_table);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_max_lv);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_table_preview_length);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_table_capture_length);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_table_video_length);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_gain_favor);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_hist_mod_en);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_hist0_sel);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_hist1_sel);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_stat_sel);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_ev_step);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_ConvDataIndex);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_blowout_pre_en);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_blowout_attr);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_delay_frame);
SET_ISP_SINGLE_VALUE(isp_3a_settings, exp_delay_frame);
SET_ISP_SINGLE_VALUE(isp_3a_settings, gain_delay_frame);
SET_ISP_SINGLE_VALUE(isp_3a_settings, exp_comp_step);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_touch_dist_ind);
SET_ISP_SINGLE_VALUE(isp_3a_settings, ae_iso2gain_ratio);

SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_interval);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_speed);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_stat_sel);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_color_temper_low);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_color_temper_high);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_base_temper);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_green_zone_dist);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_blue_sky_dist);

SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_rgain_favor);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_bgain_favor);

SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_light_num);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_ext_light_num);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_skin_color_num);
SET_ISP_SINGLE_VALUE(isp_3a_settings, awb_special_color_num);

SET_ISP_SINGLE_VALUE(isp_3a_settings, af_use_otp);
SET_ISP_SINGLE_VALUE(isp_3a_settings, vcm_min_code);
SET_ISP_SINGLE_VALUE(isp_3a_settings, vcm_max_code);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_interval_time);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_speed_ind);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_auto_fine_en);

SET_ISP_SINGLE_VALUE(isp_3a_settings, af_single_fine_en);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_fine_step);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_move_cnt);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_still_cnt);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_move_monitor_cnt);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_still_monitor_cnt);

SET_ISP_SINGLE_VALUE(isp_3a_settings, af_stable_min);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_stable_max);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_low_light_lv);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_near_tolerance);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_far_tolerance);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_tolerance_off);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_peak_th);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_dir_th);

SET_ISP_SINGLE_VALUE(isp_3a_settings, af_change_ratio);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_move_minus);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_still_minus);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_scene_motion_th);
SET_ISP_SINGLE_VALUE(isp_3a_settings, af_tolerance_tbl_len);

SET_ISP_SINGLE_VALUE(isp_tunning_settings, flash_gain);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, flash_delay_frame);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, flicker_type);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, flicker_ratio);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, hor_visual_angle);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, ver_visual_angle);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, focus_length);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, gamma_num);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, rolloff_ratio);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, gtm_hist_sel);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, gtm_type);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, gamma_type);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, auto_alpha_en);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, hist_pix_cnt);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, dark_minval);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, bright_minval);

SET_ISP_SINGLE_VALUE(isp_tunning_settings, grad_th);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, dir_v_th);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, dir_h_th);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, res_smth_high);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, res_smth_low);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, res_high_th);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, res_low_th);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, res_dir_a);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, res_dir_d);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, res_dir_v);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, res_dir_h);

SET_ISP_SINGLE_VALUE(isp_tunning_settings, dpc_remove_mode);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, dpc_sup_twinkle_en);

SET_ISP_SINGLE_VALUE(isp_tunning_settings, ctc_th_max);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, ctc_th_min);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, ctc_th_slope);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, ctc_dir_wt);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, ctc_dir_th);

SET_ISP_SINGLE_VALUE(isp_tunning_settings, ff_mod);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, lsc_mode);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, lsc_center_x);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, lsc_center_y);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, mff_mod);
SET_ISP_SINGLE_VALUE(isp_tunning_settings, msc_mode);

SET_ISP_ARRAY_VALUE(isp_3a_settings, ae_table_preview);
SET_ISP_ARRAY_VALUE(isp_3a_settings, ae_table_capture);
SET_ISP_ARRAY_VALUE(isp_3a_settings, ae_table_video);
SET_ISP_ARRAY_VALUE(isp_3a_settings, ae_win_weight);
SET_ISP_ARRAY_VALUE(isp_3a_settings, ae_fno_step);
SET_ISP_ARRAY_VALUE(isp_3a_settings, wdr_split_cfg);
SET_ISP_ARRAY_VALUE(isp_3a_settings, wdr_comm_cfg);
SET_ISP_ARRAY_VALUE(isp_3a_settings, awb_light_info);
SET_ISP_ARRAY_VALUE(isp_3a_settings, awb_ext_light_info);
SET_ISP_ARRAY_VALUE(isp_3a_settings, awb_skin_color_info);
SET_ISP_ARRAY_VALUE(isp_3a_settings, awb_special_color_info);
SET_ISP_ARRAY_VALUE(isp_3a_settings, awb_preset_gain);
SET_ISP_ARRAY_VALUE(isp_3a_settings, af_std_code_tbl);
SET_ISP_ARRAY_VALUE(isp_3a_settings, af_tolerance_value_tbl);

SET_ISP_ARRAY_VALUE_2(isp_tunning_settings, plum_var, GTM_LUM_IDX_NUM, GTM_VAR_IDX_NUM);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, bayer_gain);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, lsc_trig_cfg);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, msc_trig_cfg);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, msc_blw_lut);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, msc_blh_lut);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, gamma_trig_cfg);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, ccm_trig_cfg);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, gca_cfg);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, pltm_cfg);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, sharp_comm_cfg);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, denoise_comm_cfg);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, tdf_comm_cfg);
#ifdef USE_ENCPP
SET_ISP_ARRAY_VALUE(isp_tunning_settings, encpp_sharp_comm_cfg);
#endif
SET_ISP_ARRAY_VALUE(isp_tunning_settings, sensor_temp);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_df_shape);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_ratio_amp);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_k_dlt_bk);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_ct_rt_bk);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_dtc_hf_bk);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_dtc_mf_bk);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_lay0_d2d0_rt_br);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_lay1_d2d0_rt_br);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_lay0_nrd_rt_br);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_tdnf_lay1_nrd_rt_br);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, lca_pf_satu_lut);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, lca_gf_satu_lut);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_sharp_ss_value);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_sharp_ls_value);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_sharp_hsv);
#ifdef USE_ENCPP
SET_ISP_ARRAY_VALUE(isp_tunning_settings, encpp_sharp_ss_value);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, encpp_sharp_ls_value);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, encpp_sharp_hsv);
#endif
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_wdr_de_purpl_hsv_tbl);
SET_ISP_ARRAY_VALUE_2(isp_tunning_settings, isp_pltm_stat_gd_cv, PLTM_MAX_STAGE, 15);
SET_ISP_ARRAY_VALUE_2(isp_tunning_settings, isp_pltm_df_cv, PLTM_MAX_STAGE, ISP_REG_TBL_LENGTH);
SET_ISP_ARRAY_VALUE_2(isp_tunning_settings, isp_pltm_lum_map_cv, PLTM_MAX_STAGE, 128);
SET_ISP_ARRAY_VALUE(isp_tunning_settings, isp_pltm_gtm_tbl);

SET_ISP_STRUCT_INT(isp_iso_settings, triger);
SET_ISP_ARRAY_INT(isp_iso_settings, isp_lum_mapping_point);
SET_ISP_ARRAY_INT(isp_iso_settings, isp_gain_mapping_point);

SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 0);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 1);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 2);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 3);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 4);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 5);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 6);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 7);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 8);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 9);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 10);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 11);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 12);
SET_ISP_STRUCT_ARRAY_SHORT(isp_iso_settings, isp_dynamic_cfg, 13);

SET_ISP_CM_VALUE(isp_color_matrix, 0);
SET_ISP_CM_VALUE(isp_color_matrix, 1);
SET_ISP_CM_VALUE(isp_color_matrix, 2);

struct IspParamAttribute {
	char *main;
	char *sub;
	int len;
	void (*set_param) (struct isp_param_config *, void *, int len);
};

struct FileAttribute {
	char *file_name;
	int param_len;
	struct IspParamAttribute *pIspParam;
};

static struct IspParamAttribute IspTestParam[] = {

	ISP_FILE_SINGLE_ATTR(isp_test_cfg, isp_test_mode),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, isp_test_exptime),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, exp_line_start),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, exp_line_step),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, exp_line_end),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, exp_change_interval),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, isp_test_gain),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, gain_start),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, gain_step),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, gain_end),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, gain_change_interval),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, isp_test_focus),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, focus_start),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, focus_step),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, focus_end),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, focus_change_interval),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, isp_log_param),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, isp_gain),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, isp_exp_line),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, isp_color_temp),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, ae_forced),
	ISP_FILE_SINGLE_ATTR(isp_test_cfg, lum_forced),

	ISP_FILE_SINGLE_ATTR(isp_en_cfg, manual_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, afs_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, ae_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, af_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, awb_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, hist_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, wdr_split_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, wdr_stitch_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, otf_dpc_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, ctc_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, gca_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, nrp_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, denoise_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, tdf_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, blc_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, wb_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, dig_gain_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, lsc_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, msc_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, pltm_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, cfa_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, lca_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, sharp_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, ccm_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, defog_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, cnr_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, drc_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, gtm_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, gamma_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, cem_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, encpp_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, enc_3dnr_en),
	ISP_FILE_SINGLE_ATTR(isp_en_cfg, enc_2dnr_en),
};

static struct IspParamAttribute Isp3aParam[] = {
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, define_ae_table),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_max_lv),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_table_preview_length),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_table_capture_length),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_table_video_length),

	ISP_FILE_ARRAY_ATTR(isp_ae_cfg, ae_table_preview, 42),
	ISP_FILE_ARRAY_ATTR(isp_ae_cfg, ae_table_capture, 42),
	ISP_FILE_ARRAY_ATTR(isp_ae_cfg, ae_table_video, 42),
	ISP_FILE_ARRAY_ATTR(isp_ae_cfg, ae_win_weight, 64),
	ISP_FILE_ARRAY_ATTR(isp_ae_cfg, ae_fno_step, 16),

	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_hist_mod_en),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_hist0_sel),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_hist1_sel),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_stat_sel),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_ev_step),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_ConvDataIndex),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_blowout_pre_en),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_blowout_attr),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_delay_frame),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, exp_delay_frame),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, gain_delay_frame),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, exp_comp_step),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_touch_dist_ind),
	ISP_FILE_SINGLE_ATTR(isp_ae_cfg, ae_iso2gain_ratio),
	ISP_FILE_ARRAY_ATTR(isp_ae_cfg, wdr_split_cfg, ISP_WDR_SPLIT_CFG_MAX),
	ISP_FILE_ARRAY_ATTR(isp_ae_cfg, wdr_comm_cfg, ISP_WDR_COMM_CFG_MAX),

	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_interval),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_speed),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_stat_sel),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_color_temper_low),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_color_temper_high),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_base_temper),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_green_zone_dist),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_blue_sky_dist),

	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_light_num),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_ext_light_num),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_skin_color_num),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_special_color_num),

	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_rgain_favor),
	ISP_FILE_SINGLE_ATTR(isp_awb_cfg, awb_bgain_favor),

	ISP_FILE_ARRAY_ATTR(isp_awb_cfg, awb_light_info, 320),
	ISP_FILE_ARRAY_ATTR(isp_awb_cfg, awb_ext_light_info, 320),
	ISP_FILE_ARRAY_ATTR(isp_awb_cfg, awb_skin_color_info, 160),
	ISP_FILE_ARRAY_ATTR(isp_awb_cfg, awb_special_color_info, 320),
	ISP_FILE_ARRAY_ATTR(isp_awb_cfg, awb_preset_gain, 22),

	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_use_otp),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, vcm_min_code),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, vcm_max_code),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_interval_time),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_speed_ind),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_auto_fine_en),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_single_fine_en),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_fine_step),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_move_cnt),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_still_cnt),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_move_monitor_cnt),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_still_monitor_cnt),

	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_stable_min),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_stable_max),

	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_low_light_lv),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_near_tolerance),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_far_tolerance),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_tolerance_off),

	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_peak_th),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_dir_th),

	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_change_ratio),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_move_minus),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_still_minus),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_scene_motion_th),
	ISP_FILE_SINGLE_ATTR(isp_af_cfg, af_tolerance_tbl_len),

	ISP_FILE_ARRAY_ATTR(isp_af_cfg, af_std_code_tbl, 20),
	ISP_FILE_ARRAY_ATTR(isp_af_cfg, af_tolerance_value_tbl, 20),

};

static struct IspParamAttribute IspDynamicParam[] = {
#ifdef USE_ENCPP
	ISP_FILE_ARRAY_ATTR(isp_dynamic_gbl, triger, 19),
#else
	ISP_FILE_ARRAY_ATTR(isp_dynamic_gbl, triger, 17),
#endif
	ISP_FILE_ARRAY_ATTR(isp_dynamic_gbl, isp_lum_mapping_point, 14),
	ISP_FILE_ARRAY_ATTR(isp_dynamic_gbl, isp_gain_mapping_point, 14),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_0, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_1, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_2, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_3, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_4, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_5, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_6, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_7, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_8, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_9, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_10, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_11, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_12, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_dynamic_cfg_13, iso_param, sizeof(struct isp_dynamic_config) / sizeof(HW_S16)),
};

static struct IspParamAttribute IspTuningParam[] = {
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, flash_gain),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, flash_delay_frame),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, flicker_type),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, flicker_ratio),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, hor_visual_angle),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, ver_visual_angle),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, focus_length),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, gamma_num),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, rolloff_ratio),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, gtm_hist_sel),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, gtm_type),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, gamma_type),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, auto_alpha_en),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, hist_pix_cnt),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, dark_minval),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, bright_minval),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, plum_var, GTM_LUM_IDX_NUM * GTM_VAR_IDX_NUM),//GTM_LUM_IDX_NUM = GTM_VAR_IDX_NUM

	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, grad_th),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, dir_v_th),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, dir_h_th),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, res_smth_high),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, res_smth_low),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, res_high_th),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, res_low_th),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, res_dir_a),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, res_dir_d),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, res_dir_v),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, res_dir_h),

	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, dpc_remove_mode),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, dpc_sup_twinkle_en),

	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, ctc_th_max),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, ctc_th_min),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, ctc_th_slope),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, ctc_dir_wt),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, ctc_dir_th),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, bayer_gain, ISP_RAW_CH_MAX),

	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, ff_mod),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, lsc_mode),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, lsc_center_x),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, lsc_center_y),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, lsc_trig_cfg, ISP_LSC_TEMP_NUM),

	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, mff_mod),
	ISP_FILE_SINGLE_ATTR(isp_tuning_cfg, msc_mode),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, msc_trig_cfg, ISP_MSC_TEMP_NUM),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, msc_blw_lut, ISP_MSC_TBL_LUT_SIZE),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, msc_blh_lut, ISP_MSC_TBL_LUT_SIZE),

	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, gamma_trig_cfg, ISP_GAMMA_TRIGGER_POINTS),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, ccm_trig_cfg, ISP_CCM_TEMP_NUM),

	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, gca_cfg, ISP_GCA_MAX),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, pltm_cfg, ISP_PLTM_MAX),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, sharp_comm_cfg, ISP_SHARP_COMM_MAX),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, denoise_comm_cfg, ISP_DENOISE_COMM_MAX),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, tdf_comm_cfg, ISP_TDF_COMM_MAX),
#ifdef USE_ENCPP
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, encpp_sharp_comm_cfg, ENCPP_SHARP_COMM_MAX),
#endif
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, sensor_temp, 14 * 12),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_df_shape, ISP_REG1_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_ratio_amp, ISP_REG1_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_k_dlt_bk, ISP_REG1_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_ct_rt_bk, ISP_REG1_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_dtc_hf_bk, ISP_REG1_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_dtc_mf_bk, ISP_REG1_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_lay0_d2d0_rt_br, ISP_REG1_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_lay1_d2d0_rt_br, ISP_REG1_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_lay0_nrd_rt_br, ISP_REG1_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_tdnf_lay1_nrd_rt_br, ISP_REG1_TBL_LENGTH),

	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, lca_pf_satu_lut, ISP_REG_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, lca_gf_satu_lut, ISP_REG_TBL_LENGTH),

	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_sharp_ss_value, ISP_REG_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_sharp_ls_value, ISP_REG_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_sharp_hsv, 46),
#ifdef USE_ENCPP
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, encpp_sharp_ss_value, ISP_REG_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, encpp_sharp_ls_value, ISP_REG_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, encpp_sharp_hsv, 46),
#endif

	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_wdr_de_purpl_hsv_tbl, ISP_WDR_TBL_SIZE),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_pltm_stat_gd_cv, PLTM_MAX_STAGE * 15),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_pltm_df_cv, PLTM_MAX_STAGE * ISP_REG_TBL_LENGTH),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_pltm_lum_map_cv, PLTM_MAX_STAGE * 128),
	ISP_FILE_ARRAY_ATTR(isp_tuning_cfg, isp_pltm_gtm_tbl, 512),

	ISP_FILE_STRUCT_ARRAY_ATTR(isp_color_matrix0, matrix, 12),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_color_matrix1, matrix, 12),
	ISP_FILE_STRUCT_ARRAY_ATTR(isp_color_matrix2, matrix, 12),
};

static struct FileAttribute FileAttr[] = {
	{"isp_test_param.ini", array_size(IspTestParam), &IspTestParam[0],},
	{"isp_3a_param.ini", array_size(Isp3aParam), &Isp3aParam[0],},
	{"isp_dynamic_param.ini", array_size(IspDynamicParam), &IspDynamicParam[0],},
	{"isp_tuning_param.ini", array_size(IspTuningParam), &IspTuningParam[0],},
};

#define MAGIC_NUM 0x64387483

int cfg_get_int(dictionary *ini, char *main, char *sub, int *val)
{
	char key[128] = { 0 };
	int ret;

	strcpy(key, main);
	strcat(key,":");
	strcat(key,sub);
	ret = isp_iniparser_getint(ini, key, MAGIC_NUM);
	if(ret == MAGIC_NUM) {
		*val = 0;
		return -1;
	}
	*val = ret;
	return 0;
}

int cfg_get_double(dictionary *ini, char *main, char *sub, double *val)
{
	char key[128] = { 0 };
	double ret;

	strcpy(key, main);
	strcat(key, ":");
	strcat(key, sub);
	ret = isp_iniparser_getdouble(ini, key, MAGIC_NUM/1.0);
	if((ret <= MAGIC_NUM/1.0 + 1.0) || (ret >= MAGIC_NUM/1.0 - 1.0)) {
		*val = 0.0;
		return -1;
	}
	*val = ret;
	//printf("MAGIC_NUM/1.0 = %f, MAGIC_NUM = %d\n", MAGIC_NUM/1.0, MAGIC_NUM);
	return 0;
}

int cfg_get_int_arr(dictionary *ini, char *main, char *sub, int *arr, int len)
{
	char key[128] = { 0 };
	int ret;

	strcpy(key, main);
	strcat(key, ":");
	strcat(key, sub);
	ret = isp_iniparser_get_int_array(ini, key, arr, len, MAGIC_NUM);
	if(ret == MAGIC_NUM) {
		return -1;
	}
	return ret;
}

int cfg_get_double_arr(dictionary *ini, char *main, char *sub, double *arr, int len)
{
	char key[128] = { 0 };
	int ret;

	strcpy(key, main);
	strcat(key, ":");
	strcat(key, sub);
	ret = isp_iniparser_get_double_array(ini, key, arr, len, MAGIC_NUM);
	if(ret == MAGIC_NUM) {
		return -1;
	}
	return ret;
}

int isp_read_file(char *file_path, char *buf, size_t len)
{
	FILE *fp;
	size_t buf_len;
	struct stat s;

	if (stat(file_path, &s)) {
		ISP_ERR("%s stat error!\n", file_path);
		return -1;
	}

	fp = fopen(file_path, "r");
	if (fp == NULL) {
		ISP_ERR("open %s failed!\n", file_path);
		return -1;
	}

	buf_len = fread(buf, 1, s.st_size, fp);
	ISP_CFG_LOG(ISP_LOG_CFG, "%s len = %d, expect len = %lld\n", file_path, buf_len, s.st_size);
	fclose(fp);

	if (buf_len <= 0)
		return -1;

	return buf_len;
}

int isp_parser_cfg(struct isp_param_config *isp_ini_cfg,
		  dictionary *ini, struct FileAttribute *file_attr)
{
	int i, j, *array_value, val = 0;
	struct IspParamAttribute *param;
	char sub_name[128] = { 0 };

	/* fetch ISP config! */
	for (i = 0; i < file_attr->param_len; i++) {
		param = file_attr->pIspParam + i;
		if (param->main == NULL || param->sub == NULL) {
			ISP_WARN("param->main or param->sub is NULL!\n");
			continue;
		}
		if (param->len == 1) {
			if (-1 == cfg_get_int(ini, param->main, param->sub, &val)) {
				ISP_WARN("fetch %s->%s failed!\n", param->main, param->sub);
			} else {
				if (param->set_param) {
					param->set_param(isp_ini_cfg, (void *)&val, param->len);
					ISP_CFG_LOG(ISP_LOG_CFG, "fetch %s->%s value = %d\n", param->main, param->sub, val);
				}
			}
		} else if (param->len > 1) {
			array_value = (int *)malloc(param->len * sizeof(int));
			memset(array_value, 0, param->len * sizeof(int));

			param->len = cfg_get_int_arr(ini, param->main, param->sub, array_value, param->len);
			if (-1 == param->len) {
				ISP_WARN("fetch %s->%s failed!\n", param->main, param->sub);
			} else {
				if (param->set_param) {
					param->set_param(isp_ini_cfg, (void *)array_value, param->len);
					ISP_CFG_LOG(ISP_LOG_CFG, "fetch %s->%s length = %d\n", param->main, param->sub, param->len);
				}
			}

			if (array_value)
				free(array_value);
		}
	}
	ISP_PRINT("fetch isp_cfg done!\n");
	return 0;
}

int isp_parser_tbl(struct isp_param_config *isp_ini_cfg, char *tbl_patch)
{
	int len, ret = 0;
	char isp_gamma_tbl_path[128] = "\0";
	char isp_lsc_tbl_path[128] = "\0";
	char isp_msc_tbl_path[128] = "\0";
	char isp_cem_tbl_path[128] = "\0";

	strcpy(isp_gamma_tbl_path, tbl_patch);
	strcpy(isp_lsc_tbl_path, tbl_patch);
	strcpy(isp_msc_tbl_path, tbl_patch);
	strcpy(isp_cem_tbl_path, tbl_patch);

	strcat(isp_gamma_tbl_path, "gamma_tbl.bin");
	strcat(isp_lsc_tbl_path, "lsc_tbl.bin");
	strcat(isp_msc_tbl_path, "msc_tbl.bin");
	strcat(isp_cem_tbl_path, "cem_tbl.bin");

	ISP_PRINT("Fetch table from \"%s\"\n", tbl_patch);

	/* fetch gamma_tbl table! */
	len = isp_read_file(isp_gamma_tbl_path, (char *)isp_ini_cfg->isp_tunning_settings.gamma_tbl_ini, SIZE_OF_GAMMA_TBL);
	if (len < 0 || len != SIZE_OF_GAMMA_TBL) {
		ISP_WARN("read gamma_tbl from failed! len = %d, but %d is required\n", len, SIZE_OF_GAMMA_TBL);
		isp_ini_cfg->isp_test_settings.gamma_en = 0;
	}

	/* fetch lsc table! */
	len = isp_read_file(isp_lsc_tbl_path, (char *)isp_ini_cfg->isp_tunning_settings.lsc_tbl, SIZE_OF_LSC_TBL);
	if (len < 0 || len != SIZE_OF_LSC_TBL) {
		ISP_WARN("read lsc_tbl from failed! len = %d, but %d is required\n", len, SIZE_OF_LSC_TBL);
		isp_ini_cfg->isp_test_settings.lsc_en = 0;
	}

	/* fetch msc table! */
	len = isp_read_file(isp_msc_tbl_path, (char *)isp_ini_cfg->isp_tunning_settings.msc_tbl, sizeof(isp_ini_cfg->isp_tunning_settings.msc_tbl));
	if (len < 0 || len != sizeof(isp_ini_cfg->isp_tunning_settings.msc_tbl)) {
		ISP_WARN("read msc_tbl from failed! len = %d, but %d is required\n", len, sizeof(isp_ini_cfg->isp_tunning_settings.msc_tbl));
		isp_ini_cfg->isp_test_settings.msc_en = 0;
	}

	/* fetch cem table! */
	len = isp_read_file(isp_cem_tbl_path, (char *)isp_ini_cfg->isp_tunning_settings.isp_cem_table, ISP_CEM_MEM_SIZE + ISP_CEM_MEM_SIZE);
	if (len < 0 || len != ISP_CEM_MEM_SIZE + ISP_CEM_MEM_SIZE) {
		ISP_WARN("read cem_tbl from failed! len = %d, but %d is required\n", len, ISP_CEM_MEM_SIZE + ISP_CEM_MEM_SIZE);
		isp_ini_cfg->isp_test_settings.cem_en = 0;
	}

	return ret;
}
#endif

struct isp_cfg_array cfg_arr[] = {

#ifdef SENSOR_GC2053
	{"gc2053_mipi",  "gc2053_mipi_isp600_20220511_164617_vlc4_day", 1920, 1088, 20, 0, 0, &gc2053_mipi_v853_isp_cfg},
	{"gc2053_mipi",  "gc2053_mipi_isp600_20220415_144837_ir", 1920, 1088, 20, 0, 1, &gc2053_mipi_ir_isp_cfg},
#endif

#ifdef SENSOR_GC4663
	{"gc4663_mipi",  "gc4663_mipi_isp600_20220706_155211_day", 2560, 1440, 20, 0, 0, &gc4663_mipi_linear_isp_cfg},
	{"gc4663_mipi",  "gc4663_mipi_isp600_20220118_171111_ir", 2560, 1440, 20, 0, 1, &gc4663_mipi_ir_isp_cfg},
	{"gc4663_mipi",  "gc4663_mipi_wdr_default_v853", 2560, 1440, 20, 1, 0, &gc4663_mipi_wdr_v853_isp_cfg},
#endif

#ifdef SENSOR_SC4336
	{"sc4336_mipi",  "sc4336_mipi_isp600_20220718_170000_day", 2560, 1440, 25, 0, 0, &sc4336_mipi_isp_cfg},
#endif

};

int find_nearest_index(int mod, int temp)
{
	int i = 0, index = 0;
	int min_dist = 1 << 30, tmp_dist;
	int temp_array1[4] = {2800, 4000, 5000, 6500};
	int temp_array2[6] = {2200, 2800, 4000, 5000, 5500, 6500};

	if(mod == 1) {
		for(i = 0; i < 4; i++) {
			tmp_dist = temp_array1[i] - temp;
			tmp_dist = (tmp_dist < 0) ? -tmp_dist : tmp_dist;
			if(tmp_dist < min_dist) {
				min_dist = tmp_dist;
				index = i;
			}
			ISP_CFG_LOG(ISP_LOG_CFG, "mode: %d, tmp_dist: %d, min_dist: %d, index: %d.\n", mod, tmp_dist, min_dist, index);
		}
	} else if (mod == 2) {
		for(i = 0; i < 6; i++) {
			tmp_dist = temp_array2[i] - temp;
			tmp_dist = (tmp_dist < 0) ? -tmp_dist : tmp_dist;
			if(tmp_dist < min_dist) {
				min_dist = tmp_dist;
				index = i;
			}
			ISP_CFG_LOG(ISP_LOG_CFG, "mode: %d, tmp_dist: %d, min_dist: %d, index: %d.\n", mod, tmp_dist, min_dist, index);
		}
	} else {
		ISP_ERR("mod error.\n");
	}
	ISP_CFG_LOG(ISP_LOG_CFG, "nearest temp index: %d.\n", index);
	return index;
}

int parser_sync_info(struct isp_param_config *param, char *isp_cfg_name, int isp_id)
{
	FILE *file_fd = NULL;
	char fdstr[50];
	int sync_info[775];
	int lsc_ind = 0, lsc_cnt = 0, lsc_tab_cnt = 0;
	int version_num = 0;
	int lsc_temp = 0;
	int enable = 0;

	sprintf(fdstr, "/mnt/extsd/%s_%d.bin", isp_cfg_name, isp_id);
	file_fd = fopen(fdstr, "rb");
	if (file_fd == NULL) {
		ISP_ERR("open bin failed.\n");
		return -1;
	} else {
		fread(&sync_info[0], sizeof(int)*775, 1, file_fd);
	}
	fclose(file_fd);

	version_num = sync_info[0];
	enable = sync_info[1];

	ISP_CFG_LOG(ISP_LOG_CFG, "%s enable mode = %d.\n", __func__, enable);

	if (0 == enable) {
		return 0;
	} else if (1 == enable) {
		memcpy(param->isp_tunning_settings.bayer_gain, &sync_info[2], sizeof(int)*ISP_RAW_CH_MAX);
		return 0;
	} else if (2 == enable) {
		goto lsc_tbl;
	}
	memcpy(param->isp_tunning_settings.bayer_gain, &sync_info[2], sizeof(int)*ISP_RAW_CH_MAX);

	ISP_CFG_LOG(ISP_LOG_CFG, "%s bayer_gain: %d, %d, %d, %d.\n", __func__,
		param->isp_tunning_settings.bayer_gain[0], param->isp_tunning_settings.bayer_gain[1],
		param->isp_tunning_settings.bayer_gain[2], param->isp_tunning_settings.bayer_gain[3]);
lsc_tbl:
	lsc_temp = sync_info[6];
	lsc_ind = find_nearest_index(param->isp_tunning_settings.ff_mod, lsc_temp);
	ISP_CFG_LOG(ISP_LOG_CFG, "%s lsc_ind: %d.\n", __func__, lsc_ind);

	if(param->isp_tunning_settings.ff_mod == 1) {
		for(lsc_tab_cnt = 0; lsc_tab_cnt < 4; lsc_tab_cnt++) {
			if(lsc_tab_cnt == lsc_ind)
				continue;
			for(lsc_cnt = 0; lsc_cnt < 768; lsc_cnt++) {
				param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt]
					= sync_info[7+lsc_cnt]*param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt]/param->isp_tunning_settings.lsc_tbl[lsc_ind][lsc_cnt];
			}
		}

		for(lsc_tab_cnt = 4; lsc_tab_cnt < 8; lsc_tab_cnt++) {
			if(lsc_tab_cnt == (lsc_ind+4))
				continue;
			for(lsc_cnt = 0; lsc_cnt < 768; lsc_cnt++) {
				param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt]
					= sync_info[7+lsc_cnt]*param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt]/param->isp_tunning_settings.lsc_tbl[lsc_ind+4][lsc_cnt];
			}
		}
		for(lsc_cnt = 0; lsc_cnt < 768; lsc_cnt++)
			param->isp_tunning_settings.lsc_tbl[lsc_ind][lsc_cnt] = param->isp_tunning_settings.lsc_tbl[lsc_ind+4][lsc_cnt]
				= sync_info[7+lsc_cnt];

		ISP_CFG_LOG(ISP_LOG_CFG, "%s lsc_tbl_1 0: %d, 1: %d, 766: %d, 767: %d.\n", __func__,
			param->isp_tunning_settings.lsc_tbl[1][0], param->isp_tunning_settings.lsc_tbl[1][1],
			param->isp_tunning_settings.lsc_tbl[1][766], param->isp_tunning_settings.lsc_tbl[1][767]);
	} else if(param->isp_tunning_settings.ff_mod == 2) {
		for(lsc_tab_cnt = 0; lsc_tab_cnt < 6; lsc_tab_cnt++) {
			if(lsc_tab_cnt == lsc_ind)
				continue;
			for(lsc_cnt = 0; lsc_cnt < 768; lsc_cnt++) {
				if(param->isp_tunning_settings.lsc_tbl[lsc_ind][lsc_cnt] == 0) {
					ISP_ERR("lsc_ind: %d, lsc_cnt: %d is zero.\n", lsc_ind, lsc_cnt);
					continue;
				} else
					param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt]
						= sync_info[7+lsc_cnt]*param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt]/param->isp_tunning_settings.lsc_tbl[lsc_ind][lsc_cnt];
				if(param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt] == 0) {
					ISP_ERR("result------>lsc_ind: %d, lsc_cnt: %d is zero.\n", lsc_tab_cnt, lsc_cnt);
				}
			}
		}

		for(lsc_tab_cnt = 6; lsc_tab_cnt < 12; lsc_tab_cnt++) {
			if(lsc_tab_cnt == (lsc_ind+6))
				continue;
			for(lsc_cnt = 0; lsc_cnt < 768; lsc_cnt++) {
				if(param->isp_tunning_settings.lsc_tbl[lsc_ind + 7][lsc_cnt] == 0) {
					ISP_ERR("lsc_ind: %d, lsc_cnt: %d is zero.\n", lsc_ind+6, lsc_cnt);
					continue;
				} else
					param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt]
						= sync_info[7+lsc_cnt]*param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt]/param->isp_tunning_settings.lsc_tbl[lsc_ind+6][lsc_cnt];
				if(param->isp_tunning_settings.lsc_tbl[lsc_tab_cnt][lsc_cnt] == 0) {
					ISP_ERR("result------>lsc_ind: %d, lsc_cnt: %d is zero.\n", lsc_tab_cnt, lsc_cnt);
				}
			}
		}
		for(lsc_cnt = 0; lsc_cnt < 768; lsc_cnt++)
			param->isp_tunning_settings.lsc_tbl[lsc_ind][lsc_cnt] = param->isp_tunning_settings.lsc_tbl[lsc_ind+6][lsc_cnt]
				= sync_info[7+lsc_cnt];

		ISP_CFG_LOG(ISP_LOG_CFG, "%s lsc_tbl_1 0: %d, 1: %d, 766: %d, 767: %d.\n", __func__,
			param->isp_tunning_settings.lsc_tbl[1][0], param->isp_tunning_settings.lsc_tbl[1][1],
			param->isp_tunning_settings.lsc_tbl[1][766], param->isp_tunning_settings.lsc_tbl[1][767]);
	} else {
		ISP_ERR("isp ff_mod error.\n");
	}
	return 0;
}

int isp_save_tbl(struct isp_param_config *param, char *tbl_patch)
{
	FILE *file_fd = NULL;
	char fdstr[50];

	sprintf(fdstr, "%s/gamma_tbl.bin", tbl_patch);
	file_fd = fopen(fdstr, "wb");
	if (file_fd == NULL) {
		ISP_WARN("open %s failed!!!\n", fdstr);
		return -1;
	} else {
		fwrite(param->isp_tunning_settings.gamma_tbl_ini, SIZE_OF_GAMMA_TBL, 1, file_fd);
		ISP_PRINT("save isp_ctx to %s success!!!\n", fdstr);
	}
	fclose(file_fd);

	sprintf(fdstr, "%s/lsc_tbl.bin", tbl_patch);
	file_fd = fopen(fdstr, "wb");
	if (file_fd == NULL) {
		ISP_WARN("open %s failed!!!\n", fdstr);
		return -1;
	} else {
		fwrite(param->isp_tunning_settings.lsc_tbl, SIZE_OF_LSC_TBL, 1, file_fd);
		ISP_PRINT("save isp_ctx to %s success!!!\n", fdstr);
	}
	fclose(file_fd);

	sprintf(fdstr, "%s/cem_tbl.bin", tbl_patch);
	file_fd = fopen(fdstr, "wb");
	if (file_fd == NULL) {
		ISP_WARN("open %s failed!!!\n", fdstr);
		return -1;
	} else {
		fwrite(param->isp_tunning_settings.isp_cem_table, ISP_CEM_MEM_SIZE, 1, file_fd);
		ISP_PRINT("save isp_ctx to %s success!!!\n", fdstr);
	}
	fclose(file_fd);

	sprintf(fdstr, "%s/cem_tbl.bin", tbl_patch);
	file_fd = fopen(fdstr, "wb");
	if (file_fd == NULL) {
		ISP_WARN("open %s failed!!!\n", fdstr);
		return -1;
	} else {
		fwrite(param->isp_tunning_settings.isp_cem_table1, ISP_CEM_MEM_SIZE, 1, file_fd);
		ISP_PRINT("save isp_ctx to %s success!!!\n", fdstr);
	}
	fclose(file_fd);

	return 0;
}

static int parse_isp_cfg_bin(struct isp_param_config *param, char *isp_cfg_name, int isp_id, int sync_mode, char *isp_cfg_bin_path)
{
	int ret = 0;
	char path_str[ISP_CFG_BIN_PATH_LEN] = {0};
	char version_data[ISP_CFG_BIN_VER_DATA_LEN] = {0};
	char read_isp_cfg_name[ISP_CFG_BIN_PATH_LEN] = {0};
	int i;

#define ISP_TEST_PATH_FLASH    "/mnt/extsd/isp_test/isp/isp_test_settings.bin"
#define ISP_3A_PATH_FLASH      "/mnt/extsd/isp_test/isp/isp_3a_settings.bin"
#define ISP_ISO_PATH_FLASH     "/mnt/extsd/isp_test/isp/isp_iso_settings.bin"
#define ISP_TUNNING_PATH_FLASH "/mnt/extsd/isp_test/isp/isp_tuning_settings.bin"

	if (isp_cfg_bin_path == NULL) {
		ISP_ERR("Ivalid isp_cfg_bin_path\n");
		return -1;
	} else if (strlen(isp_cfg_bin_path) > (ISP_CFG_BIN_PATH_LEN + 64)) {
		ISP_ERR("the isp_cfg_bin_path more than [ISP_CFG_BIN_PATH_LEN] = %d, please modify the length for path\n",
			ISP_CFG_BIN_PATH_LEN);
	} else {
		struct load_isp_param_str load_isp_bin_str[4] = {
			{"isp_test_settings.bin"},
			{"isp_3a_settings.bin"},
			{"isp_iso_settings.bin"},
			{"isp_tuning_settings.bin"},
		};
		struct load_isp_param_t load_isp_cfg_bin[4] = {
			{ISP_TEST_PATH_FLASH, (char *)&param->isp_test_settings, sizeof(struct isp_test_param)},
			{ISP_3A_PATH_FLASH, (char *)&param->isp_3a_settings, sizeof(struct isp_3a_param)},
			{ISP_ISO_PATH_FLASH, (char *)&param->isp_iso_settings, sizeof(struct isp_dynamic_param )},
			{ISP_TUNNING_PATH_FLASH, (char *)&param->isp_tunning_settings, sizeof(struct isp_tunning_param)},
		};

		int i;
		for (i = 0;i < 4; i++) {
			sprintf(load_isp_cfg_bin[i].path, "%s/%s", isp_cfg_bin_path, load_isp_bin_str[i].str_path);
			ISP_PRINT("load_isp_cfg_bin[%d].path : %s\n", i+1, load_isp_cfg_bin[i].path);
		}

		if (!access(load_isp_cfg_bin[0].path, 0) &&
		!access(load_isp_cfg_bin[1].path, 0) &&
		!access(load_isp_cfg_bin[2].path, 0) &&
		!access(load_isp_cfg_bin[3].path, 0)) {

			int fd;
			struct stat buf;
			FILE *file_fd = NULL;

			sprintf(path_str, "%s/bin_version", isp_cfg_bin_path);
			/* detect bin_version and print it */
			file_fd = fopen(path_str, "r");
			if (file_fd == NULL) {
				ISP_WARN("open %s error, can not open bin_version\n", path_str);
			} else {
				ret = fread(version_data, sizeof(char), ISP_CFG_BIN_VER_DATA_LEN, file_fd);
				version_data[255] = '\0';
				ISP_PRINT("===========> ISP_CFG_BIN_VERSION <===========\n%s", version_data);

				i = 11;
				while (version_data[i] != '#') {
					// start from idex_11
					read_isp_cfg_name[i-11] = version_data[i];
					i++;
					/* bin_version is changed, it will cut the str */
					if (i >= (ISP_CFG_BIN_PATH_LEN - 1))
						read_isp_cfg_name[ISP_CFG_BIN_PATH_LEN - 1] = '\0';
				}
				read_isp_cfg_name[i+1] = '\0';
				/* copy the isp_cfg_name for isp_debug_info */
				strcpy(isp_cfg_name, read_isp_cfg_name);
				fclose(file_fd);
			}

			for(i = 0; i < 4; i++) {
				file_fd = fopen(load_isp_cfg_bin[i].path, "r");
				if (file_fd == NULL) {
					ISP_ERR("open %s error!\n", load_isp_cfg_bin[i].path);
					return -1;
				}
				ret = fread(load_isp_cfg_bin[i].isp_param_settings,
				load_isp_cfg_bin[i].size, 1, file_fd);
				fd = fileno(file_fd);
				fstat(fd, &buf);
				ISP_PRINT("isp_cfg version : %s", ctime(&buf.st_mtime));
				fclose(file_fd);
				file_fd = NULL;
			}
			ISP_PRINT("=============================================\n");
			if(sync_mode)
				ret = parser_sync_info(param, "isp external effect", isp_id);

		} else {
			ISP_ERR("Ivalid isp_cfg.bin, please check them!\n");
			return -1;
		}
	}

	return ret;
}

#if ISP_LIB_USE_INIPARSER
static int parse_isp_cfg_ini(struct isp_param_config *param, char *isp_cfg_ini_path)
{
	char file_name[ISP_CFG_BIN_PATH_LEN] = {0};
	char isp_tbl_path[ISP_CFG_BIN_PATH_LEN] = {0};
	dictionary *ini;
	int ret = 0, i;

	ISP_PRINT("================ ISP will use external isp_cfg.ini ============ \n");
	sprintf(isp_tbl_path, "%s/bin/", isp_cfg_ini_path);
	if (access(isp_cfg_ini_path, F_OK) == 0) {
		ISP_PRINT("find %s, read ini start!!!\n", isp_cfg_ini_path);
		for (i = 0; i < array_size(FileAttr); i++) {
			sprintf(file_name, "%s%s", isp_cfg_ini_path, FileAttr[i].file_name);
			ISP_PRINT("Fetch ini file form \"%s\"\n", file_name);

			ini = isp_iniparser_load(file_name);
			if (ini == NULL) {
				ISP_ERR("read ini error!!!\n");
				return -1;
			}
			isp_parser_cfg(param, ini, &FileAttr[i]);
			isp_iniparser_freedict(ini);
		}

		ret = isp_parser_tbl(param, isp_tbl_path);
		ISP_PRINT("read ini end!!!\n");
		return ret;
	} else {
		ISP_ERR("Ivalid isp_cfg_ini_path, please check path!\n");
		return -1;
	}

	return ret;
}
#endif

int parse_isp_cfg(struct isp_param_config *param, char *isp_cfg_name, int isp_id, int sync_mode, int ir_flag, char *isp_cfg_bin_path)
{
	int ret = 0;
	char detect_ini_path[ISP_CFG_BIN_PATH_LEN] = {0};
	char save_ini_path[ISP_CFG_BIN_PATH_LEN] = {0};

	/* save the path from user */
	strcpy(save_ini_path, isp_cfg_bin_path);
#if ISP_TUNING_ENABLE
		if (ir_flag == 0) {
			/* externel color isp_cfg */
			if ( ISP_TUNING_ENABLE && !access(EXTERNEL_COLOR_ISP_CFG_PATH, 0) ) {
				ISP_PRINT("================ ISP will use external color isp_cfg.bin ============ \n");
				strcpy(save_ini_path, EXTERNEL_COLOR_ISP_CFG_PATH);
			} else
				ISP_PRINT(" %s is not exit, ISP will use user's path: %s\n", EXTERNEL_COLOR_ISP_CFG_PATH, save_ini_path);
		} else if (ir_flag == 1) {
			/* externel ir isp_cfg */
			if ( ISP_TUNING_ENABLE && !access(EXTERNEL_IR_ISP_CFG_PATH, 0) ) {
				ISP_PRINT("================ ISP will use external ir isp_cfg.bin ============ \n");
				strcpy(save_ini_path, EXTERNEL_IR_ISP_CFG_PATH);
			} else
				ISP_PRINT(" %s is not exit, ISP will use user's path: %s\n", EXTERNEL_IR_ISP_CFG_PATH, save_ini_path);
		}
#endif

	sprintf(detect_ini_path, "%s/bin/", save_ini_path);
	if (access(detect_ini_path, F_OK) == 0) {
#if ISP_LIB_USE_INIPARSER
		ret = parse_isp_cfg_ini(param, save_ini_path);
#endif
	} else {
		ret = parse_isp_cfg_bin(param, isp_cfg_name, isp_id, sync_mode, save_ini_path);
	}

	return ret;
}

int parser_ini_info(struct isp_param_config *param, char *isp_cfg_name, char *sensor_name,
			int w, int h, int fps, int wdr, int ir, int sync_mode, int isp_id)
{
	int i;
	int ret = -1;

#if ISP_TUNING_ENABLE
	if( ISP_TUNING_ENABLE && !access(EXTERNEL_COLOR_ISP_CFG_PATH, 0) ) {

		ret = parse_isp_cfg(param, isp_cfg_name, isp_id, sync_mode, ir, EXTERNEL_COLOR_ISP_CFG_PATH);
	}
#endif

	if (ret == -1) {
		struct isp_cfg_pt *cfg = NULL;
		ISP_PRINT("prefer isp config: [%s], %dx%d, %d, %d, %d\n", sensor_name, w, h, fps, wdr, ir);
		for (i = 0; i < array_size(cfg_arr); i++) {
			if (!strncmp(sensor_name, cfg_arr[i].sensor_name, 15) &&
				(w == cfg_arr[i].width) && (h == cfg_arr[i].height) &&
				(fps == cfg_arr[i].fps) && (wdr == cfg_arr[i].wdr) &&
				(ir == cfg_arr[i].ir)) {
					cfg = cfg_arr[i].cfg;
					ISP_PRINT("find %s_%d_%d_%d_%d [%s] isp config\n", cfg_arr[i].sensor_name,
					cfg_arr[i].width, cfg_arr[i].height, cfg_arr[i].fps, cfg_arr[i].wdr, cfg_arr[i].isp_cfg_name);
					break;
				}
		}

		if (i == array_size(cfg_arr)) {
			for (i = 0; i < array_size(cfg_arr); i++) {
				if (!strncmp(sensor_name, cfg_arr[i].sensor_name, 15) && (wdr == cfg_arr[i].wdr)) {
					cfg = cfg_arr[i].cfg;
					ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp config, use %s_%d_%d_%d_%d_%d -> [%s]\n", sensor_name, w, h, fps, wdr, ir,
						cfg_arr[i].sensor_name,	cfg_arr[i].width, cfg_arr[i].height, cfg_arr[i].fps, cfg_arr[i].wdr,
						cfg_arr[i].ir, cfg_arr[i].isp_cfg_name);
					break;
				}
			}

			if (i == array_size(cfg_arr)) {
				for (i = 0; i < array_size(cfg_arr); i++) {
					if (wdr == cfg_arr[i].wdr) {
						cfg = cfg_arr[i].cfg;
						ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp config, use %s_%d_%d_%d_%d_%d -> [%s]\n", sensor_name, w, h, fps, wdr, ir,
							cfg_arr[i].sensor_name,	cfg_arr[i].width, cfg_arr[i].height, cfg_arr[i].fps, cfg_arr[i].wdr,
							cfg_arr[i].ir, cfg_arr[i].isp_cfg_name);
						break;
					}
				}
			}

			if (i == array_size(cfg_arr)) {
				ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp config, use default config [%s]\n",
				sensor_name, w, h, fps, wdr, ir, cfg_arr[i-1].isp_cfg_name);
				cfg = cfg_arr[i-1].cfg;// use the last one
			}
		}
		strcpy(isp_cfg_name, cfg_arr[i].isp_cfg_name);
		param->isp_test_settings = *cfg->isp_test_settings;
		param->isp_3a_settings = *cfg->isp_3a_settings;
		param->isp_iso_settings = *cfg->isp_iso_settings;
		param->isp_tunning_settings = *cfg->isp_tunning_settings;

		//isp_save_tbl(param, "/mnt/extsd");

		if(sync_mode)
			parser_sync_info(param, cfg_arr[i].isp_cfg_name, isp_id);
	}

	return 0;
}


