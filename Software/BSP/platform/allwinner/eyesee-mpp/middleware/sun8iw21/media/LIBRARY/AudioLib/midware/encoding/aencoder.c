//#define LOG_NDEBUG 0
#define LOG_TAG "AudioEncApi.c"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "alib_log.h"
#include "aenc_sw_lib.h"
#include "aacencApi.h"
#include "mp3encApi.h"
#include "pcm_enc.h"

#include "aencoder.h"

extern struct __AudioENC_AC320 *AudioG711aEncInit(void);
extern struct __AudioENC_AC320 *AudioG711uEncInit(void);
extern struct __AudioENC_AC320 *AudioG726EncInit(void);
extern int AudioG711aEncExit(struct __AudioENC_AC320 *p);
extern int AudioG711uEncExit(struct __AudioENC_AC320 *p);
extern int AudioG726EncExit(struct __AudioENC_AC320 *p);

typedef struct AudioOutBuf_tag
{
	void * buf;
	int size;
	unsigned int  timeStamp;
}AudioOutBuf_t;

typedef struct OutBufManager_tag
{
	AudioOutBuf_t	*out_buf;   //[FIFO_LEVEL]
    int mBufCnt;    //FIFO_LEVEL
	int				write_id;
	int				read_id;
    int             prefetch_id;
	int				buf_unused;
}OutBufManager_t;

typedef struct audio_Enc_data audio_Enc_data;
struct audio_Enc_data
{
	struct __AudioENC_AC320		*pAudioEnc;
	__pcm_buf_manager_t			PcmManager;
	__pcm_buf_chunk_manager_t	PcmChunkManager;
	__audio_enc_inf_t			AudioBsEncInf;
	__com_internal_prameter_t	EncInternal;
	OutBufManager_t				gOutBufManager;
	pthread_mutex_t				out_buf_lock;
	pthread_mutex_t				in_buf_lock;
	int				encode_type; // 0 aac; 1 LPCM
};

typedef struct AudioEncoderContext
{
	AudioEncoder common;
	void *priv_data;    //audio_Enc_data
	int (*RequestWriteBuf)(AudioEncoder *handle, void * pInbuf, int inSize);
	int (*AudioEncPro)(AudioEncoder *handle);
	int (*GetAudioEncBuf)(AudioEncoder *handle, void ** pOutbuf, unsigned int * outSize, long long * timeStamp, int* pBufId);
    int (*ReleaseAudioEncBuf)(AudioEncoder *handle, void* pOutbuf, unsigned int outSize, long long timeStamp, int nBufId);

    int (*GetValidPcmDataSize)(AudioEncoder *handle);
    int (*GetTotalPcmBufSize)(AudioEncoder *handle);
    int (*GetEmptyFrameNum)(AudioEncoder *handle);
    int (*GetTotalFrameNum)(AudioEncoder *handle);
}AudioEncoderContext;


static int RequestWriteBuf(AudioEncoder *handle, void * pInbuf, int inSize)
{

	AudioEncoderContext *gAEncContent = (AudioEncoderContext *)handle;
	audio_Enc_data * audioEncData = (audio_Enc_data *)gAEncContent->priv_data;
    AENC_PCM_FRM_INFO *p_pcm_frm_info = (AENC_PCM_FRM_INFO *)pInbuf;
    PCM_CHUNK *p_pcm_chunk = NULL;

	pthread_mutex_lock(&audioEncData->in_buf_lock);
	if (audioEncData->PcmManager.uFreeBufSize < inSize)
	{
		pthread_mutex_unlock(&audioEncData->in_buf_lock);
		alib_logw("not enough buffer to write, audioEncData->PcmManager.uFreeBufSize: %d", audioEncData->PcmManager.uFreeBufSize);
		return -1;
	}

	if (audioEncData->PcmChunkManager.chunks_num_usable >= audioEncData->PcmChunkManager.chunks_num-1 )
	{
		pthread_mutex_unlock(&audioEncData->in_buf_lock);
		alib_logw("not enough pcm_chunk to write, audioEncData->PcmChunkManager.chunks_num_usable: %d", audioEncData->PcmChunkManager.chunks_num_usable);
		return -1;
	}

    // fetch one usable chunk
    p_pcm_chunk = &audioEncData->PcmChunkManager.pcm_chunks[audioEncData->PcmChunkManager.chunks_wt];
    memset(p_pcm_chunk,0,sizeof(PCM_CHUNK));
    audioEncData->PcmChunkManager.chunks_num_usable ++;
    audioEncData->PcmChunkManager.chunks_wt ++;
    if(audioEncData->PcmChunkManager.chunks_wt >= audioEncData->PcmChunkManager.chunks_num)
    {
        audioEncData->PcmChunkManager.chunks_wt = 0;
    }

    if(-1 == p_pcm_frm_info->frm_pts)
    {
        if(-1 ==audioEncData->PcmChunkManager.last_chunk_pts)
        {
            p_pcm_chunk->chunk_pts = 0;
        }
        else
        {
            p_pcm_chunk->chunk_pts = audioEncData->PcmChunkManager.last_chunk_pts + 
                inSize/((audioEncData->AudioBsEncInf.SamplerBits/8)*audioEncData->AudioBsEncInf.InChan)*1000*1000/
                audioEncData->AudioBsEncInf.InSamplerate;
        }
    }
    else
    {
            p_pcm_chunk->chunk_pts = p_pcm_frm_info->frm_pts;
    }

    audioEncData->PcmChunkManager.last_chunk_pts = p_pcm_chunk->chunk_pts;

	if ((audioEncData->PcmManager.pBufWritPtr + inSize)
		> (audioEncData->PcmManager.pBufStart + audioEncData->PcmManager.uBufTotalLen))
	{
		int endSize = audioEncData->PcmManager.pBufStart + audioEncData->PcmManager.uBufTotalLen
			- audioEncData->PcmManager.pBufWritPtr;
        if(endSize > 0)
        {
            memcpy(audioEncData->PcmManager.pBufWritPtr, p_pcm_frm_info->p_frm_buff, endSize);
            p_pcm_chunk->p_addr0 = audioEncData->PcmManager.pBufWritPtr;
            p_pcm_chunk->size0 = endSize;

            memcpy(audioEncData->PcmManager.pBufStart, (char*)p_pcm_frm_info->p_frm_buff + endSize, inSize - endSize);
            p_pcm_chunk->p_addr1 = audioEncData->PcmManager.pBufStart;
            p_pcm_chunk->size1 = inSize - endSize;
            audioEncData->PcmManager.pBufWritPtr = audioEncData->PcmManager.pBufStart +(inSize - endSize);
        }
        else
        {
            memcpy(audioEncData->PcmManager.pBufStart, (char*)p_pcm_frm_info->p_frm_buff, inSize);
            p_pcm_chunk->p_addr0 = audioEncData->PcmManager.pBufStart;
            p_pcm_chunk->size0 = inSize;
            audioEncData->PcmManager.pBufWritPtr = audioEncData->PcmManager.pBufStart + inSize;
        }
	}
	else
	{
		memcpy(audioEncData->PcmManager.pBufWritPtr, p_pcm_frm_info->p_frm_buff, inSize);
        p_pcm_chunk->p_addr0 = audioEncData->PcmManager.pBufWritPtr;
        p_pcm_chunk->size0 = inSize;
        p_pcm_chunk->p_addr1 = NULL;
        p_pcm_chunk->size1 = 0;

		audioEncData->PcmManager.pBufWritPtr += inSize;
        if(audioEncData->PcmManager.pBufWritPtr == audioEncData->PcmManager.pBufStart+audioEncData->PcmManager.uBufTotalLen)
        {
            //ALOGD("(f:%s, l:%d) oh my god, we meet a return head, but don't worry.", __FUNCTION__, __LINE__);
            //audioEncData->PcmManager.pBufWritPtr = audioEncData->PcmManager.pBufStart;
        }
	}
    p_pcm_chunk->chunk_data_size = inSize;

	audioEncData->PcmManager.uFreeBufSize -= inSize;
	audioEncData->PcmManager.uDataLen += inSize;

	alib_logv("(f:%s, l:%d) after wr: uBufTotalLen: %d, uDataLen: %d, uFreeBufSize: %d, pBufReadPtr: %p, pBufWritPtr: %p", __FUNCTION__, __LINE__,
		audioEncData->PcmManager.uBufTotalLen, audioEncData->PcmManager.uDataLen, audioEncData->PcmManager.uFreeBufSize,
		audioEncData->PcmManager.pBufReadPtr, audioEncData->PcmManager.pBufWritPtr);

	pthread_mutex_unlock(&audioEncData->in_buf_lock);

	alib_logv("RequestWriteBuf --");

	return 0;
}

