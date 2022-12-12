/*
 * MP3 muxer and demuxer
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
 
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include "mp3.h"

const char * const ff_id3v1_genre_str[ID3v1_GENRE_MAX + 1] = {
      [0] = "Blues",
      [1] = "Classic Rock",
      [2] = "Country",
      [3] = "Dance",
      [4] = "Disco",
      [5] = "Funk",
      [6] = "Grunge",
      [7] = "Hip-Hop",
      [8] = "Jazz",
      [9] = "Metal",
     [10] = "New Age",
     [11] = "Oldies",
     [12] = "Other",
     [13] = "Pop",
     [14] = "R&B",
     [15] = "Rap",
     [16] = "Reggae",
     [17] = "Rock",
     [18] = "Techno",
     [19] = "Industrial",
     [20] = "Alternative",
     [21] = "Ska",
     [22] = "Death Metal",
     [23] = "Pranks",
     [24] = "Soundtrack",
     [25] = "Euro-Techno",
     [26] = "Ambient",
     [27] = "Trip-Hop",
     [28] = "Vocal",
     [29] = "Jazz+Funk",
     [30] = "Fusion",
     [31] = "Trance",
     [32] = "Classical",
     [33] = "Instrumental",
     [34] = "Acid",
     [35] = "House",
     [36] = "Game",
     [37] = "Sound Clip",
     [38] = "Gospel",
     [39] = "Noise",
     [40] = "AlternRock",
     [41] = "Bass",
     [42] = "Soul",
     [43] = "Punk",
     [44] = "Space",
     [45] = "Meditative",
     [46] = "Instrumental Pop",
     [47] = "Instrumental Rock",
     [48] = "Ethnic",
     [49] = "Gothic",
     [50] = "Darkwave",
     [51] = "Techno-Industrial",
     [52] = "Electronic",
     [53] = "Pop-Folk",
     [54] = "Eurodance",
     [55] = "Dream",
     [56] = "Southern Rock",
     [57] = "Comedy",
     [58] = "Cult",
     [59] = "Gangsta",
     [60] = "Top 40",
     [61] = "Christian Rap",
     [62] = "Pop/Funk",
     [63] = "Jungle",
     [64] = "Native American",
     [65] = "Cabaret",
     [66] = "New Wave",
     [67] = "Psychadelic",
     [68] = "Rave",
     [69] = "Showtunes",
     [70] = "Trailer",
     [71] = "Lo-Fi",
     [72] = "Tribal",
     [73] = "Acid Punk",
     [74] = "Acid Jazz",
     [75] = "Polka",
     [76] = "Retro",
     [77] = "Musical",
     [78] = "Rock & Roll",
     [79] = "Hard Rock",
     [80] = "Folk",
     [81] = "Folk-Rock",
     [82] = "National Folk",
     [83] = "Swing",
     [84] = "Fast Fusion",
     [85] = "Bebob",
     [86] = "Latin",
     [87] = "Revival",
     [88] = "Celtic",
     [89] = "Bluegrass",
     [90] = "Avantgarde",
     [91] = "Gothic Rock",
     [92] = "Progressive Rock",
     [93] = "Psychedelic Rock",
     [94] = "Symphonic Rock",
     [95] = "Slow Rock",
     [96] = "Big Band",
     [97] = "Chorus",
     [98] = "Easy Listening",
     [99] = "Acoustic",
    [100] = "Humour",
    [101] = "Speech",
    [102] = "Chanson",
    [103] = "Opera",
    [104] = "Chamber Music",
    [105] = "Sonata",
    [106] = "Symphony",
    [107] = "Booty Bass",
    [108] = "Primus",
    [109] = "Porn Groove",
    [110] = "Satire",
    [111] = "Slow Jam",
    [112] = "Club",
    [113] = "Tango",
    [114] = "Samba",
    [115] = "Folklore",
    [116] = "Ballad",
    [117] = "Power Ballad",
    [118] = "Rhythmic Soul",
    [119] = "Freestyle",
    [120] = "Duet",
    [121] = "Punk Rock",
    [122] = "Drum Solo",
    [123] = "A capella",
    [124] = "Euro-House",
    [125] = "Dance Hall",
    [126] = "Goa",
    [127] = "Drum & Bass",
    [128] = "Club-House",
    [129] = "Hardcore",
    [130] = "Terror",
    [131] = "Indie",
    [132] = "BritPop",
    [133] = "Negerpunk",
    [134] = "Polsk Punk",
    [135] = "Beat",
    [136] = "Christian Gangsta",
    [137] = "Heavy Metal",
    [138] = "Black Metal",
    [139] = "Crossover",
    [140] = "Contemporary Christian",
    [141] = "Christian Rock",
    [142] = "Merengue",
    [143] = "Salsa",
    [144] = "Thrash Metal",
    [145] = "Anime",
    [146] = "JPop",
    [147] = "SynthPop",
};

const char ff_id3v2_tags[][4] = {
   "TALB", "TBPM", "TCOM", "TCON", "TCOP", "TDEN", "TDLY", "TDOR", "TDRC",
   "TDRL", "TDTG", "TENC", "TEXT", "TFLT", "TIPL", "TIT1", "TIT2", "TIT3",
   "TKEY", "TLAN", "TLEN", "TMCL", "TMED", "TMOO", "TOAL", "TOFN", "TOLY",
   "TOPE", "TOWN", "TPE1", "TPE2", "TPE3", "TPE4", "TPOS", "TPRO", "TPUB",
   "TRCK", "TRSN", "TRSO", "TSOA", "TSOP", "TSOT", "TSRC", "TSSE", "TSST",
   { 0 },
};

static __inline void put_byte(FsWriter *s, int b)
{
    s->fsWrite(s, (char*)&b, 1);
}

static void put_buffer(FsWriter *s, char *buf, int size)
{
    s->fsWrite(s, buf, size);
}

//static void put_le32(FsWriter *s, unsigned int val)
//{
//    s->fsWrite(s, (char*)&val, 4);
//}

static void put_be32(FsWriter *s, unsigned int val)
{
	val= ((val<<8)&0xFF00FF00) | ((val>>8)&0x00FF00FF);
    val= (val>>16) | (val<<16);
    s->fsWrite(s, (char*)&val, 4);
}

// void put_le16(FILE *s, unsigned int val)
// {
//     put_byte(s, val);
//     put_byte(s, val >> 8);
// }
// 
static void put_be16(FsWriter *s, unsigned int val)
{
    put_byte(s, val >> 8);
    put_byte(s, val);
}
// 
// void put_le24(FILE *s, unsigned int val)
// {
//     put_le16(s, val & 0xffff);
//     put_byte(s, val >> 16);
// }
// 
//static void put_be24(FsWriter *s, unsigned int val)
//{
//    put_be16(s, val >> 8);
//    put_byte(s, val);
//}

//static void put_tag(FsWriter *s, const char *tag)
//{
//    while (*tag) {
//        put_byte(s, *tag++);
//    }
//}

static AVMetadataTag *
av_metadata_get(AVMetadata *m, const char *key, const AVMetadataTag *prev, int flags)
{
    int i, j;

    if(!m)
        return NULL;

    if(prev) i= prev - m->elems + 1;
    else     i= 0;

    for(; i<m->count; i++){
        const char *s= m->elems[i].key;
        if(flags & AV_METADATA_MATCH_CASE) for(j=0;         s[j]  ==         key[j]  && key[j]; j++);
        else                               for(j=0; toupper(s[j]) == toupper(key[j]) && key[j]; j++);
        if(key[j])
            continue;
        if(s[j] && !(flags & AV_METADATA_IGNORE_SUFFIX))
            continue;
        return &m->elems[i];
    }
    return NULL;
}

static int id3v1_set_string(AVFormatContext *s, const char *key,
                            unsigned char *buf, int buf_size)
{
    AVMetadataTag *tag;
    if ((tag = av_metadata_get(s->metadata, key, NULL, 0)))
        strncpy((char*)buf, tag->value, buf_size);
    return !!tag;
}

static int id3v1_create_tag(AVFormatContext *s, unsigned char *buf)
{
    AVMetadataTag *tag;
    int i, count = 0;

    memset(buf, 0, ID3v1_TAG_SIZE); /* fail safe */
    buf[0] = 'T';
    buf[1] = 'A';
    buf[2] = 'G';
    count += id3v1_set_string(s, "title",   buf +  3, 30);
    count += id3v1_set_string(s, "author",  buf + 33, 30);
    count += id3v1_set_string(s, "album",   buf + 63, 30);
    count += id3v1_set_string(s, "date",    buf + 93,  4);
    count += id3v1_set_string(s, "comment", buf + 97, 30);
    if ((tag = av_metadata_get(s->metadata, "track", NULL, 0))) {
        buf[125] = 0;
        buf[126] = atoi(tag->value);
        count++;
    }
    buf[127] = 0xFF; /* default to unknown genre */
    if ((tag = av_metadata_get(s->metadata, "genre", NULL, 0))) {
        for(i = 0; i <= ID3v1_GENRE_MAX; i++) {
            if (!strcasecmp(tag->value, ff_id3v1_genre_str[i])) {
                buf[127] = i;
                count++;
                break;
            }
        }
    }
    return count;
}

