#ifndef MPEG2TSMUXER_H
#define MPEG2TSMUXER_H

#include <stdint.h>
#include <encoder_type.h>
#include <record_writer.h>
#include "Mpeg2tsMuxer_cfg.h"
#include <FsWriter.h>

#define MAX_KEY_FRAME_NUM 2
#define MAX_SEGMENT_IN_M3U8 2

#define M3U8_PLAY_LIST


#define TS_FILE_SAVED_PATH	"/mnt/video/"
#define M3U8_FILE_SAVED_PATH	"/mnt/video/test"

#ifdef INT_MAX
#undef INT_MAX
#endif
#define INT_MAX  0xEFFFFFFF;

#define RSHIFT(a,b) ((a) > 0 ? ((a) + ((1<<(b))>>1))>>(b) : ((a) + ((1<<(b))>>1)-1)>>(b))
/* assume b>0 */
#define ROUNDED_DIV(a,b) (((a)>0 ? (a) + ((b)>>1) : (a) - ((b)>>1))/(b))
#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))
#define FFSIGN(a) ((a) > 0 ? 1 : -1)

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMAX3(a,b,c) FFMAX(FFMAX(a,b),c)
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMIN3(a,b,c) FFMIN(FFMIN(a,b),c)

#define FFSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0)
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define FFALIGN(x, a) (((x)+(a)-1)&~((a)-1))


#define MAX_STREAMS_IN_FILE 3           // current streams:video/audio/gps
#define MAX_SERVERVICES_IN_FILE 1

#define ADTS_HEADER_SIZE 7

#define TS_FEC_PACKET_SIZE 204
#define TS_DVHS_PACKET_SIZE 192
#define TS_PACKET_SIZE 188
#define TS_MAX_PACKET_SIZE 204

#define NB_PID_MAX 8192
#define MAX_SECTION_SIZE 4096

#define DEFAULT_PMT_START_PID   0x1000
#define DEFAULT_START_PID       0x0100

#define AV_NOPTS_VALUE          int64_t(0x8000000000000000)
#define AV_TIME_BASE            1000000
#define AV_TIME_BASE_Q          (AVRational){1, AV_TIME_BASE}


/* pids */
#define PAT_PID                 0x0000
#define SDT_PID                 0x0011

/* table ids */
#define PAT_TID   0x00
#define PMT_TID   0x02
#define SDT_TID   0x42


#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_VIDEO_H264      0x1b
#define STREAM_TYPE_VIDEO_VC1       0xea
#define STREAM_TYPE_VIDEO_DIRAC     0xd1
#define STREAM_TYPE_VIDEO_HEVC      0x24


#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x8a
#define STREAM_TYPE_AUDIO_LPCM      0x83

#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))

//#define ByteIOContext FILE
#define url_ftell ftell
#define url_fseek fseek

#define offset_t __s64
//#define MAX_STREAMS 2
#define AV_ROUND_UP 3

//#define av_free free
//#define av_malloc malloc
//#define av_freep free

#ifdef __SIM_TEST
#define MOV_CACHE_TINY_PAGE_SIZE (128)
#else
#define MOV_CACHE_TINY_PAGE_SIZE (2*1024)//set it to 2K !!attention it must set to below MOV_RESERVED_CACHE_ENTRIES
#endif

#define MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE (MOV_CACHE_TINY_PAGE_SIZE*4)
#define globalTimescale 1000

#define MODE_MP4  0x01


#ifdef UINT32_MAX
#undef UINT32_MAX
#endif
#define UINT32_MAX 0xffffffff

#ifdef INT32_MAX
#undef INT32_MAX
#endif
#define INT32_MAX 2147483647


#define MAX_PCE_SIZE  304
#define MAX_STREAMS 10


typedef struct {
    int write_adts;
    int objecttype;
    int sample_rate_index;
    int channel_conf;
    int pce_size;
    uint8_t pce_data[MAX_PCE_SIZE];
} ADTSContext;


typedef enum CHUNKID
{
	STCO_ID = 0,//don't change all
	STSZ_ID = 1,
	STSC_ID = 2,
	STTS_ID = 3
}CHUNKID;

#ifdef __SIM_TEST