static int AudioEncPro(AudioEncoder *handle)
{
	int ret = 0;
	void *pbuf = 0;
	int size = 0;

	AudioEncoderContext *gAEncContent = (AudioEncoderContext *)handle;
	audio_Enc_data * audioEncData = (audio_Enc_data *)gAEncContent->priv_data;

	// write out buf
	pthread_mutex_lock(&audioEncData->out_buf_lock);
	if(audioEncData->gOutBufManager.buf_unused <= 1)
	{
		pthread_mutex_unlock(&audioEncData->out_buf_lock);
		alib_logv("===AudioEncPro: no fifo to write, audioEncData->gOutBufManager.buf_unused: %d\n", audioEncData->gOutBufManager.buf_unused);
		return ERR_AUDIO_ENC_OUTFRAME_UNDERFLOW;
	}
    //ALOGD("(f:%s, l:%d) readyBufCnt[%d]", __FUNCTION__, __LINE__, FIFO_LEVEL-audioEncData->gOutBufManager.buf_unused);
    pthread_mutex_unlock(&audioEncData->out_buf_lock);

	pbuf = audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.write_id].buf;

	if (pbuf == NULL)
	{
		alib_loge("AudioEncPro: error get out buf");
		ret = ERR_AUDIO_ENC_UNKNOWN;
		goto EXIT;
	}

	// now start encoder audio buffer
	ret = audioEncData->pAudioEnc->EncFrame(audioEncData->pAudioEnc, pbuf, &size);

	if (size == 0)
	{
		alib_logv("(f:%s, l:%d) audio not encoder, ret=%d", __FUNCTION__, __LINE__, ret);
		//ret = -1;
		goto EXIT;
	}
	alib_logv("a enc ok: size: %d", size);

	pthread_mutex_lock(&audioEncData->out_buf_lock);
	audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.write_id].size = size;
	audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.write_id].timeStamp = audioEncData->EncInternal.ulNowTimeMS;

	audioEncData->gOutBufManager.buf_unused--;
	audioEncData->gOutBufManager.write_id++;
	audioEncData->gOutBufManager.write_id %= audioEncData->gOutBufManager.mBufCnt;
	pthread_mutex_unlock(&audioEncData->out_buf_lock);

	alib_logv("a wr: buf_unused: %d, read_id: %d, prefetch_id:%d, write_id: %d\n",
		audioEncData->gOutBufManager.buf_unused, audioEncData->gOutBufManager.read_id, audioEncData->gOutBufManager.prefetch_id, audioEncData->gOutBufManager.write_id);
EXIT:
	return ret;
}

