
/*
 ***************************************************************************************
 *
 * isp_3a_ae.h
 *
 * Hawkview ISP - isp_3a_ae.h module
 *
 * Copyright (c) 2015 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2015/11/04	ISP Tuning Tools Support
 *
 ****************************************************************************************
 */

#ifndef _ISP_3A_AE_H_
#define _ISP_3A_AE_H_

#if (ISP_VERSION == 522)
#define ISP_LIB_USE_FLASH	1
#else
#define ISP_LIB_USE_FLASH	0
#endif

/* AE */
#define AE_FNO_STEP	16
#define ISP_AE_START_FRAME_NUM	3
#define ISP_SAVE_SETTING_NUM 5
/*
 *  Exposure metering
 */
enum ae_metering_mode {
	AE_METERING_MODE_AVERAGE = 0,
	AE_METERING_MODE_CENTER = 1,
	AE_METERING_MODE_SPOT = 2,
	AE_METERING_MODE_MATRIX	= 3,
};

enum exposure_mode {
	EXP_AUTO         = 0,
	EXP_MANUAL       = 1,
	SHUTTER_PRIORITY,
	APERTURE_PRIORITY,
};

enum ae_mode {
	AE_NORM	= 0,
	AE_WDR	= 1,
};

enum light_mode {
	NORMAL_LIGHT = 0,
	HI_LIGHT_PRIORITY,
	LOW_LIGHT_PRIORITY,
};

enum iso_mode {
	ISO_MANUAL         = 0,
	ISO_AUTO,
};

enum flash_mode {
	FLASH_MODE_OFF      = 0,
	FLASH_MODE_ON       = 1,
	FLASH_MODE_TORCH    = 2,
	FLASH_MODE_AUTO     = 3,
	FLASH_MODE_RED_EYE  = 4,
	FLASH_MODE_NONE	    = 5,
};

enum isp_flash_ctrl {
	ISP_CTRL_FLASH_OFF = 0,
	ISP_CTRL_FLASH_ON  = 1,
	ISP_CTRL_TORCH_ON  = 2,
};

enum ae_table_change {
	AE_TABLE_EXP_CHANGED = 0,
	AE_TABLE_IRIS_CHANGED = 1,
	AE_TABLE_GAIN_CHANGED = 2,
};

enum ae_status {
	AE_STATUS_IDLE = 0,
	AE_STATUS_BUSY = 1,
	AE_STATUS_DONE = 2,
};

struct ev_setting {
	HW_U32 ev_exposure_time;  //us
	HW_U32 ev_analog_gain;
	HW_U32 ev_digital_gain;
	HW_U32 ev_total_gain;// ev_total_gain = ev_analog_gain x ev_digital_gain
	HW_U32 ev_sensor_exp_line;
	HW_U32 ev_sensor_true_exp_line;
	HW_U32 ev_f_number;	//x100
	HW_U32 ev_fno2;	//x10000

	HW_U32 ev_av;
	HW_U32 ev_tv;
	HW_U32 ev_sv;
	HW_S32 ev_lv;
	HW_U32 ev;

	HW_S32 ev_idx;
};

typedef struct ev_setting_save {
	/* AE_WDR_CHANGE */
	HW_S32 wdr_ratio_hardware;
	/* AE_DG_CHANGE */
	HW_U32 ev_digital_gain;
	HW_U32 ev_digital_gain_short;
	/* gain_delay */
	HW_U32 ev_analog_gain;
	HW_U32 ev_analog_gain_short;
	/* exp_delay */
	HW_U32 ev_sensor_exp_line;
	HW_U32 ev_sensor_exp_line_short;
	HW_U32 ev_exposure_time;
	HW_U32 ev_exposure_time_short;
	HW_S32 wdr_ratio_sensor;
} ev_setting_save_t;

/*
 *
 *   struct isp_ae_settings - Stores the exposure related settings.
 *
 */
typedef struct isp_ae_settings {
	HW_S32 exp_compensation;
	HW_U32 exp_absolute;
	HW_S32 sensor_gain;
	HW_S32 iso_sensitivity;
	HW_S32 iris_fno;

	enum ae_mode ae_mode;
	enum flash_mode flash_mode;
	enum exposure_mode exp_mode;
	enum light_mode light_mode;
	enum ae_metering_mode exp_metering_mode;
	struct isp_h3a_coor_win ae_coor;
	struct isp_h3a_coor_win hist_coor;
	enum iso_mode iso_mode;
	enum power_line_frequency flicker_mode;
	enum detected_flicker_type flicker_type;
	enum ae_table_mode scene_mode;
	enum wdr_output_mode wdr_output_select;

	bool exposure_lock;

	/*Q8 256 is 1x*/
	HW_S16 exposure_cfg[ISP_EXP_CFG_MAX];
	HW_S16 ae_hist_eq_cfg[ISP_GTM_HEQ_MAX];
	HW_S16 pltm_dynamic_cfg[ISP_PLTM_DYNAMIC_MAX];
	HW_S32 flash_open;
	bool flash_switch_flag;
	HW_S32 take_picture_flag; /* enum v4l2_take_picture */
	HW_S32 take_pic_start_cnt;
} isp_ae_settings_t;

