#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "alib_log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dlfcn.h>

#include "cedar_abs_packet_hdr.h"
//#if ( __ANDROID__ != 1 && USE_AUDIO_MIX == 1)
#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
#include "aumixcom.h"
#endif

#include "AudioDec_Decode.h"

#include <pthread.h>
#include "CDX_Common.h"

#define  MAX_AUDIO_FRAME_SIZE (96*1024) //8K error!

#include "AudioDec_Decode.h"
#ifdef MEMCHECK
#include "memname.h"
#include "memcheck.h"
#endif

#define UNIT_K(a) (a*1024)
#define TEMP_RESAMPLE_BUFFER_SIZE (256*1024)

#define PROP_RAWDATA_KEY              "mediasw.sft.rawdata"
#define PROP_RAWDATA_MODE_PCM         "PCM"
#define PROP_RAWDATA_MODE_HDMI_RAW    "HDMI_RAW"
#define PROP_RAWDATA_MODE_SPDIF_RAW   "SPDIF_RAW"
#define PROP_RAWDATA_DEFAULT_VALUE    PROP_RAWDATA_MODE_PCM
#ifdef PROPERTY_VALUE_MAX
#undef PROPERTY_VALUE_MAX
#endif
#define PROPERTY_VALUE_MAX 128

#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
#define RATE_LIMIT (32000)
#define DEFUALT_RATE 48000
#endif

#define NORMAL_MIXALBE_16BIT_PCM 0
#define IEC60958_NON_PCM_DATA    1
#define MUTICHAN_LCPM            2

#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG "Allwinner Audio Middle Layer"
#endif

#ifdef  ALIB_DEBUG
#undef  ALIB_DEBUG
#define ALIB_DEBUG 1
#endif


typedef struct __OutputPcmBuf
{
    char  *pAOutBufferStart;
    char  *pAOutBufferEnd;
    int   nAOutBufferTotalSize;
    char  *pAOutBufferWritePtr;
    char  *pAOutBufferReadPtr;
    char  *pAOutPcmEndPtr; //last byte of pcm data of address in buffer, when wrap happens, there will be some unused space in the tail area.PcmEndPtr points to last valid pcm data + 1.
    int   nAOutBufferEmptySize; //pcm valid size in buffer
    int   nAOutBufferValidSize;
    astream_fifo_t  frmFifo;
    unsigned char   extern_buf[512*1024];//buffer wrapped, need extra buffer support
    int   nMaxFrameSizeArox;
    int   nTotalFrameBufferSize;
}OutputPcmBuf;

typedef struct __ACedarContext
{
    int     nOriChannels;//
    int     nOriSampleRate;//
    char    *pTempResampleBuffer;
}ACedarContext;

typedef struct AudioDecoderContextStruct
{
    AudioDecoder      common;
    BsInFor*          pBsInFor;
    AudioDecoderLib*  pAudioDecodeLib;
    int               nCodeType;
    pthread_mutex_t   mutex_audiodec_thread;
    OutputPcmBuf      ADCedarCtxA;
    int mbWaitOutputPcmBufEmptyFlag;
    pthread_cond_t 	mCondOutputPcmBufEmpty;
    ACedarContext     ADCedarCtx;
    CdxPlaybkCfg      raw_data;
    void *            hself;
    int (*streamsize)(AudioDecoderLib* pDecoder);
    int (*ParseRequestAudioBitstreamBuffer)(AudioDecoderLib* pDecoder,
               int           nRequireSize,
               unsigned char**        ppBuf,
               int*          pBufSize,
               unsigned char**        ppRingBuf,
               int*          pRingBufSize,
               int*          nOffset);
    int (*ParseUpdateAudioBitstreamData)(AudioDecoderLib* pDecoder,
                                          int nFilledLen,
                                          int64_t nTimeStamp,
                                          int nOffset);
    void (*BitstreamQueryQuality)(AudioDecoderLib* pDecoder, int* pValidPercent, int* vbv);
    void (*ParseSeekSync                  )(AudioDecoderLib* pDecoder, int64_t nSeekTime,int nGetAudioInfoFlag);

    int (*InitializeAudioDecodeLib)(AudioDecoderLib*    pDecoder,
                           AudioStreamInfo* pAudioStreamInfo,
                           BsInFor *pBsInFor);
    int  (*DecodeAudioFrame                   )(AudioDecoderLib *pDecoder,  char* ppBuf,int* pBufSize);
    int (*DestroyAudioDecodeLib)(AudioDecoderLib* pDecoder);
    void (*SetAudiolibRawParam)(AudioDecoderLib* pDecoder, int commond);
    AudioDecoderLib* (*CreateAudioDecodeLib)(void);
    void (*SetAudioDecEOS)(AudioDecoderLib* pDecoder);
#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
    AudioMix AMX;
#endif
}AudioDecoderContext;


#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
static int do_audioresample(AudioDecoderContext * pAudioDecoder, int insrt,int inch,char* inbuf,int* buflen,int outsrt)
{
    PcmInfo AmxInputA;
    PcmInfo AmxInputB;
    PcmInfo AmxOutput;

    AudioMix* AMX = &pAudioDecoder->AMX;
    int templen = 0,temptotallen = 0,outlen = 0;
    //short   pTempResampleBuffer[64*1024] = {0};

    AMX->InputB = &AmxInputB;
    AMX->InputA = &AmxInputA;
    AMX->Output = &AmxOutput;
    temptotallen = templen = 0;

    AMX->InputB->SampleRate = insrt;
    AMX->InputB->Chan       = inch;
    AMX->InputB->PCMPtr     = (short*)inbuf;
    AMX->InputB->PcmLen     = *buflen / 2 / inch;
    AMX->InputA->PCMPtr = (short*)pAudioDecoder->ADCedarCtx.pTempResampleBuffer;//(pAudioDecoder->ADCedarCtx.pTempResampleBuffer + (inch==1?TEMP_RESAMPLE_BUFFER_SIZE/2:0));
    AMX->InputA->SampleRate = outsrt;
    AMX->InputA->Chan = AMX->InputB->Chan;
    AMX->InputA->PcmLen = (outsrt * AMX->InputB->PcmLen)/insrt + 1;
    AMX->ByPassflag = 2;
    while(AMX->InputB->PcmLen)
    {
        do_AuMIX(AMX);
        templen = AMX->MixLen * 2 * AMX->Mixch;
        temptotallen += templen;
        AMX->InputA->PCMPtr += AMX->MixLen * AMX->Mixch;
    }
    //memcpy(inbuf,pTempResampleBuffer,temptotallen);
    *buflen = temptotallen;
    return 0;

}
#endif

