/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/

#include <stdint.h>
#include "CDX_Fileformat.h"
#include "CDX_Common.h"
#include "cedar_abs_packet_hdr.h"

#include <stdlib.h>
#include <string.h>

#define DELFILETYPE
#ifdef MEMCHECK
#include "memname.h"
#include "memcheck.h"
#endif

#include "alib_log.h"

#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG "Allwinner Audio Middle Layer"
#endif

#ifdef  ALIB_DEBUG
#undef  ALIB_DEBUG
#define ALIB_DEBUG 1
#endif

//define wave header, for decode pcm data
typedef struct __WAVE_HEADER
{
    unsigned int    uRiffFcc;       // four character code, "RIFF"
    unsigned int    uFileLen;       // file total length, don't care it

    unsigned int    uWaveFcc;       // four character code, "WAVE"

    unsigned int    uFmtFcc;        // four character code, "fmt "
    unsigned int    uFmtDataLen;    // Length of the fmt data (=16)
    unsigned short  uWavEncodeTag;  // WAVE File Encoding Tag
    unsigned short  uChannels;      // Channels: 1 = mono, 2 = stereo
    unsigned int    uSampleRate;    // Samples per second: e.g., 44100
    unsigned int    uBytesPerSec;   // sample rate * block align
    unsigned short  uBlockAlign;    // channels * bits/sample / 8
    unsigned short  uBitsPerSample; // 8 or 16

    unsigned int    uDataFcc;       // four character code "data"
    unsigned int    uSampDataSize;  // Sample data size(n)

} __wave_header_t;


//define adpcm header
typedef struct __WAVE_HEADER_ADPCM
{
    unsigned int    uRiffFcc;       // four character code, "RIFF"
    unsigned int    uFileLen;       // file total length,  =totalfilelength-8

    unsigned int    uWaveFcc;       // four character code, "WAVE"

    unsigned int    uFmtFcc;        // four character code, "fmt "
    unsigned int    uFmtDataLen;    // Length of the fmt data min(adpcm=0x14,pcm =0x10)
    unsigned short  uWavEncodeTag;  // WAVE File Encoding Tag adpcm =0x11,pcm =0x01
    unsigned short  uChannels;      // Channels: 1 = mono, 2 = stereo
    unsigned int    uSampleRate;    // Samples per second: e.g., 44100
    unsigned int    uBytesPerSec;   // pcm = sample rate * block align,adpcm= sample rate* uBlockAlign/nSamplesperBlock;
    unsigned short  uBlockAlign;    // pcm=channels * bits/sample / 8,adpcm =block lenth;
    unsigned short  uBitsPerSample; // pcm=8 or 16 or 24,adpcm =4,now

    unsigned short  uWordsbSize;    // for only adpcm =0x0002;
    unsigned short  uNSamplesPerBlock;//for only adpcm = ((uBlockAlign-4*uChannels)*2+channels)/channels

    unsigned int    uDataFcc;       // four character code "data"
    unsigned int    uSampDataSize;  // Sample data size(n)

} __wave_header_adpcm_t;


typedef struct __AAC_HEADER_INFO
{
    unsigned int    ulBsHdrFlag;        // bitstream header flag, = 0xffffffff, for mask bitstream header
    unsigned int    ulSampleRate;       // sample rate
    unsigned int    ulActualRate;       // sample rate
    unsigned short  usBitsPerSample;    // bits per sample
    unsigned short  usNumChannels;      // channel count
    unsigned short  usAudioQuality;     // audio quality, don't care(default 100)
    unsigned short  usFlavorIndex;      // flavor index, don't care(default 0)
    unsigned int    ulBitsPerFrame;     // bits per frame,don't care(default 0x100)
    unsigned int    ulGranularity;      // default=0x100;ulFramesPerBlock*ulFrameSize
    unsigned int    ulOpaqueDataSize;   // size of opaque data
                                        // just put opaque data after 'ulOpaqueDataSize'
} __aac_header_info_t;


