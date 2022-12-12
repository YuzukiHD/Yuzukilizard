/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : mpgOpen.c
* Description :open mpg file
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment : open mpg file
*
*
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "mpgOpen"
#include <cdx_log.h>

#include "CdxMpgParser.h"
#include "CdxMpgParserImpl.h"
#include "mpgFunc.h"

static int parser_mpeg_frame_rate[16] =
{
    00000,23976,24000,25000,
    29970,30000,50000,59940,
    60000,00000,00000,00000,
    00000,00000,00000,00000
};


/**************************************************
MpgOpenReaderOpenFile()
Functionality : Open the mpeg file to be read
Return value  :
0   allright
-1   file open error
-2   wrong format
-3     malloc error
**************************************************/
cdx_uint8 CheckFirstPack(CdxMpgParserT *MpgParser, cdx_uint8 *buf)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint32 nNext32bits = 0;
    cdx_uint8  nSize       = 0;
    cdx_uint8 *pOrgBuf     = NULL;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;
    pOrgBuf = buf;
    if(buf[0]==0x00 && buf[1]==0x00 && buf[2]==0x01 && buf[3]==0xba)
    {
       return 0;
    }
    else
    {
        if((*buf>>4) == 0x02)
        {
            mMpgParserCxt->bIsISO11172Flag = CDX_TRUE;
        }
        else if((*buf>>6) == 0x01)
        {
           mMpgParserCxt->bIsISO11172Flag = CDX_FALSE;
        }
        if(mMpgParserCxt->bIsISO11172Flag)
        {
            buf += (12-4);
        }
        else
        {
            buf += (13-4);
            nSize = buf[0]&7;
            buf += nSize+1;
        }
        if(MpgOpenShowDword(buf)==MPG_SYSTEM_HEADER_START_CODE)
        {
            nSize = (buf[4]<<8) | buf[5];
            buf += nSize + 6;
            if(MpgParser->bIsVOB == 1)
                return 2;
        }
        nNext32bits = MpgOpenShowDword(buf);
        if(((nNext32bits & 0xf0)!=MPG_VIDEO_ID) && ((nNext32bits & 0xe0)!=MPG_AUDIO_ID) &&
            (nNext32bits !=MPG_PRIVATE_STREAM_1))
        {
            return 0;
        }
        else
        {
            if(mMpgParserCxt->bIsISO11172Flag == CDX_TRUE)
            {
                if(MpgTimeMpeg1ReadPackSCR(pOrgBuf, &mMpgParserCxt->nStartScrLow,
                                   &mMpgParserCxt->nStartScrHigh) == 0)
                {
                    mMpgParserCxt->nBaseSCR = (mMpgParserCxt->nStartScrLow>>1)
                                              |(mMpgParserCxt->nStartScrHigh<<31);
                    return 1;
                }
            }
            else
            {
                if(MpgTimeMpeg2ReadPackSCR(pOrgBuf, &mMpgParserCxt->nStartScrLow,
                                 &mMpgParserCxt->nStartScrHigh,NULL) == 0)
                {
                    mMpgParserCxt->nBaseSCR = (mMpgParserCxt->nStartScrLow>>1)
                                            |(mMpgParserCxt->nStartScrHigh<<31);
                    return 1;
                }
            }
        }
    }
    return 0;
}

cdx_uint32 CheckStreamMap(CdxMpgParserT *MpgParser, cdx_uint8 *buf, cdx_uint32 offset)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint8  nType         = 0;
    cdx_uint8  nEsId         = 0;
    cdx_uint32 off           = 0;
    cdx_uint32 nPsInfoLength = 0;
    cdx_uint32 nEsInfoLength = 0; //psm_length;
    cdx_int32  nEsMapLength  = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    off = offset + 4;
    //nPsInfoLength = (buf[off]<<8) + buf[off+1];
    off += 4;
    nPsInfoLength = (buf[off]<<8) + buf[off+1];
    off += 2+nPsInfoLength;

    nEsMapLength = (buf[off]<<8) + buf[off+1];
    off += 2;
    offset = off;

    while (nEsMapLength >= 4)
    {
        nType = buf[off++];
        nEsId = buf[off++];
        mMpgParserCxt->pPsmEsType[nEsId] = nType;
        nEsInfoLength =(buf[off]<<8) + buf[off+1];
        off += 2+nEsInfoLength;
        nEsMapLength -= 4 + nEsInfoLength;

        switch(nType)
        {
            case CDX_STREAM_TYPE_VIDEO_MPEG1:
                MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG1;
                break;
            case CDX_STREAM_TYPE_VIDEO_MPEG2:
                MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
                break;
            case CDX_STREAM_TYPE_VIDEO_MPEG4:
                MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG4;
                break;
            case CDX_STREAM_TYPE_VIDEO_CODEC_FORMAT_H264:
                MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
                break;
            case CDX_STREAM_TYPE_VIDEO_CODEC_FORMAT_H265:
                MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_H265;
                break;
            case CDX_STREAM_TYPE_AUDIO_MPEG1:
            case CDX_STREAM_TYPE_AUDIO_MPEG2:
                CDX_LOGV("need process audio info.\n");
                break;
            case CDX_STREAM_TYPE_AUDIO_AAC:
                CDX_LOGV("need process audio info.\n");
                break;
            case CDX_STREAM_TYPE_CDX_AUDIO_AC3:
                CDX_LOGV("need process audio info.\n");
                break;
            default: break;
        }
        offset = off-1;
     }
     return offset;
}

