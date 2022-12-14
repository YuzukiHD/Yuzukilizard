#include <rtthread.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>
#include <init.h>
#include <kapi.h>
#include <libc.h>

#include <log.h>
#include <drivers/sys_device.h>

#include <inttypes.h>

#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
#include "cache/cache_fs.h"
#endif

__u32 fsys_init(void);
__s32 dev_init(void);
__s32 dev_exit(void);
__u32 FSYS_Exit(void);
int epdk_flag_to_mode_trans(char **p_mode, __u32 mode);

#ifndef CONFIG_LAYERFS_DEVICE_NAME_LEN
#define CONFIG_LAYERFS_DEVICE_NAME_LEN 16
#endif

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define ALIGN_DOWN(x, a) __ALIGN_KERNEL((x) - ((a) - 1), (a))
#define __ALIGN_KERNEL(x, a) __ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask) (((x) + (mask)) & ~(mask))

static int dfs_path_to_melis_path(char **real_path, char letter, const char *dfs_path)
{
    char *vfs_path;
    int vfs_path_pos = 0;
    int dfs_path_pos = 0;
    int size = 0;

    size = dfs_path == NULL ? 0 : strlen(dfs_path);
    size += 4;

    vfs_path = malloc(size);
    if (!vfs_path)
    {
        return -1;
    }
    memset(vfs_path, 0, size);

    if (letter >= 'A' && letter <= 'Z')
    {
        vfs_path[0] = letter;
        vfs_path[1] = ':';
        vfs_path[2] = '\\';

        vfs_path_pos = 3;
        dfs_path_pos = 0;

        size -= 4;
        while (size-- > 0 && dfs_path != NULL)
        {
            if (dfs_path[dfs_path_pos] == '/')
            {
                vfs_path[vfs_path_pos] = '\\';
            }
            else
            {
                vfs_path[vfs_path_pos] = dfs_path[dfs_path_pos];
            }
            vfs_path_pos++;
            dfs_path_pos++;
        }
        *real_path = vfs_path;
        return 0;
    }
    else
    {
        free(vfs_path);
        return -1;
    }
}

static time_t melis_fstat_time_to_libc_time(ES_TIMESTAMP *timestamp)
{
    struct tm tm;

    memset(&tm, 0, sizeof(struct tm));
    tm.tm_year = timestamp->year - 1900;
    tm.tm_mon = timestamp->month - 1;
    tm.tm_mday = timestamp->day;
    tm.tm_hour = timestamp->hour;
    tm.tm_min = timestamp->minute;
    tm.tm_sec = timestamp->second;

    return mktime(&tm);
}

static void melis_fstat_to_libc_stat(__ES_FSTAT *fstat, struct stat *st)
{
    memset(st, 0, sizeof(struct stat));

    st->st_size = fstat->size;
    st->st_nlink = fstat->nlink;
    st->st_blksize = fstat->blksize;
    st->st_blocks = fstat->blocks;
    st->st_mode = S_ISDIR(fstat->mode) ? S_IFDIR : S_IFREG;

    st->st_mtime = melis_fstat_time_to_libc_time(&fstat->mtime);
    st->st_atime = melis_fstat_time_to_libc_time(&fstat->atime);
    st->st_ctime = melis_fstat_time_to_libc_time(&fstat->ctime);
}

static rt_err_t sunxi_layerfs_device_init(rt_device_t dev)
{
    return 0;
}

