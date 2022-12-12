/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : oggparsevorbis.c
 * Description : oggparsevorbis
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "ParseOpus"
#include "CdxOggParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>


struct oggopus_private {
    int need_comments;
    unsigned pre_skip;
    int64_t cur_dts;
};

#define OPUS_SEEK_PREROLL_MS 80
#define OPUS_HEAD_SIZE 19

int cdx_vorbis_comment(CdxOggParser *ogg, AVMetadata **m, const cdx_uint8 *buf, int size);

void cdx_set_pts_info(AVStream *s, int pts_wrap_bits,
                     cdx_uint32 pts_num, cdx_uint32 pts_den);

static int opus_header(CdxOggParser *ogg, int idx)
{
    struct ogg_stream *os = ogg->streams + idx;
    AVStream *st    = os->stream;
    struct oggopus_private *priv = os->private;
    cdx_uint8 *packet = os->buf + os->pstart;
    cdx_uint8 *ptr = NULL;

    if (!priv) {
        priv = os->private = av_mallocz(sizeof(*priv));
        if (!priv)
            return -1;
    }

    ptr = packet + 8;
    if (os->flags & OGG_FLAG_BOS) {
        if (os->psize < OPUS_HEAD_SIZE || (Get8Bits(ptr) & 0xF0) != 0)
            return AVERROR_INVALIDDATA;

        st->codec->channels   = Get8Bits (ptr);
        //priv->pre_skip        = AV_RL16(packet + 10);
        //st->codec->delay      = priv->pre_skip;
        /*orig_sample_rate    = AV_RL32(packet + 12);*/
        /*gain                = AV_RL16(packet + 16);*/
        /*channel_map         = AV_RL8 (packet + 18);*/

        st->codec->extradata = av_mallocz(os->psize + CDX_INPUT_BUFFER_PADDING_SIZE);
        if (st->codec->extradata) {
            memset(st->codec->extradata + os->psize, 0, CDX_INPUT_BUFFER_PADDING_SIZE);
            st->codec->extradata_size = os->psize;
        } else {
            st->codec->extradata_size = 0;
            return AVERROR_ENOMEM;
        }

        memcpy(st->codec->extradata, packet, os->psize);

        st->codec->sample_rate = 48000;
        //av_codec_set_seek_preroll(st->codec,
                                 // av_rescale(OPUS_SEEK_PREROLL_MS,
                                             //st->codec->sample_rate, 1000));
        st->time_base.den = st->codec->sample_rate;
        st->time_base.num = 1;
        //cdx_set_pts_info(st, 64, 1, 48000);
        priv->need_comments = 1;

        st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
        st->codec->codec_id   = CDX_CODEC_ID_OPUS;
        ogg->hasAudio = 1;
        return 1;
    }
#if 1
    if (priv->need_comments) {
        if (os->psize < 8 || memcmp(packet, "OpusTags", 8)){
            CDX_LOGE("To fix me! Temporary thought it has shut down comment feature");
            priv->need_comments--;
            return 1;//AVERROR_INVALIDDATA;
        }
        cdx_vorbis_comment(ogg, &st->metadata, packet + 8, os->psize - 8);
        priv->need_comments--;
        return 1;
    }
#endif

    return 0;
}

static int opus_duration(uint8_t *src, int size)
{
    unsigned nb_frames  = 1;
    unsigned toc        = src[0];
    unsigned toc_config = toc >> 3;
    unsigned toc_count  = toc & 3;
    unsigned frame_size = toc_config < (unsigned)12 ?
                          FFMAX((unsigned)480, (unsigned)(960 * (toc_config & 3))) :
                          toc_config < (unsigned)16 ? (unsigned)480 << (unsigned)(toc_config & 1) :
                                            (unsigned)120 << (unsigned)(toc_config & 3);
    if (toc_count == 3) {
        if (size<2)
            return AVERROR_INVALIDDATA;
        nb_frames = src[1] & 0x3F;
    } else if (toc_count) {
        nb_frames = 2;
    }

    return frame_size * nb_frames;
}

static int opus_packet(CdxOggParser *ogg, int idx)
{
    struct ogg_stream *os = ogg->streams + idx;
    AVStream *st    = os->stream;
    struct oggopus_private *priv = os->private;
    uint8_t *packet              = os->buf + os->pstart;
    int ret;

    if (!os->psize)
        return AVERROR_INVALIDDATA;

    if ((!os->lastpts || os->lastpts == CDX_AV_NOPTS_VALUE) && !(os->flags & OGG_FLAG_EOS)) {
        int seg, d;
        int duration;
        uint8_t *last_pkt  = os->buf + os->pstart;
        uint8_t *next_pkt  = last_pkt;

        duration = 0;
        seg = os->segp;
        d = opus_duration(last_pkt, os->psize);
        if (d < 0) {
            //os->pflags |= AV_PKT_FLAG_CORRUPT;
            return 0;
        }
        duration += d;
        last_pkt = next_pkt =  next_pkt + os->psize;
        for (; seg < os->nsegs; seg++) {
            next_pkt += os->segments[seg];
            if (os->segments[seg] < 255 && next_pkt != last_pkt) {
                int d = opus_duration(last_pkt, next_pkt - last_pkt);
                if (d > 0)
                    duration += d;
                last_pkt = next_pkt;
            }
        }
        os->lastdts                 = os->granule - duration;
        os->lastpts                 = os->lastdts * 1000000LL / os->stream->codec->sample_rate;
    }

    if ((ret = opus_duration(packet, os->psize)) < 0)
        return ret;

    os->pduration = ret;
    if (os->lastdts != CDX_AV_NOPTS_VALUE) {
        if (st->start_time == CDX_AV_NOPTS_VALUE)
            st->start_time = os->lastdts;
        priv->cur_dts = os->lastdts -= priv->pre_skip;
    }

    priv->cur_dts += os->pduration;
    if ((os->flags & OGG_FLAG_EOS)) {
        cdx_int64 skip = priv->cur_dts - os->granule + priv->pre_skip;
        skip = FFMIN(skip, os->pduration);
        if (skip > 0) {
            os->pduration = skip < os->pduration ? os->pduration - skip : 1;
            //os->end_trimming = skip;
            CDX_LOGD("Last packet was truncated to %d due to end trimming",
                     os->pduration);
        }
    }

    return 0;
}

const struct ogg_codec cdx_ff_opus_codec = {
    .name             = "Opus",
    .magic            = "OpusHead",
    .magicsize        = 8,
    .header           = opus_header,
    .packet           = opus_packet,
    .nb_header        = 1,
};