//define aac audio sub type
typedef enum __AAC_BITSTREAM_SUB
{
    AAC_SUB_TYPE_MAIN       = 1,
    AAC_SUB_TYPE_LC         = 2,
    AAC_SUB_TYPE_SSR        = 3,
    AAC_SUB_TYPE_LTP        = 4,
    AAC_SUB_TYPE_SBR        = 5,
    AAC_SUB_TYPE_SCALABLE   = 6,
    AAC_SUB_TYPE_TWINVQ     = 7,
    AAC_SUB_TYPE_CELP       = 8,
    AAC_SUB_TYPE_HVXC       = 9,
    AAC_SUB_TYPE_TTSI       = 12,
    AAC_SUB_TYPE_GENMIDI    = 15,
    AAC_SUB_TYPE_AUDIOFX    = 16,

    AAC_SUB_TYPE_

} __aac_bitstream_sub_t;


typedef enum __AAC_PROFILE_TYPE
{
    AAC_PROFILE_MP  = 0,
    AAC_PROFILE_LC  = 1,
    AAC_PROFILE_SSR = 2,

    AAC_PROFILE_

} __aac_profile_type_t;




// __AAC_OpaqueData structure define
// Note : we can't use this struct, because the big ending,
// this structure just mark that the items need by decode lib
// | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 |7-0 | 7-0 | 7 6 5 4 3     2     1 0  |
// |---uAACFrmType---|--AACOBJ--|--SFIdx--|---------sample rate--------|--chn--|-frmlen-|-flag-|

typedef struct __AAC_OpaqueData
{
    uint64_t    uAACFrmType : 8;        // 1:ADTS Frame; 2:MP4 Audio Specific Config Data
    uint64_t    uAACObjType : 5;        // __aac_bitstream_sub_t
    uint64_t    uSFIdxType  : 4;        // sample frequence type, 0:96000, 0xf:follow 24 bits
    uint64_t    uSampleRate : 24;       // sample frequence, just exist when uSFIdxType = 0xf
    uint64_t    uChannels   : 4;        // audio channel count
    uint64_t    uFrmLength  : 1;        // 0:960, 1:1024; default set to 1
    uint64_t    Flag0       : 1;        // flag reserved 0,
    uint64_t    Flag1       : 1;        // flag reserved 1,
    uint64_t    Reserved    : 16;       // reserved, no use

} __aac_opaquedata_t;



typedef struct __WMA_HEADER_INFO
{
    short           FmtTag;                 //wma format tag, should be 0x160/0x162/0x163
    short           Channel;                //audio channel count
    int             SampleRate;             //audio sample rate
    int             AvgBytePerSec;          //average bytes per seconds
    short           BlockAlign;             //block align
    short           reserved0;              //reserved
    unsigned char   reserved1[8];           //reserved
    short           EncodeOpt;              //encode options
    short           reserved2;              //reserved
    short           reserved3;              //reserved

} __wma_header_info_t;

//AAC sample rate table
#define AAC_SAMPLE_RATE_IDX_SIZE    (13)
int AacSampRateTbl[AAC_SAMPLE_RATE_IDX_SIZE] =
{
    96000, 88200, 64000, 48000,
    44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,
    7350
};

