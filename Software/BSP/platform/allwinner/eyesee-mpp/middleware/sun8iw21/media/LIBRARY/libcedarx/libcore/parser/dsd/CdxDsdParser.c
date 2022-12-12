/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxDsdParser.c
* Description :
* History :
*   Author  : Khan <chengkan@allwinnertech.com>
*   Date    : 2015/05/08
*   Comment : ¡ä¡ä?¡§3?¨º?¡ã?¡À?¡ê?¨º¦Ì?? DSD ¦Ì??a?¡ä¨®?1|?¨¹
*/

#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxDsdParser.h>

#define DSD_MATCH(x,y) (memcmp((x),(y),(unsigned int)sizeof(x))==0)
#define GUINT16_FROM_LE(x)  (cdx_uint16) (x)
#define GUINT32_FROM_LE(x)  (cdx_uint32) (x)
#define GUINT64_FROM_LE(x)  (cdx_uint64) (x)

#define GUINT16_FROM_BE(m)            \
        ((((m) & 0xff00) >> 8) | (((m) & 0x00ff) << 8))

#define GUINT32_FROM_BE(m)                 \
        ((((m) & 0x00ff0000) >> 8) |  \
         (((m) & 0xff000000) >> 24) | \
         (((m) & 0x0000ff00) << 8) |  \
         (((m) & 0x000000ff) << 24))

#define GUINT64_FROM_BE(m)                     \
        ((((m) & 0x00000000ff000000) << 8) |  \
         (((m) & 0x0000000000ff0000) << 24) | \
         (((m) & 0x000000000000ff00) << 40) | \
         (((m) & 0x00000000000000ff) << 56) | \
         (((m) & 0x000000ff00000000) >> 8)  | \
         (((m) & 0x0000ff0000000000) >> 24) | \
         (((m) & 0x00ff000000000000) >> 40) | \
         (((m) & 0xff00000000000000) >> 56))

static cdx_int32 dsd_read_raw(void *buffer, cdx_int32 bytes, dsdfile *file) {
    size_t bytes_read;
    bytes_read = CdxStreamRead(file->stream,buffer, bytes);
    file->offset += (cdx_int64)bytes_read;
    return bytes_read;
}

static cdx_bool dsd_seek(dsdfile *file, cdx_int64 offset, int whence) {

    cdx_uint8 arrBuffer[8192];
    cdx_size uRead_bytes;
    cdx_int32 nTmp;

    if (file->canseek)
    {
        if (CdxStreamSeek(file->stream, offset, whence) == 0)
        {
            file->offset += offset;
            return CDX_TRUE;
        }
        else
        {
            return CDX_FALSE;
        }
    }

    if (whence == SEEK_CUR)
    uRead_bytes = offset;
    else if (whence == SEEK_SET)
    {
        if (offset < 0 || (cdx_uint64)offset < file->offset)
            return CDX_FALSE;
        uRead_bytes = offset - file->offset;
    }
    else
        return CDX_FALSE;

    while (uRead_bytes > 0)
    {
        nTmp = (uRead_bytes > sizeof(arrBuffer) ? sizeof(arrBuffer) : uRead_bytes);
        if (dsd_read_raw(arrBuffer, nTmp, file) == nTmp)
            uRead_bytes -= nTmp;
        else
            return CDX_FALSE;
    }
    return CDX_TRUE;
}

cdx_bool read_header(dsdiff_header *pHead, dsdfile *file) {
    cdx_uint8 buffer[12];
    cdx_uint8 *pPtr=buffer;
    if ((size_t)dsd_read_raw(buffer, sizeof(buffer), file) < sizeof(buffer))
        return CDX_FALSE;
    pHead->size = GUINT64_FROM_BE(*((cdx_uint64*)pPtr));
    pPtr+=8;

    memcpy(pHead->prop, pPtr, 4);
    if (!DSD_MATCH(pHead->prop, "DSD "))
        return CDX_FALSE;
    return CDX_TRUE;
}