cdx_uint8 CheckVideoIdNo1172(CdxMpgParserT *MpgParser, cdx_uint8 *buf,
    cdx_uint32 off, cdx_uint32 pes_len)
{
    cdx_uint8 nRet  = 0;
    cdx_uint8 nCore = 0;
    cdx_uint8 nLen  = 0;
    MpgParserContextT  *mMpgParserCxt;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    off ++;
    pes_len --;
    nCore = buf[off];
    nLen = buf[off+1];
    pes_len -= nLen+2;
    off += 2;

    if((nCore & 0xc0) == 0x80)             //if(PTS_DTS_flags=='10'
    {
        off += 5;
        nLen -= 5;
    }
    else if((nCore & 0xc0) == 0xc0)        //if(PTS_DTS_flags=='11'
    {
        off += 10;
        nLen -= 10;
    }

    if((nCore & 0x20) && nLen > 0)         //ESCR_flag
    {
        off += 6;
        nLen -= 6;
    }

    if((nCore & 0x10) && nLen > 0)           //ES_rate_flag
    {
        off += 3;
        nLen -= 3;
    }

    if((nCore & 0x08) && nLen> 0)            //ES_trick_mode_flag
    {
        off += 1;
        nLen --;
    }

    if((nCore & 0x04) && nLen > 0)           //addtional_copy_info_flag
    {
        off += 1;
        nLen --;
    }
    if((nCore & 0x02) && nLen > 0)          //PES_CRC__flag
    {
        off += 2;
        nLen -= 2;
    }
    if((nCore & 0x01) && nLen > 0)          //PES_extension__flag
    {
        nCore = buf[off];
        off ++;
        nLen --;

        if((nCore & 0x80) && nLen > 0)      //PES_private_data_flag
        {
            off += 16;
            nLen -= 16;
        }
        if(nCore & 0x40)                 //pack_header_field_flag
        {
            nCore = buf[off];
            off += nCore+1;
            nLen -= nCore+1;
        }
        if((nCore & 0x20) && nLen > 0)      //program_packet_sequence_counter_flag
        {
            off++;
            if(buf[off] & 0x40)
                MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG1;
            else
                MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
            off++;
            nLen -= 2;
        }
        if((nCore & 0x10) && nLen > 0)       //P-STD_buffer flag
        {
            off += 2;
            nLen -= 2;
        }
        if((nCore & 1) && nLen > 0)           //PES_extension_flag_2
        {
            nLen -= 1 + (buf[off] & 0x7f);
            off += 1 + (buf[off] & 0x7f);
        }
    }

    while (buf[off]==0xff && nLen >0)
    {
        off ++;
        nLen --;
    }

    if(nLen != 0)
    {
        CDX_LOGW("pes header error\n");
    }
    while(pes_len&&((buf+off+3)<(mMpgParserCxt->mDataChunkT.pStartAddr+MAX_CHUNK_BUF_SIZE)))
    {
        if(buf[off]==0x00 && buf[off+1]==0x00
            && buf[off+2]==0x01 && buf[off+3]==0xb3) //check sequence header in 13818-2
        {
            mMpgParserCxt->bRecordSeqInfFlag = 1;
            memcpy(mMpgParserCxt->nSequenceBuf, buf+off, MPG_SEQUENCE_LEN);
            off += 4;
                  // MpgParser->nFstSeqAddr = off;
            MpgParser->mVideoFormatT.nWidth = (buf[off]<<4) | (buf[off+1]>>4);
            MpgParser->mVideoFormatT.nHeight = ((buf[off+1]&0x0f)<<8) | (buf[off+2]);
            MpgParser->mVideoFormatT.nFrameRate = parser_mpeg_frame_rate[buf[off+3]&0x0f];

            if(MpgParser->mVideoFormatT.nFrameRate)
            {
                MpgParser->mVideoFormatT.nFrameDuration = (1000 * 1000)
                                     /MpgParser->mVideoFormatT.nFrameRate;
            }
            nRet = 1;
            break;
        }
        else if(buf[off]==0x00 && buf[off+1]==0x00 && buf[off+2]==0x01 && buf[off+3]==0x67)
        {
            //* detect h264
            MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
            nRet = 1;
            break;
        }
        else if(buf[off]==0x00 && buf[off+1]==0x00 && buf[off+2]==0x01 && buf[off+3]==0x42)
        {
            //* detect h265
            CDX_LOGD("***maybe h265***");
            MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_H265;
            nRet = 1;
            break;
        }
        off ++;
        pes_len--;
    }
    return nRet;
}

cdx_uint8 CheckVideoId1172(CdxMpgParserT *MpgParser, cdx_uint8 *buf,
                         cdx_uint32 off, cdx_uint32 pes_len)
{
    cdx_uint8           nRet          = 0;
    MpgParserContextT  *mMpgParserCxt = NULL;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if((buf[off] & 0xc0) == 0x40)
    {
        off += 2;
        pes_len -= 2;
    }
    if((buf[off] & 0xf0) == 0x20)
    {
        off+= 5;
        pes_len -= 5;
    }
    else if((buf[off] & 0xf0) == 0x30)
    {
        off += 10;
        pes_len -= 10;
    }
    else if(buf[off] == 0x0f)
    {
        off += 1;
        pes_len -= 1;
    }

    while (pes_len)
    {
        if(buf[off]==0x00 && buf[off+1]==0x00
            && buf[off+2]==0x01 && buf[off+3]==0xb3) //check 11172-2 sequence header
        {
            mMpgParserCxt->bRecordSeqInfFlag = 1;
            memcpy(mMpgParserCxt->nSequenceBuf, buf+off, MPG_SEQUENCE_LEN);
            off += 4;
            MpgParser->nFstSeqAddr = off;
            MpgParser->mVideoFormatT.nWidth = (buf[off]<<4) | (buf[off+1]>>4);
            MpgParser->mVideoFormatT.nHeight = ((buf[off+1]&0x0f)<<8) | (buf[off+2]);
            MpgParser->mVideoFormatT.nFrameRate = parser_mpeg_frame_rate[buf[off+3]&0x0f];

            if(MpgParser->mVideoFormatT.nFrameRate)
            {
                MpgParser->mVideoFormatT.nFrameDuration = (1000 * 1000)
                     / MpgParser->mVideoFormatT.nFrameRate;
            }

            if(!MpgParser->mVideoFormatT.eCodecFormat)
                MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG1;
            nRet = 1;
            break;
        }
        off ++;
        pes_len --;
    }
    return nRet;
}

