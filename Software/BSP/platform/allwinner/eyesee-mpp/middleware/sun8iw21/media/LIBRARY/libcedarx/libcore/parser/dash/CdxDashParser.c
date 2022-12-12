/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxDashParser.c
 * Description : DashParser
 * History :
 *
 */

#define LOG_TAG "CdxDashParser"

#include <assert.h>
#include <cdx_log.h>
#include "CdxDashParser.h"

#define CDX_DASH_MAX_PATH 200

enum {
    // absolute url
    CDX_URL_TYPE_ABSOLUTE = 0,
    CDX_URL_TYPE_RELATIVE ,
    CDX_URL_TYPE_OTHER,
    CDX_URL_TYPE_BLOB  //blob url, I do not kown what it is , see GPAC?
};

enum CdxDashStatusE {
    CDX_DASH_INITIALIZED, /*control , getMediaInfo, not prefetch or read, seekTo,.*/
    CDX_DASH_IDLE,
    CDX_DASH_PREFETCHING,
    CDX_DASH_PREFETCHED,
    CDX_DASH_READING,
    CDX_DASH_SEEKING,
};

static void W8(char **s, int b)
{
    *(*s)++ = b;
}
static void Wb32(char** s, int val)
{
    W8(s,                 val>>24   );
    W8(s, (unsigned char)(val >> 16));
    W8(s, (unsigned char)(val >> 8 ));
    W8(s, (unsigned char) val       );
}
/***********************************************************************************/
/*  extract sps and pps in extradata and pack to a NALU               */
/***********************************************************************************/
static int H264Extract(char* extraData, int extarDataLen, char** buf, int* size)
{
    int sps_num;
    int sps_size;
    int pps;
    char *sps_data = extraData + 5;
    CDX_UNUSE(extarDataLen);

    for(pps=0; pps<2; pps++)
    {
        if(pps)
        {
            sps_num = *(sps_data++) & 0xff;
        }
        else
        {
            sps_num = *(sps_data++) & 0x1f;
        }
        CDX_LOGD("sps_num = %d", sps_num);

        for(; sps_num>0; sps_num--)
        {
            sps_size = (*(sps_data++) << 8);
            sps_size    += *(sps_data++);
            CDX_LOGD("sps_size = %d", sps_size);
            memcpy(*buf, sps_data, sps_size);
            *buf += sps_size;
            *size += sps_size;
            sps_data += sps_size;
        }
    }

    return 0;
}

/*************************************************************************/
// get the protocol type of a url : http:// or ftp://
//as usual, start with '/' is a absolute url,
/*************************************************************************/
static int CdxUrlGetType(const char* pPathName)
{
    char* begin;

    if(!pPathName) return CDX_URL_TYPE_OTHER;

    // url is begin with '/' or '\' or ':' or '::', is a absolute url
    if((pPathName[0] == '/') || (pPathName[0] == '\\') ||(pPathName[0] == ':') ||
            ((pPathName[0] == ':') && (pPathName[1] == ':') ) )
    {
        return CDX_URL_TYPE_ABSOLUTE;
    }

    if(!strncmp(pPathName, "blob:", 5))
    {
        return CDX_URL_TYPE_BLOB;
    }

    begin = strstr(pPathName, "://");
    if(!begin) begin = strstr(pPathName, "|//");
    if(!begin) return CDX_URL_TYPE_RELATIVE;
    if (!strncasecmp(pPathName, "file", 4)) return CDX_URL_TYPE_ABSOLUTE;
    return CDX_URL_TYPE_OTHER;
}

/*************************************************************************/
// merge the pParentName and pPathName to get the absolute url
/*************************************************************************/
char* CdxUrlConcatenate(const char* pParentName, const char* pPathName)
{
    int proto_type;
    int i, nPathSepCount;
    char* pOutPath;
    char *name;
    char tmp[CDX_DASH_MAX_PATH];

    if(!pParentName && !pPathName) return NULL;
    if(!pPathName) return strdup(pParentName);
    if(!pParentName) return strdup(pPathName);

    if((strlen(pParentName)>CDX_DASH_MAX_PATH) || (strlen(pPathName)>CDX_DASH_MAX_PATH))
    {
        CDX_LOGW(" the length of url longer than 200, not support");
        return NULL;
    }

    proto_type = CdxUrlGetType(pPathName);
    if(proto_type != CDX_URL_TYPE_RELATIVE)
    {
        char* sep = NULL;
        //if the pPathName is not relative url and pParentName is absolute url,
        //add the pPathName to the end of pParentName
        if(pPathName[0]=='/') sep = strstr(pParentName, "://");
        if(sep) sep = strchr(sep+3, '/');

        if(sep)
        {
            int len;
            sep[0] = 0;
            len = strlen(pParentName);
            pOutPath = (char*)malloc(len+1+strlen(pPathName));
            strcpy(pOutPath, pParentName);
            strcat(pOutPath, pPathName);
            sep[0] = '/';
        }
        else
        {
            pOutPath = strdup(pPathName);
        }
        goto check_spaces;
    }

    nPathSepCount = 0;
    name = NULL;
    if (pPathName[0] == '.')
    {
        if (!strcmp(pPathName, ".."))
        {
            nPathSepCount = 1;
            name = "";
        }
        if (!strcmp(pPathName, "./"))
        {
            nPathSepCount = 0;
            name = "";
        }
        for (i = 0; i< (int)strlen(pPathName) - 2; i++)
        {
            /*current dir*/
            if ( (pPathName[i] == '.') && ( (pPathName[i+1] == '\\') || (pPathName[i+1] == '/') ) )
            {
                i++;
                continue;
            }
            /*parent dir*/
            if ( (pPathName[i] == '.') && (pPathName[i+1] == '.') &&
                    ( (pPathName[i+2] == '\\') || (pPathName[i+2] == '/') ))
            {
                nPathSepCount ++;
                i+=2;
                name = (char *) &pPathName[i+1];
            }
            else
            {
                name = (char *) &pPathName[i];
                break;
            }
        }
    }
    if (!name) name = (char *) pPathName;

    strcpy(tmp, pParentName);
    while (strchr(" \r\n\t", tmp[strlen(tmp)-1]))
    {
        tmp[strlen(tmp)-1] = 0;
    }

    /*remove the last /*/
    for (i = strlen(pParentName); i > 0; i--)
    {
        //break our path at each separator
        if ((pParentName[i-1] == '\\') || (pParentName[i-1] == '/'))
        {
            tmp[i-1] = 0;
            if (!nPathSepCount) break;
            nPathSepCount--;
        }
    }
    //if i==0, the parent path was relative, just return the pPathName
    if (!i)
    {
        tmp[i] = 0;
        while (nPathSepCount)
        {
            strcat(tmp, "../");
            nPathSepCount--;
        }
    }
    else
    {
        strcat(tmp, "/");
    }
    //add the namePath  to parentPath
    i = strlen(tmp);
    pOutPath = (char *) malloc(i + strlen(name) + 1);
    sprintf(pOutPath, "%s%s", tmp, name);

    /*cleanup paths sep for win32*/
    for (i = 0; i<(int)strlen(pOutPath); i++)
        if (pOutPath[i]=='\\') pOutPath[i] = '/';

check_spaces:
    i=0;
    while (pOutPath[i])
    {
        if (pOutPath[i] == '?') break;

        if (pOutPath[i] != '%')
        {
            i++;
            continue;
        }
        if (!strncasecmp(pOutPath+i, "%3f", 3)) break;
        if (!strncasecmp(pOutPath+i, "%20", 3))
        {
            pOutPath[i]=' ';
            memmove(pOutPath + i+1, pOutPath+i+3, strlen(pOutPath+i)-2);
        }
        i++;
    }
    return pOutPath;
}

/*************************************************************************/
//*    find the init right adaptation set and representation by the bandwidth (the lowest bandwith)
/*************************************************************************/
int CdxDashRepIndexInit(CdxDashParser* dashStream, awPeriod* period)
{

    int i, j;
    awAdaptionSet* set;
    awRepresentation* rep;
    char* mime_type;

    int nb_set = aw_list_count(period->adaptationSets);
    int nb_rep;

    unsigned int bandwidth = 0xFFFFFFFF;

    // find the video and audio adaptation set
    for(i=0; i<nb_set; i++)
    {
        set = aw_list_get(period->adaptationSets, i);

        nb_rep = aw_list_count(set->representation);
        if(nb_rep < 1)
        {
            continue;
        }

        //find the mime type(  like  "video/mp4" or "audio/mp4"  )
        //to get the video and audio activeAdapSetIndex
        rep = aw_list_get(set->representation, 0);

        if(set->mime_type)
        {
            mime_type = set->mime_type;
        }
        else if(rep->mime_type)
        {
            mime_type = rep->mime_type;
        }
        else
        {
            CDX_LOGI("Be Careful: adaptation set and representation do not have mimetype!");
            break;
        }

        if(!strncmp(mime_type, "video", 5))
        {
            dashStream->mediaStream[0].activeAdapSetIndex = i;  //video
            dashStream->hasVideo = 1;
            if(rep->bandwidth)
            {
                bandwidth = rep->bandwidth;
                dashStream->mediaStream[0].activeRepSetIndex = 0;
            }

            //find the active representation of video( the bandwidth lowest is the active one)
            for(j=1; j<nb_rep; j++)
            {
                rep = aw_list_get(set->representation, j);
                if(rep->bandwidth)
                {
                    if(rep->bandwidth < bandwidth)
                    {
                        dashStream->mediaStream[0].activeRepSetIndex = j;
                    }
                }
            }
        }
        else if(!strncmp(mime_type, "audio", 5))
        {
            dashStream->mediaStream[1].activeAdapSetIndex = i;  //audio
            dashStream->mediaStream[1].activeRepSetIndex = 0;  //bandwidth
            dashStream->hasAudio = 1;
        }
        else if(!strncmp(mime_type, "subtitle", 8)) //maybe is not 'subtitle'
        {
            CDX_LOGI(" subtitle is not supported now!");
            dashStream->mediaStream[2].activeAdapSetIndex = i;  //audio
            dashStream->mediaStream[2].activeRepSetIndex = 0;  //bandwidth
            dashStream->hasSubtitle = 1;
        }
        else
        {
            CDX_LOGW(" unkown group type <%s>\n", rep->mime_type);
        }

    }
    return 0;
}

