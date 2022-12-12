/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxAacParser.h
* Description : Amr Parser
* History :
*
*/
#ifndef CDX_AAC_PARSER_H
#define CDX_AAC_PARSER_H

/* AAC file format */
enum {
    AAC_FF_Unknown = 0,        /* should be 0 on init */

    AAC_FF_ADTS = 1,
    AAC_FF_ADIF = 2,
    AAC_FF_RAW  = 3,
    AAC_FF_LATM = 4

};

#define SUCCESS 1
#define ERROR   0
#define ERRORFAMENUM 10
#define READLEN 4096
#define    AuInfTime  60
#define AAC_MAX_NCHANS 6
#define AAC_MAINBUF_SIZE    (768 * AAC_MAX_NCHANS * 2)

typedef struct AacParserImplS
{
    //audio common
    CdxParserT  base;
    CdxStreamT  *stream;

    pthread_cond_t cond;
    cdx_int64   ulDuration;//ms
    cdx_int64   dFileSize;//total file length
    cdx_int64   dFileOffset; //now read location
    cdx_int32   mErrno; //Parser Status
    cdx_uint32  nFlags; //cmd
    cdx_int32   bSeekable;
    //aac base
    cdx_int32   nFrames;//samples

    cdx_int32   ulChannels;
    cdx_int32   ulSampleRate;
    cdx_int32   ulBitRate;
    cdx_int32   ulformat;
    cdx_int32   uFirstFrmOffset;
    cdx_int32   bytesLeft;
    cdx_uint8   readBuf[2*1024*6*6];
    cdx_uint8   *readPtr;
    cdx_int32   eofReached;

}AacParserImplS;

static const int sampRateTab[12] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025,  8000
};

/* channel mapping (table 1.6.3.4) (-1 = unknown,
 * so need to determine mapping based on rules in 8.5.1) */
static const int channelMapTab[8] = {
    -1, 1, 2, 3, 4, 5, 6, 8
};
//static unsigned char readBuf[2*768*2*2];

#define AAC_SYNCWORDH     0xff
#define AAC_SYNCWORDL     0xf0

#define AAC_SYNCWORDL_LATM    0xE0
#define AAC_SYNCWORDL_H     0x56

#define AAC_ADIF_COPYID_SIZE    10
#define CHAN_ELEM_IS_CPE(x)   (((x) & 0x10) >> 4)  /* bit 4 = ucSce/CPE flag */
typedef struct ADTSHeaderS {
    /* fixed */
    unsigned char ucId;                             /* MPEG bit - should be 1 */
    unsigned char ucLayer;                          /* MPEG layer - should be 0 */
    unsigned char ucProtectBit;                     /* 0 = CRC word follows, 1 = no CRC word */
    unsigned char ucProfile;                        /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
    unsigned char ucSampRateIdx;                    /* sample rate index range = [0, 11] */
    unsigned char ucPivateBit;                     /* ignore */
    unsigned char ucChannelConfig;                  /* 0 = implicit, >0 = use default table */
    unsigned char ucOrigCopy;                       /* 0 = copy, 1 = original */
    unsigned char ucHome;                           /* ignore */
    
    /* variable */
    /* 1 bit of the 72-bit copyright ID (transmitted as 1 bit per frame) */
    unsigned char ucCopyBit;
    /* 1 = this bit starts the 72-bit ID, 0 = it does not */
    unsigned char ucCopyStart;
    int           nFrameLength;                    /* length of frame */
    /* number of 32-bit words left in enc buffer, 0x7FF = VBR */
    int           nBufferFull;
    unsigned char ucRawDataBlocks;               /* number of raw data blocks in frame */
    
    /* CRC */
    /* 16-bit CRC check word (present if ucProtectBit == 0) */
    int           nCrcCheckWord;
} ADTSHeaderT;

