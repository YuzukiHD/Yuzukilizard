/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : ionAlloc.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/


/*
 * ion_alloc.c
 *
 * john.fu@allwinnertech.com
 *
 * ion memory allocate
 *
 */

//#define CONFIG_LOG_LEVEL    OPTION_LOG_LEVEL_DETAIL
#define LOG_TAG "ionAlloc"

#include "cdc_log.h"
#include "CdcUtil.h"
#include "ionAllocList.h"
#include "ionAllocEntry.h"
#include "sc_interface.h"

#include <sys/ioctl.h>
#include <errno.h>
#include "veAdapter.h"
#include "veInterface.h"

#define DEBUG_ION_REF 0   //just for H3 ION memery info debug
#define ION_ALLOC_ALIGN    SZ_4k
#define DEV_NAME                     "/dev/ion"
#define ION_IOC_SUNXI_POOL_INFO        10

#define UNUSA_PARAM(param) (void)param

//----------------------
#if DEBUG_ION_REF==1
    int   cdx_use_mem = 0;
    typedef struct ION_BUF_NODE_TEST
    {
        unsigned long addr;
        int size;
    } ion_buf_node_test;

    #define ION_BUF_LEN  50
    ion_buf_node_test ion_buf_nodes_test[ION_BUF_LEN];
#endif
//----------------------

    struct sunxi_pool_info {
        unsigned int total;     //unit kb
        unsigned int free_kb;  // size kb
        unsigned int free_mb;  // size mb
    };


typedef struct BUFFER_NODE
{
    struct aw_mem_list_head i_list;
    unsigned long phy;        //phisical address
    unsigned long vir;        //virtual address
    unsigned int size;        //buffer size
    unsigned int tee;      //
    unsigned long user_virt;//
    ion_fd_data_t fd_data;
    struct user_iommu_param iommu_buffer;
}buffer_node;

typedef struct ION_ALLOC_CONTEXT
{
    int                    fd;            // driver handle
    struct aw_mem_list_head    list;        // buffer list
    int                    ref_cnt;    // reference count
    unsigned int           phyOffset;
    unsigned int           nTotalSize;
}ion_alloc_context;

static ion_alloc_context    *g_alloc_context = NULL;
static pthread_mutex_t      g_mutex_alloc = PTHREAD_MUTEX_INITIALIZER;



