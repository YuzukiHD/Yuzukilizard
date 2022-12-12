/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxAiffParser.c
* Description :
* History :
*   Author  : Khan <chengkan@allwinnertech.com>
*   Date    : 2014/07/07
*   Comment : 创建初始版本，实现 AIFF 的解复用功能
*/
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxAiffParser.h>
#include <math.h>
#include "CdxIoUtils.h"


#define ABS_EDIAN_FLAG_LITTLE       ((unsigned int)(0<<16))
#define ABS_EDIAN_FLAG_BIG          ((unsigned int)(1<<16))

int cdx_av_get_exact_bits_per_sample(AiffCodecID codec_id);

int cdx_av_get_bits_per_sample(AiffCodecID codec_id);

int cdx_av_get_audio_frame_duration(AVCodecContext *avctx, int frame_bytes);

uint32_t get_aiff_header(AiffParserImplS *s, int size, unsigned version);


static const CDXAVCodecTag cdx_codec_aiff_tags[] = {
    { AIFF_CODEC_ID_PCM_S16BE,    MKTAG('N','O','N','E') },
    { AIFF_CODEC_ID_PCM_S8,       MKTAG('N','O','N','E') },
    { AIFF_CODEC_ID_PCM_U8,       MKTAG('r','a','w',' ') },
    { AIFF_CODEC_ID_PCM_S24BE,    MKTAG('N','O','N','E') },
    { AIFF_CODEC_ID_PCM_S32BE,    MKTAG('N','O','N','E') },
    { AIFF_CODEC_ID_PCM_F32BE,    MKTAG('f','l','3','2') },
    { AIFF_CODEC_ID_PCM_F64BE,    MKTAG('f','l','6','4') },
    { AIFF_CODEC_ID_PCM_ALAW,     MKTAG('a','l','a','w') },
    { AIFF_CODEC_ID_PCM_MULAW,    MKTAG('u','l','a','w') },
    { AIFF_CODEC_ID_PCM_S24BE,    MKTAG('i','n','2','4') },
    { AIFF_CODEC_ID_PCM_S32BE,    MKTAG('i','n','3','2') },
    { AIFF_CODEC_ID_MACE3,        MKTAG('M','A','C','3') },
    { AIFF_CODEC_ID_MACE6,        MKTAG('M','A','C','6') },
    { AIFF_CODEC_ID_GSM,          MKTAG('G','S','M',' ') },
    { AIFF_CODEC_ID_ADPCM_G722,   MKTAG('G','7','2','2') },
    { AIFF_CODEC_ID_ADPCM_G726LE, MKTAG('G','7','2','6') },
    { AIFF_CODEC_ID_PCM_S16BE,    MKTAG('t','w','o','s') },
    { AIFF_CODEC_ID_PCM_S16LE,    MKTAG('s','o','w','t') },
    { AIFF_CODEC_ID_ADPCM_IMA_QT, MKTAG('i','m','a','4') },
    { AIFF_CODEC_ID_QDM2,         MKTAG('Q','D','M','2') },
    { AIFF_CODEC_ID_QCELP,        MKTAG('Q','c','l','p') },
    { AIFF_CODEC_ID_NONE,         0 },
};

static AiffCodecID aiff_codec_get_id(int bps)
{
    if (bps <= 8)
        return AIFF_CODEC_ID_PCM_S8;
    if (bps <= 16)
        return AIFF_CODEC_ID_PCM_S16BE;
    if (bps <= 24)
        return AIFF_CODEC_ID_PCM_S24BE;
    if (bps <= 32)
        return AIFF_CODEC_ID_PCM_S32BE;

    /* bigger than 32 isn't allowed  */
    return AIFF_CODEC_ID_NONE;
}

static AiffCodecID cdx_codec_get_id(const CDXAVCodecTag *tags, unsigned int tag)
{
    int i;
    for (i = 0; tags[i].id != AIFF_CODEC_ID_NONE; i++)
        if (tag == tags[i].tag)
            return tags[i].id;
    /*
      TO FIX
      大小写
      */
    return AIFF_CODEC_ID_NONE;
}

