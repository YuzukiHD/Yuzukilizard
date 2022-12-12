/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMovSample.c
 * Description :
 * History
 * Date    : 2017/05/02
 * Comment : refactoring mov parser,mov about sample implement in this file:
 */
#include "CdxMovSample.h"
#include "CdxMovAtom.h"

extern  MovTopParserProcess g_mov_top_parser[];

static CDX_U32 readStsc(MOVContext *c, MOVStreamContext *st, cdx_uint32 idx)
//bytes of offset from STSC start
{
    CDX_U32 Cache_Data;
    unsigned char* buffer = c->moov_buffer;
    unsigned int offset = 0;
#if 0
    if(idx >= st->stsc_size*12)
    {
        CDX_LOGW("index<%d> is large than stsc_size<%d>, maybe error", idx/12, st->stsc_size);
        //return INT_MAX;
    }
#endif
    offset = st->stsc_offset + idx;
    if(offset > c->moov_size)
        return INT_MAX;
    Cache_Data = MoovGetBe32(buffer+offset);

    return    Cache_Data;
}

static CDX_U32 readStsz(MOVContext *c, MOVStreamContext *st, cdx_uint32 idx)
//bytes of offset from STSZ start
{
    CDX_U32 Cache_Data;
    unsigned char* buffer = c->moov_buffer;
    unsigned int offset = 0;

    #if 0
    if(idx >= st->stsz_size*4)
    {
        CDX_LOGW("index<%d> is large than stsz_size<%d>, maybe error", idx, st->stsz_size);
        //return INT_MAX;
    }
    #endif

    if(st->sample_size)
        return st->sample_size;

    offset = st->stsz_offset + idx;
    if(offset > c->moov_size)
        return INT_MAX;
    Cache_Data = MoovGetBe32(buffer+offset);

    return    Cache_Data;

}

CDX_U32 ReadStss(MOVContext *c, MOVStreamContext *st, cdx_uint32 idx)
//bytes of offset from STSS start
{
    CDX_U32 Cache_Data = 0;
    unsigned char* buffer = c->moov_buffer;
    unsigned int offset = 0;

    #if 0
    if(idx >= st->stss_size*4)
    {
        CDX_LOGW("index<%d> is large than stss_size<%d>, maybe error", idx/4, st->stss_size);
        //return INT_MAX;
    }
    #endif

    if(st->real_stss_size > 1 || st->rap_seek_count == 0)
    {
        offset = st->stss_offset + idx;
        if(offset > c->moov_size)
            return INT_MAX;
        Cache_Data = MoovGetBe32(buffer+offset);
    }
    else if(st->real_stss_size == 1)
    {
        if(st->rap_seek_count > 0)
        {
            if(idx == 0)
            {
                Cache_Data = 1;
            }
            else
            {
                Cache_Data = st->rap_seek[(idx>>2)-1];
            }
        }
        else
        {
            Cache_Data = 1;
        }
    }

    return Cache_Data;
}

static CDX_U32 readStts(MOVContext *c, MOVStreamContext *st, cdx_uint32 idx)
{
    CDX_U32 Cache_Data;
    unsigned char* buffer = c->moov_buffer;
    unsigned int offset = 0;

    #if 0
    if(idx >= st->stts_size*8)
    {
        CDX_LOGW("index<%d> is large than stts_size<%d>, maybe error", idx, st->stts_size);
        return INT_MAX;
    }
    #endif

    offset = st->stts_offset + idx;
    if(offset > c->moov_size)
        return INT_MAX;
    Cache_Data = MoovGetBe32(buffer+offset);

    return Cache_Data;
}

static CDX_U64 readStco(MOVContext *c, MOVStreamContext *st, cdx_uint32 idx)
{
    CDX_U64 Cache_Data;
    unsigned char* buffer = c->moov_buffer;
    CDX_U64 offset = 0;

    #if 0
    if(idx >= st->stco_size)
    {
        CDX_LOGW("index<%d> is large than stco_size<%d>, maybe error", idx, st->stco_size);
        //return 0;
    }
    #endif

    if(!st->co64)
    {
        idx <<= 2;
        offset = st->stco_offset + idx;
        if(offset > c->moov_size)
            return INT_MAX;
        Cache_Data = MoovGetBe32(buffer+offset);
    }
    else
    {
        idx <<= 3;
        offset = st->stco_offset + idx;
        if(offset > c->moov_size)
            return INT_MAX;
        Cache_Data = MoovGetBe64(buffer+offset);
    }

    return    Cache_Data;
}

static int getMin(CDX_U64* offset, int idx)
{
    int i;
    int ret = 0;

    for(i=1; i<idx; i++)
    {
        if(offset[i] < offset[ret])
            ret = i;
    }

    return ret;
}

static CDX_S32 locatingStts(struct CdxMovParser *p,MOVStreamContext* st)
{
    CDX_S32 stts_sample_count;
    CDX_S32 stts_sample_duration;
    CDX_S32 idx;

    MOVContext      *c;
    c = (MOVContext*)p->privData;

    if(st->mov_idx_curr.sample_idx >= st->mov_idx_curr.stts_samples_acc)
    {
         while(1)
         {
             if(p->exitFlag)
             {
                 return -1;
             }
             idx = st->mov_idx_curr.stts_idx<<3;

             //sample number of the same duration
             stts_sample_count    = readStts(c, st, idx);
             //duration
             stts_sample_duration = readStts(c, st, idx+4);

             st->mov_idx_curr.stts_samples_acc += stts_sample_count;
             st->mov_idx_curr.stts_duration_acc += (CDX_U64)stts_sample_count *
                        stts_sample_duration;
             st->mov_idx_curr.stts_sample_duration = stts_sample_duration;
             st->mov_idx_curr.stts_idx++;

             //find the sample presentation time
             if(st->mov_idx_curr.sample_idx < st->mov_idx_curr.stts_samples_acc)
             {
                 break;
             }
         }
    }

    return 0;
}

static CDX_S32 locatingStsc(StscParameter *stsc_parameter,struct CdxMovParser *p,
                   MOVStreamContext* st)
{
    CDX_S32       idx;
    MOVContext    *c;
    c = (MOVContext*)p->privData;

    if(stsc_parameter->sample_num > stsc_parameter->stsc_samples_acc-1 &&
        stsc_parameter->stsc_idx <= st->stsc_size)
    {
        while(1)
        {
            if(p->exitFlag)
            {
                return -1;
            }
            idx = stsc_parameter->stsc_idx * 12;

            //read the first chunk id in the stsc
            stsc_parameter->stsc_first = readStsc(c, st, idx);
            stsc_parameter->stsc_samples = readStsc(c, st, idx+4);  //sample number in this chunk

            //if the chunk id is not exist( larger than the stsc size )
            if(st->mov_idx_curr.stsc_idx + 1 >= st->stsc_size)
            {
                stsc_parameter->stsc_next = 30*3600*24;
                stsc_parameter->samples_in_chunks = 0x7ffffff;//not int max
            }
            else
            {
                stsc_parameter->stsc_next    = readStsc(c, st, idx+12); //the next chunk id
                stsc_parameter->samples_in_chunks =
                    (stsc_parameter->stsc_next - stsc_parameter->stsc_first)*
                    stsc_parameter->stsc_samples;
                //the sample number between the two chunks
            }

            //accumulate the samples in the same chunk
            stsc_parameter->stsc_samples_acc += stsc_parameter->samples_in_chunks;

            stsc_parameter->stsc_idx++;
            st->mov_idx_curr.stsc_idx++;//has read a stsc, switch to normal play, no need to inc it
            //if find the chunk, break
            if(stsc_parameter->sample_num <= stsc_parameter->stsc_samples_acc-1 ||
                st->mov_idx_curr.stsc_idx > st->stsc_size)
                break;
            stsc_parameter->stsc_first = stsc_parameter->stsc_next;
        }
    }

    return 0;
}

