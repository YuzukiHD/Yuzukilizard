/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxFlvParser.c
 * Description : Flv parser implementation.
 * History :
 *
 */

//#define CONFIG_LOG_LEVEL 4
#define LOG_TAG "CdxFlvParser.c"
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxFlvParser.h>
#include <CdxMemory.h>
#include <errno.h>

#define SAVE_VIDEO_STREAM    (0)
#define SAVE_AUDIO_STREAM    (0)

#define PLAY_AUDIO_BITSTREAM    (1)
#define PLAY_VIDEO_BITSTREAM    (1)

#define ABS_EDIAN_FLAG_MASK         ((unsigned int)(1<<16))
#define ABS_EDIAN_FLAG_LITTLE       ((unsigned int)(0<<16))
#define ABS_EDIAN_FLAG_BIG          ((unsigned int)(1<<16))

#define RESYNC_BUFFER_SIZE    8*1024

#define FLV_ABS(x) ((x) >= 0 ? (x) : -(x))

enum CdxFlvStatusE
{
    CDX_FLV_INITIALIZED, /*control , getMediaInfo, not prefetch or read, seekTo.*/
    //CDX_PSR_prepared,
    CDX_FLV_IDLE,
    CDX_FLV_PREFETCHING,
    CDX_FLV_PREFETCHED,
    CDX_FLV_SEEKING,
    CDX_FLV_EOS,
};

//extern unsigned int EndianConvert(unsigned int x, int n);

#define GET_STRING_BUF_MAX_LEN (256)

char *header[25] = {
                    "metadatacreator",
                    "hasKeyframes",
                    "hasVideo",
                    "hasAudio",
                    "hasMetadata",
                    "canSeekToEnd",
                    "duration",
                    "datasize",
                    "videosize",
                    "framerate",
                    "videodatarate",
                    "videocodecid",
                    "width",
                    "height",
                    "audiosize",
                    "audiodatarate",
                    "audiocodecid",
                    "audiosamplerate",
                    "audiosamplesize",
                    "stereo",
                    "filesize",
                    "lasttimestamp",
                    "lastkeyframetimestamp",
                    "lastkeyframelocation",
                    "keyframes"
                    };

typedef struct CdxFlvParserImplS
{
    CdxParserT      base;
    CdxStreamT      *stream;
    CdxPacketT      pkt;
    void            *privData;
    cdx_int32       exitFlag;
    cdx_int32       mErrno;
    cdx_int32       mStatus;
    cdx_int32       flags;

    cdx_bool        hasVideo;
    cdx_bool        hasAudio;
    cdx_bool        hasSubTitle;

    cdx_int8        bFirstVidFrm;
    cdx_int8        bDiscardAud;        //  1:discard, 0:transport

    cdx_int8        videoStreamIndex;
    cdx_int8        audioStreamIndex;
    cdx_int8        subTitleStreamIndex;

    cdx_uint32      totalFrames;
    cdx_uint32      pictureNum;

    cdx_uint32      nPreFRSpeed;       //previous ff/rr speed, for dynamic adjust
    cdx_uint32      nFRSpeed;          //fast forward and fast backward speed,
                                       //multiple of normal speed
    cdx_uint32      nFRPicShowTime;    //picture show time under fast forward and backward
    cdx_uint32      nFRPicCnt;         //picture count under ff/rr, for check if need delay

    cdx_uint32      nVidPtsOffset;     //video time offset
    cdx_uint32      nAudPtsOffset;     //audio time offset
    cdx_uint32      nSubPtsOffset;     //subtitle time offset

    cdx_int8        hasSyncVideo;      //flag, mark that if has sync video
    cdx_int8        hasSyncAudio;      //flag, mark that if has sync audio

    cdx_uint32      startPos;
    //pthread_t       nOpenPid;
    cdx_int64       fileSize;

    cdx_int64       firstVideoTagPos;       //for xunlei 265 seek to first keyframe.

    //cdx_int64       firstMediaTagPos;       //audio tag / video tag
#if 0
    OMX_VIDEO_PORTDEFINITIONTYPE    vFormat;
    struct AUDIO_CODEC_FORMAT       aFormat;
#endif
    AudioStreamInfo             aFormat;
    VideoStreamInfo             vFormat;
    SubtitleStreamInfo          tFormat;

    VideoStreamInfo             tempVformat;
    cdx_uint8                    *tempBuf;
    cdx_uint32                   tempBufLen;

    FlvChunkInfoT         curChunkInfo;  //current chunk information

    cdx_int32       h265_vps_sps_pps_state; //3: read vps 2: read sps 1: read pps,
                                            //0: back seek , -1: invalid
    cdx_uint32      h265_start_pos_stored;
    cdx_uint32      h265_data_size_stored;

    pthread_mutex_t lock;
    pthread_cond_t  cond;

#if SAVE_VIDEO_STREAM
    FILE* fpVideoStream;
#endif
#if SAVE_AUDIO_STREAM
    FILE* fpAudioStream[AUDIO_STREAM_LIMIT];
    char url[256];
#endif

    cdx_int32 bNoAvcSequenceHeader;
}CdxFlvParserImplT;

static double int2dbl(cdx_int64 v)
{
    double *datapoint;
    double tmpdata;
    datapoint = (double *)(&v);
    tmpdata = *datapoint;
    return tmpdata;
}
cdx_uint32 EndianConvert(cdx_uint32 x, cdx_int32 n)
{
    cdx_uint32  a[4];
    a[0] = x & 0xff;
    a[1] = (x>>8) & 0xff;
    a[2] = (x>>16) & 0xff;
    a[3] = (x>>24) & 0xff;

    if(n==4)
    {
        return (a[0]<<24) | (a[1]<<16) | (a[2]<<8) | (a[3]);
    }
    else if(n==3)
    {
        return (a[0]<<16) | (a[1]<<8) | (a[2]);
    }
    else if(n==2)
    {
        return (a[0]<<8) | (a[1]);
    }

    return a[0];
}
cdx_int16 FlvGetString(CdxStreamT *stream, cdx_char *buffer)
{
    cdx_uint32                re;
    cdx_int16              stringLength = 0;

    re = CdxStreamRead(stream, &stringLength, 2);
    if(re != 2)
    {
        CDX_LOGE("Read 2 Bytes failed.");
        return -1;
    }
    stringLength = EndianConvert(stringLength, 2);

    if(stringLength >= GET_STRING_BUF_MAX_LEN)
    {
        CdxStreamSeek(stream, GET_STRING_BUF_MAX_LEN, STREAM_SEEK_CUR);
        CDX_LOGI("string length is %d, return directly", stringLength);
        return stringLength;
    }

    //LOGV("stringLength,%x", stringLength);
    //LOGV("cdx_tell positon: %llx",cdx_tell(stream));
    if (stringLength > 0)
    {
        if(CdxStreamRead(stream, buffer, stringLength) != stringLength)
        {
            CDX_LOGE("Read string failed.");
            return -1;
        }
    }
    buffer[stringLength] = '\0';
    CDX_LOGV("String value: %s", buffer);
    return stringLength;
}
cdx_uint64 EndianConvertInt64(cdx_uint64 x, cdx_int32 n)
{
    cdx_uint64  a[8];
    a[0] = x & 0xff;
    a[1] = (x>>8) & 0xff;
    a[2] = (x>>16) & 0xff;
    a[3] = (x>>24) & 0xff;
    a[4] = (x>>32) & 0xff;
    a[5] = (x>>40) & 0xff;
    a[6] = (x>>48) & 0xff;
    a[7] = (x>>56) & 0xff;

    if(n == 8)
    {
        return (a[0]<<56) | (a[1]<<48) | (a[2]<<40) | (a[3]<<32) | (a[4]<<24) |
            (a[5]<<16) | (a[6]<<8) | (a[7]);
    }

    return a[0];
}

cdx_int16 FlvGetData(CdxStreamT *stream, cdx_int32 *data)
{
    double      ddata;
    double      *datapoint;
    //cdx_uint64  *datapoint;
    cdx_uint64   curdata;
    cdx_uint8    type;
    cdx_int32   len;

    len = CdxStreamRead(stream, &type, 1);
    if(len != 1 || type != CDX_AMF_DATA_TYPE_NUMBER)
    {
        CDX_LOGE("not CDX_AMF_DATA_TYPE_NUMBER");
        return -1;
    }

    if(CdxStreamRead(stream, &curdata, 8) != 8)
    {
        CDX_LOGE("get data failed.");
        return -1;
    }

    curdata = EndianConvertInt64(curdata, 8);
    datapoint = (double *)&curdata;
    ddata = (*datapoint) * 1000;
    *data = (cdx_int32)ddata;
    CDX_LOGV("*data: %d",*data);

    return 0;
}
cdx_int32 CheckTagValid(FlvTagsT *pTagHead, cdx_uint32 nTagSize)
{
    cdx_int32   uDataSize;

    uDataSize = EndianConvert(pTagHead->dataSize, 3);
    if(nTagSize == (uDataSize + sizeof(FlvTagsT) - 4 - 1))//preTagSize, codecType
    {
        return 0;
    }
    else
    {
        CDX_LOGE("CHECKTag Invalid! last_tag_size[%d]!=s->tag.dataSize[%d] + 11",
            nTagSize, uDataSize);
        return -1;
    }
}
static cdx_int16 FlvFindNextValidTag(struct VdecFlvIn* s)
{
    int            len;
    int            done;
    unsigned char* buf;
    int            i;
    int            ret;

    done = 0;
    buf = (unsigned char*)malloc(RESYNC_BUFFER_SIZE);
    if(buf == NULL)
    {
        CDX_LOGE("malloc failed.");
        return -1;
    }

    while(1)
    {
        len = CdxStreamRead(s->fp, buf, RESYNC_BUFFER_SIZE);
        if(len != RESYNC_BUFFER_SIZE)
        {
            CDX_LOGE("read failed.");
            break;
        }

        for(i = 0; i < RESYNC_BUFFER_SIZE - 15; i++)
        {
            if(buf[i+4] != 0x8 && buf[i+4] != 0x9 && buf[i+4] != 0x12)    //* check tag type.
                continue;

            if(buf[i+12] != 0 || buf[i+13] != 0 || buf[i+14] != 0)        //* check streamID.
                continue;

            //* it seems to be a valid tag (according to the tag type and stream id value).
            ret = CdxStreamSeek(s->fp, i-RESYNC_BUFFER_SIZE, STREAM_SEEK_CUR);
            //* seek backward to the tag position.
            if(ret < 0)
            {
                CDX_LOGE("Seek failed.");
                break;
            }
            done = 1;
            break;
        }

        if(done)
            break;
    }

    if(buf)
        free(buf);

    if(done)
        return 0;
    else
        return -1;
}

#define CHECK_FILEEND_TAG_NUM   (10)
#define SMALL_FLVFILE_SIZE      (20*1024*1024)
#define FLV_FILE_HEADER_SIZE    (9)
cdx_int32 GetTotalTimeFromEnd(struct VdecFlvIn *s)
{
    cdx_int32   i;
    cdx_int32   tmpret;
    cdx_int32   result;
    cdx_int32   len = 0;
    cdx_int32   last_tag_size = 0;
    cdx_uint32  nTotalTime = 0xffffffff;

    s->lastTagHeadPos = 0xffffffff;
    //try to get the last tag size

    result = CdxStreamSeek(s->fp, -4, STREAM_SEEK_END);
    if(result)
    {
        CDX_LOGV("Seek to the last tag failed!");
        goto _err0;

    }

    for(i = 0; i < CHECK_FILEEND_TAG_NUM; i++)
    {
        result = CdxStreamRead(s->fp, &last_tag_size, 4);
        if(result != 4)
        {
            CDX_LOGE("Get the last tag size failed!");
            goto _err0;
        }
        last_tag_size = EndianConvert(last_tag_size, 4);

        if(last_tag_size >= MAX_CHUNK_BUF_SIZE)
        {
            CDX_LOGE("The last tag is invalid!");
            goto _err0;
        }
        //jump to the beginning of the last tag
        result = CdxStreamSeek(s->fp, (cdx_int32)(0-(last_tag_size+8)), STREAM_SEEK_CUR);
        if(result)
        {
            CDX_LOGE("Seek to the last tag header failed!");
            goto _err0;
        }
        //read the last tag header to get total time
        if(0xffffffff == s->lastTagHeadPos)
        {
            s->lastTagHeadPos = CdxStreamTell(s->fp);
        }
        len = CdxStreamRead(s->fp, &s->tag, sizeof(FlvTagsT));

        if(len != sizeof(FlvTagsT))//end of file
        {
            CDX_LOGE("Read last tag head failed!");
            goto _err0;
        }

        tmpret = CheckTagValid(&s->tag, last_tag_size);
        if(tmpret != 0)
        {
            CDX_LOGE(" -------------- tag invalid");
            goto _err0;
        }
        if(8 == s->tag.tagType || 9 == s->tag.tagType)  //8:audio, 9:video, 18:script data
        {
            CDX_LOGV(" -------------- time stamp before convert = %x", s->tag.timeStamp);
            nTotalTime = EndianConvert(s->tag.timeStamp, 3);
            CDX_LOGV(" -------------- time stamp after convert = %x", nTotalTime);
            break;
        }
        else
        {
            CDX_LOGV("s->tag.tagType[%x], continue", s->tag.tagType);
            result = CdxStreamSeek(s->fp, (0-(cdx_int32)sizeof(FlvTagsT)), STREAM_SEEK_CUR);
            if(result)
            {
                CDX_LOGE("seek fail");
                goto _err0;
            }
        }
    }

    s->totalTime = nTotalTime;
    if(i < CHECK_FILEEND_TAG_NUM)
    {
        return 0;
    }
    else
    {
        CDX_LOGE("can't find av tag in last %d tags", CHECK_FILEEND_TAG_NUM);
        return 1;
    }

_err0:
    s->totalTime = 0xffffffff;
    s->lastTagHeadPos = 0xffffffff;
    return -1;
}