static rt_size_t sunxi_layerfs_device_read(rt_device_t dev, rt_off_t offset, void *data, rt_size_t size)
{
    ssize_t ret, sz = 0;
    __hdle pPart = NULL;
    char *page_buf = NULL;
    uint64_t total_bytes = 0;
    __dev_blkinfo_t info;
    int sector_size;
    char *dname = NULL;
    char letter = 0;
    int readN = 0;

    pPart = dev->user_data;
    if (!pPart)
    {
        return -1;
    }

    if (size == 0)
    {
        return 0;
    }

    memset(&info, 0, sizeof(info));

    esFSYS_pioctrl(pPart, FSYS_PART_CMD_GET_INFO, 0, &info);
    esFSYS_pioctrl(pPart, PART_IOC_SYS_GETNAME, 0, &dname);
    esFSYS_pioctrl(pPart, PART_IOC_SYS_GETLETTER, 0, &letter);

    total_bytes = info.partsize * info.secsize;
    sector_size = info.secsize;

    if (offset >= total_bytes)
    {
        printf("read offset %ld over part size %"PRIu64"\n", offset, total_bytes);
        return 0;
    }

    if (offset + size > total_bytes)
    {
        printf("read %s(%c) over limit: offset %ld + size %lu over %"PRIu64"\n",
               dname, letter, offset, size, total_bytes);
    }

    size = MIN(total_bytes - offset, size);
    pr_debug("read %s(%c) off %ld size %lu\n", dname, letter,
             offset, size);

    if (offset % sector_size || size % sector_size)
    {
        page_buf = malloc(sector_size);
        if (!page_buf)
        {
            return -ENOMEM;
        }
        memset(page_buf, 0, sector_size);
    }

    /**
     * Step 1:
     * read the beginning data that not align to block size
     */
    if (offset % sector_size)
    {
        uint64_t addr, poff, len;

        addr = ALIGN_DOWN(offset, sector_size);
        poff = offset - addr;
        len = MIN(sector_size - poff, size);

        pr_debug("offset %ld not align %u, fix them before align read\n",
                 offset, sector_size);
        pr_debug("step1: read page data from addr %"PRIu64"\n", addr);
        ret = esFSYS_pread(page_buf, addr / sector_size, 1, pPart);
        if (ret != 1)
        {
            goto err;
        }

        pr_debug("step2: copy page data to buf with page offset %"PRIu64" and len %"PRIu64"\n",
                 poff, len);
        memcpy(data, page_buf + poff, len);

        offset += len;
        data += len;
        sz += len;
        size -= len;
    }

    /**
     * Step 2:
     * read data that align to block size
     */
    readN = size / sector_size;
    ret = esFSYS_pread(data, offset / sector_size, readN, pPart);
    if (ret != readN)
    {
        goto err;
    }

    offset += sector_size * readN;
    data += sector_size * readN;
    sz += sector_size * readN;
    size -= sector_size * readN;

    /**
     * Step 3:
     * read the last data that not align to block size
     */
    if (size)
    {
        pr_debug("last size %lu not align %d, read them\n", size, sector_size);

        pr_debug("step1: read page data from addr %ld\n", offset);
        ret = esFSYS_pread(page_buf, offset / sector_size, 1, pPart);
        if (ret != 1)
        {
            goto err;
        }

        pr_debug("step2: copy page data to buf with page with len %ld\n", size);
        memcpy(data, page_buf, size);
        sz += size;
    }

#ifdef DEBUG
    pr_debug("read data:\n");
    hexdump(data, sz);
#endif
    ret = 0;
    goto out;

err:
    pr_err("read failed - %d\n", (int)ret);
out:
    if (page_buf)
    {
        free(page_buf);
    }
    return ret ? ret : sz;
}

static rt_err_t sunxi_layerfs_device_open(rt_device_t dev, rt_uint16_t oflag)
{
    rt_err_t ret = EPDK_OK;
    __hdle pPart = NULL;
    char *partname = NULL;
    char letter = dev->parent.name[0];

    ret = dfs_path_to_melis_path(&partname, letter, NULL);
    if (ret)
    {
        return -1;
    }

    pPart = esFSYS_popen(partname, "r+");
    if (!pPart)
    {
        __err("failed!\npart %s can not be open!\n", partname);
        ret = EPDK_FAIL;
        goto out;
    }
    dev->user_data = pPart;
out:
    return ret;
}

static rt_err_t sunxi_layerfs_device_close(rt_device_t dev)
{
    __hdle pPart = NULL;

    pPart = dev->user_data;
    if (pPart)
    {
        esFSYS_pclose(pPart);
    }
    dev->user_data = NULL;
    return 0;
}

