#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <SystemBase.h>
#include <utils/plat_log.h>
#include <log/log_wrapper.h>
#include "record_writer.h"
#include "cdx_list.h"
#include "Mp4Muxer.h"
#include "file_repair.h"

#define MINIMAL_FILE_SIZE 1048576
#define META_DATA_POSITION (MINIMAL_FILE_SIZE+16) 
#define SIMPLE_CACHE_SIZE_VFS (32*1024)
#define MAXIMUM_VIDEO_STRM_NUM 6
typedef struct mp4_file_repair_context_s
{
    void *pMuxerCtx;
    CDX_RecordWriter *pWriter;
    MOV_META file_meta_info;
   
    unsigned int max_frms_tag_interval;
    struct list_head frms_tag_list; 
    unsigned int frms_tag_node_num;
}MP4_FILE_REPAIR_CONTEXT;

struct frms_tag_info_s
{
    unsigned int frms_tag_id;
    struct frms_tag_s frms_tag;
};

typedef struct frms_tag_node_s
{
    struct frms_tag_info_s frms_tag_info;
    struct list_head list;
}FRMS_TAG_NODE;
static void mp4_file_vos_buff_rls(MP4_FILE_REPAIR_CONTEXT *context)
{
    MP4_FILE_REPAIR_CONTEXT *p_repair_ctx = context;

    for(int i=0; i<p_repair_ctx->file_meta_info.v_strm_num; ++i)
    {
        if(NULL != p_repair_ctx->file_meta_info.v_strms_meta[i].v_vos_buff)
        {
            free(p_repair_ctx->file_meta_info.v_strms_meta[i].v_vos_buff);
            p_repair_ctx->file_meta_info.v_strms_meta[i].v_vos_buff = NULL;
        }
    }
    for(int i=0; i<p_repair_ctx->file_meta_info.a_strm_num; ++i)
    {
        if(NULL != p_repair_ctx->file_meta_info.a_strms_meta[i].a_vos_buff)
        {
            free(p_repair_ctx->file_meta_info.a_strms_meta[i].a_vos_buff);
            p_repair_ctx->file_meta_info.a_strms_meta[i].a_vos_buff = NULL;
        }
    }

    if(NULL != p_repair_ctx->file_meta_info.t_strms_meta.t_vos_buff)
    {
        free(p_repair_ctx->file_meta_info.t_strms_meta.t_vos_buff);
        p_repair_ctx->file_meta_info.t_strms_meta.t_vos_buff = NULL;
    }

    return;
}

static int mp4_file_meta_data_read(MP4_FILE_REPAIR_CONTEXT *context,int file_fd)
{
    MOV_META *p_file_meta = &context->file_meta_info;
    long long off = 0;
    int stream_num = 0;
    unsigned int tmp_max_frms_tag_interval = 0;
    int ret = 0;

    off = lseek(file_fd,META_DATA_POSITION,SEEK_SET);
    if(off != META_DATA_POSITION)
    {
        aloge("file_repair_position_seek_err,c:%lld,t:%d\n",
                                                    off,META_DATA_POSITION);
        return -1;
    }
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    ret = read(file_fd,&tmp_max_frms_tag_interval,sizeof(tmp_max_frms_tag_interval));
    if(ret != sizeof(tmp_max_frms_tag_interval))
    {
        aloge("file_repair_v_tmp_max_frms_tag_interval_read_err,r:%d,stm_num:%x\n",
                ret,tmp_max_frms_tag_interval);
        return -1;
    }
    tmp_max_frms_tag_interval = (tmp_max_frms_tag_interval&0xff000000)>>24|((tmp_max_frms_tag_interval&0x00ff0000)>>16)<<8| 
                    ((tmp_max_frms_tag_interval&0x0000ff00)>>8)<<16 | (tmp_max_frms_tag_interval&0xff)<<24;
    context->max_frms_tag_interval = tmp_max_frms_tag_interval;
#endif
    ret = read(file_fd,&stream_num,sizeof(stream_num));
    if(ret != sizeof(stream_num))
    {
        aloge("file_repair_v_stream_num_read_err,r:%d,stm_num:%x\n",
                ret,stream_num);
        return -1;
    }
    stream_num = (stream_num&0xff000000)>>24|((stream_num&0x00ff0000)>>16)<<8| 
                    ((stream_num&0x0000ff00)>>8)<<16 | (stream_num&0xff)<<24;
    p_file_meta->v_strm_num = stream_num;
    alogd("file_repair_v_strm_num:%d\n",stream_num); 
    if(stream_num > MAXIMUM_VIDEO_STRM_NUM)// maybe not contain repair info in file 
    {
        aloge("file_repair_invalid_video_stream_num:%d\n",stream_num);
        return -1;
    }

    for(int i=0; i<stream_num; ++i)// video stream exist
    {
        ret = read(file_fd,&p_file_meta->v_strms_meta[i],sizeof(V_STRM_META_DATA)-4);
        if(ret != sizeof(V_STRM_META_DATA)-4)
        {
            aloge("file_repair_v:%d_meta_info_read_err,r:%d,t:%d\n",
                                            i,ret,sizeof(V_STRM_META_DATA)-4);
            return -1;
        }
        alogd("file_repair_v:%d_vos_len:%d\n",i,p_file_meta->v_strms_meta[i].v_vos_len);
        if(0 != p_file_meta->v_strms_meta[i].v_vos_len)
        {
            char *tmp_buff = (char *)malloc(p_file_meta->v_strms_meta[i].v_vos_len);
            if(NULL == tmp_buff)
            {
                aloge("file_repair_v:%d_vos_buff_malloc_fail,s:%d\n",
                                    i,p_file_meta->v_strms_meta[i].v_vos_len);
                mp4_file_vos_buff_rls(context);
                return -1;
            }
            memset(tmp_buff,0,p_file_meta->v_strms_meta[i].v_vos_len);
            ret = read(file_fd,tmp_buff,p_file_meta->v_strms_meta[i].v_vos_len);
            if(ret != p_file_meta->v_strms_meta[i].v_vos_len)
            {
                aloge("file_repair_v:%d_read_vos_fail,r:%d,t:%d\n",
                        i,ret,p_file_meta->v_strms_meta[i].v_vos_len);
                return -1;
            }
            p_file_meta->v_strms_meta[i].v_vos_buff = tmp_buff;
        }
    }

    ret = read(file_fd,&stream_num,sizeof(stream_num));
    if(ret != sizeof(stream_num))
    {
        aloge("file_repair_a_stream_num_read_err,r:%d,stm_num:%x\n",
                ret,stream_num);
        return -1;
    }
    stream_num = (stream_num&0xff000000)>>24|((stream_num&0x00ff0000)>>16)<<8| 
                    ((stream_num&0x0000ff00)>>8)<<16 | (stream_num&0xff)<<24;
    p_file_meta->a_strm_num = stream_num;

    alogd("file_repair_a_strm_num:%d\n",stream_num); 
    for(int i=0; i<stream_num; ++i)// audio stream exist
    {
        ret = read(file_fd,&p_file_meta->a_strms_meta[i],sizeof(A_STRM_META_DATA)-4);
        if(ret != sizeof(A_STRM_META_DATA)-4)
        {
            aloge("file_repair_a:%d_meta_info_read_err,r:%d,t:%d\n",
                                            i,ret,sizeof(A_STRM_META_DATA)-4);
            return -1;
        }
        alogd("file_repair_a:%d_vos_len:%d\n",i,p_file_meta->a_strms_meta[i].a_vos_len);
        if(0 != p_file_meta->a_strms_meta[i].a_vos_len)
        {
            char *tmp_buff = (char *)malloc(p_file_meta->a_strms_meta[i].a_vos_len);
            if(NULL == tmp_buff)
            {
                aloge("file_repair_a:%d_vos_buff_malloc_fail,s:%d\n",
                                    i,p_file_meta->a_strms_meta[i].a_vos_len);

                mp4_file_vos_buff_rls(context);
                return -1;
            }
            memset(tmp_buff,0,p_file_meta->a_strms_meta[i].a_vos_len);
            ret = read(file_fd,tmp_buff,p_file_meta->a_strms_meta[i].a_vos_len);
            if(ret != p_file_meta->a_strms_meta[i].a_vos_len)
            {
                aloge("file_repair_a:%d_read_vos_fail,r:%d,t:%d\n",
                        i,ret,p_file_meta->a_strms_meta[i].a_vos_len);
                return -1;
            }
            p_file_meta->a_strms_meta[i].a_vos_buff = tmp_buff;
        }
    }

    ret = read(file_fd,&stream_num,sizeof(stream_num));
    if(ret != sizeof(stream_num))
    {
        aloge("file_repair_t_stream_num_read_err,r:%d,stm_num:%x\n",
                ret,stream_num);
        return -1;
    }
    stream_num = (stream_num&0xff000000)>>24|((stream_num&0x00ff0000)>>16)<<8| 
                    ((stream_num&0x0000ff00)>>8)<<16 | (stream_num&0xff)<<24;
    p_file_meta->t_strm_num = stream_num;

    alogd("file_repair_t_strm_num:%d\n",stream_num); 
    if(0<stream_num)// text stream exist
    {
        ret = read(file_fd,&p_file_meta->t_strms_meta,sizeof(T_STRM_META_DATA)-4);
        if(ret != sizeof(T_STRM_META_DATA)-4)
        {
            aloge("file_repair_t_meta_info_read_err,r:%d,t:%d\n",
                                            ret,sizeof(T_STRM_META_DATA)-4);
            return -1;
        }
    
        alogw("file_repair_t_vos_len:%d\n",p_file_meta->t_strms_meta.t_vos_len);
        if(0 != p_file_meta->t_strms_meta.t_vos_len)
        {
            char *tmp_buff = (char *)malloc(p_file_meta->t_strms_meta.t_vos_len);
            if(NULL == tmp_buff)
            {
                aloge("file_repair_t_vos_buff_malloc_fail,s:%d\n",
                                    p_file_meta->t_strms_meta.t_vos_len);
                mp4_file_vos_buff_rls(context);
                return -1;
            }
            memset(tmp_buff,0,p_file_meta->t_strms_meta.t_vos_len);
            ret = read(file_fd,tmp_buff,p_file_meta->t_strms_meta.t_vos_len);
            if(ret != p_file_meta->t_strms_meta.t_vos_len)
            {
                aloge("file_repair_t_read_vos_fail,r:%d,t:%d\n",
                        ret,p_file_meta->t_strms_meta.t_vos_len);
                return -1;
            }
            p_file_meta->t_strms_meta.t_vos_buff = tmp_buff;
        }
    }

    return 0;
}