static int AudioResampleSimple(int samplerate,int ch,short* pcmPtr,int* len )
{
    int retSamplerate = samplerate;
    int num  = 1;
    int i = 0,j= 0;
    int samplenum = *len >> 1;
    short *pcmPDtr = pcmPtr;
    short *pcmPStr = pcmPtr;

    if(retSamplerate <= 48000)
    {
        return retSamplerate;
    }

    samplenum /=ch;
    while(retSamplerate > 48000)
    {
        retSamplerate >>= 1;
        samplenum >>=1;
        num <<=1;
    }

    num--;
    for(i=0;i<samplenum;i++)
    {
        for(j=0;j<ch;j++)
        {
            *pcmPStr++ = *pcmPDtr++;
        }
        pcmPDtr +=num*ch;
    }

    *len = (samplenum<<1)*ch;

    return retSamplerate;
}

static void ConvertMonoToStereo(void *src_and_dst, int* size)
{
    int samples = 0;
    short tmp = 0;
    short* psrc_end = NULL;
    int*   pdst_end = NULL;

    samples = ((*size) >> 1);
    if(samples <= 0)
        return;

    *size = (samples << 2);

    while(samples--)
    {
        psrc_end = (short*)src_and_dst;
        pdst_end = (int*)src_and_dst;
        psrc_end += samples;
        pdst_end += samples;
        tmp = *psrc_end;
        *pdst_end = (((tmp<<16)&0xffff0000)|(tmp&0xffff));
    }

}

static void AudioMute(int nMuteMode, short *pPcmData, int nPcmSize)
{
    int i;

    switch (nMuteMode)
    {
        case 1:
            for(i=0; i<nPcmSize;i+=2)
            {
                pPcmData[i] = 0;
            }
            break;
        case 2:
            for(i=1; i<nPcmSize;i+=2)
            {
                pPcmData[i] = 0;
            }
            break;
        case 3:
            memset((void*)pPcmData, 0, nPcmSize*2);
            break;
        default:
            break;
    }
}

static int ADCedarInit(AudioDecoderContext *pAudioDecoder,AudioStreamInfo *pAudioStreamInfo,BsInFor *pBsInFor)
{
    pAudioDecoder->pAudioDecodeLib= pAudioDecoder->CreateAudioDecodeLib();
    if(!pAudioDecoder->pAudioDecodeLib)
    {
        alib_loge("audio decode CreateAudioDecoder fail!");
        return -1;
    }

    if(pAudioDecoder->InitializeAudioDecodeLib(pAudioDecoder->pAudioDecodeLib,pAudioStreamInfo,pBsInFor)!=0)
    {
        alib_loge("audio decode InitializeAudioDecoder fail!");
        return -1;
    }
    return 0;
}


static void ADCedarExit(AudioDecoderContext *pAudioDecoder)
{
    if(pAudioDecoder->DestroyAudioDecodeLib)
    {
        pAudioDecoder->DestroyAudioDecodeLib(pAudioDecoder->pAudioDecodeLib);
        pAudioDecoder->pAudioDecodeLib = 0;
    }
}


static int AdCedarInitFunction(AudioDecoderContext *pAudioDecoder)
{
    pAudioDecoder->ParseSeekSync                    = ParseBitstreamSeekSync          ;
    //add
    pAudioDecoder->ParseRequestAudioBitstreamBuffer = ParseRequestAudioBitstreamBuffer;
    pAudioDecoder->ParseUpdateAudioBitstreamData    = ParseUpdateAudioBitstreamData   ;
    pAudioDecoder->BitstreamQueryQuality            = BitstreamQueryQuality           ;
    pAudioDecoder->streamsize                       = ParseAudioStreamDataSize        ;
    pAudioDecoder->InitializeAudioDecodeLib         = InitializeAudioDecodeLib        ;
    pAudioDecoder->DecodeAudioFrame                 = DecodeAudioFrame                ;
    pAudioDecoder->DestroyAudioDecodeLib            = DestroyAudioDecodeLib           ;
    pAudioDecoder->SetAudiolibRawParam              = SetAudiolibRawParam             ;
    pAudioDecoder->CreateAudioDecodeLib             = CreateAudioDecodeLib            ;
    pAudioDecoder->SetAudioDecEOS                   = SetAudioDecEOS                  ;

    if(!pAudioDecoder->ParseSeekSync                   ||
       !pAudioDecoder->CreateAudioDecodeLib            ||
       !pAudioDecoder->InitializeAudioDecodeLib        ||
       !pAudioDecoder->DestroyAudioDecodeLib           ||
       !pAudioDecoder->ParseRequestAudioBitstreamBuffer||
       !pAudioDecoder->ParseUpdateAudioBitstreamData   ||
       !pAudioDecoder->BitstreamQueryQuality           ||
       !pAudioDecoder->streamsize                      ||
       !pAudioDecoder->DecodeAudioFrame                ||
       !pAudioDecoder->SetAudioDecEOS)
    {
       return 0;
    }
    else
    {
       return 1;
    }

}



int ParserRequestBsBuffer(AudioDecoder* pDecoder,
                          int           nRequireSize,
                          unsigned char**        ppBuf,
                          int*          pBufSize,
                          unsigned char**        ppRingBuf,
                          int*          pRingBufSize,
                          int*          nOffset)
{
    AudioDecoderContext *pAudioDecoder  = (AudioDecoderContext *)pDecoder;
    return pAudioDecoder->ParseRequestAudioBitstreamBuffer(pAudioDecoder->pAudioDecodeLib,
                                            nRequireSize,ppBuf,pBufSize,ppRingBuf,pRingBufSize,nOffset);

}

