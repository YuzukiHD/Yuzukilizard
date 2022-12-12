/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMovAtom.h
 * Description :
 * History
 * Date    : 2017/05/02
 * Comment : refactoring mov parser,mov about atom implement in this file
 */

#ifndef __CDX_mov_atom_H
#define __CDX_mov_atom_H

#include "CdxMovParser.h"

#define MAX_READ_LEN_ONCE  5*1024*1024

typedef int (*MovTopFunc)(MOVContext *c);
typedef int (*MovTopParserFunc)(MOVContext *c,unsigned char *buf,MOV_atom_t a);
typedef int (*MovParserFunc)(MOVContext *c,MOV_atom_t a);

typedef struct MovTopParserProcess
{
    unsigned short      MessageID;
    MovTopParserFunc    movParserFunc;
}MovTopParserProcess;

typedef struct MovParserProcess
{
    unsigned short      MessageID;
    MovParserFunc       movParserFunc;
}MovParserProcess;

typedef enum {
    MESSAGE_ID_FTYP = 0,
    MESSAGE_ID_WIDE,
    MESSAGE_ID_MOOV,
    MESSAGE_ID_MDAT,
    MESSAGE_ID_STYP,
    MESSAGE_ID_SIDX,
    MESSAGE_ID_MOOF,
}MessageTopId;

typedef enum {
    MESSAGE_ID_MVHD = 0,
    MESSAGE_ID_UDTA,
    MESSAGE_ID_KEYS,
    MESSAGE_ID_TRAK,
    MESSAGE_ID_MVEX,
    MESSAGE_ID_CMOV,
    MESSAGE_ID_TREX,
    MESSAGE_ID_META,
}MessageMoovId;

typedef enum {
    MESSAGE_ID_TRAF = 0,
    MESSAGE_ID_TFHD,
    MESSAGE_ID_TRUN,
    MESSAGE_ID_SENC,
}MessageMoofId;

typedef enum {
    MESSAGE_ID_TKHD = 0,
    MESSAGE_ID_ELST,
    MESSAGE_ID_MDIA,
}MessageTrakId;

typedef enum {
    MESSAGE_ID_MDHD = 0,
    MESSAGE_ID_HDLR,
    MESSAGE_ID_MINF,
}MessageMdiaId;

typedef enum {
    MESSAGE_ID_STBL = 0,
    MESSAGE_ID_STSD,
    MESSAGE_ID_STTS,
    MESSAGE_ID_STSS,
    MESSAGE_ID_STSZ,
    MESSAGE_ID_STSC,
    MESSAGE_ID_STCO,
    MESSAGE_ID_CTTS,
    MESSAGE_ID_SBGP,
}MessageStblId;

typedef enum {
    MESSAGE_ID_AVCC = 0,
    MESSAGE_ID_HVCC,
    MESSAGE_ID_WAVE,
    MESSAGE_ID_ESDS,
    MESSAGE_ID_GLBL,
}MessageStsdId;

int CdxMovAtomInit();
int CdxMovAtomCreate(MovTopFunc MovAtomFunc);
MOVStreamContext *AvNewStream(MOVContext *c, CDX_S32 id);
CDX_U32 MoovGetLe32(unsigned char *s);
CDX_U32 MoovGetBe24(unsigned char *s);
CDX_U32 MoovGetBe32(unsigned char *s);
CDX_S64 MoovGetBe64(unsigned char *s);
int _MovTop(MOVContext *s);

#endif