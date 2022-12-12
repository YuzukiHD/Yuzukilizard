/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxOggParser.c
 * Description : OggParser
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxOggParser"
#include "CdxOggParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#include <errno.h>

extern const struct ogg_codec cdx_ff_vorbis_codec;
extern const struct ogg_codec cdx_ff_ogm_video_codec;
extern const struct ogg_codec cdx_ff_ogm_audio_codec;
extern const struct ogg_codec cdx_ff_ogm_text_codec;
extern const struct ogg_codec cdx_ff_opus_codec;


static const struct ogg_codec * const cdx_ogg_codecs[] = {
    &cdx_ff_vorbis_codec,
    &cdx_ff_ogm_video_codec,
    &cdx_ff_ogm_audio_codec,
    &cdx_ff_ogm_text_codec,
    &cdx_ff_opus_codec,
    NULL
};

/*
static int ogg_read_close(CdxOggParser *ogg)
{
    int i;
    for (i = 0; i < ogg->nstreams; i++) {
        aw_freep(&ogg->streams[i].buf);
        if (ogg->streams[i].codec &&
            ogg->streams[i].codec->cleanup) {
            ogg->streams[i].codec->cleanup(s, i);
        }
        aw_freep(&ogg->streams[i].private);
    }
    aw_freep(&ogg->streams);
    return 0;
}
*/

static void avcodec_get_context_defaults2(AVCodecContext *ptr, enum CDXAVMediaType cdx_codec_type)
{
    //int flags=0;
    memset(ptr, 0, sizeof(AVCodecContext));

    ptr->codec_type = cdx_codec_type;

    ptr->time_base.num = 0;
    ptr->time_base.den = 1;
    ptr->sample_aspect_ratio.num = 0;
    ptr->sample_aspect_ratio.den = 1;
    ptr->reordered_opaque= CDX_AV_NOPTS_VALUE;
}

static inline AVCodecContext *avcodec_alloc_context2(enum CDXAVMediaType cdx_codec_type){
    AVCodecContext *ptr= malloc(sizeof(AVCodecContext));

    if(ptr==NULL) return NULL;

    avcodec_get_context_defaults2(ptr, cdx_codec_type);

    return ptr;
}

static inline AVCodecContext *avcodec_alloc_context(void){
    return avcodec_alloc_context2(AVMEDIA_TYPE_UNKNOWN);
}

static long long av_gcd(long long a, long long b){
    if(b) return av_gcd(b, a%b);
    else  return a;
}

static int av_reduce(int *dst_num, int *dst_den, long long num, long long den, long long max)
{
    AVRational a0={0,1}, a1={1,0};
    int sign= (num<0) ^ (den<0);
    long long gcd= av_gcd(FFABS(num), FFABS(den));
    //AVRational a1;

    if(gcd)
    {
        num = FFABS(num)/gcd;
        den = FFABS(den)/gcd;
    }
    if(num<=max && den<=max)
    {
       // a1= (AVRational){num, den};
        den=0;
    }

    while(den)
    {
        unsigned long long x      = num / den;
        long long next_den= num - den*x;
        long long a2n= x*a1.num + a0.num;
        long long a2d= x*a1.den + a0.den;

        if(a2n > max || a2d > max)
        {
            if(a1.num) x= (max - a0.num) / a1.num;
            if(a1.den) x= FFMIN(x, ((unsigned long long)max - a0.den) / a1.den);

            if (den*(2*(long long)x*a1.den + a0.den) > num*a1.den)
            {
                a1.num = x*a1.num + a0.num;
                a1.den = x*a1.den + a0.den;
                //a1 = (AVRational){x*a1.num + a0.num, x*a1.den + a0.den};
            }
            break;
        }

        a0= a1;
        //a1= (AVRational){a2n, a2d};
        a1.num = a2n;
        a1.den = a2d;
        num= den;
        den= next_den;
    }
    //assert(av_gcd(a1.num, a1.den) <= 1U);

    *dst_num = sign ? -a1.num : a1.num;
    *dst_den = a1.den;

    return den==0;
}

static void av_set_pts_info(AVStream *s, int pts_wrap_bits,
                     cdx_uint32 pts_num, cdx_uint32 pts_den)
{
    s->pts_wrap_bits = pts_wrap_bits;

    if(av_reduce(&s->time_base.num, &s->time_base.den, pts_num, pts_den, INT_MAX))
    {
        if(s->time_base.num != (int)pts_num)
        {
            //av_log(NULL, AV_LOG_DEBUG,
            //"st:%d removing common factor %d from timebase\n",
            //s->index, pts_num/s->time_base.num);
        }
    }
    else
    {
        //av_log(NULL, AV_LOG_WARNING, "st:%d has too large timebase, reducing\n", s->index);
    }

    if(!s->time_base.num || !s->time_base.den)
        s->time_base.num= s->time_base.den= 0;
}

static AVStream *av_new_stream(CdxOggParser *ogg, int id)
{
    AVStream *st;
    //int i;
    AVRational av_rational ={0,1};

    st = av_mallocz(sizeof(AVStream));
    if (!st)
        return NULL;

    st->codec= avcodec_alloc_context();
    st->codec->bit_rate = 0;
    st->id = id;
    st->duration = CDX_AV_NOPTS_VALUE;
    st->start_time = CDX_AV_NOPTS_VALUE;
    /* we set the current DTS to 0 so that formats without any timestamps
    but durations get some timestamps, formats with some unknown
    timestamps have their first few packets buffered and the
    timestamps corrected before they are returned to the user */
    st->cur_dts = 0;
    st->first_dts = CDX_AV_NOPTS_VALUE;

    /* default pts setting is MPEG-like */
    av_set_pts_info(st, 33, 0x01, 90000);

    /*
    for(i=0; i<MAX_REORDER_DELAY+1; i++)
        st->pts_buffer[i]= CDX_AV_NOPTS_VALUE;*/
    st->reference_dts = CDX_AV_NOPTS_VALUE;
    st->last_IP_pts = CDX_AV_NOPTS_VALUE;

    st->sample_aspect_ratio = av_rational;

    ogg->streams[id].stream = st;
    return st;
}

static int ogg_new_stream(CdxOggParser *ogg, uint32_t serial)
{
    int idx         = ogg->nstreams++;
    ogg->streams = realloc(ogg->streams, ogg->nstreams * sizeof(*ogg->streams));
    if(!ogg->streams)
    {
        CDX_LOGE("realloc fail");
        return -1;
    }
    struct ogg_stream *os = ogg->streams + idx;
    memset(os, 0, sizeof(*os));

    os->firstPacket = 1;
    os->serial        = serial;
    os->bufsize       = DECODER_BUFFER_SIZE;
    os->buf           = malloc(os->bufsize);
    if(!os->buf)
    {
        CDX_LOGE("malloc fail");
        return -1;
    }
    os->header        = -1;
    os->start_granule = OGG_NOGRANULE_VALUE;

    /* Create the associated AVStream */
    AVStream *st = av_new_stream (ogg, idx);
    if (!st)
    {
        CDX_LOGE("av_new_stream fail");
        return -1;
    }
    av_set_pts_info(st, 64, 1, 1000000);
    CDX_LOGD("serial=0x%x, idx=%d", serial, idx);
    return idx;
}
static const struct ogg_codec *ogg_search_codec(uint8_t *buf, int size)
{
    int i;
    for (i = 0; cdx_ogg_codecs[i]; i++)
        if (size >= cdx_ogg_codecs[i]->magicsize &&
            !memcmp(buf, cdx_ogg_codecs[i]->magic, cdx_ogg_codecs[i]->magicsize))
            return cdx_ogg_codecs[i];

    return NULL;
}

/**此时应是另一物理流，因为同一物理流的各逻辑流的bos是相邻的
 * Replace the current stream with a new one. This is a typical webradio
 * situation where a new audio stream spawn (identified with a new serial) and
 * must replace the previous one (track switch).
 */