static int getPhyAddr(int nIonFd, uintptr_t handle, void *pIommuBuf,
                                          void *veOps, void *pVeopsSelf, unsigned long *pAddr)
{
    if(CdcIonGetMemType()==MEMORY_IOMMU)
    {
        struct user_iommu_param *pIommuBuffer = (struct user_iommu_param *)pIommuBuf;
        VeOpsS* pVeOpsS = (VeOpsS*)veOps;
        CdcVeGetIommuAddr(pVeOpsS, pVeopsSelf, pIommuBuffer);

        if(pIommuBuffer->iommu_addr & 0xff)
        {
            loge("get iommu addr maybe wrong:%x\n", pIommuBuffer->iommu_addr);
            return -1;
        }
        logv("ion_alloc_palloc: fd:%d, iommu_addr:%x\n", pIommuBuffer->fd,
            pIommuBuffer->iommu_addr);

        *pAddr = (unsigned long)pIommuBuffer->iommu_addr;
    }
    else
    {
        struct aw_ion_custom_info custom_data;
        cdc_sunxi_phys_data   phys_data;
        memset(&phys_data, 0, sizeof(cdc_sunxi_phys_data));
        CEDARC_UNUSE(phys_data.size);
        custom_data.aw_cmd = ION_IOC_SUNXI_PHYS_ADDR;
        phys_data.handle = (aw_ion_user_handle_t)handle;
        custom_data.aw_arg = (unsigned long)&phys_data;
        int ret = ioctl(nIonFd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
        if(ret) {
            loge("ION_IOC_CUSTOM err, ret %d\n", ret);
            return -1;
        }
        logv("CdcIonGetPhyAdrVe:%x, handle:%d\n",phys_data.phys_addr, handle);

        *pAddr = (unsigned long)phys_data.phys_addr;
    }
    return 0;
}

#if DEBUG_ION_REF == 1
static int ion_alloc_get_total_size();
#endif

/*funciton begin*/
int ion_alloc_open()
{
    logd("begin ion_alloc_open \n");

    pthread_mutex_lock(&g_mutex_alloc);
    if (g_alloc_context != NULL)
    {
        logv("ion allocator has already been created \n");
        goto SUCCEED_OUT;
    }

    g_alloc_context = (ion_alloc_context*)malloc(sizeof(ion_alloc_context));
    if (g_alloc_context == NULL)
    {
        loge("create ion allocator failed, out of memory \n");
        goto ERROR_OUT;
    }
    else
    {
        logv("pid: %d, g_alloc_context = %p \n", getpid(), g_alloc_context);
    }

    memset((void*)g_alloc_context, 0, sizeof(ion_alloc_context));

    int type = VE_OPS_TYPE_NORMAL;
    VeOpsS* veOps = GetVeOpsS(type);
    if(veOps == NULL)
    {
        loge("get ve ops failed");
        goto ERROR_OUT;
    }
    VeConfig mVeConfig;
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 1;

    logd("get offset by ve");
    void* pVeopsSelf = CdcVeInit(veOps,&mVeConfig);
    if(pVeopsSelf == NULL)
    {
        loge("init ve ops failed");
        CdcVeRelease(veOps, pVeopsSelf);
        goto ERROR_OUT;
    }
    g_alloc_context->phyOffset = CdcVeGetPhyOffset(veOps, pVeopsSelf);
    logd("** phy offset = %x",g_alloc_context->phyOffset);

    CdcVeRelease(veOps, pVeopsSelf);

/* Readonly should be enough. */
    g_alloc_context->fd = open(DEV_NAME, O_RDONLY, 0);

    if (g_alloc_context->fd <= 0)
    {
        loge("open %s failed \n", DEV_NAME);
        goto ERROR_OUT;
    }

#if DEBUG_ION_REF==1
    cdx_use_mem = 0;
    memset(&ion_buf_nodes_test, 0, sizeof(ion_buf_nodes_test));
    logd("ion_open, cdx_use_mem=[%dByte].", cdx_use_mem);
    ion_alloc_get_total_size();
#endif

    AW_MEM_INIT_LIST_HEAD(&g_alloc_context->list);

SUCCEED_OUT:
    g_alloc_context->ref_cnt++;
    pthread_mutex_unlock(&g_mutex_alloc);
    return 0;

ERROR_OUT:
    if (g_alloc_context != NULL
        && g_alloc_context->fd > 0)
    {
        close(g_alloc_context->fd);
        g_alloc_context->fd = 0;
    }

    if (g_alloc_context != NULL)
    {
        free(g_alloc_context);
        g_alloc_context = NULL;
    }

    pthread_mutex_unlock(&g_mutex_alloc);
    return -1;
}

void ion_alloc_close()
{
    struct aw_mem_list_head * pos, *q;

    logv("ion_alloc_close \n");

    pthread_mutex_lock(&g_mutex_alloc);
    if (--g_alloc_context->ref_cnt <= 0)
    {
        logv("pid: %d, release g_alloc_context = %p \n", getpid(), g_alloc_context);

        aw_mem_list_for_each_safe(pos, q, &g_alloc_context->list)
        {
            buffer_node * tmp;
            tmp = aw_mem_list_entry(pos, buffer_node, i_list);
            logv("ion_alloc_close del item phy= 0x%lx vir= 0x%lx, size= %d \n", \
                tmp->phy, tmp->vir, tmp->size);
            aw_mem_list_del(pos);
            free(tmp);
        }
#if DEBUG_ION_REF==1
        logd("ion_close, cdx_use_mem=[%d MB]", cdx_use_mem/1024/1024);
        ion_alloc_get_total_size();
#endif
        close(g_alloc_context->fd);
        g_alloc_context->fd = 0;

        free(g_alloc_context);
        g_alloc_context = NULL;
    }
    else
    {
        logv("ref cnt: %d > 0, do not free \n", g_alloc_context->ref_cnt);
    }
    pthread_mutex_unlock(&g_mutex_alloc);

    //--------------
#if DEBUG_ION_REF==1
    int i = 0;
    int counter = 0;
    for(i=0; i<ION_BUF_LEN; i++)
    {
        if(ion_buf_nodes_test[i].addr != 0 || ion_buf_nodes_test[i].size != 0){

            loge("ion mem leak????  addr->[0x%lx], leak size->[%dByte]", \
                ion_buf_nodes_test[i].addr, ion_buf_nodes_test[i].size);
            counter ++;
        }
    }

    if(counter != 0)
    {
        loge("my god, have [%d]blocks ion mem leak.!!!!", counter);
    }
    else
    {
        logd("well done, no ion mem leak.");
    }
#endif
    //--------------
    return ;
}

// return virtual address: 0 failed
void* ion_alloc_palloc_base(int size, void *veOps, void *pVeopsSelf, unsigned char bIsCache)
{
    aw_ion_allocation_info_t alloc_data;
    ion_fd_data_t fd_data;
    struct aw_ion_handle_data handle_data;

    int rest_size = 0;
    unsigned long addr_phy = 0;
    unsigned long addr_vir = 0;
    buffer_node * alloc_buffer = NULL;
    int ret = 0;
    memset(&alloc_data, 0, sizeof(aw_ion_allocation_info_t));
    pthread_mutex_lock(&g_mutex_alloc);

    if (g_alloc_context == NULL)
    {
        loge("ion_alloc do not opened, should call ion_alloc_open() \
            before ion_alloc_alloc(size) \n");
        goto ALLOC_OUT;
    }

    if(size <= 0)
    {
        loge("can not alloc size 0 \n");
        goto ALLOC_OUT;
    }

    g_alloc_context->nTotalSize += size;
    logv("size info: curSize = %0.2f MB, totalSize = %0.2f MB", (float)size/1024/1024, (float)g_alloc_context->nTotalSize/1024/1024);

    alloc_data.aw_len = (size_t)size;
    alloc_data.aw_align = ION_ALLOC_ALIGN ;

    if(CdcIonGetMemType() == MEMORY_IOMMU)
    {
        alloc_data.aw_heap_id_mask = AW_ION_SYSTEM_HEAP_MASK | AW_ION_CARVEOUT_HEAP_MASK;
    }
    else
    {
        alloc_data.aw_heap_id_mask = AW_ION_DMA_HEAP_MASK | AW_ION_CARVEOUT_HEAP_MASK;
    }

    if(bIsCache != 0)
    {
        alloc_data.flags = AW_ION_CACHED_FLAG | AW_ION_CACHED_NEEDS_SYNC_FLAG;
    }

    #if 0
    #ifdef CONF_KERNEL_VERSION_4_9
         alloc_data.aw_heap_id_mask = AW_ION_DMA_HEAP_MASK | AW_ION_CARVEOUT_HEAP_MASK;
         alloc_data.flags = AW_ION_CACHED_FLAG | AW_ION_CACHED_NEEDS_SYNC_FLAG;
    #else
         alloc_data.flags = AW_ION_CACHED_FLAG | AW_ION_CACHED_NEEDS_SYNC_FLAG;
    #endif
    #endif
    ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_ALLOC, &alloc_data);
    if (ret)
    {
        loge("ION_IOC_ALLOC error , size = %d\n", size);
        goto ALLOC_OUT;
    }

    /* get dmabuf fd */
    fd_data.handle = alloc_data.handle;
    ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_MAP, &fd_data);
    if(ret)
    {
        loge("ION_IOC_MAP err, ret %d, dmabuf fd 0x%08x\n", ret, (unsigned int)fd_data.aw_fd);
        goto ALLOC_OUT;
    }

    /* mmap to user */
    addr_vir = (unsigned long)mmap(NULL, alloc_data.aw_len, \
        PROT_READ|PROT_WRITE, MAP_SHARED, fd_data.aw_fd, 0);
    if((unsigned long)MAP_FAILED == addr_vir)
    {
        loge("mmap err, ret %lx\n", (unsigned long)addr_vir);
        addr_vir = 0;
        goto ALLOC_OUT;
    }

    alloc_buffer = (buffer_node *)malloc(sizeof(buffer_node));
    if (alloc_buffer == NULL)
    {
        loge("malloc buffer node failed");

        /* unmmap */
        ret = munmap((void*)addr_vir, alloc_data.aw_len);
        if(ret) {
            loge("munmap err, ret %d\n", ret);
        }

        /* close dmabuf fd */
        close(fd_data.aw_fd);

        /* free buffer */
        handle_data.handle = alloc_data.handle;
        ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_FREE, &handle_data);

        if(ret) {
            loge("ION_IOC_FREE err, ret %d\n", ret);
        }

        addr_phy = 0;
        addr_vir = 0;        // value of MAP_FAILED is -1, should return 0

        goto ALLOC_OUT;
    }

    struct user_iommu_param iommu_buffer;
    memset(&iommu_buffer, 0, sizeof(struct user_iommu_param));
    iommu_buffer.fd = fd_data.aw_fd;

    ret = getPhyAddr(g_alloc_context->fd, (uintptr_t)alloc_data.handle,
                                 (void *)&iommu_buffer, veOps, pVeopsSelf, &addr_phy);
    if(ret < 0)
    {
        loge("get phy addr error\n");
        goto ALLOC_OUT;
    }
    memcpy(&alloc_buffer->iommu_buffer, &iommu_buffer, sizeof(struct user_iommu_param));
    alloc_buffer->phy     = addr_phy;
    alloc_buffer->vir     = addr_vir;
    alloc_buffer->user_virt = addr_vir;
    alloc_buffer->size    = size;
    alloc_buffer->fd_data.handle = fd_data.handle;
    alloc_buffer->fd_data.aw_fd = fd_data.aw_fd;

    logv("alloc succeed, addr_phy: 0x%lx, end_phy: 0x%lx,  addr_vir: 0x%lx, size: %d",
         addr_phy, addr_phy + size, addr_vir, size);

    aw_mem_list_add_tail(&alloc_buffer->i_list, &g_alloc_context->list);

    //------start-----------------