cdx_uint8 CheckVideoId(CdxMpgParserT *MpgParser, cdx_uint8 *buf, cdx_uint32 offset, cdx_uint32 code)
{
    cdx_uint8    nRet    = 0;
    cdx_uint32   off     = 0;
    cdx_uint32   pes_len = 0;
    MpgParserContextT  *mMpgParserCxt = NULL;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    //MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
    mMpgParserCxt->nVideoId = code;
    pes_len = (buf[offset+1]<<8) | buf[offset+2];
    off = offset+3;//length

    while(buf[off]==0xff)
    {
        off++;
        pes_len --;
    }

    if((buf[off]&0xc0) == 0x80)
    {
        mMpgParserCxt->bIsISO11172Flag = CDX_FALSE;
        if(!MpgParser->mVideoFormatT.eCodecFormat)
            MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
        //MpgParser->mVideoFormatT.eCompressionSubFormat = 2;
        if(CheckVideoIdNo1172(MpgParser, buf, off, pes_len) > 0)
            nRet = 1;
    }
    else
    {
        mMpgParserCxt->bIsISO11172Flag = CDX_TRUE;
        if(!MpgParser->mVideoFormatT.eCodecFormat)
            MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG1;
        if(CheckVideoId1172(MpgParser, buf, off, pes_len) > 0)
            nRet = 1;
    }

    return nRet;
}

cdx_uint8 CheckAudioType(CdxMpgParserT *MpgParser, cdx_uint8 *buf, cdx_uint32 offset)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint8 nRet = 0;
    cdx_uint8 i =0;
    cdx_uint32 off, code;
    cdx_uint8  quantWordLen[3] = {16, 20, 24};
    cdx_uint32 audioSampleFreq[2] = {48000, 96000};

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    off = offset+3;              //length

    while(buf[off]==0xff)
    {
        off++;
    }

    if((buf[off] & 0xc0) == 0x80)
    {
        off ++;
        mMpgParserCxt->bIsISO11172Flag = CDX_FALSE;
        if(!MpgParser->mVideoFormatT.eCodecFormat)
            MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
        //MpgParser->mVideoFormatT.eCompressionSubFormat = 2;
        off += 2+buf[off+1];
    }
    else
    {
        mMpgParserCxt->bIsISO11172Flag = CDX_TRUE;
        if(!MpgParser->mVideoFormatT.eCodecFormat)
            MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG1;

        if((buf[off] & 0xc0) == 0x40)
            off += 2;
        if((buf[off] & 0xf0) == 0x20)
            off+= 5;
        else if((buf[off] & 0xf0) == 0x30)
            off += 10;
        else if(buf[off] == 0x0f)
            off += 1;
    }

    code = buf[off];

    if(code>=0x80 && code <=0x87)
    {
        //MpgParser->bAudioHasAc3Flag = CDX_TRUE;
        //MpgParser->mAudioFormatT.eCodecFormat = AUDIO_AC3;
        //mMpgParserCxt->nAudioId = MPG_PRIVATE_STREAM_1;
        for(i=0; i<MpgParser->nhasAudioNum; i++)
        {
            if((MpgParser->mAudioFormatTArry[i].eCodecFormat==AUDIO_CODEC_FORMAT_AC3)
                &&(mMpgParserCxt->bAudioStreamIdCode[i]==code))
            {
                break;
            }
        }
        if(i == MpgParser->nhasAudioNum)
        {
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].eCodecFormat
                                     = AUDIO_CODEC_FORMAT_AC3;
            mMpgParserCxt->nAudioIdArray[MpgParser->nhasAudioNum] = MPG_PRIVATE_STREAM_1;
            mMpgParserCxt->bAudioStreamIdCode[MpgParser->nhasAudioNum] = code;
            MpgParser->nhasAudioNum += 1;
        }
        nRet = 1;
    }
    else if((code>=0x88 && code <=0x8f)
        ||(code>=0x98 && code <=0x9f))
    {
        //0x90--0x97 is reserved for SDDS in DVD specs
        //MpgParser->mAudioFormatT.eCodecFormat = AUDIO_DTS;
        //mMpgParserCxt->nAudioId = MPG_PRIVATE_STREAM_1;
        for(i=0; i<MpgParser->nhasAudioNum; i++)
        {
            if((MpgParser->mAudioFormatTArry[i].eCodecFormat==AUDIO_CODEC_FORMAT_DTS)
                &&(mMpgParserCxt->bAudioStreamIdCode[i]==code))
            {
                break;
            }
        }

        if(i == MpgParser->nhasAudioNum)
        {
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].eCodecFormat=
                               AUDIO_CODEC_FORMAT_DTS;
            mMpgParserCxt->nAudioIdArray[MpgParser->nhasAudioNum] = MPG_PRIVATE_STREAM_1;
            mMpgParserCxt->bAudioStreamIdCode[MpgParser->nhasAudioNum] = code;
            MpgParser->nhasAudioNum += 1;
        }
        nRet = 1;
    }
    else if(code >=0xa0 && code <=0xaf)
    {
        //MpgParser->mAudioFormatT.eCodecFormat = AUDIO_PCM;
        //*bits_sample = (buf[off+5]>>6) & 0x03;
        //*sample_sec = (buf[off+5]>>4) & 0x03;
        //*nChannelNum = buf[off+5] & 0x07;
        //mMpgParserCxt->nAudioId = MPG_PRIVATE_STREAM_1;
        for(i=0; i<MpgParser->nhasAudioNum; i++)
        {
            if((MpgParser->mAudioFormatTArry[i].eCodecFormat==AUDIO_CODEC_FORMAT_PCM)
                &&(mMpgParserCxt->bAudioStreamIdCode[i]==code))
            {
                break;
            }
        }
        if(i == MpgParser->nhasAudioNum)
        {
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].eCodecFormat =
                                          AUDIO_CODEC_FORMAT_PCM;
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].nBitsPerSample =
                                          quantWordLen[(buf[off+5]>>6)&0x03];
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].nSampleRate =
                                          audioSampleFreq[(buf[off+5]>>4)&0x03];
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].nChannelNum =
                                          (buf[off+5]&0x07)+1;
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].eSubCodecFormat=
                                          WAVE_FORMAT_DVD_LPCM | ABS_EDIAN_FLAG_BIG;
            mMpgParserCxt->nAudioIdArray[MpgParser->nhasAudioNum] = MPG_PRIVATE_STREAM_1;
            mMpgParserCxt->bAudioStreamIdCode[MpgParser->nhasAudioNum] = code;
            MpgParser->nhasAudioNum += 1;
        }
        nRet = 1;
    }
    else if(code >=0xb0 && code <=0xbf)
    {
        //MpgParser->mAudioFormatT.eCodecFormat = AUDIO_MLP;
        //mMpgParserCxt->nAudioId = MPG_PRIVATE_STREAM_1;
        for(i=0; i<MpgParser->nhasAudioNum; i++)
        {
             if((MpgParser->mAudioFormatTArry[i].eCodecFormat==AUDIO_CODEC_FORMAT_MLP)
                &&(mMpgParserCxt->bAudioStreamIdCode[i]==code))
             {
                 break;
             }
        }
        if(i == MpgParser->nhasAudioNum)
        {
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].eCodecFormat =
                                    AUDIO_CODEC_FORMAT_MLP;
            mMpgParserCxt->nAudioIdArray[MpgParser->nhasAudioNum] = MPG_PRIVATE_STREAM_1;
            mMpgParserCxt->bAudioStreamIdCode[MpgParser->nhasAudioNum] = code;
            MpgParser->nhasAudioNum += 1;
        }
        nRet = 1;
    }
    else if(code >=0xc0 && code <=0xcf)         //used for both AC-3 and E-AC-3 in EVOB files
    {
        //MpgParser->mAudioFormatT.eCodecFormat = AUDIO_AC3;
        //mMpgParserCxt->nAudioId = MPG_PRIVATE_STREAM_1;
        for(i=0; i<MpgParser->nhasAudioNum; i++)
        {
            if((MpgParser->mAudioFormatTArry[i].eCodecFormat==AUDIO_CODEC_FORMAT_MLP)
                &&(mMpgParserCxt->bAudioStreamIdCode[i]==code))
            {
                break;
            }
        }
        if(i == MpgParser->nhasAudioNum)
        {
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].eCodecFormat =
                                       AUDIO_CODEC_FORMAT_AC3;
            mMpgParserCxt->nAudioIdArray[MpgParser->nhasAudioNum] = MPG_PRIVATE_STREAM_1;
            mMpgParserCxt->bAudioStreamIdCode[MpgParser->nhasAudioNum] = code;
            MpgParser->nhasAudioNum += 1;
        }
        nRet = 1;
    }
    else if(code>=0x20 && code<=0x3f)
    {
        if(MpgParser->nSubTitleStreamIndex == 0)
        {
            MpgParser->nSubTitleStreamIndex = code;
        }
        nRet = 2;
    }
    return nRet;
}

