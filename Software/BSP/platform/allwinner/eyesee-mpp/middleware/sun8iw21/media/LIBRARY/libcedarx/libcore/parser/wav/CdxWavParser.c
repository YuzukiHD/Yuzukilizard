/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxWavParser.c
* Description :
* History :
*   Author  : Ls Zhang <lszhang@allwinnertech.com>
*   Date    : 2014/08/08
*   Comment : 创建初始版本，实现 WAV 的解复用功能
*/

#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>

#include <CdxWavParser.h>

#define AW_DCA_MARKER_14B_BE 0x1FFFE800
#define AW_DCA_MARKER_14B_LE 0xFF1F00E8
#define AW_DCA_MARKER_RAW_BE 0x7FFE8001
#define AW_DCA_MARKER_RAW_LE 0xFE7F0180

#define AV_RB32(x)  ((((const unsigned char*)(x))[0] << 24) | \
    (((const unsigned char*)(x))[1] << 16) | \
    (((const unsigned char*)(x))[2] <<  8) | \
                      ((const unsigned char*)(x))[3])

static int WavInit(CdxParserT* parameter)
{
    cdx_int32             ret            = 0;
    WAVObjectT            WAVObject;
    unsigned int          TempUid        = 0;
    int                   nFindAllFlag   = 0;
    int                   ulLength       = 0;
    int                   ulFlag         = 0;
    cdx_int32             nDataFactSize  = 0;
    cdx_int64             dDurationADPCM = 0;
    unsigned char         readBuf[64];
    struct WavParserImplS *impl          = NULL;

    impl = (WavParserImplS *)parameter;

    ret = CdxStreamRead(impl->stream, (void *)&WAVObject, 8);
    if(ret != 8)
    {
        goto OPENFAILURE;
    }
    ulLength += 8;
    if(WAVObject.UID ==RIFF)
    {
        ret = CdxStreamRead(impl->stream, (void *)&TempUid,4);
        if(ret != 4)
        {
            goto OPENFAILURE;
        }
        ulLength +=4;
        if(TempUid == WAVE)
        {
            while(nFindAllFlag<2)
            {
                ret = CdxStreamRead(impl->stream, (void *)&WAVObject, 8);
                if(ret != 8)
                {
                    goto OPENFAILURE;
                }
                ulLength +=8;
                switch(WAVObject.UID)
                {
                case fmt:
                    {
                        if(WAVObject.Dsize>64)
                        {
                            ret = CdxStreamRead(impl->stream, readBuf,64);
                            if(ret != 64)
                            {
                                goto OPENFAILURE;
                            }
                            CdxStreamSeek(impl->stream,WAVObject.Dsize-64, SEEK_CUR);
                        }
                        else
                        {
                            ret = CdxStreamRead(impl->stream, readBuf,WAVObject.Dsize);
                            if(ret != WAVObject.Dsize)
                            {
                                goto OPENFAILURE;
                            }
                        }
                        if(ulFlag==0)ulLength +=WAVObject.Dsize;
                        impl->WavFormat.wFormatag = (readBuf[0]&0xff) |
                                                    (((cdx_int32)readBuf[1]&0xff) << 8);
                        impl->WavFormat.nChannls  = (readBuf[2]&0xff) |
                                                    (((cdx_int32)readBuf[3]&0xff) << 8);
                        impl->WavFormat.nSamplesPerSec = (readBuf[4]&0xff) |
                                                         (((cdx_int32)readBuf[5]&0xff) << 8) |
                                                         (((cdx_int32)readBuf[6]&0xff) << 16) |
                                                         (((cdx_int32)readBuf[7]&0xff)<<24);
                        impl->WavFormat.nAvgBytesperSec = (readBuf[8]&0xff) |
                                                          (((cdx_int32)readBuf[9]&0xff) << 8) |
                                                          (((cdx_int32)readBuf[10]&0xff) << 16)|
                                                          (((cdx_int32)readBuf[11]&0xff) << 24);
                        impl->WavFormat.nBlockAlign = (readBuf[12]&0xff) |
                                                      (((cdx_int32)readBuf[13]&0xff) << 8);
                        impl->WavFormat.wBitsPerSample = (readBuf[14]&0xff) |
                                                         (((cdx_int32)readBuf[15]&0xff) << 8);

                        if(WAVObject.Dsize > 21)
                        {
                            impl->WavFormat.sbSize = (readBuf[16]&0xff) |
                                                     (((cdx_int32)readBuf[17]&0xff)<<8);
                            impl->WavFormat.nSamplesPerBlock = (readBuf[18]&0xff) |
                                                               (((cdx_int32)readBuf[19]&0xff)<<8) |
                                                               (((cdx_int32)readBuf[20]&0xff)<<16)|
                                                               (((cdx_int32)readBuf[21]&0xff)<<24);
                            if(!impl->WavFormat.nSamplesPerBlock)
                            {
                                impl->WavFormat.nSamplesPerBlock = impl->WavFormat.nBlockAlign*8/
                                       (impl->WavFormat.wBitsPerSample * impl->WavFormat.nChannls);
                            }
                        }
                        else
                        {
                            impl->WavFormat.nSamplesPerBlock    = 1;
                            if(impl->WavFormat.wFormatag==0x02 ||impl->WavFormat.wFormatag==0x11)
                            {
                                impl->WavFormat.nSamplesPerBlock = impl->WavFormat.nBlockAlign*8/
                                         (impl->WavFormat.wBitsPerSample*impl->WavFormat.nChannls);
                            }
                        }
                        if(impl->WavFormat.wFormatag==0x45)
                        {
                            impl->WavFormat.nSamplesPerBlock = impl->WavFormat.nBlockAlign*8/
                                         (impl->WavFormat.wBitsPerSample*impl->WavFormat.nChannls);
                        }
                        nFindAllFlag++;
                        break;
                    }
                case fact:
                    {
                        if(WAVObject.Dsize>64)
                        {
                            ret = CdxStreamRead(impl->stream, readBuf,WAVObject.Dsize);
                            if(ret != 64)
                            {
                                goto OPENFAILURE;
                            }
                            CdxStreamSeek(impl->stream,WAVObject.Dsize-64, SEEK_CUR);
                        }
                        else
                        {
                            ret = CdxStreamRead(impl->stream, readBuf,WAVObject.Dsize);
                            if(ret != WAVObject.Dsize)
                            {
                                goto OPENFAILURE;
                            }
                        }
                        nDataFactSize = (readBuf[0]&0xff)|(((cdx_int32)readBuf[1]&0xff)<<8)|
                             (((cdx_int32)readBuf[2]&0xff)<<16)|(((cdx_int32)readBuf[3]&0xff)<<24);

                        if(impl->WavFormat.nSamplesPerSec)
                        {
                            dDurationADPCM = (cdx_int64)nDataFactSize*1000000/
                                                        impl->WavFormat.nSamplesPerSec;
                        }
                        if(ulFlag==0)
                        {
                            ulLength +=WAVObject.Dsize;
                        }
                        break;
                    }
                case dataF:
                    {
                        ulFlag = 1;
                        impl->WavFormat.nDataCKSzie        =    WAVObject.Dsize;
                        nFindAllFlag++;
                        if(nFindAllFlag>1)break;
                        CdxStreamSeek(impl->stream,WAVObject.Dsize, SEEK_CUR);
                        break;

                    }
                default:
                    {
                        CdxStreamSeek(impl->stream,WAVObject.Dsize, SEEK_CUR);
                        if(ulFlag==0)ulLength +=WAVObject.Dsize;
                        break;
                    }
                }
            }
        }
    }
    else if(WAVObject.UID ==RIFX)
    {
        impl->nBigEndian = ABS_EDIAN_FLAG_BIG;
        ret = CdxStreamRead(impl->stream, (void *)&TempUid,4);
        if(ret != 4)
        {
            goto OPENFAILURE;
        }
        ulLength +=4;
        if(TempUid == WAVE)
        {
            while(nFindAllFlag<2)
            {
                ret = CdxStreamRead(impl->stream, (void *)&WAVObject, 8);
                if(ret != 8)
                {
                    goto OPENFAILURE;
                }
                WAVObject.Dsize = ((WAVObject.Dsize>>24) & 0X000000FF)
                                 |((WAVObject.Dsize>>8 ) & 0X0000FF00)
                                 |((WAVObject.Dsize<<8 ) & 0X00FF0000)
                                 |((WAVObject.Dsize<<24) & 0XFF000000);
                ulLength +=8;
                switch(WAVObject.UID)
                {
                case fmt:
                    {
                        if(WAVObject.Dsize>64)
                        {
                            ret = CdxStreamRead(impl->stream, readBuf,WAVObject.Dsize);
                            if(ret != 64)
                            {
                                goto OPENFAILURE;
                            }
                            CdxStreamSeek(impl->stream,WAVObject.Dsize-64, SEEK_CUR);
                        }
                        else
                        {
                            ret = CdxStreamRead(impl->stream, readBuf,WAVObject.Dsize);
                            if(ret != WAVObject.Dsize)
                            {
                                goto OPENFAILURE;
                            }
                        }
                        if(ulFlag==0)ulLength +=WAVObject.Dsize;
                        impl->WavFormat.wFormatag = (readBuf[1 ]&0xff)|
                                                    (((cdx_int32)readBuf[0 ]&0xff)<<8);
                        impl->WavFormat.nChannls = (readBuf[3 ]&0xff)|
                                                    (((cdx_int32)readBuf[2 ]&0xff)<<8);
                        impl->WavFormat.nSamplesPerSec = (readBuf[7 ]&0xff)|
                                                         (((cdx_int32)readBuf[6 ]&0xff)<<8)|
                                                         (((cdx_int32)readBuf[5]&0xff)<<16)|
                                                         (((cdx_int32)readBuf[4]&0xff)<<24);
                        impl->WavFormat.nAvgBytesperSec = (readBuf[11]&0xff)|
                                                          (((cdx_int32)readBuf[10]&0xff)<<8)|
                                                          (((cdx_int32)readBuf[9]&0xff)<<16)|
                                                          (((cdx_int32)readBuf[8]&0xff)<<24);
                        impl->WavFormat.nBlockAlign = (readBuf[13]&0xff)|
                                                      (((cdx_int32)readBuf[12]&0xff)<<8);
                        impl->WavFormat.wBitsPerSample = (readBuf[15]&0xff)|
                                                         (((cdx_int32)readBuf[14]&0xff)<<8);

                        if(WAVObject.Dsize>16)
                        {
                            impl->WavFormat.sbSize = (readBuf[17]&0xff)|
                                                     (((cdx_int32)readBuf[16]&0xff)<<8);
                            impl->WavFormat.nSamplesPerBlock = (readBuf[21]&0xff)|
                                                               (((cdx_int32)readBuf[20]&0xff)<<8)|
                                                               (((cdx_int32)readBuf[19]&0xff)<<16)|
                                                               (((cdx_int32)readBuf[18]&0xff)<<24);
                            if(impl->WavFormat.nSamplesPerBlock == 0)
                            {
                                impl->WavFormat.nSamplesPerBlock = impl->WavFormat.nBlockAlign*8/
                                         (impl->WavFormat.wBitsPerSample*impl->WavFormat.nChannls);
                            }
                        }
                        else
                        {
                            impl->WavFormat.nSamplesPerBlock    = 1;
                            if(impl->WavFormat.wFormatag==0x02 ||impl->WavFormat.wFormatag==0x11)
                            {
                                impl->WavFormat.nSamplesPerBlock = impl->WavFormat.nBlockAlign*8/
                                         (impl->WavFormat.wBitsPerSample*impl->WavFormat.nChannls);
                            }
                        }
                        nFindAllFlag++;
                        break;
                    }
                case fact:
                    {
                        if(WAVObject.Dsize>64)
                        {
                            ret = CdxStreamRead(impl->stream, readBuf,WAVObject.Dsize);
                            if(ret != 64)
                            {
                                goto OPENFAILURE;
                            }
                            CdxStreamSeek(impl->stream,WAVObject.Dsize-64, SEEK_CUR);
                        }
                        else
                        {
                            ret = CdxStreamRead(impl->stream, readBuf,WAVObject.Dsize);
                            if(ret != WAVObject.Dsize)
                            {
                                goto OPENFAILURE;
                            }
                        }
                        nDataFactSize = (readBuf[3]&0xff)|(((cdx_int32)readBuf[2]&0xff)<<8)|
                                                          (((cdx_int32)readBuf[1]&0xff)<<16)|
                                                          (((cdx_int32)readBuf[0]&0xff)<<24);
                        if(impl->WavFormat.nSamplesPerSec)
                        {
                            dDurationADPCM = (cdx_int64)nDataFactSize*1000000/
                                                        impl->WavFormat.nSamplesPerSec;
                        }
                        if(ulFlag==0)
                        {
                            ulLength +=WAVObject.Dsize;
                        }
                        break;
                    }
                case dataF:
                    {
                        ulFlag = 1;
                        impl->WavFormat.nDataCKSzie        =    WAVObject.Dsize;
                        nFindAllFlag++;
                        if(nFindAllFlag>1)break;
                        CdxStreamSeek(impl->stream,WAVObject.Dsize, SEEK_CUR);
                        break;
                    }
                default:
                    {
                        CdxStreamSeek(impl->stream,WAVObject.Dsize, SEEK_CUR);
                        if(ulFlag==0)ulLength +=WAVObject.Dsize;
                        break;

                    }
                }//switch
            }//while
        }//wav
    }//rifx
    if(nFindAllFlag <2)
    {
        goto OPENFAILURE;
    }

    impl->dFileSize = CdxStreamSize(impl->stream);
    CdxStreamSeek(impl->stream,ulLength, SEEK_SET);
    //during
    if((impl->WavFormat.wFormatag == WAVE_FORMAT_DVI_ADPCM)&&(impl->WavFormat.nChannls))
    {
        cdx_int16 nBlockAlign = impl->WavFormat.nBlockAlign;
        cdx_int16 nChannls    = impl->WavFormat.nChannls;
        impl->WavFormat.nSamplesPerBlock = ((nBlockAlign-4*nChannls)*2+nChannls)/nChannls;
    }

    if( ( (impl->WavFormat.wFormatag == WAVE_FORMAT_PCM)||
          (impl->WavFormat.wFormatag == WAVE_FORMAT_EXTENSIBLE)||
          (impl->WavFormat.wFormatag == WAVE_FORMAT_ALAW)||
          (impl->WavFormat.wFormatag == WAVE_FORMAT_MULAW))
           && (impl->WavFormat.nSamplesPerSec) )
    {
        cdx_int32 nDataCKSzie = impl->WavFormat.nDataCKSzie;
        cdx_int16 nChannls    = impl->WavFormat.nChannls;
        cdx_int16 wBitsPerSample = impl->WavFormat.wBitsPerSample;
        cdx_int32 nSamplesPerSec = impl->WavFormat.nSamplesPerSec;
        impl->ulDuration = ((cdx_int64)(nDataCKSzie/(nChannls*wBitsPerSample/8))*1000000/
                                        nSamplesPerSec);
    }
    else if(impl->WavFormat.nSamplesPerBlock)
    {
        cdx_int32 nDataCKSzie = impl->WavFormat.nDataCKSzie;
        cdx_int16 nSamplesPerBlock = impl->WavFormat.nSamplesPerBlock;
        cdx_int32 nSamplesPerSec = impl->WavFormat.nSamplesPerSec;
        cdx_int16 nBlockAlign = impl->WavFormat.nBlockAlign;
        impl->ulDuration = ((cdx_int64)nDataCKSzie*1000000*nSamplesPerBlock/
                                       ((cdx_int64)nBlockAlign*nSamplesPerSec));
    }
    else
    {
        if(impl->WavFormat.nAvgBytesperSec)
        {
            impl->ulDuration = ((cdx_int64)impl->WavFormat.nDataCKSzie*1000000/
                                           impl->WavFormat.nAvgBytesperSec);
        }
    }
    if(abs(impl->ulDuration-dDurationADPCM) <=
        ((cdx_int64)impl->WavFormat.nSamplesPerBlock*1000000/impl->WavFormat.nSamplesPerSec))
    {
        impl->ulDuration=dDurationADPCM;
    }
    impl->ulDuration /=1000;
    ///////////////////////////////////////////////
    impl->nHeadLen = ulLength;
    impl->dFileOffset = ulLength;

    switch(impl->WavFormat.wFormatag)
    {
        case WAVE_FORMAT_EXTENSIBTS:
            ret = CdxStreamRead(impl->stream, readBuf,4);
            if(ret != 4)
            {
                goto OPENFAILURE;
            }
            if(impl->nBigEndian)
            {
                impl->nFrameLen = ((int)readBuf[1] <<8)|((int)readBuf[0] & 0xff);
            }
            else
            {
                impl->nFrameLen = ((int)readBuf[0] <<8)|((int)readBuf[1] & 0xff);
            }
            impl->nFrameSamples = impl->nFrameLen * 8/
                                 (impl->WavFormat.nChannls * impl->WavFormat.wBitsPerSample);
            impl->nFrameLen +=4;
            CdxStreamSeek(impl->stream,impl->nHeadLen, SEEK_SET);
            break;
        case WAVE_FORMAT_EXTENSIBTSMIRACAST:
            impl->nFrameLen = 1924;
            impl->nFrameSamples = 1920 * 8 /
                                  (impl->WavFormat.nChannls *impl->WavFormat.wBitsPerSample);
            break;
        case WAVE_FORMAT_PCM:
        case WAVE_FORMAT_EXTENSIBLE:
            if(impl->WavFormat.nBlockAlign*8 !=
               impl->WavFormat.nChannls*impl->WavFormat.wBitsPerSample)
            {
                impl->WavFormat.nBlockAlign = impl->WavFormat.nChannls*
                                              impl->WavFormat.wBitsPerSample>>3;
            }
        case WAVE_FORMAT_ALAW:        /*0x0006  ALAW */
        case WAVE_FORMAT_MULAW:        /*0x0007  MULAW */
            impl->nFrameLen = READBUF_SIZE_1 * impl->WavFormat.nChannls;
            impl->nFrameSamples = impl->nFrameLen /impl->WavFormat.nBlockAlign;
            break;
        case WAVE_FORMAT_DVD_LPCM:
            impl->nFrameLen = 1920;
            break;
        default :
            impl->nFrameLen = impl->WavFormat.nBlockAlign;
            impl->nFrameSamples = impl->WavFormat.nSamplesPerBlock;
            break;
    }
    impl->mErrno = PSR_OK;
    pthread_cond_signal(&impl->cond);
    return 0;
OPENFAILURE:
    CDX_LOGE("WavOpenThread fail!!!");
    impl->mErrno = PSR_OPEN_FAIL;
    pthread_cond_signal(&impl->cond);
    return -1;
}

