// #include <CDX_LogNDebug.h>
#define LOG_TAG "Mp4Muxer.c"
#include <utils/plat_log.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include <encoder_type.h>
#include "Mp4Muxer.h"
#include "ByteIOContext.h"
#include "avc.h"
#include "hevc.h"
#include <libavutil/intreadwrite.h>
#include <libavutil/mem.h>
#include <libavutil/bswap.h>

#define CACHE_ID_VIDEO 1 //same as track ID don't change
#define CACHE_ID_AUDIO 2 //same as track ID don't change

extern unsigned int gps_pack_method;
//#define ByteIOContext FsWriter
#if 0
static __inline int BSWAP32(unsigned int val)
{
    val= ((val<<8)&0xFF00FF00) | ((val>>8)&0x00FF00FF);\
    val= (val>>16) | (val<<16);
	return val;
}

static __inline void put_byte(ByteIOContext *s, int b)
{
    fwrite(&b,1,1,s);
}

static void put_buffer(ByteIOContext *s, char *buf, int size)
{
    fwrite(buf,1,size,s);
}

static void put_le32(ByteIOContext *s, unsigned int val)
{
	fwrite(&val,1,4,s);
}

static void put_be32(ByteIOContext *s, unsigned int val)
{
	val= ((val<<8)&0xFF00FF00) | ((val>>8)&0x00FF00FF);
    val= (val>>16) | (val<<16);
	fwrite(&val,1,4,s);
}

// void put_le16(ByteIOContext *s, unsigned int val)
// {
//     put_byte(s, val);
//     put_byte(s, val >> 8);
// }
// 
static void put_be16(ByteIOContext *s, unsigned int val)
{
    put_byte(s, val >> 8);
    put_byte(s, val);
}
// 
// void put_le24(ByteIOContext *s, unsigned int val)
// {
//     put_le16(s, val & 0xffff);
//     put_byte(s, val >> 16);
// }
// 
static void put_be24(ByteIOContext *s, unsigned int val)
{
    put_be16(s, val >> 8);
    put_byte(s, val);
}

static void put_tag(ByteIOContext *s, const char *tag)
{
    while (*tag) {
        put_byte(s, *tag++);
    }
}

//FIXME support 64 bit variant with wide placeholders
static offset_t updateSize(ByteIOContext *pb, offset_t pos)
{
    offset_t curpos = url_ftell(pb);
#if 0
    url_fseek(pb, pos, SEEK_SET);
    put_be32(pb, curpos - pos); /* rewrite size */
    url_fseek(pb, curpos, SEEK_SET);
#else
//    url_fseek(pb, pos-curpos, SEEK_CUR);
//    put_be32(pb, curpos-pos); /* rewrite size */
//    url_fseek(pb, curpos-pos-4, SEEK_CUR);
    pb->fsSeek(pb, pos-curpos, SEEK_CUR);
    put_be32_cache(NULL, pb, curpos-pos);   /* rewrite size */
    pb->fsSeek(pb, curpos-pos-4, SEEK_CUR);
#endif	
    return curpos - pos;
}
#endif

static unsigned int movGetStcoTagSize(MOVTrack *track)
{
    unsigned int stcoTagSize = 0;
    if(track->use_co64)
    {
        stcoTagSize = 4*4+track->stco_size*8;
    }
    else
    {
       stcoTagSize = (track->stco_size+4)*4;
    }
    return stcoTagSize;
}

/* Chunk offset atom */
static int mov_write_stco_tag(ByteIOContext *pb, MOVTrack *track)
{
    unsigned int i,j;
    MOVContext *mov = track->mov;
    unsigned int stcoTagSize = movGetStcoTagSize(track);
    //offset_t pos = url_ftell(pb);
    put_be32_cache(mov, pb, stcoTagSize); /* size */

    if(track->use_co64)
    {
        put_tag_cache(mov, pb, "co64");
    }
    else
    {
        put_tag_cache(mov, pb, "stco");
    }
    put_be32_cache(mov, pb, 0); /* version & flags */
    put_be32_cache(mov, pb, track->stco_size); /* entry count */

    if (track->enc->codec_id == CODEC_ID_GGAD) {
        for (i=0; i<track->stco_size; i++) {
			if(track->use_co64)
			{
				put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++);
				put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++);
			}
			else
			{
				mov->cache_read_ptr[STCO_ID][track->stream_type]++;
				put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++);
			}
        }
        return stcoTagSize;
    }
	aloge("mp4_stco_tag:%d-%d-%d-%d",track->stream_type,track->stco_tiny_pages,track->stco_size,track->use_co64);
    //if(mov->mov_cache_enties_in_file==0)//access stco only from dram
    if(track->stco_tiny_pages == 0){
        for (i=0; i<track->stco_size; i++) {
            if(track->use_co64)
            {
                put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++);//write high word 
                put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++);//write low word
            }
            else
            {
                mov->cache_read_ptr[STCO_ID][track->stream_type]++;	// jump high word
                put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++); 
            }
        }
    }
	else{

		//esFSYS_fseek(mov->fd_stco[track->stream_type],0,SEEK_SET);
        struct cdx_stream_info* pStcoStream = ((struct cdx_stream_info*)mov->fd_stco[track->stream_type]);
        pStcoStream->seek(pStcoStream, 0,SEEK_SET);
		for(i=0;i<track->stco_tiny_pages;i++)
		{
			//esFSYS_fread(mov->cache_tiny_page_ptr,1,MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,mov->fd_stco[track->stream_type]);
			unsigned int tmp = 0;
            tmp = pStcoStream->read(mov->cache_tiny_page_ptr,1,MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2, pStcoStream);
			if(tmp != MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2)
			{
				aloge("mp4_stco_rd_failed:%d-%d",tmp,MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2);
			}

			if(track->use_co64)
			{
				for(j=0;j<MOV_CACHE_TINY_PAGE_SIZE;j++)
				{
					put_be32_cache(mov, pb,mov->cache_tiny_page_ptr[2*j]);
					put_be32_cache(mov, pb,mov->cache_tiny_page_ptr[2*j+1]);
				}
			}
			else
			{
				for(j=0;j<MOV_CACHE_TINY_PAGE_SIZE;j++)
				{
					put_be32_cache(mov, pb,mov->cache_tiny_page_ptr[2*j+1]);
				}
			}
		}

        for (i=track->stco_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE; i<track->stco_size; i++) {
            if(track->use_co64)
            {
                put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++); 
                put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++);
            }
            else
            {
				mov->cache_read_ptr[STCO_ID][track->stream_type]++;
                put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++);
            }
			if(mov->cache_read_ptr[STCO_ID][track->stream_type] > mov->cache_end_ptr[STCO_ID][track->stream_type])
			{
				mov->cache_read_ptr[STCO_ID][track->stream_type] = mov->cache_start_ptr[STCO_ID][track->stream_type];
			}
        }
    }

    return stcoTagSize;//updateSize(pb, pos); 
}

static unsigned int movGetStszTagSize(MOVTrack *track)
{
    unsigned int stszTagSize;
    if(track->enc->codec_id == CODEC_ID_PCM)
    {
        stszTagSize = 5*4;
    }
    else
    {
        stszTagSize = (track->stsz_size+5)*4;
    }
    return stszTagSize;
    
}
/* Sample size atom */
static int mov_write_stsz_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int stszTagSize = movGetStszTagSize(track);
    //offset_t pos = url_ftell(pb);
    unsigned int i,j;
    put_be32_cache(mov, pb, stszTagSize); /* size */
    put_tag_cache(mov, pb, "stsz");
    put_be32_cache(mov, pb, 0); /* version & flags */
    if(track->enc->codec_id == CODEC_ID_PCM)
    {
        put_be32_cache(mov, pb, track->enc->channels*(track->enc->bits_per_sample>>3)); // sample size
    }
    else
    {
        put_be32_cache(mov, pb, 0); // sample size
    }
    put_be32_cache(mov, pb, track->stsz_size); // sample count
    if(track->enc->codec_id == CODEC_ID_PCM)
    {
        return stszTagSize;
    } else if(track->enc->codec_id == CODEC_ID_GGAD) {
        for (i=0; i<track->stsz_size; i++) {
            put_be32_cache(mov, pb, /*0x0105*/*mov->cache_read_ptr[STSZ_ID][track->stream_type]++);
        }
        return stszTagSize;
    }

	if(track->stsz_tiny_pages == 0){
        for (i=0; i<track->stsz_size; i++) {
            put_be32_cache(mov, pb,*mov->cache_read_ptr[STSZ_ID][track->stream_type]++);
        }
    }
	else{
		//esFSYS_fseek(mov->fd_stsz[track->stream_type],0,SEEK_SET);
        struct cdx_stream_info* pStszStream = ((struct cdx_stream_info*)mov->fd_stsz[track->stream_type]);
        pStszStream->seek(pStszStream, 0, SEEK_SET);
		for(i=0;i<track->stsz_tiny_pages;i++)
		{
			//esFSYS_fread(mov->cache_tiny_page_ptr,1,MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,mov->fd_stsz[track->stream_type]);
            pStszStream->read(mov->cache_tiny_page_ptr,1,MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,pStszStream);
			for(j=0;j<MOV_CACHE_TINY_PAGE_SIZE;j++)
			{
				put_be32_cache(mov, pb,mov->cache_tiny_page_ptr[j]);
			}
		}
		
        for (i=track->stsz_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE; i<track->stsz_size; i++) {
            put_be32_cache(mov, pb,*mov->cache_read_ptr[STSZ_ID][track->stream_type]++);
			if(mov->cache_read_ptr[STSZ_ID][track->stream_type] > mov->cache_end_ptr[STSZ_ID][track->stream_type])
			{
				mov->cache_read_ptr[STSZ_ID][track->stream_type] = mov->cache_start_ptr[STSZ_ID][track->stream_type];
			}
        }
    }
	
    return stszTagSize; //updateSize(pb, pos);
}

static unsigned int movGetStscTagSize(MOVTrack *track)
{
    if (track->enc->codec_id != CODEC_ID_GGAD) {
        unsigned int stscTagSize = (track->stsc_size*3 + 4)*4;
        return stscTagSize;
    } else {
        return (4 + 3)*4;
    }
}

/* Sample to chunk atom */
static int mov_write_stsc_tag(ByteIOContext *pb, MOVTrack *track)
{
	unsigned int i;
	MOVContext *mov = track->mov;
    unsigned int stscTagSize = movGetStscTagSize(track);
    //offset_t pos = url_ftell(pb);
    put_be32_cache(mov, pb, stscTagSize); /* size */
    put_tag_cache(mov, pb, "stsc");
    put_be32_cache(mov, pb, 0); // version & flags

    if (track->enc->codec_id == CODEC_ID_GGAD) {
        put_be32_cache(mov, pb, 1); // entry count, do NOT use track->stsc_size
        put_be32_cache(mov, pb, 1); // first chunk
        put_be32_cache(mov, pb, 1); // samples per chunk
        put_be32_cache(mov, pb, 0x1); // sample description index
        return stscTagSize;
    }

    put_be32_cache(mov, pb, track->stsc_size); // entry count

	if(track->stsc_tiny_pages == 0){
        for (i=0; i<track->stsc_size; i++) {
			put_be32_cache(mov, pb, i+1); // first chunk
            put_be32_cache(mov, pb,*mov->cache_read_ptr[STSC_ID][track->stream_type]++);
			put_be32_cache(mov, pb, 0x1); // sample description index
        }
    }
	else{
		//esFSYS_fseek(mov->fd_stsc[track->stream_type],0,SEEK_SET);
		struct cdx_stream_info* pStscStream = ((struct cdx_stream_info*)mov->fd_stsc[track->stream_type]);
        pStscStream->seek(pStscStream, 0, SEEK_SET);
		for(i=0;i<track->stsc_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE;i++)
		{
			put_be32_cache(mov, pb, i+1); // first chunk
			//esFSYS_fread(mov->cache_tiny_page_ptr,1,4,mov->fd_stsc[track->stream_type]);
			pStscStream->read(mov->cache_tiny_page_ptr,1,4,pStscStream);
			put_be32_cache(mov, pb,*mov->cache_tiny_page_ptr);
			put_be32_cache(mov, pb, 0x1); // sample description index
		}
		
        for (i=track->stsc_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE; i<track->stsc_size; i++) {
			put_be32_cache(mov, pb, i+1); // first chunk
            put_be32_cache(mov, pb,*mov->cache_read_ptr[STSC_ID][track->stream_type]++);
			put_be32_cache(mov, pb, 0x1); // sample description index
			if(mov->cache_read_ptr[STSC_ID][track->stream_type] > mov->cache_end_ptr[STSC_ID][track->stream_type])
			{
				mov->cache_read_ptr[STSC_ID][track->stream_type] = mov->cache_start_ptr[STSC_ID][track->stream_type];
			}
        }
    }

    return stscTagSize; //updateSize(pb, pos);
}

/* Sync sample atom */
static unsigned int movGetStssTagSize(MOVTrack *track)
{
    int keyframes = track->keyFrame_num;
    unsigned int stssTagSize = (keyframes+4)*4;
    return stssTagSize;
}
static int mov_write_stss_tag(ByteIOContext *pb, MOVTrack *track)
{
	MOVContext *mov = track->mov;
    int i, keyframes;
    //int key_index = 1;
    unsigned int stssTagSize = movGetStssTagSize(track);
	keyframes = track->keyFrame_num;
    put_be32_cache(mov, pb, stssTagSize); // size
    put_tag_cache(mov, pb, "stss");
    put_be32_cache(mov, pb, 0); // version & flags
    put_be32_cache(mov, pb, keyframes); // entry count
    for (i=0; i<keyframes; i++) {
        put_be32_cache(mov, pb, mov->cache_keyframe_ptr[track->stream_type][i]);
    }

    return stssTagSize;//updateSize(pb, pos);
}


static unsigned int descrLength(unsigned int len)
{
    int i;
    for(i=1; len>>(7*i); i++);
    return len + 1 + i;
}

static void putDescr_cache(MOVContext *mov, ByteIOContext *pb, int tag, unsigned int size)
{
    int i= descrLength(size) - size - 2;
    put_byte_cache(mov, pb, tag);
    for(; i>0; i--)
        put_byte_cache(mov, pb, (size>>(7*i)) | 0x80);
    put_byte_cache(mov, pb, size & 0x7F);
}

static unsigned int movGetEsdsTagSize(MOVTrack *track)
{
    unsigned int esdsTagSize = 12;
    unsigned int DecoderSpecificInfoSize = track->vosLen ? descrLength(track->vosLen):0;
    unsigned int DecoderConfigDescriptorSize = descrLength(13 + DecoderSpecificInfoSize);
    unsigned int SLConfigDescriptorSize = descrLength(1);
    unsigned int ES_DescriptorSize = descrLength(3 + DecoderConfigDescriptorSize + SLConfigDescriptorSize);
    esdsTagSize += ES_DescriptorSize;
    return esdsTagSize;
}

static int mov_write_esds_tag(ByteIOContext *pb, MOVTrack *track) // Basic
{
    MOVContext *mov = track->mov;
    unsigned int esdsTagSize = movGetEsdsTagSize(track);
    //offset_t pos = url_ftell(pb);
    int decoderSpecificInfoLen = track->vosLen ? descrLength(track->vosLen):0;
	
    put_be32_cache(mov, pb, esdsTagSize); /* size */
    put_tag_cache(mov, pb, "esds");
    put_be32_cache(mov, pb, 0); // Version
	
    // ES descriptor
    putDescr_cache(mov, pb, 0x03, 3 + descrLength(13 + decoderSpecificInfoLen) +
		descrLength(1));
    put_be16_cache(mov, pb, track->trackID);
    put_byte_cache(mov, pb, 0x00); // flags (= no flags)
	
    // DecoderConfig descriptor
    putDescr_cache(mov, pb, 0x04, 13 + decoderSpecificInfoLen);
	
    
	// Object type indication
    put_byte_cache(mov, pb, track->enc->codec_id);
    	
    // the following fields is made of 6 bits to identify the streamtype (4 for video, 5 for audio)
    // plus 1 bit to indicate upstream and 1 bit set to 1 (reserved)
	if(track->enc->codec_type == CODEC_TYPE_AUDIO)
	{
	    put_byte_cache(mov, pb, 0x15); // flags (= Audiostream)
	}
	else
	{
    	put_byte_cache(mov, pb, 0x11); // flags (= Visualstream)
    }
	
    put_byte_cache(mov, pb,  0);    // Buffersize DB (24 bits)
    put_be16_cache(mov, pb,  0); // Buffersize DB
	
    put_be32_cache(mov, pb, 0); // maxbitrate (FIXME should be max rate in any 1 sec window)
    put_be32_cache(mov, pb, 0); // vbr
	
    if (track->vosLen) {
        // DecoderSpecific info descriptor
        putDescr_cache(mov, pb, 0x05, track->vosLen);
        put_buffer_cache(mov, pb, track->vosData, track->vosLen);
    }
	
    // SL descriptor
    putDescr_cache(mov, pb, 0x06, 1);
    put_byte_cache(mov, pb, 0x02);
    //return updateSize(pb, pos);
    return esdsTagSize;
}
#if 0
unsigned char *ff_avc_find_startcode(unsigned char *p, unsigned char *end)
{
    unsigned char *a = p + 4 - ((long)p & 3);

    for( end -= 3; p < a && p < end; p++ ) {
        if( p[0] == 0 && p[1] == 0 && p[2] == 1 )
            return p;
    }

    for( end -= 3; p < end; p += 4 ) {
        unsigned int x = *(const unsigned int*)p;
//      if( (x - 0x01000100) & (~x) & 0x80008000 ) // little endian
//      if( (x - 0x00010001) & (~x) & 0x00800080 ) // big endian
        if( (x - 0x01010101) & (~x) & 0x80808080 ) { // generic
            if( p[1] == 0 ) {
                if( p[0] == 0 && p[2] == 1 )
                    return p-1;
                if( p[2] == 0 && p[3] == 1 )
                    return p;
            }
            if( p[3] == 0 ) {
                if( p[2] == 0 && p[4] == 1 )
                    return p+1;
                if( p[4] == 0 && p[5] == 1 )
                    return p+2;
            }
        }
    }

    for( end += 3; p < end; p++ ) {
        if( p[0] == 0 && p[1] == 0 && p[2] == 1 )
            return p;
    }

    return end + 3;
}