static int ogg_replace_stream(CdxOggParser *ogg, uint32_t serial, int nsegs)
{
    const struct ogg_codec *codec;
    struct ogg_stream *os;
    int j = 0, ret;

    if(ogg->nstreams != 1 || !ogg->seekable)
    {
        CDX_LOGW("replace_stream is not supported in this situation");
        ogg->mErrno = PSR_EOS;
        return AVERROR_EOF;
    }

    uint8_t magic[8];
    int64_t pos = CdxStreamTell(ogg->file);
    if(pos < 0)
    {
        CDX_LOGE("CdxStreamTell fail, ret(%lld)", (long long int)pos);
        return pos;
    }
    ret = CdxStreamSeek(ogg->file, nsegs, STREAM_SEEK_CUR);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamSeek fail, ret(%d)", ret);
        return ret;
    }
    ret = CdxStreamRead(ogg->file, magic, sizeof(magic));
    if(ret < (int)sizeof(magic))
    {
        CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
        if(ret < 0)
        {
            return ret;
        }
        else
        {
            ogg->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return AVERROR_EOF;
        }
    }
    ret = CdxStreamSeek(ogg->file, pos, STREAM_SEEK_SET);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamSeek fail, ret(%d)", ret);
        return ret;
    }
    codec = ogg_search_codec(magic, sizeof(magic));
    if (!codec) {
        CDX_LOGE("Cannot identify new stream, maybe unsupported");

        ogg->mErrno = PSR_EOS;
        return AVERROR_EOF;
    }
    for (j = 0; j < ogg->nstreams; j++) {
        if (ogg->streams[j].codec == codec)
            break;
    }
    if (j >= ogg->nstreams)
    {
        CDX_LOGW("replace_stream fail");//getmediainfo只在开始时做过，所以对此newStream是无效的
        ogg->mErrno = PSR_EOS;
        return AVERROR_EOF;
    }

    os = &ogg->streams[j];

    os->serial  = serial;
    return j;

}

static int ogg_new_buf(CdxOggParser *ogg, int idx_os)
{
    struct ogg_stream *os = ogg->streams + idx_os;
    int size = os->bufpos - os->pstart;
    uint8_t *nb = malloc(os->bufsize);
    if(os->buf){
        memcpy(nb, os->buf + os->pstart, size);
        free(os->buf);
    }
    os->buf = nb;
    os->pstart = 0;
    os->bufpos = size;

    return 0;
}

static inline int ogg_find_stream (CdxOggParser *ogg, uint32_t serial)
{
    int i;
    for (i = 0; i < ogg->nstreams; i++)
        if ( serial == ogg->streams[i].serial )
            return i;

    return -1;
}
static int data_packets_seen(CdxOggParser *ogg)
{
    int j;

    for (j = 0; j < ogg->nstreams; j++)
        if (ogg->streams[j].got_data)
            return 1;
    return 0;
}

static int ogg_read_page(CdxOggParser *ogg, int *sid)
{
    CdxStreamT *bc = ogg->file;
    struct ogg_stream *os;
    int flags, nsegs;
    int ret, i = 0, j=0;
    uint64_t gp;
    uint32_t serial;
    cdx_uint32 size;
    int idx_os;
    uint8_t sync[4];
    uint8_t info[23];
    int sp = 0;

_nextPage:
    ret = CdxStreamRead(bc, sync, 4);
    if (ret < 4)
    {
        CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
        if(ret < 0)
        {
            return ret;
        }
        else
        {
            ogg->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return AVERROR_EOF;
        }
    }

    do {
        if (sync[sp & 3] == 'O' &&
            sync[(sp + 2) & 3] == 'g' &&
            sync[(sp + 1) & 3] == 'g' && sync[(sp + 3) & 3] == 'S')
            break;

        if(!i && ogg->seekable && ogg->page_pos > 0) {
            memset(sync, 0, 4);
            ret = CdxStreamSeek(bc, ogg->page_pos+4, SEEK_SET);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamSeek fail");
                return ret;
            }
            ogg->page_pos = -1;
        }

        ret = CdxStreamRead(bc, &sync[sp++ & 3], 1);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            return ret;
        }
        else if(ret == 0)
        {
            ogg->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return AVERROR_EOF;
        }
    } while (i++ < MAX_PAGE_SIZE);

    if (i >= MAX_PAGE_SIZE) {
        CDX_LOGE("cannot find sync word");
        return AVERROR_INVALIDDATA;
    }

    ret = CdxStreamRead(bc, info, 23);
    if (ret < 23)
    {
        CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
        if(ret < 0)
        {
            return ret;
        }
        else
        {
            ogg->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return AVERROR_EOF;
        }
    }

    cdx_uint8 *data = info;

    if(Get8Bits(data) != 0){      /* version */
        CDX_LOGE ("ogg page, unsupported version");
        return AVERROR_INVALIDDATA;
    }

    flags  = Get8Bits(data);
    gp     = GetLE64Bits(data);
    serial = GetLE32Bits(data);
    data += 8; /* seq, crc */
    nsegs  = Get8Bits(data);

    idx_os = ogg_find_stream(ogg, serial);
    if (idx_os < 0) {
        if (data_packets_seen(ogg))
            idx_os = ogg_replace_stream(ogg, serial, nsegs);
        else
            idx_os = ogg_new_stream(ogg, serial);

        if (idx_os < 0) {
            CDX_LOGE("failed to create or replace stream");
            return idx_os;
        }
    }

    os = ogg->streams + idx_os;
    cdx_int64 pos = CdxStreamTell(bc);
    if(pos < 0)
    {
        CDX_LOGE("CdxStreamTell fail");
        return pos;
    }
    ogg->page_pos = os->page_pos = pos - 27;

    if (os->psize > 0)
        ogg_new_buf(ogg, idx_os);

    ret = CdxStreamRead(bc, os->segments, nsegs);
    if (ret < nsegs)
    {
        CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
        if(ret < 0)
        {
            return ret;
        }
        else
        {
            ogg->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return AVERROR_EOF;
        }
    }

    size = 0;
    os->segp  = 0;
    os->nsegs = nsegs;

    for (j = 0; j < nsegs; j++)
        size = size + os->segments[j];

    if(os->invalid)
    {
        cdx_int64 stream_size = CdxStreamSize(bc);
        if(stream_size < 0)
        {
            CDX_LOGE("CdxStreamSize fail");
            return -1;
        }
        cdx_int64 stream_cur = CdxStreamTell(bc);
        if(stream_cur < 0)
        {
            CDX_LOGE("CdxStreamTell fail");
            return -1;
        }
        size = (size > (stream_size - stream_cur)) ? (stream_size - stream_cur) : size;
        ret = CdxStreamSkip(bc, size);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamSkip fail");
            return -1;
        }
        goto _nextPage;
    }

    if (!(flags & OGG_FLAG_BOS))
        os->got_data = 1;

    if (os->incomplete || (flags & OGG_FLAG_CONT)) {
        if (!os->psize) {
            // If this is the very first segment we started
            // playback in the middle of a continuation packet.
            // Discard it since we missed the start of it.
            while (os->segp < os->nsegs) {
                int seg_os = os->segments[os->segp++];
                os->pstart += seg_os;
                if (seg_os < 255)
                    break;
            }
            os->sync_pos = os->page_pos;
        }
    } else {
        os->sync_pos = os->page_pos;
        os->psize    = 0;
    }

    if (os->bufsize - os->bufpos < size) {
        uint8_t *nb = malloc (os->bufsize *= 2);

        if (!nb)
        {
            CDX_LOGE("malloc fail");
            return -1;
        }
        memcpy(nb, os->buf, os->bufpos);
        free(os->buf);
        os->buf = nb;
    }

    ret = CdxStreamRead(bc, os->buf + os->bufpos, size);//真的将page data读入
    if (ret < (int)size)
    {
        CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
        if(ret < 0)
        {
            return ret;
        }
        else
        {
            ogg->mErrno = PSR_EOS;
            CDX_LOGI("CdxStreamRead eos");
            return AVERROR_EOF;
        }
    }

    os->bufpos += size;
    os->granule = gp;
    os->flags   = flags;

    if (sid)
        *sid = idx_os;

    return 0;
}