//check pes_packet,MpgParser is pointed to stream_id
//pes1 is for 11172-1 and pes2 is for 13818-1
int CheckPes(unsigned char *MpgParser, unsigned char *pEndPtr)
{
    int pes1 = 0;
    //13818-1: pes2 is used to check '10' and PTS_DTS_flags
    int pes2 = ((MpgParser[3] & 0xC0) == 0x80)
            &&((MpgParser[4] & 0xC0) != 0x40)//PTS_DTS_flag value '01' is forbidden
            &&((MpgParser[4] & 0xC0) == 0x00
            ||((MpgParser[4]&0xC0)>>2)== (MpgParser[6]&0xF0));

    //11172-1
    for(MpgParser+=3; MpgParser<pEndPtr && *MpgParser == 0xFF; MpgParser++)    //skip stuffing
        ;
    if((*MpgParser&0xC0) == 0x40)
        MpgParser+=2;
    if((*MpgParser&0xF0) == 0x20)
    {
        pes1= MpgParser[0]&MpgParser[2]&MpgParser[4]&1; //check the marker_bit
        MpgParser+=5;
    }
    else if((*MpgParser&0xF0) == 0x30)
    {
        pes1= MpgParser[0]&MpgParser[2]&MpgParser[4]
            &MpgParser[5]&MpgParser[7]&MpgParser[9]&1; //check the marker_bit
        MpgParser+=10;
    }
    else
        pes1 = (*MpgParser == 0x0F);
    return pes1||pes2;
}

void  ProbeAacAudio(CdxMpgParserT *MpgParser, cdx_uint8 *curPtr)
{
    MpgParserContextT *mMpgParserCxt   = NULL;
    cdx_int16  result           = 0;
    cdx_int64  packetLen        = 0;
    cdx_uint32 ptsLow           = 0;
    cdx_uint32 ptsHigh          = 0;
    cdx_uint32 nextCode         = 0;
    cdx_uint8  adtsLayer        = 0;
    cdx_uint8  adtsProfile      = 0;
    cdx_uint8  adtsSampRateIdx  = 0;
    cdx_uint8  adsChannelConfig = 0;
    cdx_uint8  adtsFlag[4]      = {0};


    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if(mMpgParserCxt->bIsISO11172Flag)
    {
        curPtr = MpgReadProcessIsISO11172(MpgParser, curPtr,
                &result, &packetLen, &ptsLow, &ptsHigh);
    }
    else
    {
        curPtr = MpgReadProcessNonISO11172(MpgParser, curPtr,
                &result, &packetLen, &ptsLow, &ptsHigh);
    }
    nextCode = ((curPtr[0]<<8)|(curPtr[1]<<0)) & 0xfff0;

    if(nextCode == 0xfff0)
    {
        adtsLayer        = (curPtr[1] & 0x07) >> 1;
        adtsProfile      = (curPtr[2] & 0xff) >> 6;
        adtsSampRateIdx  = (curPtr[2] & 0x3f) >> 2;
        adsChannelConfig = ((curPtr[2] & 0x01)<<2) | ((curPtr[3]&0xff)>>6);

        adtsFlag[0] = (adtsLayer!= 0) && (adtsLayer!=3);
        adtsFlag[1] = (adtsProfile == AAC_NUM_PROFILES);
        adtsFlag[2] = (adtsSampRateIdx >= NUM_SAMPLE_RATES);
        adtsFlag[3] = (adsChannelConfig >= NUM_DEF_CHAN_MAPS);

        if((adtsFlag[0]==0) && (adtsFlag[1]==0) &&  (adtsFlag[2]==0) &&(adtsFlag[3]==0))
            //MpgParser->mAudioFormatT.eCodecFormat = AUDIO_MPEG_AAC_LC;
            MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].eCodecFormat =
                        AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
    }
}

