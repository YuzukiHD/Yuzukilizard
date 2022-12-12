/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmsHttpStream.c
 * Description : MmsHttpStream
 * History :
 *
 */

#include "CdxMmsStream.h"
#include <string.h>

//#define LOG_TAG "aw_mms_http_stream.c"

extern const char asfStreamHeaderGuid[16];    /*ASF_Stream_Properties_Object*/
extern const char asfFileHeaderGuid[16];     /*ASF_File_Properties_Object */
extern const char asfStreamGroupGuid[16];  /*ASF_Stream_Bitrate_Properties_Object*/

extern int MmshGetMaxIdx(int sCount, int *sRates, int bound) ;
extern int MmshFindAsfGuid(char *buf, const char *guid, int curPos, int bufLen);
extern int AsfStreamingType(aw_asf_stream_chunck_t *streamChunck, int *dropPacket);

int aw_mms_http_streaming_start(CdxDataSourceT* datasource, aw_mms_inf_t* mmsStreamInf);
int aw_mms_http_streaming_read_func(aw_mms_inf_t* mmsStreamInf);
int aw_mms_http_streaming_read(char *buffer, int size, aw_mms_inf_t* mmsStreamInf);
int aw_asf_http_read_wrapper(void *buffer, int len, aw_mms_inf_t* mmsStreamInf);
int aw_mms_http_streaming_seek(aw_mms_inf_t* mmsStreamInf);