static rt_size_t sunxi_layerfs_device_write(rt_device_t dev, rt_off_t offset, const void *data, rt_size_t size)
{
    ssize_t ret, sz = 0;
    __hdle pPart = NULL;
    char *page_buf = NULL;
    uint64_t total_bytes = 0;
    __dev_blkinfo_t info;
    int sector_size;
    char *dname = NULL;
    char letter = 0;
    int writeN = 0;

    pPart = dev->user_data;
    if (!pPart)
    {
        return -1;
    }

    if (size == 0)
    {
        return 0;
    }

    memset(&info, 0, sizeof(info));

    esFSYS_pioctrl(pPart, FSYS_PART_CMD_GET_INFO, 0, &info);
    esFSYS_pioctrl(pPart, PART_IOC_SYS_GETNAME, 0, &dname);
    esFSYS_pioctrl(pPart, PART_IOC_SYS_GETLETTER, 0, &letter);

    total_bytes = info.partsize * info.secsize;
    sector_size = info.secsize;

    if (offset >= total_bytes)
    {
        printf("read offset %ld over part size %"PRIu64"\n", offset, total_bytes);
        return 0;
    }

    if (offset + size > total_bytes)
    {
        printf("read %s(%c) over limit: offset %ld + size %lu over %"PRIu64"\n",
               dname, letter, offset, size, total_bytes);
    }

    size = MIN(total_bytes - offset, size);
    pr_debug("read %s(%c) off %ld size %lu\n", dname, letter,
             offset, size);

    if (offset % sector_size || size % sector_size)
    {
        page_buf = malloc(sector_size);
        if (!page_buf)
        {
            return -ENOMEM;
        }
        memset(page_buf, 0, sector_size);
    }

    /**
     * Step 1:
     * read the beginning data that not align to block size
     */
    if (offset % sector_size)
    {
        uint64_t addr, poff, len;

        addr = ALIGN_DOWN(offset, sector_size);
        poff = offset - addr;
        len = MIN(sector_size - poff, size);

        pr_debug("offset %ld not align %d, fix them before align read\n",
                 offset, sector_size);
        pr_debug("step1: read page data from addr %"PRIu64"\n", addr);
        ret = esFSYS_pread(page_buf, addr / sector_size, 1, pPart);
        if (ret != 1)
        {
            goto err;
        }

        pr_debug("step2: copy page data to buf with page offset %"PRIu64" and len %"PRIu64"\n",
                 poff, len);
        memcpy(page_buf + poff, data, len);

        pr_debug("step3: flush the fixed page data\n");
        ret = esFSYS_pwrite(page_buf, addr / sector_size, 1, pPart);
        if (ret != 1)
        {
            goto err;
        }

        offset += len;
        data += len;
        sz += len;
        size -= len;
    }

    /**
     * Step 2:
     * read data that align to block size
     */
    writeN = size / sector_size;
    ret = esFSYS_pwrite(data, offset / sector_size, writeN, pPart);
    if (ret != writeN)
    {
        goto err;
    }

    offset += sector_size * writeN;
    data += sector_size * writeN;
    sz += sector_size * writeN;
    size -= sector_size * writeN;

    /**
     * Step 3:
     * read the last data that not align to block size
     */
    if (size)
    {
        pr_debug("last size %lu not align %d, read them\n", size, sector_size);

        pr_debug("step1: read page data from addr %ld\n", offset);
        ret = esFSYS_pread(page_buf, offset / sector_size, 1, pPart);
        if (ret != 1)
        {
            goto err;
        }

        pr_debug("step2: copy buf to page data with page with len %d\n", size);
        memcpy(page_buf, data, size);

        pr_debug("step3: flush the fixed page data\n");
        ret = esFSYS_pwrite(page_buf, offset / sector_size, 1, pPart);
        if (ret != 1)
        {
            goto err;
        }

        sz += size;
    }

#ifdef DEBUG
    pr_debug("read data:\n");
    hexdump(data, sz);
#endif
    ret = 0;
    goto out;

err:
    pr_err("read failed - %d\n", (int)ret);
out:
    if (page_buf)
    {
        free(page_buf);
    }
    return ret ? ret : sz;
}

