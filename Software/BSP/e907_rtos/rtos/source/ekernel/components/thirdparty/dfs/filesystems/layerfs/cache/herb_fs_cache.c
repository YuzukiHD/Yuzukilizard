/*
********************************************************************************
*                                    eMOD
*                   the Easy Portable/Player Develop Kits
*                               mod_herb sub-system
*                          (module name, e.g.mpeg4 decoder plug-in) module
*
*          (c) Copyright 2010-2012, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : herb_fs_buf.c
* Version: V1.0
* By     : Eric_wang
* Date   : 2011-3-5
* Modify : zengzhijin 2020-04-24
* Description:
********************************************************************************
*/

#include "cache_fs_s.h"
#include "herb_fs_cache.h"
#include <string.h>
#include <kapi.h>
#include <log.h>
#include <stdlib.h>
#include <time.h>

extern __u32   module_id;
#define SYS_TIMEDELAY_UNIT (1000 / CONFIG_HZ)

/* 性能打印，如果出现abs,vbs满或空，muxer写flash速度不够时报警使用 */
#define SYS_LOAD_PRINT __msg

/*******************************************************************************
Function name: HERB_TimeDelay
Description:
    1.时间延迟. unit:ms
    2.封装操作系统的延迟函数。
    3.因为目前的系统的delay单位时间是10ms，所以输入值必须是10的倍数，否则
      不会精确delay.delay时间总是>=预计时间.
Parameters:

Return:

Time: 2009/12/25
*******************************************************************************/
static void HERB_TimeDelay(__s32 nDelay)
{
    if (nDelay)
    {
        esKRNL_TimeDly((nDelay + SYS_TIMEDELAY_UNIT - 1) / SYS_TIMEDELAY_UNIT);
    }

    return;
}