static cdx_int32 __WavParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    struct WavParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct WavParserImplS, base);
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
    impl->nFlags = cmd;

    return CDX_SUCCESS;
}

static cdx_int32 __WavParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    struct WavParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct WavParserImplS, base);
    pkt->type = CDX_MEDIA_AUDIO;
    pkt->length = impl->nFrameLen;
    pkt->pts = (cdx_int64)impl->nFrames*impl->nFrameSamples *1000000/
                          impl->WavFormat.nSamplesPerSec;//-1;

    pkt->flags |= (FIRST_PART|LAST_PART);

    return CDX_SUCCESS;
}

static cdx_int32 __WavParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    //cdx_int32 ret;
    cdx_int32 read_length;
    struct WavParserImplS *impl = NULL;
    CdxStreamT *cdxStream = NULL;
    //unsigned char* ptr = (unsigned char*)pkt->buf;

    impl = CdxContainerOf(parser, struct WavParserImplS, base);
    cdxStream = impl->stream;
    if(impl->WavFormat.nDataCKSzie!=0x7fffffff)
    {
        if(impl->dFileOffset + pkt->length>(unsigned int)(impl->WavFormat.nDataCKSzie))
        {
            pkt->length = impl->WavFormat.nDataCKSzie - impl->dFileOffset;    //last packet
        }
    }
    if(pkt->length <= pkt->buflen)
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->length);
    }
    else
    {
        read_length = CdxStreamRead(cdxStream, pkt->buf, pkt->buflen);
        read_length += CdxStreamRead(cdxStream, pkt->ringBuf,    pkt->length - pkt->buflen);
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
    //logv("****len:%d,plen:%d",read_length,pkt->length);
    pkt->length = read_length;
    impl->dFileOffset += read_length;
    impl->nFrames++;
    if(read_length == 0)
    {
        CDX_LOGW("audio parse no data");
        return CDX_FAILURE;
    }

    // TODO: fill pkt
    return CDX_SUCCESS;
}

