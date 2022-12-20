
/*
 * A V4L2 driver for TP9953 YUV cameras.
 *
 * Copyright (c) 2022 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  maliankang <maliankang@allwinnertech.com>
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
#define DUMP_TEST

MODULE_AUTHOR("zw");
MODULE_DESCRIPTION("A low-level driver for TP9953 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (27*1000*1000)
#define CLK_POH           V4L2_MBUS_PCLK_SAMPLE_RISING
#define CLK_POL           V4L2_MBUS_PCLK_SAMPLE_FALLING
#define V4L2_IDENT_SENSOR 0x6328


/* enable tp9953 sensor detect */
#define SENSOR_DETECT_KTHREAD 1
/* USE DETECT BY GPIO_IRQ OR POLLING
 * DET_USE_POLLING 0 meant by gpio_irq
 * DET_USE_POLLING 1 meant by POLLING
 * */
#define DET_USE_POLLING 1


#if SENSOR_DETECT_KTHREAD
struct sensor_indetect {
	struct class *sensor_class;
	struct task_struct *sensor_task;
	struct device *dev;
	struct cdev *cdev;
	struct gpio_config detect_power;
	struct gpio_config detect_gpio;
	struct delayed_work tp9953_work;
#if DET_USE_POLLING
	data_type   last_status;
#else
	unsigned int detect_irq;
#endif
	dev_t devno;
} sensor_indetect;
static DEFINE_MUTEX(det_mutex);
#endif

/*
 * Our nominal (default) frame rate.
 */
#define SENSOR_FRAME_RATE 30

/*
 * The TP9920 sits on i2c with ID 0x88 or 0x8a
 * SAD-low:0x88 SAD-high:0x8a
 */