cdx_uint8 *ProcessStartCode(CdxMpgParserT *MpgParser,cdx_uint8 *checkBufPtr,
            cdx_uint32 startCode,cdx_uint32 retDatalen,cdx_uint32 *packInf, cdx_uint32 *calPackNum)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint16 bufOffset       = 0;
    cdx_uint8  checkPesFlag    = 0;
    cdx_uint8  isVideoPack     = 0;
    cdx_uint8  isMpegAudPack   = 0;
    cdx_uint8  nRet            = 0;
    cdx_uint8  isVC1VideoPack  = 0;
    cdx_uint8  i               = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if(startCode == MPG_SYSTEM_HEADER_START_CODE)           // 0x000001BB
    {
        if(packInf[0] == 1)
        {
            packInf[2] = packInf[3];
            packInf[0] = 0;
        }
        calPackNum[sysPack]++;
        bufOffset = (checkBufPtr[4]<<8) | checkBufPtr[5];
        checkBufPtr += (bufOffset + 6);
    }
    else if(startCode == MPG_PACK_START_CODE)               // 0x000001BA
    {
        if(packInf[0] == 1)
        {
            packInf[3] = CdxStreamTell(mMpgParserCxt->pStreamT) - retDatalen
                        + (checkBufPtr-mMpgParserCxt->mDataChunkT.pStartAddr) + 4;
        }
        if(packInf[1] == 1)
        {
            CheckFirstPack(MpgParser, checkBufPtr+4);
            packInf[1] = 0;
        }
        calPackNum[mpgPack]++;
        checkBufPtr += 4;
    }
    else if(startCode == MPG_PROGRAM_STREAM_MAP)            // 0x000001BC
    {
        bufOffset = CheckStreamMap(MpgParser, checkBufPtr, 0);
        if(MpgParser->mVideoFormatT.eCodecFormat==VIDEO_CODEC_FORMAT_H264 ||
           MpgParser->mVideoFormatT.eCodecFormat==VIDEO_CODEC_FORMAT_H265)
        {
            calPackNum[avcVidPack]++;
            return NULL;
        }
        checkBufPtr += bufOffset;
    }
    else if((startCode == MPG_PADDING_STREAM)
        || (startCode == MPG_PRIVATE_STREAM_2))       // 0x000001BE 0x000001BF
    {
        bufOffset = (checkBufPtr[4]<<8) | checkBufPtr[5];
        checkBufPtr += (bufOffset + 6);
    }
    else        // video pack, audio pack, subtitle pack
    {
        checkPesFlag  = CheckPes(checkBufPtr+3,mMpgParserCxt->mDataChunkT.pStartAddr+retDatalen);
        isVideoPack   = ((startCode&0xf0) == MPG_VIDEO_ID);
        isMpegAudPack = ((startCode>=0x01c0) && (startCode<=0x01df));
        isVC1VideoPack = ((startCode&0xff) == 0xfd);

        if((isVideoPack==1) && (checkPesFlag==1))                                // video pack
        {
            if(calPackNum[vidPack] == 0)
            {
                if(CheckVideoId(MpgParser, checkBufPtr, 3, startCode) == 1)
                    // find the sequence header
                {
                    calPackNum[vidPack]++;
                }
            }
            else
            {
                calPackNum[vidPack]++;
            }
        }
        else if(isVC1VideoPack==1)
        {
            if(calPackNum[vc1VidPack] == 0)
            {
                MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_WMV3;
                //MpgParser->mVideoFormatT.eCompressionSubFormat = 2;
                mMpgParserCxt->nVideoId = startCode;
                mMpgParserCxt->bIsISO11172Flag = CDX_TRUE;
                MpgParser->mVideoFormatT.nWidth = 1920;
                MpgParser->mVideoFormatT.nHeight = 1080;
                MpgParser->mVideoFormatT.nFrameRate = 25000;
                MpgParser->mVideoFormatT.nFrameDuration = 1000 * 1000
                                  / MpgParser->mVideoFormatT.nFrameRate;
            }
            calPackNum[vc1VidPack]++;
        }
        else if((isMpegAudPack==1) && (checkPesFlag==1))                         // audio pack
        {
            for(i=0; i<MpgParser->nhasAudioNum; i++)
            {
                if(startCode ==  mMpgParserCxt->nAudioIdArray[i])
                {
                    break;
                }
            }

            if(i == MpgParser->nhasAudioNum)
            {
                MpgParser->mAudioFormatTArry[MpgParser->nhasAudioNum].eCodecFormat
                                   = AUDIO_CODEC_FORMAT_MP2;
                mMpgParserCxt->nAudioIdArray[MpgParser->nhasAudioNum] = startCode;
                ProbeAacAudio(MpgParser, checkBufPtr+4);
                calPackNum[audPack]++;
                MpgParser->nhasAudioNum += 1;
            }
        }
        else if(startCode==MPG_PRIVATE_STREAM_1)
        {
            nRet = CheckAudioType(MpgParser, checkBufPtr, 3);
            if(nRet == 1)
            {
                calPackNum[audPack]++;
            }
            else if(nRet == 2)
            {
                calPackNum[subPack]++;
            }
        }
       else if(startCode == MPG_SEQUENCE_HEADER_CODE)// sequence header
       {
            calPackNum[vidSeq]++;
            if(calPackNum[vidSeq] == 1)
            {
                mMpgParserCxt->bRecordSeqInfFlag = 1;
                memcpy(mMpgParserCxt->nSequenceBuf, checkBufPtr, MPG_SEQUENCE_LEN);
                MpgParser->mVideoFormatT.nWidth = (checkBufPtr[4]<<4) | (checkBufPtr[5]>>4);
                MpgParser->mVideoFormatT.nHeight = ((checkBufPtr[5]&0x0f)<<8) | (checkBufPtr[6]);
                MpgParser->mVideoFormatT.nFrameRate = parser_mpeg_frame_rate[checkBufPtr[6]&0x0f];
                if(!MpgParser->mVideoFormatT.eCodecFormat)
                    MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
                //MpgParser->mVideoFormatT.eCompressionSubFormat = 2;
                if(MpgParser->mVideoFormatT.nFrameRate > 0)
                {
                   MpgParser->mVideoFormatT.nFrameDuration = 1000 * 1000/
                   MpgParser->mVideoFormatT.nFrameRate;
                }
            }
        }
        bufOffset = 4;
        if((isMpegAudPack==1)||(isVideoPack==1)||(startCode==MPG_PRIVATE_STREAM_1))
        {
            bufOffset = ((checkBufPtr[4]<<8)|checkBufPtr[5]) + 6;
            if(bufOffset == 0xffff)
            {
                bufOffset = 4;
            }
        }
        checkBufPtr += bufOffset;
    }
    return checkBufPtr;
}