int aw_mms_http_streaming_start(CdxDataSourceT* datasource, aw_mms_inf_t* mmsStreamInf)
{
    aw_asf_stream_chunck_t chunk ;
    aw_mmsh_ctrl_t* asfCtrl = NULL;
    //HttpCoreStreamInfo *coreStreamInfo = NULL;
    int* vRates = NULL;
    int* aRates = NULL;

    int start;
    int pos;
    int i;

    asfCtrl = (aw_mmsh_ctrl_t*)malloc(sizeof(aw_mmsh_ctrl_t));
    if(asfCtrl==NULL)
    {
        CDX_LOGV("*******************memAlloc for asfHttpCtrl failed.\n");
        goto out;
    }
    asfCtrl->streamingType = ASF_TYPE_UNKNOWN;
    asfCtrl->request = 1;
    asfCtrl->audioStreams = NULL;
    asfCtrl->videoStreams = NULL;
    asfCtrl->nAudio = 0;
    asfCtrl->nVideo = 0;
    mmsStreamInf->data = (void*)asfCtrl;
    int num = 0;

    CdxHttpHeaderFieldT hdr[7] = {{"Accept","*/*"},
                                {"User-Agent","NSPlayer/4.1.0.3856"},
                                {"Pragma","xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}"},
                                {"Pragma","no-cache,rate=1.000000,stream-time=0,\
                                stream-offset=0:0,request-context=2,max-duration=0"},
                                {"Pragma"," xPlayStrm=1"},
                                {"Pragma","stream-switch-entry=ffff:1:0 ffff:2:0"},
                                {"Pragma","stream-switch-count=2"}};

    CdxHttpHeaderFieldsT header;
    header.num = 7;
    header.pHttpHeader = hdr;
    datasource->extraData = (void*)&header;

    int ret = CdxStreamOpen(datasource, NULL, &mmsStreamInf->exitFlag,
                     (CdxStreamT**)&mmsStreamInf->httpCore, NULL);
    if(ret < 0)
    {
        CDX_LOGE("CdxStreamOpen fail");
        goto out;
    }

    num = CdxStreamRead(mmsStreamInf->httpCore, &chunk, sizeof(aw_asf_stream_chunck_t));
    if(num < (int)sizeof(aw_asf_stream_chunck_t))
    {
        CDX_LOGE("read first chunk header error.****num = %d\n", num);
        goto out;
    }
    if( (chunk.size < 8) || (chunk.size!=chunk.sizeConfirm) )
    {
        CDX_LOGE("error. chunk.size = %d, chunk.sizeConfirm = %d.\n",
              chunk.size, chunk.sizeConfirm);
        goto out;
    }
    if(chunk.type != ASF_STREAMING_HEADER)
    {
        CDX_LOGE("the first chunk is not the asf header object.\n");
        goto out;
    }

    mmsStreamInf->firstChunkBodySize = chunk.size + 4;  //chunk size(including chunk header)
    mmsStreamInf->firstChunk = (char*)malloc(mmsStreamInf->firstChunkBodySize);
    mmsStreamInf->firstChunkPos = 0;
    memcpy(mmsStreamInf->firstChunk, &chunk, 12);
    if(NULL == mmsStreamInf->firstChunk)
        goto out;

    num = CdxStreamRead(mmsStreamInf->httpCore, mmsStreamInf->firstChunk+12,
                       mmsStreamInf->firstChunkBodySize-12);
    if(num < 0)
    {
        CDX_LOGE("****<%s, %d>******HttpCoreRead error!\n", __FILE__, __LINE__);
        goto out;
    }
    if(num < mmsStreamInf->firstChunkBodySize)
    {
        CDX_LOGW("****<%s, %d>******the data red is not enough.\n", __FILE__, __LINE__);
    }

    //******** parse the ASF_Header_Object*****//
    start = sizeof(aw_asf_header_t);
    pos = MmshFindAsfGuid(mmsStreamInf->firstChunk, asfFileHeaderGuid, start,
                        mmsStreamInf->firstChunkBodySize);
   /*ASF_File_Properties_Object GUID and object size*/
    if(pos >= 0)
    {
        aw_asf_file_header_t *fileh = (aw_asf_file_header_t *) &mmsStreamInf->firstChunk[pos];
        pos += sizeof(aw_asf_file_header_t);
        if (pos > mmsStreamInf->firstChunkBodySize)
        {
            goto out;
        }
        asfCtrl->packetSize = GetLE32(&fileh->maxPacketSize);
        mmsStreamInf->fileSize  = GetLE64(&fileh->fileSize);
        mmsStreamInf->fileDuration = (GetLE64(&fileh->playDuration)/10000);
    }

    pos = start;
    while((pos = MmshFindAsfGuid(mmsStreamInf->firstChunk, asfStreamHeaderGuid, pos,
         mmsStreamInf->firstChunkBodySize)) >= 0)
    /*ASF_Stream_Properties_Object GUID and object size*/
    {
         /* data after object size in ASF_Stream_Properties_Object */
        aw_asf_stream_header_t *streamh = (aw_asf_stream_header_t *)&mmsStreamInf->firstChunk[pos];
        pos += sizeof(aw_asf_stream_header_t);
        if (pos > mmsStreamInf->firstChunkBodySize)
        {
            goto out;
        }
        switch(ASF_LOAD_GUID_PREFIX(streamh->type))
        {

            case 0xF8699E40 :     // ASF_Audio_Media  audio stream
                if(asfCtrl->audioStreams == NULL)
                {
                    asfCtrl->audioStreams = malloc(sizeof(int));
                    asfCtrl->nAudio = 1;
                }
                else
                {
                    asfCtrl->nAudio++;
                    asfCtrl->audioStreams = realloc(asfCtrl->audioStreams,
                                              asfCtrl->nAudio*sizeof(int));
                }
                asfCtrl->audioStreams[asfCtrl->nAudio-1] = AV_RL16(&streamh->streamNo);
                break;
           case 0xBC19EFC0 : // ASF_Vedio_Media   video stream
                if(asfCtrl->videoStreams == NULL)
                {
                    asfCtrl->videoStreams = malloc(sizeof(int));
                    asfCtrl->nVideo = 1;
                }
                else
                {
                    asfCtrl->nVideo++;
                    asfCtrl->videoStreams = realloc(asfCtrl->videoStreams,
                                              asfCtrl->nVideo*sizeof(int));
                }
                asfCtrl->videoStreams[asfCtrl->nVideo-1] = AV_RL16(&streamh->streamNo);
                break;
         }
     }

     // always allocate to avoid lots of ifs later
     vRates = calloc(asfCtrl->nVideo, sizeof(int));
     aRates = calloc(asfCtrl->nAudio, sizeof(int));

     pos = MmshFindAsfGuid(mmsStreamInf->firstChunk, asfStreamGroupGuid, start,
                          mmsStreamInf->firstChunkBodySize);
    /*ASF_Stream_Bitrate_Properties_Object*/
     if (pos >= 0)
     {
         // stream bitrate properties object
         int streamCount;
         char *ptr = &mmsStreamInf->firstChunk[pos];
         char *end = &mmsStreamInf->firstChunk[mmsStreamInf->firstChunkBodySize];
         if (ptr + 2 > end)
         {
             goto out;
         }
         streamCount = AV_RL16(ptr);    //* bitrate records count
         ptr += 2;
         for( i=0 ; i<streamCount ; i++ )
         {
             unsigned int  rate;
             int id;
             int j;
             if (ptr + 6 > end)
             {
                 return -1;
             }
             id = AV_RL16(ptr);        /*flags in Bitrate Records*/
             ptr += 2;
             rate = GetLE32(ptr);
             ptr += 4;
             for (j = 0; j < asfCtrl->nVideo; j++)
             {
                 if(id == asfCtrl->videoStreams[j])   //* find the proper stream number
                 {
                     vRates[j] = rate;
                     break;
                 }
             }
             for (j = 0; j < asfCtrl->nAudio; j++)
             {
                 if (id == asfCtrl->audioStreams[j])
                 {
                     aRates[j] = rate;
                     break;
                 }
             }
         }
     }

     int aRate = 0;
     int aIdx = -1;
     int vRate = 0;
     int vIdx = -1;
     int bw = 2147483647;
     if (asfCtrl->nAudio)
     {
         // find lowest-bitrate audio stream
         aRate = aRates[0];
         aIdx = 0;
         for (i = 0; i<asfCtrl->nAudio; i++)
         {
             if (aRates[i] < aRate)
             {
                 aRate = aRates[i];
                 aIdx = i;
             }
         }
         if(MmshGetMaxIdx(asfCtrl->nVideo, vRates, bw - aRate) < 0)
         {
             // both audio and video are not possible, try video only next
             aIdx = -1;
             aRate = 0;
         }
     }
     // find best video stream
     vIdx = MmshGetMaxIdx(asfCtrl->nVideo, vRates, bw - aRate);
     if(vIdx >= 0)
     {
         vRate = vRates[vIdx];
     }

     // find best audio stream
     aIdx = MmshGetMaxIdx(asfCtrl->nAudio, aRates, bw - vRate);
     free(vRates);
     free(aRates);
     vRates = NULL;
     aRates = NULL;
     if (aIdx < 0 && vIdx < 0)
     {
         goto out;
     }

     if(aIdx >= 0)
     {
         asfCtrl->audioId = asfCtrl->audioStreams[aIdx];
     }
     if (vIdx >= 0)
     {
         asfCtrl->videoId = asfCtrl->videoStreams[vIdx];
     }
     return 1;

out:
     if (vRates)
     {
         free(vRates);
         vRates = NULL;
     }
     if(aRates)
     {
         free(aRates);
         aRates = NULL;
     }
     return -1;
}

