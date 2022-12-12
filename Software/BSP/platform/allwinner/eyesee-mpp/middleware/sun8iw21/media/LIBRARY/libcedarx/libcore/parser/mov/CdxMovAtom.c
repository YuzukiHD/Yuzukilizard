/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMovAtom.c
 * Description :
 * History
 * Date    : 2017/05/02
 * Comment : refactoring mov parser,mov about atom implement in this file
 */

#include <stdint.h>
#include "CdxMovAtom.h"
#include "mpeg4Vol.h"
#include <zconf.h>
#include <zlib.h>
#include <errno.h>
#include <stdint.h>

//#define LOG_TAG "Mov"

#define KEY_FRAME_PTS_CHECK     (0)
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define InterlaceMode

#define ABS_EDIAN_FLAG_MASK         ((unsigned int)(1<<16))
#define ABS_EDIAN_FLAG_LITTLE       ((unsigned int)(0<<16))
#define ABS_EDIAN_FLAG_BIG          ((unsigned int)(1<<16))

#define ID3v1_GENRE_MAX 147

#define MAX_READ_LEN_ONCE  5*1024*1024

static int _MovParseFtyp(MOVContext *s,unsigned char *buf,MOV_atom_t a);
static int _MovParseWide(MOVContext *s,unsigned char *buf,MOV_atom_t a);
static int _MovParseMoov(MOVContext *c,unsigned char *buf,MOV_atom_t atom);
static int _MovParseMdat(MOVContext *s,unsigned char *buf,MOV_atom_t a);
static int _MovParseStyp(MOVContext *s,unsigned char *buf,MOV_atom_t a);
static int _MovParseSidx(MOVContext *s,unsigned char *buf,MOV_atom_t atom);
static int _MovParseMoof(MOVContext *s,unsigned char *buf,MOV_atom_t atom);
static int _MovParseTraf(MOVContext *s,MOV_atom_t atom);
static int _MovParseTfhd(MOVContext *s,MOV_atom_t atom);
static int _MovParseTrun(MOVContext *s,MOV_atom_t atom);
static int _MovParseSenc(MOVContext *s,MOV_atom_t atom);
static int _MovParseMvhd(MOVContext *c,MOV_atom_t a);
static int _MovParseUdta(MOVContext *c,MOV_atom_t a);
static int _MovParseKeys(MOVContext *c,MOV_atom_t a);
static int _MovParseMeta(MOVContext *c,MOV_atom_t a);
static int _MovParseTrak(MOVContext *c,MOV_atom_t a);
static int _MovParseMvex(MOVContext *c,MOV_atom_t a);
static int _MovParseCmov(MOVContext *c,MOV_atom_t a);
static int _MovParseTrex(MOVContext *c,MOV_atom_t a);
static int _MovParseTkhd(MOVContext *c,MOV_atom_t a);
static int _MovParseElst(MOVContext *c,MOV_atom_t a);
static int _MovParseMdia(MOVContext *c,MOV_atom_t a);
static int _MovParseMdhd(MOVContext *c,MOV_atom_t a);
static int _MovParseHdlr(MOVContext *c,MOV_atom_t a);
static int _MovParseMinf(MOVContext *c,MOV_atom_t a);
static int _MovParseStbl(MOVContext *c,MOV_atom_t a);
static int _MovParseStsd(MOVContext *c,MOV_atom_t a);
static int _MovParseStts(MOVContext *c,MOV_atom_t a);
static int _MovParseStss(MOVContext *c,MOV_atom_t a);
static int _MovParseStsz(MOVContext *c,MOV_atom_t a);
static int _MovParseStsc(MOVContext *c,MOV_atom_t a);
static int _MovParseStco(MOVContext *c,MOV_atom_t a);
static int _MovParseCtts(MOVContext *c,MOV_atom_t a);
static int _MovParseSbgp(MOVContext *c,MOV_atom_t a);
static int _MovParseAvcc(MOVContext *c,MOV_atom_t a);
static int _MovParseHvcc(MOVContext *c,MOV_atom_t a);
static int _MovParseWave(MOVContext *c,MOV_atom_t a);
static int _MovParseEsds(MOVContext *c,MOV_atom_t a);
static int _MovParseGlbl(MOVContext *c,MOV_atom_t a);

MovTopParserProcess g_mov_top_parser[] =
{
    {MESSAGE_ID_FTYP,    _MovParseFtyp},
    {MESSAGE_ID_WIDE,    _MovParseWide},
    {MESSAGE_ID_MOOV,    _MovParseMoov},
    {MESSAGE_ID_MDAT,    _MovParseMdat},
    {MESSAGE_ID_STYP,    _MovParseStyp},
    {MESSAGE_ID_SIDX,    _MovParseSidx},
    {MESSAGE_ID_MOOF,    _MovParseMoof},
};

static MovParserProcess s_mov_moov_parser[] =
{
    {MESSAGE_ID_MVHD,    _MovParseMvhd},
    {MESSAGE_ID_UDTA,    _MovParseUdta},
    {MESSAGE_ID_KEYS,    _MovParseKeys},
    {MESSAGE_ID_TRAK,    _MovParseTrak},
    {MESSAGE_ID_MVEX,    _MovParseMvex},
    {MESSAGE_ID_CMOV,    _MovParseCmov},
    {MESSAGE_ID_TREX,    _MovParseTrex},
    {MESSAGE_ID_META,    _MovParseMeta},
};

static MovParserProcess s_mov_moof_parser[] =
{
    {MESSAGE_ID_TRAF,    _MovParseTraf},
    {MESSAGE_ID_TFHD,    _MovParseTfhd},
    {MESSAGE_ID_TRUN,    _MovParseTrun},
    {MESSAGE_ID_SENC,    _MovParseSenc},
};

static MovParserProcess s_mov_trak_parser[] =
{
    {MESSAGE_ID_TKHD,    _MovParseTkhd},
    {MESSAGE_ID_ELST,    _MovParseElst},
    {MESSAGE_ID_MDIA,    _MovParseMdia},
};

static MovParserProcess s_mov_mdia_parser[] =
{
    {MESSAGE_ID_MDHD,    _MovParseMdhd},
    {MESSAGE_ID_HDLR,    _MovParseHdlr},
    {MESSAGE_ID_MINF,    _MovParseMinf},
};

static MovParserProcess s_mov_stbl_parser[] =
{
    {MESSAGE_ID_STBL,    _MovParseStbl},
    {MESSAGE_ID_STSD,    _MovParseStsd},
    {MESSAGE_ID_STTS,    _MovParseStts},
    {MESSAGE_ID_STSS,    _MovParseStss},
    {MESSAGE_ID_STSZ,    _MovParseStsz},
    {MESSAGE_ID_STSC,    _MovParseStsc},
    {MESSAGE_ID_STCO,    _MovParseStco},
    {MESSAGE_ID_CTTS,    _MovParseCtts},
    {MESSAGE_ID_SBGP,    _MovParseSbgp},
};

static MovParserProcess s_mov_stsd_parser[] =
{
    {MESSAGE_ID_AVCC,    _MovParseAvcc},
    {MESSAGE_ID_HVCC,    _MovParseHvcc},
    {MESSAGE_ID_WAVE,    _MovParseWave},
    {MESSAGE_ID_ESDS,    _MovParseEsds},
    {MESSAGE_ID_GLBL,    _MovParseGlbl},
};

/* See Genre List at http://id3.org/id3v2.3.0 */
static const char * const ff_id3v1_genre_str[ID3v1_GENRE_MAX + 1] = {
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
     [67] = "Psychadelic", /* sic, the misspelling is used in the specification */
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

CDX_U32 moovGetLe16(unsigned char *s)
{
    CDX_U32 val;
    val = (CDX_U32)(*s);
    s += 1;
    val |= (CDX_U32)(*s) << 8;
    return val;
}

CDX_U32 MoovGetLe32(unsigned char *s)
{
    CDX_U32 val;
    val = moovGetLe16(s);
    s += 2;
    val |= moovGetLe16(s) << 16;
    return val;
}

CDX_S64 MoovGetLe64(unsigned char *s)
{
    CDX_S64 val;
    val = MoovGetLe32(s);
    s += 4;
    val |= (CDX_S64)MoovGetLe32(s) << 32;
    return val;
}

CDX_U32 moovGetBe16(unsigned char *s)
{
    CDX_U32 val;
    val = (CDX_U32)(*s) << 8;
    s += 1;
    val |= (CDX_U32)(*s);
    return val;
}

CDX_U32 MoovGetBe24(unsigned char *s)
{
    CDX_U32 val;
    val = moovGetBe16(s) << 8;
    s += 2;
    val |= (CDX_U32)(*s);
    return val;
}

CDX_U32 MoovGetBe32(unsigned char *s)
{
    CDX_U32 val;
    val = moovGetBe16(s) << 16;
    s += 2 ;
    val |= moovGetBe16(s);
    return val;
}

CDX_S64 MoovGetBe64(unsigned char *s)
{
    CDX_S64 val;
    val = MoovGetBe32(s);
    s += 4;
    val = val<<32;
    val = val | MoovGetBe32(s);

    return val;
}

/*
glbl - QuickTime is not contain this atom
*/
static int _MovParseGlbl(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];

    if (c->nb_streams < 1)
        return 0;

    if((CDX_U64)a.size > (1<<30))
        return -1;

    //LOGD("--- <%x%x%x%x>", buffer[offset], buffer[offset+1], buffer[offset+2], buffer[offset+3]);
    if(st->codec.extradata)
    {
        free(st->codec.extradata);
        st->codec.extradata = NULL;
    }

    st->codec.extradata = malloc(a.size -8);
    if (!st->codec.extradata)
        return -1;
    st->codec.extradataSize = a.size-8;
    memcpy(st->codec.extradata, buffer+offset, a.size-8);
    //CDX_LOGD("---  extradata_size = %d", st->codec.extradataSize);
    return 0;
}

static int mp4ParseDescr(MOVContext *c,int *tag, unsigned int *offset)
{
    unsigned char* buffer = c->moov_buffer;
    int len=0;
    int count = 4;

    *tag = buffer[(*offset)++];
    while (count--)
    {
        int ch = buffer[(*offset)++];
        len = (len << 7) | (ch & 0x7f);
        if (!(ch & 0x80))
        {
            break;
        }
    }
    return len;
}

/*
MPEG-4 Elementary Stream Descriptor Atom ('esds')
This atom is a required extension to the sound sample description for MPEG-4 audio.
This atom contains an elementary stream descriptor, which is defined in ISO/IEC FDIS 14496.
*/
static int _MovParseEsds(MOVContext *c,MOV_atom_t a)
{
//---- esds section
#define MP4ESDescrTag                   0x03
#define MP4DecConfigDescrTag            0x04
#define MP4DecSpecificDescrTag          0x05

    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];
    int tag, len = 0;

    offset += 4; /* version + flags */
    len = mp4ParseDescr(c, &tag, &(offset));
    //LOGD("edsd len = %x, tag = %x.", len, tag);

    if (tag == MP4ESDescrTag)
    {
        /* ID (2 bytes)*/
        /* priority  (1 bytes)*/
        offset += 3;
    }
    else
    {
        offset += 2; /* ID(2 bytes) */
    }

    len = mp4ParseDescr(c, &tag, &(offset));
    //LOGD("edsd len = %x, tag = %x.", len, tag);

    if (tag == MP4DecConfigDescrTag)
    {
        int object_type_id = buffer[offset ++];
        /* stream type (1byte)*/
        /* buffer size db (3bytes) */
        /* max bitrate (4 bytes)*/
        /* avg bitrate (4bytes)*/

        int max_bitrate_offset = 0,avg_bitrate_offset = 0;
        max_bitrate_offset = offset + 4;
        avg_bitrate_offset = max_bitrate_offset + 4;
        st->codec.max_bitrate = MoovGetBe32(buffer + max_bitrate_offset);
        st->codec.avg_bitrate = MoovGetBe32(buffer + avg_bitrate_offset);

        offset += 12;
        switch (object_type_id)
        {
            case 32:
            {
                st->eCodecFormat = VIDEO_CODEC_FORMAT_XVID;
                break;
            }
            case 33:
            {
                st->eCodecFormat = VIDEO_CODEC_FORMAT_H264;
                break;
            }
            case 108:
            {
                st->eCodecFormat = VIDEO_CODEC_FORMAT_MJPEG;//sorenson video3
                break;
            }
            case 0x60:
            case 0x61:
            case 0x62:
            case 0x63:
            case 0x64:
            case 0x65:
            case 0x6a: //mpeg1
            {
                st->eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
                st->eCodecFormat = (object_type_id == 0x6a ? VIDEO_CODEC_FORMAT_MPEG1 :
                                    VIDEO_CODEC_FORMAT_MPEG2);
                break;
            }
            case 64:
            case 102:
            case 103:
            case 104:
            {
                st->eCodecFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
                break;
            }
            case 105:
            case 107:  //0x6b
            {
                st->eCodecFormat = AUDIO_CODEC_FORMAT_MP3;
                break;
            }
            case 0xdd: //OGG
            {
                CDX_LOGW("it is ogg audio, not support");
                st->eCodecFormat = AUDIO_CODEC_FORMAT_UNKNOWN;
                break;
            }
        }

        len = mp4ParseDescr(c, &tag, &(offset));
        //LOGD("edsd len = %x, tag = %x.", len, tag);
        if (tag == MP4DecSpecificDescrTag)
        {
            if(len > 16384)
                return -1;
            st->codec.extradata = malloc(len);
            if (!st->codec.extradata)
                return -1;
            st->codec.extradataSize = len;
            memcpy(st->codec.extradata, buffer+offset, len);

            if(st->eCodecFormat == VIDEO_CODEC_FORMAT_MPEG4
                || st->eCodecFormat == VIDEO_CODEC_FORMAT_DIVX3
                || st->eCodecFormat == VIDEO_CODEC_FORMAT_DIVX4
                || st->eCodecFormat == VIDEO_CODEC_FORMAT_XVID)
            {
                int width=0,height=0;
                mov_getvolhdr((CDX_U8*)st->codec.extradata,len,&width,&height);
                if(width!=0 && height!=0)
                {
                    st->codec.width  = width;
                    st->codec.height = height;
                    CDX_LOGV("esds width = %d, height = %d", st->codec.width, st->codec.height);
                }
            }
        }
    }

    return 0;
}

