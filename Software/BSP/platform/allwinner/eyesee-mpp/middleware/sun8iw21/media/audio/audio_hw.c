/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : audio_hw.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/25
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "audio_hw"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <memory.h>

#include <SystemBase.h>
#include <media_common_aio.h>
#include <audio_hw.h>
#include <aec_lib.h> 

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
#define PlaybackRateDmix_SampleRate (16000) //ref to pcm.PlaybackRateDmix of asound.conf
#define PlaybackRateDmix_ChnCnt     (1)
#endif

#if (MPPCFG_ANS == OPTION_ANS_ENABLE)
#include <ans_lib.h>
#endif

#if (MPPCFG_AI_AGC == OPTION_AGC_ENABLE)
#include <agc_m.h>
#endif

#include <ConfigOption.h>

#include "cdx_list.h" 

#if (MPPCFG_ANS == OPTION_ANS_ENABLE)
extern void aw_WebRtcSpl_AnalysisQMF(const int16_t* in_data,
                           int16_t* low_band,
                           int16_t* high_band,
                           int32_t* filter_state1,
                           int32_t* filter_state2);
extern void aw_WebRtcSpl_SynthesisQMF(const int16_t* low_band,
                            const int16_t* high_band,
                            int16_t* out_data,
                            int32_t* filter_state1,
                            int32_t* filter_state2);
#endif

//#define SOUND_CARD  "default:CARD=audiocodec"

// #define SOUND_CARD_AUDIOCODEC   "default"
#define SOUND_CARD_AUDIOCODEC_PLAY   "plug:PlaybackRateDmix"
#define SOUND_CARD_AUDIOCODEC_CAP   "CaptureMic"    //"hw:0,0"
#define SOUND_CARD_SNDHDMI      "hw:1,0"

#define SOUND_MIXER_AUDIOCODEC   "hw:0"
#define SOUND_MIXER_SNDDAUDIO0   "hw:1"

#define SOUND_CARD_SNDDAUDIo0   "hw:1,0"

//#define AI_HW_AEC_DEBUG_EN

typedef enum AI_STATES_E
{
    AI_STATE_INVALID = 0,
    AI_STATE_CONFIGURED,
    AI_STATE_STARTED,
} AI_STATES_E;

typedef enum AO_STATES_E
{
    AO_STATE_INVALID = 0,
    AO_STATE_CONFIGURED,
    AO_STATE_STARTED,
} AO_STATES_E;


typedef struct AudioInputDevice
{
    AI_STATES_E mState;

    AIO_ATTR_S mAttr;
    PCM_CONFIG_S mCfg;
    AUDIO_TRACK_MODE_E mTrackMode;
    pthread_t mThdId;
    pthread_t mThdLoopId;
    volatile BOOL mThdRunning;

    struct list_head mChnList;
    pthread_mutex_t mChnListLock; 
    pthread_mutex_t mApiCallLock;   // to protect the api call,when used in two thread asynchronously.


    short *tmpBuf;
    int tmpBufLen;

    void *aecmInst;
    short *near_buff;                       // buffer used as internal buffer to store near data for conjunction.
    unsigned int near_buff_len;             // the length of the near buffer, normally is 2 x chunkbytesize.
    unsigned int near_buff_data_remain_len; // the length of the valid data that stored in near buffer.
    short *ref_buff;                        // buffer used as internal buffer to store reference data for conjunction.
    unsigned int ref_buff_len;              // the length of the ref buffer, normally is 2 x chunkbytesize.
    unsigned int ref_buff_data_remain_len;  // the length of the valid data that stored in ref buffer.
    
    short *out_buff;                        // buffer used as internal buffer to store aec produced data for conjunction.                    
    unsigned int out_buff_len;              // the length of the out buffer, normally is 2 x chunkbytesize.
    unsigned int out_buff_data_remain_len;  // the length of the valid data that stored in out buffer.

    // flag used to indicate whether one valid frm has sent to next module or not when aec feature enabled. 
    int aec_first_frm_sent_to_next_module;  
    long long aec_first_frm_pts;            // used to store the first frm pts got from audio_hw.c
    int aec_valid_frm;                      // flag used to indicate one valid output frame is ready or not.
    int ai_sent_to_ao_frms;                 // used to count the number of frame sent to ao at beginning. 

    FILE *tmp_pcm_fp_in;
    int tmp_pcm_in_size;
    FILE *tmp_pcm_fp_ref;
    int tmp_pcm_ref_size;
    FILE *tmp_pcm_fp_out;
    int tmp_pcm_out_size;

    char *pCapBuf;
    char *pRefBuf;

    int first_ref_frm_got_flag;
    int snd_card_id; 

    // for ans common process 

    short *tmpBuf_ans;
    int tmpBufLen_ans; 
    
    short *in_buff_ans;                       // buffer used as internal buffer to store near data for conjunction.
    unsigned int in_buff_len_ans;             // the length of the near buffer, normally is 2 x chunkbytesize.
    unsigned int in_buff_data_remain_len_ans; // the length of the valid data that stored in near buffer.
    
    short *out_buff_ans;                        // buffer used as internal buffer to store aec produced data for conjunction.                    
    unsigned int out_buff_len_ans;              // the length of the out buffer, normally is 2 x chunkbytesize.
    unsigned int out_buff_data_remain_len_ans;  // the length of the valid data that stored in out buffer.

    int ans_valid_frm;                      // flag used to indicate one valid output frame is ready or not.

    // for ans process
    void *ans_int;
    
    int  filter_state1[6];
    int  filter_state12[6];
    int  Synthesis_state1[6];
    int  Synthesis_state12[6];

    // for ans lstm library
    void **ans_int_lstm;
    void **ans_state_lstm;

  	int ai_agc_inited;	// agc initialized or not    
	short *ai_agc_tmp_buff;  // tmp buffer to store data processed by agc 

    BOOL mbSuspendAns;    //when in ans enable state, suspend ans and resume ans. 0:resume, 1:suspend.
    BOOL mbSuspendAec;    //when in aec enable state, suspend aec and resume aec. 0:resume, 1:suspend.
} AudioInputDevice;

typedef struct AudioOutputSubChl_S
{
    AO_STATES_E mState;

    AIO_ATTR_S mAttr;
    PCM_CONFIG_S mCfg;
    AUDIO_TRACK_MODE_E mTrackMode;
    pthread_t mThdId;
    volatile BOOL mThdRunning;
   
	int AoChlId;	// added to indicate id of current ao chl
    int snd_card_id;
} AudioOutputSubChl;
typedef struct AudioOutputDevice
{
    pthread_mutex_t mAOApiCallLock;   // to protect the api call,when used in two thread asynchronously.
    pthread_mutex_t mChnListLock; 
    struct list_head mChnList;
	AudioOutputSubChl PlayChls[MAX_AO_CHLS]; // changed for condition that two ao chl exist
	AO_CHN daudio0_ao_chl;
} AudioOutputDevice;

typedef struct AudioHwDevice
{
    BOOL mEnableFlag;
    AIO_MIXER_S mMixer;
    AudioInputDevice mCap;
    AudioOutputDevice mPlay;
} AudioHwDevice;

static AudioHwDevice gAudioHwDev[AIO_DEV_MAX_NUM];

// 0-cap; 1-play
ERRORTYPE audioHw_Construct(void)
{
    int i;
    int err;
    //memset(&gAudioHwDev, 0, sizeof(AudioHwDevice)*AIO_DEV_MAX_NUM);
    for (i = 0; i < AIO_DEV_MAX_NUM; ++i) {
        AudioHwDevice *pDev = &gAudioHwDev[i];
        if (TRUE == pDev->mEnableFlag) {
            alogw("audio_hw has already been constructed!");
            return SUCCESS;
        }
        memset(pDev, 0, sizeof(AudioHwDevice));
        if(0 == i)
        {
            err = alsaOpenMixer(&pDev->mMixer, SOUND_MIXER_AUDIOCODEC);
            if (err != 0) {
                aloge("AIO device %d open mixer failed, err[%d]!", i, err);
            }
            pDev->mMixer.snd_card_id = 0;

            pDev->mCap.snd_card_id = 0;
            pDev->mCap.mCfg.snd_card_id = 0;

            for(int j=0; j<MAX_AO_CHLS; ++j)
            {
                pDev->mPlay.PlayChls[j].snd_card_id = 0;
                pDev->mPlay.PlayChls[j].mCfg.snd_card_id = 0;
            }
            #if (MPPCFG_AEC == OPTION_AEC_ENABLE)
                alsaMixerSetAudioCodecHubMode(&pDev->mMixer,1);    // to set hub mode for the first snd card
            #endif
        }
        else if(1 == i)
        {
            err = alsaOpenMixer(&pDev->mMixer, SOUND_MIXER_SNDDAUDIO0);
            if (err != 0) {
                aloge("AIO device %d open mixer failed!", i);
            }
            pDev->mMixer.snd_card_id = 1;

            pDev->mCap.snd_card_id = 1;
            pDev->mCap.mCfg.snd_card_id = 1;

            for(int j=0; j<MAX_AO_CHLS; ++j)
            {
                pDev->mPlay.PlayChls[j].snd_card_id = 1;
                pDev->mPlay.PlayChls[j].mCfg.snd_card_id = 1;
            }
            #if (MPPCFG_AEC == OPTION_AEC_ENABLE)
                alsaMixerSetDAudio0HubMode(&pDev->mMixer,1);// to set hub mode for the second snd card
                alsaMixerSetDAudio0LoopBackEn(&pDev->mMixer,1);// to enable loopback for the second snd card
            #endif
        } 
        #if (MPPCFG_AEC == OPTION_AEC_ENABLE)
            alsaMixerSetCapPlaySyncMode(&pDev->mMixer, 1);
        #endif
        pDev->mCap.mState = AI_STATE_INVALID;
        for(int j=0; j<MAX_AO_CHLS; ++j)
        {
            pDev->mPlay.PlayChls[j].mState = AO_STATE_INVALID;
        }

        INIT_LIST_HEAD(&pDev->mCap.mChnList);
        INIT_LIST_HEAD(&pDev->mPlay.mChnList); 
        pthread_mutex_init(&pDev->mCap.mApiCallLock, NULL);
        pthread_mutex_init(&pDev->mPlay.mAOApiCallLock, NULL);
        pthread_mutex_init(&pDev->mPlay.mChnListLock, NULL);
        //INIT_LIST_HEAD(&pPlay->mChnList);
        pDev->mEnableFlag = TRUE;
    }
    return SUCCESS;
}

