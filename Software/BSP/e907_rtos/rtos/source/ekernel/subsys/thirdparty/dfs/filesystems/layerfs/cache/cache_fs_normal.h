/*
*********************************************************************************************************
*                                                    eMOD
*                                   the Easy Portable/Player Operation System
*                                              mod_mmp sub-system
*
*                                    (c) Copyright 2008-2009, Kevin.Z China
*                                              All Rights Reserved
*
* File   : cache_fs_normal.h
* Version: V1.0
* By     : kevin.z
* Date   : 2009-2-2 8:37
* Modify : zengzhijin 2020-04-24
*********************************************************************************************************
*/
#ifndef _CACHE_FS_NORMAL_H_
#define _CACHE_FS_NORMAL_H_

#include "cache_fs_base.h"

typedef struct __CACHE_NES_FILE_PTR
{
    ES_FILE     *fp;        //file pointer for read file

    __u8        *cache;     //buffer for cache file data, should be palloc
    __u32       buf_size;   //size of cache buffer, user define, CACHE_FS_CACHE_SIZE

    __u8        *cur_ptr;   //current access pointer in the cache buffer,指向cachebuf中的当前的位置
    __u32       left_len;   //length of left data in the cache，剩余的有效数据

    __s64       cache_pst;  //file offset accessed by user,用户读到的cache的地方在文件系统中的位置, 可以由left_len和file_pst推算出来
    __s64       file_pst;   //file offset accessed by cache fs，当前文件指针指向的地方
    __s64       file_size;  //文件总大小

} __cache_nfile_ptr_t;

/* 记录cachefs工作模式和相关参数的struct, byte align,字节对齐 */
typedef struct tag_CACHEFSNORMAL
{
    CacheFS CacheFSBase;
    __cache_nfile_ptr_t NFilePtr;
} CacheFSNormal;

extern CacheFSNormal *newCacheFSNormal(void);
extern void deleteCacheFSNormal(CacheFSNormal *pCacheFSNormal);

#endif //_CACHE_FS_NORMAL_H_

