/*
*********************************************************************************************************
*                                                    eMOD
*                                   the Easy Portable/Player Operation System
*                                              mod_mmp sub-system
*
*                                    (c) Copyright 2008-2009, Kevin.Z China
*                                              All Rights Reserved
*
* File   : cache_fs.h
* Version: V1.0
* By     : kevin.z
* Date   : 2009-2-2 8:37
    cache体系的文件操作的API.
    cache内部所有模块都是用这个API操作文件，不会直接调用系统的fopen等函数。

    强制规定:
    如果想使用cache_fs.h提供的函数，必须在模块打开时先调用
    CACHE_InitHeap();
    CACHE_InitPhyHeap();
    初始化cache_common的内存分配链.因为cache_fs的函数默认使用cache_common提供的
    内存操作函数.
* Modify : zengzhijin 2020-04-24
*********************************************************************************************************
*/
#ifndef _CACHE_FS_H_
#define _CACHE_FS_H_

#include <sys/types.h>
#include <libc.h>

typedef enum tag_CACHE_FS_WORK_MODE
{
    CACHE_FS_WORK_MODE_BYTEALIGN    = 0,   // read align by byte
    CACHE_FS_WORK_MODE_SECTALIGN    = 1,   // read align by sector
    /* user defined read,用户自己处理如果读取文件.目前默认no cache.*/
    /* 解决不是真正存在于硬盘上的文件的类型，比如从内存读取等，不适用于文件操作的文件。对于这类文件,不要用fopen()等函数 */
    CACHE_FS_WORK_MODE_USER_DEFINED = 2,
    CACHE_FS_WORK_MODE_NO_CACHE     = 3,   // use fread of fs，no cache
    CACHE_FS_WORK_MODE_HERBCACHE    = 4,   //内部起线程,主要用于录制写.
} CacheFSWorkMode;

extern ES_FILE     *CACHE_fopen(const char *path, const char *mode, __u32 flags);
extern __s32    CACHE_fstat(ES_FILE *fp, void *buf);
extern __s32    CACHE_fsync(ES_FILE *fp);
extern __s32    CACHE_ftruncate(ES_FILE *fp, off_t length);
extern int      CACHE_fclose(ES_FILE *fp);
extern int      CACHE_fread(void *buf, int size, int count, ES_FILE *fp);
extern int      CACHE_fwrite(void *buf, int size, int count, ES_FILE *fp);
extern int      CACHE_fseek(ES_FILE *fp, __s64 offset, int origin);
extern __s64    CACHE_ftell(ES_FILE *fp);
extern int      CACHE_fioctrl(ES_FILE *fp, __s32 Cmd, __s32 Aux, void *pBuffer);

/* 计算filesize的函数 */
extern __u32    CACHE_fsize(ES_FILE *fp);
/* 以下函数配置cachefs的属性，必须在newCacheFs之前(即调用CACHE_fopen之前)全部配置好。创建CacheFS之后，再调用这3个函数就无用了 */
extern __s32    CACHE_fs_set_workmod(__s32 mode);
extern __s32    CACHE_fs_get_workmod(void);

/* 适用于mode = CACHE_FS_WORK_MODE_SECTALIGN等有cache的模式 */
extern int      CACHE_fs_SetCacheSize(int cache_size, int mode);

/* 因为fs buffer mode要开线程， epdk系统下，开线程得给一个module id. */
extern void     CACHE_fs_set_mid(__u32 mid);

/* 如果是USRDEF模式，再配置__cache_usr_file_op_t */
/* 仅用于CACHE_FS_WORK_MODE_USER_DEFINED这种模式，设置好必要的数据结构fop = __cache_usr_file_op_t g_cache_usr_fop;在打开文件时使用 */
extern void     CACHE_ufinit(void *fop);

#endif //_CACHE_FS_H_

