/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxBdParser.h
 * Description : BdParser
 * History :
 *
 */

#ifndef BD_PARSER_H
#define BD_PARSER_H
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>
#include <CdxList.h>
#include <dirent.h>

enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
};
typedef struct {
    int PCR_PID;
    cdx_uint32 SPN_STC_start;
    int presentation_start_time;
    int presentation_end_time;
}STC;

typedef struct {
    int SPN_ATC_start;
    int number_of_STC_sequences;
    int offset_STC_id;
    STC *stc;
}ATC;
typedef enum {
    TYPE_UNKNOWN    = -1,
    TYPE_VIDEO      = 0,
    TYPE_AUDIO        = 1,
    TYPE_SUBS_PG    = 2,
    TYPE_SUBS_TEXT  = 3,
}MediaType;

typedef struct AudioMetaDataS AudioMetaData;
struct AudioMetaDataS
{
    int audioPresentationType;
    int channelNum;
    int samplingFrequency;
    int bitPerSample;
};
typedef struct VideoMetaDataS VideoMetaData;
struct VideoMetaDataS
{
    int videoFormat;
    int frameRate;
    int aspectRatio;
    int ccFlag;
};
typedef struct {
    int ref_to_EP_fine_id;
    cdx_uint32 PTS_EP_coarse;
    cdx_uint32 SPN_EP_coarse;
}EPMapCoarse;

typedef struct {
    cdx_uint32 PTS_EP_fine;
    cdx_uint32 SPN_EP_fine;
}EPMapFine;
typedef struct {
    int stream_PID;
    int EP_stream_type;
    int number_of_EP_coarse_entries;
    int number_of_EP_fine_entries;
    //int start_address;
    EPMapCoarse *coarse;
    EPMapFine *fine;
}EPMapForOneStream;

typedef struct {
    int stream_PID;
    int coding_type;
    int mediaType;
    cdx_uint8 lang[3];
    int character_code;//text subtitle
    EPMapForOneStream *epMapForOneStream;
    void *metadata;
}StreamInfo;

typedef struct {
    int SPN_program_sequence_start;
    int program_map_PID;
    int number_of_streams_in_ps;
    StreamInfo *streams;
}ProgramSequence;

typedef struct {
    cdx_uint8 ClipName[6];
    int application_type;
    int number_of_source_packets;
    int firstItemIndex;
    int finalItemIndex;

    int number_of_ATC_sequences;
    ATC *atc;

    int number_of_program_sequences;
    ProgramSequence *ps;

    int number_of_stream_PID_entries;
    EPMapForOneStream *epMapForOneStream;
    unsigned audioCount;
    unsigned videoCount;
    unsigned subsCount;
}Clip;
typedef struct {
    Clip *ditto;/*非空时,说明curPlayItem和prePlayItem同属一个clip,为空时，信息存于clip*/
    Clip clip;
    int ref_to_STC_id;
    cdx_uint32 IN_time;
    cdx_uint32 OUT_time;
    cdx_int64 durationUs;
}PlayItem;
typedef struct {
    Clip *ditto;
    Clip clip;
    int ref_to_STC_id;
    cdx_uint32 IN_time;
    cdx_uint32 OUT_time;

    int sync_PlayItem_id;
    int sync_start_PTS_of_PlayItem;
}SubPlayItem;

typedef struct {
    //int SubPath_type;
    int number_of_SubPlayItems;
    SubPlayItem *subPlayItems;
}SubPath;

typedef struct {
    struct CdxListNodeS node;
    cdx_uint8 mplsFileName[11];
    int Playlist_start_address;
    int ExtensionData_start_address;

    int Playlist_playback_type;
    int MVC_Base_view_R_flag;

    cdx_int64 durationUs;
    int numer_of_PlayItems;
    PlayItem *playItems;
    int numer_of_SubPaths;//播放列表中SubPath数目
    SubPath *subPath;
    SubPath *subPathE;//subPathExtension
    cdx_uint8 *curPosition;

    cdx_int32 audioCount;
    cdx_int32 videoCount;
    cdx_int32 subsCount;
}Playlist;

typedef struct {
    CdxParserT base;
    enum CdxParserStatus status;
    pthread_mutex_t statusLock;
    int mErrno;
    cdx_uint32 flags;/*使能标志*/

    int forceStop;
    pthread_cond_t cond;

    CdxStreamT *cdxStream;
    CdxDataSourceT cdxDataSource;/*用于child打开*/

    char *fileUrl;
    cdx_uint8 *fileBuf;
    cdx_int32 fileBufSize;
    cdx_int32 fileValidDataSize;

    char filename[512];

    void *dir;
    int fd;

    struct CdxListS playlists;
    int playlistIsAppointed;
    int playlistCount;
    Playlist *curPlaylist;
    int curPlayItemIndex;
    int curSubPlayItemIndex;
    Clip *curClip;
    Clip *curSubClip;
    CdxParserT *child[3];/*为子parser，下一级的hls parser或者媒体文件parser，如TS parser*/
    CdxStreamT *childStream[3];
    CdxMediaInfoT mediaInfo;

    struct StreamSeekPos streamSeekPos;

    int hasMvcVideo;
    CdxPacketT pkt;

    cdx_uint64 streamPts[4];
    int prefetchType;

}CdxBdParser;

#endif