int AdCedarBuildAACPacketHdr(unsigned char* extraData, 
                             int            extraDataLen, 
                             int            uPacketLen, 
                             unsigned char* pBuf, 
                             int*           pHdrLen,
                             int            channels,
                             int            sampleRate)
{
    int tmpChnCfg, tmpSampRateIdx;
    int tmpProfile = AAC_PROFILE_LC;

    //clear aac packet header length
    *pHdrLen = 0;

    if(extraData && !(extraDataLen < 2))
    {
        //parse audio obiect type
        switch(*extraData>>3)
        {
        case AAC_SUB_TYPE_LC:
            tmpProfile = AAC_PROFILE_LC;
            break;
        case AAC_SUB_TYPE_MAIN:
            tmpProfile = AAC_PROFILE_MP;
            break;
        case AAC_SUB_TYPE_SSR:
            tmpProfile = AAC_PROFILE_SSR;
            break;
        case AAC_SUB_TYPE_AUDIOFX:
            tmpProfile = AAC_PROFILE_LC;
            break;
        default:
            {
                alib_logw("AAC sub type can't support, just think it as LC!");
                tmpProfile = AAC_PROFILE_LC;
                break;
            }
        }
        
        tmpSampRateIdx = (*extraData&7)<<1 | *(extraData+1)>>7;
        if(tmpSampRateIdx == 0xf)
        {
            int samplingFrequency = ((*(extraData+1) & 0x7f) << 17)
                | (*(extraData+2) << 9)
                | (*(extraData+3) << 1)
                | (*(extraData+4) >> 7);
            
            for(tmpSampRateIdx=0; tmpSampRateIdx<AAC_SAMPLE_RATE_IDX_SIZE; tmpSampRateIdx++)
            {
                if(samplingFrequency == AacSampRateTbl[tmpSampRateIdx])
                {
                    break;
                }
            }
            tmpChnCfg  = (*(extraData+4)>>3) & 0x7;
        }
        else
        {
            tmpChnCfg  = (*(extraData+1)>>3) & 0x7;
        }
    }
    else
    {
        alib_logw("!!!please implement it!");
        tmpProfile = 1;
        tmpChnCfg = channels & 7;
        for(tmpSampRateIdx=0; tmpSampRateIdx<AAC_SAMPLE_RATE_IDX_SIZE; tmpSampRateIdx++)
        {
            if(sampleRate == AacSampRateTbl[tmpSampRateIdx])
            {
                break;
            }
        }
    }
    
    memset(pBuf, 0, 7);
    *pHdrLen = 7;
    
    //set sync word, 12bits
    *(pBuf + 0) |= 0xff;
    *(pBuf + 1) |= 0xf<<4;
    //set ID, 1bit
    *(pBuf + 1) |= 0x0<<3;
    //set layer, 2bits
    *(pBuf + 1) |= 0x0<<1;
    //set protect bit, 1bit
    *(pBuf + 1) |= 0x1<<0;
    //set profile, 2bits
    *(pBuf + 2) |= tmpProfile<<6;
    //set sample rate index, 4bits
    *(pBuf + 2) |= tmpSampRateIdx<<2;
    //set private bit
    *(pBuf + 2) |= 0<<1;
    //set channel config, 3bits
    *(pBuf + 2) |= (tmpChnCfg>>2)&0x01;
    *(pBuf + 3) |= (tmpChnCfg<<6)&0xff;
    //set orignal copy, 1bit
    *(pBuf + 3) |= 0<<5;
    //set home, 1bit
    *(pBuf + 3) |= 0<<4;
    //set copy bit, 1bit
    *(pBuf + 3) |= 0<<3;
    //set copy start, 1bit
    *(pBuf + 3) |= 0<<2;
    //set frame length, 13bits
    *(pBuf + 3) |= (uPacketLen>>11) & 0x03;
    *(pBuf + 4) |= (uPacketLen>>3) & 0xff;
    *(pBuf + 5) |= ((uPacketLen>>0) & 0x07)<<5;
    //set buffer full, 11bits
    *(pBuf + 5) |= 0x1f<<0;
    *(pBuf + 6) |= (0xff<<2)&0xff;
    //set raw block, 2bits
    *(pBuf + 6) |= 0<<0;
    return 0;
}


/*
*********************************************************************************************************
*                           UPDATE AAC BITSTREAM PACKET HEADER
*
* Description: update aac bitstream packet header;
*
* Arguments  : pBuf         buffer for store aac bitsteam packet header;
*              pHdrLen      pointer to the length of bitstream packet header;
*              uPacketLen   length of the audio bitstream packet;
*
* Returns    : result;
*                   CDX_OK,    update aac bitstream packet header successed;
*                   CDX_FAIL,  update aac bitstream packet header failed;
*********************************************************************************************************
*/
int AdCedarUpdateAACPacketHdr(unsigned char *pBuf, int uHdrLen, int uPacketLen)
{
    int       result = 0;//CDX_OK;

    if(uHdrLen == 7)
    {
        //clear frame length, 13bits
        *(pBuf + 3) &= ~(0x03<<0);
        *(pBuf + 4) = 0; //&= ~(0xff<<0);
        *(pBuf + 5) &= ~(0x07<<5);
        //set frame length, 13bits
        *(pBuf + 3) |= (((uPacketLen+7)>>11) & 0x03)<<0;
        *(pBuf + 4) |= (((uPacketLen+7)>>3)  & 0xff)<<0;
        *(pBuf + 5) |= (((uPacketLen+7)>>0)  & 0x07)<<5;
    }

    return result;
}

#ifndef NULL
#define NULL 0
#endif