/*
'wave'
*/
static int _MovParseWave(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = offset + a.size - 8;
    int err = 0;

    if(a.size > (1<<30))
        return -1;

    if (a.size > 16)
    {
        /* to read frma, esds atoms */
        while((offset < total_size) && !err)
        {
            a.size = MoovGetBe32(buffer+offset);
            offset += 4;
            a.type = MoovGetLe32(buffer+offset);
            offset += 4;
            if((a.size==0) || (a.type == 0))
            {
                CDX_LOGV("mov atom is end!");
                break;
            }

            if(a.size == 1)  /* 64 bit extended size */
            {
                a.size = (unsigned int)(MoovGetBe64(buffer+offset)-8);
                offset += 8;
            }

            if(a.type == MKTAG( 'f', 'r', 'm', 'a' ))
            {
                offset = offset + a.size - 8;
            }
            else if(a.type == MKTAG( 'e', 's', 'd', 's' ))
            {
                a.offset = offset;
                err = s_mov_stsd_parser[MESSAGE_ID_ESDS].movParserFunc(c,a);
                offset = offset + a.size - 8;
            }
            else
            {
                offset = offset + a.size - 8;
            }

        }
    }
    else
        offset += a.size-8;

    return 0;
}

static int _MovParseHvcc(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];

    if(st->codec.extradata){
        CDX_LOGW("mov extradata has been init???");
        free(st->codec.extradata);
    }

    st->codec.extradata = malloc(a.size-8);
    if(!st->codec.extradata)
        return -1;

    st->codec.extradataSize = a.size-8;

    memcpy(st->codec.extradata, buffer+offset, a.size-8);
    return 0;
}

/*
'avcC'
contain the avc extra_data ( sps and pps )
*/
static int _MovParseAvcc(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];

    if(st->codec.extradata){
    CDX_LOGW("mov extradata has been init???");
    free(st->codec.extradata);
    st->codec.extradata = NULL;
    }

    st->codec.extradata = malloc(a.size-8);
    if(!st->codec.extradata)
        return -1;
    st->codec.extradataSize = a.size - 8;

    CDX_LOGV("mov_read_avcc size:%d",a.size);
    memcpy(st->codec.extradata, buffer+offset, a.size-8);
    return 0;
}

/*
'sbgp' -  Sample-To-Group Atoms
find the group that a sample belongs to and the associated description
of that sample group
*/
static int _MovParseSbgp(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];
    unsigned int grouping_type;

    CDX_S32 entries;
    int i;

    int version = buffer[offset];
    offset += 4; /* version and flags */

    grouping_type = MoovGetLe32(buffer+offset);
    offset += 4;

    CDX_LOGV("========= grouping_type:0x%x", grouping_type);
    if(grouping_type != MKTAG('r', 'a', 'p', ' '))
        return 0;
    if(version == 1)
        offset += 4;  /* grouping_type_parameter */

    entries = MoovGetBe32(buffer+offset);
    offset += 4;
    if(entries == 0)
    {
        CDX_LOGW("---- sbgp entries is %d", entries);
    }

    if(entries >= MAXENTRY)
        return -1;

    st->rap_seek = malloc(entries*4);
    if(st->rap_seek == NULL)
        return -1;

    int sample_index = 0; // frame number
    int rap_goup_index = 0;
    for(i=0; i<entries; i++)
    {
        /* sample_count */
        sample_index +=  MoovGetBe32(buffer+offset);
        offset += 4;

        /* group_description_index */
        rap_goup_index = MoovGetBe32(buffer+offset);
        offset += 4;

        if(rap_goup_index > 0)
        {
            // it is a frame which can seek from here (not keyframe)
            st->rap_seek[st->rap_seek_count] = sample_index;
            st->rap_seek_count++;
        }
    }

    st->rap_group_count = i;
    CDX_LOGV("==== st->rap_group_count: %d, st->rap_seek_count: %d",
             st->rap_group_count, st->rap_seek_count);
    return 0;
}

// ctts - composition time ( CTS )
static int _MovParseCtts(MOVContext *c,MOV_atom_t a)
{
#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))

    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];
    CDX_U32 entries;

    CDX_S32 version = buffer[offset];
    if(version == 1)
    {
        CDX_LOGW("Composition to Decode Box(cslg) must be here, see qtff");
    }

    offset += 4; // version and flags
    entries = MoovGetBe32(buffer+offset);
    offset += 4;

    CDX_LOGV("track[%d].ctts.entries = %d", c->nb_streams-1, entries);
    if(!entries)
        return 0;
    if(entries >= MAXENTRY || a.size < entries)
        return -1;
    st->ctts_data = malloc(entries * sizeof(*st->ctts_data));
    if(!st->ctts_data)
        return -1;

    st->ctts_size = entries;
    st->ctts_offset = offset;

    unsigned int i;
    int count;
    int duration;
    for(i=0; i<entries; i++)
    {
        count = MoovGetBe32(buffer+offset);
        offset += 4;
        duration = MoovGetBe32(buffer+offset);
        offset += 4;

        st->ctts_data[i].count    = count;
        st->ctts_data[i].duration = duration;

        if((FFABS(duration)>(1<<28)) && (i+2<entries))
        {
            CDX_LOGW("CTTS invalid!");
            free(st->ctts_data);
            st->ctts_data = NULL;
            st->ctts_size = 0;
            return 0;
        }

        // dts_shift, see ffmpeg
        if(i+2< entries)
        {
            if(duration < 0)
            {
                st->dts_shift = (st->dts_shift>(-duration)) ? st->dts_shift : (-duration);
            }
        }
    }

    // in trun atom also have ctts, so we need to copy the ctts data to a new buffer
    return 0;
}

/*
'stco' -  Chunk Offset Atoms
Chunk offset atoms identify the location of each chunk of data in the media's data stream
*/
static int _MovParseStco(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];

    CDX_S32 entries;

    offset += 4; /* version and flags */

    entries = MoovGetBe32(buffer+offset);
    offset += 4;

    if(entries == 0)
    {
        CDX_LOGW("---- stco entries is %d, maybe an ISO base media file", entries);
    }

    if(entries >= MAXENTRY || (int)a.size < entries)//720000=25fps @ 8hour
        return -1;

    st->stco_size = entries;
    st->stco_offset = offset;

    if (a.type == MKTAG('c', 'o', '6', '4'))
    {
        st->co64 = 1;
    }

    return 0;
}

/*
'stsc' -   Sample-to-Chunk Atoms
The sample-to-chunk atom contains a table that maps
samples to chunks in the media data stream
*/
static int _MovParseStsc(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];
    CDX_S32 entries;

    offset += 4; /* version and flags */
    entries = MoovGetBe32(buffer+offset);
    offset += 4;

    if(entries == 0)
    {
        CDX_LOGW("---- stsc entries is %d, careful", entries);
    }
    if(entries >= MAXENTRY || (int)a.size < entries)//modify by lys UINT_MAX / sizeof(MOV_stsc_t))
        return -1;
    st->stsc_size = entries;
    st->stsc_offset = offset;

    if(st->eCodecFormat==AUDIO_CODEC_FORMAT_PCM && entries==1
        && !st->codec.extradataSize && st->codec.codecType==CODEC_TYPE_AUDIO)
    {
        MoovGetBe32(buffer+offset);
        offset += 4;
        // if will error,if add the code below
        //st->codec.extradataSize = MoovGetBe32(buffer+offset);
        offset += 4;
        CDX_LOGW("read pcm size from stsc! 0x%x", st->codec.extradataSize);
    }
    return 0;
}

/*
'stsz' - Sample Size Atoms
specify the size of each sample in the media
*/
static int _MovParseStsz(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    //int size = a.size - 8;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];

    CDX_S32 entries, sample_size;

    offset += 4; /* version and flags */

    //If all the samples are the same size, this field contains
    //that size value. If this field is set to 0, then the samples
    //have different sizes, and those sizes are
    //stored in the sample size table.
    sample_size = MoovGetBe32(buffer+offset);
    offset += 4;
    //modify by lys //if (!st->sample_size) /* do not overwrite value computed in stsd */
    st->sample_size = sample_size;
    CDX_LOGV("-- sample_size = %d", sample_size);
    entries = MoovGetBe32(buffer+offset);
    offset += 4;
    st->stsz_size = entries;
    st->stsz_offset = offset;
    return 0;
}

/*
'stss' - The sync sample atom identifies the key frames in the media.
the key frame is the sample id
*/
static int _MovParseStss(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];
    CDX_U32 entries;

    offset += 4; /* version and flags */
    entries = MoovGetBe32(buffer+offset);
    offset +=4;

    if(entries >= MAXENTRY)//720000=25fps @ 8hour I:P=1:3//modify by lys UINT_MAX / sizeof(CDX_S32))
        return -1;
    st->stss_size = entries;
    st->stss_offset = offset;
    return 0;
}

/*
Time-to-sample atoms store duration information for every media's samples, providing a mapping
from a time in a media to the corresponding data sample.
*/
static int _MovParseStts(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    int size = a.size - 8;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];
    CDX_U64 duration_total = 0;
    CDX_U64 sample_count_total = 0;
    CDX_U32 entries;

    offset += 4; /* version and flags */
    entries = MoovGetBe32(buffer+offset);
    offset += 4;
    size -= 8;

    if(entries == 0)
    {
        CDX_LOGW("---- stts entries is %d, careful", entries);
    }

    if(size <= 0)
    {
        return 0;
    }
    if(entries >= UINT_MAX/8)
        return -1;

    st->stts_size = entries;
    st->stts_offset = offset;

    // get the average sample duration to caculate the framerate of video
    if(st->codec.codecType == CODEC_TYPE_VIDEO)
    {
        for(int i = 0; i < entries; i++)
        {
            CDX_U64 sample_count = (CDX_U64)MoovGetBe32(buffer+offset);
            sample_count_total += sample_count;
            offset += 4;
            CDX_U64 sample_duration = (CDX_U64)MoovGetBe32(buffer+offset);
            duration_total += sample_count * sample_duration;
            offset += 4;
        }
        st->sample_duration = (CDX_U32)(duration_total / sample_count_total);
    }
    return 0;
}