#if DEBUG_ION_REF==1
    cdx_use_mem += size;
    logd("++++++cdx_use_mem = [%d MB], increase size->[%d B], addr_vir=[0x%lx], addr_phy=[0x%lx]", \
        cdx_use_mem/1024/1024, size, addr_vir, addr_phy);
    int i = 0;
    for(i=0; i<ION_BUF_LEN; i++)
    {
        if(ion_buf_nodes_test[i].addr == 0 && ion_buf_nodes_test[i].size == 0){
            ion_buf_nodes_test[i].addr = addr_vir;
            ion_buf_nodes_test[i].size = size;
            break;
        }
    }

    if(i>= ION_BUF_LEN){
        loge("error, ion buf len is large than [%d]", ION_BUF_LEN);
    }
#endif
//--------------------------------

ALLOC_OUT:
    pthread_mutex_unlock(&g_mutex_alloc);
    return (void*)addr_vir;
}

void* ion_alloc_palloc(int size, void *veOps, void *pVeopsSelf)
{
    unsigned char bIsCache = 1;
    return ion_alloc_palloc_base(size, veOps, pVeopsSelf, bIsCache);
}

void* ion_alloc_no_cache_palloc(int size, void *veOps, void *pVeopsSelf)
{
    unsigned char bIsCache = 0;
    return ion_alloc_palloc_base(size, veOps, pVeopsSelf, bIsCache);
}

