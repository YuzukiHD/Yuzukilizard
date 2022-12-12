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
#define LOG_TAG "ion_memmanager"

#include <cutils/log.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include <sys_linux_ioctl.h>
#include <linux/ion_uapi.h>
#include <linux/sunxi_ion_uapi.h>
#include <linux/cedar_ve_uapi.h>

#include <ion_memmanager.h>

#include "cdx_list.h"
//#include "plat_log.h"

//#include "./include/ion/ion.h"

#define SZ_64M    0x04000000
#define SZ_4M     0x00400000
#define SZ_1M     0x00100000
#define SZ_64K    0x00010000
#define SZ_4k     0x00001000
#define SZ_1k     0x00000400

#define ION_ALLOC_ALIGN  SZ_4k

#define ION_DEV_NAME        "/dev/ion"
#define CEDAR_DEV_NAME      "/dev/cedar_dev"

typedef struct tag_AW_ION_MEM_INFO_S {
    IonHeapType mIonHeapType;
    int mbSupportCache;
    unsigned int mAlign;
    unsigned int phy;  //phisical address
    unsigned int vir;  //virtual address
    unsigned int size;  //buffer size
    unsigned int tee;   //
    struct ion_fd_data fd_data;
}AW_ION_MEM_INFO_S;

typedef struct tag_ion_mem_node
{
    AW_ION_MEM_INFO_S mem_info;
    struct list_head mList;
}ION_MEM_NODE_S;

typedef struct tag_ion_mem_manager
{
    int dev_fd;
    int cedar_fd;
    int mbIommuFlag;    //1: iommu is enable, 0:disable
    struct list_head mMemList;  //ION_MEM_NODE_S
    pthread_mutex_t mLock;
    //pthread_mutex_t mRefLock;
    int ref_cnt;
}ION_MEM_MANAGER_S;

static ION_MEM_MANAGER_S *g_mem_manager = NULL;
static pthread_mutex_t g_mutex_alloc = PTHREAD_MUTEX_INITIALIZER;

/**
 * @return: 1: enabled, 0:disabled.
 */
