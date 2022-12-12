/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : AsfParser.c
 * Description : AsfParser
 * Author  : xuqi <xuqi@allwinnertech.com>
 * Comment : 创建初始版本
 *
 */

#include <cdx_log.h>
#include <CdxBinary.h>
#include <CdxDebug.h>
#include <string.h>

#include <CdxStream.h>
#include <CdxParser.h>

#include "AsfParser.h"

#define FRAME_HEADER_SIZE 17
#define ABS_EDIAN_FLAG_LITTLE       ((cdx_uint32)(0<<16))

#define PRINT_IF_GUID(guid,cmp) \
    if (!memcmp(guid, &cmp, sizeof(AsfGuid))) \
CDX_LOGD("(AsfGuid: %s) ", #cmp)

#define DO_2BITS(nBits, nVar, nDefval) \
        switch (nBits & 3) \
        { \
            case 3: nVar = CdxStreamGetLE32(ioStream); nRsize += 4; break; \
            case 2: nVar = CdxStreamGetLE16(ioStream); nRsize += 2; break; \
            case 1: nVar = CdxStreamGetU8(ioStream); nRsize++; break; \
            default: nVar = nDefval; break; \
        }

typedef struct ra_format_info_struct
{
    cdx_uint32 ulSampleRate;
    cdx_uint32 ulActualRate;
    cdx_uint16 usBitsPerSample;
    cdx_uint16 usNumChannels;
    cdx_uint16 usAudioQuality;
    cdx_uint16 usFlavorIndex;
    cdx_uint32 ulBitsPerFrame;
    cdx_uint32 ulGranularity;
    cdx_uint32 ulOpaqueDataSize;
    cdx_uint8*  pOpaqueData;
} ra_format_info;

static void AsfPrintGuid(const AsfGuid *guid)
{
    PRINT_IF_GUID(guid, asf_header);
    else PRINT_IF_GUID(guid, file_header);
    else PRINT_IF_GUID(guid, stream_header);
    else PRINT_IF_GUID(guid, audio_stream);
    else PRINT_IF_GUID(guid, audio_conceal_none);
    else PRINT_IF_GUID(guid, video_stream);
    else PRINT_IF_GUID(guid, video_conceal_none);
    else PRINT_IF_GUID(guid, command_stream);
    else PRINT_IF_GUID(guid, comment_header);
    else PRINT_IF_GUID(guid, codec_comment_header);
    else PRINT_IF_GUID(guid, data_header);
    else PRINT_IF_GUID(guid, index_guid);
    else PRINT_IF_GUID(guid, head1_guid);
    else PRINT_IF_GUID(guid, ext_stream_header);
    else PRINT_IF_GUID(guid, extended_content_header);
    else PRINT_IF_GUID(guid, ext_stream_embed_stream_header);
    else PRINT_IF_GUID(guid, ext_stream_audio_stream);
    else PRINT_IF_GUID(guid, metadata_header);
    else PRINT_IF_GUID(guid, stream_bitrate_guid);
    else
        CDX_LOGD("(AsfGuid: unknown) ");
    //for(i=0;i<16;i++)
    //    CDX_LOGD(" 0x%02x,", (*guid)[i]);
    CDX_LOGD("}\n");
}
#undef PRINT_IF_GUID

static void PrintCurPkt(AsfContextT *ctx)
{
    (void)ctx;
    CDX_LOGI("(%lld-%d), (0x%x, 0x%x, %u, %d, %d, %d, %d)",
        ctx->packet_pos, ctx->packet_hdr_size,
        ctx->packet_flags, ctx->packet_property,
        ctx->packet_size, ctx->packet_seq, ctx->packet_padsize,
        ctx->packet_timestamp, ctx->packet_segments);
}

static inline void AsfGetGuid(CdxStreamT *s, AsfGuid *guid)
{
    CdxStreamRead(s, *guid, sizeof(*guid));
}

static cdx_int32 AsfGetValue(CdxStreamT *ioStream, cdx_int32 type)
{
    switch (type)
    {
    case 2: return CdxStreamGetLE32(ioStream);
    case 3: return CdxStreamGetLE32(ioStream);
    case 4: return (cdx_int32)CdxStreamGetLE64(ioStream);
    case 5: return CdxStreamGetLE16(ioStream);
    default: return -1;
    }
}

static cdx_int32 PsrDataAudioStream(cdx_uint8 *data, int dataSize, AsfMediaStreamT *avStream)
{
    cdx_int32 id;
    int dataPos = 0;
    struct AsfCodecS *codec = &avStream->codec;
    int ret = 0;

    CDX_CHECK(dataSize >= 18);

    id = GetLE16(data + dataPos); dataPos += 2;
    codec->codec_type = CDX_MEDIA_AUDIO;
    codec->codec_tag = id;
    codec->channels = GetLE16(data + dataPos); dataPos += 2;
    codec->sample_rate = GetLE32(data + dataPos); dataPos += 4;
    codec->bit_rate = GetLE32(data + dataPos) * 8; dataPos += 4;
    codec->block_align = GetLE16(data + dataPos); dataPos += 2;
    codec->bits_per_sample = GetLE16(data + dataPos); dataPos += 2;

    codec->extradata_size = GetLE16(data + dataPos); dataPos += 2;

    if (codec->extradata_size)
    {
        codec->extradata = malloc(codec->extradata_size + ASF_INPUT_BUFFER_PADDING_SIZE);
        memcpy(codec->extradata, data + dataPos, codec->extradata_size);
             dataPos += codec->extradata_size;
    }

    switch (id)
    {
        case 0x130:
        {
            if(codec->extradata != NULL)
            {
                free(codec->extradata);
                codec->extradata = NULL;
            }
            ra_format_info p_audio_fmt_info = {0};
            p_audio_fmt_info.ulSampleRate = codec->sample_rate;
            p_audio_fmt_info.usBitsPerSample = codec->bits_per_sample;
            p_audio_fmt_info.usNumChannels = codec->channels;
            p_audio_fmt_info.usAudioQuality = 100;
            p_audio_fmt_info.pOpaqueData = NULL;
            /*skip first 4 byte*/
            codec->extradata = malloc(sizeof(ra_format_info));
            memcpy(codec->extradata + 4,&p_audio_fmt_info,sizeof(ra_format_info) - 4);

            codec->extradata_size = sizeof(ra_format_info);
            CDX_LOGD("codec->extradata_size : %d", codec->extradata_size);
            CDX_LOGD("p_audio_fmt_info.ulSampleRate: %d",p_audio_fmt_info.ulSampleRate);
            codec->codec_id = AUDIO_CODEC_FORMAT_SIPR;
            /*abandon pOpauqeData*/
            break;
        }
        case 0x160:
        case 0x161:
        case 0x162:
        {
            codec->codec_id = AUDIO_CODEC_FORMAT_WMA_STANDARD;
            break;
        }
        case 0x163:
            codec->codec_id = AUDIO_CODEC_FORMAT_WMA_LOSS;
            break;
        case 0x00ff:
            codec->codec_id = AUDIO_CODEC_FORMAT_RAAC;
            break;
        case 0x45:
            codec->codec_id = AUDIO_CODEC_FORMAT_ADPCM;
            codec->subCodecId = ADPCM_CODEC_ID_G726 | ABS_EDIAN_FLAG_LITTLE;
//            codec->extradata_size = codec->block_align;
            break;
        case 0x2:
            codec->codec_id = AUDIO_CODEC_FORMAT_ADPCM;
            codec->subCodecId = ADPCM_CODEC_ID_MS | ABS_EDIAN_FLAG_LITTLE;
//            codec->extradata_size = codec->block_align;
            break;
        case 0x6:
            codec->codec_id = AUDIO_CODEC_FORMAT_PCM;
            codec->subCodecId = WAVE_FORMAT_ALAW | ABS_EDIAN_FLAG_LITTLE;
            codec->extradata_size = codec->block_align; // TODO: why do this ??
            break;
        case 0x7:
            codec->codec_id = AUDIO_CODEC_FORMAT_PCM;
            codec->subCodecId = WAVE_FORMAT_MULAW | ABS_EDIAN_FLAG_LITTLE;
            codec->extradata_size = codec->block_align; // TODO: why do this ??
            break;
        case 0x11://CODEC_ID_ADPCM_IMA_WAV
            codec->codec_id = AUDIO_CODEC_FORMAT_ADPCM;
            codec->subCodecId = ADPCM_CODEC_ID_IMA_WAV | ABS_EDIAN_FLAG_LITTLE;
//            codec->extradata_size = codec->block_align;
            break;
        case 0x20:
            codec->codec_id = AUDIO_CODEC_FORMAT_ADPCM;
            codec->subCodecId = ADPCM_CODEC_ID_YAMAHA | ABS_EDIAN_FLAG_LITTLE;
//            codec->extradata_size = codec->block_align;
            break;
        case 0x50:
        case 0x55:
        case MKTAG('L', 'A', 'M', 'E'):
        case MKTAG('l', 'a', 'm', 'e'):
        case MKTAG('M', 'P', '3', ' '):
        case MKTAG('m', 'p', '3', ' '):
            codec->codec_id = AUDIO_CODEC_FORMAT_MP3;
            break;
        case 0x2000:
            codec->codec_id = AUDIO_CODEC_FORMAT_AC3;
            break;
        case 0x2001:
            codec->codec_id = AUDIO_CODEC_FORMAT_DTS;
            break;
        default:
            codec->codec_id = AUDIO_CODEC_FORMAT_UNKNOWN;
            return -1;//delete it maybe
            //break;
    }
    //get_wav_header end

    /* We have to init the frame size at some point .... */
    if (dataPos <= dataSize - 8)
    {
        avStream->ds_span = GetU8(data + dataPos); dataPos += 1;
        avStream->ds_packet_size = GetLE16(data + dataPos); dataPos += 2;
        avStream->ds_chunk_size = GetLE16(data + dataPos); dataPos += 2;
        dataPos += 2; //ds_data_size
        dataPos += 1; //ds_silence_data
    }

    if (avStream->ds_span > 1)
    {
        if (!avStream->ds_chunk_size
            || (avStream->ds_packet_size / avStream->ds_chunk_size <= 1)
            || (avStream->ds_packet_size % avStream->ds_chunk_size))
        {
            avStream->ds_span = 0; // disable descrambling
        }
    }

    return ret;
}

static cdx_int32 PsrDataVideoStream(cdx_uint8 *data, cdx_int32 dataLen, AsfMediaStreamT *avStream)
{
    CDX_UNUSE(dataLen);
    int formatDataSize = 0;
    int dataPos = 0;

    dataPos += 4; /*Encoded Image Width*/
    dataPos += 4; /*Encoded Image Height*/
    dataPos += 1; /*Reserved Flags*/

    dataPos += 2; /*Format Data Size*/

    formatDataSize = GetLE32(data + dataPos); dataPos += 4; /* Format Data Size */

    avStream->codec.width = GetLE32(data + dataPos); dataPos += 4;
    avStream->codec.height = GetLE32(data + dataPos); dataPos += 4;

    /* not available for asf */
    dataPos += 2; /* Reserved */
    avStream->codec.bits_per_sample = GetLE16(data + dataPos); dataPos += 2;
   /* Bits Per Pixel Count */
    avStream->codec.codec_tag = GetLE32(data + dataPos); dataPos += 4;

    dataPos += 20;  /*Image Size */
                    /*Horizontal Pixels Per Meter*/
                    /*Vertical Pixels Per Meter*/
                    /*Colors Used Count*/
                    /*Important Colors Count*/
    if (formatDataSize > 40)
    {
        avStream->codec.extradata_size = formatDataSize - 40;
        avStream->codec.extradata = malloc(avStream->codec.extradata_size);
        memcpy(avStream->codec.extradata, data + dataPos, avStream->codec.extradata_size);
             dataPos += avStream->codec.extradata_size;
    }
    CDX_BUF_DUMP(avStream->codec.extradata, avStream->codec.extradata_size);
    switch (avStream->codec.codec_tag)
    {
    case MKTAG('w','m','v','3'):
    case MKTAG('W','M','V','3'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_WMV3;

        avStream->codec.extradata_size = 4;
        break;

    case MKTAG('W', 'M', 'V', '1'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_WMV1;
        break;

    case MKTAG('W', 'M', 'V', '2'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_WMV2;

        avStream->codec.extradata_size = 4;
        break;

    case MKTAG('X', 'V', 'I', 'D'):
    case MKTAG('F', 'M', 'P', '4'):
    case MKTAG('D', 'I', 'V', 'X'):
    case MKTAG('D', 'X', '5', '0'):
    case MKTAG('M', '4', 'S', '2'):
    case MKTAG( 4 ,  0 ,  0 ,  0 ):
    case MKTAG('D', 'I', 'V', '1'):
    case MKTAG('B', 'L', 'Z', '0'):
    case MKTAG('m', 'p', '4', 'v'):
    case MKTAG('U', 'M', 'P', '4'):
    case MKTAG('W', 'V', '1', 'F'):
    case MKTAG('S', 'E', 'D', 'G'):
    case MKTAG('R', 'M', 'P', '4'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_XVID;
// TODO: why cut 10 bytes???
/*
        if (avStream->codec.extradata_size > 10)
        {
            cdx_uint8 *tmpExtraData = malloc(avStream->codec.extradata_size - 10);

            avStream->codec.extradata_size -= 10;
            memcpy(tmpExtraData, avStream->codec.extradata + 10,
                    avStream->codec.extradata_size);
            free(avStream->codec.extradata);
            avStream->codec.extradata = tmpExtraData;
        }
*/
        break;
    case MKTAG('M', 'P', '4', 'S'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_XVID;
        break;

   //MSMPEGV2
    case MKTAG('D', 'I', 'V', '2'):
    case MKTAG('d', 'i', 'v', '2'):
    case MKTAG('M', 'P', '4', '2'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_MSMPEG4V2;
        break;

    case MKTAG('D', 'I', 'V', '3'):
    case MKTAG('M', 'P', '4', '3'):
    case MKTAG('M', 'P', 'G', '3'):
    case MKTAG('D', 'I', 'V', '5'):
    case MKTAG('D', 'I', 'V', '6'):
    case MKTAG('D', 'I', 'V', '4'):
    case MKTAG('A', 'P', '4', '1'):
    case MKTAG('C', 'O', 'L', '1'):
    case MKTAG('C', 'O', 'L', '0'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_DIVX3;
        break;

    case MKTAG('H', '2', '6', '3'):
    case MKTAG('I', '2', '6', '3'):
    case MKTAG('U', '2', '6', '3'):
    case MKTAG('v', 'i', 'v', '1'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_H263;
        break;

    case MKTAG('H', '2', '6', '4'):
    case MKTAG('h', '2', '6', '4'):
    case MKTAG('X', '2', '6', '4'):
    case MKTAG('x', '2', '6', '4'):
    case MKTAG('a', 'v', 'c', '1'):
    case MKTAG('V', 'S', 'S', 'H'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_H264;
        break;

    case MKTAG( 2 ,  0 ,  0 ,  16):
    case MKTAG('D', 'V', 'R', ' '):
    case MKTAG('M', 'M', 'E', 'S'):
    case MKTAG('m', 'p', 'g', '2'):
    case MKTAG('M', 'P', 'E', 'G'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_MPEG2;
        break;

    case MKTAG('m', 'p', 'g', '1'):
    case MKTAG('P', 'I', 'M', '1'):
    case MKTAG('V', 'C', 'R', '2'):
    case MKTAG( 1 ,  0 ,  0 ,  16):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_MPEG1;
        break;

    case MKTAG('M', 'J', 'P', 'G'):
    case MKTAG('L', 'J', 'P', 'G'):
    case MKTAG('J', 'P', 'G', 'L'):
    case MKTAG('M', 'J', 'L', 'S'):
    case MKTAG('j', 'p', 'e', 'g'):
    case MKTAG('I', 'J', 'P', 'G'):
    case MKTAG('A', 'V', 'R', 'n'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_MJPEG;
        break;

    case MKTAG('W', 'V', 'C', '1'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_WMV3;
        break;
    case MKTAG('W', 'M', 'V', 'A'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_WMV1;
        break;

    case MKTAG('M', 'P', 'G', '4'):
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_MSMPEG4V1;
        break;

    default:
        avStream->codec.codec_id = VIDEO_CODEC_FORMAT_UNKNOWN;
        break;
    }

    if (avStream->codec.codec_id == VIDEO_CODEC_FORMAT_UNKNOWN)
    {
        return -1;
    }
    return 0;
}

static cdx_int32 PsrObjStream(AsfContextT *ctx, cdx_int64 objSize)
{
    CdxStreamT *ioStream = ctx->ioStream;
    AsfMediaStreamT *avStream = NULL;
    AsfGuid guid;
    int ret = 0;
    enum CdxMediaTypeE type;
    cdx_int32 type_specific_size;
    cdx_int64 pos1, pos2;
    cdx_uint8 *specificData = NULL;

    pos1 = CdxStreamTell(ioStream);

    AsfGetGuid(ioStream, &guid);

    if (!memcmp(&guid, &audio_stream, sizeof(AsfGuid)))
    {
        type = CDX_MEDIA_AUDIO;
    }
    else if (!memcmp(&guid, &video_stream, sizeof(AsfGuid)))
    {
        type = CDX_MEDIA_VIDEO;
    }
    else
    {
        /*no support ASF_Binary_Media ASF_File_Transfer_Media now*/
        CDX_LOGE("unkonw stream type");
        AsfPrintGuid((const AsfGuid *)&guid);
        goto out;
    }

    avStream = malloc(sizeof(*avStream));
    CDX_LOG_CHECK(avStream, "malloc failure, errno(%d), size(%d)",
                errno, (unsigned int)(sizeof(*avStream)));
    memset(avStream, 0x00, sizeof(*avStream));

    if(!(ctx->hdr.flags & 0x01))
    { // if we aren't streaming...
        avStream->duration = ctx->hdr.send_time / (10000000 / 1000) - ctx->hdr.preroll;
    }

    CdxStreamSkip(ioStream, 16); /*Error Correction Type*/
    CdxStreamSkip(ioStream, 8); /* Time Offset */
    type_specific_size = CdxStreamGetLE32(ioStream);
    CdxStreamSkip(ioStream, 4); /*Error Correction Data Length*/
    avStream->id  = CdxStreamGetLE16(ioStream);
    avStream->id &= 0x7f; /* stream id */

    avStream->encrypted = avStream->id >> 15;

    CdxStreamSkip(ioStream, 4); /*Reserved*/

    avStream->codec.codec_type = type;

    specificData = malloc(type_specific_size);
    CDX_LOG_CHECK(specificData, "malloc failure, errno(%d), size(%d)", errno, type_specific_size);
    CdxStreamRead(ioStream, specificData, type_specific_size);

    if (type == CDX_MEDIA_AUDIO)
    {
        ret = PsrDataAudioStream(specificData, type_specific_size, avStream);
        if (ret == 0)
        {
            ctx->audioNum++;
            ctx->nb_streams++;

            ctx->asfid2avid[avStream->id] = ctx->nb_streams - 1;
         /* mapping of asf ID to AV stream ID; */
            ctx->streams[ctx->nb_streams - 1] = avStream;
            avStream->typeIndex = ctx->audioNum - 1;
        }

    }
    else if (type == CDX_MEDIA_VIDEO)
    {
        ret = PsrDataVideoStream(specificData, type_specific_size, avStream);
        if (ret == 0)
        {
            ctx->videoNum++;
            ctx->nb_streams++;

            ctx->asfid2avid[avStream->id] = ctx->nb_streams - 1;
         /* mapping of asf ID to AV stream ID; */
            ctx->streams[ctx->nb_streams - 1] = avStream;
            avStream->typeIndex = ctx->videoNum - 1;
        }
    }

    if (ret != 0)
    {
        if (avStream->codec.extradata)
        {
            free(avStream->codec.extradata);
            avStream->codec.extradata = NULL;
        }
        free(avStream);
    }

    pos2 = CdxStreamTell(ioStream);
    CdxStreamSkip(ioStream, objSize - (pos2 - pos1 + 24));

out:
    if (specificData)
    {
        free(specificData);
 //       specificData = NULL;
    }
    return ret;
}


cdx_int32 AsfReadHeader(AsfContextT *ctx)
{
    AsfGuid guid;
    CdxStreamT *ioStream = ctx->ioStream;
    cdx_int32 i;
    cdx_int64 asfGsize;
    cdx_uint32 bitrate[128] = {0};

    AsfGetGuid(ioStream, &guid);
    if (memcmp(&guid, &asf_header, sizeof(AsfGuid)))
    {
        CDX_LOGE("not asf format file.");
        goto fail;
    }
    CdxStreamGetLE64(ioStream); /*Object Size*/
    CdxStreamGetLE32(ioStream); /*Number of Header Objects*/
    CdxStreamGetU8(ioStream); /*Reserved1*/
    CdxStreamGetU8(ioStream); /*Reserved2*/
    memset(ctx->asfid2avid, 0xff, sizeof(ctx->asfid2avid));
    for (;;)
    {
        AsfGetGuid(ioStream, &guid);
        asfGsize = CdxStreamGetLE64(ioStream);
#ifdef AWP_DEBUG
        CDX_LOGD("tell pos %lld: ", CdxStreamTell(ioStream) - 24);
        AsfPrintGuid((const AsfGuid *)&guid);
        CDX_LOGD("size = %lld", asfGsize);
#endif

        if ((asfGsize >= ctx->fileSize && !CdxStreamIsNetStream(ioStream))|| asfGsize <= 0)
        {
            CDX_LOGE("invalid AsfGuid size (%lld) file (%lld)", asfGsize,ctx->fileSize );
            goto fail;
        }

        if (!memcmp(&guid, &data_header, sizeof(AsfGuid)))
        {
            ctx->data_object_offset = CdxStreamTell(ioStream);
            // if not streaming, asfGsize is not unlimited (how?),
            //and there is enough space in the file..
            if (!(ctx->hdr.flags & 0x01) && asfGsize >= 100)
            {
                ctx->dataObjectSize = asfGsize - 24;
            }
            else
            {
                ctx->dataObjectSize = (cdx_uint64)(-1);
            }
            break;
        }

        if (asfGsize < 24)
        {
            CDX_LOGE("invalid file (%lld, %lld), asfGsize(%lld)",
                 CdxStreamTell(ioStream), ctx->fileSize, asfGsize);
            //goto fail; //*rtsp
        }

        if (!memcmp(&guid, &file_header, sizeof(AsfGuid)))
        {
            AsfGetGuid(ioStream, &ctx->hdr.guid);
            ctx->hdr.file_size          = CdxStreamGetLE64(ioStream);
            ctx->hdr.create_time        = CdxStreamGetLE64(ioStream);
            ctx->nb_packets             = CdxStreamGetLE64(ioStream);
            ctx->hdr.play_time          = CdxStreamGetLE64(ioStream);
            ctx->hdr.send_time          = CdxStreamGetLE64(ioStream);
            ctx->hdr.preroll            = CdxStreamGetLE32(ioStream);
            ctx->hdr.ignore             = CdxStreamGetLE32(ioStream);
            ctx->hdr.flags              = CdxStreamGetLE32(ioStream);
            ctx->hdr.min_pktsize        = CdxStreamGetLE32(ioStream);
            ctx->hdr.max_pktsize        = CdxStreamGetLE32(ioStream);
            ctx->hdr.max_bitrate        = CdxStreamGetLE32(ioStream);

            ctx->packet_size = ctx->hdr.max_pktsize;
            if(ctx->hdr.play_time)
               ctx->durationMs = ctx->hdr.play_time/10000 - ctx->hdr.preroll; /* 100-nanoseconds  */
            else
               ctx->durationMs = 0;
            logd("++++ durationMs: %lld", ctx->durationMs);
        }
        else if (!memcmp(&guid, &stream_header, sizeof(AsfGuid)))
        {
            PsrObjStream(ctx, asfGsize);
        }
        else if (!memcmp(&guid, &comment_header, sizeof(AsfGuid)))
        {
            cdx_int32 len1, len2, len3, len4, len5;

            len1 = CdxStreamGetLE16(ioStream);
            len2 = CdxStreamGetLE16(ioStream);
            len3 = CdxStreamGetLE16(ioStream);
            len4 = CdxStreamGetLE16(ioStream);
            len5 = CdxStreamGetLE16(ioStream);
            CdxStreamSkip(ioStream, len1);
            CdxStreamSkip(ioStream, len2);
            CdxStreamSkip(ioStream, len3);
            CdxStreamSkip(ioStream, len4);
            CdxStreamSkip(ioStream, len5);
        }
        else if (!memcmp(&guid, &stream_bitrate_guid, sizeof(AsfGuid)))
        {
            cdx_int32 stream_count = CdxStreamGetLE16(ioStream);
            cdx_int32 j;

         ctx->totalBitRate = 0;
            for (j = 0; j < stream_count; j++)
            {
                cdx_int32 flags, bitrate, stream_id;

                flags = CdxStreamGetLE16(ioStream);
                bitrate = CdxStreamGetLE32(ioStream);
                stream_id = (flags & 0x7f);
                ctx->stream_bitrates[stream_id]= bitrate;
                ctx->totalBitRate += bitrate;
            }
        }
        else if (!memcmp(&guid, &extended_content_header, sizeof(AsfGuid)))
        {
            cdx_int32 desc_count, i;

            desc_count = CdxStreamGetLE16(ioStream);
            for (i = 0; i < desc_count; i++)
            {
                cdx_int32 name_len,value_type,value_len;

                name_len = CdxStreamGetLE16(ioStream);
                CdxStreamSkip(ioStream, name_len);
                value_type = CdxStreamGetLE16(ioStream);
                value_len = CdxStreamGetLE16(ioStream);
                if (value_type <= 1) // unicode or byte
                {
                    CdxStreamSkip(ioStream, value_len);
                }
                else if (value_type <= 5) // boolean or DWORD or QWORD or WORD
                {
                    AsfGetValue(ioStream, value_type);
                }
                else
                {
                    CdxStreamSkip(ioStream, value_len);
                }
            }
        }
        else if (!memcmp(&guid, &metadata_header, sizeof(AsfGuid)))
        {
            cdx_int32 n, name_len, value_len;
            n = CdxStreamGetLE16(ioStream);

            for (i=0; i<n; i++)
            {
                CdxStreamGetLE16(ioStream); //lang_list_index
                CdxStreamGetLE16(ioStream);
                name_len=   CdxStreamGetLE16(ioStream);
                CdxStreamGetLE16(ioStream);
                value_len=  CdxStreamGetLE32(ioStream);

                CdxStreamSkip(ioStream, name_len);
                CdxStreamGetLE16(ioStream);
            //we should use get_value() here but it does not work 2 is le16 here but le32 elsewhere
                CdxStreamSkip(ioStream, value_len - 2);
            }
        }
        else if (!memcmp(&guid, &ext_stream_header, sizeof(AsfGuid)))
        {
            cdx_int32 ext_len, payload_ext_ct, stream_ct;
            cdx_uint32 leakRate, streamNum;

            CdxStreamGetLE64(ioStream); // starttime
            CdxStreamGetLE64(ioStream); // endtime
            leakRate = CdxStreamGetLE32(ioStream); // leak-datarate
            CdxStreamGetLE32(ioStream); // bucket-datasize
            CdxStreamGetLE32(ioStream); // init-bucket-fullness
            CdxStreamGetLE32(ioStream); // alt-leak-datarate
            CdxStreamGetLE32(ioStream); // alt-bucket-datasize
            CdxStreamGetLE32(ioStream); // alt-init-bucket-fullness
            CdxStreamGetLE32(ioStream); // max-object-size
            CdxStreamGetLE32(ioStream);
         // flags (reliable,seekable,no_cleanpoints?,resend-live-cleanpoints, rest of bits reserved)
            streamNum = CdxStreamGetLE16(ioStream); // stream-num
            CdxStreamGetLE16(ioStream); // stream-language-id-index
            CdxStreamGetLE64(ioStream); // avg frametime in 100ns units
            stream_ct = CdxStreamGetLE16(ioStream); //stream-name-count
            payload_ext_ct = CdxStreamGetLE16(ioStream); //payload-extension-system-count

            if (streamNum < 128)
                bitrate[streamNum] = leakRate;

            for (i=0; i<stream_ct; i++)
            {
                CdxStreamGetLE16(ioStream);
                ext_len = CdxStreamGetLE16(ioStream);
                //CdxStreamSeek(ioStream, ext_len, SEEK_CUR);
                CdxStreamSkip(ioStream, ext_len); //*rtsp
            }

            for (i=0; i<payload_ext_ct; i++)
            {
                AsfGetGuid(ioStream, &guid);
                CdxStreamGetLE16(ioStream);
                ext_len=CdxStreamGetLE32(ioStream);
                //CdxStreamSeek(ioStream, ext_len, SEEK_CUR);
                CdxStreamSkip(ioStream, ext_len); //*rtsp
            }

            // there could be a optional stream properties object to follow
            // if so the next iteration will pick it up
        }
        else if (!memcmp(&guid, &head1_guid, sizeof(AsfGuid)))
        {
            AsfGetGuid(ioStream, &guid);
            CdxStreamGetLE32(ioStream);
            CdxStreamGetLE16(ioStream);
        }
        else
        {
            //CdxStreamSeek(ioStream, asfGsize - 24, SEEK_CUR);
            CdxStreamSkip(ioStream, asfGsize - 24); //*rtsp
        }

        if (CdxStreamTell(ioStream) >= (cdx_int64)(ctx->fileSize - 1))
        {
            CDX_LOGE("invalid file (%lld, %lld)", CdxStreamTell(ioStream), ctx->fileSize);
            goto fail;
        }

    }

    AsfGetGuid(ioStream, &guid);//CdxStreamT ID
    CdxStreamGetLE64(ioStream);//Total Data Packets
    CdxStreamGetU8(ioStream);//Reserved
    CdxStreamGetU8(ioStream);//Reserved
    if (CdxStreamTell(ioStream) >= (cdx_int64)(ctx->fileSize - 1))
    {
        CDX_LOGE("invalid file (%lld, %lld)", CdxStreamTell(ioStream), ctx->fileSize);
        goto fail;
    }

    ctx->data_offset = CdxStreamTell(ioStream);
    ctx->packet_size_left = 0;

    for (i=0; i<128; i++)
    {
        cdx_int32 streamNum = ctx->asfid2avid[i];
        if (streamNum >= 0)
        {
            struct AsfCodecS *codec = &ctx->streams[streamNum]->codec;
            if (!codec->bit_rate)
            {
                codec->bit_rate = bitrate[i];
            }
        }
    }

    if(!ctx->audioNum && !ctx->videoNum)
    {
        CDX_LOGW("no media found...");
        return -1;
    }

    return 0;

fail:
    for (i = 0; i < ctx->nb_streams; i++)
    {
        AsfMediaStreamT *avStream = ctx->streams[i];
        if (avStream)
        {
           if (avStream->codec.extradata)
            {
              free(avStream->codec.extradata);
              avStream->codec.extradata = NULL;
           }
           free(avStream);
        }
        ctx->streams[i] = NULL;
    }

    return -1;
}

static cdx_int32 AsfGetPacket(AsfContextT *ctx)
{
    CdxStreamT *ioStream = ctx->ioStream;
    cdx_uint32 packetLength, padsize, dummyVal;
    cdx_int32 nRsize = 8;
    cdx_int32 c, d, e, off;

    off = (CdxStreamTell(ctx->ioStream) - ctx->data_offset) % ctx->packet_size + 3;

    c = d = e = -1;
    while (off-- > 0)
    {
        c = d; d = e;
        e = CdxStreamGetU8(ioStream);
        if (c == 0x82 && !d && !e)//0X820000
        {
            break;
        }
    }

    if (c != 0x82)
    {
        if (!CdxStreamEos(ioStream))
        {
            CDX_LOGW("ff asf bad header %x  at:%lld", c, CdxStreamTell(ioStream));
        }
    }

    if ((c & 0x8f) == 0x82)
    {
        if (d || e)
        {
            CDX_LOGW("ff asf bad non zero");
            return -1;
        }
        c = CdxStreamGetU8(ioStream);
        d = CdxStreamGetU8(ioStream);
        nRsize += 3;
    }
    else
    {
        CdxStreamSeek(ioStream, -1, SEEK_CUR); //FIXME
    }

    ctx->packet_flags = c; /*Length Type Flags*/
    ctx->packet_property = d; /*Property Flags*/

    DO_2BITS(ctx->packet_flags >> 5, packetLength, ctx->packet_size);
    DO_2BITS(ctx->packet_flags >> 1, dummyVal, 0); // sequence
    DO_2BITS(ctx->packet_flags >> 3, padsize, 0); // padding length

    //the following checks prevent overflows and infinite loops
    if (packetLength >= (1U<<29))
    {
        CDX_LOGE("invalid packetLength %d at:%lld\n", packetLength, CdxStreamTell(ioStream));
        return -1;
    }

    if (padsize >= packetLength)
    {
        CDX_LOGE("invalid padsize %d at:%lld\n", padsize, CdxStreamTell(ioStream));
        return -1;
    }

    ctx->packet_timestamp = CdxStreamGetLE32(ioStream);
    CdxStreamSkip(ioStream, 2); /* duration */
    // nRsize has at least 11 bytes which have to be present

    ctx->packet_hdr_size = (int)(CdxStreamTell(ioStream) - ctx->packet_pos);
    if (ctx->packet_flags & 0x01)
    {//multiple payloads
        ctx->packet_segsizetype = CdxStreamGetU8(ioStream); nRsize++;
        ctx->packet_segments = ctx->packet_segsizetype & 0x3f;
    }
    else
    {
        ctx->packet_segments = 1;
        ctx->packet_segsizetype = 0x80;
    }
    ctx->packet_size_left = packetLength - padsize - nRsize;
    if (packetLength < ctx->hdr.min_pktsize)
    {
        padsize += ctx->hdr.min_pktsize - packetLength;
    }
    ctx->packet_padsize = padsize;

//    PrintCurPkt(ctx);
    return 0;
}

/*Payload data*/
/*
Stream Number                   BYTE                                  8
Media Object Number           BYTE, WORD, or DWORD      0, 8, 16, 32
Offset Into Media Object       BYTE, WORD, or DWORD     0, 8, 16, 32
Replicated Data Length         BYTE, WORD, or DWORD     0, 8, 16, 32
Replicated Data                    BYTE                                  varies
Payload Data                        BYTE                                 varies
*/
static cdx_int32 AsfReadFrameHeader(AsfContextT *ctx)
{
    CdxStreamT *ioStream = ctx->ioStream;
    cdx_int32 nRsize = 1;
    cdx_uint32 num = CdxStreamGetU8(ioStream);

    ctx->packet_segments--;
    ctx->packet_key_frame = !!(num & 0x80);
    ctx->stream_index = ctx->asfid2avid[num & 0x7f];
    // sequence should be ignored!
    DO_2BITS(ctx->packet_property >> 4, ctx->packet_seq, 0);
    DO_2BITS(ctx->packet_property >> 2, ctx->packet_frag_offset, 0);
    DO_2BITS(ctx->packet_property, ctx->packet_replic_size, 0);
    if (ctx->packet_replic_size >= 8)
    {
        ctx->packet_obj_size = CdxStreamGetLE32(ioStream);
        if (ctx->packet_obj_size >= (1 << 24) || ctx->packet_obj_size <= 0)
        {
            PrintCurPkt(ctx);
            CDX_LOGE("packet_obj_size(%d) invalid, pos(%lld)",
                 ctx->packet_obj_size, CdxStreamTell(ioStream));
            CDX_LOGD("stream_index(%d) num(%d)", ctx->stream_index, num & 0x7f);
            return -1;
        }
        ctx->packet_frag_timestamp = CdxStreamGetLE32(ioStream); // timestamp
        CdxStreamSkip(ioStream, ctx->packet_replic_size - 8);
        nRsize += ctx->packet_replic_size; // FIXME - check validity
    }
    else if (ctx->packet_replic_size == 1)
    {
        // multipacket - frag_offset is beginning timestamp
        ctx->packet_time_start = ctx->packet_frag_offset;
        ctx->packet_frag_offset = 0;
        ctx->packet_frag_timestamp = ctx->packet_timestamp;

        ctx->packet_time_delta = CdxStreamGetU8(ioStream);
        nRsize++;
    }
    else if (ctx->packet_replic_size != 0)
    {
        CDX_LOGE("unexpected packet_replic_size of %d", ctx->packet_replic_size);
        return -1;
    }

    if (ctx->packet_flags & 0x01)
    {
        DO_2BITS(ctx->packet_segsizetype >> 6, ctx->packet_frag_size, 0); // 0 is illegal
        if((cdx_int32)ctx->packet_frag_size > ctx->packet_size_left - nRsize)
        {
            CDX_LOGE("packet_frag_size is invalid");
            return -1;
        }
    }
    else
    {
        ctx->packet_frag_size = ctx->packet_size_left - nRsize;
    }

//    CDX_LOGD("packet_frag_size(0x%x), pos(%lld)", ctx->packet_frag_size, CdxStreamTell(ioStream));

    if (ctx->packet_replic_size == 1)
    {
        ctx->packet_multi_size = ctx->packet_frag_size;
        if (ctx->packet_multi_size > ctx->packet_size_left)
        {
            CDX_LOGE("invilad packet multi size, (%d, %d)",
                 ctx->packet_multi_size, ctx->packet_size_left);
            return -1;
        }
    }
    ctx->packet_size_left -= nRsize;

    return 0;
}

cdx_int32 AsfNewPacket(AsfPacketT *pkt, cdx_int32 size)
{
    pkt->size = size;
    return 0;
}

cdx_int32 AsfReadPacket(AsfContextT *ctx)
{
    AsfPacketT *pkt = &ctx->pkt;
    CdxStreamT *ioStream = ctx->ioStream;

    ctx->frame_ctrl = 0;//mid
    for (;;)
    {
        if(ctx->forceStop)
        {
            return -1;
        }

        if (CdxStreamTell(ioStream) >= (cdx_int64)(ctx->fileSize - 1))
        {
            CDX_LOGD("stream eos...");
            return PSR_EOS;
        }

        //4.2.2    Payload parsing information
        if (ctx->packet_size_left < FRAME_HEADER_SIZE || ctx->packet_segments < 1)
        {
            cdx_int32 ret = ctx->packet_size_left + ctx->packet_padsize;
            if (ret < 0)
            {
                CDX_LOGW("padsize error!");
                return -1;
            }
            /* fail safe */

            CdxStreamSkip(ioStream, ret);

            ctx->packet_pos = CdxStreamTell(ctx->ioStream);
            if (ctx->dataObjectSize != (cdx_uint64)-1 &&
                (ctx->packet_pos - ctx->data_object_offset >= ctx->dataObjectSize))
            {
                CDX_LOGW("data object end...");
                return PSR_EOS; /* Do not exceed the size of the data object */
            }
            //4.2.2    Payload parsing information
            ret = AsfGetPacket(ctx);
            if (ret < 0)
            {
                CDX_LOGW("AsfGetPacket error!");
                //return -1;
            }
            ctx->packet_time_start = 0;
            continue;
        }

        if (ctx->packet_time_start == 0)
        {

            if (AsfReadFrameHeader(ctx) < 0)
            {
                ctx->packet_segments = 0;
                continue;
            }

            if (ctx->stream_index == 0xff)
            {
                CDX_LOGE("invalid stream index(%d)-----------", ctx->stream_index);
               CdxStreamSkip(ioStream, ctx->packet_frag_size);
                ctx->packet_size_left -= ctx->packet_frag_size;
                continue;
            }
            ctx->asf_st = ctx->streams[(int)ctx->stream_index];

            CDX_LOG_CHECK(ctx->asf_st, "stream index(%d)", ctx->stream_index);
            if (ctx->asf_st->codec.codec_type == CDX_MEDIA_VIDEO
                && !ctx->asf_st->active)
            {
                CdxStreamSkip(ioStream, ctx->packet_frag_size);
                ctx->packet_size_left -= ctx->packet_frag_size;
                continue;
            }

            if (ctx->asf_st->discard_data)
            {
                if (!ctx->packet_key_frame
               || (ctx->packet_key_frame && ctx->packet_frag_offset != 0))
                {
                    ctx->packet_time_start = 0;
                    CdxStreamSkip(ioStream, ctx->packet_frag_size);
                    ctx->packet_size_left -= ctx->packet_frag_size;
                    continue;
                }
            }

        }

        ctx->asf_st->discard_data = 0;

        if (ctx->packet_replic_size == 1)
        {
            // frag_offset is here used as the beginning timestamp
            ctx->packet_frag_timestamp = ctx->packet_time_start;
            ctx->packet_time_start += ctx->packet_time_delta;
            ctx->packet_obj_size = ctx->packet_frag_size = CdxStreamGetU8(ioStream);
            ctx->packet_size_left--;
            ctx->packet_multi_size--;
            if (ctx->packet_multi_size < ctx->packet_obj_size)
            {
                ctx->packet_time_start = 0;
                CdxStreamSeek(ioStream, ctx->packet_multi_size, SEEK_CUR);
                ctx->packet_size_left -= ctx->packet_multi_size;
                continue;
            }
            ctx->packet_multi_size -= ctx->packet_obj_size;
        }
        if (ctx->asf_st->frag_offset + ctx->packet_frag_size <= ctx->asf_st->pkt.size
            && ctx->asf_st->frag_offset + ctx->packet_frag_size > ctx->packet_obj_size)
        {
            ctx->packet_obj_size= ctx->asf_st->pkt.size;
        }

        if (ctx->asf_st->pkt.size != ctx->packet_obj_size
            || ctx->asf_st->frag_offset + ctx->packet_frag_size > ctx->asf_st->pkt.size)
        { //FIXME is this condition sufficient?
            if(ctx->asf_st->frag_offset)
            {
                ctx->asf_st->frag_offset = 0;
            }
            /* new packet */
            AsfNewPacket(&ctx->asf_st->pkt, ctx->packet_obj_size);
            ctx->asf_st->seq = ctx->packet_seq;

        }

        ctx->packet_size_left -= ctx->packet_frag_size;
        if (ctx->packet_size_left < 0)
        {
            continue;
        }

        if (ctx->packet_frag_offset >= ctx->asf_st->pkt.size
            || ctx->packet_frag_size > ctx->asf_st->pkt.size - ctx->packet_frag_offset)
        {
            continue;
        }

        if (ctx->asf_st->codec.codec_type == CDX_MEDIA_VIDEO && ctx->asf_st->frag_offset == 0)
        {
            ctx->frame_ctrl |= 1;
        }
        ctx->asf_st->frag_offset += ctx->packet_frag_size;

        /* test if whole packet is read */
        if (ctx->asf_st->frag_offset == ctx->asf_st->pkt.size)
        {
            ctx->asf_st->frag_offset = 0;
            *pkt = ctx->asf_st->pkt;

            if(ctx->asf_st->codec.codec_type == CDX_MEDIA_VIDEO)
            {
                ctx->frame_ctrl |= 2;
            }
            ctx->asf_st->pkt.size = 0;
        }

        break;
    }

    return 0;
}

// Added to support seeking after packets have been read
// If information is not reset, read_packet fails due to
// leftover information from previous reads
static void AsfResetHeader(AsfContextT *ctx)
{
    cdx_int32 i;

//    ctx->packet_nb_frames = 0;
    ctx->packet_size_left = 0;
    ctx->packet_flags = 0;
    ctx->packet_property = 0;
    ctx->packet_timestamp = 0;
    ctx->packet_segsizetype = 0;
    ctx->packet_segments = 0;
    ctx->packet_seq = 0;
    ctx->packet_replic_size = 0;
    ctx->packet_key_frame = 0;
    ctx->packet_padsize = 0;
    ctx->packet_frag_offset = 0;
    ctx->packet_frag_size = 0;
    ctx->packet_frag_timestamp = 0;
    ctx->packet_multi_size = 0;
    ctx->packet_obj_size = 0;
    ctx->packet_time_delta = 0;
    ctx->packet_time_start = 0;

    for (i = 0; i < ctx->nb_streams; i++)
    {
        if(ctx->streams[i])
        {
            ctx->streams[i]->frag_offset = 0;
            ctx->streams[i]->seq = 0;
        }
    }

    ctx->asf_st = NULL;
}

static cdx_int32 AsfReadClose(AsfContextT *ctx)
{
    cdx_int32 i;

    AsfResetHeader(ctx);
    for (i = 0; i < ctx->nb_streams; i++)
    {
        if(ctx->streams[i])
        {
            if (ctx->streams[i]->codec.extradata)
            {
                free(ctx->streams[i]->codec.extradata);
                ctx->streams[i]->codec.extradata = NULL;
            }

            free(ctx->streams[i]);
            ctx->streams[i] = NULL;
        }
    }

    return 0;
}

static void AsfBuildSimpleIndex(AsfContextT *ctx)
{
    AsfGuid guid;
    cdx_int64 asfGsize;
    cdx_int64 current_pos;
    cdx_int32 i, j;

    current_pos = CdxStreamTell(ctx->ioStream);
    CdxStreamSeek(ctx->ioStream, ctx->data_object_offset + ctx->dataObjectSize, SEEK_SET);

    for (j = 0; j < 4; j++)
    {
        if (CdxStreamEos(ctx->ioStream))
        {
            CDX_LOGI("stream eos...");
            break;
        }
        AsfGetGuid(ctx->ioStream, &guid);
        if (!memcmp(&guid, &index_guid, sizeof(AsfGuid)))
        {
            asfGsize = CdxStreamGetLE64(ctx->ioStream);
            CdxStreamSkip(ctx->ioStream, 16); /* file id */
            ctx->itimeMs = (int) (CdxStreamGetLE64(ctx->ioStream) / 10000);
            /* Index Entry Time Interval */
            CdxStreamSkip(ctx->ioStream, 4); /* Maximum Packet Count */

            ctx->ict = CdxStreamGetLE32(ctx->ioStream);

            if (ctx->ict < 16 * 1024 && ctx->ict > 2)
            {
                ctx->index_ptr = malloc(sizeof(struct AsfIndexS) * ctx->ict);

                for (i = 0; i < ctx->ict; i++)
                {
                    ctx->index_ptr[i].packet_number = CdxStreamGetLE32(ctx->ioStream);
                    CdxStreamSkip(ctx->ioStream, 2); /*packet_count*/
                }
            }
            else
            {
                CDX_LOGW("index tab too large, the size(%d)", ctx->ict);
            }
            /* get one index tab*/
            break;
        }
        else
        {
            asfGsize = CdxStreamGetLE64(ctx->ioStream);
            CdxStreamSkip(ctx->ioStream, asfGsize - 24);
        }

        if (CdxStreamTell(ctx->ioStream) > ctx->fileSize - 24)
        {
            CDX_LOGW("not enought data to do some thing...");
            break;
        }
    }

    CdxStreamSeek(ctx->ioStream, current_pos, SEEK_SET);
    return ;
}

static cdx_int32 AsfSeekToTime(AsfContextT *ctx, cdx_int32 seconds)
{
    cdx_int32 times;
    cdx_int64 pos;

    if (seconds < 0 || seconds * 1000 > ctx->durationMs)
    {
        return -1;
    }

    if (ctx->index_ptr)
    {
        cdx_int32 *PktIdxPtr = (cdx_int32*)((uintptr_t)ctx->index_ptr);
        cdx_int32 packetnum;
      cdx_int32 seek_temp_time;
      cdx_int32 tmp_index_number = 0;

      seconds += (ctx->hdr.preroll)/1000;
      times = (seconds * 1000) / ctx->itimeMs;

      if (times >= ctx->ict)
      {
          times = ctx->ict - 1;
      }

        ctx->curr_key_tbl_idx = times;
        packetnum = *(PktIdxPtr + ctx->curr_key_tbl_idx);
        pos = ctx->data_offset + ctx->packet_size*(cdx_int64)packetnum;
        CdxStreamSeek(ctx->ioStream, pos, SEEK_SET);

      AsfResetHeader(ctx);
      AsfGetPacket(ctx);
       AsfReadFrameHeader(ctx);
      seek_temp_time = (cdx_int32)(ctx->packet_frag_timestamp/1000);
      CdxStreamSeek(ctx->ioStream, pos, SEEK_SET);

      if(seek_temp_time != seconds)
      {
         if(seek_temp_time < seconds)
         {
            tmp_index_number = (times + seconds - seek_temp_time) < ctx->ict ?
                              (times + seconds - seek_temp_time) : (ctx->ict - 1);
         }

         if(seek_temp_time > seconds)
         {
            tmp_index_number = (times - (seek_temp_time - seconds)) >= 0 ?
                              (times - (seek_temp_time - seconds)) : 0;

         }

         ctx->curr_key_tbl_idx = tmp_index_number;
            packetnum = *(PktIdxPtr + ctx->curr_key_tbl_idx);
            pos = ctx->data_offset + ctx->packet_size*(cdx_int64)packetnum;
            CdxStreamSeek(ctx->ioStream, pos, SEEK_SET);
      }

        AsfResetHeader(ctx);
    }
    else  /* with no index tab */
    {    //we must enhance it
        cdx_int32 startpkt = 0;
        cdx_int32 curr_seconds;
        cdx_int32 cmpflag = 0;

      if(ctx->nb_packets != 0xffffffff)
      {
           startpkt = (cdx_int32)(ctx->nb_packets * seconds / ctx->durationMs);//from packet
        }
        else if(ctx->totalBitRate)
        {
           startpkt = ctx->totalBitRate * seconds / 8 / ctx->packet_size;
        }

        while (1)
        {
            pos = ctx->data_offset + ctx->packet_size * (cdx_int64)startpkt;
            if (CdxStreamSeek(ctx->ioStream, pos, SEEK_SET) < 0)
            {
                CDX_LOGW("CdxStreamSeek error! pos[%x,%x]",
                    (cdx_uint32)(pos>>32), (cdx_uint32)pos);
                return -1;
            }
            AsfResetHeader(ctx);
            AsfGetPacket(ctx);
            curr_seconds = ctx->packet_timestamp/1000;

            if (curr_seconds > ctx->durationMs * 1000)//for error stream compatiable
            {
                CDX_LOGW("invalid time (%d, %lld)", curr_seconds, ctx->durationMs);
                return -1;
            }

            if (curr_seconds == seconds)
            {

                break; /*finish*/
            }
            else if (curr_seconds < seconds)
            {
                if (cmpflag == 1)
                    break;
                cmpflag = -1;
                startpkt++;
            }
            else /*curr_seconds > seconds*/
            {
                if (cmpflag == -1)
                    break;
                cmpflag = 1;
                startpkt--;
            }
        }

        pos = ctx->data_offset + ctx->packet_size * (cdx_int64)startpkt;
        if (CdxStreamSeek(ctx->ioStream, pos, SEEK_SET) < 0)
        {
            CDX_LOGW("CdxStreamSeek error! pos[%x,%x]\n",
                 (cdx_uint32)(pos>>32), (cdx_uint32)pos);
            return -1;
        }

        if (seconds < (ctx->packet_frag_timestamp - ctx->hdr.preroll)/1000) /*backward*/
        {
            while (ctx->packet_key_frame == 0)
            {
                AsfResetHeader(ctx);
                AsfGetPacket(ctx);
                AsfReadFrameHeader(ctx);

                if (startpkt >0)
                {
                    startpkt--;
                }

                pos = ctx->data_offset + ctx->packet_size * (cdx_int64)startpkt;
                if(CdxStreamSeek(ctx->ioStream, pos, SEEK_SET) < 0)
                {
                    CDX_LOGW("CdxStreamSeek error! pos[%x,%x]\n",
                      (cdx_uint32)(pos>>32), (cdx_uint32)pos);
                    return -1;
                }
            }
        }

        AsfResetHeader(ctx);
    }

    return 0;
}

int AsfPsrCoreRead(AsfContextT *ctx)
{
    int ret = 0;

    switch (ctx->status)
    {
        case CDX_MEDIA_STATUS_PLAY:
            ret = AsfReadPacket(ctx);
            break;
        default:
            break;
    }

    return ret;
}

int AsfPsrCoreOpen(AsfContextT *ctx, CdxStreamT *stream)
{
    ctx->ioStream = stream;

   ctx->seekAble = CdxStreamSeekAble(stream);
    ctx->fileSize = CdxStreamSize(stream);
    if (ctx->fileSize == -1)
    {
        ctx->fileSize = 1LL << 62;
    }

    if (AsfReadHeader(ctx) != 0)
    {
        CDX_LOGW("asf read header failed");
        return -1;
    }

    if (ctx->seekAble)
    {
        AsfBuildSimpleIndex(ctx);
    }

    AsfResetHeader(ctx);

    return 0;
}

int AsfPsrCoreClose(AsfContextT *ctx)
{

    AsfReadClose(ctx);
    if(ctx->index_ptr)
    {
        free(ctx->index_ptr);
        ctx->index_ptr = NULL;
    }
    free(ctx);

    return 0;
}

AsfContextT *AsfPsrCoreInit(void)
{
    AsfContextT *ctx;

    ctx = malloc(sizeof(*ctx));
    CDX_LOG_CHECK(ctx, "malloc failure, size(%d), errno(%d)",
               (unsigned int)(sizeof(*ctx)), errno);
    memset(ctx, 0, sizeof(*ctx));

    return ctx;
}

int AsfPsrCoreIoctrl(AsfContextT *ctx, cdx_uint32 uCmd, cdx_uint32 uParam)
{
    switch(uCmd)
    {
    case CDX_MEDIA_STATUS_PLAY:
        if(ctx->status != CDX_MEDIA_STATUS_IDLE)
        {
            return -1;
        }

        //set video clock back to normal speed
        //SetVideoClkSpeed(1);
        //set normal play mode

        ctx->status = CDX_MEDIA_STATUS_PLAY;
        break;

    case CDX_MEDIA_STATUS_JUMP:
    {
        cdx_int64 seekPosUs;
        seekPosUs = *(cdx_int64 *)(uintptr_t)uParam;

        int seconds;
      int ret;

        seconds = seekPosUs / 1000000;

        ret = AsfSeekToTime(ctx, seconds);
        if(ret < 0)
        {
            CDX_LOGW("AsfSeekToTime() fail!");
            return -1;
        }

        int i = 0;
        for (i = 0; i < ctx->nb_streams; i++)
        {
            if (ctx->streams[i]->codec.codec_type == CDX_MEDIA_VIDEO)
            {
                ctx->streams[i]->discard_data = 1;
            }
        }

        ctx->status = CDX_MEDIA_STATUS_PLAY;
        break;
    }
    default:
        break;
    }

    return 0;
}