void ion_alloc_pfree(void * pbuf, void *veOps, void *pVeopsSelf)
{
    int flag = 0;
    unsigned long addr_vir = (unsigned long)pbuf;
    buffer_node * tmp;
    int ret;
    struct aw_ion_handle_data handle_data;

    if (0 == pbuf)
    {
        loge("can not free NULL buffer \n");
        return ;
    }

    pthread_mutex_lock(&g_mutex_alloc);

    if (g_alloc_context == NULL)
    {
        loge("ion_alloc do not opened, should call ion_alloc_open() \
            before ion_alloc_alloc(size) \n");
        pthread_mutex_unlock(&g_mutex_alloc);
        return ;
    }

    aw_mem_list_for_each_entry(tmp, &g_alloc_context->list, i_list)
    {
        if (tmp->vir == addr_vir)
        {
            logv("ion_alloc_free item phy= 0x%lx vir= 0x%lx, size= %d \n", \
                tmp->phy, tmp->vir, tmp->size);
            /*unmap user space*/
            if(CdcIonGetMemType() == MEMORY_IOMMU &&
                veOps != NULL && pVeopsSelf != NULL)
            {
                logv("ion_alloc_pfree: fd:%d, iommu_addr:%x\n",
                                        tmp->iommu_buffer.fd,
                                        tmp->iommu_buffer.iommu_addr);

                if(CdcVeFreeIommuAddr((VeOpsS*)veOps, pVeopsSelf, &tmp->iommu_buffer) < 0)
                    loge("VeFreeIommuAddr error\n");
            }
            g_alloc_context->nTotalSize -= tmp->size;

            if (munmap((void *)(tmp->user_virt), tmp->size) < 0)
            {
                loge("munmap 0x%p, size: %d failed \n", (void*)addr_vir, tmp->size);
            }

            /*close dma buffer fd*/
            close(tmp->fd_data.aw_fd);
            /* free buffer */
            handle_data.handle = tmp->fd_data.handle;

            ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_FREE, &handle_data);
            if (ret)
            {
                logv("TON_IOC_FREE failed \n");
            }

            aw_mem_list_del(&tmp->i_list);
            free(tmp);

            flag = 1;

            //------start-----------------
#if DEBUG_ION_REF==1
            int i = 0;
            for(i=0; i<ION_BUF_LEN; i++)
            {
                if(ion_buf_nodes_test[i].addr == addr_vir && ion_buf_nodes_test[i].size > 0){

                    cdx_use_mem -= ion_buf_nodes_test[i].size;
                    logv("--------cdx_use_mem = [%d MB], reduce size->[%d B]",\
                        cdx_use_mem/1024/1024, ion_buf_nodes_test[i].size);
                    ion_buf_nodes_test[i].addr = 0;
                    ion_buf_nodes_test[i].size = 0;

                    break;
                }
            }

            if(i>= ION_BUF_LEN){
                loge("error, ion buf len is large than [%d]", ION_BUF_LEN);
            }
#endif
            //--------------------------------

            break;
        }
    }

    if (0 == flag)
    {
        loge("ion_alloc_free failed, do not find virtual address: 0x%lx \n", addr_vir);
    }

    pthread_mutex_unlock(&g_mutex_alloc);
    return ;
}