ERRORTYPE audioHw_Destruct(void)
{
    int i;
    for (i = 0; i < AIO_DEV_MAX_NUM; ++i) {
        AudioHwDevice *pDev = &gAudioHwDev[i];
        if (FALSE == pDev->mEnableFlag) {
            alogw("audio_hw has already been destructed!");
            return SUCCESS;
        }
        if (pDev->mMixer.handle != NULL) 
		{
			for(int j=0; j<MAX_AO_CHLS; ++j)
			{
				if(AO_STATE_STARTED==pDev->mPlay.PlayChls[j].mState)
				{
					aloge("Why AO still running? chl[%d],playState:%d", j,pDev->mPlay.PlayChls[j].mState);
				}
			}

            if (AI_STATE_STARTED==pDev->mCap.mState) 
			{
                aloge("Why AI still running? CapState:%d ", pDev->mCap.mState);
			}
            alsaCloseMixer(&pDev->mMixer);
        }
        
        pthread_mutex_destroy(&pDev->mPlay.mChnListLock);
        pthread_mutex_destroy(&pDev->mCap.mApiCallLock);
        pthread_mutex_destroy(&pDev->mPlay.mAOApiCallLock);

        pDev->mCap.mState = AI_STATE_INVALID;

		for(int j=0; j<MAX_AO_CHLS; ++j)
		{
			pDev->mPlay.PlayChls[j].mState = AO_STATE_INVALID;
		}

        pDev->mEnableFlag = FALSE;
    }
    return SUCCESS;
}


/**************************************AI_DEV*****************************************/
ERRORTYPE audioHw_AI_Dev_lock(AUDIO_DEV AudioDevId)
{
    return pthread_mutex_lock(&gAudioHwDev[AudioDevId].mCap.mChnListLock);
}

ERRORTYPE audioHw_AI_Dev_unlock(AUDIO_DEV AudioDevId)
{
    return pthread_mutex_unlock(&gAudioHwDev[AudioDevId].mCap.mChnListLock);
}

