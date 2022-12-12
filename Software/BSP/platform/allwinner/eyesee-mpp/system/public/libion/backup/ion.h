/*
 *  ion.c
 *
 * Memory Allocator functions for ion
 *
 *   Copyright 2011 Google, Inc
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef __SYS_CORE_ION_H
#define __SYS_CORE_ION_H

//#include <sys/types.h>
//#include <linux/ion.h>
//#include <linux/ion_sunxi.h>
//#include <kernel-headers/ion.h>
//#include <kernel-headers/ion_sunxi.h>
#include <linux/ion_uapi.h>
#include <linux/sunxi_ion_uapi.h>

//__BEGIN_DECLS

#define SZ_64M    0x04000000
#define SZ_4M     0x00400000
#define SZ_1M     0x00100000
#define SZ_64K    0x00010000
#define SZ_4k     0x00001000
#define SZ_1k     0x00000400

#define AW_ION_SYSTEM_HEAP_MASK           (1 << ION_HEAP_TYPE_SYSTEM)
#define AW_ION_SYSTEM_CONTIG_HEAP_MASK    (1 << ION_HEAP_TYPE_SYSTEM_CONTIG)
#define AW_ION_CARVEOUT_HEAP_MASK         (1 << ION_HEAP_TYPE_CARVEOUT)
#define AW_ION_CHUNK_HEAP_MASK            (1 << ION_HEAP_TYPE_CHUNK)
#define AW_ION_DMA_HEAP_MASK              (1 << ION_HEAP_TYPE_DMA)
#define AW_ION_CUSTOM_HEAP_MASK           (1 << ION_HEAP_TYPE_CUSTOM)


/* mappings of this buffer should be cached,
   ion will do cache maintenance when the buffer is mapped for dma */
#define AW_ION_CACHED_FLAG ION_FLAG_CACHED

/* mappings of this buffer will created at mmap time,
   if this is set caches must be managed manually */
#define AW_ION_CACHED_NEEDS_SYNC_FLAG ION_FLAG_CACHED_NEEDS_SYNC


//typedef int ion_user_handle_t;
typedef struct ion_fd_data  AW_ION_FD_DATA;

typedef struct tag_AW_ION_MEM_INFO_S {
    unsigned int phy;  //phisical address
    unsigned int vir;  //virtual address
    unsigned int size;  //buffer size
    unsigned int tee;   //
    struct ion_fd_data fd_data;
}AW_ION_MEM_INFO_S;



int ion_open();
int ion_close(int fd);
int ion_alloc(int fd, size_t len, size_t align, unsigned int heap_mask,
              unsigned int flags, ion_user_handle_t *handle);
int ion_alloc_fd(int fd, size_t len, size_t align, unsigned int heap_mask,
              unsigned int flags, int *handle_fd);
unsigned int ion_get_phyaddr(int fd, ion_user_handle_t handle, size_t size,
              unsigned int *pPhyAddr);

int ion_flush_cache(int fd, void* pVirAddr, unsigned int size);
int ion_sync_fd(int fd, int handle_fd);
int ion_free(int fd, ion_user_handle_t handle);
int ion_map(int fd, ion_user_handle_t handle, size_t length, int prot,
            int flags, off_t offset, unsigned char **ptr, int *map_fd);
int ion_share(int fd, ion_user_handle_t handle, int *share_fd);
int ion_import(int fd, int share_fd, ion_user_handle_t *handle);

//__END_DECLS

#endif /* __SYS_CORE_ION_H */
