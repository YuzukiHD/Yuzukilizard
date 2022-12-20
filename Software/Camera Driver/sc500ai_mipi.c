/*
 * A V4L2 driver for SC500AI Raw cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *    Liang WeiJie <liangweijie@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/io.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("myf");
MODULE_DESCRIPTION("A low-level driver for SC500AI sensors");
MODULE_LICENSE("GPL");

#define MCLK              (27*1000*1000)
#define V4L2_IDENT_SENSOR 0xce1f

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

#define HDR_RATIO 16

/*
 * The SC500AI i2c address
 */
#define I2C_ADDR 0x60

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc500ai_mipi"
#define SENSOR_NAME_2 "sc500ai_mipi_2"

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_2880x1620p30_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x0c},
	{0x3106, 0x01},

	{0x320e, 0x0c},	//vts=3300
	{0x320f, 0xe4},

	{0x320f, 0x98},
	{0x3250, 0x40},
	{0x3253, 0x0a},
	{0x3301, 0x0b},
	{0x3302, 0x20},
	{0x3303, 0x10},
	{0x3304, 0x70},
	{0x3306, 0x50},
	{0x3308, 0x18},
	{0x3309, 0x80},
	{0x330a, 0x00},
	{0x330b, 0xe8},
	{0x330d, 0x30},
	{0x330e, 0x30},
	{0x330f, 0x02},
	{0x3310, 0x02},
	{0x331c, 0x08},
	{0x331e, 0x61},
	{0x331f, 0x71},
	{0x3320, 0x11},
	{0x3333, 0x10},
	{0x334c, 0x10},
	{0x3356, 0x11},
	{0x3364, 0x17},
	{0x336d, 0x03},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x0a},
	{0x3394, 0x0a},
	{0x3395, 0x12},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x0a},
	{0x339a, 0x0a},
	{0x339b, 0x0a},
	{0x339c, 0x12},
	{0x33ac, 0x10},
	{0x33ae, 0x20},
	{0x33af, 0x21},
	{0x360f, 0x01},
	{0x3621, 0xe8},
	{0x3622, 0x06},
	{0x3630, 0x82},
	{0x3633, 0x33},
	{0x3634, 0x64},
	{0x3637, 0x50},
	{0x363a, 0x1f},
	{0x363c, 0x40},
	{0x3651, 0x7d},
	{0x3670, 0x0a},
	{0x3671, 0x06},
	{0x3672, 0x16},
	{0x3673, 0x17},
	{0x3674, 0x82},
	{0x3675, 0x62},
	{0x3676, 0x44},
	{0x367a, 0x48},
	{0x367b, 0x78},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x34},
	{0x3692, 0x54},
	{0x369c, 0x48},
	{0x369d, 0x78},
	{0x36ea, 0x3a},
	{0x36eb, 0x04},
	{0x36ec, 0x0a},
	{0x36ed, 0x24},
	{0x36fa, 0x3a},
	{0x36fb, 0x04},
	{0x36fc, 0x00},
	{0x36fd, 0x26},
	{0x3904, 0x04},
	{0x3908, 0x41},
	{0x391f, 0x10},
	{0x39c2, 0x30},
	{0x3e01, 0xd2},
	{0x3e02, 0x60},
	{0x4500, 0x88},
	{0x4509, 0x20},
	{0x4800, 0x04},
	{0x4837, 0x14},
	{0x36e9, 0x24},
	{0x36f9, 0x24},
	{0x0100, 0x01},
};