#define STCO_CACHE_SIZE  (MOV_CACHE_TINY_PAGE_SIZE*2)
#define STSZ_CACHE_SIZE  (MOV_CACHE_TINY_PAGE_SIZE*4)
#define STSC_CACHE_SIZE  (STCO_CACHE_SIZE)

#else

#define STCO_CACHE_SIZE  (8*1024)          //about half hours !must times of tiny_page_size
#define STSZ_CACHE_SIZE  (8*16*1024)       //about half hours !must times of tiny_page_size
#define STTS_CACHE_SIZE  (STSZ_CACHE_SIZE)       //about half hours !must times of tiny_page_size
#define STSC_CACHE_SIZE  (STCO_CACHE_SIZE) //about half hours !must times of tiny_page_size

#endif

#define TOTAL_CACHE_SIZE (STCO_CACHE_SIZE*2 + STSZ_CACHE_SIZE*2 + STSC_CACHE_SIZE*2 + STTS_CACHE_SIZE + MOV_CACHE_TINY_PAGE_SIZE) //32bit

#define PKT_FLAG_KEY 1

#define DEFAULT_ONID            0x0001
//#define DEFAULT_TSID            0x0001
#define DEFAULT_TSID            0x0000
#define DEFAULT_SID             0x0001

/* a PES packet header is generated every DEFAULT_PES_HEADER_FREQ packets */


/* we retransmit the SI info at this rate */
#define SDT_RETRANS_TIME 500
#define PAT_RETRANS_TIME 100
#define PCR_RETRANS_TIME 20


#define DEFAULT_PES_HEADER_FREQ 16
#define DEFAULT_PES_PAYLOAD_SIZE ((DEFAULT_PES_HEADER_FREQ - 1) * 184 + 170)


typedef struct PutBitContext
{

    uint32_t bit_buf;
    int bit_left;
    uint8_t *buf, *buf_ptr, *buf_end;
    int size_in_bits;
} PutBitContext;


typedef struct MpegTSWriteStream
{
    struct MpegTSService *service;
    int pid; /* stream associated pid */
    int cc;
    int payload_index;
    int first_pts_check; ///< first pts check needed
    int64_t payload_pts;
    int64_t payload_dts;
    uint8_t payload[DEFAULT_PES_PAYLOAD_SIZE];
    ADTSContext *adts;
} MpegTSWriteStream;


//typedef bool   HXBOOL;
typedef struct ga_config_data_struct
{
    unsigned int audioObjectType;//AACLC=2;
    unsigned int samplingFrequency;       //hz
    //UINT32 extensionSamplingFrequency;
    //UINT32 frameLength;
    //UINT32 coreCoderDelay;
    unsigned int numChannels;
    //UINT32 numFrontChannels;
    //UINT32 numSideChannels;
    //UINT32 numBackChannels;
    //UINT32 numFrontElements;
    //UINT32 numSideElements;
    //UINT32 numBackElements;
    //UINT32 numLfeElements;
    //UINT32 numAssocElements;
    //UINT32 numValidCCElements;
    //HXBOOL bSBR;
} ga_config_data;

typedef struct AVRational
{
    int32_t num; ///< numerator
    int32_t den; ///< denominator
} AVRational;


typedef struct MpegTSSection {
    int pid;
    int cc;
    void (*write_packet)(struct MpegTSSection *s, uint8_t *packet);
    void *opaque;
} MpegTSSection;

typedef struct MpegTSService {
    MpegTSSection pmt; /* MPEG2 pmt table context */
    int sid;           /* service ID */
    char *name;
    char *provider_name;
    int pcr_pid;
    int pcr_packet_count;
    int pcr_packet_period;
} MpegTSService;

typedef struct MpegTSWrite {
    MpegTSSection pat; /* MPEG2 pat table */
    MpegTSSection sdt; /* MPEG2 sdt table context */
    MpegTSService **services;
    int sdt_packet_count;
    int sdt_packet_period;
    int pat_packet_count;
    int pat_packet_period;
    int nb_services;
    int onid;
    int tsid;
    uint64_t cur_pcr;
    uint8_t* ts_cache_start;
    uint8_t* ts_cache_end;
    uint32_t cache_size;
    uint64_t cache_size_total;
    uint8_t* ts_write_ptr;
    uint8_t* ts_read_ptr;
    uint32_t cache_page_num;
    int mux_rate; ///< set to 1 when VBR
} MpegTSWrite;


