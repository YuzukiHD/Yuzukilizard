
#include "../include/isp_comm.h"
#include "../include/isp_manage.h"
#include "../include/isp_debug.h"
#include "../isp_math/isp_math_util.h"

#define ISP_STAT_SAVE		0
#define ISP_LOG_SAVE		0

#define ISP_SAVE_STRING  70
#define ISP_PATH "/mnt/extsd"
extern unsigned int isp_lib_log_param;

/**************isp statistic save api**************/
int isp_stat_save_init(struct isp_lib_context *ctx)
{
#if ISP_STAT_SAVE
	char fdstr[50];

	sprintf(fdstr, "%s/isp%d_stat_log.bin", ISP_PATH, ctx->isp_id);
	ctx->stat_log_fd = fopen(fdstr, "wb");
	if (ctx->stat_log_fd == NULL) {
		ISP_WARN("open %s failed!!!\n", fdstr, ctx->isp_id);
		return -1;
	} else {
		ISP_PRINT("open %s success!!!\n", fdstr, ctx->isp_id);
	}
#endif
	return 0;
}

void isp_stat_save_exit(struct isp_lib_context *ctx)
{
#if ISP_STAT_SAVE
	fclose(ctx->stat_log_fd);
	ctx->stat_log_fd = NULL;
#endif
}

void isp_stat_save_run(struct isp_lib_context *ctx)
{
#if ISP_STAT_SAVE
	if (ctx->stat_log_fd && (ctx->alg_frame_cnt % 5 == 4)) {
		fwrite(ctx->stats_ctx.stats.ae_stats.hist, ISP_HIST_NUM*4, 1, ctx->stat_log_fd);
		fwrite(ctx->stats_ctx.stats.awb_stats.awb_avg_r, ISP_AWB_ROW*ISP_AWB_COL*4, 1, ctx->stat_log_fd);
		fwrite(ctx->stats_ctx.stats.awb_stats.awb_avg_g, ISP_AWB_ROW*ISP_AWB_COL*4, 1, ctx->stat_log_fd);
		fwrite(ctx->stats_ctx.stats.awb_stats.awb_avg_b, ISP_AWB_ROW*ISP_AWB_COL*4, 1, ctx->stat_log_fd);
	}
#endif
}

bool isp_check_if_sdcard_mounted(const char *mount_point)
{
	const char *path = "/proc/mounts";
	FILE *fp = NULL;
	char line[255] = {0};
	if (!(fp = fopen(path, "r"))) {
		ISP_WARN("fopen failed, path=%s",path);
		return 0;
	}

	while(fgets(line, sizeof(line), fp)) {
		if (line[0] == '/' && (strstr(line, mount_point) != NULL)) {
			fclose(fp);
			fp = NULL;

			if (!access(mount_point, F_OK))
				return true;
			else
				return false;
		} else {
			memset(line,'\0',sizeof(line));
			continue;
		}
	}

	if (fp) {
		fclose(fp);
	}

	return false;
}

