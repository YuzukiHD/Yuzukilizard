/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviParser.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _CDX_AVI_PARSER_H_
#define _CDX_AVI_PARSER_H_

#define AVI_TRUE    (1)
#define AVI_FALSE   (0)

#define MAX_STREAM (15)
#define MAX_AUDIO_STREAM    (8)//FILE_MAX_AUDIO_STREAM_NUM //MAX_AUDIO_STREAM_NUM
#define MAX_SUBTITLE_STREAM (10)

#define STRH_DWSCALE_THRESH 100
#define MIN_BYTERATE    1024    //min byte rate of audio stream

#define ABS_EDIAN_FLAG_LITTLE       ((cdx_uint32)(0<<16))


#ifndef CDX_MMIO_FOURCC
#define CDX_MMIO_FOURCC( ch0, ch1, ch2, ch3 )    \
            ( (cdx_uint32)(cdx_uint8)(ch0) | ( (cdx_uint32)(cdx_uint8)(ch1) << 8 ) |    \
            ( (cdx_uint32)(cdx_uint8)(ch2) << 16 ) | ( (cdx_uint32)(cdx_uint8)(ch3) << 24 ) )
#endif
/* Macro to make a TWOCC out of two characters */
#ifndef CDX_AVI_TWOCC
#define CDX_AVI_TWOCC(ch0, ch1) ((cdx_uint16)(cdx_uint8)(ch0) | ((cdx_uint16)(cdx_uint8)(ch1) << 8))
#endif

/* form types, list types, and chunk types */
#define CDX_FORM_HEADER_RIFF          CDX_MMIO_FOURCC('R', 'I', 'F', 'F')
#define CDX_FORM_TYPE_AVI             CDX_MMIO_FOURCC('A', 'V', 'I', ' ')
#define CDX_FORM_TYPE_AVIX            CDX_MMIO_FOURCC('A', 'V', 'I', 'X')
#define CDX_LIST_HEADER_LIST          CDX_MMIO_FOURCC('L', 'I', 'S', 'T')
#define CDX_LIST_TYPE_AVI_HEADER      CDX_MMIO_FOURCC('h', 'd', 'r', 'l')
#define CDX_CKID_AVI_MAINHDR          CDX_MMIO_FOURCC('a', 'v', 'i', 'h')
#define CDX_LIST_TYPE_STREAM_HEADER   CDX_MMIO_FOURCC('s', 't', 'r', 'l')
#define CDX_CKID_STREAM_HEADER        CDX_MMIO_FOURCC('s', 't', 'r', 'h')
#define CDX_CKID_STREAM_FORMAT        CDX_MMIO_FOURCC('s', 't', 'r', 'f')
#define CDX_CKID_STREAM_HANDLER_DATA  CDX_MMIO_FOURCC('s', 't', 'r', 'd')
#define CDX_CKID_STREAM_NAME          CDX_MMIO_FOURCC('s', 't', 'r', 'n')
#define CDX_LIST_TYPE_AVI_MOVIE       CDX_MMIO_FOURCC('m', 'o', 'v', 'i')
#define CDX_LIST_TYPE_AVI_RECORD      CDX_MMIO_FOURCC('r', 'e', 'c', ' ')
#define CDX_CKID_AVI_NEWINDEX         CDX_MMIO_FOURCC('i', 'd', 'x', '1')
#define CDX_CKID_ODMLINDX             CDX_MMIO_FOURCC('i', 'n', 'd', 'x')
#define CDX_CKID_IX00                 CDX_MMIO_FOURCC('i', 'x', '0', '0')
#define CDX_CKID_IX01                 CDX_MMIO_FOURCC('i', 'x', '0', '1')
#define CDX_CKID_IX02                 CDX_MMIO_FOURCC('i', 'x', '0', '2')
#define CDX_CKID_IX03                 CDX_MMIO_FOURCC('i', 'x', '0', '3')
#define CDX_CKID_IX04                 CDX_MMIO_FOURCC('i', 'x', '0', '4')
#define CDX_CKID_IX05                 CDX_MMIO_FOURCC('i', 'x', '0', '5')
#define CDX_CKID_IX06                 CDX_MMIO_FOURCC('i', 'x', '0', '6')
#define CDX_CKID_IX07                 CDX_MMIO_FOURCC('i', 'x', '0', '7')
#define CDX_CKID_IX08                 CDX_MMIO_FOURCC('i', 'x', '0', '8')
#define CDX_CKID_IX09                 CDX_MMIO_FOURCC('i', 'x', '0', '9')
#define CDX_CKID_ODMLHEADER           CDX_MMIO_FOURCC('d', 'm', 'l', 'h')