static cdx_int32 __WavParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    //cdx_int32 ret, i = 0;
    struct WavParserImplS *impl;
    struct CdxProgramS *cdxProgram = NULL;

    impl = CdxContainerOf(parser, struct WavParserImplS, base);
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
    mediaInfo->fileSize = impl->dFileSize;

    cdxProgram->duration = impl->ulDuration;
    cdxProgram->audio[0].eCodecFormat    = AUDIO_CODEC_FORMAT_PCM;
    cdxProgram->audio[0].eSubCodecFormat = impl->WavFormat.wFormatag | impl->nBigEndian;
    cdxProgram->audio[0].nChannelNum     = impl->WavFormat.nChannls;
    cdxProgram->audio[0].nBitsPerSample  = impl->WavFormat.wBitsPerSample;
    cdxProgram->audio[0].nSampleRate     = impl->WavFormat.nSamplesPerSec;
    cdxProgram->audio[0].nAvgBitrate     = impl->WavFormat.nAvgBytesperSec;
    //cdxProgram->audio[0].nMaxBitRate;
    //cdxProgram->audio[0].nFlags
    cdxProgram->audio[0].nBlockAlign     = impl->WavFormat.nBlockAlign;
    //CDX_LOGD("eSubCodecFormat:0x%04x,ch:%d,fs:%d",cdxProgram->audio[0].eSubCodecFormat,
              //cdxProgram->audio[0].nChannelNum,cdxProgram->audio[0].nSampleRate);
    return CDX_SUCCESS;
}