static rt_err_t sunxi_layerfs_device_control(rt_device_t dev, int cmd, void *args)
{
    return 0;
}

int register_layerfs_part_device(char letter)
{
    int ret = -1;
    struct rt_device *device;
    char device_name[CONFIG_LAYERFS_DEVICE_NAME_LEN] = {0};

    snprintf(device_name, CONFIG_LAYERFS_DEVICE_NAME_LEN, "%c", letter);

    device = rt_device_create(RT_Device_Class_Block, 0);
    if (!device)
    {
        return ret;
    }

    device->init = sunxi_layerfs_device_init;
    device->open = sunxi_layerfs_device_open;
    device->close = sunxi_layerfs_device_close;
    device->read = sunxi_layerfs_device_read;
    device->write = sunxi_layerfs_device_write;
    device->control = sunxi_layerfs_device_control;
    device->user_data = (void *)NULL;

    ret = rt_device_register(device, device_name, RT_DEVICE_FLAG_RDWR);
    if (ret != RT_EOK)
    {
        rt_device_destroy(device);
    }
    return ret;
}

int unregister_layerfs_part_device(char letter)
{
    int ret = -1;
    struct rt_device *device;
    char device_name[CONFIG_LAYERFS_DEVICE_NAME_LEN] = {0};

    snprintf(device_name, CONFIG_LAYERFS_DEVICE_NAME_LEN, "%c", letter);

    device = rt_device_find(device_name);
    if (device)
    {
        rt_device_unregister(device);
        rt_device_destroy(device);
        ret = 0;
    }
    return ret;
}

static int melis_vfs_mkfs(rt_device_t dev, char *fs_type)
{
    rt_err_t ret = EPDK_OK;
    __hdle pPart = NULL;
    char *partname = NULL;
    char letter = dev->parent.name[0];

    ret = dfs_path_to_melis_path(&partname, letter, NULL);
    if (ret)
    {
        return -1;
    }

    ret = esFSYS_format(partname, fs_type, 0);
out:
    return ret;
}

static int melis_vfs_mount(struct dfs_filesystem *fs,
                           unsigned long          rwflag,
                           const void            *data)
{
    fs->data = (void *)data;
    return RT_EOK;
}

static int melis_vfs_unmount(struct dfs_filesystem *fs)
{
    fs->data = NULL;
    return RT_EOK;
}

static int melis_vfs_statfs(struct dfs_filesystem *fs, struct statfs *buf)
{
    char letter;
    char *real_path = NULL;
    __ES_FSSTAT statfs_buf;
    int ret = -1;

    RT_ASSERT(buf != NULL);
    RT_ASSERT(fs != NULL);

    letter = (char)((unsigned long)fs->data);

    ret = dfs_path_to_melis_path(&real_path, letter, NULL);
    if (ret)
    {
        return -1;
    }

    memset(&statfs_buf, 0, sizeof(__ES_FSSTAT));
    memset(buf, 0, sizeof(struct statfs));

    ret = esFSYS_statfs(real_path, &statfs_buf, FSYS_KSTATUS_ALL);
    free(real_path);
    if (ret)
    {
        return -1;
    }

    buf->f_bfree = statfs_buf.f_bfree;
    buf->f_bsize = statfs_buf.f_bsize;
    buf->f_blocks = statfs_buf.f_blocks;

    return RT_EOK;
}


static int melis_vfs_stat(struct dfs_filesystem *fs,
                          const char            *path,
                          struct stat           *st)
{
    __ES_FSTAT  fstat;
    __hdle      fp;
    int ret = 0;
    char *real_path = NULL;
    char letter = (char)((unsigned long)fs->data);

    ret = dfs_path_to_melis_path(&real_path, letter, path);
    if (ret)
    {
        return -1;
    }

    memset(&fstat, 0, sizeof(__ES_FSTAT));
    memset(st, 0, sizeof(struct stat));