static int ogg_packet(CdxOggParser *ogg, int *sid, int *dstart, int *dsize,
                      int64_t *fpos)
{
    int idx_os, j, ret;
    struct ogg_stream *os;
    int complete_os = 0;
    int segp = 0, psize = 0;

    CDX_LOGV("ogg_packet: curidx=%d", ogg->curidx);
    if (sid)
        *sid = -1;

    do {
        idx_os = ogg->curidx;

        while (idx_os < 0) {
            ret = ogg_read_page(ogg, &idx_os);
            if (ret < 0)
                return ret;
        }

        os = ogg->streams + idx_os;

        if (!os->codec) {
            if (os->header < 0) {
                os->codec = ogg_search_codec(os->buf, os->bufpos);
                if (!os->codec) {
                    CDX_LOGW("Codec not found, idx=%d", idx_os);
                    os->header = 0;
                    os->invalid = 1;
                    return 0;
                }
            } else {
                os->invalid = 1;
                return 0;
            }
        }

        segp  = os->segp;
        psize = os->psize;

        while (os->segp < os->nsegs) {
            int ss = os->segments[os->segp++];
            os->psize = os->psize + ss;
            if (ss < 255) {
                complete_os = 1;
                break;
            }
        }

        if (!complete_os && os->segp == os->nsegs) {
            ogg->curidx    = -1;
            // Do not set incomplete for empty packets.
            // Together with the code in ogg_read_page
            // that discards all continuation of empty packets
            // we would get an infinite loop.
            os->incomplete = !!os->psize;
        }
    } while (!complete_os);

    if (os->granule == -1)
        CDX_LOGW("Page at %lld is missing granule", (long long int)os->page_pos);

    os->incomplete = 0;
    ogg->curidx    = idx_os;

    if (os->header) {
        os->header = os->codec->header(ogg, idx_os);
        if (!os->header) {
            os->psize = psize;
            os->segp  = segp;

            // We have reached the first non-header packet in this stream.
            // Unfortunately more header packets may still follow for others,
            // but if we continue with header parsing we may lose data packets.
            ogg->headers = 1;

            // Update the header state for all streams and
            // compute the data_offset.
            if (!ogg->data_offset)
                ogg->data_offset = os->sync_pos;

            for (j = 0; j < ogg->nstreams; j++) {
                struct ogg_stream *cur_os = ogg->streams + j;

                // if we have a partial non-header packet, its start is
                // obviously at or after the data start
                if (cur_os->incomplete)
                    ogg->data_offset = FFMIN(ogg->data_offset, cur_os->sync_pos);
            }
        } else {
            os->nb_header++;
            os->pstart = os->pstart + os->psize;
            os->psize   = 0;

        }
    } else {
        os->pduration = 0;
        os->pflags    = 0;
        if (os->codec && os->codec->packet)
            os->codec->packet(ogg, idx_os);
        if (sid)
            *sid = idx_os;
        if (dstart)
            *dstart = os->pstart;
        if (fpos)
            *fpos = os->sync_pos;
        if (dsize)
            *dsize = os->psize;
        os->pstart  += os->psize;
        os->psize    = 0;
        if(os->pstart == os->bufpos)
        {
            os->bufpos = os->pstart = 0;
        }
        os->sync_pos = os->page_pos;
    }

    // determine whether there are more complete packets in this page
    // if not, the page's granule will apply to this packet
    os->page_end = 1;
    for (j = os->segp; j < os->nsegs; j++)
        if (os->segments[j] < 255) {
            os->page_end = 0;
            break;
        }

    if (os->segp == os->nsegs)
        ogg->curidx = -1;

    return 0;
}
static int ogg_reset(CdxOggParser *ogg)
{
    int64_t start_pos = CdxStreamTell(ogg->file);
    if(start_pos < 0)
    {
        CDX_LOGE("CdxStreamTell fail");
        return -1;
    }
    struct ogg_stream *os;
    int i;
    for (i = 0; i < ogg->nstreams; i++) {
        os = ogg->streams + i;
        os->bufpos     = 0;
        os->psize      = 0;
        os->pstart     = 0;
        os->granule    = -1;
        //os->lastpts    = CDX_AV_NOPTS_VALUE;
        os->lastpts = ogg->seekTimeUs;
        os->lastdts    = CDX_AV_NOPTS_VALUE;
        os->sync_pos   = -1;
        os->page_pos   = 0;
        os->segp       = 0;
        os->nsegs      = 0;
        os->incomplete = 0;
        os->got_data = 0;
        if (start_pos <= ogg->data_offset) {
            os->lastpts = 0;
        }
    }
    ogg->seekTimeUs = -1;

    ogg->page_pos = -1;
    ogg->curidx = -1;

    return 0;
}
static inline uint64_t
ogg_gptopts (CdxOggParser *ogg, int i, uint64_t gp, int64_t *pdts)
{
    struct ogg_stream *os = ogg->streams + i;
    uint64_t ipts = CDX_AV_NOPTS_VALUE;

    if(os->codec && os->codec->gptopts){
        ipts = os->codec->gptopts(ogg, i, gp, pdts);
    } else {
        ipts = gp;
        if (pdts)
            *pdts = ipts;
    }

    return ipts;
}
static int64_t ogg_get_duration(CdxOggParser *ogg)
{
    CDX_LOGD("ogg_get_duration start");
    if(ogg->fileSize <= 0)
    {
        CDX_LOGW("ogg->fileSize = %lld, ogg_get_duration fail.", ogg->fileSize);
        return 0;
    }
    if(!ogg->seekable)
    {
        CDX_LOGW("ogg->seekable = 0, ogg_get_duration fail.");
        return 0;
    }
    cdx_int64 pos = CdxStreamTell(ogg->file);
    if(pos < 0)
    {
        CDX_LOGE("CdxStreamTell fail, ret(%lld)", pos);
        return -1;
    }
    if(ogg->fileSize - pos < MAX_PAGE_SIZE)
    {
        CDX_LOGW("ogg->fileSize - pos < MAX_PAGE_SIZE");
        //return 0;
    }

    uint8_t sync[4];
    uint8_t info[23];
    int sp = 0;
    int flags, nsegs;
    uint64_t gp, finalGp = 0;
    uint32_t serial;
    cdx_uint32 size;
    int idx, finalIndex = -1;
    struct ogg_stream *os;

    uint8_t segments[255];
    int64_t durationUs = 0;

    int64_t aimPos = ogg->fileSize - pos < MAX_PAGE_SIZE ? -ogg->fileSize : -MAX_PAGE_SIZE;
    int64_t endPos = ogg->fileSize;
    int64_t tmpPos;
    int i, ret;
    cdx_uint8 *data;

_nextSeek:
    ret = CdxStreamSeek(ogg->file, aimPos, SEEK_END);

    if(ret < 0)
    {
        CDX_LOGE("CdxStreamSeek fail, ret(%d)", ret);
        return -1;
    }

    while(1)
    {
        i = 0;
        ret = CdxStreamRead(ogg->file, sync, 4);
        if (ret < 4)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                return -1;
            }
            else
            {
                break;
            }
        }

        do {
            if (sync[sp & 3] == 'O' &&
                sync[(sp + 2) & 3] == 'g' &&
                sync[(sp + 1) & 3] == 'g' && sync[(sp + 3) & 3] == 'S')
                break;

            ret = CdxStreamRead(ogg->file, &sync[sp++ & 3], 1);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
                return -1;
            }
            else if(ret == 0)
            {
                break;
            }
        } while (i++ < MAX_PAGE_SIZE);

        if (i >= MAX_PAGE_SIZE) {
            CDX_LOGE("cannot find sync word");
            return AVERROR_INVALIDDATA;
        }
        if(ret == 0)
        {
            break;
        }

        ret = CdxStreamRead(ogg->file, info, 23);
        if (ret < 23)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                return -1;
            }
            else
            {
                break;
            }
        }

        data = info;

        if(Get8Bits(data) != 0){      /* version */
            CDX_LOGE ("ogg page, unsupported version");
            return AVERROR_INVALIDDATA;
        }

        flags  = Get8Bits(data);
        gp     = GetLE64Bits(data);
        serial = GetLE32Bits(data);
        data += 8; /* seq, crc */
        nsegs  = Get8Bits(data);

        idx = ogg_find_stream(ogg, serial);
        if (idx < 0) {

            CDX_LOGE("ogg_find_stream fail");
            return -1;
        }

        os = ogg->streams + idx;

        ret = CdxStreamRead(ogg->file, segments, nsegs);
        if (ret < nsegs)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                return -1;
            }
            else
            {
                break;
            }
        }

        size = 0;
        for (i = 0; i < nsegs; i++)
            size += segments[i];

        if(!os->invalid && gp != (uint64_t)(-1) &&
            (os->stream->codec->codec_type == (ogg->hasVideo ?
                    AVMEDIA_TYPE_VIDEO:AVMEDIA_TYPE_AUDIO)))
        {
            finalIndex = idx;
            finalGp = gp;
            ogg->total_frame = finalGp;
        }
        cdx_int64 stream_cur = CdxStreamTell(ogg->file);
        if(stream_cur < 0)
        {
            CDX_LOGE("CdxStreamTell fail");
            return -1;
        }
        size = (size > (ogg->fileSize - stream_cur)) ? (ogg->fileSize - stream_cur) : size;
        ret = CdxStreamSkip(ogg->file, size);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamSkip fail");
            return -1;
        }
        tmpPos = CdxStreamTell(ogg->file);
        if(tmpPos >= endPos)
        {
            break;
        }
    }

    if(finalIndex < 0)
    {
        endPos = ogg->fileSize + aimPos + 17;
        aimPos -= MAX_PAGE_SIZE - 17;
        goto _nextSeek;
    }

    os = ogg->streams + finalIndex;

    CDX_LOGI("finalGp = %lld, os->stream->codec->codec_type=%d",
            finalGp, os->stream->codec->codec_type);

    if(os->stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        os->stream->codec->nFrameDuration =
                os->stream->codec->time_base.num * 1000000LL/ os->stream->codec->time_base.den;
        durationUs = os->stream->codec->nFrameDuration * finalGp;
    }
    else if(os->stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        durationUs = finalGp * 1000000LL / os->stream->codec->sample_rate;
    }
    else
    {
        CDX_LOGW("it is not supported");
    }

    ret = CdxStreamSeek(ogg->file, pos, SEEK_SET);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamSeek fail, ret(%d)", ret);
        return -1;
    }

    return durationUs;

}

