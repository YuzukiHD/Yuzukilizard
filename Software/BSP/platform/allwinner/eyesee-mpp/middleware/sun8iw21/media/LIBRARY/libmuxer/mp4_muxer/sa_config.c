
#include <string.h>
#include "sa_config.h"
//#include <type_camera.h>
#include <aencoder.h>

//1.aac
const UINT32 aSampleRate[13] =
{
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000,
    11025, 8000, 7350
};
/*******************************************************************************
Function name: sa_config_get_data_aac
Description: 
    1.���aac��frame head.
    2.����ֵ��ʹ�õ��ֽ���
Parameters: 
    
Return: 
    
Time: 2010/8/2
*******************************************************************************/
int  sa_config_get_data_aac(unsigned char *bs, int length,ga_config_data *data)
{
    int i;
    int ret = -1;
    if((length < 5 )||(data->numChannels >= 8))
        return ret;
    memset(bs,0,5);
    *bs = (data->audioObjectType)<<3;
    for(i=0;i<13;i++)
    {
       if(data->samplingFrequency == aSampleRate[i])
       break;
    }
    if(i==13)//no samplerate in table
    {
      *bs |=0x7;//set 4bits 0xf;
      *(bs+1) =0x80;
      *(bs+1) |= (data->samplingFrequency>>(24-7))&0x7f;
      *(bs+2) = (data->samplingFrequency>>(24-15))&0xff;
      *(bs+3) = (data->samplingFrequency>>(24-23))&0xff;
      *(bs+4) = (data->samplingFrequency&0x01)<<7;
      *(bs+4) |= (data->numChannels&0xf)<<3;
      ret =5;
       
    }
    else
    {
      *bs |=i>>1;
      *(bs+1) |=i<<7;
      *(bs+1) |= (data->numChannels&0xf)<<3;
      ret =2;
      
    }
    return ret;
}

//2.adpcm

int MuxerGenerateAudioExtraData(__extra_data_t *pExtra, _media_file_inf_t *pAudioInf)
{    
	//LOGD("Mp4MuxerGenerateAudioExtraData ++");
	if(AUDIO_ENCODER_AAC_TYPE == pAudioInf->audio_encode_type)
	{
        ga_config_data  cfg_data;
        cfg_data.audioObjectType = AACLC;
        cfg_data.samplingFrequency = pAudioInf->sample_rate;
        cfg_data.numChannels = pAudioInf->channels;
        pExtra->extra_data_len = sa_config_get_data_aac(pExtra->extra_data, 32, &cfg_data);
	}
    else
    {
        pExtra->extra_data_len = 0;
    }

	//LOGD("Mp4MuxerGenerateAudioExtraData --");
    return 0;
}