cdx_int32 GetTotalTimeFromHead(struct VdecFlvIn *s)
{
    cdx_int32    tmpret;
    cdx_int32    len = 0;
    cdx_uint32   nTotalTime = 0xffffffff;
    cdx_uint32   uLastTagheadPst = 0xffffffff;
    FlvTagsT     *prevTag, *curTag, *nextTag, *tmpTag;
    FlvTagsT     tag1, tag2, tag3;
    cdx_uint32   prevTagHeadPst, curTagHeadPst, nextTagHeadPst;

    tmpret = CdxStreamSeek(s->fp, FLV_FILE_HEADER_SIZE, STREAM_SEEK_SET);
    if(tmpret)
    {
        CDX_LOGE("Seek to the file head fail!");
        goto _err0;
    }
    prevTag = &tag1;
    curTag  = &tag2;
    nextTag = &tag3;

    prevTagHeadPst = CdxStreamTell(s->fp);
    len = CdxStreamRead(s->fp, prevTag, sizeof(FlvTagsT));
    if(len != sizeof(FlvTagsT))//end of file
    {
        CDX_LOGE("Try to get first flv tag failed! perhaps file end");
        goto _err0;
    }

    tmpret = CdxStreamSeek(s->fp, (EndianConvert(prevTag->dataSize,3)-1), STREAM_SEEK_CUR);
    if(tmpret)
    {
        CDX_LOGE("Seek to second tag head fail! first tag is wrong or file wrong!");
        goto _err0;
    }

    curTagHeadPst = CdxStreamTell(s->fp);
    len = CdxStreamRead(s->fp, curTag, sizeof(FlvTagsT));
    if(len != sizeof(FlvTagsT))//end of file
    {
        CDX_LOGE("Try to get second flv tag failed! perhaps file end.");
        goto _err0;
    }
    tmpret = CheckTagValid(prevTag, EndianConvert(curTag->preTagsize,4));
    if(0 == tmpret)
    {
        tmpret = CdxStreamSeek(s->fp, (cdx_int32)(EndianConvert(curTag->dataSize,3)-1),
            STREAM_SEEK_CUR);
        if(tmpret)
        {
            CDX_LOGV("Seek to third tag head fail! prevTag is last tag!");
            nTotalTime = EndianConvert(prevTag->timeStamp, 3);
            uLastTagheadPst = prevTagHeadPst;
            goto _quit;
        }
    }
    else
    {
        CDX_LOGE("prevTag invalid\n");
        goto _err0;
    }

    while(1)
    {
        nextTagHeadPst = CdxStreamTell(s->fp);
        len = CdxStreamRead(s->fp, nextTag,sizeof(FlvTagsT));
        if(len != sizeof(FlvTagsT))//end of file
        {
            CDX_LOGV("Try to get flv tag failed! perhaps file end.");
            if(len >= 4)
            {
                CDX_LOGV("can verify curTag.");
                tmpret = CheckTagValid(curTag, EndianConvert(nextTag->preTagsize,4));
                if(0 == tmpret)
                {
                    nTotalTime = EndianConvert(curTag->timeStamp, 3);
                    uLastTagheadPst = curTagHeadPst;
                    goto _quit;
                }
                else
                {
                    CDX_LOGV("curTag invalid, use prevTag.");
                    nTotalTime = EndianConvert(prevTag->timeStamp, 3);
                    uLastTagheadPst = prevTagHeadPst;
                    goto _quit;
                }
            }
            else
            {
                CDX_LOGV("only can use prevTag.");
                nTotalTime = EndianConvert(prevTag->timeStamp, 3);
                uLastTagheadPst = prevTagHeadPst;
                goto _quit;
            }
        }
        else
        {
           tmpret = CheckTagValid(curTag, EndianConvert(nextTag->preTagsize, 4));
            if(0 == tmpret)
            {

                tmpret = CdxStreamSeek(s->fp, (cdx_int32)(EndianConvert(nextTag->dataSize,3)-1),
                    STREAM_SEEK_CUR);
                if(tmpret)
                {
                    CDX_LOGV("Seek to next tag head fail! cur tag is last tag!");
                    nTotalTime = EndianConvert(curTag->timeStamp, 3);
                    uLastTagheadPst = curTagHeadPst;
                    goto _quit;
                }
                else
                {
                    tmpTag = prevTag;

                    prevTag = curTag;
                    prevTagHeadPst = curTagHeadPst;

                    curTag = nextTag;
                    curTagHeadPst = nextTagHeadPst;

                    nextTag = tmpTag;
                    continue;
                }
            }
            else
            {
                CDX_LOGV("curTag invalid, use prevTag.");
                nTotalTime = EndianConvert(prevTag->timeStamp, 3);
                uLastTagheadPst = prevTagHeadPst;
                goto _quit;
            }
        }
    }
_quit:
    s->totalTime = nTotalTime;
    s->lastTagHeadPos = uLastTagheadPst;
    return 0;

_err0:
    s->totalTime = 0xffffffff;
    s->lastTagHeadPos = 0xffffffff;
    return -1;
}

cdx_int32 FlvGetTotalTime(struct VdecFlvIn *s)
{
    cdx_int32   result;
    cdx_int64   nRestorePos = CdxStreamTell(s->fp);
    result = GetTotalTimeFromEnd(s);
    if(result >= 0)
    {
        CDX_LOGI(" ------------------ total time from end = %d", s->totalTime);
        goto _err0;
    }

    if(s->fileSize > SMALL_FLVFILE_SIZE)
    {
        CDX_LOGI("filesize[%lld] > [%d]byte, don't get total time from head!",
                                  (cdx_int64)s->fileSize, SMALL_FLVFILE_SIZE);
        result =  -1;
        goto _err0;
    }
    result = GetTotalTimeFromHead(s);
    CDX_LOGV(" ------------------ total time from head = %d", s->totalTime);
_err0:
    CdxStreamSeek(s->fp, nRestorePos, STREAM_SEEK_SET);
    return result;
}

/**************************************************
  FlvBuildKeyFrameIndex()
    Functionality :  build keyframe index
    Return value  :
    0   allright
**************************************************/
cdx_int16 FlvBuildKeyFrameIndex(struct VdecFlvIn *s)
{
    cdx_int32       len;
    cdx_uint32      pos = 0;
    cdx_uint32      nextPos = 0;
    cdx_char        type = 0;
    cdx_uint32      keyframes_counter = 0;
    cdx_char        buffer[GET_STRING_BUF_MAX_LEN];
    cdx_int32       length;
    cdx_int32       pre_length;
    cdx_int32       index;
    cdx_int32       arrynum;
    double          temdouble;

    CdxStreamSeek(s->fp, 9, STREAM_SEEK_SET);
    len = CdxStreamRead(s->fp, &s->tag, sizeof(FlvTagsT));
    if(len != sizeof(FlvTagsT) || s->tag.tagType != 0x12)
    {
        CDX_LOGE("Get script failed.");
        return -1;
    }

    //used for seek next tag
    pos = CdxStreamTell(s->fp);
    s->tag.dataSize = EndianConvert(s->tag.dataSize, 3);
    //LOGV("dataSize:  %x", s->tag.dataSize);
    nextPos = pos + s->tag.dataSize - 1;

    //parse first amf
    if(FlvGetString(s->fp, buffer) < 0 || strcmp(buffer, "onMetaData") != 0)
    {
        CDX_LOGE("Get first amf failed.");
        return -1;
    }

    //parse second amf
    len = CdxStreamRead(s->fp, &type, 1);
    if(len != 1 || type != CDX_AMF_DATA_TYPE_MIXEDARRAY)
    {
        CDX_LOGE("Get second amf failed.");
        return -1;
    }

    if(CdxStreamRead(s->fp, &arrynum, 4) != 4)
    {
        CDX_LOGE("Get arrynum failed.");
        return -1;
    }
    arrynum = EndianConvert(arrynum, 4);

    while((arrynum-- > 0) && (FlvGetString(s->fp, buffer) > 0))
    {
        int headertag;

        for(headertag = 0; headertag < 25; headertag++)
        {
            if(!strcmp(buffer, header[headertag]))
            {
                break;
            }
            //LOGV("headertag headertag headertag,value: %d",headertag);
        }

        switch(headertag)
        {
            case Metadatacreator:
            {
                len = CdxStreamRead(s->fp, &type, 1);
                if(len != 1 || type != CDX_AMF_DATA_TYPE_STRING)
                {
                    CDX_LOGE("type(%x)",type);
                    return -1;
                }

                if(FlvGetString(s->fp, buffer) < 0)
                {
                    CDX_LOGE("Get Metadatacreator string failed.");
                    return -1;
                }
                break;
            }

            case hasKeyframes:
            {
                if(CdxStreamSeek(s->fp, 2, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("hasKeyframes seek failed.");
                    return -1;
                }
                break;
            }

            case hasVideo:
            {
                if(CdxStreamSeek(s->fp, 2, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("hasVideo seek failed.");
                    return -1;
                }
                break;
            }

            case hasAudio:
            {
                if(CdxStreamSeek(s->fp, 2, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("hasAudio seek failed.");
                    return -1;
                }
                break;
            }

            case hasMetadata:
            {
                if(CdxStreamSeek(s->fp, 2, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("hasMetadata seek failed.");
                    return -1;
                }
                break;
            }
//            case canSeekToEnd:
//                {
//                    if(CdxStreamSeek(s->fp, 2, STREAM_SEEK_CUR))
//                    {
//                        return -1;
//                    }
//                    break;
//                }

            case Duration:
            {
                if(FlvGetData(s->fp, (cdx_int32 *)&s->totalTime) < 0)
                {
                    CDX_LOGE("Duration seek failed.");
                    return -1;
                }

                s->durationMode = 1;
                CDX_LOGV("s->totalTime: %u", s->totalTime);
                break;
            }

            case Datasize:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Datasize seek failed.");
                    return -1;
                }
                break;
            }

            case Videosize:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Videosize seek failed.");
                    return -1;
                }
                break;
            }

            case Framerate:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Framerate seek failed.");
                    return -1;
                }
                break;
            }

            case Videodatarate:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Videodatarate seek failed.");
                    return -1;
                }
                break;
            }

            case Videocodecid:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Videocodecid seek failed.");
                    return -1;
                }
                break;
            }

            case Width:
            {
                /*
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Width seek failed.");
                    return -1;
                }*/
                int tmp = 0;
                if(FlvGetData(s->fp, &tmp) < 0)
                {
                    CDX_LOGE("get width failed.");
                    return -1;
                }
                //logd("width(%d)", tmp/1000);
                s->nWidth = tmp/1000;
                break;
            }

            case Height:
            {
                /*
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Height seek failed.");
                    return -1;
                }*/

                int tmp = 0;
                if(FlvGetData(s->fp, &tmp) < 0)
                {
                    CDX_LOGE("get height failed.");
                    return -1;
                }
                //logd("height(%d)", tmp/1000);
                s->nHeight = tmp/1000;
                break;
            }

            case Audiosize:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Audiosize seek failed.");
                    return -1;
                }
                break;
            }

            case Audiodatarate:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Audiodatarate seek failed.");
                    return -1;
                }
                break;
            }

            case Audiocodecid:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Audiocodecid seek failed.");
                    return -1;
                }
                break;
            }

            case Audiosamplerate:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Audiosamplerate seek failed.");
                    return -1;
                }
                break;
            }

            case Audiosamplesize:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Audiosamplesize seek failed.");
                    return -1;
                }
                break;
            }

            case Stereo:
            {
                if(CdxStreamSeek(s->fp, 2, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Stereo seek failed.");
                    return -1;
                }
                break;
            }

            case Filesize:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Filesize seek failed.");
                    return -1;
                }
                break;
            }

            case Lasttimestamp:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Lasttimestamp seek failed.");
                    return -1;
                }
                break;
            }

            case Lastkeyframetimestamp:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Lastkeyframetimestamp seek failed.");
                    return -1;
                }
                break;
            }

            case Lastkeyframelocation:
            {
                if(CdxStreamSeek(s->fp, 9, STREAM_SEEK_CUR))
                {
                    CDX_LOGE("Lastkeyframelocation seek failed.");
                    return -1;
                }
                break;
            }

            case Keyframes:
              {
                len = CdxStreamRead(s->fp, &type, 1);
                if(len != 1 || type != CDX_AMF_DATA_TYPE_OBJECT)
                {
                    CDX_LOGE("Keyframes Read failed.");
                    return -1;
                }
                pre_length = 0;
                for(keyframes_counter = 0; keyframes_counter < 2; keyframes_counter++)
                {
                    //*set keyframes positon index
                    if(FlvGetString(s->fp, buffer) < 0)
                    {
                        return -1;
                    }
                    if(strcmp(buffer, "filepositions") && strcmp(buffer, "times"))
                    {
                        CDX_LOGE("has no filepositions/times");
                        return -1;
                    }

                    len = CdxStreamRead(s->fp, &type, 1);
                    if(len != 1 || type != CDX_AMF_DATA_TYPE_ARRAY)
                    {
                        CDX_LOGE("read type failed.");
                        return -1;
                    }

                    if(CdxStreamRead(s->fp, &length, 4) != 4)
                    {
                        CDX_LOGE("read length failed.");
                        return -1;
                    }

                    //LOGV("CDX_AMF_DATA_TYPE_ARRAY,length,%x",length);
                    length = EndianConvert(length, 4);
                    CDX_LOGV("CDX_AMF_DATA_TYPE_ARRAY,length,%x",length);
                    if(pre_length != 0 && pre_length != length)
                        return -1;
                    s->keyFrameIndex.length = length;
                    pre_length = length;
                    if(strcmp(buffer, "times"))//case position index
                    {

                        if((s->keyFrameIndex.keyFramePosIdx =
                            (cdx_int64 *)malloc(sizeof(cdx_int64)*length)) == NULL)
                        {
                            CDX_LOGE("malloc keyFramePosIdx failed.");
                            return -1;
                        }

                        for(index = 0; index < length; index++)
                        {
                            if(CdxStreamRead(s->fp, &type, 1) != 1 ||
                                type != CDX_AMF_DATA_TYPE_NUMBER)
                            {
                                CDX_LOGE("get type failed.");
                                return -1;
                            }

                            //LOGV("the type is %x",type);
                            if(CdxStreamRead(s->fp,
                                &s->keyFrameIndex.keyFramePosIdx[index], 8) != 8)
                            {
                                CDX_LOGE("read pos index failed.");
                                return -1;
                            }
                            s->keyFrameIndex.keyFramePosIdx[index] =
                                EndianConvertInt64(s->keyFrameIndex.keyFramePosIdx[index], 8);
                            temdouble = int2dbl(s->keyFrameIndex.keyFramePosIdx[index]);
                            s->keyFrameIndex.keyFramePosIdx[index] = (cdx_int64)temdouble;
                            CDX_LOGV("s->keyFrameIndex.keyFramePosIdx[0]: %llx",
                                s->keyFrameIndex.keyFramePosIdx[index]);
                        }
                    }
                    else//times
                    {
                        if((s->keyFrameIndex.keyFramePtsIdx =
                            (cdx_int64 *)malloc(sizeof(cdx_int64)*length)) == NULL)
                        {
                            CDX_LOGE("malloc keyFramePtsIdx failed.");
                            return -1;
                        }

                        for(index = 0; index < length; index++)
                        {
                            if(CdxStreamRead(s->fp, &type, 1) != 1 ||
                                type != CDX_AMF_DATA_TYPE_NUMBER)
                            {
                                CDX_LOGE("get type failed.");
                                return -1;
                            }

                            if(CdxStreamRead(s->fp, &s->keyFrameIndex.keyFramePtsIdx[index], 8)!=8)
                            {
                                CDX_LOGD("read error");
                                return -1;
                            }

                            s->keyFrameIndex.keyFramePtsIdx[index] =
                                EndianConvertInt64(s->keyFrameIndex.keyFramePtsIdx[index], 8);
                            temdouble = int2dbl(s->keyFrameIndex.keyFramePtsIdx[index]) * 1000;//us
                            s->keyFrameIndex.keyFramePtsIdx[index] = (cdx_int64)temdouble;
                        }
                    }
                }
                if(CdxStreamSeek(s->fp, nextPos, STREAM_SEEK_SET))
                {
                    CDX_LOGE("seek failed.");
                    return -1;
                }
                //LOGV("cdx_tell(s->fp): %llx", cdx_tell(s->fp));
                return 0;
            }

            default:
            {
                if(CdxStreamRead(s->fp, &type, 1) != 1)
                {
                    CDX_LOGE("Get type failed.");
                    return -1;
                }

                switch(type)
                {
                    case CDX_AMF_DATA_TYPE_NUMBER:
                    {
                        if(CdxStreamSeek(s->fp, 8, STREAM_SEEK_CUR))
                        {
                            CDX_LOGE("CDX_AMF_DATA_TYPE_NUMBER seek failed.");
                            return -1;
                        }
                        break;
                    }
                    case CDX_AMF_DATA_TYPE_BOOL:
                    {
                        if(CdxStreamSeek(s->fp, 1, STREAM_SEEK_CUR))
                        {
                            CDX_LOGE("CDX_AMF_DATA_TYPE_BOOL seek failed.");
                            return -1;
                        }
                        break;
                    }
                    case CDX_AMF_DATA_TYPE_STRING:
                    {
                        if(FlvGetString(s->fp, buffer) <= 0)
                        {
                            CDX_LOGE("CDX_AMF_DATA_TYPE_STRING FlvGetString failed.");
                            return -1;
                        }
                        break;
                    }

                    case CDX_AMF_DATA_TYPE_DATE:
                    {
                        if(CdxStreamSeek(s->fp, 10, STREAM_SEEK_CUR))
                        {
                            CDX_LOGE("CDX_AMF_DATA_TYPE_DATE seek failed.");
                            return -1;
                        }
                        break;
                    }
                    case CDX_AMF_DATA_TYPE_ARRAY:
                    {
                        if(CdxStreamRead(s->fp, &length, 4) != 4)
                        {
                            CDX_LOGE("CDX_AMF_DATA_TYPE_ARRAY Read failed.");
                            return -1;
                        }
                        if(length != 0)
                            return -1;
                        break;
                    }
                    default:
                    {
                        CDX_LOGE("default.");
                        return -1;
                    }
                }
            }
        }
    }
    CDX_LOGV("arrynum: %d", arrynum);
    return -1;
}