void* ion_alloc_vir2phy_cpu(void * pbuf)
{
    int flag = 0;
    unsigned long addr_vir = (unsigned long)pbuf;
    unsigned long addr_phy = 0;
    buffer_node * tmp;

    if (0 == pbuf)
    {
        // logv("can not vir2phy NULL buffer \n");
        return 0;
    }

    pthread_mutex_lock(&g_mutex_alloc);

    aw_mem_list_for_each_entry(tmp, &g_alloc_context->list, i_list)
    {
        if (addr_vir >= tmp->vir
            && addr_vir < tmp->vir + tmp->size)
        {
            addr_phy = tmp->phy + addr_vir - tmp->vir;
            // logv("ion_alloc_vir2phy phy= 0x%08x vir= 0x%08x \n", addr_phy, addr_vir);
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("ion_alloc_vir2phy failed, do not find virtual address: 0x%lx \n", addr_vir);
    }

    pthread_mutex_unlock(&g_mutex_alloc);

    return (void*)addr_phy;
}

void* ion_alloc_phy2vir_cpu(void * pbuf)
{
    int flag = 0;
    unsigned long addr_vir = 0;
    unsigned long addr_phy = (unsigned long)pbuf;
    buffer_node * tmp;

    if (0 == pbuf)
    {
        loge("can not phy2vir NULL buffer \n");
        return 0;
    }

    pthread_mutex_lock(&g_mutex_alloc);

    aw_mem_list_for_each_entry(tmp, &g_alloc_context->list, i_list)
    {
        if (addr_phy >= tmp->phy
            && addr_phy < tmp->phy + tmp->size)
        {
            addr_vir = tmp->vir + addr_phy - tmp->phy;
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("ion_alloc_phy2vir failed, do not find physical address: 0x%lx \n", addr_phy);
    }

    pthread_mutex_unlock(&g_mutex_alloc);

    return (void*)addr_vir;
}

void* ion_alloc_vir2phy_ve(void * pbuf)
{
    logv("**11 phy offset = %x",g_alloc_context->phyOffset);

    return (void*)((unsigned long)ion_alloc_vir2phy_cpu(pbuf) - g_alloc_context->phyOffset);
}

void* ion_alloc_phy2vir_ve(void * pbuf)
{
    logv("**22 phy offset = %x",g_alloc_context->phyOffset);

    return (void*)((unsigned long)ion_alloc_phy2vir_cpu(pbuf) - g_alloc_context->phyOffset);
}

#ifdef CONF_KERNEL_VERSION_3_4
void ion_alloc_flush_cache(void* startAddr, int size)
{
    sunxi_cache_range range;
    struct aw_ion_custom_info custom_data;
    int ret;

    /* clean and invalid user cache */
    range.start = (unsigned long)startAddr;
    range.end = (unsigned long)startAddr + size;

    custom_data.aw_cmd = ION_IOC_SUNXI_FLUSH_RANGE;
    custom_data.aw_arg = (unsigned long)&range;

    ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
    if (ret)
    {
        loge("ION_IOC_CUSTOM failed \n");
    }

    return;
}
#else
void ion_alloc_flush_cache(void* startAddr, int size)
{
    int ret;
    sunxi_cache_range range;

     /* clean and invalid user cache */
    range.start = (unsigned long)startAddr;
    range.end = (unsigned long)startAddr + size;
    logv("start:%p, end:%lx, size:%lx(%ld)\n", startAddr, range.end, (long)size, (long)size);
    ret = ioctl(g_alloc_context->fd, ION_IOC_SUNXI_FLUSH_RANGE, &range);
    if (ret)
    {
        loge("ION_IOC_SUNXI_FLUSH_RANGE failed errno: %d, ret: %d", errno, ret);
    }

    return;
}
#endif

void ion_flush_cache_all()
{
    CEDARC_UNUSE(ion_flush_cache_all);

    ioctl(g_alloc_context->fd, ION_IOC_SUNXI_FLUSH_ALL, 0);
}

void* ion_alloc_alloc_drm(int size, void*veOps, void* pVeopsSelf)
{
    aw_ion_allocation_info_t alloc_data;
    ion_fd_data_t fd_data;
    struct aw_ion_handle_data handle_data;
    struct aw_ion_custom_info custom_data;
    cdc_sunxi_phys_data   phys_data, tee_data;

    int rest_size = 0;
    unsigned long addr_phy = 0;
    unsigned long addr_vir = 0;
    unsigned long addr_tee = 0;
    buffer_node * alloc_buffer = NULL;
    int ret = 0;

    pthread_mutex_lock(&g_mutex_alloc);

    if (g_alloc_context == NULL)
    {
        loge("ion_alloc do not opened, should call ion_alloc_open() \
            before ion_alloc_alloc(size) \n");
        goto ALLOC_OUT;
    }

    if(size <= 0)
    {
        logv("can not alloc size 0 \n");
        goto ALLOC_OUT;
    }

    /*alloc buffer*/
    alloc_data.aw_len = size;
    alloc_data.aw_align = ION_ALLOC_ALIGN ;
    alloc_data.aw_heap_id_mask = AW_ION_SECURE_HEAP_MASK;
    alloc_data.flags = AW_ION_CACHED_FLAG | AW_ION_CACHED_NEEDS_SYNC_FLAG;
    ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_ALLOC, &alloc_data);
    if (ret)
    {
        loge("ION_IOC_ALLOC error %s \n", strerror(errno));
        goto ALLOC_OUT;
    }

    /* get dmabuf fd */
    fd_data.handle = alloc_data.handle;
    ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_MAP, &fd_data);
    if(ret)
    {
        loge("ION_IOC_MAP err, ret %d, dmabuf fd 0x%08x\n", ret, (unsigned int)fd_data.aw_fd);
        goto ALLOC_OUT;
    }


    /* mmap to user */
    addr_vir = (unsigned long)mmap(NULL, alloc_data.aw_len, \
        PROT_READ|PROT_WRITE, MAP_SHARED, fd_data.aw_fd, 0);
    if((unsigned long)MAP_FAILED == addr_vir)
    {
        //loge("mmap err, ret %d\n", (unsigned int)addr_vir);
        addr_vir = 0;
        goto ALLOC_OUT;
    }

    /* get phy address */
    memset(&phys_data, 0, sizeof(phys_data));
    phys_data.handle = alloc_data.handle;
    phys_data.size = size;
    custom_data.aw_cmd = ION_IOC_SUNXI_PHYS_ADDR;
    custom_data.aw_arg = (unsigned long)&phys_data;

    ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
    if(ret) {
        loge("ION_IOC_CUSTOM err, ret %d\n", ret);
        addr_phy = 0;
        addr_vir = 0;
        goto ALLOC_OUT;
    }

    addr_phy = phys_data.phys_addr;
#if(ADJUST_ADDRESS_FOR_SECURE_OS_OPTEE)
    memset(&tee_data, 0, sizeof(tee_data));
    tee_data.handle = alloc_data.handle;
    tee_data.size = size;
    custom_data.aw_cmd = ION_IOC_SUNXI_TEE_ADDR;
    custom_data.aw_arg = (unsigned long)&tee_data;
    ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
    if(ret) {
        loge("ION_IOC_CUSTOM err, ret %d\n", ret);
        addr_phy = 0;
        addr_vir = 0;
        goto ALLOC_OUT;
    }
    addr_tee = tee_data.phys_addr;
#else
    addr_tee = addr_vir;
#endif
    alloc_buffer = (buffer_node *)malloc(sizeof(buffer_node));
    if (alloc_buffer == NULL)
    {
        loge("malloc buffer node failed");

        /* unmmap */
        ret = munmap((void*)addr_vir, alloc_data.aw_len);
        if(ret) {
            loge("munmap err, ret %d\n", ret);
        }

        /* close dmabuf fd */
        close(fd_data.aw_fd);

        /* free buffer */
        handle_data.handle = alloc_data.handle;
        ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_FREE, &handle_data);

        if(ret) {
            loge("ION_IOC_FREE err, ret %d\n", ret);
        }

        addr_phy = 0;
        addr_vir = 0;        // value of MAP_FAILED is -1, should return 0

        goto ALLOC_OUT;
    }

    if(CdcIonGetMemType() == MEMORY_IOMMU &&
        veOps != NULL && pVeopsSelf != NULL)
    {
        struct user_iommu_param iommu_buffer;
        memset(&iommu_buffer, 0, sizeof(struct user_iommu_param));
        iommu_buffer.fd = fd_data.aw_fd;

        ret = getPhyAddr(g_alloc_context->fd, (uintptr_t)alloc_data.handle,
                                 (void *)&iommu_buffer, veOps, pVeopsSelf, &addr_phy);
        if(ret < 0)
        {
            loge("get phy addr error\n");
            goto ALLOC_OUT;
        }
        logd("iommu_buffer.fd: %d, iommu_buffer.iommu_addr: %x",
            iommu_buffer.fd, iommu_buffer.iommu_addr);
        memcpy(&alloc_buffer->iommu_buffer, &iommu_buffer, sizeof(struct user_iommu_param));
    }

    alloc_buffer->size        = size;
    alloc_buffer->phy         = addr_phy;
    alloc_buffer->user_virt = addr_vir;
    alloc_buffer->vir         = addr_tee;
    alloc_buffer->tee         = addr_tee;
    alloc_buffer->fd_data.handle = fd_data.handle;
    alloc_buffer->fd_data.aw_fd = fd_data.aw_fd;

    aw_mem_list_add_tail(&alloc_buffer->i_list, &g_alloc_context->list);

    ALLOC_OUT:

    pthread_mutex_unlock(&g_mutex_alloc);

    return (void*)addr_tee;
}

