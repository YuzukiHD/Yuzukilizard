/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmsBase.h
 * Description : MmsBase
 * History :
 *
 */

#ifndef CDX_MMS_BASE_H
#define CDX_MMS_BASE_H

typedef enum {
    STREAM_CONNECT_NON_BLOCK = 0,
    STREAM_CONNECT_BLOCK     = 1
}StreamConnectModeE;

struct HttpHeaderField
{
    const char *key;
    const char *val;
};

typedef struct AW_HTTP_FIELD_TYPE
{
    char *fieldName;
    struct AW_HTTP_FIELD_TYPE *next;
}CdxHttpFieldT;

typedef struct AW_HTTP_HEADER
{
     char *protocol;
     char *method;
     char *uri;
     unsigned int statusCode;
     char *reasonPhrase;
     unsigned int httpMinorVersion;
     // Field variables
     CdxHttpFieldT *firstField;
     CdxHttpFieldT *lastField;
     unsigned int fieldNb;
     char *fieldSearch;
     CdxHttpFieldT *fieldSearchPos;
     // Body variables
     char *body;
     size_t bodySize;
     char *buffer;
     size_t bufferSize;
     unsigned int isParsed;
 } CdxHttpHeaderT;

#if 0
typedef struct CdxHttpHeaderField
{
    const char *key;
    const char *val;
}CdxHttpHeaderFieldT;

typedef struct CdxHttpHeaderFields
{
    int num;
    CdxHttpHeaderFieldT *pHttpHeader;
}CdxHttpHeaderFieldsT;
#endif

#endif