#define I2C_ADDR 0x88
#define SENSOR_NAME "tp9953"
static struct regval_list reg_dump[] = {
	{0x00, 0x00},
	{0x01, 0x00},
	{0x02, 0x00},
	{0x03, 0x00},
	{0x04, 0x00},
	{0x05, 0x00},
	{0x06, 0x00},
	{0x07, 0x00},
	{0x08, 0x00},
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x00},
	{0x0e, 0x00},
	{0x0f, 0x00},

	{0x10, 0x00},
	{0x11, 0x00},
	{0x12, 0x00},
	{0x13, 0x00},
	{0x14, 0x00},
	{0x15, 0x00},
	{0x16, 0x00},
	{0x17, 0x00},
	{0x18, 0x00},
	{0x19, 0x00},
	{0x1a, 0x00},
	{0x1b, 0x00},
	{0x1c, 0x00},
	{0x1d, 0x00},
	{0x1e, 0x00},
	{0x1f, 0x00},

	{0x20, 0x00},
	{0x21, 0x00},
	{0x22, 0x00},
	{0x23, 0x00},
	{0x24, 0x00},
	{0x25, 0x00},
	{0x26, 0x00},
	{0x27, 0x00},
	{0x28, 0x00},
	{0x29, 0x00},
	{0x2a, 0x00},
	{0x2b, 0x00},
	{0x2c, 0x00},
	{0x2d, 0x00},
	{0x2e, 0x00},
	{0x2f, 0x00},

	{0x30, 0x00},
	{0x31, 0x00},
	{0x32, 0x00},
	{0x33, 0x00},
	{0x34, 0x00},
	{0x35, 0x00},
	{0x36, 0x00},
	{0x37, 0x00},
	{0x38, 0x00},
	{0x39, 0x00},
	{0x3a, 0x00},
	{0x3b, 0x00},
	{0x3c, 0x00},
	{0x3d, 0x00},
	{0x3e, 0x00},
	{0x3f, 0x00},

	{0x40, 0x00},
	{0x41, 0x00},
	{0x42, 0x00},
	{0x43, 0x00},
	{0x44, 0x00},
	{0x45, 0x00},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x48, 0x00},
	{0x49, 0x00},
	{0x4a, 0x00},
	{0x4b, 0x00},
	{0x4c, 0x00},
	{0x4d, 0x00},
	{0x4e, 0x00},
	{0x4f, 0x00},

	{0x50, 0x00},
	{0x51, 0x00},
	{0x52, 0x00},
	{0x53, 0x00},
	{0x54, 0x00},
	{0x55, 0x00},
	{0x56, 0x00},
	{0x57, 0x00},
	{0x58, 0x00},
	{0x59, 0x00},
	{0x5a, 0x00},
	{0x5b, 0x00},
	{0x5c, 0x00},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x00},

	{0x61, 0x00},
	{0x62, 0x00},
	{0x63, 0x00},
	{0x64, 0x00},
	{0x65, 0x00},
	{0x66, 0x00},
	{0x67, 0x00},
	{0x68, 0x00},
	{0x69, 0x00},
	{0x6a, 0x00},
	{0x6b, 0x00},
	{0x6c, 0x00},
	{0x6d, 0x00},
	{0x6e, 0x00},
	{0x6f, 0x00},

	{0x70, 0x00},
	{0x71, 0x00},
	{0x72, 0x00},
	{0x73, 0x00},
	{0x74, 0x00},
	{0x75, 0x00},
	{0x76, 0x00},
	{0x77, 0x00},
	{0x78, 0x00},
	{0x79, 0x00},
	{0x7a, 0x00},
	{0x7b, 0x00},
	{0x7c, 0x00},
	{0x7d, 0x00},
	{0x7e, 0x00},
	{0x7f, 0x00},

	{0x80, 0x00},
	{0x81, 0x00},
	{0x82, 0x00},
	{0x83, 0x00},
	{0x84, 0x00},
	{0x85, 0x00},
	{0x86, 0x00},
	{0x87, 0x00},
	{0x88, 0x00},
	{0x89, 0x00},
	{0x8a, 0x00},
	{0x8b, 0x00},
	{0x8c, 0x00},
	{0x8d, 0x00},
	{0x8e, 0x00},
	{0x8f, 0x00},

	{0x90, 0x00},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x00},
	{0x96, 0x00},
	{0x97, 0x00},
	{0x98, 0x00},
	{0x99, 0x00},
	{0x9a, 0x00},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x00},
	{0x9f, 0x00},

	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0xa3, 0x00},
	{0xa4, 0x00},
	{0xa5, 0x00},
	{0xa6, 0x00},
	{0xa7, 0x00},
	{0xa8, 0x00},
	{0xa9, 0x00},
	{0xaa, 0x00},
	{0xab, 0x00},
	{0xac, 0x00},
	{0xad, 0x00},
	{0xae, 0x00},
	{0xaf, 0x00},

	{0xb0, 0x00},
	{0xb1, 0x00},
	{0xb2, 0x00},
	{0xb3, 0x00},
	{0xb4, 0x00},
	{0xb5, 0x00},
	{0xb6, 0x00},
	{0xb7, 0x00},
	{0xb8, 0x00},
	{0xb9, 0x00},
	{0xba, 0x00},
	{0xbb, 0x00},
	{0xbc, 0x00},
	{0xbd, 0x00},
	{0xbe, 0x00},
	{0xbf, 0x00},

	{0xc0, 0x00},
	{0xc1, 0x00},
	{0xc2, 0x00},
	{0xc3, 0x00},
	{0xc4, 0x00},
	{0xc5, 0x00},
	{0xc6, 0x00},
	{0xc7, 0x00},
	{0xc8, 0x00},
	{0xc9, 0x00},
	{0xca, 0x00},
	{0xcb, 0x00},
	{0xcc, 0x00},
	{0xcd, 0x00},
	{0xce, 0x00},
	{0xcf, 0x00},

	{0xd0, 0x00},
	{0xd1, 0x00},
	{0xd2, 0x00},
	{0xd3, 0x00},
	{0xd4, 0x00},
	{0xd5, 0x00},
	{0xd6, 0x00},
	{0xd7, 0x00},
	{0xd8, 0x00},
	{0xd9, 0x00},
	{0xda, 0x00},
	{0xdb, 0x00},
	{0xdc, 0x00},
	{0xdd, 0x00},
	{0xde, 0x00},
	{0xdf, 0x00},

	{0xf0, 0x00},
	{0xf1, 0x00},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf4, 0x00},
	{0xf5, 0x00},
	{0xf6, 0x00},
	{0xf7, 0x00},
	{0xf8, 0x00},
	{0xf9, 0x00},
	{0xfa, 0x00},
	{0xfb, 0x00},
	{0xfc, 0x00},
	{0xfd, 0x00},
	{0xfe, 0x00},
	{0xff, 0x00},
};

