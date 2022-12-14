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
#include <sunxi_drv_twi.h>
#include <aw_list.h>
#include <rtthread.h>
#include <log.h>
#include <init.h>

static sunxi_driver_twi_t twi0, twi1, twi2, twi3, twi4;

static rt_err_t twi_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_size_t ret = 0;
    sunxi_driver_twi_t *pusr, *pvfy;
    sunxi_hal_driver_twi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_twi_t, base);
    pusr = (sunxi_driver_twi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_twi_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->initialize)
    {
        ret = hal_drv->initialize(pusr->dev_id);
    }

    return ret;
}

static rt_err_t twi_close(struct rt_device *dev)
{
    rt_size_t ret = 0;
    sunxi_driver_twi_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_twi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_twi_t, base);
    pusr = (sunxi_driver_twi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_twi_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->uninitialize)
    {
        ret = hal_drv->uninitialize(pusr->dev_id);
    }

    return ret;
}

static rt_err_t twi_init(struct rt_device *dev)
{
    return 0;
}

static rt_size_t twi_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;
    sunxi_driver_twi_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_twi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_twi_t, base);
    pusr = (sunxi_driver_twi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_twi_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->send)
    {
        ret = hal_drv->send(pusr->dev_id, pos, buffer, size);
    }

    return ret;
}

static rt_size_t twi_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;
    sunxi_driver_twi_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_twi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_twi_t, base);
    pusr = (sunxi_driver_twi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_twi_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->receive)
    {
        ret = hal_drv->receive(pusr->dev_id, pos, buffer, size);
    }

    return ret;
}

static rt_err_t twi_control(struct rt_device *dev, int cmd, void *args)
{
    rt_size_t ret = 0;
    sunxi_driver_twi_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_twi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_twi_t, base);
    pusr = (sunxi_driver_twi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_twi_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->control)
    {
        ret = hal_drv->control(pusr->dev_id, cmd, args);
    }

    return ret;
}

static void init_twi_device(struct rt_device *dev, void *usr_data, char *dev_name)
{
    rt_err_t ret;

    dev->open       = twi_open;
    dev->close      = twi_close;
    dev->init       = twi_init;
    dev->write      = twi_write;
    dev->read       = twi_read;
    dev->control    = twi_control;
    dev->user_data  = usr_data;

    ret = rt_device_register(dev, dev_name, RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        __err("fatal error, ret %d, register device to rtsystem failure.\n", ret);
        while (1);
        return;
    }
}

static const sunxi_hal_driver_twi_t sunxi_hal_twi_driver =
{
    .initialize     = hal_twi_init,
    .uninitialize   = hal_twi_uninit,
    .send           = hal_twi_write,
    .receive        = hal_twi_read,
    .control        = hal_twi_control,
};

int sunxi_driver_twi_init(void)
{
    struct rt_device *device0, *device1, *device2, *device3, *device4;

    device0 = &twi0.base;
    twi0.dev_id = 0;
    twi0.hal_drv = &sunxi_hal_twi_driver;

    device1 = &twi1.base;
    twi1.dev_id = 1;
    twi1.hal_drv = &sunxi_hal_twi_driver;

    device2 = &twi2.base;
    twi2.dev_id = 2;
    twi2.hal_drv = &sunxi_hal_twi_driver;

    device3 = &twi3.base;
    twi3.dev_id = 3;
    twi3.hal_drv = &sunxi_hal_twi_driver;

    device4 = &twi4.base;
    twi4.dev_id = 4;
    twi4.hal_drv = &sunxi_hal_twi_driver;

    init_twi_device(device0, &twi0, "twi0");
    init_twi_device(device1, &twi1, "twi1");
    init_twi_device(device2, &twi2, "twi2");
    init_twi_device(device3, &twi3, "twi3");
    init_twi_device(device4, &twi4, "twi4");

    return 0;
}

subsys_initcall(sunxi_driver_twi_init);