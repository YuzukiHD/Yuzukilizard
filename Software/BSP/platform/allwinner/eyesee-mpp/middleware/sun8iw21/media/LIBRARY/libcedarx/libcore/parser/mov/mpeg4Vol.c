/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : mpeg4Vol.c
 * Description :
 * History :
 *
 */

#include"mpeg4Vol.h"

#define RECTANGULAR             0
#define BINARY                  1
#define BINARY_SHAPE_ONLY       2
#define GRAY_SCALE              3

#define EXTENDED_PAR 0x000f

#define VO_START_CODE           0x8
#define VO_START_CODE_MIN       0x100
#define VO_START_CODE_MAX       0x11f

#define VOL_START_CODE          0x12
#define VOL_START_CODE_MIN      0x120
#define VOL_START_CODE_MAX      0x12f

typedef struct MP4_STREAM
{
    CDX_U32   length;
    CDX_U32   count;
    CDX_U8    *startptr;
    CDX_U8    *buf_start_ptr;
    CDX_U8    *buf_end_ptr;
    CDX_U8    *rdptr;

    CDX_U32   bitcnt;
    CDX_U32   bit_a, bit_b;
}MP4_STREAM;

#define _SWAP(a,b) (b=((a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3]))

static CDX_S32 log2ceil(CDX_S32 arg)
{
    CDX_S32 j=0;
    CDX_S32 i=1;
    if(arg==0)
        return 0;
    while (arg>i)
    {
        i*=2;
        j++;
    }
    return j;
}

/* initialize buffer, call once before first getbits or showbits */
static void initbits (MP4_STREAM * _ld, CDX_U8 *stream, CDX_S32 length,
    CDX_U8 *bufstartptr,CDX_U8 *bufendptr)
{
    CDX_S32 i;
    MP4_STREAM * ld = _ld;

    ld->bitcnt = 0;
    ld->count = 0;

    ld->startptr = ld->rdptr = stream;
    ld->length = length;
    ld->buf_end_ptr = bufendptr;
    ld->buf_start_ptr = bufstartptr;

    //check 32-bits aligned
    if(((uintptr_t)ld->rdptr)&3)
    {
        ld->bit_a = 0;

        if(ld->rdptr > ld->buf_end_ptr)
            ld->rdptr = ld->buf_start_ptr;    //loop back
        ld->bit_a <<= 8;
        ld->bit_a |= ld->rdptr[0];
        ld->rdptr ++;
        ld->count ++;
        ld->bitcnt = 24;

        if(((uintptr_t)ld->rdptr)&3)
        {
            if(ld->rdptr > ld->buf_end_ptr)
                ld->rdptr = ld->buf_start_ptr;    //loop back
            ld->bit_a <<= 8;
            ld->bit_a |= ld->rdptr[0];
            ld->rdptr ++;
            ld->count ++;
            ld->bitcnt = 16;

            if(((uintptr_t)ld->rdptr)&3)
            {
                if(ld->rdptr > ld->buf_end_ptr)
                    ld->rdptr = ld->buf_start_ptr;    //loop back
                ld->bit_a <<= 8;
                ld->bit_a |= ld->rdptr[0];
                ld->rdptr ++;
                ld->count ++;
                ld->bitcnt = 8;
            }
        }
    }
    else if(ld->rdptr+4 <= ld->buf_end_ptr)
    {
        _SWAP(ld->rdptr, ld->bit_a);
        ld->rdptr += 4;
        ld->count += 4;
    }
    else
    {//will loop back
        ld->bit_a = 0;
        for (i=0;i<4;i++)
        {
            if(ld->rdptr > ld->buf_end_ptr)
                ld->rdptr = ld->buf_start_ptr; //loop back
            ld->bit_a <<= 8;
            ld->bit_a |= ld->rdptr[0];
            ld->rdptr ++;
            ld->count ++;
        }
    }

    if(ld->rdptr+4 <= ld->buf_end_ptr)
    {
        _SWAP(ld->rdptr, ld->bit_b);
        ld->rdptr += 4;
        ld->count += 4;
    }
    else
    {
        ld->bit_b = 0;
        for (i=0;i<4;i++)
        {
            if(ld->rdptr > ld->buf_end_ptr)
                ld->rdptr = ld->buf_start_ptr; //loop back
            ld->bit_b <<= 8;
            ld->bit_b |= ld->rdptr[0];
            ld->rdptr ++;
            ld->count ++;
        }

    }
}

static CDX_S32 showbits1 (MP4_STREAM * ld)
{
    if(ld->bit_a & (0x80000000 >> ld->bitcnt))
        return 1;
    else
        return 0;
}