/**************************************************
  FLV_reader_open_file()
    Functionality : Open the Flv file to be read
    Return value  :
    0   allright
    -1   file open error
    -2   wrong format
**************************************************/
cdx_int16 FlvReaderOpenFile(CdxFlvParserImplT *impl)
{
    cdx_int32               result;
    struct VdecFlvIn      *s;
    FlvFileHeaderT         *t;
    cdx_int32               len, check_audio, check_video;
    cdx_int32               last_tag_size;
    unsigned char           c;
    unsigned char  cc[13];
    //cdx_int64 firstAudioTagPos = 0;

    // initialize
    s = (struct VdecFlvIn *)impl->privData;
    t = &s->header;
    s->fp = impl->stream;
    check_audio = 0;
    check_video = 0;
    s->durationMode = 0;
    s->lastTagHeadPos = 0xffffffff;
    s->totalTime = 0xffffffff;
    s->isNetworkStream = CdxStreamIsNetStream(impl->stream);
    impl->h265_vps_sps_pps_state = -1;

    s->fileSize = CdxStreamSize(s->fp);
    impl->fileSize = s->fileSize;
    s->seekAble = s->fileSize > 0 && CdxStreamSeekAble(impl->stream);
    if(s->seekAble)
    {
        result = CdxStreamSeek(s->fp, 0, STREAM_SEEK_SET);
        if(result < 0)
        {
            CDX_LOGE("Seek to file header failed!");
            return -1;
        }
    }

    len = CdxStreamRead(s->fp, cc, 9);
    CDX_LOGV("FLV begin:%x %x %x %x %x %x %x %x %x %x %x %x",cc[0],cc[1],cc[2],
                      cc[3],cc[4],cc[5],cc[6],cc[7],cc[8],cc[9],cc[10],cc[11]);
    t->signature0             = cc[0];
    t->signature1             = cc[1];
    t->signature2             = cc[2];
    t->version                = cc[3];
    t->typeFlagVideo          = cc[4]&0x1;
    t->reserved1              = (cc[4]>>1)&0x1;
    t->typeFlagAudio          = (cc[4]>>2)&0x1;
    t->reserved0              = (cc[4]>>3)&0x1F;
    t->dataOffset             = (cc[5]<<24)|(cc[6]<<16)|(cc[7]<<8)|(cc[8]);
    CDX_LOGV("FLV file header:%x %x %x %x %x %x %x %x %x ",t->signature0,
        t->signature1,t->signature2,t->version,t->typeFlagVideo, t->reserved1,
                            t->typeFlagAudio,t->reserved0 , t->dataOffset );

    if(len != 9)
    {
        CDX_LOGE("Read flv file header failed.");
        return -1;
    }
    if (t->signature0 != 0x46 || t->signature1 != 0x4c || t->signature2 != 0x56
                                                            || t->version > 1)
    {
        CDX_LOGE("Flv file header error.");
        return -1;
    }

    //--------------------------------------------------------------------------
    //try to build keyFrameIndex
    //--------------------------------------------------------------------------
    if(s->seekAble)
    {
        if(FlvBuildKeyFrameIndex(s) < 0)
        {
            CDX_LOGV("has no keyframe index");
            s->keyFrameIndex.hasKeyFrameIdx = 0;
            if(s->keyFrameIndex.keyFramePosIdx)
            {
                free(s->keyFrameIndex.keyFramePosIdx);
                s->keyFrameIndex.keyFramePosIdx = NULL;
            }

            if(s->keyFrameIndex.keyFramePtsIdx)
            {
                free(s->keyFrameIndex.keyFramePtsIdx);
                s->keyFrameIndex.keyFramePtsIdx = NULL;
            }
        }
        else
        {
            CDX_LOGV("has keyframe index");
            s->keyFrameIndex.hasKeyFrameIdx = 1;
        }
    }

    CDX_LOGV("s->totalTime: %u, s->seekAble(%d)", s->totalTime, s->seekAble);

    //--------------------------------------------------------------------------
    //try to get the total play time by the last tag time stamp or from the Metadata
    //--------------------------------------------------------------------------
    if(!s->isNetworkStream)
    {
        if((s->durationMode == 0 || s->totalTime == 0) && s->seekAble == 1)
        {
            //try to get the last tag size
            result = CdxStreamSeek(s->fp, -4, STREAM_SEEK_END);
            if(result)
            {
                CDX_LOGE("Seek to the last tag failed!");
                return -1;
            }
            result = CdxStreamRead(s->fp, &last_tag_size, 4);
            if(result != 4)
            {
                CDX_LOGE("Get the last tag size failed!");
                return -1;
            }
            last_tag_size = EndianConvert(last_tag_size, 4);
            if(last_tag_size >= MAX_CHUNK_BUF_SIZE)
            {
                CDX_LOGW("The last tag is invalid!\n");
                goto _skip_check_file_end;
            }
            //jump to the beginning of the last tag
            result = CdxStreamSeek(s->fp, (0-(last_tag_size+8)), STREAM_SEEK_END);//preTagsize(2)
            if(result)
            {
                CDX_LOGW("Seek to the last tag header failed!");
                goto _skip_check_file_end;
            }
            //read the last tag header to get total time
            s->lastTagHeadPos = CdxStreamTell(s->fp);
            len = CdxStreamRead(s->fp, &s->tag, sizeof(FlvTagsT));
            if(len != sizeof(FlvTagsT))//end of file
            {
                CDX_LOGW("Read last tag head failed!");
                s->lastTagHeadPos = 0xffffffff;
                goto _skip_check_file_end;
            }

        //    s->tag.dataSize = EndianConvert(s->tag.dataSize,3);
        //    if(last_tag_size == (s->tag.dataSize+sizeof(FlvTagsT)-4-1))
        //    {
        //        s->totalTime = EndianConvert(s->tag.timeStamp, 3);
        //    }

                FlvGetTotalTime(s);
        }

        if(s->lastTagHeadPos == 0xffffffff) //for local file seek from end, we need lastTagHeadPos.
        {
            result = CdxStreamSeek(s->fp, 0-RESYNC_BUFFER_SIZE, STREAM_SEEK_END);
            if(result < 0)
            {
                CDX_LOGE("seek failed.");
                goto _skip_check_file_end;
            }
            result = FlvFindNextValidTag(s);
            if(result < 0)
            {
                CDX_LOGE("xxx not find valid tag.");
                goto _skip_check_file_end;
            }
            s->lastTagHeadPos = CdxStreamTell(s->fp);
            CDX_LOGD("last tag head pos:%x", s->lastTagHeadPos);
        }
    }
    CDX_LOGV("s->totalTime: %u", s->totalTime);

    _skip_check_file_end:
    //--------------------------------------------------------------------------
    //process the audio and video format information
    //--------------------------------------------------------------------------
    if(s->seekAble == 1)
    {
        result = CdxStreamSeek(s->fp, (cdx_uint32)t->dataOffset, STREAM_SEEK_SET);
        if(result)
        {
            CDX_LOGW("Seek to the first tag failed!");
            return -1;
        }
    }

    do
    {
        cdx_int64 curPos1 = CdxStreamTell(s->fp);
        //char *p=&s->tag;
        len = CdxStreamRead(s->fp, &s->tag, sizeof(FlvTagsT)-1);
        //logd("xxxxxxxxx %x %x %x %x %x %x %x %x  ",p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        if(len != (sizeof(FlvTagsT)-1))//end of file
        {
            CDX_LOGV("Read flv tag failed.");
            break;
        }
        s->tag.dataSize = EndianConvert(s->tag.dataSize, 3);
        if(s->tag.tagType == 8 && !check_audio /*&& t->typeFlagAudio*/)
        {
            CDX_LOGV("xxxxxxxxxxxxxxxxx audio tag. pos:%lld", CdxStreamTell(s->fp));
            //firstAudioTagPos = curPos;

            len = CdxStreamRead(s->fp, &c, 1);
            if(len != 1)//end of file
            {
                CDX_LOGW("Get the audio format from the first tag failed!");
                break;
            }
            check_audio = 1;
            s->audioTag.soundFormat = c>>4;
            s->audioTag.soundRate = (c>>2)&3;
            s->audioTag.soundSize = (c&2)>>1;
            s->audioTag.soundType = c&1;

            switch(s->audioTag.soundFormat)
            {
                case 0:
                {
                    //pcm data without compression
                    impl->aFormat.eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    impl->aFormat.eSubCodecFormat = WAVE_FORMAT_PCM;
                    impl->aFormat.nCodecSpecificDataLen = 0;
                    impl->aFormat.pCodecSpecificData = NULL;
                    break;
                }
#if 1
                case 1:
                {
                    //ADPCM format
                    impl->aFormat.eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    impl->aFormat.eSubCodecFormat = ADPCM_CODEC_ID_SWF | ABS_EDIAN_FLAG_LITTLE;
                    //impl->aFormat.nCodecSpecificDataLen = 0;
                    //impl->aFormat.pCodecSpecificData = (char *)(s->tag.dataSize - 1);
                    impl->aFormat.nBlockAlign = (int)(s->tag.dataSize - 1);
                    break;
                }
#endif
                case 2:
                {
                    //mp3 format
                    impl->aFormat.eCodecFormat = AUDIO_CODEC_FORMAT_MP3;
                    impl->aFormat.nCodecSpecificDataLen = 0;
                    impl->aFormat.pCodecSpecificData = NULL;
                    break;
                }

                case 10:
                {
                    //AAC format
                    impl->aFormat.eCodecFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
                    impl->aFormat.eSubCodecFormat = 0;
                    break;
                }

                default:
                {
                    CDX_LOGW("Unknown audio bitstream type(%x)", s->audioTag.soundFormat);
                    impl->aFormat.eCodecFormat = AUDIO_CODEC_FORMAT_UNKNOWN;
                    break;
                }
            }

            if(s->audioTag.soundType)
            {
                //stereo
                impl->aFormat.nChannelNum = 2;
            }
            else
            {
                //mono
                impl->aFormat.nChannelNum = 1;
            }

            if(s->audioTag.soundSize == 0)
            {
                impl->aFormat.nBitsPerSample = 8;
            }
            else
            {
                impl->aFormat.nBitsPerSample = 16;
            }

            switch(s->audioTag.soundRate)
            {
                case 0:
                {
                    impl->aFormat.nSampleRate = 5500;
                    break;
                }
                case 1:
                {
                    impl->aFormat.nSampleRate = 11025;
                    break;
                }
                case 2:
                {
                    impl->aFormat.nSampleRate = 22050;
                    break;
                }
                default:
                {
                    impl->aFormat.nSampleRate = 44100;
                    break;
                }
            }

            if(impl->aFormat.eCodecFormat == AUDIO_CODEC_FORMAT_MPEG_AAC_LC)
            {
                result = CdxStreamRead(s->fp, s->dataChunk.buffer,
                    (cdx_uint32)(s->tag.dataSize - 1));
                if(result != (cdx_int32)(s->tag.dataSize - 1))
                {
                    CDX_LOGW("Read tag data failed!");
                    break;
                }
                if(s->dataChunk.buffer[0] != 0)
                {
                    impl->aFormat.eCodecFormat = AUDIO_CODEC_FORMAT_UNKNOWN;
                }

                impl->aFormat.nCodecSpecificDataLen = (cdx_uint32)(s->tag.dataSize - 2);
                //impl->aFormat.pCodecSpecificData = (char *)(s->dataChunk.buffer + 1);
                if(impl->aFormat.nCodecSpecificDataLen)
                {
                    impl->aFormat.pCodecSpecificData=
                        malloc(impl->aFormat.nCodecSpecificDataLen);
                    if(impl->aFormat.pCodecSpecificData != NULL)
                    {
                        memcpy(impl->aFormat.pCodecSpecificData, s->dataChunk.buffer + 1,
                            impl->aFormat.nCodecSpecificDataLen);
                    }
                }
                else
                {
                    impl->aFormat.pCodecSpecificData = 0;
                }
                impl->aFormat.nSampleRate = 0;
                impl->aFormat.nChannelNum = 0;
            }
            else
            {
                char *tempBuf = malloc(s->tag.dataSize - 1);
                if(tempBuf == NULL)
                {
                    CDX_LOGE("malloc failed.");
                    return -1;
                }
                result = CdxStreamRead(s->fp, tempBuf, s->tag.dataSize - 1);
                //result = CdxStreamSeek(s->fp, (cdx_int32)(s->tag.dataSize-1), STREAM_SEEK_CUR);
                free(tempBuf);
                if(result != s->tag.dataSize - 1)
                {
                    CDX_LOGW("Skip tag data failed!");
                    break;
                }
            }
        }
        else if(s->tag.tagType == 9 && !check_video && t->typeFlagVideo)
        {
            CDX_LOGV("xxxxxxxxxxxxxxxxxx video tag.");
            if(s->tag.dataSize >= sizeof(FlvVideoTagsT))
            {
                len = CdxStreamRead(s->fp, &s->videoTag, sizeof(FlvVideoTagsT));
                if(len != sizeof(FlvVideoTagsT))//end of file
                {
                    CDX_LOGE("read failed.");
                    return -1;//FILE_PARSER_READ_FILE_FAIL;
                }

                check_video = 1;

                char *tempBuf = malloc(s->tag.dataSize-sizeof(FlvVideoTagsT));
                if(tempBuf == NULL)
                {
                    CDX_LOGE("malloc failed. size=%u, pos:%lld",
                      s->tag.dataSize-(unsigned int)(sizeof(FlvVideoTagsT)), CdxStreamTell(s->fp));
                    return -1;
                }
                result = CdxStreamRead(s->fp, tempBuf, s->tag.dataSize-sizeof(FlvVideoTagsT));
                if(result != (cdx_int32)(s->tag.dataSize-sizeof(FlvVideoTagsT)))
                {
                    CDX_LOGW("Skip first video tag data failed!");
                    free(tempBuf);
                    tempBuf = NULL;
                    break;
                }
                if(s->seekAble != 1)
                {
                    if (s->videoTag.frameType == 1) //keyframe
                    {
                        impl->firstVideoTagPos = curPos1;
                    }
                    else
                    {
                        CDX_LOGW("The first video frame is not keyframe!");
                    }
                    char *videoTotalData = (char *)malloc(s->tag.dataSize);
                    if(videoTotalData == NULL)
                    {
                        CDX_LOGE("malloc failed.");
                        if(tempBuf)
                        {
                            free(tempBuf);
                            tempBuf = NULL;
                        }
                        return -1;
                    }else
                    {
                        memcpy(videoTotalData, (char *)&s->videoTag, sizeof(FlvVideoTagsT));
                        memcpy(videoTotalData + sizeof(FlvVideoTagsT),
                            tempBuf, s->tag.dataSize-sizeof(FlvVideoTagsT));
                    }
                    free(tempBuf);
                    tempBuf = NULL;
                    if (s->videoTag.codecID == 7)
                    {
                        {//--------------------------------------------------------------
                            if(!impl->tempBuf)
                            {
                                impl->tempBufLen = s->tag.dataSize -1-4;
                                //if AVC, skip first 4 bytes, AVCPacketType(1B), CompositionTime(3B)
                                if(impl->tempBufLen > 0)
                                {
                                    impl->tempBuf = malloc(impl->tempBufLen);
                                    if(!impl->tempBuf)
                                    {
                                        CDX_LOGE("malloc failed.");
                                        if(videoTotalData)
                                        {
                                            free(videoTotalData);
                                            videoTotalData = NULL;
                                        }
                                        return -1;
                                    }
                                    memcpy(impl->tempBuf, videoTotalData+1+4, impl->tempBufLen);
                                    free(videoTotalData);
                                }
                            }
                        }
                        if (s->dataChunk.buffer[1] == 1)//AVC NALU
                        {
                            break;
                        }
                    }
                    else
                    {//--------------------------------------------------------------
                        if(!impl->tempBuf)
                        {
                            impl->tempBufLen = s->tag.dataSize - 1;
                            impl->tempBuf = malloc(impl->tempBufLen);
                            if(!impl->tempBuf)
                            {
                                CDX_LOGE("malloc failed.");
                                if(videoTotalData)
                                {
                                    free(videoTotalData);
                                    videoTotalData = NULL;
                                }
                                return -1;
                            }
                            memcpy(impl->tempBuf, (char *)s->dataChunk.buffer+1, impl->tempBufLen);
                        }
                    }
                }
                else
                {
                    free(tempBuf);
                    tempBuf = NULL;
                }

                #if 0
                result = CdxStreamSeek(s->fp, (cdx_int32)(s->tag.dataSize-sizeof(FlvVideoTagsT)),
                STREAM_SEEK_CUR);
                if(result)
                {
                    CDX_LOGW("Skip first video tag data failed!");
                    break;
                }
                #endif
            }
            else
            {
                CDX_LOGD("s->tag.dataSize=%u", s->tag.dataSize);
                result = CdxStreamSkip(s->fp, s->tag.dataSize);
                if(result < 0)
                {
                    CDX_LOGE("skip failed.");
                    return -1;
                }
                impl->bNoAvcSequenceHeader = 1;
            }
        }
        else
        {
            CDX_LOGV("xxxxxxxxxxxxxxxxxxxxxxxxx metadata. pos:%lld", CdxStreamTell(s->fp));
            char *tempBuf = malloc(s->tag.dataSize);
            if(tempBuf == NULL)
            {
                CDX_LOGE("malloc failed.");
                return -1;
            }
            result = CdxStreamRead(s->fp, tempBuf, s->tag.dataSize);
            free(tempBuf);
            if(result != s->tag.dataSize)
            {
                CDX_LOGW("Seek file failed!");
                break;
            }
            #if 0
            result = CdxStreamSeek(s->fp, (cdx_int32)s->tag.dataSize, STREAM_SEEK_CUR);
            if(result < 0)
            {
                CDX_LOGW("Seek file failed!");
                break;
            }
            #endif
        }
    } while((t->typeFlagVideo && !check_video) ||(!check_audio /*&& t->typeFlagAudio*/));

    if(t->typeFlagVideo)
    {
        if(s->videoTag.codecID == 2)//Sorenson H.263
        {
            impl->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_SORENSSON_H263;
            //impl->vFormat.eCompressionSubFormat = CEDARV_MPEG4_SUB_FORMAT_SORENSSON_H263;
            c = (s->videoTag.PicSize0<<1) | s->videoTag.picSize1;
            switch(c)
            {
                case 0:
                {
                    impl->vFormat.nWidth = ((s->videoTag.sizeData0)<<1) |
                        ((s->videoTag.sizeData1>>7)&1);
                    impl->vFormat.nHeight = ((s->videoTag.sizeData1&0x7f)<<1) |
                        ((s->videoTag.sizeData2>>7)&1);
                    break;
                }
                case 1:
                {
                    impl->vFormat.nWidth = ((s->videoTag.sizeData0)<<9) |
                        ((s->videoTag.sizeData1<<1)&0x1fe) | ((s->videoTag.sizeData2>>7)&1);
                    impl->vFormat.nHeight = ((s->videoTag.sizeData2&0x7f)<<9) |
                        ((s->videoTag.sizeData3<<1)&0x1fe) | ((s->videoTag.sizeData4>>7)&1);
                    break;
                }
                case 2:
                {
                    impl->vFormat.nWidth = 352;
                    impl->vFormat.nHeight = 288;
                    break;
                }
                case 3:
                {
                    impl->vFormat.nWidth = 176;
                    impl->vFormat.nHeight = 144;
                    break;
                }
                case 4:
                {
                    impl->vFormat.nWidth = 128;
                    impl->vFormat.nHeight = 96;
                    break;
                }
                case 5:
                {
                    impl->vFormat.nWidth = 320;
                    impl->vFormat.nHeight = 240;
                    break;
                }
                case 6:
                {
                    impl->vFormat.nWidth = 160;
                    impl->vFormat.nHeight = 120;
                    break;
                }
                default:
                {
                    CDX_LOGW("Unknown picture size!");
                    return -1;
                }
            }
        }
        else if (s->videoTag.codecID == 7)//h264
        {
            impl->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
            impl->vFormat.nWidth = 0;
            impl->vFormat.nHeight = 0;
        }
        else if (s->videoTag.codecID == 4)
        {
            cdx_uint8 *vp6info =  (cdx_uint8 *)(&s->videoTag);
            impl->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_VP6;

            if((vp6info[2]&0x01) || !(vp6info[3]&0x06))
            {
                impl->vFormat.nHeight = vp6info[6]<<4;
                impl->vFormat.nWidth = vp6info[7]<<4;
            }
            else
            {
                impl->vFormat.nWidth  = vp6info[5]<<4;
                impl->vFormat.nHeight = vp6info[4]<<4;
            }
        }
        else if(s->videoTag.codecID == 9 || s->videoTag.codecID == 0x0D) //xunlei h265 0x0d
        {
            impl->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_H265;
            impl->vFormat.nWidth = 0;
            impl->vFormat.nHeight = 0;
            impl->h265_vps_sps_pps_state = 3;
        }
        else
        {
            if(s->videoTag.codecID == 5)
            {
                CDX_LOGW("it is VP6A, do not support YUV420A pixel format");
            }
            CDX_LOGW("not support codecID[%x]", s->videoTag.codecID);
            impl->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
            return -1;
        }
    }

    if(s->seekAble == 1)
    {
        result = CdxStreamSeek(s->fp, (cdx_int64)t->dataOffset, STREAM_SEEK_SET);
        if(result < 0)
        {
            CDX_LOGE("Seek media file failed!\n");
            return -1;
        }

        //look for the first video tag, the timestamp should always be zero
        while(1)
        {
            cdx_int64 curPos = CdxStreamTell(s->fp);
            cdx_int64 curPosR = 0;
            len = CdxStreamRead(s->fp, &s->tag, sizeof(FlvTagsT)-1);
            CDX_LOGI("len: (%d), errno: (%s)",len, strerror(errno));
            curPosR = CdxStreamTell(s->fp);
            if(curPosR >= s->fileSize)
                break;
            if(len != sizeof(FlvTagsT)-1)//end of file
            {
                CDX_LOGE("Read failed.");
                return -1;
            }
            s->tag.dataSize = EndianConvert(s->tag.dataSize, 3);
            result = CdxStreamRead(s->fp, s->dataChunk.buffer, (cdx_uint32)s->tag.dataSize);
            if(result != s->tag.dataSize)
            {
                CDX_LOGE("Seek media file failed!");
                return -1;
            }

            if(s->tag.tagType == 9)
            {
                if (s->videoTag.frameType == 1) //keyframe
                {
                    impl->firstVideoTagPos = curPos;
                }
                else
                {
                    CDX_LOGW("The first video frame is not keyframe!");
                }

                if (s->videoTag.codecID == 7)
                {
                    {//--------------------------------------------------------------
                        if(!impl->tempBuf)
                        {
                            impl->tempBufLen = s->tag.dataSize -1-4;
                            //* if AVC, skip first 4 bytes, AVCPacketType(1B), CompositionTime(3B)
                            if(impl->tempBufLen > 0)
                            {
                                impl->tempBuf = malloc(impl->tempBufLen);
                                if(!impl->tempBuf)
                                {
                                    CDX_LOGE("malloc failed.");
                                    return -1;
                                }
                                memcpy(impl->tempBuf, (char *)s->dataChunk.buffer+1+4,
                                    impl->tempBufLen);
                            }
                        }
                    }
                    if (s->dataChunk.buffer[1] == 1)//AVC NALU
                    {
                        break;
                    }

                }
                else
                {
                    if(s->videoTag.codecID == 4)//On2 VP6
                    {
                        impl->vFormat.nWidth  -= (((s->dataChunk.buffer[1]) >> 4) & 0xf);
                        impl->vFormat.nHeight -= (((s->dataChunk.buffer[1]) >> 0) & 0xf);
                    }

                    {//--------------------------------------------------------------
                        if(!impl->tempBuf)
                        {
                            impl->tempBufLen = s->tag.dataSize -1;
                            impl->tempBuf = malloc(impl->tempBufLen);
                            if(!impl->tempBuf)
                            {
                                CDX_LOGE("malloc failed.");
                                return -1;
                            }
                            memcpy(impl->tempBuf, (char *)s->dataChunk.buffer+1, impl->tempBufLen);
                        }
                    }
                    break;
                }
            }
        }
    }
    if(impl->vFormat.eCodecFormat != VIDEO_CODEC_FORMAT_H265)
    {
        int ret = ProbeVideoSpecificData(&impl->tempVformat, impl->tempBuf, impl->tempBufLen,
            impl->vFormat.eCodecFormat, CDX_PARSER_FLV);
        if(ret == PROBE_SPECIFIC_DATA_ERROR)
        {
            CDX_LOGE("probeVideoSpecificData error");
        }
        else if(ret == PROBE_SPECIFIC_DATA_SUCCESS)
        {
            impl->vFormat.pCodecSpecificData = impl->tempVformat.pCodecSpecificData;
            impl->vFormat.nCodecSpecificDataLen = impl->tempVformat.nCodecSpecificDataLen;
            if(!impl->vFormat.nHeight)
            {
                impl->vFormat.nHeight = impl->tempVformat.nHeight;
            }
            if(!impl->vFormat.nWidth)
            {
                impl->vFormat.nWidth  = impl->tempVformat.nWidth;
            }
            if(!impl->vFormat.nFrameRate)
            {
                impl->vFormat.nFrameRate = impl->tempVformat.nFrameRate;
            }
            if(!impl->vFormat.nFrameDuration)
            {
                impl->vFormat.nFrameDuration = impl->tempVformat.nFrameDuration;
            }
            if(!impl->vFormat.nAspectRatio)
            {
                impl->vFormat.nAspectRatio = impl->tempVformat.nAspectRatio;
            }
        }
        else if(ret == PROBE_SPECIFIC_DATA_NONE)
        {
            CDX_LOGW("PROBE_SPECIFIC_DATA_NONE");
            //combine = 0; //* drop current es, use next es
        }
        else if(ret == PROBE_SPECIFIC_DATA_UNCOMPELETE)
        {
            CDX_LOGW("PROBE_SPECIFIC_DATA_UNCOMPELETE");
            //combine = 1; //* add next es
        }
        else
        {
            CDX_LOGE("probeVideoSpecificData (%d), it is unknown.", ret);
        }

    }

    if(impl->vFormat.nWidth == 0)
    {
        impl->vFormat.nWidth = s->nWidth;
    }

    if(impl->vFormat.nHeight == 0)
    {
        impl->vFormat.nHeight = s->nHeight;
    }

#if 0
    //look for the second video tag
    do
    {
        len = CdxStreamRead(s->fp, &s->tag, sizeof(FlvTagsT)-1);
        if(len != sizeof(FlvTagsT)-1)//end of file
        {
            CDX_LOGE("Read failed.");
            return -1;//FILE_PARSER_READ_FILE_FAIL;
        }

        s->tag.dataSize = EndianConvert(s->tag.dataSize, 3);
        s->tag.timeStamp = EndianConvert(s->tag.timeStamp, 3);
        result = CdxStreamSeek(s->fp, (cdx_uint32)s->tag.dataSize, STREAM_SEEK_CUR);
        if(result)
        {
            CDX_LOGW("Seek media file failed!");
            return -1;//FILE_PARSER_READ_FILE_FAIL;
        }
    } while(s->tag.tagType != 9 || s->tag.timeStamp == 0);
#endif

    //set frame rate
    //impl->vFormat.nFrameRate = 1000000/s->tag.timeStamp;
    //impl->vFormat.nFrameRate = 0;
    //impl->vFormat.nMicSecPerFrame = 1000*s->tag.timeStamp;

    //go back to the first tag
    if(s->seekAble == 1)
    {
        result = CdxStreamSeek(s->fp, (cdx_int64)t->dataOffset, STREAM_SEEK_SET);
        if(result)
        {
            CDX_LOGE("Seek media file failed!");
            return -1;//FILE_PARSER_READ_FILE_FAIL;
        }
    }

    if(check_video)
    {
        if(impl->vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_UNKNOWN)
        {
            impl->hasVideo = 0;
        }
        else
        {
            impl->hasVideo = 1;
        }
    }
    else
    {
        impl->hasVideo = 0;
    }

    if(check_audio)
    {
        if(impl->aFormat.eCodecFormat == AUDIO_CODEC_FORMAT_UNKNOWN)
        {
            impl->hasAudio = 0;
        }
        else
        {
            impl->hasAudio = 1;
        }
    }
    else
    {
        impl->hasAudio = 0;
    }

    if(check_audio && check_video)
    {
        if(impl->vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_UNKNOWN ||
            impl->aFormat.eCodecFormat == AUDIO_CODEC_FORMAT_UNKNOWN)
        {
            impl->hasVideo = 0;
            impl->hasAudio = 0;
        }
    }

    impl->hasSubTitle = 0;

    //initial video frame and key frame time stamp to 0
    s->lastKeyFrmPts = 0;
    s->lastKeyFrmPos = 0;
    s->lastFrmPts = 0;
    s->lastFrmPos = 0;

    return 0;
}

