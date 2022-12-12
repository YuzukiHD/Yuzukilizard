/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : secureAlloc.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

/*
 * secureos_adapter.c
 *
 *  Created on: 2014-8-18
 *      Author: wei
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "secureAlloc"
#include "cdc_log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "sc_interface.h"
#include "secureAllocEntry.h"
#include "../ionMemory/ionAllocEntry.h"

#if(SECURE_OS_OPTEE == 1)
    #include <tee_client_api.h> //* for optee_secure_os
#else
    #include <sunxi_tee_api.h>  //* for semelis_secure_os
#endif

#define SE_ASSERT(expr) \
    do {\
        if(!(expr)) {\
            loge("%s:%d, assert \"%s\" failed", __FILE__, __LINE__, #expr); \
            abort();\
        }\
    } while(0)

#ifdef ATTRIBUTE_UNUSED
#undef ATTRIBUTE_UNUSED
#endif
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))

enum {
    /* keep these values (started with MSG_ADAPTER_) in sync
     * with definitions in vdecoder in secureos*/
    MSG_ADPATER_MIN = 0,
    MSG_ADPATER_INIT,
    MSG_ADPATER_MEM_ALLOC,
    MSG_ADPATER_MEM_FREE,
    MSG_ADPATER_MEM_COPY,
    MSG_ADPATER_MEM_SET,
    MSG_ADPATER_MEM_FLUSH_CACHE,
    MSG_ADPATER_MEM_GET_PHYSICAL_ADDRESS,
    MSG_ADPATER_MEM_GET_VIRTUAL_ADDRESS,
    MSG_ADPATER_MEM_READ,
    MSG_ADPATER_MEM_WRITE,
    MSG_ADPATER_MEM_READ_INT,
    MSG_ADPATER_MEM_WRITE_INT,
    MSG_ADPATER_MEM_DUMP,
    MSG_ADPATER_MEM_DEBUG,
    MSG_ADPATER_EXIT,
    MSG_ADPATER_SETUP_VE,
    MSG_ADPATER_SHUTDOWN_VE,
    MSG_ADPATER_MAX,
};

//decoder singleton to ensure there is only one decoder created.

typedef struct {
    TEEC_Context tee_context;
    TEEC_Session tee_session;
    int ref_count;
    struct ScMemOpsS* ionMemOps;
}AdapterContext;

static AdapterContext *gAdapterContext = NULL;
static pthread_mutex_t gAdapterMutex = PTHREAD_MUTEX_INITIALIZER;

static const uint8_t adapter_UUID[16] = {
    0xEA, 0xD2, 0x78, 0x4D, 0x31, 0xA6,    0xFB, 0x70,
    0xAA, 0xA7,    0x87, 0xC2, 0xB5, 0x77, 0x30, 0x52
};

int HwSecureAllocOpen()
{
    logd("HwSecureAllocOpen, pid %d", getpid());
    pthread_mutex_lock(&gAdapterMutex);
    TEEC_Result err = TEEC_SUCCESS;
    if(gAdapterContext == NULL) {
        gAdapterContext = (AdapterContext *)malloc(sizeof(AdapterContext));
        SE_ASSERT(gAdapterContext != NULL);
        memset(gAdapterContext, 0, sizeof(AdapterContext));

        gAdapterContext->ionMemOps = __GetIonMemOpsS();
        CdcMemOpen(gAdapterContext->ionMemOps);

        err = TEEC_InitializeContext(NULL, &gAdapterContext->tee_context);
        if(err == TEEC_SUCCESS) {
            err = TEEC_OpenSession(&gAdapterContext->tee_context, &gAdapterContext->tee_session,
                    (const TEEC_UUID *)&adapter_UUID[0], TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);

            if(err == TEEC_SUCCESS) {
                //session opened successfully, init adapter.
                TEEC_Operation operation;
                memset(&operation, 0, sizeof(TEEC_Operation));

                operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
                        TEEC_NONE, TEEC_NONE);

                operation.started = 1;
                err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                        MSG_ADPATER_INIT, &operation, NULL);
                logw("secure memory adapter has been created.");
            }
        }
    }
    if(err == 0)
        gAdapterContext->ref_count ++;
    else {
        loge("initialize context failed, %d", err);
        free(gAdapterContext);
        gAdapterContext = NULL;
    }
    pthread_mutex_unlock(&gAdapterMutex);
    return err;
}

