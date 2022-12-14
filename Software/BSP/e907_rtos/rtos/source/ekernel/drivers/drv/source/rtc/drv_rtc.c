/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <sunxi_hal_rtc.h>
#include <sunxi_drv_rtc.h>

#include <rtthread.h>
#include <log.h>
#include <init.h>
const sunxi_hal_driver_rtc_t sunxi_hal_rtc_driver;
static sunxi_driver_rtc_t rtc;

static rt_err_t rtc_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_size_t ret = 0;
    sunxi_driver_rtc_t *pusr, *pvfy;
    sunxi_hal_driver_rtc_t *hal_drv = NULL;

    if (dev == NULL)
    {
        return -1;
    }

    pvfy = rt_container_of(dev, sunxi_driver_rtc_t, base);
    pusr = (sunxi_driver_rtc_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_rtc_t *)pusr->hal_drv;
    }

    return ret;
}

static rt_err_t rtc_close(struct rt_device *dev)
{
    rt_size_t ret = 0;
    sunxi_driver_rtc_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_rtc_t *hal_drv = NULL;

    if (dev == NULL)
    {
        return -1;
    }

    pvfy = rt_container_of(dev, sunxi_driver_rtc_t, base);
    pusr = (sunxi_driver_rtc_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_rtc_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->uninitialize)
    {
        ret = hal_drv->uninitialize();
    }

    return ret;
}

static rt_err_t rtc_init(struct rt_device *dev)
{
    int ret;
#if defined(CONFIG_SOC_SUN20IW1)
    ret = hal_rtc_init();
    if (ret)
    {
	__err("fatal error, ret %d, init rtc hal failure.\n", ret);
	return -1;
    }
#endif
    return 0;
}

static rt_size_t rtc_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
	return 0;
}

static rt_size_t rtc_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
	return 0;
}

static rt_err_t rtc_control(struct rt_device *dev, int cmd, void *args)
{
    rt_size_t ret = 0;
    sunxi_driver_rtc_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_rtc_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = rt_container_of(dev, sunxi_driver_rtc_t, base);
    pusr = (sunxi_driver_rtc_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_rtc_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->control)
    {
        ret = hal_drv->control(cmd, args);
    }

    return ret;
}

static void init_rtc_device(struct rt_device *dev, void *usr_data, char *dev_name)
{
    rt_err_t ret;

    dev->open       = rtc_open;
    dev->close      = rtc_close;
    dev->init       = rtc_init;
    dev->write      = rtc_write;
    dev->read       = rtc_read;
    dev->control    = rtc_control;
    dev->user_data  = usr_data;

    ret = rt_device_register(dev, dev_name, RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        __err("fatal error, ret %d, register device to rtsystem failure.\n", ret);
    }
}

int hal_rtc_control(hal_rtc_transfer_cmd_t cmd, void *args)
{
    struct rtc_time *hal_rtc_time;
    struct rtc_wkalrm *hal_rtc_wkalarm;
    rtc_callback_t user_callback;
    unsigned int *enabled;

    switch (cmd)
    {
#ifdef CONFIG_ARM
        case RTC_GET_TIME:
            hal_rtc_time = (struct rtc_time *)args;
            return hal_rtc_gettime(hal_rtc_time);
        case RTC_SET_TIME:
            hal_rtc_time = (struct rtc_time *)args;
            return hal_rtc_settime(hal_rtc_time);
        case RTC_GET_ALARM:
            hal_rtc_wkalarm = (struct rtc_wkalrm *)args;
            return hal_rtc_getalarm(hal_rtc_wkalarm);
        case RTC_SET_ALARM:
            hal_rtc_wkalarm = (struct rtc_wkalrm *)args;
            return hal_rtc_setalarm(hal_rtc_wkalarm);
        case RTC_CALLBACK:
            user_callback = (rtc_callback_t)args;
            return hal_rtc_register_callback(user_callback);
        case RTC_IRQENABLE:
            enabled = (unsigned int *)args;
            return hal_rtc_alarm_irq_enable(*enabled);
#endif
        default:
            return -1;
    }
}

const sunxi_hal_driver_rtc_t sunxi_hal_rtc_driver =
{
    .initialize     = hal_rtc_init,
    .uninitialize   = hal_rtc_deinit,
    .control        = hal_rtc_control,
};

int sunxi_driver_rtc_init(void)
{
    struct rt_device *device;

    device = &rtc.base;
    rtc.dev_id = 0;
    rtc.hal_drv = &sunxi_hal_rtc_driver;

    init_rtc_device(device, &rtc, "rtc");

    return 0;
}

subsys_initcall(sunxi_driver_rtc_init);