static void flushbits (MP4_STREAM * ld, CDX_S32 n)
{
    CDX_S32 i;

    ld->bitcnt += n;
    if (ld->bitcnt >= 32)
    {
        ld->bit_a = ld->bit_b;

        if(ld->rdptr+4 <= ld->buf_end_ptr)
        {
            _SWAP(ld->rdptr, ld->bit_b);
            ld->rdptr += 4;
            ld->count += 4;
        }
        else
        {
            ld->bit_b = 0;
            for (i=0;i<4;i++)
            {
                if(ld->rdptr > ld->buf_end_ptr)
                    ld->rdptr = ld->buf_start_ptr;    //loop back
                ld->bit_b <<= 8;
                ld->bit_b |= ld->rdptr[0];
                ld->rdptr ++;
                ld->count ++;
            }

        }
        ld->bitcnt -= 32;
    }
}

/* read n bits */
static CDX_U32 showbits (MP4_STREAM * ld, CDX_S32 n)
{
    CDX_S32 nbit = (n + ld->bitcnt) - 32;

    if (nbit > 0)
    {
        // The bits are on both ints
        return (((ld->bit_a & (0xFFFFFFFF >> (ld->bitcnt))) << nbit) |
            (ld->bit_b >> (32 - nbit)));

    }
    else
    {
        CDX_S32 rbit = 32 - ld->bitcnt;
        return (ld->bit_a & (0xFFFFFFFF >> (ld->bitcnt))) >> (rbit-n);
    }
}

static CDX_U32 getbits (MP4_STREAM * ld, CDX_S32 n)
{
    CDX_U32 l = showbits (ld, n);
    flushbits (ld, n);
    return l;
}

static CDX_U32 getbits1(MP4_STREAM * ld)
{
    CDX_U32 l = showbits1 (ld);
    flushbits (ld, 1);
    return l;
}

CDX_S32 mov_getvolhdr(CDX_U8 *start_addr,CDX_S32 length,CDX_S32 *width,CDX_S32 *height)
{
    MP4_STREAM _ld;
    MP4_STREAM * ld = &_ld;
    CDX_S32 code,i=0;

    initbits(ld, start_addr, length,start_addr,start_addr+length);

    code = showbits(ld, 32);
    if ((code >= VO_START_CODE_MIN) &&
        (code <= VO_START_CODE_MAX))
    {
        getbits(ld, 24);
        getbits(ld, 8);
    }
    while ((showbits(ld, 28)) != VOL_START_CODE && i<length)
    {
        getbits(ld, 8);
        i++;
    }

    code = showbits(ld, 28);
    if (code == VOL_START_CODE)
    {
        CDX_S32 is_object_layer_identifier,visual_object_layer_verid,aspect_ratio_info,
            vol_control_parameters,vbv_parameters,shape,time_increment_resolution,fixed_vop_rate;

        getbits(ld, 24);
        getbits(ld, 4);

        getbits(ld, 4); // vol_id
        getbits(ld, 1);
        getbits(ld, 8);
        is_object_layer_identifier = getbits(ld, 1);

        if (is_object_layer_identifier) {
            visual_object_layer_verid = getbits(ld, 4);
            getbits(ld, 3);
        }
        else {
            visual_object_layer_verid = 1;
        }
        aspect_ratio_info = getbits(ld, 4);
        if (aspect_ratio_info == EXTENDED_PAR)
        {
            getbits(ld, 8);
            getbits(ld, 8);
        }
        vol_control_parameters = getbits(ld, 1);
        if (vol_control_parameters)
        {
            getbits(ld, 2);
            getbits(ld, 1);
            vbv_parameters = getbits(ld, 1);
            if (vbv_parameters)
            {
                getbits(ld, 15);
                getbits1(ld); // marker
                getbits(ld, 15);
                getbits1(ld); // marker
                getbits(ld, 15);
                getbits1(ld); // marker
                getbits(ld, 3);
                getbits(ld, 11);
                getbits1(ld); // marker
                getbits(ld, 15);
                getbits1(ld); // marker
            }
        }
        shape = getbits(ld, 2);
        if(shape != RECTANGULAR)
            return -1;

        if ((shape == GRAY_SCALE) &&
            (visual_object_layer_verid != 0x01)) {
                getbits(ld, 4);
            return -1;
        }

        getbits1(ld); // marker
        time_increment_resolution = getbits(ld, 16);
        getbits1(ld); // marker
        fixed_vop_rate = getbits(ld, 1);

        if (fixed_vop_rate)
        {
            CDX_S32 bits = log2ceil(time_increment_resolution);
            if (bits < 1)
                bits = 1;
            getbits(ld, bits);
        }

        if (shape != BINARY_SHAPE_ONLY)
        {
            if(shape == RECTANGULAR)
            {
                getbits1(ld); // marker
                *width = (CDX_U16)getbits(ld, 13);
                getbits1(ld); // marker
                *height = (CDX_U16)getbits(ld, 13);
                getbits1(ld); // marker
            }
        }
    }

    return 0;
}

