/*
 * Copyright (c) 2008-2017 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : StringContainer.c
 * Description : string generator
 * History :
 *
 */

#include "StringContainer.h"
#include "CdxUtfCode.h"

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif

#define LOG_TAG "StringContainer"

//StringCtn
static kbool __stringSetTo8(void* hself, const char* other, size_t len)
{
    char *newString = NULL;
    StringCtn* strCtn   = NULL;
    if(!hself)
        return kfalse;
    strCtn = (StringCtn*)hself;

    if(strCtn->mString)
    {
        free(strCtn->mString);
        strCtn->mString = NULL;
    }
    newString = CdxAllocFromUTF8(other, len);

    strCtn->mString = newString;
    if (strCtn->mString) return ktrue;

    strCtn->mString = NULL;
    return kfalse;
}

static kbool __stringSetTo16(void* hself, const cdx_uint16* other, size_t len)
{
    char *newString = NULL;
    StringCtn* strCtn   = NULL;
    if(!hself)
        return kfalse;
    strCtn = (StringCtn*)hself;

    if(strCtn->mString)
    {
        free(strCtn->mString);
        strCtn->mString = NULL;
    }

    newString = CdxAllocFromUTF16(other, len);
    strCtn->mString = newString;
    if (strCtn->mString) return ktrue;

    strCtn->mString = NULL;
    return kfalse;
}

static void  __stringClear(void* hself)
{
    StringCtn* strCtn   = NULL;
    if(!hself) return;
    strCtn = (StringCtn*)hself;
    if(strCtn->mString)
    {
        free(strCtn->mString);
        strCtn->mString = NULL;
    }
}

static const char* __stringReturnStr(void* hself)
{
    StringCtn* strCtn   = NULL;
    if(!hself) return "null";
    strCtn = (StringCtn*)hself;
    if(strCtn->mString)
    {
        return strCtn->mString;
    }
    return "null";
}

StringCtn* GenerateStringContainer()
{
    StringCtn* strCtn   = NULL;
    strCtn = (StringCtn*)malloc(sizeof(StringCtn));

    if(!strCtn)
    {
        CDX_LOGE("No mem for StringContainer!");
        return NULL;
    }
    memset(strCtn, 0x00, sizeof(StringCtn));
    strCtn->setTo8 = __stringSetTo8;
    strCtn->setTo16 = __stringSetTo16;
    strCtn->clear = __stringClear;
    strCtn->string = __stringReturnStr;
    strCtn->setTo8(strCtn, "", strlen(""));
    CDX_LOGV("Generating StringContainer finish...");
    return strCtn;
}

kbool EraseStringContainer(void* arg)
{
    StringCtn* strCtn   = NULL;
    void* null_ptr = NULL;

    memcpy(&strCtn, arg, sizeof(strCtn));
    if(!strCtn)
    {
        CDX_LOGW("StringContainer has already been free");
        return kfalse;
    }

    if(strCtn->mString)
    {
        free(strCtn->mString);
        strCtn->mString = NULL;
    }
    memset(strCtn, 0x00, sizeof(StringCtn));
    free(strCtn);
    memcpy(arg, &null_ptr, sizeof(arg));
    CDX_LOGV("Free StringContainer finish...");
    return ktrue;
}
//
