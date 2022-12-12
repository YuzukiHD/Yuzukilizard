// #include <CDX_LogNDebug.h>
#define LOG_TAG "AacMuxer.c"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <encoder_type.h>
#include <record_writer.h>

#include "AacMuxer.h"
#include <FsWriter.h>

#define ADTS_HEADER_SIZE 7

typedef struct AacContext {
    int         sample_rate;
    int         channels;
    char        adts[ADTS_HEADER_SIZE];
    void        *priv_data;
    //FILE        *pb;
    struct cdx_stream_info  *pb;
    unsigned int     mFallocateLen;
    FsWriter*   mpFsWriter;
    FsCacheMemInfo mCacheMemInfo;
    FSWRITEMODE mFsWriteMode;
    int     mFsSimpleCacheSize;
    unsigned int     mbSdcardDisappear;  //1:sdcard disappear, 0:sdcard is normal.
} AacContext;

static unsigned const sampleRateTable[16] = {
  96000, 88200, 64000, 48000,
  44100, 32000, 24000, 22050,
  16000, 12000, 11025, 8000,
  7350, 0, 0, 0
};

int getSampleRateIndex(unsigned int sampleRate)
{
    int i;
    for (i = 0; i < 16; i++) {
        if (sampleRate == sampleRateTable[i]) {
            return i;
        }
    }
    return -1;
}

int initADTSHeader(AacContext *s, unsigned int sampleRate, unsigned int channels)
{
    int sf_index;
    adts_header adts;
    //unsigned int frame_len;

    sf_index = getSampleRateIndex(sampleRate);
    if (sf_index < 0) {
        return -1;
    }

    adts.syncword = 0xfff;
    adts.id = 0;
    adts.layer = 0;
    adts.protection_absent = 1;
    adts.profile = 1;
    adts.sf_index = sf_index;
    adts.protection_absent = 1;
    adts.private_bit = 0;
    adts.channel_configuration = channels;
    adts.original = 0;
    adts.home = 0;
    adts.copyright_identification_bit = 0;
    adts.copyright_identification_start = 0;
    adts.adts_buffer_fullness = 0x7ff;
    adts.no_raw_data_blocks_in_frame = 0;

    memset(s->adts, 0, ADTS_HEADER_SIZE);

    s->adts[0]  = 0xff;

    s->adts[1]  = 0xf0;                                   //syncword
    s->adts[1] |= (adts.layer & 0x03) << 1;               //layer
    s->adts[1] |= (adts.protection_absent & 0x01);        //protection_absent

    s->adts[2]  = (adts.profile & 0x03) << 6;             //profile
    s->adts[2] |= (adts.sf_index & 0x0F) << 2;
    s->adts[2] |= (adts.private_bit & 0x01) << 1;
    s->adts[2] |= (adts.channel_configuration & 0x04) >> 2;
    
    s->adts[3]  = (adts.channel_configuration & 0x03) << 6;
    s->adts[3] |= (adts.original & 0x01) << 5;
    s->adts[3] |= (adts.home & 0x01) << 4;
    s->adts[3] |= (adts.copyright_identification_bit & 0x01) << 3;
    s->adts[3] |= (adts.copyright_identification_start & 0x01) << 2;
    //s->adts[3] |= (char)(frame_len >> 11) & 0x03;
    
    //s->adts[4]  = (char)(frame_len >> 3)& 0xFF;
    
    //s->adts[5] |= ((char)frame_len & 0x07) << 5;
    s->adts[5]  = (char)(adts.adts_buffer_fullness >> 6) & 0x1F;

    s->adts[6]  = (char)(adts.adts_buffer_fullness & 0x003F) << 2;
    s->adts[6] |= adts.no_raw_data_blocks_in_frame & 0x03;

    return 0;
}

int generateAdtsHeader(AacContext *s, unsigned int length)
{
	int frame_len = length + ADTS_HEADER_SIZE;

    s->adts[3] &= 0xFC;
    s->adts[3] |= (char)(frame_len >> 11)& 0x03;
    s->adts[4]  = (char)(frame_len >> 3) & 0xFF;
    s->adts[5] &= 0x1F;
    s->adts[5] |= (char)(frame_len & 0x0007) << 5;
    
    return 0;
}

int AacWriteExtraData(void *handle, unsigned char *vosData, unsigned int vosLen, unsigned int idx)
{
    //alogd("AacWriteExtraData");
    return 0;
}

int AacMuxerWriteHeader(void *handle)
{
    AacContext *s = (AacContext *)handle;
    char *pCache = NULL;
    unsigned int nCacheSize = 0;
    alogv("AacMuxerWriteHeader");
    if(s->pb)
    {
        FSWRITEMODE mode = s->mFsWriteMode;
        if(FSWRITEMODE_CACHETHREAD == mode)
        {
    		if (s->mCacheMemInfo.mCacheSize > 0 && s->mCacheMemInfo.mpCache != NULL)
    		{
    			mode = FSWRITEMODE_CACHETHREAD;
                pCache = s->mCacheMemInfo.mpCache;
                nCacheSize = s->mCacheMemInfo.mCacheSize;
    		}
    		else
    		{
                aloge("fatal error! not set cacheMemory but set mode FSWRITEMODE_CACHETHREAD! use FSWRITEMODE_DIRECT.");
                mode = FSWRITEMODE_DIRECT;
    		}
        }
        else if(FSWRITEMODE_SIMPLECACHE == mode)
        {
            pCache = NULL;
            nCacheSize = s->mFsSimpleCacheSize;
        }
        s->mpFsWriter = createFsWriter(mode, s->pb, pCache, nCacheSize, 0);
        if(NULL == s->mpFsWriter)
        {
            aloge("fatal error! create FsWriter() fail!");
            return -1;
        }
    }
    return 0;
}