static int mp4_file_frm_hdr_read(MP4_FILE_REPAIR_CONTEXT *context,int file_fd,
                                                        MOV_FRM_HDR *p_frm_hdr)
{
    int tmp_ret = 0;
    if(NULL == p_frm_hdr) 
    {
        aloge("mp4_repair_invalid_param!!\n");
        return -1;
    }

    memset(p_frm_hdr,0,sizeof(MOV_FRM_HDR));
    tmp_ret = read(file_fd,p_frm_hdr,sizeof(MOV_FRM_HDR));
    if(tmp_ret != sizeof(MOV_FRM_HDR))
    {
        aloge("file_repair_read_frm_hdr_fail,r:%d,t:%d\n",
                                            tmp_ret,sizeof(MOV_FRM_HDR));
        return -1;
    }

    return 0;
}

static int mp4_file_muxer_init(MP4_FILE_REPAIR_CONTEXT *context,int file_fd)
{
    int tmp_ret = 0;
    MOV_META *p_file_meta = &context->file_meta_info;
    _media_file_inf_t media_info;

    if(NULL == context->pWriter)
    {
        context->pWriter = cedarx_record_writer_create(MUXER_MODE_MP4);
        if(NULL == context->pWriter)
        {
            aloge("file_repair_cedarx_record_writer_create_failed!!\n");
            return -1;
        }

        context->pMuxerCtx = context->pWriter->MuxerOpen((int*)&tmp_ret);
        if(0 != tmp_ret)
        {
            aloge("file_repair_muxer_open_failed!!\n");
            cedarx_record_writer_destroy(context->pWriter);
            return -1;
        }

        context->pWriter->MuxerIoctrl(context->pMuxerCtx, SETFALLOCATELEN, 
                                                                    0, NULL);
        tmp_ret = context->pWriter->MuxerIoctrl(context->pMuxerCtx,SETCACHEFD2,
                                                (unsigned int)file_fd, NULL); 
        if( tmp_ret != 0)
        {
            aloge("file_repair_muxer_set_output_fd_failed!!\n");
            context->pWriter->MuxerClose(context->pMuxerCtx);
            cedarx_record_writer_destroy(context->pWriter);
            return -1;
        }

        context->pWriter->MuxerIoctrl(context->pMuxerCtx, SET_FS_WRITE_MODE,
                                            FSWRITEMODE_SIMPLECACHE, NULL);
        context->pWriter->MuxerIoctrl(context->pMuxerCtx, SET_FS_SIMPLE_CACHE_SIZE,
                                            SIMPLE_CACHE_SIZE_VFS, NULL);
    }

    context->pWriter->MuxerIoctrl(context->pMuxerCtx,SET_FILE_REPAIR_FLAG,1,NULL);
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    context->pWriter->MuxerIoctrl(context->pMuxerCtx,SET_FILE_REPAIR_INTERVAL,context->max_frms_tag_interval,NULL);
#endif

    memset(&media_info,0,sizeof(_media_file_inf_t));
    media_info.mVideoInfoValidNum = p_file_meta->v_strm_num;
    for(int i=0; i<p_file_meta->v_strm_num; ++i)
    {
        MediaVideoInfo *p_dst = &media_info.mMediaVideoInfo[i];
        V_STRM_META_DATA *p_src = &p_file_meta->v_strms_meta[i];
        p_dst->mVideoEncodeType = p_src->v_enc_type;
        p_dst->uVideoFrmRate = p_src->v_frm_rate;
        p_dst->nWidth = p_src->v_frm_width;
        p_dst->nHeight = p_src->v_frm_height;
        p_dst->rotate_degree = p_src->v_frm_rotate_degree;
        p_dst->maxKeyInterval = p_src->v_frm_max_key_frm_interval;
        p_dst->create_time = p_src->v_create_time;
    }
    
    if(0 != p_file_meta->a_strm_num)
    {
        media_info.audio_encode_type = p_file_meta->a_strms_meta[0].a_enc_type;
        media_info.sample_rate = p_file_meta->a_strms_meta[0].a_sample_rate;
        media_info.channels = p_file_meta->a_strms_meta[0].a_chn_num;
        media_info.bits_per_sample = p_file_meta->a_strms_meta[0].a_bit_depth;
        media_info.frame_size = p_file_meta->a_strms_meta[0].a_frm_size;
    }

    if(0 != p_file_meta->t_strm_num)
    {
        media_info.geo_available = 1;
    }
    tmp_ret = context->pWriter->MuxerIoctrl(context->pMuxerCtx,SETAVPARA,0,
                                        (void*)(&media_info));
    if(0 != tmp_ret)
    {
        aloge("file_repair_muxer_set_meta_info_failed!!\n");
        context->pWriter->MuxerClose(context->pMuxerCtx);
        cedarx_record_writer_destroy(context->pWriter);
        return -1;
    }
    // set video vos 
    for(int i=0; i<p_file_meta->v_strm_num; ++i)
    {
        if(0 != p_file_meta->v_strms_meta[i].v_vos_len)
        {
            context->pWriter->MuxerWriteExtraData(context->pMuxerCtx, 
                    (unsigned char *)(p_file_meta->v_strms_meta[i].v_vos_buff),
                                    p_file_meta->v_strms_meta[i].v_vos_len, i);
        }
    }

    // set audio vos 
    if(0 != p_file_meta->a_strm_num && 0 != p_file_meta->a_strms_meta[0].a_vos_len)
    {
        context->pWriter->MuxerWriteExtraData(context->pMuxerCtx, 
                (unsigned char *)(p_file_meta->a_strms_meta[0].a_vos_buff),
                                p_file_meta->a_strms_meta[0].a_vos_len, 
                                p_file_meta->v_strm_num);
    }
    // set text vos
    if(0 != p_file_meta->t_strm_num && 0 != p_file_meta->t_strms_meta.t_vos_len)
    {
        context->pWriter->MuxerWriteExtraData(context->pMuxerCtx, 
                (unsigned char *)(p_file_meta->t_strms_meta.t_vos_buff),
                                p_file_meta->t_strms_meta.t_vos_len, 
                                p_file_meta->v_strm_num+1);
    }
    tmp_ret = context->pWriter->MuxerWriteHeader(context->pMuxerCtx);
    if(0 > tmp_ret)
    {
        aloge("file_repair_wrt_hdr_fail!\n");
        context->pWriter->MuxerClose(context->pMuxerCtx);
        cedarx_record_writer_destroy(context->pWriter);
        return -1;
    }
    return 0;
}
static void mp4_file_muxer_rls(MP4_FILE_REPAIR_CONTEXT *context)
{
    if(NULL != context->pMuxerCtx)
    {
        context->pWriter->MuxerClose(context->pMuxerCtx);
        context->pMuxerCtx = NULL;
    }

    if(NULL != context->pWriter)
    {
        cedarx_record_writer_destroy(context->pWriter);
        context->pWriter = NULL;
    }
}