static int ogg_read_header(CdxOggParser *ogg)
{
    int ret, j;
    ogg->curidx = -1;

    //linear headers seek from start
    do {
        ret = ogg_packet(ogg, NULL, NULL, NULL, NULL);
        if (ret < 0) {
            //ogg_read_close(ogg);
            return ret;
        }
    } while (!ogg->headers);
    CDX_LOGI("found headers");

    for (j = 0; j < ogg->nstreams; j++) {
        struct ogg_stream *os = ogg->streams + j;

        if (os->header < 0) {
            CDX_LOGE("Header parsing failed for stream %d", j);
            ogg->streams[j].codec = NULL;
        } else if (os->codec && (os->nb_header < os->codec->nb_header)) {
            CDX_LOGW("Headers mismatch for stream %d: expected %d received %d.",
                j, os->codec->nb_header, os->nb_header);
        }
        /*
        if (os->start_granule != OGG_NOGRANULE_VALUE)
            os->lastpts = os->stream->start_time =
                ogg_gptopts(ogg, i, os->start_granule, NULL);
                */
    }

    //linear granulepos seek from end
    int64_t durationUs = ogg_get_duration(ogg);
    if(durationUs < 0)
    {
        CDX_LOGE("ogg_get_duration fail, durationUs = %lld", (long long int)durationUs);
        return (int)durationUs;
    }
    else if(durationUs == 0)
    {
        CDX_LOGE("ogg_get_duration durationUs == 0");
    }
    ogg->durationUs = (cdx_uint64)durationUs;

    return 0;
}

static cdx_int64 find_next_pts(CdxOggParser *ogg, unsigned char* data, int data_size, int *tmp)
{
    unsigned char* pkt;
    unsigned char* dataEnd;
    int frameFound = 0;
    cdx_int64 val = -1;

    dataEnd  = data + data_size;
    *tmp = 0;

    for(pkt = data; pkt <= dataEnd - 17; pkt ++)
    {
        if(*pkt == 0x4F && *(pkt+1) == 0x67 && *(pkt+2) == 0x67 && *(pkt+3) == 0x53)
        {
            unsigned char* data = pkt + 14;
            struct ogg_stream *os = ogg->streams + ogg->seekStreamIndex;
            if(GetLE32Bits(data) == os->serial)
            {
                data = pkt + 6;
                val = GetLE64Bits(data);
                frameFound = 1;
                break;
            }
        }
    }

    if (frameFound == 1)
        *tmp = pkt - data;

    return val;
}

static int ogg_seek(CdxOggParser *ogg, cdx_int64 timeUs)
{
    int toleranceL, toleranceR;
    cdx_int64 dst_frame;
    struct ogg_stream *os;
    int i;
    if(ogg->hasVideo)
    {
        for (i = 0; i < ogg->nstreams; i++)
        {
            os = ogg->streams + i;
            if(os->stream->codec->codec_type == AVMEDIA_TYPE_VIDEO && !os->invalid)
            {
                break;
            }
        }
        if(i < ogg->nstreams)
        {
            VideoStreamInfo *video = &ogg->mediaInfo.program[0].video[0];
            dst_frame = timeUs/video->nFrameDuration;
            ogg->seekStreamIndex = i;
            os->keyframe_seek = 1;
            toleranceL = 25;//13;
            toleranceR = 0;
            goto _start;
        }
    }

    if(ogg->hasAudio)
    {
        CDX_LOGW("please check this code, when error happen");

        for (i = 0; i < ogg->nstreams; i++)
        {
            os = ogg->streams + i;
            if(os->stream->codec->codec_type == AVMEDIA_TYPE_AUDIO && !os->invalid)
            {
                break;
            }
        }
        if(i < ogg->nstreams)
        {
            AudioStreamInfo *audio = &ogg->mediaInfo.program[0].audio[0];
            dst_frame = timeUs * audio->nSampleRate /(1000*1000);
            ogg->seekStreamIndex = i;
            toleranceL = toleranceR = audio->nSampleRate/2;//audio->nSampleRate;
            goto _start;
        }
    }

    CDX_LOGE("should not be here");
    return -1;
_start:

    if(dst_frame < 0 || (cdx_uint64)dst_frame >= ogg->total_frame)
    {
        return -1;
    }

    cdx_int64 jump_file_pos = 0, pts_pos = 0;
    int tmp = 0, cnt = 0, pos_find = 0, ret;
    cdx_int64 guess_frame, diff_frame = 0;
    cdx_int64 practicalSize = ogg->fileSize - ogg->data_offset;
    jump_file_pos = ogg->data_offset + practicalSize * dst_frame / ogg->total_frame;
    do
    {
    reseek:
        if(jump_file_pos < 0)
        {
            jump_file_pos = 0;
        }
        ret = CdxStreamSeek(ogg->file, jump_file_pos, SEEK_SET);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamSeek fail");
            return -1;
        }
        ret = CdxStreamRead(ogg->file, ogg->buf, ogg->bufSize);
        if(ret < ogg->bufSize)
        {
            CDX_LOGW("CdxStreamRead fail, ret(%d)", ret);
            if(ret < 0)
            {
                return ret;
            }
        }

        guess_frame = find_next_pts(ogg, ogg->buf, ret, &tmp);
        CDX_LOGD("guess_frame=%lld", guess_frame);
        /*//在极端情况下，有死循环的风险
        if(guess_frame == 0)
        {
            if(ret < ogg->bufSize)
            {
                jump_file_pos -= (ogg->bufSize-17);
            }
            else
            {
                jump_file_pos += (ogg->bufSize-17);
            }
            goto reseek;
        }*/
        if(guess_frame == -1)
        {
            jump_file_pos -= (ogg->bufSize-17);
            if(jump_file_pos < ogg->data_offset)
            {
                jump_file_pos = ogg->data_offset;
            }
            goto reseek;
        }

        pts_pos = jump_file_pos + tmp;

        if(guess_frame < dst_frame)
        {
            diff_frame = dst_frame - guess_frame;
            if(diff_frame > toleranceL)
            {
                jump_file_pos += diff_frame * practicalSize/ogg->total_frame;
            }else
            {
                pos_find = 1;
                break;
            }
        }
        else if(guess_frame > dst_frame)
        {
            diff_frame = guess_frame - dst_frame;
            if(diff_frame > toleranceR)
            {
                jump_file_pos -= diff_frame * practicalSize/ogg->total_frame;
            }else
            {
                pos_find = 1;
                break;
            }
        }
        else
        {
            pos_find = 1;
            break;
        }

        cnt++;

    }while(cnt < 10);

    if(pos_find == 0)
    {
        CDX_LOGW("ogm_seek, pos_find == 0");
    }
    if(guess_frame != -1)
    {
        //os = ogg->streams + ogg->seekStreamIndex;
        if(os->stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            ogg->seekTimeUs = os->stream->codec->nFrameDuration * guess_frame;
        }
        else if(os->stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            ogg->seekTimeUs = guess_frame * 1000000LL / os->stream->codec->sample_rate;
        }
        else
        {
            CDX_LOGW("it is not supported");
        }
    }
    else
    {
        CDX_LOGW("should not be here.");
        ogg->seekTimeUs = timeUs;
    }
    if(pts_pos < ogg->data_offset)
    {
        pts_pos = ogg->data_offset;
    }
    ret = CdxStreamSeek(ogg->file, pts_pos, SEEK_SET);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamSeek fail");
        return -1;
    }

    ogg_reset(ogg);
    return 0;
}

