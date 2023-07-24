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
* File   : cache_fs_s.h
* Version: V1.0
* By     : Eric_wang
* Date   : 2011-2-21
* Modify : zengzhijin 2020-04-24
* Description:
********************************************************************************
*/
#ifndef _CACHE_FS_S_H_
#define _CACHE_FS_S_H_

void aligned_free(void *pbuf);

#define PHY_MALLOC(size, align)         aligned_alloc(align, size)
#define PHY_FREE(pbuf)                  aligned_free(pbuf)

/* 对外的API */
#include "cache_fs.h"

/* 内部共用头文件，定义的需要内部实现的头文件 */
#include "cache_fs_base.h"
#include "cache_fs_normal.h"
#include "cache_fs_align.h"
#include "cache_fs_direct.h"
#include "herb_fs_cache.h"
#include "cache_fs_debug.h"

#endif  /* _CACHE_FS_S_H_ */