void HwSecureAllocClose()
{
    logd("HwSecureAllocClose, pid %d", getpid());
    pthread_mutex_lock(&gAdapterMutex);
    if(!gAdapterContext) {
        loge("secure adapter has not been initialized");
        pthread_mutex_unlock(&gAdapterMutex);
        return ;
    }
    TEEC_Result err = 0;
    if(--gAdapterContext->ref_count == 0) {
        TEEC_Operation operation;
        memset(&operation, 0, sizeof(TEEC_Operation));

        operation.started = 1;
        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
        err  = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_EXIT, &operation, NULL);
        if (err != TEEC_SUCCESS) {
            loge("call invoke command error");
        }
        TEEC_CloseSession(&gAdapterContext->tee_session);
        TEEC_FinalizeContext(&gAdapterContext->tee_context);

        CdcMemClose(gAdapterContext->ionMemOps);
        free(gAdapterContext);
        gAdapterContext = NULL;
        logw("secure memory adapter has been destroyed.");
    }
    pthread_mutex_unlock(&gAdapterMutex);
    return ;
}

int HwSecureAllocGetTotalSize()
{
    return CdcMemTotalSize(gAdapterContext->ionMemOps);
}

void *HwSecureAllocPalloc(int size, void *veOps, void *pVeopsSelf)
{
    logv("SecureMemAdapterAlloc, size %d", size);
    if(size <= 0) {
        return NULL;
    }
    return CdcMemPallocSecure(gAdapterContext->ionMemOps, size, veOps, pVeopsSelf);
}

void HwSecureAllocPfree(void *ptr, void *veOps, void *pVeopsSelf)
{
    logv("SecureMemAdapterFree, ptr %p", ptr);
    if(ptr == NULL) {
        return ;
    }

    CdcMemPfree(gAdapterContext->ionMemOps, ptr, veOps, pVeopsSelf);
}

void* HwSecureAllocGetPhysicAddress(void *virt)
{
    logv("SecureMemAdapterGetPhysicAddress");
    return CdcMemGetPhysicAddress(gAdapterContext->ionMemOps, virt);
}

void* HwSecureAllocGetVirtualAddress(void *phy)
{
    logv("SecureMemAdapterGetVirtualAddress");
    return CdcMemGetVirtualAddress(gAdapterContext->ionMemOps, phy);
}

void* HwSecureAllocGetPhysicAddressCpu(void *virt)
{
    logv("SecureMemAdapterGetPhysicAddress");
    return CdcMemGetPhysicAddressCpu(gAdapterContext->ionMemOps, virt);
}

void* HwSecureAllocGetVirtualAddressCpu(void *phy)
{
    logv("SecureMemAdapterGetVirtualAddress");
    return CdcMemGetVirtualAddressCpu(gAdapterContext->ionMemOps, phy);
}

void HwSecureAllocFlushCache(void *ptr, int size)
{
    logv("SecureMemAdapterFlushCache, %p ", ptr);
    if(ptr == NULL || size <= 0) {
        return ;
    }
    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT, TEEC_NONE, TEEC_NONE);
    operation.started = 1;
    #if(ADJUST_ADDRESS_FOR_SECURE_OS_OPTEE)
    operation.params[0].value.a = (uintptr_t)(ptr);
    #else
    operation.params[0].value.a = (uintptr_t)HwSecureAllocGetPhysicAddressCpu(ptr);
    #endif
    operation.params[1].value.a = (uint32_t)size;
    if(operation.params[0].value.a == 0) {
        loge("flush with invalid address");
        return ;
    }
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_MEM_FLUSH_CACHE, &operation, NULL);
    logv("TEEC_InvokeCommand, err = %d",err);
    return ;
}

int HwSecureAllocSet(void *s, int c, size_t n)
{
    logv("SecureMemAdapterSet, s %p", s);
    if(s == NULL || n == 0) {
        return -1;
    }
    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);
    operation.started = 1;
    #if(ADJUST_ADDRESS_FOR_SECURE_OS_OPTEE)
    operation.params[0].value.a = (uintptr_t)(s);
    #else
    operation.params[0].value.a = (uintptr_t)HwSecureAllocGetPhysicAddressCpu(s);
    #endif
    operation.params[1].value.a = (uint32_t)c;
    operation.params[2].value.a = (uint32_t)n;
    if(operation.params[0].value.a == 0) {
        loge("set with invalid address");
        return -1;
    }
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_MEM_SET, &operation, NULL);
    return err;
}

