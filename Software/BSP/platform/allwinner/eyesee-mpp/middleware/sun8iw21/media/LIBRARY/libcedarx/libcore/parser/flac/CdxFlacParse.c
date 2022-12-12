/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxFlacParse.c
 * Description : Flac Parser.
 * History :
 *
 */

#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxFlacParser.h>

cdx_uint8 const FLAC__crc8_table_[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

/* CRC-16, poly = x^16 + x^15 + x^2 + x^0, init = 0 */

unsigned FLAC__crc16_table1[256] = {
    0x0000,  0x8005,  0x800f,  0x000a,  0x801b,  0x001e,  0x0014,  0x8011,
    0x8033,  0x0036,  0x003c,  0x8039,  0x0028,  0x802d,  0x8027,  0x0022,
    0x8063,  0x0066,  0x006c,  0x8069,  0x0078,  0x807d,  0x8077,  0x0072,
    0x0050,  0x8055,  0x805f,  0x005a,  0x804b,  0x004e,  0x0044,  0x8041,
    0x80c3,  0x00c6,  0x00cc,  0x80c9,  0x00d8,  0x80dd,  0x80d7,  0x00d2,
    0x00f0,  0x80f5,  0x80ff,  0x00fa,  0x80eb,  0x00ee,  0x00e4,  0x80e1,
    0x00a0,  0x80a5,  0x80af,  0x00aa,  0x80bb,  0x00be,  0x00b4,  0x80b1,
    0x8093,  0x0096,  0x009c,  0x8099,  0x0088,  0x808d,  0x8087,  0x0082,
    0x8183,  0x0186,  0x018c,  0x8189,  0x0198,  0x819d,  0x8197,  0x0192,
    0x01b0,  0x81b5,  0x81bf,  0x01ba,  0x81ab,  0x01ae,  0x01a4,  0x81a1,
    0x01e0,  0x81e5,  0x81ef,  0x01ea,  0x81fb,  0x01fe,  0x01f4,  0x81f1,
    0x81d3,  0x01d6,  0x01dc,  0x81d9,  0x01c8,  0x81cd,  0x81c7,  0x01c2,
    0x0140,  0x8145,  0x814f,  0x014a,  0x815b,  0x015e,  0x0154,  0x8151,
    0x8173,  0x0176,  0x017c,  0x8179,  0x0168,  0x816d,  0x8167,  0x0162,
    0x8123,  0x0126,  0x012c,  0x8129,  0x0138,  0x813d,  0x8137,  0x0132,
    0x0110,  0x8115,  0x811f,  0x011a,  0x810b,  0x010e,  0x0104,  0x8101,
    0x8303,  0x0306,  0x030c,  0x8309,  0x0318,  0x831d,  0x8317,  0x0312,
    0x0330,  0x8335,  0x833f,  0x033a,  0x832b,  0x032e,  0x0324,  0x8321,
    0x0360,  0x8365,  0x836f,  0x036a,  0x837b,  0x037e,  0x0374,  0x8371,
    0x8353,  0x0356,  0x035c,  0x8359,  0x0348,  0x834d,  0x8347,  0x0342,
    0x03c0,  0x83c5,  0x83cf,  0x03ca,  0x83db,  0x03de,  0x03d4,  0x83d1,
    0x83f3,  0x03f6,  0x03fc,  0x83f9,  0x03e8,  0x83ed,  0x83e7,  0x03e2,
    0x83a3,  0x03a6,  0x03ac,  0x83a9,  0x03b8,  0x83bd,  0x83b7,  0x03b2,
    0x0390,  0x8395,  0x839f,  0x039a,  0x838b,  0x038e,  0x0384,  0x8381,
    0x0280,  0x8285,  0x828f,  0x028a,  0x829b,  0x029e,  0x0294,  0x8291,
    0x82b3,  0x02b6,  0x02bc,  0x82b9,  0x02a8,  0x82ad,  0x82a7,  0x02a2,
    0x82e3,  0x02e6,  0x02ec,  0x82e9,  0x02f8,  0x82fd,  0x82f7,  0x02f2,
    0x02d0,  0x82d5,  0x82df,  0x02da,  0x82cb,  0x02ce,  0x02c4,  0x82c1,
    0x8243,  0x0246,  0x024c,  0x8249,  0x0258,  0x825d,  0x8257,  0x0252,
    0x0270,  0x8275,  0x827f,  0x027a,  0x826b,  0x026e,  0x0264,  0x8261,
    0x0220,  0x8225,  0x822f,  0x022a,  0x823b,  0x023e,  0x0234,  0x8231,
    0x8213,  0x0216,  0x021c,  0x8219,  0x0208,  0x820d,  0x8207,  0x0202
};

cdx_bool Flac__bitreader_read_utf8_uint64(cdx_uint8 *src,cdx_uint64 *val,
    cdx_uint8 *raw, unsigned *rawlen)
{
    cdx_uint64 v = 0;
    cdx_uint32 x = 0;
    unsigned i = 0;
    cdx_int32 idx = 0;

    if(raw)
    {
        x = src[idx++];
        raw[(*rawlen)++] = (cdx_uint8)x;
    }
    if(!(x & 0x80)) { /* 0xxxxxxx */
        v = x;
        i = 0;
    }
    else if(x & 0xC0 && !(x & 0x20)) { /* 110xxxxx */
        v = x & 0x1F;
        i = 1;
    }
    else if(x & 0xE0 && !(x & 0x10)) { /* 1110xxxx */
        v = x & 0x0F;
        i = 2;
    }
    else if(x & 0xF0 && !(x & 0x08)) { /* 11110xxx */
        v = x & 0x07;
        i = 3;
    }
    else if(x & 0xF8 && !(x & 0x04)) { /* 111110xx */
        v = x & 0x03;
        i = 4;
    }
    else if(x & 0xFC && !(x & 0x02)) { /* 1111110x */
        v = x & 0x01;
        i = 5;
    }
    else if(x & 0xFE && !(x & 0x01)) { /* 11111110 */
        v = 0;
        i = 6;
    }
    else {
        *val = (cdx_uint64)(0xffffffffffffffff);
        return CDX_TRUE;
    }
    for( ; i; i--) {
        x = src[idx++];
        if(raw)
            raw[(*rawlen)++] = (cdx_uint8)x;
        if(!(x & 0x80) || (x & 0x40)) { /* 10xxxxxx */
            *val = (cdx_uint64)(0xffffffffffffffff);
            return CDX_TRUE;
        }
        v <<= 6;
        v |= (x & 0x3F);
    }
    *val = v;
    return CDX_TRUE;
}

cdx_bool Flac__bitreader_read_utf8_uint32(cdx_uint8 *src,cdx_uint32 *val,
    cdx_uint8 *raw, unsigned *rawlen)
{
    cdx_uint32 v = 0;
    cdx_uint32 x = 0;
    unsigned   i = 0;
    cdx_int32  idx = 0;

    if(raw)
    {
        x = src[idx++];
        raw[(*rawlen)++] = (cdx_uint8)x;
    }
    if(!(x & 0x80))
    { /* 0xxxxxxx */
        v = x;
        i = 0;
    }
    else if(x & 0xC0 && !(x & 0x20))
    { /* 110xxxxx */
        v = x & 0x1F;
        i = 1;
    }
    else if(x & 0xE0 && !(x & 0x10))
    { /* 1110xxxx */
        v = x & 0x0F;
        i = 2;
    }
    else if(x & 0xF0 && !(x & 0x08))
    { /* 11110xxx */
        v = x & 0x07;
        i = 3;
    }
    else if(x & 0xF8 && !(x & 0x04))
    { /* 111110xx */
        v = x & 0x03;
        i = 4;
    }
    else if(x & 0xFC && !(x & 0x02))
    { /* 1111110x */
        v = x & 0x01;
        i = 5;
    }
    else
    {
        *val = 0xffffffff;
        return CDX_TRUE;
    }
    for( ; i; i--)
    {
        x = src[idx++];
        if(raw)
       raw[(*rawlen)++] = (cdx_uint8)x;
    if(!(x & 0x80) || (x & 0x40))
    { /* 10xxxxxx */
        *val = 0xffffffff;
        return CDX_TRUE;
    }
    v <<= 6;
    v |= (x & 0x3F);
    }
    *val = v;
    return CDX_TRUE;
}

cdx_uint8 Crc8(const cdx_uint8 *data, unsigned len)
{
    cdx_uint8 crc = 0;

    while(len--)
        crc = FLAC__crc8_table_[crc ^ *data++];

    return crc;
}

cdx_bool read_frame_header_(cdx_uint8 *src,FlacParserImplS *impl)
{
    cdx_uint32 x;
    cdx_uint64 xx;
    unsigned i, blocksize_hint = 0, sample_rate_hint = 0;
    cdx_uint8 crc8, raw_header[16]; /* MAGIC NUMBER based on the
                                     maximum frame header size, including CRC */
    unsigned raw_header_len;
    cdx_bool is_unparseable = CDX_FALSE;

    /* init the raw header with the saved bits from synchronization */
    raw_header[0] = src[0];
    raw_header[1] = src[1];
    raw_header_len = 2;
    if((AV_RB16(raw_header) & 0xFFFE) != 0xFFF8)
    {
        CDX_LOGE("+++++++++++++Sync header error+++++++++++");
        return CDX_FALSE;
    }

    /* check to make sure that reserved bit is 0 */
    if(raw_header[1] & 0x02) /* MAGIC NUMBER */
    {
        CDX_LOGV("Sync head error at bit 15!");
        is_unparseable = CDX_TRUE;
    }

    /*
     * Note that along the way as we read the header, we look for a sync
     * code inside.  If we find one it would indicate that our original
     * sync was bad since there cannot be a sync code in a valid header.
     *
     * Three kinds of things can go wrong when reading the frame header:
     *  1) We may have sync'ed incorrectly and not landed on a frame header.
     *     If we don't find a sync code, it can end up looking like we read
     *     a valid but unparseable header, until getting to the frame header
     *     CRC.  Even then we could get a false positive on the CRC.
     *  2) We may have sync'ed correctly but on an unparseable frame (from a
     *     future encoder).
     *  3) We may be on a damaged frame which appears valid but unparseable.
     *
     * For all these reasons, we try and read a complete frame header as
     * long as it seems valid, even if unparseable, up until the frame
     * header CRC.
     */

    /*
     * read in the raw header as bytes so we can CRC it, and parse it on the way
     */
    for(i = 0; i < 2; i++) {
        x = src[raw_header_len];
        if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
            /* if we get here it means our original sync was erroneous since
            the sync code cannot appear in the header */
                CDX_LOGV("Sync head error at byte 2,3!");
                return CDX_FALSE;
        }
        raw_header[raw_header_len++] = (cdx_uint8)x;
    }

    switch(x = raw_header[2] >> 4)
    {
        case 0:
            is_unparseable = CDX_TRUE;
            break;
        case 1:
            impl->orgSampsPerFrm = 192;
            break;
        case 2:
        case 3:
        case 4:
        case 5:
            impl->orgSampsPerFrm = 576 << (x-2);
            break;
        case 6:
        case 7:
            blocksize_hint = x;
            break;
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            impl->orgSampsPerFrm = 256 << (x-8);
            break;
        default:
            impl->orgSampsPerFrm = 0;
            break;
    }

    switch(x = raw_header[2] & 0x0f)
    {
        case 0:
            if(impl->orgSr)
                impl->orgSr = impl->ulSampleRate;
            else{
                CDX_LOGV("Sync head error at impl->orgSr == 0!");
                is_unparseable = CDX_TRUE;
            }
            break;
        case 1:
            impl->orgSr = 88200;
            break;
        case 2:
            impl->orgSr = 176400;
            break;
        case 3:
            impl->orgSr = 192000;
            break;
        case 4:
            impl->orgSr = 8000;
            break;
        case 5:
            impl->orgSr = 16000;
            break;
        case 6:
            impl->orgSr = 22050;
            break;
        case 7:
            impl->orgSr = 24000;
            break;
        case 8:
            impl->orgSr = 32000;
            break;
        case 9:
            impl->orgSr = 44100;
            break;
        case 10:
            impl->orgSr = 48000;
            break;
        case 11:
            impl->orgSr = 96000;
            break;
        case 12:
        case 13:
        case 14:
            sample_rate_hint = x;
            break;
        case 15:
            CDX_LOGV("Sync head error at unknown Sr !");
            return CDX_FALSE;
        default:
            impl->orgSr = 0;
            break;
    }

    x = (unsigned)(raw_header[3] >> 4);
    if(x & 8)
    {
        impl->orgChans= 2;
        switch(x & 7)
        {
            case 0:
                impl->orgChanAsign = FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE;
                break;
            case 1:
                impl->orgChanAsign = FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE;
                break;
            case 2:
                impl->orgChanAsign = FLAC__CHANNEL_ASSIGNMENT_MID_SIDE;
                break;
            default:
                CDX_LOGV("Sync head error at unknown chanasign !");
                is_unparseable = CDX_TRUE;
                break;
        }
    }
    else
    {
        impl->orgChans = (unsigned)x + 1;
        impl->orgChanAsign = FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT;
    }

    switch(x = (unsigned)(raw_header[3] & 0x0e) >> 1)
    {
        case 0:
        if(impl->ulBitsSample)
            impl->orgSampbit = impl->ulBitsSample;
        else
        {
            CDX_LOGV("Sync head error at impl->orgSampbit !");
            is_unparseable = CDX_TRUE;
        }
        break;
        case 1:
            impl->orgSampbit = 8;
            break;
        case 2:
            impl->orgSampbit = 12;
            break;
        case 4:
            impl->orgSampbit = 16;
            break;
        case 5:
            impl->orgSampbit = 20;
            break;
        case 6:
            impl->orgSampbit = 24;
            break;
        case 3:
        case 7:
            CDX_LOGV("Sync head error at unknown Sampbit !");
            is_unparseable = CDX_TRUE;
            break;
        default:
            impl->orgSampbit = 0;;
            break;
    }

    /* check to make sure that reserved bit is 0 */
    if(raw_header[3] & 0x01) /* MAGIC NUMBER */
    {
        CDX_LOGV("Sync head error at bit 31");
        is_unparseable = CDX_TRUE;
    }

    /* read the frame's starting sample number (or frame number as the case may be) */
    if((raw_header[1]&0x01) && (impl->minBlocksize != impl->maxBlocksize))
    {
        /* variable blocksize */
        if(!Flac__bitreader_read_utf8_uint64(&src[raw_header_len], &xx, raw_header,
            &raw_header_len))
        {
            return CDX_FALSE; /* read_callback_ sets the state for us */
        }
        if(xx == (cdx_uint64)(0xffffffffffffffff))
        {
            /* i.e. non-UTF8 code... */
            CDX_LOGV("Sync head error at variable blocksize uft64!");
            return CDX_FALSE;
        }
    }
    else
    {
        /* fixed blocksize */
        if(!Flac__bitreader_read_utf8_uint32(&src[raw_header_len], &x, raw_header, &raw_header_len))
            return CDX_FALSE; /* read_callback_ sets the state for us */
        if(x == 0xffffffff)
        {
            /* i.e. non-UTF8 code... */
            CDX_LOGV("Sync head error at fixed blocksize uft32!");
            return CDX_FALSE;
        }
    }

    if(blocksize_hint)
    {
        x = src[raw_header_len];
        raw_header[raw_header_len++] = (cdx_uint8)x;
        if(blocksize_hint == 7)
        {
            cdx_uint32 _x;
            _x = src[raw_header_len];
            raw_header[raw_header_len++] = (cdx_uint8)_x;
            x = (x << 8) | _x;
        }
        impl->orgSampsPerFrm = x+1;
    }

    if(sample_rate_hint) {
        x = src[raw_header_len];
        raw_header[raw_header_len++] = (cdx_uint8)x;
        if(sample_rate_hint != 12)
        {
            cdx_uint32 _x;
            _x = src[raw_header_len];
            raw_header[raw_header_len++] = (cdx_uint8)_x;
            x = (x << 8) | _x;
        }
        if(sample_rate_hint == 12)
            impl->orgSr = x*1000;
        else if(sample_rate_hint == 13)
            impl->orgSr = x;
        else
            impl->orgSr = x*10;
    }
    /*if samplerate has changed unexpected, we thouht it is a invalid frame*/
    if((impl->ulSampleRate !=0) && (impl->orgSr != impl->ulSampleRate))
    {
        return CDX_FALSE;
    }

    /*if blocksize has changed unexpected, we thouht it is a invalid frame*/
    if((impl->orgSampsPerFrm < impl->minBlocksize) || (impl->orgSampsPerFrm > impl->maxBlocksize))
    {
        CDX_LOGD("orgSampsPerFrm : %d, min : %d, max : %d",
                impl->orgSampsPerFrm, impl->minBlocksize, impl->maxBlocksize);
        return CDX_FALSE;
    }

    /* read the CRC-8 byte */
    x = src[raw_header_len];
    crc8 = (cdx_uint8)x;

    if(Crc8(raw_header, raw_header_len) != crc8) {
        CDX_LOGV("Sync head error at crc");
        return CDX_FALSE;
    }

    /* calculate the sample number from the frame number if needed

    */
    if(is_unparseable) {
        CDX_LOGV("Sync head error at unparseable");
        return CDX_FALSE;
    }

    return CDX_TRUE;
}