int ff_avc_parse_nal_units(unsigned char *buf_in, unsigned char **buf, int *size)
{
    unsigned char *p = buf_in,*ptr_t;
    unsigned char *end = p + *size;
    unsigned char *nal_start, *nal_end;
    unsigned int nal_size,nal_size_b;

    ptr_t = *buf = av_malloc(*size + 256);
    nal_start = ff_avc_find_startcode(p, end);
    while (nal_start < end) {
        while(!*(nal_start++));
        nal_end = ff_avc_find_startcode(nal_start, end);
        //put_be32(pb, nal_end - nal_start);
        //put_buffer(pb, nal_start, nal_end - nal_start);
        nal_size = nal_end - nal_start;
        nal_size_b = BSWAP32(nal_size);
        memcpy(ptr_t,&nal_size_b,4);
        ptr_t += 4;
        memcpy(ptr_t,nal_start,nal_size);
        ptr_t += nal_size;
        nal_start = nal_end;
    }

    *size = ptr_t - *buf;
    return 0;
}

static unsigned int FFIsomGetAvccSize(unsigned char *data, int len)
{
    unsigned int avccSize = 0;
    if (len > 6)
    {
        /* check for h264 start code */
        if (AV_RB32(data) == 0x00000001) 
        {
            unsigned char *buf=NULL, *end, *start;
            unsigned int sps_size=0, pps_size=0;
            unsigned char *sps=0, *pps=0;

            int ret = ff_avc_parse_nal_units(data, &buf, &len);
            if (ret < 0)
            {
                alogw("fatal error! ret[%d] of ff_avc_parse_nal_units() < 0", ret);
                free(buf);
                return 0;
            }
            start = buf;
            end = buf + len;

            /* look for sps and pps */
            while (buf < end) 
            {
                unsigned int size;
                unsigned char nal_type;
                size = AV_RB32(buf);
                nal_type = buf[4] & 0x1f;
                if (nal_type == 7) 
                { /* SPS */
                    sps = buf + 4;
                    sps_size = size;
                } 
                else if (nal_type == 8) 
                { /* PPS */
                    pps = buf + 4;
                    pps_size = size;
                }
                buf += size + 4;
            }
            //assert(sps);
            //assert(pps);

//            put_byte(pb, 1); /* version */
//            put_byte(pb, sps[1]); /* profile */
//            put_byte(pb, sps[2]); /* profile compat */
//            put_byte(pb, sps[3]); /* level */
//            put_byte(pb, 0xff); /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
//            put_byte(pb, 0xe1); /* 3 bits reserved (111) + 5 bits number of sps (00001) */
//
//            put_be16(pb, sps_size);
//            put_buffer(pb, (char*)sps, sps_size);
//            put_byte(pb, 1); /* number of pps */
//            put_be16(pb, pps_size);
//            put_buffer(pb, (char*)pps, pps_size);
            avccSize = (6 + 2 + sps_size + 1 + 2 + pps_size);
            av_free(start);
        } 
        else 
        {
//            put_buffer(pb, (char*)data, len);
            avccSize = len;
        }
    }
    return avccSize;
}

static int ff_isom_write_avcc(ByteIOContext *pb, unsigned char *data, int len, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    if (len > 6) {
        /* check for h264 start code */
        if (AV_RB32(data) == 0x00000001) {
            unsigned char *buf=NULL, *end, *start;
            unsigned int sps_size=0, pps_size=0;
            unsigned char *sps=0, *pps=0;

            int ret = ff_avc_parse_nal_units(data, &buf, &len);
            if (ret < 0)
            {
                free(buf);
                return ret;
            }
            start = buf;
            end = buf + len;

            /* look for sps and pps */
            while (buf < end) {
                unsigned int size;
                unsigned char nal_type;
                size = AV_RB32(buf);
                nal_type = buf[4] & 0x1f;
                if (nal_type == 7) { /* SPS */
                    sps = buf + 4;
                    sps_size = size;
                } else if (nal_type == 8) { /* PPS */
                    pps = buf + 4;
                    pps_size = size;
                }
                buf += size + 4;
            }
            //assert(sps);
            //assert(pps);

            put_byte_cache(mov, pb, 1); /* version */
            put_byte_cache(mov, pb, sps[1]); /* profile */
            put_byte_cache(mov, pb, sps[2]); /* profile compat */
            put_byte_cache(mov, pb, sps[3]); /* level */
            put_byte_cache(mov, pb, 0xff); /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
            put_byte_cache(mov, pb, 0xe1); /* 3 bits reserved (111) + 5 bits number of sps (00001) */

            put_be16_cache(mov, pb, sps_size);
            put_buffer_cache(mov, pb, (char*)sps, sps_size);
            put_byte_cache(mov, pb, 1); /* number of pps */
            put_be16_cache(mov, pb, pps_size);
            put_buffer_cache(mov, pb, (char*)pps, pps_size);
            av_free(start);
        } else {
            put_buffer_cache(mov, pb, (char*)data, len);
        }
    }
    return 0;
}
#endif

static unsigned int movGetAvccTagSize(MOVTrack *track)
{
    unsigned int avccTagSize = 8;
    avccTagSize += FFIsomGetAvccSize((unsigned char*)track->vosData, track->vosLen);
    return avccTagSize;
}

static unsigned int movGetHvccTagSize(MOVTrack *track, int ps_array_completeness)
{
    unsigned int hvccTagSize = 8;
    hvccTagSize += FFIsomGetHvccSize((unsigned char*)track->vosData, track->vosLen, ps_array_completeness);
    return hvccTagSize;
}

static int mov_write_avcc_tag(ByteIOContext *pb, MOVTrack *track){    
    MOVContext *mov = track->mov;
    unsigned int avccTagSize = movGetAvccTagSize(track);
    //offset_t pos = url_ftell(pb);    
    put_be32_cache(mov, pb, avccTagSize);   /* size */
    put_tag_cache(mov, pb, "avcC");    
    //ff_isom_write_avcc(pb, (unsigned char*)track->vosData, track->vosLen, track);
    ff_isom_write_avcc(pb, (unsigned char*)track->vosData, track->vosLen);
    //return updateSize(pb, pos);
    return avccTagSize;
}

static int mov_write_hvcc_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int hvccTagSize = movGetHvccTagSize(track, 0);
    //int64_t pos = avio_tell(pb);

    put_be32_cache(mov, pb, hvccTagSize);
    put_tag_cache(mov, pb, "hvcC");
    ff_isom_write_hvcc(pb, (const uint8_t*)track->vosData, track->vosLen, 0);
    return hvccTagSize;
}

static unsigned int movGetAudioTagSize(MOVTrack *track)
{
    int version = 0;
    unsigned int audioTagSize = 16 + 8 + 12;
    if(track->enc->codec_id == CODEC_ID_ADPCM)
    {
        version = 1;
    }
    if(version == 1) 
    { /* SoundDescription V1 extended info */
        audioTagSize += 16;
    }
    if(track->tag == MKTAG('m','p','4','a'))
        audioTagSize += movGetEsdsTagSize(track);

    return audioTagSize;
}
static int mov_write_audio_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int audioTagSize = movGetAudioTagSize(track);
    //offset_t pos = url_ftell(pb);
    int version = 0;

    if(track->enc->codec_id == CODEC_ID_ADPCM)
    {
        version = 1;
    }
	
    put_be32_cache(mov, pb, audioTagSize); /* size */
    put_le32_cache(mov, pb, track->tag); // store it byteswapped
    put_be32_cache(mov, pb, 0); /* Reserved */
    put_be16_cache(mov, pb, 0); /* Reserved */
    put_be16_cache(mov, pb, 1); /* Data-reference index, XXX  == 1 */
	
    /* SoundDescription */
    put_be16_cache(mov, pb, version); /* Version */
    put_be16_cache(mov, pb, 0); /* Revision level */
    put_be32_cache(mov, pb, 0); /* vendor */
	
    { /* reserved for mp4/3gp */
        put_be16_cache(mov, pb, track->enc->channels); //channel
        put_be16_cache(mov, pb, track->enc->bits_per_sample);//bits per sample
        put_be16_cache(mov, pb, 0); /* compression id = 0*/
    }
	
    put_be16_cache(mov, pb, 0); /* packet size (= 0) */
    put_be16_cache(mov, pb, track->enc->sample_rate); /* Time scale !!!??? */
    put_be16_cache(mov, pb, 0); /* Reserved */
	
    if(version == 1) { /* SoundDescription V1 extended info */
        if(track->enc->codec_id == CODEC_ID_ADPCM)
        {
            //put_be32(pb, 0x7f9); /* Samples per packet */
            put_be32_cache(mov, pb, track->enc->frame_size); /* Samples per packet */
            //put_be32_cache(mov, pb, 0x400); /* Bytes per packet */
            //put_be32_cache(mov, pb, 0x800); /* Bytes per frame */
            put_be32_cache(mov, pb, track->enc->frame_size*(track->enc->bits_per_sample>>3)); /* Bytes per packet */
            put_be32_cache(mov, pb, track->enc->frame_size*(track->enc->bits_per_sample>>3)*track->enc->channels); /* Bytes per frame */
            put_be32_cache(mov, pb, 2); /* Bytes per sample */
        }
        else
        {
            put_be32_cache(mov, pb, track->enc->frame_size); /* Samples per packet */
            put_be32_cache(mov, pb, track->sampleSize / track->enc->channels); /* Bytes per packet */
            put_be32_cache(mov, pb, track->sampleSize); /* Bytes per frame */
            put_be32_cache(mov, pb, 2); /* Bytes per sample */
        }
    }
	
    if(track->tag == MKTAG('m','p','4','a'))
        mov_write_esds_tag(pb, track);
	
    //return updateSize(pb, pos);
    return audioTagSize;
}

static int mov_find_codec_tag(AVFormatContext *s, MOVTrack *track)
{
    int tag = track->enc->codec_tag;
	
	switch(track->enc->codec_id)
	{
	case CODEC_ID_H264:
		tag = MKTAG('a','v','c','1');
		break;
    case CODEC_ID_H265:
		tag = MKTAG('h','v','c','1');
		break;
	case CODEC_ID_MPEG4:
		tag = MKTAG('m','p','4','v');
		break;
	case CODEC_ID_AAC:	
		tag = MKTAG('m','p','4','a');
		break;
    case CODEC_ID_PCM:
	    tag = MKTAG('s','o','w','t');
		break;
	case CODEC_ID_ADPCM:
	    tag = MKTAG('m','s',0x00,0x11);
		break;
	case CODEC_ID_MJPEG:  /* gushiming compressed source */
	    tag = MKTAG('m','j','p','a');  /* Motion-JPEG (format A) */
		break;
	case CODEC_ID_GGAD:  /* GPS/GSensor/ADAS/DriverID text source */
	    tag = MKTAG('t','e','x','t');
		break;
	default:
		break;
	}
	
    return tag;
}

static unsigned int movGetTextTagSize(MOVTrack *track)
{
    return 75;		// size changable by vendor_info
}

static int mov_write_text_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int textTagSize = movGetTextTagSize(track);
    int version = 0;
    char *vendor_info = "AllWinner custom";           // str_len: 16 Bytes

    put_be32_cache(mov, pb, textTagSize); /* size */
    put_le32_cache(mov, pb, track->tag); // store it byteswapped
    put_be32_cache(mov, pb, version);   /* version & flags */
    put_be32_cache(mov, pb, 1);         /* entry count */
    put_be32_cache(mov, pb, 0x6000);
    put_be32_cache(mov, pb, 1);
    put_be32_cache(mov, pb, 0);
    put_be32_cache(mov, pb, 0);
    put_be32_cache(mov, pb, 0);
    put_be32_cache(mov, pb, 0);
    put_be32_cache(mov, pb, 0);
    put_be32_cache(mov, pb, 0);
    put_be32_cache(mov, pb, 0);
    put_be32_cache(mov, pb, 0);
    put_be24_cache(mov, pb, 0);
    put_buffer_cache(mov, pb, vendor_info, strlen(vendor_info));

    return textTagSize;
}

static unsigned int movGetVideoTagSize(MOVTrack *track)
{
    unsigned int videoTagSize = 16 + 4 + 12 + 18 + 32 + 4;
    if(track->tag == MKTAG('a','v','c','1'))
        videoTagSize += movGetAvccTagSize(track);
    else if(track->tag == MKTAG('m','p','4','v'))
        videoTagSize += movGetEsdsTagSize(track);
    else if(track->tag == MKTAG('h','v','c','1'))
        videoTagSize += movGetHvccTagSize(track, 0);
    else if(track->tag == MKTAG('m','j','p','a'))
        alogd("mjpa[0x%x] need not write extra data", track->tag);
    else
        alogw("fatal error! video[0x%x] need not write extra data?", track->tag);
    return videoTagSize;
}

static int mov_write_video_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int videoTagSize = movGetVideoTagSize(track);
    //offset_t pos = url_ftell(pb);
    char compressor_name[32];
	
    put_be32_cache(mov, pb, videoTagSize); /* size */
    put_le32_cache(mov, pb, track->tag); // store it byteswapped
    put_be32_cache(mov, pb, 0); /* Reserved */
    put_be16_cache(mov, pb, 0); /* Reserved */
    put_be16_cache(mov, pb, 1); /* Data-reference index */
	
    put_be16_cache(mov, pb, 0); /* Codec stream version */
    put_be16_cache(mov, pb, 0); /* Codec stream revision (=0) */
    {
        put_be32_cache(mov, pb, 0); /* Reserved */
        put_be32_cache(mov, pb, 0); /* Reserved */
        put_be32_cache(mov, pb, 0); /* Reserved */
    }
    put_be16_cache(mov, pb, track->enc->width); /* Video width */
    put_be16_cache(mov, pb, track->enc->height); /* Video height */
    put_be32_cache(mov, pb, 0x00480000); /* Horizontal resolution 72dpi */
    put_be32_cache(mov, pb, 0x00480000); /* Vertical resolution 72dpi */
    put_be32_cache(mov, pb, 0); /* Data size (= 0) */
    put_be16_cache(mov, pb, 1); /* Frame count (= 1) */
	
    memset(compressor_name,0,32);
    put_byte_cache(mov, pb, strlen((const char *)compressor_name));
    put_buffer_cache(mov, pb, compressor_name, 31);
	
    put_be16_cache(mov, pb, 0x18); /* Reserved */
    put_be16_cache(mov, pb, 0xffff); /* Reserved */
    if(track->tag == MKTAG('a','v','c','1'))
        mov_write_avcc_tag(pb, track);
    else if(track->tag == MKTAG('m','p','4','v'))
        mov_write_esds_tag(pb, track);
    else if (track->tag == MKTAG('h','v','c','1'))
        mov_write_hvcc_tag(pb, track);

    //return updateSize(pb, pos);
    return videoTagSize;
}

static unsigned int movGetStsdTagSize(MOVTrack *track)
{
    unsigned int stsdTagSize = 16;
    if (track->enc->codec_type == CODEC_TYPE_VIDEO)
        stsdTagSize += movGetVideoTagSize(track);
    else if (track->enc->codec_type == CODEC_TYPE_AUDIO)
        stsdTagSize += movGetAudioTagSize(track);
    else if (track->enc->codec_type == CODEC_TYPE_TEXT)
        stsdTagSize += movGetTextTagSize(track);
    return stsdTagSize;
}

static int mov_write_stsd_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int stsdTagSize = movGetStsdTagSize(track);
    //offset_t pos = url_ftell(pb);
    put_be32_cache(mov, pb, stsdTagSize); /* size */
    put_tag_cache(mov, pb, "stsd");
    put_be32_cache(mov, pb, 0); /* version & flags */
    put_be32_cache(mov, pb, 1); /* entry count */
    if (track->enc->codec_type == CODEC_TYPE_VIDEO)
        mov_write_video_tag(pb, track);
    else if (track->enc->codec_type == CODEC_TYPE_AUDIO)
        mov_write_audio_tag(pb, track);
    else if (track->enc->codec_type == CODEC_TYPE_TEXT)
        mov_write_text_tag(pb, track);

    //return updateSize(pb, pos);
    return stsdTagSize;
}



/* Time to sample atom */
// static int mov_write_stts_tag(ByteIOContext *pb, MOVTrack *track)
// {
//     MOV_stts_t *stts_entries;
//     int entries = -1;
//     unsigned int atom_size;
//     int i;
// 	
//     if (track->enc->codec_type == CODEC_TYPE_AUDIO) {
//         stts_entries = av_malloc(sizeof(*stts_entries)); /* one entry */
//         stts_entries[0].count = track->sampleCount;
//         stts_entries[0].duration = track->enc->frame_size;
//         entries = 1;
//     } else {
// 		stts_entries = av_malloc(sizeof(*stts_entries)); /* one entry */
//         stts_entries[0].count = track->sampleCount;
//         stts_entries[0].duration = track->sampleDuration;
//         entries = 1;
//     }
//     atom_size = 16 + (entries * 8);
//     put_be32(pb, atom_size); /* size */
//     put_tag(pb, "stts");
//     put_be32(pb, 0); /* version & flags */
//     put_be32(pb, entries); /* entry count */
//     for (i=0; i<entries; i++) {
//         put_be32(pb, stts_entries[i].count);
//         put_be32(pb, stts_entries[i].duration);
//     }
//     av_free(stts_entries);
//     return atom_size;
// }

