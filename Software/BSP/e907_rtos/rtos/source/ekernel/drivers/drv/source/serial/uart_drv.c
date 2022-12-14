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
#include <hal_poll.h>
#include <hal_uart.h>
#include <sunxi_drv_uart.h>
#include <aw_list.h>
#include <rtthread.h>
#include <typedef.h>
#include <debug.h>
#include <log.h>


// bring hal layer in.
#ifdef CONFIG_SUNXI_UART_SUPPORT_POLL
static sunxi_hal_poll_ops uart_poll_ops =
{
    .check_poll_state = hal_uart_check_poll_state,
    .hal_poll_wakeup = hal_uart_poll_wakeup,
    .register_poll_wakeup = hal_uart_register_poll_wakeup,
};
#endif

static const sunxi_hal_driver_uart_t sunxi_hal_uart_driver =
{
    .get_version        = hal_uart_get_version,
    .get_capabilities   = hal_uart_get_capabilities,
    .initialize         = hal_uart_init,
    .uninitialize       = hal_uart_deinit,
    .power_control      = hal_uart_power_control,
    .send               = hal_uart_send,
    .receive            = hal_uart_receive,
    .transfer           = hal_uart_transfer,
    .get_tx_count       = hal_uart_get_tx_count,
    .get_rx_count       = hal_uart_get_rx_count,
    .control            = hal_uart_control,
    .get_status         = hal_uart_get_status,
    .set_modem_control  = hal_uart_set_modem_control,
    .get_modem_status   = hal_uart_get_modem_status,
    .receive_polling    = hal_uart_receive_polling,
#ifdef CONFIG_SUNXI_UART_SUPPORT_POLL
    .poll_ops           = &uart_poll_ops,
#endif
};

static sunxi_driver_uart_t uart0, uart1, uart2, uart3;
static sunxi_driver_uart_t uart4, uart5;

static rt_err_t sunxi_serial_init(struct rt_device *dev)
{
    rt_size_t ret = 0;
    sunxi_driver_uart_t *pusr, *pvfy;
    sunxi_hal_driver_uart_t *hal_drv = NULL;

    if (dev == NULL)
    {
        return -1;
    }

    pvfy = container_of(dev, sunxi_driver_uart_t, base);
    pusr = (sunxi_driver_uart_t *)dev->user_data;

    hal_assert(pvfy == pusr);

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_uart_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->initialize)
    {
        ret = hal_drv->initialize(pusr->dev_id);
    }
    return ret;
}

static rt_err_t sunxi_serial_open(struct rt_device *dev, rt_uint16_t oflag)
{
    return 0;
}

static rt_err_t sunxi_serial_close(struct rt_device *dev)
{
    rt_size_t ret = 0;

    sunxi_driver_uart_t *pusr, *pvfy;
    sunxi_hal_driver_uart_t *hal_drv = NULL;

    if (dev == NULL)
    {
        return -1;
    }

    pvfy = container_of(dev, sunxi_driver_uart_t, base);
    pusr = (sunxi_driver_uart_t *)dev->user_data;

    hal_assert(pvfy == pusr);

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_uart_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->uninitialize)
    {
        ret = hal_drv->uninitialize(pusr->dev_id);
    }

    return ret;
}

static rt_size_t sunxi_serial_read(struct rt_device *dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;
    sunxi_driver_uart_t *pusr, *pvfy;
    sunxi_hal_driver_uart_t *hal_drv = NULL;

    if (dev == NULL)
    {
        return -1;
    }

    pvfy = container_of(dev, sunxi_driver_uart_t, base);
    pusr = (sunxi_driver_uart_t *)dev->user_data;

    hal_assert(pvfy == pusr);

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_uart_t *)pusr->hal_drv;
    }

    extern int g_cli_direct_read;
    if (g_cli_direct_read == 1 && hal_drv && hal_drv->receive_polling)
    {
        ret = hal_drv->receive_polling(pusr->dev_id, buffer, size);
    }
    else if (hal_drv && hal_drv->receive)
    {
        ret = hal_drv->receive(pusr->dev_id, buffer, size);
    }

    return ret;
}