/*************************************************************************************************/
/*
*   get the right period index from playing time; ( Period has a 'start' and 'duration' property )
*           $'start'           start playing time of this period
*           $'duration'          duration of this period
*/
/*************************************************************************************************/
int CdxDashPeriodIndexFromTime(CdxDashParser* dashStream, int64_t time)
{
    int i;
    int count;
    unsigned int accu_start = 0;

    awPeriod* period;
    count = aw_list_count(dashStream->mpd->periods);
    for(i=0; i<count; i++)
    {
        period = aw_list_get(dashStream->mpd->periods, i);
        if(((int64_t)period->start > time) || ((int64_t)accu_start > time))
        {
            break;
        }
        accu_start += period->duration;
    }
    return (i-1 >= 0 ? (i-1) : 0);
}

int CdxDashGetSegmentDuration(awRepresentation *rep, awAdaptionSet *set, awPeriod *period,
                              awMpd* mpd,int *nbSegments, long long int *maxSegDuration)
{
    long long int mediaDuration;
    int single_segment = 0;
    int timescale;
    long long int duration;
    awSegmentTimeline *timeline = NULL;
    *nbSegments = timescale = 0;
    duration = 0;

    if(rep->segmentList || set->segmentList || period->segmentList)
    {
        AW_List* segments = NULL;
        if(period->segmentList)
        {
            if(period->segmentList->duration)
                duration = period->segmentList->duration;
            if(period->segmentList->timescale)
                timescale = period->segmentList->timescale;
            if(period->segmentList->segmentURL)
                segments = period->segmentList->segmentURL;
            if(period->segmentList->segmentTimeline)
                timeline = period->segmentList->segmentTimeline;
        }
        if(set->segmentList)
        {
            if(set->segmentList->duration)
                duration = set->segmentList->duration;
            if(set->segmentList->timescale)
                timescale = set->segmentList->timescale;
            if(set->segmentList->segmentURL)
                segments = set->segmentList->segmentURL;
            if(set->segmentList->segmentTimeline)
                timeline = set->segmentList->segmentTimeline;
        }
        if(rep->segmentList)
        {
            if(rep->segmentList->duration)
                duration = rep->segmentList->duration;
            if(rep->segmentList->timescale)
                timescale = rep->segmentList->timescale;
            if(rep->segmentList->segmentURL)
                segments = rep->segmentList->segmentURL;
            if(rep->segmentList->segmentTimeline)
                timeline = rep->segmentList->segmentTimeline;
        }
        if(!timescale) timescale = 1;

        if(timeline)
        {
            CDX_LOGW("get duration from timeline is not supported yet!");
            return -1;
        }
        else
        {
            if(segments)
            {
                *nbSegments = aw_list_count(segments);
            }
            if(maxSegDuration)
            {
                *maxSegDuration = (long long int)duration;
                *maxSegDuration = *maxSegDuration*1000 / timescale;
            }
        }
        return 0;
    }

    // single segment
    if(rep->segmentBase || set->segmentBase || period->segmentBase)
    {
        *maxSegDuration = mpd->mediaPresentationDuration;
        *nbSegments = 1;
        return 0;
    }

    single_segment = 1;
    if(period->segmentTemplate)
    {
        single_segment = 0;
        if(period->segmentTemplate->duration)
            duration = period->segmentTemplate->duration;
        if(period->segmentTemplate->timescale)
            timescale = period->segmentTemplate->timescale;
        if(period->segmentTemplate->segmentTimeline)
            timeline = period->segmentTemplate->segmentTimeline;
    }
    if(set->segmentTemplate)
    {
        single_segment = 0;
        if(set->segmentTemplate->duration)
            duration = set->segmentTemplate->duration;
        if(set->segmentTemplate->timescale)
            timescale = set->segmentTemplate->timescale;
        if(set->segmentTemplate->segmentTimeline)
            timeline = set->segmentTemplate->segmentTimeline;
    }
    if(rep->segmentTemplate)
    {
        single_segment = 0;
        if(rep->segmentTemplate->duration)
            duration = rep->segmentTemplate->duration;
        if(rep->segmentTemplate->timescale)
            timescale = rep->segmentTemplate->timescale;
        if(rep->segmentTemplate->segmentTimeline)
            timeline = rep->segmentTemplate->segmentTimeline;
    }
    if(!timescale) timescale = 1;

    /*if no SegmentXXX is found, this is a single segment representation (onDemand profile)*/
    if(single_segment)
    {
        *maxSegDuration = mpd->mediaPresentationDuration;
        *nbSegments = 1;
        return 0;
    }

    if(timeline)
    {
        CDX_LOGW("get duration from timeline is not supported yet!");
        return -1;
    }
    else
    {
        if(maxSegDuration)
        {

            *maxSegDuration = (long long int)duration;
            *maxSegDuration = *maxSegDuration*1000/timescale;
        }
        mediaDuration = period->duration;
        if(!mediaDuration) mediaDuration = mpd->mediaPresentationDuration;
        if(mediaDuration && duration)
        {
            long long int nbSeg = (long long int)mediaDuration;
            nbSeg *= timescale;
            nbSeg /= (duration*1000);
            *nbSegments = (int)nbSeg;
            if (*nbSegments < nbSeg) (*nbSegments)++;
        }
    }
    return 0;
}

/*************************************************************************************************/
/****<  get the media segment index according to the time, used for seekto  */
/*************************************************************************************************/
int CdxDashGetSegmentIndex(int active_rep, awAdaptionSet *set, awPeriod *period,
                           awMpd    * mpd, cdx_int64 timeUs, long long int* segmentDuration)
{
    int ret;
    int nbSegments;
    long long int seg_duration;

    int count = aw_list_count(set->representation);
    if(!count)
    {
        CDX_LOGW("the adaptation set do not have any reprensentation, we cannot get the index");
        return -1;
    }
    awRepresentation* rep = aw_list_get(set->representation, active_rep);

    ret = CdxDashGetSegmentDuration(rep, set, period, mpd, &nbSegments, &seg_duration);
    if(ret < 0)
    {
        return -1;
    }

    ret = timeUs/seg_duration;
    if(segmentDuration)
    {
        *segmentDuration = seg_duration;
    }
    //CDX_LOGD("timeUs = %lld, segduration = %lld", timeUs, seg_duration);
    return ret;
}

/*************************************************************************/
/*
*        get the segment duration
*/
/*************************************************************************/
static int CdxDashResolveDuration(awRepresentation *rep,
                                  awAdaptionSet *set,
                                  awPeriod *period,
                                  long long int *outDuration,
                                  int *outTimescale,
                                  long long int *outPtsOffset,
                                  awSegmentTimeline **outSegmentTimeline)
{
    int timescale = 0;
    long long int ptsOffset = 0;
    awSegmentTimeline *segmentTimeline;
    awMultipleSegmentBase *mbaseRep, *mbaseSet, *mbasePeriod;

    if (outSegmentTimeline) *outSegmentTimeline = NULL;
    if (outPtsOffset) *outPtsOffset = 0;

    /*single media segment - duration is not known unless indicated in period*/
    if (rep->segmentBase || set->segmentBase || period->segmentBase)
    {
        *outDuration = period ? period->duration : 0;
        timescale = 0;
        if (rep->segmentBase && rep->segmentBase->presentationTimeOffset)
            ptsOffset = rep->segmentBase->presentationTimeOffset;
        if (rep->segmentBase && rep->segmentBase->timescale)
            timescale = rep->segmentBase->timescale;
        if (!ptsOffset && set->segmentBase && set->segmentBase->presentationTimeOffset)
            ptsOffset = set->segmentBase->presentationTimeOffset;
        if (!timescale && set->segmentBase && set->segmentBase->timescale)
            timescale = set->segmentBase->timescale;
        if (!ptsOffset && period->segmentBase && period->segmentBase->presentationTimeOffset)
            ptsOffset = period->segmentBase->presentationTimeOffset;
        if (!timescale && period->segmentBase && period->segmentBase->timescale)
            timescale = period->segmentBase->timescale;

        if (outPtsOffset) *outPtsOffset = ptsOffset;
        *outTimescale = timescale ? timescale : 1;
        return 0;
    }
    /*we have a segment template list or template*/
    mbaseRep = rep->segmentList ? (awMultipleSegmentBase *) rep->segmentList :
                                  (awMultipleSegmentBase *) rep->segmentTemplate;
    mbaseSet = set->segmentList ? (awMultipleSegmentBase *)set->segmentList :
                                  (awMultipleSegmentBase *)set->segmentTemplate;
    mbasePeriod = period->segmentList ? (awMultipleSegmentBase *)period->segmentList :
                                  (awMultipleSegmentBase *)period->segmentTemplate;

    segmentTimeline = NULL;
    if (mbasePeriod) segmentTimeline =  mbasePeriod->segmentTimeline;
    if (mbaseSet && mbaseSet->segmentTimeline) segmentTimeline =  mbaseSet->segmentTimeline;
    if (mbaseRep && mbaseRep->segmentTimeline) segmentTimeline =  mbaseRep->segmentTimeline;

    timescale = mbaseRep ? mbaseRep->timescale : 0;
    if (!timescale && mbaseSet && mbaseSet->timescale) timescale = mbaseSet->timescale;
    if (!timescale && mbasePeriod && mbasePeriod->timescale) timescale  = mbasePeriod->timescale;
    if (!timescale) timescale = 1;
    *outTimescale = timescale;

    if (outPtsOffset)
    {
        ptsOffset = mbaseRep ? mbaseRep->presentationTimeOffset : 0;
        if (!ptsOffset && mbaseSet && mbaseSet->presentationTimeOffset)
            ptsOffset = mbaseSet->presentationTimeOffset;
        if (!ptsOffset && mbasePeriod && mbasePeriod->presentationTimeOffset)
            ptsOffset = mbasePeriod->presentationTimeOffset;
        *outPtsOffset = ptsOffset;
    }

    if (mbaseRep && mbaseRep->duration) *outDuration = mbaseRep->duration;
    else if (mbaseSet && mbaseSet->duration) *outDuration = mbaseSet->duration;
    else if (mbasePeriod && mbasePeriod->duration) *outDuration = mbasePeriod->duration;

    if (outSegmentTimeline) *outSegmentTimeline = segmentTimeline;

    /*for SegmentTimeline, just pick the first one as an indication*/
    /*(exact timeline solving is not done here)*/
    if (segmentTimeline)
    {
        awSegmentTimelineEntry *ent = aw_list_get(segmentTimeline->entries, 0);
        if (ent) *outDuration = ent->d;
    }
    else if (rep->segmentList)
    {
        awSegmentURL *url = aw_list_get(rep->segmentList->segmentURL, 0);
        if (url && url->duration) *outDuration = url->duration;
    }
    return 0;
}

