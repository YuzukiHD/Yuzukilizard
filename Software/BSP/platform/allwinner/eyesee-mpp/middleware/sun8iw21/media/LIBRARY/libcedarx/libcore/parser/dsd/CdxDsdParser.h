/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxDsdParser.h
* Description :
* History :
*   Author  : Khan <chengkan@allwinnertech.com>
*   Date    : 2015/05/08
*/

#ifndef CDX_DSD_PARSER_H
#define CDX_DSD_PARSER_H

#define  INPUT_BUFFER_GUARD_SIZE   8

typedef enum {
    DSF,
    DSDIFF
} dsdtype;

typedef struct {
    // char id[4]; Read by upped level
    cdx_uint64 size;
    cdx_uint8   prop[4];
} dsdiff_header;

typedef struct {
    cdx_uint8 id[4];
    cdx_uint64 size;
} dsdiff_chunk_header;

typedef struct {
    cdx_uint8  header[4];
    cdx_uint64 size_of_chunk;
} data_header;

typedef struct {
    // guchar header[4]; already ready and verified by upper level
    cdx_uint64 size_of_chunk;
    cdx_uint64 total_size;
    cdx_int64  ptr_to_metadata;
} dsd_chunk;

typedef struct {
    cdx_uint8  header[4];
    cdx_uint64 size_of_chunk;
    cdx_uint32 format_version;
    cdx_uint32 format_id;
    cdx_uint32 channel_type;
    cdx_uint32 channel_num;
    cdx_uint32 sampling_frequency;
    cdx_uint32 bits_per_sample;
    cdx_uint64 sample_count;
    cdx_uint32 block_size_per_channel;
    cdx_uint8  reserved[4];
} fmt_chunk;

typedef struct {
    cdx_uint32 format_version;
    cdx_uint32 format_id;
    cdx_uint32 channel_type;
    cdx_uint32 bits_per_sample;
    cdx_uint32 block_size_per_channel;
} dsfinfo;

typedef struct {
    cdx_uint8  num_channels;
    cdx_uint32 bytes_per_channel;   // number of valid bytes (not size of array)
    cdx_uint32 max_bytes_per_ch;
    cdx_bool   lsb_first;
    cdx_uint32 sample_step;
    cdx_uint32 ch_step;
    cdx_uint8  *data;
} dsdbuffer;

typedef struct {
    CdxStreamT *stream;              // init @ dsd_open
    cdx_bool       canseek;          // init @ dsd_open
    cdx_bool       eof;              // init @ dsd_open
    cdx_uint64  offset;              // init @ dsd_open
    dsdtype     type;                // init @ dsd_open

    cdx_int32   file_size;           // init @ dsf_init or dsdiff_init

    cdx_uint32  channel_num;         // init 0 0 dsd_open, update @ dsf_init or dsdiff:
                                     // parse_prop_chunk, check not 0
    cdx_uint32  sampling_frequency;  // init 0 0 dsd_open, update @ dsf_init or dsdiff:
                                     // parse_prop_chunk, check not 0

    cdx_uint64  sample_offset;       // init 0 @ dsd_open, set X @ dsd_set_start
    cdx_uint64  sample_count;        // init @ dsf_init or dsdiff_init
    cdx_uint64  sample_stop;         // init @ dsf_init or dsdiff_init, set X @ dsd_set_stop

    dsfinfo     dsf;                 // init @ dsf_init

    cdx_size    dataoffset;          // init @ dsf_init or dsdiff_init
    cdx_size    datasize;            // init @ dsf_init or dsdiff_init
    dsdbuffer   buffer;              //
} dsdfile;

typedef struct DsdParserImplS
{
    //audio common
    CdxParserT  base;
    CdxStreamT  *stream;
    cdx_int32   mErrno; //Parser Status
    cdx_uint32  flags; //cmd
    cdx_int32   forceStop;
    pthread_t   openTid;
    cdx_int64   fileSize;
    //dsd base

    dsdfile*    dsd;
    cdx_int64   file_offset;
    cdx_uint16  mChannels;
    cdx_uint32  mSampleRate;
    cdx_uint32  mOriSampleRate;
    cdx_uint32  mBitRate;
    cdx_int32   mBitsSample;
    cdx_uint64  mTotalSamps;
    cdx_uint64  mCurSamps;
    /*Duration*/
    cdx_uint64  mDuration;
    cdx_uint8 *extradata;
    cdx_int32   extradata_size;
}DsdParserImplS;

#endif
