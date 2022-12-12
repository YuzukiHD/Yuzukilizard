/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : PlaylistParser.c
 * Description : parse playlist.
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "PlaylistParser"
#include "PlaylistParser.h"
#include <cdx_log.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static inline int startsWith(const char* str, const char* prefix)
{
    return !strncmp(str, prefix, strlen(prefix));
}

PlaylistItem *findItemBySeqNumForPlaylist(Playlist *playlist, int seqNum)
{
    PlaylistItem *item = playlist->mItems;
    while(item->seqNum != seqNum)
    {
        item = item->next;
    }
    return item;
}
PlaylistItem *findItemByIndexForPlaylist(Playlist *playlist, int index)
{
    PlaylistItem *item = playlist->mItems;
    int j;
    for(j=0; j<index; j++)
    {
/*
        if(!item)
        {
            break;
        }*/
        item = item->next;
    }
    return item;
}

//***********************************************************//
/* baseURL,url都是标准的字符串，即必须以‘\0’结束，否则这里的strstr等查找无法终止*/
/* 这里如果baseURL，url前面是一些前导的空格，则strncasecmp函数会出错，所以最好在一开始裁剪前面的空格*/
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

    if (!strncasecmp("http://", url, 7) || !strncasecmp("https://", url, 8))
    {
        /*"url" is already an absolute URL, ignore base URL.*/
        *out = malloc(strlen(url) + 1);
        if(!*out)
        {
            CDX_LOGE("err_no_memory");
            return err_no_memory;
        }
        memcpy(*out, url, strlen(url) + 1);
        return OK;
    }

    cdx_uint32 memSize = 0;
    char *temp;
    char *protocolEnd = strstr(baseURL, "//") + 2;/*为了屏蔽http://，https://之间的差异*/

    if (url[0] == '/')
    {
         /*URL is an absolute path.*/
        char *pathStart = strchr(protocolEnd, '/');

        if (pathStart != NULL)
        {
            memSize = pathStart - baseURL + strlen(url) + 1;
            temp = (char *)malloc(memSize);
            if (temp == NULL)
            {
                CDX_LOGE("err_no_memory");
                return err_no_memory;
            }
            memcpy(temp, baseURL, pathStart - baseURL);
            memcpy(temp + (pathStart - baseURL), url, strlen(url) + 1);/*url是以'\0'结尾的*/
        }
        else
        {
            memSize = strlen(baseURL) + strlen(url) + 1;
            temp = (char *)malloc(memSize);
            if (temp == NULL)
            {
                CDX_LOGE("err_no_memory");
                return err_no_memory;
            }
            memcpy(temp, baseURL, strlen(baseURL));
            memcpy(temp + strlen(baseURL), url, strlen(url)+1);
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
                memSize = slashPos - baseURL + strlen(url) + 2;
                temp = (char *)malloc(memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return err_no_memory;
                }
                memcpy(temp, baseURL, slashPos - baseURL);
                *(temp+(slashPos - baseURL))='/';
                memcpy(temp+(slashPos - baseURL)+1, url, strlen(url) + 1);
            }
            else
            {
                memSize= n + strlen(url) + 2;
                temp = (char *)malloc(memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return err_no_memory;
                }
                memcpy(temp, baseURL, n);
                *(temp + n)='/';
                memcpy(temp + n + 1, url, strlen(url) + 1);
            }

        }
        else if (baseURL[n - 1] == '/')
        {
            memSize = n + strlen(url) + 1;
            temp = (char *)malloc(memSize);
            if (temp == NULL)
            {
                CDX_LOGE("err_no_memory");
                return err_no_memory;
            }
            memcpy(temp, baseURL, n);
            memcpy(temp + n, url, strlen(url) + 1);
        }
        else
        {
            slashPos = strrchr(protocolEnd, '/');

            if (slashPos != NULL)/*找到*/
            {
                memSize = slashPos - baseURL + strlen(url) + 2;
                temp = (char *)malloc(memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return err_no_memory;
                }
                memcpy(temp, baseURL, slashPos - baseURL);
                *(temp+(slashPos - baseURL))='/';
                memcpy(temp+(slashPos - baseURL)+1, url, strlen(url) + 1);
            }
            else
            {
                memSize= n + strlen(url) + 2;
                temp = (char *)malloc(memSize);
                if (temp == NULL)
                {
                    CDX_LOGE("err_no_memory");
                    return err_no_memory;
                }
                memcpy(temp, baseURL, n);
                *(temp + n)='/';
                memcpy(temp + n + 1, url, strlen(url) + 1);
            }

        }
    }
    *out = temp;
    return OK;
}

void destoryPlaylist_P(Playlist *playList)
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

/*判断是否Playlist文件*/
bool PlaylistProbe(const char *data, cdx_uint32 size)
{
    cdx_uint32 offset = 0;
    while (offset < size && isspace(data[offset])) //isspace中包含‘\n’\r,所以已经排除了空行的情况
    {
        offset++;
    }
    if(offset >= size || size - offset < 8)
    {
        return false;
    }
    return startsWith(data + offset, "http://")||startsWith(data + offset, "https://")||
        startsWith(data + offset, "file://");
}

status_t PlaylistParse(const void *_data, cdx_uint32 size, Playlist **P, const char *baseURI)
{
    const char *data = (const char *)_data;
    cdx_uint32 offset = 0;
    int seqNum = 0;
    status_t err = OK;
    char *line = NULL;

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
        cdx_uint32 offsetLF = offset;
        while (offsetLF < size && data[offsetLF] != '\n')
        {
            ++offsetLF;
        }
        /*
        if(offsetLF >= size)
        {
            break;
        }*/
        cdx_uint32 offsetData = offsetLF;

        while(isspace(data[offsetData-1]))
        {
            --offsetData;
        }/*offsetData的前一个位置data[offsetData-1]是有效字符，
        不会是'\r'和'\n'，data[offsetData]是'\r'或'\n'，offsetData - offset是有效字符的个数*/
        if ((offsetData - offset)<=0)        /*说明是空行*/
        {
            offset = offsetLF + 1;
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

//TODO

        PlaylistItem *item= (PlaylistItem*)malloc(sizeof(PlaylistItem));
        if (!item)
        {
            CDX_LOGE("err_no_memory");
            err = err_no_memory;
            goto _err;
        }
        memset(item, 0, sizeof(PlaylistItem));
//
        item->seqNum = seqNum;
        playList->lastSeqNum = seqNum;
//
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

        free(line);
        line = NULL;
        offset = offsetLF + 1;
    }
    if(playList->mNumItems <= 0)
    {
        CDX_LOGE("playList->mNumItems <= 0");
        goto _err;
    }
    *P = playList;
    return OK;

_err:
    if(line != NULL)
    {
        free(line);
    }
    destoryPlaylist_P(playList);
    return err;

}

