/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMovParserImpl.c
 * Description :
 * History :
 *
 */

#include <stdint.h>
#include "CdxMovParser.h"
#include "CdxMovAtom.h"
#include "CdxMovSample.h"
#include <zconf.h>
#include <zlib.h>
#include <errno.h>

static MovTopFunc s_mov_atom_parser = NULL;

int CdxMovAtomCreate(MovTopFunc MovAtomFunc)
{
    if(MovAtomFunc != NULL)
    {
        s_mov_atom_parser = MovAtomFunc;
        return 0;
    }
    else
    {
        return -1;
    }
}

CDX_S32 CdxMovOpen(struct CdxMovParser *p, CdxStreamT *stream)
{
    CDX_S32         result;
    MOVContext      *c;
    if(!p)
    {
        return -1;
    }

    c = (MOVContext*)p->privData;
    c->fp = stream;
    if(!c->fp)
    {
        return -1;
    }

    if(p->bSmsSegment) //* for sms, has no moov
    {
        MOVStreamContext *st;
        st = AvNewStream(c, c->nb_streams);
        if(!st)
        {
            CDX_LOGE("new stream failed.");
            return -1;
        }
        st->stsd_type = 1; //* 1:v 2:a lin
        st->stream_index = 0;
        st->codec.codecType = CODEC_TYPE_VIDEO;

        c->nb_streams++;
        c->streams[c->nb_streams-1] = st;

        c->is_fragment = 1;
        c->has_video = 1;
        c->video_stream_num = 1;
        c->video_stream_idx = 0;

        c->streams[0]->time_scale = 10000000; //* sms     / scale
        return 0;
    }

    if(s_mov_atom_parser != NULL)
    {
        result = s_mov_atom_parser(c);
    }
    else
    {
        CDX_LOGE("cdx mov atom create error!\n");
        return -1;
    }
    if(result != 0)
    {
        CDX_LOGE("Parser atom of mov error!\n");
        return -1;
    }

    if(c->video_stream_num > VIDEO_STREAM_LIMIT)
    {
        CDX_LOGW("---  video stream number <%d>, only support %d video stream",
            c->video_stream_num, VIDEO_STREAM_LIMIT);
    }

    if(!c->has_audio && !c->has_video)
    {
        CDX_LOGW("Neither audio nor video is recognized!\n");
        return -1;
    }

    return 0;
}

CDX_S16 CdxMovClose(struct CdxMovParser *p)
{
    MOVContext  *c;
    CDX_S32       i;

    if(!p)
    {
        return -1;
    }

    c = (MOVContext*)p->privData;

    if(c)
    {
        if(c->sidx_buffer)
        {
            free(c->sidx_buffer);
            c->sidx_buffer = NULL;
        }
        if(c->moov_buffer)
        {
            free(c->moov_buffer);
            c->moov_buffer = NULL;
        }
        if(c->trex_data)
        {
            free(c->trex_data);
            c->trex_data = NULL;
        }
        if (c->senc_data)
        {
            free(c->senc_data);
            c->senc_data = NULL;
        }
        if(c->Vsamples)
        {
            //Sample* sample = aw_list_last(c->samples);
            unsigned int i;
            Sample* samp;
            for(i=0; i<aw_list_count(c->Vsamples); i++)
            {
                samp = aw_list_get(c->Vsamples, i);
                if(samp)
                {
                    free(samp);
                    samp = NULL;
                }
            }
            aw_list_del(c->Vsamples);
            c->Vsamples = NULL;
        }
        if(c->Asamples)
        {
            //Sample* sample = aw_list_last(c->samples);
            unsigned int i;
            Sample* samp;
            for(i=0; i<aw_list_count(c->Asamples); i++)
            {
                samp = aw_list_get(c->Asamples, i);
                if(samp)
                {
                    free(samp);
                    samp = NULL;
                }
            }
            aw_list_del(c->Asamples);
            c->Asamples = NULL;
        }

        for(i=0; i<c->nb_streams; i++)
        {
            if (c->streams[i])
            {
                if(c->streams[i]->codec.extradata)
                {
                    free(c->streams[i]->codec.extradata);
                    c->streams[i]->codec.extradata = 0;
                }
                if(c->streams[i]->ctts_data)
                {
                    free(c->streams[i]->ctts_data);
                    c->streams[i]->ctts_data = NULL;
                }
                if(c->streams[i]->rap_seek)
                {
                    free(c->streams[i]->rap_seek);
                    c->streams[i]->rap_seek = NULL;
                }
                free(c->streams[i]);
                c->streams[i] = 0;
            }
        }

        if(c->pAvccHdrInfo)
        {
            free(c->pAvccHdrInfo);
            c->pAvccHdrInfo = 0;
        }

        CDX_LOGV("mov close stream = %p", c->fp);
        if(c->fp)
        {
            CdxStreamClose(c->fp);
            c->fp = NULL;
        }
    }

    return 0;
}

CDX_S16 CdxMovExit(struct CdxMovParser *p)
{
    if(!p)
    {
        return -1;
    }

    if(p->vc)
    {
        free(p->vc);
        p->vc = NULL;
    }

    if (p->privData)
    {
        free(p->privData);
        p->privData = NULL;
    }

    free(p);

    return 0;
}

/**************************************************
    MOV_reader_get_data_block()
    Functionality : Get the next data block
    Return value  :
        1   read finish
         0  allright
        -1  read error
 **************************************************/
