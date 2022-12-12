/*
********************************************************************************
*                                    eMOD
*                   the Easy Portable/Player Develop Kits
*                               mod_herb sub-system
*                          (module name, e.g.mpeg4 decoder plug-in) module
*
*          (c) Copyright 2009-2010, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : sa_config.h
* Version: V1.0
* By     : Eric_wang
* Date   : 2010-5-11
* Description:
********************************************************************************
*/
#ifndef _SA_CONFIG_H_
#define _SA_CONFIG_H_

#include "mp4_mux_lib.h"

typedef unsigned int UINT32;

//1. aac header
enum AudioObjectType
{
    AACMAIN = 1,
    AACLC   = 2,
    AACSSR  = 3,
    AACLTP  = 4,
    AACSBR  = 5,
    AACSCALABLE = 6,
    TWINVQ  = 7,
    AAC_ADD0X10 = 0x10
};
//typedef bool   HXBOOL;
typedef struct ga_config_data_struct
{
    UINT32 audioObjectType;//AACLC=2;
    UINT32 samplingFrequency;       //hz
    //UINT32 extensionSamplingFrequency;
    //UINT32 frameLength;
    //UINT32 coreCoderDelay;
    UINT32 numChannels;
    //UINT32 numFrontChannels;
    //UINT32 numSideChannels;
    //UINT32 numBackChannels;
    //UINT32 numFrontElements;
    //UINT32 numSideElements;
    //UINT32 numBackElements;
    //UINT32 numLfeElements;
    //UINT32 numAssocElements;
    //UINT32 numValidCCElements;
    //HXBOOL bSBR;
} ga_config_data;

int  sa_config_get_data_aac(unsigned char *bs, int length,ga_config_data *data);

int MuxerGenerateAudioExtraData(__extra_data_t *pExtra, _media_file_inf_t *pAudioInf);

//2. adpcm header

#endif  /* _SA_CONFIG_H_ */