static int GetAudioEncBuf(AudioEncoder *handle, void ** pOutbuf, unsigned int * outSize, long long * timeStamp, int* pBufId)
{
	AudioEncoderContext *gAEncContent = (AudioEncoderContext *)handle;
	audio_Enc_data * audioEncData = (audio_Enc_data *)gAEncContent->priv_data;

	pthread_mutex_lock(&audioEncData->out_buf_lock);
	if(audioEncData->gOutBufManager.buf_unused >= audioEncData->gOutBufManager.mBufCnt)
	{
		pthread_mutex_unlock(&audioEncData->out_buf_lock);
		alib_logv("=== GetAudioEncBuf: no valid fifo, buf_unused: %d\n", audioEncData->gOutBufManager.buf_unused);
		return -1;
	}

    if(audioEncData->gOutBufManager.write_id > audioEncData->gOutBufManager.read_id)
    {
        if(audioEncData->gOutBufManager.prefetch_id>=audioEncData->gOutBufManager.read_id
            && audioEncData->gOutBufManager.prefetch_id<=audioEncData->gOutBufManager.write_id)
        {
        }
        else
        {
            alib_loge("(f:%s, l:%d) fatal error! read_id[%d], prefechId[%d], writeId[%d], check code!\n", __FUNCTION__, __LINE__,
                audioEncData->gOutBufManager.read_id, audioEncData->gOutBufManager.prefetch_id, audioEncData->gOutBufManager.write_id);
            pthread_mutex_unlock(&audioEncData->out_buf_lock);
            return -1;
        }
    }
    else if(audioEncData->gOutBufManager.write_id < audioEncData->gOutBufManager.read_id)
    {
        if(audioEncData->gOutBufManager.prefetch_id>=audioEncData->gOutBufManager.read_id
            || audioEncData->gOutBufManager.prefetch_id<=audioEncData->gOutBufManager.write_id)
        {
        }
        else
        {
            alib_loge("(f:%s, l:%d) fatal error! read_id[%d], prefechId[%d], writeId[%d], check code!\n", __FUNCTION__, __LINE__,
                audioEncData->gOutBufManager.read_id, audioEncData->gOutBufManager.prefetch_id, audioEncData->gOutBufManager.write_id);
            pthread_mutex_unlock(&audioEncData->out_buf_lock);
            return -1;
        }
    }
    else
    {
        if(audioEncData->gOutBufManager.buf_unused!=0)
        {
            alib_loge("(f:%s, l:%d) fatal error! read_id[%d], prefechId[%d], writeId[%d], check code!\n", __FUNCTION__, __LINE__,
                audioEncData->gOutBufManager.read_id, audioEncData->gOutBufManager.prefetch_id, audioEncData->gOutBufManager.write_id);
            pthread_mutex_unlock(&audioEncData->out_buf_lock);
            return -1;
        }
    }
	if(audioEncData->gOutBufManager.prefetch_id==audioEncData->gOutBufManager.write_id)
	{
        alib_logv("(f:%s, l:%d) prefechId[%d]==writeId[%d], readId[%d], all outAudioFrames are requested\n", __FUNCTION__, __LINE__,
		    audioEncData->gOutBufManager.prefetch_id, audioEncData->gOutBufManager.write_id, audioEncData->gOutBufManager.read_id);
        pthread_mutex_unlock(&audioEncData->out_buf_lock);
        return -1;
	}
	*pOutbuf	= audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.prefetch_id].buf;
	*outSize	= (unsigned int)(audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.prefetch_id].size);
	*timeStamp	= (long long)(audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.prefetch_id].timeStamp);
    *pBufId = audioEncData->gOutBufManager.prefetch_id;
    alib_logv("(f:%s, l:%d)buf_unused: %d, read_id: %d, prefetch_id: %d, write_id: %d\n", __FUNCTION__, __LINE__,
		audioEncData->gOutBufManager.buf_unused, audioEncData->gOutBufManager.read_id, audioEncData->gOutBufManager.prefetch_id, audioEncData->gOutBufManager.write_id);
    audioEncData->gOutBufManager.prefetch_id++;
    audioEncData->gOutBufManager.prefetch_id %= audioEncData->gOutBufManager.mBufCnt;
	pthread_mutex_unlock(&audioEncData->out_buf_lock);
	return 0;
}

static int ReleaseAudioEncBuf(AudioEncoder *handle, void* pOutbuf, unsigned int outSize, long long timeStamp, int nBufId)
{
	AudioEncoderContext *gAEncContent = (AudioEncoderContext *)handle;
	audio_Enc_data * audioEncData = (audio_Enc_data *)gAEncContent->priv_data;

	pthread_mutex_lock(&audioEncData->out_buf_lock);
	if(audioEncData->gOutBufManager.buf_unused == audioEncData->gOutBufManager.mBufCnt)
	{
		pthread_mutex_unlock(&audioEncData->out_buf_lock);
		alib_loge("(f:%s, l:%d) fatal error! AudioEncPro: no valid fifo\n", __FUNCTION__, __LINE__);
		return -1;
	}
    if(nBufId != audioEncData->gOutBufManager.read_id)
    {
        pthread_mutex_unlock(&audioEncData->out_buf_lock);
        alib_loge("(f:%s, l:%d) fatal error! nReleaseId[%d]!=readId[%d]\n", __FUNCTION__, __LINE__, nBufId, audioEncData->gOutBufManager.read_id);
		return -1;
    }

    //verify.
    if(pOutbuf != audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.read_id].buf
        || outSize != (unsigned int)(audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.read_id].size)
	    //|| timeStamp != (long long)(audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.read_id].timeStamp)
	  )
    {
        alib_loge("(f:%s, l:%d) fatal error, check code!buf[%p->%p]size[%d->%d]pts[%lld->%lld]", __FUNCTION__, __LINE__,
            pOutbuf, audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.read_id].buf,
            outSize, (unsigned int)(audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.read_id].size),
            timeStamp, (long long)(audioEncData->gOutBufManager.out_buf[audioEncData->gOutBufManager.read_id].timeStamp));
    }
	audioEncData->gOutBufManager.buf_unused++;
	audioEncData->gOutBufManager.read_id++;
	audioEncData->gOutBufManager.read_id %= audioEncData->gOutBufManager.mBufCnt;
	alib_logv("(f:%s, l:%d) a rd: buf_unused: %d, read_id: %d, write_id: %d\n", __FUNCTION__, __LINE__,
		audioEncData->gOutBufManager.buf_unused, audioEncData->gOutBufManager.read_id, audioEncData->gOutBufManager.write_id);
	pthread_mutex_unlock(&audioEncData->out_buf_lock);
	return 0;
}

static int GetValidPcmDataSize(AudioEncoder *handle)
{
    AudioEncoderContext *pAEncContent = (AudioEncoderContext *)handle;
    audio_Enc_data * audioEncData = (audio_Enc_data *)pAEncContent->priv_data;
    int dataSize;
	pthread_mutex_lock(&audioEncData->in_buf_lock);
    dataSize = audioEncData->PcmManager.uDataLen;
	pthread_mutex_unlock(&audioEncData->in_buf_lock);
    return dataSize;
}

