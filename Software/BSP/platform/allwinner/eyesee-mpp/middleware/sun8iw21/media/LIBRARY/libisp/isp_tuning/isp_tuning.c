
/*
 ******************************************************************************
 *
 * iq.c
 *
 * Hawkview ISP - iq.c module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/03/22	VIDEO INPUT
 *
 *****************************************************************************
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "device/isp_dev.h"
#include "../isp_dev/tools.h"
#include "isp_ini_parse.h"

#include "isp_tuning_priv.h"
#include "../include/isp_tuning.h"

enum isp_sense_config {
	ISP_SCENE_CONFIG_0 = 0,
	ISP_SCENE_CONFIG_1,
	ISP_SCENE_CONFIG_2,
	ISP_SCENE_CONFIG_3,
	ISP_SCENE_CONFIG_4,
	ISP_SCENE_CONFIG_5,
	ISP_SCENE_CONFIG_6,
	ISP_SCENE_CONFIG_7,
	ISP_SCENE_CONFIG_8,

	ISP_SCENE_CONFIG_MAX,
};

struct isp_tuning {
	struct hw_isp_device *isp;
	struct isp_lib_context *ctx;
	struct isp_param_config params;
	unsigned int frame_count;
	unsigned int pix_max;
	pthread_mutex_t mutex;
};

int isp_params_parse(struct hw_isp_device *isp, struct isp_param_config *params, HW_U8 ir, int sync_mode)
{
	struct isp_lib_context *ctx = isp_dev_get_ctx(isp);

	if (ctx == NULL)
		return -1;

	return parser_ini_info(params, ctx->debug_param_info.isp_cfg_version, ctx->sensor_info.name,
	ctx->sensor_info.sensor_width, ctx->sensor_info.sensor_height,
		ctx->sensor_info.fps_fixed, ctx->sensor_info.wdr_mode, ir, sync_mode, ctx->isp_id);
}

int isp_cfg_bin_parse(struct hw_isp_device *isp, struct isp_param_config *params, char *isp_cfg_bin_path, int sync_mode)
{
	struct isp_lib_context *ctx = isp_dev_get_ctx(isp);

	if (ctx == NULL)
		return -1;

	return parse_isp_cfg(params, ctx->debug_param_info.isp_cfg_version, ctx->isp_id, sync_mode, ctx->isp_ir_flag, isp_cfg_bin_path);
}

static int save_ini_tuning_file(struct hw_isp_device *isp, char *path, struct isp_param_config *param, char *sensor_name, int w, int h, int fps, int wdr)
{
	char isp_cfg_path[128], command[128];
	FILE *fd = NULL;
	struct isp_lib_context *ctx = isp_dev_get_ctx(isp);

	if (strcmp(sensor_name, "")) {
		sprintf(isp_cfg_path, "%s%d_%d_%d_%d/", path, w, h, fps, wdr);
		if (access(isp_cfg_path, F_OK) == 0) {
			sprintf(command, "rm -rf %s", isp_cfg_path);
			system(command);
		}
		sprintf(command, "mkdir %s", isp_cfg_path);
		system(command);

		//version
		sprintf(command, "touch %sversion", isp_cfg_path);
		system(command);
		sprintf(command, "echo %d > %sversion", ISP_VERSION, isp_cfg_path);
		system(command);

		//ir
		sprintf(command, "touch %sir", isp_cfg_path);
		system(command);
		sprintf(command, "echo %d > %sir", ctx->isp_ir_flag, isp_cfg_path);
		system(command);

		//colorspace
		sprintf(command, "touch %scolorspace", isp_cfg_path);
		system(command);
		sprintf(command, "echo %d > %scolorspace", ctx->sensor_info.color_space, isp_cfg_path);
		system(command);

		//cfg
		sprintf(isp_cfg_path, "%s%d_%d_%d_%d/cfg", path, w, h, fps, wdr);
		fd = fopen(isp_cfg_path, "w+");

		fwrite(param, sizeof(struct isp_param_config), 1, fd);

		fclose(fd);
		fd = NULL;

		//log
		sprintf(isp_cfg_path, "%s%d_%d_%d_%d/LOG/", path, w, h, fps, wdr);
		sprintf(command, "mkdir %s", isp_cfg_path);
		system(command);
		sprintf(command, "touch %sflag", isp_cfg_path);
		system(command);
		sprintf(command, "echo 0 > %sflag", isp_cfg_path);
		system(command);
	} else {
		ISP_ERR("%s:sensor cfg name is invalid\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

static int load_ini_tuning_file(struct hw_isp_device *isp, char *path, struct isp_param_config *param, char *sensor_name, int w, int h, int fps, int wdr)
{
	int ret = 0;
	char isp_cfg_path[128], buf[50], rdval = 1;
	FILE *fd = NULL;
	struct isp_log_cfg log_cfg;
	struct isp_lib_context *ctx = isp_dev_get_ctx(isp);

	if (strcmp(sensor_name, "")) {
		sprintf(isp_cfg_path, "%sisp_tuning", path);

		fd = fopen(isp_cfg_path, "r");
		if (fd != NULL) {
			if (fgets(buf, 50, fd) != NULL) {
				rdval = atoi(buf);
			}
			fclose(fd);
			fd = NULL;
		} else {
			ISP_ERR("%s:%s open failed\n", __FUNCTION__, isp_cfg_path);
			return -1;
		}

		if (!rdval) {
			//cfg
			sprintf(isp_cfg_path, "%s%d_%d_%d_%d/cfg", path, w, h, fps, wdr);
			fd = fopen(isp_cfg_path, "r");
			if (fd != NULL) {
				fread(param, sizeof(struct isp_param_config), 1, fd);

				fclose(fd);
				fd = NULL;
			} else {
				ISP_ERR("%s:%s open failed\n", __FUNCTION__, isp_cfg_path);
				return -1;
			}
			//ir
			sprintf(isp_cfg_path, "%s%d_%d_%d_%d/ir", path, w, h, fps, wdr);
			fd = fopen(isp_cfg_path, "r");
			if (fd != NULL) {
				if (fgets(buf, 50, fd) != NULL) {
					rdval = atoi(buf);
				}
				fclose(fd);
				fd = NULL;
				if (rdval != ctx->isp_ir_flag) {
					ctx->isp_ir_flag = rdval;
					if (ctx->isp_ir_flag) {
						ctx->tune.effect = ISP_COLORFX_GRAY;
						ctx->isp_3a_change_flags |= ISP_SET_EFFECT;
					} else {
						ctx->tune.effect = ISP_COLORFX_NONE;
						ctx->isp_3a_change_flags |= ISP_SET_EFFECT;
					}
				}
			} else {
				ISP_ERR("%s:%s open failed\n", __FUNCTION__, isp_cfg_path);
				return -1;
			}
			//colorspace
			sprintf(isp_cfg_path, "%s%d_%d_%d_%d/colorspace", path, w, h, fps, wdr);
			fd = fopen(isp_cfg_path, "r");
			if (fd != NULL) {
				if (fgets(buf, 50, fd) != NULL) {
					rdval = atoi(buf);
				}
				fclose(fd);
				fd = NULL;
				if (rdval != ctx->sensor_info.color_space) {
					ctx->sensor_info.color_space = rdval;
					ctx->isp_3a_change_flags |= ISP_SET_EFFECT;
				}
			} else {
				ISP_ERR("%s:%s open failed\n", __FUNCTION__, isp_cfg_path);
				return -1;
			}
		} else {
			ret = 1;
		}

		//log
		sprintf(isp_cfg_path, "%s%d_%d_%d_%d/LOG/flag", path, w, h, fps, wdr);
		fd = fopen(isp_cfg_path, "r");
		if (fd != NULL) {
			if (fgets(buf, 50, fd) != NULL) {
				rdval = atoi(buf);
			}
			fclose(fd);
			fd = NULL;
		} else {
			ISP_ERR("%s:%s open failed\n", __FUNCTION__, isp_cfg_path);
		}

		if (!rdval) {
			sprintf(isp_cfg_path, "%s%d_%d_%d_%d/LOG/Info", path, w, h, fps, wdr);
			log_cfg.ae_log.ae_frame_id = ctx->ae_frame_cnt;
			log_cfg.ae_log.ev_sensor_exp_line = ctx->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_sensor_exp_line;
			log_cfg.ae_log.ev_exposure_time = ctx->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_exposure_time;
			log_cfg.ae_log.ev_analog_gain = ctx->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_analog_gain;
			log_cfg.ae_log.ev_digital_gain = ctx->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_digital_gain;
			log_cfg.ae_log.ev_total_gain = ctx->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_total_gain;
			log_cfg.ae_log.ev_idx = ctx->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_idx;
			log_cfg.ae_log.ev_idx_max = ctx->ae_entity_ctx.ae_result.sensor_set.ev_idx_max;
			log_cfg.ae_log.ev_lv = ctx->ae_entity_ctx.ae_result.sensor_set.ev_set_curr.ev_lv;
			log_cfg.ae_log.ev_lv_adj = ctx->ae_entity_ctx.ae_result.ev_lv_adj;
			log_cfg.ae_log.ae_target = ctx->ae_entity_ctx.ae_result.ae_target;
			log_cfg.ae_log.ae_avg_lum = ctx->ae_entity_ctx.ae_result.ae_avg_lum;
			log_cfg.ae_log.ae_weight_lum = ctx->ae_entity_ctx.ae_result.ae_weight_lum;
			log_cfg.ae_log.ae_delta_exp_idx = ctx->ae_entity_ctx.ae_result.ae_delta_exp_idx;

			log_cfg.awb_log.awb_frame_id = ctx->awb_frame_cnt;
			log_cfg.awb_log.r_gain = ctx->awb_entity_ctx.awb_result.wb_gain_output.r_gain;
			log_cfg.awb_log.gr_gain = ctx->awb_entity_ctx.awb_result.wb_gain_output.gr_gain;
			log_cfg.awb_log.gb_gain = ctx->awb_entity_ctx.awb_result.wb_gain_output.gb_gain;
			log_cfg.awb_log.b_gain = ctx->awb_entity_ctx.awb_result.wb_gain_output.b_gain;
			log_cfg.awb_log.color_temp_output = ctx->awb_entity_ctx.awb_result.color_temp_output;

			log_cfg.af_log.af_frame_id = ctx->af_frame_cnt;
			log_cfg.af_log.last_code_output = ctx->af_entity_ctx.af_result.last_code_output;
			log_cfg.af_log.real_code_output = ctx->af_entity_ctx.af_result.real_code_output;
			log_cfg.af_log.std_code_output = ctx->af_entity_ctx.af_result.std_code_output;

			log_cfg.iso_log.gain_idx = ctx->iso_entity_ctx.iso_result.gain_idx;
			log_cfg.iso_log.lum_idx = ctx->iso_entity_ctx.iso_result.lum_idx;

			log_cfg.pltm_log.pltm_auto_stren = ctx->pltm_entity_ctx.pltm_result.pltm_auto_stren;
			fd = fopen(isp_cfg_path, "w+");
			fwrite(&log_cfg, sizeof(struct isp_log_cfg), 1, fd);
			fclose(fd);
			fd = NULL;

			sprintf(isp_cfg_path, "echo 1 > %s%d_%d_%d_%d/LOG/flag", path, w, h, fps, wdr);
			system(isp_cfg_path);
		}
	} else {
		ISP_ERR("%s:sensor cfg name is invalid\n", __FUNCTION__);
		return -1;
	}
	return ret;
}

void isp_ini_tuning_run(struct hw_isp_device *isp)
{
	static unsigned char ini_tuning_en = 0;
	int ret = 0;
	struct isp_lib_context *ctx = isp_dev_get_ctx(isp);
	struct isp_tuning *tuning = isp_dev_get_tuning(isp);

	if (ctx == NULL || tuning == NULL)
		return;

	if (!ini_tuning_en && access("/tmp/isp_tuning", F_OK) == 0) {
		ini_tuning_en = 1;
		system("echo 0 > /tmp/isp_tuning");
		save_ini_tuning_file(isp, "/tmp/", &tuning->params, ctx->sensor_info.name, ctx->sensor_info.sensor_width,
			ctx->sensor_info.sensor_height, ctx->sensor_info.fps_fixed, ctx->sensor_info.wdr_mode);
		system("echo 1 > /tmp/isp_tuning");
		ISP_PRINT("%s:init ini tuning cfg OK!\n", __FUNCTION__);
	}

	if (ini_tuning_en) {
		ret = load_ini_tuning_file(isp, "/tmp/", &tuning->params, ctx->sensor_info.name, ctx->sensor_info.sensor_width,
			ctx->sensor_info.sensor_height, ctx->sensor_info.fps_fixed, ctx->sensor_info.wdr_mode);
		if (!ret) {
			if (isp_tuning_update(isp)) {
				ISP_ERR("error: unable to update isp tuning\n");
			}
			system("echo 1 > /tmp/isp_tuning");
			ISP_PRINT("%s:read ini tuning cfg OK!\n", __FUNCTION__);
		}
	}
}

int isp_sensor_otp_init(struct hw_isp_device *isp)
{
	struct isp_lib_context *ctx = isp_dev_get_ctx(isp);
	int ret = 0;
	int i = 0;

	if (ctx == NULL)
		return -1;

	// shading
	ctx->pmsc_table = malloc(ISP_MSC_TBL_LENGTH*sizeof(unsigned short));
	memset(ctx->pmsc_table, 0, ISP_MSC_TBL_LENGTH*sizeof(unsigned short));

	if (!ctx->pmsc_table) {
		ISP_ERR("msc_table alloc failed, no memory!\n");
		return -1;
	}

	ctx->otp_enable = 1;
	ret = ioctl(isp->sensor.fd, VIDIOC_VIN_GET_SENSOR_OTP_INFO, ctx->pmsc_table);
	if(ret < 0) {
		ISP_WARN("VIDIOC_VIN_GET_SENSOR_OTP_INFO return error:%s\n", strerror(errno));
		ctx->otp_enable = -1;
	}
	//ctx->otp_enable = -1;
	ctx->pwb_table = (void *)((HW_U32)(ctx->pmsc_table) + 16*16*3*sizeof(unsigned short));

	return 0;
}

int isp_sensor_otp_exit(struct hw_isp_device *isp)
{
	struct isp_lib_context *ctx = isp_dev_get_ctx(isp);

	if (ctx == NULL)
		return -1;

	// shading
	free(ctx->pmsc_table);

	return 0;
}

int isp_config_sensor_info(struct hw_isp_device *isp)
{
	struct sensor_config cfg;
	struct isp_lib_context *ctx = isp_dev_get_ctx(isp);
	int i = 0, j = 0;
	memset(&cfg, 0, sizeof(cfg));

	if (ctx == NULL)
		return -1;

	ctx->sensor_info.name = isp_dev_get_sensor_name(isp);
	if (!ctx->sensor_info.name)
		return -1;

	isp_sensor_get_configs(isp, &cfg);
	FUNCTION_LOG;

	ctx->sensor_info.sensor_width = cfg.width;
	ctx->sensor_info.sensor_height = cfg.height;
	ctx->sensor_info.fps_fixed = cfg.fps_fixed;
	ctx->sensor_info.wdr_mode = cfg.wdr_mode;
	// BT601_FULL_RANGE
	ctx->sensor_info.color_space = 1;

	switch (cfg.mbus_code) {
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		ctx->sensor_info.bayer_bit = 8;
		ctx->sensor_info.input_seq = ISP_BGGR;
		break;
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		ctx->sensor_info.bayer_bit = 10;
		ctx->sensor_info.input_seq = ISP_BGGR;
		break;
	case V4L2_MBUS_FMT_SBGGR12_1X12:
		ctx->sensor_info.bayer_bit = 12;
		ctx->sensor_info.input_seq = ISP_BGGR;
		break;
	case V4L2_MBUS_FMT_SGBRG8_1X8:
		ctx->sensor_info.bayer_bit = 8;
		ctx->sensor_info.input_seq = ISP_GBRG;
		break;
	case V4L2_MBUS_FMT_SGBRG10_1X10:
		ctx->sensor_info.bayer_bit = 10;
		ctx->sensor_info.input_seq = ISP_GBRG;
		break;
	case V4L2_MBUS_FMT_SGBRG12_1X12:
		ctx->sensor_info.bayer_bit = 12;
		ctx->sensor_info.input_seq = ISP_GBRG;
		break;
	case V4L2_MBUS_FMT_SGRBG8_1X8:
		ctx->sensor_info.bayer_bit = 8;
		ctx->sensor_info.input_seq = ISP_GRBG;
		break;
	case V4L2_MBUS_FMT_SGRBG10_1X10:
		ctx->sensor_info.bayer_bit = 10;
		ctx->sensor_info.input_seq = ISP_GRBG;
		break;
	case V4L2_MBUS_FMT_SGRBG12_1X12:
		ctx->sensor_info.bayer_bit = 12;
		ctx->sensor_info.input_seq = ISP_GRBG;
		break;
	case V4L2_MBUS_FMT_SRGGB8_1X8:
		ctx->sensor_info.bayer_bit = 8;
		ctx->sensor_info.input_seq = ISP_RGGB;
		break;
	case V4L2_MBUS_FMT_SRGGB10_1X10:
		ctx->sensor_info.bayer_bit = 10;
		ctx->sensor_info.input_seq = ISP_RGGB;
		break;
	case V4L2_MBUS_FMT_SRGGB12_1X12:
		ctx->sensor_info.bayer_bit = 12;
		ctx->sensor_info.input_seq = ISP_RGGB;
		break;
	default:
		ctx->sensor_info.bayer_bit = 10;
		ctx->sensor_info.input_seq = ISP_BGGR;
		break;
	}

	if (cfg.hts && cfg.vts && cfg.pclk) {
		ctx->sensor_info.hts = cfg.hts;
		ctx->sensor_info.vts = cfg.vts;
		ctx->sensor_info.pclk = cfg.pclk;
		ctx->sensor_info.bin_factor = cfg.bin_factor;
		ctx->sensor_info.gain_min = cfg.gain_min;
		ctx->sensor_info.gain_max = cfg.gain_max;
		ctx->sensor_info.hoffset = cfg.hoffset;
		ctx->sensor_info.voffset = cfg.voffset;

	} else {
		ctx->sensor_info.hts = cfg.width;
		ctx->sensor_info.vts = cfg.height;
		ctx->sensor_info.pclk = cfg.width * cfg.height * 30;
		ctx->sensor_info.bin_factor = 1;
		ctx->sensor_info.gain_min = 16;
		ctx->sensor_info.gain_max = 255;
		ctx->sensor_info.hoffset = 0;
		ctx->sensor_info.voffset = 0;
	}

	ctx->stat.pic_size.width = cfg.width;
	ctx->stat.pic_size.height = cfg.height;

	ctx->stats_ctx.pic_w = cfg.width;
	ctx->stats_ctx.pic_h = cfg.height;
#if 1
	// update otp infomation
	if(ctx->otp_enable == -1){
		ISP_PRINT("otp disabled, msc use 1024\n");
		for(i = 0; i < 16*16*3; i++) {
			((unsigned short*)(ctx->pmsc_table))[i] = 1024;
		}
	}
#endif
	return 0;
}

int isp_tuning_update(struct hw_isp_device *isp)
{
	struct isp_tuning *tuning;

	tuning = isp_dev_get_tuning(isp);
	if (tuning == NULL)
		return -1;

	if (tuning->ctx == NULL)
		return -1;
	pthread_mutex_lock(&tuning->mutex);
	tuning->ctx->isp_ini_cfg = tuning->params;
	pthread_mutex_unlock(&tuning->mutex);
	isp_ctx_config_update(tuning->ctx);
	return 0;
}

int isp_tuning_reset(struct hw_isp_device *isp, struct isp_param_config *param)
{
	struct isp_tuning *tuning;

	tuning = isp_dev_get_tuning(isp);
	if (tuning == NULL)
		return -1;

	if (tuning->ctx == NULL)
		return -1;
	pthread_mutex_lock(&tuning->mutex);
	tuning->params = *param;
	pthread_mutex_unlock(&tuning->mutex);
	isp_ctx_config_reset(tuning->ctx);
	return 0;
}

struct isp_tuning * isp_tuning_init(struct hw_isp_device *isp,
			const struct isp_param_config *params)
{
	struct isp_tuning *tuning;

	tuning = malloc(sizeof(struct isp_tuning));
	if (tuning == NULL)
		return NULL;
	memset(tuning, 0, sizeof(*tuning));

	tuning->isp = isp;
	tuning->frame_count = 0;

	tuning->ctx = isp_dev_get_ctx(isp);
	if (tuning->ctx == NULL) {
		ISP_ERR("ISP context is not init!\n");
		free(tuning);
		return NULL;
	}
	FUNCTION_LOG;
	tuning->params = *params;

	isp_ctx_config_init(tuning->ctx);

	isp_dev_banding_tuning(isp, tuning);

	FUNCTION_LOG;

	pthread_mutex_init(&tuning->mutex, NULL);

	return tuning;
}

void isp_tuning_exit(struct hw_isp_device *isp)
{
	struct isp_tuning *tuning;

	tuning = isp_dev_get_tuning(isp);
	if (tuning == NULL)
		return;
	pthread_mutex_destroy(&tuning->mutex);
	isp_ctx_config_exit(tuning->ctx);
	isp_dev_unbanding_tuning(isp);
	free(tuning);
}

HW_S32 isp_tuning_get_cfg_run(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, struct isp_param_config *params, void *cfg_data)
{
	HW_S32 ret = 0;
	unsigned char *data_ptr = (unsigned char *)cfg_data;

	if (!cfg_data)
		return AW_ERR_VI_INVALID_PARA;

	switch (group_id)
	{
	case HW_ISP_CFG_TEST: /* isp_test_param */
		if (cfg_ids & HW_ISP_CFG_TEST_PUB) /* isp_test_pub */
		{
			struct isp_test_pub_cfg *isp_test_pub = (struct isp_test_pub_cfg *)data_ptr;
			isp_test_pub->test_mode = params->isp_test_settings.isp_test_mode;
			isp_test_pub->gain = params->isp_test_settings.isp_gain;
			isp_test_pub->exp_line = params->isp_test_settings.isp_exp_line;
			isp_test_pub->color_temp = params->isp_test_settings.isp_color_temp;
			isp_test_pub->log_param = params->isp_test_settings.isp_log_param;

			/* offset */
			data_ptr += sizeof(struct isp_test_pub_cfg);
			ret += sizeof(struct isp_test_pub_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_EXPTIME) /* isp_test_exptime */
		{
			struct isp_test_item_cfg *isp_test_exptime = (struct isp_test_item_cfg *)data_ptr;
			isp_test_exptime->enable = params->isp_test_settings.isp_test_exptime;
			isp_test_exptime->start = params->isp_test_settings.exp_line_start;
			isp_test_exptime->step = params->isp_test_settings.exp_line_step;
			isp_test_exptime->end = params->isp_test_settings.exp_line_end;
			isp_test_exptime->change_interval = params->isp_test_settings.exp_change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_GAIN) /* isp_test_gain */
		{
			struct isp_test_item_cfg *isp_test_gain = (struct isp_test_item_cfg *)data_ptr;
			isp_test_gain->enable = params->isp_test_settings.isp_test_gain;
			isp_test_gain->start = params->isp_test_settings.gain_start;
			isp_test_gain->step = params->isp_test_settings.gain_step;
			isp_test_gain->end = params->isp_test_settings.gain_end;
			isp_test_gain->change_interval = params->isp_test_settings.gain_change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_FOCUS) /* isp_test_focus */
		{
			struct isp_test_item_cfg *isp_test_focus = (struct isp_test_item_cfg *)data_ptr;
			isp_test_focus->enable = params->isp_test_settings.isp_test_focus;
			isp_test_focus->start = params->isp_test_settings.focus_start;
			isp_test_focus->step = params->isp_test_settings.focus_step;
			isp_test_focus->end = params->isp_test_settings.focus_end;
			isp_test_focus->change_interval = params->isp_test_settings.focus_change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_FORCED) /* isp_test_forced */
		{
			struct isp_test_forced_cfg *isp_test_forced = (struct isp_test_forced_cfg *)data_ptr;
			isp_test_forced->ae_enable = params->isp_test_settings.ae_forced;
			isp_test_forced->lum = params->isp_test_settings.lum_forced;

			/* offset */
			data_ptr += sizeof(struct isp_test_forced_cfg);
			ret += sizeof(struct isp_test_forced_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_ENABLE) /* isp_test_enable */
		{
			memcpy(data_ptr, &(params->isp_test_settings.manual_en), sizeof(struct isp_test_enable_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_test_enable_cfg);
			ret += sizeof(struct isp_test_enable_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_SPECIAL_CTRL) /* isp_test_special_ctrl */
		{
			struct isp_test_special_ctrl_cfg *isp_test_ctrl = (struct isp_test_special_ctrl_cfg *)data_ptr;
			struct isp_lib_context *ctx;
			if (isp != NULL) {
				ctx = isp_dev_get_ctx(isp);
				isp_test_ctrl->ir_mode = ctx->isp_ir_flag;
				isp_test_ctrl->color_space = ctx->sensor_info.color_space;
			}

			/* offset */
			data_ptr += sizeof(struct isp_test_special_ctrl_cfg);
			ret += sizeof(struct isp_test_special_ctrl_cfg);
		}
		break;
	case HW_ISP_CFG_3A: /* isp_3a_param */
		if (cfg_ids & HW_ISP_CFG_AE_PUB) /* isp_ae_pub */
		{
			struct isp_ae_pub_cfg *isp_ae_pub = (struct isp_ae_pub_cfg *)data_ptr;
			isp_ae_pub->define_table = params->isp_3a_settings.define_ae_table;
			isp_ae_pub->max_lv = params->isp_3a_settings.ae_max_lv;
			isp_ae_pub->hist_mode_en = params->isp_3a_settings.ae_hist_mod_en;
			isp_ae_pub->hist0_sel = params->isp_3a_settings.ae_hist0_sel;
			isp_ae_pub->hist1_sel = params->isp_3a_settings.ae_hist1_sel;
			isp_ae_pub->stat_sel = params->isp_3a_settings.ae_stat_sel;
			isp_ae_pub->compensation_step = params->isp_3a_settings.exp_comp_step;
			isp_ae_pub->touch_dist_index = params->isp_3a_settings.ae_touch_dist_ind;
			isp_ae_pub->iso2gain_ratio = params->isp_3a_settings.ae_iso2gain_ratio;
			memcpy(&isp_ae_pub->fno_table[0], &params->isp_3a_settings.ae_fno_step[0],
				sizeof(isp_ae_pub->fno_table));
			isp_ae_pub->ev_step = params->isp_3a_settings.ae_ev_step;
			isp_ae_pub->conv_data_index = params->isp_3a_settings.ae_ConvDataIndex;
			isp_ae_pub->blowout_pre_en = params->isp_3a_settings.ae_blowout_pre_en;
			isp_ae_pub->blowout_attr = params->isp_3a_settings.ae_blowout_attr;

			/* offset */
			data_ptr += sizeof(struct isp_ae_pub_cfg);
			ret += sizeof(struct isp_ae_pub_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_PREVIEW_TBL) /* isp_ae_preview_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_preview_tbl = (struct isp_ae_table_cfg *)data_ptr;
			isp_ae_preview_tbl->length = params->isp_3a_settings.ae_table_preview_length;
			memcpy(&(isp_ae_preview_tbl->value[0]), params->isp_3a_settings.ae_table_preview,
				sizeof(isp_ae_preview_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_CAPTURE_TBL) /* isp_ae_capture_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_capture_tbl = (struct isp_ae_table_cfg *)data_ptr;
			isp_ae_capture_tbl->length = params->isp_3a_settings.ae_table_capture_length;
			memcpy(&(isp_ae_capture_tbl->value[0]), params->isp_3a_settings.ae_table_capture,
				sizeof(isp_ae_capture_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_VIDEO_TBL) /* isp_ae_video_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_video_tbl = (struct isp_ae_table_cfg *)data_ptr;
			isp_ae_video_tbl->length = params->isp_3a_settings.ae_table_video_length;
			memcpy(&(isp_ae_video_tbl->value[0]), params->isp_3a_settings.ae_table_video,
				sizeof(isp_ae_video_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_WIN_WEIGHT) /* isp_ae_win_weight */
		{
			memcpy(data_ptr, params->isp_3a_settings.ae_win_weight, sizeof(struct isp_ae_weight_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_ae_weight_cfg);
			ret += sizeof(struct isp_ae_weight_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_DELAY) /* isp_ae_delay */
		{
			struct isp_ae_delay_cfg *isp_ae_delay = (struct isp_ae_delay_cfg *)data_ptr;
			isp_ae_delay->ae_frame = params->isp_3a_settings.ae_delay_frame;
			isp_ae_delay->exp_frame = params->isp_3a_settings.exp_delay_frame;
			isp_ae_delay->gain_frame = params->isp_3a_settings.gain_delay_frame;

			/* offset */
			data_ptr += sizeof(struct isp_ae_delay_cfg);
			ret += sizeof(struct isp_ae_delay_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_PUB) /* isp_awb_pub */
		{
			struct isp_awb_pub_cfg *isp_awb_pub = (struct isp_awb_pub_cfg *)data_ptr;
			isp_awb_pub->interval = params->isp_3a_settings.awb_interval;
			isp_awb_pub->speed = params->isp_3a_settings.awb_speed;
			isp_awb_pub->stat_sel = params->isp_3a_settings.awb_stat_sel;

			/* offset */
			data_ptr += sizeof(struct isp_awb_pub_cfg);
			ret += sizeof(struct isp_awb_pub_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_TEMP_RANGE) /* isp_awb_temp_range */
		{
			struct isp_awb_temp_range_cfg *isp_awb_temp_range = (struct isp_awb_temp_range_cfg *)data_ptr;
			isp_awb_temp_range->low = params->isp_3a_settings.awb_color_temper_low;
			isp_awb_temp_range->high = params->isp_3a_settings.awb_color_temper_high;
			isp_awb_temp_range->base = params->isp_3a_settings.awb_base_temper;

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_range_cfg);
			ret += sizeof(struct isp_awb_temp_range_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_DIST) /* isp_awb_dist */
		{
			struct isp_awb_dist_cfg *isp_awb_dist = (struct isp_awb_dist_cfg *)data_ptr;
			isp_awb_dist->green_zone = params->isp_3a_settings.awb_green_zone_dist;
			isp_awb_dist->blue_sky = params->isp_3a_settings.awb_blue_sky_dist;

			/* offset */
			data_ptr += sizeof(struct isp_awb_dist_cfg);
			ret += sizeof(struct isp_awb_dist_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_LIGHT_INFO) /* isp_awb_light_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_light_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			isp_awb_light_info->number = params->isp_3a_settings.awb_light_num;
			memcpy(isp_awb_light_info->value, params->isp_3a_settings.awb_light_info,
				sizeof(params->isp_3a_settings.awb_light_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_EXT_LIGHT_INFO) /* isp_awb_ext_light_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_ext_light_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			isp_awb_ext_light_info->number = params->isp_3a_settings.awb_ext_light_num;
			memcpy(isp_awb_ext_light_info->value, params->isp_3a_settings.awb_ext_light_info,
				sizeof(params->isp_3a_settings.awb_ext_light_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_SKIN_INFO) /* isp_awb_skin_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_skin_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			isp_awb_skin_info->number = params->isp_3a_settings.awb_skin_color_num;
			memcpy(isp_awb_skin_info->value, params->isp_3a_settings.awb_skin_color_info,
				sizeof(params->isp_3a_settings.awb_skin_color_info));  // !!!be careful, 160 -> 320

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_SPECIAL_INFO) /* isp_awb_special_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_special_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			isp_awb_special_info->number = params->isp_3a_settings.awb_special_color_num;
			memcpy(isp_awb_special_info->value, params->isp_3a_settings.awb_special_color_info,
				sizeof(params->isp_3a_settings.awb_special_color_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_PRESET_GAIN) /* isp_awb_preset_gain */
		{
			memcpy(data_ptr, params->isp_3a_settings.awb_preset_gain, sizeof(struct isp_awb_preset_gain_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_awb_preset_gain_cfg);
			ret += sizeof(struct isp_awb_preset_gain_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_FAVOR) /* isp_awb_favor */
		{
			struct isp_awb_favor_cfg *isp_awb_favor = (struct isp_awb_favor_cfg *)data_ptr;
			isp_awb_favor->rgain = params->isp_3a_settings.awb_rgain_favor;
			isp_awb_favor->bgain = params->isp_3a_settings.awb_bgain_favor;

			/* offset */
			data_ptr += sizeof(struct isp_awb_favor_cfg);
			ret += sizeof(struct isp_awb_favor_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_VCM_CODE) /* isp_af_vcm_code */
		{
			struct isp_af_vcm_code_cfg *isp_af_vcm_code = (struct isp_af_vcm_code_cfg *)data_ptr;
			isp_af_vcm_code->min = params->isp_3a_settings.vcm_min_code;
			isp_af_vcm_code->max = params->isp_3a_settings.vcm_max_code;

			/* offset */
			data_ptr += sizeof(struct isp_af_vcm_code_cfg);
			ret += sizeof(struct isp_af_vcm_code_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_OTP) /* isp_af_otp */
		{
			struct isp_af_otp_cfg *isp_af_otp = (struct isp_af_otp_cfg *)data_ptr;
			isp_af_otp->use_otp = params->isp_3a_settings.af_use_otp;

			/* offset */
			data_ptr += sizeof(struct isp_af_otp_cfg);
			ret += sizeof(struct isp_af_otp_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_SPEED) /* isp_af_speed */
		{
			struct isp_af_speed_cfg *isp_af_speed = (struct isp_af_speed_cfg *)data_ptr;
			isp_af_speed->interval_time = params->isp_3a_settings.af_interval_time;
			isp_af_speed->index = params->isp_3a_settings.af_speed_ind;

			/* offset */
			data_ptr += sizeof(struct isp_af_speed_cfg);
			ret += sizeof(struct isp_af_speed_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_FINE_SEARCH) /* isp_af_fine_search */
		{
			struct isp_af_fine_search_cfg *isp_af_fine_search = (struct isp_af_fine_search_cfg *)data_ptr;
			isp_af_fine_search->auto_en = params->isp_3a_settings.af_auto_fine_en;
			isp_af_fine_search->single_en = params->isp_3a_settings.af_single_fine_en;
			isp_af_fine_search->step = params->isp_3a_settings.af_fine_step;

			/* offset */
			data_ptr += sizeof(struct isp_af_fine_search_cfg);
			ret += sizeof(struct isp_af_fine_search_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_REFOCUS) /* isp_af_refocus */
		{
			struct isp_af_refocus_cfg *isp_af_refocus = (struct isp_af_refocus_cfg *)data_ptr;
			isp_af_refocus->move_cnt = params->isp_3a_settings.af_move_cnt;
			isp_af_refocus->still_cnt = params->isp_3a_settings.af_still_cnt;
			isp_af_refocus->move_monitor_cnt = params->isp_3a_settings.af_move_monitor_cnt;
			isp_af_refocus->still_monitor_cnt = params->isp_3a_settings.af_still_monitor_cnt;

			/* offset */
			data_ptr += sizeof(struct isp_af_refocus_cfg);
			ret += sizeof(struct isp_af_refocus_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_TOLERANCE) /* isp_af_tolerance */
		{
			struct isp_af_tolerance_cfg *isp_af_tolerance = (struct isp_af_tolerance_cfg *)data_ptr;
			isp_af_tolerance->near_distance = params->isp_3a_settings.af_near_tolerance;
			isp_af_tolerance->far_distance = params->isp_3a_settings.af_far_tolerance;
			isp_af_tolerance->offset = params->isp_3a_settings.af_tolerance_off;
			isp_af_tolerance->table_length = params->isp_3a_settings.af_tolerance_tbl_len;
			memcpy(isp_af_tolerance->std_code_table, params->isp_3a_settings.af_std_code_tbl,
				sizeof(isp_af_tolerance->std_code_table));
			memcpy(isp_af_tolerance->value, params->isp_3a_settings.af_tolerance_value_tbl,
				sizeof(isp_af_tolerance->value));

			/* offset */
			data_ptr += sizeof(struct isp_af_tolerance_cfg);
			ret += sizeof(struct isp_af_tolerance_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_SCENE) /* isp_af_scene */
		{
			struct isp_af_scene_cfg *isp_af_scene = (struct isp_af_scene_cfg *)data_ptr;
			isp_af_scene->stable_min = params->isp_3a_settings.af_stable_min;
			isp_af_scene->stable_max = params->isp_3a_settings.af_stable_max;
			isp_af_scene->low_light_lv = params->isp_3a_settings.af_low_light_lv;
			isp_af_scene->peak_thres = params->isp_3a_settings.af_peak_th;
			isp_af_scene->direction_thres = params->isp_3a_settings.af_dir_th;
			isp_af_scene->change_ratio = params->isp_3a_settings.af_change_ratio;
			isp_af_scene->move_minus = params->isp_3a_settings.af_move_minus;
			isp_af_scene->still_minus = params->isp_3a_settings.af_still_minus;
			isp_af_scene->scene_motion_thres = params->isp_3a_settings.af_scene_motion_th;

			/* offset */
			data_ptr += sizeof(struct isp_af_scene_cfg);
			ret += sizeof(struct isp_af_scene_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_WDR_SPLIT) /* isp_wdr_split  */
		{
			struct isp_wdr_split_cfg *isp_wdr_split = (struct isp_wdr_split_cfg *)data_ptr;
			memcpy(&isp_wdr_split->wdr_split_cfg[0], &params->isp_3a_settings.wdr_split_cfg[0],
				sizeof(isp_wdr_split->wdr_split_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_wdr_split_cfg);
			ret += sizeof(struct isp_wdr_split_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_WDR_STITCH) /* isp_wdr_stitch  */
		{
			struct isp_wdr_comm_cfg *isp_wdr_cfg = (struct isp_wdr_comm_cfg *)data_ptr;
			memcpy(&isp_wdr_cfg->value[0], &params->isp_3a_settings.wdr_comm_cfg[0],
				sizeof(isp_wdr_cfg->value));

			/* offset */
			data_ptr += sizeof(struct isp_wdr_comm_cfg);
			ret += sizeof(struct isp_wdr_comm_cfg);
		}
		break;
	case HW_ISP_CFG_TUNING: /* isp_tunning_param */
		if (cfg_ids & HW_ISP_CFG_TUNING_FLASH) /* isp_tuning_flash */
		{
			struct isp_tuning_flash_cfg *isp_tuning_flash = (struct isp_tuning_flash_cfg *)data_ptr;
			isp_tuning_flash->gain = params->isp_tunning_settings.flash_gain;
			isp_tuning_flash->delay_frame = params->isp_tunning_settings.flash_delay_frame;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_flash_cfg);
			ret += sizeof(struct isp_tuning_flash_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_FLICKER) /* isp_tuning_flicker */
		{
			struct isp_tuning_flicker_cfg *isp_tuning_flicker = (struct isp_tuning_flicker_cfg *)data_ptr;
			isp_tuning_flicker->type = params->isp_tunning_settings.flicker_type;
			isp_tuning_flicker->ratio = params->isp_tunning_settings.flicker_ratio;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_flicker_cfg);
			ret += sizeof(struct isp_tuning_flicker_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_VISUAL_ANGLE) /* isp_visual_angle */
		{
			struct isp_tuning_visual_angle_cfg *isp_visual_angle = (struct isp_tuning_visual_angle_cfg *)data_ptr;
			isp_visual_angle->horizontal = params->isp_tunning_settings.hor_visual_angle;
			isp_visual_angle->vertical = params->isp_tunning_settings.ver_visual_angle;
			isp_visual_angle->focus_length = params->isp_tunning_settings.focus_length;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_visual_angle_cfg);
			ret += sizeof(struct isp_tuning_visual_angle_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_GTM) /* isp_tuning_gtm */
		{
			struct isp_tuning_gtm_cfg *isp_tuning_gtm = (struct isp_tuning_gtm_cfg *)data_ptr;
			isp_tuning_gtm->gtm_hist_sel = params->isp_tunning_settings.gtm_hist_sel;
			isp_tuning_gtm->type = params->isp_tunning_settings.gtm_type;
			isp_tuning_gtm->gamma_type = params->isp_tunning_settings.gamma_type;
			isp_tuning_gtm->auto_alpha_en = params->isp_tunning_settings.auto_alpha_en;
			isp_tuning_gtm->hist_pix_cnt= params->isp_tunning_settings.hist_pix_cnt;
			isp_tuning_gtm->dark_minval= params->isp_tunning_settings.dark_minval;
			isp_tuning_gtm->bright_minval= params->isp_tunning_settings.bright_minval;

			memcpy(isp_tuning_gtm->plum_var, params->isp_tunning_settings.plum_var,
				sizeof(isp_tuning_gtm->plum_var));
			/* offset */
			data_ptr += sizeof(struct isp_tuning_gtm_cfg);
			ret += sizeof(struct isp_tuning_gtm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CFA) /* isp_tuning_cfa */
		{
			struct isp_tuning_cfa_cfg *isp_tuning_cfa = (struct isp_tuning_cfa_cfg *)data_ptr;
			isp_tuning_cfa->grad_th = params->isp_tunning_settings.grad_th;
			isp_tuning_cfa->dir_v_th = params->isp_tunning_settings.dir_v_th;
			isp_tuning_cfa->dir_h_th = params->isp_tunning_settings.dir_h_th;
			isp_tuning_cfa->res_smth_high = params->isp_tunning_settings.res_smth_high;
			isp_tuning_cfa->res_smth_low = params->isp_tunning_settings.res_smth_low;
			isp_tuning_cfa->res_high_th = params->isp_tunning_settings.res_high_th;
			isp_tuning_cfa->res_low_th = params->isp_tunning_settings.res_low_th;
			isp_tuning_cfa->res_dir_a = params->isp_tunning_settings.res_dir_a;
			isp_tuning_cfa->res_dir_d = params->isp_tunning_settings.res_dir_d;
			isp_tuning_cfa->res_dir_v = params->isp_tunning_settings.res_dir_v;
			isp_tuning_cfa->res_dir_h = params->isp_tunning_settings.res_dir_h;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_cfa_cfg);
			ret += sizeof(struct isp_tuning_cfa_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CTC) /* isp_tuning_ctc */
		{
			struct isp_tuning_ctc_cfg *isp_tuning_ctc = (struct isp_tuning_ctc_cfg *)data_ptr;
			isp_tuning_ctc->min_thres = params->isp_tunning_settings.ctc_th_min;
			isp_tuning_ctc->max_thres = params->isp_tunning_settings.ctc_th_max;
			isp_tuning_ctc->slope_thres = params->isp_tunning_settings.ctc_th_slope;
			isp_tuning_ctc->dir_wt = params->isp_tunning_settings.ctc_dir_wt;
			isp_tuning_ctc->dir_thres = params->isp_tunning_settings.ctc_dir_th;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_ctc_cfg);
			ret += sizeof(struct isp_tuning_ctc_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_DIGITAL_GAIN) /* isp_digital_gain */
		{
			memcpy(data_ptr, params->isp_tunning_settings.bayer_gain, sizeof(struct isp_tuning_blc_gain_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_blc_gain_cfg);
			ret += sizeof(struct isp_tuning_blc_gain_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_LOW) /* isp_ccm_low */
		{
			struct isp_tuning_ccm_cfg *isp_ccm_low = (struct isp_tuning_ccm_cfg *)data_ptr;
			isp_ccm_low->temperature = params->isp_tunning_settings.ccm_trig_cfg[0];
			memcpy(&isp_ccm_low->value, &(params->isp_tunning_settings.color_matrix_ini[0]),
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_ccm_cfg);
			ret += sizeof(struct isp_tuning_ccm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_MID) /* isp_ccm_mid */
		{
			struct isp_tuning_ccm_cfg *isp_ccm_mid = (struct isp_tuning_ccm_cfg *)data_ptr;
			isp_ccm_mid->temperature = params->isp_tunning_settings.ccm_trig_cfg[1];
			memcpy(&isp_ccm_mid->value, &(params->isp_tunning_settings.color_matrix_ini[1]),
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_ccm_cfg);
			ret += sizeof(struct isp_tuning_ccm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_HIGH) /* isp_ccm_high */
		{
			struct isp_tuning_ccm_cfg *isp_ccm_high = (struct isp_tuning_ccm_cfg *)data_ptr;
			isp_ccm_high->temperature = params->isp_tunning_settings.ccm_trig_cfg[2];
			memcpy(&isp_ccm_high->value, &(params->isp_tunning_settings.color_matrix_ini[2]),
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_ccm_cfg);
			ret += sizeof(struct isp_tuning_ccm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_PLTM) /* isp_tuning_pltm */
		{
			memcpy(data_ptr, params->isp_tunning_settings.pltm_cfg, sizeof(struct isp_tuning_pltm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_pltm_cfg);
			ret += sizeof(struct isp_tuning_pltm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_GCA) /* isp_tuning_gca  */
		{
			memcpy(data_ptr, params->isp_tunning_settings.gca_cfg, sizeof(struct isp_tuning_gca_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_gca_cfg);
			ret += sizeof(struct isp_tuning_gca_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_BDNF_COMM)
		{
			memcpy(data_ptr, params->isp_tunning_settings.denoise_comm_cfg, sizeof(struct isp_tuning_bdnf_comm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_bdnf_comm_cfg);
			ret += sizeof(struct isp_tuning_bdnf_comm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_TDNF_COMM)
		{
			memcpy(data_ptr, params->isp_tunning_settings.tdf_comm_cfg, sizeof(struct isp_tuning_tdnf_comm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_tdnf_comm_cfg);
			ret += sizeof(struct isp_tuning_tdnf_comm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_SHARP_COMM)
		{
			memcpy(data_ptr, params->isp_tunning_settings.sharp_comm_cfg, sizeof(struct isp_tuning_sharp_comm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_sharp_comm_cfg);
			ret += sizeof(struct isp_tuning_sharp_comm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_DPC)
		{
			struct isp_tuning_dpc_cfg *isp_tuning_dpc = (struct isp_tuning_dpc_cfg *)data_ptr;
			isp_tuning_dpc->dpc_remove_mode = params->isp_tunning_settings.dpc_remove_mode;
			isp_tuning_dpc->dpc_sup_twinkle_en = params->isp_tunning_settings.dpc_sup_twinkle_en;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_dpc_cfg);
			ret += sizeof(struct isp_tuning_dpc_cfg);
		}
#ifdef USE_ENCPP
		if (cfg_ids & HW_ISP_CFG_TUNING_ENCPP_SHARP_COMM)
		{
			memcpy(data_ptr, params->isp_tunning_settings.encpp_sharp_comm_cfg, sizeof(struct isp_tuning_encpp_sharp_comm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_encpp_sharp_comm_cfg);
			ret += sizeof(struct isp_tuning_encpp_sharp_comm_cfg);
		}
#endif
		if (cfg_ids & HW_ISP_CFG_TUNING_SENSOR)
		{
			memcpy(data_ptr, params->isp_tunning_settings.sensor_temp, sizeof(struct isp_tuning_sensor_temp_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_sensor_temp_cfg);
			ret += sizeof(struct isp_tuning_sensor_temp_cfg);
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_COMMON)
		{
			#ifdef ANDROID_VENCODE
			memcpy(data_ptr, &vencoder_tuning_param->common_cfg, sizeof(vencoder_common_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_common_cfg_t);
			ret += sizeof(vencoder_common_cfg_t);
			#else
			memset(data_ptr, 0, sizeof(vencoder_common_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_common_cfg_t);
			ret += sizeof(vencoder_common_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_COMMON\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_VBR)
		{
			#ifdef ANDROID_VENCODE
			memcpy(data_ptr, &vencoder_tuning_param->vbr_cfg, sizeof(vencoder_vbr_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_vbr_cfg_t);
			ret += sizeof(vencoder_vbr_cfg_t);
			#else
			memset(data_ptr, 0, sizeof(vencoder_vbr_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_vbr_cfg_t);
			ret += sizeof(vencoder_vbr_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_VBR\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_AVBR)
		{
			#ifdef ANDROID_VENCODE
			memcpy(data_ptr, &vencoder_tuning_param->avbr_cfg, sizeof(vencoder_avbr_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_avbr_cfg_t);
			ret += sizeof(vencoder_avbr_cfg_t);
			#else
			memset(data_ptr, 0, sizeof(vencoder_avbr_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_avbr_cfg_t);
			ret += sizeof(vencoder_avbr_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_AVBR\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_FIXQP)
		{
			#ifdef ANDROID_VENCODE
			memcpy(data_ptr, &vencoder_tuning_param->fixqp_cfg, sizeof(vencoder_fixqp_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_fixqp_cfg_t);
			ret += sizeof(vencoder_fixqp_cfg_t);
			#else
			memset(data_ptr, 0, sizeof(vencoder_fixqp_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_fixqp_cfg_t);
			ret += sizeof(vencoder_fixqp_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_FIXQP\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_PROC)
		{
			#ifdef ANDROID_VENCODE
			memcpy(data_ptr, &vencoder_tuning_param->proc_cfg, sizeof(vencoder_proc_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_proc_cfg_t);
			ret += sizeof(vencoder_proc_cfg_t);
			#else
			memset(data_ptr, 0, sizeof(vencoder_proc_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_proc_cfg_t);
			ret += sizeof(vencoder_proc_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_PROC\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_SAVEBSFILE)
		{
			#ifdef ANDROID_VENCODE
			memcpy(data_ptr, &vencoder_tuning_param->savebsfile_cfg, sizeof(vencoder_savebsfile_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_savebsfile_cfg_t);
			ret += sizeof(vencoder_savebsfile_cfg_t);
			#else
			memset(data_ptr, 0, sizeof(vencoder_savebsfile_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_savebsfile_cfg_t);
			ret += sizeof(vencoder_savebsfile_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_SAVEBSFILE\n");
			#endif
		}
		break;
	case HW_ISP_CFG_TUNING_TABLES: /* isp tuning tables */
		if (cfg_ids & HW_ISP_CFG_TUNING_LSC) /* isp_tuning_lsc */
		{
			struct isp_tuning_lsc_table_cfg *isp_tuning_lsc = (struct isp_tuning_lsc_table_cfg *)data_ptr;
			isp_tuning_lsc->lsc_mode = params->isp_tunning_settings.lsc_mode;
			isp_tuning_lsc->ff_mod = params->isp_tunning_settings.ff_mod;
			isp_tuning_lsc->center_x = params->isp_tunning_settings.lsc_center_x;
			isp_tuning_lsc->center_y = params->isp_tunning_settings.lsc_center_y;
			isp_tuning_lsc->rolloff_ratio = params->isp_tunning_settings.rolloff_ratio;
			memcpy(&(isp_tuning_lsc->value[0][0]), &(params->isp_tunning_settings.lsc_tbl[0][0]),
				sizeof(isp_tuning_lsc->value));
			memcpy(isp_tuning_lsc->color_temp_triggers, params->isp_tunning_settings.lsc_trig_cfg,
				sizeof(isp_tuning_lsc->color_temp_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_lsc_table_cfg);
			ret += sizeof(struct isp_tuning_lsc_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_GAMMA) /* isp_tuning_gamma */
		{
			struct isp_tuning_gamma_table_cfg *isp_tuning_gamma = (struct isp_tuning_gamma_table_cfg *)data_ptr;
			isp_tuning_gamma->number = params->isp_tunning_settings.gamma_num;
			memcpy(&(isp_tuning_gamma->value[0][0]), &(params->isp_tunning_settings.gamma_tbl_ini[0][0]),
				sizeof(isp_tuning_gamma->value));
			memcpy(isp_tuning_gamma->lv_triggers, params->isp_tunning_settings.gamma_trig_cfg,
				sizeof(isp_tuning_gamma->lv_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_gamma_table_cfg);
			ret += sizeof(struct isp_tuning_gamma_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_BDNF) /* isp_tuning_bdnf */
		{
			struct isp_tuning_bdnf_table_cfg *isp_tuning_bdnf = (struct isp_tuning_bdnf_table_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++) {
				memcpy(isp_tuning_bdnf->thres[i].lp0_thres, params->isp_iso_settings.isp_dynamic_cfg[i].d2d_lp0_th, ISP_REG_TBL_LENGTH * sizeof(HW_S16));
				memcpy(isp_tuning_bdnf->thres[i].lp1_thres, params->isp_iso_settings.isp_dynamic_cfg[i].d2d_lp1_th, ISP_REG_TBL_LENGTH * sizeof(HW_S16));
				memcpy(isp_tuning_bdnf->thres[i].lp2_thres, params->isp_iso_settings.isp_dynamic_cfg[i].d2d_lp2_th, ISP_REG_TBL_LENGTH * sizeof(HW_S16));
				memcpy(isp_tuning_bdnf->thres[i].lp3_thres, params->isp_iso_settings.isp_dynamic_cfg[i].d2d_lp3_th, ISP_REG_TBL_LENGTH * sizeof(HW_S16));
			}

			/* offset */
			data_ptr += sizeof(struct isp_tuning_bdnf_table_cfg);
			ret += sizeof(struct isp_tuning_bdnf_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_TDNF) /* isp_tuning_tdnf */
		{
			struct isp_tuning_tdnf_table_cfg *isp_tuning_tdnf = (struct isp_tuning_tdnf_table_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++) {
				memcpy(&isp_tuning_tdnf->thres[i][0], params->isp_iso_settings.isp_dynamic_cfg[i].d3d_flt0_thr_vc, ISP_REG_TBL_LENGTH * sizeof(HW_S16));
			}
			memcpy(isp_tuning_tdnf->df_shape, params->isp_tunning_settings.isp_tdnf_df_shape, sizeof(isp_tuning_tdnf->df_shape));
			memcpy(isp_tuning_tdnf->ratio_amp, params->isp_tunning_settings.isp_tdnf_ratio_amp, sizeof(isp_tuning_tdnf->ratio_amp));
			memcpy(isp_tuning_tdnf->k_dlt_bk, params->isp_tunning_settings.isp_tdnf_k_dlt_bk, sizeof(isp_tuning_tdnf->k_dlt_bk));
			memcpy(isp_tuning_tdnf->ct_rt_bk, params->isp_tunning_settings.isp_tdnf_ct_rt_bk, sizeof(isp_tuning_tdnf->ct_rt_bk));
			memcpy(isp_tuning_tdnf->dtc_hf_bk, params->isp_tunning_settings.isp_tdnf_dtc_hf_bk, sizeof(isp_tuning_tdnf->dtc_hf_bk));
			memcpy(isp_tuning_tdnf->dtc_mf_bk, params->isp_tunning_settings.isp_tdnf_dtc_mf_bk, sizeof(isp_tuning_tdnf->dtc_mf_bk));
			memcpy(isp_tuning_tdnf->lay0_d2d0_rt_br, params->isp_tunning_settings.isp_tdnf_lay0_d2d0_rt_br, sizeof(isp_tuning_tdnf->lay0_d2d0_rt_br));
			memcpy(isp_tuning_tdnf->lay1_d2d0_rt_br, params->isp_tunning_settings.isp_tdnf_lay1_d2d0_rt_br, sizeof(isp_tuning_tdnf->lay1_d2d0_rt_br));
			memcpy(isp_tuning_tdnf->lay0_nrd_rt_br, params->isp_tunning_settings.isp_tdnf_lay0_nrd_rt_br, sizeof(isp_tuning_tdnf->lay0_nrd_rt_br));
			memcpy(isp_tuning_tdnf->lay1_nrd_rt_br, params->isp_tunning_settings.isp_tdnf_lay1_nrd_rt_br, sizeof(isp_tuning_tdnf->lay1_nrd_rt_br));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_tdnf_table_cfg);
			ret += sizeof(struct isp_tuning_tdnf_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_SHARP) /* isp_tuning_sharp */
		{
			struct isp_tuning_sharp_table_cfg *isp_tuning_sharp = (struct isp_tuning_sharp_table_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++) {
				memcpy(&isp_tuning_sharp->edge_lum[i][0], params->isp_iso_settings.isp_dynamic_cfg[i].sharp_edge_lum, ISP_REG_TBL_LENGTH * sizeof(HW_U16));
			}
			memcpy(isp_tuning_sharp->ss_val, params->isp_tunning_settings.isp_sharp_ss_value, sizeof(isp_tuning_sharp->ss_val));
			memcpy(isp_tuning_sharp->ls_val, params->isp_tunning_settings.isp_sharp_ls_value, sizeof(isp_tuning_sharp->ls_val));
			memcpy(isp_tuning_sharp->hsv, params->isp_tunning_settings.isp_sharp_hsv, sizeof(isp_tuning_sharp->hsv));
			/* offset */
			data_ptr += sizeof(struct isp_tuning_sharp_table_cfg);
			ret += sizeof(struct isp_tuning_sharp_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CEM) /* isp_tuning_cem */
		{
			memcpy(data_ptr, params->isp_tunning_settings.isp_cem_table, sizeof(struct isp_tuning_cem_table_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_cem_table_cfg);
			ret += sizeof(struct isp_tuning_cem_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CEM_1) /* isp_tuning_cem_1 */
		{
			memcpy(data_ptr, params->isp_tunning_settings.isp_cem_table1, sizeof(struct isp_tuning_cem_table_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_cem_table_cfg);
			ret += sizeof(struct isp_tuning_cem_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_PLTM_TBL) /* isp_tuning_pltm_table */
		{
			struct isp_tuning_pltm_table_cfg *isp_tuning_pltm = (struct isp_tuning_pltm_table_cfg *)data_ptr;
			memcpy(isp_tuning_pltm->stat_gd_cv, params->isp_tunning_settings.isp_pltm_stat_gd_cv,
				sizeof(isp_tuning_pltm->stat_gd_cv));
			memcpy(isp_tuning_pltm->df_cv, params->isp_tunning_settings.isp_pltm_df_cv,
				sizeof(isp_tuning_pltm->df_cv));
			memcpy(isp_tuning_pltm->lum_map_cv, params->isp_tunning_settings.isp_pltm_lum_map_cv,
				sizeof(isp_tuning_pltm->lum_map_cv));
			memcpy(isp_tuning_pltm->gtm_tbl, params->isp_tunning_settings.isp_pltm_gtm_tbl,
				sizeof(isp_tuning_pltm->gtm_tbl));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_pltm_table_cfg);
			ret += sizeof(struct isp_tuning_pltm_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_WDR) /* isp_tuning_wdr_tbl */
		{
			struct isp_tuning_wdr_table_cfg *isp_tuning_wdr = (struct isp_tuning_wdr_table_cfg *)data_ptr;
			memcpy(isp_tuning_wdr->wdr_de_purpl_hsv_tbl, params->isp_tunning_settings.isp_wdr_de_purpl_hsv_tbl,
				sizeof(isp_tuning_wdr->wdr_de_purpl_hsv_tbl));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_wdr_table_cfg);
			ret += sizeof(struct isp_tuning_wdr_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_LCA_TBL) /* isp_tuning_lca  */
		{
			memcpy(data_ptr, params->isp_tunning_settings.lca_pf_satu_lut, sizeof(struct isp_tuning_lca_pf_satu_lut));
			/* lca_pf_satu_lut offset */
			data_ptr += sizeof(struct isp_tuning_lca_pf_satu_lut);
			ret += sizeof(struct isp_tuning_lca_pf_satu_lut);

			memcpy(data_ptr, params->isp_tunning_settings.lca_gf_satu_lut, sizeof(struct isp_tuning_lca_gf_satu_lut));
			/* lca_gf_satu_lut offset */
			data_ptr += sizeof(struct isp_tuning_lca_gf_satu_lut);
			ret += sizeof(struct isp_tuning_lca_gf_satu_lut);

		}
		if (cfg_ids & HW_ISP_CFG_TUNING_MSC) /* isp_tuning_msc */
		{
			struct isp_tuning_msc_table_cfg *isp_tuning_msc = (struct isp_tuning_msc_table_cfg *)data_ptr;
			isp_tuning_msc->mff_mod = params->isp_tunning_settings.mff_mod;
			isp_tuning_msc->msc_mode = params->isp_tunning_settings.msc_mode;
			memcpy(isp_tuning_msc->msc_blw_lut, params->isp_tunning_settings.msc_blw_lut,
				sizeof(isp_tuning_msc->msc_blw_lut));
			memcpy(isp_tuning_msc->msc_blh_lut, params->isp_tunning_settings.msc_blh_lut,
				sizeof(isp_tuning_msc->msc_blh_lut));
			memcpy(&(isp_tuning_msc->value[0][0]), &(params->isp_tunning_settings.msc_tbl[0][0]),
				sizeof(isp_tuning_msc->value));
			memcpy(isp_tuning_msc->color_temp_triggers, params->isp_tunning_settings.msc_trig_cfg,
				sizeof(isp_tuning_msc->color_temp_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_msc_table_cfg);
			ret += sizeof(struct isp_tuning_msc_table_cfg);
		}
#ifdef USE_ENCPP
		if (cfg_ids & HW_ISP_CFG_TUNING_ENCPP_SHARP) /* isp_tuning_encpp_sharp */
		{
			struct isp_tuning_sharp_table_cfg *isp_tuning_encpp_sharp = (struct isp_tuning_sharp_table_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++) {
				memcpy(&isp_tuning_encpp_sharp->edge_lum[i][0], params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_edge_lum, ISP_REG_TBL_LENGTH * sizeof(HW_U16));
			}
			memcpy(isp_tuning_encpp_sharp->ss_val, params->isp_tunning_settings.encpp_sharp_ss_value, sizeof(isp_tuning_encpp_sharp->ss_val));
			memcpy(isp_tuning_encpp_sharp->ls_val, params->isp_tunning_settings.encpp_sharp_ls_value, sizeof(isp_tuning_encpp_sharp->ls_val));
			memcpy(isp_tuning_encpp_sharp->hsv, params->isp_tunning_settings.encpp_sharp_hsv, sizeof(isp_tuning_encpp_sharp->hsv));
			/* offset */
			data_ptr += sizeof(struct isp_tuning_sharp_table_cfg);
			ret += sizeof(struct isp_tuning_sharp_table_cfg);
		}
#endif
		break;
	case HW_ISP_CFG_DYNAMIC: /* isp_dynamic_param */
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_LUM_POINT) /* isp_dynamic_lum_mapping_point */
		{
			memcpy(data_ptr, params->isp_iso_settings.isp_lum_mapping_point, sizeof(struct isp_dynamic_single_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_GAIN_POINT) /* isp_dynamic_gain_mapping_point */
		{
			memcpy(data_ptr, params->isp_iso_settings.isp_gain_mapping_point, sizeof(struct isp_dynamic_single_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_SHARP) /* isp_dynamic_sharp */
		{
			struct isp_dynamic_sharp_cfg *isp_dynamic_sharp = (struct isp_dynamic_sharp_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_sharp->trigger = params->isp_iso_settings.triger.sharp_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(isp_dynamic_sharp->tuning_ss_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg[0],
				        sizeof(isp_dynamic_sharp->tuning_ss_cfg[i].value));
				memcpy(isp_dynamic_sharp->tuning_ls_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg[ISP_SHARP_LS_NS_LW],
				        sizeof(isp_dynamic_sharp->tuning_ls_cfg[i].value));
				memcpy(isp_dynamic_sharp->tuning_hfr_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg[ISP_SHARP_HFR_SMTH_RATIO],
				        sizeof(isp_dynamic_sharp->tuning_hfr_cfg[i].value));
				memcpy(isp_dynamic_sharp->tuning_comm_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg[ISP_SHARP_DIR_SMTH0],
				        sizeof(isp_dynamic_sharp->tuning_comm_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_sharp_cfg);
			ret += sizeof(struct isp_dynamic_sharp_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_DENOISE) /* isp_dynamic_denoise */
		{
			struct isp_dynamic_denoise_cfg *isp_dynamic_denoise = (struct isp_dynamic_denoise_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_denoise->trigger = params->isp_iso_settings.triger.denoise_triger;
			isp_dynamic_denoise->color_trigger = params->isp_iso_settings.triger.color_denoise_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(isp_dynamic_denoise->tuning_dnr_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].denoise_cfg[0],
					sizeof(isp_dynamic_denoise->tuning_dnr_cfg[i].value));
				isp_dynamic_denoise->tuning_dnr_cfg[i].color_denoise = params->isp_iso_settings.isp_dynamic_cfg[i].color_denoise;
				memcpy(isp_dynamic_denoise->tuning_dtc_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].denoise_cfg[ISP_DENOISE_HF_SMTH_RATIO],
					sizeof(isp_dynamic_denoise->tuning_dtc_cfg[i].value));
				memcpy(isp_dynamic_denoise->tuning_wdr_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].denoise_cfg[ISP_DENOISE_LYR0_DNR_LM_AMP],
					sizeof(isp_dynamic_denoise->tuning_wdr_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_denoise_cfg);
			ret += sizeof(struct isp_dynamic_denoise_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_BLACK_LV) /* isp_dynamic_black_level */
		{
			struct isp_dynamic_black_level_cfg *isp_dynamic_black_level = (struct isp_dynamic_black_level_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_black_level->trigger = params->isp_iso_settings.triger.black_level_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_dynamic_black_level->tuning_cfg[i].value, params->isp_iso_settings.isp_dynamic_cfg[i].black_level,
					sizeof(isp_dynamic_black_level->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_black_level_cfg);
			ret += sizeof(struct isp_dynamic_black_level_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_DPC) /* isp_dynamic_dpcl */
		{
			struct isp_dynamic_dpc_cfg *isp_dynamic_dpc = (struct isp_dynamic_dpc_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_dpc->trigger = params->isp_iso_settings.triger.dpc_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_dynamic_dpc->tuning_cfg[i].value, params->isp_iso_settings.isp_dynamic_cfg[i].dpc_cfg,
					sizeof(isp_dynamic_dpc->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_dpc_cfg);
			ret += sizeof(struct isp_dynamic_dpc_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_PLTM) /* isp_dynamic_pltm */
		{
			struct isp_dynamic_pltm_cfg *isp_dynamic_pltm = (struct isp_dynamic_pltm_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_pltm->trigger = params->isp_iso_settings.triger.pltm_dynamic_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_dynamic_pltm->tuning_cfg[i].value, params->isp_iso_settings.isp_dynamic_cfg[i].pltm_dynamic_cfg,
					sizeof(isp_dynamic_pltm->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_pltm_cfg);
			ret += sizeof(struct isp_dynamic_pltm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_DEFOG) /* isp_dynamic_defog */
		{
			struct isp_dynamic_defog_cfg *isp_dynamic_defog = (struct isp_dynamic_defog_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_defog->trigger = params->isp_iso_settings.triger.defog_value_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				isp_dynamic_defog->tuning_cfg[i].value = params->isp_iso_settings.isp_dynamic_cfg[i].defog_value;

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_defog_cfg);
			ret += sizeof(struct isp_dynamic_defog_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_HISTOGRAM) /* isp_dynamic_histogram */
		{
			struct isp_dynamic_histogram_cfg *isp_dynamic_histogram = (struct isp_dynamic_histogram_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_histogram->brightness_trigger = params->isp_iso_settings.triger.brightness_triger;
			isp_dynamic_histogram->contrast_trigger = params->isp_iso_settings.triger.gcontrast_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				isp_dynamic_histogram->tuning_cfg[i].brightness = params->isp_iso_settings.isp_dynamic_cfg[i].brightness;
				isp_dynamic_histogram->tuning_cfg[i].contrast = params->isp_iso_settings.isp_dynamic_cfg[i].contrast;
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_histogram_cfg);
			ret += sizeof(struct isp_dynamic_histogram_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_CEM) /* isp_dynamic_cem */
		{
			struct isp_dynamic_cem_cfg *isp_dynamic_cem = (struct isp_dynamic_cem_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_cem->trigger = params->isp_iso_settings.triger.cem_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_dynamic_cem->tuning_cfg[i].value, params->isp_iso_settings.isp_dynamic_cfg[i].cem_cfg,
					sizeof(isp_dynamic_cem->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_cem_cfg);
			ret += sizeof(struct isp_dynamic_cem_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_TDF) /* isp_dynamic_tdf */
		{
			struct isp_dynamic_tdf_cfg *isp_dynamic_tdf = (struct isp_dynamic_tdf_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_tdf->trigger = params->isp_iso_settings.triger.tdf_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(isp_dynamic_tdf->tuning_dnr_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg[0],
					sizeof(isp_dynamic_tdf->tuning_dnr_cfg[i].value));
				memcpy(isp_dynamic_tdf->tuning_mtd_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg[ISP_TDF_DIFF_INTRA_SENS],
					sizeof(isp_dynamic_tdf->tuning_mtd_cfg[i].value));
				memcpy(isp_dynamic_tdf->tuning_dtc_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg[ISP_TDF_DTC_HF_COR],
					sizeof(isp_dynamic_tdf->tuning_dtc_cfg[i].value));
				memcpy(isp_dynamic_tdf->tuning_srd_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg[ISP_TDF_D2D0_CNR_STREN],
					sizeof(isp_dynamic_tdf->tuning_srd_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_tdf_cfg);
			ret += sizeof(struct isp_dynamic_tdf_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_AE) /* isp_dynamic_ae */
		{
			struct isp_dynamic_ae_cfg *isp_dynamic_ae = (struct isp_dynamic_ae_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_ae->trigger = params->isp_iso_settings.triger.ae_cfg_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_dynamic_ae->tuning_cfg[i].value, params->isp_iso_settings.isp_dynamic_cfg[i].ae_cfg,
					sizeof(isp_dynamic_ae->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_ae_cfg);
			ret += sizeof(struct isp_dynamic_ae_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_GTM) /* isp_dynamic_gtm */
		{
			struct isp_dynamic_gtm_cfg *isp_dynamic_gtm = (struct isp_dynamic_gtm_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_gtm->trigger = params->isp_iso_settings.triger.gtm_cfg_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_dynamic_gtm->tuning_cfg[i].value, params->isp_iso_settings.isp_dynamic_cfg[i].gtm_cfg,
					sizeof(isp_dynamic_gtm->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_gtm_cfg);
			ret += sizeof(struct isp_dynamic_gtm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_LCA) /* isp_dynamic_lca */
		{
			struct isp_dynamic_lca_cfg *isp_dynamic_lca = (struct isp_dynamic_lca_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_lca->trigger = params->isp_iso_settings.triger.lca_cfg_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_dynamic_lca->tuning_cfg[i].value, params->isp_iso_settings.isp_dynamic_cfg[i].lca_cfg,
					sizeof(isp_dynamic_lca->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_lca_cfg);
			ret += sizeof(struct isp_dynamic_lca_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_CFA) /* isp_dynamic_cfa */
		{
			struct isp_dynamic_cfa_cfg *isp_dynamic_cfa = (struct isp_dynamic_cfa_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_cfa->trigger = params->isp_iso_settings.triger.cfa_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(isp_dynamic_cfa->tuning_cfg[i].value, params->isp_iso_settings.isp_dynamic_cfg[i].cfa_cfg,
					sizeof(isp_dynamic_cfa->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_cfa_cfg);
			ret += sizeof(struct isp_dynamic_cfa_cfg);
		}
#ifdef USE_ENCPP
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_ENCPP_SHARP) /* isp_dynamic_encpp_sharp */
		{
			struct isp_dynamic_encpp_sharp_cfg *isp_dynamic_encpp_sharp = (struct isp_dynamic_encpp_sharp_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_encpp_sharp->trigger = params->isp_iso_settings.triger.encpp_sharp_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(isp_dynamic_encpp_sharp->tuning_ss_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_cfg[0],
				        sizeof(isp_dynamic_encpp_sharp->tuning_ss_cfg[i].value));
				memcpy(isp_dynamic_encpp_sharp->tuning_ls_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_cfg[ENCPP_SHARP_LS_NS_LW],
				        sizeof(isp_dynamic_encpp_sharp->tuning_ls_cfg[i].value));
				memcpy(isp_dynamic_encpp_sharp->tuning_hfr_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_cfg[ENCPP_SHARP_HFR_SMTH_RATIO],
				        sizeof(isp_dynamic_encpp_sharp->tuning_hfr_cfg[i].value));
				memcpy(isp_dynamic_encpp_sharp->tuning_comm_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_cfg[ENCPP_SHARP_DIR_SMTH0],
				        sizeof(isp_dynamic_encpp_sharp->tuning_comm_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_encpp_sharp_cfg);
			ret += sizeof(struct isp_dynamic_encpp_sharp_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_ENCODER_DENOISE) /* isp_dynamic_encoder_denoise */
		{
			struct isp_dynamic_encoder_denoise_cfg *isp_dynamic_encoder_denoise = (struct isp_dynamic_encoder_denoise_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_encoder_denoise->trigger = params->isp_iso_settings.triger.encoder_denoise_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(isp_dynamic_encoder_denoise->tuning_3dnr_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].encoder_denoise_cfg[0],
				        sizeof(isp_dynamic_encoder_denoise->tuning_3dnr_cfg[i].value));
				memcpy(isp_dynamic_encoder_denoise->tuning_2dnr_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].encoder_denoise_cfg[ENCODER_DENOISE_2D_FILT_STREN_UV],
				        sizeof(isp_dynamic_encoder_denoise->tuning_2dnr_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_encoder_denoise_cfg);
			ret += sizeof(struct isp_dynamic_encoder_denoise_cfg);
		}
#endif
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_WDR) /* isp_dynamic_wdr */
		{
			struct isp_dynamic_wdr_cfg *isp_dynamic_wdr = (struct isp_dynamic_wdr_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_wdr->trigger = params->isp_iso_settings.triger.wdr_cfg_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(isp_dynamic_wdr->tuning_lm_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].wdr_cfg[0],
						sizeof(isp_dynamic_wdr->tuning_lm_cfg[i].value));
				memcpy(isp_dynamic_wdr->tuning_ms_cfg[i].value, &params->isp_iso_settings.isp_dynamic_cfg[i].wdr_cfg[WDR_CMP_MS_LTH],
							sizeof(isp_dynamic_wdr->tuning_ms_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_wdr_cfg);
			ret += sizeof(struct isp_dynamic_wdr_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_SHADING) /* isp_dynamic_shading */
		{
			struct isp_dynamic_shading_cfg *isp_dynamic_shading = (struct isp_dynamic_shading_cfg *)data_ptr;
			int i = 0;
			isp_dynamic_shading->trigger = params->isp_iso_settings.triger.shading_triger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				isp_dynamic_shading->tuning_cfg[i].shading_comp = params->isp_iso_settings.isp_dynamic_cfg[i].shading_comp;
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_shading_cfg);
			ret += sizeof(struct isp_dynamic_shading_cfg);
		}
		break;
	default:
		ret = AW_ERR_VI_INVALID_PARA;
		break;
	}

	data_ptr = NULL;
	return ret;
}

HW_S32 isp_tuning_get_cfg(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data)
{
	HW_S32 ret = AW_ERR_VI_INVALID_PARA;
	struct isp_tuning *tuning = NULL;

	if (!isp || !cfg_data)
		return AW_ERR_VI_INVALID_PARA;

	/* call isp api */
	tuning = isp_dev_get_tuning(isp);
	if (tuning == NULL)
		 return AW_ERR_VI_INVALID_NULL_PTR;

	/* fill cfg data */
	ret = isp_tuning_get_cfg_run(isp, group_id, cfg_ids, &tuning->params, cfg_data);

	return ret;
}

HW_S32 isp_tuning_set_cfg_run(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, struct isp_param_config *params, void *cfg_data)
{
	HW_S32 ret = 0;
	unsigned char *data_ptr = (unsigned char *)cfg_data;

	if (!cfg_data)
		return AW_ERR_VI_INVALID_PARA;

	switch (group_id)
	{
	case HW_ISP_CFG_TEST: /* isp_test_param */
		if (cfg_ids & HW_ISP_CFG_TEST_PUB) /* isp_test_pub */
		{
			struct isp_test_pub_cfg *isp_test_pub = (struct isp_test_pub_cfg *)data_ptr;
			params->isp_test_settings.isp_test_mode = isp_test_pub->test_mode;
			params->isp_test_settings.isp_gain = isp_test_pub->gain;
			params->isp_test_settings.isp_exp_line = isp_test_pub->exp_line;
			params->isp_test_settings.isp_color_temp = isp_test_pub->color_temp;
			params->isp_test_settings.isp_log_param = isp_test_pub->log_param;
			if (isp != NULL && params->isp_test_settings.isp_test_mode == 4) { //Manual mode
				struct sensor_exp_gain exp_gain;
				exp_gain.exp_val = params->isp_test_settings.isp_exp_line;
				exp_gain.gain_val = params->isp_test_settings.isp_gain / 16;
				exp_gain.r_gain = 0;
				exp_gain.b_gain = 0;
				isp_sensor_set_exp_gain(isp, &exp_gain);
			}

			/* offset */
			data_ptr += sizeof(struct isp_test_pub_cfg);
			ret += sizeof(struct isp_test_pub_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_EXPTIME) /* isp_test_exptime */
		{
			struct isp_test_item_cfg *isp_test_exptime = (struct isp_test_item_cfg *)data_ptr;
			params->isp_test_settings.isp_test_exptime = isp_test_exptime->enable;
			params->isp_test_settings.exp_line_start = isp_test_exptime->start;
			params->isp_test_settings.exp_line_step = isp_test_exptime->step;
			params->isp_test_settings.exp_line_end = isp_test_exptime->end;
			params->isp_test_settings.exp_change_interval = isp_test_exptime->change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_GAIN) /* isp_test_gain */
		{
			struct isp_test_item_cfg *isp_test_gain = (struct isp_test_item_cfg *)data_ptr;
			params->isp_test_settings.isp_test_gain = isp_test_gain->enable;
			params->isp_test_settings.gain_start = isp_test_gain->start;
			params->isp_test_settings.gain_step = isp_test_gain->step;
			params->isp_test_settings.gain_end = isp_test_gain->end;
			params->isp_test_settings.gain_change_interval = isp_test_gain->change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_FOCUS) /* isp_test_focus */
		{
			struct isp_test_item_cfg *isp_test_focus = (struct isp_test_item_cfg *)data_ptr;
			params->isp_test_settings.isp_test_focus = isp_test_focus->enable;
			params->isp_test_settings.focus_start = isp_test_focus->start;
			params->isp_test_settings.focus_step = isp_test_focus->step;
			params->isp_test_settings.focus_end = isp_test_focus->end;
			params->isp_test_settings.focus_change_interval = isp_test_focus->change_interval;

			/* offset */
			data_ptr += sizeof(struct isp_test_item_cfg);
			ret += sizeof(struct isp_test_item_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_FORCED) /* isp_test_forced */
		{
			struct isp_test_forced_cfg *isp_test_forced = (struct isp_test_forced_cfg *)data_ptr;
			params->isp_test_settings.ae_forced = isp_test_forced->ae_enable;
			params->isp_test_settings.lum_forced = isp_test_forced->lum;

			/* offset */
			data_ptr += sizeof(struct isp_test_forced_cfg);
			ret += sizeof(struct isp_test_forced_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_ENABLE) /* isp_test_enable */
		{
			memcpy(&(params->isp_test_settings.manual_en), data_ptr, sizeof(struct isp_test_enable_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_test_enable_cfg);
			ret += sizeof(struct isp_test_enable_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TEST_SPECIAL_CTRL) /* isp_test_special_ctrl */
		{
			struct isp_test_special_ctrl_cfg *isp_test_ctrl = (struct isp_test_special_ctrl_cfg *)data_ptr;
			struct isp_lib_context *ctx;
			if (isp != NULL) {
				ctx = isp_dev_get_ctx(isp);
				if (isp_test_ctrl->ir_mode != ctx->isp_ir_flag) {
					ctx->isp_ir_flag = isp_test_ctrl->ir_mode;
					if (ctx->isp_ir_flag) {
						ctx->tune.effect = ISP_COLORFX_GRAY;
						ctx->isp_3a_change_flags |= ISP_SET_EFFECT;
					} else {
						ctx->tune.effect = ISP_COLORFX_NONE;
						ctx->isp_3a_change_flags |= ISP_SET_EFFECT;
					}
				}
				if (isp_test_ctrl->color_space != ctx->sensor_info.color_space) {
					ctx->sensor_info.color_space = isp_test_ctrl->color_space;
					ctx->isp_3a_change_flags |= ISP_SET_EFFECT;
				}
			}

			/* offset */
			data_ptr += sizeof(struct isp_test_special_ctrl_cfg);
			ret += sizeof(struct isp_test_special_ctrl_cfg);
		}
		break;
	case HW_ISP_CFG_3A: /* isp_3a_param */
		if (cfg_ids & HW_ISP_CFG_AE_PUB) /* isp_ae_pub */
		{
			struct isp_ae_pub_cfg *isp_ae_pub = (struct isp_ae_pub_cfg *)data_ptr;
			params->isp_3a_settings.define_ae_table = isp_ae_pub->define_table;
			params->isp_3a_settings.ae_max_lv = isp_ae_pub->max_lv;
			params->isp_3a_settings.ae_hist_mod_en = isp_ae_pub->hist_mode_en;
			params->isp_3a_settings.ae_hist0_sel = isp_ae_pub->hist0_sel;
			params->isp_3a_settings.ae_hist1_sel = isp_ae_pub->hist1_sel;
			params->isp_3a_settings.ae_stat_sel = isp_ae_pub->stat_sel;
			params->isp_3a_settings.exp_comp_step = isp_ae_pub->compensation_step;
			params->isp_3a_settings.ae_touch_dist_ind = isp_ae_pub->touch_dist_index;
			params->isp_3a_settings.ae_iso2gain_ratio = isp_ae_pub->iso2gain_ratio;
			memcpy(&params->isp_3a_settings.ae_fno_step[0], &isp_ae_pub->fno_table[0],
				sizeof(isp_ae_pub->fno_table));
			params->isp_3a_settings.ae_ev_step = isp_ae_pub->ev_step;
			params->isp_3a_settings.ae_ConvDataIndex = isp_ae_pub->conv_data_index;
			params->isp_3a_settings.ae_blowout_pre_en = isp_ae_pub->blowout_pre_en;
			params->isp_3a_settings.ae_blowout_attr = isp_ae_pub->blowout_attr;

			/* offset */
			data_ptr += sizeof(struct isp_ae_pub_cfg);
			ret += sizeof(struct isp_ae_pub_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_PREVIEW_TBL) /* isp_ae_preview_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_preview_tbl = (struct isp_ae_table_cfg *)data_ptr;
			params->isp_3a_settings.ae_table_preview_length = isp_ae_preview_tbl->length;
			memcpy(params->isp_3a_settings.ae_table_preview, &(isp_ae_preview_tbl->value[0]),
				sizeof(isp_ae_preview_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_CAPTURE_TBL) /* isp_ae_capture_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_capture_tbl = (struct isp_ae_table_cfg *)data_ptr;
			params->isp_3a_settings.ae_table_capture_length = isp_ae_capture_tbl->length;
			memcpy(params->isp_3a_settings.ae_table_capture, &(isp_ae_capture_tbl->value[0]),
				sizeof(isp_ae_capture_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_VIDEO_TBL) /* isp_ae_video_tbl */
		{
			struct isp_ae_table_cfg *isp_ae_video_tbl = (struct isp_ae_table_cfg *)data_ptr;
			params->isp_3a_settings.ae_table_video_length = isp_ae_video_tbl->length;
			memcpy(params->isp_3a_settings.ae_table_video, &(isp_ae_video_tbl->value[0]),
				sizeof(isp_ae_video_tbl->value));

			/* offset */
			data_ptr += sizeof(struct isp_ae_table_cfg);
			ret += sizeof(struct isp_ae_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_WIN_WEIGHT) /* isp_ae_win_weight */
		{
			memcpy(params->isp_3a_settings.ae_win_weight, data_ptr, sizeof(struct isp_ae_weight_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_ae_weight_cfg);
			ret += sizeof(struct isp_ae_weight_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AE_DELAY) /* isp_ae_delay */
		{
			struct isp_ae_delay_cfg *isp_ae_delay = (struct isp_ae_delay_cfg *)data_ptr;
			params->isp_3a_settings.ae_delay_frame = isp_ae_delay->ae_frame;
			params->isp_3a_settings.exp_delay_frame = isp_ae_delay->exp_frame;
			params->isp_3a_settings.gain_delay_frame = isp_ae_delay->gain_frame;

			/* offset */
			data_ptr += sizeof(struct isp_ae_delay_cfg);
			ret += sizeof(struct isp_ae_delay_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_PUB) /* isp_awb_pub */
		{
			struct isp_awb_pub_cfg *isp_awb_pub = (struct isp_awb_pub_cfg *)data_ptr;
			params->isp_3a_settings.awb_interval = isp_awb_pub->interval;
			params->isp_3a_settings.awb_speed = isp_awb_pub->speed;
			params->isp_3a_settings.awb_stat_sel = isp_awb_pub->stat_sel;

			/* offset */
			data_ptr += sizeof(struct isp_awb_pub_cfg);
			ret += sizeof(struct isp_awb_pub_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_TEMP_RANGE) /* isp_awb_temp_range */
		{
			struct isp_awb_temp_range_cfg *isp_awb_temp_range = (struct isp_awb_temp_range_cfg *)data_ptr;
			params->isp_3a_settings.awb_color_temper_low = isp_awb_temp_range->low;
			params->isp_3a_settings.awb_color_temper_high = isp_awb_temp_range->high;
			params->isp_3a_settings.awb_base_temper = isp_awb_temp_range->base;

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_range_cfg);
			ret += sizeof(struct isp_awb_temp_range_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_DIST) /* isp_awb_dist */
		{
			struct isp_awb_dist_cfg *isp_awb_dist = (struct isp_awb_dist_cfg *)data_ptr;
			params->isp_3a_settings.awb_green_zone_dist = isp_awb_dist->green_zone;
			params->isp_3a_settings.awb_blue_sky_dist = isp_awb_dist->blue_sky;

			/* offset */
			data_ptr += sizeof(struct isp_awb_dist_cfg);
			ret += sizeof(struct isp_awb_dist_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_LIGHT_INFO) /* isp_awb_light_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_light_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			params->isp_3a_settings.awb_light_num = isp_awb_light_info->number;
			memcpy(params->isp_3a_settings.awb_light_info, isp_awb_light_info->value,
				sizeof(params->isp_3a_settings.awb_light_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_EXT_LIGHT_INFO) /* isp_awb_ext_light_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_ext_light_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			params->isp_3a_settings.awb_ext_light_num = isp_awb_ext_light_info->number;
			memcpy(params->isp_3a_settings.awb_ext_light_info, isp_awb_ext_light_info->value,
				sizeof(params->isp_3a_settings.awb_ext_light_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_SKIN_INFO) /* isp_awb_skin_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_skin_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			params->isp_3a_settings.awb_skin_color_num = isp_awb_skin_info->number;
			memcpy(params->isp_3a_settings.awb_skin_color_info, isp_awb_skin_info->value,
				sizeof(params->isp_3a_settings.awb_skin_color_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_SPECIAL_INFO) /* isp_awb_special_info */
		{
			struct isp_awb_temp_info_cfg *isp_awb_special_info = (struct isp_awb_temp_info_cfg *)data_ptr;
			params->isp_3a_settings.awb_special_color_num = isp_awb_special_info->number;
			memcpy(params->isp_3a_settings.awb_special_color_info, isp_awb_special_info->value,
				sizeof(params->isp_3a_settings.awb_special_color_info));

			/* offset */
			data_ptr += sizeof(struct isp_awb_temp_info_cfg);
			ret += sizeof(struct isp_awb_temp_info_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_PRESET_GAIN) /* isp_awb_preset_gain */
		{
			memcpy(params->isp_3a_settings.awb_preset_gain, data_ptr, sizeof(struct isp_awb_preset_gain_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_awb_preset_gain_cfg);
			ret += sizeof(struct isp_awb_preset_gain_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AWB_FAVOR) /* isp_awb_favor */
		{
			struct isp_awb_favor_cfg *isp_awb_favor = (struct isp_awb_favor_cfg *)data_ptr;
			params->isp_3a_settings.awb_rgain_favor = isp_awb_favor->rgain;
			params->isp_3a_settings.awb_bgain_favor = isp_awb_favor->bgain;

			/* offset */
			data_ptr += sizeof(struct isp_awb_favor_cfg);
			ret += sizeof(struct isp_awb_favor_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_VCM_CODE) /* isp_af_vcm_code */
		{
			struct isp_af_vcm_code_cfg *isp_af_vcm_code = (struct isp_af_vcm_code_cfg *)data_ptr;
			params->isp_3a_settings.vcm_min_code = isp_af_vcm_code->min;
			params->isp_3a_settings.vcm_max_code = isp_af_vcm_code->max;

			/* offset */
			data_ptr += sizeof(struct isp_af_vcm_code_cfg);
			ret += sizeof(struct isp_af_vcm_code_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_OTP) /* isp_af_otp */
		{
			struct isp_af_otp_cfg *isp_af_otp = (struct isp_af_otp_cfg *)data_ptr;
			params->isp_3a_settings.af_use_otp = isp_af_otp->use_otp;

			/* offset */
			data_ptr += sizeof(struct isp_af_otp_cfg);
			ret += sizeof(struct isp_af_otp_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_SPEED) /* isp_af_speed */
		{
			struct isp_af_speed_cfg *isp_af_speed = (struct isp_af_speed_cfg *)data_ptr;
			params->isp_3a_settings.af_interval_time = isp_af_speed->interval_time;
			params->isp_3a_settings.af_speed_ind = isp_af_speed->index;

			/* offset */
			data_ptr += sizeof(struct isp_af_speed_cfg);
			ret += sizeof(struct isp_af_speed_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_FINE_SEARCH) /* isp_af_fine_search */
		{
			struct isp_af_fine_search_cfg *isp_af_fine_search = (struct isp_af_fine_search_cfg *)data_ptr;
			params->isp_3a_settings.af_auto_fine_en = isp_af_fine_search->auto_en;
			params->isp_3a_settings.af_single_fine_en = isp_af_fine_search->single_en;
			params->isp_3a_settings.af_fine_step = isp_af_fine_search->step;

			/* offset */
			data_ptr += sizeof(struct isp_af_fine_search_cfg);
			ret += sizeof(struct isp_af_fine_search_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_REFOCUS) /* isp_af_refocus */
		{
			struct isp_af_refocus_cfg *isp_af_refocus = (struct isp_af_refocus_cfg *)data_ptr;
			params->isp_3a_settings.af_move_cnt = isp_af_refocus->move_cnt;
			params->isp_3a_settings.af_still_cnt = isp_af_refocus->still_cnt;
			params->isp_3a_settings.af_move_monitor_cnt = isp_af_refocus->move_monitor_cnt;
			params->isp_3a_settings.af_still_monitor_cnt = isp_af_refocus->still_monitor_cnt;

			/* offset */
			data_ptr += sizeof(struct isp_af_refocus_cfg);
			ret += sizeof(struct isp_af_refocus_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_TOLERANCE) /* isp_af_tolerance */
		{
			struct isp_af_tolerance_cfg *isp_af_tolerance = (struct isp_af_tolerance_cfg *)data_ptr;
			params->isp_3a_settings.af_near_tolerance = isp_af_tolerance->near_distance;
			params->isp_3a_settings.af_far_tolerance = isp_af_tolerance->far_distance;
			params->isp_3a_settings.af_tolerance_off = isp_af_tolerance->offset;
			params->isp_3a_settings.af_tolerance_tbl_len = isp_af_tolerance->table_length;
			memcpy(params->isp_3a_settings.af_std_code_tbl, isp_af_tolerance->std_code_table,
				sizeof(isp_af_tolerance->std_code_table));
			memcpy(params->isp_3a_settings.af_tolerance_value_tbl, isp_af_tolerance->value,
				sizeof(isp_af_tolerance->value));

			/* offset */
			data_ptr += sizeof(struct isp_af_tolerance_cfg);
			ret += sizeof(struct isp_af_tolerance_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_AF_SCENE) /* isp_af_scene */
		{
			struct isp_af_scene_cfg *isp_af_scene = (struct isp_af_scene_cfg *)data_ptr;
			params->isp_3a_settings.af_stable_min = isp_af_scene->stable_min;
			params->isp_3a_settings.af_stable_max = isp_af_scene->stable_max;
			params->isp_3a_settings.af_low_light_lv = isp_af_scene->low_light_lv;
			params->isp_3a_settings.af_peak_th = isp_af_scene->peak_thres;
			params->isp_3a_settings.af_dir_th = isp_af_scene->direction_thres;
			params->isp_3a_settings.af_change_ratio = isp_af_scene->change_ratio;
			params->isp_3a_settings.af_move_minus = isp_af_scene->move_minus;
			params->isp_3a_settings.af_still_minus = isp_af_scene->still_minus;
			params->isp_3a_settings.af_scene_motion_th = isp_af_scene->scene_motion_thres;

			/* offset */
			data_ptr += sizeof(struct isp_af_scene_cfg);
			ret += sizeof(struct isp_af_scene_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_WDR_SPLIT) /* isp_wdr_split */
		{
			struct isp_wdr_split_cfg *isp_wdr_split = (struct isp_wdr_split_cfg *)data_ptr;
			memcpy(&params->isp_3a_settings.wdr_split_cfg[0], &isp_wdr_split->wdr_split_cfg[0],
				sizeof(isp_wdr_split->wdr_split_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_wdr_split_cfg);
			ret += sizeof(struct isp_wdr_split_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_WDR_STITCH) /* isp_wdr_stitch */
		{
			struct isp_wdr_comm_cfg *isp_wdr_cfg = (struct isp_wdr_comm_cfg *)data_ptr;
			memcpy(&params->isp_3a_settings.wdr_comm_cfg[0], &isp_wdr_cfg->value[0],
				sizeof(isp_wdr_cfg->value));

			/* offset */
			data_ptr += sizeof(struct isp_wdr_comm_cfg);
			ret += sizeof(struct isp_wdr_comm_cfg);
		}
		break;
	case HW_ISP_CFG_TUNING: /* isp_tunning_param */
		if (cfg_ids & HW_ISP_CFG_TUNING_FLASH) /* isp_flash */
		{
			struct isp_tuning_flash_cfg *isp_tuning_flash = (struct isp_tuning_flash_cfg *)data_ptr;
			params->isp_tunning_settings.flash_gain = isp_tuning_flash->gain;
			params->isp_tunning_settings.flash_delay_frame = isp_tuning_flash->delay_frame;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_flash_cfg);
			ret += sizeof(struct isp_tuning_flash_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_FLICKER) /* isp_flicker */
		{
			struct isp_tuning_flicker_cfg *isp_tuning_flicker = (struct isp_tuning_flicker_cfg *)data_ptr;
			params->isp_tunning_settings.flicker_type = isp_tuning_flicker->type;
			params->isp_tunning_settings.flicker_ratio = isp_tuning_flicker->ratio;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_flicker_cfg);
			ret += sizeof(struct isp_tuning_flicker_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_VISUAL_ANGLE) /* isp_visual_angle */
		{
			struct isp_tuning_visual_angle_cfg *isp_visual_angle = (struct isp_tuning_visual_angle_cfg *)data_ptr;
			params->isp_tunning_settings.hor_visual_angle = isp_visual_angle->horizontal;
			params->isp_tunning_settings.ver_visual_angle = isp_visual_angle->vertical;
			params->isp_tunning_settings.focus_length = isp_visual_angle->focus_length;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_visual_angle_cfg);
			ret += sizeof(struct isp_tuning_visual_angle_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_GTM) /* isp_gtm */
		{
			struct isp_tuning_gtm_cfg *isp_tuning_gtm = (struct isp_tuning_gtm_cfg *)data_ptr;
			params->isp_tunning_settings.gtm_hist_sel = isp_tuning_gtm->gtm_hist_sel;
			params->isp_tunning_settings.gtm_type = isp_tuning_gtm->type;
			params->isp_tunning_settings.gamma_type = isp_tuning_gtm->gamma_type;
			params->isp_tunning_settings.auto_alpha_en = isp_tuning_gtm->auto_alpha_en;
			params->isp_tunning_settings.hist_pix_cnt= isp_tuning_gtm->hist_pix_cnt;
			params->isp_tunning_settings.dark_minval= isp_tuning_gtm->dark_minval;
			params->isp_tunning_settings.bright_minval= isp_tuning_gtm->bright_minval;

			memcpy(params->isp_tunning_settings.plum_var, isp_tuning_gtm->plum_var,
				sizeof(isp_tuning_gtm->plum_var));
			/* offset */
			data_ptr += sizeof(struct isp_tuning_gtm_cfg);
			ret += sizeof(struct isp_tuning_gtm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CFA) /* isp_tuning_cfa */
		{
			struct isp_tuning_cfa_cfg *isp_tuning_cfa = (struct isp_tuning_cfa_cfg *)data_ptr;
			params->isp_tunning_settings.grad_th = isp_tuning_cfa->grad_th;
			params->isp_tunning_settings.dir_v_th = isp_tuning_cfa->dir_v_th;
			params->isp_tunning_settings.dir_h_th = isp_tuning_cfa->dir_h_th;
			params->isp_tunning_settings.res_smth_high = isp_tuning_cfa->res_smth_high;
			params->isp_tunning_settings.res_smth_low = isp_tuning_cfa->res_smth_low;
			params->isp_tunning_settings.res_high_th = isp_tuning_cfa->res_high_th;
			params->isp_tunning_settings.res_low_th = isp_tuning_cfa->res_low_th;
			params->isp_tunning_settings.res_dir_a = isp_tuning_cfa->res_dir_a;
			params->isp_tunning_settings.res_dir_d = isp_tuning_cfa->res_dir_d;
			params->isp_tunning_settings.res_dir_v = isp_tuning_cfa->res_dir_v;
			params->isp_tunning_settings.res_dir_h = isp_tuning_cfa->res_dir_h;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_cfa_cfg);
			ret += sizeof(struct isp_tuning_cfa_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CTC) /* isp_tuning_ctc */
		{
			struct isp_tuning_ctc_cfg *isp_tuning_ctc = (struct isp_tuning_ctc_cfg *)data_ptr;
			params->isp_tunning_settings.ctc_th_min = isp_tuning_ctc->min_thres;
			params->isp_tunning_settings.ctc_th_max = isp_tuning_ctc->max_thres;
			params->isp_tunning_settings.ctc_th_slope = isp_tuning_ctc->slope_thres;
			params->isp_tunning_settings.ctc_dir_wt = isp_tuning_ctc->dir_wt;
			params->isp_tunning_settings.ctc_dir_th = isp_tuning_ctc->dir_thres;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_ctc_cfg);
			ret += sizeof(struct isp_tuning_ctc_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_DIGITAL_GAIN) /* isp_tuning_digital_gain */
		{
			memcpy(params->isp_tunning_settings.bayer_gain, data_ptr, sizeof(struct isp_tuning_blc_gain_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_blc_gain_cfg);
			ret += sizeof(struct isp_tuning_blc_gain_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_LOW) /* isp_ccm_low */
		{
			struct isp_tuning_ccm_cfg *isp_ccm_low = (struct isp_tuning_ccm_cfg *)data_ptr;
			params->isp_tunning_settings.ccm_trig_cfg[0] = isp_ccm_low->temperature;
			memcpy(&(params->isp_tunning_settings.color_matrix_ini[0]), &isp_ccm_low->value,
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_ccm_cfg);
			ret += sizeof(struct isp_tuning_ccm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_MID) /* isp_ccm_mid */
		{
			struct isp_tuning_ccm_cfg *isp_ccm_mid = (struct isp_tuning_ccm_cfg *)data_ptr;
			params->isp_tunning_settings.ccm_trig_cfg[1] = isp_ccm_mid->temperature;
			memcpy(&(params->isp_tunning_settings.color_matrix_ini[1]), &isp_ccm_mid->value,
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_ccm_cfg);
			ret += sizeof(struct isp_tuning_ccm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CCM_HIGH) /* isp_ccm_high */
		{
			struct isp_tuning_ccm_cfg *isp_ccm_high = (struct isp_tuning_ccm_cfg *)data_ptr;
			params->isp_tunning_settings.ccm_trig_cfg[2] = isp_ccm_high->temperature;
			memcpy(&(params->isp_tunning_settings.color_matrix_ini[2]), &isp_ccm_high->value,
				sizeof(struct isp_rgb2rgb_gain_offset));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_ccm_cfg);
			ret += sizeof(struct isp_tuning_ccm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_PLTM) /* isp_tuning_pltm  */
		{
			memcpy(params->isp_tunning_settings.pltm_cfg, data_ptr, sizeof(struct isp_tuning_pltm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_pltm_cfg);
			ret += sizeof(struct isp_tuning_pltm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_GCA) /* isp_tuning_gca  */
		{
			memcpy(params->isp_tunning_settings.gca_cfg, data_ptr, sizeof(struct isp_tuning_gca_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_gca_cfg);
			ret += sizeof(struct isp_tuning_gca_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_BDNF_COMM)
		{
			memcpy(params->isp_tunning_settings.denoise_comm_cfg, data_ptr, sizeof(struct isp_tuning_bdnf_comm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_bdnf_comm_cfg);
			ret += sizeof(struct isp_tuning_bdnf_comm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_TDNF_COMM)
		{
			memcpy(params->isp_tunning_settings.tdf_comm_cfg, data_ptr, sizeof(struct isp_tuning_tdnf_comm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_tdnf_comm_cfg);
			ret += sizeof(struct isp_tuning_tdnf_comm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_SHARP_COMM)
		{
			memcpy(params->isp_tunning_settings.sharp_comm_cfg, data_ptr, sizeof(struct isp_tuning_sharp_comm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_sharp_comm_cfg);
			ret += sizeof(struct isp_tuning_sharp_comm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_DPC)
		{
			struct isp_tuning_dpc_cfg *isp_tuning_dpc = (struct isp_tuning_dpc_cfg *)data_ptr;
			params->isp_tunning_settings.dpc_remove_mode = isp_tuning_dpc->dpc_remove_mode;
			params->isp_tunning_settings.dpc_sup_twinkle_en = isp_tuning_dpc->dpc_sup_twinkle_en;

			/* offset */
			data_ptr += sizeof(struct isp_tuning_dpc_cfg);
			ret += sizeof(struct isp_tuning_dpc_cfg);
		}
#ifdef USE_ENCPP
		if (cfg_ids & HW_ISP_CFG_TUNING_ENCPP_SHARP_COMM)
		{
			memcpy(params->isp_tunning_settings.encpp_sharp_comm_cfg, data_ptr, sizeof(struct isp_tuning_encpp_sharp_comm_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_encpp_sharp_comm_cfg);
			ret += sizeof(struct isp_tuning_encpp_sharp_comm_cfg);
		}
#endif
		if (cfg_ids & HW_ISP_CFG_TUNING_SENSOR)
		{
			memcpy(params->isp_tunning_settings.sensor_temp, data_ptr, sizeof(struct isp_tuning_sensor_temp_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_sensor_temp_cfg);
			ret += sizeof(struct isp_tuning_sensor_temp_cfg);
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_COMMON)
		{
			#ifdef ANDROID_VENCODE
			memcpy(&vencoder_tuning_param->common_cfg, data_ptr, sizeof(vencoder_common_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_common_cfg_t);
			ret += sizeof(vencoder_common_cfg_t);
			#else
			/* offset */
			data_ptr += sizeof(vencoder_common_cfg_t);
			ret += sizeof(vencoder_common_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_COMMON\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_VBR)
		{
			#ifdef ANDROID_VENCODE
			memcpy(&vencoder_tuning_param->vbr_cfg, data_ptr, sizeof(vencoder_vbr_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_vbr_cfg_t);
			ret += sizeof(vencoder_vbr_cfg_t);
			#else
			/* offset */
			data_ptr += sizeof(vencoder_vbr_cfg_t);
			ret += sizeof(vencoder_vbr_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_VBR\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_AVBR)
		{
			#ifdef ANDROID_VENCODE
			memcpy(&vencoder_tuning_param->avbr_cfg, data_ptr, sizeof(vencoder_avbr_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_avbr_cfg_t);
			ret += sizeof(vencoder_avbr_cfg_t);
			#else
			/* offset */
			data_ptr += sizeof(vencoder_avbr_cfg_t);
			ret += sizeof(vencoder_avbr_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_AVBR\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_FIXQP)
		{
			#ifdef ANDROID_VENCODE
			memcpy(&vencoder_tuning_param->fixqp_cfg, data_ptr, sizeof(vencoder_fixqp_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_fixqp_cfg_t);
			ret += sizeof(vencoder_fixqp_cfg_t);
			#else
			/* offset */
			data_ptr += sizeof(vencoder_fixqp_cfg_t);
			ret += sizeof(vencoder_fixqp_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_FIXQP\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_PROC)
		{
			#ifdef ANDROID_VENCODE
			memcpy(&vencoder_tuning_param->proc_cfg, data_ptr, sizeof(vencoder_proc_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_proc_cfg_t);
			ret += sizeof(vencoder_proc_cfg_t);
			#else
			/* offset */
			data_ptr += sizeof(vencoder_proc_cfg_t);
			ret += sizeof(vencoder_proc_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_PROC\n");
			#endif
		}
		if (cfg_ids & HW_VENCODER_CFG_TUNING_SAVEBSFILE)
		{
			#ifdef ANDROID_VENCODE
			memcpy(&vencoder_tuning_param->savebsfile_cfg, data_ptr, sizeof(vencoder_savebsfile_cfg_t));

			/* offset */
			data_ptr += sizeof(vencoder_savebsfile_cfg_t);
			ret += sizeof(vencoder_savebsfile_cfg_t);
			#else
			/* offset */
			data_ptr += sizeof(vencoder_savebsfile_cfg_t);
			ret += sizeof(vencoder_savebsfile_cfg_t);
			ISP_WARN("App in board does't have preview vencode feature, unable to get HW_VENCODER_CFG_TUNING_SAVEBSFILE\n");
			#endif
		}
		break;
	case HW_ISP_CFG_TUNING_TABLES: /* isp tuning tables*/
		if (cfg_ids & HW_ISP_CFG_TUNING_LSC) /* isp_lsc */
		{
			struct isp_tuning_lsc_table_cfg *isp_tuning_lsc = (struct isp_tuning_lsc_table_cfg *)data_ptr;
			params->isp_tunning_settings.lsc_mode = isp_tuning_lsc->lsc_mode;
			params->isp_tunning_settings.ff_mod = isp_tuning_lsc->ff_mod;
			params->isp_tunning_settings.lsc_center_x = isp_tuning_lsc->center_x;
			params->isp_tunning_settings.lsc_center_y = isp_tuning_lsc->center_y;
			params->isp_tunning_settings.rolloff_ratio = isp_tuning_lsc->rolloff_ratio;
			memcpy(&(params->isp_tunning_settings.lsc_tbl[0][0]), &(isp_tuning_lsc->value[0][0]),
				sizeof(isp_tuning_lsc->value));
			memcpy(params->isp_tunning_settings.lsc_trig_cfg, isp_tuning_lsc->color_temp_triggers,
				sizeof(isp_tuning_lsc->color_temp_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_lsc_table_cfg);
			ret += sizeof(struct isp_tuning_lsc_table_cfg);
		}

		if (cfg_ids & HW_ISP_CFG_TUNING_GAMMA) /* isp_gamma */
		{
			struct isp_tuning_gamma_table_cfg *isp_tuning_gamma = (struct isp_tuning_gamma_table_cfg *)data_ptr;
			params->isp_tunning_settings.gamma_num = isp_tuning_gamma->number;
			memcpy(&(params->isp_tunning_settings.gamma_tbl_ini[0][0]), &(isp_tuning_gamma->value[0][0]),
				sizeof(isp_tuning_gamma->value));
			memcpy(params->isp_tunning_settings.gamma_trig_cfg, isp_tuning_gamma->lv_triggers,
				sizeof(isp_tuning_gamma->lv_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_gamma_table_cfg);
			ret += sizeof(struct isp_tuning_gamma_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_BDNF) /* isp_tuning_bdnf */
		{
			struct isp_tuning_bdnf_table_cfg *isp_tuning_bdnf = (struct isp_tuning_bdnf_table_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++) {
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].d2d_lp0_th, isp_tuning_bdnf->thres[i].lp0_thres, ISP_REG_TBL_LENGTH * sizeof(HW_S16));
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].d2d_lp1_th, isp_tuning_bdnf->thres[i].lp1_thres, ISP_REG_TBL_LENGTH * sizeof(HW_S16));
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].d2d_lp2_th, isp_tuning_bdnf->thres[i].lp2_thres, ISP_REG_TBL_LENGTH * sizeof(HW_S16));
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].d2d_lp3_th, isp_tuning_bdnf->thres[i].lp3_thres, ISP_REG_TBL_LENGTH * sizeof(HW_S16));
			}

			/* offset */
			data_ptr += sizeof(struct isp_tuning_bdnf_table_cfg);
			ret += sizeof(struct isp_tuning_bdnf_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_TDNF) /* isp_tuning_tdnf */
		{
			struct isp_tuning_tdnf_table_cfg *isp_tuning_tdnf = (struct isp_tuning_tdnf_table_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++) {
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].d3d_flt0_thr_vc, &isp_tuning_tdnf->thres[i][0], ISP_REG_TBL_LENGTH * sizeof(HW_S16));
			}
			memcpy(params->isp_tunning_settings.isp_tdnf_df_shape, isp_tuning_tdnf->df_shape, sizeof(isp_tuning_tdnf->df_shape));
			memcpy(params->isp_tunning_settings.isp_tdnf_ratio_amp, isp_tuning_tdnf->ratio_amp, sizeof(isp_tuning_tdnf->ratio_amp));
			memcpy(params->isp_tunning_settings.isp_tdnf_k_dlt_bk, isp_tuning_tdnf->k_dlt_bk, sizeof(isp_tuning_tdnf->k_dlt_bk));
			memcpy(params->isp_tunning_settings.isp_tdnf_ct_rt_bk, isp_tuning_tdnf->ct_rt_bk, sizeof(isp_tuning_tdnf->ct_rt_bk));
			memcpy(params->isp_tunning_settings.isp_tdnf_dtc_hf_bk, isp_tuning_tdnf->dtc_hf_bk, sizeof(isp_tuning_tdnf->dtc_hf_bk));
			memcpy(params->isp_tunning_settings.isp_tdnf_dtc_mf_bk, isp_tuning_tdnf->dtc_mf_bk, sizeof(isp_tuning_tdnf->dtc_mf_bk));
			memcpy(params->isp_tunning_settings.isp_tdnf_lay0_d2d0_rt_br, isp_tuning_tdnf->lay0_d2d0_rt_br, sizeof(isp_tuning_tdnf->lay0_d2d0_rt_br));
			memcpy(params->isp_tunning_settings.isp_tdnf_lay1_d2d0_rt_br, isp_tuning_tdnf->lay1_d2d0_rt_br, sizeof(isp_tuning_tdnf->lay1_d2d0_rt_br));
			memcpy(params->isp_tunning_settings.isp_tdnf_lay0_nrd_rt_br, isp_tuning_tdnf->lay0_nrd_rt_br, sizeof(isp_tuning_tdnf->lay0_nrd_rt_br));
			memcpy(params->isp_tunning_settings.isp_tdnf_lay1_nrd_rt_br, isp_tuning_tdnf->lay1_nrd_rt_br, sizeof(isp_tuning_tdnf->lay1_nrd_rt_br));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_tdnf_table_cfg);
			ret += sizeof(struct isp_tuning_tdnf_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_SHARP) /* isp_tuning_sharp */
		{
			struct isp_tuning_sharp_table_cfg *isp_tuning_sharp = (struct isp_tuning_sharp_table_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++) {
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].sharp_edge_lum, &isp_tuning_sharp->edge_lum[i][0], ISP_REG_TBL_LENGTH * sizeof(HW_U16));
			}
			memcpy(params->isp_tunning_settings.isp_sharp_ss_value, isp_tuning_sharp->ss_val, sizeof(isp_tuning_sharp->ss_val));
			memcpy(params->isp_tunning_settings.isp_sharp_ls_value, isp_tuning_sharp->ls_val, sizeof(isp_tuning_sharp->ss_val));
			memcpy(params->isp_tunning_settings.isp_sharp_hsv, isp_tuning_sharp->hsv, sizeof(isp_tuning_sharp->hsv));
			/* offset */
			data_ptr += sizeof(struct isp_tuning_sharp_table_cfg);
			ret += sizeof(struct isp_tuning_sharp_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CEM) /* isp_tuning_cem */
		{
			memcpy(params->isp_tunning_settings.isp_cem_table, data_ptr, sizeof(struct isp_tuning_cem_table_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_cem_table_cfg);
			ret += sizeof(struct isp_tuning_cem_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_CEM_1) /* isp_tuning_cem_1 */
		{
			memcpy(params->isp_tunning_settings.isp_cem_table1, data_ptr, sizeof(struct isp_tuning_cem_table_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_cem_table_cfg);
			ret += sizeof(struct isp_tuning_cem_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_PLTM_TBL) /* isp_tuning_pltm_table */
		{
			struct isp_tuning_pltm_table_cfg *isp_tuning_pltm = (struct isp_tuning_pltm_table_cfg *)data_ptr;
			memcpy(params->isp_tunning_settings.isp_pltm_stat_gd_cv, isp_tuning_pltm->stat_gd_cv,
							sizeof(isp_tuning_pltm->stat_gd_cv));
			memcpy(params->isp_tunning_settings.isp_pltm_df_cv, isp_tuning_pltm->df_cv,
							sizeof(isp_tuning_pltm->df_cv));
			memcpy(params->isp_tunning_settings.isp_pltm_lum_map_cv, isp_tuning_pltm->lum_map_cv,
							sizeof(isp_tuning_pltm->lum_map_cv));
			memcpy(params->isp_tunning_settings.isp_pltm_gtm_tbl, isp_tuning_pltm->gtm_tbl,
							sizeof(isp_tuning_pltm->gtm_tbl));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_pltm_table_cfg);
			ret += sizeof(struct isp_tuning_pltm_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_WDR) /* isp_tuning_wdr_tbl */
		{
			struct isp_tuning_wdr_table_cfg *isp_tuning_wdr = (struct isp_tuning_wdr_table_cfg *)data_ptr;
			memcpy(params->isp_tunning_settings.isp_wdr_de_purpl_hsv_tbl, isp_tuning_wdr->wdr_de_purpl_hsv_tbl,
							sizeof(isp_tuning_wdr->wdr_de_purpl_hsv_tbl));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_wdr_table_cfg);
			ret += sizeof(struct isp_tuning_wdr_table_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_LCA_TBL)
		{
			memcpy(params->isp_tunning_settings.lca_pf_satu_lut, data_ptr, sizeof(struct isp_tuning_lca_pf_satu_lut));
			// lca_pf_satu_lut offset
			data_ptr += sizeof(struct isp_tuning_lca_pf_satu_lut);
			ret += sizeof(struct isp_tuning_lca_pf_satu_lut);
			memcpy(params->isp_tunning_settings.lca_gf_satu_lut, data_ptr, sizeof(struct isp_tuning_lca_gf_satu_lut));
		    // lca_gf_satu_lut offset
			data_ptr += sizeof(struct isp_tuning_lca_gf_satu_lut);
			ret += sizeof(struct isp_tuning_lca_gf_satu_lut);
		}
		if (cfg_ids & HW_ISP_CFG_TUNING_MSC) /* isp_tuning_msc */
		{
			struct isp_tuning_msc_table_cfg *isp_tuning_msc = (struct isp_tuning_msc_table_cfg *)data_ptr;
			params->isp_tunning_settings.mff_mod = isp_tuning_msc->mff_mod;
			params->isp_tunning_settings.msc_mode = isp_tuning_msc->msc_mode;
			memcpy(params->isp_tunning_settings.msc_blw_lut, isp_tuning_msc->msc_blw_lut,
				sizeof(isp_tuning_msc->msc_blw_lut));
			memcpy(params->isp_tunning_settings.msc_blh_lut, isp_tuning_msc->msc_blh_lut,
				sizeof(isp_tuning_msc->msc_blh_lut));
			memcpy(&(params->isp_tunning_settings.msc_tbl[0][0]), &(isp_tuning_msc->value[0][0]),
				sizeof(isp_tuning_msc->value));
			memcpy(params->isp_tunning_settings.msc_trig_cfg, isp_tuning_msc->color_temp_triggers,
				sizeof(isp_tuning_msc->color_temp_triggers));

			/* offset */
			data_ptr += sizeof(struct isp_tuning_msc_table_cfg);
			ret += sizeof(struct isp_tuning_msc_table_cfg);
		}
#ifdef USE_ENCPP
		if (cfg_ids & HW_ISP_CFG_TUNING_ENCPP_SHARP) /* isp_tuning_sharp */
		{
			struct isp_tuning_sharp_table_cfg *isp_tuning_encpp_sharp = (struct isp_tuning_sharp_table_cfg *)data_ptr;
			int i = 0;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++) {
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_edge_lum, &isp_tuning_encpp_sharp->edge_lum[i][0], ISP_REG_TBL_LENGTH * sizeof(HW_U16));
			}
			memcpy(params->isp_tunning_settings.encpp_sharp_ss_value, isp_tuning_encpp_sharp->ss_val, sizeof(isp_tuning_encpp_sharp->ss_val));
			memcpy(params->isp_tunning_settings.encpp_sharp_ls_value, isp_tuning_encpp_sharp->ls_val, sizeof(isp_tuning_encpp_sharp->ss_val));
			memcpy(params->isp_tunning_settings.encpp_sharp_hsv, isp_tuning_encpp_sharp->hsv, sizeof(isp_tuning_encpp_sharp->hsv));
			/* offset */
			data_ptr += sizeof(struct isp_tuning_sharp_table_cfg);
			ret += sizeof(struct isp_tuning_sharp_table_cfg);
		}
#endif
		break;
	case HW_ISP_CFG_DYNAMIC: /* isp_dynamic_param */
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_LUM_POINT) /* isp_dynamic_lum_mapping_point */
		{
			memcpy(params->isp_iso_settings.isp_lum_mapping_point, data_ptr, sizeof(struct isp_dynamic_single_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_GAIN_POINT) /* isp_dynamic_gain_mapping_point */
		{
			memcpy(params->isp_iso_settings.isp_gain_mapping_point, data_ptr, sizeof(struct isp_dynamic_single_cfg));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_single_cfg);
			ret += sizeof(struct isp_dynamic_single_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_SHARP) /* isp_dynamic_sharp */
		{
			struct isp_dynamic_sharp_cfg *isp_dynamic_sharp = (struct isp_dynamic_sharp_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.sharp_triger = (enum isp_triger_type)isp_dynamic_sharp->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg[0], isp_dynamic_sharp->tuning_ss_cfg[i].value,
				        sizeof(isp_dynamic_sharp->tuning_ss_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg[ISP_SHARP_LS_NS_LW], isp_dynamic_sharp->tuning_ls_cfg[i].value,
				        sizeof(isp_dynamic_sharp->tuning_ls_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg[ISP_SHARP_HFR_SMTH_RATIO], isp_dynamic_sharp->tuning_hfr_cfg[i].value,
				        sizeof(isp_dynamic_sharp->tuning_hfr_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].sharp_cfg[ISP_SHARP_DIR_SMTH0], isp_dynamic_sharp->tuning_comm_cfg[i].value,
				        sizeof(isp_dynamic_sharp->tuning_comm_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_sharp_cfg);
			ret += sizeof(struct isp_dynamic_sharp_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_DENOISE) /* isp_dynamic_denoise */
		{
			struct isp_dynamic_denoise_cfg *isp_dynamic_denoise = (struct isp_dynamic_denoise_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.denoise_triger = (enum isp_triger_type)isp_dynamic_denoise->trigger;
			params->isp_iso_settings.triger.color_denoise_triger = (enum isp_triger_type)isp_dynamic_denoise->color_trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].denoise_cfg[0], isp_dynamic_denoise->tuning_dnr_cfg[i].value,
					sizeof(isp_dynamic_denoise->tuning_dnr_cfg[i].value));
				params->isp_iso_settings.isp_dynamic_cfg[i].color_denoise = isp_dynamic_denoise->tuning_dnr_cfg[i].color_denoise;
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].denoise_cfg[ISP_DENOISE_HF_SMTH_RATIO], isp_dynamic_denoise->tuning_dtc_cfg[i].value,
					sizeof(isp_dynamic_denoise->tuning_dtc_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].denoise_cfg[ISP_DENOISE_LYR0_DNR_LM_AMP], isp_dynamic_denoise->tuning_wdr_cfg[i].value,
					sizeof(isp_dynamic_denoise->tuning_wdr_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_denoise_cfg);
			ret += sizeof(struct isp_dynamic_denoise_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_BLACK_LV) /* isp_dynamic_black_level  */
		{	struct isp_dynamic_black_level_cfg *isp_dynamic_black_level = (struct isp_dynamic_black_level_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.black_level_triger = (enum isp_triger_type)isp_dynamic_black_level->trigger ;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].black_level, isp_dynamic_black_level->tuning_cfg[i].value,
					sizeof(isp_dynamic_black_level->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_black_level_cfg);
			ret += sizeof(struct isp_dynamic_black_level_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_DPC) /* isp_dynamic_dpc  */
		{	struct isp_dynamic_dpc_cfg *isp_dynamic_dpc = (struct isp_dynamic_dpc_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.dpc_triger = (enum isp_triger_type)isp_dynamic_dpc->trigger ;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].dpc_cfg, isp_dynamic_dpc->tuning_cfg[i].value,
					sizeof(isp_dynamic_dpc->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_dpc_cfg);
			ret += sizeof(struct isp_dynamic_dpc_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_PLTM) /* isp_dynamic_pltm  */
		{	struct isp_dynamic_pltm_cfg *isp_dynamic_pltm = (struct isp_dynamic_pltm_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.pltm_dynamic_triger = (enum isp_triger_type)isp_dynamic_pltm->trigger ;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].pltm_dynamic_cfg, isp_dynamic_pltm->tuning_cfg[i].value,
					sizeof(isp_dynamic_pltm->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_pltm_cfg);
			ret += sizeof(struct isp_dynamic_pltm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_DEFOG) /* isp_dynamic_defog  */
		{	struct isp_dynamic_defog_cfg *isp_dynamic_defog = (struct isp_dynamic_defog_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.defog_value_triger = (enum isp_triger_type)isp_dynamic_defog->trigger ;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				params->isp_iso_settings.isp_dynamic_cfg[i].defog_value = isp_dynamic_defog->tuning_cfg[i].value;

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_defog_cfg);
			ret += sizeof(struct isp_dynamic_defog_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_HISTOGRAM) /* isp_dynamic_histogram */
		{
			struct isp_dynamic_histogram_cfg *isp_dynamic_histogram = (struct isp_dynamic_histogram_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.brightness_triger = (enum isp_triger_type)isp_dynamic_histogram->brightness_trigger;
			params->isp_iso_settings.triger.gcontrast_triger = (enum isp_triger_type)isp_dynamic_histogram->contrast_trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				params->isp_iso_settings.isp_dynamic_cfg[i].brightness = isp_dynamic_histogram->tuning_cfg[i].brightness;
				params->isp_iso_settings.isp_dynamic_cfg[i].contrast = isp_dynamic_histogram->tuning_cfg[i].contrast;
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_histogram_cfg);
			ret += sizeof(struct isp_dynamic_histogram_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_CEM) /* isp_dynamic_cem  */
		{	struct isp_dynamic_cem_cfg *isp_dynamic_cem = (struct isp_dynamic_cem_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.cem_triger = (enum isp_triger_type)isp_dynamic_cem->trigger ;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].cem_cfg, isp_dynamic_cem->tuning_cfg[i].value,
					sizeof(isp_dynamic_cem->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_cem_cfg);
			ret += sizeof(struct isp_dynamic_cem_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_TDF) /* isp_dynamic_tdf */
		{
			struct isp_dynamic_tdf_cfg *isp_dynamic_tdf = (struct isp_dynamic_tdf_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.tdf_triger = (enum isp_triger_type)isp_dynamic_tdf->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg[0], isp_dynamic_tdf->tuning_dnr_cfg[i].value,
					sizeof(isp_dynamic_tdf->tuning_dnr_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg[ISP_TDF_DIFF_INTRA_SENS], isp_dynamic_tdf->tuning_mtd_cfg[i].value,
					sizeof(isp_dynamic_tdf->tuning_mtd_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg[ISP_TDF_DTC_HF_COR], isp_dynamic_tdf->tuning_dtc_cfg[i].value,
					sizeof(isp_dynamic_tdf->tuning_dtc_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].tdf_cfg[ISP_TDF_D2D0_CNR_STREN], isp_dynamic_tdf->tuning_srd_cfg[i].value,
					sizeof(isp_dynamic_tdf->tuning_srd_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_tdf_cfg);
			ret += sizeof(struct isp_dynamic_tdf_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_AE) /* isp_dynamic_ae */
		{
			struct isp_dynamic_ae_cfg *isp_dynamic_ae = (struct isp_dynamic_ae_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.ae_cfg_triger = (enum isp_triger_type)isp_dynamic_ae->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].ae_cfg, isp_dynamic_ae->tuning_cfg[i].value,
					sizeof(isp_dynamic_ae->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_ae_cfg);
			ret += sizeof(struct isp_dynamic_ae_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_GTM) /* isp_dynamic_gtm */
		{
			struct isp_dynamic_gtm_cfg *isp_dynamic_gtm = (struct isp_dynamic_gtm_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.gtm_cfg_triger = (enum isp_triger_type)isp_dynamic_gtm->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].gtm_cfg, isp_dynamic_gtm->tuning_cfg[i].value,
					sizeof(isp_dynamic_gtm->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_gtm_cfg);
			ret += sizeof(struct isp_dynamic_gtm_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_LCA) /* isp_dynamic_lca */
		{
			struct isp_dynamic_lca_cfg *isp_dynamic_lca = (struct isp_dynamic_lca_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.lca_cfg_triger = (enum isp_triger_type)isp_dynamic_lca->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].lca_cfg, isp_dynamic_lca->tuning_cfg[i].value,
					sizeof(isp_dynamic_lca->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_lca_cfg);
			ret += sizeof(struct isp_dynamic_lca_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_CFA) /* isp_dynamic_cfa */
		{
			struct isp_dynamic_cfa_cfg *isp_dynamic_cfa = (struct isp_dynamic_cfa_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.cfa_triger = (enum isp_triger_type)isp_dynamic_cfa->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
				memcpy(params->isp_iso_settings.isp_dynamic_cfg[i].cfa_cfg, isp_dynamic_cfa->tuning_cfg[i].value,
					sizeof(isp_dynamic_cfa->tuning_cfg[i].value));

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_cfa_cfg);
			ret += sizeof(struct isp_dynamic_cfa_cfg);
		}
#ifdef USE_ENCPP
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_ENCPP_SHARP) /* isp_dynamic_encpp_sharp */
		{
			struct isp_dynamic_encpp_sharp_cfg *isp_dynamic_encpp_sharp = (struct isp_dynamic_encpp_sharp_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.encpp_sharp_triger = (enum isp_triger_type)isp_dynamic_encpp_sharp->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_cfg[0], isp_dynamic_encpp_sharp->tuning_ss_cfg[i].value,
				        sizeof(isp_dynamic_encpp_sharp->tuning_ss_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_cfg[ENCPP_SHARP_LS_NS_LW], isp_dynamic_encpp_sharp->tuning_ls_cfg[i].value,
				        sizeof(isp_dynamic_encpp_sharp->tuning_ls_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_cfg[ENCPP_SHARP_HFR_SMTH_RATIO], isp_dynamic_encpp_sharp->tuning_hfr_cfg[i].value,
				        sizeof(isp_dynamic_encpp_sharp->tuning_hfr_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].encpp_sharp_cfg[ENCPP_SHARP_DIR_SMTH0], isp_dynamic_encpp_sharp->tuning_comm_cfg[i].value,
				        sizeof(isp_dynamic_encpp_sharp->tuning_comm_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_encpp_sharp_cfg);
			ret += sizeof(struct isp_dynamic_encpp_sharp_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_ENCODER_DENOISE) /* isp_dynamic_encoder_denoise */
		{
			struct isp_dynamic_encoder_denoise_cfg *isp_dynamic_encoder_denoise = (struct isp_dynamic_encoder_denoise_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.encoder_denoise_triger = (enum isp_triger_type)isp_dynamic_encoder_denoise->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].encoder_denoise_cfg[0], isp_dynamic_encoder_denoise->tuning_3dnr_cfg[i].value,
						sizeof(isp_dynamic_encoder_denoise->tuning_3dnr_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].encoder_denoise_cfg[ENCODER_DENOISE_2D_FILT_STREN_UV], isp_dynamic_encoder_denoise->tuning_2dnr_cfg[i].value,
						sizeof(isp_dynamic_encoder_denoise->tuning_2dnr_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_encoder_denoise_cfg);
			ret += sizeof(struct isp_dynamic_encoder_denoise_cfg);
		}
#endif
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_WDR) /* isp_dynamic_wdr */
		{
			struct isp_dynamic_wdr_cfg *isp_dynamic_wdr = (struct isp_dynamic_wdr_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.wdr_cfg_triger = (enum isp_triger_type)isp_dynamic_wdr->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].wdr_cfg[0], isp_dynamic_wdr->tuning_lm_cfg[i].value,
						sizeof(isp_dynamic_wdr->tuning_lm_cfg[i].value));
				memcpy(&params->isp_iso_settings.isp_dynamic_cfg[i].wdr_cfg[WDR_CMP_MS_LTH], isp_dynamic_wdr->tuning_ms_cfg[i].value,
						sizeof(isp_dynamic_wdr->tuning_ms_cfg[i].value));
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_wdr_cfg);
			ret += sizeof(struct isp_dynamic_wdr_cfg);
		}
		if (cfg_ids & HW_ISP_CFG_DYNAMIC_SHADING) /* isp_dynamic_shading */
		{
			struct isp_dynamic_shading_cfg *isp_dynamic_shading = (struct isp_dynamic_shading_cfg *)data_ptr;
			int i = 0;
			params->isp_iso_settings.triger.shading_triger = (enum isp_triger_type)isp_dynamic_shading->trigger;
			for (i = 0; i < ISP_DYNAMIC_GROUP_COUNT; i++)
			{
				params->isp_iso_settings.isp_dynamic_cfg[i].shading_comp = isp_dynamic_shading->tuning_cfg[i].shading_comp;
			}

			/* offset */
			data_ptr += sizeof(struct isp_dynamic_shading_cfg);
			ret += sizeof(struct isp_dynamic_shading_cfg);
		}
		break;
	default:
		ret = AW_ERR_VI_INVALID_PARA;
		break;
	}
	ISP_PRINT("%s: set done(%d)\n", __FUNCTION__, ret);
	return ret;
}

HW_S32 isp_tuning_set_cfg(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data)
{
	int ret = AW_ERR_VI_INVALID_PARA;
	struct isp_tuning *tuning = NULL;

	if (!isp || !cfg_data)
		return AW_ERR_VI_INVALID_PARA;

	/* call isp api */
	tuning = isp_dev_get_tuning(isp);
	if (!tuning)
		 return AW_ERR_VI_INVALID_NULL_PTR;

	if (!tuning->ctx)
		 return AW_ERR_VI_INVALID_NULL_PTR;

	/* fill cfg data */
	ret = isp_tuning_set_cfg_run(isp, group_id, cfg_ids, &tuning->params, cfg_data);

	return ret;
}