/*
*  fragment MP4
*  find 'moof' and get the sample from it ,then store it to a list
*   @ offset: the offset in the file, used to seek from current
*   @ moov: if DASH change bandwidth, the new init segment will contain a 'moov' atom
*/
static CDX_S32 movReadSampleList(struct CdxMovParser *p)
{
    int ret = 0;
    MOVContext*   s = (MOVContext*)p->privData;
    CdxStreamT    *pb = s->fp;
    CDX_U64 offset = 0;
    unsigned char buf[8] = {0};
    MOV_atom_t    a;

    if(s->moof_end_offset)
    {
        offset = s->moof_end_offset;
        CdxStreamSeek(pb, s->moof_end_offset, SEEK_SET);
    }
    else
    {
        //the first moof will goes here
        offset = CdxStreamTell(pb);
        //CDX_LOGD("xxxxx offset=%llu", offset);
        CdxStreamSeek(pb, s->first_moof_offset, SEEK_SET);
        CDX_LOGV("xxxxx first moof offset=%llx", s->first_moof_offset);
    }

    // find the moof , if no, the file is end
    while(ret > -1)
    {
        a.offset = CdxStreamTell(pb);
        ret = CdxStreamRead(pb, buf, 8);
        if(ret < 8)
        {
            CDX_LOGD("end of file? reslut(%d)", ret);
            break;
        }

        a.size = MoovGetBe32(buf);   //big endium
        a.type = MoovGetLe32(buf+4);

        // read end
        if((a.size == 0) || (a.type == 0))
        {
            break;
        }

        if(a.type == MKTAG( 'm', 'o', 'o', 'f' ))
        {
            s->fragment.moof_offset = a.offset;
            ret = g_mov_top_parser[MESSAGE_ID_MOOF].movParserFunc(s,buf,a);
        }
        else if (a.type == MKTAG( 'm', 'd', 'a', 't' ))
        {
            //offset and duration of the chunk in segment
            s->moof_end_offset = a.offset+a.size;
            s->nSmsMdatOffset = CdxStreamTell(pb); //* for sms
            CDX_LOGV("xxx s->nSmsMdatOffset=%llx", s->nSmsMdatOffset);
            return 0;
        }
        else if (a.type == MKTAG( 's', 'i', 'd', 'x' ))
        {
            //offset and duration of the chunk in segment
            //if get a sidx the first_moof_offset should set to 0
            s->sidx_flag = 1;
            ret = g_mov_top_parser[MESSAGE_ID_SIDX].movParserFunc(s,buf,a);
        }
        else if (a.type == MKTAG( 's', 's', 'i', 'x' ))
        {
            //offset and duration of the chunk in segment
            //MovParseSidx(s, pb, a);
            ret = CdxStreamSeek(pb, a.size - 8,SEEK_CUR);
        }
        else if (a.type == MKTAG( 'm', 'o', 'o', 'v' ))
        {
            //offset and duration of the chunk in segment
            CDX_LOGV("-- sample list -- moov");
            //MovParseSidx(s, pb, a);
            ret = CdxStreamSeek(pb, a.size - 8,SEEK_CUR);
        }
        else
        {
            ret = CdxStreamSeek(pb, a.size - 8,SEEK_CUR);
        }
    }

    //end of stream
    return 1;
}

//only support video track time to sample
static CDX_S32 movTimeToSample(struct CdxMovParser *p, cdx_int64 seconds)
{
    MOVContext      *c;
    //VirCacheContext *vc = p->vc;
    CDX_U64   duration_seek;
    CDX_U64   duration_acc=0;
    CDX_S32   sample_count,sample_count_acc=0,sample_duration;
    CDX_S32   sample_idx;
    CDX_S32   time_scale;
    CDX_S32   stream_idx;
    CDX_S32   idx;
    CDX_U32   stts_idx;

    c = (MOVContext*)p->privData;

    stream_idx = c->video_stream_idx;
    time_scale = c->streams[stream_idx]->time_scale;
    duration_seek = (CDX_U64)time_scale * seconds/1000;

    //look up from video stts table
    stts_idx = 0;

    while(1)
    { //modify 20090327 night
        if(p->exitFlag)
        {
            return -1;
        }

        idx = stts_idx << 3;

        sample_count    = readStts(c, c->streams[stream_idx], idx);
        sample_duration = readStts(c, c->streams[stream_idx], idx+4);

        duration_acc += (CDX_U64)sample_count * sample_duration;
        sample_count_acc += sample_count;
        if(duration_seek < duration_acc || stts_idx > c->streams[stream_idx]->stts_size)
            break;
        stts_idx++;
    }

    if(sample_duration != 0)
    {
        sample_idx = sample_count_acc -
            (duration_acc - duration_seek) / sample_duration;
    }
    else
    {
        CDX_LOGI("Be careful: sample_duration is zero, set sample_idx to zero!");
        sample_idx = 0;
    }

    return sample_idx;
}

static CDX_S32 movTimeToSampleSidx(struct CdxMovParser *p, int seconds, int stream_index,
                               int type)
{
    MOVContext      *s;
    s = (MOVContext*)p->privData;
    CdxStreamT    *pb = s->fp;
    int sidx_idx = 0;
    int ret = 0;
    CDX_UNUSE(type);

    MOVSidx* sidx = s->sidx_buffer;
    int i = 0;

    CDX_U32 time_scale = s->streams[stream_index]->time_scale;
    CDX_U64 time = (CDX_U64)seconds * time_scale / 1000;

    if(s->bSmsSegment) //* for sms
    {
        //CDX_LOGD("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
        s->streams[stream_index]->mov_idx_curr.current_dts = time;
        return 0;
    }

    if(!sidx)
    {
        CDX_LOGW("--- sidx is NULL, cannot seek");
        return 0;
    }
    for(i=0; i<s->sidx_count; i++)
    {
        if(sidx[i].current_dts <= time)
        {
            sidx_idx = i;
        }
        else
        {
            break;
        }
    }
    s->streams[stream_index]->mov_idx_curr.current_dts = sidx[sidx_idx].current_dts;
    //reset the audio pts

    CDX_U64 offset = sidx[sidx_idx].offset;
    //CdxStreamSeek(pb, s->first_moof_offset+offset, SEEK_SET);
    ret = CdxStreamSeek(pb, offset, SEEK_SET);
    if(ret < 0)
    {
        return -1;
    }

    return 0;
}

CDX_U64 calcCurrentTime(struct CdxMovParser *p,MOVStreamContext* st,CDX_U32 curr_sample_idx)
{
    CDX_U64       current_time;
    CDX_S32       stts_sample_count;
    CDX_S32       stts_sample_duration;
    CDX_S32       idx;

    st->mov_idx_curr.stts_idx = 0;
    st->mov_idx_curr.stts_samples_acc = 0;
    st->mov_idx_curr.stts_duration_acc = 0;
    MOVContext    *c;
    c = (MOVContext*)p->privData;

    if(curr_sample_idx >= st->mov_idx_curr.stts_samples_acc)
    {
        while(1)
        {
            if(p->exitFlag)
            {
                return -1;
            }
            idx = st->mov_idx_curr.stts_idx<<3;

            //sample number of the same duration
            stts_sample_count	  = readStts(c, st, idx);
            //duration
            stts_sample_duration = readStts(c, st, idx+4);

            st->mov_idx_curr.stts_samples_acc += stts_sample_count;
            st->mov_idx_curr.stts_duration_acc += (CDX_U64)stts_sample_count *
                    stts_sample_duration;
            st->mov_idx_curr.stts_sample_duration = stts_sample_duration;
            st->mov_idx_curr.stts_idx++;

            //find the sample presentation time
            if(curr_sample_idx < st->mov_idx_curr.stts_samples_acc)
            {
                break;
            }
        }
    }

    st->mov_idx_curr.current_dts =
        st->mov_idx_curr.stts_duration_acc -
        (CDX_U64)st->mov_idx_curr.stts_sample_duration *
        (st->mov_idx_curr.stts_samples_acc - curr_sample_idx);

    current_time = st->mov_idx_curr.current_dts * 1000 / st->time_scale;

    return current_time;
}