ERRORTYPE audioHw_AI_searchChannel_l(AUDIO_DEV AudioDevId, AI_CHN AiChn, AI_CHANNEL_S** pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    ERRORTYPE ret = FAILURE;
    AI_CHANNEL_S *pEntry;
    list_for_each_entry(pEntry, &pCap->mChnList, mList)
    {
        if(pEntry->mId == AiChn) {
            if(pChn) {
                *pChn = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    return ret;
}

ERRORTYPE audioHw_AI_searchChannel(AUDIO_DEV AudioDevId, AI_CHN AiChn, AI_CHANNEL_S** pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    ERRORTYPE ret = FAILURE;
    AI_CHANNEL_S *pEntry;
    pthread_mutex_lock(&pCap->mChnListLock);
    ret = audioHw_AI_searchChannel_l(AudioDevId, AiChn, pChn);
    pthread_mutex_unlock(&pCap->mChnListLock);
    return ret;
}

ERRORTYPE audioHw_AI_AddChannel_l(AUDIO_DEV AudioDevId, AI_CHANNEL_S* pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    list_add_tail(&pChn->mList, &pCap->mChnList);
    struct list_head* pTmp;
    int cnt = 0;
    list_for_each(pTmp, &pCap->mChnList)
        cnt++;
    updateDebugfsByChnCnt(0, cnt);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_AddChannel(AUDIO_DEV AudioDevId, AI_CHANNEL_S* pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    pthread_mutex_lock(&pCap->mChnListLock);
    ERRORTYPE ret = audioHw_AI_AddChannel_l(AudioDevId, pChn);
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_RemoveChannel(AUDIO_DEV AudioDevId, AI_CHANNEL_S* pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    pthread_mutex_lock(&pCap->mChnListLock);
    list_del(&pChn->mList);
    struct list_head* pTmp;
    int cnt = 0;
    list_for_each(pTmp, &pCap->mChnList)
        cnt++;
    updateDebugfsByChnCnt(0, cnt);
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

MM_COMPONENTTYPE *audioHw_AI_GetChnComp(PARAM_IN MPP_CHN_S *pMppChn)
{
    AI_CHANNEL_S *pChn = NULL;
    if (SUCCESS != audioHw_AI_searchChannel(pMppChn->mDevId, pMppChn->mChnId, &pChn)) {
        return NULL;
    }
    return pChn->mpComp;
}

BOOL audioHw_AI_IsDevStarted(AUDIO_DEV AudioDevId)
{
    return (gAudioHwDev[AudioDevId].mCap.mState == AI_STATE_STARTED);
}

#if (MPPCFG_ANS == OPTION_ANS_ENABLE) 
static ERRORTYPE audioHw_AI_AnsProcess(void *pThreadData, AUDIO_FRAME_S *pFrm)
{
    AudioInputDevice *pCap = (AudioInputDevice*)pThreadData;
    int ret = 0; 

    if(1 != pCap->mCfg.chnCnt)    
    {
        aloge("ans_invalid_chl_number:%d",pCap->mCfg.chnCnt);
        return FAILURE;
    } 

#if (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_WEBRTC)
    if(pCap->mAttr.ai_ans_en && NULL == pCap->ans_int)
    {
        aloge("aec_ans_to_init:%d",pCap->mCfg.sampleRate);
        ret = WebRtcNs_Create(&pCap->ans_int);  // instance
        if(NULL == pCap->ans_int || 0 != ret)
        {
            aloge("aec_ans_instance_create_fail:%d",ret);
            return FAILURE;
        }

        ret = WebRtcNs_Init(pCap->ans_int,pCap->mCfg.sampleRate);
        if(0 != ret)
        {
            aloge("aec_ans_init_failed:%d",ret);
        }

        ret = WebRtcNs_set_policy(pCap->ans_int,pCap->mAttr.ai_ans_mode);
        if(0 != ret)
        {
            aloge("aec_ans_cfg_failed");
        }
        
        memset(pCap->filter_state1,0,sizeof(pCap->filter_state1));
        memset(pCap->filter_state12,0,sizeof(pCap->filter_state12));
        memset(pCap->Synthesis_state1,0,sizeof(pCap->Synthesis_state1));
        memset(pCap->Synthesis_state12,0,sizeof(pCap->Synthesis_state12));
    }
#elif(MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_LSTM)
    if(pCap->mAttr.ai_ans_en && NULL == pCap->ans_int_lstm)
    {
        aloge("aec_ans_lstm_to_init:%d",pCap->mCfg.sampleRate);
        ret = Rnn_Process_Create(&pCap->ans_int_lstm,&pCap->ans_state_lstm);
        if(NULL == pCap->ans_int_lstm || 0 != ret)
        {
            aloge("aec_ans_lstm_instance_create_fail:%d",ret);
            return FAILURE;
        }
        ret = Rnn_Process_init(pCap->ans_int_lstm,pCap->ans_state_lstm,pCap->mCfg.sampleRate);
        if(0 != ret)
        {
            aloge("aec_ans_lstm_init_failed:%d",ret);
        } 
    } 
#endif

    
    memset(pCap->tmpBuf_ans, 0, pCap->tmpBufLen_ans); 

    // move data in near buffer and reference buffer to internal buffer for conjunction with remaining data for last process. 
    if(pCap->in_buff_data_remain_len_ans+pFrm->mLen <= pCap->mCfg.chunkBytes*2)
    {
        memcpy((char*)pCap->in_buff_ans+pCap->in_buff_data_remain_len_ans,(char*)pFrm->mpAddr,pFrm->mLen);
        pCap->in_buff_data_remain_len_ans += pFrm->mLen;
            
    }
    else
    {
        aloge("fatal_err_in_buff_over_flow:%d-%d-%d-%d",pCap->in_buff_data_remain_len_ans,pCap->in_buff_len_ans,
                                                            pFrm->mLen,pCap->mCfg.chunkBytes);
    }

#if (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_WEBRTC)    
    int frm_size = 320;         // 160 samples as one unit processed by aec library, we use two units.
    short tmp_near_buffer[320];
    
    short tmp_ans_shInL[160] = {0};
    short tmp_ans_shInH[160] = {0};
    short tmp_ans_shOutL[160] = {0};
    short tmp_ans_shOutH[160] = {0}; 
#elif (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_LSTM)
    int frm_size = 160;         // 160 samples as one unit processed by aec library     
    short tmp_near_buffer[160];

#endif 
    short *near_frm_ptr = (short *)pCap->in_buff_ans;
    short *processed_frm_ptr = (short *)pCap->tmpBuf_ans; 

    int left = pCap->in_buff_data_remain_len_ans / sizeof(short); 

    // start to process
    while(left >= frm_size)
    {
        if(FALSE == pCap->mbSuspendAns)
        {
            memcpy((char *)tmp_near_buffer,(char *)near_frm_ptr,frm_size*sizeof(short));
          #if (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_WEBRTC)
            aw_WebRtcSpl_AnalysisQMF(tmp_near_buffer,tmp_ans_shInL,tmp_ans_shInH,pCap->filter_state1,pCap->filter_state12);
            if (0 == WebRtcNs_Process(pCap->ans_int ,tmp_ans_shInL,tmp_ans_shInH ,tmp_ans_shOutL , tmp_ans_shOutH))
            {
                aw_WebRtcSpl_SynthesisQMF(tmp_ans_shOutL,tmp_ans_shOutH,processed_frm_ptr,pCap->Synthesis_state1,pCap->Synthesis_state12);
            }
          #elif (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_LSTM)
            Lstm_process_frame(pCap->ans_int_lstm, pCap->ans_state_lstm[0], processed_frm_ptr, tmp_near_buffer);
          #endif 
        }
        else
        {
            memcpy((char*)processed_frm_ptr, (char *)near_frm_ptr, frm_size*sizeof(short));
        }

        near_frm_ptr += frm_size;
        processed_frm_ptr += frm_size;
        left -= frm_size; 

        pCap->in_buff_data_remain_len_ans -= frm_size*sizeof(short);
    } 

    // move remaining data in internal buffer to the beginning of the buffer 
    if(left > 0)
    { 
        unsigned int near_buff_offset = (unsigned int)near_frm_ptr-(unsigned int)pCap->in_buff_ans;
        if( near_buff_offset < pCap->in_buff_data_remain_len_ans)
        {
            aloge("ans_fatal_err_buff_left:%d-%d-%d-%d",near_buff_offset,pCap->in_buff_data_remain_len_ans);
        }

        memcpy((char*)pCap->in_buff_ans,(char*)near_frm_ptr,pCap->in_buff_data_remain_len_ans);
    }

    unsigned int out_offset = (unsigned int)processed_frm_ptr - (unsigned int)pCap->tmpBuf_ans; 

    if(out_offset + pCap->out_buff_data_remain_len_ans > pCap->out_buff_len_ans)
    {
        aloge("ans_fatal_err_out_buff_over_flow:%d-%d-%d",out_offset, pCap->out_buff_data_remain_len_ans, pCap->out_buff_len_ans);
    }
    else
    {
        memcpy((char *)pCap->out_buff_ans+pCap->out_buff_data_remain_len_ans,(char *)pCap->tmpBuf_ans,out_offset); 
        pCap->out_buff_data_remain_len_ans += out_offset;
    } 

    // fetch one valid output frame from output internal buffer, the length of valid frame must equal to chunsize.
    if(pCap->out_buff_data_remain_len_ans >= pCap->mCfg.chunkSize*sizeof(short))
    {
        memcpy((char *)pFrm->mpAddr, (char *)pCap->out_buff_ans, pCap->mCfg.chunkSize*sizeof(short));
        pCap->out_buff_data_remain_len_ans -= pCap->mCfg.chunkSize*sizeof(short);

        if(pFrm->mLen != pCap->mCfg.chunkSize*sizeof(short))
        {
            aloge("ans_fatal_error:%d-%d",pCap->mCfg.chunkSize*sizeof(short),pFrm->mLen);
        }

        
        pCap->ans_valid_frm = 1;
        if(pCap->out_buff_data_remain_len_ans > pCap->mCfg.chunkSize*sizeof(short))
        {
            aloge("ans_fatal_err_out_buff_data_mov:%d-%d",pCap->mCfg.chunkSize*sizeof(short),pCap->out_buff_data_remain_len_ans);
        }
        else
        {
            memcpy((char *)pCap->out_buff_ans,((char *)pCap->out_buff_ans+pCap->mCfg.chunkSize*sizeof(short)), 
                                                                                pCap->out_buff_data_remain_len_ans);
        }
    }
    else
    {
        pCap->ans_valid_frm = 0;
    } 

    return SUCCESS;    
}
#endif 


#if (MPPCFG_AEC == OPTION_AEC_ENABLE)

static ERRORTYPE audioHw_AI_AecProcess(void *pThreadData, AUDIO_FRAME_S *pFrm, AUDIO_FRAME_S *pRefFrm)
{
    AudioInputDevice *pCap = (AudioInputDevice*)pThreadData;
    int ret = 0; 

    if(1 != pCap->mCfg.chnCnt)    
    {
        aloge("aec_invalid_chl_number:%d",pCap->mCfg.chnCnt);
        return FAILURE;
    } 

    if(NULL == pCap->aecmInst)
    { 
        aloge("aec_to_init:%d-%d",pCap->mCfg.sampleRate,pCap->mCfg.aec_delay_ms);
       ret = WebRtcAec_Create(&pCap->aecmInst);
        if(NULL == pCap->aecmInst || 0 != ret)
        {
            aloge("aec_instance_create_fail:%d",ret);
            return FAILURE;
        }
        ret = WebRtcAec_Init(pCap->aecmInst, pCap->mCfg.sampleRate, pCap->mCfg.sampleRate);
        if(0 != ret)
        {
            aloge("aec_init_failed:%d",ret);
        }
        
        AecConfig config;
        
        memset(&config,0,sizeof(AecConfig));
        config.nlpMode = kAecNlpConservative;   
        ret = WebRtcAec_set_config(pCap->aecmInst, config);
        if(0 != ret)
        {
            aloge("aec_cfg_failed:%d",ret);
        }
        
    } 
    
    memset(pCap->tmpBuf, 0, pCap->tmpBufLen); 

    // move data in near buffer and reference buffer to internal buffer for conjunction with remaining data for last process. 
    if(pCap->near_buff_data_remain_len+pFrm->mLen <= pCap->mCfg.chunkBytes*2)
    {
        memcpy((char*)pCap->near_buff+pCap->near_buff_data_remain_len,(char*)pFrm->mpAddr,pFrm->mLen);
        pCap->near_buff_data_remain_len += pFrm->mLen;
            
    }
    else
    {
        aloge("fatal_err_near_buff_over_flow:%d-%d-%d-%d",pCap->near_buff_data_remain_len,pCap->near_buff_len,
                                                            pFrm->mLen,pCap->mCfg.chunkBytes);
    }
    
    if(pCap->ref_buff_data_remain_len+pRefFrm->mLen <= pCap->mCfg.chunkBytes*2)
    {
        memcpy((char*)pCap->ref_buff+pCap->ref_buff_data_remain_len,(char*)pRefFrm->mpAddr, pRefFrm->mLen);
        pCap->ref_buff_data_remain_len += pRefFrm->mLen;
    }
    else
    {
        aloge("fatal_err_ref_buff_over_flow:%d-%d-%d-%d",pCap->ref_buff_data_remain_len,pCap->ref_buff_len,
                                                                pRefFrm->mLen,pCap->mCfg.chunkBytes);
    }

    int frm_size = 160;         // 160 samples as one unit processed by aec library     
    short tmp_near_buffer[160];
    short tmp_far_buffer[160]; 

    short *near_frm_ptr = (short *)pCap->near_buff;
    short *ref_frm_ptr = (short *)pCap->ref_buff;
    short *processed_frm_ptr = (short *)pCap->tmpBuf; 

    int size = (pCap->near_buff_data_remain_len < pCap->ref_buff_data_remain_len)? pCap->near_buff_data_remain_len: pCap->ref_buff_data_remain_len;
    int left = size / sizeof(short); 

    // start to process
    while(left >= frm_size)
    {
        memcpy((char *)tmp_near_buffer,(char *)near_frm_ptr,frm_size*sizeof(short));
        memcpy((char*)tmp_far_buffer,(char*)ref_frm_ptr,frm_size*sizeof(short));

        if (FALSE == pCap->mbSuspendAec)
        {
            ret = WebRtcAec_BufferFarend(pCap->aecmInst, tmp_far_buffer, frm_size);
            if(0 != ret)
            {
                aloge("aec_insert_far_data_failed:%d-%d",ret,((aecpc_t*)pCap->aecmInst)->lastError);
            }

            ret = WebRtcAec_Process(pCap->aecmInst, tmp_near_buffer, NULL, processed_frm_ptr, NULL, frm_size,pCap->mCfg.aec_delay_ms,0);
            if(0 != ret)
            {
                aloge("aec_process_failed:%d-%d",ret,((aecpc_t*)pCap->aecmInst)->lastError);
            }
        }
        else
        {
            memcpy(processed_frm_ptr, near_frm_ptr, frm_size*sizeof(short));
        }

        #ifdef AI_HW_AEC_DEBUG_EN
        if(NULL != pCap->tmp_pcm_fp_in)
        {
            fwrite(tmp_near_buffer, 1, frm_size*sizeof(short), pCap->tmp_pcm_fp_in);
            pCap->tmp_pcm_in_size += frm_size*sizeof(short);
        }
        
        if(NULL != pCap->tmp_pcm_fp_ref)
        {
            fwrite(tmp_far_buffer, 1, frm_size*sizeof(short), pCap->tmp_pcm_fp_ref);
            pCap->tmp_pcm_ref_size += frm_size*sizeof(short);
        }

//        aloge("zjx_tbin:%d-%d",pCap->tmp_pcm_in_size,pCap->tmp_pcm_ref_size); 
        #endif

        near_frm_ptr += frm_size;
        ref_frm_ptr += frm_size;
        processed_frm_ptr += frm_size;
        left -= frm_size; 

        pCap->near_buff_data_remain_len -= frm_size*sizeof(short);
        pCap->ref_buff_data_remain_len -= frm_size*sizeof(short);
    } 

    // move remaining data in internal buffer to the beginning of the buffer 
    if(left > 0)
    { 
        unsigned int near_buff_offset = (unsigned int)near_frm_ptr-(unsigned int)pCap->near_buff;
        unsigned int far_buff_offset = (unsigned int)ref_frm_ptr-(unsigned int)pCap->ref_buff;
        if( near_buff_offset < pCap->near_buff_data_remain_len || 
                       far_buff_offset < pCap->ref_buff_data_remain_len)
        {
            aloge("aec_fatal_err_buff_left:%d-%d-%d-%d",near_buff_offset,pCap->near_buff_data_remain_len,
                                                            far_buff_offset, pCap->ref_buff_data_remain_len);
        }

        memcpy((char*)pCap->near_buff,(char*)near_frm_ptr,pCap->near_buff_data_remain_len);
        memcpy((char*)pCap->ref_buff,(char*)ref_frm_ptr,pCap->ref_buff_data_remain_len); 
    }

    unsigned int out_offset = (unsigned int)processed_frm_ptr - (unsigned int)pCap->tmpBuf; 

    // move the out data produced by aec library to the internal buffer for conjunction with remaining data left for last process
    if(out_offset + pCap->out_buff_data_remain_len > pCap->out_buff_len)
    {
        aloge("aec_fatal_err_out_buff_over_flow:%d-%d-%d",out_offset, pCap->out_buff_data_remain_len, pCap->out_buff_len);
    }
    else
    {
        memcpy((char *)pCap->out_buff+pCap->out_buff_data_remain_len,(char *)pCap->tmpBuf,out_offset); 
        pCap->out_buff_data_remain_len += out_offset;
    } 

    // fetch one valid output frame from output internal buffer, the length of valid frame must equal to chunsize.
    if(pCap->out_buff_data_remain_len >= pCap->mCfg.chunkSize*sizeof(short))
    {
        memcpy((char *)pFrm->mpAddr, (char *)pCap->out_buff, pCap->mCfg.chunkSize*sizeof(short));
        pCap->out_buff_data_remain_len -= pCap->mCfg.chunkSize*sizeof(short);

        if(pFrm->mLen != pCap->mCfg.chunkSize*sizeof(short))
        {
            aloge("aec_fatal_error:%d-%d",pCap->mCfg.chunkSize*sizeof(short),pFrm->mLen);
        }

        #ifdef AI_HW_AEC_DEBUG_EN
        if(NULL != pCap->tmp_pcm_fp_out)
        {
            fwrite(pFrm->mpAddr, 1, pFrm->mLen, pCap->tmp_pcm_fp_out);
            pCap->tmp_pcm_out_size += pFrm->mLen; 
        }
//        aloge("zjx_tbo:%d-%d-%d",pCap->tmp_pcm_out_size,pFrm->mLen,pCap->mCfg.chunkSize*sizeof(short));
        #endif
        
        pCap->aec_valid_frm = 1;
        if(pCap->out_buff_data_remain_len > pCap->mCfg.chunkSize*sizeof(short))
        {
            aloge("aec_fatal_err_out_buff_data_mov:%d-%d",pCap->mCfg.chunkSize*sizeof(short),pCap->out_buff_data_remain_len);
        }
        else
        {
            memcpy((char *)pCap->out_buff,((char *)pCap->out_buff+pCap->mCfg.chunkSize*sizeof(short)), pCap->out_buff_data_remain_len);
        }
    }
    else
    {
        pCap->aec_valid_frm = 0;
    } 

    return SUCCESS;    
}

#endif

static void *audioHw_AI_CapLoopThread(void *pThreadData)
{
    AudioInputDevice *pCap = (AudioInputDevice*)pThreadData; 
    AudioInputDevice *pCap_daudio0 = NULL; 
    
    pCap_daudio0 = &gAudioHwDev[pCap->snd_card_id+1].mCap;

    int nRet = alsaReadPcm(&pCap_daudio0->mCfg, pCap->pRefBuf, pCap_daudio0->mCfg.chunkSize);
    if (nRet != pCap_daudio0->mCfg.chunkSize)
    {
        aloge("fatal error! daudio0 fail to read pcm %d bytes-%d-%d", pCap_daudio0->mCfg.chunkBytes, pCap_daudio0->mState, nRet);
        if(nRet >= 0)
        {
            aloge("fatal error! alsa read pcm[%d] is impossible, check code!", nRet);
        }
    }
    return (void*)nRet;
}

static int OverflowAudioInputDevice(AudioInputDevice *pCap, char *pCapBuf)
{
    // adc driver use default 1024x8 sample, if sampleRate is 16k, cacheTime is 1024/16*8=512ms
    int nUnit = 100;    //unit:ms
    int nInterval;
    int nRet;
    int n;
    for(n=0; n<5; n++)
    {
        nInterval = nUnit*(1<<n);
        usleep(nInterval*1000);
        alogd("snd_card[%d] wait [%d]ms", pCap->snd_card_id, nInterval);
        nRet = alsaReadPcm(&pCap->mCfg, pCapBuf, pCap->mCfg.chunkSize);
        if(nRet < 0)
        {
            if(nRet != -EPIPE)
            {
                aloge("fatal error! why snd_card[%d] read ret[%d] is not -EPIPE?", pCap->snd_card_id, nRet);
            }
            break;
        }
        else
        {
            alogw("Be careful! snd_card[%d] ret[%d] still not -EPIPE.", pCap->snd_card_id, nRet);
        }
    }
    if(n >= 5)
    {
        aloge("fatal error! why snd_card[%d] don't meet -EPIPE?", pCap->snd_card_id);
        return -1;
    }
    else
    {
        return 0;
    }
}
//#define DEBUG_SAVE_DAUDIO0_PCM (1)
static void *audioHw_AI_CapThread(void *pThreadData)
{
    AudioInputDevice *pCap = (AudioInputDevice*)pThreadData; 
    AudioInputDevice *pCap_daudio0 = NULL; 
    
    char *pCapBuf = NULL; 
    char *pCapBufLoopBack = NULL; 
    ssize_t ret;
    pCap->pCapBuf = (char*)malloc(pCap->mCfg.chunkBytes);
    if (NULL == pCap->pCapBuf) {
        aloge("Failed to alloc %d bytes(%s)", pCap->mCfg.chunkBytes, strerror(errno));
    }
    pCapBuf = pCap->pCapBuf; 

    if(pCap->mAttr.ai_aec_en)
    {
        pCap->pRefBuf = (char *)malloc(pCap->mCfg.chunkBytes);
        if(NULL == pCap->pRefBuf)
        {
            aloge("fatal_error_to_malloc_ref_frm_buff:%d",pCap->mCfg.chunkBytes);
        } 
        pCapBufLoopBack = pCap->pRefBuf; 
        pCap_daudio0 = &gAudioHwDev[pCap->snd_card_id+1].mCap; 
    } 

#ifdef DEBUG_SAVE_DAUDIO0_PCM
    FILE *pdaudio0File = fopen("/mnt/extsd/AiDaudio0.pcm", "wb");
#endif
    while (pCap->mThdRunning) 
    {
        //trigger two AudioInputDevice if enable aec.
        if(pCap->mAttr.ai_aec_en && !pCap->first_ref_frm_got_flag)
        {
            alogd("aec to start temporary thread to trigger:[%lld]us", CDX_GetSysTimeUsMonotonic());
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
            pthread_create(&pCap->mThdLoopId, &attr, audioHw_AI_CapLoopThread, pCap);
            pthread_attr_destroy(&attr);

            int nMainChnTriggerResult = alsaReadPcm(&pCap->mCfg, pCapBuf, pCap->mCfg.chunkSize);
            if (nMainChnTriggerResult != pCap->mCfg.chunkSize)
            {
                if(nMainChnTriggerResult >= 0)
                {
                    aloge("fatal error! alsa read pcm impossible[%d], check code!", nMainChnTriggerResult);
                }
                if(-EPIPE == nMainChnTriggerResult)
                {
                    aloge("aec mainChn xrun happened:[%lld]us", CDX_GetSysTimeUsMonotonic());
                }
                aloge("aec fatal error! mainChn fail to read pcm %d bytes-%d-%d", pCap->mCfg.chunkBytes, pCap->mState, nMainChnTriggerResult);
            }

            int nAecChnTriggerResult = 0;
            void *pThdLoopRet = NULL;
            alogd("aec begin to join AecThdLoop[%d].", pCap->mThdLoopId);
            int nErr = pthread_join(pCap->mThdLoopId, (void**)&pThdLoopRet);
            if(nErr != 0)
            {
                aloge("fatal error! AecThdLoop[%d] join fail.", pCap->mThdLoopId);
                nAecChnTriggerResult = -1;
            }
            else
            {
                nAecChnTriggerResult = (int)pThdLoopRet;
                if(-EPIPE == nAecChnTriggerResult)
                {
                    aloge("aec daudio0 xrun happened: [%lld]us!", CDX_GetSysTimeUsMonotonic());
                }
                if(nAecChnTriggerResult < 0)
                {
                    aloge("aec fatal error! daudio0 fail to read pcm %d bytes-%d-%d", pCap_daudio0->mCfg.chunkBytes, pCap_daudio0->mState, nAecChnTriggerResult);
                }
            }

            if(nMainChnTriggerResult < 0 || nAecChnTriggerResult < 0)
            {
                //process exception
                if(nMainChnTriggerResult < 0 && nMainChnTriggerResult != -EPIPE)
                {
                    aloge("Be careful! aec mainChn trigger result[%d] is not -EPIPE. Treated as EPIPE", nMainChnTriggerResult);
                }
                if(nAecChnTriggerResult < 0 && nAecChnTriggerResult != -EPIPE)
                {
                    aloge("Be careful! aec daudio0 trigger result[%d] is not -EPIPE. Treated as EPIPE", nAecChnTriggerResult);
                }

                if(nMainChnTriggerResult >= 0)
                {
                    alogd("Be careful! aec mainChn readPcm[%d], we should make it to xrun(overflow)", nMainChnTriggerResult);
                    OverflowAudioInputDevice(pCap, pCapBuf);
                }
                if(nAecChnTriggerResult >= 0)
                {
                    alogd("Be careful! aec daudio0 readPcm[%d], we should make it to xrun(overflow)", nAecChnTriggerResult);
                    OverflowAudioInputDevice(pCap_daudio0, pCapBufLoopBack);
                }
                alogd("aec trigger fail[%d-%d]:[%lld]us", nMainChnTriggerResult, nAecChnTriggerResult, CDX_GetSysTimeUsMonotonic());
                continue;
            }
            else
            {
                alogd("aec trigger success[%d-%d]:[%lld]us", nMainChnTriggerResult, nAecChnTriggerResult, CDX_GetSysTimeUsMonotonic());
                pCap->first_ref_frm_got_flag = 1;
            }
        }
        else
        {
            ret = alsaReadPcm(&pCap->mCfg, pCapBuf, pCap->mCfg.chunkSize);
            if (ret != pCap->mCfg.chunkSize)
            {
                if(ret >= 0)
                {
                    aloge("fatal error! mainChn read pcm impossible[%d], check code!", ret);
                }
                if(-EPIPE == ret)
                {
                    aloge("aec mainChn xrun happened:[%lld]us", CDX_GetSysTimeUsMonotonic());
                }
                aloge("aec fatal error! mainChn fail to read pcm %d bytes-%d-%d", pCap->mCfg.chunkBytes, pCap->mState, ret);
                usleep(20*1000);    //to wait daudio0 xrun
            }
            int nAecChnRet = 0;
            if(pCap->mAttr.ai_aec_en)
            {
                nAecChnRet = alsaReadPcm(&pCap_daudio0->mCfg, pCapBufLoopBack, pCap_daudio0->mCfg.chunkSize);
                if (nAecChnRet != pCap_daudio0->mCfg.chunkSize)
                {
                    if(nAecChnRet >= 0)
                    {
                        aloge("fatal error! daudio0 read pcm impossible[%d], check code!", nAecChnRet);
                    }
                    if(-EPIPE == nAecChnRet)
                    {
                        aloge("aec daudio0 xrun happened:%lld", CDX_GetSysTimeUsMonotonic());
                    }
                    aloge("aec fatal error! daudio0 fail to read pcm %d bytes-%d-%d", pCap_daudio0->mCfg.chunkBytes, pCap_daudio0->mState, nAecChnRet);
                }
                else
                {
                #ifdef DEBUG_SAVE_DAUDIO0_PCM
                    fwrite(pCapBufLoopBack, 1, pCap_daudio0->mCfg.chunkBytes, pdaudio0File);
                #endif
                }
            }

            if(0 == pCap->mAttr.ai_aec_en)
            {
                if(ret < 0)
                {
                    continue;
                }
            }
            else
            {
                if(ret < 0 || nAecChnRet < 0)
                {
                    //process exception
                    if(ret < 0 && ret != -EPIPE)
                    {
                        aloge("Be careful! aec mainChn read result[%d] is not -EPIPE. Treated as EPIPE", ret);
                    }
                    if(nAecChnRet < 0 && nAecChnRet != -EPIPE)
                    {
                        aloge("Be careful! aec daudio0 read result[%d] is not -EPIPE. Treated as EPIPE", nAecChnRet);
                    }
                    if(ret >= 0)
                    {
                        alogd("Be careful! aec mainChn readPcm[%d], we should make it to xrun(overflow)", ret);
                        OverflowAudioInputDevice(pCap, pCapBuf);
                    }
                    if(nAecChnRet >= 0)
                    {
                        alogd("Be careful! aec daudio0 readPcm[%d], we should make it to xrun(overflow)", nAecChnRet);
                        OverflowAudioInputDevice(pCap_daudio0, pCapBufLoopBack);
                    }
                    alogd("aec read pcm fail[%d-%d], need trigger again!", ret, nAecChnRet);
                    pCap->first_ref_frm_got_flag = 0;
                    continue;
                }
            }
        }

        BOOL new_a_frm = TRUE;
        AI_CHANNEL_S *pEntry = NULL;
        pthread_mutex_lock(&pCap->mChnListLock);
        if (list_empty(&pCap->mChnList))
        {
            pthread_mutex_unlock(&pCap->mChnListLock);
            continue;
        } 

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
        if(pCap->mAttr.ai_aec_en)
        { 
            AUDIO_FRAME_S stFrmRef; 
            AUDIO_FRAME_S stFrmCap; 
            
            memset(&stFrmRef,0,sizeof(AUDIO_FRAME_S));
            memset(&stFrmCap,0,sizeof(AUDIO_FRAME_S)); 
        
            stFrmRef.mLen = pCap->mCfg.chunkBytes;
            stFrmRef.mpAddr = pCap->pRefBuf; 
            
            stFrmCap.mLen = pCap->mCfg.chunkBytes;
            stFrmCap.mpAddr = pCap->pCapBuf; 

            audioHw_AI_AecProcess(pCap,&stFrmCap,&stFrmRef);
            
            if(0 == pCap->aec_valid_frm)
            {
                pthread_mutex_unlock(&pCap->mChnListLock);
                continue;
            }
        }
#endif

#if (MPPCFG_ANS == OPTION_ANS_ENABLE)
        if(pCap->mAttr.ai_ans_en)
        {
            AUDIO_FRAME_S stFrmCap; 
            
            memset(&stFrmCap,0,sizeof(AUDIO_FRAME_S)); 
            stFrmCap.mLen = pCap->mCfg.chunkBytes;
            stFrmCap.mpAddr = pCap->pCapBuf; 
            audioHw_AI_AnsProcess(pCap,&stFrmCap);
            
            if(0 == pCap->ans_valid_frm)
            {
                pthread_mutex_unlock(&pCap->mChnListLock);
                continue;
            }
        }
#endif

#if (MPPCFG_AI_AGC == OPTION_AGC_ENABLE)
		if(pCap->mAttr.ai_agc_en)
		{
			if(!pCap->ai_agc_inited)
			{
				aloge("agc_to_init:%f-%d-%d-%d",pCap->mAttr.ai_agc_cfg.fSample_rate,pCap->mAttr.ai_agc_cfg.iChannel,
						pCap->mAttr.ai_agc_cfg.iSample_len,pCap->mAttr.ai_agc_cfg.iGain_level);
				agc_m_init(&pCap->mAttr.ai_agc_cfg,pCap->mAttr.ai_agc_cfg.fSample_rate,
							pCap->mAttr.ai_agc_cfg.iSample_len,pCap->mAttr.ai_agc_cfg.iBytePerSample,
							pCap->mAttr.ai_agc_cfg.iGain_level, pCap->mAttr.ai_agc_cfg.iChannel);
				pCap->ai_agc_inited = 1;
			}
			agc_m_process(&pCap->mAttr.ai_agc_cfg, (short*)pCap->pCapBuf,pCap->ai_agc_tmp_buff,pCap->mCfg.chunkBytes/2);
			memcpy(pCap->pCapBuf,pCap->ai_agc_tmp_buff,pCap->mCfg.chunkBytes);
		}
#endif		

        list_for_each_entry(pEntry, &pCap->mChnList, mList)
        {
            AUDIO_FRAME_S frame;
            COMP_BUFFERHEADERTYPE bufferHeader;
            bufferHeader.nOutputPortIndex = AI_CHN_PORT_INDEX_CAP_IN;
            bufferHeader.pOutputPortPrivate = &frame;
            frame.mLen = pCap->mCfg.chunkBytes;
            frame.mBitwidth = pCap->mAttr.enBitwidth;
            frame.mSoundmode = pCap->mAttr.enSoundmode;
            frame.mpAddr = pCapBuf;

            if(TRUE == new_a_frm)
            {
                int64_t tm1 = CDX_GetSysTimeUsMonotonic();
                frame.mTimeStamp = tm1 - pCap->mCfg.chunkSize*1000/pCap->mCfg.sampleRate*1000;// Note: the pts timestamp will be used by all ai channels

                new_a_frm = FALSE;
            }
            pEntry->mpComp->EmptyThisBuffer(pEntry->mpComp, &bufferHeader);
        }
        pthread_mutex_unlock(&pCap->mChnListLock);
    }

    alogd("AI_CapThread exit!");
#ifdef DEBUG_SAVE_DAUDIO0_PCM
    fclose(pdaudio0File);
#endif
    if(NULL != pCap->pCapBuf)
    {
        free(pCap->pCapBuf);
        pCap->pCapBuf = NULL;
    }
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    if(pCap->mAttr.ai_aec_en)
    {
        if(NULL != pCap->pRefBuf)
        {
            free(pCap->pRefBuf);
            pCap->pRefBuf = NULL;
        }
    }
#endif
    return NULL;
}

ERRORTYPE audioHw_AI_SetPubAttr(AUDIO_DEV AudioDevId, const AIO_ATTR_S *pstAttr)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    
    pthread_mutex_lock(&pCap->mApiCallLock);
    if (pstAttr == NULL) {
        aloge("pstAttr is NULL!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_ILLEGAL_PARAM;
    }
    if (AI_STATE_INVALID != pCap->mState) {
        alogw("audioHw AI PublicAttr has been set!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return SUCCESS;
    }
    pCap->mAttr = *pstAttr;
    if(pCap->mAttr.ai_aec_en)
    {
      #if (MPPCFG_AEC != OPTION_AEC_ENABLE)
        alogw("Be careful! aec is not config, so force disable.");
        pCap->mAttr.ai_aec_en = 0;
      #endif
    }
    pCap->mState = AI_STATE_CONFIGURED;
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_GetPubAttr(AUDIO_DEV AudioDevId, AIO_ATTR_S *pstAttr)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    
    pthread_mutex_lock(&pCap->mApiCallLock);
    if (pstAttr == NULL) {
        aloge("pstAttr is NULL!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_ILLEGAL_PARAM;
    }

    if (pCap->mState == AI_STATE_INVALID) {
        aloge("get attr when attr is not set!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_NOT_PERM;
    }

    *pstAttr = pCap->mAttr;
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_ClrPubAttr(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    
    pthread_mutex_lock(&pCap->mApiCallLock);
    if (pCap->mState == AI_STATE_STARTED) {
        aloge("please clear attr after AI disable!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_NOT_PERM;
    }
    memset(&pCap->mAttr, 0, sizeof(AIO_ATTR_S));
    pCap->mState = AI_STATE_INVALID;
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_Enable(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer; 
    
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    AudioInputDevice *pCap_daudio0 = &gAudioHwDev[AudioDevId+1].mCap;
    AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
#endif
    int ret;
    
    pthread_mutex_lock(&pCap->mApiCallLock);

    if (pCap->mState == AI_STATE_INVALID) {
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_NOT_CONFIG;
    }
    if (pCap->mState == AI_STATE_STARTED) {
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return SUCCESS;
    }

    if(pCap->mAttr.u32ChnCnt > 2)
    {
        aloge("mic not support chl number more than two:%d,force to mono"); 
        alsaMixerSetMic2Enable(pMixer,0); 
        pCap->mCfg.chnCnt = 1;  
    }
    else if(pCap->mAttr.u32ChnCnt == 2) // note:not support mic2 on v459 platform
    {
        alsaMixerSetMic2Enable(pMixer,1); // enable  mic2
        pCap->mCfg.chnCnt = pCap->mAttr.u32ChnCnt;
    }
    else
    {
        alsaMixerSetMic2Enable(pMixer,0); 
        pCap->mCfg.chnCnt = pCap->mAttr.u32ChnCnt;
    }
    pCap->mCfg.sampleRate = pCap->mAttr.enSamplerate;

    pCap->mCfg.aec_delay_ms = pCap->mAttr.aec_delay_ms;
    
    if (pCap->mAttr.enBitwidth == AUDIO_BIT_WIDTH_32) {
        pCap->mCfg.format = SND_PCM_FORMAT_S32_LE;
    } else if (pCap->mAttr.enBitwidth == AUDIO_BIT_WIDTH_24) {
        pCap->mCfg.format = SND_PCM_FORMAT_S24_LE;
    } else if (pCap->mAttr.enBitwidth == AUDIO_BIT_WIDTH_16) {
        pCap->mCfg.format = SND_PCM_FORMAT_S16_LE;
    } else if (pCap->mAttr.enBitwidth == AUDIO_BIT_WIDTH_8) {
        pCap->mCfg.format = SND_PCM_FORMAT_S8;
    } else {
        pCap->mCfg.format = SND_PCM_FORMAT_S16_LE;
    }
    pCap->mCfg.bitsPerSample = (pCap->mAttr.enBitwidth+1)*8;
    
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    memcpy(&pCap_daudio0->mAttr,&pCap->mAttr,sizeof(AIO_ATTR_S)); 
    int tmpCardId = pCap_daudio0->mCfg.snd_card_id;
    memcpy(&pCap_daudio0->mCfg,&pCap->mCfg,sizeof(PCM_CONFIG_S));
    pCap_daudio0->mCfg.snd_card_id = tmpCardId;
#endif
//    if(pCap->mAttr.ai_aec_en)
//    {
//        alsaMixerSetCapPlaySyncMode(pMixer,1); 
//    }
    ret = alsaOpenPcm(&pCap->mCfg, SOUND_CARD_AUDIOCODEC_CAP, 0);
    if (ret != 0) {
        aloge("fatal error! open_pcm failed");
        alsaMixerSetMic2Enable(pMixer,0); // disable  mic2
        //alsaMixerSetCapPlaySyncMode(pMixer,0); 
        pthread_mutex_unlock(&pCap->mApiCallLock); 
        return FAILURE;
    }
    
    ret = alsaSetPcmParams(&pCap->mCfg);
    if (ret < 0) {
        aloge("fatal error! SetPcmParams failed");
        goto ERR_SET_PCM_PARAM;
    }
    //update mAttr by mCfg
    pCap->mAttr.mPtNumPerFrm = pCap->mCfg.chunkSize;

    pCap->mCfg.read_pcm_aec = 0;

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    if(pCap->mAttr.ai_aec_en)
    {
        //alsaMixerSetCapPlaySyncMode(pMixer_daudio0,1); 

        ret = alsaOpenPcm(&pCap_daudio0->mCfg, SOUND_CARD_SNDDAUDIo0, 0);   // to open snd card daudio0
        if (ret != 0) {
            aloge("fatal error! daudio0 open_pcm failed");
            pthread_mutex_unlock(&pCap->mApiCallLock); 
            return FAILURE;
        }
        pCap->mCfg.read_pcm_aec = 1;
        pCap_daudio0->mCfg.read_pcm_aec = 1;
    } 
#endif 
    

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    if(pCap->mAttr.ai_aec_en)
    {
        ret = alsaSetPcmParams(&pCap_daudio0->mCfg);
        if (ret < 0) {
            aloge("fatal error! SetPcmParams failed");
            goto ERR_SET_PCM_PARAM2;
        }
    }
    if(pCap->mAttr.ai_aec_en)
    {
        pCap->first_ref_frm_got_flag = 0;
        
        pCap->near_buff_len = pCap->mCfg.chunkBytes*2;
        pCap->near_buff = (short *)malloc(pCap->near_buff_len);
        if(NULL == pCap->near_buff)
        {
            aloge("aec_fatal_error_near_buff_malloc_failed:%d",pCap->near_buff_len);
            goto ERR_SET_PCM_PARAM2;
        }
        pCap->near_buff_data_remain_len = 0;

        pCap->ref_buff_len = pCap->mCfg.chunkBytes*2;
        pCap->ref_buff = (short *)malloc(pCap->ref_buff_len);
        if(NULL == pCap->ref_buff)
        {
            aloge("aec_fatal_error_ref_buff_malloc_failed:%d",pCap->ref_buff_len);
            goto ERR_EXIT0;
        }
        pCap->ref_buff_data_remain_len = 0;

        pCap->out_buff_len = pCap->mCfg.chunkBytes*2;
        pCap->out_buff = (short *)malloc(pCap->out_buff_len);
        if(NULL == pCap->out_buff)
        {
            aloge("aec_fatal_error_out_buff_malloc_failed:%d",pCap->out_buff_len);
            goto ERR_EXIT1;
        }
        pCap->out_buff_data_remain_len = 0;

        pCap->aec_first_frm_pts = -1;
        pCap->aec_first_frm_sent_to_next_module = 0;
        pCap->aec_valid_frm = 0;
        pCap->ai_sent_to_ao_frms = 0;

        pCap->tmpBufLen = pCap->mCfg.chunkBytes*2;
        pCap->tmpBuf = (short *)malloc(pCap->tmpBufLen);
        if(pCap->tmpBuf == NULL)
        {
            aloge("aec_fatal_error_out_tmp_buff_malloc_failed:%d",pCap->tmpBufLen);
            goto ERR_EXIT2;
        }

#ifdef AI_HW_AEC_DEBUG_EN 
        pCap->tmp_pcm_fp_in = fopen("/mnt/extsd/tmp_in_ai_pcm", "wb"); 
        pCap->tmp_pcm_fp_ref = fopen("/mnt/extsd/tmp_ref_ai_pcm", "wb");
        pCap->tmp_pcm_fp_out = fopen("/mnt/extsd/tmp_out_ai_pcm", "wb");
        if(NULL==pCap->tmp_pcm_fp_in || NULL==pCap->tmp_pcm_fp_ref || NULL==pCap->tmp_pcm_fp_out)
        {
            aloge("aec_debug_file_create_failed");
        }
#endif
    }
    /* else // Commented for v853
     * {
     *     alsaMixerSetCapPlaySyncMode(pMixer,0);
     * } */
#endif 

#if (MPPCFG_ANS == OPTION_ANS_ENABLE)
    if(pCap->mAttr.ai_ans_en)
    {
        pCap->in_buff_len_ans = pCap->mCfg.chunkBytes*2;
        pCap->in_buff_ans = (short *)malloc(pCap->in_buff_len_ans);
        if(NULL == pCap->in_buff_ans)
        {
            aloge("aec_fatal_error_near_buff_malloc_failed:%d",pCap->in_buff_len_ans);
            goto ERR_EXIT2;
        }
        pCap->in_buff_data_remain_len_ans = 0; 
        
        pCap->out_buff_len_ans = pCap->mCfg.chunkBytes*2;
        pCap->out_buff_ans = (short *)malloc(pCap->out_buff_len_ans);
        if(NULL == pCap->out_buff_ans)
        {
            aloge("ans_fatal_error_out_buff_malloc_failed:%d",pCap->out_buff_len_ans);
            goto ERR_EXIT3;
        }
        pCap->out_buff_data_remain_len_ans = 0;

        
        pCap->tmpBufLen_ans = pCap->mCfg.chunkBytes*2;
        pCap->tmpBuf_ans = (short *)malloc(pCap->tmpBufLen_ans);
        if(pCap->tmpBuf_ans == NULL)
        {
            aloge("ans_fatal_error_out_tmp_buff_malloc_failed:%d",pCap->tmpBufLen_ans);
            goto ERR_EXIT4;
        }
        pCap->ans_valid_frm = 0;
    }
#endif

#if(MPPCFG_AI_AGC == OPTION_AGC_ENABLE)
	if(pCap->mAttr.ai_agc_en)
	{
		pCap->ai_agc_tmp_buff = (short *)malloc(pCap->mCfg.chunkBytes);
		if(NULL == pCap->ai_agc_tmp_buff)
		{
			aloge("agc_in_ai_error_malloc_tmp_buff_failed:%d",pCap->mCfg.chunkBytes);
            goto ERR_EXIT5;
		}
		pCap->ai_agc_inited = 0;
	}
#endif

    pthread_mutex_init(&pCap->mChnListLock, NULL);
    //INIT_LIST_HEAD(&pCap->mChnList);
    pCap->mThdRunning = TRUE;
    pthread_create(&pCap->mThdId, NULL, audioHw_AI_CapThread, pCap);

    pCap->mState = AI_STATE_STARTED;
    
    pthread_mutex_unlock(&pCap->mApiCallLock);

    return SUCCESS;
ERR_EXIT5:
#if (MPPCFG_ANS == OPTION_ANS_ENABLE)
ERR_EXIT4: 
    if(NULL != pCap->out_buff_ans)
    {
        free(pCap->out_buff_ans);
        pCap->out_buff_ans = NULL;
    }

ERR_EXIT3:
    if(NULL != pCap->in_buff_ans)
    {
        free(pCap->in_buff_ans);
        pCap->in_buff_ans = NULL;
    }
#endif
    
ERR_EXIT2: 
#if (MPPCFG_AEC == OPTION_AEC_ENABLE) 
    if(NULL != pCap->out_buff)
    {
        free(pCap->out_buff);
    }
ERR_EXIT1: 
    if(NULL != pCap->ref_buff)
    {
        free(pCap->ref_buff);
    }
ERR_EXIT0:
    if(NULL != pCap->near_buff)
    {
        free(pCap->near_buff);
    } 
ERR_SET_PCM_PARAM2: 
    if(pCap->mAttr.ai_aec_en)
    {
        alsaClosePcm(&pCap_daudio0->mCfg, 0);   // 0: cap
        //alsaMixerSetCapPlaySyncMode(pMixer_daudio0,1); 
    }
#endif
    
ERR_SET_PCM_PARAM:
    alsaMixerSetMic2Enable(pMixer,0); // disable  mic2
    alsaClosePcm(&pCap->mCfg, 0);   // 0: cap
    //alsaMixerSetCapPlaySyncMode(pMixer,0); 
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return FAILURE;
}

ERRORTYPE audioHw_AI_Disable(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer; 
    
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    AudioInputDevice *pCap_daudio0 = &gAudioHwDev[AudioDevId+1].mCap;
    AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
#endif

    int ret;
    
    pthread_mutex_lock(&pCap->mApiCallLock);
    if (pCap->mState == AI_STATE_INVALID) {
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_NOT_CONFIG;
    }
    if (pCap->mState != AI_STATE_STARTED) {
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return SUCCESS;
    }
    pthread_mutex_lock(&pCap->mChnListLock);
    if (!list_empty(&pCap->mChnList)) {
        pthread_mutex_unlock(&pCap->mChnListLock);
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return SUCCESS;
    }
    pthread_mutex_unlock(&pCap->mChnListLock);

    pCap->mThdRunning = FALSE;

    pthread_join(pCap->mThdId, (void*) &ret);

    pthread_mutex_destroy(&pCap->mChnListLock);
    
    alsaClosePcm(&pCap->mCfg, 0);   // 0: cap
    pCap->mCfg.read_pcm_aec = 0;

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    if(pCap->mAttr.ai_aec_en)
    {
        //alsaMixerSetCapPlaySyncMode(pMixer,0);
        alsaClosePcm(&pCap_daudio0->mCfg, 0);   // 0: cap
        pCap_daudio0->mCfg.read_pcm_aec = 0;
    }
    if(pCap->mAttr.ai_aec_en)
    {
        pCap->first_ref_frm_got_flag = 0;
        
        if(NULL != pCap->aecmInst)
        {
            ret = WebRtcAec_Free(pCap->aecmInst);
            if(0 == ret)
            {
                aloge("aec_released");  
                pCap->aecmInst = NULL;
            }
            else
            {
                aloge("aec_free_failed");
            }
        }
           
        if(NULL != pCap->tmpBuf)
        {
            free(pCap->tmpBuf);
            pCap->tmpBuf = NULL;
        }
        if(NULL != pCap->out_buff)
        {
            free(pCap->out_buff);
            pCap->out_buff = NULL;
        }
        if(NULL != pCap->ref_buff)
        {
            free(pCap->ref_buff);
            pCap->ref_buff = NULL;
        }
        if(NULL != pCap->near_buff)
        {
            free(pCap->near_buff);
            pCap->near_buff = NULL;
        } 
        
#ifdef AI_HW_AEC_DEBUG_EN 
        if(NULL != pCap->tmp_pcm_fp_in)
        {
            fclose(pCap->tmp_pcm_fp_in);
            pCap->tmp_pcm_fp_in = NULL;
        }
        if(NULL != pCap->tmp_pcm_fp_ref)
        {
            fclose(pCap->tmp_pcm_fp_ref);
            pCap->tmp_pcm_fp_ref = NULL;
        }
        if(NULL != pCap->tmp_pcm_fp_out)
        {
            fclose(pCap->tmp_pcm_fp_out);
            pCap->tmp_pcm_fp_out = NULL;
        }
#endif
    }
#endif 

#if (MPPCFG_ANS == OPTION_ANS_ENABLE) 
    #if (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_WEBRTC)
            if(pCap->mAttr.ai_ans_en && NULL != pCap->ans_int)
            {
                ret = WebRtcNs_Free(pCap->ans_int);
                if(0 == ret)
                {
                    aloge("aec_ans_released");
                    pCap->ans_int = NULL;
                }
                else
                {
                    aloge("aec_ans_release_failed");
                }
            }
    #elif (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_LSTM)
            if(pCap->mAttr.ai_ans_en && NULL != pCap->ans_int_lstm && NULL != pCap->ans_state_lstm)
            {
                ret = Rnn_Process_free(pCap->ans_state_lstm,pCap->ans_int_lstm);
                if(0 == ret)
                {
                    aloge("aec_ans_lstm_released");
                    pCap->ans_int_lstm = NULL;
                    pCap->ans_state_lstm = NULL;
                }
                else
                {
                    aloge("aec_ans_lstm_release_failed");
                }
            }
    #endif

        if(NULL != pCap->tmpBuf_ans)
        {
            free(pCap->tmpBuf_ans);
            pCap->tmpBuf_ans = NULL;
        }
        if(NULL != pCap->out_buff_ans)
        {
            free(pCap->out_buff_ans);
            pCap->out_buff_ans = NULL;
        }
        if(NULL != pCap->in_buff_ans)
        {
            free(pCap->in_buff_ans);
            pCap->in_buff_ans = NULL;
        } 
#endif 

#if(MPPCFG_AI_AGC == OPTION_AGC_ENABLE)
	if(pCap->mAttr.ai_agc_en)
	{
		if(NULL != pCap->ai_agc_tmp_buff)
		{
			free(pCap->ai_agc_tmp_buff);
			pCap->ai_agc_tmp_buff = NULL;
		}
		pCap->ai_agc_inited = 0;
	}
#endif

    pCap->mState = AI_STATE_CONFIGURED;
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_SetTrackMode(AUDIO_DEV AudioDevId, AUDIO_TRACK_MODE_E enTrackMode)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    pCap->mTrackMode = enTrackMode;

    return SUCCESS;
}

ERRORTYPE audioHw_AI_GetTrackMode(AUDIO_DEV AudioDevId, AUDIO_TRACK_MODE_E *penTrackMode)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    *penTrackMode = pCap->mTrackMode;

    return SUCCESS;
}

ERRORTYPE audioHw_AI_GetPcmConfig(AUDIO_DEV AudioDevId, PCM_CONFIG_S **ppCfg)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }
    *ppCfg = &pCap->mCfg;
    return SUCCESS;
}

ERRORTYPE audioHw_AI_GetAIOAttr(AUDIO_DEV AudioDevId, AIO_ATTR_S **ppAttr)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }
    *ppAttr = &pCap->mAttr;
    return SUCCESS;
}

ERRORTYPE audioHw_AI_SetAdcDrc(AUDIO_DEV AudioDevId, int enable)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerSetAudioCodecAdcDrc(pMixer, enable);
}

ERRORTYPE audioHw_AI_SetAdcHpf(AUDIO_DEV AudioDevId, int enable) 
{ 
	AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap; AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerSetAudioCodecAdcHpf(pMixer, enable);
}

ERRORTYPE audioHw_AI_SetVolume(AUDIO_DEV AudioDevId, int s32VolumeDb)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerSetVolume(pMixer, 0, s32VolumeDb);
}

ERRORTYPE audioHw_AI_GetVolume(AUDIO_DEV AudioDevId, int *ps32VolumeDb)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerGetVolume(pMixer, 0, (long*)ps32VolumeDb);
}

ERRORTYPE audioHw_AI_SetMute(AUDIO_DEV AudioDevId, int bEnable)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerSetMute(pMixer, 0, bEnable);
}

ERRORTYPE audioHw_AI_GetMute(AUDIO_DEV AudioDevId, int *pbEnable)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerGetMute(pMixer, 0, pbEnable);
}


/**************************************AO_DEV*****************************************/
ERRORTYPE audioHw_AO_Dev_lock(AUDIO_DEV AudioDevId)
{
    return pthread_mutex_lock(&gAudioHwDev[AudioDevId].mPlay.mChnListLock);
}

ERRORTYPE audioHw_AO_Dev_unlock(AUDIO_DEV AudioDevId)
{
    return pthread_mutex_unlock(&gAudioHwDev[AudioDevId].mPlay.mChnListLock);
}

ERRORTYPE audioHw_AO_searchChannel_l(AUDIO_DEV AudioDevId, AO_CHN AoChn, AO_CHANNEL_S** pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    ERRORTYPE ret = FAILURE;
    AO_CHANNEL_S *pEntry;
    list_for_each_entry(pEntry, &pPlay->mChnList, mList)
    {
        if(pEntry->mId == AoChn) {
            if(pChn) {
                *pChn = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    return ret;
}

ERRORTYPE audioHw_AO_searchChannel(AUDIO_DEV AudioDevId, AO_CHN AoChn, AO_CHANNEL_S** pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    ERRORTYPE ret = FAILURE;
    AO_CHANNEL_S *pEntry;
    pthread_mutex_lock(&pPlay->mChnListLock);
    ret = audioHw_AO_searchChannel_l(AudioDevId, AoChn, pChn);
    pthread_mutex_unlock(&pPlay->mChnListLock);
    return ret;
}

ERRORTYPE audioHw_AO_AddChannel_l(AUDIO_DEV AudioDevId, AO_CHANNEL_S* pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    list_add_tail(&pChn->mList, &pPlay->mChnList);
    struct list_head* pTmp;
    int cnt = 0;
    list_for_each(pTmp, &pPlay->mChnList)
        cnt++;
    updateDebugfsByChnCnt(1, cnt);
    return SUCCESS;
}

ERRORTYPE audioHw_AO_AddChannel(AUDIO_DEV AudioDevId, AO_CHANNEL_S* pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    pthread_mutex_lock(&pPlay->mChnListLock);
    ERRORTYPE ret = audioHw_AO_AddChannel_l(AudioDevId, pChn);
    pthread_mutex_unlock(&pPlay->mChnListLock);
    return ret;
}

ERRORTYPE audioHw_AO_RemoveChannel(AUDIO_DEV AudioDevId, AO_CHANNEL_S* pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    pthread_mutex_lock(&pPlay->mChnListLock);
    list_del(&pChn->mList);
    struct list_head* pTmp;
    int cnt = 0;
    list_for_each(pTmp, &pPlay->mChnList)
        cnt++;
    updateDebugfsByChnCnt(1, cnt);
    pthread_mutex_unlock(&pPlay->mChnListLock);
    return SUCCESS;
}

MM_COMPONENTTYPE *audioHw_AO_GetChnComp(PARAM_IN MPP_CHN_S *pMppChn)
{
    AO_CHANNEL_S *pChn = NULL;
    if (SUCCESS != audioHw_AO_searchChannel(pMppChn->mDevId, pMppChn->mChnId, &pChn)) {
        return NULL;
    }
    return pChn->mpComp;
}

BOOL audioHw_AO_IsDevConfigured(AUDIO_DEV AudioDevId,AO_CHN AoChn)  // specify SndCard and ao chn id
{
    return (gAudioHwDev[AudioDevId].mPlay.PlayChls[AoChn].mState == AO_STATE_CONFIGURED);
}

BOOL audioHw_AO_IsDevStarted(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    return (gAudioHwDev[AudioDevId].mPlay.PlayChls[AoChn].mState == AO_STATE_STARTED);
}
/* set attribute of pcm data from given ao chn */
ERRORTYPE AudioHw_AO_SetPubAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn, const AIO_ATTR_S *pstAttr)
{
    if (pstAttr == NULL) {
        aloge("pstAttr is NULL!");
        return ERR_AO_ILLEGAL_PARAM;
    }
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

	aloge("ao_set_attr_chl:%d,stat:%d",AoChn,pPlay->PlayChls[AoChn].mState);

    if (pPlay->PlayChls[AoChn].mState == AO_STATE_CONFIGURED) {
        alogw("Update AoAttr? cur_card:%d -> wanted_card:%d-%d", 
				pPlay->PlayChls[AoChn].mAttr.mPcmCardId, pstAttr->mPcmCardId,AoChn);
    } else if (pPlay->PlayChls[AoChn].mState == AO_STATE_STARTED) {
        alogw("Careful for 2 AoChns at the same time! They must have the same param:%d!",AoChn);
        return SUCCESS;
    } 
    pPlay->PlayChls[AoChn].mAttr = *pstAttr;
    pPlay->PlayChls[AoChn].mState = AO_STATE_CONFIGURED;
    return SUCCESS;
}
/* get attribute of pcm that come from given ao chn */
ERRORTYPE AudioHw_AO_GetPubAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn, AIO_ATTR_S *pstAttr)
{
    if (pstAttr == NULL) {
        aloge("pstAttr is NULL!");
        return ERR_AO_ILLEGAL_PARAM;
    }

    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    if (pPlay->PlayChls[AoChn].mState == AO_STATE_INVALID) {
        aloge("get attr when attr is not set!");
        return ERR_AO_NOT_PERM;
    }

    *pstAttr = pPlay->PlayChls[AoChn].mAttr;
    return SUCCESS;
}
/* clear the attribute for specific ao chn */
ERRORTYPE audioHw_AO_ClrPubAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    if (pPlay->PlayChls[AoChn].mState == AO_STATE_STARTED) {
        aloge("please clear attr after AI disable!");
        return ERR_AO_NOT_PERM;
    }
    memset(&pPlay->PlayChls[AoChn].mAttr, 0, sizeof(AIO_ATTR_S));
    pPlay->PlayChls[AoChn].mState = AO_STATE_INVALID;
    return SUCCESS;
}

ERRORTYPE audioHw_AO_SetTrackMode(AUDIO_DEV AudioDevId, AO_CHN AoChn,AUDIO_TRACK_MODE_E enTrackMode)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }

    pPlay->PlayChls[AoChn].mTrackMode = enTrackMode;

    return SUCCESS;
}