static int _MovParseStsd(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    MOVStreamContext *st = c->streams[c->nb_streams-1];
    CDX_S32 entries;
    CDX_U32 format;
    CDX_S32 bits_per_sample = 0;
    CDX_S32 bytes_per_packet = 0;
    int remaind_size = 0;
    unsigned int total_size = 0;
    int err = 0;

    offset += 4;  /*version and  flags */

    entries = MoovGetBe32(buffer+offset); //stsd entries
    if(entries > 2)
    {
        logw("stsd entry: %d", entries);
        entries = 2;
    }
    offset += 4;

    //Parsing Sample description table
    while(entries--)
    {
        MOV_atom_t a = { 0, 0, 0 };
        CDX_U32 start_pos = offset;  // entry body start position
        CDX_U32 size = MoovGetBe32(buffer+offset);  /* size */
        offset +=4;
        format = MoovGetLe32(buffer+offset);        /* data format */
        //CDX_LOGD("---offset = %x, stsd format  = %c%c%c%c", offset, buffer[offset],
        //buffer[offset+1], buffer[offset+2], buffer[offset+3]);
        offset +=4;
        st->codec.codecTag = format;

        // cmov_kongfupanda.mov have two avc1 atom in stsd, but we only need one of them
        if(((st->codec.codecType==CODEC_TYPE_VIDEO)&&(st->stsd_type != 1))
           || ((st->codec.codecType==CODEC_TYPE_AUDIO &&(st->stsd_type != 2)))
           || (/*(st->codec.codecType == CODEC_TYPE_SUBTITLE )&&*/(st->stsd_type != 3)))
        {
            switch (st->codec.codecTag)
            {
                //parse video format type
                case MKTAG('m', 'p', '4', 'v'):
                case MKTAG('X', 'V', 'I', 'D'):
                case MKTAG('3', 'I', 'V', '2'):
                case MKTAG('D', 'X', '5', '0'):
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_XVID;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;
                case MKTAG('D', 'I', 'V', '3'):
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_DIVX3;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;
                case MKTAG('D', 'I', 'V', 'X'):// OpenDiVX
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_DIVX4;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;
                case MKTAG('a','v','c','1'):
                case MKTAG('A','V','C','1'):
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_H264;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;
                case MKTAG('h','v','c','1'):
                case MKTAG('H','V','C','L'):
                case MKTAG('h','e','v','1'):
                case MKTAG('h','v','c','2'):
                case MKTAG('h','v','t','1'):
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_H265;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;
                case MKTAG('s','2','6','3'):
                case MKTAG('h','2','6','3'):
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_H263;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;

                case MKTAG('J', 'P', 'E', 'G'):
                case MKTAG('j', 'p', 'e', 'g'):
                case MKTAG('m', 'j', 'p', 'a')://锟剿达拷锟斤拷锟斤拷要锟斤拷写
                case MKTAG('m', 'j', 'p', 'b'):
                case MKTAG('d', 'm', 'b', '1'):
                case MKTAG('A', 'V', 'D', 'J'):
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_MJPEG;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;
                case MKTAG('m', 'p', 'e', 'g'):
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_MPEG1;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;
                case MKTAG('h', 'd', 'v', '1') : /* HDV 720p30 */
                case MKTAG('h', 'd', 'v', '2') : /* MPEG2 produced by Sony HD camera */
                case MKTAG('h', 'd', 'v', '3') : /* HDV produced by FCP */
                case MKTAG('h', 'd', 'v', '5') : /* HDV 720p25 */
                case MKTAG('m', 'x', '5', 'n') : /* MPEG2 IMX NTSC 525/60 50mb/s produced by FCP */
                case MKTAG('m', 'x', '5', 'p') : /* MPEG2 IMX PAL 625/50 50mb/s produced by FCP */
                case MKTAG('m', 'x', '4', 'n') : /* MPEG2 IMX NTSC 525/60 40mb/s produced by FCP */
                case MKTAG('m', 'x', '4', 'p') : /* MPEG2 IMX PAL 625/50 40mb/s produced by FCP */
                case MKTAG('m', 'x', '3', 'n') : /* MPEG2 IMX NTSC 525/60 30mb/s produced by FCP */
                case MKTAG('m', 'x', '3', 'p') : /* MPEG2 IMX PAL 625/50 30mb/s produced by FCP */
                case MKTAG('x', 'd', 'v', '2') :  /* XDCAM HD 1080i60 */
                case MKTAG('A', 'V', 'm', 'p'):    /* AVID IMX PAL */
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;
                case MKTAG('v', 'c', '-', '1'):    /* AVID IMX PAL */
                CDX_LOGD(" the codec atom is vc-1, maybe the codec format is not right");
                    st->eCodecFormat = VIDEO_CODEC_FORMAT_WMV1;
                    st->stsd_type = 1;
                    st->stream_index = c->video_stream_num;
                    c->video_stream_num++;
                    break;

                case MKTAG('S', 'V', 'Q', '3'):
                    CDX_LOGD("---- can not support svq3");
                    st->stsd_type = 0;
                    st->unsurpoort = 1;
                    st->read_va_end = 1;
                    break;

                //parse audio format type
                case MKTAG('m','p','4','a'):
                case MKTAG('M','P','4','A'):
                case MKTAG('a','a','c',' '):
                case MKTAG('A','C','C',' '):
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;
                case MKTAG('a','l','a','c'):
                    st->eCodecFormat =  AUDIO_CODEC_FORMAT_ALAC;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;
                case MKTAG('m','s', 0 ,'U'):
                case MKTAG('m','s', 0 ,'P'):
                case MKTAG('.','m','p','3'):
                    CDX_LOGV("----- mp3");
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_MP3;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    CDX_LOGV("--- audio stream index = %d", c->audio_stream_num);
                    break;

                case MKTAG('.','m','p','2'):
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_MP2;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;

                //pcm type, adpcm/raw pcm/alaw pc/ulaw pcm/...
                case MKTAG('u','l','a','w'):    //u-law pcm
                case MKTAG('U','L','A','W'):    //u-law pcm
                    bits_per_sample = 8;
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = WAVE_FORMAT_MULAW | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;

                case MKTAG('a','l','a','w'):    //a-law pcm
                case MKTAG('A','L','A','W'):    //a-law pcm
                    bits_per_sample = 8;
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = WAVE_FORMAT_ALAW | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;

                case MKTAG('i', 'n', '3', '2'): //32bit signed, little ending
                case MKTAG('i', 'n', '2', '4'): //24bit signed, little ending
                case MKTAG('l', 'p', 'c', 'm'): //lpcm
                case MKTAG('N', 'O', 'N', 'E'): //uncompressed
                case MKTAG('r', 'a', 'w', ' '):    //raw pcm
                case MKTAG('R', 'A', 'W', ' '):    //raw pcm
                    bits_per_sample = 8;
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = WAVE_FORMAT_PCM | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;

               case MKTAG('i', 'm', 'a', '4'):  //IMA-4 ADPCM
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = WAVE_FORMAT_DVI_ADPCM | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;

                case MKTAG('s','o','w','t'):    //signed/two\'s complement little endian
                case MKTAG('S','O','W','T'):    //signed/two\'s complement little endian
                    bits_per_sample = 16;
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = WAVE_FORMAT_PCM | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;

                case MKTAG('t','w','o','s'):    //signed/two\'s complement big endian
                case MKTAG('T','W','O','S'):    //signed/two\'s complement big endian
                    bits_per_sample = 16;
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = WAVE_FORMAT_PCM | ABS_EDIAN_FLAG_BIG;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;

                case MKTAG('s','a','m','r'):    //amr narrow band
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_AMR;
                    st->eSubCodecFormat = AMR_FORMAT_NARROWBAND;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;
                case MKTAG('s','a','w','b'):    //amr wide band
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_AMR;
                    st->eSubCodecFormat = AMR_FORMAT_WIDEBAND;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;
                case MKTAG('W', 'M', 'A', '2') ://CODEC_ID_WMAV2
                    st->eCodecFormat =  AUDIO_CODEC_FORMAT_WMA_STANDARD;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;
                case MKTAG('A','C','-','3'):
                case MKTAG('E','C','-','3'):
                case MKTAG('a','c','-','3'):
                case MKTAG('e','c','-','3'):
                    st->eCodecFormat =  AUDIO_CODEC_FORMAT_AC3;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;
                case MKTAG('d','t','s','c'):
                    st->eCodecFormat =	AUDIO_CODEC_FORMAT_DTS;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    st->stream_index = c->audio_stream_num;
                    c->audio_stream_num++;
                    break;
                case 0x1100736d:
                    loge("unsupport this audio type");
                    st->unsurpoort = 1;
                    st->read_va_end = 1;
                    break;

                //parse subtitle format type
                case MKTAG('T', 'E', 'X', 'T'):
                case MKTAG('T', 'X', '3', 'G'):
                case MKTAG('t', 'e', 'x', 't'):
                case MKTAG('t', 'x', '3', 'g'):
                    CDX_LOGW(" subtitle is not define yet!!!!");
                    st->codec.codecType = CODEC_TYPE_SUBTITLE;  //for Jumanji.mp4, which 'hdlr'
                                                                //atom do not have 'text'
                    st->eCodecFormat = SUBTITLE_CODEC_TIMEDTEXT;        // LYRIC_TXT;
                    st->eSubCodecFormat = SUBTITLE_TEXT_FORMAT_UTF8;    //LYRIC_SUB_ENCODE_UTF8;
                    st->stsd_type = 3;
                    st->stream_index = c->subtitle_stream_num;
                    c->subtitle_stream_num++;
                    break;
                case MKTAG('m', 'p', '4', 's'):
                    if(st->codec.codecType == CODEC_TYPE_SUBTITLE)
                    {
                        st->eCodecFormat = SUBTITLE_CODEC_DVDSUB;
                        st->eSubCodecFormat = SUBTITLE_TEXT_FORMAT_UNKNOWN;
                        st->stsd_type = 3;
                        st->stream_index = c->subtitle_stream_num;
                        c->subtitle_stream_num++;
                    }
                    break;
                default:
                    CDX_LOGW("unknown format tag: <0x%x>", st->codec.codecTag);
                    if(st->stsd_type == 0) // for cmov_kongfupanda.mov
                    {
                        st->stsd_type = 0;
                        st->read_va_end = 1; // we do not need read samples of this stream,
                                             // so set the end flag
                    }
                    break;
            }
        }
        else if((st->codec.codecType!=CODEC_TYPE_VIDEO)
                && (st->codec.codecType!=CODEC_TYPE_AUDIO)
                && ((st->codec.codecType!=CODEC_TYPE_SUBTITLE)))
        {
            CDX_LOGW("unknown format tag: <%d>", st->codec.codecTag);
            st->stsd_type = 0;
            st->read_va_end = 1;   // we do not need read samples of this stream,
                                   //so set the end flag
        }

        if(st->codec.codecType==CODEC_TYPE_AUDIO && (format&0xFFFF) == 'm'+('s'<<8))//
        {
            format >>= 16;
            format = ((format&0xff)<<8) | ((format>>8)&0xff);
            switch(format)
            {
                case 0x02:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = ADPCM_CODEC_ID_MS | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                case 0x11:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = ADPCM_CODEC_ID_IMA_WAV | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                case 0x20:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = ADPCM_CODEC_ID_YAMAHA | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                case 0x6:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = WAVE_FORMAT_ALAW | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                case 0x7:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = WAVE_FORMAT_MULAW | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                case 0x45:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = WAVE_FORMAT_G726_ADPCM | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                case 0x50:
                case 0x55:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_MP3;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    break;
                case 0x61:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = ADPCM_CODEC_ID_IMA_DK4 | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                case 0x62:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = ADPCM_CODEC_ID_IMA_DK3 | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                case 0xFF:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    break;
                case 0x160:
                case 0x161:
                case 0x162:
                    st->eCodecFormat =  AUDIO_CODEC_FORMAT_WMA_STANDARD;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    break;
                case 0x200:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = ADPCM_CODEC_ID_CT | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                case 0x2000:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_AC3;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    break;
                case 0x2001:
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_DTS;
                    st->eSubCodecFormat = 0;
                    st->stsd_type = 2;
                    break;
                case (('S'<<8)+'F'):
                    st->eCodecFormat = AUDIO_CODEC_FORMAT_PCM;
                    st->eSubCodecFormat = ADPCM_CODEC_ID_SWF | ABS_EDIAN_FLAG_LITTLE;
                    st->stsd_type = 2;
                    break;
                default:
                    CDX_LOGW("unkown format <%x>", format);
                    st->stsd_type = 0;
                       break;
            }
        }

         /* reserved (6 bytes)*/
         /* index  (2 bytes)*/
        offset += 8;
        //parse the video sample description if it is a vedio trak
        if(st->codec.codecType == CODEC_TYPE_VIDEO)
        {
             /* version             (2 bytes)*/
             /* revision level         (2bytes) */
             /* vendor             (4 bytes)*/
             /* temporal quality         (4 bytes)*/
             /* spacial quality         (4 bytes)*/
            offset += 16;

            st->codec.width = moovGetBe16(buffer+offset); /* width */
            offset += 2;
            st->codec.height = moovGetBe16(buffer+offset); /* height */
            offset += 2;
            CDX_LOGV("stsd width = %d, height = %d", st->codec.width, st->codec.height);
            //c->ptr_fpsr->vFormat.nWidth = st->codec.width;
            //c->ptr_fpsr->vFormat.nHeight = st->codec.height;

            /* horiz resolution         (4 bytes)*/
            /* vert resolution         (4 bytes)*/
            /* data size, always 0     (4 bytes)*/
            /* frames per samples     (2 bytes)*/
            offset += 14;

            offset += 32;//skip codec_name
            st->codec.bitsPerSample = moovGetBe16(buffer+offset); /* depth */
            offset += 2;

#if 1
            //parser color table
            {
                int color_table_id = 0;
                int color_depth = 0;
                int color_greyscale = 0;

                color_table_id = moovGetBe16(buffer+offset); /* colortable id */
                offset += 2;
                /* figure out the palette situation */
                color_depth = st->codec.bitsPerSample & 0x1F;
                color_greyscale = st->codec.bitsPerSample & 0x20;

                /* if the depth is 2, 4, or 8 bpp, file is palettized */
                if ((color_depth == 2) || (color_depth == 4) ||
                    (color_depth == 8))
                {
                    /* for palette traversal */
                    unsigned int color_start, color_count, color_end;
                    unsigned char r, g, b;

                    if (color_greyscale) {

                    } else if (color_table_id) {

                    } else {
                        unsigned int j;
                        /* load the palette from the file */
                        color_start = MoovGetBe32(buffer+offset);
                        offset += 4;
                        color_count = moovGetBe16(buffer+offset);
                        offset += 2;
                        color_end   = color_count;  //no use, just for avoid compiler warning
                        color_end = moovGetBe16(buffer+offset);
                        offset += 2;

                        if ((color_start <= 255) &&
                            (color_end <= 255)) {
                            for (j = color_start; j <= color_end; j++) {
                            /* each R, G, or B component is 16 bits;
                            * only use the top 8 bits; skip alpha bytes
                                * up front */
                                offset += 2;
                                r = g; g= b; b= r;  //no use, just for avoid compiler warning
                                r = buffer[offset];
                                offset ++;
                                g = buffer[offset];
                                offset ++;
                                b = buffer[offset];
                                offset ++;
                            }
                        }
                    }
                }
            }
#endif
        }
        else if(st->codec.codecType==CODEC_TYPE_AUDIO)
        {
            CDX_U32 version = moovGetBe16(buffer+offset);
            offset +=2;

            /* revision level (2 bytes) */
            /* vendor (4 bytes)*/
            offset += 6;
            st->codec.channels = moovGetBe16(buffer+offset);    /* channel count */
            offset += 2;

            //Formats using more than 16 bits per sample set this field to 16 and use
            //sound version 1.
            st->codec.bitsPerSample = moovGetBe16(buffer+offset);  /* sample size (8 or 16)*/
            offset += 2;
            /* do we need to force to 16 for AMR ? */

            /* handle specific s8 codec */
            /* compression id = 0 (2bytes)*/
            /* packet size = 0 (2bytes)*/
            offset += 4;

            // sample rate
            st->codec.sampleRate = ((MoovGetBe32(buffer+offset) >> 16));
            offset += 4;

            //Read QT version 1 fields. In version 0 these do not exist.
            if(!c->isom)
            {
                if(version==1)
                {
                    st->samples_per_frame = MoovGetBe32(buffer+offset);
                    offset += 4;
                    //get_be32(pb); /* bytes per packet */
                    bytes_per_packet = MoovGetBe32(buffer+offset); /* bytes per packet */
                    offset += 4;
                    st->bytes_per_frame = MoovGetBe32(buffer+offset);
                    offset += 4;
                    if(st->bytes_per_frame && st->eCodecFormat==AUDIO_CODEC_FORMAT_PCM)
                    {
                        //what is the meaning of  the code below, it will error if PCM audio
                        //c->ptr_fpsr->aFormat.nCodecSpecificDataLen = st->bytes_per_frame;
                    }
                    offset += 4; /* bytes per sample */
                }
                else if(version==2)
                {
                    offset += 4; /* sizeof struct only */
                    {
                        /*st->codec.sample_rate = av_int2dbl(get_be64(pb));  float 64 */
                        union av_intfloat64{
                            long long i;
                            double f;
                        };
                        union av_intfloat64 v;
                        v.i = MoovGetBe64(buffer+offset);
                        offset += 8;

                        st->codec.sampleRate = v.f;
                    }
                    st->codec.channels = MoovGetBe32(buffer+offset);
                    offset += 4;
                     /* always 0x7F000000 */
                     /* bits per channel if sound is uncompressed */
                     /* lcpm format specific flag */
                     /* bytes per audio packet if constant */
                     /* lpcm frames per audio packet if constant */
                     offset += 20;
                }
            }

            if (bits_per_sample)
            {
                if(version == 0)
                {
                    st->codec.bitsPerSample = bits_per_sample;
                    c->audio_sample_size = (st->codec.bitsPerSample >> 3) * st->codec.channels;
                    st->audio_sample_size = c->audio_sample_size;
                }
                else if(version == 1)
                {
                    st->codec.bitsPerSample = (bytes_per_packet << 3);
                    if (st->eCodecFormat == AUDIO_CODEC_FORMAT_PCM)
                    {
                        st->codec.bitsPerSample = bits_per_sample;
                    }
                    c->audio_sample_size = bytes_per_packet * st->codec.channels;
                    st->audio_sample_size = c->audio_sample_size;
                }
                else
                {
                    if(version == 2 && st->codec.codecTag!=MKTAG('l', 'p', 'c', 'm'))
                    {
                        st->codec.bitsPerSample = bits_per_sample;
                    }
                    c->audio_sample_size = (bits_per_sample >> 3) * st->codec.channels;
                    st->audio_sample_size = c->audio_sample_size;
                }
            }
        }
        else if (st->codec.codecType == CODEC_TYPE_SUBTITLE)
        {
            // ttxt stsd contains display flags, justification, background
            // color, fonts, and default styles, so fake an atom to read it
            MOV_atom_t fake_atom = { 0, 0, 0 };
            fake_atom.size = size -(offset - start_pos);
            //LOGD("----------- subtitle  size = %d", fake_atom.size);
            if (format != MKTAG('m','p','4','s'))
            {
                CDX_LOGV("-------- subtitle glbl, format = %x, offset = %x, size = %d",
                    format, offset, fake_atom.size);
                fake_atom.offset = offset;
                s_mov_stsd_parser[MESSAGE_ID_GLBL].movParserFunc(c,fake_atom);
                st->codec.width = st->width;
                st->codec.height = st->height;
                offset += (fake_atom.size);
            }

        }
        else
        {
            /* other codec type, just skip the entry body (rtp, mp4s, tmcd ...) */
            CDX_LOGV(" We do not support this atom: <0x%x>", st->codec.codecType);
            return 0;
        }

        /* this will read extra atoms at the end (wave, alac, damr, avcC, SMI ...) */
        remaind_size = size - (offset - start_pos);
        total_size = offset + remaind_size;
        if (remaind_size > 8)
        {
            while( (offset < total_size) && !err )
            {
                remaind_size = size - (offset - start_pos);
                if(remaind_size < 8)
                {
                    offset += remaind_size;
                }
                a.size = MoovGetBe32(buffer+offset);
                offset += 4;
                a.type = MoovGetLe32(buffer+offset);
                offset += 4;

                if((a.size==0) || (a.type == 0))
                {
                    CDX_LOGV("mov atom is end!");
                    break;
                }

                if(a.size == 1)  /* 64 bit extended size */
                {
                    a.size = (unsigned int)(MoovGetBe64(buffer+offset)-8);
                    offset += 8;
                }

                if(a.type == MKTAG( 'a', 'v', 'c', 'C' ))
                {
                    a.offset = offset;
                    err = s_mov_stsd_parser[MESSAGE_ID_AVCC].movParserFunc(c,a);
                    offset = offset + a.size - 8;
                }
                else if(a.type == MKTAG( 'h', 'v', 'c', 'C' ))
                {
                    a.offset = offset;
                    err = s_mov_stsd_parser[MESSAGE_ID_HVCC].movParserFunc(c,a);
                    offset = offset + a.size - 8;
                }
                else if(a.type == MKTAG( 'a', 'l', 'a', 'c' ))
                {
                    a.offset = offset;
                    err = s_mov_stsd_parser[MESSAGE_ID_AVCC].movParserFunc(c,a);
                    offset = offset + a.size - 8;
                }
                else if(a.type == MKTAG( 'w', 'a', 'v', 'e' ))
                {
                    a.offset = offset;
                    err = s_mov_stsd_parser[MESSAGE_ID_WAVE].movParserFunc(c,a);
                    offset = offset + a.size - 8;
                }
                else if(a.type == MKTAG( 'e', 's', 'd', 's' ))
                {
                    a.offset = offset;
                    err = s_mov_stsd_parser[MESSAGE_ID_ESDS].movParserFunc(c,a);
                    offset = offset + a.size - 8;
                }
                else if(a.type == MKTAG( 'g', 'l', 'b', 'l' ))
                {
                    a.offset = offset;
                    err = s_mov_stsd_parser[MESSAGE_ID_GLBL].movParserFunc(c,a);
                    offset = offset + a.size - 8;
                }
                else if(a.type == MKTAG( 'S', 'M', 'I', ' ' ))
                {
                    //a.offset = offset;
                    //err = MovParseSvq3(c, a);
                    offset = offset + a.size - 8;
                }
                else
                {
                    offset = offset + a.size - 8;
                }
            }

        }
        else if (a.size > 0) // 0<size && size < 8
        {
            offset += a.size;
        }
    }

    if(st->codec.codecType==CODEC_TYPE_AUDIO && st->codec.sampleRate==0 && st->time_scale>1)
    {
        st->codec.sampleRate= st->time_scale;
    }
    return 0;
}