static int mp4_file_format_chk(int fd)
{
    unsigned int size = 0;
    char atom_type[5] = {0};
    int ret = 0; 

    ret = read(fd,&size,sizeof(int));
    if(ret != sizeof(int))
    {
        aloge("file_repair_read_ftyp_atom_size_err,r:%d,t:%d\n",ret,sizeof(int));
        return -1;
    }

    ret = read(fd,atom_type,4);
    if(ret != 4)
    {
        aloge("file_repair_read_ftyp_atom_type_err,r:%d\n",ret);
        return -1;
    }
    atom_type[4] = '\0';
    if(strcmp(atom_type,"ftyp") != 0)
    {
        aloge("file_repair_file_type_may_be_err,type:%s\n",atom_type);
        return -1;
    }

    return 0;
}

static int mp4_file_need_repair_chk(int file_fd,long long file_len)
{
    // unsigned int ending_maker[4] = {0xAABBAABB,0xCCDDCCDD,0xEEFFEEFF,0xABCDEFAB };
    // unsigned int read_ending_maker[4] = {0};
    unsigned int atom_size = 0;
    char atom_type[5] = {0};
    int tmp_ret = 0;
    // use encing marker to validate integrality of file
    /* tmp_ret = lseek(file_fd,-16,SEEK_CUR);
    if(-1 == tmp_ret)
    {
        aloge("file_repair_integrity_chk_seek_fail,r:%d\n",tmp_ret);
    }

    tmp_ret = read(file_fd,read_ending_maker,16);
    if(16 != tmp_ret)
    {
        aloge("file_repair_ending_marker_read_fail,r:%d\n",tmp_ret);
    }
   
    if((read_ending_maker[0] == ending_maker[0]) &&
            (read_ending_maker[1] == ending_maker[1]) && 
            (read_ending_maker[2] == ending_maker[2]) &&
            (read_ending_maker[3] == ending_maker[3]))
    {
        return 0;   // do not need
    }
    else
    {
        return 1;   // need to be repaired
    } */

    // better way to check the existence of ending maker atom
    
    tmp_ret = lseek(file_fd,0,SEEK_SET);
    if(-1 == tmp_ret)
    {
        aloge("file_repair_integrity_chk_seek_fail,r:%d\n",tmp_ret);
    } 

    while(1)
    {
        tmp_ret = read(file_fd,&atom_size,sizeof(unsigned int)); 
        if(tmp_ret != sizeof(unsigned int))
        {
            aloge("file_repair_rd_atom_size_err,r:%d\n",tmp_ret);
            return 0;   // not to repair
        }
        atom_size = (atom_size&0xff000000)>>24|((atom_size&0x00ff0000)>>16)<<8| 
                    ((atom_size&0x0000ff00)>>8)<<16 | (atom_size&0xff)<<24;

        tmp_ret = read(file_fd,atom_type,4); 
        if(tmp_ret != 4)
        {
            aloge("file_repair_rd_atom_type_err,r:%d\n",tmp_ret);
        }
        atom_type[4] = '\0';
        
        // improve the checking,just checkt the size field of mdat atom
        if(!strcmp(atom_type,"mdat")) // search skip atom
        {
            if(atom_size != 0)
            {
                return 0; // do not need
            }
            else
            {
                return 1; // do need
            }
        }
        else
        {

            unsigned int cur_off = lseek(file_fd,0,SEEK_CUR);
            if(cur_off+atom_size-8 >= file_len)
            {
                aloge("file_repair_not_found_ending_marker,o:%d,s:%d,f:%d\n",
                                                            cur_off,atom_size,file_len);
                return 1; 
            }
            else
            {
                tmp_ret = lseek(file_fd,atom_size-8,SEEK_CUR);
                if(-1 == tmp_ret)
                {
                    aloge("file_repair_jump_atom_err,r:%d,t:%d\n",tmp_ret,atom_size-8);
                    return 0;
                }
            }
        }

        /* if(!strcmp(atom_type,"skip")) // search skip atom
        {
            tmp_ret = read(file_fd,read_ending_maker,16); // check further
            if(16 != tmp_ret)
            {
                aloge("file_repair_ending_marker_read_fail,r:%d\n",tmp_ret);
            }

            if((read_ending_maker[0] == ending_maker[0]) &&
                    (read_ending_maker[1] == ending_maker[1]) && 
                    (read_ending_maker[2] == ending_maker[2]) &&
                    (read_ending_maker[3] == ending_maker[3]))
            {
                return 0;   // do not need
            }
            else
            {
                return 1;   // need to be repaired
            }
        }
        else
        {
            if(!strcmp(atom_type,"mdat") && atom_size == 0)
            {
                aloge("file_repair_0_repair\n");
                return 1;   // need to be repaired
            }
            unsigned int cur_off = lseek(file_fd,0,SEEK_CUR);
            if(cur_off+atom_size-8 >= file_len)
            {
                aloge("file_repair_not_found_ending_marker,o:%d,s:%d,f:%d\n",
                                                            cur_off,atom_size,file_len);
                return 1;   // do not find skip atom,need to
            }
            else
            {
                tmp_ret = lseek(file_fd,atom_size-8,SEEK_CUR);
                if(-1 == tmp_ret)
                {
                    aloge("file_repair_jump_atom_err,r:%d,t:%d\n",tmp_ret,atom_size-8);
                    return 0;
                }
            }
        } */
    }
}

#define DATA_READ_CHUNK_SIZE (8192L) 
#define TAG_FOR_FRMS_TAG_FLAG 0xcdcdcdcd
#define FRMS_TAG_FLAG_FIRST 0xabababab
#define FRMS_TAG_FLAG_NORMAL 0xbabababa

struct search_flag_rst_s
{
    unsigned int tag_for_frms_tag_flag_located;
    unsigned int tag_for_frms_tag_pos_field_valid;
    unsigned int tag_for_frms_tag_last_pos_field;
    unsigned int tag_for_frms_tag_located_cnt;

    unsigned int first_frms_tag_flag_located;

    unsigned int frms_tag_flag_located;
    unsigned int frms_tag_size_field_valid;
    unsigned int frms_tag_size_field;
    unsigned int frms_tag_last_pos_field_valid;
    unsigned int frms_tag_last_pos_field;
    unsigned int frms_tag_cur_pos_field_valid;
    unsigned int frms_tag_cur_pos_field;
    unsigned int frms_tag_frm_hdrs_num_field_valid;
    unsigned int frms_tag_frm_hdrs_num_field;
    unsigned int frms_tag_frm_hdrs_field_valid;
    MOV_FRM_HDR *p_frms_tag_frm_hdrs;