/*
** Stream types for the <fccType> field of the stream header.
*/
#define CDX_CKID_STREAM_TYPE_VIDEO    CDX_MMIO_FOURCC('v', 'i', 'd', 's')
#define CDX_CKID_STREAM_TYPE_AUDIO    CDX_MMIO_FOURCC('a', 'u', 'd', 's')
#define CDX_STREAM_TYPE_MIDI          CDX_MMIO_FOURCC('m', 'i', 'd', 's')
#define CDX_STREAM_TYPE_TEXT          CDX_MMIO_FOURCC('t', 'x', 't', 's')

/* Basic chunk types */
#define CDX_CKTYPE_DIB_BITS           CDX_AVI_TWOCC('d', 'b')
#define CDX_CKTYPE_DIB_COMPRESSED     CDX_AVI_TWOCC('d', 'c')
#define CDX_CKTYPE_DIB_DRM            CDX_AVI_TWOCC('d', 'd')
#define CDX_CKTYPE_PAL_CHANGE         CDX_AVI_TWOCC('p', 'c')
#define CDX_CKTYPE_WAVE_BYTES         CDX_AVI_TWOCC('w', 'b')
#define CDX_CKTYPE_SUB_TEXT           CDX_AVI_TWOCC('s', 't')
#define CDX_CKTYPE_SUB_BMP            CDX_AVI_TWOCC('s', 'b')
#define CDX_CKTYPE_CHAP               CDX_AVI_TWOCC('c', 'h')

/* Chunk id to use for extra chunks for padding. */
#define CDX_CKID_AVI_PADDING          CDX_MMIO_FOURCC('J', 'U', 'N', 'K')

/*
** Useful macros
*/

/* Macro to get stream number out of a FOURCC ckid, only for chunk id "01wb"锟斤拷 */
#define CDX_FROM_HEX(h)               (((h) >= 'A') ? ((h) + 10 - 'A') : ((h) - '0'))
#define CDX_STREAM_FROM_FOURCC(fcc) ((CDX_FROM_HEX((fcc) & 0xff))<<4)+(CDX_FROM_HEX((fcc>>8)&0xff))

/* Macro to get TWOCC chunk type out of a FOURCC ckid */
#define CDX_TWOCC_FROM_FOURCC(fcc)    (fcc >> 16)

/* Macro to make a ckid for a chunk out of a TWOCC and a stream number
** from 0-255., tcc = "bd", stream = 0x01, --> "bd10", 锟斤拷01db
*/
#define CDX_TO_HEX(n)                 ((cdx_uint8) (((n) > 9) ? ((n) - 10 + 'A') : ((n) + '0')))
#define CDX_MAKE_AVI_CKID(tcc, stream)  ((cdx_uint32) ((CDX_TO_HEX(((stream) & 0xf0)>>4))  \
                                           | (CDX_TO_HEX((stream) & 0x0f) << 8) | (tcc << 16)))

/*
** Main AVI File Header
*/

/* flags for use in <dwFlags> in AVIFileHdr */
#define CDX_AV_IF_HAS_INDEX           0x00000010  // Index at end of file
#define CDX_AV_IF_MUST_USEINDEX       0x00000020
#define CDX_AV_IF_IS_INTERLEAVED      0x00000100
#define CDX_AV_IF_TRUSTCKTYPE         0x00000800  // Use CKType to find key frames
#define CDX_AV_IF_WASCAPTUREFILE      0x00010000
#define CDX_AV_IF_COPYRIGHTED         0x00020000

//#define PCM_TAG                 0x0001
//#define ADPCM_TAG               0x0002
//#define MP3_TAG1                0x0055
//#define MP3_TAG2                0x0050
//#define AAC_TAG                 0x00ff
//#define WMA1_TAG                0x0160
//#define WMA2_TAG                0x0161
//#define WMAPRO_TAG              0x0162
//#define AC3_TAG                 0x2000
//#define DTS_TAG                 0x2001      //Support DTS audio--Michael 2004/06/29