/**************isp lib context save api**************/
int isp_ctx_save_init(struct isp_lib_context *ctx)
{
	FILE *file_fd = NULL;
	char fdstr[ISP_SAVE_STRING];
	int data_len = sizeof(isp_af_entity_context_t) + sizeof(isp_afs_entity_context_t) +
		sizeof(isp_md_entity_context_t) + sizeof(isp_awb_entity_context_t) +
		sizeof(isp_ae_entity_context_t) + sizeof(isp_gtm_entity_context_t) +
		sizeof(isp_pltm_entity_context_t) + sizeof(isp_iso_entity_context_t) +
		sizeof(isp_rolloff_entity_context_t) + sizeof(struct isp_module_config);
	int read_len = 0;

	if (!isp_check_if_sdcard_mounted(ISP_PATH)) {
		ISP_WARN("sdcard is not mounted!!!\n");
		return -1;
	}

	sprintf(fdstr, "%s/isp%d_%d_%d_%d_ctx_saved.bin", ISP_PATH, ctx->isp_id, ctx->sensor_info.sensor_width,
		ctx->sensor_info.sensor_height, (int)ctx->sensor_info.fps_fixed);
	file_fd = fopen(fdstr, "rb");

	if (file_fd == NULL) {
		ISP_WARN("open %s failed, err:%s.\n", fdstr, strerror(errno));
		return -1;
	} else {
		fread(&read_len, sizeof(int), 1, file_fd);
		if (data_len != read_len) {
			ISP_ERR("%s read size %d != isp_ctx size %d!\n", fdstr, read_len, data_len);
		} else {
			fread(&ctx->module_cfg, sizeof(struct isp_module_config), 1, file_fd);
			fread(&ctx->af_entity_ctx, sizeof(isp_af_entity_context_t), 1, file_fd);
			fread(&ctx->afs_entity_ctx, sizeof(isp_afs_entity_context_t), 1, file_fd);
			fread(&ctx->md_entity_ctx, sizeof(isp_md_entity_context_t), 1, file_fd);
			fread(&ctx->awb_entity_ctx, sizeof(isp_awb_entity_context_t), 1, file_fd);
			fread(&ctx->ae_entity_ctx, sizeof(isp_ae_entity_context_t), 1, file_fd);
			fread(&ctx->gtm_entity_ctx, sizeof(isp_gtm_entity_context_t), 1, file_fd);
			fread(&ctx->pltm_entity_ctx, sizeof(isp_pltm_entity_context_t), 1, file_fd);
			fread(&ctx->iso_entity_ctx, sizeof(isp_iso_entity_context_t), 1, file_fd);
			fread(&ctx->rolloff_entity_ctx, sizeof(isp_rolloff_entity_context_t), 1, file_fd);
			ISP_PRINT("get isp_ctx from %s success!!!\n", fdstr);
		}
	}
	fclose(file_fd);

	return 0;
}

int isp_ctx_save_exit(struct isp_lib_context *ctx)
{
	FILE *file_fd = NULL;
	char fdstr[ISP_SAVE_STRING];
	int data_len = sizeof(isp_af_entity_context_t) + sizeof(isp_afs_entity_context_t) +
		sizeof(isp_md_entity_context_t) + sizeof(isp_awb_entity_context_t) +
		sizeof(isp_ae_entity_context_t) + sizeof(isp_gtm_entity_context_t) +
		sizeof(isp_pltm_entity_context_t) + sizeof(isp_iso_entity_context_t) +
		sizeof(isp_rolloff_entity_context_t) + sizeof(struct isp_module_config);

	if (!isp_check_if_sdcard_mounted(ISP_PATH)) {
		ISP_WARN("sdcard is not mounted!!!\n");
		return -1;
	}

	sprintf(fdstr, "%s/isp%d_%d_%d_%d_ctx_saved.bin", ISP_PATH, ctx->isp_id, ctx->sensor_info.sensor_width,
		ctx->sensor_info.sensor_height, (int)ctx->sensor_info.fps_fixed);
	file_fd = fopen(fdstr, "wb");

	if (file_fd == NULL) {
		ISP_ERR("open %s failed, err:%s.\n", fdstr, strerror(errno));
		return -1;
	} else {
		fwrite(&data_len, sizeof(int), 1, file_fd);
		fwrite(&ctx->module_cfg, sizeof(struct isp_module_config), 1, file_fd);
		fwrite(&ctx->af_entity_ctx, sizeof(isp_af_entity_context_t), 1, file_fd);
		fwrite(&ctx->afs_entity_ctx, sizeof(isp_afs_entity_context_t), 1, file_fd);
		fwrite(&ctx->md_entity_ctx, sizeof(isp_md_entity_context_t), 1, file_fd);
		fwrite(&ctx->awb_entity_ctx, sizeof(isp_awb_entity_context_t), 1, file_fd);

		fwrite(&ctx->ae_entity_ctx, sizeof(isp_ae_entity_context_t), 1, file_fd);
		fwrite(&ctx->gtm_entity_ctx, sizeof(isp_gtm_entity_context_t), 1, file_fd);
		fwrite(&ctx->pltm_entity_ctx, sizeof(isp_pltm_entity_context_t), 1, file_fd);
		fwrite(&ctx->iso_entity_ctx, sizeof(isp_iso_entity_context_t), 1, file_fd);
		fwrite(&ctx->rolloff_entity_ctx, sizeof(isp_rolloff_entity_context_t), 1, file_fd);
		ISP_PRINT("save isp_ctx to %s success, data_len:%d!!!\n", fdstr, data_len);
	}
	fclose(file_fd);

	return 0;
}

