/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : MediaProbe.c
 * Description : MediaProbe
 * History :
 *
 */

#include <CdxTsParser.h>
#include <cdx_log.h>

#define    SYNCWORDH            0xff
#define    SYNCWORDL            0xf0
#define  DCA_MARKER_14B_BE    0x1FFFE800
#define  DCA_MARKER_14B_LE    0xFF1F00E8
#define  DCA_MARKER_RAW_BE    0x7FFE8001
#define  DCA_MARKER_RAW_LE    0xFE7F0180

static const cdx_int32 sampRateTab[12] =
{
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025,  8000
};


cdx_int32 GetAACDuration(const cdx_uint8 *data, cdx_int32 datalen)
{
    cdx_int32 i                     = 0;
    cdx_int32 firstframe            = 1;
    cdx_uint32 Duration     = 0;     //ms
    cdx_uint32 BitRate      = 0;
    cdx_uint32 frameOn      = 0;
    cdx_uint8 layer       = 0;
    cdx_uint8 profile     = 0;
    cdx_uint8 channelConfig = 0;
    cdx_uint32  sampRateIdx = 0;
    //cdx_uint32  SampleRate  = 0;
    cdx_int32          frameLength = 0;

    cdx_uint8 firlayer       = 0;
    cdx_uint8 firprofile     = 0;
    cdx_uint8 firchannelConfig = 0;
    cdx_uint32  firsampRateIdx = 0;
    cdx_uint32  firSampleRate  = 0;

    if(datalen < 4)
        return 0;

    for (i = 0; i < datalen - 5; i++)
    {
        if((data[i + 0] & SYNCWORDH) == SYNCWORDH && (data[i + 1] & SYNCWORDL) == SYNCWORDL)
        {
            if (firstframe)
            {
                firlayer       = (data[i + 1] >> 1) & 0x03;
                firprofile     = (data[i +2] >> 6) & 0x03;
                firsampRateIdx = (data[i+2] >> 2) & 0x0f;
                firSampleRate  = sampRateTab[firsampRateIdx];
                firchannelConfig = (((data[i+2] << 2) & 0x04) | ((data[i+3] >> 6) & 0x03));

                frameLength = ((cdx_int32)(data[i+3] & 0x3) << 11)
                    | ((cdx_int32)(data[i+4] & 0xff) << 3)
                    | ((data[i+5] >> 5) & 0x07);

                if (layer != 0
                    || sampRateIdx >= 12
                    || channelConfig >= 8
                    || frameLength > 2*1024*2)
                {
                    continue;
                }

                firstframe  = 0;
                i += frameLength ;
                frameOn++;

            }

            if(i < datalen - 5)
            {
                layer       = (data[i+1] >> 1) & 0x03;
                profile     = (data[i+2] >> 6) & 0x03;
                sampRateIdx = (data[i+2] >> 2) & 0x0f;
                channelConfig = (((data[i+2] << 2) & 0x04) | ((data[i+3] >> 6) & 0x03));
                if ( layer != firlayer
                    || profile != firprofile
                    || sampRateIdx != firsampRateIdx
                    || channelConfig != firchannelConfig)
                    continue;

                frameLength = ((cdx_int32)(data[i+3] & 0x3) << 11)
                    | ((cdx_int32)(data[i+4] & 0xff) << 3)
                    |((data[i+5] >> 5) & 0x07);

                if (layer != 0  ||
                    sampRateIdx >= 12 || channelConfig >= 8)
                {
                    continue;
                }
                //if frameLength == 0, then i will decreased by 1 here,
                //and inceased by 1 in for statement, this is a dead loop.
                if(frameLength >= 1) {
                    i += (frameLength - 1);
                }
                frameOn++;
            }
        }

    }

    if(frameOn > 0)
        BitRate = (cdx_int32)((datalen*8*firSampleRate)/(frameOn*1024));

    if(BitRate > 0)
        Duration = datalen * 8000 / BitRate;


    return Duration;
}

static cdx_uint32 bytestream_get_be16(cdx_uint8** pptr)
{
    cdx_uint32 value = 0;
    cdx_uint8* ptr = NULL;

    ptr = *pptr;

    value = ptr[0]<<8 | ptr[1];
    ptr += 2;

    *pptr = ptr;
    return value;
}