int ParserUpdateBsBuffer(AudioDecoder* pDecoder, int nFilledLen,int64_t nTimeStamp,int nOffset)
{
    AudioDecoderContext *pAudioDecoder  = (AudioDecoderContext *)pDecoder;
    return pAudioDecoder->ParseUpdateAudioBitstreamData(pAudioDecoder->pAudioDecodeLib,nFilledLen,nTimeStamp,nOffset);
}

void BsQueryQuality(AudioDecoder* pDecoder, int* pValidPercent, int* vbv)
{
    AudioDecoderContext *pAudioDecoder  = (AudioDecoderContext *)pDecoder;
    pAudioDecoder->BitstreamQueryQuality(pAudioDecoder->pAudioDecodeLib,pValidPercent,vbv);
}
int AudioStreamDataSize(AudioDecoder* pDecoder)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    return pAudioDecoder->streamsize(pAudioDecoder->pAudioDecodeLib);
}

int AudioPCMDataSize(AudioDecoder* pDecoder)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    return pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize;
}
static void FlushPcmBuffer(AudioDecoderContext *pAudioDecoder)
{
    pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr       = pAudioDecoder->ADCedarCtxA.pAOutBufferEnd + 1;
    pAudioDecoder->ADCedarCtxA.pAOutBufferWritePtr  = pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr
                                                        = pAudioDecoder->ADCedarCtxA.pAOutBufferStart;
    pAudioDecoder->ADCedarCtxA.nAOutBufferEmptySize = pAudioDecoder->ADCedarCtxA.nAOutBufferTotalSize;
    pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize = 0;
    memset(pAudioDecoder->ADCedarCtxA.frmFifo.inFrames, 0, AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM * sizeof(aframe_info_t));
    pAudioDecoder->ADCedarCtxA.frmFifo.maxchunkNum = AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM;
    pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx = 0;
    pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx = 0;
    pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx = 0;
    pAudioDecoder->ADCedarCtxA.frmFifo.ValidchunkCnt = 0;
}
int DecRequestPcmBuffer(AudioDecoder* pDecoder, char **pOutWritePtr)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    if(pAudioDecoder->ADCedarCtxA.frmFifo.ValidchunkCnt >= AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM)
    {
        return -1;
    }
    cdx_mutex_lock(&pAudioDecoder->mutex_audiodec_thread);
    if(pAudioDecoder->ADCedarCtxA.pAOutBufferWritePtr + pAudioDecoder->ADCedarCtxA.nMaxFrameSizeArox > pAudioDecoder->ADCedarCtxA.pAOutBufferEnd)
    {    //we need to wrap around
        if(pAudioDecoder->ADCedarCtxA.nAOutBufferEmptySize > pAudioDecoder->ADCedarCtxA.nMaxFrameSizeArox)
        {
            if(pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize==0)
            {
                //for readptr = end_ptr error for PlybkRequestPcmBuffer
                FlushPcmBuffer(pAudioDecoder);
            }
            else
            {
                pAudioDecoder->ADCedarCtxA.nAOutBufferEmptySize -= pAudioDecoder->ADCedarCtxA.pAOutBufferEnd + 1 - pAudioDecoder->ADCedarCtxA.pAOutBufferWritePtr;
                pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr = pAudioDecoder->ADCedarCtxA.pAOutBufferWritePtr;
                pAudioDecoder->ADCedarCtxA.pAOutBufferWritePtr = pAudioDecoder->ADCedarCtxA.pAOutBufferStart;
            }

        }
        else
        {
            cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
            return -1;
        }
    }

    if(pAudioDecoder->ADCedarCtxA.nAOutBufferEmptySize > pAudioDecoder->ADCedarCtxA.nMaxFrameSizeArox)
    {
        *pOutWritePtr = pAudioDecoder->ADCedarCtxA.pAOutBufferWritePtr;
        cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
        return 0;
    }
    cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
//    alib_loge("request empty pcm fail: %d-%d-%d-%d, %d", pAudioDecoder->ADCedarCtxA.nAOutBufferEmptySize, 
//                pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize, 
//                pAudioDecoder->ADCedarCtxA.nAOutBufferTotalSize,
//                pAudioDecoder->ADCedarCtxA.nMaxFrameSizeArox,
//                pAudioDecoder->ADCedarCtxA.frmFifo.ValidchunkCnt);
    return -1;
}

// add pcm decoded to pcm output buffer
int DecUpdatePcmBuffer(AudioDecoder* pDecoder, int nPcmOutSize)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    astream_fifo_t* stream = &pAudioDecoder->ADCedarCtxA.frmFifo;
    int factor = 500000;
    if(pAudioDecoder->pBsInFor->bitpersample == 32)
    {
        factor = 250000;
    }
    else if(pAudioDecoder->pBsInFor->bitpersample == 24)
    {
        factor = 333333;
    }

    if(!nPcmOutSize)
    {
         return 0;//no data ,maybe ff/rr
    }
    cdx_mutex_lock(&pAudioDecoder->mutex_audiodec_thread);
    pAudioDecoder->ADCedarCtxA.nAOutBufferEmptySize -= nPcmOutSize;
    pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize += nPcmOutSize;
    pAudioDecoder->ADCedarCtxA.pAOutBufferWritePtr += nPcmOutSize;
    //---------------------------------------------------------------
    memset(&stream->inFrames[stream->wtIdx],0,sizeof(aframe_info_t));
    //stream->inFrames[stream->wtIdx].startAddr = ;
    stream->inFrames[stream->wtIdx].startAddr = (alib_uint8*)pAudioDecoder->ADCedarCtxA.pAOutBufferWritePtr - nPcmOutSize;
    stream->inFrames[stream->wtIdx].pts = pAudioDecoder->pBsInFor->NowPTSTime ; //TODO convert it to 64bit??
    stream->inFrames[stream->wtIdx].len = nPcmOutSize;
    stream->inFrames[stream->wtIdx].ptsValid = (pAudioDecoder->pBsInFor->NowPTSTime != -1);

    stream->wtIdx++;
    if(stream->wtIdx >= AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM)
    {
         stream->wtIdx =0;
    }
    stream->ValidchunkCnt++;
    pAudioDecoder->pBsInFor->NowPTSTime += (int64_t)nPcmOutSize * factor / 
                        (pAudioDecoder->pBsInFor->out_channels * pAudioDecoder->pBsInFor->out_samplerate);
    cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
    return 0;
}