/*******************************************************************************
Function name: HERB_fopen
Description:
    1.打开文件，同时启动一个线程，负责写flash.
Parameters:
    1.要读取的成员变量:cur_ptr，另一个线程会修改，所以尽快读取。
    2.要修改的成员变量:head_ptr,file_pst.这些变量，是另一个线程所不会修改的。
Return:

Time: 2010/6/23
*******************************************************************************/
static void write_filesys_maintsk(void *p_arg)
{
    HerbFSCache *pHerbFSCache = (HerbFSCache *)p_arg;
    __herb_file_ptr_t  *pHerbFile = &pHerbFSCache->HerbFilePtr;
    __u8    *pCur;
    __u8    *pTmpHeadPtr = NULL;
    __s32   nDataSize;      // 总的有效的数据长度
    __s32   nDataSize1;     // 如果数据回头，第一段数据长度
    __s32   nDataSize2;
    __s32   nTmpDataSize;   //打算写入file system的数据长度,一般是HERB_CHUNK_SIZE
    __s32   nDelay = 0;
    __u32   nWtDataSize = 0;
    __s32   timeout_flag = -1;
    struct timespec abs_timeout;

    memset(&abs_timeout, 0, sizeof(struct timespec));

    while (1)
    {
        //check if need stop main task
        if (pHerbFile->stopflag)
        {
            //buffer内的数据全部写到file system.
            //HERB_fflush((ES_FILE*)pHerbFile);
            pHerbFSCache->fflush(pHerbFSCache);
            goto _exit_write_filesys_task;
        }

        //check if any request to delete main task
        if (esKRNL_TDelReq(EXEC_prioself) == OS_TASK_DEL_REQ)
        {
            //buffer内的数据全部写到file system.
            //HERB_fflush((ES_FILE*)pHerbFile);
            pHerbFSCache->fflush(pHerbFSCache);
            goto _exit_write_filesys_task;
        }

        //上锁，因为HERB_fflush()会由另一个线程调用写filesystem,所以这里要上锁
        pHerbFile->wrt_filesys_lock.lock(&pHerbFile->wrt_filesys_lock);
        pCur = pHerbFile->cur_ptr;

        if (pHerbFile->head_ptr <= pCur)
        {
            nDataSize1 = pCur - pHerbFile->head_ptr;
            nDataSize2 = 0;
        }
        else
        {
            nDataSize1 = pHerbFile->cache + pHerbFile->buf_size - pHerbFile->head_ptr;
            nDataSize2 = pHerbFile->cur_ptr - pHerbFile->cache;
        }

        nDataSize = nDataSize1 + nDataSize2;

        if (nDataSize < HERB_CHUNK_SIZE)
        {
            nDelay = WRT_ES_FILESYS_INTERVAL;
            goto _wrt_chunk_done;
        }
        else
        {
            nDelay = 0;
        }

        nTmpDataSize = HERB_CHUNK_SIZE;

        if (nDataSize1 >= nTmpDataSize) //直接写
        {
            nWtDataSize = esFSYS_fwrite(pHerbFile->head_ptr, 1, nTmpDataSize, pHerbFile->fp);
            //之所以用下面这段代码，是因为要保证pHerbFile->head_ptr一次赋值，另一个线程是要读取pHerbFile->head_ptr的数值的，
            //所以pHerbFile->head_ptr的值更改，必须原子操作。
            pTmpHeadPtr = pHerbFile->head_ptr;
            // pTmpHeadPtr += nTmpDataSize;
            pTmpHeadPtr += nWtDataSize;//nTmpDataSize;

            if (pTmpHeadPtr == pHerbFile->cache + pHerbFile->buf_size)
            {
                pTmpHeadPtr = pHerbFile->cache;
            }
            else if (pTmpHeadPtr > pHerbFile->cache + pHerbFile->buf_size)
            {
                __wrn("fatal error! exceed cache buffer\n");
            }

            pHerbFile->head_ptr = pTmpHeadPtr;
            //pHerbFile->file_pst += nTmpDataSize;
            pHerbFile->file_pst += nWtDataSize;//nTmpDataSize;

            if (nWtDataSize != nTmpDataSize)
            {
                __wrn("flash full! [%d]<[%d]\n", nWtDataSize, nTmpDataSize);
                pHerbFile->disk_full_flg = 1;
                //去锁,写完file system后,
                pHerbFile->wrt_filesys_lock.unlock(&pHerbFile->wrt_filesys_lock);
                goto _exit_write_filesys_task;
            }
        }
        else    //buffer需要回头,写入file system.
        {
            //先写完nDataSize1
            nWtDataSize = esFSYS_fwrite(pHerbFile->head_ptr, 1, nDataSize1, pHerbFile->fp);
            pHerbFile->head_ptr = pHerbFile->cache;
            pHerbFile->file_pst += nDataSize1;
            nTmpDataSize -= nDataSize1;

            if (nWtDataSize != nDataSize1)
            {
                __wrn("flash full2! [%d]<[%d]\n", nWtDataSize, nDataSize1);
                pHerbFile->disk_full_flg = 1;
                //去锁,写完file system后,
                pHerbFile->wrt_filesys_lock.unlock(&pHerbFile->wrt_filesys_lock);
                goto _exit_write_filesys_task;
            }

            //再写nDataSize2的一部分
            nWtDataSize = esFSYS_fwrite(pHerbFile->head_ptr, 1, nTmpDataSize, pHerbFile->fp);
            pHerbFile->head_ptr += nTmpDataSize;
            pHerbFile->file_pst += nTmpDataSize;

            if (nWtDataSize != nTmpDataSize)
            {
                __wrn("flash full3! [%d]<[%d]\n", nWtDataSize, nTmpDataSize);
                pHerbFile->disk_full_flg = 1;
                //去锁,写完file system后,
                pHerbFile->wrt_filesys_lock.unlock(&pHerbFile->wrt_filesys_lock);
                goto _exit_write_filesys_task;
            }
        }

_wrt_chunk_done:
        //去锁,写完file system后,
        pHerbFile->wrt_filesys_lock.unlock(&pHerbFile->wrt_filesys_lock);
        /* HERB_TimeDelay(nDelay); */

        if (timeout_flag == 0)
        {
            sem_post(&pHerbFile->fwrite_sem);
        }

        int clock_gettime(clockid_t clockid, struct timespec * tp);
        if (nDelay != 0)
        {
            if (clock_gettime(CLOCK_REALTIME, &abs_timeout) == 0)
            {
#define TICKS_TO_NS(x)  (x * 1000 * 1000 * (1000 / CONFIG_HZ))
                long tv_nsec = abs_timeout.tv_nsec + TICKS_TO_NS(nDelay);

                abs_timeout.tv_sec += tv_nsec / (1000 * 1000 * 1000L);
                abs_timeout.tv_nsec = tv_nsec % (1000 * 1000 * 1000L);

                timeout_flag = sem_timedwait(&pHerbFile->write_task_sem, &abs_timeout) == 0 ? 0 : 1;
            }
            else
            {
                sem_wait(&pHerbFile->write_task_sem);
                timeout_flag = 0;
            }
        }
        else
        {
            timeout_flag = 1;
        }
    }

_exit_write_filesys_task:
    __inf("--------------------------- write_filesys_maintsk exit!\n");
    sem_post(&pHerbFile->fwrite_sem);
    esKRNL_TDel(EXEC_prioself);
}

