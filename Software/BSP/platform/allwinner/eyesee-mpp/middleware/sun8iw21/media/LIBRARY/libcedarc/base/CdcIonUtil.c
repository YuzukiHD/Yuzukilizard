/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : cdcionUtil.c
* Description : get phy addr for android
*              (it is only work in android, do not compile in LINUX)
* History :
*   Comment :
*
*
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#ifdef __ANDROID__
#include <cutils/properties.h>
#endif


#include "cdc_log.h"
#include "CdcUtil.h"

int CdcIonGetMemType()
{
    MEMORY_TYPE eMemoryType = MEMORY_NORMAL;

#if CONF_USE_IOMMU
    eMemoryType = MEMORY_IOMMU;
#else

    #ifdef __ANDROID__
    char prop_value[512];
    property_get("ro.kernel.iomem.type", prop_value, "0xaf01");
    logv("++++ prop_value: %s", prop_value);
    if(strcmp(prop_value, "0xaf10")==0)
    {
        eMemoryType = MEMORY_IOMMU;
    }
    #endif
#endif

    logv("eMemoryType = %d", eMemoryType);
    return eMemoryType;
}
unsigned long CdcIonGetPhyAdr(int fd, uintptr_t handle)
{
    if(CdcIonGetMemType() == MEMORY_IOMMU)
    {
        return 1;
    }
    else
    {
        int ret = 0;
        ion_custom_data_t custom_data;
        cdc_sunxi_phys_data phys_data;
        memset(&phys_data, 0, sizeof(cdc_sunxi_phys_data));
        CEDARC_UNUSE(phys_data.size);
        custom_data.aw_cmd = ION_IOC_SUNXI_PHYS_ADDR;
        phys_data.handle = (aw_ion_user_handle_t)handle;
        custom_data.aw_arg = (unsigned long)&phys_data;
        ret = ioctl(fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
        if(ret < 0)
        {
            loge("CdcIonGetPhyAdr AW_MEM_ION_IOC_CUSTOM error\n");
            return 0;
        }
        return phys_data.phys_addr;
    }
}

int CdcIonGetFd(int fd, uintptr_t handle)
{
    int ret;
    ion_fd_data_t fd_data;
    fd_data.handle = (aw_ion_user_handle_t)handle;
    ret = ioctl(fd, AW_MEM_ION_IOC_MAP, &fd_data);
    if(ret)
    {
        loge("CdcIonGetFd map other dev's handle err, ret %d, dmabuf fd 0x%08x\n",
                                                                ret, fd_data.aw_fd);
        return -1;
    }
    return fd_data.aw_fd;
}

int CdcIonImport(int fd, int share_fd, aw_ion_user_handle_t *ion_handle)
{
    int ret;
    ion_fd_data_t fd_data;

    memset(&fd_data, 0, sizeof(ion_fd_data_t));
    fd_data.aw_fd = share_fd;
    ret = ioctl(fd, AW_MEM_ION_IOC_IMPORT, &fd_data);
    if(ret)
    {
        loge("CdcIonImport get ion_handle err, ret %d\n",ret);
        return -1;
    }

    *ion_handle = fd_data.handle;
    return 0;
}

int CdcIonFree(int fd, aw_ion_user_handle_t ion_handle)
{
    int ret;
    ion_handle_data_t sIonHandleData;

    memset(&sIonHandleData, 0, sizeof(ion_handle_data_t));
    sIonHandleData.handle = ion_handle;
    ret = ioctl(fd, AW_MEM_ION_IOC_FREE, &sIonHandleData);
    if(ret)
    {
        loge("CdcIonFree free ion_handle err, ret %d\n",ret);
        return -1;
    }
    return 0;
}

int CdcIonOpen(void)
{
    int fd = open("/dev/ion", O_RDWR);
    if (fd < 0)
        loge("open /dev/ion failed!\n");
    return fd;
}

int CdcIonClose(int fd)
{
    int ret = close(fd);
    if (ret < 0)
    {
        loge("CdcIonClose failed with code %d: %s\n", ret, strerror(errno));
        return -errno;
    }
    return ret;
}

int CdcIonMmap(int buf_fd, size_t len, unsigned char **pVirAddr)
{
    CEDARC_UNUSE(CdcIonMmap);
    /* mmap to user */
    *pVirAddr = mmap(NULL, len, \
        PROT_READ|PROT_WRITE, MAP_SHARED, buf_fd, 0);
    if(MAP_FAILED == pVirAddr)
    {
        loge("CdcIonMmap failed: %s\n", strerror(errno));
        return -errno;
    }

    return 0;
}

int CdcIonUnmap(size_t len, unsigned char *pVirAddr)
{
    CEDARC_UNUSE(CdcIonUnmap);
    int ret;
    ret = munmap((void*)pVirAddr, len);
    if(ret)
    {
        loge("CdcIonUnmap failed: %s\n", strerror(errno));
        return -errno;
    }

    return 0;
}

unsigned long CdcIonGetTEEAdr(int fd, uintptr_t handle)
{
    int ret = 0;
    cdc_sunxi_phys_data tee_data;
    ion_custom_data_t custom_data;

    memset(&tee_data, 0, sizeof(cdc_sunxi_phys_data));
    tee_data.handle = (aw_ion_user_handle_t)handle;
    CEDARC_UNUSE(tee_data.size);
    custom_data.aw_cmd = ION_IOC_SUNXI_TEE_ADDR;
    custom_data.aw_arg = (unsigned long)&tee_data;
    ret = ioctl(fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
    if(ret)
    {
        loge("ION_IOC_CUSTOM get TEE addr err, ret %d\n", ret);
        return 0;
    }
    return tee_data.phys_addr;
}
