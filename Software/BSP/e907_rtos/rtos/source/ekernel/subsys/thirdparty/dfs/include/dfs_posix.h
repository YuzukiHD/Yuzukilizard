/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-05-27     Yi.qiu       The first version.
 * 2010-07-18     Bernard      add stat and statfs structure definitions.
 * 2011-05-16     Yi.qiu       Change parameter name of rename, "new" is C++ key word.
 * 2017-12-27     Bernard      Add fcntl API.
 * 2018-02-07     Bernard      Change the 3rd parameter of open/fcntl/ioctl to '...'
 */

#ifndef __DFS_POSIX_H__
#define __DFS_POSIX_H__

#include <dfs_file.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int fd;     /* directory file */
    char buf[512];
    int num;
    int cur;
} DIR;

/* directory api*/
int mkdir(const char *path, mode_t mode);
DIR *opendir(const char *name);
struct dirent *readdir(DIR *d);
long telldir(DIR *d);
void seekdir(DIR *d, off_t offset);
void rewinddir(DIR *d);
int closedir(DIR *d);

/* file api*/
int open(const char *file, int flags, ...);
int close(int d);

#ifndef _READ_WRITE_RETURN_TYPE
#define _READ_WRITE_RETURN_TYPE int
#endif

_READ_WRITE_RETURN_TYPE read(int fd, void *buf, size_t len);
_READ_WRITE_RETURN_TYPE write(int fd, const void *buf, size_t len);

off_t lseek(int fd, off_t offset, int whence);
int rename(const char *from, const char *to);
int unlink(const char *pathname);
int stat(const char *file, struct stat *buf);
int fstat(int fildes, struct stat *buf);
int fsync(int fildes);
int fcntl(int fildes, int cmd, ...);
int ioctl(int fildes, int cmd, ...);

/* directory api*/
int rmdir(const char *path);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);

/* file system api */
int statfs(const char *path, struct statfs *buf);

int access(const char *path, int amode);
int pipe(int fildes[2]);
int mkfifo(const char *path, mode_t mode);

int ftruncate(int fd, off_t length);
int dup(int fd);
int mount(const char *source, const char *target,
    const char *filesystemtype, unsigned long mountflags, const void *data);
int umount(const char *target);

#define MS_RDONLY  (1)

#ifdef __cplusplus
}
#endif

#endif