typedef struct isp_ae_ini_cfg {
	HW_S32 define_ae_table;
	HW_S32 ae_max_lv;
	HW_S32 ae_win_weight[64];
	HW_S32 ae_hist_mod_en;
	HW_S32 ae_ev_step;
	HW_S32 ae_blowout_pre_en;
	HW_S32 ae_blowout_attr;
	HW_S32 ae_ConvDataIndex;
	HW_S32 ae_delay_frame;
	HW_S32 exp_delay_frame;
	HW_S32 gain_delay_frame;
	HW_S32 exp_comp_step;
	HW_S32 ae_touch_dist_ind;
	HW_S32 ae_handle_high_fps_en;
	HW_S32 ae_iso2gain_ratio;
	HW_S32 ae_fno_step[AE_FNO_STEP];
	HW_S16 wdr_cfg[ISP_WDR_CFG_MAX];
	HW_S32 ae_gain_favor;
	HW_S32 ae_gain_range[4];
	HW_S32 ae_total_gain_range[2];
	HW_S32 ae_digital_gain_range[2];
	HW_S32 ae_analog_gain_range[2];
	struct ae_table_info ae_tbl_scene[SCENE_MODE_MAX];
	double gain_ratio;
}ae_ini_cfg_t;

typedef enum isp_ae_param_type {
	ISP_AE_INI_DATA,
	ISP_AE_UPDATE_AE_TABLE,
	ISP_AE_SET_EXP_IDX,
	ISP_AE_BUILD_TOUCH_WEIGHT,

	ISP_AE_PARAM_TYPE_MAX,
} ae_param_type_t;

typedef enum isp_ae_check_delay_type {
	ISP_AE_DELAY_WDR_EXP_SHORT = 0,
	ISP_AE_DELAY_WDR_EXP_LONG,
	ISP_AE_DELAY_WDR_AGAIN,
	ISP_AE_DELAY_WDR_ISP_HARDWARE,
	ISP_AE_DELAY_DG_ISP_HARDWARE,
	ISP_AE_DELAY_MAX,
} isp_ae_check_delay_type_t;

typedef struct ae_test_config {
	HW_S32 isp_test_mode;
	HW_S32 isp_gain;
	HW_S32 isp_exp_line;
	HW_S32 ae_forced;
	HW_S32 lum_forced;

	HW_S32 isp_test_exptime;
	HW_S32 exp_line_start;
	HW_S32 exp_line_step;
	HW_S32 exp_line_end;
	HW_S32 exp_change_interval;

	HW_S32 isp_test_gain;
	HW_S32 gain_start;
	HW_S32 gain_step;
	HW_S32 gain_end;
	HW_S32 gain_change_interval;
	HW_S32 ae_en;
	isp_ae_check_delay_type_t ae_delay_type;
	HW_S32 ae_check_delay_en;
} ae_test_config_t;

typedef struct isp_ae_param {
	ae_param_type_t type;
	HW_S32 isp_platform_id;
	HW_S32 ae_frame_id;
	HW_S32 ae_isp_id;
	ae_ini_cfg_t ae_ini;
	isp_ae_settings_t  ae_setting;
	HW_S32 ae_pline_index;
	HW_S32 sensor_update_done;
	isp_sensor_info_t ae_sensor_info;
	ae_test_config_t test_cfg;
	int ae_target_comp;
	bool ae_save_delay_en;
	HW_U8 delay_max;
} ae_param_t;

typedef struct isp_ae_stats {
	struct isp_ae_stats_s *ae_stats;
} ae_stats_t;

typedef struct sensor_settings {
	struct ev_setting ev_set;/*for algorithm itration*/
	struct ev_setting ev_set_last;
	struct ev_setting ev_set_curr;/*for sensor really set*/

	HW_S32 ev_idx_max;
	HW_S32 ev_idx_expect;
} sensor_setting_t;

typedef struct wdr_ratio {
	HW_S32 sensor;
	HW_S32 isp_hardware;
	HW_S32 tmp;
	HW_S32 last;
} wdr_ratio_t;

typedef struct isp_ae_result {
	enum ae_status ae_status;
	sensor_setting_t sensor_set;//[2] for wdr mode
	sensor_setting_t sensor_set_short;
	ev_setting_save_t ev_set_save[ISP_SAVE_SETTING_NUM];/*for isp&ae delay*/
	HW_S32 BrightPixellValue;
	HW_S32 DarkPixelValue;

	HW_U32 ae_gain;
	HW_S32 ae_target;
	HW_S32 ae_avg_lum;
	HW_S32 ae_weight_lum;
	HW_S32 ae_delta_exp_idx;
	HW_S32 ev_lv_adj;
	HW_S32 ae_flash_ev_cumul;
	HW_U32 ae_flash_ok;
	HW_U32 ae_flash_led;

	wdr_ratio_t ae_wdr_ratio;
	HW_S32 ae_wdr_delay;
	HW_S32 wdr_hi_th;
	HW_S32 wdr_low_th;

	HW_U16 hist_low;
	HW_U16 hist_mid;
	HW_U16 hist_hi;

	HW_U8 backlight;

	double gain_ratio;
} ae_result_t;

typedef struct isp_ae_core_ops {
	HW_S32 (*isp_ae_set_params)(void *ae_core_obj, ae_param_t *param, ae_result_t *result);
	HW_S32 (*isp_ae_get_params)(void *ae_core_obj, ae_param_t **param);
	HW_S32 (*isp_ae_run)(void *ae_core_obj, ae_stats_t *stats, ae_result_t *result);
} isp_ae_core_ops_t;

void* ae_init(isp_ae_core_ops_t **ae_core_ops);
void  ae_exit(void *ae_core_obj);

#endif /*_ISP_3A_AE_H_*/