/************************************************************************/
/*  Find from current pos's nearest keyframe index                      */
/************************************************************************/
static CDX_S32 movFindKeyframe(struct CdxMovParser *p, CDX_U32 curr_sample_num,
                                SeekModeType seekModeType, CDX_S32 seekTime)
{
    MOVContext    *c;
    //VirCacheContext *vc = p->vc;
    CDX_U32   stss_size;
    CDX_S32   curr_stss_idx;
    CDX_S32   previous_keyframe_num,next_keyframe_num;
    CDX_U32   keyframe_num;
    CDX_S32   low,high,i,nexti;
    CDX_U64   previous_sync_time,next_sync_time;
    CDX_U64   diff_time_previous,diff_time_next;
    MOVStreamContext    *st;
    SeekModeType curSeekMode;

    c = (MOVContext*)p->privData;
    st = c->streams[c->video_stream_idx];
    //seek to stss atom
    stss_size = st->stss_size;
    curSeekMode = AW_SEEK_CLOSEST_SYNC;

    //protect stss_idx region
    if(st->mov_idx_curr.stss_idx >= (int)stss_size)
        st->mov_idx_curr.stss_idx = stss_size-1;
    if(st->mov_idx_curr.stss_idx < 0)
        st->mov_idx_curr.stss_idx = 0;

    if(stss_size < 1)
    {
        return -1;
    }

    int FrameInterval;
    st->mov_idx_curr.stss_idx  = 0;

    keyframe_num = ReadStss(c, st, st->mov_idx_curr.stss_idx<<2);
    if((curr_sample_num == 0) || (st->mov_idx_curr.stss_idx == (int)st->stss_size))
    {
        return (keyframe_num - 1);
    }

    FrameInterval = (keyframe_num - curr_sample_num);//absolution
    if(FrameInterval < 2048 && FrameInterval > -2048)
    {
        int cmpflag = 0;

        if(keyframe_num < curr_sample_num)
        {
            cmpflag = 1;
        }
        else if(keyframe_num == curr_sample_num)
        {
            curSeekMode = AW_SEEK_CLOSEST_SYNC;
            goto key_frame_process;
        }
        else
        {
            cmpflag = 2;
        }

        while((-1 < st->mov_idx_curr.stss_idx) &&
            (st->mov_idx_curr.stss_idx < (int)stss_size))
        {
            if(p->exitFlag)
            {
                return -1;
            }

            keyframe_num = ReadStss(c, st, st->mov_idx_curr.stss_idx<<2);

            //CDX_LOGD("2.keyframe_num=%d, curr_sample_num = %d",  keyframe_num, curr_sample_num);
            if(keyframe_num < curr_sample_num)
            {
                if(cmpflag == 2)
                {
                    curSeekMode = AW_SEEK_PREVIOUS_SYNC;
                    break;
                }
                st->mov_idx_curr.stss_idx++;
            }
            else
            {
                if(keyframe_num == curr_sample_num)
                {
                    curSeekMode = AW_SEEK_CLOSEST_SYNC;
                    break;
                }
                else
                {
                    if(cmpflag == 1)
                    {
                        curSeekMode = AW_SEEK_NEXT_SYNC;
                        break;
                    }
                    st->mov_idx_curr.stss_idx--;
                }
            }
        }
        if(st->mov_idx_curr.stss_idx < 0)
        {
            st->mov_idx_curr.stss_idx = 0;
        }
    }
    else
    {
        low = 0;
        high = stss_size-1;
        nexti = (low+high)>>1;
        while (low+1 < high)
        {
            if(p->exitFlag)
            {
                return -1;
            }

            i = nexti;

            keyframe_num = ReadStss(c, st, nexti<<2);

            if(keyframe_num < curr_sample_num)
            {
                low = i;
                nexti = (low+high)>>1;
            }
            else
            {
                high = i;
                nexti = (low+high)>>1;
            }
        }

        st->mov_idx_curr.stss_idx = high;
        keyframe_num = ReadStss(c, st, st->mov_idx_curr.stss_idx<<2);
        if(keyframe_num > curr_sample_num)
        {
            curSeekMode = AW_SEEK_NEXT_SYNC;
        }
        else if(keyframe_num == curr_sample_num)
        {
            curSeekMode = AW_SEEK_CLOSEST_SYNC;
        }
        else
        {
            curSeekMode = AW_SEEK_PREVIOUS_SYNC;
        }
    }

key_frame_process:
    if(seekModeType == AW_SEEK_PREVIOUS_SYNC)
    {
        if(curSeekMode != AW_SEEK_PREVIOUS_SYNC)
        {
            if(curSeekMode == AW_SEEK_CLOSEST_SYNC ||
                curSeekMode == AW_SEEK_NEXT_SYNC)
            {
                st->mov_idx_curr.stss_idx--;
                if(st->mov_idx_curr.stss_idx < 0)
                {
                    st->mov_idx_curr.stss_idx = 0;
                }
                keyframe_num = ReadStss(c, st, st->mov_idx_curr.stss_idx<<2);
            }
        }
    }
    else if(seekModeType == AW_SEEK_NEXT_SYNC)
    {
        if(curSeekMode != AW_SEEK_NEXT_SYNC)
        {
            if(curSeekMode == AW_SEEK_CLOSEST_SYNC ||
                curSeekMode == AW_SEEK_PREVIOUS_SYNC)
            {
                st->mov_idx_curr.stss_idx++;
                if(st->mov_idx_curr.stss_idx >= (int)stss_size)
                {
                    st->mov_idx_curr.stss_idx = stss_size-1;
                }
                keyframe_num = ReadStss(c, st, st->mov_idx_curr.stss_idx<<2);
            }
        }
    }
    else if(seekModeType == AW_SEEK_CLOSEST_SYNC)
    {
        if(curSeekMode == AW_SEEK_PREVIOUS_SYNC)
        {
            curr_stss_idx = st->mov_idx_curr.stss_idx;
            previous_keyframe_num = ReadStss(c, st, curr_stss_idx<<2);
            previous_sync_time = calcCurrentTime(p,st,previous_keyframe_num);
            curr_stss_idx = curr_stss_idx + 1;
            if(curr_stss_idx >= (int)stss_size)
            {
                curr_stss_idx = stss_size-1;
            }
            next_keyframe_num = ReadStss(c, st, curr_stss_idx<<2);
            next_sync_time = calcCurrentTime(p,st,next_keyframe_num);
            diff_time_previous = seekTime - previous_sync_time;
            diff_time_next = next_sync_time - seekTime;
            if(diff_time_next < diff_time_previous)
            {
                keyframe_num = next_keyframe_num;
            }
        }
        else if(curSeekMode == AW_SEEK_NEXT_SYNC)
        {
            curr_stss_idx = st->mov_idx_curr.stss_idx;
            next_keyframe_num = ReadStss(c, st, curr_stss_idx<<2);
            next_sync_time = calcCurrentTime(p,st,next_keyframe_num);
            curr_stss_idx = curr_stss_idx - 1;
            if(curr_stss_idx < 0)
            {
                curr_stss_idx = 0;
            }
            previous_keyframe_num = ReadStss(c, st, curr_stss_idx<<2);
            previous_sync_time = calcCurrentTime(p,st,previous_keyframe_num);
            diff_time_previous = seekTime - previous_sync_time;
            diff_time_next = next_sync_time - seekTime;
            if(diff_time_previous < diff_time_next)
            {
                keyframe_num = previous_keyframe_num;
            }
        }
    }
    else if(seekModeType == AW_SEEK_THUMBNAIL || seekModeType == AW_SEEK_CLOSEST)
    {
        st->mov_idx_curr.stss_idx--;
        if(st->mov_idx_curr.stss_idx < 0)
        {
            st->mov_idx_curr.stss_idx = 0;
        }
        keyframe_num = ReadStss(c, st, st->mov_idx_curr.stss_idx<<2);
    }

    return (keyframe_num-1);
}

static CDX_S32 seekVideoSample(struct CdxMovParser *p,MOVContext *c,
                         MOVStreamContext *st,CDX_U32 sample_num)
{
    StscParameter stsc_parameter;
    CDX_S32       ret;
    CDX_S32       i;
    CDX_S32       sample_in_chunk_ost;
    st->mov_idx_curr.stsc_idx = 0;
    memset(&stsc_parameter,0,sizeof(StscParameter));
    stsc_parameter.sample_num = sample_num;

    ret = locatingStsc(&stsc_parameter,p,st);
    if(ret < 0)
    {
        return ret;
    }

    st->mov_idx_curr.stsc_samples = stsc_parameter.stsc_samples;
    st->mov_idx_curr.stsc_end = stsc_parameter.stsc_next - 1;

    //sample_in_chunk_ost : 0..x
    //below 2 lines :: -1  first: for round , second: for stsc_first start with 1
    sample_in_chunk_ost = stsc_parameter.sample_num -
    (stsc_parameter.stsc_samples_acc - stsc_parameter.samples_in_chunks);
    if(stsc_parameter.stsc_samples==1)
        st->mov_idx_curr.stco_idx = stsc_parameter.stsc_first +
        sample_in_chunk_ost - 1;
    else
        st->mov_idx_curr.stco_idx = stsc_parameter.stsc_first +
        (sample_in_chunk_ost / stsc_parameter.stsc_samples) - 1;
    st->mov_idx_curr.chunk_idx = st->mov_idx_curr.stco_idx;
    sample_in_chunk_ost = sample_in_chunk_ost - (st->mov_idx_curr.stco_idx + 1 -
        stsc_parameter.stsc_first)*stsc_parameter.stsc_samples;
    st->mov_idx_curr.chunk_sample_idx = sample_in_chunk_ost;

    if(st->sample_size != 0)
        return -1;

    {//stsz must always exist in video trak, else error, must acc it
        CDX_S32 chunk_first_sample_ost;
        st->mov_idx_curr.sample_idx = st->mov_idx_curr.stsz_idx = stsc_parameter.sample_num;
        st->mov_idx_curr.chunk_sample_size = 0;

        chunk_first_sample_ost = (stsc_parameter.sample_num - sample_in_chunk_ost)<<2;
        for(i=0;i<sample_in_chunk_ost;i++)
        {
            st->mov_idx_curr.chunk_sample_size += readStsz(c, st,
                                                  chunk_first_sample_ost + (i<<2));
        }
    }

    //calc video dts
    {
        st->mov_idx_curr.stts_idx = 0;
        st->mov_idx_curr.stts_samples_acc = 0;
        st->mov_idx_curr.stts_duration_acc = 0;

        ret = locatingStts(p,st);
        if(ret < 0)
        {
            return ret;
        }
        st->mov_idx_curr.current_dts =
            st->mov_idx_curr.stts_duration_acc -
            (CDX_U64)st->mov_idx_curr.stts_sample_duration *
            (st->mov_idx_curr.stts_samples_acc - st->mov_idx_curr.sample_idx);
    }

    return 0;
}