static cdx_int32 __WavParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    struct WavParserImplS *impl = NULL;
    cdx_int32 file_location = 0;
    cdx_int32 nFrames = 0;
    impl = CdxContainerOf(parser, struct WavParserImplS, base);
    nFrames = (timeUs/1000000) * impl->WavFormat.nSamplesPerSec /impl->nFrameSamples;
    file_location = nFrames *  impl->nFrameLen;
    file_location += impl->nHeadLen;
    //CDX_LOGD("wav seek :%d pts:%lld,now pts:%lld,nFrames:%d,samples:%d",file_location,timeUs,

    if(file_location!= impl->dFileOffset)
    {
        if(impl->dFileSize > 0 && file_location > impl->dFileSize)
        {
            CDX_LOGW("CdxStreamSeek overange, seek to(%d), filesz(%lld)",
                    file_location, impl->dFileSize);
            file_location = impl->dFileSize;
        }

        file_location = WAV_MAX(file_location, 0);

        if(CdxStreamSeek(impl->stream,file_location,SEEK_SET))
        {
              CDX_LOGE("CdxStreamSeek fail");
              return CDX_FAILURE;
        }
    }
    impl->nFrames = nFrames;
    impl->dFileOffset = file_location;
    // TODO: not implement now...
    CDX_LOGI("TODO, seek to now...");
    return CDX_SUCCESS;
}