CdxFlvParserImplT *FlvInit(cdx_int32 *ret)
{
    CdxFlvParserImplT *p;
    struct VdecFlvIn *s;

    *ret = 0;
    p = (CdxFlvParserImplT *)malloc(sizeof(CdxFlvParserImplT));
    if(!p)
    {
        *ret = -1;
        return NULL;
    }
    memset(p, 0, sizeof(CdxFlvParserImplT));

    p->mErrno = PSR_INVALID;
    p->privData = NULL;
    s = (struct VdecFlvIn *)malloc(sizeof(struct VdecFlvIn));
    if(s)
    {
        memset(s, 0, sizeof(struct VdecFlvIn));
        p->privData = (void *)s;

        s->dataChunk.buffer = NULL;
        s->dataChunk.buffer = (cdx_uint8 *)malloc(MAX_CHUNK_BUF_SIZE);
        if(s->dataChunk.buffer)
        {
            memset(s->dataChunk.buffer, 0, MAX_CHUNK_BUF_SIZE);
        }
        else
        {
            *ret = -1;
        }

    }
    else
    {
        *ret = -1;
    }

    return p;
}
cdx_int16 FlvOpen(CdxFlvParserImplT *p)
{
    cdx_int16 ret;

    ret = FlvReaderOpenFile(p);
    if(ret < 0)
    {
        CDX_LOGE("FlvReaderOpenFile failed.");
    }

    return ret;
}