/*
'stbl' - The sample table atom contains information for converting from media
time to sample number to samplelocation. This atom also indicates how to
interpret the sample (for example, whether to decompress the video data and,
if so, how). This section describes the format and content of the sample table atom.
*/
static int _MovParseStbl(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = a.offset + a.size-8;   //size of 'trak' atom
    int err = 0;

    while( (offset < total_size) && !err )
    {
        a.size = MoovGetBe32(buffer+offset);
        offset += 4;
        a.type = MoovGetLe32(buffer+offset);
        offset += 4;
        if((a.size==0) || (a.type == 0))
        {
            break;
        }

        if(a.size == 1)  /* 64 bit extended size */
        {
            a.size = (unsigned int)(MoovGetBe64(buffer+offset)-8);
            offset += 8;
        }

        if(a.type == MKTAG( 's', 't', 's', 'd' ))
        {
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_STSD].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 's', 't', 't', 's' ))
        {
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_STTS].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 's', 't', 's', 's' ))
        {
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_STSS].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 's', 't', 's', 'z' ))
        {
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_STSZ].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 's', 't', 's', 'c' ))
        {
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_STSC].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 's', 't', 'c', 'o' ))
        {
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_STCO].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 'c', 't', 't', 's' ))
        {
            CDX_LOGV(" !!!! careful ctts atom is tested yet");
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_CTTS].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 'c', 'o', '6', '4' ))
        {
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_STCO].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 's', 'b', 'g', 'p' ))
        {
            CDX_LOGV("============ sbgp");
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_SBGP].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else
        {
            offset = offset + a.size - 8;
        }
    }
    return 0;
}

/*Media information atom
store handler-specific information for a track's media data.
The media handler uses this information to map from media time to media data
and to process the media data.

contain the following data elements:
size(4 bytes)
type(4 bytes)
'vmhd'-Viedo media information atom
'hdlr' - handler refrence atom
'dinf' - data information atom
'stbl' - sample table atom
*/
static int _MovParseMinf(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = a.offset + a.size-8;   //size of 'trak' atom
    int err = 0;

    while( (offset < total_size) && !err )
    {
        a.size = MoovGetBe32(buffer+offset);
        offset += 4;
        a.type = MoovGetLe32(buffer+offset);
        offset += 4;
        if((a.size==0) || (a.type == 0))
        {
            break;
        }

        if(a.size == 1)  /* 64 bit extended size */
        {
            a.size = (unsigned int)(MoovGetBe64(buffer+offset)-8);
            offset += 8;
        }

        if(a.type == MKTAG( 'h', 'd', 'l', 'r' ))
        {
            a.offset = offset;
            err = s_mov_mdia_parser[MESSAGE_ID_HDLR].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 's', 't', 'b', 'l' ))
        {
            a.offset = offset;
            err = s_mov_stbl_parser[MESSAGE_ID_STBL].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else
        {
            offset = offset + a.size - 8;
        }

    }

    return 0;
}

static int _MovParseHdlr(MOVContext *c,MOV_atom_t a)
{

    MOVStreamContext *st = c->streams[c->nb_streams-1];
    CDX_U32 type;

    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;

    //int err = 0;

    offset += 4;  /* version */ /* flags */

    offset += 4;   //component type
    type = MoovGetLe32(buffer+offset); /* component subtype */
    offset += 4;

    //!latest mplayer del it     if(!ctype)
    //!latest mplayer del it         c->isom = 1;

    if(type == MKTAG('v', 'i', 'd', 'e'))
    {
        st->codec.codecType = CODEC_TYPE_VIDEO;
        //c->video_stream_idx = c->nb_streams-1;
        c->has_video ++;
        c->ptr_fpsr->hasVideo ++;
        c->basetime[0] = st->basetime;    //auido basetime -/+
    }

    else if(type == MKTAG('s', 'o', 'u', 'n'))
    {
        if(c->has_audio < AUDIO_STREAM_LIMIT)//select the first audio track as default
        {
            st->codec.codecType = CODEC_TYPE_AUDIO;
            c->has_audio++;
            c->ptr_fpsr->hasAudio++;
        }
    }
    else if(type == MKTAG('t', 'e', 'x', 't') || type == MKTAG('s', 'u', 'b', 'p'))
    {
        if(c->has_subtitle < SUBTITLE_STREAM_LIMIT)
        { //select the first subtitle track as default
            st->codec.codecType = CODEC_TYPE_SUBTITLE;
            c->has_subtitle++;
            c->ptr_fpsr->hasSubTitle++;
        }
    }

    /* component  manufacture     (4 bytes)*/
    /* component flags                    (4 bytes)*/
    /* component flags mask         (4 bytes)*/
   /*   Companent name         (variable )  */
    return 0;
}

/* map numeric codes from mdhd atom to ISO 639 */
/* cf. QTFileFormat.pdf p253, qtff.pdf p205 */
/* http://developer.apple.com/documentation/mac/Text/Text-368.html */
/* deprecated by putting the code as 3*5bit ascii */
static const char mov_mdhd_language_map[][4] = {
    /* 0-9 */
    "eng", "fra", "ger", "ita", "dut", "sve", "spa", "dan", "por", "nor",
    "heb", "jpn", "ara", "fin", "gre", "ice", "mlt", "tur", "hr "/*scr*/, "chi"/*ace?*/,
    "urd", "hin", "tha", "kor", "lit", "pol", "hun", "est", "lav",    "",
    "fo ",    "", "rus", "chi",    "", "iri", "alb", "ron", "ces", "slk",
    "slv", "yid", "sr ", "mac", "bul", "ukr", "bel", "uzb", "kaz", "aze",
    /*?*/
    "aze", "arm", "geo", "mol", "kir", "tgk", "tuk", "mon",    "", "pus",
    "kur", "kas", "snd", "tib", "nep", "san", "mar", "ben", "asm", "guj",
    "pa ", "ori", "mal", "kan", "tam", "tel",    "", "bur", "khm", "lao",
    /*                   roman? arabic? */
    "vie", "ind", "tgl", "may", "may", "amh", "tir", "orm", "som", "swa",
    /*==rundi?*/
    "kin", "run", "nya", "mlg", "epo",    "",    "",    "",    "",    "",
    /* 100 */
       "",    "",    "",    "",    "",    "",    "",    "",    "",    "",
       "",    "",    "",    "",    "",    "",    "",    "",    "",    "",
       "",    "",    "",    "",    "",    "",    "",    "", "wel", "baq",
    "cat", "lat", "que", "grn", "aym", "tat", "uig", "dzo", "jav"
};

static int movLangToISO639(unsigned int code, char to[4])
{
#define ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

    int i;
    memset(to, 0, 4);

    if((code >= 0x400) && (code != 0x7fff))
    {
        for(i=2; i>=0; i--)
        {
            to[i] = 0x60 + (code & 0x1f);
            code >>= 5;
        }

        return i;
    }

    /* Macintosh Language Codecs */
    if(code >= ARRAY_ELEMS(mov_mdhd_language_map))
    {
        return 0;
    }
    if(!mov_mdhd_language_map[code][0])
    {
        return 0;
    }
    memcpy(to, mov_mdhd_language_map[code], 4);
    return 1;
}

/**************************************************************************************/
//* mdhd atom is in trak atom
//* specifies the characteristics of a media,(a stream)
//* including time scale and duration.
/**************************************************************************************/
static int _MovParseMdhd(MOVContext *c,MOV_atom_t a)
{

    MOVStreamContext *st = c->streams[c->nb_streams-1];
    unsigned char* buffer = c->moov_buffer;
    unsigned int offset = a.offset;

    CDX_S32 version = buffer[offset];
    CDX_S32 lang;

    if (version > 1)
        return 1;    /* unsupported */

    offset += 4;    /* version and flags */

    if (version == 1)
    {
        //MoovGetBe64(buffer+offset);
        offset += 8;
        //MoovGetBe64(buffer+offset);
        offset += 8;
    }
    else
    {
        //MoovGetBe32(buffer+offset);    /* creation time */
        offset += 4;
        //MoovGetBe32(buffer+offset);    /* modification time */
        offset += 4;
    }

    st->time_scale = MoovGetBe32(buffer+offset);
    offset += 4;
    st->duration = (version == 1) ? MoovGetBe64(buffer+offset) : MoovGetBe32(buffer+offset);
    /* duration */
    offset += (version == 1) ? 8 : 4;

    //st->totaltime = st->duration/st->time_scale;
    st->totaltime = (CDX_S32)((CDX_S64)st->duration*1000/st->time_scale);   //fuqiang
    st->basetime = st->time_offset/st->time_scale;

    //**< the language of this media
    lang = moovGetBe16(buffer+offset); /* language */
    offset += 2;

    char language[4] = {0};
    if(movLangToISO639(lang, language))
    {
       CDX_LOGV("-- language = %s", language);
       strcpy((char *)st->language, language);
    }
    //MoovGetBe16(buffer+offset); /* quality */
    offset += 2;

    return 0;
}

static int _MovParseMdia(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = a.offset + a.size - 8;   //size of 'trak' atom
    int err = 0;

    while( (offset < total_size) && !err )
    {
        a.size = MoovGetBe32(buffer+offset);
        offset += 4;
        a.type = MoovGetLe32(buffer+offset);
        offset += 4;
        if((a.size==0) || (a.type == 0))
        {
            //LOGV("mov atom is end!");
            break;
        }

        if(a.size == 1)  /* 64 bit extended size */
        {
            a.size = (unsigned int)(MoovGetBe64(buffer+offset)-8);
            offset += 8;
        }

        if(a.type == MKTAG( 'm', 'd', 'h', 'd' ))
        {
            a.offset = offset;
            err = s_mov_mdia_parser[MESSAGE_ID_MDHD].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 'h', 'd', 'l', 'r' ))
        {
            a.offset = offset;
            err = s_mov_mdia_parser[MESSAGE_ID_HDLR].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 'm', 'i', 'n', 'f' ))
        {
            a.offset = offset;
            err = s_mov_mdia_parser[MESSAGE_ID_MINF].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else
        {
            offset = offset + a.size - 8;
        }

    }
    return 0;
}