int aw_mms_http_streaming_read_func(aw_mms_inf_t* mmsStreamInf)
{
    int remainDataSize = 0;
    int readLen = 0;

    if(mmsStreamInf->bufWritePtr < mmsStreamInf->bufReleasePtr)
    {
        remainDataSize = mmsStreamInf->bufReleasePtr - mmsStreamInf->bufWritePtr;
    }
    else
    {
        remainDataSize = mmsStreamInf->bufEndPtr+1-mmsStreamInf->bufWritePtr;
    }
    if(remainDataSize == 0)
    {
        return STREAM_READ_OK;
    }

    readLen = aw_mms_http_streaming_read(mmsStreamInf->bufWritePtr,remainDataSize,mmsStreamInf);
    if(readLen == -1)
    {
        return STREAM_READ_END;
    }
    else
    {
        mmsStreamInf->bufWritePtr += readLen;
        if(mmsStreamInf->bufWritePtr > mmsStreamInf->bufEndPtr)
        {
            mmsStreamInf->bufWritePtr -= mmsStreamInf->stream_buf_size;
        }
        pthread_mutex_lock(&mmsStreamInf->bufferMutex);
        mmsStreamInf->bufferDataSize += readLen;
        mmsStreamInf->buf_pos += readLen;
        mmsStreamInf->validDataSize  += readLen;
        pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
        return STREAM_READ_OK;
    }
    return STREAM_READ_OK;
}