// fetch pcm data from pcm outpub buffer
int PlybkRequestPcmBuffer(AudioDecoder* pDecoder, unsigned char **pOutReadPtr, int *psize)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    cdx_mutex_lock(&pAudioDecoder->mutex_audiodec_thread);

    if(pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize <= 0)
    {
        cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
        return -1;
    }
    if(pAudioDecoder->ADCedarCtxA.frmFifo.ValidchunkCnt <= 0)
    {
        cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
        return -1;
    }
    
    if(pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx > pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx)
    {
        if(pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx >= pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx 
            && pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx<=pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx)
        {
        }
        else
        {
            alib_loge("fatal error! read_id[%d], prefechId[%d], writeId[%d], check code!\n", 
                pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx, pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx, pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx);
            cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
            return -1;
        }
    }
    else if(pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx < pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx)
    {
        if(pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx>=pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx
            || pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx<=pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx)
        {
        }
        else
        {
            alib_loge("fatal error! read_id[%d], prefechId[%d], writeId[%d], check code!\n",  
                pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx, pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx, pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx);
            cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
            return -1;
        }
    }
    else
    {
        alib_logw("Be careful, very rare! frmFifo is full!\n");
        if((unsigned int)pAudioDecoder->ADCedarCtxA.frmFifo.ValidchunkCnt!=pAudioDecoder->ADCedarCtxA.frmFifo.maxchunkNum)
        {
            alib_loge("fatal error! read_id[%d], prefechId[%d], writeId[%d], check code!\n",  
                pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx, pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx, pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx);
            cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
            return -1;
        }
    }
    if(pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx==pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx)
	{
        //there is another possibility: outFrameList is full, but not prefetch, then prefetchIdx==wtIdx don't means all frames are requested,
        //but that case is very rare. So don't worry about it. 
        alib_logv("prefechId[%d]==writeId[%d], readId[%d], all outAudioFrames are requested\n", 
		    pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx, pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx, pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx);
        cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
        return -1;
	}
    *pOutReadPtr = (unsigned char *)pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx].startAddr;
    *psize = pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx].len;
    if((char*)*pOutReadPtr + *psize > pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr)
    {
        alib_loge("fatal error! dataindex[%d] address [%p] + [%d] > pcmEndPtr[%p], check code!", pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx, *pOutReadPtr, *psize, pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr);
    }
    alib_logv("validFrameCnt: %d, read_id: %d, prefetch_id: %d, write_id: %d\n", 
		pAudioDecoder->ADCedarCtxA.frmFifo.ValidchunkCnt, pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx, pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx, pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx);
    pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx++;
    pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx %= AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM;
	cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
    return 0;

    #if 0
    if((*psize > pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize)
    || ( pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize <= 0 && pAudioDecoder->pBsInFor->nBitStreamUnderFlow))
    {
        if( (pAudioDecoder->common.nPrivFlag & CDX_COMP_PRIV_FLAGS_STREAMEOF)
        && pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize > 0 )
        {
             alib_logd("Ohhhhh~~~~   end of stream...");
             *psize = pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize;
        }
        else
        {
            cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
            return -1;
        }
    }

    *pOutReadPtr = (unsigned char *)pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr;
    if(pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr + *psize >= pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr)
    {
        int len0, len1;
        len0 = pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr - pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr;
        len1 = *psize - len0;
        memcpy(&pAudioDecoder->ADCedarCtxA.extern_buf[0], pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr, len0);
        memcpy(&pAudioDecoder->ADCedarCtxA.extern_buf[len0], pAudioDecoder->ADCedarCtxA.pAOutBufferStart, len1);
        *pOutReadPtr = pAudioDecoder->ADCedarCtxA.extern_buf;
    }
    cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
    return 0;
    #endif
}

int PlybkUpdatePcmBuffer(AudioDecoder* pDecoder, int nPcmOutSize)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    int datalen = 0;
    int framelen = 0;
    cdx_mutex_lock(&pAudioDecoder->mutex_audiodec_thread);
    if(pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx].len != (alib_uint32)nPcmOutSize)
    {
        alib_loge("fatal error, check code!buf[%p]size[%d->%d]pts[%lld]", 
            pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx].startAddr,
            nPcmOutSize, pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx].len,
            pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx].pts);
    }
    pAudioDecoder->ADCedarCtxA.nAOutBufferEmptySize += nPcmOutSize;
    pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize -= nPcmOutSize;
    pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr += nPcmOutSize;
    if(pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr >= pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr)
    {
        pAudioDecoder->ADCedarCtxA.nAOutBufferEmptySize += pAudioDecoder->ADCedarCtxA.pAOutBufferEnd + 1 - pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr;
        pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr = pAudioDecoder->ADCedarCtxA.pAOutBufferStart + (pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr - pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr);
        pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr = pAudioDecoder->ADCedarCtxA.pAOutBufferEnd + 1;
    }

    while((nPcmOutSize>datalen)&&(pAudioDecoder->ADCedarCtxA.frmFifo.ValidchunkCnt))
    {
        framelen = pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx].len
                   - pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx].readlen;
        if( framelen <= (nPcmOutSize - datalen))
        {
            pAudioDecoder->ADCedarCtxA.frmFifo.ValidchunkCnt--;
            datalen += framelen;
            pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx++;
            if(pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx > (AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM - 1))
            {
                pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx = 0;
            }
        }
        else
        {
            pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx].readlen += nPcmOutSize - datalen;
            pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx].pts +=
                 (int64_t)(nPcmOutSize - datalen) * (500000) / (pAudioDecoder->pBsInFor->out_channels * pAudioDecoder->pBsInFor->out_samplerate);
            datalen += nPcmOutSize - datalen;
            break;
        }
    }
    if(pAudioDecoder->mbWaitOutputPcmBufEmptyFlag)
    {
        if(pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx == pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx
            && pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx == pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx)
        {
            pAudioDecoder->mbWaitOutputPcmBufEmptyFlag = 0;
            pthread_cond_signal(&pAudioDecoder->mCondOutputPcmBufEmpty);
        }
    }
    cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
    return 0;
}

