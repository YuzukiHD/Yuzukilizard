/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-04-15     Bernard      the first version
 * 2013-05-05     Bernard      remove CRC for ramfs persistence
 * 2013-05-22     Bernard      fix the no entry issue.
 * 2020-05-20     Zeng Zhijin  suppot directory and data slice
 */

#include <rtthread.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>

#include "dfs_ramfs.h"
#include <init.h>

#ifndef MIN
#define MIN(a, b) (a > b ? b : a)
#endif

static struct rt_mutex _ramfs_lock;

#define ramfs_dfs_lock()          rt_mutex_take(&_ramfs_lock, RT_WAITING_FOREVER);
#define ramfs_dfs_unlock()        rt_mutex_release(&_ramfs_lock);

int dfs_ramfs_mount(struct dfs_filesystem *fs,
                    unsigned long          rwflag,
                    const void            *data)
{
    struct dfs_ramfs *ramfs;

    if (data == NULL)
    {
        return -EIO;
    }

    ramfs_dfs_lock();

    ramfs = (struct dfs_ramfs *)data;
    fs->data = ramfs;

    ramfs_dfs_unlock();

    return RT_EOK;
}

int dfs_ramfs_unmount(struct dfs_filesystem *fs)
{
    ramfs_dfs_lock();

    fs->data = NULL;

    ramfs_dfs_unlock();
    return RT_EOK;
}

int dfs_ramfs_statfs(struct dfs_filesystem *fs, struct statfs *buf)
{
    struct dfs_ramfs *ramfs;

    ramfs = (struct dfs_ramfs *)fs->data;
    RT_ASSERT(ramfs != NULL);
    RT_ASSERT(buf != NULL);

    ramfs_dfs_lock();

#ifdef CONFIG_RAMFS_SYSTEM_HEAP
    rt_uint32_t total = 0;
    rt_uint32_t used = 0;
    rt_uint32_t max_used = 0;

    rt_memory_info(&total, &used, &max_used);

    buf->f_bsize  = 512;
    buf->f_blocks = (total / 4) / 512;
    buf->f_bfree  = ((total - used) / 4) / 512;
#else
    buf->f_bsize  = 512;
    buf->f_blocks = ramfs->memheap.pool_size / 512;
    buf->f_bfree  = ramfs->memheap.available_size / 512;
#endif

    ramfs_dfs_unlock();

    return RT_EOK;
}

int dfs_ramfs_ioctl(struct dfs_fd *file, int cmd, void *args)
{
    return -EIO;
}

#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
struct ramfs_dirent *dfs_ramfs_lookup_dentry(struct dfs_ramfs *ramfs,
        const char       *path,
        rt_size_t        *size,
        struct ramfs_dirent *parent_dirent)
{
    char name[RAMFS_NAME_MAX + 1];
    char *subpath = NULL;
    char *temp_path = (char *)path;
    struct ramfs_dirent *dirent;

    while (*temp_path == '/' && *temp_path)
    {
        temp_path ++;
    }

    rt_memset(name, 0, sizeof(name));

    subpath = strchr(temp_path, '/');
    if (!subpath)
    {
        snprintf(name, RAMFS_NAME_MAX, "%s", temp_path);
    }
    else
    {
        snprintf(name, (subpath - temp_path + 1) > RAMFS_NAME_MAX ? RAMFS_NAME_MAX : (subpath - temp_path + 1), "%s", temp_path);
    }

    if (parent_dirent->dirent_list.next == &(parent_dirent->dirent_list))
    {
        return NULL;
    }

    for (dirent = rt_list_entry(parent_dirent->dirent_list.next, struct ramfs_dirent, list);
         dirent->list.next != &(parent_dirent->dirent_list);
         dirent = rt_list_entry(dirent->list.next, struct ramfs_dirent, list))
    {
        if (rt_strcmp(dirent->name, name) == 0)
        {
            if (dirent->type == RAMFS_DIR && subpath)
            {
                return dfs_ramfs_lookup_dentry(ramfs, subpath, size, dirent);
            }
            return dirent;
        }
    }

    if (rt_strcmp(dirent->name, name) == 0)
    {
        if (dirent->type == RAMFS_DIR && subpath)
        {
            return dfs_ramfs_lookup_dentry(ramfs, subpath, size, dirent);
        }
        return dirent;
    }

    return NULL;
}

struct ramfs_dirent *dfs_ramfs_lookup_partent_dentry(struct dfs_ramfs *ramfs,
        const char       *path)
{
    char name[RAMFS_NAME_MAX + 1];
    char *subpath = NULL;
    struct ramfs_dirent *dirent;
    rt_size_t size;

    subpath = strrchr(path, '/');
    if (!subpath || subpath == path)
    {
        return &ramfs->root;
    }
    else
    {
        rt_memset(name, 0, sizeof(name));
        rt_memcpy(name, path, subpath - path);
        return dfs_ramfs_lookup_dentry(ramfs, name, &size, &ramfs->root);
    }
}

#endif