/*************************************************************************************************/
//get the absolute url of media segment
/*
*    @ resolveType :
*        $CDX_DASH_RESOLVE_URL_INIT :          init segment
*        $CDX_DASH_RESOLVE_URL_MEDIA :     media segment
*    @segmentDuration: the duration of the media segment, the property of SegmentTimeline
*/
/*************************************************************************************************/
static int CdxDashResolveUrl(CdxDashParser* dashStream, awPeriod* period,awAdaptionSet* set,
                             awRepresentation* rep,CDX_DASHURLResolveType resolveType,
                             int itemIndex, char** outUrl, long long int *segmentDuration)
{
    char* url;
    awBaseURL* urlChild;
    unsigned int startNumber = 1;
    awSegmentTimeline* timeLine = NULL;
    awMpd* mpd = dashStream->mpd;

    //int nbSegments = 0;
    //long long int maxSegDuration = 0;

    char* tmpUrl;
    int timescale;

    // if mpd, period, adptationset, rep have baseUrl, merge it.
    url = strdup(dashStream->mpdUrl);
    urlChild = aw_list_get(mpd->baseURLs, 0);
    if(urlChild)
    {
        tmpUrl = CdxUrlConcatenate(url, (char*)urlChild->URL);
        free(url);
        url = tmpUrl;
    }

    urlChild = aw_list_get(period->baseURL, 0);
    if(urlChild)
    {
        tmpUrl = CdxUrlConcatenate(url, (char*)urlChild->URL);
        free(url);
        url = tmpUrl;
    }

    urlChild = aw_list_get(set->baseURLs, 0);
    if(urlChild)
    {
        tmpUrl = CdxUrlConcatenate(url, (char*)urlChild->URL);
        free(url);
        url = tmpUrl;
    }

    urlChild = aw_list_get(rep->baseURLs, 0);
    if(urlChild)
    {
        // if the base url is started with http, is a whole url, do not need merge
        if(!strncasecmp("http://", (char*)urlChild->URL, 7))
        {
            free(url);
            url = (char*)urlChild->URL;
        }
        else
        {
            tmpUrl = CdxUrlConcatenate(url, (char*)urlChild->URL);
            free(url);
            url = tmpUrl;
        }
    }

    CdxDashResolveDuration(rep, set, period, segmentDuration, &timescale, NULL, NULL);
    *segmentDuration = (resolveType==CDX_DASH_RESOLVE_URL_MEDIA) ?
                                      (int) ((double) (*segmentDuration) * 1000.0 / timescale) : 0;

    //single media segment URL
    // there is a sidx in mp4 file, but it did not appear in the mpd,
    // daynamic adaptive is not supported now
    if (!rep->segmentList && !set->segmentList && !period->segmentList &&
            !rep->segmentTemplate && !set->segmentTemplate && !period->segmentTemplate)
    {
        //LOGD("single url is test. url = %s\n", url);
        //awMpdURL* res_url;
        //awSegmentBase* base_seg = NULL;
        if(itemIndex > 0)
        {
            // end of stream
            free(url);
            return 1;
        }

        switch(resolveType)
        {
        case CDX_DASH_RESOLVE_URL_INIT:
        case CDX_DASH_RESOLVE_URL_MEDIA:
        case CDX_DASH_RESOLVE_URL_MEDIA_TEMPLATE:
            dashStream->mediaStream[0].eou = 1;
            dashStream->mediaStream[1].eou = 1;
            if(!url)
            {
                return -1;

            }
            *outUrl = url;
            return 0;
#if 0
        case CDX_DASH_RESOLVE_URL_INIT:
        case CDX_DASH_RESOLVE_URL_INDEX:
            LOGD("CDX_DASH_RESOLVE_URL_INDEX not support now");
            break;
#endif
        default:
            break;
        }
        free(url);
        return -1;
    }

    //segment list
    if(rep->segmentList || set->segmentList || period->segmentList)
    {
        awMpdURL* initUrl = NULL;   //init segment url
        awMpdURL* indexUrl = NULL;
        awSegmentURL* segmentUrl;
        AW_List*     segments = NULL;
        int segmentCnt;

        /*apply inheritance of attributes, lowest level having preceedence*/
        if(period->segmentList)
        {
            if(period->segmentList->initializationSegment)
                initUrl = period->segmentList->initializationSegment;
            if(period->segmentList->representationIndex)
                indexUrl = period->segmentList->representationIndex;
            if(period->segmentList->segmentURL)
                segments = period->segmentList->segmentURL;
            if(period->segmentList->startNumber != (unsigned int) -1)
                startNumber = period->segmentList->startNumber;
            if(period->segmentList->segmentTimeline)
                timeLine = period->segmentList->segmentTimeline;
        }

        if(set->segmentList)
        {
            if(set->segmentList->initializationSegment)
                initUrl = period->segmentList->initializationSegment;
            if(set->segmentList->representationIndex)
                indexUrl = period->segmentList->representationIndex;
            if(set->segmentList->segmentURL)
                segments = period->segmentList->segmentURL;
            if(set->segmentList->startNumber != (unsigned int) -1)
                startNumber = period->segmentList->startNumber;
            if(set->segmentList->segmentTimeline)
                timeLine = period->segmentList->segmentTimeline;
        }

        if(rep->segmentList)
        {
            if(rep->segmentList->initializationSegment)
                initUrl = period->segmentList->initializationSegment;
            if(rep->segmentList->representationIndex)
                indexUrl = period->segmentList->representationIndex;
            if(rep->segmentList->segmentURL)
                segments = period->segmentList->segmentURL;
            if(rep->segmentList->startNumber != (unsigned int) -1)
                startNumber = period->segmentList->startNumber;
            if(rep->segmentList->segmentTimeline)
                timeLine = period->segmentList->segmentTimeline;
        }

        segmentCnt = aw_list_count(segments);

        switch(resolveType)
        {
        case CDX_DASH_RESOLVE_URL_INIT:
            if(initUrl)
            {
                if(initUrl->sourceURL)
                {
                    *outUrl = CdxUrlConcatenate(url, initUrl->sourceURL);
                    free(url);
                }
                else
                {
                    *outUrl = url;
                }
            }
            return 0;

        case CDX_DASH_RESOLVE_URL_MEDIA:
        case CDX_DASH_RESOLVE_URL_MEDIA_TEMPLATE:
            if(itemIndex >= segmentCnt)
            {
                free(url);
                return -1;
            }
            *outUrl = url;
            segmentUrl = aw_list_get(segments, itemIndex);
            if(segmentUrl->media)
            {
                *outUrl = CdxUrlConcatenate(url, segmentUrl->media);
                free(url);
            }
            return 0;

        case CDX_DASH_RESOLVE_URL_INDEX:
            if(itemIndex >= segmentCnt)
            {
                free(url);
                return -1;
            }
            *outUrl = url;
            segmentUrl = aw_list_get(segments, itemIndex);
            if(segmentUrl->index)
            {
                *outUrl = CdxUrlConcatenate(url, segmentUrl->index);
                free(url);
            }
            return 0;

        default:
            break;
        }

        free(url);
        return -1;
    }

    //segmentTemplate

    char* mediaUrl = NULL;
    char* initTemplate = NULL;
    char* indexTemplate = NULL;

    /*apply inheritance of attributes, lowest level having preceedence*/
    if(period->segmentTemplate)
    {
        if(period->segmentTemplate->initialization)
            initTemplate = period->segmentTemplate->initialization;
        if(period->segmentTemplate->index)
            indexTemplate = period->segmentTemplate->index;
        if(period->segmentTemplate->media)
            mediaUrl= period->segmentTemplate->media;
        if(period->segmentTemplate->startNumber != (unsigned int) -1)
            startNumber = period->segmentTemplate->startNumber;
        if(period->segmentTemplate->segmentTimeline)
            timeLine = period->segmentTemplate->segmentTimeline;
    }

    if(set->segmentTemplate)
    {
        if(set->segmentTemplate->initialization)
            initTemplate = set->segmentTemplate->initialization;
        if(set->segmentTemplate->index)
            indexTemplate = set->segmentTemplate->index;
        if(set->segmentTemplate->media)
            mediaUrl= set->segmentTemplate->media;
        if(set->segmentTemplate->startNumber != (unsigned int) -1)
            startNumber = set->segmentTemplate->startNumber;
        if(set->segmentTemplate->segmentTimeline)
            timeLine = set->segmentTemplate->segmentTimeline;
    }

    if(rep->segmentTemplate)
    {
        if(rep->segmentTemplate->initialization)
            initTemplate = rep->segmentTemplate->initialization;
        if(rep->segmentTemplate->index)
            indexTemplate = rep->segmentTemplate->index;
        if(rep->segmentTemplate->media)
            mediaUrl= rep->segmentTemplate->media;
        if(rep->segmentTemplate->startNumber != (unsigned int) -1)
            startNumber = rep->segmentTemplate->startNumber;
        if(rep->segmentTemplate->segmentTimeline)
            timeLine = rep->segmentTemplate->segmentTimeline;
    }

    if(!mediaUrl)
    {
        awBaseURL* base = aw_list_get(rep->baseURLs, 0);
        if(!base)
        {
            free(url);
            return -1;
        }
        mediaUrl = (char*)base->URL;
    }
    char* urlToSolve;
    switch(resolveType)
    {
    case CDX_DASH_RESOLVE_URL_INIT:
        urlToSolve = initTemplate;
        break;
    case CDX_DASH_RESOLVE_URL_MEDIA:
    case CDX_DASH_RESOLVE_URL_MEDIA_TEMPLATE:
        urlToSolve = mediaUrl;
        break;
    case CDX_DASH_RESOLVE_URL_INDEX:
        urlToSolve = indexTemplate;
        break;
    default:
        free(url);
        return -1;
    }
    if(!urlToSolve)
    {
        free(url);
        return -1;
    }

    /*let's solve the template*/    // url have  ' $XXX$ ', we should solve it
    char* solvedTemplate = (char*)malloc( strlen(urlToSolve)+(rep->id? strlen(rep->id) : 0)*2 );
    solvedTemplate[0] = 0;
    strcpy(solvedTemplate, urlToSolve);
    char* firstSep = strchr(solvedTemplate, '$');
    if(firstSep) firstSep[0] = 0;

    firstSep = strchr(urlToSolve, '$');
    while(firstSep)
    {
        char  cdPrintFormat[50];
        char  cdFormat[100];
        char* formatTag;
        //if the segment template url is "XXX$XXX", it is error
        char* secondSep = strchr(firstSep+1, '$');
        if(!secondSep)
        {
            free(url);
            free(solvedTemplate);
            printf("the template url is not right.\n");
            return -1;
        }

        secondSep[0] = 0;
        formatTag = strchr(firstSep+1, '%');

        // there is a '%' between the two '$'
        if(formatTag)
        {
            strcpy(cdPrintFormat, formatTag);
            formatTag[0] = 0;
            if(!strchr(formatTag+1, 'd') && !strchr(formatTag+1, 'i') && !strchr(formatTag, 'u'))
            {
                strcat(cdPrintFormat, "d");
            }
        }
        else
        {
            strcpy(cdPrintFormat, "%d");
        }
        // end

        //if the template url hvae '$$', replace by '$'
        if(!strlen(firstSep+1))
        {
            strcat(solvedTemplate, "S");
        }
        else if(!strcmp(firstSep+1, "RepresentationID"))
        {
            strcat(solvedTemplate, rep->id);
        }
        else if(!strcmp(firstSep+1, "Number"))
        {
            if(resolveType == CDX_DASH_RESOLVE_URL_MEDIA_TEMPLATE)
            {
                strcat(solvedTemplate, "$Number$");
            }
            else
            {
                // replace the '$Number$' with ( startNumber + itemIndex )
                sprintf(cdFormat, cdPrintFormat, startNumber + itemIndex);
                strcat(solvedTemplate, cdFormat);
            }
        }
        else if(!strcmp(firstSep+1, "Index"))
        {
            CDX_LOGI("wrong template index, using number instead.\n ");
            sprintf(cdFormat, cdPrintFormat, startNumber + itemIndex);
            strcat(solvedTemplate, cdFormat);
        }
        else if(!strcmp(firstSep+1, "Bandwidth"))
        {
            sprintf(cdFormat, cdPrintFormat, rep->bandwidth);
            strcat(solvedTemplate, cdFormat);
        }
        else if(!strcmp(firstSep+1, "Time"))
        {
            if(resolveType == CDX_DASH_RESOLVE_URL_MEDIA_TEMPLATE)
            {
                strcat(solvedTemplate, "$Time$");
            }
            else if(timeLine)
            {
                // use segment timeline
                int k, numSeg, cur_idx, numRepeat;
                long long int time, start_time;
                numSeg = aw_list_count(timeLine->entries);
                cur_idx = 0;
                start_time=0;
                for (k=0; k<numSeg; k++)
                {
                    awSegmentTimelineEntry *ent = aw_list_get(timeLine->entries, k);
                    if (itemIndex > cur_idx+(int)ent->r)
                    {
                        cur_idx += 1 + ent->r;
                        if (ent->t) start_time = ent->t;

                        start_time += ent->d * (1 + ent->r);
                        continue;
                    }
                    *segmentDuration = ent->d;
                    *segmentDuration = (int) ((double) (*segmentDuration) * 1000.0 / timescale);
                    numRepeat = itemIndex - cur_idx;
                    time = ent->t ? ent->t : start_time;
                    time += numRepeat * ent->d;

                    /*replace final 'd' with LLD (%lld or I64d)*/
                    cdPrintFormat[strlen(cdPrintFormat)-1] = 0;
                    strcat(cdPrintFormat, &("%lld")[1]);
                    sprintf(cdFormat, cdPrintFormat, time);
                    strcat(solvedTemplate, cdFormat);
                    break;
                }

            }
        }
        else
        {
            CDX_LOGI("[DASH] Unknown template identifier- disabling rep\n\n");
            *outUrl = NULL;
            free(url);
            free(solvedTemplate);
            return -1;
        }

        if(formatTag) formatTag[0] = '%';
        secondSep[0] = '$';
        // look for next '$' , copy over remaining text if any
        firstSep = strchr(secondSep+1, '$');
        if(firstSep) firstSep[0] = 0;
        if (strlen(secondSep+1))
            strcat(solvedTemplate, secondSep+1);
        if (firstSep) firstSep[0] = '$';
    }

    *outUrl = CdxUrlConcatenate(url, solvedTemplate);
    free(url);
    free(solvedTemplate);
    return 0;
}

