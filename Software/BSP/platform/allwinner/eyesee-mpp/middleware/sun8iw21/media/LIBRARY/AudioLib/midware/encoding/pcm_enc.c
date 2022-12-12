/*
 * encode.c
 *
 * CCITT PCM encoder
 *
 *
 * author: lszhang	2011-12-20 09:37:51
 *
 */

/*******************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "pcm_enc.c"

#include <stdlib.h>
#include <string.h>
#include "aenc_sw_lib.h"
#include "alib_log.h"

#define ENCODEEND   (ERR_AUDIO_ENC_ABSEND)
#define ERROR       (ERR_AUDIO_ENC_UNKNOWN)
#define NOENOUGHPCM (ERR_AUDIO_ENC_PCMUNDERFLOW)
#define SUCCESS     (ERR_AUDIO_ENC_NONE)
//#define MAXDECODESAMPLE 1024
#define DECODEMPEGTS   1920
static unsigned char pcm_head[44]={
	'R','I','F','F',
	0x00,0x00,0x00,0x00,	//last set	0x04-0x07
	'W','A','V','E',
	'f','m','t',' ',

	0x10,0x00,0x00,0x00,	//fmtlength
	0x01,0x00,				//formattag
	0x02,0x00,			    //channels=2			will set	0x16-0x17
	0x40,0x1F,0x00,0x00,	//samplerate=8000		will set	0x18-0x1B
	0x00,0x7D,0x00,0x00,	//nAvgBytesperSec		will set	0x1c-0x1F
	0x04,0x00,			    //blocksize				will set	0x20-0x21
	0x10,0x00,				//bitsofsample

	'd','a','t','a',
	0x00,0x00,0x00,0x00		//last set;	0x38-0x1B
};

/********************************************************/

extern  int	ReadPcmDataForEnc(void *buf, int Len,__pcm_buf_manager_t *FileInfo);
extern  int	GetPcmDataSize(__pcm_buf_manager_t *FileInfo);
int	PCMEncInit(struct __AudioENC_AC320 *p);
int		PCMframeEnc(struct __AudioENC_AC320 *p, char *OutBuffer,int *OutBuffLen );
int	PCMEncExit(struct __AudioENC_AC320 *p);

struct __AudioENC_AC320 *AudioPCMEncInit(void)
{
	struct  __AudioENC_AC320  *AEncAC320 ;

	AEncAC320 = (struct __AudioENC_AC320 *)malloc(sizeof(struct __AudioENC_AC320));
    if(!AEncAC320)
	return 0;

    memset(AEncAC320, 0, sizeof(struct __AudioENC_AC320));

	AEncAC320->EncInit = PCMEncInit;
	AEncAC320->EncFrame = PCMframeEnc;
	AEncAC320->EncExit = PCMEncExit;

	return AEncAC320;
}

int	AudioPCMEncExit(struct __AudioENC_AC320 *p)
{
	free(p);

	return 0;
}

int PCMEncInit(struct __AudioENC_AC320 *p)
{

   memcpy(p->EncoderCom->BsHeaderBuf,pcm_head,44);
   p->EncoderCom->ValidHeaderLen = 44;

   return 0;
}

int PCMEncExit(struct __AudioENC_AC320 *p)
{
   int *phead  = NULL;
   phead       = (int*)(&pcm_head[4]);
   *phead      = p->EncoderCom->framecount * MAXDECODESAMPLE * (p->AudioBsEncInf->SamplerBits>>3) * p->AudioBsEncInf->InChan + 36;
   phead       = (int*)(&pcm_head[40]);
   *phead      = p->EncoderCom->framecount * MAXDECODESAMPLE * (p->AudioBsEncInf->SamplerBits>>3) * p->AudioBsEncInf->InChan;
   memcpy(p->EncoderCom->BsHeaderBuf,pcm_head,44);
   p->EncoderCom->ValidHeaderLen = 44;

   return 0;
}