static enum EAUDIOCODECFORMAT aiff_get_audio_subfmt(AiffCodecID code_id)
{
    int format = WAVE_FORMAT_PCM;
    switch(code_id)
    {
        case AIFF_CODEC_ID_PCM_S16BE:
        case AIFF_CODEC_ID_PCM_S24BE:
        case AIFF_CODEC_ID_PCM_S32BE:
            format = WAVE_FORMAT_PCM | ABS_EDIAN_FLAG_BIG;
            break;
        case AIFF_CODEC_ID_PCM_F32BE:
        case AIFF_CODEC_ID_PCM_F64BE:
            format = WAVE_FORMAT_IEEE_FLOAT | ABS_EDIAN_FLAG_BIG;
            break;
        case AIFF_CODEC_ID_PCM_S16LE:
            format = WAVE_FORMAT_PCM | ABS_EDIAN_FLAG_LITTLE;
            break;
        case AIFF_CODEC_ID_PCM_ALAW:
            format = WAVE_FORMAT_ALAW;
            break;
        case AIFF_CODEC_ID_PCM_MULAW:
            format = WAVE_FORMAT_MULAW;
            break;
        case AIFF_CODEC_ID_MACE3:
        case AIFF_CODEC_ID_MACE6:
        case AIFF_CODEC_ID_GSM:
        case AIFF_CODEC_ID_ADPCM_G722:
        case AIFF_CODEC_ID_ADPCM_G726LE:
        case AIFF_CODEC_ID_ADPCM_IMA_QT:
        case AIFF_CODEC_ID_QDM2:
        case AIFF_CODEC_ID_QCELP:
        default:
            break;
      };
      return format;
}

static int cdx_get_extradata(AVCodecContext *avctx, CdxStreamT *pb, int size)
{
    int ret = 0;
    avctx->extradata = malloc(size + 8);
    if(!avctx->extradata)
    {
        CDX_LOGE("extradata malloc fail!");
        return -1;
    }
    memset(avctx->extradata, 0x00, size + 8);
    avctx->extradata_size = size;

    ret = CdxStreamRead(pb, avctx->extradata, size);
    if (ret != size) {
        free(avctx->extradata);
        avctx->extradata_size = 0;
        CDX_LOGW("Failed to read extradata of size %d\n", size);
        return -1;
    }

    return ret;
}

static int cdx_mov_read_chan(CdxStreamT *pb, int64_t size)
{
    uint32_t layout_tag, bitmap, num_descr, label_mask;
    uint32_t i;

    if (size < 12)
        return -1;

    layout_tag = cdxio_rb32(pb);
    bitmap     = cdxio_rb32(pb);
    num_descr  = cdxio_rb32(pb);

    CDX_LOGD("chan: layout=%u bitmap=%u num_descr=%u\n",
            layout_tag, bitmap, num_descr);

    if ((uint64_t)size < 12ULL + num_descr * 20ULL)
        return 0;

    label_mask = 0;
    for (i = 0; i < num_descr; i++) {
        uint32_t label;
        if (CdxStreamEos(pb)) {
            CDX_LOGW("reached EOF while reading channel layout\n");
            return -1;
        }
        label = cdxio_rb32(pb);          // mChannelLabel
        cdxio_rb32(pb);                      // mChannelFlags
        cdxio_rl32(pb);                      // mCoordinates[0]
        cdxio_rl32(pb);                      // mCoordinates[1]
        cdxio_rl32(pb);                      // mCoordinates[2]
        size -= 20;
#if 0
        if (layout_tag == 0) {
            uint32_t mask_incr = mov_get_channel_label(label);
            if (mask_incr == 0) {
                label_mask = 0;
                break;
            }
            label_mask |= mask_incr;
        }
#endif
    }
#if 0
    if (layout_tag == 0) {
        if (label_mask)
            st->codec->channel_layout = label_mask;
    } else
        st->codec->channel_layout = ff_mov_get_channel_layout(layout_tag, bitmap);
#endif
    CdxStreamSeek(pb, size - 12, SEEK_CUR);
    return 0;
}

static int get_tag(CdxStreamT *pb, uint32_t * tag)
{
    int size;
    if(CdxStreamEos(pb))
        return -1;
    *tag = cdxio_rl32(pb);
    size = cdxio_rb32(pb);

    if (size < 0)
        size = 0x7fffffff;

    return size;
}