#if 0
static struct regval_list reg_1080p30_2ch[] = {
	{0x40, 0x04},
	{0x02, 0xc8},
	{0x0d, 0x70},
	{0x14, 0x40},
	{0x1c, 0x88},
	{0x1d, 0x96},
	{0x15, 0x03},
	{0x16, 0xd0},
	{0x17, 0x80},
	{0x18, 0x2a},
	{0x19, 0x38},
	{0x1a, 0x47},
	{0x20, 0x38},
	{0x27, 0xad},

	/*******/
	{0x2a, 0x3C},
	/********/

	{0x2d, 0x48},
	{0x30, 0x52},
	{0x31, 0xca},
	{0x32, 0xf0},
	{0x33, 0x20},
	{0x42, 0xf3},
	{0x44, 0x51},
	{0x51, 0x00},
	{0xe8, 0x05},
	{0xea, 0x01},
	{0xeb, 0x01},
	{0xf1, 0x30},
	{0xf6, 0x10},
	{0x40, 0x08},
	{0x02, 0x80},
	{0x03, 0x80},
	{0x04, 0x80},
	{0x05, 0x80},
	{0x06, 0x80},
	{0x13, 0xef},
	{0x15, 0x00},


};
#endif

#if 1
static struct regval_list reg_1080p25_2ch[] = {
	{0x40, 0x04},
	{0x02, 0xc8},
	{0x0d, 0x70},
	{0x1c, 0x8a},
	{0x1d, 0x4e},
	{0x15, 0x03},
	{0x16, 0xd0},
	{0x17, 0x80},
	{0x18, 0x2a},
	{0x19, 0x38},
	{0x1a, 0x47},
	{0x20, 0x3c},
	{0x21, 0x46},
	{0x27, 0xad},
	/*
	 * 0x2a寄存器
	 * 不设置,寄存器默认值为0x30
	 * 设置0x30,无信号出黑屏,有信号出图
	 * 设置0x34,无信号出蓝屏,有信号出图
	 * 设置0x3c,有无信号都出蓝屏
	 */
	{0x2a, 0x34},
	{0x2c, 0x3a},
	{0x2d, 0x48},
	{0x2e, 0x40},
	{0x30, 0x52},
	{0x31, 0xc3},
	{0x32, 0x7d},
	{0x33, 0xa0},
	{0x42, 0xf3},
	/*
	 * 0x44寄存器,设置clk delay
	 * 有的板子阻抗,物料差异要求不一样,可以调整寄存器高4位测试
	 */
	{0x44, 0xf1},
	{0x51, 0x00},
	{0xe8, 0x05},
	{0xea, 0x01},
	{0xeb, 0x01},
	{0xf1, 0x30},
	{0xf6, 0x10},

	//设置通道ID
	{0x40, 0x00},
	{0x34, 0x10},
	{0x40, 0x01},
	{0x34, 0x11},

	{0x40, 0x08},
	{0x02, 0x80},
	{0x03, 0x80},
	{0x04, 0x80},
	{0x05, 0x80},
	{0x06, 0x80},
	{0x13, 0xef},
	{0x15, 0x00},
};
#endif

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	if (on_off)
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
	else
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
	return 0;
}

