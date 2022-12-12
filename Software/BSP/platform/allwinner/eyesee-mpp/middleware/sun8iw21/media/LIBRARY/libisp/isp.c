
/*
 ******************************************************************************
 *
 * isp.c
 *
 * Hawkview ISP - isp.c module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/05/27	VIDEO INPUT
 *
 *****************************************************************************
 */
//#define _GNU_SOURCE /*in order to use pthread_setname_np*/

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <linux/spi/spidev.h>
#include <pthread.h>

#include "include/device/isp_dev.h"
#include "isp_dev/tools.h"

#include "isp_events/events.h"
#include "isp_tuning/isp_tuning_priv.h"
#include "include/isp_tuning.h"

#include "include/isp_manage.h"
#include "isp_version.h"
#include "isp.h"

#include "iniparser/src/iniparser.h"
#include "include/isp_cmd_intf.h"
#include "include/V4l2Camera/sunxi_camera_v2.h"
#include <plat_systrace.h>


#define MEDIA_DEVICE		"/dev/media0"
/* color-0 ir-1 */
#define ISP_COLOR_MODE_DEFAULT 0

struct hw_isp_media_dev media_params;
struct isp_lib_context isp_ctx[HW_ISP_DEVICE_NUM] = {
	[0] = {
		.isp_ini_cfg = {
			.isp_test_settings = {
				.ae_en = 1,
				.awb_en = 1,
				.wb_en = 1,
				.hist_en = 1,
			},
			.isp_3a_settings = {
				.ae_stat_sel = 1,
				.ae_delay_frame = 0,
				.exp_delay_frame = 1,
				.gain_delay_frame = 1,

				.awb_interval = 2,
				.awb_speed = 32,
				.awb_stat_sel = 1,
				.awb_light_num = 9,
				.awb_light_info = {
					  254,	 256,	104,   256,   256,   256,    50,  1900,    32,	  80,
					  234,	 256,	108,   256,   256,   256,    50,  2500,    32,	  85,
					  217,	 256,	114,   256,   256,   256,    50,  2800,    32,	  90,
					  160,	 256,	153,   256,   256,   256,    50,  4000,    64,	  90,
					  141,	 256,	133,   256,   256,   256,    50,  4100,    64,	 100,
					  142,	 256,	174,   256,   256,   256,    50,  5000,   100,	 100,
					  118,	 256,	156,   256,   256,   256,    50,  5300,    64,	 100,
					  127,	 256,	195,   256,   256,   256,    50,  6500,    64,	 90,
					  115,	 256,	215,   256,   256,   256,    50,  8000,    64,	 80
				},
			},
		},
	},
};
struct isp_tuning *tuning[HW_ISP_DEVICE_NUM];
struct event_list events_arr[HW_ISP_DEVICE_NUM];

static void __ae_done(struct isp_lib_context *lib,
			ae_result_t *result __attribute__((__unused__)))
{
#if (HW_ISP_DEVICE_NUM > 1)
	if(media_params.isp_sync_mode && (lib->isp_id == 0)) {
		*result = isp_ctx[1].ae_entity_ctx.ae_result;
	}
#endif
	FUNCTION_LOG;
}
static void __af_done(struct isp_lib_context *lib,
			af_result_t *result __attribute__((__unused__)))
{
#if (HW_ISP_DEVICE_NUM > 1)
	if(media_params.isp_sync_mode && (lib->isp_id == 0)) {
		*result = isp_ctx[1].af_entity_ctx.af_result;
	}
#endif
	FUNCTION_LOG;
}
static void __awb_done(struct isp_lib_context *lib,
			awb_result_t *result __attribute__((__unused__)))
{
#if (HW_ISP_DEVICE_NUM > 1)
	if(media_params.isp_sync_mode && (lib->isp_id == 0)) {
		*result = isp_ctx[1].awb_entity_ctx.awb_result;
	}
#endif
	FUNCTION_LOG;
}
static void __afs_done(struct isp_lib_context *lib,
			afs_result_t *result __attribute__((__unused__)))
{
#if (HW_ISP_DEVICE_NUM > 1)
	if(media_params.isp_sync_mode && (lib->isp_id == 0)) {
		*result = isp_ctx[1].afs_entity_ctx.afs_result;
	}
#endif
	FUNCTION_LOG;
}

static void __md_done(struct isp_lib_context *lib,
			md_result_t *result __attribute__((__unused__)))
{
	FUNCTION_LOG;
}

static void __pltm_done(struct isp_lib_context *lib,
			pltm_result_t *result __attribute__((__unused__)))
{
#if (HW_ISP_DEVICE_NUM > 1)
	if(media_params.isp_sync_mode && (lib->isp_id == 0)) {
		*result = isp_ctx[1].pltm_entity_ctx.pltm_result;
	}
#endif
	FUNCTION_LOG;
}

#if ISP_LIB_USE_FLASH
void __isp_flash_open(struct hw_isp_device *isp)
{
	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	ae_result_t *ae_result = &isp_gen->ae_entity_ctx.ae_result;

	isp_gen->ae_settings.take_pic_start_cnt = isp_gen->ae_frame_cnt;
	ISP_DEV_LOG(ISP_LOG_FLASH, "%s: TORCH_ON, ev_set.ev_idx:%d, take_pic_start_cnt:%d, flash_delay:%d.\n",
		__FUNCTION__, ae_result->sensor_set.ev_set.ev_idx,
		isp_gen->ae_settings.take_pic_start_cnt,
		isp_gen->isp_ini_cfg.isp_tunning_settings.flash_delay_frame);

	ae_result->ae_flash_led = V4L2_FLASH_LED_MODE_TORCH;
	isp_flash_ctrl(isp, V4L2_FLASH_LED_MODE_TORCH);
	isp_gen->ae_settings.flash_open = 1;
}