static ES_FILE *HerbFSCache_fopen(CacheFS *thiz, const __s8 *path, const __s8 *mode)
{
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    __herb_file_ptr_t  *tmpHerbFile = &pHerbFSCache->HerbFilePtr;

    if (!path || !mode)
    {
        __wrn("Parameter for open herb file is invalid!\n");
        goto _err0_herb_fopen;
    }

    tmpHerbFile->fp = esFSYS_fopen(path, mode);
    if (!tmpHerbFile->fp)
    {
        __wrn("Open herb file failed!\n");
        goto _err1_herb_fopen;
    }

    /* request memory for cache file data. */
    tmpHerbFile->cache = PHY_MALLOC((tmpHerbFile->buf_size + 1023), 1024);

    if (!tmpHerbFile->cache)
    {
        __wrn("Request memory for cache fs cache failed!\n");
        goto _err2_herb_fopen;
    }

    tmpHerbFile->head_ptr = tmpHerbFile->cache;
    tmpHerbFile->cur_ptr   = tmpHerbFile->cache;
    tmpHerbFile->file_pst  = eLIBs_flltell(tmpHerbFile->fp);
    tmpHerbFile->cache_pst = tmpHerbFile->file_pst;
    tmpHerbFile->write_total_size = 0;
    cache_lock_init(&pHerbFSCache->HerbFilePtr.wrt_filesys_lock);
    sem_init(&pHerbFSCache->HerbFilePtr.fwrite_sem, 0, 0);
    sem_init(&pHerbFSCache->HerbFilePtr.write_task_sem, 0, 0);

    /* 开线程 */
    tmpHerbFile->write_filesys_tskprio = esKRNL_TCreate(write_filesys_maintsk,
                                         pHerbFSCache,
                                         0x2000,
                                         (module_id << 8) | WRITE_ES_FILESYS_TASK_PRIO);

    if (0 == tmpHerbFile->write_filesys_tskprio)
    {
        __wrn("herb_fopen create task fail,open file fail!\n");
        goto _err3_herb_fopen;
    }

    return (ES_FILE *)tmpHerbFile;

_err3_herb_fopen:
    cache_lock_destroy(&pHerbFSCache->HerbFilePtr.wrt_filesys_lock);
    sem_destroy(&pHerbFSCache->HerbFilePtr.fwrite_sem);
    sem_destroy(&pHerbFSCache->HerbFilePtr.write_task_sem);

    if (tmpHerbFile->cache)
    {
        PHY_FREE(tmpHerbFile->cache);
        tmpHerbFile->cache = NULL;
    }

_err2_herb_fopen:
    esFSYS_fclose(tmpHerbFile->fp);
_err1_herb_fopen:
_err0_herb_fopen:
    return 0;
}
/*******************************************************************************
Function name: HERB_fclose
Description:
    1.关闭文件，要做事情有:
      (1) 销毁线程。
      (2) 释放cache.
      (3) 关闭文件.

Parameters:

Return:
     -1:失败
    0:成功
Time: 2010/6/24
*******************************************************************************/
static __s32 HerbFSCache_fclose(CacheFS *thiz)
{
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    __herb_file_ptr_t  *tmpHerbFile = &pHerbFSCache->HerbFilePtr;

    if (!tmpHerbFile->fp)
    {
        __wrn("file handle is invalid when close herb file!\n");
        return -1;
    }

    /* 销毁线程 */
    tmpHerbFile->stopflag = 1;

    if (tmpHerbFile->write_filesys_tskprio)
    {
        /* wake up file parser main task */
        /* esKRNL_TimeDlyResume(tmpHerbFile->write_filesys_tskprio); */
        sem_post(&tmpHerbFile->write_task_sem);

        while (esKRNL_TDelReq(tmpHerbFile->write_filesys_tskprio) != OS_TASK_NOT_EXIST)
        {
            /* esKRNL_TimeDlyResume(tmpHerbFile->write_filesys_tskprio); */
            sem_post(&tmpHerbFile->write_task_sem);
            esKRNL_TimeDly(1);
        }

        tmpHerbFile->write_filesys_tskprio = 0;
    }

    cache_lock_destroy(&tmpHerbFile->wrt_filesys_lock);
    sem_destroy(&tmpHerbFile->fwrite_sem);
    sem_destroy(&tmpHerbFile->write_task_sem);

    /* release cache buffer */
    if (tmpHerbFile->cache)
    {
        PHY_FREE(tmpHerbFile->cache);
        tmpHerbFile->cache = 0;
    }

    /* close file handle */
    if (tmpHerbFile->fp)
    {
        esFSYS_fclose(tmpHerbFile->fp);
        tmpHerbFile->fp = NULL;
    }

    /* reset herb file handle */
    memset(tmpHerbFile, 0, sizeof(__herb_file_ptr_t));

    return 0;
}