static int64_t ogg_calc_pts(CdxOggParser *ogg, int idx_os, int64_t *dts)
{
    struct ogg_stream *os = ogg->streams + idx_os;
    int64_t pts           = CDX_AV_NOPTS_VALUE;

    if (dts)
        *dts = CDX_AV_NOPTS_VALUE;
    if(os->firstPacket)
    {
        pts = 0;
        os->firstPacket = 0;
    }
    if (os->lastpts != CDX_AV_NOPTS_VALUE) {
        pts         = os->lastpts;
        os->lastpts = CDX_AV_NOPTS_VALUE;
    }
    if (os->lastdts != CDX_AV_NOPTS_VALUE) {
        if (dts)
            *dts = os->lastdts;
        os->lastdts = CDX_AV_NOPTS_VALUE;
    }
    if (os->page_end && os->granule != -1LL) {
        if(os->stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            os->lastpts = os->stream->codec->nFrameDuration * os->granule;
        }
        else if(os->stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            os->lastpts = os->granule * 1000000LL / os->stream->codec->sample_rate;
        }
        else
        {
            CDX_LOGW("it is not supported");
        }
    }
    else
    {
        os->lastpts = CDX_AV_NOPTS_VALUE;
    }
    return pts;
}
static void ogg_validate_keyframe(CdxOggParser *ogg, int idx_os, int pstart, int psize)
{
    struct ogg_stream *os = ogg->streams + idx_os;
    if (psize && ogg->streams[idx_os].stream->codec->codec_id == CDX_CODEC_ID_THEORA) {
        if (!!(os->pflags & KEY_FRAME) != !(os->buf[pstart] & 0x40)) {
            os->pflags ^= KEY_FRAME;
            CDX_LOGW("Broken file, %skeyframe not correctly marked.",
                   (os->pflags & KEY_FRAME) ? "" : "non-");
        }
    }
}

static int ogg_read_packet(CdxOggParser *ogg, CdxPacketT *pkt)
{
    struct ogg_stream *os;
    int pstart, psize;
    int idx, ret;
    int64_t fpos, pts, dts;

    memset(pkt, 0x00, sizeof(CdxPacketT));

/*
    if (ogg->io_repositioned) {
        ogg_reset(ogg);
        ogg->io_repositioned = 0;
    }
*/

    //Get an ogg packet
retry:
    do {
        ret = ogg_packet(ogg, &idx, &pstart, &psize, &fpos);
        if (ret < 0)
            return ret;
    } while (idx < 0);

    os  = ogg->streams + idx;

    // pflags might not be set until after this
    pts = ogg_calc_pts(ogg, idx, &dts);
    ogg_validate_keyframe(ogg, idx, pstart, psize);

    if (!(os->pflags & KEY_FRAME) && os->keyframe_seek)
        goto retry;
    os->keyframe_seek = 0;

    //Alloc a pkt

    pkt->length = psize;
    pkt->pts = pts;
    pkt->flags = os->pflags;
    switch(os->stream->codec->codec_type)
    {
        case AVMEDIA_TYPE_VIDEO:
            pkt->type = CDX_MEDIA_VIDEO;
            break;
        case AVMEDIA_TYPE_AUDIO:
            pkt->type = CDX_MEDIA_AUDIO;
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            pkt->type = CDX_MEDIA_SUBTITLE;
            break;
        default:
            CDX_LOGW("CDX_MEDIA_UNKNOWN");
            goto retry;
    }
    pkt->streamIndex = os->streamIndex;

    ogg->curStream = idx;
    //CDX_LOGD("pkt->type = %d, ogg->curStream=%d, pts = %lld", pkt->type, ogg->curStream, pts);
    ogg->pstart = pstart;
    pkt->flags |= (FIRST_PART|LAST_PART);
    return 0;
}