typedef struct AVCodecContext {

    int32_t bit_rate;
    int32_t width;
    int32_t height;
    enum CodecType codec_type; /* see CODEC_TYPE_xxx */
    uint32_t codec_tag;
    enum CodecID codec_id;

    int32_t channels;
    int32_t frame_size;
    int32_t frame_rate;

    int32_t bits_per_sample;
    int32_t sample_rate;

    AVRational time_base;
}AVCodecContext;

typedef struct AVStream {
    int32_t index;    /**< stream index in AVFormatContext */
    int32_t id;       /**< format specific stream id */
    AVCodecContext codec; /**< codec context */
    AVRational r_frame_rate;
    void *priv_data;

    /* internal data used in av_find_stream_info() */
    int64_t first_dts;
    /** encoding: PTS generation when outputing stream */
    //struct AVFrac pts;


    AVRational time_base;
    //FIXME move stuff to a flags field?
    /** quality, as it has been removed from AVCodecContext and put in AVVideoFrame
     * MN: dunno if that is the right place for it */
    float quality;
    int64_t start_time;
    int64_t duration;

    int64_t cur_dts;
	void *vosData;
	uint32_t vosLen;
	int   firstframeflag;
} AVStream;


typedef enum OUTPUT_BUFFER_MODE {
	OUTPUT_TS_FILE,
	OUTPUT_M3U8_FILE,
	OUTPUT_CALLBACK_BUFFER,
}OUTPUT_BUFFER_MODE;

typedef struct AVFormatContext {
	TSPacketHeader PacketHdr;
    AVRational time_base;
    void *priv_data;
    //ByteIOContext *pb;
    //FILE* pb_cache;
    struct cdx_stream_info  *pb_cache;
    uint32_t mFallocateLen;
    //FILE* fd_m3u8;
    struct cdx_stream_info  *fd_m3u8;
    int32_t nb_streams;
    uint8_t v_strm_flag;   // flag indicating video stream exist
    uint8_t a_strm_flag;   // flag indicating audio stream exist
    uint8_t t_strm_flag;   // flag indicating text stream exist
    AVStream *streams[MAX_STREAMS];
    char filename[1024]; /**< input or output filename */

    int64_t data_offset; /** offset of the first packet */
    int32_t index_built;

    uint32_t nb_programs;
    int64_t timestamp;
    int8_t firstframe[2];
    uint32_t total_video_frames;
    uint32_t video_info_addr;
    uint8_t *mov_inf_cache;
    int64_t data_size;
    uint32_t max_delay;
    uint32_t mux_rate;
    uint8_t*  cache_in_ts_stream;
    ga_config_data ga_data;
	int64_t keyframe_num;
	uint32_t current_segment;
	uint32_t sequence_num;
	uint32_t first_segment;
	uint32_t first_ts;
	unsigned char *pes_buffer;
    int pes_bufsize;
	int64_t audio_frame_num;
	int64_t Video_frame_number;
	int64_t pat_pmt_counter;
	int pat_pmt_flag;
	CedarXDataSourceDesc datasource_desc;
	struct cdx_stream_info *OutStreamHandle;
	int output_buffer_mode;
	int first_video_pts;    //unit:ms
	int64_t base_video_pts; //unit:ms
	unsigned int pcr_counter;

    FsWriter *mpFsWriter;
    FsCacheMemInfo mCacheMemInfo;   //RecRender malloc, set to Muxer to use.
    FSWRITEMODE mFsWriteMode;
    int32_t     mFsSimpleCacheSize;

    int first_audio_pts;
    int64_t base_audio_pts;
} AVFormatContext;

extern int ts_write_header(AVFormatContext *s);
extern int ts_write_packet(AVFormatContext *s, AVPacket *pkt);
extern int ts_write_trailer(AVFormatContext *s);
extern void section_write_packet(MpegTSSection *s, uint8_t *packet);

#endif // MPEG2TSMUXER_H
