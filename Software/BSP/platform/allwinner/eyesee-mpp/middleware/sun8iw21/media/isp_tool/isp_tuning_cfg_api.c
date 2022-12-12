/*
 ******************************************************************************
 * 
 * isp_tuning_cfg_api.c
 * 
 * Hawkview ISP - isp_tuning_cfg_api.c module
 * 
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 * 
 * Version		  Author         Date		    Description
 * 
 *   1.0		  clarkyy	2016/05/31	VIDEO INPUT
 * 
 *****************************************************************************
 */

#include "isp_tuning_cfg_api.h"
#include "string.h"

#include "stdio.h"

#if 1
/* some essential structs and functions for test */
void *isp_dev_get_tuning(struct hw_isp_device *isp)
{
	if (!isp->tuning) {
		//ISP_ERR("ISP tuning is NULL.\n");
		return NULL;
	}
	return isp->tuning;
}

HW_S32 isp_ctx_hardware_update(struct isp_lib_context *isp_gen)
{
	return 0;
}

#endif


HW_S32 hw_get_isp_cfg(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data)
{
	HW_S32 ret = -1;
	unsigned char *data_ptr = NULL;
	struct isp_tuning *tuning = NULL;

	if (!isp || !cfg_data)
		return -1;

	/* call isp api */
	tuning = isp_dev_get_tuning(isp);
	if (tuning == NULL)
		 return -2;

	/* fill cfg data */
	ret = 0;
	data_ptr = (unsigned char *)cfg_data;

	

	switch (group_id)
	{
	case HW_ISP_CFG_TEST: /* isp_test_param */
		if (cfg_ids & HW_ISP_CFG_TEST_PUB) /* isp_test_pub */
		{

			struct isp_test_pub_cfg *isp_test_pub = (struct isp_test_pub_cfg *)data_ptr;
			isp_test_pub->test_mode = tuning->params.isp_test_settings.isp_test_mode;
			isp_test_pub->gain = tuning->params.isp_test_settings.isp_gain;
			isp_test_pub->exp_line = tuning->params.isp_test_settings.isp_exp_line;
			isp_test_pub->color_temp = tuning->params.isp_test_settings.isp_color_temp;
			isp_test_pub->log_param = tuning->params.isp_test_settings.isp_log_param;
			
			/* test */
			/*struct isp_test_pub_cfg *isp_test_pub = (struct isp_test_pub_cfg *)data_ptr;
			isp_test_pub->test_mode =htonl(tuning->params.isp_test_settings.isp_test_mode);
			isp_test_pub->gain =htonl(tuning->params.isp_test_settings.isp_gain);
			isp_test_pub->exp_line = htonl(tuning->params.isp_test_settings.isp_exp_line);
			isp_test_pub->color_temp = htonl(tuning->params.isp_test_settings.isp_color_temp);
			isp_test_pub->log_param = htonl(tuning->params.isp_test_settings.isp_log_param);*/
			
			/* offset */
			data_ptr += sizeof(struct isp_test_pub_cfg);
			ret += sizeof(struct isp_test_pub_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TEST_EXPTIME) /* isp_test_exptime */
		{
			struct isp_test_item_cfg *isp_test_exptime = (struct isp_test_item_cfg *)data_ptr;
			isp_test_exptime->enable = tuning->params.isp_test_settings.isp_test_exptime;
			isp_test_exptime->start = tuning->params.isp_test_settings.exp_line_start;
			isp_test_exptime->step = tuning->params.isp_test_settings.exp_line_step;
			isp_test_exptime->end = tuning->params.isp_test_settings.exp_line_end;
			isp_test_exptime->change_interval = tuning->params.isp_test_settings.exp_change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		
		if (cfg_ids & HW_ISP_CFG_TEST_GAIN) /* isp_test_gain */
		{
			struct isp_test_item_cfg *isp_test_gain = (struct isp_test_item_cfg *)data_ptr;
			isp_test_gain->enable = tuning->params.isp_test_settings.isp_test_gain;
			isp_test_gain->start = tuning->params.isp_test_settings.gain_start;
			isp_test_gain->step = tuning->params.isp_test_settings.gain_step;
			isp_test_gain->end = tuning->params.isp_test_settings.gain_end;
			isp_test_gain->change_interval = tuning->params.isp_test_settings.gain_change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		
		if (cfg_ids & HW_ISP_CFG_TEST_FOCUS) /* isp_test_focus */
		{
			struct isp_test_item_cfg *isp_test_focus = (struct isp_test_item_cfg *)data_ptr;
			isp_test_focus->enable = tuning->params.isp_test_settings.isp_test_focus;
			isp_test_focus->start = tuning->params.isp_test_settings.focus_start;
			isp_test_focus->step = tuning->params.isp_test_settings.focus_step;
			isp_test_focus->end = tuning->params.isp_test_settings.focus_end;
			isp_test_focus->change_interval = tuning->params.isp_test_settings.focus_change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TEST_FORCED) /* isp_test_forced */
		{
			struct isp_test_forced_cfg *isp_test_forced = (struct isp_test_forced_cfg *)data_ptr;
			isp_test_forced->ae_enable = tuning->params.isp_test_settings.ae_forced;
			isp_test_forced->lum = tuning->params.isp_test_settings.lum_forced;

			/* offset */
			data_ptr += sizeof(struct isp_test_forced_cfg);
			ret += sizeof(struct isp_test_forced_cfg);
		}
		
		if (cfg_ids & HW_ISP_CFG_TEST_ENABLE) /* isp_test_enable */
		{
			memcpy(data_ptr, &(tuning->params.isp_test_settings.manual_en), sizeof(struct isp_test_enable_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_test_enable_cfg);
			ret += sizeof(struct isp_test_enable_cfg);
		}
		break;

	case HW_ISP_CFG_3A: /* isp_3a_param */
		if (cfg_ids & HW_ISP_CFG_AE_PUB) /* isp_ae_pub */
		{
			struct isp_ae_pub_cfg *isp_ae_pub = (struct isp_ae_pub_cfg *)data_ptr;
			isp_ae_pub->define_table = tuning->params.isp_3a_settings.define_ae_table;
			isp_ae_pub->max_lv = tuning->params.isp_3a_settings.ae_max_lv;
			//isp_ae_pub->aperture = tuning->params.isp_3a_settings.fno;
			isp_ae_pub->hist_mode_en = tuning->params.isp_3a_settings.ae_hist_mod_en;
			isp_ae_pub->compensation_step = tuning->params.isp_3a_settings.exp_comp_step;
			isp_ae_pub->touch_dist_index = tuning->params.isp_3a_settings.ae_touch_dist_ind;

			/* offset */
			data_ptr += sizeof(struct isp_ae_pub_cfg);
			ret += sizeof(struct isp_ae_pub_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_PREVIEW_TBL) /* isp_ae_preview_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_preview_tbl = (struct isp_ae_table_cfg *)data_ptr;
			isp_ae_preview_tbl->length = tuning->params.isp_3a_settings.ae_table_preview_length;
			memcpy(&(isp_ae_preview_tbl->value[0]), tuning->params.isp_3a_settings.ae_table_preview,
				sizeof(isp_ae_preview_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_CAPTURE_TBL) /* isp_ae_capture_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_capture_tbl = (struct isp_ae_table_cfg *)data_ptr;
			isp_ae_capture_tbl->length = tuning->params.isp_3a_settings.ae_table_capture_length;
			memcpy(&(isp_ae_capture_tbl->value[0]), tuning->params.isp_3a_settings.ae_table_capture,
				sizeof(isp_ae_capture_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_VIDEO_TBL) /* isp_ae_video_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_video_tbl = (struct isp_ae_table_cfg *)data_ptr;
			isp_ae_video_tbl->length = tuning->params.isp_3a_settings.ae_table_video_length;
			memcpy(&(isp_ae_video_tbl->value[0]), tuning->params.isp_3a_settings.ae_table_video,
				sizeof(isp_ae_video_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_WIN_WEIGHT) /* isp_ae_win_weight */
		{
			memcpy(data_ptr, tuning->params.isp_3a_settings.ae_win_weight, sizeof(struct isp_ae_weight_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_ae_weight_cfg);
			ret += sizeof(struct isp_ae_weight_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_DELAY) /* isp_ae_delay */
		{
			struct isp_ae_delay_cfg *isp_ae_delay = (struct isp_ae_delay_cfg *)data_ptr;
			isp_ae_delay->exp_frame = tuning->params.isp_3a_settings.exp_delay_frame;
			isp_ae_delay->gain_frame = tuning->params.isp_3a_settings.gain_delay_frame;

			/* offset */
			data_ptr += sizeof(struct isp_ae_delay_cfg);
			ret += sizeof(struct isp_ae_delay_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_SPEED) /* isp_awb_speed */
		{
			struct isp_awb_speed_cfg *isp_awb_speed = (struct isp_awb_speed_cfg *)data_ptr;
			isp_awb_speed->interval = tuning->params.isp_3a_settings.awb_interval;
			isp_awb_speed->value = tuning->params.isp_3a_settings.awb_speed;

			/* offset */
			data_ptr += sizeof(struct isp_awb_speed_cfg);
			ret += sizeof(struct isp_awb_speed_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_TEMP_RANGE) /* isp_awb_temp_range */
		{
			struct isp_awb_temp_range_cfg *isp_awb_temp_range = (struct isp_awb_temp_range_cfg *)data_ptr;
			isp_awb_temp_range->low = tuning->params.isp_3a_settings.awb_color_temper_low;
			isp_awb_temp_range->high = tuning->params.isp_3a_settings.awb_color_temper_high;
			isp_awb_temp_range->base = tuning->params.isp_3a_settings.awb_base_temper;

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_range_cfg);
			ret += sizeof(struct isp_awb_temp_range_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_LIGHT_INFO) /* isp_awb_light_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_light_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			isp_awb_light_info->number = tuning->params.isp_3a_settings.awb_light_num;
			memcpy(isp_awb_light_info->value, tuning->params.isp_3a_settings.awb_light_info,
				sizeof(tuning->params.isp_3a_settings.awb_light_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_EXT_LIGHT_INFO) /* isp_awb_ext_light_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_ext_light_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			isp_awb_ext_light_info->number = tuning->params.isp_3a_settings.awb_ext_light_num;
			memcpy(isp_awb_ext_light_info->value, tuning->params.isp_3a_settings.awb_ext_light_info,
				sizeof(tuning->params.isp_3a_settings.awb_ext_light_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_SKIN_INFO) /* isp_awb_skin_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_skin_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			isp_awb_skin_info->number = tuning->params.isp_3a_settings.awb_skin_color_num;
			memcpy(isp_awb_skin_info->value, tuning->params.isp_3a_settings.awb_skin_color_info,
				sizeof(tuning->params.isp_3a_settings.awb_skin_color_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_SPECIAL_INFO) /* isp_awb_special_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_special_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			isp_awb_special_info->number = tuning->params.isp_3a_settings.awb_special_color_num;
			memcpy(isp_awb_special_info->value, tuning->params.isp_3a_settings.awb_special_color_info,
				sizeof(tuning->params.isp_3a_settings.awb_special_color_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_PRESET_GAIN) /* isp_awb_preset_gain */
		{
			memcpy(data_ptr, tuning->params.isp_3a_settings.awb_preset_gain, sizeof(struct isp_awb_preset_gain_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_awb_preset_gain_cfg);
			ret += sizeof(struct isp_awb_preset_gain_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_FAVOR) /* isp_awb_favor */
		{
			struct isp_awb_favor_cfg *isp_awb_favor = (struct isp_awb_favor_cfg *)data_ptr;
			isp_awb_favor->rgain = tuning->params.isp_3a_settings.awb_rgain_favor;
			isp_awb_favor->bgain = tuning->params.isp_3a_settings.awb_bgain_favor;

			/* offset */
			data_ptr += sizeof(struct isp_awb_favor_cfg);
			ret += sizeof(struct isp_awb_favor_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_VCM_CODE) /* isp_af_vcm_code */
		{
			struct isp_af_vcm_code_cfg *isp_af_vcm_code = (struct isp_af_vcm_code_cfg *)data_ptr;
			isp_af_vcm_code->min = tuning->params.isp_3a_settings.vcm_min_code;
			isp_af_vcm_code->max = tuning->params.isp_3a_settings.vcm_max_code;

			/* offset */
			data_ptr += sizeof(struct isp_af_vcm_code_cfg);
			ret += sizeof(struct isp_af_vcm_code_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_OTP) /* isp_af_otp */
		{
			struct isp_af_otp_cfg *isp_af_otp = (struct isp_af_otp_cfg *)data_ptr;
			isp_af_otp->use_otp = tuning->params.isp_3a_settings.af_use_otp;

			/* offset */
			data_ptr += sizeof(struct isp_af_otp_cfg);
			ret += sizeof(struct isp_af_otp_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_SPEED) /* isp_af_speed */
		{
			struct isp_af_speed_cfg *isp_af_speed = (struct isp_af_speed_cfg *)data_ptr;
			isp_af_speed->interval_time = tuning->params.isp_3a_settings.af_interval_time;
			isp_af_speed->index = tuning->params.isp_3a_settings.af_speed_ind;

			/* offset */
			data_ptr += sizeof(struct isp_af_speed_cfg);
			ret += sizeof(struct isp_af_speed_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_FINE_SEARCH) /* isp_af_fine_search */
		{
			struct isp_af_fine_search_cfg *isp_af_fine_search = (struct isp_af_fine_search_cfg *)data_ptr;
			isp_af_fine_search->auto_en = tuning->params.isp_3a_settings.af_auto_fine_en;
			isp_af_fine_search->single_en = tuning->params.isp_3a_settings.af_single_fine_en;
			isp_af_fine_search->step = tuning->params.isp_3a_settings.af_fine_step;

			/* offset */
			data_ptr += sizeof(struct isp_af_fine_search_cfg);
			ret += sizeof(struct isp_af_fine_search_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_REFOCUS) /* isp_af_refocus */
		{
			struct isp_af_refocus_cfg *isp_af_refocus = (struct isp_af_refocus_cfg *)data_ptr;
			isp_af_refocus->move_cnt = tuning->params.isp_3a_settings.af_move_cnt;
			isp_af_refocus->still_cnt = tuning->params.isp_3a_settings.af_still_cnt;
			isp_af_refocus->move_monitor_cnt = tuning->params.isp_3a_settings.af_move_monitor_cnt;
			isp_af_refocus->still_monitor_cnt = tuning->params.isp_3a_settings.af_still_monitor_cnt;

			/* offset */
			data_ptr += sizeof(struct isp_af_refocus_cfg);
			ret += sizeof(struct isp_af_refocus_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_TOLERANCE) /* isp_af_tolerance */
		{
			struct isp_af_tolerance_cfg *isp_af_tolerance = (struct isp_af_tolerance_cfg *)data_ptr;
			isp_af_tolerance->near_distance = tuning->params.isp_3a_settings.af_near_tolerance;
			isp_af_tolerance->far_distance = tuning->params.isp_3a_settings.af_far_tolerance;
			isp_af_tolerance->offset = tuning->params.isp_3a_settings.af_tolerance_off;
			isp_af_tolerance->table_length = tuning->params.isp_3a_settings.af_tolerance_tbl_len;
			memcpy(isp_af_tolerance->std_code_table, tuning->params.isp_3a_settings.af_std_code_tbl,
				sizeof(isp_af_tolerance->std_code_table));
			memcpy(isp_af_tolerance->value, tuning->params.isp_3a_settings.af_tolerance_value_tbl,
				sizeof(isp_af_tolerance->value));

			/* offset */
			data_ptr += sizeof(struct isp_af_tolerance_cfg);
			ret += sizeof(struct isp_af_tolerance_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_SCENE) /* isp_af_scene */
		{
			struct isp_af_scene_cfg *isp_af_scene = (struct isp_af_scene_cfg *)data_ptr;
			isp_af_scene->stable_min = tuning->params.isp_3a_settings.af_stable_min;
			isp_af_scene->stable_max = tuning->params.isp_3a_settings.af_stable_max;
			isp_af_scene->low_light_lv = tuning->params.isp_3a_settings.af_low_light_lv;
			isp_af_scene->peak_thres = tuning->params.isp_3a_settings.af_peak_th;
			isp_af_scene->direction_thres = tuning->params.isp_3a_settings.af_dir_th;
			isp_af_scene->change_ratio = tuning->params.isp_3a_settings.af_change_ratio;
			isp_af_scene->move_minus = tuning->params.isp_3a_settings.af_move_minus;
			isp_af_scene->still_minus = tuning->params.isp_3a_settings.af_still_minus;
			isp_af_scene->scene_motion_thres = tuning->params.isp_3a_settings.af_scene_motion_th;

			/* offset */
			data_ptr += sizeof(struct isp_af_scene_cfg);
			ret += sizeof(struct isp_af_scene_cfg);
		}
		
		break;

	case HW_ISP_CFG_TUNING: /* isp_tunning_param */
		if (cfg_ids & HW_ISP_CFG_TUNING_FLASH) /* isp_flash */
		{
			struct isp_flash_cfg *isp_flash = (struct isp_flash_cfg *)data_ptr;
			isp_flash->gain = tuning->params.isp_tunning_settings.flash_gain;
			isp_flash->delay_frame = tuning->params.isp_tunning_settings.flash_delay_frame;

			/* offset */
			data_ptr += sizeof(struct isp_flash_cfg);
			ret += sizeof(struct isp_flash_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_FLICKER) /* isp_flicker */
		{
			struct isp_flicker_cfg *isp_flicker = (struct isp_flicker_cfg *)data_ptr;
			isp_flicker->type = tuning->params.isp_tunning_settings.flicker_type;
			isp_flicker->ratio = tuning->params.isp_tunning_settings.flicker_ratio;

			/* offset */
			data_ptr += sizeof(struct isp_flicker_cfg);
			ret += sizeof(struct isp_flicker_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_DEFOG) /* isp_defog */
		{
			struct isp_defog_cfg *isp_defog = (struct isp_defog_cfg *)data_ptr;
			isp_defog->strength = tuning->params.isp_tunning_settings.defog_value;

			/* offset */
			data_ptr += sizeof(struct isp_defog_cfg);
			ret += sizeof(struct isp_defog_cfg);
		}
		
		if (cfg_ids & HW_ISP_CFG_TUNING_VISUAL_ANGLE) /* isp_visual_angle */
		{
			struct isp_visual_angle_cfg *isp_visual_angle = (struct isp_visual_angle_cfg *)data_ptr;
			isp_visual_angle->horizontal = tuning->params.isp_tunning_settings.hor_visual_angle;
			isp_visual_angle->vertical = tuning->params.isp_tunning_settings.ver_visual_angle;
			isp_visual_angle->focus_length = tuning->params.isp_tunning_settings.focus_length;

			/* offset */
			data_ptr += sizeof(struct isp_visual_angle_cfg);
			ret += sizeof(struct isp_visual_angle_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_GTM) /* isp_gtm */
		{
			struct isp_gtm_cfg *isp_gtm = (struct isp_gtm_cfg *)data_ptr;
			isp_gtm->type = tuning->params.isp_tunning_settings.gtm_type;
			isp_gtm->gamma_type = tuning->params.isp_tunning_settings.gamma_type;
			isp_gtm->auto_alpha_en = tuning->params.isp_tunning_settings.auto_alpha_en;

			/* offset */
			data_ptr += sizeof(struct isp_gtm_cfg);
			ret += sizeof(struct isp_gtm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_DPC_OTF) /* isp_dpc_otf */
		{
			struct isp_dpc_otf_cfg *isp_dpc_otf = (struct isp_dpc_otf_cfg *)data_ptr;
			isp_dpc_otf->thres_slop = tuning->params.isp_tunning_settings.dpc_th_slop;
			isp_dpc_otf->min_thres = tuning->params.isp_tunning_settings.dpc_otf_min_th;
			isp_dpc_otf->max_thres = tuning->params.isp_tunning_settings.dpc_otf_max_th;
			isp_dpc_otf->cfa_dir_thres = tuning->params.isp_tunning_settings.cfa_dir_th;

			/* offset */
			data_ptr += sizeof(struct isp_dpc_otf_cfg);
			ret += sizeof(struct isp_dpc_otf_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_GAIN_OFFSET) /* isp_gain_offset */
		{
			//memcpy(data_ptr, tuning->params.isp_tunning_settings.bayer_gain_offset, sizeof(struct isp_gain_offset_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_gain_offset_cfg);
			ret += sizeof(struct isp_gain_offset_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_LOW) /* isp_ccm_low */
		{
			struct isp_ccm_cfg *isp_ccm_low = (struct isp_ccm_cfg *)data_ptr;
			isp_ccm_low->temperature = tuning->params.isp_tunning_settings.cm_trig_cfg[0];
			memcpy(&isp_ccm_low->value, &(tuning->params.isp_tunning_settings.color_matrix_ini[0]),
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_ccm_cfg);
			ret += sizeof(struct isp_ccm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_MID) /* isp_ccm_mid */
		{
			struct isp_ccm_cfg *isp_ccm_mid = (struct isp_ccm_cfg *)data_ptr;
			isp_ccm_mid->temperature = tuning->params.isp_tunning_settings.cm_trig_cfg[1];
			memcpy(&isp_ccm_mid->value, &(tuning->params.isp_tunning_settings.color_matrix_ini[1]),
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_ccm_cfg);
			ret += sizeof(struct isp_ccm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_HIGH) /* isp_ccm_high */
		{
			struct isp_ccm_cfg *isp_ccm_high = (struct isp_ccm_cfg *)data_ptr;
			isp_ccm_high->temperature = tuning->params.isp_tunning_settings.cm_trig_cfg[2];
			memcpy(&isp_ccm_high->value, &(tuning->params.isp_tunning_settings.color_matrix_ini[2]),
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_ccm_cfg);
			ret += sizeof(struct isp_ccm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_LSC) /* isp_lsc */
		{
			struct isp_lens_shading_cfg *isp_lsc = (struct isp_lens_shading_cfg *)data_ptr;
			isp_lsc->ff_mod = tuning->params.isp_tunning_settings.ff_mod;
			isp_lsc->center_x = tuning->params.isp_tunning_settings.lsc_center_x;
			isp_lsc->center_y = tuning->params.isp_tunning_settings.lsc_center_y;
			memcpy(&(isp_lsc->value[0][0]), &(tuning->params.isp_tunning_settings.lsc_tbl[0][0]),
				sizeof(isp_lsc->value));
			memcpy(isp_lsc->color_temp_triggers, tuning->params.isp_tunning_settings.lsc_trig_cfg,
				sizeof(isp_lsc->color_temp_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_lens_shading_cfg);
			ret += sizeof(struct isp_lens_shading_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_GAMMA) /* isp_gamma */
		{
			struct isp_gamma_table_cfg *isp_gamma = (struct isp_gamma_table_cfg *)data_ptr;
			isp_gamma->number = tuning->params.isp_tunning_settings.gamma_num;
			memcpy(&(isp_gamma->value[0][0]), &(tuning->params.isp_tunning_settings.gamma_tbl_ini[0][0]),
				sizeof(isp_gamma->value));
			memcpy(isp_gamma->lv_triggers, tuning->params.isp_tunning_settings.gamma_trig_cfg,
				sizeof(isp_gamma->lv_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_gamma_table_cfg);
			ret += sizeof(struct isp_gamma_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_LINEARITY) /* isp_linearity */
		{
			memcpy(data_ptr, tuning->params.isp_tunning_settings.linear_tbl, sizeof(struct isp_linearity_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_linearity_cfg);
			ret += sizeof(struct isp_linearity_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_DISTORTION) /* isp_distortion */
		{
			memcpy(data_ptr, tuning->params.isp_tunning_settings.disc_tbl, sizeof(struct isp_distortion_cfg));
			
			/* offset */
			data_ptr += sizeof(struct isp_distortion_cfg);
			ret += sizeof(struct isp_distortion_cfg);
		}

		break;

	case HW_ISP_CFG_DYNAMIC: /* isp_dynamic_param */
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_TRIGGER) /* isp_trigger */
		{
			memcpy(data_ptr, &tuning->params.isp_iso_settings.triger, sizeof(isp_dynamic_triger_t));

			/* offset */
			data_ptr += sizeof(isp_dynamic_triger_t);
			ret += sizeof(isp_dynamic_triger_t);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_LUM_POINT) /* isp_lum_mapping_point */
		{
			memcpy(data_ptr, tuning->params.isp_iso_settings.isp_lum_mapping_point, sizeof(struct isp_dynamic_single_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_GAIN_POINT) /* isp_gain_mapping_point */
		{
			memcpy(data_ptr, tuning->params.isp_iso_settings.isp_gain_mapping_point, sizeof(struct isp_dynamic_single_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_SHARP) /* isp_sharp_cfg */
		{
			struct isp_dynamic_sharp_cfg *isp_sharp_cfg = (struct isp_dynamic_sharp_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(&(isp_sharp_cfg->tuning_cfg[i]), tuning->params.isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg,
				        sizeof(struct isp_dynamic_range_cfg));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_sharp_cfg);
			ret += sizeof(struct isp_dynamic_sharp_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_CONTRAST) /* isp_contrast_cfg */
		{
			struct isp_dynamic_contrast_cfg *isp_contrast_cfg = (struct isp_dynamic_contrast_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(&(isp_contrast_cfg->tuning_cfg[i]), tuning->params.isp_iso_settings.isp_dynamic_cfg[i].contrast_cfg,
					sizeof(struct isp_dynamic_range_cfg));
				isp_contrast_cfg->strength[i] = tuning->params.isp_iso_settings.isp_dynamic_cfg[i].contrast;
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_contrast_cfg);
			ret += sizeof(struct isp_dynamic_contrast_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_DENOISE) /* isp_denoise_cfg */
		{
			struct isp_dynamic_denoise_cfg *isp_denoise_cfg = (struct isp_dynamic_denoise_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(&(isp_denoise_cfg->tuning_cfg[i]), tuning->params.isp_iso_settings.isp_dynamic_cfg[i].denoise_cfg,
					sizeof(struct isp_dynamic_range_cfg));
				isp_denoise_cfg->color_denoise[i] = tuning->params.isp_iso_settings.isp_dynamic_cfg[i].color_denoise;
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_denoise_cfg);
			ret += sizeof(struct isp_dynamic_denoise_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_BRIGHTNESS) /* isp_brightness */
		{
			struct isp_dynamic_single_cfg *isp_brightness = (struct isp_dynamic_single_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				isp_brightness->value[i] = tuning->params.isp_iso_settings.isp_dynamic_cfg[i].brightness;

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_SATURATION) /* isp_saturation_cfg */
		{
			struct isp_dynamic_saturation_cfg *isp_saturation_cfg = (struct isp_dynamic_saturation_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				isp_saturation_cfg->value[i].cb = tuning->params.isp_iso_settings.isp_dynamic_cfg[i].saturation_cb;
				isp_saturation_cfg->value[i].cr = tuning->params.isp_iso_settings.isp_dynamic_cfg[i].saturation_cr;
				memcpy(isp_saturation_cfg->value[i].value, tuning->params.isp_iso_settings.isp_dynamic_cfg[i].saturation_cfg,
					sizeof(isp_saturation_cfg->value[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_saturation_cfg);
			ret += sizeof(struct isp_dynamic_saturation_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_TDF) /* isp_tdf_cfg */
		{
			struct isp_dynamic_tdf_cfg *isp_tdf_cfg = (struct isp_dynamic_tdf_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_tdf_cfg->value[i], tuning->params.isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg,
					sizeof(isp_tdf_cfg->value[i]));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_tdf_cfg);
			ret += sizeof(struct isp_dynamic_tdf_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_AE) /* isp_ae_cfg */
		{
			struct isp_dynamic_ae_cfg *isp_ae_cfg = (struct isp_dynamic_ae_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(&(isp_ae_cfg->tuning_cfg[i]), tuning->params.isp_iso_settings.isp_dynamic_cfg[i].ae_cfg,
					sizeof(struct isp_dynamic_ae_tuning_cfg));
			
			/* offset */
			data_ptr += sizeof(struct isp_dynamic_ae_cfg);
			ret += sizeof(struct isp_dynamic_ae_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_GTM) /* isp_gtm_cfg */
		{
			struct isp_dynamic_gtm_cfg *isp_gtm_cfg = (struct isp_dynamic_gtm_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_gtm_cfg->value[i], tuning->params.isp_iso_settings.isp_dynamic_cfg[i].gtm_cfg,
					sizeof(isp_gtm_cfg->value[i]));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_gtm_cfg);
			ret += sizeof(struct isp_dynamic_gtm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_RESERVED) /* isp_reserved */
		{
			struct isp_dynamic_reserved *isp_reserved = (struct isp_dynamic_reserved *)data_ptr;
			int i = 0;
			/*for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_reserved->value[i], tuning->params.isp_iso_settings.isp_dynamic_cfg[i].reserved,
					sizeof(isp_reserved->value[i]));*/

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_reserved);
			ret += sizeof(struct isp_dynamic_reserved);
		}

		break;

	default:
		ret = -3;
		break;
	}

	data_ptr = NULL;
	
	return ret;
}

HW_S32 hw_set_isp_cfg(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data)
{
	int ret = -1;
	unsigned char *data_ptr = NULL;
	struct isp_tuning *tuning = NULL;


	if (!isp || !cfg_data)
		return -1;

	/* call isp api */
	tuning = isp_dev_get_tuning(isp);
	if (tuning == NULL)
		 return -2;

	
	if (tuning->ctx == NULL)
		 return -3;

	/* fill cfg data */
	ret = 0;
	data_ptr = (unsigned char *)cfg_data;

	switch (group_id)
	{
	case HW_ISP_CFG_TEST: /* isp_test_param */
		if (cfg_ids & HW_ISP_CFG_TEST_PUB) /* isp_test_pub */
		{
			struct isp_test_pub_cfg *isp_test_pub = (struct isp_test_pub_cfg *)data_ptr;				
			tuning->params.isp_test_settings.isp_test_mode = isp_test_pub->test_mode;
			tuning->params.isp_test_settings.isp_gain = isp_test_pub->gain;
			tuning->params.isp_test_settings.isp_exp_line = isp_test_pub->exp_line;
			tuning->params.isp_test_settings.isp_color_temp = isp_test_pub->color_temp;
			tuning->params.isp_test_settings.isp_log_param = isp_test_pub->log_param;

			/* offset */
			data_ptr += sizeof(struct isp_test_pub_cfg);
			ret += sizeof(struct isp_test_pub_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TEST_EXPTIME) /* isp_test_exptime */
		{
			struct isp_test_item_cfg *isp_test_exptime = (struct isp_test_item_cfg *)data_ptr;
			tuning->params.isp_test_settings.isp_test_exptime = isp_test_exptime->enable;
			tuning->params.isp_test_settings.exp_line_start = isp_test_exptime->start;
			tuning->params.isp_test_settings.exp_line_step = isp_test_exptime->step;
			tuning->params.isp_test_settings.exp_line_end = isp_test_exptime->end;
			tuning->params.isp_test_settings.exp_change_interval = isp_test_exptime->change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		
		if (cfg_ids & HW_ISP_CFG_TEST_GAIN) /* isp_test_gain */
		{
			struct isp_test_item_cfg *isp_test_gain = (struct isp_test_item_cfg *)data_ptr;
			tuning->params.isp_test_settings.isp_test_gain = isp_test_gain->enable;
			tuning->params.isp_test_settings.gain_start = isp_test_gain->start;
			tuning->params.isp_test_settings.gain_step = isp_test_gain->step;
			tuning->params.isp_test_settings.gain_end = isp_test_gain->end;
			tuning->params.isp_test_settings.gain_change_interval = isp_test_gain->change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		
		if (cfg_ids & HW_ISP_CFG_TEST_FOCUS) /* isp_test_focus */
		{
			struct isp_test_item_cfg *isp_test_focus = (struct isp_test_item_cfg *)data_ptr;
			tuning->params.isp_test_settings.isp_test_focus = isp_test_focus->enable;
			tuning->params.isp_test_settings.focus_start = isp_test_focus->start;
			tuning->params.isp_test_settings.focus_step = isp_test_focus->step;
			tuning->params.isp_test_settings.focus_end = isp_test_focus->end;
			tuning->params.isp_test_settings.focus_change_interval = isp_test_focus->change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TEST_FORCED) /* isp_test_forced */
		{
			struct isp_test_forced_cfg *isp_test_forced = (struct isp_test_forced_cfg *)data_ptr;
			tuning->params.isp_test_settings.ae_forced = isp_test_forced->ae_enable;
			tuning->params.isp_test_settings.lum_forced = isp_test_forced->lum;

			/* offset */
			data_ptr += sizeof(struct isp_test_forced_cfg);
			ret += sizeof(struct isp_test_forced_cfg);
		}
		
		if (cfg_ids & HW_ISP_CFG_TEST_ENABLE) /* isp_test_enable */
		{
			memcpy(&(tuning->params.isp_test_settings.manual_en), data_ptr, sizeof(struct isp_test_enable_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_test_enable_cfg);
			ret += sizeof(struct isp_test_enable_cfg);
		}
		break;

	case HW_ISP_CFG_3A: /* isp_3a_param */
		if (cfg_ids & HW_ISP_CFG_AE_PUB) /* isp_ae_pub */
		{
			struct isp_ae_pub_cfg *isp_ae_pub = (struct isp_ae_pub_cfg *)data_ptr;
			tuning->params.isp_3a_settings.define_ae_table = isp_ae_pub->define_table;
			tuning->params.isp_3a_settings.ae_max_lv = isp_ae_pub->max_lv;
			//tuning->params.isp_3a_settings.fno = isp_ae_pub->aperture;
			tuning->params.isp_3a_settings.ae_hist_mod_en = isp_ae_pub->hist_mode_en;
			tuning->params.isp_3a_settings.exp_comp_step = isp_ae_pub->compensation_step;
			tuning->params.isp_3a_settings.ae_touch_dist_ind = isp_ae_pub->touch_dist_index;

			/* offset */
			data_ptr += sizeof(struct isp_ae_pub_cfg);
			ret += sizeof(struct isp_ae_pub_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_PREVIEW_TBL) /* isp_ae_preview_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_preview_tbl = (struct isp_ae_table_cfg *)data_ptr;
			
			tuning->params.isp_3a_settings.ae_table_preview_length = isp_ae_preview_tbl->length;
			memcpy(tuning->params.isp_3a_settings.ae_table_preview, &(isp_ae_preview_tbl->value[0]),
				sizeof(isp_ae_preview_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_CAPTURE_TBL) /* isp_ae_capture_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_capture_tbl = (struct isp_ae_table_cfg *)data_ptr;
		
			tuning->params.isp_3a_settings.ae_table_capture_length = isp_ae_capture_tbl->length;
			memcpy(tuning->params.isp_3a_settings.ae_table_capture, &(isp_ae_capture_tbl->value[0]),
				sizeof(isp_ae_capture_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_VIDEO_TBL) /* isp_ae_video_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_video_tbl = (struct isp_ae_table_cfg *)data_ptr;
			
			tuning->params.isp_3a_settings.ae_table_video_length = isp_ae_video_tbl->length;
			memcpy(tuning->params.isp_3a_settings.ae_table_video, &(isp_ae_video_tbl->value[0]),
				sizeof(isp_ae_video_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_WIN_WEIGHT) /* isp_ae_win_weight */
		{
			memcpy(tuning->params.isp_3a_settings.ae_win_weight, data_ptr, sizeof(struct isp_ae_weight_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_ae_weight_cfg);
			ret += sizeof(struct isp_ae_weight_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AE_DELAY) /* isp_ae_delay */
		{
			struct isp_ae_delay_cfg *isp_ae_delay = (struct isp_ae_delay_cfg *)data_ptr;
			tuning->params.isp_3a_settings.exp_delay_frame = isp_ae_delay->exp_frame;
			tuning->params.isp_3a_settings.gain_delay_frame = isp_ae_delay->gain_frame;

			/* offset */
			data_ptr += sizeof(struct isp_ae_delay_cfg);
			ret += sizeof(struct isp_ae_delay_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_SPEED) /* isp_awb_speed */
		{
			struct isp_awb_speed_cfg *isp_awb_speed = (struct isp_awb_speed_cfg *)data_ptr;
			tuning->params.isp_3a_settings.awb_interval = isp_awb_speed->interval;
			tuning->params.isp_3a_settings.awb_speed = isp_awb_speed->value;

			/* offset */
			data_ptr += sizeof(struct isp_awb_speed_cfg);
			ret += sizeof(struct isp_awb_speed_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_TEMP_RANGE) /* isp_awb_temp_range */
		{
			struct isp_awb_temp_range_cfg *isp_awb_temp_range = (struct isp_awb_temp_range_cfg *)data_ptr;
			tuning->params.isp_3a_settings.awb_color_temper_low = isp_awb_temp_range->low;
			tuning->params.isp_3a_settings.awb_color_temper_high = isp_awb_temp_range->high;
			tuning->params.isp_3a_settings.awb_base_temper = isp_awb_temp_range->base;

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_range_cfg);
			ret += sizeof(struct isp_awb_temp_range_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_LIGHT_INFO) /* isp_awb_light_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_light_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			tuning->params.isp_3a_settings.awb_light_num = isp_awb_light_info->number;
			memcpy(tuning->params.isp_3a_settings.awb_light_info, isp_awb_light_info->value,
				sizeof(tuning->params.isp_3a_settings.awb_light_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_EXT_LIGHT_INFO) /* isp_awb_ext_light_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_ext_light_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			tuning->params.isp_3a_settings.awb_ext_light_num = isp_awb_ext_light_info->number;
			memcpy(tuning->params.isp_3a_settings.awb_ext_light_info, isp_awb_ext_light_info->value,
				sizeof(tuning->params.isp_3a_settings.awb_ext_light_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_SKIN_INFO) /* isp_awb_skin_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_skin_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			tuning->params.isp_3a_settings.awb_skin_color_num = isp_awb_skin_info->number;
			memcpy(tuning->params.isp_3a_settings.awb_skin_color_info, isp_awb_skin_info->value,
				sizeof(tuning->params.isp_3a_settings.awb_skin_color_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_SPECIAL_INFO) /* isp_awb_special_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_special_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			tuning->params.isp_3a_settings.awb_special_color_num = isp_awb_special_info->number;
			memcpy(tuning->params.isp_3a_settings.awb_special_color_info, isp_awb_special_info->value,
				sizeof(tuning->params.isp_3a_settings.awb_special_color_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_PRESET_GAIN) /* isp_awb_preset_gain */
		{
			memcpy(tuning->params.isp_3a_settings.awb_preset_gain, data_ptr, sizeof(struct isp_awb_preset_gain_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_awb_preset_gain_cfg);
			ret += sizeof(struct isp_awb_preset_gain_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AWB_FAVOR) /* isp_awb_favor */
		{
			struct isp_awb_favor_cfg *isp_awb_favor = (struct isp_awb_favor_cfg *)data_ptr;
			tuning->params.isp_3a_settings.awb_rgain_favor = isp_awb_favor->rgain;
			tuning->params.isp_3a_settings.awb_bgain_favor = isp_awb_favor->bgain;

			/* offset */
			data_ptr += sizeof(struct isp_awb_favor_cfg);
			ret += sizeof(struct isp_awb_favor_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_VCM_CODE) /* isp_af_vcm_code */
		{
			struct isp_af_vcm_code_cfg *isp_af_vcm_code = (struct isp_af_vcm_code_cfg *)data_ptr;
			tuning->params.isp_3a_settings.vcm_min_code = isp_af_vcm_code->min;
			tuning->params.isp_3a_settings.vcm_max_code = isp_af_vcm_code->max;

			/* offset */
			data_ptr += sizeof(struct isp_af_vcm_code_cfg);
			ret += sizeof(struct isp_af_vcm_code_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_OTP) /* isp_af_otp */
		{
			struct isp_af_otp_cfg *isp_af_otp = (struct isp_af_otp_cfg *)data_ptr;
			tuning->params.isp_3a_settings.af_use_otp = isp_af_otp->use_otp;

			/* offset */
			data_ptr += sizeof(struct isp_af_otp_cfg);
			ret += sizeof(struct isp_af_otp_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_SPEED) /* isp_af_speed */
		{
			struct isp_af_speed_cfg *isp_af_speed = (struct isp_af_speed_cfg *)data_ptr;
			tuning->params.isp_3a_settings.af_interval_time = isp_af_speed->interval_time;
			tuning->params.isp_3a_settings.af_speed_ind = isp_af_speed->index;

			/* offset */
			data_ptr += sizeof(struct isp_af_speed_cfg);
			ret += sizeof(struct isp_af_speed_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_FINE_SEARCH) /* isp_af_fine_search */
		{
			struct isp_af_fine_search_cfg *isp_af_fine_search = (struct isp_af_fine_search_cfg *)data_ptr;
			tuning->params.isp_3a_settings.af_auto_fine_en = isp_af_fine_search->auto_en;
			tuning->params.isp_3a_settings.af_single_fine_en = isp_af_fine_search->single_en;
			tuning->params.isp_3a_settings.af_fine_step = isp_af_fine_search->step;

			/* offset */
			data_ptr += sizeof(struct isp_af_fine_search_cfg);
			ret += sizeof(struct isp_af_fine_search_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_REFOCUS) /* isp_af_refocus */
		{
			struct isp_af_refocus_cfg *isp_af_refocus = (struct isp_af_refocus_cfg *)data_ptr;
			tuning->params.isp_3a_settings.af_move_cnt = isp_af_refocus->move_cnt;
			tuning->params.isp_3a_settings.af_still_cnt = isp_af_refocus->still_cnt;
			tuning->params.isp_3a_settings.af_move_monitor_cnt = isp_af_refocus->move_monitor_cnt;
			tuning->params.isp_3a_settings.af_still_monitor_cnt = isp_af_refocus->still_monitor_cnt;

			/* offset */
			data_ptr += sizeof(struct isp_af_refocus_cfg);
			ret += sizeof(struct isp_af_refocus_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_TOLERANCE) /* isp_af_tolerance */
		{
			struct isp_af_tolerance_cfg *isp_af_tolerance = (struct isp_af_tolerance_cfg *)data_ptr;
			tuning->params.isp_3a_settings.af_near_tolerance = isp_af_tolerance->near_distance;
			tuning->params.isp_3a_settings.af_far_tolerance = isp_af_tolerance->far_distance;
			tuning->params.isp_3a_settings.af_tolerance_off = isp_af_tolerance->offset;
			tuning->params.isp_3a_settings.af_tolerance_tbl_len = isp_af_tolerance->table_length;
			memcpy(tuning->params.isp_3a_settings.af_std_code_tbl, isp_af_tolerance->std_code_table,
				sizeof(isp_af_tolerance->std_code_table));
			memcpy(tuning->params.isp_3a_settings.af_tolerance_value_tbl, isp_af_tolerance->value,
				sizeof(isp_af_tolerance->value));

			/* offset */
			data_ptr += sizeof(struct isp_af_tolerance_cfg);
			ret += sizeof(struct isp_af_tolerance_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_AF_SCENE) /* isp_af_scene */
		{
			struct isp_af_scene_cfg *isp_af_scene = (struct isp_af_scene_cfg *)data_ptr;
			tuning->params.isp_3a_settings.af_stable_min = isp_af_scene->stable_min;
			tuning->params.isp_3a_settings.af_stable_max = isp_af_scene->stable_max;
			tuning->params.isp_3a_settings.af_low_light_lv = isp_af_scene->low_light_lv;
			tuning->params.isp_3a_settings.af_peak_th = isp_af_scene->peak_thres;
			tuning->params.isp_3a_settings.af_dir_th = isp_af_scene->direction_thres;
			tuning->params.isp_3a_settings.af_change_ratio = isp_af_scene->change_ratio;
			tuning->params.isp_3a_settings.af_move_minus = isp_af_scene->move_minus;
			tuning->params.isp_3a_settings.af_still_minus = isp_af_scene->still_minus;
			tuning->params.isp_3a_settings.af_scene_motion_th = isp_af_scene->scene_motion_thres;

			/* offset */
			data_ptr += sizeof(struct isp_af_scene_cfg);
			ret += sizeof(struct isp_af_scene_cfg);
		}
		
		break;

	case HW_ISP_CFG_TUNING: /* isp_tunning_param */
		if (cfg_ids & HW_ISP_CFG_TUNING_FLASH) /* isp_flash */
		{
			struct isp_flash_cfg *isp_flash = (struct isp_flash_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.flash_gain = isp_flash->gain;
			tuning->params.isp_tunning_settings.flash_delay_frame = isp_flash->delay_frame;

			/* offset */
			data_ptr += sizeof(struct isp_flash_cfg);
			ret += sizeof(struct isp_flash_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_FLICKER) /* isp_flicker */
		{
			struct isp_flicker_cfg *isp_flicker = (struct isp_flicker_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.flicker_type = isp_flicker->type;
			tuning->params.isp_tunning_settings.flicker_ratio = isp_flicker->ratio;

			/* offset */
			data_ptr += sizeof(struct isp_flicker_cfg);
			ret += sizeof(struct isp_flicker_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_DEFOG) /* isp_defog */
		{
			struct isp_defog_cfg *isp_defog = (struct isp_defog_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.defog_value = isp_defog->strength;

			/* offset */
			data_ptr += sizeof(struct isp_defog_cfg);
			ret += sizeof(struct isp_defog_cfg);
		}
		
		if (cfg_ids & HW_ISP_CFG_TUNING_VISUAL_ANGLE) /* isp_visual_angle */
		{
			struct isp_visual_angle_cfg *isp_visual_angle = (struct isp_visual_angle_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.hor_visual_angle = isp_visual_angle->horizontal;
			tuning->params.isp_tunning_settings.ver_visual_angle = isp_visual_angle->vertical;
			tuning->params.isp_tunning_settings.focus_length = isp_visual_angle->focus_length;

			/* offset */
			data_ptr += sizeof(struct isp_visual_angle_cfg);
			ret += sizeof(struct isp_visual_angle_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_GTM) /* isp_gtm */
		{
			struct isp_gtm_cfg *isp_gtm = (struct isp_gtm_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.gtm_type = isp_gtm->type;
			tuning->params.isp_tunning_settings.gamma_type = isp_gtm->gamma_type;
			tuning->params.isp_tunning_settings.auto_alpha_en = isp_gtm->auto_alpha_en;

			/* offset */
			data_ptr += sizeof(struct isp_gtm_cfg);
			ret += sizeof(struct isp_gtm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_DPC_OTF) /* isp_dpc_otf */
		{
			struct isp_dpc_otf_cfg *isp_dpc_otf = (struct isp_dpc_otf_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.dpc_th_slop = isp_dpc_otf->thres_slop;
			tuning->params.isp_tunning_settings.dpc_otf_min_th = isp_dpc_otf->min_thres;
			tuning->params.isp_tunning_settings.dpc_otf_max_th = isp_dpc_otf->max_thres;
			tuning->params.isp_tunning_settings.cfa_dir_th = isp_dpc_otf->cfa_dir_thres;

			/* offset */
			data_ptr += sizeof(struct isp_dpc_otf_cfg);
			ret += sizeof(struct isp_dpc_otf_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_GAIN_OFFSET) /* isp_gain_offset */
		{
			//memcpy(tuning->params.isp_tunning_settings.bayer_gain_offset, data_ptr, sizeof(struct isp_gain_offset_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_gain_offset_cfg);
			ret += sizeof(struct isp_gain_offset_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_LOW) /* isp_ccm_low */
		{
			struct isp_ccm_cfg *isp_ccm_low = (struct isp_ccm_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.cm_trig_cfg[0] = isp_ccm_low->temperature;
			memcpy(&(tuning->params.isp_tunning_settings.color_matrix_ini[0]), &isp_ccm_low->value,
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_ccm_cfg);
			ret += sizeof(struct isp_ccm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_MID) /* isp_ccm_mid */
		{
			struct isp_ccm_cfg *isp_ccm_mid = (struct isp_ccm_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.cm_trig_cfg[1] = isp_ccm_mid->temperature;
			memcpy(&(tuning->params.isp_tunning_settings.color_matrix_ini[1]), &isp_ccm_mid->value,
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_ccm_cfg);
			ret += sizeof(struct isp_ccm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_HIGH) /* isp_ccm_high */
		{
			struct isp_ccm_cfg *isp_ccm_high = (struct isp_ccm_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.cm_trig_cfg[2] = isp_ccm_high->temperature;
			memcpy(&(tuning->params.isp_tunning_settings.color_matrix_ini[2]), &isp_ccm_high->value,
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_ccm_cfg);
			ret += sizeof(struct isp_ccm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_LSC) /* isp_lsc */
		{
			struct isp_lens_shading_cfg *isp_lsc = (struct isp_lens_shading_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.ff_mod = isp_lsc->ff_mod;
			tuning->params.isp_tunning_settings.lsc_center_x = isp_lsc->center_x;
			tuning->params.isp_tunning_settings.lsc_center_y = isp_lsc->center_y;
			memcpy(&(tuning->params.isp_tunning_settings.lsc_tbl[0][0]), &(isp_lsc->value[0][0]),
				sizeof(isp_lsc->value));
			memcpy(tuning->params.isp_tunning_settings.lsc_trig_cfg, isp_lsc->color_temp_triggers,
				sizeof(isp_lsc->color_temp_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_lens_shading_cfg);
			ret += sizeof(struct isp_lens_shading_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_GAMMA) /* isp_gamma */
		{
			struct isp_gamma_table_cfg *isp_gamma = (struct isp_gamma_table_cfg *)data_ptr;
			tuning->params.isp_tunning_settings.gamma_num = isp_gamma->number;
			memcpy(&(tuning->params.isp_tunning_settings.gamma_tbl_ini[0][0]), &(isp_gamma->value[0][0]),
				sizeof(isp_gamma->value));
			memcpy(tuning->params.isp_tunning_settings.gamma_trig_cfg, isp_gamma->lv_triggers,
				sizeof(isp_gamma->lv_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_gamma_table_cfg);
			ret += sizeof(struct isp_gamma_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_LINEARITY) /* isp_linearity */
		{
			memcpy(tuning->params.isp_tunning_settings.linear_tbl, data_ptr, sizeof(struct isp_linearity_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_linearity_cfg);
			ret += sizeof(struct isp_linearity_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_DISTORTION) /* isp_distortion */
		{
			memcpy(tuning->params.isp_tunning_settings.disc_tbl, data_ptr, sizeof(struct isp_distortion_cfg));
			
			/* offset */
			data_ptr += sizeof(struct isp_distortion_cfg);
			ret += sizeof(struct isp_distortion_cfg);
		}

		break;

	case HW_ISP_CFG_DYNAMIC: /* isp_dynamic_param */
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_TRIGGER) /* isp_trigger */
		{
			memcpy(&tuning->params.isp_iso_settings.triger, data_ptr, sizeof(isp_dynamic_triger_t));

			/* offset */
			data_ptr += sizeof(isp_dynamic_triger_t);
			ret += sizeof(isp_dynamic_triger_t);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_LUM_POINT) /* isp_lum_mapping_point */
		{
			memcpy(tuning->params.isp_iso_settings.isp_lum_mapping_point, data_ptr, sizeof(struct isp_dynamic_single_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_GAIN_POINT) /* isp_gain_mapping_point */
		{
			memcpy(tuning->params.isp_iso_settings.isp_gain_mapping_point, data_ptr, sizeof(struct isp_dynamic_single_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_SHARP) /* isp_sharp_cfg */
		{
			struct isp_dynamic_sharp_cfg *isp_sharp_cfg = (struct isp_dynamic_sharp_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(tuning->params.isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg, &(isp_sharp_cfg->tuning_cfg[i]),
				        sizeof(struct isp_dynamic_range_cfg));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_sharp_cfg);
			ret += sizeof(struct isp_dynamic_sharp_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_CONTRAST) /* isp_contrast_cfg */
		{
			struct isp_dynamic_contrast_cfg *isp_contrast_cfg = (struct isp_dynamic_contrast_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(tuning->params.isp_iso_settings.isp_dynamic_cfg[i].contrast_cfg, &(isp_contrast_cfg->tuning_cfg[i]),
					sizeof(struct isp_dynamic_range_cfg));
				tuning->params.isp_iso_settings.isp_dynamic_cfg[i].contrast = isp_contrast_cfg->strength[i];
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_contrast_cfg);
			ret += sizeof(struct isp_dynamic_contrast_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_DENOISE) /* isp_denoise_cfg */
		{
			struct isp_dynamic_denoise_cfg *isp_denoise_cfg = (struct isp_dynamic_denoise_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(tuning->params.isp_iso_settings.isp_dynamic_cfg[i].denoise_cfg, &(isp_denoise_cfg->tuning_cfg[i]),
					sizeof(struct isp_dynamic_range_cfg));
				tuning->params.isp_iso_settings.isp_dynamic_cfg[i].color_denoise = isp_denoise_cfg->color_denoise[i];
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_denoise_cfg);
			ret += sizeof(struct isp_dynamic_denoise_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_BRIGHTNESS) /* isp_brightness */
		{
			struct isp_dynamic_single_cfg *isp_brightness = (struct isp_dynamic_single_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				tuning->params.isp_iso_settings.isp_dynamic_cfg[i].brightness = isp_brightness->value[i];

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_SATURATION) /* isp_saturation_cfg */
		{
			struct isp_dynamic_saturation_cfg *isp_saturation_cfg = (struct isp_dynamic_saturation_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				tuning->params.isp_iso_settings.isp_dynamic_cfg[i].saturation_cb = isp_saturation_cfg->value[i].cb;
				tuning->params.isp_iso_settings.isp_dynamic_cfg[i].saturation_cr = isp_saturation_cfg->value[i].cr;
				memcpy(tuning->params.isp_iso_settings.isp_dynamic_cfg[i].saturation_cfg, isp_saturation_cfg->value[i].value,
					sizeof(isp_saturation_cfg->value[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_saturation_cfg);
			ret += sizeof(struct isp_dynamic_saturation_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_TDF) /* isp_tdf_cfg */
		{
			struct isp_dynamic_tdf_cfg *isp_tdf_cfg = (struct isp_dynamic_tdf_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(tuning->params.isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg, isp_tdf_cfg->value[i],
					sizeof(isp_tdf_cfg->value[i]));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_tdf_cfg);
			ret += sizeof(struct isp_dynamic_tdf_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_AE) /* isp_ae_cfg */
		{
			struct isp_dynamic_ae_cfg *isp_ae_cfg = (struct isp_dynamic_ae_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(tuning->params.isp_iso_settings.isp_dynamic_cfg[i].ae_cfg, &(isp_ae_cfg->tuning_cfg[i]),
					sizeof(struct isp_dynamic_ae_tuning_cfg));
			
			/* offset */
			data_ptr += sizeof(struct isp_dynamic_ae_cfg);
			ret += sizeof(struct isp_dynamic_ae_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_GTM) /* isp_gtm_cfg */
		{
			struct isp_dynamic_gtm_cfg *isp_gtm_cfg = (struct isp_dynamic_gtm_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(tuning->params.isp_iso_settings.isp_dynamic_cfg[i].gtm_cfg, isp_gtm_cfg->value[i],
					sizeof(isp_gtm_cfg->value[i]));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_gtm_cfg);
			ret += sizeof(struct isp_dynamic_gtm_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_DYNAMIC_RESERVED) /* isp_reserved */
		{
			struct isp_dynamic_reserved *isp_reserved = (struct isp_dynamic_reserved *)data_ptr;
			int i = 0;
			/*for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(tuning->params.isp_iso_settings.isp_dynamic_cfg[i].reserved, isp_reserved->value[i],
					sizeof(isp_reserved->value[i]));*/

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_reserved);
			ret += sizeof(struct isp_dynamic_reserved);
		}

		break;

	default:
		ret = -3;
		break;
	}

	/* call isp api to update isp params */
	//pthread_mutex_lock(&tuning->mutex);

	tuning->ctx->isp_ini_cfg = tuning->params;
	//pthread_mutex_unlock(&tuning->mutex);
	
	return isp_ctx_hardware_update(tuning->ctx);
}
 