static unsigned int movGetSttsTagSize(MOVTrack *track)
{
    //MOV_stts_t stts_entries[1];
    unsigned int entries = 0;
    unsigned int atom_size = 0;
    if (track->enc->codec_type == CODEC_TYPE_AUDIO) 
    {
        entries = 1;
        atom_size = 16 + (entries * 8);
    } 
    else if (track->enc->codec_type == CODEC_TYPE_VIDEO)
    {
        entries = track->stts_size;
        atom_size = 16 + (entries * 8);
    }
    else if (track->enc->codec_type == CODEC_TYPE_TEXT)
    {
        entries = 1;
        atom_size = 16 + (entries * 8);
    }
    return atom_size;
}

static int mov_write_stts_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    MOV_stts_t stts_entries[1];
    unsigned int entries = 0;
    unsigned int atom_size = 0;
    unsigned int i,j;
    unsigned int pos;
	
    if (track->enc->codec_type == CODEC_TYPE_AUDIO) {
        stts_entries[0].count = track->stsz_size;
        if(track->enc->codec_id == CODEC_ID_PCM)    //for uncompressed audio, one frame == one sample.
        {
            stts_entries[0].duration = 1;
        }
        else
        {
            stts_entries[0].duration = track->enc->frame_size;
        }
        entries = 1;
        atom_size = 16 + (entries * 8);
        put_be32_cache(mov, pb, atom_size); /* size */
        put_tag_cache(mov, pb, "stts");
        put_be32_cache(mov, pb, 0); /* version & flags */
        put_be32_cache(mov, pb, entries); /* entry count */
        for (i=0; i<entries; i++) {
            put_be32_cache(mov, pb, stts_entries[i].count);
            put_be32_cache(mov, pb, stts_entries[i].duration);
        }
    }
    else if (track->enc->codec_type == CODEC_TYPE_TEXT) {
        stts_entries[0].count = track->stsz_size;
        stts_entries[0].duration = track->timescale;
        entries = 1;
        atom_size = 16 + (entries * 8);
        put_be32_cache(mov, pb, atom_size); /* size */
        put_tag_cache(mov, pb, "stts");
        put_be32_cache(mov, pb, 0); /* version & flags */
        put_be32_cache(mov, pb, entries); /* entry count */     // text always 1
        for (i=0; i<entries; i++) {
            put_be32_cache(mov, pb, stts_entries[i].count);
            put_be32_cache(mov, pb, stts_entries[i].duration);
        }

    }
    else if (track->enc->codec_type == CODEC_TYPE_VIDEO) {

        int skip_first_frame = 1;
        //int *ptr;
        entries = track->stts_size;
        atom_size = 16 + (entries * 8);
        put_be32_cache(mov, pb, atom_size); /* size */
        put_tag_cache(mov, pb, "stts");
        put_be32_cache(mov, pb, 0); /* version & flags */
        put_be32_cache(mov, pb, entries); /* entry count */

        if(track->stts_tiny_pages == 0){
            for (i=0; i<track->stts_size; i++) {
                if(skip_first_frame)
                {
                    skip_first_frame = 0;
                    mov->cache_read_ptr[STTS_ID][track->stream_type]++;
                    continue;
                }
                put_be32_cache(mov, pb, 1);//count
                put_be32_cache(mov, pb,*mov->cache_read_ptr[STTS_ID][track->stream_type]++);
            }
        }
    	else{
//    		esFSYS_fseek(mov->fd_stts[track->stream_type],0,SEEK_SET);
//            pos = esFSYS_ftell(mov->fd_stts[track->stream_type]);
            struct cdx_stream_info* pSttsStream = ((struct cdx_stream_info*)mov->fd_stts[track->stream_type]);
            pSttsStream->seek(pSttsStream, 0, SEEK_SET);
            pos = pSttsStream->tell(pSttsStream);
            eLIBs_printf("the position is :%d\n",pos);
    		for(i=0;i<track->stts_tiny_pages;i++)
    		{
    			//esFSYS_fread(mov->cache_tiny_page_ptr,1,MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,mov->fd_stts[track->stream_type]);
                pSttsStream->read(mov->cache_tiny_page_ptr,1,MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,pSttsStream);
    			for(j=0;j<MOV_CACHE_TINY_PAGE_SIZE;j++)
    			{
    			    if(skip_first_frame)
                    {
                        skip_first_frame = 0;
                        continue;
                    }
    			    put_be32_cache(mov, pb, 1);//count
    				put_be32_cache(mov, pb,mov->cache_tiny_page_ptr[j]);
    			}
    		}
    		
            for (i=track->stts_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE; i<track->stts_size; i++) {
                put_be32_cache(mov, pb, 1);//count
                put_be32_cache(mov, pb,*mov->cache_read_ptr[STTS_ID][track->stream_type]++);
    			if(mov->cache_read_ptr[STTS_ID][track->stream_type] > mov->cache_end_ptr[STTS_ID][track->stream_type])
    			{
    				mov->cache_read_ptr[STTS_ID][track->stream_type] = mov->cache_start_ptr[STTS_ID][track->stream_type];
    			}
            }
        }

        //write last packet duration, set it to 0??
        put_be32_cache(mov, pb, 1);//count
        put_be32_cache(mov, pb, 0);
    }

    return atom_size;
}

static unsigned int movGetDrefTagSize()
{
    return 28;
}

static int mov_write_dref_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int drefTagSize = movGetDrefTagSize();
    put_be32_cache(mov, pb, drefTagSize); /* size */
    put_tag_cache(mov, pb, "dref");
    put_be32_cache(mov, pb, 0); /* version & flags */
    put_be32_cache(mov, pb, 1); /* entry count */
	
    put_be32_cache(mov, pb, 0xc); /* size */
    put_tag_cache(mov, pb, "url ");
    put_be32_cache(mov, pb, 1); /* version & flags */
	
    return drefTagSize;
}

static unsigned int movGetStblTagSize(MOVTrack *track)
{
    unsigned int stblTagSize = 8;
    stblTagSize += movGetStsdTagSize(track);
    stblTagSize += movGetSttsTagSize(track);
    if (track->enc->codec_type == CODEC_TYPE_VIDEO)
        stblTagSize += movGetStssTagSize(track);
    stblTagSize += movGetStscTagSize(track);
    stblTagSize += movGetStszTagSize(track);
    stblTagSize += movGetStcoTagSize(track);
    return stblTagSize;
}

static int mov_write_stbl_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int stblTagSize = movGetStblTagSize(track);
    //offset_t pos = url_ftell(pb);
    put_be32_cache(mov, pb, stblTagSize); /* size */
    put_tag_cache(mov, pb, "stbl");
    mov_write_stsd_tag(pb, track);
    mov_write_stts_tag(pb, track);
    if (track->enc->codec_type == CODEC_TYPE_VIDEO)
        mov_write_stss_tag(pb, track);
    mov_write_stsc_tag(pb, track);
    mov_write_stsz_tag(pb, track);
    mov_write_stco_tag(pb, track);
    //return updateSize(pb, pos);
    return stblTagSize;
}

static unsigned int movGetDinfTagSize()
{
    unsigned int dinfTagSize = 8;
    dinfTagSize += movGetDrefTagSize();
    return dinfTagSize;
}

static int mov_write_dinf_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int dinfTagSize = movGetDinfTagSize();
    //offset_t pos = url_ftell(pb);
    put_be32_cache(mov, pb, dinfTagSize); /* size */
    put_tag_cache(mov, pb, "dinf");
    mov_write_dref_tag(pb, track);
    //return updateSize(pb, pos);
    return dinfTagSize;
}

static unsigned int movGetSmhdTagSize()
{
    return 16;
}
static int mov_write_smhd_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int smhdTagSize = movGetSmhdTagSize();
    put_be32_cache(mov, pb, smhdTagSize); /* size */
    put_tag_cache(mov, pb, "smhd");
    put_be32_cache(mov, pb, 0); /* version & flags */
    put_be16_cache(mov, pb, 0); /* reserved (balance, normally = 0) */
    put_be16_cache(mov, pb, 0); /* reserved */
    return smhdTagSize;
}


static unsigned int movGetGminTagSize()
{
    return 0x18;
}
static int mov_write_gmin_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int gminTagSize = movGetGminTagSize();
    put_be32_cache(mov, pb, gminTagSize); /* size */
    put_tag_cache(mov, pb, "gmin");
    put_be32_cache(mov, pb, 0); /* version & flags */
    put_be16_cache(mov, pb, 0x40); /* Ambarella define */
    put_be16_cache(mov, pb, 0x8000); /* reserved */
    put_be32_cache(mov, pb, 0x80008000);
    put_be16_cache(mov, pb, 0x0);
    put_be16_cache(mov, pb, 0x0);
    return gminTagSize;
}

static unsigned int movGetGmhdTextTagSize()
{
    return 0x2c;
}
static int mov_write_gmhd_text_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int gmhdTextTagSize = movGetGmhdTextTagSize();
    put_be32_cache(mov, pb, gmhdTextTagSize); /* size */
    put_tag_cache(mov, pb, "text");
    put_be32_cache(mov, pb, 0x01000000); /* copy from Ambarella */
    put_be32_cache(mov, pb, 0); /*  */
    put_be32_cache(mov, pb, 0); /*  */
    put_be32_cache(mov, pb, 0); /*  */
    put_be32_cache(mov, pb, 0x01000000); /*  */
    put_be32_cache(mov, pb, 0); /*  */
    put_be32_cache(mov, pb, 0); /*  */
    put_be32_cache(mov, pb, 0); /*  */
    put_be32_cache(mov, pb, 0x400000); /*  */
    return gmhdTextTagSize;
}

static unsigned int movGetGmhdTagSize()
{
    unsigned int gmhdTagSize = 8;
    gmhdTagSize += movGetGminTagSize();
    gmhdTagSize += movGetGmhdTextTagSize();
    return gmhdTagSize;
}
static int mov_write_gmhd_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int gmhdTagSize = movGetGmhdTagSize();
    put_be32_cache(mov, pb, gmhdTagSize); /* size */
    put_tag_cache(mov, pb, "gmhd");
    mov_write_gmin_tag(pb, track);
    mov_write_gmhd_text_tag(pb, track);
    return gmhdTagSize;
}


static unsigned int movGetVmhdTagSize()
{
    return 0x14;
}

static int mov_write_vmhd_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int vmhdTagSize = movGetVmhdTagSize();
    put_be32_cache(mov, pb, vmhdTagSize); /* size (always 0x14) */
    put_tag_cache(mov, pb, "vmhd");
    put_be32_cache(mov, pb, 0x01); /* version & flags */
    //put_be64(pb, 0); /* reserved (graphics mode = copy) */
	put_be32_cache(mov, pb, 0x0);
	put_be32_cache(mov, pb, 0x0);
    return vmhdTagSize;
}

static unsigned int movGetHdlrTagSize(MOVTrack *track)
{
    unsigned int hdlrTagSize = 32 + 1;
	
    if (!track) 
    { /* no media --> data handler */
        hdlrTagSize += strlen("DataHandler");
    } 
    else 
    {
        if (track->enc->codec_type == CODEC_TYPE_VIDEO) 
        {
            hdlrTagSize += strlen("VideoHandler");
        } 
        else if (track->enc->codec_type == CODEC_TYPE_AUDIO)
        {
            hdlrTagSize += strlen("SoundHandler");
        }
		else if (track->enc->codec_type == CODEC_TYPE_TEXT)
		{
            hdlrTagSize += strlen("TextHandler");
		}
    }
    return hdlrTagSize;
}

static int mov_write_hdlr_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = NULL;
    if(track)
    {
        mov = track->mov;
    }
    char *descr = NULL, *hdlr = NULL, *hdlr_type = NULL;
    //offset_t pos = url_ftell(pb);
	
    if (!track) { /* no media --> data handler */
        hdlr = "dhlr";
        hdlr_type = "url ";
        descr = "DataHandler";
    } else {
        hdlr = "\0\0\0\0";
        if (track->enc->codec_type == CODEC_TYPE_VIDEO) {
            hdlr_type = "vide";
            descr = "VideoHandler";
        } else if (track->enc->codec_type == CODEC_TYPE_AUDIO){
            hdlr_type = "soun";
            descr = "SoundHandler";
        } else if (track->enc->codec_type == CODEC_TYPE_TEXT){
            hdlr_type = "text";
            descr = "TextHandler";
        }
    }
	unsigned int hdlrTagSize = movGetHdlrTagSize(track);
    put_be32_cache(mov, pb, hdlrTagSize); /* size */
    put_tag_cache(mov, pb, "hdlr");
    put_be32_cache(mov, pb, 0); /* Version & flags */
    put_buffer_cache(mov, pb, (char*)hdlr, 4); /* handler */
    put_tag_cache(mov, pb, hdlr_type); /* handler type */
    put_be32_cache(mov, pb ,0); /* reserved */
    put_be32_cache(mov, pb ,0); /* reserved */
    put_be32_cache(mov, pb ,0); /* reserved */
    put_byte_cache(mov, pb, strlen((const char *)descr)); /* string counter */
    put_buffer_cache(mov, pb, (char*)descr, strlen((const char *)descr)); /* handler description */
    //return updateSize(pb, pos);
    return hdlrTagSize;
}

static unsigned int movGetMinfTagSize(MOVTrack *track)
{
    unsigned int minfTagSize = 8;
    if(track->enc->codec_type == CODEC_TYPE_VIDEO)
        minfTagSize += movGetVmhdTagSize();
    else if(track->enc->codec_type == CODEC_TYPE_AUDIO)
        minfTagSize += movGetSmhdTagSize();
	else if(track->enc->codec_type == CODEC_TYPE_TEXT)
        minfTagSize += movGetGmhdTagSize();

    minfTagSize += movGetDinfTagSize();
    minfTagSize += movGetStblTagSize(track);
    return minfTagSize;
}

static int mov_write_minf_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int minfTagSize = movGetMinfTagSize(track);
    //offset_t pos = url_ftell(pb);
    put_be32_cache(mov, pb, minfTagSize); /* size */
    put_tag_cache(mov, pb, "minf");
    if(track->enc->codec_type == CODEC_TYPE_VIDEO)
        mov_write_vmhd_tag(pb, track);
    else if(track->enc->codec_type == CODEC_TYPE_AUDIO)
        mov_write_smhd_tag(pb, track);
	else if(track->enc->codec_type == CODEC_TYPE_TEXT)
		mov_write_gmhd_tag(pb, track);
    mov_write_dinf_tag(pb, track);
    mov_write_stbl_tag(pb, track);
    //return updateSize(pb, pos);
    return minfTagSize;
}

static unsigned int movGetMdhdTagSize()
{
    return 0x20;
}

static int mov_write_mdhd_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
	unsigned int mdhdTagSize = movGetMdhdTagSize();
    put_be32_cache(mov, pb, mdhdTagSize); /* size */
    put_tag_cache(mov, pb, "mdhd");
    put_byte_cache(mov, pb, 0);
    put_be24_cache(mov, pb, 0); /* flags */
    
    put_be32_cache(mov, pb, track->time); /* creation time */
    put_be32_cache(mov, pb, track->time); /* modification time */
    put_be32_cache(mov, pb, track->timescale); /* time scale (sample rate for audio) */
    if(track->enc->codec_type == CODEC_TYPE_AUDIO)/* duration (using specific time coordinate system for track)*/
    {
        put_be32_cache(mov, pb, track->trackDuration/1000*track->timescale); 
    }
    else/* duration (using global time coordinate system) */
    {
        put_be32_cache(mov, pb, track->trackDuration); 
    }
    put_be16_cache(mov, pb, /*track->language*/0); /* language */
    put_be16_cache(mov, pb, 0); /* reserved (quality) */
	
    return mdhdTagSize;
}

static unsigned int movGetMdiaTagSize(MOVTrack *track)
{
    unsigned int mdiaTagSize = 8;
    mdiaTagSize += movGetMdhdTagSize();
    mdiaTagSize += movGetHdlrTagSize(track);
    mdiaTagSize += movGetMinfTagSize(track);
    return mdiaTagSize;
}
static int mov_write_mdia_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int mdiaTagSize = movGetMdiaTagSize(track);
    //offset_t pos = url_ftell(pb);
    put_be32_cache(mov, pb, mdiaTagSize); /* size */
    put_tag_cache(mov, pb, "mdia");
    mov_write_mdhd_tag(pb, track);
    mov_write_hdlr_tag(pb, track);
    mov_write_minf_tag(pb, track);
    //return updateSize(pb, pos);
    return mdiaTagSize;
}

static int av_rescale_rnd(int64_t a, int64_t b, int64_t c)
{
    return (a * b + c-1)/c;
}

static unsigned int movGetElstTagSize()
{
    unsigned int elstTagSize = 0x1c; // 16+12
    return elstTagSize;
}

static int mov_write_elst_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int elstTagSize = movGetElstTagSize();	// use global time coordinate system
    unsigned int trakDuration = track->trackDuration;//track->enc->frame_rate * track->stsz_size;
    put_be32_cache(mov, pb, elstTagSize); /* size */
    put_tag_cache(mov, pb, "elst");
    put_be32_cache(mov, pb, 0);
    put_be32_cache(mov, pb, 1);
    put_be32_cache(mov, pb, trakDuration);  // note
    put_be32_cache(mov, pb, 0x00000000);// note 
    put_be32_cache(mov, pb, 0x00010000);
    return elstTagSize;
}

