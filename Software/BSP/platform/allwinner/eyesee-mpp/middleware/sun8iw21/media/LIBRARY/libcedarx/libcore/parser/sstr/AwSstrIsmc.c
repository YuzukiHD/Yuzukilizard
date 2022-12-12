/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : AwSstrIsmc.c
* Description :
* History :
*   Author  : gvc-al3 <gvc-al3@allwinnertech.com>
*   Date    :
*   Comment : smooth streaming implementation.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <assert.h>
#include <AwSstrUtils.h>

#define TIMESCALE 10000000

static cdx_int32 IsmcFragmentElementExtract(xmlNodePtr node, SmsStreamT *sms, cdx_int32 loopCount,
                                            cdx_bool *bWeird, cdx_int64 *computedStartTime,
                                            cdx_int64 *computedDuration)
{
    cdx_int64 startTime = -1, duration = -1;

    xmlAttrPtr attrPtr = node->properties;
    while(attrPtr != NULL)
    {
        if(!strcmp((char *)attrPtr->name, "t"))
            startTime = strtoull((char *)attrPtr->children->content, NULL, 10);
        else if(!strcmp((char *)attrPtr->name, "d"))
            duration = strtoull((char *)attrPtr->children->content, NULL, 10);

        attrPtr = attrPtr->next;
    }
    if(startTime == -1)
    {
        assert(duration != -1);
        (*computedStartTime) += (*computedDuration);
        *computedDuration  = duration;
    }
    else if(duration == -1)
    {
        assert(startTime != -1);
        /* Handle weird Manifests which give only the start time
             * of the first segment. In those cases, we have to look
             * at the start time of the second segment to compute
             * the duration of the first one. */
        if(loopCount == 1)
        {
            *bWeird = 1;
            *computedStartTime = startTime;
            return 0;
        }
        *computedDuration = startTime - (*computedStartTime);
        if(!(*bWeird))
            *computedStartTime = startTime;
    }
    else
    {
        if(*bWeird)
            *computedDuration = startTime - (*computedStartTime);
        else
        {
            *computedStartTime = startTime;
            *computedDuration = duration;
        }
    }

    if(ChunkNew(sms, *computedDuration, *computedStartTime) == NULL)
    {
        CDX_LOGE("No memory.");
        return -1;
    }
    if(bWeird && startTime != -1) //* for weird case, compute last fragment's duration.
        *computedStartTime = startTime;

    return 0;
}
typedef struct WfTagToFccS
{
    cdx_uint16     i_tag;
    cdx_uint32     i_fourcc;
}WfTagToFccT;

WfTagToFccT waveFormatTagToFourcc[] =
{
    {0x0160 /* WMA version 1 */,            SSTR_FOURCC('W','M','A','1')},
    {0x0161 /* WMA(v2) 7, 8, 9 Series */,      SSTR_FOURCC('W','M','A','2')},
    {0x0162 /* WMA 9 Professional */,         SSTR_FOURCC('W','M','A','P')},
    {0x0163 /* WMA 9 Lossless */,           SSTR_FOURCC('W','M','A','L')},

    {0x00FF /* AAC */,                  SSTR_FOURCC('m','p','4','a')},
    {0x4143 /* Divio's AAC */,             SSTR_FOURCC('m','p','4','a')},
    {0x1601 /* Other AAC */,              SSTR_FOURCC('m','p','4','a')},
    {0x1602 /* AAC/LATM */,               SSTR_FOURCC('m','p','4','a')},
    {0x706d,                         SSTR_FOURCC('m','p','4','a')},
    {0xa106 /* Microsoft AAC */,           SSTR_FOURCC('m','p','4','a')},
    {0, 0}
};

static void WfTagToFourcc(cdx_uint16 i_tag, cdx_uint32 *fcc)
{
    int i;
    for(i = 0; waveFormatTagToFourcc[i].i_tag != 0; i++)
    {
        if(waveFormatTagToFourcc[i].i_tag == i_tag) break;
    }
    if(fcc) *fcc = waveFormatTagToFourcc[i].i_fourcc;
    return;
}