#ifndef AV_RN16
#define AV_RN16(a) (*((unsigned short *) (a)))
#endif

int split_xiph_headers(unsigned char* codec_priv, 
                       int            codec_priv_size,
                       char**         extradata, 
                       int*           extradata_size)
{
    unsigned char* header_start[3];
    int            header_len[3];
    int            i;
    int            overall_len;
    char*          OggHeader;
    unsigned char  lacing_fill;
    unsigned char  lacing_fill_0;
    unsigned char  lacing_fill_1;
    unsigned char  lacing_fill_2;

    if (codec_priv_size >= 6 && AV_RN16(codec_priv) == 30) {

        overall_len = 6;
        for (i=0; i<3; i++) {
            header_len[i] = AV_RN16(codec_priv);
            codec_priv += 2;
            header_start[i] = codec_priv;
            codec_priv += header_len[i];
            if (overall_len > codec_priv_size - header_len[i])
                return -1;
            overall_len += header_len[i];
        }
    } else if (codec_priv_size >= 3 && codec_priv[0] == 2) {
        overall_len = 3;
        codec_priv++;
        for (i=0; i<2; i++, codec_priv++) {
            header_len[i] = 0;
            for (; overall_len < codec_priv_size && *codec_priv==0xff; codec_priv++) {
                header_len[i] += 0xff;
                overall_len   += 0xff + 1;
            }
            header_len[i] += *codec_priv;
            overall_len   += *codec_priv;
            if (overall_len > codec_priv_size)
                return -1;
        }
        header_len[2] = codec_priv_size - overall_len;
        header_start[0] = codec_priv;
        header_start[1] = header_start[0] + header_len[0];
        header_start[2] = header_start[1] + header_len[1];
    } else {
        return -1;
    }

    for(lacing_fill_0=0; lacing_fill_0*0xff<header_len[0]; lacing_fill_0++);
    for(lacing_fill_1=0; lacing_fill_1*0xff<header_len[1]; lacing_fill_1++);
    for(lacing_fill_2=0; lacing_fill_2*0xff<header_len[2]; lacing_fill_2++);

    *extradata_size =  27 + lacing_fill_0 + header_len[0]
                       +  27 + lacing_fill_1 + header_len[1]
                          + lacing_fill_2 + header_len[2];
    OggHeader = *extradata = malloc(*extradata_size);
    if(OggHeader == NULL)
        return -1;
    memset(*extradata,0,*extradata_size);
    for(i=0; i<26; i++)
    {
       OggHeader[i] = 0;
    }
    OggHeader[0] = 0x4f; OggHeader[1] = 0x67;  OggHeader[2] = 0x67;  OggHeader[3] = 0x53;
    OggHeader[5] = 0x2;
    lacing_fill = lacing_fill_0;
    OggHeader[26] = lacing_fill;
    OggHeader += 27;
    for(i=0; i<lacing_fill; i++)
    {
        if(i!=(lacing_fill-1))
        {
            OggHeader[i] = 0xff;
        }
        else
        {
            OggHeader[i] = header_len[0] - (lacing_fill-1)*0xff;
        }
    }
    OggHeader += lacing_fill;

    memcpy(OggHeader, header_start[0], header_len[0]);
    OggHeader += header_len[0];

    for(i=0; i<26; i++)
    {
       OggHeader[i] = 0;
    }
    OggHeader[0] = 0x4f; OggHeader[1] = 0x67;  OggHeader[2] = 0x67;  OggHeader[3] = 0x53;
    OggHeader[18] = 0x1;
    OggHeader[26] = lacing_fill_1 + lacing_fill_2;
    OggHeader += 27;
    lacing_fill = lacing_fill_1;
    for(i=0; i<lacing_fill; i++)
    {
        if(i!=(lacing_fill-1))
        {
            OggHeader[i] = 0xff;
        }
        else
        {
            OggHeader[i] = header_len[1] - (lacing_fill-1)*0xff;
        }
    }
    OggHeader += lacing_fill;

    lacing_fill = lacing_fill_2;
    for(i=0; i<lacing_fill; i++)
    {
        if(i!=(lacing_fill-1))
        {
            OggHeader[i] = 0xff;
        }
        else
        {
            OggHeader[i] = header_len[2] - lacing_fill*0xff - 1;
        }
    }
    OggHeader += lacing_fill;

    memcpy(OggHeader, header_start[1], header_len[1]);
    OggHeader += header_len[1];
    memcpy(OggHeader, header_start[2], header_len[2]);
    OggHeader += header_len[2];

    return 0;
}
/*******************PCM HEAD******************************************************/
#define SAMPLE_RATE_CNT     13
#define SAMPLE_THRESHOLD    500
static unsigned int ref_sample_rate[SAMPLE_RATE_CNT] =
{
    44100, 22050, 11025,
    48000, 24000, 12000,
    32000, 16000, 8000,
    192000,176400,96000,
    88200
    
};

