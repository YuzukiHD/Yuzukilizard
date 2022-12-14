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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <rtthread.h>

#include <pthread.h>

#include <init.h>

#include <sunxi_hal_spinor.h>
#include <blkpart.h>

extern int register_part(rt_device_t dev, struct part *part);

static char *ramfs_buffer = NULL;
static struct blkpart ram_blk;
static pthread_mutex_t ram_dev_mutex;

static rt_err_t sunxi_ramfs_dev_init(rt_device_t dev)
{
    return 0;
}

static rt_err_t sunxi_ramfs_dev_open(rt_device_t dev, rt_uint16_t oflag)
{
    return 0;
}

static rt_err_t sunxi_ramfs_dev_close(rt_device_t dev)
{
    return 0;
}

static rt_size_t sunxi_ramfs_dev_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;

    pthread_mutex_lock(&ram_dev_mutex);

    if (ramfs_buffer)
    {
        if (pos + size > CONFIG_RAMFS_DEVICE_SIZE)
        {
            size = CONFIG_RAMFS_DEVICE_SIZE - pos;
        }
        memcpy(buffer, ramfs_buffer + pos, size);
        ret = size;
    }

    pthread_mutex_unlock(&ram_dev_mutex);

    return ret;
}

static rt_size_t sunxi_ramfs_dev_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_size_t ret = 0;

    pthread_mutex_lock(&ram_dev_mutex);

    if (ramfs_buffer)
    {
        if (pos + size > CONFIG_RAMFS_DEVICE_SIZE)
        {
            size = CONFIG_RAMFS_DEVICE_SIZE - pos;
        }
        memcpy(ramfs_buffer + pos, buffer, size);
        ret = size;
    }

    pthread_mutex_unlock(&ram_dev_mutex);
    return ret;
}

static rt_err_t sunxi_ramfs_dev_control(rt_device_t dev, int cmd, void *args)
{
    int ret = 0;
    blk_dev_erase_t *erase_sector;
    struct rt_device_blk_geometry *geometry;
    uint32_t size = 0;;

    if (!dev)
    {
        return -EINVAL;
    }

    pthread_mutex_lock(&ram_dev_mutex);

    switch (cmd)
    {
        case BLOCK_DEVICE_CMD_ERASE_ALL:
            if (ramfs_buffer)
            {
                memset(ramfs_buffer, 0xff, CONFIG_RAMFS_DEVICE_SIZE);
            }
            break;
        case BLOCK_DEVICE_CMD_ERASE_SECTOR:
            erase_sector = (blk_dev_erase_t *)(args);
            if (ramfs_buffer)
            {
                if ((erase_sector->addr + erase_sector->len) > CONFIG_RAMFS_DEVICE_SIZE)
                {
                    size = CONFIG_RAMFS_DEVICE_SIZE - erase_sector->len;
                }
                else
                {
                    size = erase_sector->len;
                }
                memset(ramfs_buffer + erase_sector->addr, 0xff, erase_sector->len);
            }
            break;
        case BLOCK_DEVICE_CMD_GET_TOTAL_SIZE:
            *(uint32_t *)args = CONFIG_RAMFS_DEVICE_SIZE;
            break;
        case BLOCK_DEVICE_CMD_GET_PAGE_SIZE:
            *(uint32_t *)args = 256;
            break;
        case BLOCK_DEVICE_CMD_GET_BLOCK_SIZE:
            *(uint32_t *)args = 4096;
            break;
        case RT_DEVICE_CTRL_BLK_GETGEOME:
            geometry = (struct rt_device_blk_geometry *)args;
            memset(geometry, 0, sizeof(struct rt_device_blk_geometry));
            geometry->block_size = 4096;
            geometry->bytes_per_sector = 512;
            geometry->sector_count = CONFIG_RAMFS_DEVICE_SIZE / geometry->bytes_per_sector;
            break;
        default:
            break;
    }

    pthread_mutex_unlock(&ram_dev_mutex);
    return ret;
}

#if defined(RT_USING_POSIX)
#include <dfs_file.h>
#include <rtdevice.h>

static int sunxi_ramfs_dev_fops_open(struct dfs_fd *fd)
{
    rt_device_t dev;
    uint32_t total_bytes = 0;

    dev = (rt_device_t) fd->data;
    dev->control(dev, BLOCK_DEVICE_CMD_GET_TOTAL_SIZE, &total_bytes);

    fd->size = total_bytes;
    fd->type  = FT_DEVICE;

    return sunxi_ramfs_dev_open(dev, 0);
}

static int sunxi_ramfs_dev_fops_close(struct dfs_fd *fd)
{
    rt_device_t dev;
    dev = (rt_device_t) fd->data;

    return sunxi_ramfs_dev_close(dev);
}

static int sunxi_ramfs_dev_fops_ioctl(struct dfs_fd *fd, int cmd, void *args)
{
    rt_device_t dev;
    dev = (rt_device_t) fd->data;

    return sunxi_ramfs_dev_control(dev, cmd, args);
}