cdx_int32    Flac_GetInfo_GetBsInByte(cdx_int32    ByteLen, CdxStreamT *cdxStream)
{
    cdx_int32 RetVal = 0;
    cdx_int32 ret = 0;
    cdx_int32 i;
    cdx_uint8 data;
    cdx_uint8 buf[4] = {0};

    for    (i=0;i<ByteLen;i++)
    {
        ret = CdxStreamRead(cdxStream, buf, 1);
        data= buf[0];
        if(ret!=1)
            return -1;

        RetVal = (RetVal<<8)|(data&0xff);
    }
    return    RetVal;
}

static int FlacInit(CdxParserT *flac_impl)
{
    cdx_int32  retval= 0;
    cdx_uint8 *  buffer = NULL;
    cdx_uint8    buffer_head8[8] = {0x66, 0x4c, 0x61, 0x43,
                                    0x00, 0x00, 0x00, 0x00};
    cdx_int64    metadata_off = 0;
    cdx_int32 buf_end  = 0, readlen = 0;
    cdx_int32 buf_ptr  = 0;
    cdx_int64 syncoff  = 0;
    FlacParserImplS *impl= (FlacParserImplS*)flac_impl;
    impl->fileSize = CdxStreamSize(impl->stream);
    CdxStreamT *cdxStream = NULL;
    cdxStream = impl->stream;
    ///flac check
    if(impl->fileSize<0)
    {
        goto OPENFAILURE;
    }

    retval = Flac_GetInfo_GetBsInByte(4,cdxStream);
    impl->file_offset += 4;
    if(retval < 0)
    {
        goto OPENFAILURE;
    }
    if(retval == 0x664c6143)
    {
        cdx_int32  blocktype,blocklen;
        cdx_int32  lastblock=0;
        cdx_int32  data;
        cdx_int32  headlen = 4;
        cdx_int64  totalsamples = 0;
        cdx_int64  TempData;

        while(!lastblock)
        {
            blocktype = Flac_GetInfo_GetBsInByte(1,cdxStream);
            impl->file_offset += 1;
            if(blocktype < 0)
            {
                goto OPENFAILURE;
            }
            blocklen = Flac_GetInfo_GetBsInByte(3,cdxStream);
            impl->file_offset += 3;
            if(blocklen < 0)
            {
                goto OPENFAILURE;
            }
            lastblock = (blocktype&0x80)?1:0;
            blocktype &=0x7f;
            headlen += (blocklen+4);
            switch(blocktype)
            {
                case 0:  //METADATA_BLOCK_STREAMINFO
                    buffer_head8[4] = (blocktype&0xFF);
                    buffer_head8[5] = ((blocklen>>16)&0xFF);
                    buffer_head8[6] = ((blocklen>>8)&0xFF);
                    buffer_head8[7] = ((blocklen)&0xFF);
                    buffer = malloc(12 + blocklen + INPUT_BUFFER_GUARD_SIZE);
                    memset(buffer,0,12 + blocklen + INPUT_BUFFER_GUARD_SIZE);
                    metadata_off = CdxStreamTell(impl->stream);
                    CdxStreamRead(impl->stream, buffer + 8, blocklen);
                    memcpy(buffer, buffer_head8, 8);
                    buffer[8 + blocklen] = 0x81;
                    CdxStreamSeek(impl->stream, metadata_off, SEEK_SET);
                    impl->extradata = buffer;
                    impl->extradata_size = 12 + blocklen;

                    data = Flac_GetInfo_GetBsInByte(2,cdxStream);//16bit
                    if(data < 0)
                    {
                        goto OPENFAILURE;
                    }
                    impl->minBlocksize = data&0xffff;
                    impl->file_offset += 2;
                    data = Flac_GetInfo_GetBsInByte(2,cdxStream);//16bit
                    if(data < 0)
                    {
                        goto OPENFAILURE;
                    }
                    impl->maxBlocksize = data&0xffff;
                    impl->file_offset += 2;
                    data = Flac_GetInfo_GetBsInByte(3,cdxStream);//24bit   24
                    if(data < 0)
                    {
                        goto OPENFAILURE;
                    }
                    impl->minFramesize = data&0xffffff;
                    impl->file_offset += 3;

                    data = Flac_GetInfo_GetBsInByte(3,cdxStream);//24bit  24
                    if(data < 0)
                    {
                        goto OPENFAILURE;
                    }
                    impl->maxFramesize = data&0xffffff;
                    if(impl->maxFramesize <= 64)
                    {
                        impl->maxFramesize = (20*1024);//for fix some bad flac stream
                    }
                    impl->maxFramesize *= 3;//for fetch a frame
                    impl->file_offset += 3;

                    //GetInfo_SkipBsInByte(10,AIF);
                    data = Flac_GetInfo_GetBsInByte(4,cdxStream);//32bit     20  3  5   4
                    impl->file_offset += 4;
                    if(data < 0)
                    {
                        goto OPENFAILURE;
                    }
                    impl->ulSampleRate = (data>>12)&0xfffff;
                    impl->ulChannels = ((data>>9)&0x7)+1;
                    impl->ulChannels = impl->ulChannels>2?2 : impl->ulChannels;
                    impl->ulBitsSample = ((data>>4)&0x1f)+1;
                    totalsamples = data&0xf;  //  32
                    totalsamples <<=32;
                    retval = Flac_GetInfo_GetBsInByte(4,cdxStream);
                    if(retval<0)
                    {
                        goto OPENFAILURE;
                    }
                    impl->file_offset += 4;
                    totalsamples |= retval;
                    impl->totalsamples = totalsamples;
                    TempData = totalsamples*1000 ;
                    impl->ulDuration = (cdx_int32)(TempData/impl->ulSampleRate);
                    //AIF->ulBitRate = (filelen/(AIF->ulDuration/1000))*8;
                    CdxStreamSeek(cdxStream,blocklen-18,SEEK_CUR);
                    impl->file_offset += blocklen-18;
                    //GetInfo_SkipBsInByte(blocklen - 18,AIF);
                    break;
                case 4: //VORBIS_COMMENT
                    //do not apply GetInfo_oggStandardTag(impl);
                    CdxStreamSeek(cdxStream,blocklen,SEEK_CUR);
                    impl->file_offset += blocklen;
                    break;
                default:
                    CdxStreamSeek(cdxStream,blocklen,SEEK_CUR);
                    impl->file_offset += blocklen;
                    //GetInfo_SkipBsInByte(blocklen,AIF);
                    break;
                }
        }
        impl->ulBitRate = (unsigned int)((double)impl->fileSize*8/
            ((double)totalsamples/(double)impl->ulSampleRate));
        /*if(0)//impl->InforBufLeftLength>=4)
        {
            impl->ulAudio_name_sz = 4;
            impl->ulAudio_name = impl->InforBuf;
            impl->InforBuf +=impl->ulAudio_name_sz;
            impl->InforBufLeftLength -=impl->ulAudio_name_sz;
            memcpy(impl->ulAudio_name,flaccodecInfo[0],impl->ulAudio_name_sz);

        }*/

    }
    impl->pktbuf=malloc(impl->maxFramesize+INPUT_BUFFER_PADDING_SIZE+INPUT_BUFFER_GUARD_SIZE);
    memset(impl->pktbuf,0,impl->maxFramesize+INPUT_BUFFER_PADDING_SIZE+INPUT_BUFFER_GUARD_SIZE);

    readlen = impl->maxFramesize+INPUT_BUFFER_PADDING_SIZE;
    buf_end = CdxStreamRead(impl->stream, impl->pktbuf, readlen);

    if(CdxStreamTell(impl->stream) == impl->fileSize)
    {
        goto SUCESS;
    }
    if(buf_end < impl->maxFramesize+INPUT_BUFFER_PADDING_SIZE)
    {
        CDX_LOGE("First frame data not enough, failure and out!");
        if(impl->pktbuf)
        {
            free(impl->pktbuf);
            impl->pktbuf = NULL;
        }
        CdxStreamSeek(cdxStream, 0,SEEK_SET);
        goto OPENFAILURE;
    }
    if(!read_frame_header_(&impl->pktbuf[buf_ptr],impl))
    {
        CDX_LOGD("After seek is not right header,we need resync!");
        while(1)
        {
            buf_ptr ++;
            syncoff ++;
            while (buf_ptr+2 < buf_end && (AV_RB16(impl->pktbuf+buf_ptr) & 0xFFFE) != 0xFFF8)
            {
                buf_ptr++;
                syncoff++;
            }
            if(buf_ptr+2 >= buf_end)
            {
                CDX_LOGE("Sync out of range, fail to open flac parser");
                goto OPENFAILURE;
            }
            if(read_frame_header_(&impl->pktbuf[buf_ptr],impl))
            {
                CDX_LOGE("find a valid frameheader, break out ");
                break;
            }
        }
    }

SUCESS:
    impl->data_offset = impl->file_offset;
    CdxStreamSeek(cdxStream, impl->data_offset,SEEK_SET);
    CDX_LOGD("impl->extradata_size : %d, impl->extradata : %p, impl->file_offset :%lld",
            impl->extradata_size, impl->extradata, impl->file_offset);

    buffer=NULL;
    impl->mErrno = PSR_OK;

    pthread_cond_signal(&impl->cond);
    return 0;

OPENFAILURE:
    CDX_LOGE("FlacOpenThread fail!!!");
    impl->mErrno = PSR_OPEN_FAIL;
    pthread_cond_signal(&impl->cond);
    return -1;
}

