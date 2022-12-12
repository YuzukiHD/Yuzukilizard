/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : M3U9Parser.c
 * Description : M3U9Parser
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "M3U9Parser"
#include "M3U9Parser.h"
#include <cdx_log.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static inline int startsWith(const char* str, const char* prefix)
{
    return !strncmp(str, prefix, strlen(prefix));
}

PlaylistItem *findItemBySeqNumForM3u9(Playlist *playlist, int seqNum)
{
    PlaylistItem *item = playlist->mItems;
    while(item->seqNum != seqNum)
    {
        item = item->next;
    }
    return item;
}
PlaylistItem *findItemByIndexForM3u9(Playlist *playlist, int index)
{
    PlaylistItem *item = playlist->mItems;
    int j;
    for(j=0; j<index; j++)
    {
        if(!item)
        {
            break;
        }
        item = item->next;
    }
    return item;
}


//***********************************************************//
/* baseURL,url都是标准的字符串，即必须以‘\0’结束，否则这里的strstr等查找无法终止*/
/* 这里如果baseURL，url前面是一些前导的空格，则strncasecmp函数会出错，*/
/* 所以最好在一开始裁剪前面的空格*/
//***********************************************************//
static status_t MakeURL(const char *baseURL, const char *url, char **out)
{
    *out = NULL;
    if(strncasecmp("http://", baseURL, 7)
             && strncasecmp("https://", baseURL, 8)
             && strncasecmp("file://", baseURL, 7))
    {
        /*Base URL must be absolute*/
        return err_URL;
    }
    
    cdx_uint32 urlLen = strlen(url);
    if (!strncasecmp("http://", url, 7) || !strncasecmp("https://", url, 8))
    {
        /*"url" is already an absolute URL, ignore base URL.*/
        *out = malloc(urlLen + 1);
        if(!*out)
        {
            CDX_LOGE("err_no_memory");
            return err_no_memory;
        }
        memcpy(*out, url, urlLen + 1);
        return OK;
    }

    cdx_uint32 memSize = 0;
    char *temp;
    char *protocolEnd = strstr(baseURL, "//") + 2;/*为了屏蔽http://，https://之间的差异*/

    if (url[0] == '/')
    {
         /*URL is an absolute path.*/
        char *pPathStart = strchr(protocolEnd, '/');

        if (pPathStart != NULL)
        {
            memSize = pPathStart - baseURL + urlLen + 1;
            temp = (char *)malloc(memSize);
            if (temp == NULL)
            {
                CDX_LOGE("err_no_memory");
                return err_no_memory;
            }
            memcpy(temp, baseURL, pPathStart - baseURL);
            memcpy(temp + (pPathStart - baseURL), url, urlLen + 1);/*url是以'\0'结尾的*/
        }
        else
        {
            cdx_uint32 baseLen = strlen(baseURL);
            memSize = baseLen + urlLen + 1;
            temp = (char *)malloc(memSize);
            if (temp == NULL)
            {
                CDX_LOGE("err_no_memory");
                return err_no_memory;
            }
            memcpy(temp, baseURL, baseLen);
            memcpy(temp + baseLen, url, urlLen+1);
        }
    }
    else
    {
         /* URL is a relative path*/
        cdx_uint32 n = strlen(baseURL);
        char *slashPos = strstr(protocolEnd, ".m3u");
        if(slashPos != NULL)
        {
            while(slashPos >= protocolEnd)
            {
                slashPos--;
                if(*slashPos == '/')
                    break;
            }
            if (slashPos >= protocolEnd)/*找到*/
            {
                memSize = slashPos - baseURL + urlLen + 2;
                temp = (char *)malloc(memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return err_no_memory;
                }
                memcpy(temp, baseURL, slashPos - baseURL);
                *(temp+(slashPos - baseURL))='/';
                memcpy(temp+(slashPos - baseURL)+1, url, urlLen + 1);
            }
            else
            {
                memSize= n + urlLen + 2;
                temp = (char *)malloc(memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return err_no_memory;
                }
                memcpy(temp, baseURL, n);
                *(temp + n)='/';
                memcpy(temp + n + 1, url, urlLen + 1);
            }
        }
        else if (baseURL[n - 1] == '/')
        {
            memSize = n + urlLen + 1;
            temp = (char *)malloc(memSize);
            if (temp == NULL)
            {
                CDX_LOGE("err_no_memory");
                return err_no_memory;
            }
            memcpy(temp, baseURL, n);
            memcpy(temp + n, url, urlLen + 1);
        }
        else
        {
            slashPos = strrchr(protocolEnd, '/');

            if (slashPos != NULL)/*找到*/
            {
                memSize = slashPos - baseURL + urlLen + 2;
                temp = (char *)malloc(memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return err_no_memory;
                }
                memcpy(temp, baseURL, slashPos - baseURL);
                *(temp+(slashPos - baseURL))='/';
                memcpy(temp+(slashPos - baseURL)+1, url, urlLen + 1);
            }
            else
            {
                memSize= n + urlLen + 2;
                temp = (char *)malloc(memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return err_no_memory;
                }
                memcpy(temp, baseURL, n);
                *(temp + n)='/';
                memcpy(temp + n + 1, url, urlLen + 1);
            }
        }
    }
    *out = temp;
    return OK;
}