/**************isp log save api**************/
void isp_log_save_init(struct isp_lib_context *ctx)
{
#if ISP_LOG_SAVE
	char fdstr[50];
	time_t tp;
	struct tm *t;

	time(&tp);
	t = localtime(&tp);

	sprintf(fdstr, "%s/isp%d_%d_%d_%d_%d_%d_%d.log", ISP_PATH, ctx->isp_id, 1900 + t->tm_year,
		1 + t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	ctx->isp_log_fd = fopen(fdstr, "w");
	if (ctx->isp_log_fd == NULL)
		ISP_WARN("open %s failed!!!\n", fdstr);
#endif
}

int isp_log_save_run(struct isp_lib_context *ctx)
{
#if ISP_LOG_SAVE
	ae_result_t *ae_result = &ctx->ae_entity_ctx.ae_result;
	pltm_result_t *pltm_result = &ctx->pltm_entity_ctx.pltm_result;
	struct isp_pltm_stats_s *pltm_stats = &ctx->stats_ctx.stats.pltm_stats;

	char log_buf[512] = {'\0'};
	size_t count = 0;
	time_t tp;
	struct tm *t;

	if (ctx->isp_log_fd == NULL)
		return -1;

	time(&tp);
	t = localtime(&tp);
	count += snprintf(log_buf + count, 512 - count, "%d/%d/%d %d:%d:%d\n",
		1900 + t->tm_year, 1 + t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

	count += snprintf(log_buf + count, 512 - count, "ISP frame id %d\n", ctx->alg_frame_cnt);

	count += snprintf(log_buf + count, 512 - count, "hist_low %d, hist_mid %d, hist_hi %d, ae_wdr_ratio sensor:%d, isp:%d, tmp:%d, last:%d.\n",
		ae_result->hist_low, ae_result->hist_mid, ae_result->hist_hi,
		ae_result->ae_wdr_ratio.sensor,
		ae_result->ae_wdr_ratio.isp_hardware,
		ae_result->ae_wdr_ratio.tmp,
		ae_result->ae_wdr_ratio.last);

	count += snprintf(log_buf + count, 512 - count,  "AUTO_STREN %d, AE_COMP %d MIN_TH %d.\n",
		ctx->ae_settings.pltm_dynamic_cfg[ISP_PLTM_DYNAMIC_AUTO_STREN],
		pltm_result->pltm_ae_comp, pltm_result->pltm_min_th);

	count += snprintf(log_buf + count, 512 - count, "cal_stren %d old_stren %d next_stren %d.\n",
		pltm_result->pltm_cal_stren, pltm_result->pltm_old_stren, pltm_result->pltm_next_stren);

	count += snprintf(log_buf + count, 512 - count, "before_pltm min %d max %d avg %d, after_pltm min %d max %d avg %d.\n\n",
		pltm_stats->min_before_pltm, pltm_stats->max_before_pltm, pltm_stats->avg_before_pltm,
		pltm_stats->min_after_pltm, pltm_stats->max_after_pltm, pltm_stats->avg_after_pltm);

	count += snprintf(log_buf + count, 512 - count, "ae_target: %d, pic_lum: %d, weight_lum: %d, AE_TOLERANCE: %d.\n",
		ae_result->ae_target, ae_result->ae_avg_lum, ae_result->ae_weight_lum, ctx->ae_settings.exposure_cfg[AE_TOLERANCE]);

	count += snprintf(log_buf + count, 512 - count, "EXP_LINES: %d, EXP_TIME: %d, AGAIN: %d, DGAIN: %d, tbl_idx: %d, expect_idx %d, max_idx: %d, LV: %d.\n\n",
		ae_result->sensor_set.ev_set_curr.ev_sensor_exp_line,
		ae_result->sensor_set.ev_set_curr.ev_exposure_time,
		ae_result->sensor_set.ev_set_curr.ev_analog_gain,
		ae_result->sensor_set.ev_set_curr.ev_digital_gain,
		ae_result->sensor_set.ev_set.ev_idx,
		ae_result->sensor_set.ev_idx_expect,
		ae_result->sensor_set.ev_idx_max,
		ae_result->sensor_set.ev_set.ev_lv);

	fwrite(log_buf, count, 1, ctx->isp_log_fd);
#endif
	return 0;
}

void isp_log_save_exit(struct isp_lib_context *ctx)
{
#if ISP_LOG_SAVE
	if (ctx->isp_log_fd != NULL)
		fclose(ctx->isp_log_fd);
#endif
}

#ifdef ANDROID_PLATFORM
int ISP_LIB_LOG(int flag,const char *fmt,...)
{
	if(isp_lib_log_param & flag)
	{
		va_list arg_list;
		char buf[2048];
		memset(buf, 0, 2048);
		va_start(arg_list,fmt);
		vsnprintf(buf, 2048, fmt, arg_list);
		va_end(arg_list);
		ALOGE("[ISP_DEBUG]: %s",buf);
	}
	return 0;
}
#endif

/**************isp cmd for effect set api**************/
void isp_s_brightness(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->tune.brightness_level != value)
	{
		isp_gen->tune.brightness_level = value;
		isp_gen->isp_3a_change_flags |= ISP_SET_BRIGHTNESS;
	}
}

void isp_s_contrast(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->tune.contrast_level != value)
	{
		isp_gen->tune.contrast_level = value;
		isp_gen->isp_3a_change_flags |= ISP_SET_CONTRAST;
	}
}

