/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviDepackCoreCfg.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _CDX_AVI_DEPACK_CORE_CFG_H_
#define _CDX_AVI_DEPACK_CORE_CFG_H_

#include <CdxTypes.h>

#define MAX_CHUNK_BUF_SIZE (1024*512)

//for keyframe table, 128k default 366600byte, index 366600/40byte = 9165 keyframe

#define MAX_IDX_BUF_SIZE                (1024 * 16 * 27)
#define MAX_IDX_BUF_SIZE_FOR_INDEX_MODE (1024 * 16 * 41)
#define INDEX_CHUNK_BUFFER_SIZE         (1024 * 128)    //for store index chunk in readmode_index
#define MAX_FRAME_CHUNK_SIZE            (2 * 1024 * 1024)//max frame should not larger than 2M byte
#define AVI_STREAM_NAME_SIZE            (32)

#define MAX_READ_CHUNK_ERROR_NUM   (100)

//debug
#define PLAY_AUDIO_BITSTREAM    (1)
#define PLAY_VIDEO_BITSTREAM    (1)

#endif  /* _CDX_AVI_DEPACK_CORE_CFG_H_ */