static unsigned int movGetEdtsTagSize()
{
    unsigned int edtsTagSize = 8;
    edtsTagSize += movGetElstTagSize();
    return edtsTagSize;
}

static int mov_write_edts_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int edtsTagSize = movGetEdtsTagSize(track);
    put_be32_cache(mov, pb, edtsTagSize); /* size */
    put_tag_cache(mov, pb, "edts");
    mov_write_elst_tag(pb, track);
    return edtsTagSize;
}


static unsigned int movGetTkhdTagSize()
{
    return 0x5c;
}

static int mov_write_tkhd_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;        // use global time coordinate system
    int duration = track->trackDuration;//av_rescale_rnd(track->trackDuration, globalTimescale, track->timescale);
    int version = 0;
	unsigned int tkhdTagSize = movGetTkhdTagSize();
    put_be32_cache(mov, pb, tkhdTagSize); /* size */
    put_tag_cache(mov, pb, "tkhd");
    put_byte_cache(mov, pb, version);
    put_be24_cache(mov, pb, 0x3); /* flags (track enabled) */
    
    put_be32_cache(mov, pb, track->time); /* creation time */
    put_be32_cache(mov, pb, track->time); /* modification time */
    
    put_be32_cache(mov, pb, track->trackID); /* track-id */
    put_be32_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, duration);
	
    put_be32_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved (Layer & Alternate group) */
    /* Volume, only for audio */
    if(track->enc->codec_type == CODEC_TYPE_AUDIO)
        put_be16_cache(mov, pb, 0x0100);
    else
        put_be16_cache(mov, pb, 0);
    put_be16_cache(mov, pb, 0); /* reserved */
	
#if 1
    {
    	int degrees = track->enc->rotate_degree;
        unsigned int a = 0x00010000;
        unsigned int b = 0;
        unsigned int c = 0;
        unsigned int d = 0x00010000;
        switch (degrees) {
            case 0:
                break;
            case 90:
                a = 0;
                b = 0x00010000;
                c = 0xFFFF0000;
                d = 0;
                break;
            case 180:
                a = 0xFFFF0000;
                d = 0xFFFF0000;
                break;
            case 270:
                a = 0;
                b = 0xFFFF0000;
                c = 0x00010000;
                d = 0;
                break;
            default:
                aloge("Should never reach this unknown rotation");
                break;
        }

        put_be32_cache(mov, pb, a);           // a
        put_be32_cache(mov, pb, b);           // b
        put_be32_cache(mov, pb, 0);           // u
        put_be32_cache(mov, pb, c);           // c
        put_be32_cache(mov, pb, d);           // d
        put_be32_cache(mov, pb, 0);           // v
        put_be32_cache(mov, pb, 0);           // x
        put_be32_cache(mov, pb, 0);           // y
        put_be32_cache(mov, pb, 0x40000000);  // w
    }
#else
    /* Matrix structure */
    put_be32(pb, 0x00010000); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x00010000); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x40000000); /* reserved */
#endif
    /* Track width and height, for visual only */
    if(track->enc->codec_type == CODEC_TYPE_VIDEO) {
		put_be32_cache(mov, pb, track->enc->width*0x10000);
        put_be32_cache(mov, pb, track->enc->height*0x10000);
    }
    else {
        put_be32_cache(mov, pb, 0);
        put_be32_cache(mov, pb, 0);
    }
    return tkhdTagSize;
}

static unsigned int movGetTrakTagSize(MOVTrack *track)
{
    unsigned int trakTagSize = 8;
    trakTagSize += movGetTkhdTagSize();
    trakTagSize += movGetEdtsTagSize();
    trakTagSize += movGetMdiaTagSize(track);
    return trakTagSize;
}

static int mov_write_trak_tag(ByteIOContext *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    unsigned int trakTagSize = movGetTrakTagSize(track);
    //offset_t pos = url_ftell(pb);
    put_be32_cache(mov, pb, trakTagSize); /* size */
    put_tag_cache(mov, pb, "trak");
    mov_write_tkhd_tag(pb, track);
    mov_write_edts_tag(pb, track);
    mov_write_mdia_tag(pb, track);
    //return updateSize(pb, pos);
    return trakTagSize;
}

static unsigned int movGetMvhdTagSize()
{
    return 108;
}

static int mov_write_mvhd_tag(ByteIOContext *pb, MOVContext *mov)
{
    int maxTrackID = 1,i;
    int max_trk_dur = 0;

	for (i=0; i<mov->nb_streams; i++) {
        if(maxTrackID < mov->tracks[i].trackID)
            maxTrackID = mov->tracks[i].trackID;

        if(max_trk_dur <(int)mov->tracks[i].trackDuration)
        {
            max_trk_dur =(int)mov->tracks[i].trackDuration; 
        }
    }
    alogd("mp4_max_dur:%d",max_trk_dur);

    unsigned int mvhdTagSize = movGetMvhdTagSize();
	put_be32_cache(mov, pb, mvhdTagSize); /* size */
    put_tag_cache(mov, pb, "mvhd");
    put_byte_cache(mov, pb, 0);
    put_be24_cache(mov, pb, 0); /* flags */
	
    put_be32_cache(mov, pb, mov->create_time); /* creation time */
    put_be32_cache(mov, pb, mov->create_time); /* modification time */
    put_be32_cache(mov, pb, mov->timescale); /* timescale */
    put_be32_cache(mov, pb, max_trk_dur/*mov->tracks[0].trackDuration*/); /* duration of longest track */
	
    put_be32_cache(mov, pb, 0x00010000); /* reserved (preferred rate) 1.0 = normal */
    put_be16_cache(mov, pb, 0x0100); /* reserved (preferred volume) 1.0 = normal */
    put_be16_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, 0); /* reserved */
	
    /* Matrix structure */
    put_be32_cache(mov, pb, 0x00010000); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x00010000); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x40000000); /* reserved */
	
    put_be32_cache(mov, pb, 0); /* reserved (preview time) */
    put_be32_cache(mov, pb, 0); /* reserved (preview duration) */
    put_be32_cache(mov, pb, 0); /* reserved (poster time) */
    put_be32_cache(mov, pb, 0); /* reserved (selection time) */
    put_be32_cache(mov, pb, 0); /* reserved (selection duration) */
    put_be32_cache(mov, pb, 0); /* reserved (current time) */
    put_be32_cache(mov, pb, maxTrackID+1); /* Next track id */
    return mvhdTagSize;
}

static void writeLatitude(ByteIOContext *pb, MOVContext *mov, int degreex10000) {
    int isNegative = (degreex10000 < 0);
    char sign = isNegative? '-': '+';

    // Handle the whole part
    char str[9];
    int wholePart = degreex10000 / 10000;
    if (wholePart == 0) {
        snprintf(str, 5, "%c%.2d.", sign, wholePart);
    } else {
        snprintf(str, 5, "%+.2d.", wholePart);
    }

    // Handle the fractional part
    int fractionalPart = degreex10000 - (wholePart * 10000);
    if (fractionalPart < 0) {
        fractionalPart = -fractionalPart;
    }
    snprintf(&str[4], 5, "%.4d", fractionalPart);

    // Do not write the null terminator
	put_buffer_cache(mov, pb, (char *)str, 8);
}

// Written in +/- DDD.DDDD format
static void writeLongitude(ByteIOContext *pb, MOVContext *mov, int degreex10000) {
    int isNegative = (degreex10000 < 0);
    char sign = isNegative? '-': '+';

    // Handle the whole part
    char str[10];
    int wholePart = degreex10000 / 10000;
    if (wholePart == 0) {
        snprintf(str, 6, "%c%.3d.", sign, wholePart);
    } else {
        snprintf(str, 6, "%+.3d.", wholePart);
    }

    // Handle the fractional part
    int fractionalPart = degreex10000 - (wholePart * 10000);
    if (fractionalPart < 0) {
        fractionalPart = -fractionalPart;
    }
    snprintf(&str[5], 5, "%.4d", fractionalPart);

    // Do not write the null terminator
	put_buffer_cache(mov, pb, (char *)str, 9);
}

static unsigned int movGetUdtaTagSize()
{
    unsigned int size = 4+4+4+4  //(be32_cache, tag_cache, be32_cache, tag_cache)
                +4+8+9+1;      //(0x001215c7, latitude, longitude, 0x2F)
    return size;
}
static int mov_write_udta_tag(ByteIOContext *pb, MOVContext *mov)
{
    unsigned int udtaTagSize = movGetUdtaTagSize();
    //offset_t pos = url_ftell(pb);
	//offset_t pos1;
	
    put_be32_cache(mov, pb, udtaTagSize); /* size */

    put_tag_cache(mov, pb, "udta");

	//pos1 = url_ftell(pb);
	put_be32_cache(mov, pb, udtaTagSize-8);
	put_tag_cache(mov, pb, "\xA9xyz");

	/*
	 * For historical reasons, any user data start
	 * with "\0xA9", must be followed by its assoicated
	 * language code.
	 * 0x0012: text string length
	 * 0x15c7: lang (locale) code: en
	 */
	put_be32_cache(mov, pb, 0x001215c7);
	
	writeLatitude(pb, mov, mov->latitudex10000);
	writeLongitude(pb, mov, mov->longitudex10000);
	put_byte_cache(mov, pb, 0x2F);
	//updateSize(pb, pos1);

    //return updateSize(pb, pos);
    return udtaTagSize;
}

static unsigned int movGetGpsTagSize(MOVContext *mov)
{
    unsigned int gps_entry_size = 0;
    unsigned int size = 0;

    size += 8;
    size += 8;  // version,entries
    gps_entry_size = mov->gps_entry_buff_wt;

    size += (gps_entry_size *8);

    return size; 
}

static unsigned int movGetMoovTagSize(MOVContext *mov)
{
    int i;
    unsigned int size = 0;
    size += 8;  //size, "moov" tag,
    if(mov->geo_available && gps_pack_method==GPS_PACK_IN_TRACK)
    {
        size += movGetUdtaTagSize();
    }
    size += movGetMvhdTagSize();
    for (i=0; i<mov->nb_streams; i++)
    {
        if(mov->tracks[i].stsz_size> 0) 
        {
            size += movGetTrakTagSize(&(mov->tracks[i]));
        }
    }
    if(mov->geo_available && gps_pack_method==GPS_PACK_IN_MDAT)
    {
        size += movGetGpsTagSize(mov);
    }
    
    return size;
}

static int mov_write_free_tag(ByteIOContext *pb, MOVContext *mov, int size)
{
    put_be32_cache(mov, pb, size);
    put_tag_cache(mov, pb, "free");
    return size;
}

static int mov_write_skip_tag(ByteIOContext *pb, MOVContext *mov, int size)
{
    put_be32_cache(mov, pb, size);
    put_tag_cache(mov, pb, "skip");
    return size;
}

static int mov_write_thm_tag(ByteIOContext *pb, MOVContext *mov, int size)
{
    put_be32_cache(mov, pb, size);
    put_tag_cache(mov, pb, "thm ");
    return size;
}
static int mov_write_gps_tag(ByteIOContext *pb, MOVContext *mov)
{ 
    unsigned int gpsTagSize = 0;
    unsigned int version = 0x00000101;
    unsigned int entries = mov->gps_entry_buff_wt;
    GPS_ENTRY *gps_entry = NULL;
    int i = 0;
    
    gpsTagSize = movGetGpsTagSize(mov);
	put_be32_cache(mov, pb, gpsTagSize); /* size */
    put_tag_cache(mov, pb, "gps ");

    put_be32_cache(mov, pb, version);
    put_be32_cache(mov, pb, entries);

    for(i=0;i<entries;i++)
    {
        gps_entry = mov->gps_entry_buff + i;
        
        put_be32_cache(mov, pb, gps_entry->gps_data_pos);
        put_be32_cache(mov, pb, gps_entry->gps_data_len_hold);
    }

    return gpsTagSize; 
}

static int mov_write_moov_tag(ByteIOContext *pb, MOVContext *mov,
								AVFormatContext *s)
{
    int i;
    int moov_size;
    // unsigned int ending_maker[4] = {0xAABBAABB,0xCCDDCCDD,0xEEFFEEFF,0xABCDEFAB };

    moov_size = movGetMoovTagSize(mov);
    if (moov_size + 28 + 8 <= MOV_HEADER_RESERVE_SIZE) {
        //url_fseek(pb, mov->free_pos, SEEK_SET);
        //mov->mpFsWriter->fsSeek(mov->mpFsWriter, mov->free_pos, SEEK_SET);

        pb->fsSeek(pb, mov->free_pos, SEEK_SET);
    }
    else
    {
        if(s->file_repair_flag) // seek to correct position to write moov tag
        {
            pb->fsSeek(pb, (mov->mdat_size + mov->mdat_start_pos), SEEK_SET);
        }
    }

    put_be32_cache(mov, pb, moov_size); /* size placeholder*/
    put_tag_cache(mov, pb, "moov");
    mov->timescale = globalTimescale;

	if(mov->geo_available && gps_pack_method==GPS_PACK_IN_TRACK) {
		mov_write_udta_tag(pb, mov);
	}
	
    for (i=0; i<mov->nb_streams; i++) 
    {
        mov->tracks[i].time = mov->create_time;
        if(mov->tracks[i].enc->codec_type == CODEC_TYPE_VIDEO)
        {
            mov->tracks[i].trackDuration = (mov->tracks[i].last_pkt_pts - mov->tracks[i].first_pkt_pts)/1000 +
                            1000*1000/mov->tracks[i].enc->frame_rate;
        }
        else if(mov->tracks[i].enc->codec_type == CODEC_TYPE_AUDIO)
        {
            mov->tracks[i].trackDuration = (mov->tracks[i].last_pkt_pts - mov->tracks[i].first_pkt_pts)/1000 +
                                1000*mov->tracks[i].enc->frame_size/mov->tracks[i].enc->sample_rate;
        }
        alogd("mp4_dur_info:%d-%lld-%lld-%lld",i,mov->tracks[i].first_pkt_pts,mov->tracks[i].last_pkt_pts,mov->tracks[i].trackDuration);
        mov->tracks[i].trackID = i+1;
        mov->tracks[i].mov = mov;
        mov->tracks[i].stream_type = i;//Note: the actual meaning of stream_type here is stream index,using stream_type
                                       // instead of stream index just for history reason.
    }

    mov_write_mvhd_tag(pb, mov);
    for (i=0; i<mov->nb_streams; i++) 
	{
        if(mov->tracks[i].stsz_size> 0) {
            mov_write_trak_tag(pb, &(mov->tracks[i]));
        }
    }

    if(mov->geo_available && gps_pack_method==GPS_PACK_IN_MDAT)      // to write gps box
    {
        mov_write_gps_tag(pb,mov);
    }

    if (moov_size + 28 + 8 <= MOV_HEADER_RESERVE_SIZE) {
        mov_write_free_tag(pb, mov, MOV_HEADER_RESERVE_SIZE - moov_size - 28);
        
        pb->fsSeek(pb, (mov->mdat_size + mov->mdat_start_pos), SEEK_SET);

        if(NULL != mov->p_thm_pic_addr &&0 != mov->thm_pic_size)
        {
            mov_write_thm_tag(pb, mov, mov->thm_pic_size+8);  // add ending marker
            put_buffer_cache(mov, pb,mov->p_thm_pic_addr ,mov->thm_pic_size);
        }
        // mov_write_skip_tag(pb, mov, 16+8);  // add ending marker
        // put_buffer_cache(mov, pb, ending_maker,16);
    }
    else
    {
        if(NULL != mov->p_thm_pic_addr &&0 != mov->thm_pic_size)
        {
            mov_write_thm_tag(pb, mov, mov->thm_pic_size+8);  // add ending marker
            put_buffer_cache(mov, pb,mov->p_thm_pic_addr ,mov->thm_pic_size);
        }
        // mov_write_skip_tag(pb, mov, 16+8);  // add ending marker
        // put_buffer_cache(mov, pb, ending_maker,16);
    }
	
    //return updateSize(pb, pos);
    return moov_size;
}

void flush_payload_cache(MOVContext *mov, ByteIOContext *s)
{
//    int64_t tm1, tm2, tm3;
//    tm1 = CDX_GetNowUs();
    //mov->mpFsWriter->fsFlush(mov->mpFsWriter);
    s->fsFlush(s);
//    tm2 = CDX_GetNowUs();
//    alogd("[%dx%d] flush, [%lld]ms", mov->tracks[0].enc->width, mov->tracks[0].enc->height, (tm2-tm1)/1000);
}

static int mov_write_mdat_tag(ByteIOContext *pb, MOVContext *mov)
{
    mov->mdat_pos = MOV_HEADER_RESERVE_SIZE; //url_ftell(pb);
//	mov->mdat_start_pos = mov->mdat_pos + 8;
	mov->mdat_start_pos = mov->mdat_pos + 16;
    //ftruncate(fileno(pb), mov->mdat_pos);
    //url_fseek(pb, 0, SEEK_END);
//    mov->mpFsWriter->fsTruncate(mov->mpFsWriter, mov->mdat_pos);
//    mov->mpFsWriter->fsSeek(mov->mpFsWriter, 0, SEEK_END);
//    pb->fsTruncate(pb, mov->mdat_pos);
//    pb->fsSeek(pb, 0, SEEK_END);
    pb->fsSeek(pb, mov->mdat_pos, SEEK_SET);
    
    
    put_be32_cache(mov, pb, 8); /* size placeholder */ 
    put_tag_cache(mov, pb, "free");
    put_be32_cache(mov, pb, 0); /* size placeholder */
    put_tag_cache(mov, pb, "mdat");
    return 0;
}