static int sensor_power(struct v4l2_subdev *sd, int on)
{
	switch (on) {
	case STBY_ON:
		sensor_dbg("CSI_SUBDEV_STBY_ON!\n");
		sensor_s_sw_stby(sd, ON);
		break;
	case STBY_OFF:
		sensor_dbg("CSI_SUBDEV_STBY_OFF!\n");
		sensor_s_sw_stby(sd, OFF);
		break;
	case PWR_ON:
		sensor_dbg("CSI_SUBDEV_PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(2000, 2100);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(20000, 21000);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(20000, 21000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(40000, 41000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_dbg("CSI_SUBDEV_PWR_OFF!\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		usleep_range(100, 120);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, POWER_EN, 0);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		usleep_range(2000, 2100);
		cci_unlock(sd);
		break;
	default:
			return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
	usleep_range(5000, 6000);
	vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
	usleep_range(5000, 6000);
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rdval, rdval1, rdval2;
	int cnt = 0;

	rdval = 0;
	rdval1 = 0;
	rdval2 = 0;

	sensor_write(sd, 0x40, 0x00);
	sensor_read(sd, 0xff, &rdval2);
	sensor_read(sd, 0xfe, &rdval1);
	rdval = ((rdval2<<8) & 0xff00) | rdval1;
	sensor_print("L:[%d] V4L2_IDENT_SENSOR = 0x%x\n", __LINE__, rdval);
#if 1
	while ((rdval != V4L2_IDENT_SENSOR) && (cnt < 5)) {
		sensor_read(sd, 0xff, &rdval2);
		sensor_read(sd, 0xfe, &rdval1);
		rdval = ((rdval2<<8) & 0xff00) | rdval1;
		sensor_print("retry = %d, V4L2_IDENT_SENSOR = 0x%x\n", cnt,
				rdval);
		cnt++;
	}

	if (rdval != V4L2_IDENT_SENSOR)
		return -ENODEV;
#endif
	printk("[%s]-[%s]-[%d]:\n", __FILE__, __func__, __LINE__);
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
	info->width = HD1080_WIDTH;
	info->height = HD1080_HEIGHT;
	info->hflip = 0;
	info->vflip = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 25;	/* 25fps */

	info->preview_first_flag = 1;
	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg,
					info->current_wins,
					sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
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
		.desc = "BT656 2CH",
		.mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,
		//	.mbus_code = MEDIA_BUS_FMT_YUYV8_1X16,
		.regs = NULL,
		.regs_size = 0,
		.bpp = 2,
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
#if 0
	{
		.width = HD1080_WIDTH,
		.height = HD1080_HEIGHT,
		.hoffset = 0,
		.voffset = 0,
		.fps_fixed = 30,
		.regs = reg_1080p30_2ch,
		.regs_size = ARRAY_SIZE(reg_1080p30_2ch),
		.set_size = NULL,
	},
#endif
#if 1
	{
		.width = HD1080_WIDTH,
		.height = HD1080_HEIGHT,
		.hoffset = 0,
		.voffset = 0,
		.fps_fixed = 25,
		.regs = reg_1080p25_2ch,
		.regs_size = ARRAY_SIZE(reg_1080p25_2ch),
		.set_size = NULL,
	},
#endif
};
#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
		struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_BT656;
	cfg->flags = CLK_POL | CLK_POH | CSI_CH_0 | CSI_CH_1;
	//cfg->flags = CLK_POL | CSI_CH_0;
	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;

	sensor_dbg("sensor_reg_init\n");

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->fmt = sensor_fmt;
	info->width = wsize->width;
	info->height = wsize->height;

	return 0;
}

#if SENSOR_DETECT_KTHREAD
static int __sensor_insert_detect(data_type *val)
{
	int ret;

	mutex_lock(&det_mutex);
	/*detecting the sensor insert by detect_gpio
	 * if detect_gpio was height meant sensor was insert
	 * if detect_gpio was low, meant sensor was remove
	 * */
	ret = gpio_get_value_cansleep(sensor_indetect.detect_gpio.gpio);
	if (ret) {
		*val = 0x03;
	} else {
		*val = 0x04;
	}

	mutex_unlock(&det_mutex);

	return 0;
}