/******************************************************************************************/
//****<  init audio and video segment url and connect it by httpstream
/******************************************************************************************/
int CdxDashInitSegmentUrl(CdxDashParser* dashStream, int type)
{
    int ret;
    char* initUrl = NULL;
    long long int segmentDuration;
    if(!dashStream) return -1;
    awMpd* mpd = dashStream->mpd;
    awPeriod* period = aw_list_get(mpd->periods, dashStream->activePeriodIndex);
    awAdaptionSet* v_set = aw_list_get(period->adaptationSets,
                                       dashStream->mediaStream[type].activeAdapSetIndex);
    if(!v_set)
    {
        return -1;
    }

    awRepresentation* v_rep = aw_list_get(v_set->representation,
                                          dashStream->mediaStream[type].activeRepSetIndex);
    if(!v_rep )
    {
        return -1;
    }

    if(!aw_list_count(v_rep->baseURLs))
    {
        CDX_LOGD("--- segment mp4 file");
        dashStream->mediaStream[type].flag |= SEGMENT_MP4;
    }
    CDX_LOGD("++++ baseUrl num = %d", aw_list_count(v_rep->baseURLs));

    // video representation
    //if(v_rep)
    {
        ret = CdxDashResolveUrl(dashStream, period, v_set, v_rep,
                                CDX_DASH_RESOLVE_URL_INIT, 0, &initUrl, &segmentDuration);
        if(ret < 0)
        {
            CDX_LOGE("resolve init segment url error!\n");
            if(initUrl)
                free(initUrl);
            return ret;
        }
        //CDX_LOGD("--dash-- init url = %s", initUrl);

        // there is no init segment, the init segment is in the first media segment
        if(!initUrl)
        {
            return 0;
        }

        if(!strstr(initUrl, "://") || !strncasecmp(initUrl, "file://", 7) ||
                !strncasecmp(initUrl, "gmem://", 7) || !strncasecmp(initUrl, "views://", 8))
        {

        }

        dashStream->mediaStream[type].datasource->uri = initUrl;

        //*****< parser prepare, get the media stream parser
        ret = CdxParserPrepare(dashStream->mediaStream[type].datasource,
                               dashStream->mediaStream[type].flag,
                               &dashStream->mutex,
                               &dashStream->exitFlag,
                               &dashStream->mediaStream[type].parser,
                               &dashStream->mediaStream[type].stream,
                               NULL, NULL);
        if(ret < 0)
        {
            CDX_LOGE("--- parser prepare failed");
            return -1;
        }
    }
    return 0;
}

/******************************************************************************************/
//****<  get the media stream segment url
/******************************************************************************************/
int CdxDashGetSegmentUrl(CdxDashParser* dashStream,
                         awPeriod* period,
                         awAdaptionSet* set,
                         awRepresentation* rep,
                         int activeSegmentIndex)
{
    int ret;
    long long int segmentDuration;
    char* segmentUrl = NULL;

    ret = CdxDashResolveUrl(dashStream, period, set, rep, CDX_DASH_RESOLVE_URL_MEDIA,
                            activeSegmentIndex, &segmentUrl, &segmentDuration);
    if(ret)
    {
        CDX_LOGE("resolve init segment url error!\n");
        if(segmentUrl)
        {
            free(segmentUrl);
        }
        return ret;
    }
    CDX_LOGD("**** segment url = %s", segmentUrl);
    dashStream->tmpUrl = segmentUrl;
    // if the segment is not exist
    if(!segmentUrl)
    {
        return -1;
    }

    return 0;
}

/******************************************************************************************/
//****<  used by seekto
//****< NOTE: if the active segment index not changed,
//****< we need not open a new parser,just use the old one
/******************************************************************************************/
int CdxDashSeekToTime(CdxDashParser* impl, cdx_int64 timeMs, int type,
                                 SeekModeType seekModeType)
{
    int ret = 0;
    long long int segmentDuration;
    cdx_int64 time = 0;  // ms
    awMpd* mpd = impl->mpd;

    awPeriod* period = aw_list_get(mpd->periods, impl->activePeriodIndex);
    awAdaptionSet* set = aw_list_get(period->adaptationSets,
                                     impl->mediaStream[type].activeAdapSetIndex);

    //***< reset these variables
    impl->mediaStream[type].eou = 0;
    impl->mediaStream[type].eos = 0;

    //****< get the active segment index by  timeUs
    ret = CdxDashGetSegmentIndex(impl->mediaStream[type].activeRepSetIndex,
                                 set, period, mpd, timeMs, &segmentDuration);
    if(ret < 0)
    {
        CDX_LOGW("-- get segment index error!");
        return -1;
    }

    //***< if the activeRepSetIndex is change, we need replace stream
    if(ret != impl->mediaStream[type].activeRepSetIndex)
    {
        impl->mediaStream[type].activeSegmentIndex = ret;
        time = ret*segmentDuration;

        awRepresentation* rep = aw_list_get(set->representation,
                                            impl->mediaStream[type].activeRepSetIndex);

        //get the right segment url
        ret = CdxDashGetSegmentUrl(impl, period, set, rep,
                                   impl->mediaStream[type].activeSegmentIndex);
        if(ret < 0)
        {
            return ret;
        }

        impl->mediaStream[type].datasource->uri = impl->tmpUrl;

        ret = CdxStreamOpen(impl->mediaStream[type].datasource, &impl->mutex, &impl->exitFlag,
                            &impl->mediaStream[type].stream, NULL);
        if(ret < 0)
        {
            CDX_LOGW("the stream can not open");
            return -1;
        }
        CDX_LOGD("segment url = %s", impl->mediaStream[type].datasource->uri);
        //impl->mediaStream[type].parser = CdxParserPrepare(
        //                                  impl->mediaStream[type].datasource, 0, &impl->exitFlag);
        ret = CdxParserControl(impl->mediaStream[type].parser,
                               CDX_PSR_CMD_REPLACE_STREAM, (void*) impl->mediaStream[type].stream);
        if(ret < 0)
        {
            CDX_LOGW("*** dash replace stream error!");
            return -1;
        }

        impl->mediaStream[type].activeSegmentIndex ++;
    }

    impl->mediaStream[type].baseTime = time*1000;
    CDX_LOGD("---- baseTime = %lld, activeSegmentIndex = %d",
             time, impl->mediaStream[type].activeSegmentIndex);
    ret = CdxParserSeekTo(impl->mediaStream[type].parser, (timeMs-time)*1000, seekModeType);
    if(ret < 0)
    {
        return -1;
    }

    return 0;
}