/* TODO: This needs to be more general */
static int mov_write_ftyp_tag(ByteIOContext *pb, AVFormatContext *s)
{
    //offset_t pos = url_ftell(pb);
    int minor = 0x200;
	MOVContext *mov = s->priv_data;
    put_be32_cache(mov, pb, 28); /* size */
    put_tag_cache(mov, pb, "ftyp");             // 4byte
    put_tag_cache(mov, pb, "isom");             // major_brand
    put_be32_cache(mov, pb, minor);             // minor version 
	
    put_tag_cache(mov, pb, "isom");             // compatible_brands 12B
    put_tag_cache(mov, pb, "iso2");
	
    put_tag_cache(mov, pb, "mp41");
    return 0;//updateSize(pb, pos);
}

static void mov_write_extra_data(AVFormatContext *s)
{
    ByteIOContext *pb = s->mpFsWriter;
    MOVContext *mov = s->priv_data;
    MOV_META *p_mov_meta = &mov->mov_meta_info;
    int tmp_mdat_size_bkp = 0;

    // first write video extral info
    tmp_mdat_size_bkp = mov->mdat_size;
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    put_be32_cache(mov, pb, mov->frms_tag_max_interval); /* write the maximum time interval to file which is needed when do repair */
    mov->mdat_size += 4;
#endif

    put_be32_cache(mov, pb, p_mov_meta->v_strm_num); /* write valid video stram number */
    mov->mdat_size += 4;
    for(int i=0; i<p_mov_meta->v_strm_num; ++i)
    {
        put_buffer_cache(mov, pb,(char*)&p_mov_meta->v_strms_meta[i], sizeof(V_STRM_META_DATA)-4);//4:sz ofvos poin    
        mov->mdat_size += (sizeof(V_STRM_META_DATA)-4);
        if(0 != p_mov_meta->v_strms_meta[i].v_vos_len)
        {
            put_buffer_cache(mov, pb,p_mov_meta->v_strms_meta[i].v_vos_buff,p_mov_meta->v_strms_meta[i].v_vos_len);
            mov->mdat_size += p_mov_meta->v_strms_meta[i].v_vos_len;
        }
    }
    aloge("mp4_wt_v_extra_data_size:%d",mov->mdat_size-tmp_mdat_size_bkp);

    // second write audio extral info
    tmp_mdat_size_bkp = mov->mdat_size;
    put_be32_cache(mov, pb, p_mov_meta->a_strm_num); /* write valid audio stram number */
    mov->mdat_size += 4;
    if(p_mov_meta->a_strm_num > 2)
    {
        aloge("mp4_audio_strm_num:%d > 2, not suppot!!",p_mov_meta->a_strm_num);
    }
    for(int i=0; i<p_mov_meta->a_strm_num; ++i)
    {
        put_buffer_cache(mov, pb, (char*)&p_mov_meta->a_strms_meta[i], sizeof(A_STRM_META_DATA)-4);//4:sz ofvos poin    
        mov->mdat_size += (sizeof(A_STRM_META_DATA)-4);
        if(0 != p_mov_meta->a_strms_meta[i].a_vos_len)
        {
            put_buffer_cache(mov, pb,p_mov_meta->a_strms_meta[i].a_vos_buff,p_mov_meta->a_strms_meta[i].a_vos_len);
            mov->mdat_size += p_mov_meta->a_strms_meta[i].a_vos_len;
        }
    }
    aloge("mp4_wt_a_extra_data_size:%d",mov->mdat_size-tmp_mdat_size_bkp);

    // third write text extral info
    tmp_mdat_size_bkp = mov->mdat_size;
    put_be32_cache(mov, pb, p_mov_meta->t_strm_num); /* write valid text stram number */
    mov->mdat_size += 4;
    if(p_mov_meta->t_strm_num > 1)
    {
        aloge("mp4_text_strm_num:%d > 1, not suppot!!",p_mov_meta->t_strm_num);
    }   
    if(p_mov_meta->t_strm_num != 0)
    {
        put_buffer_cache(mov, pb, (char*)&p_mov_meta->t_strms_meta, sizeof(T_STRM_META_DATA)-4);//4:sz ofvos poin    
        mov->mdat_size += (sizeof(T_STRM_META_DATA)-4);
        if(0 != p_mov_meta->t_strms_meta.t_vos_len)
        {
            put_buffer_cache(mov, pb,p_mov_meta->t_strms_meta.t_vos_buff,p_mov_meta->t_strms_meta.t_vos_len);
            mov->mdat_size += p_mov_meta->t_strms_meta.t_vos_len;
        }
    }
    aloge("mp4_wt_t_extra_data_size:%d",mov->mdat_size-tmp_mdat_size_bkp);
}

int mov_write_header_fake(AVFormatContext *s)
{
    ByteIOContext *pb = s->mpFsWriter;
    MOVContext *mov = s->priv_data;
    MOV_META *p_mov_meta = &mov->mov_meta_info;
    int i;
	
    for(i=0; i<s->nb_streams; i++) {
        AVStream *st= s->streams[i];
        MOVTrack *track= &mov->tracks[i];
		
        track->enc = &st->codec;
        track->tag = mov_find_codec_tag(s, track);
    }

    mov->mdat_pos = MOV_HEADER_RESERVE_SIZE; //url_ftell(pb);
	mov->mdat_start_pos = mov->mdat_pos + 16;

    // first write video extral info
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    // put_be32_cache(mov, pb, mov->frms_tag_max_interval); // write the maximum time interval to file which is needed when do repair
    mov->mdat_size += 4;
#endif

    // put_be32_cache(mov, pb, p_mov_meta->v_strm_num); // write valid video stram number
    mov->mdat_size += 4;
    for(int i=0; i<p_mov_meta->v_strm_num; ++i)
    {
        mov->mdat_size += (sizeof(V_STRM_META_DATA)-4);
        if(0 != p_mov_meta->v_strms_meta[i].v_vos_len)
        {
            mov->mdat_size += p_mov_meta->v_strms_meta[i].v_vos_len;
        }
    }
    // second write audio extral info
    mov->mdat_size += 4;
    if(p_mov_meta->a_strm_num > 2)
    {
        aloge("mp4_audio_strm_num:%d > 2, not suppot!!",p_mov_meta->a_strm_num);
    }
    for(int i=0; i<p_mov_meta->a_strm_num; ++i)
    {
        mov->mdat_size += (sizeof(A_STRM_META_DATA)-4);
        if(0 != p_mov_meta->a_strms_meta[i].a_vos_len)
        {
            mov->mdat_size += p_mov_meta->a_strms_meta[i].a_vos_len;
        }
    }

    // third write text extral info
    mov->mdat_size += 4;
    if(p_mov_meta->t_strm_num > 1)
    {
        aloge("mp4_text_strm_num:%d > 1, not suppot!!",p_mov_meta->t_strm_num);
    }   
    if(p_mov_meta->t_strm_num != 0)
    {
        mov->mdat_size += (sizeof(T_STRM_META_DATA)-4);
        if(0 != p_mov_meta->t_strms_meta.t_vos_len)
        {
            mov->mdat_size += p_mov_meta->t_strms_meta.t_vos_len;
        }
    }

    //<-----

    mov->nb_streams = s->nb_streams;
    mov->nb_v_streams = s->nb_v_streams;
    mov->free_pos = 28;

    return 0;
}
int mov_write_header(AVFormatContext *s)
{
    ByteIOContext *pb = s->mpFsWriter;
    MOVContext *mov = s->priv_data;
    int i;
	
    mov_write_ftyp_tag(pb,s);
	
    for(i=0; i<s->nb_streams; i++) {
        AVStream *st= s->streams[i];
        MOVTrack *track= &mov->tracks[i];
		
        track->enc = &st->codec;
        track->tag = mov_find_codec_tag(s, track);
    }
    mov_write_free_tag(pb, mov, MOV_HEADER_RESERVE_SIZE - 28);
    //flush_payload_cache(mov, pb);

    mov_write_mdat_tag(pb, mov);

    if(s->file_add_repair_info_flag)
    {
        mov_write_extra_data(s);
    }

    //mov->time = s->timestamp + 0x7C25B080; //1970 based -> 1904 based
    mov->nb_streams = s->nb_streams;
    mov->nb_v_streams = s->nb_v_streams;
    mov->free_pos = 28;

    return 0;
}

//int writeCallbackPacket(AVFormatContext *handle, void *packet)
//{
//	AVPacket *pkt = (AVPacket *)packet;
//	CDXRecorderBsInfo bs_info;
//	int ret;
//
//	if (handle->OutStreamHandle == NULL) {
//		return -1;
//	}
//	RawPacketHeader *pkt_hdr = &handle->RawPacketHdr;
//	unsigned int val;
//	unsigned int idx = 0;
//
//	pkt_hdr->stream_type = pkt->stream_index == CODEC_TYPE_VIDEO ? RawPacketTypeVideo : RawPacketTypeAudio; //StreamIndex
//	pkt_hdr->size = pkt->size0 + pkt->size1;
//	pkt_hdr->pts = pkt->pts;
//
//	bs_info.total_size = 0;
//
//	bs_info.bs_count = 1;
//	bs_info.bs_data[0] = (char *)pkt_hdr;
//	bs_info.bs_size[0] = sizeof(RawPacketHeader);
//	bs_info.total_size += bs_info.bs_size[0];
//
//	if (pkt->size0) {
//		bs_info.bs_count++;
//		bs_info.bs_data[1] = (char *)pkt->data0;
//		bs_info.bs_size[1] = pkt->size0;
//		bs_info.total_size += bs_info.bs_size[1];
//	}
//
//	if (pkt->size1) {
//		bs_info.bs_count++;
//		bs_info.bs_data[2] = (char *)pkt->data1;
//		bs_info.bs_size[2] = pkt->size1;
//		bs_info.total_size += bs_info.bs_size[2];
//	}
//
//	ret = cdx_write2(&bs_info, handle->OutStreamHandle);
//	if(ret < 0) {
//		return -1;
//	}
//	return 0;
//}

static void printCacheSize(MOVContext *mov)
{
    int i;
    for(i=0;i<mov->nb_streams;i++)
    {
        alogd("mov->stsc_cache_size[%d] = [%d]kB", i, mov->stsc_cache_size[i]/1024);
        alogd("mov->stco_cache_size[%d] = [%d]kB", i, mov->stco_cache_size[i]/1024);
        alogd("mov->stsz_cache_size[%d] = [%d]kB", i, mov->stsz_cache_size[i]/1024);
        alogd("mov->stts_cache_size[%d] = [%d]kB", i, mov->stts_cache_size[i]/1024);
    }
}

static void mov_write_gps_packet(AVFormatContext *s, AVPacket *pkt)
{ 
    MOVContext *mov = s->priv_data;
    ByteIOContext *pb = s->mpFsWriter;
    MOVTrack *trk = NULL;
    unsigned int offset = 0;
    unsigned int data_size = 0;
    GPS_ENTRY *gps_entry = NULL;
    int size = pkt->size0 + pkt->size1;

    if(0==size || NULL==pkt->data0)
    {
        return;
    }

    offset = mov->mdat_size + mov->mdat_start_pos;
    data_size = 4 + 4 + 4 + 4+size;     /*free_box_size+free_box_tag+GPS marker + gps_data_len + gps_data*/

    gps_entry = mov->gps_entry_buff + mov->gps_entry_buff_wt;
    gps_entry->gps_data_pos = offset;
    gps_entry->gps_data_len_hold = data_size;

    mov->gps_entry_buff_wt ++;

    if(mov->gps_entry_buff_wt >= MOV_GPS_MAX_ENTRY_NUM)
    {
        mov->gps_entry_buff_wt = 0;
        aloge("fatal error,gps write overflow");
    }

    mov_write_free_tag(pb,mov,data_size); 
    put_tag_cache(mov, pb, "GPS "); 
    put_buffer_cache(mov, pb, (char*)&size, 4);
    put_buffer_cache(mov, pb, pkt->data0, pkt->size0);

    mov->mdat_size += data_size; 

#ifdef MP4_FILE_REPAIR_FAST_VERSION
    mov->frms_tag.frms_tag_size += data_size;
#endif

    return; 
}