static int _MovParseElst(MOVContext *c,MOV_atom_t a)
{
    MOVStreamContext *st = c->streams[c->nb_streams-1];
    unsigned char* buffer = c->moov_buffer;
    unsigned int offset = a.offset;

    CDX_S32 i, entries;

    offset += 4; /* version */ /* flags */

    entries = MoovGetBe32(buffer+offset);
    offset += 4;
    if((CDX_U64)entries*12+8 > a.size)  //CDX_U64
        return -1;

    for(i=0; i<entries; i++)
    {
        CDX_S32 time;
        CDX_S32 duration = MoovGetBe32(buffer+offset); /* Track duration */
        offset += 4;
        time = MoovGetBe32(buffer+offset); /* Media time */
        offset += 4;
        //MoovGetBe32(pb); /* Media rate */
        offset += 4;
        if (i == 0 && time >= -1)
        {
            st->time_offset = time != -1 ? time : -duration;
        }
    }
    return 0;

}

static int _MovParseTkhd(MOVContext *c,MOV_atom_t a)
{
    CDX_S32 i;
    CDX_S32 width;
    CDX_S32 height;
    //CDX_S64 disp_transform[2];
    CDX_S32 display_matrix[3][2];
    MOVStreamContext *st = c->streams[c->nb_streams-1];
    unsigned char* buffer = c->moov_buffer;
    unsigned int offset = a.offset;
    CDX_S32 version = buffer[offset];

    offset += 4;        /* flags */

    if (version == 1) {
        MoovGetBe64(buffer+offset);
        offset += 8;
        MoovGetBe64(buffer+offset);
        offset += 8;
    } else {
        MoovGetBe32(buffer+offset);    /* creation time */
        offset += 4;
        MoovGetBe32(buffer+offset);    /* modification time */
        offset += 4;
    }

    st->id = (CDX_S32)MoovGetBe32(buffer+offset); /* track id (NOT 0 !)*/
    offset += 4;
    MoovGetBe32(buffer+offset);        /* reserved */
    offset += 4;
    //st->start_time = 0; /* check */
    (version == 1) ? MoovGetBe64(buffer+offset) : MoovGetBe32(buffer+offset);
    /* highlevel (considering edits) duration in movie timebase */
    offset += (version == 1) ? 8 : 4;

    //MoovGetBe32(buffer+offset);        /* reserved */
    offset += 4;
    //MoovGetBe32(buffer+offset);        /* reserved */
    offset += 4;

    offset += 8;
     /* layer (2bytes)*/
     /* alternate group (2bytes)*/
     /* volume (2bytes)*/
    /* reserved (2bytes)*/

    //read in the display matrix (outlined in ISO 14496-12, Section 6.2.2)
    // they're kept in fixed point format through all calculations
    // ignore u,v,z b/c we don't need the scale factor to calc aspect ratio
    //                       | a     b      u |
    //  (x, y, 1)   *   | c     d      v |   = (x', y', 1)
    //                       | tx   ty    w |
    for (i = 0; i < 3; i++) {
        display_matrix[i][0] = MoovGetBe32(buffer+offset);   // 16.16 fixed point
        offset += 4;
        display_matrix[i][1] = MoovGetBe32(buffer+offset);   // 16.16 fixed point
        offset += 4;
        MoovGetBe32(buffer+offset);           // 2.30 fixed point (not used)
        offset += 4;
    }

    { //below code are used for android
        CDX_U32 rotationDegrees;
        CDX_S32 a00 = display_matrix[0][0];
        CDX_S32 a01 = display_matrix[0][1];
        CDX_S32 a10 = display_matrix[1][0];
        CDX_S32 a11 = display_matrix[1][1];

        static const CDX_S32 kFixedOne = 0x10000;
        if (a00 == kFixedOne && a01 == 0 && a10 == 0 && a11 == kFixedOne) {
            // Identity, no rotation
            rotationDegrees = 0;
            strcpy((char*)st->rotate, "0");
        } else if (a00 == 0 && a01 == kFixedOne && a10 == -kFixedOne && a11 == 0) {
            rotationDegrees = 90;
             strcpy((char*)st->rotate, "90");
        } else if (a00 == 0 && a01 == -kFixedOne && a10 == kFixedOne && a11 == 0) {
            rotationDegrees = 270;
             strcpy((char*)st->rotate, "270");
        } else if (a00 == -kFixedOne && a01 == 0 && a10 == 0 && a11 == -kFixedOne) {
            rotationDegrees = 180;
             strcpy((char*)st->rotate, "180");
        } else {
            CDX_LOGW("We only support 0,90,180,270 degree rotation matrices");
             strcpy((char*)st->rotate, "0");
            rotationDegrees = 0;
        }

        if (rotationDegrees != 0) {
            CDX_LOGD("nRotation variable is not in vFormat");
            //c->ptr_fpsr->vFormat.nRotation = rotationDegrees;
        }
    }

    width = MoovGetBe32(buffer + offset);       // 16.16 fixed point track width
    offset += 4;
    height = MoovGetBe32(buffer+offset);      // 16.16 fixed point track height
    offset += 4;
    st->width = width >> 16;
    st->height = height >> 16;
    CDX_LOGV("tkhd width = %d, height = %d", st->width, st->height);
    // transform the display width/height according to the matrix
    // skip this if the display matrix is the default identity matrix
    // or if it is rotating the picture, ex iPhone 3GS
    // to keep the same scale, use [width height 1<<16]

    return 0;
}

/*
*      in the moov atom, if the trex atom is exist, the mp4 file is a fragment mp4
*      the duration from mvhd is not representing the whole file
*      @ set up default values used by movie fragment
*/
static int _MovParseTrex(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    //unsigned int total_size = a.offset + a.size - 8;   //size of 'trak' atom

    if(a.size < 32)
    {
        CDX_LOGI("warning: trex box <%d> is less than 32 bytes !", a.size);
        return 0;
    }

    MOVTrackExt* trex;
    trex = realloc(c->trex_data, (c->trex_num+1)*sizeof(*c->trex_data));
    if(!trex) return -1;
    c->trex_data = trex;

    trex = &c->trex_data[c->trex_num++];
    MoovGetBe32(buffer+offset); // version(1byte) and flag( 3bytes )
    offset += 4;

    trex->track_id = MoovGetBe32(buffer+offset);
    offset += 4;
    trex->stsd_id  = MoovGetBe32(buffer+offset);
    offset += 4;
    trex->duration = MoovGetBe32(buffer+offset);
    offset += 4;
    trex->size     = MoovGetBe32(buffer+offset);
    offset += 4;
    trex->flags    = MoovGetBe32(buffer+offset);

    //LOGD("%x, %x, %x, %x, %x",trex->track_id, trex->stsd_id, trex->duration,
    //trex->size, trex->flags );
    return 0;
}

static int _MovParseCmov(MOVContext *c,MOV_atom_t a)
{
    uint8_t *cmov_data;
    uint8_t *moov_data; /* uncompressed data */
    long cmov_len, moov_data_len, dcom_len, cmvd_len;
    int ret = -1;
    int offset = 0;
    //unsigned int type;

    /* cmov */
    cmov_data = c->moov_buffer + a.offset; /* exclude cmov atom header */
    cmov_len = a.size;

    /* dcom */
    dcom_len = MoovGetBe32(cmov_data + offset);
    if (dcom_len != 12)
    {
        CDX_LOGE("invalid data, <%ld>", dcom_len);
        return -1;
    };
    offset += 4;
    if (MoovGetLe32(cmov_data + offset) != MKTAG('d', 'c', 'o', 'm'))
    {
        CDX_LOGE("invalid data");
        return -1;
    };
    offset += 4;
    if (MoovGetLe32(cmov_data + offset) != MKTAG('z', 'l', 'i', 'b'))
    {
        CDX_LOGE("invalid data");
        return -1;
    };
    offset += 4;

    /* cmvd */
    cmvd_len = MoovGetBe32(cmov_data + offset);
    offset += 4;
    if (MoovGetLe32(cmov_data + offset) != MKTAG('c', 'm', 'v', 'd'))
    {
        CDX_LOGE("invalid data");
        return -1;
    };
    offset += 4;

    moov_data_len = MoovGetBe32(cmov_data + offset); /* uncompressed size */
    offset += 4;

    moov_data = malloc(moov_data_len);

    if (uncompress(moov_data, (uLongf *)&moov_data_len,
            (const Bytef *)cmov_data + offset, cmov_len - offset) != Z_OK)
    {
        CDX_LOGE("uncompress cmov data fail.");
        free(moov_data);
        return -1;
    }

    free(c->moov_buffer);
    c->moov_buffer = moov_data;
    c->moov_size = moov_data_len;
    //FILE* fp = fopen("/mnt/sdcard/cmov.txt", "wb");
    //fwrite(c->moov_buffer, 1, moov_data_len, fp);
    //fclose(fp);

    ret = g_mov_top_parser[MESSAGE_ID_MOOV].movParserFunc(c,c->moov_buffer,a);
    return ret;
}

/********************************************************/
//only present in ISO media file format ( DASH )
// in 'moov' atom, set the default infomation of these segments
/****************************************************/
static int _MovParseMvex(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = a.offset + a.size - 8;   //size of 'trak' atom
    int err = 0;

    while( (offset < total_size) && !err )
    {
        a.size = MoovGetBe32(buffer+offset);
        offset += 4;
        a.type = MoovGetLe32(buffer+offset);
        offset += 4;

        if(a.type == MKTAG( 't', 'r', 'e', 'x' ))
        {
            a.offset = offset;
            err = s_mov_moov_parser[MESSAGE_ID_TREX].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else
        {
            offset = offset + a.size - 8;
        }

    }
    return err;
}

static int _MovParseTrak(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = a.offset + a.size - 8;   //size of 'trak' atom
    int err = 0;

    while( (offset < total_size) && !err )
    {
        a.size = MoovGetBe32(buffer+offset);
        if(offset+a.size > total_size)
        {
            logw("atom size error: %x", a.size);
            a.size = total_size - offset;
        }
        offset += 4;
        a.type = MoovGetLe32(buffer+offset);
        offset += 4;

        if((a.size==0) || (a.type == 0))
        {
            //LOGV("mov atom is end!");
            break;
        }

        if(a.size == 1)  /* 64 bit extended size */
        {
            a.size = (unsigned int)(MoovGetBe64(buffer+offset)-8);
            offset += 8;
        }


        if(a.type == MKTAG( 't', 'k', 'h', 'd' ))
        {
            a.offset = offset;
            err = s_mov_trak_parser[MESSAGE_ID_TKHD].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 'e', 'l', 's', 't' ))
        {
            a.offset = offset;
            err = s_mov_trak_parser[MESSAGE_ID_ELST].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 'm', 'd', 'i', 'a' ))
        {
            a.offset = offset;
            err = s_mov_trak_parser[MESSAGE_ID_MDIA].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else
        {
            offset = offset + a.size - 8;
        }

    }
    return 0;
}

static void movParseGnre(unsigned char *buffer, CDX_U8 *str, int size)
{
    int genre;
    unsigned int offset = 0;
    offset ++; // unkown

    genre = buffer[offset];
    CDX_LOGV("--- genre %s", ff_id3v1_genre_str[genre-1]);
    snprintf((char*)str, size, "%s", ff_id3v1_genre_str[genre-1]);
}

static int _MovParseKeys(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = a.offset + a.size - 8;   //size of 'keys' atom

    int mdta_index = 0;
    c->android_capture_fps = 0;

    offset += 4;
    /*mdta nums*/
    offset += 4;

    while (offset < total_size)
    {

        a.size   = MoovGetBe32(buffer+offset);
        offset  += 4;
        a.type   = MoovGetLe32(buffer+offset);
        offset  += 4;

        if(a.type == MKTAG( 'm', 'd', 't', 'a' ))
        {
            memcpy(c->extend_android_mdta[mdta_index].android_mdta,buffer+offset,
                a.size - 8);
            c->extend_android_mdta[mdta_index].index = mdta_index;
            mdta_index ++;
            offset = offset + a.size - 8;
            if(mdta_index >= MAX_MDTA_NUMS)
            {
                loge("parse mdta nums too big,beyond the scope.");
                return -1;
            }
        }
        else
        {
            offset = offset + a.size - 8;
        }

    }
    return 0;
}