static cdx_int32 __FlacParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    struct FlacParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct FlacParserImplS, base);
    (void)param;

    switch (cmd)
    {
        case CDX_PSR_CMD_DISABLE_AUDIO:
        case CDX_PSR_CMD_DISABLE_VIDEO:
        case CDX_PSR_CMD_SWITCH_AUDIO:
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            CdxStreamForceStop(impl->stream);
            break;
        case CDX_PSR_CMD_CLR_FORCESTOP:
            CdxStreamClrForceStop(impl->stream);
            break;
        default :
            CDX_LOGW("not implement...(%d)", cmd);
            break;
    }
    impl->flags = cmd;
    return CDX_SUCCESS;
}

static cdx_int32 __FlacParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 ret = 0;
    cdx_int32 framesize=0,framesizetarget=0;
    cdx_int32 buf_end  = 0;
    cdx_int32 buf_ptr  = 0;
    cdx_int64 syncoff  = 0;
    struct FlacParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct FlacParserImplS, base);
    //buf_end = impl->maxFramesize+INPUT_BUFFER_PADDING_SIZE;
    buf_end = ret = CdxStreamRead(impl->stream, impl->pktbuf,
                                  impl->maxFramesize+INPUT_BUFFER_PADDING_SIZE);
    if(ret == 0)//< impl->maxFramesize + INPUT_BUFFER_PADDING_SIZE)
    {
        CDX_LOGE("Not enough data!");
        return CDX_FAILURE;
    }
    //insure that we snyc to the head
    if ((AV_RB16(impl->pktbuf) & 0xFFFE) != 0xFFF8) {
        CDX_LOGW("Prefetch Frame Header, but NOT HERE");
        while (buf_ptr+2 < buf_end && (AV_RB16(impl->pktbuf+buf_ptr) & 0xFFFE) != 0xFFF8)
        {
            buf_ptr++;
            syncoff ++;
        }
    }
    //we have  snyc to a right header
    if(0)//!impl->isFirstFrame)
    {
        //we have to judge a right header
        if(!read_frame_header_(&impl->pktbuf[buf_ptr],impl))
        {
            CDX_LOGE("First Frame invaild!");
            return CDX_FAILURE;
        }
    }
    else
    {
        if(!read_frame_header_(&impl->pktbuf[buf_ptr],impl))
        {
            CDX_LOGW("After seek is not right header,we need resync!");
            while(1)
            {
                buf_ptr ++;
                syncoff ++;
                while (buf_ptr+2 < buf_end && (AV_RB16(impl->pktbuf+buf_ptr) & 0xFFFE) != 0xFFF8)
                {
                    buf_ptr++;
                    syncoff++;
                }
                if(buf_ptr+2 >= buf_end)
                {
                    CDX_LOGE("Sync out of range while find 2nd frame");
                    return CDX_FAILURE;
                }
                if(read_frame_header_(&impl->pktbuf[buf_ptr],impl))
                {
                    CDX_LOGE("find a valid frameheader, break out ");
                    break;
                }
            }
        }
    }

    //we need to sync to next frame head to get framelength;
    while(1)
    {
        buf_ptr += 2;
        framesize = 2;
        while (buf_ptr+2 < buf_end && (AV_RB16(impl->pktbuf+buf_ptr) & 0xFFFE) != 0xFFF8)
        {
            buf_ptr++;
            framesize++;
        }
        if(buf_ptr+2 == buf_end)
        {
            CDX_LOGW("Give up last frame!!");
            return CDX_FAILURE;
        }
        if(read_frame_header_(&impl->pktbuf[buf_ptr],impl))
        {
            framesizetarget += framesize;
            break;
        }
        if(framesizetarget > impl->maxFramesize-1)
            break;
        framesizetarget += framesize;
    }

    pkt->type = CDX_MEDIA_AUDIO;
    pkt->length = framesizetarget;
    pkt->pts = impl->nowsamples*1000000ll/impl->ulSampleRate;//one frame pts;
    pkt->flags |= (FIRST_PART|LAST_PART);

    impl->file_offset += syncoff;
    CdxStreamSeek(impl->stream,impl->file_offset ,SEEK_SET);
    if(!impl->isFirstFrame)
    {
       impl->isFirstFrame = 1;
    }
    return CDX_SUCCESS;
}