static struct regval_list sensor_2880x1620p30_wdr_regs[] = {


	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x23},
	{0x3106, 0x01},
	{0x320e, 0x0c},
	{0x320f, 0xe4},
	{0x3220, 0x53},
	{0x3250, 0x3f},
	{0x3253, 0x0a},
	{0x3301, 0x0b},
	{0x3302, 0x20},
	{0x3303, 0x10},
	{0x3304, 0x70},
	{0x3306, 0x50},
	{0x3308, 0x18},
	{0x3309, 0x80},
	{0x330a, 0x00},
	{0x330b, 0xe8},
	{0x330d, 0x30},
	{0x330e, 0x30},
	{0x330f, 0x02},
	{0x3310, 0x02},
	{0x331c, 0x08},
	{0x331e, 0x61},
	{0x331f, 0x71},
	{0x3320, 0x11},
	{0x3333, 0x10},
	{0x334c, 0x10},
	{0x3356, 0x11},
	{0x3364, 0x17},
	{0x336d, 0x03},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x0a},
	{0x3394, 0x0a},
	{0x3395, 0x12},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x0a},
	{0x339a, 0x0a},
	{0x339b, 0x0a},
	{0x339c, 0x12},
	{0x33ac, 0x10},
	{0x33ae, 0x20},
	{0x33af, 0x21},
	{0x360f, 0x01},
	{0x3621, 0xe8},
	{0x3622, 0x06},
	{0x3630, 0x82},
	{0x3633, 0x33},
	{0x3634, 0x64},
	{0x3637, 0x50},
	{0x363a, 0x1f},
	{0x363c, 0x40},
	{0x3651, 0x7d},
	{0x3670, 0x0a},
	{0x3671, 0x06},
	{0x3672, 0x16},
	{0x3673, 0x17},
	{0x3674, 0x82},
	{0x3675, 0x62},
	{0x3676, 0x44},
	{0x367a, 0x48},
	{0x367b, 0x78},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x34},
	{0x3692, 0x54},
	{0x369c, 0x48},
	{0x369d, 0x78},
	{0x36ea, 0x2d},
	{0x36eb, 0x04},
	{0x36ec, 0x0a},
	{0x36ed, 0x14},
	{0x36fa, 0x35},
	{0x36fb, 0x04},
	{0x36fc, 0x00},
	{0x36fd, 0x16},
	{0x3904, 0x04},
	{0x3908, 0x41},
	{0x391f, 0x10},
	{0x39c2, 0x30},
	{0x3e00, 0x01},
	{0x3e01, 0x82},
	{0x3e02, 0x00},
	{0x3e04, 0x18},
	{0x3e05, 0x20},
	{0x3e23, 0x00},
	{0x3e24, 0xc6},
	{0x4500, 0x88},
	{0x4509, 0x20},
	{0x4800, 0x24},
	{0x4837, 0x14},
	{0x4853, 0xfd},
	{0x36e9, 0x30},
	{0x36f9, 0x44},
	{0x0100, 0x01},

};




static struct regval_list sensor_fmt_raw[] = {

};


