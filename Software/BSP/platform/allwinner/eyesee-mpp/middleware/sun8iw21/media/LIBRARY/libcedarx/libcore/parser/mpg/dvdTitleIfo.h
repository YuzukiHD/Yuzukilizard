/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : dvdTitleInfo.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment :
*
*
*/


#ifndef _DVD_IFO_H_
#define _DVD_IFO_H_

#include "CdxMpgParser.h"

#define FP_PGC    0
#define VMGM_PGC  1
#define VTSM_PGC  2
#define TITLE_PGC 3

#define VOB_VIDEO_LB_LEN 0x800
#define VOB_BASE_FILE_LEN 0x7fee0
#define VOB_JUMP_TIME_THRESH 5000

#define NOT_FIND_VOBID -1
#define FIND_NEW_VOBID 1

#define CMD_NS              100
#define PGMAP_NS            30
#define CELLPB_NS           30
#define CELLPOS_NS          30
#define PGCIPTR_NS          50        // 500 is enough?
#define CELL_ENTRY_NS       50
#define TITLE_ENTRY_NS      35
#define CHAPTER_ENTRY_NS    30
#define FILE_TITLE_NS       10
#define MAX_LANG_LEN        32


/************************************/
/*************************************/
struct CellPosition
{
  cdx_uint8  vobId[2];
  cdx_uint8  reserved;
  cdx_uint8  cellId;
};

struct CellPlayback
{
  cdx_uint8 cellBlockIfo[2];
  cdx_uint8 cellStillTime;
  cdx_uint8 cellCmdNS;
  cdx_uint8 cellPbTime[4];
  cdx_uint8 firstVobuSa[4];
  cdx_uint8 firstIlvuEa[4];
  cdx_uint8 lastVobuSa[4];
  cdx_uint8 lastVobuEa[4];
};

struct PgcPgMap
{
    cdx_uint8 entryCellNum;
};

struct Command
{
  cdx_uint8 data[8];
};

struct PgcCmdTable
{
  cdx_uint8 preCmdNs[2];
  cdx_uint8 postCmdNs[2];
  cdx_uint8 cellCmdNs[2];
  cdx_uint8 pgcCmdtEa[2];

  struct Command *preCmd[CMD_NS];
  struct Command *postCmd[CMD_NS];
  struct Command *cellCmd[CMD_NS];
};

struct PgcIfo
{
  cdx_uint8 reserved[2];
  cdx_uint8  programNs;
  cdx_uint8  cellNs;
  cdx_uint8  pbTime[4];
  cdx_uint8  uopCtl[4];
  cdx_uint8  astCtl[8][2];
  cdx_uint8  spstCtl[32][4];
  cdx_uint8  nextPgc[2];
  cdx_uint8  prevPgc[2];
  cdx_uint8  goUpPgc[2];

  cdx_uint8    stillTime;
  cdx_uint8    pgPlaybackMode;
  cdx_uint8  subpicPlt[16][4];

  cdx_uint8 cmdTabSa[2];
  cdx_uint8 pgMapSa[2];
  cdx_uint8 cellPbiTabSa[2];
  cdx_uint8 cellPosTabSa[2];

  struct PgcCmdTable *cmdTable;
  struct PgcPgMap *programMap[PGMAP_NS];

  struct CellPlayback *cellPlayback[CELLPB_NS];
  struct CellPosition *cellPosition[CELLPOS_NS];
};

struct PgciPtr
{
  cdx_uint8 entryId;
  cdx_uint8 pgcCategory;
  cdx_uint8 ptalMgMask[2];
  cdx_uint8 pgciSa[4];
  struct PgcIfo  *pgcIfo;
};

struct PGCIT
{
  cdx_uint8   pgciNs[2];
  cdx_uint8     reserved[2];
  cdx_uint8   pgciTabEa[4];
  struct PgciPtr *pgciPtr[PGCIPTR_NS];
};

struct PgciLangUnit
{
  cdx_uint8        langCode[2];
  cdx_uint8        langExtension;
  cdx_uint8        exists;
  cdx_uint8        langSa[4];
  struct PGCIT *pgcit;
};