static cdx_int32 __FlacParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 read_length;
    struct FlacParserImplS *impl = NULL;
    CdxStreamT *cdxStream = NULL;

    impl = CdxContainerOf(parser, struct FlacParserImplS, base);
    cdxStream = impl->stream;
    //READ ACTION
    if(pkt->length <= pkt->buflen)
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->length);
    }
    else
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->buflen);
        read_length += CdxStreamRead(cdxStream, pkt->ringBuf,    pkt->length - pkt->buflen);
    }

    if(read_length < 0)
    {
        CDX_LOGE("CdxStreamRead fail");
        impl->mErrno = PSR_IO_ERR;
        return CDX_FAILURE;
    }
    else if(read_length == 0)
    {
       CDX_LOGD("CdxStream EOS");
       impl->mErrno = PSR_EOS;
       return CDX_FAILURE;
    }
    pkt->length = read_length;
    impl->file_offset += read_length;
    impl->nowsamples += impl->orgSampsPerFrm;
    // TODO: fill pkt
    return CDX_SUCCESS;
}

static cdx_int32 __FlacParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    struct FlacParserImplS *impl = NULL;
    struct CdxProgramS *cdxProgram = NULL;

    impl = CdxContainerOf(parser, struct FlacParserImplS, base);
    memset(mediaInfo, 0x00, sizeof(*mediaInfo));

    if(impl->mErrno != PSR_OK)
    {
        CDX_LOGE("audio parse status no PSR_OK");
        return CDX_FAILURE;
    }

    mediaInfo->programNum = 1;
    mediaInfo->programIndex = 0;
    cdxProgram = &mediaInfo->program[0];
    memset(cdxProgram, 0, sizeof(struct CdxProgramS));
    cdxProgram->id = 0;
    cdxProgram->audioNum = 1;
    cdxProgram->videoNum = 0;//
    cdxProgram->subtitleNum = 0;
    cdxProgram->audioIndex = 0;
    cdxProgram->videoIndex = 0;
    cdxProgram->subtitleIndex = 0;

    mediaInfo->bSeekable = CdxStreamSeekAble(impl->stream);
    mediaInfo->fileSize = impl->fileSize;

    cdxProgram->duration = impl->ulDuration;
    cdxProgram->audio[0].eCodecFormat    = AUDIO_CODEC_FORMAT_FLAC;
    cdxProgram->audio[0].eSubCodecFormat = 0;
    cdxProgram->audio[0].nChannelNum     = impl->ulChannels;
    cdxProgram->audio[0].nBitsPerSample  = impl->ulBitsSample;
    cdxProgram->audio[0].nSampleRate     = impl->ulSampleRate;
    cdxProgram->audio[0].nCodecSpecificDataLen = impl->extradata_size;
    cdxProgram->audio[0].pCodecSpecificData = (char *)impl->extradata;
    return CDX_SUCCESS;
}

