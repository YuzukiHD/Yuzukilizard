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
/*
 * ===========================================================================================
 *
 *	Filename:  drv_pwm.c
 *
 *	Description:	implemtaton of spi driver core based on hal.
 *
 *	Version:	Melis3.0
 *      Create:		2019-12-23
 *      Revision:	none
 *      Compiler:	GCC:version 9.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *  Author:  liuyu@allwinnertech.com
 *  Organization:  SWC-BPD
 *  Last Modified:  2021-1-1
 *
 * ===========================================================================================
 */
#include <stdint.h>
#include <sunxi_drv_pwm.h>
#include <sunxi_hal_pwm.h>
#include <aw_list.h>
#include <rtthread.h>
#include <log.h>
#include <init.h>
#include <rtdef.h>

static sunxi_driver_pwm_t pwm;

static rt_err_t pwm_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_size_t ret = 0;
    sunxi_driver_pwm_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_pwm_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_pwm_t, base);
    pusr = (sunxi_driver_pwm_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_pwm_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->control)
    {
        ret = hal_drv->initialize();
    }

    return ret;
}

static rt_err_t pwm_init(struct rt_device *dev)
{
    return 0;
}

static rt_err_t pwm_close(struct rt_device *dev)
{
    rt_size_t ret = 0;
    sunxi_driver_pwm_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_pwm_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_pwm_t, base);
    pusr = (sunxi_driver_pwm_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_pwm_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->uninitialize)
    {
        ret = hal_drv->uninitialize();
    }

    return 0;
}

static rt_size_t pwm_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    return 0;
}

static rt_size_t pwm_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    return 0;
}

static rt_err_t pwm_control(struct rt_device *dev, int cmd, void *args)
{
    rt_size_t ret = 0;
    sunxi_driver_pwm_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_pwm_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_pwm_t, base);
    pusr = (sunxi_driver_pwm_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_pwm_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->control)
    {
        ret = hal_drv->control(cmd, args);
    }

    return ret;
}

static void init_pwm_device(struct rt_device *dev, void *usr_data, char *dev_name)
{
    rt_err_t ret;

    dev->open       = pwm_open;
    dev->init       = pwm_init;
    dev->write      = pwm_write;
    dev->close      = pwm_close;
    dev->read       = pwm_read;
    dev->control    = pwm_control;
    dev->user_data  = usr_data;

    ret = rt_device_register(dev, dev_name, RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        __err("fatal error, ret %d, register device to rtsystem failure.\n", ret);
        while (1);
        return;
    }
}

static const sunxi_hal_driver_pwm_t sunxi_hal_pwm_driver =
{
    .initialize     = hal_pwm_init,
    .control        = hal_pwm_control,
    .uninitialize   = hal_pwm_deinit,
};

int sunxi_driver_pwm_init(void)
{
    struct rt_device *device;

    device = &pwm.base;
    pwm.dev_id = 0;
    pwm.hal_drv = &sunxi_hal_pwm_driver;
    init_pwm_device(device, &pwm, "pwm");

    return 0;
}

subsys_initcall(sunxi_driver_pwm_init);