static int GetTotalPcmBufSize(AudioEncoder *handle)
{
    AudioEncoderContext *pAEncContent = (AudioEncoderContext *)handle;
    audio_Enc_data * audioEncData = (audio_Enc_data *)pAEncContent->priv_data;
    return audioEncData->PcmManager.uBufTotalLen;
}

static int GetEmptyFrameNum(AudioEncoder *handle)
{
    AudioEncoderContext *pAEncContent = (AudioEncoderContext *)handle;
    audio_Enc_data * audioEncData = (audio_Enc_data *)pAEncContent->priv_data;
    int emptyNum;
    pthread_mutex_lock(&audioEncData->out_buf_lock);
	emptyNum = audioEncData->gOutBufManager.buf_unused;
    pthread_mutex_unlock(&audioEncData->out_buf_lock);
    return emptyNum;
}

static int GetTotalFrameNum(AudioEncoder *handle)
{
    if(handle)
    {
        audio_Enc_data * audioEncData = (audio_Enc_data *)((AudioEncoderContext *)handle)->priv_data;
        return audioEncData->gOutBufManager.mBufCnt;
    }
    else
    {
        alib_loge("fatal error! NULL ptr!");
        return 0;
    }
}

/*******************************************************/
// called by audio encoder
int GetPcmDataSize(__pcm_buf_manager_t *pPcmMan)
{
	audio_Enc_data * audioEncData = (audio_Enc_data *)pPcmMan->parent;

	alib_logd("GetPcmDataSize : %d", audioEncData->PcmManager.uDataLen);
    return audioEncData->PcmManager.uDataLen;
}