cdx_bool read_chunk_header(dsdiff_chunk_header *head, dsdfile *file) {
    cdx_uint8 buffer[12];
    cdx_uint8 *ptr=buffer;
    if ((size_t)dsd_read_raw(buffer, sizeof(buffer), file) < sizeof(buffer))
        return CDX_FALSE;

    memcpy(head->id, ptr, 4);
    ptr+=4;
    head->size = GUINT64_FROM_BE(*((cdx_uint64*)ptr));
    return CDX_TRUE;
}

cdx_bool parse_prop_chunk(dsdfile *pFile, dsdiff_chunk_header *head) {
    cdx_uint64 stop = pFile->offset + head->size;
    dsdiff_chunk_header mProp_head;
    char propType[4];

    if ((size_t)dsd_read_raw(propType, sizeof(propType), pFile) < sizeof(propType))
        return CDX_FALSE;

    if (!DSD_MATCH(propType, "SND "))
    {
        if (dsd_seek(pFile, head->size - sizeof(propType), SEEK_CUR))
          return CDX_TRUE;
        else
          return CDX_FALSE;
    }

    while (pFile->offset < stop)
    {
      if (!read_chunk_header(&mProp_head, pFile)) return CDX_FALSE;
      if (DSD_MATCH(mProp_head.id, "FS  "))
      {
          cdx_uint32 uSample_frequency;
          if ((size_t)dsd_read_raw(&uSample_frequency, sizeof(uSample_frequency), pFile)<
              sizeof(uSample_frequency))
              return CDX_FALSE;
          pFile->sampling_frequency = GUINT32_FROM_BE(uSample_frequency);
      }
      else if (DSD_MATCH(mProp_head.id, "CHNL"))
      {
          cdx_uint16 uNum_channels;
          if ((size_t)dsd_read_raw(&uNum_channels, sizeof(uNum_channels), pFile) <
              sizeof(uNum_channels) ||
              !dsd_seek(pFile, mProp_head.size - sizeof(uNum_channels), SEEK_CUR))
              return CDX_FALSE;
          pFile->channel_num = (cdx_uint32)GUINT16_FROM_BE(uNum_channels);
      }
      else
      {
          dsd_seek(pFile, mProp_head.size, SEEK_CUR);
      }
    }

    return CDX_TRUE;
}

static cdx_bool read_dsd_chunk_data(dsd_chunk *dsf_head, dsdfile *file) {
    cdx_uint8 buffer[24];
    cdx_uint8 *ptr=buffer;
    if ((size_t)dsd_read_raw(buffer, sizeof(buffer), file) < sizeof(buffer))
        return CDX_FALSE;
    dsf_head->size_of_chunk = GUINT64_FROM_LE(*((cdx_uint64*)ptr));
    ptr+=8;
    if (dsf_head->size_of_chunk != 28)
        return CDX_FALSE;

    dsf_head->total_size = GUINT64_FROM_LE(*((cdx_uint64*)ptr));
    ptr+=8;
    dsf_head->ptr_to_metadata = GUINT64_FROM_LE(*((cdx_uint64*)ptr));
    return CDX_TRUE;
}

