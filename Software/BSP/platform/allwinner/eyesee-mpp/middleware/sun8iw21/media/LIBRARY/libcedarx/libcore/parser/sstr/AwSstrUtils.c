/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : AwSstrUtils.c
* Description :
* History :
*   Author  : gvc-al3 <gvc-al3@allwinnertech.com>
*   Date    :
*   Comment : smooth streaming implementation.
*/

#include <AwSstrUtils.h>
#include <AwSstrParser.h>
//#include <iconv.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

/**
 * Converts a string from the given character encoding to utf-8.
 *
 * @return a nul-terminated utf-8 string, or null in case of error.
 * The result must be freed using free().
 */
char *AwFromCharset(const char *charset, const void *pData, size_t dataSize)
{
/*
    unsigned mul;
    iconv_t handle = iconv_open("UTF-8", charset);
    if (handle == (iconv_t)(-1))
    {
        CDX_LOGE("iconv open failed.");
        return NULL;
    }
    char *pOut = NULL;
    for(mul = 4; mul < 8; mul++)
    {
        size_t inSize = dataSize;
        const char *in = pData;
        size_t outMax = mul * dataSize;
        char *tmp = pOut = malloc (1 + outMax);
        if (!pOut)
        {
            CDX_LOGE("malloc failed.");
            break;
        }
        if (iconv(handle, (char **)&in, &inSize, &tmp, &outMax) != (size_t)(-1))
        {
            *tmp = '\0';
            break;
        }
        free(pOut);
        pOut = NULL;

        if (errno != E2BIG)
            break;
    }
    iconv_close(handle);
    return pOut;
*/

    CDX_UNUSE(charset);
    CDX_UNUSE(dataSize);
    return (char *)pData;
}

SmsStreamT *SmsNew(void)
{
    SmsStreamT *sms = malloc(sizeof(SmsStreamT));
    if(!sms)
    {
        CDX_LOGE("no memory.");
        return NULL;
    }
    memset(sms, 0x00, sizeof(SmsStreamT));

    sms->qlevels = SmsArrayNew();
    sms->chunks  = SmsArrayNew();
    sms->type    = SSTR_UNKNOWN;
    return sms;
}

void SmsFree(SmsStreamT *sms)
{
    int n;
    if(sms->qlevels)
    {
        for(n = 0; n < SmsArrayCount(sms->qlevels); n++)
        {
            QualityLevelT *qlevel = SmsArrayItemAtIndex(sms->qlevels, n);
            if(qlevel)
                QlFree(qlevel);
        }
        SmsArrayDestroy(sms->qlevels);
    }

    if(sms->chunks)
    {
        for(n = 0; n < SmsArrayCount(sms->chunks); n++)
        {
            ChunkT *chunk = SmsArrayItemAtIndex(sms->chunks, n);
            if(chunk)
                ChunkFree(chunk);
        }
        SmsArrayDestroy(sms->chunks);
    }

    free(sms->name);
    free(sms->urlTemplate);
    free(sms);
}


static int HexDigit(const char c)
{
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= '0' && c<= '9')
        return c - '0';
    else
        return -1;
}

cdx_uint8 *DecodeStringHexToBinary(const char *psz_src)
{
    int i = 0, j = 0, first_digit, second_digit;
    int i_len = strlen(psz_src);
    assert(i_len % 2 == 0);
    cdx_uint8 *p_data = malloc(i_len / 2);

    if(!p_data)
        return NULL;

    while(i < i_len)
    {
        first_digit = HexDigit(psz_src[i++]);
        second_digit = HexDigit(psz_src[i++]);
        assert(first_digit >= 0 && second_digit >= 0);
        p_data[j++] = (first_digit << 4 ) | second_digit;
    }

    return p_data;
}

QualityLevelT *QlNew(void)
{
    QualityLevelT *ql = calloc(1, sizeof(QualityLevelT ));
    if(!ql) return NULL;

    ql->Index = -1;
    return ql;
}

void QlFree(QualityLevelT *qlevel)
{
    free(qlevel->CodecPrivateData);
    free(qlevel);
}
ChunkT *ChunkNew(SmsStreamT* sms, cdx_int64 duration, cdx_int64 start_time )
{
    ChunkT *chunk = calloc(1, sizeof(ChunkT));
    if(chunk == NULL)
        return NULL;

    chunk->duration = duration;
    chunk->startTime = start_time;
    chunk->type = SSTR_UNKNOWN;
    chunk->sequence = sms->chunks->iCount;
    SmsArrayAppend(sms->chunks, chunk);
    return chunk;
}