void isp_flash_update_status(struct hw_isp_device *isp)
{
	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	isp_ae_entity_context_t *isp_ae_cxt = &isp_gen->ae_entity_ctx;
	ae_result_t *ae_result = &isp_gen->ae_entity_ctx.ae_result;
	int flash_gain = isp_gen->isp_ini_cfg.isp_tunning_settings.flash_gain;
	int flash_delay = isp_gen->isp_ini_cfg.isp_tunning_settings.flash_delay_frame;
	int ev_idx_flash = 0;

	switch (isp_gen->ae_settings.flash_mode) {
	case FLASH_MODE_ON:
		if (isp_gen->ae_settings.take_picture_flag == V4L2_TAKE_PICTURE_FLASH) {
			if(isp_gen->ae_settings.flash_open == 0) {
				isp_gen->ae_settings.flash_switch_flag = true;
				__isp_flash_open(isp);
			}
		}
		break;
	case FLASH_MODE_AUTO:
		if (isp_gen->ae_settings.take_picture_flag == V4L2_TAKE_PICTURE_FLASH) {
			if(isp_gen->ae_settings.flash_open == 0) {
				if(ae_result->sensor_set.ev_set.ev_idx > (ae_result->sensor_set.ev_idx_max - 40)) {
					ISP_DEV_LOG(ISP_LOG_FLASH, "ev_idx is %d, ev_idx_max is %d\n",
							ae_result->sensor_set.ev_set.ev_idx, ae_result->sensor_set.ev_idx_max);
					isp_gen->ae_settings.flash_switch_flag = true;
					__isp_flash_open(isp);
				}
			} else {
				if (isp_gen->ae_frame_cnt == (isp_gen->ae_settings.take_pic_start_cnt + flash_delay))
				{
					ae_result->ae_flash_led = V4L2_FLASH_LED_MODE_NONE;
					isp_flash_ctrl(isp, V4L2_FLASH_LED_MODE_NONE);
					isp_gen->ae_settings.flash_switch_flag = false;

					isp_gen->ae_settings.exposure_lock = true;

					if (ae_result->ae_flash_ev_cumul >= 100)
					{
						ev_idx_flash = ae_result->sensor_set.ev_idx_expect;
					}
					else if (ae_result->ae_flash_ev_cumul < 100 && ae_result->ae_flash_ev_cumul >= flash_gain*100/256)
					{
						ev_idx_flash = ae_result->sensor_set.ev_idx_expect;
					}
					else if (ae_result->ae_flash_ev_cumul >= -25 && ae_result->ae_flash_ev_cumul < flash_gain*100/256)
					{
						ev_idx_flash = ae_result->sensor_set.ev_idx_expect + ae_result->ae_flash_ev_cumul * flash_gain/256;
					}
					else
					{
						ev_idx_flash = ae_result->sensor_set.ev_idx_expect + ae_result->ae_flash_ev_cumul * flash_gain/256;
					}
					ev_idx_flash = clamp(ev_idx_flash, 1, ae_result->sensor_set.ev_idx_max);

					ISP_DEV_LOG(ISP_LOG_FLASH, "%s: FLASH_OFF, ae_flash_ev_cumul:%d, ev_idx_expect:%d, ev_idx_flash:%d, flash_gain:%d.\n",
						__FUNCTION__,	ae_result->ae_flash_ev_cumul,
						ae_result->sensor_set.ev_idx_expect, ev_idx_flash,
						isp_gen->isp_ini_cfg.isp_tunning_settings.flash_gain);

					isp_ae_cxt->ae_param->ae_pline_index = ev_idx_flash;
					isp_ae_set_params_helper(isp_ae_cxt, ISP_AE_SET_EXP_IDX);
				}
				else if (isp_gen->ae_frame_cnt == (1 + isp_gen->ae_settings.take_pic_start_cnt + flash_delay))
				{
					ISP_DEV_LOG(ISP_LOG_FLASH, "%s: FLASH_ON.\n", __FUNCTION__);

					isp_gen->ae_settings.flash_switch_flag = true;
					ae_result->ae_flash_led = V4L2_FLASH_LED_MODE_FLASH;
					isp_flash_ctrl(isp, V4L2_FLASH_LED_MODE_FLASH);
				}
				else if ((isp_gen->ae_frame_cnt >= (4 + isp_gen->ae_settings.take_pic_start_cnt + flash_delay)) &&
					(isp_gen->ae_frame_cnt <= (6 + isp_gen->ae_settings.take_pic_start_cnt + flash_delay)))
				{
					ae_result->ae_flash_ok = 1;
					isp_gen->image_params.isp_image_params.image_para.bits.flash_ok = ae_result->ae_flash_ok;
				}
				else if (isp_gen->ae_frame_cnt == (7 + isp_gen->ae_settings.take_pic_start_cnt + flash_delay))
				{
					ISP_DEV_LOG(ISP_LOG_FLASH, "%s: FLASH_OFF.\n", __FUNCTION__);

					ae_result->ae_flash_led = V4L2_FLASH_LED_MODE_NONE;
					isp_flash_ctrl(isp, V4L2_FLASH_LED_MODE_NONE);
					isp_gen->ae_settings.flash_switch_flag = false;

					isp_gen->ae_settings.exposure_lock = false;
				}
			}
		}
		break;
	case FLASH_MODE_TORCH:
		if (isp_gen->ae_settings.take_picture_flag == V4L2_TAKE_PICTURE_FLASH) {
			if (isp_gen->ae_settings.flash_open == 0) {
				isp_gen->ae_settings.flash_switch_flag = true;
				__isp_flash_open(isp);
			} else {
				if ((isp_gen->ae_frame_cnt >= (1 + isp_gen->ae_settings.take_pic_start_cnt + flash_delay))
					&& (isp_gen->ae_frame_cnt <= (3 + isp_gen->ae_settings.take_pic_start_cnt + flash_delay)))
				{
					ae_result->ae_flash_ok = 1;
					isp_gen->image_params.isp_image_params.image_para.bits.flash_ok = ae_result->ae_flash_ok;
				}
				if (isp_gen->ae_frame_cnt == (4 + isp_gen->ae_settings.take_pic_start_cnt + flash_delay))
				{
					ISP_DEV_LOG(ISP_LOG_FLASH, "%s: FLASH_OFF.\n", __FUNCTION__);
					ae_result->ae_flash_led = V4L2_FLASH_LED_MODE_TORCH;
					isp_flash_ctrl(isp, V4L2_FLASH_LED_MODE_NONE);
					isp_gen->ae_settings.flash_switch_flag = false;
				}
			}
		} else {
			ISP_DEV_LOG(ISP_LOG_FLASH, "%s: TORCH_ON, FLASH_MODE TORCH_ON.\n", __FUNCTION__);
			isp_gen->ae_settings.flash_switch_flag = true;
			ae_result->ae_flash_led = V4L2_FLASH_LED_MODE_TORCH;
			isp_flash_ctrl(isp, V4L2_FLASH_LED_MODE_TORCH);
		}
		break;
	case FLASH_MODE_OFF:
		if (isp_gen->ae_settings.flash_switch_flag) {
			ae_result->ae_flash_led = V4L2_FLASH_LED_MODE_NONE;
			isp_flash_ctrl(isp, V4L2_FLASH_LED_MODE_NONE);
			isp_gen->ae_settings.flash_switch_flag = false;
		}
		if (isp_gen->ae_settings.exposure_lock)
			isp_gen->ae_settings.exposure_lock = false;
		break;
	case FLASH_MODE_RED_EYE:
	case FLASH_MODE_NONE:
		break;
	default:
		ae_result->ae_flash_led = V4L2_FLASH_LED_MODE_NONE;
		isp_flash_ctrl(isp, V4L2_FLASH_LED_MODE_NONE);
		break;
	}
}
#endif

