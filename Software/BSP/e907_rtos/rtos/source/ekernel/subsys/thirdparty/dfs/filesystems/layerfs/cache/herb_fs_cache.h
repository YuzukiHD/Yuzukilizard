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
* File   : herb_fs_cache.h
* Version: V1.0
* By     : Eric_wang
* Date   : 2011-3-6
* Modify : zengzhijin 2020-04-24
* Description:
    带缓冲机制的herbfs,写入buffer中，再由单独的线程负责从buffer写入file system.
    如果并发写,调用者自己做好互斥,本类不负责并发写的互斥.
********************************************************************************
*/
#ifndef _HERB_FS_CACHE_H_
#define _HERB_FS_CACHE_H_
#include "cache_fs_base.h"
#include "cache_lock.h"
#include <semaphore.h>

/* 负责写入文件系统，必须是最高优先级，以保证编码完成的数据能及时写入，不至于使ABS, VBS的数据溢出 */
#define WRITE_ES_FILESYS_TASK_PRIO     KRNL_priolevel1

#define HERB_FS_CACHE_SIZE (6*1024*1024)    //cache size的默认值.
#define HERB_CHUNK_SIZE (256*1024)  //一次写入flash的数据量
#define WRT_CACHE_WAIT_DLY (50)  //unit:ms, 如果cache buffer满，查询的时间间隔.
//#define WRT_ES_FILESYS_WAIT_DLY (50) //unit:ms, 如果遇到其他线程也在写文件系统,delay的时间间隔
#define WRT_ES_FILESYS_INTERVAL (100) //unit:ms,write线程出现数据不够的情况下,等待下一次写的时间间隔.


typedef struct tag_HerbFSCache HerbFSCache;

/* 把cache中的数据全部写到文件系统 */
typedef __s32(*HerbFSCache_fflush)(HerbFSCache *thiz);

typedef struct __HERB_ES_FILE_PTR
{
    ES_FILE     *fp;        //file pointer for read file

    __u8        *cache;     //buffer for cache file data, should be palloc,
    __u32       buf_size;   //size of cache buffer, user define, HERB_FS_CACHE_SIZE, 真正有效的数据长度是buf_size-1，以避免cur_ptr和head_ptr的重合引起的歧义

    __u8        *head_ptr;  //缓冲区的有效数据的开始地址。
    __u8        *cur_ptr;   //指向有效数据地址的下一个字节(立刻回头). current access pointer in the cache buffer
    __s64       file_pst;   //truely file offset accessed by cache fs, data_ptr map file_pst.写文件时，在文件系统中真正写到的地方
    __s64       cache_pst;  //current access pointer map the file offset,加上cache,用户以为写进文件系统的地方
    __s64       write_total_size;  //current file writing size,(include cache's valid data)，加入fseek功能后，已经不准了，仅仅用于统计总共写了多少数据, 重复写的也统计在内了。没有太大意义了.

    __u8        stopflag;//终止线程的标记变量
    __u32       write_filesys_tskprio;
    cache_lock_t wrt_filesys_lock;
    sem_t       fwrite_sem;
    sem_t       write_task_sem;

    __u8        disk_full_flg;  //0:not full; 1:disk full
} __herb_file_ptr_t;

typedef struct tag_HerbFSCache  //记录herbfs工作模式和相关参数的struct
{
    CacheFS base;
    __herb_file_ptr_t HerbFilePtr;
    HerbFSCache_fflush   fflush;
} HerbFSCache;

extern HerbFSCache *newHerbFSCache(void);
extern void deleteHerbFSCache(HerbFSCache *pHerbFSCache);

#endif  /* _HERB_FS_CACHE_H_ */