void sensor_msg_sent(char *buf)
{
	char *envp[2];

	envp[0] = buf;
	envp[1] = NULL;
	kobject_uevent_env(&sensor_indetect.dev->kobj, KOBJ_CHANGE, envp);
}

static void sensor_det_work(struct work_struct *work)
{
	char buf[32];
	data_type val;

	__sensor_insert_detect(&val);

#if DET_USE_POLLING
	if (sensor_indetect.last_status != val) {
		memset(buf, 0, 32);
		snprintf(buf, sizeof(buf), "SENSOR_RAVAL=0x%x", val);

		sensor_msg_sent(buf);
		sensor_indetect.last_status = val;
		vin_print("val = 0x%x, Sent msg to user\n", val);
	}

	schedule_delayed_work(&sensor_indetect.tp9953_work, (msecs_to_jiffies(1 * 1000)));
#else
	memset(buf, 0, 32);
	snprintf(buf, sizeof(buf), "SENSOR_RAVAL=0x%x", val);

	sensor_msg_sent(buf);
	vin_print("val = 0x%x, Sent msg to user\n", val);
#endif

}


#if !DET_USE_POLLING
static irqreturn_t sensor_det_irq_func(int irq, void *priv)
{
	/* the work of detected was be run in workquen */
	schedule_delayed_work(&sensor_indetect.tp9953_work, 0);
	return IRQ_HANDLED;
}
#endif

#endif

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_print("%s on = %d, %d*%d %x\n", __func__, enable,
			info->current_wins->width,
			info->current_wins->height, info->fmt->mbus_code);

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
static struct cci_driver cci_drv = {
	.name = SENSOR_NAME,
	.addr_width = CCI_BITS_8,
	.data_width = CCI_BITS_8,
};

static int sensor_init_controls(struct v4l2_subdev *sd,
		const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 2);

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			256 * 1600, 1, 1 * 1600);
	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 0,
			65536 * 16, 1, 0);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}


#if SENSOR_DETECT_KTHREAD
static int sensor_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int sensor_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t sensor_dev_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t sensor_dev_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static int sensor_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

static long sensor_dev_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	return 0;
}

static const struct file_operations sensor_dev_fops = {
	.owner          = THIS_MODULE,
	.open           = sensor_dev_open,
	.release        = sensor_dev_release,
	.write          = sensor_dev_write,
	.read           = sensor_dev_read,
	.unlocked_ioctl = sensor_dev_ioctl,
	.mmap           = sensor_dev_mmap,
};

static ssize_t get_det_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	data_type val;
	__sensor_insert_detect(&val);
	return sprintf(buf, "0x%x\n", val);
}
#ifdef DUMP_TEST
static ssize_t get_tp9953_dump0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t val;
	data_type  value_ = 0;
	int i = 0;
	sensor_print("habo-->ready to write the 0x40=0x00\n");
	sensor_write(cci_drv.sd, 0x40, 0x00);
	for (i = 0 ; i < ARRAY_SIZE(reg_dump) ; i++) {

		sensor_read(cci_drv.sd, reg_dump[i].addr, &value_);
		sensor_print("habo--->0x40=0x00 0x%x = 0x%x \n", reg_dump[i].addr, value_);
		val = value_;
		value_ = 0;
	}
	return val;
}

static ssize_t get_tp9953_dump1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t val;
	data_type  value_ = 0;
	int i = 0;
	sensor_print("habo-->ready to write the 0x40=0x01\n");
	sensor_write(cci_drv.sd, 0x40, 0x01);
	for (i = 0 ; i < ARRAY_SIZE(reg_dump) ; i++) {

		sensor_read(cci_drv.sd, reg_dump[i].addr, &value_);
		sensor_print("habo--->0x40=0x01 0x%x = 0x%x \n", reg_dump[i].addr, value_);
		val = value_;
		value_ = 0;
	}

	return val;
}
#endif