static cdx_int32 IsmcQualityLevelExtract(xmlNodePtr node, SmsStreamT *sms, cdx_uint32 qid)
{
    QualityLevelT *ql = NULL;
    cdx_uint8 *waveFormatEx;

    ql = QlNew();
    if(!ql)
    {
        CDX_LOGE("no memory.");
        return -1;
    }
    ql->id = qid;
    xmlAttrPtr attrPtr = node->properties;
    while(attrPtr != NULL)
    {
        //CDX_LOGD("attrPtr->name=%s", attrPtr->name);
        if(!strcmp((char *)attrPtr->name, "Index"))
            ql->Index = strtol((char *)attrPtr->children->content, NULL, 10);
        else if(!strcmp((char *)attrPtr->name, "Bitrate"))
            ql->Bitrate = strtoull((char *)attrPtr->children->content, NULL, 10);
        else if(!strcmp((char *)attrPtr->name, "PacketSize"))
            ql->nBlockAlign = strtoull((char *)attrPtr->children->content, NULL, 10);
        else if(!strcmp((char *)attrPtr->name, "FourCC"))
            ql->FourCC = SSTR_FOURCC(attrPtr->children->content[0],
                                     attrPtr->children->content[1],
                                     attrPtr->children->content[2],
                                     attrPtr->children->content[3]);
        else if(!strcmp((char *)attrPtr->name, "CodecPrivateData"))
            ql->CodecPrivateData = strdup((char *)attrPtr->children->content);
        else if(!strcmp((char *)attrPtr->name, "WaveFormatEx"))
        {
            waveFormatEx = DecodeStringHexToBinary((char *)attrPtr->children->content);
            //* cbSize: Codec Specific Data Size
            //cdx_uint16 dataLen = ((cdx_uint16 *)waveFormatEx)[8];
            //ql->CodecPrivateData = strndup((char *)attrPtr->children->content+36, dataLen*2);
            ql->CodecPrivateData = strdup((char *)attrPtr->children->content);
/******************************
typedef struct
{
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize; //Codec Specific Data Size
    ... //Codec Specific Data
} WAVEFORMATEX; *PWAVEFORMATEX;
*******************************/
            cdx_uint16 wfTag = ((cdx_uint16 *)waveFormatEx)[0];
            //CDX_LOGD("wftag=0x%x, (char *)attrPtr->children->content=[%s]",
                      //wfTag, (char *)attrPtr->children->content);
            WfTagToFourcc(wfTag, &ql->FourCC);
            //CDX_LOGD("ql->FourCC=%x", ql->FourCC);
            ql->WfTag         = ((cdx_uint16 *)waveFormatEx)[0];
            ql->Channels      = ((cdx_uint16 *)waveFormatEx)[1];
            ql->SamplingRate  = ((cdx_uint32 *)waveFormatEx)[1];
            ql->nBlockAlign   = ((cdx_uint16 *)waveFormatEx)[6];
            ql->BitsPerSample = ((cdx_uint16 *)waveFormatEx)[7];
            ql->cbSize        = ((cdx_uint16 *)waveFormatEx)[8];
            free(waveFormatEx);
        }
        else if(!strcmp((char *)attrPtr->name, "MaxWidth") ||
                !strcmp((char *)attrPtr->name, "Width"))
            ql->MaxWidth = strtoul((char *)attrPtr->children->content, NULL, 10);
        else if(!strcmp((char *)attrPtr->name, "MaxHeight") ||
                !strcmp((char *)attrPtr->name, "Height"))
            ql->MaxHeight = strtoul((char *)attrPtr->children->content, NULL, 10);
        else if(!strcmp((char *)attrPtr->name, "Channels"))
            ql->Channels = strtoul((char *)attrPtr->children->content, NULL, 10);
        else if(!strcmp((char *)attrPtr->name, "SamplingRate"))
            ql->SamplingRate = strtoul((char *)attrPtr->children->content, NULL, 10);
        else if(!strcmp((char *)attrPtr->name, "BitsPerSample"))
            ql->BitsPerSample = strtoul((char *)attrPtr->children->content, NULL, 10);

        attrPtr = attrPtr->next;
    }

    SmsArrayAppend(sms->qlevels, ql);
    return 0;
}
static cdx_int32 IsmcStreamIndexExtract(xmlNodePtr node, AwIsmcT *ismc, cdx_uint32 trackId)
{
    SmsStreamT *sms = NULL;
    cdx_uint32 nextQid = 1;

    cdx_int32 loopCount = 0;
    cdx_int64 computedStartTime = 0;
    cdx_int64 computedDuration = 0;
    cdx_bool  bWeird = 0;

    sms = SmsNew();
    if(NULL == sms)
    {
        CDX_LOGE("NO memory.");
        return -1;
    }

    sms->id = trackId;
    xmlAttrPtr attrPtr = node->properties;
    while(attrPtr != NULL)
    {
        //CDX_LOGD("attrPtr->name=%s", attrPtr->name);
        if(!strcmp((char *)attrPtr->name, "Type"))
        {
            if(!strcmp((char *)attrPtr->children->content, "video"))
            {
                sms->type = SSTR_VIDEO;
                ismc->hasVideo = 1;
            }
            else if(!strcmp((char *)attrPtr->children->content, "audio"))
            {
                sms->type = SSTR_AUDIO;
                ismc->hasAudio = 1;
            }
            else if(!strcmp((char *)attrPtr->children->content, "text"))
            {
                sms->type = SSTR_TEXT;
                ismc->hasSubtitle = 1;
            }
            else
            {
                CDX_LOGW("Type: attrPtr->children->content(%s)", attrPtr->children->content);
            }
        }
        else if(!strcmp((char *)attrPtr->name, "Name"))
            sms->name = strdup((char *)attrPtr->children->content);
        else if(!strcmp((char *)attrPtr->name, "TimeScale"))
            sms->timeScale = strtoull((char *)attrPtr->children->content, NULL, 10);
        //else if(!strcmp((char *)attrPtr->name, "FourCC"))
        else if(!strcmp((char *)attrPtr->name, "Subtype"))
            sms->defaultFourCC = SSTR_FOURCC(attrPtr->children->content[0],
                                             attrPtr->children->content[1],
                                             attrPtr->children->content[2],
                                             attrPtr->children->content[3]);
        else if(!strcmp((char *)attrPtr->name, "Chunks"))
        {
            sms->vodChunksNb = strtol((char *)attrPtr->children->content, NULL, 10);
            if(sms->vodChunksNb == 0) // live
            {
                sms->vodChunksNb = SSTR_UINT32_MAX;
            }
        }
        else if(!strcmp((char *)attrPtr->name, "QualityLevels"))
            sms->qlevelNb = strtoul((char *)attrPtr->children->content, NULL, 10);
        else if(!strcmp((char *)attrPtr->name, "Url"))
            sms->urlTemplate = strdup((char *)attrPtr->children->content);

        attrPtr = attrPtr->next;
    }

    if(sms && !sms->timeScale)
        sms->timeScale = TIMESCALE;
    if(!sms->name)
    {
        if(sms->type == SSTR_VIDEO)
            sms->name = strdup("video");
        else if(sms->type == SSTR_AUDIO)
            sms->name = strdup("audio");
        else if(sms->type == SSTR_TEXT)
            sms->name = strdup("text");
    }

    // parse sub node
    node = node->children;
    while(node != NULL)
    {
        //CDX_LOGD("node->name=%s", node->name);
        //* QualityLevel
        if(!(strcmp((char *)node->name, "QualityLevel")))
        {
            if(IsmcQualityLevelExtract(node, sms, nextQid) < 0)
            {
                CDX_LOGE("StreamIndex parse failed.");
                return -1;
            }
            nextQid++;
        }
        else if(!strcmp((char *)node->name, "c")) //* c
        {
            if(IsmcFragmentElementExtract(node, sms, loopCount, &bWeird, &computedStartTime,
               &computedDuration) < 0)
            {
                CDX_LOGE("FragmentElement parse failed.");
                return -1;
            }
            loopCount++;
        }
        else
        {
            CDX_LOGV("node->name(%s)", node->name);
        }
        node = node->next;
    }

    if(bWeird)
    {
        if(!ChunkNew(sms, 0, computedStartTime)) //* sms, 0, 0
        {
            CDX_LOGE("no memory.");
            return -1;
        }
    }
    if(sms->qlevelNb == 0)
    {
        //sms->qlevels->iCount;
        //* iCount will increase while append... vlc_array_append(sms->qlevels, ql)
        sms->qlevelNb = SmsArrayCount(sms->qlevels);
    }

    SmsArrayAppend(ismc->smsStreams, sms);

    return 0;
}