int cdx_av_get_exact_bits_per_sample(AiffCodecID codec_id)
{
    switch (codec_id) {
    case AIFF_CODEC_ID_ADPCM_G722:
        return 4;
    case AIFF_CODEC_ID_PCM_ALAW:
    case AIFF_CODEC_ID_PCM_MULAW:
    case AIFF_CODEC_ID_PCM_S8:
    case AIFF_CODEC_ID_PCM_U8:
        return 8;
    case AIFF_CODEC_ID_PCM_S16BE:
    case AIFF_CODEC_ID_PCM_S16LE:
        return 16;
    case AIFF_CODEC_ID_PCM_S24BE:
        return 24;
    case AIFF_CODEC_ID_PCM_S32BE:
    case AIFF_CODEC_ID_PCM_F32BE:
        return 32;
    case AIFF_CODEC_ID_PCM_F64BE:
        return 64;
    default:
        return 0;
    }
}

int cdx_av_get_bits_per_sample(AiffCodecID codec_id)
{
    switch (codec_id) {
    case AIFF_CODEC_ID_ADPCM_IMA_QT:
        return 4;
    default:
        return cdx_av_get_exact_bits_per_sample(codec_id);
    }
}

int cdx_av_get_audio_frame_duration(AVCodecContext *avctx, int frame_bytes)
{
    int id, sr, ch, ba, tag, bps;

    id  = avctx->codec_id;
    sr  = avctx->sample_rate;
    ch  = avctx->channels;
    ba  = avctx->block_align;
    tag = avctx->codec_tag;
    bps = cdx_av_get_exact_bits_per_sample(avctx->codec_id);

    /* codecs with an exact constant bits per sample */
    if (bps > 0 && ch > 0 && frame_bytes > 0 && ch < 32768 && bps < 32768)
        return (frame_bytes * 8LL) / (bps * ch);
    bps = avctx->bits_per_coded_sample;

    /* codecs with a fixed packet duration */
    switch (id)
    {
        case AIFF_CODEC_ID_ADPCM_IMA_QT:
            return 64;
        case AIFF_CODEC_ID_GSM:
        case AIFF_CODEC_ID_QCELP:
            return 160;
    }

    if (frame_bytes > 0) {
        /* calc from frame_bytes only */
        if (ch > 0) {
            /* calc from frame_bytes and channels */
            switch (id) {
            case AIFF_CODEC_ID_MACE3:
                return 3 * frame_bytes / ch;
            case AIFF_CODEC_ID_MACE6:
                return 6 * frame_bytes / ch;
            }
        }
    }

    /* Fall back on using frame_size */
    if (avctx->frame_size > 1 && frame_bytes)
        return avctx->frame_size;
    return 0;
}