int aw_mms_http_streaming_read(char *buffer, int size, aw_mms_inf_t* mmsStreamInf)
{
    unsigned char buf[12] = {0};
    int resetSize = 0;
    int dropChunk = 0;
    int chunkSize = 0;
    int wantSize = 0;
    int remainDataSize = 0;
    aw_asf_stream_chunck_t chunk;
    aw_mmsh_ctrl_t *asfHttpCtrl = (aw_mmsh_ctrl_t*)mmsStreamInf->data;

    while(1)
    {
        if(mmsStreamInf->exitFlag == 1)
        {
            return -1;
        }

        memset(&chunk, 0, sizeof(aw_asf_stream_chunck_t));
        if(aw_asf_http_read_wrapper(buf, 4, mmsStreamInf) <= 0)
        {
            return -1;
        }
        // Endian handling of the stream chunk
        chunk.type = ((buf[1])<<8) | buf[0];
        chunk.size = ((buf[3])<<8) | buf[2];
        resetSize = (chunk.size>8)?  8 : chunk.size;

        if(aw_asf_http_read_wrapper(buf,resetSize, mmsStreamInf) <= 0)
        {
            return -1;
        }
        chunk.sequenceNumber = (buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
        chunk.unknown = (buf[5]<<8)|buf[4];
        if(resetSize < 8)
        {
            chunk.sizeConfirm = 8;
        }
        else
        {
            chunk.sizeConfirm = (buf[7]<<8) | buf[6];
        }
        printf("sizeConfirm = 0x%02hx\n", chunk.sizeConfirm);

        if (AsfStreamingType(&chunk,&dropChunk))
        {
            //mmsStreamInf->networkStreamConfigClearFlag = 1;
        }
        chunkSize = chunk.sizeConfirm-8;
        //printf("chunksize = %d\n", chunkSize);
        wantSize = chunkSize;    /* no padding*/
        if(chunk.type != ASF_STREAMING_HEADER && (!dropChunk))
        {
            if(asfHttpCtrl->packetSize < chunkSize)
            {
                printf("***********asfHttpCtrl->packetSize is smaller than \
                  the chunkSize, error.\n");
                return -1;
            }
            wantSize = asfHttpCtrl->packetSize;
         //the size of every data packet is the same (asfHttpCtrl->packetSize)
        }

        while(1)
        {
            if(mmsStreamInf->exitFlag == 1)
            {
                return STREAM_READ_END;
            }
            if((mmsStreamInf->stream_buf_size-mmsStreamInf->bufferDataSize) > wantSize)
            {
                break;
            }
            //printf("**************remainSize=%d, wantSize=%d\n",
            //(mmsStreamInf->stream_buf_size-mmsStreamInf->bufferDataSize), wantSize);
            usleep(40*1000);
        }

        if(size < wantSize)
        {
            if(asfHttpCtrl->packetSize>=32*1024 || chunkSize>= 32*1024)
            {
                printf("*************the asfChunkBuffer size is too small.\n");
                return -1;
            }
            if(aw_asf_http_read_wrapper(mmsStreamInf->asfChunkDataBuffer, chunkSize,
                              mmsStreamInf) <= 0)
            {
                return -1;
            }
            if(dropChunk == 1)
            {
                continue;
            }
            #if 0
            memset(mmsStreamInf->asfChunkDataBuffer+chunkSize,0,wantSize-chunkSize);
            memcpy(buffer,mmsStreamInf->asfChunkDataBuffer, size);
            memcpy(mmsStreamInf->buffer, mmsStreamInf->asfChunkDataBuffer+size, wantSize-size);
            #else
            memset(mmsStreamInf->asfChunkDataBuffer+chunkSize,0,wantSize-chunkSize);
            if(mmsStreamInf->bufWritePtr < mmsStreamInf->bufReleasePtr)
            {
                remainDataSize = mmsStreamInf->bufReleasePtr - mmsStreamInf->bufWritePtr;
                if(remainDataSize < wantSize)
                {
                    return -1;
                }
            }
            else
            {
                remainDataSize = mmsStreamInf->bufEndPtr+1-mmsStreamInf->bufWritePtr;
            }
            if(remainDataSize >= wantSize)
            {
                memcpy(buffer,mmsStreamInf->asfChunkDataBuffer, wantSize);
            }
            else
            {
                memcpy(buffer,mmsStreamInf->asfChunkDataBuffer,remainDataSize);
                memcpy(mmsStreamInf->buffer,mmsStreamInf->asfChunkDataBuffer+remainDataSize,
                  wantSize-remainDataSize);
            }
            #endif
        }
        else
        {
            if(aw_asf_http_read_wrapper( buffer, chunkSize, mmsStreamInf) <= 0)
            {
                return -1;
            }
            if(dropChunk == 1)
            {
                continue;
            }
            memset(buffer+chunkSize, 0, wantSize-chunkSize);
        }
        break;
    }
    return wantSize;
}

int aw_asf_http_read_wrapper(void *buffer, int size, aw_mms_inf_t* mmsStreamInf)
{
    int bufferLen = 0;
    int len = 0;
    int ret = 0;
    CdxStreamT *httpCoreStreamInfo = (CdxStreamT *)mmsStreamInf->httpCore;

    if(mmsStreamInf->firstChunk != NULL)
    {
        bufferLen = mmsStreamInf->firstChunkBodySize - mmsStreamInf->firstChunkPos;
        len = (size<bufferLen)? size: bufferLen;
        memcpy(buffer, mmsStreamInf->firstChunk+mmsStreamInf->firstChunkPos, len);
        mmsStreamInf->firstChunkPos += len;
        if(mmsStreamInf->firstChunkPos >= mmsStreamInf->firstChunkBodySize)
        {
            free(mmsStreamInf->firstChunk);
            mmsStreamInf->firstChunk = NULL;
            mmsStreamInf->firstChunkPos = 0;
            mmsStreamInf->firstChunkBodySize = 0;
        }
    }

    if(len<size)
    {
        while(len < size)
        {
            ret = CdxStreamRead(httpCoreStreamInfo, (char*)buffer+len, size-len);
            if(ret <= 0)
            {
                CDX_LOGE("******************ret=%d, want=%d\n", ret, size-len);
                return -1;
            }
            len += ret;
        }
    }
    return len;
}

int aw_mms_http_streaming_seek(aw_mms_inf_t* mmsStreamInf)
{
    CDX_UNUSE(mmsStreamInf);
#if 0
    aw_mmsh_ctrl_t *asfHttpCtrl = NULL;
    aw_http_header_t *httpHdr = NULL;
    char buffer[2048];
    aw_asf_stream_chunck_t chunk;
    char*bufPtr = NULL;
    aw_asf_file_header_t *fileh = NULL;
    int chunkSize = 0;
    aw_url_t*url = NULL;
    int readLen = 0;
    int dropChunk = 0;
    int fd = 0;
    int pos = 0;
    int i = 0;
    int r = 0;
    int findValidChunkFlag = 0;
    int resetSize = 0;
    char buf[12] = {0};

    CedarXDataSource *datasource;
    struct HttpHeaderField hdr[7] = {{"Accept","*/*"},{"User-Agent","NSPlayer/4.1.0.3856"},
                     {"Pragma","xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}"},
                            {"Pragma","no-cache,rate=1.000000,stream-time=0,\
                              stream-offset=0:0,request-context=2,max-duration=0"},
                            {"Pragma"," xPlayStrm=1"},
                            {"Pragma","stream-switch-entry=ffff:1:0 ffff:2:0"},
                            {"Pragma","stream-switch-count=2"}};

    datasource->pHttpHeader = (char***)hdr;
    datasource->nHttpHeaderSize = 7;

    fd = mmsStreamInf->fd;
    url = mmsStreamInf->awUrl;
    asfHttpCtrl = (aw_mmsh_ctrl_t*)mmsStreamInf->data;

    if(mmsStreamInf->sockFd > 0)
    {
        closesocket(mmsStreamInf->sockFd);
    }
    if(!strcasecmp(url->protocol, "http_proxy"))
    {
        if(url->port == 0)
        {
            url->port = 8080;
        }
    }
    else
    {
        if(url->port == 0)
        {
            url->port = 80;
        }
    }

    fd = Connect2Server(mmsStreamInf, url->hostname, url->port, 1);
    if(fd <= 0)
    {
        printf("*********************connect the stream fd=%d\n", fd);
    }
    httpHdr = MmshRequest(mmsStreamInf,(mmsStreamInf->buf_pos>>32)&0xffffffff,
                        mmsStreamInf->buf_pos&0xffffffff, 0);
    for(i=0; i < (int)httpHdr->bufferSize; )
    {
        r = send(fd, httpHdr->buffer+i, httpHdr->bufferSize-i, DEFAULT_SEND_FLAGS);
        if(r <0)
        {
            LOGV("**************MSGTR_MPDEMUX_ASF_SocketWriteError\n");
            goto err_out;
        }
        i += r;
        usleep(30*1000);
    }
    do
    {
        i = MmsGetNetworkData(mmsStreamInf, fd, buffer,2048);
        if(i<=0)
        {
            goto err_out;
        }
        CdxHttpResponseAppend(httpHdr, buffer, i);
    } while(!CdxHttpIsHeaderEntire(httpHdr));
    httpHdr->buffer[httpHdr->bufferSize] = '\0';
    mmsStreamInf->fd = fd;
    CdxHttpFree(httpHdr);

    while(1)
    {
        if(mmsStreamInf->exitFlag == 1)
        {
            return 0;
        }
        memset(&chunk, 0, sizeof(aw_asf_stream_chunck_t));
        if(AsfReadWrapper(fd, buf, 4, mmsStreamInf) <= 0)
        {
            return -1;
        }
        // Endian handling of the stream chunk
        chunk.type = (buf[1]<<8)|buf[0];
        chunk.size = (buf[3]<<8)|buf[2];
        mmsStreamInf->buf_pos += 4;
        mmsStreamInf->dmx_read_pos += 4;
        if(findValidChunkFlag==0)
        {
            if((chunk.type==ASF_STREAMING_HEADER)||(chunk.type == ASF_STREAMING_DATA))
            {
                findValidChunkFlag = 1;
            }
            else
            {
                continue;
            }
        }
        resetSize = (chunk.size>8)?  8 : chunk.size;
        if(AsfReadWrapper(fd, buf,resetSize, mmsStreamInf) <= 0)
        {
            return -1;
        }
        chunk.sequenceNumber = (buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
        chunk.unknown = (buf[5]<<8)|buf[4];
        if(resetSize < 8)
        {
            chunk.sizeConfirm = 8;
        }
        else
        {
            chunk.sizeConfirm = (buf[7]<<8)|buf[6];
        }
        AsfStreamingType(&chunk,&dropChunk);
        chunkSize = chunk.sizeConfirm-8;
        mmsStreamInf->buf_pos += resetSize;
        mmsStreamInf->dmx_read_pos += resetSize;

        bufPtr = mmsStreamInf->bufWritePtr;
        readLen = AsfReadWrapper(fd, bufPtr,chunkSize, mmsStreamInf);
        if(readLen <= 0)
        {
            goto err_out;
        }

        if(chunk.type == ASF_STREAMING_HEADER)
        {
            pos = MmshFindAsfGuid(bufPtr, asfFileHeaderGuid, 0, chunkSize);
            if(pos >= 0)
            {
                fileh = (aw_asf_file_header_t *) &bufPtr[pos];
                asfHttpCtrl->packetSize = GetLE32(&fileh->maxPacketSize);
                mmsStreamInf->fileSize  = GetLE64(&fileh->fileSize);
                mmsStreamInf->fileDuration = (GetLE64(&fileh->playDuration)/10000);
            }
            mmsStreamInf->buf_pos += chunkSize;
            mmsStreamInf->dmx_read_pos += chunkSize;
            continue;
        }
        else if(dropChunk == 1)
        {
            mmsStreamInf->buf_pos += chunkSize;
            mmsStreamInf->dmx_read_pos += chunkSize;
            continue;
        }
        else
        {
            break;
        }
    }
    pthread_mutex_lock(&mmsStreamInf->bufferMutex);
    memset(bufPtr+chunkSize, 0, asfHttpCtrl->packetSize-chunkSize);
    mmsStreamInf->bufWritePtr += asfHttpCtrl->packetSize;
    if(mmsStreamInf->bufWritePtr > mmsStreamInf->bufEndPtr)
    {
        mmsStreamInf->bufWritePtr -= mmsStreamInf->stream_buf_size;
    }
    mmsStreamInf->bufferDataSize += asfHttpCtrl->packetSize;
    mmsStreamInf->buf_pos += asfHttpCtrl->packetSize;
    mmsStreamInf->validDataSize  += asfHttpCtrl->packetSize;
    pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
    return 0;

err_out:
    mmsStreamInf->fd = -1;
    if(httpHdr != NULL)
    {
        CdxHttpFree(httpHdr);
    }
    if(asfHttpCtrl != NULL)
    {
        free(asfHttpCtrl);
        asfHttpCtrl = NULL;
        mmsStreamInf->data = NULL;
    }
#endif
    return -1;
}

