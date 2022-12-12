/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : AsfParser.h
 * Description : AsfParser
 * Author  : xuqi <xuqi@allwinnertech.com>
 * Comment : 创建初始版本
 *
 */

#ifndef ASF_PARSER_H
#define ASF_PARSER_H

#include <errno.h>
#include <stdint.h>
#include <CdxTypes.h>
#include <CdxStream.h>
#include <CdxParser.h>

typedef cdx_uint8 AsfGuid[16];

#define MAX_STREAMS 10
#define PKT_FLAG_KEY   0x0001
#define AVINDEX_KEYFRAME 1//tmp

enum CDX_MEDIA_STATUS
{
    CDX_MEDIA_STATUS_IDLE,
    CDX_MEDIA_STATUS_PLAY,
    CDX_MEDIA_STATUS_JUMP,
};

#define ASF_INPUT_BUFFER_PADDING_SIZE 8

typedef struct AsfContextS AsfContextT;
typedef struct AsfMediaStreamS AsfMediaStreamT;
typedef struct AsfPacketS AsfPacketT;

static const AsfGuid index_guid =
{
    0x90, 0x08, 0x00, 0x33, 0xb1, 0xe5, 0xcf, 0x11, 0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb
};

static const AsfGuid stream_bitrate_guid =
{ /* (http://get.to/sdp) */
    0xce, 0x75, 0xf8, 0x7b, 0x8d, 0x46, 0xd1, 0x11, 0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2
};

static const AsfGuid asf_header =
{
    0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C
};