/* simple formats */

static void id3v2_put_size(AVFormatContext *s, int size)
{
    put_byte(s->mpFsWriter, size >> 21 & 0x7f);
    put_byte(s->mpFsWriter, size >> 14 & 0x7f);
    put_byte(s->mpFsWriter, size >> 7  & 0x7f);
    put_byte(s->mpFsWriter, size       & 0x7f);
}

static void id3v2_put_ttag(AVFormatContext *s, char *buf, int len,
                           unsigned int tag)
{
    put_be32(s->mpFsWriter, tag);
    id3v2_put_size(s, len + 1);
    put_be16(s->mpFsWriter, 0);
    put_byte(s->mpFsWriter, 3); /* UTF-8 */
    put_buffer(s->mpFsWriter, (char *)buf, len);
}

int mp3_write_packet(struct AVFormatContext *s, AVPacket *pkt)
{
    put_buffer(s->mpFsWriter, pkt->data0, pkt->size0);
    //put_flush_packet(s->pb);
    return 0;
}

int mp3_write_trailer(struct AVFormatContext *s)
{
    unsigned char buf[ID3v1_TAG_SIZE];

    /* write the id3v1 tag */
    if (id3v1_create_tag(s, buf) > 0) {
        put_buffer(s->mpFsWriter, (char *)buf, ID3v1_TAG_SIZE);
        //put_flush_packet(s->pb);
    }
    return 0;
}