static int is_iommu_enabled(void)
{
    struct stat iommu_sysfs;
    if (stat("/sys/class/iommu", &iommu_sysfs) == 0 && S_ISDIR(iommu_sysfs.st_mode))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static int searchExistMemList(void *vir_ptr, ION_MEM_NODE_S **pNode)
{
    int ret = -1;
    ION_MEM_NODE_S *pEntry, *pTmp;

    pthread_mutex_lock(&g_mem_manager->mLock);
    if (list_empty(&g_mem_manager->mMemList))
    {
        pthread_mutex_unlock(&g_mem_manager->mLock);
        *pNode = NULL;
        return ret;
    }

    list_for_each_entry_safe(pEntry, pTmp, &g_mem_manager->mMemList, mList)
    {
        if (pEntry->mem_info.vir == (unsigned int)vir_ptr)
        {
            ret = 0;
            *pNode = pEntry;
            //list_del(&pEntry->mList);
            break;
        }
    }
    pthread_mutex_unlock(&g_mem_manager->mLock);

    return ret;
}

static int ion_freeMem_l(ION_MEM_NODE_S *pEntry)
{
    int ret = -1;
    if(pEntry->mem_info.mIonHeapType == IonHeapType_IOMMU)
    {
        struct user_iommu_param stUserIommuBuf;
        memset(&stUserIommuBuf, 0, sizeof(struct user_iommu_param));
        stUserIommuBuf.fd = pEntry->mem_info.fd_data.fd;
        ret = ioctl(g_mem_manager->cedar_fd, IOCTL_FREE_IOMMU_ADDR, &stUserIommuBuf);
        if(ret != 0)
        {
            ALOGE("(f:%s, l:%d) fatal error! free iommu addr fail[%d]", __FUNCTION__, __LINE__, ret);
        }
        ret = ioctl(g_mem_manager->cedar_fd, IOCTL_ENGINE_REL, 0);
        if(ret != 0)
        {
            ALOGE("fatal error! ENGINE_REL err, ret %d", ret);
        }
    }
    if (munmap((void *)(pEntry->mem_info.vir), pEntry->mem_info.size) < 0)
    {
        ALOGE("munmap 0x%p, size: %d failed", (void*)pEntry->mem_info.vir, pEntry->mem_info.size);
    }

    /*close dma buffer fd*/
    close(pEntry->mem_info.fd_data.fd);
    pEntry->mem_info.fd_data.fd = -1;
    struct ion_handle_data handleData = {
        .handle = pEntry->mem_info.fd_data.handle,
    };
    ret = ioctl(g_mem_manager->dev_fd, ION_IOC_FREE, &handleData);
    if (ret != 0) 
    {
        ALOGE("ioctl ION IOC FREE failed with code %d: %s\n", ret, strerror(errno));
        //return -errno;
    }
    return ret;
}

int ion_memOpen(void)
{
    int ret = 0;

    pthread_mutex_lock(&g_mutex_alloc);

    if (g_mem_manager != NULL)
    {
        ALOGD("ion allocator has already been created");
        goto SUCCEED_OUT;
    }

    g_mem_manager = (ION_MEM_MANAGER_S *)malloc(sizeof(ION_MEM_MANAGER_S));
    if (NULL == g_mem_manager)
    {
        ALOGE("no mem! open fail");
        goto _err0;
        //pthread_mutex_unlock(&g_mutex_alloc);
        //return -1;
    }
    memset(g_mem_manager, 0, sizeof(ION_MEM_MANAGER_S));
    g_mem_manager->dev_fd = -1;
    g_mem_manager->cedar_fd = -1;
    INIT_LIST_HEAD(&g_mem_manager->mMemList);
    pthread_mutex_init(&g_mem_manager->mLock, NULL);

    g_mem_manager->dev_fd = open(ION_DEV_NAME, O_RDWR); 
    if (g_mem_manager->dev_fd <= 0)
    {
        ALOGE("ion open fail");
        goto _err1;
    }
    g_mem_manager->mbIommuFlag = is_iommu_enabled();
    if(g_mem_manager->mbIommuFlag != 0)
    {
        g_mem_manager->cedar_fd = open(CEDAR_DEV_NAME, O_RDONLY, 0);
        if (g_mem_manager->cedar_fd < 0) 
        {
            ALOGE("%s(%d) err: open %s dev failed", __FUNCTION__, __LINE__, CEDAR_DEV_NAME);
            goto _err2;
        }
    }

SUCCEED_OUT:
    //pthread_mutex_lock(&g_mem_manager->mRefLock);
    g_mem_manager->ref_cnt++;
    //pthread_mutex_unlock(&g_mem_manager->mRefLock);

    pthread_mutex_unlock(&g_mutex_alloc);
    return ret;

FAIL_OUT:
_err2:
    close(g_mem_manager->dev_fd);
    g_mem_manager->dev_fd = -1;
_err1:
    pthread_mutex_destroy(&g_mem_manager->mLock);
    free(g_mem_manager);
    g_mem_manager = NULL;
_err0:
    pthread_mutex_unlock(&g_mutex_alloc);
    return -1;
}

int ion_memClose(void)
{
    int ret = 0;

    pthread_mutex_lock(&g_mutex_alloc);

    if (g_mem_manager == NULL)
    {
        ALOGW("has not open, please open first!");
        pthread_mutex_unlock(&g_mutex_alloc);
        return ret;
    }

    if (--g_mem_manager->ref_cnt <= 0)
    {
        if (g_mem_manager->ref_cnt < 0)
        {
            ALOGW("maybe close more times than open");
        }
        
        ION_MEM_NODE_S *pEntry, *pTmp;
        pthread_mutex_lock(&g_mem_manager->mLock);
        if (!list_empty(&g_mem_manager->mMemList))
        {
            ALOGE("fatal error! why some ion mem still in list??? force to free!");
            list_for_each_entry_safe(pEntry, pTmp, &g_mem_manager->mMemList, mList)
            {
                list_del(&pEntry->mList);

                ion_freeMem_l(pEntry);
                free(pEntry);
            }
        }
        pthread_mutex_unlock(&g_mem_manager->mLock);

        if (g_mem_manager->dev_fd >= 0)
        {
            if (close(g_mem_manager->dev_fd) != 0)
            {
                ALOGE("ion close fail");
                //ret = -1;
            }
            g_mem_manager->dev_fd = -1;
        }
        if(g_mem_manager->mbIommuFlag != 0)
        {
            if (g_mem_manager->cedar_fd >= 0)
            {
                if (close(g_mem_manager->cedar_fd ) != 0)
                {
                    ALOGE("ion cedar dev close fail");
                }
                g_mem_manager->cedar_fd  = -1;
            }
        }
        pthread_mutex_destroy(&g_mem_manager->mLock);

        free(g_mem_manager);
        g_mem_manager = NULL;
    }
    else
    {
        ALOGD("still left %d in use ", g_mem_manager->ref_cnt);
    }

    pthread_mutex_unlock(&g_mutex_alloc);

    return ret;
}

unsigned char* ion_allocMem(unsigned int size)
{
    IonAllocAttr stAttr;
    memset(&stAttr, 0, sizeof(IonAllocAttr));
    stAttr.mLen = size;
    stAttr.mAlign = ION_ALLOC_ALIGN;
    stAttr.mIonHeapType = IonHeapType_CARVEOUT;
    stAttr.mbSupportCache = 1;
    return ion_allocMem_extend(&stAttr);
}

unsigned char* ion_allocMem_extend(IonAllocAttr *pAttr)
{
    int ret;
    ion_user_handle_t handle = -1;
    int map_fd = -1;
    unsigned char *vir_ptr = NULL;
    unsigned int phy_addr = 0;

    if (0 == pAttr->mLen)
    {
        ALOGE("size error!");
        return NULL;
    }
    if(IonHeapType_IOMMU == pAttr->mIonHeapType)
    {
        if(0 == g_mem_manager->mbIommuFlag)
        {
            ALOGE("(f:%s, l:%d) fatal error! iommu is disable", __FUNCTION__, __LINE__);
            return NULL;
        }
    }
    ION_MEM_NODE_S *pMemNode;
    pMemNode = (ION_MEM_NODE_S *)malloc(sizeof(ION_MEM_NODE_S));
    if (NULL == pMemNode)
    {
        ALOGE("no mem! alloc fail");
        return NULL;
    }
    memset(pMemNode, 0, sizeof(ION_MEM_NODE_S));

    struct ion_allocation_data allocData;
    memset(&allocData, 0, sizeof(struct ion_allocation_data));
    allocData.len = pAttr->mLen;
    allocData.align = pAttr->mAlign;
    switch(pAttr->mIonHeapType)
    {
        case IonHeapType_CARVEOUT:
        {
            allocData.heap_id_mask = ION_HEAP_CARVEOUT_MASK | ION_HEAP_TYPE_DMA_MASK;
            break;
        }
        case IonHeapType_IOMMU:
        {
            allocData.heap_id_mask = ION_HEAP_SYSTEM_MASK;
            break;
        }
        default:
        {
            ALOGE("fatal error! unknown ion heap type:%d", pAttr->mIonHeapType);
            allocData.heap_id_mask = ION_HEAP_CARVEOUT_MASK;
            break;
        }
    }
    if(pAttr->mbSupportCache)
    {
        allocData.flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
    }
    else
    {
        allocData.flags = 0;
    }
    ret = ioctl(g_mem_manager->dev_fd, ION_IOC_ALLOC, &allocData);
    if (ret != 0)
    {
        ALOGE("ion alloc fail!");
        goto _err0;
//        free(pMemNode);
//        return NULL;
    }
    handle = allocData.handle;

    struct ion_fd_data fdData;
    memset(&fdData, 0, sizeof(struct ion_fd_data));
    fdData.handle = handle;
    ret = ioctl(g_mem_manager->dev_fd, ION_IOC_MAP, &fdData);
    if (0 == ret)
    {
        if (fdData.fd >= 0)
        {
            map_fd = fdData.fd;
            vir_ptr = mmap(NULL, pAttr->mLen, PROT_READ|PROT_WRITE, MAP_SHARED, fdData.fd, 0);
            if (vir_ptr == MAP_FAILED) 
            {
                ALOGE("mmap failed: %s", strerror(errno));
                close(map_fd);
                map_fd = -1;
                ret = -errno;
            }
        }
        else
        {
            ALOGE("map ioctl returned negative fd\n");
            ret = -EINVAL;
        }
    }
    if ((ret !=0) || (vir_ptr==NULL))
    {
        ALOGE("ion map fail");
        goto _err1;
    }

    //get phy addr, iommu is different with carveout/cma.
    if(pAttr->mIonHeapType != IonHeapType_IOMMU)
    {
        struct sunxi_phys_data phys_data = {0};
        struct ion_custom_data custom_data = {0};
        phys_data.handle = handle;
        phys_data.size = pAttr->mLen;
        custom_data.cmd = ION_IOC_SUNXI_PHYS_ADDR;
        custom_data.arg = (unsigned long)&phys_data;
        ret = ioctl(g_mem_manager->dev_fd, ION_IOC_CUSTOM, &custom_data);
        if (ret < 0) 
        {
            ALOGE("ION_IOC_CUSTOM to get phyaddr failed");
            goto _err2;
//            /* unmap */
//            if (munmap((void *)vir_ptr, pAttr->mLen) < 0)
//            {
//                ALOGE("munmap 0x%p, size: %d failed", (void*)vir_ptr, pAttr->mLen);
//            }
//            /*close dma buffer fd*/
//            close(map_fd);
//            map_fd = -1;
//            /* ion free */
//            struct ion_handle_data handleData = {
//                .handle = handle,
//            };
//            ioctl(g_mem_manager->dev_fd, ION_IOC_FREE, &handleData);
//            return NULL;
        }
        else
        {
            phy_addr = phys_data.phys_addr;
        }
    }
    else
    {
        ret = ioctl(g_mem_manager->cedar_fd, IOCTL_ENGINE_REQ, 0);
        if (0 == ret)
        {
            struct user_iommu_param stUserIommuBuf;
            memset(&stUserIommuBuf, 0, sizeof(struct user_iommu_param));
            stUserIommuBuf.fd = map_fd;
            ret = ioctl(g_mem_manager->cedar_fd, IOCTL_GET_IOMMU_ADDR, &stUserIommuBuf);
            if(ret != 0)
            {
                ALOGE("get iommu addr fail! ret:[%d]", ret);
                ret = ioctl(g_mem_manager->cedar_fd, IOCTL_ENGINE_REL, 0);
                if(ret != 0)
                {
                    ALOGE("fatal error! ENGINE_REL err, ret %d", ret);
                }
                goto _err2;
            }
            else
            {
                phy_addr = stUserIommuBuf.iommu_addr;
            }
        }
        else
        {
            ALOGE("fatal error! ENGINE_REQ err, ret %d", ret);
            goto _err2;
        }
    }
    pMemNode->mem_info.mIonHeapType = pAttr->mIonHeapType;
    pMemNode->mem_info.mbSupportCache = pAttr->mbSupportCache;
    pMemNode->mem_info.mAlign = pAttr->mAlign;
    pMemNode->mem_info.vir = (unsigned int)vir_ptr;
    pMemNode->mem_info.phy = phy_addr;
    pMemNode->mem_info.size = pAttr->mLen;
    pMemNode->mem_info.fd_data.fd = map_fd;
    pMemNode->mem_info.fd_data.handle = handle;

    pthread_mutex_lock(&g_mem_manager->mLock);
    list_add_tail(&pMemNode->mList, &g_mem_manager->mMemList);
    pthread_mutex_unlock(&g_mem_manager->mLock);

    return vir_ptr;

_err2:
    /* unmap */
    if (munmap((void *)vir_ptr, pAttr->mLen) < 0)
    {
        ALOGE("munmap 0x%p, size: %d failed", (void*)vir_ptr, pAttr->mLen);
    }
    /*close dma buffer fd*/
    close(map_fd);
    map_fd = -1;
_err1:
{
    struct ion_handle_data handleData = {
        .handle = handle,
    };
    ret = ioctl(g_mem_manager->dev_fd, ION_IOC_FREE, &handleData);
    if(ret != 0)
    {
        ALOGE("(f:%s, l:%d) fatal error! ion ioc free fail[%d]", __FUNCTION__, __LINE__, ret);
    }
}
_err0:
    free(pMemNode);
    return NULL;    
}

int ion_freeMem(void *vir_ptr)
{
    int ret = -1;
    ION_MEM_NODE_S *pEntry;

    ret = searchExistMemList(vir_ptr, &pEntry);
    if (!ret)
    {
        pthread_mutex_lock(&g_mem_manager->mLock);
        list_del(&pEntry->mList);
        pthread_mutex_unlock(&g_mem_manager->mLock);
        
        ret = ion_freeMem_l(pEntry);
        free(pEntry);
    }
    else
    {
        ALOGE("fatal error! vir ptr not find in memlist, free fail!");
    }

    return ret;
}

unsigned int ion_getMemPhyAddr(void *vir_ptr)
{
    int ret = 0;
    ION_MEM_NODE_S *pEntry;

    if (vir_ptr == NULL)
    {
        ALOGE("null ptr!");
        return 0;
    }

    ret = searchExistMemList(vir_ptr, &pEntry);
    if (!ret)
    {
        return pEntry->mem_info.phy;
    }
    else
    {
        ALOGE("vir_ptr not find int list!get phyaddr fail!");
    }

    return ret;
}

int ion_getMemMapFd(void *vir_ptr)
{
    int ret = 0;
    ION_MEM_NODE_S *pEntry;

    if (vir_ptr == NULL)
    {
        ALOGE("null ptr!");
        return 0;
    }

    ret = searchExistMemList(vir_ptr, &pEntry);
    if (!ret)
    {
        return pEntry->mem_info.fd_data.fd;
    }
    else
    {
        ALOGE("vir_ptr not find int list! get mem fd fail!");
    }

    return ret;
}

ion_user_handle_t ion_getMemHandle(void *vir_ptr)
{
    int ret = 0;
    ION_MEM_NODE_S *pEntry;

    if (vir_ptr == NULL)
    {
        ALOGE("null ptr!");
        return 0;
    }

    ret = searchExistMemList(vir_ptr, &pEntry);
    if (!ret)
    {
        return pEntry->mem_info.fd_data.handle;
    }
    else
    {
        ALOGE("vir_ptr not find int list!get mem handle fail!");
        return -1;
    }
}

IonHeapType ion_getMemHeapType(void *vir_ptr)
{
    int ret = 0;
    ION_MEM_NODE_S *pEntry;

    if (vir_ptr == NULL)
    {
        ALOGE("null ptr!");
        return IonHeapType_UNKNOWN;
    }

    ret = searchExistMemList(vir_ptr, &pEntry);
    if (!ret)
    {
        return pEntry->mem_info.mIonHeapType;
    }
    else
    {
        ALOGE("vir_ptr not find int list! get mem heap type fail!");
        return IonHeapType_UNKNOWN;
    }
}

int ion_flushCache(void *vir_ptr, unsigned int size)
{
    int ret = -1;
    ION_MEM_NODE_S *pEntry;

    if (vir_ptr == NULL || !size)
    {
        ALOGE("null ptr or size=0");
        return -1;
    }

    ret = searchExistMemList(vir_ptr, &pEntry);
    if (!ret)
    {
        struct sunxi_cache_range range;
        range.start = (long)pEntry->mem_info.vir;
        range.end = range.start + pEntry->mem_info.size;
        ret = ioctl(g_mem_manager->dev_fd, ION_IOC_SUNXI_FLUSH_RANGE, (void*)&range);
        if (ret != 0)
        {
            ALOGE("flush cache fail");
        }
    }
    else
    {
        ALOGE("vir_ptr not find int list! flush cache fail!");
    }

    return ret;
}

int ion_flushCache_check(void *vir_ptr, unsigned int size, int bCheck)
{
    int ret = -1;
    ION_MEM_NODE_S *pEntry;

    if (vir_ptr == NULL || !size)
    {
        ALOGE("null ptr or size=0");
        return -1;
    }

    if(bCheck)
    {
        ret = searchExistMemList(vir_ptr, &pEntry);
        if(!ret)
        {
            struct sunxi_cache_range range;
            range.start = (long)pEntry->mem_info.vir;
            range.end = range.start + pEntry->mem_info.size;
            ret = ioctl(g_mem_manager->dev_fd, ION_IOC_SUNXI_FLUSH_RANGE, (void*)&range);
            if (ret != 0)
            {
                ALOGE("flush cache fail. virAddr[%p][%d]", vir_ptr, size);
            }
        }
        else
        {
            ALOGE("vir_ptr[%p-%d] not find int list! flush cache fail!", vir_ptr, size);
        }
    }
    else
    {
        struct sunxi_cache_range range;
        range.start = (long)vir_ptr;
        range.end = range.start + size;
        ret = ioctl(g_mem_manager->dev_fd, ION_IOC_SUNXI_FLUSH_RANGE, (void*)&range);
        if (ret != 0)
        {
            ALOGE("flush cache fail. no Check, virAddr[%p-%d]", vir_ptr, size);
        }
    }

    return ret;
}

/*
#define ION_IOC_SUNXI_POOL_INFO        10
struct sunxi_pool_info {
    unsigned int total;     //unit kb
    unsigned int free_kb;  // size kb
    unsigned int free_mb;  // size mb
};
int ion_memGetTotalSize()
{
    int ret = 0;
    struct sunxi_pool_info binfo = {
        .total   = 0,    // mb
        .free_kb  = 0, //the same to free_mb
        .free_mb = 0,
    };

    struct ion_custom_data cdata;
    cdata.cmd = ION_IOC_SUNXI_POOL_INFO;
    cdata.arg = (unsigned long)&binfo;

    ret = ioctl(g_mem_manager->dev_fd, ION_IOC_CUSTOM, &cdata);
    if (ret < 0)
    {
        aloge("Failed to ioctl ion device, errno:%s\n", strerror(errno));
        goto err;
    }

    alogd(" ion dev get free pool [%d MB], [%d KB], total [%d MB]\n", binfo.free_mb, binfo.free_kb, binfo.total / 1024);
    ret = binfo.total;
err:
    return ret;
}
*/