struct ramfs_dirent *dfs_ramfs_lookup(struct dfs_ramfs *ramfs,
                                      const char       *path,
                                      rt_size_t        *size)
{
    const char *subpath;
    struct ramfs_dirent *dirent;

    subpath = path;
    while (*subpath == '/' && *subpath)
    {
        subpath ++;
    }
    if (! *subpath) /* is root directory */
    {
        *size = 0;

        return &(ramfs->root);
    }

    ramfs_dfs_lock();

#ifndef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
    for (dirent = rt_list_entry(ramfs->root.list.next, struct ramfs_dirent, list);
         dirent != &(ramfs->root);
         dirent = rt_list_entry(dirent->list.next, struct ramfs_dirent, list))
    {
        if (rt_strcmp(dirent->name, subpath) == 0)
        {
            *size = dirent->size;

            ramfs_dfs_unlock();
            return dirent;
        }
    }

    ramfs_dfs_unlock();
    /* not found */
    return NULL;
#else
    dirent = dfs_ramfs_lookup_dentry(ramfs, path, size, &ramfs->root);
    ramfs_dfs_unlock();
    return dirent;
#endif
}

int dfs_ramfs_read(struct dfs_fd *file, void *buf, size_t count)
{
    rt_size_t length;
    struct ramfs_dirent *dirent;

    ramfs_dfs_lock();

    dirent = (struct ramfs_dirent *)file->data;
    RT_ASSERT(dirent != NULL);

    if (count < file->size - file->pos)
    {
        length = count;
    }
    else
    {
        length = file->size - file->pos;
    }

    if (length > 0)
    {
#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
        int slice_pos = file->pos / DATA_SLICE_SIZE;
        int slice_inside_pos = file->pos % DATA_SLICE_SIZE;
        int size = length;
        int sz = 0;

        data_slice *data_s = rt_slist_entry(dirent->data_slice_chain.list.next, data_slice, list);

        while (slice_pos-- > 0 && data_s != NULL)
        {
            if (data_s->list.next)
            {
                data_s = rt_slist_entry(data_s->list.next, data_slice, list);
            }
            else
            {
                data_s = NULL;
            }
        }

        if (file->pos % DATA_SLICE_SIZE && data_s != NULL)
        {
            int len = MIN(DATA_SLICE_SIZE - slice_inside_pos, length);
            memcpy(buf, data_s->data_node + slice_inside_pos, len);
            size -= len;
            buf += len;
            sz += len;
            if (data_s->list.next)
            {
                data_s = rt_slist_entry(data_s->list.next, data_slice, list);
            }
            else
            {
                data_s = RT_NULL;
            }
        }

        while (size >= DATA_SLICE_SIZE && data_s != NULL)
        {
            memcpy(buf, data_s->data_node, DATA_SLICE_SIZE);
            size -= DATA_SLICE_SIZE;
            buf += DATA_SLICE_SIZE;
            sz += DATA_SLICE_SIZE;
            if (data_s->list.next)
            {
                data_s = rt_slist_entry(data_s->list.next, data_slice, list);
            }
            else
            {
                data_s = RT_NULL;
            }
        }

        if (size && data_s != NULL)
        {
            memcpy(buf, data_s->data_node, size);
            sz += size;
        }

        length = sz;
#else
        memcpy(buf, &(dirent->data[file->pos]), length);
#endif
    }

    /* update file current position */
    file->pos += length;

    ramfs_dfs_unlock();
    return length;
}

#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
static int ramfs_data_slice_free(struct ramfs_dirent *dirent, data_slice *data)
{
    struct dfs_ramfs *ramfs;
    data_slice *temp;
    rt_slist_t *pos;
    rt_slist_t *next_pos;

    RT_ASSERT(dirent != NULL);

    ramfs = dirent->fs;
    RT_ASSERT(ramfs != NULL);

    if (data == NULL)
    {
        return 0;
    }

    temp = data;
    pos = &(temp->list);
    next_pos = pos->next;
    while (next_pos)
    {
        pos = next_pos;
        temp = rt_slist_entry(pos, data_slice, list);
        if (temp)
        {
            if (temp->data_node)
            {
#ifdef CONFIG_RAMFS_SYSTEM_HEAP
                rt_free(temp->data_node);
#else
                rt_memheap_free(temp->data_node);
#endif
            }
            next_pos = pos->next;
            rt_slist_remove(&(data->list), pos);
            rt_free(temp);
        }
    }

    if (data->data_node)
    {
#ifdef CONFIG_RAMFS_SYSTEM_HEAP
        rt_free(data->data_node);
#else
        rt_memheap_free(data->data_node);
#endif
    }
    rt_free(data);

    return 0;
}