cdx_uint32 MpgOpenShowDword(cdx_uint8 *buf)
{
    cdx_uint32 d0, d1;

    d0 = buf[0];
    d1 = buf[2];
    d0 <<= 8;
    d1 <<= 8;
    d0 |= buf[1];
    d1 |= buf[3];

    return (d0<<16)|d1;
}


cdx_uint8 *MpgOpenFindPackStartReverse(cdx_uint8 *buf_end, cdx_uint8 *buf_begin)
{
    cdx_uint8 *buf;
    cdx_uint32 data[4];

    buf = buf_end -1;

    while(buf >= (buf_begin + 3))
    {
        data[0] = *(buf-3);
        data[1] = *(buf-2);
        data[2] = *(buf-1);
        data[3] = *(buf-0);
        if(data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0xba)
        {
            return buf;
        }
        buf--;
    }
    return NULL;
}

cdx_uint8 *MpgOpenFindPackStart(cdx_uint8 *buf, cdx_uint8 *buf_end)
{
    while(buf <= buf_end-4)
    {
        if(buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01 && buf[3] == 0xba)
        {
            return buf;
        }
        buf++;
    }
    return NULL;
}

cdx_uint8 *MpgOpenFindNextStartCode(CdxMpgParserT *MpgParser, cdx_uint8 *buf, cdx_uint8 *buf_end)
{
    cdx_bool  flag1=CDX_FALSE, flag2 = CDX_FALSE;
    cdx_uint32 code;

    MpgParserContextT *mMpgParserCxt;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    while(buf <= buf_end-4)
    {
        flag1 = (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01);
        code = (0x01<<8) | buf[3];

        flag2 = ((code == mMpgParserCxt->nVideoId) || (code == mMpgParserCxt->nAudioId) ||
                ((code & 0xe0) == MPG_AUDIO_ID) || (buf[3] == 0xba));

        if(flag1 && flag2)
        {
            return buf;
        }
        buf++;
    }
    return NULL;
}

cdx_uint8 *MpgOpenJudgePacketType(CdxMpgParserT *MpgParser, cdx_uint8 *cur_ptr, cdx_int16 *nRet)
{
    MpgParserContextT *mMpgParserCxt;
    cdx_uint32 nSize, nNext32bits;
    cdx_uint8 i = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    *nRet = 0;

    if(mMpgParserCxt->bIsISO11172Flag)
    {
        cur_ptr += 12;
    }
    else
    {
        cur_ptr += 13;
        nSize = cur_ptr[0]&7;
        cur_ptr += nSize+1;
    }
    if(MpgOpenShowDword(cur_ptr) == MPG_SYSTEM_HEADER_START_CODE)
    {
        nSize = (cur_ptr[4]<<8) | cur_ptr[5];
        cur_ptr += nSize + 6;
    }
    nNext32bits = MpgOpenShowDword(cur_ptr);
    if(cur_ptr >= mMpgParserCxt->mDataChunkT.pEndPtr)
        *nRet = -1;
    if(nNext32bits == mMpgParserCxt->nVideoId)
        *nRet = 1;
    //else if(nNext32bits == mMpgParserCxt->nAudioId)
    else
    {
        for(i=0; i<MpgParser->nhasAudioNum; i++)
        {
            if(nNext32bits == mMpgParserCxt->nAudioIdArray[i])
            {
                *nRet = 2;
                break;
            }
        }
        if(i == MpgParser->nhasAudioNum)
        {
            *nRet = -1;
        }
    }
    return cur_ptr;
}

cdx_uint8 *MpgOpenSearchStartCode(cdx_uint8 *startPtr,cdx_uint32 dataLen, cdx_uint32 *startCode)
{
    cdx_uint32 i = 0;
    cdx_uint32 nextCode = 0xffffffff;

    *startCode = 0;

    for(i=0; i<dataLen; i++)
    {
        nextCode = (nextCode << 8) | startPtr[i];
        if((nextCode>=0x000001B3) && nextCode<0x000001FF)
        {
            *startCode = nextCode;
            return (startPtr+i-3);
        }
    }
    return NULL;
}


int searchGopHeaderTimeCode(unsigned char* ptr, unsigned int *dataSize, unsigned int* time)
{
    int i = 0;
    int timeCodePic = 0;
    int timeCodeSec = 0;
    int timecodeMin = 0;
    int timecodeHour = 0;
    int validDataSize = 0;
    unsigned int nextCode = 0xffffffff;

    validDataSize = *dataSize;

    for(i=0; i<validDataSize; i++)
    {
        nextCode <<=8;
        nextCode |= ptr[i];

        if(nextCode == 0x000001B8)
        {
            break;
        }
    }

    if(i==validDataSize)
    {
        *dataSize = 8;
        *time = 0xffffffff;
        return -1;
    }
    if((validDataSize-i) < 4)
    {
        *dataSize = 8;
        *time = 0xffffffff;
        return -1;
    }
    *dataSize = validDataSize-i;

    i++;
    nextCode = (ptr[i]<<24)|(ptr[i+1]<<16)|(ptr[i+2]<<8)|ptr[i+3];
    nextCode >>= 7;

    timeCodePic = nextCode&0x3f;
    nextCode >>= 6;
    timeCodeSec = nextCode&0x3f;
    nextCode >>= 7;
    timecodeMin = nextCode&0x3f;
    nextCode >>= 6;
    timecodeHour = nextCode&0x1f;
    nextCode >>= 5;
    *time = ((timecodeHour*3600)+(timecodeMin*60)+timeCodeSec)*1000;   // ms
    *time += timeCodePic*33;
    return 0;
}


