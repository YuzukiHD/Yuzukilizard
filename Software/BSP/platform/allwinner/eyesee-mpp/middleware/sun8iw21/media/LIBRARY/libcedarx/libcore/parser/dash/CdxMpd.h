/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMpd.h
 * Description : Mpd
 * History :
 *
 */

#ifndef CDX_MPD_H
#define CDX_MPD_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "CdxMovList.h"

struct mpdtm {
    int tm_sec;     /* seconds after the minute - [0,59] */
    int tm_min;     /* minutes after the hour - [0,59] */
    int tm_hour;    /* hours since midnight - [0,23] */
    int tm_mday;    /* day of the month - [1,31] */
    int tm_mon;     /* months since January - [0,11] */
    int tm_year;    /* years since 1900 */
    int tm_wday;    /* days since Sunday - [0,6] */
    int tm_yday;    /* days since January 1 - [0,365] */
    int tm_isdst;    /* daylight savings time flag */
};

typedef struct _awProgramInformation awProgramInformation;
struct _awProgramInformation {
    char* lang;
    char* moreInformationURL;
    char* title;
    char* source;
    char* copyright;
};

typedef enum {
    AW_MPD_TYPE_STATIC,
    AW_MPD_TYPE_DYNAMIC,
} AW_MPD_Type;

typedef struct _awMpd awMpd;
struct _awMpd {
    char*             id;  //identifier for the Media Presentation ( optional )
    char*             profiles; // Media Presentation profiles ( MUST )

    AW_MPD_Type     type;  //MPD may be updated("dynamic": 1 ) or not ( "static" : 0 )
    int64_t         availabilityStartTime;   //if dynamic, mustbe present
    int64_t         availabilityEndTime;

    int64_t         mediaPresentationDuration; //mustbe present if "static"
    int64_t         minimumUpdatePeriod;
    int64_t         minBufferTime;
    int64_t         timeShiftBufferDepth;
    int64_t         suggestedPresentationDelay;
    int64_t         maxSegmentDuration;
    int64_t         maxSubsegmentDuration;

    AW_List*        programInfos;
    AW_List*            baseURLs;
    AW_List*            locations;
    AW_List*            metrics;
    AW_List*            periods;
};

typedef struct _awMpdByteRange awMpdByteRange;
struct _awMpdByteRange {
    long long int start_range;
    long long int end_range;
};

typedef struct {
    char             *media;
    awMpdByteRange  *media_range;
    char             *index;
    awMpdByteRange  *index_range;
    int64_t         duration;
} awSegmentURL;

typedef struct {
    unsigned char         *URL;
    char                 *serviceLocation;
    awMpdByteRange      *byteRange;
} awBaseURL;

typedef struct _awMpdURL awMpdURL;
struct _awMpdURL {
    char*                sourceURL;
    awMpdByteRange*        byteRange;
};

typedef struct {
    long long int     t;  //start time
    unsigned int     d;  //duration
    unsigned int     r;  //repeat count
}awSegmentTimelineEntry;

typedef struct {
    AW_List *entries;
} awSegmentTimeline;

#define AW_MPD_SEGMENT_BASE \
    unsigned int        timescale;                \
    int64_t             presentationTimeOffset;   \
    awMpdByteRange*         indexRange;               \
    int                 indexRangeExact;          \
    awMpdURL*            initializationSegment;    \
    awMpdURL*            representationIndex;      \
    unsigned int        time_shift_buffer_depth;   \
    double                availability_time_offset;  \

// segment Base Information
typedef struct _awSegmentBase awSegmentBase;
struct _awSegmentBase
{
    AW_MPD_SEGMENT_BASE
};

/*WARNING: duration is expressed in GF_MPD_SEGMENT_BASE@timescale unit*/
#define AW_MPD_MULTIPLE_SEGMENT_BASE            \
    AW_MPD_SEGMENT_BASE                            \
    long long int                 duration;            \
    unsigned int             startNumber;        \
    awSegmentTimeline      *segmentTimeline;        \
    awMpdURL                 *bitstreamSwitching_URL;\

typedef struct {
    AW_MPD_MULTIPLE_SEGMENT_BASE
} awMultipleSegmentBase;

typedef struct _awSegmentList awSegmentList;
struct _awSegmentList {
    AW_MPD_MULTIPLE_SEGMENT_BASE
    AW_List*            segmentURL;
    char*                 xlinkHref;
    int                    xlinkActuate;
};