unsigned int get_aiff_header(AiffParserImplS *s, int size,
                                    unsigned version)
{
    struct AiffParserImplS *impl = NULL;
    CdxStreamT *pb         = NULL;
    AVCodecContext *codec  = NULL;
    AIFFInputContext *aiff = NULL;
    int exp;
    unsigned long long val = 34;
    double sample_rate;
    unsigned int num_frames;

    impl  = s;
    pb    = impl->stream;
    codec = &impl->avctx;
    aiff  = &impl->aiff;

    if (size & 1)
        size++;
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->channels = cdxio_rb16(pb);
    num_frames = cdxio_rb32(pb);
    codec->bits_per_coded_sample = cdxio_rb16(pb);
    exp = cdxio_rb16(pb);
    val = cdxio_rb64(pb);
    sample_rate = ldexp(val, exp - 16383 - 63);
    codec->sample_rate = sample_rate;
    size -= 18;

    /* get codec id for AIFF-C */
    if (size < 4) {
        version = AIFF;
    } else if (version == AIFF_C_VERSION1) {
        codec->codec_tag = cdxio_rl32(pb);
        codec->codec_id  = cdx_codec_get_id(cdx_codec_aiff_tags, codec->codec_tag);
        size -= 4;
    }

    if (version != AIFF_C_VERSION1 || codec->codec_id == AIFF_CODEC_ID_PCM_S16BE) {
        codec->codec_id = aiff_codec_get_id(codec->bits_per_coded_sample);
        codec->bits_per_coded_sample = cdx_av_get_bits_per_sample(codec->codec_id);
        aiff->block_duration = 1;
    } else {
        switch (codec->codec_id) {
        case AIFF_CODEC_ID_PCM_F32BE:
        case AIFF_CODEC_ID_PCM_F64BE:
        case AIFF_CODEC_ID_PCM_S16LE:
        case AIFF_CODEC_ID_PCM_ALAW:
        case AIFF_CODEC_ID_PCM_MULAW:
            aiff->block_duration = 1;
            break;
        case AIFF_CODEC_ID_ADPCM_IMA_QT:
            codec->block_align = 34*codec->channels;
            break;
        case AIFF_CODEC_ID_MACE3:
            codec->block_align = 2*codec->channels;
            break;
        case AIFF_CODEC_ID_ADPCM_G726LE:
            codec->bits_per_coded_sample = 5;
        case AIFF_CODEC_ID_ADPCM_G722:
        case AIFF_CODEC_ID_MACE6:
            codec->block_align = 1*codec->channels;
            break;
        case AIFF_CODEC_ID_GSM:
            codec->block_align = 33;
            break;
        default:
            aiff->block_duration = 1;
            break;
        }
        if (codec->block_align > 0)
            aiff->block_duration = cdx_av_get_audio_frame_duration(codec, codec->block_align);
    }

    /* Block align needs to be computed in all cases, as the definition
     * is specific to applications -> here we use the WAVE format definition */
    if (!codec->block_align)
        codec->block_align = (cdx_av_get_bits_per_sample(codec->codec_id) * codec->channels) >> 3;

    if (aiff->block_duration) {
        codec->bit_rate = codec->sample_rate * (codec->block_align << 3) /
                          aiff->block_duration;
    }

    /* Chunk is over */
    if (size)
        CdxStreamSeek(pb, size, SEEK_CUR);

    return num_frames;
}