static int dfs_ramfs_extend(struct dfs_fd *fd, size_t count)
{
    struct ramfs_dirent *dirent;
    struct dfs_ramfs *ramfs;

    dirent = (struct ramfs_dirent *)fd->data;
    RT_ASSERT(dirent != NULL);

    ramfs = dirent->fs;
    RT_ASSERT(ramfs != NULL);

    int inc_slice_num = 0;
    int i = 0;
    data_slice *data_s = NULL;
    data_slice *inc_add_root = NULL;
    rt_slist_t *tail = NULL;

    if (count > 0)
    {
        int increase_size = count;
        int slice_inside_size = 0;

        if (fd->size % DATA_SLICE_SIZE)
        {
            slice_inside_size = DATA_SLICE_SIZE - (fd->size % DATA_SLICE_SIZE);
        }

        if (increase_size > slice_inside_size)
        {
            if (fd->size != 0)
            {
                increase_size -= slice_inside_size;
            }

            if (increase_size % DATA_SLICE_SIZE)
            {
                inc_slice_num = increase_size / DATA_SLICE_SIZE + 1;
            }
            else
            {
                inc_slice_num = increase_size / DATA_SLICE_SIZE;
            }

            for (i = 0; i < inc_slice_num; i++)
            {
                data_s = rt_malloc(sizeof(data_slice));
                if (!data_s)
                {
                    rt_kprintf("allocate memory failed! %s, %d\n", __func__, __LINE__);
                    goto err;
                }
                rt_memset(data_s, 0, sizeof(data_slice));

                rt_slist_init(&(data_s->list));

#ifdef CONFIG_RAMFS_SYSTEM_HEAP
                data_s->data_node = rt_malloc(DATA_SLICE_SIZE);
#else
                data_s->data_node = rt_memheap_alloc(&(ramfs->memheap), DATA_SLICE_SIZE);
#endif
                if (!data_s->data_node)
                {
                    rt_free(data_s);
                    rt_kprintf("allocate memory failed! %s, %d\n", __func__, __LINE__);
                    goto err;
                }
                rt_memset(data_s->data_node, 0, DATA_SLICE_SIZE);

                if (i == 0)
                {
                    inc_add_root = data_s;
                }
                else
                {
                    rt_slist_append(&(inc_add_root->list), &(data_s->list));
                }
            }
        }

        if (inc_add_root)
        {
            tail = rt_slist_tail(&(dirent->data_slice_chain.list));
            tail->next = &(inc_add_root->list);
        }

        return 0;
err:
        if (inc_add_root)
        {
            ramfs_data_slice_free(dirent, inc_add_root);
        }
        return -1;
    }
    return 0;
}
#endif

int dfs_ramfs_write(struct dfs_fd *fd, const void *buf, size_t count)
{
    struct ramfs_dirent *dirent;
    struct dfs_ramfs *ramfs;

    ramfs_dfs_lock();

    dirent = (struct ramfs_dirent *)fd->data;
    RT_ASSERT(dirent != NULL);

    ramfs = dirent->fs;
    RT_ASSERT(ramfs != NULL);

#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
    if (count + fd->pos > fd->size)
    {
        int increase_size = count + fd->pos - fd->size;

        if (dfs_ramfs_extend(fd, increase_size))
        {
            ramfs_dfs_unlock();
            return 0;
        }
        dirent->size = fd->pos + count;
        fd->size = dirent->size;
    }

    if (count > 0)
    {
        int slice_pos = fd->pos / DATA_SLICE_SIZE;
        int slice_inside_pos = fd->pos % DATA_SLICE_SIZE;
        int size = count;
        int sz = 0;

        data_slice *data_s = rt_slist_entry(dirent->data_slice_chain.list.next, data_slice, list);

        while (slice_pos-- > 0 && data_s != NULL)
        {
            if (data_s->list.next)
            {
                data_s = rt_slist_entry(data_s->list.next, data_slice, list);
            }
            else
            {
                data_s = RT_NULL;
            }
        }

        if (fd->pos % DATA_SLICE_SIZE && data_s != NULL)
        {
            int len = MIN(DATA_SLICE_SIZE - slice_inside_pos, size);
            memcpy(data_s->data_node + slice_inside_pos, buf, len);
            size -= len;
            buf += len;
            sz += len;
            if (data_s->list.next)
            {
                data_s = rt_slist_entry(data_s->list.next, data_slice, list);
            }
            else
            {
                data_s = RT_NULL;
            }
        }

        while (size >= DATA_SLICE_SIZE && data_s != NULL)
        {
            memcpy(data_s->data_node, buf, DATA_SLICE_SIZE);
            size -= DATA_SLICE_SIZE;
            buf += DATA_SLICE_SIZE;
            sz += DATA_SLICE_SIZE;
            if (data_s->list.next)
            {
                data_s = rt_slist_entry(data_s->list.next, data_slice, list);
            }
            else
            {
                data_s = RT_NULL;
            }
        }

        if (size && data_s != NULL)
        {
            memcpy(data_s->data_node, buf, size);
            buf += size;
            sz += size;
            size = 0;
        }

        count = sz;
    }

#else
    if (count + fd->pos > fd->size)
    {
        rt_uint8_t *ptr;
        ptr = rt_memheap_realloc(&(ramfs->memheap), dirent->data, fd->pos + count);
        if (ptr == NULL)
        {
            rt_set_errno(-ENOMEM);

            ramfs_dfs_unlock();
            return 0;
        }

        /* update dirent and file size */
        dirent->data = ptr;
        dirent->size = fd->pos + count;
        fd->size = dirent->size;
    }

    if (count > 0)
    {
        memcpy(dirent->data + fd->pos, buf, count);
    }
#endif

    /* update file current position */
    fd->pos += count;

    ramfs_dfs_unlock();
    return count;
}