typedef struct _awSegmentTemplate awSegmentTemplate;
struct _awSegmentTemplate {
    AW_MPD_MULTIPLE_SEGMENT_BASE
    char*            media;
    char*            index;
    char*            initialization;
    char*            bitstreamSwitch; // not equal to the  bitstreamSwitching in
                                      // AW_MPD_MULTIPLE_SEGMENT_BASE
};

typedef struct {
    int num, den;
} MPD_Fractional;

typedef enum {
    AW_MPD_SCANTYPE_UNKNWON,
    AW_MPD_SCANTYPE_PROGRESSIVE,
    AW_MPD_SCANTYPE_INTERLACED
} MPD_ScanType;

#define AW_MPD_COMMON_ATTRIBUTES_ELEMENTS    \
    char *profiles;    \
    unsigned int width;    \
    unsigned int height;    \
    MPD_Fractional* sar;    \
    MPD_Fractional* framerate;    \
    unsigned int samplerate;    \
    char *   mime_type;    \
    char *  segmentProfiles;    \
    char *    codecs;    \
    unsigned int maximum_sap_period;    \
    int starts_with_sap;    \
    double max_playout_rate;    \
    int coding_dependency;    \
    MPD_ScanType scan_type;    \
    AW_List*  frame_packing;    \
    AW_List*  audio_channels;    \
    AW_List*  content_protection;    \

typedef struct {
    AW_MPD_COMMON_ATTRIBUTES_ELEMENTS
} MPD_CommonAttributes;

typedef struct _awAdaptionSet awAdaptionSet;
typedef struct _awPeriod awPeriod;
struct _awPeriod {
    char*                     xlinkHref;
    int                     xlinkActuate;  //onLoad or onRequest

    char*                     id;

    unsigned int             start;      //start time of each Media Segment
    unsigned int             duration;   //duration of the Period to
                                         //determine the start time of next Period
    int                        isBitstreamSwitching;

    awSegmentBase*            segmentBase;
    awSegmentList*            segmentList;
    awSegmentTemplate*        segmentTemplate;
    AW_List*                 adaptationSets;
    AW_List*                baseURL;
    AW_List*                subsets;
};

struct _awAdaptionSet {
    AW_MPD_COMMON_ATTRIBUTES_ELEMENTS

    char*                    xlinkHref;
    int                        xlinkActuate;
    unsigned int            id;
    unsigned int            group;
    char*                   lang;
    char*                   contentType;
    MPD_Fractional*         par;
    unsigned int            minBandwidth;
    unsigned int            maxBandwidth;
    unsigned int            minWidth;
    unsigned int            maxWidth;
    unsigned int            minHeight;
    unsigned int            maxHeight;
    unsigned int            minFramerate;
    unsigned int            maxFramerate;

    //bool
    int                     segmentAlignment;
    int                        bitstreamSwitching;
    int                     subsegmentAlignment;
    int                     subsegmentStartsWithSAP;

    //list
    AW_List*                accessiblity;
    AW_List*                role;
    AW_List*                rating;
    AW_List*                viewpoint;
    AW_List*                contentComponent;
    AW_List*                baseURLs;

    awSegmentBase*            segmentBase;
    awSegmentList*            segmentList;
    awSegmentTemplate*        segmentTemplate;
    AW_List*                representation;

};

typedef struct _awRepresentation awRepresentation;
struct _awRepresentation {
    AW_MPD_COMMON_ATTRIBUTES_ELEMENTS

    char*                         id; /*MANDATORY*/
    unsigned int                 bandwidth; /*MANDATORY*/
    unsigned int                qualityRanking;
    char*                         dependencyId;
    char*                         mediaStreamStructureId;

    AW_List*                     baseURLs;
    AW_List*                    subRepresentation;
    awSegmentBase*                 segmentBase;
    awSegmentList*                 segmentList;
    awSegmentTemplate*             segmentTemplate;

    /*index of the next enhancement representation plus 1,*/
    /* 0 is reserved in case of the highest representation*/
    unsigned int enhancement_rep_index_plus_one;
} ;

//int ParseMpdFile(char* filename);

awMpd* ParseMpdFile(char* filename, int fd, char* buffer,
                    int size, const char *encoding, int options);

void MpdFree(awMpd* mpd);

#endif