cdx_bool read_fmt_chunk(fmt_chunk *pDsf_fmt, dsdfile *file) {
    cdx_uint8 buffer[52];
    cdx_uint8 *pPtr=buffer;
    if ((size_t)dsd_read_raw(buffer, sizeof(buffer), file) < sizeof(buffer))
        return CDX_FALSE;
    memcpy(pDsf_fmt->header, pPtr, 4);
    pPtr+=4;

    if (!DSD_MATCH(pDsf_fmt->header, "fmt "))
        return CDX_FALSE;
    pDsf_fmt->size_of_chunk = GUINT64_FROM_LE(*((cdx_uint64*)pPtr));
    pPtr+=8;
    if (pDsf_fmt->size_of_chunk != 52)
        return CDX_FALSE;

    pDsf_fmt->format_version = GUINT32_FROM_LE(*((cdx_uint32*)pPtr));
    pPtr+=4;
    pDsf_fmt->format_id = GUINT32_FROM_LE(*((cdx_uint32*)pPtr));
    pPtr+=4;
    pDsf_fmt->channel_type = GUINT32_FROM_LE(*((cdx_uint32*)pPtr));
    pPtr+=4;
    pDsf_fmt->channel_num = GUINT32_FROM_LE(*((cdx_uint32*)pPtr));
    pPtr+=4;
    pDsf_fmt->sampling_frequency = GUINT32_FROM_LE(*((cdx_uint32*)pPtr));
    pPtr+=4;
    pDsf_fmt->bits_per_sample = GUINT32_FROM_LE(*((cdx_uint32*)pPtr));
    pPtr+=4;
    pDsf_fmt->sample_count = GUINT64_FROM_LE(*((cdx_uint64*)pPtr));
    pPtr+=8;
    pDsf_fmt->block_size_per_channel = GUINT32_FROM_LE(*((cdx_uint32*)pPtr));
    pPtr+=4;
    //      !DSD_MATCH(pDsf_fmt.reserved,"\0\0\0\0")) {
    return CDX_TRUE;
}

cdx_bool read_data_header(data_header *pDsf_datahead, dsdfile *file) {
    cdx_uint8 buffer[12];
    cdx_uint8 *pPtr=buffer;

    if ((size_t)dsd_read_raw(buffer, sizeof(buffer), file) < sizeof(buffer))
        return CDX_FALSE;

    memcpy(pDsf_datahead->header, pPtr, 4);
    pPtr+=4;
    if (!DSD_MATCH(pDsf_datahead->header, "data"))
        return CDX_FALSE;
    pDsf_datahead->size_of_chunk = GUINT64_FROM_LE(*((cdx_uint64*)pPtr));
    return CDX_TRUE;
}

cdx_int32 aw_dsdiff_set_pos(dsdfile *file, cdx_uint32 mseconds, cdx_uint64 *offset) {
    cdx_uint64 skip_bytes = 0;
    cdx_int32  ret = 0;
    if (!file)
        return -1;
    //if(file->eof) return -1;

    file->sample_offset = (cdx_uint64)file->sampling_frequency * mseconds / 8000;
    skip_bytes = file->sample_offset * file->channel_num;

    *offset = skip_bytes;
    return 0;
}

cdx_int32 aw_dsf_set_pos(dsdfile *pFile, cdx_uint32 mseconds, cdx_uint64 *offset) {
    cdx_uint64 uSkip_blocks = 0;
    cdx_uint64 uSkip_bytes = 0;
    cdx_int32  ret = 0;
    if(!pFile)
        return -1;
    //if(pFile->eof) return -1;

    uSkip_blocks = (cdx_uint64) pFile->sampling_frequency * mseconds /
                                (8000 * pFile->dsf.block_size_per_channel);
    uSkip_bytes = uSkip_blocks * pFile->dsf.block_size_per_channel * pFile->channel_num;

    pFile->sample_offset = uSkip_blocks * pFile->dsf.block_size_per_channel;

    *offset = uSkip_bytes;
    return 0;
}

static cdx_bool aw_dsf_read(dsdfile *pFile,cdx_int32 *pLen) {
    //if (pFile->eof) return CDX_FALSE;
    *pLen = pFile->channel_num * pFile->dsf.block_size_per_channel;

    if (pFile->dsf.block_size_per_channel >= (pFile->sample_stop - pFile->sample_offset))
    {
        pFile->buffer.bytes_per_channel = (pFile->sample_stop - pFile->sample_offset);
        //pFile->eof = CDX_TRUE;
    }
    else
    {
        pFile->sample_offset += pFile->dsf.block_size_per_channel;
        pFile->buffer.bytes_per_channel = pFile->dsf.block_size_per_channel;
    }

    return CDX_TRUE;
}