__s32 HerbFSCache_fread(CacheFS *thiz, void *buf, __s32 size, __s32 count)
{
    __wrn("HerbCache mode don't support fread!\n");
    return 0;
}

/*******************************************************************************
Function name: HERB_fwrite
Description:
    1.当cache buffer满之后，做法是等，等buffer有空间再写!
      因为不允许直接写文件系统，必须先写到buffer中。
    2.要读取的成员变量:head_ptr
      要修改的成员变量:cur_ptr,cache_pst,file_size.这些变量，是另一个线程所不会修改的。

Parameters:
    1.size:一个unit的大小。
    2.count:unit的数量。
Return:
    1. EPDK_FAIL; fatal error
    2. count written: 真正写完的数据块的数量
Time: 2010/6/25
*******************************************************************************/
static __s32 HerbFSCache_fwrite(CacheFS *thiz, void *buf, __s32 size, __s32 count)   //写文件
{
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    __herb_file_ptr_t  *tmpHerbFile = &pHerbFSCache->HerbFilePtr;
    __s32               tmpDataSize;    //要写进buffer的数据长度，字节
    __s32               nWtDataSize = 0;    //已写进buffer的字节长度
    __u8                *tmpUsrPtr; //待写数据的起始地址
    __u8    *phead;     //当前有效数据的起始地址
    __s32   nLeftLen;   //cache buffer剩余的空间
    __s32   nLeftLen1;   //cache buffer剩余的第一段空间，因为是ringbuffer，所以会回头
    __s32   nLeftLen2;   //cache buffer剩余的第二段空间，

    if (!tmpHerbFile || !size || !count)
    {
        __wrn("File handle or parameter is invalid when read file!\n");
        return 0;
    }

    tmpUsrPtr = (__u8 *)buf;
_write_once:

    tmpHerbFile->wrt_filesys_lock.lock(&tmpHerbFile->wrt_filesys_lock);

    if (tmpHerbFile->disk_full_flg)
    {
        __wrn("HERB FS DISK FULL!\n");
        return nWtDataSize / size;
    }

    phead = tmpHerbFile->head_ptr;

    if (tmpHerbFile->cur_ptr >= phead)
    {
        if (phead == tmpHerbFile->cache)
        {
            nLeftLen1 = tmpHerbFile->cache + tmpHerbFile->buf_size - tmpHerbFile->cur_ptr - 1;
            nLeftLen2 = 0;
        }
        else
        {
            nLeftLen1 = tmpHerbFile->cache + tmpHerbFile->buf_size - tmpHerbFile->cur_ptr;
            nLeftLen2 = phead - tmpHerbFile->cache - 1;
        }
    }
    else
    {
        nLeftLen1 = phead - tmpHerbFile->cur_ptr - 1;
        nLeftLen2 = 0;
    }

    nLeftLen = nLeftLen1 + nLeftLen2;
    tmpDataSize = size * count - nWtDataSize;

    //写入buffer
    if (tmpDataSize <= nLeftLen) //要写入缓冲区的数据量比剩余空间少，
    {
        if (tmpDataSize <= nLeftLen1) //第一段区域可以写完
        {
            memcpy(tmpHerbFile->cur_ptr, tmpUsrPtr, tmpDataSize);

            //MEM_CPY(tmpHerbFile->cur_ptr, tmpUsrPtr, tmpDataSize);
            //tmpHerbFile->cur_ptr的值必须一次赋对!因为有另一个线程要读取它，所以它的值不能赋错.
            if (tmpHerbFile->cur_ptr + tmpDataSize == tmpHerbFile->cache + tmpHerbFile->buf_size)
            {
                tmpHerbFile->cur_ptr = tmpHerbFile->cache;
            }
            else if (tmpHerbFile->cur_ptr + tmpDataSize > tmpHerbFile->cache + tmpHerbFile->buf_size)
            {
                __wrn("HERB FS fatal error!\n");
                tmpHerbFile->cur_ptr = tmpHerbFile->cache;
                tmpHerbFile->wrt_filesys_lock.unlock(&tmpHerbFile->wrt_filesys_lock);
                return EPDK_FAIL;
            }
            else
            {
                tmpHerbFile->cur_ptr += tmpDataSize;
            }

            tmpHerbFile->cache_pst += tmpDataSize;
            tmpHerbFile->write_total_size += tmpDataSize;
            tmpHerbFile->wrt_filesys_lock.unlock(&tmpHerbFile->wrt_filesys_lock);
            return count;
        }
        else //第一段区域写不完，需要再写第二段区域
        {
            //MEM_CPY(tmpHerbFile->cur_ptr, tmpUsrPtr, nLeftLen1);
            memcpy(tmpHerbFile->cur_ptr, tmpUsrPtr, nLeftLen1);
            tmpUsrPtr += nLeftLen1;
            tmpDataSize -= nLeftLen1;
            //MEM_CPY(tmpHerbFile->cache, tmpUsrPtr, tmpDataSize);
            memcpy(tmpHerbFile->cache, tmpUsrPtr, tmpDataSize);
            tmpHerbFile->cur_ptr = tmpHerbFile->cache + tmpDataSize;
            tmpHerbFile->cache_pst += (nLeftLen1 + tmpDataSize);
            tmpHerbFile->write_total_size += (nLeftLen1 + tmpDataSize);
            tmpHerbFile->wrt_filesys_lock.unlock(&tmpHerbFile->wrt_filesys_lock);
            return count;
        }
    }
    else //如果缓冲区写不下数据，先写满，再等。
    {
        if (nLeftLen1)
        {
            //MEM_CPY(tmpHerbFile->cur_ptr, tmpUsrPtr, nLeftLen1);
            memcpy(tmpHerbFile->cur_ptr, tmpUsrPtr, nLeftLen1);
            tmpUsrPtr += nLeftLen1;
            tmpDataSize -= nLeftLen1;

            if (tmpHerbFile->cur_ptr + nLeftLen1 == tmpHerbFile->cache + tmpHerbFile->buf_size)
            {
                tmpHerbFile->cur_ptr = tmpHerbFile->cache;
            }
            else if (tmpHerbFile->cur_ptr + nLeftLen1 > tmpHerbFile->cache + tmpHerbFile->buf_size)
            {
                __wrn("HERB FS write data fatal error!\n");
                tmpHerbFile->wrt_filesys_lock.unlock(&tmpHerbFile->wrt_filesys_lock);
                return EPDK_FAIL;
            }
            else
            {
                tmpHerbFile->cur_ptr += nLeftLen1;
            }

            tmpHerbFile->cache_pst += nLeftLen1;
            tmpHerbFile->write_total_size += nLeftLen1;
        }

        if (nLeftLen2)
        {
            memcpy(tmpHerbFile->cur_ptr, tmpUsrPtr, nLeftLen2);
            tmpUsrPtr += nLeftLen2;
            tmpDataSize -= nLeftLen2;
            tmpHerbFile->cur_ptr += nLeftLen2;
            tmpHerbFile->cache_pst += nLeftLen2;
            tmpHerbFile->write_total_size += nLeftLen2;
        }

        nWtDataSize += nLeftLen;
        tmpHerbFile->wrt_filesys_lock.unlock(&tmpHerbFile->wrt_filesys_lock);
        /* HERB_TimeDelay(WRT_CACHE_WAIT_DLY); */
        sem_post(&tmpHerbFile->write_task_sem);
        sem_wait(&tmpHerbFile->fwrite_sem);
        SYS_LOAD_PRINT("[LOADTEST]herb fs write wait [%d]ms\n", WRT_CACHE_WAIT_DLY);
        goto _write_once; //在delay过程中，另一个线程会改变tmpHerbFile->head_ptr.
    }
}
/*******************************************************************************
Function name: HerbFSCache_fseek
Description:
    1. fseek的做法是:先调用fflush()，把缓冲区的数据写完，然后再fseek到指定的地方.
    2. fseek和fread不能并发调用.这由调用者保证.
Parameters:

Return:

Time: 2011/3/6
*******************************************************************************/
static __s32 HerbFSCache_fseek(CacheFS *thiz, __s64 offset, __s32 origin)   //fseek
{
    __s32   ret;
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    __herb_file_ptr_t *pHerbFile = &pHerbFSCache->HerbFilePtr;
    ret = pHerbFSCache->fflush(pHerbFSCache);

    if (EPDK_OK != ret)
    {
        __wrn("fseek,fflush() fail\n");
        return EPDK_FAIL;
    }

    ret = eLIBs_fllseek(pHerbFile->fp, offset, origin);
    pHerbFile->file_pst = pHerbFile->cache_pst = eLIBs_flltell(pHerbFile->fp);

    if (0 != ret)
    {
        __wrn("fatal error! fseek fail\n");
    }

    return ret;
}

