/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : oggCore.c
 * Description : oggCore
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "OggParseAuxiliary"
#include "CdxOggParser.h"
#include <cdx_log.h>
#include <string.h>

void aw_freep(void *arg)
{
    void **ptr= (void**)arg;
    free(*ptr);
    *ptr = NULL;
}

void *av_mallocz(cdx_uint32 size)
{
    void *ptr = malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}
unsigned int avpriv_toupper4(unsigned int x)
{
    return toupper(x & 0xFF) +
          (toupper((x >>  8) & 0xFF) << 8)  +
          (toupper((x >> 16) & 0xFF) << 16) +
          (toupper((x >> 24) & 0xFF) << 24);
}

enum CodecID cdx_codec_get_id(const AVCodecTag *tags, unsigned int tag)
{
    int j;
    for(j=0; CDX_CODEC_ID_NONE != tags[j].id ;j++) {
        if( tags[j].tag == tag )
            return tags[j].id;
    }
    for(j=0; CDX_CODEC_ID_NONE != tags[j].id; j++) {
        if ( avpriv_toupper4(tags[j].tag)== avpriv_toupper4(tag) )
            return tags[j].id;
    }
    return CDX_CODEC_ID_NONE;
}
