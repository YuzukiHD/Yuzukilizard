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
#include <sunxi_drv_avs.h>
#include <aw_list.h>
#include <rtthread.h>
#include <stdio.h>
#include <log.h>
#include <init.h>

static rt_err_t avs_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_size_t ret = 0;
    sunxi_driver_avs_t *pusr, *pvfy;
    sunxi_hal_driver_avs_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_avs_t, base);
    pusr = (sunxi_driver_avs_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_avs_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->initialize)
    {
        ret = hal_drv->initialize(pusr->dev_id);
    }

    return ret;
}

static rt_err_t avs_close(struct rt_device *dev)
{
    rt_size_t ret = 0;
    sunxi_driver_avs_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_avs_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_avs_t, base);
    pusr = (sunxi_driver_avs_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_avs_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->uninitialize)
    {
        ret = hal_drv->uninitialize(pusr->dev_id);
    }

    return ret;
}

static rt_err_t avs_init(struct rt_device *dev)
{
    return 0;
}

static rt_size_t avs_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;
    sunxi_driver_avs_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_avs_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_avs_t, base);
    pusr = (sunxi_driver_avs_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_avs_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->send)
    {
        ret = hal_drv->send(pusr->dev_id, pos, buffer, size);
    }

    return ret;
}

static rt_size_t avs_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;
    sunxi_driver_avs_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_avs_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_avs_t, base);
    pusr = (sunxi_driver_avs_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_avs_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->receive)
    {
        ret = hal_drv->receive(pusr->dev_id, pos, buffer, size);
    }

    return ret;
}

static rt_err_t avs_control(struct rt_device *dev, int cmd, void *args)
{
    rt_size_t ret = 0;
    sunxi_driver_avs_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_avs_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_avs_t, base);
    pusr = (sunxi_driver_avs_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_avs_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->control)
    {
        ret = hal_drv->control(pusr->dev_id, cmd, args);
    }

    return ret;
}

static void init_avs_device(struct rt_device *dev, void *usr_data, char *dev_name)
{
    rt_err_t ret;

    dev->open       = avs_open;
    dev->close      = avs_close;
    dev->init       = avs_init;
    dev->write      = avs_write;
    dev->read       = avs_read;
    dev->control    = avs_control;
    dev->user_data  = usr_data;

    ret = rt_device_register(dev, dev_name, RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        __err("fatal error, ret %d, register device to rtsystem failure.\n", ret);
        while (1);
        return;
    }
}

static const sunxi_hal_driver_avs_t sunxi_hal_avs_driver =
{
    .initialize     = hal_avs_init,
    .uninitialize   = hal_avs_uninit,
    .send           = NULL,
    .receive        = NULL,
    .control        = hal_avs_control,
};

int sunxi_driver_avs_init(void)
{
    struct rt_device *device[AVS_NUM];
    static sunxi_driver_avs_t avs[AVS_NUM];
    int i;

    for (i = 0; i < AVS_NUM; i++)
    {
	device[i] = &avs[i].base;
	avs[i].dev_id	= i;
	avs[i].hal_drv = &sunxi_hal_avs_driver;
	avs[i].used = 0;
	sprintf(avs[i].name, "avs%d", i);
	init_avs_device(device[i], &avs[i], avs[i].name);
    }

    return 0;
}

subsys_initcall(sunxi_driver_avs_init);