static cdx_uint32 __WavParserAttribute(CdxParserT *parser)
{
    struct WavParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct WavParserImplS, base);
    return 0;
}

static cdx_int32 __WavParserGetStatus(CdxParserT *parser)
{
    struct WavParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct WavParserImplS, base);
#if 0
    if (CdxStreamEos(impl->stream))
    {
        CDX_LOGE("file PSR_EOS! ");
        return PSR_EOS;
    }
#endif
    return impl->mErrno;
}

static cdx_int32 __WavParserClose(CdxParserT *parser)
{
    struct WavParserImplS *impl = NULL;

    impl = CdxContainerOf(parser, struct WavParserImplS, base);
    CdxStreamClose(impl->stream);
    pthread_cond_destroy(&impl->cond);
    CdxFree(impl);
    return CDX_SUCCESS;
}

static struct CdxParserOpsS wavParserOps =
{
    .control      = __WavParserControl,
    .prefetch     = __WavParserPrefetch,
    .read         = __WavParserRead,
    .getMediaInfo = __WavParserGetMediaInfo,
    .seekTo       = __WavParserSeekTo,
    .attribute    = __WavParserAttribute,
    .getStatus    = __WavParserGetStatus,
    .close        = __WavParserClose,
    .init         = WavInit
};