int ReadPcmDataForEnc(void *pBuf, int uGetLen, __pcm_buf_manager_t *pPcmMan)
{
	audio_Enc_data * audioEncData = (audio_Enc_data *)pPcmMan->parent;
    PCM_CHUNK *p_pcm_chunk = NULL;
    int uGetLen_bkp = uGetLen;
	alib_logv("ReadPcmDataForEnc ++, getLen: %d", uGetLen);
    
    pthread_mutex_lock(&audioEncData->in_buf_lock);

    if(audioEncData->PcmManager.uDataLen < uGetLen)
    {
        alib_logw("pcm is not enough for audio encoder! uGetLen: %d, uDataLen: %d\n",
			uGetLen, audioEncData->PcmManager.uDataLen);
        pthread_mutex_unlock(&audioEncData->in_buf_lock);
        return 0;
    }

    if(audioEncData->PcmChunkManager.chunks_num_usable == 0)
    {
        alib_logw("aencoder_rd_pcm_no_chk:%d-%d-%d",audioEncData->PcmChunkManager.chunks_rd,audioEncData->PcmChunkManager.chunks_wt,audioEncData->PcmManager.uDataLen);
        pthread_mutex_unlock(&audioEncData->in_buf_lock);
        return 0;
    }

    p_pcm_chunk = &audioEncData->PcmChunkManager.pcm_chunks[audioEncData->PcmChunkManager.chunks_rd];
    if(uGetLen ==  p_pcm_chunk->chunk_data_size)
    {
        if(p_pcm_chunk->p_addr0 && p_pcm_chunk->size0)
        {
            memcpy(pBuf,p_pcm_chunk->p_addr0,p_pcm_chunk->size0);
            uGetLen -= p_pcm_chunk->size0;
            p_pcm_chunk->size0 = 0;
            p_pcm_chunk->p_addr0 = NULL;
        }
        if(p_pcm_chunk->p_addr1 && p_pcm_chunk->size1)
        {
            memcpy((char *)pBuf+p_pcm_chunk->size0,p_pcm_chunk->p_addr1,p_pcm_chunk->size1);
            uGetLen -= p_pcm_chunk->size1;
            p_pcm_chunk->size1 = 0;
            p_pcm_chunk->p_addr1 = NULL;
        }
        if(uGetLen)
        {
            alib_loge("fatal error, aencoder read pcm,%d-%d",uGetLen,p_pcm_chunk->chunk_data_size);
        }
        
        p_pcm_chunk->chunk_data_size = 0;
        audioEncData->EncInternal.ulNowTimeMS = p_pcm_chunk->chunk_pts/1000;  // us -->ms
        audioEncData->PcmChunkManager.chunks_num_usable --;
        audioEncData->PcmChunkManager.chunks_rd ++;
        if(audioEncData->PcmChunkManager.chunks_rd >= audioEncData->PcmChunkManager.chunks_num)
        {
            audioEncData->PcmChunkManager.chunks_rd = 0;
        }
    }
    else if(uGetLen < p_pcm_chunk->chunk_data_size)
    {
        audioEncData->EncInternal.ulNowTimeMS = p_pcm_chunk->chunk_pts;
        p_pcm_chunk->chunk_pts += uGetLen/(audioEncData->AudioBsEncInf.SamplerBits/8*audioEncData->AudioBsEncInf.InChan)
                                                                    *1000*1000/audioEncData->AudioBsEncInf.InSamplerate;

        if(p_pcm_chunk->p_addr0 && p_pcm_chunk->size0 && uGetLen <= p_pcm_chunk->size0)
        {
            memcpy(pBuf,p_pcm_chunk->p_addr0,uGetLen);
            p_pcm_chunk->p_addr0 += uGetLen;// note: when p_addr0 is not null, the size0 maybe zero
            p_pcm_chunk->size0 -= uGetLen;
        }
        else
        {
            int tmp_left_size = uGetLen - p_pcm_chunk->size0; // just for check
            if(p_pcm_chunk->p_addr1 && p_pcm_chunk->size1 && tmp_left_size <= p_pcm_chunk->size1)
            {
                if(p_pcm_chunk->p_addr0 && p_pcm_chunk->size0)  // first copy part1 data
                {
                    memcpy(pBuf,p_pcm_chunk->p_addr0,p_pcm_chunk->size0);  //read first part 
                    p_pcm_chunk->p_addr0 = NULL;
                    p_pcm_chunk->size0 = 0;
                }

                memcpy((char *)pBuf+p_pcm_chunk->size0,p_pcm_chunk->p_addr1,tmp_left_size); // then read second part
                p_pcm_chunk->p_addr1 += tmp_left_size;
                p_pcm_chunk->size1 -= tmp_left_size;
            }
            else
            {
                alib_loge("fatal error when read pcm data for encoder,%d-%d-%d",uGetLen,p_pcm_chunk->size0,p_pcm_chunk->size1);
            }
        }
        p_pcm_chunk->chunk_data_size -= uGetLen;
    }
    else if(uGetLen > p_pcm_chunk->chunk_data_size)
    {
        char *tmp_buff = (char *)pBuf;
        audioEncData->EncInternal.ulNowTimeMS = p_pcm_chunk->chunk_pts;
        while(uGetLen > 0)
        {
            if(uGetLen >= p_pcm_chunk->chunk_data_size)
            {
                if(p_pcm_chunk->p_addr0 && p_pcm_chunk->size0)
                {
                    memcpy(tmp_buff,p_pcm_chunk->p_addr0,p_pcm_chunk->size0);
                    tmp_buff += p_pcm_chunk->size0;
                    uGetLen -= p_pcm_chunk->size0;
                    p_pcm_chunk->p_addr0 = NULL;
                    p_pcm_chunk->size0 = 0;
                }
                if(p_pcm_chunk->p_addr1 && p_pcm_chunk->size1)
                {
                    memcpy(tmp_buff,p_pcm_chunk->p_addr1,p_pcm_chunk->size1);
                    tmp_buff += p_pcm_chunk->size1;
                    uGetLen -= p_pcm_chunk->size1;
                    p_pcm_chunk->p_addr1 = NULL;
                    p_pcm_chunk->size1 = 0;
                }
                p_pcm_chunk->chunk_data_size = 0;
                audioEncData->PcmChunkManager.chunks_num_usable --;
                if(!(audioEncData->PcmChunkManager.chunks_num_usable>0) && uGetLen)
                {
                    alib_loge("fatal error,check code,%d-%d",uGetLen,audioEncData->PcmChunkManager.chunks_num_usable);
                }
                audioEncData->PcmChunkManager.chunks_rd++;
                if(audioEncData->PcmChunkManager.chunks_rd >= audioEncData->PcmChunkManager.chunks_num)
                {
                    audioEncData->PcmChunkManager.chunks_rd = 0;
                }
                p_pcm_chunk = &audioEncData->PcmChunkManager.pcm_chunks[audioEncData->PcmChunkManager.chunks_rd];
            }
            else
            {
                p_pcm_chunk->chunk_pts += uGetLen/
                    (audioEncData->AudioBsEncInf.SamplerBits/8*audioEncData->AudioBsEncInf.InChan)
                    *1000*1000/audioEncData->AudioBsEncInf.InSamplerate;

                if(p_pcm_chunk->p_addr0 && p_pcm_chunk->size0 && uGetLen <=p_pcm_chunk->size0)
                {
                    memcpy(tmp_buff,p_pcm_chunk->p_addr0,uGetLen);
                    tmp_buff += uGetLen;
                    p_pcm_chunk->p_addr0 += uGetLen;
                    p_pcm_chunk->size0 -= uGetLen;
                }
                else
                {
                    int tmp_left_size = uGetLen - p_pcm_chunk->size0;
                    if(p_pcm_chunk->p_addr1 && p_pcm_chunk->size1 && tmp_left_size <= p_pcm_chunk->size1)
                    {
                        if(p_pcm_chunk->p_addr0 && p_pcm_chunk->size0)
                        {
                            memcpy(tmp_buff,p_pcm_chunk->p_addr0,p_pcm_chunk->size0);  //read first part 
                            tmp_buff += p_pcm_chunk->size0;
                            p_pcm_chunk->p_addr0 = NULL;
                            p_pcm_chunk->size0 = 0;
                        }

                        memcpy(tmp_buff,p_pcm_chunk->p_addr1,tmp_left_size);
                        p_pcm_chunk->p_addr1 += tmp_left_size;
                        p_pcm_chunk->size1 -= tmp_left_size;
                    }
                    else
                    {
                        alib_loge("fatal error when read pcm data for encoder,%d-%d",uGetLen,p_pcm_chunk->size1);
                    }
                }
                p_pcm_chunk->chunk_data_size -= uGetLen;
                uGetLen = 0;
            }
        }
    }

    audioEncData->PcmManager.pBufReadPtr += uGetLen_bkp;
    if(audioEncData->PcmManager.pBufReadPtr
            >= audioEncData->PcmManager.pBufStart + audioEncData->PcmManager.uBufTotalLen)
    {
        audioEncData->PcmManager.pBufReadPtr -= audioEncData->PcmManager.uBufTotalLen;
    }
    audioEncData->PcmManager.uDataLen -= uGetLen_bkp;
    audioEncData->PcmManager.uFreeBufSize += uGetLen_bkp;

    if(audioEncData->PcmChunkManager.chunks_num_usable > 0)  // chk buffer address
    {
        p_pcm_chunk = &audioEncData->PcmChunkManager.pcm_chunks[audioEncData->PcmChunkManager.chunks_rd];

        if(p_pcm_chunk->p_addr0&&p_pcm_chunk->size0)
        {
            if(audioEncData->PcmManager.pBufReadPtr != p_pcm_chunk->p_addr0)
            {
                alib_logw("be carefule,check aencoder pcm read,%p-%p-%d-%d",audioEncData->PcmManager.pBufReadPtr,
                        p_pcm_chunk->p_addr0,audioEncData->PcmChunkManager.chunks_rd,audioEncData->PcmChunkManager.chunks_num_usable);
            }
        }
        else if(p_pcm_chunk->p_addr1&&p_pcm_chunk->size1)
        {
            if(audioEncData->PcmManager.pBufReadPtr != p_pcm_chunk->p_addr1)
            {
                alib_logw("be carefule,check aencoder pcm read,%p-%p-%d-%d",audioEncData->PcmManager.pBufReadPtr,
                        p_pcm_chunk->p_addr1,audioEncData->PcmChunkManager.chunks_rd,audioEncData->PcmChunkManager.chunks_num_usable);
            }
        }
    }

	alib_logv("after rd: uBufTotalLen: %d, uDataLen: %d, uFreeBufSize: %d, pBufReadPtr: %p, pBufWritPtr: %p",
		audioEncData->PcmManager.uBufTotalLen, audioEncData->PcmManager.uDataLen, audioEncData->PcmManager.uFreeBufSize,
		audioEncData->PcmManager.pBufReadPtr, audioEncData->PcmManager.pBufWritPtr);

	pthread_mutex_unlock(&audioEncData->in_buf_lock);
	alib_logv("ReadPcmDataForEnc --");

    return uGetLen_bkp;
}
/*************************************************************************/