static cdx_bool aw_dsdiff_read(dsdfile *pFile,cdx_int32 *pLen) {
    cdx_uint32 uNum_samples;
    //if (pFile->eof) return CDX_FALSE;
    if ((pFile->sample_stop - pFile->sample_offset) < pFile->buffer.max_bytes_per_ch)
        uNum_samples = pFile->sample_stop - pFile->sample_offset;
    else
        uNum_samples = pFile->buffer.max_bytes_per_ch;

    *pLen = pFile->channel_num * uNum_samples;

    pFile->buffer.bytes_per_channel = uNum_samples;
    pFile->sample_offset += uNum_samples;
    //if (pFile->sample_offset >= pFile->sample_stop) pFile->eof = CDX_TRUE;

    return CDX_TRUE;
}

static cdx_bool aw_dsdiff_init(dsdfile *pFile) {
    dsdiff_header mHead;
    dsdiff_chunk_header mChunk_head;

    if (!read_header(&mHead, pFile))
        return CDX_FALSE;
    pFile->file_size = mHead.size + 12;

    while (read_chunk_header(&mChunk_head, pFile))
    {
        if (DSD_MATCH(mChunk_head.id, "FVER"))
        {
            char version[4];
            if (mChunk_head.size != 4 ||
            (size_t)dsd_read_raw(&version, sizeof(version), pFile)<sizeof(version) ||
            version[0] > 1)
            { // Major version 1 supported
                return CDX_FALSE;
            }
        }
        else if (DSD_MATCH(mChunk_head.id, "PROP"))
        {
            if (!parse_prop_chunk(pFile, &mChunk_head))
                return CDX_FALSE;
        }
        else if (DSD_MATCH(mChunk_head.id, "DSD "))
        {
            if (pFile->channel_num == 0)
                return CDX_FALSE;

            pFile->sample_offset = 0;
            pFile->sample_count = 8 * mChunk_head.size / pFile->channel_num;
            pFile->sample_stop = pFile->sample_count / 8;

            pFile->dataoffset = pFile->offset;
            pFile->datasize = mChunk_head.size;

            pFile->buffer.max_bytes_per_ch = 4096;
            pFile->buffer.lsb_first = CDX_FALSE;
            pFile->buffer.sample_step = pFile->channel_num;
            pFile->buffer.ch_step = 1;

            return CDX_TRUE;
        }
        else
            dsd_seek(pFile, mChunk_head.size, SEEK_CUR);
    }

    return CDX_FALSE;
}

static cdx_bool aw_dsf_init(dsdfile *pFile) {
    dsd_chunk mDsf_head;
    fmt_chunk mDsf_fmt;
    data_header mDsf_datahead;
    cdx_int64 nPadtest;

    if (!read_dsd_chunk_data(&mDsf_head, pFile))
        return CDX_FALSE;

    if (!read_fmt_chunk(&mDsf_fmt, pFile))
        return CDX_FALSE;

    pFile->file_size = mDsf_head.total_size;

    pFile->channel_num = mDsf_fmt.channel_num;
    pFile->sampling_frequency = mDsf_fmt.sampling_frequency;

    pFile->sample_offset = 0;
    pFile->sample_count = mDsf_fmt.sample_count;
    pFile->sample_stop = pFile->sample_count / 8;

    pFile->dsf.format_version = mDsf_fmt.format_version;
    pFile->dsf.format_id = mDsf_fmt.format_id;
    pFile->dsf.channel_type = mDsf_fmt.channel_type;
    pFile->dsf.bits_per_sample = mDsf_fmt.bits_per_sample;
    pFile->dsf.block_size_per_channel = mDsf_fmt.block_size_per_channel;

    if ((pFile->dsf.format_version != 1) ||
        (pFile->dsf.format_id != 0) ||
        (pFile->dsf.block_size_per_channel != 4096))
        return CDX_FALSE;

    if (!read_data_header(&mDsf_datahead, pFile))
        return CDX_FALSE;

    nPadtest = (cdx_int64)(mDsf_datahead.size_of_chunk - 12) / pFile->channel_num
      - pFile->sample_count / 8;
    if ((nPadtest < 0) || (nPadtest > pFile->dsf.block_size_per_channel))
    {
        return CDX_FALSE;
    }

    pFile->dataoffset = pFile->offset;
    pFile->datasize = (pFile->sample_count / 8 * pFile->channel_num);

    pFile->buffer.max_bytes_per_ch = pFile->dsf.block_size_per_channel;
    pFile->buffer.lsb_first = (pFile->dsf.bits_per_sample == 1);
    pFile->buffer.sample_step = 1;
    pFile->buffer.ch_step = pFile->dsf.block_size_per_channel;

    return CDX_TRUE;
}

