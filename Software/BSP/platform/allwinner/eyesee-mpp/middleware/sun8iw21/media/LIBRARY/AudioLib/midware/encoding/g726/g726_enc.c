
#define LOG_TAG "g726_enc.c"

#include <stdlib.h>
#include <string.h>
#include "aenc_sw_lib.h"
#include "alib_log.h"
#include "g726.h" 


#define ENCODEEND   (ERR_AUDIO_ENC_ABSEND)
#define ERROR       (ERR_AUDIO_ENC_UNKNOWN)
#define NOENOUGHPCM (ERR_AUDIO_ENC_PCMUNDERFLOW)
#define SUCCESS     (ERR_AUDIO_ENC_NONE)
/* ................... Begin of alaw_compress() ..................... */
/*
  ==========================================================================

   FUNCTION NAME: alaw_compress

   DESCRIPTION: ALaw encoding rule according ITU-T Rec. G.711.

   PROTOTYPE: void alaw_compress(long lseg, short *linbuf, short *logbuf)

   PARAMETERS:
     lseg:	(In)  number of samples
     linbuf:	(In)  buffer with linear samples (only 12 MSBits are taken
                      into account)
     logbuf:	(Out) buffer with compressed samples (8 bit right justified,
                      without sign extension)

   RETURN VALUE: none.

   HISTORY:
   10.Dec.91	1.0	Separated A-law compression function

  ==========================================================================
*/
static void alaw_compress (long lseg, short *linbuf, short *logbuf) {
  short ix, iexp;
  long n;

  for (n = 0; n < lseg; n++) {
    ix = linbuf[n] < 0          /* 0 <= ix < 2048 */
      ? (~linbuf[n]) >> 4       /* 1's complement for negative values */
      : (linbuf[n]) >> 4;

    /* Do more, if exponent > 0 */
    if (ix > 15) {              /* exponent=0 for ix <= 15 */
      iexp = 1;                 /* first step: */
      while (ix > 16 + 15) {    /* find mantissa and exponent */
        ix >>= 1;
        iexp++;
      }
      ix -= 16;                 /* second step: remove leading '1' */

      ix += iexp << 4;          /* now compute encoded value */
    }
    if (linbuf[n] >= 0)
      ix |= (0x0080);           /* add sign bit */

    logbuf[n] = ix ^ (0x0055);  /* toggle even bits */
  }
}

/* ................... End of alaw_compress() ..................... */
/* ................... Begin of ulaw_compress() ..................... */
/*
  ==========================================================================

   FUNCTION NAME: ulaw_compress

   DESCRIPTION: Mu law encoding rule according ITU-T Rec. G.711.

   PROTOTYPE: void ulaw_compress(long lseg, short *linbuf, short *logbuf)

   PARAMETERS:
     lseg:	(In)  number of samples
     linbuf:	(In)  buffer with linear samples (only 12 MSBits are taken
                      into account)
     logbuf:	(Out) buffer with compressed samples (8 bit right justified,
                      without sign extension)

   RETURN VALUE: none.

   HISTORY:
   10.Dec.91	1.0	Separated mu-law compression function

  ==========================================================================
*/
static void ulaw_compress (long lseg, short *linbuf, short *logbuf) {
  long n;                       /* samples's count */
  short i;                      /* aux.var. */
  short absno;                  /* absolute value of linear (input) sample */
  short segno;                  /* segment (Table 2/G711, column 1) */
  short low_nibble;             /* low nibble of log companded sample */
  short high_nibble;            /* high nibble of log companded sample */

  for (n = 0; n < lseg; n++) {
    /* -------------------------------------------------------------------- */
    /* Change from 14 bit left justified to 14 bit right justified */
    /* Compute absolute value; adjust for easy processing */
    /* -------------------------------------------------------------------- */
    absno = linbuf[n] < 0       /* compute 1's complement in case of */
      ? ((~linbuf[n]) >> 2) + 33        /* negative samples */
      : ((linbuf[n]) >> 2) + 33;        /* NB: 33 is the difference value */
    /* between the thresholds for */
    /* A-law and u-law. */
    if (absno > (0x1FFF))       /* limitation to "absno" < 8192 */
      absno = (0x1FFF);

    /* Determination of sample's segment */
    i = absno >> 6;
    segno = 1;
    while (i != 0) {
      segno++;
      i >>= 1;
    }

    /* Mounting the high-nibble of the log-PCM sample */
    high_nibble = (0x0008) - segno;

    /* Mounting the low-nibble of the log PCM sample */
    low_nibble = (absno >> segno)       /* right shift of mantissa and */
      &(0x000F);                /* masking away leading '1' */
    low_nibble = (0x000F) - low_nibble;

    /* Joining the high-nibble and the low-nibble of the log PCM sample */
    logbuf[n] = (high_nibble << 4) | low_nibble;

    /* Add sign bit */
    if (linbuf[n] >= 0)
      logbuf[n] = logbuf[n] | (0x0080);
  }
}