cdx_int16 FlvRead(CdxParserT *parser)
{
    cdx_int32         result = 0;
    struct VdecFlvIn  *s;
    cdx_int32         len;
    CdxFlvParserImplT *impl;

    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);
    s = (struct VdecFlvIn *)impl->privData;

    while(1)
    {
begin:
        if(impl->exitFlag)
        {
            CDX_LOGV("exit from FlvRead.");
            return -1;
        }
        len = CdxStreamRead(s->fp, &s->tag, sizeof(FlvTagsT));
        if(len != sizeof(FlvTagsT))
        {
            CDX_LOGW("Try to get flv tag failed!");

            //seek file pointer to the head of the last tag
            if(s->lastTagHeadPos != 0xffffffff)
            {
                //CdxStreamSeek(s->fp, (cdx_uint32)s->lastTagHeadPos, STREAM_SEEK_SET);
            }
            return -1;
        }

        s->tag.timeStamp = EndianConvert(s->tag.timeStamp, 3);
        s->tag.dataSize = EndianConvert(s->tag.dataSize, 3);
/*
       CDX_LOGV("line %d, s->tag.timeStamp %x, s->tag.dataSize %x, "
               "s->tag.streamID %x, s->tag.tagType %d, s->tag.codecType %d",
               __LINE__, s->tag.timeStamp,  s->tag.dataSize,
                    s->tag.streamID, s->tag.tagType, s->tag.codecType);
*/
       if(s->tag.dataSize == 0 && s->tag.streamID == 0 &&
                        (s->tag.tagType == 8 || s->tag.tagType == 9 || s->tag.tagType == 0x12))
        {
            result = CdxStreamSeek(s->fp, -1, STREAM_SEEK_CUR);
            if(result < 0)
            {
                CDX_LOGE("Seek failed.");
                return -1;
            }

            continue;
        }

        if(s->tag.tagType == 0x12 && s->tag.dataSize < MAX_CHUNK_BUF_SIZE)
        {
            result = CdxStreamSeek(s->fp, s->tag.dataSize - 0x1, STREAM_SEEK_CUR);
            //Skip MetaData Tag
            if(result < 0)
            {
                CDX_LOGE("Seek failed.");
                return -1;
            }
            continue;
        }

        if(s->tag.tagType == 8 && impl->hasAudio == 1)
        {
            //find an audio tag
            if(impl->aFormat.eCodecFormat == AUDIO_CODEC_FORMAT_MPEG_AAC_LC)
            {
                unsigned char c;
                s->tag.tagType = CDX_TagAudio;
                s->tag.dataSize -= 1;
                len = CdxStreamRead(s->fp, &c, 1);
                if(len != 1)//end of file
                {
                    CDX_LOGW("Get the audio format from the first tag failed!");
                    return -1;
                }
                if(c == 1)// raw data
                {
                     s->tag.dataSize -= 1;
                     break;
                }
                else
                {
                    //unknown tag data, skip it
                    CDX_LOGW("xxx c(%x), s->tag.dataSize(%u), skip it.", c, s->tag.dataSize);
                    result = CdxStreamSeek(s->fp,(cdx_int32)(s->tag.dataSize-1), STREAM_SEEK_CUR);
                    //read to a temp buf maybe better, but live stream?
                    if(result < 0)
                    {
                        CDX_LOGW("Skip data tag failed!");
                        return -1;
                    }
                }
            }
            else
            {
                s->tag.tagType = CDX_TagAudio;
                s->tag.dataSize -= 1;
                break;
            }
        }
        else if(s->tag.tagType == 9 && impl->hasVideo == 1)
        {
            if(s->tag.dataSize < 9)
            {
                CDX_LOGW("xxx s->tag.dataSize(%u), skip it.", s->tag.dataSize);
                result = CdxStreamSeek(s->fp, (cdx_int32)(s->tag.dataSize-1),
                    STREAM_SEEK_CUR); //read to a temp buf maybe better, but live stream?
                if(result < 0)
                {
                    CDX_LOGW("Skip data tag failed!");
                    return -1;
                }
            }
            else
            {
                //find a video tag
                s->tag.tagType = CDX_TagVideo;
                s->tag.dataSize -= 1;
                break;
            }
        }
        else
        {
            //unknown tag data, skip it
            if(s->tag.tagType == 0x12 && s->tag.streamID == 0 &&
                s->tag.dataSize < MAX_CHUNK_BUF_SIZE)
            {
                //* script tag.
                result = CdxStreamSeek(s->fp, (cdx_int32)(s->tag.dataSize-1), STREAM_SEEK_CUR);
            }
            else
            {
                //* invalid tag, try to find the next valid tag.
                logd("try to find the next valid tag.");
                result = FlvFindNextValidTag(s);
            }

            if(result < 0)
            {
                CDX_LOGE("Skip data tag failed!");
                return -1;
            }
        }
    }

    if(s->tag.dataSize >= MAX_CHUNK_BUF_SIZE)
    {
        CDX_LOGW("Size of tag data is too large! <%d>", s->tag.dataSize);
        result = FlvFindNextValidTag(s);
        if(result == 0)
            goto begin;
        else
        {
            CDX_LOGE("find next valid tag failed.");
            return -1;
        }

    }
    //impl->nFRPicCnt = 0;

    return 0;
}