static int AudioEncInit(AudioEncoder *pEncoder, __audio_enc_inf_t * audio_inf, int encode_type, int nInBufSize, int nOutBufCnt)
{
	int i = 0;
	audio_Enc_data *audioEncData = NULL;

	if(encode_type != AUDIO_ENCODER_AAC_TYPE &&
       encode_type != AUDIO_ENCODER_PCM_TYPE &&
       encode_type != AUDIO_ENCODER_LPCM_TYPE &&
       encode_type != AUDIO_ENCODER_MP3_TYPE &&
       encode_type != AUDIO_ENCODER_G711A_TYPE &&
       encode_type != AUDIO_ENCODER_G711U_TYPE &&
       encode_type != AUDIO_ENCODER_G726A_TYPE
       && encode_type != AUDIO_ENCODER_G726U_TYPE)
	{
		alib_loge("(f:%s, l:%d) not support audio encode type[%d]", __FUNCTION__, __LINE__, encode_type);
		return -1;
	}
	//init audio encode content
	AudioEncoderContext	*gAEncContent = (AudioEncoderContext *)pEncoder;

	audioEncData = (audio_Enc_data *)malloc(sizeof(audio_Enc_data));
	if(audioEncData == NULL)
	{
		alib_loge("malloc audioEncData fail");
		return -1;
	}

	memset((void *)audioEncData, 0 , sizeof(audio_Enc_data));
	gAEncContent->priv_data = (void *)audioEncData;

    audioEncData->gOutBufManager.mBufCnt = nOutBufCnt>0?nOutBufCnt:FIFO_LEVEL;
    audioEncData->gOutBufManager.buf_unused = audioEncData->gOutBufManager.mBufCnt;
    audioEncData->gOutBufManager.out_buf = malloc(sizeof(AudioOutBuf_t)*audioEncData->gOutBufManager.mBufCnt);
    if(NULL == audioEncData->gOutBufManager.out_buf)
    {
        alib_loge("fatal error! malloc fail!");
        goto _err0;
    }
	for (i = 0; i < audioEncData->gOutBufManager.mBufCnt; i++)
	{
		audioEncData->gOutBufManager.out_buf[i].buf = (void *)malloc(OUT_ENCODE_BUFFER_SIZE);
		if (audioEncData->gOutBufManager.out_buf[i].buf == NULL)
		{
			alib_loge("AudioEncInit: malloc out buffer failed");
			//return -1;
			goto _err1;
		}
	}

    audioEncData->PcmManager.uBufTotalLen = nInBufSize>0?nInBufSize:BS_BUFFER_SIZE;
	audioEncData->PcmManager.pBufStart = (unsigned char *)malloc(audioEncData->PcmManager.uBufTotalLen);
	if (audioEncData->PcmManager.pBufStart == NULL)
	{
		alib_loge("AudioEncInit: malloc PcmManager failed");
		//return -1;
		goto _err1;
	}

	audioEncData->PcmManager.pBufWritPtr		= audioEncData->PcmManager.pBufStart;
	audioEncData->PcmManager.pBufReadPtr		= audioEncData->PcmManager.pBufStart;
	audioEncData->PcmManager.uFreeBufSize	= audioEncData->PcmManager.uBufTotalLen;
	audioEncData->PcmManager.uDataLen		= 0;
	audioEncData->PcmManager.parent				= (void *)audioEncData;

    memset((char *)audioEncData->PcmChunkManager.pcm_chunks,0,sizeof(audioEncData->PcmChunkManager.pcm_chunks));
    audioEncData->PcmChunkManager.chunks_num = PCM_CHUNK_NUM;
    audioEncData->PcmChunkManager.chunks_rd  = 0;
    audioEncData->PcmChunkManager.chunks_wt = 0;
    audioEncData->PcmChunkManager.chunks_num_usable = 0;
    audioEncData->PcmChunkManager.last_chunk_pts = -1; // -1: invalid pts


	// set audio encode information for audio encode lib
	memcpy((void *)&audioEncData->AudioBsEncInf, (void *)audio_inf, sizeof(__audio_enc_inf_t));

	// get __AudioENC_AC320
    if(encode_type == AUDIO_ENCODER_AAC_TYPE)
    {
      #if (defined(MPPCFG_AENC_AAC) && 1==MPPCFG_AENC_AAC)
        alib_logv("++++++++++++ encode aac encode ++++++++++++");
        audioEncData->pAudioEnc = AudioAACENCEncInit();
      #else
        alib_loge("fatal error! don't support aenc aac!");
      #endif
    }
  #if (defined(MPPCFG_AENC_PCM) && 1==MPPCFG_AENC_PCM)
    else if(encode_type == AUDIO_ENCODER_PCM_TYPE || encode_type == AUDIO_ENCODER_LPCM_TYPE)
    {
        alib_logv("(f:%s, l:%d) encode pcm[%d]", __FUNCTION__, __LINE__, encode_type);
        audioEncData->pAudioEnc = AudioPCMEncInit();
    }
  #endif
  #if (defined(MPPCFG_AENC_MP3) && 1==MPPCFG_AENC_MP3)
    else if(encode_type == AUDIO_ENCODER_MP3_TYPE)
    {
        alib_logv("(f:%s, l:%d) encode[%d]", __FUNCTION__, __LINE__, encode_type);
        audioEncData->pAudioEnc = AudioMP3ENCEncInit();
    }
  #endif
  #if (defined(MPPCFG_AENC_G711) && 1==MPPCFG_AENC_G711)
    else if(encode_type == AUDIO_ENCODER_G711A_TYPE)
    {
        audioEncData->pAudioEnc = AudioG711aEncInit();
    }
    else if(encode_type == AUDIO_ENCODER_G711U_TYPE)
    {
        audioEncData->pAudioEnc = AudioG711uEncInit();
    }
  #endif
  #if (defined(MPPCFG_AENC_G726) && 1==MPPCFG_AENC_G726)
    else if(encode_type == AUDIO_ENCODER_G726A_TYPE || encode_type == AUDIO_ENCODER_G726U_TYPE)
    {
        audioEncData->pAudioEnc = AudioG726EncInit();
    }
  #endif
    else
    {
        alib_loge("(f:%s, l:%d) not support other audio encode type[%d]", __FUNCTION__, __LINE__, encode_type);
        //return -1;
    }

	if (audioEncData->pAudioEnc == NULL)
	{
		alib_loge("AudioEncInit: EncInit failed");
		//return -1;
		goto _err2;
	}

    audioEncData->encode_type = encode_type;

	audioEncData->pAudioEnc->pPcmBufManager	= &(audioEncData->PcmManager);
	audioEncData->pAudioEnc->AudioBsEncInf	= &(audioEncData->AudioBsEncInf);
	audioEncData->pAudioEnc->EncoderCom		= &(audioEncData->EncInternal);
	audioEncData->pAudioEnc->EncInit(audioEncData->pAudioEnc);

	gAEncContent->AudioEncPro		  = AudioEncPro;
	gAEncContent->RequestWriteBuf	  = RequestWriteBuf;
	gAEncContent->GetAudioEncBuf	  = GetAudioEncBuf;
    gAEncContent->ReleaseAudioEncBuf  = ReleaseAudioEncBuf;
    gAEncContent->GetValidPcmDataSize = GetValidPcmDataSize;
    gAEncContent->GetTotalPcmBufSize  = GetTotalPcmBufSize;
    gAEncContent->GetEmptyFrameNum    = GetEmptyFrameNum;
    gAEncContent->GetTotalFrameNum    = GetTotalFrameNum;

	pthread_mutex_init(&audioEncData->in_buf_lock,NULL);
	pthread_mutex_init(&audioEncData->out_buf_lock,NULL);

	return 0;

_err2:
    if (audioEncData->PcmManager.pBufStart != NULL)
    {
        free(audioEncData->PcmManager.pBufStart);
        audioEncData->PcmManager.pBufStart = NULL;
    }
_err1:
    for (i = 0; i < audioEncData->gOutBufManager.mBufCnt; i++)
    {
        if (audioEncData->gOutBufManager.out_buf[i].buf != NULL)
        {
            free(audioEncData->gOutBufManager.out_buf[i].buf);
            audioEncData->gOutBufManager.out_buf[i].buf = NULL;
        }
    }
    if(audioEncData->gOutBufManager.out_buf != NULL)
    {
        free(audioEncData->gOutBufManager.out_buf);
        audioEncData->gOutBufManager.out_buf = NULL;
    }
_err0:
    if(audioEncData)
    {
        free(audioEncData);
        audioEncData = NULL;
    }
    return -1;
}

