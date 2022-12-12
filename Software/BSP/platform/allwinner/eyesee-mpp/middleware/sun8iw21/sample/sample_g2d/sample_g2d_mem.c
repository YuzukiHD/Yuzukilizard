//#define LOG_NDEBUG 0
#define LOG_TAG "SampleG2dMem"

#include <memoryAdapter.h>
#include "sc_interface.h"
#include "plat_log.h"
#include <ion_memmanager.h>

int g2d_MemOpen(void)
{
    aloge("zjx_g2d_mem_open");
    return ion_memOpen();
}

int g2d_MemClose(void)
{
    return ion_memClose();
}

unsigned char* g2d_allocMem(unsigned int size)
{
    IonAllocAttr allocAttr;
    memset(&allocAttr, 0, sizeof(IonAllocAttr));
    allocAttr.mLen = size;
    allocAttr.mAlign = 0;
    allocAttr.mIonHeapType = IonHeapType_IOMMU;
    allocAttr.mbSupportCache = 0;
    return ion_allocMem_extend(&allocAttr);
}

int g2d_freeMem(void *vir_ptr)
{
    return ion_freeMem(vir_ptr);
}

unsigned int g2d_getPhyAddrByVirAddr(void *vir_ptr)
{
    return ion_getMemPhyAddr(vir_ptr);
}

int g2d_flushCache(void *vir_ptr, unsigned int size)
{
    return ion_flushCache(vir_ptr, size);
}