/*
***********************************************************************************************
*                               GET NEXT CHUNK INFORMATION
*
*Description: Get the next chunk information.
*
*Arguments  : pFlvPsr       the flv file parser lib handle;
*             pChunkType    the pointer to the type of the chunk, for return.
*
*Return     : get next chunk information result
*               =   0   get next chunk information successed;
*               <   0   get next chunk information failed;
*Note       : This function read the header of the next chunk, and analyze the type of the
*             the bitstream, and record the chunk information, report chunk type.
***********************************************************************************************
*/
static cdx_int32 __CdxFlvParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32                result;
    cdx_int32               ret = 0;
    struct VdecFlvIn        *tmpFlvIn;
    cdx_uint32                len, dataSize = 0;
    CdxFlvParserImplT       *impl;

    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);
    tmpFlvIn = (struct VdecFlvIn *)impl->privData;

    if(impl->mErrno == PSR_EOS)
    {
        CDX_LOGI("flv eos.");
        return -1;
    }

    memset(pkt, 0x00, sizeof(CdxPacketT));

    if(impl->mStatus == CDX_FLV_PREFETCHED)//last time not read after prefetch.
    {
        memcpy(pkt, &impl->pkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&impl->lock);
    if(impl->exitFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->mStatus = CDX_FLV_PREFETCHING;
    pthread_mutex_unlock(&impl->lock);

    pkt->type = CDX_MEDIA_UNKNOWN;
    pkt->duration = 0;
    impl->startPos = CdxStreamTell(tmpFlvIn->fp);

    if(impl->h265_vps_sps_pps_state == 3 &&
       impl->vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_H265 &&
       impl->startPos != (cdx_uint32)tmpFlvIn->header.dataOffset)
    {

        impl->h265_start_pos_stored = impl->startPos;
        impl->h265_data_size_stored = (cdx_uint32)tmpFlvIn->header.dataOffset;

    }
    else if(impl->startPos == (cdx_uint32)tmpFlvIn->header.dataOffset)
    {
        // LOGD("no need to process vps_sps_pps");
        impl->h265_vps_sps_pps_state = -1;
    }

    if(impl->h265_vps_sps_pps_state > 0)
    {
        goto HEVC_vps_sps_pps_state;
    }
    else if(impl->h265_vps_sps_pps_state == 0)
    {
        int res;

        impl->startPos = impl->h265_start_pos_stored;
        tmpFlvIn->header.dataOffset = impl->h265_data_size_stored;

        res = CdxStreamSeek(tmpFlvIn->fp, (cdx_int64)impl->startPos, STREAM_SEEK_SET);

        if(res < 0)
        {
            CDX_LOGW("fail to seek back");
        }
        impl->h265_vps_sps_pps_state = -1;
    }

    if(impl->vFormat.eCodecFormat != VIDEO_CODEC_FORMAT_H264 ||
        impl->bFirstVidFrm != 1 || impl->startPos == (cdx_uint32)tmpFlvIn->header.dataOffset)
    {
        while(1)
        {
            if(impl->exitFlag)
            {
                CDX_LOGV("exit from prefetch.");
                ret = -1;
                goto __exit;
            }
            result = FlvRead(parser);
            if(result)
            {
                CDX_LOGW("Try to read next tag failed!");
                impl->mErrno = PSR_UNKNOWN_ERR;
                if(CdxStreamEos(tmpFlvIn->fp))
                    impl->mErrno = PSR_EOS;

                ret = -1;
                goto __exit;
            }

            impl->curChunkInfo.nChkType = tmpFlvIn->tag.tagType;
            impl->curChunkInfo.nTotSize = tmpFlvIn->tag.dataSize;
            impl->curChunkInfo.nReadSize = 0;
            impl->curChunkInfo.nTimeStamp = tmpFlvIn->tag.timeStamp;
            if(impl->bDiscardAud && tmpFlvIn->tag.tagType == CDX_TagAudio)
            {
                if(impl->curChunkInfo.nTotSize)
                {
                    if(CdxStreamSeek(tmpFlvIn->fp, impl->curChunkInfo.nTotSize,
                        STREAM_SEEK_CUR) < 0)
                    {
                        CDX_LOGW("seek file failed when skip bitstream!\n");
                        impl->mErrno = PSR_UNKNOWN_ERR;
                        ret = -1;
                        goto __exit;
                    }
                }
                //clear current chunk information
                impl->curChunkInfo.nChkType = CDX_TagUnknown;
            }
            else
            {
                break;
            }
        }

        if(tmpFlvIn->tag.tagType == CDX_TagVideo)
        {
            tmpFlvIn->lastFrmPts = tmpFlvIn->tag.timeStamp;
            tmpFlvIn->lastFrmPos = CdxStreamTell(tmpFlvIn->fp) - sizeof(FlvTagsT); //Tag header pos
            if((tmpFlvIn->tag.codecType & 0xf0) == 0x10)
            {
                tmpFlvIn->lastKeyFrmPts = tmpFlvIn->tag.timeStamp;
                tmpFlvIn->lastKeyFrmPos = tmpFlvIn->lastFrmPos;
                pkt->flags |= KEY_FRAME;
                CDX_LOGV("=============keyframe pts:%u, tmpFlvIn->lastKeyFrmPos=%u =========",
                    tmpFlvIn->tag.timeStamp, tmpFlvIn->lastKeyFrmPos);
            }

            pkt->type = CDX_MEDIA_VIDEO;
        }
        else if(tmpFlvIn->tag.tagType == CDX_TagAudio)
        {
            pkt->type = CDX_MEDIA_AUDIO;
        }

       //CDX_LOGV("Get Chunk Information, type:%x, size:%d", impl->curChunkInfo.nChkType,
       //impl->curChunkInfo.nTotSize);
    }
    else
    {
        if(tmpFlvIn->seekAble)
        {
            result = CdxStreamSeek(tmpFlvIn->fp, (cdx_uint32)tmpFlvIn->header.dataOffset,
                STREAM_SEEK_SET);
            if(result < 0)
            {
                CDX_LOGW("Try to read next tag failed!");
                impl->mErrno = PSR_UNKNOWN_ERR;
                ret = -1;
                goto __exit;
            }
        }

        while(1)
        {
            len = CdxStreamRead(tmpFlvIn->fp, &tmpFlvIn->tag, sizeof(FlvTagsT));
            if(len != sizeof(FlvTagsT))//end of file
            {
                CDX_LOGW("Try to read next tag failed!");
                impl->mErrno = PSR_UNKNOWN_ERR;
                if(CdxStreamEos(tmpFlvIn->fp))
                    impl->mErrno = PSR_EOS;

                ret = -1;
                goto __exit;
            }

            dataSize = EndianConvert(tmpFlvIn->tag.dataSize, 3) - 1;

            if(tmpFlvIn->tag.tagType == CDX_TagVideo)
            {
                if((tmpFlvIn->tag.codecType & 0xf0) == 0x10)
                {
                    pkt->flags |= KEY_FRAME;
                }
                break;
            }
            else
            {
            #if 0
                result = CdxStreamSeek(tmpFlvIn->fp, (cdx_uint32)dataSize, STREAM_SEEK_CUR);
                if(result < 0)
                {
                    CDX_LOGW("Try to read next tag failed!");
                    impl->mStatus = CDX_FLV_IDLE;
                    return -1;
                }
            #endif
                char *tempBuf = malloc(dataSize);
                if(tempBuf == NULL)
                {
                    CDX_LOGE("malloc failed.");
                    impl->mErrno = PSR_UNKNOWN_ERR;
                    ret = -1;
                    goto __exit;
                }
                result = CdxStreamRead(tmpFlvIn->fp, tempBuf, dataSize);
                //result = CdxStreamSeek(s->fp, (cdx_int32)(s->tag.dataSize-1), STREAM_SEEK_CUR);
                free(tempBuf);
                if(result != (cdx_int32)dataSize)
                {
                    impl->mErrno = PSR_UNKNOWN_ERR;
                    if(CdxStreamEos(tmpFlvIn->fp))
                        impl->mErrno = PSR_EOS;
                    ret = -1;
                    goto __exit;
                }
            }
        }

        tmpFlvIn->tag.tagType = CDX_TagVideo;
        impl->curChunkInfo.nChkType = CDX_TagVideo;
        impl->curChunkInfo.nTotSize = dataSize;
        impl->curChunkInfo.nReadSize = 0;
        impl->curChunkInfo.nTimeStamp = EndianConvert(tmpFlvIn->tag.timeStamp, 3);
        pkt->type = CDX_MEDIA_VIDEO;

    }

        pkt->length = impl->curChunkInfo.nTotSize + 2*1024;//keep enough buffer
        pkt->pts = (cdx_int64)(impl->curChunkInfo.nTimeStamp)*1000;
        pkt->flags |= (FIRST_PART|LAST_PART);
        memcpy(&impl->pkt, pkt, sizeof(CdxPacketT));

__exit:
    pthread_mutex_lock(&impl->lock);
    if(ret < 0)
    {
        impl->mStatus = CDX_FLV_IDLE;
    }
    else
    {
        impl->mStatus = CDX_FLV_PREFETCHED;
    }
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return ret;

HEVC_vps_sps_pps_state: //for Breakpoint play
    CDX_LOGV("coming to procedd vps_sps_pps_state");
    CDX_LOGV("impl->h265_vps_sps_pps_state = %d", impl->h265_vps_sps_pps_state);

    switch(impl->h265_vps_sps_pps_state)
    {
        case 3://get first video tag
        {
            result = CdxStreamSeek(tmpFlvIn->fp, (cdx_int64)tmpFlvIn->header.dataOffset,
                STREAM_SEEK_SET);
            if(result)
            {
                CDX_LOGW("Seek failed!");
                impl->mErrno = PSR_UNKNOWN_ERR;
                ret = -1;
                goto __exit1;
            }
        }
        case 2: //get second video tag

        case 1: //get third video tag
        {
            while(1)
               {
                   len = CdxStreamRead(tmpFlvIn->fp, &tmpFlvIn->tag, sizeof(FlvTagsT));
                   if(len != sizeof(FlvTagsT))//end of file
                   {
                       CDX_LOGW("Try to read next tag failed!");
                       impl->mErrno = PSR_UNKNOWN_ERR;
                    if(CdxStreamEos(tmpFlvIn->fp))
                        impl->mErrno = PSR_EOS;
                    ret = -1;
                    goto __exit1;
                   }

                   dataSize = EndianConvert(tmpFlvIn->tag.dataSize, 3) - 1;

                   if(tmpFlvIn->tag.tagType == 9)
                   {
                       break;
                   }
                   else
                   {
                       result = CdxStreamSeek(tmpFlvIn->fp, (cdx_int64)dataSize, STREAM_SEEK_CUR);
                       if(result)
                       {
                           CDX_LOGW("Try to read next tag failed!");
                           impl->mErrno = PSR_UNKNOWN_ERR;
                           ret = -1;
                           goto __exit;
                       }
                   }
               }
            break;
        }
        default:
        {
            CDX_LOGW("error happened.");
            break;
        }
    }

    impl->h265_vps_sps_pps_state--;

    tmpFlvIn->tag.tagType = CDX_TagVideo;
    impl->curChunkInfo.nChkType = CDX_TagVideo;
    impl->curChunkInfo.nTotSize = dataSize;
    impl->curChunkInfo.nReadSize = 0;
    impl->curChunkInfo.nTimeStamp = EndianConvert(tmpFlvIn->tag.timeStamp, 3);
    pkt->type = CDX_MEDIA_VIDEO;
    pkt->flags |= (FIRST_PART|LAST_PART);
       pkt->length = impl->curChunkInfo.nTotSize + 2*1024;
       pkt->pts = (cdx_int64)(impl->curChunkInfo.nTimeStamp)*1000;
       memcpy(&impl->pkt, pkt, sizeof(CdxPacketT));

__exit1:
    pthread_mutex_lock(&impl->lock);
    if(ret < 0)
    {
        impl->mStatus = CDX_FLV_IDLE;
    }
    else
    {
        impl->mStatus = CDX_FLV_PREFETCHED;
    }
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return ret;
}
//fill 00 00 00 01
//size: rest size of buf0
//size0: return buf0 used buf size
//flag: 1: 00 00 00 01; 0: 00 00 00 00
static void FillHeader(cdx_uint8 *buf0, cdx_uint8 *buf1, cdx_int32 size, cdx_int32 *size0,
    cdx_int32 flag)
{
    cdx_int32 i, j, k = 0;

    for(i = 0; i < size; i++)
    {
        k++;
        if(i == 3)
        {
            *buf0++ = flag;
            break;
        }
        *buf0++ = 0;
    }

    for(j = 0; j < 3-i; j++)
    {
        if(j == 2-i)
        {
            *buf1++ = flag;
            break;
        }
        *buf1++ = 0;
    }
    *size0 = k;
    return;
}

static cdx_int32 ParserReadXL(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32           ret;
    CdxStreamT         *stream;
    struct VdecFlvIn   *tmpFlvIn;
    CdxFlvParserImplT  *impl;
    //cdx_uint8          *tmpBuf;
    //cdx_int32           tmpBufSize;

    cdx_uint8          *pktBuf0 = pkt->buf;
    cdx_int32           pktSize0 = pkt->buflen;
    //cdx_uint8          *pktBuf1 = pkt->ringBuf;
    //cdx_int32           pktSize1 = pkt->ringBufLen;

    unsigned int size;
    int totSize;
    cdx_uint8 preNAL[2];
    cdx_uint8 temp[4] = {0};

    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);
    totSize = impl->curChunkInfo.nTotSize;
    tmpFlvIn = (struct VdecFlvIn *)impl->privData;
    stream = tmpFlvIn->fp;

    if(impl->startPos == impl->firstVideoTagPos)//when seek to first video frame, handle it as open.
    {
        tmpFlvIn->packetNum = 0;
    }

    if(tmpFlvIn->packetNum == 0)//packet 1
    {
        ret = CdxStreamSeek(stream, impl->curChunkInfo.nTotSize, STREAM_SEEK_CUR);
        if(ret < 0)
        {
            CDX_LOGE("seek failed.");
            return -1;
        }
        pkt->length = 0;
        tmpFlvIn->packetNum = 1;
        //CDX_LOGV("xxx packetNum1,t_size(%d)", impl->curChunkInfo.nTotSize);
    }
    else if(tmpFlvIn->packetNum == 1)//packet 2
    {
        ret = CdxStreamSeek(stream, 4, STREAM_SEEK_CUR);
        if(ret < 0)
        {
            CDX_LOGE("seek failed.");
            return -1;
        }
        memcpy(pktBuf0, temp, 4);
        totSize -= 4;
        while(totSize)
        {
            //CDX_LOGV("xxx packetNum2 0");

            ret = CdxStreamRead(stream, &size, 4);//length
            if(ret != 4)
            {
                CDX_LOGE("read failed.");
                return -1;
            }
            size = EndianConvert(size, 4);
            //CDX_LOGV("xxx size(%u), pkt_size0(%d)", size, pktSize0);
            ret = CdxStreamRead(stream, preNAL, 2);
            if(ret != 2)
            {
                CDX_LOGE("read failed.");
                return -1;
            }
            if(preNAL[0] == 0x4E && preNAL[1] == 0x01)
            {
                //CDX_LOGV("xxx 4E01");
                ret = CdxStreamSeek(stream, size-2, STREAM_SEEK_CUR);
                if(ret < 0)
                {
                    CDX_LOGE("seek failed.");
                    return -1;
                }
                pkt->length -= size+4;
            }
            else
            {
                *(pktBuf0+4) = 0;
                *(pktBuf0+5) = 0;
                *(pktBuf0+6) = 0;
                *(pktBuf0+7) = 1;
                memcpy(pktBuf0 + 8, preNAL, 2);

                ret = CdxStreamRead(stream, pktBuf0+10, size-2);
                if(ret != (cdx_int32)size-2)
                {
                    CDX_LOGE("read failed.");
                    return -1;
                }
                pktBuf0 += (size-2+10);
                /*CDX_LOGV("xxx  pkt_buf0[8-11]%x,%x,%x,%x,%x",pktBuf0[8],pktBuf0[9],
                pktBuf0[10],pktBuf0[11],pktBuf0[12]);*/

            }
            totSize -= size+4;
            //CDX_LOGV("xxx totSize(%d)", totSize);
        }
        tmpFlvIn->packetNum = 2;
        //CDX_LOGV("xxx packetNum2 1");
    }
    else //other packets
    {
        if(impl->curChunkInfo.nTotSize <= pktSize0)
        {
            ret = CdxStreamSeek(stream, 4, STREAM_SEEK_CUR);
            if(ret < 0)
            {
                CDX_LOGE("seek failed.");
                return -1;
            }
            memcpy(pktBuf0, temp, 4);
            totSize -= 4;
            pktBuf0 += 4;
            while(totSize)
            {
                ret = CdxStreamRead(stream, &size, 4);//length
                if(ret != 4)
                {
                    CDX_LOGE("read failed.");
                    return -1;
                }
                size = EndianConvert(size,4);
                *pktBuf0++ = 0;
                *pktBuf0++ = 0;
                *pktBuf0++ = 0;
                *pktBuf0++ = 1;
                ret = CdxStreamRead(stream, pktBuf0, size);
                if(ret != (cdx_int32)size)
                {
                    CDX_LOGE("read failed.");
                    return -1;
                }
                pktBuf0 += size;
                totSize -= size+4;
            }
        }
        else
        {
            cdx_int32  tempPktSize0 =  pkt->buflen;//cdx_pkt->pkt_info.epdk_read_pkt_info.pkt_size0;
            cdx_int32  tempTotSize = impl->curChunkInfo.nTotSize;
            cdx_uint8 *tempPktBuf0 = pkt->buf;//pkt_info.epdk_read_pkt_info.pkt_buf0;
            cdx_uint8 *tempPktBuf1 = pkt->ringBuf;//pkt_info.epdk_read_pkt_info.pkt_buf1;
            cdx_int32  usedSize = 0;

            ret = CdxStreamSeek(stream, 4, STREAM_SEEK_CUR);
            if(ret < 0)
            {
                CDX_LOGE("seek failed.");
                return -1;
            }

            FillHeader(tempPktBuf0, tempPktBuf1, tempPktSize0, &usedSize, 0);
            tempPktBuf0  += usedSize;
            tempPktSize0 -= usedSize;
            tempPktBuf1  += 4-usedSize;

            tempTotSize -= 4;
            if(tempPktSize0 == 0)
            {
                CDX_LOGV("xxx tempPktSize0==0");
            }

            while(tempTotSize)
            {
                ret = CdxStreamRead(stream, &size, 4);//length
                if(ret != 4)
                {
                    CDX_LOGE("read failed.");
                    return -1;
                }
                size = EndianConvert(size, 4);

                FillHeader(tempPktBuf0, tempPktBuf1, tempPktSize0, &usedSize, 1);
                tempPktBuf0  += usedSize;
                tempPktSize0 -= usedSize;
                tempPktBuf1  += 4-usedSize;
                tempTotSize  -= 4;

                if(size <= (unsigned int)tempPktSize0)
                {
                    ret = CdxStreamRead(stream, tempPktBuf0, size);
                    if(ret != (cdx_int32)size)
                    {
                        CDX_LOGE("read failed.");
                        return -1;
                    }
                    tempPktBuf0  += size;
                    tempPktSize0 -= size;

                }
                else if(size > (unsigned int)tempPktSize0 && tempPktSize0 > 0)
                {
                    ret = CdxStreamRead(stream, tempPktBuf0, tempPktSize0);
                    if(ret != tempPktSize0)
                    {
                        CDX_LOGE("read failed.");
                        return -1;
                    }
                    ret = CdxStreamRead(stream, tempPktBuf1, size-tempPktSize0);
                    if(ret != (cdx_int32)size-tempPktSize0)
                    {
                        CDX_LOGE("read failed.");
                        return -1;
                    }
                    tempPktBuf1 += size-tempPktSize0;
                    tempPktSize0 = 0;
                }
                else
                {
                    ret = CdxStreamRead(stream, tempPktBuf1, size);
                    if(ret != (cdx_int32)size)
                    {
                        CDX_LOGE("read failed.");
                        return -1;
                    }
                    tempPktBuf1 += size;
                }
                tempTotSize -= size;
            }
        }
    }

    return 0;
}
/*
*************************************************************************************************
*                       GET DATA BLOCK
*
*Description: Get data block from current file pointer.
*
*Arguments  : pFlvPsr       the flv file parser lib handle;
*             pBuf          buffer address for store media data;
*             nBufSize      the size of buffer that used for store chunk data;
*             pDataSize     pointer to the size of data block that get from the chunk, for return;
*             pFrmInf       the pointer to the frame information of current chunk,
*                           which need be sent to decoder;;
*
*Return     : get data block result;
*               > 0     get data block successed, all data has been got out;
*               = 0     got data block successed, there is some data need be got yet;
*               < 0     get data block failed;
*************************************************************************************************
*/
static cdx_int32 __CdxFlvParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32           result;
    CdxStreamT         *stream;
    struct VdecFlvIn   *tmpFlvIn;
    CdxFlvParserImplT  *impl;
    cdx_uint8          *tmpBuf;
    cdx_int32           tmpBufSize;

    cdx_uint8          *pktBuf0 = pkt->buf;
    cdx_int32           pktSize0 = pkt->buflen;
    cdx_uint8          *pktBuf1 = pkt->ringBuf;
    //cdx_int32           pktSize1 = pkt->ringBufLen;

    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);

    tmpFlvIn = (struct VdecFlvIn *)impl->privData;
    if((impl->curChunkInfo.nChkType == CDX_TagUnknown) || (impl->curChunkInfo.nTotSize <= 0))
    {
        CDX_LOGE("Current chunk is invalid.");
        return -1;
    }
    if(impl->mStatus != CDX_FLV_PREFETCHED)
    {
        CDX_LOGW("mStatus(%d) != CDX_PSR_PREFETCHED, invaild", impl->mStatus);
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    tmpBuf = pktBuf0;
    tmpBufSize = pktSize0;
    pkt->length = 0;
    stream = tmpFlvIn->fp;
    result = CDX_SUCCESS;

    if(impl->vFormat.eCodecFormat != VIDEO_CODEC_FORMAT_H264 ||
        impl->curChunkInfo.nChkType != CDX_TagVideo) //  audio, subtitle and non h264 video.
    {
        cdx_uint8 pktTemp[10];
        pkt->length = impl->curChunkInfo.nTotSize;

        if(impl->vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_H265 &&
            impl->curChunkInfo.nChkType == CDX_TagVideo)
        {

            if(tmpFlvIn->videoTag.codecID == 0x09) //aiqiyi
            {

               if(impl->curChunkInfo.nTotSize <= pktSize0)
               {
                    if(CdxStreamRead(stream, pktTemp, 4) != 4)
                    {
                         CDX_LOGW("xxx read data fail.");
                         return -1;
                    }
                    if(CdxStreamRead(stream, pktBuf0, impl->curChunkInfo.nTotSize-4) !=
                        impl->curChunkInfo.nTotSize-4)
                    {
                         CDX_LOGW("xxx read data fail.");
                         return -1;
                    }
                    memcpy(pktBuf0 + impl->curChunkInfo.nTotSize-4, pktTemp, 4);

               }
               else if(impl->curChunkInfo.nTotSize > pktSize0 && impl->curChunkInfo.nTotSize
                < pktSize0 + 4 )
               {
                    CDX_LOGD("rare case happend ************* ");
                    if(CdxStreamRead(stream, pktTemp, 4) != 4)
                    {
                         CDX_LOGW("xxx read data fail.");
                         return -1;
                    }
                    if(CdxStreamRead(stream, pktBuf0, impl->curChunkInfo.nTotSize - 4) !=
                        impl->curChunkInfo.nTotSize - 4)
                    {
                         CDX_LOGW("xxx read data fail.");
                         return -1;
                    }
                    memcpy(pktBuf0+ impl->curChunkInfo.nTotSize -4, pktTemp,
                        pktSize0+4- impl->curChunkInfo.nTotSize);
                    memcpy(pktBuf1,pktTemp + pktSize0+4- impl->curChunkInfo.nTotSize,
                        impl->curChunkInfo.nTotSize - pktSize0);

               }
               else
               {
                    if(CdxStreamRead(stream, pktTemp, 4) != 4)
                    {
                         CDX_LOGW("xxx read data fail.");
                         return -1;
                    }
                    if(CdxStreamRead(stream, pktBuf0, pktSize0) != pktSize0)
                    {
                         CDX_LOGW("xxx read data fail.");
                         return -1;
                    }
                    if(CdxStreamRead(stream, pktBuf1,impl->curChunkInfo.nTotSize - pktSize0- 4)
                        != impl->curChunkInfo.nTotSize - pktSize0- 4)
                    {
                         CDX_LOGW("xxx read data fail.");
                         return -1;
                    }
                    memcpy(pktBuf1+impl->curChunkInfo.nTotSize - pktSize0 - 4, pktTemp, 4);
                }
            }
            else if(tmpFlvIn->videoTag.codecID == 0x0D) //xunlei
            {
                result = ParserReadXL(parser, pkt);
                if(result != 0)
                {
                    CDX_LOGE("read data failed.");
                    return -1;
                }
            }
        }
        else if(impl->vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_VP6 &&
            impl->curChunkInfo.nChkType == CDX_TagVideo)
        {
            //* if VP6, skip first byte.
            result = CdxStreamSkip(stream, 1);
            if(result < 0)
            {
                CDX_LOGE("vp6 skip 1 byte failed.");
                return -1;
            }
            impl->curChunkInfo.nTotSize -= 1;
            if(impl->curChunkInfo.nTotSize <= pktSize0)
            {
                if(CdxStreamRead(stream, pktBuf0, impl->curChunkInfo.nTotSize) !=
                    impl->curChunkInfo.nTotSize)
                {
                     CDX_LOGE("xxx read data fail.");
                     return -1;
                }
            }
            else
            {
                if(CdxStreamRead(stream, pktBuf0, pktSize0) != pktSize0)
                {
                     CDX_LOGE("xxx read data fail.");
                     return -1;
                }
                if(CdxStreamRead(stream, pktBuf1, impl->curChunkInfo.nTotSize - pktSize0)
                     != impl->curChunkInfo.nTotSize - pktSize0)
                {
                     CDX_LOGE("xxx read data fail.");
                     return -1;
                }
            }
            pkt->length = impl->curChunkInfo.nTotSize;
        }
        else // audio, subtitle ...
        {
            if(impl->curChunkInfo.nTotSize <= pktSize0)
            {
                if(CdxStreamRead(stream, pktBuf0, impl->curChunkInfo.nTotSize) !=
                    impl->curChunkInfo.nTotSize)
                {
                     CDX_LOGE("xxx read data fail.");
                     return -1;
                }
            }
            else
            {
                if(CdxStreamRead(stream, pktBuf0, pktSize0) != pktSize0)
                {
                     CDX_LOGE("xxx read data fail.");
                     return -1;
                }
                if(CdxStreamRead(stream, pktBuf1, impl->curChunkInfo.nTotSize - pktSize0)
                     != impl->curChunkInfo.nTotSize - pktSize0)
                {
                     CDX_LOGE("xxx read data fail.");
                     return -1;
                }
            }
        }
    }
    else //H264 to check.
    {
        if(impl->bNoAvcSequenceHeader)//example.flv: has no AVC Sequence Header in first video tag
        {
            pkt->length = impl->curChunkInfo.nTotSize;
            if(impl->curChunkInfo.nTotSize <= pktSize0)
            {
                if(CdxStreamRead(stream, pktBuf0, impl->curChunkInfo.nTotSize) !=
                    impl->curChunkInfo.nTotSize)
                {
                     CDX_LOGE("xxx read data fail.");
                     return -1;
                }
            }
            else
            {
                if(CdxStreamRead(stream, pktBuf0, pktSize0) != pktSize0)
                {
                     CDX_LOGE("xxx read data fail.");
                     return -1;
                }
                if(CdxStreamRead(stream, pktBuf1, impl->curChunkInfo.nTotSize - pktSize0)
                     != impl->curChunkInfo.nTotSize - pktSize0)
                {
                     CDX_LOGE("xxx read data fail.");
                     return -1;
                }
            }

            if(impl->bFirstVidFrm == 1)
            {
                impl->bFirstVidFrm = 2;
                if(impl->startPos != (cdx_uint32)tmpFlvIn->header.dataOffset)
                {
                    if(tmpFlvIn->seekAble)
                    {
                        result = CdxStreamSeek(stream, impl->startPos, STREAM_SEEK_SET);
                        if(result)
                        {
                            CDX_LOGW("Try to read next tag failed!");
                            return -1;
                        }
                    }
                }
            }
            impl->mStatus = CDX_FLV_IDLE;
            return 0;
        }

        //* if AVC, skip first 4 bytes, AVCPacketType(1B), CompositionTime(3B)
        cdx_uint8 tempBuf[4];
        if(CdxStreamRead(stream, tempBuf, 4) < 4)
        {
            CDX_LOGE("read 4 byte failed.");
            return -1;
        }
        impl->curChunkInfo.nTotSize -= 4;
        if(impl->curChunkInfo.nTotSize <= pktSize0)
        {
            if(CdxStreamRead(stream, pktBuf0, impl->curChunkInfo.nTotSize) !=
                impl->curChunkInfo.nTotSize)
            {
                 CDX_LOGE("xxx read data fail.");
                 return -1;
            }
        }
        else
        {
            if(CdxStreamRead(stream, pktBuf0, pktSize0) != pktSize0)
            {
                 CDX_LOGE("xxx read data fail.");
                 return -1;
            }
            if(CdxStreamRead(stream, pktBuf1, impl->curChunkInfo.nTotSize - pktSize0)
                 != impl->curChunkInfo.nTotSize - pktSize0)
            {
                 CDX_LOGE("xxx read data fail.");
                 return -1;
            }
        }
        pkt->length = impl->curChunkInfo.nTotSize;

        if(impl->bFirstVidFrm == 1)
        {
            impl->bFirstVidFrm = 2;
            if(impl->startPos != (cdx_uint32)tmpFlvIn->header.dataOffset)
            {
                if(tmpFlvIn->seekAble)
                {
                    result = CdxStreamSeek(stream, impl->startPos, STREAM_SEEK_SET);
                    if(result)
                    {
                        CDX_LOGW("Try to read next tag failed!");
                        return -1;
                    }
                }
            }
        }
    }

#if SAVE_VIDEO_STREAM
    if(pkt->type == CDX_MEDIA_VIDEO)
    {
        if(impl->fpVideoStream)
        {
            fwrite(pktBuf0, 1, pkt->length, impl->fpVideoStream);// not exact
            sync();
        }
        else
        {
            CDX_LOGE("demux->fpVideoStream == NULL");
        }
    }
#endif
#if SAVE_AUDIO_STREAM
    if(pkt->type == CDX_MEDIA_AUDIO)
    {
        if(impl->fpAudioStream[pkt->streamIndex])
        {
            fwrite(pktBuf0, 1, pkt->length, impl->fpAudioStream[pkt->streamIndex]);
            sync();
        }
        else
        {
            CDX_LOGE("demux->fpAudioStream == NULL");
        }
    }
#endif

    impl->mStatus = CDX_FLV_IDLE;
    return result;
}
/*
**********************************************************************************************
*                                   GET MEDIA INFORMATION
*
*Description: get media information.
*
*Arguments  : pFlvPsr       flv reader handle;
*             pMediaInfo    media information pointer;
*
*Return     : result;
*               == FILE_PARSER_RETURN_OK,     get media information successed;
*               == CDX_ERROR,   get media information failed;
**********************************************************************************************
*/
static cdx_int32 __CdxFlvParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    CdxFlvParserImplT *impl;
    struct VdecFlvIn *tmpFlvIn;

    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);

    tmpFlvIn = (struct VdecFlvIn *)impl->privData;

    //set program information
    pMediaInfo->fileSize = impl->fileSize;
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    if(tmpFlvIn->seekAble == 0)
    {
        pMediaInfo->bSeekable = 0; // flv is seekable even it did not have keyframes
    }
    else
    {
        pMediaInfo->bSeekable = 1;
    }

    //set the bitstream count of audio,video,and subtitle
    pMediaInfo->program[0].audioNum = impl->hasAudio;
    pMediaInfo->program[0].audioIndex = impl->audioStreamIndex;
    pMediaInfo->program[0].videoNum = impl->hasVideo;
    pMediaInfo->program[0].audioIndex = impl->videoStreamIndex;
    pMediaInfo->program[0].subtitleNum = impl->hasSubTitle;
    pMediaInfo->program[0].subtitleIndex = impl->subTitleStreamIndex;

    //set audio bitstream information
    memcpy(&pMediaInfo->program[0].audio[0], &impl->aFormat, sizeof(AudioStreamInfo));

    //set video bitstream information
    memcpy(&pMediaInfo->program[0].video[0], &impl->vFormat, sizeof(VideoStreamInfo));
    //CDX_LOGD("xxx eCodecFormat = %d", impl->vFormat.eCodecFormat);

    //set total time
    pMediaInfo->program[0].duration = tmpFlvIn->totalTime;

    if(pMediaInfo->fileSize > 0 && tmpFlvIn->totalTime > 0)
    {
        pMediaInfo->bitrate = pMediaInfo->fileSize / tmpFlvIn->totalTime * 8;
    }
    else
    {
        pMediaInfo->bitrate = 0;
    }

    impl->mStatus = CDX_FLV_IDLE;
    return CDX_SUCCESS;
}
/**************************************************

    FlvReaderCloseFile()
    Functionality : Close the rm file
    Return value  :
        0   allright

 **************************************************/