/****************************************************************************/
//****< get the representation set index from bandwidth
/****************************************************************************/
static int CdxDashRepIndexFromBandwidth(CdxDashParser* impl, unsigned int bandwidth, int type)
{
    int repIdx = impl->mediaStream[type].activeRepSetIndex;
    awMpd* mpd = impl->mpd;
    awPeriod* period;
    awAdaptionSet* set;
    awRepresentation* rep;

    period = aw_list_get(mpd->periods, impl->activePeriodIndex);
    set = aw_list_get(period->adaptationSets, impl->mediaStream[type].activeAdapSetIndex);

    int count;
    int i;
    unsigned int bd;
    int diff = 0;
    int tmp = 0xffffffff;
    count = aw_list_count(set->representation);
    for(i=0; i<count; i++)
    {
        rep = aw_list_get(set->representation, i);
        if(!rep) return -1;
        bd = rep->bandwidth/1000;

        diff = bandwidth - bd;
        if((diff > 0) && (diff < tmp))
        {
            repIdx = i;
            tmp = diff;
        }
    }

#if 1
    if(repIdx == impl->mediaStream[type].activeRepSetIndex)
    {
        //  no need swith stream
        return 0;
    }

    // need switch stream
    impl->mediaStream[type].activeRepSetIndex = repIdx;
#else
    impl->mediaStream[type].activeRepSetIndex = (impl->mediaStream[type].activeRepSetIndex + 1)%4;

#endif
    CDX_LOGD("---- switch stream: %d", repIdx);
    return 1;
}

/****************************************************************************/
//****< get the media stream packet in the media stream thread,
//****< but is not used now
/****************************************************************************/
static int CdxDashMediaStreamPacket(CdxDashParser* impl, CdxPacketT * pkt, int type)
{
    int ret;
    awMpd* mpd = impl->mpd;
    awPeriod* period;
    awAdaptionSet* set;
    awRepresentation* rep;

    int status = CdxParserGetStatus(impl->mediaStream[type].parser);
    if(status == PSR_INVALID)
    {
        return -1;
    }
    else if(status == PSR_OK)
    {
        ret = CdxParserPrefetch(impl->mediaStream[type].parser, pkt);
        if(ret < 0)
        {
            CDX_LOGE(" prefetch error!");
            return -1;
        }
        ret = CdxParserRead(impl->mediaStream[type].parser, pkt);
        if(ret < 0)
        {
            CDX_LOGE(" read error!");
            return -1;
        }
    }
    else if(status == PSR_EOS)
    {
        //***< the media segment is end, so we need get a new url of the media segment.
        //***< if there is no url, this stream is end
        if(impl->mediaStream[type].eou == 1)
        {
            impl->mediaStream[type].eos = 1;
            return 0;
        }

        period = aw_list_get(mpd->periods, impl->activePeriodIndex);
        set = aw_list_get(period->adaptationSets, impl->mediaStream[type].activeAdapSetIndex);
        rep = aw_list_get(set->representation, impl->mediaStream[type].activeRepSetIndex);
        if(!rep) return -1;

        //***< get next segment url
        ret = CdxDashGetSegmentUrl(impl, period, set, rep,
                                   impl->mediaStream[type].activeSegmentIndex);
        if(ret < 0)
        {
            return ret;
        }
        impl->mediaStream[type].datasource->uri = impl->tmpUrl;

        ret = CdxParserPrepare(impl->mediaStream[type].datasource,
                               impl->mediaStream[type].flag,
                               &impl->mutex,
                               &impl->exitFlag,
                               &impl->mediaStream[type].parser,
                               &impl->mediaStream[type].stream,
                               NULL, NULL);
        if(ret < 0)
        {
            //***< the url is not exist, so the end of url
            impl->mediaStream[type].eou = 1;
            CDX_LOGI("end of segment url");
            return 0;
        }
        impl->mediaStream[type].activeSegmentIndex ++;
    }
    return 0;
}

/****************************************************************************/
//****< not used now
/****************************************************************************/
void* CdxDashVideoStreamThread(void* p_arg)
{
    int ret;
    CdxDashParser* impl = (CdxDashParser*)p_arg;
    while(1)
    {
        // *****< force stop
        if(impl->exitFlag == 1)
        {
            continue;
        }

        // *****< the media stream is end of downloading, we do not stop.
        //*****<  because it maybe seek
        if(impl->mediaStream[0].eos == 1)
        {
            continue;
        }

        CdxPacketT *pkt = (CdxPacketT*)malloc(sizeof(CdxPacketT));
        if(!pkt)
        {
            CDX_LOGE("malloc a packet error!");
            break;
        }
        ret = CdxDashMediaStreamPacket(impl, pkt, 0);
        if(ret < 0)
        {
            CDX_LOGE(" prefetch error!");
            break;
        }

        ret = aw_list_add(impl->mediaStream[0].packetList, pkt);
        if(ret < 0)
        {
            break;
        }
    }

    return NULL;
}

/****************************************************************************/
//****< not used now
/****************************************************************************/
#if 0
void* CdxDashAudioThread(void* p_arg)
{
    CdxDashParser* dashStream = (CdxDashParser*)p_arg;
    return NULL;
}
#endif
/****************************************************************************/
//****< download the mpd file and parse it
//****< we do not have any video, audio, subtitle thread
//****< maybe we should do this if it is  not effective after test
/****************************************************************************/
void* CdxDashOpenThread(void* p_arg)
{
    int ret;
    CdxDashParser* impl = (CdxDashParser*)p_arg;
    CdxAtomicInc(&impl->ref);

    //****< get the mpd and parse it
    cdx_int64 readSize = 0;
    while(readSize < impl->mpdSize)
    {
        if(impl->exitFlag == 1)
        {
            goto err;
        }
        ret = CdxStreamRead(impl->stream, impl->mpdBuffer+readSize, impl->mpdSize-readSize);
        if(ret < 0)
        {
            CDX_LOGW("--- download mpd file error");
            goto err;
        }
        readSize += ret;
    }

    //CdxStreamClose(impl->stream);
    impl->mpd = ParseMpdFile(NULL, -1, impl->mpdBuffer,readSize, NULL, 0);
    if(!impl->mpd)
    {
        CDX_LOGE("parse mpd file error!");
        goto err;
    }

    /****< Get the right period from initialize time 0 */
    impl->activePeriodIndex = CdxDashPeriodIndexFromTime(impl, 0);
    //****<  if there is no adaptationset in this  period , error!
    awPeriod* period = aw_list_get(impl->mpd->periods, impl->activePeriodIndex);
    if(!period)
    {
        CDX_LOGE("Error - cannot start: no enough periods or representation.\n");
        goto err;
    }

    int nb_set = aw_list_count(period->adaptationSets);
    CDX_LOGI("the number of adaptation set is %d", nb_set);
    if(!nb_set)
    {
        CDX_LOGE("Error - cannot start: no enough periods or representation.\n");
        goto err;
    }

    //***< set up the video and audio adaptationset and the right representation
    ret = CdxDashRepIndexInit(impl, period);
    if(ret < 0)
    {
        goto err;
    }

#if DASH_DISABLE_AUDIO
    impl->hasAudio = 0;
#endif

    //***< init the  video and audio segment url
    if(impl->hasVideo)
    {
        ret = CdxDashInitSegmentUrl(impl, 0);
        if(ret < 0)
        {
            CDX_LOGE("video _initsegment_url error!\n");
            goto err;
        }
        impl->streamPts[0] = 0;
        impl->mediaStream[0].eos = 0;
    }

    if(impl->hasAudio)
    {
        ret = CdxDashInitSegmentUrl(impl, 1);
        if(ret < 0)
        {
            CDX_LOGE("audio _initsegment_url error!\n");
            goto err;
        }
        impl->streamPts[1] = 0;
        impl->mediaStream[1].eos = 0;
    }
    if(impl->hasSubtitle)
    {
        ret = CdxDashInitSegmentUrl(impl, 2);
        if(ret < 0)
        {
            CDX_LOGE("subtitle _initsegment_url error!\n");
            goto err;
        }
        impl->streamPts[2] = 0;
        impl->mediaStream[2].eos = 0;
    }

    CdxAtomicDec(&impl->ref);
    impl->mStatus = CDX_DASH_IDLE;
    impl->mErrno = PSR_OK;
    return NULL;

err:
    CDX_LOGW("--- open failed");
    impl->mStatus = PSR_OPEN_FAIL;
    CdxAtomicDec(&impl->ref);
    return NULL;
}

static void CdxDashFreeMediaStream(CdxDashMedia* mediaStream)
{
    if(mediaStream->datasource)
    {
        if(mediaStream->datasource->uri)
        {
            free(mediaStream->datasource->uri);
            mediaStream->datasource->uri = NULL;
        }
        free(mediaStream->datasource);
        mediaStream->datasource = NULL;
    }

    if(mediaStream->packetList)
    {
        unsigned int i;
        CdxPacketT* packet;
        unsigned int totalNumber = aw_list_count(mediaStream->packetList);
        for(i=0; i<totalNumber; i++)
        {
            packet = aw_list_get(mediaStream->packetList, i);
            if(packet)
            {
                free(packet);
                packet = NULL;
            }
        }
        aw_list_del(mediaStream->packetList);
        mediaStream->packetList = NULL;
    }
    if(mediaStream->parser)
    {
        CdxParserClose(mediaStream->parser);
        mediaStream->parser = NULL;
    }
    else if(mediaStream->stream)
    {
        CdxStreamClose(mediaStream->stream);
        mediaStream->stream = NULL;
    }
}

static int CdxDashInitMediaStream(CdxDashMedia* mediaStream)
{
    if(!mediaStream)
    {
        return -1;
    }

    mediaStream->datasource = (CdxDataSourceT*)malloc(sizeof(CdxDataSourceT));
    if(!mediaStream->datasource)
    {
        return -1;
    }
    memset(mediaStream->datasource, 0, sizeof(CdxDataSourceT));

    mediaStream->activeAdapSetIndex = -1;  //video set
    mediaStream->activeRepSetIndex = -1;  //bandwidth
    mediaStream->eos = 1;

    mediaStream->packetList = aw_list_new();
    if(!mediaStream->packetList)
    {
        return -1;
    }

    return 0;
}

