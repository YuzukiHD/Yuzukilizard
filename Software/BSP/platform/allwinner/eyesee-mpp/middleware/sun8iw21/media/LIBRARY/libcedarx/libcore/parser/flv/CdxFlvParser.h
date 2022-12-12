/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxFlvParser.h
 * Description : Part of flv parser.
 * History :
 *
 */

#ifndef _CDX_FLV_PARSER_H_
#define _CDX_FLV_PARSER_H_

//For mpeg4 128K is enough
//for motion jpeg, maybe 256k is needed
#define MAX_CHUNK_BUF_SIZE      (1024 * 1024)    //1024k

typedef struct __FLV_CHUNK_INFO
{
    cdx_uint32       nChkType;       //chunk type, audio type, video type, lyric type
    cdx_int32        nTotSize;       //total size of the chunk, based on byte
    cdx_int32        nReadSize;      //data size has been read out, based on byte
    cdx_uint32       nTimeStamp;     //time stamp for current video chunk
} FlvChunkInfoT;


typedef struct FlvAudioTagsT
{
    unsigned soundType              :   1;        //0: sndMono, 1: sndStereo
    unsigned soundSize              :   1;        //0: snd8Bit, 1: snd16Bit
    unsigned soundRate              :   2;        //0: 5.5kHz, 1: 11kHz, 2: 22kHz, 3: 44kHz
    unsigned soundFormat            :   4;        //0: uncompresed, 1: ADPCM, 2: MP3,
                                                  //5: Nellymoser 8khz mono, 6: Nellymoser
    unsigned reserved0              :   8;
    unsigned reserved1              :   8;
    unsigned reserved2              :   8;
}FlvAudioTagsT;//1 bytes reserve 4 bytes space

typedef struct FlvVideoTagsT
{
    unsigned codecID              :   4;//2: Sorensson H.263, 3: screen video, 4: On2 VP6,
                                        //5: On2 VP6 wiht alpha channel, 6: Screen video version 2
    unsigned frameType            :   4;// 1: keyframe, 2: inter frame, 3: idsposable inter frame
    unsigned picStartCode0        :   16;
    unsigned temporalReference0   :   2;
    unsigned version              :   5;
    unsigned picStartCode1        :   1;
    unsigned PicSize0             :   2;
    unsigned temporalReference1   :   6;
    unsigned sizeData0            :   7;
    unsigned picSize1             :   1;
    unsigned sizeData1            :   8;
    unsigned sizeData2            :   8;
    unsigned sizeData3            :   8;
    unsigned sizeData4            :   8;
    unsigned reserved0            :   8;
    unsigned reserved1            :   8;
}FlvVideoTagsT;//10 bytes reserve 12 bytes space

typedef struct FlvTagsT
{
    unsigned preTagsize             :   32;
    unsigned tagType                :   8;
    unsigned dataSize               :   24;
    unsigned timeStamp              :   24;//µ¥Î»:ms
    unsigned timeStampExt           :   8;
    unsigned streamID               :   24;
    unsigned codecType              :   8;
}FlvTagsT; //15 bytes reserve 16 bytes space

typedef struct FlvFileHeaderT
{
    unsigned signature0             :   8;
    unsigned signature1             :   8;
    unsigned signature2             :   8;
    unsigned version                :   8;
    unsigned typeFlagVideo          :   1;
    unsigned reserved1              :   1;
    unsigned typeFlagAudio          :   1;
    unsigned reserved0              :   5;
    unsigned dataOffset             :   32;
}FlvFileHeaderT;//9 bytes reserve 12 bytes space

typedef struct _FLV_CHUNK_
{
    cdx_uint32       length;
    cdx_uint8        *buffer;
}FlvChunkT;

typedef struct Keyframe_idx
{
    cdx_int64             *keyFramePosIdx;
    cdx_int64             *keyFramePtsIdx;
    cdx_int32              hasKeyFrameIdx;
    cdx_int32              length;
}Keyframe_idx;

struct VdecFlvIn
{
    FlvFileHeaderT    header;
    FlvTagsT          tag;
    FlvAudioTagsT     audioTag;
    FlvVideoTagsT     videoTag;

    cdx_uint32        lastKeyFrmPts; // the previous keyframe pts
    cdx_uint32        lastKeyFrmPos; // the previous keyframe pos, for seekTo while has
                                     // no keyframe index when not found keyframe.
    cdx_uint32        lastFrmPts;    // the previous frame pts
    cdx_uint32        lastFrmPos;    // the previous frame pos, for seekTo while has
                                     // no keyframe index.
    cdx_uint32        totalTime;
    cdx_uint32          durationMode; // 1 get the total time from the onmetadata

    FlvChunkT         dataChunk;
    cdx_uint8        *pktData;
    cdx_int32         sliceLen;
    CdxStreamT       *fp;
    cdx_uint32        lastTagHeadPos;
    cdx_int32         blockSizeType;
    Keyframe_idx      keyFrameIndex;

    cdx_int64         fileSize;
    cdx_uint32        isNetworkStream;
    cdx_uint32        seekAble;
    cdx_int32         packetNum;
    cdx_int32         nHeight;
    cdx_int32         nWidth;
};

typedef enum {
    CDX_AMF_DATA_TYPE_NUMBER      = 0x00,
    CDX_AMF_DATA_TYPE_BOOL        = 0x01,
    CDX_AMF_DATA_TYPE_STRING      = 0x02,
    CDX_AMF_DATA_TYPE_OBJECT      = 0x03,
    CDX_AMF_DATA_TYPE_NULL        = 0x05,
    CDX_AMF_DATA_TYPE_UNDEFINED   = 0x06,
    CDX_AMF_DATA_TYPE_REFERENCE   = 0x07,
    CDX_AMF_DATA_TYPE_MIXEDARRAY  = 0x08,
    CDX_AMF_DATA_TYPE_OBJECT_END  = 0x09,
    CDX_AMF_DATA_TYPE_ARRAY       = 0x0a,
    CDX_AMF_DATA_TYPE_DATE        = 0x0b,
    CDX_AMF_DATA_TYPE_LONG_STRING = 0x0c,
    CDX_AMF_DATA_TYPE_UNSUPPORTED = 0x0d,
} AMFType;

typedef enum {
    Metadatacreator            = 0x00,
    hasKeyframes               = 0x01,
    hasVideo                  = 0x02,
    hasAudio                  = 0x03,
    hasMetadata               = 0x04,
    canSeekToEnd              = 0x05,
    Duration                  = 0x06,
    Datasize                  = 0x07,
    Videosize                 = 0x08,
    Framerate                 = 0x09,
    Videodatarate             = 0x0A,
    Videocodecid              = 0x0B,
    Width                     = 0x0C,
    Height                    = 0x0D,
    Audiosize                 = 0x0E,
    Audiodatarate             = 0x0F,
    Audiocodecid              = 0x10,
    Audiosamplerate           = 0x11,
    Audiosamplesize           = 0x12,
    Stereo                    = 0x13,
    Filesize                  = 0x14,
    Lasttimestamp             = 0x15,
    Lastkeyframetimestamp     = 0x16,
    Lastkeyframelocation      = 0x17,
    Keyframes                 = 0x18,
    tagnone                   = 0x30,
}_Object_header;

typedef enum CDXTAGTYPE
{
    CDX_TagUnknown = 0x00,
    CDX_TagAudio = 0x08,
    CDX_TagVideo = 0x09,
    CDX_TagSubtitle = 0x12,
}CDX_TAGTYPE;
#endif