ERRORTYPE audioHw_AO_GetTrackMode(AUDIO_DEV AudioDevId, AO_CHN AoChn,AUDIO_TRACK_MODE_E *penTrackMode)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }

    *penTrackMode = pPlay->PlayChls[AoChn].mTrackMode;

    return SUCCESS;
}

ERRORTYPE audioHw_AO_Enable(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay; 
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer; 
	int j = 0; 

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    AudioOutputDevice *pPlay_daudio0 = &gAudioHwDev[AudioDevId+1].mPlay; // related with second snd card
    AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
#endif

    int ret;

	aloge("ao_enable_chl:%d",AoChn);

    pthread_mutex_lock(&pPlay->mAOApiCallLock);
	if (pPlay->PlayChls[AoChn].mState == AO_STATE_INVALID) {
		aloge("fatal error! error_state:%d", pPlay->PlayChls[AoChn].mState);
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return ERR_AO_NOT_CONFIG;
    }
	if (pPlay->PlayChls[AoChn].mState == AO_STATE_STARTED) {
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return SUCCESS;
    }
	PCM_CONFIG_S *p_cfg = &pPlay->PlayChls[AoChn].mCfg;
	AIO_ATTR_S *p_attr = &pPlay->PlayChls[AoChn].mAttr;

	p_cfg->chnCnt = p_attr->u32ChnCnt;
	p_cfg->sampleRate = p_attr->enSamplerate;
	switch (p_attr->enBitwidth)
	{
		case AUDIO_BIT_WIDTH_32:
			p_cfg->format = SND_PCM_FORMAT_S32_LE;
			break;
		case AUDIO_BIT_WIDTH_24:
			p_cfg->format = SND_PCM_FORMAT_S24_LE;
			break;
		case AUDIO_BIT_WIDTH_16:
			p_cfg->format = SND_PCM_FORMAT_S16_LE;
			break;
		case AUDIO_BIT_WIDTH_8:
			p_cfg->format = SND_PCM_FORMAT_S8;
			break;
		default :
			p_cfg->format = SND_PCM_FORMAT_S16_LE;
			break;
	}

    p_cfg->bitsPerSample = (p_attr->enBitwidth+1)*8;

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    memcpy(&pPlay_daudio0->PlayChls[AoChn].mAttr,&pPlay->PlayChls[AoChn].mAttr,sizeof(AIO_ATTR_S)); 
    memcpy(&pPlay_daudio0->PlayChls[AoChn].mCfg,&pPlay->PlayChls[AoChn].mCfg,sizeof(PCM_CONFIG_S)); 
    alogw("params of play_daudio0 must follow PlaybackRateDmix in asound.conf!");
    pPlay_daudio0->PlayChls[AoChn].mAttr.enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(PlaybackRateDmix_SampleRate);
    pPlay_daudio0->PlayChls[AoChn].mAttr.u32ChnCnt = PlaybackRateDmix_ChnCnt;
    pPlay_daudio0->PlayChls[AoChn].mCfg.sampleRate = PlaybackRateDmix_SampleRate;
    pPlay_daudio0->PlayChls[AoChn].mCfg.chnCnt = PlaybackRateDmix_ChnCnt;
    for(j=0; j<MAX_AO_CHLS; ++j)
    {
        if(pPlay->PlayChls[j].mState == AO_STATE_STARTED)
        {
            break;
        }
    }
//    if(j == MAX_AO_CHLS)    // only need to set AudioCodecHubMode when the AoChn is the first one
//    {
//        alsaMixerSetAudioCodecHubMode(pMixer,1);    // to set hub mode for the first snd card
//    }
//    else
//    {
//        alogd("Be careful! aoChn[%d] is started, not set AudioCodecHubMode to 1", j);
//    }
#endif
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    if(j == MAX_AO_CHLS)    // only need to set daudio0 when the AoChn is the first one
    {
        //alsaMixerSetDAudio0HubMode(pMixer_daudio0,1);// to set hub mode for the second snd card
        //alsaMixerSetDAudio0LoopBackEn(pMixer_daudio0,1);// to enable loopback for the second snd card
        ret = alsaOpenPcm(&pPlay_daudio0->PlayChls[AoChn].mCfg, SOUND_CARD_SNDDAUDIo0, 1);  // to open the second snd card
        if (ret != 0)
        {
            aloge("fatal error! daudio0 open_pcm failed");
            pthread_mutex_unlock(&pPlay->mAOApiCallLock);
            return FAILURE;
        }
        pPlay_daudio0->daudio0_ao_chl = AoChn;      // record the ao chn index to set daudio0
        ret = alsaSetPcmParams(&pPlay_daudio0->PlayChls[AoChn].mCfg);   // to set params for the second snd card
        if (ret < 0)
        {
            aloge("fatal error! SetPcmParams failed");
            goto ERR_SET_PCM_PARAM2;
        }
        alsaPreparePcm(&pPlay_daudio0->PlayChls[AoChn].mCfg);           // and call the prepare api for the second snd card
    }
#endif
    const char *pCardType = (pPlay->PlayChls[AoChn].mAttr.mPcmCardId==PCM_CARD_TYPE_AUDIOCODEC) ? SOUND_CARD_AUDIOCODEC_PLAY:SOUND_CARD_SNDHDMI;
    ret = alsaOpenPcm(&pPlay->PlayChls[AoChn].mCfg,pCardType, 1);
    if (ret != 0)
    {
        aloge("fatal error! open_pcm failed");
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return FAILURE;
    }
    ret = alsaSetPcmParams(&pPlay->PlayChls[AoChn].mCfg);
    if (ret < 0)
    {
        aloge("fatal error! SetPcmParams failed");
        goto ERR_SET_PCM_PARAM;
    }
    alsaPreparePcm(&pPlay->PlayChls[AoChn].mCfg);           // and call the prepare api for the first snd card

    //pPlay->mThdRunning = TRUE;
    //pthread_create(&pPlay->mThdId, NULL, audioHw_AO_PlayThread, &pPlay);

    pPlay->PlayChls[AoChn].mState = AO_STATE_STARTED;
    pthread_mutex_unlock(&pPlay->mAOApiCallLock);
    return SUCCESS;
    
ERR_SET_PCM_PARAM:
    alsaClosePcm(&pPlay->PlayChls[AoChn].mCfg, 1);  // 1: playback
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
ERR_SET_PCM_PARAM2:
    alsaClosePcm(&pPlay_daudio0->PlayChls[AoChn].mCfg, 1);  // close the second snd card
#endif
    pthread_mutex_unlock(&pPlay->mAOApiCallLock);
    return FAILURE;
}