//load the first sample to chunk and time to sample
static CDX_S32 loadFirstStscAndStts(struct CdxMovParser *p,MOVStreamContext* st,
                                      CalcSampleParameter *calc_sample_parameter)
{
    CDX_U32     idx;
    MOVContext  *c;

    c = (MOVContext*)p->privData;

    st->mov_idx_curr.stsc_idx = 0;
    st->mov_idx_curr.stts_idx = 0;
    st->mov_idx_curr.stts_samples_acc = 0;
    st->mov_idx_curr.stts_duration_acc = 0;

    idx = 0;
    calc_sample_parameter->stsz_acc = 0;

    calc_sample_parameter->stsc_first   = readStsc(c, st, 0);
    calc_sample_parameter->stsc_samples = readStsc(c, st, idx+4);
    calc_sample_parameter->stsc_next    = readStsc(c, st, idx+12);

    calc_sample_parameter->stsz_acc += calc_sample_parameter->stsc_samples;

    st->mov_idx_curr.stsc_idx++;
    if(st->mov_idx_curr.stsc_idx >= st->stsc_size)
    {
        calc_sample_parameter->stsc_next = INT_MAX;
    }
    while(1)
    {
        if(p->exitFlag)
        {
            return -1;
        }

        if (st->mov_idx_curr.stts_idx == 0)
        {
            calc_sample_parameter->stts_sample_count    = readStts(c, st, idx);
            calc_sample_parameter->stts_sample_duration = readStts(c, st, idx+4);

            st->mov_idx_curr.stts_samples_acc = calc_sample_parameter->stts_sample_count;
            st->mov_idx_curr.stts_duration_acc = (CDX_U64)calc_sample_parameter->stts_sample_count *
                calc_sample_parameter->stts_sample_duration;
            st->mov_idx_curr.stts_sample_duration = calc_sample_parameter->stts_sample_duration;
            st->mov_idx_curr.stts_idx++;
        }
        else if ((calc_sample_parameter->stsz_acc > st->mov_idx_curr.stts_samples_acc)
                && (st->mov_idx_curr.stts_idx <= (st->stts_size-1)))
        {
            idx = st->mov_idx_curr.stts_idx<<3;

            calc_sample_parameter->stts_sample_count    = readStts(c, st, idx);
            calc_sample_parameter->stts_sample_duration = readStts(c, st, idx+4);

            // for recoding 3gpp file
            if(!c->has_video)
            {
                int i;
                CDX_U32 duration = st->mov_idx_curr.stts_duration_acc;
                calc_sample_parameter->seek_audio_sample = st->mov_idx_curr.stts_samples_acc;
                for(i=0; i<calc_sample_parameter->stts_sample_count; i++)
                {
                    duration += calc_sample_parameter->stts_sample_duration;
                    if(duration > calc_sample_parameter->seek_dts)
                       break;

                    calc_sample_parameter->seek_audio_sample ++;
                }
            }

            st->mov_idx_curr.stts_samples_acc += calc_sample_parameter->stts_sample_count;
            st->mov_idx_curr.stts_duration_acc +=
                (CDX_U64)calc_sample_parameter->stts_sample_count *
                calc_sample_parameter->stts_sample_duration;
            st->mov_idx_curr.stts_sample_duration = calc_sample_parameter->stts_sample_duration;
            st->mov_idx_curr.stts_idx++;
        }

        if(calc_sample_parameter->seek_dts > 0)
        {
            if(st->mov_idx_curr.stts_duration_acc > calc_sample_parameter->seek_dts)
            {
                break;
            }
        }

        if (calc_sample_parameter->stsz_acc <= st->mov_idx_curr.stts_samples_acc)
        {
            break;
        }
    }

    return 0;
}

//seek current sample to chunk by stco_idx
static CDX_S32 seekCurrentStsc(struct CdxMovParser *p,MOVStreamContext* st,
                        CalcSampleParameter *calc_sample_parameter)
{
    CDX_U32     idx;
    MOVContext  *c;

    c = (MOVContext*)p->privData;

    st->mov_idx_curr.stsc_idx = 0;
    calc_sample_parameter->stsz_acc = 0;

    st->mov_idx_curr.stsc_first = calc_sample_parameter->stsc_first = readStsc(c, st, 0);
    while(1)
    {
        if(p->exitFlag)
        {
            return -1;
        }

        idx = st->mov_idx_curr.stsc_idx * 12;

        calc_sample_parameter->stsc_samples = readStsc(c, st, idx+4);
        calc_sample_parameter->stsc_next    = readStsc(c, st, idx+12);

        //stsc_next count from 1, stco_idx count from 0
        if(calc_sample_parameter->stsc_next > st->mov_idx_curr.stco_idx+1 ||
            st->mov_idx_curr.stsc_idx > st->stsc_size)
        {
            break;
        }
        st->mov_idx_curr.stsc_idx++;
        calc_sample_parameter->stsz_acc += calc_sample_parameter->stsc_samples *
            (calc_sample_parameter->stsc_next - calc_sample_parameter->stsc_first);
        calc_sample_parameter->stsc_first = calc_sample_parameter->stsc_next;
        if(st->mov_idx_curr.stsc_idx >= st->stsc_size)
        {
            break;
        }
    }

    return 0;
}

//calculate current dts , current stts , current stco_idx
static CDX_S32 calcCurrentSttsDtsStco(struct CdxMovParser *p,MOVStreamContext* st,
                          CDX_U32 *stsc_first_flag,CalcSampleParameter *calc_sample_parameter)
{
    CDX_U32     idx;
    MOVContext  *c;

    calc_sample_parameter->curr_chunk_duration = 0;
    c = (MOVContext*)p->privData;

    while(1)
    {
        if(p->exitFlag)
        {
            return -1;
        }

        if(st->mov_idx_curr.stco_idx+1 >= calc_sample_parameter->stsc_next)
        {
            idx = st->mov_idx_curr.stsc_idx * 12;
            //stsc_first = ReadAudioSTSC(vc, 0);

            calc_sample_parameter->stsc_samples = readStsc(c, st, idx+4);
            calc_sample_parameter->stsc_next    = readStsc(c, st, idx+12);

            //stsz_acc += stsc_samples;
            st->mov_idx_curr.stsc_idx++;
            if(st->mov_idx_curr.stsc_idx >= st->stsc_size)
            {
                calc_sample_parameter->stsc_next = INT_MAX;
            }
        }

        if (0 == *stsc_first_flag)
        {
            *stsc_first_flag = 1;
        }
        else
        {
            calc_sample_parameter->stsz_acc += calc_sample_parameter->stsc_samples;
        }

        if(st->stts_size == 1)//most of streams goes here
        {
            if(*stsc_first_flag == 2)//when stsc first flag == 2,is subtitle sample
            {
                if(calc_sample_parameter->stsz_acc > calc_sample_parameter->seek_subtitle_sample)
                {
                    break;
                }
            }
            else
            {
                if(calc_sample_parameter->stsz_acc > calc_sample_parameter->seek_audio_sample)
                {
                    break;
                }
            }
        }
        else
        {
            if(*stsc_first_flag == 2)//when stsc first flag == 2,is subtitle sample
            {
                calc_sample_parameter->curr_chunk_duration +=
                    (CDX_U64)calc_sample_parameter->stsc_samples*
                    calc_sample_parameter->stts_sample_duration;
                if(calc_sample_parameter->curr_chunk_duration > calc_sample_parameter->seek_dts)
                    break;

                if(calc_sample_parameter->stsz_acc > st->mov_idx_curr.stts_samples_acc
                    && st->mov_idx_curr.stts_idx < st->stts_size-1)
                {
                    idx = st->mov_idx_curr.stts_idx<<3;

                    calc_sample_parameter->stts_sample_count    = readStts(c, st, idx);
                    calc_sample_parameter->stts_sample_duration = readStts(c, st, idx+4);

                    st->mov_idx_curr.stts_samples_acc +=
                        calc_sample_parameter->stts_sample_count;
                    st->mov_idx_curr.stts_duration_acc +=
                        (CDX_U64)calc_sample_parameter->stts_sample_count *
                        calc_sample_parameter->stts_sample_duration;
                    st->mov_idx_curr.stts_sample_duration =
                        calc_sample_parameter->stts_sample_duration;
                    st->mov_idx_curr.stts_idx++;
                }
            }
            else
            {
                while(1)
                {
                    if(p->exitFlag)
                    {
                        return -1;
                    }

                    if((calc_sample_parameter->stsz_acc > st->mov_idx_curr.stts_samples_acc)
                        && (st->mov_idx_curr.stts_idx <= (st->stts_size-1)))
                    {
                        idx = st->mov_idx_curr.stts_idx<<3;

                        calc_sample_parameter->stts_sample_count    = readStts(c, st, idx);
                        calc_sample_parameter->stts_sample_duration = readStts(c, st, idx+4);

                        st->mov_idx_curr.stts_samples_acc += calc_sample_parameter->stts_sample_count;
                        st->mov_idx_curr.stts_duration_acc +=
                            (CDX_U64)calc_sample_parameter->stts_sample_count *
                            calc_sample_parameter->stts_sample_duration;
                        st->mov_idx_curr.stts_sample_duration =
                            calc_sample_parameter->stts_sample_duration;
                        st->mov_idx_curr.stts_idx++;
                    }
                    else
                    {
                        break;
                    }

                    if(st->mov_idx_curr.stts_duration_acc > calc_sample_parameter->seek_dts)
                    {
                        break;
                    }
                }
                calc_sample_parameter->curr_chunk_duration =
                st->mov_idx_curr.stts_duration_acc -
                           (CDX_U64)st->mov_idx_curr.stts_sample_duration *
                           (st->mov_idx_curr.stts_samples_acc - calc_sample_parameter->stsz_acc);
                if (calc_sample_parameter->stsz_acc > st->mov_idx_curr.stts_samples_acc)
                {
                    calc_sample_parameter->curr_chunk_duration = st->mov_idx_curr.stts_duration_acc;
                    break;
                }
                else if (calc_sample_parameter->curr_chunk_duration > calc_sample_parameter->seek_dts)
                {
                    break;
                }
            }
        }
        if(st->mov_idx_curr.stco_idx >= st->stco_size)
        {
            st->read_va_end = 1;
            break;
        }

        st->mov_idx_curr.stco_idx++;
    }

    if(st->stts_size == 1)//most of streams goes here
    {
        if(st->stco_size==1)
        {
            if(*stsc_first_flag == 2)//when stsc first flag == 2,is subtitle sample
            {
                calc_sample_parameter->curr_chunk_duration =
                (CDX_U64)calc_sample_parameter->seek_subtitle_sample*
                calc_sample_parameter->stts_sample_duration;
            }
            else
            {
                 calc_sample_parameter->curr_chunk_duration =
                 (CDX_U64)calc_sample_parameter->seek_audio_sample *
                 calc_sample_parameter->stts_sample_duration;
            }
        }
        else
        {
            calc_sample_parameter->curr_chunk_duration =
            (CDX_U64)calc_sample_parameter->stsz_acc *
            calc_sample_parameter->stts_sample_duration;
        }
    }

    st->mov_idx_curr.current_dts = calc_sample_parameter->curr_chunk_duration;
    //c->mov_idx_curr[1].stsc_first = stsc_first;
    st->mov_idx_curr.stsc_samples = calc_sample_parameter->stsc_samples;
    st->mov_idx_curr.stsc_end = calc_sample_parameter->stsc_next;
    st->mov_idx_curr.chunk_idx = st->mov_idx_curr.stco_idx;//+1 to make it read stsc_end

    return 0;
}