enum AVI_AUDIO_TAG
{
    PCM_TAG     = 0x0001,
    ADPCM_TAG   = 0x0002,
    ALAW_TAG    = 0x0006,
    MULAW_TAG   = 0x0007,
    ADPCM11_TAG = 0X0011,
    MP3_TAG1    = 0x0055,
    MP3_TAG2    = 0x0050,
    AAC_TAG     = 0x00ff,
    WMA1_TAG    = 0x0160,
    WMA2_TAG    = 0x0161,
    WMAPRO_TAG  = 0x0162,
    AC3_TAG     = 0x2000,
    DTS_TAG     = 0x2001,      //Support DTS audio--Michael 2004/06/29
    //VORBIS_TAG  = 0x6771,
};

/* The AVI File Header LIST chunk should be padded to this size */
#define AVI_HEADERSIZE          2048        // size of AVI header list

typedef struct MainAVIHeaderT
{
    cdx_uint32        dwMicroSecPerFrame;        // frame display rate (or 0L)
    cdx_uint32        dwMaxBytesPerSec;          // max. transfer rate
    cdx_uint32        dwPaddingGranularity;      // pad to multiples of this size; normally 2K.
    cdx_uint32        dwFlags;                   // the ever-present flags
    cdx_uint32        dwTotalFrames;   // total frames in file   LIST odml->dmlh->dwTotalFrames为准
    cdx_uint32        dwInitialFrames;           //
    cdx_uint32        dwStreams;
    cdx_uint32        dwSuggestedBufferSize;
    cdx_uint32        dwWidth;
    cdx_uint32        dwHeight;
    cdx_uint32        dwReserved[4];
}MainAVIHeaderT;

/*
** Stream header
*/

#define CDX_AVISF_DISABLED              0x00000001
#define CDX_AVISF_VIDEO_PALCHANGES      0x00010000

typedef cdx_uint32 FOURCC;

typedef struct _video_rect_
{
    cdx_uint16        left;
    cdx_uint16        top;
    cdx_uint16        right;
    cdx_uint16        bottom;
} VideoRectT;

typedef struct _AVISTREAMHEADER_
{
    FOURCC              fccType;    //表示数据流的种类,vids表示视频数据流，auds音频数据流
    FOURCC              fccHandler;
    cdx_uint32          dwFlags;    // Contains AVITF_* flags
    cdx_uint16          wPriority;
    cdx_uint16          wLanguage;
    cdx_uint32          dwInitialFrames;
    cdx_uint32          dwScale;    //数据量，视频每帧的大小或者音频的采样大小
    cdx_uint32          dwRate;     // dwRate / dwScale == samples/second, for video:frame/second,
                                    // for audio:byte/second
    cdx_uint32          dwStart;
    cdx_uint32          dwLength;   //In units above, for vidoe:frame count;for audio: byte count
    cdx_uint32          dwSuggestedBufferSize;
    cdx_uint32          dwQuality;
    cdx_uint32          dwSampleSize;
    VideoRectT          rcFrame;
} AVIStreamHeaderT;

typedef struct tagWAVEFORMATEX
{
    cdx_uint16       wFormatTag;     //MP3_TAG1,...
    cdx_uint16       nChannels;
    cdx_uint32       nSamplesPerSec;
    cdx_uint32       nAvgBytesPerSec;
    cdx_uint16       nBlockAlign;
    cdx_uint16       wBitsPerSample;
    cdx_uint16       cbSize;
} WAVEFORMATEX;

/* Flags for index */
#define CDX_AVIIF_LIST          0x00000001L // chunk is a 'LIST'
#define CDX_AVIIF_KEYFRAME      0x00000010L // this frame is a key frame.
#define CDX_AVIIF_NOTIME        0x00000100L // this frame doesn't take any time
#define CDX_AVIIF_COMPUSE       0x0FFF0000L // these bits are for compressor use

typedef struct AVIPALCHANGE
{
    cdx_uint8        bFirstEntry;    /* first entry to change */
    cdx_uint8        bNumEntries;    /* # entries to change (0 if 256) */
    cdx_uint16       wFlags;         /* Mostly to preserve alignment... */
//    PALETTEENTRY    peNew[];    /* New color specifications */
}AVIPALCHANGE;