static void mov_write_gps_packet_fake(AVFormatContext *s, AVPacket *pkt)
{ 
    MOVContext *mov = s->priv_data;
    ByteIOContext *pb = s->mpFsWriter;
    MOVTrack *trk = NULL;
    unsigned int offset = 0;
    unsigned int data_size = 0;
    GPS_ENTRY *gps_entry = NULL;
    int size = pkt->size0 + pkt->size1;


    offset = mov->mdat_size + mov->mdat_start_pos;
    data_size = 4 + 4 + 4 + 4+size;     /*free_box_size+free_box_tag+GPS marker + gps_data_len + gps_data*/

    if(offset != pkt->pos)
    {
        aloge("mp4_gps_data_pos_err:%d-%d",offset,pkt->pos);
    }

    gps_entry = mov->gps_entry_buff + mov->gps_entry_buff_wt;
    gps_entry->gps_data_pos = offset;
    gps_entry->gps_data_len_hold = data_size;

    mov->gps_entry_buff_wt ++;

    if(mov->gps_entry_buff_wt >= MOV_GPS_MAX_ENTRY_NUM)
    {
        mov->gps_entry_buff_wt = 0;
        aloge("fatal error,gps write overflow");
    }
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    mov->frms_tag.frms_tag_size += data_size;
#endif

    mov->mdat_size += data_size; 

    return; 
}
// special flag used to indicate current pkt writing is normal or speical case
int mov_write_packet_fake(AVFormatContext *s, AVPacket *pkt)
{
    MOVContext *mov = s->priv_data;
    ByteIOContext *pb = s->mpFsWriter;
    MOVTrack *trk = NULL;
    MOV_FRM_HDR frm_hdr;
    
	if (NULL == mov || NULL == s->mpFsWriter)
	{
		__wrn("in param is null\n");
		return -1;
	}
	
    if(mov->geo_available && mov->gps_strm_idx == pkt->stream_index && gps_pack_method==GPS_PACK_IN_MDAT)
    {
        pkt->size0 -= 16;   //16B: free_atom_size + free_atom_type + gps maker +gps_data_size
    }                       // pkt->size0 is the size of gps related data stored in mdat atom,
                            // -16 : convet the size to size of actual gps data.
    int size = pkt->size0 + pkt->size1;

	if(pkt->stream_index == -1)//last packet
	{
        if(mov->last_stream_index == -1) // fix segment fault bug,when no acutal pkt written
        {
            return 0;
        }
		*mov->cache_write_ptr[STSC_ID][mov->last_stream_index]++ = mov->stsc_cnt;
		mov->tracks[mov->last_stream_index].stsc_size++;
		//!!!! add protection
		return 0;
	}
        
    trk = &mov->tracks[pkt->stream_index];
    // -->write frm header start
    if(1)
    {
        memset(&frm_hdr,0,sizeof(MOV_FRM_HDR));

#ifdef MP4_FILE_REPAIR_FAST_VERSION
        struct tag_for_frms_tag_s tmp_tag_for_frms_tag;

        memset(&tmp_tag_for_frms_tag,0,sizeof(struct tag_for_frms_tag_s));

        mov->frms_tag.frm_hdrs[mov->frms_tag.frms_num] = frm_hdr;
        mov->frms_tag.frms_num ++;
        if(mov->frms_tag.frms_num >= MAX_FRM_HDRS_IN_FRMS_TAG)
        {
            aloge("mp4_file_repair_fast_version_frm_hdr_num_execeed_the_maximum:%d",mov->frms_tag.frms_num);
        }
        mov->frms_tag.frms_tag_size += sizeof(struct tag_for_frms_tag_s);

        mov->mdat_size += sizeof(struct tag_for_frms_tag_s);
        
        if(0 == mov->frms_tag_first_frm_pts_set)
        {
            if(mov->frms_tag_pts_strm_idx == pkt->stream_index)
            {
                mov->frms_tag_first_frm_pts = pkt->pts;
                mov->frms_tag_first_frm_pts_set = 1;
            }
        }
#else
        mov->mdat_size += sizeof(MOV_FRM_HDR);
#endif
    }
	// <---end of write frm header
    if(pkt->flags&AVPACKET_FLAG_THUMBNAIL)
    {
        // store thumbnail data
        if(pkt->size0) // backup thm pic data
        {
            if(NULL == mov->p_thm_pic_addr)
            {
                mov->p_thm_pic_addr = malloc(pkt->size0);
                if(NULL != mov->p_thm_pic_addr)
                {
                    memcpy(mov->p_thm_pic_addr,pkt->data0,pkt->size0);
                    mov->thm_pic_size = pkt->size0;
                }
                else
                {
                    mov->thm_pic_size = 0;
                    aloge("mp4_muxer_thm_buff_malloc_fail,s:%d",pkt->size0);
                }
            }
            else
            {
                aloge("mp4_muxer_thm_fatal_err,more than one thm,s:%d",pkt->size0);
            }
        }
#ifdef MP4_FILE_REPAIR_FAST_VERSION
        mov->frms_tag.frms_tag_size += mov->thm_pic_size;
#endif
        mov->mdat_size += mov->thm_pic_size;
        mov->last_thumbnail_pkt = 1; // record pkt flag
        return 0;
    }
	// For case one: if add repair info to mp4 file  -> record new file offset;
	// For case two: if do not add repair info, 
	//               and current pkt type is different with last one ->record new file offset.
    if(1)
    {
        if(trk->use_co64==0 && (mov->mdat_size + mov->mdat_start_pos) > 0xffffffff )
        {
			aloge("stco_need_co64:%d-%lld-%lld",pkt->stream_index,mov->mdat_size,(mov->mdat_size + mov->mdat_start_pos));
            trk->use_co64 = 1;
        }
		*mov->cache_write_ptr[STCO_ID][pkt->stream_index]++ = (mov->mdat_size + mov->mdat_start_pos)>>32; 
		*mov->cache_write_ptr[STCO_ID][pkt->stream_index]++ = (mov->mdat_size + mov->mdat_start_pos)&0xFFFFFFFF;
        long long tmp_off = mov->mdat_size + mov->mdat_start_pos; 
        // aloge("mp4_r_d:%d-%lld-%lld-%lld-%lld",pkt->stream_index,mov->mdat_start_pos,mov->mdat_size,mov->mdat_size-sizeof(struct tag_for_frms_tag_s),pkt->pos);
        if(tmp_off != pkt->pos)
        {
            aloge("mp4_pkt_pos_err,i:%d,%lld-%lld",pkt->stream_index,tmp_off,pkt->pos);
        }

		// aloge("mp4_sco:%d-%d-%llx",pkt->stream_index,trk->stco_size,(mov->mdat_size + mov->mdat_start_pos));
		trk->stco_size++;
		mov->stco_cache_size[pkt->stream_index]++;
		
		if(mov->last_stream_index >= 0)
		{
			*mov->cache_write_ptr[STSC_ID][mov->last_stream_index]++ = mov->stsc_cnt;
			mov->tracks[mov->last_stream_index].stsc_size++;
			mov->stsc_cache_size[mov->last_stream_index]++;

			if(mov->cache_write_ptr[STSC_ID][mov->last_stream_index] > 
					mov->cache_end_ptr[STSC_ID][mov->last_stream_index])
			{
                if(mov->cache_write_ptr[STSC_ID][mov->last_stream_index] != 
						mov->cache_end_ptr[STSC_ID][mov->last_stream_index] + 1)
                {
                    aloge("fatal error! stsc buf wrPtr[%p]!=endPtr[%p]+1", 
							mov->cache_write_ptr[STSC_ID][mov->last_stream_index], 
							mov->cache_end_ptr[STSC_ID][mov->last_stream_index]);
                }
				mov->cache_write_ptr[STSC_ID][mov->last_stream_index] = 
														mov->cache_start_ptr[STSC_ID][mov->last_stream_index];
			}

			if(mov->stsc_cache_size[mov->last_stream_index] >= STSC_CACHE_SIZE)
			{
				int ret;
				
				alogv("streamIndex[%d] stsc tinypage: %d, mov[%p]", pkt->stream_index, trk->stsc_tiny_pages, mov);
                if(!mov->fd_stsz[0])
                {
                    alogd("strm[%d] not create mp4 tmp file stsc, create now! mov[%p]", mov->last_stream_index, mov);
                    //printCacheSize(mov);
                    //not create mp4 tmp file, create now.
                    if(0!=movCreateTmpFile(mov))
                    {
                        aloge("fatal error! movCreateTmpFile() fail!");
						return -1;
                    }
                }

                struct cdx_stream_info *pStscStream = (struct cdx_stream_info*)mov->fd_stsc[mov->last_stream_index];
                pStscStream->seek(pStscStream, trk->stsc_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE, SEEK_SET);
                ret = pStscStream->write(mov->cache_read_ptr[STSC_ID][mov->last_stream_index],1,
                                                        MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE, pStscStream);
				if(ret != (MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE))
					return -1;

				trk->stsc_tiny_pages++;
				mov->stsc_cache_size[mov->last_stream_index] -= MOV_CACHE_TINY_PAGE_SIZE;
				mov->cache_read_ptr[STSC_ID][mov->last_stream_index] += MOV_CACHE_TINY_PAGE_SIZE;
				if(mov->cache_read_ptr[STSC_ID][mov->last_stream_index] >
						mov->cache_end_ptr[STSC_ID][mov->last_stream_index])
				{
                    if(mov->cache_read_ptr[STSC_ID][mov->last_stream_index] != 
							mov->cache_end_ptr[STSC_ID][mov->last_stream_index]+1)
                    {
                        aloge("fatal error! stsc buf rdPtr[%p]!=endPtr[%p]+1", 
								mov->cache_read_ptr[STSC_ID][mov->last_stream_index], 
								mov->cache_end_ptr[STSC_ID][mov->last_stream_index]);
                    }
					mov->cache_read_ptr[STSC_ID][mov->last_stream_index] = 
												mov->cache_start_ptr[STSC_ID][mov->last_stream_index];
				}
			}
		}

        if(mov->cache_write_ptr[STCO_ID][pkt->stream_index] > mov->cache_end_ptr[STCO_ID][pkt->stream_index])
        {
            if(mov->cache_write_ptr[STCO_ID][pkt->stream_index] != mov->cache_end_ptr[STCO_ID][pkt->stream_index]+1)
            {
                aloge("fatal error! stco buf wtPtr[%p]!=endPtr[%p]+1", 
													mov->cache_write_ptr[STCO_ID][pkt->stream_index], 
													mov->cache_end_ptr[STCO_ID][pkt->stream_index]);
            }
			mov->cache_write_ptr[STCO_ID][pkt->stream_index] = mov->cache_start_ptr[STCO_ID][pkt->stream_index];
		}
		
		if(mov->stco_cache_size[pkt->stream_index] >= STCO_CACHE_SIZE/2)
		{
            int ret;
			aloge("mp4_stco_tinyp:%d-%d-%d",pkt->stream_index,mov->stco_cache_size[pkt->stream_index],STCO_CACHE_SIZE/2);	
			alogv("streamIndex[%d] stco tinypage: %d, mov[%p]", pkt->stream_index, trk->stco_tiny_pages, mov);
			if(!mov->fd_stsz[0])
            {
                alogd("strm[%d] not create mp4 tmp file stco, create now! mov[%p]", pkt->stream_index, mov);
                //printCacheSize(mov);
                //not create mp4 tmp file, create now.
                if(0!=movCreateTmpFile(mov))
                {
                    aloge("fatal error! movCreateTmpFile() fail!");
					return -1;
                }
            }
            struct cdx_stream_info *pStcoStream = (struct cdx_stream_info*)mov->fd_stco[pkt->stream_index];
            pStcoStream->seek(pStcoStream, trk->stco_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2,SEEK_SET);
            ret = pStcoStream->write(mov->cache_read_ptr[STCO_ID][pkt->stream_index],1,
															MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2,pStcoStream);
			aloge("mp4_stco_tinyp_w:%x-%d",(unsigned int)(mov->cache_read_ptr[STCO_ID][pkt->stream_index]),
																				MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2);
			if(ret != (MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2))
			{
                aloge("fatal error!stco write fail:%d-%d!",ret,MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2);
				return -1;
			}

			trk->stco_tiny_pages++;
			mov->stco_cache_size[pkt->stream_index] -= MOV_CACHE_TINY_PAGE_SIZE;
			mov->cache_read_ptr[STCO_ID][pkt->stream_index] += MOV_CACHE_TINY_PAGE_SIZE*2;
            if(mov->cache_read_ptr[STCO_ID][pkt->stream_index] > mov->cache_end_ptr[STCO_ID][pkt->stream_index])
            {
                if(mov->cache_read_ptr[STCO_ID][pkt->stream_index] != mov->cache_end_ptr[STCO_ID][pkt->stream_index]+1)
                {
                    aloge("fatal error! stco buf rdPtr[%p]!=endPtr[%p]+1", 
																mov->cache_read_ptr[STCO_ID][pkt->stream_index], 
																mov->cache_end_ptr[STCO_ID][pkt->stream_index]);
                }
                mov->cache_read_ptr[STCO_ID][pkt->stream_index] = mov->cache_start_ptr[STCO_ID][pkt->stream_index];
            }
        }     
		
        mov->stsc_cnt = 0;
    }
	
    mov->last_stream_index = pkt->stream_index;
	
    if(mov->geo_available && mov->gps_strm_idx == pkt->stream_index && gps_pack_method==GPS_PACK_IN_MDAT)
    {
        mov_write_gps_packet_fake(s,pkt);
        return 0;
    }

	//uncompressed audio need not write stsz field, because sample size is fixed.
    if(s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_PCM)   
    {
        trk->stsz_size+=size/(trk->enc->channels*(trk->enc->bits_per_sample>>3)); //av stsz size
        mov->stsc_cnt+=size/(trk->enc->channels*(trk->enc->bits_per_sample>>3));
    }
    else
    {
        *mov->cache_write_ptr[STSZ_ID][pkt->stream_index]++ = size;
        trk->stsz_size++; //av stsz size
        mov->stsc_cnt++;
        mov->stsz_cache_size[pkt->stream_index]++;
    }

	if(mov->cache_write_ptr[STSZ_ID][pkt->stream_index] > mov->cache_end_ptr[STSZ_ID][pkt->stream_index])
	{
        if(mov->cache_write_ptr[STSZ_ID][pkt->stream_index] != mov->cache_end_ptr[STSZ_ID][pkt->stream_index]+1)
        {
            aloge("fatal error! stsz buf wtPtr[%p]!=endPtr[%p]+1", 
					mov->cache_write_ptr[STSZ_ID][pkt->stream_index], mov->cache_end_ptr[STSZ_ID][pkt->stream_index]);
        }
		mov->cache_write_ptr[STSZ_ID][pkt->stream_index] = mov->cache_start_ptr[STSZ_ID][pkt->stream_index];
	}


	if(mov->stsz_cache_size[pkt->stream_index] >= STSZ_CACHE_SIZE)
	{
		int ret;
				
		alogv("streamIndex[%d] stsz tinypage: %d, mov[%p]", pkt->stream_index, trk->stsz_tiny_pages, mov);
		if(!mov->fd_stsz[0])
        {
            alogd("strm[%d] not create mp4 tmp file stsz, create now! mov[%p]", pkt->stream_index, mov);
            //printCacheSize(mov);
            //not create mp4 tmp file, create now.
            if(0!=movCreateTmpFile(mov))
            {
                aloge("fatal error! movCreateTmpFile() fail!");
				return -1;
            }
        }
        struct cdx_stream_info *pStszStream = (struct cdx_stream_info*)mov->fd_stsz[pkt->stream_index];
        pStszStream->seek(pStszStream, trk->stsz_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,SEEK_SET);
        ret = pStszStream->write(mov->cache_read_ptr[STSZ_ID][pkt->stream_index],1,
													MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE, pStszStream);
		if(ret != (MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE))
			return -1;
		trk->stsz_tiny_pages++;
		mov->stsz_cache_size[pkt->stream_index] -= MOV_CACHE_TINY_PAGE_SIZE;
		mov->cache_read_ptr[STSZ_ID][pkt->stream_index] += MOV_CACHE_TINY_PAGE_SIZE;
		if(mov->cache_read_ptr[STSZ_ID][pkt->stream_index] > mov->cache_end_ptr[STSZ_ID][pkt->stream_index])
		{
            if(mov->cache_read_ptr[STSZ_ID][pkt->stream_index] != mov->cache_end_ptr[STSZ_ID][pkt->stream_index]+1)
            {
                aloge("fatal error! stsz buf rdPtr[%p]!=endPtr[%p]+1", 
						mov->cache_read_ptr[STSZ_ID][pkt->stream_index], 
						mov->cache_end_ptr[STSZ_ID][pkt->stream_index]);
            }
			mov->cache_read_ptr[STSZ_ID][pkt->stream_index] = mov->cache_start_ptr[STSZ_ID][pkt->stream_index];
		}
	}

    int bNeedSpecialWriteFlag = 0;
    int pkt_size = 0;
    if(pkt->stream_index < mov->nb_v_streams)//video packet
    {
        *mov->cache_write_ptr[STTS_ID][pkt->stream_index]++ = pkt->duration;
		
        trk->stts_size++; 
        mov->stts_cache_size[pkt->stream_index]++;
			
        if(mov->cache_write_ptr[STTS_ID][pkt->stream_index] > mov->cache_end_ptr[STTS_ID][pkt->stream_index])
    	{
            if(mov->cache_write_ptr[STTS_ID][pkt->stream_index] != mov->cache_end_ptr[STTS_ID][pkt->stream_index]+1)
            {
                aloge("fatal error! stts buf wtPtr[%p]!=endPtr[%p]+1", 
						mov->cache_write_ptr[STTS_ID][pkt->stream_index], 
						mov->cache_end_ptr[STTS_ID][pkt->stream_index]);
            }
    		mov->cache_write_ptr[STTS_ID][pkt->stream_index] = mov->cache_start_ptr[STTS_ID][pkt->stream_index];
    	}
    	
    	if(mov->stts_cache_size[pkt->stream_index] >= STTS_CACHE_SIZE)
    	{
            alogv("streamIndex[%d] stts tinypage: %d, mov[%p]", pkt->stream_index, trk->stsz_tiny_pages, mov);
    		int ret;
    		if(!mov->fd_stsz[0])
            {
                alogd("strm[%d] not create mp4 tmp file stts, create now! mov[%p]", pkt->stream_index, mov);
                if(0!=movCreateTmpFile(mov))
                {
                    aloge("fatal error! movCreateTmpFile() fail!");
					return -1;
                }
            }
            struct cdx_stream_info *pSttsStream = (struct cdx_stream_info*)mov->fd_stts[pkt->stream_index];
            pSttsStream->seek(pSttsStream, trk->stts_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,SEEK_SET);
            ret = pSttsStream->write(mov->cache_read_ptr[STTS_ID][pkt->stream_index],1,
											MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,pSttsStream);
    		if(ret != (MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE))
    			return -1;
    		trk->stts_tiny_pages++;
    		mov->stts_cache_size[pkt->stream_index] -= MOV_CACHE_TINY_PAGE_SIZE;
    		mov->cache_read_ptr[STTS_ID][pkt->stream_index] += MOV_CACHE_TINY_PAGE_SIZE;
    		if(mov->cache_read_ptr[STTS_ID][pkt->stream_index] > mov->cache_end_ptr[STTS_ID][pkt->stream_index])
    		{
                if(mov->cache_read_ptr[STTS_ID][pkt->stream_index] != mov->cache_end_ptr[STTS_ID][pkt->stream_index]+1)
                {
                    aloge("fatal error! stts buf rdPtr[%p]!=endPtr[%p]+1", 
							mov->cache_read_ptr[STTS_ID][pkt->stream_index], 
							mov->cache_end_ptr[STTS_ID][pkt->stream_index]);
                }
    			mov->cache_read_ptr[STTS_ID][pkt->stream_index] = mov->cache_start_ptr[STTS_ID][pkt->stream_index];
    		}
    	}

        if(pkt->flags & AVPACKET_FLAG_KEYFRAME)
        {
            mov->cache_keyframe_ptr[pkt->stream_index][trk->keyFrame_num] = trk->stts_size;
            trk->keyFrame_num++;
        }
    }
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    mov->frms_tag.frms_tag_size += size;
#endif
    mov->mdat_size += size;
    if(!trk->first_pts_set)
    {
        trk->first_pkt_pts = pkt->pts;
        trk->first_pts_set = 1;
    }
    trk->last_pkt_pts = pkt->pts;
    
#ifdef MP4_FILE_REPAIR_FAST_VERSION
    if(1 == mov->frms_tag_first_frm_pts_set && pkt->stream_index == mov->frms_tag_pts_strm_idx && 
            pkt->pts - mov->frms_tag_first_frm_pts >= mov->frms_tag_max_interval)
    {
        if(0 == mov->frms_tag_written_flag)
        {
            mov->last_frms_tag_pos = mov->mdat_size + mov->mdat_start_pos;
            mov->frms_tag.cur_frms_tag_pos = mov->mdat_size + mov->mdat_start_pos;
            mov->mdat_size += FRMS_TAG_INFO_HDR_SIZE;
            mov->frms_tag.frms_tag_size = FRMS_TAG_INFO_HDR_SIZE; // re-calculate the size info for next frms_tag
            
            mov->mdat_size += sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num;
            mov->frms_tag.frms_tag_size += sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num;

            mov->frms_tag_written_flag = 1;
        }
        else
        {
            mov->frms_tag.frms_tag_pos = mov->last_frms_tag_pos; 
            mov->frms_tag.cur_frms_tag_pos = mov->mdat_size + mov->mdat_start_pos;
            mov->last_frms_tag_pos = mov->mdat_size + mov->mdat_start_pos;

            mov->mdat_size += FRMS_TAG_INFO_HDR_SIZE;
            mov->frms_tag.frms_tag_size = FRMS_TAG_INFO_HDR_SIZE;
            
            mov->mdat_size += sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num;
            mov->frms_tag.frms_tag_size += sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num;
        }

        mov->frms_tag.frms_num = 0;
        mov->frms_tag_first_frm_pts = pkt->pts;   // update first frm pts for next frms_tag
    }
#endif
    return 0;
}
int mov_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    MOVContext *mov = s->priv_data;
    ByteIOContext *pb = s->mpFsWriter;
    MOVTrack *trk = NULL;
    int size = pkt->size0 + pkt->size1;
    unsigned char tmpStrmByte = 0;
	unsigned char tmpMjepgTrailer[2];
    MOV_FRM_HDR frm_hdr;
    
	if (NULL == mov || NULL == s->mpFsWriter)
	{
		__wrn("in param is null\n");
		return -1;
	}
	
	if(pkt->stream_index == -1)//last packet  
	{
        if(mov->last_stream_index == -1) // fix segment fault bug,when no acutal pkt written
        {
            return 0;
        }
		*mov->cache_write_ptr[STSC_ID][mov->last_stream_index]++ = mov->stsc_cnt;
		mov->tracks[mov->last_stream_index].stsc_size++;
		//!!!! add protection
		return 0;
	}
    /* if(2 == pkt->stream_index && gps_pack_method==GPS_PACK_IN_MDAT)      // pkt with gps info
    {
        mov_write_gps_packet(s,pkt);
		mov->last_stream_index = pkt->stream_index;
        return 0;
    } */
    trk = &mov->tracks[pkt->stream_index];
	if(s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_MJPEG && !(pkt->flags&AVPACKET_FLAG_THUMBNAIL)) {  /* gushiming compressed source */
		size += 2;
	}

    // -->write frm header start
    if(s->file_add_repair_info_flag)
    {
        memset(&frm_hdr,0,sizeof(MOV_FRM_HDR));
        frm_hdr.frm_strm_index = pkt->stream_index;
        if(pkt->stream_index < mov->nb_v_streams)
        {
            if (pkt->size0 >= 5)
            {
                tmpStrmByte = pkt->data0[4];
            }
            else if (pkt->size0 > 0)
            {
                tmpStrmByte = pkt->data1[4 - pkt->size0];
            }
            else
            {
            }

            if (s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_H264)
            {
                if((tmpStrmByte&0x1f) == 5)
                {
                    frm_hdr.frm_key_flag = 1;
                }
            }
            else if(s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_H265)
            {
                if(pkt->flags & AVPACKET_FLAG_KEYFRAME)
                {
                    frm_hdr.frm_key_flag = 1;
                }
            }
            else if (s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_MPEG4)
            {
                if((tmpStrmByte>>6) == 0)
                {
                    frm_hdr.frm_key_flag = 1;
                }
            }
            else if (s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_MJPEG)
            {  /* gushiming compressed source */
                if(mov->keyframe_num2 == 0) 
                {
                    frm_hdr.frm_key_flag = 1;
                }
                mov->keyframe_num2++;
                if(mov->keyframe_num2>= trk->enc->frame_rate/*25*/) {
                    mov->keyframe_num2= 0;
                }
            }
            else
            {
                aloge("err codec type!\n");
                return -1;
            }
        }
        
        if(pkt->flags&AVPACKET_FLAG_THUMBNAIL)
        {
            frm_hdr.frm_key_flag |= AVPACKET_FLAG_THUMBNAIL;
            pkt->stream_index = 0; 
        }
        frm_hdr.frm_pts = pkt->pts;
        frm_hdr.frm_duration = pkt->duration;
#ifdef MP4_FILE_REPAIR_FAST_VERSION
        frm_hdr.frm_off = mov->mdat_size + mov->mdat_start_pos+sizeof(struct tag_for_frms_tag_s);
#else
        frm_hdr.frm_off = mov->mdat_size + mov->mdat_start_pos+sizeof(MOV_FRM_HDR);
#endif
        
        // aloge("mp4_r_d_w:%d-%lld-%lld-%lld-%u",pkt->stream_index,mov->mdat_start_pos,mov->mdat_size+sizeof(struct tag_for_frms_tag_s),mov->mdat_size,frm_hdr.frm_off);
        if(mov->geo_available && mov->gps_strm_idx == pkt->stream_index && gps_pack_method==GPS_PACK_IN_MDAT)
        {
            int gps_size_in_mdat = size + 16; // special process for gps pkt.
            frm_hdr.frm_size = gps_size_in_mdat;
        }
        else
        {
            frm_hdr.frm_size = size;
        }
#ifdef MP4_FILE_REPAIR_FAST_VERSION
        struct tag_for_frms_tag_s tmp_tag_for_frms_tag;

        memset(&tmp_tag_for_frms_tag,0,sizeof(struct tag_for_frms_tag_s));
        tmp_tag_for_frms_tag.tag_frms_tag_flag = TAG_FOR_FRMS_TAG_FLAG;
        if(0 == mov->frms_tag_written_flag)
        {
            tmp_tag_for_frms_tag.last_frms_tag_pos = 0;
            mov->frms_tag.frms_tag_flag = FRMS_TAG_FLAG_FIRST; 
        }
        else
        {
            tmp_tag_for_frms_tag.last_frms_tag_pos = mov->last_frms_tag_pos;
            mov->frms_tag.frms_tag_flag = FRMS_TAG_FLAG_NORMAL; 
        }

        mov->frms_tag.frm_hdrs[mov->frms_tag.frms_num] = frm_hdr;
        mov->frms_tag.frms_num ++;
        if(mov->frms_tag.frms_num >= MAX_FRM_HDRS_IN_FRMS_TAG)
        {
            aloge("mp4_file_repair_fast_version_frm_hdr_num_execeed_the_maximum:%d",mov->frms_tag.frms_num);
        }
        mov->frms_tag.frms_tag_size += sizeof(struct tag_for_frms_tag_s);

        put_buffer_cache(mov, pb, (char *)&tmp_tag_for_frms_tag, sizeof(struct tag_for_frms_tag_s));
        mov->mdat_size += sizeof(struct tag_for_frms_tag_s);
        
        if(0 == mov->frms_tag_first_frm_pts_set)
        {
            if(mov->frms_tag_pts_strm_idx == pkt->stream_index)
            {
                mov->frms_tag_first_frm_pts = pkt->pts;
                mov->frms_tag_first_frm_pts_set = 1;
            }
        }
#else
        put_buffer_cache(mov, pb, (char *)&frm_hdr, sizeof(MOV_FRM_HDR));
        mov->mdat_size += sizeof(MOV_FRM_HDR);
#endif
    }
	// <---end of write frm header
    if(pkt->flags&AVPACKET_FLAG_THUMBNAIL)
    {
        // store and write thumbnail data
        if(pkt->size0) // backup thm pic data
        {
            if(NULL == mov->p_thm_pic_addr)
            {
                mov->p_thm_pic_addr = malloc(pkt->size0);
                if(NULL != mov->p_thm_pic_addr)
                {
                    memcpy(mov->p_thm_pic_addr,pkt->data0,pkt->size0);
                    mov->thm_pic_size = pkt->size0;
                }
                else
                {
                    mov->thm_pic_size = 0;
                    aloge("mp4_muxer_thm_buff_malloc_fail,s:%d",pkt->size0);
                }
            }
            else
            {
                aloge("mp4_muxer_thm_fatal_err,more than one thm,s:%d",pkt->size0);
            }
        }
        put_buffer_cache(mov, pb, (char *)pkt->data0, pkt->size0); // write thm 

#ifdef MP4_FILE_REPAIR_FAST_VERSION
        mov->frms_tag.frms_tag_size += mov->thm_pic_size;
#endif

        mov->mdat_size += mov->thm_pic_size;
        mov->last_thumbnail_pkt = 1; // record pkt flag
        return 0;
    }
	// For case one: if add repair info to mp4 file  -> record new file offset;
	// For case two: if do not add repair info, 
	//               and current pkt type is different with last one ->record new file offset.
    if(s->file_add_repair_info_flag || 
            (0==s->file_add_repair_info_flag && 
                (pkt->stream_index != mov->last_stream_index ||
                                               1==mov->last_thumbnail_pkt)))
    {
        if(trk->use_co64==0 && (mov->mdat_size + mov->mdat_start_pos) > 0xffffffff )
        {
			aloge("stco_need_co64:%d-%lld-%lld",pkt->stream_index,mov->mdat_size,(mov->mdat_size + mov->mdat_start_pos));
            trk->use_co64 = 1;
        }
		*mov->cache_write_ptr[STCO_ID][pkt->stream_index]++ = (mov->mdat_size + mov->mdat_start_pos)>>32; 
		*mov->cache_write_ptr[STCO_ID][pkt->stream_index]++ = (mov->mdat_size + mov->mdat_start_pos)&0xFFFFFFFF;

		// aloge("mp4_sco:%d-%d-%llx",pkt->stream_index,trk->stco_size,(mov->mdat_size + mov->mdat_start_pos));
		trk->stco_size++;
		mov->stco_cache_size[pkt->stream_index]++;
		
		if(mov->last_stream_index >= 0)
		{
			*mov->cache_write_ptr[STSC_ID][mov->last_stream_index]++ = mov->stsc_cnt;
			mov->tracks[mov->last_stream_index].stsc_size++;
			mov->stsc_cache_size[mov->last_stream_index]++;

			if(mov->cache_write_ptr[STSC_ID][mov->last_stream_index] > 
					mov->cache_end_ptr[STSC_ID][mov->last_stream_index])
			{
                if(mov->cache_write_ptr[STSC_ID][mov->last_stream_index] != 
						mov->cache_end_ptr[STSC_ID][mov->last_stream_index] + 1)
                {
                    aloge("fatal error! stsc buf wrPtr[%p]!=endPtr[%p]+1", 
							mov->cache_write_ptr[STSC_ID][mov->last_stream_index], 
							mov->cache_end_ptr[STSC_ID][mov->last_stream_index]);
                }
				mov->cache_write_ptr[STSC_ID][mov->last_stream_index] = 
														mov->cache_start_ptr[STSC_ID][mov->last_stream_index];
			}

			if(mov->stsc_cache_size[mov->last_stream_index] >= STSC_CACHE_SIZE)
			{
				int ret;
				
				alogv("streamIndex[%d] stsc tinypage: %d, mov[%p]", pkt->stream_index, trk->stsc_tiny_pages, mov);
                if(!mov->fd_stsz[0])
                {
                    alogd("strm[%d] not create mp4 tmp file stsc, create now! mov[%p]", mov->last_stream_index, mov);
                    //printCacheSize(mov);
                    //not create mp4 tmp file, create now.
                    if(0!=movCreateTmpFile(mov))
                    {
                        aloge("fatal error! movCreateTmpFile() fail!");
						return -1;
                    }
                }

                struct cdx_stream_info *pStscStream = (struct cdx_stream_info*)mov->fd_stsc[mov->last_stream_index];
                pStscStream->seek(pStscStream, trk->stsc_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE, SEEK_SET);
                ret = pStscStream->write(mov->cache_read_ptr[STSC_ID][mov->last_stream_index],1,
                                                        MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE, pStscStream);
				if(ret != (MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE))
					return -1;

				trk->stsc_tiny_pages++;
				mov->stsc_cache_size[mov->last_stream_index] -= MOV_CACHE_TINY_PAGE_SIZE;
				mov->cache_read_ptr[STSC_ID][mov->last_stream_index] += MOV_CACHE_TINY_PAGE_SIZE;
				if(mov->cache_read_ptr[STSC_ID][mov->last_stream_index] >
						mov->cache_end_ptr[STSC_ID][mov->last_stream_index])
				{
                    if(mov->cache_read_ptr[STSC_ID][mov->last_stream_index] != 
							mov->cache_end_ptr[STSC_ID][mov->last_stream_index]+1)
                    {
                        aloge("fatal error! stsc buf rdPtr[%p]!=endPtr[%p]+1", 
								mov->cache_read_ptr[STSC_ID][mov->last_stream_index], 
								mov->cache_end_ptr[STSC_ID][mov->last_stream_index]);
                    }
					mov->cache_read_ptr[STSC_ID][mov->last_stream_index] = 
												mov->cache_start_ptr[STSC_ID][mov->last_stream_index];
				}
			}
		}

        if(mov->cache_write_ptr[STCO_ID][pkt->stream_index] > mov->cache_end_ptr[STCO_ID][pkt->stream_index])
        {
            if(mov->cache_write_ptr[STCO_ID][pkt->stream_index] != mov->cache_end_ptr[STCO_ID][pkt->stream_index]+1)
            {
                aloge("fatal error! stco buf wtPtr[%p]!=endPtr[%p]+1", 
													mov->cache_write_ptr[STCO_ID][pkt->stream_index], 
													mov->cache_end_ptr[STCO_ID][pkt->stream_index]);
            }
			mov->cache_write_ptr[STCO_ID][pkt->stream_index] = mov->cache_start_ptr[STCO_ID][pkt->stream_index];
		}
		
		if(mov->stco_cache_size[pkt->stream_index] >= STCO_CACHE_SIZE/2)
		{
            int ret;
			aloge("mp4_stco_tinyp:%d-%d-%d",pkt->stream_index,mov->stco_cache_size[pkt->stream_index],STCO_CACHE_SIZE/2);	
			alogv("streamIndex[%d] stco tinypage: %d, mov[%p]", pkt->stream_index, trk->stco_tiny_pages, mov);
			if(!mov->fd_stsz[0])
            {
                alogd("strm[%d] not create mp4 tmp file stco, create now! mov[%p]", pkt->stream_index, mov);
                //printCacheSize(mov);
                //not create mp4 tmp file, create now.
                if(0!=movCreateTmpFile(mov))
                {
                    aloge("fatal error! movCreateTmpFile() fail!");
					return -1;
                }
            }
            struct cdx_stream_info *pStcoStream = (struct cdx_stream_info*)mov->fd_stco[pkt->stream_index];
            pStcoStream->seek(pStcoStream, trk->stco_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2,SEEK_SET);
            ret = pStcoStream->write(mov->cache_read_ptr[STCO_ID][pkt->stream_index],1,
															MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2,pStcoStream);
			aloge("mp4_stco_tinyp_w:%x-%d",(unsigned int)(mov->cache_read_ptr[STCO_ID][pkt->stream_index]),
																				MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2);
			if(ret != (MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2))
			{
                aloge("fatal error!stco write fail:%d-%d!",ret,MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE*2);
				return -1;
			}

			trk->stco_tiny_pages++;
			mov->stco_cache_size[pkt->stream_index] -= MOV_CACHE_TINY_PAGE_SIZE;
			mov->cache_read_ptr[STCO_ID][pkt->stream_index] += MOV_CACHE_TINY_PAGE_SIZE*2;
            if(mov->cache_read_ptr[STCO_ID][pkt->stream_index] > mov->cache_end_ptr[STCO_ID][pkt->stream_index])
            {
                if(mov->cache_read_ptr[STCO_ID][pkt->stream_index] != mov->cache_end_ptr[STCO_ID][pkt->stream_index]+1)
                {
                    aloge("fatal error! stco buf rdPtr[%p]!=endPtr[%p]+1", 
																mov->cache_read_ptr[STCO_ID][pkt->stream_index], 
																mov->cache_end_ptr[STCO_ID][pkt->stream_index]);
                }
                mov->cache_read_ptr[STCO_ID][pkt->stream_index] = mov->cache_start_ptr[STCO_ID][pkt->stream_index];
            }
        }     
		
        mov->stsc_cnt = 0;
    }
	
    mov->last_stream_index = pkt->stream_index;
	
    if(mov->geo_available && mov->gps_strm_idx == pkt->stream_index && gps_pack_method==GPS_PACK_IN_MDAT)
    {
        mov_write_gps_packet(s,pkt);
        return 0;
    }

	//uncompressed audio need not write stsz field, because sample size is fixed.
    if(s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_PCM)   
    {
        trk->stsz_size+=size/(trk->enc->channels*(trk->enc->bits_per_sample>>3)); //av stsz size
        mov->stsc_cnt+=size/(trk->enc->channels*(trk->enc->bits_per_sample>>3));
    }
    else
    {
        *mov->cache_write_ptr[STSZ_ID][pkt->stream_index]++ = size;
        trk->stsz_size++; //av stsz size
        mov->stsc_cnt++;
        mov->stsz_cache_size[pkt->stream_index]++;
    }

	if(mov->cache_write_ptr[STSZ_ID][pkt->stream_index] > mov->cache_end_ptr[STSZ_ID][pkt->stream_index])
	{
        if(mov->cache_write_ptr[STSZ_ID][pkt->stream_index] != mov->cache_end_ptr[STSZ_ID][pkt->stream_index]+1)
        {
            aloge("fatal error! stsz buf wtPtr[%p]!=endPtr[%p]+1", 
					mov->cache_write_ptr[STSZ_ID][pkt->stream_index], mov->cache_end_ptr[STSZ_ID][pkt->stream_index]);
        }
		mov->cache_write_ptr[STSZ_ID][pkt->stream_index] = mov->cache_start_ptr[STSZ_ID][pkt->stream_index];
	}


	if(mov->stsz_cache_size[pkt->stream_index] >= STSZ_CACHE_SIZE)
	{
		int ret;
				
		alogv("streamIndex[%d] stsz tinypage: %d, mov[%p]", pkt->stream_index, trk->stsz_tiny_pages, mov);
		if(!mov->fd_stsz[0])
        {
            alogd("strm[%d] not create mp4 tmp file stsz, create now! mov[%p]", pkt->stream_index, mov);
            //printCacheSize(mov);
            //not create mp4 tmp file, create now.
            if(0!=movCreateTmpFile(mov))
            {
                aloge("fatal error! movCreateTmpFile() fail!");
				return -1;
            }
        }
        struct cdx_stream_info *pStszStream = (struct cdx_stream_info*)mov->fd_stsz[pkt->stream_index];
        pStszStream->seek(pStszStream, trk->stsz_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,SEEK_SET);
        ret = pStszStream->write(mov->cache_read_ptr[STSZ_ID][pkt->stream_index],1,
													MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE, pStszStream);
		if(ret != (MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE))
			return -1;
		trk->stsz_tiny_pages++;
		mov->stsz_cache_size[pkt->stream_index] -= MOV_CACHE_TINY_PAGE_SIZE;
		mov->cache_read_ptr[STSZ_ID][pkt->stream_index] += MOV_CACHE_TINY_PAGE_SIZE;
		if(mov->cache_read_ptr[STSZ_ID][pkt->stream_index] > mov->cache_end_ptr[STSZ_ID][pkt->stream_index])
		{
            if(mov->cache_read_ptr[STSZ_ID][pkt->stream_index] != mov->cache_end_ptr[STSZ_ID][pkt->stream_index]+1)
            {
                aloge("fatal error! stsz buf rdPtr[%p]!=endPtr[%p]+1", 
						mov->cache_read_ptr[STSZ_ID][pkt->stream_index], 
						mov->cache_end_ptr[STSZ_ID][pkt->stream_index]);
            }
			mov->cache_read_ptr[STSZ_ID][pkt->stream_index] = mov->cache_start_ptr[STSZ_ID][pkt->stream_index];
		}
	}

    int bNeedSpecialWriteFlag = 0;
    int pkt_size = 0;
    if(pkt->stream_index < mov->nb_v_streams)//video packet
    {
        *mov->cache_write_ptr[STTS_ID][pkt->stream_index]++ = pkt->duration;
		
        trk->stts_size++; 
        mov->stts_cache_size[pkt->stream_index]++;
			
        if(mov->cache_write_ptr[STTS_ID][pkt->stream_index] > mov->cache_end_ptr[STTS_ID][pkt->stream_index])
    	{
            if(mov->cache_write_ptr[STTS_ID][pkt->stream_index] != mov->cache_end_ptr[STTS_ID][pkt->stream_index]+1)
            {
                aloge("fatal error! stts buf wtPtr[%p]!=endPtr[%p]+1", 
						mov->cache_write_ptr[STTS_ID][pkt->stream_index], 
						mov->cache_end_ptr[STTS_ID][pkt->stream_index]);
            }
    		mov->cache_write_ptr[STTS_ID][pkt->stream_index] = mov->cache_start_ptr[STTS_ID][pkt->stream_index];
    	}
    	
    	if(mov->stts_cache_size[pkt->stream_index] >= STTS_CACHE_SIZE)
    	{
            alogv("streamIndex[%d] stts tinypage: %d, mov[%p]", pkt->stream_index, trk->stsz_tiny_pages, mov);
    		int ret;
    		if(!mov->fd_stsz[0])
            {
                alogd("strm[%d] not create mp4 tmp file stts, create now! mov[%p]", pkt->stream_index, mov);
                if(0!=movCreateTmpFile(mov))
                {
                    aloge("fatal error! movCreateTmpFile() fail!");
					return -1;
                }
            }
            struct cdx_stream_info *pSttsStream = (struct cdx_stream_info*)mov->fd_stts[pkt->stream_index];
            pSttsStream->seek(pSttsStream, trk->stts_tiny_pages*MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,SEEK_SET);
            ret = pSttsStream->write(mov->cache_read_ptr[STTS_ID][pkt->stream_index],1,
											MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE,pSttsStream);
    		if(ret != (MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE))
    			return -1;
    		trk->stts_tiny_pages++;
    		mov->stts_cache_size[pkt->stream_index] -= MOV_CACHE_TINY_PAGE_SIZE;
    		mov->cache_read_ptr[STTS_ID][pkt->stream_index] += MOV_CACHE_TINY_PAGE_SIZE;
    		if(mov->cache_read_ptr[STTS_ID][pkt->stream_index] > mov->cache_end_ptr[STTS_ID][pkt->stream_index])
    		{
                if(mov->cache_read_ptr[STTS_ID][pkt->stream_index] != mov->cache_end_ptr[STTS_ID][pkt->stream_index]+1)
                {
                    aloge("fatal error! stts buf rdPtr[%p]!=endPtr[%p]+1", 
							mov->cache_read_ptr[STTS_ID][pkt->stream_index], 
							mov->cache_end_ptr[STTS_ID][pkt->stream_index]);
                }
    			mov->cache_read_ptr[STTS_ID][pkt->stream_index] = mov->cache_start_ptr[STTS_ID][pkt->stream_index];
    		}
    	}

        if (pkt->size0 >= 5)
        {
            tmpStrmByte = pkt->data0[4];
        }
        else if (pkt->size0 > 0)
        {
            tmpStrmByte = pkt->data1[4 - pkt->size0];
        }
        else
        {
        }

        if (s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_H264)
        {
            if((tmpStrmByte&0x1f) == 5)
            {
                mov->cache_keyframe_ptr[pkt->stream_index][trk->keyFrame_num] = trk->stts_size;
                trk->keyFrame_num++;
            }

        }
        else if(s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_H265)
        {
            if(pkt->flags & AVPACKET_FLAG_KEYFRAME)
            {
                mov->cache_keyframe_ptr[pkt->stream_index][trk->keyFrame_num] = trk->stts_size;
                trk->keyFrame_num++;
            }
        }
        else if (s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_MPEG4)
        {
            if((tmpStrmByte>>6) == 0)
            {
                mov->cache_keyframe_ptr[pkt->stream_index][trk->keyFrame_num] = trk->stts_size;
                trk->keyFrame_num++;
            }
        }
		else if (s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_MJPEG)
        {  /* gushiming compressed source */
        	if(mov->keyframe_num == 0) {
			    mov->cache_keyframe_ptr[pkt->stream_index][trk->keyFrame_num] = trk->stts_size;
                trk->keyFrame_num++;	
        	}
			mov->keyframe_num ++;
			if(mov->keyframe_num >= 25) {
				mov->keyframe_num = 0;
			}
        }
        else
        {
            aloge("err codec type!\n");
            return -1;
        }

        if((s->streams[pkt->stream_index]->codec.codec_id==CODEC_ID_H264 ||
					s->streams[pkt->stream_index]->codec.codec_id==CODEC_ID_H265) && 0 != size)
        {   
            pkt_size = av_bswap32(size-4);
            //memcpy(pkt->data0,&pkt_size,4);
            bNeedSpecialWriteFlag = 1;
        }
    }