static CDX_S32 seekAudioSample(struct CdxMovParser *p,MOVStreamContext* st,MOVContext *c,
                        CDX_S32 seconds)
{
    CDX_U32 stsc_first_flag = 0;
    CDX_S32 ret,i = 0,chunk_num = 0;
    CDX_U32 idx;
    CalcSampleParameter calc_sample_parameter;

    calc_sample_parameter.stsz_acc = 0;
    calc_sample_parameter.seek_audio_sample = 0;
    calc_sample_parameter.stts_sample_duration = 0;

    //search audio stco > video stco
    st->mov_idx_curr.stco_idx = 0;

    if(c->has_video)
    {
        calc_sample_parameter.seek_dts = c->streams[c->video_stream_idx]->mov_idx_curr.current_dts *
            st->time_scale/c->streams[c->video_stream_idx]->time_scale;
    }
    else
        calc_sample_parameter.seek_dts = (CDX_U64)st->time_scale * seconds/1000;

    ret = loadFirstStscAndStts(p,st,&calc_sample_parameter);
    if(ret < 0)
    {
        return ret;
    }

    if(st->stts_size == 1)
    {
        calc_sample_parameter.seek_audio_sample = (CDX_U32)(calc_sample_parameter.seek_dts/
            calc_sample_parameter.stts_sample_duration);
    }

    ret = calcCurrentSttsDtsStco(p,st,&stsc_first_flag,&calc_sample_parameter);
    if(ret < 0)
    {
        return ret;
    }

    //audio stsc search begin  stsz calc
    ret = seekCurrentStsc(p,st,&calc_sample_parameter);
    if(ret < 0)
    {
        return 0;
    }

    st->mov_idx_curr.stsc_samples =
        calc_sample_parameter.stsc_samples;//add 20090512 to fix audio 1 table bug
    st->mov_idx_curr.stsc_end = 0; //force to read a audio stco idx
    //c->mov_idx_curr[1].stsc_first = stsc_first-1;
    //we must add the skiped stsc
    calc_sample_parameter.stsz_acc += calc_sample_parameter.stsc_samples *
    (st->mov_idx_curr.stco_idx - (calc_sample_parameter.stsc_first-1));

    if(st->sample_size == 0)
    {
        st->mov_idx_curr.stsz_idx = calc_sample_parameter.stsz_acc;
    }
    else
    {
        st->mov_idx_curr.stsz_idx = 0;
    }

    if(st->stco_size == 1)
    {   //some 3GP stream goes here
        st->mov_idx_curr.sample_idx = calc_sample_parameter.seek_audio_sample;
        st->mov_idx_curr.stsz_idx = 0;
        st->mov_idx_curr.chunk_sample_size = 0;
        for(idx=0; idx<calc_sample_parameter.seek_audio_sample; idx++)
        {
            st->mov_idx_curr.chunk_sample_size += readStsz(c, st,
               st->mov_idx_curr.stsz_idx<<2);

            st->mov_idx_curr.stsz_idx++;
        }
        st->mov_idx_curr.chunk_sample_idx = calc_sample_parameter.seek_audio_sample;
        c->chunk_sample_idx_assgin_by_seek = 1;
    }
    else
    {
        // for 3D-1080P-B052.mov, the sample_idx is not right.
        // but it is not a good idea
        if((st->stts_size > 1) && (calc_sample_parameter.stsc_samples > 10))
        {
            st->mov_idx_curr.sample_idx = calc_sample_parameter.stsz_acc +
                calc_sample_parameter.stsc_samples;
            //st->mov_idx_curr.sample_idx = st->mov_idx_curr.stts_samples_acc-1;
            CDX_LOGD("--- sample_idx=%d", st->mov_idx_curr.sample_idx);

            st->mov_idx_curr.stco_idx++;
            st->mov_idx_curr.stsz_idx = st->mov_idx_curr.sample_idx;
            st->mov_idx_curr.chunk_idx++;
            st->mov_idx_curr.chunk_sample_size = 0;
            st->mov_idx_curr.chunk_sample_idx = 0;
        }
        else
        {
            st->mov_idx_curr.sample_idx = calc_sample_parameter.stsz_acc;
            st->mov_idx_curr.chunk_sample_size = 0;
            st->mov_idx_curr.chunk_sample_idx = 0;
            if(st->eCodecFormat != AUDIO_CODEC_FORMAT_PCM)
            {
                if(st->stts_size == 1)
                {
                    chunk_num = calc_sample_parameter.seek_audio_sample -
                        calc_sample_parameter.stsz_acc;
                    if(chunk_num > 0)
                    {
                        for(i = 0;i < chunk_num;i++)
                        {
                            CDX_U64 curr_sample_siez = 0;
                            curr_sample_siez = readStsz(c,st,st->mov_idx_curr.stsz_idx<<2);
                            st->mov_idx_curr.stsz_idx++;
                            st->mov_idx_curr.chunk_sample_idx++;
                            st->mov_idx_curr.chunk_sample_size += curr_sample_siez;
                        }
                        //st->mov_idx_curr.stco_idx++;
                        st->mov_idx_curr.sample_idx = calc_sample_parameter.seek_audio_sample;
                        st->mov_idx_curr.current_dts =
                            (CDX_U64)calc_sample_parameter.seek_audio_sample *
                        calc_sample_parameter.stts_sample_duration;
                    }
                }
            }
        }
    }

    return 0;
}

static CDX_S32 seekSubtitleSample(struct CdxMovParser *p,MOVStreamContext* st,MOVContext *c)
{
    CDX_U32 stsc_first_flag = 2;

    if(p->exitFlag)
    {
        return -1;
    }

    //CDX_S32 video_chunk_ost,audio_chunk_ost;
    CalcSampleParameter calc_sample_parameter;
    CDX_U32 idx;
    CDX_S32 ret;

    //search audio stco > video stco
    st->mov_idx_curr.stco_idx = 0;
    calc_sample_parameter.seek_subtitle_sample = 0;
    calc_sample_parameter.curr_chunk_duration = 0;
    st->mov_idx_curr.stsc_idx = 0;
    st->mov_idx_curr.stts_idx = 0;
    idx = 0;
    calc_sample_parameter.stsz_acc = 0;

    calc_sample_parameter.stsc_first = readStsc(c, st, 0);
    calc_sample_parameter.stsc_samples = readStsc(c, st, idx+4);
    calc_sample_parameter.stsc_next    = readStsc(c, st, idx+12);

    st->mov_idx_curr.stsc_idx++;
    if(st->mov_idx_curr.stsc_idx >= st->stsc_size)
    {
        calc_sample_parameter.stsc_next = INT_MAX;
    }

    calc_sample_parameter.stts_sample_count    = readStts(c, st, idx);
    calc_sample_parameter.stts_sample_duration = readStts(c, st, idx+4);

    st->mov_idx_curr.stts_samples_acc = calc_sample_parameter.stts_sample_count;
    st->mov_idx_curr.stts_duration_acc = (CDX_U64)calc_sample_parameter.stts_sample_count *
        calc_sample_parameter.stts_sample_duration;
    st->mov_idx_curr.stts_sample_duration = calc_sample_parameter.stts_sample_duration;
    st->mov_idx_curr.stts_idx = 1;
    calc_sample_parameter.seek_dts = c->streams[c->video_stream_idx]->mov_idx_curr.current_dts *
        st->time_scale/c->streams[c->video_stream_idx]->time_scale;

    if(st->stts_size == 1)
    {
        calc_sample_parameter.seek_subtitle_sample = (CDX_U32)(calc_sample_parameter.seek_dts/
            calc_sample_parameter.stts_sample_duration);
    }

    ret = calcCurrentSttsDtsStco(p,st,&stsc_first_flag,&calc_sample_parameter);
    if(ret < 0)
    {
        return ret;
    }

    //audio stsc search begin  stsz calc
    ret = seekCurrentStsc(p,st,&calc_sample_parameter);
    if(ret < 0)
    {
        return 0;
    }

    st->mov_idx_curr.stsc_samples =
        calc_sample_parameter.stsc_samples;//add 20090512 to fix audio 1 table bug
    st->mov_idx_curr.stsc_end = 0; //force to read a audio stco idx

    //we must add the skiped stsc
    calc_sample_parameter.stsz_acc += calc_sample_parameter.stsc_samples*
    (st->mov_idx_curr.stco_idx - (calc_sample_parameter.stsc_first-1));
    if(st->sample_size == 0)
    {
        st->mov_idx_curr.stsz_idx = calc_sample_parameter.stsz_acc;
    }
    else
    {
        st->mov_idx_curr.stsz_idx = 0;
    }

    if(st->stco_size==1)
    {   //some 3GP stream goes here
        st->mov_idx_curr.sample_idx = calc_sample_parameter.seek_subtitle_sample;
        st->mov_idx_curr.stsz_idx = 0;
        st->mov_idx_curr.chunk_sample_size = 0;
        for(idx=0;idx<calc_sample_parameter.seek_subtitle_sample;idx++)
        {
            st->mov_idx_curr.chunk_sample_size += readStsz(c, st,
                st->mov_idx_curr.stsz_idx<<2);

            st->mov_idx_curr.stsz_idx++;
        }
        st->mov_idx_curr.chunk_sample_idx = calc_sample_parameter.seek_subtitle_sample;
        c->chunk_sample_idx_assgin_by_seek = 1;
    }
    else
    {
        st->mov_idx_curr.sample_idx = calc_sample_parameter.stsz_acc;
        st->mov_idx_curr.chunk_sample_size = 0;
        st->mov_idx_curr.chunk_sample_idx = 0;
    }

    return 0;
}