static cdx_int32 DtsProbe(cdx_char *buf, cdx_int32 len)
{
    cdx_char *d,*dp;
    d = buf;
    cdx_int32 markers[3] = {0};
    cdx_int32 sum, max;
    cdx_uint32 state = -1;
    for(; d < (buf+len)-2; d+=2)
    {
        dp = d;
        state = (state << 16) | bytestream_get_be16((cdx_uint8**)&dp);

        /* regular bitstream */
        if (state == DCA_MARKER_RAW_BE || state == DCA_MARKER_RAW_LE)
            markers[0]++;

        /* 14 bits big-endian bitstream */
        if (state == DCA_MARKER_14B_BE)
            if ((bytestream_get_be16((cdx_uint8**)&dp) & 0xFFF0) == 0x07F0)
                markers[1]++;

        /* 14 bits little-endian bitstream */
        if (state == DCA_MARKER_14B_LE)
            if ((bytestream_get_be16((cdx_uint8**)&dp) & 0xF0FF) == 0xF007)
                markers[2]++;
    }
    sum = markers[0] + markers[1] + markers[2];
    max = markers[1] > markers[0];
    max = markers[2] > markers[max] ? 2 : max;
    if (markers[max] > 3 && len / markers[max] < 32*1024 &&
        markers[max] * 4 > sum * 3)
    {
        CDX_LOGD("It's DTS");
        return CDX_TRUE;
    }
    else if(sum)
    {
        CDX_LOGD("It's DTS");
        return CDX_TRUE;
    }
    return CDX_FALSE;
}

static cdx_uint32 probe_dts(cdx_char *buf, cdx_int32 len)
{
    if(len < 4)
    {
        CDX_LOGE("Probe DTS_header data is not enough.");
        return CDX_FALSE;
    }
    if(len >= 8 && buf != NULL && !memcmp(buf, "DTSHDHDR", 8))
    {
        return CDX_TRUE;
    }
    if(!DtsProbe(buf, len))
    {
        CDX_LOGE("dts probe failed.");
        return CDX_FALSE;
    }
    return CDX_TRUE;
}

static cdx_uint32 probe_ac3(cdx_char *buf, cdx_int32 len)
{
    /*ToDo.*/
    CDX_UNUSE(buf);
    CDX_UNUSE(len);
    return CDX_FALSE;
}

cdx_uint32 probe_input_format(cdx_char *buf, cdx_int32 len)
{
    if(probe_dts(buf, len))
        return AUDIO_CODEC_FORMAT_DTS;
    else if(probe_ac3(buf, len))
        return AUDIO_CODEC_FORMAT_AC3;
    else
        return AUDIO_CODEC_FORMAT_UNKNOWN;
}

#if (PROBE_STREAM && !DVB_TEST)

#define min(x, y)   ((x) <= (y) ? (x) : (y));

static cdx_uint8 getbits8(cdx_uint8* buffer, cdx_uint32 start, cdx_uint8 len)
{
    cdx_uint32 i = 0;
    cdx_uint8  n = 0;
    cdx_uint8  w = 0;
    cdx_uint8  k = 0;
    cdx_uint8  ret = 0;

    n = start % 8;
    i = start / 8;
    w = 8 - n;
    k = (len > w ? len - w : 0);

    ret = (buffer[i] << n);
    if(8 > len)
        ret  >>= (8-len);
    if(k)
        ret |= (buffer[i+1] >> (8-k));

    return  ret;
}

static __inline cdx_uint32 getbits16(cdx_uint8* buffer, cdx_uint32 start, cdx_uint8 length)
{
    if(length > 8)
        return (getbits8(buffer, start, length - 8) << 8)
        | getbits8(buffer, start + length - 8, 8);
    else
        return getbits8(buffer, start, length);
}

static cdx_uint32 cdx_read_golomb(cdx_uint8* buffer, cdx_uint32* init)
{
    cdx_uint32 y, w = 0, w2 = 0, k, len = 0, m = *init;

    while(getbits8(buffer, m++, 1) == 0)
        len++;

    y = len + m;
    while(m < y)
    {
        k = min(y - m, 8);
        w |= getbits8(buffer, m, k);
        m += k;
        if(y - m > 8)
            w <<= 8;
    }

    w2 = 1;
    for(m = 0; m < len; m++)
        w2 <<= 1;
    w2 = (w2 - 1) + w;

    //fprintf(stderr, "READ_GOLOMB(%u), V=2^%u + %u-1 = %u\n", *init, len, w, w2);
    *init = y;
    return w2;
}


static __inline cdx_int32 ReadGolomb(cdx_uint8* buffer, cdx_uint32* init)
{
    cdx_uint32 w = cdx_read_golomb(buffer, init);
    return (w & 1) ? ((w + 1) >> 1) : -(w >> 1);
}