    unsigned int frms_tag_located_cnt;
    unsigned int cur_buff_off;

};
static int mp4_file_search_flag(char *p_buff,struct search_flag_rst_s *p_rst)
{
    unsigned int chking_flag = 0;
    unsigned int tag_for_frms_tag_located = 0;
    unsigned int tag_for_frms_tag_located_cnt = 0;
    unsigned int tag_for_frms_tag_pos_field = 0;
    char *p_buff_start = p_buff;
    char *p_buff_end = p_buff + DATA_READ_CHUNK_SIZE;

    while(p_buff < p_buff_end)
    {
        chking_flag >>= 8;
        chking_flag |= *p_buff << 24;
        p_buff ++;
        if(chking_flag == TAG_FOR_FRMS_TAG_FLAG)
        {
            p_rst->tag_for_frms_tag_flag_located = 1;
            if(p_buff+4 < p_buff_end)   // pos field valid, but still need to check further for frms_tag exist or not
            {
                tag_for_frms_tag_pos_field |= *(p_buff);
                tag_for_frms_tag_pos_field |= *(p_buff+1)<<8;
                tag_for_frms_tag_pos_field |= *(p_buff+2)<<16;
                tag_for_frms_tag_pos_field |= *(p_buff+3)<<24;

                p_rst->tag_for_frms_tag_pos_field_valid = 1;
                p_rst->tag_for_frms_tag_last_pos_field = tag_for_frms_tag_pos_field;
                p_rst->tag_for_frms_tag_located_cnt ++;
                tag_for_frms_tag_located = 1;
                tag_for_frms_tag_located_cnt ++;
            }
            else        // pos field invalid
            {
                p_rst->tag_for_frms_tag_pos_field_valid = 0;
                p_rst->cur_buff_off =(unsigned int) (p_buff - p_buff_start);    // data size consumed
                alogd("mp4_file_repair_tag_for_frms_tag_data_not_enought_break");
                break;
            }
        }
        else if(chking_flag == FRMS_TAG_FLAG_NORMAL || chking_flag == FRMS_TAG_FLAG_FIRST)    // located one frms_tag in current search chunk(no more than one)
        {
            alogd("mp4_file_repair_located_one_frms_tag");
            if(chking_flag == FRMS_TAG_FLAG_NORMAL)
            {
                p_rst->frms_tag_flag_located = 1;
            }
            else if(chking_flag == FRMS_TAG_FLAG_FIRST)
            {
                p_rst->first_frms_tag_flag_located = 1;
            }
            char *tmp_p_buff = NULL;
            if(p_buff+4 < p_buff_end)   // frms_tag size field valid
            {
                p_rst->frms_tag_size_field_valid = 1;
                p_rst->frms_tag_size_field |= *p_buff;
                p_rst->frms_tag_size_field |= *(p_buff+1)<<8;
                p_rst->frms_tag_size_field |= *(p_buff+2)<<16;
                p_rst->frms_tag_size_field |= *(p_buff+3)<<24;
            }
            else
            {
                p_rst->frms_tag_size_field_valid = 0;
                p_rst->cur_buff_off =(unsigned int) (p_buff - p_buff_start);    // data size consumed

                alogd("mp4_file_repair_frms_tag_size_data_not_enought_break");
                break;
            }
            tmp_p_buff = p_buff+4;      // skip size field
            if(tmp_p_buff+4 < p_buff_end)   // frms_tag last_pos field valid
            {
                p_rst->frms_tag_last_pos_field_valid = 1;
                p_rst->frms_tag_last_pos_field |= *tmp_p_buff;
                p_rst->frms_tag_last_pos_field |= *(tmp_p_buff+1)<<8;
                p_rst->frms_tag_last_pos_field |= *(tmp_p_buff+2)<<16;
                p_rst->frms_tag_last_pos_field |= *(tmp_p_buff+3)<<24;
            }
            else
            {
                p_rst->frms_tag_last_pos_field_valid = 0;
                p_rst->cur_buff_off =(unsigned int) (p_buff - p_buff_start);    // data size consumed
                alogd("mp4_file_repair_frms_tag_pos_data_not_enought_break");
                break;
            }
            tmp_p_buff += 4;            // skip last pos field 
            if(tmp_p_buff+4 < p_buff_end)   // frms_tag cur_pos field valid
            {
                p_rst->frms_tag_cur_pos_field_valid = 1;
                p_rst->frms_tag_cur_pos_field |= *tmp_p_buff;
                p_rst->frms_tag_cur_pos_field |= *(tmp_p_buff+1)<<8;
                p_rst->frms_tag_cur_pos_field |= *(tmp_p_buff+2)<<16;
                p_rst->frms_tag_cur_pos_field |= *(tmp_p_buff+3)<<24;
            }
            else
            {
                p_rst->frms_tag_cur_pos_field_valid = 0;
                p_rst->cur_buff_off =(unsigned int) (p_buff - p_buff_start);    // data size consumed
                alogd("mp4_file_repair_frms_tag_cur_pos_data_not_enought_break");
                break;
            }
            
            tmp_p_buff += 4;            // skip cur_pos field
            if(tmp_p_buff+4 < p_buff_end)   // frms_tag num field valid
            {
                p_rst->frms_tag_frm_hdrs_num_field_valid = 1;
                p_rst->frms_tag_frm_hdrs_num_field |= *tmp_p_buff;
                p_rst->frms_tag_frm_hdrs_num_field |= *(tmp_p_buff+1)<<8;
                p_rst->frms_tag_frm_hdrs_num_field |= *(tmp_p_buff+2)<<16;
                p_rst->frms_tag_frm_hdrs_num_field |= *(tmp_p_buff+3)<<24;
            }
            else
            {
                p_rst->frms_tag_frm_hdrs_num_field_valid = 0;
                p_rst->cur_buff_off =(unsigned int) (p_buff - p_buff_start);    // data size consumed
                alogd("mp4_file_repair_frms_tag_num_data_not_enought_break");
                break;
            }
            tmp_p_buff += 4;        // skip frm_hdrs_num field
            
            if(tmp_p_buff+p_rst->frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR) < p_buff_end)
            {
                p_rst->frms_tag_frm_hdrs_field_valid = 1;
                if(NULL == p_rst->p_frms_tag_frm_hdrs)
                {
                    p_rst->p_frms_tag_frm_hdrs = (MOV_FRM_HDR*)malloc(p_rst->frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR));
                    if(NULL == p_rst->p_frms_tag_frm_hdrs)
                    {
                        aloge("mp4_file_repair_frms_tag_frms_hdr_buff_malloc_failed:%d",p_rst->frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR));
                        return -1;      // fatal error, return -1
                    }
                }
                memcpy((char *)p_rst->p_frms_tag_frm_hdrs,tmp_p_buff,p_rst->frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR));
                if(1 == tag_for_frms_tag_located)
                {
                    if(p_rst->frms_tag_last_pos_field != p_rst->tag_for_frms_tag_last_pos_field)
                    {
                        alogw("mp4_file_repair_frms_tag_last_pos_not_match_with_previous_tag_for_frms_tag:%d-%d",
                                p_rst->frms_tag_last_pos_field,p_rst->tag_for_frms_tag_last_pos_field);
                    }
                    alogd("mp4_file_the_num_of_tag_for_frms_tag_before_frms_tag_located,%d",tag_for_frms_tag_located_cnt);
                }
                if(chking_flag == FRMS_TAG_FLAG_NORMAL)
                {
                    p_rst->frms_tag_located_cnt ++;
                }
                p_rst->cur_buff_off =(unsigned int) (p_buff - p_buff_start);    // data size consumed
                tmp_p_buff += p_rst->frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR);
                p_buff = tmp_p_buff;
                // break;          // located one frms_tag, then stop the matching
                continue;           // located one frms_tag, but continue to check 
            }
            else
            {
                p_rst->frms_tag_frm_hdrs_field_valid = 0;       // maybe check furthur, to calculate the acutal number of frm_hdr
                p_rst->p_frms_tag_frm_hdrs = NULL;
                p_rst->cur_buff_off =(unsigned int) (p_buff - p_buff_start);    // data size consumed
                alogd("mp4_file_repair_frms_tag_frm_hdrs_data_not_enought_break");
                break;              // left data is not enought to continue the parsing,then end
            }
        }
    }

    return 0;
}