ERRORTYPE audioHw_AO_Disable(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer; 
	int tmp_cnt = 0;

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    AudioOutputDevice *pPlay_daudio0 = &gAudioHwDev[AudioDevId+1].mPlay; // related with second snd card
    AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;

#endif
    pthread_mutex_lock(&pPlay->mAOApiCallLock);
    if (pPlay->PlayChls[AoChn].mState == AO_STATE_INVALID) {
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return ERR_AO_NOT_CONFIG;
    }
    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return SUCCESS;
    }

    //pPlay->mThdRunning = FALSE;
    //pthread_join(pPlay->mThdId, (void*) &ret);

    /* if (!list_empty(&pPlay->mChnList)) {
        alogw("When ao_disable, still exist channle in PlayChnList?! list them below:");
        AO_CHANNEL_S *pEntry;
        list_for_each_entry(pEntry, &pPlay->mChnList, mList)
        {
            alogw("AoCardType[%d] AoChn[%d] still run!", pPlay->PlayChls[AoChn].mAttr.mPcmCardId, pEntry->mId);
        }
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return SUCCESS;
    } */

    alogd("close pcm! current AoCardType:[%d-%d]",AoChn, pPlay->PlayChls[AoChn].mAttr.mPcmCardId);
    alsaClosePcm(&pPlay->PlayChls[AoChn].mCfg, 1);  // 1: playback
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    for(int j=0; j<MAX_AO_CHLS; ++j)
    {
        if(pPlay->PlayChls[j].mState == AO_STATE_STARTED)
        {
            tmp_cnt++;
        }
    }
    if(1 == tmp_cnt)
    {
        //alsaMixerSetAudioCodecHubMode(pMixer,0);    // to clean hub mode for the first snd card
        //alsaMixerSetDAudio0HubMode(pMixer_daudio0,0);// to clean hub mode for the second snd card
        //alsaMixerSetDAudio0LoopBackEn(pMixer_daudio0,0);// to disable loopback for the second snd card
        alsaClosePcm(&pPlay_daudio0->PlayChls[pPlay_daudio0->daudio0_ao_chl].mCfg, 1);  // to close the the second snd card
    }
