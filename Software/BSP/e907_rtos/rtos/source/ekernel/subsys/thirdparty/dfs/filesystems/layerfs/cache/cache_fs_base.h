/*
********************************************************************************
*                                    eMOD
*                   the Easy Portable/Player Develop Kits
*                               mod_cache sub-system
*                          (module name, e.g.mpeg4 decoder plug-in) module
*
*          (c) Copyright 2010-2012, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : cache_fs_base.h
* Version: V1.0
* By     : Eric_wang
* Date   : 2011-2-18
* Description:
    作为内部共用的头文件
    作为基类使用
    子类中初始化，这里不提供初始化函数
* Modify : zengzhijin 2020-04-24
********************************************************************************
*/
#ifndef _CACHE_FS_BASE_H_
#define _CACHE_FS_BASE_H_

typedef struct tag_CACHEFS CacheFS;

typedef ES_FILE *(*CacheFS_fopen)(CacheFS *thiz, const __s8 *path, const __s8 *mode);   //打开文件,返回的是真正的文件指针
typedef __s32(*CacheFS_fclose)(CacheFS *thiz);      //关闭文件
typedef __s32(*CacheFS_fread)(CacheFS *thiz, void *buf, __s32 size, __s32 count);      //读文件
typedef __s32(*CacheFS_fwrite)(CacheFS *thiz, void *buf, __s32 size, __s32 count);      //写文件
typedef __s32(*CacheFS_fseek)(CacheFS *thiz, __s64 offset, __s32 origin);
typedef __s64(*CacheFS_ftell)(CacheFS *thiz);
typedef __s32(*CacheFS_fstat)(CacheFS *thiz, void *stat_buf);
typedef __s32(*CacheFS_fsync)(CacheFS *thiz);
typedef __s32(*CacheFS_ftruncate)(CacheFS *thiz, off_t offset);
typedef __s32(*CacheFS_SetCacheSize)(CacheFS *thiz, __s32 cache_size);      //适用于mode = CACHE_FS_WORK_MODE_SECTALIGN等有cache的模式
typedef ES_FILE   *(*CacheFS_GetFp)(CacheFS *thiz);   //得到真正的文件指针

/*******************************************************************************
Function name: tag_CACHEFS
Members:
    这里大部分函数接口都视作虚函数，但也有些callback函数不是, 所以需要初始化操作.
    因为初始化操作没有malloc内存，所以不需要exit函数

*******************************************************************************/
typedef struct tag_CACHEFS  //记录cachefs工作模式和相关参数的struct
{
    __s32       cache_fs_work_mode; //CACHE_FS_WORK_MODE_SECTALIGN;用于debug

    //CACHE_FS_WORK_MODE_SECTALIGN等带缓存的fs机制，会在这里malloc内存
    CacheFS_fopen           cachefs_fopen;
    CacheFS_fclose          cachefs_fclose;
    CacheFS_fread           cachefs_fread;
    CacheFS_fwrite          cachefs_fwrite;
    CacheFS_fseek           cachefs_fseek;
    CacheFS_ftell           cachefs_ftell;
    CacheFS_SetCacheSize    SetCacheSize;   //一般要求在fopen之前设置，否则后果难料
    CacheFS_GetFp           GetFp;          //得到真正的文件指针.
    CacheFS_fstat           cachefs_fstat;
    CacheFS_fsync           cachefs_fsync;
    CacheFS_ftruncate       cachefs_ftruncate;
} CacheFS;

extern __s32 CacheFS_Init(CacheFS *thiz);

#endif  /* _CACHE_FS_BASE_H_ */