int mp4_file_repair_frms_tag_parse(MP4_FILE_REPAIR_CONTEXT *context,int file_fd)
{
    long long ret = 0;
    long long off = 0;
    long long file_len = 0;
    char *tmp_buff = NULL;
    unsigned int chunk_num = 1;
    long long target_seek_offset = 0;
    struct search_flag_rst_s search_rst;
    unsigned int start_from_tag_for_frms_tag = 0;

    INIT_LIST_HEAD(&context->frms_tag_list);

    tmp_buff = (char *)malloc(DATA_READ_CHUNK_SIZE);
    if(NULL == tmp_buff)
    {
        aloge("mp4_file_repair_malloc_4k_buffer_failed");
        return -1;
    }
    memset(tmp_buff,0,DATA_READ_CHUNK_SIZE);

    off = lseek(file_fd,0,SEEK_END);
    if(-1 == off)
    {
        aloge("mp4_file_repair_seek_to_file_end_failed");
        if (NULL != tmp_buff)
        {
            free(tmp_buff);
            tmp_buff = NULL;
        }
        return -1;
    }
    file_len = off;
    target_seek_offset = DATA_READ_CHUNK_SIZE*chunk_num;
    for(;;)
    {
        off = lseek(file_fd,-target_seek_offset,SEEK_END);
        if(-1 == off)
        {
            aloge("mp4_file_repair_seek_failed:%lld",file_len-DATA_READ_CHUNK_SIZE);
            if(NULL != tmp_buff)
            {
                free(tmp_buff);
            }
            return -1;
        }

        ret = read(file_fd,tmp_buff,DATA_READ_CHUNK_SIZE);
        if(ret != DATA_READ_CHUNK_SIZE)
        {
            aloge("mp4_file_repair_read_data_failed:%lld-%lld",ret,DATA_READ_CHUNK_SIZE);
        }
        // to search target flag
        memset(&search_rst,0,sizeof(struct search_flag_rst_s));
        ret = mp4_file_search_flag(tmp_buff,&search_rst); 
        if(0 == ret)
        {
            // check if first frms_tag located
            alogd("file_repair_search_rst:%d-%d-%d-%u-%u-%lld-%u-%lld",
                    search_rst.first_frms_tag_flag_located,search_rst.frms_tag_flag_located,search_rst.tag_for_frms_tag_flag_located,
                    search_rst.frms_tag_located_cnt, chunk_num,off,search_rst.cur_buff_off,file_len);
            if(1 == search_rst.frms_tag_flag_located) // check if frms_tag located
            {
                if(0 == search_rst.frms_tag_frm_hdrs_field_valid)   
                {
                    if(chunk_num == 1)
                    {
                        chunk_num ++; // need one more chunk to be concatenated with current chunk 
                        target_seek_offset = DATA_READ_CHUNK_SIZE*chunk_num;
                        alogd("mp4_file_repair_frms_tag_located_in_first_chunk_but_data_not_enough");
                    }
                    else
                    {
                        if(1 == search_rst.frms_tag_frm_hdrs_num_field_valid)
                        {
                            target_seek_offset = DATA_READ_CHUNK_SIZE*chunk_num - 
                                search_rst.frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR) - FRMS_TAG_INFO_HDR_SIZE; 
                            alogd("mp4_file_repair_frms_tag_located_in_%d_chunk_but_data_not_enough",chunk_num);
                                    //sizeof(frm_hdr_num)+sizeof(frms_tag_cur_pos)+sizeof(frms_tag_pos)+sizeof(frms_tag_size)+sizeof(frms_tag_flag)
                            if(target_seek_offset <= 0)
                            {
                                aloge("mp4_fiel_repair_seek_pos_offset_error,%lld-%u",target_seek_offset,search_rst.frms_tag_frm_hdrs_num_field);
                            }
                        }
                        else
                        {
                            alogd("mp4_file_repair_frms_tag_located_in_%d_chunk_but_data_not_enough2",chunk_num);
                            target_seek_offset = DATA_READ_CHUNK_SIZE*chunk_num - DATA_READ_CHUNK_SIZE/3;   // 1/3 not accurate value
                        }
                    }
                    continue;       // for next chunk check
                }
                else
                {
                    struct frms_tag_node_s *p_frms_tag_node = (struct frms_tag_node_s *)malloc(sizeof(struct  frms_tag_node_s));
                    if(NULL != p_frms_tag_node)
                    {
                        memset(p_frms_tag_node,0,sizeof(struct frms_tag_node_s));
                        p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_flag = 0xbabababa;
                        p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_size = search_rst.frms_tag_size_field;
                        p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_pos = search_rst.frms_tag_last_pos_field;
                        p_frms_tag_node->frms_tag_info.frms_tag.cur_frms_tag_pos = search_rst.frms_tag_cur_pos_field;
                        p_frms_tag_node->frms_tag_info.frms_tag.frms_num = search_rst.frms_tag_frm_hdrs_num_field;
                        if(NULL == p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                        {
                            p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs = (MOV_FRM_HDR *)malloc(search_rst.frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR));
                            if(NULL == p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                            {
                                aloge("mp4_file_repair_malloc_frm_hdrs_failed,%d",search_rst.frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR));
                                if(NULL != p_frms_tag_node)
                                {
                                    free(p_frms_tag_node);
                                    p_frms_tag_node = NULL;
                                }
                                if(NULL != search_rst.p_frms_tag_frm_hdrs)
                                {
                                    free(search_rst.p_frms_tag_frm_hdrs);
                                    search_rst.p_frms_tag_frm_hdrs = NULL;
                                }
                                if(NULL != tmp_buff)
                                {
                                    free(tmp_buff);
                                    tmp_buff = NULL;
                                }
                                return -1;
                            }
                        }
                        memcpy((char *)p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs,(char *)search_rst.p_frms_tag_frm_hdrs,
                                search_rst.frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR));
                        if(NULL != search_rst.p_frms_tag_frm_hdrs)
                        {
                            free(search_rst.p_frms_tag_frm_hdrs);
                            search_rst.p_frms_tag_frm_hdrs = NULL;
                        }
                        p_frms_tag_node->frms_tag_info.frms_tag_id ++;
                        list_add(&p_frms_tag_node->list,&context->frms_tag_list);
                        context->frms_tag_node_num ++;
                        alogd("mp4_file_repair_frms_tag_break_search_and_chk");
                        break;      // locate one frms_tag, then break out the search and check loop
                    }
                    else
                    {
                        aloge("mp4_file_repair_malloc_one_frms_tag_node_failed2,%u",sizeof(struct  frms_tag_node_s));
                        if(NULL != search_rst.p_frms_tag_frm_hdrs)
                        {
                            free(search_rst.p_frms_tag_frm_hdrs);
                            search_rst.p_frms_tag_frm_hdrs = NULL;
                        }
                        if(NULL != tmp_buff)
                        {
                            free(tmp_buff);
                            tmp_buff = NULL;
                        }
                        return -1;
                    }
                }
            }
            else if(1 == search_rst.first_frms_tag_flag_located)
            {
                if(0 == search_rst.frms_tag_frm_hdrs_field_valid)
                {
                    if(chunk_num == 1)      // file data is not enought to repair
                    {
                        context->frms_tag_node_num = 0;
                        alogw("mp4_file_repair_located_first_frms_tag,but invalid,too less data remained in file!");
                        if(NULL != tmp_buff)
                        {
                            free(tmp_buff);
                            tmp_buff = NULL;
                        }
                        return -1;
                    }
                    else   // adjust seek position and search next data chunk
                    {
                        if(1 == search_rst.frms_tag_frm_hdrs_num_field_valid)
                        {
                            target_seek_offset = DATA_READ_CHUNK_SIZE*chunk_num - 
                                search_rst.frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR) - FRMS_TAG_INFO_HDR_SIZE; 
                                    //sizeof(frm_hdr_num)+sizeof(frms_tag_cur_pos)+sizeof(frms_tag_pos)+sizeof(frms_tag_size)+sizeof(frms_tag_flag)
                            
                            alogw("mp4_file_repair_located_first_frms_tag,but invalid,concatenate next chunk1!");
                            if(target_seek_offset <= 0)
                            {
                                aloge("mp4_fiel_repair_seek_pos_offset_error,%lld-%u",target_seek_offset,search_rst.frms_tag_frm_hdrs_num_field);
                            }
                        }
                        else
                        {
                            alogw("mp4_file_repair_located_first_frms_tag,but invalid,concatenate next chunk2!");
                            target_seek_offset = DATA_READ_CHUNK_SIZE*chunk_num - DATA_READ_CHUNK_SIZE/3;
                        }

                        continue;       // for next chunk check
                    }
                }
                else
                {
                    struct frms_tag_node_s *p_frms_tag_node = (struct frms_tag_node_s *)malloc(sizeof(struct  frms_tag_node_s));
                    if(NULL != p_frms_tag_node)
                    {
                        memset(p_frms_tag_node,0,sizeof(struct frms_tag_node_s));
                        p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_flag = 0xabababab;
                        p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_size = search_rst.frms_tag_size_field;
                        p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_pos = search_rst.frms_tag_last_pos_field;
                        p_frms_tag_node->frms_tag_info.frms_tag.cur_frms_tag_pos = search_rst.frms_tag_cur_pos_field;
                        p_frms_tag_node->frms_tag_info.frms_tag.frms_num = search_rst.frms_tag_frm_hdrs_num_field;
                        if(NULL == p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                        {
                            p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs = (MOV_FRM_HDR *)malloc(search_rst.frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR));
                            if(NULL == p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                            {
                                aloge("mp4_file_repair_malloc_frm_hdrs_failed,%d",search_rst.frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR));
                                if(NULL != p_frms_tag_node)
                                {
                                    free(p_frms_tag_node);
                                    p_frms_tag_node = NULL;
                                }
                                if(NULL != search_rst.p_frms_tag_frm_hdrs)
                                {
                                    free(search_rst.p_frms_tag_frm_hdrs);
                                    search_rst.p_frms_tag_frm_hdrs = NULL;
                                }
                                if(NULL != tmp_buff)
                                {
                                    free(tmp_buff);
                                    tmp_buff = NULL;
                                }
                                return -1;
                            }
                        }
                        memcpy((char *)p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs,(char *)search_rst.p_frms_tag_frm_hdrs,
                                search_rst.frms_tag_frm_hdrs_num_field*sizeof(MOV_FRM_HDR));

                        if(NULL != search_rst.p_frms_tag_frm_hdrs)
                        {
                            free(search_rst.p_frms_tag_frm_hdrs);
                            search_rst.p_frms_tag_frm_hdrs = NULL;
                        }
                        p_frms_tag_node->frms_tag_info.frms_tag_id ++;
                        list_add(&p_frms_tag_node->list,&context->frms_tag_list);
                        context->frms_tag_node_num ++;
                        alogd("mp4_file_repair_first_frms_tag_break_search_and_chk");
                        break;      // only one frms_tag located, break out the search and check loop
                    }
                    else
                    {
                        aloge("mp4_file_repair_malloc_one_frms_tag_node_failed,%u",sizeof(struct  frms_tag_node_s));
                        if(NULL != search_rst.p_frms_tag_frm_hdrs)
                        {
                            free(search_rst.p_frms_tag_frm_hdrs);
                            search_rst.p_frms_tag_frm_hdrs = NULL;
                        }
                        if(NULL != tmp_buff)
                        {
                            free(tmp_buff);
                            tmp_buff = NULL;
                        }
                        return -1;
                    }
                }
            }
            else if(1 == search_rst.tag_for_frms_tag_flag_located) // check if tag_for_frms_tag located
            {
                if(0 == search_rst.tag_for_frms_tag_pos_field_valid) // data is not enought for pos_field of tag_for_frms_tag 
                {
                    if(search_rst.tag_for_frms_tag_located_cnt > 0)  // tag_for_frms_tag has been located before
                    {
                        if(search_rst.tag_for_frms_tag_last_pos_field > 0) // usefull tag_for_frms_tag
                        {
                            start_from_tag_for_frms_tag = 1;
                            alogd("mp4_file_repair_tag_for_frms_tag_break_search_and_chk,%u-%u",search_rst.tag_for_frms_tag_located_cnt,search_rst.tag_for_frms_tag_last_pos_field);
                            break;
                        }
                        else                    // not useful tag_for_frms_tag
                        {
                            if(NULL != tmp_buff)
                            {
                                free(tmp_buff);
                                tmp_buff = NULL;
                            }
                            alogw("mp4_file_repair_tag_for_frms_tag_useless_return");
                            return -1;
                        }
                    }
                    else
                    {
                        if(chunk_num == 1)          // data remain is not enought
                        {
                            if(NULL != tmp_buff)
                            {
                                free(tmp_buff);
                                tmp_buff = NULL;
                            }
                            alogw("mp4_file_repair_tag_for_frms_tag_useless_return2");
                            return -1;
                        }
                        else
                        {
                            target_seek_offset = DATA_READ_CHUNK_SIZE*chunk_num -8;// sizeof(tag_for_frms_tag.flag)+sizeof(tag_for_frms_tag.pos)
                            continue;
                        }
                    }
                }
                else
                {
                    if(search_rst.tag_for_frms_tag_last_pos_field == 0) // check data valid or not         
                    {
                        if(NULL != tmp_buff)
                        {
                            free(tmp_buff);
                            tmp_buff = NULL;
                        }
                        alogw("mp4_file_repair_tag_for_frms_tag_useless_return3");
                        return -1;
                    }
                    else
                    {
                        start_from_tag_for_frms_tag = 1;
                        alogd("mp4_file_repair_tag_for_frms_tag_break_search_and_chk2,%u-%u",search_rst.tag_for_frms_tag_located_cnt,search_rst.tag_for_frms_tag_last_pos_field);
                        break;      // locate one tag_for_frms_tag,then break search and check loop
                    }
                }
            }
            else
            {
                chunk_num ++;
                target_seek_offset = DATA_READ_CHUNK_SIZE*chunk_num;
            }
        }
        else
        {
            if(NULL != tmp_buff)
            {
                free(tmp_buff);
                tmp_buff = NULL;
            }
            aloge("mp4_file_repair_no_more_resource_repair_failed");
            return -1;      // the result of mp4_file_search_flag is fatal error,such as memory malloc failed
        }
    }

    if(NULL != tmp_buff)
    {
        free(tmp_buff);
        tmp_buff = NULL;
    }
    unsigned int next_frms_tag_pos = 0;
    // check and process the result of location
    if(1 == start_from_tag_for_frms_tag)    // located tag_for_frms_tag
    {
        next_frms_tag_pos = search_rst.tag_for_frms_tag_last_pos_field;
        if(0 == next_frms_tag_pos)
        {
            aloge("mp4_file_repair_should_not_happed!");
        }
        for(;;)
        {
            off = lseek(file_fd,next_frms_tag_pos,SEEK_SET);
            if(-1 == off)
            {
                aloge("mp4_file_repair_seek_failed,%u",(unsigned int)off);
                return - 1;
            }

            struct frms_tag_node_s *p_frms_tag_node = (struct frms_tag_node_s *)malloc(sizeof(struct  frms_tag_node_s));
            if(NULL != p_frms_tag_node)
            {
                memset(p_frms_tag_node,0,sizeof(struct frms_tag_node_s));
                ret = read(file_fd,&p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_flag,FRMS_TAG_INFO_HDR_SIZE);
                if(ret != FRMS_TAG_INFO_HDR_SIZE)  // read flag/size/last_pos/cur_po/frm_hdrs_num
                {
                    aloge("mp4_file_repair_read_fatal_error,%lld-%u!",ret,FRMS_TAG_INFO_HDR_SIZE);
                }

                if(next_frms_tag_pos != p_frms_tag_node->frms_tag_info.frms_tag.cur_frms_tag_pos)
                {
                    alogw("mp4_file_repair_locate_error:%u-%u",next_frms_tag_pos,p_frms_tag_node->frms_tag_info.frms_tag.cur_frms_tag_pos);
                }

                if(NULL == p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                {
                    p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs = (MOV_FRM_HDR *)malloc(p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR));
                    if(NULL == p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                    {
                        aloge("mp4_file_repair_malloc_frm_hdrs_failed,%d",p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR));
                        if(NULL != p_frms_tag_node)
                        {
                            free(p_frms_tag_node);
                            p_frms_tag_node = NULL;
                        }
                        if(!list_empty(&context->frms_tag_list))
                        {
                            FRMS_TAG_NODE  *p_frms_tag_node, *pTmp;
                            list_for_each_entry_safe(p_frms_tag_node, pTmp, &context->frms_tag_list, list)
                            {
                                if(NULL != p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                                {
                                    free(p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs);
                                    p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs = NULL;
                                }
                                list_del(&p_frms_tag_node->list);
                                free(p_frms_tag_node);
                                context->frms_tag_node_num --;
                            }
                        }
                        return -1;
                    }
                }
                ret = read(file_fd,(char *)p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs,
                                p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR));
                if(ret != p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR)) 
                {
                    aloge("mp4_file_repair_read_fatal_error,%lld-%u!",ret,p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR));
                }
                p_frms_tag_node->frms_tag_info.frms_tag_id ++;
                list_add(&p_frms_tag_node->list,&context->frms_tag_list);
                context->frms_tag_node_num ++;
                if(p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_flag == 0xabababab && p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_pos == 0)
                {
                    alogd("mp4_file_repair_frms_tag_list_build_done,%d",context->frms_tag_node_num);
                    break;          // meet the first frms_tag in file
                }
                next_frms_tag_pos = p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_pos;
            }
            else
            {
                aloge("mp4_file_repair_malloc_frms_tag_node_s_failed,%u",sizeof(struct  frms_tag_node_s));
                if(!list_empty(&context->frms_tag_list))
                {
                    FRMS_TAG_NODE  *p_frms_tag_node, *pTmp;
                    list_for_each_entry_safe(p_frms_tag_node, pTmp, &context->frms_tag_list, list)
                    {
                        if(NULL != p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                        {
                            free(p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs);
                            p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs = NULL;
                        }
                        list_del(&p_frms_tag_node->list);
                        free(p_frms_tag_node);
                        context->frms_tag_node_num --;
                    }
                }

                return -1;
            }
        }
    }
    else            // locate frms_tag or first frms_tag
    {
        if(1 != context->frms_tag_node_num)
        {
            aloge("mp4_file_repair_frms_tag_node_num_err,%u",context->frms_tag_node_num);
        }
        struct frms_tag_node_s *p_tmp_frms_tag_node = list_first_entry(&context->frms_tag_list,FRMS_TAG_NODE,list);
        next_frms_tag_pos = p_tmp_frms_tag_node->frms_tag_info.frms_tag.frms_tag_pos;

        if(0 == next_frms_tag_pos && p_tmp_frms_tag_node->frms_tag_info.frms_tag.frms_tag_flag==FRMS_TAG_FLAG_FIRST)
        {
            alogw("mp4_file_repair_only_one_frms_tag");
            return 0;
        }
        else
        {
            for(;;)
            {
                off = lseek(file_fd,next_frms_tag_pos,SEEK_SET);
                if(-1 == off)
                {
                    aloge("mp4_file_repair_seek_failed,%u",next_frms_tag_pos);
                    return -1;
                }

                struct frms_tag_node_s *p_frms_tag_node = (struct frms_tag_node_s *)malloc(sizeof(struct  frms_tag_node_s));
                if(NULL != p_frms_tag_node)
                {
                    memset(p_frms_tag_node,0,sizeof(struct frms_tag_node_s));
                    ret = read(file_fd,&p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_flag,FRMS_TAG_INFO_HDR_SIZE);
                    if(ret != FRMS_TAG_INFO_HDR_SIZE)  // read flag/size/last_pos/cur_po/frm_hdrs_num
                    {
                        aloge("mp4_file_repair_read_fatal_error,%lld-%u!",ret,FRMS_TAG_INFO_HDR_SIZE);
                    }

                    if(next_frms_tag_pos != p_frms_tag_node->frms_tag_info.frms_tag.cur_frms_tag_pos)
                    {
                        alogw("mp4_file_repair_locate_error:%u-%u",next_frms_tag_pos,p_frms_tag_node->frms_tag_info.frms_tag.cur_frms_tag_pos);
                    }

                    if(NULL == p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                    {
                        p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs = (MOV_FRM_HDR *)malloc(p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR));
                        if(NULL == p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                        {
                            aloge("mp4_file_repair_malloc_frm_hdrs_failed,%d",p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR));
                            if(NULL != p_frms_tag_node)
                            {
                                free(p_frms_tag_node);
                                p_frms_tag_node = NULL;
                            }
                            if(!list_empty(&context->frms_tag_list))
                            {
                                FRMS_TAG_NODE  *p_frms_tag_node, *pTmp;
                                list_for_each_entry_safe(p_frms_tag_node, pTmp, &context->frms_tag_list, list)
                                {
                                    if(NULL != p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                                    {
                                        free(p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs);
                                        p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs = NULL;
                                    }
                                    list_del(&p_frms_tag_node->list);
                                    free(p_frms_tag_node);
                                    context->frms_tag_node_num --;
                                }
                            }
                            return -1;
                        }
                    }
                    ret = read(file_fd,(char *)p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs,
                                    p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR));
                    if(ret != p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR)) 
                    {
                        aloge("mp4_file_repair_read_fatal_error,%lld-%u!",ret,p_frms_tag_node->frms_tag_info.frms_tag.frms_num*sizeof(MOV_FRM_HDR));
                    }
                    p_frms_tag_node->frms_tag_info.frms_tag_id ++;
                    list_add(&p_frms_tag_node->list,&context->frms_tag_list);
                    context->frms_tag_node_num ++;
                    if(p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_flag == 0xabababab && p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_pos == 0)
                    {
                        alogd("mp4_file_repair_frms_tag_list_build_done,%d",context->frms_tag_node_num);
                        break;          // meet the first frms_tag in file
                    }
                    next_frms_tag_pos = p_frms_tag_node->frms_tag_info.frms_tag.frms_tag_pos;
                }
                else
                {
                    aloge("mp4_file_repair_malloc_frms_tag_node_s_failed,%u",sizeof(struct  frms_tag_node_s));
                    if(!list_empty(&context->frms_tag_list))
                    {
                        FRMS_TAG_NODE  *p_frms_tag_node, *pTmp;
                        list_for_each_entry_safe(p_frms_tag_node, pTmp, &context->frms_tag_list, list)
                        {
                            if(NULL != p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                            {
                                free(p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs);
                                p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs = NULL;
                            }
                            list_del(&p_frms_tag_node->list);
                            free(p_frms_tag_node);
                            context->frms_tag_node_num --;
                        }
                    }
                    return -1;
                }
            }
        }
    }
    
    return 0;
}