    fp = esFSYS_fopen(real_path, "rb");
    free(real_path);
    if (!fp)
    {
        return -1;
    }

    if (EPDK_OK != esFSYS_fstat(fp, &fstat))
    {
        esFSYS_fclose(fp);
        return -1;
    }

    esFSYS_fclose(fp);

    melis_fstat_to_libc_stat(&fstat, st);

    return RT_EOK;
}


static int melis_vfs_open(struct dfs_fd *file)
{
    __ES_FSTAT  fstat;
    __hdle      fp;
    int ret = 0;
    char *real_path = NULL;
    char letter = (char)((unsigned long)file->fs->data);

    ret = dfs_path_to_melis_path(&real_path, letter, file->path);
    if (ret)
    {
        return -1;
    }

    if (file->flags & O_DIRECTORY)
    {
        if (file->flags & O_CREAT)
        {
            if (esFSYS_mkdir(real_path) != EPDK_OK)
            {
                free(real_path);
                goto _error_dir;
            }
        }

        fp = esFSYS_opendir(real_path);
        free(real_path);
        if (fp == NULL)
        {
            goto _error_dir;
        }
        else
        {
            file->data = (void *)fp;
            return RT_EOK;
        }

_error_dir:
        return -EIO;;
    }
    else
    {
        char *mode;
        if (epdk_flag_to_mode_trans(&mode, file->flags))
        {
            goto _error_file;
        }

#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
        fp = CACHE_fopen(real_path, mode, file->flags);
#else
        fp = esFSYS_fopen(real_path, mode);
#endif
        free(real_path);
        if (fp == NULL)
        {
            goto _error_file;
        }
        else
        {
            file->data = (void *)fp;
            memset(&fstat, 0, sizeof(__ES_FSTAT));

#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
            if (EPDK_OK == CACHE_fstat(fp, &fstat))
#else
            if (EPDK_OK == esFSYS_fstat(fp, &fstat))
#endif
            {
                file->size = fstat.size;
                file->pos = fstat.pos;
                return RT_EOK;
            }
            else
            {
#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
                CACHE_fclose(fp);
#else
                esFSYS_fclose(fp);
#endif
                return -EIO;
            }
        }

_error_file:
        return -EIO;;
    }

    return 0;
}

static int melis_vfs_close(struct dfs_fd *file)
{
    __hdle fp;
    int ret = 0;

    fp = file->data;
    if (file->flags & O_DIRECTORY)
    {
        ret = esFSYS_closedir(fp);
    }
    else
    {
#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
        ret = CACHE_fclose(fp);
#else
        ret = esFSYS_fclose(fp);
#endif
    }

    return ret;
}

static int melis_vfs_ioctl(struct dfs_fd *file, int cmd, void *args)
{
    __hdle  fp;
    int ret = 0;

    fp = file->data;
    if (file->flags & O_DIRECTORY)
    {
        ret = -1;
    }
    else
    {
#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
        ret = CACHE_fioctrl(fp, cmd, 0, args);
#else
        ret = esFSYS_fioctrl(fp, cmd, 0, args);
#endif
    }
    return ret;

}

static int melis_vfs_read(struct dfs_fd *file, void *buf, size_t count)
{
    __hdle fp;
    int ret = 0;

    fp = file->data;
    if (file->flags & O_DIRECTORY)
    {
        ret = -1;
    }
    else
    {
#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
        ret = CACHE_fread(buf, 1, count, fp);
#else
        ret = esFSYS_fread(buf, 1, count, fp);
#endif
        if (ret >= 0)
        {
            __ES_FSTAT  fstat;
            memset(&fstat, 0, sizeof(__ES_FSTAT));

#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
            if (EPDK_OK != CACHE_fstat(fp, &fstat))
#else
            if (EPDK_OK != esFSYS_fstat(fp, &fstat))
#endif
            {
                __err("stat file failed!");
                goto out;
            }

            file->pos = fstat.pos;
            file->size = fstat.size;
        }
        else
        {
            ret = -1;
            __err("read file failed");
        }
    }
out:
    return ret;
}