int HwSecureAllocCopy(void *dest, void *src, size_t n)
{
    logv("SecureMemAdapterCopy, dest %p, src %p", dest, src);

    if(dest == NULL || src == NULL) {
        return -1;
    }

    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);
    operation.started = 1;
    #if(ADJUST_ADDRESS_FOR_SECURE_OS_OPTEE)
    operation.params[0].value.a = (uintptr_t)dest;
    operation.params[1].value.a = (uintptr_t)src;
    #else
    operation.params[0].value.a = (uintptr_t)HwSecureAllocGetPhysicAddressCpu(dest);
    operation.params[1].value.a = (uintptr_t)(HwSecureAllocGetPhysicAddressCpu)(src);
    #endif
    operation.params[2].value.a = (uint32_t)n;
    if((operation.params[0].value.a == 0) || (operation.params[1].value.a == 0)) {
        loge("copy with invalid address, 0x%x vs 0x%x", operation.params[0].value.a,
                operation.params[1].value.a);
        return -1;
    }
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_MEM_COPY, &operation, NULL);
    return err;
}

int HwSecureAllocRead(void *dest, void *src, size_t n)
{
    logv("SecureMemAdapterRead");
    if(src == NULL || dest == 0 || n == 0) {
        return 0;
    }
    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;

    memset(&operation, 0, sizeof(TEEC_Operation));
    operation.started = 1;
    #if(ADJUST_ADDRESS_FOR_SECURE_OS_OPTEE)
    operation.params[0].value.a = (uintptr_t)src;
    #else
    operation.params[0].value.a = (uintptr_t)HwSecureAllocGetPhysicAddressCpu(src);
    #endif

    operation.params[2].value.a = (uint32_t)n;
    if(operation.params[0].value.a == 0) {
        loge("read with invalid address");
        return -1;
    }
    TEEC_Result err;
    if (n > 4) {
        TEEC_SharedMemory share_mem;
        share_mem.size = n;
        share_mem.flags = TEEC_MEM_OUTPUT ;
        if(TEEC_AllocateSharedMemory(&gAdapterContext->tee_context, &share_mem) != TEEC_SUCCESS) {
            loge("allocate share memory fail");
            return 0;
        }
        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
                TEEC_MEMREF_WHOLE, TEEC_VALUE_INPUT, TEEC_NONE);

        operation.params[1].memref.parent = &share_mem;
        operation.params[1].memref.offset = 0;
        operation.params[1].memref.size   = 0;

        err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_MEM_READ, &operation, NULL);

        memcpy(dest, share_mem.buffer, n);
        TEEC_ReleaseSharedMemory(&share_mem);
    } else {
        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
                TEEC_VALUE_OUTPUT, TEEC_VALUE_INPUT, TEEC_NONE);
        err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_MEM_READ_INT, &operation, NULL);

        memcpy(dest, (unsigned char*)(&operation.params[1].value.a), n);
    }

    return err;
}

int HwSecureAllocWrite(void *dest, void *src, size_t n)
{
    logv("SecureMemAdapterWrite");
    if(src == NULL || dest == 0 || n == 0) {
        return 0;
    }
    SE_ASSERT(gAdapterContext != NULL);

    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    #if(ADJUST_ADDRESS_FOR_SECURE_OS_OPTEE)
    operation.params[1].value.a = (uintptr_t)dest;
    #else
    operation.params[1].value.a = (uintptr_t)HwSecureAllocGetPhysicAddressCpu(dest);
    #endif
    operation.params[2].value.a = (uint32_t)n;

    if(operation.params[1].value.a == 0) {
        loge("write with invalid address");
        return -1;
    }
    TEEC_Result err;
    if (n > 4) {
        TEEC_SharedMemory share_mem;
        share_mem.size = n;
        share_mem.flags = TEEC_MEM_INPUT;
        if(TEEC_AllocateSharedMemory(&gAdapterContext->tee_context, &share_mem) != TEEC_SUCCESS) {
            loge("allocate share memory fail");
            return 0;
        }
        memcpy(share_mem.buffer, src, n);

        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
                TEEC_VALUE_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);
        operation.started = 1;

        operation.params[0].memref.parent = &share_mem;
        operation.params[0].memref.offset = 0;
        operation.params[0].memref.size   = 0;

        err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_MEM_WRITE, &operation, NULL);
        TEEC_ReleaseSharedMemory(&share_mem);
    } else {
        operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
                TEEC_VALUE_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);

        operation.params[0].value.a = *((uint32_t*)src);
        err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
                MSG_ADPATER_MEM_WRITE_INT, &operation, NULL);
    }

    return err;
}