static int _MovParseMeta(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = a.offset + a.size - 8;   //size of 'trak' atom
    int err = 0;
    int meta_pos_flag = 0;

    memset(c->extend_android_mdta,0,sizeof(ExtendAndroidMdta)*MAX_MDTA_NUMS);

    a.size = MoovGetBe32(buffer+offset);
    if(a.size <=0 || a.size > total_size)
    {
        offset += 4;
        meta_pos_flag = 1;
    }

    while (offset < total_size && !err )
    {
        if(meta_pos_flag)
        {
            a.size = MoovGetBe32(buffer+offset);
        }
        meta_pos_flag = 1;
        offset += 4;
        a.type = MoovGetLe32(buffer+offset);
        offset += 4;

        if(a.type == MKTAG( 'i', 'l', 's', 't' ))
        {
            a.offset = offset;
            err = s_mov_moov_parser[MESSAGE_ID_UDTA].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }else if(a.type == MKTAG( 'I', 'D', '3', '2' ))
        {
            a.size -= 8;
            offset += 6;/* 'I''D''3''2' + 6 has the true id3v2 infomation*/
            a.size -= 6;
            a.offset = offset;
            CDX_LOGV("Mov id3 offset : %d, id3 size : %d", a.offset, a.size);
            c->id3v2 = GenerateId3(c->fp, c->moov_buffer + a.offset , a.size, ktrue);
            offset = a.offset + a.size;
        }
        else if(a.type == MKTAG( 'k', 'e', 'y', 's' ))
        {
            a.offset = offset;
            err = s_mov_moov_parser[MESSAGE_ID_KEYS].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else
        {
            offset = offset + a.size - 8;
        }
    }
    return 0;
}

static void movParseUdtaString2(unsigned char *buffer, CDX_U8 *str, int size)
{
    unsigned int offset = 0;
    CDX_U16 str_size = moovGetBe16(buffer+offset); /* string length */;

    offset += 4; /* skip language */

    memcpy(str, buffer+offset, MIN(size, str_size));
}

typedef union
{
    float ul_Temp;
    unsigned char uc_Buf[4];
}un_DtformConver;

static float uintToFloat(unsigned int framerate_val)
{
    float val_float;
    int i;
    un_DtformConver DtformConver;
    memset((unsigned char *)&DtformConver.uc_Buf[0],0,4);
    for(i=0;i<4;i++)
    {
        DtformConver.uc_Buf[i] = (unsigned char)(framerate_val>>(i*8));
    }
    val_float = DtformConver.ul_Temp;
    return val_float;
}

/* user data atom */
static int _MovParseUdta(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = a.offset + a.size - 8;   //size of 'trak' atom
    int err = 0;

    int length;
    unsigned int next;
    unsigned int data_offset;
    int android_capture_fps_flag = 0,i;
    unsigned int data_type;
    unsigned int framerate_val;
    for(i = 0; i < MAX_MDTA_NUMS; i++)
    {
        if(!strncmp((char*)c->extend_android_mdta[i].android_mdta,
            "com.android.capture.fps",23))
        {
            android_capture_fps_flag = 1;
            offset += 8;
            break;
        }
    }

    while (offset < total_size)
    {
        a.size   = MoovGetBe32(buffer+offset);
        offset  += 4;
        a.type   = MoovGetLe32(buffer+offset);
        offset  += 4;
        next     = a.size - 8;

        if (next > total_size) // stop if tag_size is wrong
            break;

        switch (a.type)
        {
        case MKTAG('m', 'e', 't', 'a'):
            a.offset = offset;
            err = s_mov_moov_parser[MESSAGE_ID_META].movParserFunc(c,a);
            break;

        case MKTAG(0xA9,'n','a','m'):
            length = a.size-8-4-1;
            movParseUdtaString2(buffer+offset, c->title, sizeof(c->title));
            length = MIN(length, 31);
            c->title[length] = '\0';
            break;
        case MKTAG(0xA9,'w','r','t'):
            length = a.size-8-4-1;
            movParseUdtaString2(buffer+offset, c->writer, sizeof(c->writer));
            length = MIN(length, 31);
            c->writer[length] = '\0';
            break;

        case MKTAG(0xA9,'a','u','t'):
        case MKTAG(0xA9,'A','R','T'):
            length = a.size-8-4-1;
            movParseUdtaString2(buffer+offset, c->artist, sizeof(c->artist));
            length = MIN(length, 31);
            c->artist[length] = '\0';
            break;

        case MKTAG(0xA9,'d','a','y'):
            length = a.size-8-4-1;
            movParseUdtaString2(buffer+offset, c->date, sizeof(c->date));
            length = MIN(length, 31);
            c->date[length] = '\0';
            break;
        case MKTAG(0xA9,'a','l','b'):
            length = a.size-8-4-1;
            movParseUdtaString2(buffer+offset, c->album, sizeof(c->album));
            length = MIN(length, 31);
            c->album[length] = '\0';
            break;
        case MKTAG('a','A','R','T'):
            length = a.size-8-4-1;
            movParseUdtaString2(buffer+offset, c->albumArtistic, sizeof(c->albumArtistic));
            length = MIN(length, 31);
            c->albumArtistic[length] = '\0';
            break;
        case MKTAG(0xA9,'g','e','n'):
            length = a.size-8-4-1;
            movParseUdtaString2(buffer+offset, c->genre, sizeof(c->genre));
            length = MIN(length, 31);
            c->genre[length] = '\0';
            break;
        case MKTAG('g','n','r','e'):
            //length = a.size-8-4-1;
            CDX_LOGV("---- gnre, care");
            movParseGnre(buffer+offset, c->genre, sizeof(c->genre));
            //length = MIN(length, 31);
            //c->genre[length] = '\0';
            break;
//        case MKTAG(0xa9,'c','p','y'):
//            mov_parse_udta_string(pb, c->fc->copyright, sizeof(c->fc->copyright));
//            break;
//        case MKTAG(0xa9,'i','n','f'):
//            mov_parse_udta_string(pb, c->fc->comment,   sizeof(c->fc->comment));
//            break;
        case MKTAG(0xA9, 'x', 'y', 'z'):
            // it is the geometry position of user, which is very important in a CTS test
            length = a.size-8-4;
            movParseUdtaString2(buffer+offset, c->location, sizeof(c->location));
            length = MIN(length,  (int)sizeof(c->location)-1);
            c->location[length] = '\0';
            break;
        case MKTAG('d', 'a', 't', 'a'):
            data_offset = offset;
            data_type = MoovGetBe32(buffer+data_offset);
            if(data_type == 23 && android_capture_fps_flag)
            {
                data_offset += 8;
                framerate_val = MoovGetBe32(buffer+data_offset);
                if(framerate_val)
                {
                    c->android_capture_fps = uintToFloat(framerate_val);
                }
            }
            else
            {
               next += 8;
            }

            break;
        default:
            break;
        }

        offset += next;
    }

    return 0;
}

static cdx_bool movConvertTimeToDate(cdx_int64 time_1904, cdx_uint8 *dst)
{
    // delta between mpeg4 time and unix epoch time
    static const cdx_int64 delta = (((66 * 365 + 17) * 24) * 3600);
    time_t time_1970 = time_1904 - delta;
    char tmp[32] = {0};
    struct tm* tm = gmtime(&time_1970);

    if (time_1904 < INT64_MIN + delta) {
        return CDX_FALSE;
    }
    if (tm != NULL &&
            strftime(tmp, sizeof(tmp), "%Y%m%dT%H%M%S.000Z", tm) > 0) {
        memcpy(dst, tmp, strlen(tmp));
        return CDX_TRUE;
    }
    return CDX_FALSE;
}

/********************************************************************************/
//* specify the characteristics of an entire QuickTime movie.
//*  defines characteristics of the entire QuickTime movie,
//* such as time scale and duration.
//* in DASH, the duration maybe 0 here
/********************************************************************************/
static int _MovParseMvhd(MOVContext *c,MOV_atom_t a)
{
    unsigned int offset = a.offset;
    unsigned char* buffer = c->moov_buffer;
    unsigned int total_size = a.offset + a.size - 8;   //size of 'trak' atom
    cdx_uint64   date = 0;
    int err = 0;

    while( (offset < total_size) && !err )
    {
        CDX_S32 version = buffer[offset]; /* version */
        offset += 4; /* flags */
        //MoovGetBe64(buffer + offset);

        if (version == 1)
        {
            date = MoovGetBe64(buffer+offset);
            //MoovGetBe64(buffer + offset);
            offset += 8;
            //MoovGetBe64(buffer + offset);
            offset += 8;
        }
        else if(version == 0)
        {
            date = MoovGetBe32(buffer+offset);
            //MoovGetBe32(buffer+offset); /* creation time */
            offset += 4;
            //MoovGetBe32(buffer+offset); /* modification time */
            offset += 4;
        }
        else
        {
            CDX_LOGW("version<%d> is not support!", version);
        }

        c->time_scale = MoovGetBe32(buffer+offset); /* time scale */
        offset += 4;

        CDX_U64 duration = 0;
        if(version == 1)
        {
            duration = MoovGetBe64(buffer+offset);
            c->duration = (CDX_S32)duration;
            offset += 8;
        }
        else
        {
            duration = MoovGetBe32(buffer+offset);
            c->duration = (CDX_S32)duration;
            offset += 4;
        }
        if(c->duration != 0 && c->time_scale)
        {
            c->mvhd_total_time = duration*1000 / c->time_scale;
        }
        //LOGD("duration = %llx, timescale = %x", duration, c->time_scale);
        //LOGD("mvhd duration = %d", c->mvhd_total_time);
        movConvertTimeToDate(date, c->date);
        offset = offset +6+10+36+28;
    }
    return err;
}

/*
 *  'uuid'  (Sample Encryption Box)
 */
static int _MovParseSenc(MOVContext *s,MOV_atom_t atom)
{
    unsigned char buffer[20] = {0};
    int ret = 0;
    int flags;
    unsigned int sample_count;
    int i, j;

    CDX_LOGV("MovParseSenc,atom.size:%d",atom.size);

    ret = CdxStreamRead(s->fp, buffer, 16); // a2394f52-5a9b-4f14-a244-6c427c648df4
    CDX_LOGV("type=%2.2x%2.2x%2.2x%2.2x-%2.2x%2.2x-%2.2x%2.2x-%2.2x%2.2x-"
        "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
        buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7],
        buffer[8], buffer[9], buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15]);

    ret = CdxStreamRead(s->fp, buffer, 4);
    //version  ( 1 byte )
    flags = MoovGetBe24(buffer+1);

    if (flags & 0x1) {
        ret = CdxStreamRead(s->fp, buffer, 20); // AlgorithmID, iv_size, KID
    }

    ret = CdxStreamRead(s->fp, buffer, 4);
    sample_count = MoovGetBe32(buffer); //sample count
    CDX_LOGV("sample_count=%d", sample_count);

    if (s->senc_data)
    {
        free(s->senc_data);
        s->senc_data = NULL;
    }
    s->senc_data = malloc(sample_count*sizeof(MOVSampleEnc));
    if (s->senc_data == NULL) {
        CDX_LOGE("malloc fail.");
        return -1;
    }
    memset(s->senc_data, 0, sample_count*sizeof(MOVSampleEnc));

    for (i = 0; i < (int)sample_count; ++i) {
        ret = CdxStreamRead(s->fp, buffer, 8);
        s->senc_data[i].iv = MoovGetLe64(buffer);
        if (flags & 0x2) {
            unsigned int entries;
            ret = CdxStreamRead(s->fp, buffer, 2);
            entries = moovGetBe16(buffer);
            if (entries > 16) {
                CDX_LOGE("region too large.");
                return -1;
            }
            s->senc_data[i].region_count = entries*2;
            for (j = 0; j < (int)entries; ++j) {
                ret = CdxStreamRead(s->fp, buffer, 6);
                if (j < 16) {
                    s->senc_data[i].regions[j*2] = moovGetBe16(buffer); // bytes_of_clear
                    s->senc_data[i].regions[j*2+1] = MoovGetBe32(buffer+2); //bytes_of_encrypted
                }
            }
        }
    }
    return 0;
}

/*
*    'trun'  ( Track Fragment Run Box )
*         if the duration_is_empty flag is set in the tf_flag of 'tfhd' box,
*         so need a track run document
*         @ choose the audio stream or video stream by the track_id of trun
*/
static int _MovParseTrun(MOVContext *s,MOV_atom_t atom)
{
    int size = atom.size - 8;
    unsigned char buffer[8] = {0};
    int ret = 0;
    MOVFragment* frag = &s->fragment;
    MOVStreamContext* st = NULL;
    MOVCtts *ctts_data;
    int flags;
    CDX_U32 data_offset_delta = 0;
    unsigned int first_sample_flags = frag->flags;
    int found_keyframe = 0;

    int i;
    int entries;

    //find the corresponding track stream
    for(i=0; i<s->nb_streams; i++)
    {
        // streams->id is the stream track_id, set in 'tkhd' atom in every 'trak'
        if(s->streams[i]->id == (int)frag->track_id)
        {
            st = s->streams[i];
            break;
        }
    }

    if(s->bSmsSegment)
    {
        st = s->streams[0];
        i = 0;
    }

    if(!st)
    {
        CDX_LOGW(" could not find corresponding track id for trun!");
        return -1;
    }

    // select the approparite stream when bandwidth changed
    if(st->codec.codecType == CODEC_TYPE_VIDEO)
    {
        s->video_stream_idx = i;
    }
    else if(st->codec.codecType == CODEC_TYPE_AUDIO)
    {
        s->audio_stream_idx = i;
    }

    ret = CdxStreamRead(s->fp, buffer, 8);
    //version  ( 1 byte )
    flags = MoovGetBe24(buffer+1);  //tr_flags 3 bytes
    entries = MoovGetBe32(buffer+4); //sample count

    size -= 8;

    /* Always assume the presence of composition time offsets.
     * Without this assumption, for instance, we cannot deal with a track in fragmented movies
     * that meet the following.
     *  1) in the initial movie, there are no samples.
     *  2) in the first movie fragment, there is only one sample without composition time offset.
     *  3) in the subsequent movie fragments, there are samples with composition time offset. */

    // if the stsz have sample size, add it to ctts
    if(!st->ctts_size && st->sample_size)
    {
        /* Complement ctts table if moov atom doesn't have ctts atom. */
        ctts_data = malloc(sizeof(*st->ctts_data));
        if(!ctts_data) return -1;

        st->ctts_data = ctts_data;
        st->ctts_data[st->ctts_size].count = st->sample_size;
        st->ctts_data[st->ctts_size].duration = 0;
        st->ctts_size ++;
    }

    ctts_data = realloc(st->ctts_data, (entries+st->ctts_size)*sizeof(*st->ctts_data));
    if(!ctts_data) return -1;

    st->ctts_data = ctts_data;

    // data offset is the relative offset  to the 'moof' box,
    // so the first sample offset in the file is caculated by ( base_data_offset + Offset_moof
    // + data_offset )
    if(flags & CDX_MOV_TRUN_DATA_OFFSET)
    {
        if(size < 4)
        {
            return -1;
        }
        ret = CdxStreamRead(s->fp, buffer, 4);
        data_offset_delta = MoovGetBe32(buffer);
        size -= 4;
    }

    if(flags & CDX_MOV_TRUN_FIRST_SAMPLE_FLAGS)
    {
        if(size < 4)
        {
            return -1;
        }
        ret = CdxStreamRead(s->fp, buffer, 4);
        first_sample_flags = MoovGetBe32(buffer);
        size -= 4;
    }

    CDX_U64 data_offset = frag->base_data_offset + data_offset_delta;
    s->nDataOffsetDelta = data_offset_delta;

    CDX_LOGV("base data offset = %llx, data_offset_delta = %x, data_offset=%llx",
    frag->base_data_offset, data_offset_delta, data_offset); //* for debug
    for(i=0; i<entries; i++)
    {
        unsigned int sample_size = frag->size;
        int sample_flags = i ? frag->flags : first_sample_flags;
        unsigned int sample_duration = frag->duration;
        int keyframe = 0;

        if(flags & CDX_MOV_TRUN_SAMPLE_DURATION)
        {
            ret = CdxStreamRead(s->fp, buffer, 4);
            sample_duration = MoovGetBe32(buffer);
        }
        if(flags & CDX_MOV_TRUN_SAMPLE_SIZE)
        {
            ret = CdxStreamRead(s->fp, buffer, 4);
            sample_size     = MoovGetBe32(buffer);
        }
        if(flags & CDX_MOV_TRUN_SAMPLE_FLAGS)
        {
            ret = CdxStreamRead(s->fp, buffer, 4);
            sample_flags    = MoovGetBe32(buffer);
        }

        st->ctts_data[st->ctts_size].count = 1;
        if(flags & CDX_MOV_TRUN_SAMPLE_CTS)
        {
            ret = CdxStreamRead(s->fp, buffer, 4);
            st->ctts_data[st->ctts_size].duration = MoovGetBe32(buffer);
        }
        else
        {
            st->ctts_data[st->ctts_size].duration = 0;
        }
        st->ctts_size++;

        if(st->codec.codecType==CODEC_TYPE_AUDIO)
        {
            keyframe = 1;
        }
        else if(!found_keyframe)
        {
            // if sample_flags =0x01010000, the sampe is not the key frame
             keyframe = found_keyframe =
                !(sample_flags & (CDX_MOV_FRAG_SAMPLE_FLAG_IS_NON_SYNC |
                                  CDX_MOV_FRAG_SAMPLE_FLAG_DEPENDS_YES));
        }

        Sample* tmp = (Sample*)malloc(sizeof(Sample));
        if(!tmp) return -1;
        memset(tmp, 0, sizeof(Sample));

        tmp->duration = sample_duration;
        tmp->offset = data_offset;
        tmp->size = sample_size;
        tmp->keyframe = keyframe; // keyframe is not need now
        tmp->index = i;

        CDX_LOGV("%s, duration = %x, offset = %llx, sample size = %x, flag = %x, data_offset = %lld",
        st->codec.codecType == CODEC_TYPE_VIDEO ? "video sample" : "audio sample",
        sample_duration, data_offset, tmp->size, sample_flags, data_offset);
        if(st->codec.codecType == CODEC_TYPE_VIDEO)
        {
            if( aw_list_add(s->Vsamples, tmp) < 0)
            {
                CDX_LOGE("aw_list_add error!");
                return -1;
            }
        }
        else if(st->codec.codecType == CODEC_TYPE_AUDIO)
        {
            if( aw_list_add(s->Asamples, tmp) < 0)
            {
                CDX_LOGE("aw_list_add error!");
                return -1;
            }
        }

        //s->last_sample_pts = tmp->pts;
        data_offset += sample_size;
    }
    return 0;
}