void isp_s_saturation(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->tune.saturation_level != value)
	{
		isp_gen->tune.saturation_level = value;
		isp_gen->isp_3a_change_flags |= ISP_SET_SATURATION;
	}
}

void isp_s_hue(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->tune.hue_level != value)
	{
		isp_gen->tune.hue_level = value;
		isp_gen->isp_3a_change_flags |= ISP_SET_HUE;
	}
}

void isp_s_auto_white_balance(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->awb_settings.wb_mode != value)
	{
		isp_gen->awb_settings.wb_mode = value;
		isp_gen->isp_3a_change_flags |= ISP_SET_AWB_MODE;
	}
}

void isp_s_exposure(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->sensor_info.pclk)
		isp_gen->ae_settings.exp_absolute = (((double)value / 16) * 1000000 * isp_gen->sensor_info.hts / isp_gen->sensor_info.pclk);
}

void isp_s_auto_gain(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.iso_mode != value)
		isp_gen->ae_settings.iso_mode = value;
}

void isp_s_gain(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.sensor_gain != value)
		isp_gen->ae_settings.sensor_gain = value;
}

void isp_s_power_line_frequency(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.flicker_mode != value)
	{
		isp_gen->ae_settings.flicker_mode = value;
		isp_gen->isp_3a_change_flags |= ISP_SET_FLICKER_MODE;
	}
}

void isp_s_white_balance_temperature(struct isp_lib_context *isp_gen, int value)
{
	isp_gen->awb_settings.wb_mode = WB_MANUAL;
	if (isp_gen->awb_settings.wb_temperature != value)
		isp_gen->awb_settings.wb_temperature = value;
}

void isp_s_sharpness(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->tune.sharpness_level != value)
	{
		isp_gen->tune.sharpness_level = value;
		isp_gen->isp_3a_change_flags |= ISP_SET_SHARPNESS;
	}
}

void isp_s_auto_brightness(struct isp_lib_context *isp_gen, int value)
{
	if(1 == value)
		isp_gen->tune.brightness_level = 0;
}

void isp_s_band_stop_filter(struct isp_lib_context *isp_gen, int value)
{
	if (value  == 1)
	{
		isp_gen->ae_settings.flicker_mode = FREQUENCY_AUTO;
		isp_gen->isp_3a_change_flags |= ISP_SET_FLICKER_MODE;
	}
	else
	{
		isp_gen->ae_settings.flicker_mode = FREQUENCY_DISABLED;
		isp_gen->isp_3a_change_flags |= ISP_SET_FLICKER_MODE;
	}
}

