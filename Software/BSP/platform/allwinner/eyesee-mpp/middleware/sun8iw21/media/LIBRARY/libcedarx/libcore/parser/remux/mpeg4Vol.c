/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : mpeg4Vol.c
 * Description : mpeg4Vol
 * History :
 *
 */

#include "mpeg4Vol.h"
#include <inttypes.h>

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
    cdx_uint32   length;
    cdx_uint32   count;
    cdx_uint8    *startptr;
    cdx_uint8    *buf_start_ptr;
    cdx_uint8    *buf_end_ptr;
    cdx_uint8    *rdptr;

    cdx_uint32   bitcnt;
    cdx_uint32   bit_a, bit_b;
}MP4_STREAM;


#define _SWAP(a,b) (b=((a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3]))

static cdx_int32 log2ceil(cdx_int32 arg)
{
    cdx_int32 j=0;
    cdx_int32 i=1;
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
static void initbits (MP4_STREAM * _ld, cdx_uint8 *stream, cdx_int32 length,
    cdx_uint8 *bufstartptr,cdx_uint8 *bufendptr)
{
    cdx_int32 i;
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

static cdx_int32 showbits1 (MP4_STREAM * ld)
{
    if(ld->bit_a & (0x80000000 >> ld->bitcnt))
        return 1;
    else
        return 0;
}

static void flushbits (MP4_STREAM * ld, cdx_int32 n)
{
    cdx_int32 i;

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
static cdx_uint32 showbits (MP4_STREAM * ld, cdx_int32 n)
{
    cdx_int32 nbit = (n + ld->bitcnt) - 32;

    if (nbit > 0)
    {
        // The bits are on both ints
        return (((ld->bit_a & (0xFFFFFFFF >> (ld->bitcnt))) << nbit) |
            (ld->bit_b >> (32 - nbit)));

    }
    else
    {
        cdx_int32 rbit = 32 - ld->bitcnt;
        return (ld->bit_a & (0xFFFFFFFF >> (ld->bitcnt))) >> (rbit-n);
    }
}

static cdx_uint32 getbits (MP4_STREAM * ld, cdx_int32 n)
{
    cdx_uint32 l = showbits (ld, n);
    flushbits (ld, n);
    return l;
}

static cdx_uint32 getbits1(MP4_STREAM * ld)
{
    cdx_uint32 l = showbits1 (ld);
    flushbits (ld, 1);
    return l;
}

cdx_int32 mpeg4getvolhdr(cdx_uint8 *start_addr,cdx_int32 length,cdx_int32 *width,cdx_int32 *height)
{
    MP4_STREAM _ld;
    MP4_STREAM * ld = &_ld;
    cdx_int32 code,i=0;

    initbits(ld, start_addr, length,start_addr,start_addr+length-1);

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
        cdx_int32 is_object_layer_identifier,visual_object_layer_verid,aspect_ratio_info,
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
            cdx_int32 bits = log2ceil(time_increment_resolution);
            if (bits < 1)
                bits = 1;
            getbits(ld, bits);
        }

        if (shape != BINARY_SHAPE_ONLY)
        {
            if(shape == RECTANGULAR)
            {
                getbits1(ld); // marker
                *width = (cdx_uint16)getbits(ld, 13);
                getbits1(ld); // marker
                *height = (cdx_uint16)getbits(ld, 13);
                getbits1(ld); // marker
            }
        }
    }

    return 0;
}

int h263GetPicHdr(cdx_uint8 *start_addr, cdx_int32 length, cdx_int32 *width, cdx_int32 *height)
{
    MP4_STREAM _ld;
    MP4_STREAM * ld = &_ld;
    cdx_uint32 code, gob;
    cdx_uint32 tmp;
    cdx_uint32 UFEP;
    cdx_uint32 temp_ref;
    cdx_int32 trd = 0;
    cdx_uint32 source_format = 0;

    cdx_int32 lines[7] = {-1,128,176,352,704,1408,-1};
    cdx_int32 pels[7]  = {-1,96,144,288,576,1152,-1};

    initbits(ld, start_addr, length,start_addr,start_addr+length-1);

    //* start code;
    code = getbits(ld, 17);
    if (code != 0x1)
        return -1;

    gob  = getbits (ld, 5);
    if (gob == 0x1f)
        return -1;      //* end of stream;

    if (gob != 0)
        return -1;       //* no picture header;

    temp_ref = getbits (ld, 8);
    if (trd < 0)
        trd += 256;

    tmp = getbits (ld, 5);
    /* 1, 0, split_screen_indicator, document_camera_indicator,  freeze_picture_release*/

    tmp = getbits (ld, 3);

    if(tmp == 0)        //forbidden format
    {
        return -1;
    }

    if (tmp != 0x7)
    {
        source_format = tmp;
        *width  = lines[source_format];
        *height = pels[source_format];
    }

    return 0;
}