//when seek a sample, we must update:
//video: stsc_idx  stco_idx stsz_idx chunk_sample_size   chunk_sample_idx    sample_idx
//audio: stsc_idx  stco_idx stsz_idx chunk_sample_size=0 chunk_sample_idx=0  chunk_idx=stco_idx-1
//(sample_num: 0..x)
static CDX_S16 movSeekKeySample(struct CdxMovParser *p, CDX_U32 sample_num, CDX_S32 seconds)
{
    MOVContext      *c;
    CDX_S32         ret;
    CDX_S32         i;
    CDX_U64         video_time = 0,audio_time = 0;

    c = (MOVContext*)p->privData;
    MOVStreamContext* st;

    // we should seek the video firstly, then seek audio and subtitle,
    // because they are depending on video current dts after seek
    if(c->has_video)
    {
        st = c->streams[c->video_stream_idx];
        if(st->stsd_type == 1)
        {
            ret = seekVideoSample(p,c,st,sample_num);
            if(ret < 0)
            {
                return ret;
            }
        }
        video_time = st->mov_idx_curr.current_dts * 1000 / st->time_scale;
    }

    for(i=0; i<c->nb_streams; i++)
    {
        st = c->streams[i];
        //----------------------------------------------------
        //      we should update audio offsets also
        //----------------------------------------------------
        if(st->stsd_type == 2)
        {
            if(c->has_video)
            {
                if(seconds == 0 || video_time == 0)
                {
                    c->streams[c->video_stream_idx]->mov_idx_curr.current_dts = 0;
                    st->mov_idx_curr.current_dts = 0;
                    st->mov_idx_curr.stsz_idx = 0;
                    st->mov_idx_curr.chunk_sample_idx = 0;
                    st->mov_idx_curr.chunk_sample_size = 0;
                    st->mov_idx_curr.sample_idx = 0;
                }
            }
            ret = seekAudioSample(p,st,c,seconds);
            if(ret < 0)
            {
                return ret;
            }
            audio_time = st->mov_idx_curr.current_dts * 1000 / st->time_scale;
        }
        //----------------------------------------------------
        //      we should update subtitle offsets also
        //----------------------------------------------------
        else if(st->stsd_type == 3)
        {
            ret = seekSubtitleSample(p,st,c);
            if(ret < 0)
            {
                return ret;
            }
        }
    }
    CDX_LOGV("--- +++++video_time:%lld audio_time:%lld\n",video_time,audio_time);

    return 0;
}

static CDX_S32 loadNewStsc(MOVContext *c,MOVStreamContext* st)
{
    CDX_S32    idx;
    if(st->mov_idx_curr.stsc_idx < st->stsc_size
       && st->mov_idx_curr.chunk_idx >= st->mov_idx_curr.stsc_end)
    {
        idx = st->mov_idx_curr.stsc_idx*12;

        //first chunk id in 'stsc' atom
        st->mov_idx_curr.stsc_first = readStsc(c, st, idx) - 1;
        //sample number of this chunk
        st->mov_idx_curr.stsc_samples = readStsc(c, st, idx + 4);

        /////******************  care , i do not kown it. maybe for audio seek
        if(st->stsd_type==2 && c->chunk_sample_idx_assgin_by_seek)
        {
            c->chunk_sample_idx_assgin_by_seek = 0;//assigned by seek
            // maybe should      st->chunk_sample_idx_assgin_by_seek = 0;
            CDX_LOGE("***** maybe error here");
        }

        st->mov_idx_curr.stsc_idx++;

        // the next entry of stsc atom
        st->mov_idx_curr.stsc_end = readStsc(c, st, idx + 12) - 1;
        // check if it is beyond the boundary
        if(st->mov_idx_curr.stsc_idx >= st->stsc_size)
        {
            st->mov_idx_curr.stsc_end = INT_MAX;
        }

        //? skip one redundance stsc table for stream Tomsk_PSP.mp4,
        // some entry of this table is error
        if(st->mov_idx_curr.stsc_end == st->mov_idx_curr.stsc_first)
        {
            idx = st->mov_idx_curr.stsc_idx*12;
            st->mov_idx_curr.stsc_first = readStsc(c, st, idx) - 1;
            st->mov_idx_curr.stsc_samples = readStsc(c, st, idx + 4);
            st->mov_idx_curr.chunk_sample_idx = 0;//a new sample idx
            st->mov_idx_curr.stsc_idx++;
            st->mov_idx_curr.stsc_end = readStsc(c, st, idx + 12) - 1;

            if(st->mov_idx_curr.stsc_idx >= st->stsc_size)
            {
                st->mov_idx_curr.stsc_end = INT_MAX;
            }
        }

   }

   return 0;
}

static CDX_S32 calcSampleSize(MOVContext *c,MOVStreamContext *st,CDX_U64 vas_chunk_ost[],
                      CDX_U64 *curr_sample_size,MOV_CHUNK *chunk_info)
{
    //read current sample size;
    //video or there is stsz table
    if(st->sample_size == 0  || st->stsd_type == 1 || st->stsd_type == 3)
    {
        // the size of this sample in stsz
        *curr_sample_size = readStsz(c, st, st->mov_idx_curr.stsz_idx<<2);

        //add the current sample size tp the chunk_sample_size to get the next sample offset
        st->mov_idx_curr.chunk_sample_size += *curr_sample_size;
        st->mov_idx_curr.stsz_idx++;
    }
    else /*No stsz table, read from stsc for 1 chunk, audio sample size are all same,
   (else only for audio)*/
    {
        //MPX000002.3gp akira.mov will goes here
        if (st->sample_size> 1 )
        {
            if ((st->eCodecFormat == AUDIO_CODEC_FORMAT_PCM) && st->audio_sample_size)
            {
                if (st->sample_size != 0)
                {                    //201200621 for ulaw;
                    st->audio_sample_size = st->sample_size;
                }
                *curr_sample_size = st->mov_idx_curr.stsc_samples * st->audio_sample_size;
            }
            else //MPX000002.3gp akira.mov will goes here
            {
                *curr_sample_size = st->mov_idx_curr.stsc_samples * st->sample_size;
            }
            st->mov_idx_curr.chunk_sample_size += *curr_sample_size;
            if(st->mov_idx_curr.stsc_samples > 1 &&
                st->eCodecFormat == AUDIO_CODEC_FORMAT_MPEG_AAC_LC)
            {
                chunk_info->cycle_to = st->mov_idx_curr.stsc_samples;;
                chunk_info->audio_segsz = (*curr_sample_size) / st->mov_idx_curr.stsc_samples;
                chunk_info->no_completed_read = 1;
            }
        }
        else if ( st->samples_per_frame > 0 && //akira.mov will goes here
                  (st->mov_idx_curr.stsc_samples * st->bytes_per_frame %
                  st->samples_per_frame == 0))
        {
            if (1 || st->samples_per_frame < 160) //.lys 2012-03-26 always set it to compute!
            {
                *curr_sample_size = st->mov_idx_curr.stsc_samples * st->bytes_per_frame /
                    st->samples_per_frame;
            }
            else
            {
                //!!maybe multiframe error! see ffmpeg
                *curr_sample_size = st->bytes_per_frame;
            }
            st->mov_idx_curr.chunk_sample_size += *curr_sample_size;
        }
        else if (st->sample_size == 1 )//MPX000002.3gp akira.mov P1000100.MOV will goes here
        {
            if(st->audio_sample_size)
                *curr_sample_size = st->mov_idx_curr.stsc_samples * st->audio_sample_size;
            else
                *curr_sample_size = st->mov_idx_curr.stsc_samples * st->sample_size;
            st->mov_idx_curr.chunk_sample_size += *curr_sample_size;
        }
        else
        { //kodak.mov will goes here  //!caution, not same as origin source
            if(c->nb_streams<3)
            {
                *curr_sample_size = vas_chunk_ost[0] - vas_chunk_ost[1];
                st->mov_idx_curr.chunk_sample_size += *curr_sample_size;
            }
            else
            {/*has not found any stream, it's too complex to decode the stream,
            not decode the stream */
                *curr_sample_size = 0;
            }
        }
    }

    return 0;
}

