/*
********************************************************************************
*                                    eMOD
*                   the Easy Portable/Player Develop Kits
*                               mod_cache sub-system
*                          (module name, e.g.mpeg4 decoder plug-in) module
*
*          (c) Copyright 2010-2012, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : cache_fs_base.c
* Version: V1.0
* By     : Eric_wang
* Date   : 2011-2-21
* Modify : zengzhijin 2020-04-24
* Description:
********************************************************************************
*/
#include "cache_fs_s.h"
#include "cache_fs_base.h"
#include <string.h>

__s32 CacheFS_Init(CacheFS *thiz)
{
    memset(thiz, 0, sizeof(CacheFS));
    return EPDK_OK;
}

__attribute__((weak)) __s32 eLIBs_fllseek(ES_FILE *stream, __s64 Offset, __s32 Whence)
{
    if (esFSYS_fseekex((__hdle)stream, Offset, Offset >> 32, Whence) >= 0)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

__attribute__((weak)) __s64 eLIBs_flltell(ES_FILE *stream)
{
    __s32 l_pos;
    __s32 h_pos;

    if (esFSYS_ftellex((__hdle)stream, &l_pos, &h_pos) == EPDK_OK)
    {
        return ((0x00000000FFFFFFFF & ((__s64)l_pos)) | (((__s64)h_pos) << 32));
    }

    return EPDK_FAIL;
}