static int melis_vfs_write(struct dfs_fd *file, const void *buf, size_t count)
{
    __hdle  fp;
    int ret = 0;

    fp = file->data;
    if (file->flags & O_DIRECTORY)
    {
        ret = -1;
    }
    else
    {
#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
        ret = CACHE_fwrite((void *)buf, 1, count, fp);
#else
        ret = esFSYS_fwrite((void *)buf, 1, count, fp);
#endif
        if (ret >= 0)
        {
            __ES_FSTAT  fstat;
            memset(&fstat, 0, sizeof(__ES_FSTAT));

#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
            if (EPDK_OK != CACHE_fstat(fp, &fstat))
#else
            if (EPDK_OK != esFSYS_fstat(fp, &fstat))
#endif
            {
                __err("stat file failed!");
                goto out;
            }

            file->pos = fstat.pos;
            file->size = fstat.size;
        }
        else
        {
            ret = -1;
            __err("read file failed\n");
        }
    }
out:
    return ret;
}

static _off64_t melis_vfs_lseek64(struct dfs_fd *file, _off64_t offset)
{
    __hdle  fp;
    int ret = 0;

    fp = file->data;
    if (file->flags & O_DIRECTORY)
    {
        ret = -1;
    }
    else
    {
#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
        ret = CACHE_fseek(fp, offset, 0);
#else
        ret = esFSYS_fseekex(fp, (__s32)(offset & 0xffffffffLL), (__s32)(offset >> 32), 0);
#endif
        if (ret >= 0)
        {
            __ES_FSTAT  fstat;
            memset(&fstat, 0, sizeof(__ES_FSTAT));

#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
            if (EPDK_OK != CACHE_fstat(fp, &fstat))
#else
            if (EPDK_OK != esFSYS_fstat(fp, &fstat))
#endif
            {
                __err("stat file failed!");
                return -1;
            }

            file->pos = fstat.pos;
            file->size = fstat.size;
        }
        else
        {
            __err("fseek file failed\n");
            return -1;
        }
    }
    return offset;
}

static int melis_vfs_lseek(struct dfs_fd *file, off_t offset)
{
    __hdle  fp;
    int ret = 0;

    fp = file->data;
    if (file->flags & O_DIRECTORY)
    {
        ret = -1;
    }
    else
    {
#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
        ret = CACHE_fseek(fp, offset & 0xffffffffLL, 0);
#else
        ret = esFSYS_fseek(fp, offset & 0xffffffffL, 0);
#endif
        if (ret >= 0)
        {
            __ES_FSTAT  fstat;
            memset(&fstat, 0, sizeof(__ES_FSTAT));

#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
            if (EPDK_OK != CACHE_fstat(fp, &fstat))
#else
            if (EPDK_OK != esFSYS_fstat(fp, &fstat))
#endif
            {
                __err("stat file failed!");
                return -1;
            }

            file->pos = fstat.pos;
            file->size = fstat.size;
        }
        else
        {
            __err("fseek file failed\n");
            return -1;
        }
    }
    return offset;
}

static int melis_vfs_flush(struct dfs_fd *file)
{
    __hdle  fp;
    int ret = 0;

    fp = file->data;
    if (file->flags & O_DIRECTORY)
    {
        return -1;
    }
    else
    {
#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
        ret = CACHE_fsync(fp);
#else
        ret = esFSYS_fsync(fp);
#endif
    }
    return ret;
}