struct MPGCIUT
{
    cdx_uint8   langUnitNs[2];
    cdx_uint8   reserved[2];
    cdx_uint8   mpgciutEa[4];
    struct PgciLangUnit *langUnit;
};


/************************************/
/************************************/
struct tAuAttr
{
    cdx_uint8 data[8];
};

struct tSubpicAttr
{
    cdx_uint8 data[6];
};


struct CellAdEntry
{
    cdx_uint8 vobId[2];
    cdx_uint8  cellId;
    cdx_uint8  reserved;
    cdx_uint8 cpSa[4];
    cdx_uint8 cpEa[4];
};


struct VTSI
{
    cdx_uint8    identifier[12];
    cdx_uint8    vtsEa[4];
    cdx_uint8    padding1[12];

    cdx_uint8    vtsiEa[4];
    cdx_uint8    reserved1;
    cdx_uint8    vern;
    cdx_uint8    category[4];

    cdx_uint8    padding2[90];

    cdx_uint8    vtsiMatEA[4];
    cdx_uint8    padding3[60];

    cdx_uint8    menuVobSa[4];
    cdx_uint8    titleVobSa[4];

    cdx_uint8    chapterTaPtrSa[4];

    cdx_uint8    titlePgcitSa[4];
    cdx_uint8    menuPgciutSa[4];

    cdx_uint8    timeMapSa[4];

    cdx_uint8    menuCellAdtSa[4];
    cdx_uint8   menuVobuAdmapSa[4];

    cdx_uint8    titleCellAdSa[4];
    cdx_uint8    titleVobuAdmapSa[4];

    cdx_uint8    padding5[24];

    cdx_uint8   menuVideoAttr[2];
    cdx_uint8   menuAudioStreamNs[2];
    cdx_uint8   menuAudioAttr[2];
    cdx_uint8   padding4[79];
    cdx_uint8   menuSubpicStreamNs;
    cdx_uint8   menuSubpicAttr;
    cdx_uint8   padding6[169];

    cdx_uint8   titleVideoAttr[2];
    cdx_uint8   titleAudioStreamNs[2];
    struct tAuAttr titleAudioAttr[8];
    cdx_uint8   padding7[16];
    cdx_uint8   titleSubpicStreamNs[2];
    struct tSubpicAttr titleSubpicAttr[32];
};

struct TITLEAUDIO
{
    cdx_uint8   audioNum;
    cdx_uint16  nChannelNum[8];           // channel count
    cdx_uint16  bitsPerSample[8];    // bits per sample
    cdx_uint32  samplePerSecond[8];  // sample count per second
    cdx_uint8   audioStreamId[8];
    cdx_uint8   audioLang[8][MAX_LANG_LEN];
    cdx_uint8   audioCodeMode[8];
    cdx_uint16  audioPackId[8];
    cdx_uint8   audioCheckFlag[8];
    cdx_uint16   audioErrorPackId[8];
    cdx_uint8   audioErrorStrmId[8];
    cdx_uint8   audioRightNum;
    cdx_uint8   audioWrongNum;
};

struct TITLESUBPIC
{
    cdx_uint8 nHasSubpic;
    cdx_uint8 subpicId[32];
    cdx_uint8 subpicLang[32][MAX_LANG_LEN];
    cdx_uint32 palette[16];
};

//*****************************************//

//*******************************************//
typedef struct DvdIfoS
{
    cdx_bool titleIfoFlag;
    cdx_uint16  pgciPtrNum[4];

    cdx_uint8   *inputPath;
    cdx_uint8   *vtsBuffer;

    struct VTSI         *vtsIfo;
    struct PGCIT        *titlePgcit;      // program chain information table
    struct TITLEAUDIO   audioIfo;
    struct TITLESUBPIC  subpicIfo;
} DvdIfoT;

//globle function interfaces
cdx_int16  DvdParseTitleInfo(CdxMpgParserT *MpgParser, cdx_char *pUrl);
cdx_int16  DvdOpenTitleFile(CdxMpgParserT *MpgParser, cdx_char *pUrl);

#endif /* _DVD_IFO_H_ */