#ifdef DUMP_TEST
static struct device_attribute  detect_dev_attrs[] = {
	{
		.attr = {
			.name = "online",
			.mode =  S_IRUGO,
		},
		.show =  get_det_status_show,
		.store = NULL,
	},

	{
		.attr = {
			.name = "dump0",
			.mode =  S_IRUGO,
		},
		.show =  get_tp9953_dump0_show,
		.store = NULL,
	},

	{
		.attr = {
			.name = "dump1",
			.mode =  S_IRUGO,
		},
		.show =  get_tp9953_dump1_show,
		.store = NULL,
	},
};
#else
static struct device_attribute  detect_dev_attrs = {
	.attr = {
		.name = "online",
		.mode =  S_IRUGO,
	},
	.show =  get_det_status_show,
	.store = NULL,

};

#endif
static int tp9953_sensor_det_init(struct i2c_client *client)
{
	int ret;
	struct device_node *np = NULL;

	/* enable detect work queue */
	INIT_DELAYED_WORK(&sensor_indetect.tp9953_work, sensor_det_work);
	np = of_find_node_by_name(NULL, "tp9953_detect");
	if (np == NULL) {
		sensor_err("can not find the tp9953_detect node, will not \
				enable detect kthread\n");
		return -1;
	}

	sensor_indetect.detect_power.gpio = of_get_named_gpio_flags(
			np, "gpio_power", 0,
			(enum of_gpio_flags *)(&(sensor_indetect.detect_power)));
	sensor_indetect.detect_gpio.gpio = of_get_named_gpio_flags(
			np, "gpio_detect", 0,
			(enum of_gpio_flags *)(&(sensor_indetect.detect_gpio)));

	if ((!gpio_is_valid(sensor_indetect.detect_power.gpio) ||
				sensor_indetect.detect_power.gpio < 0) &&
			(!gpio_is_valid(sensor_indetect.detect_gpio.gpio) ||
			 sensor_indetect.detect_gpio.gpio < 0)) {
		sensor_err("enable tp9953 sensor detect fail!!\n");
		return -1;
	} else {
		/* enable the detect power*/
		ret = gpio_request(sensor_indetect.detect_power.gpio, NULL);
		if (ret < 0) {
			sensor_err("enable detect_power fail!!gpio:%d\n", sensor_indetect.detect_power.gpio);
			return -1;
		}
		gpio_direction_output(sensor_indetect.detect_power.gpio, 1);
		__gpio_set_value(sensor_indetect.detect_power.gpio, 1);
		/* enable irq to detect */
		ret = gpio_request(sensor_indetect.detect_gpio.gpio, NULL);
		if (ret < 0) {
			sensor_err("enable  detect_gpio  fail! gpio:%d\n", sensor_indetect.detect_gpio.gpio);
			return -1;
		}

		gpio_direction_input(sensor_indetect.detect_gpio.gpio);

#if DET_USE_POLLING
	}
	/* start detect work  */
	schedule_delayed_work(&sensor_indetect.tp9953_work, 0);
#else
	/* request gpio to irq  */
	sensor_indetect.detect_irq =
		gpio_to_irq(sensor_indetect.detect_gpio.gpio);
	ret = request_irq(
			sensor_indetect.detect_irq, sensor_det_irq_func,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"tp9953_sensor_detect", client);
}
#endif
return 0;
}

static void tp9953_sensor_det_remove(void)
{
	cancel_delayed_work_sync(&sensor_indetect.tp9953_work);

#if !DET_USE_POLLING
	/*free irq*/
	if (free_irq > 0) {
		disable_irq(sensor_indetect.detect_irq);
		free_irq(sensor_indetect.detect_irq, NULL);
	}
#endif

	if (!(sensor_indetect.detect_power.gpio < 0 ||
				sensor_indetect.detect_gpio.gpio < 0)) {
		gpio_free(sensor_indetect.detect_power.gpio);
		gpio_free(sensor_indetect.detect_gpio.gpio);
	}
}

