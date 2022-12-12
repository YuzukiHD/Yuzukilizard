/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxG729Parser.h
 * Description : G729Parser
 * Author  : chengkan<chengkan@allwinnertech.com>
 * Date    : 2016/04/13
 * Comment : 创建初始版本
 *
 */

#ifndef CDX_ALAC_PARSER_H
#define CDX_ALAC_PARSER_H

#include <CdxTypes.h>

#define SERIAL_SIZE (80 + 2)
#define L_FRAME      80      /* Frame size.                                */
#define PKTSIZE   1024
#define SYNC_WORD (short)0x6b21 /* definition of frame erasure flag          */
#define SIZE_WORD (short)80     /* number of speech bits                     */


typedef struct G729ParserImplS
{
  //audio common
  CdxParserT base;
  CdxStreamT *stream;
  cdx_int64  ulDuration;//ms
  pthread_cond_t  cond;
  cdx_int64  fileSize;//total file length
  cdx_int64  file_offset; //now read location
  cdx_int32  mErrno; //Parser Status
  cdx_uint32 flags; //cmd
  //caf base
  cdx_int32 framecount;
  cdx_int32 ulSampleRate ;
  cdx_int32 ulChannels ;
  cdx_int32 ulBitsSample ;
  cdx_int32 ulBitRate;
  cdx_int32 totalsamples;
  cdx_int32 nowsamples;
  cdx_int64 seektime;
  cdx_uint8 *extradata;
  cdx_int32 extradata_size;
  //meta data
  //not imply yet
}G729ParserImplS;


#endif