static cdx_bool aw_dsd_open(dsdfile *pFile) {
    cdx_uint8 header_id[4];
    pFile->canseek = CDX_TRUE;
    pFile->eof = CDX_FALSE;
    pFile->offset = 0;
    pFile->sample_offset = 0;
    pFile->channel_num = 0;
    pFile->sampling_frequency = 0;

    if((size_t)dsd_read_raw(header_id,sizeof(header_id),pFile)<sizeof(header_id))
        return CDX_FALSE;

    if (DSD_MATCH(header_id, "DSD "))
    {
        pFile->type = DSF;
        if (!aw_dsf_init(pFile))
        {
            free(pFile);
            return CDX_FALSE;
        }
    }
    else if (DSD_MATCH(header_id, "FRM8"))
    {
        pFile->type = DSDIFF;
        if (!aw_dsdiff_init(pFile))
        {
            free(pFile);
            return CDX_FALSE;
        }
    }
    else
    {
        free(pFile);
        return CDX_FALSE;
    }

    // Finalize buffer
    pFile->buffer.num_channels = pFile->channel_num;
    pFile->buffer.bytes_per_channel = 0;
    pFile->buffer.data =
      (cdx_uint8 *)malloc(sizeof(cdx_uint8) * pFile->buffer.max_bytes_per_ch * pFile->channel_num);
    return CDX_TRUE;
}

static int DsdInit(CdxParserT *dsd_impl)
{
    cdx_int32 ret = 0;
    cdx_int64 offset=0;
    cdx_int32 tmpFd = 0;
    cdx_uint8 *buffer = NULL;
    unsigned char headbuf[10] = {0};
    struct DsdParserImplS *impl = NULL;
    impl = (DsdParserImplS*)dsd_impl;
    impl->fileSize = CdxStreamSize(impl->stream);

    impl->dsd = (dsdfile *)malloc(sizeof(dsdfile));
    memset(impl->dsd,0x00,sizeof(dsdfile));
    impl->dsd->stream = impl->stream;
    if(!aw_dsd_open(impl->dsd))
        goto OPENFAILURE;
    impl->file_offset = CdxStreamTell(impl->stream);
    buffer = malloc(impl->file_offset+INPUT_BUFFER_GUARD_SIZE);
    memset(buffer,0,impl->file_offset+INPUT_BUFFER_GUARD_SIZE);

    CdxStreamSeek(impl->stream, 0,SEEK_SET);
    CdxStreamRead(impl->stream, buffer, impl->file_offset);
    impl->extradata = buffer;
    impl->extradata_size = impl->file_offset;
    buffer = NULL;
    impl->mChannels   = impl->dsd->channel_num;
    impl->mSampleRate = 44100;
    impl->mOriSampleRate = impl->dsd->sampling_frequency;
    impl->mTotalSamps = impl->dsd->sample_stop;
    impl->mDuration   = impl->mTotalSamps * 8000/impl->mOriSampleRate;
    impl->mBitsSample = 16;
    CDX_LOGD("CK-----  impl->mChannels:%d,impl->mSampleRate:%d,impl->mOriSampleRate:%d,\
              impl->mTotalSamps:%lld,impl->mDuration:%lld",impl->mChannels,impl->mSampleRate,
              impl->mOriSampleRate,impl->mTotalSamps,impl->mDuration);
    impl->mErrno = PSR_OK;
    return 0;
OPENFAILURE:
    CDX_LOGE("DsdOpenThread fail!!!");
    impl->mErrno = PSR_OPEN_FAIL;
    return -1;
}