cdx_int16 FlvReaderCloseFile(struct VdecFlvIn *s)
{
    // simply close the file
    if(!s)
        return -1;

    if (s->fp)
    {
        CdxStreamClose(s->fp);
        s->fp = NULL;
    }
    if(s->keyFrameIndex.keyFramePosIdx)
    {
        free(s->keyFrameIndex.keyFramePosIdx);
        s->keyFrameIndex.keyFramePosIdx = NULL;
    }

    if(s->keyFrameIndex.keyFramePtsIdx)
    {
        free(s->keyFrameIndex.keyFramePtsIdx);
        s->keyFrameIndex.keyFramePtsIdx = NULL;
    }

    return 0;
}
static cdx_int32 __CdxFlvParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    CdxFlvParserImplT *impl;
    cdx_int32         result;
    cdx_int32         ret = 0;
    struct VdecFlvIn  *s;
    cdx_int32         len;
    cdx_int64         seekTime;

    seekTime = timeUs/1000;  //timeUs is in unit of us

    CDX_FORCE_CHECK(parser);
    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);
    s = (struct VdecFlvIn *)impl->privData;

    if(seekTime < 0)
    {
        CDX_LOGE("Bad seek_time. seekTime = %lld, totalTime = %d", seekTime, s->totalTime);
        impl->mStatus = CDX_FLV_IDLE;
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    else if(seekTime >= s->totalTime)
    {
        CDX_LOGI("flv eos.");
        impl->mErrno = PSR_EOS;
        return 0;
    }

    pthread_mutex_lock(&impl->lock);
    if(impl->exitFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->mStatus = CDX_FLV_SEEKING;
    pthread_mutex_unlock(&impl->lock);

    if(s->keyFrameIndex.hasKeyFrameIdx == 1)
    {
        //*Binsearch the appropriate pts
        int low = 0;
        int high = s->keyFrameIndex.length - 1;
        int mid;
        CDX_LOGV("s->keyFrameIndex.hasKeyFrameIdx == TRUE");

        while(low <= high)
        {
            mid = (low + high)/2;
            if(s->keyFrameIndex.keyFramePtsIdx[mid] == seekTime)
            {
                break;
            }

            if(s->keyFrameIndex.keyFramePtsIdx[mid] > seekTime)
            {
                high = mid - 1;
            }
            else
            {
                low = mid + 1;
            }
        }
        if(low > high)
        {
            if(s->lastKeyFrmPts < seekTime)
            {
                mid = low;
            }
            else
            {
                mid = high;
            }
        }

        if(mid >= s->keyFrameIndex.length)
        {
            mid--;
        }

        if(mid < 0)
        {
            mid = 0;
        }

        if(0/*s->keyFrameIndex.keyFramePtsIdx[mid] == 0*/)
            //FLV020_IFTlhxwwkKE_320x320_0.007fps_Sore....flv. just 1 video frame, a|v|aaaaaaaa..
        {
            CDX_LOGV("xxxxxxxx pts = 0.");
            //seek to begin
            if(CdxStreamSeek(s->fp, s->header.dataOffset, STREAM_SEEK_SET) < 0)
            {
                CDX_LOGE("Seek back failed.");
                impl->mErrno = PSR_UNKNOWN_ERR;
                ret = -1;
                goto __exit;
            }
        }
        else
        {
            if(CdxStreamSeek(s->fp, s->keyFrameIndex.keyFramePosIdx[mid] - 4, STREAM_SEEK_SET) < 0)
            {
                CDX_LOGE("Seek back failed.");
                impl->mErrno = PSR_UNKNOWN_ERR;
                ret = -1;
                goto __exit;
            }
        }
    }
    else // has no keyframe index.
    {
        int                     mode = 0;
        int                     d = 0;

        //default search from beginning,
        if(seekTime == 0)
        {
            mode = 0;           //search from beginning
        }
        else if((seekTime == s->totalTime) && (s->lastTagHeadPos != 0xffffffff))
        {
            mode = 2;            //search from end
        }
        else
        {
            if(s->lastKeyFrmPts > seekTime) //s->lastFrmPts > seekTime
            {
                d = s->lastKeyFrmPts - seekTime;
            }
            else
            {
                d = seekTime - s->lastKeyFrmPts;
            }

            if(d < seekTime)
            {
                mode = 1;       //search from current
            }
            else
            {
                mode = 0;       //search from beginning
                d = seekTime;
            }

            if(((s->totalTime - seekTime) < d) && (s->lastTagHeadPos != 0xffffffff))
                mode = 2;       //search from end
        }

SEEK_MODE_SELECT:
        switch(mode)
        {
            case 1:
            {
                CDX_LOGV("mode 1");
                result = CdxStreamSeek(s->fp, s->lastFrmPos, STREAM_SEEK_SET);
                if(result < 0)
                {
                    CDX_LOGE("seek to last tag header failed.");
                    impl->mErrno = PSR_UNKNOWN_ERR;
                    ret = -1;
                    goto __exit;
                }

                if(s->lastKeyFrmPts > seekTime)
                    goto fb_search; //seek backward
                else
                    goto ff_search; //seek forward
            }

            case 2:
            {
                CDX_LOGV("mode 2");
                //search from the tail of the file
                //seek file pointer to the last tag header
                result = CdxStreamSeek(s->fp, (cdx_uint32)s->lastTagHeadPos, STREAM_SEEK_SET);
                if(result)
                {
                    CDX_LOGW("Seek file to last tag header failed, result = %d!\n", result);
                    impl->mErrno = PSR_UNKNOWN_ERR;
                    ret = -1;
                    goto __exit;
                }

                fb_search:
                while(1)
                {
                    if(impl->exitFlag == 1)
                    {
                        CDX_LOGV("force exit.");
                        ret = -1;
                        goto __exit;
                    }
                    len = CdxStreamRead(s->fp, &s->tag, sizeof(FlvTagsT));
                    if(len != sizeof(FlvTagsT))//end of file
                    {
                        CDX_LOGW("Try to get flv tag header failed, len = %d!\n", len);
                        impl->mErrno = PSR_UNKNOWN_ERR;
                        if(CdxStreamEos(s->fp))
                            impl->mErrno = PSR_EOS;

                        ret = -1;
                        goto __exit;
                    }

                    s->tag.timeStamp = EndianConvert(s->tag.timeStamp, 3);
                    s->tag.dataSize = EndianConvert(s->tag.dataSize, 3);
                    s->tag.preTagsize = EndianConvert(s->tag.preTagsize, 4);
                    if((s->tag.tagType == 9) && ((s->tag.codecType & 0xf0) == 0x10))
                    {
                        //check current video key frame is we wanted
                        if(s->tag.timeStamp <= seekTime)
                        {
                            /* check which one is close to seekTime,  this keyframe's
                            pts or last keyframe.*/
                            if(FLV_ABS(seekTime - s->tag.timeStamp) >=
                                FLV_ABS(s->lastKeyFrmPts - seekTime))
                                //* seekTime is closer to lastKeyframe pts
                            {
                                result = CdxStreamSeek(s->fp, s->lastKeyFrmPos, STREAM_SEEK_SET);
                                if(result < 0)
                                {
                                    CDX_LOGE("seek to last key frame tag header failed.");
                                    impl->mErrno = PSR_UNKNOWN_ERR;
                                    ret = -1;
                                    goto __exit;
                                }
                            }
                            else
                            {
                                CDX_LOGD("s->tag.timeStamp=%u fb", s->tag.timeStamp);
                                //seek back to the tag header, for prefetch.
                                result = CdxStreamSeek(s->fp, (cdx_int32)(0-sizeof(FlvTagsT)),
                                    STREAM_SEEK_CUR);
                                if(result < 0)
                                {
                                    CDX_LOGE("Seek back to the tag header failed.");
                                    impl->mErrno = PSR_UNKNOWN_ERR;
                                    ret = -1;
                                    goto __exit;
                                }
                            }
                            break;
                        }
                        s->lastKeyFrmPts = s->tag.timeStamp;
                        s->lastKeyFrmPos = CdxStreamTell(s->fp) - sizeof(FlvTagsT);
                        CDX_LOGV("xxx fb s->tag.timeStamp(%u) > seekTime(%lld),"
                            " continue find keyframe...", s->tag.timeStamp, seekTime);
                    }

                    //seek to previous tag header
                    result = CdxStreamSeek(s->fp,
                        (cdx_int32)(0-(sizeof(FlvTagsT)+s->tag.preTagsize+4)), STREAM_SEEK_CUR);
                    if(result < 0)
                    {
                        CDX_LOGW("Seek back to the previous tag header failed. preTagsize=%u",
                            s->tag.preTagsize);
                        //impl->mErrno = PSR_UNKNOWN_ERR;
                        //ret = -1;
                        //goto __exit;
                        mode = 1; //* try to find keyframe from lastKeyFrmPos.
                        goto SEEK_MODE_SELECT;
                    }
                }
                break;
            }

            default:
            {
                CDX_LOGV("mode 0");
                //search from the head of the file
                result = CdxStreamSeek(s->fp, (cdx_uint32)s->header.dataOffset, STREAM_SEEK_SET);
                if(result < 0)
                {
                    CDX_LOGW("Seek file pointer failed!");
                    impl->mErrno = PSR_UNKNOWN_ERR;
                    ret = -1;
                    goto __exit;
                }

                ff_search:
                while(1)
                {
                    if(impl->exitFlag == 1)
                    {
                        CDX_LOGV("force exit.");
                        ret = -1;
                        goto __exit;
                    }
                    len = CdxStreamRead(s->fp, &s->tag, sizeof(FlvTagsT));
                    if(len != sizeof(FlvTagsT))
                    //may not found keyframe even read to file end, seek to last keyframe.
                    {
                        result = CdxStreamSeek(s->fp, s->lastKeyFrmPos, STREAM_SEEK_SET);
                        if(result < 0)
                        {
                            CDX_LOGE("seek to last key frame tag header failed.");
                            impl->mErrno = PSR_UNKNOWN_ERR;
                            ret = -1;
                            goto __exit;
                        }
                        break;
                    }

                    s->tag.timeStamp = EndianConvert(s->tag.timeStamp,3);
                    s->tag.dataSize = EndianConvert(s->tag.dataSize,3);
                    s->tag.preTagsize = EndianConvert(s->tag.preTagsize, 4);
                    if((s->tag.tagType == 9) && ((s->tag.codecType&0xf0) == 0x10))
                    {
                        //check current video key frame is we wanted
                        if(s->tag.timeStamp >= seekTime)
                        {
                            /* check which one is close to seekTime, this keyframe's
                            pts or last keyframe.*/
                            if(FLV_ABS(s->tag.timeStamp - seekTime) >=
                                FLV_ABS(seekTime - s->lastKeyFrmPts))
                                //* seekTime is closer to lastKeyframe pts
                            {
                                result = CdxStreamSeek(s->fp, s->lastKeyFrmPos, STREAM_SEEK_SET);
                                if(result < 0)
                                {
                                    CDX_LOGE("seek to last key frame tag header failed.");
                                    impl->mErrno = PSR_UNKNOWN_ERR;
                                    ret = -1;
                                    goto __exit;
                                }
                            }
                            else
                            {
                                CDX_LOGD("s->tag.timeStamp=%u ff", s->tag.timeStamp);
                                //seek back to the tag header, for prefetch.
                                result = CdxStreamSeek(s->fp,
                                    (cdx_int32)(0-sizeof(FlvTagsT)), STREAM_SEEK_CUR);
                                if(result < 0)
                                {
                                    CDX_LOGW("Seek back to the tag header failed.");
                                    impl->mErrno = PSR_UNKNOWN_ERR;
                                    ret = -1;
                                    goto __exit;
                                }
                            }
                            break;
                        }
                        s->lastKeyFrmPts = s->tag.timeStamp;
                        s->lastKeyFrmPos = CdxStreamTell(s->fp) - sizeof(FlvTagsT);
                        CDX_LOGV("xxx ff s->tag.timeStamp(%u) < seekTime(%lld),"
                            " continue find keyframe...", s->tag.timeStamp, seekTime);
                    }

                    //seek to next tag header
                    result = CdxStreamSeek(s->fp, (cdx_int64)(s->tag.dataSize-1), STREAM_SEEK_CUR);
                    if(result < 0)
                    {
                        result = CdxStreamSeek(s->fp, s->lastKeyFrmPos, STREAM_SEEK_SET);
                        if(result < 0)
                        {
                            CDX_LOGE("seek to last key frame tag header failed.");
                            impl->mErrno = PSR_UNKNOWN_ERR;
                            ret = -1;
                            goto __exit;
                        }
                        break;
                    }
                }
                break;
            }
        }
    }

    impl->hasSyncVideo = 0;
    impl->hasSyncAudio = 0;

__exit:
    pthread_mutex_lock(&impl->lock);
    impl->mStatus = CDX_FLV_IDLE;
    impl->mErrno = PSR_OK;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return ret;
}
static cdx_uint32 __CdxFlvParserAttribute(CdxParserT *parser)
{//not use yet..
    CdxFlvParserImplT *impl;

    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);
    impl->flags = 0;

    return impl->flags;
}
static cdx_int32 CdxFlvParserForceStop(CdxParserT *parser)
{
    CdxFlvParserImplT *impl;
    struct VdecFlvIn *tmpFlvIn;
    CdxStreamT *stream;

    CDX_CHECK(parser);
    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);
    tmpFlvIn = (struct VdecFlvIn *)impl->privData;
    stream = tmpFlvIn->fp;

    pthread_mutex_lock(&impl->lock);
    impl->exitFlag = 1;
    pthread_mutex_unlock(&impl->lock);
    if(stream)
    {
        CdxStreamForceStop(stream);
    }
    pthread_mutex_lock(&impl->lock);
    while((impl->mStatus != CDX_FLV_IDLE) && (impl->mStatus != CDX_FLV_PREFETCHED))
    {
        pthread_cond_wait(&impl->cond, &impl->lock);
    }
    pthread_mutex_unlock(&impl->lock);

    impl->mErrno = PSR_USER_CANCEL;
    impl->mStatus = CDX_FLV_IDLE;
    CDX_LOGV("xxx flv forcestop finish.");

    return CDX_SUCCESS;
}
static cdx_int32 CdxFlvParserClrForceStop(CdxParserT *parser)
{
    CdxFlvParserImplT *impl;
    struct VdecFlvIn *tmpFlvIn;
    CdxStreamT *stream;

    CDX_CHECK(parser);
    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);
    tmpFlvIn = (struct VdecFlvIn *)impl->privData;
    stream = tmpFlvIn->fp;

    if(impl->mStatus != CDX_FLV_IDLE)
    {
        CDX_LOGW("impl->mStatus != CDX_FLV_IDLE");
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    impl->exitFlag = 0;
    if(stream)
    {
        CdxStreamClrForceStop(stream);
    }

    return 0;
}
static int FlvGetCacheState(CdxFlvParserImplT *impl, struct ParserCacheStateS *cacheState)
{
    struct StreamCacheStateS streamCS;

    if (CdxStreamControl(impl->stream, STREAM_CMD_GET_CACHESTATE, &streamCS) < 0)
    {
        CDX_LOGE("STREAM_CMD_GET_CACHESTATE fail");
        return -1;
    }

    cacheState->nCacheCapacity = streamCS.nCacheCapacity;
    cacheState->nCacheSize = streamCS.nCacheSize;
    cacheState->nBandwidthKbps = streamCS.nBandwidthKbps;
    cacheState->nPercentage = streamCS.nPercentage;

    return 0;
}