int dfs_ramfs_lseek(struct dfs_fd *file, off_t offset)
{
    if (offset <= (off_t)file->size)
    {
        file->pos = offset;

        return file->pos;
    }
    else
    {
        struct ramfs_dirent *dirent;
        struct dfs_ramfs *ramfs;

        dirent = (struct ramfs_dirent *)file->data;
        RT_ASSERT(dirent != NULL);

        ramfs = dirent->fs;
        RT_ASSERT(ramfs != NULL);

        ramfs_dfs_lock();

#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
        int increase_size = offset - file->size;
        if (file->size % DATA_SLICE_SIZE)
        {
            increase_size += (DATA_SLICE_SIZE  - file->size % DATA_SLICE_SIZE);
        }
        if (dfs_ramfs_extend(file, increase_size))
        {
            ramfs_dfs_unlock();
            return file->pos;
        }
        dirent->size = offset;
        file->size = dirent->size;

        file->pos = offset;
        ramfs_dfs_unlock();
        return file->pos;
#else
        rt_uint8_t *ptr;
        ptr = rt_memheap_realloc(&(ramfs->memheap), dirent->data, offset);
        if (ptr == NULL)
        {
            ramfs_dfs_unlock();
            rt_set_errno(-ENOMEM);
            return -EIO;
        }
        /* update dirent and file size */
        dirent->data = ptr;
        dirent->size = offset;
        file->size = dirent->size;

        file->pos = offset;
        ramfs_dfs_unlock();
        return file->pos;
#endif
    }
}

_off64_t dfs_ramfs_lseek64(struct dfs_fd *file, _off64_t offset64)
{
    off_t offset;
    _off64_t res;

    offset = (off_t)(offset64 & 0xFFFFFFFF);

    if (offset <= (off_t)file->size)
    {
        file->pos = offset;

        res = (file->pos) & 0x00000000FFFFFFFFL;
        return res;
    }
    else
    {
        struct ramfs_dirent *dirent;
        struct dfs_ramfs *ramfs;

        dirent = (struct ramfs_dirent *)file->data;
        RT_ASSERT(dirent != NULL);

        ramfs = dirent->fs;
        RT_ASSERT(ramfs != NULL);

        ramfs_dfs_lock();

#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
        int increase_size = offset - file->size;
        if (file->size % DATA_SLICE_SIZE)
        {
            increase_size += (DATA_SLICE_SIZE  - file->size % DATA_SLICE_SIZE);
        }
        if (dfs_ramfs_extend(file, increase_size))
        {
            res = (file->pos) & 0x00000000FFFFFFFFL;
            ramfs_dfs_unlock();
            return res;
        }
        dirent->size = offset;
        file->size = dirent->size;

        file->pos = offset;
        ramfs_dfs_unlock();
        return file->pos;
#else
        rt_uint8_t *ptr;
        ptr = rt_memheap_realloc(&(ramfs->memheap), dirent->data, offset);
        if (ptr == NULL)
        {
            ramfs_dfs_unlock();
            rt_set_errno(-ENOMEM);
            return -EIO;
        }
        /* update dirent and file size */
        dirent->data = ptr;
        dirent->size = offset;
        file->size = dirent->size;

        file->pos = offset;
        res = (file->pos) & 0x00000000FFFFFFFFL;
        ramfs_dfs_unlock();
        return res;
#endif
    }
}


int dfs_ramfs_close(struct dfs_fd *file)
{
    ramfs_dfs_lock();

    file->data = NULL;

    ramfs_dfs_unlock();
    return RT_EOK;
}