static void __isp_stats_process(struct hw_isp_device *isp,	const void *buffer)
{
	struct isp_lib_context *ctx;
	struct isp_table_reg_map reg;
	struct sensor_exp_gain exp_gain;
	ae_result_t *ae_result = NULL;
	awb_result_t *awb_result = NULL;
	af_result_t *af_result = NULL;
	struct sensor_temp temp;
	MPP_AtraceBegin(ATRACE_TAG_ISP, __FUNCTION__);

	ctx = isp_dev_get_ctx(isp);
	if (ctx == NULL)
		return;

	ctx->isp_stat_buf = buffer;
#if (HW_ISP_DEVICE_NUM > 1)
	if(media_params.isp_sync_mode) {
		isp_ctx_stats_prepare_sync(ctx, isp_ctx[0].isp_stat_buf, isp_ctx[1].isp_stat_buf);
	} else {
		isp_ctx_stats_prepare(ctx, ctx->isp_stat_buf);
	}
#else
	isp_ctx_stats_prepare(ctx, ctx->isp_stat_buf);
#endif
	isp_stat_save_run(ctx);

	isp_ini_tuning_run(isp);

	if (ctx->temp_info.enable) {
		if (isp_sensor_get_temp(isp, &temp) < 0) {
			ctx->temp_info.enable = 0;
		} else {
			ctx->sensor_info.temperature = temp.temp;
		}
	}

	FUNCTION_LOG;
	isp_ctx_algo_run(ctx);
	FUNCTION_LOG;

	isp_log_save_run(ctx);

	af_result = &ctx->af_entity_ctx.af_result;
	if (ctx->isp_ini_cfg.isp_test_settings.af_en || ctx->isp_ini_cfg.isp_test_settings.isp_test_focus) {
		if (af_result->last_code_output != af_result->real_code_output) {
			isp_act_set_pos(isp, af_result->real_code_output);
			af_result->last_code_output = af_result->real_code_output;
		}
		ISP_DEV_LOG(ISP_LOG_ISP, "set sensor pos real_code_output: %d.\n", af_result->real_code_output);
	}

#if ISP_LIB_USE_FLASH
	isp_flash_update_status(isp);
#endif

	ae_result = &ctx->ae_entity_ctx.ae_result;
	awb_result = &ctx->awb_entity_ctx.awb_result;
	exp_gain.exp_val = ae_result->sensor_set.ev_set_curr.ev_sensor_exp_line;
	//exp_gain.ae_wdr_ratio = ae_result->ae_wdr_ratio.sensor;
	exp_gain.gain_val = ae_result->sensor_set.ev_set_curr.ev_analog_gain >> 4;
	exp_gain.r_gain = awb_result->wb_gain_output.r_gain * 256 / awb_result->wb_gain_output.gr_gain;
	exp_gain.b_gain = awb_result->wb_gain_output.b_gain * 256 / awb_result->wb_gain_output.gb_gain;

#if (HW_ISP_DEVICE_NUM > 1)
	/*isp0 and isp1 are opened and have same head sensor, so we only use isp0 control ae*/
	if (isp->id == 1 && media_params.isp_dev[0] != NULL && media_params.isp_dev[1] != NULL &&
	    !strcmp(media_params.isp_dev[0]->sensor.info.name, media_params.isp_dev[1]->sensor.info.name)) {
		ISP_DEV_LOG(ISP_LOG_ISP, "isp0 and isp1 are opened and have same head, so we only use isp0 to do ae!\n");
	} else {
		isp_sensor_set_exp_gain(isp, &exp_gain);
	}
#else
	isp_sensor_set_exp_gain(isp, &exp_gain);
#endif
	FUNCTION_LOG;

	reg.addr = ctx->load_reg_base;
	reg.size = ISP_LOAD_DRAM_SIZE;
	isp_set_load_reg(isp, &reg);

	isp_get_debug_info(isp);
	isp_sync_debug_info(isp, &ctx->debug_param_info);

}

static void __isp_fsync_process(struct hw_isp_device *isp, struct v4l2_event *event)
{
	struct isp_lib_context *ctx = isp_dev_get_ctx(isp);
	MPP_AtraceBegin(ATRACE_TAG_ISP, __FUNCTION__);

	if(ctx->sensor_info.color_space != event->u.data[1]) {
		ctx->sensor_info.color_space = event->u.data[1];
		ctx->isp_3a_change_flags |= ISP_SET_HUE;
	}

	isp_lib_log_param = (event->u.data[3] << 8) | event->u.data[2] | ctx->isp_ini_cfg.isp_test_settings.isp_log_param;
	MPP_AtraceEnd(ATRACE_TAG_ISP);
}