/*
    CDX_PSR_CMD_MEDIAMODE_CONTRL,

    CDX_PSR_CMD_SWITCH_AUDIO,
    CDX_PSR_CMD_SWITCH_SUBTITLE,

    CDX_PSR_CMD_DISABLE_AUDIO,
    CDX_PSR_CMD_DISABLE_VIDEO,
    CDX_PSR_CMD_DISABLE_SUBTITLE,*/

static cdx_int32 __CdxFlvParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    CdxFlvParserImplT *impl;
    struct VdecFlvIn  *pFlvIn;

    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);

    if(NULL == impl || NULL == impl->privData)
    {
       CDX_LOGW("pFlvPsr == NULL");
       return CDX_FAILURE;
    }
    pFlvIn = (struct VdecFlvIn *)impl->privData;

    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        {
            CDX_LOGW("not support CDX_PSR_CMD_SWITCH_AUDIO.");
            break;
        }
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
        {
            CDX_LOGW("not support CDX_PSR_CMD_SWITCH_SUBTITLE.");
            break;
        }
        case CDX_PSR_CMD_SET_FORCESTOP:
        {
            return CdxFlvParserForceStop(parser);
        }
        case CDX_PSR_CMD_CLR_FORCESTOP:
        {
            return CdxFlvParserClrForceStop(parser);
        }
        case CDX_PSR_CMD_GET_CACHESTATE:
        {
            return FlvGetCacheState(impl, param);
        }
        default:
        {
            CDX_LOGW("cmd(%d) not support yet...", cmd);
            break;
        }
    }

    return CDX_SUCCESS;
}

static cdx_int32 __CdxFlvParserGetStatus(CdxParserT *parser)
{
    CdxFlvParserImplT *impl;

    CDX_CHECK(parser);
    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);

    return impl->mErrno;
}

/*
***********************************************************************************************
*                       FLV struct cdx_stream_info PARSER CLOSE FLV struct cdx_stream_info
*
*Description: flv file parser close flv file.
*
*Arguments  : pFlvPsr   the pointer to  the flv file parser lib handle;
*
*Return     : close flv file result;
*               =   0   close flv file successed;
*               <   0   close flv file failed;
*
*Note       : This function close the media file, and release all memory resouce.
***********************************************************************************************
*/
static cdx_int32 __CdxFlvParserClose(CdxParserT *parser)
{
    CdxFlvParserImplT *impl;
    struct VdecFlvIn *s;

    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);
    impl->exitFlag = 1;

    s = (struct VdecFlvIn *)impl->privData;
    if(s)
    {
        FlvReaderCloseFile(s);
        if(s->dataChunk.buffer)
        {
            free(s->dataChunk.buffer);
            s->dataChunk.buffer = NULL;
        }
        free(impl->privData);
        impl->privData = NULL;
    }

#if SAVE_VIDEO_STREAM
    if(impl->fpVideoStream)
    {
        fclose(impl->fpVideoStream);
    }
#endif
#if SAVE_AUDIO_STREAM
    int i;
    for(i = 0; i < AUDIO_STREAM_LIMIT; i++)
    {
        if(impl->fpAudioStream[i])
        {
            fclose(impl->fpAudioStream[i]);
        }
    }
#endif

    if(impl->tempBuf)
    {
        free(impl->tempBuf);
        impl->tempBuf = NULL;
    }

    if(impl->aFormat.pCodecSpecificData)
    {
        free(impl->aFormat.pCodecSpecificData);
    }
    if(impl->vFormat.pCodecSpecificData)
    {
        free(impl->vFormat.pCodecSpecificData);
    }
    if(impl->tFormat.pCodecSpecificData)
    {
        free(impl->tFormat.pCodecSpecificData);
    }
    pthread_mutex_destroy(&impl->lock);
    pthread_cond_destroy(&impl->cond);
    CdxFree(impl);

    return CDX_SUCCESS;
}

static int __CdxFlvParserInit(CdxParserT *parser)
{
    cdx_int32 result, ret = 0;
    CdxFlvParserImplT *impl;

    CDX_CHECK(parser);
    impl = CdxContainerOf(parser, CdxFlvParserImplT, base);

    result = FlvOpen(impl);
    if(result < 0)
    {
        CDX_LOGE("Open flv failed.");
        impl->mErrno = PSR_OPEN_FAIL;
        ret = -1;
        goto __exit;
    }
    CDX_LOGI("read flv head finish.");

    impl->mErrno = PSR_OK;
    ret = 0;

__exit:
    pthread_mutex_lock(&impl->lock);
    impl->mStatus = CDX_FLV_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return ret;
}

static struct CdxParserOpsS flvParserOps =
{
    .control      = __CdxFlvParserControl,
    .prefetch     = __CdxFlvParserPrefetch,
    .read         = __CdxFlvParserRead,
    .getMediaInfo = __CdxFlvParserGetMediaInfo,
    .seekTo        = __CdxFlvParserSeekTo,
    .attribute    = __CdxFlvParserAttribute,
    .getStatus    = __CdxFlvParserGetStatus,
    .close        = __CdxFlvParserClose,
    .init         = __CdxFlvParserInit
};

/*
**********************************************************************************************
*                       FLV struct cdx_stream_info PARSER OPEN FLV struct cdx_stream_info
*
*Description: flv file parser open flv file.
*
*Arguments  : pFlvPsr   the pointer to the flv file parser lib handle;
*             nFmtType  the type of the media file, parsed by cedar, can be ignored;
*             pFile     flv file path;
*
*Return     : open flv file result;
*               =   0   open flv file successed;
*               <   0   open flv file failed;
*
*Note       : This function open the flv file with the file path, create the flv file information
*             handle, and get the video, audio and subtitle bitstream information from the file
*             to set in the flv file information handle. Then, set the flv file information handle
*             to the pFlvPsr for return.
**********************************************************************************************
*/
static CdxParserT *__CdxFlvParserCreate(CdxStreamT *stream, cdx_uint32 flags)
{
    CdxFlvParserImplT *impl;
    cdx_int32 result;

    if(flags > 0)
    {
        CDX_LOGI("Flv not support multi-stream yet...");
    }

    impl = FlvInit(&result);
    CDX_FORCE_CHECK(impl);
    if(result < 0)
    {
        CDX_LOGE("Initiate flv file parser lib module error.");
        goto failure;
    }

    impl->base.ops = &flvParserOps;
    impl->stream = stream;

    impl->nVidPtsOffset = 0;
    impl->nAudPtsOffset = 0;
    impl->nSubPtsOffset = 0;

    impl->hasSyncVideo = 0;
    impl->hasSyncAudio = 0;
    impl->bFirstVidFrm = 1;

    impl->curChunkInfo.nChkType = 0;
    impl->curChunkInfo.nTotSize = 0;
    impl->curChunkInfo.nReadSize = 0;

#if SAVE_VIDEO_STREAM
    impl->fpVideoStream = fopen("/data/camera/flv_videostream.es", "wb+");
    if (!impl->fpVideoStream)
    {
        CDX_LOGE("open video stream debug file failure errno(%d)", errno);
    }
#endif
#if SAVE_AUDIO_STREAM
    int i;
    for(i = 0; i < impl->hasAudio && i < AUDIO_STREAM_LIMIT; i++)
    {
        sprintf(impl->url, "/data/camera/flv_audiostream_%d.es", i);
        impl->fpAudioStream[i] = fopen(impl->url, "wb+");
        if (!impl->fpAudioStream[i])
        {
            CDX_LOGE("open audio stream debug file failure errno(%d)", errno);
        }
    }
#endif
    pthread_mutex_init(&impl->lock, NULL);
    pthread_cond_init(&impl->cond, NULL);
    impl->mStatus = CDX_FLV_INITIALIZED;
    impl->mErrno = PSR_INVALID;
    return &impl->base;

failure:
    if(impl)
    {
        __CdxFlvParserClose(&impl->base);
        impl = NULL;
    }
    return NULL;
}

//probe
static int FlvProbe(CdxStreamProbeDataT *p)
{
    cdx_char *d;

    d = p->buf;
    if (d[0] == 'F' && d[1] == 'L' && d[2] == 'V' && d[3] < 5 && d[5] == 0)
    {
        return CDX_TRUE;
    }

    return CDX_FALSE;
}
static cdx_uint32 __CdxFlvParserProbe(CdxStreamProbeDataT *probeData)
{
    if(probeData->len < 9)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }

    if(!FlvProbe(probeData))
    {
        CDX_LOGE("FlvProbe failed.");
        return 0;
    }

    return 100;
}

CdxParserCreatorT flvParserCtor =
{
    .create = __CdxFlvParserCreate,
    .probe = __CdxFlvParserProbe
};