static cdx_uint32 aw_bytestream_get_be16(cdx_uint8** pptr)
{
    cdx_uint32 value = 0;
    cdx_uint8* ptr = NULL;

    ptr = *pptr;

    value = ptr[0]<<8 | ptr[1];
    ptr += 2;

    *pptr = ptr;
    return value;
}

#define MAXREADBYTES 1024
#define MAXBYTESCHECKED  (48 * 1024)

static const uint32_t MpegMask = 0xfffe0c00;
extern cdx_bool GetMPEGAudioFrameSize(
        uint32_t header, size_t *frameSize,
        int *outSamplingRate, int *outChannels,
        int *outBitrate, int *outNumSamples) ;
static cdx_int32 Mp3Probe(CdxStreamProbeDataT *p){
    //const sp<DataSource> &source, uint32_t match_header,
    //off64_t *inout_pos, off64_t *post_id3_pos, uint32_t *out_header) {
    cdx_int64  inout_pos,post_id3_pos;
    cdx_uint32 out_header,match_header;
    cdx_uint8 *source = (cdx_uint8 *)p->buf;

    post_id3_pos = 0;
    inout_pos = 0;
    match_header = 0;
    cdx_int64 pos1 = 0;

    if (inout_pos == 0) {
        // Skip an optional ID3 header if syncing at the very beginning
        // of the datasource.
        for (;;) {
            cdx_uint8 id3header[10];
            pos1 += sizeof(id3header);
            if(pos1 > p->len)
            {
                return 0;
            }
            memcpy(id3header,source + inout_pos,sizeof(id3header));
            //if (source->readAt(*inout_pos, id3header, sizeof(id3header))
              //      < (ssize_t)sizeof(id3header)) {
                // If we can't even read these 10 bytes, we might as well bail
                // out, even if there _were_ 10 bytes of valid mp3 audio data...
                //return false;
            //}
            if (memcmp("ID3", id3header, 3)) {
                break;
            }
            // Skip the ID3v2 header.
            cdx_int32 len =
                ((id3header[6] & 0x7f) << 21)
                | ((id3header[7] & 0x7f) << 14)
                | ((id3header[8] & 0x7f) << 7)
                | (id3header[9] & 0x7f);

            len += 10;
            inout_pos += len;
            CDX_LOGD("Wav parser : skipped ID3 tag, new starting offset is %lld (0x%016llx)",
                 inout_pos, inout_pos);
            if(inout_pos != 0)
            {
                CDX_LOGD("Wav parser : this is ID3 first : %lld",inout_pos);
                return 0;
            }
        }
        post_id3_pos = inout_pos;
    }
    cdx_int64 pos = inout_pos;
    cdx_int32  valid = 0;

    //const cdx_int32 kMaxReadBytes = 1024;
    //const cdx_int32 kMaxBytesChecked = 128 * 1024;
    cdx_uint8 buf[MAXREADBYTES];
    cdx_int32 bytesToRead = MAXREADBYTES;
    cdx_int32 totalBytesRead = 0;
    cdx_int32 remainingBytes = 0;
    cdx_int32 reachEOS = 0;
    cdx_uint8 *tmp = buf;

    do {
        if (pos >= inout_pos + MAXBYTESCHECKED) {
            // Don't scan forever.
            CDX_LOGW("giving up at offset %lld", pos);
            break;
        }

        if (remainingBytes < 4) {
            if (reachEOS) {
                break;
            } else {
                memcpy(buf, tmp, remainingBytes);
                bytesToRead = MAXREADBYTES - remainingBytes;
                /*
                 * The next read position should start from the end of
                 * the last buffer, and thus should include the remaining
                 * bytes in the buffer.
                 */
                if(pos + remainingBytes + bytesToRead > p->len)
                {
                    reachEOS = 1;
                    bytesToRead = p->len - pos - remainingBytes;
                }
                memcpy(buf + remainingBytes,source + pos + remainingBytes,bytesToRead);
                totalBytesRead = bytesToRead;
                //totalBytesRead = source->readAt(pos + remainingBytes,
                                               // buf + remainingBytes,
                                               // bytesToRead);
                //if (totalBytesRead <= 0) {
                 //   break;
                //}
                totalBytesRead += remainingBytes;
                remainingBytes = totalBytesRead;
                tmp = buf;
                continue;
            }
        }
        cdx_uint32 header = AV_RB32(tmp);
        if (match_header != 0 && (header & MpegMask) != (match_header & MpegMask)) {
            ++pos;
            ++tmp;
            --remainingBytes;
            continue;
        }

        size_t frame_size;
        cdx_int32  sample_rate,num_channels, bitrate, NumSamples;
        if (!GetMPEGAudioFrameSize(
                    header, &frame_size,
                    &sample_rate, &num_channels, &bitrate, &NumSamples)) {
            ++pos;
            ++tmp;
            --remainingBytes;
            continue;
        }
        CDX_LOGV("found possible 1st frame at %lld (header = 0x%08x)", pos, header);
        // We found what looks like a valid frame,
        // now find its successors.

        cdx_int64 test_pos = pos + frame_size;
        valid = 1;
        cdx_int32 j = 0;
        for (j = 0; j < 6; ++j) {
            cdx_uint8 tmp[4];
            if(test_pos + 4 > p->len)
            {
                valid = 0;
                break;
            }
            memcpy(tmp, source + test_pos, 4);
            //if (source->readAt(test_pos, tmp, 4) < 4) {
               // valid = false;
               // break;
            //}

            cdx_uint32 test_header = AV_RB32(tmp);
            CDX_LOGV("subsequent header is %08x", test_header);
            if ((test_header & MpegMask) != (header & MpegMask)) {
                valid = 0;
                break;
            }

            size_t test_frame_size;
            if (!GetMPEGAudioFrameSize(
                        test_header, &test_frame_size,
                        &sample_rate, &num_channels, &bitrate, &NumSamples)) {
                valid = 0;
                break;
            }

            CDX_LOGV("found subsequent frame #%d at %lld", j + 2, test_pos);
            test_pos += test_frame_size;
        }

        if (valid) {
            inout_pos = pos;
            out_header = header;

        } else {
            CDX_LOGW("Wav parser : no dice, no valid sequence of frames found.");
        }

        ++pos;
        ++tmp;
        --remainingBytes;
    } while (!valid);

    return valid;
}