typedef struct tagBITMAPINFOHEADER
{                               // bmih
    cdx_uint32       biSize;
    cdx_uint32       biWidth;
    cdx_uint32       biHeight;
    cdx_uint16       biPlanes;
    cdx_uint16       biBitCount;
    cdx_uint32       biCompression;
    cdx_uint32       biSizeImage;
    cdx_uint32       biXPelsPerMeter;
    cdx_uint32       biYPelsPerMeter;
    cdx_uint32       biClrUsed;
    cdx_uint32       biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagTEXTINFO
{
    cdx_uint16       wCodePage;
    cdx_uint16       wCountryCode;
    cdx_uint16       wLanguageCode;
    cdx_uint16       wDialect;
} TEXTINFO;

//#define MAX_STREAM 8
#define INDEX_INC 1000

typedef struct _AVI_CHUNK_
{
    FOURCC      fcc;
    cdx_uint32  length;
    cdx_uint32  flag;
    cdx_char    *buffer;
}AviChunkT;

#define CDX_AVI_INDEX_OF_INDEXES       0x00 //when each entry is aIndex array points to
                                            // an index chunk
#define CDX_AVI_INDEX_OF_CHUNKS        0x01 //when each entry is aIndex array points to
                                            // a chunk in the file
#define CDX_AVI_INDEX_IS_DATA          0x80 //when each entry is aIndex is really the data
                                            //bIndexSubType codes for INDEX_OF_CHUNKS
#define CDX_AVI_INDEX_2FIELD           0x01 //when fields within frames are also indexed.
#define CDX_AVI_INDEX_SUBTYPE_1FRMAE   0x00 //when frame is not indexed to two fileds.

typedef struct _avisuperindex_entry
{
    cdx_uint32       offsetLow;
    cdx_uint32       offsetHigh;   //point to indx chunk head.
    cdx_uint32       size;          //size of index chunk, two case:
                                    //(1)not include chunk head, (2)include chunk head.
    cdx_uint32       duration;      //chunk_entry total count in this index_chunk.
}AviSuperIndexEntryT;

typedef struct _avistdindex_enty
{
    cdx_uint32       offset; //offset is set to the chunk body, not chunk head!
    cdx_uint32       size;   //bit31 is set if this is NOT a keyframe!
                             //(ref to OpenDML AVI File Format)
}AviStdIndexEntyT;

typedef struct _avifiledindex_enty
{
    cdx_uint32       offset; //offset is set to the chunk body, not chunk head!
    cdx_uint32       size;   //bit31 is set if this is NOT a keyframe!
                        //and size is the chunk body size.Not include chunk head.
    cdx_uint32       offsetField2;//offset to second field in this chunk.
}AviFieldIndexEntyT;


typedef struct _ODMLExtendedAVIHeader
{
    cdx_uint32       dwTotalFrames;
} ODMLExtendedAVIHeaderT;

typedef struct _avistdindex_chunk
{
    cdx_uint16       wLongsperEntry;
    cdx_uint8        bIndexSubType;  //AVI_INDEX_2FILED,etc
    cdx_uint8        bIndexType;
    cdx_uint32       nEntriesInUse;
    cdx_uint32       dwChunkId;
    cdx_uint32       baseOffsetLow;
    cdx_uint32       baseOffsetHigh;
    cdx_uint32       dwReserved;
    AviStdIndexEntyT    *aIndex;
}AviStdIndexChunkT;

#define MAX_IX_ENTRY_NUM    128
typedef struct _avisuperindex_chunk
{
    cdx_uint16       wLongsPerEntry; //sizeof entry in this table
    cdx_uint8        bIndexSubType;  //AVI_INDEX_2FILED,etc
    cdx_uint8        bIndexType;     //CDX_AVI_INDEX_OF_INDEXES / CDX_AVI_INDEX_OF_CHUNKS
    cdx_uint32       nEntriesInUse;  //aIndex
    cdx_uint32       dwChunkId;
    cdx_uint32       baseOffsetLow;
    cdx_uint32       baseOffsetHigh;
    cdx_uint32       dwReserved;
    AviSuperIndexEntryT *aIndex[MAX_IX_ENTRY_NUM];
}AviSuperIndexChunkT;

typedef struct AVIIXENTRY
{
    cdx_uint32       position;
    cdx_uint32       length;
}AVIIXENTRY;

typedef struct AVIIXCHUNK
{
    cdx_uint32       ix_tag;
    cdx_uint32       size;
    cdx_uint16       wLongsPerEntry;
    cdx_uint8        bIndexSubType;      //(0 == frame index)
    cdx_uint8        bIndexType;         //(1 == CDX_AVI_INDEX_OF_CHUNKS)
    cdx_uint32       nEntriesInUse;
    cdx_uint32       dwChunkId;
    cdx_uint32       qwBaseOffset_low;
    cdx_uint32       qwBaseOffset_high;
    cdx_uint32       reserved;
}AVIIXCHUNK;    //odml锟斤拷锟侥硷拷锟斤拷式


typedef struct _AVI_STREAM_INFO_
{
    cdx_int8        streamType;// strn: "Video/Audio/Subtitle/Chapter [- Description]"
    /**********************
    type of the stream
    a - audio
    v - video
    s - subtitle
    c - chapter
    u - unknown
    **********************/

    cdx_uint8        mediaType; //AVIStreamHeader->fccType.
    /**********************
    type of the media
    a - audio
    v - video
    t - text
    u - unknown
    **********************/

    int        isODML;
    cdx_int64       indxTblOffset; // LIST strl -> indx
    AviSuperIndexChunkT *indx;     //

    AviChunkT       *strh;  //strh,length, AVIStreamHeader,  malloc
    AviChunkT       *strf;  //strf ,length
    AviChunkT       *strd;  //锟斤拷要malloc
    AviChunkT       *strn;  //strn, length, "string" 锟斤拷要malloc

}AviStreamInfoT;
typedef struct _AUDIO_STREAM_INFO_
{
    cdx_int32   streamIdx;     //
    cdx_int32   aviAudioTag;   //PCM_TAG
    cdx_int32   cbrFlag;       //1:cbr, 0:vbr
    cdx_int32   sampleRate;
    cdx_int32   sampleNumPerFrame;
    cdx_int32   avgBytesPerSec; //unit: byte/s
    cdx_int32   nBlockAlign;    //if VBR, indicate an audioframe's max bytes

    cdx_int32   dataEncodeType; //enum __CEDARLIB_SUBTITLE_ENCODE_TYPE,
    //CDX_SUBTITLE_ENCODE_UTF8,  <==>__cedar_subtitle_encode_t,  CDX_SUBTITLE_ENCODE_UTF8
    cdx_uint8   sStreamName[AVI_STREAM_NAME_SIZE];
}AudioStreamInfoT;

typedef struct _SUB_STREAM_INFO_ //for subtitle
{
    cdx_int32   streamIdx;
    cdx_int32   aviSubTag;
    cdx_uint8   sStreamName[AVI_STREAM_NAME_SIZE];
}SubStreamInfoT;

//struct avi_chunk_reader{
//    struct cdx_stream_info    *fp;
//    cdx_uint32   cur_pos;        //the offset in file start.
//    cdx_uint32   chunk_length;   //not include 8-byte header.
//
//    AVI_CHUNK   *pavichunk; //used to read a chunk. But its space is malloc outside.
//};
enum READ_CHUNK_MODE{
    READ_CHUNK_SEQUENCE = 0,    //sequence read avi chunk
    READ_CHUNK_BY_INDEX = 1,    //read avi chunk base index table's indication.
};
enum USE_INDEX_STYLE{
    NOT_USE_INDEX   = 0,    //in common, when index table not exist, we set this value.
    USE_IDX1        = 1,
    USE_INDX        = 2,    //use Open-DML index table.
};
enum AVI_CHUNK_TYPE{
    CHUNK_TYPE_VIDEO = 0,
    CHUNK_TYPE_AUDIO = 1,
};

typedef struct AviVideoStreamInfo
{
    VideoStreamInfo vFormat;
    cdx_uint32      nMicSecPerFrame;
    
}AviVideoStreamInfo;

#endif /* _CDX_AVI_PARSER_H_ */