static cdx_int32 __DsdParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    struct DsdParserImplS *impl = NULL;
    impl = (DsdParserImplS*)parser;
    (void)param;

    switch (cmd)
    {
        case CDX_PSR_CMD_DISABLE_AUDIO:
        case CDX_PSR_CMD_DISABLE_VIDEO:
        case CDX_PSR_CMD_SWITCH_AUDIO:
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            CdxStreamForceStop(impl->stream);
            break;
        case CDX_PSR_CMD_CLR_FORCESTOP:
            CdxStreamClrForceStop(impl->stream);
            break;
        default :
            CDX_LOGW("not implement...(%d)", cmd);
            break;
    }
    impl->flags = cmd;
    return CDX_SUCCESS;
}

static cdx_int32 __DsdParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 ret = CDX_SUCCESS;
    struct DsdParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct DsdParserImplS, base);
    dsdfile * file = impl->dsd;
    if (file->type == DSF)
        if(!aw_dsf_read(file,&pkt->length))
            goto FAIL;

    if (file->type == DSDIFF)
        if(!aw_dsdiff_read(file,&pkt->length))
            goto FAIL;

    pkt->type = CDX_MEDIA_AUDIO;
    pkt->pts = impl->mCurSamps*8000000ll/(impl->mOriSampleRate);//one frame pts;
    CDX_LOGV("pkt->pts:%lld",pkt->pts);
    pkt->flags |= (FIRST_PART|LAST_PART);
    return ret;
FAIL:
    return ret;
}

static cdx_int32 __DsdParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 ret = CDX_FAILURE;
    cdx_int32 read_length= 0;
    CdxStreamT *cdxStream = NULL;
    dsdfile *   file = NULL;
    struct DsdParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct DsdParserImplS, base);
    cdxStream = impl->stream;
    file = impl->dsd;
    //READ ACTION

    if(pkt->length <= pkt->buflen)
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->length);
    }
    else
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->buflen);
        read_length += CdxStreamRead(cdxStream, pkt->ringBuf, pkt->length - pkt->buflen);
    }

    if(read_length < 0)
    {
        CDX_LOGE("CdxStreamRead fail");
        impl->mErrno = PSR_IO_ERR;
        return CDX_FAILURE;
    }
    else if(read_length == 0)
    {
       CDX_LOGD("CdxStream EOS");
       impl->mErrno = PSR_EOS;
       return CDX_FAILURE;
    }

    pkt->length = read_length;
    impl->file_offset += read_length;
    impl->mCurSamps = file->sample_offset;
    //TODO: fill pkt
    return CDX_SUCCESS;
}

static cdx_int32 __DsdParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    struct CdxProgramS *cdxProgram = NULL;
    struct DsdParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct DsdParserImplS, base);
    memset(mediaInfo, 0x00, sizeof(*mediaInfo));

    if(impl->mErrno != PSR_OK)
    {
        CDX_LOGE("audio parse status no PSR_OK");
        return CDX_FAILURE;
    }

    mediaInfo->programNum = 1;
    mediaInfo->programIndex = 0;
    cdxProgram = &mediaInfo->program[0];
    memset(cdxProgram, 0, sizeof(struct CdxProgramS));
    cdxProgram->id = 0;
    cdxProgram->audioNum = 1;
    cdxProgram->videoNum = 0;//
    cdxProgram->subtitleNum = 0;
    cdxProgram->audioIndex = 0;
    cdxProgram->videoIndex = 0;
    cdxProgram->subtitleIndex = 0;

    mediaInfo->bSeekable = CdxStreamSeekAble(impl->stream);
    mediaInfo->fileSize = impl->fileSize;

    cdxProgram->duration = impl->mDuration;
    cdxProgram->audio[0].eCodecFormat    = AUDIO_CODEC_FORMAT_DSD;
    cdxProgram->audio[0].eSubCodecFormat = 0;
    cdxProgram->audio[0].nChannelNum     = impl->mChannels;
    cdxProgram->audio[0].nBitsPerSample  = impl->mBitsSample;
    cdxProgram->audio[0].nSampleRate     = impl->mSampleRate;
    cdxProgram->audio[0].nCodecSpecificDataLen = impl->extradata_size;
    cdxProgram->audio[0].pCodecSpecificData = (char *)impl->extradata;

    return CDX_SUCCESS;
}