static CDX_S32 calcStts(struct CdxMovParser *p,MOVContext *c,MOVStreamContext* st)
{
    CDX_S32 ret;

    ret = locatingStts(p,st);
    if(ret < 0)
    {
        return ret;
    }
    //current pts
    if(st->mov_idx_curr.stts_duration_acc < (CDX_U64)st->mov_idx_curr.stts_sample_duration *
                    (st->mov_idx_curr.stts_samples_acc - st->mov_idx_curr.sample_idx))
    {
        st->mov_idx_curr.current_dts = 0;
    }
    else
    {
        st->mov_idx_curr.current_dts =
                    st->mov_idx_curr.stts_duration_acc -
                    (CDX_U64)st->mov_idx_curr.stts_sample_duration *
                    (st->mov_idx_curr.stts_samples_acc - st->mov_idx_curr.sample_idx);
    }
    // only video sample need add cts
    if(st->stsd_type == 1 && !c->is_fragment && st->eCodecFormat != VIDEO_CODEC_FORMAT_H265)
    {
        if(st->ctts_data && st->ctts_index < st->ctts_size)
        {
            st->mov_idx_curr.current_dts += (st->dts_shift +
                st->ctts_data[st->ctts_index].duration);
            //update ctts context
            st->ctts_sample ++;
            if((st->ctts_index < st->ctts_size) &&
                (st->ctts_data[st->ctts_index].count == st->ctts_sample) )
            {
                st->ctts_index ++;
                st->ctts_sample = 0;
            }
        }
    }

    if(st->stsd_type == 3)
    {
        st->mov_idx_curr.current_duration = (CDX_U64)st->mov_idx_curr.stts_sample_duration;
    }
    else
    {
        st->mov_idx_curr.current_duration =
                        (CDX_U64)st->mov_idx_curr.stts_sample_duration *
                        (st->mov_idx_curr.stts_samples_acc - st->mov_idx_curr.sample_idx);
    }

    return 0;
}

static CDX_S32 readStreamSample(struct CdxMovParser *p,MOVStreamContext *st,
                            CDX_U64 vas_chunk_ost[],CDX_S32 va_sel)
{
    MOVContext      *c;
    CDX_U64         curr_sample_offset,curr_sample_size;
    MOV_CHUNK       *chunk_info;
    CDX_S32         ret;

    c = (MOVContext*)p->privData;
    chunk_info = &c->chunk_info;

    loadNewStsc(c,st);

    //calc current sample offset;
    curr_sample_offset = vas_chunk_ost[va_sel] + st->mov_idx_curr.chunk_sample_size;

    calcSampleSize(c,st,vas_chunk_ost,&curr_sample_size,chunk_info);

    ret = calcStts(p,c,st);
    if(ret < 0)
    {
        return ret;
    }

    //save chunk information
    chunk_info->type   = st->stsd_type;
    chunk_info->offset = curr_sample_offset;  // calculate from stsc
    chunk_info->length = curr_sample_size;    // read from stsz

    if(st->sample_size == 0 || st->stsd_type == 1 || st->stsd_type == 3)
    {
        st->mov_idx_curr.sample_idx++;//for video every sample(not every chunck) gets here
        st->mov_idx_curr.chunk_sample_idx++;
    }
    else
    {
        //c->mov_idx_curr[va_sel].sample_idx += curr_sample_size;
        st->mov_idx_curr.sample_idx += st->mov_idx_curr.stsc_samples;
        //modify 080508 every chunck gets
        st->mov_idx_curr.chunk_sample_idx = INT_MAX;
    }

    //read 1 chunk end
    if(st->mov_idx_curr.chunk_sample_idx >= st->mov_idx_curr.stsc_samples)
    {
        st->mov_idx_curr.chunk_sample_idx = 0;//for skiped chunk reset
        st->mov_idx_curr.stco_idx++;
        st->mov_idx_curr.chunk_idx++;
        st->mov_idx_curr.chunk_sample_size = 0;
        if(st->mov_idx_curr.stco_idx >= st->stco_size)
            st->read_va_end = 1;
    }

    return 0;
}

//************************************************************************/
//*  read a sample sequence in streams      (fragment MP4) */
//*  Return value  :
//*      1   read finish
//*      0  allright
//*    -1  read error
//************************************************************************/
CDX_S16 MovReadSampleFragment(struct CdxMovParser *p)
{
    int ret;
    MOVContext*         s = (MOVContext*)p->privData;
    AW_List* Vsamples = s->Vsamples;
    AW_List* Asamples = s->Asamples;
    MOV_CHUNK* chunk_info = &s->chunk_info;
    int a_end = 0, v_end = 0;

    if(!s->has_audio)
    {
        a_end = 1;
    }
    if(!s->has_video)
    {
        v_end = 1;
    }

    // get the video and audio sample list
    if(s->has_audio && !a_end)
    {
        while(!aw_list_count(Asamples) )
        {
            ret = movReadSampleList(p);
            if(ret == 1)
            {
                a_end = 1;
                break;
            }
            else if(ret < 0)
            {
                return -1;
            }
        }
    }
    if(s->has_video && !v_end)
    {
        while(!aw_list_count(Vsamples) )
        {
            ret = movReadSampleList(p);
            if(ret == 1)
            {
                v_end = 1;
                break;
            }
            else if (ret < 0)
            {
                return -1;
            }
        }
    }

    CDX_U64 apts = 0xFFFFFFFF, vpts = 0xFFFFFFFF;
    Sample* Vsample = NULL;
    Sample* Asample = NULL;
    CDX_U64 voffset = 0xFFFFFFFF;
    CDX_U64 aoffset = 0xFFFFFFFF;
    CDX_U64 vNum = aw_list_count(Vsamples);
    //CDX_LOGD("lin vNum = %llu, s->video_stream_idx = %d", vNum, s->video_stream_idx);
    if(s->has_video && vNum)
    {
        // get the video sample
        Vsample = aw_list_get(Vsamples, 0);
        if(!Vsample)
        {
            voffset = 0xFFFFFFFF;
        }
        else
        {
            if(s->bSmsSegment && s->nSmsMdatOffset && !s->nDataOffsetDelta) //* for sms
            {
                //CDX_LOGD("000 s->nSmsMdatOffset = %lld, s->fp=%p, Vsample->offset=%llu",
                //s->nSmsMdatOffset, s->fp, Vsample->offset);
                Vsample->offset += s->nSmsMdatOffset;
                //CDX_LOGD("111 s->nSmsMdatOffset = %lld, s->fp=%p, Vsample->offset=%llu",
                //s->nSmsMdatOffset, s->fp, Vsample->offset);
            }
            voffset = Vsample->offset;
        }
        vpts = (CDX_S64)s->streams[s->video_stream_idx]->mov_idx_curr.current_dts * 1000 * 1000
                                / s->streams[s->video_stream_idx]->time_scale +
                                s->basetime[0]*1000*1000;
        CDX_LOGV("xxx vpts=%lld, current dts=%llu, s->video_stream_idx=%d,"
            " s->streams[s->video_stream_idx]->time_scale=%d, s->basetime[0]=%d, voffset=%llu",
           vpts, (CDX_S64)s->streams[s->video_stream_idx]->mov_idx_curr.current_dts,
           s->video_stream_idx, s->streams[s->video_stream_idx]->time_scale,
           s->basetime[0], voffset);
    }

    if(s->has_audio && aw_list_count(Asamples))
    {
        //get the audio sample and the sample pts
        Asample = aw_list_get(Asamples, 0);
        if(!Asample)
        {
            aoffset = 0xFFFFFFFF;
        }
        else
        {
            aoffset = Asample->offset;
        }
        apts = (CDX_S64)s->streams[s->audio_stream_idx]->mov_idx_curr.current_dts * 1000*1000
                            / s->streams[s->audio_stream_idx]->time_scale +
                            s->basetime[1]*1000*1000;
    }
//    CDX_LOGD("has_video = %d, has_audio = %d", s->has_video, s->has_audio);

    // how to select the sample( audio or video ) to show, there were two methods to deal with it
    // @ 1.compare with their pts, the sample with the smaller pts would be show in first
    // @ 2.compare with their sample offset. this method should be used in network
    if( (vpts < apts) && s->has_video && !v_end)
    {
        s->streams[s->video_stream_idx]->mov_idx_curr.current_dts += (CDX_U64)Vsample->duration;
        s->streams[s->video_stream_idx]->mov_idx_curr.current_duration = (CDX_U64)Vsample->duration;

        chunk_info->offset = Vsample->offset;
        chunk_info->length = Vsample->size;
        chunk_info->index = Vsample->index;
        chunk_info->type = 0; //video
        s->prefetch_stream_index = s->video_stream_idx;
        free(Vsample);
        ret = aw_list_rem(Vsamples, 0);
        if(ret < 0)
        {
            return -1;
        }
    }
    else if((vpts >= apts) && s->has_audio && !a_end)
    {
        s->streams[s->audio_stream_idx]->mov_idx_curr.current_dts += (CDX_U64)Asample->duration;
        s->streams[s->audio_stream_idx]->mov_idx_curr.current_duration =
            (CDX_U64)Asample->duration;

        chunk_info->offset = Asample->offset;
        chunk_info->length = Asample->size;
        chunk_info->index = Asample->index;
        chunk_info->type = 1; //audio
        s->prefetch_stream_index = s->audio_stream_idx;
        free(Asample);
        ret = aw_list_rem(Asamples, 0);
        if(ret < 0)
        {
            return -1;
        }
    }

    if(v_end && a_end)
    {
        return READ_FINISH;
    }

    return READ_FREE;
}