#ifdef MP4_FILE_REPAIR_FAST_VERSION
    mov->frms_tag.frms_tag_size += size;
#endif
    mov->mdat_size += size;
    if(!trk->first_pts_set)
    {
        trk->first_pkt_pts = pkt->pts;
        trk->first_pts_set = 1;
    }
    trk->last_pkt_pts = pkt->pts;

    int bNeedSkipDataFlag = 0;
    int nSkipSize = 0;
	if(pkt->size0)
	{
        if(0 == bNeedSpecialWriteFlag)
        {
		    put_buffer_cache(mov, pb, pkt->data0, pkt->size0);
        }
        else
        {
            put_buffer_cache(mov, pb, (char*)&pkt_size, 4);
            if(pkt->size0 <= 4)
            {
                //alogd("Be careful! pkt->size0[%d]<=4", pkt->size0);
                bNeedSkipDataFlag = 1;
                nSkipSize = 4-pkt->size0;
            }
            else
            {
                put_buffer_cache(mov, pb, pkt->data0+4, pkt->size0-4);
            }
        }
	}
    if(pkt->size1)
    {
        if(0 == bNeedSkipDataFlag)
        {
            put_buffer_cache(mov, pb, pkt->data1, pkt->size1);    
        }
        else
        {
            if(pkt->size1<=nSkipSize)
            {
                aloge("fatal error! size1[%d]<=skipSize[%d], check code!", pkt->size1, nSkipSize);
            }
            put_buffer_cache(mov, pb, pkt->data1+nSkipSize, pkt->size1-nSkipSize);
        }
    }

	if(s->streams[pkt->stream_index]->codec.codec_id == CODEC_ID_MJPEG) {  /* gushiming compressed source */
		tmpMjepgTrailer[0] = 0xff;
		tmpMjepgTrailer[1] = 0xd9;
		put_buffer_cache(mov, pb, (char*)tmpMjepgTrailer, 2);	
        
        mov->mdat_size += 2;
#ifdef MP4_FILE_REPAIR_FAST_VERSION
        mov->frms_tag.frms_tag_size += 2;
#endif
	}