#define PLAYREADY_SYSTEM_ID "9A04F079-9840-4286-AB92-E65BE0885F95"
static cdx_int32 IsmcProtectionExtract(xmlNodePtr node, AwIsmcT *ismc)
{
    node = node->children;
    while(node != NULL)
    {
        if(!(strcmp((char *)node->name, "ProtectionHeader")))
        {
            xmlAttrPtr attrPtr = node->properties;
            while(attrPtr != NULL)
            {
                if(!strcmp((char *)attrPtr->name, "SystemID"))
                {
                    if(!strcasecmp((char *)attrPtr->children->content, PLAYREADY_SYSTEM_ID))
                    {
                        CDX_LOGV("playready system");
                        ismc->protectSystem = SSTR_PROTECT_PLAYREADY;
                    }
                    else
                    {
                        CDX_LOGW("Type: attrPtr->children->content(%s)",
                                 attrPtr->children->content);
                    }
                }
                attrPtr = attrPtr->next;
            }

        }
        else
        {
            CDX_LOGV("node->name(%s)", node->name);
        }

        node = node->next;
    }
    return 0;
}

/*
* parse .ismc file from xml root node "SmoothStreamingMedia"
*
*/
static int AwIsmcExtract(xmlNodePtr node, AwIsmcT *ismc)
{
    //SmsStreamT *sms = NULL;
    cdx_uint32 nextTrackId = 1;
    if(NULL == node || NULL == ismc)
    {
        CDX_LOGE("bad param.");
        return -1;
    }

    //* SmoothStreamingMedia
    if(!strcmp((char *)node->name, "SmoothStreamingMedia"))
    {
        xmlAttrPtr attrPtr = node->properties;
        while(attrPtr != NULL)
        {
            //CDX_LOGD("attrPtr->name=%s", attrPtr->name);
            if(!strcmp((char *)attrPtr->name, "Duration"))
                ismc->vodDuration = strtoull((char *)attrPtr->children->content, NULL, 10);
            if(!strcmp((char *)attrPtr->name, "TimeScale"))
                ismc->timeScale = strtoull((char *)attrPtr->children->content, NULL, 10);

            attrPtr = attrPtr->next;
        }
        if(!ismc->timeScale)
        {
            ismc->timeScale = TIMESCALE;
        }
    }
    else
    {
        CDX_LOGE("root node is not \"SmoothStreamingMedia\"");
        return -1;
    }

    node = node->children;
    while(node != NULL)
    {
        //* StreamIndex
        if(!strcmp((char *)node->name, "StreamIndex"))
        {
            if(IsmcStreamIndexExtract(node, ismc, nextTrackId) < 0)
            {
                CDX_LOGE("StreamIndex parse failed.");
                return -1;
            }
            nextTrackId++;
        }
        else if (!strcmp((char *)node->name, "Protection"))
        {
            CDX_LOGV("Protection node");
            IsmcProtectionExtract(node, ismc);
        }
        else
        {
            //CDX_LOGW("node->name(%s)", node->name);
        }
        node = node->next;
    }
    return 0;
}