CDX_S16 CdxMovRead(struct CdxMovParser *p)
{
    MOVContext      *c;
    CDX_S16           ret = 0;

    if(!p)
    {
        CDX_LOGW("mov reader handle is invalid!\n");
        return -1;
    }
    c = (MOVContext*)p->privData;

    if(c->is_fragment)
    {
        //CDX_LOGD("xxx MovReadSampleFragment");
        ret = MovReadSampleFragment(p);
    }
    else
    {
        ret = MovReadSample(p);
    }

    p->nFRPicCnt = 0;
    return ret;
}

CDX_S32 CdxMovSeek(struct CdxMovParser *p, cdx_int64  timeUs, SeekModeType seekModeType)
{
    MOVContext      *c = (MOVContext*)p->privData;
    MOVStreamContext       *st = NULL;
    CDX_S32           result;

    int               seekTime;
    seekTime = timeUs / 1000;
    CDX_LOGD("=============mov seek to: %d ms, totaltime = %d ms\n",seekTime, p->totalTime);

    if((CDX_U32)seekTime > p->totalTime)
    {
        CDX_LOGW("The seek time is larger than total time!\n");
        return 1;
    }
    if(seekTime < 0)
    {
        CDX_LOGW("The parameter for jump play is invalid!\n");
        return -1;
    }

    if(!c->bSeekAble)
    {
        CDX_LOGD("-- can not seekable");
        return -1;
    }

    int i;
    for(i=0; i<c->has_subtitle; i++)
    {
        st = c->streams[c->s2st[i]];
        st->SubStreamSyncFlg = 1;
    }

    //in DASH, the totaltime maybe 0, but it seekable  ( BigBuckBunny audio segment )
    if(c->is_fragment)
    {
        result = MovSeekSampleFragment(p,c,timeUs);
        if(result < 0)
        {
            return result;
        }
    }
    else
    {
        result = MovSeekSample(p,c,seekTime,seekModeType);
        if(result < 0)
        {
            return result;
        }
    }

    for(i=0; i<c->nb_streams; i++)
    {
        if(c->streams[i]->stsd_type && !c->streams[i]->unsurpoort)
        {
            c->streams[i]->read_va_end = 0; // reset the eof flag
        }
    }

    CDX_LOGV("---- seek end");

    return 0;
}

struct CdxMovParser* CdxMovInit(CDX_S32 *ret)
{
    MOVContext           *s;
    struct CdxMovParser  *p;

    *ret = 0;
    p = (struct CdxMovParser *)malloc(sizeof(struct CdxMovParser));
    if(!p)
    {
        *ret = -1;
        return NULL;
    }
    memset(p, 0, sizeof(struct CdxMovParser));
    p->mErrno = PSR_INVALID;

    p->privData = NULL;
    s = (MOVContext *)malloc(sizeof(MOVContext));
    if(!s)
    {
        *ret = -1;
        return p;
    }
    memset(s, 0, sizeof(MOVContext));

    p->privData = (void *)s;

    s->ptr_fpsr = p;
    s->Vsamples = aw_list_new();
    s->Asamples = aw_list_new();

    p->vc = (VirCacheContext *)malloc(sizeof(VirCacheContext));
    if (p->vc == NULL)
    {
        *ret = -1;
        return p;
    }
    memset(p->vc, 0, sizeof(VirCacheContext));
    *ret = CdxMovAtomInit();

    return p;
}

//***************************************************************************/
//* initial in getmediaInfo, set the video, audio stream index
//***************************************************************************/
CDX_S32 CdxMovSetStream(struct CdxMovParser *p)
{
    MOVContext    *c;
    VirCacheContext *vc;
    //CDX_U32 fpos;

    c = (MOVContext*)p->privData;

    vc = p->vc;

    c->streams[c->video_stream_idx]->real_stss_size = c->streams[c->video_stream_idx]->stss_size;
    if(c->streams[c->video_stream_idx]->stss_size == 1)
        c->streams[c->video_stream_idx]->stss_size +=
        c->streams[c->video_stream_idx]->rap_seek_count;
    CDX_LOGV("== stss_size: %d", c->streams[c->video_stream_idx]->stss_size);

    c->basetime[0] = c->streams[c->video_stream_idx]->basetime;
    c->basetime[1] = c->streams[c->audio_stream_idx]->basetime;
    c->basetime[2] = c->streams[c->subtitle_stream_idx]->basetime;

    if(c->has_audio && c->totaltime < c->streams[c->audio_stream_idx]->totaltime)
        c->totaltime = c->streams[c->audio_stream_idx]->totaltime;
    //p->total_time = c->totaltime * 1000;
    p->totalTime = c->totaltime;   //fuqiang

    if(c->mvhd_total_time > p->totalTime)
    {
        p->totalTime = c->mvhd_total_time;
    }
    if(c->sidx_total_time > p->totalTime)
    {
        p->totalTime = c->sidx_total_time;
    }
    CDX_LOGV("mvhd = %d, ", c->mvhd_total_time);

    if(c->streams[c->video_stream_idx]->stss_size < 2)
    {
        //not support ff or rev
        p->hasIdx = 0;
    }
    else
    {
        p->hasIdx = 1;
    }

    if(c->sidx_flag)
    {
        p->hasIdx = 1;
    }

    return 0;
}