//* video probe functions
cdx_int32 prob_mpg(Stream *st)
{
    cdx_uint32 code;
    cdx_uint32 tmp;
    cdx_uint8* ptr;

    cdx_int32 frame_rate_table[16] =
        {0, 23976, 24000, 25000, 29970, 30000, 50000, 59940, 60000, 0, 0, 0, 0, 0, 0, 0};

    code = 0xffffffff;

    for(ptr = st->probeBuf; ptr <= st->probeBuf + st->probeDataSize - 6;)
    {
        code = code<<8 | *ptr++;
        if (code == 0x01b3) //* sequence header
        {
            tmp = ptr[0]<<24 | ptr[1]<<16 | ptr[2]<<8 | ptr[3];
            if(!st->metadata)
            {
                st->metadata = (VideoMetaData *)malloc(sizeof(VideoMetaData));
                memset(st->metadata, 0, sizeof(VideoMetaData));
            }
            else
            {
                CDX_LOGW("may be created yet.");
            }
            VideoMetaData *videoMetaData = (VideoMetaData *)st->metadata;
            videoMetaData->width = (tmp >> 20);
            videoMetaData->height = (tmp >> 8) & 0xfff;
            videoMetaData->frameRate = frame_rate_table[tmp & 0xf];
            videoMetaData->bitRate = (ptr[4]<<10 | ptr[5]<<2 | ptr[6]>>6) * 400;
            break;
        }
    }
    return 0;
}
cdx_int32 prob_mpg4(Stream *st)
{
    (void)st;
    return 0;
}
static cdx_int32 h264_parse_vui(VideoMetaData *videoMetaData, cdx_uint8* buf, cdx_uint32 n)
{
    cdx_uint32 chroma, timing, fixed_fps, overscan, vsp_color;
    cdx_uint32 aspect_ratio_information, timeinc_unit, timeinc_resolution;
    cdx_uint32 width, height;

    timeinc_unit = 0;
    timeinc_resolution = 0;

    if(getbits8(buf, n++, 1))
    {
        aspect_ratio_information = getbits8(buf, n, 8);
        n += 8;
        if(aspect_ratio_information == 255)
        {
            width = (getbits8(buf, n, 8) << 8) | getbits8(buf, n + 8, 8);
            n += 16;

            height = (getbits8(buf, n, 8) << 8) | getbits8(buf, n + 8, 8);
            n += 16;
        }
    }

    overscan = getbits8(buf, n++, 1);
    if(overscan)
        n++;

    vsp_color = getbits8(buf, n++, 1);
    if(vsp_color)
    {
        n += 4;
        if(getbits8(buf, n++, 1))
            n += 24;
    }

    chroma=getbits8(buf, n++, 1);
    if(chroma)
    {
        cdx_read_golomb(buf, &n);
        cdx_read_golomb(buf, &n);
    }

    timing=getbits8(buf, n++, 1);
    if(timing)
    {
        timeinc_unit = (getbits8(buf, n, 8) << 24)
            | (getbits8(buf, n+8, 8) << 16)
            | (getbits8(buf, n+16, 8) << 8)
            | getbits8(buf, n+24, 8);
        n += 32;

        timeinc_resolution = (getbits8(buf, n, 8) << 24)
            | (getbits8(buf, n+8, 8) << 16)
            | (getbits8(buf, n+16, 8) << 8)
            | getbits8(buf, n+24, 8);
        n += 32;

        fixed_fps = getbits8(buf, n, 1);

        if(timeinc_unit > 0 && timeinc_resolution > 0)
            videoMetaData->frameRate = timeinc_resolution * 1000 / timeinc_unit;

        if(fixed_fps)
            videoMetaData->frameRate /= 2;
    }

    return n;
}

