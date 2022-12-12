
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : memoryAdapter.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#include "memoryAdapter.h"

#include "cdc_log.h"
#include "ionMemory/ionAllocEntry.h"
#include "secureMemory/secureAllocEntry.h"


#define NODE_DDR_FREQ    "/sys/class/devfreq/sunxi-ddrfreq/cur_freq"
static int readNodeValue(const char *node, char * strVal, size_t size)
{
    int ret = -1;
    int fd = open(node, O_RDONLY);
    if (fd >= 0)
    {
        read(fd, strVal, size);
        close(fd);
        ret = 0;
    }
    return ret;
}

//* get current ddr frequency, if it is too slow, we will cut some spec off.
int MemAdapterGetDramFreq()
{
    CEDARC_UNUSE(MemAdapterGetDramFreq);

    char freq_val[8] = {0};
    if (!readNodeValue(NODE_DDR_FREQ, freq_val, 8))
    {
        return atoi(freq_val);
    }

    // unknow freq
    return -1;
}

struct ScMemOpsS* MemAdapterGetOpsS()
{
    return __GetIonMemOpsS();
}

struct ScMemOpsS* SecureMemAdapterGetOpsS()
{
#if(PLATFORM_SURPPORT_SECURE_OS == 1)
    return __GetSecureMemOpsS();
#else
    loge("platform not surpport secure os, return null");
    return NULL;
#endif
}