int PCMframeEnc(struct __AudioENC_AC320 *p, char *OutBuffer,int *OutBuffLen)
{
	int	retVal	 = 0;
	int read_sum = 0;
    int nEncodeOutSize = 0;

	*OutBuffLen  = 0;
	//init static data;
	if(!p->Encinitedflag)
	{
		if(((p->AudioBsEncInf->SamplerBits != 8) & (p->AudioBsEncInf->SamplerBits != 16)
			& (p->AudioBsEncInf->SamplerBits != 24)& (p->AudioBsEncInf->SamplerBits != 32)) |
			 ((p->AudioBsEncInf->InChan < 0) |(p->AudioBsEncInf->InChan > 6)))return ERROR;

		//set adpcm_head willset;
		//samplerate
		pcm_head[0x18] = (unsigned char)(p->AudioBsEncInf->InSamplerate & 0xff);
		pcm_head[0x19] = (unsigned char)((p->AudioBsEncInf->InSamplerate>>8)  & 0xff);
		pcm_head[0x1A] = (unsigned char)((p->AudioBsEncInf->InSamplerate>>16) & 0xff);
		pcm_head[0x1B] = (unsigned char)((p->AudioBsEncInf->InSamplerate>>24) & 0xff);
		//channels
		pcm_head[0x16] = (unsigned char)(p->AudioBsEncInf->InChan & 0xff);
		pcm_head[0x17] = (unsigned char)((p->AudioBsEncInf->InChan>>8)& 0xff);;
		//nAvgBytesperSec
		pcm_head[0x1C] = (unsigned char)((p->AudioBsEncInf->InSamplerate* (p->AudioBsEncInf->SamplerBits>>3) * p->AudioBsEncInf->InChan)&0xff);
		pcm_head[0x1D] = (unsigned char)(((p->AudioBsEncInf->InSamplerate* (p->AudioBsEncInf->SamplerBits>>3) * p->AudioBsEncInf->InChan) >> 8) & 0xff);
		pcm_head[0x1E] = (unsigned char)(((p->AudioBsEncInf->InSamplerate* (p->AudioBsEncInf->SamplerBits>>3) * p->AudioBsEncInf->InChan) >> 16) & 0xff);
		pcm_head[0x1F] = (unsigned char)(((p->AudioBsEncInf->InSamplerate* (p->AudioBsEncInf->SamplerBits>>3) * p->AudioBsEncInf->InChan) >> 24) & 0xff);
		//blocksize
		pcm_head[0x20] = (unsigned char)(((p->AudioBsEncInf->SamplerBits>>3) * p->AudioBsEncInf->InChan)&0xff);
		pcm_head[0x21] = (unsigned char)((((p->AudioBsEncInf->SamplerBits>>3) * p->AudioBsEncInf->InChan) >> 8) & 0xff);
		p->Encinitedflag = 1;
	}

	read_sum = GetPcmDataSize(p->pPcmBufManager);
	if(p->AudioBsEncInf->frame_style==2)
	{
		if(read_sum<DECODEMPEGTS)
		{
			if(p->EncoderCom->ulEncCom == 5)
			{
				return ENCODEEND;//stop
			}
			else
			{
				return NOENOUGHPCM;//wait enough data
			}
		}
	}
	else
	{
		if(read_sum<MAXDECODESAMPLE * (p->AudioBsEncInf->SamplerBits>>3)*p->AudioBsEncInf->InChan)
		{
			if(p->EncoderCom->ulEncCom == 5)
			{
				return ENCODEEND;//stop
			}
			else
			{
				return NOENOUGHPCM;//wait enough data
			}
		}
	}

	if(p->AudioBsEncInf->frame_style == 2)
	{
		int i;
		int sampling_frequence = 3; //fixed it later
	    short   *wordptr;
	    short   byte1,byte2,word;

		//flash time
	p->EncoderCom->ulNowTimeMS = (unsigned int)((double)p->EncoderCom->framecount * DECODEMPEGTS *1000/((double)p->AudioBsEncInf->InSamplerate*2*p->AudioBsEncInf->InChan));

		alib_logv("p->EncoderCom->ulNowTimeMS: %d", p->EncoderCom->ulNowTimeMS);
		read_sum = ReadPcmDataForEnc((void *)(OutBuffer+4),DECODEMPEGTS, p->pPcmBufManager);

		//big little
		//4 bytes
		OutBuffer[0] = 0xA0;
		OutBuffer[1] = 0x06;
		OutBuffer[2] = 0;
		OutBuffer[3] = 0;

		wordptr = (short *)(OutBuffer + 4);

		if(p->AudioBsEncInf->InSamplerate == 44100)
		{
			sampling_frequence = 1;
		}
		else if(p->AudioBsEncInf->InSamplerate == 48000)
		{
			sampling_frequence = 2;
		}
		else
		{
			alib_loge("unsupport audio Sample rate: %d", p->AudioBsEncInf->InSamplerate);
		}

		OutBuffer[3] =  sampling_frequence<<3 | 0x01;

		for (i = 0; i < DECODEMPEGTS / 2; i++)
		{
            word = *wordptr;//pk_buf;
            byte1 = ( word << 8 ) & 0xff00;
            byte2 = ( word >> 8 ) & 0x00ff;
            *wordptr++ = (short)(byte1 | byte2);
		}

		*OutBuffLen = read_sum + 4;
	}
	else
	{
		//flash time
	p->EncoderCom->ulNowTimeMS = (unsigned int)((double)p->EncoderCom->framecount * MAXDECODESAMPLE *1000 /(double)(p->AudioBsEncInf->InSamplerate));
        nEncodeOutSize = MAXDECODESAMPLE * (p->AudioBsEncInf->SamplerBits>>3)*p->AudioBsEncInf->InChan;
        if(nEncodeOutSize > OUT_ENCODE_BUFFER_SIZE)
        {
            alib_loge("(f:%s, l:%d) must modify code! encodeOutBufSize[%d] < len[%d]", __FUNCTION__, __LINE__, OUT_ENCODE_BUFFER_SIZE, nEncodeOutSize);
            nEncodeOutSize = OUT_ENCODE_BUFFER_SIZE;
        }
		read_sum = ReadPcmDataForEnc((void *)OutBuffer, nEncodeOutSize, p->pPcmBufManager);

		*OutBuffLen = read_sum;
	}

	p->EncoderCom->framecount++;

	return retVal;
}