static cdx_int32 h264_parse_sps(VideoMetaData *videoMetaData,
    cdx_uint8* buffer, cdx_int32 len)
{
    cdx_uint32 n = 0;
    cdx_uint32 i = 0;
    cdx_uint32 j = 0;
    cdx_uint32 k = 0;
    cdx_uint32 mbh = 0;
    cdx_int32 bFrame_mbs_only = 0;

    (void)len;

    n = 24;
    cdx_read_golomb(buffer, &n);
    if(buffer[0] >= 100)
    {
        if(cdx_read_golomb(buffer, &n) == 3)
            n++;
        cdx_read_golomb(buffer, &n);
        cdx_read_golomb(buffer, &n);
        n++;
        if(getbits8(buffer, n++, 1))
        {
            for(i = 0; i < 8; i++)
            {
                // scaling list is skipped for now
                if(getbits8(buffer, n++, 1))
                {
                    j = 8;
                    for(k = (i < 6 ? 16 : 64); k && j ; k--)
                        j = (ReadGolomb(buffer, &n) + j) & 255;
                }
            }
        }
    }

    cdx_read_golomb(buffer, &n);
    j = cdx_read_golomb(buffer, &n);
    if(j == 0)
        cdx_read_golomb(buffer, &n);
    else if(j == 1)
    {
        getbits8(buffer, n++, 1);
        cdx_read_golomb(buffer, &n);
        cdx_read_golomb(buffer, &n);
        j = cdx_read_golomb(buffer, &n);
        for(i = 0; i < j; i++)
            cdx_read_golomb(buffer, &n);
    }

    cdx_read_golomb(buffer, &n);
    getbits8(buffer, n++, 1);
    videoMetaData->width = 16 *(cdx_read_golomb(buffer, &n)+1);
    mbh = cdx_read_golomb(buffer, &n)+1;
    bFrame_mbs_only = getbits8(buffer, n++, 1);
    videoMetaData->height = 16 * (2 - bFrame_mbs_only) * mbh;
    if(!bFrame_mbs_only)
        getbits8(buffer, n++, 1);
    getbits8(buffer, n++, 1);
    if(getbits8(buffer, n++, 1))
    {
        cdx_read_golomb(buffer, &n);
        cdx_read_golomb(buffer, &n);
        cdx_read_golomb(buffer, &n);
        cdx_read_golomb(buffer, &n);
    }
    if(getbits8(buffer, n++, 1))
        n = h264_parse_vui(videoMetaData, buffer, n);

    return n;
}

cdx_int32 prob_mvc(Stream *st)
{
    cdx_uint32       code;
    cdx_uint8*       ptr;
    code = 0xffffffff;

    for (ptr = st->probeBuf; ptr <= st->probeBuf + st->probeDataSize - 16;)
    {
        code = 0x1f &(*ptr++);
        if (code == 0xf) //* sequence header
        {
            if(!st->metadata)
            {
                st->metadata = (VideoMetaData *)malloc(sizeof(VideoMetaData));
                memset(st->metadata, 0, sizeof(VideoMetaData));
            }
            else
            {
                CDX_LOGW("may be created yet.");
            }
            VideoMetaData *videoMetaData = (VideoMetaData *)st->metadata;
            h264_parse_sps(videoMetaData, ptr, st->probeBuf + st->probeDataSize - ptr);
            break;
        }
    }
    return 0;
}

cdx_int32 prob_h264(Stream *st)
{
    cdx_uint32       code;
    cdx_uint8*       ptr;

    code = 0xffffffff;

    for (ptr = st->probeBuf; ptr <= st->probeBuf + st->probeDataSize - 16;)
    {
        code = code<<8 | *ptr++;
        if (code == 0x0167
            || code == 0x0147
            || code == 0x0127
            || code == 0x0107)
            //* sequence header
        {
            if(!st->metadata)
            {
                st->metadata = (VideoMetaData *)malloc(sizeof(VideoMetaData));
                memset(st->metadata, 0, sizeof(VideoMetaData));
            }
            else
            {
                CDX_LOGW("may be created yet.");
            }
            VideoMetaData *videoMetaData = (VideoMetaData *)st->metadata;
            h264_parse_sps(videoMetaData, ptr, st->probeBuf + st->probeDataSize - ptr);
            break;
        }
    }
    return 0;
}