/* ................... End of ulaw_compress() ..................... */
static int g726encode_func(g726_enc_handle__ * enc_handle, short *input_pcm,
					long sampno, unsigned char *g726encbuf)
{
	int ig726_bytes		= 0; 

	int i				= 0;

	int ibitstream		= 0; 
	int iresidue		= 0; 
	unsigned char code	= 0; 

	short temp_compress[SAMPLE_NUM]   = { 0 }; 
	short g726encbuf_temp[SAMPLE_NUM] = { 0 }; 

	if ((NULL == enc_handle) ||
		(NULL == input_pcm)  ||
		(NULL == g726encbuf)) 
	{
		printf("error parameter!! \n");
		return 0;
	}

	// param ¼ì²é
	if ((enc_handle->rate != 2) &&
		(enc_handle->rate != 3) &&
		(enc_handle->rate != 4) &&
		(enc_handle->rate != 5))
	{
		printf(" Invalid rate (5/4/3/2) or (40/32/24/16)! Aborted...\n");
		return 0;
	}

	if ('1' == enc_handle->law)
	{
		alaw_compress(sampno, input_pcm, temp_compress);
	}
	else
	{
		ulaw_compress(sampno, input_pcm, temp_compress);
	}

	G726_encode(temp_compress, 
				g726encbuf_temp,
				sampno,
				&(enc_handle->law),
				enc_handle->rate,
				enc_handle->reset,
				&(enc_handle->encoder_state));


	for (i = 0; i < sampno; i++)
	{
		code = g726encbuf_temp[i];
		ibitstream = (ibitstream << enc_handle->rate) | code;
		iresidue += enc_handle->rate;
		if (iresidue >=  8)
		{
			g726encbuf[ig726_bytes++] = (unsigned char)((ibitstream >> (iresidue - 8)) & 0xFF);
			iresidue -= 8;
		}
	}

	if (ig726_bytes != enc_handle->g726_enc_bytes)
	{
		printf("warning!!! ig726_bytes = %d\n", ig726_bytes);
	}

	return ig726_bytes;
}



static int G726EncInit(struct __AudioENC_AC320 *p)
{ 
    int pcm_frm_size_bytes = 0;
    g726_enc_handle__ *g726_enc_priv = NULL; 
	char *pcm_in_tmp_buff = NULL;

    g726_enc_priv = (g726_enc_handle__ *)malloc(sizeof(g726_enc_handle__));
    if(NULL == g726_enc_priv)
    {
        alib_loge("g726_priv_malloc_fialed:%d",sizeof(g726_enc_handle__));
    }

    memset(g726_enc_priv,0,sizeof(g726_enc_handle__));

	pcm_frm_size_bytes = (MAXDECODESAMPLE * (p->AudioBsEncInf->SamplerBits>>3)*p->AudioBsEncInf->InChan);
	pcm_in_tmp_buff = (char *)malloc(pcm_frm_size_bytes);
	if(NULL == pcm_in_tmp_buff)
	{
	   alib_loge("g726_frm_tmp_fuff_malloc_failed!:%d",pcm_frm_size_bytes);
	}

	g726_enc_priv->pcm_in_buff = pcm_in_tmp_buff;
    switch(p->AudioBsEncInf->bitrate)
    {
        case 16000:
        {
            g726_enc_priv->rate = 2;
            break;
        }
        case 24000:
        {
            g726_enc_priv->rate = 3;
            break;
        }
        case 32000:
        {
            g726_enc_priv->rate = 4;
            break;
        }
        case 40000:
        {
            g726_enc_priv->rate = 5;
            break;
        }
        default:
        {
            g726_enc_priv->rate = 4;
            break;
        }
    }

    if(1 == p->AudioBsEncInf->g726_enc_law)
    {
        g726_enc_priv->law = '1';
    }
    else if(0 == p->AudioBsEncInf->g726_enc_law)
    {
        g726_enc_priv->law = '0';
    }
    else
    {
        alib_loge("g726_unsupported_law:%d",p->AudioBsEncInf->g726_enc_law);
        g726_enc_priv->law = '1';
    }

    g726_enc_priv->sample_proc = MAXDECODESAMPLE;
    g726_enc_priv->g726_enc_bytes = (g726_enc_priv->sample_proc * g726_enc_priv->rate) / 8;
    g726_enc_priv->reset = 1;

    p->EncoderCom->pEncInfoSet = (unsigned int *)g726_enc_priv;

   return 0;
}