int dfs_ramfs_open(struct dfs_fd *file)
{
    rt_size_t size;
    struct dfs_ramfs *ramfs;
    struct ramfs_dirent *dirent;
    struct dfs_filesystem *fs;

    fs = (struct dfs_filesystem *)file->data;

    ramfs = (struct dfs_ramfs *)fs->data;
    RT_ASSERT(ramfs != NULL);

    if (file->flags & O_DIRECTORY)
    {
        if (file->flags & O_CREAT)
        {
#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
            struct ramfs_dirent *parent;

            dirent = dfs_ramfs_lookup(ramfs, file->path, &size);
            if (dirent == &(ramfs->root)) /* it's root directory */
            {
                return -ENOENT;
            }

            if (dirent == NULL)
            {
                {
                    char *name_ptr, *ptr;

                    ramfs_dfs_lock();
                    /* create a file entry */
#ifdef CONFIG_RAMFS_SYSTEM_HEAP
                    dirent = (struct ramfs_dirent *)
                             rt_malloc(sizeof(struct ramfs_dirent));
#else
                    dirent = (struct ramfs_dirent *)
                             rt_memheap_alloc(&(ramfs->memheap),
                                              sizeof(struct ramfs_dirent));
#endif
                    if (dirent == NULL)
                    {
                        ramfs_dfs_unlock();
                        return -ENOMEM;
                    }

                    rt_memset(dirent, 0, sizeof(struct ramfs_dirent));
                    /* remove '/' separator */
                    name_ptr = file->path;
                    while (*name_ptr == '/' && *name_ptr)
                    {
                        name_ptr ++;
                    }
                    ptr = strrchr(name_ptr, '/');
                    if (ptr)
                    {
                        name_ptr = ptr + 1;
                    }
                    strncpy(dirent->name, name_ptr, RAMFS_NAME_MAX);

                    rt_list_init(&(dirent->list));
                    rt_list_init(&(dirent->dirent_list));
#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
                    rt_memset(&(dirent->data_slice_chain), 0, sizeof(data_slice));
                    rt_slist_init(&(dirent->data_slice_chain.list));
#else
                    dirent->data = NULL;
#endif
                    dirent->size = 0;
                    dirent->fs = ramfs;
                    dirent->type = RAMFS_DIR;

                    /* add to the root directory */
                    parent = dfs_ramfs_lookup_partent_dentry(ramfs, file->path);
                    if (parent)
                    {
                        rt_list_insert_after(&parent->dirent_list, &(dirent->list));
                        dirent->parent = parent;
                    }
                    else
                    {
                        rt_list_insert_after(&(ramfs->root.dirent_list), &(dirent->list));
                        dirent->parent = &(ramfs->root);
                    }
                    ramfs_dfs_unlock();
                }
            }
            else
            {
                return -ENOENT;
            }
#else
            return -ENOSPC;
#endif
        }

        /* open directory */
        dirent = dfs_ramfs_lookup(ramfs, file->path, &size);
        if (dirent == NULL)
        {
            return -ENOENT;
        }
        if (dirent == &(ramfs->root)) /* it's root directory */
        {
            if (!(file->flags & O_DIRECTORY))
            {
#ifndef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
                return -ENOENT;
#endif
            }
        }
#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
        if (dirent->type != RAMFS_DIR)
        {
            return -ENOENT;
        }
#endif
    }
    else
    {
        dirent = dfs_ramfs_lookup(ramfs, file->path, &size);
        if (dirent == &(ramfs->root)) /* it's root directory */
        {
            return -ENOENT;
        }

        if (dirent == NULL)
        {
            if (file->flags & O_CREAT || file->flags & O_WRONLY)
            {
                char *name_ptr;

                /* create a file entry */
#ifdef CONFIG_RAMFS_SYSTEM_HEAP
                dirent = (struct ramfs_dirent *)
                         rt_malloc(sizeof(struct ramfs_dirent));
#else
                dirent = (struct ramfs_dirent *)
                         rt_memheap_alloc(&(ramfs->memheap),
                                          sizeof(struct ramfs_dirent));
#endif
                if (dirent == NULL)
                {
                    return -ENOMEM;
                }
                rt_memset(dirent, 0, sizeof(struct ramfs_dirent));

                ramfs_dfs_lock();

                /* remove '/' separator */
                name_ptr = file->path;
                while (*name_ptr == '/' && *name_ptr)
                {
                    name_ptr ++;
                }
#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
                {
                    char *ptr = strrchr(name_ptr, '/');
                    if (ptr)
                    {
                        name_ptr = ptr + 1;
                    }
                }
#endif
                strncpy(dirent->name, name_ptr, RAMFS_NAME_MAX);

                rt_list_init(&(dirent->list));
#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
                rt_memset(&(dirent->data_slice_chain), 0, sizeof(data_slice));
                rt_slist_init(&(dirent->data_slice_chain.list));
#else
                dirent->data = NULL;
#endif
                dirent->size = 0;
                dirent->fs = ramfs;
#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
                dirent->type = RAMFS_FILE;
                rt_list_init(&(dirent->dirent_list));

                struct ramfs_dirent *parent;
                parent = dfs_ramfs_lookup_partent_dentry(ramfs, file->path);
                if (parent)
                {
                    rt_list_insert_after(&parent->dirent_list, &(dirent->list));
                    dirent->parent = parent;
                }
                else
                {
                    rt_list_insert_after(&(ramfs->root.dirent_list), &(dirent->list));
                    dirent->parent = &(ramfs->root);
                }
#else
                /* add to the root directory */
                rt_list_insert_after(&(ramfs->root.list), &(dirent->list));
#endif
                ramfs_dfs_unlock();
            }
            else
            {
                return -ENOENT;
            }
        }

        /* Creates a new file.
         * If the file is existing, it is truncated and overwritten.
         */
        if (file->flags & O_TRUNC)
        {
            dirent->size = 0;

            ramfs_dfs_lock();

#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
            if (dirent->data_slice_chain.list.next)
            {
                ramfs_data_slice_free(dirent, rt_slist_entry(dirent->data_slice_chain.list.next, data_slice, list));
            }
            rt_memset(&(dirent->data_slice_chain), 0, sizeof(data_slice));
            rt_slist_init(&(dirent->data_slice_chain.list));
#else
            if (dirent->data != NULL)
            {
#ifdef CONFIG_RAMFS_SYSTEM_HEAP
                rt_free(dirent->data);
#else
                rt_memheap_free(dirent->data);
#endif
                dirent->data = NULL;
            }
#endif
            ramfs_dfs_unlock();
        }
    }

    ramfs_dfs_lock();

    file->data = dirent;
    file->size = dirent->size;
    if (file->flags & O_APPEND)
    {
        file->pos = file->size;
    }
    else
    {
        file->pos = 0;
    }

    ramfs_dfs_unlock();
    return 0;
}