cdx_int32 vc1_decode_sequence_header(VideoMetaData *videoMetaData,
    cdx_uint8* buf, cdx_int32 len)
{
    cdx_int32 m, y;
    (void)len;

    m = 0;
    y = getbits8(buf, m, 2);
    m += 2;
    if(y != 3) //not advanced profile
        return 0;

    getbits16(buf, m, 14);
    m += 14;
    videoMetaData->width = getbits16(buf, m, 12) * 2 + 2;
    m += 12;
    videoMetaData->height = getbits16(buf, m, 12) * 2 + 2;
    m += 12;
    getbits8(buf, m, 6);
    m += 6;
    y = getbits8(buf, m, 1);
    m += 1;
    if(y) //display info
    {
        getbits16(buf, m, 14);
        m += 14;
        getbits16(buf, m, 14);
        m += 14;
        if(getbits8(buf, m++, 1)) //aspect ratio
        {
            y = getbits8(buf, m, 4);
            m += 4;
            if(y == 15)
            {
                getbits16(buf, m, 16);
                m += 16;
            }
        }

        if(getbits8(buf, m++, 1)) //framerates
        {
            cdx_int32 frexp=0, frnum=0, frden=0;

            if(getbits8(buf, m++, 1))
            {
                frexp = getbits16(buf, m, 16);
                m += 16;
                videoMetaData->frameRate = (frexp+1)*1000 / 32.0;
            }
            else
            {
                cdx_uint32 frates[] = {0, 24000, 25000, 30000, 50000, 60000, 48000, 72000, 0};
                cdx_uint32 frdivs[] = {0, 1000, 1001, 0};

                frnum = getbits8(buf, m, 8);
                m += 8;
                frden = getbits8(buf, m, 4);
                m += 4;
                if((frden == 1 || frden == 2) && (frnum < 8))
                    videoMetaData->frameRate = frates[frnum]*1000 / frdivs[frden];
            }
        }
    }

    return 1;
}

cdx_int32 prob_vc1(Stream *st)
{
    cdx_uint32       code;
    cdx_uint8*       ptr;

    code = 0xffffffff;

    for (ptr = st->probeBuf; ptr <= st->probeBuf + st->probeDataSize - 16;)
    {
        code = code<<8 | *ptr++;
        if (code == 0x010f) //* sequence header
        {
            if(!st->metadata)
            {
                st->metadata = (VideoMetaData *)malloc(sizeof(VideoMetaData));
                memset(st->metadata, 0, sizeof(VideoMetaData));
            }
            else
            {
                CDX_LOGW("may be created yet.");
            }
            VideoMetaData *videoMetaData = (VideoMetaData *)st->metadata;
            vc1_decode_sequence_header(videoMetaData,
                ptr, st->probeBuf + st->probeDataSize - ptr);
            break;
        }
    }
    return 0;
}
static cdx_uint32 h265_read_golomb(cdx_uint8* buffer, cdx_uint32* init)
{
    cdx_uint32 y, w = 0, w2 = 0, k, length = 0, j = *init;

    while(getbits8(buffer, j++, 1) == 0)
        length++;

    y = length + j;

    while(j < y)
    {
        k = min(y - j, 8); /* getbits8() function, number of bits should not greater than 8 */
        w |= getbits8(buffer, j, k);
        j += k;
        if(y - j > 8)
            w <<= 8;
        else if((y - j) > 0)
            w <<= (y - j);
    }

    w2 = 1;
    for(j = 0; j < length; j++)
        w2 <<= 1;
    w2 = (w2 - 1) + w;

    *init = y;
    return w2;
}