static int CdxDashGetCacheState(CdxDashParser *impl, struct ParserCacheStateS *cacheState)
{
    //struct ParserCacheStateS parserCS;

    // TODO: we only get the video cacheState, excluding audio
    if (CdxParserControl(impl->mediaStream[0].parser, CDX_PSR_CMD_GET_CACHESTATE, cacheState) < 0)
    {
        CDX_LOGE("CDX_PSR_CMD_STREAM_CONTROL fail");
        return -1;
    }

    return 0;
}

/**************************************************************************************************/
//****< check whether we need switch video stream if bandwith changed
//****<
//****< return value:    1:  need switch
//****<                             0:  not need
//****<                           -1:  error
/**************************************************************************************************/
static int CdxDashCheckVideoStream(CdxDashParser *impl)
{
    struct ParserCacheStateS parserCS;
    int ret;

    ret = CdxDashGetCacheState(impl, &parserCS);
    if(ret < 0)
    {
        CDX_LOGE("get cache state failed");
        return -1;
    }

    ret = CdxDashRepIndexFromBandwidth(impl, parserCS.nBandwidthKbps, 0);
    if(ret < 0)
    {
        CDX_LOGE("get representation index failed");
        return -1;
    }

    return ret;
}

static void CdxDashClose(CdxDashParser *parser)
{
    if(parser)
    {
        parser->exitFlag = 1;
        if(parser->stream)
        {
            CdxStreamClose(parser->stream);
            parser->stream = NULL;
        }
        if(parser->mpd)
        {
            MpdFree(parser->mpd);
            parser->mpd = NULL;
        }
        if(parser->mpdUrl)
        {
            free(parser->mpdUrl);
            parser->mpdUrl = NULL;
        }
        if(parser->mpdBuffer)
        {
            free(parser->mpdBuffer);
            parser->mpdBuffer = NULL;
        }
        int i;
        for(i=0; i<3; i++)
        {
            if(&parser->mediaStream[i])
                CdxDashFreeMediaStream(&parser->mediaStream[i]);
        }

        pthread_mutex_destroy(&parser->mutex);
        free(parser);
    }
}

static cdx_int32 __CdxDashParserClose(CdxParserT *parser)
{
    CdxDashParser          *DashPsr;
    DashPsr = (CdxDashParser *)parser;
    int ret;

    int i;
    DashPsr->exitFlag = 1;

    CdxAtomicDec(&DashPsr->ref);
    while ((ret = CdxAtomicRead(&DashPsr->ref)) != 0)
    {
        CDX_LOGD("wait for ref = %d", ret);
        usleep(10000);
    }
    pthread_join(DashPsr->openTid, NULL);

    CdxDashClose(DashPsr);
    return 0;
}

static cdx_int32 __CdxDashParserPrefetch(CdxParserT *parser, CdxPacketT * pkt)
{
    int ret;
    CdxDashParser          *impl;
    impl = (CdxDashParser *)parser;
    awMpd* mpd = impl->mpd;
    awPeriod* period;
    awAdaptionSet* set;
    awRepresentation* rep;

    if(impl->mStatus == CDX_DASH_PREFETCHED)
    {
        CDX_LOGW("care, prefetch in prefected status...");
        pkt->length   = impl->packet.length;
        pkt->type     = impl->packet.type;
        pkt->pts      = impl->packet.pts;
        pkt->duration = impl->packet.duration;
        pkt->flags    = impl->packet.flags;
        // it is prefetched status, so do not need to change it
        return 0;
    }

    if((impl->mediaStream[0].eos ) && (impl->mediaStream[1].eos) && (impl->mediaStream[2].eos))
    {
        CDX_LOGD("dash parser end, status: %d", impl->mStatus);
        impl->mErrno = PSR_EOS;
        impl->mStatus = CDX_DASH_IDLE;
        return -1;
    }

    impl->prefetchType = 0;
    if(impl->streamPts[1] < impl->streamPts[0])
    {
        impl->prefetchType = 1;
    }
    if((impl->streamPts[2] < impl->streamPts[1])&&(impl->streamPts[2] < impl->streamPts[0]))
    {
        impl->prefetchType = 2;
    }

    impl->mStatus = CDX_DASH_PREFETCHING;

    int status = CdxParserGetStatus(impl->mediaStream[impl->prefetchType].parser);
    if(status == PSR_INVALID)
    {
        CDX_LOGD("the status is PSR_INVALID, we can not prefetch");
        impl->mStatus = CDX_DASH_IDLE;
        return -1;
    }
    else if(status == PSR_OK)
    {
        ret = CdxParserPrefetch(impl->mediaStream[impl->prefetchType].parser, pkt);
        if(ret < 0)
        {
            // maybe is not end
            if(impl->exitFlag)
            {
                impl->mStatus = CDX_DASH_IDLE;
                CDX_LOGW("--- force Stop");
                return -1;
            }
            impl->mStatus = CDX_DASH_PREFETCHING;
            return CdxParserPrefetch(parser, pkt);
            //return -1;
        }
        pkt->pts += impl->mediaStream[impl->prefetchType].baseTime;
    }
    else if(status == PSR_EOS)
    {
        //***< the media segment is end, so we need get a new url of the media segment.
        //***< if there is no url, this stream is end
        if(impl->mediaStream[impl->prefetchType].eou == 1)
        {
            impl->mediaStream[impl->prefetchType].eos = 1;
            // maybe the pts is small in live play
            impl->streamPts[impl->prefetchType] = 0xfffffffffffffff;
            impl->mStatus = CDX_DASH_PREFETCHING;
            return CdxParserPrefetch(parser, pkt);
        }

        period = aw_list_get(mpd->periods, impl->activePeriodIndex);
        set = aw_list_get(period->adaptationSets,
                          impl->mediaStream[impl->prefetchType].activeAdapSetIndex);
        rep = aw_list_get(set->representation,
                          impl->mediaStream[impl->prefetchType].activeRepSetIndex);
        if(!rep)
        {
            CDX_LOGE("-- get representation failed, repIdx(%d)",
                     impl->mediaStream[impl->prefetchType].activeRepSetIndex);
            impl->mStatus = CDX_DASH_IDLE;
            return -1;
        }

// resolution change
#if 0
        if(impl->prefetchType == 0)
        {
            ret = CdxDashCheckVideoStream(impl);
            if(ret < 0)
            {
                CDX_LOGE("-- check video stream failed");
                return -1;
            }
            else if(ret == 1)
            {
                // need change resolution for video stream
                CDX_LOGD("--- change resolution");
                if(impl->hasVideo)
                {
                    ret = CdxDashInitSegmentUrl(impl, 0);
                    if(ret < 0)
                    {
                        CDX_LOGE("video _initsegment_url error!\n");
                        return -1;
                    }

                    CdxMediaInfoT mediaInfo;
                    CdxParserGetMediaInfo(impl->mediaStream[impl->prefetchType].parser, &mediaInfo);

                    //char buffer[1024];
                    char* buf = impl->extraBuffer+4;
                    int i;
                    int size = 0;
                    H264Extract(mediaInfo.program[0].video[0].pCodecSpecificData,
                                mediaInfo.program[0].video[0].nCodecSpecificDataLen, &buf, &size);

                    CDX_LOGD("-- extradataSize = %d, size=%d",
                             mediaInfo.program[0].video[0].nCodecSpecificDataLen, size);
                    buf = impl->extraBuffer;
                    Wb32(&buf, size);

                    for(i=0; i<size+4; i++)
                    {
                        CDX_LOGD("buffer[%d]=%d", i, impl->extraBuffer[i]);
                    }

                    impl->extraBufferSize = size +4;
                    impl->switchFlag = 1;
                }
            }
        }
#endif

        //***< get next segment url
        ret = CdxDashGetSegmentUrl(impl, period, set, rep,
                                   impl->mediaStream[impl->prefetchType].activeSegmentIndex);
        if(ret < 0)
        {
            impl->mStatus = CDX_DASH_IDLE;
            CDX_LOGE("** get next segment Url error");
            return ret;
        }
        impl->mediaStream[impl->prefetchType].datasource->uri = impl->tmpUrl;

        ret = CdxStreamOpen(impl->mediaStream[impl->prefetchType].datasource, &impl->mutex,
                            &impl->exitFlag, &impl->mediaStream[impl->prefetchType].stream, NULL);
        if(ret < 0)
        {
            //***< the url is not exist, so the end of url
            // set the pts to the large, then prefetch again
            impl->mediaStream[impl->prefetchType].eou = 1;
            impl->mediaStream[impl->prefetchType].eos = 1;
            // maybe the pts is small in live play
            impl->streamPts[impl->prefetchType] = 0xfffffffffffffff;
            impl->mStatus = CDX_DASH_PREFETCHING;
            return CdxParserPrefetch(parser, pkt);
        }
        ret = CdxParserControl(impl->mediaStream[impl->prefetchType].parser,
                               CDX_PSR_CMD_REPLACE_STREAM,
                               (void*)impl->mediaStream[impl->prefetchType].stream);
        if(ret < 0)
        {
            CDX_LOGW("*** dash replace stream error!");
            impl->mStatus = CDX_DASH_IDLE;
            return -1;
        }

        impl->mediaStream[impl->prefetchType].activeSegmentIndex ++;

        if(impl->switchFlag == 1)
        {
            pkt->length   = impl->extraBufferSize;
            pkt->type     = CDX_MEDIA_VIDEO;
            pkt->pts      = 0;
            pkt->duration = 0;
            pkt->flags    = 0;
            impl->mStatus = CDX_DASH_PREFETCHED;

            // we should change the base time when change resolution video
            impl->mediaStream[impl->prefetchType].baseTime = impl->packet.pts;
            return 0;
        }

        ret = CdxParserPrefetch(impl->mediaStream[impl->prefetchType].parser, pkt);
        if(ret < 0)
        {
            if(!impl->exitFlag)
            {
                CDX_LOGE(" prefetch error! ret(%d)", ret);
                impl->mErrno = PSR_UNKNOWN_ERR;
            }

            impl->mStatus = CDX_DASH_IDLE;
            CDX_LOGE("--- prefetch failed");
            return -1;
        }
        pkt->pts += impl->mediaStream[impl->prefetchType].baseTime;
    }
    else if(status == PSR_USER_CANCEL)
    {
        CDX_LOGD("force stop");
        impl->mStatus = CDX_DASH_IDLE;
        return -1;
    }
    else
    {
        CDX_LOGE("error");
        impl->mErrno = PSR_IO_ERR;
        impl->mStatus = CDX_DASH_IDLE;
        return -1;
    }

    //CDX_LOGD("type = %d, pts = %lld, length = %d", pkt->type, pkt->pts, pkt->length);

    impl->streamPts[impl->prefetchType] = pkt->pts;

    impl->packet.length    = pkt->length;
    impl->packet.type      = pkt->type;
    impl->packet.pts       = pkt->pts;
    impl->packet.duration  = pkt->duration;
    impl->packet.flags     = pkt->flags;
    impl->mStatus = CDX_DASH_PREFETCHED;

    //CDX_LOGD("type = %d, streamindex = %d, pts = %lld, length = %d, duration = %lld",
    //        pkt->type, pkt->streamIndex, pkt->pts, pkt->length, pkt->duration);
    return 0;
}