static cdx_int32 OggParserPrefetch(CdxParserT *parser, CdxPacketT *cdxPkt)
{
    CdxOggParser *oggParser = (CdxOggParser*)parser;
    if(oggParser->status != CDX_PSR_IDLE && oggParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_IDLE && status != CDX_PSR_PREFETCHED, \
                  OggParserPrefetch invaild");
        oggParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(oggParser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(cdxPkt, &oggParser->cdxPkt, sizeof(CdxPacketT));
        return 0;
    }
    if(oggParser->packetPos < oggParser->packetNum)
    {
        memcpy(cdxPkt, oggParser->packets[oggParser->packetPos], sizeof(CdxPacketT));
        cdxPkt->buf = NULL;
        cdxPkt->buflen = 0;
        memcpy(&oggParser->cdxPkt, cdxPkt, sizeof(CdxPacketT));
        oggParser->status = CDX_PSR_PREFETCHED;
        return 0;
    }
    if(oggParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if((oggParser->flags & DISABLE_VIDEO) && (oggParser->flags & DISABLE_AUDIO))
    {
        CDX_LOGE("DISABLE_VIDEO && DISABLE_AUDIO");
        oggParser->mErrno = PSR_UNKNOWN_ERR;
        return -1;
    }

    pthread_mutex_lock(&oggParser->statusLock);
    if(oggParser->forceStop)
    {
        pthread_mutex_unlock(&oggParser->statusLock);
        return -1;
    }
    oggParser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&oggParser->statusLock);

    int ret = ogg_read_packet(oggParser, cdxPkt);
    pthread_mutex_lock(&oggParser->statusLock);
    if(ret < 0)
    {
        CDX_LOGE("ogg_read_packet fail.");
        oggParser->status = CDX_PSR_IDLE;
    }
    else
    {
        memcpy(&oggParser->cdxPkt, cdxPkt, sizeof(CdxPacketT));
        oggParser->status = CDX_PSR_PREFETCHED;
    }
    pthread_mutex_unlock(&oggParser->statusLock);
    pthread_cond_signal(&oggParser->cond);
    return ret;
}
static int VideoCodecMap(int codec_id)
{
    int vCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
    switch(codec_id)
    {
        case CDX_CODEC_ID_MPEG4:
        {
            vCodecFormat = VIDEO_CODEC_FORMAT_MPEG4;
            break;
        }
        case CDX_CODEC_ID_XVID:
        {
            vCodecFormat = VIDEO_CODEC_FORMAT_XVID;
            break;
        }
        case CDX_CODEC_ID_MSMPEG4V3:
        {
            vCodecFormat = VIDEO_CODEC_FORMAT_DIVX3;
            break;
        }

        case CDX_CODEC_ID_DIVX4:
        {
            vCodecFormat = VIDEO_CODEC_FORMAT_DIVX4;
            break;
        }

        case CDX_CODEC_ID_DIVX5:
        {
            vCodecFormat = VIDEO_CODEC_FORMAT_DIVX5;
            break;
        }

        default:
        {
            CDX_LOGE("VIDEO_CODEC_FORMAT_UNKNOWN, codec_id = %d", codec_id);
            vCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
            break;
        }
    }
    return vCodecFormat;
}
static int SetMediaInfo(CdxOggParser *oggParser)
{
    CdxMediaInfoT *mediaInfo = &oggParser->mediaInfo;
    mediaInfo->fileSize = oggParser->fileSize;
    mediaInfo->bSeekable = oggParser->seekable;
    struct CdxProgramS *program = &mediaInfo->program[0];
    program->duration = oggParser->durationUs / 1000;//ms
    CDX_LOGD("*********oggParser->durationUs=%llu", oggParser->durationUs);
    int i, j=0;
    struct ogg_stream *os;
    VideoStreamInfo *video = NULL;
    for (i = 0; i < oggParser->nstreams; i++)
    {
        os = oggParser->streams + i;
        if(os->invalid)
        {
            continue;
        }
        if(os->stream->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
           VIDEO_STREAM_LIMIT > program->videoNum++)
        {
            video = &program->video[0];

            int codec_id = os->stream->codec->codec_id;
            cdx_uint32 codec_tag = os->stream->codec->codec_tag;

            video->eCodecFormat = VideoCodecMap(codec_id);
            if (video->eCodecFormat == VIDEO_CODEC_FORMAT_MPEG4 &&
                codec_tag == MKTAG('D','I','V','X'))
            {
                video->eCodecFormat = VIDEO_CODEC_FORMAT_XVID;
            }

            if(video->eCodecFormat == VIDEO_CODEC_FORMAT_UNKNOWN)
            {
                os->invalid = 1;
                continue;
            }

            video->nWidth = os->stream->codec->width;
            video->nHeight = os->stream->codec->height;
            video->nFrameDuration = os->stream->codec->nFrameDuration =
                os->stream->codec->time_base.num * 1000000LL/ os->stream->codec->time_base.den;
            /*两帧画面之间的间隔时间，是帧率的倒数，单位为us*/
            video->nFrameRate = 1000 * 1000000/video->nFrameDuration;
            /*表示每1000秒有多少帧画面被播放，例如25000、29970、30000等*/
            video->nCodecSpecificDataLen = oggParser->tempVideoInfo.nCodecSpecificDataLen;
            video->pCodecSpecificData = oggParser->tempVideoInfo.pCodecSpecificData;
            os->streamIndex = 0;
        }
        else if(os->stream->codec->codec_type == AVMEDIA_TYPE_AUDIO && AUDIO_STREAM_LIMIT > j)
        {
            int codec_id = os->stream->codec->codec_id;
            switch(codec_id)
            {
                case CDX_CODEC_ID_VORBIS:
                    program->audio[j].eCodecFormat = AUDIO_CODEC_FORMAT_OGG;
                    break;
                case CDX_CODEC_ID_MP3:
                    program->audio[j].eCodecFormat = AUDIO_CODEC_FORMAT_MP3;
                    break;
                case CDX_CODEC_ID_AAC:
                    program->audio[j].eCodecFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
                    break;
                case CDX_CODEC_ID_OPUS:
                    program->audio[j].eCodecFormat = AUDIO_CODEC_FORMAT_OPUS;
                    break;
                default:
                    program->audio[j].eCodecFormat = AUDIO_CODEC_FORMAT_UNKNOWN;
                    CDX_LOGE("AUDIO_CODEC_FORMAT_UNKNOWN, codec_id = %d", codec_id);
                    break;
            }

            if(program->audio[j].eCodecFormat == AUDIO_CODEC_FORMAT_UNKNOWN)
            {
                os->invalid = 1;
                continue;
            }
            if(program->audio[j].eCodecFormat == AUDIO_CODEC_FORMAT_MPEG_AAC_LC)
                program->audio[j].nFlags |= 1;

            if (os->stream->metadata!=NULL)
            {
                int k;
                for(k = 0; k < os->stream->metadata->count; k++)
                {
                    if(!strcmp(os->stream->metadata->elems->key,"LANGUAGE"))
                    {
                        strcpy((char *)program->audio[j].strLang,
                               os->stream->metadata->elems->value);
                    }
                }
            }

            if(!strcmp(os->codec->magic, "\001vorbis"))
            {
                struct oggvorbis_private *priv = os->private;
                os->extradata_size = priv->len[0] + priv->len[2];
                os->extradata = malloc(os->extradata_size);
                if(!os->extradata)
                {
                    CDX_LOGE("!!!malloc fail");
                }
                else
                {
                    //CDX_LOGD("priv=%p, os->extradata=%p, priv->packet[0]=%p, priv->len[0]=%d",
                    //priv,os->extradata,priv->packet[0], priv->len[0]);
                    memcpy(os->extradata, priv->packet[0], priv->len[0]);

                    //CDX_BUF_DUMP(priv->packet[0], priv->len[0]);
                    //CDX_LOGD("os->extradata=%p, priv->packet[0]=%p, priv->len[0]=%d",
                    //os->extradata,priv->packet[2], priv->len[2]);
                    memcpy(os->extradata + priv->len[0], priv->packet[2], priv->len[2]);

                    //CDX_BUF_DUMP(priv->packet[2], priv->len[2]);

                    program->audio[j].nCodecSpecificDataLen = os->extradata_size;
                    program->audio[j].pCodecSpecificData = os->extradata;

                    //CDX_LOGD("os->extradata_size = %d,
                    //program->audio[j].pCodecSpecificData=%p, j=%d",
                    //os->extradata_size, program->audio[j].pCodecSpecificData,j);
                }
            }

            program->audio[j].nChannelNum = os->stream->codec->channels;
            program->audio[j].nSampleRate = os->stream->codec->sample_rate;
            //program->audio[j].nBitsPerSample = 0;
            //os->stream->codec->bit_rate / os->stream->codec->sample_rate;

            os->streamIndex = j++;
        }
        else if(os->stream->codec->codec_type == AVMEDIA_TYPE_SUBTITLE &&
                SUBTITLE_STREAM_LIMIT > program->subtitleNum++)
        {
            CDX_LOGW("subtitle is not support yet.");
            os->invalid = 1;
        }
        else
        {
            os->invalid = 1;
            continue;
        }
    }
    program->audioNum = j;
    PrintMediaInfo(mediaInfo);

#if OGG_SAVE_VIDEO_STREAM
    oggParser->fpVideoStream[0] = fopen("/data/camera/ogg_videostream.es", "wb+");
    if (!oggParser->fpVideoStream[0])
    {
        CDX_LOGE("open video stream debug file failure errno(%d)", errno);
    }
    if(video && video->bIs3DStream)
    {
        oggParser->fpVideoStream[1] = fopen("/data/camera/ogg_minorvideostream.es", "wb+");
        if (!oggParser->fpVideoStream[1])
        {
            CDX_LOGE("open video stream debug file failure errno(%d)", errno);
        }
    }
#endif
#if OGG_SAVE_AUDIO_STREAM
    for(i = 0; i < program->audioNum && i < AUDIO_STREAM_LIMIT; i++)
    {
        sprintf(oggParser->url, "/data/camera/demux_audiostream_%d.es", i);
        oggParser->fpAudioStream[i] = fopen(oggParser->url, "wb+");
        if (!oggParser->fpAudioStream[i])
        {
            CDX_LOGE("open audio stream debug file failure errno(%d)", errno);
        }
        //CDX_LOGD("******program->audio[i].nCodecSpecificDataLen = %d,
        //program->audio[i].pCodecSpecificData=%p, i=%d",
        //program->audio[i].nCodecSpecificDataLen, program->audio[i].pCodecSpecificData, i);
        if(oggParser->fpAudioStream[i] &&
           program->audio[i].pCodecSpecificData &&
           program->audio[i].nCodecSpecificDataLen)
        {
            //CDX_LOGD("******program->audio[i].nCodecSpecificDataLen = %d,
            //program->audio[i].pCodecSpecificData=%p",
            //program->audio[i].nCodecSpecificDataLen,
            //program->audio[i].pCodecSpecificData);
            int ret = fwrite(program->audio[i].pCodecSpecificData, 1,
                             program->audio[i].nCodecSpecificDataLen, oggParser->fpAudioStream[i]);
            sync();
            //CDX_LOGD("pos = %d, ret = %d", pos, ret);
            //CDX_BUF_DUMP(program->audio[i].pCodecSpecificData,
            //program->audio[i].nCodecSpecificDataLen);
        }
    }
#endif

    return 0;
}
static cdx_int32 ReadPacket(CdxOggParser *oggParser, CdxPacketT *pkt)
{
    struct ogg_stream *os  = oggParser->streams + oggParser->curStream;
    cdx_uint8 *data = os->buf + oggParser->pstart;

    if(pkt->length <= pkt->buflen)
    {
        memcpy(pkt->buf, data, pkt->length);
    }
    else
    {
        memcpy(pkt->buf, data, pkt->buflen);
        memcpy(pkt->ringBuf, data + pkt->buflen,
                pkt->length - pkt->buflen);
    }

#if OGG_SAVE_VIDEO_STREAM
    if(pkt->type == CDX_MEDIA_VIDEO)
    {
        if(oggParser->fpVideoStream[pkt->streamIndex])
        {
            fwrite(data, 1, pkt->length, oggParser->fpVideoStream[pkt->streamIndex]);
            sync();
        }
        else
        {
            CDX_LOGE("demux->fpVideoStream == NULL");
        }

    }
#endif
#if OGG_SAVE_AUDIO_STREAM
    if(pkt->type == CDX_MEDIA_AUDIO)
    {
        if(oggParser->fpAudioStream[pkt->streamIndex])
        {
            fwrite(data, 1, pkt->length, oggParser->fpAudioStream[pkt->streamIndex]);
            sync();
        }
        else
        {
            CDX_LOGE("demux->fpAudioStream == NULL");
        }

    }
#endif

    return 0;
}
static int MakeSpecificData(CdxOggParser *oggParser)
{
    int ret = 0;
    //ProbeSpecificDataBuf *vProbeBuf = &oggParser->vProbeBuf;
    CdxPacketT *pkt;
    unsigned int vCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
    int i;
    int haveVideo = 0;
    struct ogg_stream *os;
    for (i = 0; i < oggParser->nstreams; i++)
    {
        os = oggParser->streams + i;
        if(os->invalid)
        {
            continue;
        }
        if(os->stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            int codec_id = os->stream->codec->codec_id;
            cdx_uint32 codec_tag = os->stream->codec->codec_tag;

            vCodecFormat = VideoCodecMap(codec_id);
            if (vCodecFormat == VIDEO_CODEC_FORMAT_MPEG4 &&
                codec_tag == MKTAG('D','I','V','X'))
            {
                vCodecFormat = VIDEO_CODEC_FORMAT_XVID;
            }
            else if (vCodecFormat == VIDEO_CODEC_FORMAT_DIVX3 &&
                codec_tag == MKTAG('D', 'I', 'V', '3'))
            {
                //DIVX3 no need specific data so return immediatly
                vCodecFormat = VIDEO_CODEC_FORMAT_DIVX3;
                return PROBE_SPECIFIC_DATA_SUCCESS;
            }

            if(vCodecFormat == VIDEO_CODEC_FORMAT_UNKNOWN)
            {
                os->invalid = 1;
                continue;
            }
            haveVideo = 1;
            break;
        }
    }
    if(!haveVideo)
    {
        return PROBE_SPECIFIC_DATA_SUCCESS;
    }
    while(1)
    {
        pkt = NULL;
        if(oggParser->forceStop)
        {
            CDX_LOGE("PSR_USER_CANCEL");
            goto _exit;
        }

        pkt = (CdxPacketT *)malloc(sizeof(CdxPacketT));
        if(!pkt)
        {
            CDX_LOGE("malloc fail.");
            goto _exit;
        }
        ret = ogg_read_packet(oggParser, pkt);
        if(ret < 0)
        {
            CDX_LOGW("PrefetchPacket fail.");
            goto _exit;
        }
        pkt->buf = (char *)malloc(pkt->length);
        if(!pkt->buf)
        {
            CDX_LOGE("malloc fail.");
            goto _exit;
        }
        pkt->buflen = pkt->length;
        ret = ReadPacket(oggParser, pkt);
        if(ret < 0)
        {
            CDX_LOGE("ogg_read_packet fail");
            goto _exit;
        }
        if(oggParser->packetNum >= MaxProbePacketNum)
        {
            CDX_LOGE("oggParser->probePacketNum >= MaxProbePacketNum");
            goto _exit;
        }
        oggParser->packets[oggParser->packetNum++] = pkt;
        if(pkt->type == CDX_MEDIA_VIDEO)
        {
            if(pkt->length + oggParser->vProbeBuf.probeDataSize >
               oggParser->vProbeBuf.probeBufSize)
            {
                CDX_LOGE("probeDataSize too big!");
                goto _exit1;
            }
            else
            {
                cdx_uint8 *data =
                        oggParser->vProbeBuf.probeBuf + oggParser->vProbeBuf.probeDataSize;
                memcpy(data, pkt->buf, pkt->length);
                oggParser->vProbeBuf.probeDataSize += pkt->length;
            }
            ret = ProbeVideoSpecificData(&oggParser->tempVideoInfo,
                                         oggParser->vProbeBuf.probeBuf,
                                         oggParser->vProbeBuf.probeDataSize,
                                         vCodecFormat,
                                         CDX_PARSER_OGG);
            if(ret == PROBE_SPECIFIC_DATA_ERROR)
            {
                CDX_LOGE("probeVideoSpecificData error");
                goto _exit1;
            }
            else if(ret == PROBE_SPECIFIC_DATA_SUCCESS)
            {
                return ret;
            }
            else if(ret == PROBE_SPECIFIC_DATA_NONE)
            {
                oggParser->vProbeBuf.probeDataSize = 0;
            }
            else if(ret == PROBE_SPECIFIC_DATA_UNCOMPELETE)
            {

            }
            else
            {
                CDX_LOGE("probeVideoSpecificData (%d), it is unknown.", ret);
            }
        }
    }

_exit:
    if(pkt)
    {
        if(pkt->buf)
        {
            free(pkt->buf);
        }
        free(pkt);
    }
_exit1:
    return ret;
}

int OggParserInit(CdxParserT *parser)
{
    CDX_LOGI("OggParserInit start");

    CdxOggParser *oggParser = (CdxOggParser *)parser;

    oggParser->fileSize = CdxStreamSize(oggParser->file);
    oggParser->seekable = CdxStreamSeekAble(oggParser->file);

    int ret = ogg_read_header(oggParser);
    if(ret < 0)
    {
        CDX_LOGE("ogg_read_header fail");
        goto _exit;
    }
    ret = MakeSpecificData(oggParser);
    if(ret != PROBE_SPECIFIC_DATA_SUCCESS)
    {
        CDX_LOGE("MakeSpecificData fail");
        goto _exit;
    }
    SetMediaInfo(oggParser);

    oggParser->seekTimeUs = -1;

    oggParser->mErrno = PSR_OK;
    pthread_mutex_lock(&oggParser->statusLock);
    oggParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&oggParser->statusLock);
    pthread_cond_signal(&oggParser->cond);
    CDX_LOGI("CdxOggOpenThread success");
    return 0;

_exit:
    oggParser->mErrno = PSR_OPEN_FAIL;
    pthread_mutex_lock(&oggParser->statusLock);
    oggParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&oggParser->statusLock);
    pthread_cond_signal(&oggParser->cond);
    CDX_LOGI("CdxOggOpenThread fail");
    return -1;
}

