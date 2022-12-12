#define LOG_TAG "g711a_enc.c"

#include <stdlib.h>
#include <string.h>
#include "aenc_sw_lib.h"
#include "g711codec.h" 
#include "alib_log.h"

#define ENCODEEND   (ERR_AUDIO_ENC_ABSEND)
#define ERROR       (ERR_AUDIO_ENC_UNKNOWN)
#define NOENOUGHPCM (ERR_AUDIO_ENC_PCMUNDERFLOW)
#define SUCCESS     (ERR_AUDIO_ENC_NONE)

extern int PCM2G711a( char *InAudioData, char *OutAudioData, int DataLen, int reserve );

int G711aEncInit(struct __AudioENC_AC320 *p)
{ 
    int pcm_frm_size_bytes = 0;
	char *pcm_in_tmp_buff = NULL;
	G711_ENC_PRIV *g711a_enc_priv = NULL;

	g711a_enc_priv = (G711_ENC_PRIV *)malloc(sizeof(G711_ENC_PRIV));
	if(NULL == g711a_enc_priv)
	{
		alib_loge("g711a_malloc_priv_failed:%d",sizeof(G711_ENC_PRIV));
	}

	pcm_frm_size_bytes = (MAXDECODESAMPLE * (p->AudioBsEncInf->SamplerBits>>3)*p->AudioBsEncInf->InChan);
	pcm_in_tmp_buff = (char *)malloc(pcm_frm_size_bytes);
	if(NULL == pcm_in_tmp_buff)
	{
	   alib_loge("g711a_frm_tmp_fuff_malloc_failed!:%d",pcm_frm_size_bytes);
	}
	g711a_enc_priv->pcm_in_buff = pcm_in_tmp_buff;
	p->EncoderCom->pEncInfoSet = (unsigned int *)g711a_enc_priv;
   return 0;
}

int G711aEncExit(struct __AudioENC_AC320 *p)
{ 
   return 0;
} 

int G711aFrameEnc(struct __AudioENC_AC320 *p, char *OutBuffer,int *OutBuffLen)
{
	int	retVal	 = 0;
	int read_sum = 0;
    int nEncodeInSize = 0; 
    int nEncOutSize = 0;
	G711_ENC_PRIV *g711a_enc_priv = NULL;
	char *pcm_in_tmp_buff = NULL;

	g711a_enc_priv = (G711_ENC_PRIV *)p->EncoderCom->pEncInfoSet;
    pcm_in_tmp_buff = g711a_enc_priv->pcm_in_buff;

	*OutBuffLen  = 0; 

	read_sum = GetPcmDataSize(p->pPcmBufManager);

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

    //flash time
    p->EncoderCom->ulNowTimeMS = (unsigned int)((double)p->EncoderCom->framecount * MAXDECODESAMPLE *1000 /(double)(p->AudioBsEncInf->InSamplerate));
    nEncodeInSize = MAXDECODESAMPLE * (p->AudioBsEncInf->SamplerBits>>3)*p->AudioBsEncInf->InChan;
    
    read_sum = ReadPcmDataForEnc((void *)pcm_in_tmp_buff, nEncodeInSize, p->pPcmBufManager);

    nEncOutSize = PCM2G711a((char *)pcm_in_tmp_buff, (char *)OutBuffer, read_sum, 0);
    
    *OutBuffLen = nEncOutSize;

	p->EncoderCom->framecount++;

	return retVal;
} 

struct __AudioENC_AC320 *AudioG711aEncInit(void)
{
	struct  __AudioENC_AC320  *AEncAC320 ;

	AEncAC320 = (struct __AudioENC_AC320 *)malloc(sizeof(struct __AudioENC_AC320));
    if(!AEncAC320)
	return 0;

    memset(AEncAC320, 0, sizeof(struct __AudioENC_AC320));

	AEncAC320->EncInit = G711aEncInit;
	AEncAC320->EncFrame = G711aFrameEnc;
	AEncAC320->EncExit = G711aEncExit;

    alib_loge("g711a_enc_init");

	return AEncAC320;
}

int	AudioG711aEncExit(struct __AudioENC_AC320 *p)
{
	G711_ENC_PRIV *g711a_enc_priv = NULL;
	char *pcm_in_tmp_buff = NULL;

	g711a_enc_priv = (G711_ENC_PRIV *)p->EncoderCom->pEncInfoSet;
    pcm_in_tmp_buff = g711a_enc_priv->pcm_in_buff;

    if(NULL != pcm_in_tmp_buff)
    {
        free(pcm_in_tmp_buff);
        pcm_in_tmp_buff = NULL;
    }

	if(g711a_enc_priv != NULL)
	{
		free(g711a_enc_priv);
	}
	if(NULL != p)
    {
        free(p);
    }
    
    alib_loge("g711a_enc_exit");
	return 0;
}