#endif

static int sensor_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int ret;
	int i = 0;
	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv);
	sensor_init_controls(sd, &sensor_ctrl_ops);
	mutex_init(&info->lock);

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;

#if SENSOR_DETECT_KTHREAD
	alloc_chrdev_region(&sensor_indetect.devno, 0, 1, "csi");
	sensor_indetect.cdev = cdev_alloc();
	if (IS_ERR(sensor_indetect.cdev)) {
		sensor_err("cdev_alloc fail!\n");
		goto free_devno;
	}

	cdev_init(sensor_indetect.cdev, &sensor_dev_fops);
	sensor_indetect.cdev->owner = THIS_MODULE;
	ret = cdev_add(sensor_indetect.cdev, sensor_indetect.devno, 1);
	if (ret) {
		sensor_err("cdev_add fail.\n");
		goto free_cdev;
	}

	sensor_indetect.sensor_class = class_create(THIS_MODULE, "csi");
	if (IS_ERR(sensor_indetect.sensor_class)) {
		sensor_err("class_create fail!\n");
		goto unregister_cdev;
	}



	sensor_indetect.dev =
		device_create(sensor_indetect.sensor_class, NULL,
				sensor_indetect.devno, NULL, "tp9953");
	if (IS_ERR(sensor_indetect.dev)) {
		sensor_err("device_create fail!\n");
		goto free_class;
	}
#ifdef DUMP_TEST
	for (i = 0; i < ARRAY_SIZE(detect_dev_attrs); i++) {
		ret = device_create_file(sensor_indetect.dev, &detect_dev_attrs[i]);
		if (ret) {
			sensor_err("class_create  file fail!\n");
			goto free_class;
		}
	}
#else
	ret = device_create_file(sensor_indetect.dev, &detect_dev_attrs);
	if (ret) {
		sensor_err("class_create  file fail!\n");
		goto free_class;
	}
#endif

	/* init tp9953 detect mode */
	ret = tp9953_sensor_det_init(client);
	if (ret) {
		goto free_det;
	}

	return 0;


free_det:
	tp9953_sensor_det_remove();

free_class:
	class_destroy(sensor_indetect.sensor_class);
#ifdef DUMP_TEST
	for (i--; i >= 0; i--)
		device_remove_file(sensor_indetect.dev,
				&detect_dev_attrs[i]);
#endif

unregister_cdev:
	cdev_del(sensor_indetect.cdev);

free_cdev:
	kfree(sensor_indetect.cdev);

free_devno:
	unregister_chrdev_region(sensor_indetect.devno, 1);


#endif

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd;

	sd = cci_dev_remove_helper(client, &cci_drv);

#if SENSOR_DETECT_KTHREAD
	tp9953_sensor_det_remove();

	device_destroy(sensor_indetect.sensor_class, sensor_indetect.devno);
	class_destroy(sensor_indetect.sensor_class);
	cdev_del(sensor_indetect.cdev);
	kfree(sensor_indetect.cdev);
	unregister_chrdev_region(sensor_indetect.devno, 1);
#endif

	kfree(to_state(sd));
	return 0;
}


static void sensor_shutdown(struct i2c_client *client)
{
	struct v4l2_subdev *sd;

	sd = cci_dev_remove_helper(client, &cci_drv);

#if SENSOR_DETECT_KTHREAD
	tp9953_sensor_det_remove();

	device_destroy(sensor_indetect.sensor_class, sensor_indetect.devno);
	class_destroy(sensor_indetect.sensor_class);
	cdev_del(sensor_indetect.cdev);
	kfree(sensor_indetect.cdev);
	unregister_chrdev_region(sensor_indetect.devno, 1);

#endif
	kfree(to_state(sd));
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.shutdown = sensor_shutdown,
	.id_table = sensor_id,
};
static __init int init_sensor(void)
{
	return cci_dev_init_helper(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	cci_dev_exit_helper(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);