static cdx_int32 __FlacParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    struct FlacParserImplS *impl = NULL;
    cdx_int32 samples = 0;
    cdx_int64 offset = 0;
    cdx_int32 times = 0;
    impl = CdxContainerOf(parser, struct FlacParserImplS, base);
    samples=(timeUs*impl->ulSampleRate)/1000000ll;
    offset = (cdx_int64)impl->data_offset + (cdx_int64)samples*
        impl->fileSize/(cdx_int64)impl->totalsamples;
    if(offset!= impl->file_offset)
    {
        if(offset > impl->fileSize)
        {
            offset = impl->fileSize;
        }
        if(CdxStreamSeek(impl->stream,offset,SEEK_SET))
        {
            CDX_LOGE("CdxStreamSeek fail");
            return CDX_FAILURE;
        }
    }
    impl->file_offset = offset;
    times = samples/impl->orgSampsPerFrm;
    samples = impl->orgSampsPerFrm*(times+1);
    impl->nowsamples = samples;
    CdxStreamSeek(impl->stream,impl->file_offset,SEEK_SET);
    pthread_cond_signal(&impl->cond);
    return CDX_SUCCESS;
}

static cdx_uint32 __FlacParserAttribute(CdxParserT *parser)
{
    struct FlacParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct FlacParserImplS, base);
    return 0;
}