int HwSecureAllocSetupHW()
{
    logv("SecureAdapterSetupHW");
    pthread_mutex_lock(&gAdapterMutex);
    if(gAdapterContext == NULL) {
        pthread_mutex_unlock(&gAdapterMutex);
        return -1;
    }
    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
            TEEC_NONE, TEEC_NONE);

    operation.started = 1;
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_SETUP_VE, &operation, NULL);
    pthread_mutex_unlock(&gAdapterMutex);
    return err;
}

int HwSecureAllocShutdownHW()
{
    logv("SecureAdapterShutdownHW");
    pthread_mutex_lock(&gAdapterMutex);
    //do not abort here, so codec can shutdown drm protection
    //without caring whether secure os is initialized or not.
    if(gAdapterContext == NULL) {
        pthread_mutex_unlock(&gAdapterMutex);
        return -1;
    }
    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE,
            TEEC_NONE, TEEC_NONE);

    operation.started = 1;
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_SHUTDOWN_VE, &operation, NULL);
    pthread_mutex_unlock(&gAdapterMutex);
    return err;
}

int HwSecureAllocDump(void *ptr, size_t size)
{
    CEDARC_UNUSE(HwSecureAllocDump);

    logv("SecureMemAdapterDump");
    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    SE_ASSERT(gAdapterContext != NULL);

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
            TEEC_VALUE_INPUT, TEEC_NONE, TEEC_NONE);
    operation.started = 1;
    #if(ADJUST_ADDRESS_FOR_SECURE_OS_OPTEE)
    operation.params[0].value.a = (uintptr_t)ptr;
    #else
    operation.params[0].value.a = (uintptr_t)HwSecureAllocGetPhysicAddressCpu(ptr);
    #endif
    operation.params[0].value.b = (uint32_t)size;

    if(operation.params[0].value.a == 0) {
        loge("dump with invalid address");
         return -1;
     }
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_MEM_DUMP, &operation, NULL);
    return err;
}

int HwSecureAllocDebug(int s)
{
    CEDARC_UNUSE(HwSecureAllocDebug);

    logv("SecureMemAdapterDebug");
    TEEC_Operation operation;
    memset(&operation, 0, sizeof(TEEC_Operation));

    SE_ASSERT(gAdapterContext != NULL);

    operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
            TEEC_NONE, TEEC_NONE, TEEC_NONE);
    operation.started = 1;
    operation.params[0].value.a = (uint32_t)s;
    logv("operation.params[0].value.a %d", operation.params[0].value.a);
    TEEC_Result err = TEEC_InvokeCommand(&gAdapterContext->tee_session,
            MSG_ADPATER_MEM_DEBUG, &operation, NULL);
    return err;
}
//****************************end*************************//

int SwSecureAllocOpen()
{
    logv("SecureMemAdapterOpen");
    int err = 0;
    if(gAdapterContext == NULL)
    {
        gAdapterContext = (AdapterContext *)malloc(sizeof(AdapterContext));
        SE_ASSERT(gAdapterContext != NULL);
        memset(gAdapterContext, 0, sizeof(AdapterContext));

        gAdapterContext->ionMemOps = __GetIonMemOpsS();
        err = CdcMemOpen(gAdapterContext->ionMemOps);
    }
    if(err == 0)
    {
        gAdapterContext->ref_count++;
    }
    return err;
}

void SwSecureAllocClose()
{
    logv("SecureMemAdapterClose");
    if(!gAdapterContext)
    {
        loge("secure adapter has not been initialized");
        return ;
    }
    if(--gAdapterContext->ref_count == 0)
    {
        CdcMemClose(gAdapterContext->ionMemOps);
        free(gAdapterContext);
        gAdapterContext = NULL;
    }
    return ;
}

int SwSecureAllocGetTotalSize()
{
    logv("SwSecureAllocGetTotalSize");
    return CdcMemTotalSize(gAdapterContext->ionMemOps);
}

void *SwSecureAllocPalloc(int size, void *veOps, void *pVeopsSelf)
{
    logv("SwSecureAllocPalloc");
    return CdcMemPalloc(gAdapterContext->ionMemOps, size, veOps, pVeopsSelf);
}

void SwSecureAllocPfree(void *ptr, void *veOps, void *pVeopsSelf)
{
    logv("SwSecureAllocPfree");
    CdcMemPfree(gAdapterContext->ionMemOps, ptr, veOps, pVeopsSelf);
}

void SwSecureAllocFlushCache(void *ptr, int size)
{
    logv("SecureMemAdapterFlushCache");
    CdcMemFlushCache(gAdapterContext->ionMemOps, ptr, size);
}

