/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : PlaylistParser.h
 * Description :
 * History :
 *
 */

#ifndef PLAYLIST_H
#define PLAYLIST_H
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
PlaylistItem *findItemBySeqNumForPlaylist(Playlist *playlist, int seqNum);
PlaylistItem *findItemByIndexForPlaylist(Playlist *playlist, int index);
status_t PlaylistParse(const void *_data, cdx_uint32 size, Playlist **P, const char *baseURI);
bool PlaylistProbe(const char *data, cdx_uint32 size);
void destoryPlaylist_P(Playlist *playList);
#endif