cdx_uint32 rawVideoStreamCalDuration(CdxMpgParserT *MpgParser, MpgParserContextT *mMpgParserCxt)
{
    #define MAX_BUFFER_LEN 1024

    int len = 0;
    int pos = 0;
    int ret = 0;
    unsigned char* dataPtr = NULL;
    unsigned int size = 0;
    unsigned int validDataSize = 0;
    unsigned int time = 0;
    unsigned int startTime = 0;
    unsigned int endTime = 0;
    unsigned char buffer[MAX_BUFFER_LEN];


    CdxStreamSeek(mMpgParserCxt->pStreamT,0,STREAM_SEEK_SET);

    dataPtr = buffer;
    size = MAX_BUFFER_LEN;
    validDataSize = 0;

    while(1)
    {
        if(MpgParser->bForceReturnFlag == 1)
        {
            return 0;
        }
        len = CdxStreamRead(mMpgParserCxt->pStreamT, dataPtr, size);
        if(len <= 0)
        {
            return 0;
        }
        validDataSize += len;
        ret = searchGopHeaderTimeCode(dataPtr, &validDataSize, &startTime);
        if(ret < 0)
        {
            dataPtr = buffer;
            memcpy(dataPtr, dataPtr+MAX_BUFFER_LEN-validDataSize, validDataSize);
            size = MAX_BUFFER_LEN-validDataSize;
            dataPtr += validDataSize;
            continue;
        }
        break;
    }

    if(startTime == 0xffffffff)
    {
        return 0;
    }
    if(mMpgParserCxt->nFileLength > 1024*1024)
    {
        pos = (-1)*1*1024*1024;
    }
    else
    {
        pos = 0;
    }
    CdxStreamSeek(mMpgParserCxt->pStreamT,pos, STREAM_SEEK_END);

    dataPtr = buffer;
    size = MAX_BUFFER_LEN;
    validDataSize = 0;

    while(1)
    {
        if(MpgParser->bForceReturnFlag == 1)
        {
            return 0;
        }
        len = CdxStreamRead(mMpgParserCxt->pStreamT, dataPtr, size);
        if(len == 0)
        {
            break;
        }
        validDataSize += len;
        search_next_time_code:
        ret = searchGopHeaderTimeCode(dataPtr, &validDataSize, &time);
        if(ret==0)
        {
            endTime = time;
        }
        dataPtr = buffer;
        memcpy(dataPtr, dataPtr+MAX_BUFFER_LEN-validDataSize, validDataSize);
        size = MAX_BUFFER_LEN-validDataSize;
        dataPtr += validDataSize;
        continue;
    }

    time = 0;

    if((endTime!=0xffffffff) && (startTime!=0xffffffff))
    {
        if(startTime >= endTime)
        {
            time = 0;
        }
        else
        {
            time = endTime-startTime;
        }
    }
    return time;
}

cdx_int16 MpgOpenReaderOpenFile(CdxMpgParserT *MpgParser, CdxStreamT  *stream)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint8 *checkBufPtr     = NULL;
    //cdx_uint8  bitsPerSample   = 0;
    //cdx_uint8  samplePerSec    = 1;
    //cdx_uint8  nChannelNum     = 2;
    cdx_int16  nRet            = 0;
    cdx_int32 redLen          = 0;
    cdx_int64  hasPrseDataLen  = 0;
    cdx_int64  parseFileLen    = 0;
    cdx_int32 retDatalen      = 0;
    cdx_uint32 reachEndFlag    = 0;
    cdx_uint32 startCode       = 0;
    cdx_uint32 calPackNum[10]   = {0};
    cdx_uint32 packInf[4]      = {1,1,0,0};
    //0: isfstSystemFlag,1:isfstPackFlag,2:fstSysPackPos, 3, lastPackPos
    cdx_uint8* endCheckBuf = NULL;

    (void)stream;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    checkBufPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
    if(checkBufPtr == NULL)
    {
        return FILE_PARSER_PARA_ERR;
    }
    if(mMpgParserCxt->pStreamT == NULL)
    {
        return FILE_PARSER_OPEN_FILE_FAIL;
    }
    if(mMpgParserCxt->bIsNetworkStream == 1 && mMpgParserCxt->bSeekDisableFlag==1)
    {
        parseFileLen = MAX_CHUNK_BUF_SIZE;
    }
    else
    {
#if 0
        CdxStreamSeek(mMpgParserCxt->pStreamT, 0, STREAM_SEEK_END);
        parseFileLen = CdxStreamTell(mMpgParserCxt->pStreamT);
#else
        parseFileLen = CdxStreamSize(mMpgParserCxt->pStreamT);
#endif
        mMpgParserCxt->nFileLength = parseFileLen;
        parseFileLen >>= 1;
        if(parseFileLen <= 256)
        {
            return FILE_PARSER_NO_AV;
        }
        else if(parseFileLen >= 8*1024*1024)
        {
            parseFileLen = 8*1024*1024;
        }
        //CdxStreamSeek(mMpgParserCxt->pStreamT, 0, STREAM_SEEK_SET);
    }