static cdx_int32 DcaProbe(CdxStreamProbeDataT *p)
{
    cdx_char *d,*dp;
    d = p->buf;
    cdx_int32 markers[3] = {0};
    cdx_int32 sum, max;
    cdx_uint32 state = -1;
    for(; d < (p->buf+p->len)-2; d+=2) {
        dp = d;
        state = (state << 16) | aw_bytestream_get_be16((cdx_uint8**)&dp);

        /* regular bitstream */
        if (state == AW_DCA_MARKER_RAW_BE || state == AW_DCA_MARKER_RAW_LE)
            markers[0]++;

        /* 14 bits big-endian bitstream */
        if (state == AW_DCA_MARKER_14B_BE)
            if ((aw_bytestream_get_be16((cdx_uint8**)&dp) & 0xFFF0) == 0x07F0)
                markers[1]++;

        /* 14 bits little-endian bitstream */
        if (state == AW_DCA_MARKER_14B_LE)
            if ((aw_bytestream_get_be16((cdx_uint8**)&dp) & 0xF0FF) == 0xF007)
                markers[2]++;
    }
    sum = markers[0] + markers[1] + markers[2];
    max = markers[1] > markers[0];
    max = markers[2] > markers[max] ? 2 : max;
    if (markers[max] > 3 && p->len / markers[max] < 32*1024 &&  markers[max] * 4 > sum * 3){
        CDX_LOGD("DTS PARSER");
        return CDX_TRUE;
    }else if(sum)
    {
        CDX_LOGD("DTS PARSER");
        return CDX_TRUE;
    }

    return CDX_FALSE;
}