static int AiffInit(CdxParserT* parameter)
{
    int size, filesize;
    int64_t offset = 0;
    uint32_t tag;
    unsigned version = AIFF_C_VERSION1;

    struct AiffParserImplS *impl = NULL;
    impl = (AiffParserImplS *)parameter;

    CdxStreamT* pb = impl->stream;
    AIFFInputContext *aiff = &impl->aiff;

    /* check FORM header */

    filesize = get_tag(pb, &tag);
    impl->dFileSize = filesize + 8;

    if (filesize < 0 || tag != MKTAG('F', 'O', 'R', 'M')){
          goto OPENFAILURE;
    }

    /* AIFF data type */
    tag = cdxio_rl32(pb);
    if (tag == MKTAG('A', 'I', 'F', 'F'))       /* Got an AIFF file */
        version = AIFF;
    else if (tag != MKTAG('A', 'I', 'F', 'C'))  /* An AIFF-C file then */
    {
        CDX_LOGD("LINE : %d",__LINE__);
        goto OPENFAILURE;
    }
    filesize -= 4;

    while (filesize > 0) {
        /* parse different chunks */
        size = get_tag(pb, &tag);

        if (size == -1 && offset > 0 && impl->avctx.block_align) {
            CDX_LOGD("header parser hit EOF\n");
            goto got_sound;
        }
        if (size < 0)
            return size;

        filesize -= size + 8;

        switch (tag) {
        case MKTAG('C', 'O', 'M', 'M'):     /* Common chunk */
            /* Then for the complete header info */
            impl->nb_frames = get_aiff_header(impl, size, version);
            if (impl->nb_frames < 0)
                return impl->nb_frames;
            if (offset > 0) // COMM is after SSND
                goto got_sound;
            break;
        case MKTAG('I', 'D', '3', ' '):
#ifdef PARSE_ID3
            position = CdxStreamTell(pb);
            ff_id3v2_read(s, ID3v2_DEFAULT_MAGIC, &id3v2_extra_meta, size);
            if (id3v2_extra_meta)
                if ((ret = ff_id3v2_parse_apic(s, &id3v2_extra_meta)) < 0) {
                    ff_id3v2_free_extra_meta(&id3v2_extra_meta);
                    return ret;
                }
            ff_id3v2_free_extra_meta(&id3v2_extra_meta);
            if (position + size > CdxStreamTell(pb))
                CdxStreamSeek(pb, position + size - CdxStreamTell(pb), SEEK_CUR);
#endif
            CdxStreamSeek(pb, size, SEEK_CUR);
            break;
        case MKTAG('F', 'V', 'E', 'R'):     /* Version chunk */
            version = cdxio_rb32(pb);
            break;
        case MKTAG('N', 'A', 'M', 'E'):     /* Sample name chunk */
            //get_meta(s, "title"    , size);
            CdxStreamSeek(pb, size, SEEK_CUR);
            break;
       case MKTAG('A', 'U', 'T', 'H'):     /* Author chunk */
            //get_meta(s, "author"   , size);
            CdxStreamSeek(pb, size, SEEK_CUR);
            break;
        case MKTAG('(', 'c', ')', ' '):     /* Copyright chunk */
            //get_meta(s, "copyright", size);
            CdxStreamSeek(pb, size, SEEK_CUR);
            break;
        case MKTAG('A', 'N', 'N', 'O'):     /* Annotation chunk */
            //get_meta(s, "comment"  , size);
            CdxStreamSeek(pb, size, SEEK_CUR);
            break;
        case MKTAG('S', 'S', 'N', 'D'):     /* Sampled sound chunk */
            aiff->data_end = CdxStreamTell(pb) + size;
            offset = cdxio_rb32(pb);      /* Offset of sound data */
            cdxio_rb32(pb);               /* BlockSize... don't care */
            offset += CdxStreamTell(pb);    /* Compute absolute data offset */
            if (impl->avctx.block_align)    /* Assume COMM already parsed */
                goto got_sound;
            CdxStreamSeek(pb, size - 8, SEEK_CUR);
            break;
        case MKTAG('w', 'a', 'v', 'e'):
            if ((uint64_t)size > (1<<30))
                return -1;
            if (cdx_get_extradata(&impl->avctx, pb, size) < 0){
                goto OPENFAILURE;
            }
            if (impl->avctx.codec_id == AIFF_CODEC_ID_QDM2 && size>=12*4 &&
                !impl->avctx.block_align)
            {
                impl->avctx.block_align = AV_RB32(impl->avctx.extradata+11*4);
                aiff->block_duration = AV_RB32(impl->avctx.extradata+9*4);
            } else if (impl->avctx.codec_id == AIFF_CODEC_ID_QCELP) {
                char rate = 0;
                if (size >= 25)
                    rate = impl->avctx.extradata[24];
                switch (rate) {
                case 'H': // RATE_HALF
                    impl->avctx.block_align = 17;
                    break;
                case 'F': // RATE_FULL
                default:
                    impl->avctx.block_align = 35;
                }
                aiff->block_duration = 160;
                impl->avctx.bit_rate = impl->avctx.sample_rate * (impl->avctx.block_align << 3) /
                                      aiff->block_duration;
            }
            break;
        case MKTAG('C','H','A','N'):
            if(cdx_mov_read_chan(pb, size) < 0){
                goto OPENFAILURE;
            }
            break;
        case 0:
            if (offset > 0 && impl->avctx.block_align) // COMM && SSND
                goto got_sound;
        default: /* Jump */
            if (size & 1)   /* Always even aligned */
                size++;
            CdxStreamSeek(pb, size, SEEK_CUR);
        }
    }

got_sound:
    if (!impl->avctx.block_align) {
        CDX_LOGW("could not find COMM tag or invalid block_align value\n");
        goto OPENFAILURE;
    }

    /* Now for that packet */
    switch (impl->avctx.codec_id) {
        case AIFF_CODEC_ID_ADPCM_IMA_QT:
        case AIFF_CODEC_ID_GSM:
        case AIFF_CODEC_ID_QDM2:
        case AIFF_CODEC_ID_QCELP:
            size = impl->avctx.block_align;
            break;
        default:
            size = (MAX_SIZE / impl->avctx.block_align) * impl->avctx.block_align;
            break;
     }
     impl->Framelen = size;
    /* Now positioned, get the sound data start and end */
    impl->ulDuration = impl->nb_frames * aiff->block_duration;
    /* Position the stream at the first block */
    CdxStreamSeek(pb, offset, SEEK_SET);
    impl->dFileOffset = offset;
    impl->dSndataOffset= offset;
    impl->mErrno = PSR_OK;
    CDX_LOGD("offset : %lld, Snd data end : %lld, dSndataOffset : %lld",
        offset, aiff->data_end, impl->dSndataOffset);
    pthread_cond_signal(&impl->cond);
    return 0;
OPENFAILURE:
    CDX_LOGW("AiffOpenThread fail!!!");
    impl->mErrno = PSR_OPEN_FAIL;
    pthread_cond_signal(&impl->cond);
    return -1;
}