static void AudioEncExit(void *handle)
{
	int i = 0;
	AudioEncoderContext *gAEncContent = NULL;
	audio_Enc_data * audioEncData = NULL;
	
	gAEncContent = (AudioEncoderContext *)handle;
	if(gAEncContent == NULL)
	{
		alib_loge("gAEncContent == NULL");
		return;
	}
	else
	{	
		audioEncData = (audio_Enc_data *)gAEncContent->priv_data;
	}

	for (i = 0; i < audioEncData->gOutBufManager.mBufCnt; i++)
	{
		if (audioEncData->gOutBufManager.out_buf[i].buf != NULL)
		{
			free(audioEncData->gOutBufManager.out_buf[i].buf);
			audioEncData->gOutBufManager.out_buf[i].buf = NULL;
		}
	}
    if(audioEncData->gOutBufManager.out_buf != NULL)
    {
        free(audioEncData->gOutBufManager.out_buf);
        audioEncData->gOutBufManager.out_buf = NULL;
    }
	if (audioEncData->PcmManager.pBufStart != NULL)
	{
		free(audioEncData->PcmManager.pBufStart);
		audioEncData->PcmManager.pBufStart = NULL;
	}
    if(audioEncData->PcmChunkManager.chunks_num_usable > 0)
    {
        alib_loge("AudioEnc still has [%d]chunks to wait to encode. [%d,%d]", audioEncData->PcmChunkManager.chunks_num_usable, 
            audioEncData->PcmChunkManager.chunks_rd, audioEncData->PcmChunkManager.chunks_wt);
    }
	if (audioEncData->pAudioEnc != NULL)
	{
		audioEncData->pAudioEnc->EncExit(audioEncData->pAudioEnc);

        if(audioEncData->encode_type == AUDIO_ENCODER_AAC_TYPE)
        {
          #if (defined(MPPCFG_AENC_AAC) && 1==MPPCFG_AENC_AAC)
            AudioAACENCEncExit(audioEncData->pAudioEnc);
          #else
            alib_loge("fatal error! don't support aenc aac!");
          #endif
        }
      #if (defined(MPPCFG_AENC_MP3) && 1==MPPCFG_AENC_MP3)
        else if(audioEncData->encode_type == AUDIO_ENCODER_MP3_TYPE)
        {
            AudioMP3ENCEncExit(audioEncData->pAudioEnc);
        }
      #endif
      #if (defined(MPPCFG_AENC_G711) && 1==MPPCFG_AENC_G711)
        else if(audioEncData->encode_type == AUDIO_ENCODER_G711A_TYPE)
        {
            AudioG711aEncExit(audioEncData->pAudioEnc);
        }
        else if(audioEncData->encode_type == AUDIO_ENCODER_G711U_TYPE)
        {
            AudioG711uEncExit(audioEncData->pAudioEnc);
        }
      #endif
      #if (defined(MPPCFG_AENC_G726) && 1==MPPCFG_AENC_G726)
        else if(audioEncData->encode_type == AUDIO_ENCODER_G726A_TYPE || audioEncData->encode_type == AUDIO_ENCODER_G726U_TYPE)
        {
            AudioG726EncExit(audioEncData->pAudioEnc);
        }
      #endif
      #if (defined(MPPCFG_AENC_PCM) && 1==MPPCFG_AENC_PCM)
        else if(audioEncData->encode_type == AUDIO_ENCODER_PCM_TYPE || audioEncData->encode_type == AUDIO_ENCODER_LPCM_TYPE)
        {
            AudioPCMEncExit(audioEncData->pAudioEnc);
        }
      #endif
        else
        {
            alib_loge("(f:%s, l:%d) not support other audio encode type[%d]", __FUNCTION__, __LINE__, audioEncData->encode_type);
        }

		audioEncData->pAudioEnc = NULL;
	}

	pthread_mutex_destroy(&audioEncData->in_buf_lock);
	pthread_mutex_destroy(&audioEncData->out_buf_lock);

	if(audioEncData)
	{
		free(audioEncData);
		audioEncData = NULL;
	}

	if(gAEncContent)
	{
		free(gAEncContent);
		gAEncContent = NULL;
	}

	alib_logv("AudioEncExit --");
}


