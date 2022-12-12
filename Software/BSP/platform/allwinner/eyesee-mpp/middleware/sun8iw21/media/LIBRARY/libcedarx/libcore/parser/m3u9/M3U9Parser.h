/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : M3U9Parser.h
 * Description : M3U9Parser
 * History :
 *
 */

#ifndef M3U9_H
#define M3U9_H
#include <CdxTypes.h>

typedef enum bool
{
    false,
    true
}bool;

typedef enum status_t
{
    OK                                 = 0,
    err_unknown                     = -1,
    ERROR_MALFORMED                 = -2,
    ERROR_maxNumAtom_too_little     = -3,
    err_no_memory                     = -4,
    err_URL                         = -5,
    err_no_bandwidth                = -6,
    err_allocateBigBuffer             = -7,
}status_t;

typedef struct PlaylistItem PlaylistItem;
struct PlaylistItem
{
    char *mURI;
    cdx_int64 durationUs;
    cdx_int32 seqNum;
    cdx_int64 baseTimeUs;
    PlaylistItem *next;
};

typedef struct Playlist Playlist;
struct Playlist
{
    char *mBaseURI;
    PlaylistItem *mItems;
    cdx_uint32 mNumItems;
    cdx_int32 lastSeqNum;
    cdx_int64 durationUs;
};
PlaylistItem *findItemBySeqNumForM3u9(Playlist *playlist, int seqNum);
PlaylistItem *findItemByIndexForM3u9(Playlist *playlist, int index);
status_t M3u9Parse(const void *_data, cdx_uint32 size, Playlist **P, const char *baseURI);
bool M3u9Probe(const char *data, cdx_uint32 size);
void destoryM3u9Playlist(Playlist *playList);
#endif