static __s32 HerbFSCache_ftruncate(CacheFS *thiz, off_t length)
{
    __s32   ret;
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    __herb_file_ptr_t *pHerbFile = &pHerbFSCache->HerbFilePtr;

    ret = pHerbFSCache->fflush(pHerbFSCache);
    if (EPDK_OK != ret)
    {
        __wrn("HerbFSCache_ftruncate, fflush() fail\n");
        return EPDK_FAIL;
    }

    ret = esFSYS_ftruncate(pHerbFile->fp, length);
    if (EPDK_OK == ret)
    {
        pHerbFile->file_pst = pHerbFile->cache_pst = eLIBs_flltell(pHerbFile->fp);
    }
    else
    {
        __wrn("fatal error! ftruncate fail\n");
    }

    return ret;
}

static __s32 HerbFSCache_fsync(CacheFS *thiz)
{
    __s32   ret;
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    __herb_file_ptr_t *pHerbFile = &pHerbFSCache->HerbFilePtr;

    ret = pHerbFSCache->fflush(pHerbFSCache);
    if (EPDK_OK != ret)
    {
        __wrn("HerbFSCache_ftruncate, fflush() fail\n");
        return EPDK_FAIL;
    }

    ret = esFSYS_fsync(pHerbFile->fp);
    if (EPDK_OK == ret)
    {
        pHerbFile->file_pst = pHerbFile->cache_pst = eLIBs_flltell(pHerbFile->fp);
    }
    else
    {
        __wrn("fatal error! fsync fail\n");
    }

    return ret;
}