static cdx_int32 OggParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxOggParser *oggParser = (CdxOggParser*)parser;
    if(oggParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_PREFETCHED, we can not read!");
        oggParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(oggParser->packetPos < oggParser->packetNum)
    {

        cdx_uint8 *data = (cdx_uint8 *)oggParser->packets[oggParser->packetPos]->buf;
        if(pkt->length <= pkt->buflen)
        {
            memcpy(pkt->buf, data, pkt->length);
        }
        else
        {
            memcpy(pkt->buf, data, pkt->buflen);
            memcpy(pkt->ringBuf, data + pkt->buflen, pkt->length - pkt->buflen);
        }
        if(oggParser->packets[oggParser->packetPos])
        {
            if(oggParser->packets[oggParser->packetPos]->buf)
            {
                free(oggParser->packets[oggParser->packetPos]->buf);
            }
            free(oggParser->packets[oggParser->packetPos]);
            oggParser->packets[oggParser->packetPos] = NULL;
        }
        oggParser->packetPos++;
        oggParser->status = CDX_PSR_IDLE;
        return 0;
    }
    int ret = ReadPacket(oggParser, pkt);
    oggParser->status = CDX_PSR_IDLE;
    return ret;
}

cdx_int32 OggParserForceStop(CdxParserT *parser)
{
    CDX_LOGD("OggParserForceStop start");
    CdxOggParser *oggParser = (CdxOggParser*)parser;
    pthread_mutex_lock(&oggParser->statusLock);
    oggParser->forceStop = 1;
    oggParser->mErrno = PSR_USER_CANCEL;
    int ret = CdxStreamForceStop(oggParser->file);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamForceStop fail");
        //oggParser->mErrno = PSR_UNKNOWN_ERR;
        //return -1;
    }
    while(oggParser->status != CDX_PSR_IDLE && oggParser->status != CDX_PSR_PREFETCHED)
    {
        pthread_cond_wait(&oggParser->cond, &oggParser->statusLock);
    }
    pthread_mutex_unlock(&oggParser->statusLock);

    oggParser->mErrno = PSR_USER_CANCEL;
    oggParser->status = CDX_PSR_IDLE;
    CDX_LOGD("OggParserForceStop end");
    return 0;
}
cdx_int32 OggParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGV("OggParserClrForceStop start");
    CdxOggParser *oggParser = (CdxOggParser*)parser;
    if(oggParser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("oggParser->status != CDX_PSR_IDLE");
        oggParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    oggParser->forceStop = 0;
    int ret = CdxStreamClrForceStop(oggParser->file);
    if(ret < 0)
    {
        CDX_LOGW("CdxStreamClrForceStop fail");
    }
    CDX_LOGI("OggParserClrForceStop end");
    return 0;
}
static int OggParserControl(CdxParserT *parser, int cmd, void *param)
{
    CdxOggParser *oggParser = (CdxOggParser*)parser;
    (void)oggParser;
    (void)param;

    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI("pmp parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            return OggParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return OggParserClrForceStop(parser);
        default:
            break;
    }
    return 0;
}