#endif
    pPlay->PlayChls[AoChn].mState = AO_STATE_CONFIGURED;
    pthread_mutex_unlock(&pPlay->mAOApiCallLock);

    return SUCCESS;
}

ERRORTYPE audioHw_AO_SetDacDrc(AUDIO_DEV AudioDevId, int enable)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerSetAudioCodecDacDrc(pMixer, enable);
}

ERRORTYPE audioHw_AO_SetDacHpf(AUDIO_DEV AudioDevId, int enable)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;
	
    /* if (pPlay->mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerSetAudioCodecDacHpf(pMixer, enable);
}


ERRORTYPE audioHw_AO_SetVolume(AUDIO_DEV AudioDevId, int s32VolumeDb)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerSetVolume(pMixer, 1, s32VolumeDb);
}

ERRORTYPE audioHw_AO_GetVolume(AUDIO_DEV AudioDevId, int *ps32VolumeDb)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerGetVolume(pMixer, 1, (long*)ps32VolumeDb);
}

ERRORTYPE audioHw_AO_SetMute(AUDIO_DEV AudioDevId, BOOL bEnable, AUDIO_FADE_S *pstFade)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerSetMute(pMixer, 1, (int)bEnable);
}

ERRORTYPE audioHw_AO_GetMute(AUDIO_DEV AudioDevId, BOOL *pbEnable, AUDIO_FADE_S *pstFade)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    int MainVolVal;
    alsaMixerGetMute(pMixer, 1, &MainVolVal);
    if (MainVolVal > 0)
        *pbEnable = FALSE;
    else
        *pbEnable = TRUE;

    return SUCCESS;
}

ERRORTYPE audioHw_AO_SetPA(AUDIO_DEV AudioDevId, BOOL bHighLevel)
{
    ERRORTYPE ret;
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    ret = alsaMixerSetPlayBackPA(pMixer, (int)bHighLevel);
    if(0 != ret)
    {
        aloge("fatal error! alsaMixer SetPlayBackPA fail[0x%x]!", ret);
    }
    return ret;
}

ERRORTYPE audioHw_AO_GetPA(AUDIO_DEV AudioDevId, BOOL *pbHighLevel)
{
    ERRORTYPE ret;
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    int bHighLevel = 0;
    ret = alsaMixerGetPlayBackPA(pMixer, &bHighLevel);
    if(0 == ret)
    {
        *pbHighLevel = bHighLevel?TRUE:FALSE;
    }
    else
    {
        aloge("fatal error! alsaMixer GetPlayBackPA fail[0x%x]!", ret);
    }

    return ret;
}

ERRORTYPE audioHw_AO_FillPcmRingBuf(AUDIO_DEV AudioDevId,AO_CHN AoChn, void* pData, int Len)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    size_t frame_cnt = Len / (pPlay->PlayChls[AoChn].mCfg.bitsPerFrame >> 3);
    ssize_t ret;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }
    ret = alsaWritePcm(&pPlay->PlayChls[AoChn].mCfg, pData, frame_cnt);
    if (ret != frame_cnt) {
        aloge("alsaWritePcm error!");
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE audioHw_AO_DrainPcmRingBuf(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    alsaDrainPcm(&pPlay->PlayChls[AoChn].mCfg);

    return SUCCESS;
}

ERRORTYPE audioHw_AO_FeedPcmData(AUDIO_DEV AudioDevId,AO_CHN AoChn, AUDIO_FRAME_S *pFrm)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    size_t frame_cnt = pFrm->mLen / (pPlay->PlayChls[AoChn].mCfg.bitsPerFrame >> 3);
    ssize_t ret;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }

    ret = alsaWritePcm(&pPlay->PlayChls[AoChn].mCfg, pFrm->mpAddr, frame_cnt);
    if (ret != frame_cnt) {
        aloge("alsaWritePcm error!");
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE audioHw_AO_GetPcmConfig(AUDIO_DEV AudioDevId,AO_CHN AoChn, PCM_CONFIG_S **ppCfg)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }
    *ppCfg = &pPlay->PlayChls[AoChn].mCfg;
    return SUCCESS;
}

ERRORTYPE audioHw_AO_GetAIOAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn, AIO_ATTR_S **ppAttr)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }
    *ppAttr = &pPlay->PlayChls[AoChn].mAttr;
    return SUCCESS;
}

ERRORTYPE audioHw_AI_SuspendAns(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    pthread_mutex_lock(&pCap->mChnListLock);
    if (pCap->mState != AI_STATE_STARTED)
    {
        aloge("fatal error! audioDev[%d] is not started!", AudioDevId);
        pthread_mutex_unlock(&pCap->mChnListLock);
        return ERR_AI_NOT_ENABLED;
    }
    pCap->mbSuspendAns = TRUE;
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}
ERRORTYPE audioHw_AI_ResumeAns(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    pthread_mutex_lock(&pCap->mChnListLock);
    if (pCap->mState != AI_STATE_STARTED)
    {
        aloge("fatal error! audioDev[%d] is not started!", AudioDevId);
        pthread_mutex_unlock(&pCap->mChnListLock);
        return ERR_AI_NOT_ENABLED;
    }
    pCap->mbSuspendAns = FALSE;
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_SuspendAec(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    pthread_mutex_lock(&pCap->mChnListLock);
    if (pCap->mState != AI_STATE_STARTED)
    {
        aloge("fatal error! audioDev[%d] is not started!", AudioDevId);
        pthread_mutex_unlock(&pCap->mChnListLock);
        return ERR_AI_NOT_ENABLED;
    }
    pCap->mbSuspendAec = TRUE;
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_ResumeAec(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    pthread_mutex_lock(&pCap->mChnListLock);
    if (pCap->mState != AI_STATE_STARTED)
    {
        aloge("fatal error! audioDev[%d] is not started!", AudioDevId);
        pthread_mutex_unlock(&pCap->mChnListLock);
        return ERR_AI_NOT_ENABLED;
    }
    pCap->mbSuspendAec = FALSE;
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

