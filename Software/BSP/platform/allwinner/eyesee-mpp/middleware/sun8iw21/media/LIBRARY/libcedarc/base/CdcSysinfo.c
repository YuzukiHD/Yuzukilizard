
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdcSysinfo.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/


#include "cdc_log.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#include <CdcSysinfo.h>

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
int CdcGetDramFreq()
{
    CEDARC_UNUSE(CdcGetDramFreq);

    char freq_val[8] = {0};
    if (!readNodeValue(NODE_DDR_FREQ, freq_val, 8))
    {
        return atoi(freq_val);
    }

    // unknow freq
    return -1;
}


int CdcGetSysinfoChipId(void)
{
    int ret = 0;
    int dev = 0;
    char buf[16]={0};

    dev = open("/dev/sunxi_soc_info", O_RDONLY);
    if (dev < 0)
    {
        logv("cannot open /dev/sunxi_soc_info\n");
        return  SI_CHIP_UNKNOWN;
    }

    ret = ioctl(dev, 3, buf);
    if(ret < 0)
    {
        loge("ioctl err!\n");
        close(dev);
        return  SI_CHIP_UNKNOWN;
    }

    logd("%s\n", buf);

    close(dev);

    if(!strncmp(buf, "00000000", 8) ||
       !strncmp(buf, "00000081", 8))
    {
        ret = SI_CHIP_H3;
    }
    else if(!strncmp(buf, "00000042", 8) ||
            !strncmp(buf, "00000083", 8))
    {
        ret = SI_CHIP_H2PLUS;
    }
    else if(!strcmp(buf, "H2"))    // deprecated
    {
        ret = SI_CHIP_H2;
    }
    else if(!strcmp(buf, "H3s"))// deprecated
    {
        ret = SI_CHIP_H3s;
    }
    else
    {
        ret = SI_CHIP_UNKNOWN;
    }
    return ret;
}


