/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdcSysinfo.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef CDX_SYS_INFO_H
#define CDX_SYS_INFO_H


#ifdef __cplusplus
extern "C"
{
#endif

//* get current ddr frequency, if it is too slow, we will cut some spec off.
int CdcGetDramFreq();


enum CHIPID
{
    SI_CHIP_UNKNOWN = 0,
    SI_CHIP_H3s = 1,
    SI_CHIP_H3 = 2,
    SI_CHIP_H2 = 3,
    SI_CHIP_H2PLUS = 4,
};

int CdcGetSysinfoChipId(void);


#ifdef __cplusplus
}
#endif

#endif