/*
* parse Client manifest file *.ismc
* buffer, size: input
* encoding: file encoding
* options:
*/
AwIsmcT *ParseIsmc(char *buffer, int size, AwIsmcT *ismc, const char *encoding, int options)
{
    xmlNodePtr rootNode;
    xmlDocPtr doc = NULL;

    if(buffer == NULL || size == 0)
    {
        CDX_LOGE("bad param, ismc file is empty.");
        return NULL;
    }

    doc = xmlReadMemory(buffer, size, NULL, encoding, options);//* read xml from buffer
    if(NULL == doc)
    {
        CDX_LOGE("read xml failed.");
        return NULL;
    }

    rootNode = xmlDocGetRootElement(doc);
    if(NULL == rootNode)
    {
        CDX_LOGE("has no root element.");
        goto err_out;
    }
    if(strcmp((const char *)rootNode->name, "SmoothStreamingMedia"))
    {
        CDX_LOGW("Root Node is not \"SmoothStreamingMedia\"");
    }

 //   ismc = malloc(sizeof(AwIsmcT));
 //   if(ismc == NULL)
 //   {
 //       CDX_LOGE("init ismc failed.");
 //       goto err_out;
 //   }

    if(AwIsmcExtract(rootNode, ismc) < 0)
    {
        CDX_LOGE("parse ismc failed.");
        goto err_out;
    }

    xmlFreeDoc(doc);
    return ismc;
err_out:
    if(ismc)
    {
        free(ismc);
    }
    if(doc)
    {
        xmlFreeDoc(doc);
    }
    return NULL;
}