void __isp_ctrl_process(struct hw_isp_device *isp, struct v4l2_event *event)
{
	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	HW_S32 iso_qmenu[] = { 100, 200, 400, 800, 1600, 3200, 6400};
	HW_S32 exp_bias_qmenu[] = { -4, -3, -2, -1, 0, 1, 2, 3, 4, };
	MPP_AtraceBegin(ATRACE_TAG_ISP, __FUNCTION__);

	if (isp_gen == NULL) {
		MPP_AtraceEnd(ATRACE_TAG_ISP);
		return;
	}

	switch(event->id) {
	case V4L2_CID_BRIGHTNESS:
		isp_s_brightness(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_CONTRAST:
		isp_s_contrast(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_SATURATION:
		isp_s_saturation(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_HUE:
		isp_s_hue(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		isp_s_auto_white_balance(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_EXPOSURE:
		isp_s_exposure(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_AUTOGAIN:
		isp_s_auto_gain(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_GAIN:
		isp_s_gain(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		isp_s_power_line_frequency(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		isp_s_white_balance_temperature(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_SHARPNESS:
		isp_s_sharpness(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_AUTOBRIGHTNESS:
		isp_s_auto_brightness(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_BAND_STOP_FILTER:
		isp_s_band_stop_filter(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_ILLUMINATORS_1:
		isp_s_illuminators_1(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_ILLUMINATORS_2:
		isp_s_illuminators_2(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		isp_s_exposure_auto(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_EXPOSURE_ABSOLUTE:
		isp_s_exposure_absolute(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		isp_s_focus_absolute(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_FOCUS_RELATIVE:
		isp_s_focus_relative(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_FOCUS_AUTO:
		isp_s_focus_auto(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_AUTO_EXPOSURE_BIAS:
		isp_s_auto_exposure_bias(isp_gen, exp_bias_qmenu[event->u.ctrl.value]);
		break;
	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
		isp_s_auto_n_preset_white_balance(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_ISO_SENSITIVITY:
		isp_s_iso_sensitivity(isp_gen, iso_qmenu[event->u.ctrl.value]);
		break;
	case V4L2_CID_ISO_SENSITIVITY_AUTO:
		isp_s_iso_sensitivity_auto(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_EXPOSURE_METERING:
		isp_s_ae_metering_mode(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_SCENE_MODE:
		isp_s_scene_mode(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_3A_LOCK:
		//isp_s_3a_lock(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_AUTO_FOCUS_START:
		isp_s_auto_focus_start(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_AUTO_FOCUS_STOP:
		isp_s_auto_focus_stop(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_AUTO_FOCUS_RANGE:
		isp_s_auto_focus_range(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_TAKE_PICTURE:
		isp_s_take_picture(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_FLASH_LED_MODE:
		isp_s_flash_mode(isp_gen, event->u.ctrl.value);
		break;
	case V4L2_CID_FLASH_LED_MODE_V1:
		isp_s_flash_mode_v1(isp_gen, event->u.ctrl.value);
		break;
	default:
		ISP_ERR("Unknown ctrl.\n");
		break;
	}
	MPP_AtraceEnd(ATRACE_TAG_ISP);
}

static void __isp_stream_off(struct hw_isp_device *isp __attribute__((__unused__)))
{
	if ((isp->id >= HW_ISP_DEVICE_NUM) || (isp->id == -1))
		ISP_ERR("ISP ID is invalid, __isp_stream_off failed!\n");

	events_stop(&events_arr[isp->id]);
}

static void __isp_monitor_fd(int id, int fd, enum hw_isp_event_type type,
			      void(*callback)(void *), void *priv)
{
	events_monitor_fd(&events_arr[id], fd, type, callback, priv);
}

static void __isp_unmonitor_fd(int id, int fd)
{
	events_unmonitor_fd(&events_arr[id], fd);
}

static struct isp_ctx_operations isp_ctx_ops = {
	.ae_done = __ae_done,
	.af_done = __af_done,
	.awb_done = __awb_done,
	.afs_done = __afs_done,
	.md_done = __md_done,
	.pltm_done = __pltm_done,
};

static struct isp_dev_operations isp_dev_ops = {
	.stats_ready = __isp_stats_process,
	.fsync = __isp_fsync_process,
	.stream_off = __isp_stream_off,
	.ctrl_process = __isp_ctrl_process,
	.monitor_fd = __isp_monitor_fd,
	.unmonitor_fd = __isp_unmonitor_fd,
};

static void *__isp_thread(void *arg)
{
	int ret = 0;
	struct hw_isp_device *isp = (struct hw_isp_device *)arg;

	if ((isp->id >= HW_ISP_DEVICE_NUM) || (isp->id == -1))
		ISP_ERR("ISP ID is invalid, isp_run failed!\n");

	ret = isp_dev_start(isp);
	if (ret < 0)
		goto end;
	if (events_loop(&events_arr[isp->id]))
		goto end;
end:
	isp_dev_stop(isp);
	return NULL;
}

int isp_set_sync(int mode)
{
#if (HW_ISP_DEVICE_NUM > 1)
	if(mode == 0 || mode == 1)
		media_params.isp_sync_mode = mode;
	else
		return -1;
#else
	media_params.isp_sync_mode = 0;
#endif
	return 0;
}

int media_dev_init(void)
{
	/*must be called before all isp init*/
	//memset(&media_params, 0, sizeof(media_params));

	isp_version_info();

	return 0;
}

void media_dev_exit(void)
{
	/*must be called after all isp exit*/
#if (HW_ISP_DEVICE_NUM > 1)
	if ((media_params.isp_use_cnt[0] == 0) && (media_params.isp_use_cnt[1] == 0)) {
		if (media_params.mdev) {
			media_close(media_params.mdev);
			media_params.mdev = NULL;
		}
	}
#else
	if (media_params.isp_use_cnt[0] == 0) {
		if (media_params.mdev) {
			media_close(media_params.mdev);
			media_params.mdev = NULL;
		}
	}
#endif
}

int isp_ir_reset(int dev_id, int mode_flag)
{
	int ret = 0;

	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	if (mode_flag & 0x01) {
		ISP_WARN("ISP select wdr config fail\n");
	}

	if (mode_flag & 0x02) {
		isp_ctx[dev_id].isp_ir_flag = 1;
		ISP_PRINT("ISP select ir config\n");
	} else {
		isp_ctx[dev_id].isp_ir_flag = 0;
	}

	isp_params_parse(isp, &isp_ctx[dev_id].isp_ini_cfg, isp_ctx[dev_id].isp_ir_flag, media_params.isp_sync_mode);
	ret = isp_tuning_reset(isp, &isp_ctx[dev_id].isp_ini_cfg);
	if (ret) {
		ISP_ERR("error: unable to reset isp tuning\n");
	}

	return ret;
}

int isp_reset(int dev_id)
{
	int ret = 0;
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	isp_ctx[dev_id].isp_ir_flag = 0;
	isp_params_parse(isp, &isp_ctx[dev_id].isp_ini_cfg, isp_ctx[dev_id].isp_ir_flag, media_params.isp_sync_mode);
	ret = isp_tuning_reset(isp, &isp_ctx[dev_id].isp_ini_cfg);
	if (ret) {
		ISP_ERR("error: unable to reset isp tuning\n");
	}

	return ret;
}

int isp_init(int dev_id)
{
	int ret = 0;
	struct hw_isp_device *isp = NULL;
	struct isp_table_reg_map reg;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	if (media_params.isp_use_cnt[dev_id]++ > 0)
		return 0;

	/*update media entity and links*/
	if (media_params.mdev) {
		media_close(media_params.mdev);
		media_params.mdev = NULL;
	}

	media_params.mdev = media_open(MEDIA_DEVICE, 0);
	if (media_params.mdev == NULL) {
		ISP_ERR("isp%d update media entity and links failed!\n", dev_id);
		return -1;
	}

	ret = isp_dev_open(&media_params, dev_id);
	if (ret < 0)
		return ret;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	isp_dev_register(isp, &isp_dev_ops);
	isp_dev_banding_ctx(isp, &isp_ctx[dev_id]);

	isp_ctx[dev_id].isp_id = dev_id;

//	isp_sensor_otp_init(isp);
	isp_config_sensor_info(isp);

	isp_ctx_save_init(&isp_ctx[dev_id]);
	isp_stat_save_init(&isp_ctx[dev_id]);
	isp_log_save_init(&isp_ctx[dev_id]);

	isp_ctx[dev_id].isp_ir_flag = ISP_COLOR_MODE_DEFAULT; // default colorful
	ret = isp_params_parse(isp, &isp_ctx[dev_id].isp_ini_cfg, isp_ctx[dev_id].isp_ir_flag, media_params.isp_sync_mode);
	if (ret < 0) {
		if (dev_id >= 1)
			isp_ctx[dev_id].isp_ini_cfg = isp_ctx[0].isp_ini_cfg;
	}

	FUNCTION_LOG;
	isp_ctx_algo_init(&isp_ctx[dev_id], &isp_ctx_ops);
	FUNCTION_LOG;

	tuning[dev_id] = isp_tuning_init(isp, &isp_ctx[dev_id].isp_ini_cfg);
	if (tuning[dev_id] == NULL) {
		ISP_ERR("error: unable to initialize isp tuning\n");
		return -1;
	}
	FUNCTION_LOG;

	if (isp_ctx[dev_id].isp_ini_cfg.isp_test_settings.af_en || isp_ctx[dev_id].isp_ini_cfg.isp_test_settings.isp_test_focus)
		isp_act_init_range(isp, isp_ctx[dev_id].isp_ini_cfg.isp_3a_settings.vcm_min_code, isp_ctx[dev_id].isp_ini_cfg.isp_3a_settings.vcm_max_code);

	events_arr[dev_id].isp_id = dev_id;
	events_init(&events_arr[dev_id]);
	events_star(&events_arr[dev_id]);
	FUNCTION_LOG;

	reg.addr = isp_ctx[dev_id].load_reg_base;
	reg.size = ISP_LOAD_DRAM_SIZE;
	isp_set_load_reg(isp, &reg);
	strcpy(isp_ctx[dev_id].debug_param_info.libs_version, ISP_REPO_COMMIT);

	ISP_DEV_LOG(ISP_LOG_ISP, "isp%d init end!!!\n", dev_id);

	return 0;
}

int isp_update(int dev_id)
{
	int ret = 0;
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	ret = isp_tuning_update(isp);
	if (ret) {
		ISP_ERR("error: unable to update isp tuning\n");
	}
	return ret;
}

int isp_get_imageparams(int dev_id, isp_image_params_t *pParams)
{
	struct hw_isp_device *isp = NULL;
	struct isp_lib_context *isp_gen = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	isp_gen = isp_dev_get_ctx(isp);
	memcpy(pParams, &isp_gen->image_params, sizeof(isp_image_params_t));
	return 0;
}

int isp_stop(int dev_id)
{
	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	if (media_params.isp_use_cnt[dev_id] == 1)
		events_stop(&events_arr[dev_id]);

	return 0;
}

/* stop ldci to close LDCI_VIDEO_CHN(video4) to events_stop __isp_thread*/
int isp_stop_ldci(int dev_id)
{
	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	if (isp_ctx[dev_id].isp_ini_cfg.isp_test_settings.gtm_en && isp_ctx[dev_id].isp_ini_cfg.isp_tunning_settings.gtm_type == ISP_GTM_LDCI)
		isp_ctx[dev_id].isp_ini_cfg.isp_tunning_settings.gtm_type = 0;

	return 0;
}

int isp_exit(int dev_id)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	if ((media_params.isp_use_cnt[dev_id] == 0) || (--media_params.isp_use_cnt[dev_id] > 0))
		return 0;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	/*wait to exit until the thread is finished*/
	pthread_join(media_params.isp_tid[dev_id], NULL);

	isp_log_save_exit(&isp_ctx[dev_id]);
	isp_stat_save_exit(&isp_ctx[dev_id]);
	isp_ctx_save_exit(&isp_ctx[dev_id]);
	isp_tuning_exit(isp);
	isp_ctx_algo_exit(&isp_ctx[dev_id]);
//	isp_sensor_otp_exit(isp);
	isp_dev_close(&media_params, dev_id);

	ISP_DEV_LOG(ISP_LOG_ISP, "isp%d exit end!!!\n", dev_id);

	return 0;
}

int isp_run(int dev_id)
{
	int ret = 0;
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	if (media_params.isp_use_cnt[dev_id] > 1)
		return 0;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	ISP_PRINT("create isp%d server thread!\n", dev_id);

	ret = pthread_create(&media_params.isp_tid[dev_id], NULL, __isp_thread, isp);
	if(ret != 0)
		ISP_ERR("%s: %s\n",__func__, strerror(ret));
	pthread_setname_np(media_params.isp_tid[dev_id], "isp_thread");

	return ret;
}

/*
 *if donot want to run isp_stop/isp_exit and isp_init.
 *run isp_events_stop to stop isp run, and then run isp_events_restar and isp_run to rerun isp.
 */
int isp_events_stop(int dev_id)
{
	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	events_stop(&events_arr[dev_id]);

	/*wait to exit until the thread is finished*/
	pthread_join(media_params.isp_tid[dev_id], NULL);

	return 0;
}

int isp_events_restar(int dev_id)
{
	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	events_init(&events_arr[dev_id]);
	events_star(&events_arr[dev_id]);

	isp_ctx[dev_id].af_frame_cnt  = 0;
	isp_ctx[dev_id].ae_frame_cnt  = 0;
	isp_ctx[dev_id].awb_frame_cnt = 0;
	isp_ctx[dev_id].gtm_frame_cnt = 0;

	isp_ctx[dev_id].md_frame_cnt  = 0;
	isp_ctx[dev_id].afs_frame_cnt  = 0;
	isp_ctx[dev_id].iso_frame_cnt = 0;
	isp_ctx[dev_id].rolloff_frame_cnt = 0;
	isp_ctx[dev_id].alg_frame_cnt = 0;

	return 0;
}

HW_S32 isp_pthread_join(int dev_id)
{
#if 0
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	if (media_params.isp_use_cnt[dev_id] == 1)
		pthread_join(media_params.isp_tid[dev_id], NULL);
#endif
	return 0;
}

HW_S32 isp_get_cfg(int dev_id, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	return isp_tuning_get_cfg(isp, group_id, cfg_ids, cfg_data);
}
HW_S32 isp_set_cfg(int dev_id, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	return isp_tuning_set_cfg(isp, group_id, cfg_ids, cfg_data);
}

HW_S32 isp_stats_req(int dev_id, struct isp_stats_context *stats_ctx)
{
	struct hw_isp_device *isp = NULL;
	struct isp_lib_context *ctx = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	ctx = isp_dev_get_ctx(isp);
	if (ctx == NULL)
		return -1;

	return isp_ctx_stats_req(ctx, stats_ctx);
}

HW_S32 isp_set_saved_ctx(int dev_id)
{
	return isp_ctx_save_exit(&isp_ctx[dev_id]);
}

int isp_set_fps(int dev_id, int s_fps)
{
	struct hw_isp_device *isp = NULL;
	struct sensor_fps fps;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	fps.fps = s_fps;
	isp_sensor_set_fps(isp, &fps);
	if(s_fps > 1)
		isp_ctx_update_ae_tbl(&isp_ctx[dev_id], s_fps);

	return 0;
}

HW_S32 isp_get_sensor_info(int dev_id, struct sensor_config *cfg)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}
	memset(cfg, 0, sizeof(struct sensor_config));

	isp_sensor_get_configs(isp, cfg);

	return 0;
}

/*******************isp for video buffer*********************/
HW_S32 isp_get_lv(int dev_id)
{
	struct hw_isp_device *isp;
	struct isp_lib_context *ctx;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		//ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	ctx = isp_dev_get_ctx(isp);
	if (ctx == NULL) {
		//ISP_ERR("isp%d get isp ctx failed!\n", dev_id);
		return -1;
	}

	return ctx->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_lv;
}

HW_S32 isp_get_encpp_cfg(int dev_id, HW_U32 ctrl_id, void *value)
{
#ifdef USE_ENCPP
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	switch(ctrl_id) {
		case ISP_CTRL_ENCPP_EN:
			*(HW_S32 *)value = isp_gen->isp_ini_cfg.isp_test_settings.encpp_en;
			break;
		case ISP_CTRL_ENCPP_STATIC_CFG:
			*(struct encpp_static_sharp_config *)value = isp_gen->encpp_static_sharp_cfg;
			break;
		case ISP_CTRL_ENCPP_DYNAMIC_CFG:
			*(struct encpp_dynamic_sharp_config *)value = isp_gen->iso_entity_ctx.iso_result.encpp_dynamic_sharp_cfg;
			break;
		case ISP_CTRL_ENCODER_3DNR_CFG:
			*(struct encoder_3dnr_config *)value = isp_gen->iso_entity_ctx.iso_result.encoder_3dnr_cfg;
			break;
		case ISP_CTRL_ENCODER_2DNR_CFG:
			*(struct encoder_2dnr_config *)value = isp_gen->iso_entity_ctx.iso_result.encoder_2dnr_cfg;
			break;
		default:
			ISP_ERR("Unknown ctrl.\n");
			break;
	}
	return 0;
#else
	ISP_ERR("isp%d don't support ctrl encpp param!\n", dev_id);
	return -1;
#endif
}

HW_S32 isp_get_attr_cfg(int dev_id, HW_U32 ctrl_id, void *value)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	switch(ctrl_id) {
		case ISP_CTRL_MODULE_EN:
			break;
		case ISP_CTRL_DIGITAL_GAIN:
			*(HW_S32 *)value = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_digital_gain;
			break;
		case ISP_CTRL_TOTAL_GAIN:
			*(HW_S32 *)value = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_total_gain;
			break;
		case ISP_CTRL_PLTMWDR_STR:
			*(HW_S32 *)value = isp_gen->tune.pltmwdr_level;
			break;
		case ISP_CTRL_PLTM_HARDWARE_STR:
			*(HW_S32 *)value = isp_gen->pltm_entity_ctx.pltm_result.pltm_auto_stren;
			break;
		case ISP_CTRL_DN_STR:
			*(HW_S32 *)value = isp_gen->tune.denoise_level;
			break;
		case ISP_CTRL_3DN_STR:
			*(HW_S32 *)value = isp_gen->tune.tdf_level;
			break;
		case ISP_CTRL_HIGH_LIGHT:
			*(HW_S32 *)value = isp_gen->tune.highlight_level;
			break;
		case ISP_CTRL_BACK_LIGHT:
			*(HW_S32 *)value = isp_gen->tune.backlight_level;
			break;
		case ISP_CTRL_WB_MGAIN:
			*(struct isp_wb_gain *)value = isp_gen->awb_entity_ctx.awb_result.wb_gain_output;
			break;
		case ISP_CTRL_AGAIN_DGAIN:
			*(struct gain_cfg *)value = isp_gen->tune.gains;
			break;
		case ISP_CTRL_COLOR_EFFECT:
			*(HW_S32 *)value = isp_gen->tune.effect;
			break;
		case ISP_CTRL_AE_ROI:
			*(struct isp_h3a_coor_win *)value = isp_gen->ae_settings.ae_coor;
			break;
		case ISP_CTRL_COLOR_TEMP:
			*(HW_S32 *)value = isp_gen->awb_entity_ctx.awb_result.color_temp_output;
			break;
		case ISP_CTRL_EV_IDX:
			*(HW_S32 *)value = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set.ev_idx;
			break;
		case ISP_CTRL_ISO_LUM_IDX:
			*(HW_S32 *)value = isp_gen->iso_entity_ctx.iso_result.lum_idx;
			break;
		case ISP_CTRL_COLOR_SPACE:
			*(HW_S32 *)value = isp_gen->sensor_info.color_space;
			break;
		default:
			ISP_ERR("Unknown ctrl.\n");
			break;
	}
	return 0;
}

HW_S32 isp_set_attr_cfg(int dev_id, HW_U32 ctrl_id, void *value)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	switch(ctrl_id) {
		case ISP_CTRL_MODULE_EN:
			break;
		case ISP_CTRL_DIGITAL_GAIN:
			break;
		case ISP_CTRL_PLTMWDR_STR:
			isp_gen->tune.pltmwdr_level = *(HW_S32 *)value;
			break;
		case ISP_CTRL_DN_STR:
			isp_gen->tune.denoise_level = *(HW_S32 *)value;
			break;
		case ISP_CTRL_3DN_STR:
			isp_gen->tune.tdf_level = *(HW_S32 *)value;
			break;
		case ISP_CTRL_HIGH_LIGHT:
			isp_gen->tune.highlight_level = *(HW_S32 *)value;
			break;
		case ISP_CTRL_BACK_LIGHT:
			isp_gen->tune.backlight_level = *(HW_S32 *)value;
			break;
		case ISP_CTRL_WB_MGAIN:
			isp_gen->awb_settings.wb_gain_manual = *(struct isp_wb_gain *)value;
			break;
		case ISP_CTRL_AGAIN_DGAIN:
			if (memcmp(&isp_gen->tune.gains, value, sizeof(struct gain_cfg))) {
				isp_gen->tune.gains = *(struct gain_cfg *)value;
				isp_gen->isp_3a_change_flags |= ISP_SET_GAIN_STR;
			}
			break;
		case ISP_CTRL_COLOR_EFFECT:
			if (isp_gen->tune.effect != *(HW_S32 *)value) {
				isp_gen->tune.effect = *(HW_S32 *)value;
				isp_gen->isp_3a_change_flags |= ISP_SET_EFFECT;
			}
			break;
		case ISP_CTRL_AE_ROI:
			isp_s_ae_roi(isp_gen, AE_METERING_MODE_SPOT, value);
			break;
		case ISP_CTRL_AF_METERING:
			isp_s_af_metering_mode(isp_gen, value);
			break;
		case ISP_CTRL_VENC2ISP_PARAM:
			isp_gen->VencVe2IspParam = *(struct enc_VencVe2IspParam *)value;
			break;
		case ISP_CTRL_NPU_NR_PARAM:
			isp_gen->npu_nr_cfg = *(struct npu_face_nr_config *)value;
			break;
		default:
			ISP_ERR("Unknown ctrl.\n");
			break;
	}
	return 0;
}

void* isp_get_ctx_addr(int dev_id)
{
	if (dev_id >= HW_ISP_DEVICE_NUM) {
		return NULL;
	}

	return (void*)&isp_ctx[dev_id];
}

HW_S32 isp_get_info_length(HW_S32* i3a_length, HW_S32* debug_length)
{
	HW_S32 data_len = 0;

	*i3a_length =
		sizeof(ae_result_t) + sizeof(ae_param_t)+ sizeof(struct isp_ae_stats_s) // ae info
		+ sizeof(awb_result_t)+ sizeof(awb_param_t)+ sizeof(struct isp_awb_stats_s) // awb info
		+ sizeof(af_result_t)+ sizeof(af_param_t)+ sizeof(struct isp_af_stats_s); // af info

	*debug_length =
		sizeof(iso_result_t)
		+sizeof(iso_param_t) // iso info
		+sizeof(struct isp_module_config)  // isp module info
		+sizeof(int) 						// otp enable flag
		+16*16*3*sizeof(unsigned short)	// msc tbl
		+4*2*sizeof(unsigned short);	// wb otp data

	ISP_PRINT("i3a_length:%d, debug_length:%d.\n", *i3a_length, *debug_length);
	ISP_PRINT("af_result_t:%d, af_param_t:%d, isp_af_stats_s:%d.\n", sizeof(ae_result_t), sizeof(ae_param_t), sizeof(struct isp_ae_stats_s));
 	ISP_PRINT("af_result_t:%d, af_param_t:%d, isp_af_stats_s:%d.\n", sizeof(awb_result_t), sizeof(awb_param_t), sizeof(struct isp_awb_stats_s));
	ISP_PRINT("af_result_t:%d, af_param_t:%d, isp_af_stats_s:%d.\n", sizeof(af_result_t), sizeof(af_param_t), sizeof(struct isp_af_stats_s));
	return 0;
}

HW_S32 isp_get_3a_parameters(int dev_id, void* params)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
	return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}
	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	void * ptr = params;
	// ae info
	memcpy(ptr, &(isp_gen->ae_entity_ctx.ae_result), sizeof(ae_result_t));
	ptr += sizeof(ae_result_t);

	memcpy(ptr, isp_gen->ae_entity_ctx.ae_param, sizeof(ae_param_t));
	ptr += sizeof(ae_param_t);

	memcpy(ptr, isp_gen->ae_entity_ctx.ae_stats.ae_stats, sizeof(struct isp_ae_stats_s));
	ptr += sizeof(struct isp_ae_stats_s);

	// awb info
	memcpy(ptr, &(isp_gen->awb_entity_ctx.awb_result), sizeof(awb_result_t));
	ptr += sizeof(awb_result_t);

	memcpy(ptr, isp_gen->awb_entity_ctx.awb_param, sizeof(awb_param_t));
	ptr += sizeof(awb_param_t);

	memcpy(ptr, isp_gen->awb_entity_ctx.awb_stats.awb_stats, sizeof(struct isp_awb_stats_s));
	ptr += sizeof(struct isp_awb_stats_s);

	// af info
	memcpy(ptr, &(isp_gen->af_entity_ctx.af_result), sizeof(af_result_t));
	ptr += sizeof(af_result_t);

	memcpy(ptr, isp_gen->af_entity_ctx.af_param, sizeof(af_param_t));
	ptr += sizeof(af_param_t);

	memcpy(ptr, isp_gen->af_entity_ctx.af_stats.af_stats, sizeof(struct isp_af_stats_s));
	ptr += sizeof(struct isp_af_stats_s);

	ptr = NULL;
	return 0;
}

HW_S32 isp_get_debug_msg(int dev_id, void* msg)
{
	struct hw_isp_device *isp = NULL;
	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}
	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	void * ptr = msg;
	memcpy(ptr, &(isp_gen->iso_entity_ctx.iso_result), sizeof(iso_result_t));
	ptr += sizeof(iso_result_t);

	memcpy(ptr, isp_gen->iso_entity_ctx.iso_param, sizeof(iso_param_t));
	ptr += sizeof(iso_param_t);

	memcpy(ptr, &isp_gen->module_cfg, sizeof(struct isp_module_config));
	ptr += sizeof(struct isp_module_config);

	memcpy(ptr, &isp_gen->otp_enable, sizeof(int));
	ptr += sizeof(int);

	// shading msc rgb tbl 16x16x3
	memcpy(ptr, isp_gen->pmsc_table, 16*16*3*sizeof(unsigned short));
	ptr += 16*16*3*sizeof(unsigned short);

	// wb otp & golden data
	memcpy(ptr, isp_gen->pwb_table, 4*2*sizeof(unsigned short));
	ptr += 4*2*sizeof(unsigned short);

	ptr = NULL;
	return 0;
}

HW_S32 isp_get_ae_info(int dev_id, isp_3a_info_ae *ae_info)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}
	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	ae_info->ae_target = isp_gen->ae_entity_ctx.ae_result.ae_target;
	ae_info->pic_lum = isp_gen->ae_entity_ctx.ae_result.ae_avg_lum;
	ae_info->weight_lum = isp_gen->ae_entity_ctx.ae_result.ae_weight_lum;
	ae_info->exp_line = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_sensor_exp_line;
	ae_info->exp_time = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_exposure_time;
	ae_info->again = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_analog_gain;
	ae_info->dgain = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_digital_gain;
	ae_info->tgain=  isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_total_gain;
	ae_info->tbl_idx = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_idx;
	ae_info->tbl_max_idx = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_idx_max;
	ae_info->lv = isp_gen->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_lv;

	return 0;
}

HW_S32 isp_get_awb_info(int dev_id, isp_3a_info_awb *awb_info)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}
	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	awb_info->color_temp = isp_gen->awb_entity_ctx.awb_result.color_temp_output;
	awb_info->base_color_temper = isp_gen->awb_entity_ctx.awb_param->awb_ini.awb_base_temper;
	awb_info->lv = isp_gen->awb_entity_ctx.awb_param->awb_sensor_info.ae_lv;
	awb_info->r_gain = isp_gen->awb_entity_ctx.awb_result.wb_gain_output.r_gain;
	awb_info->g_gain = isp_gen->awb_entity_ctx.awb_result.wb_gain_output.gr_gain;
	awb_info->b_gain = isp_gen->awb_entity_ctx.awb_result.wb_gain_output.b_gain;

	return 0;
}

HW_S32 isp_get_ev_lv_adj(int dev_id)
{
	struct hw_isp_device *isp;
	struct isp_lib_context *ctx;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	ctx = isp_dev_get_ctx(isp);
	if (ctx == NULL) {
		ISP_ERR("isp%d get isp ctx failed!\n", dev_id);
		return -1;
	}

	return ctx->ae_entity_ctx.ae_result.ev_lv_adj;
}

int isp_get_temp(int dev_id)
{
	struct hw_isp_device *isp = NULL;
	struct sensor_temp temp;
	int sensor_temperature = 0;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	isp_sensor_get_temp(isp, &temp);
	sensor_temperature = temp.temp;

	return sensor_temperature;
}

int isp_read_cfg_bin(int dev_id, int mode_flag, char *isp_cfg_bin_path)
{
	struct hw_isp_device *isp = NULL;
	int ret = 0;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	switch (mode_flag) {
	case 0x00:
		isp_ctx[dev_id].isp_ir_flag = 0;
		ISP_PRINT("ISP set color mode\n");
		break;
	case 0x01:
		isp_ctx[dev_id].isp_ir_flag = 1;
		ISP_PRINT("ISP set ir mode\n");
		break;
	case 0x02:
	case 0x03:
		ISP_WARN("ISP select wdr config fail\n");
		break;
	default:
		isp_ctx[dev_id].isp_ir_flag = 0;
	}

	ret = isp_cfg_bin_parse(isp, &isp_ctx[dev_id].isp_ini_cfg, isp_cfg_bin_path, media_params.isp_sync_mode);
	if (ret < 0) {
		ISP_ERR("ISP read isp_cfg.bin error, please check the isp_cfg.bin!\n");
		return ret;
	}
	ret = isp_tuning_reset(isp, &isp_ctx[dev_id].isp_ini_cfg);
	if (ret) {
		ISP_ERR("error: unable to reset isp tuning\n");
	}

	return ret;
}

int isp_set_ae_roi(int dev_id, struct isp_h3a_coor_win *coor, HW_U16 force_ae_target, HW_U16 enable)
{
	struct hw_isp_device *isp = NULL;
	int ret;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL) {
		ISP_ERR("isp_gen is NULL!\n");
		return -1;
	}

	if (!enable) {
		ISP_PRINT("It will set default ae_roi_area!\n");
		isp_gen->ae_entity_ctx.ae_param->test_cfg.ae_forced = 0;
		coor->x1 = H3A_PIC_OFFSET;
		coor->y1 = H3A_PIC_OFFSET;
		coor->x2 = H3A_PIC_SIZE + H3A_PIC_OFFSET;
		coor->y2 = H3A_PIC_SIZE + H3A_PIC_OFFSET;
	} else {
		force_ae_target = clamp(force_ae_target, 1, 255);
		isp_gen->ae_entity_ctx.ae_param->test_cfg.ae_forced = 1;
		isp_gen->ae_entity_ctx.ae_param->test_cfg.lum_forced = force_ae_target;
	}
	ret = isp_set_attr_cfg(dev_id, ISP_CTRL_AE_ROI, coor);

	return ret;
}

HW_S32 isp_get_yuv_ystat(struct isp_size pic_size, struct isp_h3a_coor_win target_area, void* VirAddr)
{
	HW_S32 y_stat = 0;
	HW_S32 y_sum = 0;
	HW_U8 *portion_y = NULL;
	HW_S32 i, j;

	if (VirAddr == NULL) {
		ISP_ERR("The VirAddr is NULL!\n");
		return -1;
	}

	for (i = target_area.y1;i < target_area.y2;i++) {
		for(j = target_area.x1;j < target_area.x2;j++) {
			portion_y = VirAddr + (i*pic_size.width + j);
			y_sum += (HW_S32)(*portion_y);
		}
	}
	y_stat = y_sum / ((target_area.x2-target_area.x1)*(target_area.y2-target_area.y1));
	ISP_PRINT("target_area's y_stat = %d\n", y_stat);

	return y_stat;
}

HW_S32 isp_get_awb_gain_ir(int dev_id, HW_S32 *rgain_ir, HW_S32 *bgain_ir)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	*rgain_ir = isp_gen->rgain_ir;
	*bgain_ir = isp_gen->bgain_ir;

	return 0;
}

HW_S32 isp_get_awb_stats_avg(int dev_id, HW_U32 *awb_stats_ravg, HW_U32 *awb_stats_gavg, HW_U32 *awb_stats_bavg)
{
	struct hw_isp_device *isp = NULL;
	HW_U32 r_avg_sum = 0, g_avg_sum = 0, b_avg_sum = 0;
	int row, col;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	for (row = 0;row < ISP_AWB_ROW;row++) {
		for (col = 0;col < ISP_AWB_COL;col++) {
			r_avg_sum += isp_gen->stats_ctx.stats.awb_stats.awb_avg_r[row][col];
			g_avg_sum += isp_gen->stats_ctx.stats.awb_stats.awb_avg_g[row][col];
			b_avg_sum += isp_gen->stats_ctx.stats.awb_stats.awb_avg_b[row][col];
		}
	}
	*awb_stats_ravg = r_avg_sum / (ISP_AWB_ROW*ISP_AWB_COL);
	*awb_stats_gavg = g_avg_sum / (ISP_AWB_ROW*ISP_AWB_COL);
	*awb_stats_bavg = b_avg_sum / (ISP_AWB_ROW*ISP_AWB_COL);
	ISP_PRINT("awb_stats_ravg = %d, awb_stats_gavg = %d, awb_stats_bavg = %d\n",
		*awb_stats_ravg, *awb_stats_gavg, *awb_stats_bavg);

	return 0;
}

HW_S32 isp_get_ae_weight_lum(int dev_id)
{
	struct hw_isp_device *isp;
	struct isp_lib_context *ctx;

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	ctx = isp_dev_get_ctx(isp);
	if (ctx == NULL) {
		ISP_ERR("isp%d get isp ctx failed!\n", dev_id);
		return -1;
	}

	return ctx->ae_entity_ctx.ae_result.ae_weight_lum;
}

HW_S32 isp_set_ae_flicker_comp(int dev_id, HW_S16 enable)
{
	struct hw_isp_device *isp = NULL;

	if (dev_id >= HW_ISP_DEVICE_NUM)
		return -1;

	if ((enable < 0) || (enable > 1)) {
		ISP_ERR("Invaild ae_flicker_comp option !\n");
		return -1;
	}

	isp = media_params.isp_dev[dev_id];
	if (!isp) {
		ISP_ERR("isp%d device is NULL!\n", dev_id);
		return -1;
	}

	struct isp_lib_context *isp_gen = isp_dev_get_ctx(isp);
	if (isp_gen == NULL)
		return -1;

	isp_gen->ae_entity_ctx.ae_param->ae_ini.ae_blowout_pre_en = enable;
	ISP_PRINT("Current flicker comp = %d\n", isp_gen->ae_entity_ctx.ae_param->ae_ini.ae_blowout_attr);

	return 0;
}

/*******************get isp version*********************/
HW_S32 isp_get_version(char* version)
{
	sprintf(version, "ISP%d", ISP_VERSION);

	ISP_PRINT("ISP Version: ISP%d\n", ISP_VERSION);

	return 0;
}