static const AsfGuid file_header =
{
    0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

static const AsfGuid stream_header =
{
    0x91, 0x07, 0xDC, 0xB7, 0xB7, 0xA9, 0xCF, 0x11, 0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

static const AsfGuid ext_stream_header =
{
    0xCB, 0xA5, 0xE6, 0x14, 0x72, 0xC6, 0x32, 0x43, 0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A
};

static const AsfGuid audio_stream =
{
    0x40, 0x9E, 0x69, 0xF8, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
};

static const AsfGuid audio_conceal_none =
{
    0x00, 0x57, 0xfb, 0x20, 0x55, 0x5B, 0xCF, 0x11, 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b
};

static const AsfGuid audio_conceal_spread =
{
    0x50, 0xCD, 0xC3, 0xBF, 0x8F, 0x61, 0xCF, 0x11, 0x8B, 0xB2, 0x00, 0xAA, 0x00, 0xB4, 0xE2, 0x20
};

static const AsfGuid video_stream =
{
    0xC0, 0xEF, 0x19, 0xBC, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
};

static const AsfGuid video_conceal_none =
{
    0x00, 0x57, 0xFB, 0x20, 0x55, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
};

static const AsfGuid command_stream =
{
    0xC0, 0xCF, 0xDA, 0x59, 0xE6, 0x59, 0xD0, 0x11, 0xA3, 0xAC, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6
};

static const AsfGuid comment_header =
{
    0x33, 0x26, 0xb2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c
};

static const AsfGuid codec_comment_header =
{
    0x40, 0x52, 0xD1, 0x86, 0x1D, 0x31, 0xD0, 0x11, 0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6
};

static const AsfGuid data_header =
{
    0x36, 0x26, 0xb2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c
};

static const AsfGuid head1_guid =
{
    0xb5, 0x03, 0xbf, 0x5f, 0x2E, 0xA9, 0xCF, 0x11, 0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65
};

static const AsfGuid extended_content_header =
{
    0x40, 0xA4, 0xD0, 0xD2, 0x07, 0xE3, 0xD2, 0x11, 0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50
};

static const AsfGuid simple_index_header =
{
    0x90, 0x08, 0x00, 0x33, 0xB1, 0xE5, 0xCF, 0x11, 0x89, 0xF4, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB
};

static const AsfGuid ext_stream_embed_stream_header =
{
    0xe2, 0x65, 0xfb, 0x3a, 0xEF, 0x47, 0xF2, 0x40, 0xac, 0x2c, 0x70, 0xa9, 0x0d, 0x71, 0xd3, 0x43
};

static const AsfGuid ext_stream_audio_stream =
{
    0x9d, 0x8c, 0x17, 0x31, 0xE1, 0x03, 0x28, 0x45, 0xb5, 0x82, 0x3d, 0xf9, 0xdb, 0x22, 0xf5, 0x03
};

static const AsfGuid metadata_header =
{
    0xea, 0xcb, 0xf8, 0xc5, 0xaf, 0x5b, 0x77, 0x48, 0x84, 0x67, 0xaa, 0x8c, 0x44, 0xfa, 0x4c, 0xca
};
#if 0
#define ASF_PACKET_FLAG_ERROR_CORRECTION_PRESENT 0x80 //1000 0000

//   ASF data packet structure
//   =========================
//
//
//  -----------------------------------
// | Error Correction Data             |  Optional
//  -----------------------------------
// | Payload Parsing Information (PPI) |
//  -----------------------------------
// | Payload Data                      |
//  -----------------------------------
// | Padding Data                      |
//  -----------------------------------

// PPI_FLAG - Payload parsing information flags
#define ASF_PPI_FLAG_MULTIPLE_PAYLOADS_PRESENT 1

#define ASF_PPI_FLAG_SEQUENCE_FIELD_IS_BYTE  0x02 //0000 0010
#define ASF_PPI_FLAG_SEQUENCE_FIELD_IS_WORD  0x04 //0000 0100
#define ASF_PPI_FLAG_SEQUENCE_FIELD_IS_DWORD 0x06 //0000 0110
#define ASF_PPI_MASK_SEQUENCE_FIELD_SIZE     0x06 //0000 0110

#define ASF_PPI_FLAG_PADDING_LENGTH_FIELD_IS_BYTE  0x08 //0000 1000
#define ASF_PPI_FLAG_PADDING_LENGTH_FIELD_IS_WORD  0x10 //0001 0000
#define ASF_PPI_FLAG_PADDING_LENGTH_FIELD_IS_DWORD 0x18 //0001 1000
#define ASF_PPI_MASK_PADDING_LENGTH_FIELD_SIZE     0x18 //0001 1000

#define ASF_PPI_FLAG_PACKET_LENGTH_FIELD_IS_BYTE  0x20 //0010 0000
#define ASF_PPI_FLAG_PACKET_LENGTH_FIELD_IS_WORD  0x40 //0100 0000
#define ASF_PPI_FLAG_PACKET_LENGTH_FIELD_IS_DWORD 0x60 //0110 0000
#define ASF_PPI_MASK_PACKET_LENGTH_FIELD_SIZE     0x60 //0110 0000

// PL_FLAG - Payload flags
#define ASF_PL_FLAG_REPLICATED_DATA_LENGTH_FIELD_IS_BYTE   0x01 //0000 0001
#define ASF_PL_FLAG_REPLICATED_DATA_LENGTH_FIELD_IS_WORD   0x02 //0000 0010
#define ASF_PL_FLAG_REPLICATED_DATA_LENGTH_FIELD_IS_DWORD  0x03 //0000 0011
#define ASF_PL_MASK_REPLICATED_DATA_LENGTH_FIELD_SIZE      0x03 //0000 0011

#define ASF_PL_FLAG_OFFSET_INTO_MEDIA_OBJECT_LENGTH_FIELD_IS_BYTE  0x04 //0000 0100
#define ASF_PL_FLAG_OFFSET_INTO_MEDIA_OBJECT_LENGTH_FIELD_IS_WORD  0x08 //0000 1000
#define ASF_PL_FLAG_OFFSET_INTO_MEDIA_OBJECT_LENGTH_FIELD_IS_DWORD 0x0c //0000 1100
#define ASF_PL_MASK_OFFSET_INTO_MEDIA_OBJECT_LENGTH_FIELD_SIZE     0x0c //0000 1100

#define ASF_PL_FLAG_MEDIA_OBJECT_NUMBER_LENGTH_FIELD_IS_BYTE  0x10 //0001 0000
#define ASF_PL_FLAG_MEDIA_OBJECT_NUMBER_LENGTH_FIELD_IS_WORD  0x20 //0010 0000
#define ASF_PL_FLAG_MEDIA_OBJECT_NUMBER_LENGTH_FIELD_IS_DWORD 0x30 //0011 0000
#define ASF_PL_MASK_MEDIA_OBJECT_NUMBER_LENGTH_FIELD_SIZE     0x30 //0011 0000

#define ASF_PL_FLAG_STREAM_NUMBER_LENGTH_FIELD_IS_BYTE  0x40 //0100 0000
#define ASF_PL_MASK_STREAM_NUMBER_LENGTH_FIELD_SIZE     0xc0 //1100 0000

#define ASF_PL_FLAG_PAYLOAD_LENGTH_FIELD_IS_BYTE  0x40 //0100 0000
#define ASF_PL_FLAG_PAYLOAD_LENGTH_FIELD_IS_WORD  0x80 //1000 0000
#define ASF_PL_MASK_PAYLOAD_LENGTH_FIELD_SIZE     0xc0 //1100 0000

#define ASF_PL_FLAG_KEY_FRAME 0x80 //1000 0000
#endif
//#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))

struct AsfPacketS  /*struct AVPacket*/
{
    int size;
};

struct AsfCodecS /*struct AVCodecContext*/
{
    enum CdxMediaTypeE codec_type;
    int codec_id;
    int subCodecId; // for audio... terrible
    cdx_uint32 codec_tag;
    int frame_size;
    int width;
    int height;
    int bits_per_sample;
    int extradata_size;
    cdx_uint8 *extradata;
    int channels;
    int sample_rate;
    int bit_rate;
    int block_align;
};

struct AsfMediaStreamS /*struct AVStream + ASFStream*/
{
    int typeIndex;
    int id;       /**< format specific stream id */
    int encrypted;/* bitstream encrypted flag   */
    struct AsfCodecS codec;
    cdx_int64 duration;
    int active;
    cdx_uint8 seq;

    AsfPacketT pkt;
    int frag_offset;

    int ds_span;                /* descrambling  */
    int ds_packet_size;
    int ds_chunk_size;

    int discard_data;
} ;

struct AsfMainHeaderS /*ASFMainHeader*/
{
    AsfGuid guid;
    cdx_uint64 file_size;         /**< in bytes
    *   invalid if broadcasting */
    cdx_uint64 create_time;       /**< time of creation, in 100-nanosecond units since 1.1.1601
    *   invalid if broadcasting */
    cdx_uint64 play_time;         /**< play time, in 100-nanosecond units
    * invalid if broadcasting */
    cdx_uint64 send_time;         /**< time to send file, in 100-nanosecond units
    *   invalid if broadcasting (could be ignored) */
    cdx_uint32 preroll;           /**< timestamp of the first packet, in milliseconds
    *   if nonzero - subtract from time */
    cdx_uint32 ignore;            /**< preroll is 64bit -
    but let's just ignore it**/
    cdx_uint32 flags;             /**< 0x01 - broadcast
                                *   0x02 - seekable
    *   rest is reserved should be 0 */
    cdx_uint32 min_pktsize;       /**< size of a data packet
    *   invalid if broadcasting */
    cdx_uint32 max_pktsize;       /**< shall be the same as for min_pktsize
    *   invalid if broadcasting */
    cdx_uint32 max_bitrate;       /**< bandwith of stream in bps
                                *   should be the sum of bitrates of the
    *   individual media streams */
};

struct AsfIndexS
{
    cdx_uint32 packet_number;
    //cdx_uint16 packet_count;
};

struct AsfContextS /*ASFContext*/
{
    CdxStreamT *ioStream;
    struct AsfMainHeaderS hdr;
    cdx_int64 fileSize;
    int nb_streams;
    AsfMediaStreamT *streams[128];
    int asfid2avid[128];                 ///< conversion table from asf ID 2 AVStream ID
    cdx_uint32 stream_bitrates[128];///< max number of streams, bitrate for each (for streaming)
    cdx_int64 totalBitRate;

    cdx_uint32 packet_size;
    AsfMediaStreamT *asf_st; /*current read stream*/
    /* stream info */

    cdx_int64 durationMs;

    AsfMediaStreamT *cur_st;

    int ict; /*Index Entries Count*/
    int itimeMs; /*Index Entry Time Interval*/
    int curr_key_tbl_idx;
    int seekAble;

    /* non streamed additonnal info */
    cdx_uint64 nb_packets;  ///< how many packets are there in the file, invalid if broadcasting

    /* packet filling */
    int packet_size_left;

    /* only for reading */
    cdx_uint64 data_offset;                ///< beginning of the first data packet
    cdx_uint64 data_object_offset;         ///< data object offset (excl. AsfGuid & size)
    cdx_uint64 dataObjectSize;           ///< size of the data object

    cdx_int64 packet_pos;
    int packet_hdr_size;
    int packet_flags;
    int packet_property;
    int packet_timestamp;
    int packet_segsizetype;
    int packet_segments;
    int packet_seq;
    int packet_replic_size;
    int packet_key_frame;
    int frame_ctrl;//bit0:start bit1:end
    int packet_padsize;
    int packet_frag_offset;
    int packet_frag_size;
    cdx_int64 packet_frag_timestamp;
    int packet_multi_size;
    int packet_obj_size;
    int packet_time_delta;
    int packet_time_start;

    cdx_uint8 stream_index;

    struct AsfIndexS *index_ptr;
    AsfPacketT pkt;

    cdx_int32  status;
    cdx_int32 videoNum;
    cdx_int32 audioNum;

    int forceStop;
} ;

#ifdef __cplusplus
extern "C"
{
#endif

AsfContextT *AsfPsrCoreInit(void);

int AsfPsrCoreOpen(AsfContextT *p, CdxStreamT *stream);

int AsfPsrCoreClose(AsfContextT *p);

int AsfPsrCoreRead(AsfContextT *p);

int AsfPsrCoreIoctrl(AsfContextT *p, cdx_uint32 uCmd, cdx_uint32 uParam);

#ifdef __cplusplus
}
#endif

#endif