__read_new_data:
    redLen     = CdxStreamRead(mMpgParserCxt->pStreamT,checkBufPtr, MAX_CHUNK_BUF_SIZE-retDatalen);
    retDatalen += redLen;

    if(retDatalen < MAX_CHUNK_BUF_SIZE)
    {
        reachEndFlag = 1;
    }
    checkBufPtr  = mMpgParserCxt->mDataChunkT.pStartAddr;

    if(MpgParser->bForceReturnFlag == 1)
    {
        return FILE_PARSER_OPEN_FILE_FAIL;
    }
    endCheckBuf = mMpgParserCxt->mDataChunkT.pStartAddr+retDatalen-256;
    while((checkBufPtr!=NULL) &&(checkBufPtr <endCheckBuf))
    {
        checkBufPtr = MpgOpenSearchStartCode(checkBufPtr, endCheckBuf - checkBufPtr, &startCode);
        if(checkBufPtr == NULL)
        {
            goto __cal_parse_result;
        }
        else
        {
            checkBufPtr = ProcessStartCode(MpgParser,checkBufPtr,startCode,
                        retDatalen,packInf,calPackNum);
        }
    }
 __cal_parse_result:
    // calculate parse result
    hasPrseDataLen += retDatalen - 256;
    if(hasPrseDataLen >= parseFileLen)
    {
        reachEndFlag = 1;
    }

    if(reachEndFlag == 1)
    {
        mMpgParserCxt->bIsPsFlag = 1;
        if((calPackNum[mpgPack]==0) && (calPackNum[vidSeq]!=0))
        {
            mMpgParserCxt->bIsPsFlag = 0;
        }
        else if((calPackNum[vidPack]==0) && (calPackNum[audPack]==0))
        {
            return FILE_PARSER_NO_AV;
        }
    }
    else if(MpgParser->bIsVOB ==0)
    {
        if((calPackNum[mpgPack]==0) && (calPackNum[vidSeq]!=0))
        {
            mMpgParserCxt->bIsPsFlag = 0;
        }
        else if((calPackNum[vidPack]!=0) && (calPackNum[audPack]!=0))
        {
            mMpgParserCxt->bIsPsFlag = 1;
        }
        else if((calPackNum[vc1VidPack]!=0) && (calPackNum[audPack]!=0))
        {
            return FILE_PARSER_FILE_FMT_ERR;
        }
        else
        {
            memcpy(mMpgParserCxt->mDataChunkT.pStartAddr,
                mMpgParserCxt->mDataChunkT.pStartAddr+retDatalen-256, 256);
            checkBufPtr = mMpgParserCxt->mDataChunkT.pStartAddr + 256;
            retDatalen = 256;
            goto __read_new_data;
        }
    }
    else
    {
        if((calPackNum[vidPack]!=0) && (calPackNum[audPack]!=0))
        {
            if((calPackNum[subPack]==0) &&(hasPrseDataLen<8*1024*1024))
            {
                 memcpy(mMpgParserCxt->mDataChunkT.pStartAddr,
                     mMpgParserCxt->mDataChunkT.pStartAddr+retDatalen-256, 256);
                 checkBufPtr = mMpgParserCxt->mDataChunkT.pStartAddr + 256;
                 retDatalen = 256;
                 goto __read_new_data;
            }
            mMpgParserCxt->bIsPsFlag = 1;
        }
        else if((calPackNum[vc1VidPack]!=0) && (calPackNum[audPack]!=0))
        {
            return FILE_PARSER_FILE_FMT_ERR;
        }
        else
        {
            memcpy(mMpgParserCxt->mDataChunkT.pStartAddr,
                mMpgParserCxt->mDataChunkT.pStartAddr+retDatalen-256, 256);
            checkBufPtr = mMpgParserCxt->mDataChunkT.pStartAddr + 256;
            retDatalen = 256;
            goto __read_new_data;
        }
    }

    if(MpgParser->bForceReturnFlag == 1)
    {
        return FILE_PARSER_OPEN_FILE_FAIL;
    }


    if(mMpgParserCxt->bIsPsFlag == 0)
    {
        //calculate the file duration
        mMpgParserCxt->nFileTimeLength =  rawVideoStreamCalDuration(MpgParser, mMpgParserCxt);

    }

    mMpgParserCxt->bHasInitAVSFlag = CDX_FALSE;
    mMpgParserCxt->bHasNvpackFlag  = CDX_FALSE;
    mMpgParserCxt->mDataChunkT.nId = 0;

    if(mMpgParserCxt->bIsNetworkStream == 1 && mMpgParserCxt->bSeekDisableFlag==1)
    {
        mMpgParserCxt->mDataChunkT.nUpdateSize = 0;
        mMpgParserCxt->mDataChunkT.nSegmentNum = 0;
        mMpgParserCxt->mDataChunkT.pReadPtr= mMpgParserCxt->mDataChunkT.pStartAddr;
        mMpgParserCxt->mDataChunkT.pEndPtr= mMpgParserCxt->mDataChunkT.pStartAddr
                                            +MAX_CHUNK_BUF_SIZE;
        mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_FALSE;
        mMpgParserCxt->bHasInitAVSFlag                = CDX_TRUE;
        MpgParser->nTotalTimeLength         = 0;

        if(packInf[2] < 4)
        {
            packInf[2] = 4;
        }
        CdxStreamSeek(mMpgParserCxt->pStreamT,packInf[2]-4,STREAM_SEEK_SET);
    }
    else
    {
        if(mMpgParserCxt->bIsPsFlag == 1)
        {
            if((mMpgParserCxt->bIsNetworkStream==0) && (MpgParser->bIsVOB==1) && (packInf[2]!=0))
            {
                nRet = VobCheckPsPts(MpgParser,packInf[2]);
                if(nRet == -1)
                {
                    return FILE_PARSER_NO_AV;
                }
                else if(nRet == 0)
                {
                    MpgTimeCheckPsPts(MpgParser);
                }
            }
            else if(mMpgParserCxt->bIsNetworkStream == 1)
            {
                MpgTimeCheckPsNetWorkStreamPts(MpgParser, packInf[2]-4);
            }
            else
            {
                MpgTimeCheckPsPts(MpgParser);
            }
        }

        if(packInf[2] < 4)
        {
            packInf[2] = 4;
        }
        CdxStreamSeek(mMpgParserCxt->pStreamT,packInf[2]-4,STREAM_SEEK_SET);
        if(MpgParser->bForceReturnFlag == 1)
        {
            return FILE_PARSER_OPEN_FILE_FAIL;
        }

        mMpgParserCxt->mDataChunkT.nUpdateSize    = 0;
        mMpgParserCxt->mDataChunkT.nSegmentNum        = 0;
        mMpgParserCxt->mDataChunkT.pReadPtr          = mMpgParserCxt->mDataChunkT.pStartAddr;
        mMpgParserCxt->mDataChunkT.pEndPtr            = mMpgParserCxt->mDataChunkT.pStartAddr;
        mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_FALSE;
        MpgParser->nTotalTimeLength         = mMpgParserCxt->nFileTimeLength;
    }

    if(MpgParser->mVideoFormatT.nFrameRate == 0)
    {
        MpgParser->mVideoFormatT.nFrameRate = 25000;
    }
    MpgParser->mVideoFormatT.nFrameDuration = 1000 * 1000 / MpgParser->mVideoFormatT.nFrameRate;

    if((MpgParser->bIsVOB == 1) && (calPackNum[subPack]!=0))
    {
        MpgParser->mSubFormatT.eCodecFormat= SUBTITLE_CODEC_DVDSUB;
        mMpgParserCxt->nSubpicStreamId = 0x20;
    }
    return FILE_PARSER_RETURN_OK;
}


