/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviDepackLib.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _CDX_AVI_DEPACK_LIB_H_
#define _CDX_AVI_DEPACK_LIB_H_

#include <CdxTypes.h>

typedef enum __AVI_DEPACK_RETURN_VAL
{
    AVI_SUCCESS                     = 0,
    AVI_ERR_READ_END                ,//= 1
    AVI_ERR_NO_INDEX_TABLE          ,//= 2
    AVI_ERR_INDEX_HAS_NO_KEYFRAME   ,//= 3
    AVI_ERR_SEARCH_INDEX_CHUNK_END  ,//= 4   //INDEX TABLE search chunk end
    AVI_ERR_GET_INDEX_ERR           ,//= 5
    AVI_ERR_CORRECT_STREAM_INFO     ,//= 6
    AVI_ERR_PART_INDEX_TABLE        ,//= 7   //index table
    AVI_ERR_IGNORE                  ,//= 8

    AVI_ERR_PARA_ERR                = -1,
    AVI_ERR_OPEN_FILE_FAIL          = -2,
    AVI_ERR_READ_FILE_FAIL          = -3,
    AVI_ERR_FILE_FMT_ERR            = -4,
    AVI_ERR_NO_AV                   = -5,
    AVI_ERR_END_OF_MOVI             = -6,
    AVI_ERR_REQMEM_FAIL             = -7,
    AVI_EXCEPTION                   = -8,
    AVI_ERR_CHUNK_TYPE_NONE         = -9,
    AVI_ERR_FAIL                    = -10,
    AVI_ERR_FILE_DATA_WRONG         = -11,
    AVI_ERR_MEM_OVERFLOW            = -12,
    AVI_ERR_CHUNK_OVERFLOW          = -13,
    AVI_ERR_ABORT                   = -14,
    AVI_ERR_,
} __avi_depack_return_val_t;

#endif  /* _CDX_AVI_DEPACK_LIB_H_ */