static cdx_int32 WavProbe(CdxStreamProbeDataT *p)
{
    cdx_char *d;

    d = p->buf;

    if(Mp3Probe(p))
    {
        CDX_LOGD("Wav wrapped mp3, return out!!");
        return CDX_FALSE;
    }
    if(d[8] == 'W' && d[9] == 'A' && d[10] == 'V' && d[11] == 'E' )
    {
        if((d[0] == 'R' && d[1] == 'I' && d[2] == 'F' && d[3] == 'F' )
                    || (d[0] == 'R' && d[1] == 'I' && d[2] == 'F' && d[3] == 'X'))
        {
            cdx_int32 is_dca = DcaProbe(p);
            if(is_dca)
                return CDX_FALSE;
            else
                return CDX_TRUE;
        }
    }
    CDX_LOGE("audio probe fail!!!");
    return CDX_FALSE;
}

static cdx_uint32 __WavParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_CHECK(probeData);
    if(probeData->len < 32)
    {
        CDX_LOGW("Probe data is not enough.");
        return 0;
    }

    if(!WavProbe(probeData))
    {
        CDX_LOGE("wav probe failed.");
        return 0;
    }
    return 100;
}

static CdxParserT *__WavParserOpen(CdxStreamT *stream, cdx_uint32 nFlags)
{
    //cdx_int32 ret = 0;
    struct WavParserImplS *impl;
    impl = CdxMalloc(sizeof(*impl));
    (void)nFlags;

    memset(impl, 0x00, sizeof(*impl));
    impl->stream = stream;
    impl->base.ops = &wavParserOps;
    pthread_cond_init(&impl->cond, NULL);
    //ret = pthread_create(&impl->openTid, NULL, WavOpenThread, (void*)impl);
    //CDX_FORCE_CHECK(!ret);
    impl->mErrno = PSR_INVALID;
    return &impl->base;
}

struct CdxParserCreatorS wavParserCtor =
{
    .probe = __WavParserProbe,
    .create  = __WavParserOpen
};