static int mp4_file_repair_wrt_pkt(MP4_FILE_REPAIR_CONTEXT *p_repair_ctx,MOV_FRM_HDR *pFrmHdr,int fd)
{
    MOV_FRM_HDR frm_hdr;
    AVPacket av_pkt;
    int tmp_ret = 0;

    memcpy(&frm_hdr,pFrmHdr,sizeof(MOV_FRM_HDR));
    memset(&av_pkt,0,sizeof(AVPacket));
    // construct pkt info
    av_pkt.stream_index = frm_hdr.frm_strm_index;
    av_pkt.flags = frm_hdr.frm_key_flag;
    av_pkt.pts = frm_hdr.frm_pts;
    av_pkt.duration = frm_hdr.frm_duration;
    av_pkt.size0 = frm_hdr.frm_size;
    av_pkt.pos = frm_hdr.frm_off;
    if(av_pkt.flags&AVPACKET_FLAG_THUMBNAIL)
    {
        av_pkt.data0 = malloc(av_pkt.size0);
        if(NULL != av_pkt.data0)
        {
            tmp_ret = lseek(fd,frm_hdr.frm_off,SEEK_SET);       // seek to thumbnail pic
            if(-1 == tmp_ret)
            {
                aloge("mp4_file_repair_frms_tag_thumb_nail_seek_failed,%u",frm_hdr.frm_off);
            }
            tmp_ret = read(fd,av_pkt.data0,av_pkt.size0);
            if(tmp_ret != av_pkt.size0)
            {
                aloge("file_repair_read_thm_data_err,s:%d,t:%d\n",
                        tmp_ret,av_pkt.size0);
            }

            // send pkt to muxer
            tmp_ret = p_repair_ctx->pWriter->MuxerWritePacket(p_repair_ctx->pMuxerCtx, &av_pkt);
            if(0 != tmp_ret)
            {
                aloge("file_repair_pkt_wrt_fail!!\n");
            }
            free(av_pkt.data0);
            
        }
        else
        {
            aloge("file_repair_thm_buff_malloc_failed,s:%d\n",
                    av_pkt.size0);
        }
    }
    else
    {
        // send pkt to muxer
        tmp_ret = p_repair_ctx->pWriter->MuxerWritePacket(p_repair_ctx->pMuxerCtx, &av_pkt);
        if(0 != tmp_ret)
        {
            aloge("file_repair_pkt_wrt_fail!!\n");
        }
        //adjust file read position to next frm header
        // lseek(fd,frm_hdr.frm_size,SEEK_CUR);
    }
    return 0;
}
static int mp4_file_repair_frm_by_frm(MP4_FILE_REPAIR_CONTEXT *context,int file_fd)
{
    MOV_FRM_HDR frm_hdr;
    MP4_FILE_REPAIR_CONTEXT *p_repair_ctx = context;
    MOV_META *p_meta = &p_repair_ctx->file_meta_info;
    AVPacket av_pkt;
    long long file_len = 0;
    int fd = file_fd;
    int tmp_ret = 0;

    file_len = lseek(fd,0,SEEK_END);
    if(-1 == file_len)
    {
        aloge("file_repair_file_seek_end_failed\n");
        if(-1 != fd)
        {
            close(fd);
            fd = -1;
        }
        return -1;
    }
    // seek to the position of the first frm header
    long long first_frm_hdr_off = META_DATA_POSITION;
    
    first_frm_hdr_off += sizeof(p_meta->v_strm_num);
    for(int j=0; j<p_meta->v_strm_num; ++j)
    {
        first_frm_hdr_off += (sizeof(V_STRM_META_DATA)-4);
        if(0 != p_meta->v_strms_meta[j].v_vos_len)
        {
            first_frm_hdr_off += p_meta->v_strms_meta[j].v_vos_len;
        }
    }

    first_frm_hdr_off += sizeof(p_meta->a_strm_num);
    for(int j=0; j<p_meta->a_strm_num; ++j)
    {
        first_frm_hdr_off += (sizeof(A_STRM_META_DATA)-4);
        if(0 != p_meta->a_strms_meta[j].a_vos_len)
        {
            first_frm_hdr_off += p_meta->a_strms_meta[j].a_vos_len;
        }
    }

    first_frm_hdr_off += sizeof(p_meta->t_strm_num);
    if(0 != p_meta->t_strm_num)
    {
        first_frm_hdr_off += (sizeof(T_STRM_META_DATA)-4);
        if(0 != p_meta->t_strms_meta.t_vos_len)
        {
            first_frm_hdr_off += p_meta->t_strms_meta.t_vos_len;
        }
    }
    
    tmp_ret = lseek(fd,first_frm_hdr_off,SEEK_SET);
    if(tmp_ret != first_frm_hdr_off)
    {
        aloge("file_repair_seek_err:%d-%d\n",tmp_ret,first_frm_hdr_off);
    }

    unsigned int last_frm_off = first_frm_hdr_off;
    while(1)
    {
        // read frm header
        tmp_ret = mp4_file_frm_hdr_read(p_repair_ctx,fd,&frm_hdr);
        if(0 != tmp_ret)
        {
            aloge("file_repair_frm_hdr_rd_fail,maybe reach file ending!!\n");
            break;
        }

        if(frm_hdr.frm_strm_index < 0 || 
                frm_hdr.frm_strm_index >=(p_meta->v_strm_num+p_meta->a_strm_num+p_meta->t_strm_num) 
                || frm_hdr.frm_size<0 || frm_hdr.frm_off<0 || frm_hdr.frm_duration<0) // patch again
        {
            alogw("file_repair_invalid_frm_hdr_to_end_repair:%d-%d-%d",
                    frm_hdr.frm_strm_index,frm_hdr.frm_size,frm_hdr.frm_off);
            break;
        }

        // to check the validation of frm hdr
        if(frm_hdr.frm_off <= last_frm_off) // patch for the case that only part of frm data is written into file
        {
            alogw("file_repair_may_reach_the_end_of_valid_data:%d-%d",last_frm_off,frm_hdr.frm_off);
            break;
        }
        // check the validation of frm header
        if(frm_hdr.frm_off+frm_hdr.frm_size <= file_len)
        {
            memset(&av_pkt,0,sizeof(AVPacket));
            // construct pkt info
            av_pkt.stream_index = frm_hdr.frm_strm_index;
            av_pkt.flags = frm_hdr.frm_key_flag;
            av_pkt.pts = frm_hdr.frm_pts;
            av_pkt.duration = frm_hdr.frm_duration;
            av_pkt.size0 = frm_hdr.frm_size;
            av_pkt.pos = frm_hdr.frm_off;

            last_frm_off = frm_hdr.frm_off;

            if(av_pkt.flags&AVPACKET_FLAG_THUMBNAIL)
            {
                av_pkt.data0 = malloc(av_pkt.size0);
                if(NULL != av_pkt.data0)
                {
                    tmp_ret = read(fd,av_pkt.data0,av_pkt.size0);
                    if(tmp_ret != av_pkt.size0)
                    {
                        aloge("file_repair_read_thm_data_err,s:%d,t:%d\n",
                                                    tmp_ret,av_pkt.size0);
                    }

                    // send pkt to muxer
                    tmp_ret = p_repair_ctx->pWriter->MuxerWritePacket(p_repair_ctx->pMuxerCtx,
                            &av_pkt);
                    if(0 != tmp_ret)
                    {
                        aloge("file_repair_pkt_wrt_fail!!\n");
                    }
                    free(av_pkt.data0);
                }
                else
                {
                    aloge("file_repair_thm_buff_malloc_failed,s:%d\n",
                                                    av_pkt.size0);
                }
            }
            else
            {
                tmp_ret = lseek(fd,frm_hdr.frm_size,SEEK_CUR);
                if(-1 != tmp_ret)
                {
                    tmp_ret = readahead(fd,tmp_ret,sizeof(MOV_FRM_HDR));
                    if(-1 == tmp_ret)
                    {
                        aloge("readahead_failed");
                    }
                }

                // send pkt to muxer
                tmp_ret = p_repair_ctx->pWriter->MuxerWritePacket(p_repair_ctx->pMuxerCtx,
                        &av_pkt);
                if(0 != tmp_ret)
                {
                    aloge("file_repair_pkt_wrt_fail!!\n");
                }
                //adjust file read position to next frm header
                // lseek(fd,frm_hdr.frm_size,SEEK_CUR);
            }
        }
        else
        {
            alogd("file_repair_end!!\n");
            break;
        }
    } 
    
    tmp_ret = p_repair_ctx->pWriter->MuxerWriteTrailer(p_repair_ctx->pMuxerCtx);
    if(0 != tmp_ret)
    {
        aloge("file_repair_trailer_wrt_fail!!\n");
    }

    return 0;
}
int mp4_file_repair(char const *file_path)
{
    MP4_FILE_REPAIR_CONTEXT file_repair_ctx;
    MP4_FILE_REPAIR_CONTEXT *p_repair_ctx = &file_repair_ctx;
    long long file_len = 0;
    int fd = -1;
    int tmp_ret = 0;

    if(NULL == file_path)
    {
        aloge("file_repair_file_path_invalid\n");
        return -1;
    }

    int64_t stamp1 = CDX_GetSysTimeUsMonotonic();
    alogd("file_repair_file_name:%s\n",file_path);
    fd = open(file_path,O_RDWR,0666);
    if(-1 == fd)
    {
        aloge("file_repair_file_open_failed!!\n");
        return -1;
    }
    // to check file is mp4 or not
    tmp_ret = mp4_file_format_chk(fd);
    if(0 != tmp_ret)
    {
        aloge("file_repair_file_type_err!\n");
        if(-1 != fd)
        {
            close(fd);
            fd = -1;
        }
        return -1;
    }
   
    file_len = lseek(fd,0,SEEK_END);
    if(-1 == file_len)
    {
        aloge("file_repair_file_seek_end_failed\n");
        if(-1 != fd)
        {
            close(fd);
            fd = -1;
        }
        return -1;
    }

    // check file validation
    if(MINIMAL_FILE_SIZE >= file_len)
    {
        aloge("file_repair_file_too_small,can't be repaired,s:%lld\n",file_len);
        if(-1 != fd)
        {
            close(fd);
            fd = -1;
        }
        return -1;
    }
    tmp_ret = mp4_file_need_repair_chk(fd,file_len); 
    if(0 == tmp_ret)
    {
        aloge("file_repair_cur_file_do_not_need_to_be_repaired!\n");
        if(-1 != fd)
        {
            close(fd);
            fd = -1;
        }
        return 0;
    }

    memset(p_repair_ctx,0,sizeof(MP4_FILE_REPAIR_CONTEXT));

    // read meata info
    tmp_ret = mp4_file_meta_data_read(p_repair_ctx,fd);
    if(0 != tmp_ret)
    {
        aloge("file_repair_meta_info_read_failed!!\n");
        if(-1 != fd)
        {
            close(fd);
            fd = -1;
        }
        return -1;
    }
    
    // mp4 muxer initilization
    tmp_ret = mp4_file_muxer_init(p_repair_ctx,fd); 
    if(0 != tmp_ret)
    {
        aloge("file_repair_muxer_init_failed!!\n");
        if(-1 != fd)
        {
            close(fd);
            fd = -1;
        }
        return -1;
    }

#ifdef MP4_FILE_REPAIR_FAST_VERSION 
    tmp_ret = mp4_file_repair_frms_tag_parse(p_repair_ctx,fd);
    if(0 == tmp_ret)
    {
        if(!list_empty(&p_repair_ctx->frms_tag_list))
        {
            FRMS_TAG_NODE  *p_frms_tag_node, *pTmp;
            list_for_each_entry_safe(p_frms_tag_node, pTmp, &p_repair_ctx->frms_tag_list, list)
            {
                // alogd("file_repair_frms_tag_node_info:%u-%u",p_repair_ctx->frms_tag_node_num,p_frms_tag_node->frms_tag_info.frms_tag.frms_num);
                for(int i=0; i<p_frms_tag_node->frms_tag_info.frms_tag.frms_num; i++)
                {
                    mp4_file_repair_wrt_pkt(p_repair_ctx,&p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs[i],fd);
                }

                if(NULL != p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs)
                {
                    free(p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs);
                    p_frms_tag_node->frms_tag_info.frms_tag.frm_hdrs = NULL;
                }
                list_del(&p_frms_tag_node->list);
                free(p_frms_tag_node);
                p_repair_ctx->frms_tag_node_num --;
            }

            tmp_ret = p_repair_ctx->pWriter->MuxerWriteTrailer(p_repair_ctx->pMuxerCtx);
            if(0 != tmp_ret)
            {
                aloge("file_repair_trailer_wrt_fail!!\n");
            }
        }
    }

#else
    mp4_file_repair_frm_by_frm(p_repair_ctx,fd);
#endif    

    mp4_file_muxer_rls(p_repair_ctx);
    mp4_file_vos_buff_rls(p_repair_ctx);
    
    if(-1 != fd)
    {
        close(fd);
        fd = -1;
    }
    int64_t stamp2 = CDX_GetSysTimeUsMonotonic();
    aloge("file_repair_time_consumed:%lld",(stamp2-stamp1)/1000);
    return 0;
}