//return total meminfo with MB
int ion_alloc_get_total_size()
{
    int ret = 0;

    int ion_fd = open(DEV_NAME, O_WRONLY);

    if (ion_fd < 0) {
        loge("open ion dev failed, cannot get ion mem.");
        goto err;
    }

    struct sunxi_pool_info binfo = {
        .total   = 0,    // mb
        .free_kb  = 0, //the same to free_mb
        .free_mb = 0,
    };

    struct aw_ion_custom_info cdata;

    cdata.aw_cmd = ION_IOC_SUNXI_POOL_INFO;
    cdata.aw_arg = (unsigned long)&binfo;
    ret = ioctl(ion_fd,AW_MEM_ION_IOC_CUSTOM, &cdata);
    if (ret < 0){
        logw("Failed to ioctl ion device, errno:%s\n", strerror(errno));
        goto err;
    }

    logd(" ion dev get free pool [%d MB], total [%d MB]\n", binfo.free_mb, binfo.total / 1024);
    ret = binfo.total;
err:
    if(ion_fd > 0){
        close(ion_fd);
    }
    return ret;
}

int ion_alloc_memset(void* buf, int value, size_t n)
{
    memset(buf, value, n);
    return -1;
}

int ion_alloc_copy(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

int ion_alloc_read(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

int ion_alloc_write(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

int ion_alloc_setup()
{
    return -1;
}

int ion_alloc_shutdown()
{
    return -1;
}

unsigned int ion_alloc_get_ve_addr_offset()
{
    if(g_alloc_context != NULL)
        return g_alloc_context->phyOffset;
    else
    {
        loge("g_alloc_context is NULL, please call ion_alloc_open\n");
        return 0;
    }
}


struct ScMemOpsS _ionMemOpsS =
{
    .open  =              ion_alloc_open,
    .close =              ion_alloc_close,
    .total_size=          ion_alloc_get_total_size,
    .palloc =             ion_alloc_palloc,
    .palloc_no_cache = ion_alloc_no_cache_palloc,
    .pfree =              ion_alloc_pfree,
    .flush_cache =        ion_alloc_flush_cache,
    .ve_get_phyaddr=      ion_alloc_vir2phy_ve,
    .ve_get_viraddr =     ion_alloc_phy2vir_ve,
    .cpu_get_phyaddr =    ion_alloc_vir2phy_cpu,
    .cpu_get_viraddr =    ion_alloc_phy2vir_cpu,
    .mem_set =            ion_alloc_memset,
    .mem_cpy =            ion_alloc_copy,
    .mem_read =           ion_alloc_read,
    .mem_write =          ion_alloc_write,
    .setup =              ion_alloc_setup,
    .shutdown =           ion_alloc_shutdown,
    .palloc_secure =      ion_alloc_alloc_drm,
    .get_ve_addr_offset = ion_alloc_get_ve_addr_offset
};

struct ScMemOpsS* __GetIonMemOpsS()
{
    logd("*** get __GetIonMemOpsS ***");
    return &_ionMemOpsS;
}
