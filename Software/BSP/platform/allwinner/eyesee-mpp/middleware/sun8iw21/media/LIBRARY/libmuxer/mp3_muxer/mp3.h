#ifndef _MP3_H
#define _MP3_H

#include <stdio.h>
#include <string.h>
#include <record_writer.h>
#include <encoder_type.h>
#include <FsWriter.h>

#define ENOMEM 12
#define EINVAL 22

#define AVERROR(e) (-(e)) // Returns a negative error code from a POSIX error code, to return from library functions. 
#define AVUNERROR(e) (-(e)) // Returns a POSIX error code from a library function error return value. 

#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))

#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#define MKBETAG(a,b,c,d) (d | (c << 8) | (b << 16) | (a << 24))

#define ID3v1_TAG_SIZE 128
#define ID3v1_GENRE_MAX 147

#define ID3v2_HEADER_SIZE 10

#define url_ftell ftell
#define url_fseek fseek

#define av_free     free
#define av_malloc   malloc
#define av_realloc  realloc

#define AV_RB32(x)                              \
    ((((const unsigned char*)(x))[0] << 24) |         \
     (((const unsigned char*)(x))[1] << 16) |         \
     (((const unsigned char*)(x))[2] <<  8) |         \
      ((const unsigned char*)(x))[3])


#define AV_METADATA_MATCH_CASE      1
#define AV_METADATA_IGNORE_SUFFIX   2
#define AV_METADATA_DONT_STRDUP_KEY 4
#define AV_METADATA_DONT_STRDUP_VAL 8
#define AV_METADATA_DONT_OVERWRITE 16   ///< Don't overwrite existing tags.

typedef struct {
    char *key;
    char *value;
}AVMetadataTag;

typedef struct AVMetadata AVMetadata;
typedef struct AVMetadataConv AVMetadataConv;

struct AVMetadata{
    int count;
    AVMetadataTag *elems;
};

typedef struct AVFormatContext {
    //const AVClass *av_class; /**< Set by avformat_alloc_context. */
    void *priv_data;
    //ByteIOContext *pb;
    //FILE *pb;
    struct cdx_stream_info *pb;
    unsigned int     mFallocateLen;
    //unsigned int nb_streams;
    //AVStream *streams[MAX_STREAMS];
    char filename[1024]; /**< input or output filename */
    /* stream info */
    int64_t timestamp;
    int ctx_flags; /**< Format-specific flags, see AVFMTCTX_xx */
    /** Decoding: position of the first frame of the component, in
       AV_TIME_BASE fractional seconds. NEVER set this value directly:
       It is deduced from the AVStream values.  */
    int64_t start_time;
    /** Decoding: duration of the stream, in AV_TIME_BASE fractional
       seconds. Only set this value if you know none of the individual stream
       durations and also dont set any of them. This is deduced from the
       AVStream values if not set.  */
    int64_t duration;
    /** decoding: total file size, 0 if unknown */
    int64_t file_size;
    /** Decoding: total stream bitrate in bit/s, 0 if not
       available. Never set it directly if the file_size and the
       duration are known as FFmpeg can compute it automatically. */
    int bit_rate;

    int mux_rate;
    unsigned int packet_size;
    int max_delay;

    int flags;

    FsWriter*   mpFsWriter;
    FsCacheMemInfo mCacheMemInfo;
    FSWRITEMODE mFsWriteMode;
    int     mFsSimpleCacheSize;
    unsigned int mbSdcardDisappear;  //1:sdcard disappear, 0:sdcard is normal.

    AVMetadata *metadata;
} AVFormatContext;


int mp3_write_packet(struct AVFormatContext *s, AVPacket *pkt);

int mp3_write_trailer(struct AVFormatContext *s);

int mp3_write_header(struct AVFormatContext *s);

#endif /* _MP3_H */