AudioEncoder* CreateAudioEncoder()
{
	AudioEncoderContext *pAudioEncoder = (AudioEncoderContext*)malloc(sizeof(AudioEncoderContext));
	if(NULL == pAudioEncoder)
	{
		alib_loge("create audio encoder failed");
		return NULL;
	}
	memset(pAudioEncoder, 0x00, sizeof(AudioEncoderContext));

	return (AudioEncoder*)pAudioEncoder;
}

int InitializeAudioEncoder(AudioEncoder *pEncoder, AudioEncConfig *pConfig)
{
	AudioEncoderContext *p;

	p = (AudioEncoderContext*)pEncoder;

	if(pConfig->nType != AUDIO_ENCODER_AAC_TYPE &&
	   pConfig->nType != AUDIO_ENCODER_PCM_TYPE &&
	   pConfig->nType != AUDIO_ENCODER_MP3_TYPE &&
	   pConfig->nType != AUDIO_ENCODER_G711A_TYPE &&
	   pConfig->nType != AUDIO_ENCODER_G711U_TYPE &&
	   pConfig->nType != AUDIO_ENCODER_G726A_TYPE
	   && pConfig->nType != AUDIO_ENCODER_G726U_TYPE)
	{
		alib_loge("cannot support sudio encode type(%d)", pConfig->nType);
		return -1;
	}

	__audio_enc_inf_t audioInfo;
	audioInfo.bitrate	= pConfig->nBitrate;
	audioInfo.frame_style	= pConfig->nFrameStyle;
	audioInfo.InChan	= pConfig->nInChan;
	audioInfo.InSamplerate	= pConfig->nInSamplerate;
	audioInfo.OutChan		= pConfig->nOutChan;
	audioInfo.OutSamplerate = pConfig->nOutSamplerate;
	audioInfo.SamplerBits	= pConfig->nSamplerBits;
    if( pConfig->nType == AUDIO_ENCODER_G726A_TYPE)
    {
        audioInfo.g726_enc_law = 1;
        //audioInfo.target_bit_rate = pConfig->nBitrate;
    }
    else if(pConfig->nType == AUDIO_ENCODER_G726U_TYPE)
    {
        audioInfo.g726_enc_law = 0;
    }

	return AudioEncInit(pEncoder, &audioInfo, pConfig->nType, pConfig->mInBufSize, pConfig->mOutBufCnt);
}

int ResetAudioEncoder(AudioEncoder* pEncoder)
{
	AudioEncoderContext *p;

	p = (AudioEncoderContext*)pEncoder;
	AudioEncExit(pEncoder);

	return 0;
}

void DestroyAudioEncoder(AudioEncoder* pEncoder)
{
	AudioEncoderContext *pAudioEncCtx;

	pAudioEncCtx = (AudioEncoderContext*)pEncoder;
	AudioEncExit(pEncoder);

}

int EncodeAudioStream(AudioEncoder *pEncoder)
{
	AudioEncoderContext *pAudioEncCtx;

	pAudioEncCtx = (AudioEncoderContext*)pEncoder;

	return pAudioEncCtx->AudioEncPro(pEncoder);
}

int WriteAudioStreamBuffer(AudioEncoder *pEncoder, char* pBuf, int len)
{
	AudioEncoderContext *pAudioEncCtx;
	pAudioEncCtx = (AudioEncoderContext*)pEncoder;

	return pAudioEncCtx->RequestWriteBuf(pEncoder, pBuf, len);
}

int RequestAudioFrameBuffer(AudioEncoder *pEncoder, char **pOutBuf, unsigned int *size, long long *pts, int *bufId)
{
	AudioEncoderContext *pAudioEncCtx;

	pAudioEncCtx = (AudioEncoderContext*)pEncoder;
	return pAudioEncCtx->GetAudioEncBuf(pEncoder, (void**)pOutBuf, size, pts, bufId);
}

int ReturnAudioFrameBuffer(AudioEncoder *pEncoder, char *pOutBuf, unsigned int size, long long pts, int bufId)
{
	AudioEncoderContext *pAudioEncCtx;

	pAudioEncCtx = (AudioEncoderContext*)pEncoder;
	return pAudioEncCtx->ReleaseAudioEncBuf(pEncoder, pOutBuf, size, pts, bufId);
}

int AudioEncoder_GetValidPcmDataSize(AudioEncoder *pEncoder)
{
    AudioEncoderContext *pAudioEncCtx = (AudioEncoderContext*)pEncoder;
	return pAudioEncCtx->GetValidPcmDataSize(pEncoder);
}

int AudioEncoder_GetTotalPcmBufSize(AudioEncoder *pEncoder)
{
    AudioEncoderContext *pAudioEncCtx = (AudioEncoderContext*)pEncoder;
	return pAudioEncCtx->GetTotalPcmBufSize(pEncoder);
}

int AudioEncoder_GetEmptyFrameNum(AudioEncoder *pEncoder)
{
    AudioEncoderContext *pAudioEncCtx = (AudioEncoderContext*)pEncoder;
	return pAudioEncCtx->GetEmptyFrameNum(pEncoder);
}

int AudioEncoder_GetTotalFrameNum(AudioEncoder *pEncoder)
{
    AudioEncoderContext *pAudioEncCtx = (AudioEncoderContext*)pEncoder;
	return pAudioEncCtx->GetTotalFrameNum(pEncoder);
}

