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
#include <sunxi_hal_spi.h>
#include <sunxi_drv_spi.h>
#include <aw_list.h>
#include <rtthread.h>
#include <log.h>
#include <init.h>

static sunxi_driver_spi_t spi0, spi1, spi2, spi3;

static rt_err_t spi_init(struct rt_device *dev)
{
    return 0;
}

static rt_err_t spi_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_size_t ret = 0;
    sunxi_driver_spi_t *pusr, *pvfy;
    sunxi_hal_driver_spi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_spi_t, base);
    pusr = (sunxi_driver_spi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_spi_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->initialize)
    {
        ret = hal_drv->initialize(pusr->dev_id);
    }

    return ret;
}

static rt_size_t spi_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;
    sunxi_driver_spi_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_spi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_spi_t, base);
    pusr = (sunxi_driver_spi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_spi_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->send)
    {
        ret = hal_drv->send(pusr->dev_id, buffer, size);
    }

    return ret;
    return 0;
}

static rt_err_t spi_close(struct rt_device *dev)
{
    rt_size_t ret = 0;
    sunxi_driver_spi_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_spi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_spi_t, base);
    pusr = (sunxi_driver_spi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_spi_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->uninitialize)
    {
        ret = hal_drv->uninitialize(pusr->dev_id);
    }

    return ret;
}

static rt_size_t spi_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;
    sunxi_driver_spi_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_spi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_spi_t, base);
    pusr = (sunxi_driver_spi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_spi_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->receive)
    {
        ret = hal_drv->receive(pusr->dev_id, buffer, size);
    }

    return ret;
}

static rt_err_t spi_control(struct rt_device *dev, int cmd, void *args)
{
    rt_size_t ret = 0;
    sunxi_driver_spi_t *pusr = NULL, *pvfy = NULL;
    sunxi_hal_driver_spi_t *hal_drv = NULL;

    if (dev == NULL)
    {
        while (1);
    }

    pvfy = container_of(dev, sunxi_driver_spi_t, base);
    pusr = (sunxi_driver_spi_t *)dev->user_data;

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_spi_t *)pusr->hal_drv;
    }

    switch (cmd)
    {
        case SPI_WRITE_READ:
            if (hal_drv && hal_drv->transfer)
            {
                ret = hal_drv->transfer(pusr->dev_id, (hal_spi_master_transfer_t *)args);
            }
            break;
        case SPI_CONFIG:
            if (hal_drv && hal_drv->hw_config)
            {
                ret = hal_drv->hw_config(pusr->dev_id, (hal_spi_master_config_t *)args);
            }
            break;
        default:
            ret = SPI_MASTER_ERROR;
    }

    return ret;
}

static void init_spi_device(struct rt_device *dev, void *usr_data, char *dev_name)
{
    rt_err_t ret;

    dev->open       = spi_open;
    dev->init       = spi_init;
    dev->write      = spi_write;
    dev->close      = spi_close;
    dev->read       = spi_read;
    dev->control    = spi_control;
    dev->user_data  = usr_data;

    ret = rt_device_register(dev, dev_name, RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        __err("fatal error, ret %d, register device to rtsystem failure.\n", ret);
        while (1);
        return;
    }
}

const sunxi_hal_driver_spi_t sunxi_hal_spi_driver =
{
    .initialize     = hal_spi_init,
    .uninitialize   = hal_spi_deinit,
    .send           = hal_spi_write,
    .receive        = hal_spi_read,
    .hw_config      = hal_spi_hw_config,
    .transfer       = hal_spi_xfer,
};

int  sunxi_driver_spi_init(void)
{
    struct rt_device *device0, *device1, *device2, *device3;

    device0 = &spi0.base;
    spi0.dev_id = 0;
    spi0.hal_drv = &sunxi_hal_spi_driver;

    device1 = &spi1.base;
    spi1.dev_id = 1;
    spi1.hal_drv = &sunxi_hal_spi_driver;

    device2 = &spi2.base;
    spi2.dev_id = 2;
    spi2.hal_drv = &sunxi_hal_spi_driver;

    device3 = &spi3.base;
    spi3.dev_id = 3;
    spi3.hal_drv = &sunxi_hal_spi_driver;

    init_spi_device(device0, &spi0, "spi0");
    init_spi_device(device1, &spi1, "spi1");
    init_spi_device(device2, &spi2, "spi2");
    init_spi_device(device3, &spi3, "spi3");

    return 0;
}

late_initcall(sunxi_driver_spi_init);