typedef struct ADIFHeaderS {
    /* 0 = no copyright ID, 1 = 72-bit copyright ID follows immediately */
    unsigned char ucCopyBit;
    unsigned char ucOrigCopy;                       /* 0 = copy, 1 = original */
    unsigned char ucHome;                           /* ignore */
    unsigned char ucBsType;                         /* bitstream type: 0 = CBR, 1 = VBR */
    /* nBitRate: CBR = bits/sec, VBR = peak bits/frame, 0 = unknown */
    int           nBitRate;

    unsigned char ucNumPCE;                  /* number of program config elements (max = 16) */
    int           nBufferFull;                     /* bits left in bit reservoir */
    unsigned char ucCopyID[AAC_ADIF_COPYID_SIZE];  /* optional 72-bit copyright ID */
} ADIFHeaderT;

#define IS_ADIF(p)    (((p)[0] == 'A') && ((p)[1] == 'D') && ((p)[2] == 'I') && ((p)[3] == 'F'))

typedef struct _BitStreamInfo {
    unsigned char *pBytes;
    unsigned int nCache;
    int nCachedBits;
    int nBytes;
} BitStreamInfo;

#define AAC_NUM_PROFILES  3
#define AAC_PROFILE_MP    0
#define AAC_PROFILE_LC    1
#define AAC_PROFILE_SSR   2
#define CDX_ADTS_HEADER_BYTES 7
#define CDX_NUM_SAMPLE_RATES  12
#define CDX_NUM_DEF_CHAN_MAPS 8
#define CDX_NUM_ELEMENTS    8
#define AAC_MAX_NUM_PCE_ADIF  16

#define AAC_MAX_NUM_FCE     15
#define AAC_MAX_NUM_SCE     15
#define AAC_MAX_NUM_BCE     15
#define AAC_MAX_NUM_LCE      3
#define AAC_MAX_NUM_ADE      7
#define AAC_MAX_NUM_CCE     15
/* sizeof(ProgConfigElementT) = 82 bytes (if KEEP_PCE_COMMENTS not defined) */
typedef struct ProgConfigElementS {
    unsigned char ucElemInstTag;                /* element instance tag */
    unsigned char ucProfile;                    /* 0 = main, 1 = LC, 2 = SSR, 3 = reserved */
    unsigned char ucSampRateIdx;                /* sample rate index range = [0, 11] */
    unsigned char ucNumFCE;                     /* number of front channel elements (max = 15) */
    unsigned char ucNumSCE;                     /* number of side channel elements (max = 15) */
    unsigned char ucNumBCE;                     /* number of back channel elements (max = 15) */
    unsigned char ucNumLCE;                     /* number of LFE channel elements (max = 3) */
    unsigned char ucNumADE;                     /* number of associated data elements (max = 7) */
    unsigned char ucNumCCE;                     /* number of valid channel coupling elements
                                                   (max = 15) */
    unsigned char ucMonoMixdown;                /* mono mixdown: bit 4 = present flag,
                                                   bits 3-0 = element number */
    unsigned char ucStereoMixdown;              /* stereo mixdown: bit 4 = present flag,
                                                   bits 3-0 = element number */
    unsigned char ucMatrixMixdown;              /* matrix mixdown: bit 4 = present flag,
                                                   bit 3 = unused, bits 2-1 = index,
                                                   bit 0 = pseudo-surround enable */
    unsigned char ucFce[AAC_MAX_NUM_FCE];       /* front element channel pair:
                                                   bit 4 = ucSce/CPE flag, bits 3-0 = inst tag */
    unsigned char ucSce[AAC_MAX_NUM_SCE];       /* side element channel pair:
                                                   bit 4 = ucSce/CPE flag, bits 3-0 = inst tag */
    unsigned char ucBce[AAC_MAX_NUM_BCE];       /* back element channel pair:
                                                   bit 4 = ucSce/CPE flag, bits 3-0 = inst tag */
    unsigned char ucLce[AAC_MAX_NUM_LCE];       /* instance tag for LFE elements */
    unsigned char ucAde[AAC_MAX_NUM_ADE];       /* instance tag for ucAde elements */
    unsigned char ucCce[AAC_MAX_NUM_BCE];       /* channel coupling elements:
                                                   bit 4 = switching flag, bits 3-0 = inst tag */

#ifdef KEEP_PCE_COMMENTS
    /* make this optional - if not enabled, decoder will just skip comments */
    unsigned char ucCommentBytes;
    unsigned char ucCommentField[MAX_COMMENT_BYTES];
#endif

} ProgConfigElementT;

#endif
