/*
********************************************************************************
*                                    eMOD
*                   the Easy Portable/Player Develop Kits
*                               mod_herb sub-system
*                               muxer_Mp4 module
*
*          (c) Copyright 2009-2010, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : muxer_mp4_i.h
* Version: V1.0
* By     : Eric_wang
* Date   : 2010-2-22
* Description:
********************************************************************************
*/
#ifndef MPEG2TS_MUXER_CFG
#define MPEG2TS_MUXER_CFG

typedef unsigned int AVCRC;

typedef enum {
    AV_CRC_8_ATM,
    AV_CRC_16_ANSI,
    AV_CRC_16_CCITT,
    AV_CRC_32_IEEE,
    AV_CRC_32_IEEE_LE,  /*< reversed bitorder version of AV_CRC_32_IEEE */
    AV_CRC_MAX,         /*< Not part of public API! Do not use outside libavutil. */
}AVCRCId;

#endif  /* MPEG2TS_MUXER_CFG */

