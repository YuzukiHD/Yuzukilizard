/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMpd.c
 * Description : Mpd
 * History :
 *
 */

#define LOG_NDEBUG 0
#define LOG_TAG "CdxMpd.c"       //* prefix of the printed messages.
#include <cdx_log.h>
#include "CdxMpd.h"

// !!!!!!!!!
//NOTE:  the duration is parsed by MpdParseInt, is seconds
//  the other time like "XHXXMXXS", is parsed by MpdParseDuration, is micro seconds

static unsigned int MpdParseInt(char *attr)
{
    return atoi(attr);
}

static long long int MpdParseLongInt(char *attr)
{
    long long int longint;
    sscanf(attr, "%lld", &longint);
    return longint;
}

static double MpdParseDouble(char *attr)
{
    return atof(attr);
}

static int MpdParseBool(char* attr)
{
    if(!strcmp(attr, "true")) return 1;
    if(!strcmp(attr, "1")) return 1;
    return 0;
}

static awMpdByteRange* MpdParseByteRange(char *attr)
{
    awMpdByteRange *br;
    br = (awMpdByteRange*)malloc(sizeof(awMpdByteRange));
    if(!br) return NULL;
    memset(br, 0, sizeof(awMpdByteRange));

    sscanf(attr, "%lld-%lld", &br->start_range, &br->end_range);
    return br;
}

static char* MpdParseString(char* attr)
{
    return strdup(attr);
}

static MPD_Fractional* MpdParseFrac(char *attr)
{
    MPD_Fractional *res;
    res = (MPD_Fractional*)malloc(sizeof(MPD_Fractional));
    if(!res) return NULL;

    sscanf(attr, "%d:%d", &res->num, &res->den);
    return res;
}

static unsigned int MpdParseDuration(char* duration)
{
    if(!duration) return 0;
    unsigned int i = 0;

    // cast the space in the header of duration
    while(1)
    {
        if(' ' == duration[i])
        {
            i++;
        }
        else if (0 == duration[i])
        {
            return 0;
        }
        else
        {
            break;
        }
    }

    //duration is start with "PT"
    //for example,  "PT0H1M59.91S" means 1 minutes and 59.91 seconds
    if('P' == duration[i])
    {
        if((0 == duration[i+1]) || ('T' != duration[i+1]))
        {
            return 0;
        }
        else
        {
            char *pH; //pointer to 'H'
            char *pM; //pointer to 'M'
            char *pS;
            unsigned int h = 0;  //hour
            unsigned int m = 0;  //minute
            double       s = 0.0;  //seconds

            pH = strchr(duration+i+2, 'H'); //find 'H'
            if(pH)
            {
                *pH = 0;
                h = atoi(duration+i+2); //get the hours
                *pH = 'H';
                pH ++;
            }
            else
            {
                pH = duration+i+2;
            }

            pM = strchr(pH, 'M');
            if(pM)
            {
                *pM = 0;
                m = atoi(pH);
                *pM = 'M';
                pM++;
            }
            else
            {
                pM = pH;
            }

            pS = strchr(pM, 'S');
            if(pS)
            {
                *pS = 0;
                s = atof(pM);
                *pS = 'S';
            }
            return (unsigned int)( (h*3600+m*60+s)*1000 );
        }

    }
    else
    {
        return 0;
    }
}

static int64_t MpdParseDate(char *attr)
{
    //attr = NULL;
    /*
        unsigned int nYear, nMonth, nDay, h, m, ms;
        int oh, om;
        float s;
        int64_t res;
        int ok = 0;
        int neg_time_zone = 0;
        struct tm timeS;

        nYear = nMonth = nDay = h = m  = ms = 0;
        s = 0;
        oh = om = 0;
        if (sscanf(attr, "%d-%d-%dT%d:%d:%gZ", &nYear, &nMonth, &nDay, &h, &m, &s) == 6)
            ok = 1;
        else if (sscanf(attr, "%d-%d-%dT%d:%d:%g-%d:%d",
                        &nYear, &nMonth, &nDay, &h, &m, &s, &oh, &om) == 8)
        {
            neg_time_zone = 1;
            ok = 1;
        } else if (sscanf(attr, "%d-%d-%dT%d:%d:%g+%d:%d", &nYear,
                        &nMonth, &nDay, &h, &m, &s, &oh, &om) == 8) {
            ok = 1;
        }

        if (!ok) {
            CDX_LOGD("[MPD] Failed to parse MPD date %s - format not supported\n", attr);
            return 0;
        }

        memset(&timeS, 0, sizeof(struct tm));
        timeS.tm_year = (nYear > 1900) ? nYear - 1900 : 0;
        timeS.tm_mon = nMonth ? nMonth - 1 : 0;
        timeS.tm_mday = nDay;
        timeS.tm_hour = h;
        timeS.tm_min = m;
        timeS.tm_sec = (unsigned int) s;

        int val = timezone;
        res = mktime(&timeS) - val;

        if (om || oh) {
            int diff = (60*oh + om)*60;
            if (neg_time_zone) diff = -diff;
            res = res + diff;
        }
        res *= 1000;
        ms = (cdx_uint32) ( (s - (cdx_uint32)s) * 1000);
        return res + ms;
    */
    CDX_UNUSE(attr);
    CDX_LOGI("the function MpdParseDate() is not complete.\n");
    return 0;
}