static __s32 HerbFSCache_fstat(CacheFS *thiz, void *buf)
{
    __s32 ret = 0;
    __ES_FSTAT *stat_buf = (__ES_FSTAT *)buf;
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    __herb_file_ptr_t *pHerbFile = &pHerbFSCache->HerbFilePtr;

    stat_buf->pos = pHerbFile->cache_pst;
    stat_buf->size = pHerbFile->cache_pst;

    return ret;
}

static __s64 HerbFSCache_ftell(CacheFS *thiz)
{
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    return pHerbFSCache->HerbFilePtr.cache_pst;
}

/* 适用于mode = HERB_FS_WORK_MODE_CACHE等有buffer的模式 */
static __s32 HerbFSCache_SetCacheSize(CacheFS *thiz, __s32 cache_size)
{
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    pHerbFSCache->HerbFilePtr.buf_size = cache_size;
    return EPDK_OK;
}

static ES_FILE *HerbFSCache_GetFp(CacheFS *thiz)
{
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    return pHerbFSCache->HerbFilePtr.fp;
}

/*******************************************************************************
Function name: HERB_fflush
Description:
    1.把cache buffer中的数据全部写到file system中去。
Parameters:
    1.要读取的成员变量:cur_ptr，现在有互斥信号量保护.
    2.要修改的成员变量:head_ptr,file_pst.这些变量，是另一个线程所不会修改的。
Return:
    1. EPDK_OK,
    2.EPDK_FAIL;
Time: 2010/6/28
*******************************************************************************/
static __s32 Impl_HerbFSCache_fflush(HerbFSCache *thiz)     //把cache中的数据全部写到文件系统
{
    HerbFSCache *pHerbFSCache = (HerbFSCache *)thiz;
    __herb_file_ptr_t  *pHerbFile = &pHerbFSCache->HerbFilePtr;
    __s32   ret = EPDK_OK;
    __u8    *pCur;  //指向当前的有效数据的下一字节，即将要写入的buf的位置
    __s32   nDataSize1;
    __s32   nDataSize2;

    if (pHerbFile->disk_full_flg)
    {
        __wrn("fflush detect disk full\n");
        return EPDK_FAIL;
    }

    pHerbFile->wrt_filesys_lock.lock(&pHerbFile->wrt_filesys_lock);
    pCur = pHerbFile->cur_ptr;

    if (pHerbFile->head_ptr <= pCur)
    {
        nDataSize1 = pCur - pHerbFile->head_ptr;
        nDataSize2 = 0;
    }
    else
    {
        nDataSize1 = pHerbFile->cache + pHerbFile->buf_size - pHerbFile->head_ptr;
        nDataSize2 = pHerbFile->cur_ptr - pHerbFile->cache;
    }

    //tmpDataSize = nDataSize1 + nDataSize2;

    //开始写file system
    if (nDataSize1)
    {
        esFSYS_fwrite(pHerbFile->head_ptr, 1, nDataSize1, pHerbFile->fp);

        if (pHerbFile->head_ptr + nDataSize1 == pHerbFile->cache + pHerbFile->buf_size)
        {
            pHerbFile->head_ptr = pHerbFile->cache;
            pHerbFile->file_pst += nDataSize1;
        }
        else if (pHerbFile->head_ptr + nDataSize1 < pHerbFile->cache + pHerbFile->buf_size)
        {
            pHerbFile->head_ptr += nDataSize1;
            pHerbFile->file_pst += nDataSize1;
        }
        else
        {
            __wrn("HERB_fflush() fatal error!\n");
            ret = EPDK_FAIL;
            goto _quit_flush;
        }
    }

    if (nDataSize2)
    {
        if (0 == nDataSize1)
        {
            __wrn("fatal error, nDataSize1=%d, nDataSize2=%d\n", nDataSize1, nDataSize2);
            ret = EPDK_FAIL;
            goto _quit_flush;
        }

        esFSYS_fwrite(pHerbFile->head_ptr, 1, nDataSize2, pHerbFile->fp);
        pHerbFile->head_ptr += nDataSize2;
        pHerbFile->file_pst += nDataSize2;
    }

    ret = EPDK_OK;
_quit_flush:
    pHerbFile->wrt_filesys_lock.unlock(&pHerbFile->wrt_filesys_lock);
    return ret;
}

