
//#define LOG_NDEBUG 0
#define LOG_TAG "ByteIOContext"
#include <utils/plat_log.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include <encoder_type.h>
#include "Mp4Muxer.h"
#include "ByteIOContext.h"

void put_buffer_cache(void *mov, ByteIOContext *s, char *buf, int size)
{
#if 0
    int64_t tm1, tm2, tm3;
    tm1 = CDX_GetNowUs();
#endif
	//mov->mpFsWriter->fsWrite(mov->mpFsWriter, buf, size);
    s->fsWrite(s, buf, size);
#if 0
    tm2 = CDX_GetNowUs();
    if((tm2-tm1)/1000 > 40)
    {
        if(mov->tracks[0].enc)
        {
            alogd("[%dx%d]Be careful, forbid to appear this case: fwrite [%d]kByte, [%lld]ms >40ms", 
                __FUNCTION__, __LINE__, mov->tracks[0].enc->width, mov->tracks[0].enc->height, size/1024, (tm2-tm1)/1000);
        }
        else
        {
            alogd("Be careful, forbid to appear this case: fwrite [%d]kByte, [%lld]ms >40ms", 
                __FUNCTION__, __LINE__, size/1024, (tm2-tm1)/1000);
        }
        
    }
#endif
    return;
}

void put_byte_cache(void *mov, ByteIOContext *s, int b)
{
    put_buffer_cache(mov, s, (char*)&b, 1);
}

void put_le32_cache(void *mov, ByteIOContext *s, unsigned int val)
{
	put_buffer_cache(mov, s, (char*)&val, 4);
}

void put_be32_cache(void *mov, ByteIOContext *s, unsigned int val)
{
	val= ((val<<8)&0xFF00FF00) | ((val>>8)&0x00FF00FF);
    val= (val>>16) | (val<<16);
    put_buffer_cache(mov, s, (char*)&val, 4);
}

// void put_le16_cache(ByteIOContext *s, unsigned int val)
// {
//     put_byte(s, val);
//     put_byte(s, val >> 8);
// }
// 
void put_be16_cache(void *mov, ByteIOContext *s, unsigned int val)
{
    put_byte_cache(mov, s, val >> 8);
    put_byte_cache(mov, s, val);
}
// 
// void put_le24_cache(ByteIOContext *s, unsigned int val)
// {
//     put_le16(s, val & 0xffff);
//     put_byte(s, val >> 16);
// }
// 
void put_be24_cache(void *mov, ByteIOContext *s, unsigned int val)
{
    put_be16_cache(mov, s, val >> 8);
    put_byte_cache(mov, s, val);
}

void put_tag_cache(void *mov, ByteIOContext *s, const char *tag)
{
    while (*tag) {
        put_byte_cache(mov, s, *tag++);
    }
}

