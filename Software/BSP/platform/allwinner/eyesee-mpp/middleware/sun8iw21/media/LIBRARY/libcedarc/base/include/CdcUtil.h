/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdcUtil.h
* Description :
* History :
*   Author  :
*   Date    : 2016/08/31
*   Comment :
*
*
*/

#ifndef CDC_UTIL_H__
#define CDC_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#include <asm-generic/ioctl.h>
#include "CdcMessageQueue.h"
#include "veInterface.h"
#include "cdc_log.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ALIGN_XXB(y, x) (((x) + ((y)-1)) & ~((y)-1))


#if (defined CONF_KERNEL_VERSION_3_10) ||      \
    (defined CONF_KERNEL_VERSION_4_4) ||       \
    (defined CONF_KERNEL_VERSION_4_9)
    typedef int aw_ion_user_handle_t;
    #define ION_USER_HANDLE_INIT_VALUE (-1)
#else
    typedef void* aw_ion_user_handle_t;
    #define ION_USER_HANDLE_INIT_VALUE (NULL)
#endif

typedef struct aw_ion_allocation_info
{
    size_t aw_len;
    size_t aw_align;
    unsigned int aw_heap_id_mask;
    unsigned int flags;
    aw_ion_user_handle_t handle;
} aw_ion_allocation_info_t;

typedef struct aw_ion_handle_data
{
    aw_ion_user_handle_t handle;
}ion_handle_data_t ;

typedef struct aw_ion_fd_data
{
    aw_ion_user_handle_t handle;
    int aw_fd;
}ion_fd_data_t;

typedef struct aw_ion_custom_info
{
    unsigned int aw_cmd;
    unsigned long aw_arg;
}ion_custom_data_t;

typedef struct CDC_SUNXI_PHYS_DATA
{
    aw_ion_user_handle_t handle;
    unsigned int  phys_addr;
    unsigned int  size;
}cdc_sunxi_phys_data;

typedef struct {
    long    start;
    long    end;
}sunxi_cache_range;

#define SZ_64M        0x04000000
#define SZ_4M        0x00400000
#define SZ_1M        0x00100000
#define SZ_64K        0x00010000
#define SZ_4k       0x00001000
#define SZ_1k       0x00000400

enum aw_ion_heap_type {
    AW_ION_SYSTEM_HEAP_TYPE,
    AW_ION_SYSTEM_CONTIG_HEAP_TYPE,
    AW_ION_CARVEOUT_HEAP_TYPE,
    AW_ION_TYPE_HEAP_CHUNK,
    AW_ION_TYPE_HEAP_DMA,
    AW_ION_TYPE_HEAP_CUSTOM,

    AW_ION_TYPE_HEAP_SECURE,

    AW_ION_NUM_HEAPS = 16,/* must be last so device specific heaps always
                              are at the end of this enum */
};

#define AW_MEM_ION_IOC_MAGIC        'I'
#define AW_MEM_ION_IOC_ALLOC        _IOWR(AW_MEM_ION_IOC_MAGIC, 0, struct aw_ion_allocation_info)
#define AW_MEM_ION_IOC_FREE        _IOWR(AW_MEM_ION_IOC_MAGIC, 1, struct aw_ion_handle_data)
#define AW_MEM_ION_IOC_MAP            _IOWR(AW_MEM_ION_IOC_MAGIC, 2, struct aw_ion_fd_data)
#define AW_MEM_ION_IOC_SHARE        _IOWR(AW_MEM_ION_IOC_MAGIC, 4, struct aw_ion_fd_data)
#define AW_MEM_ION_IOC_IMPORT        _IOWR(AW_MEM_ION_IOC_MAGIC, 5, struct aw_ion_fd_data)
#define AW_MEM_ION_IOC_SYNC        _IOWR(AW_MEM_ION_IOC_MAGIC, 7, struct aw_ion_fd_data)
#define AW_MEM_ION_IOC_CUSTOM        _IOWR(AW_MEM_ION_IOC_MAGIC, 6, struct aw_ion_custom_info)

#define AW_ION_CACHED_FLAG 1
/* mappings of this buffer should be cached,
ion will do cache maintenance when the buffer is mapped for dma */
#define AW_ION_CACHED_NEEDS_SYNC_FLAG 2
/* mappings of this buffer will created at mmap time,
if this is set caches must be managed manually */

#define ION_IOC_SUNXI_FLUSH_RANGE           5
#define ION_IOC_SUNXI_FLUSH_ALL             6
#define ION_IOC_SUNXI_PHYS_ADDR             7
#define ION_IOC_SUNXI_DMA_COPY              8

#define ION_IOC_SUNXI_TEE_ADDR              17
#define AW_ION_SYSTEM_HEAP_MASK        (1 << AW_ION_SYSTEM_HEAP_TYPE)
#define AW_ION_SYSTEM_CONTIG_HEAP_MASK    (1 << ION_HEAP_TYPE_SYSTEM_CONTIG)
#define AW_ION_CARVEOUT_HEAP_MASK        (1 << AW_ION_CARVEOUT_HEAP_TYPE)
#define AW_ION_DMA_HEAP_MASK        (1 << AW_ION_TYPE_HEAP_DMA)
#define AW_ION_SECURE_HEAP_MASK      (1 << AW_ION_TYPE_HEAP_SECURE)

#define  BASE_PROC_BUF_LEN          (2*1024)
#define  ADVANCE_PROC_BUF_LEN       (2*1024)

typedef enum MEMORY_TYPE {
    MEMORY_NORMAL,
    MEMORY_IOMMU,
}MEMORY_TYPE;

typedef struct ve_lbc_in_param
{
    eVeLbcMode   eLbcMode;
    unsigned int nWidht;
    unsigned int nHeight;
}ve_lbc_in_param;

typedef struct ve_lbc_out_param
{
    unsigned int buffer_size;
    unsigned int seg_tar_bits;
    unsigned int segline_tar_bits;
    unsigned int is_lossy;
    unsigned int seg_rc_en;
}ve_lbc_out_param;

int ComputeLbcParameter(ve_lbc_in_param* in_param, ve_lbc_out_param* out_param);
int procInfo_initMem(struct ve_channel_proc_info *proc_info_mgr);
void procInfo_DestroyMem(struct ve_channel_proc_info *proc_info_mgr);

unsigned long CdcIonGetPhyAdr(int fd, uintptr_t handle);
int CdcIonGetFd(int fd, uintptr_t handle);
int CdcIonImport(int fd, int share_fd, aw_ion_user_handle_t *ion_handle);
int CdcIonFree(int fd, aw_ion_user_handle_t ion_handle);
int CdcIonOpen(void);
int CdcIonClose(int fd);
int CdcIonMmap(int buf_fd, size_t len, unsigned char **pVirAddr);
int CdcIonUnmap(size_t len, unsigned char *pVirAddr);
int CdcIonGetMemType();
unsigned long CdcIonGetTEEAdr(int fd, uintptr_t handle);

#ifdef __cplusplus
}
#endif

#endif