ChunkT *ChunkGet(SmsStreamT *sms, cdx_int64 start_time)
{
    int len = SmsArrayCount(sms->chunks);
    int i;
    for(i = 0; i < len; i++)
    {
        ChunkT *chunk = SmsArrayItemAtIndex(sms->chunks, i);
        if(!chunk)
            return NULL;

        if(chunk->startTime <= start_time && chunk->startTime + chunk->duration > start_time)
        {
            return chunk;
        }
    }
    return NULL;
}

void ChunkFree(ChunkT *chunk)
{
    if(chunk->data)
    {
        free(chunk->data);
        chunk->data = NULL;
    }
    free(chunk);
}

QualityLevelT *GetQlevel(SmsStreamT *sms, const unsigned qid)
{
    QualityLevelT *qlevel = NULL;
    unsigned i;
    for(i = 0; i < sms->qlevelNb; i++)
    {
        qlevel = SmsArrayItemAtIndex(sms->qlevels, i);
        if(qlevel->id == qid)
            return qlevel;
    }
    return NULL;
}

SmsQueueT *SmsQueueInit(const int length)
{
    SmsQueueT *ret = malloc(sizeof(SmsQueueT));
    if(!ret)
        return NULL;
    ret->length = length;
    ret->first = NULL;
    return ret;
}

void SmsQueueFree(SmsQueueT* queue)
{
    ItemT *item = queue->first, *next = NULL;
    while(item)
    {
        next = item->next;
        free(item);
        item = next;
    }
    free(queue);
}

int SmsQueuePut(SmsQueueT *queue, const cdx_uint64 value)
{
    /* Remove the last (and oldest) item */
    ItemT *item, *prev = NULL;
    int count = 0;
    for(item = queue->first; item != NULL; item = item->next)
    {
        count++;
        if(count == queue->length)
        {
            free(item);
            if(prev)
                prev->next = NULL;
            break;
        }
        else
            prev = item;
    }

    /* Now insert the new item */
    ItemT *newItem = malloc(sizeof(ItemT));
    if(!newItem)
        return -1;

    newItem->value = value;
    newItem->next = queue->first;
    queue->first = newItem;

    return 0;
}

cdx_uint64 SmsQueueAvg(SmsQueueT *queue)
{
    ItemT *last = queue->first;
    int i;
    if(last == NULL)
        return 0;

    cdx_uint64 sum = queue->first->value;
    for(i = 0; i < queue->length - 1; i++ )
    {
        if(last)
        {
            last = last->next;
            if(last)
                sum += last->value;
        }
    }
    return sum / queue->length;
}

SmsStreamT * SmsGetStreamByCat( SmsArrayT *streams, int i_cat )
{
    SmsStreamT *ret = NULL;
    int i;
    int count = SmsArrayCount(streams);
    assert(count >= 0 && count <= 3);

    for(i = 0; i < count; i++)
    {
        ret = SmsArrayItemAtIndex(streams, i);
        if(ret->type == i_cat)
            return ret;
    }
    return NULL;
}

int EsCatToIndex(int i_cat)
{
    switch(i_cat)
    {
        case SSTR_VIDEO:
            return 0;
        case SSTR_AUDIO:
            return 1;
        case SSTR_TEXT:
            return 2;
        default:
            return -1;
    }
}

int IndexToEsCat(int index)
{
    switch(index)
    {
        case 0:
            return SSTR_VIDEO;
        case 1:
            return SSTR_AUDIO;
        case 2:
            return SSTR_TEXT;
        default:
            return -1;
    }
}

char *CdxStrToKr(char *pStr, const char *pDelim, char **ppSavePtr)
{
    char *flag;

    if (NULL == pStr)
        pStr = *ppSavePtr;

    /* Scan leading delimiters. */
    pStr += strspn (pStr, pDelim);//* 返回字符串中第一个不在指定字符串中出现的字符下标
    if (*pStr == '\0')
        return NULL;

    /* Find the end of the token. */
    flag = pStr;
    //* 依次检验字符串s1中的字符，当被检验字符在字符串s2中也包含时，则停止检验，并返回该字符位置,
    //* 空字符NULL不包括在内。
    pStr = strpbrk(flag, pDelim);
    if (pStr == NULL)
        /* This token finishes the string. */
        *ppSavePtr = strchr (flag, '\0');
    else
    {
        /* Terminate the token and make *SAVE_PTR point past it. */
        *pStr = '\0';
        *ppSavePtr = pStr + 1;
    }
    return flag;
}