static cdx_int32 __AiffParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    struct AiffParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct AiffParserImplS, base);
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
    impl->nFlags = cmd;

    return CDX_SUCCESS;
}

static cdx_int32 __AiffParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    struct AiffParserImplS *impl = NULL;
    int size = 0;
    impl = CdxContainerOf(parser, struct AiffParserImplS, base);

    size = impl->Framelen;
    if(CdxStreamEos(impl->stream)){
        CDX_LOGD("AiffParser Prefetch to the eos!");
        return CDX_FAILURE;
    }
    if(impl->aiff.data_end != 0)
    {
        int64_t curoff = CdxStreamTell(impl->stream);
        if(curoff + size > impl->aiff.data_end)
            size = impl->aiff.data_end - curoff;
        if(curoff == impl->aiff.data_end)
        {
            CDX_LOGD("AiffParser Prefetch to the data end!");
            return CDX_FAILURE;
        }
    }
    pkt->type = CDX_MEDIA_AUDIO;
    pkt->length = size;
    pkt->pts = (cdx_int64)impl->nFrames * 1000000 / impl->avctx.sample_rate;//-1;

    pkt->flags |= (FIRST_PART|LAST_PART);
    return CDX_SUCCESS;
}

static cdx_int32 __AiffParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    //cdx_int32 ret;
    cdx_int32 read_length;
    struct AiffParserImplS *impl = NULL;
    CdxStreamT *cdxStream = NULL;
    //unsigned char* ptr = (unsigned char*)pkt->buf;

    impl = CdxContainerOf(parser, struct AiffParserImplS, base);
    cdxStream = impl->stream;

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
        CDX_LOGD("CdxStreamRead fail");
        impl->mErrno = PSR_IO_ERR;
        return CDX_FAILURE;
    }
    else if(read_length == 0)
    {
       CDX_LOGD("CdxStream EOS");
       impl->mErrno = PSR_EOS;
       return CDX_FAILURE;
    }
    //logv("****len:%d,plen:%d",read_length,pkt->length);
    pkt->length = read_length;
    impl->dFileOffset += read_length;
    impl->nFrames += (read_length / impl->avctx.block_align);

    // TODO: fill pkt
    return CDX_SUCCESS;
}

static cdx_int32 __AiffParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    //cdx_int32 ret, i = 0;
    struct AiffParserImplS *impl;
    struct CdxProgramS *cdxProgram = NULL;

    impl = CdxContainerOf(parser, struct AiffParserImplS, base);
    memset(mediaInfo, 0x00, sizeof(*mediaInfo));

    if(impl->mErrno != PSR_OK)
    {
        CDX_LOGD("audio parse status no PSR_OK");
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
    mediaInfo->fileSize = impl->dFileSize;

    cdxProgram->duration = impl->ulDuration * 1000/ impl->avctx.sample_rate;
    cdxProgram->audio[0].eCodecFormat    = AUDIO_CODEC_FORMAT_PCM;
    cdxProgram->audio[0].eSubCodecFormat = aiff_get_audio_subfmt(impl->avctx.codec_id);
    cdxProgram->audio[0].nChannelNum     = impl->avctx.channels;
    cdxProgram->audio[0].nBitsPerSample  = impl->avctx.bits_per_coded_sample;
    cdxProgram->audio[0].nSampleRate     = impl->avctx.sample_rate;
    cdxProgram->audio[0].nAvgBitrate     = impl->avctx.bit_rate;
    //cdxProgram->audio[0].nMaxBitRate;
    //cdxProgram->audio[0].nFlags
    cdxProgram->audio[0].nBlockAlign     = impl->avctx.block_align;
    //CDX_LOGD("eSubCodecFormat:0x%04x,ch:%d,fs:%d",cdxProgram->audio[0].eSubCodecFormat,
              //cdxProgram->audio[0].nChannelNum,cdxProgram->audio[0].nSampleRate);
    return CDX_SUCCESS;
}