static int G726EncExit(struct __AudioENC_AC320 *p)
{ 
   return 0;
} 

static int G726FrameEnc(struct __AudioENC_AC320 *p, char *OutBuffer,int *OutBuffLen)
{
	int	retVal	 = 0;
	int read_sum = 0;
    int nEncodeInSize = 0; 
    int nEncOutSize = 0;
    g726_enc_handle__ *g726_enc_priv = NULL; 
	char *pcm_in_tmp_buff = NULL;

	*OutBuffLen  = 0; 
    g726_enc_priv = (g726_enc_handle__ *)p->EncoderCom->pEncInfoSet;
	pcm_in_tmp_buff = g726_enc_priv->pcm_in_buff;

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
//    alib_loge("zjx_g726_enc");
    read_sum = ReadPcmDataForEnc((void *)pcm_in_tmp_buff, nEncodeInSize, p->pPcmBufManager);

    if(0 == p->Encinitedflag)
    {
        p->Encinitedflag = 1;
        g726_enc_priv->reset = 1; 
        nEncOutSize = g726encode_func(g726_enc_priv,(short *)pcm_in_tmp_buff,MAXDECODESAMPLE,(unsigned char *)OutBuffer);
        g726_enc_priv->reset =  0;
    } 
    else
    { 
        nEncOutSize = g726encode_func(g726_enc_priv,(short *)pcm_in_tmp_buff,MAXDECODESAMPLE,(unsigned char *)OutBuffer);
    }
    
    *OutBuffLen = nEncOutSize;

	p->EncoderCom->framecount++;

	return retVal;
}


struct __AudioENC_AC320 *AudioG726EncInit(void)
{
	struct  __AudioENC_AC320  *AEncAC320 ;

	AEncAC320 = (struct __AudioENC_AC320 *)malloc(sizeof(struct __AudioENC_AC320));
    if(!AEncAC320)
	return 0;

    memset(AEncAC320, 0, sizeof(struct __AudioENC_AC320));

	AEncAC320->EncInit = G726EncInit;
	AEncAC320->EncFrame = G726FrameEnc;
	AEncAC320->EncExit = G726EncExit;

    alib_loge("g726_enc_init");

	return AEncAC320;
}

int	AudioG726EncExit(struct __AudioENC_AC320 *p)
{
    g726_enc_handle__ *g726_enc_priv = NULL; 
	char *pcm_in_tmp_buff = NULL;

    g726_enc_priv = (g726_enc_handle__ *)p->EncoderCom->pEncInfoSet;
	pcm_in_tmp_buff = g726_enc_priv->pcm_in_buff;
    if(NULL != pcm_in_tmp_buff)
    {
        free(pcm_in_tmp_buff);
    }

    if(NULL != p->EncoderCom->pEncInfoSet)
    {
        free(p->EncoderCom->pEncInfoSet);
    }
    
    if(NULL != p)
    {
        free(p);
    }
    alib_loge("g726_enc_exit");
	return 0;
}