static cdx_int32 __CdxDashParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    int ret = 0;
    CdxDashParser          *impl;
    impl = (CdxDashParser *)parser;
    if(impl->mErrno == PSR_INVALID)
    {
        CDX_LOGE("the status is PSR_INVALID, we can not get media info!");
        return -1;
    }

    if(impl->mStatus != CDX_DASH_PREFETCHED)
    {
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    impl->mStatus = CDX_DASH_READING;

    if(impl->switchFlag == 1)
    {

        CDX_LOGD("--- extradata stream");
        if(pkt->length <= pkt->buflen)
        {
            memcpy(pkt->buf, impl->extraBuffer, pkt->length);
        }
        else
        {
            memcpy(pkt->buf, impl->extraBuffer, pkt->buflen);
            memcpy(pkt->ringBuf, impl->extraBuffer + pkt->buflen, pkt->length - pkt->buflen);
        }
        impl->switchFlag = 0;
    }
    else
    {
        ret = CdxParserRead(impl->mediaStream[impl->prefetchType].parser, pkt);
    }

    impl->mStatus = CDX_DASH_IDLE;
    return ret;
}

static cdx_int32 __CdxDashParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT * pMediaInfo)
{
    int ret;
    int i;
    CdxDashParser          *impl;
    impl = (CdxDashParser *)parser;
    CdxMediaInfoT* pVideoMediaInfo = NULL;
    CdxMediaInfoT* pAudioMediaInfo = NULL;
    CdxMediaInfoT* pSubtitleMediaInfo = NULL;

    while(impl->mErrno == PSR_INVALID)
    {
        usleep(100);
        CDX_LOGE("the status is PSR_INVALID, we can not get media info!");
    }
    memset(pMediaInfo, 0, sizeof(CdxMediaInfoT));

    if(impl->hasVideo)
    {
        pVideoMediaInfo = (CdxMediaInfoT*)malloc(sizeof(CdxMediaInfoT));
        if(!pVideoMediaInfo) goto error;
        memset(pVideoMediaInfo, 0, sizeof(CdxMediaInfoT));

        ret = CdxParserGetMediaInfo(impl->mediaStream[0].parser, pVideoMediaInfo);
        if(ret < 0)
        {
            CDX_LOGE("video stream getMediaInfo error!");
            goto error;
        }
        pMediaInfo->program[0].videoNum = pVideoMediaInfo->program[0].videoNum;
        pMediaInfo->program[0].videoIndex = pVideoMediaInfo->program[0].videoIndex;

        if(pMediaInfo->program[0].videoNum > 1)
        {
            CDX_LOGE("  video number(%d) > 1", pMediaInfo->program[0].videoNum);
            goto error;
        }

        for(i=0; i<pMediaInfo->program[0].videoNum; i++)
        {
            memcpy(&pMediaInfo->program[0].video[i], &pVideoMediaInfo->program[0].video[i],
                    sizeof(VideoStreamInfo));
        }
    }
    if(impl->hasAudio)
    {
        pAudioMediaInfo = (CdxMediaInfoT*)malloc(sizeof(CdxMediaInfoT));
        if(!pAudioMediaInfo) goto error;
        memset(pAudioMediaInfo, 0, sizeof(CdxMediaInfoT));

        ret = CdxParserGetMediaInfo(impl->mediaStream[1].parser, pAudioMediaInfo);
        if(ret < 0)
        {
            CDX_LOGE("video stream getMediaInfo error!");
            goto error;
        }
        pMediaInfo->program[0].audioNum = pAudioMediaInfo->program[0].audioNum;
        pMediaInfo->program[0].audioIndex = pAudioMediaInfo->program[0].audioIndex;
        for(i=0; i<pMediaInfo->program[0].audioNum; i++)
        {
            //pMediaInfo->program[0].audio[i] = pVideoMediaInfo->program[0].audio[i];
            memcpy(&pMediaInfo->program[0].audio[i], &pAudioMediaInfo->program[0].audio[i],
                    sizeof(AudioStreamInfo));
        }
    }
    if(impl->hasSubtitle)
    {
        pSubtitleMediaInfo = (CdxMediaInfoT*)malloc(sizeof(CdxMediaInfoT));
        if(!pSubtitleMediaInfo) goto error;
        memset(pSubtitleMediaInfo, 0, sizeof(CdxMediaInfoT));

        ret = CdxParserGetMediaInfo(impl->mediaStream[2].parser, pSubtitleMediaInfo);
        if(ret < 0)
        {
            CDX_LOGE("video stream getMediaInfo error!");
            goto error;
        }
        pMediaInfo->program[0].subtitleNum = pSubtitleMediaInfo->program[0].subtitleNum;
        pMediaInfo->program[0].subtitleIndex = pSubtitleMediaInfo->program[0].subtitleIndex;
        for(i=0; i<pMediaInfo->program[0].subtitleNum; i++)
        {
            //pMediaInfo->program[0].subtitle[i] = pVideoMediaInfo->program[0].subtitle[i];
            memcpy(&pMediaInfo->program[0].subtitle[i],
                   &pSubtitleMediaInfo->program[0].subtitle[i], sizeof(SubtitleStreamInfo));
        }
    }

    CDX_LOGD("--- videoNum = %d, audioNum = %d",
             pMediaInfo->program[0].videoNum, pMediaInfo->program[0].audioNum);

    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pMediaInfo->bSeekable = impl->mpd->mediaPresentationDuration > 0 ? 1: 0;

    if(impl->mpd->mediaPresentationDuration)
    {
        pMediaInfo->program[0].duration = impl->mpd->mediaPresentationDuration;
    }
    else
    {
        if(pVideoMediaInfo)
        {
            pMediaInfo->program[0].duration = pVideoMediaInfo->program[0].duration;
        }
        else
        {
            pMediaInfo->program[0].duration = pAudioMediaInfo->program[0].duration;
        }
    }

    if(pVideoMediaInfo)
    {
        free(pVideoMediaInfo);
        pVideoMediaInfo = NULL;
    }
    if(pAudioMediaInfo)
    {
        free(pAudioMediaInfo);
        pAudioMediaInfo = NULL;
    }
    if(pSubtitleMediaInfo)
    {
        free(pSubtitleMediaInfo);
        pSubtitleMediaInfo = NULL;
    }
    impl->mStatus = CDX_DASH_IDLE;
    return 0;

error:
    if(pVideoMediaInfo)
    {
        free(pVideoMediaInfo);
        pVideoMediaInfo = NULL;
    }
    if(pAudioMediaInfo)
    {
        free(pAudioMediaInfo);
        pAudioMediaInfo = NULL;
    }
    if(pSubtitleMediaInfo)
    {
        free(pSubtitleMediaInfo);
        pSubtitleMediaInfo = NULL;
    }
    return -1;
}

cdx_int32 __CdxDashParserForceStop(CdxParserT *parser)
{
    int ret;
    CdxDashParser          *impl;
    impl = (CdxDashParser *)parser;

    impl->exitFlag = 1;
    impl->mErrno = PSR_USER_CANCEL;

    pthread_mutex_lock(&impl->mutex);
    ret = CdxStreamForceStop(impl->stream);
    if(impl->hasVideo)
    {
        // change video resolution, the handler of parser is NULL
        if(impl->mediaStream[0].parser)
            ret = CdxParserForceStop(impl->mediaStream[0].parser);
    }

    if(impl->hasAudio && impl->mediaStream[1].parser)
    {
        ret = CdxParserForceStop(impl->mediaStream[1].parser);
    }

    if(impl->hasSubtitle && impl->mediaStream[2].parser)
    {
        ret = CdxParserForceStop(impl->mediaStream[2].parser);
    }
    pthread_mutex_unlock(&impl->mutex);

    while((impl->mStatus != CDX_DASH_IDLE) && (impl->mStatus != CDX_DASH_PREFETCHED))
    {
        usleep(1000);
    }

    impl->mStatus = CDX_DASH_IDLE;

    CDX_LOGD("-- dash forcestop end");
    return 0;
}

cdx_int32 __CdxDashParserClrForceStop(CdxParserT *parser)
{
    int ret;
    CdxDashParser          *impl;
    impl = (CdxDashParser *)parser;

    pthread_mutex_lock(&impl->mutex);
    ret = CdxStreamClrForceStop(impl->stream);
    if(impl->hasVideo && impl->mediaStream[0].parser)
    {
        ret = CdxParserClrForceStop(impl->mediaStream[0].parser);
    }

    if(impl->hasAudio && impl->mediaStream[1].parser)
    {
        ret = CdxParserClrForceStop(impl->mediaStream[1].parser);
    }

    if(impl->hasSubtitle && impl->mediaStream[2].parser)
    {
        ret = CdxParserClrForceStop(impl->mediaStream[2].parser);
    }
    pthread_mutex_unlock(&impl->mutex);

    impl->exitFlag = 0;
    impl->mStatus = CDX_DASH_IDLE;
    impl->mErrno = PSR_OK;

    CDX_LOGD("-- dash clr forcestop end");
    return 0;
}

