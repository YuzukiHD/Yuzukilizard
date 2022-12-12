// #include <CDX_LogNDebug.h>
#define LOG_TAG "Mp3Muxer.c"
#include <utils/plat_log.h>
//#include <CDX_Recorder.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <encoder_type.h>
//#include <cedarx_stream.h>
//#include <CDX_Types.h>
//#include "cedarv_osal_linux.h"
//#include <recorde_writer.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

//#include "Mp3Muxer.h"
#include "mp3.h"
//#include <ConfigOption.h>

int Mp3WriteExtraData(void *handle, unsigned char *vosData, unsigned int vosLen, unsigned int idx)
{
    //alogd("Mp3WriteExtraData");
    return 0;
}

int Mp3MuxerWriteHeader(void *handle)
{
    AVFormatContext *s = (AVFormatContext *)handle;
    char *pCache = NULL;
    unsigned int nCacheSize = 0;
    alogv("Mp3MuxerWriteHeader");
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
    if(s->mpFsWriter)
    {
        return mp3_write_header(s);
    }
    return -1;
}

int Mp3MuxerWriteTrailer(void *handle)
{
    AVFormatContext *s = (AVFormatContext *)handle;
    if (s->mpFsWriter) 
    {
        return mp3_write_trailer(s);
    }
    return -1;
}

int Mp3MuxerWritePacket(void *handle, void *pkt)
{
    AVFormatContext *s = (AVFormatContext *)handle;
    if (s->mpFsWriter) 
    {
        return mp3_write_packet(s, (AVPacket *)pkt);
    }
    return -1;
}

int Mp3MuxerIoctrl(void *handle, unsigned int uCmd, unsigned int uParam, void *pParam2)
{
    AVFormatContext *s = (AVFormatContext *)handle;

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

void *Mp3MuxerOpen(int *ret)
{
    AVFormatContext *s;
    //int i;

    alogv("Mp3MuxerOpen");

    *ret = 0;

    s = (AVFormatContext *)malloc(sizeof(AVFormatContext));
    if(!s)
    {   
        *ret = -1;
        return NULL;
    }
    
    memset(s,0,sizeof(AVFormatContext));

    return (void*)s;
}

int Mp3MuxerClose(void *handle)
{
    AVFormatContext *s = (AVFormatContext *)handle;
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


CDX_RecordWriter record_writer_mp3 = {
    .info                = "recode write mp3"   ,
    .MuxerOpen           = Mp3MuxerOpen         ,
    .MuxerClose          = Mp3MuxerClose        ,
    .MuxerWriteExtraData = Mp3WriteExtraData    ,
    .MuxerWriteHeader    = Mp3MuxerWriteHeader  ,
    .MuxerWriteTrailer   = Mp3MuxerWriteTrailer ,
    .MuxerWritePacket    = Mp3MuxerWritePacket  ,
    .MuxerIoctrl         = Mp3MuxerIoctrl       ,
};