/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function ,retrun -EINVAL
 */

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int sc500ai_sensor_vts;
static int sc500ai_sensor_svr;
static int shutter_delay = 1;
static int shutter_delay_cnt;
static int fps_change_flag;

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh = 0;
	struct sensor_info *info = to_state(sd);

	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		if (exp_val < 16 * HDR_RATIO)
			exp_val = 16 * HDR_RATIO;

		exphigh = (unsigned char) (0x0f & (exp_val>>15));
		expmid = (unsigned char) (0xff & (exp_val>>7));
		explow = (unsigned char) (0xf0 & (exp_val<<1));

		sensor_write(sd, 0x3e02, explow);
		sensor_write(sd, 0x3e01, expmid);
		sensor_write(sd, 0x3e00, exphigh);

		sensor_dbg("sensor_set_long_exp = %d line Done!\n", exp_val);

		exp_val /= HDR_RATIO;

		expmid = (unsigned char) (0xff & (exp_val>>7));
		explow = (unsigned char) (0xf0 & (exp_val<<1));

		sensor_write(sd, 0x3e05, explow);
		sensor_write(sd, 0x3e04, expmid);

		sensor_dbg("sensor_set_short_exp = %d line Done!\n", exp_val);

	} else {
		if (exp_val < 16)
			exp_val = 16;

		exphigh = (unsigned char) (0xf & (exp_val>>15));
		expmid = (unsigned char) (0xff & (exp_val>>7));
		explow = (unsigned char) (0xf0 & (exp_val<<1));

		sensor_write(sd, 0x3e02, explow);
		sensor_write(sd, 0x3e01, expmid);
		sensor_write(sd, 0x3e00, exphigh);
		sensor_dbg("sensor_set_exp = %d line Done!\n", exp_val);
	}
	info->exp = exp_val;
	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	data_type gainlow = 0;
	data_type gainhigh = 0;
	data_type gaindiglow = 0x80;
	data_type gaindighigh = 0x00;

	int gainana = gain_val << 2;


	if (gainana <= 96) {
		gainhigh = 0x03; //  x64
		gainlow = gainana;
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana <= 192) {// 3.008 * 64=192.512
		gainhigh = 0x23;
		gainlow = gainana * 63 / (96) + 1;
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana <= 384) {// 6.017 * 64= 385.088
		gainhigh = 0x27;
		gainlow = gainana * 63 / (192) + 1;
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana <= 768) {// 12.033 * 64= 770.112
		gainhigh = 0x2F;
		gainlow = gainana * 63 / (384) + 1;
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana <= 1540) { // 24.067 * 64= 1540.288
		gainhigh = 0x3F;
		gainlow = gainana * 63 / (772) + 2;
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana < 2 * 1540) { // digital gain, 24.067 * 64 * 2
		gainhigh = 0x3F;
		gainlow = 0x7F;
		gaindighigh = 0x00;
		gaindiglow = 127 * gainana  / (1540) + 1 ;
		gaindiglow = gaindiglow > 0xFE ? 0xFE : gaindiglow;
	} else if (gainana < 4 * 1540) {// 24.067 * 64 * 4
	     gainhigh = 0x3F;
	     gainlow = 0x7F;
	     gaindighigh = 0x01;
	     gaindiglow = 127 * gainana / (1540 * 2) +1  ;
		 gaindiglow = gaindiglow > 0xFE ? 0xFE : gaindiglow;
	} else if (gainana < 8 * 1540) { // 24.067 * 64 * 8
	     gainhigh = 0x3F;
	     gainlow = 0x7F;
	     gaindighigh = 0x03;
	     gaindiglow = 127 * gainana  / (1540 * 4) +1 ;
		 gaindiglow = gaindiglow > 0xFE ? 0xFE : gaindiglow;
	} else if (gainana < 16 * 1540) { // 24.067 * 64 * 16
	     gainhigh = 0x3F;
	     gainlow = 0x7F;
	     gaindighigh = 0x07;
	     gaindiglow = 127 * gainana  / (1540 * 8) + 1;
		 gaindiglow = gaindiglow > 0xFE ? 0xFE : gaindiglow;
	} else if (gainana <= 3175 * 1540 / 100) { // 24.067 * 64 * 32
	     gainhigh = 0x3F;
	     gainlow = 0x7F;
	     gaindighigh = 0x0f;
	     gaindiglow = 127 * gainana  / (1540 * 16) + 1;
		 gaindiglow = gaindiglow > 0xFE ? 0xFE : gaindiglow;
	} else {
		 gainhigh = 0x3F;
	     gainlow = 0x7F;
	     gaindighigh = 0x0f;
	     gaindiglow = 0xFE;
	}


	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		sensor_write(sd, 0x3e13, (unsigned char)gainlow);
		sensor_write(sd, 0x3e12, (unsigned char)gainhigh);
		sensor_write(sd, 0x3e11, (unsigned char)gaindiglow);
		sensor_write(sd, 0x3e10, (unsigned char)gaindighigh);
	}

	sensor_write(sd, 0x3e09, (unsigned char)gainlow);
	sensor_write(sd, 0x3e08, (unsigned char)gainhigh);
	sensor_write(sd, 0x3e07, (unsigned char)gaindiglow);
	sensor_write(sd, 0x3e06, (unsigned char)gaindighigh);

	sensor_dbg("sensor_set_anagain = %d, 0x%x, 0x%x Done!\n", gain_val, gainhigh, gainlow);
	sensor_dbg("digital_gain = 0x%x, 0x%x Done!\n", gaindighigh, gaindiglow);
	info->gain = gain_val;

	return 0;
}