static int melis_vfs_getdents(struct dfs_fd *file, struct dirent *dirp, uint32_t count)
{
    ES_DIR *dir;
    ES_DIRENT   *direntp;
    int index;
    struct dirent *d;

    RT_ASSERT(file->data != RT_NULL);

    dir = (ES_DIR *)(file->data);

    /* make integer count */
    count = (count / sizeof(struct dirent)) * sizeof(struct dirent);
    if (count == 0)
    {
        return -EINVAL;
    }

    index = 0;
    while (1)
    {
        d = dirp + index;

        direntp = esFSYS_readdir(dir);
        if (direntp == NULL)
        {
            break;
        }

        if (strcmp(direntp->d_name, ".") == 0)
        {
            continue;
        }
        else if (strcmp(direntp->d_name, "..") == 0)
        {
            continue;
        }

        d->d_type |= direntp->fatdirattr & FSYS_ATTR_DIRECTORY ? DT_DIR : DT_REG;

        d->d_namlen = (rt_uint8_t)strlen(direntp->d_name);
        d->d_reclen = (rt_uint16_t)sizeof(struct dirent);
        strncpy(d->d_name, direntp->d_name, strlen(direntp->d_name) + 1);

        index++;
        if (index * sizeof(struct dirent) >= count)
        {
            break;
        }
    }

    if (index == 0)
    {
        return 0;
    }

    file->pos += index * sizeof(struct dirent);

    return index * sizeof(struct dirent);
}

static int melis_vfs_unlink(struct dfs_filesystem *dfs, const char *path)
{
    int ret = 0;
    char *real_path = NULL;

    __ES_FSTAT  fstat;
    __hdle      fp;

    char letter = (char)((unsigned long)dfs->data);

    ret = dfs_path_to_melis_path(&real_path, letter, path);
    if (ret)
    {
        return -1;
    }

    memset(&fstat, 0, sizeof(__ES_FSTAT));

    ret = EPDK_OK;

    fp = esFSYS_fopen(real_path, "rb");
    if (!fp)
    {
        ret = -1;
        goto out;
    }

    if (EPDK_OK != esFSYS_fstat(fp, &fstat))
    {
        esFSYS_fclose(fp);
        ret = -1;
        goto out;
    }

    esFSYS_fclose(fp);

    if (!S_ISDIR(fstat.mode))
    {
        if (esFSYS_remove(real_path) != EPDK_OK)
        {
            ret = -1;
            goto out;
        }
    }
    else
    {
        if (esFSYS_rmdir(real_path) != EPDK_OK)
        {
            ret = -1;
            goto out;
        }
    }

out:
    free(real_path);
    return ret;
}

static int melis_vfs_rename(struct dfs_filesystem *dfs, const char *old_path, const char *new_path)
{
    int ret = 0;
    char *real_old_path = NULL;
    char *real_new_path = NULL;

    __ES_FSTAT  fstat;
    __hdle      fp;

    char letter = (char)((unsigned long)dfs->data);

    ret = dfs_path_to_melis_path(&real_old_path, letter, old_path);
    if (ret)
    {
        ret = -1;
        goto out;
    }

    ret = dfs_path_to_melis_path(&real_new_path, letter, new_path);
    if (ret)
    {
        ret = -1;
        goto out;
    }

    memset(&fstat, 0, sizeof(__ES_FSTAT));

    ret = EPDK_OK;

    fp = esFSYS_fopen(real_old_path, "rb");
    if (!fp)
    {
        ret = -1;
        goto out;
    }

    if (EPDK_OK != esFSYS_fstat(fp, &fstat))
    {
        esFSYS_fclose(fp);
        ret = -1;
        goto out;
    }

    esFSYS_fclose(fp);

    if (S_ISDIR(fstat.mode) || S_ISREG(fstat.mode))
    {
        if (esFSYS_rename(real_new_path, real_old_path) != EPDK_OK)
        {
            ret = -1;
            goto out;
        }
    }

out:
    if (real_old_path)
    {
        free(real_old_path);
    }

    if (real_new_path)
    {
        free(real_new_path);
    }

    return ret;
}

static int melis_vfs_ftruncate(struct dfs_fd *file, off_t length)
{
    __hdle  fp;
    int ret = -1;

    fp = file->data;
    if (file->flags & O_DIRECTORY)
    {
        ret = -EBADF;
    }
    else
    {
#ifdef CONFIG_LAYERFS_CACHE_SUPPORT
        ret = CACHE_ftruncate(fp, length);
#else
        ret = esFSYS_ftruncate(fp, length);
#endif
    }
    return ret;
}