void isp_s_illuminators_1(struct isp_lib_context *isp_gen, int value)
{
	if (0 == value)
		isp_gen->ae_settings.flash_mode = FLASH_MODE_OFF;
	else
		isp_gen->ae_settings.flash_mode = FLASH_MODE_TORCH;
}

void isp_s_illuminators_2(struct isp_lib_context *isp_gen, int value)
{
	;
}

void isp_s_af_metering_mode(struct isp_lib_context *isp_gen, struct v4l2_win_setting  *win)
{
  ISP_PRINT("isp_s_af_metering_mode");

  isp_gen->af_settings.af_metering_mode = win->metering_mode;

  if ((isp_gen->af_settings.af_metering_mode == AUTO_FOCUS_METERING_SPOT) && (win != NULL))
  {
    //isp_gen->af_settings.af_mode = AUTO_FOCUS_TOUCH;
    isp_gen->af_settings.af_coor.x1 = win->coor.x1;
    isp_gen->af_settings.af_coor.y1 = win->coor.y1;
    isp_gen->af_settings.af_coor.x2 = win->coor.x2;
    isp_gen->af_settings.af_coor.y2 = win->coor.y2;
  }
  else
  {
    //isp_gen->af_settings.af_mode = AUTO_FOCUS_CONTINUEOUS;
  }
  isp_gen->isp_3a_change_flags |= ISP_SET_AF_METERING_MODE;
}

/*void isp_s_af_metering_mode(struct isp_lib_context *isp_gen, int value, struct isp_h3a_coor_win *coor)
{

	isp_gen->af_settings.af_metering_mode = value;
	if ((value == AUTO_FOCUS_METERING_SPOT) && (coor != NULL))
	{
		//isp_gen->af_settings.af_mode = AUTO_FOCUS_TOUCH;
		isp_gen->af_settings.af_coor.x1 = coor->x1;
		isp_gen->af_settings.af_coor.y1 = coor->y1;
		isp_gen->af_settings.af_coor.x2 = coor->x2;
		isp_gen->af_settings.af_coor.y2 = coor->y2;
	}
	else
	{
		//isp_gen->af_settings.af_mode = AUTO_FOCUS_CONTINUEOUS;
	}
	isp_gen->isp_3a_change_flags |= ISP_SET_AF_METERING_MODE;
}*/

void isp_s_ae_metering_mode(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.exp_metering_mode != value) {
		isp_gen->ae_settings.exp_metering_mode = value;
		isp_gen->ae_settings.exposure_lock = false;
		isp_gen->isp_3a_change_flags |= ISP_SET_AE_METERING_MODE;
	}
}

void isp_s_ae_roi(struct isp_lib_context *isp_gen, int value, struct isp_h3a_coor_win *coor)
{
	if ((value == AE_METERING_MODE_SPOT) && (coor != NULL) &&
	    memcmp(&isp_gen->ae_settings.ae_coor, coor, sizeof(struct isp_h3a_coor_win))) {
		isp_gen->ae_settings.exp_metering_mode = value;
		isp_gen->ae_settings.ae_coor = *coor;
		isp_gen->ae_settings.exposure_lock = false;
		isp_gen->isp_3a_change_flags |= ISP_SET_AE_METERING_MODE;
	}
}

void isp_s_light_mode(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.light_mode != value)
		isp_gen->ae_settings.light_mode = value;
}

void isp_s_exposure_auto(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.exp_mode != value)
		isp_gen->ae_settings.exp_mode = value;
}

void isp_s_exposure_absolute(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.exp_absolute != value)
		isp_gen->ae_settings.exp_absolute = value;
}

void isp_s_aperture(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.iris_fno != value)
		isp_gen->ae_settings.iris_fno = value;
}

void isp_s_focus_absolute(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->af_settings.focus_absolute != value)
		isp_gen->af_settings.focus_absolute = value;
}

void isp_s_focus_relative(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->af_settings.focus_relative != value)
		isp_gen->af_settings.focus_relative = value;
}