/*
*       tfhd ( Track Fragment Header Box )
*       set up information and defaults used for samples in fragment
*/
static int _MovParseTfhd(MOVContext *s,MOV_atom_t atom)
{
    if(atom.size < 12)
    {
        CDX_LOGI("Careful: the tfhd box size is less than 12 bytes!");
    }

    MOVFragment* frag = &s->fragment;
    MOVTrackExt* trex = NULL;
    int flags, track_id, i;
    unsigned char buffer[8] = {0};
    int ret;
    int size = atom.size - 8;

    ret = CdxStreamRead(s->fp, buffer, 4);
    if(ret < 4)
    {
        CDX_LOGE("read failed.");
        return -1;
    }
    flags = MoovGetBe24(buffer+1);  // version 1byte; tf_flags (3 bytes)
    size -= 4;

    ret = CdxStreamRead(s->fp, buffer, 4);
    size -= 4;
    track_id = MoovGetBe32(buffer);
    if(!track_id)
    {
        CDX_LOGE("Track id =0.");
        return -1;
    }
    frag->track_id = track_id;

    //find the same track id in the 'trex' ( in 'moov', which setup the default information )
    for(i=0; i<s->trex_num; i++)
    {
        if(s->trex_data[i].track_id == frag->track_id)
        {
            trex = &s->trex_data[i];
            break;
        }
    }

    if(!trex)
    {
        if(!s->bSmsSegment) //* sms has no trex ??
        {
            CDX_LOGE("Worning: could not find corresponding trex!");
            return -1;
        }
    }

    if(flags & CDX_MOV_TFHD_BASE_DATA_OFFSET)
    {
        ret = CdxStreamRead(s->fp, buffer, 8);
        size -= 8;
        frag->base_data_offset = MoovGetBe64(buffer);
    }
    else
    {
        frag->base_data_offset = frag->moof_offset;
    }

    if(flags & CDX_MOV_TFHD_STSD_ID)
    {
        ret = CdxStreamRead(s->fp, buffer, 4);
        size -= 4;
        frag->stsd_id = MoovGetBe32(buffer);
    }
    else
    {
        if(trex)
            frag->stsd_id = trex->stsd_id;
        else
            frag->stsd_id = 0;
    }

    if(flags & CDX_MOV_TFHD_DEFAULT_DURATION)
    {
        ret = CdxStreamRead(s->fp, buffer, 4);
        size -= 4;
        frag->duration = MoovGetBe32(buffer);
    }
    else
    {
        if(trex)
            frag->duration = trex->duration;
        else
            frag->duration = 0;
    }

    if(flags & CDX_MOV_TFHD_DEFAULT_SIZE)
    {
        ret = CdxStreamRead(s->fp, buffer, 4);
        size -= 4;
        frag->size = MoovGetBe32(buffer);
    }
    else
    {
        if(trex)
            frag->size = trex->size;
        else
            frag->size = 0;
    }

    if(flags & CDX_MOV_TFHD_DEFAULT_FLAGS)
    {
        ret = CdxStreamRead(s->fp, buffer, 4);
        size -= 4;
        frag->flags   =  MoovGetBe32(buffer);
    }
    else
    {
        if(trex)
            frag->flags = trex->flags;
        else
            frag->flags = 0;
    }

    if(size < 0)
    {
        CDX_LOGI("the size<%d> is error", size);
        return -1;
    }

    return 0;
}

static int _MovParseTraf(MOVContext *s,MOV_atom_t atom)
{
    int ret = 0;
    unsigned char buffer[4096];
    if(atom.size == 0)
        return 0;

    unsigned int offset = 0;
    MOV_atom_t a = {0, 0, 0};
    while( (offset < atom.size-8)&& (!ret) )
    {
        ret = CdxStreamRead(s->fp, buffer, 8);
        if(ret < 8)
        {
            break;
        }

        a.size = MoovGetBe32(buffer);   //big endium
        a.type = MoovGetLe32(buffer+4);

        CDX_LOGV("traf  size = %x, type=%x", a.size, a.type);
        if((a.size == 0) || (a.type == 0))
        {
            CDX_LOGE("atom error!");
            return -1;
        }

        if(a.type == MKTAG( 't', 'f', 'h', 'd' ))
        {
            // sample default infomation: base offset, duration,...
            ret = s_mov_moof_parser[MESSAGE_ID_TFHD].movParserFunc(s,a);
            if(ret < 0)
            {
                CDX_LOGE("parse tfhd failed.");
                return -1;
            }
        }
        else if(a.type == MKTAG( 't', 'r', 'u', 'n' ))
        {
            // the offset, duration, size of samples
            ret = s_mov_moof_parser[MESSAGE_ID_TRUN].movParserFunc(s,a);
            if(ret < 0)
            {
                CDX_LOGE("parse trun failed.");
                return -1;
            }
        }
        else if(a.type == MKTAG( 'u', 'u', 'i', 'd' ))
        {
            ret = s_mov_moof_parser[MESSAGE_ID_SENC].movParserFunc(s,a);
            if(ret < 0)
            {
                CDX_LOGE("parse trun failed.");
                return -1;
            }
        }
        //else if(a.type == MKTAG( 's', 'd', 't', 'p' ))
        //{
        //    ret = CdxStreamSeek(pb, a.size-8, SEEK_CUR);
        //    if(ret < 0) return -1;
        //}
        else
        {
            if(s->bSeekAble)
            {
                ret = CdxStreamSeek(s->fp, a.size-8, SEEK_CUR);
                if(ret < 0) return -1;
            }
            else
            {
                ret = CdxStreamSkip(s->fp, a.size-8);
                if(ret < 0) return -1;
            }
        }
        offset += a.size;
        //CDX_LOGD("offset=%u", offset);
    }

    return ret;
}

/*
********************************************************************************************
// every segment MP4 file in DASH
 ----styp
 ----sidx  ( time scale )  'mvhd' atom also have the same time scale
 ----moof
            ------mfhd
            ------traf
                        ------tfhd ( the sample default infomation  )
                        ------tfdt (not used)
                        ------trun ( the sample offset in file  )
                        ------sdtp(not used)
********************************************************************************************
*/

/*
*    the movie fragment Box of fragment MP4 file
*    get the sample offset, sample duration and sample size from it
*
*/
static int _MovParseMoof(MOVContext *s,unsigned char *buf,MOV_atom_t atom)
{
    int ret = 0;
    if(atom.size == 0)
        return 0;
    unsigned char buffer[8];
    if(buf == NULL)
    {
        return -1;
    }

    unsigned int offset = 0;
    MOV_atom_t a = {0, 0, 0};

    while((offset < atom.size-8) && (!ret))
    {
        ret = CdxStreamRead(s->fp, buffer, 8);
        if(ret < 8)
        {
            break;
        }

        a.size = MoovGetBe32(buffer);   //big endium
        a.type = MoovGetLe32(buffer+4);

        CDX_LOGV("type = %x, size=%x", a.type, a.size);
        if((a.size == 0) || (a.type == 0))
        {
            CDX_LOGE(" moof atom error!");
            return -1;
        }

        CDX_LOGV("type = %x, size=%x", a.type, a.size);
        if(a.type == MKTAG( 't', 'r', 'a', 'f' ))
        {
            ret = s_mov_moof_parser[MESSAGE_ID_TRAF].movParserFunc(s,a);
        }
        else
        {
             if(s->bSeekAble)
            {
                ret = CdxStreamSeek(s->fp, a.size-8, SEEK_CUR);
                if(ret < 0) return -1;
            }
            else
            {
                ret = CdxStreamSkip(s->fp, a.size-8);
                if(ret < 0) return -1;
            }
        }
        offset += a.size;
    }

    return ret;
}

// some media segment was cut to many chunks( like 'stco' in MP4 file ),
//'sidx' was used for get the chunk offset and chunk duration in a same segment.
// but in DASH, there are two kinds of segment
// @ 1. every segment is a chunk, sidx is in the start of every segment, and the sidx did not
//      have any effect to the segment but for seeking in the segment( HDWorld )
//          in this case, we should parser sidx in seek
// @ 2. the segments are in one mp4 file, and each segment is set by the indexRange in mpd, so
//      the sidx is very important in this environment (Big buck bunny)
//in this case, all of the sidx atoms are in the start of file, so we parse all the sidx in movTop
static int _MovParseSidx(MOVContext *s,unsigned char *buf,MOV_atom_t atom)
{
    int size = atom.size-8;
    int ret = 0;
    int flags;
    int version;
    unsigned char buffer[8] = {0};
    MOVStreamContext* st = NULL;
    if(buf == NULL)
    {
        return -1;
    }

    ret = CdxStreamRead(s->fp, buffer, 4);
    version = buffer[0];
    flags = MoovGetBe24(buffer+1);
    size -= 4;

    ret = CdxStreamRead(s->fp, buffer, 8);
    int reference_id = MoovGetBe32(buffer);
    int time_scale = MoovGetBe32(buffer+4);
    size -= 8;

    int i;
    //CDX_LOGD("nb_stream = %d, time_scale = %d", s->nb_streams, time_scale);
    for(i=0; i<s->nb_streams; i++)
    {
        // streams->id is the stream track_id, set in 'tkhd' atom in every 'trak'
        if(s->streams[i]->id == reference_id)
        {
            st = s->streams[i];
            break;
        }
    }
    if(!st)
    {
        CDX_LOGW("Worning: sidx- could not find corresponding stream id!");
        return -1;
    }
    st->time_scale = time_scale;

    long long int earlist_presentation_time;
    long long int first_offset;

    if(version == 0)
    {
        if(size < 8)
        {
            CDX_LOGE("the size of 'sidx' is error!");
            return -1;
        }
        int tmp;
        ret = CdxStreamRead(s->fp, buffer, 8);
        tmp = MoovGetBe32(buffer);
        earlist_presentation_time = tmp;
        tmp = MoovGetBe32(buffer+4);
        first_offset = tmp;

        size -= 8;
    }
    else
    {
        if(size < 16)
        {
            CDX_LOGE("the size of 'sidx' is error!");
            return -1;
        }
        ret = CdxStreamRead(s->fp, buffer, 8);
        earlist_presentation_time = MoovGetBe64(buffer);
        ret = CdxStreamRead(s->fp, buffer, 8);
        first_offset = MoovGetBe64(buffer);

        size -= 16;
    }

    if(size < 4) return -1;

    ret = CdxStreamRead(s->fp, buffer, 4);
    //reserved (2 bytes)
    CDX_S16 reference_count = moovGetBe16(buffer+2);
    CDX_S32 refrence_count_end = s->sidx_count + reference_count;
    size -= 4;

    if(size < reference_count*12)
    {
        return -1;
    }

    CDX_U64 total_duration = 0;
    CDX_U64 offset = 0;

    MOVSidx* sidx;
    sidx = (MOVSidx*)realloc(s->sidx_buffer, refrence_count_end * sizeof(MOVSidx));
    if(!sidx)
    {
        CDX_LOGE("sidx realloc error!");
        return -1;
    }
    else
    {
        s->sidx_buffer = sidx;
    }
    int sub_sidx = 0;

    CDX_U32 d1, d2, d3;
    for(i=s->sidx_count; i<refrence_count_end; i++)
    {
        CdxStreamRead(s->fp, buffer, 4);
        d1 = MoovGetBe32(buffer); //refrerence size
        CdxStreamRead(s->fp, buffer, 4);
        d2 = MoovGetBe32(buffer); //reference duration
        CdxStreamRead(s->fp, buffer, 4);
        d3 = MoovGetBe32(buffer); //reference type

        if(d1 & 0x80000000)
        {
            //it is the index table of other sidx box, so skip it
            sub_sidx = 1;
        }

        int sap = d3 & 0x80000000;
        int saptype = (d3&0x70000000) >> 28;
        if(!sap || saptype > 2)
        {
            CDX_LOGI("not a Stream Access Point, unsupported type!");
        }

        s->sidx_buffer[i].current_dts = s->sidx_time + total_duration;
        s->sidx_buffer[i].offset = offset + atom.size + atom.offset + first_offset;
        //s->sidx_buffer[i].offset = offset ;
        s->sidx_buffer[i].size = d1 & 0x7fffffff;
        s->sidx_buffer[i].duration = ((CDX_U64)d2) *1000/ (CDX_U64)time_scale ;
        //CDX_LOGD("current_dts = %lld, sidx_time = %lld, total_duration = %lld",
        //s->sidx_buffer[i].current_dts, s->sidx_time, total_duration);
        //LOGD("duration = %lld. offst = %lld", s->sidx_buffer[i].duration,
        //s->sidx_buffer[i].offset);
        offset += d1 & 0x7fffffff;
        total_duration += (CDX_U64)d2;
    }

    s->sidx_count = refrence_count_end;
    s->sidx_time += total_duration;
    s->sidx_total_time += total_duration*1000 / (CDX_U64)time_scale;
    if(sub_sidx)
    {
        free(s->sidx_buffer);
        s->sidx_buffer = NULL;
        s->sidx_count = 0;
        s->sidx_time = 0;
        s->sidx_total_time = 0;
    }

    return 0;
}