static awMpdURL* MpdParseUrl(xmlNodePtr node)
{
    awMpdURL* url;
    url = (awMpdURL*)malloc(sizeof(awMpdURL));
    if(!url) return NULL;
    memset(url, 0, sizeof(awMpdURL));

    xmlAttrPtr atrrPtr = node->properties;
    while(atrrPtr != NULL)
    {
        if(!strcmp((const char*)atrrPtr->name, "sourceURL"))
        {
            url->sourceURL= MpdParseString((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "range"))
        {
            url->byteRange = MpdParseByteRange((char*)atrrPtr->children->content);
        }
        atrrPtr = atrrPtr->next;
    }

    return url;
}

static void MpdInitCommonAttribute(MPD_CommonAttributes* com)
{
    com->audio_channels = aw_list_new();
    com->content_protection = aw_list_new();
    com->frame_packing = aw_list_new();
}

/**********************************************************/
/*                           parse the mpd                               */
/*********************************************************/
static awSegmentTimeline* SegmentTimelineTraverse(xmlNodePtr node, awMpd* mpd)
{
    awSegmentTimeline* seg;
    xmlAttrPtr atrrPtr;
    CDX_UNUSE(mpd);
    seg = (awSegmentTimeline*)malloc(sizeof(awSegmentTimeline));
    if(!seg) return NULL;
    memset(seg, 0, sizeof(awSegmentTimeline));
    seg->entries = aw_list_new();

    // parse the sub node  "S"
    node = node->children;
    while(node != NULL)
    {
        CDX_LOGD("node->name = %s.\n", node->name);
        if(!strcmp((const char*)node->name, "S"))
        {
            CDX_LOGD("Timeline: S.\n");
            awSegmentTimelineEntry* segment;
            segment = (awSegmentTimelineEntry*)malloc(sizeof(awSegmentTimelineEntry));
            if (!segment)  return NULL;
            memset(segment, 0, sizeof(awSegmentTimelineEntry));
            aw_list_add(seg->entries, segment);

            atrrPtr = node->properties;
            while (atrrPtr != NULL)
            {
                if(!strcmp((const char*)atrrPtr->name, "t"))
                {
                    segment->t = MpdParseLongInt((char*)atrrPtr->children->content);
                }
                else if(!strcmp((const char*)atrrPtr->name, "d"))
                {
                    segment->d = MpdParseInt((char*)atrrPtr->children->content);
                }
                else if(!strcmp((const char*)atrrPtr->name, "r"))
                {
                    segment->r = MpdParseInt((char*)atrrPtr->children->content);
                }

                //CDX_LOGD(" SegmentTemplate attribute: %s = %s. \n",
                //            atrrPtr->name, atrrPtr->children->content);
                atrrPtr = atrrPtr->next;
            }
        }

        node = node->next;
    }

    return seg;
}

static int ContentComponentTraverse(xmlNodePtr node, AW_List* list)
{
    CDX_LOGI("ContentComponentTraverse is not complete yet");
    CDX_UNUSE(node);
    CDX_UNUSE(list);
    return 0;
}

// parse the base url
// BaseUrl node maybe appearent in the child of MPD node, period node, Rep Node and segment node
//     @ container : the baseURL AW_List varible in these structures
static int BaseUrlTraverse(xmlNodePtr node, AW_List* container)
{
    if(    container == NULL)
    {
        return -1;
    }
    int ret;

    awBaseURL* url = (awBaseURL*)malloc(sizeof(awBaseURL));
    if(!url)
    {
        return -1;
    }
    memset(url, 0, sizeof(awBaseURL));

    ret = aw_list_add(container, url);
    if(ret < 0)
    {
        return -1;
    }

    xmlAttrPtr attrPtr = node->properties;
    while(attrPtr != NULL)
    {
        if(!strcmp((const char*)attrPtr->name, "serviceLocation"))
        {
            //CDX_LOGD("Period: href. \n");
            url->serviceLocation= MpdParseString((char*)attrPtr->children->content);
        }
        else if(!strcmp((const char*)attrPtr->name, "byteRange"))
        {
            //CDX_LOGD("Period: href. \n");
            url->byteRange= MpdParseByteRange((char*)attrPtr->children->content);
        }
        //CDX_LOGD("  attribute: %s = %s. \n", attrPtr->name, attrPtr->children->content);
        attrPtr = attrPtr->next;
    }

    // get node content
    url->URL = xmlNodeGetContent(node);

    return 0;
}

static void commonAttributeTraverse(xmlNodePtr node, MPD_CommonAttributes* com)
{
    xmlAttrPtr atrrPtr = node->properties;
    while (atrrPtr != NULL)
    {
        if(!strcmp((const char*)atrrPtr->name, "profiles"))
        {
            com->profiles = MpdParseString((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "width"))
        {
            com->width = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "height"))
        {
            com->height = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "sar"))
        {
            com->sar = MpdParseFrac((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "frameRate"))
        {
            com->framerate = MpdParseFrac((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "audioSamplingRate"))
        {
            com->samplerate = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "mimeType"))
        {
            com->mime_type = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "segmentProfiles"))
        {
            com->segmentProfiles = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "codecs"))
        {
            com->codecs = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "maximumSAPPeriod"))
        {
            com->maximum_sap_period = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "maxPlayoutRate"))
        {
            com->max_playout_rate = MpdParseDouble((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "maxPlayoutRate"))
        {
            com->coding_dependency = MpdParseBool((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "scanType"))
        {
            if (!strcmp((char*)atrrPtr->children->content, "progressive"))
            {
                    com->scan_type = AW_MPD_SCANTYPE_PROGRESSIVE;
            }
            else if (!strcmp((char*)atrrPtr->children->content, "interlaced"))
            {
                    com->scan_type = AW_MPD_SCANTYPE_INTERLACED;
            }
        }
        else if (!strcmp((const char*)atrrPtr->name, "startWithSAP"))
        {
            if(!strcmp((char*)atrrPtr->children->content, "false"))
            {
                com->starts_with_sap = 0;
            }
            else
            {
                com->starts_with_sap = MpdParseInt((char*)atrrPtr->children->content);
            }
        }
        //CDX_LOGD(" SegmentTemplate attribute: %s = %s. \n",
        //    atrrPtr->name, atrrPtr->children->content);
        atrrPtr = atrrPtr->next;
    }

    // parse the sub node
    node = node->children;
    while(node != NULL)
    {
        if(!strcmp((const char*)node->name, "FramePacking"))
        {
            ContentComponentTraverse(node, com->frame_packing);
        }
        else if (!strcmp((const char*)node->name, "AudioChannelConfiguration"))
        {
            ContentComponentTraverse(node, com->audio_channels);
        }
        else if (!strcmp((const char*)node->name, "ContentProtection"))
        {
            ContentComponentTraverse(node, com->content_protection);
        }
        node = node->next;
    }
}