int64_t PlybkRequestPcmPts(AudioDecoder* pDecoder)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    int64_t pts = -1;
    cdx_mutex_lock(&pAudioDecoder->mutex_audiodec_thread);
    if(pAudioDecoder->pBsInFor->out_channels * pAudioDecoder->pBsInFor->out_samplerate != 0)
    {
        pts = (pAudioDecoder->pBsInFor->nDecodeMode == CDX_DECODER_MODE_RAWMUSIC 
              ? (int64_t)pAudioDecoder->pBsInFor->NowPlayTime * 1000 - (int64_t)pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize * (500000) / (pAudioDecoder->pBsInFor->out_channels * pAudioDecoder->pBsInFor->out_samplerate)
              : (pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize 
                  ? pAudioDecoder->ADCedarCtxA.frmFifo.inFrames[pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx].pts
                  : pAudioDecoder->pBsInFor->NowPTSTime));
    }
    cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
    if(pts < 0)
    {
        pts = 0;
    }
    return pts;
}

int ParserSetDecodingEOS(AudioDecoder* pDecoder)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    cdx_mutex_lock(&pAudioDecoder->mutex_audiodec_thread);
    pAudioDecoder->common.nPrivFlag |= CDX_COMP_PRIV_FLAGS_STREAMEOF;
    cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
    return 0;
}

void PcmQueryQuality(AudioDecoder* pDecoder, int* pValidPercent, int* vbv)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    cdx_mutex_lock(&pAudioDecoder->mutex_audiodec_thread);
    *pValidPercent = pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize * 100 / pAudioDecoder->ADCedarCtxA.nAOutBufferTotalSize;
    *vbv= pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize;
    cdx_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
    return;
}

void AudioDecoderSeek(AudioDecoder* pDecoder, int64_t nSeekTime)
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    if(pAudioDecoder!=NULL && pAudioDecoder->pAudioDecodeLib!=NULL)
    {
        pthread_mutex_lock(&pAudioDecoder->mutex_audiodec_thread);
        while(1)
        {
            if(pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx != pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx
                || pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx != pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx)
            {
                alib_loge("Be careful! OutputPcmBuf prefetchIdx[%d],wtIdx[%d],rdIdx[%d] are not same, need wait!",
                    pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx,
                    pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx,
                    pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx);
                pAudioDecoder->mbWaitOutputPcmBufEmptyFlag = 1;
                pthread_cond_wait(&pAudioDecoder->mCondOutputPcmBufEmpty, &pAudioDecoder->mutex_audiodec_thread);
                alib_loge("Be careful! OutputPcmBuf prefetchIdx[%d], wtIdx[%d], rdIdx[%d] same done?",
                    pAudioDecoder->ADCedarCtxA.frmFifo.prefetchIdx,
                    pAudioDecoder->ADCedarCtxA.frmFifo.wtIdx,
                    pAudioDecoder->ADCedarCtxA.frmFifo.rdIdx);
            }
            else
            {
                break;
            }
        }
        pAudioDecoder->ParseSeekSync(pAudioDecoder->pAudioDecodeLib, nSeekTime, pAudioDecoder->common.nGetAudioInfoFlag);
        FlushPcmBuffer(pAudioDecoder);
        pAudioDecoder->common.nPrivFlag &= ~CDX_COMP_PRIV_FLAGS_STREAMEOF;
        pthread_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
    }
}

int InitializeAudioDecoder(AudioDecoder*    pDecoder,
                           AudioStreamInfo* pAudioStreamInfo,
                           BsInFor* pBsInFor)
{
    AudioDecoderContext *pAudioDecoder  = (AudioDecoderContext *)pDecoder;
    int ret = 0;
    pAudioDecoder->pBsInFor = pBsInFor;
    pAudioDecoder->nCodeType = pAudioStreamInfo->eCodecFormat;
    if(AdCedarInitFunction(pAudioDecoder) == 0)
    {
        alib_loge("Get audio hanlde apis fail!");
        goto EXIT;
    }
    alib_logd("AudioDec_Installaudiolib ok");

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    alib_logd("audio decoder init start ...");

    //ret = pAudioDecData->cedar_init(pAudioDecData);
    ret = ADCedarInit(pAudioDecoder,pAudioStreamInfo,pBsInFor);
    if(ret==-1)
    {
        goto EXIT;
    }
    alib_logd("AUDIO DECODE INIT OK...%d",ret);

    pAudioDecoder->ADCedarCtxA.nMaxFrameSizeArox = MAX_AUDIO_FRAME_SIZE;
    pAudioDecoder->ADCedarCtxA.nTotalFrameBufferSize = AUDIO_PCM_BUFFER_SIZE;

    if( pAudioStreamInfo->eCodecFormat == AUDIO_CODEC_FORMAT_APE )
    {
        int ape_file_num = 0;
        AudioInternalCtl(AUDIO_INTERNAL_CMD_GET_APE_FILE_VERSION, (void*)pAudioStreamInfo->pCodecSpecificData, (void*)(&ape_file_num));
        alib_logd("ape file version : %d", ape_file_num);
        if(ape_file_num < 3930)
        {
            alib_logd("For lower verion ape files, there is possibility counter huge frame size... so malloc huge buffer");
            pAudioDecoder->ADCedarCtxA.nMaxFrameSizeArox     = UNIT_K(288);
            pAudioDecoder->ADCedarCtxA.nTotalFrameBufferSize = UNIT_K(1536);
        }
    }

    pAudioDecoder->ADCedarCtxA.pAOutBufferStart = (char *)malloc(pAudioDecoder->ADCedarCtxA.nTotalFrameBufferSize); 
    if(pAudioDecoder->ADCedarCtxA.pAOutBufferStart==NULL)
    {
        free(pAudioDecoder);
        return 0;
    }

    pAudioDecoder->ADCedarCtxA.nAOutBufferTotalSize = pAudioDecoder->ADCedarCtxA.nTotalFrameBufferSize;
    pAudioDecoder->ADCedarCtxA.pAOutBufferEnd = pAudioDecoder->ADCedarCtxA.pAOutBufferStart + pAudioDecoder->ADCedarCtxA.nAOutBufferTotalSize - 1;
    pAudioDecoder->ADCedarCtxA.pAOutPcmEndPtr = pAudioDecoder->ADCedarCtxA.pAOutBufferEnd + 1;
    pAudioDecoder->ADCedarCtxA.pAOutBufferWritePtr = pAudioDecoder->ADCedarCtxA.pAOutBufferReadPtr = pAudioDecoder->ADCedarCtxA.pAOutBufferStart;
    pAudioDecoder->ADCedarCtxA.nAOutBufferEmptySize = pAudioDecoder->ADCedarCtxA.nAOutBufferTotalSize;
    pAudioDecoder->ADCedarCtxA.nAOutBufferValidSize = 0;
    pAudioDecoder->ADCedarCtxA.frmFifo.inFrames = (aframe_info_t *)malloc(AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM * sizeof(aframe_info_t));

    if(!pAudioDecoder->ADCedarCtxA.frmFifo.inFrames)
    {
        free(pAudioDecoder->ADCedarCtxA.pAOutBufferStart);
        pAudioDecoder->ADCedarCtxA.pAOutBufferStart = 0;
        return -1;
    }
    memset(pAudioDecoder->ADCedarCtxA.frmFifo.inFrames, 0, AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM * sizeof(aframe_info_t));
    pAudioDecoder->ADCedarCtxA.frmFifo.maxchunkNum = AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM;
    pAudioDecoder->common.nGetAudioInfoFlag = 1;

#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
    pAudioDecoder->common.nEnableResample = 1;
#endif
    return 0;
EXIT:
    return -1;
}