static int _MovParseStyp(MOVContext *s,unsigned char *buf,MOV_atom_t a)
{
    CDX_S32 ret;
    //if every segment has a url, the 'styp' must be in the first box in the media segmnet
    if(buf == NULL)
    {
        return -1;
    }

    s->first_styp_offset = 0;
    s->is_fragment = 1;
    if(s->bSeekAble)
    {
        ret = CdxStreamSeek(s->fp, a.size-8, SEEK_CUR);
        if(ret < 0) return -1;
    }
    else
    {
        ret = CdxStreamSkip(s->fp, a.size-8);
        if(ret < 0) return -1;
    }
    return ret;
}

static int _MovParseMdat(MOVContext *s,unsigned char *buf,MOV_atom_t a)
{
    CDX_S32 ret = 0;
    s->mdat_count++;
    s->found_mdat = 1;
    if(s->found_moov)
    {
        return 1;
    }

    if(a.size == 1)
    {
        // if the mdat size is 1, is a extend mdat size.
        // the 8 size after 'mdat' is the true size
        ret = CdxStreamRead(s->fp, buf, 8);

        CDX_S64 size = MoovGetBe64(buf);

        if(s->bSeekAble)
        {
            //CDX_LOGD("---size=%llx, offset = %llx", size, CdxStreamTell(pb));
            ret = CdxStreamSeek(s->fp, size-16, SEEK_CUR);

            if(ret < 0) return -1;
        }
        else
        {
            unsigned char* buf = malloc(size-16);
            ret = CdxStreamRead(s->fp, buf, size-16);
            free(buf);
            if(ret < 0) return -1;
        }
    }
    else
    {
        if(s->bSeekAble)
        {
            ret = CdxStreamSeek(s->fp, a.size-8, SEEK_CUR);
            if(ret < 0) return -1;
        }
        else
        {
            unsigned char* buf = malloc(a.size-8);
            ret = CdxStreamRead(s->fp, buf, a.size-8);
            free(buf);
            if(ret < 0) return -1;
        }
    }
    return 0;
}

//parse the data in the moov_buffer
static int _MovParseMoov(MOVContext *c,unsigned char *buf,MOV_atom_t atom)
{
    MOV_atom_t a = {0, 0, 0};
    CDX_S32 err = 0;
    unsigned char* buffer = c->moov_buffer;
    unsigned int offset = 0;
    unsigned int total_size = 0;

    if(buf == NULL)
    {
        return -1;
    }

    CDX_LOGV("Moov atom size:%d\n",atom.size);

    a.size = MoovGetBe32(buffer+offset);
    offset += 4;
    a.type = MoovGetLe32(buffer+offset);
    offset += 4;
    if((a.size==0) || (a.type == 0))
    {
        CDX_LOGV("mov atom is end!");
    }

    total_size = a.size - 8;  // size of 'moov' atom

    //LOGD("buf[0] = %x, buf[1]= %x, buf[2]=%x, buf[3]=%x",
    //buffer[0], buffer[1], buffer[2], buffer[3]);
    //LOGD("size = %x, type = %x, totalsize = %x", a.size, a.type, total_size);

    while( (offset < total_size) && !err)
    {
        if(a.size >= 8)
        {
            a.size = MoovGetBe32(buffer+offset);
            offset += 4;
            a.type = MoovGetLe32(buffer+offset);
            offset += 4;
            if((a.size==0) || (a.type == 0))
            {
                CDX_LOGV("mov atom is end!");
                break;
            }
        }

        if(a.size == 1)  /* 64 bit extended size */
        {
            a.size = (unsigned int)(MoovGetBe64(buffer+offset)-8);
            offset += 8;
        }

        if(a.type == MKTAG( 'm', 'v', 'h', 'd' ))
        {
            a.offset = offset ;
            err = s_mov_moov_parser[MESSAGE_ID_MVHD].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 'u', 'd', 't', 'a' ))
        {
            a.offset = offset ;
            err = s_mov_moov_parser[MESSAGE_ID_UDTA].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }

        else if(a.type == MKTAG( 't', 'r', 'a', 'k' ))
        {
            MOVStreamContext *st;
            if (c->nb_streams < MOV_MAX_STREAMS)
            {
                st = AvNewStream(c, c->nb_streams);
                if (!st)
                    return -2;
            }
            else
            {
                CDX_LOGW("the stream of this file is large than MOV_MAX_STREAMS !!!");
                return -2;
            }

            c->nb_streams++;
            st->codec.codecType = CODEC_TYPE_DATA;
            st->codec.extradata = NULL;
            c->streams[c->nb_streams-1] = st;

            a.offset = offset ;
            err = s_mov_moov_parser[MESSAGE_ID_TRAK].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 'm', 'v', 'e', 'x' ))
        {
            // we cannot sure it fragment mp4, if has mvex atom, // camera record for IPAD
            if(c->bDash)
            {
                c->is_fragment = 1;
            }
            a.offset = offset ;
            err = s_mov_moov_parser[MESSAGE_ID_MVEX].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else if(a.type == MKTAG( 'c', 'm', 'o', 'v' ))
        {
            a.offset = offset;
            err = s_mov_moov_parser[MESSAGE_ID_CMOV].movParserFunc(c,a);
            break;
            //offset = offset + a.size - 8; // do we need it?
        }
        else if(a.type == MKTAG( 'm', 'e', 't', 'a' ))
        {
            CDX_LOGV("===meta===");
            a.offset = offset ;
            err = s_mov_moov_parser[MESSAGE_ID_META].movParserFunc(c,a);
            offset = offset + a.size - 8;
        }
        else
        {
            offset = offset + a.size - 8;
        }

    }
    return 0;
}

static int _MovParseWide(MOVContext *s,unsigned char *buf,MOV_atom_t a)
{
    // sometimes, the mdat is in the wide atom
    CDX_S32 ret = 0;

    if(buf == NULL)
    {
        return -1;
    }

    if(a.size != 8)
    {
       CDX_LOGW("care, the 'wide' atom size is not 8 <%d> ", a.size);
       if(s->bSeekAble)
       {
           ret = CdxStreamSeek(s->fp, a.size-8, SEEK_CUR);
           if(ret < 0) return -1;
       }
       else
       {
           ret = CdxStreamSkip(s->fp, a.size-8);
           if(ret < 0) return -1;
       }
    }
    return ret;
}

static int _MovParseFtyp(MOVContext *s,unsigned char *buf,MOV_atom_t a)
{
    int ret = 0;
    CDX_U32 major_brand;
    int   minor_ver;

    //size is out of boundary.
    if(a.size < 16 || a.size >= 1032)
    {
        CDX_LOGE("error, the ftyp atom is invalid");
        return -1;
    }
    ret = CdxStreamRead(s->fp, buf, a.size-8);
    major_brand = MoovGetLe32(buf);
    minor_ver = MoovGetBe32(buf+4);

    char* compatible = (char*)buf+8;
    buf[a.size-8] = '\0';
    CDX_LOGV("---- compatible = %s", compatible);
    //if the major brand and compatible brand are neithor "qt  ",
    //it is ISO base media file
    if (major_brand != MKTAG('q','t',' ',' ') && !strstr(compatible, "qt  "))
    {
       s->isom = 1;
    }

    return ret;
}

MOVStreamContext *AvNewStream(MOVContext *c, CDX_S32 id)
{
    MOVStreamContext *st;

    if (c->nb_streams >= MOV_MAX_STREAMS)
        return NULL;

    st = (MOVStreamContext *)malloc(sizeof(MOVStreamContext));
    if (!st)
        return NULL;
    memset(st,0,sizeof(MOVStreamContext));

    st->index = c->nb_streams;
    st->id = id;
    st->rotate[0] = '\0';

    return st;
}

//get the top level atom of mov, and read the 'moov' atom data to moov_buffer
int _MovTop(MOVContext *s)
{
    int ret = 0;
    int datalen = 0;
    int readlen = 0;
    int tmplen = 0;
    MOV_atom_t a = {0, 0, 0};
    unsigned char buf[1024] = {0};

    while(ret > -1)
    {
        a.offset = CdxStreamTell(s->fp);
        ret = CdxStreamRead(s->fp, buf, 8);
        if(ret < 8)
        {
            CDX_LOGI("end of file? reslut(%d)", ret);
            break;
        }

        a.size = MoovGetBe32(buf);   //big endium
        a.type = MoovGetLe32(buf+4);
        if((a.size == 0) || (a.type == 0))
        {
            break;
        }

        if(a.type == MKTAG( 'f', 't', 'y', 'p' ))
        {
            ret = g_mov_top_parser[MESSAGE_ID_FTYP].movParserFunc(s,buf,a);
            if(ret < 0)
            {
               return ret;
            }
        }
        else if (a.type == MKTAG( 'w', 'i', 'd', 'e' ))  //if have the 'wide' atom, it must
                                                         //before the 'mdat' atom
        {
           ret = g_mov_top_parser[MESSAGE_ID_WIDE].movParserFunc(s,buf,a);
           if(ret < 0)
           {
               return ret;
           }
        }
        else if (a.type == MKTAG( 'm', 'o', 'o', 'v' ))
        {
            //in dash, 'moov' atom will come twice, one for video init segment, another one for
            //audio init segment
            // if the video bitrate changed, there were be a 'moov' atom in the middle of file
            if(!s->found_moov)
            {
                s->found_moov = 1;
                s->moov_buffer = (unsigned char*)malloc(a.size);
                s->moov_size = a.size;
                if(!s->moov_buffer)
                {
                   return -1;
                }
                memset(s->moov_buffer, 0, a.size);
                memcpy(s->moov_buffer, buf, 8);
                datalen = a.size-8 ;
                readlen = 0;
                while ( datalen > 0 )
                {
                    if (datalen > MAX_READ_LEN_ONCE)
                                    tmplen = MAX_READ_LEN_ONCE;
                                else
                                    tmplen = datalen;
                                ret = CdxStreamRead(s->fp, s->moov_buffer+8+readlen , tmplen);
                                if(ret < 0)
                                {
                                    CDX_LOGE("CdxStreamRead error!ret (%d)",ret);
                                    return -1;
                                }
                                readlen += tmplen;
                                datalen = datalen - tmplen;
                }
            }
            else
            {
                CDX_LOGI("duplicated moov atom, skip it!");
                if(s->bSeekAble)
                {
                    ret = CdxStreamSeek(s->fp, a.size-8, SEEK_CUR);
                    if(ret < 0) return -1;
                }
                else
                {
                    ret = CdxStreamSkip(s->fp, a.size-8);
                    if(ret < 0) return -1;
                }
            }
#if 0
            FILE* fp = fopen("/mnt/sdcard/moov.s", "wb");
            fwrite(s->moov_buffer, 1, s->moov_size, fp);
            fclose(fp);
#endif
            ret = g_mov_top_parser[MESSAGE_ID_MOOV].movParserFunc(s,buf,a);
            if(s->found_mdat)
            {
                break;
            }
        }
        else if (a.type == MKTAG( 'm', 'd', 'a', 't' ))
        {
            ret = g_mov_top_parser[MESSAGE_ID_MDAT].movParserFunc(s,buf,a);
            if(ret < 0)
            {
               return ret;
            }
            else if(ret == 1)
            {
                break;
            }
        }
        else if (a.type == MKTAG( 's', 't', 'y', 'p' ))
        {
            ret = g_mov_top_parser[MESSAGE_ID_STYP].movParserFunc(s,buf,a);
            if(ret < 0)
            {
               return ret;
            }
        }
        else if (a.type == MKTAG( 's', 'i', 'd', 'x' ))
        {
            //offset and duration of the chunk in segment
            //sidx is used for dash (one segment), local file is not needed
            s->is_fragment = 1;
            s->sidx_flag = 1;

            ret = g_mov_top_parser[MESSAGE_ID_SIDX].movParserFunc(s,buf,a);
        }
        else if (a.type == MKTAG( 'm', 'o', 'o', 'f' ))
        {
            CDX_LOGD("the mov file contain movie fragment box!");
            s->is_fragment = 1;
            if(!s->first_moof_offset)
            {
                s->first_moof_offset = CdxStreamTell(s->fp)-8;
            }
            s->fragment.moof_offset = CdxStreamTell(s->fp)-8;
            if(s->bSeekAble)
            {
                ret = CdxStreamSeek(s->fp, a.size-8, SEEK_CUR);
                if(ret < 0) return -1;
            }
            else
            {
                ret = CdxStreamSkip(s->fp, a.size-8);
                if(ret < 0) return -1;
            }
            break;
            //MovParseMoof(s, pb, a);
        }
        else
        {
            if(s->bSeekAble)
            {
                ret = CdxStreamSeek(s->fp, a.size-8, SEEK_CUR);
                if(ret < 0) return -1;
            }
            else
            {
                ret = CdxStreamSkip(s->fp, a.size-8);
                if(ret < 0) return -1;
            }
        }
    }

    return 0;
}

int CdxMovAtomInit()
{
    CDX_S32 ret;
    ret = CdxMovAtomCreate(_MovTop);
    return ret;
}