static void SegmentBaseGeneric(xmlNodePtr node, awMpd* mpd, awSegmentBase* seg)
{
    seg->time_shift_buffer_depth = (unsigned int)-1;

    xmlAttrPtr atrrPtr = node->properties;
    while (atrrPtr != NULL)
    {
        if(!strcmp((const char*)atrrPtr->name, "timescale"))
        {
            seg->timescale = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "presentationTimeOffset"))
        {
            seg->presentationTimeOffset = MpdParseLongInt((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "indexRange"))
        {
            seg->indexRange = MpdParseByteRange((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "indexRangeExact"))
        {
            seg->indexRangeExact = MpdParseBool((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "availabilityTimeOffset"))
        {
            seg->availability_time_offset = MpdParseDouble((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "timeShiftBufferDepth"))
        {
            seg->time_shift_buffer_depth = MpdParseDuration((char*)atrrPtr->children->content);
        }

        //CDX_LOGD(" SegmentTemplate attribute: %s = %s. \n",
        //    atrrPtr->name, atrrPtr->children->content);
        atrrPtr = atrrPtr->next;
    }

    if(mpd->type == AW_MPD_TYPE_STATIC)    seg->time_shift_buffer_depth = 0;

    // parse the sub node
    node = node->children;
    while(node != NULL)
    {
        CDX_LOGD("node->name = %s.\n", node->name);
        if(!strcmp((const char*)node->name, "Initialization"))
        {
            //progInf->title = strdup(content);
            CDX_LOGD("Initialization.\n");
            seg->initializationSegment = MpdParseUrl(node);
        }
        else if(!strcmp((const char*)node->name, "RepresentationIndex"))
        {
            seg->representationIndex = MpdParseUrl(node);
        }

        node = node->next;
    }

}

static awSegmentBase* SegmentBaseTraverse(xmlNodePtr node, awMpd* mpd)
{
    struct _awSegmentBase* seg;
    seg = (struct _awSegmentBase*)malloc(sizeof(struct _awSegmentBase));
    if(!seg) return NULL;
    memset(seg, 0, sizeof(struct _awSegmentBase));

    SegmentBaseGeneric(node, mpd, seg);
    return seg;
}

static void MutipleSegmentBaseTraverse(xmlNodePtr node, awMpd* mpd, awSegmentTemplate* seg)
{
    SegmentBaseGeneric(node, mpd, (awSegmentBase*)seg);
    seg->startNumber = (unsigned int) -1;

    xmlAttrPtr atrrPtr = node->properties;
    while (atrrPtr != NULL)
    {
        if(!strcmp((const char*)atrrPtr->name, "duration"))
        {
            seg->duration = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "startNumber"))
        {
            seg->startNumber = MpdParseInt((char*)atrrPtr->children->content);
        }

        atrrPtr = atrrPtr->next;
    }

    // parse the sub node
    node = node->children;
    while(node != NULL)
    {
        CDX_LOGD("node->name = %s.\n", node->name);
        if(!strcmp((const char*)node->name, "SegmentTimeline"))
        {
            seg->segmentTimeline = SegmentTimelineTraverse(node, mpd);
        }
        else if(!strcmp((const char*)node->name, "BitstreamSwitching"))
        {
            seg->bitstreamSwitching_URL = MpdParseUrl(node);
        }

        node = node->next;
    }
    return;
}

static awSegmentTemplate* SegmentTemplateTraverse(xmlNodePtr node, awMpd* mpd)
{
    awSegmentTemplate* seg;

    seg = (awSegmentTemplate*)malloc(sizeof(awSegmentTemplate));
    if(!seg) return NULL;
    memset(seg, 0, sizeof(awSegmentTemplate));

    xmlAttrPtr atrrPtr = node->properties;
    while(atrrPtr != NULL)
    {
        if(!strcmp((const char*)atrrPtr->name, "media"))
        {
            seg->media = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "index"))
        {
            seg->index = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "bitstreamSwitching"))
        {
            seg->bitstreamSwitch = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "initialization"))
        {
            seg->initialization = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcasecmp((const char*)atrrPtr->name, "initialisation") ||
                 !strcasecmp((const char*)atrrPtr->name, "initialization"))
        {
            CDX_LOGD("Wornning: wrong spell, got %s but expected 'initialization' \n",
                      atrrPtr->children->content);
            seg->initialization = MpdParseString((char*)atrrPtr->children->content);
        }
        CDX_LOGD("SegmentTemplate attribute: %s = %s. \n",
                  atrrPtr->name, atrrPtr->children->content);
        atrrPtr = atrrPtr->next;
    }

    MutipleSegmentBaseTraverse(node, mpd, seg);
    return seg;
}

static int RepresentationTraverse(xmlNodePtr node, awMpd* mpd, AW_List* container)
{
    int ret;

    awRepresentation *rep;
    rep = (awRepresentation*)malloc(sizeof(awRepresentation));
    if(!rep) return -1;
    memset(rep, 0, sizeof(awRepresentation));

    rep->baseURLs = aw_list_new();
    ret = aw_list_add(container, rep);
    if(ret < 0) return ret;

    xmlAttrPtr atrrPtr = node->properties;
    while(atrrPtr != NULL)
    {
        if(!strcmp((const char*)atrrPtr->name, "id"))
        {
            rep->id = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "bandwidth"))
        {
            rep->bandwidth = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "qualityRanking"))
        {
            rep->qualityRanking = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "dependencyId"))
        {
            rep->dependencyId = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcasecmp((const char*)atrrPtr->name, "mediaStreamStructureId"))
        {
            rep->mediaStreamStructureId = MpdParseString((char*)atrrPtr->children->content);
        }
        //CDX_LOGD(" SegmentTemplate attribute: %s = %s. \n",
        //    atrrPtr->name, atrrPtr->children->content);
        atrrPtr = atrrPtr->next;
    }

    commonAttributeTraverse(node, (MPD_CommonAttributes*) rep);

    // parse the sub node
    node = node->children;
    while(node != NULL)
    {
        //CDX_LOGD("node->name = %s.\n", node->name);
        if(!strcmp((const char*)node->name, "BaseURL"))
        {
            //progInf->title = strdup(content);
            if(BaseUrlTraverse(node, rep->baseURLs) < 0)
                return -1;
        }
        else if(!strcmp((const char*)node->name, "SegmentBase"))
        {
            //todo
            rep->segmentBase = SegmentBaseTraverse(node, mpd);
        }
        else if(!strcmp((const char*)node->name, "SegmentList"))
        {
            //todo
        }
        else if(!strcmp((const char*)node->name, "SegmentTemplate"))
        {
            rep->segmentTemplate = SegmentTemplateTraverse(node, mpd);
        }

        node = node->next;
    }

    return 0;

}