void destoryM3u9Playlist(Playlist *playList)
{
    if(playList)
    {
        if(playList->mBaseURI)
        {
            free(playList->mBaseURI);
        }

        PlaylistItem *e, *p;
        p = playList->mItems;
        while (p)
        {
            e = p->next;
            
            if (p->mURI)
            {
                free(p->mURI);
            }
            free(p);
            p = e;
        }
        
        free(playList);
    }
    return ;
}

static inline status_t ParseDouble(const char *s, double *x)
{
    char *end;
    double dVal = strtod(s, &end);

    if (!strcmp(end, s) || (*end != '\0' && *end != ','))
        return ERROR_MALFORMED;

    *x = dVal;
    return OK;
}

static status_t parseDuration(const char *line, int64_t *durationUs)
{
    char *colonPos = strstr(line, ":");
    
    if (colonPos == NULL)
        return ERROR_MALFORMED;

    double x;
    status_t err = ParseDouble(colonPos + 1, &x);

    if (err != OK)
        return err;

    *durationUs = (int64_t)(x * 1E6);
    return OK;
}

/*判断是否m3u9文件*/
bool M3u9Probe(const char *data, cdx_uint32 size)
{
    cdx_uint32 offset = 0;
    while (offset < size && isspace(data[offset])) //isspace中包含‘\n’\r,所以已经排除了空行的情况
    {
        offset++;
    }
    if(offset >= size)
    {
        return false;
    }
    cdx_uint32 nOffsetLF = offset;
    while (nOffsetLF < size && data[nOffsetLF] != '\n')
    {
        ++nOffsetLF;
    }
    if(nOffsetLF >= size)
    {
        return false;
    }
    cdx_uint32 offsetData = nOffsetLF;
    
    while(isspace(data[offsetData-1]))
    {
        --offsetData;
    }/*offsetData的前一个位置data[offsetData-1]是有效字符，不会是'\r'和'\n'，
    data[offsetData]是'\r'或'\n'，offsetData - offset是有效字符的个数*/
    if(offsetData - offset != 9)
    {
        return false;
    }
    
    return !strncmp(data + offset, "#HISIPLAY", 9);
}