void isp_s_focus_auto(struct isp_lib_context *isp_gen, int value)
{
	if (value != AUTO_FOCUS_MANUAL)
	{
		isp_gen->af_settings.af_mode = AUTO_FOCUS_CONTINUEOUS;
		isp_gen->af_settings.focus_lock = false;
		isp_gen->isp_3a_change_flags |= ISP_SET_AF_METERING_MODE;
		if (!isp_gen->af_entity_ctx.af_param) {
			ISP_ERR("af is not init, please init first!\n");
			return;
		}
		isp_gen->af_entity_ctx.af_param->auto_focus_trigger = 1;
		isp_af_set_params_helper(&isp_gen->af_entity_ctx, ISP_AF_TRIGGER);
	}
	else
	{
		isp_gen->af_settings.af_mode = AUTO_FOCUS_MANUAL;
	}
}

void isp_s_auto_exposure_bias(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.exp_compensation != value)
		isp_gen->ae_settings.exp_compensation = value;
}

void isp_s_auto_n_preset_white_balance(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->awb_settings.wb_mode != value)
		isp_gen->awb_settings.wb_mode = value;
}

void isp_s_iso_sensitivity(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.iso_sensitivity != value)
		isp_gen->ae_settings.iso_sensitivity = value;
}

void isp_s_iso_sensitivity_auto(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.iso_mode != value)
		isp_gen->ae_settings.iso_mode = value;
}
void isp_s_scene_mode(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.scene_mode != value)
	{
		isp_gen->ae_settings.scene_mode= value;
		isp_gen->isp_3a_change_flags |= ISP_SET_SCENE_MODE;
	}
}
void isp_s_auto_focus_start(struct isp_lib_context *isp_gen, int value)
{
	isp_gen->af_settings.focus_lock = false;
	isp_gen->af_settings.af_mode = AUTO_FOCUS_TOUCH;

	if (!isp_gen->af_entity_ctx.af_param) {
		ISP_ERR("af is not init, please init first!\n");
		return;
	}
	isp_gen->af_entity_ctx.af_param->auto_focus_trigger = 1;
	isp_af_set_params_helper(&isp_gen->af_entity_ctx, ISP_AF_TRIGGER);
}

void isp_s_auto_focus_stop(struct isp_lib_context *isp_gen, int value)
{
	//isp_gen->af_settings.focus_lock = false;
	isp_gen->af_settings.focus_lock = true;
	isp_gen->af_settings.af_mode  = AUTO_FOCUS_CONTINUEOUS;
}

void isp_s_auto_focus_status(struct isp_lib_context *isp_gen, int value)
{

}

void isp_s_auto_focus_range(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->af_settings.af_range != value)
		isp_gen->af_settings.af_range = value;
}

void isp_s_take_picture(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.take_picture_flag != value) {
		isp_gen->ae_settings.take_picture_flag = value;
	}

	// Actruly we turn off in isp_flash_update_status, but set flag there for avoid torch ligth turn on again.
	if(isp_gen->ae_settings.take_picture_flag == V4L2_TAKE_PICTURE_STOP) {
		isp_gen->ae_settings.flash_open = 0;
		isp_gen->image_params.isp_image_params.image_para.bits.flash_ok = 0;
	}

}

void isp_s_flash_mode(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->ae_settings.flash_mode != value)
		isp_gen->ae_settings.flash_mode= value;
}

void isp_s_flash_mode_v1(struct isp_lib_context *isp_gen, int value)
{
	if (!value) {
		if (isp_gen->ae_settings.flash_mode != value)
			isp_gen->ae_settings.flash_mode= value;
	} else {
		if (isp_gen->ae_settings.flash_mode != (value + 2))
			isp_gen->ae_settings.flash_mode= value + 2;
	}
}

void isp_s_r_gain(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->awb_settings.wb_gain_manual.r_gain != value)
		isp_gen->awb_settings.wb_gain_manual.r_gain = value;
}
void isp_s_gr_gain(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->awb_settings.wb_gain_manual.gr_gain != value)
		isp_gen->awb_settings.wb_gain_manual.gr_gain = value;
}

void isp_s_gb_gain(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->awb_settings.wb_gain_manual.gb_gain != value)
		isp_gen->awb_settings.wb_gain_manual.gb_gain = value;
}

void isp_s_b_gain(struct isp_lib_context *isp_gen, int value)
{
	if (isp_gen->awb_settings.wb_gain_manual.b_gain != value)
		isp_gen->awb_settings.wb_gain_manual.b_gain = value;
}