static cdx_int32 __DsdParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    dsdfile *file = NULL;
    cdx_uint64 off = 0;
    struct DsdParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct DsdParserImplS, base);
    cdx_uint32 mseconds = (cdx_uint32)(timeUs/1000);
    CDX_LOGD("SEEK TO timeUs:%lld",timeUs);
    file = impl->dsd;
    if (file->type == DSF)
        if(aw_dsf_set_pos(file,mseconds,&off))
            goto FAIL;

    if (file->type == DSDIFF)
        if(aw_dsdiff_set_pos(file,mseconds,&off))
            goto FAIL;

    impl->mCurSamps = file->sample_offset;
    impl->file_offset = impl->extradata_size + off;
    CdxStreamSeek(impl->stream,impl->file_offset,SEEK_SET);
    return CDX_SUCCESS;
FAIL:
    return CDX_FAILURE;
}

static cdx_uint32 __DsdParserAttribute(CdxParserT *parser)
{
    struct DsdParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct DsdParserImplS, base);
    return 0;
}
#if 0
static cdx_int32 __Id3ParserForceStop(CdxParserT *parser)
{
    struct Id3ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct Id3ParserImplS, base);
    return CdxParserForceStop(impl->child);
}
#endif
static cdx_int32 __DsdParserGetStatus(CdxParserT *parser)
{
    struct DsdParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct DsdParserImplS, base);

    if (CdxStreamEos(impl->stream))
    {
        return PSR_EOS;
    }
    return impl->mErrno;
}

static cdx_int32 __DsdParserClose(CdxParserT *parser)
{
    struct DsdParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct DsdParserImplS, base);

    CdxStreamClose(impl->stream);
    if(impl->extradata)
    {
        free(impl->extradata);
        impl->extradata = NULL;
    }

    CdxFree(impl);
    return CDX_SUCCESS;
}

static struct CdxParserOpsS dsdParserOps =
{
    .control = __DsdParserControl,
    .prefetch = __DsdParserPrefetch,
    .read = __DsdParserRead,
    .getMediaInfo = __DsdParserGetMediaInfo,
    .seekTo = __DsdParserSeekTo,
    .attribute = __DsdParserAttribute,
    .getStatus = __DsdParserGetStatus,
    .close = __DsdParserClose,
    .init = DsdInit
};

static cdx_uint32 __DsdParserProbe(CdxStreamProbeDataT *probeData)
{
    int ret = 0;
    CDX_CHECK(probeData);
    if(probeData->len < 10)
    {
        CDX_LOGE("Probe data is not enough.");
        return ret;
    }
    if (DSD_MATCH(probeData->buf, "DSD "))
    {
        CDX_LOGD("YES,IT'S IS DSD");
        ret = 100;
    }
    else if (DSD_MATCH(probeData->buf, "FRM8"))
    {
        CDX_LOGD("YES,IT'S IS DSDIFF");
        ret = 100;
    }
    return ret;
}

static CdxParserT *__DsdParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    cdx_int32 ret = 0;
    struct DsdParserImplS *impl;
    impl = CdxMalloc(sizeof(*impl));

    memset(impl, 0x00, sizeof(*impl));

    impl->stream = stream;
    impl->base.ops = &dsdParserOps;
    (void)flags;
    //ret = pthread_create(&impl->openTid, NULL, DsdOpenThread, (void*)impl);
    //CDX_FORCE_CHECK(!ret);
    impl->mErrno = PSR_INVALID;

    return &impl->base;
}

struct CdxParserCreatorS dsdParserCtor =
{
    .probe = __DsdParserProbe,
    .create = __DsdParserOpen
};