static int melis_vfs_exfat_mkfs(rt_device_t dev)
{
    return melis_vfs_mkfs(dev, "exfat");
}

static int melis_vfs_fat_mkfs(rt_device_t dev)
{
    return melis_vfs_mkfs(dev, "fat");
}

static int melis_vfs_ntfs_mkfs(rt_device_t dev)
{
    return melis_vfs_mkfs(dev, "ntfs");
}

static const struct dfs_file_ops _melis_vfs_fops =
{
    .open = melis_vfs_open,
    .close = melis_vfs_close,
    .ioctl = melis_vfs_ioctl,
    .read = melis_vfs_read,
    .write = melis_vfs_write,
    .flush = melis_vfs_flush,                    /* flush */
    .lseek = melis_vfs_lseek,                    /* lseek */
    .ftruncate = melis_vfs_ftruncate,            /* ftruncate */
    .getdents = melis_vfs_getdents,
    .poll = NULL,
    .lseek64 = melis_vfs_lseek64,                /* lseek */
};

static const struct dfs_filesystem_ops _fat_melis_vfs =
{
    .name = "fat",
    .flags = DFS_FS_FLAG_DEFAULT,
    .fops = &_melis_vfs_fops,

    .mount = melis_vfs_mount,
    .unmount = melis_vfs_unmount,
    .mkfs = melis_vfs_fat_mkfs,
    .statfs = melis_vfs_statfs,

    .rename = melis_vfs_rename,
    .unlink = melis_vfs_unlink,
    .stat = melis_vfs_stat,
};

static const struct dfs_filesystem_ops _exfat_melis_vfs =
{
    .name = "exfat",
    .flags = DFS_FS_FLAG_DEFAULT,
    .fops = &_melis_vfs_fops,

    .mount = melis_vfs_mount,
    .unmount = melis_vfs_unmount,
    .mkfs = melis_vfs_exfat_mkfs,
    .statfs = melis_vfs_statfs,

    .rename = melis_vfs_rename,
    .unlink = melis_vfs_unlink,
    .stat = melis_vfs_stat,
};

static const struct dfs_filesystem_ops _ntfs_melis_vfs =
{
    .name = "ntfs",
    .flags = DFS_FS_FLAG_DEFAULT,
    .fops = &_melis_vfs_fops,

    .mount = melis_vfs_mount,
    .unmount = melis_vfs_unmount,
    .mkfs = melis_vfs_ntfs_mkfs,
    .statfs = melis_vfs_statfs,

    .rename = melis_vfs_rename,
    .unlink = melis_vfs_unlink,
    .stat = melis_vfs_stat,
};

static const struct dfs_filesystem_ops _minfs_melis_vfs =
{
    .name = "minfs",
    .flags = DFS_FS_FLAG_DEFAULT,
    .fops = &_melis_vfs_fops,

    .mount = melis_vfs_mount,
    .unmount = melis_vfs_unmount,
    .mkfs = NULL,
    .statfs = melis_vfs_statfs,

    .rename = melis_vfs_rename,
    .unlink = melis_vfs_unlink,
    .stat = melis_vfs_stat,
};

static const struct dfs_filesystem_ops _melis_devfs_melis_vfs =
{
    .name = "melis_devfs",
    .flags = DFS_FS_FLAG_DEFAULT,
    .fops = &_melis_vfs_fops,

    .mount = melis_vfs_mount,
    .unmount = melis_vfs_unmount,
    .mkfs = NULL,
    .statfs = melis_vfs_statfs,

    .rename = melis_vfs_rename,
    .unlink = melis_vfs_unlink,
    .stat = melis_vfs_stat,
};

int melis_vfs_init(void)
{
    /* register rom file system */
    dfs_register(&_fat_melis_vfs);
    dfs_register(&_exfat_melis_vfs);
    dfs_register(&_ntfs_melis_vfs);
    dfs_register(&_minfs_melis_vfs);
    dfs_register(&_melis_devfs_melis_vfs);

    return 0;
}
rootfs_initcall(melis_vfs_init);