int AacMuxerWriteTrailer(void *handle)
{
    //AacContext *s = (AacContext *)handle;

    return 0;
}

int AacMuxerWritePacket(void *handle, void *pkt)
{
    AacContext *s = (AacContext *)handle;
    AVPacket *avpkt = (AVPacket *)pkt;
    if (s->mpFsWriter) 
    {
        generateAdtsHeader(s, avpkt->size0);
        s->mpFsWriter->fsWrite(s->mpFsWriter, (char*)s->adts, ADTS_HEADER_SIZE);
        s->mpFsWriter->fsWrite(s->mpFsWriter, avpkt->data0, avpkt->size0);
        return 0;
    }
    return -1;
}

int AacMuxerIoctrl(void *handle, unsigned int uCmd, unsigned int uParam, void *pParam2)
{
    AacContext *s = (AacContext *)handle;
	_media_file_inf_t *pMediaInf = 0;

    switch (uCmd) {
    case SETTOTALTIME:
        break;
    case SETFALLOCATELEN:
        s->mFallocateLen = uParam;
        break;
    case SETCACHEFD:
    {
        //s->pb = (FILE *)uParam;
        CedarXDataSourceDesc datasourceDesc;
        memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
        datasourceDesc.source_url = (char*)pParam2;
        datasourceDesc.source_type = CEDARX_SOURCE_FILEPATH;
        datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
        s->pb = create_outstream_handle(&datasourceDesc);
        if(NULL == s->pb)
        {
            aloge("fatal error! create aac outstream fail.");
            return -1;
        }
		break;
    }
    case SETCACHEFD2:
    {
        CedarXDataSourceDesc datasourceDesc;
        memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
        datasourceDesc.ext_fd_desc.fd = (int)uParam;
        datasourceDesc.source_type = CEDARX_SOURCE_FD;
        datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
        s->pb = create_outstream_handle(&datasourceDesc);
        if(NULL == s->pb)
        {
            aloge("fatal error! create aac outstream fail.");
            return -1;
        }
        if(s->mFallocateLen > 0)
        {
            if(s->pb->fallocate(s->pb, 0x01, 0, s->mFallocateLen) < 0)
            {
                aloge("fatal error! Failed to fallocate size %d, fd[%d](%s)", s->mFallocateLen, s->pb->fd_desc.fd, strerror(errno));
            }
        }
		break;
    }
    case SETOUTURL:
        aloge("DO not support set URL");
        break;
    case SETAVPARA:
		pMediaInf = (_media_file_inf_t *)pParam2;
		s->channels = pMediaInf->channels;
		s->sample_rate = pMediaInf->sample_rate;
        alogd("SETAVPARA: pMediaInf->sample_rate(%d), pMediaInf->channels(%d)",
            pMediaInf->sample_rate, pMediaInf->channels);
        initADTSHeader(s, s->sample_rate, s->channels);
        break;
    case SETSDCARDSTATE:
        s->mbSdcardDisappear = !uParam;
        alogd("SETSDCARDSTATE, mbSdcardDisappear[%d]", s->mbSdcardDisappear);
        break;
    case SETCACHEMEM:
        s->mCacheMemInfo = *(FsCacheMemInfo*)pParam2;
        break;
    case SET_FS_WRITE_MODE:
        s->mFsWriteMode = (FSWRITEMODE)uParam;
        break;
    case SET_FS_SIMPLE_CACHE_SIZE:
        s->mFsSimpleCacheSize = (int)uParam;
        break;
    default:
        break;
    }

    return 0;
}

void *AacMuxerOpen(int *ret)
{
    AacContext *s;
    //int i;

    alogd("AacMuxerOpen");

    *ret = 0;

    s = (AacContext *)malloc(sizeof(AacContext));
    if(!s)
    {   
        *ret = -1;
        return NULL;
    }
    
    memset(s,0,sizeof(AacContext));

    return (void*)s;
}

int AacMuxerClose(void *handle)
{
    AacContext *s = (AacContext *)handle;

    if(s->mpFsWriter)
    {
        destroyFsWriter(s->mpFsWriter);
        s->mpFsWriter = NULL;
    }
    if(s->pb)
    {
        destroy_outstream_handle(s->pb);
        s->pb = NULL;
    }
    if(s->priv_data) {
        free(s->priv_data);
        s->priv_data = NULL;
    }

    if (s) {
        free(s);
        s = NULL;
    }
    return 0;
}


CDX_RecordWriter record_writer_aac = {
    .info                = "recode write aac"   ,
    .MuxerOpen           = AacMuxerOpen         ,
    .MuxerClose          = AacMuxerClose        ,
    .MuxerWriteExtraData = AacWriteExtraData    ,
    .MuxerWriteHeader    = AacMuxerWriteHeader  ,
    .MuxerWriteTrailer   = AacMuxerWriteTrailer ,
    .MuxerWritePacket    = AacMuxerWritePacket  ,
    .MuxerIoctrl         = AacMuxerIoctrl       ,
};