static int sunxi_ramfs_dev_fops_read(struct dfs_fd *fd, void *buf, size_t count)
{
    rt_device_t dev;
    int ret = -1;
    dev = (rt_device_t) fd->data;

    ret = dev->read(dev, fd->pos, buf, count);
    if (ret >= 0)
    {
        fd->pos += ret;
    }

    return ret;
}

static int sunxi_ramfs_dev_fops_write(struct dfs_fd *fd, const void *buf, size_t count)
{
    rt_device_t dev;
    int ret = -1;
    dev = (rt_device_t) fd->data;

    ret = dev->write(dev, fd->pos, buf, count);
    if (ret >= 0)
    {
        fd->pos += ret;
    }

    return ret;
}

static int sunxi_ramfs_dev_fops_lseek(struct dfs_fd *fd, off_t offset)
{
    int ret = -1;

    if (fd->flags & O_DIRECTORY)
    {
        ret = -1;
    }
    else
    {
        if (offset > fd->size)
        {
            ret = -1;
        }
        else
        {
            fd->pos = offset;
            ret = fd->pos;
        }
    }
    return ret;
}

static struct dfs_file_ops ramfs_dev_device_fops =
{
    .open = sunxi_ramfs_dev_fops_open,
    .close = sunxi_ramfs_dev_fops_close,
    .ioctl = sunxi_ramfs_dev_fops_ioctl,
    .lseek = sunxi_ramfs_dev_fops_lseek,
    .read = sunxi_ramfs_dev_fops_read,
    .write = sunxi_ramfs_dev_fops_write,
};
#endif

int sunxi_ramfs_init(void)
{
    int ret = -1;
    int i = 0;
    struct rt_device *device;
    struct part *part;

    memset(&ram_blk, 0, sizeof(struct blkpart));

    ramfs_buffer = malloc(CONFIG_RAMFS_DEVICE_SIZE);
    if (!ramfs_buffer)
    {
        return -1;
    }

    ret = pthread_mutex_init(&ram_dev_mutex, NULL);
    if (ret)
    {
        pr_err("%s, %d, mutex init failed!\n", __func__, __LINE__);
        free(ramfs_buffer);
        ramfs_buffer = NULL;
        goto err;
    }

    device = rt_device_create(RT_Device_Class_Block, 0);
    if (!device)
    {
        free(ramfs_buffer);
        ramfs_buffer = NULL;
        pthread_mutex_destroy(&ram_dev_mutex);
        return ret;
    }

    device->init = sunxi_ramfs_dev_init;
    device->open = sunxi_ramfs_dev_open;
    device->close = sunxi_ramfs_dev_close;
    device->read = sunxi_ramfs_dev_read;
    device->write = sunxi_ramfs_dev_write;
    device->control = sunxi_ramfs_dev_control;
    device->user_data = NULL;

    ret = rt_device_register(device, CONFIG_RAMFS_DEVICE_NAME, RT_DEVICE_FLAG_RDWR);
    if (ret != RT_EOK)
    {
        rt_device_destroy(device);
        free(ramfs_buffer);
        ramfs_buffer = NULL;
        pthread_mutex_destroy(&ram_dev_mutex);
        goto err;
    }

#if defined(RT_USING_POSIX)
    device->fops = &ramfs_dev_device_fops;
#endif

    device->control(device, BLOCK_DEVICE_CMD_GET_PAGE_SIZE, &ram_blk.page_bytes);
    device->control(device, BLOCK_DEVICE_CMD_GET_TOTAL_SIZE, &ram_blk.total_bytes);
    device->control(device, BLOCK_DEVICE_CMD_GET_BLOCK_SIZE, &ram_blk.blk_bytes);
    ram_blk.dev = device;

    ram_blk.n_parts = 1;
    ram_blk.parts = malloc(sizeof(struct part) * ram_blk.n_parts);
    if (ram_blk.parts == NULL)
    {
        pr_err("allocate part array failed.\n");
        rt_device_destroy(device);
        free(ramfs_buffer);
        ramfs_buffer = NULL;
        pthread_mutex_destroy(&ram_dev_mutex);
        ret = -1;
        goto err;
    }
    memset(ram_blk.parts, 0, sizeof(struct part) * ram_blk.n_parts);

    for (i = 0; i < ram_blk.n_parts; i++)
    {
        part = &ram_blk.parts[i];
        part->blk = &ram_blk;
        part->erase_flag = 0;
        part->off = 0;
        part->bytes = ram_blk.total_bytes;
        snprintf(part->name, MAX_BLKNAME_LEN, "%s%d", CONFIG_RAMFS_DEVICE_NAME, i);
    }

    for (i = 0; i < ram_blk.n_parts; i++)
    {
        part = &ram_blk.parts[i];
        if (part->bytes % ram_blk.blk_bytes)
        {
            pr_err("part %s with bytes %u should align to block size %u\n",
                   part->name, part->bytes, ram_blk.blk_bytes);
        }
        else
        {
            register_part(device, part);
        }
    }

    blkpart_add_list(&ram_blk);
    ret = 0;
err:
    return ret;
}

device_initcall(sunxi_ramfs_init);