int ResetAudioDecoder(AudioDecoder*    pDecoder, int64_t nSeekTime)
{
    AudioDecoderSeek(pDecoder,nSeekTime);
    return 0;
}
int DecodeAudioStream(AudioDecoder*    pDecoder,
                      AudioStreamInfo *pAudioStreamInfo,
                      char*        ppBuf,
                      int*          pBufSize)

{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    int ret =0;
    int OutSampleRate = 0;
    int BsInForSamplerate = 0,BsInForChan = 0;

#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
    PcmInfo AmxInputA;
    PcmInfo AmxInputB;
    PcmInfo AmxOutput;
#endif
    ret = pAudioDecoder->DecodeAudioFrame(pAudioDecoder->pAudioDecodeLib, ppBuf, pBufSize);
    if(ret != 0)
    {
        alib_logw("audiodec ret need aware of: %d",ret);
        return ret;
    }
    else
    {
        BsInForSamplerate = pAudioDecoder->pBsInFor->Samplerate;
        BsInForChan       = pAudioDecoder->pBsInFor->chan;

#ifdef  DOLBY_IPTV_TEST
#define THREE_DB_Q14 23170
        if((*pBufSize) != 0 && (pAudioStreamInfo->eCodecFormat == AUDIO_CODEC_FORMAT_MP1 || pAudioStreamInfo->eCodecFormat == AUDIO_CODEC_FORMAT_MP2))
        {
            int buflen = *pBufSize;
            short* src = (short*)ppBuf;
            int tmp = 0;
            int samples = buflen/BsInForChan/2;
            while(samples--)
            {
                tmp = (int)(src[0]);
                tmp *= THREE_DB_Q14;
                tmp >>= 14;
                if(tmp > 32767) tmp = 32767;
                else if(tmp < -32768) tmp = -32768;
                src[0] = tmp;

                tmp = (int)(src[1]);
                tmp *= THREE_DB_Q14;
                tmp >>= 14;
                if(tmp > 32767) tmp = 32767;
                else if(tmp < -32768) tmp = -32768;
                src[1] = tmp;
                src += 2;
            }
        }
#endif
    }

#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
    if (!ret && pAudioDecoder->common.nEnableResample && (pAudioDecoder->pBsInFor->Samplerate < RATE_LIMIT || pAudioDecoder->pBsInFor->chan==1)&&!pAudioDecoder->pBsInFor->modeflag)
    {
        AudioStreamInfo *pAudioStreamInfo = pAudioStreamInfo;
        int templen = 0,temptotallen = 0;
        short *tempaddr = NULL;
        pAudioDecoder->ADCedarCtx.nOriSampleRate = BsInForSamplerate;
        pAudioDecoder->ADCedarCtx.nOriChannels   = BsInForChan;
        pAudioDecoder->AMX.InputB = &AmxInputB;
        pAudioDecoder->AMX.InputA = &AmxInputA;
        pAudioDecoder->AMX.Output = &AmxOutput;

        if(BsInForSamplerate < RATE_LIMIT)
        {
            do_audioresample(pAudioDecoder, pAudioDecoder->pBsInFor->Samplerate, pAudioDecoder->pBsInFor->chan, ppBuf, pBufSize, DEFUALT_RATE);
            pAudioDecoder->pBsInFor->out_samplerate = BsInForSamplerate = DEFUALT_RATE;
        }
        else if(BsInForChan == 1)
        { //only do convert to double channel
            memcpy(pAudioDecoder->ADCedarCtx.pTempResampleBuffer , ppBuf, *pBufSize);
        }

        //convert it to double channel
        if (BsInForChan == 1)
        {
            ConvertMonoToStereo(pAudioDecoder->ADCedarCtx.pTempResampleBuffer, pBufSize);
        }

        pAudioDecoder->pBsInFor->out_channels = BsInForChan = 2;

        if(*pBufSize <= TEMP_RESAMPLE_BUFFER_SIZE / 2)
        {
            memcpy(ppBuf, pAudioDecoder->ADCedarCtx.pTempResampleBuffer, *pBufSize);
        }
        else
        {
            alib_loge("Resample overflow!");
        }
    }
#endif
    if( ret == 0 )
    {
        if((!pAudioDecoder->raw_data.nNeedDirect)&&(pAudioDecoder->pBsInFor->modeflag != NORMAL_MIXALBE_16BIT_PCM))//init only raw
        {
            pAudioDecoder->raw_data.nChannels = BsInForChan;
            pAudioDecoder->raw_data.nSamplerate = BsInForSamplerate;
            switch(pAudioDecoder->nCodeType)
            {
                case AUDIO_CODEC_FORMAT_AC3:
                    pAudioDecoder->raw_data.nDataType = AUDIO_RAW_DATA_AC3;
                    if(pAudioDecoder->pBsInFor->Samplerate > 48000)
                        pAudioDecoder->raw_data.nDataType = AUDIO_RAW_DATA_DOLBY_DIGITAL_PLUS;
                    if(pAudioDecoder->pBsInFor->Samplerate == 192000 && pAudioDecoder->pBsInFor->chan == 8)
                        pAudioDecoder->raw_data.nDataType = AUDIO_RAW_DATA_MAT;
                    break;
                case AUDIO_CODEC_FORMAT_DTS:
                    pAudioDecoder->raw_data.nDataType = AUDIO_RAW_DATA_DTS;
                    if(pAudioDecoder->pBsInFor->Samplerate > 48000)
                        pAudioDecoder->raw_data.nDataType = AUDIO_RAW_DATA_DTS_HD;
                    break;
                case AUDIO_CODEC_FORMAT_DSD:
                    pAudioDecoder->raw_data.nDataType = AUDIO_RAW_DATA_ONE_BIT_AUDIO;
                    break;
                case AUDIO_CODEC_FORMAT_UNKNOWN:
                case AUDIO_CODEC_FORMAT_MP1:
                case AUDIO_CODEC_FORMAT_MP2:
                case AUDIO_CODEC_FORMAT_MP3:
                case AUDIO_CODEC_FORMAT_MPEG_AAC_LC:
                case AUDIO_CODEC_FORMAT_LPCM_V:
                case AUDIO_CODEC_FORMAT_LPCM_A:
                case AUDIO_CODEC_FORMAT_ADPCM:
                case AUDIO_CODEC_FORMAT_PCM:
                case AUDIO_CODEC_FORMAT_WMA_STANDARD:
                case AUDIO_CODEC_FORMAT_FLAC:
                case AUDIO_CODEC_FORMAT_APE:
                case AUDIO_CODEC_FORMAT_OGG:
                case AUDIO_CODEC_FORMAT_RAAC:
                case AUDIO_CODEC_FORMAT_COOK:
                case AUDIO_CODEC_FORMAT_SIPR:
                case AUDIO_CODEC_FORMAT_ATRC:
                case AUDIO_CODEC_FORMAT_AMR:
                case AUDIO_CODEC_FORMAT_RA:
                default:
                    pAudioDecoder->raw_data.nDataType = AUDIO_RAW_DATA_PCM;
                    break;
            }

            if(pAudioDecoder->pBsInFor->modeflag == MUTICHAN_LCPM)//hdmi direct pcm
            {
                pAudioDecoder->raw_data.nDataType = AUDIO_RAW_DATA_PCM;
            }

            if(pAudioDecoder->pBsInFor->bitpersample == 24)
            {
                pAudioDecoder->raw_data.nBitpersample = 24;
            }
            else if(pAudioDecoder->pBsInFor->bitpersample == 32)
            {
                pAudioDecoder->raw_data.nBitpersample = 32;
            }
            else
            {
                pAudioDecoder->raw_data.nBitpersample = 16;
            }

            pAudioDecoder->raw_data.nNeedDirect = 1;
            pAudioDecoder->common.nGetAudioInfoFlag = 0;
            memcpy(&pAudioStreamInfo->raw_data,&pAudioDecoder->raw_data,sizeof(CdxPlaybkCfg));
        }
    }


    if(!pAudioDecoder->pBsInFor->modeflag)
    {
        if(BsInForSamplerate > 48000)
        {
            OutSampleRate = AudioResampleSimple(BsInForSamplerate,BsInForChan,(short*)ppBuf,pBufSize);
        }
        else
        {
            OutSampleRate = BsInForSamplerate;
        }
        if (pAudioStreamInfo->nChannelNum != BsInForChan
        || pAudioStreamInfo->nSampleRate != OutSampleRate)
        {
            pAudioDecoder->common.nGetAudioInfoFlag = 1;
            alib_logw("get audio decoder change ch or fs!");
        }

        if (pAudioDecoder->common.nGetAudioInfoFlag)
        {
            //AudioStreamInfo *pAudioStreamInfo = pAudioStreamInfo;

            pAudioStreamInfo->nChannelNum = pAudioDecoder->pBsInFor->out_channels = pAudioDecoder->ADCedarCtx.nOriChannels = BsInForChan;
            pAudioStreamInfo->nSampleRate =  pAudioDecoder->pBsInFor->out_samplerate = pAudioDecoder->ADCedarCtx.nOriSampleRate = OutSampleRate;//pAudioDecData->pBsInFor.Samplerate;
            pAudioDecoder->common.nGetAudioInfoFlag = 0;
            if (!pAudioDecoder->pBsInFor->out_channels || !pAudioDecoder->pBsInFor->out_samplerate)
            {
                pAudioDecoder->common.nGetAudioInfoFlag = 1;
                alib_logw("get audio decoder info fail!");
                return -1;
            }
            alib_logv("============ ad_cedar info [channel:%d samplerate:%d] =============",
            BsInForChan, BsInForSamplerate);
            pAudioDecoder->raw_data.nNeedDirect = 0;
        }

        if(pAudioDecoder->common.mute)
        {
            AudioMute(pAudioDecoder->common.mute, (short *)ppBuf, *pBufSize/2);
        }

#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
        if(!pAudioDecoder->raw_data.nNeedDirect)//init  raw
        {
            pAudioDecoder->raw_data.nChannels = BsInForChan;
            pAudioDecoder->raw_data.nSamplerate = OutSampleRate;
            pAudioDecoder->raw_data.nDataType = AUDIO_RAW_DATA_PCM;
            pAudioDecoder->raw_data.nBitpersample = 16;
            pAudioDecoder->raw_data.nNeedDirect = 1;
            memcpy(&pAudioStreamInfo->raw_data,&pAudioDecoder->raw_data,sizeof(CdxPlaybkCfg));
        }
#endif
    }
    else
    {
        pAudioStreamInfo->nChannelNum = pAudioDecoder->pBsInFor->out_channels = pAudioDecoder->ADCedarCtx.nOriChannels = BsInForChan;
        pAudioStreamInfo->nSampleRate =    pAudioDecoder->pBsInFor->out_samplerate = pAudioDecoder->ADCedarCtx.nOriSampleRate = BsInForSamplerate;
    }
    return ret;
EXIT:
    return ERR_AUDIO_DEC_EXIT;

}
int DestroyAudioDecoder(AudioDecoder* pDecoder)
{
    AudioDecoderContext *pAudioDecoder  = (AudioDecoderContext *)pDecoder;
    if(!pAudioDecoder)
    {
        return -1;
    }
    pthread_mutex_lock(&pAudioDecoder->mutex_audiodec_thread);
    pthread_cond_destroy(&pAudioDecoder->mCondOutputPcmBufEmpty);
    pthread_mutex_unlock(&pAudioDecoder->mutex_audiodec_thread);
    pthread_mutex_destroy(&pAudioDecoder->mutex_audiodec_thread);
    ADCedarExit(pAudioDecoder);

#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
    if(pAudioDecoder->ADCedarCtx.pTempResampleBuffer)
    {
        free(pAudioDecoder->ADCedarCtx.pTempResampleBuffer);
        pAudioDecoder->ADCedarCtx.pTempResampleBuffer = NULL;
    }
    if(pAudioDecoder->AMX.RESI != NULL)
    {
        alib_logd("destroy_ResampleInfo!!");
        Destroy_ResampleInfo(pAudioDecoder->AMX.RESI);
        pAudioDecoder->AMX.RESI = NULL;
    }
#endif

    if(pAudioDecoder->ADCedarCtxA.pAOutBufferStart)
    {
        free(pAudioDecoder->ADCedarCtxA.pAOutBufferStart);
        pAudioDecoder->ADCedarCtxA.pAOutBufferStart = NULL;
    }
    if(pAudioDecoder->ADCedarCtxA.frmFifo.inFrames)
    {
        free(pAudioDecoder->ADCedarCtxA.frmFifo.inFrames);
        pAudioDecoder->ADCedarCtxA.frmFifo.inFrames = NULL;
    }
    free(pAudioDecoder);
#ifdef MEMCHECK
    showmeinfo();
#endif
    pAudioDecoder = NULL;
    return 0;
}
AudioDecoder* CreateAudioDecoder(void)
{
    int ret;
    AudioDecoderContext *pAudioDecoder = malloc(sizeof(AudioDecoderContext));
    if(!pAudioDecoder)
    {
        alib_loge("malloc pAudioDecoder fail!");
        return 0;
    }
    memset(pAudioDecoder,0,sizeof(AudioDecoderContext));
    ret = pthread_mutex_init(&pAudioDecoder->mutex_audiodec_thread, NULL);
    if(ret != 0)
    {
        alib_loge("mutex init fail!");
    }
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    ret = pthread_cond_init(&pAudioDecoder->mCondOutputPcmBufEmpty, &condAttr);
    if(ret != 0)
    {
        alib_loge("cond init fail!");
    }

#if (!defined(__ANDROID__) || __ANDROID__ != 1) && (defined(USE_AUDIO_MIX) && USE_AUDIO_MIX == 1)
    pAudioDecoder->ADCedarCtx.pTempResampleBuffer = malloc(TEMP_RESAMPLE_BUFFER_SIZE);
    if(!pAudioDecoder->ADCedarCtx.pTempResampleBuffer)
    {
        alib_loge("malloc pTempResampleBuffer fail!");
        free(pAudioDecoder);
        return 0;
    }

    pAudioDecoder->AMX.RESI = Init_ResampleInfo();
    if(pAudioDecoder->AMX.RESI == NULL)
    {
        alib_loge("Init_ResampleInfo fail!!");
        free(pAudioDecoder);
        return 0;
    }
#endif
    alib_logd("Create Decoder!!=====");
    return (AudioDecoder*)pAudioDecoder;
}