cdx_int32 OggParserGetStatus(CdxParserT *parser)
{
    CdxOggParser *oggParser = (CdxOggParser *)parser;
    return oggParser->mErrno;
}

cdx_int32 OggParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    CDX_LOGD("OggParserSeekTo start, timeUs = %lld", timeUs);
    CdxOggParser *oggParser = (CdxOggParser *)parser;
    oggParser->mErrno = PSR_OK;
    oggParser->status = CDX_PSR_IDLE;
    oggParser->packetPos = oggParser->packetNum;
    if(timeUs < 0)
    {
        CDX_LOGE("timeUs invalid");
        oggParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    else if((cdx_uint64)timeUs >= oggParser->durationUs)
    {
        CDX_LOGI("PSR_EOS");
        oggParser->mErrno = PSR_EOS;
        return 0;
    }
    if(!oggParser->seekable)
    {
        CDX_LOGE("seekable = false");
        oggParser->mErrno = PSR_UNKNOWN_ERR;
        return -1;
    }

    pthread_mutex_lock(&oggParser->statusLock);
    if(oggParser->forceStop)
    {
        oggParser->mErrno = PSR_USER_CANCEL;
        pthread_mutex_unlock(&oggParser->statusLock);
        return -1;
    }
    oggParser->status = CDX_PSR_SEEKING;
    pthread_mutex_unlock(&oggParser->statusLock);

    int ret = ogg_seek(oggParser, timeUs);
    if(ret < 0)
    {
        CDX_LOGE("ogg_seek fail, ret(%d)", ret);
    }
    //oggParser->io_repositioned = 1;

    pthread_mutex_lock(&oggParser->statusLock);
    oggParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&oggParser->statusLock);
    pthread_cond_signal(&oggParser->cond);
    return ret;
}
static cdx_int32 OggParserClose(CdxParserT *parser)
{
    CdxOggParser *oggParser = (CdxOggParser *)parser;
    int ret = OggParserForceStop(parser);
    if(ret < 0)
    {
        CDX_LOGW("PmpParserForceStop fail");
    }

    CdxStreamClose(oggParser->file);

    int i,j;
    struct ogg_stream *os;
    AVStream *stream;
    for (i = 0; i < oggParser->nstreams; i++) {
        os = oggParser->streams + i;
        if(os->buf)
        {
            free(os->buf);
        }
        if (os->codec &&
            os->codec->cleanup) {
            os->codec->cleanup(oggParser, i);
        }

        if(os->private)
        {
            if(os->codec == &cdx_ff_vorbis_codec)
            {
                struct oggvorbis_private *priv = os->private;
                int k;
                for(k = 0; k < 3; k++)
                {
                    if(priv->packet[k])
                    {
                        free(priv->packet[k]);
                    }
                }

            }
            free(os->private);
        }
        if(os->extradata)
        {
            free(os->extradata);
        }

        stream = os->stream;
        if(stream)
        {
            if(stream->codec)
            {
                if(stream->codec->extradata)
                {
                    free(stream->codec->extradata);
                }
                free(stream->codec);
            }
            if(stream->metadata)
            {
                for(j=0; j<stream->metadata->count; j++)
                {
                    if(stream->metadata->elems+j)
                    {
                        if(stream->metadata->elems[j].key)
                        {
                            free(stream->metadata->elems[j].key);
                        }
                        if(stream->metadata->elems[j].value)
                        {
                            free(stream->metadata->elems[j].value);
                        }
                    }
                }

                if(stream->metadata->elems)
                {
                    free(stream->metadata->elems);
                }

                free(stream->metadata);
            }
            free(stream);
        }
    }

    if(oggParser->streams)
    {
        free(oggParser->streams);
    }

    for(i=0; i<(int)oggParser->nb_chapters; i++)
    {
        if(oggParser->chapters[i])
        {
            if(oggParser->chapters[i]->metadata)
            {
                if(oggParser->chapters[i]->metadata->elems)
                {
                    for(j=0; j<oggParser->chapters[i]->metadata->count; j++)
                    {
                        if(oggParser->chapters[i]->metadata->elems[j].key)
                        {
                            free(oggParser->chapters[i]->metadata->elems[j].key);
                        }
                        if(oggParser->chapters[i]->metadata->elems[j].value)
                        {
                            free(oggParser->chapters[i]->metadata->elems[j].value);
                        }

                    }
                    free(oggParser->chapters[i]->metadata->elems);
                }

                free(oggParser->chapters[i]->metadata);
            }
            free(oggParser->chapters[i]);
        }
    }
    if(oggParser->chapters)
    {
        free(oggParser->chapters);
    }

#if OGG_SAVE_VIDEO_STREAM
    if(oggParser->fpVideoStream[0])
    {
        fclose(oggParser->fpVideoStream[0]);
    }
    if(oggParser->fpVideoStream[1])
    {
        fclose(oggParser->fpVideoStream[1]);
    }
#endif
#if OGG_SAVE_AUDIO_STREAM
    for(i = 0; i < AUDIO_STREAM_LIMIT; i++)
    {
        if(oggParser->fpAudioStream[i])
        {
            fclose(oggParser->fpAudioStream[i]);
        }
    }
#endif

    for(i = 0; i < (int)oggParser->packetNum; i++)
    {
        if(oggParser->packets[i])
        {
            if(oggParser->packets[i]->buf)
            {
                free(oggParser->packets[i]->buf);
            }
            free(oggParser->packets[i]);
        }
    }
    if(oggParser->vProbeBuf.probeBuf)
    {
        free(oggParser->vProbeBuf.probeBuf);
    }
    if(oggParser->tempVideoInfo.pCodecSpecificData)
    {
        free(oggParser->tempVideoInfo.pCodecSpecificData);
    }

    free(oggParser->buf);
    pthread_mutex_destroy(&oggParser->statusLock);
    pthread_cond_destroy(&oggParser->cond);
    free(oggParser);
    return 0;
}
static cdx_int32 OggParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    CdxOggParser *oggParser = (CdxOggParser*)parser;
    if(oggParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGE("status < CDX_PSR_IDLE, OggParserGetMediaInfo invaild");
        oggParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    memcpy(pMediaInfo, &oggParser->mediaInfo, sizeof(CdxMediaInfoT));
    /*for the request from ericwang, for */
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    /**/
    return 0;
}

cdx_uint32 OggParserAttribute(CdxParserT *parser)
{
    CdxOggParser *oggParser = (CdxOggParser *)parser;
    return oggParser->flags;
}

static struct CdxParserOpsS oggParserOps =
{
    .control         = OggParserControl,
    .prefetch         = OggParserPrefetch,
    .read             = OggParserRead,
    .getMediaInfo     = OggParserGetMediaInfo,
    .close             = OggParserClose,
    .seekTo            = OggParserSeekTo,
    .attribute        = OggParserAttribute,
    .getStatus        = OggParserGetStatus,
    .init            = OggParserInit
};

CdxParserT *OggParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    CdxOggParser *oggParser = CdxMalloc(sizeof(CdxOggParser));
    if(!oggParser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(oggParser, 0x00, sizeof(CdxOggParser));

    int ret = pthread_cond_init(&oggParser->cond, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_mutex_init(&oggParser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);

    oggParser->bufSize = 64*1024;
    oggParser->buf = CdxMalloc(oggParser->bufSize);
    CDX_FORCE_CHECK(oggParser->buf);
    oggParser->vProbeBuf.probeBuf = malloc(SIZE_OF_VIDEO_PROVB_DATA);
    CDX_FORCE_CHECK(oggParser->vProbeBuf.probeBuf);
    oggParser->vProbeBuf.probeBufSize = SIZE_OF_VIDEO_PROVB_DATA;

    oggParser->file = stream;
    oggParser->flags = flags & (DISABLE_SUBTITLE | MUTIL_AUDIO);
    oggParser->base.ops = &oggParserOps;
    oggParser->base.type = CDX_PARSER_OGG;
    oggParser->mErrno = PSR_INVALID;
    oggParser->status = CDX_PSR_INITIALIZED;
    return &oggParser->base;
}

cdx_uint32 OggParserProbe(CdxStreamProbeDataT *probeData)
{
    if(probeData->len < 6)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }
    if(memcmp("OggS", probeData->buf, 5) || probeData->buf[5] > 0x7)
    {
        CDX_LOGE("It is not ogg version 0.");
        return 0;
    }
    return 100;
}

CdxParserCreatorT oggParserCtor =
{
    .create    = OggParserOpen,
    .probe     = OggParserProbe
};