static cdx_int32 __FlacParserGetStatus(CdxParserT *parser)
{
    struct FlacParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct FlacParserImplS, base);
#if 0
    if (CdxStreamEos(impl->stream))
    {
        return PSR_EOS;
    }
#endif
    return impl->mErrno;
}

static cdx_int32 __FlacParserClose(CdxParserT *parser)
{
    struct FlacParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct FlacParserImplS, base);

    //pthread_join(impl->openTid, NULL);
    if(impl->pktbuf)
    {
        free(impl->pktbuf);
        impl->pktbuf = NULL;
    }
    if(impl->extradata)
    {
        free(impl->extradata);
        impl->extradata = NULL;
    }
    CdxStreamClose(impl->stream);
    pthread_cond_destroy(&impl->cond);
    CdxFree(impl);
    return CDX_SUCCESS;
}

static struct CdxParserOpsS flacParserOps =
{
    .control = __FlacParserControl,
    .prefetch = __FlacParserPrefetch,
    .read = __FlacParserRead,
    .getMediaInfo = __FlacParserGetMediaInfo,
    .seekTo = __FlacParserSeekTo,
    .attribute = __FlacParserAttribute,
    .getStatus = __FlacParserGetStatus,
    .close = __FlacParserClose,
    .init = FlacInit
};