static unsigned int convert_sample_rate(unsigned int sample_rate)
{
    int i;
    for(i=0;i<SAMPLE_RATE_CNT;i++)
    {
        if(sample_rate>=ref_sample_rate[i]-SAMPLE_THRESHOLD
            && sample_rate<=ref_sample_rate[i]+SAMPLE_THRESHOLD)
        {
            return ref_sample_rate[i];
        }
    }
    if(sample_rate > 0)
        return sample_rate;
    return ref_sample_rate[0];
}


/*
*********************************************************************************************************
*                           SET AUDIO BITSTREAM HEADER
*
*Description: set audio bitstream header, because some audio bitstream is parsing from video file,
*             the audio bitstream information is saved insulated from the audio bitstream, for expand
*             audio decoder's capability, we set the bitstream information in to audio bitstream
*             buffer, so, audio decoder can process the audio bitstream from either audio file or
*             video file.
*
*Arguments  : pDev  audio decode device handle;
*
*Return     : result
*               =   0,  set audio bitstream header successed;
*               <   0,  set audio bitstream header failed;
*********************************************************************************************************
*/
int SetAudioBsHeader_PCM(AudioStreamInfo  *tmpAbsFmt, Ac320FileRead   *FileReadInfo)
{
    //audio bitstream is parsed from video file, need set audio bitstream header
    if(((tmpAbsFmt->eSubCodecFormat & 0xffff) != WAVE_FORMAT_ADPCM)&&((tmpAbsFmt->eSubCodecFormat & 0xffff) != WAVE_FORMAT_DVI_ADPCM)&&((tmpAbsFmt->eSubCodecFormat & 0xf000) != ADPCM_CODEC_ID_IMA_QT))
    {
        __wave_header_t         *tmpWavHdr;

        tmpWavHdr = (__wave_header_t *)(FileReadInfo->bufWritingPtr);

        tmpWavHdr->uRiffFcc = ('R'<<0) | ('I'<<8) | ('F'<<16) | ('F'<<24);
        tmpWavHdr->uFileLen = 0x7fffffff;

        tmpWavHdr->uWaveFcc = ('W'<<0) | ('A'<<8) | ('V'<<16) | ('E'<<24);
        tmpWavHdr->uFmtFcc  = ('f'<<0) | ('m'<<8) | ('t'<<16) | (' '<<24);
        tmpWavHdr->uFmtDataLen = 16;
        if(AUDIO_CODEC_FORMAT_G711a == tmpAbsFmt->eCodecFormat)
        {
            tmpWavHdr->uWavEncodeTag = WAVE_FORMAT_ALAW;
        }
        else if(AUDIO_CODEC_FORMAT_G711u == tmpAbsFmt->eCodecFormat)
        {
            tmpWavHdr->uWavEncodeTag = WAVE_FORMAT_MULAW;
        }
        else
        {
            tmpWavHdr->uWavEncodeTag = tmpAbsFmt->eSubCodecFormat & 0xffff;
        }
        tmpWavHdr->uChannels = tmpAbsFmt->nChannelNum;
        tmpWavHdr->uSampleRate = convert_sample_rate(tmpAbsFmt->nSampleRate);

        tmpWavHdr->uBitsPerSample = tmpAbsFmt->nBitsPerSample;
        if((tmpWavHdr->uWavEncodeTag == WAVE_FORMAT_ALAW)||(tmpWavHdr->uWavEncodeTag == WAVE_FORMAT_MULAW))
        {
            tmpWavHdr->uBitsPerSample = 0x8;
        }
        tmpWavHdr->uBlockAlign = tmpAbsFmt->nChannelNum * tmpWavHdr->uBitsPerSample / 8;
        tmpWavHdr->uBytesPerSec = tmpWavHdr->uSampleRate * tmpWavHdr->uBlockAlign;
        tmpWavHdr->uDataFcc = ('d'<<0) | ('a'<<8) | ('t'<<16) | ('a'<<24);
        tmpWavHdr->uSampDataSize = 0x7fffffff;
        
        //add pts of out
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].ptsValid = 0;
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].pts = 0;
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].startAddr = FileReadInfo->bufWritingPtr;
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].len = sizeof(__wave_header_t);
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].readlen = 0;
        FileReadInfo->frmFifo->ValidchunkCnt++;
        FileReadInfo->frmFifo->wtIdx ++;
        if(FileReadInfo->frmFifo->wtIdx >(FileReadInfo->frmFifo->maxchunkNum - 1))
        {
            FileReadInfo->frmFifo->wtIdx = 0;
        }
        
        //flush buffer
        FileReadInfo->bufWritingPtr += sizeof(__wave_header_t);
        FileReadInfo->BufValideLen -= sizeof(__wave_header_t);
        FileReadInfo->BufLen += sizeof(__wave_header_t);
        FileReadInfo->FileWritingOpsition += sizeof(__wave_header_t);
        
        //the pcm in mpeg2 sirial file is big endian, need change data
        if((tmpAbsFmt->eSubCodecFormat & ABS_EDIAN_FLAG_MASK) == ABS_EDIAN_FLAG_BIG)
        {
            unsigned int       tmpVal;

            //set the 'RIFX' flag for bit endian
            tmpWavHdr->uRiffFcc = ('R'<<0) | ('I'<<8) | ('F'<<16) | ('X'<<24);

            //exchange every member to big endian
            tmpVal = tmpWavHdr->uFileLen;
            tmpWavHdr->uFileLen = (((tmpVal>>0) & 0xff) << 24)
                                | (((tmpVal>>8) & 0xff) << 16)
                                | (((tmpVal>>16) & 0xff) << 8)
                                | (((tmpVal>>24) & 0xff) << 0);

            tmpVal = tmpWavHdr->uFmtDataLen;
            tmpWavHdr->uFmtDataLen = (((tmpVal>>0) & 0xff) << 24)
                                   | (((tmpVal>>8) & 0xff) << 16)
                                   | (((tmpVal>>16) & 0xff) << 8)
                                   | (((tmpVal>>24) & 0xff) << 0);

            tmpVal = tmpWavHdr->uWavEncodeTag;
            tmpWavHdr->uWavEncodeTag = (((tmpVal>>0) & 0xff) << 8)
                                     | (((tmpVal>>8) & 0xff) << 0);

            tmpVal = tmpWavHdr->uChannels;
            tmpWavHdr->uChannels = (((tmpVal>>0) & 0xff) << 8)
                                 | (((tmpVal>>8) & 0xff) << 0);

            tmpVal = tmpWavHdr->uSampleRate;
            tmpWavHdr->uSampleRate = (((tmpVal>>0) & 0xff) << 24)
                                   | (((tmpVal>>8) & 0xff) << 16)
                                   | (((tmpVal>>16) & 0xff) << 8)
                                   | (((tmpVal>>24) & 0xff) << 0);

            tmpVal = tmpWavHdr->uBytesPerSec;
            tmpWavHdr->uBytesPerSec = (((tmpVal>>0) & 0xff) << 24)
                                    | (((tmpVal>>8) & 0xff) << 16)
                                    | (((tmpVal>>16) & 0xff) << 8)
                                    | (((tmpVal>>24) & 0xff) << 0);

            tmpVal = tmpWavHdr->uBlockAlign;
            tmpWavHdr->uBlockAlign = (((tmpVal>>0) & 0xff) << 8)
                                   | (((tmpVal>>8) & 0xff) << 0);

            tmpVal = tmpWavHdr->uBitsPerSample;
            tmpWavHdr->uBitsPerSample = (((tmpVal>>0) & 0xff) << 8)
                                      | (((tmpVal>>8) & 0xff) << 0);

            tmpVal = tmpWavHdr->uSampDataSize;
            tmpWavHdr->uSampDataSize = (((tmpVal>>0) & 0xff) << 24)
                                     | (((tmpVal>>8) & 0xff) << 16)
                                     | (((tmpVal>>16) & 0xff) << 8)
                                     | (((tmpVal>>24) & 0xff) << 0);
        }
    }
    else
    {
        //build wave adpcm header
        __wave_header_adpcm_t   *tmpWavAdpcmHdr;

        tmpWavAdpcmHdr = (__wave_header_adpcm_t *)FileReadInfo->bufWritingPtr;

        tmpWavAdpcmHdr->uRiffFcc = ('R'<<0) | ('I'<<8) | ('F'<<16) | ('F'<<24);
        tmpWavAdpcmHdr->uFileLen = 0x7fffffff;

        tmpWavAdpcmHdr->uWaveFcc = ('W'<<0) | ('A'<<8) | ('V'<<16) | ('E'<<24);
        tmpWavAdpcmHdr->uFmtFcc  = ('f'<<0) | ('m'<<8) | ('t'<<16) | (' '<<24);
        tmpWavAdpcmHdr->uFmtDataLen = 20;

        //set adpcm encode tag
        tmpWavAdpcmHdr->uWavEncodeTag = tmpAbsFmt->eSubCodecFormat & 0xffff;

        tmpWavAdpcmHdr->uChannels = tmpAbsFmt->nChannelNum;
        tmpWavAdpcmHdr->uSampleRate = convert_sample_rate(tmpAbsFmt->nSampleRate);
        tmpWavAdpcmHdr->uBitsPerSample = 4;
        tmpWavAdpcmHdr->uWordsbSize = 0x02; 

        tmpWavAdpcmHdr->uBlockAlign = (unsigned int)tmpAbsFmt->nBlockAlign;
        tmpWavAdpcmHdr->uNSamplesPerBlock = ((tmpWavAdpcmHdr->uBlockAlign - \
                              4*tmpAbsFmt->nChannelNum)*2 + tmpAbsFmt->nChannelNum)/tmpAbsFmt->nChannelNum;
        if(tmpAbsFmt->nAvgBitrate)
        {
            tmpWavAdpcmHdr->uBytesPerSec = tmpAbsFmt->nAvgBitrate >> 3;
        }
        else
        {
            tmpWavAdpcmHdr->uBytesPerSec = tmpWavAdpcmHdr->uSampleRate *    \
                                  tmpWavAdpcmHdr->uBlockAlign / tmpWavAdpcmHdr->uNSamplesPerBlock;
        }
        
        tmpWavAdpcmHdr->uDataFcc = ('d'<<0) | ('a'<<8) | ('t'<<16) | ('a'<<24);
        tmpWavAdpcmHdr->uSampDataSize = 0x7fffffff;
        
        //add pts of out
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].ptsValid = 0;
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].pts = 0;
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].startAddr = FileReadInfo->bufWritingPtr;
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].len = sizeof(__wave_header_t);   
        FileReadInfo->frmFifo->inFrames[FileReadInfo->frmFifo->wtIdx].readlen = 0;
        FileReadInfo->frmFifo->ValidchunkCnt++;
        FileReadInfo->frmFifo->wtIdx ++;
        if(FileReadInfo->frmFifo->wtIdx >(FileReadInfo->frmFifo->maxchunkNum - 1))
        {
            FileReadInfo->frmFifo->wtIdx = 0;
        }
        //flush buffer
        FileReadInfo->bufWritingPtr += sizeof(__wave_header_adpcm_t);
        FileReadInfo->BufValideLen -= sizeof(__wave_header_adpcm_t);
        FileReadInfo->BufLen += sizeof(__wave_header_adpcm_t);
        FileReadInfo->FileWritingOpsition += sizeof(__wave_header_adpcm_t);
    }
#if 1
    alib_logv("****************PCM HEAD START***************");
    alib_logv("eSubCodecFormat =%d",tmpAbsFmt->eSubCodecFormat);   
    alib_logv("nBitsPerSample =%d",tmpAbsFmt->nBitsPerSample);
    alib_logv("nChannelNum =%d",tmpAbsFmt->nChannelNum);
    alib_logv("nSampleRate =%d",tmpAbsFmt->nSampleRate);    
    alib_logv("****************PCM HEAD END***************");
#endif
    return 0;
}

