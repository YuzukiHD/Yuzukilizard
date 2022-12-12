/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmsParser.h
 * Description : parser for mms stream
 * History :
 *
 */

#ifndef CDX_MMS_PARSER_H
#define CDX_MMS_PARSER_H

#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <pthread.h>

struct CdxMmsParser
{
    CdxParserT parserinfo;
    CdxStreamT *stream;

    int mErrno;
    int mStatus;
    // exit flag, for unblocking in prepare
    int exitFlag;

    // the parser which called by mms parser ( asf parser )
    CdxParserT *asfParser;

    int videoCodecFormat;
    int audioCodecFormat;
};

#endif