static int __CdxDashParserControl(CdxParserT *parser, int cmd, void *param)
{
    CdxDashParser          *impl;
    impl = (CdxDashParser *)parser;
    CDX_UNUSE(param);

    if(!parser)
    {
        CDX_LOGW(" the parser is not exit, error!");
        return -1;
    }

    switch(cmd)
    {
    case CDX_PSR_CMD_SWITCH_AUDIO:
    case CDX_PSR_CMD_SWITCH_SUBTITLE:
        CDX_LOGI(" dash parser is not support switch stream yet!!!");
        break;

    case CDX_PSR_CMD_SET_FORCESTOP:
        return __CdxDashParserForceStop(parser);
    case CDX_PSR_CMD_CLR_FORCESTOP:
        return __CdxDashParserClrForceStop(parser);
    default:
        break;
    }

    return 0;
}

cdx_int32 __CdxDashParserSeekTo(CdxParserT *parser, cdx_int64  timeUs, SeekModeType seekModeType)
{
    int ret;
    CdxDashParser          *impl;
    impl = (CdxDashParser *)parser;
    cdx_int64 timeMs = timeUs / 1000;

    impl->mStatus = CDX_DASH_SEEKING;
    if(!impl->mpd->mediaPresentationDuration)
    {
        CDX_LOGW("live mode is not need seekTo");
        impl->mStatus = CDX_DASH_IDLE;
        return -1;
    }

    if((timeMs > impl->mpd->mediaPresentationDuration))
    {
        impl->mErrno = PSR_EOS;
        return 0;
    }

    if((timeMs<0))
    {
        CDX_LOGE("the parameter of seekTo error, <%lld>ms", timeMs);
        impl->mStatus = CDX_DASH_IDLE;
        return -1;
    }

    //***< get the active period index
    int periodIdx = CdxDashPeriodIndexFromTime(impl, timeMs);
    if(periodIdx != impl->activePeriodIndex)
    {
        CDX_LOGW("!!! the period index changed because of seektotime, we do not support yet!");
        impl->activePeriodIndex = periodIdx;

        // TODO:
        // if the period index changed, the adaptationset index should changed at the same time,
        // so we should call the CdxDashRepIndexInit() function again
        impl->mStatus = CDX_DASH_IDLE;
        return -1;
    }

    //***<   firstly, we should get the right index of segment and then reset the variables
    if(impl->hasVideo)
    {
        ret = CdxDashSeekToTime(impl, timeMs, 0, seekModeType);
        if(ret < 0)
        {
            impl->mStatus = CDX_DASH_IDLE;
            return -1;
        }
        impl->streamPts[0] = timeUs;
    }

    if(impl->hasAudio)
    {
        ret = CdxDashSeekToTime(impl, timeMs, 1, seekModeType);
        if(ret < 0)
        {
            impl->mStatus = CDX_DASH_IDLE;
            return -1;
        }
        impl->streamPts[1] = timeUs;
    }

    if(impl->hasSubtitle)
    {
        ret = CdxDashSeekToTime(impl, timeMs, 2, seekModeType);
        if(ret < 0)
        {
            impl->mStatus = CDX_DASH_IDLE;
            return -1;
        }
        impl->streamPts[2] = timeUs;
    }
    CDX_LOGD("video pts = %lld, audio pts = %lld", impl->streamPts[0], impl->streamPts[1]);

    impl->mStatus = CDX_DASH_IDLE;
    return 0;
}

cdx_uint32 __CdxDashParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    CdxDashParser          *impl;
    impl = (CdxDashParser *)parser;
    return -1;
}

cdx_int32 __CdxDashParserGetStatus(CdxParserT *parser)
{
    CdxDashParser          *impl;
    impl = (CdxDashParser *)parser;
    return impl->mErrno;
}

static int __CdxDashParserInit(CdxParserT *parser)
{
    int ret;
    CdxDashParser* impl = (CdxDashParser*)parser;
    CdxAtomicInc(&impl->ref);

    //****< get the mpd and parse it
    cdx_int64 readSize = 0;
    while(readSize < impl->mpdSize)
    {
        if(impl->exitFlag == 1)
        {
            goto err;
        }
        ret = CdxStreamRead(impl->stream, impl->mpdBuffer+readSize, impl->mpdSize-readSize);
        if(ret < 0)
        {
            CDX_LOGW("--- download mpd file error");
            goto err;
        }
        readSize += ret;
    }

    //CdxStreamClose(impl->stream);
    impl->mpd = ParseMpdFile(NULL, -1, impl->mpdBuffer,readSize, NULL, 0);
    if(!impl->mpd)
    {
        CDX_LOGE("parse mpd file error!");
        goto err;
    }

    /****< Get the right period from initialize time 0 */
    impl->activePeriodIndex = CdxDashPeriodIndexFromTime(impl, 0);
    //****<  if there is no adaptationset in this  period , error!
    awPeriod* period = aw_list_get(impl->mpd->periods, impl->activePeriodIndex);
    if(!period)
    {
        CDX_LOGE("Error - cannot start: no enough periods or representation.\n");
        goto err;
    }

    int nb_set = aw_list_count(period->adaptationSets);
    CDX_LOGI("the number of adaptation set is %d", nb_set);
    if(!nb_set)
    {
        CDX_LOGE("Error - cannot start: no enough periods or representation.\n");
        goto err;
    }

    //***< set up the video and audio adaptationset and the right representation
    ret = CdxDashRepIndexInit(impl, period);
    if(ret < 0)
    {
        goto err;
    }

#if DASH_DISABLE_AUDIO
    impl->hasAudio = 0;
#endif

    //***< init the  video and audio segment url
    if(impl->hasVideo)
    {
        ret = CdxDashInitSegmentUrl(impl, 0);
        if(ret < 0)
        {
            CDX_LOGE("video _initsegment_url error!\n");
            goto err;
        }
        impl->streamPts[0] = 0;
        impl->mediaStream[0].eos = 0;
    }

    if(impl->hasAudio)
    {
        ret = CdxDashInitSegmentUrl(impl, 1);
        if(ret < 0)
        {
            CDX_LOGE("audio _initsegment_url error!\n");
            goto err;
        }
        impl->streamPts[1] = 0;
        impl->mediaStream[1].eos = 0;
    }
    if(impl->hasSubtitle)
    {
        ret = CdxDashInitSegmentUrl(impl, 2);
        if(ret < 0)
        {
            CDX_LOGE("subtitle _initsegment_url error!\n");
            goto err;
        }
        impl->streamPts[2] = 0;
        impl->mediaStream[2].eos = 0;
    }

    CdxAtomicDec(&impl->ref);
    impl->mStatus = CDX_DASH_IDLE;
    impl->mErrno = PSR_OK;
    return 0;

err:
    CDX_LOGW("--- open failed");
    impl->mErrno = PSR_OPEN_FAIL;
    impl->mStatus = CDX_DASH_IDLE;
    CdxAtomicDec(&impl->ref);
    return -1;

}

static struct CdxParserOpsS dashParserOps =
{
    .init           = __CdxDashParserInit,
    .control         = __CdxDashParserControl,
    .prefetch         = __CdxDashParserPrefetch,
    .read             = __CdxDashParserRead,
    .getMediaInfo     = __CdxDashParserGetMediaInfo,
    .close             = __CdxDashParserClose,
    .seekTo            = __CdxDashParserSeekTo,
    .attribute        = __CdxDashParserAttribute,
    .getStatus        = __CdxDashParserGetStatus
};

static CdxParserT *__CdxDashParserOpen(CdxStreamT *stream, cdx_uint32 flag)
{
    cdx_int32            ret;
    CdxDashParser *impl = NULL;
    CDX_UNUSE(flag);

    // init the dash parser
    impl = CdxMalloc(sizeof(CdxDashParser));
    if(impl == NULL)
    {
        CDX_LOGE("dash parser malloc error!");
        goto open_error;
    }
    memset(impl, 0x00, sizeof(CdxDashParser));
    impl->mErrno = PSR_INVALID;
    impl->parserinfo.ops = &dashParserOps;
    impl->stream = stream;
    CDX_LOGD("---- stream = %p", stream);
    impl->mStatus = CDX_DASH_INITIALIZED;
    impl->streamPts[0] = 0xfffffffffffffff;
    impl->streamPts[1] = 0xfffffffffffffff;
    impl->streamPts[2] = 0xfffffffffffffff;

    pthread_mutex_init(&impl->mutex, NULL);

    ret = CdxDashInitMediaStream(&impl->mediaStream[0]);
    if(ret < 0)
    {
        CDX_LOGE("init video stream error!");
        goto open_error;
    }
    ret = CdxDashInitMediaStream(&impl->mediaStream[1]);
    if(ret < 0)
    {
        CDX_LOGE("init audio stream error!");
        goto open_error;
    }
    ret = CdxDashInitMediaStream(&impl->mediaStream[2]);
    if(ret < 0)
    {
        CDX_LOGE("init subtitle stream error!");
        goto open_error;
    }

    // TODO:if refresh the mpd file, you should update the baseUrl at the same time
    //get the base url of mpd file.
    char* tmpUrl;
    int urlLen;
    ret = CdxStreamGetMetaData(stream, "uri", (void **)&tmpUrl);
    if(ret < 0)
    {
        CDX_LOGE("get the uri of the stream error!");
        goto open_error;
    }
    urlLen = strlen(tmpUrl)+1;
    impl->mpdUrl = malloc(urlLen);
    if(!impl->mpdUrl)
    {
        goto open_error;
    }
    memcpy(impl->mpdUrl, tmpUrl, urlLen);

    // get the mpd file size
    impl->mpdSize = CdxStreamSize(stream);
    impl->mpdBuffer = malloc(impl->mpdSize);
    if(!impl->mpdBuffer)
    {
        goto open_error;
    }
    memset(impl->mpdBuffer, 0, impl->mpdSize);
    CdxAtomicSet(&impl->ref, 1);

    //ret = pthread_create(&impl->openTid, NULL, CdxDashOpenThread, (void*)impl);
    //if(ret != 0)
    //{
    //    impl->openTid = (pthread_t)0;
    //}

    return &impl->parserinfo;

open_error:
    CDX_LOGE("open failed");
    if(stream)
    {
        CdxStreamClose(stream);
    }
    if(impl)
    {
        CdxDashClose(impl);
        impl = NULL;
    }

    return NULL;
}

static cdx_uint32 __CdxDashParserProbe(CdxStreamProbeDataT *probeData)
{
    /* check file header */
    if(strstr(probeData->buf, "MPD"))
    {
        CDX_LOGD("it is a mpd file, so dash parser");
        return 100;
    }

    return 0;
}

CdxParserCreatorT dashParserCtor =
{
    .create = __CdxDashParserOpen,
    .probe     = __CdxDashParserProbe
};