#ifdef MP4_FILE_REPAIR_FAST_VERSION
    if(1 == mov->frms_tag_first_frm_pts_set && pkt->stream_index == mov->frms_tag_pts_strm_idx && 
            pkt->pts - mov->frms_tag_first_frm_pts >= mov->frms_tag_max_interval)
    {
        if(0 == mov->frms_tag_written_flag)
        {
            // aloge("dbg_chk1:%lld-%lld-%lld-%u-%u",pkt->pts,mov->frms_tag_first_frm_pts,(pkt->pts-mov->frms_tag_first_frm_pts),mov->frms_tag_max_interval,mov->frms_tag.frms_num);
            mov->last_frms_tag_pos = mov->mdat_size + mov->mdat_start_pos;
            mov->frms_tag.cur_frms_tag_pos = mov->mdat_size + mov->mdat_start_pos;
            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.frms_tag_flag, 4);	
            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.frms_tag_size, 4);	
            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.frms_tag_pos, 4);	
            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.cur_frms_tag_pos, 4);	
            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.frms_num, 4);	
            mov->mdat_size += FRMS_TAG_INFO_HDR_SIZE;
            mov->frms_tag.frms_tag_size = FRMS_TAG_INFO_HDR_SIZE; // re-calculate the size info for next frms_tag
            
            put_buffer_cache(mov, pb, (char*)mov->frms_tag.frm_hdrs, sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num);	
            mov->mdat_size += sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num;
            mov->frms_tag.frms_tag_size += sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num;

            mov->frms_tag_written_flag = 1;
        }
        else
        {
            // aloge("dbg_chk2:%lld-%lld-%lld-%u-%u",pkt->pts,mov->frms_tag_first_frm_pts,(pkt->pts-mov->frms_tag_first_frm_pts),mov->frms_tag_max_interval,mov->frms_tag.frms_num);
            mov->frms_tag.frms_tag_pos = mov->last_frms_tag_pos; 
            mov->frms_tag.cur_frms_tag_pos = mov->mdat_size + mov->mdat_start_pos;
            if(mov->frms_tag.cur_frms_tag_pos-mov->frms_tag.frms_tag_pos != mov->frms_tag.frms_tag_size)
            {
                alogw("mp4_muxer_frms_tag_pos_size_miss_match:%u-%u-%u-%u",mov->frms_tag.cur_frms_tag_pos,
                                mov->frms_tag.frms_tag_pos,(mov->frms_tag.cur_frms_tag_pos-mov->frms_tag.frms_tag_pos),
                                mov->frms_tag.frms_tag_size);
            }
            mov->last_frms_tag_pos = mov->mdat_size + mov->mdat_start_pos;

            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.frms_tag_flag, 4);	
            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.frms_tag_size, 4);	
            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.frms_tag_pos, 4);	
            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.cur_frms_tag_pos, 4);	
            put_buffer_cache(mov, pb, (char*)&mov->frms_tag.frms_num, 4);	
            mov->mdat_size += FRMS_TAG_INFO_HDR_SIZE;
            mov->frms_tag.frms_tag_size = FRMS_TAG_INFO_HDR_SIZE;
            
            put_buffer_cache(mov, pb, (char*)mov->frms_tag.frm_hdrs, sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num);	
            mov->mdat_size += sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num;
            mov->frms_tag.frms_tag_size += sizeof(MOV_FRM_HDR)*mov->frms_tag.frms_num;
        }

        mov->frms_tag.frms_num = 0;
        mov->frms_tag_first_frm_pts = pkt->pts;   // update first frm pts for next frms_tag
    }
#endif
    return 0;
}

int mov_write_trailer(AVFormatContext *s)
{
    MOVContext *mov = s->priv_data;
    //ByteIOContext *pb = s->pb;
    ByteIOContext *pb = s->mpFsWriter;
    //ByteIOContext *pb_cache = s->pb_cache;
    offset_t filesize = 0;//moov_pos = url_ftell(pb);
	AVPacket pkt;
    int64_t file_len = 0;


	pkt.data0 = 0;
	pkt.size0 = 0;
	pkt.data1 = 0;
	pkt.size1 = 0;
	pkt.stream_index = -1;
	//flush_payload_cache(mov, pb);
	mov_write_packet(s, &pkt);//update last packet
    //CTM_FFLUSH(pb_cache);
    //flush_payload_cache(mov, pb);
    //usleep(10000);
    mov_write_moov_tag(pb, mov, s);
    //flush_payload_cache(mov, pb);
    //usleep(10000);
    //filesize = url_ftell(pb);
    /* Write size of mdat tag */
    //url_fseek(pb, mov->mdat_pos, SEEK_SET);
    //mov->mpFsWriter->fsSeek(mov->mpFsWriter, mov->mdat_pos, SEEK_SET);

    file_len =  pb->fsTell(pb);

	aloge("mp4_trail_mdat_size:%lld",mov->mdat_size);
    if(mov->mdat_size > 0xffffffff)
    { 
		aloge("mp4_upd_mdat_long");
        pb->fsSeek(pb, mov->mdat_pos, SEEK_SET);
        put_be32_cache(mov, pb, 1); /* size placeholder */ 
        put_tag_cache(mov, pb, "mdat"); 
        put_be32_cache(mov, pb, (mov->mdat_size+16)>>32); 
        put_be32_cache(mov, pb, (mov->mdat_size+16));
    }
    else
    {
		aloge("mp4_upd_mdat_nrml");
        pb->fsSeek(pb, mov->mdat_pos+8, SEEK_SET);
        put_be32_cache(mov, pb, mov->mdat_size+8);
    }
    flush_payload_cache(mov, pb);

    if(s->file_repair_flag)
    {
        alogd("file_len:%lld",file_len);
        pb->fsTruncate(pb,file_len); // truncate to actual file size
    }
    
	//fflush(pb);
	//sync();
    return filesize;
}

#define MOV_TMPFILE_DIR "/mnt/extsd/mp4tmp/"
#define MOV_TMPFILE_EXTEND_NAME ".buf"
static void makeMovTmpFullFilePath(char *pPath, int nPathSize, char *filetype, int nStreamIndex, void* nFileId)
{
    // /mnt/extsd/mov_stsz_muxer_0[0xaaaabbbb].buf
    if(!strcmp(filetype, "stsz"))
    {
        eLIBs_sprintf(pPath, MOV_TMPFILE_DIR"mov_stsz_muxer_%d[0x%p]"MOV_TMPFILE_EXTEND_NAME ,nStreamIndex, nFileId);
    }
    else if(!strcmp(filetype, "stts"))
    {
        eLIBs_sprintf(pPath, MOV_TMPFILE_DIR"mov_stts_muxer_%d[0x%p]"MOV_TMPFILE_EXTEND_NAME ,nStreamIndex, nFileId);
    }
    else if(!strcmp(filetype, "stco"))
    {
        eLIBs_sprintf(pPath, MOV_TMPFILE_DIR"mov_stco_muxer_%d[0x%p]"MOV_TMPFILE_EXTEND_NAME ,nStreamIndex, nFileId);
    }
    else if(!strcmp(filetype, "stsc"))
    {
        eLIBs_sprintf(pPath, MOV_TMPFILE_DIR"mov_stsc_muxer_%d[0x%p]"MOV_TMPFILE_EXTEND_NAME ,nStreamIndex, nFileId);
    }
    else
    {
        aloge("fatal error, mov tmp filetype[%s] is not support!", filetype);
    }
    return;
}

static __hdle openMovTmpFile(char *pPath)
{
    //int fd;
    //int ret;
    struct cdx_stream_info *pStream = NULL;
//    fd = open(pPath, O_CREAT|O_RDWR, 0777);
//	if(fd < 0)
//    {
//        aloge("opening file failed,filePath=%s\n", pPath);
//        return NULL;
//	}
    CedarXDataSourceDesc datasourceDesc;
    memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
    datasourceDesc.source_url = (char*)pPath;
    datasourceDesc.source_type = CEDARX_SOURCE_FILEPATH;
    datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
    pStream = create_outstream_handle(&datasourceDesc);
    if(NULL == pStream)
    {
        aloge("fatal error! create mov tmp file fail.");
    }
//	ret = fallocate(fd,0x01,0,4*1024*1024);
//	if (ret < 0)
//	{
//		ALOGV("fallocate failed try again\n");
//	}
//    FILE *pFile = fdopen(fd, "a+");
//    if(pFile == NULL) 
//    {
//        aloge("get file stream fail");
//        close(fd);
//    }
    return (__hdle)pStream;
}

int movCreateTmpFile(MOVContext *mov)
{
    int i;

    stream_mkdir(MOV_TMPFILE_DIR, 0777);
    for(i = 0;i < MAX_STREAMS_IN_FILE;i++)
    {
        if(!mov->fd_stsz[i])
        {
            makeMovTmpFullFilePath(mov->FilePath_stsz[i], MOV_BUF_NAME_LEN, "stsz", i, (void*)mov);
            mov->fd_stsz[i] = openMovTmpFile(mov->FilePath_stsz[i]);
            if(!mov->fd_stsz[i])
            {
                aloge("mp4_create_stsz_idx:%d_tmp_file_error = %d\n",i,-errno);
                return -1;
            }
        }

        if(!mov->fd_stts[i])
        {
            makeMovTmpFullFilePath(mov->FilePath_stts[i], MOV_BUF_NAME_LEN, "stts", i, (void*)mov);
            mov->fd_stts[i] = openMovTmpFile(mov->FilePath_stts[i]);
            if(!mov->fd_stts[i])
            {
                aloge("mp4_create_stts_idx:%d_tmp_file_error = %d\n",i,-errno);
                return -1;
            }
        }

        if(!mov->fd_stco[i])
        {
            makeMovTmpFullFilePath(mov->FilePath_stco[i], MOV_BUF_NAME_LEN, "stco", i, (void*)mov);
            mov->fd_stco[i] = openMovTmpFile(mov->FilePath_stco[i]);
            if(!mov->fd_stco[i])
            {
                aloge("mp4_create_stco_idx:%d_tmp_file_error = %d\n",i,-errno);
                return -1;
            }
        }

        if(!mov->fd_stsc[i])
        {
            makeMovTmpFullFilePath(mov->FilePath_stsc[i], MOV_BUF_NAME_LEN, "stsc", i, (void*)mov);
            mov->fd_stsc[i] = openMovTmpFile(mov->FilePath_stsc[i]);
            if(!mov->fd_stsc[i])
            {
                aloge("mp4_create_stsc_idx:%d_tmp_file_error = %d\n",i,-errno);
                return -1;
            }
        }
    }
    return 0;
}