static cdx_int32 FlacProbe(CdxStreamProbeDataT *p)
{
    cdx_char *d;
    d = p->buf;
    if (p->len < 4 || memcmp(d, "fLaC", 4))
        return CDX_FALSE;
    return CDX_TRUE;
}

static cdx_uint32 __FlacParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_CHECK(probeData);
    if(probeData->len < 4)
    {
        CDX_LOGE("Probe Flac_header data is not enough.");
        return 0;
    }

    if(!FlacProbe(probeData))
    {
        CDX_LOGE("Flac probe failed.");
        return 0;
    }
    return 100;
}

static CdxParserT *__FlacParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    //cdx_int32 ret = 0;
    struct FlacParserImplS *impl;
    impl = CdxMalloc(sizeof(*impl));

    memset(impl, 0x00, sizeof(*impl));
    (void)flags;

    impl->stream = stream;
    impl->base.ops = &flacParserOps;

    pthread_cond_init(&impl->cond, NULL);
    //ret = pthread_create(&impl->openTid, NULL, FlacOpenThread, (void*)impl);
    //CDX_FORCE_CHECK(!ret);
    impl->mErrno = PSR_INVALID;

    return &impl->base;
}

struct CdxParserCreatorS flacParserCtor =
{
    .probe = __FlacParserProbe,
    .create = __FlacParserOpen,
};