//************************************************************************/
//*  read a sample sequence in streams   (normal MP4 file) */
//*  Return value  :
//*      1   read finish
//*      0  allright
//*    -1  read error
//************************************************************************/
CDX_S16 MovReadSample(struct CdxMovParser *p)
{
    MOVContext        *c;
    CDX_U64           vas_time[MOV_MAX_STREAMS];
    CDX_U64           vas_chunk_ost[MOV_MAX_STREAMS];
    CDX_S32           va_sel = 1;
    CdxStreamT*       pb;
    CDX_S32           ret;

    c = (MOVContext*)p->privData;
    pb = c->fp;

    MOVStreamContext *st = NULL;

    if (c->isReadEnd == 1)
    {
        CDX_LOGW("return read finish\n");
        return READ_FINISH;
    }

    int i;
    for(i=0; i<c->nb_streams; i++)
    {
        //CDX_LOGD("+++++ stsd = %d, c->streams[%d]->read_va_end = %d",
        //c->streams[i]->stsd_type, i, c->streams[i]->read_va_end);
        vas_chunk_ost[i] = (CDX_U64)-1LL;
        vas_time[i] = (CDX_U64)-1LL;
    }

    for(i=0; i<c->nb_streams; i++)
    {
        st = c->streams[i];
        if((st->stsd_type && !st->read_va_end))
        {
            // we only need one video stream
            if(st->stsd_type == 1 && st->stream_index != c->select_video_stream_idx)
            {
                st->read_va_end = 1;
                continue;
            }
            vas_chunk_ost[i] = readStco(c, st, st->mov_idx_curr.stco_idx);
        }
    }

    // read sample from file offset in network stream,
    // from dts ( not pts ) in local file
    if(CdxStreamIsNetStream(pb))
    {
        va_sel = getMin(vas_chunk_ost, c->nb_streams);
    }
    else
    {
        for(i=0; i<c->nb_streams; i++)
        {
            st = c->streams[i];
            if(st->stsd_type && !st->read_va_end)
            {
                if(st->stsd_type == 1 && st->stream_index != c->select_video_stream_idx)
                {
                    st->read_va_end = 1;
                    continue;
                }
                vas_time[i] = st->mov_idx_curr.current_dts * 1000 / st->time_scale;
            }
        }
        va_sel = getMin(vas_time, c->nb_streams);
    }

    c->prefetch_stream_index = va_sel;
    st = c->streams[va_sel];

    ret = readStreamSample(p,st,vas_chunk_ost,va_sel);
    if(ret < 0)
    {
        return ret;
    }

    for(i=0; i<c->nb_streams; i++)
    {
        //CDX_LOGD("stsd = %d, c->streams[%d]->read_va_end = %d", c->streams[i]->stsd_type,
        //i, c->streams[i]->read_va_end);
        if((c->streams[i]->read_va_end == 0) && c->streams[i]->stsd_type)
        {
            break;
        }
    }

    if(i==c->nb_streams)
    {
        CDX_LOGW("read finish\n");
        c->isReadEnd = 1;
    }

    return READ_FREE;
}

CDX_S32 MovSeekSampleFragment(struct CdxMovParser *p,MOVContext *c,cdx_int64  timeUs)
{
    CDX_S32    result;

    // need to clean up Vsamples and Asamples
    while(aw_list_count(c->Vsamples))
    {
        void* item = aw_list_last(c->Vsamples);
        aw_list_rem_last(c->Vsamples);
        if(item)
        {
            free(item);
            item = NULL;
        }
    }
    while(aw_list_count(c->Asamples))
    {
        void* item = aw_list_last(c->Asamples);
        aw_list_rem_last(c->Asamples);
        if(item)
        {
            free(item);
            item = NULL;
        }
    }

    //sidx mode for single url in DASH
    if(!p->bDashSegment && !p->bSmsSegment) //* for sms
    {

        if(c->has_video)
        {
            result = movTimeToSampleSidx(p, timeUs/1000, c->video_stream_idx, 0);
            if(result < 0)
            {
                return -1;
            }
        }
        if(c->has_audio)
        {
            result = movTimeToSampleSidx(p, timeUs/1000, c->audio_stream_idx, 1);
            if(result < 0)
            {
                return -1;
            }
        }

        c->moof_end_offset = CdxStreamTell(c->fp); // we must reset it when jump
    }
    else  //mutil url in DASH
    {
        result = MovReadSampleFragment(p);
        if(c->has_video)
        {
            result = movTimeToSampleSidx(p, timeUs/1000, c->video_stream_idx, 0);
            if(result < 0)
            {
                return -1;
            }
        }
        if(c->has_audio)
        {
            result = movTimeToSampleSidx(p, timeUs/1000, c->audio_stream_idx, 1);
            if(result < 0)
            {
                return -1;
            }
        }
    }
    return 0;
}

CDX_S32 MovSeekSample(struct CdxMovParser *p,MOVContext *c,cdx_int64 seekTime,
                               SeekModeType seekModeType)
{
    CDX_S32        result;
    CDX_S32        frmidx = 0;

    if(seekTime >= (int)p->totalTime)
    {
        p->mErrno = PSR_EOS;
        return 1;
    }
    else
    {
       c->isReadEnd = 0;
    }

    if (c->has_video)
    {
        //find the video sample nearest seekTime
        frmidx = movTimeToSample(p, seekTime);
        if(frmidx < 0)
        {
            CDX_LOGW("TimeToSample failed!\n");
            return -1;
        }
        //CDX_LOGV("---- seek_time = %lld", timeUs);
        CDX_LOGV("############## sfrmidx = [%d]", frmidx);
        if(!c->bSeekWithoutKeyframe)
            frmidx = movFindKeyframe(p, (unsigned int)frmidx, seekModeType, seekTime);
        CDX_LOGD("--- key frame = [%d], seekWithoutKeyFrame [%s]", frmidx,
            c->bSeekWithoutKeyframe ? "Yes" : "No");
        if(frmidx < 0)
        {
            CDX_LOGW("look for key frame failed!\n");
            return -1;
        }

        // adjust ctts index
        MOVStreamContext* sc = c->streams[c->video_stream_idx];
        int time_sample;
        int i;
        if(sc->ctts_data)
        {
            time_sample = 0;
            int next = 0;
            for(i=0; i<sc->ctts_size; i++)
            {
                next = time_sample + sc->ctts_data[i].count;
                if(next > frmidx)
                {
                    sc->ctts_index = i;
                    sc->ctts_sample = frmidx-time_sample;
                    break;
                }
                time_sample = next;
            }
        }
    }
    result = movSeekKeySample(p, frmidx, seekTime);
    if(result < 0)
    {
        CDX_LOGW("Seek sample failed!\n");
        return -1;
    }

    if(CdxStreamIsNetStream(c->fp))
    {
        CDX_U64          vas_chunk_ost[MOV_MAX_STREAMS];
        CDX_S32          va_sel = 1;

        MOVStreamContext *st = NULL;

        int i;
        for(i=0; i<c->nb_streams; i++)
        {
            vas_chunk_ost[i] = (CDX_U64)-1LL;
        }

        for(i=0; i<c->nb_streams; i++)
        {
            st = c->streams[i];
            if((st->stsd_type && !st->read_va_end))
            {
                // we only need one video stream
                if(st->stsd_type == 1 && st->stream_index != 0)
                {
                    st->read_va_end = 1;
                    continue;
                }
                vas_chunk_ost[i] = readStco(c, st, st->mov_idx_curr.stco_idx);
            }
        }

        // read sample from file offset in network stream,
        // from dts ( not pts ) in local file
        va_sel = getMin(vas_chunk_ost, c->nb_streams);

        if(vas_chunk_ost[va_sel] != (CDX_U64)-1LL)
        {
            //CDX_LOGD("--- stream seek: %llx", vas_chunk_ost[va_sel]);
            result = CdxStreamSeek(c->fp, vas_chunk_ost[va_sel], SEEK_SET);
            if(result < 0)
            {
                return -1;
            }
        }
    }
    return 0;
}