//***********************************************************//
/* 解析_data所指向的已下载到的m3u9文件，需解析的大小为size*/
/* baseURI所指字符串的内存是在外部开辟的,必须以‘\0’结束,程序中不会改变char *baseURI*/
/* 如果parse出错，相应内存已经被释放*/
//***********************************************************//
status_t M3u9Parse(const void *_data, cdx_uint32 size, Playlist **P, const char *baseURI)
{
    const char *data = (const char *)_data;
    cdx_uint32 offset = 0;
    int seqNum = 0, lineNo = 0;
    status_t err = OK;
    int64_t durationUs = 0;
    char *line = NULL;
    PlaylistItem *item = NULL;
    
    Playlist *playList = malloc(sizeof(Playlist));
    if(!playList)
    {
        CDX_LOGE("err_no_memory");
        return err_no_memory;
    }
    memset(playList, 0x00, sizeof(Playlist));
    
    CDX_LOGV("baseURI=%s", baseURI);
    int baseLen = strlen(baseURI);
    playList->mBaseURI = malloc(baseLen+1);
    if(!playList->mBaseURI)
    {
        CDX_LOGE("err_no_memory");
        err = err_no_memory;
        goto _err;
    }
    memcpy(playList->mBaseURI, baseURI, baseLen+1);
    
    while(offset < size)
    {
        while(offset < size && isspace(data[offset]))
            //isspace中包含‘\n’\r,所以已经排除了空行的情况
        {
            offset++;
        }
        if(offset >= size)
        {
            break;
        }
        cdx_uint32 nOffsetLF = offset;
        while (nOffsetLF < size && data[nOffsetLF] != '\n')
        {
            ++nOffsetLF;
        }
        /*去掉这段code,以兼容最后一行不以'\n'结束的情况
        if(offsetLF >= size)
        {
            break;
        }*/
        cdx_uint32 offsetData = nOffsetLF;
        
        while(isspace(data[offsetData-1]))
        {
            --offsetData;
        }/*offsetData的前一个位置data[offsetData-1]是有效字符，不会是'\r'和'\n'，
        data[offsetData]是'\r'或'\n'，offsetData - offset是有效字符的个数*/
        if ((offsetData - offset)<=0)        /*说明是空行*/
        {
            offset = nOffsetLF + 1;
            continue;
        }
        else
        {
            line = (char *)malloc(offsetData - offset + 1);
            if(!line)
            {
                CDX_LOGE("err_no_memory");
                err = err_no_memory;
                goto _err;
            }
            memcpy(line, &data[offset], offsetData - offset);
            /*从data[offset]读到data[offsetData-1]构成一行*/
            line[offsetData - offset] = '\0';
            CDX_LOGI("#%s#", line);
        }

        if (lineNo == 0)
        {
            if (strcmp(line, "#HISIPLAY"))
            {
                CDX_LOGE("lineNo == 0, but line != #HISIPLAY");
                err = ERROR_MALFORMED;
                goto _err;
            }
        }
        else
        {
            if (startsWith(line,"#HISIPLAY_STREAM"))
            {
                err = parseDuration(line, &durationUs);
            }
            else if (startsWith(line,"#HISIPLAY_ENDLIST"))
            {
                //break;否则line没有释放
            }

            if (err != OK)
            {
                CDX_LOGE("err = %d", err);
                goto _err;
            }
        }

        if (!startsWith(line,"#")) /*不是空行，不是标签，不是注释，是URL*/
        {
            item= (PlaylistItem*)malloc(sizeof(PlaylistItem));
            if (!item)
            {
                CDX_LOGE("err_no_memory");
                err = err_no_memory;
                goto _err;
            }
            memset(item, 0, sizeof(PlaylistItem));
            if(durationUs > 0)
            {
                item->durationUs = durationUs;
                item->seqNum = seqNum;
                item->baseTimeUs = playList->durationUs;
                playList->durationUs += durationUs;/*计算片长*/
                playList->lastSeqNum = seqNum;
            }
            else
            {
                CDX_LOGE("ERROR_MALFORMED");
                err = ERROR_MALFORMED;
                goto _err;
            }
            err = MakeURL(playList->mBaseURI, line, &item->mURI);
            if (err != OK)
            {
                CDX_LOGE("err = %d", err);
                goto _err;
            }
            //item->next = NULL;//memset已经清零了
            if (playList->mItems == NULL)
            {
                playList->mItems = item;
            }
            else
            {
                PlaylistItem *item2 = playList->mItems;
                while(item2->next != NULL)
                    item2 = item2->next;
                item2->next = item;
            }
            (playList->mNumItems)++;

            seqNum++;
            durationUs = 0;
            item = NULL;
            
        }

        free(line);
        line = NULL;
        offset = nOffsetLF + 1;
        ++lineNo;
    }
    if(playList->mNumItems <= 0)
    {
        CDX_LOGE("playList->mNumItems <= 0");
        goto _err;
    }
    *P = playList;
    return OK;
    
_err:
    if(item != NULL)
    {
        free(item);
    }
    if(line != NULL)
    {
        free(line);
    }
    destoryM3u9Playlist(playList);
    return err;
    
}
