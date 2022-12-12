/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : VideoSpecificData.c
 * Description : VideoSpecificData
 * History :
 *
 */

#include <CdxParser.h>
#include <CdxBitReader.h>
#include <string.h>

typedef struct GET_BIT_CONTEXT
{
    cdx_uint8 *buffer;
    cdx_uint8 *buffer_end;
    cdx_int32 index;
    cdx_int32 size_in_bits;
}GetBitContext;

#define AV_RB32(x)  ((((const cdx_uint8*)(x))[0] << 24) | \
                (((const cdx_uint8*)(x))[1] << 16) | \
                (((const cdx_uint8*)(x))[2] <<  8) | \
                ((const cdx_uint8*)(x))[3])

#    define NEG_SSR32(a,s) (((cdx_int32)(a))>>(32-(s)))
#    define NEG_USR32(a,s) (((cdx_uint32)(a))>>(32-(s)))

#   define MIN_CACHE_BITS 25

#   define OPEN_READER(name, gb)\
        cdx_int32 name##_index= (gb)->index;\
cdx_int32 name##_cache= 0;\

#   define CLOSE_READER(name, gb)\
        (gb)->index= name##_index;\

#   define UPDATE_CACHE(name, gb)\
        name##_cache= AV_RB32( ((const cdx_uint8 *)(gb)->buffer)+(name##_index>>3) ) \
        << (name##_index&0x07);\

#   define SKIP_CACHE(name, gb, num)\
        name##_cache <<= (num);

#   define SHOW_UBITS(name, gb, num)\
        NEG_USR32(name##_cache, num)

#   define SHOW_SBITS(name, gb, num)\
        NEG_SSR32(name##_cache, num)

#   define SKIP_COUNTER(name, gb, num)\
        name##_index += (num);\

#   define SKIP_BITS(name, gb, num)\
{\
        SKIP_CACHE(name, gb, num)\
        SKIP_COUNTER(name, gb, num)\
}\

#   define LAST_SKIP_BITS(name, gb, num) SKIP_COUNTER(name, gb, num)
#   define LAST_SKIP_CACHE(name, gb, num) ;

#   define GET_CACHE(name, gb)\
        ((cdx_uint32)name##_cache)

const cdx_uint8 golomb_vlc_len[512]={
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

cdx_uint8 ue_golomb_vlc_code[512]={
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,
        7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,
        11,11,11,11,12,12,12,12,13,13,13,13,14,14,14,14,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

cdx_uint8 log2_tab[256]={
        0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

const cdx_int8 se_golomb_vlc_code[512]={
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        8, -8,  9, -9, 10,-10, 11,-11, 12,-12, 13,-13, 14,-14, 15,-15,
        4,  4,  4,  4, -4, -4, -4, -4,  5,  5,  5,  5, -5, -5, -5, -5,
        6,  6,  6,  6, -6, -6, -6, -6,  7,  7,  7,  7, -7, -7, -7, -7,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
        3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
        -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3,
        1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
        1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
        1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
        1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

#define min(x, y)   ((x) <= (y) ? (x) : (y));

static unsigned char getbits(unsigned char* buffer, unsigned int from, unsigned char len)
{
    unsigned int n;
    unsigned char  m, u, l, y;

    n = from / 8;
    m = from % 8;
    u = 8 - m;
    l = (len > u ? len - u : 0);

    y = (buffer[n] << m);
    if(8 > len)
        y  >>= (8-len);
    if(l)
        y |= (buffer[n+1] >> (8-l));

    return  y;
}

static cdx_int32 av_log2(cdx_uint32 v)
{
    cdx_int32 n = 0;
    if (v & 0xffff0000)
    {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00)
    {
        v >>= 8;
        n += 8;
    }
    n += log2_tab[v];

    return n;
}
static cdx_uint32 getBits(GetBitContext *s, cdx_int32 n)
{
    register cdx_int32 tmp;
    OPEN_READER(re, s)
    UPDATE_CACHE(re, s)
    tmp= SHOW_UBITS(re, s, n);
    LAST_SKIP_BITS(re, s, n)
    CLOSE_READER(re, s)
    return tmp;
}

static cdx_uint32 getBits1(GetBitContext *s)
{
    cdx_int32 index= s->index;
    cdx_uint8 result = s->buffer[index>>3] ;
    result<<= (index&0x07);
    result>>= 8 - 1;
    index++;
    s->index= index;
    return result;
}

static cdx_int32 getUeGolomb(GetBitContext *gb)
{
    cdx_uint32 buf;
    cdx_int32 log;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf=GET_CACHE(re, gb);

    if(buf >= (1<<27))
    {
        buf >>= 32 - 9;
        LAST_SKIP_BITS(re, gb, golomb_vlc_len[buf]);
        CLOSE_READER(re, gb);
        return ue_golomb_vlc_code[buf];
    }
    else
    {
        log= 2*av_log2(buf) - 31;
        buf>>= log;
        buf--;
        LAST_SKIP_BITS(re, gb, 32 - log);
        CLOSE_READER(re, gb);
        return buf;
    }
}

static void initGetBits(GetBitContext *s, cdx_uint8 *buffer, cdx_uint32 bit_size)
{
    cdx_int32 buffer_size = (bit_size+7)>>3;
    s->buffer = (cdx_uint8*)buffer;
    s->size_in_bits = (cdx_int32)bit_size;
    s->buffer_end = (cdx_uint8*)(buffer + buffer_size);
    s->index=0;
}

static cdx_int32 getSeGolomb(GetBitContext *gb)
{
    cdx_int32 buf;
    cdx_int32 log;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf=GET_CACHE(re, gb);

    if(buf >= (1<<27))
    {
        buf >>= 32 - 9;
        LAST_SKIP_BITS(re, gb, golomb_vlc_len[buf]);
        CLOSE_READER(re, gb);
        return se_golomb_vlc_code[buf];
    }
    else
    {
        log= 2*av_log2(buf) - 31;
        buf>>= log;
        LAST_SKIP_BITS(re, gb, 32 - log);
        CLOSE_READER(re, gb);

        if(buf&1)
            buf= -(buf>>1);
        else
            buf=  (buf>>1);
        return buf;
    }
}

void decode_scaling_list(GetBitContext* sBitContextPtr, cdx_int32 size)
{
    cdx_int32 i, last = 8, next = 8;

    if(!getBits1(sBitContextPtr)) /* matrix not written, we use the predicted one */
    {
        return;
    }
    else
    {
        for(i=0;i<size;i++)
        {
            if(next)
                next = (last + getSeGolomb(sBitContextPtr)) & 0xff;
            if(!i && !next)
            { /* matrix not written, we use the preset one */
                break;
            }
            if(next)
            {
               last = next;
            }
        }
    }
    return;
}

void decode_scaling_matrices(GetBitContext* sBitContextPtr)
{
    if(getBits1(sBitContextPtr))
    {
        decode_scaling_list(sBitContextPtr,16); // Intra, Y
        decode_scaling_list(sBitContextPtr,16); // Intra, Cr
        decode_scaling_list(sBitContextPtr,16); // Intra, Cb
        decode_scaling_list(sBitContextPtr,16); // Inter, Y
        decode_scaling_list(sBitContextPtr,16); // Inter, Cr
        decode_scaling_list(sBitContextPtr,16); // Inter, Cb
        decode_scaling_list(sBitContextPtr,64);  // Intra, Y
        decode_scaling_list(sBitContextPtr,64);  // Inter, Y
    }
}

cdx_int32 probeH264SizeInfo(VideoStreamInfo* pVideoInfo,
        cdx_uint8* pDataBuf, cdx_uint32 nDataLen)
{
    cdx_int32 pocType = 0;
    cdx_int32 tmp = 0;
    cdx_int32 i = 0;
    cdx_int32 pocCycleLength = 0;
    cdx_uint8 profileIdc = 0;
    cdx_uint32 refFrameCount = 0;
    cdx_uint32 mbWidth = 0;
    cdx_uint32 mbHeight = 0;
    cdx_uint32 frameMbsOnlyFlag = 0;
    cdx_uint32 width = 0;
    cdx_uint32 height = 0;
    GetBitContext sBitContext;
    initGetBits(&sBitContext, pDataBuf, nDataLen*8);

    profileIdc = getBits(&sBitContext, 8);
    getBits1(&sBitContext);   //constraint_set0_flag
    getBits1(&sBitContext);   //constraint_set1_flag
    getBits1(&sBitContext);   //constraint_set2_flag
    getBits1(&sBitContext);   //constraint_set3_flag
    getBits(&sBitContext, 4); // reserved
    getBits(&sBitContext, 8);
    getUeGolomb(&sBitContext);

    if(profileIdc >= 100)
    { //high profile
        tmp = getUeGolomb(&sBitContext);
        if(tmp == 3) //chroma_format_idc
        {
            getBits1(&sBitContext);  //residual_color_transform_flag
        }
        getUeGolomb(&sBitContext);  //bit_depth_luma_minus8
        getUeGolomb(&sBitContext);  //bit_depth_chroma_minus8
        getBits1(&sBitContext);
        decode_scaling_matrices(&sBitContext);
    }

    getUeGolomb(&sBitContext);
    pocType = getUeGolomb(&sBitContext);

    if(pocType == 0)
    { //FIXME #define
        getUeGolomb(&sBitContext);
    }
    else if(pocType == 1)
    {//FIXME #define
        getBits1(&sBitContext);
        getSeGolomb(&sBitContext);
        getSeGolomb(&sBitContext);
        pocCycleLength = getUeGolomb(&sBitContext);

        for(i=0; i<pocCycleLength; i++)
        {
            getSeGolomb(&sBitContext);
        }
    }
    else if(pocType != 2)
    {
        CDX_LOGE("pocType != 2");
        return PROBE_SPECIFIC_DATA_ERROR;
    }

    refFrameCount = getUeGolomb(&sBitContext);
    getBits1(&sBitContext);
    mbWidth = getUeGolomb(&sBitContext) + 1;
    mbHeight = getUeGolomb(&sBitContext) + 1;
    frameMbsOnlyFlag= getBits1(&sBitContext);

    width = mbWidth*16;
    height = (2-frameMbsOnlyFlag)*mbHeight*16;
    if(pVideoInfo->nWidth == 0)
    {
        pVideoInfo->nWidth = width;
    }
    if(pVideoInfo->nHeight == 0)
    {
        pVideoInfo->nHeight = height;
    }
    //CDX_LOGD("width(%d), height(%d), pVideoInfo->nWidth(%d)", width, height, pVideoInfo->nWidth);
    return PROBE_SPECIFIC_DATA_SUCCESS;
}

cdx_int32 probeStartCode(cdx_uint8* pDataBuf, cdx_uint32 dataLen, cdx_uint32 startCode)
{
    cdx_uint32 i = 0;

    while(i < dataLen)
    {
        startCode <<= 8;
        startCode += pDataBuf[i];
        if((startCode&0xffffff) == 0x01)
        {
            return i;
        }
        i++;
    }
    return -1;
}

cdx_int32 probeConvertAvcSpecificData(cdx_uint8* dstBuf, cdx_uint8* orgBuf,
                                      cdx_uint32 orgDataLen, cdx_uint32* dstDataLen)
{
    (void)orgDataLen;
    cdx_uint8 i = 0;
    cdx_uint8  cnt = 0;
    cdx_uint16 nalSize = 0;

    *dstDataLen = 0;
    cnt = *(orgBuf+5) & 0x1f;      // Number of sps
    orgBuf += 6;
    while(i<2)
    {
        for(;cnt>0; cnt--)
        {
            dstBuf[0] = 0x00;
            dstBuf[1] = 0x00;
            dstBuf[2] = 0x00;
            dstBuf[3] = 0x01;
            nalSize = (orgBuf[0]<<8)|orgBuf[1];
            orgBuf += 2;
            memcpy(dstBuf+4,orgBuf, nalSize);
            dstBuf += 4+nalSize;
            orgBuf += nalSize;
            *dstDataLen += 4+nalSize;
        }
        cnt = *(orgBuf) & 0xff;      // Number of pps
        orgBuf += 1;
        i++;
    }
    return 0;
}
cdx_int32 VerifyAvcSpecificDataForPacketOriented(VideoStreamInfo* pVideoInfo,
                                                 cdx_uint8* data, cdx_uint32 orgDataLen)
{
    cdx_uint8 i = 0, j = 0;
    int ret = 0;
    cdx_uint8  cnt = 0;
    cdx_uint16 nalSize = 0;
    cdx_uint8* orgBuf = data;
    cnt = *(orgBuf+5) & 0x1f;      // Number of sps
    orgBuf += 6;
    while(i<2)
    {
        for(;cnt>0; cnt--)
        {
            nalSize = (orgBuf[0]<<8)|orgBuf[1];
            orgBuf += 2;
            if(i == 0 && j == 0)
            {
                ret = probeH264SizeInfo(pVideoInfo, orgBuf+1, nalSize-1);
                if(ret != PROBE_SPECIFIC_DATA_SUCCESS)
                {
                    CDX_LOGE("probeH264SizeInfo fail.");
                    return ret;
                }
            }
            j++;
            orgBuf += nalSize;
        }
        ++i;
        if(orgBuf > data + orgDataLen)
        {
            return PROBE_SPECIFIC_DATA_UNCOMPELETE;
        }
        else if(i == 2)
        {
            pVideoInfo->pCodecSpecificData = malloc(orgDataLen);
            if(pVideoInfo->pCodecSpecificData != NULL)
            {
                memcpy(pVideoInfo->pCodecSpecificData, data, orgDataLen);
                pVideoInfo->nCodecSpecificDataLen = orgDataLen;
                return PROBE_SPECIFIC_DATA_SUCCESS;
            }
            else
            {
                CDX_LOGE("malloc fail.");
                return PROBE_SPECIFIC_DATA_ERROR;
            }
        }
        cnt = (*orgBuf++) & 0xff;      // Number of pps
    }
    return PROBE_SPECIFIC_DATA_ERROR;
}

cdx_int32 probeH264SpecificData(VideoStreamInfo* pVideoInfo,
                                cdx_uint8* pDataBuf, cdx_uint32 nDataLen)
{
    cdx_int32 nStartCodeIndex;
    cdx_uint32 i = 0;
    cdx_uint32 j = 0;
    cdx_int32 ret = 0;
    cdx_uint32 nIndex[3] = {0};
    cdx_uint8 spsData[1024];
    cdx_uint8* pSpecificDataPtr = NULL;
    cdx_uint8* ppDataPtr = NULL;

    cdx_uint32 nSpecificDataLen=0;
    cdx_int32 nnDatalen = 0;

    nStartCodeIndex = probeStartCode(pDataBuf, nDataLen, 0x00);

    if(nStartCodeIndex == -1)
    {
        CDX_LOGE("cannot find the startcode");
        return PROBE_SPECIFIC_DATA_NONE;
    }
    if((nDataLen-nStartCodeIndex) < 7)
    {
        CDX_LOGE("the data len is %d, is smaller than 7", nDataLen-nStartCodeIndex);
        return PROBE_SPECIFIC_DATA_UNCOMPELETE;
    }
    if(nStartCodeIndex == 0)
    {
        return VerifyAvcSpecificDataForPacketOriented(pVideoInfo, pDataBuf, nDataLen);
    }
    pSpecificDataPtr = pDataBuf;
    nSpecificDataLen = nDataLen;
    ppDataPtr = pSpecificDataPtr;
    nnDatalen = (int)nSpecificDataLen;
    while(1)
    {
        nStartCodeIndex = probeStartCode(ppDataPtr,nnDatalen, 0xffffffff);
        //CDX_LOGD("here11: nStartCodeIndex=%d, ppDataPtr=%x,
        //nnDatalen=%d\n",nStartCodeIndex, ppDataPtr, nnDatalen);
        if(nStartCodeIndex == -1)
        {
            if(i==0)
            {
                //CDX_LOGD("return error 1\n");
                return PROBE_SPECIFIC_DATA_NONE;
            }
            else
            {
                //CDX_LOGD("return error 2\n");
                return PROBE_SPECIFIC_DATA_UNCOMPELETE;
            }
        }

        if((i==0) && (ppDataPtr[nStartCodeIndex+1]&0x01f) == 7)
        {
            nIndex[i++] = (ppDataPtr-pSpecificDataPtr)+nStartCodeIndex+2;
            //CDX_LOGD("here22: pSpecificDataPtr[nIndex[0]-1]=%x,
            //pSpecificDataPtr[nIndex[0]]=%x\n", pSpecificDataPtr[nIndex[0]-1],
            //pSpecificDataPtr[nIndex[0]]);
        }
        else if((i==1) && (ppDataPtr[nStartCodeIndex+1]&0x01f) == 8)
        {
            nIndex[i++] = (ppDataPtr-pSpecificDataPtr)+nStartCodeIndex+2;
            //CDX_LOGD("here33: pSpecificDataPtr[nIndex[1]-1]=%x,
            //pSpecificDataPtr[nIndex[1]]=%x\n", pSpecificDataPtr[nIndex[1]-1],
            //pSpecificDataPtr[nIndex[1]]);
        }
        else if(i==2)
        {
            nIndex[i++] = (ppDataPtr-pSpecificDataPtr)+nStartCodeIndex+2;
            //CDX_LOGD("here44: pSpecificDataPtr[nIndex[2]-1]=%x,
            //pSpecificDataPtr[nIndex[2]]=%x\n", pSpecificDataPtr[nIndex[2]-1],
            //pSpecificDataPtr[nIndex[2]]);
            break;
        }
        ppDataPtr += nStartCodeIndex+2;
        nnDatalen -= nStartCodeIndex+2;
        if(nnDatalen <= 0)
        {
            CDX_LOGD("*****");
            return (i==0)? PROBE_SPECIFIC_DATA_NONE : PROBE_SPECIFIC_DATA_UNCOMPELETE;
        }
    }

    // skip the 0x03 data
    for(i=nIndex[0]; i<(nIndex[1]-4); )
    {
        if(pSpecificDataPtr[i]==0x00 && pSpecificDataPtr[i+1]==0x00 && pSpecificDataPtr[i+2]==0x03)
        {
            spsData[j++] = 0x00;
            spsData[j++] = 0x00;
            i += 3;
            continue;
        }
        spsData[j++] = pSpecificDataPtr[i++];
    }
    ret = probeH264SizeInfo(pVideoInfo, spsData,j);
    //CDX_LOGD("here4: nDataLen=%d, nIndex[0]=%d, nIndex[2]=%d\n", nIndex[0], nIndex[2]);

    if(ret != PROBE_SPECIFIC_DATA_SUCCESS)
    {
        return ret;
    }
    pVideoInfo->pCodecSpecificData = malloc(nIndex[2]-nIndex[0]);
    if(pVideoInfo->pCodecSpecificData != NULL)
    {
        //������������nal
        memcpy(pVideoInfo->pCodecSpecificData, pSpecificDataPtr+nIndex[0]-4, nIndex[2]-nIndex[0]);
        pVideoInfo->nCodecSpecificDataLen = nIndex[2]-nIndex[0];
    }
    else
    {
        ret = PROBE_SPECIFIC_DATA_ERROR;
        CDX_LOGE("malloc fail.");
    }
    return ret;
}
//************************************************************************************************//
//************************************************************************************************//

cdx_int32 probeMpeg2SpecificData(VideoStreamInfo* pVideoInfo,
                                 cdx_uint8* pDataBuf, cdx_uint32 nDataLen)
{
    cdx_uint8* ppDataPtr = NULL;
    cdx_uint8* pPtr = NULL;
    cdx_int32 nnDatalen = 0;
    cdx_int32 nStartCodeIndex = 0;
    cdx_uint32 i = 0;
    cdx_uint32 nIndex[3]={0};
    cdx_int32 width = 0;
    cdx_int32 height = 0;
    cdx_uint32 frameRate = 0;
    cdx_uint32 aMpgFrmRate[16] = {00000, 23976, 24000, 25000,
                          29970, 30000, 50000, 59940,
                          60000, 00000, 00000, 00000,
                          00000, 00000, 00000, 00000};

    ppDataPtr = pDataBuf;
    nnDatalen = (int)nDataLen;

    while(1)
    {
        //CDX_LOGD("ppDataPtr(%p), nnDatalen(%u)", ppDataPtr, nnDatalen);
        //CDX_BUF_DUMP(ppDataPtr, nnDatalen);
        nStartCodeIndex = probeStartCode(ppDataPtr,nnDatalen, 0xffffff);
        if(nStartCodeIndex == -1)
        {
            //CDX_LOGD("return here2: i=%d\n", i);
            return (i==0)? PROBE_SPECIFIC_DATA_NONE : PROBE_SPECIFIC_DATA_UNCOMPELETE;
        }

        //CDX_LOGD("nStartCodeIndex=%d\n", nStartCodeIndex);

        //CDX_BUF_DUMP((ppDataPtr+nStartCodeIndex), (nnDatalen-nStartCodeIndex));

        //CDX_LOGD("here111:i=%d, %x %x %x\n", i, ppDataPtr[nStartCodeIndex],
        //ppDataPtr[nStartCodeIndex+1], ppDataPtr[nStartCodeIndex+2]);

        if((i==0) && (ppDataPtr[nStartCodeIndex+1]==0xB3))
        {
            nIndex[i++] = (ppDataPtr-pDataBuf)+nStartCodeIndex+2;
        }
        else if((i==1) && (ppDataPtr[nStartCodeIndex+1]==0xB8))
        {
            nIndex[i++] = (ppDataPtr-pDataBuf)+nStartCodeIndex+2;
        }
        else if(i==2)
        {
            nIndex[i++] = (ppDataPtr-pDataBuf)+nStartCodeIndex+2;
            break;
        }
        ppDataPtr += nStartCodeIndex+2;
        nnDatalen -= nStartCodeIndex+2;
        if(nnDatalen <= 0)
        {
            CDX_LOGD("*****");
            return (i==0)? PROBE_SPECIFIC_DATA_NONE : PROBE_SPECIFIC_DATA_UNCOMPELETE;
        }
    }
    //CDX_BUF_DUMP((pDataBuf+nIndex[0]), (nDataLen-nIndex[0]));
    pPtr = pDataBuf+nIndex[0];
    width  = (pPtr[0]<<4) | (pPtr[1]>>4);
    height = ((pPtr[1]&0x0f)<<8) | (pPtr[2]);
    frameRate = aMpgFrmRate[pPtr[3]&0x0f];

    if(pVideoInfo->nWidth == 0)
    {
        pVideoInfo->nWidth = width;
    }
    if(pVideoInfo->nHeight == 0)
    {
        pVideoInfo->nHeight = height;
    }
    if(pVideoInfo->nFrameRate == 0)
    {
        pVideoInfo->nFrameRate = frameRate;
    }
    if(pVideoInfo->nFrameRate)
    {
        pVideoInfo->nFrameDuration = 1000*1000*1000LL / pVideoInfo->nFrameRate;
    }
    pVideoInfo->pCodecSpecificData = malloc(nIndex[2]-nIndex[0]);
    if(pVideoInfo->pCodecSpecificData == NULL)
    {
        CDX_LOGE("malloc fail.");
        return PROBE_SPECIFIC_DATA_ERROR;
    }
    memcpy(pVideoInfo->pCodecSpecificData, pDataBuf+nIndex[0]-4, nIndex[2]-nIndex[0]);
    return PROBE_SPECIFIC_DATA_SUCCESS;
}

//***********************************************************************************************//
//***********************************************************************************************//

cdx_int32 decodeWmv3SequenceHeader(VideoStreamInfo* pVideoInfo,
                                   cdx_uint8* pDataBuf, cdx_uint32 nDataLen)
{
    GetBitContext sBitContext;
    cdx_uint32 profile = 0;
    cdx_uint16 width = 0;
    cdx_uint16 height = 0;
    cdx_uint32 frameRate = 0;
    cdx_uint8  resSprite = 0;
    cdx_uint8  fastuvmc = 0;
    cdx_uint8  extendedMv = 0;
    cdx_uint8  resTranstab = 0;
    cdx_uint8  level = 0;

    /** Available Profiles */
    //@{
    enum Profile {
        PROFILE_SIMPLE,
        PROFILE_MAIN,
        PROFILE_COMPLEX, ///< TODO: WMV9 specific
        PROFILE_ADVANCED
    };
    cdx_uint32 frameRateNumerator[] = {0, 24000, 25000, 30000, 60000, 48000, 72000};
    cdx_uint32 frameRateDenominator[] = {0, 1000, 1001};
    initGetBits(&sBitContext, pDataBuf, nDataLen*8);
    profile = getBits(&sBitContext, 2);

    if(profile == PROFILE_ADVANCED)
    {
        level = getBits(&sBitContext, 3);
        if(level >= 5)
        {
            CDX_LOGW("Reserved LEVEL %i",level);
        }
        getBits(&sBitContext, 2);
        getBits(&sBitContext, 3); //common
        getBits(&sBitContext, 5); //common
        getBits1(&sBitContext);   //common

        width       = (getBits(&sBitContext, 12) + 1) << 1;
        height      = (getBits(&sBitContext, 12) + 1) << 1;
        cdx_uint8 display_ext = getBits(&sBitContext, 7)&0x1;
        if(display_ext)
        {
            cdx_uint8 aspect_ratio_flag = getBits(&sBitContext, 29)&0x1;
            if(aspect_ratio_flag)
            {
                cdx_uint8 aspect_ratio = getBits(&sBitContext, 4);
                if(aspect_ratio == 15)
                {
                    getBits(&sBitContext, 16);
                }
            }
            cdx_uint8 framerate_flag = getBits1(&sBitContext);
            if(framerate_flag)
            {
                cdx_uint8 framerate_ind = getBits1(&sBitContext);
                if(framerate_ind == 0)
                {
                    cdx_uint8 framerateNR = getBits(&sBitContext, 8);
                    cdx_uint8 framerateDR = getBits(&sBitContext, 4);
                    if(framerateNR >= 7 || framerateNR == 0 ||
                       framerateDR == 0 || framerateDR >= 3)
                    {
                        CDX_LOGE("should not be here.");
                        goto _exit;
                    }
                    frameRate =
                            1000*frameRateNumerator[framerateNR]/frameRateDenominator[framerateDR];
                }
                else
                {
                    cdx_uint16 framerate_exp = getBits(&sBitContext, 16);
                    frameRate = 1000*(framerate_exp + 1)/32;
                }
            }
        }
    }
    else
    {
        CDX_LOGE("profile = %d, not PROFILE_ADVANCED",profile);
        getBits1(&sBitContext);
        resSprite = getBits1(&sBitContext);
        getBits(&sBitContext, 3); //common
        getBits(&sBitContext, 5); //common
        getBits1(&sBitContext); //common

        getBits1(&sBitContext); //reserved
        getBits1(&sBitContext);
        getBits1(&sBitContext);

        fastuvmc  = getBits1(&sBitContext); //common
        if(!profile && !fastuvmc)
        {
            CDX_LOGD("FASTUVMC unavailable in Simple Profile\n");
            return PROBE_SPECIFIC_DATA_ERROR;
        }
        extendedMv = getBits1(&sBitContext); //common
        if (!profile && extendedMv)
        {
            CDX_LOGD("Extended MVs unavailable in Simple Profile\n");
            return PROBE_SPECIFIC_DATA_ERROR;
        }
        getBits(&sBitContext, 2); //common
        getBits1(&sBitContext); //common
        resTranstab    = getBits1(&sBitContext);
        if(resTranstab)
        {
            CDX_LOGD("1 for reserved RES_TRANSTAB is forbidden\n");
            return PROBE_SPECIFIC_DATA_ERROR;
        }

        getBits1(&sBitContext); //common
        getBits1(&sBitContext);
        getBits1(&sBitContext);
        getBits(&sBitContext, 3); //common
        getBits(&sBitContext, 2); //common
        getBits1(&sBitContext); //common

        if(resSprite)
        {
            width = getBits(&sBitContext, 11);
            height = getBits(&sBitContext, 11);
            frameRate = getBits(&sBitContext, 5); //frame rate
        }
    }
_exit:
    if(!pVideoInfo->nWidth)
    {
        pVideoInfo->nWidth = width;
    }
    if(!pVideoInfo->nHeight)
    {
        pVideoInfo->nHeight = height;
    }
    if(!pVideoInfo->nFrameRate)
    {
        pVideoInfo->nFrameRate = frameRate;
    }
    if(pVideoInfo->nFrameRate)
    {
        pVideoInfo->nFrameDuration = 1000*1000*1000LL / pVideoInfo->nFrameRate;
    }
    return PROBE_SPECIFIC_DATA_SUCCESS;
}
cdx_int32 probeWmv3SpecificData(VideoStreamInfo* pVideoInfo,
                                cdx_uint8* pDataBuf, cdx_uint32 nDataLen)
{
    cdx_uint8* ppDataPtr = NULL;
    cdx_uint8* pPtr = NULL;
    cdx_int32 nnDatalen = 0;
    cdx_int32 nStartCodeIndex = 0;
    cdx_uint32  i = 0;
    cdx_uint32 nIndex[3]={0};
    cdx_int32 ret = 0;

    ppDataPtr = pDataBuf;
    nnDatalen = (int)nDataLen;

    while(1)
    {
        nStartCodeIndex = probeStartCode(ppDataPtr,nnDatalen, 0xffffff);
        if(nStartCodeIndex == -1)
        {
            return (i==0)? PROBE_SPECIFIC_DATA_NONE : PROBE_SPECIFIC_DATA_UNCOMPELETE;
        }

        if((i==0) && (ppDataPtr[nStartCodeIndex+1]==0x0F))
        {
            nIndex[i++] = (ppDataPtr-pDataBuf)+nStartCodeIndex+2;
        }
        else if((i==1) && (ppDataPtr[nStartCodeIndex+1]==0x0E))    //VC1_CODE_ENTRYPOINT
        {
            nIndex[i++] = (ppDataPtr-pDataBuf)+nStartCodeIndex+2;
        }
        else if(i==2)
        {
            nIndex[i++] = (ppDataPtr-pDataBuf)+nStartCodeIndex+2;
            break;
        }
        ppDataPtr += nStartCodeIndex+2;
        nnDatalen -= nStartCodeIndex+2;

        if(nnDatalen <= 0)
        {
            CDX_LOGD("*****");
            return (i==0)? PROBE_SPECIFIC_DATA_NONE : PROBE_SPECIFIC_DATA_UNCOMPELETE;
        }
    }

    pPtr = pDataBuf+nIndex[0];
    ret = decodeWmv3SequenceHeader(pVideoInfo, pPtr, nIndex[1]-nIndex[0]);
    if(ret != PROBE_SPECIFIC_DATA_SUCCESS)
    {
        return ret;
    }

    pVideoInfo->pCodecSpecificData = malloc(nIndex[2]-nIndex[0]);
    if(pVideoInfo->pCodecSpecificData == NULL)
    {
        CDX_LOGE("malloc fail.");
        return PROBE_SPECIFIC_DATA_ERROR;
    }
    memcpy(pVideoInfo->pCodecSpecificData, pDataBuf+nIndex[0]-4, nIndex[2]-nIndex[0]);
    return PROBE_SPECIFIC_DATA_SUCCESS;
}
//**************************************************************************************//
//**************************************************************************************//
static unsigned int h265_read_golomb(unsigned char* buffer, unsigned int* init)
{
    unsigned int x, v = 0, v2 = 0, m, len = 0, n = *init;

    while(getbits(buffer, n++, 1) == 0)
        len++;

    x = len + n;

    while(n < x)
    {
        m = min(x - n, 8); /* getbits() function, number of bits should not greater than 8 */
        v |= getbits(buffer, n, m);
        n += m;
        if(x - n > 8)
            v <<= 8;
        else if((x - n) > 0)
            v <<= (x - n);
    }

    v2 = 1;
    for(n = 0; n < len; n++)
        v2 <<= 1;
    v2 = (v2 - 1) + v;

    *init = x;
    return v2;
}

static int h265_parse_sps_profile_tier_level(unsigned char* buf,
                                unsigned int *len, unsigned int sps_max_sub_layers_minus1)
{
    unsigned int n = *len;
    unsigned char sub_layer_profile_present_flag[64] = { 0 };
    unsigned char sub_layer_level_present_flag[64] = { 0 };
    unsigned int i, j;

    n += 2; /* general_profile_space: u(2) */
    n += 1; /* general_tier_flag: u(1) */
    n += 5; /* general_profile_idc: u(5) */
    for(j = 0; j < 32; j++)
        n += 1; /* general_profile_compatibility_flag: u(1) */
    n += 1; /* general_progressive_source_flag: u(1) */
    n += 1; /* general_interlaced_source_flag: u(1) */
    n += 1; /* general_non_packed_constraint_flag: u(1) */
    n += 1; /* general_frame_only_constraint_flag: u(1) */
    n += 44; /* general_reserved_zero_44bits: u(44) */
    n += 8; /* general_level_idc: u(8) */

    for(i = 0; i < sps_max_sub_layers_minus1; i++)
    {
        sub_layer_profile_present_flag[i] = getbits(buf, n++, 1); /* u(1) */
        sub_layer_level_present_flag[i] = getbits(buf, n++, 1); /* u(1) */
    }
    if(sps_max_sub_layers_minus1 > 0)
        for(i = sps_max_sub_layers_minus1; i < 8; i++)
            n += 2; /* reserved_zero_2bits: u(2) */
    for(i = 0; i < sps_max_sub_layers_minus1; i++)
    {
        if(sub_layer_profile_present_flag[i])
        {
            n += 2; /* sub_layer_profile_space[i]: u(2) */
            n += 1; /* sub_layer_tier_flag[i]: u(1) */
            n += 5; /* sub_layer_profile_idc[i]: u(5) */
            for(j = 0; j < 32; j++)
                n += 1; /* sub_layer_profile_compatibility_flag[i][j]: u(1) */
            n += 1; /* sub_layer_progressive_source_flag[i]: u(1) */
            n += 1; /* sub_layer_interlaced_source_flag[i]: u(1) */
            n += 1; /* sub_layer_non_packed_constraint_flag[i]: u(1) */
            n += 1; /* sub_layer_frame_only_constraint_flag[i]: u(1) */
            n += 44; /* sub_layer_reserved_zero_44bits[i]: u(44) */
        }
        if(sub_layer_level_present_flag[i])
            n += 8; /* sub_layer_level_idc[i]: u(8)*/
    }

    (*len) = n;
    return 0;
}

static int h265_parse_sps(VideoStreamInfo* pVideoInfo, unsigned char* buf, int len)
{
    (void)len;
    unsigned int n = 0;
    unsigned int sps_max_sub_layers_minus1 = 0;
    unsigned int chroma_format_idc = 0;
    unsigned int flag;
    unsigned int sps_max_sub_layers, start, i;
    unsigned int nSpsMaxDecPicBuffering[16], ref_pictures;
    //unsigned int pic_width = 0;
    //unsigned int pic_height = 0;
    //CDX_BUF_DUMP(buf, len);
    //n += 1;  /* forbidden_zero_bit: u(1)*/
    //n += 6; /* nal_unit_type: u(6) */
    //n += 6; /* nuh_layer_id: u(6) */
    //n += 3; /* nuh_temporal_id_plus1: u(3) */

    n += 4;  /* sps_video_parameter_set_id: u(4) */
    sps_max_sub_layers_minus1 = getbits(buf, n, 3);
    sps_max_sub_layers = sps_max_sub_layers_minus1 + 1;
    n += 3;  /* sps_max_sub_layers_minus1:  u(3) */
    n += 1;  /* sps_temporal_id_nesting_flag: u(1) */
    h265_parse_sps_profile_tier_level(buf, &n, sps_max_sub_layers_minus1);
    h265_read_golomb(buf, &n); /* sps_seq_parameter_set_id: ue(v) */
    chroma_format_idc = h265_read_golomb(buf, &n); /* chroma_format_idc: ue(v) */
    if(chroma_format_idc == 3)
        n += 1; /* separate_colour_plane_flag: u(1) */

    pVideoInfo->nWidth = h265_read_golomb(buf, &n); /* pic_width_in_luma_samples: ue(v) */
    pVideoInfo->nHeight = h265_read_golomb(buf, &n); /* pic_height_in_luma_samples: ue(v) */
    flag = getbits(buf, n, 1);  /* conformance_window_flag: u(1) */
    n += 1;
    if(flag)
    {
        h265_read_golomb(buf, &n);  /*conf_win_left_offset: ue(v)*/
        h265_read_golomb(buf, &n);  /*conf_win_right_offset: ue(v)*/
        h265_read_golomb(buf, &n);  /*conf_win_top_offset: ue(v)*/
        h265_read_golomb(buf, &n);  /*conf_win_bottom_offset: ue(v)*/
    }
    h265_read_golomb(buf, &n);  /*bit_depth_luma_minus8: ue(v)*/
    h265_read_golomb(buf, &n);  /*bit_depth_chroma_minus8: ue(v)*/
    h265_read_golomb(buf, &n);  /*log2_max_pic_order_cnt_lsb_minus4: ue(v)*/
    flag = getbits(buf, n, 1);  /* sps_sub_layer_ordering_info_present_flag: u(1) */
    n += 1;
    for(i = 0; i < 16; i++)
        nSpsMaxDecPicBuffering[i] = 1;
    start = flag ? 0 : sps_max_sub_layers - 1;
    for(i = start ; i <= sps_max_sub_layers - 1; i++)
    {
        /*sps_max_dec_pic_buffering_minus1[i]: ue(v)*/
        nSpsMaxDecPicBuffering[i] = h265_read_golomb(buf, &n) + 1;
        loge("nSpsMaxDecPicBuffering[i]: %d", nSpsMaxDecPicBuffering[i]);
        h265_read_golomb(buf, &n);  /*sps_max_num_reorder_pics[i]: ue(v)*/
        h265_read_golomb(buf, &n);  /*sps_max_latency_increase_plus1[i]: ue(v)*/
    }
    ref_pictures = 1;
    for(i = 0; i < 16; i++)
    {
        if(ref_pictures < nSpsMaxDecPicBuffering[i])
            ref_pictures = nSpsMaxDecPicBuffering[i];
    }
    pVideoInfo->h265ReferencePictureNum = ref_pictures;
    CDX_LOGV("zwh h265 parser prob pic width: %d, pic height: %d, reference pictures: %d",
            pVideoInfo->nWidth, pVideoInfo->nHeight, pVideoInfo->h265ReferencePictureNum);
    return PROBE_SPECIFIC_DATA_SUCCESS;
}

static int HevcParseExtraDataSearchNaluSize(cdx_uint8 *pData, int nSize)
{
    int i, nTemp, nNaluLen;
    int mask = 0xffffff;
    char *p;
    nTemp = -1;
    nNaluLen = -1;
    for(i = 0; i < nSize; i++)
    {
        nTemp = (nTemp << 8) | pData[i];
        if((nTemp & mask) == 0x000001)
        {
            nNaluLen = i - 3;
            break;
        }
    }
    return nNaluLen;
}

static int HevcParseExtraDataDeleteEmulationCode(cdx_uint8 *pBuf, int nSize)
{
    int i, nOutputSize;
    int nSkipped = 0;
    int nTemp = -1;
    const unsigned int mask = 0xFFFFFF;
    for(i = 0; i < nSize; i++)
    {
        nTemp = (nTemp << 8) | pBuf[i];
        switch(nTemp & mask)
        {
        case 0x000003:
            nSkipped += 1;
            break;
        default:
            if(nSkipped > 0)
                pBuf[i - nSkipped] = pBuf[i];
        }
    }
    return     (i - nSkipped);
}

cdx_int32 probeH265RefPictureNumber(cdx_uint8* pDataBuf, cdx_uint32 nDataLen)
{
    VideoStreamInfo VideoInfo;
    VideoStreamInfo *pVideoInfo;
    int nDataTrunkLen, nTemp, nRet, bBufCallocFlag;
    cdx_uint32 i;
    int naluType;
    cdx_uint8 *pBuf;

    pVideoInfo = &VideoInfo;
    memset(pVideoInfo, 0, sizeof(VideoInfo));
    pBuf = malloc(2*1024);
    if(pBuf == NULL)
    {
        loge(" probeH265RefPictureNumber( ) malloc error ");
        return 0;
    }
#if 0
    if(1)
    {
        cdx_uint8 *t = pDataBuf;
        for(i = 0; i < 10; i++)
        {
            logd(" %2x, %2x, %2x, %2x, %2x, %2x, %2x, %2x",
                  t[0],t[1],t[2],t[3],t[4],t[5],t[6],t[7]);
            t += 8;
        }
    }
#endif

    nTemp = -1;
    for(i = 0; i < nDataLen; )
    {
        nTemp <<= 8;
        nTemp |= pDataBuf[i];
        switch(nTemp & 0xffffff)
        {
        case 0xa00001:
        case 0xa10001:
        case 0xa20001:

        case 0x200001:
        case 0x210001:
        case 0x220001:

        case 0x600001:
        case 0x610001:
        case 0x620001:
            /* a data trunk */
            logv(" --------  mov mkv case -----");
            nDataTrunkLen = (pDataBuf[i + 1] << 8) | pDataBuf[i + 2];
            i += 3;
            naluType = (pDataBuf[i] >> 1) & 0x3f;

            if(naluType == 33)
            {
                memcpy(pBuf, &pDataBuf[i], nDataTrunkLen);
                nDataTrunkLen = HevcParseExtraDataDeleteEmulationCode(pBuf, nDataTrunkLen);
                h265_parse_sps(pVideoInfo, pBuf, nDataTrunkLen);
            }
            i += nDataTrunkLen;
            break;
        case 0x000001:
            /* ts container's extra data starts with 0x000001 */
            logv(" --------  ts case");
            i += 1;
            nDataTrunkLen = HevcParseExtraDataSearchNaluSize(&pDataBuf[i], nDataLen - i);
            if(nDataTrunkLen < 0)
                nDataTrunkLen = nDataLen - i;
            naluType = (pDataBuf[i] >> 1) & 0x3f;

            if(naluType == 33)
            {
                memcpy(pBuf, &pDataBuf[i], nDataTrunkLen);
                nDataTrunkLen = HevcParseExtraDataDeleteEmulationCode(pBuf, nDataTrunkLen);
                h265_parse_sps(pVideoInfo, pBuf, nDataTrunkLen);
            }
            i += nDataTrunkLen;
            break;
        default:
            logv(" searching next");
             i++;
            break;
        }
    }
    free(pBuf);
    return pVideoInfo->h265ReferencePictureNum;
}

cdx_int32 probeH265SpecificData(VideoStreamInfo* pVideoInfo,
                                cdx_uint8* pDataBuf, cdx_uint32 nDataLen)
{
    cdx_int32 nStartCodeIndex;
    cdx_uint32 i = 0;
    cdx_int32 j = 0;
    cdx_int32 ret = 0;
    cdx_uint32 nIndex[4] = {0};
    cdx_uint8 spsData[2048];
    cdx_uint8* pSpecificDataPtr = NULL;
    cdx_uint8* ppDataPtr = NULL;

    cdx_uint32 nSpecificDataLen=0;
    cdx_int32 nnDatalen = 0;

    pSpecificDataPtr = pDataBuf;
    nSpecificDataLen = nDataLen;
    ppDataPtr = pSpecificDataPtr;
    nnDatalen = nSpecificDataLen;

    while(1)
    {
        nStartCodeIndex = probeStartCode(ppDataPtr,nnDatalen, 0xffffffff);
        if(nStartCodeIndex == -1)
        {
            if(i==0)
            {
                return PROBE_SPECIFIC_DATA_NONE;
            }
            else
            {
                return PROBE_SPECIFIC_DATA_UNCOMPELETE;
            }
        }

        if((i==0) && (ppDataPtr[nStartCodeIndex+1]>>1&0x3f) == 32)
        {
            nIndex[i++] = (ppDataPtr-pSpecificDataPtr)+nStartCodeIndex+3;
            //CDX_LOGD("here22: pSpecificDataPtr[nIndex[0]-1]=%x,
            //pSpecificDataPtr[nIndex[0]]=%x\n", pSpecificDataPtr[nIndex[0]-1],
            //pSpecificDataPtr[nIndex[0]]);
        }
        else if((i==1) && (ppDataPtr[nStartCodeIndex+1]>>1&0x3f) == 33)
        {
            nIndex[i++] = (ppDataPtr-pSpecificDataPtr)+nStartCodeIndex+3;
            //CDX_LOGD("here33: pSpecificDataPtr[nIndex[1]-1]=%x,
            //pSpecificDataPtr[nIndex[1]]=%x\n", pSpecificDataPtr[nIndex[1]-1],
            //pSpecificDataPtr[nIndex[1]]);
        }
        else if(i==2 && (ppDataPtr[nStartCodeIndex+1]>>1&0x3f) == 34)
        {
            nIndex[i++] = (ppDataPtr-pSpecificDataPtr)+nStartCodeIndex+3;
            //CDX_LOGD("here44: pSpecificDataPtr[nIndex[2]-1]=%x,
            //pSpecificDataPtr[nIndex[2]]=%x\n", pSpecificDataPtr[nIndex[2]-1],
            //pSpecificDataPtr[nIndex[2]]);
        }
        else if(i == 3)
        {
            nIndex[i++] = (ppDataPtr-pSpecificDataPtr)+nStartCodeIndex+3;
        }
        ppDataPtr += nStartCodeIndex+3;
        nnDatalen -= nStartCodeIndex+3;
        if(i==4)
        {
            break;
        }
        if(nnDatalen <= 0)
        {
            CDX_LOGD("*****");
            return (i==0)? PROBE_SPECIFIC_DATA_NONE : PROBE_SPECIFIC_DATA_UNCOMPELETE;
        }
    }

    //CDX_BUF_DUMP(pSpecificDataPtr+nIndex[1] - 2, nIndex[2]-nIndex[1]);

    // skip the 0x03 data
    for(i=nIndex[1] - 2; i<(nIndex[2]-5);)
    {
        if(pSpecificDataPtr[i]==0x00 && pSpecificDataPtr[i+1]==0x00 && pSpecificDataPtr[i+2]==0x03)
        {
            spsData[j++] = 0x00;
            spsData[j++] = 0x00;
            i += 3;
            continue;
        }
        spsData[j++] = pSpecificDataPtr[i++];
    }
    ret = h265_parse_sps(pVideoInfo, spsData,j);
    //CDX_LOGD("here4: nDataLen=%d, nIndex[0]=%d, nIndex[2]=%d\n", nIndex[0], nIndex[2]);

    if(ret != PROBE_SPECIFIC_DATA_SUCCESS)
    {
        return ret;
    }
    pVideoInfo->pCodecSpecificData = malloc(nIndex[3]-nIndex[0]);
    if(pVideoInfo->pCodecSpecificData != NULL)
    {
        //不包含第三个nal
        memcpy(pVideoInfo->pCodecSpecificData, pSpecificDataPtr+nIndex[0]-5, nIndex[3]-nIndex[0]);
        pVideoInfo->nCodecSpecificDataLen = nIndex[3]-nIndex[0];
    }
    else
    {
        ret = PROBE_SPECIFIC_DATA_ERROR;
        CDX_LOGE("malloc fail.");
    }
    return ret;
}
static int getNextChunkSize(const cdx_uint8 *data, cdx_uint32 size)
{
    static const char kStartCode[] = "\x00\x00\x01";

    if (size < 3 || memcmp(kStartCode, data, 3)) {
        CDX_LOGE("should not be here.");
        return -1;
    }

    cdx_uint32 offset = 3;
    while (offset + 2 < size) {
        if (!memcmp(&data[offset], kStartCode, 3)) {
            return offset;
        }

        ++offset;
    }

    return -1;
}
cdx_bool ExtractDimensionsFromVOLHeader(
        const cdx_uint8 *data, cdx_uint32 size, int32_t *width, int32_t *height) {
    CdxBitReaderT *br = CdxBitReaderCreate(&data[4], size - 4);
    CdxBitReaderGetBits(br, 1);  // random_accessible_vol
    unsigned video_object_type_indication = CdxBitReaderGetBits(br, 8);

    CDX_CHECK(video_object_type_indication != 0x21u /* Fine Granularity Scalable */);

    unsigned video_object_layer_verid;
    unsigned video_object_layer_priority;
    if (CdxBitReaderGetBits(br, 1)) {
        video_object_layer_verid = CdxBitReaderGetBits(br, 4);
        video_object_layer_priority = CdxBitReaderGetBits(br, 3);
    }
    unsigned aspect_ratio_info = CdxBitReaderGetBits(br, 4);
    if (aspect_ratio_info == 0x0f /* extended PAR */) {
        CdxBitReaderGetBits(br, 8);  // par_width
        CdxBitReaderGetBits(br, 8);  // par_height
    }
    if (CdxBitReaderGetBits(br, 1)) {  // vol_control_parameters
        CdxBitReaderGetBits(br, 2);  // chroma_format
        CdxBitReaderGetBits(br, 1);  // low_delay
        if (CdxBitReaderGetBits(br, 1)) {  // vbv_parameters
            CdxBitReaderGetBits(br, 15);  // first_half_bit_rate
            CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit
            CdxBitReaderGetBits(br, 15);  // latter_half_bit_rate
            CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit
            CdxBitReaderGetBits(br, 15);  // first_half_vbv_buffer_size
            CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit
            CdxBitReaderGetBits(br, 3);  // latter_half_vbv_buffer_size
            CdxBitReaderGetBits(br, 11);  // first_half_vbv_occupancy
            CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit
            CdxBitReaderGetBits(br, 15);  // latter_half_vbv_occupancy
            CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit
        }
    }
    unsigned video_object_layer_shape = CdxBitReaderGetBits(br, 2);
    CDX_CHECK(video_object_layer_shape == 0x00u /* rectangular */);

    CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit
    unsigned vop_time_increment_resolution = CdxBitReaderGetBits(br, 16);
    CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit

    if (CdxBitReaderGetBits(br, 1)) {  // fixed_vop_rate
        // range [0..vop_time_increment_resolution)

        // vop_time_increment_resolution
        // 2 => 0..1, 1 bit
        // 3 => 0..2, 2 bits
        // 4 => 0..3, 2 bits
        // 5 => 0..4, 3 bits
        // ...

        CDX_CHECK(vop_time_increment_resolution > 0u);
        --vop_time_increment_resolution;

        unsigned numBits = 0;
        while (vop_time_increment_resolution > 0) {
            ++numBits;
            vop_time_increment_resolution >>= 1;
        }

        CdxBitReaderSkipBits(br, numBits);  // fixed_vop_time_increment
    }

    CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit
    unsigned video_object_layer_width = CdxBitReaderGetBits(br, 13);
    CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit
    unsigned video_object_layer_height = CdxBitReaderGetBits(br, 13);
    CDX_CHECK(CdxBitReaderGetBits(br, 1));  // marker_bit

    //unsigned interlaced =
        CdxBitReaderGetBits(br, 2);

    *width = video_object_layer_width;
    *height = video_object_layer_height;
    CdxBitReaderDestroy(br);

    return CDX_TRUE;
}
cdx_int32 probeMpeg4SpecificData(VideoStreamInfo* pVideoInfo, cdx_uint8* pDataBuf,
                                 cdx_uint32 nDataLen, enum CdxParserTypeE type)
{
    //CDX_BUF_DUMP(pDataBuf, nDataLen);
    cdx_int32 nStartCodeIndex = probeStartCode(pDataBuf, nDataLen, 0xffffffff);
    if(nStartCodeIndex < 0)
    {
        CDX_LOGE("PROBE_SPECIFIC_DATA_NONE");
        return PROBE_SPECIFIC_DATA_NONE;
    }
    cdx_uint8 *data = pDataBuf + nStartCodeIndex - 2;
    cdx_uint32 size = nDataLen - (nStartCodeIndex - 2);
    if(type != CDX_PARSER_TS)
    {
        cdx_uint32 i = 0;
        cdx_uint32 startCode = 0xffffffff;

        while(i < size)
        {
            startCode <<= 8;
            startCode += data[i];
            if((startCode&0xffffffff) == 0x01b6)
            {
                break;
            }
            i++;
        }
        if(i >= size)
        {
            return PROBE_SPECIFIC_DATA_NONE;
        }
        size = i - 3;

        pVideoInfo->pCodecSpecificData = malloc(size);
        if(!pVideoInfo->pCodecSpecificData)
        {
            CDX_LOGE("malloc fail.");
            return PROBE_SPECIFIC_DATA_ERROR;
        }
        pVideoInfo->nCodecSpecificDataLen = size;
        memcpy(pVideoInfo->pCodecSpecificData, data, size);
        //CDX_BUF_DUMP(pVideoInfo->pCodecSpecificData, size);
        return PROBE_SPECIFIC_DATA_SUCCESS;
    }

    enum {
        SKIP_TO_VISUAL_OBJECT_SEQ_START,
        EXPECT_VISUAL_OBJECT_START,
        EXPECT_VO_START,
        EXPECT_VOL_START,
        WAIT_FOR_VOP_START,
        SKIP_TO_VOP_START,

    } state;

    state = SKIP_TO_VISUAL_OBJECT_SEQ_START;

    int32_t width = 0, height = 0;

    cdx_uint32 offset = 0;
    int chunkSize;
    while ((chunkSize = getNextChunkSize(
                    &data[offset], size - offset)) > 0) {

        unsigned chunkType = data[offset + 3];

        switch (state) {
            case SKIP_TO_VISUAL_OBJECT_SEQ_START:
            {
                if (chunkType == 0xb0) {
                    // Discard anything before this marker.
                    state = EXPECT_VISUAL_OBJECT_START;
                }
                break;
            }

            case EXPECT_VISUAL_OBJECT_START:
            {
                CDX_CHECK(chunkType == 0xb5);
                state = EXPECT_VO_START;
                break;
            }

            case EXPECT_VO_START:
            {
                CDX_CHECK(chunkType <= 0x1f);
                state = EXPECT_VOL_START;
                break;
            }

            case EXPECT_VOL_START:
            {
                CDX_CHECK((chunkType & 0xf0) == 0x20);

                CDX_CHECK(ExtractDimensionsFromVOLHeader(
                            &data[offset], chunkSize,
                            &width, &height));

                state = WAIT_FOR_VOP_START;
                break;
            }

            case WAIT_FOR_VOP_START:
            {
                if (chunkType == 0xb3 || chunkType == 0xb6) {
                    // group of VOP or VOP start.

                    CDX_LOGD("found MPEG4 video codec config (%d x %d)",
                         width, height);
                    pVideoInfo->pCodecSpecificData = malloc(offset);
                    if(!pVideoInfo->pCodecSpecificData)
                    {
                        CDX_LOGE("malloc fail.");
                        return PROBE_SPECIFIC_DATA_ERROR;
                    }
                    pVideoInfo->nCodecSpecificDataLen = offset;
                    memcpy(pVideoInfo->pCodecSpecificData, data, offset);
                    if(!pVideoInfo->nWidth)
                    {
                        pVideoInfo->nWidth = width;
                    }
                    if(!pVideoInfo->nHeight)
                    {
                        pVideoInfo->nHeight = height;
                    }
                    // hexdump(csd->data(), csd->size());

                    //state = SKIP_TO_VOP_START;
                    return PROBE_SPECIFIC_DATA_SUCCESS;
                }

                break;
            }

            default:
            {
                CDX_LOGE("should not be here, state=%d", state);
                return PROBE_SPECIFIC_DATA_ERROR;
            }
        }

        offset += chunkSize;
    }
    if(state >= EXPECT_VISUAL_OBJECT_START)
    {
        return PROBE_SPECIFIC_DATA_UNCOMPELETE;
    }
    else
    {
        CDX_LOGD("PROBE_SPECIFIC_DATA_NONE");
        return PROBE_SPECIFIC_DATA_NONE;
    }
}
cdx_int32 probeAvsSpecificData(VideoStreamInfo* pVideoInfo,
                               cdx_uint8* pDataBuf, cdx_uint32 nDataLen)
{
    cdx_int32 AvsFrmRate[16] = {00000, 23976, 24000, 25000,
                          29970, 30000, 50000, 59940,
                          60000, 00000, 00000, 00000,
                          00000, 00000, 00000, 00000};

    cdx_uint32 i = 0;
    cdx_uint32 startCode = 0xffffffff;

    while(i < nDataLen)
    {
        startCode <<= 8;
        startCode += pDataBuf[i];
        if((startCode&0xffffffff) == 0x01b0)
        {
            break;
        }
        i++;
    }
    if(i >= nDataLen)
    {
        return PROBE_SPECIFIC_DATA_NONE;
    }
    cdx_uint8 *data = pDataBuf + i - 3;
    cdx_uint32 size = nDataLen - (i - 3);
    if(size < 18)
    {
        return PROBE_SPECIFIC_DATA_UNCOMPELETE;
    }
    CdxBitReaderT *br = CdxBitReaderCreate(data, 18);
    CdxBitReaderSkipBits(br, 49);
    cdx_uint32 width = CdxBitReaderGetBits(br, 14);
    cdx_uint32 height = CdxBitReaderGetBits(br, 14);
    CdxBitReaderGetBits(br, 9);
    cdx_int32 frameRate = AvsFrmRate[CdxBitReaderGetBits(br, 4)];
    //cdx_uint32 bit_rate_lower = CdxBitReaderGetBits(br, 18);
    //CdxBitReaderGetBits(br, 1);
    //cdx_uint32 bit_rate_upper = CdxBitReaderGetBits(br, 12);
    CdxBitReaderDestroy(br);
    pVideoInfo->pCodecSpecificData = malloc(18);
    if(!pVideoInfo->pCodecSpecificData)
    {
        CDX_LOGE("malloc fail.");
        return PROBE_SPECIFIC_DATA_ERROR;
    }
    pVideoInfo->nCodecSpecificDataLen = 18;
    memcpy(pVideoInfo->pCodecSpecificData, data, 18);
    if(!pVideoInfo->nWidth)
    {
        pVideoInfo->nWidth = width;
    }
    if(!pVideoInfo->nHeight)
    {
        pVideoInfo->nHeight = height;
    }
    if(!pVideoInfo->nFrameRate)
    {
        pVideoInfo->nFrameRate = frameRate;
    }
    if(pVideoInfo->nFrameRate)
    {
        pVideoInfo->nFrameDuration = 1000*1000*1000LL / pVideoInfo->nFrameRate;
    }
    return PROBE_SPECIFIC_DATA_SUCCESS;

}
cdx_int32 ProbeVideoSpecificData(VideoStreamInfo* pVideoInfo, cdx_uint8* pDataBuf,
                        cdx_uint32 dataLen, cdx_uint32 eVideoCodecFormat, enum CdxParserTypeE type)
{
    cdx_int32 ret = PROBE_SPECIFIC_DATA_NONE;
    if((dataLen==0) || (pDataBuf==NULL))
    {
        CDX_LOGE("dataLen=%u, pDataBuf=%p", dataLen, pDataBuf);
        return PROBE_SPECIFIC_DATA_ERROR;
    }

    if((eVideoCodecFormat>VIDEO_CODEC_FORMAT_MAX) || (eVideoCodecFormat<VIDEO_CODEC_FORMAT_MIN) ||
            (eVideoCodecFormat==VIDEO_CODEC_FORMAT_MPEG4))
    {
        CDX_LOGE("the eVideoCodecFormat(0x%x) is invalid", eVideoCodecFormat);
        return PROBE_SPECIFIC_DATA_ERROR;
    }

    switch(eVideoCodecFormat)
    {
        case VIDEO_CODEC_FORMAT_MPEG1:
        case VIDEO_CODEC_FORMAT_MPEG2:
        {
            ret = probeMpeg2SpecificData(pVideoInfo, pDataBuf, dataLen);
            break;
        }
        case VIDEO_CODEC_FORMAT_MSMPEG4V1:
        case VIDEO_CODEC_FORMAT_MSMPEG4V2:
        case VIDEO_CODEC_FORMAT_SORENSSON_H263:
        case VIDEO_CODEC_FORMAT_RXG2:
        case VIDEO_CODEC_FORMAT_DIVX3:
        case VIDEO_CODEC_FORMAT_DIVX4:
        case VIDEO_CODEC_FORMAT_DIVX5:
        case VIDEO_CODEC_FORMAT_XVID:
        case VIDEO_CODEC_FORMAT_H263:
        {
            ret = probeMpeg4SpecificData(pVideoInfo, pDataBuf, dataLen, type);
            break;
        }
        case VIDEO_CODEC_FORMAT_WMV3:
        {
            ret = probeWmv3SpecificData(pVideoInfo, pDataBuf, dataLen);
            break;
        }
        case VIDEO_CODEC_FORMAT_H264:
        {
            ret = probeH264SpecificData(pVideoInfo, pDataBuf, dataLen);
            break;
        }
        case VIDEO_CODEC_FORMAT_H265:
        {
            ret = probeH265SpecificData(pVideoInfo, pDataBuf, dataLen);
            break;
        }
        case VIDEO_CODEC_FORMAT_AVS:
        {
            ret = probeAvsSpecificData(pVideoInfo, pDataBuf, dataLen);
            break;
        }

        case VIDEO_CODEC_FORMAT_WMV1:
        case VIDEO_CODEC_FORMAT_WMV2:
        {
           /* the specific data of wmv2
                s->fps                = extra_data[0]>>3;
                s->bit_rate           = (extra_data[0]&7)<<16;
                s->bit_rate          |= extra_data[1]<<8;
                s->bit_rate          |= extra_data[2];
                s->bit_rate         <<= 10;
                s->mspel_bit          = (extra_data[2]>>7)&1;
                s->deblockingflag     = (extra_data[2]>>6)&1;
                s->abt_flag           = (extra_data[2]>>5)&1;
                s->j_type_bit         = (extra_data[2]>>4)&1;
                s->top_left_mv_flag   = (extra_data[2]>>3)&1;
                s->per_mb_rl_bit      = (extra_data[2]>>2)&1;
                code                  = (extra_data[2]&3)<<1;
                code                 |= (extra_data[3]>>7)&1;

                if(code==0)
                    return -1;
           */
        }
        default:
        {
            CDX_LOGE("the eVideoCodecFormat(0x%x) is not supported yet.", eVideoCodecFormat);
            return PROBE_SPECIFIC_DATA_ERROR;
        }
    }
    return ret;
}