int CheckAudioDecoder(AudioDecoder* pDecoder) //just for checking, dont touch...
{
    AudioDecoderContext *pAudioDecoder = (AudioDecoderContext *)pDecoder;
    (void)pAudioDecoder;

    return 0;
}

#if (defined(__ANDROID__) && __ANDROID__ == 1)
void SetRawPlayParam(AudioDecoder* pDecoder,void *self)
#else
void SetRawPlayParam(AudioDecoder* pDecoder,void *self,int flag)
#endif
{
    AudioDecoderContext *pAudioDecoder    = (AudioDecoderContext *)pDecoder;
    char prop_value[PROPERTY_VALUE_MAX] = {0};
#if (defined(__ANDROID__) && __ANDROID__ == 1)
    int len = 0;    
    len = __system_property_get(PROP_RAWDATA_KEY, prop_value);
    if(len <= 0) {
        len = strlen(PROP_RAWDATA_DEFAULT_VALUE);
        memcpy(prop_value, PROP_RAWDATA_DEFAULT_VALUE, len + 1);
    }
#endif
    if(!strcmp(prop_value, PROP_RAWDATA_MODE_HDMI_RAW))
        pAudioDecoder->raw_data.nRoutine = 1;//AUDIO_DATA_MODE_HDMI_RAW;
    else if(!strcmp(prop_value, PROP_RAWDATA_MODE_SPDIF_RAW))
        pAudioDecoder->raw_data.nRoutine = 2;//AUDIO_DATA_MODE_SPDIF_RAW;
    else
        pAudioDecoder->raw_data.nRoutine = 0;//AUDIO_DATA_MODE_PCM;
#if (!defined(__ANDROID__) || __ANDROID__ != 1) //#if ( __ANDROID__ != 1 )
    pAudioDecoder->raw_data.nRoutine = flag;
#endif
    pAudioDecoder->SetAudiolibRawParam(pAudioDecoder->pAudioDecodeLib, pAudioDecoder->raw_data.nRoutine);
    pAudioDecoder->hself = self;
}

int AudioStreamBufferSize(void)
{
    return AUDIO_BITSTREAM_BUFFER_SIZE;
}

int AudioStreamBufferMaxFrameNum(void)
{
    return AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