static rt_size_t sunxi_serial_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;

    sunxi_driver_uart_t *pusr, *pvfy;
    sunxi_hal_driver_uart_t *hal_drv = NULL;

    if (dev == NULL)
    {
        return -1;
    }

    pvfy = container_of(dev, sunxi_driver_uart_t, base);
    pusr = (sunxi_driver_uart_t *)dev->user_data;

    hal_assert(pvfy == pusr);

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_uart_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->send)
    {
        ret = hal_drv->send(pusr->dev_id, buffer, size);
    }

    return ret;
}

static rt_err_t sunxi_serial_control(struct rt_device *dev, int cmd, void *args)
{
    rt_size_t ret = 0;
    sunxi_driver_uart_t *pusr, *pvfy;
    sunxi_hal_driver_uart_t *hal_drv = NULL;

    if (dev == NULL)
    {
        return -1;
    }

    pvfy = container_of(dev, sunxi_driver_uart_t, base);
    pusr = (sunxi_driver_uart_t *)dev->user_data;

    hal_assert(pvfy == pusr);

    if (pusr)
    {
        hal_drv = (sunxi_hal_driver_uart_t *)pusr->hal_drv;
    }

    if (hal_drv && hal_drv->control)
    {
        ret = hal_drv->control(pusr->dev_id, cmd, args);
    }

    return ret;
}

#if defined(RT_USING_POSIX)
#include <dfs_file.h>
#include <rtdevice.h>
#include <dfs_poll.h>
#include <string.h>
#include <inttypes.h>

#define CHECK_POLL_OPS_FUNC_IS_VALAID(drv, func) \
	drv && drv->poll_ops && drv->poll_ops->func

static int32_t uart_wakeup_poll_waitqueue(int32_t dev_id, short key)
{
    rt_device_t dev;
    char dev_name [RT_NAME_MAX];

    memset(dev_name, 0, RT_NAME_MAX);
    sprintf(dev_name, "uart%"PRIu32, dev_id);
    dev = rt_device_find(dev_name);
    if (!dev)
    {
        return -EINVAL;
    }

    rt_wqueue_wakeup(&dev->wait_queue, (void *)(unsigned int)key);
    return 0;
}

static int sunxi_serial_fops_poll(struct dfs_fd *fd, struct rt_pollreq *req)
{
    rt_device_t dev;
    int mask = 0;
    short key;
    int ret = -1;
    sunxi_driver_uart_t *pusr, *pvfy;
    sunxi_hal_driver_uart_t *hal_drv = NULL;

    dev = (rt_device_t)fd->data;
    if (!dev)
    {
        return mask;
    }

    pvfy = container_of(dev, sunxi_driver_uart_t, base);
    pusr = (sunxi_driver_uart_t *)dev->user_data;

    hal_assert(pvfy == pusr);

    if (!pusr)
    {
        return mask;
    }

    hal_drv = (sunxi_hal_driver_uart_t *)pusr->hal_drv;
    key = req->_key;

    if (CHECK_POLL_OPS_FUNC_IS_VALAID(hal_drv, check_poll_state))
    {
        mask = hal_drv->poll_ops->check_poll_state(pusr->dev_id, key);
    }

    if (!mask && req->_proc != NULL)
    {
        if (CHECK_POLL_OPS_FUNC_IS_VALAID(hal_drv, register_poll_wakeup))
        {
            hal_drv->poll_ops->register_poll_wakeup(uart_wakeup_poll_waitqueue);
        }
        rt_poll_add(&dev->wait_queue, req);
    }
	/*
	 * TODO:
	 * Maybe it need to unregister poll_wakeup function, but need to take care of
	 * select the same fd as the same time
	 * */
    return mask;
}