//parse adaptionSet
static int AdaptionSetTraverse(xmlNodePtr node, awMpd* mpd, AW_List* adption)
{
    int ret;

    awAdaptionSet *set = (awAdaptionSet*)malloc(sizeof(awAdaptionSet));
    if(!set) return -1;
    memset(set, 0, sizeof(awAdaptionSet));

    MpdInitCommonAttribute((MPD_CommonAttributes *)set);

    set->accessiblity = aw_list_new();
    set->role = aw_list_new();
    set->rating = aw_list_new();
    set->viewpoint = aw_list_new();
    set->contentComponent = aw_list_new();
    set->baseURLs = aw_list_new();
    set->representation = aw_list_new();
    //default group
    set->group = -1;

    ret = aw_list_add(adption, set);
    if(ret < 0) return ret;

    xmlAttrPtr atrrPtr = node->properties;
    while(atrrPtr != NULL)
    {
        if(!strcmp((const char*)atrrPtr->name, "href"))
        {
            set->xlinkHref = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "actuate"))
        {
            set->xlinkActuate = !strcmp((char*)atrrPtr->children->content, "onload")? 1: 0;
        }
        else if (!strcmp((const char*)atrrPtr->name, "id"))
        {
            set->id = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "group"))
        {
            set->group = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "lang"))
        {
            set->lang = MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "contentType"))
        {
            set->contentType= MpdParseString((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "par"))
        {
            set->par = MpdParseFrac((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "minBandwidth"))
        {
            set->minBandwidth = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "maxBandwidth"))
        {
            set->maxBandwidth = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "minWidth"))
        {
            set->minWidth = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "maxWidth"))
        {
            set->maxWidth = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "minHeight"))
        {
            set->minHeight = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "maxHeight"))
        {
            set->maxHeight = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "minFrameRate"))
        {
            set->minFramerate = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "maxFrameRate"))
        {
            set->maxFramerate = MpdParseInt((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "segmentAlignment"))
        {
            set->segmentAlignment = MpdParseBool((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "bitstreamSwitching"))
        {
            set->bitstreamSwitching = MpdParseBool((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "subsegmentAlignment"))
        {
            set->subsegmentAlignment = MpdParseBool((char*)atrrPtr->children->content);
        }
        else if (!strcmp((const char*)atrrPtr->name, "subsegmentStartsWithSAP"))
        {
            if(!strcmp((char*)atrrPtr->children->content, "false"))
            {
                set->subsegmentStartsWithSAP = 0;
            }
            else
            {
                set->subsegmentStartsWithSAP = MpdParseInt((char*)atrrPtr->children->content);
            }
        }

        CDX_LOGD("  attribute: %s = %s. \n", atrrPtr->name, atrrPtr->children->content);
        atrrPtr = atrrPtr->next;
    }

    commonAttributeTraverse(node, (MPD_CommonAttributes*)set);

    // parse the sub node
    node = node->children;
    while(node != NULL)
    {
        //CDX_LOGD("node->name = %s.\n", node->name);
        if(!strcmp((const char*)node->name, "Accessibility"))
        {
            //progInf->title = strdup(content);
        }
        else if(!strcmp((const char*)node->name, "Role"))
        {
            //todo
        }
        else if(!strcmp((const char*)node->name, "Rating"))
        {
            //todo
        }
        else if(!strcmp((const char*)node->name, "Viewpoint"))
        {
            //todo
        }
        else if(!strcmp((const char*)node->name, "BaseURL"))
        {
            if(BaseUrlTraverse(node, set->baseURLs) < 0)
                return -1;
        }
        else if(!strcmp((const char*)node->name, "ContentComponent"))
        {
            //CDX_LOGD("AdaptionSet: ContentComponent. \n");
            ContentComponentTraverse(node, set->contentComponent);
        }
        else if(!strcmp((const char*)node->name, "SegmentBase"))
        {
            set->segmentBase = SegmentBaseTraverse(node, mpd);
        }
        else if(!strcmp((const char*)node->name, "SegmentList"))
        {

        }
        else if(!strcmp((const char*)node->name, "SegmentTemplate"))
        {
            //CDX_LOGD("AdaptionSet: SegmentTemplate. \n");
            set->segmentTemplate = SegmentTemplateTraverse(node, mpd);
            //todo
        }
        else if(!strcmp((const char*)node->name, "Representation"))
        {
            //CDX_LOGD("AdaptionSet: Representation. \n");
            ret = RepresentationTraverse(node, mpd, set->representation);
            if(ret < 0) return -1;
        }
        node = node->next;
    }
    return 0;
}

static int ProgramInfTraverse(xmlNodePtr node, awMpd* mpd)
{
    if(    node== NULL)
        return -1;

    awProgramInformation* progInf = (awProgramInformation*)malloc(sizeof(awProgramInformation));
    if(!progInf) return -1;
    memset(progInf, 0, sizeof(awProgramInformation));

    xmlAttrPtr atrrPtr = node->properties;
    while(atrrPtr != NULL)
    {
        if(!strcmp((const char*)atrrPtr->name, "moreInformationURL"))
        {
            CDX_LOGD("moreInformationURL. \n");
            progInf->moreInformationURL = MpdParseString((char*)atrrPtr->children->content);
        }
        else if(!strcmp((const char*)atrrPtr->name, "lang"))
        {
            progInf->lang = MpdParseString((char*)atrrPtr->children->content);
        }

        CDX_LOGD("  attribute: %s = %s. \n", atrrPtr->name, atrrPtr->children->content);
        atrrPtr = atrrPtr->next;
    }

    // parse the sub node
    node = node->children;
    while(node != NULL)
    {
        CDX_LOGD("node->name = %s.\n", node->name);
        if(!strcmp((const char*)node->name, "Title"))
        {
            //char* content = (char*)xmlNodeGetContent(node);
            //CDX_LOGD("Title content = %s\n", content);
            progInf->title = (char*)xmlNodeGetContent(node);
        }
        else if(!strcmp((const char*)node->name, "Source"))
        {
            progInf->source = (char*)xmlNodeGetContent(node);
        }
        else if(!strcmp((const char*)node->name, "Copyright"))
        {
            progInf->copyright = (char*)xmlNodeGetContent(node);
        }
        node = node->next;
    }

    //add the new proginfo to the end of list
    return aw_list_add(mpd->programInfos, progInf);
}

/*
*  parse the Period Node
*  @Properties: id , start, ...
*  @subNode: AdaptionSets
*/
static int PeriodTraverse(xmlNodePtr node, awMpd* mpd)
{
    if(    node== NULL)
        return -1;
    int ret;

    awPeriod* period;
    period = (awPeriod*)malloc(sizeof(awPeriod));
    if(!period) return -1;
    memset(period, 0, sizeof(awPeriod));

    period->adaptationSets = aw_list_new();
    period->baseURL = aw_list_new();
    period->subsets = aw_list_new();

    ret = aw_list_add(mpd->periods, period);
    if(ret < 0) return ret;

    xmlAttrPtr attrPtr = node->properties;
    while(attrPtr != NULL)
    {
        if(!strcmp((const char*)attrPtr->name, "href"))
        {
            CDX_LOGD("Period: href. \n");
            period->xlinkHref = MpdParseString((char*)attrPtr->children->content);
        }
        else if(!strcmp((const char*)attrPtr->name, "actuate"))
        {
            CDX_LOGD("Period: href. \n");
            period->xlinkActuate = !strcmp((char*)attrPtr->children->content, "onLoad") ? 1: 0;
        }
        else if(!strcmp((const char*)attrPtr->name, "id"))
        {
            CDX_LOGD("Period: id. \n");
            period->id = MpdParseString((char*)attrPtr->children->content);
        }
        else if(!strcmp((const char*)attrPtr->name, "start"))
        {
            CDX_LOGD("Period: duration. \n");
            period->start = MpdParseDuration((char*)attrPtr->children->content);
        }
        else if(!strcmp((const char*)attrPtr->name, "duration"))
        {
            CDX_LOGD("Period: duration. \n");
            period->duration = MpdParseDuration((char*)attrPtr->children->content);
        }
        else if(!strcmp((const char*)attrPtr->name, "bitstreamSwitching"))
        {
            CDX_LOGD("Period: bitstreamSwitching. \n");
            period->isBitstreamSwitching = MpdParseBool((char*)attrPtr->children->content);
        }

        //CDX_LOGD("  attribute: %s = %s. \n", attrPtr->name, attrPtr->children->content);
        attrPtr = attrPtr->next;
    }

    // parse the sub node
    node = node->children;
    while(node != NULL)
    {
        //CDX_LOGD("node->name = %s.\n", node->name);
        if(!strcmp((const char*)node->name, "BaseURL"))
        {
            CDX_LOGD("Period: BaseURL is not complete. \n");
            if(BaseUrlTraverse(node, period->baseURL) < 0)
                return -1;
        }
        else if(!strcmp((const char*)node->name, "SegmentBase"))
        {
            CDX_LOGD("Period: SegmentBase. \n");
            period->segmentBase = SegmentBaseTraverse(node, mpd);
        }
        else if(!strcmp((const char*)node->name, "SegmentList"))
        {
            CDX_LOGD("Period: SegmentList. \n");
            //todo
        }
        else if(!strcmp((const char*)node->name, "SegmentTemplate"))
        {
            CDX_LOGD("Period: SegmentTemplate. \n");
            period->segmentTemplate = SegmentTemplateTraverse(node, mpd);
        }
        else if(!strcmp((const char*)node->name, "AdaptationSet"))
        {
            CDX_LOGD("Period: AdaptationSet. \n");
            ret = AdaptionSetTraverse(node, mpd, period->adaptationSets);
        }
        else if(!strcmp((const char*)node->name, "SubSet"))
        {
            CDX_LOGD("Period: SubSet. \n");
            //todo
        }
        node = node->next;
    }

    return 0;
}

/*
*  parse the mpd file from the xml root node "MPD"
*  @Properties: "type", "id", "profiles"
*  @subnode: "ProgramInformation", "Period", "BaseURL"
*/
static int MpdTraverse(xmlNodePtr node, awMpd* mpd)
{
    if(    node== NULL)
        return -1;

    // parse the mpd node attribute
    if(!strcmp((const char*)node->name, "MPD"))
    {
        CDX_LOGD("MPD node\n");

        xmlAttrPtr atrrPtr = node->properties;
        while(atrrPtr != NULL)
        {
            if(!strcmp((const char*)atrrPtr->name, "type"))
            {
                if(!strcmp((const char*)atrrPtr->children->content, "dynamic"))
                {
                    mpd->type = AW_MPD_TYPE_DYNAMIC;
                }
                else if (!strcmp((const char*)atrrPtr->children->content, "static"))
                {
                    mpd->type = AW_MPD_TYPE_STATIC;
                }
            }
            else if (!strcmp((const char*)atrrPtr->name, "id"))
            {
                mpd->id= MpdParseString((char*)atrrPtr->children->content);
            }
            else if (!strcmp((const char*)atrrPtr->name, "profiles"))
            {
                mpd->profiles = MpdParseString((char*)atrrPtr->children->content);
            }
            else if(!strcmp((const char*)atrrPtr->name, "minBufferTime"))
            {
                mpd->minBufferTime = MpdParseDuration((char*)atrrPtr->children->content);
            }
            else if(!strcmp((const char*)atrrPtr->name, "mediaPresentationDuration"))
            {
                mpd->mediaPresentationDuration =
                        MpdParseDuration((char*)atrrPtr->children->content);
            }
            else if(!strcmp((const char*)atrrPtr->name, "minimumUpdatePeriod"))
            {
                mpd->minimumUpdatePeriod = MpdParseDuration((char*)atrrPtr->children->content);
            }
            else if(!strcmp((const char*)atrrPtr->name, "suggestedPresentationDelay"))
            {
                mpd->suggestedPresentationDelay =
                        MpdParseDuration((char*)atrrPtr->children->content);
            }
            else if(!strcmp((const char*)atrrPtr->name, "maxSegmentDuration"))
            {
                mpd->maxSegmentDuration = MpdParseDuration((char*)atrrPtr->children->content);
            }
            else if(!strcmp((const char*)atrrPtr->name, "maxSubsegmentDuration"))
            {
                mpd->maxSubsegmentDuration = MpdParseDuration((char*)atrrPtr->children->content);
            }
            else if(!strcmp((const char*)atrrPtr->name, "timeShiftBufferDepth"))
            {
                mpd->timeShiftBufferDepth = MpdParseDuration((char*)atrrPtr->children->content);
            }
            else if(!strcmp((const char*)atrrPtr->name, "availabilityStartTime"))
            {
                mpd->availabilityStartTime = MpdParseDate((char*)atrrPtr->children->content);
            }
            else if(!strcmp((const char*)atrrPtr->name, "availabilityEndTime"))
            {
                mpd->availabilityEndTime = MpdParseDate((char*)atrrPtr->children->content);
            }
            //CDX_LOGD("  attribute: %s = %s. \n", atrrPtr->name, atrrPtr->children->content);
            atrrPtr = atrrPtr->next;
        }
    }
    else
    {
        CDX_LOGW("warning: the root node is not MPD.\n");
        return -1;
    }

    //if 'ststic' minimumUpdatePeriod and timeShiftBufferDepth is not exist
    if(mpd->type == AW_MPD_TYPE_STATIC)
    {
        mpd->minimumUpdatePeriod = mpd->timeShiftBufferDepth = 0;
    }

    node = node->children;

    while(node != NULL)
    {
        if(!strcmp((const char*)node->name, "ProgramInformation"))
        {
            CDX_LOGD("ProgramInformation node\n");
            if(ProgramInfTraverse(node, mpd) < 0)
                return -1;
        }
        else if(!strcmp((const char*)node->name, "Period"))
        {
            CDX_LOGD("Period node\n");
            if(PeriodTraverse(node, mpd) < 0)
                return -1;
        }
        else if(!strcmp((const char*)node->name, "Location"))
        {
            CDX_LOGD("Location node\n");
            //if(PeriodTraverse(node, mpd) < 0)
            return -1;
        }
        else if(!strcmp((const char*)node->name, "Metrics"))
        {
            CDX_LOGD("Metrics node\n");
            //if(PeriodTraverse(node, mpd) < 0)
            return -1;
        }
        else if(!strcmp((const char*)node->name, "BaseURL"))
        {
            CDX_LOGD("BaseURL node\n");
            if(BaseUrlTraverse(node, mpd->baseURLs) < 0)
                return -1;
        }

        node = node->next;
    }

    return 0;
}

static awMpd* InitMpd()
{
    //init mpd
    awMpd* mpd = (awMpd*)malloc(sizeof(awMpd));
    if(mpd == NULL)
    {
        CDX_LOGD("mpd molloc error.\n");
        return NULL;
    }
    memset(mpd, 0, sizeof(awMpd));

    mpd->periods = aw_list_new();
    mpd->programInfos= aw_list_new();
    mpd->baseURLs = aw_list_new();
    mpd->locations = aw_list_new();
    mpd->metrics = aw_list_new();

    mpd->type = AW_MPD_TYPE_STATIC;

    return mpd;
}

/****************************************************/
/*                free mpd file                     */
/****************************************************/
static void MpdDescriptorFree(void *item)
{
    CDX_LOGD("[MPD] descriptor not implemented\n");
    free(item);
}

void MpdContentComponentFree(void *item)
{
    CDX_LOGD("[MPD] descriptor not implemented\n");
    free(item);
}

void SubRepresentationFree(void *item)
{
    CDX_LOGD("[MPD] SubRepresentationFree not implemented\n");
    free(item);
}

static void MpdUrlFree(void* item)
{
    if(!item) return;

    awMpdURL* ptr = (awMpdURL*)item;
    if(ptr->sourceURL)
    {
        free(ptr->sourceURL);
        ptr->sourceURL = NULL;
    }
    if(ptr->byteRange)
    {
        free(ptr->byteRange);
        ptr->byteRange = NULL;
    }
    free(ptr);
//    item = NULL;
}

// free the AW_List pointer in awMpd struct
static void MpdListFree(AW_List* list, void(*_free)(void*))
{
    if(!list)    return;

    while(aw_list_count(list))
    {
        void* item = aw_list_last(list);
        aw_list_rem_last(list);
        if(item && _free) _free(item);
    }

    aw_list_del(list);
}

static void ProgramInfoFree(void* item)
{
    if(!item) return;

    awProgramInformation* ptr = (awProgramInformation*)item;
    if(ptr->lang)
    {
        free(ptr->lang);
        ptr->lang = NULL;
    }
    if(ptr->title)
    {
        free(ptr->title);
        ptr->title = NULL;
    }
    if(ptr->source)
    {
        free(ptr->source);
        ptr->source = NULL;
    }
    if(ptr->copyright)
    {
        free(ptr->copyright);
        ptr->copyright = NULL;
    }
    if(ptr->moreInformationURL)
    {
        free(ptr->moreInformationURL);
        ptr->moreInformationURL = NULL;
    }
    free(ptr);
//    item = NULL;
}

static void MpdSegmentEntryFree(void* item)
{
    free(item);
}

static void SegmentTimeLineFree(void* item)
{
    if(!item) return;
    awSegmentTimeline* ptr = (awSegmentTimeline*)item;
    MpdListFree(ptr->entries, MpdSegmentEntryFree);
    free(ptr);
}

static void SegmentUrlFree(void* item)
{
    if(!item) return;

    awSegmentURL* ptr = (awSegmentURL*)item;
    if(ptr->index)
    {
        free(ptr->index);
        ptr->index = NULL;
    }
    if(ptr->media)
    {
        free(ptr->media);
        ptr->index = NULL;
    }
    if(ptr->index_range)
    {
        free(ptr->index_range);
        ptr->index_range = NULL;
    }
    if(ptr->media_range)
    {
        free(ptr->media_range);
        ptr->media_range = NULL;
    }

    free(ptr);
//    item = NULL;
}

static void SegmentBaseFree(void* item)
{
    if(!item) return;

    awSegmentBase* ptr = (awSegmentBase*)item;
    if(ptr->initializationSegment)
    {
        MpdUrlFree(ptr->initializationSegment);
        ptr->initializationSegment = NULL;
    }
    if(ptr->representationIndex)
    {
        MpdUrlFree(ptr->representationIndex);
        ptr->representationIndex = NULL;
    }
    if(ptr->indexRange)
    {
        free(ptr->indexRange);
        ptr->indexRange = NULL;
    }

    free(ptr);
//    item = NULL;
}

static void SegmentListFree(void* item)
{
    if(!item) return;

    awSegmentList* ptr = (awSegmentList*)item;
    if(ptr->xlinkHref)
    {
        free(ptr->xlinkHref);
        ptr->xlinkHref = NULL;
    }
    if(ptr->initializationSegment)
    {
        MpdUrlFree(ptr->initializationSegment);
        ptr->initializationSegment = NULL;
    }
    if(ptr->bitstreamSwitching_URL)
    {
        MpdUrlFree(ptr->bitstreamSwitching_URL);
        ptr->bitstreamSwitching_URL = NULL;
    }
    if(ptr->representationIndex)
    {
        MpdUrlFree(ptr->representationIndex);
        ptr->representationIndex = NULL;
    }
    // free awSegmentTimeline
    if(ptr->segmentTimeline)
    {
        SegmentTimeLineFree(ptr->segmentTimeline);
        ptr->segmentTimeline = NULL;
    }
    MpdListFree(ptr->segmentURL, SegmentUrlFree);

    free(ptr);
//    item = NULL;
}

static void SegmentTemplateFree(void* item)
{
    if(!item) return;
    awSegmentTemplate* ptr = (awSegmentTemplate*)item;
    if(ptr->initializationSegment)
    {
        MpdUrlFree(ptr->initializationSegment);
        ptr->initializationSegment = NULL;
    }
    if(ptr->bitstreamSwitching_URL)
    {
        MpdUrlFree(ptr->bitstreamSwitching_URL);
        ptr->bitstreamSwitching_URL = NULL;
    }
    if(ptr->representationIndex)
    {
        MpdUrlFree(ptr->representationIndex);
        ptr->representationIndex = NULL;
    }
    if(ptr->segmentTimeline)
    {
        SegmentTimeLineFree(ptr->segmentTimeline);
        ptr->segmentTimeline = NULL;
    }

    if(ptr->bitstreamSwitch)
    {
        free(ptr->bitstreamSwitch);
        ptr->bitstreamSwitch = NULL;
    }
    if(ptr->index)
    {
        free(ptr->index);
        ptr->index = NULL;
    }
    if(ptr->initialization)
    {
        free(ptr->index);
        ptr->index = NULL;
    }
    if(ptr->media)
    {
        free(ptr->media);
        ptr->media = NULL;
    }

    free(ptr);
//    item = NULL;
}

static void BaseUrlFree(void* item)
{
    awBaseURL* base_url = (awBaseURL*)item;
    if(base_url->URL)
    {
        free(base_url->URL);
        base_url->URL = NULL;
    }
    if(base_url->serviceLocation)
    {
        free(base_url->serviceLocation);
        base_url->serviceLocation = NULL;
    }
    if(base_url->byteRange)
    {
        free(base_url->byteRange);
        base_url->byteRange = NULL;
    }
    free(base_url);
//    item = NULL;
}

static void CommonAttributeFree(MPD_CommonAttributes* ptr)
{
    if(!ptr) return;

    if (ptr->profiles)
    {
        free(ptr->profiles);
        ptr->profiles = NULL;
    }
    if (ptr->sar)
    {
        free(ptr->sar);
        ptr->sar = NULL;
    }
    if (ptr->framerate)
    {
        free(ptr->framerate);
        ptr->framerate = NULL;
    }
    if (ptr->mime_type)
    {
        free(ptr->mime_type);
        ptr->mime_type = NULL;
    }
    if (ptr->segmentProfiles)
    {
        free(ptr->segmentProfiles);
        ptr->segmentProfiles = NULL;
    }
    if (ptr->codecs)
    {
        free(ptr->codecs);
        ptr->codecs = NULL;
    }

    MpdListFree(ptr->frame_packing, MpdDescriptorFree);
    MpdListFree(ptr->audio_channels, MpdDescriptorFree);
    MpdListFree(ptr->content_protection, MpdDescriptorFree);
}

static void RepresentationFree(void* item)
{
    if(!item) return;
    awRepresentation* ptr = (awRepresentation*)item;
    if(ptr->id)
    {
        free(ptr->id);
        ptr->id = NULL;
    }
    if(ptr->dependencyId)
    {
        free(ptr->dependencyId);
        ptr->dependencyId = NULL;
    }
    if(ptr->mediaStreamStructureId)
    {
        free(ptr->mediaStreamStructureId);
        ptr->mediaStreamStructureId = NULL;
    }

    MpdListFree(ptr->baseURLs, BaseUrlFree);
    MpdListFree(ptr->subRepresentation, SubRepresentationFree);

    if(ptr->segmentBase)
    {
        SegmentBaseFree(ptr->segmentBase);
        ptr->segmentBase = NULL;
    }
    if(ptr->segmentList)
    {
        SegmentListFree(ptr->segmentList);
        ptr->segmentList= NULL;
    }
    if(ptr->segmentTemplate)
    {
        SegmentTemplateFree(ptr->segmentTemplate);
        ptr->segmentTemplate= NULL;
    }

    free(ptr);
}

static void AdaptationSetFree(void* item)
{
    if(!item)  return;

    awAdaptionSet* ptr = (awAdaptionSet*)item;
    CommonAttributeFree((MPD_CommonAttributes*)item);
    if(ptr->lang)
    {
        free(ptr->lang);
        ptr->lang = NULL;
    }
    if(ptr->contentType)
    {
        free(ptr->contentType);
        ptr->contentType = NULL;
    }
    if(ptr->par)
    {
        free(ptr->par);
        ptr->par = NULL;
    }
    if(ptr->xlinkHref)
    {
        free(ptr->xlinkHref);
        ptr->xlinkHref = NULL;
    }
    MpdListFree(ptr->accessiblity, MpdDescriptorFree);
    MpdListFree(ptr->role, MpdDescriptorFree);
    MpdListFree(ptr->rating, MpdDescriptorFree);
    MpdListFree(ptr->viewpoint, MpdDescriptorFree);
    MpdListFree(ptr->contentComponent, MpdContentComponentFree);

    if(ptr->segmentBase)
    {
        SegmentBaseFree(ptr->segmentBase);
        ptr->segmentBase = NULL;
    }
    if(ptr->segmentList)
    {
        SegmentListFree(ptr->segmentList);
        ptr->segmentList= NULL;
    }
    if(ptr->segmentTemplate)
    {
        SegmentTemplateFree(ptr->segmentTemplate);
        ptr->segmentTemplate= NULL;
    }

    MpdListFree(ptr->baseURLs, BaseUrlFree);
    MpdListFree(ptr->representation,RepresentationFree);
    free(ptr);
//    item = NULL;
}

static void SubSetFree(void* item)
{
    CDX_LOGD("W: subset is not complete.\n");
    CDX_UNUSE(item);
}

static void PeriodFree(void* item)
{
    if(!item) return;

    awPeriod* ptr = (awPeriod*)item;
    //char* type in the awPeriod structure
    if(ptr->id)
    {
        free(ptr->id);
        ptr->id = NULL;
    }
    if(ptr->xlinkHref)
    {
        free(ptr->xlinkHref);
        ptr->xlinkHref = NULL;
    }
    // free some structure
    if(ptr->segmentBase)
    {
        SegmentBaseFree(ptr->segmentBase);
        ptr->segmentBase = NULL;
    }
    if(ptr->segmentList)
    {
        SegmentListFree(ptr->segmentList);
        ptr->segmentList = NULL;
    }
    if(ptr->segmentTemplate)
    {
        SegmentTemplateFree(ptr->segmentTemplate);
        ptr->segmentTemplate = NULL;
    }

    //free list
    if(ptr->baseURL)
    {
        MpdListFree(ptr->baseURL, BaseUrlFree);
        ptr->baseURL = NULL;
    }
    if(ptr->adaptationSets)
    {
        MpdListFree(ptr->adaptationSets, AdaptationSetFree);
        ptr->adaptationSets= NULL;
    }
    if(ptr->subsets)
    {
        MpdListFree(ptr->subsets, SubSetFree);
        ptr->subsets = NULL;
    }

    free(item);
//    item = NULL;
}

void MpdFree(awMpd* mpd)
{
    if(!mpd) return;

    if(mpd->profiles)
    {
        free(mpd->profiles);
        mpd->profiles = NULL;
    }
    if(mpd->id)
    {
        free(mpd->id);
        mpd->id = NULL;
    }

    if(mpd->programInfos)
    {
        MpdListFree(mpd->programInfos, ProgramInfoFree);
        mpd->programInfos = NULL;
    }

    if(mpd->baseURLs)
    {
        MpdListFree(mpd->baseURLs, BaseUrlFree);
        mpd->baseURLs = NULL;
    }
    if(mpd->locations)
    {
        //to do
    }
    if(mpd->metrics)
    {
        //to do
    }
    if(mpd->periods)
    {
        MpdListFree(mpd->periods, PeriodFree);
        mpd->periods= NULL;
    }

    free(mpd);
//    mpd = NULL;

}

/*
*   parse mpd file
*   @filename: if mpd file is a local file, filename is not NULL;
*   @fd : if the mpd file is opened, file descriptor is not 0
*   @buffer, size :  if the mpd content is a char array,
*   @encoding: the document encoding or NULL;
*   @option: a combination of xmlParserOption
*/
awMpd* ParseMpdFile(char* filename, int fd, char* buffer,
                    int size, const char *encoding, int options)
{
    xmlNodePtr rootNode;      // node
    xmlDocPtr doc = NULL;           //xml file handle
    awMpd* mpd = NULL;

    if((filename == NULL) && (fd<0) && ((buffer == NULL) || (size == 0)) )
    {
        CDX_LOGD("mpd file is not exist!.\n");
        return NULL;
    }

    /* parse "data" into a document (which is a libxml2 tree structure xmlDoc) */
    // local mpd file
    if(filename)
    {
        doc = xmlReadFile(filename, encoding, options); //read file
    }
    else if(fd > -1)
    {
        doc = xmlReadFd(fd, NULL, encoding, options);
    }
    else if(buffer)
    {
        doc = xmlReadMemory(buffer, size, NULL, encoding, options);//read from a char array
    }

    if (NULL == doc)
    {
        CDX_LOGE("Document not parsed successfully\n");
        return NULL;
    }

    rootNode = xmlDocGetRootElement(doc); // read root node
    if (NULL == rootNode)
    {
        CDX_LOGE("empty document\n");
        goto parse_err;
    }
    if(strcmp((const char*)rootNode->name, "MPD"))
    {
        CDX_LOGI("Warning: the root node is not 'MPD'. \n");
    }

    mpd = InitMpd();
    if(!mpd)
    {
        goto parse_err;
    }

    if(MpdTraverse(rootNode, mpd) < 0)
        goto parse_err;

    xmlFreeDoc(doc);
    return mpd;

parse_err:
    if(mpd)
    {
        MpdFree(mpd);
        mpd = NULL;
    }
    if(doc)
    {
        xmlFreeDoc(doc);
        doc = NULL;
    }
    return NULL;
}
