/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmshttpParser.h
 * Description : parser for mms stream whose url is started with "http"
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

struct CdxMmshttpParser
{
    CdxParserT parserinfo;
    CdxStreamT *stream;
    cdx_int64 size;     //the Refrence file size
    char *buffer;       // the Refrence file buffer

    int status;
    int mErrno;
    int exitFlag;       // exit flag, for unblocking in prepare

    CdxParserT *parserinfoNext;     //the parser which called by mms parser (asf parser)
    CdxStreamT *streamNext;         // stream of next parser (mmsh stream)
    CdxDataSourceT* datasource;

    pthread_mutex_t   mutex;
};

#endif