void * SwSecureAllocGetPhysicAddress(void *virt)
{
    logv("SecureMemAdapterGetPhysicAddress");
    return CdcMemGetPhysicAddress(gAdapterContext->ionMemOps, virt);
}

void * SwSecureAllocGetVirtualAddress(void *phy)
{
    logv("SwSecureAllocGetVirtualAddress");
    return CdcMemGetVirtualAddress(gAdapterContext->ionMemOps, phy);
}

void* SwSecureAllocGetPhysicAddressCpu(void *virt)
{
    logv("SwSecureAllocGetPhysicAddressCpu");
    return CdcMemGetPhysicAddressCpu(gAdapterContext->ionMemOps, virt);
}

void* SwSecureAllocGetVirtualAddressCpu(void *phy)
{
    logv("SecureMemAdapterGetVirtualAddress");
    return CdcMemGetVirtualAddressCpu(gAdapterContext->ionMemOps, phy);
}

int SwSecureAllocSet(void *s, int c, size_t n)
{
    logv("SecureMemAdapterSet");
    memset(s, c, n);
    return 0;
}

int SwSecureAllocCopy(void *dest, void *src, size_t n)
{
    logv("SecureMemAdapterCopy");
    memcpy(dest, src, n);
    return 0;
}

int SwSecureAllocRead(void *dest, void *src, size_t n)
{
    logv("SecureMemAdapterRead");
    memcpy(dest, src, n);
    return n;
}

int SwSecureAllocWrite(void *dest, void *src, size_t n)
{
    logv("SecureMemAdapterWrite");
    memcpy(dest, src, n);
    return n;
}

int SwSecureAllocSetupHW()
{
    return 0;
}

int SwSecureAllocShutdownHW()
{
    return 0;
}

#if 0
int SwSecureAllocDump(void *ptr ATTRIBUTE_UNUSED, size_t size ATTRIBUTE_UNUSED)
{
    logv("SecureMemAdapterDump");
    return 0;
}

int SwSecureAllocDebug(int s ATTRIBUTE_UNUSED)
{
    return 0;
}
#endif

struct ScMemOpsS _HwSecureMemOpsS =
{
    open:                 HwSecureAllocOpen,
    close:                 HwSecureAllocClose,
    total_size:         HwSecureAllocGetTotalSize,
    palloc:             HwSecureAllocPalloc,
    pfree:                HwSecureAllocPfree,
    flush_cache:        HwSecureAllocFlushCache,
    ve_get_phyaddr:     HwSecureAllocGetPhysicAddress,
    ve_get_viraddr:     HwSecureAllocGetVirtualAddress,
    cpu_get_phyaddr:    HwSecureAllocGetPhysicAddressCpu,
    cpu_get_viraddr:    HwSecureAllocGetVirtualAddressCpu,
    mem_set:            HwSecureAllocSet,
    mem_cpy:            HwSecureAllocCopy,
    mem_read:            HwSecureAllocRead,
    mem_write:            HwSecureAllocWrite,
    setup:                HwSecureAllocSetupHW,
    shutdown:            HwSecureAllocShutdownHW

};

struct ScMemOpsS _SwSecureMemOpsS =
{
    open:                 SwSecureAllocOpen,
    close:                 SwSecureAllocClose,
    total_size:         SwSecureAllocGetTotalSize,
    palloc:             SwSecureAllocPalloc,
    pfree:                SwSecureAllocPfree,
    flush_cache:        SwSecureAllocFlushCache,
    ve_get_phyaddr:     SwSecureAllocGetPhysicAddress,
    ve_get_viraddr:     SwSecureAllocGetVirtualAddress,
    cpu_get_phyaddr:    SwSecureAllocGetPhysicAddressCpu,
    cpu_get_viraddr:    SwSecureAllocGetVirtualAddressCpu,
    mem_set:            SwSecureAllocSet,
    mem_cpy:            SwSecureAllocCopy,
    mem_read:            SwSecureAllocRead,
    mem_write:            SwSecureAllocWrite,
    setup:                SwSecureAllocSetupHW,
    shutdown:            SwSecureAllocShutdownHW
};

struct ScMemOpsS* __GetSecureMemOpsS()
{
    logd("*** __GetSecureMemOpsS ***");

#if(PLATFORM_SURPPORT_SECURE_OS == 1)
    return &_HwSecureMemOpsS;
#else
    return &_SwSecureMemOpsS;
#endif
}