static cdx_int32 h265_parse_sps_profile_tier_level(cdx_uint8* buf,
                                cdx_uint32 *len, cdx_uint32 sps_max_sub_layers_minus1)
{
    cdx_uint32 n = *len;
    cdx_uint8 sub_layer_profile_present_flag[64] = { 0 };
    cdx_uint8 sub_layer_level_present_flag[64] = { 0 };
    cdx_uint32 i, j;

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
        sub_layer_profile_present_flag[i] = getbits8(buf, n++, 1); /* u(1) */
        sub_layer_level_present_flag[i] = getbits8(buf, n++, 1); /* u(1) */
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

static cdx_int32 h265_parse_sps(VideoMetaData *videoMetaData,
    cdx_uint8* buf, cdx_int32 len)
{
    cdx_uint32 n = 0;
    cdx_uint32 sps_max_sub_layers_minus1 = 0;
    cdx_uint32 chroma_format_idc = 0;
    //cdx_uint32 pic_width = 0;
    //cdx_uint32 pic_height = 0;

    (void)len;

    n += 1;  /* forbidden_zero_bit: u(1)*/
    n += 6; /* nal_unit_type: u(6) */
    n += 6; /* nuh_layer_id: u(6) */
    n += 3; /* nuh_temporal_id_plus1: u(3) */

    n += 4;  /* sps_video_parameter_set_id: u(4) */
    sps_max_sub_layers_minus1 = getbits8(buf, n, 3);
    n += 3;  /* sps_max_sub_layers_minus1:  u(3) */
    n += 1;  /* sps_temporal_id_nesting_flag: u(1) */
    h265_parse_sps_profile_tier_level(buf, &n, sps_max_sub_layers_minus1);
    h265_read_golomb(buf, &n); /* sps_seq_parameter_set_id: ue(v) */
    chroma_format_idc = h265_read_golomb(buf, &n); /* chroma_format_idc: ue(v) */
    if(chroma_format_idc == 3)
        n += 1; /* separate_colour_plane_flag: u(1) */

    videoMetaData->width = h265_read_golomb(buf, &n); /* pic_width_in_luma_samples: ue(v) */
    videoMetaData->height = h265_read_golomb(buf, &n); /* pic_height_in_luma_samples: ue(v) */
    CDX_LOGD("zwh h265 parser prob pic width: %d, pic height: %d",
        videoMetaData->width, videoMetaData->height);
    return 0;
}
static cdx_int32 prob_h265_delete_emulation_code(cdx_uint8 *buf_out,
    cdx_uint8 *buf, cdx_int32 len)
{
    cdx_int32 i, size, skipped;
    cdx_int32 temp_value = -1;
    const cdx_uint32 mask = 0xFFFFFF;

    size = len;
    skipped = 0;
    for(i = 0; i < size; i++)
    {
        temp_value =(temp_value << 8)| buf[i];
        switch(temp_value & mask)
        {
        case 0x000003:
            skipped += 1;
            break;
        default:
            buf_out[i - skipped] = buf[i];
        }
    }

    return (size - skipped);
}

static cdx_int32 prob_h265(Stream *st)
{
    cdx_uint8 *ptr, *ptr_nalu;
    cdx_uint8 *nalu;
    cdx_int32 kk = 0, sps_nalu_len = 0;;
    cdx_int32 found = 0;

    for (ptr = st->probeBuf; ptr <= st->probeBuf + st->probeDataSize - 16;)
    {
        if(ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 1 && /*h265 nalu start code*/
                ptr[3] == 0x42 && ptr[4] == 0x01 /*h265 sps nalu type*/)
        {
            ptr += 3;
            ptr_nalu = ptr;
            found = 1;
            break; /* find sps, next we will get the size of sps_nalu by searching next start_code*/
        }
        ++kk;
        ++ptr;
    }
    kk = 0;
    if(found == 1)
    {
        for (; ptr <= st->probeBuf + st->probeDataSize - 16;)
        {
            if(ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 1)
            {
                   nalu = calloc(kk+16, 1);
                sps_nalu_len = prob_h265_delete_emulation_code(nalu, ptr_nalu, kk);
                if(!st->metadata)
                {
                    st->metadata = (VideoMetaData *)malloc(sizeof(VideoMetaData));
                    memset(st->metadata, 0, sizeof(VideoMetaData));
                }
                else
                {
                    CDX_LOGW("may be created yet.");
                }
                VideoMetaData *videoMetaData = (VideoMetaData *)st->metadata;

                h265_parse_sps(videoMetaData, nalu, sps_nalu_len);
                free(nalu);
                return 0;
            }
            ++kk;
            ++ptr;
        }
    }
    return 0;
}

cdx_int32 ProbeVideo(Stream *stream)
{
    if (stream->codec_id == VIDEO_CODEC_FORMAT_MPEG1
        || stream->codec_id == VIDEO_CODEC_FORMAT_MPEG2)
    {
        return prob_mpg(stream);
    }
    else if (stream->codec_id == VIDEO_CODEC_FORMAT_MPEG4)
    {
        return prob_mpg4(stream);
    }
    else if (stream->codec_id == VIDEO_CODEC_FORMAT_H264)
    {
        return prob_h264(stream);
    }
    else if (stream->codec_id == VIDEO_CODEC_FORMAT_H265)
    {
        return prob_h265(stream);
    }
    else if (stream->codec_id == VIDEO_CODEC_FORMAT_WMV3)
    {
        return prob_vc1(stream);
    }
    else if (stream->codec_id == VIDEO_CODEC_FORMAT_AVS)
    {
        return 0;
    }
    else
    {
        CDX_LOGE("should not be here.");
        return -1;
    }
}

cdx_int32 ProbeAudio(Stream *stream)
{
    (void)stream;
    return 0;
}

cdx_int32 ProbeStream(Stream *stream)
{
    if(stream->mMediaType == TYPE_VIDEO)
    {
        return ProbeVideo(stream);
    }
    else if(stream->mMediaType == TYPE_AUDIO)
    {
        return ProbeAudio(stream);
    }
    else
    {
        CDX_LOGE("should not be here.");
        return -1;
    }
}
#endif