/**
 * Write an ID3v2.4 header at beginning of stream
 */
int mp3_write_header(struct AVFormatContext *s)
{
    AVMetadataTag *t = NULL;
    int totlen = 0;
    int64_t size_pos, cur_pos;

    put_be32(s->mpFsWriter, MKBETAG('I', 'D', '3', 0x04)); /* ID3v2.4 */
    put_byte(s->mpFsWriter, 0);
    put_byte(s->mpFsWriter, 0); /* flags */

    /* reserve space for size */
    size_pos = s->mpFsWriter->fsTell(s->mpFsWriter);
    put_be32(s->mpFsWriter, 0);

    while ((t = av_metadata_get(s->metadata, "", t, AV_METADATA_IGNORE_SUFFIX))) {
        unsigned int tag = 0;

        if (t->key[0] == 'T' && strlen(t->key) == 4) {
            int i;
            for (i = 0; *ff_id3v2_tags[i]; i++)
                if (AV_RB32(t->key) == AV_RB32(ff_id3v2_tags[i])) {
                    int len = strlen(t->value);
                    tag = AV_RB32(t->key);
                    totlen += len + ID3v2_HEADER_SIZE + 2;
                    id3v2_put_ttag(s, t->value, len + 1, tag);
                    break;
                }
        }

        if (!tag) { /* unknown tag, write as TXXX frame */
            int   len = strlen(t->key), len1 = strlen(t->value);
            char *buf = av_malloc(len + len1 + 2);
            if (!buf)
                return AVERROR(ENOMEM);
            tag = MKBETAG('T', 'X', 'X', 'X');
            strcpy(buf,           t->key);
            strcpy(buf + len + 1, t->value);
            id3v2_put_ttag(s, buf, len + len1 + 2, tag);
            totlen += len + len1 + ID3v2_HEADER_SIZE + 3;
            av_free(buf);
        }
    }

    cur_pos = s->mpFsWriter->fsTell(s->mpFsWriter);
    s->mpFsWriter->fsSeek(s->mpFsWriter, size_pos, SEEK_SET);
    id3v2_put_size(s, totlen);
    s->mpFsWriter->fsSeek(s->mpFsWriter, cur_pos, SEEK_SET);
    return 0;
}

