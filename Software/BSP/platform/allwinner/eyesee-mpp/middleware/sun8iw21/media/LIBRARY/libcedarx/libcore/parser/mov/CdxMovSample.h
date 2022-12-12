/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMovSample.h
 * Description :
 * History
 * Date    : 2017/05/02
 * Comment : refactoring mov parser,mov about sample implement in this file:
 */

#ifndef CDX_mov_sample_H
#define CDX_mov_sample_H
#include "CdxMovParser.h"

typedef struct StscParameter
{
    CDX_U32 stsc_first;
    CDX_U32 stsc_idx;
    CDX_U32 stsc_next;
    CDX_U32 stsc_samples;
    CDX_S32 stsc_samples_acc;
    CDX_S32 samples_in_chunks;
    CDX_S32 sample_num;
}StscParameter;

typedef struct CalcSampleParameter
{
    CDX_U32 stsz_acc;
    CDX_U32 stsc_samples;
    CDX_U32 stsc_next;
    CDX_S32 stsc_first;
    CDX_S32 stts_sample_duration;
    CDX_S32 stts_sample_count;
    CDX_U64 curr_chunk_duration;
    CDX_U64 seek_dts;
    CDX_U32 seek_audio_sample;
    CDX_U32 seek_subtitle_sample;
}CalcSampleParameter;

CDX_S16 MovReadSample(struct CdxMovParser *p);
CDX_S16 MovReadSampleFragment(struct CdxMovParser *p);
CDX_S32 MovSeekSampleFragment(struct CdxMovParser *p,MOVContext *c,cdx_int64  timeUs);
CDX_S32 MovSeekSample(struct CdxMovParser *p,MOVContext *c,cdx_int64 seekTime,
	                           SeekModeType seekModeType);

#endif