/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "DemuxTypeMap"
#include <utils/plat_log.h>

#include "DemuxTypeMap.h"

typedef struct DemuxTypeMapping {
	enum CDX_MEDIA_FILE_FORMAT  demux_type;
	enum CdxParserTypeE         parser_type;
}DemuxTypeMapping;
DemuxTypeMapping DemuxTypeMap[] =
{
    {CDX_MEDIA_FILE_FMT_UNKOWN          , CDX_PARSER_UNKNOW},
    {CDX_MEDIA_FILE_FMT_MOV             , CDX_PARSER_MOV},
    {CDX_MEDIA_FILE_FMT_MKV             , CDX_PARSER_MKV},
    {CDX_MEDIA_FILE_FMT_ASF             , CDX_PARSER_ASF},
    {CDX_MEDIA_FILE_FMT_TS              , CDX_PARSER_TS},
    {CDX_MEDIA_FILE_FMT_AVI             , CDX_PARSER_AVI},
    {CDX_MEDIA_FILE_FMT_FLV             , CDX_PARSER_FLV},
    {CDX_MEDIA_FILE_FMT_PMP             , CDX_PARSER_PMP},
    
    {CDX_MEDIA_FILE_FMT_NETWORK_OTHERS  , CDX_PARSER_HLS},
    //CDX_PARSER_DASH,
    {CDX_MEDIA_FILE_FMT_NETWORK_OTHERS  , CDX_PARSER_MMS},
    //CDX_PARSER_BD,
    {CDX_MEDIA_FILE_FMT_OGG             , CDX_PARSER_OGG},
    //CDX_PARSER_M3U9,
    {CDX_MEDIA_FILE_FMT_RMVB            , CDX_PARSER_RMVB},
    //CDX_PARSER_PLAYLIST,
    {CDX_MEDIA_FILE_FMT_APE             , CDX_PARSER_APE},
    {CDX_MEDIA_FILE_FMT_FLAC            , CDX_PARSER_FLAC},
    {CDX_MEDIA_FILE_FMT_AMR             , CDX_PARSER_AMR},
    //CDX_PARSER_ATRAC,
    {CDX_MEDIA_FILE_FMT_MP3             , CDX_PARSER_MP3},
    {CDX_MEDIA_FILE_FMT_DTS             , CDX_PARSER_DTS},
    {CDX_MEDIA_FILE_FMT_AC3             , CDX_PARSER_AC3},
    {CDX_MEDIA_FILE_FMT_AAC             , CDX_PARSER_AAC},
    {CDX_MEDIA_FILE_FMT_WAV             , CDX_PARSER_WAV},	
    //CDX_PARSER_REMUX, /* rtsp, etc... */
    //CDX_PARSER_WVM,
    {CDX_MEDIA_FILE_FMT_MPG             , CDX_PARSER_MPG},
    //CDX_PARSER_MMSHTTP,
    //CDX_PARSER_AWTS,
    //CDX_PARSER_SSTR,
    //CDX_PARSER_CAF,
};

CDX_MEDIA_FILE_FORMAT ParserType2DemuxType(enum CdxParserTypeE parserType)
{   
    int i = 0;

	for(i=0; i< (int)(sizeof(DemuxTypeMap)/sizeof(DemuxTypeMapping)); i++)
	{
		if(DemuxTypeMap[i].parser_type == parserType)
		{
			return DemuxTypeMap[i].demux_type;
		}
	}

	return CDX_MEDIA_FILE_FMT_UNSUPPORT;
}
enum CdxParserTypeE DemuxType2ParserType(CDX_MEDIA_FILE_FORMAT demuxType)
{   
    int i = 0;

	for(i=0; i< (int)(sizeof(DemuxTypeMap)/sizeof(DemuxTypeMapping)); i++)
	{
		if(DemuxTypeMap[i].demux_type == demuxType)
		{
			return DemuxTypeMap[i].parser_type;
		}
	}

	return CDX_PARSER_UNKNOW;
}