int dfs_ramfs_stat(struct dfs_filesystem *fs,
                   const char            *path,
                   struct stat            *st)
{
    rt_size_t size;
    struct ramfs_dirent *dirent;
    struct dfs_ramfs *ramfs;

    ramfs = (struct dfs_ramfs *)fs->data;
    dirent = dfs_ramfs_lookup(ramfs, path, &size);

    if (dirent == NULL)
    {
        return -ENOENT;
    }

    ramfs_dfs_lock();

    st->st_dev = 0;
#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
    st->st_mode = dirent->type == RAMFS_FILE ? S_IFREG : S_IFDIR;
#else
    st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH |
                  S_IWUSR | S_IWGRP | S_IWOTH;
#endif

    st->st_size = dirent->size;
    st->st_mtime = 0;

    ramfs_dfs_unlock();
    return RT_EOK;
}

int dfs_ramfs_getdents(struct dfs_fd *file,
                       struct dirent *dirp,
                       uint32_t    count)
{
    rt_size_t index, end;
    struct dirent *d;
    struct ramfs_dirent *dirent;
    struct ramfs_dirent *target_dirent;
    struct dfs_ramfs *ramfs;

    dirent = (struct ramfs_dirent *)file->data;
    target_dirent = dirent;

    ramfs  = dirent->fs;
    RT_ASSERT(ramfs != RT_NULL);

#ifndef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
    if (dirent != &(ramfs->root))
    {
        return -EINVAL;
    }
#endif

    /* make integer count */
    count = (count / sizeof(struct dirent));
    if (count == 0)
    {
        return -EINVAL;
    }

    ramfs_dfs_lock();

    end = file->pos + count;
    index = 0;
    count = 0;
#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY

    if (target_dirent->dirent_list.next == &(target_dirent->dirent_list))
    {
        ramfs_dfs_unlock();
        return count * sizeof(struct dirent);
    }

    for (dirent = rt_list_entry(target_dirent->dirent_list.next, struct ramfs_dirent, list);
         dirent->list.next != &(target_dirent->dirent_list) && index < end;
#else
    for (dirent = rt_list_entry(dirent->list.next, struct ramfs_dirent, list);
         dirent != &(ramfs->root) && index < end;
#endif
         dirent = rt_list_entry(dirent->list.next, struct ramfs_dirent, list))
    {
        if (index >= (rt_size_t)file->pos)
        {
            d = dirp + count;
#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
            d->d_type = dirent->type == RAMFS_FILE ? DT_REG : DT_DIR;
#else
            d->d_type = DT_REG;
#endif
            d->d_namlen = RT_NAME_MAX;
            d->d_reclen = (rt_uint16_t)sizeof(struct dirent);
            rt_strncpy(d->d_name, dirent->name, RAMFS_NAME_MAX);

            count += 1;
            file->pos += 1;
        }
        index += 1;
    }

#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
    if (dirent->list.next == &(target_dirent->dirent_list) && index < end)
    {
        if (index >= (rt_size_t)file->pos)
        {
            d = dirp + count;
            d->d_type = dirent->type == RAMFS_FILE ? DT_REG : DT_DIR;
            d->d_namlen = RT_NAME_MAX;
            d->d_reclen = (rt_uint16_t)sizeof(struct dirent);
            rt_strncpy(d->d_name, dirent->name, RAMFS_NAME_MAX);

            count += 1;
            file->pos += 1;
        }
        index += 1;
    }
#endif
    ramfs_dfs_unlock();
    return count * sizeof(struct dirent);
}

int dfs_ramfs_unlink(struct dfs_filesystem *fs, const char *path)
{
    rt_size_t size;
    struct dfs_ramfs *ramfs;
    struct ramfs_dirent *dirent;

    ramfs = (struct dfs_ramfs *)fs->data;
    RT_ASSERT(ramfs != NULL);

    dirent = dfs_ramfs_lookup(ramfs, path, &size);
    if (dirent == NULL)
    {
        return -ENOENT;
    }

    if (&(ramfs->root) == dirent)
    {
        return -EIO;
    }

    if (dirent->type == RAMFS_DIR)
    {
        if (!rt_list_isempty(&dirent->dirent_list))
        {
            rt_kprintf("Can not unlink non empty directory!\n");
            return -EIO;
        }
    }

    ramfs_dfs_lock();

    rt_list_remove(&(dirent->list));
#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
    if (dirent->data_slice_chain.list.next)
    {
        ramfs_data_slice_free(dirent, rt_slist_entry(dirent->data_slice_chain.list.next, data_slice, list));
    }
#else
    if (dirent->data != NULL)
    {
#ifdef CONFIG_RAMFS_SYSTEM_HEAP
        rt_free(dirent->data);
#else
        rt_memheap_free(dirent->data);
#endif
    }
#endif
#ifdef CONFIG_RAMFS_SYSTEM_HEAP
    rt_free(dirent);
#else
    rt_memheap_free(dirent);
#endif

    ramfs_dfs_unlock();
    return RT_EOK;
}

int dfs_ramfs_rename(struct dfs_filesystem *fs,
                     const char            *oldpath,
                     const char            *newpath)
{
    struct ramfs_dirent *dirent;
    struct dfs_ramfs *ramfs;
    rt_size_t size;

    ramfs = (struct dfs_ramfs *)fs->data;
    RT_ASSERT(ramfs != NULL);

    dirent = dfs_ramfs_lookup(ramfs, newpath, &size);
    if (dirent != NULL)
    {
        return -EEXIST;
    }

    dirent = dfs_ramfs_lookup(ramfs, oldpath, &size);
    if (dirent == NULL)
    {
        return -ENOENT;
    }

    ramfs_dfs_lock();

#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
    const char *subpath;
    char *ptr;

    subpath = newpath;
    while (*subpath == '/' && *subpath)
    {
        subpath ++;
    }

    if (! *subpath)
    {
        ramfs_dfs_unlock();
        return -EINVAL;
    }

    ptr = strrchr(subpath, '/');
    if (ptr)
    {
        subpath = ptr + 1;
    }
    strncpy(dirent->name, subpath, RAMFS_NAME_MAX);
#else
    strncpy(dirent->name, newpath, RAMFS_NAME_MAX);
#endif

#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
    {
        struct ramfs_dirent *new_parent;
        struct ramfs_dirent *old_parent;

        old_parent = dirent->parent;
        rt_list_remove(&(dirent)->list);

        /* add to the root directory */
        new_parent = dfs_ramfs_lookup_partent_dentry(ramfs, newpath);
        if (new_parent)
        {
            rt_list_insert_after(&new_parent->dirent_list, &(dirent->list));
            dirent->parent = new_parent;
        }
        else
        {
            rt_list_insert_after(&(ramfs->root.dirent_list), &(dirent->list));
            dirent->parent = &(ramfs->root);
        }
    }
#endif
    ramfs_dfs_unlock();
    return RT_EOK;
}

static int dfs_ramfs_ftruncate(struct dfs_fd *file, off_t length)
{
    int ret = -1;

    if (file->flags & O_DIRECTORY)
    {
        return -EBADF;
    }
    else
    {
        struct ramfs_dirent *dirent;
        struct dfs_ramfs *ramfs;

        ramfs_dfs_lock();

        dirent = (struct ramfs_dirent *)file->data;
        RT_ASSERT(dirent != NULL);

        ramfs = dirent->fs;
        RT_ASSERT(ramfs != NULL);

#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
        if (length == file->size)
        {
            ramfs_dfs_unlock();
            return 0;
        }
        else if (length > file->size)
        {
            int increase_size = length - file->size;

            if (dfs_ramfs_extend(file, increase_size))
            {
                ramfs_dfs_unlock();
                return -1;
            }

            dirent->size = length;
            file->size = dirent->size;
            if (file->size <= file->pos)
            {
                file->pos = file->size - 1;
            }
            ramfs_dfs_unlock();
            return 0;
        }
        else
        {
            data_slice *data_s = RT_NULL, *last_data_s = RT_NULL;
            int slice_pos = length / DATA_SLICE_SIZE;
            int slice_inside_pos = length % DATA_SLICE_SIZE;

            if (slice_inside_pos != 0)
            {
                slice_pos ++;
            }

            data_s = rt_slist_entry(dirent->data_slice_chain.list.next, data_slice, list);
            last_data_s = data_s;

            while (slice_pos-- > 0 && data_s != NULL)
            {
                if (data_s->list.next)
                {
                    data_s = rt_slist_entry(data_s->list.next, data_slice, list);
                }
                else
                {
                    data_s = RT_NULL;
                }

                if (slice_pos == 1)
                {
                    last_data_s = data_s;
                }
            }

            if (data_s)
            {
                if (last_data_s)
                {
                    last_data_s->list.next = RT_NULL;
                }
                ramfs_data_slice_free(dirent, data_s);
            }

            dirent->size = length;
            file->size = dirent->size;
            if (file->size <= file->pos)
            {
                file->pos = file->size - 1;
            }
            ramfs_dfs_unlock();
            return 0;
        }
#else
        rt_uint8_t *ptr;
        ptr = rt_memheap_realloc(&(ramfs->memheap), dirent->data, length);
        if (ptr == NULL)
        {
            rt_set_errno(-ENOMEM);
            ramfs_dfs_unlock();
            return -EIO;
        }
        /* update dirent and file size */
        dirent->data = ptr;
        dirent->size = length;
        file->size = dirent->size;
        if (file->size <= file->pos)
        {
            file->pos = file->size - 1;
        }
        ramfs_dfs_unlock();
        return 0;
#endif
    }
}

static const struct dfs_file_ops _ram_fops =
{
    dfs_ramfs_open,
    dfs_ramfs_close,
    dfs_ramfs_ioctl,
    dfs_ramfs_read,
    dfs_ramfs_write,
    NULL, /* flush */
    dfs_ramfs_lseek,
    dfs_ramfs_getdents,
    dfs_ramfs_ftruncate,
    NULL,
    NULL,
    dfs_ramfs_lseek64,
};

static const struct dfs_filesystem_ops _ramfs =
{
    "ram",
    DFS_FS_FLAG_DEFAULT,
    &_ram_fops,

    dfs_ramfs_mount,
    dfs_ramfs_unmount,
    NULL, /* mkfs */
    dfs_ramfs_statfs,

    dfs_ramfs_unlink,
    dfs_ramfs_stat,
    dfs_ramfs_rename,
};

int dfs_ramfs_init(void)
{
    /* register ram file system */
    rt_mutex_init(&_ramfs_lock, "ramfsmtx", RT_IPC_FLAG_FIFO);
    dfs_register(&_ramfs);

    return 0;
}
INIT_COMPONENT_EXPORT(dfs_ramfs_init);
rootfs_initcall(dfs_ramfs_init);

struct dfs_ramfs *dfs_ramfs_system_heap_create(struct dfs_ramfs *ramfs)
{
    if (ramfs == NULL)
    {
        return NULL;
    }

    /* initialize root directory */
    memset(&(ramfs->root), 0x00, sizeof(ramfs->root));
    rt_list_init(&(ramfs->root.list));
    ramfs->root.size = 0;
    strcpy(ramfs->root.name, ".");
    ramfs->root.fs = ramfs;
#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
    rt_slist_init(&(ramfs->root.data_slice_chain.list));
#endif
#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
    ramfs->root.type = RAMFS_DIR;
    rt_list_init(&(ramfs->root.dirent_list));
#endif

    return ramfs;
}

struct dfs_ramfs *dfs_ramfs_create(rt_uint8_t *pool, rt_size_t size)
{
    struct dfs_ramfs *ramfs;
    rt_uint8_t *data_ptr;
    rt_err_t result;

    size  = RT_ALIGN_DOWN(size, RT_ALIGN_SIZE);
    ramfs = (struct dfs_ramfs *)pool;

    data_ptr = (rt_uint8_t *)(ramfs + 1);
    size = size - sizeof(struct dfs_ramfs);
    size = RT_ALIGN_DOWN(size, RT_ALIGN_SIZE);

    result = rt_memheap_init(&ramfs->memheap, "ramfs", data_ptr, size);
    if (result != RT_EOK)
    {
        return NULL;
    }
    /* detach this memheap object from the system */
    rt_object_detach((rt_object_t) & (ramfs->memheap));

    /* initialize ramfs object */
    ramfs->magic = RAMFS_MAGIC;
    ramfs->memheap.parent.type = RT_Object_Class_MemHeap | RT_Object_Class_Static;

    /* initialize root directory */
    memset(&(ramfs->root), 0x00, sizeof(ramfs->root));
    rt_list_init(&(ramfs->root.list));
    ramfs->root.size = 0;
    strcpy(ramfs->root.name, ".");
    ramfs->root.fs = ramfs;
#ifdef CONFIG_RT_USING_DFS_RAMFS_DATA_SLICE
    rt_slist_init(&(ramfs->root.data_slice_chain.list));
#endif
#ifdef CONFIG_RT_USING_DFS_RAMFS_SUPPORT_DIRECTORY
    ramfs->root.type = RAMFS_DIR;
    rt_list_init(&(ramfs->root.dirent_list));
#endif

    return ramfs;
}

int dfs_ramfs_create_mount(rt_size_t size)
{
    rt_uint8_t *ramfs_pool = RT_NULL;
    struct dfs_ramfs *ramfs = RT_NULL;
    int ret = -1;

#ifdef CONFIG_RAMFS_SYSTEM_HEAP
    ramfs = (struct dfs_ramfs *) rt_malloc(sizeof(struct dfs_ramfs));
    ramfs = dfs_ramfs_system_heap_create(ramfs);
#else
    ramfs_pool = rt_malloc(size);
    if (ramfs_pool)
    {
        ramfs = (struct dfs_ramfs *) dfs_ramfs_create((rt_uint8_t *)ramfs_pool, size);
    }
#endif
    if (ramfs != RT_NULL)
    {
        if (dfs_mount(RT_NULL, CONFIG_RT_USING_DFS_RAMFS_PATH, "ram", 0, ramfs) == 0)
        {
            ret = 0;
            rt_kprintf("Mount RAMDisk done!\n");
        }
        else
        {
            rt_kprintf("Mount RAMDisk failed.\n");
        }
    }
    else
    {
        if (ramfs_pool)
        {
            rt_free(ramfs_pool);
        }
        rt_kprintf("alloc ramfs failed\n");
    }
    return ret;
}
