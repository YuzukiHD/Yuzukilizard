#include <rtthread.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>

#include "dfs_rootfs.h"
#include <init.h>

extern struct dfs_filesystem filesystem_table[DFS_FILESYSTEMS_MAX];

int dfs_rootfs_mount(struct dfs_filesystem *fs,
                     unsigned long          rwflag,
                     const void            *data)
{
    return RT_EOK;
}

int dfs_rootfs_unmount(struct dfs_filesystem *fs)
{
    return RT_EOK;
}

int dfs_rootfs_statfs(struct dfs_filesystem *fs, struct statfs *buf)
{
    buf->f_bsize  = 512;
    buf->f_blocks = 0;
    buf->f_bfree  = 0;

    return RT_EOK;
}

static int dfs_rootfs_close(struct dfs_fd *file)
{
    return RT_EOK;
}

int dfs_rootfs_open(struct dfs_fd *file)
{
    if (file->flags & O_DIRECTORY)
    {
        if (file->flags & O_CREAT)
        {
            return -ENOSPC;
        }
        if (strcmp(file->path, "/"))
        {
            return -EEXIST;
        }
        file->pos = 0;
        file->size = 0;
        return RT_EOK;
    }
    return -EEXIST;
}

int dfs_rootfs_stat(struct dfs_filesystem *fs,
                    const char            *path,
                    struct stat           *st)
{
    struct dfs_filesystem *iter;

    for (iter = &filesystem_table[0];
         iter < &filesystem_table[DFS_FILESYSTEMS_MAX]; iter++)
    {
        if (iter->ops == NULL)
        {
            continue;
        }
        if (!strcmp(iter->path, path))
        {
            st->st_mode = S_IFDIR;
            return RT_EOK;
        }
    }

    return -EIO;
}

int dfs_rootfs_getdents(struct dfs_fd *file,
                        struct dirent *dirp,
                        uint32_t    count)
{
    rt_size_t index, end;
    struct dirent *d;
    struct dfs_filesystem *iter;

    /* make integer count */
    count = (count / sizeof(struct dirent));
    if (count == 0)
    {
        return -EINVAL;
    }

    end = file->pos + count;
    index = 0;
    count = 0;
    for (iter = &filesystem_table[0];
         iter < &filesystem_table[DFS_FILESYSTEMS_MAX]; iter++)
    {
        if (iter->ops == NULL)
        {
            continue;
        }
        if (!strcmp(iter->path, "/"))
        {
            continue;
        }
        if (index >= end)
        {
            break;
        }

        if (index >= (rt_size_t)file->pos)
        {
            d = dirp + count;
            d->d_type = DT_REG;
            d->d_namlen = RT_NAME_MAX;
            d->d_reclen = (rt_uint16_t)sizeof(struct dirent);
            rt_strncpy(d->d_name, iter->path, RT_NAME_MAX);

            count += 1;
            file->pos += 1;
        }
        index += 1;
    }

    return count;
}

static const struct dfs_file_ops _root_fops =
{
    dfs_rootfs_open,
    dfs_rootfs_close,
    NULL,
    NULL,
    NULL,
    NULL, /* flush */
    NULL,
    dfs_rootfs_getdents,
    NULL,
    NULL,
    NULL,
    NULL,
};

static const struct dfs_filesystem_ops _rootfs =
{
    "rootfs",
    DFS_FS_FLAG_DEFAULT,
    &_root_fops,

    dfs_rootfs_mount,
    dfs_rootfs_unmount,
    NULL, /* mkfs */
    dfs_rootfs_statfs,

    NULL,
    dfs_rootfs_stat,
    NULL,
};

int dfs_rootfs_init(void)
{
    /* register root file system */
    dfs_register(&_rootfs);

    return 0;
}
rootfs_initcall(dfs_rootfs_init);
INIT_COMPONENT_EXPORT(dfs_rootfs_init);