static int DPC_frame_cnt;
static int DPC_Flag = 1;
static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	struct sensor_info *info = to_state(sd);
	int exp_val, gain_val;
	unsigned short R = 0;
	data_type R_high, R_low;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;
	if (gain_val < 1 * 16)
		gain_val = 16;
	if (exp_val > 0xfffff)
		exp_val = 0xfffff;
	if (fps_change_flag) {
		if (shutter_delay_cnt == shutter_delay) {
			shutter_delay_cnt = 0;
			fps_change_flag = 0;
		} else
			shutter_delay_cnt++;
	}


	//sensor_write(sd, 0x3812, 0x00);
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
	//sensor_write(sd, 0x3812, 0x30);

	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;

}


static int sensor_s_fps(struct v4l2_subdev *sd,
			struct sensor_fps *fps)
{
	data_type rdval1, rdval2, rdval3;
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;

	sc500ai_sensor_vts = wsize->pclk/fps->fps/wsize->hts;
	fps_change_flag = 1;

	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	data_type rdval;

	ret = sensor_read(sd, 0x0100, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == STBY_ON)
		ret = sensor_write(sd, 0x0100, rdval&0xfe);
	else
		ret = sensor_write(sd, 0x0100, rdval|0x01);
	return ret;
}

/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		ret = sensor_s_sw_stby(sd, STBY_ON);
		if (ret < 0)
			sensor_err("soft stby falied!\n");
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		usleep_range(10000, 12000);
		ret = sensor_s_sw_stby(sd, STBY_OFF);
		if (ret < 0)
			sensor_err("soft stby off falied!\n");
		cci_unlock(sd);
		break;
	case PWR_ON:
		sensor_dbg("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(30000, 32000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_dbg("PWR_OFF!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		vin_gpio_set_status(sd, POWER_EN, 0);
		cci_unlock(sd);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	switch (val) {
	case 0:
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(1000, 1200);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rdval = 0;

	sensor_read(sd, 0x3107, &rdval);
	if (rdval != (V4L2_IDENT_SENSOR>>8))
		return -ENODEV;
	sensor_print("0x3107 = 0x%x\n", rdval);
	sensor_read(sd, 0x3108, &rdval);
	if (rdval != (V4L2_IDENT_SENSOR&0xff))
		return -ENODEV;
	sensor_print("0x3108 = 0x%x\n", rdval);

	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 2880;
	info->height = 1620;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 30;	/* 30fps */

	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins,
				sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		ret = 0;
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		ret = sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_SET_FPS:
		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_ACT_SET_ZOOM:
		actuator_set_zoom(sd, (struct actuator_ctrl_step *)arg);
		break;
	case VIDIOC_VIN_ACT_SET_FOCUS:
		actuator_set_focus(sd, (struct actuator_ctrl_step *)arg);
		break;
	case VIDIOC_VIN_ACT_SET_IRIS:
		actuator_set_iris(sd, (struct actuator_ctrl_step *)arg);
		break;
	case VIDIOC_VIN_ACT_SET_CODE:
		actuator_set_code(sd, (struct actuator_ctrl *)arg);
		break;
	case VIDIOC_VIN_ACT_SET_ZOOM_POS:
		actuator_set_zoom_pos(sd, (struct actuator_ctrl *)arg);
		break;
	case VIDIOC_VIN_ACT_SET_IRIS_POS:
		actuator_set_iris_pos(sd, (struct actuator_ctrl *)arg);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
	{
		.desc = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.regs = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {

	 {
	 .width = 2880,
	 .height = 1620,
	 .hoffset = 0,
	 .voffset = 0,
	 .hts = 3200,
	 .vts = 3300,
	 .pclk = 316800000,
	 .mipi_bps = 810000000,
	 .fps_fixed = 30,
	 .bin_factor = 1,
	 .intg_min = 1 << 4,
	 .intg_max = (2*3300 - 8) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 128 << 4,
	 .regs = sensor_2880x1620p30_regs,
	 .regs_size = ARRAY_SIZE(sensor_2880x1620p30_regs),
	 .set_size = NULL,
	 },

	 {
	 .width = 2880,
	 .height = 1620,
	 .hoffset = 0,
	 .voffset = 0,
	 .hts = 3200,
	 .vts = 3300,
	 .pclk = 316800000,
	 .mipi_bps = 820800000,
	 .fps_fixed = 30,
	 .bin_factor = 1,
	 .if_mode = MIPI_VC_WDR_MODE,
	 .wdr_mode = ISP_DOL_WDR_MODE,
	 .intg_min = 1 << 4,
	 .intg_max = (2*3300 - 8) << 4,
	 .gain_min = 1 << 4,
	 .gain_max = 128 << 4,
	 .regs = sensor_2880x1620p30_wdr_regs,
	 .regs_size = ARRAY_SIZE(sensor_2880x1620p30_wdr_regs),
	 .set_size = NULL,
	 },

};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct sensor_info *info = to_state(sd);

	cfg->type = V4L2_MBUS_CSI2;
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE)
		cfg->flags = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
	else
		cfg->flags = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_g_gain(sd, &ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_g_exp(sd, &ctrl->val);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->val);
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	data_type rdval_l, rdval_h;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	struct sensor_exp_gain exp_gain;

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init\n");

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	sc500ai_sensor_vts = wsize->vts;

	exp_gain.exp_val = wsize->intg_max;
	exp_gain.gain_val =  wsize->gain_max/3;
	sensor_s_exp_gain(sd, &exp_gain);

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width,
		     wsize->height);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_dbg("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
		     info->current_wins->width, info->current_wins->height,
		     info->current_wins->fps_fixed, info->fmt->mbus_code);

	if (!enable)
		return 0;

	return sensor_reg_init(info);
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.g_volatile_ctrl = sensor_g_ctrl,
	.s_ctrl = sensor_s_ctrl,
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_power,
	.ioctl = sensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_parm = sensor_s_parm,
	.g_parm = sensor_g_parm,
	.s_stream = sensor_s_stream,
	.g_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.enum_mbus_code = sensor_enum_mbus_code,
	.enum_frame_size = sensor_enum_frame_size,
	.get_fmt = sensor_get_fmt,
	.set_fmt = sensor_set_fmt,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 2);

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			      256 * 1600, 1, 1 * 1600);
	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1,
			      65536 * 16, 1, 1);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

static int sensor_dev_id;

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int i;

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[i]);
	} else {
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[sensor_dev_id++]);
	}

	sensor_init_controls(sd, &sensor_ctrl_ops);

	mutex_init(&info->lock);

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET1 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->time_hs = 0x09;
	info->exp = 0;
	info->gain = 0;

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd;
	int i;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		sd = cci_dev_remove_helper(client, &cci_drv[i]);
	} else {
		sd = cci_dev_remove_helper(client, &cci_drv[sensor_dev_id++]);
	}

	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

static const struct i2c_device_id sensor_id_2[] = {
	{SENSOR_NAME_2, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);
MODULE_DEVICE_TABLE(i2c, sensor_id_2);

static struct i2c_driver sensor_driver[] = {
	{
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id,
	}, {
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME_2,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id_2,
	},
};
static __init int init_sensor(void)
{
	int i, ret = 0;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		ret = cci_dev_init_helper(&sensor_driver[i]);

	return ret;
}

static __exit void exit_sensor(void)
{
	int i;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		cci_dev_exit_helper(&sensor_driver[i]);
}

module_init(init_sensor);
module_exit(exit_sensor);