static int sunxi_serial_fops_open(struct dfs_fd *fd)
{
    rt_device_t dev;
    dev = (rt_device_t) fd->data;

    return sunxi_serial_open(dev, 0);
}

static int sunxi_serial_fops_close(struct dfs_fd *fd)
{
    rt_device_t dev;
    dev = (rt_device_t) fd->data;

    return sunxi_serial_close(dev);
}

static int sunxi_serial_fops_ioctl(struct dfs_fd *fd, int cmd, void *args)
{
    rt_device_t dev;
    dev = (rt_device_t) fd->data;

    return sunxi_serial_control(dev, cmd, args);
}

static int sunxi_serial_fops_read(struct dfs_fd *fd, void *buf, size_t count)
{
    rt_device_t dev;
    dev = (rt_device_t) fd->data;

    return sunxi_serial_read(dev, fd->pos, buf, count);
}

static int sunxi_serial_fops_write(struct dfs_fd *fd, const void *buf, size_t count)
{
    rt_device_t dev;
    dev = (rt_device_t) fd->data;

    return sunxi_serial_write(dev, fd->pos, buf, count);
}

static int sunxi_serial_fops_flush(struct dfs_fd *fd)
{
    return 0;
}

static int sunxi_serial_fops_lseek(struct dfs_fd *fd, off_t offset)
{
    return 0;
}

static int sunxi_serial_fops_getdents(struct dfs_fd *fd, struct dirent *dirp, uint32_t count)
{
    return 0;
}

static struct dfs_file_ops uart_device_fops =
{
    .open       = sunxi_serial_fops_open,
    .close      = sunxi_serial_fops_close,
    .flush      = sunxi_serial_fops_flush,
    .ioctl      = sunxi_serial_fops_ioctl,
    .lseek      = sunxi_serial_fops_lseek,
    .read       = sunxi_serial_fops_read,
    .write      = sunxi_serial_fops_write,
    .getdents   = sunxi_serial_fops_getdents,
    .poll       = sunxi_serial_fops_poll,
    .mmap       = NULL,
};
#endif

static void init_uart_device(struct rt_device *dev, void *usr_data, char *dev_name)
{
    rt_err_t ret;

    dev->init        = sunxi_serial_init;
    dev->open        = sunxi_serial_open;
    dev->close       = sunxi_serial_close;
    dev->read        = sunxi_serial_read;
    dev->write       = sunxi_serial_write;
    dev->control     = sunxi_serial_control;
    dev->user_data   = usr_data;

    ret = rt_device_register(dev, dev_name, RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        __err("fatal error, ret %d, register device to rtsystem failure.\n", ret);
        software_break();
        return;
    }

#if defined(RT_USING_POSIX)
    dev->fops = &uart_device_fops;
#endif
}

void sunxi_driver_uart_init(void)
{
    struct rt_device *device0, *device1, *device2, *device3, *device4, *device5;

    device0 = &uart0.base;
    uart0.dev_id = 0;
    uart0.hal_drv = &sunxi_hal_uart_driver;

    device1 = &uart1.base;
    uart1.dev_id = 1;
    uart1.hal_drv = &sunxi_hal_uart_driver;

    device2 = &uart2.base;
    uart2.dev_id = 2;
    uart2.hal_drv = &sunxi_hal_uart_driver;

    device3 = &uart3.base;
    uart3.dev_id = 3;
    uart3.hal_drv = &sunxi_hal_uart_driver;

    device4 = &uart4.base;
    uart4.dev_id = 4;
    uart4.hal_drv = &sunxi_hal_uart_driver;

    device5 = &uart5.base;
    uart5.dev_id = 5;
    uart5.hal_drv = &sunxi_hal_uart_driver;

    init_uart_device(device0, &uart0, "uart0");
    init_uart_device(device1, &uart1, "uart1");
    init_uart_device(device2, &uart2, "uart2");
    init_uart_device(device3, &uart3, "uart3");
    init_uart_device(device4, &uart4, "uart4");
    init_uart_device(device5, &uart5, "uart5");

    return;
}