static cdx_int32 __AiffParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    struct AiffParserImplS *impl = NULL;
    cdx_int64 file_location = 0;
    cdx_int64 nFrames_off = 0;
    impl = CdxContainerOf(parser, struct AiffParserImplS, base);
    nFrames_off = timeUs * impl->avctx.sample_rate / 1000000;
    CDX_LOGD("nFrames_off : %lld", nFrames_off);
    file_location = impl->dSndataOffset +
                    (nFrames_off * impl->avctx.block_align)/impl->aiff.block_duration;
    if(file_location > impl->dFileSize)
    {
        CDX_LOGW("invalid position (out of bound) : %lld, bound : %lld"
                , file_location, impl->dFileSize);
        return CDX_FAILURE;
    }
    CdxStreamSeek(impl->stream, file_location, SEEK_SET);
    impl->nFrames = nFrames_off;
    return CDX_SUCCESS;
}

static cdx_uint32 __AiffParserAttribute(CdxParserT *parser)
{
    struct AiffParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct AiffParserImplS, base);
    return 0;
}

static cdx_int32 __AiffParserGetStatus(CdxParserT *parser)
{
    struct AiffParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct AiffParserImplS, base);
#if 0
    if (CdxStreamEos(impl->stream))
    {
        CDX_LOGE("file PSR_EOS! ");
        return PSR_EOS;
    }
#endif
    return impl->mErrno;
}

static cdx_int32 __AiffParserClose(CdxParserT *parser)
{
    struct AiffParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct AiffParserImplS, base);
    if(impl->avctx.extradata)
    {
        CdxFree(impl->avctx.extradata);
        impl->avctx.extradata = NULL;
        impl->avctx.extradata_size= 0;
    }
    CdxStreamClose(impl->stream);
    pthread_cond_destroy(&impl->cond);
    CdxFree(impl);
    return CDX_SUCCESS;
}

static struct CdxParserOpsS aiffParserOps =
{
    .control      = __AiffParserControl,
    .prefetch     = __AiffParserPrefetch,
    .read         = __AiffParserRead,
    .getMediaInfo = __AiffParserGetMediaInfo,
    .seekTo       = __AiffParserSeekTo,
    .attribute    = __AiffParserAttribute,
    .getStatus    = __AiffParserGetStatus,
    .close        = __AiffParserClose,
    .init         = AiffInit
};

static cdx_int32 AiffProbe(CdxStreamProbeDataT *p)
{
    /* check file header */
    if (p->buf[0] == 'F' && p->buf[1] == 'O' &&
        p->buf[2] == 'R' && p->buf[3] == 'M' &&
        p->buf[8] == 'A' && p->buf[9] == 'I' &&
        p->buf[10] == 'F' && (p->buf[11] == 'F' || p->buf[11] == 'C'))
        return CDX_TRUE;

    CDX_LOGD("audio probe fail!!!");
    return CDX_FALSE;
}

static cdx_uint32 __AiffParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_CHECK(probeData);
    if(probeData->len < 32)
    {
        CDX_LOGI("Probe data is not enough.");
        return 0;
    }

    if(!AiffProbe(probeData))
    {
        CDX_LOGI("wav probe failed.");
        return 0;
    }
    return 100;
}

static CdxParserT *__AiffParserOpen(CdxStreamT *stream, cdx_uint32 nFlags)
{
    //cdx_int32 ret = 0;
    struct AiffParserImplS *impl;
    impl = CdxMalloc(sizeof(*impl));
    (void)nFlags;

    memset(impl, 0x00, sizeof(*impl));
    impl->stream = stream;
    impl->base.ops = &aiffParserOps;
    pthread_cond_init(&impl->cond, NULL);
    //ret = pthread_create(&impl->openTid, NULL, WavOpenThread, (void*)impl);
    //CDX_FORCE_CHECK(!ret);
    impl->mErrno = PSR_INVALID;
    return &impl->base;
}

struct CdxParserCreatorS aiffParserCtor =
{
    .probe = __AiffParserProbe,
    .create  = __AiffParserOpen
};