HerbFSCache *newHerbFSCache()
{
    HerbFSCache *pHerbFSCache = (HerbFSCache *)malloc(sizeof(HerbFSCache));

    if (NULL == pHerbFSCache)
    {
        __wrn("NewHerbFSCache() malloc fail\n");
        return NULL;
    }

    memset(pHerbFSCache, 0, sizeof(HerbFSCache));
    CacheFS_Init(&pHerbFSCache->base);

    pHerbFSCache->base.cache_fs_work_mode = CACHE_FS_WORK_MODE_HERBCACHE;
    pHerbFSCache->base.cachefs_fopen = HerbFSCache_fopen;
    pHerbFSCache->base.cachefs_fclose = HerbFSCache_fclose;
    pHerbFSCache->base.cachefs_fread = HerbFSCache_fread;
    pHerbFSCache->base.cachefs_fwrite = HerbFSCache_fwrite;
    pHerbFSCache->base.cachefs_fseek = HerbFSCache_fseek;
    pHerbFSCache->base.cachefs_ftell = HerbFSCache_ftell;
    pHerbFSCache->base.cachefs_fsync = HerbFSCache_fsync;
    pHerbFSCache->base.cachefs_fstat = HerbFSCache_fstat;
    pHerbFSCache->base.cachefs_ftruncate = HerbFSCache_ftruncate;
    pHerbFSCache->base.SetCacheSize = HerbFSCache_SetCacheSize;
    pHerbFSCache->base.GetFp = HerbFSCache_GetFp;

    memset(&pHerbFSCache->HerbFilePtr, 0, sizeof(__herb_file_ptr_t));
    pHerbFSCache->HerbFilePtr.buf_size = HERB_FS_CACHE_SIZE;
    pHerbFSCache->fflush = Impl_HerbFSCache_fflush;
    return pHerbFSCache;
}

void deleteHerbFSCache(HerbFSCache *pHerbFSCache)
{
    free(pHerbFSCache);
}
